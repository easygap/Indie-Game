[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$liftRoot = Split-Path -Parent $PSScriptRoot
$liftProject = Join-Path $liftRoot 'IndieGame.uproject'
$liftEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -ProjectPath $liftProject -Commandlet
$liftLog = Join-Path $liftRoot 'Saved/Logs/ElevatorRoundtrip.log'
# 같은 승강기를 쓰는 기존 중간 정차 시나리오로 왕복과 층 표시를 함께 검사한다.
$liftArgs = @($liftProject, '/Game/Maps/Prologue_Morning', '-game', '-unattended', '-nosplash',
    '-NoLoadingScreen', '-RenderOffscreen', '-d3d12', '-nosound', '-Windowed',
    '-ResX=1920', '-ResY=1080', '-ForceRes', '-IGCaptureCH02', '-IGSkipFrontend',
    '-ExecCmds=Scalability 2,sg.ResolutionQuality 100,r.ScreenPercentage 100', "-abslog=$liftLog")
$settingsPath = Join-Path $liftRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$settingsBytes = if (Test-Path -LiteralPath $settingsPath) { [IO.File]::ReadAllBytes($settingsPath) } else { $null }
try {
    & $liftEditor @liftArgs | Out-Null
    $liftExit = $LASTEXITCODE
} finally {
    if ($null -ne $settingsBytes) { [IO.File]::WriteAllBytes($settingsPath, $settingsBytes) }
    elseif (Test-Path -LiteralPath $settingsPath) { Remove-Item -LiteralPath $settingsPath }
}
$liftText = Get-Content -LiteralPath $liftLog -Raw
if ($liftExit -ne 0 -or $liftText -notmatch 'CH02 elevator roundtrip validation complete\.' -or
    $liftText -match 'Fatal error:|Failed to compile Material') { throw "승강기 왕복 검사 실패: $liftLog" }
$floors = [regex]::Matches($liftText, 'ELEVATOR_DISPLAY floor=(\d)') | ForEach-Object { $_.Groups[1].Value }
if (($floors -join ',') -notmatch '4,3,2,(2,)*1,(1,)*2,3,4') { throw "층 표시 순서 오류: $($floors -join ',')" }
Write-Host "ELEVATOR_ROUNDTRIP PASS intermediate=2 lower=1 upper=4 display=$($floors -join ',')"
