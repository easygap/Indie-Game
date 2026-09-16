"""실물 참조와 RoofTank2000LStudy_v1을 따른 저수조·배관·벽 손잡이."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig

OUT = ig.out_root_from_argv()
PRINTS = os.path.join(os.path.dirname(__file__), '../../Content/SourceArt/UtilityPrints')
FRONT = (math.pi/2, 0, 0)


def tank():
    ig.reset_scene()
    metal = ig.mat_metal('TankSUS', (.47, .49, .48), roughness=.40, streak=.025)
    # 큰 얼룩 노이즈가 반사에 구름처럼 보이지 않게 하고 가는 세로 결만 남긴다.
    for node in metal.node_tree.nodes:
        if node.bl_idname == 'ShaderNodeMapping':
            node.inputs['Scale'].default_value=(60,60,1)
        elif node.bl_idname == 'ShaderNodeMapRange' and node.inputs['To Min'].default_value < 0:
            node.inputs['To Min'].default_value=-.006
            node.inputs['To Max'].default_value=.008
        elif node.bl_idname == 'ShaderNodeBump':
            node.inputs['Distance'].default_value=.00015
    seam = ig.mat_metal('TankWeld', (.35, .37, .35), roughness=.52, streak=0)
    body = ig.lathe('tank', [(0,0),(.63,0),(.65,.014),(.65,1.60),
        (.642,1.616),(.20,1.71),(.20,1.738),(0,1.738)], segments=64, material=metal)
    parts = [body]
    for z in (.02, .48, 1.04, 1.596):
        parts.append(ig.torus('rolled_bead', .650, .007, (0,0,z), major_segments=64, minor_segments=8, material=metal))
    parts.append(ig.cylinder('access_lid', .216, .026, (0,0,1.749), segments=48, bevel=.003, material=metal))
    parts.append(ig.pipe('lid_handle', [(-.07,0,1.765),(-.07,0,1.8),(.07,0,1.8),(.07,0,1.765)],
        .008, resolution=6, corner_radius=.012, material=metal))
    # 뒤쪽 세로 용접선. 폭 2mm만 남겨 띠처럼 보이지 않게 한다.
    parts.append(ig.pipe('weld', [(0,-.6505,.04),(0,-.6505,1.59)], .0012, resolution=4, material=seam))
    # 넘침관은 아래를 향한다. 게임 배관과 같은 북쪽(+Y)에 둔다.
    parts.append(ig.pipe('overflow', [(-.42,.46,1.49),(-.42,.72,1.49),(-.42,.77,1.42)],
        .025, resolution=12, corner_radius=.05, caps=False, material=metal))
    parts.append(ig.lathe('overflow_lip',[(.023,0),(.023,.025),(.025,.025),(.025,0),(.023,0)],
        segments=24,location=(-.42,.77,1.42),material=metal))
    # 수위계: 불투명 보호 홈, 얇은 관, 위쪽에 떠 있는 지시자. 바깥에서 만수를 확인한다.
    dark = ig.mat_plastic('GaugeChannel', (.028,.032,.028), roughness=.52, bump=0)
    water = ig.mat_plastic('GaugeWater', (.08,.18,.19), roughness=.24, bump=0)
    red = ig.mat_plastic('GaugeFloat', (.35,.035,.02), roughness=.42, bump=0)
    parts.append(ig.box('gauge_channel', (.048,.014,1.35), (.43,.645,.84), bevel=.003, material=dark))
    parts.append(ig.cylinder('level_tube', .010, 1.25, (.43,.659,.85), segments=12, material=water))
    parts.append(ig.cylinder('float', .011, .028, (.43,.660,1.47), segments=12, material=red))
    for z in (.17,1.51):
        parts.append(ig.pipe('gauge_mount',[(.43,.488,z),(.43,.664,z)],.012,resolution=6,material=metal))
    ig.build_asset('SM_RoofTank2000L','prop',parts,OUT,collision_parts=[],texture_size=1024,preview_yaw=180,sharp_angle=40,
        notes='원통 지름 130cm·몸통 높이 160cm. 정상 수위 150cm에서 약 1991L. 북쪽 +Y가 표찰·배관 쪽. 원점은 탱크 바닥. 실물 FRP 비례와 STS 제작 사진, AI 3면 참조는 RoofUtility_20260916.json. 기단과 충돌은 런타임 분리.')


def pipework():
    ig.reset_scene()
    steel = ig.mat_metal('GalvanizedPipe', (.37,.40,.39), roughness=.51, streak=.018)
    brass = ig.mat_metal('ValveBronze', (.31,.20,.085), roughness=.48, streak=0)
    rubber = ig.mat_plastic('FlangeGasket', (.02,.025,.02), roughness=.7, bump=0)
    parts=[]
    # 원점은 옥상 바닥 (0,0,1200)cm. 접속부는 탱크 (0,60,1220)와 공유한다.
    paths = [
        ('tank_outlet',[(0,1.21,.36),(0,1.40,.36),(-.62,1.40,.36),(-.62,1.40,1.40)],.027),
        ('drain_outlet',[(-.62,1.40,1.40),(-1.04,1.40,1.40),(-1.04,1.40,-.08)],.027),
        ('bypass_inlet',[(1.05,1.40,-.08),(1.05,1.40,.73),(.72,1.40,.73),(.72,1.40,1.40)],.021),
        ('bypass_return',[(.72,1.40,1.40),(.92,1.40,1.40),(.92,.60,1.40),(.63,.60,1.40)],.021)]
    for name, points, radius in paths:
        parts.append(ig.pipe(name,points,radius,resolution=12,corner_radius=.07,corner_steps=5,material=steel))
    for x in (-.62,.72):
        parts.append(ig.cylinder('bonnet',.052,.11,(x,1.40,1.40),segments=16,bevel=.004,material=brass))
        parts.append(ig.cylinder('stem_housing',.025,.13,(x,1.48,1.40),rotation=FRONT,segments=16,material=brass))
        parts.append(ig.cylinder('stem',.009,.12,(x,1.60,1.40),rotation=FRONT,segments=12,material=steel))
        for z in (.98,1.28):
            parts.append(ig.cylinder('union',.038,.035,(x,1.40,z),segments=8,bevel=.002,material=steel))
        # 지지대는 지면까지 이어진다. 손잡이 아래 글씨를 가리지 않는다.
        parts.append(ig.box('support',(.028,.03,1.13),(x,1.33,.57),material=steel))
        parts.append(ig.box('foot',(.13,.11,.012),(x,1.33,.006),bevel=.002,material=steel))
    # 탱크 접속 플랜지. 관통 구멍은 연결 관에 가려지고 가장자리·볼트가 실루엣을 만든다.
    for y in (1.255,1.28):
        parts.append(ig.cylinder('flange',.056,.012,(0,y,.36),rotation=FRONT,segments=24,bevel=.002,material=steel))
    parts.append(ig.cylinder('gasket',.05,.006,(0,1.268,.36),rotation=FRONT,segments=24,material=rubber))
    for a in range(4):
        theta=math.tau*(a+.5)/4
        parts.append(ig.cylinder('bolt',.007,.035,(math.cos(theta)*.043,1.274,.36+math.sin(theta)*.043),rotation=FRONT,segments=6,material=steel))
    ig.build_asset('SM_RoofCleaningPipework','prop',parts,OUT,collision_parts=[],texture_size=1024,preview_yaw=180,sharp_angle=40,
        notes='배수·우회 회로를 하나로 구운 메시. 손잡이 축은 (-62,166,140), (72,166,140)cm. 탱크 배수구에서 왼쪽 배수관으로, 우회관은 별도 오른쪽 입수관에서 탱크로 연결. 배수관과 급수관은 지면 안으로 이어짐. 옥상바닥 원점.')


def handrail():
    ig.reset_scene()
    steel=ig.mat_metal('RailSUS',(.32,.34,.33),roughness=.44,streak=.02)
    parts=[ig.pipe('handrail',[(-.06,-.14,.78),(0,-.14,.78),(0,0,.85),
        (0,4.18,3.68),(0,4.30,3.68),(-.06,4.30,3.68)],.019,resolution=12,corner_radius=.055,material=steel)]
    for y in (.16,1.43,2.70,4.03):
        z=.85+y*(2.83/4.18)
        parts.append(ig.pipe('bracket',[(-.075,y,z-.10),(0,y,z-.10),(0,y,z)],.008,resolution=6,corner_radius=.015,material=steel))
        parts.append(ig.cylinder('wall_plate',.034,.006,(-.075,y,z-.10),rotation=(0,math.pi/2,0),segments=16,material=steel))
    ig.build_asset('SM_RoofStairHandrail','prop',parts,OUT,collision_parts=[],texture_size=512,sharp_angle=40,
        notes='벽 손잡이 지름38mm. 시작 위치 (-317,-220,918)cm. 벽 쪽 원형 브래킷 4개. 보행 캡슐에 걸리는 별도 충돌 없음. 현재 계단 상승률에 맞춘 높이.')


def plates():
    for name, image, width, height in [('SM_RoofTankPlate','RoofTankPlate',.24,.14),
            ('SM_RoofDrainPlate','RoofDrainPlate',.16,.08),('SM_RoofBypassPlate','RoofBypassPlate',.16,.08)]:
        ig.reset_scene()
        metal=ig.mat_image_uv('EtchedPlate',os.path.join(PRINTS,image+'.png'),roughness=.48,metallic=.75)
        back=ig.mat_metal('BlankBack',(.36,.38,.36),roughness=.48,streak=0)
        p=ig.image_quad('plate',(width,height),(0,0,0),metal,thickness=.001)
        p.data.materials.append(back)
        for f in p.data.polygons:
            if f.normal.y>-.5:f.material_index=1
        ig.build_asset(name,'prop',[p],OUT,collision_parts=[],texture_size=512,mirror_print_for_ue=True,origin='center',
            notes='앞 -Y. 읽을 수 있는 한글과 수치를 폰트로 확정한 표찰. 금속판 1mm. 런타임 부착면과 읽기 판정 별도.')


tank()
pipework()
handrail()
plates()
