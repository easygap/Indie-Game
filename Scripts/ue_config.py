"""Parse Unreal's struct text out of an ini, the way ImportText reads it.

A regex is enough to *find* `+PrimaryAssetTypesToScan=(...)`. It is not enough
to know the line is right, and being right matters more here than usual: an
Asset Manager rule is the only thing keeping twelve HUD textures in the pak,
Unreal reports a rule it cannot parse as a log line nobody reads, and the
symptom is a blank draw in a packaged build. A misspelled key, a stray
parenthesis or a quote in the wrong place all fail exactly that way.

So the rules are parsed rather than matched. The grammar Unreal's
`FStructProperty::ImportText` accepts, in the subset an ini rule uses:

    struct := '(' [ pair { ',' pair } ] ')'
    pair   := identifier '=' value
    array  := '(' [ value { ',' value } ] ')'
    value  := struct | array | '"' ... '"' | bare-token | <empty>

Struct and array are spelled the same; what tells them apart is whether the
first element is `identifier=`. An empty value (`SpecificAssets=,`) is how an
empty array is written, which is why it cannot simply be rejected.

Nothing here knows anything about the cook. It answers "is this the struct it
claims to be", and `check_cook_references.py` decides what that means.
"""

from __future__ import annotations


class ConfigSyntaxError(ValueError):
    """The struct text is not something Unreal would parse."""


_IDENTIFIER_START = set(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_")
_IDENTIFIER_BODY = _IDENTIFIER_START | set("0123456789")


class _Reader:
    def __init__(self, text: str):
        self.text = text
        self.at = 0

    def peek(self) -> str:
        return self.text[self.at] if self.at < len(self.text) else ""

    def take(self) -> str:
        char = self.peek()
        if not char:
            raise ConfigSyntaxError("struct text ended early")
        self.at += 1
        return char

    def expect(self, char: str) -> None:
        if self.peek() != char:
            raise ConfigSyntaxError(
                f"expected {char!r} at offset {self.at}, "
                f"found {self.peek()!r}"
            )
        self.at += 1

    def skip_space(self) -> None:
        while self.peek() in (" ", "\t"):
            self.at += 1

    # --- productions ------------------------------------------------------
    def identifier(self) -> str:
        if self.peek() not in _IDENTIFIER_START:
            raise ConfigSyntaxError(
                f"expected a field name at offset {self.at}")
        start = self.at
        while self.peek() in _IDENTIFIER_BODY:
            self.at += 1
        return self.text[start:self.at]

    def quoted(self) -> str:
        self.expect('"')
        out = []
        while True:
            char = self.take()
            if char == "\\":
                out.append(self.take())
            elif char == '"':
                return "".join(out)
            else:
                out.append(char)

    def bare(self) -> str:
        start = self.at
        depth = 0
        while True:
            char = self.peek()
            if not char:
                break
            if char in ",)" and depth == 0:
                break
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            self.at += 1
        return self.text[start:self.at].strip()

    def _is_struct_ahead(self) -> bool:
        """`(Key=` is a struct; anything else inside `(` is an array."""
        probe = self.at + 1  # past the '('
        while probe < len(self.text) and self.text[probe] in (" ", "\t"):
            probe += 1
        if probe >= len(self.text) or self.text[probe] not in _IDENTIFIER_START:
            return False
        while probe < len(self.text) and self.text[probe] in _IDENTIFIER_BODY:
            probe += 1
        while probe < len(self.text) and self.text[probe] in (" ", "\t"):
            probe += 1
        return probe < len(self.text) and self.text[probe] == "="

    def value(self):
        self.skip_space()
        char = self.peek()
        if char == "(":
            return self.struct() if self._is_struct_ahead() else self.array()
        if char == '"':
            return self.quoted()
        return self.bare()

    def array(self) -> list:
        self.expect("(")
        self.skip_space()
        items: list = []
        if self.peek() == ")":
            self.at += 1
            return items
        while True:
            items.append(self.value())
            self.skip_space()
            if self.peek() == ",":
                self.at += 1
                self.skip_space()
                # A trailing comma before the close is legal in Unreal's text.
                if self.peek() == ")":
                    self.at += 1
                    return items
                continue
            self.expect(")")
            return items

    def struct(self) -> dict:
        self.expect("(")
        self.skip_space()
        fields: dict = {}
        if self.peek() == ")":
            self.at += 1
            return fields
        while True:
            name = self.identifier()
            self.skip_space()
            self.expect("=")
            if name in fields:
                raise ConfigSyntaxError(f"field {name} appears twice")
            fields[name] = self.value()
            self.skip_space()
            if self.peek() == ",":
                self.at += 1
                self.skip_space()
                if self.peek() == ")":
                    self.at += 1
                    return fields
                continue
            self.expect(")")
            return fields


def parse_struct(text: str) -> dict:
    """One `(Key=Value,...)` literal. Raises if anything is left over."""
    reader = _Reader(text.strip())
    reader.skip_space()
    if reader.peek() != "(":
        raise ConfigSyntaxError("struct text does not start with '('")
    if not reader._is_struct_ahead():
        raise ConfigSyntaxError("struct text has no Key=Value field")
    value = reader.struct()
    reader.skip_space()
    if reader.at != len(reader.text):
        raise ConfigSyntaxError(
            f"trailing text after the struct: {reader.text[reader.at:]!r}")
    return value


def as_array(value) -> list:
    """Unreal writes an empty array as nothing at all, not as `()`."""
    if value is None or value == "":
        return []
    if isinstance(value, list):
        return value
    raise ConfigSyntaxError(f"expected an array, found {value!r}")


def as_bool(value) -> bool:
    if isinstance(value, str) and value.lower() in ("true", "false"):
        return value.lower() == "true"
    raise ConfigSyntaxError(f"expected True or False, found {value!r}")


def as_int(value) -> int:
    try:
        return int(str(value))
    except ValueError:
        raise ConfigSyntaxError(f"expected an integer, found {value!r}")


def ini_entries(text: str, key: str):
    """Every `key=`, `+key=` or `.key=` assignment, in file order.

    Yields (line number, right-hand side). Unreal reads the whole value from
    one line, so this does not join continuations.
    """
    for number, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith((";", "#")):
            continue
        for prefix in ("+", ".", "!", "-", ""):
            head = f"{prefix}{key}"
            if stripped.startswith(head):
                rest = stripped[len(head):].lstrip()
                if rest.startswith("="):
                    yield number, rest[1:].strip()
                break
