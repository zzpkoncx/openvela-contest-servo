#!/bin/bash
# ===========================================================================
# install_skill.sh —— 把自定义 Skill 推到真机 /data/agent/skills/
# ===========================================================================
# 为什么必须有这一步（"喊了没用"的头号原因）：
#   openvela 的 ai_agent 只会加载位于 /data/agent/skills/<name>/SKILL.md 的 Skill。
#   如果本仓库的 skills/servo-control 没被推上板子，LLM 根本不知道有 `servo`
#   这个命令，语音"往左转"也就永远不会被执行。
#
# 用法（在已 adb 连上烧录好固件的 Ubuntu 机器上）：
#   ./tools/install_skill.sh            # 推 skills + .env
#   ./tools/install_skill.sh --no-env   # 只推 skills，不推密钥
#   SKILL_SRC=../skills DEST=/data/agent/skills ./tools/install_skill.sh
# ===========================================================================
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

SKILL_SRC="${SKILL_SRC:-$ROOT/skills}"
DEST="${DEST:-/data/agent/skills}"
ENV_SRC="$ROOT/.env"

echo "==> 检查 adb ..."
if ! command -v adb >/dev/null 2>&1; then
  echo "ERROR: 找不到 adb，请先安装 android-tools-adb 并确保板子已 USB 连接、adb 授权"
  exit 1
fi

echo "==> 设备列表："
adb devices

echo "==> 在板子创建 $DEST ..."
adb shell "mkdir -p $DEST"

echo "==> 推送 skills/ 下所有 Skill 到 $DEST ..."
for d in "$SKILL_SRC"/*/; do
  name="$(basename "$d")"
  if [ -f "$d/SKILL.md" ]; then
    echo "    -> $name"
    adb push "$d" "$DEST/"
  fi
done

if [ "$1" != "--no-env" ] && [ -f "$ENV_SRC" ]; then
  echo "==> 推送 .env（火山引擎密钥）到 /data/.env ..."
  adb push "$ENV_SRC" /data/.env
else
  echo "==> 跳过 .env（用 --no-env 或缺少 .env 文件）"
fi

echo ""
echo "==> 完成。在板子串口执行以下命令让 ai_agent 重新扫描 Skill 并启动语音："
echo "    vela> voice_stop"
echo "    vela> voice_start"
echo "    vela> set_volc_asr"
echo "然后说：『你好，openvela，舵机往左转』"
