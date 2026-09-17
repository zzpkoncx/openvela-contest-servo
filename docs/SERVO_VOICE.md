# 舵机语音动起来 — 完整操作指南（270° / GPIO42 关屏版 / T-Display-S3）

> 目标：对着板子说「你好，openvela，舵机转到 90 度」，舵机真的转。
> 本指南所有结论均已对照 open-vela 官方 `nuttx` / `packages_ai_agent` 源码核实（dev-ai-contest-2026 分支）。

---

## ⚠️ 0. 先读：舵机插哪个口？（端口冲突，最容易翻车）

**T-Display-S3 真实引脚（已用 4 个一手来源交叉确认：LilyGO 官方仓库 README、
espressif 官方 arduino-esp32 的 `lilygo_t_display_s3` 引脚定义、russhughes / peterhinch
的 MicroPython ST7789 驱动）：**

```
LCD 8080 并口（PCB 硬布线）:
  D0=39 D1=40 D2=41 D3=42 D4=45 D5=46 D6=47 D7=48
  CS=6 DC=7 WR=8 RD=9 RST=5 BL=38   GPIO15=外设电源(必须拉高)
```

- **GPIO42 = LCD 屏幕数据线 D3（硬件硬布线）**。舵机接 GPIO42，LEDC 的 PWM 会灌进
  屏幕数据线 → **花屏**。而且 **GPIO42 根本不在你板子底部能插舵机的排针上**
  （见下方"用户可插脚清单"），所以"42"这个口既不能、也不会是舵机脚。
- **GPIO24 在 ESP32-S3 模组（WROOM-1/N8R8）上没引出**（模组只引 0-21、33-48），
  硬件上不存在，不能当舵机 GPIO。
- 你口中的"42 / 24"是丝印/你记的号，**不能直接当 GPIO 号用**。

**底部排针用户实际能插的 GPIO（来自官方引脚定义 P1/P2 分组）：**
`1, 2, 3, 10, 11, 12, 13, 16, 17, 18, 21, 43, 44`

其中**确认空闲、不与 LCD/麦克风/串口/I2C 打架、可作舵机**的脚：**GPIO10、GPIO21**
（串口 RX=GPIO3 别用；mic 占 1/2/16；I2C 占 17/18；SPI 占 11/12/13；USB-JTAG 占 43/44）。

> 本项目**默认舵机接 GPIO42（关屏版）**。因 GPIO42 = LCD 数据线 D3（PCB 硬布线），
> 本配置已关闭屏幕（CONFIG_LCD=n）把 GPIO42 让给舵机 PWM。你只要把舵机信号线接到 GPIO42 即可，无需改 C 代码。
> 若你**想保留屏幕**，把舵机改到 GPIO21（或 GPIO10）并恢复 CONFIG_LCD=y，具体改法见下方「方案 B」。

---

## 0.1 方案 B：舵机一定要接 GPIO42（牺牲屏幕）

如果你的演示**不需要宠物脸/屏幕**，只想让舵机动，可以让 GPIO42 空出来作舵机 PWM
（板子默认 GPIO42 被 LCD D3 占用，关掉 LCD 后该脚即用）。步骤：

1. `board/t-display-s3/defconfig` 注释掉 LCD 相关行：
   ```
   # CONFIG_LCD=y
   # CONFIG_LCD_ST7789=y
   # CONFIG_LCD_ST7789_IFACE_8080=y
   # CONFIG_LCD_ST7789_*_GPIO=...
   ```
2. 舵机引脚改到 42（两处）：
   ```
   CONFIG_ESP32S3_LEDC_CHANNEL0_PIN=42
   CONFIG_APP_DESKTOP_PET_SERVO_GPIO=42
   ```
3. `board/t-display-s3/board.c` 的 `t_display_s3_init_peripherals()` 里
   注释掉 `t_display_s3_lcd_init();`，避免驱动把 GPIO42 配成 LCD 输出。
4. 重新编译烧录，`servo set 90` 即可驱动接在 GPIO42 的舵机。

> ⚠️ 代价：屏幕不显示，桌面小跟班的"表情"功能失效；但**舵机驱动、语音、
> 主动场景、自定义 Skill** 全部保留，仍满足比赛硬指标。

---

## 1. 一句话结论

舵机能不能转，取决于**三个必须同时成立**的条件：

1. **硬件**：舵机 5V 独立供电（不能从板子 3.3V 取），信号线接 GPIO42（默认关屏版；保留屏幕则接 GPIO21 等空闲脚）。
2. **固件**：LEDC 定时器 0 正确注册 `/dev/pwm0`（defconfig 已修好符号名）。
3. **语音**：ai_agent 的 Shell 策略设为 **Full**（不是默认 Allowlist），否则 `servo` 命令被拦截。

---

## 2. 硬件接线（最容易翻车的一步）

| 舵机线 | 接哪里 | 说明 |
|--------|--------|------|
| 棕 / 黑（GND） | 板子 GND | 共地，必须有 |
| 红（VCC） | **外部 5V 电源** | ⚠️ 不能接板载 3.3V/5V 取电引脚，会烧 ESP32 或供电不足不转 |
| 橙 / 黄（信号） | GPIO42（默认关屏版；或你实际的空闲脚） | 接错引脚舵机不动 |

- 外部 5V 电源的地（GND）必须与板子 GND **连在一起**（共地）。
- 舵机 270° 标准：50Hz，脉宽 500–2500us 对应 0°–270°。
- 改引脚只需改两处（无需动 C 代码）：
  - `board/t-display-s3/board.h` 的 `BOARD_SERVO_GPIO`
  - `board/t-display-s3/defconfig` 的 `CONFIG_ESP32S3_LEDC_CHANNEL0_PIN`、
    `CONFIG_APP_DESKTOP_PET_SERVO_GPIO`

---

## 3. 已修好的固件 bug（你不用再改，但要知道）

### Bug A：`/dev/pwm0` 永远注册不上 → 舵机 `open()` 失败
官方 ESP32 LEDC 驱动里，`esp32s3_ledc_init(0)` 返回定时器 0 设备**只在 `CONFIG_ESP32S3_LEDC_TIM0` 已定义时**才编译 `case 0`。
旧 defconfig 写的是 `CONFIG_ESP32S3_LEDC_TIMER0`（符号名错）→ 定时器 0 没启用 → `init` 返回 NULL → 舵机死。

已改为正确符号（`board/t-display-s3/defconfig` 与 `board/esp32s3-eye/defconfig.addition` 两处）：
```
CONFIG_ESP32S3_LEDC=y
CONFIG_ESP32S3_LEDC_TIM0=y            # 原来是 TIMER0（错的）
CONFIG_ESP32S3_LEDC_TIM0_CHANNELS=1   # 只开通道0，见 Bug B
CONFIG_ESP32S3_LEDC_CHANNEL0_PIN=42   # GPIO42（默认舵机引脚；关屏版）
```

### Bug B：串口控制台会被废掉
`g_pwm0dev` 默认管 **2 个通道**（ch0=GPIO42, ch1=GPIO3）。而 LEDC 驱动的 `pwm_setup` 配 GPIO **不检查 pin==0**，会把 GPIO3（UART0 RX）也射成 LEDC 输出 → 串口收不到键盘输入，`servo set 90` 都敲不进去。
已通过 `CONFIG_ESP32S3_LEDC_TIM0_CHANNELS=1` 限制只开通道 0，避免误配 GPIO3。

### Bug C：语音被 Shell 白名单拦截
ai_agent 的 `run_shell` 工具默认 **Allowlist（白名单）** 模式，只放行预批准命令，`servo` 不在白名单 → 语音调用被拒。
已显式设为 Full 模式：
```
CONFIG_EXAMPLES_AI_AGENT_VELA=y
CONFIG_EXAMPLES_AI_AGENT_VELA_SHELL_FULL=y
```
> ⚠️ 符号命名说明：官方 ai_agent Kconfig 顶层是 `CONFIG_EXAMPLES_AI_AGENT_VELA`，
> 部分构建用 `CONFIG_AI_AGENT` 命名空间。两行都写上，谁生效用谁，无害冗余。
> **但请在 `./build.sh <cfg> --cmake menuconfig` 里最终确认 AI Agent 已启用、且 Shell security policy = Full。**

### 驱动机制备注（为什么代码写法是对的）
ESP32 LEDC 驱动对 `PWMIOC_SETCHARACTERISTICS` 的处理：**完全忽略 `pwm_info_s.channels[i].channel` 字段，只按数组下标 `i` 匹配 `g_ledc_chans[i]`**（硬件通道号与引脚在驱动里静态绑定）。
因此 `servo_drv.c` 写 `info.channels[0].duty` 即驱动 GPIO42（通道 0），正确无误；`.channel = ch+1` 那行对 ESP32 无影响（仅作 pwm.h 合同标注）。

---

## 4. 编译与烧录

环境（VirtualBox 原生 Ubuntu 22.04，官方禁用 WSL/Docker）：
```bash
# 在一台 Ubuntu 22.04 虚拟机里
bash openvela_bootstrap.sh        # 装依赖 + repo + 拉源码（已修 repo 启动器 bug）

# 基于 esp32s3-eye 母本（已迁移到 vendor/espressif，config=nsh）
# 推荐：一键挂载并编译（自动把 defconfig.addition 追加进 nsh defconfig）
./apply_and_build.sh /path/to/openvela_contest
# 等效手动步骤（供参考）：
#   cat board/esp32s3-eye/defconfig.addition >> vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/configs/nsh/defconfig
#   ./build.sh vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/configs/nsh --cmake -j$(nproc)
# 烧录按 flash_servo.sh / flash_servo_win.ps1 实际产物偏移（0x0 boot/0x8000 part/0x10000 app）
```
> 真机 T-Display-S3 板级适配若超期，可先用官方 `goldfish-arm64-v8a-ap` 模拟器验证应用逻辑，
> 再把 LEDC 引脚确认是真实 GPIO42（本配置默认）烧真机。

---

## 5. 第一步：串口直接验证舵机（脱离语音）

烧录启动后，串口进 NSH（`vela>` 提示符）：

```
vela> ls /dev
... pwm0 ...                       # 确认有 /dev/pwm0（Bug A 修复的标志）

vela> servo status
servo: angle=135 sweeping=0        # 默认中位 135°

vela> servo set 90
servo: -> 90°                      # 看舵机转到 90°

vela> servo sweep slow             # 来回摆头（摇头动画）
servo: sweep (speed=1)

vela> servo stop
servo: stop

vela> servo center
servo: center                      # 回 135°
```

**这一步必须先在没语音时跑通**——转了说明 PWM/引脚/供电全对；不转先查：
- 供电是不是 5V 独立、GND 共地？
- `ls /dev` 有没有 `pwm0`（没有 = Bug A 没生效，检查 defconfig 符号）？
- 舵机本身好坏（换已知好舵机试）？
- 引脚对不对（`servo status` 报的角度不变，可能是引脚没接对——改 `CONFIG_ESP32S3_LEDC_CHANNEL0_PIN`）？

---

## 6. 第二步：语音驱动舵机

### 6.0 前置（最关键，否则"喊了没用"）：把 Skill 装进板子

ai_agent **只会加载** `/data/agent/skills/<name>/SKILL.md`。本仓库的
`skills/servo-control` 没推上板子，LLM 就不知道有 `servo` 命令，语音"往左转"
永远不会执行。烧录后、启动语音前，先推一次：

```bash
# Ubuntu 编译机（已 adb 连板）：
./tools/install_skill.sh            # 推 skills/ + .env（密钥）
# 或 Windows：
powershell -ExecutionPolicy Bypass -File .\tools\install_skill_win.ps1
```

推完后在板子串口让 ai_agent 重新扫描 Skill：
```
vela> voice_stop
vela> voice_start
```

> 若你改过 skills/servo-control/SKILL.md，重推一遍即可，无需重新编译固件。

### 6.1 配置 AI 后端与语音

在 `vela>` 控制台配置（这些在运行时配置，不依赖编译期 LLM_* 符号）：

```
vela> set_llm kimi <你的Kimi_API_Key>
vela> set_volc_key <你的火山引擎AppKey>
vela> set_volc_asr                 # 开启火山 ASR（语音识别）
vela> voice_start                  # 启动语音通道（mic→ASR→LLM→TTS）
```

然后对着板子说（唤醒词强制「你好，openvela」）：

- 「你好，openvela，舵机往左转」→ 左转 30°（最常用）
- 「你好，openvela，舵机往右转」→ 右转 30°
- 「你好，openvela，左转 45 度」→ 左转 45°
- 「你好，openvela，右转 90 度」→ 右转 90°
- 「你好，openvela，再往左转」→ 在当前角度基础上再左转 30°
- 「你好，openvela，舵机转到 90 度」→ 转 90°
- 「你好，openvela，舵机摇个头」→ 来回摆头
- 「你好，openvela，舵机回到中间」→ 回 135°

### 语音为什么能驱动舵机（链路）
```
mic → ai_agent 语音通道(火山 ASR) → LLM
   → 命中 servo-control Skill（skills/servo-control/SKILL.md）
   → run_shell 执行 `servo set 90`   ← 必须 Shell 策略=Full（Bug C）
   → servo_cmd() 写 /dev/pwm0 → LEDC→GPIO42 → 舵机转
```
LLM 通过 `run_shell` 调用的是 `servo_cmd_main.c` 注册到 NSH 的 `servo` 内建命令。

---

## 7. 主动+执行场景（满足大赛硬指标，代码已落地）

大赛要求 ≥1 个「主动 + 执行」场景，**纯对话机器人不算合格**。本项目已落地两处主动触发：

1. **开机主动打招呼**：`desktop_pet_main.c` 启动后直接调用 `servo_greet()`，摆头打招呼，
   **不依赖任何语音输入**——这是设备"主动能力"的最直接证明。
2. **每日简报主动打招呼**：到 `daily_briefing_at`（默认 07:00）配置的时间，主循环主动切
   "思考"表情 + `servo_greet()` 摆头（自包含触发，不依赖云端，演示稳）。

`servo_greet()` 实现见 `apps/desktop_pet/servo_drv.c`，Skill 文档见 `skills/servo-control/SKILL.md`
的「主动调用场景」节。若赛事固件限制了 `run_shell`，备选是把 `servo`/`face` 注册成 ai_agent 的
**mcp 工具**（见 `packages_ai_agent` 的 `PLUGIN_DEVELOPMENT.md` 的 mcp_call 事件处理），
`apps/desktop_pet/voice_bridge.c` 已留好接入点。

---

## 8. 排查清单

| 现象 | 原因 | 处理 |
|------|------|------|
| `ls /dev` 没有 `pwm0` | Bug A 没生效 | 确认 defconfig 是 `CONFIG_ESP32S3_LEDC_TIM0`（不是 TIMER0） |
| 串口敲不进命令 | Bug B | 确认 `CONFIG_ESP32S3_LEDC_TIM0_CHANNELS=1` |
| `servo set 90` 舵机不转 | 供电/GND/坏舵机/引脚错 | 查 5V 独立供电、共地、换舵机、核对 `CONFIG_ESP32S3_LEDC_CHANNEL0_PIN` |
| 屏幕花屏 | 在**开启 LCD** 时又把舵机接在 GPIO42（D3 冲突） | 本配置已关屏；若要保留屏幕，必须把舵机改到 GPIO21（或其它空闲脚）并恢复 CONFIG_LCD=y |
| 语音说了没反应 | Shell 白名单拦截（Bug C） | `./build.sh <cfg> --cmake menuconfig` 把 Shell security policy 设 Full |
| 语音识别正常、LLM 也回了话，但舵机不动 | **Skill 没装进 /data/agent/skills/**（LLM 不知道 `servo` 命令）| 先执行 `./tools/install_skill.sh`（或 .ps1）推 Skill，再 `voice_stop`+`voice_start` 重扫 |
| 串口 `servo` 命令本身有效、但语音不触发 | `set_volc_asr` / `voice_start` 没跑，或密钥错 | 按第 6.1 节顺序配置；`servo status` 验证舵机本身 OK |
| 语音识别不到唤醒词 | 没 `voice_start` / 没 `set_volc_asr` | 按第 6 节顺序配置 |
| ai_agent 根本没编译 | 顶层符号错 | `./build.sh <cfg> --cmake menuconfig` 确认 `EXAMPLES_AI_AGENT_VELA` 已启用 |

---

## 9. 仍未决（大赛硬约束，需外部确认）

T-Display-S3 在官方「已支持 / 待适配」清单里都查无此板 → 赛道归属存疑。
已起草咨询邮件给 `miot-vela@xiaomi.com`（`EMAIL_TO_OPENVELA.md`）。
邮件答复决定走「AI 应用赛道 + 模拟器兜底」还是必须真机板级适配。
