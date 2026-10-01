"""게임 PID에 묶인 화면 출력과 Windows GPU 메모리 원본을 확인한다."""

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def describe(values):
    if not values:
        return None
    return {"samples": len(values), "median": round(statistics.median(values), 3),
            "p95": round(statistics.quantiles(values, n=100, method="inclusive")[94], 3)
            if len(values) > 1 else round(values[0], 3), "max": round(max(values), 3)}


def finite(value, label, minimum=0):
    result = float(value)
    if not math.isfinite(result) or result < minimum:
        raise ValueError(f"유효하지 않은 {label}: {value}")
    return result


def analyze_present(rows, process_id, executable_name):
    if not rows:
        raise ValueError("화면 출력 원본이 비었습니다.")
    expected = {"Application", "ProcessID", "SwapChainAddress", "Runtime", "Dropped",
                "msInPresentAPI", "msUntilDisplayed", "msBetweenPresents", "msBetweenDisplayChange", "PresentMode"}
    if not expected <= rows[0].keys():
        raise ValueError("PresentMon 1.x 화면 출력 열이 없습니다.")
    result = {"captured_frames": len(rows), "displayed_frames": 0, "dropped_frames": 0}
    intervals, displayed, present_api, display_latency = [], [], [], []
    modes, swapchains = set(), set()
    previous = {}
    clock = "QPCTime" if "QPCTime" in rows[0] else "TimeInSeconds"
    for row in rows:
        if int(row["ProcessID"]) != process_id or row["Application"].lower() != executable_name.lower():
            raise ValueError("다른 프로세스의 화면 출력이 섞였습니다.")
        if row["Runtime"] != "DXGI" or row["Dropped"] not in ("0", "1"):
            raise ValueError("DXGI 출력 또는 프레임 표시 상태를 확인할 수 없습니다.")
        now = finite(row[clock], "화면 출력 시각")
        swapchain = row["SwapChainAddress"]
        if now <= previous.get(swapchain, -1):
            raise ValueError("화면 출력 시각이 뒤집히거나 중복됐습니다.")
        previous[swapchain] = now
        swapchains.add(swapchain)
        modes.add(row["PresentMode"])
        interval = finite(row["msBetweenPresents"], "프레임 제출 간격")
        on_screen = finite(row["msBetweenDisplayChange"], "화면 변경 간격")
        latency = finite(row["msUntilDisplayed"], "표시 대기 시간")
        present_api.append(finite(row["msInPresentAPI"], "Present 호출 시간"))
        if interval > 0:
            intervals.append(interval)
        if row["Dropped"] == "0":
            result["displayed_frames"] += 1
            if on_screen > 0:
                displayed.append(on_screen)
            display_latency.append(latency)
        else:
            result["dropped_frames"] += 1
    result.update({"clock": clock, "swapchain_count": len(swapchains), "present_modes": sorted(modes),
                   "between_presents_ms": describe(intervals), "between_display_changes_ms": describe(displayed),
                   "present_api_ms": describe(present_api), "until_displayed_ms": describe(display_latency),
                   "display_observed": result["displayed_frames"] > 0,
                   "display_observation_sufficient": result["displayed_frames"] >= 100 and bool(displayed),
                   "scope": "게임 프로세스의 전체 캡처다. 워밍업과 종료를 포함하며 야간 경로의 합격 판정에 섞지 않는다. 입력 장치 지연은 측정하지 않는다."})
    return result


def analyze_memory(rows, process_id):
    if not rows:
        raise ValueError("GPU 메모리 원본이 비었습니다.")
    dedicated, shared, valid_times = [], [], []
    previous = -1
    gaps = []
    missing = 0
    for row in rows:
        if int(row["ProcessId"]) != process_id:
            raise ValueError("다른 프로세스의 GPU 메모리가 섞였습니다.")
        now = finite(row["ElapsedSeconds"], "메모리 표본 시각")
        if now <= previous:
            raise ValueError("메모리 표본 시각이 뒤집히거나 중복됐습니다.")
        if previous >= 0:
            gaps.append(now - previous)
        previous = now
        if row["QueryStatus"] == "unavailable":
            if int(row["ValidCounters"]) != 0 or row["DedicatedBytes"] != "-1" or row["SharedBytes"] != "-1":
                raise ValueError("미측정 메모리를 정상 값으로 기록했습니다.")
            missing += 1
            continue
        if row["QueryStatus"] != "ok" or int(row["ValidCounters"]) < 2:
            raise ValueError("GPU 메모리 카운터의 유효성을 확인할 수 없습니다.")
        dedicated.append(finite(row["DedicatedBytes"], "전용 GPU 메모리"))
        shared.append(finite(row["SharedBytes"], "공유 GPU 메모리"))
        valid_times.append(now)
    if len(dedicated) < 2 or max(dedicated) <= 0:
        raise ValueError("유효한 GPU 메모리 관측이 부족합니다.")
    return {"samples": len(rows), "valid_samples": len(dedicated), "unavailable_samples": missing,
            "observed": True, "valid_coverage": round(len(dedicated) / len(rows), 6),
            "first_valid_seconds": valid_times[0], "last_valid_seconds": valid_times[-1],
            "sample_interval_seconds": describe(gaps), "dedicated_bytes": describe(dedicated),
            "shared_bytes": describe(shared), "sampled_peak_dedicated_gb": round(max(dedicated) / 1e9, 6),
            "sampled_peak_within_5_5gb": max(dedicated) <= 5_500_000_000,
            "scope": "프로세스 전용 GPU 메모리의 표본이다. 프로세스 사이 공유분을 포함하며, 관측 사이 순간 최대치와 전체 장비의 예산 통과는 보증하지 않는다."}


def summarize(root):
    metadata_path = root / "windows-telemetry.json"
    metadata = json.loads(metadata_path.read_text(encoding="utf-8-sig"))
    if metadata["presentationMode"] != "Windowed" or metadata["offscreen"] is not False:
        raise ValueError("화면 출력 검사의 실행 조건이 다릅니다.")
    contents = {}
    for key in ("presentCsv", "memoryCsv"):
        name = metadata[key]
        if Path(name).name != name or "/" in name or "\\" in name:
            raise ValueError("원본은 같은 폴더의 파일이어야 합니다.")
        source = root / name
        if digest(source) != metadata[key + "Sha256"]:
            raise ValueError(f"측정 원본이 바뀌었습니다: {name}")
        with source.open(encoding="utf-8-sig", newline="") as handle:
            contents[key] = list(csv.DictReader(handle))
    if len(contents["memoryCsv"]) != metadata["sampleCount"]:
        raise ValueError("메모리 원본의 행 수가 기록과 다릅니다.")
    memory = analyze_memory(contents["memoryCsv"], metadata["processId"])
    if memory["unavailable_samples"] != metadata["queryFailures"]:
        raise ValueError("누락된 메모리 표본 수가 기록과 다릅니다.")
    presentation = analyze_present(contents["presentCsv"], metadata["processId"], metadata["executableName"])
    result = {"schemaVersion": 1, "status": "OBSERVED" if presentation["display_observation_sufficient"] else "INSUFFICIENT_DISPLAY",
              "metadata_sha256": digest(metadata_path),
              "shipping_sha256": metadata["shippingSha256"], "present_source_sha256": metadata["presentCsvSha256"],
              "memory_source_sha256": metadata["memoryCsvSha256"],
              "presentation": presentation,
              "gpu_memory": memory, "shipping_release_certified": False}
    (root / "windows-telemetry-summary.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"WINDOWS_TELEMETRY_OBSERVED displayed={result['presentation']['displayed_frames']} "
          f"sampled_vram_gb={memory['sampled_peak_dedicated_gb']}")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    result = summarize(parser.parse_args().directory)
    if not result["presentation"]["display_observation_sufficient"]:
        parser.exit(1, "실제로 화면에 표시된 프레임이 100개보다 적습니다. 원본과 미완료 요약을 보존했습니다.\n")
