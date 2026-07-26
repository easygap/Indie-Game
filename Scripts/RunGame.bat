@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
set "UE_EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

if not exist "%UE_EDITOR%" (
    echo Unreal Engine 5.8 was not found at:
    echo %UE_EDITOR%
    echo.
    echo Install UE 5.8 with Epic Games Launcher, then try again.
    pause
    exit /b 1
)

start "Indie Game" "%UE_EDITOR%" "%PROJECT_ROOT%\IndieGame.uproject" -game -windowed -ResX=1280 -ResY=720
endlocal
