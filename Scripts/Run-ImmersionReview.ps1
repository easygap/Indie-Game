[CmdletBinding()]
param([int]$Width = 1920, [int]$Height = 1080)
$ErrorActionPreference = 'Stop'
$uxRoot = Split-Path -Parent $PSScriptRoot
$uxEditor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -Commandlet
$uxLog = Join-Path $uxRoot "Saved/Logs/ImmersionReview-${Width}x${Height}.log"
$uxSettings = Join-Path $uxRoot 'Saved/Config/WindowsEditor/GameUserSettings.ini'
$uxBackup = if (Test-Path -LiteralPath $uxSettings) { [IO.File]::ReadAllBytes($uxSettings) } else { $null }
$uxStart = Get-Date
try {
    & $uxEditor (Join-Path $uxRoot 'IndieGame.uproject') -game -unattended -nosplash -NoLoadingScreen `
        -RenderOffscreen -d3d12 -nosound -Windowed "-ResX=$Width" "-ResY=$Height" -ForceRes `
        -IGMissingFloor -IGIgnoreDirectStart -IGArrivalCapture -IGImmersionReview -IGSkipFrontend `
        '-ExecCmds=Scalability 2,r.ScreenPercentage 100,t.MaxFPS 60' "-abslog=$uxLog" | Out-Null
    $uxResult = $LASTEXITCODE
}
finally {
    if ($null -ne $uxBackup) { [IO.File]::WriteAllBytes($uxSettings, $uxBackup) }
    elseif (Test-Path -LiteralPath $uxSettings) { Remove-Item -LiteralPath $uxSettings }
}
Select-String -LiteralPath $uxLog -Pattern 'IMMERSION_' | ForEach-Object { Write-Host $_.Line }
if ($uxResult -ne 0 -or -not (Select-String -LiteralPath $uxLog -Pattern 'IMMERSION_REVIEW PASS failures=0')) {
    throw "안내 화면 검사 실패: $uxLog"
}
foreach ($uxShot in @('tutorial','quiet','guide-keyboard','guide-gamepad','reading')) {
    $uxImage = Get-Item -LiteralPath (Join-Path $uxRoot "Docs/Media/ux-$uxShot.png")
    if ($uxImage.LastWriteTime -lt $uxStart -or $uxImage.Length -lt 10000) { throw "새 화면 없음: $uxShot" }
    Move-Item -LiteralPath $uxImage.FullName -Destination (Join-Path $uxRoot "Docs/Media/ux-$uxShot-${Width}x${Height}.png") -Force
}
