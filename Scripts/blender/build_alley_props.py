"""골목 소품. SM_TrafficCone(주차 콘), SM_UtilityPole(전신주).

콘: 씬은 엔진 원뿔에 주황 재질을 씌워 30 x 30 x 48 상자 세 곳에 세운다. 같은
봉투 안에서 검정 고무 받침·주황 원뿔·흰 반사띠 둘·손잡이 구멍을 만든다.
원점은 바닥 중심.

전신주: 씬은 지름 14, 높이 450의 원기둥 하나에 스캔 분전함을 붙인다. 위로
가늘어지는 콘크리트 전주에 완철 둘과 애자, 조임 밴드, 번호판을 얹는다. 분전함은
씬의 스캔 소품(utility_box_01)이 그대로 맡는다. 원점 바닥 중심, 충돌은 기둥만.

    blender -b --factory-startup --python Scripts/blender/build_alley_props.py -- <out_dir> [cone|pole ...]
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402


def build_cone(out_root):
    ig.reset_scene()
    orange = ig.mat_plastic("ConeOrange", (0.85, 0.22, 0.03), roughness=0.55, bump=0.02, grain_scale=200.0)
    rubber = ig.mat_rubber("ConeBase", (0.03, 0.03, 0.03), roughness=0.9)
    white = ig.mat_plastic("Reflective", (0.9, 0.9, 0.88), roughness=0.3, bump=0.0)
    parts = []
    base = ig.box("base", (0.30, 0.30, 0.03), origin="bottom", bevel=0.008, segments=2, material=rubber)
    parts.append(base)
    body = ig.lathe("body", [
        (0.0, 0.03), (0.12, 0.03), (0.115, 0.05), (0.04, 0.44), (0.035, 0.47), (0.0, 0.48),
    ], segments=40, material=orange)
    parts.append(body)
    for z0, z1 in ((0.18, 0.24), (0.30, 0.34)):
        r0 = 0.115 - (z0 - 0.05) * (0.075 / 0.39) + 0.0012
        r1 = 0.115 - (z1 - 0.05) * (0.075 / 0.39) + 0.0012
        band = ig.lathe(f"band_{int(z0 * 100)}", [(r0, z0), (r1, z1)], segments=40, material=white)
        parts.append(band)
    return ig.build_asset(
        "SM_TrafficCone", "prop", parts, out_root, collision_parts=[[base, body]],
        notes="주차 콘 30 x 30 x 48. 원점 바닥 중심. 씬의 원뿔 상자(중심 Z 24)는 바닥 Z 0으로 옮겨 놓는다.",
        texture_size=512)


def build_pole(out_root):
    ig.reset_scene()
    concrete = ig.mat_speckle("PoleConcrete", (0.50, 0.49, 0.47), (0.36, 0.35, 0.33), scale=700.0, threshold=0.6,
                              roughness=0.82, bump=0.04)
    steel = ig.mat_painted_steel("GalvSteel", (0.42, 0.43, 0.44), roughness=0.55, wear=0.3, bump=0.03)
    porcelain = ig.mat_gloss("Porcelain", (0.72, 0.70, 0.62), roughness=0.18)
    plate = ig.mat_plastic("NumberPlate", (0.85, 0.85, 0.80), roughness=0.4)
    parts = []
    # 콘크리트 전주: 아래 지름 20, 위 지름 13, 높이 450. 씬 원기둥(지름 14)과 같은 봉투 안.
    pole = ig.lathe("pole", [(0.0, 0.0), (0.10, 0.0), (0.10, 0.05), (0.095, 0.30), (0.065, 4.45), (0.06, 4.50),
                             (0.0, 4.50)], segments=28, material=concrete)
    parts.append(pole)
    # 완철 둘(위·아래)과 애자 셋씩. 골목을 따라(X) 뻗는다.
    for z, arm_len in ((4.25, 1.20), (3.90, 1.00)):
        arm = ig.box(f"arm_{int(z * 100)}", (arm_len, 0.08, 0.08), location=(0.0, 0.0, z), bevel=0.004, segments=1,
                     material=steel)
        parts.append(arm)
        for x in (-arm_len * 0.5 + 0.10, 0.0, arm_len * 0.5 - 0.10):
            if x == 0.0 and z > 4.0:
                continue
            ins = ig.lathe(f"insulator_{int(z * 100)}_{int((x + 1) * 100)}",
                           [(0.0, 0.0), (0.03, 0.0), (0.045, 0.03), (0.04, 0.06), (0.05, 0.09), (0.035, 0.12),
                            (0.0, 0.13)], segments=20, location=(x, 0.0, z + 0.04), material=porcelain)
            parts.append(ins)
        # 완철을 전주에 죄는 밴드.
        parts.append(ig.cylinder(f"band_{int(z * 100)}", 0.085, 0.10, location=(0.0, 0.0, z), segments=28,
                                 material=steel))
    # 가로등 앵커 볼트 자리 대신, 사람 눈높이의 전주 번호판과 아래 밴드.
    parts.append(ig.box("number_plate", (0.14, 0.006, 0.20), location=(0.0, -0.095, 1.80), bevel=0.002,
                        segments=1, material=plate))
    parts.append(ig.cylinder("foot_band", 0.104, 0.06, location=(0.0, 0.0, 0.32), segments=28, material=steel))
    return ig.build_asset(
        "SM_UtilityPole", "large", parts, out_root, collision_parts=[[pole]],
        notes=("골목 콘크리트 전신주 20 x 20 x 450. 원점 바닥 중심(씬 (X, -422, 0)). 완철은 골목 방향(X). "
               "분전함은 씬의 스캔 소품 utility_box_01이 그대로 붙는다. 충돌은 기둥만."),
        texture_size=1024)


BUILDERS = {"cone": build_cone, "pole": build_pole}


def main():
    out_root = ig.out_root_from_argv()
    only = [a for a in sys.argv[sys.argv.index("--") + 2:]] if "--" in sys.argv else []
    for key, builder in BUILDERS.items():
        if not only or key in only:
            builder(out_root)


main()
