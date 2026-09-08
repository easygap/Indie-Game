"""축 프로브. Blender의 (x, y, z)가 UE에서 어디로 떨어지는지 실측한다.

긴 팔은 +X, 짧은 혹은 +Y, 기둥은 +Z에 둔다. UE에서 바운드를 읽으면 각 축이
뒤집혔는지 바로 보인다. import_blender_assets.py의 probe 모드가 읽는다.

    blender -b --factory-startup --python Scripts/blender/build_probe_axes.py -- <out_dir>
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402


def main():
    out_root = ig.out_root_from_argv()
    ig.reset_scene()
    paint = ig.mat_plastic("Probe", (0.5, 0.5, 0.5))
    core = ig.box("core", (0.2, 0.2, 0.2), origin="bottom", material=paint)
    arm_x = ig.box("arm_x", (0.6, 0.1, 0.1), location=(0.4, 0.0, 0.1), material=paint)
    bump_y = ig.box("bump_y", (0.1, 0.3, 0.1), location=(0.0, 0.2, 0.1), material=paint)
    post_z = ig.box("post_z", (0.05, 0.05, 0.5), location=(0.0, 0.0, 0.45), material=paint)
    ob = ig.join([core, arm_x, bump_y, post_z], "SM_ProbeAxes")
    ig.uv_box_project(ob)
    out_dir = os.path.join(out_root, "SM_ProbeAxes")
    os.makedirs(out_dir, exist_ok=True)
    ig.export_fbx(os.path.join(out_dir, "SM_ProbeAxes.fbx"), [ob])
    lo, hi = ig.bounds(ob)
    print(f"[PROBE] blender bounds min={tuple(round(v, 3) for v in lo)} max={tuple(round(v, 3) for v in hi)}")


main()
