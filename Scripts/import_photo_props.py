"""Import the Poly Haven glTF props (Content/SourceArt/PhotoProps/<id>/) into
/Game/Photo/Props/<id> via Interchange. The scene resolves each prop's static
mesh at runtime and falls back to composed greybox shapes when missing."""

import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from photo_prop_lod_contract import apply_photo_prop_lod_contract


PROJECT_DIR = unreal.SystemLibrary.get_project_directory()
PROPS_DIR = os.path.join(PROJECT_DIR, "Content", "SourceArt", "PhotoProps")
DEST_ROOT = "/Game/Photo/Props"


def import_props():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if asset_tools is None or asset_subsystem is None:
        raise RuntimeError("Editor asset services are unavailable")
    if not os.path.isdir(PROPS_DIR):
        raise RuntimeError(f"Props folder missing: {PROPS_DIR}")

    imported_any = False
    for asset_id in sorted(os.listdir(PROPS_DIR)):
        asset_dir = os.path.join(PROPS_DIR, asset_id)
        gltf_path = os.path.join(asset_dir, f"{asset_id}_1k.gltf")
        if not os.path.isfile(gltf_path):
            unreal.log_warning(f"[IndieGame] No gltf for prop: {asset_id}")
            continue

        task = unreal.AssetImportTask()
        task.filename = gltf_path
        task.destination_path = f"{DEST_ROOT}/{asset_id}"
        task.automated = True
        task.replace_existing = True
        task.save = True
        asset_tools.import_asset_tasks([task])
        unreal.log(f"[IndieGame] Imported prop: {asset_id}")
        imported_any = True

    if not imported_any:
        raise RuntimeError("No props were imported")

    lod_result = apply_photo_prop_lod_contract()
    unreal.log(
        "[IndieGame] Photo-prop LOD contract applied: "
        f"meshes={lod_result['meshes']} updated={lod_result['updated']}"
    )


if __name__ == "__main__":
    import_props()
