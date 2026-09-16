"""저수조와 두 배관의 표찰. 수치·한글은 폰트로 확정해 읽는 내용과 맞춘다."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[1] / 'Content/SourceArt/UtilityPrints'
OUT.mkdir(parents=True, exist_ok=True)
FONT = 'C:/Windows/Fonts/malgun.ttf'
BOLD = 'C:/Windows/Fonts/malgunbd.ttf'


def plate(name, size, lines):
    im = Image.new('RGB', size, (166, 170, 167))
    d = ImageDraw.Draw(im)
    d.rectangle((10, 10, size[0]-11, size[1]-11), outline=(72, 76, 73), width=2)
    for text, y, px, bold in lines:
        d.text((size[0]/2, y), text, anchor='mt', font=ImageFont.truetype(BOLD if bold else FONT, px), fill=(24, 29, 27))
    im.save(OUT / (name + '.png'))


plate('RoofTankPlate', (1024, 600), [('생활용수 저수조', 50, 78, True),
    ('용량 2,000 L', 182, 94, True), ('정상 수위 1,500 mm', 335, 58, False),
    ('점검 후 뚜껑을 닫아 주세요', 461, 48, False)])
plate('RoofDrainPlate', (768, 384), [('세척 배수', 60, 110, True), ('반시계 방향으로 열림', 226, 56, False)])
plate('RoofBypassPlate', (768, 384), [('부자밸브 우회', 65, 83, True), ('반시계 방향으로 열림', 226, 56, False)])
