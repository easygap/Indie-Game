[CmdletBinding()]
param(
	[ValidateRange(640, 7680)]
	[int]$ResX = 1920,
	[ValidateRange(360, 4320)]
	[int]$ResY = 1080
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'IndieGame.uproject'
$resolver = Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1'
# 리졸버는 PowerShell 7로 돌리는 쪽이 낫다. 5.1은 기본 출력 인코딩이 시스템
# ANSI 코드페이지라 한글 경로가 섞이면 깨질 수 있다. 다만 PS7이 없다고 캡처
# 전체를 막을 이유는 없다 — 리졸버가 돌려주는 것은 엔진 설치 경로뿐이고
# 거기에는 한글이 안 들어간다. Build-ArtAssets.ps1과 같은 처리를 한다.
$powerShellCore = Get-Command 'pwsh.exe' -ErrorAction SilentlyContinue
$usingPowerShellCore = $null -ne $powerShellCore
if ($usingPowerShellCore) {
	$resolverShell = $powerShellCore.Source
} else {
	$windowsPowerShell = Get-Command 'powershell.exe' -ErrorAction SilentlyContinue
	if ($null -eq $windowsPowerShell) {
		throw 'PowerShell을 찾을 수 없어 캡처를 시작할 수 없습니다.'
	}
	$resolverShell = $windowsPowerShell.Source
	Write-Host 'PROLOGUE_CAPTURE pwsh 없음 — Windows PowerShell로 리졸버 실행(UTF-8 고정)'
}
$runLog = Join-Path $projectRoot 'Saved\Logs\PrologueCapture.log'
$mediaRoot = Join-Path $projectRoot 'Docs\Media'
$expectedCaptures = @(
	'prologue-bedroom.png',
	'prologue-kitchen.png',
	'prologue-not-found-note.png',
	'prologue-corridor.png',
	'prologue-elevator.png',
	'prologue-lobby.png',
	'prologue-villa.png',
	'prologue-alley.png',
	'prologue-ramyeon.png',
	'prologue-store.png'
)

$editorOutput = @(
	if ($usingPowerShellCore) {
		& $resolverShell `
			-NoProfile `
			-File $resolver `
			-ProjectPath $projectFile 2>&1
	} else {
		& $resolverShell `
			-NoProfile `
			-ExecutionPolicy Bypass `
			-Command "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; & '$resolver' -ProjectPath '$projectFile'" 2>&1
	}
)
if ($LASTEXITCODE -ne 0 -or $editorOutput.Count -eq 0) {
	throw 'Unreal Engine resolution failed.'
}
$editor = ([string]$editorOutput[-1]).Trim()
if (-not $editor.EndsWith(
		'UnrealEditor.exe',
		[StringComparison]::OrdinalIgnoreCase)) {
	throw "Prologue capture requires UnrealEditor.exe: $editor"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $runLog) |
	Out-Null
if (Test-Path -LiteralPath $runLog -PathType Leaf) {
	Remove-Item -LiteralPath $runLog -Force
}
$captureStartedAt = Get-Date
$arguments = @(
	$projectFile,
	'-game',
	'-unattended',
	'-nop4',
	'-nosplash',
	'-NoLoadingScreen',
	'-RenderOffscreen',
	'-d3d12',
	'-nosound',
	'-Windowed',
	"-ResX=$ResX",
	"-ResY=$ResY",
	'-ForceRes',
	"-abslog=$runLog",
	'-IGCapture',
	'-IGSkipFrontend'
)

Write-Host "PROLOGUE_CAPTURE running ${ResX}x${ResY} offscreen D3D12"
& $editor @arguments
$editorExit = $LASTEXITCODE
if ($editorExit -ne 0) {
	throw "Prologue capture exited with code $editorExit. Check $runLog"
}

# UnrealEditor.exe is a launcher stub in installed builds and can return before
# the spawned editor process completes. Treat the completion marker as the
# authoritative result, while still failing early when the process disappears.
$captureDeadline = $captureStartedAt.AddMinutes(10)
$completionPattern = 'Demo walkthrough complete; exiting.'
while ((Get-Date) -lt $captureDeadline) {
	if ((Test-Path -LiteralPath $runLog -PathType Leaf) -and
		(Select-String -LiteralPath $runLog -Pattern $completionPattern -Quiet)) {
		break
	}
	$elapsedSeconds = ((Get-Date) - $captureStartedAt).TotalSeconds
	if ($elapsedSeconds -ge 10 -and
		-not (Get-Process -Name 'UnrealEditor' -ErrorAction SilentlyContinue)) {
		throw "Unreal Editor stopped before capture completion. Check $runLog"
	}
	Start-Sleep -Seconds 1
}
if (-not (Select-String `
		-LiteralPath $runLog `
		-Pattern $completionPattern `
		-Quiet)) {
	throw "Prologue capture did not report completion. Check $runLog"
}

$missing = [Collections.Generic.List[string]]::new()
$stale = [Collections.Generic.List[string]]::new()
foreach ($captureName in $expectedCaptures) {
	$path = Join-Path $mediaRoot $captureName
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		$missing.Add($captureName)
		continue
	}
	if ((Get-Item -LiteralPath $path).LastWriteTime -lt $captureStartedAt) {
		$stale.Add($captureName)
	}
}
if ($missing.Count -gt 0 -or $stale.Count -gt 0) {
	throw (
		"Prologue capture output verification failed. missing=[{0}] stale=[{1}]" -f
		($missing -join ', '), ($stale -join ', ')
	)
}

Write-Host ((
	"PROLOGUE_CAPTURE PASS captures={0} resolution={1}x{2} " +
	"offscreen_d3d12=true"
) -f $expectedCaptures.Count, $ResX, $ResY)
