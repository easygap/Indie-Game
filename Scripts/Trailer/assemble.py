"""실제 게임에서 촬영한 프레임을 30fps로 편집한다. 색과 입자는 원본 그대로 쓴다."""
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
# 시작 시각, 길이, 촬영 폴더, 첫 프레임. 빈 폴더명은 암전이다.
SEGMENTS = [
    (0.0, 4.0, 'corridor-day', 0),
    (4.0, 4.0, 'bedroom-dusk', 15),
    (8.0, 3.0, 'alley', 20),
    (11.0, 4.0, 'bedroom-night', 0),
    (15.0, 3.3, 'meter-cabinet', 0),
    (18.3, 3.2, 'booth-cctv', 10),
    (21.5, 4.7, 'corridor-night', 0),
    (26.2, 4.5, 'listener-approach', 40),
    (30.7, 1.0, 'capture-front', 45),
    (31.7, 0.5, '', 0),
    (32.2, 4.5, '', 0),
]
CARDS = [('address', 0.6, 3.7), ('fourfloors', 4.6, 7.8), ('knock', 11.5, 14.8), ('ending', 32.4, 36.7)]
LENGTH = 36.7
MARKS = {'night': 11.0, 'knock': 12.0, 'meter': 15.0, 'booth': 18.3, 'walk': 21.5,
         'approach': 26.2, 'rush': 30.7, 'grab': 31.5, 'cut': 31.7, 'title': 32.2, 'last_knock': 34.9}


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


@lru_cache(maxsize=4)
def card(name):
    return Image.open(HERE / 'cards' / f'{name}.png').convert('RGBA')


def compose(frame_index):
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
            alpha = max(0, min(1, (t - start) / 0.25, (end - t) / 0.25))
            layer = card(name).copy()
            layer.putalpha(layer.getchannel('A').point(lambda x: round(x * alpha)))
            img = Image.alpha_composite(img, layer)
    return img.convert('RGB')


def main(out):
    validate_frames()
    cmd = ['ffmpeg', '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '1920x1080',
           '-r', str(FPS), '-i', '-', '-i', str(HERE / 'trailer_audio.wav'), '-map', '0:v', '-map', '1:a',
           '-c:v', 'libx264', '-preset', 'medium', '-crf', '18', '-pix_fmt', 'yuv420p',
           '-c:a', 'aac', '-b:a', '192k', '-movflags', '+faststart', '-t', str(LENGTH), str(out)]
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    try:
        for n in range(round(LENGTH * FPS)):
            proc.stdin.write(compose(n).tobytes())
            if n % 180 == 0:
                print(f'편집 {n}/{round(LENGTH * FPS)}', flush=True)
    finally:
        proc.stdin.close()
    if proc.wait() != 0:
        raise RuntimeError('ffmpeg 인코딩 실패')
    print(f'TRAILER PASS {out} {LENGTH}s')


if __name__ == '__main__':
    HERE.mkdir(parents=True, exist_ok=True)
    if len(sys.argv) > 1 and sys.argv[1] == '--marks':
        (HERE / 'edl.json').write_text(json.dumps({'length': LENGTH, 'fps': FPS, 'segments': SEGMENTS, 'marks': MARKS}, indent=2), encoding='utf-8')
    elif len(sys.argv) > 1 and sys.argv[1] == '--still':
        compose(round(float(sys.argv[2]) * FPS)).save(HERE / f'still-{sys.argv[2]}.png')
    else:
        main(HERE / (sys.argv[1] if len(sys.argv) > 1 else 'MissingFloor-Trailer.mp4'))