@echo off
rem  Starts TPFanControl from this folder with administrator rights.
setlocal
cd /d "%~dp0"
powershell -NoProfile -Command "Start-Process -FilePath '%~dp0TPFanControl.exe' -WorkingDirectory '%~dp0' -Verb RunAs" 2>nul
if errorlevel 1 (
    echo.
    echo   Cancelled or refused. The fan stays under BIOS control.
    echo.
    pause
)
