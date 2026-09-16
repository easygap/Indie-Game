"""실물 우편함과 LandingFixturesReference_20260915.png에 맞춘 3열 3단 우편함.

너비 96cm, 높이 58cm, 벽에서 10cm. 원점은 뒷면의 아래 중앙, 앞은 -Y다.
"""
import math
import os
import sys
import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig


def main():
    ig.reset_scene()
    steel = ig.mat_metal("MailboxSatin", (.46, .48, .47), roughness=.43, streak=.025)
    dark = ig.mat_plastic("SlotInterior", (.014, .017, .018), roughness=.68, bump=0)
    parts = []
    font = bpy.data.fonts.load("C:/Windows/Fonts/malgunbd.ttf")
    back = ig.box("back", (.96, .092, .58), (0, -.046, .29), bevel=.002, segments=1, material=steel)
    parts.append(back)
    # 낱개 상자 아홉을 겹쳐 만들지 않아 옆면에 불필요한 턱이 생기지 않는다.
    for row in range(3):
        for col in range(3):
            x, z = -.316 + col * .316, .098 + row * .192
            tag = f"{row}_{col}"
            gap = ig.box(f"gap_{tag}", (.312, .001, .188), (x, -.0925, z), material=dark)
            door = ig.box(f"door_{tag}", (.305, .004, .181), (x, -.095, z), bevel=.001, segments=1, material=steel)
            slot = ig.box(f"slot_cut_{tag}", (.20, .025, .019), (x-.015, -.095, z-.056), bevel=.008, segments=3)
            ig.boolean(door, slot, "DIFFERENCE")
            parts.extend([gap, door])
            parts.append(ig.box(f"flap_{tag}", (.299, .002, .033), (x, -.098, z+.069), bevel=.0008, segments=1, material=steel))
            parts.append(ig.cylinder(f"lock_{tag}", .009, .003, (x+.125, -.099, z-.057), rotation=(math.pi/2, 0, 0), segments=12, material=steel))
            parts.append(ig.box(f"keyway_{tag}", (.002, .0005, .008), (x+.125, -.1006, z-.057), material=dark))
            # 호수는 얇은 인쇄 면으로 구워, 가까이서도 임의의 기호처럼 보이지 않게 한다.
            curve = bpy.data.curves.new(f"number_{tag}", "FONT")
            curve.body = str((row + 2) * 100 + col + 1)
            curve.font = font
            curve.size = .027
            curve.align_x = "CENTER"
            curve.align_y = "CENTER"
            curve.resolution_u = 2
            label = bpy.data.objects.new(f"number_{tag}", curve)
            bpy.context.collection.objects.link(label)
            label.location = (x, -.0971, z+.006)
            label.rotation_euler = (math.pi/2, 0, 0)
            bpy.ops.object.select_all(action="DESELECT")
            bpy.context.view_layer.objects.active = label
            label.select_set(True)
            bpy.ops.object.convert(target="MESH")
            label.select_set(False)
            ig.assign_material(label, dark)
            parts.append(label)
    return ig.build_asset("SM_MailboxUnit", "prop", parts, ig.out_root_from_argv(), collision_parts=[[back]], texture_size=1024,
                          mirror_print_for_ue=True,
                          notes="실물·생성 참조: LandingFixtures_20260915.json. 96×10×58cm, 앞 -Y, 원점 뒷면 아래 중앙.")


main()
