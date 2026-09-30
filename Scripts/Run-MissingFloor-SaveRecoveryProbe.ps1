[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ArchiveDirectory,
    [Parameter(Mandatory)][string]$SeedSaveFile,
    [string]$EvidenceDirectory,
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 90
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$archiveRoot = (Resolve-Path -LiteralPath $ArchiveDirectory).Path.TrimEnd('\', '/')
$seedPath = (Resolve-Path -LiteralPath $SeedSaveFile).Path
if (-not (Test-Path -LiteralPath $seedPath -PathType Leaf)) { throw '검사에 쓸 입주 저장 파일이 없습니다.' }
$seedHash = (Get-FileHash -LiteralPath $seedPath -Algorithm SHA256).Hash
$seedBytes = [IO.File]::ReadAllBytes($seedPath)
if ($seedBytes.Length -lt 64) { throw '검사에 쓸 입주 저장 파일이 너무 짧습니다.' }
# 속성 구조와 저장 시각은 유지하고 맵 이름만 같은 길이의 없는 이름으로 바꾼다.
$missingMapBytes = [byte[]]$seedBytes.Clone()
$mapToken = [Text.Encoding]::ASCII.GetBytes('/Game/Maps/Prologue_Morning')
$missingMapToken = [Text.Encoding]::ASCII.GetBytes('/Game/Maps/Prologue_Missing')
$mapMatches = 0
for ($offset = 0; $offset -le $seedBytes.Length - $mapToken.Length; $offset++) {
    $isTokenMatch = $true
    for ($index = 0; $index -lt $mapToken.Length; $index++) {
        if ($seedBytes[$offset + $index] -ne $mapToken[$index]) { $isTokenMatch = $false; break }
    }
    if ($isTokenMatch) {
        [Array]::Copy($missingMapToken, 0, $missingMapBytes, $offset, $missingMapToken.Length)
        $mapMatches++
    }
}
if ($mapMatches -ne 1) { throw '입주 저장 파일의 맵 이름을 한 곳에서 찾을 수 없습니다.' }
& (Join-Path $PSScriptRoot 'Test-WindowsPackageManifest.ps1') -ArchiveDirectory $archiveRoot
# 패키지 안의 Shipping 실행 파일을 직접 실행해 저장 복원을 확인한다.
$launcher = Join-Path $archiveRoot 'Windows/IndieGame/Binaries/Win64/IndieGame-Win64-Shipping.exe'
$manifest = Get-Content -LiteralPath (Join-Path $archiveRoot 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if (-not $EvidenceDirectory) {
    $EvidenceDirectory = Join-Path $projectRoot ('Saved/Validation/SaveRecovery-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
}
$evidenceRoot = [IO.Path]::GetFullPath($EvidenceDirectory).TrimEnd('\', '/')
if ($evidenceRoot -eq $archiveRoot -or
    $evidenceRoot.StartsWith($archiveRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw '저장 복원 검사 결과는 배포 폴더 밖에 두어야 합니다.'
}
if ((Test-Path -LiteralPath $evidenceRoot) -and @(Get-ChildItem -LiteralPath $evidenceRoot -Force).Count) {
    throw "검사 결과는 새 폴더에 저장해야 합니다: $evidenceRoot"
}
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$results = [Collections.Generic.List[object]]::new()

function Invoke-SaveCase([string]$Name, [int]$ValidSlot, [int[]]$DamagedSlots, [switch]$Truncated, [switch]$MissingMap) {
    $caseRoot = Join-Path $evidenceRoot $Name
    $userRoot = Join-Path $caseRoot 'User'
    $saveRoot = Join-Path $userRoot 'Saved/SaveGames'
    $receipt = Join-Path $caseRoot 'receipt.txt'
    New-Item -ItemType Directory -Path $saveRoot -Force | Out-Null
    if ($ValidSlot -ge 0) {
        [IO.File]::WriteAllBytes((Join-Path $saveRoot "AutoSave_$ValidSlot.sav"), $seedBytes)
    }
    foreach ($slot in $DamagedSlots) {
        # 원본을 건드리지 않고 이 실행의 복사본만 손상시킨다.
        $damaged = if ($MissingMap) { $missingMapBytes }
            elseif ($Truncated) { [byte[]]$seedBytes[0..31] }
            else { [Text.Encoding]::UTF8.GetBytes('invalid-save-fixture') }
        [IO.File]::WriteAllBytes((Join-Path $saveRoot "AutoSave_$slot.sav"), $damaged)
    }
    $before = @(Get-ChildItem -LiteralPath $saveRoot -Filter '*.sav' | Sort-Object Name | ForEach-Object {
        $_.Name + ' ' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    })
    $arguments = @('-unattended', '-nosplash', '-NoLoadingScreen', '-RenderOffscreen', '-nullrhi', '-nosound',
        '-IGSkipFrontend', '-IGMissingFloor', '-IGArrivalSaveRead',
        "-UserDir=$userRoot", "-IGMissingFloorResultPath=$receipt")
    $quoted = @($arguments | ForEach-Object {
        if ($_.Contains('"')) { throw '실행 인자에 잘못된 따옴표가 있습니다.' }
        '"' + $_ + '"'
    })
    $started = [DateTime]::UtcNow
    $process = Start-Process -FilePath $launcher -ArgumentList $quoted -WorkingDirectory (Split-Path $launcher -Parent) -WindowStyle Hidden -PassThru
    try {
        $null = $process.Handle
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            & taskkill.exe /PID $process.Id /T /F | Out-Null
            throw "저장 복원 검사 시간 초과: $Name"
        }
        $exitCode = $process.ExitCode
    }
    finally { $process.Dispose() }
    $expectSuccess = $ValidSlot -ge 0
    $expectedResult = if ($expectSuccess) { 'PASS' } else { 'FAIL' }
    # UE 5.8의 창 없는 실행에서는 RequestExitWithStatus(false, 1)도 0으로
    # 끝난다. 따라서 예상한 실패도 새 영수증의 FAIL을 반드시 확인한다.
    if (($expectSuccess -and $exitCode -ne 0) -or (-not $expectSuccess -and $exitCode -notin @(0, 1))) {
        throw "저장 복원 검사 종료 코드가 다릅니다: $Name actual=$exitCode"
    }
    if (-not (Test-Path -LiteralPath $receipt -PathType Leaf)) {
        throw "저장 복원 검사 영수증이 없습니다: $Name"
    }
    $receiptFile = Get-Item -LiteralPath $receipt
    $receiptText = (Get-Content -LiteralPath $receipt -Raw -Encoding UTF8).Trim()
    if ($receiptFile.LastWriteTimeUtc -lt $started -or
        $receiptText -notmatch "^MISSINGFLOOR_GREYBOX $expectedResult step=[0-9]+$") {
        throw "저장 복원 결과가 예상과 다릅니다: $Name $receiptText"
    }
    $after = @(Get-ChildItem -LiteralPath $saveRoot -Filter '*.sav' | Sort-Object Name | ForEach-Object {
        $_.Name + ' ' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    })
    if (($before -join "`n") -cne ($after -join "`n")) {
        throw "불러오기 검사 도중 저장 파일을 덮어썼습니다: $Name"
    }
    $results.Add([ordered]@{
        name = $Name; passed = $true; restored = $expectSuccess; exitCode = $exitCode
        savesUnchanged = $true; seconds = [math]::Round(([DateTime]::UtcNow - $started).TotalSeconds, 2)
        receipt = $receiptText; receiptSha256 = (Get-FileHash -LiteralPath $receipt -Algorithm SHA256).Hash
    })
    Write-Host "SAVE_RECOVERY_CASE PASS $Name restored=$expectSuccess"
}

# 정상 슬롯 하나만으로 입주 조사·상자·취침 상태를 복원해야 한다.
Invoke-SaveCase 'SingleValidSlot' 0 @()
Invoke-SaveCase 'CorruptSlot0' 1 @(0)
Invoke-SaveCase 'CorruptSlot1' 0 @(1)
Invoke-SaveCase 'TruncatedSlot0' 1 @(0) -Truncated
Invoke-SaveCase 'MissingMapSlot0' 1 @(0) -MissingMap
Invoke-SaveCase 'OnlyMissingMap' -1 @(0) -MissingMap
Invoke-SaveCase 'BothCorrupt' -1 @(0, 1)
Invoke-SaveCase 'NoSave' -1 @()
if ((Get-FileHash -LiteralPath $seedPath -Algorithm SHA256).Hash -ne $seedHash) {
    throw '원본 저장 파일이 검사 도중 바뀌었습니다.'
}
& (Join-Path $PSScriptRoot 'Test-WindowsPackageManifest.ps1') -ArchiveDirectory $archiveRoot
[ordered]@{
    schemaVersion = 1; createdAt = [DateTime]::UtcNow.ToString('o'); commit = $manifest.commit
    hasLocalChanges = $manifest.hasLocalChanges; seedSaveSha256 = $seedHash; seedUnchanged = $true
    scope = 'Shipping에서 입주 저장 복원, 손상되거나 맵이 없는 슬롯 건너뛰기, 불러올 수 있는 저장이 없을 때의 명시적 실패를 검사합니다. 타이틀 오류 안내와 밤별 저장, 엔딩 복원은 별도 검사 대상입니다.'
    cases = $results.ToArray()
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'summary.json') -Encoding UTF8
Write-Host "SAVE_RECOVERY_PROBE PASS cases=$($results.Count) evidence=$evidenceRoot"
