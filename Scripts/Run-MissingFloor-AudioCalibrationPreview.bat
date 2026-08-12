@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
set "PROJECT_FILE=%PROJECT_ROOT%\IndieGame.uproject"
set "UE_RESOLVER=%PROJECT_ROOT%\Scripts\Resolve-UnrealEditor.ps1"
set "RESOLVED_UE_EDITOR="
set "RUN_LOG=%PROJECT_ROOT%\Saved\Logs\MissingFloorAudioCalibrationPreview.log"
set "CAPTURE_PATH=%PROJECT_ROOT%\Docs\Media\m65-first-run-audio-calibration.png"
if not defined IG_AUDIO_CALIBRATION_RES_X set "IG_AUDIO_CALIBRATION_RES_X=1920"
if not defined IG_AUDIO_CALIBRATION_RES_Y set "IG_AUDIO_CALIBRATION_RES_Y=1080"
if defined IG_AUDIO_CALIBRATION_CAPTURE_PATH set "CAPTURE_PATH=%IG_AUDIO_CALIBRATION_CAPTURE_PATH%"
set "PENDING_CAPTURE_PATH=%CAPTURE_PATH%.pending.png"

for /f "usebackq delims=" %%I in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%UE_RESOLVER%" -ProjectPath "%PROJECT_FILE%" -Commandlet 2^>nul`) do (
    set "RESOLVED_UE_EDITOR=%%I"
)
if not defined RESOLVED_UE_EDITOR (
    echo Unreal Engine matching IndieGame.uproject was not found.
    exit /b 1
)
if not exist "%PROJECT_ROOT%\Saved\Logs" mkdir "%PROJECT_ROOT%\Saved\Logs"
if exist "%RUN_LOG%" del /q "%RUN_LOG%"
if exist "%PENDING_CAPTURE_PATH%" del /q "%PENDING_CAPTURE_PATH%"

"%RESOLVED_UE_EDITOR%" "%PROJECT_FILE%" /Game/Maps/Prologue_Morning -game -unattended -nosplash -NoLoadingScreen -RenderOffscreen -d3d12 -nosound -Windowed -ResX=%IG_AUDIO_CALIBRATION_RES_X% -ResY=%IG_AUDIO_CALIBRATION_RES_Y% -ForceRes -NoVSync -abslog="%RUN_LOG%" -IGAudioCalibrationPreview -IGAudioCalibrationExpectedWidth=%IG_AUDIO_CALIBRATION_RES_X% -IGAudioCalibrationExpectedHeight=%IG_AUDIO_CALIBRATION_RES_Y% -IGAudioCalibrationScreenshotPath="%PENDING_CAPTURE_PATH%" %*
set "EDITOR_EXIT=%ERRORLEVEL%"
if not "%EDITOR_EXIT%"=="0" (
    echo Audio calibration preview exited with code %EDITOR_EXIT%.
    exit /b 1
)
findstr /C:"MISSINGFLOOR_AUDIO_CALIBRATION_PREVIEW PASS" "%RUN_LOG%" >nul
if errorlevel 1 (
    echo Audio calibration preview did not report PASS. Check %RUN_LOG%.
    exit /b 1
)
if not exist "%PENDING_CAPTURE_PATH%" (
    echo Audio calibration capture is missing: %PENDING_CAPTURE_PATH%
    exit /b 1
)
move /y "%PENDING_CAPTURE_PATH%" "%CAPTURE_PATH%" >nul
if errorlevel 1 (
    echo Audio calibration capture could not replace the approved image: %CAPTURE_PATH%
    exit /b 1
)
echo Audio calibration preview passed: %CAPTURE_PATH%
endlocal
