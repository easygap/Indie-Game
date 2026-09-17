"""실물 면 침구 사진을 참고해 403호 침구를 고정 메시로 굽는다."""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig


def cotton(name, tint):
    mat = ig.mat_plastic(name, tint, .87, bump=0)
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get('Principled BSDF')
    bsdf.inputs['Sheen Weight'].default_value = .18
    tex = nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(os.path.abspath(os.path.join(
        __file__, '..', '..', '..', 'Content/SourceArt/AI/BeddingCotton_20260917.png')))
    uv = nodes.new('ShaderNodeUVMap')
    uv.uv_map = 'CottonUV'
    links.new(uv.outputs[0], tex.inputs[0])
    gray = nodes.new('ShaderNodeRGBToBW')
    links.new(tex.outputs['Color'], gray.inputs[0])
    factor = nodes.new('ShaderNodeMapRange')
    links.new(gray.outputs[0], factor.inputs['Value'])
    factor.inputs['From Min'].default_value = .28
    factor.inputs['From Max'].default_value = .70
    factor.inputs['To Min'].default_value = .91
    factor.inputs['To Max'].default_value = 1.04
    mix = nodes.new('ShaderNodeMixRGB')
    mix.blend_type = 'MULTIPLY'
    mix.inputs[0].default_value = 1
    mix.inputs[1].default_value = (*tint, 1)
    links.new(factor.outputs[0], mix.inputs[2])
    links.new(mix.outputs[0], bsdf.inputs['Base Color'])
    bump = nodes.new('ShaderNodeBump')
    bump.inputs['Strength'].default_value = .045
    bump.inputs['Distance'].default_value = .00018
    links.new(gray.outputs[0], bump.inputs['Height'])
    links.new(bump.outputs['Normal'], bsdf.inputs['Normal'])
    return mat


def fabric_uv(ob):
    uv = ob.data.uv_layers.new(name='CottonUV')
    for poly in ob.data.polygons:
        axes = (0, 1) if abs(poly.normal.z) > .55 else ((1, 2) if abs(poly.normal.x) > .55 else (0, 2))
        for loop in poly.loop_indices:
            co = ob.matrix_world @ ob.data.vertices[ob.data.loops[loop].vertex_index].co
            uv.data[loop].uv = (co[axes[0]]/.16, co[axes[1]]/.16)


def pillow(material):
    # 면 두 장을 테두리에서 맞대어 봉제한 베개. 가장자리가 상자처럼 두껍지 않다.
    n, m = 34, 24
    vertices, faces = [], []
    for side in (1, -1):
        for j in range(m+1):
            v = j/m*2-1
            for i in range(n+1):
                u = i/n*2-1
                fullness = max(0, (1-u**4)*(1-v**4))**.38
                edge = min(1-abs(u), 1-abs(v))
                gather = .0032*math.sin(u*42+v*17)*math.exp(-edge*12)*math.sin(edge*35)
                x = u*.35*(1-.075*abs(v)**8)
                y = v*.225*(1-.095*abs(u)**8)
                z = .010 + ((.086 if side > 0 else -.023)*fullness) + gather
                if side > 0:
                    z -= .010*math.exp(-((u-.14)**2+(v+.10)**2)/.23)
                vertices.append((x, y, z))
    count=(n+1)*(m+1)
    for side in range(2):
        offset=side*count
        for j in range(m):
            for i in range(n):
                a=offset+j*(n+1)+i
                q=(a,a+1,a+n+2,a+n+1)
                faces.append(q if side == 0 else tuple(reversed(q)))
    mesh=bpy.data.meshes.new('베개_봉제면')
    mesh.from_pydata(vertices,[],faces)
    mesh.update()
    ob=ig._link(bpy.data.objects.new('베개',mesh))
    bm=bmesh.new();bm.from_mesh(mesh)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.0001)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh);bm.free()
    mesh.materials.append(material)
    for poly in mesh.polygons: poly.use_smooth=True
    # 눌린 아랫면을 매트리스 윗면에 놓는다.
    lowest=min(v.co.z for v in mesh.vertices)
    for v in mesh.vertices: v.co.z += .182-lowest
    ob.location.y=.675
    ob.rotation_euler.z=math.radians(-3)
    fabric_uv(ob)
    return ob


def duvet(material, mattress):
    n,m=48,62
    vertices,faces=[] ,[]
    for j in range(m+1):
        t=j/m
        # 윗단 19cm를 되접는다. 한 장의 연속된 면이므로 접힌 곳이 벌어지지 않는다.
        y=-.87+1.43*min(t/.85,1)
        if t > .85: y=.56-(t-.85)/.15*.19
        fold_height=.018*math.sin(max(0,(t-.82)/.18)*math.pi)
        for i in range(n+1):
            u=(i/n*2-1)*.69
            edge=max(0,abs(u)-.43)
            x=math.copysign(.43+.06*math.sin(min(edge/.08,math.pi/2)),u) if edge else u
            drop=max(0,edge-.03)
            waves=.010*math.sin(u*17+y*6)+.006*math.sin(u*36-y*13)+.004*math.sin(y*25+u*4)
            # 중심을 향하는 주름 몇 개에만 높이를 준다. 바둑판식 반복을 피한다.
            ridge=.013*math.exp(-((u-.12-y*.16)/.10)**2)*math.exp(-((y+.18)/.55)**2)
            z=.207+waves+ridge+fold_height-drop
            if t>.85: z+=.018
            vertices.append((x,y,z))
    for j in range(m):
        for i in range(n):
            a=j*(n+1)+i
            faces.append((a,a+1,a+n+2,a+n+1))
    mesh=bpy.data.meshes.new('이불_재단면')
    mesh.from_pydata(vertices,[],faces);mesh.update()
    ob=ig._link(bpy.data.objects.new('이불',mesh))
    mesh.materials.append(material)
    uv=mesh.uv_layers.new(name='CottonUV')
    for poly in mesh.polygons:
        poly.use_smooth=True
        for loop in poly.loop_indices:
            index=mesh.loops[loop].vertex_index
            uv.data[loop].uv=((index%(n+1))/n*1.38/.16,(index//(n+1))/m*1.62/.16)
    pinned=ob.vertex_groups.new(name='놓인부분')
    for i,co in enumerate(vertices):
        if abs(co[0])<.32 and i//(n+1)<int(m*.80):
            pinned.add([i],.7,'REPLACE')
    mattress.modifiers.new('매트리스_접촉','COLLISION')
    mattress.collision.thickness_outer=.006
    ig.set_active(ob)
    cloth=ob.modifiers.new('제작시_천계산','CLOTH')
    cloth.settings.quality=8
    cloth.settings.mass=.28
    cloth.settings.air_damping=4
    cloth.settings.tension_stiffness=22
    cloth.settings.compression_stiffness=22
    cloth.settings.shear_stiffness=15
    cloth.settings.bending_stiffness=.6
    cloth.settings.vertex_group_mass=pinned.name
    cloth.settings.pin_stiffness=1
    cloth.collision_settings.distance_min=.003
    cloth.collision_settings.use_self_collision=True
    cloth.collision_settings.self_distance_min=.004
    cloth.point_cache.frame_end=60
    for frame in range(1,61):
        bpy.context.scene.frame_set(frame)
        bpy.context.view_layer.update()
        evaluated=ob.evaluated_get(bpy.context.evaluated_depsgraph_get())
        evaluated.to_mesh();evaluated.to_mesh_clear()
    bpy.ops.object.modifier_apply(modifier=cloth.name)
    movement=max((v.co-Vector(vertices[v.index])).length for v in ob.data.vertices)
    ig.log(f'침구 천 계산 60프레임 최대 변위 {movement*100:.2f}cm')
    if movement<.003: raise RuntimeError('침구 천 계산이 적용되지 않았다')
    mattress.modifiers.remove(mattress.modifiers.get('매트리스_접촉'))
    bpy.context.scene.frame_set(1)
    smooth=ob.modifiers.new('주름_곡면','SUBSURF');smooth.levels=1
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    # 천의 두께는 외곽 실루엣에만 남긴다. 별도 반투명 재질은 쓰지 않는다.
    solid=ob.modifiers.new('이불_두께','SOLIDIFY');solid.thickness=.009;solid.offset=-.5
    bpy.ops.object.modifier_apply(modifier=solid.name)
    # 서쪽 벽과 매트리스 사이의 3cm 틈 안으로 이불 가장자리를 접어 넣는다.
    for vertex in ob.data.vertices:
        if vertex.co.x < -.485:
            vertex.co.x = -.485 + (vertex.co.x + .485)*.08
    ig.decimate(ob,6200)
    return ob


def build_bedding(out):
    ig.reset_scene()
    sheet=cotton('매트리스_면커버',(.44,.424,.385))
    cover=cotton('이불_면커버',(.145,.195,.225))
    pillow_mat=cotton('베개_면커버',(.55,.535,.497))
    mattress=ig.box('매트리스',(.94,1.94,.18),(0,0,.09),bevel=.024,segments=5,material=sheet)
    for poly in mattress.data.polygons: poly.use_smooth=True
    fabric_uv(mattress)
    cushion=pillow(pillow_mat)
    cloth=duvet(cover,mattress)
    bed=ig.join([mattress,cushion,cloth],'침구')
    ig.decimate(bed,8500)
    collision=ig.box('침구_단순충돌',(.94,1.94,.18),(0,0,.09))
    collision.hide_render=True
    result=ig.build_asset('SM_ApartmentBedding','large',[bed],out,collision_parts=[[collision]],
        texture_size=2048,preview_yaw=60,sharp_angle=70,
        notes='IKEA 면 침구·베개 실물 사진과 BeddingStudy_20260917 참고. imagegen 면 원단 입력. 60프레임 천 계산을 고정 메시로 굽고 런타임 계산은 하지 않는다. 매트리스 94x194x18cm, 베개 70x45cm, 2K D/N/ORM 한 재질. 원점은 매트리스 밑면 중심. 403호 (-140,110,38)에 배치. Bedding_20260917.json 참조.')
    path=os.path.join(out,'SM_ApartmentBedding')
    textures={k:os.path.join(path,'SM_ApartmentBedding_'+k+'.png') for k in ('D','N','ORM')}
    ig.preview_material_from_bakes(bpy.data.objects['SM_ApartmentBedding'],textures)
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(path,'SM_ApartmentBedding.blend'))
    return result


if __name__=='__main__':
    build_bedding(ig.out_root_from_argv())
