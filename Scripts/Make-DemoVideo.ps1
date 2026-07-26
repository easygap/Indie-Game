# Assembles Saved/DemoFrames/frame_%05d.png (dumped by -IGDemoFrames) into
# Docs/Media/prologue-walkthrough.mp4 using ffmpeg.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$framesDir = Join-Path $projectRoot 'Saved\DemoFrames'
$outPath = Join-Path $projectRoot 'Docs\Media\prologue-walkthrough.mp4'

$frameCount = @(Get-ChildItem -Path $framesDir -Filter 'frame_*.png' -ErrorAction Stop).Count
if ($frameCount -lt 20) {
    throw "Too few demo frames in $framesDir ($frameCount); run the game with -IGDemoFrames first."
}

# The last frame can still be mid-write when the game exits; encoding it
# fails the whole run, so drop any zero-length or truncated tail frame.
$lastFrame = Get-ChildItem -Path $framesDir -Filter 'frame_*.png' | Sort-Object Name | Select-Object -Last 1
if ($lastFrame.Length -lt 1024) {
    Remove-Item $lastFrame.FullName -Force -Confirm:$false
    $frameCount--
}

ffmpeg -y -framerate 7 -i (Join-Path $framesDir 'frame_%05d.png') `
    -c:v libx264 -pix_fmt yuv420p -crf 22 `
    -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" `
    $outPath
if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed with exit code $LASTEXITCODE" }

$sizeMB = [math]::Round((Get-Item $outPath).Length / 1MB, 1)
Write-Host "Wrote $outPath ($frameCount frames, $sizeMB MB)"
