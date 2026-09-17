/****************************************************************************
 * board/t-display-s3/board.c
 *
 * T-Display-S3 板级初始化实现
 *
 * 关键点：
 *   1. GPIO15 在 setup() 拉高
 *   2. strapping pins (GPIO45/46) 不要乱动
 *   3. LCD 8080 并口 DMA 配置
 *   4. LEDC PWM 配置舵机
 *   5. I2S 配置 INMP441
 *
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <debug.h>
#include <syslog.h>
#include <nuttx/board.h>
#include <nuttx/timers/pwm.h>
#include <arch/board/board.h>
#include <esp32s3_gpio.h>
#include <esp32s3_ledc.h>
#include <esp32s3_i2s.h>

/* GPIO15 拉高（板载 SD 卡 CS 互锁）*/
static int setup_gpio15(void)
{
    syslog(LOG_INFO, "board: GPIO15 拉高");
    esp32s3_gpio_config(15, GPIO_OUTPUT);
    esp32s3_gpio_write(15, 1);
    return OK;
}

/* LCD 8080 并口初始化（仅当 CONFIG_LCD=y 时编译/调用；
 * 舵机占 GPIO42=D3 时 LCD 必须关闭，否则 PWM 灌进屏幕数据线）*/
#ifdef CONFIG_LCD
int t_display_s3_lcd_init(void)
{
    syslog(LOG_INFO, "board: 初始化 LCD (ST7789V 8080, 170x320)");

    /* 配置数据线 D0~D7 为输出 */
    int data_pins[] = {BOARD_LCD_D0, BOARD_LCD_D1, BOARD_LCD_D2, BOARD_LCD_D3,
                       BOARD_LCD_D4, BOARD_LCD_D5, BOARD_LCD_D6, BOARD_LCD_D7};
    for (int i = 0; i < 8; i++) {
        if (data_pins[i] == 11 || data_pins[i] == 12 || data_pins[i] == 13) {
            syslog(LOG_WARN, "board: GPIO%d 是 Flash/PSRAM 占用，跳过", data_pins[i]);
            continue;
        }
        esp32s3_gpio_config(data_pins[i], GPIO_OUTPUT);
    }

    /* 配置控制线 */
    esp32s3_gpio_config(BOARD_LCD_CS, GPIO_OUTPUT);
    esp32s3_gpio_config(BOARD_LCD_DC, GPIO_OUTPUT);
    esp32s3_gpio_config(BOARD_LCD_WR, GPIO_OUTPUT);
    esp32s3_gpio_config(BOARD_LCD_RD, GPIO_OUTPUT);
    esp32s3_gpio_config(BOARD_LCD_RST, GPIO_OUTPUT);
    esp32s3_gpio_config(BOARD_LCD_BL, GPIO_OUTPUT);

    /* 初始电平 */
    esp32s3_gpio_write(BOARD_LCD_CS, 1);   /* CS 高 = 不选中 */
    esp32s3_gpio_write(BOARD_LCD_WR, 1);
    esp32s3_gpio_write(BOARD_LCD_RD, 1);
    esp32s3_gpio_write(BOARD_LCD_DC, 1);
    esp32s3_gpio_write(BOARD_LCD_BL, 1);   /* 背光开 */

    /* 硬件复位 */
    esp32s3_gpio_write(BOARD_LCD_RST, 0);
    usleep(100000);  /* 100ms */
    esp32s3_gpio_write(BOARD_LCD_RST, 1);
    usleep(100000);

    /* ST7789V 初始化序列 */
    /* 这些命令会通过 st7789 驱动发出去 */
    /* 这里只是复位，命令在 driver 里 */

    syslog(LOG_INFO, "board: LCD 初始化完成");
    return OK;
}
#endif /* CONFIG_LCD */

/* 舵机 PWM 初始化
 *
 * ⚠️ 关键点（从 aivox3 工程验证过的写法搬来）：
 *   openvela 里舵机应用层（servo_drv.c）是通过 **PWM 字符设备 /dev/pwm0**
 *   用 ioctl(PWMIOC_SETCHARACTERISTICS / PWMIOC_START) 来驱动的，
 *   **不是**直接写 /sys/class/ledc/...（openvela 没有 sysfs LEDC 节点）。
 *
 *   因此 board 侧必须：
 *     1) esp32s3_ledc_init(timer) 初始化 LEDC 定时器/通道，返回 pwm_lowerhalf_s*
 *     2) pwm_register("/dev/pwm0", lower) 把通道注册成字符设备
 *   引脚（GPIO42，舵机占 D3 故 LCD 关闭）由 menuconfig 的
 *   CONFIG_ESP32S3_LEDC_CHANNEL0_PIN 决定，
 *   这里不做运行时绑定（esp32s3 没有运行时 set_pin API）。 */
int t_display_s3_servo_init(void)
{
#ifndef CONFIG_PWM
    syslog(LOG_INFO, "board: CONFIG_PWM 未开启，舵机不可用（请在 defconfig 加 CONFIG_PWM=y）\n");
    return -ENOSYS;
#else
    FAR struct pwm_lowerhalf_s *ledc;
    int ret;

    syslog(LOG_INFO, "board: 初始化舵机 PWM (GPIO%d, ch%d, timer%d, %dHz)",
           BOARD_SERVO_GPIO, BOARD_SERVO_LEDC_CH,
           BOARD_SERVO_LEDC_TIMER, BOARD_SERVO_FREQ_HZ);

    ledc = esp32s3_ledc_init(BOARD_SERVO_LEDC_TIMER);
    if (ledc == NULL)
      {
        syslog(LOG_ERR, "board: esp32s3_ledc_init(timer=%d) 失败\n",
               BOARD_SERVO_LEDC_TIMER);
        return -ENODEV;
      }

    ret = pwm_register("/dev/pwm0", ledc);
    if (ret < 0)
      {
        syslog(LOG_ERR, "board: pwm_register(/dev/pwm0) 失败: %d\n", ret);
        return ret;
      }

    syslog(LOG_INFO, "board: 舵机 PWM 就绪 /dev/pwm0 (GPIO%d)\n", BOARD_SERVO_GPIO);
    return OK;
#endif
}

/* INMP441 麦克风初始化 */
int t_display_s3_mic_init(void)
{
    syslog(LOG_INFO, "board: 初始化 INMP441 I2S mic");

    esp32s3_gpio_config(BOARD_MIC_SCK_GPIO, GPIO_INPUT);
    esp32s3_gpio_config(BOARD_MIC_WS_GPIO, GPIO_INPUT);
    esp32s3_gpio_config(BOARD_MIC_SD_GPIO, GPIO_INPUT);

    /* I2S 配置：16kHz, 16bit, mono, INMP441 WS 反相 */
    struct esp32s3_i2s_config_s cfg = {
        .sample_rate = 16000,
        .bits_per_sample = 16,
        .channels = 1,
        .ws_invert = BOARD_MIC_WS_INV,
        .sck_pin = BOARD_MIC_SCK_GPIO,
        .ws_pin = BOARD_MIC_WS_GPIO,
        .sd_pin = BOARD_MIC_SD_GPIO,
    };
    esp32s3_i2s_init(0, &cfg);

    syslog(LOG_INFO, "board: 麦克风初始化完成");
    return OK;
}

/* 综合外设初始化 */
int t_display_s3_init_peripherals(void)
{
    syslog(LOG_INFO, "===========================================");
    syslog(LOG_INFO, "T-Display-S3 board init");
    syslog(LOG_INFO, "===========================================");

    setup_gpio15();
#ifdef CONFIG_LCD
    t_display_s3_lcd_init();
#endif
    t_display_s3_servo_init();
    t_display_s3_mic_init();

    syslog(LOG_INFO, "board: 所有外设初始化完成");
    return OK;
}

/* board_late_initialize: 内核启动后期回调
 *
 * ⚠️ 关键修复：当 CONFIG_BOARD_LATE_INITIALIZE=y 时，NuttX 在 up_initialize
 * 阶段会调用本函数。board 层必须把所有外设初始化挂在这里，否则：
 *   - t_display_s3_servo_init() 不会执行 → pwm_register("/dev/pwm0") 不发生
 *   - 应用层 servo_init() 里 open("/dev/pwm0") 直接失败 → 舵机永远不动
 * 这曾是"命令/语音都驱不动舵机"的根因之一（原 board.c 没有实现本函数）。 */
#ifdef CONFIG_BOARD_LATE_INITIALIZE
int board_late_initialize(void)
{
    return t_display_s3_init_peripherals();
}
#endif
