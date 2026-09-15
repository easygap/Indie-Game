"""실제 맵에서 캡처한 네 시점을 모니터의 일시 정지 화면으로 묶는다.

먼저 Run-FixtureReview.ps1 -BakeCctv를 실행한다. 임의의 건물 사진을 넣지 않는다.
"""

import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Content/SourceArt/Cctv"
OUT.mkdir(parents=True, exist_ok=True)
views = [
    ("entrance", "01  현관", [490, -255, 229], [-25, -30, 0]),
    ("parking", "02  골목", [2000, -460, 230], [-12, 180, 0]),
    ("stair", "03  계단 앞", [30, -325, 1100], [-18, 180, 0]),
    ("corridor", "04  복도", [680, -325, 1104], [-13, 180, 0]),
]
atlas = Image.new("RGB", (1024, 768), (8, 12, 12))
font = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 24)
footer = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 25)
records = []
for index, (name, label, eye, rotation) in enumerate(views):
    source = ROOT / f"Docs/Media/cctv-source-{name}.png"
    with Image.open(source) as original:
        if original.size != (1920, 1080):
            raise ValueError(f"출하 해상도 캡처가 아닙니다: {source}")
        # 네 분할 화면은 각각 4:3이다. 종횡비를 늘여 건물이 찌그러지지 않게 자른다.
        frame = ImageOps.fit(original.convert("RGB"), (504, 346), Image.Resampling.LANCZOS)
    x, y = 4 + (index % 2) * 512, 4 + (index // 2) * 362
    atlas.paste(frame, (x, y))
    draw = ImageDraw.Draw(atlas)
    draw.rectangle((x, y + 312, x + 503, y + 345), fill=(9, 14, 14))
    draw.text((x + 9, y + 314), label, fill=(195, 205, 203), font=font)
    records.append({"source": source.relative_to(ROOT).as_posix(),
                    "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                    "eye_cm": eye, "pitch_yaw_roll": rotation, "fov": 78})
draw.text((15, 728), "Ⅱ  일시 정지", fill=(175, 188, 184), font=footer)
draw.text((853, 728), "HDD 없음", fill=(235, 160, 111), font=footer)
atlas.save(OUT / "CctvStandby.png")
(OUT / "CctvStandby.json").write_text(json.dumps({
    "source": "Run-FixtureReview.ps1 -BakeCctv / 실제 게임 D3D12 캡처",
    "resolution": [1024, 768], "runtime_scene_captures": 0,
    "frames": records,
}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("CCTV_STANDBY PASS actual_views=4 atlas=1024x768")
