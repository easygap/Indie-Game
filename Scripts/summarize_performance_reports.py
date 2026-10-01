"""테스터가 보낸 성능 측정 결과(MissingFloor-performance-*.zip)를 모아 출시 기준과 견준다.

「테스트 도구/성능 측정 - 높음·낮음.bat」이 바탕 화면에 남기는 ZIP을 한 폴더에 모아
그 폴더를 넘긴다. 압축을 푼 폴더(performance-report.json이 든 곳)도 읽는다.

    python Scripts/summarize_performance_reports.py <결과 폴더>

판정 기준은 Docs/PERFORMANCE.md의 성능 합격선이다. 1080p High는 p95 16.67ms·1% low
50fps·50ms 초과 0건, 나머지는 p95 33.33ms·1% low 25fps·100ms 초과 0건. 세 번 모두
넘어야 그 PC의 그 조건을 통과로 센다. 여섯 장비 축(최소 NVIDIA·AMD × Windows 10·11,
권장 NVIDIA·AMD)에 어느 결과가 해당하는지는 사양을 보고 사람이 정한다.
"""
import glob
import json
import os
import re
import sys
import zipfile

LIMITS = {
    ("High", "1080p"): (16.67, 50.0, "over_50ms"),
}
DEFAULT_LIMIT = (33.33, 25.0, "over_100ms")


def reports(folder):
    for path in sorted(glob.glob(os.path.join(folder, "**", "*.zip"), recursive=True)):
        with zipfile.ZipFile(path) as archive:
            names = [n for n in archive.namelist() if n.endswith("performance-report.json")]
            for name in names:
                yield os.path.basename(path), json.loads(archive.read(name).decode("utf-8-sig"))
    for path in sorted(glob.glob(os.path.join(folder, "**", "performance-report.json"), recursive=True)):
        with open(path, encoding="utf-8-sig") as handle:
            yield os.path.relpath(os.path.dirname(path), folder), json.load(handle)


INTEGRATED = re.compile(r"^(AMD Radeon\(TM\) Graphics|Intel\(R\) (UHD|HD|Iris).*Graphics.*)$")


def gpu_name(report):
    # 게임이 실제로 그린 어댑터가 있으면 그것이 답이다(측정 도구가 게임의 기록에서 옮긴다).
    game = report.get("game_gpu") or {}
    if game.get("gpu"):
        return game["gpu"], game.get("driver") or "?"
    gpus = report.get("system", {}).get("gpus") or []
    if isinstance(gpus, dict):
        gpus = [gpus]
    # 예전 결과는 장치 목록뿐이다. 내장 GPU가 함께 잡히면 전용 GPU를 고른다.
    ordered = sorted(gpus, key=lambda g: (bool(INTEGRATED.match(g.get("name", ""))), g.get("name", "")))
    if not ordered:
        return "?", "?"
    return ordered[0].get("name", "?"), ordered[0].get("driver", "?")


def main(argv=None):
    argv = argv if argv is not None else sys.argv[1:]
    if len(argv) != 1:
        print(__doc__)
        return 2
    rows = list(reports(argv[0]))
    if not rows:
        print("성능 측정 결과가 없습니다.")
        return 2
    print(f"결과 {len(rows)}개\n")
    print("| 결과 | GPU | 드라이버 | CPU | Windows | 조건 | 실행 | p95 (ms) | 1% low (fps) | 50ms 초과 | 100ms 초과 | 경로 완주 | 기준 |")
    print("|---|---|---|---|---|---|---:|---:|---:|---:|---:|---|---|")
    verdicts = []
    for source, report in rows:
        system = report.get("system", {})
        gpu, driver = gpu_name(report)
        condition = (report.get("quality", "?"), report.get("resolution", "?"))
        p95_limit, low_limit, hitch_key = LIMITS.get(condition, DEFAULT_LIMIT)
        passed_runs = 0
        runs = report.get("runs", [])
        for run in runs:
            summary = run.get("summary") or {}
            ok = (bool(summary) and run.get("route_completed")
                  and summary.get("p95_ms", 1e9) <= p95_limit
                  and summary.get("one_percent_low_fps", 0) >= low_limit
                  and summary.get(hitch_key, 1) == 0)
            passed_runs += 1 if ok else 0
            print(f"| {source} | {gpu} | {driver} | {system.get('cpu', '?')} | {system.get('os', '?')} | "
                  f"{condition[1]} {condition[0]} | {run.get('run', '?')} | {summary.get('p95_ms', '-')} | "
                  f"{summary.get('one_percent_low_fps', '-')} | {summary.get('over_50ms', '-')} | "
                  f"{summary.get('over_100ms', '-')} | {'예' if run.get('route_completed') else '아니오'} | "
                  f"{'통과' if ok else '실패'} |")
        verdicts.append((gpu, condition, passed_runs, len(runs)))
    print("\n## PC별 판정\n")
    for gpu, condition, passed_runs, total in verdicts:
        verdict = "통과" if total >= 3 and passed_runs == total else "기준 미달"
        print(f"- {gpu} · {condition[1]} {condition[0]}: {passed_runs}/{total}회 통과 → {verdict}")
    print("\n세 번 모두 통과해야 그 조건을 통과로 센다. 여섯 장비 축의 해당 여부는 사양으로 따로 정한다.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
