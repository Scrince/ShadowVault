#Requires -Version 5.1
<#
.SYNOPSIS
  Clone EDK2 for ShadowVaultBoot.efi builds.

.EXAMPLE
  .\pba-setup-edk2.ps1 -Destination C:\edk2
#>
[CmdletBinding()]
param(
    [string] $Destination = 'C:\edk2',
    [switch] $InitSubmodules
)

$ErrorActionPreference = 'Stop'
$Destination = [IO.Path]::GetFullPath($Destination)

if (Test-Path (Join-Path $Destination 'edksetup.bat')) {
    Write-Host "EDK2 already present: $Destination"
    Write-Output $Destination
    exit 0
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "git required to clone EDK2"
}

Write-Host "Cloning tianocore/edk2 -> $Destination (shallow)"
git clone --depth 1 https://github.com/tianocore/edk2.git $Destination

Push-Location $Destination
try {
    Write-Host "Initializing MdePkg + BaseTools submodules..."
    git submodule update --init MdePkg MdeModulePkg BaseTools

    Write-Host "Building BaseTools (first run may take several minutes)..."
    py -3 -m pip install --quiet edk2-pytool-library edk2-pytool-extensions regex 2>$null
    cmd /c "edksetup.bat Rebuild" 2>&1 | Out-Null
    py -3 BaseTools\Edk2ToolsBuild.py -t VS2022 2>&1 | Out-Null
}
finally {
    Pop-Location
}

Write-Host "EDK2_ROOT=$Destination"
Write-Output $Destination