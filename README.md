# 桌面小跟班 × openvela 大赛

**参赛项目**：基于 openvela 的桌面陪伴机器人
**比赛**：第一届 openVela 全球 AI 硬件开发者大赛（2026）
**队伍**：393 - 极速派 (jizhipai)
**仓库**：[A3625/contest2026_393_jizhipai](https://github.com/A3625/contest2026_393_jizhipai)
**截止**：2026-09-20

---

## 这是什么

`openvela_contest/` 是提交到 openvela 大赛的所有代码、配置、文档。

**核心思路**：**不动** openvela 自带的 `ai_agent` 框架（它已经实现了 火山引擎 ASR + LLM + TTS 全链路），**只做两件事**：

1. 把我们的板子能力（LCD 表情 / 舵机 / 麦克风 / WiFi）注册成 ai_agent 能调用的 **Skill**（Markdown 文件）
2. 写一个轻量级 **C 应用**（`desktop_pet`）驻留后台：被 Skill 调用、刷新 LCD、控制舵机

> 这样既满足「必须基于 openvela 系统能力」+「≥1 个自定义 Skill」+「≥1 个主动+执行场景」的硬性指标，又把开发量压到最小。

---

## 5 分钟看到效果

> 前提：板子已经烧录 openvela 固件（用 `esp32s3-eye` 母本），串口能进 `vela>` 提示符

```bash
# 1. 把 Skills 复制到板子
scp -r skills/ root@192.168.0.123:/data/agent/

# 2. 填入火山引擎 Key
scp config/.env.example root@192.168.0.123:/data/.env
ssh root@192.168.0.123 "vi /data/.env"   # 填入 3 个 VOLC_* 值

# 3. 串口配置 + 启动语音
minicom -D /dev/ttyUSB0 -b 115200
vela> set_volc_key sk-xxxxxxxx
vela> set_volc_asr <app_id> <token> volcengine_streaming_common
vela> voice_start

# 4. 说话测试
你：「你好，openvela，把舵机转到 90 度」
板子：表情变"思考" → 舵机转到 90° → 播报"已转到 90 度"
```

完整操作 → [`docs/VOICE_WAKE_SETUP.md`](docs/VOICE_WAKE_SETUP.md)

---

## 目录结构

```
openvela_contest/
├── apps/desktop_pet/        # openvela 应用（C 代码）
│   ├── Kconfig
│   ├── Make.defs
│   ├── Makefile
│   ├── desktop_pet_main.c   # 主程序入口
│   ├── face_lcd.c           # LCD 表情显示（ST7789V 8080 并口）
│   ├── servo_drv.c          # 舵机 PWM
│   └── voice_bridge.c       # 与 ai_agent 消息总线通信
├── skills/                  # ai_agent Skill（Markdown 描述）
│   ├── face-expression/     # 表情控制
│   ├── servo-control/       # 舵机控制
│   ├── weather/             # 天气
│   └── daily-briefing/      # 每日简报
├── board/                   # 板级 defconfig
│   ├── esp32s3-eye/         # 官方母本（补丁）
│   └── t-display-s3/        # 自建板子（T-Display-S3）
├── config/
│   ├── .env.example         # 火山引擎 Key 占位
│   ├── boot_setup.sh        # 开机自启动脚本
│   └── voice_wake_test.sh   # 一键测试脚本
├── docs/
│   ├── VOICE_WAKE_SETUP.md  # 语音唤醒操作手册
│   ├── BUILD_AND_FLASH.md   # 编译烧录
│   ├── ARCHITECTURE.md      # 架构图
│   ├── SUBMISSION_GUIDE.md  # 提交指南
│   └── DEMO_VIDEO_SCRIPT.md # 演示视频脚本
└── logs/
    └── AI_DEV_LOG.md        # AI 编码日志（大赛要，10 分）
```

---

## 必做 vs 加分对照

| 大赛硬性要求 | 状态 | 说明 |
|---|---|---|
| 必须基于 openvela 系统能力 | ✅ | ai_agent + 我们写 Skill |
| ≥1 个自定义 Skill（`/data/agent/skills/`） | ✅ | 4 个 Skill（face/servo/weather/briefing）|
| ≥1 个「主动+执行」场景 | ✅ | 定时主动报天气 + 触发舵机动作 |
| 唤醒词「你好，openvela」 | ✅ | ai_agent 强制（云端 ASR） |
| 烧录真机 | 计划中 | T-Display-S3（基于 esp32s3-eye 母本） |
| 提交物齐全 | ✅ | README + 视频脚本 + AI 日志 + Skills |
| AI 编码日志（白名单工具） | ⚠️ | 用 Claude Code / AIoT-IDE / OpenCode / Codex 才能拿 10 分 |

---

## 接下来要做什么

1. [ ] 拉 `open-vela/contest2026_393_jizhipai` 到本地，把这些文件 copy 进去
2. [ ] 启动 Ubuntu 22.04 VM（跑 `setup_openvela_vm.ps1`）
3. [ ] 在 VM 里编译 `desktop_pet` + `ai_agent`（先跑 `goldfish-arm64-v8a-ap` 模拟器验证逻辑）
4. [ ] 烧到 T-Display-S3 真机（先 esp32s3-eye 母本通过，再移植）
5. [ ] 录 ≤5 分钟演示视频
6. [ ] 提交 PR + 签 CLA

详细 → [`docs/SUBMISSION_GUIDE.md`](docs/SUBMISSION_GUIDE.md)
