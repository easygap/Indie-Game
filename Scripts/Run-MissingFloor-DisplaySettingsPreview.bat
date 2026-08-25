@echo off
setlocal
rem Captures one offscreen still of the display settings screen. That screen
rem used to be evidenced only from a Shipping first run, and repackaging for a
rem copy change is not worth it. Same shape as the audio calibration preview.
rem
rem Comments here stay ASCII on purpose: cmd.exe reads .bat in the system code
rem page, and Korean bytes above the first set= line break parsing outright.
rem
rem   IG_DISPLAY_SETTINGS_ROW      row to park on (0 display mode .. 7 changes)
rem   IG_DISPLAY_SETTINGS_CAPTURE  output PNG path
set "PROJECT_ROOT=%~dp0.."
set "PROJECT_FILE=%PROJECT_ROOT%\IndieGame.uproject"
set "UE_RESOLVER=%PROJECT_ROOT%\Scripts\Resolve-UnrealEditor.ps1"
set "RESOLVED_UE_EDITOR="
set "RUN_LOG=%PROJECT_ROOT%\Saved\Logs\DisplaySettingsPreview.log"
set "CAPTURE_PATH=%PROJECT_ROOT%\Docs\Media\display-settings-1080.png"
if not defined IG_DISPLAY_SETTINGS_RES_X set "IG_DISPLAY_SETTINGS_RES_X=1920"
if not defined IG_DISPLAY_SETTINGS_RES_Y set "IG_DISPLAY_SETTINGS_RES_Y=1080"
if not defined IG_DISPLAY_SETTINGS_ROW set "IG_DISPLAY_SETTINGS_ROW=0"
if defined IG_DISPLAY_SETTINGS_CAPTURE set "CAPTURE_PATH=%IG_DISPLAY_SETTINGS_CAPTURE%"

for /f "usebackq delims=" %%I in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%UE_RESOLVER%" -ProjectPath "%PROJECT_FILE%" -Commandlet 2^>nul`) do (
    set "RESOLVED_UE_EDITOR=%%I"
)
if not defined RESOLVED_UE_EDITOR (
    echo Unreal Engine matching IndieGame.uproject was not found.
    exit /b 1
)
if not exist "%PROJECT_ROOT%\Saved\Logs" mkdir "%PROJECT_ROOT%\Saved\Logs"
if exist "%RUN_LOG%" del /q "%RUN_LOG%"
if exist "%CAPTURE_PATH%" del /q "%CAPTURE_PATH%"

rem No window appears: -RenderOffscreen makes the engine build a Null platform
rem application, so there is no present-to-desktop path at all.
"%RESOLVED_UE_EDITOR%" "%PROJECT_FILE%" /Game/Maps/Prologue_Morning -game -unattended -nosplash -NoLoadingScreen -RenderOffscreen -d3d12 -nosound -Windowed -ResX=%IG_DISPLAY_SETTINGS_RES_X% -ResY=%IG_DISPLAY_SETTINGS_RES_Y% -ForceRes -NoVSync -abslog="%RUN_LOG%" -IGDisplaySettingsPreview -IGDisplaySettingsRow=%IG_DISPLAY_SETTINGS_ROW% -IGDisplaySettingsExpectedWidth=%IG_DISPLAY_SETTINGS_RES_X% -IGDisplaySettingsExpectedHeight=%IG_DISPLAY_SETTINGS_RES_Y% -IGDisplaySettingsScreenshotPath="%CAPTURE_PATH%" %*
set "EDITOR_EXIT=%ERRORLEVEL%"
if not "%EDITOR_EXIT%"=="0" (
    echo Display settings preview exited with code %EDITOR_EXIT%. Check %RUN_LOG%.
    exit /b 1
)

findstr /C:"MISSINGFLOOR_DISPLAY_SETTINGS_PREVIEW PASS" "%RUN_LOG%" >nul
if errorlevel 1 (
    echo Display settings preview did not report PASS. Check %RUN_LOG%.
    exit /b 1
)
if not exist "%CAPTURE_PATH%" (
    echo Display settings preview reported PASS but wrote no capture: %CAPTURE_PATH%
    exit /b 1
)

echo Display settings preview captured: %CAPTURE_PATH%
endlocal
