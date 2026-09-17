[CmdletBinding()]
param([int]$Width=1920, [int]$Height=1080, [ValidateSet(1,2)][int]$TextScale=1)
$ErrorActionPreference = 'Stop'
$readingRoot = Split-Path -Parent $PSScriptRoot
$readingEditor = & "$PSScriptRoot/Resolve-UnrealEditor.ps1" -Commandlet
$readingKey = "${Width}x${Height}-$TextScale"
$readingLog = "$readingRoot/Saved/Logs/ReadingReview-$readingKey.log"
$readingSettings = "$readingRoot/Saved/Config/WindowsEditor/GameUserSettings.ini"
$readingBackup = if (Test-Path -LiteralPath $readingSettings) { [IO.File]::ReadAllBytes($readingSettings) } else { $null }
$readingStart = Get-Date
try {
    & $readingEditor "$readingRoot/IndieGame.uproject" -game -unattended -nosplash -NoLoadingScreen `
        -RenderOffscreen -d3d12 -nosound -Windowed "-ResX=$Width" "-ResY=$Height" -ForceRes `
        -IGMissingFloor -IGIgnoreDirectStart -IGArrivalCapture -IGReadingReview -IGSkipFrontend `
        "-IGCaptionScale=$TextScale" '-ExecCmds=Scalability 2,r.ScreenPercentage 100,t.MaxFPS 60' "-abslog=$readingLog" | Out-Null
    $readingResult = $LASTEXITCODE
}
finally {
    if ($null -ne $readingBackup) { [IO.File]::WriteAllBytes($readingSettings,$readingBackup) }
    elseif (Test-Path -LiteralPath $readingSettings) { Remove-Item -LiteralPath $readingSettings }
}
Select-String -LiteralPath $readingLog -Pattern 'READING_' | ForEach-Object { Write-Host $_.Line }
if ($readingResult -ne 0 -or -not (Select-String -LiteralPath $readingLog -Pattern 'READING_REVIEW PASS failures=0')) {
    throw "문서 실행 검사 실패: $readingLog"
}
$readingShots = @(Get-ChildItem -LiteralPath "$readingRoot/Docs/Media" -Filter 'reading-*.png' |
    Where-Object { $_.LastWriteTime -gt $readingStart })
if ($readingShots.Count -lt 5) { throw '문서 캡처가 빠졌다' }
foreach ($shot in $readingShots) {
    Move-Item -LiteralPath $shot.FullName -Destination "$readingRoot/Docs/Media/$($shot.BaseName)-$readingKey.png" -Force
}
