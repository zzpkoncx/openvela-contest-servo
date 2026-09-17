# flash_servo_win.ps1
# ============================================================
# Flash openvela servo/voice firmware to T-Display-S3 (ESP32-S3) on Windows.
# Board: COM7 (VID_303A&PID_1001, ESP32-S3, 8MB PSRAM, 16MB Flash)
# Tool: local venv esptool 5.4.0
#
# Usage:
#   .\flash_servo_win.ps1                 # preview only (no flash)
#   .\flash_servo_win.ps1 -Go            # actually flash
#   .\flash_servo_win.ps1 -Go -Backup    # backup factory firmware first (read 16MB)
#   .\flash_servo_win.ps1 -BinDir D:\firmware -Port COM7
#
# NOTE: This script only flashes. The firmware (bin) must be built first on
# the Ubuntu build machine via apply_and_build.sh, then copied back to Windows.
# ============================================================
param(
  [string]$Port   = "COM7",
  [string]$BinDir = "",
  [int]   $Baud   = 115200,
  [switch]$Go,
  [switch]$Backup
)

# 候选 Python 解释器 (按优先级): 受管 venv -> 受管版本 python
$venvPy = "C:\Users\admin\.workbuddy\binaries\python\envs\default\Scripts\python.exe"
$verPy  = "C:\Users\admin\.workbuddy\binaries\python\versions\3.13.12\python.exe"

# 选一个能跑 esptool 的解释器; 都没有则回退到 PATH 上的 esptool / esptool.py
function Resolve-EspPy {
  foreach ($p in @($venvPy, $verPy)) {
    if (Test-Path $p) { return $p }
  }
  return $null
}

$espPy = Resolve-EspPy

function Run-Esp {
  param([string[]]$EspArgs)
  $toolArgs = @("--chip", "esp32s3", "--port", $Port, "--baud", $Baud) + $EspArgs
  if ($espPy) {
    & $espPy -m esptool @toolArgs
  } elseif (Get-Command esptool -ErrorAction SilentlyContinue) {
    & esptool @toolArgs
  } elseif (Get-Command esptool.py -ErrorAction SilentlyContinue) {
    & esptool.py @toolArgs
  } else {
    Write-Host "ERROR: 找不到 esptool。请先安装: pip install esptool" -ForegroundColor Red
    Write-Host "  (建议装到受管 venv: & '$venvPy' -m pip install esptool)" -ForegroundColor Yellow
    exit 1
  }
}

# ---- locate firmware dir ----
if ($BinDir -eq "") {
  $cands = @(
    (Split-Path -Parent $MyInvocation.MyCommand.Path),
    "C:\Users\admin\Desktop\lk\openvela_contest\out",
    "C:\Users\admin\Desktop\lk\cmake_out"
  )
  foreach ($c in $cands) {
    if (Test-Path $c) { $BinDir = $c; break }
  }
}
if (-not (Test-Path $BinDir)) {
  Write-Host "ERROR: firmware dir not found: $BinDir" -ForegroundColor Red
  Write-Host "Use -BinDir to point at the dir containing bins copied from the build machine." -ForegroundColor Yellow
  exit 1
}
Write-Host "Firmware dir: $BinDir"

# ---- find bins recursively ----
$merged = Get-ChildItem -Path $BinDir -Recurse -Filter "merged*.bin"          -ErrorAction SilentlyContinue | Select-Object -First 1
$boot   = Get-ChildItem -Path $BinDir -Recurse -Filter "bootloader*.bin"      -ErrorAction SilentlyContinue | Select-Object -First 1
$part   = Get-ChildItem -Path $BinDir -Recurse -Filter "partition-table*.bin" -ErrorAction SilentlyContinue | Select-Object -First 1
$app    = Get-ChildItem -Path $BinDir -Recurse -ErrorAction SilentlyContinue |
          Where-Object { $_.Name -match '^(nuttx|app|ota|vela_esp32).*\.bin$' } | Select-Object -First 1

$useMerged = $false
if ($merged) {
  Write-Host "Found merged firmware: $($merged.FullName)"
  $useMerged = $true
} elseif ($boot -and $part -and $app) {
  Write-Host "Found split firmware:"
  Write-Host "  bootloader : $($boot.FullName)"
  Write-Host "  partition  : $($part.FullName)"
  Write-Host "  app        : $($app.FullName)"
} else {
  Write-Host "ERROR: no flashable firmware in $BinDir (need merged*.bin or bootloader+partition+app)." -ForegroundColor Red
  Write-Host "Copy the build-machine bins here, or use -BinDir." -ForegroundColor Yellow
  exit 1
}

# ---- optional backup ----
if ($Backup) {
  $bk = "C:\Users\admin\Desktop\lk\backup_$(Get-Date -Format yyyyMMdd_HHmmss).bin"
  Write-Host "Backing up factory firmware -> $bk (read 16MB, slow)" -ForegroundColor Yellow
  if ($Go) { Run-Esp read_flash 0x0 0x1000000 $bk }
  else { Write-Host "  [preview] read_flash 0x0 0x1000000 $bk" }
}

# ---- run or preview ----
if (-not $Go) {
  Write-Host "`n[PREVIEW] add -Go to actually flash. Will run:" -ForegroundColor Cyan
  if ($useMerged) {
    Write-Host "  write_flash -z 0x0 $($merged.FullName)"
  } else {
    Write-Host "  write_flash -z 0x0 $($boot.FullName) 0x8000 $($part.FullName) 0x10000 $($app.FullName)"
  }
  exit 0
}

Write-Host "`n[FLASH] running..." -ForegroundColor Green
if ($useMerged) {
  Run-Esp write_flash -z 0x0 $merged.FullName
} else {
  Run-Esp write_flash -z 0x0 $boot.FullName 0x8000 $part.FullName 0x10000 $app.FullName
}
Write-Host "`nDone. Press RST on board; serial 115200 to see boot log; then:" -ForegroundColor Green
Write-Host "  ls /dev            # confirm pwm0 exists" -ForegroundColor White
Write-Host "  servo set 90       # check 270-deg servo turns to 90" -ForegroundColor White
Write-Host "  servo sweep slow   # head shake" -ForegroundColor White
