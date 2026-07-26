"""Diagnose why the bottle label material renders blank."""

import unreal


def log(message):
    unreal.log_warning(f"[LABELPROBE] {message}")


def probe():
    for texture_name in ("T_LabelWater_D", "T_PosterSale_D"):
        texture = unreal.load_asset(f"/Game/Prototype/Textures/{texture_name}")
        if texture is None:
            log(f"{texture_name}: MISSING")
            continue
        log(f"{texture_name}: size={texture.blueprint_get_size_x()}x"
            f"{texture.blueprint_get_size_y()} "
            f"srgb={texture.get_editor_property('srgb')} "
            f"comp={texture.get_editor_property('compression_settings')}")

    for material_name in ("M_LabelWater", "M_PosterSale"):
        material = unreal.load_asset(f"/Game/Prototype/Materials/{material_name}")
        if material is None:
            log(f"{material_name}: MISSING")
            continue
        expressions = unreal.MaterialEditingLibrary.get_expressions(material) \
            if hasattr(unreal.MaterialEditingLibrary, "get_expressions") else None
        log(f"{material_name}: loaded, expressions={expressions}")
        for prop in (unreal.MaterialProperty.MP_BASE_COLOR,
                     unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            try:
                connected = unreal.MaterialEditingLibrary.get_material_property_input_node(
                    material, prop)
                log(f"  {material_name}.{prop}: {connected}")
            except Exception as error:  # noqa: BLE001
                log(f"  {material_name}.{prop}: query failed ({error})")

    sleeve = unreal.load_asset("/Game/Meshes/SM_LabelSleeve")
    if sleeve:
        log(f"SM_LabelSleeve materials={sleeve.get_num_sections(0)} "
            f"lods={sleeve.get_num_lods()}")
        log(f"SM_LabelSleeve static_materials={len(sleeve.static_materials)}")
    else:
        log("SM_LabelSleeve: MISSING")

    log("done")


if __name__ == "__main__":
    probe()
