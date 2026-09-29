"""트레일러 음향. 편집표(edl.json)의 시각에 맞춰 효과음과 음악을 얹는다.

음악은 조율이 틀어진 업라이트 피아노와 건물 웅웅거림 두 가지뿐이다. 노크가
박자를 맡고, 마지막 타격 앞에는 거의 아무 소리도 두지 않는다.
"""
import json
import os
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from synth import (SR, load, lowpass, highpass, bandpass, pan, piano_note, sub_hit,
                   riser, drone, reverb_ir, apply_reverb, write_wav)

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HERE = os.path.join(ROOT, 'Saved', 'Trailer')
edl = json.load(open(os.path.join(HERE, 'edl.json'), encoding='utf-8'))
T = edl['marks']
LENGTH = edl['length']
N = int(LENGTH * SR) + SR
mix_dry = np.zeros((N, 2), dtype=np.float32)
mix_wet = np.zeros((N, 2), dtype=np.float32)   # 방 울림을 탈 소리
music = np.zeros((N, 2), dtype=np.float32)


def at(t):
    return int(t * SR)


def put(buf, x, t, gain=1.0, p=0.0):
    if x.ndim == 1:
        x = pan(x, p)
    i = at(t)
    j = min(N, i + len(x))
    if j > i:
        buf[i:j] += x[:j - i] * gain


def fade(x, fin=0.0, fout=0.0):
    x = x.copy()
    n = len(x)
    if fin > 0:
        k = min(n, int(fin * SR))
        x[:k] *= np.linspace(0, 1, k)[:, None] if x.ndim == 2 else np.linspace(0, 1, k)
    if fout > 0:
        k = min(n, int(fout * SR))
        x[n - k:] *= np.linspace(1, 0, k)[:, None] if x.ndim == 2 else np.linspace(1, 0, k)
    return x


def loop_to(x, seconds):
    n = int(seconds * SR)
    reps = int(np.ceil(n / len(x)))
    return np.tile(x, reps)[:n]


# --- 공간음 -----------------------------------------------------------------
city = load('Bed_City_Night')
corridor = load('Bed_Corridor')
hum = load('Hum_Machine')

# 첫 암전: 방의 공기만 천천히 올라온다.
put(mix_dry, fade(lowpass(loop_to(corridor, T['day_start']), 2400), 1.5, 0.3), 0.0, 0.35)
# 입주 저녁: 동네 소리
day_len = T['day_end'] - T['day_start']
put(mix_dry, fade(loop_to(city, day_len), 0.6, 0.8), T['day_start'], 0.55)
# 밤: 복도 공기와 기계 웅웅거림
night_len = T['rush_start'] - T['night_start']
put(mix_dry, fade(lowpass(loop_to(corridor, night_len), 2600), 0.8, 1.0), T['night_start'], 0.22)
put(mix_dry, fade(lowpass(loop_to(hum, night_len), 900), 1.5, 1.0), T['night_start'], 0.07)

# --- 노크 -------------------------------------------------------------------
knock_muffled = load('Entity_KnockTriple_Muffled')
knock_triple = load('Entity_KnockTriple')
plaster = [load('Knock_Plaster_%d' % i) for i in range(3)]

# 콜드 오픈: 천장 너머에서 세 번. 먹먹하게.
put(mix_wet, lowpass(knock_muffled, 1600), T['cold_knock'], 0.95)
# 밤 첫 장면: 천장에서 세 번, 크게.
put(mix_wet, knock_triple, T['night_knock'], 0.9)
# 포획 뒤 암전 속 노크 둘(게임과 같은 0.42초 간격). 방을 타지 않는다.
put(mix_dry, plaster[0], T['cut'] + 0.72, 0.55, -0.1)
put(mix_dry, plaster[2], T['cut'] + 1.14, 0.50, 0.1)
# 끝: 한 번만.
put(mix_wet, lowpass(plaster[1], 2000), T['last_knock'], 0.85)

# --- 효과음 -----------------------------------------------------------------
put(mix_dry, load('Player_Breath_Scared')[:int(3.2 * SR)], T['night_walk'], 0.30)
crawl = [load('Entity_CrawlStep_%d' % i) for i in range(3)]
t = T['approach_start'] + 0.6
k = 0
while t < T['approach_end'] - 0.3:
    # 가까워질수록 크고 잦아진다.
    a = (t - T['approach_start']) / (T['approach_end'] - T['approach_start'])
    put(mix_wet, crawl[k % 3], t, 0.25 + 0.55 * a, -0.2 + 0.4 * (k % 2))
    t += 0.62 - 0.22 * a
    k += 1
put(mix_dry, load('Entity_Alert'), T['approach_end'] - 1.2, 0.55)
# 들킨 뒤: 기는 소리가 빨라지며 달려든다.
put(mix_dry, load('Stinger_CloseCall'), T['rush_start'], 0.8)
t = T['rush_start'] + 0.1
k = 0
while t < T['capture_start']:
    put(mix_wet, crawl[k % 3], t, 0.75, -0.2 + 0.4 * (k % 2))
    t += 0.2
    k += 1
put(mix_dry, load('Player_Breath_Scared')[:int(1.4 * SR)], T['rush_start'] + 0.2, 0.5)
# 콜드 오픈 끝의 짧은 컷: 낮은 쿵 하나와 들숨.
put(mix_dry, sub_hit(1.6, 70, 40, 0.8, decay=0.35), T['hook'], 0.45)
put(mix_dry, load('Player_Gasp')[:int(0.9 * SR)], T['hook'] + 0.02, 0.35)
put(mix_dry, load('Entity_Grab')[:int(1.2 * SR)], T['capture_start'], 1.0)
put(mix_dry, fade(load('Player_Gasp')[:int(2.4 * SR)], 0, 0.8), T['wake'] + 0.25, 0.7)
put(mix_wet, load('Paper_Turn_1'), T['booth'] + 0.4, 0.35, 0.2)
put(mix_wet, load('Ballast_Tick'), T['meter'] + 0.5, 0.5, -0.3)
put(mix_wet, load('Settle_Creak_1'), T['stair'] + 1.2, 0.45, 0.3)

# 화면을 끊는 저역과 귀울림(게임의 포획 끊기와 같은 재료)
cut_hit = sub_hit(2.2, 95, 38, 0.9, decay=0.55)
put(mix_dry, cut_hit, T['cut'], 0.5)
ring_t = np.arange(int(2.2 * SR)) / SR
ring = (np.sin(2 * np.pi * 6120 * ring_t) * 0.020 + np.sin(2 * np.pi * 6187 * ring_t) * 0.016)
ring *= np.exp(-ring_t / 0.9) * np.minimum(1, ring_t / 0.1)
put(mix_dry, ring.astype(np.float32), T['cut'] + 0.04, 1.2)

# --- 음악 -------------------------------------------------------------------
# 입주 저녁의 피아노. 느리고 조금 틀어져 있다.
A3, C4, D4, E4, G3, B3 = 220.0, 261.63, 293.66, 329.63, 196.0, 246.94
motif = [(0.0, E4, 0.55), (1.1, C4, 0.45), (2.2, D4, 0.5), (3.6, A3, 0.6),
         (5.2, E4, 0.5), (6.3, C4, 0.42), (7.4, B3, 0.48), (9.0, A3, 0.55),
         (10.6, G3, 0.5), (11.8, A3, 0.42)]
for i, (dt, f, v) in enumerate(motif):
    note = piano_note(f, 5.5, v, detune_cents=5.0 + 2.0 * (i % 3), brightness=0.7, seed=i)
    put(music, note, T['day_start'] + 0.4 + dt, 0.30, -0.25 + 0.5 * ((i * 7) % 5) / 4)
# 밤의 낮은 음. 자막이 뜰 때마다 하나씩.
for i, key in enumerate(['night_card', 'stair_card', 'booth_card', 'quiet_card']):
    f = [55.0, 51.91, 55.0, 46.25][i]
    put(music, piano_note(f, 7.0, 0.75, detune_cents=9.0, brightness=0.5, seed=20 + i), T[key], 0.36)
    put(music, piano_note(f * 2.0, 6.0, 0.35, detune_cents=12.0, brightness=0.4, seed=40 + i), T[key] + 0.02, 0.18)

# 건물 드론: 밤이 시작되면 깔리고 추격 직전까지 서서히 부푼다.
d_len = T['cut'] - T['night_start']
dr = drone(d_len, 55.0, 1.0)
env = np.linspace(0.25, 1.0, len(dr)) ** 1.6
dr *= env[:, None]
put(music, fade(dr, 1.5, 0.05), T['night_start'], 0.14)

# 추격: 올라가는 소리가 포획 순간 꼭대기에 닿고 끊긴다.
r_len = T['cut'] - (T['approach_end'] - 2.0)
rs = riser(r_len, 1.0)
put(music, pan(rs), T['approach_end'] - 2.0, 0.34)

# 제목: 저역 타격과 틀어진 화음, 긴 꼬리.
put(music, sub_hit(5.0, 80, 34, 1.0, decay=1.4), T['title'], 0.6)
for i, f in enumerate([55.0, 58.27, 110.0, 164.81, 233.08]):
    put(music, piano_note(f, 8.0, 0.8, detune_cents=10.0, brightness=0.8, seed=60 + i), T['title'] + 0.01 * i, 0.22,
        -0.4 + 0.2 * i)

# --- 공간과 합치기 ------------------------------------------------------------
ir = reverb_ir(2.2)
wet = apply_reverb(mix_wet, ir, 0.32)[:N]
music_wet = apply_reverb(music, reverb_ir(3.4, bright=4200, seed=9), 0.38)[:N]
out = mix_dry + wet + music_wet * 0.9

# 끊긴 뒤의 정적은 진짜 정적이어야 한다: 암전 구간의 공간음은 이미 들어가 있지 않다.
# 첫 1.5초 음량을 조금 올리고 전체를 -14 LUFS 근처로 맞춘다(대략 RMS 기준).
out = highpass(out, 24, 2)
rms = np.sqrt(np.mean(out ** 2))
out *= 10 ** (-17.5 / 20) / (rms + 1e-9)
# -3dBFS 위만 부드럽게 눌러 타격이 찢어지지 않게 한다.
knee = 10 ** (-3 / 20)
mag = np.abs(out)
over = mag > knee
out[over] = np.sign(out[over]) * (knee + (1 - knee) * np.tanh((mag[over] - knee) / (1 - knee)))
out *= 0.97
out = out[:int(LENGTH * SR)]
write_wav(os.path.join(HERE, 'trailer_audio.wav'), out)
print('audio', LENGTH, 'peak', float(np.max(np.abs(out))), 'rms_db', float(20 * np.log10(np.sqrt(np.mean(out ** 2)))))
