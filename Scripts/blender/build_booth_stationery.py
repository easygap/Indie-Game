"""실물 장부와 생성 참고도에 맞춘 관리실 장부·접수철·연필."""
import math
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import ig_blender_lib as ig

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../Content/SourceArt'))
OUT = ig.out_root_from_argv()

def ledger():
    ig.reset_scene()
    green = ig.mat_plastic('GreenBookcloth', (.025,.065,.050), roughness=.88, bump=.004, grain_scale=650)
    cream = ig.mat_plastic('CreamBinding', (.58,.54,.44), roughness=.91, bump=.002, grain_scale=800)
    paper = ig.mat_plastic('IvoryPages', (.69,.66,.57), roughness=.94, bump=0)
    p = [ig.box('page_block', (.205,.288,.023), (0,0,0), bevel=.001, material=paper)]
    for z in (-.013,.013):
        p.append(ig.box('cover',(.220,.307,.002),(0,0,z),bevel=.001,material=green))
        for y in (-.134,.134):
            verts=[(-.109,y+(-.019 if y<0 else .019),z+.0011),
                   (-.078,y+(-.019 if y<0 else .019),z+.0011),(-.109,y-(-.010 if y<0 else .010),z+.0011)]
            data=bpy.data.meshes.new('corner');data.from_pydata(verts,[],[(0,1,2) if y<0 else (2,1,0)]);data.update()
            corner=ig._link(bpy.data.objects.new('cloth_corner',data));ig.assign_material(corner,cream);p.append(corner)
    # 실제 화면에서 왼쪽은 +X. 천 책등은 이쪽 한 면만 감싼다.
    p.append(ig.box('cloth_spine',(.025,.307,.0286),(.100,0,0),bevel=.0015,material=cream))
    for z in (-.009,-.005,0,.005,.009):
        p.append(ig.box('page_edge',(.00025,.286,.00012),(-.1026,0,z),material=cream))
    ink=ig.mat_image_uv('CoverLabel',os.path.join(ROOT,'UtilityPrints/ComplaintLedgerCover.png'),roughness=.91)
    p.append(ig.image_quad('cover_label',(.123,.0615),(-.011,-.022,.01415),ink,rotation=(-math.pi/2,0,0),thickness=.0001))
    ig.build_asset('SM_ComplaintLedger','prop',p,OUT,collision_parts=[[p[0]]],texture_size=2048,
        origin='center',mirror_print_uv=True,notes='근영사 A4 장부 실물과 BoothStationeryStudy_20260917 참조. 22×30.7×2.8cm, 왼쪽 천 책등·모서리 보강·종이 단면. 인쇄 UV만 보정.')

def pad():
    ig.reset_scene()
    paper=ig.mat_plastic('ReceiptPaper',(.78,.75,.67),roughness=.94,bump=.001,grain_scale=1100)
    card=ig.mat_plastic('PadBacking',(.26,.25,.22),roughness=.96,bump=.002)
    steel=ig.mat_metal('OldStaples',(.24,.25,.24),roughness=.57,streak=0)
    p=[ig.box('backing',(.213,.300,.001),(0,0,.0005),bevel=.0002,material=card),
       ig.box('thin_pages',(.210,.297,.0044),(0,0,.0032),bevel=.0002,material=paper)]
    # 뜯긴 윗장의 접착부. 불규칙한 가장자리를 두께 있는 메시로 남긴다.
    lower=[(-.105+i*.0105,.127+(.0008,-.0011,.0002,.0013,-.0005)[i%5],.00565) for i in range(21)]
    verts=lower+[(.105,.1485,.00565),(-.105,.1485,.00565)]
    data=bpy.data.meshes.new('torn_stub');data.from_pydata(verts,[],[list(range(len(verts)))]);data.update()
    stub=ig._link(bpy.data.objects.new('torn_stub',data));ig.assign_material(stub,paper);p.append(stub)
    for x in (-.063,.063):
        p.append(ig.pipe('staple',[(x-.002,.138,.0055),(x-.002,.138,.0061),(x+.002,.138,.0061),(x+.002,.138,.0055)],.00023,resolution=6,material=steel))
    for z in (.0015,.0025,.0036,.0045):
        p.append(ig.box('paper_edge',(.209,.00012,.00007),(0,-.1485,z),material=card))
    ig.build_asset('SM_ComplaintImpressionPad','prop',p,OUT,collision_parts=[[p[1]]],texture_size=512,
        origin='center',notes='A4 밑장 21×29.7cm, 회색 받침·얇은 종이·뜯긴 윗장·철심. 표면 기록은 별도 동적 평면에서 복원하며 종이 전체를 두꺼운 책으로 만들지 않는다.')

def pencil_parts():
    yellow=ig.mat_plastic('PencilLacquer',(.74,.42,.035),roughness=.49,bump=0)
    dark=ig.mat_plastic('PencilBlack',(.014,.016,.014),roughness=.48,bump=0)
    red=ig.mat_plastic('EndCap',(.40,.025,.018),roughness=.5,bump=0)
    wood=ig.mat_plastic('SharpenedWood',(.49,.31,.15),roughness=.82,bump=.002)
    graphite=ig.mat_plastic('Graphite',(.045,.047,.049),roughness=.34,bump=0)
    body=ig.cylinder('hex_barrel',.00355,.131,(0,0,.0775),segments=6,material=yellow)
    body.data.materials.append(dark)
    for face in body.data.polygons:
        if abs(face.normal.z)<.5 and face.index%2: face.material_index=1
    p=[body,ig.lathe('wood_tip',[(0,0),(.0008,.003),(.00355,.012)],segments=6,material=wood),
       ig.lathe('lead',[(0,-.002),(.0008,.003),(0,.003)],segments=12,material=graphite),
       ig.cylinder('red_cap',.00355,.006,(0,0,.146),segments=6,material=red)]
    return p

def pencil():
    ig.reset_scene()
    p = pencil_parts()
    ig.build_asset('SM_GraphitePencil','prop',p,OUT,collision_parts=[[p[0]]],texture_size=256,
        origin='center',notes='Staedtler 노리스 실물과 BoothStationeryStudy_20260917 참조. 육각 축·깎인 목재·연속된 흑연 심. 약15cm, 상표 인쇄 없음. 장축 Z.')

if __name__ == '__main__':
    ledger()
    pad()
    pencil()
