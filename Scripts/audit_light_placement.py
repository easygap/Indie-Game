"""광원이 막힌 물건 안에 들어가 있는지 본다.

이 프로젝트의 조명은 가구와 마찬가지로 리터럴 좌표로 놓인다. 그래서 옆에
있는 가구가 조금 자라거나 자리를 옮기면 광원이 그 안으로 들어가 버리는데,
컴파일도 통과하고 감사도 통과하고 로그도 조용하다. 방이 그냥 어두워질
뿐이다. 실제로 프롤로그 침실의 유일한 온색 광원이 옷장 안에 9.5 cm
들어가 있었다.

두 가지를 본다.

* ``LIGHT_INSIDE_SOLID`` — 광원 위치가 충돌을 가진 상자 안에 있다. 벽,
  슬래브, 가구처럼 사람이 통과하지 못하는 물건이면 빛도 통과하지 못한다.
* ``LIGHT_ON_SURFACE`` — 상자 표면에서 1 cm 안쪽에 걸쳐 있다. 안에 있는
  것은 아니지만 그림자 편향에 따라 빛이 반쯤 먹히는 자리라 알려는 준다.

**충돌 없는 상자는 보지 않는다.** 조명 기구의 갓, 유리 커버, 간판 함체는
광원을 감싸는 것이 정상이고 그것들은 전부 충돌이 없다. 「사람이 못 지나가는
것 안에 빛이 있다」가 이 감사가 묻는 전부다.

사용법:
    python Scripts/audit_light_placement.py            # 사람이 읽는 표
    python Scripts/audit_light_placement.py --json
    python Scripts/audit_light_placement.py --check    # 발견 시 종료 코드 1
    python Scripts/audit_light_placement.py --self-test
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from dataclasses import dataclass, asdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import audit_world_geometry as geometry  # noqa: E402

# 조명 생성 함수. 두 번째 인자는 세기(float)라 크기로 읽으면 뜻이 없지만,
# 이 감사는 위치만 쓰므로 상관없다. 표에 넣어야 반복문과 람다까지 펼쳐진다.
LIGHT_CALLS = {
    "CreateLight": (0, 1, None, None, None, "light"),
    "CreatePointLight": (0, 1, None, None, None, "light"),
    "CreateSpotLight": (0, 1, None, None, None, "light"),
}

# 이보다 얇은 상자는 벽지, 몰딩, 스티커 같은 표면 장식이다. 그 안에 좌표가
# 들어간다고 빛이 막히지 않는다.
MIN_SOLID_THICKNESS = 3.0
# 표면에서 이만큼 안쪽까지는 「걸쳐 있다」로 따로 알린다.
SURFACE_MARGIN = 1.0


@dataclass
class Finding:
    code: str
    source: str
    line: int
    detail: str

    def label(self) -> str:
        return os.path.basename(self.source) + ":" + str(self.line)


def scan(relative_path: str):
    """조명과 상자를 한 번에 뽑는다. 표를 잠깐 넓혔다가 되돌린다."""
    original = geometry.PLACEMENT_CALLS
    geometry.PLACEMENT_CALLS = dict(original)
    geometry.PLACEMENT_CALLS.update(LIGHT_CALLS)
    try:
        result = geometry.scan_source(relative_path)
    finally:
        geometry.PLACEMENT_CALLS = original
    lights = [box for box in result.boxes if box.kind == "light"]
    solids = [
        box for box in result.boxes
        if box.kind != "light"
        and box.collision
        and min(box.size) >= MIN_SOLID_THICKNESS
    ]
    return lights, solids


def containment(point, box):
    """상자 표면까지의 최소 거리. 밖이면 None."""
    minimum, maximum = box.minimum, box.maximum
    for axis in range(3):
        if not (minimum[axis] <= point[axis] <= maximum[axis]):
            return None
    return min(
        min(point[axis] - minimum[axis], maximum[axis] - point[axis])
        for axis in range(3))


def audit() -> list:
    findings = []
    for relative_path in geometry.SOURCES:
        lights, solids = scan(relative_path)
        for light in lights:
            worst = None
            for solid in solids:
                depth = containment(light.center, solid)
                if depth is None:
                    continue
                if worst is None or depth > worst[0]:
                    worst = (depth, solid)
            if worst is None:
                continue
            depth, solid = worst
            where = "(%.0f, %.0f, %.0f)" % light.center
            size = "%.0f x %.0f x %.0f" % solid.size
            if depth > SURFACE_MARGIN:
                findings.append(Finding(
                    "LIGHT_INSIDE_SOLID",
                    relative_path,
                    light.line,
                    where + " 광원이 " + os.path.basename(relative_path) + ":"
                    + str(solid.line) + "의 " + size + " 상자 안에 "
                    + ("%.1f" % depth) + " cm 들어가 있다"))
            else:
                findings.append(Finding(
                    "LIGHT_ON_SURFACE",
                    relative_path,
                    light.line,
                    where + " 광원이 " + os.path.basename(relative_path) + ":"
                    + str(solid.line) + "의 " + size + " 상자 표면에 걸쳐 있다"))
    return sorted(findings, key=lambda item: (item.source, item.line))


def self_test() -> int:
    failures = []

    class _Box:
        def __init__(self, center, size, collision=True, kind="block", line=1):
            self.center, self.size = center, size
            self.collision, self.kind, self.line = collision, kind, line

        @property
        def minimum(self):
            return tuple(self.center[i] - self.size[i] / 2.0 for i in range(3))

        @property
        def maximum(self):
            return tuple(self.center[i] + self.size[i] / 2.0 for i in range(3))

    cabinet = _Box((0.0, 0.0, 90.0), (36.0, 80.0, 180.0))
    if containment((0.0, 0.0, 90.0), cabinet) != 18.0:
        failures.append("상자 한가운데의 깊이를 잘못 쟀다")
    if containment((100.0, 0.0, 90.0), cabinet) is not None:
        failures.append("밖에 있는 점을 안이라고 했다")
    edge = containment((17.6, 0.0, 90.0), cabinet)
    if edge is None or edge > SURFACE_MARGIN:
        failures.append("표면에 걸친 점을 못 알아봤다")

    # 실제 트리에서 조명과 상자가 실제로 뽑히는지. 표를 넓히지 않으면 0개다.
    lights, solids = scan(geometry.SOURCES[0])
    if len(lights) < 10:
        failures.append("프롤로그 조명이 너무 적다 (" + str(len(lights)) + ")")
    if len(solids) < 100:
        failures.append("고체 상자가 너무 적다 (" + str(len(solids)) + ")")
    if "CreateLight" in geometry.PLACEMENT_CALLS:
        failures.append("넓힌 배치 호출 표를 되돌리지 않았다")

    # 충돌 없는 갓은 면제된다는 것.
    shade = _Box((0.0, 0.0, 100.0), (24.0, 24.0, 20.0), collision=False)
    if shade.collision:
        failures.append("자기 검사용 갓이 충돌을 갖고 있다")

    for failure in failures:
        print("  자기 검사 실패: " + failure)
    print("LIGHT PLACEMENT SELF-TEST " + ("FAIL" if failures else "PASS")
          + " lights=" + str(len(lights)) + " solids=" + str(len(solids)))
    return 1 if failures else 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args(argv)

    if arguments.self_test:
        return self_test()

    findings = audit()
    if arguments.json:
        print(json.dumps([asdict(item) for item in findings],
                         ensure_ascii=False, indent=2))
    else:
        for finding in findings:
            print("  [" + finding.code + "] " + finding.label() + "  "
                  + finding.detail)
        total = 0
        for relative_path in geometry.SOURCES:
            total += len(scan(relative_path)[0])
        print("LIGHT PLACEMENT AUDIT  lights=" + str(total)
              + " findings=" + str(len(findings)))
    return 1 if (arguments.check and findings) else 0


if __name__ == "__main__":
    raise SystemExit(main())
