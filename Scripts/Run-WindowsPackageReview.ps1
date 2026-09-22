[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ArchiveDirectory,
    [string]$EvidenceDirectory,
    [ValidateRange(60, 900)][int]$TimeoutSeconds = 360
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$archiveRoot = (Resolve-Path -LiteralPath $ArchiveDirectory).Path
$launcher = Join-Path $archiveRoot 'Windows/IndieGame.exe'
if (-not (Test-Path -LiteralPath $launcher)) { throw "실행 파일이 없습니다: $launcher" }
if (-not $EvidenceDirectory) {
    $EvidenceDirectory = Join-Path $projectRoot ('Saved/Validation/WindowsPackage-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$EvidenceDirectory = [IO.Path]::GetFullPath($EvidenceDirectory)
if ((Test-Path -LiteralPath $EvidenceDirectory) -and @(Get-ChildItem -LiteralPath $EvidenceDirectory -Force).Count) {
    throw "검사 결과는 새 폴더에 저장해야 합니다: $EvidenceDirectory"
}
New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null

function Get-ArchiveHashes {
    @(Get-ChildItem -LiteralPath $archiveRoot -Recurse -File | Sort-Object FullName | ForEach-Object {
        '{0} {1}' -f [IO.Path]::GetRelativePath($archiveRoot, $_.FullName), (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    })
}
$before = Get-ArchiveHashes
$results = [Collections.Generic.List[object]]::new()

function Invoke-GameCase([string]$Name, [string[]]$ExtraArguments, [switch]$Audio, [string]$UserDirectory) {
    $caseRoot = Join-Path $EvidenceDirectory $Name
    New-Item -ItemType Directory -Path $caseRoot -Force | Out-Null
    $receipt = Join-Path $caseRoot 'receipt.txt'
    if (-not $UserDirectory) { $UserDirectory = Join-Path $caseRoot 'User' }
    $arguments = @('-unattended', '-nosplash', '-NoLoadingScreen', '-RenderOffscreen',
        '-Windowed', '-ResX=1280', '-ResY=720', '-ForceRes', '-IGSkipFrontend',
        "-UserDir=$UserDirectory", "-IGMissingFloorResultPath=$receipt") + $ExtraArguments
    if ($Audio) {
        $arguments += @('-d3d12', '-AudioMixer', '-AllowCommandletAudio',
            '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0', '-ExecCmds=t.MaxFPS 60')
    }
    else { $arguments += '-nosound' }
    $quoted = @($arguments | ForEach-Object {
        if ($_.Contains('"')) { throw '실행 인자에 잘못된 따옴표가 있습니다.' }
        '"' + $_ + '"'
    })
    $started = [DateTime]::UtcNow
    $process = Start-Process -FilePath $launcher -ArgumentList $quoted -WorkingDirectory (Split-Path $launcher -Parent) -WindowStyle Hidden -PassThru
    try {
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $process.Kill($true)
            throw "게임 실행 검사 시간 초과: $Name"
        }
        if ($process.ExitCode -ne 0) { throw "게임 실행 검사 실패: $Name (종료 코드 $($process.ExitCode))" }
    }
    finally { $process.Dispose() }
    if ($Audio) {
        $audioReceipts = @(Get-ChildItem -LiteralPath $caseRoot -Filter receipt.txt -Recurse | Where-Object { $_.Directory.Name -eq 'AudioPresentation' })
        if ($audioReceipts.Count -ne 1) { throw '소리 검사 결과 파일이 없습니다.' }
        $receipt = $audioReceipts[0].FullName
        $text = Get-Content -LiteralPath $receipt -Raw
        if ($text -notmatch 'AUDIO_PRESENTATION_PROBE PASS failures=0' -or $text -match 'CHECK .+ FAIL') { throw "소리 검사 실패: $receipt" }
        $recording = Join-Path (Split-Path $receipt -Parent) 'presentation-mix.wav'
        & python (Join-Path $PSScriptRoot 'check_audio_presentation.py') $recording |
            Tee-Object -FilePath (Join-Path $caseRoot 'waveform.json')
        if ($LASTEXITCODE -ne 0) { throw '배포 파일의 실제 믹서 녹음이 검사를 통과하지 못했습니다.' }
    }
    elseif (-not (Test-Path -LiteralPath $receipt) -or (Get-Content -Raw -LiteralPath $receipt) -notmatch '^MISSINGFLOOR_GREYBOX PASS') {
        throw "게임 진행 검사 결과가 없습니다: $Name"
    }
    if ((Get-Item -LiteralPath $receipt).LastWriteTimeUtc -lt $started) { throw "이전 검사 결과가 남아 있습니다: $Name" }
    $results.Add([ordered]@{ name = $Name; passed = $true; seconds = [math]::Round(([DateTime]::UtcNow - $started).TotalSeconds, 2); receipt = $receipt })
    Write-Host "WINDOWS_GAME_CASE PASS $Name"
}

& (Join-Path $PSScriptRoot 'Run-Rebirth-FrontendShippingProbe.ps1') -ArchiveDirectory $archiveRoot -EvidenceDirectory (Join-Path $EvidenceDirectory 'Frontend')
$saveUser = Join-Path $EvidenceDirectory 'SaveUser'
Invoke-GameCase 'Arrival' @('-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalProbe', '-IGArrivalSaveWrite', '-d3d12') -UserDirectory $saveUser
$saveFiles = @(Get-ChildItem -LiteralPath $saveUser -Recurse -Filter 'AutoSave_*.sav')
if ($saveFiles.Count -lt 1 -or $saveFiles.Count -gt 2) { throw '자동 저장 파일이 생성되지 않았습니다.' }
Invoke-GameCase 'ArrivalResume' @('-IGMissingFloor', '-IGIgnoreDirectStart', '-IGArrivalSaveRead', '-d3d12') -UserDirectory $saveUser
Invoke-GameCase 'FullGame' @('-IGListenerGreybox', '-IGListenerGreyboxProbe', '-nullrhi')
Invoke-GameCase 'Audio' @('-IGAudioPresentationProbe') -Audio
$after = Get-ArchiveHashes
if (@(Compare-Object $before $after).Count) { throw '검사 중 배포 폴더의 파일이 바뀌었습니다.' }
[ordered]@{
    createdAt = [DateTime]::UtcNow.ToString('o')
    archive = $archiveRoot
    launcherSha256 = (Get-FileHash -LiteralPath $launcher -Algorithm SHA256).Hash
    archiveUnchanged = $true
    frontend = (Join-Path $EvidenceDirectory 'Frontend/summary.json')
    cases = $results.ToArray()
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'summary.json') -Encoding utf8
Write-Host "WINDOWS_PACKAGE_REVIEW PASS $EvidenceDirectory"
