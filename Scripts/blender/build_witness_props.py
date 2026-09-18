"""눈으로 확인한 물건과 목격 대사를 맞추는 작은 소품 세 가지."""
import math
import os
import sys
import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig

PRINTS = os.path.abspath('Content/SourceArt/WitnessPrints')


def finish(name, parts, out, collision, size=1024, notes=''):
    result = ig.build_asset(name, 'prop', parts, out, collision_parts=[collision],
        texture_size=size, preview_yaw=15, notes=notes,
        mirror_print_uv=name in ('SM_WitnessPrescriptionEnvelope', 'SM_WitnessStoreRoster'))
    folder = os.path.join(out, name)
    ig.preview_material_from_bakes(bpy.data.objects[name],
        {k: os.path.join(folder, name+'_'+k+'.png') for k in ('D','N','ORM')})
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, name+'.blend'))
    return result


def surface(name, vertices, faces, material, width=None, length=None):
    mesh=bpy.data.meshes.new(name); mesh.from_pydata(vertices,[],faces); mesh.update()
    ob=bpy.data.objects.new(name,mesh); bpy.context.collection.objects.link(ob)
    mesh.materials.append(material)
    if width and length:
        uv=mesh.uv_layers.new(name='ImageUV')
        for poly in mesh.polygons:
            for li in poly.loop_indices:
                co=mesh.vertices[mesh.loops[li].vertex_index].co
                uv.data[li].uv=(co.x/width+.5, co.y/length+.5)
    return ob


def envelope(out):
    ig.reset_scene()
    printmat=ig.mat_image_uv('PrescriptionInk',os.path.join(PRINTS,'PrescriptionEnvelope.png'),roughness=.90)
    paper=ig.mat_image('EnvelopePaper',os.path.abspath('Content/SourceArt/T_PaperClean_V2_D.png'),roughness=.91)
    # 인쇄 면은 위를 향하고, 봉투 입구는 얇게 벌어진다. 알약은 밖에 놓지 않는다.
    w,l=.11,.165
    verts=[]
    for j in range(5):
        y=-l/2+l*j/4
        for i in range(3):
            x=-w/2+w*i/2
            z=.0013+.00055*math.sin(j*1.8+i*.7)
            if j==4:z+=.002
            verts.append((x,y,z))
    faces=[(j*3+i,j*3+i+1,(j+1)*3+i+1,(j+1)*3+i) for j in range(4) for i in range(2)]
    front=surface('printed_envelope',verts,faces,printmat,w,l)
    solid=front.modifiers.new('paper_thickness','SOLIDIFY');solid.thickness=.00018
    back=ig.box('back_paper',(w,l,.00035),(0,0,.0003),material=paper)
    flap=surface('open_flap',[(-w/2,l/2,.00045),(w/2,l/2,.00045),
        (w/2-.004,l/2+.015,.002),(-w/2+.004,l/2+.015,.002)],[(0,1,2,3)],paper)
    solid=flap.modifiers.new('flap_thickness','SOLIDIFY');solid.thickness=.00018
    hull=ig.box('envelope_probe',(w,l,.006),(0,0,.003));hull.hide_render=True
    return finish('SM_WitnessPrescriptionEnvelope',[front,back,flap],out,[hull],1024,
        '인쇄코리아 약봉투 실물과 WitnessPropsStudy_20260917.png 참고. 11×18cm 종이 봉투, 0.35cm 이내 두께. 인쇄면은 별도 UV에 실제 한글 조판. 이름과 조제일만 목격 근거로 쓴다.')


def clipboard(out):
    ig.reset_scene()
    boardmat=ig.mat_speckle('Hardboard',(.30,.21,.12),(.18,.13,.085),scale=1200,roughness=.79,bump=.02)
    steel=ig.mat_metal('ClipSteel',(.54,.57,.58),roughness=.28)
    black=ig.mat_rubber('ClipPads',(.028,.03,.031))
    paper=ig.mat_image_uv('RosterPrint',os.path.join(PRINTS,'StoreRoster.png'),roughness=.87)
    outline=[]
    for cx,cy,start in [(.1095,.1595,0),(-.1095,.1595,90),(-.1095,-.1595,180),(.1095,-.1595,270)]:
        for i in range(6):
            a=math.radians(start+i*18)
            outline.append((cx+.008*math.cos(a),cy+.008*math.sin(a)))
    n=len(outline)
    verts=[(x,y,z) for z in [0,.003] for x,y in outline]
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    board=surface('hardboard',verts,faces,boardmat)
    sheet=surface('printed_roster',[(-.105,-.152,.0034),(.105,-.152,.0034),
        (.105,.145,.0036),(-.105,.145,.0036)],[(0,1,2,3)],paper,.210,.297)
    # 인쇄 종이의 중심이 살짝 아래로 놓였으므로 UV는 종이 모서리에 맞춘다.
    for li,uv in zip(sheet.data.polygons[0].loop_indices,[(0,0),(1,0),(1,1),(0,1)]):
        sheet.data.uv_layers['ImageUV'].data[li].uv=uv
    solid=sheet.modifiers.new('sheet_edge','SOLIDIFY');solid.thickness=.00018
    parts=[board,sheet,
        ig.box('clip_base',(.11,.036,.001),(0,.145,.0045),material=steel,bevel=.001,segments=2),
        ig.box('spring_leaf',(.102,.023,.001),(0,.139,.011),rotation=(math.radians(-10),0,0),material=steel,bevel=.001,segments=2)]
    for x in [-.047,.047]:
        parts.append(ig.box('rubber_grip',(.009,.020,.004),(x,.130,.0057),material=black,bevel=.001,segments=2))
        parts.append(ig.cylinder('rivet',.0026,.001,(x,.147,.012),segments=16,material=steel))
    parts.append(ig.cylinder('spring_pin',.002,.096,(0,.154,.009),rotation=(0,math.pi/2,0),segments=16,material=steel))
    return finish('SM_WitnessStoreRoster',parts,out,[board],2048,
        'Elizabeth Richards A4 클립보드 실물과 WitnessPropsStudy_20260917.png 참고. 23.5×33.5cm, 3mm 하드보드, 낮은 금속 클립. 평일 야간·주말 오후가 구분되는 7월 마지막 주 근무표. 한 개 재질로 굽는다.')


def butts(out):
    ig.reset_scene()
    cork=ig.mat_speckle('CorkFilter',(.46,.25,.09),(.67,.49,.28),scale=1850,threshold=.56,roughness=.94,bump=.06)
    paper=ig.mat_speckle('CigarettePaper',(.69,.65,.54),(.23,.19,.12),scale=700,threshold=.69,roughness=.93,bump=.06)
    ash=ig.mat_speckle('ColdAsh',(.038,.036,.032),(.22,.21,.20),scale=1000,threshold=.6,roughness=.99,bump=.13)
    end=ig.mat_plastic('FilterEnd',(.49,.43,.29),roughness=.98,bump=.35)
    parts=[]
    placements=[(-.027,-.018,17),(.008,-.026,-38),(.031,.001,71),(-.018,.014,-15),(.008,.030,112),(-.033,.037,41)]
    # 재료 경계마다 같은 단면을 공유해 종이와 필터 사이가 벌어지지 않는다.
    for idx,(cx,cy,angle) in enumerate(placements):
        a=math.radians(angle);ca,sa=math.cos(a),math.sin(a)
        count=16
        def ring(y,rx,rz):
            points=[]
            for k in range(count):
                t=2*math.pi*k/count
                wrinkle=1+(.03 if y < .003 else .16)*math.sin(k*2.9+idx*.6+y*600)
                x=rx*math.cos(t)*wrinkle
                local_y=y+(.00015 if y < .003 else .00065)*math.sin(k*2.1+idx)
                points.append((cx+ca*x-sa*local_y,cy+sa*x+ca*local_y,.004+rz*math.sin(t)*wrinkle))
            return points
        specs=[(-.014-idx*.0004,.0038,.0038),(.003,.0037,.0036),
            (.007,.0041,.0032),(.011,.0041,.0025),(.015+idx*.0003,.0052,.0015)]
        rings=[ring(*v) for v in specs]
        for j,mat in enumerate([cork,paper,paper,ash]):
            v=rings[j]+rings[j+1]
            f=[(k,k+count,(k+1)%count+count,(k+1)%count) for k in range(count)]
            ob=surface(f'butt_{idx}_{j}',v,f,mat)
            for poly in ob.data.polygons:poly.use_smooth=True
            parts.append(ob)
        parts.append(surface(f'filter_end_{idx}',rings[0],[tuple(range(count))],end))
        parts.append(surface(f'ash_end_{idx}',rings[-1],[tuple(reversed(range(count)))],ash))
        parts.append(ig.box('ash_fragment',(.002,.0015,.0005),(cx+.008,cy-.005,.00035),material=ash))
    hull=ig.box('butts_probe',(.095,.085,.01),(0,.009,.005));hull.hide_render=True
    return finish('SM_WitnessCigaretteButts',parts,out,[hull],512,
        'Museum of Litter의 실물 사진과 WitnessPropsStudy_20260917.png 참고. 차갑게 꺼지고 끝이 눌린 꽁초 여섯 개, 개당 약 3cm. 기초 위 한 군데에 둔다. 후속 대사에 나오는 실제 개수와 일치한다.')


if __name__ == '__main__':
    out=ig.out_root_from_argv()
    envelope(out)
    clipboard(out)
    butts(out)
