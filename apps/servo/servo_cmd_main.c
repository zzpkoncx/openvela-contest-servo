/****************************************************************************
 * apps/desktop_pet/servo_cmd_main.c
 *
 * 舵机 NSH 命令 —— 让"语音控制舵机"先能脱离语音、用串口直接验证
 *
 * 为什么需要它：
 *   openvela ai_agent 通过「Markdown Skill + 工具调用」工作。servo-control
 *   Skill（skills/servo-control/SKILL.md）里写的 `vela> servo set 90` 这类
 *   shell 命令，正是 LLM 触发 ai_agent 的 run_shell 工具后实际要执行的命令。
 *   所以本文件把舵机动作注册成一个标准 NSH 内建命令 `servo`，使得：
 *     1) 烧录后，串口里直接敲 `servo set 90` 就能看到舵机转（验证 PWM/引脚/舵机）
 *     2) 语音唤醒"你好，openvela" + 说"舵机转到 90 度"时，ai_agent 经
 *        run_shell 调用 `servo set 90`，完成真正的语音驱动
 *
 * 用法：
 *   servo set <0~270>      转到绝对角度
 *   servo left [步长]      向左转（默认 30°）
 *   servo right [步长]     向右转（默认 30°）
 *   servo center           回到 135° 中间
 *   servo sweep [slow|medium|fast]  来回摆头
 *   servo stop             立即停止
 *   servo status           查询当前角度 / 是否在摆头
 *   servo greet            主动打招呼（手动触发摆头；默认不开机自动）
 *
 * 注册方式：`servo` 作为独立 builtin app（见 apps/servo/Makefile 的
 * PROGNAME=servo），由 NuttX 构建系统注册为标准 NSH 命令；语音经
 * run_shell 调用 `servo set 90` 即触发真正的舵机驱动。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "desktop_pet.h"    /* servo_cmd / servo_get_angle / servo_stop / 枚举 */

/****************************************************************************
 * 命令实现
 ****************************************************************************/

static int speed_from_str(const char *s, uint8_t def)
{
  if (!s)
    return def;
  if (strcmp(s, "fast") == 0)
    return SERVO_SPEED_FAST;
  if (strcmp(s, "medium") == 0)
    return SERVO_SPEED_MEDIUM;
  if (strcmp(s, "slow") == 0)
    return SERVO_SPEED_SLOW;
  return def;
}

int servo_main(int argc, char *argv[])
{
  if (argc < 2)
    {
      printf("usage: servo <set|left|right|center|sweep|stop|status|greet> [val]\n");
      return 1;
    }

  const char *cmd = argv[1];
  int rc = 0;

  if (strcmp(cmd, "set") == 0)
    {
      if (argc < 3)
        {
          printf("servo set: 缺少角度 (0~270)\n");
          return 1;
        }
      int ang = atoi(argv[2]);
      rc = servo_cmd(SERVO_SET_ANGLE, (int16_t)ang, SERVO_SPEED_MEDIUM);
      printf("servo: -> %d°\n", ang);
    }
  else if (strcmp(cmd, "left") == 0)
    {
      int step = (argc >= 3) ? atoi(argv[2]) : 30;
      rc = servo_cmd(SERVO_LEFT, (int16_t)step, SERVO_SPEED_MEDIUM);
      printf("servo: left %d°\n", step);
    }
  else if (strcmp(cmd, "right") == 0)
    {
      int step = (argc >= 3) ? atoi(argv[2]) : 30;
      rc = servo_cmd(SERVO_RIGHT, (int16_t)step, SERVO_SPEED_MEDIUM);
      printf("servo: right %d°\n", step);
    }
  else if (strcmp(cmd, "center") == 0)
    {
      rc = servo_cmd(SERVO_CENTER, 0, SERVO_SPEED_MEDIUM);
      printf("servo: center\n");
    }
  else if (strcmp(cmd, "sweep") == 0)
    {
      uint8_t sp = speed_from_str((argc >= 3) ? argv[2] : NULL,
                                  SERVO_SPEED_SLOW);
      rc = servo_cmd(SERVO_SWEEP, 0, sp);
      printf("servo: sweep (speed=%d)\n", sp);
    }
  else if (strcmp(cmd, "stop") == 0)
    {
      servo_stop();
      printf("servo: stop\n");
    }
  else if (strcmp(cmd, "status") == 0)
    {
      printf("servo: angle=%d sweeping=%d\n",
             servo_get_angle(), servo_is_sweeping());
    }
  else if (strcmp(cmd, "greet") == 0)
    {
      rc = servo_greet();
      printf("servo: greet\n");
    }
  else
    {
      printf("servo: 未知命令 %s\n", cmd);
      printf("usage: servo <set|left|right|center|sweep|stop|status|greet> [val]\n");
      return 1;
    }

  if (rc < 0)
    {
      printf("servo: 执行失败 (rc=%d，检查 /dev/pwm0 与 CONFIG_PWM)\n", rc);
      return 1;
    }

  return 0;
}

/* 注：`servo` 现在是独立 builtin app（apps/servo/），由 NuttX 构建系统
 * 注册为标准 NSH 命令，不再用 NSH_ADDCMD —— NSH_ADDCMD 只能出现在
 * nsh_cmdset[] 数组内部，写在文件作用域链接期会报语法错误。*/
