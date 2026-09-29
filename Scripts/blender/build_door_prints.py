"""4층 현관문에 붙은 스티커와 전단지. 문마다 메시 하나, 구운 텍스처 한 장.

인쇄 원본은 전부 gpt-image로 만든 실물 인쇄물 사진이다(SourceArt/AI/Door*_20260929).
문짝 하나에 붙은 것을 한 메시로 묶어 그리기 호출을 문마다 하나로 줄인다.
원점은 문짝 앞면(Y 0) 가운데 바닥이다. 앞 -Y, 치수는 m.

- 401호: 가스 점검 스티커, 전단지 부착 금지, 도어락 옆 열쇠 스티커.
- 402호: 봄에 이사 나간 뒤로 아무도 떼지 않은 전단지 두 장과 스티커 둘.
  전단지는 위만 테이프로 붙어 있어 아래쪽이 몇 mm 들떠 있다.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmesh
import bpy
import ig_blender_lib as ig

AI = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../Content/SourceArt/AI')


def art(name):
    return os.path.join(AI, f'{name}_20260929.png')


def backed(ob, back):
    """앞면(-Y)만 인쇄하고 나머지 면은 종이 뒷면 재질로 둔다."""
    ob.data.materials.append(back)
    for face in ob.data.polygons:
        if face.normal.y > -.5:
            face.material_index = 1
    return ob


def sticker(name, image, back, size, x, z, tilt_deg=0.0, gap=.0002, thickness=.0003):
    ink = ig.mat_image_uv(name, art(image), roughness=.42)
    ob = ig.image_quad(name, size, (x, -(gap + thickness / 2), z), ink,
                       rotation=(0.0, math.radians(tilt_deg), 0.0), thickness=thickness)
    return backed(ob, back)


def disc_sticker(name, image, back, diameter, x, z, tilt_deg=0.0, gap=.0002, thickness=.0003):
    """원형 스티커. 정사각 원본의 안쪽 원만 UV로 쓴다.

    원기둥은 로컬 Z가 축이다. X축으로 90도 돌리면 로컬 +Z 뚜껑이 -Y(앞)를 보고
    로컬 +Y가 위(+Z)가 된다. 그래서 UV와 앞뒤 판정은 로컬 좌표로 한다."""
    ink = ig.mat_image_uv(name, art(image), roughness=.40)
    ob = ig.cylinder(name, diameter / 2, thickness, (x, -(gap + thickness / 2), z),
                     rotation=(math.pi / 2, math.radians(tilt_deg), 0.0), segments=40, material=ink)
    me = ob.data
    layer = me.uv_layers.new(name='ImageUV')
    for poly in me.polygons:
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            layer.data[li].uv = (co.x / diameter + .5, co.y / diameter + .5)
    me.materials.append(back)
    for face in me.polygons:
        if face.normal.z < .5:
            face.material_index = 1
    return ob


def curled_flyer(name, image, back, size, x, z, tilt_deg, lift, layer_gap):
    """위 가장자리만 테이프로 붙은 종이. 아래로 갈수록 문에서 들뜬다."""
    ink = ig.mat_image_uv(name, art(image), roughness=.62)
    width, height = size
    columns, rows = 8, 12
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new('ImageUV')
    tilt = math.radians(tilt_deg)
    grid = []
    for row in range(rows + 1):
        line = []
        for col in range(columns + 1):
            u, v = col / columns, row / rows
            lx, lz = (u - .5) * width, (v - .5) * height
            # 들뜸은 아래쪽 모서리로 갈수록 제곱으로 커지고, 한쪽 모서리가 조금 더 뜬다.
            fall = (1.0 - v) ** 2
            y = -(layer_gap + lift * fall * (.8 + .4 * u))
            rx = lx * math.cos(tilt) - lz * math.sin(tilt)
            rz = lx * math.sin(tilt) + lz * math.cos(tilt)
            line.append((bm.verts.new((x + rx, y, z + rz)), (u, v)))
        grid.append(line)
    for row in range(rows):
        for col in range(columns):
            corners = (grid[row][col], grid[row][col + 1], grid[row + 1][col + 1], grid[row + 1][col])
            face = bm.faces.new([vert for vert, _ in corners])
            for loop, (_, coord) in zip(face.loops, corners):
                loop[uv].uv = coord
    bm.normal_update()
    for face in bm.faces:
        if face.normal.y > 0:
            face.normal_flip()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(ink)
    # 종이 두께 0.15 mm. 뒷면과 마구리는 종이 뒷면 색이다.
    solid = ob.modifiers.new('paper', 'SOLIDIFY')
    solid.thickness = .00015
    solid.offset = 1.0
    ig.apply_modifiers(ob)
    return backed(ob, back)


def tape(name, x, z, tilt_deg, y, material):
    return ig.box(name, (.042, .00006, .016), (x, y, z), rotation=(0.0, math.radians(tilt_deg), 0.0),
                  material=material)


def build_401(out):
    ig.reset_scene()
    back = ig.mat_plastic('StickerBack', (.80, .80, .77), roughness=.8, bump=0)
    parts = [
        disc_sticker('GasSticker', 'DoorGasSticker', back, .07, -.22, 1.50),
        sticker('NoFlyer', 'DoorNoFlyer', back, (.12, .031), .0, 1.71),
        sticker('KeySticker', 'DoorKeySticker', back, (.08, .04), .22, 1.41, tilt_deg=-2.0),
    ]
    ig.build_asset('SM_DoorPrints401', 'prop', parts, out, collision_parts=[], texture_size=512,
                   mirror_print_uv=True, origin='door-face',
                   notes='401호 문짝 앞면 가운데 바닥이 원점, 앞 -Y. 가스 점검(지름 7cm)·전단지 부착 금지(12×3.1cm)·'
                         '열쇠 출장(8×4cm) 스티커. 원본은 SourceArt/AI/Door*_20260929(gpt-image). 충돌·그림자 없음.')


def build_402(out):
    ig.reset_scene()
    back = ig.mat_plastic('PaperBack', (.80, .79, .75), roughness=.85, bump=0)
    tape_material = ig.mat_plastic('Tape', (.74, .72, .64), roughness=.3, bump=0)
    parts = [
        disc_sticker('GasSticker', 'DoorGasSticker', back, .07, -.22, 1.50, tilt_deg=3.0),
        sticker('KeySticker', 'DoorKeySticker', back, (.08, .04), .22, 1.41, tilt_deg=1.5),
        curled_flyer('ChineseFlyer', 'DoorFlyerChinese', back, (.148, .21), -.14, .84, 4.0, .006, .0003),
        curled_flyer('RealtyFlyer', 'DoorFlyerRealty', back, (.148, .21), -.03, .70, -5.0, .005, .0009),
        tape('ChineseTape', -.14 - .105 * math.sin(math.radians(4.0)), .84 + .098, 4.0, -.0005, tape_material),
        tape('RealtyTape', -.03 + .105 * math.sin(math.radians(5.0)), .70 + .098, -5.0, -.0011, tape_material),
    ]
    ig.build_asset('SM_DoorPrints402', 'prop', parts, out, collision_parts=[], texture_size=1024,
                   mirror_print_uv=True, origin='door-face',
                   notes='402호 문짝 앞면 가운데 바닥이 원점, 앞 -Y. 봄에 비어 버린 집이라 떼지 않은 A5 전단 두 장'
                         '(홍반점·무영부동산, 아래쪽 5~6mm 들뜸, 위쪽 테이프)과 가스·열쇠 스티커. '
                         '원본은 SourceArt/AI/Door*_20260929(gpt-image). 충돌·그림자 없음. 402호 메모(Z 1.18)와 겹치지 않는다.')


if __name__ == '__main__':
    out = ig.out_root_from_argv()
    build_401(out)
    build_402(out)
