"""실물 LS 차단기와 CircuitPanelReference_20260917 시트를 대조한 분전반.

외함 앞은 -Y. 인쇄 UV만 보정해서 힌지·회로의 좌우 배치는 보존한다.
토글은 축을 원점으로 따로 내보내며, 배선과 단자는 안전 덮개 안에 있다.
"""
import math
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig

PRINTS = os.path.join(os.path.dirname(__file__), '..', '..', 'Content', 'SourceArt', 'UtilityPrints')

def build_panel(out):
    ig.reset_scene()
    paint = ig.mat_painted_steel('분체도장', (.50,.53,.50), roughness=.5, wear=0, bump=0)
    ivory = ig.mat_plastic('사출전면', (.70,.70,.65), roughness=.54, bump=0)
    gray = ig.mat_plastic('절연몸체', (.19,.21,.20), roughness=.54, bump=0)
    dark = ig.mat_plastic('토글안쪽', (.018,.023,.022), roughness=.62, bump=0)
    steel = ig.mat_metal('나사', (.55,.56,.54), roughness=.37, streak=0)
    yellow = ig.mat_plastic('시험버튼', (.71,.56,.07), roughness=.4, bump=0)
    parts = []
    prints = []
    def box(name, size, at, mat, bevel=.001):
        ob=ig.box(name,size,at,bevel=bevel,segments=2,material=mat); parts.append(ob); return ob
    def screw(x,y,z):
        parts.append(ig.cylinder('덮개나사',.0023,.0013,(x,y,z),rotation=(math.pi/2,0,0),segments=12,material=steel))
        box('나사홈',(.0032,.0002,.00055),(x,y-.0008,z),dark,0)
    def print_at(name, size, at):
        mat=ig.mat_image_uv(name,os.path.join(PRINTS,f'Circuit{name}.png'),roughness=.64)
        prints.append(ig.image_quad(name,size,at,mat,thickness=.0002))

    body=box('외함',(.36,.057,.54),(0,0,0),paint,.003)
    box('문틈',(.335,.001,.514),(0,-.029,0),dark,.002)
    cover=box('안전덮개',(.331,.003,.510),(0,-.031,0),paint,.002)
    for z in (-.18,.18):
        parts.append(ig.cylinder('힌지',.0035,.036,(.167,-.032,z),segments=12,material=steel))
    parts.append(ig.cylinder('잠금쇠',.006,.005,(-.146,-.034,-.013),rotation=(math.pi/2,0,0),segments=16,material=steel))
    box('잠금쇠홈',(.0008,.0003,.007),(-.146,-.0368,-.013),dark,0)
    print_at('Title',(.066,.016),(0,-.033,.239))

    for row, z in enumerate((.14,-.01,-.16)):
        for col, x in enumerate((.066,-.067)):
            spare = row == 2 and col == 1
            # 단자부는 금속 덮개에 가려지고 조작 전면만 14mm 나온다.
            ig.boolean(cover,ig.box('개구부',(.053,.02,.101),(x,-.031,z)),'DIFFERENCE')
            box('절연틀',(.052,.012,.100),(x,-.036,z),gray,.0013)
            face=box('예비덮개' if spare else '차단기전면',(.050,.012,.096),(x,-.041,z),ivory,.0015)
            if not spare:
                ig.boolean(face,ig.box('토글구멍',(.014,.026,.050),(x,-.043,z)),'DIFFERENCE')
                box('토글홈바닥',(.014,.001,.050),(x,-.037,z),dark,0)
                print_at('On',(.006,.006),(x,-.0472,z+.018))
                print_at('Off',(.006,.006),(x,-.0472,z-.018))
                # 정격은 토글 양쪽의 실제 여백에만 넣는다.
                print_at('Rating',(.015,.015),(x+.016,-.0472,z+.004))
                parts.append(ig.cylinder('시험버튼',.0033,.002,(x-.016,-.048,z-.009),rotation=(math.pi/2,0,0),segments=16,material=yellow))
                box('버튼표시',(.002,.0002,.0005),(x-.016,-.0473,z-.016),gray,0)
            for dz in (-.039,.039): screw(x,-.048,z+dz)
            label = (('401','402'),('403','Common'),('Blank','Spare'))[row][col]
            print_at(label,(.052,.012),(x,-.033,z-.061))

    ig.build_asset('SM_LobbyCircuitPanel','prop',parts,out,collision_parts=[[body]],texture_size=1024,
        origin='center',notes='36×54cm 분전반. 중심 원점, 앞 -Y. 다섯 회로+예비 덮개. 토글 축 로컬 X +6.6/-6.7cm, Y -4.8cm, Z +14/-1/-16cm. 실제 사진과 생성 참고 시트 대조.')
    return ig.build_asset('SM_CircuitPanelPrints','prop',prints,out,collision_parts=[],texture_size=1024,
        mirror_print_uv=True,origin='center',notes='분전반과 같은 원점. 한글 이름표·정격·I/O 표기만 모은 1K 아틀라스. 충돌·그림자 없음. 전체 함체에 굽지 않아 근접 글자 해상도를 확보한다.')

def build_toggle(out):
    ig.reset_scene()
    black=ig.mat_plastic('토글사출',(.12,.14,.13),roughness=.46,bump=0)
    parts=[ig.cylinder('회전축',.0036,.012,(0,0,0),rotation=(0,math.pi/2,0),segments=12,material=black),
        ig.box('토글몸체',(.012,.010,.014),(0,-.005,0),bevel=.0013,segments=2,material=black)]
    for z in (-.004,0,.004):
        parts.append(ig.box('손잡이요철',(.011,.0012,.0012),(0,-.0103,z),bevel=.0003,segments=1,material=black))
    return ig.build_asset('SM_CircuitToggle','prop',parts,out,collision_parts=[],texture_size=256,origin='hinge',
        notes='1.2cm 폭. 축 중심 원점. 앞 -Y, X축으로 ±32도 회전. 함체와 별도 메시, 이동 없이 ON/OFF를 표현한다.')

def build_closed_cabinet(out):
    ig.reset_scene()
    paint=ig.mat_painted_steel('복도함체도장',(.50,.53,.50),roughness=.5,wear=0,bump=0)
    dark=ig.mat_plastic('닫힌문틈',(.018,.023,.022),roughness=.65,bump=0)
    steel=ig.mat_metal('캠잠금쇠',(.55,.56,.54),roughness=.37,streak=0)
    body=ig.box('매입함체',(.34,.060,.50),(0,0,0),bevel=.002,segments=2,material=paint)
    parts=[body,ig.box('문틈',(.317,.001,.477),(0,-.0305,0),bevel=.001,segments=2,material=dark),
        ig.box('철제문짝',(.313,.003,.473),(0,-.0325,0),bevel=.001,segments=2,material=paint)]
    for z in (-.175,.175):
        parts.append(ig.cylinder('핀힌지',.0032,.030,(.157,-.033,z),segments=12,material=steel))
    parts.append(ig.cylinder('잠금쇠테',.008,.0035,(-.134,-.036,0),rotation=(math.pi/2,0,0),segments=20,material=steel))
    parts.append(ig.box('드라이버홈',(.0012,.0003,.010),(-.134,-.038,0),bevel=.0003,segments=1,material=dark))
    label=ig.mat_image_uv('분전반표시',os.path.join(PRINTS,'CircuitTitle.png'),roughness=.65)
    prints=[ig.image_quad('이름표',(.075,.019),(0,-.0342,.197),label,thickness=.0002)]
    ig.build_asset('SM_CorridorCircuitCabinet','prop',parts,out,collision_parts=[],texture_size=512,
        origin='center',notes='34×50cm 매입형 분전함. 중심 원점, 앞 -Y. 앞 문짝은 Y -3.4cm, 힌지·잠금쇠는 최대 -3.82cm. 외함은 벽에 매입하고 앞면만 8mm 나온다. 실물과 CorridorCabinetReference_20260917 참조.')
    return ig.build_asset('SM_CorridorCircuitPrint','prop',prints,out,collision_parts=[],texture_size=256,
        mirror_print_uv=True,origin='center',notes='복도 분전함과 같은 원점. 7.5×1.9cm 이름표만 별도 256px로 구워 한글 획을 보존한다. 충돌·그림자 없음.')

if __name__ == '__main__':
    out=ig.out_root_from_argv()
    build_panel(out)
    build_toggle(out)
    build_closed_cabinet(out)
