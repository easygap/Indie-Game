"""실물·imagegen 참조를 따른 간편식 포장과 공사 자재. 실행 중에는 재질 한 장씩 쓴다."""
import math
import os
import random
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import ig_blender_lib as ig

OUT=ig.out_root_from_argv()
ROOT=os.path.join(os.path.dirname(__file__),'../../Content/SourceArt')

def image_mat(name, file, rough, scale=1, bump=0):
    m=ig.mat_image(name,os.path.join(ROOT,file),roughness=rough)
    nt=m.node_tree; t=next(n for n in nt.nodes if n.bl_idname=='ShaderNodeTexImage')
    c=nt.nodes.new('ShaderNodeTexCoord'); v=nt.nodes.new('ShaderNodeVectorMath');v.operation='SCALE';v.inputs[3].default_value=scale
    nt.links.new(c.outputs['Object'],v.inputs[0]);nt.links.new(v.outputs[0],t.inputs['Vector'])
    t.projection='BOX'; t.projection_blend=.1
    if bump:
        b=nt.nodes.new('ShaderNodeBump');b.inputs['Distance'].default_value=bump;b.inputs['Strength'].default_value=.18
        nt.links.new(t.outputs['Color'],b.inputs['Height']);nt.links.new(b.outputs[0],nt.nodes.get('Principled BSDF').inputs['Normal'])
    return m

def mesh(name, vertices, faces, material):
    data=bpy.data.meshes.new(name);data.from_pydata(vertices,[],faces);data.update()
    ob=ig._link(bpy.data.objects.new(name,data));data.materials.append(material);return ob

def print_face(name, points, y, material, reverse=False):
    ob=mesh(name,[(x,y,z) for x,z in points],[list(range(len(points))) if not reverse else list(reversed(range(len(points))))],material)
    uv=ob.data.uv_layers.new(name='ImageUV');lo=min(x for x,z in points);hi=max(x for x,z in points);bottom=min(z for x,z in points);top=max(z for x,z in points)
    for f in ob.data.polygons:
        for li in f.loop_indices:
            p=ob.data.vertices[ob.data.loops[li].vertex_index].co
            uv.data[li].uv=((p.x-lo)/(hi-lo) if not reverse else (hi-p.x)/(hi-lo),(p.z-bottom)/(top-bottom))
    return ob

def kimbap(index, out_root=None):
    ig.reset_scene()
    nori=image_mat('WrappedNori','AI/KimbapFilmAlbedo_20260916.png',.30,10,.00018)
    # 110g 소포장. 모서리를 깎고 1.6mm 둥글림을 줘 식품 포장의 실루엣을 만든다.
    loop=[(-.042,0),(.042,0),(.045,.004),(.004,.076),(0,.080),(-.004,.076),(-.045,.004)]
    vertices=[(x,y,z+.0018) for y in (-.016,.016) for x,z in loop];n=len(loop)
    faces=[list(range(n)),list(reversed(range(n,2*n)))]+[(j+n,(j+1)%n+n,(j+1)%n,j) for j in range(n)]
    body=mesh('rice_packet',vertices,faces,nori)
    mod=body.modifiers.new('soft_corners','BEVEL');mod.width=.0016;mod.segments=3;ig.apply_modifiers(body)
    parts=[body]
    colors=[(.06,.15,.085),(.30,.07,.05),(.39,.16,.065),(.15,.09,.045)]
    strip=ig.mat_plastic('TearStrip',colors[index],roughness=.37,bump=0)
    fold=ig.mat_plastic('FilmSeam',(.13,.15,.115),roughness=.31,bump=0)
    # 몸통 위에 밀착한 뜯는 띠와 얇은 포장 끝. 뒷면에도 같은 띠가 이어진다.
    for side in (-1,1):
        parts.append(ig.box('tear_strip',(.0028,.00018,.079),(0,side*.01618,.041),material=strip))
    parts.append(ig.box('pull_tab',(.012,.0005,.013),(0,-.013,.0865),rotation=(.18,0,0),bevel=.0008,material=fold))
    for x in (-.041,.041):
        parts.append(ig.box('folded_end',(.012,.022,.0015),(x,0,.001),rotation=(0,0,(-.12 if x<0 else .12)),bevel=.0005,material=fold))
    # 얕은 밀봉 자국만 메시로 남긴다. 전면에 별도의 투명 껍질을 겹치지 않는다.
    for y in (-.014,-.007,0,.007,.014):
        parts.append(ig.box('bottom_crimp',(.082,.0007,.0004),(0,y,.0003),material=fold))
    front=ig.mat_image_uv('FrontPrint',os.path.join(ROOT,f'UtilityPrints/KimbapFront{index}.png'),roughness=.49)
    back=ig.mat_image_uv('BackPrint',os.path.join(ROOT,f'UtilityPrints/KimbapBack{index}.png'),roughness=.48)
    parts.append(print_face('front_label',[(-.029,.009),(.029,.009),(.025,.035),(.018,.043),(-.018,.043),(-.025,.035)],-.0164,front))
    parts.append(print_face('back_label',[(-.021,.008),(.021,.008),(.021,.036),(-.021,.036)],.0164,back,True))
    # UE 정면(-Y)에서 화면 오른쪽은 -X. 개봉 탭은 정점 위치로 배치한다.
    for number,x,z in [(1,0,.085),(2,-.035,.008),(3,.035,.008)]:
        m=ig.mat_image_uv('Tab'+str(number),os.path.join(ROOT,f'UtilityPrints/KimbapTab{number}.png'),roughness=.44)
        parts.append(ig.image_quad('tab_number',(.006,.006),(x,-.0167,z),m))
    return ig.build_asset('SM_TriangleKimbap'+'ABCD'[index],'prop',parts,out_root or OUT,collision_parts=[],texture_size=1024,
        sharp_angle=50,mirror_print_uv=True,notes='실물 포장과 KimbapConstructionStudy_20260916 참조. 본체9×8×3.2cm, 종이 라벨·중앙 뜯는 띠·접힌 밑면. 1024px 단일 불투명 재질, 앞 -Y. 글씨 방향은 UE 실제 화면에서 확인.')

def board():
    ig.reset_scene()
    paper=image_mat('FacingPaper','AI/GypsumPaperAlbedo_20260916.png',.91,1.5,.0001)
    core=ig.mat_plastic('GypsumCore',(.66,.65,.59),roughness=.97,bump=.00015,grain_scale=1800)
    # 국내 12.5T 판재를 잘라 둔 자재. 노출된 앞뒤 절단면에는 흰 코어가 보인다.
    body=ig.box('core',(1.2,.8,.0119),(0,0,0),bevel=.00025,material=core)
    top=ig.box('face',(1.2,.8,.0003),(0,0,.0061),material=paper)
    bottom=ig.box('back',(1.2,.8,.0003),(0,0,-.0061),material=paper)
    # 양쪽 공장 가장자리는 종이가 코어를 감싼다.
    sides=[ig.box('wrapped_edge',(.0003,.8,.0125),(x,0,0),material=paper) for x in (-.6001,.6001)]
    ig.build_asset('SM_GypsumCutBoard','prop',[body,top,bottom]+sides,OUT,collision_parts=[[body]],texture_size=1024,
        origin='center',notes='120×80×1.25cm 절단재. 위아래 종이0.3mm, 흰 석고 코어, X 양끝의 감싼 종이. 12.5T 판재, 스택은 ISM으로 재사용.')

def paint_can():
    ig.reset_scene()
    steel=ig.mat_metal('CanRim',(.44,.45,.42),roughness=.45,streak=.02)
    white=ig.mat_plastic('CanPaint',(.53,.54,.49),roughness=.64,bump=0)
    body=ig.lathe('can',[(0,0),(.113,0),(.119,.008),(.11890,.03),(.11809,.206),(.118,.225),(.120,.237),(.113,.241),(0,.241)],segments=40,material=white)
    parts=[body]
    for z in (.008,.224,.237):parts.append(ig.torus('rim',.118,.0026,(0,0,z),major_segments=40,minor_segments=6,material=steel))
    parts.append(ig.cylinder('lid',.112,.003,(0,0,.242),segments=40,material=steel))
    # 접힌 철사 손잡이는 옆으로 내려와 바닥을 뚫지 않는다.
    pts=[(.121*math.cos(t),-.03-.07*math.sin(t),.176-.109*math.sin(t)) for t in [math.pi*i/24 for i in range(25)]]
    parts.append(ig.pipe('wire_bail',pts,.0018,resolution=6,corner_radius=0,material=steel))
    for side in (-1, 1):
        parts.append(ig.box('bail_lug',(.009,.012,.012),(side*.118,-.03,.176),bevel=.002,material=steel))
    label=ig.mat_image_uv('PaintLabel',os.path.join(ROOT,'UtilityPrints/WorkPaintCan.png'),roughness=.61)
    # 별도 라벨 껍질은 LOD가 줄어들면 본체를 뚫는다. 본체의 같은 정점에
    # 인쇄 UV를 붙여 모든 거리에서 표면이 하나만 남도록 한다.
    body.data.materials.append(label);uv=body.data.uv_layers.new(name='ImageUV')
    for f in body.data.polygons:
        c=f.center;angle=math.atan2(c.y,c.x)
        if .03001 < c.z < .20599 and abs(angle+math.pi/2) < .943:
            f.material_index=1
            for li in f.loop_indices:
                p=body.data.vertices[body.data.loops[li].vertex_index].co
                uv.data[li].uv=((math.atan2(p.y,p.x)+math.pi/2+.942478)/1.884956,(p.z-.03)/.176)
    # 4L 캔 제작사 규격: 지름167×높이190mm. 모든 부품과 위치를 함께 줄인다.
    for part in parts:
        for v in part.data.vertices: v.co.x*=.167/.24;v.co.y*=.167/.24;v.co.z*=.190/.2435
        part.location.x*=.167/.24;part.location.y*=.167/.24;part.location.z*=.190/.2435
    ig.build_asset('SM_WorkPaintCan','prop',parts,OUT,collision_parts=[[body]],texture_size=1024,mirror_print_uv=True,
        sharp_angle=40,notes='실물 4L 캔의 지름167×높이190mm 규격. 뚜껑·접힌 철사 손잡이·말린 테두리와 한글 인쇄. 앞 -Y.')

def chips():
    ig.reset_scene();rng=random.Random(116)
    paper=image_mat('ChipPaper','AI/GypsumPaperAlbedo_20260916.png',.92,2)
    core=ig.mat_plastic('ChipCore',(.69,.68,.62),roughness=.97,bump=.00012,grain_scale=1000)
    parts=[]
    for i in range(18):
        count=rng.randrange(5,9);r=rng.uniform(.009,.044);x=rng.uniform(-.35,.35);y=rng.uniform(-.25,.25)
        loop=[]
        for j in range(count):
            a=(j+rng.uniform(-.22,.22))*math.tau/count;rad=r*rng.uniform(.48,1.15)
            loop.append((x+math.cos(a)*rad,y+math.sin(a)*rad))
        thick=rng.uniform(.003,.010)
        ob=mesh('broken_board',[(a,b,z) for z in (0,thick) for a,b in loop],
            [list(reversed(range(count))),list(range(count,count*2))]+[(j,(j+1)%count,(j+1)%count+count,j+count) for j in range(count)],core)
        ob.data.materials.append(paper);ob.data.polygons[1].material_index=1 if i%3 else 0;parts.append(ob)
    ig.build_asset('SM_GypsumChipCluster','prop',parts,OUT,collision_parts=[],texture_size=512,
        notes='가로80cm 안쪽에 흩어진 얇은 석고 파편18개. 잘린 흰 코어와 종이 윗면. 보행 충돌 없음. 바닥과5mm이상 떨어뜨리지 않음.')

def glove():
    ig.reset_scene()
    knit=image_mat('CottonKnit','AI/WorkGloveKnit_20260916.png',.94,4,.00024)
    thread=ig.mat_plastic('CuffThread',(.07,.065,.056),roughness=.95,bump=0)
    def oval(name, center, scale, angle=0):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=12,location=center)
        ob=bpy.context.object;ob.name=name;ob.scale=scale;ob.rotation_euler[2]=angle
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        ob.data.materials.append(knit);return ob
    parts=[oval('palm',(0,-.025,.007),(.046,.067,.006))]
    for x,y,length,angle in [(-.033,.064,.078,.10),(-.009,.070,.090,.015),(.015,.066,.082,-.035),(.036,.055,.061,-.10)]:
        parts.append(oval('empty_finger',(x,y,.006),(.010,length/2,.005),angle))
    parts.append(oval('thumb',(-.055,-.015,.006),(.012,.041,.005),-.62))
    body=ig.join(parts,'glove_body')
    # 손가락 뿌리를 손바닥과 이어서 이음새가 튀어나오지 않게 한다.
    mod=body.modifiers.new('knit_union','REMESH');mod.mode='VOXEL';mod.voxel_size=.0013
    mod.use_smooth_shade=True;ig.apply_modifiers(body)
    mod=body.modifiers.new('soft_fold','SMOOTH');mod.factor=.6;mod.iterations=3;ig.apply_modifiers(body)
    mod=body.modifiers.new('game_mesh','DECIMATE');mod.ratio=.035;ig.apply_modifiers(body)
    # 손목은 안쪽과 바깥쪽을 가진 얇은 통으로 만든다.
    verts=[];faces=[];steps=48
    for ring,(y,width,height) in enumerate([(-.12,.032,.0045),(-.10,.034,.006),(-.074,.036,.0065)]):
        for a in range(steps):
            t=a*math.tau/steps
            verts.append((width*math.cos(t),y,.007+height*math.sin(t)))
    for ring in range(2):
        for a in range(steps):
            b=(a+1)%steps;faces.append((ring*steps+a,ring*steps+b,(ring+1)*steps+b,(ring+1)*steps+a))
    cuff=mesh('hollow_cuff',verts,faces,knit)
    mod=cuff.modifiers.new('cotton_thickness','SOLIDIFY');mod.thickness=.00065;ig.apply_modifiers(cuff)
    hem=ig.pipe('overlock',[(.032*math.cos(a*math.tau/48),-.1202,.007+.0045*math.sin(a*math.tau/48)) for a in range(49)],.0007,resolution=4,corner_radius=0,material=thread)
    for ob in (body,cuff):
        for f in ob.data.polygons:f.use_smooth=True
    hull=ig.box('glove_trace',(.13,.237,.015),(-.013,-.002,.0075),material=knit)
    hull.hide_render=True
    ig.build_asset('SM_CottonWorkGlove','prop',[body,cuff,hem],OUT,collision_parts=[[hull]],texture_size=1024,
        notes='실제 면장갑과 imagegen 삼면도 참조. 빈 손가락 다섯 개와 뚫린 손목, 면 편직 무늬. 길이 약24cm, 바닥에 눕힌 상태. Pawn 충돌은 증거 액터에서 끈다.')

def notebook():
    ig.reset_scene()
    cover=ig.mat_plastic('NotebookCover',(.055,.068,.057),roughness=.89,bump=.00013,grain_scale=600)
    paper=ig.mat_plastic('PageEdges',(.65,.63,.55),roughness=.96,bump=0)
    parts=[ig.box('pages',(.151,.21,.010),(0,0,0),bevel=.001,material=paper)]
    for z in (-.006,.006):parts.append(ig.box('cover',(.16,.22,.0018),(0,0,z),bevel=.001,material=cover))
    # UE에서 이름표를 정방향으로 읽는 쪽의 왼편은 +X다.
    parts.append(ig.box('spine',(.006,.22,.013),(.078,0,0),bevel=.0015,material=cover))
    for z in (-.004,-.002,0,.002,.004):
        parts.append(ig.box('page_line',(.0002,.209,.00018),(-.0756,0,z),material=cover))
    label=ig.mat_image_uv('OwnerLabel',os.path.join(ROOT,'UtilityPrints/TunerNotebook.png'),roughness=.9)
    # 인쇄 앞면(-Y)이 표지 위(+Z)를 향해야 이름표를 뒤집지 않고 읽는다.
    parts.append(ig.image_quad('owner',(.090,.034),(0,-.048,.00705),label,rotation=(-math.pi/2,0,0)))
    ig.build_asset('SM_TunerNotebook','prop',parts,OUT,collision_parts=[[parts[0]]],texture_size=1024,
        origin='center',mirror_print_uv=True,notes='16×22cm 천 표지 수첩. 표지·책등·종이 단면·이름표를 구분. 중심 원점, 두께1.4cm.')

def sheet(draped):
    ig.reset_scene()
    m=ig.mat_plastic('Sheet',(.40,.43,.39),roughness=.38,bump=0)
    nx,ny=24,18;v=[];faces=[]
    for j in range(ny+1):
        y=(j/ny-.5)*(1.00 if draped else .86)
        for i in range(nx+1):
            x=(i/nx-.5)*(1.40 if draped else 1.04)
            if draped:
                drop=max(abs(x)-.60,abs(y)-.40,0)
                z=.003-drop*1.15 + .0015*math.sin(21*x+4*y)*math.cos(28*y)
            else:
                z=.001+abs(.008*math.sin(16*x+4*y)*math.cos(12*y))
                x+=.011*math.sin(j*1.7)*(abs(x)/.52)**8
            v.append((x,y,z))
    for j in range(ny):
        for i in range(nx):
            a=j*(nx+1)+i;faces.append((a,a+1,a+nx+2,a+nx+1))
    ob=mesh('protective_film',v,faces,m);ig.uv_box_project(ob,1)
    for f in ob.data.polygons:f.use_smooth=True
    ig.build_asset('SM_ConstructionSheetDrape' if draped else 'SM_ConstructionSheetFloor','prop',[ob],OUT,
        collision_parts=[],raw_uv=True,preview=False,notes='두께 없는 보양 비닐 표면. 주름은 실제 정점, 충돌·그림자 없음. 런타임 M_ConstructionFilm 사용. 덮개는120×80cm 판재 위에서 가장자리만 아래로 처짐.' if draped else '바닥에 접힌 보양 비닐. 높이1~9mm, 종전 기울어진 상자의 떠 있는 모서리를 없앰.')

if __name__ == '__main__':
    only = sys.argv[sys.argv.index('--') + 2:] if '--' in sys.argv else []
    if not only or 'kimbap' in only:
        for i in range(4): kimbap(i)
    if not only or 'board' in only: board()
    if not only or 'paint' in only: paint_can()
    if not only or 'chips' in only: chips()
    if not only or 'glove' in only: glove()
    if not only or 'notebook' in only: notebook()
    if not only or 'sheet' in only:
        sheet(True)
        sheet(False)
