"""게임 화면 위에 얹을 짧은 자막과 마지막 안내 화면을 만든다."""
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/Trailer/cards'
FONTS = ROOT / 'Source/IndieGame/UI/Fonts'
SIZE = (1920, 1080)


def text_line(draw, text, y, size, bold=False, fill=(235, 235, 229, 255)):
    font = ImageFont.truetype(str(FONTS / ('Pretendard-SemiBold.otf' if bold else 'Pretendard-Regular.otf')), size)
    box = draw.textbbox((0, 0), text, font=font)
    x = (SIZE[0] - (box[2] - box[0])) / 2
    if box[2] - box[0] > SIZE[0] - 160:
        raise ValueError(f'자막이 화면 폭을 넘습니다: {text}')
    draw.text((x, y - box[1]), text, font=font, fill=fill, stroke_width=1, stroke_fill=(12, 14, 16, 170))


def caption(name, text):
    layer = Image.new('RGBA', SIZE)
    # 하단 한 줄만 사용해 공간과 상호작용을 가리지 않는다.
    shade = Image.new('RGBA', SIZE)
    d = ImageDraw.Draw(shade)
    d.rectangle((100, 908, 1820, 1000), fill=(0, 0, 0, 125))
    layer = Image.alpha_composite(layer, shade.filter(ImageFilter.GaussianBlur(25)))
    text_line(ImageDraw.Draw(layer), text, 934, 45)
    layer.save(OUT / f'{name}.png')


def ending():
    layer = Image.new('RGBA', SIZE)
    d = ImageDraw.Draw(layer)
    text_line(d, 'Missing Floor', 372, 118, True)
    text_line(d, 'Windows 플레이 테스트 · 무료 다운로드', 572, 35)
    text_line(d, 'github.com/easygap/Missing-Floor', 646, 29, fill=(174, 180, 181, 255))
    layer.save(OUT / 'ending.png')


def social(path):
    # 홍보 카드에도 실제 게임 화면을 사용한다.
    art = Image.open(ROOT / 'Docs/Media/game-corridor-day.png').convert('RGBA')
    art = Image.alpha_composite(art, Image.new('RGBA', SIZE, (0, 0, 0, 145)))
    d = ImageDraw.Draw(art)
    text_line(d, 'Missing Floor', 355, 132, True)
    text_line(d, '오빠의 주소는 501호. 이 빌라는 4층까지다.', 570, 42)
    art.convert('RGB').resize((1280, 720), Image.Resampling.LANCZOS).save(path)


if __name__ == '__main__':
    OUT.mkdir(parents=True, exist_ok=True)
    caption('address', '오빠의 마지막 주소, 501호.')
    caption('fourfloors', '그런데 이 빌라는 4층까지다.')
    caption('knock', '그날 밤, 천장에서 노크 소리가 났다.')
    ending()
    if len(sys.argv) > 1:
        social(sys.argv[1])