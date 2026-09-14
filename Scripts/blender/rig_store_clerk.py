"""정면 사진에서 다듬은 근무복 인물에 작은 호흡과 고개 움직임을 붙인다."""
import json
import math
import os
import sys
import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ig_blender_lib as ig
from rig_crawler import new_action, key_rot, cyclic, export_skeletal_fbx

out = ig.out_root_from_argv()
name = "SK_NarinClerk"
folder = os.path.join(out, name)
bpy.ops.wm.open_mainfile(filepath=os.path.join(folder, name + ".blend"))
mesh = bpy.data.objects[name]
for obj in list(bpy.data.objects):
    if obj.type == 'ARMATURE':
        bpy.data.objects.remove(obj, do_unlink=True)
for modifier in list(mesh.modifiers):
    if modifier.type == 'ARMATURE': mesh.modifiers.remove(modifier)
mesh.vertex_groups.clear()
for action in list(bpy.data.actions): bpy.data.actions.remove(action)
arm_data = bpy.data.armatures.new(name + "Rig")
arm = bpy.data.objects.new(name + "Rig", arm_data)
bpy.context.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
arm.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for bone_name, start, end, parent in (
    ('Root', (0,0,0), (0,0,.85), None),
    ('Torso', (0,0,.85), (0,0,1.35), 'Root'),
    ('Head', (0,0,1.35), (0,0,1.62), 'Torso'),
):
    bone = arm_data.edit_bones.new(bone_name)
    bone.head = start; bone.tail = end
    if parent: bone.parent = arm_data.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')
groups = {n:mesh.vertex_groups.new(name=n) for n in ('Root','Torso','Head')}
for vertex in mesh.data.vertices:
    z = vertex.co.z
    torso = max(0.,min(1.,(z-.82)/.23))
    head = max(0.,min(1.,(z-1.34)/.12))
    weights = {'Root':1.-torso,'Torso':torso*(1.-head),'Head':head}
    for bone, weight in weights.items():
        if weight > 0: groups[bone].add([vertex.index],weight,'REPLACE')
modifier = mesh.modifiers.new('근무 중 호흡','ARMATURE')
modifier.object = arm
mesh.parent = arm
for bone in arm.pose.bones: bone.rotation_mode = 'XYZ'
bpy.context.scene.render.fps = 30
action = new_action(arm, 'Idle', 180)
for frame in range(0,181,6):
    phase = frame / 180 * math.tau
    key_rot(arm, 'Torso', frame, rx=.22*math.sin(phase*2), rz=.18*math.sin(phase))
    key_rot(arm, 'Head', frame, rx=.7*math.sin(phase), ry=1.7*math.sin(phase))
cyclic(arm, action)
bpy.context.scene.frame_set(0)
ig.ensure_primary_uv(mesh)
export_skeletal_fbx(os.path.join(folder,name+'.fbx'),arm,mesh)
path = os.path.join(folder,'manifest.json')
with open(path,encoding='utf-8') as f: manifest=json.load(f)
manifest['skeletal']=True
manifest['animations']={'Idle':{'frames':180,'fps':30,'loop':True}}
manifest['notes'] += ' 몸통·머리 리그 3개, 6초 호흡 동작. 얼굴 방향은 게임에서 고객 위치를 따른다.'
with open(path,'w',encoding='utf-8') as f:json.dump(manifest,f,ensure_ascii=False,indent=2);f.write('\n')
ig.preview_material_from_bakes(mesh,{role:os.path.join(folder,filename) for role,filename in manifest['textures'].items()})
ig.render_preview(mesh,os.path.join(folder,name+'_front.png'),camera_yaw_deg=180,camera_pitch_deg=8)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder,name+'.blend'))
ig.log('STORE_CLERK_RIG PASS bones=3 animation=Idle frames=180')
