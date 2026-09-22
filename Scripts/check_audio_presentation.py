"""연출 검사가 녹음한 실제 믹서 출력의 무음·클리핑을 확인한다."""
from __future__ import annotations

import array
import json
import math
from pathlib import Path
import sys
import wave


def inspect(path: Path) -> dict:
    with wave.open(str(path), "rb") as source:
        if source.getsampwidth() != 2:
            raise ValueError("16비트 PCM 녹음이 필요합니다.")
        channels = source.getnchannels()
        rate = source.getframerate()
        frames = source.getnframes()
        samples = array.array("h", source.readframes(frames))
    if sys.byteorder != "little":
        samples.byteswap()
    if not samples:
        raise ValueError("녹음에 샘플이 없습니다.")
    peak = max(abs(sample) for sample in samples) / 32768
    rms = math.sqrt(sum(sample * sample for sample in samples) / len(samples)) / 32768
    clipped = sum(abs(sample) >= 32767 for sample in samples)
    result = {
        "file": path.name, "seconds": round(frames / rate, 3),
        "sample_rate": rate, "channels": channels,
        "peak_dbfs": round(20 * math.log10(max(peak, 1e-10)), 3),
        "rms_dbfs": round(20 * math.log10(max(rms, 1e-10)), 3),
        "clipped_samples": clipped,
    }
    # LUFS 인수 검사는 별도다. 여기서는 오디오 장치가 실제 신호를 냈는지 검사한다.
    result["passed"] = frames / rate >= 5 and rms > 0.0001 and clipped == 0
    return result


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "Saved/AudioPresentation/presentation-mix.wav"
    report = inspect(path)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    raise SystemExit(0 if report["passed"] else 1)
