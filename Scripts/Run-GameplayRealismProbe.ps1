[CmdletBinding()]
param([ValidateSet(30, 60, 120)][int]$FrameRate = 60)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $projectRoot 'IndieGame.uproject'
$editor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -Commandlet
$logPath = Join-Path $projectRoot "Saved/Logs/GameplayRealism-$FrameRate.log"
& $editor $projectPath -game -unattended -nosplash -nullrhi -nosound -NoLoadingScreen `
	-IGGameplayRealismProbe -IGSkipFrontend -UseFixedTimeStep "-FPS=$FrameRate" "-abslog=$logPath" | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $logPath -Pattern 'REALISM_PROBE PASS failures=0')) {
	Select-String -LiteralPath $logPath -Pattern 'REALISM_' | ForEach-Object { Write-Host $_.Line }
	throw "게임플레이 검사에 실패했습니다: $logPath"
}
Select-String -LiteralPath $logPath -Pattern 'REALISM_' | ForEach-Object { Write-Host $_.Line }
