# 静态一致性校验报告 — openvela 舵机固件（构建/烧录脚本与文档）

> 校验人：QA 工程师 严过关（software-qa-engineer）
> 方式：纯静态审查（Grep + Read + `bash -n` 语法检查），不运行编译。
> 基准：官方 openvela quickstart `dev-ai-contest-2026` 已核实事实。
> 受审文件：`tools/apply_and_build.sh`、`tools/flash_servo.sh`、`tools/flash_servo_win.ps1`、`docs/BUILD_AND_FLASH.md`、`docs/SERVO_VOICE.md`、`board/esp32s3-eye/defconfig.addition`、`board/t-display-s3/defconfig`

---

## 总判定：**PASS**（清单 A–G 全部通过）

| 项 | 结论 | 关键证据 |
|----|------|----------|
| A 路径正确性 | ✅ 通过 | 编译命令均用新路径 `vendor/espressif/.../esp32s3-eye/configs/nsh` |
| B 命令正确性 | ✅ 通过 | 无 `make`/`configure.sh`，统一 `./build.sh <cfg> --cmake` |
| C goldfish 仅模拟器 | ✅ 通过 | 文档明确区分 模拟器(goldfish,否) vs 真机(esp32s3-eye,是) |
| D 烧录偏移一致 | ✅ 通过 | 三处均为 0x0/0x8000/0x10000 或 merged 0x0，无 0x20000 |
| E servo GPIO 一致 | ✅ 通过 | 两 defconfig 与 SERVO_VOICE.md 均为 GPIO42/LCD关/FACE=n/LEDC ch0=42 |
| F app 复制目标 | ✅ 通过 | `apps/external/desktop_pet` 为第一优先 |
| G 脚本可执行性 | ✅ 通过 | `bash -n` 两脚本语法干净；ps1 解析通过 |

---

## A. 路径正确性 — 通过

**实际编译目标路径（全部正确，指向迁移后新路径）**
- `tools/apply_and_build.sh:108` — `EYE_BASE="$ROOT/vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/configs/nsh/defconfig"`
- `tools/apply_and_build.sh:112` — `find "$ROOT" -path '*esp32s3-eye*'` 回退（命中同一前缀）
- `tools/flash_servo.sh:74` — `./build.sh vendor/espressif/boards/xtensa/esp32s3/esp32s3-eye/configs/nsh --cmake -j$(nproc)`
- `docs/BUILD_AND_FLASH.md:58, :80, :82, :109`
- `docs/SERVO_VOICE.md:138, :139`

**坏字符串 grep 结果**
- `vendor/openvela/boards/lilygo/esp32s3-eye` → **0 命中**
- `vendor/openvela/boards/vela/configs/esp32s3-eye` → **0 命中**
- `boards/xtensa/esp32s3/esp32s3-eye`（旧路径）→ 仅出现在**合法说明性注释**中，且均明确标注"已失效/已迁移"，非命令用法：
  - `tools/apply_and_build.sh:13-14`（"boards/xtensa/... 迁移到 vendor/espressif/..."）
  - `docs/BUILD_AND_FLASH.md:86`（"旧路径 `boards/xtensa/esp32s3/esp32s3-eye` 已失效"）
  - `docs/BUILD_AND_FLASH.md:277`（"路径从 `boards/xtensa/...` 变为 `vendor/espressif/...`"）

**残留观察（不在 A 范围，非命令）**：`board/t-display-s3/defconfig:3-4` 注释仍写 `vendor/openvela/boards/lilygo/t-display-s3`、`boards/xtensa/esp32s3/t-display-s3`（针对 t-display-s3 自身，未作为命令使用，不影响功能）。属轻微过期注释，建议顺手更新，但不判失败。

---

## B. 命令正确性 — 通过

- `make -j$(nproc)` → **0 命中**
- `make menuconfig` → **0 命中**（仅 `logs/AI_DEV_LOG.md:56` 日志叙述"make menuconfig 没勾 NSH=y"为历史失误记录，不在受审 7 文件内，且非指令）
- `./tools/configure.sh` → **0 命中**

所有编译命令统一为 `./build.sh <cfg> --cmake`：
- `tools/apply_and_build.sh:151`（`./build.sh "$cfg" --cmake -j"$(nproc)"`）
- `tools/flash_servo.sh:74`、`docs/BUILD_AND_FLASH.md:57-58,:80,:82,:110`、`docs/SERVO_VOICE.md:139`
- `docs/BUILD_AND_FLASH.md:60,:279` 显式声明"不是 NuttX 的 make / 没有 configure.sh"
- `README.md:57`、`apps/desktop_pet/Make.defs` 中的 `Make.defs`/`Makefile` 为文件名，非 make 命令。

---

## C. goldfish 仅模拟器 — 通过

`docs/BUILD_AND_FLASH.md` 未将 goldfish 标成真机/esp32s3-eye 母本，明确区分：
- `:6` — "官方 quickstart **只编译 goldfish** 模拟器…整篇没有真机烧录步骤。真机 ESP32-S3 烧录是我们的扩展"
- `:51-58` 表格：goldfish 行"否（ARM64 软仿真）"，esp32s3-eye 行"是（真机）"
- `:62-73` 小标题"模拟器 goldfish（仅验证逻辑，不能替代真机）"；`:73` "不代表真机外设（PWM/GPIO42 舵机）可用"
- `:75` "2.2 真机 esp32s3-eye（烧 T-Display-S3）"
- emulator 命令 `./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap/`（`:69`）与官方事实一致。

---

## D. 烧录偏移一致 — 通过

三处偏移完全一致，且无虚构的 `0x20000 ai_agent_model.bin`：
- `tools/flash_servo.sh:90` merged→`0x0`；`:92-94` `0x0 bootloader`/`0x8000 partition`/`0x10000 nuttx`；无 0x20000
- `docs/BUILD_AND_FLASH.md:132-137` 表 `0x0/0x8000/0x10000`，merged `0x0`；`:139` 明确"⚠️ 没有 `0x20000 ai_agent_model.bin` 这一项"
- `tools/flash_servo_win.ps1:108` `0x0 boot / 0x8000 part / 0x10000 app`；`:106,:115` merged `0x0`；无 0x20000

---

## E. servo GPIO 一致 — 通过

| 符号 | `defconfig.addition` | `t-display-s3/defconfig` | `SERVO_VOICE.md` |
|------|----------------------|--------------------------|------------------|
| `CONFIG_APP_DESKTOP_PET_SERVO_GPIO` | `=42`（:49） | `=42`（:102） | GPIO42（多处）✅ |
| `CONFIG_ESP32S3_LEDC_CHANNEL0_PIN` | `=42`（:70） | `=42`（:101） | 42（:103,:87-88）✅ |
| LCD 关闭 | `CONFIG_LCD=n`（:58）+ `is not set`（:57） | `# CONFIG_LCD is not set`（:68，ST7789 全注释） | LCD=n（:5,:33,:54-58）✅ |
| FACE 关闭 | `=n`（:41,:59） | `=n`（:139） | FACE=n（:39）✅ |
| LEDC TIM0 | `CONFIG_ESP32S3_LEDC_TIM0=y`（:66） | `=y`（:42） | TIM0 非 TIMER0（:100-103）✅ |

三者一致：servo=GPIO42、LCD 关、FACE=n、LEDC ch0=42、TIM0 正确（非错误符号 TIMER0）。

---

## F. app 复制目标 — 通过

`tools/apply_and_build.sh:79-88` 候选顺序：
```
"$ROOT/apps/external/desktop_pet"   # 第一优先
"$ROOT/apps/desktop_pet"
"$ROOT/nuttx-apps/examples/desktop_pet"
```
`:84-87` 取第一个可写目标（external 优先，父目录不存在则创建后使用），`:91` 复制。`:73-77` 注释明确 external 机制优先（旧脚本顺序颠倒导致 app 漏编的 bug 已修正）。

---

## G. 脚本可执行性 — 通过

- `tools/apply_and_build.sh:1` `#!/usr/bin/env bash` shebang 完整；`bash -n` 语法检查通过（无未闭合引号、无语法错）。
- `tools/flash_servo.sh:1` shebang 完整；`bash -n` 通过（含 `mapfile`/进程替换，需 bash，shebang 正确）。
- `tools/flash_servo_win.ps1` `param(...)` 块完整、函数闭合，`[ScriptBlock]::Create()` 解析通过。

> 注：本沙箱 Bash 工具因 PATH 损坏（`ls`/`dirname` not found）无法直接执行，已改用 PowerShell 调用 PortableGit 自带 `bash.exe` 完成 `bash -n` 验证。

---

## 残留坏字符串（工程师漏改项）

经全仓库 grep，**受审 7 文件中未发现清单所列坏字符串作为实际命令/路径使用**。旧路径 `boards/xtensa/esp32s3/esp32s3-eye` 仅以"已失效"注释出现，属正确提示。
唯一轻微残留（非 esp32s3-eye、非命令，不影响功能）：`board/t-display-s3/defconfig:3-4` 注释里的旧式 t-display-s3 路径，建议顺手更新为 `vendor/espressif/boards/xtensa/esp32s3/t-display-s3/`。

---

## 结论与遗留风险（给主理人）

**结论**：静态一致性 PASS。脚本与文档已与官方 openvela quickstart（`dev-ai-contest-2026`）在路径、编译命令、模拟器/真机区分、烧录偏移、servo GPIO 五个维度对齐，相互之间无矛盾；两个 shell 脚本语法干净、powershell 脚本可解析。上一轮工程师对坏路径/坏命令的修正均已落地，未发现残留的 `vendor/openvela/.../esp32s3-eye` 或 `make`/`configure.sh` 类坏字符串。

**需诚实标注、静态审查无法确认、须真机首次编译验证的点**（不判失败，但为真实风险）：
1. **nsh 基 config 是否自带 ai_agent 依赖**：`defconfig.addition` 已显式补 `CONFIG_AI_AGENT_*` 与 `CONFIG_EXAMPLES_AI_AGENT_VELA`，但 `vendor/espressif` 下 `esp32s3-eye` 的 `nsh` 基 config 是否真能在树内拉到 `packages_ai_agent`、以及 `CONFIG_BOARD_LATE_INITIALIZE` 在该基 config 中是否已开（决定 `/dev/pwm0` 能否注册），只能首次真机编译 + 烧录后 `ls /dev` 确认。
2. **goldfish 编译 config 路径未经官方逐字核对**：`BUILD_AND_FLASH.md:67` 使用 `vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap/`，文档已用"形如"兜底；建议对照官方 quickstart 原文确认该路径字符串。
3. **命名板 t-display-s3 为 best-effort 备选**：`apply_and_build.sh` 仅做 eye→t-display-s3 整目录复制，并需在 menuconfig 手动注册 `BOARD_T_DISPLAY_S3`；正式真机固件走的是 `esp32s3-eye nsh` 母本，命名板路径非主路径。
4. **LCD 关闭依赖追加生效顺序**：若 eye `nsh` 基 config 已内联 `CONFIG_LCD=y`，则追加的 `CONFIG_LCD=n` 因 defconfig 后写优先而覆盖（逻辑成立），但建议首次编译后 grep 产物 defconfig 确认 LCD 确为 n、FACE 确为 n。
5. **Shell Full 模式须终确认**：`CONFIG_EXAMPLES_AI_AGENT_VELA_SHELL_FULL=y` 已写，双命名空间（`CONFIG_AI_AGENT` vs `CONFIG_EXAMPLES_AI_AGENT_VELA`）冗余已文档化、无害；但语音能否真正调 `servo` 取决于该策略在 menuconfig 中确实生效，只能运行时验证。

总体：交付物静态质量合格，可进入真机编译/烧录阶段；上述 5 点为首次真机验证时必须逐一拍实的不确定项。
