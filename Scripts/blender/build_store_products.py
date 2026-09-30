"""편의점 상품과 작은 집기. 편의점을 한국 편의점답게 채우는 것들.

좌표계는 다른 빌더와 같다(원점 바닥 중심, 앞면 -Y). 삼각김밥과 담배 진열장은
build_detail_props.py와 build_retail_refresh.py가 만들고 여기서는 불러 주기만
한다. wrapped_strip은 build_retail_refresh.py도 가져다 쓴다.

    SM_TriangleKimbapA~D 삼각김밥 넷
    SM_TobaccoCabinet  계산대 뒤 담배 진열장
    SM_WindowBar       창가 취식대 (34 cm 깊이 목재 상판, 철제 브래킷)
    SM_HotWaterDispenser 라면 온수기
    SM_TrashBin        일반·재활용 2구 쓰레기통

    blender -b --factory-startup --python Scripts/blender/build_store_products.py -- <out_dir> [kimbap|tobacco|bar|water|bin ...]
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


def mats():
    return {
        "white": ig.mat_plastic("WhitePlastic", (0.9, 0.9, 0.88), roughness=0.45, bump=0.01),
        "dark": ig.mat_plastic("DarkPlastic", (0.02, 0.02, 0.022), roughness=0.5),
        "grey": ig.mat_painted_steel("GreySteel", (0.32, 0.33, 0.34), roughness=0.5, wear=0.2),
        "steel": ig.mat_metal("Stainless", (0.78, 0.78, 0.77), roughness=0.34, streak=0.1),
        "chrome": ig.mat_metal("Chrome", (0.86, 0.86, 0.86), roughness=0.22, streak=0.05, anisotropic=False),
        "wood": ig.mat_plastic("BarWood", (0.42, 0.28, 0.16), roughness=0.5, bump=0.02, grain_scale=80.0),
        "red": ig.mat_emissive("RedLamp", (1.0, 0.15, 0.05), strength=3.0),
    }


# --------------------------------------------------------------------------
# 삼각김밥
# --------------------------------------------------------------------------

def build_triangle_kimbap(out_root):
    # 예전 단독 호출도 현재 포장 빌더를 사용한다.
    from build_detail_props import kimbap
    return [kimbap(index, out_root) for index in range(4)]


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
    # NS-3000B 제품 사진의 돌출 코크·하향 출수구·외부 물받이 구조를 따른다.
    # 창가 취식대 안에 받침이 들어오도록 소형 본체의 깊이는 22cm로 잡는다.
    w, d, h = 0.24, 0.22, 0.50
    body = ig.box("body", (w, d, h), origin="bottom", bevel=0.006, segments=2, material=m["steel"])
    parts.append(body)
    parts.append(ig.box("전면패널",(.13,.004,.43),(0,-.112,.255),material=m["dark"]))
    parts.append(ig.box("물받이",(.22,.095,.017),(0,-.122,.012),bevel=.003,material=m["dark"]))
    for x in (-.10,.10):
        parts.append(ig.box("물받이테두리",(.006,.091,.018),(x,-.122,.024),material=m["steel"]))
    for x in range(13):
        parts.append(ig.box("물받이살",(.005,.083,.004),(-.09+x*.015,-.122,.028),material=m["steel"]))
    parts.append(ig.cylinder("코크연결관",.012,.042,(0,-.13,.255),segments=16,
                             rotation=(math.pi/2,0,0),material=m["chrome"]))
    parts.append(ig.cylinder("코크몸체",.017,.05,(0,-.149,.257),segments=20,material=m["dark"]))
    parts.append(ig.cylinder("아래로난출수구",.007,.018,(0,-.149,.224),segments=16,material=m["chrome"]))
    parts.append(ig.cylinder("출수구안쪽",.005,.001,(0,-.149,.2145),segments=16,material=m["dark"]))
    parts.append(ig.box("누름레버",(.025,.026,.045),(0,-.15,.306),rotation=(math.radians(-18),0,0),bevel=.003,material=m["dark"]))
    parts.append(ig.box("온도표시창",(.052,.003,.033),(0,-.116,.411),material=m["grey"]))
    # 한글 경고와 온도 숫자는 글꼴로 조판한 뒤 본체와 함께 굽는다.
    font=bpy.data.fonts.load('C:/Windows/Fonts/malgun.ttf')
    for label,z,size,material in (('98',.407,.021,m['red']),('온수',.367,.015,m['white']),('화상주의',.193,.009,m['white'])):
        curve=bpy.data.curves.new(label,'FONT');curve.body=label;curve.font=font
        curve.align_x='CENTER';curve.align_y='CENTER';curve.size=size
        ob=bpy.data.objects.new(label,curve);bpy.context.collection.objects.link(ob)
        ob.location=(0,-.118,z);ob.rotation_euler=(math.pi/2,0,0)
        curve.materials.append(material);ig.set_active(ob);bpy.ops.object.convert(target='MESH');parts.append(bpy.context.object)
    parts.append(ig.cylinder("lamp", 0.005, 0.002, location=(-0.03, -d * 0.5 - 0.005, 0.33), segments=10,
                             rotation=(math.pi * 0.5, 0.0, 0.0), material=m["red"]))
    parts.append(ig.box("lid", (w - 0.02, d - 0.02, 0.02), location=(0.0, 0.0, h + 0.01), bevel=0.004, segments=1,
                        material=m["steel"]))
    return ig.build_asset(
        "SM_HotWaterDispenser", "prop", parts, out_root, collision_parts=[[body]],
        notes="라면 온수기 24×27.95×52cm. NS-3000B 사진에서 코크와 물받이 구조를 확인했다. 본체 밖의 레버·하향 출수구·물받이, 꼭지는 -Y.", texture_size=1024,
        mirror_print_for_ue=True)


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
    "kimbap": build_triangle_kimbap,
    "tobacco": build_tobacco_cabinet,
    "bar": build_window_bar,
    "water": build_water_dispenser,
    "bin": build_trash_bin,
}


def main():
    out_root = ig.out_root_from_argv()
    only = [a for a in sys.argv[sys.argv.index("--") + 2:]] if "--" in sys.argv else []
    for key, builder in BUILDERS.items():
        # 전체 빌드의 삼각김밥은 detail_props가 맡는다.
        if (not only and key != 'kimbap') or key in only:
            builder(out_root)


if __name__ == "__main__":
    main()
