# AriaAgent — one-shot Release build + runtime deployment.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1
#
# Steps:
#   1. configure Release build (Ninja + MSYS2 UCRT64)
#   2. build aria_agent.exe
#   3. deploy runtime DLLs (windeployqt + recursive deps)
#   4. print the final path
param(
    [string]$QtPrefix = "D:\worksoft\msys64\ucrt64",
    [string]$BuildDir = "build\release"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

# ── MSYS2 toolchain must be on PATH for cmake/ninja/g++ ─────────────────────
$Msys2Bin = "D:\worksoft\msys64\ucrt64\bin"
if (Test-Path (Join-Path $Msys2Bin "g++.exe")) {
    $env:PATH = "$Msys2Bin;$env:PATH"
} else {
    Write-Host "MSYS2 UCRT64 not found at $Msys2Bin" -ForegroundColor Red
    exit 1
}

# ── 1. Configure ────────────────────────────────────────────────────────────
& cmake -S $RepoRoot -B (Join-Path $RepoRoot $BuildDir) -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH=$QtPrefix
if ($LASTEXITCODE -ne 0) { Write-Host "configure failed" -ForegroundColor Red; exit 1 }

# ── 2. Build ────────────────────────────────────────────────────────────────
& cmake --build (Join-Path $RepoRoot $BuildDir) -j 8
if ($LASTEXITCODE -ne 0) { Write-Host "build failed" -ForegroundColor Red; exit 1 }

# ── 3. Deploy DLLs ──────────────────────────────────────────────────────────
& powershell -NoProfile -ExecutionPolicy Bypass -File `
    (Join-Path $PSScriptRoot "deploy-dlls.ps1") (Join-Path $RepoRoot $BuildDir)

$Exe = Join-Path $RepoRoot (Join-Path $BuildDir "aria_agent.exe")
Write-Host ""
Write-Host "✅ Release build ready: $Exe" -ForegroundColor Green
Write-Host "   Run it by double-clicking, or:" -ForegroundColor Cyan
Write-Host "   cd $RepoRoot ; .\$BuildDir\aria_agent.exe" -ForegroundColor Cyan
