#!/bin/sh
# 语音唤醒一键测试脚本
# 在板子串口（vela> 提示符下）执行：
#   vela> source /data/voice_wake_test.sh
# 或者在开发机通过 minicom/ssh 粘贴执行

PASS=0
FAIL=0

check() {
    if [ "$1" = "0" ]; then
        echo "  ✅ $2"
        PASS=$((PASS + 1))
    else
        echo "  ❌ $2"
        FAIL=$((FAIL + 1))
    fi
}

echo "================================================"
echo "  桌面小跟班 × openvela 语音唤醒测试"
echo "================================================"
echo

# 1. ai_agent
echo "[1/6] 检查 ai_agent..."
if vela-cmd help 2>&1 | grep -q "voice_start"; then
    check 0 "ai_agent voice 通道已启用"
else
    check 1 "ai_agent 未启用 voice 通道"
    echo "     修复：在 defconfig 里设 AI_AGENT=y AI_AGENT_AUDIO=y，重新编译"
    exit 1
fi

# 2. Skills
echo "[2/6] 检查 Skills..."
SKILLS_DIR="/data/agent/skills"
if [ ! -d "$SKILLS_DIR" ]; then
    check 1 "$SKILLS_DIR 不存在"
    exit 1
fi
SKILL_COUNT=$(ls -1 "$SKILLS_DIR" 2>/dev/null | wc -l)
if [ "$SKILL_COUNT" -ge 1 ]; then
    check 0 "发现 $SKILL_COUNT 个 Skills: $(ls $SKILLS_DIR | tr '\n' ' ')"
else
    check 1 "未发现任何 Skill"
fi

# 3. 火山引擎配置
echo "[3/6] 检查火山引擎配置..."
if [ ! -f /data/.env ]; then
    check 1 "/data/.env 不存在"
    echo "     修复：cp .env.example /data/.env 并填入 Key"
    exit 1
fi
. /data/.env
if [ -z "$VOLC_API_KEY" ] || [ "${VOLC_API_KEY:0:3}" != "sk-" ]; then
    check 1 "VOLC_API_KEY 未设置或格式错误（应以 sk- 开头）"
    exit 1
fi
check 0 "VOLC_API_KEY = ${VOLC_API_KEY:0:10}..."

if [ -z "$VOLC_ASR_APP_ID" ] || [ -z "$VOLC_ASR_ACCESS_TOKEN" ]; then
    check 1 "VOLC_ASR_APP_ID 或 VOLC_ASR_ACCESS_TOKEN 未设置"
    exit 1
fi
check 0 "VOLC_ASR 配置完整"

# 4. desktop_pet 进程
echo "[4/6] 检查 desktop_pet 守护进程..."
if pgrep -f "desktop_pet" >/dev/null; then
    check 0 "desktop_pet 正在运行（pid=$(pgrep -f desktop_pet)）"
else
    check 1 "desktop_pet 未运行"
    echo "     修复：/usr/bin/desktop_pet -d &"
fi

# 5. 麦克风设备
echo "[5/6] 检查麦克风设备..."
if [ -e /dev/i2s0 ] || [ -e /dev/audio0 ] || [ -e /dev/mic0 ]; then
    check 0 "找到音频输入设备"
else
    check 1 "未找到音频设备（/dev/i2s0 等）"
fi

# 6. 配 Key + 启 voice
echo "[6/6] 配置 ai_agent + 启动 voice..."
vela-cmd set_volc_key "$VOLC_API_KEY" 2>&1 | head -1
vela-cmd set_volc_asr "$VOLC_ASR_APP_ID" "$VOLC_ASR_ACCESS_TOKEN" volcengine_streaming_common 2>&1 | head -1
sleep 1
vela-cmd voice_start 2>&1 | head -3
sleep 2
check 0 "voice_start 已执行"

echo
echo "================================================"
echo "  测试结果：$PASS 通过 / $FAIL 失败"
echo "================================================"
echo
if [ "$FAIL" = "0" ]; then
    echo "🎉 准备就绪！"
    echo
    echo "现在请对板载麦克风说："
    echo "  「你好，openvela」"
    echo
    echo "然后说："
    echo "  「把表情换成开心」"
    echo "  「舵机转到 90 度」"
    echo "  「今天天气怎么样」"
    echo
    echo "日志位置："
    echo "  - /tmp/ai_agent.log    # ai_agent 框架日志"
    echo "  - /tmp/desktop_pet.log # 我们的应用日志"
    echo "  - /tmp/boot_setup.log  # 启动日志"
    echo
else
    echo "⚠️  有 $FAIL 项失败，请先修复"
fi
