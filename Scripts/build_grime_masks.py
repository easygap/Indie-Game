"""gpt-image로 만든 때 사진을 공용부 재질이 곱할 수 있는 반복 텍스처로 다듬는다.

원본(Content/SourceArt/AI/Grime*_20260929.png)은 중간 회색(#808080) 위에 얼룩만 올린
사진이다. 재질(retail_surface_contract.author)은 이 값을 두 배 해서 벽·천장·바닥
색에 곱하므로, 회색은 그대로 두고 얼룩만 색과 함께 남는다. 여기서는 세 가지만 한다.

1. 생성 이미지 특유의 자글자글한 잔점을 얼룩 경계는 살린 채 걷어 낸다.
2. 얼룩 없는 바탕을 정확히 128로 맞춘다. 조금만 어긋나도 벽 전체 톤이 바뀐다.
3. 반 칸 민 사본과 섞어 네 변이 이어지게 한다. 월드 좌표로 펴서 반복하기 때문이다.

    python Scripts/build_grime_masks.py
"""

from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Content" / "SourceArt" / "AI"
OUT = ROOT / "Content" / "SourceArt" / "Grime"
SIZE = 1024
JOBS = (
    ("GrimeWall_20260929.png", "WallGrime_M.png"),
    ("GrimeCeiling_20260929.png", "CeilingStain_M.png"),
    ("GrimeFloor_20260929.png", "FloorGrime_M.png"),
)


def despeckle(image):
    """잔점은 얼룩보다 훨씬 작다. 3픽셀 중앙값으로 점을 먼저 지우고,
    가장자리 보존 평활로 남은 결을 고른다."""
    image = image.filter(ImageFilter.MedianFilter(3))
    pixels = np.asarray(image).astype(np.float32)
    blurred = np.asarray(image.filter(ImageFilter.GaussianBlur(1.6))).astype(np.float32)
    # 얼룩 경계처럼 밝기 차이가 큰 곳은 원본을, 평평한 곳은 흐린 쪽을 쓴다.
    difference = np.abs(pixels - blurred).mean(axis=2, keepdims=True)
    keep = np.clip((difference - 3.0) / 10.0, 0.0, 1.0)
    return pixels * keep + blurred * (1.0 - keep)


def neutralise(pixels):
    """얼룩 없는 바탕(밝기 상위 절반의 중앙값)을 정확히 128로 맞춘다."""
    luminance = pixels @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
    clean = luminance >= np.percentile(luminance, 50)
    for channel in range(3):
        background = np.median(pixels[..., channel][clean])
        pixels[..., channel] *= 128.0 / max(background, 1.0)
    return pixels


def seamless(pixels):
    """반 칸 민 사본을 가장자리로 갈수록 더 섞는다. 가장자리는 사본의 가운데라서
    이어 붙이면 경계가 사라진다."""
    height, width, _ = pixels.shape
    shifted = np.roll(pixels, (height // 2, width // 2), axis=(0, 1))
    y = np.abs(np.linspace(-1.0, 1.0, height))[:, None]
    x = np.abs(np.linspace(-1.0, 1.0, width))[None, :]
    edge = np.maximum(x, y)
    weight = 1.0 - np.clip((edge - 0.55) / 0.45, 0.0, 1.0)
    weight = (weight * weight * (3.0 - 2.0 * weight))[..., None]
    return pixels * weight + shifted * (1.0 - weight)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for source_name, out_name in JOBS:
        image = Image.open(SOURCE / source_name).convert("RGB").resize((SIZE, SIZE), Image.LANCZOS)
        pixels = seamless(neutralise(despeckle(image)))
        # 때는 어둡게만 만든다. 회색 바탕보다 밝게 나온 얼룩(천장 누수 자국)은
        # 가장 밝은 채널을 128에 맞춰 색만 남긴다. 흰 천장에 누런 물이 든 모양이 된다.
        brightest = np.maximum(pixels.max(axis=2, keepdims=True), 128.0)
        pixels = pixels * (128.0 / brightest)
        Image.fromarray(np.clip(pixels + 0.5, 0, 255).astype(np.uint8), "RGB").save(OUT / out_name)
        darkening = 1.0 - pixels.mean(axis=2) / 128.0
        print(out_name, "평균 어두워짐", round(float(darkening.mean()), 3), "최대", round(float(darkening.max()), 3))


if __name__ == "__main__":
    main()
