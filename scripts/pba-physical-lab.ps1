#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Stage ShadowVault PBA on a physical USB ESP (FAT32) for UEFI laptop boot (P1-01).

.PARAMETER UsbDrive
  Drive letter of the USB ESP (e.g. E:). Must be FAT32 with EFI partition.

.PARAMETER EfiPath
  Path to ShadowVaultBoot.efi (LabUnlock build recommended).

.EXAMPLE
  .\pba-physical-lab.ps1 -UsbDrive E -EfiPath C:\edk2\Build\ShadowVaultPkg\RELEASE_VS2022\X64\ShadowVaultBoot.efi
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Z]:$')]
    [string] $UsbDrive,

    [Parameter(Mandatory = $true)]
    [string] $EfiPath,

    [switch] $IncludeLabVectors,

    [string] $ExportExe = 'C:\Temp\shadowvault-build\tests\Release\sv_pba_export_lab.exe'
)

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $PSCommandPath

$EfiPath = (Resolve-Path $EfiPath).Path
$BuildDir = Split-Path -Parent $EfiPath
$BootMgr = Join-Path $BuildDir 'LabBootManager.efi'
$Rescue = Join-Path $BuildDir 'LabRescue.efi'

if (-not (Test-Path $UsbDrive)) {
    throw "USB drive not found: $UsbDrive"
}

$vol = Get-Volume -DriveLetter $UsbDrive.TrimEnd(':') -ErrorAction Stop
if ($vol.FileSystem -ne 'FAT32') {
    throw "USB drive $UsbDrive must be FAT32 (got $($vol.FileSystem))"
}

$Staging = Join-Path $env:TEMP ("sv_pba_usb_" + [guid]::NewGuid().ToString('N'))
& (Join-Path $ScriptDir 'pba-prepare-esp-dir.ps1') `
    -EfiPath $EfiPath `
    -OutputDir $Staging `
    -IncludeLabVectors:$IncludeLabVectors `
    -BootMgrPath $BootMgr `
    -ExportExe $ExportExe | Out-Null

if (Test-Path $Rescue) {
    $rescueDir = Join-Path $Staging 'EFI\ShadowVault\Rescue'
    New-Item -ItemType Directory -Force -Path $rescueDir | Out-Null
    Copy-Item -Force $Rescue (Join-Path $rescueDir 'ShadowVaultRescue.efi')
}

Write-Host "Copying ESP tree to $UsbDrive ..."
Copy-Item -Recurse -Force (Join-Path $Staging 'EFI') (Join-Path $UsbDrive 'EFI')
Remove-Item -Recurse -Force $Staging

Write-Host ''
Write-Host 'Physical lab ESP staged.'
Write-Host "  Boot entry: $UsbDrive\EFI\BOOT\BOOTX64.EFI"
Write-Host "  Chain-load: $UsbDrive\EFI\Microsoft\Boot\bootmgfw.efi (lab stub)"
if ($IncludeLabVectors) {
    Write-Host '  Lab vectors: EFI\ShadowVault\lab\ (passphrase: shadowvault-lab)'
}
Write-Host ''
Write-Host 'Firmware steps:'
Write-Host '  1. Disable Secure Boot (unsigned lab EFI).'
Write-Host '  2. Boot once from USB (OVMF/UEFI boot menu).'
Write-Host '  3. Select [1] Unlock and boot — expect chain-load to lab boot manager.'
Write-Host '  4. Document firmware vendor + boot path in lab notes (P1-01).'