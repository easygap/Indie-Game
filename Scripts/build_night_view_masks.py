# -*- coding: utf-8 -*-
"""밤 원경의 불빛 마스크를 만든다.

창밖 풍경과 옥상 둘레의 원경은 사진 한 장이다. 그 안의 불 켜진 창을 덩어리로
찾아 창마다 따로 켜고 끌 수 있게, 원본과 같은 크기의 마스크를 만든다.

    R  불빛의 세기(0~1). 이 값만큼 사진의 불빛을 걷어 내고 다시 얹는다.
    G  창마다 다른 문턱값. 재질의 「깨어 있는 집」 값이 이보다 크면 켜진다.
    B  종류. 0은 보통 창, 0.5는 늘 켜진 불(가로등·항공 장애등·교회 십자가),
       1은 TV만 켜 둔 창(푸르게 흔들린다).

입주하는 저녁에는 대부분 켜져 있고, 04:30에는 몇 집만 남고, 새벽이 오면 출근하는
집부터 하나둘 켜진다. 시간은 게임이 정하고 이 스크립트는 창의 목록만 만든다.

    python Scripts/build_night_view_masks.py
"""

import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AI = os.path.join(ROOT, "Content", "SourceArt", "AI")
OUT = os.path.join(ROOT, "Content", "SourceArt", "NightView")

# 원경 넷은 한 장의 세로 띠로 묶는다. 순서가 곧 재질의 사분 번호다.
SKYLINE = (
    ("SkylineNorth_20260928.png", 0),
    ("SkylineEast_20260928.png", 1),
    ("SkylineSouth_20260928.png", 2),
    ("SkylineWest_20260928.png", 3),
)
VISTA = "ApartmentNightVista_20260916.png"
CELL = (1536, 1024)
# 그림을 보고 정한 「늘 켜진 줄」. 남쪽 큰길의 상가 1층은 편의점 유리가 칸마다
# 쪼개져 창처럼 잡히는데, 24시간 가게와 버스 정류장은 새벽에도 켜져 있다.
ALWAYS_BANDS = {
    "SkylineSouth_20260928.png": ((0, 560, 1536, 690),),
}


def light_mask(rgb, alpha):
    """밝고 따뜻하거나 형광등처럼 흰 픽셀. 불투명한 곳에서 상위 몇 퍼센트만."""
    lum = rgb @ np.array([0.2126, 0.7152, 0.0722])
    solid = alpha > 0.5
    if not solid.any():
        return np.zeros_like(lum)
    # 사진마다 노출이 달라 고정 문턱은 안 맞는다. 불투명 영역의 97번째 백분위를 기준으로 삼되
    # 너무 낮으면(창이 거의 없는 사진) 절대 하한을 쓴다.
    threshold = max(np.percentile(lum[solid], 97.0), 0.30)
    soft = np.clip((lum - threshold) / 0.18, 0.0, 1.0)
    soft *= solid
    return soft


def classify(rgb, mask, rng):
    """덩어리마다 문턱값과 종류를 정한다."""
    binary = mask > 0.08
    # 창틀 사이 1픽셀 틈으로 한 창이 둘로 쪼개지지 않게 살짝 부풀려 이름을 붙인다.
    labels, count = ndimage.label(ndimage.binary_dilation(binary, iterations=1))
    labels = labels * binary
    g = np.zeros(mask.shape, dtype=np.float32)
    b = np.zeros(mask.shape, dtype=np.float32)
    indices = np.arange(1, count + 1)
    sizes = ndimage.sum(np.ones_like(mask), labels, index=indices)
    red = rgb[..., 0] - np.maximum(rgb[..., 1], rgb[..., 2])
    redness = ndimage.mean(red, labels, index=indices)
    lum = rgb @ np.array([0.2126, 0.7152, 0.0722])
    peak = ndimage.maximum(lum, labels, index=indices)
    boxes = ndimage.find_objects(labels)
    windows = always = tv = 0
    for index in range(count):
        label = index + 1
        where = labels == label
        area = sizes[index]
        box = boxes[index]
        fill = area / max(1, (box[0].stop - box[0].start) * (box[1].stop - box[1].start)) if box else 1.0
        # 따뜻한 전구색 창도 빨강이 0.2쯤 앞선다. 십자가와 항공 장애등은 그보다 훨씬 붉다.
        is_red = redness[index] > 0.45
        # 점처럼 작고 하얗게 타 버린 불은 가로등이다.
        is_point = area < 40 and peak[index] > 0.93
        # 창은 네모라 테두리 상자를 거의 채운다. 가로등 빛이 나뭇잎에 번진 자국은
        # 들쭉날쭉해서 상자의 3분의 1도 못 채운다. 가로등과 같이 밤새 남긴다.
        is_splash = fill < 0.35 and area >= 12
        # 옆으로 긴 불빛은 창이 아니다. 편의점 앞, 버스 정류장, 젖은 길에 비친 가로등이
        # 이렇게 생겼고 새벽에도 꺼지지 않는다.
        height = (box[0].stop - box[0].start) if box else 1
        width = (box[1].stop - box[1].start) if box else 1
        is_street = width >= 3.0 * height and width >= 8
        if is_red or is_point or is_splash or is_street:
            g[where] = 0.0
            b[where] = 0.5
            always += 1
            continue
        if rng.random() < 0.05:
            g[where] = rng.uniform(0.02, 0.4)
            b[where] = 1.0
            tv += 1
            continue
        g[where] = rng.uniform(0.02, 1.0)
        windows += 1
    return g, b, windows, always, tv


def build(image, seed, bands=()):
    rgba = np.asarray(image.convert("RGBA")).astype(np.float32) / 255.0
    rgb, alpha = rgba[..., :3], rgba[..., 3]
    mask = light_mask(rgb, alpha)
    g, b, windows, always, tv = classify(rgb, mask, np.random.default_rng(seed))
    for x0, y0, x1, y1 in bands:
        band = np.zeros(mask.shape, dtype=bool)
        band[y0:y1, x0:x1] = True
        band &= mask > 0.08
        g[band] = 0.0
        b[band] = 0.5
    packed = np.stack([mask, g, b], axis=-1)
    return (packed * 255.0 + 0.5).astype(np.uint8), windows, always, tv


def clean_alpha(image):
    """생성기가 남긴 반투명 안개를 걷고 하늘을 완전히 비운다. 가장자리는 1픽셀만 부드럽게."""
    rgba = np.asarray(image.convert("RGBA")).astype(np.float32)
    alpha = rgba[..., 3] / 255.0
    alpha = np.clip((alpha - 0.35) / 0.3, 0.0, 1.0)
    rgba[..., 3] = alpha * 255.0
    # 완전히 투명한 곳의 색은 가장자리 색으로 채운다. 밉맵에서 검은 테두리가 번지지 않는다.
    solid = alpha > 0.5
    if solid.any() and not solid.all():
        idx = ndimage.distance_transform_edt(~solid, return_distances=False, return_indices=True)
        rgba[..., :3] = rgba[..., :3][idx[0], idx[1]]
    return Image.fromarray(rgba.clip(0, 255).astype(np.uint8), "RGBA")


def main():
    os.makedirs(OUT, exist_ok=True)
    strips_d, strips_m = [], []
    report = []
    for name, quadrant in SKYLINE:
        path = os.path.join(AI, name)
        if not os.path.isfile(path):
            sys.exit(f"원경 원본이 없습니다: {path}")
        image = clean_alpha(Image.open(path).resize(CELL, Image.LANCZOS))
        packed, windows, always, tv = build(image, 4040 + quadrant, ALWAYS_BANDS.get(name, ()))
        strips_d.append(np.asarray(image))
        strips_m.append(packed)
        report.append(f"{name}: 창 {windows}, 늘 켜진 불 {always}, TV {tv}")
    Image.fromarray(np.concatenate(strips_d, axis=0), "RGBA").save(os.path.join(OUT, "NightSkyline_D.png"))
    Image.fromarray(np.concatenate(strips_m, axis=0), "RGB").save(os.path.join(OUT, "NightSkyline_M.png"))

    vista = Image.open(os.path.join(AI, VISTA)).convert("RGBA")
    packed, windows, always, tv = build(vista, 4030)
    Image.fromarray(packed, "RGB").save(os.path.join(OUT, "ApartmentNightVista_M.png"))
    report.append(f"{VISTA}: 창 {windows}, 늘 켜진 불 {always}, TV {tv}")
    for line in report:
        print(line)
    print("NIGHT_VIEW_MASKS PASS")


if __name__ == "__main__":
    main()
