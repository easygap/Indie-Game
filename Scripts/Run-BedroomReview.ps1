[CmdletBinding()]
param([switch]$Before, [switch]$Measure,
    [ValidatePattern('^[a-zA-Z0-9_-]*$')][string]$Profile='')
$ErrorActionPreference = 'Stop'
$bedroomRoot = Split-Path -Parent $PSScriptRoot
$bedroomProject = Join-Path $bedroomRoot 'IndieGame.uproject'
$bedroomEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $bedroomProject -Commandlet
$bedroomLog = Join-Path $bedroomRoot "Saved/Logs/BedroomReview-$Before-$Profile.log"
$bedroomStart = Get-Date
$bedroomArgs = @($bedroomProject, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGBedroomReview', '-IGSkipFrontend', "-abslog=$bedroomLog")
$commands = @('Scalability 2','sg.ResolutionQuality 100','r.ScreenPercentage 100')
if ($Before) { $bedroomArgs += '-IGBedroomBaseline' }
if ($Measure) { $bedroomArgs += @('-IGCaptureMetricsOnly','-csvGpuStats'); $commands += @('t.MaxFPS 0','r.VSync 0') }
$bedroomArgs += '-ExecCmds=' + ($commands -join ',')
$settings = Join-Path $bedroomRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$savedSettings = if (Test-Path -LiteralPath $settings) { [IO.File]::ReadAllBytes($settings) } else { $null }
try { & $bedroomEditor @bedroomArgs | Out-Null; $result = $LASTEXITCODE }
finally {
    if ($null -ne $savedSettings) { [IO.File]::WriteAllBytes($settings, $savedSettings) }
    elseif (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings }
}
if ($result -ne 0 -or -not (Select-String -LiteralPath $bedroomLog -Pattern 'BEDROOM_REVIEW PASS') -or
    (Select-String -LiteralPath $bedroomLog -Pattern 'Failed to compile Material|Fatal error:|missing usage flag')) {
    throw "침실 검사 실패: $bedroomLog"
}
$prefix = if ($Before) { 'bedroom-before-' } else { 'bedroom-after-' }
foreach ($shot in $(if ($Measure) { @() } else { @('room','bed','foot','pillow','hem','desk') })) {
    $file = Get-Item -LiteralPath (Join-Path $bedroomRoot "Docs/Media/$prefix$shot.png")
    if ($file.LastWriteTime -lt $bedroomStart -or $file.Length -lt 10000) { throw "새 캡처 없음: $shot" }
}
if ($Measure -and -not (Select-String -LiteralPath $bedroomLog -Pattern 'Writing CSV to file')) { throw '성능 CSV 저장 실패' }
Write-Host "BEDROOM_REVIEW PASS measure=$Measure"
