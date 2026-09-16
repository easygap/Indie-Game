"""3.2mm 접힌 철판. 돌기는 고밀도에서만 만들고 216삼각형 마감판에 굽는다."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import ig_blender_lib as ig

ig.reset_scene()
name='SM_RoofStairTread'
out=os.path.join(ig.out_root_from_argv(), name)
os.makedirs(out,exist_ok=True)
paint=ig.mat_image('StairPaint',os.path.join(os.path.dirname(__file__),'../../Content/SourceArt/AI/StairPaintAlbedo_v1.png'),roughness=.65,metallic=.06)
nodes=paint.node_tree.nodes
coords=nodes.new('ShaderNodeTexCoord')
tex=next(n for n in nodes if n.bl_idname=='ShaderNodeTexImage')
tex.projection='BOX'
tex.projection_blend=.1
paint.node_tree.links.new(coords.outputs['Object'],tex.inputs['Vector'])
# 원점은 기존 충돌 상자의 단 윗면. 위 0.03cm, 앞 0.05cm 간격으로 겹침을 피한다.
low=ig.join([
    ig.box('tread',(.85,.222,.0032),(0,-.001,.0019),bevel=.0008,segments=2,material=paint),
    ig.box('riser',(.85,.0032,.167),(0,-.1116,-.0820),bevel=.0008,segments=2,material=paint)
],name)
high=low.copy(); high.data=low.data.copy();ig._link(high)
high_parts=[high]
# 34mm 간격, 길이 25mm·폭 7mm·높이 1.2mm. 알베도 명암 없이 실제 법선으로만 읽힌다.
for ix in range(25):
    for iy in range(6):
        a=math.pi/4 if (ix+iy)%2 else -math.pi/4
        x=(ix-12)*.034
        y=(iy-2.5)*.034
        diamond=ig.cylinder('raised_tear',1,.0012,(x,y,.0041),segments=8,bevel=None,material=paint)
        diamond.scale=(.0125,.0035,1)
        diamond.rotation_euler[2]=a
        high_parts.append(diamond)
high=ig.join(high_parts,'TreadHigh')
ig.mark_sharp_by_angle(high,45)
ig.mark_sharp_by_angle(low,45)
low.hide_render=True
ig.render_preview(high,os.path.join(out,name+'_preview.png'))
ig.render_preview(high,os.path.join(out,name+'_preview_torch.png'),flashlight=True)
low.hide_render=False
ue_bounds=ig.bounds(low)
ig.mirror_y(low);ig.mirror_y(high)
ig.uv_smart(low,margin=.008)
textures=ig.bake_from_high(low,high,name,out,size=1024,cage_extrusion=.004,max_ray_distance=.009)
high.hide_render=True
slots=ig.finalize_slots(low)
ig.ensure_primary_uv(low)
fbx=os.path.join(out,name+'.fbx')
ig.export_fbx(fbx,[low])
ig.write_manifest(out,name,'prop',fbx,textures,low,slots,
    '85×22cm 접힌 철판. 원점은 단 윗면. 충돌 없음. 34mm 간격의 돌기를 고밀도에서 노멀맵으로 굽고 색상은 imagegen StairPaintAlbedo_v1 사용. 실루엣만 저밀도로 남김.',origin='center',ue_bounds=ue_bounds)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out,name+'.blend'))
ig.log(f'{name}: done tris={ig.triangle_count(low)}')
