#!/usr/bin/env python3
"""Grade one square source and build the Windows multi-resolution game icon."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageEnhance, ImageFilter, ImageOps, ImageStat


ICON_SIZES = (16, 24, 32, 48, 64, 128, 256)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--png-output", required=True, type=Path)
    parser.add_argument("--ico-output", required=True, type=Path)
    return parser.parse_args()


def center_square(image: Image.Image) -> Image.Image:
    width, height = image.size
    edge = min(width, height)
    left = (width - edge) // 2
    top = (height - edge) // 2
    return image.crop((left, top, left + edge, top + edge))


def grade_icon(image: Image.Image) -> Image.Image:
    image = ImageOps.exif_transpose(image).convert("RGB")
    image = center_square(image).resize((1024, 1024), Image.Resampling.LANCZOS)

    # The source is intentionally near-black, but Windows renders small taskbar
    # icons against both dark and light shells. Lift only the metal midtones;
    # pure black in the hatch remains black and keeps the silhouette readable.
    gamma = 0.82
    gamma_lut = [round(((value / 255.0) ** gamma) * 255.0) for value in range(256)]
    image = image.point(gamma_lut * 3)
    image = ImageEnhance.Contrast(image).enhance(1.08)
    image = ImageEnhance.Color(image).enhance(0.90)
    image = image.filter(ImageFilter.UnsharpMask(radius=1.2, percent=115, threshold=3))
    return image


def main() -> int:
    args = parse_args()
    if not args.input.is_file():
        raise FileNotFoundError(args.input)

    args.png_output.parent.mkdir(parents=True, exist_ok=True)
    args.ico_output.parent.mkdir(parents=True, exist_ok=True)

    with Image.open(args.input) as source:
        icon = grade_icon(source)
    icon.save(args.png_output, format="PNG", optimize=True)
    icon.save(args.ico_output, format="ICO", sizes=[(size, size) for size in ICON_SIZES])

    thumbnail = icon.resize((32, 32), Image.Resampling.LANCZOS)
    luminance = ImageStat.Stat(thumbnail.convert("L"))
    payload = {
        "status": "PASS",
        "source": str(args.input.resolve()),
        "png": str(args.png_output.resolve()),
        "ico": str(args.ico_output.resolve()),
        "sizes": list(ICON_SIZES),
        "thumbnail_mean_luminance": round(luminance.mean[0], 2),
        "thumbnail_extrema": list(luminance.extrema[0]),
    }
    print("APPLICATION_ICON " + json.dumps(payload, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
