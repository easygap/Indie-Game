[CmdletBinding()]
param([string]$ArchiveDirectory)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) {
    $ArchiveDirectory = Join-Path $projectRoot ('Saved/Packages/Windows-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$ArchiveDirectory = [IO.Path]::GetFullPath($ArchiveDirectory)
if ((Test-Path -LiteralPath $ArchiveDirectory) -and
    @(Get-ChildItem -LiteralPath $ArchiveDirectory -Force).Count -gt 0) {
    throw "배포 폴더는 비어 있어야 합니다: $ArchiveDirectory"
}

# 한글 경로의 빌드 복사는 기존 스크립트 한 곳에서 관리한다.
& (Join-Path $PSScriptRoot 'Build-ArtAssets.ps1') -CodeOnly
$buildRoot = $projectRoot
if ($projectRoot -match '[^\x00-\x7F]') {
    $bytes = [Text.Encoding]::UTF8.GetBytes($projectRoot.ToLowerInvariant())
    $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).Substring(0, 8).ToLowerInvariant()
    $buildRoot = Join-Path $env:LOCALAPPDATA "IndieGame/AsciiBuild/Art_$hash"
}
$buildProject = Join-Path $buildRoot 'IndieGame.uproject'
$editor = & (Join-Path $PSScriptRoot 'Resolve-UnrealEditor.ps1') -Commandlet
$engineRoot = Split-Path (Split-Path (Split-Path $editor -Parent) -Parent) -Parent
$uat = Join-Path $engineRoot 'Build/BatchFiles/RunUAT.bat'
# UE 5.8은 지정한 보관 폴더에 실행 파일을 바로 놓는다.
$windowsDirectory = Join-Path $ArchiveDirectory 'Windows'
$arguments = @(
    'BuildCookRun', "-project=$buildProject", '-target=IndieGame',
    '-noP4', '-unattended', '-utf8output', '-platform=Win64', '-clientconfig=Shipping',
    '-build', '-cook', '-allmaps', '-stage', '-pak', '-iostore', '-package',
    '-compressed', '-nodebuginfo', '-prereqs',
    '-applocaldirectory=$(EngineDir)/Binaries/ThirdParty/AppLocalDependencies',
    '-archive', "-archivedirectory=$windowsDirectory"
)
& $uat @arguments
if ($LASTEXITCODE -ne 0) { throw "Windows 배포 파일 생성 실패: $LASTEXITCODE" }

$launcher = Join-Path $ArchiveDirectory 'Windows/IndieGame.exe'
$game = Join-Path $ArchiveDirectory 'Windows/IndieGame/Binaries/Win64/IndieGame-Win64-Shipping.exe'
& (Join-Path $PSScriptRoot 'Copy-Windows-ExecutableVersionResource.ps1') -SourceExecutable $game -DestinationExecutable $launcher
foreach ($exe in @($launcher, $game)) {
    & (Join-Path $PSScriptRoot 'Test-Windows-ExecutableMetadata.ps1') -Executable $exe
}
$files = Get-ChildItem -LiteralPath (Join-Path $ArchiveDirectory 'Windows') -File -Recurse | ForEach-Object {
    [ordered]@{
        path = [IO.Path]::GetRelativePath($ArchiveDirectory, $_.FullName).Replace('\', '/')
        bytes = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
[ordered]@{
    createdAt = [DateTime]::UtcNow.ToString('o')
    commit = (& git -C $projectRoot rev-parse HEAD)
    hasLocalChanges = [bool](& git -C $projectRoot status --porcelain)
    buildProject = $buildProject
    files = @($files)
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $ArchiveDirectory 'manifest.json') -Encoding utf8
Write-Host "WINDOWS_PACKAGE PASS $ArchiveDirectory"
