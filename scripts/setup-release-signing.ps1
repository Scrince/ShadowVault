

[CmdletBinding()]
param(
    [string] $SourceRoot = 'T:\ShadowVault',
    [switch] $Force
)

$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path $SourceRoot).Path
$docsDir = Join-Path $SourceRoot 'docs'
$gnupgHome = Join-Path $SourceRoot '.gnupg-release'
$privateKeysDir = Join-Path $gnupgHome 'private-keys-v1.d'

$pubkeyPath = Join-Path $docsDir 'ShadowVault_Release_Signing_2026_pubkey.asc'
$cerPath = Join-Path $docsDir 'ShadowVault_Local_Code_Signing_2026.cer'
$pfxPath = Join-Path $privateKeysDir 'ShadowVault_Local_Code_Signing_2026.pfx'
$p12Path = Join-Path $privateKeysDir 'ShadowVault_Local_Code_Signing_2026.p12'

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

if (-not $Force) {
    $existing = @($pubkeyPath, $cerPath, $pfxPath, $p12Path) | Where-Object { Test-Path -LiteralPath $_ }
    if ($existing.Count -gt 0) {
        throw "Signing material already exists. Pass -Force to regenerate: $($existing -join ', ')"
    }
}
elseif (Test-Path -LiteralPath $gnupgHome) {
    Write-Host '[RUN] Removing existing GPG homedir for single-key regeneration...'
    $preserve = @()
    foreach ($path in @($pfxPath, $p12Path)) {
        if (Test-Path -LiteralPath $path) {
            $preserve += [ordered]@{
                Path = $path
                Bytes = [IO.File]::ReadAllBytes($path)
            }
        }
    }
    Remove-Item -LiteralPath $gnupgHome -Recurse -Force
    New-Item -ItemType Directory -Force -Path $privateKeysDir | Out-Null
    foreach ($item in $preserve) {
        [IO.File]::WriteAllBytes($item.Path, $item.Bytes)
    }
}

New-Item -ItemType Directory -Force -Path $docsDir, $gnupgHome, $privateKeysDir | Out-Null

$gpg = Find-Gpg
$env:GNUPGHOME = $gnupgHome

$batchPath = Join-Path $env:TEMP 'shadowvault-gpg-batch.txt'
@'
%no-protection
Key-Type: RSA
Key-Length: 4096
Key-Usage: cert,sign
Name-Real: ShadowVault Release Signing (2026)
Name-Email: release@shadowvault.local
Expire-Date: 2y
%commit
%echo done
'@ | Set-Content -LiteralPath $batchPath -Encoding ASCII

Push-Location $SourceRoot
try {
    Write-Host '[RUN] Generating single-key PGP release-signing key (no subkeys)...'
    & $gpg --batch --pinentry-mode loopback --generate-key $batchPath
    if ($LASTEXITCODE -ne 0) { throw 'gpg --generate-key failed' }

    $secretKeys = & $gpg --list-secret-keys --keyid-format long 2>$null
    if (($secretKeys | Select-String -Pattern '^\s*ssb\s').Count -gt 0) {
        throw 'Expected a single signing key, but GPG created subkeys.'
    }

    Write-Host '[RUN] Exporting public PGP key...'
    & $gpg --armor --export 'release@shadowvault.local' 2>$null | Set-Content -LiteralPath $pubkeyPath -Encoding ASCII
    if ($LASTEXITCODE -ne 0) { throw 'gpg --export failed' }

    $publicKeys = & $gpg --show-keys --with-fingerprint $pubkeyPath 2>$null
    if (($publicKeys | Select-String -Pattern '^\s*sub\s').Count -gt 0) {
        throw 'Exported public key still contains subkeys.'
    }

    $fingerprintLine = $publicKeys |
        Where-Object { $_ -match '^\s+[0-9A-F]{4}' } |
        Select-Object -Last 1
    if (-not $fingerprintLine) { throw 'Could not read PGP fingerprint from exported public key' }
    $fingerprint = (($fingerprintLine -replace '\s', '') -replace '\.+$', '')
}
finally {
    Pop-Location
    Remove-Item -LiteralPath $batchPath -Force -ErrorAction SilentlyContinue
}

if (-not $Force -and (Test-Path -LiteralPath $cerPath) -and (Test-Path -LiteralPath $pfxPath) -and (Test-Path -LiteralPath $p12Path)) {
    Write-Host '[SKIP] Keeping existing local code-signing certificate.'
}
else {
Write-Host '[RUN] Creating local code-signing certificate...'
$cert = New-SelfSignedCertificate `
    -Subject 'CN=ShadowVault Local Code Signing' `
    -CertStoreLocation 'Cert:\CurrentUser\My' `
    -KeyAlgorithm RSA `
    -KeyLength 3072 `
    -HashAlgorithm SHA256 `
    -KeyExportPolicy Exportable `
    -Type CodeSigningCert `
    -NotAfter (Get-Date).AddYears(2)

try {
    Export-Certificate -Cert $cert -FilePath $cerPath -Force | Out-Null
    Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password ([securestring]::new()) -Force | Out-Null
    Copy-Item -LiteralPath $pfxPath -Destination $p12Path -Force
}
finally {
    Remove-Item -LiteralPath "Cert:\CurrentUser\My\$($cert.Thumbprint)" -Force
}
}

Hide-GnupgHome $gnupgHome

Write-Host ''
Write-Host 'ShadowVault release signing material ready:' -ForegroundColor Green
Write-Host "  PGP public key:  $pubkeyPath"
Write-Host "  PGP fingerprint: $fingerprint"
Write-Host "  Code-sign cert:  $cerPath"
Write-Host "  PFX (private):   $pfxPath"
Write-Host "  P12 (private):   $p12Path"
Write-Host ''
Write-Host 'Verify the public key:'
Write-Host "  gpg --show-keys $pubkeyPath"