"""트레일러 음악과 효과음 재료. 48kHz 스테레오 float32."""
import numpy as np
import wave
import os
from scipy.signal import butter, sosfilt, fftconvolve

SR = 48000
AUDIO_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "Content", "SourceArt", "Audio")
rng = np.random.default_rng(403)


def load(name):
    w = wave.open(os.path.join(AUDIO_DIR, name + '.wav'))
    raw = w.readframes(w.getnframes())
    x = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    if w.getnchannels() == 2:
        x = x.reshape(-1, 2).mean(axis=1)
    return x


def lowpass(x, hz, order=4):
    return sosfilt(butter(order, hz, 'low', fs=SR, output='sos'), x, axis=0)


def highpass(x, hz, order=4):
    return sosfilt(butter(order, hz, 'high', fs=SR, output='sos'), x, axis=0)


def bandpass(x, lo, hi, order=3):
    return sosfilt(butter(order, [lo, hi], 'band', fs=SR, output='sos'), x, axis=0)


def pan(mono, p=0.0):
    """p: -1 왼쪽 .. 1 오른쪽, 등전력."""
    a = (p + 1) * np.pi / 4
    return np.stack([mono * np.cos(a), mono * np.sin(a)], axis=1)


def env_adsr(n, a, d, s, r, sustain_len=None):
    a_n, d_n, r_n = int(a * SR), int(d * SR), int(r * SR)
    s_n = n - a_n - d_n - r_n if sustain_len is None else int(sustain_len * SR)
    s_n = max(s_n, 0)
    e = np.concatenate([
        np.linspace(0, 1, max(a_n, 1)),
        np.linspace(1, s, max(d_n, 1)),
        np.full(s_n, s),
        np.linspace(s, 0, max(r_n, 1)),
    ])
    if len(e) < n:
        e = np.pad(e, (0, n - len(e)))
    return e[:n]


def reverb_ir(seconds=2.4, pre=0.012, bright=5200, seed=7):
    """방 울림. 오른쪽과 왼쪽을 따로 만든 지수 감쇠 잡음."""
    r = np.random.default_rng(seed)
    n = int(seconds * SR)
    t = np.arange(n) / SR
    decay = np.exp(-6.9 * t / seconds)
    ir = r.standard_normal((n, 2)) * decay[:, None]
    ir = lowpass(ir, bright, 2)
    ir[:int(pre * SR)] = 0
    ir /= np.sqrt((ir ** 2).sum(axis=0, keepdims=True))
    return ir.astype(np.float32)


def apply_reverb(x, ir, wet=0.3):
    if x.ndim == 1:
        x = pan(x)
    out = np.zeros((len(x) + len(ir) - 1, 2), dtype=np.float32)
    for c in range(2):
        out[:, c] = fftconvolve(x[:, c], ir[:, c])
    dry = np.pad(x, ((0, len(ir) - 1), (0, 0)))
    return dry * (1 - wet) + out * wet


def piano_note(freq, seconds=6.0, velocity=0.8, detune_cents=4.0, brightness=1.0, seed=0):
    """조율이 조금 틀어진 업라이트. 한 음에 현 두 가닥을 어긋나게 두어 맥놀이가 난다."""
    r = np.random.default_rng(seed)
    n = int(seconds * SR)
    t = np.arange(n) / SR
    B = 0.00035
    out = np.zeros(n, dtype=np.float64)
    for string, cents in enumerate((-detune_cents / 2, detune_cents / 2)):
        f0 = freq * 2 ** (cents / 1200)
        for k in range(1, 16):
            fk = k * f0 * np.sqrt(1 + B * k * k)
            if fk > SR / 2 - 500:
                break
            amp = (1.0 / k ** (1.35 - 0.25 * brightness)) * (0.6 + 0.4 * r.random())
            tau = 3.2 / (1 + 0.45 * k) * (220 / max(freq, 60)) ** 0.35
            phase = r.random() * 2 * np.pi
            out += amp * np.exp(-t / tau) * np.sin(2 * np.pi * fk * t + phase)
    # 해머가 현을 때리는 짧은 소리
    click = r.standard_normal(int(0.012 * SR)) * np.exp(-np.arange(int(0.012 * SR)) / (0.0025 * SR))
    click = bandpass(click, 900, 5000)
    out[:len(click)] += click * 0.35 * brightness
    attack = np.minimum(1, t / 0.004)
    out *= attack
    out /= np.max(np.abs(out)) + 1e-9
    return (out * velocity).astype(np.float32)


def sub_hit(seconds=4.0, start_hz=92, end_hz=36, gain=1.0, decay=1.1):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    f = end_hz + (start_hz - end_hz) * np.exp(-t / 0.09)
    phase = 2 * np.pi * np.cumsum(f) / SR
    body = np.sin(phase) * np.exp(-t / decay)
    body = np.tanh(body * 1.8) / np.tanh(1.8)
    noise = lowpass(rng.standard_normal(n), 900) * np.exp(-t / 0.06) * 0.5
    return ((body + noise) * gain).astype(np.float32)


def riser(seconds=5.0, gain=0.5):
    """잡음과 음이 함께 올라간다. 끝에서 가장 크다."""
    n = int(seconds * SR)
    t = np.arange(n) / SR
    x = t / seconds
    noise = rng.standard_normal(n)
    # 짧은 창으로 대역을 올린다.
    out = np.zeros(n)
    hop = 2400
    for i in range(0, n, hop):
        seg = noise[i:i + hop * 2]
        c = 300 * (20 ** x[min(i, n - 1)])
        lo, hi = max(40, c * 0.6), min(SR / 2 - 100, c * 1.6)
        f = bandpass(seg, lo, hi, 2)
        w = np.hanning(len(f))
        out[i:i + len(f)] += f * w
    tone_f = 110 * 2 ** (2.2 * x ** 1.6)
    tone = np.sin(2 * np.pi * np.cumsum(tone_f) / SR) * 0.35
    shimmer = np.sin(2 * np.pi * np.cumsum(tone_f * 2.003) / SR) * 0.12
    env = x ** 2.2
    y = (out * 0.5 + tone + shimmer) * env
    return (y / (np.max(np.abs(y)) + 1e-9) * gain).astype(np.float32)


def drone(seconds, base=55.0, gain=0.3, seed=11):
    """건물이 웅웅거리는 소리. 전원 험과 낮은 공기."""
    r = np.random.default_rng(seed)
    n = int(seconds * SR)
    t = np.arange(n) / SR
    lfo = 0.5 + 0.5 * np.sin(2 * np.pi * 0.07 * t + r.random() * 6)
    hum = (np.sin(2 * np.pi * base * t) * 0.5
           + np.sin(2 * np.pi * base * 2.003 * t) * 0.25
           + np.sin(2 * np.pi * base * 3.0 * t + 1.0) * 0.08)
    air = lowpass(r.standard_normal(n), 260, 2) * 3.0
    air *= 0.6 + 0.4 * lfo
    y = hum * (0.7 + 0.3 * lfo) + air * 0.25
    L = y + lowpass(r.standard_normal(n), 180, 2) * 0.4
    R = y + lowpass(r.standard_normal(n), 180, 2) * 0.4
    st = np.stack([L, R], axis=1)
    return (st / (np.max(np.abs(st)) + 1e-9) * gain).astype(np.float32)


def write_wav(path, stereo):
    x = np.clip(stereo, -1, 1)
    data = (x * 32767).astype(np.int16)
    w = wave.open(path, 'wb')
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(data.tobytes())
    w.close()
