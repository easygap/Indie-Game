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
    python3 Scripts/build_texture_atlas.py --preflight # can the editor import?
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
    GUTTER_SAFE_MIP_LEVELS,
    MANIFEST_VERSION,
    PAGE_SHAPES,
    PAGE_SIZE,
    PRINT_ATLAS_ENTRIES,
    page_file_name,
    source_texture_path,
    validate_manifest,
    validate_manifest_geometry,
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

    def __init__(self, width: int, height: int):
        self.width = width
        self.height = height
        self.free = [(0, 0, width, height)]
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
        """Share of the page's own area covered by real pixels."""
        area = sum((w - GUTTER * 2) * (h - GUTTER * 2) for _, _, w, h in self.used)
        return area / float(
            (self.width - GUTTER * 2) * (self.height - GUTTER * 2))


def _contains(outer, inner) -> bool:
    ox, oy, ow, oh = outer
    ix, iy, iw, ih = inner
    return ox <= ix and oy <= iy and ox + ow >= ix + iw and oy + oh >= iy + ih


def _pack_into(sizes, page_width, page_height, gutter=GUTTER):
    """Packs everything into one page of the given shape, or returns None.

    Packing runs in a page grown by one gutter on every side, and the gutter is
    then subtracted back off. An entry can therefore sit flush against the page
    edge -- nothing is next to it there, and the sampler clamps -- while any two
    entries still end up two gutters apart.
    """
    page = MaxRectsPage(page_width + gutter * 2, page_height + gutter * 2)
    placements = {}
    for name, width, height in sizes:
        if width > page_width or height > page_height:
            return None
        spot = page.insert(width + gutter * 2, height + gutter * 2)
        if spot is None:
            return None
        placements[name] = (spot[0], spot[1], width, height)
    return placements, page


def _shrink(sizes, gutter=GUTTER):
    """Repacks one page's contents into the smallest shape that holds them.

    Sorts largest-area first here rather than trusting the caller, so the
    result depends only on which entries are on the page, never on the order
    they were handed over.
    """
    order = sorted(sizes, key=lambda item: (-(item[1] * item[2]), item[0]))
    for page_width, page_height in PAGE_SHAPES:
        result = _pack_into(order, page_width, page_height, gutter)
        if result is not None:
            return result
    return None  # pragma: no cover - the caller only ever shrinks a valid page


def pack(sizes, page_size=PAGE_SIZE, gutter=GUTTER):
    """Places (name, width, height) triples. Returns placements and pages.

    Entries are tried largest-area first so the big notices claim their space
    before the door plates fill the gaps, but the *result* is keyed by name, so
    the caller's order does not leak into the layout.

    Two passes. The first fills full-size pages, which decides how many pages
    there are and which entries share one. The second repacks each page on its
    own into the smallest power-of-two shape its contents fit in, so a tail
    page holding a third of a page's worth ships at a third of the size. The
    first pass sets the page count, so shrinking can only ever save memory --
    it can never split a page in two.
    """
    order = sorted(sizes, key=lambda item: (-(item[1] * item[2]), item[0]))
    virtual_size = page_size + gutter * 2
    pages: list[MaxRectsPage] = []
    assigned: list[list[tuple[str, int, int]]] = []

    for name, width, height in order:
        padded_w = width + gutter * 2
        padded_h = height + gutter * 2
        if width > page_size or height > page_size:
            raise AtlasContractError(
                f"{name} is {width}x{height}; too large for a "
                f"{page_size} px page"
            )
        placed = False
        for page_index, page in enumerate(pages):
            if page.insert(padded_w, padded_h) is not None:
                assigned[page_index].append((name, width, height))
                placed = True
                break
        if not placed:
            page = MaxRectsPage(virtual_size, virtual_size)
            if page.insert(padded_w, padded_h) is None:  # pragma: no cover
                raise AtlasContractError(f"{name} did not fit an empty page")
            pages.append(page)
            assigned.append([(name, width, height)])

    placements = {}
    shrunk: list[MaxRectsPage] = []
    for page_index, contents in enumerate(assigned):
        result = _shrink(contents, gutter)
        if result is None:  # pragma: no cover - a full page always refits
            raise AtlasContractError(f"page {page_index} could not be repacked")
        page_placements, page = result
        for name, (x, y, width, height) in page_placements.items():
            placements[name] = (page_index, x, y, width, height)
        shrunk.append(page)
    return placements, shrunk


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

def compose(sources, placements, page_shapes, gutter=GUTTER):
    """Paints every source into its page, with edge-extended bleed.

    page_shapes is one (width, height) per page; pages are not all square and
    not all the same size.
    """
    from PIL import Image

    pages = [
        Image.new("RGBA", shape, (0, 0, 0, 0))
        for shape in page_shapes
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

    Nearest-neighbour throughout. Stretching a one-pixel line with any
    filtering resamples it, and the gutter then differs from the edge it is
    supposed to be a copy of -- by one or two in each channel, which is enough
    to show as a seam once a mip averages across it.
    """
    from PIL import Image

    width, height = tile.size
    page_w, page_h = page.size

    left_span = min(gutter, x)
    if left_span:
        column = tile.crop((0, 0, 1, height)).resize(
            (left_span, height), Image.NEAREST)
        page.paste(column, (x - left_span, y))
    right_span = min(gutter, page_w - (x + width))
    if right_span:
        column = tile.crop((width - 1, 0, width, height)).resize(
            (right_span, height), Image.NEAREST)
        page.paste(column, (x + width, y))

    band_x0 = x - left_span
    band_x1 = x + width + right_span
    top_span = min(gutter, y)
    if top_span:
        band = page.crop((band_x0, y, band_x1, y + 1))
        page.paste(band.resize((band.width, top_span), Image.NEAREST),
                   (band_x0, y - top_span))
    bottom_span = min(gutter, page_h - (y + height))
    if bottom_span:
        band = page.crop((band_x0, y + height - 1, band_x1, y + height))
        page.paste(band.resize((band.width, bottom_span), Image.NEAREST),
                   (band_x0, y + height))


# ---------------------------------------------------------------------------
# Manifest
# ---------------------------------------------------------------------------

def page_shapes(pages, gutter=GUTTER):
    """(width, height) of each page, with the packing gutter taken back off."""
    return [
        (page.width - gutter * 2, page.height - gutter * 2) for page in pages
    ]


def build_manifest(sources, placements, pages, page_size=PAGE_SIZE, gutter=GUTTER):
    shapes = page_shapes(pages, gutter)
    entries = {}
    for stem, _, width, height, digest in sources:
        page_index, x, y, _, _ = placements[stem]
        page_width, page_height = shapes[page_index]
        entries[stem] = {
            "page": page_index,
            "x": x,
            "y": y,
            "w": width,
            "h": height,
            # Normalised against this entry's own page, which is not the same
            # size as every other page.
            "uv_scale": [width / page_width, height / page_height],
            "uv_bias": [x / page_width, y / page_height],
            "source_size": [width, height],
            "source_sha256": digest,
        }
    return {
        "version": MANIFEST_VERSION,
        "page_size": page_size,
        "gutter": gutter,
        "gutter_safe_mip_levels": GUTTER_SAFE_MIP_LEVELS,
        "pages": [
            {
                "index": index,
                "asset": page_file_name(index)[:-4],
                "file": os.path.join("Content", "SourceArt", "Atlas",
                                     page_file_name(index)).replace("\\", "/"),
                "width": shapes[index][0],
                "height": shapes[index][1],
                "occupancy": round(page.occupancy(), 4),
            }
            for index, page in enumerate(pages)
        ],
        "entries": {name: entries[name] for name in sorted(entries)},
    }


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def command_preflight() -> int:
    """Answers "will the editor run work?" before anyone opens the editor.

    The atlas stage of the art build spends several minutes in a commandlet
    before it touches the atlas at all, so every way it can fail is worth
    finding from a shell in a second: Pillow missing, Git LFS never pulled,
    a manifest that no longer matches the sources, pages that were never
    baked or were baked at a different shape.
    """
    problems: list[str] = []
    notes: list[str] = []

    try:
        from PIL import Image as _Image  # noqa: F401
        notes.append("Pillow available")
    except ImportError:
        problems.append(
            "Pillow is not installed; the packer cannot read the source art "
            "(pip install pillow)"
        )

    pointers = [
        stem for stem in PRINT_ATLAS_ENTRIES
        if os.path.isfile(source_texture_path(stem))
        and _is_lfs_pointer(source_texture_path(stem))
    ]
    absent = [
        stem for stem in PRINT_ATLAS_ENTRIES
        if not os.path.isfile(source_texture_path(stem))
    ]
    if absent:
        problems.append(
            f"{len(absent)} source texture(s) are not in the tree: "
            + ", ".join(absent[:6])
        )
    if pointers:
        problems.append(
            f"{len(pointers)} source texture(s) are Git LFS pointers; run "
            "`git lfs pull` first"
        )
    if not absent and not pointers:
        notes.append(f"{len(PRINT_ATLAS_ENTRIES)} source textures present")

    manifest = None
    if not os.path.isfile(MANIFEST_PATH):
        problems.append(
            f"no manifest at {MANIFEST_PATH}; run the packer with no arguments"
        )
    else:
        try:
            with open(MANIFEST_PATH, "r", encoding="utf-8") as handle:
                manifest = json.load(handle)
            validate_manifest(manifest)
            notes.append(
                f"manifest v{manifest['version']}: "
                f"{len(manifest['entries'])} entries on "
                f"{len(manifest['pages'])} page(s)"
            )
        except (AtlasContractError, ValueError) as error:
            manifest = None
            problems.append(f"manifest is not usable: {error}")

    if manifest is not None:
        for page in manifest["pages"]:
            path = os.path.join(ATLAS_DIR, os.path.basename(page["file"]))
            if not os.path.isfile(path):
                problems.append(
                    f"page {page['index']} has not been baked: {path}"
                )
                continue
            try:
                from PIL import Image
                with Image.open(path) as image:
                    shape = image.size
            except Exception as error:  # noqa: BLE001
                problems.append(f"page {page['index']} is unreadable: {error}")
                continue
            if shape != (page["width"], page["height"]):
                problems.append(
                    f"page {page['index']} on disk is {shape[0]}x{shape[1]}, "
                    f"manifest says {page['width']}x{page['height']}; re-bake"
                )
            else:
                notes.append(
                    f"page {page['index']}: {shape[0]}x{shape[1]} "
                    f"({page['occupancy'] * 100:.1f}% used)"
                )

    if manifest is not None and not problems:
        # Everything is present; the last question is whether it is current.
        # --check reports for itself, which would interleave with this report,
        # so run it quietly and keep only the verdict.
        import contextlib
        import io

        sink = io.StringIO()
        with contextlib.redirect_stdout(sink):
            stale = command_build(write=False) != 0
        if stale:
            problems.append(
                "the bake is stale against the current source art; re-run the "
                "packer with no arguments"
            )
        else:
            notes.append("bake matches the current source art")

    for note in notes:
        print(f"  ok    {note}")
    for problem in problems:
        print(f"  FAIL  {problem}")
    if problems:
        print(f"FAIL preflight: {len(problems)} thing(s) block the editor run")
        return 1
    print("PASS preflight: the editor can import this atlas")
    return 0


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
    composed = compose(sources, placements, page_shapes(pages))
    for index, page in enumerate(composed):
        page.save(os.path.join(ATLAS_DIR, page_file_name(index)), "PNG",
                  optimize=True)
    with open(MANIFEST_PATH, "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2, sort_keys=True)
        handle.write("\n")

    for page in manifest["pages"]:
        print(f"  page {page['index']}: {page['width']}x{page['height']}, "
              f"{page['occupancy'] * 100:.1f}% used")
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
    shapes = page_shapes(pages)

    assert len(placements) == len(fixtures), "every fixture must be placed"
    assert len(pages) >= 2, "the fixture set is designed to need a second page"
    for index, (width, height) in enumerate(shapes):
        for extent in (width, height):
            assert 0 < extent <= PAGE_SIZE and extent & (extent - 1) == 0, (
                f"page {index} is {width}x{height}, not a power of two")

    for name, width, height in fixtures:
        page_index, x, y, placed_w, placed_h = placements[name]
        page_width, page_height = shapes[page_index]
        assert (placed_w, placed_h) == (width, height), name
        assert x >= 0 and y >= 0, f"{name} starts outside the page"
        assert x + width <= page_width, f"{name} overruns in X"
        assert y + height <= page_height, f"{name} overruns in Y"

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

        composed = compose(sources, placements, shapes)
        manifest = build_manifest(sources, placements, pages)
        validate_manifest_geometry(manifest)

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
            page_width, page_height = shapes[entry["page"]]
            u = entry["uv_bias"][0] + entry["uv_scale"][0] * 0.5
            v = entry["uv_bias"][1] + entry["uv_scale"][1] * 0.5
            assert page.size == (page_width, page_height), \
                f"composed page {entry['page']} is not the manifest's shape"
            assert page.getpixel(
                (int(u * page_width), int(v * page_height))) == colour, \
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
    small_shape = page_shapes(small_pages)[0]

    # Shrinking: a page that only holds a corner's worth must not ship as a
    # full 2048 square. This is the assertion the tail-page saving rests on.
    # Padding is what decides it, not raw area -- every entry carries two
    # gutters on each axis into the pack.
    sparse = [(f"T_Sparse{index:02d}_D", 256, 256) for index in range(12)]
    sparse_placements, sparse_pages = pack(sparse)
    assert len(sparse_pages) == 1
    sparse_shape = page_shapes(sparse_pages)[0]
    padded = sum(
        (width + GUTTER * 2) * (height + GUTTER * 2)
        for _, width, height in sparse
    )
    assert sparse_shape[0] * sparse_shape[1] < PAGE_SIZE * PAGE_SIZE, (
        f"a sparse page stayed at {sparse_shape[0]}x{sparse_shape[1]}")
    assert sparse_shape[0] * sparse_shape[1] >= padded, "page is too small"
    for name, (page_index, x, y, width, height) in sparse_placements.items():
        assert x + width <= sparse_shape[0] and y + height <= sparse_shape[1], (
            f"{name} overruns the shrunk page")

    # Shrinking must not depend on the order the page's contents arrive in.
    reordered, _ = pack(list(reversed(sparse)))
    assert reordered == sparse_placements, "shrinking is order-dependent"

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
          f"48-texture print set packs to one "
          f"{small_shape[0]}x{small_shape[1]} page at {occupancy:.0%}; "
          f"a sparse page shrinks to {sparse_shape[0]}x{sparse_shape[1]}; "
          f"{len(dense)} textures pack to {len(dense_pages)} pages "
          f"(floor {floor_pages})")
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="verify the committed atlas without rewriting it")
    parser.add_argument("--self-test", action="store_true",
                        help="run the packer against synthetic fixtures")
    parser.add_argument("--preflight", action="store_true",
                        help="report whether an editor import would work")
    arguments = parser.parse_args(argv)

    if arguments.self_test:
        return command_self_test()
    if arguments.preflight:
        try:
            return command_preflight()
        except AtlasContractError as error:
            print(f"FAIL {error}")
            return 1
    try:
        return command_build(write=not arguments.check)
    except AtlasContractError as error:
        print(f"FAIL {error}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
