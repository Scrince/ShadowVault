#Requires -Version 5.1
<#
.SYNOPSIS
  Create a FAT32 ESP VHD image with full PBA lab layout for QEMU.

.EXAMPLE
  .\pba-make-esp.ps1 -EfiPath C:\edk2\Build\ShadowVaultPkg\RELEASE_VS2022\X64\ShadowVaultBoot.efi -IncludeLabVectors
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $EfiPath,

    [string] $OutputImage = 'C:\Temp\shadowvault-pba-esp.img',

    [int] $SizeMiB = 64,

    [switch] $IncludeLabVectors,

    [string] $ExportExe = 'C:\Temp\shadowvault-build\tests\Release\sv_pba_export_lab.exe'
)

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $PSCommandPath

$EfiPath = (Resolve-Path $EfiPath).Path
$OutputImage = [IO.Path]::GetFullPath($OutputImage)
$BuildDir = Split-Path -Parent $EfiPath

$Staging = Join-Path $env:TEMP ("sv_pba_esp_staging_" + [guid]::NewGuid().ToString('N'))
& (Join-Path $ScriptDir 'pba-prepare-esp-dir.ps1') `
    -EfiPath $EfiPath `
    -OutputDir $Staging `
    -IncludeLabVectors:$IncludeLabVectors `
    -BootMgrPath (Join-Path $BuildDir 'LabBootManager.efi') `
    -RescueEfiPath (Join-Path $BuildDir 'LabRescue.efi') `
    -ExportExe $ExportExe | Out-Null

if (Test-Path $OutputImage) {
    Remove-Item -Force $OutputImage
}

$Vhd = [IO.Path]::ChangeExtension($OutputImage, '.vhdx')
if (Test-Path $Vhd) {
    Remove-Item -Force $Vhd
}

New-Vhd -Path $Vhd -SizeBytes ($SizeMiB * 1MB) -Fixed | Out-Null
$Disk = Mount-Vhd -Path $Vhd -Passthru
Start-Sleep -Seconds 2
Initialize-Disk -Number $Disk.Number -PartitionStyle GPT -Confirm:$false | Out-Null
$Partition = New-Partition -DiskNumber $Disk.Number -UseMaximumSize -AssignDriveLetter
Format-Volume -Partition $Partition -FileSystem FAT32 -NewFileSystemLabel 'SV_PBA_ESP' -Confirm:$false | Out-Null
$Drive = "$($Partition.DriveLetter):"
Copy-Item -Recurse -Force (Join-Path $Staging 'EFI') (Join-Path $Drive 'EFI')
Dismount-Vhd -Path $Vhd

Copy-Item -Force $Vhd $OutputImage
Remove-Item -Force $Vhd
Remove-Item -Recurse -Force $Staging

Write-Host "ESP image: $OutputImage"
Write-Host "Layout: EFI/BOOT/BOOTX64.EFI + EFI/Microsoft/Boot/bootmgfw.efi"
if ($IncludeLabVectors) {
    Write-Host "Lab: EFI/ShadowVault/lab/ (passphrase: shadowvault-lab)"
}
Write-Output $OutputImage