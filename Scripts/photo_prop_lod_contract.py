"""Shared UE editor contract for imported photo-prop static-mesh LODs."""

from __future__ import annotations

import unreal


PHOTO_PROP_ROOT = "/Game/Photo/Props"

# Furniture and building fixtures keep detail farther from the camera than
# handheld or shelf-size clutter. Screen-size thresholds still come from the
# engine LOD groups; these names do not encode world-distance cutoffs.
LARGE_PROP_IDS = {
    "electric_stove",
    "metal_office_desk",
    "modern_wooden_cabinet",
    "old_bed_frame",
    "outdoor_table_chair_set_01",
    "painted_wooden_chair_01",
    "plastic_monobloc_chair_01",
    "steel_frame_shelves_01",
    "street_lamp_01",
}


def _editor_services():
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    mesh_subsystem = unreal.get_editor_subsystem(
        unreal.StaticMeshEditorSubsystem
    )
    if asset_subsystem is None or mesh_subsystem is None:
        raise RuntimeError("Photo-prop LOD editor services are unavailable")
    return asset_subsystem, mesh_subsystem


def _asset_id(asset_path: str) -> str:
    parts = asset_path.split("/")
    try:
        return parts[parts.index("Props") + 1]
    except (ValueError, IndexError) as error:
        raise RuntimeError(
            f"Photo-prop asset escaped the expected root: {asset_path}"
        ) from error


def inspect_photo_prop_lods() -> list[dict[str, object]]:
    """Returns the persisted LOD state for every imported static mesh."""
    asset_subsystem, mesh_subsystem = _editor_services()
    results: list[dict[str, object]] = []
    for asset_path in sorted(
        asset_subsystem.list_assets(PHOTO_PROP_ROOT, True, False)
    ):
        asset = asset_subsystem.load_asset(asset_path)
        if not isinstance(asset, unreal.StaticMesh):
            continue
        try:
            vertex_count = mesh_subsystem.get_number_verts(asset, 0)
        except Exception:  # noqa: BLE001 - diagnostics only across UE minors
            vertex_count = -1
        results.append(
            {
                "asset": asset,
                "path": asset_path,
                "asset_id": _asset_id(asset_path),
                "lod_count": mesh_subsystem.get_lod_count(asset),
                "vertex_count": vertex_count,
            }
        )
    if not results:
        raise RuntimeError(f"No static meshes found below {PHOTO_PROP_ROOT}")
    return results


def apply_photo_prop_lod_contract() -> dict[str, int]:
    """Builds a reduced LOD only where an imported mesh does not have one."""
    asset_subsystem, mesh_subsystem = _editor_services()
    inspected = inspect_photo_prop_lods()
    updated = 0
    large_props = 0
    nanite_review_candidates = 0

    for item in inspected:
        asset = item["asset"]
        asset_id = str(item["asset_id"])
        if asset_id in LARGE_PROP_IDS:
            lod_group = "LargeProp"
            large_props += 1
        else:
            lod_group = "SmallProp"

        # High vertex count is only a review signal. Material compatibility,
        # fallback cost and an A/B GPU trace decide Nanite; this script never
        # changes that production choice on its own.
        if int(item["vertex_count"]) >= 50_000:
            nanite_review_candidates += 1

        if int(item["lod_count"]) >= 2:
            continue
        mesh_subsystem.set_lod_group(asset, lod_group, True)
        lod_count = mesh_subsystem.get_lod_count(asset)
        if lod_count < 2:
            raise RuntimeError(
                f"Photo prop did not build a reduced LOD: {item['path']}"
            )
        if not asset_subsystem.save_loaded_asset(asset, False):
            raise RuntimeError(f"Photo prop failed to save: {item['path']}")
        updated += 1

    return {
        "meshes": len(inspected),
        "updated": updated,
        "large_props": large_props,
        "small_props": len(inspected) - large_props,
        "nanite_review_candidates": nanite_review_candidates,
    }
