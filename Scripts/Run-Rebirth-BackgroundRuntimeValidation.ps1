[CmdletBinding()]
param(
	[ValidateRange(30, 900)]
	[int]$TimeoutSeconds = 240,
	[switch]$SkipMapCheck,
	[switch]$SkipRuntimeEndings
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'
$editorCommand = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') `
	-ProjectPath $projectFile `
	-Commandlet
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($editorCommand)) {
	throw 'UnrealEditor-Cmd.exe를 찾지 못했습니다.'
}
$editorCommand = [string]$editorCommand

$runId = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
$runDirectory = Join-Path $projectRoot "Saved\Validation\BackgroundRuntime\$runId"
New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null

function ConvertTo-ProcessArgument {
	param([Parameter(Mandatory = $true)][string]$Value)

	if ($Value -notmatch '[\s"]') {
		return $Value
	}
	return '"' + $Value.Replace('"', '\"') + '"'
}

function Invoke-HiddenUnreal {
	param(
		[Parameter(Mandatory = $true)][string]$Name,
		[Parameter(Mandatory = $true)][string]$LogPath,
		[Parameter(Mandatory = $true)][string[]]$Arguments
	)

	if (Test-Path -LiteralPath $LogPath) {
		Remove-Item -LiteralPath $LogPath -Force
	}
	$processArguments = @(
		$Arguments | ForEach-Object { ConvertTo-ProcessArgument -Value $_ }
	)
	$process = Start-Process `
		-FilePath $editorCommand `
		-ArgumentList $processArguments `
		-PassThru `
		-WindowStyle Hidden
	try {
		if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
			$process.Kill()
			[void]$process.WaitForExit(5000)
			throw "$Name 검증이 ${TimeoutSeconds}초 안에 끝나지 않았습니다."
		}
		$process.Refresh()
		if ($process.ExitCode -ne 0) {
			throw "$Name 검증 프로세스가 종료 코드 $($process.ExitCode)를 반환했습니다."
		}
	}
	finally {
		$process.Dispose()
	}

	if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
		throw "$Name 검증 로그가 생성되지 않았습니다: $LogPath"
	}
	Get-Content -Raw -Encoding UTF8 -LiteralPath $LogPath
}

if (-not $SkipMapCheck) {
	$mapLog = Join-Path $runDirectory 'MapCheck_Prologue_Morning.log'
	$mapText = Invoke-HiddenUnreal `
		-Name '프롤로그 맵 검사' `
		-LogPath $mapLog `
		-Arguments @(
			$projectFile,
			'/Game/Maps/Prologue_Morning',
			'-unattended',
			'-nosplash',
			'-nullrhi',
			'-nosound',
			'-RenderOffscreen',
			'-stdout',
			'-FullStdOutLogOutput',
			"-abslog=$mapLog",
			'-ExecCmds=MAP CHECK,QUIT_EDITOR'
		)
	$mapPassPattern =
		'(?im)MapCheck:.*(?:Map check complete:\s*0 Error|' +
		'맵 체크 완료:\s*오류 0 회,\s*경고 0 회)'
	if ($mapText -match '(?im)MapCheck:.*Error:' -or
		$mapText -match '(?im)Map check failed|맵 체크 실패' -or
		$mapText -notmatch $mapPassPattern) {
		throw "프롤로그 맵 검사가 오류 없이 완료되지 않았습니다: $mapLog"
	}
	Write-Host 'REBIRTH_BACKGROUND PASS map_check errors=0'
}

if (-not $SkipRuntimeEndings) {
	foreach ($ending in @('A', 'B')) {
		$runtimeLog = Join-Path $runDirectory "RebirthRelease_Ending$ending.log"
		$runtimeArguments = @(
			$projectFile,
			'-game',
			'-unattended',
			'-nosplash',
			'-nullrhi',
			'-nosound',
			'-RenderOffscreen',
			'-stdout',
			'-FullStdOutLogOutput',
			"-abslog=$runtimeLog",
			'-IGRebirthGreybox',
			'-IGRebirthReleaseValidation',
			"-IGRebirthEnding=$ending"
		)
		if ($ending -eq 'A') {
			$runtimeArguments += '-IGRebirthEndToEndValidation'
		}
		else {
			$runtimeArguments += '-IGChapterThree'
		}
		$runtimeText = Invoke-HiddenUnreal `
			-Name "엔딩 $ending 종단" `
			-LogPath $runtimeLog `
			-Arguments $runtimeArguments
		foreach ($marker in @(
			'REBIRTH_GREYBOX PASS',
			'REBIRTH_RELEASE PASS s2_roof_door',
			'REBIRTH_RELEASE PASS collision_route',
			'REBIRTH_RELEASE PASS audio_queue',
			'REBIRTH_RELEASE PASS p3_p5',
			'REBIRTH_RELEASE PASS savegame_v3',
			"REBIRTH_SPIKE PASS s4_common_prop ending=$ending duplicates=0",
			"REBIRTH_RELEASE PASS ending=$ending",
			"REBIRTH_RELEASE PASS complete ending=$ending"
		)) {
			if (-not $runtimeText.Contains($marker)) {
				throw "엔딩 $ending 필수 마커가 없습니다: $marker"
			}
		}
		if ($ending -eq 'A') {
			foreach ($marker in @(
				'REBIRTH_E2E PASS ch01_router',
				'REBIRTH_E2E PASS ch02_router',
				'REBIRTH_E2E PASS ch03_handoff',
				'REBIRTH_SPIKE PASS s1_outfit_sleeve chapters=3 duplicates=0 stitches=3'
			)) {
				if (-not $runtimeText.Contains($marker)) {
					throw "엔딩 A 종단 필수 마커가 없습니다: $marker"
				}
			}
		}
		Write-Host "REBIRTH_BACKGROUND PASS runtime ending=$ending"
	}
}

$visibleProcessNames = @(
	'UnrealEditor',
	'UnrealEditor-Cmd',
	'EpicGamesLauncher',
	'CrashReportClient',
	'CrashReportClientEditor'
)
$visibleProcesses = @(
	Get-Process -ErrorAction SilentlyContinue |
		Where-Object {
			$visibleProcessNames -contains $_.ProcessName -and
			$_.MainWindowHandle -ne 0
		}
)
if ($visibleProcesses.Count -ne 0) {
	throw "화면에 표시된 Unreal 관련 프로세스가 있습니다: $($visibleProcesses.Count)"
}

Write-Host (
	'REBIRTH_BACKGROUND PASS complete ' +
	"visible_windows=0 evidence=$runDirectory")
