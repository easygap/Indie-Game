"""실물 사진과 UtilityMeter/BoothRecorderReference_20260915 시트를 대조한 설비.

계량기 앞면은 -Y, 원점은 뒷판 하단이다. 회전 원판은 따로 반입한다.
모니터는 화면 중심이 Z 25cm, 앞면 Y -5cm이며 받침 바닥이 원점이다.
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bpy
import ig_blender_lib as ig


def mats():
    return {
        "case": ig.mat_plastic("Phenolic", (.012, .014, .016), roughness=.48, bump=0),
        "steel": ig.mat_metal("BrushedSteel", (.64, .66, .67), roughness=.36, streak=.015),
        "paint": ig.mat_painted_steel("CabinetEnamel", (.52, .54, .52), roughness=.42, wear=.008, bump=.0001),
        "copper": ig.mat_metal("CopperCoil", (.36, .15, .06), roughness=.42, streak=0),
        "brass": ig.mat_metal("BrassTerminal", (.47, .38, .16), roughness=.38, streak=0),
        "black": ig.mat_plastic("PrintBlack", (.008, .009, .01), roughness=.6),
        "white": ig.mat_plastic("PrintIvory", (.78, .79, .74), roughness=.65),
        "glass": ig.mat_glass("Glass"),
        "green": ig.mat_emissive("PowerLed", (.09, .48, .11), strength=1.6),
        "red": ig.mat_plastic("DiskLedOff", (.025, .005, .003), roughness=.27, bump=0),
    }


def lettering(name, value, height, at, material):
    curve = bpy.data.curves.new(name, "FONT")
    curve.body = value
    curve.align_x = "CENTER"
    curve.align_y = "CENTER"
    curve.size = height
    curve.resolution_u = 6
    ob = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(ob)
    ob.location = at
    ob.rotation_euler = (math.pi / 2, 0, 0)
    ob.data.materials.append(material)
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.convert(target="MESH")
    return bpy.context.object


def finish(name, parts, out, note, size=1024, collision=None):
    return ig.build_asset(name, "prop", parts, out, texture_size=size,
                          collision_parts=collision if collision is not None else [], notes=note,
                          mirror_print_for_ue=name in ("SM_InductionMeter", "SM_BoothMonitor", "SM_BoothRecorder", "SM_BoothKeyring"))


def meter(out):
    ig.reset_scene()
    m = mats()
    p = [ig.box("절연뒷판", (.128, .009, .22), (0, 0, .11), bevel=.003, segments=2, material=m["case"]),
         ig.box("하부단자덮개", (.12, .036, .046), (0, -.022, .028), bevel=.003, segments=2, material=m["case"]),
         ig.box("회전자뒤", (.094, .016, .057), (0, -.017, .084), material=m["case"]),
         ig.box("상부계수기", (.084, .025, .077), (0, -.031, .149), bevel=.002, segments=2, material=m["case"]),
         ig.box("명판", (.096, .0015, .088), (0, -.064, .157), bevel=.0015, segments=2, material=m["steel"])]
    # 숫자 드럼은 창 안쪽으로 들어간다. 시계 눈금이나 지침은 없다.
    plate = p[-1]
    ig.boolean(plate, ig.box("계수기창", (.073, .016, .023), (0, -.064, .178)), "DIFFERENCE")
    p.append(ig.box("숫자창안쪽", (.074, .003, .024), (0, -.060, .178), material=m["black"]))
    # 다섯 계기의 누적 검침값은 게임의 공용 숫자 아틀라스로 각각 그린다.
    p.extend([lettering("단위", "kWh", .009, (.018, -.065, .149), m["black"]),
              lettering("정격", "220 V   60 Hz", .006, (0, -.065, .130), m["black"])])
    for x in (-.04, .04):
        p.append(ig.cylinder("코일", .012, .026, (x, -.028, .085), segments=16, material=m["copper"]))
        for z in (.065, .072, .079, .086, .093):
            p.append(ig.torus("권선", .012, .0007, (x, -.028, z), major_segments=16, minor_segments=4, material=m["copper"]))
        for z in (.054, .207):
            p.append(ig.cylinder("덮개나사", .003, .002, (x, -.076, z), rotation=(math.pi/2, 0, 0), segments=12, material=m["brass"]))
    # 투명 앞면은 얇은 외피다. 원판과 간섭하지 않는 관찰 공간을 남긴다.
    cover = ig.box("투명덮개", (.125, .074, .177), (0, -.040, .132), bevel=.007, segments=3, material=m["glass"])
    inner = ig.box("덮개안쪽", (.121, .074, .173), (0, -.038, .132), bevel=.005, segments=2)
    ig.boolean(cover, inner, "DIFFERENCE")
    p.append(cover)
    for x in (-.044, -.022, 0, .022, .044):
        p.append(ig.cylinder("단자나사", .0032, .003, (x, -.042, .036), rotation=(math.pi/2,0,0), segments=12, material=m["brass"]))
    finish("SM_InductionMeter", p, out, "유도형 계량기. 뒷판 하단 원점, 앞면 -Y. 별도 수평 원판은 로컬 (0,-4.2,9.6)cm. 실물·생성 시트 대조.")


def disc(out):
    ig.reset_scene()
    m = mats()
    p = [ig.cylinder("알루미늄원판", .034, .0022, segments=48, material=m["steel"]),
         ig.cylinder("중앙축", .003, .026, segments=16, material=m["steel"])]
    # 검정 검침 띠가 옆면까지 내려와 수평 원판의 회전을 읽을 수 있다.
    p.append(ig.box("검침표시윗면", (.007, .013, .0002), (0, -.0265, .00125), material=m["black"]))
    p.append(ig.box("검침표시가장자리", (.007, .00025, .0025), (0, -.034, 0), material=m["black"]))
    finish("SM_MeterRotor", p, out, "6.8cm 수평 원판. 원점과 회전축 Z를 보존. 검정 띠가 돌아오는 것으로 관찰.", size=512)


def cabinet(out):
    ig.reset_scene()
    m = mats()
    p = []
    # 열린 검침 창 다섯 개. 창 위의 그림을 읽는 방식이 아니라 내부 계기를 본다.
    face = ig.box("검침창판", (.98, .0018, .64), (0, 0, 0), bevel=.003, segments=2, material=m["paint"])
    for x in (-.36, -.18, 0, .18, .36):
        cut = ig.box("창", (.141, .03, .241), (x, 0, .053), bevel=.006, segments=3)
        ig.boolean(face, cut, "DIFFERENCE")
    p.append(face)
    for x in (-.48, .48):
        p.append(ig.box("측면절곡", (.018, .09, .64), (x, .044, 0), material=m["paint"]))
    for z in (-.31, .31):
        p.append(ig.box("상하절곡", (.96, .09, .018), (0, .044, z), material=m["paint"]))
    for x in (-.47, .47):
        for z in (-.29, .29):
            p.append(ig.cylinder("고정나사", .0038, .002, (x, -.0016, z), rotation=(math.pi/2,0,0), segments=12, material=m["steel"]))
    p.append(ig.box("아래점검덮개", (.84, .003, .086), (0,-.002,-.254), bevel=.002, segments=2, material=m["paint"]))
    p.append(ig.cylinder("캠잠금", .008, .007, (.36,-.007,-.254), rotation=(math.pi/2,0,0), segments=16, material=m["steel"]))
    finish("SM_MeterCabinetFive", p, out, "5칸 검침함 98x64cm. 열린 창 14.1x24.1cm, 18cm 간격. 검침함 실물 사진의 절곡 강판·캠록을 반영.")


def monitor(out):
    ig.reset_scene()
    m=mats()
    p=[]
    body=ig.box("모니터케이스", (.40,.07,.314), (0,-.015,.25), bevel=.006, segments=3, material=m["case"])
    cut=ig.box("액정개구부", (.341,.018,.256), (0,-.05,.25), bevel=.001, segments=1)
    ig.boolean(body,cut,"DIFFERENCE")
    p.append(body)
    p.append(ig.box("목", (.072,.035,.093), (0,0,.057), bevel=.005, segments=2, material=m["case"]))
    base=ig.cylinder("타원받침", .092,.011, (0,0,.0055), segments=40, bevel=.002, material=m["case"])
    base.scale.y=.64
    ig.set_active(base)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    p.append(base)
    for i in range(5):
        x=.088+i*.016
        p.append(ig.cylinder("하단버튼", .0024,.002, (x,-.051,.108), rotation=(math.pi/2,0,0), segments=12, material=m["black"]))
    p.append(lettering("입력표시", "INPUT", .005, (.152,-.0505,.12), m["white"]))
    p.append(ig.cylinder("동작램프", .0015,.001, (.178,-.051,.108), rotation=(math.pi/2,0,0), segments=8, material=m["green"]))
    for x in range(16):
        p.append(ig.box("후면방열홈", (.002,.001,.040), (-.14+x*.018,.0205,.32), material=m["black"]))
    for x in (-.027,.027):
        p.append(ig.cylinder("BNC입력", .004,.010, (x,.025,.18), rotation=(math.pi/2,0,0), segments=12, material=m["steel"]))
    finish("SM_BoothMonitor",p,out,"보안 모니터 실물 형태. 화면 34x25.5cm, 중심 Z25cm, 앞면 Y-5cm. 게임이 화면을 별도로 그린다.")


def recorder(out):
    ig.reset_scene()
    m=mats()
    p=[ig.box("녹화기본체",(.26,.215,.038),(0,0,.026),bevel=.002,segments=2,material=m["case"]),
       ig.box("분리형전면",(.257,.004,.032),(0,-.109,.025),bevel=.002,segments=2,material=m["case"])]
    for x in (-.105,.105):
        for y in (-.08,.08):
            p.append(ig.cylinder("고무발",.010,.007,(x,y,.0035),segments=12,material=m["black"]))
    p.extend([ig.box("USB테두리",(.014,.002,.006),(-.096,-.112,.024),material=m["steel"]),
              ig.box("USB안쪽",(.011,.002,.004),(-.096,-.113,.024),material=m["black"])])
    for x,mat in ((-.07,m["green"]),(-.055,m["red"])):
        p.append(ig.cylinder("상태램프",.0012,.001,(x,-.112,.024),rotation=(math.pi/2,0,0),segments=8,material=mat))
    p.append(lettering("전면표기","POWER  REC",.0045,(-.064,-.112,.015),m["white"]))
    for x in (-.085,-.05,-.015,.02,.055):
        p.append(ig.cylinder("BNC단자",.0045,.009,(x,.112,.025),rotation=(math.pi/2,0,0),segments=12,material=m["steel"]))
    for y in range(10):
        p.append(ig.box("통풍슬릿",(.0008,.006,.012),(.1302,-.06+y*.012,.026),material=m["black"]))
    finish("SM_BoothRecorder",p,out,"4채널 녹화기 26x21.5x4.5cm. SRD-440 제품 사진과 생성 시트 대조. 앞면 -Y, 바닥 원점.")


def keyring(out):
    ig.reset_scene()
    m = mats()
    p = [ig.torus("스플릿링", .014, .0011, (0, 0, .0011), major_segments=32, minor_segments=8, material=m["steel"])]
    font = bpy.data.fonts.load("C:/Windows/Fonts/malgun.ttf")
    for index, angle in enumerate((-.38, .42)):
        key = []
        head = ig.box("열쇠머리", (.022, .018, .0022), (0, -.023, .0012), bevel=.003, segments=3, material=m["steel"])
        ig.boolean(head, ig.cylinder("고리구멍", .0035, .012, (0, -.018, .003), segments=20), "DIFFERENCE")
        key.append(head)
        blade = ig.box("날", (.0075, .035, .0018), (0, -.0475, .0012), material=m["brass"] if index else m["steel"])
        for i, depth in enumerate((.002, .0035, .0015, .003)):
            ig.boolean(blade, ig.box("톱니홈", (.006, .004, .012), (.006-depth, -.036-i*.007, .002)), "DIFFERENCE")
        key.append(blade)
        key.append(ig.box("키홈", (.0012, .027, .0001), (-.001, -.046, .00215), material=m["black"]))
        tag = ig.box("이름표", (.022, .033, .0028), (.026, -.026, .0014), bevel=.003, segments=3, material=m["white"])
        ig.boolean(tag, ig.cylinder("태그구멍", .0024, .012, (.026, -.014, .002), segments=16), "DIFFERENCE")
        key.append(tag)
        key.append(ig.torus("태그연결링", .009, .0007, (.018, -.008, .0022), major_segments=20, minor_segments=6, material=m["steel"]))
        curve = bpy.data.curves.new("태그글자", "FONT")
        curve.body = "옥상" if index == 0 else "창고"
        curve.font = font
        curve.align_x = "CENTER"
        curve.align_y = "CENTER"
        curve.size = .009
        curve.resolution_u = 4
        label = bpy.data.objects.new("태그글자", curve)
        bpy.context.collection.objects.link(label)
        label.location = (.026, -.029, .0029)
        label.data.materials.append(m["black"])
        ig.set_active(label)
        bpy.ops.object.convert(target="MESH")
        key.append(bpy.context.object)
        for part in key:
            x, y = part.location.x, part.location.y
            part.location.x = x*math.cos(angle)-y*math.sin(angle)
            part.location.y = x*math.sin(angle)+y*math.cos(angle)
            part.rotation_euler.z += angle
        p.extend(key)
    finish("SM_BoothKeyring", p, out, "기존 SheetRooftopUnlockedPadlockKeysReference의 금속 열쇠와 분리링을 대조. 열쇠 두 개와 옥상·창고 이름표. 상판에 눕혀 둔다.", collision=[p])


if __name__ == "__main__":
    out=ig.out_root_from_argv()
    builders={"SM_InductionMeter":meter,"SM_MeterRotor":disc,"SM_MeterCabinetFive":cabinet,
              "SM_BoothMonitor":monitor,"SM_BoothRecorder":recorder,"SM_BoothKeyring":keyring}
    selected={s for s in os.environ.get("IG_BLENDER_ONLY", "").split(",") if s}
    for name,build in builders.items():
        if not selected or name in selected: build(out)
