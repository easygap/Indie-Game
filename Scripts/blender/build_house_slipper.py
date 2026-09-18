"""실물 사진과 여러 각도의 참조 시트를 보고 만든 면 실내화. 치수 단위는 m."""

import math
import os
import sys

import bpy
import bmesh

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig


def mesh_part(name, vertices, faces, material):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    ob = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(material)
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
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
    grain.inputs["Scale"].default_value = 2200.0
    grain.inputs["Detail"].default_value = 2.0
    links.new(coords.outputs["Object"], grain.inputs["Vector"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    bump.inputs["Distance"].default_value = 0.00035
    links.new(grain.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    if name == "WarmGrayTerry":
        scale = nodes.new("ShaderNodeVectorMath")
        scale.operation = "SCALE"
        scale.inputs["Scale"].default_value = 12.5
        links.new(coords.outputs["Object"], scale.inputs[0])
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(os.path.abspath("Content/SourceArt/AI/CottonTerry_20260917.png"))
        tex.projection = "BOX"
        tex.projection_blend = .18
        links.new(scale.outputs["Vector"], tex.inputs["Vector"])
        tint = nodes.new("ShaderNodeMixRGB")
        tint.blend_type = "MULTIPLY"
        tint.inputs[0].default_value = 1
        tint.inputs[2].default_value = (*[c/.56 for c in color], 1)
        links.new(tex.outputs["Color"], tint.inputs[1])
        links.new(tint.outputs[0], bsdf.inputs["Base Color"])
        links.new(tex.outputs["Color"], bump.inputs["Height"])
        bump.inputs["Strength"].default_value = .24
        bump.inputs["Distance"].default_value = .0006
    mat["ig_kind"] = "baked"
    return mat


def outline(angle):
    # 실제 파일 실내화의 앞뒤는 타원 끝처럼 뾰족하지 않다.
    power = 2 / 2.8
    return (0.05 * math.copysign(abs(math.sin(angle)) ** power, math.sin(angle)),
            0.135 * math.copysign(abs(math.cos(angle)) ** power, math.cos(angle)))


def main():
    ig.reset_scene()
    fabric = cloth("WarmGrayTerry", (0.39, 0.375, 0.35))
    binding = cloth("CottonBinding", (0.46, 0.445, 0.415))
    rubber = ig.mat_rubber("SoftSole", (0.105, 0.11, 0.108))
    segments = 40
    verts = []
    for scale, z in ((0.97, 0.0), (1.0, 0.0035), (0.99, 0.007)):
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

    # 얇은 밑창 위의 패딩을 따로 만든다. 뒷꿈치 중앙도 평평한 검정 판이 아니다.
    foot_verts, foot_faces = [(0, 0, .012)], []
    for scale, z in ((.28,.012),(.62,.0115),(.85,.010),(.975,.0078)):
        for i in range(segments):
            x,y = outline(i * math.tau / segments)
            foot_verts.append((x*scale,y*scale,z))
    for i in range(segments):
        foot_faces.append((0,1+i,1+(i+1)%segments))
    for row in range(3):
        for i in range(segments):
            a=1+row*segments+i; b=1+row*segments+(i+1)%segments
            foot_faces.append((a,b,b+segments,a+segments))
    footbed = mesh_part("padded_terry_footbed",foot_verts,foot_faces,fabric)

    # 발등 덮개는 앞뒤가 열린 얇은 아치다. 사각 블록으로 내부를 채우지 않는다.
    rows, arc = 8, 24
    verts, faces = [], []
    for row in range(rows + 1):
        v = row / rows
        y = -0.022 + v * 0.117
        width = .048 * (1.0-(abs(y)/.135)**2.8)**(1/2.8)
        for col in range(arc + 1):
            a = col * math.pi / arc
            arch = math.sin(a)
            verts.append((width * math.cos(a), y-.012*arch,
                          .009 + arch*(.056-.027*v) + .0006*math.sin(v*math.pi*4)*arch))
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

    parts = [sole, footbed, upper]
    perimeter = [(*outline(i * math.tau / segments), 0.0075) for i in range(segments)]
    perimeter.append(perimeter[0])
    parts.append(ig.pipe("footbed_binding", perimeter, 0.0012, resolution=6, material=binding))
    for row in (0, rows):
        edge = verts[row * (arc + 1):(row + 1) * (arc + 1)]
        parts.append(ig.pipe(f"upper_binding_{row}", edge, 0.0012, resolution=6, material=binding))

    out = ig.out_root_from_argv()
    ig.build_asset("SM_HouseSlipper", "prop", parts, out, collision_parts=[[sole]],
                   texture_size=1024, preview_yaw=145,
                   notes=("MUJI 파일 실내화 실물 사진과 CottonSlipperStudy_20260917.png 참고. "
                          "길이 27 cm, 폭 10 cm. 앞코 +Y, 바닥 중심 원점. 넓고 둥근 앞뒤 끝, "
                          "높이가 다른 두 개구부, 패딩 깔창, 짧은 파일 원단과 얇은 고무 바닥. "
                          "밑창만 볼록 충돌로 사용해 빈 발등의 부피가 발에 걸리지 않게 한다. 질량 200 g."))
    folder=os.path.join(out,"SM_HouseSlipper")
    ig.preview_material_from_bakes(bpy.data.objects["SM_HouseSlipper"],
        {k:os.path.join(folder,"SM_HouseSlipper_"+k+".png") for k in ("D","N","ORM")})
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder,"SM_HouseSlipper.blend"))


if __name__ == "__main__":
    main()
