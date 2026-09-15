"""검침창과 호실 표기를 선명한 공용 아틀라스로 만든다. 숫자는 누적 사용량이다."""

from pathlib import Path
import calendar
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[1] / "Content/SourceArt/UtilityPrints"
OUT.mkdir(parents=True, exist_ok=True)
digits = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 112)
labels = ImageFont.truetype("C:/Windows/Fonts/malgunbd.ttf", 88)
for name, values, font, background, foreground in (
    ("MeterCounter", ("21482", "32806", "10439", "08612", "00632"), digits, (10, 13, 12), (218, 224, 214)),
    ("MeterLabel", ("401호", "402호", "403호", "공용", ""), labels, (196, 196, 179), (30, 34, 31)),
):
    # 각 행의 종횡비를 실제 창에 맞춘다. 폭을 1024로 늘여 두면 게임에서
    # 글자가 가늘고 길게 눌린다.
    image = Image.new("RGB", (416 if name == "MeterCounter" else 320, 1024), background)
    draw = ImageDraw.Draw(image)
    for index, value in enumerate(values):
        if name == "MeterCounter":
            # 각 숫자는 별도 드럼이다. 작은 면에서도 간격이 뭉개지지 않는다.
            for place, digit in enumerate(value):
                draw.text((42 + place * 83, index * 128 + 64), digit,
                          font=font, fill=foreground, anchor="mm")
                if place < 4:
                    draw.line((83 + place*83, index*128 + 8, 83+place*83, index*128 + 120), fill=(41, 45, 42), width=2)
        else:
            draw.text((image.width // 2, index * 128 + 64), value, font=font, fill=foreground, anchor="mm")
    image.save(OUT / f"{name}.png")
title_font = ImageFont.truetype("C:/Windows/Fonts/malgunbd.ttf", 46)
body_font = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 31)
for name, title, lines in (
    ("BoothAgentNote", "무영부동산 문자 사본", ["7/26  14:02  부동산", "사장님, 옥탑 짐은 다 뺐습니다.", "", "7/26  14:05  목한수", "네. 이제 창고로 쓸 거예요.", "", "7/26  14:11  부동산", "열쇠는 우편함에 넣고 갑니다."]),
    ("BoothReceipts", "자재 반입 영수증", ["무영건재 / 달빛빌라", "7/26  석고보드 9.5T 12장 · 현금", "       경량스터드 3.6m 8본", "7/27  석고보드 9.5T 12장 · 현금", "       미장몰탈 20kg 4포", "기재일 7/26 · 옥상 보수비"]),
):
    image = Image.new("RGB", (768, 1024) if name == "BoothAgentNote" else (1024, 576), (210, 207, 194))
    draw = ImageDraw.Draw(image)
    draw.text((48, 50), title, font=title_font, fill=(33, 35, 34))
    draw.line((48, 124, image.width-48, 124), fill=(81, 81, 76), width=2)
    for i, line in enumerate(lines):
        draw.text((48, 164 + i * 62), line, font=body_font, fill=(43, 44, 41))
    image.save(OUT / f"{name}.png")

image = Image.new("RGB", (768, 1024), (202, 200, 185))
draw = ImageDraw.Draw(image)
draw.rectangle((0, 0, 767, 130), fill=(58, 72, 70))
draw.text((46, 28), "2024년 7월", font=title_font, fill=(229, 228, 215))
draw.text((54, 158), "일     월     화     수     목     금     토", font=body_font, fill=(53, 56, 52))
for row, week in enumerate(calendar.Calendar(firstweekday=6).monthdayscalendar(2024, 7)):
    for col, day in enumerate(week):
        x, y = 62 + col * 105, 263 + row * 138
        if day:
            draw.text((x, y), str(day), font=body_font, fill=(103, 55, 44) if col == 0 else (44, 47, 43), anchor="mm")
            if day == 26:
                draw.ellipse((x-34, y-28, x+34, y+30), outline=(113, 45, 36), width=5)
        draw.line((col*105+10, y+65, col*105+115, y+65), fill=(158, 159, 148), width=1)
image.save(OUT / "BoothCalendar.png")
print("UTILITY_PRINTS PASS shared_atlases=2 meters=5 documents=3")
