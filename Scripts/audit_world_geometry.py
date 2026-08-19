#!/usr/bin/env python3
"""Static physical-plausibility audit of the code-authored world geometry.

The building, the alley, the store and the CH03 rooftop are not placed in a
level editor — they are assembled from literal centimetre coordinates in
``IGPrologueWorldScene.cpp`` and ``IGThirdMorningDirector.cpp``. That is fast
to author and impossible to eyeball: a shelf can end up three centimetres
inside a wall, a bottle can rest four centimetres above the counter it is
supposed to be standing on, and nothing in the compiler or the engine will
complain. The player only sees "that is not how objects work".

This script re-evaluates those coordinates without Unreal, reconstructs every
axis-aligned box the builders create, and reports the three states that cannot
happen in a real room:

* ``FLOATING``  - a prop with clear air underneath it and nothing holding it up
* ``SUNK``      - a prop whose base is below the floor it stands on
* ``EMBEDDED``  - a prop driven into a wall, slab or another prop

Run it from anywhere:

    python3 Scripts/audit_world_geometry.py            # human-readable report
    python3 Scripts/audit_world_geometry.py --json     # machine-readable
    python3 Scripts/audit_world_geometry.py --check    # exit 1 on any finding

``--check`` is the release gate: the audited world must stay clean, so a new
placement that breaks physics fails here instead of in a playtest.
"""

from __future__ import annotations

import argparse
import ast
import json
import math
import os
import re
import sys
from dataclasses import dataclass, field
from typing import Iterable


PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Builders that author world geometry from literal centimetres.
SOURCES = (
    os.path.join("Source", "IndieGame", "Core", "IGPrologueWorldScene.cpp"),
    os.path.join("Source", "IndieGame", "Sequence", "IGThirdMorningDirector.cpp"),
)

# Scene roots the builders switch between. Every offset is authored in the
# same file the geometry is; keep this table in step with those assignments.
FRAME_OFFSETS = {
    "nullptr": (0.0, 0.0, 0.0),
    "SceneRoot": (0.0, 0.0, 0.0),
    "UpperFloorRoot": (0.0, 0.0, 900.0),
}

# Tolerances, in centimetres. These are deliberately generous: the point is to
# catch what a player reads as wrong, not to enforce micrometre contact.
FLOAT_GAP_TOLERANCE = 1.0     # air under a prop before it reads as hovering
SINK_TOLERANCE = 6.0          # base below its floor before it reads as buried
SINK_HEIGHT_SHARE = 0.25      # ...or a quarter of the object's own height
EMBED_TOLERANCE = 3.0         # penetration into another solid
CONTAINMENT_LIMIT = 0.70      # share of a prop's volume swallowed by structure
FOOTPRINT_EPSILON = 0.5       # XY overlap needed to count as support

# Below this, a box is dressing (a poster, a seam, a price rail, a decal
# plane): it is meant to sit flush against a surface and cannot "float".
MIN_SOLID_THICKNESS = 2.0

# A slab this large in its two major axes is building structure, not a prop.
STRUCTURE_MIN_SPAN = 150.0
STRUCTURE_MAX_THICKNESS = 45.0
# Nothing a person puts in a Korean walk-up is two and a half metres across.
# At that size the box is a wall, a slab, a facade or a shutter.
STRUCTURE_ABSOLUTE_SPAN = 250.0

# The engine's unit shapes are 100 cm across, so a block built on one has a
# real size. Anything else is an authored asset whose "size" argument is a
# percentage scale of its own bounds, and tells us nothing about its volume.
ENGINE_UNIT_MESHES = frozenset(
    {"nullptr", "CubeMesh", "PlaneMesh", "CylinderMesh", "SphereMesh", "ConeMesh"}
)

# CreateBlock's tail differs between the two builders.
#   AIGPrologueWorldScene: center, size, material, collision, mesh, rotation, parent
#   AIGThirdMorningDirector: center, size, material, collision, rotation, mesh, movable
CREATE_BLOCK_LAYOUT = {
    "IGPrologueWorldScene.cpp": {"mesh": 4, "rotation": 5, "movable": None},
    "IGThirdMorningDirector.cpp": {"mesh": 5, "rotation": 4, "movable": 6},
}

STRUCTURE_MATERIAL_HINTS = (
    "wall", "floor", "ceil", "slab", "jangpan", "concrete", "asphalt",
    "stucco", "brick", "tile", "waterproof", "roof", "plaster", "gypsum",
)


# ---------------------------------------------------------------------------
# A tiny vector type so authored expressions evaluate the way C++ reads them
# ---------------------------------------------------------------------------

class Vec3:
    __slots__ = ("x", "y", "z")

    def __init__(self, x, y=None, z=None):
        if y is None and z is None:
            y = z = x
        self.x, self.y, self.z = float(x), float(y), float(z)

    # FVector member access shows up as Foo.X / Foo.Y / Foo.Z in the sources.
    @property
    def X(self):  # noqa: N802 - mirrors the engine field name
        return self.x

    @property
    def Y(self):  # noqa: N802
        return self.y

    @property
    def Z(self):  # noqa: N802
        return self.z

    def _pair(self, other):
        return other if isinstance(other, Vec3) else Vec3(other)

    def __add__(self, other):
        o = self._pair(other)
        return Vec3(self.x + o.x, self.y + o.y, self.z + o.z)

    __radd__ = __add__

    def __sub__(self, other):
        o = self._pair(other)
        return Vec3(self.x - o.x, self.y - o.y, self.z - o.z)

    def __rsub__(self, other):
        return self._pair(other) - self

    def __mul__(self, other):
        o = self._pair(other)
        return Vec3(self.x * o.x, self.y * o.y, self.z * o.z)

    __rmul__ = __mul__

    def __truediv__(self, other):
        o = self._pair(other)
        return Vec3(self.x / o.x, self.y / o.y, self.z / o.z)

    def __neg__(self):
        return Vec3(-self.x, -self.y, -self.z)

    def as_tuple(self):
        return (self.x, self.y, self.z)

    def __repr__(self):
        return f"({self.x:.2f}, {self.y:.2f}, {self.z:.2f})"


class Rot3:
    """FRotator(Pitch, Yaw, Roll) in degrees, matching the C++ argument order."""

    __slots__ = ("pitch", "yaw", "roll")

    def __init__(self, pitch=0.0, yaw=0.0, roll=0.0):
        self.pitch, self.yaw, self.roll = float(pitch), float(yaw), float(roll)

    def is_identity(self):
        return (
            abs(self.pitch) < 1e-6
            and abs(self.yaw) < 1e-6
            and abs(self.roll) < 1e-6
        )

    def as_tuple(self):
        return (self.pitch, self.yaw, self.roll)

    def __repr__(self):
        return f"R(p={self.pitch:g}, y={self.yaw:g}, r={self.roll:g})"


def rotated_extent(size: Vec3, rot: Rot3) -> Vec3:
    """Axis-aligned full size of a box after an FRotator, in world axes."""
    if rot.is_identity():
        return Vec3(size.x, size.y, size.z)

    cp, sp = _cos_sin(rot.pitch)
    cy, sy = _cos_sin(rot.yaw)
    cr, sr = _cos_sin(rot.roll)

    # Unreal's rotation order is Roll (X), then Pitch (Y), then Yaw (Z).
    basis = (
        (cp * cy, sr * sp * cy - cr * sy, cr * sp * cy + sr * sy),
        (cp * sy, sr * sp * sy + cr * cy, cr * sp * sy - sr * cy),
        (-sp, sr * cp, cr * cp),
    )
    half = (size.x / 2.0, size.y / 2.0, size.z / 2.0)
    out = []
    for row in basis:
        out.append(2.0 * sum(abs(row[i]) * half[i] for i in range(3)))
    return Vec3(*out)


def _cos_sin(degrees: float) -> tuple[float, float]:
    radians = math.radians(degrees)
    return math.cos(radians), math.sin(radians)


# ---------------------------------------------------------------------------
# Expression evaluation
# ---------------------------------------------------------------------------

_FLOAT_SUFFIX = re.compile(r"(?<=[0-9.])[fF](?![\w.])")
_HEX_OR_WORD = re.compile(r"[A-Za-z_][A-Za-z0-9_]*(::[A-Za-z_][A-Za-z0-9_]*)+")

_BUILTINS = {
    "V": Vec3,
    "R": Rot3,
    "max": lambda a, b: a if a > b else b,
    "min": lambda a, b: a if a < b else b,
    "abs": abs,
    "sqrt": math.sqrt,
    "cos": lambda d: math.cos(math.radians(d)),
    "sin": lambda d: math.sin(math.radians(d)),
    "floor": math.floor,
    "ceil": math.ceil,
    "clamp": lambda v, lo, hi: lo if v < lo else (hi if v > hi else v),
    "ZERO_ROT": Rot3(),
    "ZERO_VEC": Vec3(0.0),
    "UE_ARRAY_COUNT": len,
}

_QUALIFIED_REPLACEMENTS = {
    "FVector::ZeroVector": "ZERO_VEC",
    "FVector::UpVector": "V(0,0,1)",
    "FVector::OneVector": "V(1,1,1)",
    "FRotator::ZeroRotator": "ZERO_ROT",
    "FMath::Max": "max",
    "FMath::Min": "min",
    "FMath::Abs": "abs",
    "FMath::Sqrt": "sqrt",
    "FMath::Square": "(lambda v: v * v)",
    "FMath::Cos": "cos",
    "FMath::Sin": "sin",
    "FMath::FloorToFloat": "floor",
    "FMath::CeilToFloat": "ceil",
    "FMath::Clamp": "clamp",
    "FMath::RoundToFloat": "round",
    "UE_PI": repr(math.pi),
    "PI": repr(math.pi),
}


class Unresolved(Exception):
    """Raised when an authored expression depends on runtime state."""


_NAMESPACE_QUALIFIER = re.compile(r"\b(?:IGPrologueWorld|IGThirdMorning)::(\w+)")


def to_python(expression: str) -> str:
    text = expression.strip()
    for source, target in _QUALIFIED_REPLACEMENTS.items():
        text = text.replace(source, target)
    # The builders keep their shared dimensions in a file-level namespace; those
    # names are hoisted into the evaluation scope, so the qualifier can go.
    text = _NAMESPACE_QUALIFIER.sub(r"\1", text)
    # Anything still carrying :: is an enum or a static we cannot evaluate.
    if _HEX_OR_WORD.search(text):
        raise Unresolved(f"qualified name in {expression!r}")
    text = _FLOAT_SUFFIX.sub("", text)
    text = re.sub(r"\bFVector\b", "V", text)
    text = re.sub(r"\bFRotator\b", "R", text)
    text = text.replace("TEXT(", "(")
    text = re.sub(r"static_cast<\w+>", "", text)
    return _convert_ternaries(text)


def _convert_ternaries(text: str) -> str:
    """Rewrites C's `cond ? a : b` as Python's `a if cond else b`.

    The builders pick alternating window heights, pier ends and shelf depths
    this way; without it those placements drop out of the audit entirely.
    """
    depth = 0
    for index, char in enumerate(text):
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif char == "?" and depth == 0:
            inner = 0
            for scan in range(index + 1, len(text)):
                current = text[scan]
                if current in "([{":
                    inner += 1
                elif current in ")]}":
                    inner -= 1
                elif current == ":" and inner == 0:
                    condition = text[:index]
                    when_true = text[index + 1:scan]
                    when_false = text[scan + 1:]
                    return (
                        f"(({_convert_ternaries(when_true)})"
                        f" if ({_convert_ternaries(condition)})"
                        f" else ({_convert_ternaries(when_false)}))"
                    )
            break
    return text


def evaluate(expression: str, scope: dict) -> object:
    text = to_python(expression)
    if not text:
        raise Unresolved("empty expression")
    try:
        tree = ast.parse(text, mode="eval")
    except SyntaxError as error:
        raise Unresolved(f"unparsable {expression!r}: {error}") from error

    names = {"__builtins__": {"round": round, "abs": abs}}
    names.update(_BUILTINS)
    names.update(scope)
    try:
        return eval(compile(tree, "<geometry>", "eval"), names)  # noqa: S307
    except (NameError, TypeError, AttributeError, ZeroDivisionError) as error:
        raise Unresolved(f"{expression!r}: {error}") from error


def as_vec(value) -> Vec3:
    if isinstance(value, Vec3):
        return value
    if isinstance(value, (int, float)):
        return Vec3(value)
    raise Unresolved(f"not a vector: {value!r}")


def as_rot(value) -> Rot3:
    if isinstance(value, Rot3):
        return value
    raise Unresolved(f"not a rotator: {value!r}")


# ---------------------------------------------------------------------------
# C++ scanning
# ---------------------------------------------------------------------------

def strip_comments(text: str) -> str:
    """Blanks comments while preserving every byte offset and line number."""
    out = list(text)
    index = 0
    length = len(text)
    state = None  # None | 'line' | 'block' | 'string' | 'char'
    while index < length:
        char = text[index]
        nxt = text[index + 1] if index + 1 < length else ""
        if state is None:
            if char == "/" and nxt == "/":
                state = "line"
                out[index] = out[index + 1] = " "
                index += 2
                continue
            if char == "/" and nxt == "*":
                state = "block"
                out[index] = out[index + 1] = " "
                index += 2
                continue
            if char == '"':
                state = "string"
            elif char == "'":
                state = "char"
        elif state == "line":
            if char == "\n":
                state = None
            else:
                out[index] = " "
        elif state == "block":
            if char == "*" and nxt == "/":
                out[index] = out[index + 1] = " "
                index += 2
                state = None
                continue
            if char != "\n":
                out[index] = " "
        elif state == "string":
            if char == "\\":
                index += 2
                continue
            if char == '"':
                state = None
        elif state == "char":
            if char == "\\":
                index += 2
                continue
            if char == "'":
                state = None
        index += 1
    return "".join(out)


def split_arguments(argument_text: str) -> list[str]:
    """Splits a call's argument list on top-level commas."""
    parts = []
    depth = 0
    current = []
    in_string = False
    index = 0
    while index < len(argument_text):
        char = argument_text[index]
        if in_string:
            current.append(char)
            if char == "\\":
                if index + 1 < len(argument_text):
                    current.append(argument_text[index + 1])
                    index += 2
                    continue
            elif char == '"':
                in_string = False
            index += 1
            continue
        if char == '"':
            in_string = True
            current.append(char)
        elif char in "([{":
            depth += 1
            current.append(char)
        elif char in ")]}":
            depth -= 1
            current.append(char)
        elif char == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(char)
        index += 1
    tail = "".join(current).strip()
    if tail:
        parts.append(tail)
    return parts


def match_call(text: str, open_paren: int) -> tuple[str, int]:
    """Returns the argument text of a call whose '(' is at open_paren."""
    depth = 0
    in_string = False
    index = open_paren
    while index < len(text):
        char = text[index]
        if in_string:
            if char == "\\":
                index += 2
                continue
            if char == '"':
                in_string = False
        elif char == '"':
            in_string = True
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return text[open_paren + 1:index], index
        index += 1
    raise Unresolved("unterminated call")


@dataclass
class Box:
    identifier: str
    source: str
    line: int
    function: str
    frame: str
    kind: str
    center: tuple[float, float, float]
    size: tuple[float, float, float]
    rotation: tuple[float, float, float]
    collision: bool
    material: str
    note: str = ""
    exempt: str = ""

    @property
    def minimum(self):
        return tuple(self.center[i] - self.size[i] / 2.0 for i in range(3))

    @property
    def maximum(self):
        return tuple(self.center[i] + self.size[i] / 2.0 for i in range(3))

    @property
    def thickness(self):
        return min(self.size)

    @property
    def span(self):
        return sorted(self.size, reverse=True)

    def is_dressing(self):
        return self.thickness < MIN_SOLID_THICKNESS

    def is_structure(self):
        if max(self.size) >= STRUCTURE_ABSOLUTE_SPAN:
            return True
        lowered = self.material.lower()
        if any(hint in lowered for hint in STRUCTURE_MATERIAL_HINTS):
            if self.span[0] >= STRUCTURE_MIN_SPAN:
                return True
        return (
            self.span[0] >= STRUCTURE_MIN_SPAN
            and self.span[1] >= STRUCTURE_MIN_SPAN
            and self.thickness <= STRUCTURE_MAX_THICKNESS
        )

    def label(self):
        return f"{os.path.basename(self.source)}:{self.line} {self.function}"


@dataclass
class ScanResult:
    boxes: list[Box] = field(default_factory=list)
    resolved: int = 0
    unresolved: int = 0
    unresolved_examples: list[str] = field(default_factory=list)


FUNCTION_PATTERN = re.compile(
    r"^[A-Za-z_][\w:<>*&\s]*?\b(?P<cls>AIG\w+)::(?P<name>\w+)\s*\(",
    re.MULTILINE,
)

CONST_FLOAT = re.compile(
    r"\b(?:static\s+)?(?:const|constexpr)\s+(?:float|double|int32)\s+"
    r"(?P<name>\w+)\s*=\s*(?P<value>[^;{},]+)\s*[;,]"
)
# `const float A = 1.0f, B = 2.0f;` continues after the first declarator.
CONST_FLOAT_CONTINUED = re.compile(r",\s*(?P<name>\w+)\s*=\s*(?P<value>[^;{},]+)")
# `for (const float LegX : {-164.0f, -36.0f})` — the builders lay out repeated
# legs, treads, rails and shelves this way, and skipping them would hide most
# of the small parts from the audit.
RANGE_FOR = re.compile(
    r"\bfor\s*\(\s*(?:const\s+)?(?:float|double|int32|auto)\s*&?\s*"
    r"(?P<name>\w+)\s*:\s*\{(?P<values>[^{}]*)\}\s*\)"
)
# `for (int32 Index = 0; Index < 14; ++Index)` — treads, balusters, shelves.
COUNT_FOR = re.compile(
    r"\bfor\s*\(\s*(?:const\s+)?(?:int32|int|uint32|size_t)\s+(?P<name>\w+)"
    r"\s*=\s*(?P<init>[^;]+);\s*(?P=name)\s*<\s*(?P<limit>[^;]+);"
    r"\s*(?:\+\+\s*(?P=name)|(?P=name)\s*\+\+)\s*\)"
)
# `auto DressUnitDoor = [captures](const float DoorX, const float FaceY)` —
# the builders factor repeated dressing (doors, meters, shelf bays) this way.
LAMBDA_DEF = re.compile(
    r"\b(?:const\s+)?auto\s*&?\s*(?P<name>\w+)\s*=\s*\[[^\]]*\]\s*"
    r"\((?P<params>[^;]*?)\)\s*(?:mutable\s*)?(?:->[^{;]+)?\s*(?=\{)"
)
MAX_LOOP_ITERATIONS = 64
CONST_VECTOR_ASSIGN = re.compile(
    r"\bconst\s+FVector\s+(?P<name>\w+)\s*=\s*(?P<value>[^;{}]+);"
)
CONST_VECTOR_CTOR = re.compile(
    r"\bconst\s+FVector\s+(?P<name>\w+)\s*\((?P<value>[^;{}]*)\)\s*;"
)
CONST_ROTATOR = re.compile(
    r"\bconst\s+FRotator\s+(?P<name>\w+)\s*(?:=\s*)?\(?(?P<value>[^;{}]*)\)?\s*;"
)
# `const float NorthWindowXs[] = {300, 700, 1200};` indexed by a loop counter.
CONST_ARRAY = re.compile(
    r"\b(?:static\s+)?(?:const|constexpr)\s+(?:float|double|int32|FVector)\s+"
    r"(?P<name>\w+)\s*\[\s*\w*\s*\]\s*=\s*\{(?P<values>[^{}]*(?:\{[^{}]*\}[^{}]*)*)\}\s*;"
)
ACTIVE_PARENT = re.compile(r"\bActiveParent\s*=\s*(?P<value>[\w:]+)\s*;")

PLACEMENT_CALLS = {
    # name: (center index, size index, material index, collision index,
    #        rotation index, kind)
    "CreateBlock": (0, 1, 2, 3, None, "block"),
    "AddStoreStockBlock": (0, 1, 2, None, 4, "stock"),
}


def function_bodies(text: str) -> Iterable[tuple[str, int, str]]:
    """Yields (qualified name, start offset, body text) for each definition."""
    for match in FUNCTION_PATTERN.finditer(text):
        open_paren = text.index("(", match.end() - 1)
        try:
            _, close_paren = match_call(text, open_paren)
        except Unresolved:
            continue
        brace = text.find("{", close_paren)
        if brace < 0:
            continue
        # Skip declarations that are not definitions (';' before '{').
        semicolon = text.find(";", close_paren)
        if 0 <= semicolon < brace:
            continue
        depth = 0
        index = brace
        while index < len(text):
            if text[index] == "{":
                depth += 1
            elif text[index] == "}":
                depth -= 1
                if depth == 0:
                    break
            index += 1
        name = f"{match.group('cls')}::{match.group('name')}"
        yield name, brace, text[brace:index + 1]


def _file_scope_constants(text: str) -> dict:
    """Hoists the builders' file-level namespace dimensions into one scope."""
    scope: dict[str, object] = {}
    namespace = re.search(r"\bnamespace\s+\w+\s*\{", text)
    if not namespace:
        return scope
    start = namespace.end() - 1
    depth = 0
    index = start
    while index < len(text):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                break
        index += 1
    body = text[start:index]
    for match in CONST_FLOAT.finditer(body):
        try:
            scope[match.group("name")] = evaluate(match.group("value"), scope)
        except Unresolved:
            pass
    for pattern in (CONST_VECTOR_ASSIGN, CONST_VECTOR_CTOR):
        for match in pattern.finditer(body):
            value = match.group("value")
            expression = value if value.strip().startswith("FVector") \
                else f"FVector({value})"
            try:
                scope[match.group("name")] = as_vec(evaluate(expression, scope))
            except Unresolved:
                pass
    return scope


def _lambda_body_offset(text: str, offset: int) -> int:
    """LAMBDA_DEF already stops at the body brace; kept for readability."""
    return offset


def _lambda_parameters(parameter_text: str) -> list[str | None]:
    """Names a lambda's parameters, or None where the name cannot be read."""
    names: list[str | None] = []
    for parameter in split_arguments(parameter_text):
        identifiers = re.findall(r"\b[A-Za-z_]\w*\b", parameter)
        names.append(identifiers[-1] if identifiers else None)
    return names


def _block_after(text: str, offset: int) -> tuple[int, int] | None:
    """Returns (start, end) of the brace block that follows offset."""
    index = offset
    while index < len(text) and text[index] in " \t\r\n":
        index += 1
    if index >= len(text) or text[index] != "{":
        return None
    depth = 0
    while index < len(text):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return (offset, index + 1)
        index += 1
    return None


# A placement can declare itself deliberate. Put this on the line above the
# call, with the reason, and the audit will report it as exempt instead of a
# finding -- hidden collision proxies and other invisible-on-purpose blocks.
EXEMPT_MARKER = re.compile(r"physics-audit:\s*intentional(?:\s+(?P<why>.*))?")
EXEMPT_REACH_LINES = 8


class BodyScanner:
    """Walks one function body, resolving declarations in source order."""

    def __init__(self, relative_path, text, line_of, result, file_scope):
        self.relative_path = relative_path
        self.text = text
        self.line_of = line_of
        self.result = result
        self.file_scope = file_scope
        self.frame = "SceneRoot"
        self.function = ""
        self.lambdas: dict[str, tuple[list, tuple[int, int]]] = {}
        self.exempt_lines: dict[int, str] = {}

    def run(self, function_name: str, span: tuple[int, int], frame: str):
        self.function = function_name
        self.frame = frame
        self.lambdas = {}
        self.walk(span[0], span[1], dict(self.file_scope))

    def walk(self, start: int, end: int, scope: dict, depth: int = 0):
        """Processes [start, end) at one nesting level, recursing into loops."""
        if depth > 12:
            return
        text = self.text
        events: list[tuple[int, str, object]] = []
        skip: list[tuple[int, int]] = []

        # Loops repeat their body; recurse into each iteration so repeated
        # legs, treads, rails and shelves all reach the audit. Only the loops
        # at this nesting level belong here — the ones inside them are found
        # again by the recursive call, with the outer value already bound.
        # Lambdas are the same idea: skip the definition, walk it per call.
        loops = []
        for pattern, loop_kind in ((RANGE_FOR, "loop"), (COUNT_FOR, "count")):
            for match in pattern.finditer(text, start, end):
                block = _block_after(text, match.end())
                if block is not None:
                    loops.append((match, block, loop_kind))
        for match in LAMBDA_DEF.finditer(text, start, end):
            block = _block_after(text, _lambda_body_offset(text, match.end()))
            if block is not None:
                loops.append((match, block, "lambda"))

        for match, block, loop_kind in loops:
            if any(other[0] <= match.start() < other[1]
                   for _, other, _ in loops if other is not block):
                continue
            skip.append(block)
            events.append((match.start(), loop_kind, (match, block)))

        def inside_skipped(offset: int) -> bool:
            return any(low <= offset < high for low, high in skip)

        for name, (params, block) in self.lambdas.items():
            for match in re.finditer(rf"\b{name}\s*\(", text[start:end]):
                offset = start + match.start()
                if not inside_skipped(offset):
                    events.append((offset, "invoke", (name, offset)))

        for pattern, kind in (
            (CONST_FLOAT, "float"),
            (CONST_ARRAY, "array"),
            (CONST_VECTOR_ASSIGN, "vector"),
            (CONST_VECTOR_CTOR, "vector"),
            (CONST_ROTATOR, "rotator"),
            (ACTIVE_PARENT, "parent"),
        ):
            for match in pattern.finditer(text, start, end):
                if not inside_skipped(match.start()):
                    events.append((match.start(), kind, match))
        for call_name in PLACEMENT_CALLS:
            for match in re.finditer(rf"\b{call_name}\s*\(", text[start:end]):
                offset = start + match.start()
                if not inside_skipped(offset):
                    events.append((offset, "call", (call_name, offset)))

        events.sort(key=lambda item: item[0])

        for _, kind, payload in events:
            if kind == "loop":
                match, block = payload
                name = match.group("name")
                values = []
                for raw in split_arguments(match.group("values")):
                    try:
                        values.append(evaluate(raw, scope))
                    except Unresolved:
                        pass
                self._iterate(block, name, values, scope, depth)
            elif kind == "count":
                match, block = payload
                name = match.group("name")
                try:
                    first = int(evaluate(match.group("init"), scope))
                    limit = int(evaluate(match.group("limit"), scope))
                except (Unresolved, ValueError):
                    continue
                if limit - first > MAX_LOOP_ITERATIONS:
                    limit = first + MAX_LOOP_ITERATIONS
                self._iterate(
                    block, name, list(range(first, limit)), scope, depth)
            elif kind == "lambda":
                match, block = payload
                self.lambdas[match.group("name")] = (
                    _lambda_parameters(match.group("params")), block)
            elif kind == "invoke":
                self._invoke(payload[0], payload[1], scope, depth)
            elif kind == "float":
                self._declare_float(payload, scope)
            elif kind == "array":
                name = payload.group("name")
                entries = []
                for raw in split_arguments(payload.group("values")):
                    try:
                        entries.append(evaluate(raw, scope))
                    except Unresolved:
                        entries = None
                        break
                if entries is None:
                    scope.pop(name, None)
                else:
                    scope[name] = entries
            elif kind == "vector":
                name = payload.group("name")
                value = payload.group("value")
                expression = value if value.strip().startswith("FVector") \
                    else f"FVector({value})"
                try:
                    scope[name] = as_vec(evaluate(expression, scope))
                except Unresolved:
                    scope.pop(name, None)
            elif kind == "rotator":
                name = payload.group("name")
                value = payload.group("value").strip()
                expression = value if value.startswith("FRotator") \
                    else f"FRotator({value})"
                try:
                    scope[name] = as_rot(evaluate(expression, scope))
                except Unresolved:
                    scope.pop(name, None)
            elif kind == "parent":
                self.frame = payload.group("value").split("::")[-1]
            elif kind == "call":
                self._placement(payload[0], payload[1], scope)

    def _iterate(self, block, name, values, scope: dict, depth: int):
        """Walks a loop body once per value, carrying manual counters along."""
        body = self.text[block[0]:block[1]]
        counters = {
            candidate for candidate in
            set(re.findall(r"\+\+\s*(\w+)\s*;", body))
            | set(re.findall(r"(\w+)\s*\+\+\s*;", body))
            if isinstance(scope.get(candidate), (int, float))
        }
        for value in values:
            nested = dict(scope)
            nested[name] = value
            self.walk(block[0], block[1], nested, depth + 1)
            for counter in counters:
                scope[counter] = scope[counter] + 1

    def _invoke(self, name: str, offset: int, scope: dict, depth: int):
        """Walks a local lambda's body with its arguments bound."""
        parameters, block = self.lambdas[name]
        try:
            argument_text, _ = match_call(self.text, self.text.index("(", offset))
        except (Unresolved, ValueError):
            return
        arguments = split_arguments(argument_text)
        nested = dict(scope)
        for index, parameter in enumerate(parameters):
            if parameter is None or index >= len(arguments):
                continue
            try:
                nested[parameter] = evaluate(arguments[index], scope)
            except Unresolved:
                nested.pop(parameter, None)
        self.walk(block[0], block[1], nested, depth + 1)

    def _declare_float(self, match, scope: dict):
        name = match.group("name")
        try:
            scope[name] = evaluate(match.group("value"), scope)
        except Unresolved:
            scope.pop(name, None)
            return
        # `const float A = 1.0f, B = 2.0f;` keeps declaring after the comma.
        if match.group(0).rstrip().endswith(","):
            tail_end = self.text.find(";", match.end())
            if tail_end < 0:
                return
            tail = self.text[match.end() - 1:tail_end]
            for extra in CONST_FLOAT_CONTINUED.finditer(tail):
                try:
                    scope[extra.group("name")] = evaluate(
                        extra.group("value"), scope)
                except Unresolved:
                    scope.pop(extra.group("name"), None)

    def _placement(self, call_name: str, offset: int, scope: dict):
        text = self.text
        open_paren = text.index("(", offset)
        line = self.line_of(offset)
        try:
            argument_text, _ = match_call(text, open_paren)
        except Unresolved:
            self.result.unresolved += 1
            return
        arguments = split_arguments(argument_text)
        (center_index, size_index, material_index, collision_index,
         rotation_index, kind) = PLACEMENT_CALLS[call_name]
        try:
            if len(arguments) <= max(center_index, size_index):
                raise Unresolved("too few arguments")
            center = as_vec(evaluate(arguments[center_index], scope))
            size = as_vec(evaluate(arguments[size_index], scope))
        except Unresolved as error:
            self.result.unresolved += 1
            if len(self.result.unresolved_examples) < 40:
                self.result.unresolved_examples.append(
                    f"{os.path.basename(self.relative_path)}:{line} {error}")
            return

        material = ""
        if material_index is not None and len(arguments) > material_index:
            material = arguments[material_index].strip()
        collision = True
        if collision_index is not None and len(arguments) > collision_index:
            token = arguments[collision_index].strip()
            if token in ("true", "false"):
                collision = token == "true"

        layout = CREATE_BLOCK_LAYOUT.get(
            os.path.basename(self.relative_path), {})
        rotation = Rot3()
        rotation_argument = None
        if rotation_index is not None and len(arguments) > rotation_index:
            rotation_argument = arguments[rotation_index]
        elif call_name == "CreateBlock" and layout:
            slot = layout["rotation"]
            if len(arguments) > slot:
                rotation_argument = arguments[slot]
        rotation_known = True
        if rotation_argument:
            try:
                rotation = as_rot(evaluate(rotation_argument, scope))
            except Unresolved:
                rotation = Rot3()
                rotation_known = False

        # `SizeCentimeters` is only a real size when the block keeps the
        # engine's 100 cm unit shapes. With an authored mesh override the same
        # argument is a percentage scale of that mesh's own bounds (a label
        # sleeve is scaled 336%), so the box tells us nothing about the volume
        # and must stay out of the geometric checks.
        note = ""
        mesh_token = "nullptr"
        if call_name == "CreateBlock" and layout:
            slot = layout["mesh"]
            if len(arguments) > slot:
                mesh_token = arguments[slot].strip()
            movable_slot = layout["movable"]
            # CH03's builder can request a Movable block. Those exist to be
            # driven somewhere by a beat -- a receipt that flutters down, a
            # water level that rises -- so the authored transform is a starting
            # pose, not a resting place, and nothing has to hold it up.
            if (movable_slot is not None
                    and len(arguments) > movable_slot
                    and arguments[movable_slot].strip() == "true"):
                note = "movable"

        base_mesh = re.sub(r"\s*\?.*$", "", mesh_token).strip()
        base_mesh = base_mesh.replace(".Get()", "")
        if not note:
            if base_mesh == "PlaneMesh":
                note = "plane"
            elif base_mesh not in ENGINE_UNIT_MESHES:
                note = "authored"
            elif abs(size.x - 100.0) < 1e-6 and abs(size.y - 100.0) < 1e-6 \
                    and abs(size.z - 100.0) < 1e-6:
                # `FVector(100.0f)` is the file's idiom for "this asset's own
                # size", so the block is an authored mesh even where the
                # override reached CreateBlock through a local.
                note = "authored"

        if rotation_known:
            world_size = rotated_extent(size, rotation)
        else:
            # A rotation the audit cannot evaluate (an FVector::Rotation() off
            # a runtime arm, say) makes the exact box unknowable. Bound it by
            # the diagonal so the box still counts as something a neighbour can
            # touch, and mark it so it is never itself judged.
            diagonal = math.sqrt(size.x ** 2 + size.y ** 2 + size.z ** 2)
            world_size = Vec3(diagonal)
            note = "unknown-rotation"
        offset_vector = FRAME_OFFSETS.get(self.frame, (0.0, 0.0, 0.0))
        self.result.boxes.append(Box(
            identifier=f"{os.path.basename(self.relative_path)}#{line}",
            source=self.relative_path,
            line=line,
            function=self.function,
            # CH03 is spawned at its own stage origin, so its coordinates never
            # meet the prologue building's. Key the frame by file as well.
            frame=f"{os.path.basename(self.relative_path)}:{self.frame}",
            kind=kind,
            center=(
                center.x + offset_vector[0],
                center.y + offset_vector[1],
                center.z + offset_vector[2],
            ),
            size=world_size.as_tuple(),
            rotation=rotation.as_tuple(),
            collision=collision,
            material=material,
            note=note,
            exempt=self._exemption(line),
        ))
        self.result.resolved += 1

    def _exemption(self, line: int) -> str:
        for candidate in range(line, max(0, line - EXEMPT_REACH_LINES), -1):
            if candidate in self.exempt_lines:
                return self.exempt_lines[candidate]
        return ""


def scan_source(relative_path: str) -> ScanResult:
    absolute = os.path.join(PROJECT_ROOT, relative_path)
    with open(absolute, "r", encoding="utf-8-sig") as handle:
        raw = handle.read()
    text = strip_comments(raw)

    line_starts = [0]
    for index, char in enumerate(text):
        if char == "\n":
            line_starts.append(index + 1)

    def line_of(offset: int) -> int:
        low, high = 0, len(line_starts) - 1
        while low < high:
            mid = (low + high + 1) // 2
            if line_starts[mid] <= offset:
                low = mid
            else:
                high = mid - 1
        return low + 1

    result = ScanResult()
    file_scope = _file_scope_constants(text)
    scanner = BodyScanner(relative_path, text, line_of, result, file_scope)
    exempt_lines = {}
    for number, source_line in enumerate(raw.splitlines(), start=1):
        marker = EXEMPT_MARKER.search(source_line)
        if marker:
            exempt_lines[number] = (marker.group("why") or "").strip()

    for function_name, brace, body in function_bodies(text):
        scanner.exempt_lines = exempt_lines
        scanner.run(function_name, (brace, brace + len(body)), "SceneRoot")

    return result


# ---------------------------------------------------------------------------
# Physical checks
# ---------------------------------------------------------------------------

def overlap_1d(a_min, a_max, b_min, b_max):
    return min(a_max, b_max) - max(a_min, b_min)


def boxes_overlap(a: Box, b: Box, tolerance=0.0):
    a_min, a_max = a.minimum, a.maximum
    b_min, b_max = b.minimum, b.maximum
    depths = [
        overlap_1d(a_min[i], a_max[i], b_min[i], b_max[i]) for i in range(3)
    ]
    if min(depths) <= tolerance:
        return None
    return depths


def footprint_overlap(a: Box, b: Box):
    a_min, a_max = a.minimum, a.maximum
    b_min, b_max = b.minimum, b.maximum
    x = overlap_1d(a_min[0], a_max[0], b_min[0], b_max[0])
    y = overlap_1d(a_min[1], a_max[1], b_min[1], b_max[1])
    if x <= FOOTPRINT_EPSILON or y <= FOOTPRINT_EPSILON:
        return 0.0
    return x * y


@dataclass
class Finding:
    code: str
    severity: str
    box: Box
    detail: str
    other: Box | None = None

    def to_dict(self):
        payload = {
            "code": self.code,
            "severity": self.severity,
            "source": self.box.source,
            "line": self.box.line,
            "function": self.box.function,
            "center": [round(v, 2) for v in self.box.center],
            "size": [round(v, 2) for v in self.box.size],
            "detail": self.detail,
        }
        if self.other is not None:
            payload["other"] = {
                "source": self.other.source,
                "line": self.other.line,
                "function": self.other.function,
            }
        return payload


def audit(boxes: list[Box]) -> list[Finding]:
    findings: list[Finding] = []
    # Authored mesh overrides carry a percentage scale rather than a size, so
    # their boxes are meaningless here. Everything else is real surface: thin
    # dressing cannot itself float, but a switch plate is still what a breaker
    # toggle is screwed to, so it stays in as something to touch.
    surfaces = [b for b in boxes if b.note != "authored"]
    checkable = [
        b for b in surfaces
        if not b.is_dressing()
        and b.note not in ("unknown-rotation", "movable")
        and not b.exempt
    ]

    # Group by frame so the 4F flat and the ground floor never interact.
    by_frame: dict[str, list[Box]] = {}
    for box in surfaces:
        by_frame.setdefault(box.frame, []).append(box)

    for frame, frame_boxes in by_frame.items():
        structures = [b for b in frame_boxes if b.is_structure()]
        props = [
            b for b in checkable
            if b.frame == frame and not b.is_structure()
        ]

        # Horizontal surfaces a prop could legitimately rest on.
        supports = [
            b for b in frame_boxes
            if b.size[0] >= 8.0 and b.size[1] >= 8.0
        ]

        for prop in props:
            findings.extend(_check_support(prop, supports, frame_boxes))
            findings.extend(_check_embedding(prop, structures))

    return findings


def _check_support(
        prop: Box, supports: list[Box], neighbours: list[Box]) -> list[Finding]:
    """A prop with air under it must be held up by something.

    Anything that touches the prop counts as holding it up: a shelf beneath, a
    wall behind a bracket, a rail it hangs from. Only a box that touches
    nothing at all in any direction is reported, because that is the one case
    with no physical reading — it is simply suspended in the room.
    """
    base = prop.minimum[2]
    best_gap = None
    for other in supports:
        if other is prop:
            continue
        if footprint_overlap(prop, other) <= 0.0:
            continue
        top = other.maximum[2]
        if top > prop.maximum[2] - 0.01:
            continue  # taller than the prop: a wall beside it, not a shelf
        gap = base - top
        if gap < -0.01:
            continue  # the prop reaches into it; that is contact, not air
        if best_gap is None or gap < best_gap:
            best_gap = gap

    if best_gap is not None and best_gap <= FLOAT_GAP_TOLERANCE:
        return []
    if _has_lateral_contact(prop, neighbours) or _has_overhead_contact(
            prop, neighbours):
        return []

    detail = (
        f"{best_gap:.1f} cm of air under the base"
        if best_gap is not None
        else "nothing under the footprint"
    )
    return [Finding(
        "FLOATING", "error", prop,
        f"{detail}; touches nothing (base z={prop.minimum[2]:.1f})",
    )]


def _has_lateral_contact(prop: Box, others: list[Box]) -> bool:
    """True when something touches a side face: a bracket, wall or rail."""
    p_min, p_max = prop.minimum, prop.maximum
    for other in others:
        if other is prop:
            continue
        z = overlap_1d(p_min[2], p_max[2], other.minimum[2], other.maximum[2])
        if z <= 0.1:
            continue
        x = overlap_1d(p_min[0], p_max[0], other.minimum[0], other.maximum[0])
        y = overlap_1d(p_min[1], p_max[1], other.minimum[1], other.maximum[1])
        if x > -0.6 and y > -0.6 and (x > 0.0 or y > 0.0):
            return True
    return False


def _has_overhead_contact(prop: Box, others: list[Box]) -> bool:
    """True when the prop hangs from something directly above it."""
    for other in others:
        if other is prop:
            continue
        if footprint_overlap(prop, other) <= 0.0:
            continue
        gap = other.minimum[2] - prop.maximum[2]
        if -0.5 <= gap <= FLOAT_GAP_TOLERANCE:
            return True
    return False


def _fully_swallowed(prop: Box, structure: Box, exposure=0.3) -> bool:
    """True when no part of the prop stands clear of the structure."""
    for axis in range(3):
        if prop.minimum[axis] < structure.minimum[axis] - exposure:
            return False
        if prop.maximum[axis] > structure.maximum[axis] + exposure:
            return False
    return True


def _check_embedding(prop: Box, structures: list[Box]) -> list[Finding]:
    """A prop must not be swallowed by a wall or a slab.

    Trim is *supposed* to key into structure: a door jamb sits inside its
    opening, skirting laps the wall, a threshold beds into the floor. So a few
    centimetres of overlap says nothing. What cannot happen is an object that
    is mostly *inside* the wall, or one whose base is well under the floor it
    is standing on while its body is above it.
    """
    findings = []
    volume = prop.size[0] * prop.size[1] * prop.size[2]
    if volume <= 0.0:
        return findings

    for structure in structures:
        if any(abs(angle) > 1e-6 for angle in structure.rotation):
            # A rotated slab's axis-aligned box is far bigger than the slab,
            # so "inside it" means nothing. A raked handrail would otherwise
            # swallow every baluster under it.
            continue
        depths = boxes_overlap(prop, structure, 0.0)
        if depths is None:
            continue
        contained = (depths[0] * depths[1] * depths[2]) / volume

        # Standing well below the surface it rests on, with its body above:
        # the object was dropped through the floor rather than onto it.
        # Trim is bedded into structure on purpose, so the bar is a visible
        # share of the object's own height, not a bare millimetre count.
        top = structure.maximum[2]
        sink = top - prop.minimum[2]
        sink_limit = max(SINK_TOLERANCE, prop.size[2] * SINK_HEIGHT_SHARE)
        if (structure.size[2] <= STRUCTURE_MAX_THICKNESS
                and sink > sink_limit
                and prop.maximum[2] > top
                and contained < CONTAINMENT_LIMIT):
            findings.append(Finding(
                "SUNK", "warning", prop,
                f"base {sink:.1f} cm below the surface at z={top:.1f}",
                structure,
            ))
            continue

        # Recessed fittings are normal building: a flush distribution board,
        # a door jamb in its opening, a sunk floor channel. What is wrong is a
        # prop with no face left outside the structure at all, because then
        # nobody can ever see it.
        if contained >= CONTAINMENT_LIMIT and _fully_swallowed(prop, structure):
            findings.append(Finding(
                "EMBEDDED", "error", prop,
                f"{contained * 100.0:.0f}% inside structure and no face of it "
                f"reaches the surface",
                structure,
            ))
    return findings


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="machine-readable")
    parser.add_argument("--check", action="store_true",
                        help="exit 1 when any finding is reported")
    parser.add_argument("--coverage", action="store_true",
                        help="report unresolved placement expressions")
    arguments = parser.parse_args(argv)

    all_boxes: list[Box] = []
    resolved = unresolved = 0
    examples: list[str] = []
    for relative in SOURCES:
        result = scan_source(relative)
        all_boxes.extend(result.boxes)
        resolved += result.resolved
        unresolved += result.unresolved
        examples.extend(result.unresolved_examples)

    findings = audit(all_boxes)

    if arguments.json:
        print(json.dumps({
            "boxes": len(all_boxes),
            "resolved": resolved,
            "unresolved": unresolved,
            "findings": [f.to_dict() for f in findings],
        }, indent=2))
    else:
        total = resolved + unresolved
        coverage = (resolved / total * 100.0) if total else 0.0
        exempt = [b for b in all_boxes if b.exempt]
        print(f"WORLD GEOMETRY AUDIT  boxes={len(all_boxes)} "
              f"coverage={coverage:.1f}% ({resolved}/{total}) "
              f"exempt={len(exempt)}")
        for box in exempt:
            print(f"  exempt  {box.label()}: {box.exempt}")
        if arguments.coverage and examples:
            print("\nunresolved placements:")
            for example in examples:
                print(f"  {example}")
        if not findings:
            print("\nno physically impossible placements found")
        else:
            print(f"\n{len(findings)} finding(s):\n")
            for finding in sorted(
                    findings, key=lambda f: (f.code, f.box.source, f.box.line)):
                location = finding.box.label()
                print(f"  [{finding.code}] {location}")
                print(f"      center={finding.box.center} "
                      f"size={tuple(round(v, 1) for v in finding.box.size)}")
                print(f"      {finding.detail}")
                if finding.other is not None:
                    print(f"      against {finding.other.label()}")

    if arguments.check and any(f.severity == "error" for f in findings):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
