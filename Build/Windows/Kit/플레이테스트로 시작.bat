@echo off
cd /d "%~dp0.."
start "" /wait "MissingFloor.exe" -IGPlayRecord
if exist "%LOCALAPPDATA%\IndieGame\Saved\PlayRecords" explorer "%LOCALAPPDATA%\IndieGame\Saved\PlayRecords"
