"""1층 로비 우편함 SM_MailboxUnit. 3열 3단, 청회색 분체도장 강판.

기준: Content/SourceArt/AI/SheetVillaCorridorFixturesReference.png 우상.
씬(BuildLobby)의 상자 배치를 그대로 잇는다: 뒷판 96 x 4 x 66, 상자 26 x 9 x 18이
X 피치 28·Z 피치 20. 원점은 뒷판 뒷면(벽면)의 바닥 중심, 앞면은 -Y.
씬은 (528, -235, 118)에 놓는다.

각 문짝: 위쪽 가로 투입구와 그 위의 짧은 차양, 왼쪽 아래 캠록, 오른쪽 아래
이름표 창. 문짝은 1.5 mm 나와 있어 손전등에 테두리가 잡힌다.

    blender -b --factory-startup --python Scripts/blender/build_lobby_mailboxes.py -- <out_dir>
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402


def main():
    out_root = ig.out_root_from_argv()
    ig.reset_scene()
    coat = ig.mat_painted_steel("PowderBlueGrey", (0.085, 0.10, 0.125), roughness=0.52, wear=0.25,
                                dirt_color=(0.05, 0.05, 0.05))
    chrome = ig.mat_metal("Chrome", (0.86, 0.86, 0.86), roughness=0.22, streak=0.05, anisotropic=False)
    dark = ig.mat_plastic("DarkPlastic", (0.02, 0.02, 0.022), roughness=0.5)
    card = ig.mat_plastic("NameCard", (0.88, 0.87, 0.82), roughness=0.55, bump=0.0)

    parts = []
    back = ig.box("backplate", (0.96, 0.04, 0.66), location=(0.0, -0.02, 0.33), bevel=0.003, segments=2, material=coat)
    parts.append(back)
    box_d = 0.09
    for z in (0.13, 0.33, 0.53):
        for x in (-0.28, 0.0, 0.28):
            tag = f"{int(x * 100):+d}_{int(z * 100)}"
            body = ig.box(f"box{tag}", (0.26, box_d, 0.18), location=(x, -0.04 - box_d * 0.5, z),
                          bevel=0.002, segments=2, material=coat)
            front = -0.04 - box_d
            # 문짝: 1.5 mm 나온 판. 투입구를 문짝과 본체 모두에서 뚫는다.
            door = ig.box(f"door{tag}", (0.246, 0.003, 0.166), location=(x, front - 0.0015, z),
                          bevel=0.001, segments=1, material=coat)
            slot = ig.box(f"slot{tag}", (0.18, 0.05, 0.022), location=(x, front, z + 0.045))
            ig.boolean(door, slot, "DIFFERENCE", remove_cutter=False)
            ig.boolean(body, slot, "DIFFERENCE")
            parts.extend([body, door])
            # 차양: 투입구 위, 앞으로 8 mm, 살짝 기울어짐.
            hood = ig.box(f"hood{tag}", (0.19, 0.009, 0.004), location=(x, front - 0.0045, z + 0.058),
                          rotation=(math.radians(-15.0), 0.0, 0.0), bevel=0.0008, segments=1, material=coat)
            parts.append(hood)
            # 캠록: 크롬 원통과 세로 열쇠 구멍.
            lock = ig.cylinder(f"lock{tag}", 0.011, 0.005, location=(x - 0.08, front - 0.0025 - 0.0015, z - 0.04),
                               rotation=(math.pi * 0.5, 0.0, 0.0), segments=20, bevel=0.001, bevel_segments=1,
                               material=chrome)
            keyhole = ig.box(f"keyhole{tag}", (0.0025, 0.004, 0.008), location=(x - 0.08, front - 0.005, z - 0.04),
                             material=dark)
            parts.extend([lock, keyhole])
            # 이름표 창: 어두운 테와 흰 카드.
            frame = ig.box(f"frame{tag}", (0.056, 0.0025, 0.024), location=(x + 0.045, front - 0.0015 - 0.00125, z - 0.04),
                           bevel=0.0006, segments=1, material=dark)
            window = ig.box(f"window{tag}", (0.048, 0.01, 0.016), location=(x + 0.045, front - 0.003, z - 0.04))
            ig.boolean(frame, window, "DIFFERENCE")
            paper = ig.box(f"card{tag}", (0.047, 0.0008, 0.015), location=(x + 0.045, front - 0.0016, z - 0.04),
                           material=card)
            parts.extend([frame, paper])

    return ig.build_asset(
        "SM_MailboxUnit", "prop", parts, out_root,
        collision_parts=[[back] + [p for p in parts if p.name.startswith("box")]],
        notes="로비 북쪽 벽 우편함 3x3. 원점은 벽면 바닥 중심, 앞면 -Y. 씬 위치 (528, -235, 118).",
        texture_size=2048)


main()
