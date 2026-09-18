[CmdletBinding()]
param([switch]$Before, [switch]$Measure,
    [ValidatePattern('^[a-zA-Z0-9_-]*$')][string]$Profile='')
$ErrorActionPreference = 'Stop'
$householdRoot = Split-Path -Parent $PSScriptRoot
$householdProject = Join-Path $householdRoot 'IndieGame.uproject'
$householdEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $householdProject -Commandlet
$householdLog = Join-Path $householdRoot "Saved/Logs/HouseholdReview-$Before-$Profile.log"
$householdStart = Get-Date
$householdArgs = @($householdProject, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGHouseholdReview', '-IGSkipFrontend', "-abslog=$householdLog")
$commands = @('Scalability 2','sg.ResolutionQuality 100','r.ScreenPercentage 100')
if ($Before) { $householdArgs += '-IGHouseholdBaseline' }
if ($Measure) { $householdArgs += @('-IGCaptureMetricsOnly','-csvGpuStats'); $commands += @('t.MaxFPS 0','r.VSync 0') }
$householdArgs += '-ExecCmds=' + ($commands -join ',')
$settings = Join-Path $householdRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$savedSettings = if (Test-Path -LiteralPath $settings) { [IO.File]::ReadAllBytes($settings) } else { $null }
try { & $householdEditor @householdArgs | Out-Null; $result = $LASTEXITCODE }
finally {
    if ($null -ne $savedSettings) { [IO.File]::WriteAllBytes($settings, $savedSettings) }
    elseif (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings }
}
if ($result -ne 0 -or -not (Select-String -LiteralPath $householdLog -Pattern 'HOUSEHOLD_REVIEW PASS') -or
    (Select-String -LiteralPath $householdLog -Pattern 'Failed to compile Material|Fatal error:|missing usage flag')) {
    throw "생활 소품 검사 실패: $householdLog"
}
$prefix = if ($Before) { 'household-before-' } else { 'household-after-' }
if (-not $Before -and -not $Measure) {
    foreach ($marker in @('HOUSEHOLD_LAYOUT PASS','HOUSEHOLD_INTERACTION PASS')) {
        if (-not (Select-String -LiteralPath $householdLog -Pattern $marker)) { throw "생활 소품 검사가 끝나지 않았습니다: $marker" }
    }
}
foreach ($shot in $(if ($Measure) { @() } else { @('slippers','slipper-low','bowl','bowl-close','alley','condenser-front','condenser-side','facade','delivery') })) {
    $file = Get-Item -LiteralPath (Join-Path $householdRoot "Docs/Media/$prefix$shot.png")
    if ($file.LastWriteTime -lt $householdStart -or $file.Length -lt 10000) { throw "새 캡처 없음: $shot" }
}
if ($Measure -and -not (Select-String -LiteralPath $householdLog -Pattern 'Writing CSV to file')) { throw '성능 CSV 저장 실패' }
Write-Host "HOUSEHOLD_REVIEW PASS measure=$Measure"
