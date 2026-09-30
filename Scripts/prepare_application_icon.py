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
    # 원본에서 아이콘으로 쓸 구역(왼쪽, 위, 오른쪽, 아래를 0~1 비율로). 원본은
    # 생성한 그대로 두고, 작은 크기에서 읽히도록 여기서 당겨 자른다.
    parser.add_argument("--crop", type=float, nargs=4, metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"))
    return parser.parse_args()


def center_square(image: Image.Image) -> Image.Image:
    width, height = image.size
    edge = min(width, height)
    left = (width - edge) // 2
    top = (height - edge) // 2
    return image.crop((left, top, left + edge, top + edge))


def grade_icon(image: Image.Image, crop: list[float] | None = None) -> Image.Image:
    image = ImageOps.exif_transpose(image).convert("RGB")
    if crop:
        width, height = image.size
        left, top, right, bottom = crop
        image = image.crop((round(left * width), round(top * height),
                            round(right * width), round(bottom * height)))
    image = center_square(image).resize((1024, 1024), Image.Resampling.LANCZOS)

    # 새벽 빌라는 거의 검은색이지만 작업 표시줄은 밝은 테마와 어두운 테마를
    # 다 쓴다. 중간톤만 조금 올려 옥탑방 벽이 떠오르게 하고, 검은 벽돌과
    # 창 불빛의 대비는 그대로 둔다.
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
        icon = grade_icon(source, args.crop)
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
