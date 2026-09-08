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

# 저작 메시를 쓰는 상자는 크기 인자가 배율이라 상자를 세울 수 없었다. 실제
# 크기는 소스가 아니라 구운 에셋에 있고, `Scripts/export_mesh_bounds.py`가
# 그것을 여기로 뽑아 둔다. 이 표가 있으면 배율에 곱해 진짜 상자가 나온다.
MESH_BOUNDS_RELATIVE = os.path.join("Docs", "mesh_bounds.json")
# `WaterBottleMesh = IGThirdMorning::LoadMesh(TEXT("/Game/Meshes/SM_X.SM_X"));`
# 이나 `Mesh = PropMesh(TEXT("SM_X"));` 꼴로 이름이 붙는다.
MESH_BINDING = re.compile(
    r"(?P<name>\w+)\s*=\s*[^;]*?(?:/Game/[\w/]*?(?P<path>SM_\w+)\."
    r"|(?:PropMesh|LoadMesh)\(\s*TEXT\(\s*\"(?P<short>SM_\w+)\")")


def load_mesh_bounds() -> dict:
    path = os.path.join(PROJECT_ROOT, MESH_BOUNDS_RELATIVE)
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle).get("meshes", {})
    except (OSError, ValueError):
        return {}


MESH_BOUNDS = load_mesh_bounds()


def _authored_mesh_name(token: str, bindings: dict):
    """메시 인자가 가리키는 에셋 이름. 삼항이면 저작 메시 쪽을 고른다."""
    for part in re.split(r"[?:]", token):
        part = part.strip().replace(".Get()", "")
        inline = re.search(r'TEXT\(\s*"(SM_\w+)"', part)
        if inline:
            return inline.group(1)
        if re.fullmatch(r"\w+", part):
            if part in bindings:
                return bindings[part]
            if part.startswith("SM_"):
                return part
    return None

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


def _rotation_basis(rot: Rot3) -> tuple:
    """Unreal's rotation order is Roll (X), then Pitch (Y), then Yaw (Z)."""
    cp, sp = _cos_sin(rot.pitch)
    cy, sy = _cos_sin(rot.yaw)
    cr, sr = _cos_sin(rot.roll)
    return (
        (cp * cy, sr * sp * cy - cr * sy, cr * sp * cy + sr * sy),
        (cp * sy, sr * sp * sy + cr * cy, cr * sp * sy - sr * cy),
        (-sp, sr * cp, cr * cp),
    )


def rotate_vector(vector: Vec3, rot: Rot3) -> Vec3:
    """메시 바운드 원점처럼 피벗에서 밀린 오프셋을 월드 축으로 돌린다."""
    if rot.is_identity():
        return Vec3(vector.x, vector.y, vector.z)
    basis = _rotation_basis(rot)
    local = (vector.x, vector.y, vector.z)
    return Vec3(*[sum(row[i] * local[i] for i in range(3)) for row in basis])


def rotated_extent(size: Vec3, rot: Rot3) -> Vec3:
    """Axis-aligned full size of a box after an FRotator, in world axes."""
    if rot.is_identity():
        return Vec3(size.x, size.y, size.z)

    basis = _rotation_basis(rot)
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
    if "?" not in text:
        return text
    # 깊이 0에 삼항이 없다면 괄호 안에 들어앉은 것이다. `FRotator(0,
    # StepIndex % 2 == 0 ? -12.0f : 12.0f, 0)`이 그 꼴이고, 여태 통째로
    # 못 읽어 그 회전을 쓰는 상자가 전부 판정에서 빠졌다. 인자를 최상위
    # 쉼표로 가른 다음 하나씩 다시 본다 — 가르지 않으면 조건 자리에
    # 앞 인자까지 딸려 들어간다.
    pieces = []
    index = 0
    while index < len(text):
        character = text[index]
        if character not in "([{":
            pieces.append(character)
            index += 1
            continue
        close = _matching_bracket(text, index)
        if close is None:
            pieces.append(text[index:])
            break
        inner = text[index + 1:close]
        arguments = split_arguments(inner)
        pieces.append(character)
        pieces.append(
            ", ".join(_convert_ternaries(one) for one in arguments)
            if arguments else inner)
        pieces.append(text[close])
        index = close + 1
    return "".join(pieces)


def _matching_bracket(text: str, start: int) -> int | None:
    """start의 여는 괄호와 짝이 되는 닫는 괄호 위치."""
    depth = 0
    for index in range(start, len(text)):
        if text[index] in "([{":
            depth += 1
        elif text[index] in ")]}":
            depth -= 1
            if depth == 0:
                return index
    return None


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
    # 구운 바운드로 크기를 푼 저작 메시의 에셋 이름. 그 상자는 크기는
    # 믿을 수 있어도 **속이 꽉 찼다고 믿을 수는 없다** — 물탱크 셸도
    # 서비스 캐비닛도 껍데기라, AABB를 구조물로 쓰면 그 안의 물건이
    # 전부 파묻힌 것으로 잡힌다.
    mesh: str = ""
    # 배치 인자 그대로의 원점·크기와 엔진 단위 메시 이름. 감사는 안 쓰고
    # Blender 배치 렌더(Scripts/blender/render_scene_layout.py)가 읽는다 —
    # 회전·구운 바운드로 부풀린 AABB로는 메시를 제자리에 다시 세울 수 없다.
    location: tuple[float, float, float] | None = None
    local_size: tuple[float, float, float] | None = None
    shape: str = ""

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
    r"\b(?:static\s+)?(?:const|constexpr)\s+(?:float|double|int32|bool)\s+"
    r"(?P<name>\w+)\s*=\s*(?P<value>[^;{},]+)\s*[;,]"
)
# `const float A = 1.0f, B = 2.0f;` continues after the first declarator.
CONST_FLOAT_CONTINUED = re.compile(r",\s*(?P<name>\w+)\s*=\s*(?P<value>[^;{},]+)")
# `int32 ChilledIndex = 0;` — 루프 안에서 ++로 올리는 수동 카운터. const가
# 아니라 여태 스코프에 들어오지 못했고, _iterate는 이미 스코프에 있는 이름만
# 카운터로 잡기 때문에 그 카운터를 쓰는 배치가 통째로 빠졌다. 정수 리터럴로
# 시작하는 선언만 본다. 여는 괄호 뒤는 for 머리이므로 뺀다.
MUTABLE_INT = re.compile(
    r"(?<![(,])\b(?:int32|int)\s+(?P<name>\w+)\s*=\s*(?P<value>-?\d+)\s*;"
)
# `for (const float LegX : {-164.0f, -36.0f})` — the builders lay out repeated
# legs, treads, rails and shelves this way, and skipping them would hide most
# of the small parts from the audit.
# 값 목록을 중괄호로 바로 적는 범위 for. FVector도 이렇게 돈다 — 계량기함
# 두 짝이 `for (const FVector& UnitCenter : {FVector(...), FVector(...)})`인데
# 타입 목록에 FVector가 없어 그 안의 배치가 빠져 있었다.
RANGE_FOR = re.compile(
    r"\bfor\s*\(\s*(?:const\s+)?(?:float|double|int32|FVector|auto)\s*&?\s*"
    r"(?P<name>\w+)\s*:\s*\{(?P<values>[^{}]*(?:\([^()]*\)[^{}]*)*)\}\s*\)"
)
# `for (int32 Index = 0; Index < 14; ++Index)` — treads, balusters, shelves.
COUNT_FOR = re.compile(
    r"\bfor\s*\(\s*(?:const\s+)?(?:int32|int|uint32|size_t)\s+(?P<name>\w+)"
    r"\s*=\s*(?P<init>[^;]+);\s*(?P=name)\s*<\s*(?P<limit>[^;]+);"
    r"\s*(?:\+\+\s*(?P=name)|(?P=name)\s*\+\+)\s*\)"
)
# `for (float SnackX = 2500.0f; SnackX <= 2780.0f; SnackX += 14.0f)` — 진열대는
# 개수가 아니라 간격으로 채운다. 정수 카운터만 보던 탓에 편의점 매대와 담배
# 진열대, 화강석 줄눈, 필로티 기둥이 통째로 감사 밖에 있었다. 조건에 붙은
# 가드(`!bRamyeonBay &&`)는 평가하지 않고 그냥 돈다 — 분기를 모르는 채로는
# 도는 쪽이 더 많은 상자를 보게 한다.
STEP_FOR = re.compile(
    r"\bfor\s*\(\s*(?:const\s+)?(?:float|double)\s+(?P<name>\w+)\s*=\s*"
    r"(?P<init>[^;]+);(?P<guard>[^;]*?)(?P=name)\s*(?P<op><=|<)\s*"
    r"(?P<limit>[^;]+);\s*(?P=name)\s*\+=\s*(?P<step>[^)]+)\)"
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

# 골목 점포 넷은 `struct FShopSpec { float X; float Width; ... };`를 세워
# 리터럴 배열로 적고, 그 원소를 인덱스로 꺼내 쓴다. 이 세 조각 중 하나라도
# 못 읽으면 그 안의 배치가 통째로 감사 밖으로 빠진다 — 점포 간판, 차양,
# 돌출 간판 열일곱 개가 그 상태였다.
STRUCT_DECL = re.compile(
    r"\bstruct\s+(?P<name>\w+)\s*\{(?P<body>[^{}]*)\}\s*;")
STRUCT_ARRAY = re.compile(
    r"\bconst\s+(?P<type>\w+)\s+(?P<name>\w+)\s*\[\s*\w*\s*\]\s*=\s*"
    r"\{(?P<values>[^{}]*(?:\{[^{}]*\}[^{}]*)*)\}\s*;")
# `const FShopSpec& Shop = Shops[ShopIndex];`
RECORD_ALIAS = re.compile(
    r"\bconst\s+(?P<type>\w+)\s*&\s*(?P<name>\w+)\s*=\s*"
    r"(?P<value>[^;{}]+?)\s*;")


def _struct_fields(text: str) -> dict:
    """구조체 이름 -> 선언 순서대로의 필드 이름."""
    table = {}
    for match in STRUCT_DECL.finditer(text):
        fields = []
        for declaration in match.group("body").split(";"):
            identifiers = re.findall(IDENTIFIER, declaration)
            if identifiers:
                fields.append(identifiers[-1])
        if fields:
            table[match.group("name")] = fields
    return table


class Record:
    """구조체 리터럴 하나.

    값과 원문을 함께 들고 있어야 한다. 좌표는 평가한 값으로 쓰지만,
    재질은 감사가 `TexMat(TEXT("M_X"), Fallback)` 꼴을 글자로 읽기
    때문에 원문 그대로 돌려주어야 이름이 잡힌다.
    """

    def __init__(self, fields: dict, texts: dict):
        self.__dict__.update(fields)
        object.__setattr__(self, "_source_text", texts)

    def source_text(self) -> dict:
        return self.__dict__["_source_text"]

    def __repr__(self):
        shown = {k: v for k, v in self.__dict__.items()
                 if k != "_source_text"}
        return "Record(%s)" % shown


def _expand_record_fields(argument: str, scope: dict) -> str:
    """`Shop.Sign`처럼 구조체 필드를 가리키는 조각을 원문으로 되돌린다."""
    if "." not in argument:
        return argument
    for name, value in scope.items():
        if not isinstance(value, Record):
            continue
        for field, cell in value.source_text().items():
            argument = re.sub(
                r"\b" + re.escape(name) + r"\." + re.escape(field) + r"\b",
                cell.replace("\\", "\\\\"),
                argument)
    return argument


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
        self.structs = _struct_fields(text)
        self.mesh_bindings = {
            match.group("name"): match.group("path") or match.group("short")
            for match in MESH_BINDING.finditer(text)
        }
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
                                   (STEP_FOR, "step"),
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
            (MUTABLE_INT, "float"),
            (CONST_ARRAY, "array"),
            (CONST_VECTOR_ASSIGN, "vector"),
            (CONST_VECTOR_CTOR, "vector"),
            (CONST_ROTATOR, "rotator"),
            (STRUCT_ARRAY, "record-array"),
            (RECORD_ALIAS, "record"),
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
            elif kind == "step":
                match, block = payload
                try:
                    value = float(evaluate(match.group("init"), scope))
                    limit = float(evaluate(match.group("limit"), scope))
                    step = float(evaluate(match.group("step"), scope))
                except (Unresolved, TypeError, ValueError):
                    continue
                if step == 0.0:
                    continue
                values = []
                while len(values) < MAX_LOOP_ITERATIONS:
                    if step > 0.0:
                        if value > limit + (1e-6 if match.group("op") == "<=" else 0.0):
                            break
                        if match.group("op") == "<" and value >= limit:
                            break
                    else:
                        if value < limit - (1e-6 if match.group("op") == "<=" else 0.0):
                            break
                    values.append(value)
                    value += step
                self._iterate(block, match.group("name"), values, scope, depth)
            elif kind == "lambda":
                match, block = payload
                self.lambdas[match.group("name")] = (
                    _lambda_parameters(match.group("params")), block)
            elif kind == "invoke":
                self._invoke(payload[0], payload[1], scope, depth)
            elif kind == "record-array":
                self._declare_record_array(payload, scope)
            elif kind == "record":
                self._declare_record(payload, scope)
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

    def _declare_record_array(self, match, scope: dict):
        """구조체 리터럴 배열을 Record 목록으로 묶는다."""
        fields = self.structs.get(match.group("type"))
        if not fields:
            return
        records = []
        for raw in split_arguments(match.group("values")):
            raw = raw.strip()
            if not (raw.startswith("{") and raw.endswith("}")):
                return
            values = {}
            texts = {}
            for name, cell in zip(fields, split_arguments(raw[1:-1])):
                texts[name] = cell.strip()
                try:
                    values[name] = evaluate(cell, scope)
                except Unresolved:
                    # 재질과 텍스처 이름은 글자 그대로 쓰인다. 평가하지
                    # 못해도 원문을 들고 있어야 재질 자리가 비지 않는다.
                    values[name] = cell.strip()
            records.append(Record(values, texts))
        if records:
            scope[match.group("name")] = records

    def _declare_record(self, match, scope: dict):
        """`const FShopSpec& Shop = Shops[ShopIndex];` 같은 별칭."""
        if match.group("type") not in self.structs:
            return
        try:
            value = evaluate(match.group("value"), scope)
        except Unresolved:
            return
        if isinstance(value, Record):
            scope[match.group("name")] = value

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
        # 구조체 필드도 마찬가지다. `TexMat(Shop.Sign, ...)`을 글자 그대로
        # 두면 재질을 읽는 감사 셋이 그 상자를 「이름을 못 푼 것」으로
        # 넘긴다 — 점포 간판 넷이 인쇄면 검사 밖에 있었다.
        arguments = [_expand_record_fields(part, scope) for part in arguments]
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
        # 저작 메시는 크기 인자가 배율이다. 구운 바운드를 알면 그 배율에
        # 곱해 진짜 상자가 나오고, 그때부터 이 상자도 판정 대상이 된다.
        # 바운드 원점은 피벗에서 밀린 값이므로 회전을 태워 중심에 더한다.
        frame_offset = FRAME_OFFSETS.get(self.frame, (0.0, 0.0, 0.0))
        placement = (center.x + frame_offset[0], center.y + frame_offset[1],
                     center.z + frame_offset[2])
        local_size = (size.x, size.y, size.z)
        bounds = None
        asset = None
        if base_mesh not in ENGINE_UNIT_MESHES or (
                abs(size.x - 100.0) < 1e-6 and abs(size.y - 100.0) < 1e-6
                and abs(size.z - 100.0) < 1e-6):
            asset = _authored_mesh_name(mesh_token, self.mesh_bindings)
            bounds = MESH_BOUNDS.get(asset) if asset else None
        if bounds is not None and rotation_known:
            scale = (size.x / 100.0, size.y / 100.0, size.z / 100.0)
            size = Vec3(*[bounds["extent"][i] * 2.0 * scale[i]
                          for i in range(3)])
            shift = rotate_vector(
                Vec3(*[bounds["origin"][i] * scale[i] for i in range(3)]),
                rotation)
            center = Vec3(center.x + shift.x, center.y + shift.y,
                          center.z + shift.z)
        elif not note:
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
        if bounds is not None and base_mesh == "PlaneMesh" and not note:
            note = "plane"

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
            mesh=(asset or "") if bounds is not None else "",
            location=placement,
            local_size=local_size,
            shape=base_mesh if base_mesh in ENGINE_UNIT_MESHES else "",
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
        # 배치 헬퍼 자신의 몸통은 배치가 아니다. `CreatePrintedBlock`이
        # 자기 안에서 부르는 CreateBlock은 인자가 그 함수의 파라미터라
        # 영원히 안 풀리고, 못 푼 자리로만 세어져 커버리지를 깎는다.
        if function_name.split("::")[-1] in PLACEMENT_CALLS:
            continue
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
        # 저작 메시의 AABB는 바깥 한계이지 고체가 아니다. 구조물로 쓰면
        # 속이 빈 물탱크 셸이 그 안의 인체와 사다리를 통째로 삼킨다.
        structures = [
            b for b in frame_boxes if b.is_structure() and not b.mesh]
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
            # 시뮬레이션 물체는 떨어져서 자리를 잡는다. 저작 좌표는 시작
            # 자세이지 놓인 자리가 아니다 — movable 블록과 같은 이유이고,
            # 파고든 것은 _check_simulated_start가 더 엄한 눈으로 본다.
            if prop.kind != "physics":
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

    # 괄호 안에 들어앉은 삼항. 여태 깊이 0만 봐서 `FRotator(0, cond ? a : b,
    # 0)` 꼴이 통째로 안 읽혔고, 그 회전을 쓰는 상자는 전부 판정에서 빠졌다.
    try:
        nested = as_rot(evaluate(
            "FRotator(0, StepIndex % 2 == 0 ? -12.0f : 12.0f, 0)",
            {"StepIndex": 1})).yaw
        first = as_vec(evaluate(
            "FVector(A ? 1 : 2, 3, 4)", {"A": False})).x
    except Unresolved:
        nested = first = None
    if nested != 12.0:
        failures.append("인자 안에 들어앉은 삼항을 못 읽는다")
    if first != 2.0:
        failures.append("첫 인자의 삼항에 앞 인자가 딸려 들어간다")

    # 지역 구조체 배열을 인덱스로 도는 배치. 셋 중 하나만 못 읽어도
    # 그 안의 상자가 통째로 감사 밖으로 빠진다 — 골목 점포 넷의 기둥과
    # 간판, 차양이 그 상태였고, 열리자마자 인쇄면 결함 넷이 나왔다.
    record_source = (
        "void AIGTest::Build()\n{\n"
        "\tstruct FShopSpec { float X; float Width; const TCHAR* Sign; };\n"
        "\tconst FShopSpec Shops[] = {\n"
        "\t\t{400.0f, 150.0f, TEXT(\"M_SignA\")},\n"
        "\t\t{900.0f, 128.0f, TEXT(\"M_SignB\")},\n"
        "\t};\n"
        "\tfor (int32 ShopIndex = 0; ShopIndex < 2; ++ShopIndex)\n\t{\n"
        "\t\tconst FShopSpec& Shop = Shops[ShopIndex];\n"
        "\t\tCreateBlock(FVector(Shop.X, 0, 0), "
        "FVector(Shop.Width, 16, 42), TexMat(Shop.Sign, Fallback));\n"
        "\t}\n}\n")
    record_result = scan_text(record_source)
    if len(record_result.boxes) != 2:
        failures.append("구조체 배열을 도는 배치를 못 읽는다 (%d개)"
                        % len(record_result.boxes))
    elif record_result.boxes[1].center[0] != 900.0 \
            or record_result.boxes[1].size[0] != 128.0:
        failures.append("구조체 필드를 좌표와 크기로 못 푼다")
    elif "M_SignB" not in record_result.boxes[1].material:
        failures.append("구조체 필드가 재질 자리에서 원문으로 안 돌아온다")

    # 진열대는 개수가 아니라 간격으로 채운다. 정수 카운터만 보던 탓에
    # 편의점 매대가 통째로 감사 밖에 있었다. 조건에 붙은 가드는 평가하지
    # 않고 그냥 돈다.
    step_source = (
        "void AIGTest::Build()\n{\n"
        "\tfor (float ItemX = 100.0f; !bGuard && ItemX <= 160.0f; "
        "ItemX += 20.0f)\n\t{\n"
        "\t\tCreateBlock(FVector(ItemX, 0, 0), FVector(4, 4, 4), Mat);\n"
        "\t}\n}\n")
    step_result = scan_text(step_source)
    if [box.center[0] for box in step_result.boxes] != [100.0, 120.0, 140.0, 160.0]:
        failures.append("실수 증분 for 루프를 제대로 못 돈다 (%s)"
                        % [box.center[0] for box in step_result.boxes])

    # 루프 밖에서 선언하고 안에서 ++로 올리는 카운터. const가 아니라
    # 스코프에 들어오지 못하면 그것을 쓰는 배치가 통째로 빠진다.
    counter_source = (
        "void AIGTest::Build()\n{\n"
        "\tint32 Tally = 0;\n"
        "\tfor (const float Where : {0.0f, 50.0f})\n\t{\n"
        "\t\tCreateBlock(FVector(Where, 0, 0), "
        "(Tally % 2) == 0 ? FVector(9, 9, 9) : FVector(13, 13, 13), Mat);\n"
        "\t\t++Tally;\n\t}\n}\n")
    counter_result = scan_text(counter_source)
    if [round(box.size[0]) for box in counter_result.boxes] != [9, 13]:
        failures.append("루프 안에서 올리는 수동 카운터를 못 따라간다 (%s)"
                        % [round(box.size[0]) for box in counter_result.boxes])

    # 값 목록을 중괄호로 적는 범위 for에 FVector도 온다.
    vector_loop = scan_text(
        "void AIGTest::Build()\n{\n"
        "\tfor (const FVector& Where : "
        "{FVector(10, 20, 30), FVector(40, 50, 60)})\n\t{\n"
        "\t\tCreateBlock(Where, FVector(4, 4, 4), Mat);\n"
        "\t}\n}\n")
    if len(vector_loop.boxes) != 2:
        failures.append("FVector 목록을 도는 범위 for를 못 읽는다 (%d개)"
                        % len(vector_loop.boxes))

    # 저작 메시는 크기 인자가 배율이다. 구운 바운드를 알면 진짜 상자가
    # 나오고 그때부터 판정 대상이 된다. 바운드 원점은 피벗에서 밀린
    # 값이라 중심에 더해야 한다.
    MESH_BOUNDS["SM_SelfTestProp"] = {
        "origin": [0.0, 0.0, 10.0], "extent": [3.0, 4.0, 10.0]}
    try:
        bound_result = scan_text(
            "void AIGTest::Build()\n{\n"
            "\tProp = LoadMesh(TEXT(\"SM_SelfTestProp\"));\n"
            "\tCreateBlock(FVector(0, 0, 0), FVector(200.0f), Mat, "
            "false, Prop);\n}\n")
    finally:
        MESH_BOUNDS.pop("SM_SelfTestProp", None)
    if len(bound_result.boxes) != 1:
        failures.append("저작 메시 상자를 세우지 못한다")
    elif tuple(round(v, 1) for v in bound_result.boxes[0].size) != (12.0, 16.0, 40.0):
        failures.append("구운 바운드에 배율을 곱하지 않는다 (%s)"
                        % (bound_result.boxes[0].size,))
    elif round(bound_result.boxes[0].center[2], 1) != 20.0:
        failures.append("바운드 원점을 중심에 더하지 않는다 (%s)"
                        % (bound_result.boxes[0].center,))
    elif bound_result.boxes[0].mesh != "SM_SelfTestProp":
        failures.append("저작 메시 상자에 이름표가 남지 않는다")

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
    parser.add_argument("--export-layout", metavar="PATH",
                        help="풀린 상자 전부를 JSON으로 적는다 (Blender 배치 렌더용)")
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

    if arguments.export_layout:
        payload = [{
            "source": box.source, "line": box.line, "function": box.function,
            "frame": box.frame, "center": box.center, "size": box.size,
            "location": box.location, "local_size": box.local_size,
            "rotation": box.rotation, "material": box.material, "mesh": box.mesh,
            "shape": box.shape, "collision": box.collision, "note": box.note,
            "exempt": box.exempt,
        } for box in all_boxes]
        with open(arguments.export_layout, "w", encoding="utf-8") as handle:
            json.dump({"boxes": payload}, handle, ensure_ascii=False)
        print(f"layout -> {arguments.export_layout} ({len(payload)} boxes)")

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
        if unresolved:
            # 비율만 적으면 상자가 늘 때 분모도 같이 늘어 못 보는 수가
            # 늘어난 것이 안 보인다. 다른 감사들과 같은 말로 개수를 적는다.
            print(f"  자리를 풀지 못한 상자 {unresolved}건 — 좌표가 트랜스폼"
                  f" 지역 변수나 포인터 삼항에 걸려 계산할 수 없었다")
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
