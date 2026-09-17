# 语音唤醒操作手册

> 「你好，openvela」→ LLM 理解 → 表情/舵机/天气/简报 ——最短路径跑通指南

---

## 0. 前置条件

- [x] 板子烧了 openvela 固件（`esp32s3-eye` 母本或自建 `t-display-s3`）
- [x] 串口能进 `vela>` 提示符（默认 `115200 8N1`）
- [x] 板子连上 WiFi（能 `ping baidu.com`）
- [x] 已注册火山引擎账号，开通「**大模型推理（豆包）**」+「**语音技术（流式）**」

> **没开火山引擎？** 5 分钟搞定：
> 1. 打开 https://console.volcengine.com/ 注册
> 2. 顶部菜单 → "大模型服务" → 开通 "豆包" → 创建 API Key（形如 `sk-xxx`）
> 3. 顶部菜单 → "语音技术" → 开通 "流式语音识别" → 创建应用 → 拿 `App ID` + `Access Token`

---

## 1. 部署 Skills

把 `skills/` 整个目录复制到板子 `/data/agent/skills/`：

```bash
# 板子开 SSH 后 scp（推荐）
scp -r skills/ root@192.168.0.123:/data/agent/

# 验证
ssh root@192.168.0.123 "ls /data/agent/skills/"
# 应该看到：daily-briefing  face-expression  servo-control  weather
```

> ai_agent 启动时会**自动扫描**这个目录，无需注册。

---

## 2. 填入 Key

```bash
# 上传 .env
scp config/.env.example root@192.168.0.123:/data/.env

# 编辑填 Key
ssh root@192.168.0.123 "vi /data/.env"
```

至少填这三个：

```bash
VOLC_API_KEY=sk-xxxxxxxxxxxxxxxxxxxxxxxxxxxxx
VOLC_ASR_APP_ID=1234567890
VOLC_ASR_ACCESS_TOKEN=xxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

---

## 3. 进 `vela>` 配 Key

```bash
# 接串口（Windows 用 mobaxterm / putty / VSCode Serial Monitor）
minicom -D /dev/ttyUSB0 -b 115200
# 或 telnet（如果板子开了）
telnet 192.168.0.123 2323
```

在 `vela>` 提示符下：

```bash
vela> set_volc_key sk-xxxxxxxxxxxxxx
[ai_agent] volc key set, len=42

vela> set_volc_asr 1234567890 xxxxxxxxxxxxxxxx volcengine_streaming_common
[ai_agent] volc asr configured

vela> voice_start
[voice] initializing i2s mic on GPIO1/2/16...
[voice] connecting to volcengine ASR...
[voice] ready, say "你好，openvela" to wake
```

---

## 4. 说话测试

```
你: 你好，openvela
板子: 滴 - 唤醒（播放提示音 + 表情变"思考"）
你: 把表情换成开心
板子: 好的，已切换到开心表情。
     [face-expression] set to happy (id=1)
你: 舵机转到 90 度
板子: 好的，舵机已转到 90 度。
     [servo-control] action=set_angle angle=90
你: 让舵机摇个头
板子: 好的，开始摆头。
     [servo-control] action=sweep speed=slow
你: 今天天气怎么样
板子: 让我看看。
     [weather] query=current city=auto
     今天北京晴，22°C，适合出门。☀️
```

---

## 5. 调试命令

| 命令 | 作用 |
|---|---|
| `vela> help` | 列出所有命令 |
| `vela> skill list` | 列出已加载的 Skill |
| `vela> skill show face-expression` | 查看 face-expression 详情 |
| `vela> chat 你好` | 直接文字测试 LLM（不走语音） |
| `vela> voice_start` / `voice_stop` | 启停语音监听 |
| `vela> set_verbose 1` | 打开调试日志 |
| `vela> face set happy` | 直接测试 face Skill（不经过 LLM） |
| `vela> servo set 90` | 直接测试舵机 Skill |
| `vela> weather now` | 直接测试天气 |
| `vela> briefing now` | 立即触发每日简报 |

---

## 6. 一键测试

如果你懒得上文一步步来，把 `config/voice_wake_test.sh` 传到板子，跑：

```bash
scp config/voice_wake_test.sh root@192.168.0.123:/data/
ssh root@192.168.0.123 "sh /data/voice_wake_test.sh"
```

会跑 6 步自动检查，最后输出"准备就绪/有 N 项失败"。

---

## 7. 开机自启（可选）

装上 `config/boot_setup.sh` 实现"上电即用"：

```bash
scp config/boot_setup.sh root@192.168.0.123:/data/
ssh root@192.168.0.123 "chmod +x /data/boot_setup.sh && \
                        ln -sf /data/boot_setup.sh /etc/init.d/S99desktop_pet && \
                        sync && reboot"
```

下次重启，板子自动：
1. 等待 WiFi 联网（30s 超时）
2. 加载 `/data/.env`
3. 检查 Skills 目录
4. 把 `set_volc_key` / `set_volc_asr` 命令排队，等 ai_agent 启动后消费
5. 启动 `desktop_pet` 守护进程
6. 15s 后入队 `voice_start`

---

## 8. 常见问题

### Q1: 板子说"未找到 Skill"
- 检查 `/data/agent/skills/<name>/SKILL.md` 是否存在
- 文件名大小写敏感
- ai_agent 只识别目录名（不带 SKILL.md 后缀）

### Q2: 唤醒词不灵
- 确认用「**你好，openvela**」原文（含逗号）
- 检查 `voice_start` 后日志是否有 `ASR connected`
- 火山引擎 ASR 是流式的，需要稳定 100ms+ 语音输入
- 麦克风硬件：ESP32-S3 + INMP441（GPIO1/2/16）

### Q3: LLM 回复"我不知道" / 工具调用失败
- `vela> set_verbose 1` 打开调试
- 看 `/tmp/ai_agent.log` 里 LLM 调工具的返回
- 可能是 Skill 描述里没写清楚触发词，LLM 理解不了

### Q4: 舵机不转 / 表情不变
- `vela> face set happy` / `vela> servo set 90` 直接测试
- 不经过 LLM 也不响应 → 板子硬件问题（看 `dmesg`）
- 经过 LLM 失败但直接调成功 → Skill 注册问题

### Q5: ASR 一直说"网络异常"
- 检查 `ping 8.8.8.8` 通不通
- 火山引擎域名：`openspeech.bytedance.com`（需 DNS 解析）
- 部分 WiFi 路由器屏蔽外网 DNS → 配 `8.8.8.8` 作为备用

### Q6: 怎么切换 LLM 后端
```bash
vela> set_llm deepseek    # 切到 DeepSeek
vela> set_llm qwen        # 切到 Qwen
vela> set_llm kimi        # 切到 Kimi
vela> set_llm glm         # 切到 GLM
vela> set_llm volc        # 默认（豆包）
```

---

## 9. 下一步

- [ ] 跑 `docs/DEMO_VIDEO_SCRIPT.md` 录 5 分钟演示视频
- [ ] 整理 `docs/SUBMISSION_GUIDE.md` 准备提交
- [ ] 累积 `logs/AI_DEV_LOG.md`（用 Claude Code / AIoT-IDE 等白名单工具）
- [ ] 烧真机 → 录视频 → 提交 PR
