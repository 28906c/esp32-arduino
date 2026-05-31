# build.ps1 - Build shengyu_tongxing_v2 firmware
param(
    [string]$Port = ""
)

$ErrorActionPreference = "Stop"

Write-Host "=== 声语同行 v2 - Build ===" -ForegroundColor Cyan

idf.py set-target esp32p4
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

idf.py build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Build completed successfully." -ForegroundColor Green
