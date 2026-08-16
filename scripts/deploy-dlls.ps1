# AriaAgent — deploy runtime DLLs next to the exe so it runs by double-click.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\deploy-dlls.ps1 [BuildDir]
#
# Steps:
#   1. windeployqt  -> Qt DLLs + plugins (platforms/qwindows.dll, imageformats, …)
#   2. recursive    -> objdump-based closure copy of MSYS2 / aria DLLs
#   System DLLs (kernel32, d3d11, ntdll, …) are intentionally skipped.

param(
    [string]$BuildDir = "build\flavors\debug"
)

$ErrorActionPreference = "Stop"

# Accept a repo-root-relative or absolute path.
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path (Split-Path -Parent $PSScriptRoot) $BuildDir
}

$Exe = Join-Path $BuildDir "aria_agent.exe"
if (-not (Test-Path $Exe)) { Write-Host "exe not found: $Exe" -ForegroundColor Red; exit 1 }

$Msys2Bin = "D:\worksoft\msys64\ucrt64\bin"
$AriaBin  = Join-Path $BuildDir "bin"

# ── 1. Qt deployment ────────────────────────────────────────────────────────
$Windeployqt = Join-Path $Msys2Bin "windeployqt.exe"
if (Test-Path $Windeployqt) {
    Write-Host "[deploy] windeployqt..." -ForegroundColor Cyan
    & $Windeployqt --no-translations --no-system-d3d-compiler --no-opengl-sw $Exe | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "windeployqt failed" -ForegroundColor Red }
} else {
    Write-Host "[deploy] windeployqt not found, skipping Qt plugins" -ForegroundColor Yellow
}

# ── 2. Recursive dependency copy (objdump) ──────────────────────────────────
function Get-Imports($filePath) {
    $tmp = [System.IO.Path]::GetTempFileName()
    & "$Msys2Bin\objdump.exe" -p $filePath > $tmp 2>$null
    $output = Get-Content $tmp
    Remove-Item $tmp
    $dlls = @()
    foreach ($line in $output) {
        if ($line -match "DLL Name:\s*(.+)") { $dlls += $matches[1].Trim() }
    }
    return $dlls
}

# System DLLs that must never be copied.
$SystemDll = '^(api-ms-win|kernel32|user32|gdi32|advapi32|shell32|ole32|oleaut32|comdlg32|ws2_32|crypt32|dwmapi|winmm|version|shcore|ucrtbase|vcruntime|winhttp|wldap32|netapi32|secur32|authz|mpr|rpcrt4|userenv|usp10|uxtheme|d3d11|d3d12|dxgi|ntdll|dwrite)'

Write-Host "[deploy] copying runtime dependencies..." -ForegroundColor Cyan
$queue = @($Exe)
$processed = @{}
$copied = 0
while ($queue.Count -gt 0) {
    $file = $queue[0]; $queue = $queue[1..($queue.Count - 1)]
    if ($processed.ContainsKey($file)) { continue }
    $processed[$file] = $true
    foreach ($dll in (Get-Imports $file)) {
        if ($dll -match $SystemDll) { continue }
        $dest = Join-Path $BuildDir $dll
        if (Test-Path $dest) { $queue += $dest; continue }
        foreach ($srcDir in @($Msys2Bin, $AriaBin)) {
            $src = Join-Path $srcDir $dll
            if (Test-Path $src) {
                Copy-Item -Path $src -Destination $dest -Force
                Write-Host "  + $dll" -ForegroundColor DarkGray
                $copied++
                $queue += $dest
                break
            }
        }
    }
}
Write-Host "[deploy] done. $copied DLL(s) copied." -ForegroundColor Green
Write-Host "Now run: $Exe" -ForegroundColor Cyan
