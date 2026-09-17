# 编译与烧录指南（openvela 舵机 + 语音固件）

> 适用范围：把 `openvela_contest` 里的 `desktop_pet`（舵机/语音）应用 + Skills 编译进 **openvela** 固件，并烧录到 **ESP32-S3（T-Display-S3）** 真机。
> 编译在 Ubuntu 22.04 原生机完成；烧录可在 Linux 或 Windows 进行。

> ⚠️ 与官方 quickstart 的关系：官方 `dev-ai-contest-2026` quickstart **只编译 goldfish-arm64-v8a-ap 模拟器（ARM64 软仿真）**，整篇没有真机烧录步骤。真机 ESP32-S3 烧录是我们的扩展（详见文末「与官方 quickstart 的差异说明」）。**比赛要求烧录真机，goldfish 模拟器不能替代真机演示。**

---

## 0. 前置条件

- [x] Ubuntu 22.04 原生（非 WSL/Docker，官方明文禁止）
- [x] ≥16GB RAM，≥40GB 硬盘
- [x] **已执行 `git lfs install`**（必须！否则 LFS 大文件只是指针，编出的固件跑不起来）
- [x] Python 3.10+（openvela 工具链）
- [x] 已装 ESP32 烧录工具：`esptool`（或 `esptool.py`）

---

## 1. 拉取 openvela 源码

推荐 GitHub HTTPS（也可用 Gitee 镜像）：

```bash
mkdir -p ~/openvela && cd ~/openvela

# ⚠️ 先初始化 git lfs（否则后面的模型等大文件只是指针）
git lfs install

# 方式一（推荐）: GitHub
repo init -u https://github.com/open-vela/manifests.git \
    -b dev-ai-contest-2026 \
    -m openvela.xml \
    --repo-url=https://mirrors.tuna.tsinghua.edu.cn/git/git-repo/ \
    --git-lfs

# 方式二（备选）: Gitee 镜像
# repo init -u https://gitee.com/open-vela/manifests.git \
#     -b dev-ai-contest-2026 \
#     -m openvela.xml \
#     --repo-url=https://mirrors.tuna.tsinghua.edu.cn/git/git-repo/ \
#     --git-lfs

repo sync -c -j8
```

> 首次拉取 ~30-60 分钟（取决于网速）。`--git-lfs` / `git lfs install` 会下载模型等大文件。

---

## 2. 两个编译目标：模拟器 vs 真机（务必分清）

两者是**不同的 build 目标，命令不同，产物不同**：

| 目标 | 用途 | 是否真机 | openvela 编译命令 |
|---|---|---|---|
| `goldfish-arm64-v8a-ap` | 快速验证逻辑（无需硬件） | 否（ARM64 软仿真） | `./build.sh <goldfish config 路径> --cmake -j$(nproc)` |
| `esp32s3-eye`（config `nsh`） | 烧录到 T-Display-S3 真机 | 是 | `./build.sh vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/configs/nsh --cmake -j$(nproc)` |

> openvela 统一用 `./build.sh <config-path> --cmake ...`，**不是 NuttX 的 `make`**，也**没有** `./tools/configure.sh`。`menuconfig` 写法是 `./build.sh <config-path> --cmake menuconfig`。

### 2.1 模拟器 goldfish（仅验证逻辑，不能替代真机）

```bash
cd ~/openvela
# goldfish 目标 (ARM64 软仿真), 具体 config 路径以官方 quickstart 为准, 形如:
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap/ --cmake -j$(nproc)
# 跑模拟器:
./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap/
# 等提示符 goldfish-armv8a-ap>
```

在模拟器里验证应用/Skills 逻辑即可，**不代表真机外设（PWM/GPIO42 舵机）可用**。

### 2.2 真机 esp32s3-eye（烧 T-Display-S3）

```bash
cd ~/openvela
# esp32s3-eye 母本 + nsh 基 config (官方迁移后的真实路径)
./build.sh vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/configs/nsh --cmake -j$(nproc)
# 如需改配置:
./build.sh vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/configs/nsh --cmake menuconfig
```

要点：
- **esp32s3-eye 板已迁移**（2026-05-26 commit `bb5cd7e` "relocate board to vendor/espressif overlay"）。旧路径 `boards/xtensa/esp32s3/esp32s3-eye` 已失效；新路径 `vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/`。
- 该板可用 configs（在 `configs/` 下）：`gpio` `i2c` `lcd` `nsh` `wifi`（**没有单一默认 config**）。为舵机+ai_agent 起步，**以 `nsh` 为基 config**，再叠加我们的 `board/esp32s3-eye/defconfig.addition`（已开启 ai_agent / LEDC / 关 LCD / 关 FACE / servo GPIO42）。
- T-Display-S3 与 esp32s3-eye 同为 **ESP32-S3R8** 同硅片；我们的配置已关 LCD、只用芯片级 LEDC，把 esp32s3-eye 固件烧到 T-Display-S3 真机即可驱动 GPIO42 舵机，无需新建板级 Kconfig。

> 嫌手动挂载代码麻烦？直接用下面第 3 节的一键脚本，它自动把应用/Skills 挂进源码树并以 `nsh` 基 config 编译。

---

## 3. 推荐一键流程（apply_and_build.sh）

最省事的真机固件产出方式（在 Ubuntu 编译机执行）：

① **拉源码**（含 `git lfs install`，见第 1 节）。

② **挂代码 + 编译**（一条命令）：
```bash
cd ~/openvela                       # 源码树根 (含 .repo / build.sh)
./apply_and_build.sh /path/to/openvela_contest
```
该脚本会：
- 检查 `git lfs`；
- 把 `apps/desktop_pet` 复制到 `apps/external/desktop_pet`（openvela 外部 app 自动 include 机制，确保编进固件）；
- 把 `skills/` 复制到 data 分区的 `agent/skills/`；
- 定位 `vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/configs/nsh/defconfig`，追加 `defconfig.addition`；
- 以 `./build.sh <cfg> --cmake -j$(nproc)` 编译，产物在 `cmake_out/` 下（目录名形如 `vela_esp32s3-eye`）。

③ **拷回 Windows 并烧录**：
```powershell
# 把 Ubuntu 上 cmake_out/ 下含 bin 的目录拷到 Windows (如 C:\Users\admin\Desktop\lk\cmake_out)
pwsh flash_servo_win.ps1 -Go            # 烧 COM7
# 或指定目录/端口:
pwsh flash_servo_win.ps1 -Go -BinDir D:\firmware -Port COM7
```
或在 Linux 上直接：
```bash
cd ~/openvela
OPENVELA_ROOT=~/openvela ./flash_servo.sh --go            # 自动发现产物, 烧检测到的串口
OPENVELA_ROOT=~/openvela ./flash_servo.sh --port /dev/ttyUSB0 --go
```

---

## 4. 烧录真机（ESP32-S3 标准镜像布局）

标准 ESP32-S3 openvela 镜像布局（与 `flash_servo.sh` / `flash_servo_win.ps1` 对齐）：

| 偏移 | 文件 | 说明 |
|---|---|---|
| `0x0` | `bootloader.bin` | 二级 bootloader |
| `0x8000` | `partition-table.bin` | 分区表 |
| `0x10000` | `nuttx.bin` | 应用固件 |
| `0x0`（整体） | `nuttx.merged.bin` | 若产物含合并镜像，则整体烧 `0x0`（比分散烧更安全） |

> ⚠️ **没有 `0x20000 ai_agent_model.bin` 这一项**（除非你在本次 repo 里确认确有该产物；本项目舵机/语音走 ai_agent 在线/内置，不单独烧模型 bin）。

### 4.1 Linux 一键烧录（flash_servo.sh）

```bash
cd ~/openvela
OPENVELA_ROOT=~/openvela ./flash_servo.sh                 # 仅显示烧录命令(不真烧)
OPENVELA_ROOT=~/openvela ./flash_servo.sh --go            # 真正烧录
OPENVELA_ROOT=~/openvela ./flash_servo.sh --port /dev/ttyUSB0 --go
```
脚本会自动在 `cmake_out/` 下查找含 `nuttx.bin` / `nuttx.merged.bin` 的目录，无需硬编码路径。

### 4.2 Windows 一键烧录（flash_servo_win.ps1，推荐）

```powershell
pwsh flash_servo_win.ps1                 # 预览(不烧)
pwsh flash_servo_win.ps1 -Go             # 烧 COM7
pwsh flash_servo_win.ps1 -Go -Backup     # 先备份出厂固件(读 16MB)
pwsh flash_servo_win.ps1 -Go -BinDir D:\firmware -Port COM7
```

### 4.3 手工 esptool（可选参考）

```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash \
  0x0     <dir>/bootloader.bin \
  0x8000  <dir>/partition-table.bin \
  0x10000 <dir>/nuttx.bin
# 若产物含 nuttx.merged.bin, 整体烧 0x0:
# esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write_flash 0x0 <dir>/nuttx.merged.bin
```

烧录后：按一下板子 RST/EN 复位，串口监控（115200）查看开机日志。

---

## 5. Skills / 应用如何进固件

- `desktop_pet` 应用：由 `apply_and_build.sh` 复制到 `apps/external/desktop_pet`，顶层 `apps/Makefile` 会 `include` external 下的 `Make.defs`，**编译时自动编进固件**。无需单独拷贝到板子。
- `skills/`：由 `apply_and_build.sh` 复制到 data 分区的 `agent/skills/`，随镜像烧入。
- 若想事后增量更新某个 Skill，可在板子启动后通过串口/adb 放到 `/data/agent/skills/` 下（需文件系统可写），但常规流程不需要。

---

## 6. 故障排查

| 现象 | 原因 | 排查 |
|---|---|---|
| 编译报 `undefined reference to vela_xxx` | ai_agent 没启用 | `./build.sh <cfg> --cmake menuconfig` → System Tools → AI Agent → [*] |
| 编译报 `esp32s3_ledc_init not found` | LEDC 驱动没开 | Drivers → PWM → LEDC |
| 启动后 `/dev/lcd0` 不存在 | LCD 驱动没编进去（本配置本就关屏） | 属正常；舵机用 GPIO42 必须关屏 |
| 烧录后串口没输出 | 烧错地址 / Flash 模式 | 重新烧 0x0, 0x8000, 0x10000（或整体烧 nuttx.merged.bin 到 0x0） |
| 板子一直重启 | PSRAM 没配 | defconfig 里 `CONFIG_ESP32S3_PSRAM_MODE_OCT=y` |
| 麦克风没声音 | I2S 引脚或 WS 反相 | 检查 `BOARD_MIC_*_GPIO` 和 `WS_INV` |
| 编译报 app 没被编进固件 | 应用没走 external 机制 | 确认 `apps/external/desktop_pet` 存在且被 include（用 apply_and_build.sh） |
| `git lfs` 相关大文件是指针 | 没 `git lfs install` / `git lfs pull` | 先执行后再 repo sync 或 `git lfs pull` |

---

## 7. 时间线（参考）

| 步骤 | 预计时间 | 备注 |
|---|---|---|
| repo sync 源码（含 git lfs） | 30-60 min | 第一次 |
| 模拟器编译 | 5-10 min | 验证逻辑（非真机） |
| esp32s3-eye 母本编译 | 10-20 min | 真机固件 |
| 真机烧录 + 跑通 | 2-4 h | 调试硬件 |
| 总 | 1-2 d | 留余量给视频/PR |

---

## 8. 让舵机语音动起来（验证清单）

> 舵机默认接 **GPIO42**（270°，500~2500us，50Hz）。⚠️ GPIO42=LCD D3 硬布线，故本配置已关屏让 42 空出给舵机；若保留屏幕改 GPIO21（见 SERVO_VOICE.md 方案 B）；GPIO24 模组未引出不可用。
> 语音链路：mic → ai_agent 语音通道(火山 ASR) → LLM → 命中 servo-control Skill
> → ai_agent 用 run_shell 执行 `servo` 命令 → 写 /dev/pwm0 → 舵机转。

### 8.1 先不靠语音，串口直接验证舵机（最重要的一步）

烧录后串口进 NSH，依次敲：

```
nsh> ls /dev
... 确认有 pwm0 ...          # 没有 pwm0 → board_late_initialize 没生效，见故障排查
nsh> servo status            # 应显示 angle=135 sweeping=0
nsh> servo set 90            # 舵机应转到 ~90°
nsh> servo sweep slow        # 舵机应缓慢来回摆头
nsh> servo stop              # 停止
nsh> servo center            # 回中 135°
```

只要 `servo set 90` 能让舵机物理转动，说明 **PWM/引脚/舵机/供电全部 OK**，
剩下只是把语音接到这个命令上。这一步不通，先别折腾语音。

### 8.2 确认 Skill 已部署

```
nsh> ls /data/agent/skills/
servo-control/   face-expression/   weather/   daily-briefing/
```

缺 `servo-control` 就补（一键脚本已把它拷进镜像；若手动部署）：

```
nsh> mkdir -p /data/agent/skills
# 通过串口/adb 把 skills/servo-control/ 放进 /data/agent/skills/
```

### 8.3 起语音（ai_agent 通道）

```
nsh> vela
vela> set_llm kimi <你的LLM_API_KEY>          # 任一云端后端
vela> set_volc_key <火山引擎Key>              # ASR/TTS 后端
vela> set_volc_asr                            # 启用火山 ASR
vela> voice_start                             # 开语音通道（mic → ASR → LLM → TTS）
```

然后对着板子说：**「你好，openvela，舵机转到 90 度」**
→ 舵机应转到 90°；说「摇个头」→ 摆头；「舵机停」→ 停。

### 8.4 故障排查（ servo 专项）

| 现象 | 原因 | 处理 |
|---|---|---|
| `ls /dev` 没有 `pwm0` | `board_late_initialize` 未调用 | 确认 defconfig `CONFIG_BOARD_LATE_INITIALIZE=y` 且 board.c 已实现该函数 |
| `servo set 90` 报 rc=-19 | 没开 CONFIG_PWM / LEDC | menuconfig → Drivers → PWM(LEDC)，`CONFIG_PWM=y` `CONFIG_ESP32S3_LEDC=y` |
| 舵机抖但不转 / 只响不动 | 供电不足（从 3.3V 取电） | 舵机必须 **5V 独立供电**；信号线接 GPIO42 |
| 舵机转向反 / 角度不对 | 脉宽范围不符 | 检查 `CONFIG_APP_DESKTOP_PET_SERVO_MIN_US/MAX_US`（500/2500） |
| 语音唤醒但舵机不动 | LLM 没调 run_shell | 看 ai_agent 日志是否执行了 `servo`；Shell 需 Full 模式（`CONFIG_EXAMPLES_AI_AGENT_VELA_SHELL_FULL=y`） |
| 语音完全不唤醒 | 火山 Key/ASR 未设 | 确认 `set_volc_key` + `set_volc_asr` + `voice_start` 都成功 |

---

## 9. 与官方 quickstart 的差异说明

- **官方只给 goldfish 模拟器**：`dev-ai-contest-2026` quickstart 仅编译 `goldfish-arm64-v8a-ap` 软仿真，整篇无真机烧录步骤。真机 ESP32-S3（esp32s3-eye 母本烧 T-Display-S3）是本项目扩展。
- **esp32s3-eye 已迁移**：2026-05-26 commit `bb5cd7e` "relocate board to vendor/espressif overlay"，路径从 `boards/xtensa/esp32s3/esp32s3-eye` 变为 `vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/`。
- **基 config 选 `nsh`**：该板可用 `gpio/i2c/lcd/nsh/wifi`，无单一默认；以 `nsh` 为基，叠加 `board/esp32s3-eye/defconfig.addition`（开 ai_agent/LEDC、关 LCD/FACE、servo GPIO42）。
- **编译命令统一**：openvela 用 `./build.sh <config-path> --cmake [menuconfig]`，不是 NuttX 的 `make` / `./tools/configure.sh`。
- **烧录布局**：标准 ESP32-S3 openvela 偏移 `0x0 / 0x8000 / 0x10000`，或 `nuttx.merged.bin` 整体 `0x0`；无 `ai_agent_model.bin` 单独项。
