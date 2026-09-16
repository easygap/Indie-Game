"""실물 규격의 발신기 세트와 3.3kg 소화기. 참조·생성 원본은 SourceArt/AI의 20260916 자료."""
import math
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import ig_blender_lib as ig

PRINTS = os.path.join(os.path.dirname(__file__), '../../Content/SourceArt/UtilityPrints')
FRONT = (math.pi/2, 0, 0)


def printed_disc(name, radius, location, material, segments=32):
    vertices = [(0, 0, 0)] + [(radius*math.cos(i*math.tau/segments), 0, radius*math.sin(i*math.tau/segments)) for i in range(segments)]
    faces = [(0, i+1, (i+1) % segments+1) for i in range(segments)]
    me = bpy.data.meshes.new(name)
    me.from_pydata(vertices, [], faces)
    layer = me.uv_layers.new(name='ImageUV')
    for poly in me.polygons:
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            layer.data[li].uv = (co.x/(2*radius)+.5, co.z/(2*radius)+.5)
    ob = ig._link(bpy.data.objects.new(name, me))
    ob.location = location
    ig.assign_material(ob, material)
    return ob


def alarm(out):
    ig.reset_scene()
    metal = ig.mat_metal('AlarmSUS', (.37, .39, .39), roughness=.43, streak=.03, anisotropic=False)
    dark = ig.mat_plastic('DoorSeam', (.02, .025, .023), roughness=.65, bump=0)
    red = ig.mat_plastic('RedLens', (.48, .013, .009), roughness=.24, bump=0)
    white = ig.mat_plastic('CallPointBezel', (.76, .75, .69), roughness=.38, bump=0)
    parts = []
    def box(name, size, at, mat, bevel=.001):
        ob=ig.box(name, size, at, bevel=bevel, segments=2, material=mat); parts.append(ob); return ob
    body = box('cabinet', (.25, .084, .65), (0, .0055, .325), metal, .006)
    box('recess', (.233, .002, .630), (0, -.0375, .325), dark, .003)
    box('front', (.229, .008, .625), (0, -.043, .325), metal, .004)
    mat=ig.mat_image_uv('AlarmPrint', os.path.join(PRINTS, 'FireAlarmFace.png'), roughness=.43, metallic=.78)
    parts.append(ig.image_quad('face_print', (.216, .608), (0, -.0473, .325), mat))
    # 표시등·발신 버튼은 타공 패턴과 달리 정면에서 튀어나오는 실루엣이다.
    for z in (.540, .188):
        parts.append(ig.cylinder('bezel', .036 if z>.3 else .045, .006, (0, -.051, z), rotation=FRONT, segments=32, material=metal if z>.3 else white))
    parts.append(ig.lathe('indicator', [(0,0),(.029,0),(.031,.004),(.028,.018),(.017,.026),(0,.029)],
        segments=32, location=(0,-.054,.540), rotation=FRONT, material=red))
    parts.append(ig.cylinder('manual_button', .033, .005, (0,-.057,.188), rotation=FRONT, segments=32, material=red))
    for z in (.05, .60):
        parts.append(ig.cylinder('fastener', .004, .002, (.103,-.049,z), rotation=FRONT, segments=12, material=metal))
    box('door_latch', (.007,.003,.023), (-.104,-.050,.323), dark)
    parts.append(ig.cylinder('phone_jack', .006, .003, (.068,-.050,.109), rotation=FRONT, segments=16, material=dark))
    return ig.build_asset('SM_FireAlarmPanel','prop',parts,out,collision_parts=[[body]],texture_size=1024,mirror_print_for_ue=True,
        notes='GFS CP-201GS 실물 규격 25×65×9.5cm 기준. 바닥 중심, 앞 -Y. FireAlarmReference_20260916.png의 구성과 대조. 호스함이 아닌 발신기 세트.')


def extinguisher(out):
    ig.reset_scene()
    red=ig.mat_painted_steel('SafetyRed',(.48,.020,.014),roughness=.32,wear=0,bump=.005)
    black=ig.mat_plastic('ValveLever',(.012,.014,.012),roughness=.40,bump=0)
    hosemat=ig.mat_rubber('RubberHose',(.018,.021,.019),roughness=.69)
    brass=ig.mat_metal('GaugeBrass',(.55,.37,.12),roughness=.32,streak=0)
    steel=ig.mat_metal('ValveSteel',(.5,.51,.47),roughness=.3,streak=0)
    white=ig.mat_plastic('Nozzle',(.73,.74,.69),roughness=.45,bump=0)
    parts=[]
    body=ig.lathe('vessel',[(0,0),(.050,0),(.053,.006),(.053,.026),(.062,.036),(.070,.057),(.0725,.077),
        (.0725,.180),(.0735,.185),(.0735,.194),(.0725,.200),(.0725,.294),(.071,.313),(.065,.329),(.053,.343),
        (.035,.354),(.018,.358),(.018,.371),(0,.371)],segments=32,material=red)
    parts.append(body)
    head=ig.box('valve',(.028,.028,.027),(0,0,.383),bevel=.004,segments=2,material=steel)
    parts.append(head)
    # 얇은 철판 레버가 머리 위로 벌어진다. 완제품 최고 높이 450mm.
    for name, path in (('upper',[(0,0,.403),(.040,0,.437),(.082,0,.446)]),('lower',[(0,0,.398),(.045,0,.390),(.084,0,.374)])):
        parts.append(ig.pipe(name,path,radius=.006,resolution=8,corner_radius=.010,material=black))
    parts.append(ig.cylinder('gauge_rim',.014,.008,(0,-.021,.384),rotation=FRONT,segments=32,material=brass))
    gauge=ig.mat_image_uv('PressureDial',os.path.join(PRINTS,'ExtinguisherGauge.png'),roughness=.3)
    parts.append(printed_disc('gauge_face',.012,(0,-.0254,.384),gauge))
    parts.append(ig.torus('safety_pin',.012,.0015,location=(.015,.010,.407),rotation=FRONT,major_segments=24,minor_segments=6,material=steel))
    parts.append(ig.pipe('tamper_tie',[(.016,.010,.412),(.022,.011,.393),(.034,.01,.374)],radius=.001,resolution=6,material=white))
    parts.append(ig.pipe('hose',[(-.014,0,.388),(-.057,0,.380),(-.088,0,.346),(-.094,0,.282),(-.094,0,.129)],
        radius=.008,resolution=12,corner_radius=.025,material=hosemat))
    parts.append(ig.cylinder('hose_coupling',.010,.017,(-.094,0,.120),segments=16,material=steel))
    parts.append(ig.cylinder('nozzle',.012,.065,(-.094,0,.079),segments=16,radius_top=.009,material=white))
    parts.append(ig.box('hose_clip',(.035,.016,.013),(-.084,0,.192),bevel=.002,segments=1,material=red))
    # 원통에 종이를 감은 면이다. 앞에서 읽히는 반원만 만들고 뒤는 도장면을 남긴다.
    vertices=[]; faces=[]; count=40
    for z in (.210,.300):
        for i in range(count+1):
            a=-math.pi/2 + (i/count-.5)*math.radians(245)
            vertices.append((.073*math.cos(a),.073*math.sin(a),z))
    for i in range(count): faces.append((i,i+1,i+count+2,i+count+1))
    me=bpy.data.meshes.new('label_wrap'); me.from_pydata(vertices,[],faces)
    uv=me.uv_layers.new(name='ImageUV')
    for poly in me.polygons:
        for li in poly.loop_indices:
            idx=me.loops[li].vertex_index
            uv.data[li].uv=(idx%(count+1)/count,idx//(count+1))
    label=ig._link(bpy.data.objects.new('label_wrap',me))
    ig.assign_material(label,ig.mat_image_uv('SafetyLabel',os.path.join(PRINTS,'ExtinguisherLabel.png'),roughness=.4))
    parts.append(label)
    return ig.build_asset('SM_FireExtinguisher','prop',parts,out,collision_parts=[[body],[head]],texture_size=2048,mirror_print_for_ue=True,
        notes='국내 3.3kg 제품: 몸통 지름14.5cm, 높이45cm. 실물 사진→ExtinguisherReference_20260916.png→조립 구조. 원점 바닥 중심, 라벨 -Y. 총 질량5.2kg.')


if __name__=='__main__':
    if not os.path.isfile(os.path.join(PRINTS, 'FireAlarmFace.png')):
        raise RuntimeError('python Scripts/build_fire_safety_prints.py를 먼저 실행하세요.')
    out=ig.out_root_from_argv()
    alarm(out)
    extinguisher(out)
