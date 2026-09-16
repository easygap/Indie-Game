"""실제 현관 카메라 사진과 EntranceCameraReference_20260916.png를 대조한 소품."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig

ig.reset_scene()
case = ig.mat_plastic("CameraGraphite", (.052, .055, .054), roughness=.45, bump=0)
steel = ig.mat_metal("CameraLowerPlate", (.25, .27, .26), roughness=.4, streak=.018)
black = ig.mat_plastic("Recess", (.008, .009, .009), roughness=.5, bump=0)
glass = ig.mat_gloss("LensGlass", (.018, .022, .023), roughness=.13)
white = ig.mat_plastic("LampDiffuser", (.65, .68, .66), roughness=.38, bump=0)
chrome = ig.mat_metal("ButtonRing", (.65, .66, .65), roughness=.26, streak=0)
body = ig.box("case", (.092, .032, .15), (0, -.016, .075), bevel=.006, segments=3, material=case)
parts = [body,
         ig.box("camera_face", (.084, .002, .071), (0, -.032, .110), bevel=.004, segments=3, material=case),
         ig.box("speaker_face", (.084, .001, .066), (0, -.0325, .038), bevel=.003, segments=3, material=steel)]
for x in (-.031, .031):
    parts.append(ig.box("diffuser", (.005, .001, .023), (x, -.0337, .105), bevel=.0024, segments=3, material=white))
for name, radius, depth, y, z, material in (
    ("lens_recess", .014, .001, -.0337, .105, black),
    ("lens_glass", .0065, .0004, -.0344, .105, glass),
    ("button_rim", .011, .001, -.0337, .022, chrome),
    ("call_button", .009, .0006, -.0345, .022, case)):
    parts.append(ig.cylinder(name, radius, depth, (0, y, z), rotation=(math.pi/2, 0, 0), segments=24, material=material))
# 작은 타공은 검은 안쪽 면만 굽는다. 충돌과 먼 거리 LOD에는 구멍이 필요 없다.
for row in range(6):
    for col in range(9):
        parts.append(ig.cylinder("speaker_hole", .0009, .00015,
                                (-.0192+col*.0048, -.0331, .047+row*.0045),
                                rotation=(math.pi/2, 0, 0), segments=8, material=black))
ig.build_asset("SM_EntranceCamera", "prop", parts, ig.out_root_from_argv(),
               collision_parts=[[body]], texture_size=512,
               notes="실물 DRC-4Y와 EntranceCamera_20260916.json 참조. 9.2×3.5×15cm, 앞 -Y, 원점 뒷면 아래 중앙.")
