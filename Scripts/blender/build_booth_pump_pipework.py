"""펌프 사진·설치 설명서를 기준으로 조작반 옆을 돌아가는 관리실 배관."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig

ig.reset_scene()
steel = ig.mat_metal("PipeSteel", (.40, .42, .41), roughness=.46, streak=0)
clamp = ig.mat_painted_steel("PipeClamp", (.09, .105, .10), roughness=.52, wear=0, bump=0)

# 원점은 밸브 아래 바닥. 하부 관과 밸브 위치를 유지하고 조작반 아래에서 옆으로 꺾는다.
# 상부 관은 벽에서 15cm, 조작반 가장자리에서 13cm 떨어진다.
riser = ig.pipe("discharge", [(0, .08, .175), (0, 0, .175),
    (0, 0, .72), (0, .30, .72), (0, .30, 2.44)],
    .02, resolution=8, corner_radius=.04, corner_steps=6, material=steel)
parts = [riser]
parts.append(ig.cylinder("suction", .02, .056, (-.122, .20, .175),
    rotation=(0, math.pi/2, 0), segments=16, material=steel))
for z in (1.60, 2.20):
    parts.append(ig.box("wall_support", (.14, .02, .03), (-.075, .30, z),
        bevel=.003, segments=1, material=steel))
    parts.append(ig.cylinder("clamp", .025, .025, (0, .30, z),
        segments=16, material=clamp))

ig.build_asset("SM_BoothPumpPipework", "prop", parts, ig.out_root_from_argv(),
    collision_parts=[[riser]], texture_size=512,
    notes="BoothPump_20260916.json의 사진과 설치 설명서 참조. 배관·지지대를 한 메시로 묶음. 장면 원점 (75,-170,0)cm, 반경 2cm, 상부 Y=-140cm. 조작반 앞을 비우고 천장 내부까지 연결.")
