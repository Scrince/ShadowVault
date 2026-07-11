#Requires -Version 5.1
<#
.SYNOPSIS
  Build Shadow Vault production Windows installer (MSI + portable ZIP).

.PARAMETER BuildDir
  CMake build directory (Release).

.PARAMETER OutputDir
  Directory for ShadowVault-x.y.z-win64.msi and .zip.

.PARAMETER IncludePba
  Bundle ShadowVaultBoot.efi and PBA lab scripts as optional MSI feature.

.PARAMETER IncludeGui
  Run windeployqt and bundle svgui when Qt build is present.

.EXAMPLE
  .\build-installer.ps1 -BuildDir C:\Temp\shadowvault-build -IncludePba
#>
[CmdletBinding()]
param(
    [string] $BuildDir = 'C:\Temp\shadowvault-build',

    [string] $OutputDir = 'C:\Temp\shadowvault-dist',

    [string] $SourceRoot = 'T:\ShadowVault',

    [switch] $IncludePba,

    [switch] $IncludeGui,

    [string] $EfiPath = 'C:\edk2\Build\ShadowVaultPkg\RELEASE_VS2022\X64\ShadowVaultBoot.efi'
)

$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path $SourceRoot).Path
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$Staging = Join-Path $env:TEMP ("sv_installer_stage_" + [guid]::NewGuid().ToString('N'))

function Find-CMake {
    $candidates = @(
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'cmake'
    )
    foreach ($c in $candidates) {
        if ($c -eq 'cmake') {
            $cmd = Get-Command cmake -ErrorAction SilentlyContinue
            if ($cmd) { return $cmd.Source }
        }
        elseif (Test-Path $c) { return $c }
    }
    throw 'cmake not found'
}

function Find-WixTool {
    param([string] $Name)
    $roots = @(
        "${env:ProgramFiles(x86)}\WiX Toolset v3.11\bin",
        "${env:ProgramFiles}\WiX Toolset v3.11\bin",
        "${env:ProgramFiles(x86)}\WiX Toolset v3.14\bin"
    )
    foreach ($root in $roots) {
        $path = Join-Path $root "$Name.exe"
        if (Test-Path $path) { return $path }
    }
    return $null
}

function New-PlaceholderIcon {
    param([string] $Path)
    if (Test-Path $Path) { return }
    # Minimal valid 16x16 32bpp ICO (single pixel)
    $bytes = [byte[]]@(
        0,0,1,0,1,0,16,16,0,0,1,0,32,0,40,1,0,0,22,0,0,0,
        40,0,0,0,16,0,0,0,32,0,0,0,1,0,32,0,0,0,0,0,0,1,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,120,215,255,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    )
    [IO.File]::WriteAllBytes($Path, $bytes)
}

# --- Read version from CMake project ---
$cmakeLists = Get-Content (Join-Path $SourceRoot 'CMakeLists.txt') -Raw
if ($cmakeLists -match 'project\s*\(\s*ShadowVault\s+VERSION\s+([\d.]+)') {
    $ProductVersion = $Matches[1]
}
else {
    $ProductVersion = '1.2.0'
}

# WiX/MSI ProductVersion requires exactly four numeric fields (major.minor.build.revision).
$versionParts = $ProductVersion.Split('.')
while ($versionParts.Count -lt 4) { $versionParts += '0' }
$MsiProductVersion = ($versionParts[0..3] -join '.')

Write-Host "Shadow Vault installer build v$ProductVersion"

$cmake = Find-CMake
$cmakeArgs = @(
    '-S', $SourceRoot,
    '-B', $BuildDir,
    '-G', 'Visual Studio 17 2022',
    '-A', 'x64',
    '-DSV_BUILD_TESTS=OFF',
    '-DSV_BUILD_CLI=ON',
    '-DSV_BUILD_MOUNT=ON',
    '-DSV_BUILD_PBA=ON'
)
if ($IncludeGui) {
    $cmakeArgs += '-DSV_BUILD_GUI=ON'
}
& $cmake @cmakeArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }

& $cmake --build $BuildDir --config Release | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'cmake build failed' }

# --- Stage install tree ---
if (Test-Path $Staging) { Remove-Item -Recurse -Force $Staging }
$prefix = Join-Path $Staging 'root'
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'bin') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'doc') | Out-Null

$releaseBin = Join-Path $BuildDir 'src\sv_cli\Release'
$svcli = Join-Path $releaseBin 'svcli.exe'
if (-not (Test-Path $svcli)) {
    throw "svcli.exe not found at $svcli"
}
Copy-Item -Force $svcli (Join-Path $prefix 'bin\svcli.exe')

$winfsp = Join-Path $releaseBin 'winfsp-x64.dll'
if (Test-Path $winfsp) {
    Copy-Item -Force $winfsp (Join-Path $prefix 'bin\winfsp-x64.dll')
}

if ($IncludeGui) {
    $svgui = Join-Path $BuildDir 'src\sv_gui\Release\svgui.exe'
    if (Test-Path $svgui) {
        Copy-Item -Force $svgui (Join-Path $prefix 'bin\svgui.exe')
        $windeployqt = Get-Command windeployqt -ErrorAction SilentlyContinue
        if ($windeployqt) {
            & $windeployqt.Source --no-translations --no-compiler-runtime (Join-Path $prefix 'bin\svgui.exe')
            Write-Host 'Deployed Qt runtime for svgui'
        }
        else {
            Write-Warning 'windeployqt not found — svgui may miss Qt DLLs'
        }
    }
}

Copy-Item -Force (Join-Path $SourceRoot 'LICENSE') (Join-Path $prefix 'doc\LICENSE')
Copy-Item -Force (Join-Path $SourceRoot 'README.md') (Join-Path $prefix 'doc\README.md')
Copy-Item -Force (Join-Path $SourceRoot 'packaging\THIRD_PARTY_NOTICES.txt') (Join-Path $prefix 'doc\THIRD_PARTY_NOTICES.txt')
Copy-Item -Force (Join-Path $SourceRoot 'docs\cli-spec.md') (Join-Path $prefix 'doc\cli-spec.md')
Copy-Item -Force (Join-Path $SourceRoot 'packaging\windows\shadowvault.ini.example') (Join-Path $prefix 'doc\shadowvault.ini.example')

# Flatten staging for heat: MSI installs to INSTALLFOLDER with bin/ + doc/
$flat = Join-Path $Staging 'msi'
New-Item -ItemType Directory -Force -Path (Join-Path $flat 'bin') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $flat 'doc') | Out-Null
Copy-Item -Force (Join-Path $prefix 'bin\*') (Join-Path $flat 'bin\')
Copy-Item -Force (Join-Path $prefix 'doc\*') (Join-Path $flat 'doc\')

Copy-Item -Force (Join-Path $SourceRoot 'packaging\windows\license.rtf') (Join-Path $flat 'license.rtf')
New-PlaceholderIcon (Join-Path $flat 'shadowvault.ico')

if ($IncludePba) {
    $efi = $EfiPath
    if (-not (Test-Path $efi)) {
        throw "PBA EFI not found: $efi (build LabUnlock first)"
    }
    $efiBuild = Split-Path -Parent $efi
    foreach ($root in @($prefix, $flat)) {
        $pbaDir = Join-Path $root 'pba'
        $scriptDir = Join-Path $root 'scripts'
        New-Item -ItemType Directory -Force -Path $pbaDir, $scriptDir | Out-Null
        Copy-Item -Force $efi (Join-Path $pbaDir 'ShadowVaultBoot.efi')
        foreach ($lab in @('LabBootManager.efi', 'LabRescue.efi')) {
            $src = Join-Path $efiBuild $lab
            if (Test-Path $src) { Copy-Item -Force $src (Join-Path $pbaDir $lab) }
        }
        Copy-Item -Force (Join-Path $SourceRoot 'scripts\pba-physical-lab.ps1') $scriptDir
        Copy-Item -Force (Join-Path $SourceRoot 'scripts\pba-prepare-esp-dir.ps1') $scriptDir
        Copy-Item -Force (Join-Path $SourceRoot 'scripts\pba-qemu-smoke.ps1') $scriptDir
    }
}

# --- USB-ready portable release (flat layout) ---
$portableScript = Join-Path $SourceRoot 'scripts\build-portable.ps1'
if (Test-Path $portableScript) {
    & $portableScript -BuildDir $BuildDir -OutputDir $OutputDir -SourceRoot $SourceRoot -SkipBuild
}
else {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
    $zipName = "ShadowVault-$ProductVersion-win64-portable.zip"
    $zipPath = Join-Path $OutputDir $zipName
    if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
    Compress-Archive -Path (Join-Path $prefix '*') -DestinationPath $zipPath -Force
    Write-Host "Portable ZIP: $zipPath"
}

# --- MSI via WiX ---
$candle = Find-WixTool 'candle'
$light = Find-WixTool 'light'
$heat = Find-WixTool 'heat'

if (-not ($candle -and $light -and $heat)) {
    Write-Warning 'WiX Toolset not found — MSI skipped. Install WiX 3.11+ or use portable ZIP.'
    Write-Host "Done (ZIP only). Staging: $flat"
    return
}

$wxsDir = Join-Path $Staging 'wxs'
New-Item -ItemType Directory -Force -Path $wxsDir | Out-Null
$harvested = Join-Path $wxsDir 'harvested.wxs'
if ($IncludePba) {
    $productWxs = Join-Path $SourceRoot 'packaging\windows\ShadowVault.wxs'
    # PBA/scripts are explicit WiX components — heat only bin/ + doc/ to avoid ICE30 duplicates.
    $heatFlat = Join-Path $Staging 'heat'
    New-Item -ItemType Directory -Force -Path (Join-Path $heatFlat 'bin'), (Join-Path $heatFlat 'doc') | Out-Null
    Copy-Item -Force (Join-Path $flat 'bin\*') (Join-Path $heatFlat 'bin\')
    Copy-Item -Force (Join-Path $flat 'doc\*') (Join-Path $heatFlat 'doc\')
    Copy-Item -Force (Join-Path $flat 'license.rtf') $heatFlat
    Copy-Item -Force (Join-Path $flat 'shadowvault.ico') $heatFlat
}
else {
    $productWxs = Join-Path $SourceRoot 'packaging\windows\ShadowVault-core.wxs'
    $heatFlat = $flat
}

& $heat dir $heatFlat `
    -cg HarvestedComponents `
    -dr INSTALLFOLDER `
    -var var.StagingDir `
    -arch x64 `
    -gg -sfrag -srd -scom -ke `
    -out $harvested
if ($LASTEXITCODE -ne 0) { throw 'heat failed' }
$wixObj = Join-Path $wxsDir 'output'
New-Item -ItemType Directory -Force -Path $wixObj | Out-Null

& $candle -nologo `
    "-dProductVersion=$MsiProductVersion" `
    "-dStagingDir=$flat" `
    -arch x64 `
    -out $wixObj\ `
    -ext WixUtilExtension `
    $productWxs $harvested
if ($LASTEXITCODE -ne 0) { throw 'candle failed' }

$msiName = "ShadowVault-$ProductVersion-win64.msi"
$msiPath = Join-Path $OutputDir $msiName
$productObj = Get-ChildItem $wixObj -Filter '*.wixobj' |
    Where-Object { $_.Name -notmatch 'harvested' } |
    Select-Object -First 1
& $light -nologo `
    -ext WixUIExtension `
    -ext WixUtilExtension `
    -out $msiPath `
    $productObj.FullName `
    (Join-Path $wixObj 'harvested.wixobj')
if ($LASTEXITCODE -ne 0) { throw 'light failed' }

Write-Host "Production MSI: $msiPath"
Write-Host "Install (elevated): msiexec /i `"$msiPath`""

Remove-Item -Recurse -Force $Staging -ErrorAction SilentlyContinue