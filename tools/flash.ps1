# flash.ps1 - Flash shengyu_tongxing_v2 firmware
param(
    [string]$Port = "COM3"
)

$ErrorActionPreference = "Stop"

Write-Host "=== 声语同行 v2 - Flash ===" -ForegroundColor Cyan
Write-Host "Target port: $Port"

idf.py -p $Port flash
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Flash completed successfully." -ForegroundColor Green
