"""실제 매장 사진에 맞춘 자기질 타일과 도장 벽 마감. 치수 단위는 cm."""

import unreal

MARKER = "IG_RetailFinish_20260914"


def author(material, finish):
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

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
        # 실제 줄눈 폭 2mm. 월드 좌표라 블록 크기를 바꿔도 타일은 600mm다.
        position = node("MaterialExpressionWorldPosition")
        grid = node("MaterialExpressionCustom", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                    code="float2 t=frac(P.xy/60.0); float2 d=min(t,1-t); float w=max(fwidth(t.x),fwidth(t.y)); return 1-smoothstep(0.0017,0.0033+w,min(d.x,d.y));",
                    description="600mm 타일 / 2mm 줄눈")
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", "P")
        grid.set_editor_property("inputs", [pin])
        link(position, grid, "P")
        if finish == "ceiling":
            base = color((.73, .735, .705))
            rough = scalar(.82)
        seam = color((.27, .275, .25) if finish == "floor" else (.57, .58, .55))
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
