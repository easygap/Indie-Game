"""간편식과 페인트 통에 붙는 글씨. 재질 사진과 분리해 읽을 수 있게 조판한다."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[1] / 'Content/SourceArt/UtilityPrints'
OUT.mkdir(parents=True, exist_ok=True)
F = 'C:/Windows/Fonts/malgun.ttf'
B = 'C:/Windows/Fonts/malgunbd.ttf'
NAMES = [('참치마요', '#365c40'), ('전주비빔', '#983b30'), ('김치볶음', '#ac6037'), ('불고기', '#684b32')]
for i, (name, color) in enumerate(NAMES):
    im = Image.new('RGB', (768, 480), '#eee9dc')
    d = ImageDraw.Draw(im)
    for text, y, size, bold in [('새벽24', 26, 35, True), (name, 114, 81, True),
            ('삼각김밥', 230, 32, False), ('110 g  ·  1,200원', 304, 37, False), ('냉장보관  0~10℃', 381, 25, False)]:
        d.text((384,y),text,font=ImageFont.truetype(B if bold else F,size),fill=color if bold else '#353730',anchor='mt')
    im.save(OUT / f'KimbapFront{i}.png')
    im = Image.new('RGB', (512, 512), '#e9e6d9'); d=ImageDraw.Draw(im)
    for text,y,sz in [(name,25,39),('쌀(국산), 김(국산)',95,24),('냉장 보관 / 개봉 후 바로 드세요',146,21),
            ('표시된 순서대로 포장을 벗겨 주세요',194,21),('1. 가운데 띠를 아래로 당기기',258,21),('2. 오른쪽 비닐 벗기기',299,21),('3. 왼쪽 비닐 벗기기',340,21)]:
        d.text((28,y),text,font=ImageFont.truetype(F,sz),fill='#373b35')
    # 장면 소품용 식별 무늬다. 실제 결제용 바코드로 사용하지 않는다.
    for j in range(70):
        if (j*19)%11 < 5: d.rectangle((42+j*6,410,44+j*6,477),fill='#252922')
    im.save(OUT / f'KimbapBack{i}.png')
for n in (1,2,3):
    im=Image.new('RGB',(128,128),'#eee9dc'); d=ImageDraw.Draw(im)
    d.text((64,15),str(n),font=ImageFont.truetype(B,86),fill='#324638',anchor='mt')
    im.save(OUT / f'KimbapTab{n}.png')
im=Image.new('RGB',(768,768),'#e1ded1'); d=ImageDraw.Draw(im)
d.rectangle((0,55,768,168),fill='#365447')
d.text((384,73),'실내용 수성 도료',font=ImageFont.truetype(B,64),anchor='mt',fill='#eeece2')
for text,y,size in [('무광 백색',241,83),('벽 · 천장용',371,45),('4 L',489,89),('사용 후 뚜껑을 꼭 닫아 주세요',649,29)]:
    d.text((384,y),text,font=ImageFont.truetype(F,size),anchor='mt',fill='#343c35')
im.save(OUT / 'WorkPaintCan.png')
im=Image.new('RGB',(768,288),'#dcd8c7'); d=ImageDraw.Draw(im)
d.text((384,24),'백도하',font=ImageFont.truetype(F,104),anchor='mt',fill='#373b35')
d.text((384,176),'조율 · 수리',font=ImageFont.truetype(F,48),anchor='mt',fill='#52554a')
im.save(OUT / 'TunerNotebook.png')
