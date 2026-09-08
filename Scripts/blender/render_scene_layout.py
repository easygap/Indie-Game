"""씬 배치 렌더. 에디터를 못 여는 동안 「메시가 제자리에, 제 크기로, 제 방향으로
들어갔는지」를 눈으로 확인한다.

audit_world_geometry.py --export-layout이 적은 상자 전부(벽·바닥·상자 소품)와
Blender 에셋을 UE 좌표 그대로 세우고 EEVEE로 찍는다. 상자는 재질 이름으로 색을
정하고, 저작 메시는 저장된 .blend와 구운 D/N/ORM을 그대로 쓴다. 폴백 블록아웃
(physics-audit: intentional 저작 메시가 없을 때만…)은 빼고, Blender 원본이 없는
메시(스캔 소품·예전 지오메트리 스크립트 메시)는 구운 바운드 상자로만 선다.

    python Scripts/audit_world_geometry.py --export-layout Saved/scene_layout.json
    blender -b --factory-startup --python Scripts/blender/render_scene_layout.py -- \\
        Saved/scene_layout.json Saved/scene_preview [view ...]

카메라 자리는 아래 VIEWS다. 이름을 주면 그것만 찍는다.
"""

import colorsys
import glob
import hashlib
import json
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
ASSET_ROOT = os.path.join(REPO, "Content", "SourceArt", "Blender")
FALLBACK_MARK = "저작 메시가 없을 때만"

# 이름, 카메라 자리(cm), 보는 점(cm), 화각(도)
VIEWS = {
    "store_entrance": ((2420.0, -470.0, 165.0), (2860.0, -430.0, 110.0), 75.0),
    "store_cooler_open": ((2740.0, -470.0, 150.0), (2925.0, -565.0, 110.0), 70.0),
    "store_counter": ((2660.0, -330.0, 175.0), (2500.0, -255.0, 100.0), 80.0),
    "store_showcase": ((2860.0, -600.0, 160.0), (2700.0, -660.0, 90.0), 75.0),
    "store_rack_freezer": ((2500.0, -470.0, 165.0), (2450.0, -620.0, 80.0), 75.0),
    # 403호는 UpperFloorRoot(Z +900)에 있다.
    "apt_entrance": ((150.0, -140.0, 1050.0), (55.0, -214.0, 1010.0), 75.0),
    "apt_window": ((40.0, 40.0, 1060.0), (-100.0, 214.0, 1050.0), 70.0),
    "apt_kitchen": ((40.0, 100.0, 1050.0), (170.0, 128.0, 1000.0), 75.0),
    # 위에서 내려다보는 평면도. 넷째 값은 천장 컷 — 그보다 높이 놓인 상자는 숨긴다.
    "plan_apartment": ((0.0, 0.0, 1650.0), (0.0, 0.0, 900.0), 55.0, 1125.0),
    "plan_store": ((2680.0, -430.0, 850.0), (2680.0, -430.0, 0.0), 55.0, 255.0),
    "plan_lobby": ((520.0, -305.0, 700.0), (520.0, -305.0, 0.0), 55.0, 235.0),
    "corridor_401": ((-60.0, -330.0, 1055.0), (-150.0, -234.0, 1000.0), 70.0),
    "lobby_mailbox": ((528.0, -335.0, 160.0), (528.0, -236.0, 150.0), 70.0),
    "alley_facade": ((60.0, -600.0, 180.0), (60.0, -395.0, 600.0), 80.0),
    "alley_pole": ((260.0, -620.0, 170.0), (500.0, -422.0, 300.0), 70.0),
}

EMISSIVE_HINTS = ("Glow", "Light", "Screen", "Panel", "Neon", "Sign", "Lamp", "Fluor", "Led")
GLASS_HINTS = ("Glass", "Window", "Pane")
STRUCTURE_HINTS = ("Wall", "Stucco", "Concrete", "Tile", "Ceil", "Floor", "Jangpan", "Plaster",
                   "Brick", "Granite", "Asphalt", "Kerb", "Curb", "Slab", "Stone", "Paint")


def cm(v):
    return (v[0] / 100.0, v[1] / 100.0, v[2] / 100.0)


def ue_rotation(rotation):
    """UE (pitch, yaw, roll) → Blender XYZ 오일러. 저작은 UE 좌표 그대로다."""
    pitch, yaw, roll = rotation
    return (math.radians(roll), -math.radians(pitch), math.radians(yaw))


# --------------------------------------------------------------------------
# 재질
# --------------------------------------------------------------------------

_flat_cache = {}


def flat_material(name):
    key = name or "nullptr"
    if key in _flat_cache:
        return _flat_cache[key]
    mat = bpy.data.materials.new(f"layout_{key[:40]}")
    bsdf = ig._principled(mat)
    lowered = key
    if any(h in lowered for h in GLASS_HINTS):
        bsdf.inputs["Base Color"].default_value = (0.6, 0.75, 0.85, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.1
        bsdf.inputs["Alpha"].default_value = 0.3
        if hasattr(mat, "surface_render_method"):
            mat.surface_render_method = "BLENDED"
        else:
            mat.blend_method = "BLEND"
    elif any(h in lowered for h in EMISSIVE_HINTS):
        bsdf.inputs["Base Color"].default_value = (0.9, 0.9, 0.85, 1.0)
        bsdf.inputs["Emission Color"].default_value = (1.0, 0.95, 0.85, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 2.5
    elif any(h in lowered for h in STRUCTURE_HINTS):
        digest = int(hashlib.md5(key.encode("utf-8")).hexdigest()[:6], 16)
        v = 0.45 + (digest % 40) / 100.0
        bsdf.inputs["Base Color"].default_value = (v, v * 0.98, v * 0.95, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.85
    else:
        digest = int(hashlib.md5(key.encode("utf-8")).hexdigest()[:6], 16)
        r, g, b = colorsys.hsv_to_rgb((digest % 360) / 360.0, 0.35, 0.55)
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.65
    _flat_cache[key] = mat
    return mat


def unknown_mesh_material():
    if "__unknown" in _flat_cache:
        return _flat_cache["__unknown"]
    mat = bpy.data.materials.new("layout_unknown_mesh")
    bsdf = ig._principled(mat)
    bsdf.inputs["Base Color"].default_value = (0.75, 0.35, 0.8, 1.0)
    bsdf.inputs["Alpha"].default_value = 0.45
    if hasattr(mat, "surface_render_method"):
        mat.surface_render_method = "BLENDED"
    else:
        mat.blend_method = "BLEND"
    _flat_cache["__unknown"] = mat
    return mat


def glass_material():
    return flat_material("Glass")


# --------------------------------------------------------------------------
# 기본 도형
# --------------------------------------------------------------------------

_shape_cache = {}


def shape_mesh(shape, material):
    """(도형, 재질) 하나에 메시 데이터 하나. 100 cm 단위 도형을 1 m로 만든다."""
    key = (shape, material.name)
    if key in _shape_cache:
        return _shape_cache[key]
    if shape == "CylinderMesh":
        ob = ig.cylinder("__shape", 0.5, 1.0, segments=24)
    elif shape == "SphereMesh":
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, segments=20, ring_count=12)
        ob = bpy.context.active_object
    elif shape == "ConeMesh":
        ob = ig.cylinder("__shape", 0.5, 1.0, segments=24, radius_top=0.0)
    elif shape == "PlaneMesh":
        ob = ig.box("__shape", (1.0, 1.0, 0.002))
    else:
        ob = ig.box("__shape", (1.0, 1.0, 1.0))
    me = ob.data
    me.materials.clear()
    me.materials.append(material)
    bpy.data.objects.remove(ob, do_unlink=True)
    _shape_cache[key] = me
    return me


# --------------------------------------------------------------------------
# 저작 메시
# --------------------------------------------------------------------------

_asset_cache = {}


def asset_mesh(name):
    """저장된 .blend에서 오브젝트를 가져와 저작 좌표로 되돌리고 구운 재질을 입힌다.
    없으면 None."""
    if name in _asset_cache:
        return _asset_cache[name]
    folder = os.path.join(ASSET_ROOT, name)
    blends = glob.glob(os.path.join(folder, "*.blend"))
    if not blends:
        _asset_cache[name] = None
        return None
    with bpy.data.libraries.load(blends[0], link=False) as (data_from, data_to):
        data_to.objects = [n for n in data_from.objects if n == name]
    if not data_to.objects or data_to.objects[0] is None:
        _asset_cache[name] = None
        return None
    ob = data_to.objects[0]
    ig._link(ob)
    # 저장본은 FBX 내보내기용으로 Y를 뒤집어 둔 상태다. 저작 좌표로 되돌린다.
    ig.mirror_y(ob)
    me = ob.data
    textures = {}
    for role in ("D", "N", "ORM", "E"):
        path = os.path.join(folder, f"{name}_{role}.png")
        if os.path.exists(path):
            textures[role] = path
    slot_names = [slot.material.name if slot.material else "" for slot in ob.material_slots]
    if all(role in textures for role in ("D", "N", "ORM")):
        baked = bpy.data.materials.new(f"preview_{name}")
        bsdf = ig._principled(baked)
        tree, nodes, links = ig._nodes(baked)
        base = nodes.new("ShaderNodeTexImage")
        base.image = bpy.data.images.load(textures["D"])
        base.image.colorspace_settings.name = "sRGB"
        links.new(base.outputs["Color"], bsdf.inputs["Base Color"])
        orm = nodes.new("ShaderNodeTexImage")
        orm.image = bpy.data.images.load(textures["ORM"])
        orm.image.colorspace_settings.name = "Non-Color"
        sep = nodes.new("ShaderNodeSeparateColor")
        links.new(orm.outputs["Color"], sep.inputs["Color"])
        links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
        links.new(sep.outputs["Blue"], bsdf.inputs["Metallic"])
        if "E" in textures:
            emi = nodes.new("ShaderNodeTexImage")
            emi.image = bpy.data.images.load(textures["E"])
            links.new(emi.outputs["Color"], bsdf.inputs["Emission Color"])
            bsdf.inputs["Emission Strength"].default_value = 3.0
        for index, slot_name in enumerate(slot_names):
            if slot_name.startswith("Glass"):
                me.materials[index] = glass_material()
            else:
                me.materials[index] = baked
    bpy.data.objects.remove(ob, do_unlink=True)
    _asset_cache[name] = me
    return me


# --------------------------------------------------------------------------
# 배치
# --------------------------------------------------------------------------

def place(box):
    if box["exempt"].startswith(FALLBACK_MARK):
        return None
    if not box["frame"].startswith("IGPrologueWorldScene.cpp"):
        return None
    mesh_name = box.get("mesh") or ""
    if mesh_name:
        me = asset_mesh(mesh_name)
        if me is not None:
            ob = bpy.data.objects.new(f"{mesh_name}@{box['line']}", me)
            ig._link(ob)
            ob.location = cm(box["location"])
            ob.rotation_euler = ue_rotation(box["rotation"])
            ob.scale = tuple(s / 100.0 for s in box["local_size"])
            return ob
        # Blender 원본이 없는 메시: 구운 바운드 상자.
        me = shape_mesh("CubeMesh", unknown_mesh_material())
        ob = bpy.data.objects.new(f"{mesh_name}@{box['line']}", me)
        ig._link(ob)
        ob.location = cm(box["center"])
        ob.scale = tuple(max(s, 0.5) / 100.0 for s in box["size"])
        return ob
    if box["note"] == "unknown-rotation":
        me = shape_mesh("CubeMesh", flat_material(box["material"]))
        ob = bpy.data.objects.new(f"box@{box['line']}", me)
        ig._link(ob)
        ob.location = cm(box["center"])
        ob.scale = tuple(max(s, 0.5) / 100.0 for s in box["size"])
        return ob
    me = shape_mesh(box["shape"] or "CubeMesh", flat_material(box["material"]))
    ob = bpy.data.objects.new(f"box@{box['line']}", me)
    ig._link(ob)
    ob.location = cm(box["location"])
    ob.rotation_euler = ue_rotation(box["rotation"])
    ob.scale = tuple(max(s, 0.2) / 100.0 for s in box["local_size"])
    return ob


def setup_world():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.render.resolution_percentage = 100
    scene.eevee.taa_render_samples = 24
    scene.view_settings.exposure = 0.0
    # 헤드라이트가 가까운 흰 벽을 날리지 않도록 하이라이트를 눌러 주는 변환.
    transforms = [i.identifier for i in bpy.types.ColorManagedViewSettings.bl_rna.properties["view_transform"].enum_items]
    for candidate in ("AgX", "Filmic"):
        if candidate in transforms:
            scene.view_settings.view_transform = candidate
            break
    world = bpy.data.worlds.new("__layout_world")
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    # 금속(스테인리스 상판·라이너)은 비칠 하늘이 있어야 검게 죽지 않는다.
    bg.inputs[0].default_value = (0.55, 0.56, 0.60, 1.0)
    bg.inputs[1].default_value = 1.0
    scene.world = world


def render_view(name, out_dir):
    view = VIEWS[name]
    loc_cm, look_cm, fov = view[:3]
    z_cut = view[3] if len(view) > 3 else None
    scene = bpy.context.scene
    # 평면도: 천장 컷보다 위에 놓인 것은 숨긴다. 카메라가 천장을 뚫고 본다.
    for ob in bpy.data.objects:
        if ob.type != "MESH":
            continue
        ob.hide_render = z_cut is not None and ob.location.z * 100.0 > z_cut
    cam_data = bpy.data.cameras.new(f"__cam_{name}")
    cam_data.angle = math.radians(fov)
    cam_data.clip_start = 0.05
    cam_data.clip_end = 300.0
    cam = ig._link(bpy.data.objects.new(f"__cam_{name}", cam_data))
    cam.location = cm(loc_cm)
    direction = Vector(cm(look_cm)) - Vector(cm(loc_cm))
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    # 카메라에 붙은 헤드라이트. 실내는 벽과 천장에 막혀 하늘빛이 안 든다.
    light_data = bpy.data.lights.new(f"__head_{name}", "POINT")
    light_data.energy = 250.0
    light_data.shadow_soft_size = 0.5
    light = ig._link(bpy.data.objects.new(f"__head_{name}", light_data))
    light.location = cam.location + Vector((0.0, 0.0, 0.3))
    if z_cut is not None:
        # 평면도는 헤드라이트 대신 위에서 고르게 비춘다.
        light_data.type = "SUN"
        light_data.energy = 3.0
        light.rotation_euler = (0.0, 0.0, 0.0)
    scene.render.filepath = os.path.join(out_dir, f"{name}.png")
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(light, do_unlink=True)
    bpy.data.objects.remove(cam, do_unlink=True)
    ig.log(f"view {name} -> {scene.render.filepath}")


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(argv) < 2:
        raise SystemExit("usage: -- <layout.json> <out_dir> [view ...]")
    # Blender는 상대 경로를 .blend 자리(없으면 드라이브 루트) 기준으로 푼다.
    layout_path, out_dir = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    names = argv[2:] or list(VIEWS)
    os.makedirs(out_dir, exist_ok=True)
    ig.reset_scene()
    setup_world()
    with open(layout_path, "r", encoding="utf-8") as handle:
        boxes = json.load(handle)["boxes"]
    placed = 0
    for box in boxes:
        if place(box) is not None:
            placed += 1
    missing = sorted({n for n, me in _asset_cache.items() if me is None})
    ig.log(f"placed {placed} of {len(boxes)} boxes; assets={len([m for m in _asset_cache.values() if m])}"
           f" no-source={missing}")
    for name in names:
        render_view(name, out_dir)


main()
