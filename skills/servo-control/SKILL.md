---
name: servo-control
version: 1.0.0
description: 控制桌面小跟班的 270° 舵机（摇头），支持角度/方向/速度/摆头等动作
author: 极速派 (jizhipai)
target: openvela / ai_agent
trigger: 当用户想控制舵机、摇头、让小跟班"动一动"时调用
---

# Servo Control Skill

## 功能

控制桌面小跟板载的 270° 舵机（点头/摇头用），通过 LEDC PWM 输出 50Hz 控制信号。

## ⭐ 语音控制左右转（最常用、头号演示场景）

这是本 Skill 最核心的语音能力。唤醒后说下面任意一句，ai_agent 都会经 `run_shell`
执行 `servo left` / `servo right`，让舵机真正左右转动：

| 你说的话（唤醒词后） | 实际执行 |
|---|---|
| 「舵机往左转」/「往左转」/「向左转」/「左转」 | `servo left` （默认左转 30°）|
| 「舵机往右转」/「往右转」/「向右转」/「右转」 | `servo right`（默认右转 30°）|
| 「头往左边转」/「看左边」/「往左边看」 | `servo left` |
| 「头往右边转」/「看右边」/「往右边看」 | `servo right` |
| 「左转 45 度」/「往左转 60」 | `servo left 45` / `servo left 60`（带步长）|
| 「右转 90 度」/「往右转 30」 | `servo right 90` / `servo right 30`（带步长）|

> 角度约定：0° 在最左，270° 在最右，135° 在正中间。`left` = 角度减小（往左），
> `right` = 角度增大（往右）。每次转动是**相对当前角度**的步进，可连续说"再往左转"
> 让它一点点转过去。想精确归位就说「回到中间」→ `servo center`（135°）。

## 触发词

ai_agent 在听到「你好，openvela」唤醒后，识别以下意图会自动调用本 Skill：

- 「舵机转到 N 度」/「转到 N 度」/「转 N 度」
- **「舵机往左转 / 往右转 / 向左转 / 向右转 / 左转 / 右转 / 看左边 / 看右边」**
- 「舵机转一圈」/「来回摆头」/「摇个头」
- 「舵机回到中间 / 居中 / 正中」
- 「舵机停 / 停止」
- 「慢慢转 / 快转 / 加速」（控制速度）

## 支持的动作

| action | 参数 | 说明 |
|---|---|---|
| `set_angle` | 0~270（度） | 转到指定绝对角度 |
| `turn_left` | 步长 1~135（默认 30°）| 向左转（角度减小） |
| `turn_right` | 步长 1~135（默认 30°）| 向右转（角度增大） |
| `center` | - | 回到 135° 中间位置 |
| `sweep` | speed: `slow` / `medium` / `fast` | 来回摆头（摇头动画）|
| `stop` | - | 立即停止当前动作 |

## 调用方式

### 1. 自然语言

```
你: 你好，openvela，舵机往左转
LLM: 好的，向左转 30 度。
[servo-control] action=turn_left step=30 → run_shell: servo left

你: 你好，openvela，再往左转 45 度
LLM: 好的，再向左转 45 度。
[servo-control] action=turn_left step=45 → run_shell: servo left 45

你: 你好，openvela，往右转
LLM: 好的，向右转 30 度。
[servo-control] action=turn_right step=30 → run_shell: servo right

你: 你好，openvela，右转 90 度
LLM: 好的，向右转 90 度。
[servo-control] action=turn_right step=90 → run_shell: servo right 90

你: 你好，openvela，回到中间
LLM: 好的，回到中间 135 度。
[servo-control] action=center → run_shell: servo center

你: 你好，openvela，舵机转到 90 度
LLM: 好的，舵机已转到 90 度。
[servo-control] action=set_angle angle=90 → run_shell: servo set 90

你: 你好，openvela，让舵机摇个头
LLM: 好的，开始摆头。
[servo-control] action=sweep speed=slow → run_shell: servo sweep slow
```

> 关键：语音识别到"左/右/转"等词后，务必调用 `run_shell` 执行 `servo left` /
> `servo right`（**不是**去调一个不存在的 turn_left 函数）。舵机驱动只认 NSH 命令 `servo`。

### 2. shell 命令

```bash
vela> servo set 90              # 转到 90°
vela> servo left 30            # 向左转 30°
vela> servo right              # 向右转 30°（默认步长）
vela> servo center             # 回到 135°
vela> servo sweep fast         # 快速摆头
vela> servo stop               # 停止
vela> servo status             # 查询状态
vela> servo greet              # 手动触发主动打招呼（摆头）
```

### 3. C 代码（应用内直接调用，无需经语音）

舵机驱动已编译进 `desktop_pet` 应用，对外接口在 `apps/desktop_pet/desktop_pet.h`：

```c
#include "desktop_pet.h"   /* 舵机接口：servo_init / servo_cmd / servo_stop / servo_greet ... */

/* 动作枚举：0=set_angle 1=left 2=right 3=center 4=sweep 5=stop */
/* 速度枚举：0=stop 1=slow(~150ms/60°) 2=medium(~80ms) 3=fast(~50ms) */

/* 初始化（必须先调用，会 open("/dev/pwm0")）*/
servo_init();

/* 转到绝对角度 90°（中速）*/
servo_cmd(SERVO_SET_ANGLE, 90, SERVO_SPEED_MEDIUM);

/* 向左转 45° */
servo_cmd(SERVO_LEFT, 45, SERVO_SPEED_MEDIUM);

/* 来回摆头（快速）*/
servo_cmd(SERVO_SWEEP, 0, SERVO_SPEED_FAST);

/* 立即停止 */
servo_stop();

/* 主动打招呼（默认不开机自动；手动 servo greet 或开启 AUTOGREET 才触发）*/
servo_greet();
```

> 注意：上行语音经 `run_shell` 执行的是 NSH 内建命令 `servo`（`servo_cmd_main.c`），
> 它内部就是调用上面的 `servo_cmd()`；本 C 接口用于应用内主动触发，两条路径殊途同归。

## 硬件参数

- **型号**：MG996R / SG90 / TS-90A 等标准 50Hz 舵机
- **接口**：ESP32-S3 LEDC 通道 0，GPIO42（默认；因 GPIO42=LCD D3 已关屏；保留屏幕改 21 见 board.h / defconfig）
- **频率**：50Hz（20ms 周期）
- **分辨率**：14-bit（精度 ~1.2us/LSB）
- **脉宽范围**：500~2500us（对应 0°~270°）
  - 0° → 500us
  - 90° → 1000us
  - 135° → 1500us
  - 180° → 2000us
  - 270° → 2500us
- **电源**：5V 独立供电（**不能**从板子 3.3V 取，会烧 ESP32）
- **信号线**：**GPIO42** → 舵机橙线（信号），GND → 舵机棕线（地）

> ⚠️ 端口冲突（务必看）：GPIO42 = LCD 数据线 D3（PCB 硬布线）。本项目**当前默认 GPIO42**（用户指定"42 端口"），
> 故 LCD 已关闭（CONFIG_LCD=n，face 模块随之关闭），把 GPIO42 让给舵机 PWM。GPIO24 模组未引出，不可用。
> 若想保留屏幕，改 `CONFIG_ESP32S3_LEDC_CHANNEL0_PIN` 与 `CONFIG_APP_DESKTOP_PET_SERVO_GPIO` 为 21（或 10），并恢复 CONFIG_LCD=y，无需改 C 代码。

## 速度档位

| speed | 名称 | 一步 60° 耗时 | 用途 |
|---|---|---|---|
| 0 | stop | - | 立即停止 |
| 1 | slow | ~150ms | 默认，温柔、可看 |
| 2 | medium | ~80ms | 较快 |
| 3 | fast | ~50ms | 极限，避免堵转烧舵机 |

## 安全限制

- **角度夹紧**：所有 set_angle 自动夹到 [0, 270] 区间
- **超时保护**：默认动作 1.5 秒后自动 stop（防堵转烧舵机）
- **启动校验**：首次执行前会发 3 次中心点 PWM，确认舵机有响应
- **看门狗**：连续 30 秒持续高电平 → 强制 stop 并报警
- **过流保护**：依赖外部 5V 电源的限流（板子只供信号）

## 返回值

| 字段 | 类型 | 说明 |
|---|---|---|
| status | string | `"ok"` / `"error"` / `"busy"` |
| current_angle | int | 当前角度（0~270） |
| target_angle | int | 目标角度（动作完成后） |
| is_sweeping | bool | 是否在摆头模式 |
| speed | int | 当前速度档位 0~3 |
| error | string | 错误信息（如有） |

## 错误处理

- 角度越界 → 自动夹紧 + warning
- 舵机无响应（信号线断开）→ 启动重试 3 次 → error
- 已经在 sweep 模式又收到 set_angle → 打断 sweep，执行 set_angle

## 主动调用场景

**默认行为（CONFIG_APP_DESKTOP_PET_SERVO_AUTOGREET=n）**：舵机**只听指令才动**。
开机、每日简报都不会自动摆头；舵机仅在以下情况移动：

- 语音唤醒「你好，openvela」+ 说"舵机转到 N 度 / 摇个头 / ..." → ai_agent 经 `run_shell` 执行 `servo ...`
- 串口/终端直接敲 `servo set 90` 等命令
- 你手动敲 `servo greet` 触发主动打招呼摆头

> 即：板子不会擅自动，完全由你的指令驱动。

满足大赛「主动+执行」要求（代码已落地，非仅文档）——当开启 `AUTOGREET=y` 时：

- **开机主动打招呼**：`desktop_pet_main.c` 启动后直接调用 `servo_greet()`，摆头打招呼，
  不依赖任何语音输入（证明设备有主动能力）。
- **每日简报主动打招呼**：到 `daily_briefing_at` 配置的时间，主循环主动切"思考"表情 +
  `servo_greet()` 摆头（ai_agent cron 之外的自包含主动触发，确保演示不依赖云端）。
- 手动 `servo greet` 命令随时可复现上述主动摆头（无需开 AUTOGREET）。
- 唤醒时舵机自动从当前角度"点点头"（center → +10° → center）
- 任务开始转 30°"看用户"（左看 → 右看 → center）
- 任务出错时来回摆头表示"搞不定"
- 睡觉表情（face id=6）下舵机停止动作
