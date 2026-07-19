

[CmdletBinding()]
param(
    [string] $Destination = 'C:\Temp\libsodium-src',
    [string] $Tag = '1.0.20-RELEASE'
)

$ErrorActionPreference = 'Stop'
$Destination = [IO.Path]::GetFullPath($Destination)
$IncludeDir = Join-Path $Destination 'src\libsodium\include'

if (Test-Path (Join-Path $IncludeDir 'sodium.h')) {
    Write-Host "libsodium already present: $IncludeDir"
    Write-Output $Destination
    exit 0
}

$Parent = Split-Path -Parent $Destination
New-Item -ItemType Directory -Force -Path $Parent | Out-Null

if (Get-Command git -ErrorAction SilentlyContinue) {
    Write-Host "Cloning libsodium $Tag -> $Destination"
    git clone --depth 1 --branch $Tag https://github.com/jedisct1/libsodium.git $Destination
}
else {
    throw "git not found; clone libsodium manually to $Destination"
}

if (-not (Test-Path (Join-Path $IncludeDir 'sodium.h'))) {
    throw "sodium.h not found after clone"
}

$VersionTemplate = Join-Path $PSScriptRoot '..\src\sv_pba\efi\ShadowVaultPkg\Library\SvLibsodium\Include\sodium\version.h'
$VersionTarget = Join-Path $IncludeDir 'sodium\version.h'
if (Test-Path $VersionTemplate) {
    Copy-Item -Force $VersionTemplate $VersionTarget
}

Write-Host "LIBSODIUM_ROOT=$Destination"
Write-Host "LIBSODIUM_INCLUDE=$IncludeDir"
Write-Output $Destination