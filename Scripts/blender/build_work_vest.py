"""안전조끼의 봉제선을 잇고 중력으로 늘어뜨린 뒤 고정 메시로 굽는다."""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector
from mathutils.kdtree import KDTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig


def fabric_material():
    material = ig.mat_plastic('조끼_폴리에스터', (.50, .62, .035), .87, bump=0)
    nodes, links = material.node_tree.nodes, material.node_tree.links
    bsdf = nodes.get('Principled BSDF')
    bsdf.inputs['Sheen Weight'].default_value = .24
    uv = nodes.new('ShaderNodeUVMap')
    uv.uv_map = 'WeaveUV'
    tex = nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(os.path.abspath(os.path.join(
        __file__, '..', '..', '..', 'Content', 'SourceArt', 'AI', 'WorkVestKnit_20260917.png')))
    links.new(uv.outputs['UV'], tex.inputs['Vector'])
    weave_color = nodes.new('ShaderNodeMixRGB')
    weave_color.inputs[0].default_value = .28
    weave_color.inputs[1].default_value = (.46, .57, .029, 1)
    links.new(tex.outputs['Color'], weave_color.inputs[2])
    # 테이프를 면 번호로 칠하면 폴리곤 축약 때 지그재그가 된다.
    # 재단 UV에서 경계를 계산해 굽고, 메시 축약과 인쇄선을 분리한다.
    separate = nodes.new('ShaderNodeSeparateXYZ')
    links.new(uv.outputs['UV'], separate.inputs[0])
    def math_node(operation, a, b=0):
        node = nodes.new('ShaderNodeMath')
        node.operation = operation
        for index, value in enumerate((a, b)):
            if isinstance(value, (int, float)):
                node.inputs[index].default_value = value
            else:
                links.new(value, node.inputs[index])
        return node.outputs[0]
    x = math_node('ABSOLUTE', separate.outputs['X'])
    z = separate.outputs['Y']
    def interval(value, low, high):
        return math_node('MULTIPLY', math_node('GREATER_THAN', value, low/.12),
                         math_node('LESS_THAN', value, high/.12))
    horizontal = math_node('MAXIMUM', interval(z, .115, .160), interval(z, .270, .320))
    vertical = math_node('MULTIPLY', interval(x, .100, .150), math_node('GREATER_THAN', z, .320/.12))
    mask = math_node('MAXIMUM', horizontal, vertical)
    mix = nodes.new('ShaderNodeMixRGB')
    links.new(mask, mix.inputs[0])
    links.new(weave_color.outputs[0], mix.inputs[1])
    mix.inputs[2].default_value = (.30, .32, .31, 1)
    links.new(mix.outputs[0], bsdf.inputs['Base Color'])
    rough = nodes.new('ShaderNodeMapRange')
    links.new(mask, rough.inputs['Value'])
    rough.inputs['To Min'].default_value = .87
    rough.inputs['To Max'].default_value = .48
    links.new(rough.outputs[0], bsdf.inputs['Roughness'])
    bump = nodes.new('ShaderNodeBump')
    bump.inputs['Strength'].default_value = .035
    bump.inputs['Distance'].default_value = .00016
    links.new(tex.outputs['Color'], bump.inputs['Height'])
    links.new(bump.outputs['Normal'], bsdf.inputs['Normal'])
    return material


def build_vest(out):
    ig.reset_scene()
    scene = bpy.context.scene
    scene.render.fps = 30
    fabric = fabric_material()
    binding = ig.mat_plastic('직물_바인딩', (.019, .021, .018), .88, bump=.05, grain_scale=1800)
    wire = ig.mat_metal('옷걸이', (.036, .039, .035), roughness=.55, streak=0)
    hook = ig.mat_plastic('벽걸이', (.66, .64, .58), .59, bump=0)

    # 반사띠의 위·아래 경계가 재단 좌표와 일치해야 주름에도 폭이 유지된다.
    rows = [.010, .035, .075, .115, .160, .195, .230, .270, .320,
            .355, .390, .430, .470, .510, .550, .585, .612, .630]
    vertices, faces, weave = [], [], []
    panels = {}
    pins = []
    columns = 8
    for back in (False, True):
        for sign in (-1, 1):
            panel = []
            for ri, z in enumerate(rows):
                t = max(0, (z-.355)/.275)
                inner = 0 if back else .004 + t*.079
                if back and z > .585:
                    inner = .083 * math.sqrt(max(0, 1-((.630-z)/.045)**2))
                # 사진의 암홀은 옆선부터 어깨로 완만하게 들어간다.
                outer = .252 - .064 * math.sin(t*math.pi/2)
                xs = [inner, (inner+.100)/2, .100, .125, .150,
                      .150+(outer-.150)*.25, .150+(outer-.150)*.5,
                      .150+(outer-.150)*.75, outer]
                strip = []
                for x in xs:
                    x *= sign
                    # 어깨가 옷걸이에 닿는다. 초기 여유분이 중력 아래서 주름이 된다.
                    shoulder = (.668 - abs(x)*.339 + .001) - .630
                    height = z + shoulder * max(0, (z-.43)/.20)**2
                    fold = .012*math.sin(x*45+z*9) + .007*math.sin(x*81-z*8)
                    depth = (.017 if back else .056) + fold*(.3 if back else 1)
                    index = len(vertices)
                    vertices.append((x, depth, height))
                    weave.append((x/.12, z/.12))
                    strip.append(index)
                    if ri == len(rows)-1:
                        pins.append(index)
                panel.append(strip)
            for ri in range(len(rows)-1):
                for ci in range(columns):
                    face = (panel[ri][ci], panel[ri][ci+1], panel[ri+1][ci+1], panel[ri+1][ci])
                    if (sign > 0) != back:
                        face = tuple(reversed(face))
                    faces.append(face)
            panels[back, sign] = panel

    for sign in (-1, 1):
        front, back = panels[False, sign], panels[True, sign]
        for ri in range(rows.index(.355)):
            face = (front[ri][-1], back[ri][-1], back[ri+1][-1], front[ri+1][-1])
            faces.append(face if sign > 0 else tuple(reversed(face)))
        for ci in range(columns):
            face = (front[-1][ci], front[-1][ci+1], back[-1][ci+1], back[-1][ci])
            faces.append(face if sign > 0 else tuple(reversed(face)))

    mesh = bpy.data.meshes.new('조끼_봉제패턴')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    garment = ig._link(bpy.data.objects.new('조끼_천', mesh))
    mesh.materials.append(fabric)
    uv = mesh.uv_layers.new(name='WeaveUV')
    for poly in mesh.polygons:
        poly.material_index = 0
        poly.use_smooth = True
        for loop in poly.loop_indices:
            uv.data[loop].uv = weave[mesh.loops[loop].vertex_index]
    # 뒤판 중심 봉제선을 먼저 용접한다. 겹친 정점은 자기 충돌에서 서로 튀어 오른다.
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.00015)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    lookup = KDTree(len(mesh.vertices))
    for vertex in mesh.vertices:
        lookup.insert(vertex.co, vertex.index)
    lookup.balance()
    remap = [lookup.find(co)[1] for co in vertices]
    pins = list({remap[i] for i in pins})
    wall = ig.box('제작시_벽충돌', (1.2,.02,1.2), (0,-.016,.35))
    wall.modifiers.new('벽충돌', 'COLLISION')
    pinned = garment.vertex_groups.new(name='어깨고정')
    pinned.add(pins, 1, 'REPLACE')
    ig.set_active(garment, solo=True)
    cloth = garment.modifiers.new('제작시_천계산', 'CLOTH')
    cloth.settings.quality = 8
    cloth.settings.mass = .12
    cloth.settings.air_damping = 3
    cloth.settings.tension_stiffness = 30
    cloth.settings.compression_stiffness = 30
    cloth.settings.shear_stiffness = 15
    cloth.settings.bending_stiffness = .15
    cloth.settings.vertex_group_mass = pinned.name
    cloth.settings.pin_stiffness = 1
    cloth.collision_settings.use_self_collision = True
    cloth.collision_settings.distance_min = .0015
    cloth.collision_settings.self_distance_min = .0015
    cloth.collision_settings.self_friction = 8
    cloth.point_cache.frame_end = 75
    for frame in range(1, 76):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        # 프레임마다 평가해야 백그라운드에서도 연속된 천 계산을 얻는다.
        evaluated = garment.evaluated_get(bpy.context.evaluated_depsgraph_get())
        evaluated.to_mesh()
        evaluated.to_mesh_clear()
    bpy.ops.object.modifier_apply(modifier=cloth.name)
    movement = max((garment.data.vertices[remap[i]].co - Vector(co)).length
                   for i, co in enumerate(vertices))
    ig.log(f'천 계산 75프레임 최대 변위: {movement*100:.2f} cm')
    if movement < .003:
        raise RuntimeError('천 계산이 메시를 변형하지 않았다')
    bpy.data.objects.remove(wall, do_unlink=True)
    scene.frame_set(1)

    # 곡면을 만든 뒤 경계를 추출해야 바인딩이 옷감에서 떨어지지 않는다.
    parts = [garment]
    ig.set_active(garment, solo=True)
    smooth = garment.modifiers.new('주름곡면', 'SUBSURF')
    smooth.levels = 1
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    bm = bmesh.new()
    bm.from_mesh(garment.data)
    remaining = {edge for edge in bm.edges if edge.is_boundary}
    while remaining:
        edge = remaining.pop()
        start, current = edge.verts
        points = [tuple(start.co), tuple(current.co)]
        while current != start:
            candidates = [item for item in current.link_edges if item in remaining]
            if not candidates:
                break
            edge = candidates[0]
            remaining.remove(edge)
            current = edge.other_vert(current)
            points.append(tuple(current.co))
        parts.append(ig.pipe('바인딩', points, .001, resolution=3, material=binding))
    bm.free()
    solid = garment.modifiers.new('원단_두께', 'SOLIDIFY')
    solid.thickness = .0006
    solid.offset = 0
    ig.set_active(garment, solo=True)
    bpy.ops.object.modifier_apply(modifier=solid.name)
    # 같은 장소에 놓인 뒤판의 중앙 두 에지를 용접한다.
    bm = bmesh.new()
    bm.from_mesh(garment.data)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.00015)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(garment.data)
    bm.free()

    parts.append(ig.pipe('옷걸이', [(-.225,.033,.590),(-.231,.033,.595),(0,.033,.668),(.231,.033,.595),(.225,.033,.590),(-.225,.033,.590)],
        .0016, resolution=5, corner_radius=.006, corner_steps=3, material=wire))
    parts.append(ig.pipe('옷걸이_고리', [(0,.033,.668),(0,.033,.681),(.014,.033,.696),(.016,.033,.710),(.007,.033,.720),(-.008,.033,.719),(-.014,.033,.710)],
        .0016, resolution=5, corner_radius=.007, corner_steps=3, material=wire))
    parts.append(ig.box('벽고리받침', (.025,.005,.040), (0,.0025,.705), bevel=.004, segments=2, material=hook))
    parts.append(ig.pipe('벽고리', [(0,.006,.698),(0,.032,.698),(0,.038,.707)], .0025,
        resolution=5, corner_radius=.004, corner_steps=3, material=hook))
    vest = ig.join(parts, '주름이잡힌조끼')
    ig.decimate(vest, 2980)
    if ig.triangle_count(vest) > 3000:
        raise RuntimeError('작업조끼 삼각형 예산 초과')
    manifest = ig.build_asset('SM_HangingWorkVest', 'prop', [vest], out, collision_parts=[],
        texture_size=2048, preview_yaw=200, sharp_angle=65,
        notes='3M 실물 사진·WorkVestReference·image_gen 원단 스캔을 참고. 봉제 패턴에 75프레임 천 계산을 적용한 고정 메시. 런타임 천 계산 없음. 삼각형 3000 이내·한 재질·2K D/N/ORM·4단계 LOD. 반사띠는 재단 좌표에 고정. 앞 +Y, 벽면 밑단 원점. WorkVestFinish_20260917.json 참조.')
    asset_dir = os.path.join(out, 'SM_HangingWorkVest')
    textures = {key: os.path.join(asset_dir, 'SM_HangingWorkVest_'+key+'.png') for key in ('D','N','ORM')}
    ig.preview_material_from_bakes(bpy.data.objects['SM_HangingWorkVest'], textures)
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(asset_dir, 'SM_HangingWorkVest.blend'))
    return manifest


if __name__ == '__main__':
    build_vest(ig.out_root_from_argv())
