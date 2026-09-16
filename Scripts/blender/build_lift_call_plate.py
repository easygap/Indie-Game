"""한국 승강기 호출판의 얇은 테·원형 버튼·화살표를 저작 크기로 만든다."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig


def main():
    ig.reset_scene()
    steel = ig.mat_metal("CallPlateSatin", (.49, .51, .50), roughness=.39, streak=.02)
    dark = ig.mat_plastic("ButtonGasket", (.018, .022, .023), roughness=.6, bump=0)
    face = ig.mat_metal("ButtonFace", (.29, .31, .30), roughness=.48, streak=0)
    white = ig.mat_plastic("ButtonMark", (.79, .79, .74), roughness=.55, bump=0)
    plate = ig.box("plate", (.09, .014, .24), (0, .007, .12), bevel=.004, segments=3, material=steel)
    parts = [plate]
    for idx, z in enumerate((.162, .087)):
        for name, radius, depth, y, mat in (("gasket", .022, .002, -.001, dark), ("rim", .020, .002, -.0025, steel), ("face", .0174, .002, -.004, face)):
            parts.append(ig.cylinder(f"{name}_{idx}", radius, depth, (0, y, z), rotation=(math.pi/2, 0, 0), segments=24, material=mat))
        for side in (-1, 1):
            parts.append(ig.box(f"arrow_{idx}_{side}", (.0025, .0005, .012), (side*.0035, -.0053, z+.004),
                                rotation=(0, math.radians(side * (-37 if idx == 0 else 37)), 0), material=white))
        for dot_x, dot_z in ((-.003, -.010), (.002, -.010), (-.003, -.013), (.002, -.013)):
            parts.append(ig.cylinder(f"dot_{idx}_{dot_x}_{dot_z}", .0007, .0007,
                                    (dot_x, -.0056, z+dot_z), rotation=(math.pi/2, 0, 0), segments=8, material=white))
    for z in (.02, .22):
        parts.append(ig.cylinder(f"screw_{z}", .0026, .0008, (0, -.0007, z), rotation=(math.pi/2, 0, 0), segments=12, material=steel))
        parts.append(ig.box(f"screw_slot_{z}", (.003, .0002, .0006), (0, -.0012, z), material=dark))
    return ig.build_asset("SM_LiftCallPlate", "prop", parts, ig.out_root_from_argv(), collision_parts=[[plate]], texture_size=1024,
                          notes="실물·생성 참조: LandingFixtures_20260915.json. 9×1.4×24cm. 앞 -Y, 원점 판 앞면 아래 중앙.")


main()
