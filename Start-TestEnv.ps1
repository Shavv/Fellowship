param(
    [int]$TestDuration = 60
)

$ErrorActionPreference = "Continue"

$ProjectDir  = $PSScriptRoot
$SkyrimDir   = "F:\SteamLibrary\steamapps\common\Skyrim Special Edition"
$Loader      = Join-Path $SkyrimDir "skse64_loader.exe"
$PluginSrc   = Join-Path $ProjectDir "client\build\Release\Fellowship.dll"
$PluginDest  = Join-Path $SkyrimDir "Data\SKSE\Plugins\Fellowship.dll"
$BuildDir    = Join-Path $ProjectDir "client\build"
$ServerDir   = Join-Path $ProjectDir "server"

Write-Host "--- Fellowship TEST CYCLE ---" -ForegroundColor Cyan

# 0. Cleanup
Write-Host "[0/4] Cleaning up existing instances..." -ForegroundColor Yellow
& (Join-Path $ProjectDir "Stop-TestEnv.ps1")

try {
    # 1. Build & deploy plugin
    Write-Host "[1/4] Rebuilding Plugin..." -ForegroundColor Yellow
    Set-Location $BuildDir
    & cmake --build . --config Release --target Fellowship
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    Copy-Item $PluginSrc $PluginDest -Force
    Write-Host "Plugin deployed: $((Get-Item $PluginDest).LastWriteTime)" -ForegroundColor Gray

    # 2. Start server
    Write-Host "[2/4] Starting Node.js Server..." -ForegroundColor Yellow
    Start-Process -FilePath "node" -ArgumentList "index.js" -WorkingDirectory $ServerDir -WindowStyle Minimized
    Start-Sleep -Seconds 1

    # 3. Launch both instances
    # skse.ini has AllowMultipleInstances=1 — SKSE handles the rest
    Write-Host "[3/4] Launching Client 1..." -ForegroundColor Green
    Start-Process -FilePath $Loader -WorkingDirectory $SkyrimDir

    Start-Sleep -Seconds 3

    Write-Host "[3/4] Launching Client 2..." -ForegroundColor Green
    Start-Process -FilePath $Loader -WorkingDirectory $SkyrimDir

    # 4. Verify both are running
    Write-Host "[4/4] Waiting for both instances..." -ForegroundColor Yellow
    $Verified = $false
    for ($i = 0; $i -lt 60; $i++) {
        $procs = Get-Process -Name SkyrimSE -ErrorAction SilentlyContinue
        if ($procs.Count -ge 2) {
            Write-Host "SUCCESS: 2 instances running (PIDs: $($procs.Id -join ', '))" -ForegroundColor Green
            $Verified = $true
            break
        }
        Write-Host "  Waiting... ($i/60) — currently $($procs.Count) instance(s)" -ForegroundColor Gray
        Start-Sleep -Seconds 1
    }

    if (-not $Verified) {
        throw "FAILED: Could not detect 2 SkyrimSE.exe instances after 60s."
    }

    Write-Host "Test running for $TestDuration seconds..." -ForegroundColor Magenta
    for ($i = $TestDuration; $i -gt 0; $i--) {
        Write-Progress -Activity "Fellowship Test" -Status "Closing in $i seconds..." -PercentComplete (($TestDuration - $i) / $TestDuration * 100)
        Start-Sleep -Seconds 1
    }

    Write-Host "Test finished. Shutting down..." -ForegroundColor Yellow
    & (Join-Path $ProjectDir "Stop-TestEnv.ps1")

} catch {
    Write-Error "Error: $_"
    & (Join-Path $ProjectDir "Stop-TestEnv.ps1")
}

Set-Location $ProjectDir
Write-Host "Done."
