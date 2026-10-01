@echo off
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0PerformanceCheck.ps1" -Quality High -Resolution 1080p
pause
