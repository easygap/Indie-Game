#!/usr/bin/env python3
"""같은 평면에 두 면을 겹쳐 놓아 화면에서 서로 깜빡이는 자리를 찾는다.

이 씬의 물건은 전부 리터럴 좌표다. 그래서 나중에 세운 상자가 이미 있던
상자의 면과 **소수점까지 같은 평면**에 놓이는 일이 생긴다. 깊이 버퍼는
둘 중 어느 쪽이 앞인지 정하지 못하고, 카메라가 조금만 움직여도 두 재질이
번갈아 이긴다. 멀리서 보면 표면이 지글거리고, 가까이서 보면 무늬가 통째로
바뀐다 — 흔히 Z-파이팅이라고 부르는 그것이다.

컴파일러도 쿠커도 이것을 모른다. 기하 감사도 못 본다. 파고든 깊이가 0이라
`EMBEDDED`가 아니고, 바닥에 붙어 있으니 `FLOATING`도 아니다. 틀린 것은
**두 면이 같은 자리에 있다**는 사실 하나뿐이다.

실제로 그렇게 나가 있었다. 편의점 냉장 진열대 뒷판은 동쪽 벽 안에 통째로
박혀 벽면과 같은 평면이었고, 필로티 바닥과 로비 연결통로 바닥은 6.2 m²를
같은 높이로 겹쳐 깔고 있었다.

코드만 읽고 하나를 본다.

* ``COPLANAR_FIGHT`` - 서로 다른 재질을 쓰는 두 면이 같은 평면에서 겹치고,
                      그 면 앞에 사람이 설 자리가 있다

**보이지 않는 면은 세지 않는다.** 면 바깥으로 20 cm 안에 다른 상자가 있으면
그 자리는 어차피 가려진다. 아래를 보는 면은 그 밑에 방이 있을 때만 센다 —
건물 바닥 슬래브의 배는 사람이 갈 수 없는 자리다. 같은 재질끼리 겹친 것도
세지 않는다. 두 면이 똑같이 그려지므로 어느 쪽이 이기든 화면은 같다.

어디서든 돌아간다.

    python3 Scripts/audit_coplanar_surfaces.py             # 사람이 읽는 보고
    python3 Scripts/audit_coplanar_surfaces.py --json      # 기계가 읽는 보고
    python3 Scripts/audit_coplanar_surfaces.py --check     # 발견되면 exit 1
    python3 Scripts/audit_coplanar_surfaces.py --self-test # 감사 자체를 검사
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, asdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import audit_world_geometry as geometry  # noqa: E402

# 두 면이 이보다 가까우면 깊이 버퍼가 둘을 갈라놓지 못한다. 4 mm 인쇄판이
# 이 씬의 가장 얇은 드레싱이므로, 그보다 훨씬 작게 잡아야 정상 배치를
# 잘못 걸지 않는다.
COPLANAR_EPSILON = 0.05
# 겹친 면의 두 변이 각각 이보다 짧으면 모서리가 스친 것이다.
MIN_FACE_SIDE = 6.0
# 이만큼도 안 보이는 자리는 알려 봐야 고칠 것이 없다.
MIN_VISIBLE_AREA = 400.0
# 면 바깥으로 이 거리들에 상자가 없어야 「앞이 트였다」로 본다. 카메라가
# 벽에 붙어도 20 cm는 남는다.
CLEARANCE_PROBES = (2.0, 8.0, 20.0)
# 아래를 보는 면은 그 밑에 방이 있을 때만 보인다. 층고 범위다.
ROOM_BELOW_NEAR = 70.0
ROOM_BELOW_FAR = 420.0
# 면 하나를 이 간격으로 훑는다. 촘촘하게 해 봐야 결론이 안 바뀐다.
SAMPLE_STEP = 4.0
MAX_SAMPLES_PER_AXIS = 40

AXIS_NAMES = ("X", "Y", "Z")

# `CreateBlock(...)`을 변수로 받아 곧바로 숨기는 관용구. 숨긴 상자는
# 그려지지 않으므로 어느 평면에 있든 상관없다 — 물탱크의 충돌 대역이 그렇다.
HIDE_CALL = re.compile(
    r"(\w+)->SetVisibility\(\s*false|(\w+)->SetHiddenInGame\(\s*true")
HIDE_REACH_LINES = 16


@dataclass
class Finding:
    code: str
    source: str
    line: int
    other_line: int
    axis: str
    plane: float
    area_cm2: float
    visible_cm2: float
    detail: str

    def label(self) -> str:
        return os.path.basename(self.source) + ":" + str(self.line)


def hidden_lines(relative_path: str) -> set:
    """숨기는 호출이 붙은 CreateBlock의 줄 번호."""
    absolute = os.path.join(geometry.PROJECT_ROOT, relative_path)
    with open(absolute, encoding="utf-8") as handle:
        lines = handle.read().splitlines()
    found = set()
    for index, line in enumerate(lines):
        match = HIDE_CALL.search(line)
        if not match:
            continue
        name = match.group(1) or match.group(2)
        assignment = re.compile(re.escape(name) + r"\s*=\s*CreateBlock")
        for back in range(index, max(0, index - HIDE_REACH_LINES), -1):
            if not assignment.search(lines[back]):
                continue
            for forward in range(back, min(len(lines), back + HIDE_REACH_LINES)):
                if "CreateBlock" in lines[forward]:
                    found.add(forward + 1)
                    break
            break
    return found


def axis_aligned(box) -> bool:
    return all(
        abs(angle) < 1e-6
        or abs(abs(angle) - 90.0) < 1e-6
        or abs(abs(angle) - 180.0) < 1e-6
        for angle in box.rotation)


def drawable(box, hidden: set) -> bool:
    """화면에 실제 크기로 그려지는 상자인가."""
    if box.note in ("authored", "unknown-rotation"):
        # 저작 메시는 크기 인자가 배율이라 상자가 뜻이 없고, 못 읽은
        # 회전은 자리 자체가 불확실하다.
        return False
    if getattr(box, "exempt", ""):
        # 기하 감사와 같은 표시를 읽는다. 서로 배타적인 분기가 같은
        # 봉투를 그리는 자리는 둘 다 세면 늘 겹친 것으로 나온다.
        return False
    return axis_aligned(box) and box.line not in hidden


def contains(box, point, pad: float = 0.02) -> bool:
    minimum, maximum = box.minimum, box.maximum
    return all(
        minimum[axis] - pad <= point[axis] <= maximum[axis] + pad
        for axis in range(3))


def covers_column(box, axis: int, first: float, second: float) -> bool:
    """상자가 면 위 한 점의 수직선을 덮는가."""
    others = [index for index in range(3) if index != axis]
    minimum, maximum = box.minimum, box.maximum
    return (minimum[others[0]] <= first <= maximum[others[0]]
            and minimum[others[1]] <= second <= maximum[others[1]])


def sample_grid(low: float, high: float) -> list:
    span = high - low
    count = max(1, min(MAX_SAMPLES_PER_AXIS, int(span / SAMPLE_STEP)))
    step = span / count
    return [low + step * (index + 0.5) for index in range(count)]


def visible_share(axis, sign, plane, low, high, neighbours) -> float:
    """겹친 면 중 사람이 볼 수 있는 비율."""
    others = [index for index in range(3) if index != axis]
    first_samples = sample_grid(low[0], high[0])
    second_samples = sample_grid(low[1], high[1])
    looking_down = axis == 2 and sign < 0
    visible = 0
    for first in first_samples:
        for second in second_samples:
            point = [0.0, 0.0, 0.0]
            point[others[0]] = first
            point[others[1]] = second
            blocked = False
            for distance in CLEARANCE_PROBES:
                point[axis] = plane + sign * distance
                if any(contains(box, tuple(point)) for box in neighbours):
                    blocked = True
                    break
            if blocked:
                continue
            if looking_down and not any(
                    ROOM_BELOW_NEAR <= plane - box.maximum[2] <= ROOM_BELOW_FAR
                    and covers_column(box, axis, first, second)
                    for box in neighbours):
                # 밑에 방이 없다. 슬래브의 배는 사람이 볼 수 없다.
                continue
            visible += 1
    return visible / float(len(first_samples) * len(second_samples))


def overlap(first_min, first_max, second_min, second_max) -> float:
    return min(first_max, second_max) - max(first_min, second_min)


def collect(sources=None) -> list:
    boxes = []
    for relative_path in (sources or geometry.SOURCES):
        hidden = hidden_lines(relative_path)
        for box in geometry.scan_source(relative_path).boxes:
            if drawable(box, hidden):
                boxes.append(box)
    return boxes


def audit(boxes: list) -> list:
    by_frame = {}
    for box in boxes:
        by_frame.setdefault(box.frame, []).append(box)

    findings = []
    for group in by_frame.values():
        # 좌표 하나로 정렬해 두면 겹칠 수 없는 짝을 일찍 끊을 수 있다.
        group = sorted(group, key=lambda box: box.minimum[0])
        for first_index, first in enumerate(group):
            for second in group[first_index + 1:]:
                if second.minimum[0] > first.maximum[0] + COPLANAR_EPSILON:
                    break
                findings.extend(_check_pair(first, second, group))
    return sorted(
        findings,
        key=lambda item: (-item.visible_cm2, item.source, item.line))


def _check_pair(first, second, group) -> list:
    if first.material == second.material:
        # 같은 재질이면 어느 쪽이 이기든 같은 그림이 나온다.
        return []
    first_min, first_max = first.minimum, first.maximum
    second_min, second_max = second.minimum, second.maximum
    spans = [
        overlap(first_min[axis], first_max[axis],
                second_min[axis], second_max[axis])
        for axis in range(3)
    ]
    findings = []
    for axis in range(3):
        others = [index for index in range(3) if index != axis]
        if spans[others[0]] < MIN_FACE_SIDE or spans[others[1]] < MIN_FACE_SIDE:
            continue
        low = tuple(
            max(first_min[index], second_min[index]) for index in others)
        high = tuple(
            min(first_max[index], second_max[index]) for index in others)
        area = spans[others[0]] * spans[others[1]]
        for sign, first_plane, second_plane in (
                (1, first_max[axis], second_max[axis]),
                (-1, first_min[axis], second_min[axis])):
            if abs(first_plane - second_plane) > COPLANAR_EPSILON:
                continue
            neighbours = [
                box for box in group
                if box is not first and box is not second
            ]
            share = visible_share(
                axis, sign, first_plane, low, high, neighbours)
            visible = area * share
            if visible < MIN_VISIBLE_AREA:
                continue
            centre = [0.0, 0.0, 0.0]
            centre[axis] = first_plane
            centre[others[0]] = (low[0] + high[0]) / 2.0
            centre[others[1]] = (low[1] + high[1]) / 2.0
            findings.append(Finding(
                "COPLANAR_FIGHT",
                first.source,
                first.line,
                second.line,
                AXIS_NAMES[axis] + ("+" if sign > 0 else "-"),
                round(first_plane, 2),
                round(area, 1),
                round(visible, 1),
                "%s 면이 %s:%d의 %s와 같은 평면(%s=%.2f)에서 %.2f m² 겹친다"
                " — 보이는 넓이 %.2f m², (%.0f, %.0f, %.0f)"
                % (first.material,
                   os.path.basename(second.source), second.line,
                   second.material,
                   AXIS_NAMES[axis], first_plane,
                   area / 10000.0, visible / 10000.0,
                   centre[0], centre[1], centre[2])))
    return findings


def self_test() -> int:
    failures = []

    class _Box:
        def __init__(self, center, size, material, line=1, frame="t",
                     note="", rotation=(0.0, 0.0, 0.0), source="t.cpp"):
            self.center, self.size, self.material = center, size, material
            self.line, self.frame, self.note = line, frame, note
            self.rotation, self.source = rotation, source
            self.collision = True

        @property
        def minimum(self):
            return tuple(self.center[i] - self.size[i] / 2.0 for i in range(3))

        @property
        def maximum(self):
            return tuple(self.center[i] + self.size[i] / 2.0 for i in range(3))

    # 바닥 두 장을 같은 높이로 깔면 걸린다.
    slab = _Box((0.0, 0.0, -10.0), (400.0, 400.0, 20.0), "Concrete", line=1)
    finish = _Box((0.0, 0.0, -10.0), (200.0, 200.0, 20.0), "Tile", line=2)
    hits = audit([slab, finish])
    if not any(item.axis == "Z+" for item in hits):
        failures.append("같은 높이로 깐 바닥 두 장을 못 봤다")

    # 마감을 1 cm 올려 놓으면 걸리지 않는다.
    raised = _Box((0.0, 0.0, -9.0), (200.0, 200.0, 20.0), "Tile", line=2)
    if audit([slab, raised]):
        failures.append("1 cm 띄운 마감을 겹쳤다고 했다")

    # 같은 재질끼리는 세지 않는다.
    twin = _Box((0.0, 0.0, -10.0), (200.0, 200.0, 20.0), "Concrete", line=2)
    if audit([slab, twin]):
        failures.append("같은 재질끼리 겹친 것을 걸었다")

    # 앞을 다른 상자가 막고 있으면 세지 않는다.
    lid = _Box((0.0, 0.0, 5.0), (400.0, 400.0, 10.0), "Steel", line=3)
    if any(item.axis == "Z+" for item in audit([slab, finish, lid])):
        failures.append("덮개로 막힌 면을 보인다고 했다")

    # 슬래브의 배는 밑에 방이 없으면 보이지 않는다.
    if any(item.axis == "Z-" for item in audit([slab, finish])):
        failures.append("밑에 방도 없는 슬래브 배를 보인다고 했다")
    room = _Box((0.0, 0.0, -230.0), (400.0, 400.0, 20.0), "Concrete", line=4)
    if not any(item.axis == "Z-" for item in audit([slab, finish, room])):
        failures.append("밑에 방이 있는 슬래브 배를 못 봤다")

    # 벽 안에 박힌 판. 앞면이 벽면과 같은 평면이면 걸린다.
    wall = _Box((10.0, 0.0, 130.0), (20.0, 400.0, 260.0), "Stucco", line=5)
    panel = _Box((5.0, 0.0, 116.0), (10.0, 300.0, 220.0), "Steel", line=6)
    if not any(item.axis == "X-" for item in audit([wall, panel])):
        failures.append("벽 안에 박힌 판의 앞면을 못 봤다")

    # 저작 메시와 못 읽은 회전은 크기를 믿을 수 없으므로 아예 빠진다.
    if drawable(_Box((0.0,) * 3, (100.0,) * 3, "M", note="authored"), set()):
        failures.append("저작 메시 상자를 그대로 재고 있다")
    if drawable(_Box((0.0,) * 3, (10.0,) * 3, "M", rotation=(0.0, 33.0, 0.0)),
                set()):
        failures.append("축에 붙지 않은 회전 상자를 그대로 재고 있다")

    # 실제 트리에서 상자가 뽑히고, 숨긴 상자를 실제로 찾아내는지.
    boxes = collect()
    if len(boxes) < 800:
        failures.append("실제 트리에서 상자가 너무 적다 (%d)" % len(boxes))
    hidden_total = sum(len(hidden_lines(path)) for path in geometry.SOURCES)
    if hidden_total < 1:
        failures.append("숨긴 상자를 하나도 못 찾았다")

    for failure in failures:
        print("  자기 검사 실패: " + failure)
    print("COPLANAR SURFACE SELF-TEST " + ("FAIL" if failures else "PASS")
          + " boxes=" + str(len(boxes)) + " hidden=" + str(hidden_total))
    return 1 if failures else 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args(argv)

    if arguments.self_test:
        return self_test()

    boxes = collect()
    findings = audit(boxes)

    if arguments.json:
        print(json.dumps([asdict(item) for item in findings],
                         ensure_ascii=False, indent=2))
    else:
        print("COPLANAR SURFACE AUDIT  boxes=" + str(len(boxes))
              + " findings=" + str(len(findings)))
        for finding in findings:
            print("  [" + finding.code + "] " + finding.label() + "  "
                  + finding.detail)
        if not findings:
            print("\nno two drawn surfaces share a plane")

    return 1 if (arguments.check and findings) else 0


if __name__ == "__main__":
    raise SystemExit(main())
