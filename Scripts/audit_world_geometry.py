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
SIMULATED_PENETRATION_TOLERANCE = 0.5  # a physics body inside static collision
# Both horizontal spans a slab needs before it is something a prop stands on.
# Below this it is a kerb, a cornice, a handrail -- things props legitimately
# lap into, and things nothing "sinks" into.
FLOOR_MIN_SPAN = 60.0

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

IDENTIFIER = re.compile(r"\b[A-Za-z_]\w*\b")

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

    def Size(self):  # noqa: N802 - mirrors the engine method name
        return math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)

    def Size2D(self):  # noqa: N802
        return math.sqrt(self.x * self.x + self.y * self.y)

    def GetSafeNormal(self):  # noqa: N802
        length = self.Size()
        return Vec3(0.0) if length < 1e-8 else Vec3(
            self.x / length, self.y / length, self.z / length)

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
    "atan2": lambda y, x: math.atan2(y, x),
    "degrees": math.degrees,
    "radians": math.radians,
    # 난간과 배관처럼 두 점 사이에 놓는 물건은 이 세 개로 자리를 잡는다.
    "lerp": lambda a, b, t: a + (b - a) * t,
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
    "FMath::CeilToInt": "ceil",
    "FMath::FloorToInt": "floor",
    "FMath::Atan2": "atan2",
    "FMath::RadiansToDegrees": "degrees",
    "FMath::DegreesToRadians": "radians",
    "FMath::Lerp": "lerp",
    "UE_PI": repr(math.pi),
    "PI": repr(math.pi),
}


class Unresolved(Exception):
    """Raised when an authored expression depends on runtime state."""


_NAMESPACE_QUALIFIER = re.compile(r"\b(?:IGPrologueWorld|IGThirdMorning)::(\w+)")


def to_python(expression: str) -> str:
    # 소스는 긴 식을 다음 줄로 넘겨 쓴다. 줄바꿈과 들여쓰기를 그대로 두면
    # 파이썬 파서가 「unexpected indent」로 죽어서, 값 하나가 안 풀리고
    # 그 이름을 쓰는 배치가 줄줄이 감사 밖으로 빠진다.
    text = re.sub(r"\s+", " ", expression).strip()
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
# `for (const float WindowX : WindowXs)` — 값을 중괄호로 바로 적지 않고
# 위에서 선언한 배열을 도는 꼴. 이걸 못 읽으면 그 안의 배치가 전부
# 감사 밖으로 빠진다 — 가로변 창 네 짝이 그 상태였다.
RANGE_FOR_ARRAY = re.compile(
    r"\bfor\s*\(\s*(?:const\s+)?(?:float|double|int32|auto)\s*&?\s*"
    r"(?P<name>\w+)\s*:\s*(?P<array>\w+)\s*\)"
)
ACTIVE_PARENT = re.compile(r"\bActiveParent\s*=\s*(?P<value>[\w:]+)\s*;")

PLACEMENT_CALLS = {
    # name: (center index, size index, material index, collision index,
    #        rotation index, kind)
    "CreateBlock": (0, 1, 2, 3, None, "block"),
    # (Center, Size, BodyMaterial, PrintMaterial, PrintFacing, bCollision,
    #  bPrintBothFaces). 몸통은 CreateBlock과 같은 상자이고, 인쇄판은 그
    #  안에서 4 mm 드레싱으로 붙는다 — 드레싱은 이 감사가 어차피 봐준다.
    "CreatePrintedBlock": (0, 1, 2, 5, None, "block"),
    "AddStoreStockBlock": (0, 1, 2, None, 4, "stock"),
    # (Mesh, Material, Scale, Location, Rotation, MassKg). Its third argument
    # is a component scale, not centimetres, so the size is recovered from the
    # engine unit shape it scales.
    "CreatePhysicsProp": (3, 2, 1, None, 4, "physics"),
}

# Calls whose "size" argument is a scale of a 100 cm engine shape.
SCALE_ARGUMENT_CALLS = {"CreatePhysicsProp"}

# Where each call names the static mesh it uses, when it is not CreateBlock.
MESH_ARGUMENT_INDEX = {"CreatePhysicsProp": 0}


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
    # 파일 스코프 회전은 여태 한 번도 걷지 않았다. 못 읽은 회전은 상자를
    # 대각선 길이짜리 정육면체로 부풀리고 `unknown-rotation`을 달아 감사
    # 판정에서 통째로 빼기 때문에, 이름 하나가 빠지면 그 자리는 검사되지
    # 않는 것과 같다. CCTV 카메라 회전이 그 상태였다.
    for match in CONST_ROTATOR.finditer(body):
        value = match.group("value").strip()
        # 값 부분이 닫는 괄호까지 삼킨다. 본문 스캐너와 같은 손질이다.
        while value.endswith(")") and value.count(")") > value.count("("):
            value = value[:-1].rstrip()
        expression = value if value.startswith("FRotator") \
            else f"FRotator({value})"
        try:
            scope[match.group("name")] = as_rot(evaluate(expression, scope))
        except Unresolved:
            pass
    return scope


def _lambda_body_offset(text: str, offset: int) -> int:
    """LAMBDA_DEF already stops at the body brace; kept for readability."""
    return offset


def _lambda_parameters(parameter_text: str) -> list[tuple]:
    """(이름, 기본값) 쌍. 이름을 못 읽으면 이름 자리가 None이다."""
    names: list[str | None] = []
    for parameter in split_arguments(parameter_text):
        # 기본값이 붙은 인자는 `= ` 앞까지가 이름이다. 통째로 읽으면
        # `const FRotator& Rotation = FRotator::ZeroRotator`에서 마지막
        # 식별자인 ZeroRotator를 이름으로 잡고, 정작 Rotation은 어디에도
        # 묶이지 않는다. 그러면 람다 안의 회전이 안 풀려 상자가 대각선
        # 길이짜리 정육면체로 부푼다.
        declaration, _, default = parameter.partition("=")
        identifiers = re.findall(IDENTIFIER, declaration)
        names.append((identifiers[-1] if identifiers else None,
                      default.strip() or None))
    return names


def _balanced_value(text: str, start: int) -> str:
    """괄호 밖의 첫 `;` 또는 `,`까지를 한 값으로 읽는다."""
    depth = 0
    index = start
    while index < len(text):
        character = text[index]
        if character in "([{":
            depth += 1
        elif character in ")]}":
            if depth == 0:
                break
            depth -= 1
        elif character in ";," and depth == 0:
            break
        index += 1
    return text[start:index]


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
        # 람다 인자의 **원문**. 배치 함수는 재질과 충돌 플래그, 메시 이름을
        # 평가하지 않고 글자로 읽으므로, 헬퍼 안에서는 파라미터 이름만
        # 보인다. 호출부가 준 글자로 바꿔 주어야 재질과 충돌 여부가 잡힌다.
        self.text_bindings: dict[str, str] = {}

    def run(self, function_name: str, span: tuple[int, int], frame: str):
        self.function = function_name
        self.frame = frame
        self.lambdas = {}
        self.text_bindings = {}
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
        for pattern, loop_kind in ((RANGE_FOR, "loop"), (COUNT_FOR, "count"),
                                   (RANGE_FOR_ARRAY, "array-loop")):
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

        # 람다는 이벤트를 모으기 전에 등록해 둔다. 예전에는 이벤트를 다 모은
        # 뒤에 등록했기 때문에, 같은 블록에서 정의하고 바로 부르는 람다는
        # 호출부가 이벤트로 잡히지 않았다. 옥상 난간처럼 헬퍼 하나로 만드는
        # 물건이 통째로 감사 밖에 있었다.
        for match in LAMBDA_DEF.finditer(text, start, end):
            body = _block_after(text, _lambda_body_offset(text, match.end()))
            if body is not None:
                self.lambdas.setdefault(
                    match.group("name"),
                    (_lambda_parameters(match.group("params")), body))

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
            if kind == "array-loop":
                match, block = payload
                values = scope.get(match.group("array"))
                if isinstance(values, list):
                    self._iterate(
                        block, match.group("name"), values, scope, depth)
            elif kind == "loop":
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
                # 값 자체가 이미 벡터인 경우가 있다 — `End - Start`,
                # `(Start + End) * 0.5f`처럼. 무조건 FVector()로 감싸면
                # 벡터를 스칼라 생성자에 넣는 꼴이 되어 통째로 풀리지 않는다.
                scope.pop(name, None)
                for expression in (
                        value,
                        value if value.strip().startswith("FVector")
                        else f"FVector({value})"):
                    try:
                        scope[name] = as_vec(evaluate(expression, scope))
                        break
                    except Unresolved:
                        continue
            elif kind == "rotator":
                name = payload.group("name")
                value = payload.group("value").strip()
                # 정규식의 값 부분이 욕심을 부려 닫는 괄호까지 삼킨다.
                # `const FRotator R(0, 90, 0);`이 `0, 90, 0)`로 잡히고,
                # 그것을 FRotator()로 감싸면 괄호가 하나 남아 못 읽는다.
                # 회전이 안 풀리면 감사는 상자를 대각선 길이짜리 정육면체로
                # 부풀리므로, 저작 메시 하나가 방 안 소품을 통째로 삼킨다.
                while value.endswith(")") and value.count(")") > value.count("("):
                    value = value[:-1].rstrip()
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
        text_nested = dict(self.text_bindings)
        for index, (parameter, default) in enumerate(parameters):
            if parameter is None:
                continue
            # 호출부가 인자를 생략하면 기본값이 그 자리에 온다. 예전에는
            # 그냥 비워 두어서, 기본값이 붙은 회전 인자를 쓰는 헬퍼의 상자가
            # 전부 「회전을 모름」이 되어 대각선 길이짜리 정육면체로 부풀었다.
            # 그 정육면체 하나가 방 안 소품을 통째로 삼킨다.
            source = arguments[index] if index < len(arguments) else default
            if source is None:
                text_nested.pop(parameter, None)
                nested.pop(parameter, None)
                continue
            text_nested[parameter] = source.strip()
            try:
                nested[parameter] = evaluate(source, scope)
            except Unresolved:
                nested.pop(parameter, None)
        previous = self.text_bindings
        self.text_bindings = text_nested
        try:
            self.walk(block[0], block[1], nested, depth + 1)
        finally:
            self.text_bindings = previous

    def _declare_float(self, match, scope: dict):
        name = match.group("name")
        # 정규식의 값 부분은 쉼표에서 끊긴다 — `const float A = 1, B = 2;`를
        # 갈라 읽어야 하기 때문이다. 그래서 인자가 둘인 함수 호출이 통째로
        # 잘려 나갔다. 괄호 깊이를 세면 둘 다 된다.
        value = _balanced_value(self.text, match.start("value"))
        try:
            scope[name] = evaluate(value, scope)
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
        # 헬퍼 안에서는 인자가 파라미터 이름으로만 보인다. 호출부가 준
        # 원문으로 바꿔 두면 재질 이름과 충돌 플래그가 제대로 읽힌다.
        arguments = [self.text_bindings.get(part.strip(), part)
                     for part in split_arguments(argument_text)]
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
        if call_name in SCALE_ARGUMENT_CALLS:
            size = size * 100.0

        note = ""
        mesh_token = "nullptr"
        if call_name in MESH_ARGUMENT_INDEX:
            slot = MESH_ARGUMENT_INDEX[call_name]
            if len(arguments) > slot:
                mesh_token = arguments[slot].strip()
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

        findings.extend(_check_simulated_start(frame_boxes))

    return findings


def _check_simulated_start(frame_boxes: list[Box]) -> list[Finding]:
    """A simulated body may not start inside something solid.

    Chaos resolves initial penetration by pushing the two apart, so a slipper
    authored inside the shoe step does not rest on the step -- it is spat out
    of it in the first frames of the level, in front of the player.
    """
    findings = []
    simulated = [b for b in frame_boxes if b.kind == "physics"]
    if not simulated:
        return findings
    solids = [b for b in frame_boxes if b.kind != "physics" and b.collision]
    for body in simulated:
        for solid in solids:
            depths = boxes_overlap(body, solid, SIMULATED_PENETRATION_TOLERANCE)
            if depths is None:
                continue
            findings.append(Finding(
                "PENETRATING", "error", body,
                f"a simulated body starts {min(depths):.1f} cm inside static "
                f"collision and will be pushed out of it",
                solid,
            ))
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


def _is_floor_like(box: Box) -> bool:
    """True for a horizontal slab: thin in Z and broad in both X and Y."""
    return (
        box.size[2] <= min(box.size) + 1e-6
        and box.size[0] >= FLOOR_MIN_SPAN
        and box.size[1] >= FLOOR_MIN_SPAN
    )


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
        #
        # Only a floor-like slab can be sunk into. A kerb, a cornice or a
        # handrail is thin in a horizontal axis too, and props lap into those
        # by design -- a railing post passes its own rail, a crate laps the
        # kerb it stands beside. And a slab lapping another slab is how a
        # building is built, not an object dropped through a floor, so a
        # slab-shaped prop is exempt as well.
        #
        # Trim is bedded into structure on purpose, so the bar is a visible
        # share of the object's own height, not a bare millimetre count.
        top = structure.maximum[2]
        sink = top - prop.minimum[2]
        sink_limit = max(SINK_TOLERANCE, prop.size[2] * SINK_HEIGHT_SHARE)
        if (_is_floor_like(structure)
                and not _is_floor_like(prop)
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


def scan_text(text: str) -> ScanResult:
    """파일 대신 문자열을 훑는다. 자기 검사가 쓴다."""
    stripped = strip_comments(text)
    line_starts = [0]
    for index, character in enumerate(stripped):
        if character == "\n":
            line_starts.append(index + 1)

    def line_of(offset: int) -> int:
        low, high = 0, len(line_starts) - 1
        while low < high:
            middle = (low + high + 1) // 2
            if line_starts[middle] <= offset:
                low = middle
            else:
                high = middle - 1
        return low + 1

    result = ScanResult()
    scanner = BodyScanner(
        "IGPrologueWorldScene.cpp", stripped, line_of, result,
        _file_scope_constants(stripped))
    for function_name, brace, _body in function_bodies(stripped):
        end = _block_after(stripped, brace)
        if end:
            scanner.run(function_name, end, "SceneRoot")
    return result


# 자기 검사용 최소 빌더. 헬퍼 람다를 정의한 자리에서 바로 부르고, 회전
# 인자는 한 번은 넘기고 한 번은 생략한다 — 예전 스캐너가 통째로 놓치던 모양.
SELF_TEST_SOURCE = """
void AIGSelfTestScene::Build()
{
	const FRotator Turned(0.0f, 90.0f, 0.0f);
	auto AddPanel = [this](
		const FVector& Center,
		const FVector& Size,
		UMaterialInterface* Material,
		const bool bCollide = true,
		UStaticMesh* Mesh = nullptr,
		const FRotator& Rotation = FRotator::ZeroRotator)
	{
		CreateBlock(Center, Size, Material, bCollide, Mesh, Rotation);
	};
	AddPanel(FVector(0, 0, 50), FVector(40, 10, 100), WallPaint, false);
	AddPanel(FVector(300, 0, 50), FVector(40, 10, 100), DoorSkin, true,
		nullptr, Turned);
	const float ShelfXs[] = {10.0f, 60.0f, 110.0f};
	for (const float ShelfX : ShelfXs)
	{
		const float ShelfZ = 20.0f + ShelfX * 0.1f;
		CreateBlock(
			FVector(ShelfX, 400.0f, ShelfZ),
			FVector(30, 12, 3),
			ShelfBoard,
			false);
	}
	auto AddBar = [this](const FVector& Start, const FVector& End)
	{
		const FVector Delta = End - Start;
		const float Length = Delta.Size2D();
		const FVector Midpoint = (Start + End) * 0.5f;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
		CreateBlock(
			FVector(Midpoint.X, Midpoint.Y, 120.0f),
			FVector(Length, 4.0f, 4.0f),
			RailMetal,
			true,
			nullptr,
			FRotator(0.0f, Yaw, 0.0f));
	};
	AddBar(FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 200.0f, 0.0f));
}
"""


def self_test() -> int:
    failures = []

    names = _lambda_parameters(
        "const FVector& Center, const bool bCollide = true, "
        "const FRotator& Rotation = FRotator::ZeroRotator")
    if [name for name, _ in names] != ["Center", "bCollide", "Rotation"]:
        failures.append("기본값이 붙은 인자의 이름을 잘못 읽는다: "
                        + str([name for name, _ in names]))
    if names[2][1] != "FRotator::ZeroRotator":
        failures.append("기본값 자체를 안 들고 온다")

    source = "const float Yaw = FMath::Atan2(D.Y, D.X); const float A = 1;"
    if _balanced_value(source, source.index("FMath")) \
            != "FMath::Atan2(D.Y, D.X)":
        failures.append("괄호 안 쉼표에서 값이 끊긴다")

    rotator = "const FRotator Turned(0.0f, 90.0f, 0.0f);"
    match = CONST_ROTATOR.search(rotator)
    value = match.group("value").strip()
    while value.endswith(")") and value.count(")") > value.count("("):
        value = value[:-1].rstrip()
    if as_rot(evaluate(f"FRotator({value})", {})).yaw != 90.0:
        failures.append("생성자 꼴 FRotator 선언이 안 풀린다")

    # 파일 스코프 회전을 걷지 않으면, 그것을 쓰는 상자는 회전을 못 읽어
    # 대각선 길이짜리 정육면체가 되고 감사 판정에서 통째로 빠진다.
    hoisted = _file_scope_constants(
        "namespace IGTest\n{\n"
        "\tconst FVector Where(1.0f, 2.0f, 3.0f);\n"
        "\tconst FRotator Facing(-25.0f, 29.0f, 0.0f);\n}\n")
    if not isinstance(hoisted.get("Facing"), Rot3) \
            or hoisted["Facing"].yaw != 29.0:
        failures.append("파일 스코프 FRotator 상수를 걷지 않는다")

    if abs(evaluate("Delta.Size2D()", {"Delta": Vec3(3, 4, 12)}) - 5.0) > 1e-6:
        failures.append("Vec3.Size2D가 없다")
    if abs(evaluate("FMath::RadiansToDegrees(FMath::Atan2(1, 0))", {})
           - 90.0) > 1e-6:
        failures.append("Atan2/RadiansToDegrees가 안 풀린다")
    # 소스는 긴 식을 다음 줄로 넘겨 쓴다. 줄바꿈이 그대로 남으면 파이썬
    # 파서가 죽어서 값 하나가 통째로 안 풀린다.
    wrapped_value = "-150.0f" + chr(10) + "\t\t\t\t+ 25.0f"
    try:
        if abs(evaluate(wrapped_value, {}) + 125.0) > 1e-6:
            failures.append("다음 줄로 이어진 값이 틀리게 풀린다")
    except Unresolved:
        failures.append("다음 줄로 이어진 값이 안 풀린다")

    if as_vec(evaluate("End - Start",
                       {"Start": Vec3(1, 1, 1), "End": Vec3(4, 5, 1)})).x != 3.0:
        failures.append("이미 벡터인 초기값을 못 읽는다")

    result = scan_text(SELF_TEST_SOURCE)
    panels = [box for box in result.boxes if box.material in
              ("WallPaint", "DoorSkin")]
    if len(panels) != 2:
        failures.append("같은 블록에서 정의하고 바로 부른 람다를 놓친다 ("
                        + str(len(panels)) + "개)")
    else:
        wall = next(box for box in panels if box.material == "WallPaint")
        door = next(box for box in panels if box.material == "DoorSkin")
        if wall.collision or not door.collision:
            failures.append("람다로 넘긴 충돌 플래그를 안 읽는다")
        if abs(wall.size[0] - 40.0) > 0.01 or abs(wall.size[1] - 10.0) > 0.01:
            failures.append("회전 인자를 생략한 상자가 부풀었다: "
                            + str(wall.size))
        if abs(door.size[0] - 10.0) > 0.01 or abs(door.size[1] - 40.0) > 0.01:
            failures.append("넘긴 회전이 상자에 안 먹었다: " + str(door.size))
    shelves = [box for box in result.boxes if box.material == "ShelfBoard"]
    if len(shelves) != 3:
        failures.append("이름 붙인 배열을 도는 반복문을 놓친다 ("
                        + str(len(shelves)) + "개)")

    bars = [box for box in result.boxes if box.material == "RailMetal"]
    if len(bars) != 1:
        failures.append("두 점으로 놓는 헬퍼를 놓친다")
    elif abs(bars[0].size[1] - 200.0) > 0.01 or abs(bars[0].size[0] - 4.0) > 0.01:
        failures.append("길이·요각을 계산하는 헬퍼의 상자가 틀렸다: "
                        + str(bars[0].size))

    for failure in failures:
        print("  자기 검사 실패: " + failure)
    print("WORLD GEOMETRY SELF-TEST " + ("FAIL" if failures else "PASS")
          + " boxes=" + str(len(result.boxes)))
    return 1 if failures else 0


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
    parser.add_argument("--self-test", action="store_true",
                        help="check the scanner itself, without the sources")
    arguments = parser.parse_args(argv)

    if arguments.self_test:
        return self_test()

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
