[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
Set-Location $ProjectRoot

if (-not (Get-Command conan -ErrorAction SilentlyContinue))
{
    python -m pip install --upgrade conan
}

conan profile detect --force

$qtDir = $env:QT_DIR
if ( [string]::IsNullOrWhiteSpace($qtDir))
{
    $qtCandidates = @(
        'C:\Qt\6.8.3\msvc2022_64',
        'C:\Qt\6.7.3\msvc2022_64',
        'C:\Qt\6.6.3\msvc2022_64'
    )

    foreach ($candidate in $qtCandidates)
    {
        if (Test-Path $candidate)
        {
            $qtDir = $candidate
            break
        }
    }
}

if ( [string]::IsNullOrWhiteSpace($qtDir))
{
    $qtDir = 'C:\Qt'
}

@"
DISABLE_DEBUG=true
BUILD_TESTS=false
QT_DIR=$qtDir
OBS_STUDIO_PATH=C:/
"@ | Set-Content -Path '.env' -Encoding utf8NoBOM

python configure.py
