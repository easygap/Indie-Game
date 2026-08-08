@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
set "PROJECT_FILE=%PROJECT_ROOT%\IndieGame.uproject"
set "UE_RESOLVER=%PROJECT_ROOT%\Scripts\Resolve-UnrealEditor.ps1"
set "RESOLVED_UE_EDITOR="
set "RUN_LOG=%PROJECT_ROOT%\Saved\Logs\MissingFloorGreybox.log"

for /f "usebackq delims=" %%I in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%UE_RESOLVER%" -ProjectPath "%PROJECT_FILE%" -Commandlet 2^>nul`) do (
    set "RESOLVED_UE_EDITOR=%%I"
)

if not defined RESOLVED_UE_EDITOR (
    echo Unreal Engine matching IndieGame.uproject was not found.
    echo Install the associated engine or set IG_UNREAL_EDITOR to its editor binary.
    exit /b 1
)

if not exist "%PROJECT_ROOT%\Saved\Logs" mkdir "%PROJECT_ROOT%\Saved\Logs"
if not exist "%PROJECT_ROOT%\Saved\Logs" (
    echo Could not create the Missing Floor greybox log directory.
    exit /b 1
)

if exist "%RUN_LOG%" del /q "%RUN_LOG%"
if exist "%RUN_LOG%" (
    echo Could not remove the previous Missing Floor greybox log.
    exit /b 1
)

"%RESOLVED_UE_EDITOR%" "%PROJECT_FILE%" -game -unattended -nosplash -nullrhi -nosound -RenderOffscreen -stdout -FullStdOutLogOutput -abslog="%RUN_LOG%" -IGListenerGreybox -IGListenerGreyboxProbe
set "EDITOR_EXIT=%ERRORLEVEL%"

if not "%EDITOR_EXIT%"=="0" (
    echo Unreal Editor exited with code %EDITOR_EXIT%.
    exit /b 1
)

if not exist "%RUN_LOG%" (
    echo Missing Floor greybox log was not created.
    exit /b 1
)

findstr /C:"MISSINGFLOOR_GREYBOX FAIL" "%RUN_LOG%" >nul
if not errorlevel 1 (
    echo Missing Floor greybox validation reported a failure. Check %RUN_LOG%.
    exit /b 1
)

findstr /C:"MISSINGFLOOR_GREYBOX PASS" "%RUN_LOG%" >nul
if errorlevel 1 (
    echo Missing Floor greybox validation did not report PASS. Check %RUN_LOG%.
    exit /b 1
)

echo Missing Floor greybox validation passed.
endlocal
