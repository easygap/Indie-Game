"""Headless validation for generated meshes, PBR textures, and materials."""

from __future__ import annotations

import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from photo_prop_lod_contract import inspect_photo_prop_lods


MESH_NAMES = (
    "SM_AlleyCatRun",
    "SM_FirstPersonHoodieSleeve",
    "SM_P3ServiceCabinetShell",
    "SM_P3ServiceManifold",
    "SM_P3ValveWheelLarge",
    "SM_P3ValveWheelSmall",
    "SM_P3PressureGauge",
    "SM_SubmergedHoodieCurl",
    "SM_SubmergedPantsCurl",
    "SM_SubmergedSlippersCurl",
    "SM_RooftopWaterTankShell",
    "SM_TankInternalLining",
    "SM_RooftopTankPipeCluster",
    "SM_TankInternalLadder",
    "SM_TankAccessGuardRail",
    "SM_TankAccessDeck",
    "SM_TankAccessLid",
    "SM_RooftopServiceHose",
    "SM_HoseCoupling",
    "SM_CarrierBagCollapsed",
    "SM_RooftopFireDoorLeaf",
    "SM_RooftopFireDoorFrame",
    "SM_RooftopUnlockedPadlockKeys",
    "SM_TankExteriorAccessStair",
    "SM_LadderFailureRung",
    "SM_LadderRungPadLifted",
    "SM_LadderRungRetainingClips",
    "SM_HornRimGlasses",
    "SM_InspectionRod",
    "SM_CrackedPhone",
    "SM_OfferingWaterBowl",
)

HERO_MESHES = {
    "SM_AlleyCatRun",
    "SM_FirstPersonHoodieSleeve",
    "SM_HornRimGlasses",
    "SM_InspectionRod",
    "SM_CrackedPhone",
    "SM_CarrierBagCollapsed",
    "SM_LadderFailureRung",
    "SM_LadderRungPadLifted",
    "SM_LadderRungRetainingClips",
    "SM_P3ValveWheelLarge",
    "SM_P3ValveWheelSmall",
    "SM_P3PressureGauge",
    "SM_OfferingWaterBowl",
}

PBR_STEMS = {
    "T_ApartmentWallpaperV2": ("D", "N", "R", "A"),
    "T_WetHoodie": ("D", "N", "R", "A", "W"),
    "T_AlleyCatTabby": ("D", "N", "R", "A"),
    "T_WaterTankGalvanized": ("D", "N", "R", "A", "W", "M"),
    "T_TankInteriorBiofilm": ("D", "N", "R", "A", "W", "M"),
    "T_WetServiceHose": ("D", "N", "R", "A", "W"),
    "T_WetRungPad": ("D", "N", "R", "A", "W"),
    "T_P3CabinetPaintedSteel": ("D", "N", "R", "A", "W"),
    "T_CarrierBagFilm": ("D", "N", "R", "A"),
    "T_TankWaterSurface": ("D", "N", "R", "A"),
}

MATERIAL_TEXTURES = {
    "M_Wallpaper_X": "T_ApartmentWallpaperV2",
    "M_Wallpaper_Y": "T_ApartmentWallpaperV2",
    "M_WallpaperCeil": "T_ApartmentWallpaperV2",
    "M_WetHoodieUV": "T_WetHoodie",
    "M_SubmergedHoodieUV": "T_WetHoodie",
    "M_SubmergedPantsUV": "T_WetHoodie",
    "M_SubmergedSlippersUV": "T_WetServiceHose",
    "M_SubmergedSlipperWearUV": "T_WetRungPad",
    "M_AlleyCatTabbyUV": "T_AlleyCatTabby",
    "M_WaterTankMetalUV": "T_WaterTankGalvanized",
    "M_TankInteriorBiofilmUV": "T_TankInteriorBiofilm",
    "M_WetServiceHoseUV": "T_WetServiceHose",
    "M_WetRungPadUV": "T_WetRungPad",
    "M_P3CabinetMetalUV": "T_P3CabinetPaintedSteel",
    "M_CarrierBagFilm": "T_CarrierBagFilm",
    "M_TankWaterReveal": "T_TankWaterSurface",
}

MASK_MATERIALS = {
    "M_ApartmentWallPatina": "T_ApartmentWallPatina_M",
    "M_EvidenceSlipperTrail": "T_EvidenceSlipperTrail_M",
    "M_EvidenceCatPawTrail": "T_EvidenceCatPawTrail_M",
    "M_EvidenceHoseDrag": "T_EvidenceHoseDrag_M",
    "M_EvidenceHandSmear": "T_EvidenceHandSmear_M",
    "M_DecalDampWallpaper": "T_DecalDampWallpaper_D",
    "M_DecalRustFasteners": "T_DecalRustFasteners_D",
    "M_DecalMineralScale": "T_DecalMineralScale_D",
    "M_DecalRainGrime": "T_DecalRainGrime_D",
}

EVIDENCE_MASK_MATERIALS = {
    "M_EvidenceSlipperTrail": "T_EvidenceSlipperTrail_M",
    "M_EvidenceCatPawTrail": "T_EvidenceCatPawTrail_M",
    "M_EvidenceHoseDrag": "T_EvidenceHoseDrag_M",
    "M_EvidenceHandSmear": "T_EvidenceHandSmear_M",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def load(path: str, expected_type):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    require(asset is not None, f"Missing asset: {path}")
    require(isinstance(asset, expected_type), f"Wrong asset class: {path}")
    return asset


def texture_path(texture) -> str:
    return texture.get_path_name().split(".", 1)[0]


def expression_texture_paths(material) -> set[str]:
    """Read persisted graph nodes; this also works when NullRHI has no resource."""
    paths = set()
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    for expression in expressions:
        try:
            texture = expression.get_editor_property("texture")
        except Exception:  # noqa: BLE001 - most expressions are not texture nodes
            continue
        if texture is not None:
            paths.add(texture_path(texture))
    return paths


def texture_sample_types(material) -> dict[str, str]:
    """Return persisted sampler types keyed by the linked texture path."""
    sample_types = {}
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    for expression in expressions:
        if not isinstance(expression, unreal.MaterialExpressionTextureSample):
            continue
        texture = expression.get_editor_property("texture")
        if texture is not None:
            sample_types[texture_path(texture)] = str(
                expression.get_editor_property("sampler_type")
            )
    return sample_types


def validate_meshes() -> tuple[int, int]:
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    require(subsystem is not None, "StaticMeshEditorSubsystem is unavailable")
    total_lods = 0
    reduced_meshes = 0
    for name in MESH_NAMES:
        mesh = load(f"/Game/Meshes/{name}", unreal.StaticMesh)
        lod_count = subsystem.get_lod_count(mesh)
        require(lod_count >= 1, f"Mesh has no LOD0: {name}")
        total_lods += lod_count
        if name not in HERO_MESHES:
            require(lod_count >= 2, f"Non-hero mesh has no reduced LOD: {name}")
            reduced_meshes += 1
    return total_lods, reduced_meshes


def validate_photo_prop_lods() -> int:
    inspected = inspect_photo_prop_lods()
    for item in inspected:
        require(
            int(item["lod_count"]) >= 2,
            f"Photo prop has no reduced LOD: {item['path']}",
        )
    return len(inspected)


def validate_textures() -> int:
    checked = 0
    for stem, suffixes in PBR_STEMS.items():
        for suffix in suffixes:
            name = f"{stem}_{suffix}"
            texture = load(f"/Game/Prototype/Textures/{name}", unreal.Texture2D)
            srgb = bool(texture.get_editor_property("srgb"))
            if suffix == "D":
                require(srgb, f"Base colour must be sRGB: {name}")
            else:
                require(not srgb, f"Data texture must be linear: {name}")
            compression = str(texture.get_editor_property("compression_settings"))
            if suffix == "N":
                require("NORMALMAP" in compression.upper(), f"Normal map compression missing: {name}")
            elif suffix == "M":
                require("MASK" in compression.upper(), f"Mask compression missing: {name}")
            elif suffix in {"R", "A", "W"}:
                require("GRAYSCALE" in compression.upper(), f"Grayscale compression missing: {name}")
            size_x = texture.blueprint_get_size_x()
            size_y = texture.blueprint_get_size_y()
            require(size_x >= 1024 and size_y >= 1024, f"Texture below 1K: {name} ({size_x}x{size_y})")
            checked += 1
    for texture_name in EVIDENCE_MASK_MATERIALS.values():
        texture = load(
            f"/Game/Prototype/Textures/{texture_name}", unreal.Texture2D
        )
        require(not texture.get_editor_property("srgb"), f"Mask must be linear: {texture_name}")
        compression = str(texture.get_editor_property("compression_settings"))
        require("MASK" in compression.upper(), f"Mask compression missing: {texture_name}")
        checked += 1
    return checked


def material_input(material, material_property) -> None:
    node = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, material_property
    )
    require(node is not None, f"Missing material input {material_property}: {material.get_name()}")


def validate_materials() -> tuple[int, int]:
    checked = 0
    linked_textures = 0
    for name, stem in MATERIAL_TEXTURES.items():
        material = load(f"/Game/Prototype/Materials/{name}", unreal.Material)
        errors = unreal.MaterialEditingLibrary.recompile_material(material)
        require(not errors, f"Material compile failed: {name}: {errors}")
        used = expression_texture_paths(material)
        expected = {
            f"/Game/Prototype/Textures/{stem}_{suffix}"
            for suffix in PBR_STEMS[stem]
        }
        missing = sorted(expected - used)
        require(not missing, f"PBR maps are not linked to {name}: {missing}")
        linked_textures += len(expected)
        for material_property in (
            unreal.MaterialProperty.MP_BASE_COLOR,
            unreal.MaterialProperty.MP_NORMAL,
            unreal.MaterialProperty.MP_ROUGHNESS,
            unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,
        ):
            material_input(material, material_property)
        if name in {"M_WaterTankMetalUV", "M_TankInteriorBiofilmUV"}:
            material_input(material, unreal.MaterialProperty.MP_METALLIC)
        if name in {"M_CarrierBagFilm", "M_TankWaterReveal"}:
            require(
                material.get_editor_property("blend_mode")
                == unreal.BlendMode.BLEND_TRANSLUCENT,
                f"Translucent blend mode missing: {name}",
            )
            material_input(material, unreal.MaterialProperty.MP_OPACITY)
        if name == "M_TankWaterReveal":
            expressions = unreal.MaterialEditingLibrary.get_material_expressions(
                material
            )
            panner_count = sum(
                isinstance(expression, unreal.MaterialExpressionPanner)
                for expression in expressions
            )
            normal_layer_count = 0
            for expression in expressions:
                if not isinstance(
                    expression, unreal.MaterialExpressionTextureSample
                ):
                    continue
                texture = expression.get_editor_property("texture")
                if texture is not None and texture_path(texture).endswith(
                    "/T_TankWaterSurface_N"
                ):
                    normal_layer_count += 1
            require(
                panner_count >= 2,
                "Tank water needs two independent ripple panners",
            )
            require(
                normal_layer_count >= 2,
                "Tank water needs two blended normal layers",
            )
        if name == "M_TankInteriorBiofilmUV":
            expressions = unreal.MaterialEditingLibrary.get_material_expressions(
                material
            )
            require(
                any(
                    isinstance(
                        expression, unreal.MaterialExpressionWorldPosition
                    )
                    for expression in expressions
                ),
                "Tank interior needs a world-height waterline blend",
            )
            require(
                any(
                    isinstance(expression, unreal.MaterialExpressionSubtract)
                    for expression in expressions
                ),
                "Tank interior waterline blend lost its depth calculation",
            )
        checked += 1

    for name, texture_name in MASK_MATERIALS.items():
        material = load(f"/Game/Prototype/Materials/{name}", unreal.Material)
        errors = unreal.MaterialEditingLibrary.recompile_material(material)
        require(not errors, f"Material compile failed: {name}: {errors}")
        used = expression_texture_paths(material)
        expected = f"/Game/Prototype/Textures/{texture_name}"
        require(expected in used, f"Mask texture is not linked to {name}: {expected}")
        if name in EVIDENCE_MASK_MATERIALS:
            sampler_types = texture_sample_types(material)
            sampler_type = sampler_types.get(expected, "")
            require(
                "MASK" in sampler_type.upper(),
                f"Mask sampler type does not match TC_MASKS: {name}: {sampler_type}",
            )
        require(
            material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_MASKED,
            f"Masked blend mode missing: {name}",
        )
        material_input(material, unreal.MaterialProperty.MP_OPACITY_MASK)
        linked_textures += 1
        checked += 1
    return checked, linked_textures


def main() -> None:
    total_lods, reduced_meshes = validate_meshes()
    photo_prop_meshes = validate_photo_prop_lods()
    texture_count = validate_textures()
    material_count, linked_textures = validate_materials()
    unreal.log_warning(
        "ART_UASSET_AUDIT PASS "
        f"meshes={len(MESH_NAMES)} total_lods={total_lods} "
        f"reduced_meshes={reduced_meshes} photo_meshes={photo_prop_meshes} "
        f"textures={texture_count} "
        f"materials={material_count} linked_textures={linked_textures}"
    )


if __name__ == "__main__":
    main()
