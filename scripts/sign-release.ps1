#Requires -Version 5.1
<#
.SYNOPSIS
  Sign ShadowVault release artifacts with the project PGP key.

.DESCRIPTION
  Creates detached ASCII-armored signatures (.asc) beside release binaries and
  writes docs/SHA256SUMS plus docs/SHA256SUMS.asc.
#>
[CmdletBinding()]
param(
    [string] $SourceRoot = 'T:\ShadowVault',
    [string] $DistDir = 'T:\ShadowVault\dist',
    [switch] $SignAuthenticode
)

$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path $SourceRoot).Path
$DistDir = (Resolve-Path $DistDir).Path
$docsDir = Join-Path $SourceRoot 'docs'
$gnupgHome = Join-Path $SourceRoot '.gnupg-release'
$pubkeyPath = Join-Path $docsDir 'ShadowVault_Release_Signing_2026_pubkey.asc'
$pfxPath = Join-Path $gnupgHome 'private-keys-v1.d\ShadowVault_Local_Code_Signing_2026.pfx'

function Hide-GnupgHome {
    param([string]$Path)
    if (Test-Path -LiteralPath $Path) {
        attrib +h -s $Path | Out-Null
    }
}

function Find-Gpg {
    $candidates = @(
        'C:\Program Files\GnuPG\bin\gpg.exe',
        'C:\Program Files (x86)\GnuPG\bin\gpg.exe',
        'C:\Program Files\Git\usr\bin\gpg.exe'
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw 'gpg not found. Install GnuPG or Git for Windows.'
}

function Find-SignTool {
    $kits = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending
    foreach ($kit in $kits) {
        $candidate = Join-Path $kit.FullName 'x64\signtool.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    $cmd = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

if (-not (Test-Path -LiteralPath $pubkeyPath)) {
    throw "Missing public key: $pubkeyPath. Run scripts/setup-release-signing.ps1 first."
}
if (-not (Test-Path -LiteralPath $gnupgHome)) {
    throw "Missing GPG homedir: $gnupgHome. Run scripts/setup-release-signing.ps1 first."
}

$gpg = Find-Gpg
$env:GNUPGHOME = $gnupgHome

$artifactPatterns = @('*.exe', '*.msi', '*.zip')
$artifacts = @()
foreach ($pattern in $artifactPatterns) {
    $artifacts += Get-ChildItem -LiteralPath $DistDir -Filter $pattern -File -ErrorAction SilentlyContinue
}
$artifacts = $artifacts | Sort-Object Name -Unique
if ($artifacts.Count -eq 0) {
    throw "No release artifacts found in $DistDir"
}

if ($SignAuthenticode) {
    if (-not (Test-Path -LiteralPath $pfxPath)) {
        throw "Missing PFX for Authenticode signing: $pfxPath"
    }
    $signtool = Find-SignTool
    if (-not $signtool) { throw 'signtool.exe not found. Install the Windows SDK.' }

    Write-Host '[RUN] Authenticode signatures...'
    foreach ($artifact in $artifacts) {
        if ($artifact.Extension -notin '.exe', '.msi') { continue }
        & $signtool sign /fd SHA256 /f $pfxPath /tr http://timestamp.digicert.com /td SHA256 /d "ShadowVault" $artifact.FullName
        if ($LASTEXITCODE -ne 0) { throw "signtool failed for $($artifact.Name)" }
        $status = (Get-AuthenticodeSignature -LiteralPath $artifact.FullName).Status
        Write-Host "[PASS] $($artifact.Name) Authenticode=$status"
    }
}

Push-Location $SourceRoot
try {
    Write-Host '[RUN] Detached PGP signatures...'
    foreach ($artifact in $artifacts) {
        $sigPath = "$($artifact.FullName).asc"
        & $gpg --batch --yes --pinentry-mode loopback --armor --detach-sign --output $sigPath $artifact.FullName
        if ($LASTEXITCODE -ne 0) { throw "gpg detach-sign failed for $($artifact.Name)" }
        Write-Host "[PASS] $($artifact.Name).asc"
    }
}
finally {
    Pop-Location
}

$manifestEntries = @()
$manifestEntries += [ordered]@{
    Path = 'docs/ShadowVault_Release_Signing_2026_pubkey.asc'
    File = Get-Item -LiteralPath $pubkeyPath
}
foreach ($artifact in $artifacts) {
    $manifestEntries += [ordered]@{
        Path = "dist/$($artifact.Name)"
        File = $artifact
    }
    $sig = Get-Item -LiteralPath "$($artifact.FullName).asc"
    $manifestEntries += [ordered]@{
        Path = "dist/$($artifact.Name).asc"
        File = $sig
    }
}

$shaPath = Join-Path $docsDir 'SHA256SUMS'
$lines = foreach ($entry in $manifestEntries) {
    $hash = (Get-FileHash -LiteralPath $entry.File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $($entry.Path)"
}
$lines | Set-Content -LiteralPath $shaPath -Encoding ASCII -NoNewline:$false
if (-not ($lines[-1].EndsWith("`n"))) {
    Add-Content -LiteralPath $shaPath -Value '' -Encoding ASCII
}

$shaAscPath = "$shaPath.asc"
Push-Location $SourceRoot
try {
    & $gpg --batch --yes --pinentry-mode loopback --armor --detach-sign --output $shaAscPath $shaPath
    if ($LASTEXITCODE -ne 0) { throw 'gpg detach-sign failed for SHA256SUMS' }
}
finally {
    Pop-Location
}

Hide-GnupgHome $gnupgHome

Write-Host ''
Write-Host 'Release signing complete:' -ForegroundColor Green
Write-Host "  Manifest: $shaPath"
Write-Host "  Manifest signature: $shaAscPath"
Write-Host ''
Write-Host 'Verify:'
Write-Host "  gpg --import $pubkeyPath"
Write-Host "  gpg --verify $shaAscPath $shaPath"