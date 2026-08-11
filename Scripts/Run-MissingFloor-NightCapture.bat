@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
set "PROJECT_FILE=%PROJECT_ROOT%\IndieGame.uproject"
set "UE_RESOLVER=%PROJECT_ROOT%\Scripts\Resolve-UnrealEditor.ps1"
set "RESOLVED_UE_EDITOR="
set "RUN_LOG=%PROJECT_ROOT%\Saved\Logs\MissingFloorNightCapture.log"
if not defined IG_NIGHT_CAPTURE_RES_X set "IG_NIGHT_CAPTURE_RES_X=1920"
if not defined IG_NIGHT_CAPTURE_RES_Y set "IG_NIGHT_CAPTURE_RES_Y=1080"

for /f "usebackq delims=" %%I in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%UE_RESOLVER%" -ProjectPath "%PROJECT_FILE%" -Commandlet 2^>nul`) do (
    set "RESOLVED_UE_EDITOR=%%I"
)

if not defined RESOLVED_UE_EDITOR (
    echo Unreal Engine matching IndieGame.uproject was not found.
    exit /b 1
)

if exist "%RUN_LOG%" del /q "%RUN_LOG%"
if exist "%PROJECT_ROOT%\Saved\NightCapture" rmdir /s /q "%PROJECT_ROOT%\Saved\NightCapture"

rem Offscreen D3D12 render, the same recipe the lens droplet capture uses:
rem real frames without depending on a desktop window. Override the two
rem IG_NIGHT_CAPTURE_RES_* variables for 16:10 or ultrawide visual checks.
"%RESOLVED_UE_EDITOR%" "%PROJECT_FILE%" -game -unattended -nosplash -NoLoadingScreen -RenderOffscreen -d3d12 -nosound -Windowed -ResX=%IG_NIGHT_CAPTURE_RES_X% -ResY=%IG_NIGHT_CAPTURE_RES_Y% -ForceRes -abslog="%RUN_LOG%" -IGListenerGreybox -IGNightCapture -IGSkipFrontend %*
set "EDITOR_EXIT=%ERRORLEVEL%"

if not "%EDITOR_EXIT%"=="0" (
    echo Night capture run exited with code %EDITOR_EXIT%.
    exit /b 1
)

findstr /C:"MISSINGFLOOR_CAPTURE DONE" "%RUN_LOG%" >nul
if errorlevel 1 (
    echo Night capture did not report DONE. Check %RUN_LOG%.
    exit /b 1
)

echo Night capture finished. Assemble GIFs with Make-NightCaptureGifs.ps1.
endlocal
