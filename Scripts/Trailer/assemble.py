"""트레일러 편집. 게임에서 찍은 프레임을 이어 붙이고 색, 자막, 입자를 얹어 ffmpeg로 보낸다."""
import json
import os
import subprocess
import sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HERE = os.path.join(ROOT, "Saved", "Trailer")
SHOTS = HERE
CARDS = os.path.join(HERE, "cards")
FPS = 30
W, H = 1920, 1080
# GitHub 본문 영상은 첫 프레임이 표지다. 제목 화면으로 열고 0.45초에 걸쳐 검게 넘어간다.
LEAD = 0.70

# (시작, 길이, 종류, 설정)
SEGMENTS = [
    (0.00, 3.00, 'black', {}),
    (3.00, 0.50, 'frames', dict(shot='listener-approach', first=196, grade='night', push=0.02)),
    (3.50, 1.50, 'black', {}),
    (5.00, 4.50, 'frames', dict(shot='alley', first=14, grade='day', fade_in=0.5)),
    (9.50, 4.00, 'frames', dict(shot='bedroom-dusk', first=10, grade='day', speed=0.9)),
    (13.50, 3.50, 'frames', dict(shot='corridor-day', first=10, grade='day', fade_out=0.45)),
    (17.00, 0.40, 'black', {}),
    (17.40, 4.00, 'frames', dict(shot='bedroom-night', first=14, grade='night', fade_in=0.5, lift=1.25)),
    (21.40, 4.00, 'frames', dict(shot='stair-landing', first=0, grade='night', push=0.03)),
    (25.40, 3.00, 'frames', dict(shot='meter-cabinet', first=6, grade='night')),
    (28.40, 3.20, 'frames', dict(shot='booth-cctv', first=18, grade='night')),
    (31.60, 3.40, 'frames', dict(shot='corridor-night', first=24, grade='night', lift=1.1)),
    (35.00, 5.20, 'frames', dict(shot='listener-approach', first=50, grade='night', lift=1.15, push=0.04)),
    (40.20, 0.30, 'black', {}),
    (40.50, 2.10, 'frames', dict(shot='capture-front', first=30, grade='night', lift=1.1)),
    (42.60, 1.60, 'black', {}),
    (44.20, 3.40, 'frames', dict(shot='bedroom-night', first=0, grade='night', fade_in=0.9, lift=1.3, speed=0.8)),
    (47.60, 1.20, 'black', {}),
    (48.80, 5.60, 'still', dict(image=os.path.join(ROOT, "Content", "SourceArt", "T_TitleBackground_D.png"),
                               grade='title', fade_in=0.35, push=0.07)),
    (54.40, 6.00, 'black', {}),
]
CARD_TIMES = [
    ('c00_clock', 3.62, 4.90, 0.25, 0.25),
    ('c01_brother', 5.50, 8.95, 0.45, 0.40),
    ('c02_parcel', 9.70, 13.20, 0.40, 0.40),
    ('c03_fourfloors', 13.80, 16.75, 0.40, 0.45),
    ('c04_knock', 17.85, 21.15, 0.45, 0.40),
    ('c05_neighbors', 31.95, 34.85, 0.45, 0.40),
    ('c06_quiet', 36.10, 39.70, 0.45, 0.45),
    ('c07_loop', 44.90, 47.45, 0.45, 0.40),
    ('c08_title', 49.25, 54.10, 0.70, 0.60),
    ('c09_end', 54.70, 60.00, 0.60, 0.40),
]
LENGTH = 60.40

MARKS = {
    'cold_knock': 0.8, 'hook': 3.0, 'day_start': 5.0, 'day_end': 17.0, 'night_start': 17.4, 'night_knock': 17.55,
    'night_card': 17.85, 'stair': 21.4, 'stair_card': 21.85, 'meter': 25.4, 'booth': 28.4, 'booth_card': 31.95,
    'night_walk': 31.6, 'approach_start': 35.0, 'quiet_card': 36.1, 'approach_end': 40.2,
    'rush_start': 40.5, 'capture_start': 40.5 + (69 - 30) / FPS, 'cut': 40.5 + (93 - 30) / FPS,
    'wake': 44.2, 'title': 48.8, 'last_knock': 59.55,
}

rng = np.random.default_rng(29)
_yy, _xx = np.mgrid[0:H, 0:W].astype(np.float32)
_r = np.sqrt(((_xx - W / 2) / (W / 2)) ** 2 + ((_yy - H / 2) / (H / 2)) ** 2)
VIGNETTE = np.clip(1.0 - 0.32 * np.clip(_r - 0.35, 0, None) ** 1.6, 0.55, 1.0)[..., None]
_cache = {}


def load_frame(shot, index):
    folder = os.path.join(SHOTS, shot)
    count = len([n for n in os.listdir(folder) if n.endswith('.png')])
    index = max(0, min(count - 1, index))
    path = os.path.join(folder, 'frame_%04d.png' % index)
    return np.asarray(Image.open(path).convert('RGB'), dtype=np.float32) / 255.0


def load_still(path):
    if path not in _cache:
        _cache[path] = np.asarray(Image.open(path).convert('RGB').resize((W, H), Image.LANCZOS), dtype=np.float32) / 255.0
    return _cache[path]


def zoom(img, scale):
    if scale <= 1.0005:
        return img
    h, w = img.shape[:2]
    cw, ch = int(round(w / scale)), int(round(h / scale))
    x0, y0 = (w - cw) // 2, (h - ch) // 2
    crop = Image.fromarray((img[y0:y0 + ch, x0:x0 + cw] * 255).astype(np.uint8))
    return np.asarray(crop.resize((w, h), Image.BICUBIC), dtype=np.float32) / 255.0


def grade(img, kind, lift=1.0):
    x = img
    if kind == 'day':
        # 저녁빛은 조금 누르고 채도를 뺀다. 빨강·노랑 간판이 튀지 않게.
        lum = x.mean(axis=2, keepdims=True)
        x = lum + (x - lum) * 0.82
        x = np.clip(x * 0.95, 0, 1) ** 1.05
        x = x * np.array([1.02, 1.0, 0.96], dtype=np.float32)
    elif kind == 'night':
        # 밤은 그림자를 조금 들어 올려 몸이 읽히게 하고, 푸른 기를 얹는다.
        x = np.clip(x * lift, 0, 1) ** 0.9
        lum = x.mean(axis=2, keepdims=True)
        x = lum + (x - lum) * 0.70
        x = x * np.array([0.93, 1.0, 1.08], dtype=np.float32)
    elif kind == 'title':
        x = np.clip(x * 0.92, 0, 1)
    return np.clip(x, 0, 1)


def poster(t):
    img = grade(load_still(os.path.join(ROOT, "Content", "SourceArt", "T_TitleBackground_D.png")), 'title')
    c = load_card('c08_title')
    img = img * (1 - c[..., 3:4]) + c[..., :3] * c[..., 3:4]
    return img * max(0.0, min(1.0, (LEAD - t) / 0.45))


def frame_at(t):
    if t < LEAD:
        return poster(t)
    t -= LEAD
    for start, dur, kind, cfg in SEGMENTS:
        if start <= t < start + dur:
            local = t - start
            if kind == 'black':
                return np.zeros((H, W, 3), dtype=np.float32)
            if kind == 'frames':
                speed = cfg.get('speed', 1.0)
                img = load_frame(cfg['shot'], cfg.get('first', 0) + int(local * FPS * speed))
            else:
                img = load_still(cfg['image'])
            push = cfg.get('push', 0.0)
            if push:
                img = zoom(img, 1.0 + push * (local / dur))
            img = grade(img, cfg.get('grade', 'night'), cfg.get('lift', 1.0))
            a = 1.0
            if cfg.get('fade_in'):
                a = min(a, local / cfg['fade_in'])
            if cfg.get('fade_out'):
                a = min(a, (dur - local) / cfg['fade_out'])
            return img * max(0.0, min(1.0, a))
    return np.zeros((H, W, 3), dtype=np.float32)


def card_alpha(t, a, b, fin, fout):
    if t < a or t > b:
        return 0.0
    return max(0.0, min(1.0, (t - a) / fin, (b - t) / fout))


def load_card(name):
    if name not in _cache:
        c = np.asarray(Image.open(os.path.join(CARDS, name + '.png')).convert('RGBA'), dtype=np.float32) / 255.0
        _cache[name] = c
    return _cache[name]


def compose(t):
    img = frame_at(t)
    for name, a, b, fin, fout in CARD_TIMES:
        k = card_alpha(t - LEAD, a, b, fin, fout)
        if k > 0:
            c = load_card(name)
            alpha = c[..., 3:4] * k
            img = img * (1 - alpha) + c[..., :3] * alpha
    img = img * VIGNETTE
    grain = rng.normal(0.0, 0.011, (H // 2, W // 2, 1)).astype(np.float32)
    grain = np.repeat(np.repeat(grain, 2, axis=0), 2, axis=1)
    lum = img.mean(axis=2, keepdims=True)
    img = img + grain * (0.35 + 0.65 * np.sqrt(np.clip(lum, 0, 1)))
    return (np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8)


def main(out_path, audio_path):
    total = int(round((LENGTH + LEAD) * FPS))
    cmd = ['ffmpeg', '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '%dx%d' % (W, H),
           '-r', str(FPS), '-i', '-', '-i', audio_path, '-map', '0:v', '-map', '1:a',
           '-c:v', 'libx264', '-preset', 'slow', '-crf', '16', '-pix_fmt', 'yuv420p', '-profile:v', 'high',
           '-c:a', 'aac', '-b:a', '192k', '-shortest', '-movflags', '+faststart', out_path]
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    for i in range(total):
        proc.stdin.write(compose(i / FPS).tobytes())
        if i % 150 == 0:
            print('frame', i, '/', total, flush=True)
    proc.stdin.close()
    proc.wait()
    print('done', out_path, os.path.getsize(out_path))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--marks':
        json.dump({'length': LENGTH + LEAD, 'marks': {k: v + LEAD for k, v in MARKS.items()}},
                  open(os.path.join(HERE, 'edl.json'), 'w'), indent=1)
        print('marks written')
    elif len(sys.argv) > 1 and sys.argv[1] == '--still':
        t = float(sys.argv[2])
        Image.fromarray(compose(t)).save(os.path.join(HERE, 'still_%05.2f.png' % t))
    else:
        main(os.path.join(HERE, sys.argv[1] if len(sys.argv) > 1 else 'trailer_master.mp4'), os.path.join(HERE, 'trailer_audio.wav'))
