#Requires -Version 5.1
<#
.SYNOPSIS
  Build ShadowVaultBoot.efi with EDK2 (VS2022 / X64).

.PARAMETER Mode
  Stub — passphrase stub only (no crypto).
  LabUnlock — freestanding unlock + vendored SvLibsodium (default; requires LIBSODIUM_ROOT).

.EXAMPLE
  $env:EDK2_ROOT = 'C:\edk2'
  .\build-edk2.ps1 -Edk2Root $env:EDK2_ROOT

.EXAMPLE
  $env:LIBSODIUM_ROOT = 'C:\Temp\libsodium-src'
  $env:LIBSODIUM_INCLUDE = "$env:LIBSODIUM_ROOT\src\libsodium\include"
  .\build-edk2.ps1 -Edk2Root $env:EDK2_ROOT -Mode LabUnlock
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Edk2Root,

    [ValidateSet('Stub', 'LabUnlock')]
    [string] $Mode = 'LabUnlock',

    [ValidateSet('DEBUG', 'RELEASE')]
    [string] $BuildTarget = 'RELEASE',

    [ValidateSet('VS2022', 'VS2019', 'GCC5', 'CLANGPDB')]
    [string] $Toolchain = 'VS2022',

    [string] $Arch = 'X64'
)

$ErrorActionPreference = 'Stop'

$RepoEfi = $PSScriptRoot
$PackageRoot = Join-Path $RepoEfi 'ShadowVaultPkg'
$Edk2Root = (Resolve-Path $Edk2Root).Path.TrimEnd('\')

if (-not (Test-Path (Join-Path $Edk2Root 'edksetup.bat'))) {
    throw "EDK2 root not found (missing edksetup.bat): $Edk2Root"
}

if (-not (Test-Path $PackageRoot)) {
    throw "ShadowVaultPkg not found: $PackageRoot"
}

if ($Mode -eq 'LabUnlock') {
    $DefaultLibsodiumRoot = (Resolve-Path (Join-Path $RepoEfi '..\..\..\external\libsodium-src')).Path
    if (-not $env:LIBSODIUM_ROOT) {
        $env:LIBSODIUM_ROOT = $DefaultLibsodiumRoot
    }
    if (-not (Test-Path $env:LIBSODIUM_ROOT)) {
        throw "LabUnlock requires libsodium sources at LIBSODIUM_ROOT (run scripts/pba-fetch-libsodium.ps1)"
    }
    if (-not $env:LIBSODIUM_INCLUDE) {
        $env:LIBSODIUM_INCLUDE = Join-Path $env:LIBSODIUM_ROOT 'src\libsodium\include'
    }
    if (-not (Test-Path (Join-Path $env:LIBSODIUM_INCLUDE 'sodium.h'))) {
        throw "LabUnlock requires LIBSODIUM_INCLUDE pointing at sodium.h"
    }
}

$PackagesPath = $RepoEfi.TrimEnd('\')
if ($env:PACKAGES_PATH) {
    $PackagesPath = ($env:PACKAGES_PATH.Split(';') + $PackagesPath) -join ';'
}

$NasmPrefix = ''
$nasmCmd = Get-Command nasm -ErrorAction SilentlyContinue
if ($nasmCmd) {
    $NasmDir = Split-Path -Parent $nasmCmd.Source
    $NasmPrefix = $NasmDir.TrimEnd('\') + '\'
}
elseif (Test-Path 'C:\Program Files\NASM\nasm.exe') {
    $NasmPrefix = 'C:\Program Files\NASM\'
}

$DscFile = 'ShadowVaultPkg/ShadowVaultPkg.dsc'
$ModuleInf = 'ShadowVaultPkg/Application/ShadowVaultBoot/ShadowVaultBoot.inf'
if ($Mode -eq 'LabUnlock') {
    $DscFile = 'ShadowVaultPkg/ShadowVaultPkgFreestanding.dsc'
    $ModuleInf = 'ShadowVaultPkg/Application/ShadowVaultBoot/ShadowVaultBootLab.inf'
}

$BuildBat = Join-Path $Edk2Root 'BaseTools\Bin\Win32\build.exe'
if (-not (Test-Path $BuildBat)) {
    $BuildBat = Join-Path $Edk2Root 'BaseTools\BinWrappers\WindowsLike\build.bat'
}

$EdkSetup = Join-Path $Edk2Root 'edksetup.bat'
$BuildCmd = @(
    "call `"$EdkSetup`" >nul",
    "set `"WORKSPACE=$Edk2Root`"",
    "set `"PACKAGES_PATH=$PackagesPath`""
)
if ($NasmPrefix) {
    $BuildCmd += "set `"NASM_PREFIX=$NasmPrefix`""
}
if ($Mode -eq 'LabUnlock') {
    $BuildCmd += "set `"LIBSODIUM_ROOT=$($env:LIBSODIUM_ROOT)`""
    $BuildCmd += "set `"LIBSODIUM_INCLUDE=$($env:LIBSODIUM_INCLUDE)`""
}
$BuildCmd += "call `"$BuildBat`" -p $DscFile -m $ModuleInf -a $Arch -t $Toolchain -b $BuildTarget"

Push-Location $Edk2Root
try {
    cmd /c ($BuildCmd -join ' && ')
    if ($LASTEXITCODE -ne 0) {
        throw "EDK2 build failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

$OutDir = Join-Path $Edk2Root "Build\ShadowVaultPkg\${BuildTarget}_$Toolchain\X64"
$EfiPath = Join-Path $OutDir 'ShadowVaultBoot.efi'
if (-not (Test-Path $EfiPath)) {
    throw "Expected output not found: $EfiPath"
}

$LabExtraModules = @(
    'ShadowVaultPkg/Application/LabBootManager/LabBootManager.inf',
    'ShadowVaultPkg/Application/LabRescue/LabRescue.inf'
)
foreach ($LabModuleInf in $LabExtraModules) {
$LabBuildCmd = @(
    "call `"$EdkSetup`" >nul",
    "set `"WORKSPACE=$Edk2Root`"",
    "set `"PACKAGES_PATH=$PackagesPath`""
)
if ($NasmPrefix) {
    $LabBuildCmd += "set `"NASM_PREFIX=$NasmPrefix`""
}
$LabBuildCmd += "call `"$BuildBat`" -p $DscFile -m $LabModuleInf -a $Arch -t $Toolchain -b $BuildTarget"
Push-Location $Edk2Root
try {
    cmd /c ($LabBuildCmd -join ' && ')
    if ($LASTEXITCODE -ne 0) {
        throw "Lab module build failed ($LabModuleInf) exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}
}

foreach ($labEfi in @('LabBootManager.efi', 'LabRescue.efi')) {
    $labPath = Join-Path $OutDir $labEfi
    if (-not (Test-Path $labPath)) {
        throw "Expected output not found: $labPath"
    }
    Write-Host "Built $labEfi"
}

Write-Host "Built ($Mode): $EfiPath"
Write-Output $EfiPath