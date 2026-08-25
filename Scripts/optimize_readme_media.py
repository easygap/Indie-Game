# -*- coding: utf-8 -*-
"""README에 인라인으로 걸리는 캡처의 표시용 파생본을 만든다.

원본은 건드리지 않는다. Docs/Media의 1080p 캡처는 접근성 설정과 대화 HUD의
증거이고 계약이 그 경로를 고정하고 있어서, 해상도를 깎으면 증거가 아니게 된다.
그래서 표시용으로만 Docs/Media/readme/에 축소본을 따로 만든다.

왜 필요한가: 원본을 그대로 인라인하면 README를 여는 순간 37 MB를 받는다.
GitHub는 이미지를 camo로 프록시하므로 그 무게가 그대로 첫 화면 지연이 된다.
어두운 게임 화면은 JPEG에서 계조가 뭉치므로 WebP를 쓴다.

    python Scripts/optimize_readme_media.py
"""

import os
import shutil
import subprocess
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MEDIA = os.path.join(ROOT, "Docs", "Media")
OUT = os.path.join(MEDIA, "readme")

# 인라인으로 걸리는 무거운 캡처만 줄인다. 접힌 <details> 안이나 이미 작은
# 파일은 그대로 쓴다 — 파생본이 하나 늘 때마다 확인해야 할 것도 하나 늘어난다.
STILLS = (
    "title-menu-first-run-1080.png",
    # 입주 첫날 두 장. 원본이 각각 2 MB를 넘어서 README 첫 화면에 그대로
    # 걸면 그 둘만으로 4.5 MB다. prologue-villa는 README를 플레이어 관점으로
    # 다시 쓰면서 빠졌는데 이 목록에 남아, 아무도 참조하지 않는 파생본을 매번
    # 만들고 있었다.
    "arrival-contract.png",
    "arrival-moving-boxes.png",
    "m0-knock-contact.png",
    "night1-stair-sighting.png",
    "hud-noise-ripple.png",
    "p1-meter-cabinet.png",
    "p2-booth-desk.png",
    "night-sealed-entrance.png",
    "day-corridor-hwang.png",
    "dialogue-hud-default-1080.png",
    "dialogue-hud-accessibility-200-1080.png",
    "night1-card.png",
    "cctv5-feed.png",
    "prologue-alley.png",
    "prologue-store.png",
    "prologue-not-found-note.png",
    # 설정 메뉴·보정 화면·밤 4 스포일러는 README에서 뺐다. 원본은 각자의
    # 계약이 Docs/Media에 증거로 잡고 있고, 표시용 파생본만 여기서 빠진다.
)

# 움직임이 설명의 절반인 것만 인라인이므로, 그 하나는 제대로 줄인다.
# 애니메이션 WebP는 브라우저마다 첫 프레임만 보이는 경우가 있어 GIF로 남긴다.
ANIMATIONS = (
    ("night-listener-chase.gif", 720, 12),
    ("m1-capture-embrace.gif", 640, 12),
    # 기상 잔향은 README에서 뺐지만 파생본 자체는 M1 기상 잔향 계약이
    # 파일로 잡고 있어 계속 만든다.
    ("m1-capture-wake-echo.gif", 640, 12),
    # 접힌 <details> 안이라 첫 화면에는 안 걸리지만, 펼친 사람에게는 이 둘이
    # 문서 무게의 대부분이다. 본문에 적어 둔 용량 표기도 함께 맞춰야 한다.
    ("night1-extinguisher-drop.gif", 640, 12),
    ("readme-route-preview.gif", 640, 10),
)

MAX_WIDTH = 1600
QUALITY = 86


def optimize_still(name):
    source = os.path.join(MEDIA, name)
    target = os.path.join(OUT, os.path.splitext(name)[0] + ".webp")
    with Image.open(source) as image:
        frame = image.convert("RGB")
        if frame.width > MAX_WIDTH:
            height = round(frame.height * MAX_WIDTH / frame.width)
            frame = frame.resize((MAX_WIDTH, height), Image.LANCZOS)
        frame.save(target, "WEBP", quality=QUALITY, method=6)
    return source, target


def optimize_animation(name, width, fps):
    source = os.path.join(MEDIA, name)
    target = os.path.join(OUT, name)
    palette = os.path.join(OUT, "_palette.png")
    chain = "fps={0},scale={1}:-1:flags=lanczos".format(fps, width)
    subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", "-i", source,
         "-vf", chain + ",palettegen=stats_mode=diff", palette],
        check=True)
    subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", "-i", source, "-i", palette,
         "-lavfi", chain + " [x]; [x][1:v] paletteuse=dither=bayer:bayer_scale=3",
         target],
        check=True)
    os.remove(palette)
    return source, target


def main():
    if not shutil.which("ffmpeg"):
        print("ffmpeg를 찾을 수 없습니다. GIF 축소를 건너뜁니다.", file=sys.stderr)
    os.makedirs(OUT, exist_ok=True)
    before = after = 0
    for name in STILLS:
        source, target = optimize_still(name)
        source_size, target_size = os.path.getsize(source), os.path.getsize(target)
        before += source_size
        after += target_size
        print("%-44s %7.2f MB -> %6.2f MB" % (
            name, source_size / 1048576.0, target_size / 1048576.0))
        if target_size >= source_size:
            raise SystemExit("파생본이 원본보다 크다: %s" % name)
    if shutil.which("ffmpeg"):
        for name, width, fps in ANIMATIONS:
            source, target = optimize_animation(name, width, fps)
            source_size, target_size = os.path.getsize(source), os.path.getsize(target)
            before += source_size
            after += target_size
            print("%-44s %7.2f MB -> %6.2f MB" % (
                name, source_size / 1048576.0, target_size / 1048576.0))
            if target_size >= source_size:
                raise SystemExit("파생본이 원본보다 크다: %s" % name)
    print("-" * 62)
    print("%-44s %7.2f MB -> %6.2f MB" % (
        "합계", before / 1048576.0, after / 1048576.0))


if __name__ == "__main__":
    main()
