param(
    [switch]$Build
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$MsysRoot = "D:\msys64"
$CMakeExe = "D:\ST\STM32CubeCLT_1.18.0\CMake\bin\cmake.exe"
$BuildDir = Join-Path $ProjectRoot "build"
$SimulatorExe = Join-Path $BuildDir "lvgl_motion_demo.exe"

$MingwBin = Join-Path $MsysRoot "mingw64\bin"
$UsrBin = Join-Path $MsysRoot "usr\bin"

if(-not (Test-Path $MingwBin)) {
    throw "MSYS2 MinGW64 bin not found: $MingwBin"
}

if(-not (Test-Path $UsrBin)) {
    throw "MSYS2 usr bin not found: $UsrBin"
}

$env:Path = "$MingwBin;$UsrBin;$env:Path"
Set-Location $ProjectRoot

if($Build) {
    if(-not (Test-Path $CMakeExe)) {
        throw "CMake not found: $CMakeExe"
    }

    if(-not (Test-Path $BuildDir)) {
        & $CMakeExe -S $ProjectRoot -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release
    }

    & $CMakeExe --build $BuildDir
}

if(-not (Test-Path $SimulatorExe)) {
    throw "Simulator executable not found: $SimulatorExe. Run this script with -Build first."
}

& $SimulatorExe
exit $LASTEXITCODE
