/****************************************************************************
 * apps/desktop_pet/servo_drv.c
 *
 * 270° 舵机控制 - ESP32-S3 LEDC PWM（T-Display-S3 版）
 *
 * 硬件：
 *   - 舵机信号线当前接 GPIO42（用户指定"42 端口"）。GPIO42 = LCD 数据线 D3（PCB 硬布线），
 *     故舵机占 42 时必须关屏（CONFIG_LCD=n，face 模块随之关闭），否则 PWM 灌进屏幕→花屏。
 *     GPIO24 模组未引出，不可用。引脚真正绑定由 board.c + CONFIG_ESP32S3_LEDC_CHANNEL0_PIN 决定。
 *   - 5V 独立供电（板子只供信号，不能从 3.3V 取，会烧 ESP32）
 *   - 频率：50Hz (周期 20ms)
 *   - 脉宽：500us ~ 2500us → 0° ~ 270°
 *
 * 驱动方式（openvela / NuttX 标准做法）：
 *   board 侧已把 LEDC 通道 0 注册成 PWM 字符设备 /dev/pwm0
 *   （见 board/t-display-s3/board.c 的 t_display_s3_servo_init()）。
 *   本文件 open("/dev/pwm0") 后用
 *     ioctl(PWMIOC_SETCHARACTERISTICS) + PWMIOC_START
 *   输出 PWM。**不要**去写 /sys/class/ledc/...（openvela 没有该 sysfs 节点）。
 *
 * 两个易错点（从 aivox3 工程 QA 审查确认，务必别改回去）：
 *   1) pwm_info_s.channels[] **只在 CONFIG_PWM_MULTICHAN 下存在**；
 *      否则结构体里是裸字段 `ub16_t duty`。两种形态用 #ifdef 分开处理。
 *   2) channels[i].channel 的取值是 **1..N**，填 0 表示"该通道跳过/不使用"，
 *      驱动会直接忽略。所以配置里的 0-based 通道索引必须 **+1** 再填。
 *
 * 所有引脚/设备路径/脉宽范围都走 Kconfig（见 apps/desktop_pet/Kconfig），
 * 改引脚只改 menuconfig，不用动本文件。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <syslog.h>

#include <nuttx/timers/pwm.h>

#include "desktop_pet.h"

/****************************************************************************
 * 常量（全部来自 Kconfig，便于 menuconfig 调整）
 ****************************************************************************/

#define SERVO_PWM_DEV       CONFIG_APP_DESKTOP_PET_SERVO_PWM_DEV
#define SERVO_PWM_CH        CONFIG_APP_DESKTOP_PET_SERVO_PWM_CH
#define SERVO_GPIO          CONFIG_APP_DESKTOP_PET_SERVO_GPIO

#define SERVO_FREQ_HZ       50
#define SERVO_PERIOD_US     20000u          /* 1/50Hz = 20ms */
#define SERVO_MIN_US        CONFIG_APP_DESKTOP_PET_SERVO_MIN_US
#define SERVO_MAX_US        CONFIG_APP_DESKTOP_PET_SERVO_MAX_US
#define SERVO_ANGLE_RANGE   CONFIG_APP_DESKTOP_PET_SERVO_RANGE_DEG
#define SERVO_DUTY_MAX      65536ul

/****************************************************************************
 * 私有状态
 ****************************************************************************/

static int              g_fd_pwm = -1;
static int              g_cur_angle = CONFIG_APP_DESKTOP_PET_SERVO_DEFAULT_ANGLE;
static volatile bool    g_sweeping = false;
static volatile bool    g_abort = false;
static pthread_t        g_sweep_tid;
static bool             g_sweep_running = false;
static pthread_mutex_t  g_lock = PTHREAD_MUTEX_INITIALIZER;
/* 命令串行锁：语音每次唤醒都起一个独立 `servo` 进程，若不串行化，多个
 * servo_cmd 会并发写 g_cur_angle 与 /dev/pwm0，互相打架 → "有时不转/卡顿"。
 * 持锁覆盖整条 servo_cmd（含 smooth_move），保证同一时刻只有一条命令在动。*/
static pthread_mutex_t  g_cmd_lock = PTHREAD_MUTEX_INITIALIZER;
/* PWM 是否已 START：LEDC 定时器只需启动一次，后续只更新占空比(SETCHARACTERISTICS)
 * 即可平滑变脉宽。每度都 START 会把定时器停启一次 → 肉眼可见的"一卡一卡"。*/
static bool             g_started = false;

/****************************************************************************
 * 工具函数
 ****************************************************************************/

/* 角度 -> 脉宽(us)，自动夹紧到 [0, RANGE] */
static int angle_to_us(int angle)
{
  if (angle < 0)
    {
      angle = 0;
    }

  if (angle > SERVO_ANGLE_RANGE)
    {
      angle = SERVO_ANGLE_RANGE;
    }

  return SERVO_MIN_US +
         (angle * (SERVO_MAX_US - SERVO_MIN_US)) / SERVO_ANGLE_RANGE;
}

/* 脉宽(us) -> duty（16 位归一） */
static uint32_t us_to_duty(int us)
{
  return (uint32_t)((uint32_t)us * SERVO_DUTY_MAX / SERVO_PERIOD_US);
}

/****************************************************************************
 * Name: pwm_apply_us
 *
 * Description:
 *   把脉宽写到 PWM 字符设备 /dev/pwm0。
 ****************************************************************************/

static int pwm_apply_us(int us)
{
  struct pwm_info_s info;
  int ret;

  if (g_fd_pwm < 0)
    {
      return -ENODEV;
    }

  memset(&info, 0, sizeof(info));
  info.frequency = SERVO_FREQ_HZ;

#ifdef CONFIG_PWM_MULTICHAN
  {
    int i;
    int ch = SERVO_PWM_CH;

    if (ch >= CONFIG_PWM_NCHANNELS)
      {
        syslog(LOG_ERR, "servo: PWM channel %d >= CONFIG_PWM_NCHANNELS(%d)\n",
               ch, CONFIG_PWM_NCHANNELS);
        return -EINVAL;
      }

    /* 先把所有通道标记为"跳过"（channel=0），再单独设置舵机通道。
     * ⚠️ 修复：pwm_info_s 的多通道数组字段是 `channel[]`（单数），
     *    不是 `channels[]`（复数；后者是通道计数 ub8_t）。
     *    且必须显式设置 info.channels = 启用的通道数，否则驱动循环 0 次
     *    → 任何角度都写不进去，舵机永远不动。*/
    for (i = 0; i < CONFIG_PWM_NCHANNELS; i++)
      {
        info.channel[i].channel = 0;
        info.channel[i].duty    = 0;
      }

    info.channel[ch].channel = (ub16_t)(ch + 1);  /* 1-based，必须 +1 */
    info.channel[ch].duty    = (ub16_t)us_to_duty(us);
    info.channels = (ub8_t)(ch + 1);  /* 驱动处理到 ch 为止的条目数 */
  }
#else
  /* 非多通道形态：结构体里只有一个裸 duty */
  info.duty = (ub16_t)us_to_duty(us);
#endif

  ret = ioctl(g_fd_pwm, PWMIOC_SETCHARACTERISTICS,
              (unsigned long)((uintptr_t)&info));
  if (ret < 0)
    {
      syslog(LOG_ERR, "servo: PWMIOC_SETCHARACTERISTICS failed: %d\n", errno);
      return -errno;
    }

  /* ⚠️ 关键修复（"一卡一卡"）：LEDC 定时器只 START 一次，之后每个角度只更新占空比。
   * 若每度都 START，会把 PWM 定时器停/启一次，舵机输出出现肉眼可见的顿挫。*/
  if (!g_started)
    {
      ret = ioctl(g_fd_pwm, PWMIOC_START,
                  (unsigned long)((uintptr_t)&info));
      if (ret < 0)
        {
          syslog(LOG_ERR, "servo: PWMIOC_START failed: %d\n", errno);
          return -errno;
        }

      g_started = true;
    }

  return OK;
}

/* 按速度档位返回每度延时(us) */
static int speed_step_us(int speed)
{
  switch (speed)
    {
      case SERVO_SPEED_FAST:   return 830;   /* ~50ms / 60°  */
      case SERVO_SPEED_MEDIUM: return 1330;  /* ~80ms / 60°  */
      case SERVO_SPEED_SLOW:
      default:                 return 2500;  /* ~150ms / 60° */
    }
}

/* 平滑移动：逐步逼近目标角度，期间可被新命令打断。
 *
 * 并发约定：**调用方不能持有 g_lock**。
 * 本函数只在更新共享状态 g_cur_angle 的瞬间持锁，usleep 期间放锁，
 * 这样 servo_cmd / servo_stop 不会被一次几秒的转动阻塞。 */
static int smooth_move(int from, int to, int speed)
{
  int step = (to > from) ? 1 : -1;
  int cur  = from;

  while (cur != to)
    {
      cur += step;

      if ((step > 0 && cur > to) || (step < 0 && cur < to))
        {
          cur = to;
        }

      pwm_apply_us(angle_to_us(cur));

      pthread_mutex_lock(&g_lock);
      g_cur_angle = cur;
      pthread_mutex_unlock(&g_lock);

      usleep(speed_step_us(speed));

      if (g_abort)
        {
          return -EINTR;
        }
    }

  return OK;
}

/****************************************************************************
 * 摆头线程
 ****************************************************************************/

static FAR void *sweep_thread(FAR void *arg)
{
  int speed = (int)(intptr_t)arg;
  int ret;

  /* 不持锁：smooth_move() 内部按需短暂持锁，避免整段转动期间阻塞 servo_stop */
  while (g_sweeping && !g_abort)
    {
      ret = smooth_move(g_cur_angle, SERVO_ANGLE_RANGE, speed);
      if (ret < 0)
        {
          break;
        }

      if (!g_sweeping || g_abort)
        {
          break;
        }

      ret = smooth_move(g_cur_angle, 0, speed);
      if (ret < 0)
        {
          break;
        }
    }

  /* 退出摆头后回中 */
  if (!g_abort)
    {
      smooth_move(g_cur_angle, CONFIG_APP_DESKTOP_PET_SERVO_DEFAULT_ANGLE,
                  speed);
    }

  g_sweep_running = false;
  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: servo_init
 ****************************************************************************/

int servo_init(void)
{
#ifndef CONFIG_APP_DESKTOP_PET_SERVO
  syslog(LOG_INFO, "servo: disabled by Kconfig\n");
  return -ENOSYS;
#else
  /* ⚠️ 延迟打开：不在 init 长持 /dev/pwm0。
   * 语音每次唤醒都会起一个独立的 `servo` 进程，它的 g_fd_pwm 初始为 -1，
   * 若这里长持 fd，命令进程拿不到设备、servo_cmd 直接 -ENODEV。
   * 改为首次 servo_cmd 时 lazy-open（见 servo_cmd），daemon 与命令进程各自打开。*/
  g_fd_pwm    = -1;
  g_cur_angle = CONFIG_APP_DESKTOP_PET_SERVO_DEFAULT_ANGLE;
  g_started   = false;
  syslog(LOG_INFO, "servo: init OK (dev=%s 延迟到首次命令时 open)\n",
         SERVO_PWM_DEV);
  return OK;
#endif
}

void servo_deinit(void)
{
#ifdef CONFIG_APP_DESKTOP_PET_SERVO
  servo_stop();

  if (g_sweep_running)
    {
      pthread_join(g_sweep_tid, NULL);
      g_sweep_running = false;
    }

  if (g_fd_pwm >= 0)
    {
      ioctl(g_fd_pwm, PWMIOC_STOP, 0);
      close(g_fd_pwm);
      g_fd_pwm = -1;
      g_started = false;   /* 下次 servo_init 首个角度会重新 START PWM */
    }
#endif
}

/****************************************************************************
 * Name: servo_cmd
 *
 * action: enum servo_action_e
 * value : 角度 / 步长
 * speed : enum servo_speed_e
 ****************************************************************************/

int servo_cmd(uint8_t action, int16_t value, uint8_t speed)
{
#ifndef CONFIG_APP_DESKTOP_PET_SERVO
  (void)action;
  (void)value;
  (void)speed;
  return -ENOSYS;
#else
  int target = g_cur_angle;
  int ret = OK;

  /* 串行化整条命令：同一时刻只有一条 servo 命令在动舵机（见 g_cmd_lock 说明）。
   * 语音并发触发的多条 `servo` 进程会在此排队，避免写 g_cur_angle / /dev/pwm0 打架。*/
  pthread_mutex_lock(&g_cmd_lock);

  /* 延迟打开 PWM 设备：独立 `servo` 命令进程里 g_fd_pwm 初始为 -1，
   * 必须在此自行 open；daemon 也走同一路径（servo_init 不再长持 fd）。
   * 打开后本进程内保持，直到 servo_deinit / 进程退出由 OS 回收。*/
  if (g_fd_pwm < 0)
    {
      g_fd_pwm = open(SERVO_PWM_DEV, O_RDWR);
      if (g_fd_pwm < 0)
        {
          syslog(LOG_ERR, "servo: open %s failed: %d "
                          "(CONFIG_PWM / ESP32S3_LEDC 是否打开？)\n",
                 SERVO_PWM_DEV, errno);
          pthread_mutex_unlock(&g_cmd_lock);
          return -errno;
        }

      g_started = false;  /* 新 fd，首个角度需要重新 START PWM 定时器 */
    }

  if (speed > SERVO_SPEED_FAST)
    {
      speed = SERVO_SPEED_FAST;
    }

  /* ---- 阶段 1：处理摆头线程的打断（持锁，但 join 前必须放锁）---- */
  pthread_mutex_lock(&g_lock);

  if (g_sweeping && action != (uint8_t)SERVO_SWEEP)
    {
      g_sweeping = false;
      g_abort    = true;

      /* ⚠️ 必须先放锁再 join：sweep 线程内部也要拿同一把 g_lock，
       * 持锁 join 会直接死锁。 */
      pthread_mutex_unlock(&g_lock);
      pthread_join(g_sweep_tid, NULL);
      pthread_mutex_lock(&g_lock);

      g_sweep_running = false;
      g_abort         = false;
    }
  else if (action != (uint8_t)SERVO_STOP)
    {
      /* 除 STOP 外，每条新命令都清掉上一次的中断标志 */
      g_abort = false;
    }

  pthread_mutex_unlock(&g_lock);

  /* ---- 阶段 2：执行动作（smooth_move 内部按需短暂持锁）---- */
  switch (action)
    {
      case SERVO_SET_ANGLE:
        target = value;
        if (target < 0)
          {
            target = 0;
          }

        if (target > SERVO_ANGLE_RANGE)
          {
            target = SERVO_ANGLE_RANGE;
          }

        smooth_move(g_cur_angle, target, speed);
        break;

      case SERVO_LEFT:
        target = g_cur_angle - ((value > 0) ? value : 30);
        if (target < 0)
          {
            target = 0;
          }

        smooth_move(g_cur_angle, target, speed);
        break;

      case SERVO_RIGHT:
        target = g_cur_angle + ((value > 0) ? value : 30);
        if (target > SERVO_ANGLE_RANGE)
          {
            target = SERVO_ANGLE_RANGE;
          }

        smooth_move(g_cur_angle, target, speed);
        break;

      case SERVO_CENTER:
        target = CONFIG_APP_DESKTOP_PET_SERVO_DEFAULT_ANGLE;
        smooth_move(g_cur_angle, target, speed);
        break;

      case SERVO_SWEEP:
        {
          int start = 0;

          pthread_mutex_lock(&g_lock);
          if (!g_sweeping)
            {
              g_sweeping      = true;
              g_abort         = false;
              g_sweep_running = true;
              start           = 1;
            }

          pthread_mutex_unlock(&g_lock);

          if (start)
            {
              ret = pthread_create(&g_sweep_tid, NULL, sweep_thread,
                                   (FAR void *)(intptr_t)speed);
              if (ret != 0)
                {
                  syslog(LOG_ERR, "servo: pthread_create failed: %d\n", ret);
                  pthread_mutex_lock(&g_lock);
                  g_sweeping      = false;
                  g_sweep_running = false;
                  pthread_mutex_unlock(&g_lock);
                  ret = -ret;
                }
              /* 不 detach：保留 join 能力，打断时回收线程资源。
               * 自然结束时由下一次非 sweep 命令或 servo_deinit() join。 */
            }
        }
        break;

      case SERVO_STOP:
        pthread_mutex_lock(&g_lock);
        g_sweeping = false;
        g_abort    = true;
        pthread_mutex_unlock(&g_lock);
        break;

      default:
        ret = -EINVAL;
        break;
    }

  syslog(LOG_DEBUG, "servo: action=%d value=%d speed=%d cur=%d ret=%d\n",
         action, value, speed, g_cur_angle, ret);
  pthread_mutex_unlock(&g_cmd_lock);
  return ret;
#endif
}

int servo_get_angle(void)
{
  return g_cur_angle;
}

int servo_stop(void)
{
#ifdef CONFIG_APP_DESKTOP_PET_SERVO
  pthread_mutex_lock(&g_lock);
  g_sweeping = false;
  g_abort    = true;
  pthread_mutex_unlock(&g_lock);
  syslog(LOG_DEBUG, "servo: stop\n");
  return OK;
#else
  return -ENOSYS;
#endif
}

/****************************************************************************
 * Name: servo_greet
 *
 * Description:
 *   主动打招呼（"主动+执行"场景）：设备开机 / 每日日程时，**不依赖语音**主动
 *   摆头打招呼。这是大赛「主动能力 + 工具执行」硬指标的直接证明。
 *   复用已有的 servo_cmd()（内部带平滑移动与打断保护），不引入新并发逻辑。
 ****************************************************************************/

int servo_greet(void)
{
#ifndef CONFIG_APP_DESKTOP_PET_SERVO
  return -ENOSYS;
#else
  syslog(LOG_INFO, "servo: 主动打招呼（开机/日程，无需语音）");
  servo_cmd(SERVO_CENTER, 0,  SERVO_SPEED_MEDIUM);
  usleep(300000);
  servo_cmd(SERVO_LEFT,  40, SERVO_SPEED_MEDIUM);  /* 左看 */
  usleep(300000);
  servo_cmd(SERVO_RIGHT, 80, SERVO_SPEED_MEDIUM);  /* 右看（累计回中+40）*/
  usleep(300000);
  servo_cmd(SERVO_CENTER, 0,  SERVO_SPEED_MEDIUM);  /* 回中 */
  return OK;
#endif
}

int servo_is_sweeping(void)
{
  return g_sweeping ? 1 : 0;
}
