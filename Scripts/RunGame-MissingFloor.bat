@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
set "PROJECT_FILE=%PROJECT_ROOT%\IndieGame.uproject"
set "UE_RESOLVER=%PROJECT_ROOT%\Scripts\Resolve-UnrealEditor.ps1"
set "RESOLVED_UE_EDITOR="

for /f "usebackq delims=" %%I in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%UE_RESOLVER%" -ProjectPath "%PROJECT_FILE%" 2^>nul`) do (
    set "RESOLVED_UE_EDITOR=%%I"
)

if not defined RESOLVED_UE_EDITOR (
    echo Unreal Engine matching IndieGame.uproject was not found.
    echo Install the associated engine or set IG_UNREAL_EDITOR to UnrealEditor.exe.
    pause
    exit /b 1
)

rem 없는 층 밤 무대 미리보기: 위층 사람, 그 시간 봉쇄, P1/P2, 낮/밤 순환.
start "Missing Floor" "%RESOLVED_UE_EDITOR%" "%PROJECT_FILE%" -game -windowed -ResX=1600 -ResY=900 -IGListenerGreybox -IGSkipFrontend
endlocal
