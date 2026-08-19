"""Shared, dependency-free contract for the printed-artwork texture atlas.

Every notice, plate, label, snack bag and shop sign in the game is a separate
1-channel colour texture on its own material, and each of those is a separate
draw call and a separate streaming entry. There are fifty-two of them, none of
them tiles, and most of them are on screen at the same time in the store and on
the fourth-floor landing. That is exactly the set an atlas is for.

This module is imported by three very different callers -- the offline packer,
the release validator, and the in-editor material builder -- so it must stay
free of both Pillow and ``unreal``.

The atlas is authored, not discovered: ``PRINT_ATLAS_ENTRIES`` is the list of
textures allowed in, in a fixed order, so two machines pack identical pages.
"""

from __future__ import annotations

import json
import os


PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SOURCE_ART_DIR = os.path.join(PROJECT_ROOT, "Content", "SourceArt")
ATLAS_DIR = os.path.join(SOURCE_ART_DIR, "Atlas")
MANIFEST_PATH = os.path.join(ATLAS_DIR, "print_atlas.json")

# Package path the pages import to, alongside the individual textures.
ATLAS_TEXTURE_ROOT = "/Game/Prototype/Textures"
ATLAS_PAGE_PREFIX = "T_PrintAtlas"

MANIFEST_VERSION = 1

# 2048 is the largest page that still streams in one 8 MB BC7 block on the
# minimum spec in PERFORMANCE.md. The current print set measures 12.66 Mpx, so
# it needs four such pages -- which is the floor for that area, not slack in
# the packer: pages come out 94.6 / 89.9 / 83.4 / 33.9 per cent full.
PAGE_SIZE = 2048

# Eight pixels of edge-extended bleed around every entry. At 2048 that keeps
# neighbours out of the sample down to the 1/32 mip, which is well past the
# distance any of this artwork is still legible at.
GUTTER = 8

# The mip chain has to stop before a texel spans two entries.
MAX_MIP_LEVELS = 5

# Entries are packed in this order. Adding a texture goes at the end so the
# existing pages keep their layout; a reshuffle is a deliberate act.
PRINT_ATLAS_ENTRIES = (
    # Unit doors and the fourth-floor landing.
    "T_Plate401_D",
    "T_Plate402_D",
    "T_Plate403_D",
    "T_Plate404_D",
    "T_PlateCommon_D",
    "T_DoorLock_D",
    "T_Intercom_D",
    "T_MeterBox_D",
    "T_SwitchPlate_D",
    "T_FireBox_D",
    "T_DoorAd_D",
    "T_Note404NotFound_D",
    "T_MercyNoteUnderDoor_D",
    "T_CaptureMercyNote_D",
    # Lobby and lift.
    "T_LiftCOP_D",
    "T_LiftHall_D",
    "T_ElevatorPanel_D",
    "T_NoticeA4_D",
    "T_NoticeRent_D",
    "T_SignVilla_D",
    "T_SignToilet_D",
    "T_ClockFace_D",
    "T_Calendar_D",
    # Apartment paper.
    "T_ArrivalContract_D",
    "T_NoteFridge_D",
    "T_PaperClean_V2_D",
    "T_PaperWet_V2_D",
    "T_PaperFolded_V2_D",
    "T_PaperOld_V2_D",
    # Store interior print.
    "T_PosterSale_D",
    "T_PosterRamyeon_D",
    "T_PosterFlyer_D",
    "T_TobaccoNotice_D",
    "T_SignAutoDoor_D",
    "T_SignAux5MonitorOnly_D",
    "T_Banner_D",
    # Product artwork.
    "T_LabelWater_D",
    "T_LabelGreenTea_D",
    "T_LabelBarley_D",
    "T_LabelSoda_D",
    "T_LabelSoju_D",
    "T_LabelRamyeon_D",
    "T_SnackShrimp_D",
    "T_SnackPotato_D",
    "T_SnackSquid_D",
    "T_SnackCorn_D",
    # Alley shopfronts.
    "T_SignLaundry_D",
    "T_SignHair_D",
    "T_SignHof_D",
    "T_SignSuper_D",
    "T_SignPC_D",
    "T_SignKaraoke_D",
)

# Textures that must stay off the atlas, with the reason. Kept explicit so a
# later reader does not "helpfully" add them back.
ATLAS_EXCLUSIONS = {
    "T_PriceStrip_D": "tiles along U; an atlas rect cannot wrap",
    "T_SignMain_D": "1K facade hero sign, lit by its own emissive path",
    "T_SignBlade_D": "1K facade hero sign, lit by its own emissive path",
    "T_TitleBackground_D": "full-screen frontend art, never in the world",
    "T_HudDialogueFilm_D": "UI group, no mips, not streamed",
    "T_MissingFloorJournalPaper_D": "UI group, no mips, not streamed",
    "T_AudioCalibrationWall_D": "UI group, no mips, not streamed",
}


class AtlasContractError(RuntimeError):
    """Raised when the atlas on disk does not satisfy the contract."""


def page_asset_name(page_index: int) -> str:
    return f"{ATLAS_PAGE_PREFIX}{page_index}_D"


def page_file_name(page_index: int) -> str:
    return f"{page_asset_name(page_index)}.png"


def page_package_path(page_index: int) -> str:
    return f"{ATLAS_TEXTURE_ROOT}/{page_asset_name(page_index)}"


def source_texture_path(stem: str) -> str:
    return os.path.join(SOURCE_ART_DIR, f"{stem}.png")


def load_manifest(path: str = MANIFEST_PATH) -> dict:
    """Reads the packed layout, or raises if it has not been built."""
    if not os.path.isfile(path):
        raise AtlasContractError(
            f"No atlas manifest at {path}. Run Scripts/build_texture_atlas.py."
        )
    with open(path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    validate_manifest(manifest)
    return manifest


def try_load_manifest(path: str = MANIFEST_PATH):
    """Manifest if it is present and valid, otherwise None.

    The material builder uses this: a checkout that has not run the packer
    yet still produces a playable build from the individual textures.
    """
    try:
        return load_manifest(path)
    except (AtlasContractError, ValueError):
        return None


def validate_manifest(manifest: dict) -> None:
    """Structural and geometric checks that need no image data."""
    if manifest.get("version") != MANIFEST_VERSION:
        raise AtlasContractError(
            f"Atlas manifest version {manifest.get('version')} "
            f"!= {MANIFEST_VERSION}"
        )
    page_size = manifest.get("page_size")
    if page_size != PAGE_SIZE:
        raise AtlasContractError(f"Atlas page size {page_size} != {PAGE_SIZE}")
    if page_size & (page_size - 1):
        raise AtlasContractError("Atlas page size must be a power of two")
    if manifest.get("gutter") != GUTTER:
        raise AtlasContractError(
            f"Atlas gutter {manifest.get('gutter')} != {GUTTER}"
        )

    entries = manifest.get("entries") or {}
    missing = [name for name in PRINT_ATLAS_ENTRIES if name not in entries]
    if missing:
        raise AtlasContractError(
            f"Atlas is missing {len(missing)} contracted entries: "
            + ", ".join(missing[:6])
        )
    extra = [name for name in entries if name not in PRINT_ATLAS_ENTRIES]
    if extra:
        raise AtlasContractError(
            "Atlas holds textures outside the contract: " + ", ".join(extra)
        )

    pages = manifest.get("pages") or []
    if not pages:
        raise AtlasContractError("Atlas manifest lists no pages")

    occupied: dict[int, list[tuple[str, int, int, int, int]]] = {}
    for name, entry in entries.items():
        page = entry["page"]
        if not 0 <= page < len(pages):
            raise AtlasContractError(f"{name} references page {page}")
        x, y = entry["x"], entry["y"]
        width, height = entry["w"], entry["h"]
        if x < 0 or y < 0:
            raise AtlasContractError(f"{name} starts outside page {page}")
        # A page edge needs no gutter: there is no neighbour beyond it and the
        # sampler clamps. Gutters are only ever about the entry next door.
        if x + width > page_size or y + height > page_size:
            raise AtlasContractError(f"{name} overruns page {page}")
        occupied.setdefault(page, []).append((name, x, y, width, height))

        scale = entry["uv_scale"]
        bias = entry["uv_bias"]
        expected = (width / page_size, height / page_size)
        if abs(scale[0] - expected[0]) > 1e-9 or abs(scale[1] - expected[1]) > 1e-9:
            raise AtlasContractError(f"{name} UV scale disagrees with its rect")
        if abs(bias[0] - x / page_size) > 1e-9 or abs(bias[1] - y / page_size) > 1e-9:
            raise AtlasContractError(f"{name} UV bias disagrees with its rect")

    for page, rects in occupied.items():
        for first in range(len(rects)):
            name_a, ax, ay, aw, ah = rects[first]
            for second in range(first + 1, len(rects)):
                name_b, bx, by, bw, bh = rects[second]
                # Gutters may not overlap either: one entry's bleed must not
                # land inside another entry's pixels.
                if (ax - GUTTER < bx + bw + GUTTER
                        and bx - GUTTER < ax + aw + GUTTER
                        and ay - GUTTER < by + bh + GUTTER
                        and by - GUTTER < ay + ah + GUTTER):
                    raise AtlasContractError(
                        f"{name_a} and {name_b} overlap on page {page}"
                    )


def atlas_uv_transform(manifest: dict, stem: str):
    """(scale_u, scale_v, bias_u, bias_v) for one texture, or None."""
    entry = (manifest.get("entries") or {}).get(stem)
    if entry is None:
        return None
    scale = entry["uv_scale"]
    bias = entry["uv_bias"]
    return (scale[0], scale[1], bias[0], bias[1])
