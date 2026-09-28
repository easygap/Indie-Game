# -*- coding: utf-8 -*-
"""5층 옥탑 외벽 샌드위치 패널 텍스처를 위아래로도 이어지게 다듬는다.

생성 원본은 좌우 이음은 맞지만 위아래를 붙이면 가운데에 옅은 띠가 생긴다.
원본을 리브 간격의 정수배만큼 세로로 민 사본을 위아래 가장자리에만 섞는다.
리브 위상이 맞아서 섞인 자리에 리브가 두 겹으로 보이지 않는다.

    python Scripts/build_annex_panel_texture.py
"""

import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(ROOT, "Content", "SourceArt", "AI", "AnnexSandwichPanel_20260928.png")
OUT_DIR = os.path.join(ROOT, "Content", "SourceArt", "AnnexPanel")
OUT = os.path.join(OUT_DIR, "AnnexSandwichPanel_D.png")


def rib_period(gray):
    """패널 가운데 띠의 밝기 줄에서 자기상관으로 가로 리브의 세로 간격(px)을 잰다."""
    width = gray.shape[1]
    # 이음매 기둥을 피해 두 패널의 가운데 부분만 본다.
    columns = np.r_[int(width * 0.12):int(width * 0.38), int(width * 0.62):int(width * 0.88)]
    profile = gray[:, columns].mean(axis=1)
    profile = profile - profile.mean()
    corr = np.correlate(profile, profile, mode="full")[len(profile) - 1:]
    corr /= corr[0]
    # 4px보다 긴 첫 봉우리. 리브는 20 mm라 1024px 높이에서 15px 안팎이다.
    for lag in range(4, 80):
        if corr[lag] > corr[lag - 1] and corr[lag] >= corr[lag + 1] and corr[lag] > 0.2:
            return lag
    sys.exit("리브 간격을 찾지 못했습니다.")


def main():
    if not os.path.isfile(SOURCE):
        sys.exit(f"패널 원본이 없습니다: {SOURCE}")
    image = np.asarray(Image.open(SOURCE).convert("RGB")).astype(np.float32)
    height = image.shape[0]
    period = rib_period(image.mean(axis=2))
    shift = int(round((height / 2) / period)) * period
    shifted = np.roll(image, shift, axis=0)
    y = (np.arange(height) + 0.5) / height
    # 가운데는 원본, 위아래 15%는 민 사본. 민 사본의 위아래 끝은 원본에서 서로 붙어 있던 줄이다.
    weight = np.clip((0.5 - np.abs(y - 0.5)) / 0.15, 0.0, 1.0)[:, None, None]
    result = image * weight + shifted * (1.0 - weight)
    os.makedirs(OUT_DIR, exist_ok=True)
    Image.fromarray(result.clip(0, 255).astype(np.uint8), "RGB").save(OUT)
    top_bottom = np.abs(result[0] - result[-1]).mean()
    left_right = np.abs(result[:, 0] - result[:, -1]).mean()
    print(f"리브 간격 {period}px, 세로 이동 {shift}px, 위아래 차이 {top_bottom:.2f}, 좌우 차이 {left_right:.2f}")
    print("ANNEX_PANEL_TEXTURE PASS")


if __name__ == "__main__":
    main()
