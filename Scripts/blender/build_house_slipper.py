"""실물 사진과 여러 각도의 참조 시트를 보고 만든 면 실내화. 치수 단위는 m."""

import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig


def mesh_part(name, vertices, faces, material):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    ob = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(material)
    for poly in mesh.polygons:
        poly.use_smooth = True
    return ob


def cloth(name, color):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.88
    bsdf.inputs["Sheen Weight"].default_value = 0.25
    coords = nodes.new("ShaderNodeTexCoord")
    grain = nodes.new("ShaderNodeTexNoise")
    grain.inputs["Scale"].default_value = 950.0
    grain.inputs["Detail"].default_value = 1.0
    links.new(coords.outputs["Object"], grain.inputs["Vector"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.22
    bump.inputs["Distance"].default_value = 0.00025
    links.new(grain.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    mat["ig_kind"] = "baked"
    return mat


def outline(angle):
    return (0.049 * math.sin(angle) * (1.0 + 0.07 * math.cos(angle)),
            0.135 * math.cos(angle))


def main():
    ig.reset_scene()
    fabric = cloth("CharcoalCotton", (0.115, 0.111, 0.106))
    binding = cloth("CottonBinding", (0.077, 0.074, 0.07))
    rubber = ig.mat_rubber("SoftSole", (0.025, 0.025, 0.024))
    segments = 48
    verts = []
    for scale, z in ((0.95, 0.0), (1.0, 0.006), (0.97, 0.013)):
        for i in range(segments):
            x, y = outline(i * math.tau / segments)
            verts.append((x * scale, y * scale, z))
    faces = []
    for ring in range(2):
        for i in range(segments):
            j = (i + 1) % segments
            faces.append((ring * segments + i, (ring + 1) * segments + i,
                          (ring + 1) * segments + j, ring * segments + j))
    faces.extend((tuple(range(segments)), tuple(reversed(range(2 * segments, 3 * segments)))))
    sole = mesh_part("padded_sole", verts, faces, rubber)
    sole.data.materials.append(fabric)
    sole.data.polygons[-1].material_index = 1

    # 발등 덮개는 앞뒤가 열린 얇은 아치다. 사각 블록으로 내부를 채우지 않는다.
    rows, arc = 8, 24
    verts, faces = [], []
    for row in range(rows + 1):
        v = row / rows
        y = -0.006 + v * 0.111
        width = 0.046 * math.sqrt(max(0.35, 1.0 - (y / 0.152) ** 2))
        for col in range(arc + 1):
            a = col * math.pi / arc
            verts.append((width * math.cos(a), y,
                          0.014 + math.sin(a) * (0.047 - 0.006 * v)))
    for row in range(rows):
        for col in range(arc):
            k = row * (arc + 1) + col
            faces.append((k, k + arc + 1, k + arc + 2, k + 1))
    upper = mesh_part("open_cotton_upper", verts, faces, fabric)
    ig.set_active(upper, solo=True)
    solid = upper.modifiers.new("cloth_thickness", "SOLIDIFY")
    solid.thickness = 0.003
    solid.offset = -1.0
    bpy.ops.object.modifier_apply(modifier=solid.name)

    parts = [sole, upper]
    perimeter = [(*outline(i * math.tau / segments), 0.014) for i in range(segments)]
    perimeter.append(perimeter[0])
    parts.append(ig.pipe("footbed_binding", perimeter, 0.0011, resolution=6, material=binding))
    for row in (0, rows):
        edge = verts[row * (arc + 1):(row + 1) * (arc + 1)]
        parts.append(ig.pipe(f"upper_binding_{row}", edge, 0.0012, resolution=6, material=binding))

    ig.build_asset("SM_HouseSlipper", "prop", parts, ig.out_root_from_argv(),
                   texture_size=1024, preview_yaw=145,
                   notes=("면 실내화 한 짝. 길이 27 cm, 폭 10 cm, 높이 6.1 cm. 앞코 +Y, 바닥 중심 원점. "
                          "무인양품 면 파일 실내화 실물 사진과 CottonSlippersReference_20260915.png의 "
                          "정면·측면·윗면을 참고했다. 발등 덮개 안은 비어 있고 테두리는 별도 봉제선이다. "
                          "소품 하나당 충돌 볼록체 하나, 질량 200 g은 게임에서 지정한다."))


if __name__ == "__main__":
    main()
