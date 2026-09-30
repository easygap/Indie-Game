# -*- coding: utf-8 -*-
"""편의점 담뱃갑 앞면 그림. 전부 가상 브랜드이고 실제 상표·로고·전화번호는 없다.

담배 진열장 빌더(Scripts/blender/build_retail_refresh.py)가 image_quad로 담뱃갑
앞면에 붙여 굽는다. 게임 텍스처가 아니라 굽기 입력이라 해상도는 넉넉히 잡되
한 장의 아틀라스로 묶는다.

    Content/SourceArt/Labels/Store/CigarettePacks.png   4x2, 담뱃갑 앞면 여덟

    python Scripts/create_store_product_art.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Content", "SourceArt", "Labels", "Store")
FONT = "C:/Windows/Fonts/malgun.ttf"
FONT_BOLD = "C:/Windows/Fonts/malgunbd.ttf"


def font(size, bold=False):
    return ImageFont.truetype(FONT_BOLD if bold else FONT, size)


def text_center(draw, box, text, fnt, fill):
    x0, y0, x1, y1 = box
    w, h = draw.textbbox((0, 0), text, font=fnt)[2:]
    draw.text((x0 + (x1 - x0 - w) / 2, y0 + (y1 - y0 - h) / 2), text, font=fnt, fill=fill)


# --------------------------------------------------------------------------
# 담뱃갑: 한국 규격대로 앞면 위 절반이 경고 면이다. 그림 대신 검정 바탕 글자 경고.
# --------------------------------------------------------------------------

PACKS = [
    ("한라", "HALLA", "1.0 mg", (0.86, 0.87, 0.90), (30, 60, 120)),
    ("서라벌", "SEORABEOL", "", (0.16, 0.22, 0.40), (230, 200, 90)),
    ("동해", "DONGHAE", "3.0 mg", (0.10, 0.35, 0.55), (240, 240, 240)),
    ("미래", "MIRAE", "0.5 mg", (0.92, 0.92, 0.92), (40, 140, 90)),
    ("청솔", "CHEONGSOL", "", (0.15, 0.40, 0.25), (240, 235, 200)),
    ("백두", "BAEKDU", "6.0 mg", (0.70, 0.12, 0.12), (250, 240, 220)),
    ("새벽", "SAEBYEOK", "1.0 mg", (0.10, 0.10, 0.12), (200, 190, 150)),
    ("남산", "NAMSAN", "", (0.95, 0.80, 0.30), (60, 40, 20)),
]


def cigarette_packs():
    cw, ch = 300, 480
    atlas = Image.new("RGB", (cw * 4, ch * 2), (255, 255, 255))
    for index, (ko, en, tar, base, accent) in enumerate(PACKS):
        cell = Image.new("RGB", (cw, ch), tuple(int(c * 255) for c in base))
        d = ImageDraw.Draw(cell)
        # 위 절반: 경고 면. 검정 바탕에 흰 글자, 노란 테.
        d.rectangle((0, 0, cw, ch // 2), fill=(12, 12, 12))
        d.rectangle((8, 8, cw - 8, ch // 2 - 8), outline=(230, 200, 40), width=4)
        text_center(d, (0, 30, cw, 80), "경고", font(44, True), (255, 255, 255))
        for i, line in enumerate(("흡연은 폐암 등", "각종 질병의 원인!", "그래도 피우시겠습니까?")):
            text_center(d, (0, 95 + i * 40, cw, 130 + i * 40), line, font(24, True if i < 2 else False), (255, 255, 255))
        # 아래 절반: 브랜드.
        y = ch // 2
        d.rectangle((0, y, cw, y + 6), fill=accent)
        text_center(d, (0, y + 40, cw, y + 110), ko, font(64, True), accent)
        text_center(d, (0, y + 112, cw, y + 140), en, font(20, False), accent)
        if tar:
            d.rounded_rectangle((cw // 2 - 50, y + 158, cw // 2 + 50, y + 186), radius=8, outline=accent, width=2)
            text_center(d, (cw // 2 - 50, y + 158, cw // 2 + 50, y + 186), "타르 " + tar, font(16), accent)
        text_center(d, (0, ch - 44, cw, ch - 20), "20개비", font(16), accent)
        atlas.paste(cell, ((index % 4) * cw, (index // 4) * ch))
    atlas.save(os.path.join(OUT, "CigarettePacks.png"))


def main():
    os.makedirs(OUT, exist_ok=True)
    cigarette_packs()
    print("store product art ->", OUT)


if __name__ == "__main__":
    main()
