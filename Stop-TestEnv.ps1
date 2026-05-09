Write-Host "Stopping all SkyrimSE instances..." -ForegroundColor Yellow
# Use taskkill /F to force-kill even if access is denied to normal Stop-Process
taskkill /F /IM SkyrimSE.exe /T 2>$null
taskkill /F /IM SkyrimSE2.exe /T 2>$null
taskkill /F /IM SkyrimSE_Multi.exe /T 2>$null

Write-Host "Stopping Node.js server..." -ForegroundColor Yellow
Get-Process -Name node -ErrorAction SilentlyContinue | Where-Object { $_.CommandLine -match "index.js" } | Stop-Process -Force -ErrorAction SilentlyContinue

Write-Host "Environment stopped." -ForegroundColor Green
