#!/bin/sh
# openvela 开机自启动脚本
# 用途：板子上电后自动配火山引擎 Key、启动语音监听、跑 desktop_pet 守护进程
#
# 安装步骤：
#   scp config/boot_setup.sh root@<board>:/data/
#   ssh root@<board> "chmod +x /data/boot_setup.sh"
#   ssh root@<board> "ln -sf /data/boot_setup.sh /etc/init.d/S99desktop_pet"
#   ssh root@<board> "sync && reboot"

set -e

CONFIG_FILE="/data/.env"
LOG_FILE="/tmp/boot_setup.log"
PID_FILE="/tmp/desktop_pet.pid"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$LOG_FILE"
}

# ============ 0. 准备日志 ============
echo "========================================" >> "$LOG_FILE"
log "boot_setup 启动 (pid=$$)"

# ============ 1. 等待 WiFi（最多 30 秒）============
log "[1/5] 等待 WiFi 联网..."
WIFI_OK=0
for i in $(seq 1 30); do
    if ping -c 1 -W 1 8.8.8.8 >/dev/null 2>&1; then
        WIFI_OK=1
        log "  WiFi 已连接 (耗时 ${i}s)"
        break
    fi
    sleep 1
done
if [ "$WIFI_OK" = "0" ]; then
    log "  WARN: WiFi 未连上，voice 启动会失败"
fi

# ============ 2. 加载 .env ============
log "[2/5] 加载 $CONFIG_FILE..."
if [ ! -f "$CONFIG_FILE" ]; then
    log "  ERROR: $CONFIG_FILE 不存在"
    log "  跳过自启动，请手动配置"
    exit 1
fi
. "$CONFIG_FILE"

# ============ 3. 检查 Skills ============
log "[3/5] 检查 Skills..."
SKILLS_DIR="/data/agent/skills"
if [ ! -d "$SKILLS_DIR" ]; then
    log "  ERROR: $SKILLS_DIR 不存在，请先部署 Skills"
    log "  scp -r skills/ root@board:/data/agent/"
    exit 1
fi
SKILL_COUNT=$(ls -1 "$SKILLS_DIR" 2>/dev/null | wc -l)
log "  发现 $SKILL_COUNT 个 Skills: $(ls $SKILLS_DIR)"

# ============ 4. 配置 ai_agent（写命令队列，等 ai_agent 启动后消费）============
log "[4/5] 配置 ai_agent..."
VELA_CMD_LOG="/tmp/vela_commands.log"
rm -f "$VELA_CMD_LOG"

if [ -n "$VOLC_API_KEY" ]; then
    echo "set_volc_key $VOLC_API_KEY" >> "$VELA_CMD_LOG"
    log "  queued: set_volc_key"
fi

if [ -n "$VOLC_ASR_APP_ID" ] && [ -n "$VOLC_ASR_ACCESS_TOKEN" ]; then
    echo "set_volc_asr $VOLC_ASR_APP_ID $VOLC_ASR_ACCESS_TOKEN volcengine_streaming_common" >> "$VELA_CMD_LOG"
    log "  queued: set_volc_asr"
fi

# ============ 5. 启动 desktop_pet 守护进程 ============
log "[5/5] 启动 desktop_pet..."
if [ -x /usr/bin/desktop_pet ]; then
    /usr/bin/desktop_pet -d -p "$PID_FILE" &
    log "  desktop_pet pid=$!"
else
    log "  WARN: /usr/bin/desktop_pet 不存在（可能还没编译/烧录）"
fi

# ============ 6. 可选：自动启动 voice_start ============
if [ "$AUTO_VOICE_START" = "true" ] && [ "$WIFI_OK" = "1" ]; then
    DELAY="${VOICE_START_DELAY:-15}"
    log "将在 ${DELAY}s 后入队 voice_start..."
    ( sleep "$DELAY"; echo "voice_start" >> "$VELA_CMD_LOG" ) &
fi

log "boot_setup 完成"
exit 0
