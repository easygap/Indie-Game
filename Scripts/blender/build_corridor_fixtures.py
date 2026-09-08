"""복도 설비 소품. SM_FireExtinguisherBox(소화전함)와 SM_FireExtinguisher(소화기).

기준: Content/SourceArt/AI/SheetVillaCorridorFixturesReference.png 좌하.
씬에서 소화전함은 남쪽 벽에 26 x 9 x 34 cm 상자로 걸려 있고 「소화전」이
인쇄돼 있었다. 같은 자리·같은 크기에 문짝이 4 mm 들어간 적색 도장 강판함,
유리창, 크롬 손잡이, 도장 위에 흰 글자로 다시 만든다. 글자는 텍스처가 아니라
얇은 문자 기하라 굽힐 때 알베도에 그대로 들어간다.

소화기는 밤1에 떨어지는 그 물리 소품이다. 반지름 7.5 cm·높이 48 cm의 예전
원기둥과 같은 봉투 안에서 본체·목·밸브·레버·압력계·호스·라벨을 만든다.
원점은 둘 다 바닥 중심, 앞면은 -Y.

    blender -b --factory-startup --python Scripts/blender/build_corridor_fixtures.py -- <out_dir>
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import bpy  # noqa: E402

import ig_blender_lib as ig  # noqa: E402

FONT_CANDIDATES = (
    r"C:\Windows\Fonts\malgunbd.ttf",
    r"C:\Windows\Fonts\malgun.ttf",
)


def load_font():
    for path in FONT_CANDIDATES:
        if os.path.isfile(path):
            return bpy.data.fonts.load(path)
    return None


def text_mesh(name, text, size, location, material, rotation=(0.0, 0.0, 0.0), extrude=0.0004,
              align="CENTER"):
    """한글 문자를 얇은 판 기하로. 원점은 글줄 가운데 아래."""
    curve = bpy.data.curves.new(name + "_text", "FONT")
    curve.body = text
    font = load_font()
    if font is not None:
        curve.font = font
    curve.size = size
    curve.extrude = extrude
    curve.align_x = align
    curve.align_y = "CENTER"
    text_ob = ig._link(bpy.data.objects.new(name + "_text", curve))
    text_ob.location = location
    text_ob.rotation_euler = rotation
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(text_ob.evaluated_get(depsgraph))
    me.name = name
    bpy.data.objects.remove(text_ob, do_unlink=True)
    bpy.data.curves.remove(curve)
    ob = ig._link(bpy.data.objects.new(name, me))
    ob.location = location
    ob.rotation_euler = rotation
    ig.assign_material(ob, material)
    return ob


def build_box(out_root):
    ig.reset_scene()
    red = ig.mat_painted_steel("FireRed", (0.42, 0.03, 0.025), roughness=0.5, wear=0.3,
                               dirt_color=(0.16, 0.05, 0.04))
    white = ig.mat_plastic("SignWhite", (0.85, 0.85, 0.82), roughness=0.45, bump=0.0)
    chrome = ig.mat_metal("Chrome", (0.86, 0.86, 0.86), roughness=0.2, streak=0.05, anisotropic=False)
    black = ig.mat_rubber("Gasket")
    glass = ig.mat_glass("Glass")

    W, D, H = 0.26, 0.09, 0.34
    front = -D * 0.5
    parts = []
    # 외함: 앞이 열린 상자. 옆·위·아래·뒤 판 두께 1 mm.
    shell = ig.box("shell", (W, D, H), origin="bottom", bevel=0.002, segments=2, material=red)
    hollow = ig.box("hollow", (W - 0.004, D, H - 0.004), location=(0.0, -0.004, H * 0.5))
    ig.boolean(shell, hollow, "DIFFERENCE")
    parts.append(shell)
    # 문짝: 4 mm 들어간 자리에, 창을 뚫고 유리를 끼운다.
    door = ig.box("door", (W - 0.012, 0.008, H - 0.012), location=(0.0, front + 0.004 + 0.004, H * 0.5),
                  bevel=0.0015, segments=1, material=red)
    window = ig.box("window_cut", (0.10, 0.03, 0.15), location=(0.0, front + 0.008, H * 0.56))
    ig.boolean(door, window, "DIFFERENCE")
    parts.append(door)
    pane = ig.box("pane", (0.10, 0.002, 0.15), location=(0.0, front + 0.008, H * 0.56), material=glass)
    parts.append(pane)
    gasket = ig.box("gasket", (0.108, 0.003, 0.158), location=(0.0, front + 0.0065, H * 0.56), material=black)
    gasket_cut = ig.box("gasket_cut", (0.098, 0.01, 0.148), location=(0.0, front + 0.0065, H * 0.56))
    ig.boolean(gasket, gasket_cut, "DIFFERENCE")
    parts.append(gasket)
    # 손잡이: 왼쪽 세로 크롬 바.
    handle = ig.pipe("handle", [
        (-0.095, front + 0.004, H * 0.30), (-0.095, front - 0.012, H * 0.30),
        (-0.095, front - 0.012, H * 0.42), (-0.095, front + 0.004, H * 0.42),
    ], radius=0.004, resolution=12, corner_radius=0.006, material=chrome)
    parts.append(handle)
    # 「소화전」 흰 글자, 창 아래.
    # 문짝 앞면은 front + 0.004. 글자판(두께 0.8 mm)이 그 앞에 온전히 나오게 둔다.
    label = text_mesh("label", "소화전", 0.032, (0.0, front + 0.004 - 0.0006, H * 0.18), white,
                      rotation=(math.pi * 0.5, 0.0, 0.0))
    parts.append(label)
    # 경첩 너클 둘(오른쪽).
    for z in (H * 0.2, H * 0.8):
        parts.append(ig.cylinder(f"hinge_{int(z * 1000)}", 0.004, 0.05, location=(W * 0.5 - 0.004, front + 0.005, z),
                                 segments=12, material=chrome))
    return ig.build_asset(
        "SM_FireExtinguisherBox", "prop", parts, out_root,
        collision_parts=[[shell]],
        notes="복도 남쪽 벽의 소화전함 26 x 9 x 34. 앞면 -Y, 원점 바닥 중심. Glass 슬롯 있음.",
        texture_size=1024)


def build_extinguisher(out_root):
    ig.reset_scene()
    red = ig.mat_painted_steel("ExtinguisherRed", (0.45, 0.035, 0.03), roughness=0.42, wear=0.2,
                               dirt_color=(0.18, 0.05, 0.04))
    brass = ig.mat_metal("Brass", (0.75, 0.55, 0.25), roughness=0.38, streak=0.06, anisotropic=False)
    black = ig.mat_plastic("BlackPlastic", (0.02, 0.02, 0.022), roughness=0.5)
    rubber = ig.mat_rubber("Hose", (0.025, 0.025, 0.025), roughness=0.75)
    white = ig.mat_plastic("LabelWhite", (0.86, 0.86, 0.84), roughness=0.5, bump=0.0)
    glass = ig.mat_glass("Glass")

    R = 0.075
    parts = []
    # 본체: 바닥 굽, 원통, 어깨, 목.
    body = ig.lathe("body", [
        (0.0, 0.0), (0.055, 0.0), (0.068, 0.006), (R, 0.02), (R, 0.33), (0.066, 0.36),
        (0.04, 0.375), (0.02, 0.38), (0.02, 0.40), (0.0, 0.40),
    ], segments=48, material=red)
    parts.append(body)
    # 밸브 헤드(황동)와 레버 둘(검정), 안전핀 고리.
    head = ig.box("head", (0.05, 0.045, 0.045), location=(0.0, 0.0, 0.42), bevel=0.006, segments=2, material=brass)
    parts.append(head)
    for dz, length in ((0.045, 0.11), (0.06, 0.12)):
        lever = ig.box(f"lever_{int(dz * 1000)}", (0.024, length, 0.007),
                       location=(0.0, -length * 0.5 + 0.02, 0.42 + dz), bevel=0.002, segments=1, material=black)
        parts.append(lever)
    pin = ig.torus("pin_ring", 0.014, 0.0015, location=(0.03, 0.0, 0.465), rotation=(0.0, math.pi * 0.5, 0.0),
                   material=brass)
    parts.append(pin)
    # 압력계: 작은 원판, 유리면.
    gauge = ig.cylinder("gauge", 0.013, 0.008, location=(0.022, -0.028, 0.43), rotation=(math.pi * 0.5, 0.0, 0.0),
                        segments=24, material=black)
    parts.append(gauge)
    gauge_glass = ig.cylinder("gauge_glass", 0.011, 0.001, location=(0.022, -0.0325, 0.43),
                              rotation=(math.pi * 0.5, 0.0, 0.0), segments=24, material=glass)
    parts.append(gauge_glass)
    # 호스: 헤드 옆에서 나와 본체 옆에 걸린다. 노즐은 검정 원뿔대.
    hose = ig.pipe("hose", [
        (0.03, 0.0, 0.43), (0.09, 0.0, 0.40), (0.092, 0.0, 0.25), (0.088, 0.01, 0.12),
    ], radius=0.007, resolution=12, corner_radius=0.03, material=rubber)
    parts.append(hose)
    nozzle = ig.cylinder("nozzle", 0.014, 0.06, location=(0.088, 0.01, 0.09), segments=20,
                         radius_top=0.009, material=black)
    parts.append(nozzle)
    clip = ig.box("clip", (0.03, 0.02, 0.02), location=(0.08, 0.0, 0.25), bevel=0.003, segments=1, material=black)
    parts.append(clip)
    # 라벨 띠: 흰 바탕 얇은 원통 띠, 글자는 앞면(-Y)에.
    band = ig.lathe("band", [(R + 0.0006, 0.10), (R + 0.0006, 0.27)], segments=48, material=white)
    parts.append(band)
    label = text_mesh("label", "소화기", 0.028, (0.0, -R - 0.0006, 0.22), red,
                      rotation=(math.pi * 0.5, 0.0, 0.0))
    parts.append(label)
    sub = text_mesh("label_sub", "ABC 분말 3.3kg", 0.011, (0.0, -R - 0.0006, 0.17), black,
                    rotation=(math.pi * 0.5, 0.0, 0.0))
    parts.append(sub)
    return ig.build_asset(
        "SM_FireExtinguisher", "prop", parts, out_root,
        collision_parts=[[body], [head]],
        notes="밤1에 떨어지는 소화기. 반지름 7.5, 높이 48 안. 원점 바닥 중심, 라벨 정면 -Y. 물리 소품이라 충돌은 본체·헤드 두 껍데기.",
        texture_size=1024)


def main():
    out_root = ig.out_root_from_argv()
    only = sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else []
    if not only or "box" in only:
        build_box(out_root)
    if not only or "extinguisher" in only:
        build_extinguisher(out_root)


main()
