"""403호 소형 냉장고. SM_FridgeBody(본체)와 SM_FridgeDoor(문짝).

기준: Content/SourceArt/AI/SheetOneroomKitchenAppliancesReference.png 좌상.
AIGFridge 액터의 좌표계를 그대로 따른다: X가 문에서 뒤판까지 깊이(앞면 -X),
Y가 폭(힌지 쪽 -Y), Z가 높이. 본체 66 x 72 x 158, 패널 6 cm, 문 두께 6 cm.

본체 원점은 바닥 중심(FridgeRoot). 문짝 원점은 힌지 축 위 피벗 높이
(Z 79)라 DoorPivot에 상대 변환 없이 붙는다. 문짝은 Y 0..72, X -6..0.
문 아래 6 cm는 컴프레서 그릴이 보인다.

    blender -b --factory-startup --python Scripts/blender/build_fridge.py -- <out_dir>
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402

W, D, H = 0.66, 0.72, 1.58     # X 깊이, Y 폭, Z 높이
T = 0.06                       # 패널 두께
PIVOT_Z = H * 0.5


def materials():
    return {
        "enamel": ig.mat_painted_steel("EnamelWhite", (0.85, 0.85, 0.83), roughness=0.34, wear=0.12,
                                       bump=0.02, dirt_color=(0.55, 0.53, 0.50)),
        "liner": ig.mat_plastic("LinerGrey", (0.74, 0.75, 0.74), roughness=0.42),
        "dark": ig.mat_plastic("DarkPlastic", (0.02, 0.02, 0.022), roughness=0.5),
        "chrome": ig.mat_metal("Chrome", (0.84, 0.84, 0.84), roughness=0.25, streak=0.05, anisotropic=False),
        "rubber": ig.mat_rubber("Gasket", (0.05, 0.05, 0.05), roughness=0.85),
        "glass": ig.mat_glass("Glass"),
        "lamp": ig.mat_plastic("LampCover", (0.92, 0.90, 0.85), roughness=0.4, bump=0.0),
    }


def build_body(out_root):
    ig.reset_scene()
    m = materials()
    parts = []
    shell = ig.box("shell", (W, D, H), origin="bottom", bevel=0.012, segments=3, material=m["enamel"])
    # 앞이 열린 속. 뒤판 6, 옆판 6, 바닥 6, 천장 6.
    cavity = ig.box("cavity", (W + 0.02 - T, D - 2 * T, H - 2 * T),
                    location=(-W * 0.5 - 0.01 + (W + 0.02 - T) * 0.5, 0.0, H * 0.5))
    ig.boolean(shell, cavity, "DIFFERENCE")
    parts.append(shell)
    # 라이너: 속 벽에 4 mm 회색 플라스틱.
    inner_w, inner_d, inner_h = W - T, D - 2 * T, H - 2 * T
    liner = ig.box("liner", (inner_w, inner_d, inner_h), location=(-W * 0.5 + inner_w * 0.5, 0.0, T + inner_h * 0.5),
                   material=m["liner"])
    liner_cut = ig.box("liner_cut", (inner_w + 0.02, inner_d - 0.008, inner_h - 0.008),
                       location=(-W * 0.5 + inner_w * 0.5 - 0.01, 0.0, T + inner_h * 0.5))
    ig.boolean(liner, liner_cut, "DIFFERENCE")
    parts.append(liner)
    # 유리 선반 둘과 앞 트림. 높이는 AIGFridge가 내용물을 올리는 값과 같다.
    for z in (H * 0.50, H * 0.26):
        shelf = ig.box(f"shelf_{int(z * 100)}", (0.50, inner_d - 0.012, 0.006), location=(0.01, 0.0, z),
                       material=m["glass"])
        trim = ig.box(f"trim_{int(z * 100)}", (0.02, inner_d - 0.012, 0.022), location=(-0.25, 0.0, z),
                      bevel=0.002, segments=1, material=m["chrome"])
        parts.extend([shelf, trim])
        # 선반 받침 리브.
        for side in (-1.0, 1.0):
            rib = ig.box(f"rib_{int(z * 100)}_{int(side)}", (0.46, 0.012, 0.012),
                         location=(0.02, side * (inner_d * 0.5 - 0.006), z - 0.009), material=m["liner"])
            parts.append(rib)
    # 야채칸: 회색 서랍과 투명 앞판, 손잡이 레일.
    drawer = ig.box("drawer", (0.46, inner_d - 0.06, 0.22), location=(0.03, 0.0, T + 0.12),
                    bevel=0.008, segments=2, material=m["liner"])
    drawer_cut = ig.box("drawer_cut", (0.44, inner_d - 0.08, 0.22), location=(0.03, 0.0, T + 0.13))
    ig.boolean(drawer, drawer_cut, "DIFFERENCE")
    parts.append(drawer)
    drawer_front = ig.box("drawer_front", (0.012, inner_d - 0.07, 0.20), location=(-0.205, 0.0, T + 0.13),
                          material=m["glass"])
    parts.append(drawer_front)
    drawer_rail = ig.box("drawer_rail", (0.02, inner_d - 0.09, 0.02), location=(-0.21, 0.0, T + 0.245),
                         bevel=0.003, segments=1, material=m["liner"])
    parts.append(drawer_rail)
    # 뒤쪽 냉기 패널과 슬릿, 천장 등 커버.
    back = ig.box("cooling_panel", (0.035, inner_d - 0.26, 0.52), location=(W * 0.5 - T - 0.02, 0.0, H * 0.62),
                  bevel=0.004, segments=1, material=m["liner"])
    parts.append(back)
    for i in range(3):
        slit = ig.box(f"slit_{i}", (0.008, inner_d - 0.34, 0.014),
                      location=(W * 0.5 - T - 0.04, 0.0, H * 0.55 + i * 0.07), material=m["dark"])
        parts.append(slit)
    lamp = ig.box("lamp", (0.18, 0.12, 0.035), location=(0.02, 0.0, H - T - 0.0175), bevel=0.006, segments=2,
                  material=m["lamp"])
    parts.append(lamp)
    # 앞 아래 컴프레서 그릴: 바닥 패널 앞면에 검정 가로 슬릿 판.
    grille = ig.box("grille", (0.006, D - 0.16, 0.045), location=(-W * 0.5 + 0.003, 0.0, 0.032), material=m["dark"])
    for i in range(4):
        vent = ig.box(f"vent_{i}", (0.02, D - 0.20, 0.004), location=(-W * 0.5 + 0.004, 0.0, 0.016 + i * 0.011))
        ig.boolean(grille, vent, "DIFFERENCE")
    parts.append(grille)
    # 힌지 판 둘(앞 왼쪽 모서리, 위·아래).
    for z in (H, 0.0):
        plate = ig.box(f"hinge_{int(z * 100)}", (0.06, 0.06, 0.014), location=(-W * 0.5 + 0.02, -D * 0.5 + 0.02, z + (0.007 if z == 0.0 else -0.007)),
                       bevel=0.002, segments=1, material=m["dark"])
        pin = ig.cylinder(f"pin_{int(z * 100)}", 0.008, 0.03, location=(-W * 0.5, -D * 0.5, z + (0.022 if z == 0.0 else -0.022)),
                          segments=16, material=m["chrome"])
        parts.extend([plate, pin])

    # 충돌: 다섯 패널 상자. AIGFridge가 세우던 껍데기 다섯 장과 같다.
    coll = [
        ig.box("c_back", (T, D, H), location=(W * 0.5 - T * 0.5, 0.0, H * 0.5)),
        ig.box("c_left", (W, T, H), location=(0.0, -D * 0.5 + T * 0.5, H * 0.5)),
        ig.box("c_right", (W, T, H), location=(0.0, D * 0.5 - T * 0.5, H * 0.5)),
        ig.box("c_top", (W, D, T), location=(0.0, 0.0, H - T * 0.5)),
        ig.box("c_bottom", (W, D, T), location=(0.0, 0.0, T * 0.5)),
    ]
    for c in coll:
        c.hide_render = True
    return ig.build_asset(
        "SM_FridgeBody", "large", parts, out_root,
        collision_parts=[[c] for c in coll],
        notes="403호 냉장고 본체 66 x 72 x 158. 앞면 -X(AIGFridge 좌표계), 원점 바닥 중심. Glass 슬롯(선반·야채칸 앞판).",
        texture_size=2048)


def build_door(out_root):
    ig.reset_scene()
    m = materials()
    parts = []
    door_h = H - T                       # 문 아래 6 cm는 그릴
    z_lo, z_hi = T - PIVOT_Z, H - PIVOT_Z
    zc = (z_lo + z_hi) * 0.5
    slab = ig.box("slab", (T, D, door_h), location=(-T * 0.5, D * 0.5, zc), bevel=0.012, segments=3,
                  material=m["enamel"])
    # 손잡이: 자유단 쪽 세로 포켓.
    pocket = ig.box("pocket", (0.03, 0.04, 0.32), location=(-T, D - 0.055, zc + 0.20))
    ig.boolean(slab, pocket, "DIFFERENCE")
    parts.append(slab)
    pocket_liner = ig.box("pocket_liner", (0.028, 0.038, 0.318), location=(-T + 0.014, D - 0.055, zc + 0.20),
                          material=m["dark"])
    pocket_liner_cut = ig.box("pocket_liner_cut", (0.03, 0.034, 0.314), location=(-T + 0.011, D - 0.055, zc + 0.20))
    ig.boolean(pocket_liner, pocket_liner_cut, "DIFFERENCE")
    parts.append(pocket_liner)
    # 개스킷: 안쪽 면 테두리.
    gasket = ig.box("gasket", (0.008, D - 0.02, door_h - 0.02), location=(0.004, D * 0.5, zc), material=m["rubber"])
    gasket_cut = ig.box("gasket_cut", (0.02, D - 0.05, door_h - 0.05), location=(0.004, D * 0.5, zc))
    ig.boolean(gasket, gasket_cut, "DIFFERENCE")
    parts.append(gasket)
    # 문 안쪽 선반 둘(AIGFridge 포켓 높이 -42, +8).
    for z in (-0.42, 0.08):
        tray = ig.box(f"tray_{int(abs(z) * 100)}{'n' if z < 0 else 'p'}", (0.09, D - 0.18, 0.016),
                      location=(0.012 + 0.045, D * 0.5, z), bevel=0.003, segments=1, material=m["liner"])
        lip = ig.box(f"lip_{int(abs(z) * 100)}{'n' if z < 0 else 'p'}", (0.014, D - 0.18, 0.09),
                     location=(0.012 + 0.09 - 0.007, D * 0.5, z + 0.045), bevel=0.003, segments=1, material=m["liner"])
        parts.extend([tray, lip])
    return ig.build_asset(
        "SM_FridgeDoor", "prop", parts, out_root,
        collision_parts=[[slab]],
        notes="403호 냉장고 문짝. 원점은 힌지 축의 피벗 높이(Z 79). X -6..0, Y 0..72, Z -73..79.",
        texture_size=2048)


def main():
    out_root = ig.out_root_from_argv()
    only = sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else []
    if not only or "body" in only:
        build_body(out_root)
    if not only or "door" in only:
        build_door(out_root)


main()
