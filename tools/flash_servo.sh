#!/usr/bin/env bash
# =============================================================================
# flash_servo.sh —— openvela 真机(ESP32-S3 / T-Display-S3)一键烧录 + 舵机验证
#
# 适用环境: 你的 Ubuntu 编译机(板子通过 USB 直通/直连到这台机器)
# 说明: 官方 quickstart 只讲了 goldfish 模拟器, 真机烧录官方文档没给, 这里按
#       NuttX ESP32-S3 标准镜像布局自动拼 esptool 命令, 偏移量与实际产物对齐。
#
# 用法:
#   ./flash_servo.sh                 # 交互式: 检测串口 + 编译产物, 只显示烧录命令(不真烧)
#   ./flash_servo.sh --go            # 真正执行 esptool 烧录
#   ./flash_servo.sh --port /dev/ttyUSB0 --go
#   OPENVELA_ROOT=/path/to/openvela ./flash_servo.sh --go
# =============================================================================
set -euo pipefail

OPENVELA_ROOT="${OPENVELA_ROOT:-$HOME/openvela}"
BAUD=921600
CHIP=esp32s3
PORT=""
GO=0
ARGS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --go)    GO=1 ;;
    --port)  PORT="$2"; shift ;;
    *)       ARGS+=("$1") ;;
  esac
  shift
done

# ---- esptool 命令(兼容 esptool / esptool.py) ----
ESPTOOL="$(command -v esptool.py 2>/dev/null || command -v esptool 2>/dev/null || true)"
if [ -z "$ESPTOOL" ]; then
  echo "[!] 未找到 esptool, 请先安装:  pip install esptool"
  exit 1
fi

# ---- 1. 检测串口端口 ----
if [ -z "$PORT" ]; then
  mapfile -t PORTS < <(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true)
  if [ ${#PORTS[@]} -eq 0 ]; then
    echo "[!] 未检测到任何串口设备 (/dev/ttyUSB* / /dev/ttyACM*)。"
    echo "    请确认: 板子已用 USB 数据线连接、且 ls /dev/tty* 能看到新设备。"
    exit 1
  fi
  if [ ${#PORTS[@]} -gt 1 ]; then
    echo "检测到多个串口: ${PORTS[*]}"
    read -r -p "请选择端口(默认 ${PORTS[0]}): " SEL
    PORT="${SEL:-${PORTS[0]}}"
  else
    PORT="${PORTS[0]}"
  fi
fi
echo "[*] 使用端口: $PORT"

# ---- 2. 定位编译产物目录 ----
OUT=""
for cand in \
  "$OPENVELA_ROOT/cmake_out/vela_esp32s3-eye" \
  "$OPENVELA_ROOT/cmake_out/vela_t-display-s3" ; do
  [ -d "$cand" ] && { OUT="$cand"; break; }
done
if [ -z "$OUT" ]; then
  # 退而求其次: 在 cmake_out 下找含 nuttx.bin 的 esp32s3 目录
  OUT="$(find "$OPENVELA_ROOT/cmake_out" -maxdepth 2 -name nuttx.bin 2>/dev/null | head -1 | xargs -r dirname 2>/dev/null || true)"
fi
if [ -z "$OUT" ] || [ ! -d "$OUT" ]; then
  echo "[!] 未找到编译产物 (已在 cmake_out/ 下查找含 nuttx.bin 的目录)。"
  echo "    最可靠的做法是直接跑一键挂代码+编译脚本产出固件:"
  echo "      cd <openvela-source-root>"
  echo "      ./apply_and_build.sh /path/to/openvela_contest"
  echo "    该脚本会以官方迁移后的真实目标编译 (esp32s3-eye 母本 + nsh 基 config):"
  echo "      ./build.sh vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/configs/nsh --cmake -j\$(nproc)"
  echo "    (产物目录名由 build.sh 决定, 形如 cmake_out/vela_esp32s3-eye/, 本脚本会自动查找)"
  exit 1
fi
echo "[*] 编译产物目录: $OUT"

# ---- 3. 收集镜像, 拼出正确的烧录偏移 ----
BL=""; PT=""; NX=""; MERGED=""
[ -f "$OUT/bootloader.bin" ]      && BL="$OUT/bootloader.bin"
[ -f "$OUT/partition-table.bin" ]  && PT="$OUT/partition-table.bin"
[ -f "$OUT/nuttx.bin" ]           && NX="$OUT/nuttx.bin"
[ -f "$OUT/nuttx.merged.bin" ]     && MERGED="$OUT/nuttx.merged.bin"

FLASH_ARGS=()
if [ -n "$MERGED" ]; then
  # 合并镜像: 直接烧到 0x0
  FLASH_ARGS=( "0x0" "$MERGED" )
elif [ -n "$NX" ]; then
  [ -n "$BL" ] && FLASH_ARGS+=( "0x0"    "$BL" )
  [ -n "$PT" ] && FLASH_ARGS+=( "0x8000" "$PT" )
  FLASH_ARGS+=( "0x10000" "$NX" )
else
  echo "[!] 未找到可烧录镜像 (nuttx.bin / nuttx.merged.bin)。请检查编译是否成功。"
  exit 1
fi

CMD=( "$ESPTOOL" --chip "$CHIP" --port "$PORT" --baud "$BAUD" write_flash "${FLASH_ARGS[@]}" )

echo "[*] 建议烧录命令:"
echo "      ${CMD[*]}"
echo

if [ $GO -eq 1 ]; then
  echo ">>> 开始烧录 (chip=$CHIP port=$PORT baud=$BAUD) ..."
  # 先进入下载模式(多数板子自动 DTR/RTS; 若失败请手动按住 BOOT 再复位)
  "${CMD[@]}"
  echo
  echo ">>> 烧录完成! 请按一下板子 RST/EN 复位。"
  echo ">>> 打开串口监控查看开机日志 (波特率 115200):"
  echo "      picocom -b 115200 $PORT      # 或:  minicom -D $PORT -b 115200"
  echo
  echo ">>> 舵机/语音验证(在串口控制台里输入):"
  echo "      ls /dev              # 确认有 pwm0"
  echo "      servo set 90         # 看 270° 舵机是否转到 90°"
  echo "      servo sweep slow     # 摆头(主动打招呼同款动作)"
  echo "      set_llm kimi <你的Key>"
  echo "      set_volc_key <火山引擎Key>"
  echo "      set_volc_asr"
  echo "      voice_start"
  echo "      说: 你好, openvela, 舵机转到 90 度"
else
  echo "(未加 --go, 仅显示命令。确认偏移无误后加 --go 真正烧录)"
  echo "提示: 若产物里出现了 nuttx.merged.bin, 它会整体烧到 0x0, 比分散烧更安全。"
fi
