"""장부 표지와 민원 밑장의 정확한 한글 인쇄. 사진 질감은 별도 생성 원본을 쓴다."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Content/SourceArt/UtilityPrints'
OUT.mkdir(parents=True, exist_ok=True)
FONT = 'C:/Windows/Fonts/malgun.ttf'
HAND = str(ROOT / 'Scripts/fonts/NanumPenScript/NanumPenScript-Regular.ttf')

label = Image.new('RGB', (1024, 512), '#d8d0b6')
d = ImageDraw.Draw(label)
for y, text, size in [(62, '달빛빌라', 68), (182, '민원 처리 대장', 92), (353, '2024년', 55)]:
    d.text((512, y), text, font=ImageFont.truetype(FONT, size), fill='#272b23', anchor='mt')
label.save(OUT / 'ComplaintLedgerCover.png')

# R: 눌린 글씨(흑연이 닿지 않는 부분). G: 원래 인쇄된 제목과 줄.
size = (1024, 1448)
letters = Image.new('L', size, 0)
form = Image.new('L', size, 0)
r, g = ImageDraw.Draw(letters), ImageDraw.Draw(form)
g.text((512, 95), '민원 접수', font=ImageFont.truetype(FONT, 54), fill=210, anchor='mt')
g.text((512, 170), '접수일 / 호실 / 내용', font=ImageFont.truetype(FONT, 27), fill=150, anchor='mt')
for y in (312, 648, 983, 1305):
    g.line((90, y, 934, y), fill=38, width=2)
for y, first, second in [
    (382, '7/27  401호', '벽에서 쿵쿵. 사람 소리 같음'),
    (475, '7/28  401호', '어제보다 작지만 계속 들림'),
    (697, '7/29  401호', '새벽에 다시 들림'),
    (790, '7/30  401호', '멎었다가 다시. 현장 확인 요청'),
    (1062, '7/31  401호', '아직 들림. 언제 오는지 문의'),
]:
    r.text((150, y), first, font=ImageFont.truetype(HAND, 48), fill=255)
    r.text((150, y + 43), second, font=ImageFont.truetype(HAND, 48), fill=255)
Image.merge('RGB', (letters, form, Image.new('L', size, 0))).save(OUT / 'ComplaintImpressionMask.png')
print('BOOTH_PRINTS PASS cover=1 impression=1')
