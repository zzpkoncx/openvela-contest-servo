/****************************************************************************
 * apps/desktop_pet/desktop_pet_main.c
 *
 * 桌面小跟班 - 主程序入口
 *
 * 职责：
 *   1. 启动后台守护线程
 *   2. 加载 .env 配置
 *   3. 初始化 LCD 表情模块
 *   4. 初始化舵机模块
 *   5. 注册 ai_agent 桥接器
 *   6. 启动每日简报定时任务
 *
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/boardctl.h>
#include <syslog.h>

#include <time.h>

#include "desktop_pet.h"

/* 全局运行标志 */
static volatile int g_running = 1;

/* PID 文件路径 */
#define PID_FILE "/tmp/desktop_pet.pid"
#define LOG_FILE "/tmp/desktop_pet.log"
#define ENV_FILE "/data/.env"

/* 信号处理：优雅退出 */
static void signal_handler(int sig)
{
    syslog(LOG_INFO, "desktop_pet: 收到信号 %d，准备退出", sig);
    g_running = 0;
}

/* 加载 .env 到全局配置 */
static int load_env(struct pet_config_s *cfg)
{
    FILE *fp = fopen(ENV_FILE, "r");
    if (!fp) {
        syslog(LOG_WARN, "desktop_pet: %s 不存在，使用默认配置", ENV_FILE);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        /* 跳过注释和空行 */
        if (line[0] == '#' || line[0] == '\n') continue;

        /* 解析 KEY=VALUE */
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        /* 去除尾部换行 */
        char *nl = strchr(val, '\n');
        if (nl) *nl = '\0';

        if (strcmp(key, "VOLC_API_KEY") == 0)
            strncpy(cfg->volc_api_key, val, sizeof(cfg->volc_api_key) - 1);
        else if (strcmp(key, "VOLC_ASR_APP_ID") == 0)
            strncpy(cfg->volc_asr_app_id, val, sizeof(cfg->volc_asr_app_id) - 1);
        else if (strcmp(key, "VOLC_ASR_ACCESS_TOKEN") == 0)
            strncpy(cfg->volc_asr_access_token, val, sizeof(cfg->volc_asr_access_token) - 1);
        else if (strcmp(key, "DAILY_BRIEFING_AT") == 0)
            strncpy(cfg->daily_briefing_at, val, sizeof(cfg->daily_briefing_at) - 1);
        else if (strcmp(key, "AUTO_VOICE_START") == 0)
            cfg->auto_voice_start = (strcmp(val, "true") == 0);
    }
    fclose(fp);

    syslog(LOG_INFO, "desktop_pet: .env 加载完成");
    return 0;
}

/* 写 PID 文件 */
static void write_pidfile(void)
{
    FILE *fp = fopen(PID_FILE, "w");
    if (fp) {
        fprintf(fp, "%d\n", getpid());
        fclose(fp);
    }
}

/* 主线程：心跳 + 每日简报定时检测 */
static void *heartbeat_thread(void *arg)
{
    struct pet_config_s *cfg = (struct pet_config_s *)arg;
    int idle_sec = 0;
    int last_min = -1;

    syslog(LOG_INFO, "desktop_pet: 心跳线程启动");

    while (g_running) {
        sleep(1);
        idle_sec++;

        /* 每 60 秒打印一次心跳 */
        if (idle_sec % 60 == 0) {
#ifdef CONFIG_APP_DESKTOP_PET_FACE
            syslog(LOG_INFO, "desktop_pet: 心跳 idle=%ds face=%d servo=%d",
                   idle_sec, face_get_current(), servo_get_angle());
#else
            syslog(LOG_INFO, "desktop_pet: 心跳 idle=%ds servo=%d",
                   idle_sec, servo_get_angle());
#endif
        }

        /* 5 分钟无活动 → 切到"睡觉"表情 */
        if (idle_sec == 300) {
#ifdef CONFIG_APP_DESKTOP_PET_FACE
            face_set(6, 0);  /* 6 = 睡觉 */
#endif
        }

        /* 每日简报定时检测（粗略版，每分钟查一次）*/
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        int cur_min = t->tm_hour * 60 + t->tm_min;

        if (last_min != cur_min) {
            last_min = cur_min;

            /* 解析配置里的 "HH:MM" */
            int h = 7, m = 0;
            sscanf(cfg->daily_briefing_at, "%d:%d", &h, &m);
            int cfg_min = h * 60 + m;

            if (cur_min == cfg_min) {
                syslog(LOG_INFO, "desktop_pet: 触发每日简报（主动场景）");
#ifdef CONFIG_APP_DESKTOP_PET_FACE
                face_set(5, 0);  /* 思考 */
#endif
#ifdef CONFIG_APP_DESKTOP_PET_SERVO_AUTOGREET
                servo_greet();   /* 主动摆头打招呼（仅当 AUTOGREET=y）*/
#endif
                /* TODO: 调用 voice_bridge 触发简报语音 */
            }
        }
    }

    syslog(LOG_INFO, "desktop_pet: 心跳线程退出");
    return NULL;
}

/* 入口：支持前台/守护模式 */
int main(int argc, char *argv[])
{
    int daemon = 0;
    int ch;
    while ((ch = getopt(argc, argv, "dh")) != -1) {
        switch (ch) {
            case 'd': daemon = 1; break;
            case 'h':
                printf("usage: desktop_pet [-d] [-h]\n");
                printf("  -d  守护模式（后台）\n");
                printf("  -h  帮助\n");
                return 0;
            default: break;
        }
    }

    /* 守护模式 */
    if (daemon) {
        pid_t pid = fork();
        if (pid > 0) {
            printf("desktop_pet: 守护进程 pid=%d\n", pid);
            return 0;
        }
        if (pid < 0) {
            fprintf(stderr, "fork failed\n");
            return 1;
        }
        setsid();
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    /* 注册信号 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 写 PID */
    write_pidfile();

    /* 初始化 syslog */
    openlog("desktop_pet", LOG_PID | LOG_NDELAY, LOG_DAEMON);
    syslog(LOG_INFO, "===========================================");
    syslog(LOG_INFO, "desktop_pet 启动 (build " __DATE__ " " __TIME__ ")");
    syslog(LOG_INFO, "===========================================");

    /* 加载配置 */
    struct pet_config_s cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.auto_voice_start = true;
    strncpy(cfg.daily_briefing_at, "07:00", sizeof(cfg.daily_briefing_at));
    load_env(&cfg);

    /* 初始化各模块 */
#ifdef CONFIG_APP_DESKTOP_PET_FACE
    face_init();
#endif
    servo_init();

    /* 舵机默认只在收到显式指令时才动（语音/串口 servo 命令），开机不自动摆头。
     * 若开启 CONFIG_APP_DESKTOP_PET_SERVO_AUTOGREET，才开机主动打招呼。*/
#ifdef CONFIG_APP_DESKTOP_PET_SERVO_AUTOGREET
    servo_greet();
#endif

    voice_bridge_init(&cfg);

    /* 启动心跳线程 */
    pthread_t hb_tid;
    pthread_create(&hb_tid, NULL, heartbeat_thread, &cfg);
    pthread_detach(hb_tid);

    /* 主循环：等待信号 */
    syslog(LOG_INFO, "desktop_pet: 进入主循环，按 SIGTERM 退出");
    while (g_running) {
        pause();
    }

    /* 清理 */
    syslog(LOG_INFO, "desktop_pet: 关闭中...");
    voice_bridge_deinit();
    servo_deinit();
#ifdef CONFIG_APP_DESKTOP_PET_FACE
    face_deinit();
#endif

    /* 删除 PID */
    unlink(PID_FILE);

    syslog(LOG_INFO, "desktop_pet: 退出");
    closelog();
    return 0;
}
