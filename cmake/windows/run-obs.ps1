param(
    [Parameter(Mandatory = $true)]
    [string]$ObsBinDir
)

$ErrorActionPreference = 'Stop'

$obsExe = Join-Path $ObsBinDir 'obs64.exe'
if (-not (Test-Path -LiteralPath $obsExe)) {
    throw "OBS not found: $obsExe"
}

$installRoot = Split-Path -Parent (Split-Path -Parent $ObsBinDir)
$logDirs = @(
    (Join-Path $installRoot 'config\obs-studio\logs'),
    (Join-Path $env:APPDATA 'obs-studio\logs')
)

$existingLogs = @{}
foreach ($dir in $logDirs) {
    if (Test-Path -LiteralPath $dir) {
        Get-ChildItem -LiteralPath $dir -Filter '*.txt' -ErrorAction SilentlyContinue |
            ForEach-Object { $existingLogs[$_.FullName] = $true }
    }
}

$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $obsExe
$startInfo.WorkingDirectory = $ObsBinDir
$startInfo.Arguments = '--verbose'
$startInfo.UseShellExecute = $false

$obs = [System.Diagnostics.Process]::Start($startInfo)
if ($null -eq $obs) {
    throw 'Failed to start OBS'
}

Write-Host "Started OBS (pid $($obs.Id))"

function Find-ObsLog {
    foreach ($dir in $logDirs) {
        if (-not (Test-Path -LiteralPath $dir)) {
            continue
        }

        $newest = Get-ChildItem -LiteralPath $dir -Filter '*.txt' -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($null -eq $newest) {
            continue
        }
        if (-not $existingLogs.ContainsKey($newest.FullName)) {
            return $newest.FullName
        }
        if ($newest.LastWriteTime -gt (Get-Date).AddSeconds(-20)) {
            return $newest.FullName
        }
    }
    return $null
}

$logFile = $null
for ($i = 0; $i -lt 100 -and -not $obs.HasExited; $i++) {
    Start-Sleep -Milliseconds 100
    $logFile = Find-ObsLog
    if ($null -ne $logFile) {
        break
    }
}

function Stop-ObsProcess {
    if ($null -ne $obs -and -not $obs.HasExited) {
        try {
            $obs.Kill()
        } catch {
        }
    }
    Get-Process -Name obs64 -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

try {
    if ($null -eq $logFile) {
        Write-Host 'OBS log file not found; waiting for OBS to exit.'
        $obs.WaitForExit()
        exit $obs.ExitCode
    }

    Write-Host "Streaming OBS log: $logFile"
    $stream = [System.IO.File]::Open(
        $logFile,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite
    )
    $reader = New-Object System.IO.StreamReader($stream)
    try {
        while (-not $obs.HasExited) {
            $chunk = $reader.ReadToEnd()
            if ($chunk.Length -gt 0) {
                [Console]::Write($chunk)
                [Console]::Out.Flush()
            } else {
                Start-Sleep -Milliseconds 80
            }
        }

        $chunk = $reader.ReadToEnd()
        if ($chunk.Length -gt 0) {
            [Console]::Write($chunk)
        }
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }

    exit $obs.ExitCode
} finally {
    Stop-ObsProcess
}
