"""편의점 상품과 작은 집기. 편의점을 한국 편의점답게 채우는 것들.

앞면 그림은 Scripts/create_store_product_art.py가 그린 가상 브랜드 아틀라스
(Content/SourceArt/Labels/Store)다. 좌표계는 다른 빌더와 같다(원점 바닥 중심,
앞면 -Y). 상품은 씬 코드가 선반 위 좌표에 놓는다.

    SM_CupNoodle       컵라면 몸통(발포 컵). raw_uv — 재질은 씬의 CupNoodleMaterial
    SM_CupSleeve       라벨 슬리브. raw_uv, U 한 바퀴·V는 위가 0(UE). M_LabelRamyeon이 읽는다
    SM_CupLid          종이 뚜껑과 손잡이 탭. raw_uv 평면 UV, M_StainlessUV
    SM_SnackBoxA~D     과자 상자 앞면 넷
    SM_TriangleKimbapA~D 삼각김밥 넷
    SM_RiceBowlPack    즉석밥 3개 묶음
    SM_TobaccoCabinet  계산대 뒤 담배 진열장. 담뱃갑 136개가 들어 있고 위에 청소년 판매 금지 띠
    SM_WindowBar       창가 취식대 (34 cm 깊이 목재 상판, 철제 브래킷)
    SM_HotWaterDispenser 라면 온수기
    SM_TrashBin        일반·재활용 2구 쓰레기통

    blender -b --factory-startup --python Scripts/blender/build_store_products.py -- <out_dir> [cup|sleeve|lid|boxes|kimbap|rice|tobacco|bar|water|bin ...]
"""

import math
import os
import sys

import bmesh
import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
ART = os.path.join(REPO, "Content", "SourceArt", "Labels", "Store")

# 컵라면 치수(m). 작은 컵: 위 지름 11, 아래 7.5, 높이 9.6.
CUP_R0, CUP_R1, CUP_H = 0.0375, 0.055, 0.096
SLEEVE_Z0, SLEEVE_Z1 = 0.016, 0.090      # 씬이 슬리브를 +1.6 cm에 놓으므로 로컬 0..7.4


def cup_radius(z):
    return CUP_R0 + (CUP_R1 - CUP_R0) * (z / CUP_H)


def wrapped_strip(name, profile, segments, material, uv_layer="UVMap"):
    """(r, z) 프로파일을 회전한 띠. 이음매를 터서 U가 0..1로 한 바퀴, V는 아래 0 위 1
    (Blender 기준. FBX가 V를 뒤집어 UE에서는 아래가 1, 위가 0이 된다)."""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new(uv_layer)
    rings, column = [], {}
    for r, z in profile:
        ring = []
        for i in range(segments + 1):
            a = math.tau * i / segments
            v = bm.verts.new((r * math.cos(a), r * math.sin(a), z))
            column[v] = i
            ring.append(v)
        rings.append(ring)
    z0, z1 = profile[0][1], profile[-1][1]
    for k in range(len(profile) - 1):
        for i in range(segments):
            face = bm.faces.new((rings[k][i], rings[k][i + 1], rings[k + 1][i + 1], rings[k + 1][i]))
            for loop in face.loops:
                co = loop.vert.co
                loop[uv].uv = (column[loop.vert] / segments, (co.z - z0) / (z1 - z0) if z1 != z0 else 0.0)
            # 바깥을 보게. 열린 띠라 recalc가 방향을 못 정한다.
            face.normal_update()
            radial = face.calc_center_median()
            radial.z = 0.0
            if face.normal.dot(radial) < 0.0:
                face.normal_flip()
    me = ig._mesh_from_bm(name, bm)
    ob = ig._link(bpy.data.objects.new(name, me))
    if material is not None:
        me.materials.append(material)
    return ob


def planar_uv(ob, scale, uv_layer="UVMap"):
    """윗면 기준 평면 UV. 뚜껑처럼 위에서 보는 것에 쓴다."""
    me = ob.data
    layer = me.uv_layers.get(uv_layer) or me.uv_layers.new(name=uv_layer)
    for poly in me.polygons:
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            layer.data[li].uv = (co.x / scale + 0.5, co.y / scale + 0.5)


def mats():
    return {
        "foam": ig.mat_plastic("Foam", (0.93, 0.92, 0.88), roughness=0.75, bump=0.02, grain_scale=600.0),
        "paper": ig.mat_plastic("Paper", (0.95, 0.94, 0.90), roughness=0.7, bump=0.01),
        "white": ig.mat_plastic("WhitePlastic", (0.9, 0.9, 0.88), roughness=0.45, bump=0.01),
        "dark": ig.mat_plastic("DarkPlastic", (0.02, 0.02, 0.022), roughness=0.5),
        "grey": ig.mat_painted_steel("GreySteel", (0.32, 0.33, 0.34), roughness=0.5, wear=0.2),
        "steel": ig.mat_metal("Stainless", (0.78, 0.78, 0.77), roughness=0.34, streak=0.1),
        "chrome": ig.mat_metal("Chrome", (0.86, 0.86, 0.86), roughness=0.22, streak=0.05, anisotropic=False),
        "wood": ig.mat_plastic("BarWood", (0.42, 0.28, 0.16), roughness=0.5, bump=0.02, grain_scale=80.0),
        "glass": ig.mat_glass("Glass"),
        "led": ig.mat_emissive("LedStrip", (1.0, 0.96, 0.9), strength=4.0),
        "red": ig.mat_emissive("RedLamp", (1.0, 0.15, 0.05), strength=3.0),
        "packs": ig.mat_image_uv("CigarettePacks", os.path.join(ART, "CigarettePacks.png"), roughness=0.35),
        "boxes": ig.mat_image_uv("SnackBoxes", os.path.join(ART, "SnackBoxes.png"), roughness=0.5),
        "kimbap": ig.mat_image_uv("TriangleKimbap", os.path.join(ART, "TriangleKimbap.png"), roughness=0.25),
        "rice": ig.mat_image_uv("RiceBowls", os.path.join(ART, "RiceBowls.png"), roughness=0.3),
        "sign": ig.mat_image_uv("MinorSign", os.path.join(ART, "MinorSign.png"), roughness=0.5),
    }


# --------------------------------------------------------------------------
# 컵라면 세 조각 (raw_uv)
# --------------------------------------------------------------------------

def build_cup(out_root):
    ig.reset_scene()
    m = mats()
    # 몸통: 아래에서 위로 벌어지는 발포 컵, 위에 말린 테. 바닥은 살짝 들려 있다.
    profile = [(CUP_R0 - 0.004, 0.0), (CUP_R0, 0.004)]
    for z in (0.02, 0.04, 0.06, 0.08):
        profile.append((cup_radius(z), z))
    profile += [(CUP_R1, CUP_H - 0.004), (CUP_R1 + 0.003, CUP_H - 0.001), (CUP_R1 + 0.002, CUP_H),
                (CUP_R1 - 0.003, CUP_H - 0.001)]
    body = wrapped_strip("cup", profile, 48, m["foam"])
    bottom = ig.cylinder("bottom", CUP_R0 - 0.004, 0.003, location=(0.0, 0.0, 0.0015), segments=48, material=m["foam"])
    planar_uv(bottom, CUP_R1 * 2.4)
    return ig.build_asset(
        "SM_CupNoodle", "prop", [body, bottom], out_root, collision_parts=[[body]],
        notes="컵라면 발포 컵. 위 지름 11, 아래 7.5, 높이 9.6. 원점 바닥 중심. 재질은 씬의 CupNoodleMaterial(UV 원통).",
        raw_uv=True, preview=True)


def build_sleeve(out_root):
    ig.reset_scene()
    m = mats()
    # 컵 벽에서 1 mm 띄운 띠. 씬이 컵 바닥 +1.6 cm에 놓으므로 로컬 Z 0이 컵 Z 1.6이다.
    profile = []
    for z in (SLEEVE_Z0, 0.03, 0.05, 0.07, SLEEVE_Z1):
        profile.append((cup_radius(z) + 0.001, z - SLEEVE_Z0))
    sleeve = wrapped_strip("sleeve", profile, 64, m["paper"])
    return ig.build_asset(
        "SM_CupSleeve", "prop", [sleeve], out_root, collision_parts=[],
        notes="컵라면 라벨 슬리브. 컵 Z 1.6..9.0을 감싼다(로컬 0..7.4). U 한 바퀴, V는 UE에서 위가 0. M_LabelRamyeon이 읽는다.",
        raw_uv=True, preview=False)


def build_lid(out_root):
    ig.reset_scene()
    m = mats()
    lid = ig.cylinder("lid", CUP_R1 + 0.004, 0.0016, location=(0.0, 0.0, CUP_H + 0.0008), segments=48, material=m["paper"])
    tab = ig.box("tab", (0.024, 0.014, 0.0012), location=(0.0, -(CUP_R1 + 0.010), CUP_H + 0.0006), material=m["paper"])
    planar_uv(lid, (CUP_R1 + 0.014) * 2.0)
    planar_uv(tab, (CUP_R1 + 0.014) * 2.0)
    return ig.build_asset(
        "SM_CupLid", "prop", [lid, tab], out_root, collision_parts=[],
        notes="컵라면 종이 뚜껑과 탭. 컵 원점과 같아 컵 자리에 그대로 놓는다(Z 9.6). 평면 UV, 씬의 M_StainlessUV.",
        raw_uv=True, preview=False)


# --------------------------------------------------------------------------
# 과자 상자·삼각김밥·즉석밥
# --------------------------------------------------------------------------

BOX_COLORS = [(0.55, 0.10, 0.14), (0.10, 0.25, 0.55), (0.92, 0.75, 0.15), (0.92, 0.52, 0.15)]


def build_snack_boxes(out_root):
    manifests = []
    for index in range(4):
        ig.reset_scene()
        m = mats()
        w, d, h = 0.18, 0.06, 0.24
        body_mat = ig.mat_plastic(f"BoxCard{index}", BOX_COLORS[index], roughness=0.55, bump=0.01)
        body = ig.box("body", (w, d, h), origin="bottom", bevel=0.002, segments=1, material=body_mat)
        face = ig.image_quad("front", (w - 0.004, h - 0.004), (0.0, -d * 0.5 - 0.0005, h * 0.5), m["boxes"],
                             uv_rect=ig.atlas_rect(2, 2, index))
        manifests.append(ig.build_asset(
            f"SM_SnackBox{'ABCD'[index]}", "prop", [body, face], out_root, collision_parts=[[body]],
            notes=f"과자 상자 {index + 1}/4. 18 x 6 x 24, 원점 바닥 중심, 앞면 -Y.", texture_size=512))
    return manifests


def triangle_prism(name, side, thickness, material, uv_rect, uv_layer="ImageUV"):
    """정삼각 기둥. 앞뒤 삼각면은 아틀라스 칸에 내접하는 삼각형, 옆면은 칸의 어두운 구석."""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new(uv_layer)
    hgt = side * math.sqrt(3.0) * 0.5
    pts = [(-side * 0.5, 0.0), (side * 0.5, 0.0), (0.0, hgt)]
    front = [bm.verts.new((x, -thickness * 0.5, z)) for x, z in pts]
    back = [bm.verts.new((x, thickness * 0.5, z)) for x, z in pts]
    u0, v0, u1, v1 = uv_rect

    def cell(u, v):
        return (u0 + (u1 - u0) * u, v0 + (v1 - v0) * v)
    tri_uv = [cell(0.08, 0.05), cell(0.92, 0.05), cell(0.5, 0.92)]
    f = bm.faces.new(front)
    for loop, t in zip(f.loops, tri_uv):
        loop[uv].uv = t
    b = bm.faces.new(reversed(back))
    for loop, t in zip(b.loops, reversed(tri_uv)):
        loop[uv].uv = t
    for i in range(3):
        j = (i + 1) % 3
        side_face = bm.faces.new((front[j], front[i], back[i], back[j]))
        for loop in side_face.loops:
            loop[uv].uv = cell(0.02, 0.5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = ig._mesh_from_bm(name, bm)
    ob = ig._link(bpy.data.objects.new(name, me))
    me.materials.append(material)
    return ob


def build_triangle_kimbap(out_root):
    manifests = []
    for index in range(4):
        ig.reset_scene()
        m = mats()
        prism = triangle_prism("kimbap", 0.075, 0.032, m["kimbap"], ig.atlas_rect(2, 2, index))
        manifests.append(ig.build_asset(
            f"SM_TriangleKimbap{'ABCD'[index]}", "prop", [prism], out_root, collision_parts=[[prism]],
            notes=f"삼각김밥 {index + 1}/4. 한 변 7.5, 두께 3.2, 원점 바닥 중심, 앞면 -Y.", texture_size=512))
    return manifests


def build_rice_pack(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    # 즉석밥 셋을 옆으로 묶은 팩. 그릇은 위가 넓은 트레이, 뚜껑 인쇄.
    for index, x in enumerate((-0.047, 0.0, 0.047)):
        bowl = ig.cylinder(f"bowl{index}", 0.045, 0.035, location=(x, 0.0, 0.0175), segments=28, radius_top=0.045,
                           material=m["white"])
        parts.append(bowl)
        lid = ig.cylinder(f"lidbody{index}", 0.046, 0.002, location=(x, 0.0, 0.036), segments=28, material=m["white"])
        parts.append(lid)
        quad = ig.image_quad(f"lidprint{index}", (0.086, 0.086), (x, 0.0, 0.0372), m["rice"],
                             rotation=(math.pi * 0.5, 0.0, 0.0), uv_rect=ig.atlas_rect(2, 1, index % 2))
        parts.append(quad)
    wrap = ig.box("wrap", (0.14, 0.092, 0.04), location=(0.0, 0.0, 0.02), material=m["glass"])
    parts.append(wrap)
    return ig.build_asset(
        "SM_RiceBowlPack", "prop", parts, out_root, collision_parts=[[wrap]],
        notes="즉석밥 3개 묶음 14 x 9 x 4. 원점 바닥 중심. 비닐 포장은 Glass 슬롯.", texture_size=512)


# --------------------------------------------------------------------------
# 담배 진열장
# --------------------------------------------------------------------------

def build_tobacco_cabinet(out_root):
    from build_retail_refresh import tobacco
    tobacco(out_root)


def build_window_bar(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    length, depth, height = 0.72, 0.34, 1.05
    top = ig.box("top", (depth, length, 0.04), location=(0.0, 0.0, height - 0.02), bevel=0.004, segments=2,
                 material=m["wood"])
    parts.append(top)
    # 벽(유리 멀리언) 쪽 앵글 브래킷 둘과 바닥까지 내려오는 강관 다리 둘.
    for y in (-length * 0.5 + 0.08, length * 0.5 - 0.08):
        parts.append(ig.box("bracket", (depth - 0.06, 0.03, 0.03), location=(0.02, y, height - 0.055),
                            material=m["grey"]))
        parts.append(ig.cylinder("leg", 0.014, height - 0.04, location=(depth * 0.5 - 0.05, y, (height - 0.04) * 0.5),
                                 segments=16, material=m["grey"]))
        parts.append(ig.cylinder("foot", 0.03, 0.006, location=(depth * 0.5 - 0.05, y, 0.003), segments=16,
                                 material=m["dark"]))
    return ig.build_asset(
        "SM_WindowBar", "prop", parts, out_root, collision_parts=[[top]],
        notes="창가 취식대 34 x 72 x 105. 원점 바닥 중심(씬 (2433, -634, 6)), 유리 쪽이 -X. 상판 윗면 씬 Z 111.",
        texture_size=1024, preview_yaw=60.0)


def build_water_dispenser(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    w, d, h = 0.30, 0.36, 0.42
    body = ig.box("body", (w, d, h), origin="bottom", bevel=0.006, segments=2, material=m["steel"])
    parts.append(body)
    # 앞면(-Y) 아래 물받이 홈, 꼭지, 표시등, 위 뚜껑.
    tray = ig.box("tray_cut", (0.22, 0.10, 0.12), location=(0.0, -d * 0.5 + 0.05, 0.10))
    ig.boolean(body, tray, "DIFFERENCE")
    parts.append(ig.box("grate", (0.20, 0.09, 0.006), location=(0.0, -d * 0.5 + 0.05, 0.043), material=m["grey"]))
    parts.append(ig.cylinder("tap", 0.008, 0.05, location=(0.0, -d * 0.5 + 0.03, 0.22), segments=16,
                             rotation=(math.pi * 0.5, 0.0, 0.0), material=m["chrome"]))
    parts.append(ig.box("lever", (0.06, 0.02, 0.014), location=(0.0, -d * 0.5 - 0.005, 0.245), bevel=0.003, segments=1,
                        material=m["dark"]))
    parts.append(ig.box("panel", (0.10, 0.004, 0.05), location=(0.0, -d * 0.5 - 0.002, 0.33), material=m["dark"]))
    parts.append(ig.cylinder("lamp", 0.005, 0.002, location=(-0.03, -d * 0.5 - 0.005, 0.33), segments=10,
                             rotation=(math.pi * 0.5, 0.0, 0.0), material=m["red"]))
    parts.append(ig.box("lid", (w - 0.02, d - 0.02, 0.02), location=(0.0, 0.0, h + 0.01), bevel=0.004, segments=1,
                        material=m["steel"]))
    parts.append(ig.box("label", (0.10, 0.002, 0.03), location=(0.0, -d * 0.5 - 0.001, 0.375), material=m["white"]))
    return ig.build_asset(
        "SM_HotWaterDispenser", "prop", parts, out_root, collision_parts=[[body]],
        notes="라면 온수기 30 x 36 x 44. 원점 바닥 중심, 꼭지가 -Y.", texture_size=1024)


def build_trash_bin(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    w, d, h = 0.60, 0.36, 0.82
    body = ig.box("body", (w, d, h), origin="bottom", bevel=0.006, segments=2, material=m["grey"])
    parts.append(body)
    for index, x in enumerate((-0.15, 0.15)):
        hole = ig.box("hole", (0.18, 0.05, 0.10), location=(x, -d * 0.5 + 0.02, h - 0.10))
        ig.boolean(body, hole, "DIFFERENCE")
        plate = ig.box("plate", (0.22, 0.004, 0.06), location=(x, -d * 0.5 - 0.002, h - 0.22), material=m["white"])
        parts.append(plate)
    parts.append(ig.box("top_rim", (w, d, 0.02), location=(0.0, 0.0, h + 0.01), bevel=0.004, segments=1,
                        material=m["dark"]))
    return ig.build_asset(
        "SM_TrashBin", "prop", parts, out_root, collision_parts=[[body]],
        notes="2구 쓰레기통 60 x 36 x 84(일반·재활용). 원점 바닥 중심, 투입구 -Y.", texture_size=1024)


BUILDERS = {
    "cup": build_cup,
    "sleeve": build_sleeve,
    "lid": build_lid,
    "boxes": build_snack_boxes,
    "kimbap": build_triangle_kimbap,
    "rice": build_rice_pack,
    "tobacco": build_tobacco_cabinet,
    "bar": build_window_bar,
    "water": build_water_dispenser,
    "bin": build_trash_bin,
}


def main():
    out_root = ig.out_root_from_argv()
    only = [a for a in sys.argv[sys.argv.index("--") + 2:]] if "--" in sys.argv else []
    for key, builder in BUILDERS.items():
        if not only or key in only:
            builder(out_root)


if __name__ == "__main__":
    main()
