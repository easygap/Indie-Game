"""국내 펌프 제어반 실물과 PumpPanelReference_20260916.png를 대조한 함체·선택 손잡이."""
import math
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig


def build_panel(out):
    ig.reset_scene()
    paint = ig.mat_painted_steel('PanelPowderCoat', (.67, .68, .65), roughness=.46, wear=0, bump=0)
    metal = ig.mat_metal('NickelBezel', (.54, .55, .53), roughness=.32, streak=0)
    dark = ig.mat_plastic('DoorGasket', (.017, .019, .018), roughness=.65, bump=0)
    parts = []
    def box(name,size,at,mat,bevel=.001):
        ob=ig.box(name,size,at,bevel=bevel,segments=2,material=mat); parts.append(ob); return ob
    body=box('enclosure',(.34,.072,.52),(0,.004,0),paint,.005)
    box('door_gasket',(.327,.002,.507),(0,-.033,0),dark,.004)
    box('door',(.328,.008,.508),(0,-.038,0),paint,.005)
    for z in (-.18,.18):
        parts.append(ig.cylinder('hinge',.007,.042,(-.167,-.032,z),segments=12,material=metal))
    # 판넬의 한글은 실물 규격에 맞춘 인쇄 원본을 굽는다.
    labels=os.path.join(os.path.dirname(__file__),'..','..','Content','SourceArt','UtilityPrints','PumpPanelFace.png')
    mat=ig.mat_image_uv('PanelLettering',labels,roughness=.6)
    parts.append(ig.image_quad('printed_door',(.308,.485),(0,-.0422,0),mat))
    for x in (-.082,0,.082):
        parts.append(ig.cylinder('lamp_bezel',.014,.006,(x,-.046,.071),rotation=(math.pi/2,0,0),segments=20,material=metal))
        parts.append(ig.cylinder('lamp_socket',.0112,.003,(x,-.050,.071),rotation=(math.pi/2,0,0),segments=20,material=dark))
    parts.append(ig.cylinder('selector_bezel',.020,.006,(0,-.046,-.077),rotation=(math.pi/2,0,0),segments=24,material=metal))
    parts.append(ig.cylinder('latch',.010,.009,(.133,-.046,-.014),rotation=(math.pi/2,0,0),segments=16,material=metal))
    box('latch_slot',(.013,.001,.002),(.133,-.051,-.014),dark,0)
    for x in (-.07,.07):
        parts.append(ig.cylinder('cable_gland',.012,.021,(x,.006,-.270),segments=8,material=metal))
    return ig.build_asset('SM_PumpControlPanel','prop',parts,out,collision_parts=[[body]],texture_size=2048,mirror_print_for_ue=True,
        notes='34×8×54cm. 중심 원점, 앞 -Y. 회전 스위치와 표시등 렌즈는 장면에서 상태에 맞춰 배치한다. 참조: PumpPanelReference_20260916.json.')


def build_selector(out):
    ig.reset_scene()
    black=ig.mat_plastic('SelectorBlack',(.016,.018,.019),roughness=.36,bump=0)
    white=ig.mat_plastic('SelectorPointer',(.78,.77,.72),roughness=.5,bump=0)
    base=ig.cylinder('selector_disc',.016,.008,(0,0,0),rotation=(math.pi/2,0,0),segments=20,material=black)
    parts=[base,ig.box('selector_grip',(.009,.021,.036),(0,-.011,0),bevel=.002,segments=2,material=black),
        ig.box('pointer',(.002,.001,.012),(0,-.022,.009),material=white)]
    return ig.build_asset('SM_PumpSelector','prop',parts,out,collision_parts=[],texture_size=512,
        notes='회전축 원점, 앞 -Y. 수직은 정지, 반시계 45도는 수동. 조작반과 별도 메시.')


if __name__=='__main__':
    out=ig.out_root_from_argv()
    build_panel(out)
    build_selector(out)
