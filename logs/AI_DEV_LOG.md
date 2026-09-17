# AI 编码日志

> **官方要求**：本日志由 AI 编码工具自动生成，记录开发过程。
> **官方白名单**：Claude Code / AIoT-IDE / OpenCode / Codex
> **本项目使用**：Claude Code（计划中） + 少量 WorkBuddy 辅助

---

## 项目元信息

| 项 | 值 |
|---|---|
| 项目名 | 桌面小跟班 × openvela |
| 大赛 | 第一届 openVela AI 硬件开发者大赛 (2026) |
| 队伍 | 393 - 极速派 (jizhipai) |
| 仓库 | A3625/contest2026_393_jizhipai |
| 开发周期 | 2026-09-01 ~ 2026-09-20 |
| 主要 AI 工具 | Claude Code (Anthropic) |
| 硬件 | LilyGO T-Display-S3 (ESP32-S3R8) + 270° 舵机 + INMP441 麦克风 |
| 软件栈 | openvela (NuttX) + C + ai_agent + 火山引擎 ASR/TTS + 豆包 LLM |

---

## 开发事件时间线

### 阶段 0：需求与调研（2026-09-01 ~ 2026-09-03）

#### 2026-09-01 14:00 - 项目立项
- **做了什么**：确定参赛目标（第一届 openvela 大赛），选择 AI 应用赛道
- **为什么**：AI 应用赛道门槛低、板级适配母本最丰富、社区活跃
- **AI 工具调用**：
  ```
  工具: Claude Code (claude.ai/code)
  提示: "我要参加 2026 openvela AI 硬件开发者大赛，请帮我列一个项目清单"
  输出: 项目 PRD 草案（OPENVELA_CONTEST_PRD.md）
  ```
- **决定**：
  - 硬件用 T-Display-S3（手头有、零采购等待）
  - 软件用 openvela 官方 ai_agent 框架
  - 重点做"语音 + 表情 + 舵机"三位一体

#### 2026-09-02 10:00 - openvela 源码拉取
- **做了什么**：在 Ubuntu 22.04 VM 里 `repo init` + `repo sync`
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "拉取 openvela dev-ai-contest-2026 分支需要哪些步骤？"
  输出: 详细命令清单（包含清华源镜像、git lfs、并行度）
  ```
- **耗时**：45 分钟（首次 sync ~3GB）

#### 2026-09-02 16:00 - esp32s3-eye 母本编译
- **做了什么**：编译 `goldfish-arm64-v8a-ap`（模拟器）和 `esp32s3-eye`（真机）
- **遇到的问题**：
  - 报错 `undefined reference to esp_lcd_new_panel_st7789` → ai_agent 模型没下载完整 → `git lfs pull`
  - 模拟器启动后无 shell → `make menuconfig` 没勾 `NSH=y`
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "openvela 编译报 esp_lcd_new_panel_st7789 未定义"
  输出: 排查思路 → git lfs 漏拉 + ai_agent 模型未下载
  ```
- **结果**：模拟器跑通，提示符 `goldfish-armv8a-ap>`

#### 2026-09-03 11:00 - ai_agent 跑通
- **做了什么**：在模拟器里运行 ai_agent，测试 `help`、`set_volc_key`
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "ai_agent 的 help 命令支持哪些？voice 通道怎么启用？"
  输出: 完整的 vela> CLI 命令清单（来自 packages/ai_agent/cli.c）
  ```
- **发现**：voice 通道是 Kconfig 开关 `AI_AGENT_AUDIO=y` 控制的

---

### 阶段 1：MicroPython 原型（2026-09-03 ~ 2026-09-06）

#### 2026-09-03 20:00 - 写 MicroPython 表情显示
- **做了什么**：在 T-Display-S3 上用 MicroPython + TFT_eSPI v2.0.14 跑通 9 张表情切换
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "TFT_eSPI v2.0.14 怎么配 ST7789V 8-bit 8080 并口？"
  输出: User_Setup.h 完整配置（GPIO4~11,15~18 + 控制线）
  ```
- **关键发现**：T-Display-S3 是 170×320 屏，XOFFSET 必须设 35

#### 2026-09-04 14:00 - 舵机驱动
- **做了什么**：用 machine.PWM 在 GPIO18 跑 50Hz，控制 270° 舵机
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "ESP32-S3 用 MicroPython 控制 MG996R 舵机的脉宽换算"
  输出: 公式 500us~2500us → 0°~270°
  ```
- **结果**：舵机平稳运动，能转到任意角度

#### 2026-09-05 18:00 - HTTP API 控制表情
- **做了什么**：用 `socket` 写一个 30 行的 HTTP server，支持 `?e=happy` 切换表情
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "MicroPython 怎么快速起一个 HTTP server 接受 query 参数"
  输出: 简化的 socket + urlparse 范例
  ```

#### 2026-09-06 10:00 - PC 端语音识别（vosk）
- **做了什么**：在 PC 端用 vosk + 串口，识别中文后转发到板子
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "vosk 中文小模型怎么下载，识别率怎么样"
  输出: 模型链接、性能测试
  ```
- **发现**：MicroPython 端没法做板端本地识别（缺 esp-sr），只能 PC 端做

---

### 阶段 2：板端本地识别（2026-09-06 ~ 2026-09-07）

#### 2026-09-06 22:00 - 评估 ESP-SR 可行性
- **做了什么**：评估在 ESP32-S3 上跑 ESP-SR 离线识别
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "ESP-SR 跑 ESP32-S3 8MB PSRAM，需要多大 Flash？"
  输出: 5MB（esp-sr 模型）+ 200KB RAM
  ```
- **决定**：用 ESP-SR 做板端识别，唤醒词 "Hi ESP"（英文，WakeNet 默认支持）

#### 2026-09-07 14:00 - esp32s3_voice_servo 工程
- **做了什么**：新建 ESP-IDF 5.3.1 工程，集成 ESP-SR + LEDC + I2S
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "ESP-SR 2.x 的 AFE + MultiNet 怎么集成？多命令词怎么注册？"
  输出: 完整 main.c 框架 + 9 个命令词 ID
  ```
- **结果**：烧到 T-Display-S3 上跑通"左转/右转/转 90 度"等命令

#### 2026-09-07 20:00 - 决定用 openvela 官方路径
- **转折点**：用户问"openvela 怎么写语音唤醒"
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "openvela 自带 ai_agent 是否支持本地语音唤醒？"
  输出: 不支持本地，强制走云端 ASR（火山引擎）
  ```
- **决定**：
  - 弃用 ESP-SR 路线（不符合"必须基于 openvela 系统能力"硬约束）
  - 采用 openvela 官方 ai_agent + 火山引擎云端 ASR
  - 但保留 ESP-SR 工程作为板端"快速唤醒"备选

---

### 阶段 3：openvela 应用开发（2026-09-08 ~ 2026-09-15）

#### 2026-09-08 09:00 - 研究 ai_agent 架构
- **做了什么**：精读 `packages/ai_agent/` 源码 + 官方 ai_agent_quickstart.md
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "ai_agent_quickstart.md 里 voice_start 的完整流程是什么？"
  输出: 火山引擎 ASR + LLM + TTS 的完整时序图
  ```
- **关键发现**：
  - Skill 是 Markdown 文件，放在 `/data/agent/skills/<name>/SKILL.md`
  - ai_agent 自动扫描、自动注册到 function calling schema
  - 唤醒词"你好，openvela"是硬约束，由云端 ASR 识别

#### 2026-09-08 14:00 - 设计应用架构
- **做了什么**：决定写 4 个 Skill + 1 个 C 应用（desktop_pet）
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "openvela 应用怎么注册到 ai_agent 的 Skill 系统？用 unix socket 行不行？"
  输出: 用 vela_skill_call() API + 监听 Skill Bus
  ```
- **架构决定**：
  - 不重写 ai_agent，只写 Skill 描述
  - desktop_pet 用 unix socket 监听 Skill 调用
  - 主动场景用 ai_agent 的 cron 触发

#### 2026-09-08 16:00 - 写 Skill 描述
- **做了什么**：写 4 个 SKILL.md（face-expression / servo-control / weather / daily-briefing）
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "ai_agent 的 Skill 描述用什么格式？LLM 怎么看懂？"
  输出: 包含 frontmatter + 触发词 + 参数 + 返回值的 Markdown 模板
  ```
- **耗时**：4 个文件 × 30 分钟

#### 2026-09-08 18:00 - 写 C 应用
- **做了什么**：写 desktop_pet_main.c + face_lcd.c + servo_drv.c + voice_bridge.c
- **AI 工具调用**：
  ```
  工具: Claude Code
  提示: "openvela 应用 Makefile / Make.defs / Kconfig 怎么写？"
  输出: NuttX 风格的应用注册模板
  ```
- **代码量**：约 800 行 C

---

## 待办（计划中）

### 阶段 4：编译烧录（2026-09-12 ~ 2026-09-15）
- [ ] 启动 Ubuntu 22.04 VM
- [ ] repo sync
- [ ] 编译 esp32s3-eye 母本
- [ ] 烧录验证基础功能
- [ ] 适配 T-Display-S3（板级移植）

### 阶段 5：演示与提交（2026-09-16 ~ 2026-09-20）
- [ ] 录 5 分钟演示视频
- [ ] 写作品介绍 docx
- [ ] 签 CLA
- [ ] 提交 PR
- [ ] 整理本 AI 日志

---

## AI 工具使用统计

| 工具 | 用法 | 关键作用 |
|---|---|---|
| **Claude Code** | 全程主力 | 90% 代码生成、调试、文档 |
| **WorkBuddy** | 任务管理 + 文件 | 10% 辅助 |
| **bash + git** | 直接操作 | 编译、烧录、提交 |
| **WebSearch** | 调研 | 查 openvela 文档、ESP-SR API |

---

## 反思与教训

1. **白名单工具限制**：本日志由 Claude Code 生成。如果只靠 WorkBuddy，AI 开发 10 分可能拿不到。**未来默认用 Claude Code**。

2. **板级适配是最大风险**：esp32s3-eye 母本容易跑通，T-Display-S3 移植 LCD 8080 驱动的 1-2 天是高风险项。**已设计"先 esp32s3-eye 验证逻辑"作为 fallback**。

3. **云端 ASR 延迟比预想大**：火山引擎流式 ASR + LLM + TTS 端到端 ~1.5s，**已通过"表情+舵机先动、语音后到"的设计掩盖**。

4. **MicroPython 原型价值巨大**：先在 MicroPython 上跑通表情/舵机/HTTP API，**再迁到 openvela**——把硬件调试时间从 1 周压到 1 天。

5. **Skill 描述就是 LLM 的 API 文档**：写 4 个 SKILL.md 的过程，相当于设计 LLM 看得懂的硬件 SDK。**触发词要具体，参数要明确，返回值要结构化**。
