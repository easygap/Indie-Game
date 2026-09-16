[CmdletBinding()]
param(
    [switch]$Measure,
    [ValidateSet('High', 'Performance')][string]$Quality = 'High',
    [ValidatePattern('^[a-zA-Z0-9_-]*$')][string]$Profile = ''
)
$ErrorActionPreference = 'Stop'
$landingRoot = Split-Path -Parent $PSScriptRoot
$landingProject = Join-Path $landingRoot 'IndieGame.uproject'
$landingEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $landingProject -Commandlet
$landingLog = Join-Path $landingRoot $(if ($Profile) { "Saved/Logs/LandingReview-$Profile.log" } else { 'Saved/Logs/LandingReview.log' })
$landingStart = Get-Date
$landingArgs = @($landingProject, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGLandingAudit', '-IGSkipFrontend', "-abslog=$landingLog")
$landingCommands = if ($Quality -eq 'Performance') {
    @('Scalability 1', 'sg.ResolutionQuality 71', 'r.ScreenPercentage 71')
} else { @('Scalability 2', 'sg.ResolutionQuality 100', 'r.ScreenPercentage 100') }
if ($Measure) {
    $landingArgs += @('-IGLandingMetrics', '-IGCaptureMetricsOnly', '-csvGpuStats')
    $landingCommands += @('t.MaxFPS 0', 'r.VSync 0')
}
$landingArgs += '-ExecCmds=' + ($landingCommands -join ',')
$settingsPath = Join-Path $landingRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$settingsBytes = if (Test-Path -LiteralPath $settingsPath) { [IO.File]::ReadAllBytes($settingsPath) } else { $null }
try {
    & $landingEditor @landingArgs | Out-Null
    $landingExit = $LASTEXITCODE
} finally {
    if ($null -ne $settingsBytes) { [IO.File]::WriteAllBytes($settingsPath, $settingsBytes) }
    elseif (Test-Path -LiteralPath $settingsPath) { Remove-Item -LiteralPath $settingsPath }
}
if ($landingExit -ne 0 -or -not (Select-String -LiteralPath $landingLog -Pattern 'LANDING_CAPTURE PASS') -or
    (Select-String -LiteralPath $landingLog -Pattern 'Failed to compile Material|Fatal error:|missing usage flag')) {
    throw "승강기 주변 화면 검사 실패: $landingLog"
}
$landingShots = @('landing-lobby-lift', 'landing-lobby-mail', 'landing-entrance', 'landing-fourth-lift',
    'landing-fourth-return', 'landing-corridor-approach', 'landing-neighbor-doors', 'landing-lobby-wide',
    'landing-noticeboard', 'landing-meter-record', 'landing-fourth-waiting', 'landing-lift-open')
foreach ($shot in $(if ($Measure) { @() } else { $landingShots })) {
    $file = Get-Item -LiteralPath (Join-Path $landingRoot "Docs/Media/$shot.png")
    if ($file.LastWriteTime -lt $landingStart -or $file.Length -lt 10000) { throw "새 캡처 없음: $shot" }
}
if ($Measure -and -not (Select-String -LiteralPath $landingLog -Pattern 'Writing CSV to file')) { throw '성능 CSV가 저장되지 않았다.' }
Write-Host "LANDING_REVIEW PASS shots=$($landingShots.Count) measure=$Measure quality=$Quality"
