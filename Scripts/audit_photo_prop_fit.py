"""스캔 소품이 지정한 상자 안에 어떻게 들어앉는지 실측으로 확인한다.

`PlacePhotoProp`은 맞춤 상자를 **메시의 로컬 축**에 먹이고, 요각은 그 뒤에
돈다. 그래서 상자를 월드 기준으로 적으면 요각 90도에서 가로세로가 뒤집히고,
종횡비를 지키는 맞춤은 셋 중 가장 빡빡한 비율 하나로 전체를 줄인다. 둘 다
컴파일도 감사도 통과하고 로그도 조용하다.

실제로 프롤로그 침대가 그랬다. 원본은 90 x 200 x 120이고 머리판이 로컬 -Y
쪽인데, 요각 90도가 길이를 월드 X로 눕혀 -X 벽을 33 cm 파고들었고 같은
자리의 이불(Y를 따라 눕는다)과 직각이 됐다.

원본 glTF가 `Content/SourceArt/PhotoProps/`에 그대로 있으므로, 에디터 없이
정점 바운드를 읽어 최종 크기를 그대로 계산할 수 있다. glTF는 Y-up이고
언리얼은 Z-up이므로 축은 (X, Z, Y)로 옮긴다 — 이 대응은 `metal_office_desk`
와 `painted_wooden_chair_01`이 `PlacePhotoPropExactSize`에 적어 둔 값과
원본 치수를 맞춰 확인했다.

두 가지를 본다.

* ``PROP_THROUGH_STRUCTURE`` — 소품의 월드 상자가 벽·슬래브·연석을 파고든다.
* ``PROP_AXIS_MISMATCH``     — 종횡비 맞춤인데 축별 비율이 두 배 넘게 벌어진다.
  맞춤 상자의 축이 메시의 축과 어긋났다는 뜻이고, 결과는 지정한 것보다
  훨씬 작은 소품이다.

**메시가 여럿인 원본은 건너뛴다.** `FindPhotoPropMesh`가 그중 하나를 골라
쓰는데, 임포터가 이름을 바꾸기 때문에 glTF 쪽에서 같은 것을 짚을 방법이
없다. 건너뛴 것은 요약에 수를 적는다.

사용법:
    python Scripts/audit_photo_prop_fit.py            # 사람이 읽는 표
    python Scripts/audit_photo_prop_fit.py --json
    python Scripts/audit_photo_prop_fit.py --check    # 발견 시 종료 코드 1
    python Scripts/audit_photo_prop_fit.py --self-test
"""

from __future__ import annotations

import argparse
import glob
import json
import math
import os
import re
import sys
from dataclasses import dataclass, asdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import audit_world_geometry as geometry  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_ART = os.path.join("Content", "SourceArt", "PhotoProps")
SCENE_SOURCE = os.path.join(
    "Source", "IndieGame", "Core", "IGPrologueWorldScene.cpp")

# 이 에셋만 FindPhotoPropMesh가 일부러 nullptr을 돌려준다. 사진 소품이 아니라
# 대체 상자가 사는 자리이므로 여기서도 보지 않는다.
REJECTED_ASSETS = frozenset({"modern_wooden_cabinet"})

# 구조물로 볼 상자의 최소 두께. 이보다 얇으면 걸레받이나 바닥 도색이라
# 소품이 겹쳐도 잘못이 아니다.
MIN_STRUCTURE_THICKNESS = 10.0
# 벽에 붙여 놓은 소품이 정상이므로 얼마간은 접촉으로 본다. 고정값에 더해
# 소품 자기 발자국의 1/5까지 봐주는 이유는, 돌려 놓은 소품의 축정렬 상자
# 모서리는 대개 빈 공간이기 때문이다 — 쓰레기봉투처럼 둥근 물건은 벽에
# 기대 놓기만 해도 상자 모서리가 벽 안으로 들어간다.
STRUCTURE_TOLERANCE = 5.0
STRUCTURE_FOOTPRINT_SHARE = 0.2
# 종횡비 맞춤에서 축별 비율이 이보다 더 벌어지면 축이 어긋난 것으로 본다.
MIN_RATIO_SPREAD = 0.5

# 4층 집은 UpperFloorRoot 아래에 있다. 소품 좌표가 어느 쪽인지는 최종 상자가
# 구조물과 맞물리는지로 갈리므로, 두 원점을 다 시험하고 더 잘 맞는 쪽을 쓴다.
FRAME_Z_OFFSETS = (0.0, 900.0)

CALL = re.compile(
    r'PlacePhotoProp(?P<exact>ExactSize)?\(\s*\n?\s*TEXT\("(?P<asset>\w+)"\)'
    r'\s*,\s*\n?\s*FVector\((?P<location>[^)]*)\)'
    r'\s*,\s*\n?\s*FVector\((?P<fit>[^)]*)\)'
    r'\s*,\s*\n?\s*(?P<yaw>-?[\d.]+)f?(?=\s*[,\)])',
    re.S)


@dataclass
class Finding:
    code: str
    source: str
    line: int
    detail: str

    def label(self) -> str:
        return os.path.basename(self.source) + ":" + str(self.line)


def numbers(text: str):
    return [float(part.strip().rstrip("f")) for part in text.split(",")]


def mesh_bounds():
    """단일 메시 원본의 언리얼 로컬 크기(cm). 여럿이면 이름만 돌려준다."""
    sizes = {}
    skipped = []
    pattern = os.path.join(ROOT, SOURCE_ART, "*", "*.gltf")
    for path in sorted(glob.glob(pattern)):
        asset = os.path.basename(os.path.dirname(path))
        try:
            with open(path, encoding="utf-8") as handle:
                document = json.load(handle)
        except (ValueError, UnicodeDecodeError):
            # LFS 포인터거나 깨진 파일. 없는 것으로 둔다.
            skipped.append(asset)
            continue
        meshes = document.get("meshes", [])
        if len(meshes) != 1:
            skipped.append(asset)
            continue
        low = [float("inf")] * 3
        high = [float("-inf")] * 3
        for primitive in meshes[0].get("primitives", []):
            index = primitive.get("attributes", {}).get("POSITION")
            if index is None:
                continue
            accessor = document["accessors"][index]
            if "min" not in accessor or "max" not in accessor:
                continue
            for axis in range(3):
                low[axis] = min(low[axis], accessor["min"][axis])
                high[axis] = max(high[axis], accessor["max"][axis])
        if low[0] == float("inf"):
            skipped.append(asset)
            continue
        span = [(high[axis] - low[axis]) * 100.0 for axis in range(3)]
        # glTF (X 오른쪽, Y 위, Z 앞) → 언리얼 (X, Y, Z 위)
        sizes[asset] = (span[0], span[2], span[1])
    return sizes, skipped


def world_extent(size, yaw_degrees):
    """요각을 먹인 뒤의 축정렬 가로·세로."""
    radians = math.radians(yaw_degrees)
    cosine, sine = abs(math.cos(radians)), abs(math.sin(radians))
    return (size[0] * cosine + size[1] * sine,
            size[0] * sine + size[1] * cosine)


def placements(text: str):
    for match in CALL.finditer(text):
        try:
            location = numbers(match.group("location"))
            fit = numbers(match.group("fit"))
        except ValueError:
            continue
        if len(location) != 3 or len(fit) != 3:
            continue
        yield {
            "line": text.count("\n", 0, match.start()) + 1,
            "asset": match.group("asset"),
            "exact": bool(match.group("exact")),
            "location": location,
            "fit": fit,
            "yaw": float(match.group("yaw")),
        }


def final_size(fit, bounds, exact):
    ratios = [fit[axis] / bounds[axis] for axis in range(3)]
    if exact:
        scale = ratios
    else:
        scale = [min(ratios)] * 3
    return [bounds[axis] * scale[axis] for axis in range(3)], ratios


def audit() -> tuple:
    sizes, skipped = mesh_bounds()
    with open(os.path.join(ROOT, SCENE_SOURCE), encoding="utf-8-sig") as handle:
        text = handle.read()

    result = geometry.scan_source(SCENE_SOURCE)
    structures = [
        box for box in result.boxes
        if box.collision
        and box.is_structure()
        and min(box.size) >= MIN_STRUCTURE_THICKNESS
    ]

    findings = []
    checked = 0
    for placement in placements(text):
        asset = placement["asset"]
        if asset in REJECTED_ASSETS or asset not in sizes:
            continue
        checked += 1
        bounds = sizes[asset]
        size, ratios = final_size(placement["fit"], bounds, placement["exact"])
        extent_x, extent_y = world_extent(size, placement["yaw"])
        spread = min(ratios) / max(ratios)
        if not placement["exact"] and spread < MIN_RATIO_SPREAD:
            findings.append(Finding(
                "PROP_AXIS_MISMATCH",
                SCENE_SOURCE,
                placement["line"],
                asset + " 맞춤 "
                + "x".join("%.0f" % value for value in placement["fit"])
                + " 이 원본 "
                + "x".join("%.0f" % value for value in bounds)
                + " 과 축이 어긋난다 — 종횡비 때문에 "
                + "%.0f x %.0f x %.0f" % (size[0], size[1], size[2])
                + " 로 줄어든다"))

        worst = None
        for offset in FRAME_Z_OFFSETS:
            low = (placement["location"][0] - extent_x / 2.0,
                   placement["location"][1] - extent_y / 2.0,
                   placement["location"][2] + offset)
            high = (low[0] + extent_x, low[1] + extent_y, low[2] + size[2])
            for structure in structures:
                sl, sh = structure.minimum, structure.maximum
                overlap = [min(high[axis], sh[axis]) - max(low[axis], sl[axis])
                           for axis in range(3)]
                if any(value <= 0 for value in overlap):
                    continue
                depth = min(overlap[0], overlap[1])
                if worst is None or depth > worst[0]:
                    worst = (depth, structure)
        tolerance = max(
            STRUCTURE_TOLERANCE,
            STRUCTURE_FOOTPRINT_SHARE * min(extent_x, extent_y))
        if worst and worst[0] > tolerance:
            findings.append(Finding(
                "PROP_THROUGH_STRUCTURE",
                SCENE_SOURCE,
                placement["line"],
                asset + " 가 " + os.path.basename(SCENE_SOURCE) + ":"
                + str(worst[1].line) + "의 구조물을 "
                + ("%.0f" % worst[0]) + " cm 파고든다"))

    findings.sort(key=lambda item: (item.line, item.code))
    return findings, checked, sorted(set(skipped))


def self_test() -> int:
    failures = []

    if numbers(" -140, 110 , 0 ") != [-140.0, 110.0, 0.0]:
        failures.append("좌표 문자열을 못 읽었다")

    extent = world_extent((90.0, 200.0, 120.0), 0.0)
    if abs(extent[0] - 90.0) > 0.01 or abs(extent[1] - 200.0) > 0.01:
        failures.append("요각 0도에서 크기가 바뀌었다")
    extent = world_extent((90.0, 200.0, 120.0), 90.0)
    if abs(extent[0] - 200.0) > 0.01 or abs(extent[1] - 90.0) > 0.01:
        failures.append("요각 90도에서 가로세로가 안 바뀐다")
    extent = world_extent((90.0, 200.0, 120.0), 180.0)
    if abs(extent[0] - 90.0) > 0.01:
        failures.append("요각 180도가 90도처럼 계산된다")

    size, ratios = final_size((108.0, 208.0, 130.0), (90.0, 200.0, 120.0), False)
    if abs(size[0] - 93.6) > 0.5 or abs(size[1] - 208.0) > 0.5:
        failures.append("종횡비 맞춤 결과가 틀렸다")
    size, _ = final_size((145.0, 68.0, 74.0), (200.0, 95.0, 79.0), True)
    if abs(size[0] - 145.0) > 0.01 or abs(size[1] - 68.0) > 0.01:
        failures.append("정확 크기 맞춤이 지정값을 안 지킨다")

    sizes, skipped = mesh_bounds()
    if len(sizes) < 8:
        failures.append("읽어 낸 원본이 너무 적다 (" + str(len(sizes)) + ")")
    bed = sizes.get("old_bed_frame")
    if not bed or abs(bed[0] - 90) > 2 or abs(bed[1] - 200) > 2 \
            or abs(bed[2] - 120) > 2:
        failures.append("침대 원본 치수가 90x200x120이 아니다: " + str(bed))
    chair = sizes.get("painted_wooden_chair_01")
    if not chair or abs(chair[0] - 43) > 2 or abs(chair[1] - 54) > 2             or abs(chair[2] - 96) > 2:
        failures.append("의자 원본이 43x54x96이 아니다 — 축 대응이 틀렸다: "
                        + str(chair))
    if "outdoor_table_chair_set_01" in sizes:
        failures.append("메시가 여럿인 원본을 걸러 내지 못했다")
    if not skipped:
        failures.append("건너뛴 원본이 하나도 없다 — 걸러 내기가 안 돈다")

    calls = list(placements(open(
        os.path.join(ROOT, SCENE_SOURCE), encoding="utf-8-sig").read()))
    fixture = """
    PlacePhotoProp(TEXT("crate"), FVector(1, 2, 3), FVector(4, 5, 6), 0);
    PlacePhotoPropExactSize(TEXT("bed"), FVector(7, 8, 9), FVector(10, 11, 12), -90.0f, false);
    PlacePhotoProp(TEXT("bag"), FVector(1, 2, 3), FVector(4, 5, 6), 12.5);
    """
    parsed = list(placements(fixture))
    if [call["yaw"] for call in parsed] != [0.0, -90.0, 12.5]:
        failures.append("정수·실수·f 접미사가 붙은 요각을 같은 방식으로 읽지 못했다")
    if len(parsed) != 3 or not parsed[1]["exact"]:
        failures.append("두 배치 함수의 크기 지정 방식을 구별하지 못했다")
    if not calls:
        failures.append("월드에서 사진 소품 배치를 하나도 찾지 못했다")
    if not any(call["exact"] for call in calls):
        failures.append("PlacePhotoPropExactSize 호출을 못 찾았다")

    for failure in failures:
        print("  자기 검사 실패: " + failure)
    print("PHOTO PROP FIT SELF-TEST " + ("FAIL" if failures else "PASS")
          + " sources=" + str(len(sizes)) + " calls=" + str(len(calls)))
    return 1 if failures else 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args(argv)

    if arguments.self_test:
        return self_test()

    findings, checked, skipped = audit()
    if arguments.json:
        print(json.dumps([asdict(item) for item in findings],
                         ensure_ascii=False, indent=2))
    else:
        for finding in findings:
            print("  [" + finding.code + "] " + finding.label() + "  "
                  + finding.detail)
        print("PHOTO PROP FIT AUDIT  placements=" + str(checked)
              + " findings=" + str(len(findings)))
        if skipped:
            print("  메시가 여럿이라 건너뛴 원본 " + str(len(skipped))
                  + "종 — " + ", ".join(skipped))
    return 1 if (arguments.check and findings) else 0


if __name__ == "__main__":
    raise SystemExit(main())
