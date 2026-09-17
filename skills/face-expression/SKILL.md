---
name: face-expression
version: 1.0.0
description: 控制桌面小跟班 TFT 屏幕上的机器人表情（开心/难过/惊讶/眨眼/思考/睡觉/愤怒/爱心眼）
author: 极速派 (jizhipai)
target: openvela / ai_agent
trigger: 当用户表达情绪、想让机器人做表情、或者配合其他动作时调用
---

# Face Expression Skill

## 功能

控制桌面小跟班 1.9 寸 ST7789V TFT 屏幕（170×320）上的机器人表情显示。

## 触发词

ai_agent 在听到「你好，openvela」唤醒后，识别以下意图会自动调用本 Skill：

- 「把表情换成 开心 / 难过 / 惊讶 / 眨眼 / 思考 / 睡觉 / 愤怒 / 爱心眼」
- 「表情开心一下」/「给我笑一个」/「严肃点」/「你困了吗」
- 「换到第 N 个表情」/「显示 1 号表情」

## 支持的表情

| ID | 名称 | 触发语示例 | 用途 |
|---|---|---|---|
| 0 | 默认 | 默认 / 待机 | 空闲显示 |
| 1 | 开心 | 开心 / 笑 / 高兴 / 笑一个 | 任务完成 / 收到好消息 |
| 2 | 难过 | 难过 / 不开心 / 伤心 / 委屈 | 错误反馈 / 收到坏消息 |
| 3 | 惊讶 | 惊讶 / 哇 / 什么 / 震惊 | 收到意外消息 |
| 4 | 眨眼 | 眨眼 / 抛媚眼 / wink | 互动卖萌 |
| 5 | 思考 | 思考 / 想一下 / 让我想想 | 任务进行中 |
| 6 | 睡觉 | 睡觉 / 休息 / 困了 | 长时间无操作 |
| 7 | 愤怒 | 愤怒 / 生气 / 警告 | 危险警告 / 错误 |
| 8 | 爱心眼 | 爱心 / 喜欢 / 比心 | 表达喜欢 |

## 调用方式

### 1. 自然语言（最常用）

```
你: 你好，openvela，把表情换成开心
LLM: 好的，已切换到开心表情。😊
[face-expression] set to happy (id=1, duration=0)

你: 你好，openvela，先思考再告诉我
LLM: 让我想想。🤔
[face-expression] set to thinking (id=5, duration=0)
```

### 2. shell 命令（直调，绕过 LLM）

```bash
vela> face set happy              # 按名字
vela> face set 1                  # 按 ID
vela> face set 1 5000             # 显示 5 秒后回默认
vela> face list                   # 列出所有表情
vela> face current                # 当前表情
```

### 3. C 代码（被其他 openvela 应用调用）

```c
#include <sys/boardctl.h>

struct face_msg_s {
    uint8_t cmd;     // 0=set, 1=get, 2=list
    uint8_t id;      // 表情 ID（0~8）
    uint16_t dur_ms; // 持续时间（0=永久）
};

struct face_msg_s msg = { .cmd = 0, .id = 1, .dur_ms = 0 };
boardctl(BOARDIOC_FACE_SET, (uintptr_t)&msg);
```

### 4. NVS / 消息总线（进程间通信）

桌面小跟班应用监听 `/dev/face_ctl` 设备节点，外部应用：

```c
int fd = open("/dev/face_ctl", O_WRONLY);
write(fd, "1\n", 2);   // 切到 ID=1
close(fd);
```

## 实现细节

- **C 实现**：`apps/desktop_pet/face_lcd.c`
- **资源位置**：`/data/agent/skills/face-expression/assets/face_<id>.bin`
- **图像格式**：RGB565，170×320 像素，每张 ~108KB
- **刷新方式**：DMA + 8080 并口，~30ms/帧
- **与 MicroPython 版本兼容**：可直接复用之前生成的 `face_img.bin`

## 返回值

| 字段 | 类型 | 说明 |
|---|---|---|
| status | string | `"ok"` / `"error"` |
| current_id | int | 当前表情 ID |
| current_name | string | 当前表情名称 |
| duration_ms | int | 剩余显示时间（0=永久） |
| error | string | 错误信息（如有） |

## 错误处理

- 表情 ID 越界（< 0 或 > 8）→ 切到默认（id=0）+ warning
- LCD 初始化失败 → 返回 error，但不影响其他 Skill
- 资源文件缺失 → 切到默认 + 写入 /tmp/face_expression_error.log

## 主动调用场景

满足大赛「主动+执行」要求：
- 启动后默认显示 `id=0`（默认）
- 收到语音时切换到 `id=5`（思考）
- 任务完成切换到 `id=1`（开心）
- 5 分钟无操作切换到 `id=6`（睡觉）
