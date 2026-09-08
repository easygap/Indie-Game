"""복도·로비 천장 LED 등. SM_CeilingLightRing(스테인리스 테)과 SM_CeilingLightDome(확산 돔).

기준: Content/SourceArt/AI/SheetVillaCorridorFixturesReference.png 우하.
씬은 지름 26 cm 원반 둘(테와 발광 원반)로 천장 등을 그린다. 죽어 가는 등의
깜빡임이 발광 원반의 재질을 통째로 바꾸는 방식이라, 돔을 따로 메시로 두고
씬이 그 슬롯에 예전처럼 M_LightPanel / 어두운 재질을 건다. 그래서 돔은
굽더라도 재질은 씬이 덮어쓴다.

둘 다 원점은 천장에 닿는 윗면 중심이고 아래(-Z)로 늘어진다. 씬은 천장
아랫면 Z에 그대로 놓는다(복도 240, 로비 240).

    blender -b --factory-startup --python Scripts/blender/build_ceiling_light.py -- <out_dir>
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402


def build_ring(out_root):
    ig.reset_scene()
    steel = ig.mat_metal("SatinStainless", (0.72, 0.72, 0.71), roughness=0.42, streak=0.08)
    ring = ig.lathe("ring", [
        (0.0, 0.0), (0.132, 0.0), (0.132, -0.030), (0.116, -0.030), (0.116, -0.020), (0.0, -0.020),
    ], segments=64, material=steel)
    ig.add_bevel(ring, 0.0015, 2)
    ig.apply_modifiers(ring)
    return ig.build_asset(
        "SM_CeilingLightRing", "prop", [ring], out_root,
        collision_parts=[[ring]],
        notes="천장 등 테. 원점은 천장 접촉면 중심, 아래로 3 cm.",
        texture_size=1024)


def build_dome(out_root):
    ig.reset_scene()
    diffuser = ig.mat_plastic("Diffuser", (0.92, 0.92, 0.90), roughness=0.38, bump=0.0)
    dome = ig.lathe("dome", [
        (0.0, -0.020), (0.114, -0.020), (0.114, -0.030), (0.108, -0.040), (0.088, -0.047),
        (0.055, -0.051), (0.0, -0.053),
    ], segments=64, material=diffuser)
    return ig.build_asset(
        "SM_CeilingLightDome", "prop", [dome], out_root,
        collision_parts=[[dome]],
        notes="천장 등 확산 돔. 테 안쪽 지름 22.8. 재질 슬롯은 씬이 M_LightPanel로 덮어쓴다.",
        texture_size=512)


def main():
    out_root = ig.out_root_from_argv()
    only = sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else []
    if not only or "ring" in only:
        build_ring(out_root)
    if not only or "dome" in only:
        build_dome(out_root)


main()
