# 语音「喊了没用」排查（T-Display-S3 / openvela）

> 适用：板子已烧录、串口已连、对着麦喊「你好, openvela」却毫无反应。

## 0. 一句话根因（已用官方源码坐实）
ai_agent 的麦克风采集**不走 `/dev/i2s0`**，而是走 Vela 的 `media_recorder`
框架、用 `MEDIA_SOURCE_MIC` 去拿"系统默认麦源"。代码证据
（`packages_ai_agent/src/voice/audio_capture.c`）：
```c
audio_capture_open(const char* dev_path, ...) {
    (void)dev_path;   /* media framework handles routing */
    cap->recorder = media_recorder_open(MEDIA_SOURCE_MIC);
    ...
}
```
`dev_path` 被直接忽略。所以：**板子必须把某个麦注册成 Vela 音频层的
`MEDIA_SOURCE_MIC` 采集源**，ai_agent 才采得到音。

T-Display-S3 **没有原生声卡**，esp32s3-eye 的 ES8311 麦是在 Vela 音频服务层
注册的（nuttx 的 `esp32s3_bringup.c` / `esp32s3_appinit.c` 里**完全没有**
音频注册代码，证明注册在更高层的 Vela 音频服务）。我们当前的
`board/t-display-s3/board.c` 只调了 `esp32s3_i2s_init()`（裸 I2S），
**没有把 INMP441 注册成 `MEDIA_SOURCE_MIC`** → `media_recorder_open` 失败
→ 没音频 → 永远不唤醒。

## 1. 60 秒定位：到底卡在哪一环
串口连上板子（波特率 115200），在 `vela>` 提示符依次敲：
```
set_llm kimi <你的LLM Key>
set_volc_key <火山TTS Key>
set_volc_asr <APP_ID> <ACCESS_TOKEN> <cluster>     # 注意：ASR 要 3 个参数，不止 Key！
voice_start
```
然后**看串口日志**，对号入座：

| 日志里出现 | 说明 | 怎么办 |
|---|---|---|
| `[audio_cap] media_recorder_open failed` | 麦源没注册（根因） | 见第 2 节 |
| `[audio_cap] opened (16000Hz 1ch 16bit)` + `capture started` 但喊了没反应 | 音频通了，问题在 ASR/Key/唤醒词 | 查 `ASR connected` 是否出现；确认 `set_volc_asr` 三参正确；确认唤醒词是「你好, openvela」 |
| 完全没 `[audio_cap]` 日志 | voice_start 没真正起 | 确认 `CONFIG_AI_AGENT_AUDIO=y`、AI Agent 已编入；`voice_start` 返回是否报错 |
| `servo` / 表情有反应但语音没有 | ai_agent 没起或 Shell 策略拦了 | 确认 `EXAMPLES_AI_AGENT_VELA=y` 且 `SHELL_FULL=y`（否则 `servo` 命令被白名单拦） |

## 2. 修「麦源没注册」（根因）
需要把 INMP441 的 I2S 采集端点注册进 Vela 音频层，作为 `MEDIA_SOURCE_MIC`。
这一步依赖 Vela 音频服务（非 nuttx board 代码），注册范式参考 eye 板的
ES8311 接入方式。两个方向：

- **方向 A（推荐，最稳）**：在 Vela 音频服务的板级配置里，把 I2S0 设为
  默认 capture 设备并绑定 `MEDIA_SOURCE_MIC`（照搬 eye 板 ES8311 的接入代码，
  把 codec 换成 INMP441 的裸 I2S capture）。
- **方向 B（兜底）**：若音频层暂时接不通，先走 **文件/网络测试通道**验证
  整条链路：用 `voice_test_asr <pcm文件>` 把一段录好的「你好 openvela」PCM
  直接喂给 ASR→LLM→`servo` 命令，确认语音之后的链路（ASR/LLM/Skill/舵机）
  是通的，再回头攻麦源注册。（`voice_test_asr` 命令见 `packages_ai_agent/docs/channels.md`）

## 3. 已修的两个增益/配置坑
- `CONFIG_AI_AGENT_AUDIO_CAPTURE_GAIN=1`（默认 6 是 eye 6 路 DMIC 补偿，
  单路 INMP441 会被放大 6 倍削波，唤醒率暴跌）→ 已在 `board/t-display-s3/defconfig` 设好。
- `set_volc_asr` 必须给 **app_id + access_token + cluster** 三个参数，
  只设 `set_volc_key` 只能 TTS，不能唤醒。

## 4. 接线复核（喊了没用的物理原因）
- INMP441：SCK→GPIO2、WS→GPIO1、SD→GPIO16、VCC→3.3V、GND→共地。
- 麦的正反面/方向：INMP441 有丝印面朝外；焊反了没声音。
- 喊的时候离麦 10~20cm，正常音量；环境太吵 ASR 会拒识。
