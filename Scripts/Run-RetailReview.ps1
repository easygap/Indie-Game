[CmdletBinding()]
param(
    [switch]$Measure,
    [ValidateSet('High', 'Performance')][string]$Quality = 'High',
    [string[]]$RenderCommands = @(),
    [ValidatePattern('^[a-zA-Z0-9_-]*$')][string]$Profile = ''
)
$ErrorActionPreference = 'Stop'
$reviewRoot = Split-Path -Parent $PSScriptRoot
$reviewProject = Join-Path $reviewRoot 'IndieGame.uproject'
$reviewEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $reviewProject -Commandlet
$reviewLog = Join-Path $reviewRoot $(if ($Profile) { "Saved/Logs/RetailReview-$Profile.log" } else { 'Saved/Logs/RetailReview.log' })
$reviewStarted = Get-Date
$reviewArgs = @($reviewProject, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGRetailCapture', '-IGSkipFrontend', "-abslog=$reviewLog")
if ($Measure) { $reviewArgs += @('-IGRetailMetrics', '-IGCaptureMetricsOnly', '-csvGpuStats') }
$reviewCommands = if ($Quality -eq 'Performance') {
    @('Scalability 1', 'sg.ResolutionQuality 71', 'r.ScreenPercentage 71')
} else {
    @('Scalability 2', 'sg.ResolutionQuality 100', 'r.ScreenPercentage 100')
}
$reviewCommands += @($RenderCommands)
if ($Measure) { $reviewCommands += @('t.MaxFPS 0', 'r.VSync 0') }
if ($reviewCommands.Count) { $reviewArgs += ('-ExecCmds=' + ($reviewCommands -join ',')) }
$settingsPath = Join-Path $reviewRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$settingsExisted = Test-Path -LiteralPath $settingsPath
$settingsBytes = if ($settingsExisted) { [IO.File]::ReadAllBytes($settingsPath) } else { $null }
try {
    & $reviewEditor @reviewArgs | Out-Null
    $reviewExit = $LASTEXITCODE
} finally {
    if ($settingsExisted) { [IO.File]::WriteAllBytes($settingsPath, $settingsBytes) }
    elseif (Test-Path -LiteralPath $settingsPath) { Remove-Item -LiteralPath $settingsPath }
}
if ($reviewExit -ne 0 -or -not (Select-String -LiteralPath $reviewLog -Pattern 'RETAIL_CAPTURE PASS shots=') -or
    (Select-String -LiteralPath $reviewLog -Pattern 'missing usage flag|Failed to compile Material|Scalability.ini can only set|RETAIL_STOCK FAIL|KITCHEN_SUPPORT FAIL|Fatal error:')) {
    throw "매장 화면 검증 실패: $reviewLog"
}
foreach ($shot in $(if ($Measure) { @() } else { @('retail-exterior', 'retail-counter', 'retail-stock', 'retail-cooler', 'retail-delivery-lane', 'retail-clerk', 'kitchen-microwave') })) {
    $file = Get-Item -LiteralPath (Join-Path $reviewRoot "Docs/Media/$shot.png")
    if ($file.LastWriteTime -lt $reviewStarted -or $file.Length -lt 10000) {
        throw "새 화면이 저장되지 않았다: $($file.FullName)"
    }
}
Select-String -LiteralPath $reviewLog -Pattern 'RETAIL_STOCK|RETAIL_CAPTURE' | ForEach-Object { Write-Host $_.Line }
if ($Measure -and -not (Select-String -LiteralPath $reviewLog -Pattern 'Writing CSV to file')) { throw '성능 CSV가 저장되지 않았다.' }
Write-Host "RETAIL_REVIEW PASS measure=$Measure quality=$Quality production=1 resolution=1920x1080"
