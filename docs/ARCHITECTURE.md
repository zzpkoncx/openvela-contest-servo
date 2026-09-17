# 系统架构

## 总览图

```
┌─────────────────────────────────────────────────────────────────┐
│                      用户说「你好，openvela」                       │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│  INMP441 麦克风  (I2S GPIO1/2/16, 16kHz/16bit/mono)              │
└────────────────────────────┬────────────────────────────────────┘
                             │ PCM 数据
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│  openvela ai_agent 应用  (内置)                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ audio input → 火山引擎 ASR (流式 WebSocket)                ││
│  │ 识别出文本 → LLM (豆包/Kimi/Qwen/...)                       ││
│  │ LLM 决定调用哪些 Skill（基于 SKILL.md 描述）                  ││
│  │ 调起 Skill 参数（JSON over unix socket）                     ││
│  └─────────────────────────────────────────────────────────────┘│
└────────────────────────────┬────────────────────────────────────┘
                             │ {"skill":"face-expression","params":{...}}
                             │ {"skill":"servo-control","params":{...}}
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│  desktop_pet 应用  (我们写的)                                    │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ voice_bridge 监听 Skill Bus → 解析 JSON                      ││
│  │    ├─→ face-expression  → face_lcd.c → /dev/lcd0 → ST7789V  ││
│  │    ├─→ servo-control    → servo_drv.c → LEDC PWM → GPIO18    ││
│  │    ├─→ weather          → http client → wttr.in              ││
│  │    └─→ daily-briefing   → cron 触发 + 组装文本 → TTS          ││
│  └─────────────────────────────────────────────────────────────┘│
│  主动任务：cron 线程每天 7:00 触发 daily-briefing                │
└────────────────────────────┬────────────────────────────────────┘
                             │ 文字结果
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│  火山引擎 TTS  (WebSocket 流式)                                   │
│  → 板载喇叭 (I2S DAC, GPIO3/4/5)                                │
└─────────────────────────────────────────────────────────────────┘
```

## 关键设计

### 1. 不重写 ai_agent，专注 Skill
ai_agent 已经把 90% 的活干了：
- ASR（流式 WebSocket）
- 多 LLM 后端（豆包/Kimi/Qwen/DeepSeek/GLM/MiMo）
- TTS（流式 WebSocket）
- Skill 自动发现（扫描 `/data/agent/skills/`）
- 工具调用（function calling）
- 多通道（CLI/voice/WeChat/Feishu/WebSocket/MQTT）

我们要做的只是**写 Skill 描述 + 底层 C 实现**。

### 2. 进程间通信
- ai_agent → desktop_pet：unix socket `/data/agent/skills_bus.sock`
- 消息格式：JSON `{"skill": "...", "params": {...}}`
- 协议简单，调试方便（`socat` 就能测）

### 3. 主动场景
满足大赛「主动+执行」要求：
- **每日简报**（daily-briefing Skill）：cron 每天 7:00 触发
- **表情自动切换**：
  - 启动 → 默认表情
  - 收到语音 → 思考表情
  - 任务完成 → 开心表情
  - 5 分钟无操作 → 睡觉表情
- **舵机联动**：
  - 唤醒时点头（center → +10° → center）
  - 任务开始时左看右看

### 4. 资源管理
- 表情图片：`/data/agent/skills/face-expression/assets/face_*.bin`
  - 9 张 × 108KB = 1MB（在 8MB PSRAM 里随便放）
- 配置文件：`/data/.env`
- 日志：`/tmp/desktop_pet.log`、`/tmp/ai_agent.log`、`/tmp/boot_setup.log`

## 文件系统布局（板子上）

```
/data/
├── .env                              # 火山引擎 Key 等配置
├── boot_setup.sh                     # 开机自启动
├── voice_wake_test.sh                # 一键测试
├── agent/
│   └── skills/
│       ├── face-expression/
│       │   ├── SKILL.md
│       │   └── assets/
│       │       ├── face_0_default.bin
│       │       ├── face_1_happy.bin
│       │       └── ... (9 个)
│       ├── servo-control/SKILL.md
│       ├── weather/SKILL.md
│       └── daily-briefing/SKILL.md
├── skills_bus.sock                   # unix socket（运行时）
└── ai_agent_state.json               # ai_agent 持久化状态

/usr/bin/
├── desktop_pet                       # 我们的主程序
├── ai_agent                          # ai_agent 框架（已存在）
└── vela                              # 串口 CLI（已存在）

/etc/init.d/
└── S99desktop_pet → /data/boot_setup.sh
```

## 数据流时序图

```
用户     麦克风    ai_agent   火山引擎ASR  LLM       voice_bridge  硬件
 │        │          │            │          │            │          │
 │ "你好" │          │            │          │            │          │
 ├───────→│  PCM     │            │          │            │          │
 │        ├─────────→│  WS        │          │            │          │
 │        │          ├───────────→│          │            │          │
 │        │          │←───────────┤  text    │            │          │
 │        │          ├──────────────────────→│            │          │
 │ "openvela 把舵机转 90 度"         │          │            │          │
 │        │          ├──────────────────────→│  工具调用  │          │
 │        │          │←──────────────────────────────────────→│ 解析JSON
 │        │          │            │          │            ├─────────→│ PWM 90°
 │        │          │            │          │            │←─────────│ OK
 │        │          │←─────────────────────────────────────────────┤
 │        │          │  文字 "已转到 90 度"                        │
 │        │          ├───────────→│  TTS     │            │          │
 │        │  喇叭    │←───────────┤  PCM     │            │          │
 │←───────┤←─────────┤            │          │            │          │
```

## 性能指标

| 指标 | 实测（估算） | 备注 |
|---|---|---|
| 唤醒词检测延迟 | ~300ms | 火山引擎 ASR 流式 |
| LLM 推理 | 500-1500ms | 豆包最快 |
| Skill 调用 + 硬件执行 | ~50ms | 本地 |
| TTS 播放 | 200-500ms | 流式 |
| 总端到端 | 1-2.5s | 主流对话机器人水平 |

## 内存占用估算

| 组件 | RAM | PSRAM | Flash |
|---|---|---|---|
| openvela 内核 | 80KB | - | 600KB |
| ai_agent 框架 | 60KB | 200KB | 300KB |
| ai_agent 运行（llm context） | - | 1MB | - |
| desktop_pet 应用 | 16KB | 100KB (表情缓存) | 80KB |
| LLM 推理临时 | - | 2MB | - |
| 表情资源（9张） | - | - | 1MB |
| 板子系统堆 | 100KB | - | - |
| **合计** | **~256KB** | **~3.3MB** | **~2MB** |

ESP32-S3 内部 RAM ~512KB，PSRAM 8MB——**完全装得下**。
