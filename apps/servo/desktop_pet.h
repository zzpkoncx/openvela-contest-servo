/****************************************************************************
 * apps/desktop_pet/desktop_pet.h
 *
 * 桌面小跟班 - 公共头文件
 *
 ****************************************************************************/

#ifndef __DESKTOP_PET_H
#define __DESKTOP_PET_H

#include <stdint.h>
#include <stdbool.h>

/* 表情 ID 枚举（与 SKILL.md 一致）*/
enum face_id_e {
    FACE_DEFAULT   = 0,  /* 默认 */
    FACE_HAPPY     = 1,  /* 开心 */
    FACE_SAD       = 2,  /* 难过 */
    FACE_SURPRISED = 3,  /* 惊讶 */
    FACE_WINK      = 4,  /* 眨眼 */
    FACE_THINKING  = 5,  /* 思考 */
    FACE_SLEEPING  = 6,  /* 睡觉 */
    FACE_ANGRY     = 7,  /* 愤怒 */
    FACE_LOVE      = 8,  /* 爱心眼 */
    FACE_MAX
};

/* 舵机动作枚举 */
enum servo_action_e {
    SERVO_SET_ANGLE = 0,
    SERVO_LEFT      = 1,
    SERVO_RIGHT     = 2,
    SERVO_CENTER    = 3,
    SERVO_SWEEP     = 4,
    SERVO_STOP      = 5,
};

/* 舵机速度档位 */
enum servo_speed_e {
    SERVO_SPEED_STOP   = 0,
    SERVO_SPEED_SLOW   = 1,  /* ~150ms/60° */
    SERVO_SPEED_MEDIUM = 2,  /* ~80ms/60° */
    SERVO_SPEED_FAST   = 3,  /* ~50ms/60° */
};

/* 全局配置 */
struct pet_config_s {
    char volc_api_key[128];
    char volc_asr_app_id[64];
    char volc_asr_access_token[128];
    char daily_briefing_at[8];  /* "HH:MM" */
    bool auto_voice_start;
};

/* face_lcd.c 接口 */
int  face_init(void);
void face_deinit(void);
int  face_set(uint8_t id, uint16_t dur_ms);
int  face_get_current(void);
int  face_get_current_name(char *buf, int len);
int  face_list(char *buf, int len);

/* servo_drv.c 接口 */
int  servo_init(void);
void servo_deinit(void);
int  servo_cmd(uint8_t action, int16_t value, uint8_t speed);
int  servo_get_angle(void);
int  servo_stop(void);
int  servo_is_sweeping(void);
int  servo_greet(void);    /* 主动打招呼：默认需手动触发（servo greet 或开 AUTOGREET）*/

/* voice_bridge.c 接口 */
int  voice_bridge_init(struct pet_config_s *cfg);
void voice_bridge_deinit(void);

#endif /* __DESKTOP_PET_H */
