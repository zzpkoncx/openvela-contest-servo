/****************************************************************************
 * board/t-display-s3/board.h
 *
 * T-Display-S3 (LilyGO ESP32-S3R8) 板级头文件
 * 参考: https://github.com/Xinyuan-LilyGO/T-Display-S3
 *
 * 重要引脚（已按 LilyGO 官方 T-Display-S3 引脚图校准，wiki + 官方仓库 PinMap 一致）:
 *   - LCD: ST7789V 8080 并口, 170×320, 官方引脚如下
 *   - 电池: ADC GPIO4
 *   - 按钮: GPIO14 (BOOT), GPIO0 (另一个)
 *   - 舵机: GPIO42（用户指定"42 端口"；GPIO42 = LCD 数据线 D3，故舵机占 42 时必须关屏，
 *          见下方 servo 段与 defconfig 的 LCD 关闭说明；想保留屏幕则改 GPIO21）
 *   - 麦克风: GPIO1/2/16 (INMP441 I2S，与 LCD 8080 无冲突：LCD 数据线用 GPIO39-48)
 *   - 背光: GPIO38
 *
 * 启动约束:
 *   - GPIO15 在 setup() 中必须拉高（板载上拉已含，但需保险）
 *   - GPIO45/46 是 strapping pins，启动时不可随意改
 *   - GPIO11/12/13 被 Flash/PSRAM 占用
 *
 ****************************************************************************/

#ifndef __BOARDS_T_DISPLAY_S3_H
#define __BOARDS_T_DISPLAY_S3_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 频率 */
#define BOARD_XTAL_FREQ        40000000   /* 外部晶振 40MHz */
#define BOARD_CPU_FREQ_MHZ     240

/* GPIO15 setup() 拉高（板载 SD 卡/CS 复用，保险起见）*/
#define BOARD_GPIO15_HOLD_HIGH  1

/* LCD 引脚 */
#define BOARD_LCD_WIDTH        170
#define BOARD_LCD_HEIGHT       320
#define BOARD_LCD_XOFFSET      35
#define BOARD_LCD_YOFFSET      0

/* LCD 8080 并口 —— LilyGO T-Display-S3 官方引脚（wiki + 官方仓库 PinMap 一致）
 * 关键: 数据线 D0-D7 = GPIO39,40,41,42,45,46,47,48（不是 4-7/15-18），
 * 这样 GPIO16 不被 LCD 占用，INMP441 的 SD=GPIO16 才能安全使用。*/
#define BOARD_LCD_D0           39
#define BOARD_LCD_D1           40
#define BOARD_LCD_D2           41
#define BOARD_LCD_D3           42
#define BOARD_LCD_D4           45
#define BOARD_LCD_D5           46
#define BOARD_LCD_D6           47
#define BOARD_LCD_D7           48
#define BOARD_LCD_CS           6
#define BOARD_LCD_DC           7
#define BOARD_LCD_WR           8
#define BOARD_LCD_RD           9
#define BOARD_LCD_RST          5
#define BOARD_LCD_BL           38   /* 背光（非 strapping pin） */

/* 舵机引脚 —— 单一来源，改一处即可换脚（C 代码不动）。
 *
 * ⚠️ 经 LilyGO 官方引脚图（4 个一手来源交叉确认：LilyGO T-Display-S3 仓库 README、
 *    espressif 官方 arduino-esp32 的 lilygo_t_display_s3 引脚定义、russhughes /
 *    peterhinch 的 MicroPython ST7789 驱动）确认：
 *
 *    T-Display-S3 真实引脚（LCD 8080 并口，PCB 硬布线）：
 *      D0=39 D1=40 D2=41 D3=42 D4=45 D5=46 D6=47 D7=48
 *      CS=6 DC=7 WR=8 RD=9 RST=5 BL=38   GPIO15=外设电源(必须拉高)
 *
 *    → GPIO42 = LCD 屏幕数据线 D3，**硬件硬布线**。用户指定舵机插"42 端口"，
 *      故本配置把舵机设在 GPIO42，并**同时关闭 LCD（CONFIG_LCD=n）**，
 *      让 GPIO42 空出来给舵机 PWM，屏幕功能牺牲（face 模块也随之关闭）。
 *    → GPIO24 在 ESP32-S3 模组（WROOM-1/N8R8）上**根本没引出**（模组只引 0-21、33-48）。
 *
 *    若想保留屏幕：把舵机改到空闲脚 GPIO21（或 GPIO10），只改两处 + defconfig 两个 CONFIG，
 *    并把 LCD 重新打开（CONFIG_LCD=y + ST7789 段恢复）：
 *      board.h:                 BOARD_SERVO_GPIO = 21
 *      defconfig:               CONFIG_ESP32S3_LEDC_CHANNEL0_PIN=21、
 *                               CONFIG_APP_DESKTOP_PET_SERVO_GPIO=21、
 *                               CONFIG_LCD=y（及 ST7789 段恢复）
 *    底部排针用户实际能插的 GPIO 只有：1,2,3,10,11,12,13,16,17,18,21,43,44
 *      （其中 GPIO21 空闲不冲突；避开 LCD 39-48、I2C 17/18、mic 1/2、串口 RX 3、
 *       USB-JTAG 43/44、strapping 0/45/46、Flash/PSRAM 11/12/13）
 *
 *    当前默认 GPIO42（用户指定"42 端口"，屏幕关闭）。换脚只改下面 1 行 + defconfig 两个 CONFIG。 */
#define BOARD_SERVO_GPIO       42
#define BOARD_SERVO_LEDC_CH    0
#define BOARD_SERVO_LEDC_TIMER 0
#define BOARD_SERVO_FREQ_HZ    50
#define BOARD_SERVO_RES_BITS   14

/* INMP441 麦克风 (I2S) */
#define BOARD_MIC_SCK_GPIO     2
#define BOARD_MIC_WS_GPIO      1
#define BOARD_MIC_SD_GPIO      16
#define BOARD_MIC_WS_INV       1   /* INMP441 要求 WS 左声道高电平 */

/* 按钮 */
#define BOARD_BTN_BOOT_GPIO    14
#define BOARD_BTN_USER_GPIO    0

/* 电池 ADC */
#define BOARD_BATTERY_ADC_GPIO 4

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* board.c 实现的初始化函数 */
int t_display_s3_init_peripherals(void);
int t_display_s3_lcd_init(void);
int t_display_s3_servo_init(void);
int t_display_s3_mic_init(void);

#endif /* __BOARDS_T_DISPLAY_S3_H */
