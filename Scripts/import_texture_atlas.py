"""Import the packed print-atlas pages built by Scripts/build_texture_atlas.py.

The pages are ordinary colour textures with two things set deliberately:
clamped addressing, because every entry's UV rect ends inside the page and a
wrapped sample would pull in the entry on the far side; and a mip floor, so no
mip level is ever coarse enough for one texel to straddle two entries.

Run with:
    UnrealEditor-Cmd <uproject> -ExecutePythonScript=.../import_texture_atlas.py
"""

from __future__ import annotations

import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from texture_atlas_contract import (  # noqa: E402
    ATLAS_DIR,
    ATLAS_TEXTURE_ROOT,
    AtlasContractError,
    MAX_MIP_LEVELS,
    page_asset_name,
    page_file_name,
    try_load_manifest,
)


def _set_if_supported(texture, name, value):
    """UE renames a texture property now and then; never fail the import."""
    try:
        texture.set_editor_property(name, value)
        return True
    except Exception:  # noqa: BLE001 - property set differs across UE minors
        unreal.log_warning(f"[IndieGame] Atlas: could not set {name}")
        return False


def import_texture_atlas() -> int:
    manifest = try_load_manifest()
    if manifest is None:
        # No packed atlas in this checkout. Every print material falls back to
        # its own texture, which is the pre-atlas behaviour and still ships.
        unreal.log_warning(
            "PRINT_ATLAS_IMPORT PASS pages=0 entries=0 "
            "(no manifest; run Scripts/build_texture_atlas.py)"
        )
        return 0

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if asset_tools is None or asset_subsystem is None:
        raise RuntimeError("Editor asset services are unavailable")

    imported = []
    for page in manifest["pages"]:
        index = page["index"]
        source = os.path.join(ATLAS_DIR, page_file_name(index))
        if not os.path.isfile(source):
            raise AtlasContractError(f"Atlas page not built: {source}")

        task = unreal.AssetImportTask()
        task.filename = source
        task.destination_path = ATLAS_TEXTURE_ROOT
        task.destination_name = page_asset_name(index)
        task.automated = True
        task.replace_existing = True
        task.save = False
        asset_tools.import_asset_tasks([task])

        asset_path = f"{ATLAS_TEXTURE_ROOT}/{page_asset_name(index)}"
        texture = unreal.load_asset(asset_path)
        if texture is None:
            raise RuntimeError(f"Atlas page import failed: {asset_path}")

        # Clamp: an atlas rect has no wrap. Without this a UV that lands a
        # hair past 1.0 on one entry samples whatever is packed opposite it.
        _set_if_supported(
            texture, "address_x", unreal.TextureAddress.TA_CLAMP)
        _set_if_supported(
            texture, "address_y", unreal.TextureAddress.TA_CLAMP)
        # Stop the mip chain before a texel spans two entries plus their
        # gutters. Beyond this the artwork is a few pixels on screen anyway.
        _set_if_supported(texture, "mip_gen_settings",
                          unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
        _set_if_supported(texture, "num_cinematic_mip_levels", 0)
        _set_if_supported(texture, "max_texture_size", 0)
        _set_if_supported(texture, "lod_bias", 0)
        _set_if_supported(texture, "never_stream", False)
        # BC7 keeps the small Korean type on the notices legible; the pages are
        # the only textures in the project where several signs share one block.
        _set_if_supported(
            texture,
            "compression_settings",
            unreal.TextureCompressionSettings.TC_BC7,
        )
        _set_if_supported(texture, "srgb", True)
        imported.append(texture)

    if not asset_subsystem.save_loaded_assets(imported, False):
        raise RuntimeError("Could not save the imported atlas pages")

    unreal.log_warning(
        "PRINT_ATLAS_IMPORT PASS "
        f"pages={len(imported)} entries={len(manifest['entries'])} "
        f"max_mips={MAX_MIP_LEVELS}"
    )
    return len(imported)


if __name__ == "__main__":
    import_texture_atlas()
