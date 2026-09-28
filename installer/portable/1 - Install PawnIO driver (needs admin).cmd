@echo off
rem  Installs the PawnIO driver once on this computer (Windows asks for admin).
setlocal
cd /d "%~dp0"
echo.
echo   Installing the PawnIO driver. Windows will ask for administrator rights.
echo.
powershell -NoProfile -Command "Start-Process -FilePath '%~dp0PawnIO\PawnIO_setup.exe' -ArgumentList '-install' -Verb RunAs -Wait" 2>nul
if errorlevel 1 (
    echo   Cancelled or refused. Nothing was installed.
) else (
    echo   Done. Now double-click "2 - Run TPFanControl (needs admin).cmd".
)
echo.
pause
