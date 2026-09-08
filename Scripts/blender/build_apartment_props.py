"""403호 가구·설비. SM_Wardrobe(옷장)와 SM_WallAirConditioner(벽걸이 에어컨).

씬(BuildApartment) 좌표계와 상자 봉투를 그대로 잇는다.
- 옷장: 상자 40 x 80 x 180, 중심 (-170, -104, 90). 서쪽 벽에 등을 대고 있으니
  앞면이 +X다. 원점은 바닥 중심.
- 에어컨: 상자 82 x 19 x 27, 중심 (40, -206, 196). 북쪽 벽에 붙어 있고 루버가
  Y -196.2에 있으니 앞면이 +Y다. 원점은 상자 바닥 중심 (40, -206, 182.5).

    blender -b --factory-startup --python Scripts/blender/build_apartment_props.py -- <out_dir>
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import bpy  # noqa: E402

import ig_blender_lib as ig  # noqa: E402


def mat_wood(name, color, grain_axis=(1.0, 40.0, 6.0), roughness=0.42):
    """저가 무늬목 합판. 한 축으로 늘인 노이즈가 결이 된다."""
    mat = bpy.data.materials.new(name)
    bsdf = ig._principled(mat)
    tree, nodes, links = ig._nodes(mat)
    coords = ig._object_coords(nodes)
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = grain_axis
    links.new(coords, mapping.inputs["Vector"])
    grain = ig._noise(nodes, links, 18.0, detail=4.0, roughness=0.6, coords=mapping.outputs["Vector"])
    tone = ig._map_range(nodes, links, grain.outputs["Fac"], 0.3, 0.7, 0.72, 1.12)
    base = nodes.new("ShaderNodeRGB")
    base.outputs[0].default_value = (*color, 1.0)
    tinted = nodes.new("ShaderNodeMixRGB")
    tinted.blend_type = "MULTIPLY"
    tinted.inputs["Fac"].default_value = 1.0
    links.new(base.outputs[0], tinted.inputs["Color1"])
    links.new(tone.outputs["Result"], tinted.inputs["Color2"])
    links.new(tinted.outputs[0], bsdf.inputs["Base Color"])
    rough = ig._map_range(nodes, links, grain.outputs["Fac"], 0.3, 0.7, roughness - 0.06, roughness + 0.06)
    links.new(rough.outputs["Result"], bsdf.inputs["Roughness"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.05
    bump.inputs["Distance"].default_value = 0.001
    links.new(grain.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    mat["ig_kind"] = "baked"
    return mat


def build_wardrobe(out_root):
    ig.reset_scene()
    wood = mat_wood("WardrobeWood", (0.16, 0.10, 0.065))
    edge = mat_wood("WardrobeEdge", (0.12, 0.075, 0.05), grain_axis=(30.0, 1.0, 6.0))
    chrome = ig.mat_metal("Chrome", (0.84, 0.84, 0.84), roughness=0.25, streak=0.05, anisotropic=False)
    D, W, H = 0.40, 0.80, 1.80
    parts = []
    # 몸통(앞이 열림) + 플린스 + 상부 코니스.
    body = ig.box("body", (D, W, H - 0.08), location=(0.0, 0.0, 0.08 + (H - 0.08) * 0.5), bevel=0.003, segments=2,
                  material=wood)
    hollow = ig.box("hollow", (D - 0.02, W - 0.036, H - 0.08 - 0.036), location=(0.02, 0.0, 0.08 + (H - 0.08) * 0.5))
    ig.boolean(body, hollow, "DIFFERENCE")
    parts.append(body)
    plinth = ig.box("plinth", (D - 0.03, W - 0.03, 0.08), location=(-0.01, 0.0, 0.04), material=edge)
    parts.append(plinth)
    cornice = ig.box("cornice", (D + 0.02, W + 0.02, 0.03), location=(0.01, 0.0, H - 0.015), bevel=0.004, segments=2,
                     material=edge)
    parts.append(cornice)
    # 문 둘: 각 39.4 x 168, 3 mm 틈. 앞면 +X.
    for side in (-1.0, 1.0):
        y = side * 0.2
        door = ig.box(f"door_{'r' if side > 0 else 'l'}", (0.018, W * 0.5 - 0.006, H - 0.08 - 0.036),
                      location=(D * 0.5 - 0.009, y, 0.08 + (H - 0.08) * 0.5), bevel=0.002, segments=1, material=wood)
        parts.append(door)
        handle = ig.pipe(f"handle_{'r' if side > 0 else 'l'}", [
            (D * 0.5, -side * 0.05, 0.85), (D * 0.5 + 0.03, -side * 0.05, 0.85),
            (D * 0.5 + 0.03, -side * 0.05, 1.15), (D * 0.5, -side * 0.05, 1.15),
        ], radius=0.006, resolution=12, corner_radius=0.01, material=chrome)
        parts.append(handle)
    coll = [ig.box("c_body", (D, W, H), location=(0.0, 0.0, H * 0.5))]
    coll[0].hide_render = True
    return ig.build_asset(
        "SM_Wardrobe", "prop", parts, out_root, collision_parts=[[coll[0]]],
        notes="403호 옷장 40 x 80 x 180. 앞면 +X(서쪽 벽에 등), 원점 바닥 중심 (-170,-104,0).",
        texture_size=1024)


def build_wall_ac(out_root):
    ig.reset_scene()
    white = ig.mat_plastic("AcWhite", (0.86, 0.87, 0.86), roughness=0.4, bump=0.006)
    dark = ig.mat_plastic("AcDark", (0.03, 0.03, 0.032), roughness=0.5)
    led = ig.mat_emissive("AcLed", (0.3, 1.0, 0.5), strength=4.0)
    W, D, H = 0.82, 0.19, 0.27
    parts = []
    # 몸체: 앞이 둥근 쪽(+Y). 뒤판 얇고 앞판이 부풀었다.
    body = ig.lathe("body_profile", [(0.0, 0.0), (0.0, 0.0)], segments=4)  # 자리만, 아래서 지운다
    bpy.data.objects.remove(body, do_unlink=True)
    shell = ig.box("shell", (W, D, H), location=(0.0, 0.0, H * 0.5), bevel=0.03, segments=4, material=white)
    parts.append(shell)
    # 상단 흡입 그릴: 얕은 슬릿 열.
    for i in range(12):
        slot = ig.box(f"inlet_{i}", (0.05, 0.10, 0.004), location=(-0.33 + i * 0.06, -0.02, H - 0.002))
        ig.boolean(shell, slot, "DIFFERENCE")
    # 토출구: 앞 아래 가로 홈과 루버 날개.
    outlet = ig.box("outlet", (0.70, 0.06, 0.04), location=(0.0, D * 0.5, 0.045))
    ig.boolean(shell, outlet, "DIFFERENCE")
    louver = ig.box("louver", (0.70, 0.015, 0.03), location=(0.0, D * 0.5 - 0.005, 0.048),
                    rotation=(math.radians(-25.0), 0.0, 0.0), bevel=0.002, segments=1, material=white)
    parts.append(louver)
    for i in range(9):
        vane = ig.box(f"vane_{i}", (0.004, 0.03, 0.03), location=(-0.32 + i * 0.08, D * 0.5 - 0.02, 0.045),
                      material=dark)
        parts.append(vane)
    # 표시창과 LED.
    window = ig.box("display", (0.09, 0.002, 0.03), location=(0.28, D * 0.5 + 0.0005, 0.13), material=dark)
    parts.append(window)
    dot = ig.cylinder("led", 0.003, 0.001, location=(0.30, D * 0.5 + 0.0018, 0.13), rotation=(math.pi * 0.5, 0.0, 0.0),
                      segments=12, material=led)
    parts.append(dot)
    coll = [ig.box("c_shell", (W, D, H), location=(0.0, 0.0, H * 0.5))]
    coll[0].hide_render = True
    return ig.build_asset(
        "SM_WallAirConditioner", "prop", parts, out_root, collision_parts=[[coll[0]]],
        notes="벽걸이 에어컨 82 x 19 x 27. 앞면 +Y(북쪽 벽에 등), 원점 바닥 중심 (40,-206,182.5).",
        texture_size=1024)


def main():
    out_root = ig.out_root_from_argv()
    only = sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else []
    if not only or "wardrobe" in only:
        build_wardrobe(out_root)
    if not only or "ac" in only:
        build_wall_ac(out_root)


main()
