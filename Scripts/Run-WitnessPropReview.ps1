[CmdletBinding()]
param([switch]$Before)
$ErrorActionPreference = 'Stop'
$reviewRoot = Split-Path -Parent $PSScriptRoot
$reviewProject = Join-Path $reviewRoot 'IndieGame.uproject'
$reviewEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $reviewProject -Commandlet
$reviewLog = Join-Path $reviewRoot "Saved/Logs/WitnessPropReview-$Before.log"
$reviewStart = Get-Date
$reviewArgs = @($reviewProject, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGWitnessPropReview', '-IGSkipFrontend',
    '-ExecCmds=Scalability 2,sg.ResolutionQuality 100,r.ScreenPercentage 100', "-abslog=$reviewLog")
if ($Before) { $reviewArgs += '-IGWitnessPropBaseline' }
$settings = Join-Path $reviewRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$savedSettings = if (Test-Path -LiteralPath $settings) { [IO.File]::ReadAllBytes($settings) } else { $null }
try { & $reviewEditor @reviewArgs | Out-Null; $result = $LASTEXITCODE }
finally {
    if ($null -ne $savedSettings) { [IO.File]::WriteAllBytes($settings, $savedSettings) }
    elseif (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings }
}
if ($result -ne 0 -or -not (Select-String -LiteralPath $reviewLog -Pattern 'WITNESS_PROP_REVIEW PASS') -or
    (Select-String -LiteralPath $reviewLog -Pattern 'Failed to compile Material|Fatal error:|missing usage flag')) {
    throw "목격 소품 검사 실패: $reviewLog"
}
if (-not $Before -and -not (Select-String -LiteralPath $reviewLog -Pattern 'WITNESS_PROP_INTERACTION PASS count=3')) {
    throw '목격 소품 조사 검사가 끝나지 않았습니다.'
}
$prefix = if ($Before) { 'witness-before-' } else { 'witness-after-' }
foreach ($shot in @('pills','pills-close','butts','butts-close','roster','roster-close')) {
    $file = Get-Item -LiteralPath (Join-Path $reviewRoot "Docs/Media/$prefix$shot.png")
    if ($file.LastWriteTime -lt $reviewStart -or $file.Length -lt 10000) { throw "새 캡처 없음: $shot" }
}
Write-Host 'WITNESS_PROP_REVIEW PASS'
