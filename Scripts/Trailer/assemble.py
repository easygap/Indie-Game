"""실제 게임에서 30fps로 찍은 프레임을 이어 붙여 예고편을 만든다.

구성은 참고작 두 편과 예고편 편집자들의 조언을 따른다. 설명 자막 없이 소리로 연다.
어두운 방에서 천장을 올려다보는 장면으로 시작해, 낮의 빌라를 한 사람의 걸음으로
보여 주고, 밤의 복도에서 붙잡히는 순간 끊는다. 색 보정과 입자는 따로 얹지 않는다.
화면 질감은 게임 자체의 것이다.

    python Scripts/Trailer/assemble.py --marks          # 편집표(edl.json)만 쓴다
    python Scripts/Trailer/assemble.py ko               # MissingFloor-Trailer.mp4
    python Scripts/Trailer/assemble.py en               # MissingFloor-Trailer-en.mp4
    python Scripts/Trailer/assemble.py --still 12.0 ko  # 한 장만 확인
"""
from pathlib import Path
from functools import lru_cache
import json
import subprocess
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
HERE = ROOT / 'Saved/Trailer'
FPS = 30
SIZE = (1920, 1080)
# (시작 초, 길이 초, 촬영 폴더, 첫 프레임). 빈 폴더명은 암전이다.
SEGMENTS = [
    (0.0, 2.6, '', 0),
    (2.6, 5.8, 'bedroom-night', 0),
    (8.4, 0.8, '', 0),
    (9.2, 6.7, 'corridor-day', 0),
    (15.9, 4.5, 'door-402', 0),
    (20.4, 4.5, 'alley', 5),
    (24.9, 3.5, 'meter-cabinet', 0),
    (28.4, 3.5, 'booth-cctv', 10),
    (31.9, 0.8, '', 0),
    (32.7, 6.4, 'corridor-night', 6),
    (39.1, 4.0, 'stair-landing', 0),
    (43.1, 3.0, 'listener-approach', 0),
    (46.1, 1.4, 'capture-front', 4),
    (47.5, 9.5, '', 0),
]
# (글자 판, 시작, 끝). 두 언어가 같은 자리를 쓴다.
CARDS = [('knock', 1.0, 3.9), ('topfloor', 4.6, 8.1), ('title', 49.5, 57.0)]
LENGTH = 57.0
# 붙잡힘(grab)은 capture-front 24번째 프레임, 고개가 꺾이는 순간이다.
MARKS = {
    'first_knock': 0.8, 'wake': 2.6, 'day': 9.2, 'door': 15.9, 'alley': 20.4, 'meter': 24.9,
    'booth': 28.4, 'night': 32.7, 'stair': 39.1, 'approach': 43.1, 'rush': 46.1, 'grab': 46.77,
    'cut': 47.5, 'title': 49.5, 'answer': 52.3,
}
OUTPUT = {'ko': 'MissingFloor-Trailer.mp4', 'en': 'MissingFloor-Trailer-en.mp4'}


def validate_frames():
    previous_end = 0
    for start, length, shot, first in SEGMENTS:
        if abs(start - previous_end) > 0.001:
            raise ValueError(f'편집 구간이 이어지지 않습니다: {start}')
        previous_end = start + length
        if not shot:
            continue
        for n in range(first, first + round(length * FPS)):
            if not (HERE / shot / f'frame_{n:04d}.png').is_file():
                raise FileNotFoundError(f'촬영 프레임이 없습니다: {shot}/{n}')
    if abs(previous_end - LENGTH) > 0.001:
        raise ValueError('편집 길이가 다릅니다.')


@lru_cache(maxsize=8)
def card(lang, name):
    return Image.open(HERE / 'cards' / lang / f'{name}.png').convert('RGBA')


def compose(frame_index, lang):
    t = frame_index / FPS
    img = Image.new('RGBA', SIZE, (0, 0, 0, 255))
    for start, length, shot, first in SEGMENTS:
        if round(start * FPS) <= frame_index < round((start + length) * FPS):
            if shot:
                index = first + frame_index - round(start * FPS)
                with Image.open(HERE / shot / f'frame_{index:04d}.png') as source:
                    if source.size != SIZE:
                        raise ValueError(f'촬영 해상도가 다릅니다: {shot}, {source.size}')
                    img = source.convert('RGBA')
            break
    for name, start, end in CARDS:
        if start <= t < end:
            fade = 0.6 if name == 'title' else 0.2
            alpha = max(0.0, min(1.0, (t - start) / fade, (end - t) / fade))
            layer = card(lang, name).copy()
            layer.putalpha(layer.getchannel('A').point(lambda x: round(x * alpha)))
            img = Image.alpha_composite(img, layer)
    return img.convert('RGB')


def main(lang):
    validate_frames()
    out = HERE / OUTPUT[lang]
    cmd = ['ffmpeg', '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '1920x1080',
           '-r', str(FPS), '-i', '-', '-i', str(HERE / 'trailer_audio.wav'), '-map', '0:v', '-map', '1:a',
           '-c:v', 'libx264', '-preset', 'slow', '-crf', '17', '-pix_fmt', 'yuv420p',
           '-c:a', 'aac', '-b:a', '192k', '-movflags', '+faststart', '-t', str(LENGTH), str(out)]
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    try:
        for n in range(round(LENGTH * FPS)):
            proc.stdin.write(compose(n, lang).tobytes())
            if n % 300 == 0:
                print(f'편집 {n}/{round(LENGTH * FPS)}', flush=True)
    finally:
        proc.stdin.close()
    if proc.wait() != 0:
        raise RuntimeError('ffmpeg 인코딩 실패')
    print(f'TRAILER PASS {out} {LENGTH}s')


if __name__ == '__main__':
    HERE.mkdir(parents=True, exist_ok=True)
    if len(sys.argv) > 1 and sys.argv[1] == '--marks':
        (HERE / 'edl.json').write_text(json.dumps(
            {'length': LENGTH, 'fps': FPS, 'segments': SEGMENTS, 'marks': MARKS}, indent=2), encoding='utf-8')
    elif len(sys.argv) > 2 and sys.argv[1] == '--still':
        lang = sys.argv[3] if len(sys.argv) > 3 else 'ko'
        compose(round(float(sys.argv[2]) * FPS), lang).save(HERE / f'still-{lang}-{sys.argv[2]}.png')
    else:
        main(sys.argv[1] if len(sys.argv) > 1 else 'ko')
