"""게임에서 쓰는 환경음과 효과음만으로 예고편 소리를 짠다. 음악과 나레이션은 없다.

조용한 곳과 큰 소리의 차이가 공포를 만든다. 암전 구간은 비워 두고, 걸음과 노크,
붙잡히는 소리 뒤의 정적에 무게를 둔다. 마지막 노크는 게임의 가족 노크(둘, 쉬고, 하나)다.

    python Scripts/Trailer/assemble.py --marks
    python Scripts/Trailer/compose.py
"""
from pathlib import Path
import json
import subprocess
import wave
import numpy as np
from scipy.signal import butter, sosfilt

ROOT = Path(__file__).resolve().parents[2]
HERE = ROOT / 'Saved/Trailer'
AUDIO = ROOT / 'Content/SourceArt/Audio'
SR = 48000
EDL = json.loads((HERE / 'edl.json').read_text(encoding='utf-8'))
T = EDL['marks']
MIX = np.zeros((round(EDL['length'] * SR), 2), dtype=np.float32)


def load(name):
    with wave.open(str(AUDIO / f'{name}.wav')) as w:
        if w.getsampwidth() != 2 or w.getframerate() != SR:
            raise ValueError(f'48kHz PCM16 파일이 필요합니다: {name}')
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32) / 32768
        return x.reshape(-1, w.getnchannels()).mean(axis=1)


def put(name, start, gain, duration=None, pan=0.0, fade=0.04, lowpass=None):
    x = load(name)
    if duration is not None:
        n = round(duration * SR)
        x = np.tile(x, int(np.ceil(n / len(x))))[:n]
    if lowpass:
        x = sosfilt(butter(3, lowpass, fs=SR, output='sos'), x)
    k = min(round(fade * SR), len(x) // 2)
    if k:
        x[:k] *= np.linspace(0, 1, k)
        x[-k:] *= np.linspace(1, 0, k)
    i = round(start * SR)
    n = min(len(x), len(MIX) - i)
    angle = (pan + 1) * np.pi / 4
    MIX[i:i + n] += np.column_stack((x[:n] * np.cos(angle), x[:n] * np.sin(angle))) * gain


def steps(start, end, interval, gain, surface='Concrete', count=5, pan=0.0):
    t, index = start, 0
    while t < end:
        put(f'Foot_{surface}_{index % count}', t, gain * (0.9 + 0.2 * ((index * 7) % 3) / 2), pan=pan)
        t += interval
        index += 1


def write(path, data):
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.clip(data, -1, 1) * 32767).astype('<i2').tobytes())


# 새벽 방. 거의 들리지 않는 방 소리 위로 천장의 노크가 먼저 온다.
put('Bed_Corridor', 0.0, 0.07, duration=T['day'] - 0.9, fade=0.6, lowpass=900)
put('Entity_KnockTriple_Muffled', T['first_knock'], 0.95)
put('Settle_Creak_0', T['wake'] + 3.6, 0.22, pan=0.15, lowpass=2500)

# 전날 저녁의 빌라. 복도를 걷는 발소리, 골목의 도시 소리.
put('Bed_Corridor', T['day'], 0.30, duration=T['alley'] - T['day'], fade=0.25)
steps(T['day'] + 0.2, T['door'] - 0.3, 0.52, 0.17)
put('Settle_Creak_1', T['door'] + 1.8, 0.12, pan=-0.3)
put('Bed_City_Night', T['alley'], 0.44, duration=T['meter'] - T['alley'], fade=0.35)
put('Wind_Gap', T['alley'] + 0.5, 0.10, duration=3.5, fade=0.8)

# 불 꺼진 공용부. 안정기와 기계 소리만 남는다.
put('Ballast_Tick', T['meter'] + 0.4, 0.34, pan=-0.2)
put('Hum_Machine', T['meter'], 0.07, duration=T['night'] - T['meter'] - 0.9, fade=0.4, lowpass=900)
put('Ballast_Tick', T['booth'] + 1.2, 0.18, pan=0.25)

# 새벽 네 시 반의 복도.
put('Bed_Corridor', T['night'], 0.18, duration=T['cut'] - T['night'], fade=0.4, lowpass=2200)
put('Hum_Machine', T['night'], 0.05, duration=T['cut'] - T['night'], fade=0.6, lowpass=700)
steps(T['night'] + 0.3, T['stair'] - 0.4, 0.74, 0.09)
put('Player_Breath_Scared', T['night'] + 1.0, 0.13, duration=4.8, fade=0.5)
put('Entity_KnockTriple', T['night'] + 3.4, 0.30, pan=-0.55, lowpass=3000)
for offset, pan in ((0.6, 0.3), (1.5, 0.2), (2.6, 0.35)):
    put(f'Entity_CrawlStep_{int(offset * 10) % 3}', T['stair'] + offset, 0.2, pan=pan)
put('Entity_Breath_Loop', T['stair'], 0.10, duration=T['approach'] - T['stair'] + 1.0, fade=0.5)

# 다가오는 걸음은 점점 잦아지고 커진다. 붙잡히는 소리 뒤는 완전한 정적이다.
t, index = T['approach'] + 0.4, 0
while t < T['cut'] - 0.25:
    progress = (t - T['approach']) / (T['cut'] - T['approach'])
    put(f'Entity_CrawlStep_{index % 3}', t, 0.22 + 0.4 * progress, pan=0.12 if index % 2 else -0.12)
    t += 0.68 - 0.34 * progress
    index += 1
put('Player_Breath_Scared', T['approach'] + 2.0, 0.16, duration=T['rush'] - T['approach'] - 2.0, fade=0.3)
put('Player_Breath_Scared', T['rush'], 0.27, duration=T['cut'] - T['rush'], fade=0.05)
put('Entity_Grab', T['grab'], 0.72, duration=T['cut'] - T['grab'], fade=0.01)

# 제목 뒤로 위에서 둘, 쉬고, 하나.
put('Bed_Corridor', T['title'], 0.045, duration=EDL['length'] - T['title'], fade=0.8, lowpass=1200)
for offset, name in ((0.0, 'Knock_Plaster_0'), (0.42, 'Knock_Plaster_1'), (1.34, 'Knock_Plaster_2')):
    put(name, T['answer'] + offset, 0.58, lowpass=1600)

peak = float(np.max(np.abs(MIX)))
if peak > 0.89:
    MIX *= 0.89 / peak
raw = HERE / 'trailer_mix.wav'
write(raw, MIX)
base = ['ffmpeg', '-y', '-hide_banner', '-i', str(raw)]
result = subprocess.run(base + ['-af', 'loudnorm=I=-19:TP=-1.5:LRA=14:print_format=json', '-f', 'null', '-'],
                        capture_output=True, text=True, check=True)
report = json.JSONDecoder().raw_decode(result.stderr[result.stderr.rfind('{'):])[0]
(HERE / 'audio-loudness.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
params = ':'.join([f'measured_I={report["input_i"]}', f'measured_TP={report["input_tp"]}',
                   f'measured_LRA={report["input_lra"]}', f'measured_thresh={report["input_thresh"]}',
                   f'offset={report["target_offset"]}'])
subprocess.run(base + ['-af', f'loudnorm=I=-19:TP=-1.5:LRA=14:{params}:linear=true', '-ar', str(SR),
                       '-c:a', 'pcm_s16le', str(HERE / 'trailer_audio.wav')],
               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, check=True)
print('TRAILER_AUDIO PASS 목표 -19 LUFS / 최대 -1.5 dBTP, 측정 기록 audio-loudness.json')
