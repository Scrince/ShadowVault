

[CmdletBinding()]
param(
    [string] $BuildDir = 'C:\Temp\shadowvault-build',

    [string] $OutputDir = 'C:\Temp\shadowvault-dist',

    [string] $SourceRoot = 'T:\ShadowVault',

    [switch] $SkipBuild
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

$releaseBin = Join-Path $BuildDir 'src\sv_cli\Release'
$svcli = Join-Path $releaseBin 'svcli.exe'

if (-not $SkipBuild -or -not (Test-Path $svcli)) {
    Write-Host "Building Shadow Vault v$ProductVersion (Release)..."
    $cmake = Find-CMake
    & $cmake -S $SourceRoot -B $BuildDir -G 'Visual Studio 17 2022' -A x64 `
        -DSV_BUILD_TESTS=OFF -DSV_BUILD_CLI=ON -DSV_BUILD_MOUNT=ON -DSV_BUILD_PBA=OFF | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }
    & $cmake --build $BuildDir --config Release --target sv_cli | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'cmake build failed' }
}

if (-not (Test-Path $svcli)) {
    throw "svcli.exe not found at $svcli"
}

$folderName = "ShadowVault-$ProductVersion-win64-portable"
$stageRoot = Join-Path $OutputDir $folderName
if (Test-Path $stageRoot) { Remove-Item -Recurse -Force $stageRoot }
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null


Copy-Item -Force $svcli (Join-Path $stageRoot 'svcli.exe')
$winfsp = Join-Path $releaseBin 'winfsp-x64.dll'
if (Test-Path $winfsp) {
    Copy-Item -Force $winfsp (Join-Path $stageRoot 'winfsp-x64.dll')
}
else {
    Write-Warning 'winfsp-x64.dll not found — mount will not work until WinFsp DLL is copied beside svcli.exe'
}


$portablePack = Join-Path $SourceRoot 'packaging\portable'
Copy-Item -Force (Join-Path $portablePack 'shadowvault.ini') (Join-Path $stageRoot 'shadowvault.ini')
Copy-Item -Force (Join-Path $portablePack 'README-USB.txt') (Join-Path $stageRoot 'README-USB.txt')
Copy-Item -Force (Join-Path $portablePack 'sv.cmd') (Join-Path $stageRoot 'sv.cmd')
Copy-Item -Force (Join-Path $portablePack 'Start-ShadowVault.cmd') (Join-Path $stageRoot 'Start-ShadowVault.cmd')


foreach ($dir in @('vaults', 'keyfiles', '.shadowvault\temp')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $stageRoot $dir) | Out-Null
}


$docDir = Join-Path $stageRoot 'doc'
New-Item -ItemType Directory -Force -Path $docDir | Out-Null
foreach ($doc in @('LICENSE', 'README.md')) {
    Copy-Item -Force (Join-Path $SourceRoot $doc) (Join-Path $docDir $doc)
}
Copy-Item -Force (Join-Path $SourceRoot 'packaging\THIRD_PARTY_NOTICES.txt') (Join-Path $docDir 'THIRD_PARTY_NOTICES.txt')
Copy-Item -Force (Join-Path $SourceRoot 'docs\cli-spec.md') (Join-Path $docDir 'cli-spec.md')


New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$zipPath = Join-Path $OutputDir "$folderName.zip"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path $stageRoot -DestinationPath $zipPath -Force

Write-Host ''
Write-Host 'Portable release ready:' -ForegroundColor Green
Write-Host "  Folder: $stageRoot"
Write-Host "  ZIP:    $zipPath"
Write-Host ''
Write-Host 'Copy the folder (or extract the ZIP) to your USB drive.'
Write-Host 'Run Start-ShadowVault.cmd or: svcli --version'
Write-Host ''
Write-Host 'Verify portable mode:'
Write-Host "  cd `"$stageRoot`""
Write-Host '  .\svcli.exe portable-status'