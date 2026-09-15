"""최종 캐릭터에서 목 연결부의 열린 변과 UV 양쪽 색 차이를 검사한다."""
import json
import os
import sys
import bmesh
import bpy
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig

root = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
folder = os.path.join(root, 'Content/SourceArt/Blender/SK_NarinClerk')
bpy.ops.wm.open_mainfile(filepath=os.path.join(folder, 'SK_NarinClerk.blend'))
ob = bpy.data.objects['SK_NarinClerk']


def neck(p):
    return 1.35 < p.z < 1.378 and abs(p.x) < .064 and p.y > -.06


bm = bmesh.new(); bm.from_mesh(ob.data)
open_edges = sum(1 for e in bm.edges if not e.is_manifold and all(neck(v.co) for v in e.verts))
bm.free()
image = bpy.data.images.load(os.path.join(folder, 'SK_NarinClerk_D.png'))
pixels = ig._pixels(image); h, w = pixels.shape[:2]
samples = {}
uv = ob.data.uv_layers.active.data
for loop in ob.data.loops:
    vertex = ob.data.vertices[loop.vertex_index]
    if not neck(vertex.co): continue
    u, v = uv[loop.index].uv
    color = pixels[min(h-1, int(v*h)), min(w-1, int(u*w)), :3]
    samples.setdefault(vertex.index, {0:[], 1:[]})[int(u > .33)].append(color)
deltas = [float(np.max(np.abs(np.mean(s[0], axis=0)-np.mean(s[1], axis=0))))
          for s in samples.values() if s[0] and s[1]]
result = {'open_neck_edges': open_edges, 'shared_seam_vertices': len(deltas),
          'srgb_seam_p95': float(np.percentile(deltas,95)) if deltas else None,
          'triangles': ig.triangle_count(ob)}
path = os.path.join(root,'Saved/ClerkNeckCheck.json')
with open(path,'w',encoding='utf-8') as f: json.dump(result,f,indent=2); f.write('\n')
if open_edges or len(deltas)<8 or result['srgb_seam_p95']>.12:
    raise RuntimeError('목 이음부 검사 실패: '+str(result))
ig.log('CLERK_NECK_CHECK PASS '+json.dumps(result))
