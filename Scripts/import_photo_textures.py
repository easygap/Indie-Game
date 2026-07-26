"""Import the CC0 photo textures (Content/SourceArt/Photo/<Surface>/) as
T_Photo_<Surface>_{D,N,R}. create_textured_materials.py prefers these over the
procedural fallbacks when they exist."""

import os

import unreal


PROJECT_DIR = unreal.SystemLibrary.get_project_directory()
PHOTO_DIR = os.path.join(PROJECT_DIR, "Content", "SourceArt", "Photo")
TEXTURE_PACKAGE_ROOT = "/Game/Prototype/Textures"

SUFFIX_BY_ROLE = {
    "_Color.jpg": "D",
    "_NormalDX.jpg": "N",
    "_Roughness.jpg": "R",
}


def collect_import_files():
    if not os.path.isdir(PHOTO_DIR):
        raise RuntimeError(f"Photo texture folder missing: {PHOTO_DIR}")

    plan = []  # (source_path, target_asset_name)
    for surface in sorted(os.listdir(PHOTO_DIR)):
        surface_dir = os.path.join(PHOTO_DIR, surface)
        if not os.path.isdir(surface_dir):
            continue
        for entry in os.listdir(surface_dir):
            for suffix, role in SUFFIX_BY_ROLE.items():
                if entry.endswith(suffix):
                    plan.append(
                        (os.path.join(surface_dir, entry), f"T_Photo_{surface}_{role}")
                    )
    if not plan:
        raise RuntimeError("No photo texture files matched the expected suffixes")
    return plan


def import_photo_textures():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if asset_tools is None or asset_subsystem is None:
        raise RuntimeError("Editor asset services are unavailable")

    plan = collect_import_files()

    imported = []
    for source_path, asset_name in plan:
        # Import under the temporary source name, then rename to the target.
        task = unreal.AssetImportTask()
        task.filename = source_path
        task.destination_path = TEXTURE_PACKAGE_ROOT
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = False
        asset_tools.import_asset_tasks([task])

        asset_path = f"{TEXTURE_PACKAGE_ROOT}/{asset_name}"
        texture = unreal.load_asset(asset_path)
        if texture is None:
            raise RuntimeError(f"Import failed for {asset_path} (from {source_path})")

        if asset_name.endswith("_N"):
            texture.set_editor_property(
                "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP
            )
            texture.set_editor_property("srgb", False)
            texture.set_editor_property(
                "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP
            )
        elif asset_name.endswith("_R"):
            texture.set_editor_property("srgb", False)
            texture.set_editor_property(
                "compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE
            )
        imported.append(texture)

    if not asset_subsystem.save_loaded_assets(imported, False):
        raise RuntimeError("Could not save imported photo textures")
    unreal.log(f"[IndieGame] Imported {len(imported)} photo textures")


if __name__ == "__main__":
    import_photo_textures()
