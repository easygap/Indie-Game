"""Create the small, deterministic material set used by the prologue greybox."""

import unreal


MATERIAL_ROOT = "/Game/Prototype/Materials"

MATERIALS = {
    # --- apartment ---------------------------------------------------------
    "M_RoomWall": {
        "base": (0.10, 0.14, 0.19, 1.0),
        "roughness": 0.94,
    },
    "M_RoomFloor": {
        # Vinyl jangpan sheet flooring: warm, slightly glossy under lamp light.
        "base": (0.09, 0.035, 0.02, 1.0),
        "roughness": 0.48,
    },
    "M_DarkWood": {
        "base": (0.13, 0.025, 0.01, 1.0),
        "roughness": 0.62,
    },
    "M_Bedding": {
        "base": (0.09, 0.12, 0.16, 1.0),
        "roughness": 0.98,
    },
    "M_Door": {
        "base": (0.035, 0.045, 0.07, 1.0),
        "roughness": 0.48,
    },
    "M_Alarm": {
        "base": (0.004, 0.003, 0.003, 1.0),
        "roughness": 0.35,
        "emissive": (0.35, 0.002, 0.001, 1.0),
    },
    "M_FridgeBody": {
        "base": (0.72, 0.73, 0.71, 1.0),
        "roughness": 0.45,
    },
    "M_FridgeInterior": {
        "base": (0.85, 0.87, 0.86, 1.0),
        "roughness": 0.6,
    },
    "M_WalletBrown": {
        "base": (0.12, 0.06, 0.03, 1.0),
        "roughness": 0.7,
    },
    "M_WindowGlow": {
        "base": (0.02, 0.03, 0.06, 1.0),
        "roughness": 0.2,
        "emissive": (0.045, 0.07, 0.13, 1.0),
    },
    # --- alley -------------------------------------------------------------
    "M_Asphalt": {
        "base": (0.028, 0.028, 0.032, 1.0),
        "roughness": 0.95,
    },
    "M_Concrete": {
        "base": (0.16, 0.155, 0.15, 1.0),
        "roughness": 0.9,
    },
    "M_ConcreteDark": {
        "base": (0.055, 0.055, 0.062, 1.0),
        "roughness": 0.92,
    },
    "M_WindowDark": {
        "base": (0.01, 0.012, 0.02, 1.0),
        "roughness": 0.15,
        "emissive": (0.015, 0.022, 0.04, 1.0),
    },
    "M_NightSky": {
        "base": (0.005, 0.008, 0.018, 1.0),
        "roughness": 1.0,
        "emissive": (0.012, 0.02, 0.045, 1.0),
    },
    "M_StreetLampGlow": {
        "base": (0.1, 0.07, 0.03, 1.0),
        "roughness": 0.4,
        "emissive": (3.0, 2.0, 0.9, 1.0),
    },
    "M_TrashBag": {
        "base": (0.045, 0.05, 0.045, 1.0),
        "roughness": 0.4,
    },
    "M_Cardboard": {
        "base": (0.32, 0.20, 0.09, 1.0),
        "roughness": 0.9,
    },
    # --- convenience store -------------------------------------------------
    "M_StoreFloor": {
        # Polished tile: glossy enough for Lumen to mirror the ceiling lights.
        "base": (0.55, 0.54, 0.50, 1.0),
        "roughness": 0.18,
    },
    "M_LightPanel": {
        "base": (0.9, 0.9, 0.88, 1.0),
        "roughness": 0.6,
        "emissive": (3.2, 3.15, 2.95, 1.0),
    },
    "M_SignMint": {
        "base": (0.02, 0.10, 0.08, 1.0),
        "roughness": 0.4,
        "emissive": (0.15, 2.6, 1.7, 1.0),
    },
    "M_SignWhite": {
        "base": (0.8, 0.8, 0.78, 1.0),
        "roughness": 0.4,
        "emissive": (2.4, 2.4, 2.2, 1.0),
    },
    "M_Glass": {
        # Clear glass: a bright base colour turns translucency into milk, so
        # the tint stays dark and the surface reads through specular instead.
        "base": (0.09, 0.12, 0.13, 1.0),
        "roughness": 0.04,
        "opacity": 0.10,
    },
    "M_MetalFrame": {
        "base": (0.35, 0.36, 0.38, 1.0),
        "roughness": 0.4,
        "metallic": 0.9,
    },
    "M_PlasticDark": {
        "base": (0.04, 0.04, 0.045, 1.0),
        "roughness": 0.5,
    },
    "M_CounterTop": {
        "base": (0.48, 0.46, 0.43, 1.0),
        "roughness": 0.28,
    },
    "M_CoolerBody": {
        "base": (0.20, 0.25, 0.30, 1.0),
        "roughness": 0.5,
    },
    "M_ScreenGlow": {
        "base": (0.02, 0.02, 0.02, 1.0),
        "roughness": 0.3,
        "emissive": (0.15, 0.5, 0.7, 1.0),
    },
    "M_WaterBlue": {
        "base": (0.15, 0.35, 0.60, 1.0),
        "roughness": 0.2,
        "emissive": (0.02, 0.05, 0.09, 1.0),
    },
    "M_BottleGreen": {
        "base": (0.10, 0.40, 0.15, 1.0),
        "roughness": 0.25,
    },
    "M_BottleBrown": {
        "base": (0.25, 0.12, 0.04, 1.0),
        "roughness": 0.3,
    },
    "M_SnackRed": {
        "base": (0.50, 0.06, 0.04, 1.0),
        "roughness": 0.6,
    },
    "M_SnackYellow": {
        "base": (0.60, 0.45, 0.05, 1.0),
        "roughness": 0.6,
    },
    "M_SnackBlue": {
        "base": (0.08, 0.20, 0.50, 1.0),
        "roughness": 0.6,
    },
    "M_CupNoodle": {
        "base": (0.70, 0.65, 0.55, 1.0),
        "roughness": 0.7,
    },
    "M_ConeOrange": {
        "base": (0.80, 0.22, 0.03, 1.0),
        "roughness": 0.55,
    },
}


def _connect_constant(material, expression_class, value, material_property):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class
    )
    if expression is None:
        raise RuntimeError(f"Could not create expression for {material.get_path_name()}")

    if expression_class is unreal.MaterialExpressionConstant3Vector:
        expression.set_editor_property("constant", unreal.LinearColor(*value))
    else:
        expression.set_editor_property("r", value)

    if not unreal.MaterialEditingLibrary.connect_material_property(
        expression, "", material_property
    ):
        raise RuntimeError(
            f"Could not connect {expression_class} on {material.get_path_name()}"
        )


def create_materials():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    if assets is None or tools is None:
        raise RuntimeError("Unreal editor asset services are unavailable")

    loaded_materials = []
    for name, settings in MATERIALS.items():
        asset_path = f"{MATERIAL_ROOT}/{name}"
        if assets.does_asset_exist(asset_path) and not assets.delete_asset(asset_path):
            raise RuntimeError(f"Could not replace material: {asset_path}")

        material = tools.create_asset(
            name, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
        )
        if material is None:
            raise RuntimeError(f"Could not create material: {asset_path}")

        if "opacity" in settings:
            material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
            material.set_editor_property(
                "translucency_lighting_mode",
                unreal.TranslucencyLightingMode.TLM_SURFACE,
            )

        _connect_constant(
            material,
            unreal.MaterialExpressionConstant3Vector,
            settings["base"],
            unreal.MaterialProperty.MP_BASE_COLOR,
        )
        _connect_constant(
            material,
            unreal.MaterialExpressionConstant,
            settings["roughness"],
            unreal.MaterialProperty.MP_ROUGHNESS,
        )
        if "metallic" in settings:
            _connect_constant(
                material,
                unreal.MaterialExpressionConstant,
                settings["metallic"],
                unreal.MaterialProperty.MP_METALLIC,
            )
        if "opacity" in settings:
            _connect_constant(
                material,
                unreal.MaterialExpressionConstant,
                settings["opacity"],
                unreal.MaterialProperty.MP_OPACITY,
            )
        if "emissive" in settings:
            _connect_constant(
                material,
                unreal.MaterialExpressionConstant3Vector,
                settings["emissive"],
                unreal.MaterialProperty.MP_EMISSIVE_COLOR,
            )

        unreal.MaterialEditingLibrary.layout_material_expressions(material)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.log(f"[IndieGame] Created material: {asset_path}")

        loaded_materials.append(material)

    if not assets.save_loaded_assets(loaded_materials, False):
        raise RuntimeError("Could not save one or more prototype materials")


if __name__ == "__main__":
    create_materials()
