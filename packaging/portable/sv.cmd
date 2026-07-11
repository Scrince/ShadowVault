@echo off
setlocal EnableExtensions
cd /d "%~dp0"
"%~dp0svcli.exe" %*
exit /b %ERRORLEVEL%