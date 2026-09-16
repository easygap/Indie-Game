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


def constant(asset, prop, value):
    if isinstance(value, tuple): n=node(asset,'Constant3Vector',constant=unreal.LinearColor(*value,1))
    else: n=node(asset,'Constant',r=value)
    LIB.connect_material_property(n,'',getattr(unreal.MaterialProperty,'MP_'+prop))


def finish(asset):
    LIB.layout_material_expressions(asset)
    LIB.recompile_material(asset)
    ASSETS.save_loaded_asset(asset)


def build(texture_loader, material_library, asset_subsystem):
    global texture, LIB, ASSETS
    texture, LIB, ASSETS = texture_loader, material_library, asset_subsystem
    photo=texture('ApartmentNightVista_20260916.png','T_ApartmentNightVista_D')
    if not photo: raise RuntimeError('창 밖 원경 원본이 없습니다.')
    photo.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
    photo.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
    ASSETS.save_loaded_asset(photo)
    mat=start('M_ApartmentNightGlass')
    # 카메라에서 유리를 통과한 광선을 10m 뒤의 원경 평면으로 보낸다.
    # 두 창짝이 같은 사진 좌표를 쓰므로 가운데에서 풍경이 반복되지 않는다.
    position=node(mat,'WorldPosition')
    camera=node(mat,'CameraPositionWS')
    uv=node(mat,'Custom',code='float3 D=P-C; float t=(1215.0-C.y)/max(D.y,1.0); float3 H=C+D*t; return saturate(float2(.5-(H.x+100.0)/2400.0,.5-(H.z-1050.0)/1600.0));',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    pins=[]
    for name in ('P','C'):
        pin=unreal.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    uv.set_editor_property('inputs',pins)
    LIB.connect_material_expressions(position,'',uv,'P');LIB.connect_material_expressions(camera,'',uv,'C')
    sample=node(mat,'TextureSample',texture=photo)
    LIB.connect_material_expressions(uv,'',sample,'Coordinates')
    dim=node(mat,'Multiply',const_b=.28)
    LIB.connect_material_expressions(sample,'RGB',dim,'A')
    LIB.connect_material_property(dim,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    constant(mat,'BASE_COLOR',(.008,.012,.014));constant(mat,'ROUGHNESS',.16);constant(mat,'SPECULAR',.55)
    finish(mat)
    mat=start('M_PumpIndicator')
    tint=node(mat,'VectorParameter',parameter_name='Tint',default_value=unreal.LinearColor(.02,.24,.04,1))
    strength=node(mat,'ScalarParameter',parameter_name='Lit',default_value=0.)
    emission=node(mat,'Multiply')
    LIB.connect_material_expressions(tint,'',emission,'A');LIB.connect_material_expressions(strength,'',emission,'B')
    LIB.connect_material_property(tint,'',unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    constant(mat,'ROUGHNESS',.26);constant(mat,'SPECULAR',.5)
    finish(mat)
    unreal.log('INTERIOR_MATERIALS PASS materials=2')
