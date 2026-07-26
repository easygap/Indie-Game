"""Create or load the prologue bedroom map, then save it.

Run this script with UnrealEditor-Cmd.exe and -ExecutePythonScript. It uses
editor subsystems that are available in Unreal Engine 5.8 and deliberately
avoids the deprecated EditorLevelLibrary API.
"""

import unreal


MAP_ASSET_PATH = "/Game/Maps/Prologue_Morning"


def _require_editor_subsystem(subsystem_class):
    subsystem = unreal.get_editor_subsystem(subsystem_class)
    if subsystem is None:
        raise RuntimeError(
            f"Required Unreal editor subsystem is unavailable: {subsystem_class}"
        )
    return subsystem


def create_or_load_prologue_map():
    level_editor = _require_editor_subsystem(unreal.LevelEditorSubsystem)
    asset_editor = _require_editor_subsystem(unreal.EditorAssetSubsystem)
    actor_editor = _require_editor_subsystem(unreal.EditorActorSubsystem)

    if asset_editor.does_asset_exist(MAP_ASSET_PATH):
        if not level_editor.load_level(MAP_ASSET_PATH):
            raise RuntimeError(f"Failed to load existing map: {MAP_ASSET_PATH}")
        operation = "Loaded"
    else:
        if not level_editor.new_level(MAP_ASSET_PATH, False):
            raise RuntimeError(
                f"Failed to create non-partitioned map: {MAP_ASSET_PATH}"
            )
        operation = "Created"

    current_level = level_editor.get_current_level()
    if current_level is None:
        raise RuntimeError("LevelEditorSubsystem returned no current level")

    current_package = current_level.get_outermost().get_name()
    if current_package != MAP_ASSET_PATH:
        raise RuntimeError(
            "Unexpected current level package after create/load: "
            f"expected {MAP_ASSET_PATH}, got {current_package}"
        )

    player_starts = [
        actor
        for actor in actor_editor.get_all_level_actors()
        if isinstance(actor, unreal.PlayerStart)
    ]
    if not player_starts:
        player_start = actor_editor.spawn_actor_from_class(
            unreal.PlayerStart,
            unreal.Vector(-325.0, -40.0, 98.0),
            unreal.Rotator(0.0, -90.0, 0.0),
            False,
        )
        if player_start is None:
            raise RuntimeError("Failed to create the bedroom PlayerStart")
        player_start.set_actor_label("Bedroom_PlayerStart")

    if not level_editor.save_current_level():
        raise RuntimeError(f"Failed to save current map: {MAP_ASSET_PATH}")

    unreal.log(f"[IndieGame] {operation} and saved map: {MAP_ASSET_PATH}")
    return current_level


if __name__ == "__main__":
    create_or_load_prologue_map()
