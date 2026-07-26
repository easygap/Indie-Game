"""Download the CC0 photogrammetry props (polyhaven.com, glTF @1k textures)
used by the prologue into Content/SourceArt/PhotoProps/<id>/.

Runs with the system Python (no Unreal). Each asset's include files (bin +
textures) are fetched preserving relative paths so the .gltf imports cleanly.
CC0 1.0 — recorded in Docs/ASSET_POLICY.md.
"""

import json
import os
import sys
import urllib.request

PROPS = [
    "old_bed_frame",
    "side_table_01",
    "metal_office_desk",
    "painted_wooden_chair_01",
    "modern_wooden_cabinet",
    "desk_lamp_arm_01",
    "electric_stove",
    "street_lamp_01",
    "trashbag",
    "cardboard_box_01",
    "steel_frame_shelves_01",
    "CashRegister_01",
    "plastic_crate_01",
    "utility_box_01",
    "wine_bottles_01",
    "outdoor_table_chair_set_01",
    "plastic_monobloc_chair_01",
]

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_ROOT = os.path.join(PROJECT_ROOT, "Content", "SourceArt", "PhotoProps")


USER_AGENT = "Mozilla/5.0 (IndieGame asset fetch; CC0 content)"


def _open(url, timeout):
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    return urllib.request.urlopen(request, timeout=timeout)


def fetch_json(url):
    with _open(url, 60) as response:
        return json.load(response)


def download(url, dest):
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        return 0
    with _open(url, 300) as response, open(dest, "wb") as out:
        data = response.read()
        out.write(data)
        return len(data)


def main():
    os.makedirs(OUT_ROOT, exist_ok=True)
    ok = 0
    for asset_id in PROPS:
        try:
            files = fetch_json(f"https://api.polyhaven.com/files/{asset_id}")
            gltf_entry = files["gltf"]["1k"]["gltf"]
            asset_dir = os.path.join(OUT_ROOT, asset_id)
            total = download(
                gltf_entry["url"], os.path.join(asset_dir, f"{asset_id}_1k.gltf")
            )
            for rel_path, meta in gltf_entry.get("include", {}).items():
                total += download(meta["url"], os.path.join(asset_dir, rel_path))
            print(f"OK {asset_id}: {total // 1024} KB new")
            ok += 1
        except Exception as error:  # noqa: BLE001 - report and continue
            print(f"FAIL {asset_id}: {error}")
    print(f"Downloaded {ok}/{len(PROPS)} props")
    return 0 if ok == len(PROPS) else 1


if __name__ == "__main__":
    sys.exit(main())
