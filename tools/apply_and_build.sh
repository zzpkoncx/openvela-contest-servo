#!/usr/bin/env bash
# ============================================================
# apply_and_build.sh  —— 把 openvela_contest 里的舵机/语音代码挂进
# 完整 openvela 源码树, 并编译出可烧录固件 (在 Ubuntu 22.04 编译机运行)。
#
# 设计要点:
#   - 真正能编译的目标 = 官方 in-tree 的 esp32s3-eye 母本 + 我们的
#     defconfig.addition (GPIO42 关屏版). esp32s3-eye 与 T-Display-S3
#     同为 ESP32-S3R8, 且本配置已关 LCD、只用芯片级 LEDC, 烧到
#     T-Display-S3 真机即可驱动 GPIO42 舵机, 无需新建板级 Kconfig。
#   - esp32s3-eye 已于 2026-05-26 (commit bb5cd7e
#     "relocate board to vendor/espressif overlay") 从
#     boards/xtensa/esp32s3/esp32s3-eye 迁移到
#     vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/。
#     本脚本优先直接命中该迁移后的新路径, 并用 find 做兼容回退。
#   - 基 config 选用 nsh (esp32s3-eye 可用 configs: gpio/i2c/lcd/nsh/wifi,
#     没有单一默认 config); nsh 已含 NSH shell 与基础外设, 适合在其上叠加
#     ai_agent / LEDC / servo。
#   - 同时 best-effort 复制出一个命名板 t-display-s3 (整目录复制 eye
#     再覆盖 board.c/board.h/defconfig), 方便想用命名板的用户 (需在
#     menuconfig 手动注册 BOARD_T_DISPLAY_S3)。
#
# 前置:
#   - 已在 Ubuntu 22.04 原生系统 repo sync 出 openvela 源码树
#     (分支 dev-ai-contest-2026, manifest openvela.xml)
#   - 当前 shell 位于源码树根目录 (含 .repo / build.sh)
#   - 必须先 `git lfs install` (见下方检查), 否则 LFS 大文件只是指针,
#     编出的固件跑不起来。
#
# 用法:
#   cd <openvela-source-root>
#   ./apply_and_build.sh /path/to/openvela_contest
#
# 产物: 在 cmake_out/ 下 (目录名由 build.sh 决定, 形如
#   cmake_out/vela_esp32s3-eye/), 含 bootloader/partition/nuttx 或 merged bin。
#   把该目录传回 Windows, 用 flash_servo_win.ps1 -Go 烧录到 COM7。
# ============================================================
set -e
CONTEST="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
ROOT="$(pwd)"
echo "== openvela 源码树: $ROOT"
echo "== contest 代码:   $CONTEST"

[ -f "$ROOT/build.sh" ] || { echo "ERROR: 当前目录不是 openvela 源码树根 (缺 build.sh)"; exit 1; }
[ -d "$CONTEST/apps/desktop_pet" ] || { echo "ERROR: 找不到 $CONTEST/apps/desktop_pet"; exit 1; }
[ -d "$CONTEST/apps/servo" ] || { echo "ERROR: 找不到 $CONTEST/apps/servo (独立 servo 命令 app)"; exit 1; }
[ -f "$CONTEST/board/esp32s3-eye/defconfig.addition" ] || { echo "ERROR: 找不到 $CONTEST/board/esp32s3-eye/defconfig.addition"; exit 1; }

# ---- git lfs 检查 (官方要求, 否则 LFS 大文件只是指针) ----
if ! git lfs version >/dev/null 2>&1; then
  echo "ERROR: 本机未安装 git-lfs。请先安装 git-lfs 后重跑:"
  echo "      sudo apt-get install git-lfs   # 或见 https://git-lfs.com"
  echo "      git lfs install"
  exit 1
fi
# 若源码树根本身是 git 仓库, 确认 LFS 已初始化/拉取; 若是 repo 多仓库工作区,
# 仅在根无 git 时给出提醒 (各子项目请用 git lfs pull 取回真实文件)。
if git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  if ! git -C "$ROOT" lfs ls-files >/dev/null 2>&1; then
    echo "[!] 警告: 当前 git 仓库未检测到 LFS 跟踪/已拉取。若源码含 LFS 指针而非真实"
    echo "    文件, 编出的固件可能跑不起来。请先执行: git lfs install && git lfs pull"
    read -r -p "仍要继续? [y/N] " _ans
    case "$_ans" in
      y|Y) ;;
      *) echo "已取消。请先 git lfs install && git lfs pull。"; exit 1 ;;
    esac
  fi
else
  echo "[*] 提示: 源码根为 repo 多仓库工作区 (非单一 git 仓库)。请确保已对 esp32 等子项目"
  echo "    执行过 git lfs install / git lfs pull, 否则大文件只是指针, 固件跑不起来。"
fi

# ---- 复制 desktop_pet 应用 (openvela external app 机制) ----
# openvela 的外部 app 自动包含机制: 放到 apps/external/<app>/, 顶层
# apps/Makefile 会 include external 下的 Make.defs, 从而把 app 编进固件。
# 因此优先目标必须是 $ROOT/apps/external/desktop_pet; 旧式 apps/desktop_pet
# 与 nuttx-apps/examples/desktop_pet 仅作为更次优先回退 (旧版脚本把顺序反了,
# 会导致 external 机制不触发, app 漏编进固件)。
APP_DST=""
for cand in \
  "$ROOT/apps/external/desktop_pet" \
  "$ROOT/apps/desktop_pet" \
  "$ROOT/nuttx-apps/examples/desktop_pet" ; do
  # 选择第一个可写入的目标 (external 优先, 不存在则创建父目录后使用)
  mkdir -p "$(dirname "$cand")" 2>/dev/null || continue
  APP_DST="$cand"
  mkdir -p "$APP_DST"
  break
done
[ -n "$APP_DST" ] || { echo "ERROR: 无法创建 app 目标目录 (磁盘/权限问题)"; exit 1; }
echo "== 复制 desktop_pet 应用 -> $APP_DST"
cp -rv "$CONTEST/apps/desktop_pet/." "$APP_DST/"

# ---- 复制 servo 命令应用 (独立 NSH 命令 `servo`, 让语音 run_shell 能调用) ----
# 与 desktop_pet 同样走 external 机制: 放到 apps/external/servo, 顶层
# apps/Makefile 会 include 其 Make.defs, 把 `servo` 命令编进固件。
SERVO_DST="$ROOT/apps/external/servo"
mkdir -p "$SERVO_DST"
echo "== 复制 servo 命令应用 -> $SERVO_DST"
cp -rv "$CONTEST/apps/servo/." "$SERVO_DST/"

# ---- 复制自定义 Skills (/data/agent/skills/) ----
# openvela 的 data 分区通常在源码树同级 ../data, 或 vendor/openvela/data / vendor/espressif/data
for base in "$ROOT/../data" "$ROOT/vendor/openvela/data" "$ROOT/vendor/espressif/data" "$ROOT/data"; do
  sk="$base/agent/skills"
  if mkdir -p "$sk" 2>/dev/null && cp -rv "$CONTEST/skills/." "$sk/" 2>/dev/null; then
    echo "== 复制 Skills -> $sk"
    break
  fi
done

# ---- 定位 esp32s3-eye 母本 defconfig (最可靠的编译目标) ----
# esp32s3-eye 已迁移到 vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/。
# 可用 configs: gpio/i2c/lcd/nsh/wifi (无单一默认 config)。
# 基 config 优先选 nsh (含 NSH + 基础外设, 适合叠加 ai_agent/LEDC/servo);
# 若迁移后路径命中, 直接用; 否则回退 find (兼容任意 vendor/espressif 位置)。
EYE_BASE="$ROOT/vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/configs/nsh/defconfig"
if [ -f "$EYE_BASE" ]; then
  EYE_DEFCONFIG="$EYE_BASE"
else
  EYE_DEFCONFIG="$(find "$ROOT" -path '*esp32s3-eye*' -name defconfig 2>/dev/null | head -1)"
  if [ -n "$EYE_DEFCONFIG" ]; then
    echo "[!] 未在 vendor/espressif 默认 nsh 路径找到 defconfig, 回退 find: $EYE_DEFCONFIG"
    echo "    (建议确认基 config 为 nsh; 若命中 gpio/i2c/lcd/wifi, 也可叠加, 但需自行验证外设)"
  fi
fi
if [ -z "${EYE_DEFCONFIG:-}" ]; then
  echo "ERROR: 找不到 esp32s3-eye 的 defconfig (请确认已 repo sync 出 vendor/espressif 下的 esp32s3-eye)"
  exit 1
fi
echo "== 找到 esp32s3-eye defconfig: $EYE_DEFCONFIG"

# 追加我们的舵机/语音配置 (GPIO42 关屏版, 与 t-display-s3/defconfig 对齐)
# 先备份, 再追加 (defconfig 中后出现的同名符号覆盖先出现的, 因此 LCD/FACE 会被关掉)
echo "== 追加 servo/voice 配置到 esp32s3-eye defconfig"
cp -v "$EYE_DEFCONFIG" "$EYE_DEFCONFIG.bak"
cat "$CONTEST/board/esp32s3-eye/defconfig.addition" >> "$EYE_DEFCONFIG"

# ---- best-effort: 复制出一个命名板 t-display-s3 (整目录复制 eye + 覆盖) ----
# 注意: EYE_BOARD_DIR 由上面命中/回退得到, 已自动使用迁移后的
#       vendor/espressif/boards/xtensa/esp32s3/ 前缀。
#       命名板需在 menuconfig 里手动注册 BOARD_T_DISPLAY_S3 才会被识别
#       (apply_and_build.sh 不自动注册)。
EYE_BOARD_DIR="$(dirname "$(dirname "$EYE_DEFCONFIG")")"   # .../esp32s3-eye
TDS3_BOARD_DIR="$(dirname "$EYE_BOARD_DIR")/t-display-s3"
if [ -d "$EYE_BOARD_DIR" ] && [ ! -e "$TDS3_BOARD_DIR" ]; then
  echo "== 复制 eye 整板为 t-display-s3 (best-effort 命名板, 需在 menuconfig 注册)"
  cp -r "$EYE_BOARD_DIR" "$TDS3_BOARD_DIR"
  [ -f "$CONTEST/board/t-display-s3/board.c" ] && cp -v "$CONTEST/board/t-display-s3/board.c" "$TDS3_BOARD_DIR/src/board.c" 2>/dev/null || true
  [ -f "$CONTEST/board/t-display-s3/board.h" ] && cp -v "$CONTEST/board/t-display-s3/board.h" "$TDS3_BOARD_DIR/include/board.h" 2>/dev/null || true
  mkdir -p "$TDS3_BOARD_DIR/configs/t-display-s3"
  cp -v "$CONTEST/board/t-display-s3/defconfig" "$TDS3_BOARD_DIR/configs/t-display-s3/defconfig"
  echo "== t-display-s3 命名板已就绪 (需在 menuconfig 里确认 BOARD_T_DISPLAY_S3 已注册)"
fi

# ---- 编译: 优先 esp32s3-eye (可靠), 失败再试 t-display-s3 ----
build_one() {
  local cfg="$1"
  echo "== 编译: ./build.sh $cfg --cmake -j$(nproc)"
  ./build.sh "$cfg" --cmake -j"$(nproc)"
}

# 推导 build.sh 需要的 config 路径 (去掉末尾 defconfig, 转相对路径)
EYE_CFG_REL="${EYE_DEFCONFIG%/defconfig}"
EYE_CFG_REL="${EYE_CFG_REL#"$ROOT"/}"

if build_one "$EYE_CFG_REL"; then
  echo "== 编译成功 (esp32s3-eye nsh + servo GPIO42 关屏版)"
else
  echo "== esp32s3-eye 编译失败, 尝试命名板 t-display-s3"
  TDS3_CFG_REL="${TDS3_BOARD_DIR#"$ROOT"/}/configs/t-display-s3"
  build_one "$TDS3_CFG_REL" || { echo "ERROR: 编译失败, 见上方日志"; exit 1; }
fi

# ---- 产物目录 (实际名称由 build.sh 决定, 不要硬编码) ----
OUT_REL="$(ls -d cmake_out/*/ 2>/dev/null | head -1)"
echo
echo "== 完成。编译产物目录(由 build.sh 决定, 名称可能形如 vela_esp32s3-eye):"
if [ -n "$OUT_REL" ]; then
  echo "      $ROOT/$OUT_REL"
else
  echo "      cmake_out/ 下 (形如 cmake_out/vela_esp32s3-eye/), 把该目录传回 Windows"
fi
echo "== 推荐烧录流程:"
echo "      1) 把上述 cmake_out 目录拷回 Windows"
echo "      2) Windows: pwsh flash_servo_win.ps1 -Go        (烧 COM7)"
echo "         或 Linux: ./flash_servo.sh --go              (自动发现产物并烧录)"
echo "     (flash_servo.sh / flash_servo_win.ps1 会自动在 cmake_out 下查找含"
echo "      nuttx.bin / nuttx.merged.bin 的目录, 无需硬编码路径)"
echo "== 烧录后串口验证: ls /dev (确认 pwm0) -> servo set 90 (舵机应转到 90°)"
