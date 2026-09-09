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


def build_gas_meter(out_root):
    """가스 계량기함. 벽에 붙는 회색 강판 함에 계량기 창, 위아래 배관. 원점은 벽면 바닥 중심, 앞면 -Y."""
    ig.reset_scene()
    steel = ig.mat_painted_steel("MeterBoxSteel", (0.62, 0.64, 0.62), roughness=0.5, wear=0.35, bump=0.03)
    dark = ig.mat_plastic("MeterDark", (0.03, 0.03, 0.03), roughness=0.5)
    glass = ig.mat_gloss("MeterGlass", (0.02, 0.02, 0.025), roughness=0.15)
    yellow = ig.mat_plastic("PipeYellow", (0.85, 0.65, 0.10), roughness=0.5)
    parts = []
    box = ig.box("box", (0.40, 0.20, 0.50), location=(0.0, -0.10, 0.25), bevel=0.004, segments=2, material=steel)
    parts.append(box)
    parts.append(ig.box("door_seam", (0.34, 0.004, 0.42), location=(0.0, -0.202, 0.26), material=dark))
    parts.append(ig.box("window", (0.14, 0.004, 0.08), location=(0.06, -0.204, 0.34), material=glass))
    parts.append(ig.cylinder("latch", 0.012, 0.01, location=(-0.12, -0.205, 0.22), rotation=(math.pi * 0.5, 0.0, 0.0),
                             segments=12, material=dark))
    for x in (-0.12, 0.10):
        parts.append(ig.cylinder(f"pipe_{int((x + 1) * 100)}", 0.017, 0.60, location=(x, -0.06, 0.80), segments=14,
                                 material=yellow))
        parts.append(ig.cylinder(f"pipe_low_{int((x + 1) * 100)}", 0.017, 0.10, location=(x, -0.06, -0.05), segments=14,
                                 material=yellow))
    return ig.build_asset(
        "SM_GasMeterBox", "prop", parts, out_root, collision_parts=[[box]],
        notes="샛길 가스 계량기함 40 x 20 x 50, 위로 60 cm 배관. 원점 벽면 바닥 중심, 앞면 -Y.", texture_size=512)


def build_ac_outdoor(out_root):
    """에어컨 실외기. 앞면(-Y)에 팬 그릴, 옆에 배관 구멍. 벽걸이 받침 둘. 원점은 벽면 바닥 중심."""
    ig.reset_scene()
    shell = ig.mat_painted_steel("AcShell", (0.80, 0.80, 0.78), roughness=0.45, wear=0.25, bump=0.02)
    dark = ig.mat_plastic("AcDark", (0.04, 0.04, 0.04), roughness=0.55)
    steel = ig.mat_metal("Bracket", (0.45, 0.46, 0.47), roughness=0.5, streak=0.1)
    parts = []
    body = ig.box("body", (0.80, 0.30, 0.55), location=(0.0, -0.15 - 0.06, 0.275), bevel=0.006, segments=2, material=shell)
    parts.append(body)
    # 팬 그릴: 동심 링 여섯과 가운데 허브.
    for index in range(6):
        parts.append(ig.torus(f"ring{index}", 0.05 + index * 0.035, 0.004, location=(-0.16, -0.36 - 0.006, 0.28),
                              rotation=(math.pi * 0.5, 0.0, 0.0), major_segments=40, minor_segments=6, material=dark))
    parts.append(ig.cylinder("hub", 0.03, 0.01, location=(-0.16, -0.362, 0.28), rotation=(math.pi * 0.5, 0.0, 0.0),
                             segments=20, material=dark))
    # 오른쪽 루버.
    for z in (0.10, 0.17, 0.24, 0.31, 0.38, 0.45):
        parts.append(ig.box(f"louvre_{int(z * 100)}", (0.28, 0.006, 0.02), location=(0.22, -0.363, z), material=dark))
    # 벽 받침 둘.
    for x in (-0.28, 0.28):
        parts.append(ig.box(f"bracket_{int((x + 1) * 100)}", (0.04, 0.34, 0.04), location=(x, -0.17, -0.02), material=steel))
        parts.append(ig.box(f"strut_{int((x + 1) * 100)}", (0.04, 0.04, 0.30), location=(x, -0.02, -0.15), material=steel))
    parts.append(ig.cylinder("pipe", 0.012, 0.30, location=(0.36, -0.06, 0.10), segments=12, material=dark))
    return ig.build_asset(
        "SM_AcOutdoorUnit", "prop", parts, out_root, collision_parts=[[body]],
        notes="벽걸이 에어컨 실외기 80 x 36 x 55, 받침 포함. 원점 벽면 바닥 중심, 팬이 -Y.", texture_size=1024)


def build_convex_mirror(out_root):
    """골목 볼록거울. 주황 테두리, 금속 거울면(루멘 반사), 벽 브래킷. 원점은 벽면 거울 중심."""
    ig.reset_scene()
    orange = ig.mat_plastic("MirrorRim", (0.90, 0.35, 0.05), roughness=0.5, bump=0.01)
    mirror = ig.mat_metal("MirrorFace", (0.92, 0.92, 0.92), roughness=0.04, streak=0.0, anisotropic=False)
    steel = ig.mat_metal("MirrorSteel", (0.40, 0.41, 0.42), roughness=0.5, streak=0.1)
    parts = []
    parts.append(ig.box("bracket", (0.06, 0.30, 0.06), location=(0.0, -0.15, 0.0), material=steel))
    parts.append(ig.cylinder("joint", 0.03, 0.06, location=(0.0, -0.31, 0.0), segments=16, material=steel))
    rim = ig.torus("rim", 0.29, 0.02, location=(0.0, -0.36, 0.0), rotation=(math.pi * 0.5, 0.0, 0.0),
                   major_segments=48, minor_segments=10, material=orange)
    parts.append(rim)
    # 볼록면: 얕은 돔.
    dome = ig.lathe("dome", [(0.0, 0.05), (0.12, 0.045), (0.22, 0.03), (0.285, 0.0)], segments=48,
                    location=(0.0, -0.36, 0.0), rotation=(math.pi * 0.5, 0.0, 0.0), material=mirror)
    parts.append(dome)
    return ig.build_asset(
        "SM_ConvexMirror", "prop", parts, out_root, collision_parts=[],
        notes="골목 볼록거울 지름 62, 벽에서 40 cm. 원점은 벽면의 거울 중심 높이, 거울면 -Y. 충돌 없음.", texture_size=512)


BUILDERS = {"cone": build_cone, "pole": build_pole, "meter": build_gas_meter, "ac": build_ac_outdoor,
            "mirror": build_convex_mirror}


def main():
    out_root = ig.out_root_from_argv()
    only = [a for a in sys.argv[sys.argv.index("--") + 2:]] if "--" in sys.argv else []
    for key, builder in BUILDERS.items():
        if not only or key in only:
            builder(out_root)


main()
