@echo off
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0PerformanceCheck.ps1" -Quality Low -Resolution 1080p
pause
