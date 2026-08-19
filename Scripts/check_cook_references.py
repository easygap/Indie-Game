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

The Asset Manager's rules are *parsed*, not pattern-matched. They are the only
per-asset cook rule Unreal offers, so they are the only thing holding an asset
that nothing serialized references -- and a rule Unreal cannot read is a log
line nobody reads and a blank draw in the pak. Every way one can look right
and hold nothing is checked: a field name that is not a field, a CookRule that
is not one of the enum values, a `Package.Object` path whose halves disagree,
an asset that is not in the tree, an AssetBaseClass the named assets are not.
``--explain-rules`` then re-runs reachability with the rules removed and says,
one asset at a time, whether the rule is what keeps it.

``--simulate-rebuild`` answers the atlas half the same way, before the editor
has run. The rebuild changes exactly one edge per material -- the diffuse
sample moves from the texture to the page -- so the simulation makes that edit
to the graph and re-runs reachability. What matters in the output is not that
the textures leave; it is what else does, and what refuses to. A mesh slot
naming a texture directly, a cook rule covering one, a ``T_Photo_`` twin the
material samples instead, a pre-atlas sampler that survived an in-place
update, or a package reachable only through a texture: each is a page paid for
and nothing saved, or a shipping bug the atlas caused.

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
    python Scripts/check_cook_references.py --explain-rules
    python Scripts/check_cook_references.py --simulate-rebuild
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

import texture_atlas_contract  # noqa: E402
import ue_config  # noqa: E402
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
PRIMARY_ASSET_SETTING = "PrimaryAssetTypesToScan"

# FPrimaryAssetTypeInfo and FPrimaryAssetRules, as Unreal declares them. A rule
# is only worth trusting if every field it names is a field that exists: Unreal
# skips what it cannot map and logs, so a misspelled key is a rule that holds
# nothing and says so where nobody looks.
PRIMARY_ASSET_TYPE_FIELDS = frozenset({
    "PrimaryAssetType", "AssetBaseClass", "bHasBlueprintClasses",
    "bIsEditorOnly", "Directories", "SpecificAssets", "Rules",
})
PRIMARY_ASSET_RULE_FIELDS = frozenset({
    "Priority", "ChunkId", "bApplyRecursively", "CookRule", "bIsEditorOnly",
})
COOK_RULES = frozenset({
    "Unknown", "NeverCook", "ProductionNeverCook",
    "DevelopmentAlwaysProductionNeverCook", "DevelopmentAlwaysCook",
    "AlwaysCook",
})
# `/Game/Path/Asset.Object`. Unreal resolves the half after the dot as the
# object inside the package, so for a top-level asset the two halves are the
# same word and a mismatch resolves to nothing.
SOFT_OBJECT_PATH = re.compile(
    r"^(/Game/[A-Za-z0-9_/]+?/([A-Za-z0-9_]+))\.([A-Za-z0-9_]+)$")

# Where create_textured_materials.py writes. The rebuild rewrites references
# from here and nowhere else.
MATERIAL_ROOT = "/Game/Prototype/Materials"


class CookAudit:
    """The cook's reachable set, and everything that disagrees with it."""

    def __init__(self, project_root: str = PROJECT_ROOT):
        self.project_root = project_root
        self.content_dir = os.path.join(project_root, "Content")
        self.source_dir = os.path.join(project_root, "Source")
        self.game_config = os.path.join(project_root, "Config", "DefaultGame.ini")
        self.engine_config = os.path.join(
            project_root, "Config", "DefaultEngine.ini")
        self.atlas_manifest = os.path.join(
            project_root, "Content", "SourceArt", "Atlas", "print_atlas.json")
        self.always_cook: list[str] = []
        self.never_cook: list[str] = []
        self.always_cook_assets: set[str] = set()
        self.primary_asset_rules: list[PrimaryAssetRule] = []
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


class PrimaryAssetRule:
    """One parsed `+PrimaryAssetTypesToScan=` line, and what is wrong with it."""

    def __init__(self, line: int, text: str):
        self.line = line
        self.text = text
        self.name = "?"
        self.base_class = ""
        self.cook_rule = ""
        self.fields: dict = {}
        self.directories: list[str] = []
        self.assets: list[str] = []          # full `/Game/Pkg.Object` paths
        self.packages: list[str] = []        # just `/Game/Pkg`
        self.problems: list[str] = []

    @property
    def always_cook(self) -> bool:
        return self.cook_rule == "AlwaysCook"


def _validate_rule_fields(rule: PrimaryAssetRule, fields: dict) -> None:
    unknown = sorted(set(fields) - PRIMARY_ASSET_TYPE_FIELDS)
    if unknown:
        rule.problems.append(
            "FPrimaryAssetTypeInfo has no field(s) " + ", ".join(unknown)
            + "; Unreal drops what it cannot map"
        )
    for required in ("PrimaryAssetType", "AssetBaseClass", "Rules"):
        if required not in fields:
            rule.problems.append(f"the rule names no {required}")

    nested = fields.get("Rules")
    if not isinstance(nested, dict):
        rule.problems.append("Rules is not a struct")
        return
    unknown = sorted(set(nested) - PRIMARY_ASSET_RULE_FIELDS)
    if unknown:
        rule.problems.append(
            "FPrimaryAssetRules has no field(s) " + ", ".join(unknown))
    rule.cook_rule = str(nested.get("CookRule", ""))
    if rule.cook_rule not in COOK_RULES:
        rule.problems.append(
            f"CookRule {rule.cook_rule!r} is not an EPrimaryAssetCookRule "
            "value")
    for numeric in ("Priority", "ChunkId"):
        if numeric in nested:
            try:
                ue_config.as_int(nested[numeric])
            except ue_config.ConfigSyntaxError as error:
                rule.problems.append(f"Rules.{numeric}: {error}")
    for flag in ("bApplyRecursively", "bIsEditorOnly"):
        if flag in nested:
            try:
                ue_config.as_bool(nested[flag])
            except ue_config.ConfigSyntaxError as error:
                rule.problems.append(f"Rules.{flag}: {error}")


def _validate_rule_assets(audit: CookAudit, rule: PrimaryAssetRule) -> None:
    """Every named asset resolves, and is of the class the rule scans for.

    The Asset Manager only registers an asset that is a subclass of
    AssetBaseClass. A path that resolves to nothing, or to the wrong class,
    leaves the rule holding nothing at all -- which looks exactly like a rule
    that works until the pak is opened.
    """
    expected_class = rule.base_class.rsplit(".", 1)[-1]
    for path in rule.assets:
        match = SOFT_OBJECT_PATH.match(path)
        if match is None:
            rule.problems.append(f"{path} is not a /Game/Package.Object path")
            continue
        package, asset_name, object_name = match.groups()
        if object_name != asset_name:
            rule.problems.append(
                f"{path} names object {object_name} inside package "
                f"{asset_name}; a top-level asset has one name"
            )
            continue
        file_path = audit.packages.get(package)
        if file_path is None:
            rule.problems.append(f"{path} is not in the tree")
            continue
        if package in audit.unreadable:
            continue  # cannot check the class of a package we cannot open
        if expected_class:
            with open(file_path, "rb") as handle:
                blob = handle.read()
            if expected_class.encode("ascii") not in blob:
                rule.problems.append(
                    f"{path} does not look like a {expected_class}; the rule "
                    "would register nothing for it"
                )


def _read_primary_asset_rules(audit: CookAudit) -> None:
    """Parse the Asset Manager rules instead of pattern-matching them."""
    text = _read_config(audit.engine_config)
    for line, body in ue_config.ini_entries(text, PRIMARY_ASSET_SETTING):
        rule = PrimaryAssetRule(line, body)
        audit.primary_asset_rules.append(rule)
        try:
            fields = ue_config.parse_struct(body)
        except ue_config.ConfigSyntaxError as error:
            rule.problems.append(f"unparseable: {error}")
            continue

        rule.fields = fields
        rule.name = str(fields.get("PrimaryAssetType", "?"))
        rule.base_class = str(fields.get("AssetBaseClass", ""))
        _validate_rule_fields(rule, fields)

        try:
            for entry in ue_config.as_array(fields.get("Directories")):
                if not isinstance(entry, dict) or "Path" not in entry:
                    rule.problems.append(
                        "Directories takes FDirectoryPath structs, "
                        f"found {entry!r}")
                    continue
                rule.directories.append(str(entry["Path"]).rstrip("/"))
            rule.assets = [
                str(entry)
                for entry in ue_config.as_array(fields.get("SpecificAssets"))
            ]
        except ue_config.ConfigSyntaxError as error:
            rule.problems.append(str(error))
            continue

        _validate_rule_assets(audit, rule)
        rule.packages = [
            path.split(".")[0] for path in rule.assets
            if SOFT_OBJECT_PATH.match(path)
        ]
        if not rule.directories and not rule.packages:
            rule.problems.append("the rule names no directory and no asset")

    # Differential: the rules in a project are written to one shape. A rule
    # whose field set differs from every other one is usually a rule that was
    # typed rather than exported, and the difference is the mistake.
    shapes = [
        (rule, frozenset(rule.fields)) for rule in audit.primary_asset_rules
    ]
    counts: dict[frozenset, int] = {}
    for _rule, shape in shapes:
        if shape:
            counts[shape] = counts.get(shape, 0) + 1
    if len(counts) > 1:
        common = max(counts, key=lambda shape: counts[shape])
        for rule, shape in shapes:
            if shape and shape != common:
                rule.problems.append(
                    "field set differs from the other rules in this file: "
                    + ", ".join(sorted(shape ^ common))
                )


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
    _read_primary_asset_rules(audit)
    for rule in audit.primary_asset_rules:
        if not rule.always_cook:
            continue
        audit.always_cook.extend(rule.directories)
        audit.always_cook_assets.update(rule.packages)

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
    # Packages first: validating a rule means resolving the assets it names,
    # which needs to know what is in the tree.
    _collect_packages(audit)
    _cook_directories(audit)
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


def atlas_rule_cover(audit: CookAudit, entries=PRINT_ATLAS_ENTRIES) -> dict:
    """Which contracted textures a cook rule or directory would hold anyway.

    The whole saving rests on nothing naming these except the materials that
    are about to stop naming them. A rule or an always-cook directory that
    covers one keeps it in the pak after the rebuild, and it is then paying
    for a page it does not use.
    """
    covered = {}
    for stem in entries:
        package = f"{ATLAS_TEXTURE_ROOT}/{stem}"
        reasons = []
        if package in audit.always_cook_assets:
            for rule in audit.primary_asset_rules:
                if package in rule.packages:
                    reasons.append(
                        f"named by the {rule.name} rule "
                        f"({audit.engine_config}:{rule.line})")
        for directory in audit.always_cook:
            if _is_under(package, directory):
                reasons.append(f"inside always-cook directory {directory}")
        if audit.packages.get(package, "").endswith(".umap"):
            reasons.append("is a map, which is always a cook root")
        if reasons:
            covered[stem] = reasons
    return covered


def atlas_photo_shadows(audit: CookAudit, entries=PRINT_ATLAS_ENTRIES) -> dict:
    """Contracted textures that have a `T_Photo_` twin in the tree.

    create_textured_materials._load_texture prefers the photo capture over the
    procedural texture of the same name. Where one exists the material samples
    it, so a page packed from the procedural source would put different art on
    screen -- and the photo twin, not the contracted one, is what stays cooked.
    """
    shadows = {}
    for stem in entries:
        if not stem.startswith("T_"):
            continue
        twin = f"{ATLAS_TEXTURE_ROOT}/T_Photo_{stem[2:]}"
        if twin in audit.packages:
            shadows[stem] = twin
    return shadows


def atlas_stale_samples(audit: CookAudit, entries=PRINT_ATLAS_ENTRIES) -> dict:
    """Packages that reference an atlas page *and* a texture it replaced.

    A material that reads the page has no use for the individual texture, so
    naming both means the pre-atlas sampler survived the rebuild -- which is
    what the in-place update path does if nothing retires it. The material
    draws from the page and the texture still ships: the page is paid for and
    nothing is saved.
    """
    contracted = {f"{ATLAS_TEXTURE_ROOT}/{stem}" for stem in entries}
    page_prefix = f"{ATLAS_TEXTURE_ROOT}/{texture_atlas_contract.ATLAS_PAGE_PREFIX}"
    stale = {}
    for package in sorted(audit.cooked):
        references = audit.references.get(package, set())
        if not any(other.startswith(page_prefix) for other in references):
            continue
        left_over = sorted(references & contracted)
        if left_over:
            stale[package] = left_over
    return stale


def simulate_atlas_rebuild(audit: CookAudit, entries=PRINT_ATLAS_ENTRIES) -> dict:
    """Re-run reachability as if the editor had rebuilt the print materials.

    The rebuild changes exactly one thing per material: the diffuse sample
    moves from the individual texture to the atlas page. So the simulation is
    an edit to the graph, not a guess -- for every package that references a
    contracted texture, swap that edge for an edge to the page the manifest
    put it on. Everything else about the material, including the companion
    normal/roughness maps it also samples, is left alone because the rebuild
    leaves it alone.

    The interesting output is not that the textures leave. It is what *else*
    leaves, because a collateral drop is a shipping bug the atlas caused.
    """
    # Layout only: which stem landed on which page. Whether the manifest holds
    # the contracted set is build_texture_atlas.py --check's question.
    manifest = texture_atlas_contract.try_load_manifest(
        audit.atlas_manifest, strict=False)
    pages = {}
    if manifest is not None:
        for stem in entries:
            entry = (manifest.get("entries") or {}).get(stem)
            if entry is not None:
                pages[stem] = texture_atlas_contract.page_package_path(
                    entry["page"])

    after = CookAudit(audit.project_root)
    after.always_cook = list(audit.always_cook)
    after.never_cook = list(audit.never_cook)
    after.always_cook_assets = set(audit.always_cook_assets)
    after.primary_asset_rules = audit.primary_asset_rules
    after.packages = dict(audit.packages)
    after.unreadable = set(audit.unreadable)
    after.code_paths = audit.code_paths
    after.references = {
        package: set(references)
        for package, references in audit.references.items()
    }

    # The pages do not exist until the editor imports them; the graph has to
    # carry them for the rebuild to be reachable through.
    for page in set(pages.values()):
        after.packages.setdefault(page, "<imported by the atlas stage>")
        after.references.setdefault(page, set())

    # Only the print materials are rebuilt. An edge from anywhere else -- a
    # mesh's material slot, a map, a data asset -- is one create_textured_
    # materials.py never touches, so the simulation must not touch it either:
    # that texture goes on shipping and the page is spent for nothing.
    material_root = MATERIAL_ROOT
    rewritten = {}
    stragglers = {}
    for stem in entries:
        package = f"{ATLAS_TEXTURE_ROOT}/{stem}"
        page = pages.get(stem)
        if page is None:
            continue
        for referrer, references in after.references.items():
            if package not in references:
                continue
            if _is_under(referrer, material_root):
                references.discard(package)
                references.add(page)
                rewritten.setdefault(stem, []).append(referrer)
            else:
                stragglers.setdefault(stem, []).append(referrer)

    _seed_roots(after)
    _close(after)

    dropped = sorted(audit.cooked - after.cooked)
    gained = sorted(after.cooked - audit.cooked)
    expected = {
        f"{ATLAS_TEXTURE_ROOT}/{stem}" for stem in entries
        if stem in pages
    }
    return {
        "after": after,
        "pages": sorted(set(pages.values())),
        "rewritten": rewritten,
        "stragglers": stragglers,
        "dropped": dropped,
        "gained": gained,
        "atlas_dropped": sorted(expected & set(dropped)),
        "atlas_kept": sorted(
            package for package in expected if package in after.cooked),
        "collateral": sorted(set(dropped) - expected),
    }


def cook_rule_problems(audit: CookAudit) -> list[tuple[PrimaryAssetRule, str]]:
    """Every reason a parsed Asset Manager rule would not do what it says."""
    return [
        (rule, problem)
        for rule in audit.primary_asset_rules
        for problem in rule.problems
    ]


def rule_coverage(audit: CookAudit) -> list[dict]:
    """For each per-asset cook rule, what it actually holds in the build.

    An entry is *load-bearing* when the rule is the only thing cooking it:
    code loads it by path, and no other cooked package references it. That is
    the case the rule exists for, and the one worth proving one asset at a
    time.
    """
    coverage = []
    for rule in audit.primary_asset_rules:
        if not rule.packages:
            continue
        entries = []
        for package in rule.packages:
            others = sorted(
                other for other in audit.cooked
                if other != package
                and package in audit.references.get(other, ())
            )
            entries.append({
                "package": package,
                "cooked": package in audit.cooked,
                "root": package in audit.roots,
                "code_sites": audit.code_paths.get(package, []),
                "other_referrers": others,
                "load_bearing": bool(audit.code_paths.get(package))
                and not others,
            })
        coverage.append({
            "type": rule.name,
            "line": rule.line,
            "cook_rule": rule.cook_rule,
            "base_class": rule.base_class,
            "entries": entries,
        })
    return coverage


def gate(
    audit: CookAudit,
    require_atlas_dropped: bool = False,
    entries=PRINT_ATLAS_ENTRIES,
) -> tuple[int, str]:
    """(exit code, closing line) for --check. Separated so it can be tested."""
    # A rule Unreal would not parse, or would parse into something other than
    # what it says, is read off the ini and the tree alone.
    rule_problems = cook_rule_problems(audit)
    if rule_problems:
        return 1, (
            f"FAIL {len(rule_problems)} problem(s) in the Asset Manager cook "
            "rules; they would hold less than they claim"
        )

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

    stale = atlas_stale_samples(audit, entries)
    if stale:
        return 1, (
            f"FAIL {len(stale)} material(s) reference both an atlas page and "
            "a texture it replaced; the page is paid for and nothing saved"
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

    for rule, problem in cook_rule_problems(audit):
        print(f"  [COOK_RULE] {audit.engine_config}:{rule.line} "
              f"{rule.name}: {problem}")

    for rule in rule_coverage(audit):
        held = [e for e in rule["entries"] if e["load_bearing"]]
        redundant = [e for e in rule["entries"] if e["other_referrers"]]
        unused = [
            e for e in rule["entries"]
            if not e["code_sites"] and not e["other_referrers"]
        ]
        print(
            f"  rule {rule['type']} ({rule['cook_rule']}, "
            f"{rule['base_class'].rsplit('.', 1)[-1]}) holds "
            f"{len(rule['entries'])} asset(s): {len(held)} load-bearing, "
            f"{len(redundant)} also referenced elsewhere, {len(unused)} "
            "referenced by nothing"
        )
        for entry in rule["entries"]:
            if not entry["cooked"]:
                print(f"      NOT COOKED {entry['package']}")

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


def command_explain_rules(audit: CookAudit) -> int:
    """Walk every per-asset cook rule entry and say what holds it, and why.

    The counterfactual is the point: for each asset, re-run reachability with
    the rules removed and report whether it survives. An entry that survives
    either way is redundant; one that does not is the rule earning its line.
    """
    without = run_audit(audit.project_root)
    without.always_cook_assets = set()
    without.roots = set()
    without.cooked = set()
    _seed_roots(without)
    _close(without)

    problems = cook_rule_problems(audit)
    print(f"COOK RULES  {audit.engine_config}")
    for rule in audit.primary_asset_rules:
        print(
            f"\n  line {rule.line}  {rule.name}  CookRule={rule.cook_rule}  "
            f"AssetBaseClass={rule.base_class}"
        )
        print(f"    parsed fields: {', '.join(sorted(rule.fields))}")
        if rule.directories:
            print(f"    directories:   {', '.join(rule.directories)}")
        for problem in rule.problems:
            print(f"    PROBLEM  {problem}")
        if not rule.packages:
            continue
        print(f"    {len(rule.packages)} named asset(s):")
        for package in rule.packages:
            sites = audit.code_paths.get(package, [])
            held = package in audit.cooked
            survives = package in without.cooked
            if held and not survives:
                verdict = "HELD BY THIS RULE"
            elif held and survives:
                verdict = "cooked anyway (rule redundant here)"
            else:
                verdict = "NOT COOKED"
            loaded = (
                "loaded by " + ", ".join(sites) if sites
                else "not loaded by any code path"
            )
            print(f"      {verdict:34s} {package.rsplit('/', 1)[-1]}")
            print(f"        {loaded}")

    held = sum(
        1 for rule in audit.primary_asset_rules for package in rule.packages
        if package in audit.cooked and package not in without.cooked
    )
    named = sum(len(rule.packages) for rule in audit.primary_asset_rules)
    print(
        f"\n{held}/{named} named asset(s) are in the cook only because of "
        f"these rules; {len(problems)} problem(s) found"
    )
    return 1 if problems else 0


def hud_texture_rule(assets: str, **overrides: str) -> str:
    """A well-formed per-asset AlwaysCook rule, with fields swappable.

    The overrides are what the negative half of the self-test uses: each one
    breaks the rule in a way Unreal reacts to by holding less than the line
    says, without changing anything a regex would notice.
    """
    fields = {
        "PrimaryAssetType": '"T"',
        "AssetBaseClass": '"/Script/Engine.Texture2D"',
        "bHasBlueprintClasses": "False",
        "bIsEditorOnly": "False",
        "Directories": "",
        "SpecificAssets": f"({assets})" if assets else "",
        "Rules": "(Priority=0,ChunkId=-1,bApplyRecursively=False,"
                 "CookRule=AlwaysCook)",
    }
    for key, value in overrides.items():
        fields[key] = value
    body = ",".join(f"{key}={value}" for key, value in fields.items())
    return f"+{PRIMARY_ASSET_SETTING}=({body})"


def command_simulate_rebuild(audit: CookAudit) -> int:
    """Report the whole delta of the material rebuild, and fault the surprises.

    Three ways this can be wrong, all of them checked here rather than
    asserted: something other than a print material references a contracted
    texture, so the rebuild leaves it cooked; a rule or an always-cook
    directory covers one, so it ships regardless; or a texture with a
    `T_Photo_` twin is not actually the one the material samples.
    """
    blind = blind_spots(audit)
    if blind:
        print(
            f"SKIP {len(blind)} cooked package(s) not fetched from LFS. A "
            "texture whose material could not be opened looks unreferenced, "
            "so every entry would report as dropping. Run git lfs pull."
        )
        return 0

    result = simulate_atlas_rebuild(audit)
    covered = atlas_rule_cover(audit)
    shadows = atlas_photo_shadows(audit)
    stale = atlas_stale_samples(audit)
    findings = 0

    print(f"ATLAS REBUILD SIMULATION  {len(PRINT_ATLAS_ENTRIES)} contracted "
          f"textures onto {len(result['pages'])} page(s)")
    if not result["pages"]:
        # The atlas is optional by design: no manifest means the materials
        # keep their own textures and the build still ships.
        print("  SKIP no packed atlas; run Scripts/build_texture_atlas.py")
        return 0

    for stem in PRINT_ATLAS_ENTRIES:
        package = f"{ATLAS_TEXTURE_ROOT}/{stem}"
        referrers = sorted(
            other for other in audit.cooked
            if package in audit.references.get(other, ())
        )
        stragglers = result["stragglers"].get(stem, [])
        note = ""
        if package in result["atlas_dropped"]:
            verdict = "drops out"
        elif package not in audit.cooked:
            verdict = "already out"
        else:
            verdict = "STAYS IN"
            findings += 1
        if stem in covered:
            note = "; " + "; ".join(covered[stem])
            findings += 1
        if stem in shadows:
            note += f"; shadowed by {shadows[stem].rsplit('/', 1)[-1]}"
            findings += 1
        if stragglers:
            note += "; not a print material: " + ", ".join(
                s.rsplit("/", 1)[-1] for s in stragglers)
            findings += 1
        print(
            f"  {verdict:12s} {stem:32s} "
            f"{len(referrers)} referrer(s){note}"
        )

    print(
        f"\n  {len(result['atlas_dropped'])}/{len(PRINT_ATLAS_ENTRIES)} leave "
        f"the cook, {len(result['atlas_kept'])} stay"
    )
    print(f"  pages enter the cook: {len(result['gained'])} package(s)")
    for package in result["gained"]:
        print(f"      + {package}")
    if result["collateral"]:
        findings += len(result["collateral"])
        print(f"\n  {len(result['collateral'])} package(s) leave that are not "
              "contracted textures -- the rebuild would break these:")
        for package in result["collateral"]:
            print(f"      - {package}")
    else:
        print("  nothing else leaves the cook")

    if stale:
        findings += len(stale)
        print(f"\n  {len(stale)} material(s) already name a page and a texture "
              "it replaced; the pre-atlas sampler survived:")
        for package, left_over in stale.items():
            print(f"      {package.rsplit('/', 1)[-1]} -> "
                  + ", ".join(s.rsplit("/", 1)[-1] for s in left_over))

    if findings:
        print(f"\nFAIL {findings} finding(s): the rebuild would not do what "
              "the atlas contract says")
        return 1
    print("\nPASS every contracted texture leaves the cook on the rebuild, "
          "nothing else does, and no rule or directory holds one back")
    return 0


def _fixture(root: str, rule: str | None, atlas_material_reads: str) -> str:
    """A small project: a map, a mesh, a print material and three textures.

    ``rule`` is the Asset Manager line to write, or None for a config with no
    per-asset rule at all. ``atlas_material_reads`` is the package the print
    material samples, which is what changes when the editor rebuilds it to
    read an atlas page.
    """
    os.makedirs(os.path.join(root, "Config"), exist_ok=True)
    with open(os.path.join(root, "Config", "DefaultGame.ini"), "w") as handle:
        handle.write(
            "[/Script/UnrealEd.ProjectPackagingSettings]\n"
            "+DirectoriesToAlwaysCook=(Path=\"/Game/Prototype/Materials\")\n"
        )
    with open(os.path.join(root, "Config", "DefaultEngine.ini"), "w") as handle:
        handle.write("[/Script/Engine.AssetManagerSettings]\n")
        if rule:
            handle.write(rule + "\n")

    def package(relative: str, body: str) -> None:
        path = os.path.join(root, "Content", *relative.split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as handle:
            handle.write(body.encode("ascii"))

    # The map is the only root that is not named by a directory rule, so it
    # carries the one reference that proves maps are seeded.
    package("Maps/Level.umap", "/Game/Meshes/SM_Placed")
    package("Meshes/SM_Placed.uasset", "StaticMesh")
    package("Prototype/Materials/M_Print.uasset", atlas_material_reads)
    # Real texture packages carry their class name in the export table, which
    # is what lets the rule's AssetBaseClass be checked against them.
    package("Prototype/Textures/T_Print_D.uasset", "Texture2D")
    package("Prototype/Textures/T_PrintAtlas0_D.uasset", "Texture2D")
    package("Prototype/Textures/T_HudFrame_D.uasset", "Texture2D")

    # A one-entry manifest, so the rebuild simulation has a page to move the
    # contracted texture onto. Geometry only; membership is the packer's check.
    atlas = os.path.join(root, "Content", "SourceArt", "Atlas")
    os.makedirs(atlas, exist_ok=True)
    with open(os.path.join(atlas, "print_atlas.json"), "w") as handle:
        json.dump({
            "version": texture_atlas_contract.MANIFEST_VERSION,
            "page_size": texture_atlas_contract.PAGE_SIZE,
            "gutter": texture_atlas_contract.GUTTER,
            "pages": [{"index": 0, "width": 256, "height": 256}],
            "entries": {
                "T_Print_D": {
                    "page": 0, "x": 0, "y": 0, "w": 128, "h": 128,
                    "uv_scale": [0.5, 0.5], "uv_bias": [0.0, 0.0],
                },
            },
        }, handle)

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
    good_rule = hud_texture_rule(f'"{hud}.T_HudFrame_D"')
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
            os.path.join(tmp, "after"), good_rule,
            "/Game/Prototype/Textures/T_PrintAtlas0_D")
        audit = run_audit(after)
        assert not cook_rule_problems(audit), \
            f"a well-formed rule was faulted: {cook_rule_problems(audit)}"
        assert not code_only_assets(audit), \
            "an asset named by SpecificAssets is still reported unreachable"
        assert not atlas_status(audit, entries)["still_cooked"], \
            "a texture nothing references any more still looks cooked"
        assert hud in audit.cooked, "the SpecificAssets rule was not parsed"
        # The rule is load-bearing for that texture, not decoration: with the
        # per-asset rules taken away it drops straight out of the cook.
        holding = [
            entry for rule in rule_coverage(audit)
            for entry in rule["entries"] if entry["load_bearing"]
        ]
        assert [entry["package"] for entry in holding] == [hud], \
            "the rule was not reported as the only thing cooking the texture"

        # 2b. Every way a rule can look right and hold nothing. Each of these
        #     passes a regex looking for `CookRule=AlwaysCook` and a quoted
        #     /Game path, which is why the rule is parsed instead.
        tampered = {
            "unparseable": good_rule[:-1],
            "unknown field": hud_texture_rule(
                f'"{hud}.T_HudFrame_D"').replace(
                    "SpecificAssets=", "SpecficAssets="),
            "misspelled cook rule": hud_texture_rule(
                f'"{hud}.T_HudFrame_D"',
                Rules="(Priority=0,ChunkId=-1,CookRule=AlwaysCok)"),
            "object name mismatch": hud_texture_rule(f'"{hud}.T_HudFrame"'),
            "asset not in the tree": hud_texture_rule(
                '"/Game/Prototype/Textures/T_Absent_D.T_Absent_D"'),
            "wrong AssetBaseClass": hud_texture_rule(
                f'"{hud}.T_HudFrame_D"',
                AssetBaseClass='"/Script/Engine.StaticMesh"'),
            "holds nothing": hud_texture_rule(""),
            "non-integer priority": hud_texture_rule(
                f'"{hud}.T_HudFrame_D"',
                Rules="(Priority=high,ChunkId=-1,CookRule=AlwaysCook)"),
            "directories not structs": hud_texture_rule(
                f'"{hud}.T_HudFrame_D"',
                Directories='("/Game/UI")'),
        }
        for name, rule_text in tampered.items():
            root = _fixture(
                os.path.join(tmp, "bad_" + name.replace(" ", "_")), rule_text,
                "/Game/Prototype/Textures/T_PrintAtlas0_D")
            audit = run_audit(root)
            assert cook_rule_problems(audit), \
                f"a rule broken by {name} was accepted"
            code, message = gate(audit, entries=entries)
            assert code == 1 and message.startswith("FAIL"), \
                f"a rule broken by {name} did not fail the gate"

        # 2c. The rebuild simulation, forwards. Before the editor runs, the
        #     material still samples its own texture, and moving that one edge
        #     to the page takes the texture out of the cook and nothing else.
        before_root = os.path.join(tmp, "before")
        audit = run_audit(before_root)
        result = simulate_atlas_rebuild(audit, entries)
        print_texture = f"{ATLAS_TEXTURE_ROOT}/T_Print_D"
        page = f"{ATLAS_TEXTURE_ROOT}/T_PrintAtlas0_D"
        assert result["pages"] == [page], \
            f"the manifest's page was not resolved: {result['pages']}"
        assert result["atlas_dropped"] == [print_texture], \
            f"the contracted texture did not leave the cook: {result}"
        assert not result["atlas_kept"] and not result["collateral"], \
            f"the rebuild moved more than the one edge: {result}"
        assert result["gained"] == [page], \
            f"the page did not enter the cook: {result['gained']}"

        # 2d. Every way the rebuild can fail to save anything, or break
        #     something. Each is a real shape: a mesh slot naming the texture
        #     directly, a cook rule covering it, a photo twin the material
        #     actually samples, the pre-atlas sampler surviving an in-place
        #     update, and a package reachable only through the texture.
        def tamper(name, files=None, engine=None):
            root = _fixture(
                os.path.join(tmp, "sim_" + name), engine or good_rule,
                f"{ATLAS_TEXTURE_ROOT}/T_Print_D")
            for relative, body in (files or {}).items():
                path = os.path.join(root, "Content", *relative.split("/"))
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "wb") as handle:
                    handle.write(body.encode("ascii"))
            return run_audit(root)

        audit = tamper("mesh_referrer", {
            "Meshes/SM_Placed.uasset": f"StaticMesh {print_texture}"})
        result = simulate_atlas_rebuild(audit, entries)
        assert result["stragglers"].get("T_Print_D") == ["/Game/Meshes/SM_Placed"], \
            "a referrer the rebuild does not touch was not reported"
        assert result["atlas_kept"] == [print_texture], \
            "a texture a mesh also names was reported as leaving the cook"

        audit = tamper("rule_cover", engine=hud_texture_rule(
            f'"{hud}.T_HudFrame_D","{print_texture}.T_Print_D"'))
        assert atlas_rule_cover(audit, entries), \
            "a cook rule covering a contracted texture was not reported"

        audit = tamper("photo_twin", {
            f"Prototype/Textures/T_Photo_Print_D.uasset": "Texture2D"})
        assert atlas_photo_shadows(audit, entries) == {
            "T_Print_D": f"{ATLAS_TEXTURE_ROOT}/T_Photo_Print_D"}, \
            "a photo twin the material would sample instead was not reported"

        audit = tamper("stale_sample", {
            "Prototype/Materials/M_Print.uasset": f"{page} {print_texture}"})
        assert atlas_stale_samples(audit, entries), \
            "a material naming both a page and its old texture was accepted"
        code, message = gate(audit, entries=entries)
        assert code == 1 and message.startswith("FAIL"), \
            "a surviving pre-atlas sampler did not fail the gate"

        audit = tamper("collateral", {
            "Prototype/Textures/T_Print_D.uasset":
                "Texture2D /Game/Meshes/SM_Orphan",
            "Meshes/SM_Orphan.uasset": "StaticMesh"})
        result = simulate_atlas_rebuild(audit, entries)
        assert result["collateral"] == ["/Game/Meshes/SM_Orphan"], \
            f"a package the rebuild orphans was not reported: {result}"

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
            os.path.join(tmp, "blinded"), good_rule,
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
        f"PASS cook reference self-test: code-only detection, SpecificAssets "
        f"coverage and load-bearing, atlas drop before and after the material "
        f"rebuild, 5 ways the rebuild can save nothing or break something, "
        f"the code-load conflict, the unfetched-LFS hole, and "
        f"{len(tampered)} ways a cook rule can look right and hold nothing"
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
    parser.add_argument(
        "--explain-rules", action="store_true",
        help="walk the Asset Manager cook rules asset by asset and report, "
             "for each, whether the rule is what keeps it in the cook")
    parser.add_argument(
        "--simulate-rebuild", action="store_true",
        help="re-run reachability as if the editor had rebuilt the print "
             "materials, and report the whole delta of that rebuild")
    arguments = parser.parse_args(argv)

    if arguments.self_test:
        return command_self_test()

    audit = run_audit()
    if arguments.explain_rules:
        return command_explain_rules(audit)
    if arguments.simulate_rebuild:
        return command_simulate_rebuild(audit)
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
            "cook_rules": rule_coverage(audit),
            "cook_rule_problems": [
                {"line": rule.line, "type": rule.name, "problem": problem}
                for rule, problem in cook_rule_problems(audit)
            ],
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
