# openvela 合规审计报告

> 审计对象：`C:/Users/admin/Desktop/lk/openvela_contest/`（2026 openvela AI 硬件开发比赛提交包）
> 审计角色：软件工程师（独立复核）
> 审计依据：比赛硬约束——**必须基于 openvela 系统能力（NuttX 内核仓库除外），且至少落地图形 / AI / 多媒体之一**。

---

## 一、一句话结论

✅ **全程基于 openvela（NuttX）系统能力，无任一处非 openvela 写法，合规通过（IS_PASS: YES）。**
舵机控制走 openvela 的 **PWM 字符设备 + NSH 内建命令 + 板级 `board_late_initialize` 初始化 + ai_agent 消息总线（run_shell）** 四条系统能力链路，LCD 表情走 openvela **LCD 字符设备 `/dev/lcd0`**，全部为 openvela/NuttX 标准 API，未引入 ESP-IDF / Arduino / MicroPython / 裸寄存器直写。

---

## 二、文件级合规矩阵

归类说明：
- ✅ = openvela / NuttX 系统能力（PWM 字符设备、NSH、板级初始化、ai_agent 总线、LCD 设备、syslog 等）
- ⚠️ = 用户态-only 但合法（标准 C 库、POSIX pthread/fork/signal、文件 IO）
- ❌ = 非 openvela（ESP-IDF driver 层、Arduino、裸寄存器直写、MicroPython）—— 本工程 **0 项**

| 文件 | 关键 API | 归类 |
|------|----------|------|
| `apps/desktop_pet/Make.defs` | `CONFIGURED_APPS += $(APPDIR)/desktop_pet`（openvela apps 标准注册） | ✅ |
| `apps/desktop_pet/Makefile` | `PROGNAME`/`CSRCS`/`PRIORITY`/`CFLAGS`/`LDADD`（openvela/NuttX 应用 Makefile 标准格式） | ✅ |
| `apps/desktop_pet/Kconfig` | `config APP_DESKTOP_PET`、`depends on AI_AGENT`（openvela Kconfig 标准） | ✅ |
| `apps/desktop_pet/desktop_pet.h` | 仅枚举 + 函数声明（`<stdint.h>`） | ✅ |
| `apps/desktop_pet/desktop_pet_main.c` | `<nuttx/config.h>`、`<sys/boardctl.h>`、`syslog`、POSIX `fork`/`setsid`/`signal`/`pause`、`pthread`、`getopt`、文件 IO（`fopen`/`fgets`/`getpid`） | ✅ + ⚠️ |
| `apps/desktop_pet/face_lcd.c` | `open("/dev/lcd0")`、字符设备 `write()`、文件系统 `fopen`/`fread`、`<nuttx/config.h>`、`syslog`、`malloc`/`free` | ✅ + ⚠️ |
| `apps/desktop_pet/servo_drv.c` | `<nuttx/timers/pwm.h>`、`open("/dev/pwm0")`、`ioctl(PWMIOC_SETCHARACTERISTICS / PWMIOC_START / PWMIOC_STOP)`、`pwm_info_s`、`pthread`、`syslog` | ✅ + ⚠️ |
| `apps/desktop_pet/servo_cmd_main.c` | `<nsh.h>` + `NSH_ADDCMD(servo, ...)`（openvela NSH 内建命令注册）、标准 C 解析 | ✅ + ⚠️ |
| `apps/desktop_pet/voice_bridge.c` | `<nuttx/config.h>`、`syslog`、ai_agent `run_shell`/`Markdown Skill` 集成说明（无 socket，无裸驱动） | ✅ |
| `board/t-display-s3/board.h` | `<nuttx/config.h>`、`<stdint.h>`、纯引脚宏定义 | ✅ |
| `board/t-display-s3/board.c` | `<nuttx/board.h>`、`<arch/board/board.h>`、`<esp32s3_gpio.h>`、`<esp32s3_ledc.h>`、`<esp32s3_i2s.h>`、`<nuttx/timers/pwm.h>`；`board_late_initialize()`、`esp32s3_gpio_config/write`、`esp32s3_ledc_init()`、`esp32s3_i2s_init()`、`pwm_register("/dev/pwm0", lower)` | ✅ |
| `board/t-display-s3/defconfig` | openvela 标准 defconfig（`CONFIG_BOARD_LATE_INITIALIZE`、`CONFIG_PWM`、`CONFIG_ESP32S3_LEDC`、`CONFIG_AI_AGENT` 等；`CONFIG_NSH` 实际由 `board/esp32s3-eye/defconfig.addition:105` 提供） | ✅ |
| `board/esp32s3-eye/defconfig.addition` | openvela 标准 defconfig 追加片段（与母本对齐） | ✅ |

> **defconfig 合并说明**：提交时需将 `board/t-display-s3/defconfig` 与 `board/esp32s3-eye/defconfig.addition` 合并应用方为完整配置——`CONFIG_NSH=y` 实际由 `defconfig.addition:105` 提供（见矩阵中 `defconfig` 行备注）。功能正确，此处仅注明来源文件，不影响合规结论。

### 全局反模式扫描结果（针对真实 `.c`/`.h` 源码）

对整个 `openvela_contest/` 下所有 `.c`/`.h` 执行正则扫描，模式包含：
`esp_idf` · `ESP_ERROR_CHECK` · `ledc_channel_config` · `ledc_timer_config` · `gpio_config(` · `i2s_driver_install` · `i2s_config_t` · `driver/` · `arduino` · `machine.` · `#include <esp_` · `freertos`

**真实违规 0 处；** 扫描子串存在少量"伪命中"，但均非提交代码中的非 openvela 写法，具体为两类：
- **openvela BSP 函数名被子串误匹配**：如 `esp32s3_gpio_config(...)` 内含子串 `gpio_config(`，被 `gpio_config(` 模式命中；`esp32s3_i2s_init` 等同属 openvela ESP32-S3 板级头文件里的合法函数。
- **注释 / 文档 / 日志 / 工具脚本引用**：如 `arduino-esp32` 引脚来源说明（board.h 注释）、`machine.PWM` 历史开发日志（logs/AI_DEV_LOG.md）、构建脚本中的 `build machine.` 等。

即：无 ESP-IDF driver 层调用、无裸 Arduino API、无 MicroPython 运行时、无裸寄存器直写、无裸 FreeRTOS 调用。早期反向 socket、裸 ESP-IDF 写法已在重写阶段被剔除，相关字样仅残留在 `docs/`、`logs/AI_DEV_LOG.md` 的注释/开发日志中，非提交代码。

---

## 三、舵机用到的 openvela 系统能力（逐条对应）

1. **PWM 字符设备（核心驱动能力）**
   - `board/t-display-s3/board.c` 的 `t_display_s3_servo_init()` 调用 openvela ESP32-S3 板级驱动 `esp32s3_ledc_init(timer)` 取得 `struct pwm_lowerhalf_s *`，再用 NuttX 标准接口 `pwm_register("/dev/pwm0", lower)` 把 LEDC 通道 0 注册为 PWM 字符设备 `/dev/pwm0`。
   - `apps/desktop_pet/servo_drv.c` 全程只通过 `open("/dev/pwm0")` + `ioctl(PWMIOC_SETCHARACTERISTICS / PWMIOC_START / PWMIOC_STOP)` + `pwm_info_s` 驱动舵机，**绝不直接写 `/sys/class/ledc/...`（openvela 无该 sysfs 节点）**，也绝不调 ESP-IDF 的 `ledc_channel_config`。

2. **板级初始化（NuttX 启动钩子）**
   - `board_late_initialize()`（在 `CONFIG_BOARD_LATE_INITIALIZE=y` 下由内核 `up_initialize` 阶段回调）中链式调用 `t_display_s3_init_peripherals()` → `t_display_s3_servo_init()` / `_lcd_init()` / `_mic_init()`。这是 openvela/NuttX 标准的板级外设挂载点，保证 `pwm_register` 在应用 `open()` 之前完成。

3. **NSH 内建命令（openvela 标准命令注册）**
   - `servo_cmd_main.c` 用 `NSH_ADDCMD(servo, servo_main, 0, ...)`（来自 `<nsh.h>`）把舵机动作注册为 NSH 内建命令 `servo`。链接期生效，无需运行时显式注册。串口可直接 `servo set 90` 验证，也是语音链路的执行落点。

4. **ai_agent 消息总线（AI 能力落地）**
   - 语音链路：`mic → ai_agent 语音通道(火山 ASR) → LLM → 命中 servo-control Skill → ai_agent 经 run_shell 执行 NSH 命令 \`servo set 90\` → servo_cmd() → /dev/pwm0 → 舵机转`。`voice_bridge.c` 已修掉早期"自建反向 unix socket"的错误架构，仅保留为集成说明 + 主动场景入口，符合 openvela ai_agent 的"统一消息总线 + Markdown Skill + 工具调用"范式。
   - 对应 Kconfig：`CONFIG_AI_AGENT=y`、`CONFIG_AI_AGENT_AUDIO/HTTP/WEBSOCKET/CRON/LLM_VOLC/TTS_VOLC/ASR_VOLC`，以及放通自定义命令的 `CONFIG_EXAMPLES_AI_AGENT_VELA_SHELL_FULL=y`。

> **AI 与多媒体已落地**（ai_agent 消息总线 + INMP441 I2S 麦克风）；图形表情代码（`face_lcd.c`）已就绪且合规，但**当前构建因舵机占 GPIO42 与 LCD 数据线 D3 硬件冲突，`defconfig` 设为 `CONFIG_LCD=n` / `CONFIG_APP_DESKTOP_PET_FACE=n`，故图形表情在本固件未启用**。硬约束由 AI + 多媒体满足。

---

## 四、明确排除项（务必写入提交说明）

⚠️ **工作区 `lcd/`（早期 MicroPython 版，191 个 `.py`）不属于本提交，禁止进入 contest PR。**
- 该 MicroPython 版已被 triage 判定为**不合格**，当前 `openvela_contest/` 是重写后的 C 版（基于 openvela 系统能力）。
- 提交包的 `OPENVELA_COMPLIANCE` 仅覆盖 `openvela_contest/` 内的 C 源码、board 配置与 Kconfig；`lcd/`、`ai_vox3_wake/`、`openvela_aivox3/` 等其它目录不在审计与提交范围。
- 建议在 PR 文案/README 中明确标注：`lcd/` 为历史废弃版本，请勿打包提交。

---

## 五、就地修正记录

本次独立复核**未发现任何 ❌ 项**，故无需修改任何源码。

- 修正文件清单：**无**
- 每处改动说明：**无**

（如需把 `desktop_pet_main.c` 中未实际调用的 `#include <sys/boardctl.h>` 或遗留 `TODO` 注释清理，属代码整洁度优化，不影响合规性，未做改动以遵循"最小变更"原则。）

---

## 六、一致性审查（IS_PASS）

- 跨文件接口一致性：`desktop_pet.h` 声明的 `servo_init/deinit/cmd/get_angle/stop/greet`、`face_init/deinit/set/get_current/...`、`voice_bridge_init/deinit` 均被对应 `.c` 实现，且被 `desktop_pet_main.c` 正确调用。✅
- Kconfig ↔ 源码符号一致：`CONFIG_APP_DESKTOP_PET_*`、`CONFIG_PWM`、`CONFIG_ESP32S3_LEDC`、`CONFIG_AI_AGENT` 等均在 `defconfig`/`defconfig.addition` 中打开并与源码 `#ifdef` 匹配。✅
- board ↔ app 设备契约一致：`board.c` 注册 `/dev/pwm0`，`servo_drv.c` 打开 `/dev/pwm0`；`defconfig` 中 `CONFIG_ESP32S3_LEDC_CHANNEL0_PIN=42` 与 `board.h` 的 `BOARD_SERVO_GPIO=42` 一致。✅
- 反模式扫描：0 处非 openvela 写法。✅

### IS_PASS: **YES**

> 全套舵机/桌面宠物代码均基于 openvela（NuttX）系统能力实现，无 ESP-IDF / Arduino / MicroPython / 裸寄存器直写，合规通过，可直接进入比赛提交包。
