#Requires -Version 5.1
<#
.SYNOPSIS
  Build a single-file Shadow Vault executable for USB / traveler use.

.DESCRIPTION
  Produces one .exe with WinFsp and portable config embedded. On first run it
  writes winfsp-x64.dll and shadowvault.ini beside the executable (hidden
  .shadowvault\ for temp). Copy only ShadowVault.exe to your USB stick.

.EXAMPLE
  .\build-single-exe.ps1 -OutputDir D:\ShadowVault
#>
[CmdletBinding()]
param(
    [string] $BuildDir = 'T:\ShadowVault\build-single',

    [string] $OutputDir = 'T:\ShadowVault\dist',

    [string] $SourceRoot = 'T:\ShadowVault',

    [string] $OutputName = 'ShadowVault.exe'
)

$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path $SourceRoot).Path
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)

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

$cmakeLists = Get-Content (Join-Path $SourceRoot 'CMakeLists.txt') -Raw
if ($cmakeLists -match 'project\s*\(\s*ShadowVault\s+VERSION\s+([\d.]+)') {
    $ProductVersion = $Matches[1]
}
else {
    $ProductVersion = '1.2.0'
}

$winfspDefault = 'C:\Program Files (x86)\WinFsp\bin\winfsp-x64.dll'
$winfspDll = $env:SV_WINFSP_RUNTIME_DLL
if (-not $winfspDll -and (Test-Path $winfspDefault)) {
    $winfspDll = $winfspDefault
}

Write-Host "Building single-file Shadow Vault v$ProductVersion..."

$cmake = Find-CMake
$cmakeArgs = @(
    '-S', $SourceRoot,
    '-B', $BuildDir,
    '-G', 'Visual Studio 17 2022',
    '-A', 'x64',
    '-DSV_BUILD_TESTS=OFF',
    '-DSV_BUILD_CLI=ON',
    '-DSV_BUILD_MOUNT=ON',
    '-DSV_BUILD_PBA=OFF',
    '-DSV_SINGLE_EXE_PORTABLE=ON'
)
if ($winfspDll) {
    $cmakeArgs += "-DSV_WINFSP_RUNTIME_DLL=$winfspDll"
}
else {
    Write-Warning 'WinFsp DLL not found — building CLI-only single exe (mount disabled)'
    $cmakeArgs += '-DSV_BUILD_MOUNT=OFF'
    $cmakeArgs = $cmakeArgs | Where-Object { $_ -ne '-DSV_SINGLE_EXE_PORTABLE=ON' }
}

& $cmake @cmakeArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }

& $cmake --build $BuildDir --config Release --target sv_cli | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'cmake build failed' }

$built = Join-Path $BuildDir 'src\sv_cli\Release\ShadowVault.exe'
if (-not (Test-Path $built)) {
    throw "Build output not found: $built"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$dest = Join-Path $OutputDir $OutputName
Copy-Item -Force $built $dest

$versioned = Join-Path $OutputDir "ShadowVault-$ProductVersion-portable.exe"
Copy-Item -Force $built $versioned

$sizeKb = [math]::Round((Get-Item $dest).Length / 1024)
Write-Host ''
Write-Host 'Single-file portable executable ready:' -ForegroundColor Green
Write-Host "  $dest ($sizeKb KB)"
Write-Host "  $versioned"
Write-Host ''
Write-Host 'Copy ShadowVault.exe to your USB drive — that is the only file required.'
Write-Host 'First run writes shadowvault.ini and vaults\ beside the exe; WinFsp unpacks to %TEMP%.'
Write-Host ''
Write-Host 'Quick test:'
Write-Host "  cd `"$OutputDir`""
Write-Host "  .\ShadowVault.exe --version"
Write-Host "  .\ShadowVault.exe portable-status"
Write-Host '  .\ShadowVault.exe create vaults\test.svlt --size 16M --password "your-passphrase" --quick'