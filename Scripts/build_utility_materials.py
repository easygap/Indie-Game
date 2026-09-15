"""설비 표면과 실제 게임 화면을 쓰는 관리실 모니터 재질."""

from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.SystemLibrary.get_project_directory())
sys.path.insert(0, str(ROOT / "Scripts"))
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def texture(filename, name):
    source = ROOT / "UtilitySources" / filename
    if not source.is_file():
        return None
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = "/Game/Prototype/Textures"
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    result = unreal.load_asset(f"{task.destination_path}/{name}")
    result.set_editor_property("srgb", True)
    result.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    result.set_editor_property("power_of_two_mode", unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
    result.set_editor_property("max_texture_size", 1024)
    result.set_editor_property("never_stream", False)
    result.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    ASSETS.save_loaded_asset(result)
    return result


steel = texture("TankSatinSteel_20260915.png", "T_UtilityTankSteel_D")
screen = texture("CctvStandby.png", "T_CctvStandby_D")
if not steel or not screen:
    raise RuntimeError("저수조 원본이나 실제 맵 CCTV 아틀라스가 없습니다.")


def material(name, surface):
    asset = unreal.load_asset(f"/Game/Prototype/Materials/{name}")
    if not asset:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, "/Game/Prototype/Materials", unreal.Material, unreal.MaterialFactoryNew())
    for expression in list(LIB.get_material_expressions(asset)):
        LIB.delete_material_expression(asset, expression)

    def node(kind, **properties):
        result = LIB.create_material_expression(asset, getattr(unreal, kind))
        for key, value in properties.items():
            result.set_editor_property(key, value)
        return result

    def scalar(value):
        return node("MaterialExpressionConstant", r=value)

    def color(value):
        return node("MaterialExpressionConstant3Vector", constant=unreal.LinearColor(*value, 1))

    def link(a, b, pin):
        if not LIB.connect_material_expressions(a, "", b, pin):
            raise RuntimeError(f"연결 실패: {name} / {pin}")

    def output(a, pin):
        LIB.connect_material_property(a, "", getattr(unreal.MaterialProperty, pin))

    asset.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    asset.set_editor_property("tangent_space_normal", True)
    asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT if surface == "screen" else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    asset.set_editor_property("two_sided", False)
    base = color((.002, .004, .004))
    if surface == "screen":
        if screen:
            base = node("MaterialExpressionTextureSample", texture=screen)
        output(base, "MP_EMISSIVE_COLOR")
    else:
        position = node("MaterialExpressionWorldPosition")
        normal = node("MaterialExpressionVertexNormalWS")
        # 원통 옆면은 실제 둘레 길이로, 기초는 각 면의 축으로 투영한다.
        # 바닥 XY 투영을 옆면에 늘이던 세로 줄무늬가 생기지 않는다.
        tile = 210.0 if surface == "street_brick" else 190.0 if surface == "brick" else 60.0 if surface == "granite" else 260.0 if surface == "dark" else 150.0
        code = ("return abs(N.z)>.707 ? P.xy/80.0 : float2(atan2(P.y+25.0,P.x)*153.0/80.0,P.z/80.0);"
                if surface == "steel" else
                f"float3 n=abs(N); return (n.z>.707 ? P.xy : (n.x>n.y ? P.yz : P.xz))/{tile};")
        uv = node("MaterialExpressionCustom", code=code,
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2)
        inputs = []
        for label in ("P", "N"):
            pin = unreal.CustomInput()
            pin.set_editor_property("input_name", label)
            inputs.append(pin)
        uv.set_editor_property("inputs", inputs)
        link(position, uv, "P")
        link(normal, uv, "N")
        image = steel if surface == "steel" else unreal.load_asset(
            "/Game/Prototype/Textures/T_Photo_Brick_D" if surface == "street_brick" else
            "/Game/Prototype/Textures/T_Photo_VillaBrick_D" if surface == "brick" else
            "/Game/Prototype/Textures/T_PocheonGranite_20260915_D" if surface == "granite" else "/Game/Prototype/Textures/T_Concrete_D")
        if not image:
            raise RuntimeError(f"설비 표면 원본 텍스처가 없습니다: {surface}")
        base = node("MaterialExpressionTextureSample", texture=image)
        link(uv, base, "UVs")
        if surface in ("dark", "brick"):
            tinted = node("MaterialExpressionMultiply")
            link(base, tinted, "A")
            link(color((.86, .80, .76) if surface == "brick" else (.32, .33, .36)), tinted, "B")
            base = tinted
        output(base, "MP_BASE_COLOR")
        output(color((0., 0., 0.)), "MP_EMISSIVE_COLOR")
    output(scalar(.40 if surface == "steel" else .36 if surface == "granite" else .86), "MP_ROUGHNESS")
    output(scalar(.95 if surface == "steel" else 0.), "MP_METALLIC")
    # 색에 찍힌 얼룩은 높이가 아니다. 금속을 자갈처럼 울퉁불퉁하게 만들지 않는다.
    output(color((0., 0., 1.)), "MP_NORMAL")
    output(scalar(1.), "MP_AMBIENT_OCCLUSION")
    LIB.layout_material_expressions(asset)
    LIB.recompile_material(asset)
    ASSETS.save_loaded_asset(asset)


material("M_UtilityTankSteel", "steel")
material("M_UtilityFoundation", "foundation")
material("M_UtilityGraniteCladding", "granite")
material("M_UtilityConcreteDark", "dark")
material("M_CctvStandby", "screen")
material("M_UtilityVillaBrick", "brick")
material("M_UtilityStreetBrick", "street_brick")


def meter_print(kind):
    image = texture(f"Meter{kind}.png", f"T_UtilityMeter{kind}_D")
    if not image:
        raise RuntimeError("build_utility_prints.py를 먼저 실행하세요.")
    name = f"M_UtilityMeter{kind}"
    asset = unreal.load_asset(f"/Game/Prototype/Materials/{name}")
    if not asset:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, "/Game/Prototype/Materials", unreal.Material, unreal.MaterialFactoryNew())
    for expression in list(LIB.get_material_expressions(asset)):
        LIB.delete_material_expression(asset, expression)
    coords = LIB.create_material_expression(asset, unreal.MaterialExpressionTextureCoordinate)
    row = LIB.create_material_expression(asset, unreal.MaterialExpressionScalarParameter)
    row.set_editor_property("parameter_name", "MeterIndex")
    crop = LIB.create_material_expression(asset, unreal.MaterialExpressionCustom)
    crop.set_editor_property("code", "return float2(U.x,(U.y+I)/8.0);")
    crop.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    pins = []
    for label in ("U", "I"):
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", label)
        pins.append(pin)
    crop.set_editor_property("inputs", pins)
    LIB.connect_material_expressions(coords, "", crop, "U")
    LIB.connect_material_expressions(row, "", crop, "I")
    sample = LIB.create_material_expression(asset, unreal.MaterialExpressionTextureSample)
    sample.set_editor_property("texture", image)
    LIB.connect_material_expressions(crop, "", sample, "UVs")
    LIB.connect_material_property(sample, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = LIB.create_material_expression(asset, unreal.MaterialExpressionConstant)
    rough.set_editor_property("r", .55)
    LIB.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    LIB.layout_material_expressions(asset)
    LIB.recompile_material(asset)
    ASSETS.save_loaded_asset(asset)


meter_print("Counter")
meter_print("Label")
for name in ("BoothAgentNote", "BoothReceipts", "BoothCalendar"):
    image = texture(f"{name}.png", f"T_{name}_D")
    if not image:
        raise RuntimeError(f"인쇄 원본 없음: {name}")
    asset = unreal.load_asset(f"/Game/Prototype/Materials/M_{name}")
    if not asset:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            f"M_{name}", "/Game/Prototype/Materials", unreal.Material, unreal.MaterialFactoryNew())
    for expression in list(LIB.get_material_expressions(asset)):
        LIB.delete_material_expression(asset, expression)
    sample = LIB.create_material_expression(asset, unreal.MaterialExpressionTextureSample)
    sample.set_editor_property("texture", image)
    LIB.connect_material_property(sample, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = LIB.create_material_expression(asset, unreal.MaterialExpressionConstant)
    rough.set_editor_property("r", .9)
    LIB.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    LIB.recompile_material(asset)
    ASSETS.save_loaded_asset(asset)
# 계단 챌판을 포함한 석재 타일은 전체 아트 빌드와 같은 저작 함수를 쓴다.
import retail_surface_contract
granite_tile = unreal.load_asset("/Game/Prototype/Materials/M_GraniteTile_XY")
if not granite_tile:
    raise RuntimeError("석재 타일 재질이 없습니다.")
retail_surface_contract.author(granite_tile, "granite")
ASSETS.save_loaded_asset(granite_tile)
unreal.log(f"UTILITY_MATERIALS PASS materials=13 cctv_atlas={int(screen is not None)}")
