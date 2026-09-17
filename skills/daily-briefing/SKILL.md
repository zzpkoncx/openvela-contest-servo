---
name: daily-briefing
version: 1.0.0
description: 每日简报——把今天日期、农历、天气、日程、提醒一口气播报
author: 极速派 (jizhipai)
target: openvela / ai_agent
trigger: 定时主动触发（每天 7:00），或用户说"汇报一下"/"今天怎么样"/"有什么安排"
---

# Daily Briefing Skill

## 功能

每日主动推送的「简报」：日期、农历、天气、行程、提醒、励志一句话。
满足大赛「主动+执行」硬性要求。

## 触发词

- 定时：每天 7:00（可在 config 里改）
- 手动：「汇报一下」/「今天怎么样」/「有什么安排」/「每日简报」

## 输出格式

```
早上好呀～
今天是 2026 年 9 月 8 日，农历七月廿六，星期二。
北京今天晴转多云，22~28°C，适合出门。
你今天有 3 个日程：
  10:00 团队站会
  14:00 客户对接
  19:00 健身
提醒：明天是大赛提交截止前一天，记得检查 README！
加油，新的一天开始啦 ☀️
```

## 数据源

- 日期/农历：内置 lunar 库（C 数组，无网络）
- 天气：调用 `weather` Skill
- 日程：暂用硬编码示例，未来接飞书/Google Calendar

## 主动触发实现

openvela 的 ai_agent 框架支持 `cron` 风格定时任务（`packages/ai_agent/cron.c`），
我们在 `apps/desktop_pet/desktop_pet_main.c` 里注册一个 daily 任务：

```c
/* 每天 7:00 触发 */
ai_agent_cron_register("0 7 * * *", "daily-briefing", daily_briefing_cb);
```

`daily_briefing_cb` 内部调用 `weather` Skill 拿天气，组装文本，调用 TTS 播报，
同时让舵机做"点头"动作（center → +15° → center）。

## 调用方式

### shell

```bash
vela> briefing now            # 立即播报
vela> briefing set 7:30       # 修改定时时间
vela> briefing disable        # 关闭定时
vela> briefing enable         # 开启
```

### C

```c
boardctl(BOARDIOC_BRIEFING_NOW, 0);
```

## 返回值

| 字段 | 类型 | 说明 |
|---|---|---|
| status | string | ok / error |
| played_at | string | 播报时间 |
| duration_ms | int | 播报耗时 |
| summary | string | 简报全文 |

## 主动调用场景

这是大赛最显著的「主动+执行」示范：
- **触发**：到点（7:00）或收到关键词
- **执行**：拉天气 + 拉日程 + 组装文本 + 调 TTS 播报 + 舵机点头
- **持续**：如果用户 5 分钟没回应 → 表情变回"默认" + 舵机停
