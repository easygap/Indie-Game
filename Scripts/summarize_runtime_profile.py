"""UE CSV에서 초기 프레임을 제외한 원본 수치와 요약을 보존한다."""

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--warmup", type=int, default=500)
args = parser.parse_args()
if args.warmup < 0:
    parser.error("제외할 프레임 수는 0 이상이어야 합니다.")
csv.field_size_limit(16 * 1024 * 1024)
columns = ["FrameTime", "GPUTime", "GameThreadTime", "RenderThreadTime",
           "RHI/DrawCalls", "RHI/PrimitivesDrawn", "GPUMem/LocalUsedMB"]
rows = []
with args.source.open(encoding="utf-8-sig", newline="") as handle:
    reader = csv.DictReader(handle)
    if not reader.fieldnames or not all(column in reader.fieldnames for column in columns):
        raise ValueError("필수 측정 열이 없습니다. -csvGpuStats를 사용하세요.")
    for index, row in enumerate(reader):
        # UE가 끝에 붙이는 열 이름과 메타데이터만 제외한다. 손상된 프레임을
        # 조용히 버리면 긴 프레임이 요약에서 빠져 성능이 좋게 보일 수 있다.
        if (all(row.get(column) == column for column in columns)
                or row.get(reader.fieldnames[0]) == "[HasHeaderRowAtEnd]"):
            continue
        try:
            values = {column: float(row[column]) for column in columns}
        except (ValueError, TypeError) as error:
            raise ValueError(f"프레임 측정값을 읽을 수 없습니다: {index}") from error
        if not all(math.isfinite(value) for value in values.values()):
            raise ValueError(f"유효하지 않은 측정값: {index}")
        if values["FrameTime"] <= 0 or any(value < 0 for value in values.values()):
            raise ValueError(f"시간이나 개수가 음수이거나 프레임 시간이 0입니다: {index}")
        rows.append({"capture_frame": index, **values})
selected = rows[args.warmup:]
if len(selected) < 100:
    raise ValueError("초기 프레임을 제외한 유효 표본이 100개 미만입니다.")
args.output.mkdir(parents=True, exist_ok=True)
with (args.output / "frames.csv").open("w", encoding="utf-8", newline="") as handle:
    writer = csv.DictWriter(handle, fieldnames=["capture_frame", *columns])
    writer.writeheader()
    writer.writerows(selected)
summary = {
    "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
    "captured_frames": len(rows),
    "warmup_frames": args.warmup,
    "analyzed_frames": len(selected),
    "units": "시간은 ms, GPU 메모리는 MiB, 나머지는 개수",
    "metrics": {},
}
for column in columns:
    values = [row[column] for row in selected]
    summary["metrics"][column] = {
        "median": round(statistics.median(values), 3),
        "p95": round(statistics.quantiles(values, n=100, method="inclusive")[94], 3),
        "max": round(max(values), 3),
    }
summary["frames_over_33_33ms"] = sum(row["FrameTime"] > 1000 / 30 for row in selected)
summary["frames_over_50ms"] = sum(row["FrameTime"] > 50 for row in selected)
summary["frames_over_100ms"] = sum(row["FrameTime"] > 100 for row in selected)
# 출시 계약과 같은 산식: 가장 느린 1%의 평균 프레임 시간을 fps로 환산한다.
# 경계의 한 프레임을 버리지 않도록 표본 수를 올림한다.
slow_frame_count = max(1, math.ceil(len(selected) * 0.01))
slow_frames = sorted((row["FrameTime"] for row in selected), reverse=True)[:slow_frame_count]
summary["one_percent_low_fps"] = round(1000 / statistics.mean(slow_frames), 3)
summary["one_percent_low_sample_count"] = slow_frame_count
(args.output / "summary.json").write_text(
    json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
with zipfile.ZipFile(args.output / "full-profile.csv.zip", "w", zipfile.ZIP_DEFLATED) as archive:
    archive.write(args.source, "full-profile.csv")
print(json.dumps(summary, ensure_ascii=False, indent=2))
