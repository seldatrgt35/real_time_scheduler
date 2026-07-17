[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",

    [ValidateSet("FP", "RMS", "EDF")]
    [string]$Policy = "FP",

    [string]$S32DSRoot = "",
    [string]$SdkRoot = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot

function Find-LatestDirectory {
    param([string]$Root, [string]$Filter)

    Get-ChildItem -Path $Root -Directory -Filter $Filter -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

if (-not $S32DSRoot) {
    $S32DSRoot = Find-LatestDirectory -Root "C:\NXP" -Filter "S32DS.*"
}
if (-not $SdkRoot) {
    $SdkRoot = Find-LatestDirectory -Root "C:\NXP" -Filter "S32_SDK_S32K1xx*"
}
if (-not $S32DSRoot -or -not (Test-Path -LiteralPath $S32DSRoot)) {
    throw "S32 Design Studio was not found. Pass -S32DSRoot <directory>."
}
if (-not $SdkRoot -or -not (Test-Path -LiteralPath $SdkRoot)) {
    throw "The S32K1xx SDK was not found. Pass -SdkRoot <directory>."
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    $knownCMake = "C:\Program Files\CMake\bin\cmake.exe"
    if (Test-Path -LiteralPath $knownCMake) {
        $cmake = $knownCMake
    } else {
        throw "CMake 3.20 or newer is required. Install Kitware CMake and reopen PowerShell."
    }
} else {
    $cmake = $cmakeCommand.Source
}

$gcc = Get-ChildItem -Path (Join-Path $S32DSRoot "S32DS\build_tools") `
    -Filter "arm-none-eabi-gcc.exe" -File -Recurse |
    Where-Object { $_.FullName -notmatch "gcc_v9\.2" } |
    Select-Object -First 1
if (-not $gcc) {
    $gcc = Get-ChildItem -Path (Join-Path $S32DSRoot "S32DS\build_tools") `
        -Filter "arm-none-eabi-gcc.exe" -File -Recurse |
        Select-Object -First 1
}
if (-not $gcc) {
    throw "The S32DS arm-none-eabi GCC toolchain was not found."
}
$toolchainBin = $gcc.Directory.FullName

$make = Join-Path $S32DSRoot "S32DS\build_tools\msys32\usr\bin\make.exe"
if (-not (Test-Path -LiteralPath $make)) {
    throw "The S32DS MSYS make executable was not found at $make."
}
$msysBin = Split-Path -Parent $make

$deviceInclude = Join-Path $SdkRoot "platform\devices\S32K148\include"
if (-not (Test-Path -LiteralPath (Join-Path $deviceInclude "S32K148.h"))) {
    throw "S32K148.h was not found below the selected SDK."
}

$cmsisHeader = Get-ChildItem -Path (Join-Path $S32DSRoot "eclipse\plugins") `
    -Filter "core_cm4.h" -File -Recurse |
    Select-Object -First 1
if (-not $cmsisHeader) {
    throw "CMSIS core_cm4.h was not found in the S32DS installation."
}
$cmsisInclude = $cmsisHeader.Directory.FullName

$buildDirectory = Join-Path $repoRoot `
    ("build-s32k148-{0}-{1}" -f $Policy.ToLowerInvariant(), $BuildType.ToLowerInvariant())
if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    $resolvedBuild = (Resolve-Path -LiteralPath $buildDirectory).Path
    if (-not $resolvedBuild.StartsWith($repoRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a build directory outside the repository."
    }
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}

$savedPath = $env:PATH
try {
    $env:PATH = "$toolchainBin;$msysBin;$savedPath"
    & $cmake -S $repoRoot -B $buildDirectory -G "MSYS Makefiles" `
        "-DCMAKE_MAKE_PROGRAM=$make" `
        "-DCMAKE_TOOLCHAIN_FILE=$repoRoot\cmake\arm-none-eabi-gcc.cmake" `
        "-DRTS_ARM_TOOLCHAIN_BIN=$toolchainBin" `
        "-DCMAKE_BUILD_TYPE=$BuildType" `
        "-DRTS_BUILD_HOST_TESTS=OFF" `
        "-DRTS_BUILD_S32K148_SMOKE=ON" `
        "-DRTS_S32K148_POLICY=$Policy" `
        "-DRTS_S32K148_DEVICE_INCLUDE_DIR=$deviceInclude" `
        "-DRTS_S32K148_CMSIS_INCLUDE_DIR=$cmsisInclude"
    if ($LASTEXITCODE -ne 0) {
        throw "S32K148 CMake configuration failed."
    }

    & $cmake --build $buildDirectory --target rts_target_s32k148_smoke
    if ($LASTEXITCODE -ne 0) {
        throw "S32K148 target build failed."
    }
} finally {
    $env:PATH = $savedPath
}

$artifactDirectory = Join-Path $buildDirectory "targets\nxp\s32k148"
Write-Host ""
Write-Host "S32K148 EVB-Q176 image created successfully:"
Write-Host "  ELF : $(Join-Path $artifactDirectory 'rts_s32k148_smoke.elf')"
Write-Host "  HEX : $(Join-Path $artifactDirectory 'rts_s32k148_smoke.hex')"
Write-Host "  SREC: $(Join-Path $artifactDirectory 'rts_s32k148_smoke.srec')"

