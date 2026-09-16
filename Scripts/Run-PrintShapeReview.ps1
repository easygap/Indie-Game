[CmdletBinding()]
param([switch]$Before, [string]$View = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'IndieGame.uproject'
$editor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $project -Commandlet
$log = Join-Path $root "Saved/Logs/PrintShapeReview-$Before.log"
$started = Get-Date
$gameArgs = @($project, '-game', '-unattended', '-nosplash', '-NoLoadingScreen', '-RenderOffscreen',
    '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGPrintShapeAudit', '-IGSkipFrontend',
    "-abslog=$log", '-ExecCmds=Scalability 2,sg.ResolutionQuality 100,r.ScreenPercentage 100')
if ($Before) { $gameArgs += '-IGPrintBaseline' }
if ($View) { $gameArgs += "-IGPrintView=$View" }
$settings = Join-Path $root 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$savedSettings = if (Test-Path -LiteralPath $settings) { [IO.File]::ReadAllBytes($settings) } else { $null }
try { & $editor @gameArgs | Out-Null; $result = $LASTEXITCODE }
finally {
    if ($null -ne $savedSettings) { [IO.File]::WriteAllBytes($settings, $savedSettings) }
    elseif (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings }
}
if ($result -ne 0 -or -not (Select-String -LiteralPath $log -Pattern 'PRINT_SHAPE_CAPTURE PASS') -or
    (Select-String -LiteralPath $log -Pattern 'Failed to compile Material|Fatal error:|missing usage flag')) {
    throw "인쇄·형상 검사 실패: $log"
}
$prefix = if ($Before) { 'print-before-' } else { 'print-' }
$shots = @('rice-a-front','rice-a-back','rice-b-front','rice-b-back','rice-c-front','rice-c-back',
    'rice-d-front','rice-d-back','rice-shelf','rice-distance','wrench-near','wrench-standing','wall-traces',
    'annex-floor','store-floor','lobby-floor','corridor-floor','elevator-space','notebook-close')
if ($View) { $shots = @($View) }
foreach ($shot in $shots) {
    $file = Get-Item -LiteralPath (Join-Path $root "Docs/Media/$prefix$shot.png")
    if ($file.LastWriteTime -lt $started -or $file.Length -lt 10000) { throw "새 캡처 없음: $shot" }
}
Write-Host "PRINT_SHAPE_REVIEW PASS before=$Before"
