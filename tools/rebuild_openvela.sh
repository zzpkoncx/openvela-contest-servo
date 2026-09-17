#!/usr/bin/env bash
# ===========================================================================
# rebuild_openvela.sh —— 在 Ubuntu 22.04 虚拟机里一键重编舵机/语音固件
# ===========================================================================
# 这是给"不会操作"的用户准备的：把本文件传到 Ubuntu 虚拟机，
#   chmod +x rebuild_openvela.sh
#   ./rebuild_openvela.sh
# 一条命令跑完即可（中间会找源码根、挂入我们的代码、编译、拷 bin）。
#
# 注意：openvela 官方要求原生 Ubuntu 22.04 编译，Windows / WSL / Docker 不行。
#       本脚本必须在 Ubuntu 里运行，不能在 Windows 跑。
# ===========================================================================
set -e

echo "==================================================="
echo "  openvela 舵机/语音固件 一键重编"
echo "==================================================="

# ---- 1. 定位 openvela 源码根（需含 build.sh）----
SRC=""
for c in "$OPENVELA_SRC" "$HOME/openvela" "$HOME/openvela_src" /opt/openvela /workspace/openvela; do
  if [ -f "$c/build.sh" ]; then SRC="$c"; break; fi
done
if [ -z "$SRC" ]; then
  echo "ERROR: 找不到 openvela 源码（需含 build.sh）。"
  echo "  请先按官方 quickstart 用 openvela_bootstrap.sh 拉取源码，"
  echo "  或设置： export OPENVELA_SRC=/你的/源码/路径   后重跑本脚本。"
  exit 1
fi
echo "== openvela 源码根: $SRC"

# ---- 2. 定位 contest 代码（含 apps/desktop_pet）----
CONTEST=""
for c in "$CONTEST_SRC" /mnt/shared/openvela_contest /mnt/hgfs/openvela_contest "$HOME/openvela_contest" /media/*/openvela_contest; do
  if [ -d "$c/apps/desktop_pet" ]; then CONTEST="$c"; break; fi
done
if [ -z "$CONTEST" ]; then
  echo "ERROR: 找不到 openvela_contest 代码（需含 apps/desktop_pet）。"
  echo "  请在 VirtualBox 里把 Windows 的 contest 文件夹共享给 Ubuntu，"
  echo "  或设置： export CONTEST_SRC=/挂载点/openvela_contest   后重跑。"
  exit 1
fi
echo "== contest 代码: $CONTEST"

# ---- 3. 编译（apply_and_build.sh 会挂应用+Skill、追加 defconfig、编 esp32s3-eye nsh）----
cd "$SRC"
bash "$CONTEST/tools/apply_and_build.sh" "$CONTEST"

# ---- 4. 找产物目录 ----
OUTDIR="$(ls -d cmake_out/*/ 2>/dev/null | head -1)"
if [ -z "$OUTDIR" ]; then
  echo "ERROR: 未找到 cmake_out 产物，编译可能失败，请看上方日志。"
  exit 1
fi
echo "== 编译产物目录: $SRC/$OUTDIR"

# ---- 5. 自动拷到 Windows 共享目录（若检测到），否则打印路径 ----
WINSHARE=""
for c in /mnt/shared /mnt/hgfs /mnt/sharedfolders /media/*/shared; do
  if [ -d "$c" ]; then WINSHARE="$c"; break; fi
done
if [ -n "$WINSHARE" ]; then
  DST="$WINSHARE/openvela_contest/out"
  mkdir -p "$DST"
  cp -rv "$OUTDIR"* "$DST/" 2>/dev/null || cp -rv "$OUTDIR" "$DST/"
  echo
  echo "== 已自动把固件拷到 Windows: $DST"
  echo "== 回到 Windows，告诉 WorkBuddy『烧录』，它会用 flash_servo_win.ps1 烧 COM7。"
else
  echo
  echo "== 没检测到 Windows 共享目录，请手动把下面这个目录拷回 Windows："
  echo "      $SRC/$OUTDIR"
  echo "   拷到： C:\\Users\\admin\\Desktop\\lk\\openvela_contest\\out\\"
  echo "   然后告诉 WorkBuddy『烧录』。"
fi
echo "== 完成。"
