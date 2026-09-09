# -*- coding: utf-8 -*-
"""-IGAudioRenderProbe가 구운 WAV를 스펙트로그램 시트와 표로 만든다.

소리는 런타임 합성이라 파일이 없고, 그래서 귀 없이는 확인할 길이 없었다. 이
스크립트는 Saved/AudioProbe/*.wav를 읽어 큐마다 피크·RMS·길이·스펙트럼 중심을
재고, 스펙트로그램 타일을 한 장에 모은다. 파형이 「삐」인지 「쿵」인지, 꼬리가
있는지, 클리핑했는지는 그림에서 바로 보인다.

    python Scripts/render_audio_probe_sheet.py            # 시트와 표
    python Scripts/render_audio_probe_sheet.py --check    # 피크 ≥ 0dBFS면 실패
"""
from __future__ import annotations

import argparse
import math
import pathlib
import struct
import sys
import wave

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
PROBE_DIR = ROOT / "Saved" / "AudioProbe"


def read_wav(path: pathlib.Path):
    with wave.open(str(path), "rb") as handle:
        rate = handle.getframerate()
        frames = handle.readframes(handle.getnframes())
    samples = np.frombuffer(frames, dtype=np.int16).astype(np.float32) / 32768.0
    return rate, samples


def spectrogram(samples: np.ndarray, rate: int, width: int, height: int) -> np.ndarray:
    """로그 주파수축 스펙트로그램. 30Hz~16kHz, dB 스케일."""
    window = 1024
    hop = max(1, len(samples) // width)
    frames = []
    hann = np.hanning(window)
    for column in range(width):
        start = column * hop
        chunk = samples[start:start + window]
        if len(chunk) < window:
            chunk = np.pad(chunk, (0, window - len(chunk)))
        spectrum = np.abs(np.fft.rfft(chunk * hann)) / (window / 2)
        frames.append(spectrum)
    magnitude = np.array(frames).T  # bins × columns
    freqs = np.fft.rfftfreq(window, 1.0 / rate)
    log_edges = np.geomspace(30.0, 16000.0, height + 1)
    rows = []
    for row in range(height):
        low, high = log_edges[row], log_edges[row + 1]
        mask = (freqs >= low) & (freqs < high)
        if not mask.any():
            index = int(np.argmin(np.abs(freqs - low)))
            mask = np.zeros_like(freqs, dtype=bool)
            mask[index] = True
        rows.append(magnitude[mask].max(axis=0))
    grid = np.array(rows[::-1])  # 위가 고역
    db = 20.0 * np.log10(np.maximum(grid, 1e-6))
    return np.clip((db + 84.0) / 84.0, 0.0, 1.0)


def colorize(norm: np.ndarray) -> Image.Image:
    # 검정 → 진한 자주 → 주황 → 흰색
    stops = np.array([[0, 0, 0], [70, 10, 90], [200, 60, 30], [255, 200, 90], [255, 255, 255]], dtype=np.float32)
    positions = np.linspace(0.0, 1.0, len(stops))
    rgb = np.zeros(norm.shape + (3,), dtype=np.float32)
    for channel in range(3):
        rgb[..., channel] = np.interp(norm, positions, stops[:, channel])
    return Image.fromarray(rgb.astype(np.uint8), "RGB")


def stats(samples: np.ndarray, rate: int):
    peak = float(np.max(np.abs(samples))) if len(samples) else 0.0
    rms = float(np.sqrt(np.mean(samples ** 2))) if len(samples) else 0.0
    spectrum = np.abs(np.fft.rfft(samples * np.hanning(len(samples)))) if len(samples) else np.array([0.0])
    freqs = np.fft.rfftfreq(len(samples), 1.0 / rate) if len(samples) else np.array([0.0])
    centroid = float((spectrum * freqs).sum() / max(spectrum.sum(), 1e-9))
    clipped = int(np.sum(np.abs(samples) >= 0.999))
    return peak, rms, centroid, clipped


def db(value: float) -> float:
    return 20.0 * math.log10(value) if value > 0.0 else -120.0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--out", default=str(PROBE_DIR / "sheet.png"))
    args = parser.parse_args()

    files = sorted(PROBE_DIR.glob("*.wav"))
    if not files:
        print("AUDIO_PROBE FAIL: no wav under", PROBE_DIR)
        return 1
    tile_w, tile_h, label_h = 320, 150, 22
    columns = 4
    rows = math.ceil(len(files) / columns)
    sheet = Image.new("RGB", (columns * tile_w, rows * (tile_h + label_h)), (12, 12, 14))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 14)
    except OSError:
        font = ImageFont.load_default()
    findings = 0
    print("%-24s %7s %7s %9s %7s %6s" % ("cue", "peak", "rms", "centroid", "sec", "clip"))
    for index, path in enumerate(files):
        rate, samples = read_wav(path)
        peak, rms, centroid, clipped = stats(samples, rate)
        seconds = len(samples) / rate
        print("%-24s %6.1fdB %6.1fdB %8.0fHz %6.2f %6d" % (
            path.stem, db(peak), db(rms), centroid, seconds, clipped))
        if clipped > 0:
            findings += 1
        tile = colorize(spectrogram(samples, rate, tile_w, tile_h))
        x = (index % columns) * tile_w
        y = (index // columns) * (tile_h + label_h)
        sheet.paste(tile, (x, y + label_h))
        draw.text((x + 4, y + 3), "%s  %.1f dB  %.2fs" % (path.stem, db(peak), seconds),
                  fill=(230, 230, 230), font=font)
    sheet.save(args.out)
    print("AUDIO_PROBE sheet ->", args.out)
    if args.check and findings:
        print("AUDIO_PROBE FAIL: %d cue(s) clipped" % findings)
        return 1
    print("AUDIO_PROBE PASS cues=%d" % len(files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
