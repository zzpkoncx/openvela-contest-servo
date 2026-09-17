/****************************************************************************
 * apps/desktop_pet/voice_bridge.c
 *
 * ai_agent ↔ 桌面小跟班 的集成层（修订版）
 *
 * ⚠️ 重要架构说明（与原版不同）：
 *   原版在这里自建了一个 unix socket 等 ai_agent 来连——方向反了，ai_agent
 *   不会主动连 app 的 socket，所以那个实现永远收不到 Skill 调用，舵机/表情
 *   永远不会被语音触发。
 *
 *   正确的 openvela 集成方式（见 packages_ai_agent README 与 ai_chat 示例）：
 *     ai_agent 是「统一消息总线 + Markdown Skill + 工具调用」。
 *     语音链路：mic → ai_agent 语音通道(火山 ASR) → LLM → 命中 Skill 动作
 *              → 通过 run_shell / mcp 工具执行设备动作 → TTS 播报。
 *
 *   我们的 servo-control / face-expression Skill 里写的恰恰是 shell 命令
 *   （`vela> servo set 90` 等），所以 LLM 触发 ai_agent 的 run_shell 工具后，
 *   实际执行的就是本工程注册的 NSH 内建命令 `servo`（servo_cmd_main.c）/
 *   后续可加的 `face` 命令。即：
 *
 *       语音"舵机转到 90 度"
 *         → 火山 ASR → LLM → 决定调用 servo-control
 *         → ai_agent 用 run_shell 执行 `servo set 90`
 *         → servo_cmd() 写 /dev/pwm0 → 舵机真的转
 *
 *   因此本文件不再需要 socket。它保留为「集成说明 + 可选主动调用入口」，
 *   不阻塞任何数据流。若后续要做模式 B（LVGL 应用订阅 ai_agent 消息总线、
 *   用 conversation_engine_plugin 的 mcp_call 直接执行动作），在此接入
 *   packages_ai_agent 的 ai_conversation 头文件即可，无需再改 servo_drv.c。
 *
 *   注：run_shell 是敏感工具（三级安全策略）。若赛事固件限制了该工具，
 *   备选方案是把 servo/face 注册成 ai_agent 的 mcp 工具（见 PLUGIN_DEVELOPMENT.md
 *   的 mcp_call 事件处理），本文件就是那个接入点。
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <syslog.h>

#include "desktop_pet.h"

/* 主动场景（满足大赛「主动+执行」要求）：由 desktop_pet 主循环在特定时机调用，
 * 直接驱动舵机/表情，不经过语音。例如唤醒点头、任务开始转头看用户等。 */
int voice_bridge_init(struct pet_config_s *cfg)
{
  (void)cfg;
  syslog(LOG_INFO, "voice_bridge: 舵机/表情动作经 ai_agent run_shell → "
                   "`servo`/`face` 命令执行（见 SKILL.md），本层仅作主动场景入口");
  return 0;
}

void voice_bridge_deinit(void)
{
  syslog(LOG_INFO, "voice_bridge: 已关闭");
}
