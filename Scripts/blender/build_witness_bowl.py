"""401호 앞 물그릇. 눌러 만든 스테인리스 그릇과 접어 깐 종이를 묶는다."""
import os
import sys
import bpy

sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig


def build(out):
    ig.reset_scene()
    steel=ig.mat_metal('BrushedStainless',(.62,.65,.66),roughness=.27,streak=.18,anisotropic=False)
    rubber=ig.mat_rubber('AntiSlipRing',(.024,.027,.025))
    paper=ig.mat_image('FoldedPaper',os.path.abspath('Content/SourceArt/T_PaperClean_V2_D.png'),roughness=.91)
    # 실물의 22cm 외경을 18cm로 줄인 낮은 그릇. 입구·안벽·바닥이 이어진다.
    profile=[(0,.003),(.080,.003),(.084,.004),(.085,.007),
        (.071,.040),(.070,.043),(.068,.045),(.066,.045),(.064,.043),
        (.064,.040),(.057,.012),(.052,.009),(0,.009)]
    bowl=ig.lathe('thin_pressed_bowl',profile,segments=64,location=(0,0,.001),material=steel)
    for poly in bowl.data.polygons: poly.use_smooth=True
    base=ig.lathe('foot_ring',[(.079,.001),(.088,.001),(.09,.002),(.09,.004),
        (.087,.006),(.082,.005),(.079,.001)],segments=64,location=(0,0,.001),material=rubber)
    # 종이는 독립적인 낮은 표면이다. 사각 금속 블록을 받침으로 남기지 않는다.
    verts=[(-.109,-.112,.0003),(.103,-.115,.0003),(.107,.109,.0003),(-.107,.111,.0003),
           (-.109,0,.0011),(.105,0,.0011)]
    mesh=bpy.data.meshes.new('folded_paper');mesh.from_pydata(verts,[],[(0,1,5,4),(4,5,2,3)]);mesh.update()
    sheet=bpy.data.objects.new('folded_paper',mesh);bpy.context.collection.objects.link(sheet)
    mesh.materials.append(paper)
    solid=sheet.modifiers.new('paper_edge','SOLIDIFY');solid.thickness=.00035
    # 수면은 별도 불투명 재질 슬롯을 늘리지 않고 같은 구운 표면으로 넣는다.
    # 3cm 안팎의 물 아래 금속 바닥색을 남기고, 저거칠기 반사로 수면을 읽는다.
    water=ig.mat_plastic('StillWater',(.10,.125,.12),roughness=.085,bump=0)
    water_surface=ig.cylinder('water_surface',.061,.0004,(0,0,.029),segments=64,material=water)
    result=ig.build_asset('SM_WitnessWaterBowl','prop',[bowl,base,sheet,water_surface],out,
        collision_parts=[[bowl]],texture_size=1024,preview_yaw=30,
        notes='IKEA UTSADD 실물 사진과 CatWaterBowlStudy_20260917.png 참고. 외경 18cm, 높이 4.6cm. 열린 안벽과 말린 테두리, 미끄럼 방지 고무, 접은 받침 종이. 바닥 원점, 1개 재질, 1개 조사 충돌, Pawn 충돌은 사용하지 않는다. 수면은 굴절 없는 정적 근사.')
    folder=os.path.join(out,'SM_WitnessWaterBowl')
    ig.preview_material_from_bakes(bpy.data.objects['SM_WitnessWaterBowl'],
        {k:os.path.join(folder,'SM_WitnessWaterBowl_'+k+'.png') for k in ('D','N','ORM')})
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder,'SM_WitnessWaterBowl.blend'))
    return result


if __name__=='__main__':
    build(ig.out_root_from_argv())
