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
    ig.reset_scene()
    fabric=ig.mat_plastic('형광폴리에스터',(.53,.65,.028),roughness=.87,bump=.008,grain_scale=1100)
    tape=ig.mat_plastic('회색반사띠',(.34,.37,.34),roughness=.36,bump=.003,grain_scale=1500)
    binding=ig.mat_plastic('검은바인딩',(.016,.018,.017),roughness=.82,bump=.003)
    hanger=ig.mat_metal('철사옷걸이',(.028,.032,.031),roughness=.48,streak=0)
    hook=ig.mat_plastic('벽고리',(.66,.64,.58),roughness=.59,bump=0)
    parts=[]
    front_rows=[.008,.035,.075,.115,.160,.210,.270,.320,.355,.390,.430,.470,.510,.550,.585,.600,.613,.620]
    back_rows=[.008,.075,.115,.160,.210,.270,.320,.355,.430,.510,.585,.605,.620]
    def bounds(z,back):
        inner=0 if back else .006+max(0,z-.345)/.275*.080
        if back and z>.588: inner=math.sqrt(max(0,1-((.620-z)/.032)**2))*.086
        outer=.255 if z<=.355 else .255-.065*math.sin((z-.355)/.265*math.pi/2)
        return inner,outer
    def point(x,z,back):
        fold=math.sin(x*61+z*4)*.009+math.sin(x*27-z*7)*.008
        depth=(.012+fold*.28) if back else (.055+fold+.004*math.sin(z*14+x*8))
        hem=.006*math.sin(x*32+1)*(1-z/.62)
        return (x,depth,z+hem)
    for back in (False,True):
        rows=back_rows if back else front_rows
        for sign in (-1,1):
            verts=[];faces=[];materials=[]
            columns=3 if back else 6
            for z in rows:
                lo,hi=bounds(z,back)
                # 반사띠 경계에 정점을 둬 줄이 계단처럼 꺾이지 않게 한다.
                xs=(lo,.105,.155,hi) if back else (lo,(lo+.105)/2,.105,.130,.155,(.155+hi)/2,hi)
                for x in xs:
                    verts.append(point(sign*x,z,back))
            for r in range(len(rows)-1):
                for c in range(columns):
                    a=r*(columns+1)+c
                    face=(a,a+1,a+columns+2,a+columns+1)
                    if (sign>0) != back: face=tuple(reversed(face))
                    faces.append(face)
                    center=[sum(verts[v][k] for v in face)/4 for k in (0,2)]
                    x,z=abs(center[0]),center[1]
                    materials.append(int(.114<z<.161 or .269<z<.321 or (z>=.32 and .105<x<.155)))
            mesh=bpy.data.meshes.new('봉제패널');mesh.from_pydata(verts,[],faces);mesh.update()
            ob=ig._link(bpy.data.objects.new('조끼뒤판' if back else '조끼앞판',mesh))
            ob.data.materials.append(fabric);ob.data.materials.append(tape)
            for poly,index in zip(mesh.polygons,materials): poly.material_index=index;poly.use_smooth=True
            solid=ob.modifiers.new('원단두께','SOLIDIFY');solid.thickness=.001;solid.offset=0
            ig.set_active(ob);bpy.ops.object.modifier_apply(modifier=solid.name)
            parts.append(ob)
            # 목둘레, 암홀, 밑단은 실제 윤곽에 붙는다. 어깨는 뒤판과 이어진다.
            outer=[point(sign*bounds(z,back)[1],z,back) for z in rows]
            inner=[point(sign*bounds(z,back)[0],z,back) for z in rows]
            if not back:
                parts.append(ig.pipe('앞중심바인딩',inner,.0023,resolution=3,material=binding))
            elif sign==1:
                neckline=[point(.086*math.sin(a),.620-.032*math.cos(a),True) for a in (-math.pi/2,-1,-.5,0,.5,1,math.pi/2)]
                parts.append(ig.pipe('뒷목바인딩',neckline,.002,resolution=3,material=binding))
            parts.append(ig.pipe('암홀옆선',outer,.0023,resolution=3,material=binding))
            lo,hi=bounds(rows[0],back)
            hem=[point(sign*(lo+(hi-lo)*c/4),rows[0],back) for c in range(5)]
            parts.append(ig.pipe('밑단',hem,.0023,resolution=3,material=binding))
    # 옆구리와 어깨를 연결하되 암홀은 비워 둔다.
    for sign in (-1,1):
        for top in (False,True):
            coords=[]
            if top:
                lo,hi=bounds(.62,False)
                for back in (False,True): coords.extend([point(sign*lo,.62,back),point(sign*hi,.62,back)])
            else:
                for back in (False,True): coords.extend([point(sign*.255,.008,back),point(sign*.255,.355,back)])
            mesh=bpy.data.meshes.new('이음');mesh.from_pydata(coords,[],[(0,1,3,2)]);mesh.update()
            ob=ig._link(bpy.data.objects.new('어깨이음' if top else '옆구리이음',mesh));ob.data.materials.append(fabric)
            solid=ob.modifiers.new('이음두께','SOLIDIFY');solid.thickness=.001
            ig.set_active(ob);bpy.ops.object.modifier_apply(modifier=solid.name);parts.append(ob)
    # 지퍼 손잡이는 V자 끝에만 있다. 옷걸이와 벽고리를 모델 안에 포함한다.
    parts.append(ig.box('지퍼손잡이',(.008,.004,.018),(0,.064,.327),bevel=.001,segments=2,material=binding))
    parts.append(ig.pipe('옷걸이',[(-.225,.026,.577),(-.23,.026,.59),(0,.026,.668),(.23,.026,.59),(.225,.026,.577),(-.225,.026,.577)],
        .002,resolution=4,corner_radius=.006,corner_steps=2,material=hanger))
    parts.append(ig.pipe('옷걸이고리',[(0,.026,.668),(0,.026,.681),(.015,.026,.695),(.017,.026,.712),(.006,.026,.722),(-.008,.026,.720),(-.015,.026,.710)],
        .002,resolution=4,corner_radius=.007,corner_steps=3,material=hanger))
    parts.append(ig.box('벽고리받침',(.025,.005,.040),(0,.0025,.705),bevel=.004,segments=2,material=hook))
    parts.append(ig.pipe('벽고리',[(0,.006,.698),(0,.025,.698),(0,.031,.707)],.003,resolution=6,corner_radius=.004,corner_steps=3,material=hook))
    # 반사띠 경계는 보존하고 격자와 가는 봉제선의 밀도를 줄인다.
    for part in parts:
        if part.name.startswith(('앞중심바인딩','뒷목바인딩','암홀옆선','밑단','옷걸이','벽고리')):
            ig.decimate(part,max(16,ig.triangle_count(part)//2))
    vest=ig.join(parts,'조끼저밀도')
    ig.log(f'조끼 삼각형: {ig.triangle_count(vest)}')
    if ig.triangle_count(vest)>3000:
        raise RuntimeError('작업조끼의 근접 메시가 삼각형 예산을 넘었다')
    return ig.build_asset('SM_HangingWorkVest','prop',[vest],out,collision_parts=[],texture_size=1024,preview_yaw=200,
        notes='실물 안전조끼와 WorkVestReference_20260917 시트 대조. 폭51cm, 옷높이62cm, 고리포함73cm. 앞 +Y, 원점 벽면 밑단 중심. 앞뒤판·빈 암홀·목둘레·고정 주름·옷걸이. 발광 없음. 관리실 동쪽 벽 (279.8,-155,130), yaw 90. 아래 보관 자재와 11cm 간격. 한 재질·고정 메시, 천 시뮬레이션·충돌 없음.')


if __name__=='__main__':
    out=ig.out_root_from_argv()
    build_intercom(out)
    build_vest(out)
