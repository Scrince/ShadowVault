

[CmdletBinding()]
param(
    [string] $EspImage = 'C:\Temp\shadowvault-pba-esp.img',

    [string] $OvmfCode = 'C:\Program Files\qemu\share\edk2-x86_64-code.fd',

    [string] $OvmfVars = 'C:\Temp\shadowvault-pba-OVMF_VARS.fd',

    [string] $OvmfVarsTemplate = 'C:\Program Files\qemu\share\edk2-i386-vars.fd',

    [string] $EspDir = '',

    [string] $QemuExe = '',

    [string[]] $QemuSearchPaths = @(
        'qemu-system-x86_64',
        'C:\Program Files\qemu\qemu-system-x86_64.exe',
        'C:\Program Files (x86)\qemu\qemu-system-x86_64.exe'
    ),

    [int] $MemoryMiB = 512
)

$ErrorActionPreference = 'Stop'

if (-not $QemuExe) {
    foreach ($candidate in $QemuSearchPaths) {
        if ($candidate -match '[\\/]' -and (Test-Path $candidate)) {
            $QemuExe = $candidate
            break
        }
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($cmd) {
            $QemuExe = $cmd.Source
            break
        }
    }
}

if (-not $QemuExe -or -not (Test-Path $QemuExe)) {
    throw "qemu-system-x86_64 not found — install QEMU or pass -QemuExe"
}

if ($EspDir) {
    if (-not (Test-Path $EspDir)) {
        throw "ESP directory not found: $EspDir (run scripts/pba-prepare-esp-dir.ps1 first)"
    }
}
elseif (-not (Test-Path $EspImage)) {
    throw "ESP image not found: $EspImage (run scripts/pba-make-esp.ps1 or pass -EspDir)"
}

if (-not (Test-Path $OvmfCode)) {
    throw "OVMF code FD not found: $OvmfCode — install QEMU with UEFI firmware or set -OvmfCode"
}

if (-not (Test-Path $OvmfVars)) {
    if (Test-Path $OvmfVarsTemplate) {
        Copy-Item -Force $OvmfVarsTemplate $OvmfVars
    }
    else {
        Copy-Item -Force $OvmfCode $OvmfVars
    }
    Write-Host "Seeded OVMF vars: $OvmfVars"
}


$OvmfCodeLocal = 'C:\Temp\shadowvault-ovmf-code.fd'
$OvmfVarsLocal = 'C:\Temp\shadowvault-ovmf-vars.fd'
New-Item -ItemType Directory -Force -Path 'C:\Temp' | Out-Null
Copy-Item -Force $OvmfCode $OvmfCodeLocal
Copy-Item -Force $OvmfVars $OvmfVarsLocal
foreach ($fd in @($OvmfCodeLocal, $OvmfVarsLocal)) {
    if (-not (Test-Path $fd) -or ((Get-Item $fd).Length -lt 1024)) {
        throw "OVMF staging failed: $fd"
    }
}

Write-Host "Starting QEMU — passphrase scaffold accepts: shadowvault-lab"
if ($EspDir) {
    $FatPath = $EspDir -replace '\\', '/'
    Write-Host "ESP dir (fat:): $EspDir"
    $DataDrive = "file=fat:rw:$FatPath,format=raw"
}
else {
    Write-Host "ESP image: $EspImage"
    $DataDrive = "file=$EspImage,format=raw,if=virtio"
}

& $QemuExe `
    -machine q35 `
    -m $MemoryMiB `
    -cpu max `
    -drive 'if=pflash,format=raw,readonly=on,file=C:/Temp/shadowvault-ovmf-code.fd' `
    -drive 'if=pflash,format=raw,file=C:/Temp/shadowvault-ovmf-vars.fd' `
    -drive $DataDrive `
    -serial stdio `
    -display none