"""미사용 소화전함의 이전 원본을 보존하는 빌더. 현행 설비는 build_fire_safety.py."""

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



def main():
    out_root = ig.out_root_from_argv()
    only = sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else []
    if not only or "box" in only:
        build_box(out_root)


main()
