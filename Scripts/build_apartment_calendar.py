"""입주 계약서와 같은 2025년 7월 달력. 날짜는 생성 이미지에 맡기지 않는다."""
import calendar
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[1] / 'Content/SourceArt/UtilityPrints/ApartmentCalendar2025.png'
OUT.parent.mkdir(parents=True, exist_ok=True)
im = Image.new('RGB', (620, 840), (225, 224, 215))
d = ImageDraw.Draw(im)
regular = lambda size: ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', size)
bold = lambda size: ImageFont.truetype('C:/Windows/Fonts/malgunbd.ttf', size)
d.text((36, 46), '2025', font=regular(38), fill=(48, 49, 44))
d.text((568, 96), '7', font=bold(114), fill=(39, 41, 38), anchor='rm')
for col, day in enumerate('일월화수목금토'):
    ink = (141, 46, 34) if col == 0 else (46, 50, 46)
    d.text((58+col*84, 199), day, font=regular(24), fill=ink, anchor='mm')
for row, week in enumerate(calendar.Calendar(firstweekday=6).monthdayscalendar(2025, 7)):
    for col, day in enumerate(week):
        x, y = 58+col*84, 283+row*88
        if day:
            d.text((x, y), str(day), font=regular(35), fill=(150, 46, 33) if col == 0 else (43, 46, 42), anchor='mm')
    d.line((22, 326+row*88, 598, 326+row*88), fill=(159, 161, 151), width=1)
d.text((310, 784), '왕발통닭 · 호프', font=bold(24), fill=(74, 70, 61), anchor='mm')
im.save(OUT)
print('APARTMENT_CALENDAR PASS year=2025 month=7 first=Tuesday days=31')
