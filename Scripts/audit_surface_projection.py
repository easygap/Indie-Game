#!/usr/bin/env python3
"""월드 투영 재질이 엉뚱한 방향의 면에 붙어 늘어나는 자리를 찾는다.

건축 재질은 메시 UV가 아니라 월드 좌표를 읽는다. ``create_textured_materials``
가 ``mapping``으로 두 축을 고르고, UV는 그 두 축의 월드 좌표를 타일 크기로
나눈 값이다. 그래서 같은 텍스처가 ``_X``(XZ)·``_Y``(YZ)·``_XY`` 세 벌로
존재한다 — 벽을 늘려도 무늬 밀도가 변하지 않게 하려고 만든 구조다.

문제는 **빠진 축을 따라서는 UV가 전혀 변하지 않는다**는 것이다. XY 재질을
세로 면에 붙이면 위아래로 한 줄이 무한히 늘어나고, XZ 재질을 바닥이나
천장에 붙이면 폭 방향으로 같은 일이 일어난다. 컴파일러도 쿠커도 모르고,
기존 감사 넷도 이것을 보지 않는다 — 이름이 맞는 재질이고, 자리도 맞고,
크기도 맞다. 틀린 것은 **면의 방향** 하나뿐이다.

실제로 그렇게 새어 나갔다. 필로티 천장 7.8 x 1.7 m 전체와 편의점 정면
기둥·상인방, 공동현관 캐노피, 기둥 주두와 굽이 전부 한 줄로 늘어난 채
빌드에 들어가 있었다.

코드만 읽고 두 가지를 본다.

* ``PROJECTION_STRETCH``    - 지배면이 늘어나고, 같은 텍스처의 다른 축 변형이
                              이미 구워져 있다. 그 이름으로 바꾸면 끝난다
* ``PROJECTION_NO_VARIANT`` - 지배면이 늘어나는데 바꿔 낄 변형이 없다.
                              재질을 새로 구워야 하는 자리다

지배면이 뚜렷하지 않은 상자(정사각 기둥처럼 두 방향의 면이 같은 크기인
경우)는 어느 변형을 골라도 절반은 늘어나므로 세지 않는다. 다른 상자에 묻혀
보이지 않는 면도 뺀다 — 계단처럼 앞면이 다음 단에 가려지는 구조가 그렇다.

어디서든 돌아간다.

    python3 Scripts/audit_surface_projection.py             # 사람이 읽는 보고
    python3 Scripts/audit_surface_projection.py --json      # 기계가 읽는 보고
    python3 Scripts/audit_surface_projection.py --check     # 발견되면 exit 1
    python3 Scripts/audit_surface_projection.py --self-test # 감사 자체를 검사
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import sys
from dataclasses import dataclass


PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if os.path.dirname(os.path.abspath(__file__)) not in sys.path:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import audit_world_geometry  # noqa: E402


# 재질 표를 소유한 파일. `unreal`을 import하므로 본문을 읽어서 판다.
MATERIAL_RECIPE = os.path.join("Scripts", "create_textured_materials.py")
MATERIAL_DIR = os.path.join("Content", "Prototype", "Materials")

# 상자를 리터럴 좌표로 짓는 두 빌더. 기하 감사와 같은 목록이다.
SOURCES = (
    os.path.join("Source", "IndieGame", "Core", "IGPrologueWorldScene.cpp"),
    os.path.join("Source", "IndieGame", "Sequence", "IGThirdMorningDirector.cpp"),
)

# mapping이 고르는 두 축과, 그래서 UV가 따라가지 못하는 나머지 한 축.
MISSING_AXIS = {"XY": 2, "XZ": 1, "YZ": 0}
# 면 법선이 그 빠진 축과 이루는 각도. 1.0이면 완전히 정상, 0이면 완전히 늘어난다.
# 0.35는 약 3배 늘어남 — 손전등이 스치면 줄이 보이기 시작하는 지점이다.
MAX_STRETCH_COSINE = 0.35
# 지배면이 다음 면보다 이만큼은 커야 「이 상자의 얼굴」이라고 말할 수 있다.
DOMINANCE_RATIO = 1.6
# 이보다 작은 면은 벽의 마구리나 문선 반턱이다. 그 자리의 늘어남은 이 구조가
# 어디서나 감수하는 것이라 세지 않는다.
MIN_FACE_AREA = 2000.0
MIN_FACE_MINOR = 30.0
# 면 바로 바깥 8 cm 안에 있는 상자는 그 면을 덮는다. 계단처럼 앞면이 다음
# 단에 묻히는 구조에서는 덮이고 남은 띠만 화면에 닿으므로, 그 남은 띠를
# 같은 잣대(넓이와 짧은 변)로 다시 잰다.
OCCLUSION_PROBE = 8.0

AXIS_TO_MAPPING = {0: "YZ", 1: "XZ", 2: "XY"}
MAPPING_SUFFIX = {"YZ": "_Y", "XZ": "_X", "XY": "_XY"}
SUFFIX_PATTERN = re.compile(r"_(X|Y|XY)$")

# "M_Name": { ... "mapping": "XY" ... } — 한 줄짜리와 여러 줄짜리가 섞여 있다.
SPEC_BLOCK = re.compile(r'"(M_[A-Za-z0-9_]+)"\s*:\s*\{(.*?)\n(?:\s{4})?\}', re.S)
SPEC_INLINE = re.compile(r'"(M_[A-Za-z0-9_]+)"\s*:\s*\{([^{}]*)\}')
SPEC_MAPPING = re.compile(r'"mapping"\s*:\s*"(\w+)"')
SPEC_TEXTURE = re.compile(r'"tex"\s*:\s*"(\w+)"')

# 씬이 재질을 잡는 두 관용구. TexMat은 이름으로, 나머지는 에셋 경로로 연다.
MATERIAL_NAME = re.compile(
    r'(?:TexMat\(\s*TEXT\("|"/Game/Prototype/Materials/)(M_[A-Za-z0-9_]+)')
ASSIGNMENT = re.compile(
    r'^\s*(?:UMaterialInterface\s*\*\s*(?:const\s+)?)?'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)$')
IDENTIFIER = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\b')


# ---------------------------------------------------------------------------
# 재질 표
# ---------------------------------------------------------------------------

def parse_material_specs(text: str) -> dict:
    """재질 이름 -> (mapping, 텍스처 이름)."""
    specs = {}
    for match in SPEC_BLOCK.finditer(text):
        mapping = SPEC_MAPPING.search(match.group(2))
        if mapping:
            texture = SPEC_TEXTURE.search(match.group(2))
            specs.setdefault(
                match.group(1),
                (mapping.group(1), texture.group(1) if texture else ""))
    for match in SPEC_INLINE.finditer(text):
        mapping = SPEC_MAPPING.search(match.group(2))
        if mapping:
            texture = SPEC_TEXTURE.search(match.group(2))
            specs[match.group(1)] = (
                mapping.group(1), texture.group(1) if texture else "")
    return specs


def baked_materials(project_root: str) -> set:
    directory = os.path.join(project_root, MATERIAL_DIR)
    if not os.path.isdir(directory):
        return set()
    return {
        os.path.splitext(name)[0]
        for name in os.listdir(directory)
        if name.endswith(".uasset")
    }


def sibling_material(name: str, wanted_mapping: str, specs: dict,
                     baked: set) -> str | None:
    """같은 텍스처를 원하는 축으로 읽는 형제 재질. 없으면 None.

    같은 텍스처를 여러 계열이 나눠 쓴다 — Concrete 하나를 ``M_Concrete_*``,
    ``M_ConcreteDark_*``, ``M_StoreWall_*``가 각자의 틴트로 읽는다. 그래서
    이름 줄기가 같은 것을 먼저 찾는다. 텍스처만 보고 고르면 색이 다른
    재질을 권하게 된다.
    """
    stem = SUFFIX_PATTERN.sub("", name)
    guess = stem + MAPPING_SUFFIX[wanted_mapping]
    if guess in baked and specs.get(guess, (None,))[0] == wanted_mapping:
        return guess

    texture = specs.get(name, ("", ""))[1]
    for candidate, (mapping, candidate_texture) in sorted(specs.items()):
        if mapping != wanted_mapping:
            continue
        if texture and candidate_texture == texture and candidate in baked:
            return candidate
    return None


# ---------------------------------------------------------------------------
# 호출부가 넘긴 재질 표현식 풀기
# ---------------------------------------------------------------------------

def parse_bindings(text: str) -> list:
    """(줄 번호, 이름, 오른쪽 식) 목록. 지역 변수와 멤버를 함께 담는다."""
    bindings = []
    lines = text.split("\n")
    for index, line in enumerate(lines, start=1):
        match = ASSIGNMENT.match(line)
        if not match:
            continue
        name, expression = match.group(1), match.group(2)
        cursor = index
        # 한 문장이 여러 줄에 걸치는 것이 이 코드베이스의 기본형이다.
        while ";" not in expression and cursor < len(lines) and cursor < index + 6:
            expression += " " + lines[cursor].strip()
            cursor += 1
        bindings.append((index, name, expression))
    return bindings


def resolve_material(bindings: list, expression: str, line: int,
                     seen: set | None = None) -> str | None:
    """호출부에 적힌 것이 무엇이든 실제 재질 이름으로 바꾼다."""
    direct = MATERIAL_NAME.search(expression)
    if direct:
        return direct.group(1)

    name = expression.strip()
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
        return None

    seen = set() if seen is None else seen
    if name in seen or len(seen) > 4:
        return None
    seen.add(name)

    latest = None
    for binding_line, binding_name, binding_expression in bindings:
        if binding_name == name and binding_line <= line:
            latest = (binding_line, binding_expression)
    if latest is None:
        return None

    named = MATERIAL_NAME.search(latest[1])
    if named:
        return named.group(1)
    # 삼항이나 다른 변수로 넘긴 자리는 첫 번째로 풀리는 이름을 쓴다.
    for identifier in IDENTIFIER.findall(latest[1]):
        if identifier == name:
            continue
        resolved = resolve_material(bindings, identifier, latest[0], seen)
        if resolved:
            return resolved
    return None


# ---------------------------------------------------------------------------
# 면
# ---------------------------------------------------------------------------

def box_axes(rotation) -> list:
    """상자 로컬 축 세 개의 월드 방향. FRotator 순서는 Roll, Pitch, Yaw다."""
    pitch, yaw, roll = (math.radians(value) for value in rotation)
    cos_p, sin_p = math.cos(pitch), math.sin(pitch)
    cos_y, sin_y = math.cos(yaw), math.sin(yaw)
    cos_r, sin_r = math.cos(roll), math.sin(roll)
    forward = (cos_p * cos_y, cos_p * sin_y, sin_p)
    right = (
        sin_r * sin_p * cos_y - cos_r * sin_y,
        sin_r * sin_p * sin_y + cos_r * cos_y,
        -sin_r * cos_p,
    )
    up = (
        -(cos_r * sin_p * cos_y + sin_r * sin_y),
        -(cos_r * sin_p * sin_y - sin_r * cos_y),
        cos_r * cos_p,
    )
    return [forward, right, up]


def dominant_face(size, rotation):
    """(면적, 법선, 짧은 변, 로컬 축 번호, 다음 면과의 비)."""
    dimensions = list(size)
    axes = box_axes(rotation)
    faces = []
    for index in range(3):
        first = dimensions[(index + 1) % 3]
        second = dimensions[(index + 2) % 3]
        faces.append((first * second, axes[index], min(first, second), index))
    faces.sort(key=lambda face: face[0], reverse=True)
    runner_up = faces[1][0]
    ratio = math.inf if runner_up <= 0.0 else faces[0][0] / runner_up
    return faces[0][0], faces[0][1], faces[0][2], faces[0][3], ratio


def rotation_tuple(rotation) -> tuple:
    """스캐너는 회전을 Rot3로도 평범한 튜플로도 돌려준다."""
    if hasattr(rotation, "as_tuple"):
        return rotation.as_tuple()
    return tuple(rotation)


def _axis_aligned(rotation) -> bool:
    return all(abs(value) < 1.0e-6 for value in rotation)


def _overlap(low_a, high_a, low_b, high_b) -> float:
    return max(0.0, min(high_a, high_b) - max(low_a, low_b))


def face_exposure(box, neighbours, axis: int) -> tuple:
    """(드러난 넓이, 드러난 띠의 짧은 변). 양면 중 더 많이 드러난 쪽을 쓴다."""
    half = [value * 0.5 for value in box.size]
    centre = list(box.center)
    lateral = [index for index in range(3) if index != axis]
    spans = [box.size[index] for index in lateral]
    face_area = spans[0] * spans[1]
    if face_area <= 0.0 or not _axis_aligned(rotation_tuple(box.rotation)):
        return face_area, min(spans) if spans else 0.0

    best = (0.0, 0.0)
    for direction in (-1.0, 1.0):
        plane = centre[axis] + direction * half[axis]
        probe_low = min(plane, plane + direction * OCCLUSION_PROBE)
        probe_high = max(plane, plane + direction * OCCLUSION_PROBE)
        covered_area = 0.0
        covered_spans = [0.0, 0.0]
        for other in neighbours:
            if other is box or other.frame != box.frame:
                continue
            if not _axis_aligned(rotation_tuple(other.rotation)):
                continue
            other_half = [value * 0.5 for value in other.size]
            depth = _overlap(
                probe_low, probe_high,
                other.center[axis] - other_half[axis],
                other.center[axis] + other_half[axis])
            if depth <= 0.0:
                continue
            overlaps = []
            for position, index in enumerate(lateral):
                overlaps.append(_overlap(
                    centre[index] - half[index], centre[index] + half[index],
                    other.center[index] - other_half[index],
                    other.center[index] + other_half[index]))
            area = overlaps[0] * overlaps[1]
            if area > covered_area:
                covered_area = area
                covered_spans = overlaps
        exposed_area = max(0.0, face_area - covered_area)
        # 덮개가 한 축을 통째로 가리면 남는 것은 반대 축의 띠다. 그렇지
        # 않으면 긴 변으로 나눠 띠 폭을 낮게 잡는다 — 봐주는 쪽으로 틀린다.
        divisor = max(spans)
        for position, span in enumerate(spans):
            if covered_spans[position] >= span - 0.5:
                divisor = span
                break
        strip = exposed_area / divisor if divisor > 0.0 else 0.0
        if exposed_area > best[0]:
            best = (exposed_area, min(strip, min(spans)))
    return best


# ---------------------------------------------------------------------------
# 감사
# ---------------------------------------------------------------------------

@dataclass
class Finding:
    code: str
    source: str
    line: int
    function: str
    material: str
    mapping: str
    wanted: str
    replacement: str
    face_area: float
    stretch: float
    centre: tuple
    size: tuple

    def to_dict(self) -> dict:
        return {
            "code": self.code,
            "site": f"{self.source}:{self.line}",
            "function": self.function,
            "material": self.material,
            "mapping": self.mapping,
            "wanted": self.wanted,
            "replacement": self.replacement,
            "face_area_cm2": round(self.face_area, 1),
            "stretch_cosine": round(self.stretch, 3),
            "centre": [round(value, 2) for value in self.centre],
            "size": [round(value, 2) for value in self.size],
        }


def audit_boxes(boxes, bindings, specs, baked, source_label):
    """한 빌더의 상자 목록을 본다. (findings, 통계)."""
    findings = []
    counts = {"planar": 0, "unresolved": 0, "uv_or_prop": 0, "hidden": 0,
              "no_dominant": 0}
    for box in boxes:
        expression = (box.material or "").strip()
        name = resolve_material(bindings, expression, box.line)
        if not name:
            counts["unresolved"] += 1
            continue
        mapping = specs.get(name, (None, None))[0]
        if mapping is None or mapping == "UV":
            counts["uv_or_prop"] += 1
            continue
        counts["planar"] += 1

        area, normal, minor, axis, ratio = dominant_face(
            box.size, rotation_tuple(box.rotation))
        if ratio < DOMINANCE_RATIO:
            counts["no_dominant"] += 1
            continue
        if area < MIN_FACE_AREA or minor < MIN_FACE_MINOR:
            continue
        stretch = abs(normal[MISSING_AXIS[mapping]])
        if stretch >= MAX_STRETCH_COSINE:
            continue
        exposed_area, exposed_strip = face_exposure(box, boxes, axis)
        if exposed_area < MIN_FACE_AREA or exposed_strip < MIN_FACE_MINOR:
            counts["hidden"] += 1
            continue
        area = exposed_area

        wanted = AXIS_TO_MAPPING[axis]
        replacement = sibling_material(name, wanted, specs, baked)
        findings.append(Finding(
            code="PROJECTION_STRETCH" if replacement else "PROJECTION_NO_VARIANT",
            source=source_label,
            line=box.line,
            function=box.function,
            material=name,
            mapping=mapping,
            wanted=wanted,
            replacement=replacement or "",
            face_area=area,
            stretch=stretch,
            centre=tuple(box.center),
            size=tuple(box.size),
        ))
    return findings, counts


def run(project_root: str):
    recipe_path = os.path.join(project_root, MATERIAL_RECIPE)
    with open(recipe_path, "r", encoding="utf-8") as handle:
        specs = parse_material_specs(handle.read())
    baked = baked_materials(project_root)

    findings = []
    totals = {"planar": 0, "unresolved": 0, "uv_or_prop": 0, "hidden": 0,
              "no_dominant": 0, "boxes": 0}
    for relative in SOURCES:
        absolute = os.path.join(project_root, relative)
        with open(absolute, "r", encoding="utf-8") as handle:
            bindings = parse_bindings(handle.read())
        scan = audit_world_geometry.scan_source(relative)
        label = relative.replace("\\", "/")
        found, counts = audit_boxes(scan.boxes, bindings, specs, baked, label)
        findings.extend(found)
        totals["boxes"] += len(scan.boxes)
        for key, value in counts.items():
            totals[key] += value
    findings.sort(key=lambda finding: (-finding.face_area, finding.line))
    return specs, totals, findings


# ---------------------------------------------------------------------------
# 자기 검사. 아무것도 못 찾는 감사와 통과하는 트리는 화면에서 똑같이 보인다.
# ---------------------------------------------------------------------------

SELF_TEST_RECIPE = '''
SPECS = {
    "M_Demo_X":  {"tex": "Demo", "mapping": "XZ", "tile": 150.0},
    "M_Demo_Y":  {"tex": "Demo", "mapping": "YZ", "tile": 150.0},
    "M_Demo_XY": {"tex": "Demo", "mapping": "XY", "tile": 150.0},
    "M_Lonely_XY": {
        "tex": "Lonely", "mapping": "XY", "tile": 60.0,
        "rough": 0.5,
    },
    "M_PropUV":  {"tex": "Demo", "mapping": "UV", "tile": 1.0},
}
'''


class _FakeRotation:
    def __init__(self, values=(0.0, 0.0, 0.0)):
        self.values = tuple(values)

    def as_tuple(self):
        return self.values


class _FakeBox:
    def __init__(self, line, material, centre, size, rotation=(0.0, 0.0, 0.0)):
        self.line = line
        self.material = material
        self.center = tuple(centre)
        self.size = tuple(size)
        self.rotation = _FakeRotation(rotation)
        self.function = "SelfTest"
        self.frame = "SceneRoot"
        # 스캐너가 저작 메시에 붙이는 표식. 인쇄면 감사가 이것을 읽는다.
        self.note = ""


def _self_test() -> int:
    failures = []

    def check(label, actual, expected):
        if actual != expected:
            failures.append(f"{label}: {actual!r} != {expected!r}")

    specs = parse_material_specs(SELF_TEST_RECIPE)
    check("한 줄 표", specs.get("M_Demo_X"), ("XZ", "Demo"))
    check("여러 줄 표", specs.get("M_Lonely_XY"), ("XY", "Lonely"))
    check("UV 표", specs.get("M_PropUV"), ("UV", "Demo"))

    bindings = parse_bindings(
        '\tUMaterialInterface* Soffit = TexMat(TEXT("M_Demo_X"), Fallback);\n'
        '\tUMaterialInterface* Alias = Soffit;\n'
        '\tWallMember = FindMaterial(\n'
        '\t\tTEXT("/Game/Prototype/Materials/M_Demo_Y.M_Demo_Y"));\n'
        '\tUMaterialInterface* Prop = TexMat(TEXT("M_PropUV"), Fallback);\n')
    check("지역 변수", resolve_material(bindings, "Soffit", 9), "M_Demo_X")
    check("별칭", resolve_material(bindings, "Alias", 9), "M_Demo_X")
    # 여러 줄에 걸친 멤버 대입도 같은 대입이다.
    check("멤버", resolve_material(bindings, "WallMember", 9), "M_Demo_Y")
    # 선언 전의 호출부는 그 이름을 아직 모른다.
    check("선언 순서", resolve_material(bindings, "Soffit", 0), None)
    check("모르는 이름", resolve_material(bindings, "LoopVariable", 9), None)
    check("직접 호출",
          resolve_material(bindings, 'TexMat(TEXT("M_Demo_XY"), F)', 9),
          "M_Demo_XY")

    baked = {"M_Demo_X", "M_Demo_Y", "M_Demo_XY", "M_Lonely_XY", "M_PropUV"}

    # 7.8 x 1.7 m 소핏에 XZ 재질 — 필로티 천장이 그랬다.
    soffit = _FakeBox(1, "Soffit", (0, 0, 244), (780, 170, 12))
    findings, counts = audit_boxes([soffit], bindings, specs, baked, "Fake.cpp")
    check("소핏 코드", [f.code for f in findings], ["PROJECTION_STRETCH"])
    check("소핏 교체", findings[0].replacement, "M_Demo_XY")
    check("평면 재질 수", counts["planar"], 1)

    # 같은 상자에 맞는 축을 주면 조용해야 한다.
    ceiling = _FakeBox(1, 'TexMat(TEXT("M_Demo_XY"), F)', (0, 0, 244),
                       (780, 170, 12))
    findings, _ = audit_boxes([ceiling], bindings, specs, baked, "Fake.cpp")
    check("맞는 축", findings, [])

    # 프롭 재질은 메시 UV를 쓰므로 방향과 무관하다.
    prop = _FakeBox(9, "Prop", (0, 0, 100), (780, 170, 12))
    findings, counts = audit_boxes([prop], bindings, specs, baked, "Fake.cpp")
    check("UV 재질", (findings, counts["uv_or_prop"]), ([], 1))

    # 정사각 기둥은 어느 변형을 골라도 절반이 늘어난다. 세지 않는다.
    column = _FakeBox(1, "Soffit", (0, 0, 118), (38, 38, 236))
    findings, counts = audit_boxes([column], bindings, specs, baked, "Fake.cpp")
    check("지배면 없음", (findings, counts["no_dominant"]), ([], 1))

    # 벽 마구리처럼 좁은 면의 늘어남은 이 구조가 어디서나 감수한다.
    reveal = _FakeBox(1, "Soffit", (0, 0, 120), (10, 20, 240))
    findings, _ = audit_boxes([reveal], bindings, specs, baked, "Fake.cpp")
    check("좁은 면", findings, [])

    # 앞뒤로 완전히 묻힌 면은 화면에 닿지 않는다.
    step = _FakeBox(1, "Soffit", (0, 0, 50), (60, 200, 100))
    front = _FakeBox(2, "Soffit", (60, 0, 50), (60, 200, 100))
    back = _FakeBox(3, "Soffit", (-60, 0, 50), (60, 200, 100))
    findings, counts = audit_boxes(
        [step, front, back], bindings, specs, baked, "Fake.cpp")
    check("묻힌 면", [f.line for f in findings], [2, 3])
    check("묻힘 카운트", counts["hidden"], 1)

    # 바닥에서 자란 계단은 아래 단이 챌면 대부분을 덮는다. 가운데 단에
    # 남는 것은 17 cm 띠뿐이고, 그 폭의 늘어남은 좁은 마구리와 같은 급이라
    # 세지 않는다. 양 끝 단은 한쪽이 통째로 드러나므로 그대로 걸린다.
    tread = 'TexMat(TEXT("M_Demo_XY"), F)'
    lower = _FakeBox(1, tread, (0, -22, 36.5), (200, 22, 73))
    middle = _FakeBox(2, tread, (0, 0, 45), (200, 22, 90))
    upper = _FakeBox(3, tread, (0, 22, 53.5), (200, 22, 107))
    findings, counts = audit_boxes(
        [lower, middle, upper], bindings, specs, baked, "Fake.cpp")
    check("자란 계단", sorted(f.line for f in findings), [1, 3])
    check("계단 띠 카운트", counts["hidden"], 1)

    # 아무것도 덮지 않으면 같은 상자가 그대로 걸린다.
    lone = _FakeBox(1, tread, (0, 0, 60), (200, 22, 120))
    findings, _ = audit_boxes([lone], bindings, specs, baked, "Fake.cpp")
    check("드러난 계단", [f.code for f in findings], ["PROJECTION_STRETCH"])

    # 바꿔 낄 변형이 없으면 이름이 아니라 없다는 사실을 찍는다.
    lonely = _FakeBox(1, 'TexMat(TEXT("M_Lonely_XY"), F)', (0, 0, 120),
                      (12, 200, 240))
    findings, _ = audit_boxes([lonely], bindings, specs, baked, "Fake.cpp")
    check("변형 없음",
          [(f.code, f.replacement) for f in findings],
          [("PROJECTION_NO_VARIANT", "")])

    # 못 푼 재질을 통과로 세면 안 된다.
    unknown = _FakeBox(1, "LoopVariable", (0, 0, 244), (780, 170, 12))
    findings, counts = audit_boxes([unknown], bindings, specs, baked, "Fake.cpp")
    check("못 푼 재질", (findings, counts["unresolved"]), ([], 1))

    if failures:
        for failure in failures:
            print(f"  FAIL {failure}")
        print(f"SURFACE PROJECTION AUDIT SELF-TEST FAIL  {len(failures)} case(s)")
        return 1
    print("PASS surface projection audit self-test: recipe table, local and "
          "member bindings, both codes on and off, square columns, narrow "
          "reveals, buried faces and unresolved materials")
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="발견 항목이 있으면 exit 1")
    parser.add_argument("--json", action="store_true",
                        help="기계가 읽는 보고")
    parser.add_argument("--self-test", action="store_true",
                        help="감사 자체를 합성 입력으로 검사한다")
    arguments = parser.parse_args(argv)

    if arguments.self_test:
        return _self_test()

    specs, totals, findings = run(PROJECT_ROOT)

    if arguments.json:
        print(json.dumps({
            "materials": len(specs),
            "boxes": totals["boxes"],
            "planar": totals["planar"],
            "unresolved": totals["unresolved"],
            "hidden": totals["hidden"],
            "findings": [finding.to_dict() for finding in findings],
        }, indent=2, ensure_ascii=False))
    else:
        print(f"SURFACE PROJECTION AUDIT  boxes={totals['boxes']} "
              f"planar={totals['planar']} hidden={totals['hidden']} "
              f"unresolved={totals['unresolved']} findings={len(findings)}")
        if totals["unresolved"]:
            print(f"  재질을 풀지 못한 상자 {totals['unresolved']}건 — "
                  f"호출부가 리터럴도 지역 변수도 아니었다")
        if not findings:
            print("\nevery world-projected surface faces the axes its "
                  "material reads")
        else:
            print()
            for finding in findings:
                print(f"  [{finding.code}] {finding.source}:{finding.line} "
                      f"{finding.function}")
                target = finding.replacement or f"{finding.wanted} 변형 없음"
                print(f"      {finding.material}({finding.mapping}) -> {target}")
                print(f"      지배면 {finding.face_area:.0f} cm2, "
                      f"늘어남 {1.0 / max(finding.stretch, 1.0e-3):.0f}배, "
                      f"중심 {tuple(round(v, 1) for v in finding.centre)}, "
                      f"크기 {tuple(round(v, 1) for v in finding.size)}")

    return 1 if (arguments.check and findings) else 0


if __name__ == "__main__":
    raise SystemExit(main())
