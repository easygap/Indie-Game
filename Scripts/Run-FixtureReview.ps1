[CmdletBinding()]
param([switch]$BakeCctv)
$ErrorActionPreference = 'Stop'
$fixtureRoot = Split-Path -Parent $PSScriptRoot
$fixtureProject = Join-Path $fixtureRoot 'IndieGame.uproject'
$fixtureEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $fixtureProject -Commandlet
$fixtureLog = Join-Path $fixtureRoot 'Saved/Logs/FixtureReview.log'
$fixtureStart = Get-Date
$fixtureArgs = @($fixtureProject, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGFixtureAudit', '-IGSkipFrontend',
    '-ExecCmds=Scalability 2,sg.ResolutionQuality 100,r.ScreenPercentage 100', "-abslog=$fixtureLog")
if ($BakeCctv) { $fixtureArgs += '-IGBakeCctv' }
$settingsPath = Join-Path $fixtureRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$settingsBytes = if (Test-Path -LiteralPath $settingsPath) { [IO.File]::ReadAllBytes($settingsPath) } else { $null }
try {
    & $fixtureEditor @fixtureArgs | Out-Null
    $fixtureExit = $LASTEXITCODE
} finally {
    if ($null -ne $settingsBytes) { [IO.File]::WriteAllBytes($settingsPath, $settingsBytes) }
    elseif (Test-Path -LiteralPath $settingsPath) { Remove-Item -LiteralPath $settingsPath }
}
if ($fixtureExit -ne 0 -or -not (Select-String -LiteralPath $fixtureLog -Pattern 'FIXTURE_CAPTURE PASS') -or
    (Select-String -LiteralPath $fixtureLog -Pattern 'Failed to compile Material|Fatal error:|missing usage flag')) {
    throw "설비 화면 검사 실패: $fixtureLog"
}
$fixtureShots = @('utility-booth-valve', 'utility-meter-wide', 'utility-meter-close', 'utility-booth-front',
    'utility-booth-side', 'utility-tank-steel', 'utility-countertop', 'utility-booth-pump')
if ($BakeCctv) { $fixtureShots += @('cctv-source-entrance', 'cctv-source-parking', 'cctv-source-stair', 'cctv-source-corridor') }
foreach ($shot in $fixtureShots) {
    $file = Get-Item -LiteralPath (Join-Path $fixtureRoot "Docs/Media/$shot.png")
    if ($file.LastWriteTime -lt $fixtureStart -or $file.Length -lt 10000) { throw "새 캡처 없음: $shot" }
}
Write-Host "FIXTURE_REVIEW PASS shots=$($fixtureShots.Count)"
