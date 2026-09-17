# ===========================================================================
# install_skill_win.ps1 —— Windows 下一键把 Skill 推到真机 /data/agent/skills/
# ===========================================================================
# 用法（PowerShell，管理员或非管理员均可；板子已 USB 连接且 adb 已授权）：
#   powershell -ExecutionPolicy Bypass -File .\tools\install_skill_win.ps1
#   # 跳过密钥：  ... -NoEnv
# 参数：
#   -NoEnv    只推 skills，不推 .env
#   -Adb <path>  指定 adb.exe 路径（默认从 PATH 取）
# ===========================================================================
param(
  [switch]$NoEnv,
  [string]$Adb = "adb"
)

$ErrorActionPreference = "Stop"
$HERE = Split-Path -Parent $MyInvocation.MyCommand.Definition
$ROOT = Resolve-Path (Join-Path $HERE "..")
$SKILL_SRC = Join-Path $ROOT "skills"
$ENV_SRC   = Join-Path $ROOT ".env"
$DEST      = "/data/agent/skills"

# 自动发现 adb（winget 装的 Platform-Tools 默认不进 PATH，需搜常见路径）
$Adb = "adb"
$adbCands = @(
  "adb",
  "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
  "$env:ProgramFiles\Android\Sdk\platform-tools\adb.exe",
  "C:\Android\Sdk\platform-tools\adb.exe"
)
foreach ($a in $adbCands) {
  if (Get-Command $a -ErrorAction SilentlyContinue) { $Adb = $a; break }
}

# 检查 adb
try {
  & $Adb devices | Out-Null
} catch {
  Write-Error "找不到 adb.exe。请先安装：winget install -e --id Google.PlatformTools （或 -Adb 指定路径）"
  exit 1
}

Write-Host "==> 设备列表："
& $Adb devices

Write-Host "==> 在板子创建 $DEST ..."
& $Adb shell "mkdir -p $DEST"

Write-Host "==> 推送 skills/ 下所有 Skill ..."
$skills = Get-ChildItem -Path $SKILL_SRC -Directory
foreach ($s in $skills) {
  $skillMd = Join-Path $s.FullName "SKILL.md"
  if (Test-Path $skillMd) {
    Write-Host "    -> $($s.Name)"
    & $Adb push $s.FullName $DEST/
  }
}

if (-not $NoEnv -and (Test-Path $ENV_SRC)) {
  Write-Host "==> 推送 .env（火山引擎密钥）到 /data/.env ..."
  & $Adb push $ENV_SRC /data/.env
} else {
  Write-Host "==> 跳过 .env（用 -NoEnv 或缺少 .env 文件）"
}

Write-Host ""
Write-Host "==> 完成。在板子串口执行以下命令让 ai_agent 重新扫描 Skill 并启动语音："
Write-Host "    vela> voice_stop"
Write-Host "    vela> voice_start"
Write-Host "    vela> set_volc_asr"
Write-Host "然后说：『你好，openvela，舵机往左转』"
