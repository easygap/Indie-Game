"""실물 사진과 EntryReference_20260917 시트를 대조한 인터폰·작업조끼.

인터폰은 벽면 하단 중심, 조끼는 벽면 밑단 중심이 원점이며 앞은 +Y다.
옷의 주름과 반사띠를 한 메시·한 재질에 굽는다. 실시간 천 계산은 없다.
"""
import math
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import ig_blender_lib as ig


def build_intercom(out):
    ig.reset_scene()
    ivory = ig.mat_plastic('인터폰외함', (.67,.64,.54), roughness=.38, bump=0)
    black = ig.mat_plastic('전면패널', (.026,.028,.027), roughness=.36, bump=0)
    glass = ig.mat_gloss('꺼진LCD', (.007,.009,.010), roughness=.16)
    ink = ig.mat_plastic('버튼인쇄', (.61,.60,.53), roughness=.52, bump=0)
    dark = ig.mat_plastic('틈새', (.008,.009,.009), roughness=.65, bump=0)
    parts=[]
    def box(name, size, at, mat, bevel=.001, segments=2):
        ob=ig.box(name,size,at,bevel=bevel,segments=segments,material=mat); parts.append(ob); return ob
    # 제조사 규격 140×203×26mm. 유리는 검은 전면 패널의 개구부 안에만 있다.
    body=box('후면몸체',(.140,.023,.203),(0,.0115,.1015),ivory,.006,4)
    box('벽걸이받침',(.112,.003,.165),(0,.0015,.1015),dark,.003)
    face=box('검은전면',(.128,.003,.191),(0,.0245,.1015),black,.006,4)
    ig.boolean(face,ig.box('화면구멍',(.097,.020,.055),(0,.026,.147)),'DIFFERENCE')
    box('LCD홈',(.097,.001,.055),(0,.0232,.147),dark,.002)
    box('LCD유리',(.095,.0007,.0534),(0,.0242,.147),glass,.0015)
    box('상단스피커',(.023,.0005,.0018),(0,.0262,.194),dark,.0004)
    box('하단스피커',(.031,.0005,.0018),(0,.0262,.013),dark,.0004)
    parts.append(ig.cylinder('마이크',.0007,.0005,(-.047,.0262,.025),rotation=(math.pi/2,0,0),segments=8,material=dark))
    # 모니터·경비실·통화·문열림. 그림으로 겹치지 않고 전면에 얇게 인쇄한다.
    def stroke(name, points):
        parts.append(ig.pipe(name,[(x,.0263,z) for x,z in points],.00032,resolution=4,material=ink))
    for x in (-.037,-.012,.013,.038):
        if x == -.037:
            stroke('모니터',[(x-.003,.107),(x-.003,.112),(x+.003,.112),(x+.003,.107),(x-.003,.107)])
            stroke('모니터받침',[(x,.107),(x,.105),(x+.002,.105),(x-.002,.105)])
        elif x == -.012:
            stroke('경비실',[(x-.003,.104),(x-.003,.111),(x,.113),(x+.003,.111),(x+.003,.104),(x-.003,.104)])
            box('경비실창',(.001,.0002,.002),(x,.0263,.108),ink,0)
        elif x == .013:
            stroke('전화',[(x-.003,.113),(x-.004,.110),(x-.002,.107),(x+.001,.105),(x+.004,.106)])
            box('수화기위',(.003,.0003,.0016),(x-.003,.0263,.1125),ink,.0003)
            box('수화기아래',(.0016,.0003,.003),(x+.0035,.0263,.106),ink,.0003)
        else:
            stroke('문열림',[(x-.003,.104),(x-.003,.113),(x+.003,.113),(x+.003,.104)])
            stroke('열린문',[(x-.003,.104),(x+.001,.103),(x+.001,.112),(x-.003,.113)])
    return ig.build_asset('SM_VideoIntercom','prop',parts,out,collision_parts=[],texture_size=1024,
        preview_yaw=200,notes='실물 KCV-R431E 치수 14×2.6×20.3cm. 로고 없는 외함, 꺼진 LCD 한 개, 발광 없음. 원점은 벽면 아래 중심, 앞 +Y. 현관 (62,-214.8,136). EntryReference_20260917.json 참조.')


def build_vest(out):
    from build_work_vest import build_vest as build_cloth_vest
    return build_cloth_vest(out)


if __name__=='__main__':
    out=ig.out_root_from_argv()
    build_intercom(out)
    build_vest(out)
