"""소방 설비의 작은 글자와 계기판. 생성 이미지의 글자를 그대로 쓰지 않는다."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Content/SourceArt/UtilityPrints'
OUT.mkdir(parents=True, exist_ok=True)
FONT = 'C:/Windows/Fonts/malgun.ttf'
BOLD = 'C:/Windows/Fonts/malgunbd.ttf'


def text(draw, xy, value, size, fill, bold=False, anchor='mm'):
    draw.text(xy, value, font=ImageFont.truetype(BOLD if bold else FONT, size), fill=fill, anchor=anchor)


def run():
    # SUS 외함과 같은 밝기. 경종 타공은 근접해도 2mm라 인쇄면에 굽는다.
    im = Image.new('RGB', (512, 1280), (165, 169, 169))
    d = ImageDraw.Draw(im)
    for y in range(482, 666, 14):
        for x in range(154, 360, 14):
            if (x-256)**2 + (y-573)**2 < 101**2:
                d.ellipse((x-3, y-3, x+3, y+3), fill=(31, 34, 34))
    text(d, (256, 773), '화재 시 누르세요', 27, (36, 39, 38), True)
    text(d, (256, 1070), '발신기', 28, (36, 39, 38))
    text(d, (256, 1197), '화 재 경 보', 29, (36, 39, 38), True)
    im.save(OUT / 'FireAlarmFace.png')

    im = Image.new('RGB', (512, 512), (196, 194, 179))
    d = ImageDraw.Draw(im)
    d.ellipse((7, 7, 505, 505), fill=(225, 218, 194), outline=(44, 46, 40), width=8)
    d.arc((61, 61, 451, 451), 205, 263, fill=(175, 42, 26), width=44)
    d.arc((61, 61, 451, 451), 263, 293, fill=(43, 126, 62), width=44)
    d.arc((61, 61, 451, 451), 293, 337, fill=(175, 42, 26), width=44)
    for i in range(11):
        a = math.radians(205 + i*13.2)
        d.line((256+164*math.cos(a), 256+164*math.sin(a), 256+186*math.cos(a), 256+186*math.sin(a)), fill=(25, 27, 24), width=3)
    d.polygon(((245, 270), (257, 93), (270, 270)), fill=(22, 23, 21))
    d.ellipse((241, 241, 271, 271), fill=(66, 67, 55))
    text(d, (256, 364), 'MPa', 34, (36, 37, 33))
    im.save(OUT / 'ExtinguisherGauge.png')

    im = Image.new('RGB', (2048, 512), (154, 26, 22))
    d = ImageDraw.Draw(im)
    ink = (240, 231, 216)
    d.rectangle((14, 12, 2034, 500), outline=ink, width=6)
    for x in (505, 1415):
        d.line((x, 12, x, 500), fill=ink, width=4)
    text(d, (960, 90), '분말소화기', 73, ink, True)
    text(d, (960, 192), 'ABC · 3.3 kg', 56, ink, True)
    text(d, (960, 294), '일반 · 유류 · 전기 화재용', 35, ink)
    text(d, (960, 382), '압력계의 바늘을 확인하세요', 32, ink)
    text(d, (960, 445), '바늘이 녹색 범위를 벗어나면 교체', 28, ink)
    text(d, (265, 82), '사용 전 확인', 37, ink, True)
    for y, line in zip((187, 268, 349, 426), ('안전핀과 봉인', '호스 연결 상태', '몸통의 부식', '지정 위치 보관')):
        text(d, (265, y), line, 30, ink)
    text(d, (1720, 82), '사용 방법', 38, ink, True)
    for y, line in zip((190, 296, 402), ('1  안전핀을 뽑는다', '2  호스를 불 쪽으로', '3  손잡이를 움켜쥔다')):
        text(d, (1720, y), line, 29, ink)
    im.save(OUT / 'ExtinguisherLabel.png')
    print('FIRE_SAFETY_PRINTS PASS count=3')


if __name__ == '__main__':
    run()
