#!/usr/bin/env python3
"""Pack the printed-artwork textures into atlas pages, outside the editor.

Fifty-two colour textures -- door plates, notices, product labels, shop signs,
the tenancy contract -- each carry their own material, their own streaming
entry and their own draw call. In the store and on the fourth-floor landing
most of them are on screen together. This packs them into 2048 px pages with
edge-extended gutters
and writes the layout to ``Content/SourceArt/Atlas/print_atlas.json``; the
editor scripts import the pages and rebuild those materials to sample one page
with a UV scale and bias.

    python3 Scripts/build_texture_atlas.py             # pack and write
    python3 Scripts/build_texture_atlas.py --check     # verify, change nothing
    python3 Scripts/build_texture_atlas.py --self-test # prove the packer

Needs Pillow and the real source art. On a checkout where Git LFS has not been
pulled the sources are pointer files; the packer says so and stops rather than
writing a garbage page.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from texture_atlas_contract import (  # noqa: E402
    ATLAS_DIR,
    AtlasContractError,
    GUTTER,
    MANIFEST_PATH,
    MANIFEST_VERSION,
    MAX_MIP_LEVELS,
    PAGE_SIZE,
    PRINT_ATLAS_ENTRIES,
    page_file_name,
    source_texture_path,
    validate_manifest,
)

LFS_POINTER_PREFIX = b"version https://git-lfs.github.com/spec/v1"


# ---------------------------------------------------------------------------
# Packing
# ---------------------------------------------------------------------------

class MaxRectsPage:
    """One atlas page, packed best-short-side-fit.

    Deterministic: candidate free rectangles are scanned in list order and the
    list only ever changes in ways driven by the placement order, so the same
    inputs always produce the same page.
    """

    def __init__(self, size: int):
        self.size = size
        self.free = [(0, 0, size, size)]
        self.used: list[tuple[int, int, int, int]] = []

    def insert(self, width: int, height: int):
        best = None
        best_score = None
        for rect in self.free:
            _, _, free_width, free_height = rect
            if free_width < width or free_height < height:
                continue
            leftover_h = free_width - width
            leftover_v = free_height - height
            score = (min(leftover_h, leftover_v), max(leftover_h, leftover_v))
            if best_score is None or score < best_score:
                best, best_score = rect, score
        if best is None:
            return None

        x, y = best[0], best[1]
        placed = (x, y, width, height)
        self._split(placed)
        self._prune()
        self.used.append(placed)
        return (x, y)

    def _split(self, placed):
        px, py, pw, ph = placed
        remaining = []
        for rect in self.free:
            fx, fy, fw, fh = rect
            if px >= fx + fw or px + pw <= fx or py >= fy + fh or py + ph <= fy:
                remaining.append(rect)
                continue
            if px > fx:
                remaining.append((fx, fy, px - fx, fh))
            if px + pw < fx + fw:
                remaining.append((px + pw, fy, fx + fw - (px + pw), fh))
            if py > fy:
                remaining.append((fx, fy, fw, py - fy))
            if py + ph < fy + fh:
                remaining.append((fx, py + ph, fw, fy + fh - (py + ph)))
        self.free = remaining

    def _prune(self):
        keep = []
        for index, rect in enumerate(self.free):
            contained = False
            for other_index, other in enumerate(self.free):
                if index == other_index:
                    continue
                if _contains(other, rect) and (rect != other or other_index < index):
                    contained = True
                    break
            if not contained:
                keep.append(rect)
        self.free = keep

    def occupancy(self) -> float:
        area = sum((w - GUTTER * 2) * (h - GUTTER * 2) for _, _, w, h in self.used)
        return area / float(PAGE_SIZE * PAGE_SIZE)


def _contains(outer, inner) -> bool:
    ox, oy, ow, oh = outer
    ix, iy, iw, ih = inner
    return ox <= ix and oy <= iy and ox + ow >= ix + iw and oy + oh >= iy + ih


def pack(sizes, page_size=PAGE_SIZE, gutter=GUTTER):
    """Places (name, width, height) triples. Returns placements and pages.

    Entries are tried largest-area first so the big notices claim their space
    before the door plates fill the gaps, but the *result* is keyed by name, so
    the caller's order does not leak into the layout.

    Packing runs in a page grown by one gutter on every side, and the gutter is
    then subtracted back off. An entry can therefore sit flush against the page
    edge -- nothing is next to it there, and the sampler clamps -- while any two
    entries still end up two gutters apart.
    """
    order = sorted(sizes, key=lambda item: (-(item[1] * item[2]), item[0]))
    virtual_size = page_size + gutter * 2
    pages: list[MaxRectsPage] = []
    placements = {}

    for name, width, height in order:
        padded_w = width + gutter * 2
        padded_h = height + gutter * 2
        if width > page_size or height > page_size:
            raise AtlasContractError(
                f"{name} is {width}x{height}; too large for a "
                f"{page_size} px page"
            )
        spot = None
        for page_index, page in enumerate(pages):
            spot = page.insert(padded_w, padded_h)
            if spot is not None:
                placements[name] = (page_index, spot[0], spot[1], width, height)
                break
        if spot is None:
            page = MaxRectsPage(virtual_size)
            spot = page.insert(padded_w, padded_h)
            if spot is None:  # pragma: no cover - guarded by the size check
                raise AtlasContractError(f"{name} did not fit an empty page")
            pages.append(page)
            placements[name] = (len(pages) - 1, spot[0], spot[1], width, height)
    return placements, pages


# ---------------------------------------------------------------------------
# Source art
# ---------------------------------------------------------------------------

def _is_lfs_pointer(path: str) -> bool:
    with open(path, "rb") as handle:
        return handle.read(len(LFS_POINTER_PREFIX)) == LFS_POINTER_PREFIX


def collect_sources(entries=PRINT_ATLAS_ENTRIES):
    """Returns [(stem, path, width, height, sha256)] for every atlas entry."""
    from PIL import Image  # imported here so --self-test can report it clearly

    missing, pointers, sources = [], [], []
    for stem in entries:
        path = source_texture_path(stem)
        if not os.path.isfile(path):
            missing.append(stem)
            continue
        if _is_lfs_pointer(path):
            pointers.append(stem)
            continue
        with Image.open(path) as image:
            width, height = image.size
        with open(path, "rb") as handle:
            digest = hashlib.sha256(handle.read()).hexdigest()
        sources.append((stem, path, width, height, digest))

    if missing:
        raise AtlasContractError(
            f"{len(missing)} atlas source(s) are not in the tree: "
            + ", ".join(missing[:8])
        )
    if pointers:
        raise AtlasContractError(
            f"{len(pointers)} atlas source(s) are Git LFS pointers, not images. "
            "Run `git lfs pull` before packing: " + ", ".join(pointers[:8])
        )
    return sources


# ---------------------------------------------------------------------------
# Page composition
# ---------------------------------------------------------------------------

def compose(sources, placements, page_count, page_size=PAGE_SIZE, gutter=GUTTER):
    """Paints every source into its page, with edge-extended bleed."""
    from PIL import Image

    pages = [
        Image.new("RGBA", (page_size, page_size), (0, 0, 0, 0))
        for _ in range(page_count)
    ]
    for stem, path, width, height, _ in sources:
        page_index, x, y, placed_w, placed_h = placements[stem]
        if (placed_w, placed_h) != (width, height):  # pragma: no cover
            raise AtlasContractError(f"{stem} placement size drifted")
        with Image.open(path) as image:
            tile = image.convert("RGBA")
        page = pages[page_index]
        page.paste(tile, (x, y))
        _bleed(page, tile, x, y, gutter)
    return pages


def _bleed(page, tile, x, y, gutter):
    """Extends the tile's border outward so mips cannot sample a neighbour.

    Everything is clipped to the page: an entry flush against the edge simply
    has less bleed on that side, which is what the clamp does anyway.
    """
    width, height = tile.size
    page_w, page_h = page.size

    left_span = min(gutter, x)
    if left_span:
        column = tile.crop((0, 0, 1, height)).resize((left_span, height))
        page.paste(column, (x - left_span, y))
    right_span = min(gutter, page_w - (x + width))
    if right_span:
        column = tile.crop((width - 1, 0, width, height)).resize(
            (right_span, height))
        page.paste(column, (x + width, y))

    band_x0 = x - left_span
    band_x1 = x + width + right_span
    top_span = min(gutter, y)
    if top_span:
        band = page.crop((band_x0, y, band_x1, y + 1))
        page.paste(band.resize((band.width, top_span)), (band_x0, y - top_span))
    bottom_span = min(gutter, page_h - (y + height))
    if bottom_span:
        band = page.crop((band_x0, y + height - 1, band_x1, y + height))
        page.paste(band.resize((band.width, bottom_span)), (band_x0, y + height))


# ---------------------------------------------------------------------------
# Manifest
# ---------------------------------------------------------------------------

def build_manifest(sources, placements, pages, page_size=PAGE_SIZE, gutter=GUTTER):
    entries = {}
    for stem, _, width, height, digest in sources:
        page_index, x, y, _, _ = placements[stem]
        entries[stem] = {
            "page": page_index,
            "x": x,
            "y": y,
            "w": width,
            "h": height,
            "uv_scale": [width / page_size, height / page_size],
            "uv_bias": [x / page_size, y / page_size],
            "source_size": [width, height],
            "source_sha256": digest,
        }
    return {
        "version": MANIFEST_VERSION,
        "page_size": page_size,
        "gutter": gutter,
        "max_mip_levels": MAX_MIP_LEVELS,
        "pages": [
            {
                "index": index,
                "asset": page_file_name(index)[:-4],
                "file": os.path.join("Content", "SourceArt", "Atlas",
                                     page_file_name(index)).replace("\\", "/"),
                "occupancy": round(page.occupancy(), 4),
            }
            for index, page in enumerate(pages)
        ],
        "entries": {name: entries[name] for name in sorted(entries)},
    }


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def command_build(write: bool) -> int:
    sources = collect_sources()
    sizes = [(stem, width, height) for stem, _, width, height, _ in sources]
    placements, pages = pack(sizes)
    manifest = build_manifest(sources, placements, pages)
    validate_manifest(manifest)

    if not write:
        if not os.path.isfile(MANIFEST_PATH):
            print("FAIL no atlas manifest on disk; run without --check")
            return 1
        with open(MANIFEST_PATH, "r", encoding="utf-8") as handle:
            on_disk = json.load(handle)
        if on_disk != manifest:
            print("FAIL atlas manifest is stale; re-run the packer")
            return 1
        for page in manifest["pages"]:
            page_path = os.path.join(ATLAS_DIR, os.path.basename(page["file"]))
            if not os.path.isfile(page_path):
                print(f"FAIL atlas page missing: {page_path}")
                return 1
        print(f"PASS atlas up to date: {len(manifest['entries'])} entries on "
              f"{len(manifest['pages'])} page(s)")
        return 0

    os.makedirs(ATLAS_DIR, exist_ok=True)
    composed = compose(sources, placements, len(pages))
    for index, page in enumerate(composed):
        page.save(os.path.join(ATLAS_DIR, page_file_name(index)), "PNG",
                  optimize=True)
    with open(MANIFEST_PATH, "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2, sort_keys=True)
        handle.write("\n")

    for page in manifest["pages"]:
        print(f"  page {page['index']}: {page['occupancy'] * 100:.1f}% used")
    print(f"PASS packed {len(manifest['entries'])} textures into "
          f"{len(manifest['pages'])} page(s)")
    return 0


def command_self_test() -> int:
    """Proves the packer on synthetic art, so it can be checked anywhere.

    The real sources live in Git LFS and a fresh clone does not have them, but
    the packing, bleeding and manifest maths are the parts that can be wrong,
    and none of them care what the pixels are.
    """
    from PIL import Image

    fixtures = [
        ("T_Fixture_Wide_D", 512, 128),
        ("T_Fixture_Tall_D", 128, 512),
        ("T_Fixture_Square_D", 256, 256),
        ("T_Fixture_Tiny_D", 32, 24),
        ("T_Fixture_Big_D", 1024, 1024),
        ("T_Fixture_Big2_D", 1024, 1024),
        ("T_Fixture_Big3_D", 1024, 1024),
        ("T_Fixture_Big4_D", 1024, 1024),
        ("T_Fixture_Spill_D", 900, 900),
    ]
    placements, pages = pack(fixtures)

    assert len(placements) == len(fixtures), "every fixture must be placed"
    assert len(pages) >= 2, "the fixture set is designed to need a second page"

    for name, width, height in fixtures:
        page_index, x, y, placed_w, placed_h = placements[name]
        assert (placed_w, placed_h) == (width, height), name
        assert x >= 0 and y >= 0, f"{name} starts outside the page"
        assert x + width <= PAGE_SIZE, f"{name} overruns in X"
        assert y + height <= PAGE_SIZE, f"{name} overruns in Y"

    by_page: dict[int, list] = {}
    for name, (page_index, x, y, width, height) in placements.items():
        by_page.setdefault(page_index, []).append((name, x, y, width, height))
    for page_index, rects in by_page.items():
        for first in range(len(rects)):
            an, ax, ay, aw, ah = rects[first]
            for second in range(first + 1, len(rects)):
                bn, bx, by_, bw, bh = rects[second]
                overlap = (ax - GUTTER < bx + bw + GUTTER
                           and bx - GUTTER < ax + aw + GUTTER
                           and ay - GUTTER < by_ + bh + GUTTER
                           and by_ - GUTTER < ay + ah + GUTTER)
                assert not overlap, f"{an} overlaps {bn} on page {page_index}"

    # Determinism: the same inputs in a different order pack the same way.
    shuffled = list(reversed(fixtures))
    again, _ = pack(shuffled)
    assert again == placements, "packing is not order-independent"

    # Compose real pixels and confirm the manifest UVs address them.
    with tempfile.TemporaryDirectory() as work:
        sources = []
        for index, (name, width, height) in enumerate(fixtures):
            colour = (17 * index % 256, 61 * index % 256, 113 * index % 256, 255)
            path = os.path.join(work, f"{name}.png")
            Image.new("RGBA", (width, height), colour).save(path)
            with open(path, "rb") as handle:
                digest = hashlib.sha256(handle.read()).hexdigest()
            sources.append((name, path, width, height, digest))

        composed = compose(sources, placements, len(pages))
        manifest = build_manifest(sources, placements, pages)

        for index, (name, _, width, height) in enumerate(
                [(s[0], s[1], s[2], s[3]) for s in sources]):
            entry = manifest["entries"][name]
            page = composed[entry["page"]]
            colour = (17 * index % 256, 61 * index % 256, 113 * index % 256, 255)
            centre = page.getpixel((entry["x"] + width // 2,
                                    entry["y"] + height // 2))
            assert centre == colour, f"{name} did not land where the manifest says"
            # Where there is room for bleed it must carry the entry's own
            # edge colour, never the page's void.
            if entry["x"] >= GUTTER:
                bleed = page.getpixel((entry["x"] - GUTTER,
                                       entry["y"] + height // 2))
                assert bleed == colour, f"{name} has no bleed on its left edge"
            u = entry["uv_bias"][0] + entry["uv_scale"][0] * 0.5
            v = entry["uv_bias"][1] + entry["uv_scale"][1] * 0.5
            assert page.getpixel((int(u * PAGE_SIZE), int(v * PAGE_SIZE))) == colour, \
                f"{name} UV transform does not address its own pixels"

    # A realistic print set -- the shipping art is 64..512 px, not 1K -- has to
    # land on one page and actually use it. Without this the packer could
    # "pass" while spilling every texture onto a page of its own.
    realistic = []
    for index in range(48):
        width = (64, 128, 192, 256, 320, 512)[index % 6]
        height = (64, 96, 128, 192, 256, 256)[(index * 5) % 6]
        realistic.append((f"T_Realistic{index:02d}_D", width, height))
    small_placements, small_pages = pack(realistic)
    assert len(small_placements) == len(realistic)
    assert len(small_pages) == 1, (
        f"a 48-texture print set should fit one page, took {len(small_pages)}")
    occupancy = small_pages[0].occupancy()

    # Density: three times that set has to stay within one page of the
    # theoretical floor. This is what catches a packer that fits everything but
    # scatters it, which costs pages and therefore draw calls.
    dense = [
        (f"{name}_x{copy}", width, height)
        for copy in range(3)
        for name, width, height in realistic
    ]
    _, dense_pages = pack(dense)
    dense_area = sum(width * height for _, width, height in dense)
    floor_pages = -(-dense_area // (PAGE_SIZE * PAGE_SIZE))
    assert len(dense_pages) <= floor_pages + 1, (
        f"{len(dense_pages)} pages for a {floor_pages}-page set")

    print(f"PASS packer self-test: {len(fixtures)} fixtures, "
          f"{len(pages)} pages, bleed and UV transforms verified; "
          f"48-texture print set packs to one page at {occupancy:.0%}; "
          f"{len(dense)} textures pack to {len(dense_pages)} pages "
          f"(floor {floor_pages})")
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="verify the committed atlas without rewriting it")
    parser.add_argument("--self-test", action="store_true",
                        help="run the packer against synthetic fixtures")
    arguments = parser.parse_args(argv)

    if arguments.self_test:
        return command_self_test()
    try:
        return command_build(write=not arguments.check)
    except AtlasContractError as error:
        print(f"FAIL {error}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
