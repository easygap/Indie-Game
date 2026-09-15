"""실제 매장 사진에 맞춘 자기질 타일과 도장 벽 마감. 치수 단위는 cm."""

import unreal
import os

MARKER = "IG_RetailFinish_20260914"


def floor_albedo(finish):
    """생성 원본을 직접 가져온다. 예전 타일 스캔으로 덮어쓰지 않는다."""
    name = "PocheonGranite" if finish == "granite" else "PorcelainStore"
    asset_name = f"T_{name}_20260915_D"
    source = os.path.join(unreal.SystemLibrary.get_project_directory(),
                          "Content", "SourceArt", "AI", f"{name}_20260915.png")
    if not os.path.isfile(source):
        raise RuntimeError(f"바닥 재질 원본이 없다: {source}")
    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = "/Game/Prototype/Textures"
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(f"{task.destination_path}/{asset_name}")
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    # 생성 원본은 비정규 크기일 수 있다. 패딩 대신 늘여서 반복 경계를 보존하고
    # 실제 게임에서는 1K까지 사용한다. 밉과 이방성 필터는 엔진이 만든다.
    texture.set_editor_property("power_of_two_mode", unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
    texture.set_editor_property("max_texture_size", 1024)
    texture.set_editor_property("never_stream", False)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(texture)
    return texture


def author(material, finish):
    # 재반입은 이 텍스처를 쓰는 재질을 갱신한다. 노드를 만드는 도중 실행하면
    # 막 연결한 그래프가 이전 저장 상태로 돌아갈 수 있으므로 먼저 끝낸다.
    albedo = floor_albedo(finish) if finish in ("floor", "granite") else None
    lib = unreal.MaterialEditingLibrary
    # UE 5.8의 일괄 삭제는 순회 중 원본 배열을 줄여 일부 노드를 남긴다.
    # 목록을 복사해 하나씩 지워야 재실행 때 텍스처 샘플이 쌓이지 않는다.
    for expression in list(lib.get_material_expressions(material)):
        lib.delete_material_expression(material, expression)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("tangent_space_normal", True)

    def node(kind, **properties):
        obj = lib.create_material_expression(material, getattr(unreal, kind))
        for key, value in properties.items():
            obj.set_editor_property(key, value)
        return obj

    def scalar(value):
        return node("MaterialExpressionConstant", r=value)

    def color(value):
        return node("MaterialExpressionConstant3Vector", constant=unreal.LinearColor(*value, 1))

    def link(a, b, slot):
        if lib.connect_material_expressions(a, "", b, slot) is False:
            raise RuntimeError(f"재질 노드 연결 실패: {b.get_name()} / {slot}")

    base = color((.68, .69, .665) if finish == "wall" else (.61, .59, .54))
    rough = scalar(.42 if finish == "wall" else .31)
    if finish in ("floor", "granite"):
        # 600mm 실물 사진을 기준으로 한 색상 원본이다. 연마면의 검은 광물은
        # 구멍이 아니므로 색의 명암에서 높이·노멀을 만들지 않는다.
        position = node("MaterialExpressionWorldPosition")
        axes = node("MaterialExpressionComponentMask", r=True, g=True, b=False, a=False)
        link(position, axes, "")
        uv = node("MaterialExpressionDivide", const_b=60.)
        link(axes, uv, "A")
        base = node("MaterialExpressionTextureSample", texture=albedo)
        link(uv, base, "UVs")
        rough = scalar(.36 if finish == "granite" else .34)
    if finish == "gypsum":
        # 넓은 면은 아이보리 원지, 절단면은 회백색 석고다. 옆면에 평면 투영을 늘리지 않는다.
        normal = node("MaterialExpressionVertexNormalWS")
        axis = node("MaterialExpressionComponentMask", r=False, g=False, b=True, a=False)
        link(normal, axis, "")
        face = node("MaterialExpressionAbs")
        link(axis, face, "")
        paper = node("MaterialExpressionLinearInterpolate")
        link(color((.48, .46, .42)), paper, "A")
        link(color((.69, .65, .55)), paper, "B")
        link(face, paper, "Alpha")
        random = node("MaterialExpressionPerInstanceRandom")
        variation = node("MaterialExpressionMultiply", const_b=.06)
        link(random, variation, "A")
        brightness = node("MaterialExpressionAdd", const_b=.97)
        link(variation, brightness, "A")
        base = node("MaterialExpressionMultiply")
        link(paper, base, "A")
        link(brightness, base, "B")
        rough = scalar(.9)
    if finish not in ("wall", "gypsum"):
        # 반복 전 좌표의 미분으로 픽셀 면적을 구한다. frac의 경계에서 미분하면
        # 줄눈 한 줄이 타일 너비만큼 번지거나 먼 바닥에서 반짝인다.
        position = node("MaterialExpressionWorldPosition")
        grid = node("MaterialExpressionCustom", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                    code="float2 uv=P.xy/60.0; float2 w=max(fwidth(uv),1e-5); float2 d=abs(frac(uv+0.5)-0.5); float2 a=saturate((0.001667-d)/w+0.5); a=lerp(a,float2(0.003333,0.003333),saturate(w*2.0-1.0)); return 1.0-(1.0-a.x)*(1.0-a.y);",
                    description="600mm 타일 / 2mm 줄눈")
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", "P")
        grid.set_editor_property("inputs", [pin])
        link(position, grid, "P")
        if finish == "ceiling":
            base = color((.73, .735, .705))
            rough = scalar(.82)
        seam = color((.24, .245, .235) if finish in ("floor", "granite") else (.57, .58, .55))
        blend = node("MaterialExpressionLinearInterpolate")
        link(base, blend, "A")
        link(seam, blend, "B")
        link(grid, blend, "Alpha")
        base = blend
        rough_mix = node("MaterialExpressionLinearInterpolate")
        link(rough, rough_mix, "A")
        link(scalar(.78), rough_mix, "B")
        link(grid, rough_mix, "Alpha")
        rough = rough_mix
    lib.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # 기존 그래프의 노드를 지워도 출력 핀 참조는 남을 수 있어 모두 다시 연결한다.
    lib.connect_material_property(color((0., 0., 1.)), "", unreal.MaterialProperty.MP_NORMAL)
    lib.connect_material_property(scalar(0.), "", unreal.MaterialProperty.MP_METALLIC)
    lib.connect_material_property(scalar(1.), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    lib.connect_material_property(color((0., 0., 0.)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    marker = node("MaterialExpressionScalarParameter", parameter_name=MARKER, default_value=1.)
    specular = node("MaterialExpressionMultiply")
    link(marker, specular, "A")
    link(scalar(.45), specular, "B")
    lib.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    lib.layout_material_expressions(material)
    lib.recompile_material(material)
    return material
