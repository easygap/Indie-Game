"""Reimport the retail textures that must match the in-game POS receipt.

Run inside UnrealEditor-Cmd with ``-ExecutePythonScript``.  Keeping this pass
separate from ``generate_surface_textures.py`` avoids rewriting every authored
surface texture when only the fictional store identity or shelf price changes.
"""

import os

import unreal


PROJECT_DIR = unreal.SystemLibrary.get_project_directory()
SOURCE_ART_DIR = os.path.join(PROJECT_DIR, "Content", "SourceArt")
DESTINATION_PATH = "/Game/Prototype/Textures"
TEXTURE_NAMES = ("T_SignMain_D", "T_PriceStrip_D", "T_PosterSale_D", "T_LabelWater_D")


def import_retail_identity():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if asset_tools is None or asset_subsystem is None:
        raise RuntimeError("Editor asset services are unavailable")

    tasks = []
    for texture_name in TEXTURE_NAMES:
        source_file = os.path.join(SOURCE_ART_DIR, f"{texture_name}.png")
        if not os.path.isfile(source_file):
            raise RuntimeError(f"Missing source texture: {source_file}")

        task = unreal.AssetImportTask()
        task.filename = source_file
        task.destination_path = DESTINATION_PATH
        task.destination_name = texture_name
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)

    asset_tools.import_asset_tasks(tasks)

    imported_assets = []
    for texture_name in TEXTURE_NAMES:
        asset_path = f"{DESTINATION_PATH}/{texture_name}"
        texture = unreal.load_asset(asset_path)
        if texture is None:
            raise RuntimeError(f"Import failed for {asset_path}")
        imported_assets.append(texture)

    if not asset_subsystem.save_loaded_assets(imported_assets, False):
        raise RuntimeError("Could not save retail identity textures")

    unreal.log(
        "[IndieGame] Reimported retail identity: "
        + ", ".join(TEXTURE_NAMES)
    )


if __name__ == "__main__":
    import_retail_identity()
