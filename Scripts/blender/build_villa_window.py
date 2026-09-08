"""빌라 파사드 창. SM_VillaWindow — 2~4층 창 열다섯 자리에 같은 메시를 놓는다.

씬(BuildAlley)의 상자: 알루미늄 틀(머리·밑틀 104x5x6, 세로틀 6x5x116, 가운데 살
4x5x112), 돌 창턱 112x12x7, 아래 절반의 검정 난간(가로대 둘, 세로대 아홉). 유리
판(WindowDark/WindowGlow 96x4x116)은 씬 상자가 그대로 맡는다 — 깨어 있는 집
하나의 발광이 그 판이다.

원점은 벽면(Y -395)에서 창 개구부 바닥 중심 (WindowX, -395, WindowZ - 60).
골목 쪽이 -Y라 앞면 -Y다.

    blender -b --factory-startup --python Scripts/blender/build_villa_window.py -- <out_dir>
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402


def build_window(out_root):
    ig.reset_scene()
    alu = ig.mat_painted_steel("DarkAluminium", (0.05, 0.052, 0.055), roughness=0.45, wear=0.2, bump=0.03)
    black = ig.mat_painted_steel("RailingBlack", (0.015, 0.015, 0.016), roughness=0.5, wear=0.3, bump=0.05)
    stone = ig.mat_speckle("SillStone", (0.46, 0.45, 0.43), (0.60, 0.58, 0.55), scale=500.0, threshold=0.58,
                           roughness=0.78, bump=0.03)
    parts = []
    depth, y_frame = 0.05, -0.025      # 틀은 벽면에서 5 cm 나온다(씬 Y -400..-395)
    # 바깥 틀: 머리, 밑틀, 세로틀 둘, 가운데 세로 살.
    parts.append(ig.box("head", (1.04, depth, 0.06), location=(0.0, y_frame, 1.20), bevel=0.003, segments=1,
                        material=alu))
    parts.append(ig.box("cill", (1.04, depth, 0.06), location=(0.0, y_frame, 0.0), bevel=0.003, segments=1,
                        material=alu))
    for x in (-0.49, 0.49):
        parts.append(ig.box("jamb", (0.06, depth, 1.16), location=(x, y_frame, 0.60), bevel=0.003, segments=1,
                            material=alu))
    parts.append(ig.box("mullion", (0.04, depth, 1.12), location=(0.0, y_frame, 0.60), bevel=0.002, segments=1,
                        material=alu))
    # 미닫이 두 짝의 얇은 프로파일. 안짝은 벽 쪽, 바깥짝은 골목 쪽 레일.
    for index, (xc, y) in enumerate(((-0.25, -0.012), (0.25, -0.034))):
        for sx in (xc - 0.21, xc + 0.21):
            parts.append(ig.box(f"sash{index}_stile", (0.03, 0.018, 1.06), location=(sx, y, 0.60),
                                bevel=0.0015, segments=1, material=alu))
        for sz in (0.045, 1.155):
            parts.append(ig.box(f"sash{index}_rail", (0.42, 0.018, 0.03), location=(xc, y, sz),
                                bevel=0.0015, segments=1, material=alu))
    # 돌 창턱과 물끊기(아래 앞모서리 홈).
    sill = ig.box("sill", (1.12, 0.105, 0.07), location=(0.0, -0.0575, -0.06), bevel=0.004, segments=2,
                  material=stone)
    drip = ig.box("drip", (1.16, 0.015, 0.012), location=(0.0, -0.105, -0.09))
    ig.boolean(sill, drip, "DIFFERENCE")
    parts.append(sill)
    # 검정 난간: 가로대 둘, 세로대 아홉, 벽에 박힌 받침 둘.
    parts.append(ig.box("rail_top", (1.08, 0.035, 0.035), location=(0.0, -0.06, 0.54), bevel=0.003, segments=1,
                        material=black))
    parts.append(ig.box("rail_bottom", (1.08, 0.03, 0.03), location=(0.0, -0.06, 0.26), bevel=0.003, segments=1,
                        material=black))
    for index in range(-4, 5):
        parts.append(ig.box("bar", (0.022, 0.022, 0.60), location=(index * 0.12, -0.06, 0.26), material=black))
    for x in (-0.52, 0.52):
        parts.append(ig.box("rail_bracket", (0.03, 0.06, 0.03), location=(x, -0.03, 0.54), material=black))
        parts.append(ig.box("rail_bracket_low", (0.03, 0.06, 0.03), location=(x, -0.03, 0.26), material=black))
    return ig.build_asset(
        "SM_VillaWindow", "prop", parts, out_root,
        collision_parts=[],
        notes=("빌라 파사드 창(2~4층 열다섯 자리 공용). 원점은 벽면 Y -395의 창 개구부 바닥 중심, 앞면 -Y. "
               "틀 5 cm, 창턱 12 cm, 난간 8 cm 돌출. 유리는 씬의 WindowDark/WindowGlow 판이 맡는다. 충돌 없음."),
        texture_size=1024)


def main():
    out_root = ig.out_root_from_argv()
    build_window(out_root)


main()
