"""실물 조율 렌치의 목재 손잡이, 연속된 강철 축, 열린 별 소켓."""
import math
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import ig_blender_lib as ig

OUT = ig.out_root_from_argv()
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../Content/SourceArt'))
ig.reset_scene()
wood = ig.mat_image_uv('TurnedHardwood', os.path.join(ROOT, 'Photo/WoodDark/Wood067_2K-JPG_Color.jpg'), roughness=.47)
steel = ig.mat_metal('NickelSteel', (.62, .64, .63), roughness=.28, streak=.008)
handle = ig.lathe('wood_handle', [(0,0),(.006,.0005),(.012,.003),(.0147,.010),
    (.015,.022),(.0148,.058),(.014,.104),(.0124,.149),(.012,.165),(0,.165)], segments=32, material=wood)
# 목재 사진의 가로 결을 손잡이의 길이 방향으로 감는다.
uv = handle.data.uv_layers.new(name='ImageUV')
for face in handle.data.polygons:
    angles = [math.atan2(handle.data.vertices[handle.data.loops[i].vertex_index].co.y,
                         handle.data.vertices[handle.data.loops[i].vertex_index].co.x) / math.tau for i in face.loop_indices]
    if max(angles)-min(angles) > .5:
        angles = [a+1 if a<0 else a for a in angles]
    for li,a in zip(face.loop_indices,angles):
        p = handle.data.vertices[handle.data.loops[li].vertex_index].co
        uv.data[li].uv = (p.z*4.5, (a+.5)*.75+.12)
parts = [handle]
parts.append(ig.lathe('ferrule',[(0,.161),(.0122,.161),(.0127,.162),(.0127,.174),(.0118,.175),(0,.175)],segments=32,material=steel))
# 축과 굽은 목을 한 곡선으로 만든다. 원형 단면이 경로와 평행하게 누워
# 사라지던 GeometryScript 스윕 대신 실제 두께를 가진 Blender 관을 쓴다.
shaft = ig.pipe('continuous_shaft',[(0,0,.171),(0,0,.270),(.004,0,.283),(.013,0,.290),(.024,0,.291)],
                .0048, resolution=12, corner_radius=.009, corner_steps=5, material=steel)
parts.append(shaft)
socket = ig.lathe('star_socket',[(0,0),(.0062,0),(.0067,.001),(.0067,.023),(.0062,.024),(0,.024)],segments=32,material=steel)
# 여덟 모서리를 가진 별 모양 구멍은 그림이 아닌 관통하지 않는 실제 홈이다.
loop = [(math.cos(i*math.tau/16)*(.0043 if i%2==0 else .00335),
         math.sin(i*math.tau/16)*(.0043 if i%2==0 else .00335)) for i in range(16)]
verts = [(x,y,z) for z in (.007,.03) for x,y in loop]
faces = [list(reversed(range(16))),list(range(16,32))] + [(i,(i+1)%16,(i+1)%16+16,i+16) for i in range(16)]
data = bpy.data.meshes.new('socket_recess');data.from_pydata(verts,[],faces);data.update()
cutter = ig._link(bpy.data.objects.new('socket_recess',data))
ig.boolean(socket,cutter)
socket.rotation_euler[1] = math.pi/2-math.radians(5)
socket.location = (.019,0,.290)
parts.append(socket)
for ob in parts:
    for f in ob.data.polygons: f.use_smooth = True
ig.build_asset('SM_TuningHammer','prop',parts,OUT,
    collision_parts=[[handle],[shaft,socket]], texture_size=1024, sharp_angle=35,
    preview_yaw=35, notes='Howard A-5A 실물 사진과 TuningLeverStudy_20260916 참조. 목재 손잡이16.5cm·니켈 축·별 소켓. 장축 Z, 손잡이 반지름1.5cm. 단일 구운 재질, 연결부도 실체 메시.')
