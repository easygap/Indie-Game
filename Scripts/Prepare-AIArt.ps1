# Turns raw ImageGen artwork into the exact texture sheets the prologue needs.
#
# ImageGen returns whatever aspect ratio it feels like (usually near-square,
# with the actual label floating on a background). The game needs precise
# power-of-two sheets that wrap authored geometry: a bottle label has to be
# circumference-by-height or it shears when it goes round the sleeve.
#
# So each entry declares the crop taken from the raw art (as fractions of the
# source, which survives ImageGen changing its output resolution) and the
# final pixel size. Output lands in Content/SourceArt next to the
# System.Drawing-generated textures, and generate_surface_textures.py imports
# the whole folder without caring which tool made which file.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$projectRoot = Split-Path -Parent $PSScriptRoot
$aiDir = Join-Path $projectRoot 'Content\SourceArt\AI'
$outDir = Join-Path $projectRoot 'Content\SourceArt'

# Source stem -> crop rectangle (left, top, width, height as 0..1 fractions)
# and the target texture size. CropTight trims the flat border ImageGen puts
# around a "flat lay" design so the label fills the sheet edge to edge.
# Patches are applied after the crop, in final-sheet pixel coordinates.
# ImageGen renders headline Hangul cleanly but invents fine print, and any
# invented brand name is a trademark risk however small it is on screen. So
# every body-copy block that names the product gets painted out and rewritten
# with our own fictional wording — the artwork is the AI's, the words are ours.
$patchesWater = @(
    @{ Rect = @(4, 0, 176, 84); Fill = '#FFFFFF'
       Lines = @('새벽수는 이 동네 지하 200 m,', '아무도 손대지 않은 물길에서', '밤사이 길어 올린 물입니다.')
       FontSize = 12; Color = '#1F3A66'; LineHeight = 17 }
    @{ Rect = @(921, 163, 96, 36); Fill = '#FFFFFF'
       Lines = @('(주)새벽수'); FontSize = 12; Color = '#1F3A66'; LineHeight = 16 }
    @{ Rect = @(921, 213, 96, 36); Fill = '#FFFFFF'
       Lines = @('(주)새벽수'); FontSize = 12; Color = '#1F3A66'; LineHeight = 16 }
    @{ Rect = @(852, 0, 172, 14); Fill = '#FFFFFF'
       Lines = @(); FontSize = 12; Color = '#1F3A66'; LineHeight = 16 }
)

$plan = @(
    [pscustomobject]@{
        Source = 'LabelWater_raw'; Target = 'T_LabelWater_D.png'
        Crop = @(0.045, 0.185, 0.910, 0.575); Size = @(1024, 416)
        Patches = $patchesWater
    }
    [pscustomobject]@{
        Source = 'LabelRamyeon_raw'; Target = 'T_LabelRamyeon_D.png'
        Crop = @(0.009, 0.125, 0.986, 0.771); Size = @(1024, 250)
    }
    [pscustomobject]@{
        Source = 'SignMain_raw'; Target = 'T_SignMain_D.png'
        Crop = @(0.030, 0.360, 0.940, 0.280); Size = @(1024, 256)
    }
    # Grid sheets: one generation carries four or six designs, which is the
    # only way to get twenty textures out of a tool that takes five minutes a
    # picture. Each cell is cut out separately by the same crop machinery.
    [pscustomobject]@{
        Source = 'SheetSnacks'; Target = 'T_SnackShrimp_D.png'
        Crop = @(0.092, 0.006, 0.318, 0.484); Size = @(512, 640)
    }
    [pscustomobject]@{
        Source = 'SheetSnacks'; Target = 'T_SnackPotato_D.png'
        Crop = @(0.592, 0.004, 0.303, 0.476); Size = @(512, 640)
    }
    [pscustomobject]@{
        Source = 'SheetSnacks'; Target = 'T_SnackSquid_D.png'
        Crop = @(0.086, 0.508, 0.331, 0.487); Size = @(512, 640)
    }
    [pscustomobject]@{
        Source = 'SheetSnacks'; Target = 'T_SnackCorn_D.png'
        Crop = @(0.592, 0.515, 0.335, 0.476); Size = @(512, 640)
    }
    [pscustomobject]@{
        Source = 'SheetBottles'; Target = 'T_LabelGreenTea_D.png'
        Crop = @(0.022, 0.011, 0.956, 0.231); Size = @(1024, 416)
    }
    [pscustomobject]@{
        Source = 'SheetBottles'; Target = 'T_LabelBarley_D.png'
        Crop = @(0.022, 0.263, 0.956, 0.229); Size = @(1024, 416)
    }
    [pscustomobject]@{
        Source = 'SheetBottles'; Target = 'T_LabelSoda_D.png'
        Crop = @(0.022, 0.506, 0.956, 0.230); Size = @(1024, 416)
    }
    [pscustomobject]@{
        Source = 'SheetBottles'; Target = 'T_LabelSoju_D.png'
        Crop = @(0.022, 0.754, 0.956, 0.232); Size = @(1024, 416)
    }
    [pscustomobject]@{
        Source = 'SheetSigns'; Target = 'T_SignLaundry_D.png'
        Crop = @(0.019, 0.011, 0.965, 0.148); Size = @(512, 128)
    }
    [pscustomobject]@{
        Source = 'SheetSigns'; Target = 'T_SignHair_D.png'
        Crop = @(0.019, 0.174, 0.965, 0.152); Size = @(512, 128)
    }
    [pscustomobject]@{
        Source = 'SheetSigns'; Target = 'T_SignHof_D.png'
        Crop = @(0.019, 0.343, 0.965, 0.153); Size = @(512, 128)
    }
    [pscustomobject]@{
        Source = 'SheetSigns'; Target = 'T_SignSuper_D.png'
        Crop = @(0.019, 0.512, 0.965, 0.153); Size = @(512, 128)
    }
    [pscustomobject]@{
        Source = 'SheetSigns'; Target = 'T_SignPC_D.png'
        Crop = @(0.019, 0.683, 0.965, 0.152); Size = @(512, 128)
    }
    [pscustomobject]@{
        Source = 'SheetSigns'; Target = 'T_SignKaraoke_D.png'
        Crop = @(0.019, 0.851, 0.965, 0.148); Size = @(512, 128)
    }
    [pscustomobject]@{
        Source = 'SheetPosters'; Target = 'T_PosterSale_D.png'
        Crop = @(0.082, 0.022, 0.386, 0.478); Size = @(512, 704)
    }
    [pscustomobject]@{
        Source = 'SheetPosters'; Target = 'T_PosterRamyeon_D.png'
        Crop = @(0.528, 0.016, 0.387, 0.496); Size = @(512, 352)
    }
    [pscustomobject]@{
        Source = 'SheetPosters'; Target = 'T_PosterFlyer_D.png'
        Crop = @(0.074, 0.516, 0.414, 0.451); Size = @(384, 512)
    }
    [pscustomobject]@{
        Source = 'SheetPosters'; Target = 'T_NoticeRent_D.png'
        Crop = @(0.528, 0.524, 0.383, 0.435); Size = @(384, 512)
    }
    # Blank aged paper: the base every readable note is printed onto. The
    # Korean copy is drawn on top at runtime by the HUD, never baked in.
    [pscustomobject]@{
        Source = 'SheetPaper'; Target = 'T_PaperClean_D.png'
        Crop = @(0.055, 0.020, 0.400, 0.460); Size = @(512, 724)
    }
    [pscustomobject]@{
        Source = 'SheetPaper'; Target = 'T_PaperWet_D.png'
        Crop = @(0.545, 0.020, 0.400, 0.460); Size = @(512, 724)
    }
    [pscustomobject]@{
        Source = 'SheetPaper'; Target = 'T_PaperFolded_D.png'
        Crop = @(0.055, 0.520, 0.400, 0.460); Size = @(512, 724)
    }
    [pscustomobject]@{
        Source = 'SheetPaper'; Target = 'T_PaperOld_D.png'
        Crop = @(0.545, 0.520, 0.400, 0.460); Size = @(512, 724)
    }
    # Versioned replacement for the first paper sheet, which was accidentally
    # populated with snack artwork. Keep the old source/outputs as provenance;
    # materials consume these V2 textures instead.
    [pscustomobject]@{
        Source = 'SheetPaperNotes_v2'; Target = 'T_PaperClean_V2_D.png'
        Crop = @(0.091, 0.023, 0.318, 0.453); Size = @(512, 724)
    }
    [pscustomobject]@{
        Source = 'SheetPaperNotes_v2'; Target = 'T_PaperWet_V2_D.png'
        Crop = @(0.594, 0.023, 0.315, 0.453); Size = @(512, 724)
    }
    [pscustomobject]@{
        Source = 'SheetPaperNotes_v2'; Target = 'T_PaperFolded_V2_D.png'
        Crop = @(0.091, 0.522, 0.320, 0.453); Size = @(512, 724)
    }
    [pscustomobject]@{
        Source = 'SheetPaperNotes_v2'; Target = 'T_PaperOld_V2_D.png'
        Crop = @(0.588, 0.522, 0.323, 0.453); Size = @(512, 724)
    }
)

$written = 0
foreach ($entry in $plan) {
    $sourcePath = Join-Path $aiDir ($entry.Source + '.png')
    if (-not (Test-Path $sourcePath)) {
        Write-Host "skip $($entry.Source): not generated yet"
        continue
    }

    $source = [System.Drawing.Image]::FromFile($sourcePath)
    try {
        $cropRect = New-Object System.Drawing.Rectangle(
            [int]($source.Width * $entry.Crop[0]),
            [int]($source.Height * $entry.Crop[1]),
            [int]($source.Width * $entry.Crop[2]),
            [int]($source.Height * $entry.Crop[3]))

        $target = New-Object System.Drawing.Bitmap($entry.Size[0], $entry.Size[1])
        $graphics = [System.Drawing.Graphics]::FromImage($target)
        # Labels are read close up and at a glancing angle, so resampling
        # quality matters more here than in almost any other texture.
        $graphics.InterpolationMode = 'HighQualityBicubic'
        $graphics.PixelOffsetMode = 'HighQuality'
        $graphics.SmoothingMode = 'HighQuality'
        $destRect = New-Object System.Drawing.Rectangle(0, 0, $entry.Size[0], $entry.Size[1])
        $graphics.DrawImage($source, $destRect, $cropRect, [System.Drawing.GraphicsUnit]::Pixel)

        # Paint out and rewrite any fine print the generator invented.
        $graphics.TextRenderingHint = 'AntiAliasGridFit'
        foreach ($patch in $entry.Patches) {
            $fill = [System.Drawing.ColorTranslator]::FromHtml($patch.Fill)
            $brush = New-Object System.Drawing.SolidBrush($fill)
            $graphics.FillRectangle(
                $brush, $patch.Rect[0], $patch.Rect[1], $patch.Rect[2], $patch.Rect[3])
            $brush.Dispose()

            if ($patch.Lines.Count -eq 0) { continue }
            $ink = New-Object System.Drawing.SolidBrush(
                [System.Drawing.ColorTranslator]::FromHtml($patch.Color))
            $font = New-Object System.Drawing.Font(
                'Malgun Gothic', [single]$patch.FontSize,
                [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
            for ($i = 0; $i -lt $patch.Lines.Count; $i++) {
                $graphics.DrawString(
                    $patch.Lines[$i], $font, $ink,
                    [single]$patch.Rect[0], [single]($patch.Rect[1] + $i * $patch.LineHeight))
            }
            $font.Dispose(); $ink.Dispose()
        }

        $graphics.Dispose()

        $targetPath = Join-Path $outDir $entry.Target
        $target.Save($targetPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $target.Dispose()
        Write-Host "wrote $($entry.Target)  ($($entry.Size[0])x$($entry.Size[1]))  from $($source.Width)x$($source.Height)"
        $written++
    }
    finally {
        $source.Dispose()
    }
}

Write-Host "AI art prepared: $written/$($plan.Count)"
