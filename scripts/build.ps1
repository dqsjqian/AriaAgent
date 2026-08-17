# Unified one-shot build runner for Windows (MSYS2 UCRT64 + Qt6).
#
# Usage:
#   .\scripts\build.ps1             # Release build
#   .\scripts\build.ps1 debug       # Debug build
#   .\scripts\build.ps1 run         # Debug build, deploy DLLs, and launch
#   .\scripts\build.ps1 release-run # Release build, deploy DLLs, and launch
#   .\scripts\build.ps1 clean       # Remove all build output
param(
    [ValidateSet("release", "debug", "run", "release-run", "clean")]
    [string]$Mode = "release"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

if ($Mode -eq "clean") {
    $BuildRoot = Join-Path $RepoRoot "build"
    Write-Host "[build] Removing $BuildRoot"
    if (Test-Path $BuildRoot) { Remove-Item -Recurse -Force $BuildRoot }
    exit 0
}

$BuildType = if ($Mode -in @("debug", "run")) { "Debug" } else { "Release" }
$Flavor = $BuildType.ToLowerInvariant()
$BuildDir = Join-Path $RepoRoot "build\flavors\$Flavor"

function Find-Msys2Prefix {
    if ($env:MSYS2_ROOT) {
        $candidate = Join-Path $env:MSYS2_ROOT "ucrt64"
        if (Test-Path (Join-Path $candidate "bin\g++.exe")) { return $candidate }
    }
    foreach ($candidate in @(
        "C:\msys64\ucrt64",
        "D:\msys64\ucrt64",
        "D:\worksoft\msys64\ucrt64",
        (Join-Path $env:USERPROFILE "msys64\ucrt64")
    )) {
        if (Test-Path (Join-Path $candidate "bin\g++.exe")) { return $candidate }
    }
    return $null
}

$Msys2Prefix = Find-Msys2Prefix
if (-not $Msys2Prefix) {
    Write-Error "MSYS2 UCRT64 was not found. Install it or set MSYS2_ROOT."
}
$Msys2Bin = Join-Path $Msys2Prefix "bin"
$env:PATH = "$Msys2Bin;$env:PATH"
if (-not $env:CC) { $env:CC = "gcc" }
if (-not $env:CXX) { $env:CXX = "g++" }

$QtPrefix = if ($env:QT_DIR) { $env:QT_DIR } else { $Msys2Prefix }
if (-not (Test-Path (Join-Path $QtPrefix "lib\cmake\Qt6\Qt6Config.cmake"))) {
    Write-Error "Qt6 was not found. Install the MSYS2 Qt6 package or set QT_DIR."
}

foreach ($tool in @("cmake", "git")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Error "$tool is not installed or not on PATH."
    }
}

$AriaCMake = Join-Path $RepoRoot "third_party\aria\CMakeLists.txt"
if (-not (Test-Path $AriaCMake)) {
    Write-Host "[build] Initializing the Aria submodule..."
    & git -C $RepoRoot submodule update --init third_party/aria
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$ConfigureArgs = @(
    "-S", $RepoRoot,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_PREFIX_PATH=$QtPrefix"
)
if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $ConfigureArgs += @("-G", "Ninja")
}

Write-Host "[build] Configuring $BuildType"
& cmake @ConfigureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$Jobs = if ($env:ARIA_BUILD_JOBS) { $env:ARIA_BUILD_JOBS } elseif ($env:NUMBER_OF_PROCESSORS) { $env:NUMBER_OF_PROCESSORS } else { 4 }
Write-Host "[build] Building with $Jobs jobs"
& cmake --build $BuildDir --parallel $Jobs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot "deploy-dlls.ps1") -BuildDir $BuildDir -Msys2Bin $Msys2Bin
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$Executable = Join-Path $BuildDir "bin\aria_agent.exe"
if (-not (Test-Path $Executable)) {
    Write-Error "Executable not found at $Executable"
}
Write-Host "[build] Complete: $Executable" -ForegroundColor Green

if ($Mode -in @("run", "release-run")) {
    Write-Host "[build] Launching AriaAgent"
    & $Executable
    exit $LASTEXITCODE
}
