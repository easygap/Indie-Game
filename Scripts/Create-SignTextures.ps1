# Renders the Korean signage/poster/label bitmaps used by the prologue into
# Content/SourceArt as PNGs. generate_surface_textures.py imports everything
# in that folder afterwards. Pure System.Drawing + system fonts; no external
# assets are involved.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$projectRoot = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $projectRoot 'Content\SourceArt'
New-Item -ItemType Directory -Force $outDir | Out-Null

function New-SignBitmap {
    param(
        [int]$Width,
        [int]$Height,
        [System.Drawing.Color]$Background,
        [scriptblock]$Draw,
        [string]$FileName
    )
    $bitmap = New-Object System.Drawing.Bitmap($Width, $Height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = 'AntiAlias'
    $graphics.TextRenderingHint = 'AntiAliasGridFit'
    $graphics.Clear($Background)
    & $Draw $graphics $Width $Height
    $graphics.Dispose()
    $path = Join-Path $outDir $FileName
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
    Write-Host "Wrote $path"
}

function Draw-CenteredText {
    param($Graphics, $Text, $FontFamily, [single]$Size, [System.Drawing.FontStyle]$Style,
        [System.Drawing.Color]$Color, [single]$CenterX, [single]$CenterY)
    $font = New-Object System.Drawing.Font($FontFamily, $Size, $Style, [System.Drawing.GraphicsUnit]::Pixel)
    $brush = New-Object System.Drawing.SolidBrush($Color)
    $format = New-Object System.Drawing.StringFormat
    $format.Alignment = 'Center'
    $format.LineAlignment = 'Center'
    $Graphics.DrawString($Text, $font, $brush, $CenterX, $CenterY, $format)
    $format.Dispose(); $brush.Dispose(); $font.Dispose()
}

$malgun = 'Malgun Gothic'
$mint = [System.Drawing.Color]::FromArgb(255, 22, 168, 128)
$white = [System.Drawing.Color]::White
$nearWhite = [System.Drawing.Color]::FromArgb(255, 236, 240, 238)
$dark = [System.Drawing.Color]::FromArgb(255, 24, 26, 28)
$red = [System.Drawing.Color]::FromArgb(255, 198, 40, 32)

# --- Store fascia sign: mint band, white lettering -------------------------
New-SignBitmap -Width 1024 -Height 256 -Background $mint -FileName 'T_SignMain_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '새벽편의점' $malgun 128 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.42) ($h * 0.5)
    $pen = New-Object System.Drawing.Pen($white, 5)
    $g.DrawLine($pen, [single]($w * 0.755), [single]($h * 0.2), [single]($w * 0.755), [single]($h * 0.8))
    $pen.Dispose()
    Draw-CenteredText $g '24' $malgun 118 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.85) ($h * 0.42)
    Draw-CenteredText $g 'HOURS' $malgun 34 ([System.Drawing.FontStyle]::Regular) $white ($w * 0.85) ($h * 0.80)
}

# --- Vertical blade sign ---------------------------------------------------
New-SignBitmap -Width 192 -Height 640 -Background $white -FileName 'T_SignBlade_D.png' -Draw {
    param($g, $w, $h)
    $chars = @('편', '의', '점')
    for ($i = 0; $i -lt $chars.Count; $i++) {
        Draw-CenteredText $g $chars[$i] $malgun 128 ([System.Drawing.FontStyle]::Bold) $mint ($w * 0.5) ($h * (0.18 + 0.3 * $i))
    }
}

# --- Sale poster (storefront glass) ---------------------------------------
New-SignBitmap -Width 512 -Height 704 -Background $nearWhite -FileName 'T_PosterSale_D.png' -Draw {
    param($g, $w, $h)
    $band = New-Object System.Drawing.SolidBrush($red)
    $g.FillRectangle($band, 0, 0, $w, [single]($h * 0.30))
    $band.Dispose()
    Draw-CenteredText $g '1+1' $malgun 150 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.5) ($h * 0.15)
    Draw-CenteredText $g '생수 행사' $malgun 84 ([System.Drawing.FontStyle]::Bold) $dark ($w * 0.5) ($h * 0.45)
    Draw-CenteredText $g '500mL 950원' $malgun 52 ([System.Drawing.FontStyle]::Regular) $red ($w * 0.5) ($h * 0.62)
    $gray = [System.Drawing.Color]::FromArgb(255, 150, 152, 150)
    for ($i = 0; $i -lt 3; $i++) {
        $pen = New-Object System.Drawing.Pen($gray, 6)
        $y = [single]($h * (0.76 + $i * 0.07))
        $g.DrawLine($pen, [single]($w * 0.16), $y, [single]($w * 0.84), $y)
        $pen.Dispose()
    }
}

# --- Ramyeon shelf poster --------------------------------------------------
New-SignBitmap -Width 512 -Height 352 -Background ([System.Drawing.Color]::FromArgb(255, 240, 200, 60)) -FileName 'T_PosterRamyeon_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '라면 특가' $malgun 96 ([System.Drawing.FontStyle]::Bold) $dark ($w * 0.5) ($h * 0.34)
    Draw-CenteredText $g '컵라면 全품목 할인' $malgun 44 ([System.Drawing.FontStyle]::Regular) $dark ($w * 0.5) ($h * 0.68)
}

# --- Fridge note (the foreshadowing) --------------------------------------
New-SignBitmap -Width 256 -Height 256 -Background ([System.Drawing.Color]::FromArgb(255, 245, 228, 130)) -FileName 'T_NoteFridge_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '물 사올 것' $malgun 52 ([System.Drawing.FontStyle]::Italic) $dark ($w * 0.5) ($h * 0.36)
    Draw-CenteredText $g '- 나' $malgun 34 ([System.Drawing.FontStyle]::Italic) $dark ($w * 0.68) ($h * 0.66)
}

# --- Bathroom door plate ---------------------------------------------------
New-SignBitmap -Width 256 -Height 128 -Background ([System.Drawing.Color]::FromArgb(255, 88, 92, 96)) -FileName 'T_SignToilet_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '화장실' $malgun 62 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.5) ($h * 0.5)
}

# --- Sliding door sticker --------------------------------------------------
New-SignBitmap -Width 256 -Height 128 -Background $white -FileName 'T_SignAutoDoor_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '자동문' $malgun 56 ([System.Drawing.FontStyle]::Bold) $mint ($w * 0.5) ($h * 0.36)
    Draw-CenteredText $g '24시간 영업' $malgun 30 ([System.Drawing.FontStyle]::Regular) $dark ($w * 0.5) ($h * 0.74)
}

# --- Cooler price strip ----------------------------------------------------
New-SignBitmap -Width 1024 -Height 64 -Background $white -FileName 'T_PriceStrip_D.png' -Draw {
    param($g, $w, $h)
    for ($i = 0; $i -lt 4; $i++) {
        $x = [single]($w * (0.125 + 0.25 * $i))
        Draw-CenteredText $g '생수 950' $malgun 34 ([System.Drawing.FontStyle]::Regular) $dark $x ($h * 0.5)
        $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 190, 192, 190), 3)
        $g.DrawLine($pen, [single]($w * 0.25 * ($i + 1)), 8, [single]($w * 0.25 * ($i + 1)), [single]($h - 8))
        $pen.Dispose()
    }
}

# --- Building name plate over the common entrance --------------------------
New-SignBitmap -Width 320 -Height 96 -Background ([System.Drawing.Color]::FromArgb(255, 30, 36, 48)) -FileName 'T_SignVilla_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '달빛빌라' $malgun 52 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.42) ($h * 0.5)
    Draw-CenteredText $g '37-4' $malgun 30 ([System.Drawing.FontStyle]::Regular) ([System.Drawing.Color]::FromArgb(255, 150, 200, 190)) ($w * 0.85) ($h * 0.5)
}

# --- Unit number plates (4th floor) ----------------------------------------
foreach ($unit in @('401', '403', '404')) {
    $file = "T_Plate$unit`_D.png"
    New-SignBitmap -Width 128 -Height 64 -Background $nearWhite -FileName $file -Draw {
        param($g, $w, $h)
        Draw-CenteredText $g "$unit호" $malgun 36 ([System.Drawing.FontStyle]::Bold) $dark ($w * 0.5) ($h * 0.5)
    }
}

# --- Alarm clock LED face ---------------------------------------------------
New-SignBitmap -Width 256 -Height 128 -Background ([System.Drawing.Color]::FromArgb(255, 8, 6, 6)) -FileName 'T_ClockFace_D.png' -Draw {
    param($g, $w, $h)
    $led = [System.Drawing.Color]::FromArgb(255, 255, 40, 24)
    $ledDim = [System.Drawing.Color]::FromArgb(255, 120, 18, 12)
    Draw-CenteredText $g '4:44' 'Consolas' 92 ([System.Drawing.FontStyle]::Bold) $led ($w * 0.56) ($h * 0.5)
    Draw-CenteredText $g 'AM' $malgun 26 ([System.Drawing.FontStyle]::Bold) $ledDim ($w * 0.12) ($h * 0.3)
    Draw-CenteredText $g 'ALARM' $malgun 16 ([System.Drawing.FontStyle]::Regular) $ledDim ($w * 0.12) ($h * 0.72)
}

# --- Elevator floor indicator ----------------------------------------------
New-SignBitmap -Width 128 -Height 96 -Background ([System.Drawing.Color]::FromArgb(255, 16, 16, 18)) -FileName 'T_ElevatorPanel_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '4' $malgun 52 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::FromArgb(255, 255, 120, 40)) ($w * 0.5) ($h * 0.34)
    Draw-CenteredText $g '▼' $malgun 28 ([System.Drawing.FontStyle]::Regular) ([System.Drawing.Color]::FromArgb(255, 200, 90, 30)) ($w * 0.5) ($h * 0.76)
}

# --- Alley flyer wall ------------------------------------------------------
New-SignBitmap -Width 384 -Height 512 -Background ([System.Drawing.Color]::FromArgb(255, 214, 214, 208)) -FileName 'T_PosterFlyer_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '원룸 월세' $malgun 64 ([System.Drawing.FontStyle]::Bold) $dark ($w * 0.5) ($h * 0.18)
    Draw-CenteredText $g '보증금 상담' $malgun 44 ([System.Drawing.FontStyle]::Regular) $dark ($w * 0.5) ($h * 0.36)
    $gray = [System.Drawing.Color]::FromArgb(255, 120, 122, 120)
    for ($i = 0; $i -lt 4; $i++) {
        $pen = New-Object System.Drawing.Pen($gray, 5)
        $y = [single]($h * (0.52 + $i * 0.08))
        $g.DrawLine($pen, [single]($w * 0.14), $y, [single]($w * 0.86), $y)
        $pen.Dispose()
    }
    # Tear-off tabs along the bottom edge.
    for ($i = 0; $i -lt 6; $i++) {
        $pen = New-Object System.Drawing.Pen($gray, 3)
        $x = [single]($w * (0.08 + $i * 0.15))
        $g.DrawLine($pen, $x, [single]($h * 0.88), $x, [single]($h * 0.99))
        $pen.Dispose()
    }
}

# --- Closed neighborhood shop signs (dark before dawn) ----------------------
New-SignBitmap -Width 512 -Height 128 -Background ([System.Drawing.Color]::FromArgb(255, 24, 60, 140)) -FileName 'T_SignLaundry_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '하나세탁소' $malgun 72 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.5) ($h * 0.5)
}
New-SignBitmap -Width 512 -Height 128 -Background ([System.Drawing.Color]::FromArgb(255, 120, 40, 120)) -FileName 'T_SignHair_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '은하미용실' $malgun 70 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.5) ($h * 0.5)
}
New-SignBitmap -Width 512 -Height 128 -Background ([System.Drawing.Color]::FromArgb(255, 170, 30, 24)) -FileName 'T_SignHof_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '왕발통닭 · 호프' $malgun 62 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::FromArgb(255, 255, 230, 120)) ($w * 0.5) ($h * 0.5)
}
New-SignBitmap -Width 512 -Height 128 -Background ([System.Drawing.Color]::FromArgb(255, 20, 110, 60)) -FileName 'T_SignSuper_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '골목슈퍼' $malgun 74 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.5) ($h * 0.5)
}

New-SignBitmap -Width 512 -Height 128 -Background ([System.Drawing.Color]::FromArgb(255, 18, 40, 110)) -FileName 'T_SignPC_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '스타PC방' $malgun 68 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::FromArgb(255, 120, 220, 255)) ($w * 0.42) ($h * 0.5)
    Draw-CenteredText $g '24' $malgun 54 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.88) ($h * 0.5)
}
New-SignBitmap -Width 512 -Height 128 -Background ([System.Drawing.Color]::FromArgb(255, 150, 24, 90)) -FileName 'T_SignKaraoke_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '달빛노래방' $malgun 66 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::FromArgb(255, 255, 210, 240)) ($w * 0.5) ($h * 0.5)
}

# --- Rental banner strung on the facade -------------------------------------
New-SignBitmap -Width 1024 -Height 128 -Background ([System.Drawing.Color]::FromArgb(255, 250, 220, 40)) -FileName 'T_Banner_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '원룸 · 투룸 월세 문의  010-2345-6789' $malgun 58 ([System.Drawing.FontStyle]::Bold) $red ($w * 0.5) ($h * 0.5)
}

# --- Lobby notice, door ad stickers, calendar, fire box, tobacco notice -----
New-SignBitmap -Width 256 -Height 352 -Background $nearWhite -FileName 'T_NoticeA4_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '공 지' $malgun 44 ([System.Drawing.FontStyle]::Bold) $dark ($w * 0.5) ($h * 0.12)
    Draw-CenteredText $g '7월 관리비 납부 안내' $malgun 26 ([System.Drawing.FontStyle]::Regular) $dark ($w * 0.5) ($h * 0.28)
    $gray = [System.Drawing.Color]::FromArgb(255, 130, 132, 130)
    for ($i = 0; $i -lt 6; $i++) {
        $pen = New-Object System.Drawing.Pen($gray, 4)
        $y = [single]($h * (0.42 + $i * 0.08))
        $g.DrawLine($pen, [single]($w * 0.12), $y, [single]($w * 0.88), $y)
        $pen.Dispose()
    }
    Draw-CenteredText $g '달빛빌라 관리사무소' $malgun 20 ([System.Drawing.FontStyle]::Regular) $dark ($w * 0.5) ($h * 0.93)
}
New-SignBitmap -Width 256 -Height 256 -Background ([System.Drawing.Color]::FromArgb(255, 235, 235, 230)) -FileName 'T_DoorAd_D.png' -Draw {
    param($g, $w, $h)
    $colors = @([System.Drawing.Color]::FromArgb(255,210,60,40), [System.Drawing.Color]::FromArgb(255,40,90,190), [System.Drawing.Color]::FromArgb(255,240,150,20))
    $labels = @('열쇠 24시', '도배 장판', '치킨 배달')
    for ($i = 0; $i -lt 3; $i++) {
        $brush = New-Object System.Drawing.SolidBrush($colors[$i])
        $g.FillRectangle($brush, [single]($w*0.08), [single]($h*(0.06+$i*0.32)), [single]($w*0.84), [single]($h*0.24))
        $brush.Dispose()
        Draw-CenteredText $g $labels[$i] $malgun 34 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.5) ($h * (0.18 + $i * 0.32))
    }
}
New-SignBitmap -Width 256 -Height 320 -Background $nearWhite -FileName 'T_Calendar_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '2026  7월' $malgun 44 ([System.Drawing.FontStyle]::Bold) $dark ($w * 0.5) ($h * 0.12)
    $gray = [System.Drawing.Color]::FromArgb(255, 150, 152, 150)
    for ($r = 0; $r -lt 5; $r++) {
        for ($c = 0; $c -lt 7; $c++) {
            $color = if ($c -eq 0) { $red } else { $gray }
            $pen = New-Object System.Drawing.Pen($color, 2)
            $x = [single]($w * (0.08 + $c * 0.125)); $y = [single]($h * (0.3 + $r * 0.13))
            $g.DrawRectangle($pen, $x, $y, [single]($w*0.1), [single]($h*0.09))
            $pen.Dispose()
        }
    }
}
New-SignBitmap -Width 256 -Height 320 -Background ([System.Drawing.Color]::FromArgb(255, 180, 30, 24)) -FileName 'T_FireBox_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '소 화 전' $malgun 58 ([System.Drawing.FontStyle]::Bold) $white ($w * 0.5) ($h * 0.5)
}
New-SignBitmap -Width 512 -Height 64 -Background $nearWhite -FileName 'T_TobaccoNotice_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '청소년에게 담배를 판매하지 않습니다' $malgun 28 ([System.Drawing.FontStyle]::Bold) $dark ($w * 0.5) ($h * 0.5)
}

# --- Villa fittings, from the reference photos -----------------------------
# Digital door lock: matte black slab, 3x4 keypad, a hairline touch strip.
New-SignBitmap -Width 192 -Height 512 -Background ([System.Drawing.Color]::FromArgb(255, 20, 21, 23)) -FileName 'T_DoorLock_D.png' -Draw {
    param($g, $w, $h)
    $keyBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 40, 42, 46))
    $labels = @('1','2','3','4','5','6','7','8','9','*','0','#')
    for ($row = 0; $row -lt 4; $row++) {
        for ($col = 0; $col -lt 3; $col++) {
            $cx = $w * (0.24 + 0.26 * $col)
            $cy = $h * (0.34 + 0.145 * $row)
            $g.FillEllipse($keyBrush, ($cx - 20), ($cy - 20), 40, 40)
            Draw-CenteredText $g $labels[$row * 3 + $col] $malgun 24 ([System.Drawing.FontStyle]::Regular) ([System.Drawing.Color]::FromArgb(255, 200, 205, 212)) $cx $cy
        }
    }
    $strip = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 96, 100, 106))
    $g.FillRectangle($strip, ($w * 0.2), ($h * 0.16), ($w * 0.6), 6)
    Draw-CenteredText $g 'OPEN' $malgun 20 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::FromArgb(255, 120, 190, 150)) ($w * 0.5) ($h * 0.92)
    $strip.Dispose(); $keyBrush.Dispose()
}

# Distribution board door: the grey steel panel every Korean landing has.
New-SignBitmap -Width 256 -Height 384 -Background ([System.Drawing.Color]::FromArgb(255, 92, 96, 98)) -FileName 'T_MeterBox_D.png' -Draw {
    param($g, $w, $h)
    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 60, 63, 66), 3)
    $g.DrawRectangle($pen, 10, 10, ($w - 20), ($h - 20))
    Draw-CenteredText $g '분전반' $malgun 34 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::FromArgb(255, 232, 234, 236)) ($w * 0.5) ($h * 0.16)
    Draw-CenteredText $g '취급주의' $malgun 22 ([System.Drawing.FontStyle]::Regular) ([System.Drawing.Color]::FromArgb(255, 224, 196, 60)) ($w * 0.5) ($h * 0.86)
    $pen.Dispose()
}

# Video intercom handset plate on the corridor wall and inside the unit.
New-SignBitmap -Width 256 -Height 320 -Background ([System.Drawing.Color]::FromArgb(255, 232, 234, 232)) -FileName 'T_Intercom_D.png' -Draw {
    param($g, $w, $h)
    $screen = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 26, 32, 36))
    $g.FillRectangle($screen, ($w * 0.12), ($h * 0.09), ($w * 0.76), ($h * 0.46))
    $btn = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 198, 202, 204))
    for ($i = 0; $i -lt 3; $i++) {
        $g.FillEllipse($btn, ($w * (0.2 + 0.26 * $i)), ($h * 0.66), 34, 34)
    }
    Draw-CenteredText $g '통화' $malgun 20 ([System.Drawing.FontStyle]::Regular) $dark ($w * 0.5) ($h * 0.9)
    $screen.Dispose(); $btn.Dispose()
}

# Car operating panel: floor column plus the red segment readout above it.
New-SignBitmap -Width 192 -Height 768 -Background ([System.Drawing.Color]::FromArgb(255, 176, 180, 184)) -FileName 'T_LiftCOP_D.png' -Draw {
    param($g, $w, $h)
    $display = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 18, 18, 20))
    $g.FillRectangle($display, ($w * 0.14), ($h * 0.03), ($w * 0.72), ($h * 0.1))
    Draw-CenteredText $g '4' $malgun 62 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::FromArgb(255, 255, 60, 40)) ($w * 0.5) ($h * 0.08)
    $ring = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 122, 126, 130), 3)
    $face = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 214, 217, 220))
    $floors = @('5','4','3','2','1','B1')
    for ($i = 0; $i -lt $floors.Length; $i++) {
        $cy = $h * (0.22 + 0.105 * $i)
        $g.FillEllipse($face, ($w * 0.5 - 28), ($cy - 28), 56, 56)
        $g.DrawEllipse($ring, ($w * 0.5 - 28), ($cy - 28), 56, 56)
        Draw-CenteredText $g $floors[$i] $malgun 26 ([System.Drawing.FontStyle]::Bold) $dark ($w * 0.5) $cy
    }
    # Door open/close pair and the alarm bell at the bottom.
    for ($i = 0; $i -lt 3; $i++) {
        $cy = $h * (0.86 + 0.045 * $i)
        $g.FillEllipse($face, ($w * 0.5 - 22), ($cy - 22), 44, 44)
        $g.DrawEllipse($ring, ($w * 0.5 - 22), ($cy - 22), 44, 44)
    }
    Draw-CenteredText $g '열림' $malgun 16 ([System.Drawing.FontStyle]::Regular) $dark ($w * 0.5) ($h * 0.86)
    Draw-CenteredText $g '닫힘' $malgun 16 ([System.Drawing.FontStyle]::Regular) $dark ($w * 0.5) ($h * 0.905)
    Draw-CenteredText $g '비상' $malgun 16 ([System.Drawing.FontStyle]::Regular) $red ($w * 0.5) ($h * 0.95)
    $ring.Dispose(); $face.Dispose(); $display.Dispose()
}

# Hall lantern above the landing doors: red digit on black.
New-SignBitmap -Width 256 -Height 160 -Background ([System.Drawing.Color]::FromArgb(255, 12, 12, 14)) -FileName 'T_LiftHall_D.png' -Draw {
    param($g, $w, $h)
    Draw-CenteredText $g '▼' $malgun 44 ([System.Drawing.FontStyle]::Regular) ([System.Drawing.Color]::FromArgb(255, 255, 70, 44)) ($w * 0.26) ($h * 0.5)
    Draw-CenteredText $g '4' $malgun 96 ([System.Drawing.FontStyle]::Bold) ([System.Drawing.Color]::FromArgb(255, 255, 70, 44)) ($w * 0.64) ($h * 0.5)
}

# Wall switch plate: two rockers, one with the pilot dot lit.
New-SignBitmap -Width 256 -Height 256 -Background ([System.Drawing.Color]::FromArgb(255, 238, 238, 234)) -FileName 'T_SwitchPlate_D.png' -Draw {
    param($g, $w, $h)
    $rocker = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 248, 248, 245))
    $edge = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 196, 196, 190), 3)
    for ($i = 0; $i -lt 2; $i++) {
        $x = $w * (0.1 + 0.44 * $i)
        $g.FillRectangle($rocker, $x, ($h * 0.16), ($w * 0.36), ($h * 0.68))
        $g.DrawRectangle($edge, $x, ($h * 0.16), ($w * 0.36), ($h * 0.68))
    }
    $pilot = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 250, 150, 60))
    $g.FillEllipse($pilot, ($w * 0.22), ($h * 0.72), 16, 16)
    $rocker.Dispose(); $edge.Dispose(); $pilot.Dispose()
}

Write-Host 'Sign textures complete.'
