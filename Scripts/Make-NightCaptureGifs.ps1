# Assembles the two -IGNightCapture frame bursts into README GIFs with a
# two-pass ffmpeg palette so the dark corridor does not band.
#   Saved/NightCapture/extinguisher -> Docs/Media/night1-extinguisher-drop.gif
#   Saved/NightCapture/chase        -> Docs/Media/night-listener-chase.gif
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

function Convert-Burst {
    param(
        [string]$BurstName,
        [string]$OutputName,
        [int]$Fps
    )
    $framesDir = Join-Path $projectRoot "Saved\NightCapture\$BurstName"
    $frameCount = @(Get-ChildItem -LiteralPath $framesDir -Filter 'frame_*.png' -ErrorAction SilentlyContinue).Count
    if ($frameCount -lt 8) {
        throw "Too few frames in $framesDir ($frameCount); run Run-MissingFloor-NightCapture.bat first."
    }
    $output = Join-Path $projectRoot "Docs\Media\$OutputName"
    ffmpeg -y -framerate $Fps -i (Join-Path $framesDir 'frame_%05d.png') `
        -vf "fps=$Fps,scale=960:-1:flags=lanczos,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4" `
        -loop 0 $output
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed for $BurstName with exit code $LASTEXITCODE" }
    $size = (Get-Item -LiteralPath $output).Length
    if ($size -gt 10MB) {
        throw "$OutputName is $([math]::Round($size / 1MB, 1)) MB; over the 10 MB review budget."
    }
    Write-Output ("{0}: {1} frames -> {2:N1} MB" -f $OutputName, $frameCount, ($size / 1MB))
}

# 6 fps matches roughly twice the sustained capture rate (each 1080p PNG
# write throttles the burst to ~3 fps), so playback reads as a brisk clip
# rather than a slideshow.
Convert-Burst -BurstName 'extinguisher' -OutputName 'night1-extinguisher-drop.gif' -Fps 6
Convert-Burst -BurstName 'chase' -OutputName 'night-listener-chase.gif' -Fps 6
