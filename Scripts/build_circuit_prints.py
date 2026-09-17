"""분전반 회로명과 차단기 표기. 작은 글씨도 뒤집힘 없이 원본에서 관리한다."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Content/SourceArt/UtilityPrints'
OUT.mkdir(parents=True, exist_ok=True)
FONT = 'C:/Windows/Fonts/malgun.ttf'

def label(name, text, size=(768, 192), font_size=108, background='#dadbd2'):
    image = Image.new('RGB', size, background)
    draw = ImageDraw.Draw(image)
    draw.text((size[0]/2, size[1]/2), text, font=ImageFont.truetype(FONT, font_size),
              fill='#30332f', anchor='mm')
    image.save(OUT / f'Circuit{name}.png')

for name, text in [('401','401호'), ('402','402호'), ('403','403호'), ('Common','공용 조명'), ('Spare','예비'), ('Blank','')]:
    label(name, text, font_size=95 if name == 'Common' else 108)
label('Title', '분전반', (1024,256), 158, '#b9bfba')
label('Rating', '누전차단기\n30A  220V', (512,512), 79, '#dadbd2')
label('On', 'I', (128,128), 96, '#737871')
label('Off', 'O', (128,128), 90, '#737871')
print('CIRCUIT_PRINTS PASS labels=10')
