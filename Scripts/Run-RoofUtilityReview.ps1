[CmdletBinding()]
param(
    [switch]$Measure,
    [ValidateSet('High', 'Performance')][string]$Quality = 'High'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'IndieGame.uproject'
$editor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $project -Commandlet
$log = Join-Path $root "Saved/Logs/RoofUtility-$Quality-$Measure.log"
$started = Get-Date
$runArgs = @($project, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGRoofUtilityAudit', '-IGSkipFrontend', "-abslog=$log")
$commands = if ($Quality -eq 'Performance') { @('Scalability 1', 'sg.ResolutionQuality 71', 'r.ScreenPercentage 71') }
    else { @('Scalability 2', 'sg.ResolutionQuality 100', 'r.ScreenPercentage 100') }
if ($Measure) { $runArgs += @('-IGCaptureMetricsOnly', '-csvGpuStats'); $commands += @('t.MaxFPS 0', 'r.VSync 0') }
$runArgs += '-ExecCmds=' + ($commands -join ',')
$settings = Join-Path $root 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$savedSettings = if (Test-Path -LiteralPath $settings) { [IO.File]::ReadAllBytes($settings) } else { $null }
try { & $editor @runArgs | Out-Null; $result = $LASTEXITCODE }
finally {
    if ($null -ne $savedSettings) { [IO.File]::WriteAllBytes($settings, $savedSettings) }
    elseif (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings }
}
if ($result -ne 0 -or -not (Select-String -LiteralPath $log -Pattern 'ROOF_UTILITY_CAPTURE PASS') -or
    (Select-String -LiteralPath $log -Pattern 'Failed to compile Material|Fatal error:|missing usage flag')) {
    throw "옥상 설비 검사 실패: $log"
}
if ($Measure) {
    if (-not (Select-String -LiteralPath $log -Pattern 'Writing CSV to file')) { throw '성능 CSV가 저장되지 않았다.' }
} else {
    foreach ($shot in @('roof-stair-up', 'roof-stair-handrail', 'roof-stair-down', 'roof-tank-wide', 'roof-tank-plate',
        'roof-tank-side', 'roof-valve-closed', 'roof-valve-open', 'roof-bypass-open', 'roof-circuit-running')) {
        $file = Get-Item -LiteralPath (Join-Path $root "Docs/Media/$shot.png")
        if ($file.LastWriteTime -lt $started -or $file.Length -lt 10000) { throw "새 캡처 없음: $shot" }
    }
}
Write-Host "ROOF_UTILITY_REVIEW PASS measure=$Measure quality=$Quality"
