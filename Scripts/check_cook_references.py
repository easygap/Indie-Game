"""Answer one question without running a cook: what actually ships?

The cooker does not walk C++. It starts from the directories in
``DirectoriesToAlwaysCook``, the Asset Manager's primary-asset rules and the
maps, then follows the *serialized* references inside those packages. A
texture that only ever appears as a string literal in
``LoadObject<UTexture2D>(nullptr, TEXT("/Game/..."))`` is invisible to it: the
editor finds the asset because the whole project is on disk, and the packaged
build gets a null pointer.

That failure is silent in exactly the wrong way -- the HUD keeps drawing, just
without the picture -- and it only appears in a packaged build, which is the
one build nobody runs while iterating. So it gets a static check instead.

The same walk answers the atlas question. Moving a print texture onto a shared
atlas page is only a saving if the individual texture then stops being cooked,
and it stops being cooked only when the last material that sampled it has been
rebuilt to read the page. Before that rebuild all of them still ship, and the
atlas is pure cost.

Two things this deliberately does not do:

* It does not parse Unreal's package format. It scans each package for
  ``/Game/...`` strings, which over-approximates the reference set: some hits
  are property values or thumbnail metadata rather than imports. The bias is
  the safe one. Anything reported as unreachable really is unreachable, and
  the atlas report can only ever under-state the saving.
* It does not know what a cook actually did. It reads the same config the
  cooker reads and follows the same graph, which is enough to catch a
  reference that was never there -- not enough to catch one the cooker
  dropped for its own reasons.

A checkout that did not fetch its LFS content has holes in that graph, and a
hole makes both answers unsound: an asset can look unreachable only because
the package naming it could not be opened. That case skips with a reason
rather than failing or passing.

Run with:
    python Scripts/check_cook_references.py             # report
    python Scripts/check_cook_references.py --check     # exit 1 on findings
    python Scripts/check_cook_references.py --check \\
        --require-atlas-dropped   # after the editor rebuilt the materials
    python Scripts/check_cook_references.py --self-test
    python Scripts/check_cook_references.py --json
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from texture_atlas_contract import (  # noqa: E402
    ATLAS_EXCLUSIONS,
    ATLAS_TEXTURE_ROOT,
    PRINT_ATLAS_ENTRIES,
    PROJECT_ROOT,
)

PACKAGE_SUFFIXES = (".uasset", ".umap")
CODE_SUFFIXES = (".cpp", ".h")

# `/Game/A/B` and `/Game/A/B.B` both appear; the object suffix is dropped so
# every reference is compared as a package path.
GAME_PATH = re.compile(rb"/Game/[A-Za-z0-9_/]+(?:\.[A-Za-z0-9_]+)?")
GAME_PATH_TEXT = re.compile(r"/Game/[A-Za-z0-9_/]+(?:\.[A-Za-z0-9_]+)?")
# Two adjacent string literals are one string. Long paths in the source are
# split across lines to stay inside the column limit, and a scan that reads
# them as written finds half a path.
LITERAL_JOIN = re.compile(r'"\s*"')

LFS_POINTER_MAGIC = b"version https://git-lfs"

ALWAYS_COOK_DIRECTORY = re.compile(
    r"^\+?DirectoriesToAlwaysCook\s*=\s*\(Path\s*=\s*\"([^\"]+)\"\)",
    re.MULTILINE,
)
NEVER_COOK_DIRECTORY = re.compile(
    r"^\+?DirectoriesToNeverCook\s*=\s*\(Path\s*=\s*\"([^\"]+)\"\)",
    re.MULTILINE,
)
PRIMARY_ASSET_RULE = re.compile(
    r"^\+?PrimaryAssetTypesToScan\s*=\s*\((.*)\)\s*$", re.MULTILINE
)
RULE_DIRECTORY = re.compile(r"\(Path\s*=\s*\"([^\"]+)\"\)")
# SpecificAssets is a TArray<FSoftObjectPath>: a parenthesised list of quoted
# `/Game/Package.Object` paths. Matched strictly, so a malformed entry is
# reported as uncovered rather than quietly counted as covered.
RULE_SPECIFIC_ASSETS = re.compile(r"SpecificAssets\s*=\s*\(([^)]*)\)")
RULE_SPECIFIC_ASSET = re.compile(r"\"(/Game/[A-Za-z0-9_/]+\.[A-Za-z0-9_]+)\"")


class CookAudit:
    """The cook's reachable set, and everything that disagrees with it."""

    def __init__(self, project_root: str = PROJECT_ROOT):
        self.project_root = project_root
        self.content_dir = os.path.join(project_root, "Content")
        self.source_dir = os.path.join(project_root, "Source")
        self.game_config = os.path.join(project_root, "Config", "DefaultGame.ini")
        self.engine_config = os.path.join(
            project_root, "Config", "DefaultEngine.ini")
        self.always_cook: list[str] = []
        self.never_cook: list[str] = []
        self.always_cook_assets: set[str] = set()
        self.packages: dict[str, str] = {}      # package path -> file path
        self.unreadable: set[str] = set()       # LFS pointers, not fetched
        self.references: dict[str, set[str]] = {}
        self.roots: set[str] = set()
        self.cooked: set[str] = set()
        self.code_paths: dict[str, list[str]] = {}  # package path -> sites


def _package_path(audit: CookAudit, file_path: str) -> str:
    relative = os.path.relpath(file_path, audit.content_dir).replace(os.sep, "/")
    return "/Game/" + os.path.splitext(relative)[0]


def _read_config(path: str) -> str:
    if not os.path.isfile(path):
        return ""
    with open(path, "r", encoding="utf-8-sig") as handle:
        return handle.read()


def _cook_directories(audit: CookAudit) -> None:
    game = _read_config(audit.game_config)
    audit.always_cook = [
        p.rstrip("/") for p in ALWAYS_COOK_DIRECTORY.findall(game)
    ]
    audit.never_cook = [
        p.rstrip("/") for p in NEVER_COOK_DIRECTORY.findall(game)
    ]

    # The Asset Manager cooks primary assets by its own rules, which name both
    # directories and individual assets the packaging settings never mention.
    # SpecificAssets is the only per-asset cook rule Unreal offers: an asset
    # that nothing serialized references, and that only C++ loads by path,
    # ships because it is named here or it does not ship at all.
    for body in PRIMARY_ASSET_RULE.findall(_read_config(audit.engine_config)):
        if "CookRule=AlwaysCook" not in body.replace(" ", ""):
            continue
        audit.always_cook.extend(
            p.rstrip("/") for p in RULE_DIRECTORY.findall(body)
        )
        for group in RULE_SPECIFIC_ASSETS.findall(body):
            for path in RULE_SPECIFIC_ASSET.findall(group):
                audit.always_cook_assets.add(path.split(".")[0])

    # One entry per directory, in the order the config named them.
    audit.always_cook = list(dict.fromkeys(audit.always_cook))


def _collect_packages(audit: CookAudit) -> None:
    for directory, _subdirs, files in os.walk(audit.content_dir):
        for name in files:
            if not name.endswith(PACKAGE_SUFFIXES):
                continue
            file_path = os.path.join(directory, name)
            package = _package_path(audit, file_path)
            audit.packages[package] = file_path

            with open(file_path, "rb") as handle:
                blob = handle.read()
            if blob.startswith(LFS_POINTER_MAGIC):
                # Present in the tree, but the bytes are in LFS and were not
                # fetched. Its outgoing references cannot be read.
                audit.unreadable.add(package)
                audit.references[package] = set()
                continue

            found = set()
            for match in GAME_PATH.finditer(blob):
                reference = match.group().decode("ascii").split(".")[0]
                if reference != package:
                    found.add(reference)
            audit.references[package] = found


def _is_under(package: str, directory: str) -> bool:
    return package == directory or package.startswith(directory + "/")


def blind_spots(audit: CookAudit) -> set[str]:
    """Cooked packages whose own references could not be read.

    An unfetched LFS pointer is a hole in the graph, and every verdict that
    depends on the graph is unsound while one is inside the cook set: an
    asset can look unreachable only because the package that names it could
    not be opened. Reported, not guessed at.
    """
    return audit.unreadable & audit.cooked


def _seed_roots(audit: CookAudit) -> None:
    for package in audit.packages:
        if any(_is_under(package, d) for d in audit.never_cook):
            continue
        # Maps are always a cook root: a level that is not cooked is not a
        # game. Everything else has to be named by config.
        if audit.packages[package].endswith(".umap"):
            audit.roots.add(package)
        elif package in audit.always_cook_assets:
            audit.roots.add(package)
        elif any(_is_under(package, d) for d in audit.always_cook):
            audit.roots.add(package)


def _close(audit: CookAudit) -> None:
    pending = list(audit.roots)
    seen = set(pending)
    while pending:
        package = pending.pop()
        for reference in audit.references.get(package, ()):
            if reference in seen:
                continue
            if reference not in audit.packages:
                continue  # a path into a plugin or an asset not in this tree
            if any(_is_under(reference, d) for d in audit.never_cook):
                continue
            seen.add(reference)
            pending.append(reference)
    audit.cooked = seen


def _collect_code_paths(audit: CookAudit) -> None:
    for directory, _subdirs, files in os.walk(audit.source_dir):
        for name in files:
            if not name.endswith(CODE_SUFFIXES):
                continue
            file_path = os.path.join(directory, name)
            with open(file_path, "r", encoding="utf-8", errors="replace") as f:
                text = LITERAL_JOIN.sub("", f.read())
            relative = os.path.relpath(file_path, audit.project_root)
            for match in GAME_PATH_TEXT.finditer(text):
                reference = match.group().split(".")[0].rstrip("/")
                if reference.count("/") < 3:
                    continue  # /Game/Dir alone is a directory, not an asset
                sites = audit.code_paths.setdefault(reference, [])
                if relative not in sites:
                    sites.append(relative)


def run_audit(project_root: str = PROJECT_ROOT) -> CookAudit:
    audit = CookAudit(project_root)
    _cook_directories(audit)
    _collect_packages(audit)
    _seed_roots(audit)
    _close(audit)
    _collect_code_paths(audit)
    return audit


def code_only_assets(audit: CookAudit) -> list[tuple[str, list[str]]]:
    """Assets that exist, that code loads by path, that no cook root reaches.

    These load in the editor and are null in a packaged build.
    """
    findings = []
    for package, sites in sorted(audit.code_paths.items()):
        if package not in audit.packages:
            continue  # built later by a script, or a typo; a separate problem
        if package in audit.cooked:
            continue
        findings.append((package, sites))
    return findings


def code_load_coverage(audit: CookAudit) -> dict[str, str]:
    """Why each asset that only code loads survives the cook."""
    coverage = {}
    for package in sorted(audit.code_paths):
        if package not in audit.packages or package not in audit.cooked:
            continue
        if package in audit.always_cook_assets:
            coverage[package] = "named by an AlwaysCook primary-asset rule"
    return coverage


def _is_content_directory(audit: CookAudit, package: str) -> bool:
    """`/Game/Prototype/Materials` is a printf prefix, not an asset."""
    relative = package[len("/Game/"):].replace("/", os.sep)
    return os.path.isdir(os.path.join(audit.content_dir, relative))


def missing_code_assets(audit: CookAudit) -> list[tuple[str, list[str]]]:
    """Paths code loads that are not in the tree at all."""
    return [
        (package, sites)
        for package, sites in sorted(audit.code_paths.items())
        if package not in audit.packages
        and not _is_content_directory(audit, package)
    ]


def atlas_status(audit: CookAudit, entries=PRINT_ATLAS_ENTRIES) -> dict:
    """How many of the contracted print textures the cook still pulls in."""
    still_cooked = {}
    for stem in entries:
        package = f"{ATLAS_TEXTURE_ROOT}/{stem}"
        if package not in audit.packages:
            continue
        if package not in audit.cooked:
            continue
        referrers = sorted(
            other for other in audit.cooked
            if package in audit.references.get(other, ())
        )
        still_cooked[stem] = referrers
    return {
        "contracted": len(entries),
        "still_cooked": still_cooked,
        "dropped": [stem for stem in entries if stem not in still_cooked],
    }


def atlas_entries_loaded_by_code(
    audit: CookAudit, entries=PRINT_ATLAS_ENTRIES
) -> list[tuple[str, list[str]]]:
    """Contracted atlas entries that C++ also loads by package path.

    A canvas draw has no UV transform, so these cannot read a page -- and the
    string load is invisible to the cooker, so nothing would keep the
    individual texture in the build once its material stops sampling it.
    They belong in ATLAS_EXCLUSIONS, not on a page.
    """
    conflicts = []
    for stem in entries:
        sites = audit.code_paths.get(f"{ATLAS_TEXTURE_ROOT}/{stem}")
        if sites:
            conflicts.append((stem, sites))
    return conflicts


def gate(
    audit: CookAudit,
    require_atlas_dropped: bool = False,
    entries=PRINT_ATLAS_ENTRIES,
) -> tuple[int, str]:
    """(exit code, closing line) for --check. Separated so it can be tested."""
    # An atlas entry that code also loads by path is read off the contract and
    # the source, so this verdict holds whatever the working tree fetched.
    conflicts = atlas_entries_loaded_by_code(audit, entries)
    if conflicts:
        return 1, (
            f"FAIL {len(conflicts)} atlassed texture(s) are loaded by code; "
            "they belong in ATLAS_EXCLUSIONS"
        )

    # Everything below is read off the reference graph. With a hole in it a
    # finding may be an artefact of the hole, and so may its absence, so
    # neither gate can be honoured. Say which, rather than passing quietly.
    blind = blind_spots(audit)
    if blind:
        return 0, (
            f"SKIP {len(blind)} cooked package(s) not fetched from LFS; "
            "reachability could not be decided. Run git lfs pull."
        )

    code_only = code_only_assets(audit)
    if code_only:
        return 1, (
            f"FAIL {len(code_only)} asset(s) only code reaches; they are "
            "absent from the pak and null at runtime"
        )

    still_cooked = atlas_status(audit, entries)["still_cooked"]
    if require_atlas_dropped and still_cooked:
        return 1, (
            f"FAIL {len(still_cooked)} atlassed texture(s) are still cooked; "
            "the print materials were not rebuilt"
        )
    return 0, ""


def _report(audit: CookAudit) -> None:
    print(
        f"COOK REFERENCE AUDIT  packages={len(audit.packages)} "
        f"roots={len(audit.roots)} reachable={len(audit.cooked)} "
        f"unreadable={len(audit.unreadable)}"
    )
    print("  always cook: " + ", ".join(audit.always_cook))
    if audit.never_cook:
        print("  never cook:  " + ", ".join(audit.never_cook))
    blind = blind_spots(audit)
    if blind:
        print(
            f"  {len(blind)} cooked package(s) are unfetched LFS pointers, so "
            "the reference graph has holes and everything below is advisory "
            "only (run git lfs pull)"
        )
    elif audit.unreadable:
        print(
            f"  {len(audit.unreadable)} unfetched LFS pointer(s), none of them "
            "cooked; the reference graph is complete"
        )

    conflicts = atlas_entries_loaded_by_code(audit)
    if conflicts:
        print(f"\n{len(conflicts)} atlas entr(y/ies) are also loaded by code:")
        for stem, sites in conflicts:
            print(f"  [ATLAS_CODE_LOAD] {stem}  <- {', '.join(sites)}")

    findings = code_only_assets(audit)
    if findings:
        print(f"\n{len(findings)} asset(s) reachable only from code:")
        for package, sites in findings:
            print(f"  [CODE_ONLY] {package}")
            print(f"      loaded by {', '.join(sites)}")
            print("      no cooked package references it; null once packaged")

    missing = missing_code_assets(audit)
    if missing:
        print(f"\n{len(missing)} path(s) code loads that are not in the tree:")
        for package, sites in missing:
            print(f"  [ABSENT] {package}  <- {', '.join(sites)}")

    status = atlas_status(audit)
    kept = len(status["still_cooked"])
    print(
        f"\nprint atlas: {len(status['dropped'])}/{status['contracted']} "
        f"contracted textures are out of the cook, {kept} still in"
    )
    if kept:
        example = sorted(status["still_cooked"])[:3]
        for stem in example:
            referrers = status["still_cooked"][stem] or ["(no referrer found)"]
            print(f"  {stem} <- {', '.join(r.rsplit('/', 1)[-1] for r in referrers)}")
        if kept > len(example):
            print(f"  ... and {kept - len(example)} more")
        print(
            "  expected before the editor rebuild: the committed materials "
            "still sample their own texture."
        )
    print(f"  {len(ATLAS_EXCLUSIONS)} texture(s) deliberately off the atlas")


def _fixture(root: str, always_cook_asset: str | None, atlas_material_reads:
             str) -> str:
    """A four-package project: a map, a material, a texture and a HUD frame.

    ``atlas_material_reads`` is the package the print material samples, which
    is what changes when the editor rebuilds it to read an atlas page.
    """
    os.makedirs(os.path.join(root, "Config"), exist_ok=True)
    with open(os.path.join(root, "Config", "DefaultGame.ini"), "w") as handle:
        handle.write(
            "[/Script/UnrealEd.ProjectPackagingSettings]\n"
            "+DirectoriesToAlwaysCook=(Path=\"/Game/Prototype/Materials\")\n"
        )
    specific = f'"{always_cook_asset}.{always_cook_asset.rsplit("/", 1)[-1]}"' \
        if always_cook_asset else ""
    with open(os.path.join(root, "Config", "DefaultEngine.ini"), "w") as handle:
        handle.write(
            "[/Script/Engine.AssetManagerSettings]\n"
            "+PrimaryAssetTypesToScan=(PrimaryAssetType=\"T\","
            "AssetBaseClass=\"/Script/Engine.Texture2D\","
            f"Directories=,SpecificAssets=({specific}),"
            "Rules=(Priority=0,ChunkId=-1,CookRule=AlwaysCook))\n"
        )

    def package(relative: str, body: str) -> None:
        path = os.path.join(root, "Content", *relative.split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as handle:
            handle.write(body.encode("ascii"))

    # The map is the only root that is not named by a directory rule, so it
    # carries the one reference that proves maps are seeded.
    package("Maps/Level.umap", "/Game/Meshes/SM_Placed")
    package("Meshes/SM_Placed.uasset", "leaf")
    package("Prototype/Materials/M_Print.uasset", atlas_material_reads)
    package("Prototype/Textures/T_Print_D.uasset", "leaf")
    package("Prototype/Textures/T_PrintAtlas0_D.uasset", "leaf")
    package("Prototype/Textures/T_HudFrame_D.uasset", "leaf")

    source = os.path.join(root, "Source", "Player")
    os.makedirs(source, exist_ok=True)
    with open(os.path.join(source, "HUD.cpp"), "w") as handle:
        # Split mid-path across two literals, the way IGListenerEntity.cpp
        # writes a path too long for the column limit. Read as written this
        # is a directory and an unrelated word, not an asset.
        handle.write(
            'X = LoadObject<UTexture2D>(nullptr,\n'
            '    TEXT("/Game/Prototype/Textures/"\n'
            '        "T_HudFrame_D.T_HudFrame_D"));\n'
        )
    return root


def command_self_test() -> int:
    """Exercise both verdicts on a synthetic project.

    The gate that matters here -- ``--require-atlas-dropped`` -- cannot pass
    in this repository until an editor has rebuilt the print materials, so
    without this it would ship never having been seen to pass at all.
    """
    import tempfile

    entries = ("T_Print_D",)
    hud = "/Game/Prototype/Textures/T_HudFrame_D"
    with tempfile.TemporaryDirectory() as tmp:
        # 1. Before the rebuild: the material samples its own texture, and
        #    nothing but code names the HUD frame.
        before = _fixture(
            os.path.join(tmp, "before"), None,
            "/Game/Prototype/Textures/T_Print_D")
        audit = run_audit(before)
        assert "/Game/Meshes/SM_Placed" in audit.cooked, \
            "a mesh the map places is not cooked; maps are not seeded as roots"
        assert [p for p, _ in code_only_assets(audit)] == [hud], \
            "a code-only asset was not reported"
        assert atlas_status(audit, entries)["still_cooked"], \
            "an atlassed texture its material still samples looks dropped"
        assert not atlas_entries_loaded_by_code(audit, entries), \
            "a texture code does not load was reported as a code load"

        # 2. After: the material reads the page, and the config names the
        #    frame. Both findings clear, and the texture leaves the cook.
        after = _fixture(
            os.path.join(tmp, "after"), hud,
            "/Game/Prototype/Textures/T_PrintAtlas0_D")
        audit = run_audit(after)
        assert not code_only_assets(audit), \
            "an asset named by SpecificAssets is still reported unreachable"
        assert not atlas_status(audit, entries)["still_cooked"], \
            "a texture nothing references any more still looks cooked"
        assert hud in audit.cooked, "the SpecificAssets rule was not parsed"

        # 3. The conflict the atlas contract exists to prevent: a texture on
        #    a page that code also loads by path.
        audit = run_audit(after)
        conflicts = atlas_entries_loaded_by_code(audit, ("T_HudFrame_D",))
        assert conflicts and conflicts[0][0] == "T_HudFrame_D", \
            "an atlassed texture loaded by code was not reported"

        # 4. A checkout that did not fetch LFS. The material is a pointer, so
        #    its texture looks unreferenced -- which is a hole in the graph,
        #    not a finding, and the gate has to say so instead of failing.
        blinded = _fixture(
            os.path.join(tmp, "blinded"), hud,
            "/Game/Prototype/Textures/T_Print_D")
        material = os.path.join(
            blinded, "Content", "Prototype", "Materials", "M_Print.uasset")
        with open(material, "wb") as handle:
            # Written out rather than built from LFS_POINTER_MAGIC, so that
            # breaking the constant breaks the detection instead of moving
            # the fixture along with it.
            handle.write(
                b"version https://git-lfs.github.com/spec/v1\n"
                b"oid sha256:0000000000000000000000000000000000000000"
                b"000000000000000000000000\nsize 1\n"
            )
        audit = run_audit(blinded)
        assert blind_spots(audit) == {"/Game/Prototype/Materials/M_Print"}, \
            "an unfetched cooked package was not reported as a blind spot"
        assert not atlas_status(audit, entries)["still_cooked"], \
            "the fixture no longer exercises the hole"
        code, message = gate(audit, require_atlas_dropped=True, entries=entries)
        assert code == 0 and message and message.startswith("SKIP"), \
            f"a hole in the graph was treated as a verdict: {code} {message}"

    print(
        "PASS cook reference self-test: code-only detection, SpecificAssets "
        "coverage, atlas drop, the code-load conflict and the unfetched-LFS "
        "hole all verified"
    )
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="exit 1 when any finding is reported")
    parser.add_argument("--json", action="store_true",
                        help="machine-readable")
    parser.add_argument(
        "--require-atlas-dropped", action="store_true",
        help="also fail while a contracted atlas texture is still cooked; "
             "for use after the editor has rebuilt the print materials")
    parser.add_argument("--self-test", action="store_true",
                        help="run the audit against a synthetic project")
    arguments = parser.parse_args(argv)

    if arguments.self_test:
        return command_self_test()

    audit = run_audit()
    conflicts = atlas_entries_loaded_by_code(audit)
    code_only = code_only_assets(audit)
    status = atlas_status(audit)

    if arguments.json:
        print(json.dumps({
            "packages": len(audit.packages),
            "roots": sorted(audit.roots),
            "cooked": len(audit.cooked),
            "unreadable": sorted(audit.unreadable),
            "atlas_code_load": {s: sites for s, sites in conflicts},
            "code_only": {p: sites for p, sites in code_only},
            "absent": {p: sites for p, sites in missing_code_assets(audit)},
            "atlas": status,
        }, indent=2))
    else:
        _report(audit)

    if not arguments.check:
        return 0

    code, message = gate(audit, arguments.require_atlas_dropped)
    if message:
        print("\n" + message)
    return code


if __name__ == "__main__":
    sys.exit(main())
