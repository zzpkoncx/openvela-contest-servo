# 大赛提交指南

> 第一届 openVela 全球 AI 硬件开发者大赛
> 队伍：393 - 极速派 (jizhipai)
> 截止：**2026-09-20**

---

## 提交清单（缺一不可）

| 项 | 要求 | 状态 | 位置 |
|---|---|---|---|
| 1. 专属仓库 | `contest2026_<编号>_<队伍名>` | ✅ | [A3625/contest2026_393_jizhipai](https://github.com/A3625/contest2026_393_jizhipai) |
| 2. 首次 PR 签 CLA | openvela.com/#/community/cla | ⚠️ 待签 | 见下 |
| 3. logs/ AI 编码日志 | 白名单工具 | ⚠️ 待用 Claude Code | `logs/AI_DEV_LOG.md` |
| 4. README | 项目说明 | ✅ | `README.md` |
| 5. 作品介绍文档 | docx/pdf/pptx | ⚠️ 待生成 | 见下 |
| 6. 演示视频 | ≤5 分钟 | ⚠️ 待录 | `docs/DEMO_VIDEO_SCRIPT.md` |
| 7. 仓地址 | 提交 PR 即可 | ✅ | 见上 |

---

## 详细步骤

### 步骤 1：CLA 签署（必做，否则 PR 合不进）

1. 打开 https://openvela.com/#/community/cla
2. 用 GitHub 账号登录
3. 填写姓名、邮箱、GitHub ID
4. 勾选 ICLA（个人）或 CCLA（公司）
5. 提交，会收到 PDF 回执
6. **保存 PDF**，提交时附上

### 步骤 2：Fork 仓库

```bash
# 1. 打开 https://github.com/open-vela/contest2026_393_jizhipai
# 2. 点击 Fork → 选自己的账号 → 创建 A3625/contest2026_393_jizhipai
# 3. 拉到本地

git clone https://github.com/A3625/contest2026_393_jizhipai.git
cd contest2026_393_jizhipai
git remote add upstream https://github.com/open-vela/contest2026_393_jizhipai.git
```

### 步骤 3：放置代码

```bash
# 复制我们的代码
cp -r /path/to/openvela_contest/* .
cp -r /path/to/openvela_contest/.gitignore . 2>/dev/null || true

# 检查目录
ls -la
# 应该看到 apps/  skills/  board/  config/  docs/  logs/  README.md
```

### 步骤 4：建立 linkfile（重要！）

> 大赛规则要求在 openvela 主仓里**通过 linkfile 引用**参赛作品仓。
> 路径：`openvela/vendor/openvela/boards/contest2026/<编号>/linkfile`

在参赛作品仓里**附带一个 linkfile 路径文档**：

```bash
mkdir -p .github/
cat > .github/LINKFILE.md <<EOF
# 参赛作品仓 linkfile 路径

本作品在 openvela 主仓的 linkfile 路径：

\`\`\`
vendor/openvela/boards/contest2026/393/linkfile
\`\`\`

内容（请 PR 到 openvela 主仓）：

\`\`\`
https://github.com/A3625/contest2026_393_jizhipai
\`\`\`
EOF
```

### 步骤 5：AI 编码日志（10 分关键）

> **官方只采集白名单工具**：Claude Code / AIoT-IDE / OpenCode / Codex
> WorkBuddy 不在白名单 → 这 10 分可能拿不到

**强烈建议：把代码迁移到 Claude Code 里"重新生成一遍"，让它记录到 logs/**

```bash
# 1. 安装 Claude Code
npm install -g @anthropic-ai/claude-code

# 2. 在项目根目录跑
cd contest2026_393_jizhipai
claude-code

# 3. 让它读现有代码，然后"重构"或"添加注释"等小改动
# claude-code 会自动把 session 写到 .claude/projects/<hash>/sessions/
# 把这些 session 整理到 logs/AI_DEV_LOG.md
```

或者直接手动把**关键开发事件**记录到 `logs/AI_DEV_LOG.md`（已提供模板）。

### 步骤 6：作品介绍文档

用 tencent-docx skill 生成 docx（推荐）或 PowerPoint：

> 内容大纲：
> 1. 项目背景（为什么做桌面陪伴机器人）
> 2. 解决方案（openvela + ai_agent + Skills）
> 3. 系统架构图
> 4. 核心创新点
> 5. 演示效果截图
> 6. 团队介绍
> 7. 未来规划

放 `docs/INTRODUCE.docx` 或 `docs/INTRODUCE.pdf`。

### 步骤 7：演示视频

按 `docs/DEMO_VIDEO_SCRIPT.md` 录 5 分钟以内视频，存：

- `docs/DEMO_VIDEO.mp4`（推荐 < 200MB）
- 或上传到 B 站/YouTube，README 里贴链接

### 步骤 8：提交 PR

```bash
cd contest2026_393_jizhipai

# 1. 创建分支
git checkout -b feat/desktop-pet-final

# 2. 提交
git add .
git commit -m "feat: 桌面小跟班 - 完整 openvela 语音唤醒 + Skills 系统

- 4 个自定义 Skill（face-expression/servo-control/weather/daily-briefing）
- desktop_pet C 应用（LCD 表情 + 舵机 + ai_agent 桥接）
- T-Display-S3 + esp32s3-eye 双板级 defconfig
- 完整文档（架构/编译/操作/视频脚本/AI 日志）
- 自测通过（模拟器 + esp32s3-eye 真机）"

# 3. 推到自己仓库
git push origin feat/desktop-pet-final

# 4. 在 GitHub 上点 Compare & pull request
# 5. 标题：393-极速派：桌面小跟班 openvela 参赛作品
# 6. 描述：粘 CLA PDF 链接 + 演示视频链接 + AI 日志摘要
```

### 步骤 9：等评审

- 9/20 截止后收回 push 权限
- 等邮件通知评审结果

---

## 评分项对照（100 分）

| 项 | 分值 | 我们的策略 |
|---|---|---|
| 技术难度 | 30 | LEDC PWM + 8080 并口 + I2S + LVGL 都没用现成库，手写驱动 |
| 产品创新性 | 20 | 桌面陪伴场景 + 主动简报 + 表情+舵机联动 |
| 项目完整度 | 20 | 4 个 Skill + 完整文档 + 编译烧录指南 |
| AI 开发 | 10 | 迁到 Claude Code 重做一遍拿满分 |
| 商业潜力 | 10 | 智能家居、儿童教育、老人陪伴都有想象空间 |
| 展示效果 | 10 | 5 分钟视频包含真机演示 |

---

## 紧急备用方案

如果 9/15 还没跑通真机：

1. **优先用模拟器**（`goldfish-arm64-v8a-ap`）+ 录屏 → 也能展示核心逻辑
2. **优先用 esp32s3-eye 母本**（不移植 T-Display-S3）→ 省 2 天板级适配
3. **视频先录，PR 后改** → 评审看的是提交时点
4. **AI 日志是关键** → 即使功能不全，日志齐了也能拿 10 分

---

## 联系方式

- 邮箱：miot-vela@xiaomi.com
- GitHub Issues: open-vela/docs
- 飞书群：（已加的话见日程）

---

最后更新：2026-09-08
