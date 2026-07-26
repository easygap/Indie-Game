# Renders wrap-around product label art for the store props (bottles, snack
# bags, cup ramyeon) into Content/SourceArt. Every brand here is fictional —
# no real trademarks are reproduced.
#
# Bottle labels are drawn at circumference:height aspect so they wrap a
# cylindrical label sleeve 1:1. Drop-in replacement: overwrite these PNGs with
# any other artwork of the same name/size and re-run the import step.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$projectRoot = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $projectRoot 'Content\SourceArt'
New-Item -ItemType Directory -Force $outDir | Out-Null

$malgun = 'Malgun Gothic'

function New-Canvas {
    param([int]$Width, [int]$Height)
    $bitmap = New-Object System.Drawing.Bitmap($Width, $Height)
    $g = [System.Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $g.InterpolationMode = 'HighQualityBicubic'
    return @($bitmap, $g)
}

function Save-Canvas {
    param($Bitmap, $Graphics, [string]$FileName)
    $Graphics.Dispose()
    $path = Join-Path $outDir $FileName
    $Bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $Bitmap.Dispose()
    Write-Host "Wrote $path"
}

function Fill-Vertical {
    param($G, [int]$W, [int]$H, [int]$Y, [int]$BandH, $ColorTop, $ColorBottom)
    $rect = New-Object System.Drawing.Rectangle(0, $Y, $W, $BandH)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $rect, $ColorTop, $ColorBottom, 90.0)
    $G.FillRectangle($brush, $rect)
    $brush.Dispose()
}

function Draw-Text {
    param($G, [string]$Text, [single]$Size, [System.Drawing.FontStyle]$Style,
        $Color, [single]$CX, [single]$CY, [string]$Family = 'Malgun Gothic')
    $font = New-Object System.Drawing.Font($Family, $Size, $Style, [System.Drawing.GraphicsUnit]::Pixel)
    $brush = New-Object System.Drawing.SolidBrush($Color)
    $format = New-Object System.Drawing.StringFormat
    $format.Alignment = 'Center'
    $format.LineAlignment = 'Center'
    $G.DrawString($Text, $font, $brush, $CX, $CY, $format)
    $format.Dispose(); $brush.Dispose(); $font.Dispose()
}

function Draw-Barcode {
    param($G, [single]$X, [single]$Y, [single]$W, [single]$H, [int]$Seed)
    $white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $G.FillRectangle($white, $X, $Y, $W, $H)
    $white.Dispose()
    $black = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::Black)
    $cursor = $X + 3
    $value = $Seed
    while ($cursor -lt ($X + $W - 4)) {
        $value = ($value * 1103515245 + 12345) -band 0x7FFFFFFF
        $barWidth = 1 + ($value % 3)
        $gap = 1 + (($value -shr 8) % 3)
        $G.FillRectangle($black, $cursor, $Y + 2, $barWidth, $H - 10)
        $cursor += $barWidth + $gap
    }
    $black.Dispose()
    Draw-Text $G '8 801234 567890' 11 ([System.Drawing.FontStyle]::Regular) `
        ([System.Drawing.Color]::Black) ($X + $W / 2) ($Y + $H - 5)
}

function Draw-NutritionStrip {
    param($G, [single]$X, [single]$Y, [single]$W, [single]$H, $LineColor)
    $pen = New-Object System.Drawing.Pen($LineColor, 1.5)
    $G.DrawRectangle($pen, $X, $Y, $W, $H)
    for ($i = 1; $i -lt 4; $i++) {
        $y = $Y + ($H / 4) * $i
        $G.DrawLine($pen, $X, $y, ($X + $W), $y)
    }
    $G.DrawLine($pen, ($X + $W * 0.62), $Y, ($X + $W * 0.62), ($Y + $H))
    $pen.Dispose()
}

# ---------------------------------------------------------------------------
# Bottle labels (wrap sleeves) — 1024 x 416
# ---------------------------------------------------------------------------

function New-BottleLabel {
    param(
        [string]$FileName, [string]$BrandKo, [string]$BrandEn, [string]$Kind,
        [string]$Volume, $BaseTop, $BaseBottom, $AccentColor, $TextColor,
        [switch]$Waves, [switch]$Leaves, [switch]$Bubbles
    )
    $W = 1024; $H = 416
    $canvas = New-Canvas -Width $W -Height $H
    $bitmap = $canvas[0]; $g = $canvas[1]

    Fill-Vertical $g $W $H 0 $H $BaseTop $BaseBottom

    # Wide accent band: bottles sit under bright cooler lights, so the label
    # needs saturated colour to survive the exposure.
    $band = New-Object System.Drawing.SolidBrush($AccentColor)
    $g.FillRectangle($band, 0, [int]($H * 0.22), $W, [int]($H * 0.56))
    $band.Dispose()

    if ($Waves) {
        # Two stylised water waves plus a mountain ridge silhouette.
        $wavePen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(120, 255, 255, 255), 10)
        for ($k = 0; $k -lt 2; $k++) {
            $points = New-Object 'System.Collections.Generic.List[System.Drawing.PointF]'
            for ($x = 0; $x -le $W; $x += 32) {
                $y = ($H * (0.80 + $k * 0.07)) + [math]::Sin(($x / 90.0) + $k) * 16
                $points.Add((New-Object System.Drawing.PointF($x, $y)))
            }
            $g.DrawCurve($wavePen, $points.ToArray())
        }
        $wavePen.Dispose()
        $ridge = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(70, 255, 255, 255))
        $peaks = @(
            (New-Object System.Drawing.PointF(($W * 0.10), ($H * 0.72))),
            (New-Object System.Drawing.PointF(($W * 0.20), ($H * 0.40))),
            (New-Object System.Drawing.PointF(($W * 0.28), ($H * 0.58))),
            (New-Object System.Drawing.PointF(($W * 0.36), ($H * 0.34))),
            (New-Object System.Drawing.PointF(($W * 0.46), ($H * 0.72)))
        )
        $g.FillPolygon($ridge, $peaks)
        $ridge.Dispose()
    }
    if ($Leaves) {
        $leaf = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(90, 255, 255, 255))
        foreach ($cx in @(0.14, 0.78, 0.90)) {
            $g.FillEllipse($leaf, [single]($W * $cx), [single]($H * 0.36), 92, 44)
            $g.FillEllipse($leaf, [single]($W * $cx + 26), [single]($H * 0.52), 78, 36)
        }
        $leaf.Dispose()
    }
    if ($Bubbles) {
        $bubble = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(150, 255, 255, 255), 3)
        $seed = 7
        for ($i = 0; $i -lt 22; $i++) {
            $seed = ($seed * 1103515245 + 12345) -band 0x7FFFFFFF
            $r = 8 + ($seed % 22)
            $x = ($seed -shr 4) % $W
            $y = ($seed -shr 12) % $H
            $g.DrawEllipse($bubble, $x, $y, $r, $r)
        }
        $bubble.Dispose()
    }

    # Brand block, repeated so the wrap reads from any angle. Text inside the
    # accent band uses the light colour; everything on the pale base uses ink
    # drawn from the accent so it stays legible.
    $ink = $AccentColor
    Draw-Text $g $BrandKo 108 ([System.Drawing.FontStyle]::Bold) $TextColor ($W * 0.5) ($H * 0.46)
    Draw-Text $g $BrandEn 30 ([System.Drawing.FontStyle]::Regular) $TextColor ($W * 0.5) ($H * 0.62)
    Draw-Text $g $Kind 34 ([System.Drawing.FontStyle]::Bold) $ink ($W * 0.5) ($H * 0.17)
    Draw-Text $g $Volume 40 ([System.Drawing.FontStyle]::Bold) $ink ($W * 0.90) ($H * 0.17)
    Draw-Text $g $BrandKo 44 ([System.Drawing.FontStyle]::Bold) $ink ($W * 0.10) ($H * 0.86)

    Draw-NutritionStrip $g ($W * 0.60) ($H * 0.76) ($W * 0.16) ($H * 0.18) $ink
    Draw-Barcode $g ($W * 0.79) ($H * 0.75) ($W * 0.17) ($H * 0.20) 91
    Save-Canvas $bitmap $g $FileName
}

$deepBlue  = [System.Drawing.Color]::FromArgb(255, 12, 92, 168)
$paleBlue  = [System.Drawing.Color]::FromArgb(255, 176, 214, 240)
$midBlue   = [System.Drawing.Color]::FromArgb(255, 26, 120, 200)
$white     = [System.Drawing.Color]::White
$deepGreen = [System.Drawing.Color]::FromArgb(255, 16, 92, 52)
$paleGreen = [System.Drawing.Color]::FromArgb(255, 186, 224, 190)
$midGreen  = [System.Drawing.Color]::FromArgb(255, 30, 124, 66)
$amber     = [System.Drawing.Color]::FromArgb(255, 140, 84, 20)
$paleAmber = [System.Drawing.Color]::FromArgb(255, 236, 206, 148)
$midAmber  = [System.Drawing.Color]::FromArgb(255, 176, 116, 36)
$sodaGreen = [System.Drawing.Color]::FromArgb(255, 22, 150, 96)
$sojuBlue  = [System.Drawing.Color]::FromArgb(255, 20, 62, 130)

New-BottleLabel -FileName 'T_LabelWater_D.png' -BrandKo '새벽샘물' -BrandEn 'SAEBYEOK SPRING WATER' `
    -Kind '먹는샘물' -Volume '500mL' -BaseTop $paleBlue -BaseBottom $white `
    -AccentColor $deepBlue -TextColor $white -Waves
New-BottleLabel -FileName 'T_LabelGreenTea_D.png' -BrandKo '숲의녹차' -BrandEn 'FOREST GREEN TEA' `
    -Kind '녹차 · 무가당' -Volume '500mL' -BaseTop $paleGreen -BaseBottom $white `
    -AccentColor $deepGreen -TextColor $white -Leaves
New-BottleLabel -FileName 'T_LabelBarley_D.png' -BrandKo '구수한보리차' -BrandEn 'ROASTED BARLEY TEA' `
    -Kind '보리차' -Volume '500mL' -BaseTop $paleAmber -BaseBottom $white `
    -AccentColor $amber -TextColor $white
New-BottleLabel -FileName 'T_LabelSoda_D.png' -BrandKo '톡사이다' -BrandEn 'TOK CIDER' `
    -Kind '탄산음료' -Volume '500mL' -BaseTop $white -BaseBottom $paleGreen `
    -AccentColor $sodaGreen -TextColor $white -Bubbles
New-BottleLabel -FileName 'T_LabelSoju_D.png' -BrandKo '새벽소주' -BrandEn 'SAEBYEOK SOJU' `
    -Kind '희석식 소주 16.5%' -Volume '360mL' -BaseTop $white -BaseBottom $paleBlue `
    -AccentColor $sojuBlue -TextColor $white

# ---------------------------------------------------------------------------
# Snack bag fronts — 512 x 640
# ---------------------------------------------------------------------------

function New-SnackBag {
    param([string]$FileName, [string]$NameKo, [string]$Tagline, [string]$Weight,
        $BaseTop, $BaseBottom, $BandColor, $NameColor, [string]$BlobKind)
    $W = 512; $H = 640
    $canvas = New-Canvas -Width $W -Height $H
    $bitmap = $canvas[0]; $g = $canvas[1]

    Fill-Vertical $g $W $H 0 $H $BaseTop $BaseBottom

    # Top brand ribbon.
    $ribbon = New-Object System.Drawing.SolidBrush($BandColor)
    $g.FillRectangle($ribbon, 0, 0, $W, [int]($H * 0.14))
    $ribbon.Dispose()
    Draw-Text $g '새벽식품' 42 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::White) ($W * 0.5) ($H * 0.07)

    # Product photo stand-in: a soft plate of snack shapes.
    $plate = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(60, 0, 0, 0))
    $g.FillEllipse($plate, [single]($W * 0.12), [single]($H * 0.40), [single]($W * 0.76), [single]($H * 0.30))
    $plate.Dispose()
    $chip = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 236, 196, 112))
    $chipEdge = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 176, 130, 54), 3)
    $seed = 13
    for ($i = 0; $i -lt 9; $i++) {
        $seed = ($seed * 1103515245 + 12345) -band 0x7FFFFFFF
        $cx = ($W * 0.18) + (($seed -shr 5) % [int]($W * 0.64))
        $cy = ($H * 0.43) + (($seed -shr 13) % [int]($H * 0.22))
        switch ($BlobKind) {
            'ring' {
                $g.FillEllipse($chip, $cx, $cy, 74, 60)
                $hole = New-Object System.Drawing.SolidBrush($BaseBottom)
                $g.FillEllipse($hole, ($cx + 22), ($cy + 18), 30, 24)
                $hole.Dispose()
                $g.DrawEllipse($chipEdge, $cx, $cy, 74, 60)
            }
            'stick' {
                $g.FillRectangle($chip, $cx, $cy, 22, 96)
                $g.DrawRectangle($chipEdge, $cx, $cy, 22, 96)
            }
            default {
                $g.FillEllipse($chip, $cx, $cy, 84, 66)
                $g.DrawEllipse($chipEdge, $cx, $cy, 84, 66)
            }
        }
    }
    $chip.Dispose(); $chipEdge.Dispose()

    Draw-Text $g $NameKo 92 ([System.Drawing.FontStyle]::Bold) $NameColor ($W * 0.5) ($H * 0.26)
    Draw-Text $g $Tagline 30 ([System.Drawing.FontStyle]::Regular) $NameColor ($W * 0.5) ($H * 0.35)
    Draw-Text $g $Weight 34 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::White) ($W * 0.5) ($H * 0.78)
    Draw-Barcode $g ($W * 0.28) ($H * 0.84) ($W * 0.44) ($H * 0.11) 41
    Save-Canvas $bitmap $g $FileName
}

New-SnackBag -FileName 'T_SnackShrimp_D.png' -NameKo '새우칩' -Tagline '바삭한 새우의 맛' -Weight '순중량 68g' `
    -BaseTop ([System.Drawing.Color]::FromArgb(255, 246, 108, 60)) `
    -BaseBottom ([System.Drawing.Color]::FromArgb(255, 214, 46, 32)) `
    -BandColor ([System.Drawing.Color]::FromArgb(255, 158, 24, 18)) -NameColor $white -BlobKind 'chip'
New-SnackBag -FileName 'T_SnackPotato_D.png' -NameKo '감자칩' -Tagline '두께가 다른 감자' -Weight '순중량 60g' `
    -BaseTop ([System.Drawing.Color]::FromArgb(255, 250, 214, 78)) `
    -BaseBottom ([System.Drawing.Color]::FromArgb(255, 232, 168, 30)) `
    -BandColor ([System.Drawing.Color]::FromArgb(255, 152, 96, 12)) `
    -NameColor ([System.Drawing.Color]::FromArgb(255, 62, 38, 8)) -BlobKind 'chip'
New-SnackBag -FileName 'T_SnackSquid_D.png' -NameKo '오징어링' -Tagline '고소한 링 스낵' -Weight '순중량 55g' `
    -BaseTop ([System.Drawing.Color]::FromArgb(255, 86, 150, 226)) `
    -BaseBottom ([System.Drawing.Color]::FromArgb(255, 28, 82, 168)) `
    -BandColor ([System.Drawing.Color]::FromArgb(255, 16, 52, 118)) -NameColor $white -BlobKind 'ring'
New-SnackBag -FileName 'T_SnackCorn_D.png' -NameKo '콘스틱' -Tagline '달콤한 옥수수 스틱' -Weight '순중량 72g' `
    -BaseTop ([System.Drawing.Color]::FromArgb(255, 132, 206, 110)) `
    -BaseBottom ([System.Drawing.Color]::FromArgb(255, 42, 142, 62)) `
    -BandColor ([System.Drawing.Color]::FromArgb(255, 22, 92, 40)) -NameColor $white -BlobKind 'stick'

# ---------------------------------------------------------------------------
# Cup ramyeon wrapper — 1024 x 328 (wraps the tapered cup)
# ---------------------------------------------------------------------------

$W = 1024; $H = 328
$canvas = New-Canvas -Width $W -Height $H
$bitmap = $canvas[0]; $g = $canvas[1]
Fill-Vertical $g $W $H 0 $H ([System.Drawing.Color]::FromArgb(255, 214, 40, 30)) `
    ([System.Drawing.Color]::FromArgb(255, 150, 16, 14))
$stripe = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 250, 208, 60))
$g.FillRectangle($stripe, 0, [int]($H * 0.66), $W, [int]($H * 0.08))
$stripe.Dispose()
# Steaming bowl motif.
$bowl = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 252, 246, 232))
$g.FillEllipse($bowl, [single]($W * 0.06), [single]($H * 0.30), 150, 108)
$bowl.Dispose()
$noodle = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 240, 194, 84), 7)
for ($i = 0; $i -lt 5; $i++) {
    $g.DrawArc($noodle, [single]($W * 0.075 + $i * 6), [single]($H * 0.34 + $i * 8), 120, 60, 200, 140)
}
$noodle.Dispose()
$steam = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(160, 255, 255, 255), 6)
foreach ($dx in @(0.10, 0.13, 0.16)) {
    $pts = @(
        (New-Object System.Drawing.PointF(($W * $dx), ($H * 0.26))),
        (New-Object System.Drawing.PointF(($W * $dx + 18), ($H * 0.16))),
        (New-Object System.Drawing.PointF(($W * $dx - 10), ($H * 0.08)))
    )
    $g.DrawCurve($steam, $pts)
}
$steam.Dispose()
Draw-Text $g '왕라면' 132 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::White) ($W * 0.46) ($H * 0.38)
Draw-Text $g 'BIG BOWL RAMYEON · 매운맛' 34 ([System.Drawing.FontStyle]::Bold) `
    ([System.Drawing.Color]::FromArgb(255, 60, 20, 12)) ($W * 0.46) ($H * 0.70)
Draw-Text $g '용기면 110g' 30 ([System.Drawing.FontStyle]::Regular) ([System.Drawing.Color]::White) ($W * 0.46) ($H * 0.86)
Draw-Text $g '왕라면' 70 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::White) ($W * 0.86) ($H * 0.40)
Draw-Barcode $g ($W * 0.74) ($H * 0.62) ($W * 0.22) ($H * 0.30) 63
Save-Canvas $bitmap $g 'T_LabelRamyeon_D.png'

Write-Host 'Label textures complete.'
