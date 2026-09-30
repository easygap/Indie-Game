"""설치된 게임의 CPU 프레임과 비동기 GPU 결과를 각각 검사하고 요약한다."""

import argparse
import bisect
import csv
import hashlib
import itertools
import json
import math
from pathlib import Path
import statistics


CPU_COLUMNS = {
    "EngineFrame", "ElapsedSeconds", "FrameTime", "WorldSeconds", "Paused",
    "Process/CommittedMiB", "GPUMem/LocalUsedMB", "ScreenPercentage",
    "SecondaryScreenPercentage", "DynamicResolutionMode", "AntiAliasingMethod",
    "QualityLevel", "Width", "Height", "VSync", "FrameLimit", "Stage",
}
GPU_COLUMNS = {"SampleIndex", "ReceivedElapsedSeconds", "GPUTime", "Stage", "Disjoint"}
# 17번은 따로 시작하는 메모 촬영이다. 기본 경로는 18번 포획과 복귀를 끝으로 종료한다.
EXPECTED_STAGES = list(range(17)) + [18]


def read_samples(source, required):
    with source.open(encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        fields = reader.fieldnames or []
        if len(fields) != len(required) or set(fields) != required:
            raise ValueError(f"성능 원본의 열이 검사기와 다릅니다: {source.name}")
        rows = [{key: float(value) for key, value in row.items()} for row in reader]
    if not rows or any(not math.isfinite(value) for row in rows for value in row.values()):
        raise ValueError(f"성능 원본이 비었거나 유효하지 않은 숫자가 있습니다: {source.name}")
    if any(not row["Stage"].is_integer() or row["Stage"] < -1 for row in rows):
        raise ValueError("장면 번호가 유효하지 않습니다.")
    return rows


def route_samples(rows):
    first = next((index for index, row in enumerate(rows) if row["Stage"] >= 0), len(rows))
    selected = rows[first:]
    # 중간에 -1로 돌아간 프레임도 버리지 않고 경로 오류로 처리한다.
    transitions = [int(stage) for stage, _ in itertools.groupby(row["Stage"] for row in selected)]
    if transitions != EXPECTED_STAGES:
        raise ValueError(f"밤 장면 경로가 빠지거나 순서가 바뀌었습니다: {transitions}")
    return selected


def describe(values):
    if not values:
        return None
    return {"median": round(statistics.median(values), 3),
            "p95": round(statistics.quantiles(values, n=100, method="inclusive")[94], 3)
            if len(values) > 1 else round(values[0], 3),
            "max": round(max(values), 3), "samples": len(values)}


def summarize(source, output, warmup_seconds, width, height, quality, aa, target_fps):
    rows = read_samples(source, CPU_COLUMNS)
    metadata_path = source.with_suffix(".csv.txt")
    metadata = dict(line.split("=", 1) for line in metadata_path.read_text(
        encoding="utf-8-sig").splitlines() if "=" in line)
    if int(metadata["frames"]) != len(rows):
        raise ValueError("기록된 프레임 수와 원본 행 수가 다릅니다.")
    previous_time = 0
    previous_frame = None
    for row in rows:
        if (row["ElapsedSeconds"] <= previous_time or row["FrameTime"] <= 0
                or not row["EngineFrame"].is_integer()
                or (previous_frame is not None and row["EngineFrame"] != previous_frame + 1)):
            raise ValueError("프레임이 누락되거나 순서가 뒤집혔습니다.")
        # CSV의 소수점 반올림 오차만 허용한다. 행을 버리면 경과 시간과 맞지 않는다.
        if abs(row["FrameTime"] - (row["ElapsedSeconds"] - previous_time) * 1000) > .002:
            raise ValueError("프레임 시간과 실제 경과 시간이 맞지 않습니다.")
        previous_time = row["ElapsedSeconds"]
        previous_frame = row["EngineFrame"]
    selected = route_samples(rows)
    if selected[0]["ElapsedSeconds"] < warmup_seconds:
        raise ValueError("실제 워밍업 시간이 기준보다 짧습니다.")
    if len(selected) < 100 or selected[-1]["ElapsedSeconds"] - selected[0]["ElapsedSeconds"] < 20:
        raise ValueError("검사할 구간이 20초 또는 100프레임보다 짧습니다.")
    expected = {"Width": width, "Height": height, "ScreenPercentage": 100,
                "SecondaryScreenPercentage": 100, "DynamicResolutionMode": 0,
                "QualityLevel": quality, "AntiAliasingMethod": aa, "VSync": 0, "FrameLimit": 0}
    for row in selected:
        if any(abs(row[key] - value) > 0.01 for key, value in expected.items()):
            raise ValueError(f"측정 중 화면 설정이 기준과 다릅니다: {row}")
        if row["Paused"] != 0:
            raise ValueError("일시 정지 프레임이 섞였습니다.")

    gpu_path = source.with_suffix(".gpu.csv")
    gpu_rows = read_samples(gpu_path, GPU_COLUMNS)
    if (metadata.get("gpu_timing") != "history_queue"
            or metadata.get("gpu_stage") != "receipt_time"
            or metadata.get("gpu_tail_waited") != "0"
            or int(metadata["gpu_samples"]) != len(gpu_rows)):
        raise ValueError("GPU 기록 방식 또는 원본의 표본 수가 메타데이터와 다릅니다.")
    elapsed = [row["ElapsedSeconds"] for row in rows]
    previous_time = -1
    for index, row in enumerate(gpu_rows):
        received = row["ReceivedElapsedSeconds"]
        if (row["SampleIndex"] != index or received < previous_time or received < 0
                or row["GPUTime"] < 0 or row["Disjoint"] not in (0, 1)):
            raise ValueError("GPU 결과가 누락되거나 순서·시간·상태가 유효하지 않습니다.")
        # 같은 EndFrame에서 여러 GPU 결과가 도착할 수 있다. CPU 프레임과 1:1 대응하지 않는다.
        cpu_index = max(0, bisect.bisect_right(elapsed, received + .000001) - 1)
        if row["Stage"] != rows[cpu_index]["Stage"]:
            raise ValueError("GPU 결과의 수신 시각과 장면 번호가 맞지 않습니다.")
        previous_time = received
    gpu_selected = route_samples(gpu_rows)
    if any(row["Disjoint"] or row["GPUTime"] <= 0 for row in gpu_selected):
        raise ValueError("측정 구간의 GPU 결과가 누락됐거나 시간 질의가 유효하지 않습니다.")
    coverage = len(gpu_selected) / len(selected)
    # 비동기 결과의 시작·끝 경계는 몇 프레임 어긋날 수 있지만 큰 누락은 허용하지 않는다.
    if not .95 <= coverage <= 1.05:
        raise ValueError(f"GPU 표본 수가 CPU 측정 구간과 크게 다릅니다: {coverage:.3f}")
    times = [row["FrameTime"] for row in selected]
    gpu_times = [row["GPUTime"] for row in gpu_selected]
    slow_count = math.ceil(len(times) * .01)
    low_fps = 1000 / statistics.mean(sorted(times, reverse=True)[:slow_count])
    metrics = {column: describe([row[column] for row in selected if row[column] >= 0])
               for column in ("FrameTime", "Process/CommittedMiB", "GPUMem/LocalUsedMB")}
    metrics["GPUTime"] = describe(gpu_times)
    frame_p95 = statistics.quantiles(times, n=100, method="inclusive")[94]
    max_ms = 50 if target_fps == 60 else 100
    gate = frame_p95 <= 1000 / target_fps and low_fps >= (50 if target_fps == 60 else 25) and max(times) <= max_ms
    stage_metrics = {}
    for stage in EXPECTED_STAGES:
        cpu_stage = [row for row in selected if row["Stage"] == stage]
        gpu_stage = [row["GPUTime"] for row in gpu_selected if row["Stage"] == stage]
        stage_metrics[str(stage)] = {
            "FrameTime": describe([row["FrameTime"] for row in cpu_stage]),
            "GPUTimeByReceiptStage": describe(gpu_stage),
            "cpu_duration_seconds": round(sum(row["FrameTime"] for row in cpu_stage) / 1000, 6),
        }
    summary = {
        "scope": "현재 장비의 Shipping 오프스크린 자동 밤 장면 경로. 실제 화면 출력, 여러 장비, 사람의 완주 검수는 별도로 필요하다.",
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "gpu_source_sha256": hashlib.sha256(gpu_path.read_bytes()).hexdigest(),
        "metadata_sha256": hashlib.sha256(metadata_path.read_bytes()).hexdigest(),
        "hardware": metadata, "settings": expected, "target_fps": target_fps,
        "captured_frames": len(rows), "warmup_seconds": warmup_seconds,
        "analyzed_frames": len(selected),
        "analyzed_seconds": round(sum(times) / 1000, 3),
        "stage_samples": {str(stage): sum(row["Stage"] == stage for row in selected) for stage in EXPECTED_STAGES},
        "stage_metrics": stage_metrics,
        "captured_gpu_samples": len(gpu_rows), "analyzed_gpu_samples": len(gpu_selected),
        "gpu_sample_coverage": round(coverage, 6),
        "gpu_disjoint_warmup_events": sum(row["Disjoint"] for row in gpu_rows if row["Stage"] < 0),
        "gpu_disjoint_measured_events": 0,
        "metrics": metrics, "one_percent_low_fps": round(low_fps, 3),
        "frames_over_50ms": sum(time > 50 for time in times),
        "frames_over_100ms": sum(time > 100 for time in times),
        "local_route_frame_gate_passed": gate,
        "shipping_release_certified": False,
        "notes": "GPU 시간은 큐에서 한 번씩 꺼낸 결과다. 장면 번호와 경과 시간은 수신 시점이므로 렌더 시점과 지연 차이가 있다. 종료 때 GPU를 기다리지 않으며 마지막 미도착 결과는 제외된다. 메모리는 초당 한 번의 표본이고 최초 로딩·워밍업·순간 멈춤은 원본에 보존한다.",
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--warmup-seconds", type=float, default=120)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--quality", type=int, choices=[1, 2], default=2)
    parser.add_argument("--aa", type=int, choices=[2, 4], default=2)
    parser.add_argument("--target-fps", type=int, choices=[30, 60], default=60)
    args = parser.parse_args()
    if not math.isfinite(args.warmup_seconds) or args.warmup_seconds < 0:
        parser.error("워밍업은 유효한 0 이상의 숫자여야 합니다.")
    summarize(args.source, args.output, args.warmup_seconds, args.width, args.height, args.quality, args.aa, args.target_fps)

