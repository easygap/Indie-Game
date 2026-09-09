# -*- coding: utf-8 -*-
"""편의점 상품 앞면 그림. 전부 가상 브랜드이고 실제 상표·로고·전화번호는 없다.

Blender 빌더(build_store_products.py)가 image_quad로 상자·포장 앞면에 붙여
굽는다. 게임 텍스처가 아니라 굽기 입력이라 해상도는 넉넉히 잡되 한 장의
아틀라스로 묶는다.

    Content/SourceArt/Labels/Store/CigarettePacks.png   4x2, 담뱃갑 앞면 여덟
    Content/SourceArt/Labels/Store/SnackBoxes.png       2x2, 과자 상자 앞면 넷
    Content/SourceArt/Labels/Store/TriangleKimbap.png   2x2, 삼각김밥 포장 넷
    Content/SourceArt/Labels/Store/RiceBowls.png        2x1, 즉석밥 뚜껑 둘
    Content/SourceArt/Labels/Store/MinorSign.png        청소년 판매 금지 띠

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


def barcode(draw, box, seed):
    x0, y0, x1, y1 = box
    x = x0
    import random
    rng = random.Random(seed)
    while x < x1:
        w = rng.choice((2, 2, 3, 4))
        if rng.random() < 0.55:
            draw.rectangle((x, y0, x + w, y1), fill=(20, 20, 20))
        x += w + rng.choice((1, 2))


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


# --------------------------------------------------------------------------
# 과자 상자 앞면
# --------------------------------------------------------------------------

BOXES = [
    ("새벽제과", "초코파이", "12개입 · 456g", (150, 30, 40), (250, 235, 220), (90, 50, 30)),
    ("숲의", "버터쿠키", "10개입 · 240g", (30, 70, 150), (255, 250, 230), (210, 160, 60)),
    ("톡", "크림웨하스", "8개입 · 180g", (240, 200, 40), (60, 40, 20), (255, 255, 255)),
    ("한라", "카스타드", "6개입 · 138g", (240, 140, 40), (255, 250, 235), (120, 60, 20)),
]


def snack_boxes():
    cw, ch = 480, 360
    atlas = Image.new("RGB", (cw * 2, ch * 2), (255, 255, 255))
    for index, (brand, name, weight, bg, fg, accent) in enumerate(BOXES):
        cell = Image.new("RGB", (cw, ch), bg)
        d = ImageDraw.Draw(cell)
        # 사선 띠와 제품 원형 그림자리(그림 없이 색면).
        d.polygon([(0, ch), (cw, ch - 120), (cw, ch), (0, ch)], fill=accent)
        d.ellipse((cw - 190, 40, cw - 30, 200), fill=accent)
        d.ellipse((cw - 175, 55, cw - 45, 185), fill=bg)
        text_center(d, (20, 24, 300, 60), brand, font(26, True), fg)
        d.text((20, 70), name, font=font(72, True), fill=fg)
        d.text((22, 170), weight, font=font(24), fill=fg)
        barcode(d, (22, ch - 80, 160, ch - 40), index + 11)
        text_center(d, (cw - 200, ch - 80, cw - 20, ch - 40), "1+1 행사", font(26, True), (255, 255, 255))
        atlas.paste(cell, ((index % 2) * cw, (index // 2) * ch))
    atlas.save(os.path.join(OUT, "SnackBoxes.png"))


# --------------------------------------------------------------------------
# 삼각김밥 포장: 검정 김 위에 색 띠와 이름표. 정삼각 앞면에 붙인다.
# --------------------------------------------------------------------------

KIMBAP = [
    ("참치마요", (60, 140, 80)),
    ("전주비빔", (200, 40, 40)),
    ("김치볶음", (220, 90, 30)),
    ("불고기", (110, 60, 30)),
]


def triangle_kimbap():
    cw = 400
    atlas = Image.new("RGB", (cw * 2, cw * 2), (20, 22, 20))
    for index, (name, color) in enumerate(KIMBAP):
        cell = Image.new("RGB", (cw, cw), (24, 26, 24))
        d = ImageDraw.Draw(cell)
        # 김 질감: 어두운 얼룩.
        import random
        rng = random.Random(index * 7)
        for _ in range(500):
            x, y = rng.randrange(cw), rng.randrange(cw)
            r = rng.randrange(3, 14)
            d.ellipse((x, y, x + r, y + r), fill=(rng.randrange(12, 40), rng.randrange(16, 44), rng.randrange(10, 30)))
        # 가운데 세로 뜯는 띠와 이름표.
        d.rectangle((cw // 2 - 22, 0, cw // 2 + 22, cw), fill=(235, 235, 235))
        d.rectangle((cw // 2 - 14, 0, cw // 2 + 14, cw), fill=color)
        d.rounded_rectangle((70, cw - 150, cw - 70, cw - 60), radius=14, fill=(250, 250, 250))
        text_center(d, (70, cw - 150, cw - 70, cw - 100), name, font(40, True), color)
        text_center(d, (70, cw - 104, cw - 70, cw - 68), "삼각김밥 · 110g", font(20), (60, 60, 60))
        atlas.paste(cell, ((index % 2) * cw, (index // 2) * cw))
    atlas.save(os.path.join(OUT, "TriangleKimbap.png"))


# --------------------------------------------------------------------------
# 즉석밥 뚜껑
# --------------------------------------------------------------------------

def rice_bowls():
    cw = 400
    atlas = Image.new("RGB", (cw * 2, cw), (255, 255, 255))
    for index, (name, color) in enumerate((("흰쌀밥", (200, 40, 50)), ("잡곡밥", (120, 80, 40)))):
        cell = Image.new("RGB", (cw, cw), (250, 248, 240))
        d = ImageDraw.Draw(cell)
        d.ellipse((30, 30, cw - 30, cw - 30), fill=(255, 255, 255), outline=color, width=10)
        text_center(d, (0, 110, cw, 170), "햇살밥", font(60, True), color)
        text_center(d, (0, 180, cw, 230), name, font(40, True), (40, 40, 40))
        text_center(d, (0, 240, cw, 275), "210g · 전자레인지 2분", font(20), (90, 90, 90))
        atlas.paste(cell, (index * cw, 0))
    atlas.save(os.path.join(OUT, "RiceBowls.png"))


def minor_sign():
    im = Image.new("RGB", (900, 120), (255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 900, 120), outline=(200, 30, 30), width=6)
    text_center(d, (0, 10, 900, 66), "청소년에게 담배·주류를 판매하지 않습니다", font(40, True), (200, 30, 30))
    text_center(d, (0, 68, 900, 108), "신분증을 확인합니다", font(26), (60, 60, 60))
    im.save(os.path.join(OUT, "MinorSign.png"))


def main():
    os.makedirs(OUT, exist_ok=True)
    cigarette_packs()
    snack_boxes()
    triangle_kimbap()
    rice_bowls()
    minor_sign()
    print("store product art ->", OUT)


if __name__ == "__main__":
    main()
