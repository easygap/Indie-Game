"""목의 겹친 껍질과 서로 다른 피부색을 한 표면으로 정리한다."""
import os
import math
import bmesh
import bpy
import numpy as np

import ig_blender_lib as ig


def connect_neck(body, head):
    """닫힌 두 메시의 교차면을 없앤다. 얼굴 위쪽의 정점과 UV는 유지한다."""
    for ob in (body, head):
        for p in ob.data.polygons:
            p.use_smooth = True
    modifier = body.modifiers.new('목 안쪽 겹침 제거', 'BOOLEAN')
    modifier.operation = 'UNION'
    modifier.solver = 'EXACT'
    modifier.use_self = True
    modifier.use_hole_tolerant = True
    modifier.object = head
    ig.apply_modifiers(body)
    bpy.data.objects.remove(head, do_unlink=True)
    bm = bmesh.new()
    bm.from_mesh(body.data)
    neck = [v for v in bm.verts if 1.335 < v.co.z < 1.382
            and abs(v.co.x) < .075 and v.co.y > -.105]
    for _ in range(3):
        bmesh.ops.smooth_vert(bm, verts=neck, factor=.35,
                              use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    # 불리언 교차점에 남은 내부 삼각형은 세 변이 모두 세 면을 공유한다.
    interior = [f for f in bm.faces if all(len(e.link_faces)>2 for e in f.edges)
                and 1.33 < f.calc_center_median().z < 1.385]
    if interior: bmesh.ops.delete(bm, geom=interior, context='FACES_ONLY')
    border = [e for e in bm.edges if e.is_boundary and all(
        1.33 < v.co.z < 1.385 and abs(v.co.x)<.075 and v.co.y>-.105 for v in e.verts)]
    if border:
        fill = bmesh.ops.holes_fill(bm, edges=border, sides=8)
        uv = bm.loops.layers.uv.active
        for i, face in enumerate(fill['faces']):
            face.smooth=True
            # 새 면은 몸과 머리 사이에 비워 둔 UV 띠를 쓴다.
            for j, loop in enumerate(face.loops):
                angle=j/len(face.loops)*math.tau
                loop[uv].uv=(.330+.006*math.cos(angle), .04+i*.025+.010*math.sin(angle))
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(body.data)
    bm.free()
    ig.log(f'CLERK_NECK connected_vertices={len(neck)} triangles={ig.triangle_count(body)}')
    return body


def match_neck_material(ob, textures, name, out):
    """생성한 삼면 원화의 연속된 목 피부를 기준으로 베이크의 색 차이를 보정한다."""
    ig.preview_material_from_bakes(ob, textures)
    mat = ob.data.materials[0]
    bsdf = ig._principled(mat)
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    source = bsdf.inputs['Base Color'].links[0].from_socket

    def math(op, a, b):
        n = nodes.new('ShaderNodeMath'); n.operation = op
        for i, value in enumerate((a, b)):
            if isinstance(value, (int, float)): n.inputs[i].default_value = value
            else: links.new(value, n.inputs[i])
        return n.outputs[0]

    def ramp(value, low, high):
        n = nodes.new('ShaderNodeMapRange'); n.interpolation_type = 'SMOOTHERSTEP'
        links.new(value, n.inputs['Value'])
        n.inputs['From Min'].default_value = low; n.inputs['From Max'].default_value = high
        return n.outputs['Result']

    geometry = nodes.new('ShaderNodeNewGeometry')
    xyz = nodes.new('ShaderNodeSeparateXYZ'); links.new(geometry.outputs['Position'], xyz.inputs[0])
    rgb = nodes.new('ShaderNodeSeparateColor'); links.new(source, rgb.inputs[0])
    # 회색 티셔츠·초록 조끼·검은 머리를 피부 마스크에서 제외한다.
    skin = ramp(math('SUBTRACT', rgb.outputs[0], rgb.outputs[1]), .012, .065)
    front = ramp(xyz.outputs['Y'], -.115, -.072)
    bottom = ramp(xyz.outputs['Z'], 1.331, 1.352)
    top = math('SUBTRACT', 1., ramp(xyz.outputs['Z'], 1.389, 1.414))
    # 교차면에 새로 생긴 면은 기존 UV의 검은 여백을 읽을 수 있다.
    # 목 단면 안쪽은 색에 의존하지 않는 마스크로 채운다. 머리카락과 어깨는 바깥이다.
    x = math('DIVIDE', xyz.outputs['X'], .070)
    y = math('DIVIDE', math('ADD', xyz.outputs['Y'], .030), .090)
    radial = math('ADD', math('MULTIPLY', x, x), math('MULTIPLY', y, y))
    core = math('SUBTRACT', 1., ramp(radial, .85, 1.2))
    coverage = math('MAXIMUM', skin, core)
    mask = math('MULTIPLY', math('MULTIPLY', coverage, front), math('MULTIPLY', bottom, top))
    # 같은 얼굴의 양쪽 볼에서 피부 기준색을 읽는다. 조명·그림자는 색 텍스처에 굽지 않는다.
    image = bpy.data.images.load(textures['D'], check_existing=False)
    pixels = ig._pixels(image); h, w = pixels.shape[:2]
    samples = []
    uv = ob.data.uv_layers.active.data
    for loop in ob.data.loops:
        p = ob.data.vertices[loop.vertex_index].co
        if 1.435 < p.z < 1.475 and .035 < abs(p.x) < .068 and p.y > .055:
            u, v = uv[loop.index].uv
            color = pixels[min(h-1, int(v*h)), min(w-1, int(u*w)), :3]
            if color[0] > color[1] + .06 and color[0] > .60:
                samples.append(color)
    if len(samples) < 12:
        raise RuntimeError('목 색 기준으로 쓸 볼의 피부 표본이 부족하다')
    srgb = np.median(samples, axis=0)
    linear = np.where(srgb <= .04045, srgb/12.92, ((srgb+.055)/1.055)**2.4)
    bpy.data.images.remove(image)
    tone = nodes.new('ShaderNodeRGB'); tone.outputs[0].default_value = (*linear, 1.)
    # 낮은 세기의 원본 색 변화는 남겨 목을 단색 플라스틱처럼 만들지 않는다.
    detail = nodes.new('ShaderNodeMixRGB'); detail.blend_type = 'MIX'
    links.new(math('MAXIMUM', .94, math('SUBTRACT',1.,ramp(rgb.outputs[0],.02,.15))),detail.inputs[0])
    links.new(source, detail.inputs[1]); links.new(tone.outputs[0], detail.inputs[2])
    blend = nodes.new('ShaderNodeMixRGB'); links.new(mask, blend.inputs[0])
    links.new(source, blend.inputs[1]); links.new(detail.outputs[0], blend.inputs[2])
    links.new(blend.outputs[0], bsdf.inputs['Base Color'])
    result = ig._new_image(name+'_Continuous_D', w, srgb=True)
    ig._bake_input_via_emit(ob, result, [mat], 'Base Color')
    ig._save(result, textures['D'])
    bpy.data.images.remove(result)
    ig.log(f'CLERK_SKIN samples={len(samples)} cheek_srgb={srgb.tolist()}')
    # 새 교차면도 정상적인 탄젠트와 자기 폐색을 갖도록 세 채널을 다시 굽는다.
    normal = ig._new_image(name+'_Joined_N', w, srgb=False)
    # 얇은 피부면의 기존 노멀에서 빈 여백을 읽지 않게 이음부는 면 노멀을 쓴다.
    for link in list(bsdf.inputs['Normal'].links): links.remove(link)
    ig._bake(ob, 'NORMAL', normal, [mat])
    px = ig._pixels(normal); px[:, :, 1] = 1.-px[:, :, 1]
    normal.pixels.foreach_set(px.reshape(-1)); ig._save(normal,textures['N'])
    ao = ig._new_image(name+'_Joined_A', w, srgb=False)
    ig._bake(ob,'AO',ao,[mat],samples=16)
    orm = ig._new_image(name+'_Joined_ORM',w,srgb=False)
    px=ig._pixels(ao); px[:,:,1]=.67; px[:,:,2]=0.; px[:,:,3]=1.
    orm.pixels.foreach_set(px.reshape(-1)); ig._save(orm,textures['ORM'])
    for img in (normal,ao,orm): bpy.data.images.remove(img)
    ig.preview_material_from_bakes(ob, textures)
