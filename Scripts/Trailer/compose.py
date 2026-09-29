"""게임에서 쓰는 환경음과 효과음을 영상의 동작에 맞춰 편집한다. 음악과 나레이션은 없다."""
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


def put(name, start, gain, duration=None, pan=0, fade=0.04, lowpass=None):
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
    MIX[i:i+n] += np.column_stack((x[:n] * np.cos(angle), x[:n] * np.sin(angle))) * gain


def write(path, data):
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.clip(data, -1, 1) * 32767).astype('<i2').tobytes())


put('Bed_Corridor', 0, 0.34, duration=4, fade=0.2)
put('Bed_City_Night', 4, 0.46, duration=7, fade=0.3)
put('Bed_Corridor', T['night'], 0.21, duration=T['cut']-T['night'], fade=0.3, lowpass=2400)
put('Hum_Machine', T['night'], 0.045, duration=T['cut']-T['night'], fade=0.7, lowpass=800)
put('Entity_KnockTriple_Muffled', T['knock'], 0.85)
put('Ballast_Tick', T['meter']+0.4, 0.4, pan=-0.2)
put('Settle_Creak_1', T['booth']+0.6, 0.25, pan=0.25)
put('Player_Breath_Scared', T['walk'], 0.19, duration=3.1, fade=0.2)
t = T['approach']+0.5
index = 0
while t < T['cut']-0.3:
    progress = (t-T['approach'])/(T['cut']-T['approach'])
    put(f'Entity_CrawlStep_{index%3}', t, 0.24+0.36*progress, pan=0.1 if index%2 else -0.1)
    t += 0.65-0.3*progress
    index += 1
put('Player_Breath_Scared', T['rush'], 0.25, duration=1.0)
put('Entity_Grab', T['grab'], 0.65, duration=0.2, fade=0.015)
put('Bed_Corridor', 32.2, 0.12, duration=1.6, fade=0.18)
put('Bed_Corridor', T['title'], 0.065, duration=EDL['length']-T['title'], fade=0.4, lowpass=1800)
put('Entity_KnockTriple_Muffled', T['last_knock'], 0.45, duration=1.8, fade=0.03)
# 측정 전에 클리핑을 막고, 두 번 측정하는 loudnorm으로 실제 통합 음량을 맞춘다.
peak = float(np.max(np.abs(MIX)))
if peak > 0.89:
    MIX *= 0.89/peak
raw = HERE / 'trailer_mix.wav'
write(raw, MIX)
base = ['ffmpeg', '-y', '-hide_banner', '-i', str(raw)]
result = subprocess.run(base+['-af','loudnorm=I=-18:TP=-2:LRA=12:print_format=json','-f','null','-'], capture_output=True, text=True, check=True)
report = json.JSONDecoder().raw_decode(result.stderr[result.stderr.rfind('{'):])[0]
(HERE / 'audio-loudness.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
params = ':'.join([f'measured_I={report["input_i"]}', f'measured_TP={report["input_tp"]}', f'measured_LRA={report["input_lra"]}', f'measured_thresh={report["input_thresh"]}', f'offset={report["target_offset"]}'])
subprocess.run(base+['-af',f'loudnorm=I=-18:TP=-2:LRA=12:{params}:linear=true','-ar',str(SR),'-c:a','pcm_s16le',str(HERE/'trailer_audio.wav')], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, check=True)
print('TRAILER_AUDIO PASS 목표 -18 LUFS / 최대 -2 dBTP, 측정 기록 audio-loudness.json')