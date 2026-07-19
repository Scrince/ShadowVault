

[CmdletBinding()]
param(
    [string] $OutputDir = 'C:\Temp\shadowvault-pba-lab',

    [string] $ExportExe = 'C:\Temp\shadowvault-build\tests\Release\sv_pba_export_lab.exe'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $ExportExe)) {
    throw "sv_pba_export_lab not found: $ExportExe (build with -DSV_BUILD_PBA=ON)"
}

$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

& $ExportExe $OutputDir
if ($LASTEXITCODE -ne 0) {
    throw "sv_pba_export_lab failed with exit code $LASTEXITCODE"
}

$LabDir = Join-Path $OutputDir 'lab'
Write-Host "Vectors: $LabDir\header.bin, superblock.bin"
Write-Host "Passphrase: shadowvault-lab"
Write-Output $LabDir