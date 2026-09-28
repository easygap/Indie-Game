"""창 밖 원경과 펌프 상태등. 원경에는 추가 건물·광원을 만들지 않는다."""
import unreal


def start(name):
    asset = unreal.load_asset(f'/Game/Prototype/Materials/{name}')
    if not asset:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, '/Game/Prototype/Materials', unreal.Material, unreal.MaterialFactoryNew())
    for expr in list(LIB.get_material_expressions(asset)):
        LIB.delete_material_expression(asset, expr)
    return asset


def node(asset, kind, **props):
    n = LIB.create_material_expression(asset, getattr(unreal, 'MaterialExpression'+kind))
    for k,v in props.items(): n.set_editor_property(k,v)
    return n


def link(source, output, target, pin):
    """연결이 조용히 실패하면 샘플이 메시 UV로 돌아간다. 실패는 바로 멈춘다."""
    if not LIB.connect_material_expressions(source, output, target, pin):
        raise RuntimeError(f'재질 노드 연결 실패: {pin}')


def constant(asset, prop, value):
    if isinstance(value, tuple): n=node(asset,'Constant3Vector',constant=unreal.LinearColor(*value,1))
    else: n=node(asset,'Constant',r=value)
    LIB.connect_material_property(n,'',getattr(unreal.MaterialProperty,'MP_'+prop))


def finish(asset):
    LIB.layout_material_expressions(asset)
    LIB.recompile_material(asset)
    ASSETS.save_loaded_asset(asset)


def custom(asset, code, names, output):
    n = node(asset, 'Custom', code=code, output_type=output)
    pins = []
    for name in names:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    n.set_editor_property('inputs', pins)
    return n


def primitive_scalar(asset, name, index, default):
    """컴포넌트마다 다른 값을 게임 코드가 커스텀 프리미티브 데이터로 넘긴다."""
    n = node(asset, 'ScalarParameter', parameter_name=name, default_value=default)
    for flag, slot in (('use_custom_primitive_data', 'primitive_data_index'),
                       ('b_use_custom_primitive_data', 'primitive_data_index')):
        try:
            n.set_editor_property(flag, True)
            n.set_editor_property(slot, index)
            return n
        except Exception:
            continue
    raise RuntimeError(f'커스텀 프리미티브 데이터를 켤 수 없습니다: {name}')


def night_texture(filename, name, srgb):
    """원경은 옥상에서 화면 폭을 다 채우므로 1024 제한을 쓰지 않는다."""
    source = f'{unreal.SystemLibrary.get_project_directory()}UtilitySources/{filename}'
    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = '/Game/Prototype/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    result = unreal.load_asset(f'/Game/Prototype/Textures/{name}')
    if not result:
        raise RuntimeError(f'밤 원경 텍스처를 들이지 못했습니다: {filename}')
    result.set_editor_property('srgb', srgb)
    result.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    result.set_editor_property('max_texture_size', 4096)
    result.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    result.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    # 좌표를 재질이 시선으로 계산해서 스트리머가 필요한 밉을 짐작하지 못한다. 통째로 올려 둔다.
    result.set_editor_property('never_stream', True)
    if not srgb:
        # 창마다 문턱값이 G에 들어 있다. 색 공간 보정이나 BC1 블록이 값을 흩뜨리지 않게.
        result.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
    ASSETS.save_loaded_asset(result)
    return result


# 불빛 마스크를 풀어 사진의 불을 걷어 내고, 깨어 있는 집(A)만 다시 켠다.
# M.r 불빛 세기, M.g 창마다의 문턱, M.b 종류(0 창, 0.5 늘 켜진 불, 1 TV).
WINDOW_LIGHTS = (
    'float always = (M.b > 0.25 && M.b < 0.75) ? 1.0 : 0.0;'
    'float isTv = M.b >= 0.75 ? 1.0 : 0.0;'
    'float on = max(always, M.g <= A ? 1.0 : 0.0);'
    'float flick = 0.45 + 0.55 * frac(sin(floor(T * 7.0 + M.g * 91.0) * 12.9898) * 43758.5453);'
    'float tv = lerp(1.0, flick, isTv);'
    'float3 tint = lerp(float3(1, 1, 1), float3(0.70, 0.85, 1.30), isTv);'
    'float3 glass = float3(0.010, 0.012, 0.020);'
    'float3 dark = lerp(D.rgb, glass, M.r);'
    'return dark * Dim + D.rgb * M.r * on * tv * tint * Lit;'
)


def window_lights(d, m, out, dim, lit):
    """WINDOW_LIGHTS를 한 노드 안에서 두 번 쓴다. 중괄호로 감싸 지역 변수가 겹치지 않는다."""
    body = WINDOW_LIGHTS.replace('Dim', str(dim)).replace('Lit', str(lit))
    body = body.replace('return ', f'{out} = ')
    return f'float3 {out}; {{ float4 D = {d}; float4 M = {m}; {body} }}'


# 옥상 원경은 판이 서 있는 자리와 상관없이 시선 방향으로 그림을 찾는다. 원경이 무한히 먼
# 곳에 있는 셈이라 옥상을 걸어도 따라오지 않고, 난간과 이웃 건물만 그 앞을 지나간다.
# 방위각 90도마다 그림 한 장(동 0도, 북 90도, 서 180도, 남 270도)이고, 이음매 좌우 4도는
# 두 그림을 섞는다. 옥상 눈높이(1361)에서 수평선이 그림 높이의 0.46 줄에 오도록 맞추고,
# 100 m 떨어진 원통에 그린 셈으로 쳐서 골목에서 올려다보면 7도쯤 올라가 보인다.
SKYLINE_LOOKUP = (
    'float3 V = P - C;'
    'float h = max(length(V.xy), 0.001);'
    'float yaw = atan2(V.y, V.x) * 57.2957795;'
    'float t = V.z / h + (C.z - 1361.0) / 10000.0;'
    'float v = clamp(0.46 - 0.75 * t, 0.004, 0.996);'
    'float span = 98.0;'
    'int kk = ((int)floor((yaw + 45.0) / 90.0) + 4) % 4;'
    'float d = yaw - kk * 90.0;'
    'd -= 360.0 * round(d / 360.0);'
    'int nn = d > 0.0 ? (kk + 1) % 4 : (kk + 3) % 4;'
    'float d2 = d > 0.0 ? d - 90.0 : d + 90.0;'
    'float w = saturate((abs(d) - 41.0) / 8.0);'
    'float gyx = ddx(yaw); gyx -= 360.0 * round(gyx / 360.0);'
    'float gyy = ddy(yaw); gyy -= 360.0 * round(gyy / 360.0);'
    'float2 gx = float2(gyx / span, -0.75 * ddx(t) * 0.25);'
    'float2 gy = float2(gyy / span, -0.75 * ddy(t) * 0.25);'
    # 띠의 칸 순서는 북·동·남·서라서 동(0)·북(1)·서(2)·남(3)과 짝이 1씩 엇갈린다.
    'float2 uv1 = float2(0.5 + d / span, (v + (float)(kk ^ 1)) * 0.25);'
    'float2 uv2 = float2(0.5 + d2 / span, (v + (float)(nn ^ 1)) * 0.25);'
    'float4 D1 = TexD.SampleGrad(TexDSampler, uv1, gx, gy);'
    'float4 D2 = TexD.SampleGrad(TexDSampler, uv2, gx, gy);'
    'float4 M1 = TexM.SampleGrad(TexMSampler, uv1, gx, gy);'
    'float4 M2 = TexM.SampleGrad(TexMSampler, uv2, gx, gy);'
    '{lights1}'
    '{lights2}'
    'return float4(lerp(E1, E2, w), lerp(D1.a, D2.a, w));'
)


# 서울의 새벽 하늘은 까맣지 않다. 지평선 가까이가 도시 불빛에 탁한 주황빛으로 뜨고,
# 7월 말 04:30이면 동쪽(+X) 끝에 해 뜨기 한 시간 전의 푸른 기가 막 비친다. 이 띠가
# 있어야 원경의 건물 윤곽이 하늘을 등지고 검게 읽힌다. 원경 상자보다 한 겹 바깥에
# 두르고 더해서 그리므로, 그림의 건물이 가린 곳에는 들어가지 않는다.
# A는 동네가 깨어 있는 정도, G는 게임이 정하는 전체 세기다.
SKY_GLOW = (
    'float3 V = P - C;'
    'float h = max(length(V.xy), 0.001);'
    'float t = V.z / h + (C.z - 1361.0) / 10000.0;'
    'float yaw = atan2(V.y, V.x);'
    'float band = exp(-max(t + 0.02, 0.0) / 0.15) * saturate((t + 0.25) / 0.1);'
    'float dawn = pow(saturate(cos(yaw)), 3.0) * exp(-max(t, 0.0) / 0.3);'
    'float3 city = float3(1.0, 0.62, 0.42) * (0.75 + 0.25 * A);'
    'float3 blue = float3(0.30, 0.50, 1.0) * 1.6 * dawn;'
    'return (city + blue) * band * G;'
)


def build_sky_glow():
    mat = start('M_NightSkyGlow')
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('two_sided', True)
    glow = custom(mat, SKY_GLOW, ('P', 'C', 'A', 'G'), unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    link(node(mat, 'WorldPosition'), '', glow, 'P')
    link(node(mat, 'CameraPositionWS'), '', glow, 'C')
    link(primitive_scalar(mat, 'Awake', 0, 0.4), '', glow, 'A')
    link(primitive_scalar(mat, 'Glow', 1, 0.15), '', glow, 'G')
    LIB.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat)


# 골목 이웃 건물의 창. 유리 뒤에 방이 있는 것처럼 보이게 한다(인테리어 매핑).
# 방 한 칸은 창 밖 E cm 앞에서 정면으로 찍은 1점 투시 사진이다. 시선 광선이 깊이
# L cm의 방 상자에서 옆벽·바닥·천장·뒷벽 중 어디에 닿는지 구하고, 그 점을 사진
# 좌표로 다시 투영한다(bgolus의 원근 보정 2D 방식). 뒷벽이 사진 폭의 S를 차지하면
# E = S·L/(1-S)다. 방 종류와 불 켜짐 문턱은 창 위치의 해시로 정하고, 깨어 있는 집(A)
# 값을 원경과 같이 따른다. F가 1이면 언제나 켜져 있다.
ROOM_INTERIOR = (
    'float3 D = normalize(P - C);'
    'float s = D.y >= 0.0 ? 1.0 : -1.0;'
    # UE 5.8의 ObjectBounds는 월드 기준 절반 크기(BoxExtent)다. 다시 반으로 나누면 방이 반쪽이 된다.
    'float a = max(B.x, 0.5); float b = max(B.z, 0.5);'
    'float L = 320.0; float E = {S} / (1.0 - {S}) * L;'
    'float x = clamp(P.x - O.x, -a, a); float y = clamp(P.z - O.z, -b, b);'
    'float dx = D.x; float dy = D.z; float dz = max(abs(D.y), 0.0001);'
    'float tx = abs(dx) > 0.00001 ? ((dx > 0.0 ? a : -a) - x) / dx : 100000.0;'
    'float ty = abs(dy) > 0.00001 ? ((dy > 0.0 ? b : -b) - y) / dy : 100000.0;'
    'float t = min(min(tx, ty), L / dz);'
    'float hx = x + dx * t; float hy = y + dy * t; float hz = dz * t;'
    'float k = E / (hz + E);'
    'float2 uv = float2(0.5 - 0.5 * s * (hx / a) * k, 0.5 - 0.5 * (hy / b) * k);'
    'float pick = frac(sin(dot(O.xz, float2(12.9898, 78.233))) * 43758.5453);'
    'float room = min(floor(pick * 4.0), 3.0);'
    'float2 cell = float2(fmod(room, 2.0), floor(room / 2.0));'
    'float3 c = Tex.Sample(TexSampler, (cell + clamp(uv, 0.002, 0.998)) * 0.5).rgb;'
    'float g = 0.02 + 0.98 * frac(sin(dot(O.xz, float2(39.346, 11.135))) * 24634.6345);'
    'float lit = max(F, g <= A ? 1.0 : 0.0);'
    'float tv = room == 1.0 ? 0.55 + 0.45 * frac(sin(floor(T * 9.0 + g * 40.0) * 12.9898) * 43758.5453) : 1.0;'
    # 꺼진 방도 완전히 검지 않다. 바깥 불빛에 가구 윤곽이 희미하고, 유리에는 하늘과
    # 가로등이 옅게 비친다. 비스듬히 볼수록 반사가 세다.
    'float3 glassSheen = float3(0.020, 0.022, 0.028) + float3(0.05, 0.05, 0.055) * pow(1.0 - dz, 3.0);'
    'return c * lerp(0.04, 0.85 * tv, lit) * float3(0.93, 0.96, 1.0) + glassSheen * (1.0 - 0.6 * lit);'
)


ROOM_BACK_WALL_FRACTION = 0.5


def build_room_interior():
    atlas = night_texture('RoomInteriors_D.png', 'T_RoomInteriors_D', True)
    mat = start('M_RoomInterior')
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    # 네 사진의 뒷벽이 폭의 절반쯤을 차지한다(build_room_interior_atlas.py가 잰다).
    code = ROOM_INTERIOR.format(S=ROOM_BACK_WALL_FRACTION)
    look = custom(mat, code, ('P', 'C', 'O', 'B', 'A', 'F', 'T', 'Tex'),
                  unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    link(node(mat, 'WorldPosition'), '', look, 'P')
    link(node(mat, 'CameraPositionWS'), '', look, 'C')
    link(node(mat, 'ObjectPositionWS'), '', look, 'O')
    link(node(mat, 'ObjectBounds'), '', look, 'B')
    link(primitive_scalar(mat, 'Awake', 0, 0.8), '', look, 'A')
    link(primitive_scalar(mat, 'ForceLit', 1, 0.0), '', look, 'F')
    link(node(mat, 'Time'), '', look, 'T')
    link(node(mat, 'TextureObject', texture=atlas), '', look, 'Tex')
    LIB.connect_material_property(look, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat)


def build_night_skyline():
    """옥상 사방의 원경. 방향마다 다른 그림을 세로 띠 한 장에 묶었다."""
    color = night_texture('NightSkyline_D.png', 'T_NightSkyline_D', True)
    lights = night_texture('NightSkyline_M.png', 'T_NightSkyline_M', False)
    mat = start('M_NightSkyline')
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('opacity_mask_clip_value', 0.5)
    # 판의 안쪽 면만 보이면 되지만, 상자 모서리에서 뒷면이 비치지 않게 양면으로 둔다.
    mat.set_editor_property('two_sided', True)
    code = SKYLINE_LOOKUP.format(
        lights1=window_lights('D1', 'M1', 'E1', 0.26, 0.55),
        lights2=window_lights('D2', 'M2', 'E2', 0.26, 0.55))
    look = custom(mat, code, ('P', 'C', 'A', 'T', 'TexD', 'TexM'),
                  unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    link(node(mat, 'WorldPosition'), '', look, 'P')
    link(node(mat, 'CameraPositionWS'), '', look, 'C')
    link(primitive_scalar(mat, 'Awake', 0, 0.4), '', look, 'A')
    link(node(mat, 'Time'), '', look, 'T')
    link(node(mat, 'TextureObject', texture=color), '', look, 'TexD')
    link(node(mat, 'TextureObject', texture=lights), '', look, 'TexM')
    rgb = node(mat, 'ComponentMask', r=True, g=True, b=True, a=False)
    alpha = node(mat, 'ComponentMask', r=False, g=False, b=False, a=True)
    link(look, '', rgb, '')
    link(look, '', alpha, '')
    LIB.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    finish(mat)


def build(texture_loader, material_library, asset_subsystem):
    global texture, LIB, ASSETS
    texture, LIB, ASSETS = texture_loader, material_library, asset_subsystem
    photo=texture('ApartmentNightVista_20260916.png','T_ApartmentNightVista_D')
    if not photo: raise RuntimeError('창 밖 원경 원본이 없습니다.')
    photo.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
    photo.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
    # 창 너머 사진도 좌표를 재질이 계산한다. 창 면의 UV로 밉을 고르면 흐려진다.
    photo.set_editor_property('never_stream',True)
    ASSETS.save_loaded_asset(photo)
    photo_lights = night_texture('ApartmentNightVista_M.png', 'T_ApartmentNightVista_M', False)
    mat=start('M_ApartmentNightGlass')
    # 카메라에서 유리를 통과한 광선을 유리 너머 10m의 원경 평면으로 보낸다.
    # 두 창짝이 같은 사진 좌표를 쓰므로 가운데에서 풍경이 반복되지 않는다.
    # 남향 복도 창은 광선이 -Y로 나간다. 부호를 곱해야 좌우가 뒤집히지 않고,
    # 예전 식은 북향만 셈해서 복도 창에서는 사진 모서리 한 점으로 뭉개졌다.
    # 복도를 따라 비스듬히 보면 광선이 사진 밖으로 나간다. 잘라 붙이면 가장자리 한 줄이
    # 가로 줄무늬로 늘어나므로 q/sqrt(1+q²)로 가장자리를 눌러 사진 안에 담는다.
    position=node(mat,'WorldPosition')
    camera=node(mat,'CameraPositionWS')
    uv=node(mat,'Custom',code='float3 D=normalize(P-C); float s=D.y>=0.0?1.0:-1.0; float3 H=P+D*(1000.0/max(abs(D.y),0.05)); float2 q=float2(s*(H.x+100.0)/1200.0,(H.z-1050.0)/800.0); q*=rsqrt(1.0+q*q); return saturate(.5-.5*q);',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    pins=[]
    for name in ('P','C'):
        pin=unreal.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    uv.set_editor_property('inputs',pins)
    link(position,'',uv,'P');link(camera,'',uv,'C')
    sample=node(mat,'TextureSample',texture=photo)
    link(uv,'',sample,'UVs')
    # 창 너머 동네도 시간에 따라 불이 꺼지고 켜진다. 밝기는 예전 0.28을 그대로 쓴다.
    sample_lights=node(mat,'TextureSample',texture=photo_lights,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(uv,'',sample_lights,'UVs')
    awake=primitive_scalar(mat,'Awake',0,0.8)
    time=node(mat,'Time')
    glow=custom(mat,WINDOW_LIGHTS.replace('Dim','0.28').replace('Lit','0.28'),('D','M','A','T'),unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    link(sample,'RGBA',glow,'D')
    link(sample_lights,'RGB',glow,'M')
    link(awake,'',glow,'A')
    link(time,'',glow,'T')
    LIB.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    constant(mat,'BASE_COLOR',(.008,.012,.014));constant(mat,'ROUGHNESS',.16);constant(mat,'SPECULAR',.55)
    finish(mat)
    build_night_skyline()
    build_sky_glow()
    build_room_interior()
    mat=start('M_PumpIndicator')
    tint=node(mat,'VectorParameter',parameter_name='Tint',default_value=unreal.LinearColor(.02,.24,.04,1))
    strength=node(mat,'ScalarParameter',parameter_name='Lit',default_value=0.)
    emission=node(mat,'Multiply')
    link(tint,'',emission,'A');link(strength,'',emission,'B')
    LIB.connect_material_property(tint,'',unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    constant(mat,'ROUGHNESS',.26);constant(mat,'SPECULAR',.5)
    finish(mat)
    unreal.log('INTERIOR_MATERIALS PASS materials=5')
