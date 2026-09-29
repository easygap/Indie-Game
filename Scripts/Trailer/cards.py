"""트레일러 자막 카드와 GitHub 소셜 미리보기 이미지."""
import os
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FONTS = os.path.join(ROOT, "Source", "IndieGame", "UI", "Fonts")
SERIF = os.path.join(FONTS, "GowunBatang-Bold.ttf")
SANS = os.path.join(FONTS, "Pretendard-Regular.otf")
SANS_SEMI = os.path.join(FONTS, "Pretendard-SemiBold.otf")
OUT = os.path.join(ROOT, "Saved", "Trailer", "cards")
os.makedirs(OUT, exist_ok=True)


def font(path, size):
    return ImageFont.truetype(path, size)


def text_width(draw, text, fnt, tracking=0.0):
    if not tracking:
        return draw.textlength(text, font=fnt)
    return sum(draw.textlength(ch, font=fnt) for ch in text) + tracking * fnt.size * (len(text) - 1)


def draw_tracked(draw, xy, text, fnt, fill, tracking=0.0):
    x, y = xy
    if not tracking:
        draw.text((x, y), text, font=fnt, fill=fill)
        return
    for ch in text:
        draw.text((x, y), ch, font=fnt, fill=fill)
        x += draw.textlength(ch, font=fnt) + tracking * fnt.size


def ink(draw, text, fnt):
    """글자가 실제로 칠해지는 범위. (위 여백, 높이)"""
    x0, y0, x1, y1 = draw.textbbox((0, 0), text, font=fnt)
    return y0, y1 - y0


def glow_layer(size, painter, radius=18, strength=160):
    """글자 뒤에 옅은 어둠을 깔아 밝은 화면에서도 읽히게 한다."""
    mask = Image.new("L", size, 0)
    painter(ImageDraw.Draw(mask), 255)
    blurred = mask.filter(ImageFilter.GaussianBlur(radius))
    shadow = Image.new("RGBA", size, (0, 0, 0, 0))
    shadow.putalpha(blurred.point(lambda v: min(255, v * strength // 255)))
    return shadow


def card(name, korean, english, size=(1920, 1080), y_ratio=0.5, ko_size=58, en_size=30):
    """가운데 두 줄 자막. 한국어가 크고 영어가 아래에 작게 붙는다."""
    w, h = size
    ko_font = font(SERIF, ko_size)
    en_font = font(SANS, en_size)
    probe = ImageDraw.Draw(Image.new("RGBA", (10, 10)))
    ko_w = text_width(probe, korean, ko_font)
    en_w = text_width(probe, english, en_font, 0.02) if english else 0
    ko_top, ko_h = ink(probe, korean, ko_font)
    en_top, en_h = ink(probe, english, en_font) if english else (0, 0)
    gap = int(ko_size * 0.42)
    block_h = ko_h + (gap + en_h if english else 0)
    top = int(h * y_ratio - block_h / 2)
    ko_xy = ((w - ko_w) / 2, top - ko_top)
    en_xy = ((w - en_w) / 2, top + ko_h + gap - en_top)

    def paint(d, value):
        d.text(ko_xy, korean, font=ko_font, fill=value)
        if english:
            draw_tracked(d, en_xy, english, en_font, value, 0.02)

    layer = glow_layer(size, paint, radius=22, strength=150)
    d = ImageDraw.Draw(layer)
    d.text(ko_xy, korean, font=ko_font, fill=(236, 234, 228, 255))
    if english:
        draw_tracked(d, en_xy, english, en_font, (210, 212, 214, 205), 0.02)
    layer.save(os.path.join(OUT, name + ".png"))


def clock_card(name, size=(1920, 1080)):
    w, h = size
    f = font(SANS, 132)
    probe = ImageDraw.Draw(Image.new("RGBA", (10, 10)))
    text = "04:30"
    tw = text_width(probe, text, f, 0.04)
    xy = ((w - tw) / 2, h * 0.5 - 80)
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    draw_tracked(d, xy, text, f, (232, 232, 228, 235), 0.04)
    layer.save(os.path.join(OUT, name + ".png"))


def title_card(name, size=(1920, 1080), x_center=0.5):
    w, h = size
    ko = font(SERIF, 150)
    en = font(SANS_SEMI, 34)
    probe = ImageDraw.Draw(Image.new("RGBA", (10, 10)))
    ko_text, en_text = "없는 층", "THE MISSING FLOOR"
    ko_w = text_width(probe, ko_text, ko)
    en_w = text_width(probe, en_text, en, 0.42)
    ko_top, ko_h = ink(probe, ko_text, ko)
    en_top, en_h = ink(probe, en_text, en)
    gap = 44
    top = h * 0.5 - (ko_h + gap + en_h) / 2
    cx = w * x_center
    ko_xy = (cx - ko_w / 2, top - ko_top)
    en_xy = (cx - en_w / 2, top + ko_h + gap - en_top)

    def paint(d, value):
        d.text(ko_xy, ko_text, font=ko, fill=value)
        draw_tracked(d, en_xy, en_text, en, value, 0.42)

    layer = glow_layer(size, paint, radius=30, strength=170)
    d = ImageDraw.Draw(layer)
    d.text(ko_xy, ko_text, font=ko, fill=(240, 238, 232, 255))
    draw_tracked(d, en_xy, en_text, en, (214, 214, 212, 230), 0.42)
    layer.save(os.path.join(OUT, name + ".png"))


def end_card(name, size=(1920, 1080)):
    w, h = size
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    ko = font(SERIF, 96)
    en = font(SANS_SEMI, 24)
    line = font(SANS, 30)
    small = font(SANS, 24)
    rows = [
        ("없는 층", ko, (238, 236, 230, 255), 0.0),
        ("THE MISSING FLOOR", en, (205, 205, 203, 220), 0.42),
    ]
    y = h * 0.33
    for text, f, fill, tr in rows:
        tw = text_width(d, text, f, tr)
        top, height = ink(d, text, f)
        draw_tracked(d, ((w - tw) / 2, y - top), text, f, fill, tr)
        y += height + (34 if f is ko else 74)
    for text, f, fill in [
        ("Windows 무료 테스트 버전  ·  GitHub에서 받기", line, (226, 226, 222, 240)),
        ("Free test build for Windows  ·  Download on GitHub", small, (190, 192, 194, 210)),
    ]:
        tw = text_width(d, text, f)
        d.text(((w - tw) / 2, y), text, font=f, fill=fill)
        y += f.size + 18
    y += 34
    for text, f, fill in [
        ("헤드폰을 끼고 해 주세요.  Best with headphones.", small, (170, 172, 174, 200)),
    ]:
        tw = text_width(d, text, f)
        d.text(((w - tw) / 2, y), text, font=f, fill=fill)
    layer.save(os.path.join(OUT, name + ".png"))


def social_preview(path):
    """GitHub 저장소 카드. 1280x640, 1MB 아래."""
    art = Image.open(os.path.join(ROOT, "Content", "SourceArt", "T_TitleBackground_D.png")).convert("RGB")
    art = art.crop((0, 50, 1920, 1010)).resize((1280, 640), Image.LANCZOS)
    # 왼쪽을 조금 더 어둡게 눌러 글자 자리를 만든다.
    shade = Image.new("L", (1280, 640))
    sd = ImageDraw.Draw(shade)
    for x in range(1280):
        v = int(max(0, min(1, (700 - x) / 520)) * 150)
        sd.line([(x, 0), (x, 640)], fill=v)
    art = Image.composite(Image.new("RGB", (1280, 640), (8, 10, 14)), art, shade)
    d = ImageDraw.Draw(art)
    left = 78
    ko = font(SERIF, 118)
    en = font(SANS_SEMI, 25)
    tag = font(SANS, 29)
    tag_en = font(SANS, 21)
    meta = font(SANS, 21)
    y = 150
    top, height = ink(d, "없는 층", ko)
    d.text((left, y - top), "없는 층", font=ko, fill=(240, 238, 232))
    y += height + 30
    top, height = ink(d, "THE MISSING FLOOR", en)
    draw_tracked(d, (left + 4, y - top), "THE MISSING FLOOR", en, (205, 205, 203), 0.42)
    y += height + 34
    d.line([(left + 4, y), (left + 64, y)], fill=(150, 152, 154), width=2)
    y += 30
    top, height = ink(d, "새벽 네 시 반마다, 위층에서 누가 두드린다.", tag)
    d.text((left + 2, y - top), "새벽 네 시 반마다, 위층에서 누가 두드린다.", font=tag, fill=(222, 222, 218))
    y += height + 18
    top, height = ink(d, "Every night at 4:30, someone knocks from upstairs.", tag_en)
    d.text((left + 3, y - top), "Every night at 4:30, someone knocks from upstairs.", font=tag_en, fill=(176, 178, 180))
    d.text((left + 3, 548), "1인칭 공포  ·  Windows  ·  무료 테스트 버전  ·  5개 언어", font=meta, fill=(160, 162, 166))
    art.save(path, quality=88, optimize=True, progressive=True)
    return os.path.getsize(path)


if __name__ == "__main__":
    card("c01_brother", "연락이 끊긴 오빠를 찾아 이사 왔다.", "I moved in to find my missing brother.", y_ratio=0.82)
    card("c02_parcel", "되돌아온 택배에 적힌 오빠 주소는 501호.", "A returned package listed his address as Unit 501.", y_ratio=0.80)
    card("c03_fourfloors", "이 빌라는 4층까지밖에 없다.", "This building only has four floors.", y_ratio=0.80)
    card("c04_knock", "새벽 네 시 반, 위층에서 누가 두드린다.", "At 4:30 a.m., someone knocks from upstairs.")
    card("c05_neighbors", "옆집도 그 소리를 듣고 있었다.", "The neighbors hear it too.")
    card("c06_quiet", "소리 내지 마.", "Don't make a sound.")
    card("c07_loop", "잡히면, 다시 새벽 네 시 반.", "Get caught, and it's 4:30 all over again.")
    clock_card("c00_clock")
    title_card("c08_title", x_center=0.26)
    end_card("c09_end")
    if len(sys.argv) > 1:
        print("social", social_preview(sys.argv[1]))
