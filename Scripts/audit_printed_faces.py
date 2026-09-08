#!/usr/bin/env python3
"""인쇄 재질이 두꺼운 몸통에 통째로 발려 옆면에도 찍히는 자리를 찾는다.

간판·명판·게시물은 월드 좌표가 아니라 **메시 UV**를 읽는다. 그런데
``CreateBlock``이 쓰는 엔진 기본 큐브는 여섯 면이 같은 UV를 쓴다. 그래서
두께가 있는 상자에 인쇄 재질을 그대로 주면 정면뿐 아니라 옆면·윗면·뒷면에도
같은 그림이 눌려 한 번 더 찍힌다.

실제로 그렇게 나가 있었다. 복도 소화전함은 9 cm 마구리에 「소화전」을 한 번
더 달고 있었고, 계량기함 여섯 면에는 「분전반」이 세로로 눌려 있었으며,
편의점 3.2 m 파사드는 밑면 12 cm에 상호를 한 줄 더 깔고 있었다.

고치는 방법은 하나뿐이다 — 몸통은 민무늬로 두고 인쇄는 앞면에 얇은 판으로
따로 붙인다. ``AIGPrologueWorldScene::CreatePrintedBlock``이 그 일을 한다.

코드만 읽고 하나를 본다.

* ``PRINT_ON_EDGES`` - 엔진 큐브에 인쇄 재질이 통째로 발렸고, 몸통이 옆면을
                      마구리로 넘길 만큼 두껍다

두께가 ``MAX_DRESSING_THICKNESS`` 이하인 판은 이 씬이 게시물·명판·가격표에
쓰는 관례이므로 세지 않는다. 그 정도 마구리는 두 번째 인쇄로 읽히지 않는다.
저작 메시(``PropMesh``로 구운 것)는 자기 UV를 들고 있으므로 대상이 아니다.

어디서든 돌아간다.

    python3 Scripts/audit_printed_faces.py             # 사람이 읽는 보고
    python3 Scripts/audit_printed_faces.py --json      # 기계가 읽는 보고
    python3 Scripts/audit_printed_faces.py --check     # 발견되면 exit 1
    python3 Scripts/audit_printed_faces.py --self-test # 감사 자체를 검사
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass


PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if os.path.dirname(os.path.abspath(__file__)) not in sys.path:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import audit_world_geometry  # noqa: E402
import audit_surface_projection as projection  # noqa: E402


MATERIAL_RECIPE = projection.MATERIAL_RECIPE
SOURCES = projection.SOURCES

# 메시 UV로 곧장 굽는 두 표. 앞의 것은 포스터·명판·기기 도장면이고 뒤의
# 것은 발광 간판이다.
PRINT_TABLES = ("DECAL_MATERIALS", "SIGN_MATERIALS")

# 위 표에 있지만 인쇄가 아니라 **표면 스캔**인 것들. 천·아연도 강판·물때·
# 종이 지질처럼 앞뒤가 없는 재질이라 여섯 면에 같이 나오는 것이 정상이다.
# 여기 든 것은 전부 그 성질 때문에 든 것이고, 자리가 불편해서 든 것은 없다.
UNIFORM_SURFACES = frozenset({
    "M_AlleyCatTabbyUV",
    "M_ApartmentWallPatina",
    "M_MissingFloorListenerPlasterUV",
    "M_MovingBoxCardboardUV",
    "M_P3CabinetMetalUV",
    # 읽는 종이의 지질. 한글 본문은 HUD가 런타임에 그리므로 텍스처에는
    # 구김·테이프·물자국만 있다.
    "M_PaperClean",
    "M_PaperFolded",
    "M_PaperOld",
    "M_PaperWet",
    "M_SubmergedHoodieUV",
    "M_SubmergedPantsUV",
    "M_SubmergedSlipperWearUV",
    "M_SubmergedSlippersUV",
    "M_TankInteriorBiofilmUV",
    "M_WaterTankMetalUV",
    "M_WetHoodieUV",
    "M_WetRungPadUV",
    "M_WetServiceHoseUV",
})

# 이보다 얇으면 마구리가 아니라 종이다.
MAX_DRESSING_THICKNESS = 3.0

TABLE_PATTERN = re.compile(r"^([A-Z_]+) = \{", re.M)
ENTRY_PATTERN = re.compile(r'^\s+"(M_[A-Za-z0-9_]+)"\s*:', re.M)


def parse_print_materials(text: str) -> set:
    """PRINT_TABLES 안에 선언된 재질 이름."""
    bounds = [(match.start(), match.group(1))
              for match in TABLE_PATTERN.finditer(text)]
    bounds.append((len(text), ""))
    names = set()
    for (start, name), (end, _) in zip(bounds, bounds[1:]):
        if name not in PRINT_TABLES:
            continue
        for entry in ENTRY_PATTERN.finditer(text, start, end):
            names.add(entry.group(1))
    return names - UNIFORM_SURFACES


@dataclass
class Finding:
    source: str
    line: int
    function: str
    material: str
    thickness: float
    size: tuple

    def to_dict(self) -> dict:
        return {
            "code": "PRINT_ON_EDGES",
            "site": f"{self.source}:{self.line}",
            "function": self.function,
            "material": self.material,
            "thickness_cm": round(self.thickness, 2),
            "size": [round(value, 2) for value in self.size],
        }


def audit_boxes(boxes, bindings, print_materials, source_label):
    findings = []
    counts = {"printed": 0, "unresolved": 0, "authored_mesh": 0}
    for box in boxes:
        expression = (box.material or "").strip()
        # nullptr 재질에 저작 메시가 있는 자리는 메시의 구운 재질이다. 인쇄
        # 재질이 아니고, 못 푼 것도 아니다.
        if expression == "nullptr" and (box.note == "authored" or getattr(box, "mesh", "")):
            counts["authored_mesh"] += 1
            continue
        name = projection.resolve_material(bindings, expression, box.line)
        if not name:
            counts["unresolved"] += 1
            continue
        if name not in print_materials:
            continue
        # 저작 메시는 자기 UV를 들고 있다. 여섯 면이 같은 UV인 것은 엔진
        # 기본 큐브의 성질이므로 그쪽만 본다.
        # 저작 메시는 자기 UV를 들고 있다. 구운 바운드로 크기를 풀면
        # note 는 비지만 이름표는 남으므로, 둘 중 하나만 봐도 놓친다.
        if box.note == "authored" or getattr(box, "mesh", ""):
            counts["authored_mesh"] += 1
            continue
        counts["printed"] += 1
        thickness = min(box.size)
        if thickness <= MAX_DRESSING_THICKNESS:
            continue
        findings.append(Finding(
            source=source_label,
            line=box.line,
            function=box.function,
            material=name,
            thickness=thickness,
            size=tuple(box.size),
        ))
    return findings, counts


def run(project_root: str):
    recipe_path = os.path.join(project_root, MATERIAL_RECIPE)
    with open(recipe_path, "r", encoding="utf-8") as handle:
        print_materials = parse_print_materials(handle.read())

    findings = []
    totals = {"printed": 0, "unresolved": 0, "authored_mesh": 0}
    for relative in SOURCES:
        absolute = os.path.join(project_root, relative)
        with open(absolute, "r", encoding="utf-8") as handle:
            bindings = projection.parse_bindings(handle.read())
        scan = audit_world_geometry.scan_source(relative)
        found, counts = audit_boxes(
            scan.boxes, bindings, print_materials,
            relative.replace("\\", "/"))
        findings.extend(found)
        for key, value in counts.items():
            totals[key] += value
    findings.sort(key=lambda finding: (-finding.thickness, finding.line))
    return print_materials, totals, findings


# ---------------------------------------------------------------------------
# 자기 검사
# ---------------------------------------------------------------------------

SELF_TEST_RECIPE = '''
TEXTURED_MATERIALS = {
    "M_Concrete_X": {"tex": "Concrete", "mapping": "XZ", "tile": 150.0},
}

DECAL_MATERIALS = {
    "M_DemoNotice": {"tex_asset": "T_DemoNotice_D", "rough": 0.7},
    "M_WaterTankMetalUV": {"tex_asset": "T_WaterTankGalvanized_D"},
}

SIGN_MATERIALS = {
    "M_DemoSignLit": {"tex_asset": "T_DemoSign_D", "emissive_scale": 0.6},
}

INSTANCED_PRODUCT_MATERIALS = {
    "M_NotAPrint",
}
'''


def _self_test() -> int:
    failures = []

    def check(label, actual, expected):
        if actual != expected:
            failures.append(f"{label}: {actual!r} != {expected!r}")

    materials = parse_print_materials(SELF_TEST_RECIPE)
    check("표 경계", sorted(materials), ["M_DemoNotice", "M_DemoSignLit"])
    # 표면 스캔은 이름이 표 안에 있어도 빠진다.
    check("표면 스캔 제외", "M_WaterTankMetalUV" in materials, False)
    # 다른 표의 이름이 새어 들어오면 안 된다.
    check("다른 표", "M_NotAPrint" in materials, False)

    bindings = projection.parse_bindings(
        '\tUMaterialInterface* Notice = TexMat(TEXT("M_DemoNotice"), F);\n'
        '\tUMaterialInterface* Plain = TexMat(TEXT("M_Concrete_X"), F);\n')

    body = projection._FakeBox(9, "Notice", (0, 0, 140), (26, 9, 34))
    findings, counts = audit_boxes([body], bindings, materials, "Fake.cpp")
    check("두꺼운 몸통", [f.material for f in findings], ["M_DemoNotice"])
    check("두께", findings[0].thickness, 9.0)
    check("인쇄 블록 수", counts["printed"], 1)

    # 얇은 판은 이 씬의 관례다.
    plate = projection._FakeBox(9, "Notice", (0, 0, 140), (26, 0.4, 34))
    findings, _ = audit_boxes([plate], bindings, materials, "Fake.cpp")
    check("얇은 판", findings, [])

    # 인쇄가 아닌 재질은 두꺼워도 상관없다.
    plain = projection._FakeBox(9, "Plain", (0, 0, 140), (26, 9, 34))
    findings, _ = audit_boxes([plain], bindings, materials, "Fake.cpp")
    check("비인쇄 재질", findings, [])

    # 저작 메시는 자기 UV를 들고 있다.
    authored = projection._FakeBox(9, "Notice", (0, 0, 140), (26, 9, 34))
    authored.note = "authored"
    findings, counts = audit_boxes([authored], bindings, materials, "Fake.cpp")
    check("저작 메시", (findings, counts["authored_mesh"]), ([], 1))

    # 못 푼 재질을 통과로 세면 안 된다.
    unknown = projection._FakeBox(9, "LoopVariable", (0, 0, 140), (26, 9, 34))
    findings, counts = audit_boxes([unknown], bindings, materials, "Fake.cpp")
    check("못 푼 재질", (findings, counts["unresolved"]), ([], 1))

    # nullptr 재질에 저작 메시가 있으면 구운 재질이다. 못 푼 것으로 세지 않는다.
    baked_prop = projection._FakeBox(9, "nullptr", (0, 0, 140), (26, 9, 34))
    baked_prop.mesh = "SM_Demo"
    findings, counts = audit_boxes([baked_prop], bindings, materials, "Fake.cpp")
    check("nullptr 저작 메시",
          (findings, counts["authored_mesh"], counts["unresolved"]), ([], 1, 0))

    if failures:
        for failure in failures:
            print(f"  FAIL {failure}")
        print(f"PRINTED FACE AUDIT SELF-TEST FAIL  {len(failures)} case(s)")
        return 1
    print("PASS printed face audit self-test: table bounds, uniform-surface "
          "exemptions, thick bodies and thin plates, authored meshes and "
          "unresolved materials")
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

    materials, totals, findings = run(PROJECT_ROOT)

    if arguments.json:
        print(json.dumps({
            "print_materials": len(materials),
            "printed_blocks": totals["printed"],
            "authored_mesh": totals["authored_mesh"],
            "unresolved": totals["unresolved"],
            "findings": [finding.to_dict() for finding in findings],
        }, indent=2, ensure_ascii=False))
    else:
        print(f"PRINTED FACE AUDIT  materials={len(materials)} "
              f"blocks={totals['printed']} "
              f"authored_mesh={totals['authored_mesh']} "
              f"unresolved={totals['unresolved']} findings={len(findings)}")
        if totals["unresolved"]:
            print(f"  재질을 풀지 못한 상자 {totals['unresolved']}건 — "
                  f"호출부가 리터럴도 지역 변수도 아니었다")
        if not findings:
            print("\nevery printed surface is a face, not a whole body")
        else:
            print()
            for finding in findings:
                print(f"  [PRINT_ON_EDGES] {finding.source}:{finding.line} "
                      f"{finding.function}")
                print(f"      {finding.material} on a "
                      f"{finding.thickness:.1f} cm body "
                      f"{tuple(round(v, 1) for v in finding.size)}")

    return 1 if (arguments.check and findings) else 0


if __name__ == "__main__":
    raise SystemExit(main())
