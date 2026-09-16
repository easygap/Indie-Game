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

# 빈 종이를 붙여 놓고 팝업에서만 읽게 하지 않는다. 벽에 보이는 표와
# 실제로 읽는 문서의 숫자를 같은 내용으로 인쇄한다.
for name, title, lines in (
    ("PumpProcedure", "저수조 세척 절차", ["2019.03 / 관리실 보관", "", "1. 옥상 세척 배수 밸브 개방", "2. 옥상 부자밸브 우회 개방", "3. 관리실 이송펌프 선택반 → 수동", "", "배수 전에 우회를 열면 넘칩니다.", "관을 열기 전에 펌프를 켜지 마세요.", "", "역지변 · 인터록 10초"]),
    ("LobbyWaterNotice", "단수 안내", ["7월 26일 금요일", "오전 4시 ~ 6시", "", "옥상 물탱크를 청소합니다.", "물을 미리 받아 두세요.", "", "작업 중 옥상 출입을 삼가 주세요.", "", "달빛빌라 관리사무소"]),
    ("LobbyContactNotice", "입주민 안내", ["우편물은 해당 호실 우편함에", "넣어 주세요.", "", "공용 설비가 고장 났거나", "소음으로 불편하시면", "1층 관리실에 말씀해 주세요.", "", "택배는 출입문 앞에 두지 마세요."]),
    ("LobbyForumPrint", "관리인께 드립니다", ["층간소음 카페 게시글 사본", "", "6/30  새벽 네 시만 되면 위에서", "뭘 질질 끕니다. 자다가 매번 깨요.", "", "7/12  창고라 사람이 없대요.", "그럼 이 소리는 어디서 나는 건가요?", "", "7/26  03:12  또 시작됐네요.", "오늘은 직접 올라가 보려고요.", "", "댓글: 일단 녹음해 두세요."]),
):
    image = Image.new("RGB", (724, 1024), (218, 217, 209))
    draw = ImageDraw.Draw(image)
    draw.text((56, 70), title, font=title_font, fill=(32, 35, 34))
    draw.line((56, 146, 668, 146), fill=(91, 94, 90), width=2)
    for i, line in enumerate(lines):
        draw.text((56, 190 + i * 57), line, font=body_font, fill=(42, 44, 42))
    image.save(OUT / f"{name}.png")

image = Image.new("RGB", (724, 1024), (218, 217, 209))
draw = ImageDraw.Draw(image)
draw.text((50, 68), "달빛빌라 검침 기록", font=title_font, fill=(34, 37, 35))
draw.text((50, 146), "월 사용량 (kWh) / 2024년", font=body_font, fill=(50, 53, 50))
table_font = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 33)
for row, values in enumerate((("호실", "4월", "5월", "6월", "7월"),
                              ("401", "182", "174", "169", "201"),
                              ("402", "240", "233", "251", "266"),
                              ("403", "118", "121", "115", "130"),
                              ("공용", "97", "102", "99", "104"),
                              ("", "63", "58", "61", "0"))):
    y = 270 + row * 84
    draw.line((45, y-40, 678, y-40), fill=(134, 138, 132), width=2)
    for col, value in enumerate(values):
        draw.text((107 + col * 127, y), value, font=table_font, fill=(40, 44, 40), anchor="mm")
draw.line((45, 734, 678, 734), fill=(134, 138, 132), width=2)
draw.text((50, 780), "공용: 복도등", font=body_font, fill=(45, 49, 44))
draw.text((50, 836), "공란: 24.07부터 검침 생략", font=body_font, fill=(45, 49, 44))
draw.text((50, 892), "회로 확인 후 기입", font=body_font, fill=(45, 49, 44))
image.save(OUT / "LobbyMeterSheet.png")
print("UTILITY_PRINTS PASS shared_atlases=2 meters=5 documents=8")
