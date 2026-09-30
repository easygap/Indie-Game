[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ArchiveDirectory,
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedCommit,
    [switch]$RequireCleanCommit,
    [ValidateRange(1, 3)][int]$RepeatCount = 3,
    [ValidateSet(2, 4)][int]$AntiAliasing = 2,
    [ValidateSet(1, 2)][int]$Quality = 2,
    [ValidateSet(30, 60)][int]$TargetFps = 60,
    [ValidateRange(0, 600)][int]$WarmupSeconds = 120,
    [ValidateRange(60, 900)][int]$TimeoutSeconds = 420
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$archiveRoot = (Resolve-Path -LiteralPath $ArchiveDirectory).Path
$evidenceRoot = [IO.Path]::GetFullPath($EvidenceDirectory)
if (Test-Path -LiteralPath $evidenceRoot) { throw '새 결과 폴더를 지정해 주세요. 이전 측정을 덮어쓰지 않습니다.' }
$manifestArguments = @{ ArchiveDirectory = $archiveRoot; RequireCleanCommit = $RequireCleanCommit }
if ($ExpectedCommit) { $manifestArguments.ExpectedCommit = $ExpectedCommit }
& (Join-Path $PSScriptRoot 'Test-WindowsPackageManifest.ps1') @manifestArguments
$launcher = Join-Path $archiveRoot 'Windows\MissingFloor.exe'
$python = (Get-Command python -ErrorAction Stop).Source
$results = [Collections.Generic.List[object]]::new()
for ($run = 1; $run -le $RepeatCount; $run++) {
    $runRoot = Join-Path $evidenceRoot "Run$run"
    New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
    $userRoot = Join-Path $runRoot 'User'
    $receipt = Join-Path $runRoot 'receipt.txt'
    # Shipping에서는 ExecCmds와 명령행 -ini: 덮어쓰기를 읽지 않는다.
    # 새 사용자 폴더의 일반 설정 파일로 입력하고 실제 관측값을 분석기에서 확인한다.
    $configRoot = Join-Path $userRoot 'Saved/Config/Windows'
    New-Item -ItemType Directory -Path $configRoot -Force | Out-Null
    $engineSettings = @"
[SystemSettings]
r.ScreenPercentage=100
r.ScreenPercentage.MinResolution=0
r.ScreenPercentage.MaxResolution=0
r.SecondaryScreenPercentage.GameViewport=100
r.DynamicRes.OperationMode=0
r.AntiAliasingMethod=$AntiAliasing
r.VSync=0
t.MaxFPS=0

[Audio]
UnfocusedVolumeMultiplier=1.0
"@
    $gameSettings = @"
[/Script/Engine.GameUserSettings]
bUseVSync=False
bUseDynamicResolution=False
FrameRateLimit=0.000000
ResolutionSizeX=1920
ResolutionSizeY=1080
LastUserConfirmedResolutionSizeX=1920
LastUserConfirmedResolutionSizeY=1080
DesiredScreenWidth=1920
DesiredScreenHeight=1080
bUseDesiredScreenHeight=False
FullscreenMode=2
LastConfirmedFullscreenMode=2
PreferredFullscreenMode=2
Version=5

[ScalabilityGroups]
sg.ResolutionQuality=100
sg.ViewDistanceQuality=$Quality
sg.AntiAliasingQuality=$Quality
sg.ShadowQuality=$Quality
sg.GlobalIlluminationQuality=$Quality
sg.ReflectionQuality=$Quality
sg.PostProcessQuality=$Quality
sg.TextureQuality=$Quality
sg.EffectsQuality=$Quality
sg.FoliageQuality=$Quality
sg.ShadingQuality=$Quality
sg.LandscapeQuality=$Quality
"@
    foreach ($settings in @(
        @{ Name = 'Engine.ini'; Text = $engineSettings },
        @{ Name = 'GameUserSettings.ini'; Text = $gameSettings }
    )) {
        [IO.File]::WriteAllText((Join-Path $configRoot $settings.Name), $settings.Text, [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $runRoot "Input-$($settings.Name)"), $settings.Text, [Text.UTF8Encoding]::new($false))
    }
    $arguments = @('-unattended', '-nosplash', '-NoLoadingScreen', '-RenderOffscreen', '-d3d12',
        '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-AudioMixer',
        '-AllowCommandletAudio', '-IGListenerGreybox', '-IGNightCapture', '-IGCaptureMetricsOnly',
        '-IGSkipFrontend', '-IGRuntimeProfile', "-IGPerformanceWarmupSeconds=$WarmupSeconds",
        "-UserDir=$userRoot", "-IGMissingFloorResultPath=$receipt")
    $quoted = @($arguments | ForEach-Object {
        if ($_.Contains('"')) { throw '실행 인자에 따옴표를 넣을 수 없습니다.' }
        '"' + $_ + '"'
    })
    $started = [DateTime]::UtcNow
    $process = Start-Process -FilePath $launcher -ArgumentList $quoted -WorkingDirectory (Split-Path $launcher -Parent) -WindowStyle Hidden -PassThru
    try {
        $null = $process.Handle
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            & taskkill.exe /PID $process.Id /T /F | Out-Null
            throw "배포본 성능 검사 시간 초과: Run$run"
        }
        if ($process.ExitCode -ne 0) { throw "배포본 성능 검사 종료 코드: $($process.ExitCode)" }
    }
    finally { $process.Dispose() }
    $settingsSnapshots = foreach ($settingsName in @('Engine.ini', 'GameUserSettings.ini')) {
        $beforeName = "Input-$settingsName"
        $afterName = "After-$settingsName"
        $liveSettings = Join-Path $configRoot $settingsName
        $afterExists = Test-Path -LiteralPath $liveSettings
        if ($afterExists) {
            Copy-Item -LiteralPath $liveSettings -Destination (Join-Path $runRoot $afterName)
        }
        # 엔진이 저장할 차이가 없다고 판단해 ini를 지울 수도 있으므로 부재도 그대로 기록한다.
        [ordered]@{
            name = $settingsName;
            before = $beforeName;
            beforeSha256 = (Get-FileHash -LiteralPath (Join-Path $runRoot $beforeName) -Algorithm SHA256).Hash.ToLowerInvariant();
            afterExists = $afterExists;
            after = $(if ($afterExists) { $afterName } else { $null });
            afterSha256 = $(if ($afterExists) { (Get-FileHash -LiteralPath $liveSettings -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null })
        }
    }
    [IO.File]::WriteAllText((Join-Path $runRoot 'settings.json'), ($settingsSnapshots | ConvertTo-Json -Depth 5) + "`n", [Text.UTF8Encoding]::new($false))
    if (-not (Test-Path -LiteralPath $receipt) -or
        (Get-Content -LiteralPath $receipt -Raw).Trim() -notmatch '^MISSINGFLOOR_GREYBOX PASS step=[0-9]+$') {
        throw "밤 장면 경로가 완료되지 않았습니다: Run$run"
    }
    $csvPath = Join-Path $userRoot 'Saved\Profiling\MissingFloorRuntime.csv'
    if (-not (Test-Path -LiteralPath $csvPath) -or (Get-Item -LiteralPath $csvPath).LastWriteTimeUtc -lt $started) {
        throw "새 Shipping 프레임 원본이 없습니다: Run$run"
    }
    & $python (Join-Path $PSScriptRoot 'summarize_shipping_profile.py') $csvPath $runRoot `
        --warmup-seconds $WarmupSeconds --quality $Quality --aa $AntiAliasing --target-fps $TargetFps
    if ($LASTEXITCODE -ne 0) { throw "성능 원본 검증 실패: Run$run" }
    $summary = Get-Content -LiteralPath (Join-Path $runRoot 'summary.json') -Raw | ConvertFrom-Json
    $results.Add([ordered]@{ run = $run; summary = "Run$run/summary.json";
        frameGatePassed = $summary.local_route_frame_gate_passed;
        p95ms = $summary.metrics.FrameTime.p95; onePercentLowFps = $summary.one_percent_low_fps;
        rawSha256 = $summary.source_sha256; gpuRawSha256 = $summary.gpu_source_sha256; settingsEvidence = "Run$run/settings.json";
        engineSettingsSha256 = (Get-FileHash -LiteralPath (Join-Path $runRoot 'Input-Engine.ini') -Algorithm SHA256).Hash.ToLowerInvariant();
        gameSettingsSha256 = (Get-FileHash -LiteralPath (Join-Path $runRoot 'Input-GameUserSettings.ini') -Algorithm SHA256).Hash.ToLowerInvariant() })
    Write-Host "SHIPPING_RUNTIME_PROFILE Run$run frame_gate=$($summary.local_route_frame_gate_passed)"
}
& (Join-Path $PSScriptRoot 'Test-WindowsPackageManifest.ps1') @manifestArguments
$report = [ordered]@{ scope = '현재 장비의 Shipping 오프스크린 자동 밤 장면 경로. 실제 화면 출력 비용은 제외됨';
    repeatedRuns = $RepeatCount; warmupSeconds = $WarmupSeconds; quality = $Quality;
    antiAliasing = $AntiAliasing; targetFps = $TargetFps;
    allLocalFrameGatesPassed = @($results | Where-Object { -not $_.frameGatePassed }).Count -eq 0;
    shippingReleaseCertified = $false; runs = @($results.ToArray()) }
[IO.File]::WriteAllText((Join-Path $evidenceRoot 'summary.json'), ($report | ConvertTo-Json -Depth 10) + "`n", [Text.UTF8Encoding]::new($false))
