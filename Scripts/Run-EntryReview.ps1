[CmdletBinding()]
param([switch]$Before, [switch]$Measure, [ValidateSet('High','Performance')][string]$Quality='High',
    [ValidatePattern('^[a-zA-Z0-9_-]*$')][string]$Profile='')
$ErrorActionPreference = 'Stop'
$entryRoot = Split-Path -Parent $PSScriptRoot
$entryProject = Join-Path $entryRoot 'IndieGame.uproject'
$entryEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $entryProject -Commandlet
$entryLog = Join-Path $entryRoot "Saved/Logs/EntryReview-$Before-$Profile.log"
$entryStart = Get-Date
$entryArgs = @($entryProject, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGEntryReview', '-IGSkipFrontend', "-abslog=$entryLog")
$commands = if ($Quality -eq 'High') { @('Scalability 2','sg.ResolutionQuality 100','r.ScreenPercentage 100') }
    else { @('Scalability 1','sg.ResolutionQuality 71','r.ScreenPercentage 71') }
if ($Before) { $entryArgs += '-IGEntryBaseline' }
if ($Measure) { $entryArgs += @('-IGCaptureMetricsOnly','-csvGpuStats'); $commands += @('t.MaxFPS 0','r.VSync 0') }
$entryArgs += '-ExecCmds=' + ($commands -join ',')
$settings = Join-Path $entryRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$savedSettings = if (Test-Path -LiteralPath $settings) { [IO.File]::ReadAllBytes($settings) } else { $null }
try { & $entryEditor @entryArgs | Out-Null; $result = $LASTEXITCODE }
finally {
    if ($null -ne $savedSettings) { [IO.File]::WriteAllBytes($settings, $savedSettings) }
    elseif (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings }
}
if ($result -ne 0 -or -not (Select-String -LiteralPath $entryLog -Pattern 'ENTRY_REVIEW PASS') -or
    (Select-String -LiteralPath $entryLog -Pattern 'Failed to compile Material|Fatal error:|missing usage flag')) {
    throw "현관·관리실 검사 실패: $entryLog"
}
$shots = @('room','intercom-front','intercom-side','old-vest-wall','desk','booth-wide','vest-front','vest-side','intercom-far')
if (-not $Before -and -not $Measure) { $shots += @('vest-installed','vest-angle','booth-room','labels-front','labels-side') }
$prefix = if ($Before) { 'entry-before-' } else { 'entry-' }
foreach ($shot in $(if ($Measure) { @() } else { $shots })) {
    $file = Get-Item -LiteralPath (Join-Path $entryRoot "Docs/Media/$prefix$shot.png")
    if ($file.LastWriteTime -lt $entryStart -or $file.Length -lt 10000) { throw "새 캡처 없음: $shot" }
}
if ($Measure -and -not (Select-String -LiteralPath $entryLog -Pattern 'Writing CSV to file')) { throw '성능 CSV 저장 실패' }
Write-Host "ENTRY_REVIEW PASS shots=$($shots.Count) measure=$Measure quality=$Quality"
