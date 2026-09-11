"""Content/SourceArt/Audio의 WAV를 /Game/Audio/S_<이름> USoundWave로 반입한다.

Import-AudioSamples.ps1이 콘텐츠 전용 프로젝트(ArtImport)에서 돌린다.
manifest.json의 loop 플래그가 USoundWave의 looping으로 간다. 게임은
IGAudio::Sample(이름)으로 찾고, 없으면 합성기로 내려간다.

환경 변수
    IG_AUDIO_SOURCE  manifest.json과 WAV가 있는 폴더(ASCII 경로)
"""

import json
import os

import unreal

AUDIO_ROOT = "/Game/Audio"


def log(message):
    unreal.log_warning(f"[AUDIO_IMPORT] {message}")


def main():
    source = os.environ.get("IG_AUDIO_SOURCE")
    if not source or not os.path.isdir(source):
        raise RuntimeError(f"IG_AUDIO_SOURCE is not a folder: {source}")
    with open(os.path.join(source, "manifest.json"), "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    imported = 0
    for entry in manifest["sounds"]:
        name = f"S_{entry['name']}"
        path = os.path.join(source, entry["file"])
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = AUDIO_ROOT
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        tools.import_asset_tasks([task])
        wave = unreal.load_asset(f"{AUDIO_ROOT}/{name}")
        if wave is None:
            raise RuntimeError(f"sound import failed: {path}")
        for prop, value in (("looping", bool(entry.get("loop", False))),
                            ("sound_group", unreal.SoundGroup.SOUNDGROUP_EFFECTS)):
            try:
                wave.set_editor_property(prop, value)
            except Exception as error:  # noqa: BLE001 - 마이너마다 이름이 다르다
                log(f"  property skipped {prop}: {error}")
        unreal.EditorAssetLibrary.save_asset(f"{AUDIO_ROOT}/{name}", False)
        imported += 1
        log(f"  {name} loop={entry.get('loop', False)} {entry.get('seconds', 0)}s")
    log(f"AUDIO_IMPORT PASS sounds={imported}")


main()
