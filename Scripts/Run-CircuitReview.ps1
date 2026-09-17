[CmdletBinding()]
param([switch]$Before, [switch]$CorridorOnly)
$ErrorActionPreference = 'Stop'
$circuitRoot = Split-Path -Parent $PSScriptRoot
$circuitProject = Join-Path $circuitRoot 'IndieGame.uproject'
$circuitEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $circuitProject -Commandlet
$circuitLog = Join-Path $circuitRoot "Saved/Logs/CircuitReview-$Before.log"
$circuitStart = Get-Date
$circuitArgs = @($circuitProject, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGCircuitReview', '-IGSkipFrontend',
    '-ExecCmds=Scalability 2,sg.ResolutionQuality 100,r.ScreenPercentage 100', "-abslog=$circuitLog")
if ($Before) { $circuitArgs += '-IGCircuitBaseline' }
if ($CorridorOnly) { $circuitArgs += '-IGCircuitCorridorOnly' }
$settings = Join-Path $circuitRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$savedSettings = if (Test-Path -LiteralPath $settings) { [IO.File]::ReadAllBytes($settings) } else { $null }
try { & $circuitEditor @circuitArgs | Out-Null; $result = $LASTEXITCODE }
finally {
    if ($null -ne $savedSettings) { [IO.File]::WriteAllBytes($settings, $savedSettings) }
    elseif (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings }
}
if ($result -ne 0 -or -not (Select-String -LiteralPath $circuitLog -Pattern 'CIRCUIT_REVIEW PASS') -or
    (Select-String -LiteralPath $circuitLog -Pattern 'Failed to compile Material|Fatal error:|missing usage flag')) {
    throw "분전반 화면 검사 실패: $circuitLog"
}
$prefix = if ($Before) { 'circuit-before-' } else { 'circuit-' }
$shots = @('corridor-front','corridor-side')
if (-not $CorridorOnly) { $shots = @('wide','front','side','day-reset','common-off','unnamed-on','meter-on','restored') + $shots }
if (-not $Before -and -not $CorridorOnly) {
    $shots += 'day-raised'
    if (-not (Select-String -LiteralPath $circuitLog -Pattern 'CIRCUIT_DAY_TRIP PASS')) { throw '낮의 차단기 복귀 검사 실패' }
}
foreach ($shot in $shots) {
    $file = Get-Item -LiteralPath (Join-Path $circuitRoot "Docs/Media/$prefix$shot.png")
    if ($file.LastWriteTime -lt $circuitStart -or $file.Length -lt 10000) { throw "새 캡처 없음: $shot" }
}
Write-Host "CIRCUIT_REVIEW PASS shots=$($shots.Count)"
