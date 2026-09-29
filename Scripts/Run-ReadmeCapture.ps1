[CmdletBinding()]
param(
	[ValidateRange(60, 600)][int]$TimeoutSeconds = 240,
	# 전후 비교 촬영에 쓴다. 예: -ExtraArguments '-IGCameraTexture=0'
	[string[]]$ExtraArguments = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$editor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -Commandlet
$runLog = Join-Path $projectRoot 'Saved/Logs/ReadmeCapture.log'
$settings = Join-Path $projectRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$settingsBackup = if (Test-Path -LiteralPath $settings) { [IO.File]::ReadAllBytes($settings) } else { $null }
$startedAt = Get-Date
$process = $null
try {
	$arguments = @(
		('"{0}"' -f (Join-Path $projectRoot 'IndieGame.uproject')),
		'-game', '-unattended', '-nosplash', '-NoLoadingScreen',
		'-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
		('-UserDir={0}' -f (Join-Path $projectRoot 'Saved/Validation/Readme-User')),
		'-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGReadmeCapture', '-IGSkipFrontend',
		'"-ExecCmds=Scalability 2,r.ScreenPercentage 100,t.MaxFPS 60"', ('"-abslog={0}"' -f $runLog)
	) + $ExtraArguments
	$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
	# Windows PowerShell 5.1은 핸들을 먼저 잡아 두지 않으면 ExitCode를 비워 둔다.
	$null = $process.Handle
	if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
		& taskkill.exe /PID $process.Id /T /F | Out-Null
		throw "소개 화면 촬영 시간 초과: $runLog"
	}
	if ($process.ExitCode -ne 0 -or -not (Select-String -LiteralPath $runLog -Pattern 'README_CAPTURE PASS shots=8 production=1')) {
		throw "소개 화면 촬영 실패: $runLog"
	}
}
finally {
	if ($null -ne $settingsBackup) { [IO.File]::WriteAllBytes($settings, $settingsBackup) }
	elseif (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings }
}
foreach ($name in @('bedroom','corridor-day','alley','store','corridor-night','booth','bedroom-dawn','alley-dawn')) {
	$shot = Get-Item -LiteralPath (Join-Path $projectRoot "Docs/Media/game-$name.png")
	if ($shot.LastWriteTime -lt $startedAt -or $shot.Length -lt 10000) { throw "새 화면이 없습니다: $name" }
}
Write-Host 'README_CAPTURE PASS 새 플레이 화면 8장, 1920×1080'
