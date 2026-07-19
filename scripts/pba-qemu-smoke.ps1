

[CmdletBinding()]
param(
    [string] $EfiPath = 'C:\edk2\Build\ShadowVaultPkg\RELEASE_VS2022\X64\ShadowVaultBoot.efi',

    [string] $EspDir = 'C:\Temp\shadowvault-pba-esp',

    [string] $OvmfCode = 'C:\Program Files\qemu\share\edk2-x86_64-code.fd',

    [string] $OvmfVarsTemplate = 'C:\Program Files\qemu\share\edk2-i386-vars.fd',

    [string] $OvmfVars = 'C:\Temp\shadowvault-pba-OVMF_VARS.fd',

    [string] $LogFile = 'C:\Temp\shadowvault-pba-qemu-serial.log',

    [int] $TimeoutSec = 30,

    [switch] $TestUnlock,

    [switch] $TestChainLoad,

    [switch] $TestWrongPassword,

    [switch] $TestRescue,

    [string] $LabPassphrase = 'shadowvault-lab',

    [int] $SerialPort = 45678,

    [int] $UnlockTimeoutSec = 180
)

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $PSCommandPath


$LogFile = [IO.Path]::GetFullPath($LogFile)
$LogParent = Split-Path -Parent $LogFile
if ($LogParent -and -not (Test-Path $LogParent)) {
    New-Item -ItemType Directory -Force -Path $LogParent | Out-Null
}

if (-not (Test-Path $EfiPath)) {
    throw "EFI not found: $EfiPath"
}

$BuildDir = Split-Path -Parent $EfiPath
& (Join-Path $ScriptDir 'pba-prepare-esp-dir.ps1') `
    -EfiPath $EfiPath `
    -OutputDir $EspDir `
    -IncludeLabVectors `
    -BootMgrPath (Join-Path $BuildDir 'LabBootManager.efi') `
    -RescueEfiPath (Join-Path $BuildDir 'LabRescue.efi') `
    -ForceWrongPasswordTest:$TestWrongPassword `
    -ForceRescueTest:$TestRescue | Out-Null

$QemuCandidates = @(
    'C:\Program Files\qemu\qemu-system-x86_64.exe',
    'C:\Program Files (x86)\qemu\qemu-system-x86_64.exe',
    'qemu-system-x86_64'
)
$QemuExe = $null
foreach ($candidate in $QemuCandidates) {
    if ($candidate -match '[\\/]' -and (Test-Path $candidate)) {
        $QemuExe = (Resolve-Path $candidate).Path
        break
    }
    $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($cmd) {
        $QemuExe = $cmd.Source
        break
    }
}
if (-not $QemuExe) {
    throw 'qemu-system-x86_64 not found — install QEMU (choco install qemu)'
}

if (-not (Test-Path $OvmfVars)) {
    if (Test-Path $OvmfVarsTemplate) {
        Copy-Item -Force $OvmfVarsTemplate $OvmfVars
    }
    else {
        Copy-Item -Force $OvmfCode $OvmfVars
    }
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

if (Test-Path $LogFile) {
    Remove-Item -Force $LogFile
}

$FatPath = $EspDir -replace '\\', '/'
$LogPath = $LogFile -replace '\\', '/'

Get-Process qemu-system-x86_64 -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue

if ($TestUnlock -or $TestChainLoad -or $TestWrongPassword -or $TestRescue) {
    $modeLabel = if ($TestRescue) { 'rescue' } elseif ($TestWrongPassword) { 'wrong-password' } elseif ($TestChainLoad) { 'chain-load' } else { 'unlock' }
    Write-Host "QEMU $modeLabel test (TCP serial :$SerialPort, ${UnlockTimeoutSec}s timeout)"
    $serialArg = "tcp:127.0.0.1:${SerialPort},server,nowait"
}
else {
    Write-Host "QEMU smoke ($TimeoutSec s) -> $LogFile"
    $serialArg = "file:$LogPath"
}

$memoryMiB = if ($TestUnlock -or $TestChainLoad -or $TestWrongPassword -or $TestRescue) { 1024 } else { 512 }
$qemuArgs = @(
    '-machine', 'q35',
    '-m', "$memoryMiB",
    '-cpu', 'max',
    '-drive', 'if=pflash,format=raw,readonly=on,file=C:/Temp/shadowvault-ovmf-code.fd',
    '-drive', 'if=pflash,format=raw,file=C:/Temp/shadowvault-ovmf-vars.fd',
    '-drive', "file=fat:rw:$FatPath,format=raw",
    '-serial', $serialArg,
    '-display', 'none',
    '-no-reboot'
) -join ' '

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $QemuExe
$psi.Arguments = $qemuArgs
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$proc = [System.Diagnostics.Process]::Start($psi)

if ($TestUnlock -or $TestChainLoad -or $TestWrongPassword -or $TestRescue) {
    try {
        $client = $null
        for ($attempt = 0; $attempt -lt 60; ++$attempt) {
            try {
                $client = New-Object System.Net.Sockets.TcpClient
                $client.Connect('127.0.0.1', $SerialPort)
                break
            }
            catch {
                if ($proc.HasExited) {
                    throw 'QEMU exited before serial TCP became available'
                }
                Start-Sleep -Seconds 1
            }
        }
        if (-not $client -or -not $client.Connected) {
            throw "Could not connect to QEMU serial TCP port $SerialPort"
        }

        $stream = $client.GetStream()
        $buffer = New-Object byte[] 4096
        $transcript = New-Object System.Text.StringBuilder
        $sentMenu = $false
        $sentPassphrase = $false
        $readyForPassphrase = $false
        $deadline = (Get-Date).AddSeconds($UnlockTimeoutSec)

        while ((Get-Date) -lt $deadline) {
            if ($stream.DataAvailable) {
                $read = $stream.Read($buffer, 0, $buffer.Length)
                if ($read -gt 0) {
                    $chunk = [System.Text.Encoding]::ASCII.GetString($buffer, 0, $read)
                    [void]$transcript.Append($chunk)
                    $text = $transcript.ToString()

                    if (-not $sentMenu -and $text -match 'Choice:') {
                        Start-Sleep -Milliseconds 200
                        $payload = [System.Text.Encoding]::ASCII.GetBytes("1`r")
                        $stream.Write($payload, 0, $payload.Length)
                        $stream.Flush()
                        $sentMenu = $true
                        Write-Host 'Sent menu choice 1 (boot) on serial'
                    }

                    if ($text -match '\[lab auto-boot\]') {
                        $sentMenu = $true
                    }

                    if ($text -match '\[lab auto-rescue\]') {
                        $sentMenu = $true
                    }

                    if ($text -match '\[lab serial input\]') {
                        $readyForPassphrase = $true
                    }

                    if (-not $sentPassphrase -and $readyForPassphrase) {
                        Start-Sleep -Milliseconds 200
                        $payload = [System.Text.Encoding]::ASCII.GetBytes("${LabPassphrase}`r")
                        $stream.Write($payload, 0, $payload.Length)
                        $stream.Flush()
                        $sentPassphrase = $true
                        Write-Host 'Sent lab passphrase on serial'
                    }

                    if ($TestRescue -and $text -match 'LAB_RESCUE_OK') {
                        Set-Content -Path $LogFile -Value $text -NoNewline
                        Write-Host 'OK: Rescue chain-load succeeded in QEMU'
                        Write-Host "Log: $LogFile"
                        return
                    }

                    if ($TestWrongPassword -and $text -match 'Unlock failed\.') {
                        Set-Content -Path $LogFile -Value $text -NoNewline
                        Write-Host 'OK: Uniform unlock failure in QEMU (P1-03)'
                        Write-Host "Log: $LogFile"
                        return
                    }

                    if ($TestChainLoad -and $text -match 'LAB_BOOTMGR_OK') {
                        Set-Content -Path $LogFile -Value $text -NoNewline
                        Write-Host 'OK: Chain-load to lab bootmgfw succeeded in QEMU'
                        Write-Host "Log: $LogFile"
                        return
                    }

                    if (-not $TestChainLoad -and $text -match 'Freestanding unlock OK') {
                        Set-Content -Path $LogFile -Value $text -NoNewline
                        Write-Host 'OK: Freestanding unlock succeeded in QEMU'
                        Write-Host "Log: $LogFile"
                        return
                    }
                    if ($text -match 'Stub unlock OK' -and -not $TestChainLoad) {
                        Set-Content -Path $LogFile -Value $text -NoNewline
                        Write-Host 'OK: Stub unlock succeeded in QEMU'
                        Write-Host "Log: $LogFile"
                        return
                    }
                    if ($text -match 'Unlock failed') {
                        Write-Host '--- serial transcript (tail) ---'
                        Write-Host ($text.Substring([Math]::Max(0, $text.Length - 1200)))
                        throw 'Unlock failed in QEMU serial output'
                    }
                    if ($text -match 'Chain-load failed') {
                        Write-Host '--- serial transcript (tail) ---'
                        Write-Host ($text.Substring([Math]::Max(0, $text.Length - 1200)))
                        throw 'Chain-load failed in QEMU serial output'
                    }
                }
            }
            elseif ($proc.HasExited) {
                break
            }
            else {
                Start-Sleep -Milliseconds 100
            }
        }

        $text = $transcript.ToString()
        if ($text.Length -gt 0) {
            Set-Content -Path $LogFile -Value $text -NoNewline
            Write-Host '--- serial transcript (tail) ---'
            Write-Host ($text.Substring([Math]::Max(0, $text.Length - 1200)))
        }
        $expect = if ($TestRescue) { 'LAB_RESCUE_OK' } elseif ($TestWrongPassword) { 'Unlock failed.' } elseif ($TestChainLoad) { 'LAB_BOOTMGR_OK' } else { 'Freestanding unlock OK' }
        throw "Timed out waiting for $expect in serial output"
    }
    finally {
        if ($proc -and -not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

Start-Sleep -Seconds $TimeoutSec
if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
$logReady = $false
for ($attempt = 0; $attempt -lt 5; ++$attempt) {
    Start-Sleep -Seconds 1
    if ((Test-Path $LogFile) -and ((Get-Item $LogFile).Length -gt 0)) {
        $logReady = $true
        break
    }
}

if (-not $logReady) {
    throw "No serial log produced at $LogFile (QEMU may have failed to open the serial file)"
}

$log = Get-Content -Raw -Path $LogFile
if ($log -notmatch 'ShadowVault PBA') {
    Write-Host '--- serial log (tail) ---'
    Get-Content $LogFile -Tail 40
    throw 'Banner not found in serial output'
}

Write-Host 'OK: ShadowVault PBA banner seen in QEMU serial log'
Write-Host "Log: $LogFile"