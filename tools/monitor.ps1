# monitor.ps1 - Monitor shengyu_tongxing_v2 serial output
param(
    [string]$Port = "COM3"
)

$ErrorActionPreference = "Stop"

Write-Host "=== 声语同行 v2 - Monitor ===" -ForegroundColor Cyan
Write-Host "Target port: $Port"
Write-Host "Press Ctrl+] to exit."
Write-Host ""

idf.py -p $Port monitor
