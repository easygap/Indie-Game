"""Shared UE editor contract for imported photo-prop static-mesh LODs.

The fifteen Poly Haven props are photogrammetry: they arrive as one dense,
evenly-triangulated shell with no LODs and no lightmap channel, at a density
chosen by whoever ran the scan. Asking the engine for a LOD *group* and
stopping there leaves LOD0 exactly as scanned, which is where a two-metre
office desk costs more triangles than the room it stands in.

This applies the same treatment the generated meshes get: budget LOD0, build
an authored reduction chain, and give a static prop the lightmap channel it
needs. Budgets and screen sizes come from mesh_lod_contract, so the scanned
props and the procedural ones reduce on the same curve.
"""

from __future__ import annotations

import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

import mesh_lod_contract  # noqa: E402


PHOTO_PROP_ROOT = "/Game/Photo/Props"

# Furniture and building fixtures keep detail farther from the camera than
# handheld or shelf-size clutter. Screen-size thresholds still come from the
# LOD chain in mesh_lod_contract; these names only choose which chain.
LARGE_PROP_IDS = {
    "electric_stove",
    "metal_office_desk",
    "modern_wooden_cabinet",
    "old_bed_frame",
    "outdoor_table_chair_set_01",
    "painted_wooden_chair_01",
    # 크기로는 소품이지만 스캔의 UV 심 보존 감축 바닥이 6652라 3000 예산에
    # 물리적으로 못 들어간다. 실측(18320 -> 6652 수렴)이 근거다.
    "plastic_crate_01",
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


def prop_class(asset_id: str):
    """The mesh_lod_contract class a scanned prop reduces on."""
    return (
        mesh_lod_contract.LARGE
        if asset_id in LARGE_PROP_IDS
        else mesh_lod_contract.PROP
    )


def _set_properties(target, values) -> int:
    applied = 0
    for name, value in values:
        try:
            target.set_editor_property(name, value)
            applied += 1
        except Exception:  # noqa: BLE001 - property names drift across minors
            pass
    return applied


def lod_triangle_count(asset, use_render_data) -> int:
    """LOD0 삼각형 실측. 5.8에서 서브시스템의 get_number_triangles가 사라져
    지오메트리 스크립트로 돌아 읽는다.

    스캔 원본의 밀도는 소스 모델에, 감축 체인이 적용된 출하 밀도는 렌더
    데이터에 있다. 감축 비율은 소스로 계산하고 예산 검증은 렌더로 한다 —
    버텍스 수를 삼각형인 척 쓰면 비율이 모자라 LOD0이 예산을 넘는다.
    """
    try:
        dynamic = unreal.new_object(unreal.DynamicMesh)
    except Exception:  # noqa: BLE001 - binding differences across versions
        dynamic = unreal.DynamicMesh()
    read_lod = unreal.GeometryScriptMeshReadLOD()
    lod_type = getattr(unreal, "GeometryScriptLODType", None)
    if lod_type is not None:
        try:
            read_lod.set_editor_property(
                "lod_type",
                lod_type.RENDER_DATA if use_render_data
                else lod_type.SOURCE_MODEL,
            )
        except Exception:  # noqa: BLE001 - MaxAvailable이 소스 모델에 닿는다
            pass
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        asset,
        dynamic,
        unreal.GeometryScriptCopyMeshFromAssetOptions(),
        read_lod,
    )
    counter = getattr(dynamic, "get_triangle_count", None)
    if counter is not None:
        return int(counter())
    counter = getattr(
        unreal.GeometryScript_MeshQueries, "get_num_triangle_i_ds", None)
    if counter is not None:
        return int(counter(dynamic))
    return -1


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
        asset_id = _asset_id(asset_path)
        results.append(
            {
                "asset": asset,
                "path": asset_path,
                "asset_id": asset_id,
                "lod_count": mesh_subsystem.get_lod_count(asset),
                "vertex_count": vertex_count,
                # 검증이 보는 것은 출하 밀도, 곧 감축 체인이 적용된 렌더
                # LOD0이다. 소스 밀도는 감축 비율을 계산하는 쪽만 쓴다.
                "triangle_count": lod_triangle_count(asset, True),
                "budget": prop_class(asset_id).lod0_triangles,
            }
        )
    if not results:
        raise RuntimeError(f"No static meshes found below {PHOTO_PROP_ROOT}")
    return results


def _build_chain(mesh_subsystem, asset, mesh_class, over_budget_ratio) -> bool:
    """Authored reduction chain, with LOD0 pulled into budget when needed."""
    reduction_options = getattr(unreal, "StaticMeshReductionOptions", None)
    reduction_settings = getattr(unreal, "StaticMeshReductionSettings", None)
    if reduction_options is None or reduction_settings is None:
        return False

    settings = []
    # LOD0 first: a scan over budget is reduced in place, so everything below
    # it is a percentage of geometry the project actually chose.
    lod0 = reduction_settings()
    _set_properties(lod0, (
        ("percent_triangles", min(1.0, over_budget_ratio)),
        ("screen_size", 1.0),
    ))
    settings.append(lod0)
    for percent, screen in mesh_class.chain:
        entry = reduction_settings()
        _set_properties(entry, (
            ("percent_triangles", percent),
            ("screen_size", screen),
        ))
        settings.append(entry)

    options = reduction_options()
    _set_properties(options, (
        ("reduction_settings", settings),
        ("auto_compute_lod_screen_size", False),
    ))
    try:
        mesh_subsystem.set_lods(asset, options)
        return True
    except Exception:  # noqa: BLE001 - fall back to the engine group
        return False


def _apply_lightmap_channel(mesh_subsystem, asset, mesh_class) -> None:
    build_settings = getattr(unreal, "MeshBuildSettings", None)
    if build_settings is None:
        return
    settings = build_settings()
    _set_properties(settings, (
        ("recompute_normals", False),
        ("recompute_tangents", True),
        ("use_mikk_t_space", True),
        ("remove_degenerates", True),
        ("generate_lightmap_u_vs", True),
        ("src_lightmap_index", 0),
        ("dst_lightmap_index", 1),
        ("min_lightmap_resolution", mesh_class.lightmap_resolution),
    ))
    try:
        mesh_subsystem.set_lod_build_settings(asset, 0, settings)
    except Exception:  # noqa: BLE001
        return
    _set_properties(asset, (
        ("light_map_resolution", mesh_class.lightmap_resolution),
        ("light_map_coordinate_index", 1),
    ))


def apply_photo_prop_lod_contract() -> dict[str, int]:
    """Budgets LOD0 and builds the authored chain on every imported scan."""
    asset_subsystem, mesh_subsystem = _editor_services()
    inspected = inspect_photo_prop_lods()
    updated = 0
    large_props = 0
    retopologised = 0
    nanite_review_candidates = 0

    for item in inspected:
        asset = item["asset"]
        asset_id = str(item["asset_id"])
        mesh_class = prop_class(asset_id)
        if mesh_class is mesh_lod_contract.LARGE:
            large_props += 1

        # High vertex count is only a review signal. Material compatibility,
        # fallback cost and an A/B GPU trace decide Nanite; this script never
        # changes that production choice on its own.
        if int(item["vertex_count"]) >= 50_000:
            nanite_review_candidates += 1

        # 감축 비율은 스캔 원본, 곧 소스 모델의 실제 삼각형 수로 계산한다.
        # 감축기가 목표 비율에 정확히 못 맞는 경우가 있어 2%를 덜어 예산
        # 안쪽에 착지시킨다.
        source_triangles = lod_triangle_count(asset, False)
        budget = mesh_class.lod0_triangles
        ratio = 1.0
        if 0 < budget < source_triangles:
            ratio = budget * 0.98 / float(source_triangles)
            retopologised += 1

        if _build_chain(mesh_subsystem, asset, mesh_class, ratio):
            _apply_lightmap_channel(mesh_subsystem, asset, mesh_class)
        else:
            group = "LargeProp" if mesh_class is mesh_lod_contract.LARGE \
                else "SmallProp"
            mesh_subsystem.set_lod_group(asset, group, True)

        lod_count = mesh_subsystem.get_lod_count(asset)
        if lod_count < 2:
            raise RuntimeError(
                f"Photo prop did not build a reduced LOD: {item['path']}"
            )
        if not asset_subsystem.save_loaded_asset(asset, False):
            raise RuntimeError(f"Photo prop failed to save: {item['path']}")
        if ratio < 1.0:
            # 스캔은 UV 심 보존 때문에 감축 바닥이 프롭마다 다르다. 요청한
            # 비율과 실제 착지를 로그로 남겨야 분류(LARGE_PROP_IDS) 판단을
            # 숫자로 할 수 있다.
            achieved = lod_triangle_count(asset, True)
            unreal.log_warning(
                f"[PHOTO_LOD] {asset_id}: {source_triangles} -> {achieved} "
                f"tris (budget {budget})"
            )
        updated += 1

    return {
        "meshes": len(inspected),
        "updated": updated,
        "retopologised": retopologised,
        "large_props": large_props,
        "small_props": len(inspected) - large_props,
        "nanite_review_candidates": nanite_review_candidates,
    }
