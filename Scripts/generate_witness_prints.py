"""조사 소품에 붙는 이름표와 근무표를 실제 글자로 조판한다."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Content/SourceArt/WitnessPrints'
FONT = 'C:/Windows/Fonts/malgun.ttf'
BOLD = 'C:/Windows/Fonts/malgunbd.ttf'


def text(draw, xy, content, size, fill='#24322e', bold=False):
    draw.text(xy, content, font=ImageFont.truetype(BOLD if bold else FONT, size), fill=fill)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    page = Image.new('RGB', (1024, 1536), '#edece5')
    d = ImageDraw.Draw(page)
    d.rounded_rectangle((80, 220, 944, 1350), radius=34, outline='#406758', width=5)
    text(d, (146, 125), '약', 104, bold=True)
    text(d, (303, 167), '처방 조제', 55, bold=True)
    text(d, (135, 312), '서일영 님', 68, bold=True)
    d.line((128, 424, 896, 424), fill='#406758', width=3)
    text(d, (135, 483), '조제일  2025. 07. 26.', 43)
    text(d, (135, 586), '복용 방법', 45, bold=True)
    text(d, (135, 675), '약사에게 안내받은 대로', 44)
    text(d, (135, 745), '복용하세요.', 44)
    text(d, (135, 1003), '복용 중 불편한 점이 있으면', 37)
    text(d, (135, 1071), '약국에 문의해 주세요.', 37)
    text(d, (267, 1200), '무영약국', 74, bold=True)
    page.save(OUT / 'PrescriptionEnvelope.png')

    page = Image.new('RGB', (1536, 2048), '#f0efe7')
    d = ImageDraw.Draw(page)
    text(d, (120, 135), '7월 마지막 주 근무표', 84, '#282b2c', True)
    text(d, (120, 275), '새벽24 무영로점', 46, '#424849')
    text(d, (120, 367), '7/28(월) — 8/3(일)', 46, '#424849')
    xs = [120, 377, 720, 1063, 1406]
    top, rowh = 518, 145
    d.rectangle((xs[0], top, xs[-1], top+rowh), fill='#dee0d8')
    for x in xs:d.line((x, top, x, top+rowh*8), fill='#656963', width=3)
    for n in range(9):d.line((xs[0], top+n*rowh, xs[-1], top+n*rowh), fill='#656963', width=3)
    for x, name in zip(xs, ['날짜','07–15시','15–23시','23–07시']):
        text(d, (x+26, top+45), name, 43, '#282b2c', True)
    dates = ['7/28 월','7/29 화','7/30 수','7/31 목','8/1 금','8/2 토','8/3 일']
    for n, date in enumerate(dates):
        y = top+(n+1)*rowh+42
        names = [date,'점장','민재','나린'] if n < 5 else [date,'민재','나린','점장']
        for x,name in zip(xs,names):text(d,(x+28,y),name,48,'#303431')
    text(d, (120, 1770), '시간 바뀌면 단톡방에 먼저 남겨 주세요.', 43, '#444943')
    text(d, (120, 1850), '교대할 때 시재 확인하고 서명.', 43, '#444943')
    page.save(OUT / 'StoreRoster.png')
    print('WITNESS_PRINTS PASS')


if __name__ == '__main__':
    main()
