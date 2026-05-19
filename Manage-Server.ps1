$ServerPath = "E:\Projects\Fellowship\server"
$PidFile = "$ServerPath\server.pid"

function Stop-Server {
    Write-Host "Checking for existing server processes..." -ForegroundColor Yellow
    $processes = Get-CimInstance Win32_Process -Filter "Name = 'node.exe' and CommandLine like '%index.js%'"
    if ($processes) {
        foreach ($p in $processes) {
            Write-Host "Stopping server process $($p.ProcessId)..." -ForegroundColor Gray
            Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path $PidFile) {
        Remove-Item $PidFile
    }
}

function Start-Server {
    Write-Host "Starting server in a new window..." -ForegroundColor Cyan
    # Use Start-Process with cmd /k to keep the window open and visible
    Start-Process cmd.exe -ArgumentList "/k cd /d `"$ServerPath`" && node --no-warnings index.js"
}

$action = $args[0]
if ($action -eq "stop") {
    Stop-Server
} elseif ($action -eq "start") {
    Stop-Server
    Start-Server
} elseif ($action -eq "restart") {
    Stop-Server
    Start-Server
} else {
    Write-Host "Usage: Manage-Server.ps1 [start|stop|restart]" -ForegroundColor Yellow
}
