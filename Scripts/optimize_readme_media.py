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
    # 걸면 그 둘만으로 4.5 MB다.
    "arrival-contract.png",
    "arrival-moving-boxes.png",
    "m0-knock-contact.png",
    "hud-noise-ripple.png",
    "p1-meter-cabinet.png",
    "p2-booth-desk.png",
    "night-sealed-entrance.png",
    "day-corridor-hwang.png",
    "dialogue-hud-default-1080.png",
    "dialogue-hud-accessibility-200-1080.png",
    "night1-card.png",
    "cctv5-feed.png",
    # 건물과 낮을 보여 주는 넉 장. 처음 보는 사람은 규칙보다 장소를 먼저
    # 궁금해한다.
    "prologue-villa.png",
    "prologue-corridor.png",
    "prologue-alley.png",
    "prologue-ramyeon.png",
    "prologue-not-found-note.png",
    # 샛길과 계산대. 골목이 복도가 아니라는 것과 편의점이 편의점이라는 것을
    # 이 둘이 보여 준다.
    "prologue-alley-passage.png",
    "prologue-store-counter.png",
    # 편의점 음료 매대는 라면 매대와 같은 말을 해서 뺐다. 원본은 낮 동선
    # GIF의 마지막 프레임으로 계속 쓰인다.
    # 계단참 목격 컷은 뺐다. 원본부터 거의 검은 화면이라 GitHub에서는 빈
    # 사각형으로 보인다. 게임 안에서 통하는 어둠이 문서에서도 통하지는 않는다.
    # 설정 메뉴·보정 화면·밤 4 스포일러도 README에는 걸지 않는다. 원본은 각자의
    # 계약이 Docs/Media에 증거로 잡고 있고, 표시용 파생본만 여기서 빠진다.
)

# 프롤로그 맵 캡처는 「4시 44분」 시절 모닝 루틴 디렉터가 같이 돌 때 찍혔다.
# 화면 위쪽 목표 띠에 "골목 끝 편의점에서 물을 사 오자"가 그대로 남아 있어서,
# 「없는 층」을 설명하는 README에 걸면 글과 그림이 서로 다른 말을 한다.
# 빌라·복도·골목·편의점은 두 작품이 같이 쓰는 실제 맵이므로 목표 띠만 잘라낸다.
# 원본은 Docs/Media에 그대로 두고 파생본에서만 자른다.
OBJECTIVE_BAND = 0.09
LEGACY_OBJECTIVE = frozenset((
    "prologue-villa.png",
    "prologue-corridor.png",
    "prologue-alley.png",
    "prologue-ramyeon.png",
    "prologue-not-found-note.png",
    "prologue-alley-passage.png",
    "prologue-store-counter.png",
    # 낮 동선 GIF도 같은 프롤로그 캡처를 이어 붙인 것이라 같이 자른다.
    "readme-route-preview.gif",
))

# 움직임이 설명의 절반인 것만 인라인이므로, 그 하나는 제대로 줄인다.
# 애니메이션 WebP는 브라우저마다 첫 프레임만 보이는 경우가 있어 GIF로 남긴다.
ANIMATIONS = (
    # 추격 컷은 README에서 가장 무거운 파일 하나다. 720·12fps에서 4.5 MB였고
    # 그것만으로 첫 화면 무게의 절반을 넘겼다. 640·10fps면 절반 아래로 떨어지고,
    # GitHub 본문 폭에서는 차이가 눈에 띄지 않는다.
    ("night-listener-chase.gif", 640, 10),
    ("m1-capture-embrace.gif", 640, 12),
    # 기상 잔향은 README에서 뺐지만 파생본 자체는 M1 기상 잔향 계약이
    # 파일로 잡고 있어 계속 만든다.
    ("m1-capture-wake-echo.gif", 640, 12),
    # 접힌 <details> 안이라 첫 화면에는 안 걸린다. 펼친 사람에게는 이 둘이
    # 문서 무게의 대부분이다.
    ("night1-extinguisher-drop.gif", 640, 12),
    ("readme-route-preview.gif", 640, 10),
)

MAX_WIDTH = 1600
QUALITY = 86


def objective_band(height):
    """잘라낼 위쪽 목표 띠의 픽셀 높이."""
    return int(round(height * OBJECTIVE_BAND))


def optimize_still(name):
    source = os.path.join(MEDIA, name)
    target = os.path.join(OUT, os.path.splitext(name)[0] + ".webp")
    with Image.open(source) as image:
        frame = image.convert("RGB")
        if name in LEGACY_OBJECTIVE:
            top = objective_band(frame.height)
            frame = frame.crop((0, top, frame.width, frame.height))
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
    if name in LEGACY_OBJECTIVE:
        with Image.open(source) as probe:
            top = objective_band(probe.height)
            chain = "crop={0}:{1}:0:{2},".format(
                probe.width, probe.height - top, top) + chain
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
