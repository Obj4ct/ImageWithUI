# build.ps1 - one-click build for ImageWithUI (Qt 5 + MSVC)
#
# Usage:
#   .\build.ps1                 # generate Visual Studio solution -> build-vs\ImageWithUI.sln
#   .\build.ps1 -VSBuild        # generate + build (Release) -> build-vs\Release\ImageWithUI.exe
#   .\build.ps1 -VSBuild -Config Debug
#   .\build.ps1 -NMake          # NMake build instead (output -> build\)
#   .\build.ps1 -Clean          # remove build-vs\ and build\ first
#
# Notes:
#   - Runs without cmd.exe / vcvars64.bat; sets MSVC + Windows SDK env directly.
#   - The VS generator requires the "Desktop development with C++" workload;
#     on a flat VS layout auto-detection may fail (this script binds VCTargetsPath
#     to work around it).
#   - If execution policy blocks the script, run:
#       powershell -ExecutionPolicy Bypass -File build.ps1

param(
    [ValidateSet('Release','Debug')][string]$Config = 'Release',
    [switch]$NMake,
    [switch]$VSBuild,
    [switch]$Clean
)

# ---- toolchain paths (edit to match your machine) ----
$VsTools  = "D:\Microsoft Visual Studio\VC\Tools\MSVC\14.44.35207"
$WinSdk   = "D:\Windows Kits\10"
$WinSdkVer= "10.0.19041.0"
$QtPrefix = "D:\Qt\5.15.2\msvc2019_64"
$CMakeExe = "D:\cmake-gui\bin\cmake.exe"
$MSBuildExe = "D:\Microsoft Visual Studio\MSBuild\Current\Bin\amd64\MSBuild.exe"
$NMakeExe = Join-Path $VsTools "bin\Hostx64\x64\nmake.exe"

# project dir = parent of this scripts\ folder
$Proj = Split-Path $PSScriptRoot -Parent

# ---- de-duplicate env names that differ only in case ------------------
# .NET Framework loads the process environment into a case-insensitive
# Hashtable; if both "HTTPS_PROXY" and "https_proxy" exist, MSBuild throws
#   MSB6001 / ArgumentException: An item with the same key has already been added
# and never even starts cl.exe -> CMake reports "compiler identification is
# unknown". Keep exactly one spelling of each such name.
$h = [Environment]::GetEnvironmentVariables()
$groups = @{}
foreach ($k in $h.Keys) {
    $lk = "$k".ToLowerInvariant()
    if (-not $groups.ContainsKey($lk)) { $groups[$lk] = New-Object System.Collections.ArrayList }
    [void]$groups[$lk].Add("$k")
}
foreach ($lk in $groups.Keys) {
    $names = $groups[$lk]
    if ($names.Count -le 1) { continue }
    $val = [Environment]::GetEnvironmentVariable($names[0])
    foreach ($n in $names) { [Environment]::SetEnvironmentVariable($n, $null) }
    [Environment]::SetEnvironmentVariable($names[0], $val)
}

# ---- set MSVC + Windows SDK environment (no cmd.exe / vcvars needed) ----
$env:PATH    = "$VsTools\bin\Hostx64\x64;$QtPrefix\bin;$WinSdk\bin\$WinSdkVer\x64;" + $env:PATH
$env:INCLUDE = "$VsTools\include;$WinSdk\Include\$WinSdkVer\ucrt;$WinSdk\Include\$WinSdkVer\um;$WinSdk\Include\$WinSdkVer\shared"
$env:LIB     = "$VsTools\lib\x64;$WinSdk\Lib\$WinSdkVer\ucrt\x64;$WinSdk\Lib\$WinSdkVer\um\x64"

if ($Clean) {
    Write-Host "Cleaning build and build-vs directories..."
    foreach ($d in @("build","build-vs")) {
        $p = Join-Path $Proj $d
        if (Test-Path $p) { Remove-Item -Recurse -Force $p }
    }
}

if (-not $NMake) {
    # default: generate the Visual Studio solution (build also with -VSBuild)
    # Bind MSBuild's VC targets so CMake's VS generator can resolve cl.exe
    # (needed on flat VS layouts where auto-detection fails).
    $env:VCTargetsPath = "D:\Microsoft Visual Studio\MSBuild\Microsoft\VC\v170\"
    $env:VSINSTALLDIR = "D:\Microsoft Visual Studio\"
    $env:VCINSTALLDIR = "D:\Microsoft Visual Studio\VC\"
    $env:VCToolsInstallDir = "D:\Microsoft Visual Studio\VC\Tools\"
    $env:VisualStudioVersion = "17.0"
    $env:WindowsSDKVersion = "10.0.19041.0\"
    $env:WindowsSdkDir = "$WinSdk\"
    $env:UniversalCRTSdkDir = "$WinSdk\"
    $env:UCRTVersion = "10.0.19041.0\"
    $out = Join-Path $Proj "build-vs"
    Write-Host "Generating Visual Studio solution -> $out"
    & $CMakeExe -G "Visual Studio 17 2022" -A x64 "-DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION=10.0.19041.0" -DCMAKE_PREFIX_PATH=$QtPrefix -S $Proj -B $out
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake (VS generator) could not detect cl.exe on this flat VS layout. In VS2022 use File > Open > Folder on the project dir (CMake integration), or just build with .\build.ps1"
        exit 1
    }
    $sln = Join-Path $out "ImageWithUI.sln"
    if (-not (Test-Path $sln)) { Write-Error "Solution not generated."; exit 1 }
    Write-Host "Done: $sln"

    if ($VSBuild) {
        Write-Host "Building solution with MSBuild ..."
        & $MSBuildExe $sln /p:Configuration=$Config /p:Platform=x64 /v:m /nologo
        if ($LASTEXITCODE -ne 0) { Write-Error "MSBuild failed."; exit 1 }
        $vsexe = Join-Path $out "$Config\ImageWithUI.exe"
        if (Test-Path $vsexe) { Write-Host "OK: $vsexe ($((Get-Item $vsexe).Length) bytes)" }
    }
    exit 0
}

# ---- NMake build (default) ----
$out = Join-Path $Proj "build"
New-Item -ItemType Directory -Path $out -Force | Out-Null
Write-Host "Configuring ($Config) in $out ..."
& $CMakeExe -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=$Config -DCMAKE_PREFIX_PATH=$QtPrefix -S $Proj -B $out
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed."; exit 1 }

Write-Host "Building ..."
Push-Location $out
& $NMakeExe
$code = $LASTEXITCODE
Pop-Location
if ($code -ne 0) { Write-Error "nmake failed."; exit 1 }

$exe = Join-Path $out "ImageWithUI.exe"
if (Test-Path $exe) { Write-Host "OK: $exe ($((Get-Item $exe).Length) bytes)" } else { Write-Error "exe not found."; exit 1 }
