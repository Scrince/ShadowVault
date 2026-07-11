@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title Shadow Vault Portable
echo Shadow Vault Portable — run commands from this window.
echo.
"%~dp0svcli.exe" --version
echo.
echo Examples:
echo   svcli create vaults\secret.svlt --size 64M --password "your-passphrase"
echo   svcli --vault vaults\secret.svlt list / --password "your-passphrase"
echo   svcli portable-status
echo.
echo Type: svcli --help
echo.
cmd /k "cd /d %~dp0"