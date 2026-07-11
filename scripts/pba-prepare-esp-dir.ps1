#Requires -Version 5.1
<#
.SYNOPSIS
  Populate a FAT-ready directory tree for QEMU -drive file=fat:rw:...

.EXAMPLE
  .\pba-prepare-esp-dir.ps1 -EfiPath C:\edk2\Build\ShadowVaultPkg\RELEASE_VS2022\X64\ShadowVaultBoot.efi -OutputDir C:\Temp\shadowvault-pba-esp
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $EfiPath,

    [string] $OutputDir = 'C:\Temp\shadowvault-pba-esp',

    [switch] $IncludeLabVectors,

    [string] $BootMgrPath,

    [string] $RescueEfiPath,

    [switch] $ForceWrongPasswordTest,

    [switch] $ForceRescueTest,

    [string] $ExportExe = 'C:\Temp\shadowvault-build\tests\Release\sv_pba_export_lab.exe'
)

$ErrorActionPreference = 'Stop'

$EfiPath = (Resolve-Path $EfiPath).Path
$OutputDir = [IO.Path]::GetFullPath($OutputDir)

if (Test-Path $OutputDir) {
    Remove-Item -Recurse -Force $OutputDir
}

$BootDir = Join-Path $OutputDir 'EFI\BOOT'
$SvDir = Join-Path $OutputDir 'EFI\ShadowVault'
$MsBootDir = Join-Path $OutputDir 'EFI\Microsoft\Boot'
New-Item -ItemType Directory -Force -Path $BootDir, $SvDir, $MsBootDir | Out-Null

Copy-Item -Force $EfiPath (Join-Path $BootDir 'BOOTX64.EFI')
Copy-Item -Force $EfiPath (Join-Path $SvDir 'ShadowVaultBoot.efi')

if (-not $BootMgrPath) {
    $candidate = Join-Path (Split-Path -Parent $EfiPath) 'LabBootManager.efi'
    if (Test-Path $candidate) {
        $BootMgrPath = $candidate
    }
}
if ($BootMgrPath -and (Test-Path $BootMgrPath)) {
    Copy-Item -Force $BootMgrPath (Join-Path $MsBootDir 'bootmgfw.efi')
    Write-Host "Chain-load target: EFI\Microsoft\Boot\bootmgfw.efi"
}

if ($IncludeLabVectors) {
    $ExportRoot = Join-Path $env:TEMP ("sv_pba_lab_" + [guid]::NewGuid().ToString('N'))
    $ScriptDir = Split-Path -Parent $PSCommandPath
    & (Join-Path $ScriptDir 'pba-export-lab-vectors.ps1') -OutputDir $ExportRoot -ExportExe $ExportExe | Out-Null
    $LabSrc = Join-Path $ExportRoot 'lab'
    $LabDest = Join-Path $SvDir 'lab'
    New-Item -ItemType Directory -Force -Path $LabDest | Out-Null
    Copy-Item -Force (Join-Path $LabSrc 'header.bin') (Join-Path $LabDest 'header.bin')
    Copy-Item -Force (Join-Path $LabSrc 'superblock.bin') (Join-Path $LabDest 'superblock.bin')
    if ($ForceWrongPasswordTest) {
        New-Item -ItemType File -Force -Path (Join-Path $LabDest 'force_wrong_password') | Out-Null
    }
    if ($ForceRescueTest) {
        New-Item -ItemType File -Force -Path (Join-Path $LabDest 'force_rescue_test') | Out-Null
    }
    Remove-Item -Recurse -Force $ExportRoot -ErrorAction SilentlyContinue
}

if ($RescueEfiPath -and (Test-Path $RescueEfiPath)) {
    $RescueDir = Join-Path $SvDir 'Rescue'
    New-Item -ItemType Directory -Force -Path $RescueDir | Out-Null
    Copy-Item -Force $RescueEfiPath (Join-Path $RescueDir 'ShadowVaultRescue.efi')
    Write-Host "Rescue target: EFI\ShadowVault\Rescue\ShadowVaultRescue.efi"
}

Write-Host "ESP directory: $OutputDir"
Write-Host "Boot entry: EFI\BOOT\BOOTX64.EFI"
Write-Output $OutputDir