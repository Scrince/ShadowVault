#Requires -Version 5.1
<#
.SYNOPSIS
  Verify PBA lab vector export + host unlock (no EDK2/QEMU required).

.EXAMPLE
  .\pba-verify-lab.ps1
#>
[CmdletBinding()]
param(
    [string] $BuildDir = 'C:\Temp\shadowvault-build',
    [string] $ExportRoot = 'C:\Temp\shadowvault-pba-lab-verify'
)

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $PSCommandPath

$ExportExe = Join-Path $BuildDir 'tests\Release\sv_pba_export_lab.exe'
$IntegrationExe = Join-Path $BuildDir 'tests\Release\sv_integration_tests.exe'

foreach ($exe in @($ExportExe, $IntegrationExe)) {
    if (-not (Test-Path $exe)) {
        throw "Missing $exe - build with -DSV_BUILD_PBA=ON"
    }
}

& (Join-Path $ScriptDir 'pba-export-lab-vectors.ps1') -OutputDir $ExportRoot -ExportExe $ExportExe

$Header = Join-Path $ExportRoot 'lab\header.bin'
$Super = Join-Path $ExportRoot 'lab\superblock.bin'
if ((Get-Item $Header).Length -ne 4096) {
    throw "header.bin size != 4096"
}
if ((Get-Item $Super).Length -ne 512) {
    throw "superblock.bin size != 512"
}

cmd /c "`"$IntegrationExe`" [integration][pba][p107]"
if ($LASTEXITCODE -ne 0) {
    throw "P1-07 integration test failed"
}

Write-Host "Lab pipeline OK (export + host unlock)"
Write-Host "Passphrase: shadowvault-lab"
Write-Host "Vectors: $ExportRoot\lab"