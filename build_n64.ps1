# PowerShell build launcher for Windows using WSL + libdragon
$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptDir

Write-Host "Iniciando compilação do X-Men 2: Clone Wars (N64) via WSL..." -ForegroundColor Cyan
wsl -e bash -c "cd `$(wslpath '$scriptDir') && ./build_n64.sh"
if ($LASTEXITCODE -eq 0) {
    Write-Host "Compilação concluída com sucesso! ROM gerada: xmen2_heavy_60_opt.z64" -ForegroundColor Green
} else {
    Write-Host "Falha na compilação. Código de saída: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}

