"""걸어 다니는 면에 발소리 표면 태그가 붙어 있는지 본다.

플레이어는 발밑을 라인 트레이스해서 Footstep.* 컴포넌트 태그를 읽는다.
태그가 없으면 콘크리트로 떨어지는데, 이 값은 소리만 정하는 게 아니라
UIGMissingFloorAudioSubsystem::ClassifyAcousticSpace가 반향 공간까지 같이
고른다. 그래서 옥상 슬래브 하나에 태그를 빼먹으면 탁 트인 옥상이 실내
복도 반향으로 울린다. 눈으로는 절대 안 보이는 종류의 결함이다.

세 가지를 본다.

1. FOOTSTEP_ORPHAN_SURFACE — 플레이어가 구분할 줄 아는 표면인데 월드
   어디에서도 그 태그를 붙이지 않는다. 침수 복도에서 물 발소리가 빠져
   있던 것이 이 모양이었다.
2. FOOTSTEP_ROUTE_UNTAGGED / FOOTSTEP_ROUTE_WRONG_TAG — 셋째 날 아침
   디렉터는 자기 보행 경로를 ValidateRebirthCollisionRoute에 좌표로 적어
   둔다. 그 점 아래에 있는 상자를 찾아, 재질이 콘크리트가 아닌 것들이
   제 태그를 달고 있는지 본다.
3. FOOTSTEP_TAG_NO_COLLISION — 태그가 충돌 없는 장식판에 붙었다. 발밑
   트레이스가 닿지 않으므로 붙여 봐야 아무 일도 일어나지 않는다.

사용법:
    python Scripts/audit_footstep_surfaces.py            # 사람이 읽는 표
    python Scripts/audit_footstep_surfaces.py --json
    python Scripts/audit_footstep_surfaces.py --check    # 발견 시 종료 코드 1
    python Scripts/audit_footstep_surfaces.py --self-test
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

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PLAYER_SOURCE = os.path.join(
    "Source", "IndieGame", "Player", "IGPlayerCharacter.cpp")
ROUTE_SOURCE = os.path.join(
    "Source", "IndieGame", "Sequence", "IGThirdMorningDirector.cpp")
ROUTE_FUNCTION = "ValidateRebirthCollisionRoute"

# 재질 이름에서 표면을 읽는다. 콘크리트 계열은 일부러 넣지 않는다 —
# 태그가 없을 때의 기본값이고, 옥상 슬래브처럼 콘크리트로 만든 물건이
# 옥상 발소리를 갖는 것도 정상이기 때문이다.
MATERIAL_SURFACES = (
    ("jangpan", "Vinyl"),
    ("roomfloor", "Vinyl"),
    ("rooftopwaterproofing", "Rooftop"),
    ("roofdeck", "Rooftop"),
    ("stairsteel", "MetalStair"),
    ("steelstair", "MetalStair"),
    ("tankmetal", "MetalStair"),
    ("gypsumdebris", "GypsumDebris"),
)

# 경로 표본의 Z와 상자 윗면이 이만큼까지 어긋나도 같은 면으로 본다.
SURFACE_Z_TOLERANCE = 2.0
# 상자를 만든 줄에서 몇 줄 뒤까지를 그 상자에 태그를 붙이는 자리로 볼지.
TAG_LOOKAHEAD_LINES = 40

TAG_PATTERN = re.compile(r'"Footstep\.(?P<surface>\w+)"')
TAG_CONSTANT = re.compile(
    r"\bconst\s+FName\s+(?P<name>\w+)\s*\(\s*TEXT\(\s*\"Footstep\."
    r"(?P<surface>\w+)\"\s*\)\s*\)")
TAG_HELPER_CALL = re.compile(r"\bTagFootstepSurface\s*\(")
TAG_DIRECT_CALL = re.compile(
    r"(?P<target>\w+)\s*->\s*ComponentTags\s*\.\s*Add(?:Unique)?\s*\(")
ASSIGNED_BLOCK = re.compile(
    r"(?P<name>\w+)\s*=\s*(?:CreateBlock|CreatePrintedBlock)\s*\(")
FLOOR_SAMPLE = re.compile(r"\bFloorSamples\s*\.\s*Add\s*\(")


@dataclass
class Finding:
    code: str
    source: str
    line: int
    detail: str

    def label(self) -> str:
        return os.path.basename(self.source) + ":" + str(self.line)


def read(relative_path: str) -> str:
    with open(os.path.join(ROOT, relative_path), encoding="utf-8-sig") as handle:
        return handle.read()


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def surface_constants(text: str) -> dict:
    """const FName FootstepVinylTag(TEXT("Footstep.Vinyl")) 표를 만든다."""
    return {
        match.group("name"): match.group("surface")
        for match in TAG_CONSTANT.finditer(text)
    }


def resolve_surface(argument: str, constants: dict):
    literal = TAG_PATTERN.search(argument)
    if literal:
        return literal.group("surface")
    bare = argument.strip().split("::")[-1].strip()
    return constants.get(bare)


def material_surface(material: str):
    lowered = material.lower()
    for hint, surface in MATERIAL_SURFACES:
        if hint in lowered:
            return surface
    return None


def is_standable(box) -> bool:
    """발밑 트레이스가 닿을 수 있는 상자인가.

    기하로 「바닥처럼 생겼는가」를 묻지 않는다. 계단 한 단은 밑면을 바닥까지
    끌어내려 만들기 때문에 위로 갈수록 벽처럼 길쭉해지는데, 그래도 사람이
    밟고 서는 면이다. 실제로 트레이스가 못 읽는 경우는 충돌이 없을 때다.
    """
    return bool(box.collision)


def collect_tag_sites(text: str, constants: dict) -> list:
    """붙은 태그를 전부 모은다. 감싸는 형태와 변수 형태 둘 다 쓴다."""
    sites = []
    for match in TAG_HELPER_CALL.finditer(text):
        call, end = geometry.match_call(text, match.end() - 1)
        arguments = geometry.split_arguments(call)
        if len(arguments) != 2:
            continue
        surface = resolve_surface(arguments[1], constants)
        if surface is None:
            continue
        sites.append({
            "surface": surface,
            "target": arguments[0].strip(),
            "first_line": line_of(text, match.start()),
            "last_line": line_of(text, end),
        })
    for match in TAG_DIRECT_CALL.finditer(text):
        call, _end = geometry.match_call(text, match.end() - 1)
        surface = resolve_surface(call, constants)
        if surface is None:
            continue
        line = line_of(text, match.start())
        sites.append({
            "surface": surface,
            "target": match.group("target"),
            "first_line": line,
            "last_line": line,
        })
    return sites


def assigned_name(text: str, line: int):
    """그 줄에서 CreateBlock 결과를 받는 변수 이름."""
    lines = text.split("\n")
    for index in range(max(0, line - 2), min(len(lines), line + 1)):
        match = ASSIGNED_BLOCK.search(lines[index])
        if match:
            return match.group("name")
    return None


def box_surface(box, text: str, sites: list):
    name = assigned_name(text, box.line)
    for site in sites:
        if site["first_line"] <= box.line <= site["last_line"]:
            return site["surface"]
        if (name and site["target"] == name
                and box.line <= site["first_line"]
                <= box.line + TAG_LOOKAHEAD_LINES):
            return site["surface"]
    return None


def function_span(text: str, name: str):
    match = re.search(r"\b\w+\s+\w+::" + name + r"\s*\(", text)
    if not match:
        return None
    # 인자 목록을 먼저 건너뛴다. 여는 중괄호는 그다음에야 나온다.
    _call, after_parameters = geometry.match_call(text, match.end() - 1)
    tail = re.match(r"[^{;]*", text[after_parameters:])
    return geometry._block_after(text, after_parameters + tail.end())


def route_samples(text: str) -> list:
    """셋째 날 아침 디렉터가 스스로 적어 둔 보행 경로 표본."""
    span = function_span(text, ROUTE_FUNCTION)
    if span is None:
        return []
    scope = geometry._file_scope_constants(text)
    loops = []
    for match in geometry.COUNT_FOR.finditer(text, span[0], span[1]):
        block = geometry._block_after(text, match.end())
        if block:
            loops.append((match, block))

    def parse_region(start: int, end: int, bindings: dict, skip_loops: bool):
        found = []
        for match in FLOOR_SAMPLE.finditer(text, start, end):
            if skip_loops and any(
                    low <= match.start() < high for _, (low, high) in loops):
                continue
            call, _end = geometry.match_call(text, match.end() - 1)
            inner = call.strip()
            if inner.startswith("{") and inner.endswith("}"):
                inner = inner[1:-1]
            arguments = geometry.split_arguments(inner)
            if len(arguments) != 2:
                continue
            merged = dict(scope)
            merged.update(bindings)
            # 루프 안에서 선언한 지역 상수(StepTopZ 같은 것)를 먼저 푼다.
            for local in geometry.CONST_FLOAT.finditer(text, start, end):
                try:
                    merged[local.group("name")] = geometry.evaluate(
                        local.group("value"), merged)
                except geometry.Unresolved:
                    continue
            try:
                position = geometry.as_vec(
                    geometry.evaluate(arguments[0], merged))
                surface_z = float(geometry.evaluate(arguments[1], merged))
            except (geometry.Unresolved, TypeError, ValueError):
                continue
            found.append((position.x, position.y, surface_z))
        return found

    samples = list(parse_region(span[0], span[1], {}, True))
    for match, (low, high) in loops:
        try:
            limit = int(geometry.evaluate(match.group("limit"), scope))
            init = int(geometry.evaluate(match.group("init"), scope))
        except (geometry.Unresolved, TypeError, ValueError):
            continue
        for value in range(init, limit):
            samples.extend(
                parse_region(low, high, {match.group("name"): value}, False))
    return samples


def audit() -> list:
    findings = []

    player_text = read(PLAYER_SOURCE)
    resolved_surfaces = {
        match.group("surface") for match in TAG_PATTERN.finditer(player_text)}

    applied = {}
    per_source = {}
    for relative_path in geometry.SOURCES:
        text = read(relative_path)
        constants = surface_constants(text)
        sites = collect_tag_sites(text, constants)
        result = geometry.scan_source(relative_path)
        per_source[relative_path] = (text, sites, result.boxes)
        for site in sites:
            applied.setdefault(site["surface"], relative_path)

    for surface in sorted(resolved_surfaces):
        # 콘크리트는 태그 없는 면의 기본값이라 아무도 붙이지 않아도 된다.
        if surface == "Concrete" or surface in applied:
            continue
        findings.append(Finding(
            "FOOTSTEP_ORPHAN_SURFACE",
            PLAYER_SOURCE,
            line_of(player_text, player_text.index('"Footstep.' + surface + '"')),
            "플레이어는 " + surface + " 표면을 구분하는데 월드 어디에서도 "
            "이 태그를 붙이지 않는다"))

    for relative_path, (text, sites, boxes) in per_source.items():
        by_line = {}
        for box in boxes:
            by_line.setdefault(box.line, box)
        for site in sites:
            box = None
            for line in range(site["first_line"], site["last_line"] + 1):
                if line in by_line:
                    box = by_line[line]
                    break
            if box is None:
                for candidate in boxes:
                    if (candidate.line <= site["first_line"]
                            <= candidate.line + TAG_LOOKAHEAD_LINES
                            and assigned_name(text, candidate.line)
                            == site["target"]):
                        box = candidate
                        break
            if box is not None and not is_standable(box):
                findings.append(Finding(
                    "FOOTSTEP_TAG_NO_COLLISION",
                    relative_path,
                    site["first_line"],
                    site["surface"] + " 태그가 충돌 없는 상자에 붙어 있다 "
                    "(크기 " + str(tuple(round(v, 1) for v in box.size))
                    + ") — 발밑 트레이스가 닿지 않는다"))

    text, sites, boxes = per_source[ROUTE_SOURCE]
    floor_boxes = [box for box in boxes if is_standable(box)]
    for x, y, surface_z in route_samples(text):
        best = None
        for box in floor_boxes:
            minimum, maximum = box.minimum, box.maximum
            if not (minimum[0] <= x <= maximum[0]
                    and minimum[1] <= y <= maximum[1]):
                continue
            if abs(maximum[2] - surface_z) > SURFACE_Z_TOLERANCE:
                continue
            if best is None or box.maximum[2] > best.maximum[2]:
                best = box
        if best is None:
            continue
        expected = material_surface(best.material)
        if expected is None:
            continue
        actual = box_surface(best, text, sites)
        where = "(%.0f, %.0f, %.0f)" % (x, y, surface_z)
        if actual is None:
            findings.append(Finding(
                "FOOTSTEP_ROUTE_UNTAGGED",
                ROUTE_SOURCE,
                best.line,
                where + "에서 밟는 면인데 " + best.material + "이 "
                + expected + " 태그를 달고 있지 않다"))
        elif actual != expected:
            findings.append(Finding(
                "FOOTSTEP_ROUTE_WRONG_TAG",
                ROUTE_SOURCE,
                best.line,
                where + "의 " + best.material + "은 " + expected
                + "여야 하는데 " + actual + " 태그가 붙어 있다"))

    unique = {}
    for finding in findings:
        unique[(finding.code, finding.source, finding.line, finding.detail)] = \
            finding
    return sorted(unique.values(), key=lambda item: (item.source, item.line))


def self_test() -> int:
    failures = []

    constants = surface_constants(
        'const FName FootstepVinylTag(TEXT("Footstep.Vinyl"));')
    if constants.get("FootstepVinylTag") != "Vinyl":
        failures.append("FName 상수 표를 못 읽었다")

    if resolve_surface("IGPrologueWorld::FootstepVinylTag",
                       {"FootstepVinylTag": "Vinyl"}) != "Vinyl":
        failures.append("네임스페이스 붙은 상수를 못 풀었다")
    if resolve_surface('TEXT("Footstep.Water")', {}) != "Water":
        failures.append("문자열 리터럴 태그를 못 읽었다")

    if material_surface("TankMetal") != "MetalStair":
        failures.append("물탱크 강판을 철계단으로 못 읽었다")
    if material_surface("ConcreteMaterial") is not None:
        failures.append("콘크리트는 기대 표면이 없어야 한다")
    if material_surface(
            'TexMat(TEXT("M_RooftopWaterproofing_XY"), RoofFloor)') != "Rooftop":
        failures.append("옥상 방수면을 못 읽었다")

    class _Box:
        def __init__(self, size, collision=True):
            self.size = size
            self.collision = collision

    if not is_standable(_Box((700.0, 440.0, 20.0))):
        failures.append("바닥 슬래브를 밟을 수 있는 면으로 못 봤다")
    if not is_standable(_Box((22.0, 105.0, 360.0))):
        failures.append("밑면을 끌어내린 계단 한 단을 빠뜨렸다")
    if is_standable(_Box((700.0, 440.0, 20.0), collision=False)):
        failures.append("충돌 없는 판을 밟을 수 있는 면으로 봤다")

    wrapped = (
        "\tIGThirdMorning::TagFootstepSurface(\n"
        "\t\tCreateBlock(FVector(0, 0, -10), FVector(700, 440, 20), Floor),\n"
        "\t\tIGThirdMorning::FootstepVinylTag);\n")
    sites = collect_tag_sites(wrapped, {"FootstepVinylTag": "Vinyl"})
    if len(sites) != 1 or sites[0]["surface"] != "Vinyl":
        failures.append("감싼 형태의 태그를 못 찾았다")
    elif not (sites[0]["first_line"] <= 2 <= sites[0]["last_line"]):
        failures.append("감싼 형태의 줄 범위가 상자를 덮지 않는다")

    variable = (
        "\tif (UStaticMeshComponent* FloodFloor = CreateBlock(\n"
        "\t\t\tFVector(675, 0, -10), FVector(650, 440, 20), Concrete))\n"
        "\t{\n"
        "\t\tFloodFloor->ComponentTags.AddUnique(FootstepWaterTag);\n"
        "\t}\n")
    sites = collect_tag_sites(variable, {"FootstepWaterTag": "Water"})
    if len(sites) != 1 or sites[0]["target"] != "FloodFloor":
        failures.append("변수 형태의 태그를 못 찾았다")
    if assigned_name(variable, 1) != "FloodFloor":
        failures.append("CreateBlock 결과를 받는 변수를 못 읽었다")

    samples = route_samples(read(ROUTE_SOURCE))
    if len(samples) < 30:
        failures.append("보행 경로 표본이 너무 적다 (" + str(len(samples)) + ")")
    if not any(abs(z - 620.0) < 0.5 for _, _, z in samples):
        failures.append("물탱크 상부 플랫폼 표본이 빠졌다")
    if not any(abs(x - 1915.0) < 0.5 and abs(z - 260.0) < 0.5
               for x, _, z in samples):
        failures.append("철계단 첫 단 표본이 빠졌다 (루프를 못 펼쳤다)")

    for failure in failures:
        print("  자기 검사 실패: " + failure)
    print("FOOTSTEP SELF-TEST " + ("FAIL" if failures else "PASS")
          + " samples=" + str(len(samples)))
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
        print("FOOTSTEP SURFACE AUDIT findings=" + str(len(findings)))
    return 1 if (arguments.check and findings) else 0


if __name__ == "__main__":
    raise SystemExit(main())
