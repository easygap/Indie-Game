[CmdletBinding()]
param([switch]$Before, [switch]$Night)
$ErrorActionPreference = 'Stop'
$boothRoot = Split-Path -Parent $PSScriptRoot
$boothProject = Join-Path $boothRoot 'IndieGame.uproject'
$boothEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $boothProject -Commandlet
$boothLog = Join-Path $boothRoot "Saved/Logs/BoothReview-$Before-$Night.log"
$boothStart = Get-Date
$boothArgs = @($boothProject, '-game', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed', '-ResX=1920', '-ResY=1080', '-ForceRes',
    '-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalCapture', '-IGBoothReview', '-IGSkipFrontend',
    '-ExecCmds=Scalability 2,sg.ResolutionQuality 100,r.ScreenPercentage 100', "-abslog=$boothLog")
if ($Before) { $boothArgs += '-IGBoothBaseline' }
if ($Night) { $boothArgs += '-IGBoothNight' }
$settings = Join-Path $boothRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$savedSettings = if (Test-Path -LiteralPath $settings) { [IO.File]::ReadAllBytes($settings) } else { $null }
try { & $boothEditor @boothArgs | Out-Null; $result = $LASTEXITCODE }
finally {
    if ($null -ne $savedSettings) { [IO.File]::WriteAllBytes($settings, $savedSettings) }
    elseif (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings }
}
if ($result -ne 0 -or -not (Select-String -LiteralPath $boothLog -Pattern 'BOOTH_REVIEW PASS') -or
    (Select-String -LiteralPath $boothLog -Pattern 'Failed to compile Material|Fatal error:|missing usage flag')) {
    throw "관리실 화면 검사 실패: $boothLog"
}
$prefix = if ($Before) { 'booth-before-' } elseif ($Night) { 'booth-night-' } else { 'booth-' }
foreach ($shot in @('desk','ledger','pad-blank','pad-partial','pad-first','pad-second','pad-complete','desk-return')) {
    $file = Get-Item -LiteralPath (Join-Path $boothRoot "Docs/Media/$prefix$shot.png")
    if ($file.LastWriteTime -lt $boothStart -or $file.Length -lt 10000) { throw "새 캡처 없음: $shot" }
}
Write-Host 'BOOTH_REVIEW PASS shots=8'
