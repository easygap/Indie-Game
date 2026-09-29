"""예고편 위에 얹을 글자. 게임 화면의 소리 자막·속말 상자와 같은 모양으로 그린다.

설명 자막을 따로 쓰지 않는다. 화면에 나오는 글은 게임에서 실제로 뜨는 자막 두 줄과
제목뿐이다. 한국어판과 영어판은 같은 촬영분에 글자만 바꿔 얹는다.

    python Scripts/Trailer/cards.py            # 두 언어의 글자 판
    python Scripts/Trailer/cards.py --social Docs/Media/social-preview.png
"""
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/Trailer/cards'
FONTS = ROOT / 'Source/IndieGame/UI/Fonts'
SIZE = (1920, 1080)

# 게임 문자열을 그대로 쓴다(IGMissingFloor, NightOneCeilingKnockCaption /
# NightOneTopFloorThought, 방위 표기 IGHUD, SoundCaptionWithBearing).
TEXT = {
    'ko': {
        'knock': '[위] 천장에서 세 번 두드리는 소리',
        'topfloor': '…4층이 꼭대기인데.',
        'subtitle': 'Windows 플레이 테스트',
    },
    'en': {
        'knock': '[above] three knocks from the ceiling',
        'topfloor': '…The fourth floor’s the top floor, though.',
        'subtitle': 'Free Windows playtest',
    },
}


def font(size, bold=False):
    return ImageFont.truetype(str(FONTS / ('Pretendard-SemiBold.otf' if bold else 'Pretendard-Regular.otf')), size)


def sound_caption(text):
    """화면 아래쪽 알약 모양의 소리 자막. 왼쪽에 소리 막대 표시가 붙는다."""
    layer = Image.new('RGBA', SIZE)
    draw = ImageDraw.Draw(layer)
    typeface = font(21)
    box = draw.textbbox((0, 0), text, font=typeface)
    width = box[2] - box[0] + 76
    left = (SIZE[0] - width) // 2
    top, height = 861, 46
    draw.rounded_rectangle((left, top, left + width, top + height), radius=12, fill=(32, 36, 36, 214))
    for index, bar in enumerate((6, 12, 16, 9)):
        x = left + 20 + index * 5
        draw.rectangle((x, top + height / 2 - bar / 2, x + 2, top + height / 2 + bar / 2), fill=(170, 176, 172, 255))
    draw.text((left + 54, top + (height - (box[3] - box[1])) / 2 - box[1]), text, font=typeface, fill=(214, 214, 206, 255))
    return layer


def thought(text):
    """속말 상자. 게임처럼 넓은 상자에 왼쪽 정렬로 쓴다."""
    layer = Image.new('RGBA', SIZE)
    draw = ImageDraw.Draw(layer)
    left, top, right, bottom = 540, 922, 1380, 982
    draw.rounded_rectangle((left, top, right, bottom), radius=8, fill=(36, 40, 40, 206), outline=(86, 92, 92, 150))
    typeface = font(24)
    box = draw.textbbox((0, 0), text, font=typeface)
    draw.text((left + 30, top + (bottom - top - (box[3] - box[1])) / 2 - box[1]), text, font=typeface,
              fill=(226, 226, 219, 255))
    return layer


def centered(draw, text, y, size, bold=False, fill=(234, 234, 228, 255)):
    typeface = font(size, bold)
    box = draw.textbbox((0, 0), text, font=typeface)
    draw.text(((SIZE[0] - (box[2] - box[0])) / 2 - box[0], y - box[1]), text, font=typeface, fill=fill)


def title(subtitle):
    layer = Image.new('RGBA', SIZE)
    draw = ImageDraw.Draw(layer)
    centered(draw, 'Missing Floor', 458, 108, bold=True)
    centered(draw, subtitle, 604, 30, fill=(160, 166, 166, 255))
    return layer


def social(path):
    """저장소 미리보기 카드. 실제 게임 화면 위에 제목과 한 줄만 둔다."""
    art = Image.open(ROOT / 'Docs/Media/game-corridor-day.png').convert('RGBA')
    art = Image.alpha_composite(art, Image.new('RGBA', SIZE, (0, 0, 0, 140)))
    draw = ImageDraw.Draw(art)
    centered(draw, 'Missing Floor', 372, 128, bold=True)
    centered(draw, '오빠의 주소는 501호. 이 빌라는 4층까지다.', 574, 42)
    art.convert('RGB').resize((1280, 720), Image.Resampling.LANCZOS).save(path)


def main():
    for lang, text in TEXT.items():
        folder = OUT / lang
        folder.mkdir(parents=True, exist_ok=True)
        sound_caption(text['knock']).save(folder / 'knock.png')
        thought(text['topfloor']).save(folder / 'topfloor.png')
        title(text['subtitle']).save(folder / 'title.png')
    print('TRAILER_CARDS PASS', ', '.join(TEXT))


if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[1] == '--social':
        social(sys.argv[2])
    else:
        main()
