$ProjectDir  = $PSScriptRoot
$SkyrimDir   = "F:\SteamLibrary\steamapps\common\Skyrim Special Edition"
$PluginSrc   = Join-Path $ProjectDir "client\build\Release\Fellowship.dll"
$PluginDest  = Join-Path $SkyrimDir "Data\SKSE\Plugins\Fellowship.dll"
$BuildDir    = Join-Path $ProjectDir "client\build"

Write-Host "--- Fellowship DEPLOY ---" -ForegroundColor Cyan

# 0. Cleanup Skyrim processes first to avoid file locks
Write-Host "Checking for Skyrim processes..." -ForegroundColor Yellow
taskkill /f /im SkyrimSE.exe /fi "status eq running" 2>$null
taskkill /f /im skse64_loader.exe /fi "status eq running" 2>$null

try {
    # 1. Build plugin
    Write-Host "[1/2] Rebuilding Plugin..." -ForegroundColor Yellow
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }
    
    Set-Location $BuildDir
    if (-not (Test-Path "CMakeCache.txt")) {
        Write-Host "Configuring CMake..." -ForegroundColor Yellow
        & cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="../vcpkg/scripts/buildsystems/vcpkg.cmake"
        if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
    }
    
    Write-Host "Building plugin..." -ForegroundColor Yellow
    & cmake --build . --config Release --target Fellowship
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }

    # 2. Deploy plugin
    Write-Host "[2/2] Deploying to Skyrim..." -ForegroundColor Yellow
    if (-not (Test-Path (Split-Path $PluginDest))) {
        New-Item -ItemType Directory -Path (Split-Path $PluginDest) -Force | Out-Null
    }
    
    # Check for Address Library
    $AddrLib_Ver = "1-6-1170-0"
    $AddrLib1 = Join-Path (Split-Path $PluginDest) "versionlib-$AddrLib_Ver.bin"
    $AddrLib2 = Join-Path (Split-Path $PluginDest) "version-$AddrLib_Ver.bin"
    
    if ((Test-Path $AddrLib2) -and -not (Test-Path $AddrLib1)) {
        Write-Host "Creating versionlib link for CommonLibSSE-NG..." -ForegroundColor Yellow
        Copy-Item $AddrLib2 $AddrLib1 -Force
    } elseif ((Test-Path $AddrLib1) -and -not (Test-Path $AddrLib2)) {
        Write-Host "Creating standard version link..." -ForegroundColor Yellow
        Copy-Item $AddrLib1 $AddrLib2 -Force
    }
    
    if (-not (Test-Path $AddrLib1) -and -not (Test-Path $AddrLib2)) {
        Write-Warning "Address Library ($AddrLib2) NOT found in Plugins folder!"
    }

    Copy-Item $PluginSrc $PluginDest -Force
    
    # Copy dependency DLLs to Skyrim root
    Write-Host "Copying dependency DLLs to Skyrim root..." -ForegroundColor Yellow
    Get-ChildItem -Path (Split-Path $PluginSrc) -Filter "*.dll" | Where-Object { $_.Name -ne "Fellowship.dll" } | ForEach-Object {
        $dest = Join-Path $SkyrimDir $_.Name
        Copy-Item $_.FullName $dest -Force
        Write-Host "  Deployed dependency: $($_.Name)"
    }

    Write-Host "Plugin and dependencies deployed successfully!" -ForegroundColor Green

} catch {
    Write-Error "Deployment failed: $_"
} finally {
    # 3. Handle Server
    Write-Host "Restarting Server to ensure window is visible..." -ForegroundColor Cyan
    
    # Try to close existing window by title
    $windowTitle = "Fellowship Server"
    Get-Process | Where-Object { $_.MainWindowTitle -eq $windowTitle } | Stop-Process -Force -ErrorAction SilentlyContinue

    $portProcess = Get-NetUDPEndpoint -LocalPort 3000 -ErrorAction SilentlyContinue
    if ($portProcess) {
        Write-Host "Killing existing process on port 3000 (PID: $($portProcess.OwningProcess))..." -ForegroundColor Gray
        Stop-Process -Id $portProcess.OwningProcess -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 1
    }
    
    Write-Host "Starting Server in new window..." -ForegroundColor Cyan
    Start-Process cmd.exe -ArgumentList "/k title $windowTitle && cd /d $ProjectDir\server && node --no-warnings index.js"
    
    Set-Location $ProjectDir
}
