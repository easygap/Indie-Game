"""기존 베이크를 보존하며 ImageUV가 먼저 나간 FBX의 채널 순서를 고친다."""

import json
import os
import sys
from pathlib import Path

import bpy

sys.path.insert(0, os.path.dirname(__file__))
import ig_blender_lib as ig

root = Path(ig.out_root_from_argv())
repaired = []
for path in sorted(root.glob("*/manifest.json")):
    manifest = json.loads(path.read_text(encoding="utf-8-sig"))
    name = manifest["name"]
    blend = path.parent / (name + ".blend")
    if not manifest.get("textures") or not blend.exists() or not name.startswith("SM_"):
        continue
    bpy.ops.wm.open_mainfile(filepath=str(blend))
    mesh = bpy.data.objects.get(name)
    if mesh is None or mesh.type != "MESH" or not ig.ensure_primary_uv(mesh):
        continue
    objects = [mesh] + [o for o in bpy.data.objects if o.name.startswith("UCX_" + name)]
    ig.export_fbx(str(path.parent / manifest["fbx"]), objects)
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    repaired.append(name)
    ig.log("UV_REPAIRED " + name)
(root / "uv_repaired.json").write_text(json.dumps(repaired, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
ig.log(f"UV_REPAIR PASS count={len(repaired)}")
