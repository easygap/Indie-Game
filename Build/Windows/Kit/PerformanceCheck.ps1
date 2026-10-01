<#
  Missing Floor 성능 측정. 같은 폴더의 「성능 측정 - 높음.bat」과 「성능 측정 - 낮음.bat」이
  이 파일을 실행한다. 게임은 한 단계 위 폴더에 있다.

  밤 장면을 정해진 경로로 세 번 돌며 프레임 시간을 기록하고, PC 사양과 함께
  바탕 화면에 ZIP으로 묶는다. 측정이 끝나면 결과 요약을 보여 준다.
  개인 정보는 담지 않는다. CPU·그래픽카드 이름, 드라이버 버전, 메모리 크기,
  Windows 버전과 게임 설정만 들어간다.
#>
param(
	[ValidateSet('High', 'Low')][string]$Quality = 'High',
	[ValidateSet('1080p', '720p', '1440p')][string]$Resolution = '1080p',
	[ValidateRange(1, 3)][int]$Runs = 3,
	[ValidateRange(0, 600)][int]$WarmupSeconds = 120
)
$ErrorActionPreference = 'Stop'
$gameRoot = Split-Path $PSScriptRoot -Parent
$game = Join-Path $gameRoot 'MissingFloor.exe'
if (-not (Test-Path -LiteralPath $game)) { throw 'ZIP의 압축을 모두 푼 뒤 「테스트 도구」 폴더에서 실행해 주세요.' }
$width, $height = @{ '720p' = @(1280, 720); '1080p' = @(1920, 1080); '1440p' = @(2560, 1440) }[$Resolution]
# 게임 설정의 낮음은 엔진 품질 단계 1, 높음은 2다(IGPlayerController).
$level = if ($Quality -eq 'High') { 2 } else { 1 }
$stamp = Get-Date -Format 'yyyyMMdd-HHmm'
$desktop = [Environment]::GetFolderPath('Desktop')
$outRoot = Join-Path $desktop "MissingFloor-performance-$stamp"
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null

Write-Host ''
Write-Host "Missing Floor 성능 측정: $Resolution $Quality, $Runs회"
Write-Host '한 번에 4~5분 걸립니다. 측정 중에는 다른 프로그램을 쓰지 말고 기다려 주세요.'
Write-Host ''

# 1% low는 가장 느린 1% 프레임 시간의 평균을 fps로 바꾼 값이다(Docs/PERFORMANCE.md).
function Measure-Run([string]$CsvPath) {
	$frames = Import-Csv -LiteralPath $CsvPath | Where-Object { [int]$_.Stage -ge 0 } | ForEach-Object { [double]$_.FrameTime }
	$sorted = @($frames | Sort-Object)
	if ($sorted.Count -lt 100) { return $null }
	$p95 = $sorted[[math]::Min($sorted.Count - 1, [int][math]::Round(0.95 * ($sorted.Count - 1)))]
	$worstCount = [math]::Max(1, [math]::Ceiling($sorted.Count / 100))
	$worst = $sorted[($sorted.Count - $worstCount)..($sorted.Count - 1)]
	$worstMean = ($worst | Measure-Object -Average).Average
	[ordered]@{
		frames = $sorted.Count
		p95_ms = [math]::Round($p95, 3)
		one_percent_low_fps = [math]::Round(1000.0 / $worstMean, 2)
		max_ms = [math]::Round($sorted[-1], 2)
		over_50ms = @($sorted | Where-Object { $_ -gt 50 }).Count
		over_100ms = @($sorted | Where-Object { $_ -gt 100 }).Count
	}
}

$results = @()
for ($run = 1; $run -le $Runs; $run++) {
	$runRoot = Join-Path $outRoot "Run$run"
	$userRoot = Join-Path $runRoot 'User'
	$configRoot = Join-Path $userRoot 'Saved/Config/Windows'
	New-Item -ItemType Directory -Force -Path $configRoot | Out-Null
	$groups = @('ViewDistance', 'AntiAliasing', 'Shadow', 'GlobalIllumination', 'Reflection', 'PostProcess',
		'Texture', 'Effects', 'Foliage', 'Shading', 'Landscape') | ForEach-Object { "sg.${_}Quality=$level" }
	$engineIni = "[SystemSettings]`nr.ScreenPercentage=100`nr.DynamicRes.OperationMode=0`nr.VSync=0`nt.MaxFPS=0`n"
	$userIni = "[/Script/Engine.GameUserSettings]`nbUseVSync=False`nbUseDynamicResolution=False`nFrameRateLimit=0.000000`n" +
		"ResolutionSizeX=$width`nResolutionSizeY=$height`nLastUserConfirmedResolutionSizeX=$width`nLastUserConfirmedResolutionSizeY=$height`n" +
		"FullscreenMode=1`nLastConfirmedFullscreenMode=1`nPreferredFullscreenMode=1`nVersion=5`n`n[ScalabilityGroups]`nsg.ResolutionQuality=100`n" + ($groups -join "`n") + "`n"
	[IO.File]::WriteAllText((Join-Path $configRoot 'Engine.ini'), $engineIni, [Text.UTF8Encoding]::new($false))
	[IO.File]::WriteAllText((Join-Path $configRoot 'GameUserSettings.ini'), $userIni, [Text.UTF8Encoding]::new($false))
	$receipt = Join-Path $runRoot 'receipt.txt'
	$arguments = @('-unattended', '-nosplash', '-NoLoadingScreen', '-d3d12', "-ResX=$width", "-ResY=$height", '-ForceRes',
		'-IGListenerGreybox', '-IGNightCapture', '-IGCaptureMetricsOnly', '-IGSkipFrontend', '-IGRuntimeProfile',
		"-IGPerformanceWarmupSeconds=$WarmupSeconds", "-UserDir=$userRoot", "-IGMissingFloorResultPath=$receipt")
	$quoted = @($arguments | ForEach-Object { '"' + $_ + '"' })
	Write-Host "측정 $run/$Runs 시작"
	$process = Start-Process -FilePath $game -ArgumentList $quoted -WorkingDirectory $gameRoot -PassThru
	$null = $process.Handle
	if (-not $process.WaitForExit(900 * 1000)) {
		& taskkill.exe /PID $process.Id /T /F | Out-Null
		Write-Warning "측정 $run 이 15분 안에 끝나지 않아 멈췄습니다."
	}
	$csv = Join-Path $userRoot 'Saved/Profiling/MissingFloorRuntime.csv'
	$summary = if (Test-Path -LiteralPath $csv) { Measure-Run $csv } else { $null }
	$passed = (Test-Path -LiteralPath $receipt) -and ((Get-Content -Raw -LiteralPath $receipt) -match 'MISSINGFLOOR_GREYBOX PASS')
	$results += [ordered]@{ run = $run; route_completed = $passed; summary = $summary }
	if ($summary) {
		Write-Host ("  p95 {0} ms, 1% low {1} fps, 50 ms 넘은 프레임 {2}개" -f $summary.p95_ms, $summary.one_percent_low_fps, $summary.over_50ms)
	}
	else { Write-Warning "  측정 $run 의 기록을 읽지 못했습니다." }
}

# 사양. 게임 설정과 함께 결과를 해석하는 데 쓴다.
$gpu = @(Get-CimInstance Win32_VideoController | ForEach-Object { [ordered]@{ name = $_.Name; driver = $_.DriverVersion } })
$cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1).Name
$os = Get-CimInstance Win32_OperatingSystem
$memoryGb = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1)
$power = (& powercfg.exe /getactivescheme) -join ' '
$report = [ordered]@{
	schema = 1
	created = (Get-Date).ToUniversalTime().ToString('o')
	quality = $Quality; resolution = $Resolution; warmup_seconds = $WarmupSeconds
	system = [ordered]@{ cpu = $cpu.Trim(); gpus = $gpu; memory_gb = $memoryGb; os = "$($os.Caption) $($os.Version) build $($os.BuildNumber)"; power_scheme = $power }
	game_version = (Get-Item -LiteralPath $game).VersionInfo.ProductVersion
	runs = $results
}
[IO.File]::WriteAllText((Join-Path $outRoot 'performance-report.json'), ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
Write-Host ''
Write-Host 'PC 사양을 모으는 중입니다(dxdiag, 30초 정도).'
& dxdiag.exe /t (Join-Path $outRoot 'dxdiag.txt') | Out-Null
$deadline = (Get-Date).AddSeconds(90)
while (-not (Test-Path -LiteralPath (Join-Path $outRoot 'dxdiag.txt')) -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 2 }

$zip = "$outRoot.zip"
Compress-Archive -Path (Join-Path $outRoot '*') -DestinationPath $zip -Force
$limit = if ($Quality -eq 'High' -and $Resolution -eq '1080p') { @{ p95 = 16.67; low = 50; hitch = 'over_50ms' } } else { @{ p95 = 33.33; low = 25; hitch = 'over_100ms' } }
$ok = @($results | Where-Object { $_.summary -and $_.summary.p95_ms -le $limit.p95 -and $_.summary.one_percent_low_fps -ge $limit.low -and $_.summary[$limit.hitch] -eq 0 }).Count
Write-Host ''
Write-Host "목표(p95 $($limit.p95) ms 이하, 1% low $($limit.low) fps 이상)를 넘은 측정: $ok/$Runs"
Write-Host "결과 파일: $zip"
Write-Host 'https://github.com/easygap/Missing-Floor/issues/new?template=performance_report.yml 에 이 ZIP을 끌어다 놓아 보내 주세요.'
Start-Process explorer.exe "/select,`"$zip`""
