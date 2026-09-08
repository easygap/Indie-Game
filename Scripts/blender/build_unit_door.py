"""세대 현관문. SM_UnitDoorLeaf(문짝)·SM_UnitDoorHardware(레버·도어락)·SM_UnitDoorFrame(문틀).

기준: Content/SourceArt/AI/SheetVillaCorridorFixturesReference.png 좌상.
무광 차콜 도장 강판 문짝 84 x 200 x 5 cm, 손잡이 쪽에서 안으로 들어온
브러시드 스테인리스 세로 띠와 그 위의 정사각 인레이 다섯, 눈높이 도어스코프,
하단 고무 스위프, 힌지 쪽 너클 셋. 원형 로제트의 레버와 그 위 디지털 도어락은
SM_UnitDoorHardware로 따로 낸다. 문틀은 8 x 7 cm 도장 강판 문선에 스톱 립.

문짝과 철물을 나눈 이유: 레버가 문짝 앞으로 7 cm 넘게 나온다. 한 메시로 두면
바운드가 그만큼 두꺼워져서 문에 붙인 종이(밤3 일지)가 감사에서 문짝을 뚫는
것으로 잡힌다. 문짝 메시는 판 앞으로 4 mm 안쪽만 나오게 두고, 철물은 같은
원점의 충돌 없는 메시로 같은 자리에 놓는다. 403호 회전문은 AIGSwingDoor가
두 메시를 같은 회전축에 단다.

원점은 문짝 바닥 중심. 앞면(복도에서 보는 면)은 -Y. 씬 코드가 DressUnitDoor에서
문짝을 (DoorX, -234.5, 0)에 두고 철물을 -Y쪽에 붙이므로 그 관례를 그대로 따른다.

    blender -b --factory-startup --python Scripts/blender/build_unit_door.py -- <out_dir> [leaf|leafL|hardware|hardwareL|frame ...]
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402

W, T, H = 0.84, 0.05, 2.00          # 문짝 폭·두께·높이
FRONT = -T * 0.5                    # 앞면 Y
HANDLE_X = 0.32                     # 문짝 중심에서 레버까지
BAND_X = 0.14                       # 스테인리스 띠 중심 X
# 문짝 메시가 판 앞면보다 나올 수 있는 한계(m). 감사의 관통 허용치가 0.5 cm라
# 이보다 안쪽이어야 문에 붙인 종이가 문짝을 뚫는 것으로 잡히지 않는다.
LEAF_PROUD_MAX = 0.0035


def materials():
    return {
        "paint": ig.mat_painted_steel("PaintedCharcoal", (0.06, 0.062, 0.068), roughness=0.58, wear=0.35),
        "steel": ig.mat_metal("Stainless", (0.80, 0.80, 0.79), roughness=0.34, streak=0.10),
        "chrome": ig.mat_metal("Chrome", (0.88, 0.88, 0.88), roughness=0.22, streak=0.04, anisotropic=False),
        "black": ig.mat_plastic("BlackPlastic", (0.018, 0.018, 0.02), roughness=0.46),
        "gloss": ig.mat_gloss("KeypadGlass", (0.008, 0.008, 0.01), roughness=0.10),
        "rubber": ig.mat_rubber("Rubber"),
        "led": ig.mat_emissive("KeypadLED", (0.55, 0.75, 1.0), strength=6.0),
    }


def hinge_side(hinge):
    """hinge="R": 정면(-Y)에서 봤을 때 힌지가 왼쪽(-X), 레버가 오른쪽.
    hinge="L": 좌우 거울상. 403호 문(AIGSwingDoor)은 힌지 축이 로컬 원점이고
    문짝이 +Y로 뻗으며 바깥면이 -X라서, L 변형을 yaw -90으로 놓아야 맞는다."""
    return 1.0 if hinge == "R" else -1.0


def leaf_parts(m, side):
    """문짝 판과 판에 붙어 4 mm 안쪽만 나오는 것들. 돌아오는 값은 (부품, 판)."""
    parts = []
    # 문짝 본체. 모서리 3 mm 접힘.
    leaf = ig.box("leaf", (W, T, H), origin="bottom", bevel=0.003, segments=2, material=m["paint"])
    parts.append(leaf)

    # 스테인리스 세로 띠. 문짝 앞면에서 1.2 mm 나온다.
    band_h = 1.88
    band = ig.box("band", (0.13, 0.0024, band_h), location=(side * BAND_X, FRONT - 0.0012, 0.06 + band_h * 0.5),
                  bevel=0.0008, segments=1, material=m["steel"])
    parts.append(band)
    # 인레이 다섯: 띠에 2 mm 패인 자리에 검정 사각.
    for z in (0.36, 0.68, 1.00, 1.32, 1.64):
        cutter = ig.box("cut", (0.05, 0.01, 0.05), location=(side * BAND_X, FRONT - 0.0012, z))
        ig.boolean(band, cutter, "DIFFERENCE")
        inlay = ig.box(f"inlay_{int(z * 100)}", (0.05, 0.004, 0.05),
                       location=(side * BAND_X, FRONT - 0.0002, z), material=m["black"])
        parts.append(inlay)

    # 도어스코프: 크롬 링과 어두운 렌즈. 링은 판에서 3 mm만 나온다.
    viewer = ig.cylinder("viewer", 0.016, 0.005, location=(0.0, FRONT - 0.0005, 1.55),
                         rotation=(math.pi * 0.5, 0.0, 0.0), segments=32, bevel=0.001,
                         bevel_segments=2, material=m["chrome"])
    parts.append(viewer)
    lens = ig.cylinder("lens", 0.009, 0.002, location=(0.0, FRONT - 0.0025, 1.55),
                       rotation=(math.pi * 0.5, 0.0, 0.0), segments=24, material=m["gloss"])
    parts.append(lens)

    # 힌지 너클: 손잡이 반대쪽 옆면에 셋. 복도에서는 옆면으로 얇게 읽힌다.
    for z in (0.28, 1.00, 1.72):
        knuckle = ig.cylinder(f"hinge_{int(z * 100)}", 0.0075, 0.10,
                              location=(-side * (W * 0.5 + 0.004), 0.0, z), segments=20, material=m["steel"])
        parts.append(knuckle)

    # 하단 고무 스위프. 판에 7 mm 묻히고 3 mm만 나온다.
    sweep = ig.box("sweep", (W - 0.02, 0.010, 0.03), location=(0.0, FRONT + 0.002, 0.017),
                   bevel=0.002, segments=1, material=m["rubber"])
    parts.append(sweep)
    return parts, leaf


def hardware_parts(m, side):
    """레버·도어락. 문짝 원점 기준으로 판 앞(-Y)에 매달린 것들."""
    parts = []
    # 레버 손잡이: 로제트, 넥, 레버.
    rose = ig.cylinder("rose", 0.030, 0.009, location=(side * HANDLE_X, FRONT - 0.0045, 0.95),
                       rotation=(math.pi * 0.5, 0.0, 0.0), segments=40, bevel=0.0015,
                       bevel_segments=2, material=m["steel"])
    parts.append(rose)
    neck = ig.cylinder("neck", 0.0105, 0.045, location=(side * HANDLE_X, FRONT - 0.009 - 0.0225, 0.95),
                       rotation=(math.pi * 0.5, 0.0, 0.0), segments=24, material=m["steel"])
    parts.append(neck)
    lever = ig.pipe("lever", [
        (side * HANDLE_X, FRONT - 0.054, 0.95),
        (side * (HANDLE_X - 0.02), FRONT - 0.062, 0.95),
        (side * (HANDLE_X - 0.115), FRONT - 0.062, 0.946),
        (side * (HANDLE_X - 0.128), FRONT - 0.058, 0.938),
    ], radius=0.0105, resolution=16, corner_radius=0.012, material=m["steel"])
    parts.append(lever)

    # 디지털 도어락 본체(무광 검정, 모서리 6 mm)와 터치 패널.
    lock = ig.box("lock", (0.09, 0.032, 0.24), location=(side * HANDLE_X, FRONT - 0.016, 1.22),
                  bevel=0.006, segments=3, material=m["black"])
    parts.append(lock)
    panel = ig.box("panel", (0.066, 0.002, 0.17), location=(side * HANDLE_X, FRONT - 0.0325, 1.235),
                   bevel=0.0008, segments=1, material=m["gloss"])
    parts.append(panel)
    # 3 x 4 터치 숫자 자리에 아주 작은 발광점. 실제 도어락은 꺼진 상태에서도
    # 희미하게 한 점이 남는다.
    for row in range(4):
        for col in range(3):
            dot = ig.cylinder(f"dot_{row}{col}", 0.0025, 0.0006,
                              location=(side * (HANDLE_X - 0.02 + col * 0.02), FRONT - 0.0338, 1.30 - row * 0.03),
                              rotation=(math.pi * 0.5, 0.0, 0.0), segments=8, material=m["led"])
            parts.append(dot)
    # 상태 LED 하나.
    status = ig.cylinder("status_led", 0.002, 0.0006, location=(side * HANDLE_X, FRONT - 0.0338, 1.165),
                         rotation=(math.pi * 0.5, 0.0, 0.0), segments=12, material=m["led"])
    parts.append(status)
    return parts


def build_leaf(out_root, hinge="R"):
    side = hinge_side(hinge)
    ig.reset_scene()
    parts, leaf = leaf_parts(materials(), side)
    lo, _ = ig.bounds(leaf)
    parts_lo = min(ig.bounds(p)[0][1] for p in parts)
    proud = lo[1] - parts_lo
    if proud > LEAF_PROUD_MAX + 1e-6:
        raise RuntimeError(f"문짝 부품이 판 앞으로 {proud * 1000:.1f} mm 나온다. 한계 {LEAF_PROUD_MAX * 1000:.1f} mm")
    ig.log(f"unit door leaf {hinge}: proud of panel {proud * 1000:.1f} mm")

    # 충돌은 문짝 상자 하나. 플레이어가 두드리고 팔 길이에서 보는 문짝이라
    # hero 예산을 준다.
    suffix = "" if hinge == "R" else "L"
    return ig.build_asset(
        f"SM_UnitDoorLeaf{suffix}", "hero", parts, out_root,
        collision_parts=[[leaf]],
        notes=(f"세대 현관문 문짝({hinge} 힌지). 앞면 -Y, 원점 바닥 중심. 판 앞으로 4 mm 안쪽만 나온다. "
               f"레버·도어락은 같은 원점의 SM_UnitDoorHardware{suffix}, 문틀은 SM_UnitDoorFrame."),
        texture_size=2048)


def build_hardware(out_root, hinge="R"):
    side = hinge_side(hinge)
    ig.reset_scene()
    parts = hardware_parts(materials(), side)
    # 레버는 손이 닿는 프롬프트용이라 충돌에 안 넣는다.
    suffix = "" if hinge == "R" else "L"
    return ig.build_asset(
        f"SM_UnitDoorHardware{suffix}", "prop", parts, out_root,
        collision_parts=[],
        notes=(f"세대 현관문 레버·도어락({hinge} 힌지). 원점은 문짝과 같은 바닥 중심이라 "
               f"SM_UnitDoorLeaf{suffix}와 같은 자리에 놓는다. 충돌 없음."),
        texture_size=1024)


def build_frame(out_root):
    ig.reset_scene()
    paint = ig.mat_painted_steel("PaintedCharcoalFrame", (0.05, 0.052, 0.056), roughness=0.6, wear=0.3)
    jamb_w, jamb_d = 0.08, 0.07
    opening_w, opening_h = 0.86, 2.02
    parts = []
    for sign in (-1.0, 1.0):
        jamb = ig.box(f"jamb_{'r' if sign > 0 else 'l'}", (jamb_w, jamb_d, opening_h + jamb_w),
                      location=(sign * (opening_w * 0.5 + jamb_w * 0.5), 0.0, (opening_h + jamb_w) * 0.5),
                      bevel=0.003, segments=2, material=paint)
        parts.append(jamb)
        # 스톱 립: 문짝이 닫혀 걸리는 안쪽 턱.
        lip = ig.box(f"lip_{'r' if sign > 0 else 'l'}", (0.015, 0.03, opening_h),
                     location=(sign * (opening_w * 0.5 - 0.0075), 0.02, opening_h * 0.5),
                     bevel=0.002, segments=1, material=paint)
        parts.append(lip)
    head = ig.box("head", (opening_w + jamb_w * 2.0, jamb_d, jamb_w),
                  location=(0.0, 0.0, opening_h + jamb_w * 0.5), bevel=0.003, segments=2, material=paint)
    parts.append(head)
    head_lip = ig.box("head_lip", (opening_w, 0.03, 0.015), location=(0.0, 0.02, opening_h - 0.0075),
                      bevel=0.002, segments=1, material=paint)
    parts.append(head_lip)
    return ig.build_asset(
        "SM_UnitDoorFrame", "prop", parts, out_root,
        collision_parts=[[parts[0], parts[1]], [parts[2], parts[3]], [head, head_lip]],
        notes="세대 현관문 문틀. 개구부 86 x 202, 문선 8 x 7. 원점은 개구부 바닥 중심.",
        texture_size=1024)


def main():
    out_root = ig.out_root_from_argv()
    only = [a for a in sys.argv[sys.argv.index("--") + 2:]] if "--" in sys.argv else []
    if not only or "leaf" in only:
        build_leaf(out_root, "R")
    if not only or "leafL" in only:
        build_leaf(out_root, "L")
    if not only or "hardware" in only:
        build_hardware(out_root, "R")
    if not only or "hardwareL" in only:
        build_hardware(out_root, "L")
    if not only or "frame" in only:
        build_frame(out_root)


main()
