"""Read create_textured_materials.py's build passes out of the source.

The material builder has one full pass and a dozen targeted ones, each behind
an ``IG_*_ONLY`` environment variable, and the targeted ones differ in a way
that matters to the atlas: some recreate the material, some update it in place.

An in-place update appends the new graph and reconnects the outputs without
deleting the old expressions, because deleting from a material the prologue
CDO is holding can take the editor down before the package saves. A
disconnected sampler costs nothing in the compiled shader -- but it still
holds, and still cooks, the texture the atlas page replaced.

So "does the atlas save anything" has a different answer per pass, and the
answer has to come from the source rather than from a list somebody keeps in
their head. This parses it: which passes exist, which builder each calls,
whether it updates in place, and which materials it touches.

Parsed with ``ast`` rather than matched with a regex, for the same reason the
cook rules are: the shapes here are dict comprehensions over tuples declared
a few lines up, and a pattern that reads them by eye goes stale the first time
somebody adds a mode.

Dependency-free, and it does not import the module it reads -- that one needs
``unreal``.
"""

from __future__ import annotations

import ast
import os


PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILDER_SOURCE = os.path.join(
    PROJECT_ROOT, "Scripts", "create_textured_materials.py")

# The builder that consults the atlas. The other two never do: the surface
# table samples per-axis world UVs and the masked builder has no atlas path.
ATLAS_AWARE_BUILDER = "create_flat_texture_materials"
BUILDERS = frozenset({
    ATLAS_AWARE_BUILDER,
    "create_textured_materials",
    "create_masked_texture_materials",
})

# Material tables, by the name the source gives them.
SPEC_TABLES = (
    "DECAL_MATERIALS",
    "SIGN_MATERIALS",
    "TEXTURED_MATERIALS",
    "EVIDENCE_MASK_MATERIALS",
    "SURFACE_OVERLAY_MATERIALS",
)


class PassParseError(RuntimeError):
    """The builder's shape changed in a way this cannot read."""


class MaterialPass:
    """One `IG_*_ONLY` build pass, or the full pass when ``env`` is None."""

    def __init__(self, env, builder, update_in_place, table, materials, line):
        self.env = env
        self.builder = builder
        self.update_in_place = update_in_place
        self.table = table
        self.materials = materials
        self.line = line

    @property
    def name(self) -> str:
        return self.env or "(full pass)"

    @property
    def atlas_aware(self) -> bool:
        return self.builder == ATLAS_AWARE_BUILDER

    def __repr__(self) -> str:
        return (f"MaterialPass({self.name}, {self.builder}, "
                f"in_place={self.update_in_place}, "
                f"{len(self.materials)} material(s))")


def _read_source(path: str = BUILDER_SOURCE) -> ast.Module:
    # The builder carries a UTF-8 BOM; ast.parse rejects one as a
    # non-printable character, so it has to be stripped on the way in.
    with open(path, "r", encoding="utf-8-sig") as handle:
        return ast.parse(handle.read())


def spec_tables(tree: ast.Module) -> dict[str, dict[str, str]]:
    """{table: {material: tex_asset}} for every module-level spec table.

    Only ``tex_asset`` is read. It is the one field the atlas cares about,
    and it is the field whose value ends up as a texture reference in the
    finished material.
    """
    tables: dict[str, dict[str, str]] = {}
    for node in tree.body:
        if not isinstance(node, ast.Assign) or len(node.targets) != 1:
            continue
        target = node.targets[0]
        if not isinstance(target, ast.Name) or target.id not in SPEC_TABLES:
            continue
        if not isinstance(node.value, ast.Dict):
            continue
        entries: dict[str, str] = {}
        for key, value in zip(node.value.keys, node.value.values):
            if not isinstance(key, ast.Constant):
                continue
            stem = ""
            if isinstance(value, ast.Dict):
                for inner_key, inner_value in zip(value.keys, value.values):
                    if (isinstance(inner_key, ast.Constant)
                            and inner_key.value == "tex_asset"
                            and isinstance(inner_value, ast.Constant)):
                        stem = inner_value.value
            entries[key.value] = stem
        tables[target.id] = entries
    return tables


def _names_in_scope(body: list[ast.stmt]) -> dict[str, tuple]:
    """Local tuple/list literals assigned in this block, by name."""
    found: dict[str, tuple] = {}
    for statement in body:
        if not isinstance(statement, ast.Assign) or len(statement.targets) != 1:
            continue
        target = statement.targets[0]
        if not isinstance(target, ast.Name):
            continue
        if isinstance(statement.value, (ast.Tuple, ast.List)):
            found[target.id] = tuple(
                element.value for element in statement.value.elts
                if isinstance(element, ast.Constant)
            )
    return found


def _table_of(node: ast.expr) -> str:
    """The spec table a subscript or name refers to."""
    if isinstance(node, ast.Name) and node.id in SPEC_TABLES:
        return node.id
    if isinstance(node, ast.Subscript):
        return _table_of(node.value)
    return ""


def _materials_of(spec: ast.expr, scope: dict[str, tuple],
                  tables: dict[str, dict[str, str]]) -> tuple[str, list[str]]:
    """(table, material names) for a builder's spec argument.

    Three shapes appear: the whole table by name, a dict literal naming
    materials one at a time, and a comprehension over a tuple of names --
    inline or declared a few lines above.
    """
    if isinstance(spec, ast.Name) and spec.id in SPEC_TABLES:
        return spec.id, sorted(tables.get(spec.id, {}))

    if isinstance(spec, ast.Dict):
        table = ""
        names = []
        for key, value in zip(spec.keys, spec.values):
            if isinstance(key, ast.Constant):
                names.append(key.value)
            table = table or _table_of(value)
        return table, names

    if isinstance(spec, ast.DictComp):
        table = _table_of(spec.value)
        if len(spec.generators) != 1:
            raise PassParseError("a spec comprehension has several generators")
        iterated = spec.generators[0].iter
        if isinstance(iterated, (ast.Tuple, ast.List)):
            return table, [
                element.value for element in iterated.elts
                if isinstance(element, ast.Constant)
            ]
        if isinstance(iterated, ast.Name):
            if iterated.id not in scope:
                raise PassParseError(
                    f"a spec comprehension iterates {iterated.id}, which is "
                    "not a tuple literal in the same block"
                )
            return table, list(scope[iterated.id])
        raise PassParseError(
            f"cannot read the spec comprehension over {ast.unparse(iterated)}")

    raise PassParseError(f"cannot read the spec {ast.unparse(spec)}")


def _env_of(test: ast.expr) -> str:
    """`IG_X_ONLY` out of `os.environ.get("IG_X_ONLY") == "1"`."""
    if not isinstance(test, ast.Compare) or not isinstance(test.left, ast.Call):
        return ""
    call = test.left
    if not isinstance(call.func, ast.Attribute) or call.func.attr != "get":
        return ""
    if not isinstance(call.func.value, ast.Attribute):
        return ""
    if call.func.value.attr != "environ":
        return ""
    if call.args and isinstance(call.args[0], ast.Constant):
        return str(call.args[0].value)
    return ""


def _builder_calls(body: list[ast.stmt]):
    module = ast.Module(body=body, type_ignores=[])
    for node in ast.walk(module):
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                and node.func.id in BUILDERS):
            yield node


def _in_place(call: ast.Call) -> bool:
    for keyword in call.keywords:
        if keyword.arg == "update_in_place":
            return bool(isinstance(keyword.value, ast.Constant)
                        and keyword.value.value)
    return False


def material_passes(path: str = BUILDER_SOURCE) -> list[MaterialPass]:
    """Every build pass the builder offers, targeted and full."""
    tree = _read_source(path)
    tables = spec_tables(tree)
    run = None
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "run":
            run = node
    if run is None:
        raise PassParseError("create_textured_materials.run is gone")

    passes: list[MaterialPass] = []
    gated: set[int] = set()
    for statement in ast.walk(run):
        if not isinstance(statement, ast.If):
            continue
        env = _env_of(statement.test)
        if not env:
            continue
        scope = _names_in_scope(statement.body)
        for call in _builder_calls(statement.body):
            gated.add(id(call))
            if len(call.args) < 3:
                # create_textured_materials(assets, tools) -- the whole table.
                table, materials = "TEXTURED_MATERIALS", sorted(
                    tables.get("TEXTURED_MATERIALS", {}))
            else:
                table, materials = _materials_of(call.args[2], scope, tables)
            passes.append(MaterialPass(
                env, call.func.id, _in_place(call), table, materials,
                call.lineno))

    # Whatever is left is the full pass: the tail of run() that executes when
    # no IG_*_ONLY is set.
    for call in _builder_calls(run.body):
        if id(call) in gated:
            continue
        if len(call.args) < 3:
            table, materials = "TEXTURED_MATERIALS", sorted(
                tables.get("TEXTURED_MATERIALS", {}))
        else:
            table, materials = _materials_of(call.args[2], {}, tables)
        passes.append(MaterialPass(
            None, call.func.id, _in_place(call), table, materials,
            call.lineno))
    return passes


def pass_textures(pass_: MaterialPass,
                  path: str = BUILDER_SOURCE) -> dict[str, str]:
    """{material: tex_asset} for one pass, dropping specs with no texture."""
    tables = spec_tables(_read_source(path))
    table = tables.get(pass_.table, {})
    return {
        material: table[material]
        for material in pass_.materials
        if table.get(material)
    }
