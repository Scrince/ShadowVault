#Requires -Version 5.1
<#
.SYNOPSIS
  End-to-end smoke test for Shadow Vault CLI (non-interactive).
#>
[CmdletBinding()]
param(
    [string] $Svcli = 'C:\Temp\shadowvault-build\src\sv_cli\Release\svcli.exe',
    [string] $WorkDir = 'C:\Temp\sv_e2e_smoke'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $Svcli)) { throw "svcli not found: $Svcli" }

New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
$vault = Join-Path $WorkDir 'e2e.svlt'
$localIn = Join-Path $WorkDir 'input.txt'
$localOut = Join-Path $WorkDir 'output.txt'
$pw = 'e2e-test-password-16chars'
$hiddenPw = 'hidden-e2e-pass-12'
$mountLetter = 'Y:'
function Invoke-Sv {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]] $CliArgs)
    $out = & $Svcli @CliArgs 2>&1
    $code = $LASTEXITCODE
    return @{ Out = ($out | Out-String); Code = $code }
}

function Assert-Ok($result, [string] $step) {
    if ($result.Code -ne 0) {
        throw "$step failed (exit $($result.Code)): $($result.Out)"
    }
}

function Assert-Fail($result, [int] $expected, [string] $step) {
    if ($result.Code -ne $expected) {
        throw "$step expected exit $expected, got $($result.Code): $($result.Out)"
    }
}

Remove-Item $vault -Force -ErrorAction SilentlyContinue
Remove-Item $localOut -Force -ErrorAction SilentlyContinue
'hello shadow vault e2e' | Set-Content -Path $localIn -NoNewline -Encoding utf8

Write-Host '[1/11] version'
$r = Invoke-Sv -CliArgs '--version'
Assert-Ok $r 'version'
if ($r.Out -notmatch '1\.2\.0') { throw 'version string unexpected' }

Write-Host '[2/11] create'
$r = Invoke-Sv -CliArgs 'create', $vault, '--size', '128M', '--password', $pw, '--quick'
Assert-Ok $r 'create'
if (-not (Test-Path $vault)) { throw 'vault file missing after create' }

Write-Host '[3/11] wrong password'
$r = Invoke-Sv -CliArgs 'unlock', $vault, '--password', 'wrong-password-here'
Assert-Fail $r 3 'wrong password'

Write-Host '[4/11] put'
$r = Invoke-Sv -CliArgs '--vault', $vault, 'put', $localIn, '/docs/readme.txt', '--password', $pw
Assert-Ok $r 'put'

Write-Host '[5/11] list'
$r = Invoke-Sv -CliArgs '--vault', $vault, 'list', '/', '--password', $pw
Assert-Ok $r 'list'
if ($r.Out -notmatch 'docs') { throw "list missing docs dir: $($r.Out)" }

Write-Host '[6/11] get'
$r = Invoke-Sv -CliArgs '--vault', $vault, 'get', '/docs/readme.txt', $localOut, '--password', $pw
Assert-Ok $r 'get'
$got = Get-Content $localOut -Raw
if ($got -ne (Get-Content $localIn -Raw)) { throw 'get content mismatch' }

Write-Host '[7/11] mkdir + rename'
$r = Invoke-Sv -CliArgs '--vault', $vault, 'mkdir', '/archive', '--password', $pw
Assert-Ok $r 'mkdir'
$r = Invoke-Sv -CliArgs '--vault', $vault, 'rename', '/docs/readme.txt', '/archive/readme.txt', '--password', $pw
Assert-Ok $r 'rename'

Write-Host '[8/11] verify --deep'
$r = Invoke-Sv -CliArgs '--vault', $vault, 'verify', '--deep', '--password', $pw
Assert-Ok $r 'verify'

Write-Host '[9/11] create-hidden'
$r = Invoke-Sv -CliArgs @(
    'create-hidden', $vault, '--size', '32M',
    '--password', $pw, '--new-password', $hiddenPw
)
Assert-Ok $r 'create-hidden'

Write-Host '[10/11] portable-status'
$r = Invoke-Sv -CliArgs 'portable-status'
Assert-Ok $r 'portable-status'

Write-Host '[11/11] mount OS roundtrip (if Y: free)'
$useMount = -not (Test-Path $mountLetter)
if ($useMount) {
    $mountProc = Start-Process -FilePath $Svcli -ArgumentList @(
        'mount', $vault, '--point', $mountLetter, '--password', $pw
    ) -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 2
    try {
        if (-not (Test-Path $mountLetter)) { throw 'mount point not visible' }
        $osFile = Join-Path $mountLetter 'cli_write.txt'
        'written via explorer path' | Set-Content -Path $osFile -NoNewline -Encoding utf8
        Start-Sleep -Milliseconds 500
        $mounted = Get-Content $osFile -Raw
        if ($mounted -ne 'written via explorer path') { throw 'mount write roundtrip mismatch' }
    }
    finally {
        if (-not $mountProc.HasExited) {
            Stop-Process -Id $mountProc.Id -Force -ErrorAction SilentlyContinue
        }
        Start-Sleep -Milliseconds 500
    }
}
else {
    Write-Host '  SKIP: mount letter in use'
}

Write-Host ''
Write-Host 'E2E smoke PASSED' -ForegroundColor Green