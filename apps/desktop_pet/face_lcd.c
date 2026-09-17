/****************************************************************************
 * apps/desktop_pet/face_lcd.c
 *
 * 表情显示 - ST7789V 8080 并口 (T-Display-S3)
 *
 * 资源：/data/agent/skills/face-expression/assets/face_<id>.bin
 * 格式：RGB565, 170×320, 108800 bytes/张
 *
 * 硬件（引脚由 board 层决定，见 CONFIG_LCD_ST7789_*_GPIO，LilyGO 官方 T-Display-S3）：
 *   - LCD_DC:   GPIO7   (Data/Command)
 *   - LCD_CS:   GPIO6
 *   - LCD_WR:   GPIO8
 *   - LCD_RD:   GPIO9
 *   - LCD_D0~D7: GPIO39,40,41,42,45,46,47,48
 *   - LCD_RST:  GPIO5   LCD_BL: GPIO38   电源EN: GPIO15
 *
 * openvela 已实现 ST7789 并口驱动（drivers/lcd/st7789.c），注册为 /dev/lcd0。
 * 本文件只是 high-level 包装：open("/dev/lcd0") 后 write() RGB565 帧，不直接操作 GPIO。
 *
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>

#include "desktop_pet.h"

#define FACE_W 170
#define FACE_H 320
#define FACE_PIXELS (FACE_W * FACE_H)
#define FACE_BYTES  (FACE_PIXELS * 2)  /* RGB565 */
#define ASSETS_DIR  "/data/agent/skills/face-expression/assets"

static int g_current = 0;
static int g_fd_lcd = -1;
static const char *g_names[FACE_MAX] = {
    "default", "happy", "sad", "surprised", "wink",
    "thinking", "sleeping", "angry", "love"
};

/* 把 RGB565 bin 写到 LCD framebuffer */
static int lcd_blit(const uint8_t *rgb565, int len)
{
    if (g_fd_lcd < 0) return -1;
    if (write(g_fd_lcd, rgb565, len) != len) {
        syslog(LOG_ERR, "face: lcd write failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/* 从文件系统读取表情图片到内存 */
static int load_face(int id, uint8_t **out_buf)
{
    if (id < 0 || id >= FACE_MAX) {
        syslog(LOG_ERR, "face: invalid id %d", id);
        return -1;
    }

    char path[128];
    snprintf(path, sizeof(path), "%s/face_%d_%s.bin",
             ASSETS_DIR, id, g_names[id]);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        syslog(LOG_ERR, "face: cannot open %s: %s", path, strerror(errno));
        return -1;
    }

    uint8_t *buf = (uint8_t *)malloc(FACE_BYTES);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    size_t rd = fread(buf, 1, FACE_BYTES, fp);
    fclose(fp);

    if (rd != FACE_BYTES) {
        syslog(LOG_ERR, "face: short read %s: got %u/%d",
               path, (unsigned)rd, FACE_BYTES);
        free(buf);
        return -1;
    }

    *out_buf = buf;
    return 0;
}

int face_init(void)
{
    syslog(LOG_INFO, "face: 初始化 LCD (ST7789V 8080)");

    /* 打开 LCD 设备（openvela 驱动注册的设备节点）*/
    g_fd_lcd = open("/dev/lcd0", O_WRONLY);
    if (g_fd_lcd < 0) {
        syslog(LOG_ERR, "face: open /dev/lcd0 失败: %s", strerror(errno));
        syslog(LOG_ERR, "face: 确认 defconfig 里有 CONFIG_LCD=y CONFIG_LCD_ST7789=y");
        return -1;
    }

    /* 设置显示区域：170×320 */
    /* 通过 ioctl 或环境变量，取决于具体驱动 */
    /* 这里假设驱动默认就是 170x320 */

    /* 加载默认表情 */
    face_set(FACE_DEFAULT, 0);

    syslog(LOG_INFO, "face: 初始化完成");
    return 0;
}

void face_deinit(void)
{
    if (g_fd_lcd >= 0) {
        close(g_fd_lcd);
        g_fd_lcd = -1;
    }
}

int face_set(uint8_t id, uint16_t dur_ms)
{
    if (id >= FACE_MAX) {
        syslog(LOG_WARN, "face: id %d 越界，夹到 0", id);
        id = 0;
    }

    /* 加载图片 */
    uint8_t *buf = NULL;
    if (load_face(id, &buf) < 0) {
        /* 加载失败：填充全黑，避免显示 garbage */
        syslog(LOG_ERR, "face: 加载表情 %d 失败，保持上一张", id);
        return -1;
    }

    /* 推到 LCD */
    if (lcd_blit(buf, FACE_BYTES) < 0) {
        free(buf);
        return -1;
    }
    free(buf);

    g_current = id;
    syslog(LOG_INFO, "face: set to %s (id=%d, dur=%ums)",
           g_names[id], id, dur_ms);

    /* TODO: 启动一个定时器，dur_ms 后回到默认 */
    /* 简化：用 pthread_create 起个一次性线程 */
    if (dur_ms > 0) {
        /* 这里用最简实现：把 dur_ms 写日志，实际定时由 ai_agent 做 */
        syslog(LOG_INFO, "face: %ums 后会回到默认（由 ai_agent 触发）", dur_ms);
    }

    return 0;
}

int face_get_current(void)
{
    return g_current;
}

int face_get_current_name(char *buf, int len)
{
    if (!buf || len <= 0) return -1;
    if (g_current < 0 || g_current >= FACE_MAX) {
        snprintf(buf, len, "unknown");
        return -1;
    }
    snprintf(buf, len, "%s", g_names[g_current]);
    return 0;
}

int face_list(char *buf, int len)
{
    if (!buf || len <= 0) return -1;
    int n = 0;
    for (int i = 0; i < FACE_MAX; i++) {
        n += snprintf(buf + n, len - n, "%d:%s ", i, g_names[i]);
    }
    return 0;
}
