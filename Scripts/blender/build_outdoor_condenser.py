"""LG 설치 사진과 다각도 참고도로 만든 실외기. 벽면 원점, 팬은 -Y."""
import math
import os
import sys
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig


def beam(name, a, b, width, material):
    direction = Vector(b)-Vector(a)
    ob = ig.box(name, (width,width,direction.length),
                location=(Vector(a)+Vector(b))/2, material=material)
    ob.rotation_euler = direction.to_track_quat('Z','Y').to_euler()
    return ob


def fan_blade(index, material):
    verts,faces=[],[]
    for row in range(7):
        t=row/6
        radius=.034+.174*t
        for col in range(4):
            across=col/3
            angle=index*math.tau/3 + .50*t + (across-.5)*(.5+.65*t)
            verts.append((-.14+radius*math.cos(angle),
                          -.406 + .022*(across-.5)*t, .28+radius*math.sin(angle)))
    for row in range(6):
        for col in range(3):
            k=row*4+col
            faces.append((k,k+1,k+5,k+4))
    mesh=bpy.data.meshes.new('fan_blade')
    mesh.from_pydata(verts,[],faces);mesh.update()
    ob=bpy.data.objects.new('fan_blade_'+str(index),mesh)
    bpy.context.collection.objects.link(ob)
    mesh.materials.append(material)
    for poly in mesh.polygons: poly.use_smooth=True
    solid=ob.modifiers.new('blade_thickness','SOLIDIFY');solid.thickness=.002
    return ob


def build(out):
    ig.reset_scene()
    paint=ig.mat_painted_steel('PowderCoat',(.59,.585,.555),roughness=.48,wear=.065,bump=.012)
    grille=ig.mat_painted_steel('WireGuard',(.43,.44,.43),roughness=.47,wear=.02,bump=.004)
    metal=ig.mat_metal('GalvanizedBracket',(.46,.49,.50),roughness=.47,streak=.12)
    dark=ig.mat_plastic('FanABS',(.023,.027,.03),roughness=.52,bump=0)
    rubber=ig.mat_rubber('RubberIsolator',(.018,.019,.018))
    wrap=ig.mat_plastic('PipeInsulation',(.56,.52,.43),roughness=.86,bump=.022,grain_scale=360)
    parts=[]
    # 몸통 전체를 채우지 않는다. 앞판에 원형 개구부를 뚫고 팬을 4cm 안쪽에 둔다.
    front=ig.box('front_shell',(.8,.003,.55),(0,-.4515,.275),bevel=.003,material=paint)
    cutter=ig.cylinder('fan_opening',.224,.035,(-.14,-.452,.28),
                       rotation=(math.pi/2,0,0),segments=64)
    ig.boolean(front,cutter)
    parts.append(front)
    for name,size,pos in (
        ('top',(.8,.306,.004),(0,-.302,.548)),
        ('base',(.79,.30,.004),(0,-.30,.003)),
        ('left',(.003,.30,.55),(-.3985,-.30,.275)),
        ('right',(.003,.30,.55),(.3985,-.30,.275))):
        parts.append(ig.box(name,size,pos,bevel=.002,material=paint))
    rear=ig.box('rear_fins',(.77,.009,.50),(0,-.159,.275),material=dark)
    parts.append(rear)
    for z in (.06,.14,.22,.30,.38,.46):
        parts.append(ig.box('rear_guard',(.78,.006,.006),(0,-.15,z),material=paint))
    for x in (-.3,-.1,.1,.3):
        parts.append(ig.box('rear_upright',(.006,.006,.50),(x,-.148,.275),material=paint))
    # 옆 흡입구는 얕게 들어간 열교환기와 얇은 보호살로 나눈다.
    parts.append(ig.box('side_fins',(.003,.213,.39),(.4005,-.287,.30),material=dark))
    for z in (.115,.18,.245,.31,.375,.44,.495):
        parts.append(ig.box('side_guard',(.007,.218,.006),(.404,-.287,z),material=paint))
    for y in (-.385,-.285,-.185):
        parts.append(ig.box('side_upright',(.007,.006,.385),(.404,y,.303),material=paint))
    for index in range(3): parts.append(fan_blade(index,dark))
    parts.append(ig.cylinder('motor_hub',.042,.033,(-.14,-.412,.28),
                            rotation=(math.pi/2,0,0),segments=32,material=dark))
    # 촘촘한 와이어는 가까운 LOD에서만 유지한다. 투명 평면을 겹치지 않는다.
    for i in range(10):
        radius=.046+.0193*i
        parts.append(ig.torus('wire_ring_'+str(i),radius,.00135,
                     (-.14,-.468+.006*(radius/.22)**2,.28),(math.pi/2,0,0),
                     major_segments=56,minor_segments=4,material=grille))
    for i in range(10):
        a=i*math.tau/10
        parts.append(ig.pipe('wire_spoke_'+str(i),
            [(-.14,-.47,.28),(-.14+.222*math.cos(a),-.462,.28+.222*math.sin(a))],
            .0014,resolution=4,material=grille))
    parts.append(ig.cylinder('guard_center',.034,.004,(-.14,-.472,.28),
                            rotation=(math.pi/2,0,0),segments=32,material=paint))
    parts.append(ig.torus('opening_lip',.226,.003,(-.14,-.453,.28),(math.pi/2,0,0),
                          major_segments=64,minor_segments=4,material=paint))
    parts.append(ig.box('service_cover',(.262,.004,.43),(.247,-.456,.282),bevel=.008,segments=3,material=paint))
    for z in (.12,.18,.24):
        parts.append(ig.box('pressed_line',(.197,.002,.004),(.247,-.459,z),bevel=.002,material=paint))
    for x,z in ((-.38,.525),(.38,.525),(-.38,.025),(.38,.025),(.35,.45),(.35,.08)):
        parts.append(ig.cylinder('screw',.0035,.002,(x,-.46,z),rotation=(math.pi/2,0,0),segments=8,material=metal))
    # 받침과 대각 버팀대를 모델에 한 번만 넣는다. 벽판 후면은 Y=0이다.
    for x in (-.28,.28):
        parts.append(ig.box('wall_anchor',(.048,.008,.29),(x,-.004,-.16),material=metal))
        parts.append(ig.box('support_arm',(.033,.50,.028),(x,-.25,-.035),material=metal))
        parts.append(beam('diagonal_brace',(x,-.017,-.28),(x,-.45,-.048),.022,metal))
        for y in (-.19,-.405):
            parts.append(ig.cylinder('isolator',.017,.021,(x,y,-.0095),segments=12,material=rubber))
        for z in (-.055,-.263):
            parts.append(ig.cylinder('anchor_bolt',.007,.006,(x,-.011,z),rotation=(math.pi/2,0,0),segments=6,material=metal))
    parts.append(ig.pipe('insulated_lines',[(.395,-.24,.095),(.47,-.24,.095),
        (.51,-.19,.17),(.61,-.10,.33),(.61,-.075,.62),(.61,0,.69)],
        .022,resolution=8,corner_radius=.065,corner_steps=6,material=wrap))
    parts.append(ig.cylinder('wall_sleeve',.045,.012,(.61,-.006,.69),
                            rotation=(math.pi/2,0,0),segments=24,material=wrap))
    # 여기서는 설치물의 외형만 충돌로 저장한다. 게임 배치는 보행 높이 밖이다.
    collision=ig.box('collision',(.8,.30,.55),(0,-.30,.275))
    collision.hide_render=True
    result=ig.build_asset('SM_AcOutdoorUnit','large',parts,out,
        collision_parts=[[collision]],texture_size=2048,preview_yaw=26,
        notes='LG 실외기 설치 사진과 OutdoorCondenserStudy_20260917.png 참고. 외함 80×30×55cm, 후면 벽 간격 15cm. 열린 팬, 3개 날개, 얇은 와이어 보호망, 받침 두 개와 절연 배관. 팬 -Y, 벽면 Y=0. 하나의 재질과 4단계 LOD. 받침은 별도로 덧대지 않는다.')
    folder=os.path.join(out,'SM_AcOutdoorUnit')
    ig.preview_material_from_bakes(bpy.data.objects['SM_AcOutdoorUnit'],
        {k:os.path.join(folder,'SM_AcOutdoorUnit_'+k+'.png') for k in ('D','N','ORM')})
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder,'SM_AcOutdoorUnit.blend'))
    return result


if __name__=='__main__':
    build(ig.out_root_from_argv())
