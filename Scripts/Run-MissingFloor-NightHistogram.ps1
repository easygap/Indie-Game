<#
.SYNOPSIS
	§11 V5 밤 구간 8지점 히스토그램 검증.

.DESCRIPTION
	여덟 개 저자 시점을 차례로 세우고, 각 프레임에서 5% 미만 암부와 98% 초과
	하이라이트 픽셀 비율을 재서 지점별 밴드와 대조한다. V1 조명 재작업은
	「암부가 진짜 검게 떨어진다」는 대비에 관한 주장이고, 스크린샷은 그것을
	증명하지 못한다. 픽셀을 세는 것이 증명한다.

	이 검증은 사람의 화면 승인을 대체하지 않는다. 밴드 안에 들어온다는 것은
	프레임이 깨지지 않았다는 뜻이고, 그림이 좋은지는 여전히 눈이 판단한다.
	그래서 지점마다 PNG를 남긴다 — 나중에 엔진을 다시 돌리지 않고 볼 수 있도록.

.NOTES
	게임 창은 절대 뜨지 않는다. -RenderOffScreen을 주면 Windows에서 엔진이
	실제 윈도우 애플리케이션 대신 Null 플랫폼 애플리케이션을 만들고
	(WindowsPlatformApplicationMisc.cpp), D3D12 뷰포트가 스왑체인을 만들지
	않는다(WindowsD3D12Viewport.cpp). 창이 숨겨지거나 최소화되는 게 아니라
	애초에 생성되지 않으며, 데스크톱으로 프레젠트되는 경로 자체가 없다.

	이 스크립트는 그 플래그가 빠진 인자 조합을 실행 자체로 거부한다. 오타
	하나로 창이 뜨는 일을 코드로 막는다.
#>
[CmdletBinding()]
param(
	[ValidateRange(640, 3840)]
	[int]$ResX = 1280,
	[ValidateRange(360, 2160)]
	[int]$ResY = 720,
	# 밴드를 저작할 때만 쓴다. 측정값을 보고하되 판정하지 않는다.
	[switch]$ReportOnly,
	[ValidateRange(60, 900)]
	[int]$TimeoutSeconds = 420
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'
if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
	throw "언리얼 프로젝트를 찾을 수 없습니다: $projectFile"
}

$editor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') `
	-ProjectPath $projectFile -Commandlet
if ([string]::IsNullOrWhiteSpace($editor)) {
	throw 'IndieGame.uproject에 맞는 언리얼 에디터를 찾을 수 없습니다.'
}

$logDirectory = Join-Path $projectRoot 'Saved\Logs'
New-Item -ItemType Directory -Force $logDirectory | Out-Null
$runLog = Join-Path $logDirectory 'MissingFloorNightHistogram.log'
if (Test-Path -LiteralPath $runLog) {
	Remove-Item -LiteralPath $runLog -Force
}

$arguments = @(
	$projectFile,
	'-game',
	'-unattended',
	'-nosplash',
	'-NoLoadingScreen',
	# 창을 만들지 않는 플래그. 아래 가드가 이것의 존재를 강제한다.
	'-RenderOffScreen',
	'-d3d12',
	'-nosound',
	"-ResX=$ResX",
	"-ResY=$ResY",
	'-ForceRes',
	'-NoVSync',
	'-stdout',
	'-FullStdOutLogOutput',
	"-abslog=$runLog",
	'-IGListenerGreybox',
	'-IGSkipFrontend',
	'-IGNightHistogram'
)
if ($ReportOnly) {
	$arguments += '-IGNightHistogramReport'
}

# 창이 뜨지 않는다는 보장은 이 두 줄이다. 인자를 손대다 플래그가 빠지면
# 실행되지 않고 여기서 멈춘다.
if ($arguments -notcontains '-RenderOffScreen') {
	throw '-RenderOffScreen 없이는 실행하지 않습니다. 게임 창이 뜰 수 있습니다.'
}
foreach ($forbidden in @('-windowed', '-fullscreen', '-game -log')) {
	if ($arguments -contains $forbidden) {
		throw "화면을 띄울 수 있는 인자가 포함되었습니다: $forbidden"
	}
}

Write-Host (
	"§11 V5 히스토그램 스윕 시작 — ${ResX}x${ResY}, 오프스크린, 창 없음") `
	-ForegroundColor DarkGray
$process = Start-Process -FilePath $editor -ArgumentList $arguments `
	-PassThru -NoNewWindow
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
	try { $process.Kill($true) } catch {}
	throw "히스토그램 스윕이 ${TimeoutSeconds}초 안에 끝나지 않았습니다: $runLog"
}
if (-not (Test-Path -LiteralPath $runLog -PathType Leaf)) {
	throw "히스토그램 로그가 생성되지 않았습니다: $runLog"
}

$measurements = Select-String -Path $runLog -Pattern 'MISSINGFLOOR_V5 point=' |
	ForEach-Object { ($_.Line -split 'MISSINGFLOOR_V5 ')[-1] }
foreach ($measurement in $measurements) {
	$colour = if ($measurement -match 'OK$') { 'Green' } else { 'Yellow' }
	Write-Host "  $measurement" -ForegroundColor $colour
}

$failed = Select-String -Path $runLog -Pattern 'MISSINGFLOOR_V5 FAIL'
if ($failed) {
	$failed | ForEach-Object { Write-Host $_.Line -ForegroundColor Red }
	throw "§11 V5 히스토그램 검증이 실패했습니다. 로그: $runLog"
}
if ($ReportOnly) {
	$report = Select-String -Path $runLog -Pattern 'MISSINGFLOOR_V5 REPORT'
	if (-not $report) {
		throw "§11 V5 보고 패스가 끝까지 돌지 않았습니다. 로그: $runLog"
	}
	Write-Host $report[-1].Line -ForegroundColor Cyan
	return
}

$passed = Select-String -Path $runLog -Pattern 'MISSINGFLOOR_V5 PASS'
if (-not $passed) {
	throw "§11 V5가 PASS를 남기지 않았습니다. 로그: $runLog"
}
Write-Host $passed[-1].Line -ForegroundColor Green
