"""입구와 바닥이 있는 연필꽂이. 작은 문구를 한 메시로 묶는다."""
import math
import os
import sys
import bpy
from mathutils import Matrix

sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig
from build_booth_stationery import pencil_parts


def build_desk_cup(out):
    ig.reset_scene()
    black=ig.mat_plastic('연필꽂이_ABS',(.018,.021,.024),roughness=.43,bump=.004)
    # J.Burrows 실물 사진: 얇은 벽, 열린 입구, 안쪽 바닥과 둥근 윗모서리.
    profile=[(0,0),(.037,0),(.039,.001),(.04,.003),(.04,.092),
             (.0397,.094),(.0388,.095),(.0375,.0946),(.037,.093),
             (.037,.005),(.035,.003),(0,.003)]
    cup=ig.lathe('연필꽂이',profile,segments=40,material=black)
    parts=[cup]
    for i,(angle,tilt,scale) in enumerate(((15,13,1.14),(135,12,1),(255,11,.91))):
        pencil=ig.join(pencil_parts(),f'연필_{i}')
        # 한 자루는 깎인 쪽이 위를 향한다. 축의 밑끝이 컵 바닥에 닿는다.
        turn=Matrix.Translation((0,0,.152)) @ Matrix.Rotation(math.pi,4,'Y') if i==0 else Matrix.Translation((0,0,.002))
        lean=Matrix.Rotation(math.radians(angle),4,'Z') @ Matrix.Rotation(math.radians(tilt),4,'Y')
        radius=.014
        base=(radius*math.cos(math.radians(angle)),radius*math.sin(math.radians(angle)),.0034)
        pencil.matrix_world=Matrix.Translation(base) @ lean @ Matrix.Diagonal((1,1,scale,1)) @ turn
        parts.append(pencil)
    result=ig.build_asset('SM_DeskPencilCup','prop',parts,out,collision_parts=[],texture_size=512,
        sharp_angle=45,preview_yaw=45,
        notes='J.Burrows 연필꽂이 실물 사진의 열린 입구·얇은 벽·바닥 구조. 관리실과 같은 육각 연필 세 자루. 장식 네 컴포넌트를 한 메시로 통합. 512 D/N/ORM 한 재질, 충돌 없음. 바닥 중심 원점, 탁자에 직접 놓는다. 사진 출처는 BEDROOM_REVIEW_20260917.md.')
    path=os.path.join(out,'SM_DeskPencilCup')
    ig.preview_material_from_bakes(bpy.data.objects['SM_DeskPencilCup'],
        {k:os.path.join(path,'SM_DeskPencilCup_'+k+'.png') for k in ('D','N','ORM')})
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(path,'SM_DeskPencilCup.blend'))
    return result


if __name__=='__main__':
    build_desk_cup(ig.out_root_from_argv())
