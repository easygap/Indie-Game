"""PB 계열 실물 사진과 BoothPumpReference_20260916.png를 대조한 소형 펌프."""
import math
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig

ig.reset_scene()
green = ig.mat_painted_steel("PumpPaint", (.014, .26, .20), roughness=.4, wear=0, bump=.001)
iron = ig.mat_painted_steel("CastIron", (.035, .038, .037), roughness=.52, wear=0, bump=.002)
black = ig.mat_plastic("ElectricalBox", (.016, .019, .020), roughness=.42, bump=0)
steel = ig.mat_metal("Bolts", (.54, .55, .53), roughness=.4, streak=0)
rubber = ig.mat_plastic("Cable", (.009, .010, .011), roughness=.6, bump=0)
axis = (0, math.pi/2, 0)
parts = []
def cylinder(name, radius, length, point, mat, rotation=axis, segments=24):
    obj = ig.cylinder(name, radius, length, point, rotation=rotation, segments=segments, material=mat)
    parts.append(obj)
    return obj

motor = cylinder("motor", .065, .16, (.035, 0, .096), green)
cylinder("rear_fan_cover", .071, .033, (.13, 0, .096), iron)
cylinder("motor_front", .072, .018, (-.053, 0, .096), green)
cylinder("cast_pump_case", .084, .047, (-.083, 0, .095), iron)
cylinder("front_case_ring", .073, .005, (-.109, 0, .095), iron)
cylinder("front_hub", .043, .019, (-.12, 0, .095), iron)
# 실제 사진의 모터 방열판은 축을 따라 길게 이어진다.
for i in range(14):
    a = math.tau*i/14
    parts.append(ig.box("cooling_fin", (.145, .005, .019),
        (.035, math.sin(a)*.066, .096+math.cos(a)*.066),
        rotation=(-a, 0, 0), material=green))
for x in (-.043, .096):
    for y in (-.054, .054):
        parts.append(ig.box("foot", (.043, .032, .012), (x, y, .006), bevel=.002, segments=1, material=green))
        parts.append(ig.box("foot_web", (.012, .019, .037), (x, y, .025), material=green))
        cylinder("anchor", .005, .006, (x, y, .015), steel, rotation=(0, 0, 0), segments=6)
parts.append(ig.box("terminal_box", (.148, .112, .048), (.026, 0, .183), bevel=.005, segments=2, material=black))
parts.append(ig.box("terminal_lid", (.153, .117, .004), (.026, 0, .209), bevel=.003, segments=1, material=black))
for x in (-.040, .092):
    for y in (-.046, .046):
        cylinder("lid_screw", .0035, .003, (x, y, .213), steel, rotation=(0, 0, 0), segments=6)
for y, z in ((-.047,.15),(.047,.15),(-.047,.04),(.047,.04)):
    cylinder("case_bolt", .006, .007, (-.111, y, z), steel, segments=6)
# 흡입구는 앞, 토출구는 케이싱 옆이다. 배관은 장면에서 각각 벽과 라이저로 잇는다.
cylinder("inlet_union", .027, .039, (-.153, 0, .095), iron, segments=8)
cylinder("outlet_cast_neck", .023, .081, (-.075, -.085, .095), iron, rotation=(math.pi/2,0,0))
cylinder("outlet_union", .029, .032, (-.075, -.125, .095), iron, rotation=(math.pi/2,0,0), segments=8)
parts.append(ig.pipe("cable", [(.071,-.058,.181),(.093,-.076,.145),(.089,-.075,.099)],
                     .0035, resolution=4, corner_radius=.01, corner_steps=3, material=rubber))
# 뒤쪽 통풍구는 얇은 어두운 선으로 굽고, 원형 덮개의 실루엣만 남긴다.
for i in range(7):
    y = (i-3)*.014
    parts.append(ig.box("rear_vent", (.0003,.004,.052), (.147,y,.096), material=black))
ig.build_asset("SM_BoothPump", "prop", parts, ig.out_root_from_argv(),
    collision_parts=[[motor]], texture_size=1024,
    notes="PB 계열 제품 사진과 BoothPump_20260916.json 참조. 모터축 X, 흡입 -X, 토출 -Y. 원점 바닥 중심, native scale. 관로는 장면 소유.")
