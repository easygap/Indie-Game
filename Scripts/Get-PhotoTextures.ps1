# Downloads the CC0 photo-texture sets (ambientCG.com, 2K JPG) the game still
# samples and extracts them into Content/SourceArt/Photo/<Surface>. The zip is a
# download cache under Content/SourceArt/PhotoZips and is not kept in the repo.
# CC0 1.0: free for commercial use, modification and redistribution; each
# download is recorded in Docs/ASSET_POLICY.md.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$zipDir = Join-Path $projectRoot 'Content\SourceArt\PhotoZips'
$photoDir = Join-Path $projectRoot 'Content\SourceArt\Photo'
New-Item -ItemType Directory -Force $zipDir | Out-Null
New-Item -ItemType Directory -Force $photoDir | Out-Null

# Surface -> candidate ambientCG asset ids (first one that downloads wins).
$surfaces = [ordered]@{
    'Asphalt'     = @('Asphalt025', 'Asphalt026', 'Asphalt012')
    'Concrete'    = @('Concrete034', 'Concrete016', 'Concrete042')
    'Jangpan'     = @('WoodFloor051', 'WoodFloor040', 'WoodFloor007')
    'MetalBrushed'= @('Metal032', 'Metal012', 'Metal009')
    'Blanket'     = @('Fabric022', 'Fabric030', 'Fabric001')
    'WoodDark'    = @('Wood067', 'Wood051', 'Wood026')
    # 승강기 바닥 대리석. 복도 바닥과 벽은 생성 텍스처(SourceArt/AI의
    # PocheonGranite, LandingPaint)를 쓰고, 빌라 파사드 벽돌은 설비 재질이 맡는다.
    'MarbleFloor' = @('Marble016', 'Marble006', 'Terrazzo004')
}

$report = @()
foreach ($surface in $surfaces.Keys) {
    $downloaded = $false
    foreach ($assetId in $surfaces[$surface]) {
        $zipPath = Join-Path $zipDir "$assetId`_2K-JPG.zip"
        $url = "https://ambientcg.com/get?file=$assetId`_2K-JPG.zip"
        try {
            if (-not (Test-Path $zipPath)) {
                Write-Host "Downloading $assetId for $surface..."
                curl.exe -sL --max-time 120 -o $zipPath $url
            }
            $size = (Get-Item $zipPath).Length
            if ($size -lt 100KB) { Remove-Item $zipPath -Confirm:$false; continue }

            $extractDir = Join-Path $photoDir $surface
            if (Test-Path $extractDir) { Remove-Item -Recurse -Force -Confirm:$false $extractDir }
            Expand-Archive -Path $zipPath -DestinationPath $extractDir -Force
            $report += [pscustomobject]@{
                Surface = $surface; Asset = $assetId; SizeKB = [math]::Round($size / 1KB)
            }
            $downloaded = $true
            break
        }
        catch {
            Write-Warning "Failed $assetId for $surface`: $_"
            if (Test-Path $zipPath) { Remove-Item $zipPath -Confirm:$false }
        }
    }
    if (-not $downloaded) {
        Write-Warning "No photo texture obtained for $surface (procedural fallback stays)."
    }
}

$report | Format-Table -AutoSize
Write-Host "Photo texture download complete: $($report.Count)/$($surfaces.Count) sets."
