"""403호 빌트인 주방. 씬(BuildApartment)의 상자 배치를 실제 가구로 바꾼다.

기준: Content/SourceArt/AI/SheetOneroomKitchenAppliancesReference.png 우상·좌하.
좌표계는 씬 그대로다: 주방은 X 134..190에 서 있고 앞면이 -X, Y로 176 길게
뻗는다. 원점은 각 에셋의 바닥 중심(런의 경우 (162, 128, 0)), Z는 바닥.

    SM_KitchenBaseRun   하부장 캐비닛 + 걸레받이 + 상판 + 문 둘 + 백스플래시
    SM_DrumWasher       하부장 남쪽 끝의 드럼세탁기 앞면(포트홀·조작 패널)
    SM_KitchenWallUnits 상부장 + 밸런스 + LED 띠
    SM_RangeHood        후드와 연통
    SM_Microwave        삼성 제품 사진과 치수를 참고한 전자레인지
    SM_KitchenSink      싱크 볼과 구즈넥 수전
    SM_InductionHob     인덕션 유리판

    blender -b --factory-startup --python Scripts/blender/build_kitchen.py -- <out_dir>
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
PANELS = os.path.join(REPO, "Content", "SourceArt", "AI", "panels")

# 씬 좌표(cm) → 에셋 로컬(m). 런 원점 (162, 128, 0).
RUN_X, RUN_Y = 1.62, 1.28


def L(x_cm, y_cm, z_cm):
    return ((x_cm / 100.0) - RUN_X, (y_cm / 100.0) - RUN_Y, z_cm / 100.0)


def mats():
    # 실물 인조대리석 샘플을 참고한 원본. 검고 흰 절차식 노이즈는 상판 전체에서
    # 반짝이는 모래처럼 보여 없앴다. 표면은 평평하고 입자는 색상에만 남긴다.
    stone = ig.mat_image("CounterSolidSurface", os.path.join(REPO, "Content", "SourceArt", "AI", "CounterSolidSurface_20260915.png"), roughness=.34)
    tree = stone.node_tree
    texture = next(n for n in tree.nodes if n.type == "TEX_IMAGE")
    texture.projection = "BOX"
    texture.projection_blend = .12
    coordinates = tree.nodes.new("ShaderNodeTexCoord")
    scale = tree.nodes.new("ShaderNodeVectorMath")
    scale.operation = "SCALE"
    scale.inputs[3].default_value = 1.0 / .30
    tree.links.new(coordinates.outputs["Object"], scale.inputs[0])
    tree.links.new(scale.outputs["Vector"], texture.inputs["Vector"])
    return {
        "gloss": ig.mat_painted_steel("GlossWhite", (0.86, 0.86, 0.84), roughness=0.16, wear=0.08, bump=0.008,
                                      dirt_color=(0.55, 0.53, 0.5)),
        "stone": stone,
        "dark": ig.mat_plastic("DarkPlastic", (0.02, 0.02, 0.022), roughness=0.5),
        "steel": ig.mat_metal("Stainless", (0.78, 0.78, 0.77), roughness=0.36, streak=0.1),
        "chrome": ig.mat_metal("Chrome", (0.86, 0.86, 0.86), roughness=0.22, streak=0.05, anisotropic=False),
        "glass": ig.mat_glass("Glass"),
        "blackglass": ig.mat_gloss("BlackGlass", (0.006, 0.006, 0.007), roughness=0.08),
        "rubber": ig.mat_rubber("Rubber"),
        "led": ig.mat_emissive("LedStrip", (1.0, 0.93, 0.8), strength=5.0),
    }


def facing_neg_x():
    """image_quad는 -Y를 본다. yaw -90이면 -X를 본다((0,-1) → (-1,0))."""
    return (0.0, 0.0, -math.pi * 0.5)


def build_base_run(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    # 캐비닛 몸체 56 x 176 x 72(Z 10..82), 걸레받이 48 x 176 x 10, 상판 60 x 176 x 5.
    carcass = ig.box("carcass", (0.56, 1.76, 0.72), location=(0.0, 0.0, 0.46), bevel=0.004, segments=2,
                     material=m["gloss"])
    parts.append(carcass)
    kick = ig.box("toe_kick", (0.48, 1.76, 0.10), location=(0.04, 0.0, 0.05), material=m["dark"])
    parts.append(kick)
    top = ig.box("worktop", (0.60, 1.76, 0.05), location=(-0.01, 0.0, 0.845), bevel=0.006, segments=2,
                 material=m["stone"])
    sink_cut = ig.box("sink_cut", (0.46, 0.45, 0.10), location=(-0.02, (1.825 - RUN_Y), 0.845))
    ig.boolean(top, sink_cut, "DIFFERENCE")
    parts.append(top)
    splash = ig.box("splashback", (0.06, 1.76, 0.20), location=(0.25, 0.0, 0.96), bevel=0.003, segments=1,
                    material=m["stone"])
    parts.append(splash)
    # 세탁기 자리(Y 70): 앞판을 뚫어 포트홀 너머로 캐비닛 벽이 보이지 않게 한다.
    port_hole = ig.cylinder("port_hole", 0.215, 0.20, location=(-0.24, 0.70 - RUN_Y, 0.44),
                            rotation=(0.0, math.pi * 0.5, 0.0), segments=48)
    ig.boolean(carcass, port_hole, "DIFFERENCE")
    # 문 둘(인덕션 아래, 싱크 아래): 54 x 68, 앞면 X 133.2 → 로컬 -0.288. 홈 손잡이.
    for y_cm in (129.0, 186.0):
        y = y_cm / 100.0 - RUN_Y
        door = ig.box(f"door_{int(y_cm)}", (0.018, 0.54, 0.68), location=(-0.279, y, 0.46), bevel=0.003, segments=2,
                      material=m["gloss"])
        groove = ig.box(f"groove_{int(y_cm)}", (0.02, 0.50, 0.016), location=(-0.288, y, 0.78))
        ig.boolean(door, groove, "DIFFERENCE")
        parts.append(door)
        groove_liner = ig.box(f"gliner_{int(y_cm)}", (0.014, 0.50, 0.016), location=(-0.281, y, 0.78),
                              material=m["dark"])
        parts.append(groove_liner)
        # 문 옆 가스켓 그림자 대신 실제 1 mm 간격: 캐비닛 앞면을 2 mm 파낸다.
        gap = ig.box(f"gap_{int(y_cm)}", (0.01, 0.55, 0.69), location=(-0.28, y, 0.46))
        ig.boolean(carcass, gap, "DIFFERENCE")
    coll = [
        ig.box("c_carcass", (0.56, 1.76, 0.72), location=(0.0, 0.0, 0.46)),
        ig.box("c_top", (0.60, 1.76, 0.05), location=(-0.01, 0.0, 0.845)),
    ]
    for c in coll:
        c.hide_render = True
    return ig.build_asset(
        "SM_KitchenBaseRun", "large", parts, out_root, collision_parts=[[c] for c in coll],
        notes="403호 하부장 런 56 x 176 x 87(상판 포함). 앞면 -X, 원점 (162,128,0). 싱크 자리 Y 160..205 뚫림.",
        texture_size=2048)


def build_washer(out_root):
    ig.reset_scene()
    m = mats()
    panel_mat = ig.mat_image_uv("WasherPanel", os.path.join(PANELS, "WasherPanel.png"), roughness=0.3)
    parts = []
    # 세탁기 앞면 원점: 씬 (133.4, 70, 0). 앞면 판 54 x 68, 포트홀 지름 42.
    face = ig.box("face", (0.02, 0.54, 0.68), location=(0.0, 0.0, 0.46), bevel=0.004, segments=2, material=m["gloss"])
    port_cut = ig.cylinder("port_cut", 0.21, 0.06, location=(0.0, 0.0, 0.44), rotation=(0.0, math.pi * 0.5, 0.0),
                           segments=48)
    ig.boolean(face, port_cut, "DIFFERENCE")
    parts.append(face)
    ring = ig.torus("port_ring", 0.205, 0.018, location=(-0.012, 0.0, 0.44), rotation=(0.0, math.pi * 0.5, 0.0),
                    major_segments=48, minor_segments=12, material=m["steel"])
    parts.append(ring)
    glass = ig.lathe("port_glass", [(0.0, -0.02), (0.14, -0.03), (0.19, -0.01), (0.19, 0.0), (0.0, 0.0)],
                     segments=48, location=(-0.01, 0.0, 0.44), rotation=(0.0, math.pi * 0.5, 0.0), material=m["glass"])
    parts.append(glass)
    # Y축 +90도는 프로파일의 +Z를 +X(안쪽)로 보낸다. 드럼은 기계 속으로 들어간다.
    drum = ig.lathe("drum", [(0.0, 0.0), (0.18, 0.0), (0.19, 0.30), (0.0, 0.30)], segments=40,
                    location=(0.02, 0.0, 0.44), rotation=(0.0, math.pi * 0.5, 0.0), material=m["steel"])
    parts.append(drum)
    handle = ig.box("handle", (0.02, 0.04, 0.14), location=(-0.026, 0.24, 0.44), bevel=0.005, segments=2,
                    material=m["dark"])
    parts.append(handle)
    # 조작 패널: 위쪽 54 x 11 cm 판에 생성 시트(비율 54:11)를 붙인다.
    panel = ig.image_quad("panel", (0.52, 0.10), (-0.012, 0.0, 0.76), panel_mat, rotation=facing_neg_x())
    parts.append(panel)
    knob = ig.cylinder("knob", 0.022, 0.018, location=(-0.02, 0.17, 0.76), rotation=(0.0, math.pi * 0.5, 0.0),
                       segments=24, bevel=0.002, bevel_segments=1, material=m["dark"])
    parts.append(knob)
    return ig.build_asset(
        "SM_DrumWasher", "prop", parts, out_root, collision_parts=[[face]],
        notes="드럼세탁기 앞면 54 x 68, 포트홀 42. 앞면 -X, 원점 씬 (133.4, 70, 0). 하부장 캐비닛 안에 들어간다.",
        texture_size=1024)


def build_wall_units(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    # 상부장 32 x 176 x 68, Z 144..212, 앞면 X 158 → 원점 (174, 128, 0).
    carcass = ig.box("carcass", (0.32, 1.76, 0.68), location=(0.0, 0.0, 1.78), bevel=0.004, segments=2,
                     material=m["gloss"])
    parts.append(carcass)
    for y_cm in (70.0, 195.0):
        y = y_cm / 100.0 - RUN_Y
        door = ig.box(f"door_{int(y_cm)}", (0.018, 0.54, 0.64), location=(-0.169, y, 1.78), bevel=0.003, segments=2,
                      material=m["gloss"])
        groove = ig.box(f"groove_{int(y_cm)}", (0.02, 0.50, 0.016), location=(-0.178, y, 1.48))
        ig.boolean(door, groove, "DIFFERENCE")
        parts.append(door)
        parts.append(ig.box(f"gliner_{int(y_cm)}", (0.014, 0.50, 0.016), location=(-0.171, y, 1.48),
                            material=m["dark"]))
        gap = ig.box(f"gap_{int(y_cm)}", (0.01, 0.55, 0.65), location=(-0.16, y, 1.78))
        ig.boolean(carcass, gap, "DIFFERENCE")
    # 밸런스와 LED 띠(아래를 향한 발광면).
    valance = ig.box("valance", (0.05, 1.76, 0.05), location=(-0.16, 0.0, 1.43), bevel=0.003, segments=1,
                     material=m["gloss"])
    parts.append(valance)
    strip = ig.box("led", (0.02, 1.68, 0.006), location=(-0.174, 0.0, 1.402), material=m["led"])
    parts.append(strip)
    coll = [ig.box("c_carcass", (0.34, 1.76, 0.68), location=(-0.01, 0.0, 1.78))]
    coll[0].hide_render = True
    return ig.build_asset(
        "SM_KitchenWallUnits", "large", parts, out_root, collision_parts=[[coll[0]]],
        notes="403호 상부장 32 x 176 x 68(Z 144..212). 앞면 -X, 원점 (174,128,0). LED 띠는 발광 텍스처.",
        texture_size=2048)


def build_range_hood(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    # 씬: 몸체 36 x 52 x 26 at (172,128,168), 필터판 24 x 52 x 9 at (166,128,151), 연통 18 x 26 x 24 at (178,128,200).
    # 원점 (172, 128, 0).
    body = ig.box("body", (0.36, 0.52, 0.26), location=(0.0, 0.0, 1.68), bevel=0.006, segments=2, material=m["steel"])
    parts.append(body)
    canopy = ig.box("canopy", (0.24, 0.52, 0.09), location=(-0.06, 0.0, 1.51), bevel=0.004, segments=2,
                    material=m["steel"])
    parts.append(canopy)
    # 필터: 아래면의 검정 격자판 둘.
    for y in (-0.12, 0.12):
        filt = ig.box(f"filter_{int((y + 1) * 100)}", (0.20, 0.22, 0.004), location=(-0.06, y, 1.464), material=m["dark"])
        parts.append(filt)
    strip = ig.box("hood_led", (0.02, 0.46, 0.004), location=(-0.17, 0.0, 1.464), material=m["led"])
    parts.append(strip)
    chimney = ig.box("chimney", (0.18, 0.26, 0.24), location=(0.06, 0.0, 2.0), bevel=0.004, segments=1,
                     material=m["steel"])
    parts.append(chimney)
    return ig.build_asset(
        "SM_RangeHood", "prop", parts, out_root, collision_parts=[[body, canopy]],
        notes="레인지 후드. 원점 (172,128,0), 앞면 -X. 몸체 Z 155..181, 연통 188..212.",
        texture_size=1024)


def build_microwave(out_root):
    """삼성 MS20A3010AL 제품 사진과 440×259×337.5mm 외형 치수를 참고한다."""
    ig.reset_scene()
    m = mats()
    paint = ig.mat_plastic("검정분체도장", (.024, .026, .028), roughness=.34, bump=.003)
    lettering = ig.mat_plastic("흰눈금", (.65, .67, .67), roughness=.5, bump=0)
    # 앞은 -X. 깊이와 폭을 바꾸지 않는다. 고무발 8mm 위에 몸체를 얹는다.
    body = ig.box("몸체", (.316, .440, .251), (.008, 0, .1335),
                  bevel=.005, segments=3, material=paint)
    parts = [body]
    parts.append(ig.box("유리문", (.010, .355, .241), (-.155, -.041, .134),
                        bevel=.003, segments=2, material=m["blackglass"]))
    parts.append(ig.box("조작부", (.011, .077, .241), (-.155, .1785, .134),
                        bevel=.002, segments=2, material=m["blackglass"]))
    # 창의 천공망은 가까운 사진에서 보이는 어두운 회색 판으로 굽는다.
    mesh_window = ig.mat_speckle("전자파차폐망", (.009, .01, .011), (.029, .03, .033),
                                 scale=1800, threshold=.72, roughness=.46)
    parts.append(ig.box("창", (.001, .260, .129), (-.161, -.045, .128),
                        bevel=.004, segments=2, material=mesh_window))
    for z in (.089, .190):
        parts.append(ig.cylinder("다이얼테", .018, .011, (-.166, .174, z),
                                 rotation=(0, math.pi/2, 0), segments=40, bevel=.001, material=m["chrome"]))
        parts.append(ig.cylinder("다이얼", .0155, .012, (-.170, .174, z),
                                 rotation=(0, math.pi/2, 0), segments=40, bevel=.001, material=m["dark"]))
        parts.append(ig.box("다이얼지시선", (.0008, .001, .009), (-.1765, .174, z+.011), material=lettering))
        for index in range(9):
            angle = math.radians(-135 + index*33.75)
            parts.append(ig.box("조절눈금", (.0007, .0014, .0014),
                                (-.161, .174 + .024*math.sin(angle), z+.024*math.cos(angle)), material=lettering))
    parts.append(ig.box("문열림버튼", (.003, .062, .025), (-.163, .178, .031),
                        bevel=.001, segments=1, material=paint))
    # 통풍구는 몸체에서 떨어진 장식이 되지 않도록 옆판에 붙인다.
    for index in range(12):
        parts.append(ig.box("측면통풍구", (.003, .0008, .035),
                            (-.03+index*.009, -.2204, .104), material=m["dark"]))
    for index, (x, y) in enumerate(((-.119, -.178), (.127, -.178), (-.119, .178), (.127, .178))):
        parts.append(ig.cylinder(f"고무발_{index}", .010, .008, (x,y,.004), segments=12, material=m["rubber"]))
    return ig.build_asset(
        "SM_Microwave", "prop", parts, out_root, collision_parts=[[body]],
        notes="삼성 MS20A3010AL 사진과 치수를 참고한 44 x 33.75 x 25.9cm 전자레인지. 앞면 -X, 원점 (157,68,87). 고무발 네 개를 상판에 놓고 뒤쪽 환기 여유를 둔다.",
        texture_size=1024)


def build_sink(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    # 싱크 볼 46 x 47, 깊이 26, 림 54 x 55. 원점: 씬 (160, 182, 87.4) 상판 윗면 중심 → 여기서는 Z 0이 림 윗면.
    basin = ig.box("basin", (0.46, 0.47, 0.26), location=(0.0, 0.0, -0.13), bevel=0.02, segments=4, material=m["steel"])
    hollow = ig.box("hollow", (0.42, 0.43, 0.26), location=(0.0, 0.0, -0.11), bevel=0.02, segments=4)
    ig.boolean(basin, hollow, "DIFFERENCE")
    parts.append(basin)
    rim = ig.box("rim", (0.54, 0.55, 0.014), location=(0.0, 0.0, -0.007), bevel=0.003, segments=2, material=m["steel"])
    rim_cut = ig.box("rim_cut", (0.42, 0.43, 0.03), location=(0.0, 0.0, -0.007))
    ig.boolean(rim, rim_cut, "DIFFERENCE")
    parts.append(rim)
    drain = ig.cylinder("drain", 0.045, 0.006, location=(0.0, 0.0, -0.237), segments=32, material=m["dark"])
    parts.append(drain)
    drain_ring = ig.torus("drain_ring", 0.044, 0.004, location=(0.0, 0.0, -0.234), material=m["chrome"])
    parts.append(drain_ring)
    # 구즈넥 수전: 씬에서 기둥 (178,182) → 로컬 x +0.18, 호가 -X로 넘어와 (160,182,107)에서 떨어진다.
    column = ig.cylinder("tap_column", 0.022, 0.13, location=(0.18, 0.0, 0.065), segments=32, bevel=0.003,
                         bevel_segments=2, material=m["chrome"])
    parts.append(column)
    base = ig.cylinder("tap_base", 0.03, 0.01, location=(0.18, 0.0, 0.005), segments=32, bevel=0.002, bevel_segments=1,
                       material=m["chrome"])
    parts.append(base)
    neck = ig.pipe("tap_neck", [
        (0.18, 0.0, 0.13), (0.18, 0.0, 0.24), (0.15, 0.0, 0.265), (0.05, 0.0, 0.265), (0.0, 0.0, 0.24), (0.0, 0.0, 0.20),
    ], radius=0.016, resolution=16, corner_radius=0.045, material=m["chrome"])
    parts.append(neck)
    aerator = ig.cylinder("aerator", 0.012, 0.012, location=(0.0, 0.0, 0.194), segments=20, material=m["dark"])
    parts.append(aerator)
    lever = ig.pipe("lever", [(0.18, 0.02, 0.125), (0.18, 0.06, 0.13), (0.18, 0.10, 0.145)], radius=0.008,
                    resolution=12, corner_radius=0.02, material=m["chrome"])
    parts.append(lever)
    return ig.build_asset(
        "SM_KitchenSink", "prop", parts, out_root, collision_parts=[[basin, rim]],
        notes="싱크 볼과 구즈넥 수전. 원점은 림 윗면 중심(씬 (160,182,87.4)). 볼은 Z -26..0, 수전 Z 0..26.5.",
        texture_size=1024)


def build_hob(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    # 46 x 50 x 1.4 검정 유리, 원점 상판 위 중심(씬 (160,128,87)). 링 둘과 터치 띠는 얕은 홈.
    glass = ig.box("glass", (0.46, 0.50, 0.014), location=(0.0, 0.0, 0.007), bevel=0.003, segments=2,
                   material=m["blackglass"])
    parts.append(glass)
    grey = ig.mat_plastic("RingGrey", (0.30, 0.30, 0.30), roughness=0.3, bump=0.0)
    for (x_cm, y_cm) in ((150.0, 116.0), (170.0, 140.0)):
        x, y = x_cm / 100.0 - 1.60, y_cm / 100.0 - 1.28
        ring = ig.torus(f"ring_{int(x_cm)}", 0.09, 0.0015, location=(x, y, 0.0145), major_segments=48, minor_segments=6,
                        material=grey)
        parts.append(ring)
        dot = ig.cylinder(f"dot_{int(x_cm)}", 0.008, 0.0006, location=(x, y, 0.0143), segments=16, material=grey)
        parts.append(dot)
    touch = ig.box("touch", (0.05, 0.30, 0.0006), location=(-0.19, 0.0, 0.0143), material=grey)
    parts.append(touch)
    return ig.build_asset(
        "SM_InductionHob", "prop", parts, out_root, collision_parts=[[glass]],
        notes="인덕션 46 x 50 x 1.4. 원점 상판 위 중심(씬 (160,128,87)).",
        texture_size=1024)


BUILDERS = {
    "base": build_base_run,
    "washer": build_washer,
    "wall": build_wall_units,
    "hood": build_range_hood,
    "microwave": build_microwave,
    "sink": build_sink,
    "hob": build_hob,
}


def main():
    out_root = ig.out_root_from_argv()
    only = sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else []
    for key, builder in BUILDERS.items():
        if not only or key in only:
            builder(out_root)


main()
