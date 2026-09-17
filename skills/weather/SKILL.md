---
name: weather
version: 1.0.0
description: 查询当前/未来天气，支持定位（IP/手动）和多城市
author: 极速派 (jizhipai)
target: openvela / ai_agent
trigger: 当用户问天气、气温、会不会下雨、穿什么时调用
---

# Weather Skill

## 功能

查询当前和未来 3 天的天气情况。

## 触发词

- 「今天天气怎么样」/「明天会下雨吗」/「后天多少度」
- 「北京天气」/「上海会下雪吗」/「深圳现在多少度」
- 「该穿什么」/「需要带伞吗」/「适不适合出门」

## 数据源

- 主：wttr.in（无需 Key，全球城市，中文友好）
- 备：和风天气 / OpenWeatherMap（需 Key）

## 调用方式

### 1. 自然语言

```
你: 你好，openvela，今天天气怎么样
LLM: 让我看看。
[weather] query=current city=auto
LLM: 今天北京晴，22°C，东南风 2 级，适合出门。☀️

你: 你好，openvela，明天上海会下雨吗
LLM: 让我查查。
[weather] query=2026-09-09 city=上海
LLM: 明天上海多云转小雨，气温 18~24°C，出门记得带伞。🌦️
```

### 2. shell 命令

```bash
vela> weather now
vela> weather city 北京
vela> weather now 上海
vela> weather forecast 3
vela> weather setcity 北京
```

## 实现

调用 wttr.in：

```bash
curl -s "https://wttr.in/北京?format=j1&lang=zh" | jq '.current_condition[0]'
```

## 返回值

| 字段 | 类型 | 说明 |
|---|---|---|
| status | string | ok / error |
| city | string | 城市名 |
| temp_c | int | 当前气温（°C） |
| feels_like_c | int | 体感温度 |
| humidity | int | 湿度 % |
| condition | string | 晴/多云/雨/雪 |
| wind | string | 风向 + 风力 |
| forecast | array | 未来 3 天 |

## 主动调用场景

满足「主动+执行」要求：
- 每天早上 7:00 自动播报当日天气 + 穿衣建议
- 检测到下雨 → 表情变"惊讶" + 提醒"记得带伞"
- 极端天气（>35° 或 <-10°）→ 表情变"愤怒" + 推送提醒
