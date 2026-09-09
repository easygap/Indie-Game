# -*- coding: utf-8 -*-
"""생성 단계에서 파형이 깎이는지 본다.

합성기는 음을 그냥 더한다. `Mixed += Amplitude * Envelope * Waveform` 뒤에
±1.0으로 자르기 때문에, 같은 순간에 겹친 음의 합이 1을 넘으면 그 자리에서
파형이 깎인다. 이건 int16으로 굳기 전에 일어나는 일이라 뒤에서 버스를
줄이든 감쇠를 걸든 되돌릴 수 없다.

인수 기준표의 -1.5 dBTP를 대신하지는 못한다. 최종 출력은 감쇠와 버스 게인과
볼륨 배수와 동시에 우는 다른 소리를 다 거친 뒤에 정해지고, 그건 사람이 재야
안다. 여기서 보는 것은 그 앞의 한 칸 — **파형 하나가 만들어지는 순간에 이미
깎이는지**다. 여기서 깎이면 뒤에서 할 수 있는 게 없다.

두 단계로 본다.

1. 엔벨로프까지 반영한 상한을 1ms 격자로 잡는다. |파형| <= 1이므로 이 값은
   진짜 최대치의 상한이고, 이게 1.0 아래면 안 깎인다는 게 증명된다.
2. 상한이 1.0을 넘는 생성기만 실제 샘플레이트로 돌려서 진짜 최고점을 잰다.
   클램프가 샘플 단위로 걸리니 샘플 격자의 최고점이 곧 정답이다.

여유가 1.5dB도 안 남은 파형은 따로 적는다. 깎이지는 않지만 진폭 하나만
올려도 넘어가는 자리라 어디인지는 보여야 한다. 이건 발견이 아니다.

반복문 안에서 음을 만들거나 값이 안 풀리는 생성기는 다 못 본다. 읽은 음만
재서 통과시키면 안 된다 — 나머지가 조용하다는 근거가 없다. 그래서 통과는 다
읽은 것에만 주고, 못 본 것은 수를 세어 Validate-Project.ps1이 천장으로 잡는다.

거꾸로 읽은 것만으로 이미 1.0을 넘으면 못 본 게 있어도 발견이다. 하한이
클램프를 넘었으니 나머지를 몰라도 결론이 안 바뀐다.
"""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

SOURCES = [
    "Source/IndieGame/Audio/IGToneSequenceSoundWave.cpp",
    "Source/IndieGame/Environment/IGNeighborhoodLifeDirector.cpp",
    "Source/IndieGame/Interaction/IGElevator.cpp",
    "Source/IndieGame/Player/IGStressComponent.cpp",
    "Source/IndieGame/Sequence/IGChapterTwoHumanGateDirector.cpp",
    "Source/IndieGame/Sequence/IGThirdMorningDirector.cpp",
]

SAMPLE_RATE = 48000.0
TWO_PI = 2.0 * math.pi

# 클램프가 걸리는 지점. 여기 닿으면 파형이 실제로 깎인다.
CLIP_LEVEL = 1.0

# 여유가 이만큼도 안 남으면 이름을 적어 둔다. 발견은 아니다.
TIGHT_MARGIN_DB = 1.5
TIGHT_LEVEL = CLIP_LEVEL * 10.0 ** (-TIGHT_MARGIN_DB / 20.0)

# 상한을 훑는 격자. 이보다 짧은 음은 없다.
BOUND_STEP_SECONDS = 0.001

NUMBER = re.compile(r"^[-+]?(?:\d+\.?\d*|\.\d+)f?$")
NOTE = re.compile(
    r"\{\s*([^,{}]+?),\s*([^,{}]+?),\s*([^,{}]+?),\s*([^,{}]+?),"
    r"\s*([^,{}]+?),\s*([^,{}]+?),\s*EIGToneWaveform::(\w+)"
    r"(?:\s*,\s*[^,{}]+?)?\s*\}")
# 파형 하나에 거는 방 울림. 음의 합 × (1 + Mix ÷ (1 − Feedback))까지 커진다.
ROOM_TAIL = re.compile(
    r"ConfigureRoomTail\(\s*([^,]+?),\s*([^,]+?),\s*([^,]+?),\s*([^)]+?)\s*\)")
# 상태가 있어서 파이썬으로 똑같이 못 돌리는 파형. |파형| <= 1은 코드가 보장한다.
BOUNDED_ONLY = {"WhiteNoise", "BandNoise", "Crackle", "Pluck", "Growl"}
SCALAR = re.compile(
    r"^\s*(?:constexpr|const)\s+float\s+(\w+)\s*=\s*"
    r"([-+]?(?:\d+\.?\d*|\.\d+)f?)\s*;", re.MULTILINE)
# 최상위 정의의 시작. 들여쓰기 없이 시작하면서 `::이름(`을 가진 줄이다.
DEFINITION = re.compile(r"^[A-Za-z][^\n]*?::(\w+)\s*\(", re.MULTILINE)
LOOP_HEAD = re.compile(r"^[ \t]+(?:for|while)\s*\(", re.MULTILINE)
# 펼칠 수 있는 반복문 하나. 0에서 시작해 상한까지 하나씩 올라가는 것만 본다.
COUNTED_LOOP = re.compile(
    r"^[ 	]+for\s*\(\s*(?:const\s+)?int32\s+(?P<name>\w+)\s*=\s*0\s*;"
    r"\s*(?P=name)\s*<\s*(?P<bound>[\w.()]+)\s*;"
    r"\s*\+\+(?P=name)\s*\)",
    re.MULTILINE)
INTEGER = re.compile(
    r"^\s*(?:constexpr|const)\s+int32\s+(\w+)\s*=\s*(\d+)\s*;",
    re.MULTILINE)
FLOAT_ARRAY = re.compile(
    r"^\s*(?:constexpr|const|static constexpr)\s+float\s+(\w+)\[\]?\w*\]?"
    r"\s*=\s*\{([^}]*)\}\s*;",
    re.MULTILINE)
INDEXED = re.compile(r"^(\w+)\[([^\]]+)\]$")
# 값이 리터럴이 아니어도 되는 지역 바인딩. 선언 순서대로 풀어야 앞의 것이
# 뒤의 것에 쓰인다.
LOCAL_FLOAT = re.compile(
    r"^[ \t]*(?:constexpr|const)\s+float\s+(\w+)\s*=\s*([^;]+);",
    re.MULTILINE)
ARRAY_COUNT = re.compile(r"^UE_ARRAY_COUNT\(\s*(\w+)\s*\)$")
# 인덱스로 갈라지는 삼항. 인덱스가 정해지면 답도 하나다.
TERNARY = re.compile(
    r"^(?P<left>[^?]+?)\s*(?P<op>==|!=|<=|>=|<|>)\s*(?P<right>[^?]+?)"
    r"\s*\?\s*(?P<yes>[^:]+?)\s*:\s*(?P<no>.+)$")


class Note:
    __slots__ = ("start", "duration", "frequency", "amplitude",
                 "attack", "release", "waveform")

    def __init__(self, start, duration, frequency, amplitude,
                 attack, release, waveform):
        self.start = start
        self.duration = duration
        self.frequency = frequency
        self.amplitude = amplitude
        self.attack = attack
        self.release = release
        self.waveform = waveform


class Generator:
    def __init__(self, source, name):
        self.source = source
        self.name = name
        self.notes: list[Note] = []
        self.unresolved = 0
        self.looped_notes = 0
        # 방 울림의 최대 배율. 없으면 1.
        self.tail_gain = 1.0

    @property
    def site(self) -> str:
        return "%s::%s" % (pathlib.Path(self.source).name, self.name)

    @property
    def blind(self) -> bool:
        return self.looped_notes > 0 or self.unresolved > 0

    @property
    def blind_reason(self) -> str:
        if self.looped_notes and self.unresolved:
            return "반복문 %d, 안 풀린 진폭 %d" % (self.looped_notes,
                                            self.unresolved)
        if self.looped_notes:
            return "반복문 안에서 만든 음 %d" % self.looped_notes
        return "안 풀린 진폭 %d" % self.unresolved


def to_float(token, scalars, arrays=None):
    """리터럴, 이름, 배열 색인, 그리고 그 셋으로 된 덧뺄셈·곱셈 식을 푼다.

    이름은 같은 함수 안의 것이 먼저고, 없으면 파일 위쪽 네임스페이스 상수를
    본다. 타이밍과 주파수를 그쪽에 모아 두는 생성기가 많다. 반복문을 펼칠
    때는 반복 변수가 스칼라 표에 그때의 값으로 들어온다.
    """
    token = token.strip()
    if NUMBER.match(token):
        return float(token.rstrip("f"))
    if token in scalars:
        return scalars[token]
    if arrays:
        counted = ARRAY_COUNT.match(token)
        if counted and counted.group(1) in arrays:
            return float(len(arrays[counted.group(1)]))
        indexed = INDEXED.match(token)
        if indexed and indexed.group(1) in arrays:
            position = to_float(indexed.group(2), scalars, arrays)
            row = arrays[indexed.group(1)]
            if position is None or position != int(position):
                return None
            index = int(position)
            return row[index] if 0 <= index < len(row) else None

    ternary = TERNARY.match(token)
    if ternary:
        left = to_float(ternary.group("left"), scalars, arrays)
        right = to_float(ternary.group("right"), scalars, arrays)
        if left is not None and right is not None:
            operators = {"==": left == right, "!=": left != right,
                         "<=": left <= right, ">=": left >= right,
                         "<": left < right, ">": left > right}
            branch = "yes" if operators[ternary.group("op")] else "no"
            return to_float(ternary.group(branch), scalars, arrays)
        return None

    # 덧뺄셈을 먼저 가른다. 부호로 시작하는 리터럴은 위에서 이미 걸렀다.
    terms = re.split(r"(?<=[\w)f])\s*([+-])\s*", token)
    if len(terms) > 1:
        total = to_float(terms[0], scalars, arrays)
        if total is None:
            return None
        for index in range(1, len(terms) - 1, 2):
            value = to_float(terms[index + 1], scalars, arrays)
            if value is None:
                return None
            total += value if terms[index] == "+" else -value
        return total

    if "*" in token:
        value = 1.0
        for part in token.split("*"):
            resolved = to_float(part, scalars, arrays)
            if resolved is None:
                return None
            value *= resolved
        return value
    return None


def unrollable_loops(body: str, scalars, arrays=None):
    """펼칠 수 있는 반복문. 변수 이름과 횟수와 본문 구간을 돌려준다."""
    found = []
    for head in COUNTED_LOOP.finditer(body):
        bound = to_float(head.group("bound"), scalars, arrays)
        if bound is None or bound != int(bound) or not 0 < bound <= 64:
            continue
        opening = body.find("{", head.end())
        if opening < 0:
            continue
        depth = 0
        for index in range(opening, len(body)):
            if body[index] == "{":
                depth += 1
            elif body[index] == "}":
                depth -= 1
                if depth == 0:
                    inner = body[opening:index]
                    # 안에 또 반복문이 있으면 손대지 않는다. 겹친 것까지
                    # 펼치려다 잘못 펼치면 조용히 틀린 값을 믿게 된다.
                    if not LOOP_HEAD.search(inner):
                        found.append(
                            (head.group("name"), int(bound), opening, index))
                    break
    return found


def loop_spans(body: str):
    """반복문 본문이 차지하는 구간. 여기 들어간 음은 몇 번 울지 모른다."""
    spans = []
    for head in LOOP_HEAD.finditer(body):
        opening = body.find("{", head.end())
        if opening < 0:
            continue
        depth = 0
        for index in range(opening, len(body)):
            if body[index] == "{":
                depth += 1
            elif body[index] == "}":
                depth -= 1
                if depth == 0:
                    spans.append((opening, index))
                    break
    return spans


def parse_source(relative: str) -> list[Generator]:
    text = (ROOT / relative).read_bytes().decode("utf-8-sig")
    text = text.replace("\r\n", "\n")

    starts = [(m.start(), m.group(1)) for m in DEFINITION.finditer(text)]
    # 파일 위쪽 네임스페이스 상수. 함수 안에 같은 이름이 있으면 그쪽이 이긴다.
    head = text[:starts[0][0]] if starts else text
    file_scalars = {m.group(1): float(m.group(2).rstrip("f"))
                    for m in SCALAR.finditer(head)}
    generators = []
    for index, (offset, name) in enumerate(starts):
        end = starts[index + 1][0] if index + 1 < len(starts) else len(text)
        body = text[offset:end]
        if ".Add({" not in body.replace(" ", ""):
            continue
        generator = Generator(relative, name)
        spans = loop_spans(body)
        scalars = dict(file_scalars)
        scalars.update({m.group(1): float(m.group(2).rstrip("f"))
                        for m in SCALAR.finditer(body)})
        scalars.update({m.group(1): float(m.group(2))
                        for m in INTEGER.finditer(body)})
        arrays = {}
        for row in FLOAT_ARRAY.finditer(body):
            values = [to_float(cell, scalars)
                      for cell in row.group(2).split(",") if cell.strip()]
            if values and all(value is not None for value in values):
                arrays[row.group(1)] = values

        # 펼칠 수 있는 반복문은 먼저 펼친다. 남은 반복문 구간만 못 본 것으로
        # 센다.
        unrolled = unrollable_loops(body, scalars, arrays)
        opened = set()
        for variable, count, low, high in unrolled:
            opened.add((low, high))
            inner = body[low:high]
            for step in range(count):
                stepped = dict(scalars)
                stepped[variable] = float(step)
                # 지역 바인딩을 선언 순서대로 푼다. 시작 시각을 인덱스로
                # 계산해 두고 그 이름으로 음을 넣는 생성기가 대부분이다.
                # 자리를 보고 그 음보다 앞선 것만 쓴다 — 본문 전체를 걷어다
                # 쓰면 뒤에 선언된 이름까지 보이고, C++는 그렇게 안 읽는다.
                bindings = [(b.start(), b.group(1), b.group(2))
                            for b in LOCAL_FLOAT.finditer(inner)]
                for match in NOTE.finditer(inner):
                    local = dict(stepped)
                    for at, bound_name, expression in bindings:
                        if at >= match.start():
                            break
                        value = to_float(expression, local, arrays)
                        if value is not None:
                            local[bound_name] = value
                    fields = [to_float(match.group(i), local, arrays)
                              for i in range(1, 7)]
                    if any(field is None for field in fields):
                        generator.unresolved += 1
                        continue
                    generator.notes.append(Note(*fields, match.group(7)))

        for match in NOTE.finditer(body):
            inside_open = any(low <= match.start() <= high
                              for low, high in opened)
            if inside_open:
                continue
            if any(low <= match.start() <= high for low, high in spans):
                generator.looped_notes += 1
                continue
            fields = [to_float(match.group(i), scalars, arrays)
                      for i in range(1, 7)]
            if any(field is None for field in fields):
                generator.unresolved += 1
                continue
            generator.notes.append(Note(*fields, match.group(7)))
        tail = ROOM_TAIL.search(body)
        if tail:
            feedback = to_float(tail.group(2), scalars, arrays)
            mix = to_float(tail.group(4), scalars, arrays)
            if feedback is None or mix is None:
                generator.unresolved += 1
            else:
                feedback = min(max(feedback, 0.0), 0.85)
                mix = min(max(mix, 0.0), 1.0)
                generator.tail_gain = 1.0 + mix / (1.0 - feedback)
        if generator.notes or generator.blind:
            generators.append(generator)
    return generators


def envelope(note: Note, progress: float) -> float:
    attack = min(max(note.attack, 0.001), 0.9)
    if progress <= attack:
        value = progress / attack
        return value * value * (3.0 - 2.0 * value)
    release = (progress - attack) / max(1.0 - attack, 0.001)
    return max(0.0, 1.0 - release) ** max(0.25, note.release)


def hash_to_signed(value: int) -> float:
    h = (value * 2654435761) & 0xFFFFFFFF
    h ^= h >> 16
    h = (h * 2246822519) & 0xFFFFFFFF
    h ^= h >> 13
    h = (h * 3266489917) & 0xFFFFFFFF
    h ^= h >> 16
    return h / 2147483648.0 - 1.0


def waveform(kind: str, frequency: float, seconds: float) -> float:
    cycles = seconds * frequency
    phase = cycles - math.floor(cycles)
    radians = TWO_PI * phase
    if kind == "Sine":
        return math.sin(radians)
    if kind == "SoftSquare":
        return (0.72 * math.sin(radians)
                + 0.24 * math.sin(3.0 * radians)
                + 0.10 * math.sin(5.0 * radians))
    if kind == "Triangle":
        return 2.0 * abs(2.0 * (phase - math.floor(phase + 0.5))) - 1.0
    if kind in BOUNDED_ONLY:
        # 상태 파형은 코드가 ±1로 묶는다. 최악값으로 센다.
        return 1.0
    if kind == "ValueNoise":
        cursor = seconds * max(40.0, frequency)
        cell = int(math.floor(cursor))
        fraction = cursor - math.floor(cursor)
        smooth = fraction * fraction * (3.0 - 2.0 * fraction)
        low = hash_to_signed(cell & 0xFFFFFFFF)
        high = hash_to_signed((cell + 1) & 0xFFFFFFFF)
        return low + (high - low) * smooth
    return 0.0


def pattern_seconds(generator: Generator) -> float:
    return max((n.start + n.duration for n in generator.notes), default=0.0)


def envelope_bound(generator: Generator) -> float:
    """엔벨로프까지 반영한 상한. |파형| <= 1이라 이 값을 넘을 수 없다."""
    span = pattern_seconds(generator)
    if span <= 0.0:
        return 0.0
    worst = 0.0
    for step in range(int(span / BOUND_STEP_SECONDS) + 2):
        moment = step * BOUND_STEP_SECONDS
        total = 0.0
        for note in generator.notes:
            elapsed = moment - note.start
            if elapsed < 0.0 or elapsed >= note.duration or note.duration <= 0.0:
                continue
            total += abs(note.amplitude) * envelope(note, elapsed / note.duration)
        worst = max(worst, total)
    return worst


def true_peak(generator: Generator) -> float:
    """실제 샘플레이트로 돌려서 진짜 최고점을 잰다."""
    span = pattern_seconds(generator)
    if span <= 0.0:
        return 0.0
    worst = 0.0
    for index in range(int(span * SAMPLE_RATE) + 1):
        moment = index / SAMPLE_RATE
        mixed = 0.0
        for note in generator.notes:
            elapsed = moment - note.start
            if elapsed < 0.0 or elapsed >= note.duration or note.duration <= 0.0:
                continue
            mixed += (note.amplitude
                      * envelope(note, elapsed / note.duration)
                      * waveform(note.waveform, note.frequency, elapsed))
        worst = max(worst, abs(mixed))
    return worst


def decibels(value: float) -> float:
    return 20.0 * math.log10(value) if value > 0.0 else -120.0


def measure(generators):
    """생성기마다 최고점과 그 값이 증명인지 실측인지를 돌려준다.

    상한이 클램프 아래면 거기서 끝낸다. 그 위일 때만 돌려 본다 — 상한은
    겹친 음의 마루가 다 같은 순간에 온다고 가정하므로 대개 실제보다 높다.
    """
    results = []
    for generator in generators:
        if not generator.notes:
            results.append((generator, 0.0, "빈 표"))
            continue
        bound = envelope_bound(generator) * generator.tail_gain
        if bound < CLIP_LEVEL:
            results.append((generator, bound, "상한"))
        elif any(n.waveform in BOUNDED_ONLY for n in generator.notes):
            # 상태 파형이 섞이면 실측을 못 한다. 상한이 곧 판정이다.
            results.append((generator, bound, "상한"))
        else:
            results.append((generator, true_peak(generator) * generator.tail_gain, "실측"))
    return results


def report(results, as_json=False):
    # 읽은 것만으로 이미 넘으면 못 본 게 있어도 발견이다.
    clipped = [row for row in results if row[1] >= CLIP_LEVEL]
    # 통과와 「여유가 빠듯하다」는 다 읽은 생성기에만 준다.
    clear = [row for row in results
             if row[1] < CLIP_LEVEL and not row[0].blind]
    blind = [row[0] for row in results
             if row[1] < CLIP_LEVEL and row[0].blind]
    tight = [row for row in clear if row[1] >= TIGHT_LEVEL]
    measured = sum(1 for _, _, how in results if how == "실측")
    if as_json:
        print(json.dumps({
            "generators": len(results),
            "proven": len(clear),
            "measured": measured,
            "blind": len(blind),
            "tight": [{"site": g.site, "peak_dbfs": round(decibels(p), 2)}
                      for g, p, _ in tight],
            "findings": [{"site": g.site, "peak_dbfs": round(decibels(p), 2)}
                         for g, p, _ in clipped],
        }, ensure_ascii=False))
        return len(clipped)

    print("TONE HEADROOM AUDIT  generators=%d proven=%d measured=%d tight=%d "
          "blind=%d findings=%d"
          % (len(results), len(clear), measured, len(tight), len(blind),
             len(clipped)))
    for generator, peak, how in sorted(clipped, key=lambda r: -r[1]):
        print("  %s  %.2f dBFS — 만들어지는 순간에 깎인다 (%s)"
              % (generator.site, decibels(peak), how))
    for generator, peak, _ in sorted(tight, key=lambda r: -r[1]):
        print("  여유 %.2f dB  %s  진폭 하나만 올려도 깎인다"
              % (-decibels(peak), generator.site))
    if blind:
        print("  음을 다 못 읽어서 판정을 못 하는 생성기 %d개" % len(blind))
        for generator in sorted(blind, key=lambda g: g.site)[:4]:
            print("    %s — %s" % (generator.site, generator.blind_reason))
        if len(blind) > 4:
            print("    … 그 밖에 %d개" % (len(blind) - 4))
    return len(clipped)


SELF_TEST_SOURCE = """
namespace SelfTestTiming
{
	constexpr float ChordStart = 0.50f;
	constexpr float NoteA2 = 110.0f;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateSelfTestQuiet(
	UObject* Outer)
{
	TArray<FIGToneNote> Notes;
	Notes.Add({0.00f, 1.00f, 220.0f, 0.30f, 0.10f, 1.0f, EIGToneWaveform::Sine});
	Notes.Add({0.00f, 1.00f, 330.0f, 0.20f, 0.10f, 1.0f, EIGToneWaveform::Sine});
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateSelfTestClipping(
	UObject* Outer)
{
	TArray<FIGToneNote> Notes;
	Notes.Add({0.00f, 1.00f, 220.0f, 0.70f, 0.10f, 1.0f, EIGToneWaveform::Sine});
	Notes.Add({0.00f, 1.00f, 220.0f, 0.70f, 0.10f, 1.0f, EIGToneWaveform::Sine});
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateSelfTestPhased(
	UObject* Outer)
{
	TArray<FIGToneNote> Notes;
	Notes.Add({0.00f, 1.00f, 200.0f, 0.60f, 0.10f, 1.0f, EIGToneWaveform::Sine});
	Notes.Add({0.00f, 1.00f, 200.0f, 0.60f, 0.10f, 1.0f, EIGToneWaveform::Triangle});
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateSelfTestScaled(
	UObject* Outer)
{
	const float Gain = 0.40f;
	TArray<FIGToneNote> Notes;
	Notes.Add({0.00f, 1.00f, 220.0f, 0.25f*Gain, 0.10f, 1.0f, EIGToneWaveform::Sine});
	Notes.Add({ChordStart + 0.20f, 1.00f, NoteA2, 0.10f, 0.10f, 1.0f, EIGToneWaveform::Sine});
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateSelfTestUnrolled(
	UObject* Outer)
{
	constexpr int32 StepCount = 4;
	constexpr float StepStarts[] = {0.00f, 0.25f, 0.50f, 0.75f};
	TArray<FIGToneNote> Notes;
	Notes.Add({0.00f, 0.50f, 300.0f, 0.20f, 0.10f, 1.0f, EIGToneWaveform::Sine});
	for (int32 Index = 0; Index < StepCount; ++Index)
	{
		Notes.Add({StepStarts[Index], 0.20f, 400.0f + Index * 50.0f, 0.10f, 0.10f, 1.0f, EIGToneWaveform::Sine});
	}
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateSelfTestBindings(
	UObject* Outer)
{
	constexpr float Rungs[] = {110.0f, 220.0f, 330.0f};
	TArray<FIGToneNote> Notes;
	for (int32 Step = 0; Step < UE_ARRAY_COUNT(Rungs); ++Step)
	{
		const float Start = 0.10f + Step * 0.25f;
		const float Weight = Step == 1 ? 0.30f : 0.10f;
		Notes.Add({Start, 0.20f, Rungs[Step], Weight, 0.10f, 1.0f, EIGToneWaveform::Sine});
	}
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateSelfTestRebound(
	UObject* Outer)
{
	TArray<FIGToneNote> Notes;
	for (int32 Step = 0; Step < 2; ++Step)
	{
		{
			const float Gap = 0.10f;
			Notes.Add({Gap, 0.05f, 300.0f, 0.10f, 0.10f, 1.0f, EIGToneWaveform::Sine});
		}
		{
			const float Gap = 0.60f;
			Notes.Add({Gap, 0.05f, 300.0f, 0.10f, 0.10f, 1.0f, EIGToneWaveform::Sine});
		}
	}
	return Wave;
}

UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateSelfTestOpaqueLoop(
	UObject* Outer)
{
	TArray<FIGToneNote> Notes;
	for (const FIGToneNote& Source : Borrowed)
	{
		Notes.Add({Source.StartSeconds, 0.20f, 400.0f, 0.10f, 0.10f, 1.0f, EIGToneWaveform::Sine});
	}
	return Wave;
}
"""


def self_test() -> int:
    import tempfile

    failures = []
    with tempfile.TemporaryDirectory() as directory:
        path = pathlib.Path(directory) / "SelfTest.cpp"
        path.write_text(SELF_TEST_SOURCE, encoding="utf-8")
        global ROOT
        original_root = ROOT
        ROOT = path.parent
        try:
            generators = parse_source("SelfTest.cpp")
        finally:
            ROOT = original_root

    by_name = {g.name: g for g in generators}
    expected = {"CreateSelfTestQuiet", "CreateSelfTestClipping",
                "CreateSelfTestPhased", "CreateSelfTestScaled",
                "CreateSelfTestUnrolled", "CreateSelfTestOpaqueLoop",
                "CreateSelfTestBindings", "CreateSelfTestRebound"}
    if set(by_name) != expected:
        print("TONE HEADROOM SELF-TEST FAIL")
        print("  생성기를 %s로 읽었다" % sorted(by_name))
        return 1

    results = {g.name: (peak, how) for g, peak, how in measure(generators)}

    # 조용한 것은 상한만으로 끝나야 한다. 굳이 실측까지 가면 느려진다.
    peak, how = results["CreateSelfTestQuiet"]
    if how != "상한" or peak >= CLIP_LEVEL:
        failures.append("조용한 표가 %s로 %.3f (상한에서 끝나야 한다)" % (how, peak))

    # 같은 음 둘이 같은 위상으로 겹치면 1.4가 되어 실제로 깎인다.
    peak, how = results["CreateSelfTestClipping"]
    if peak < CLIP_LEVEL:
        failures.append("겹쳐서 1.4가 되는 표를 %.3f로 통과시켰다" % peak)

    # 사인과 삼각파는 진폭 합이 1.2지만 마루가 어긋나서 실제로는 안 넘는다.
    # 상한만 보면 걸리고 실측하면 풀린다 — 2단계가 일을 하는지 여기서 본다.
    peak, how = results["CreateSelfTestPhased"]
    if how != "실측":
        failures.append("위상이 어긋난 표가 실측까지 안 갔다 (%s)" % how)
    elif peak >= CLIP_LEVEL:
        failures.append("위상이 어긋난 표를 %.3f로 잘못 걸었다" % peak)

    # 지역 스칼라와 곱을 푼다.
    scaled = by_name["CreateSelfTestScaled"]
    if scaled.unresolved or len(scaled.notes) != 2:
        failures.append("0.25f*Gain이나 파일 상수를 못 풀었다")
    else:
        if abs(scaled.notes[0].amplitude - 0.10) > 1e-6:
            failures.append("0.25f*Gain을 %.4f로 읽었다" % scaled.notes[0].amplitude)
        # 파일 위쪽 네임스페이스 상수와 덧셈.
        if abs(scaled.notes[1].start - 0.70) > 1e-6:
            failures.append("ChordStart + 0.20f를 %.4f로 읽었다"
                            % scaled.notes[1].start)
        if abs(scaled.notes[1].frequency - 110.0) > 1e-6:
            failures.append("NoteA2를 %.2f로 읽었다" % scaled.notes[1].frequency)

    # 횟수가 정해진 반복문은 펼친다. 밖의 하나에 안의 넷이 붙어 다섯이다.
    unrolled = by_name["CreateSelfTestUnrolled"]
    if unrolled.blind:
        failures.append("펼칠 수 있는 반복문을 못 본 것으로 셌다 (%s)"
                        % unrolled.blind_reason)
    if len(unrolled.notes) != 5:
        failures.append("펼친 뒤 음이 %d개다 (다섯이어야 한다)"
                        % len(unrolled.notes))
    else:
        # 펼친 값이 맞는지 본다. 개수만 맞고 값이 틀리면 더 나쁘다 —
        # 틀린 값으로 「안 깎인다」를 증명하게 된다.
        starts = sorted(note.start for note in unrolled.notes)
        if starts != [0.0, 0.0, 0.25, 0.50, 0.75]:
            failures.append("배열 색인을 %s로 펼쳤다" % starts)
        pitches = sorted(note.frequency for note in unrolled.notes)
        if pitches != [300.0, 400.0, 450.0, 500.0, 550.0]:
            failures.append("반복 변수가 든 식을 %s로 펼쳤다" % pitches)

    # 지역 바인딩과 배열 길이와 인덱스 삼항. 셋 다 값까지 맞아야 한다 —
    # 개수만 맞고 값이 틀리면 틀린 값으로 「안 깎인다」를 증명하게 된다.
    bound = by_name["CreateSelfTestBindings"]
    if bound.blind:
        failures.append("UE_ARRAY_COUNT 반복문을 못 본 것으로 셌다 (%s)"
                        % bound.blind_reason)
    elif len(bound.notes) != 3:
        failures.append("배열 길이를 %d번으로 읽었다" % len(bound.notes))
    else:
        starts = sorted(note.start for note in bound.notes)
        if [round(value, 6) for value in starts] != [0.10, 0.35, 0.60]:
            failures.append("지역 바인딩을 %s로 풀었다" % starts)
        weights = sorted(note.amplitude for note in bound.notes)
        if [round(value, 6) for value in weights] != [0.10, 0.10, 0.30]:
            failures.append("인덱스 삼항을 %s로 풀었다" % weights)

    # 같은 이름을 두 블록이 각자 쓰면 음마다 자기 앞의 값을 써야 한다.
    # 자리를 안 보면 뒤엣값이 앞의 음까지 덮는다.
    rebound = by_name["CreateSelfTestRebound"]
    rebound_starts = sorted(round(note.start, 6) for note in rebound.notes)
    if rebound_starts != [0.10, 0.10, 0.60, 0.60]:
        failures.append("같은 이름을 다시 쓴 자리를 %s로 풀었다"
                        % rebound_starts)

    # 범위 기반 반복문은 몇 번 도는지 모른다. 못 본 것으로 세야 한다.
    opaque = by_name["CreateSelfTestOpaqueLoop"]
    if not opaque.blind:
        failures.append("몇 번 도는지 모르는 반복문을 다 본 것처럼 셌다")

    # 못 읽은 음이 있으면 조용하더라도 통과 쪽에 세우면 안 된다. 나머지가
    # 조용하다는 근거가 없다.
    rows = measure(generators)
    proven = {row[0].name for row in rows
              if row[1] < CLIP_LEVEL and not row[0].blind}
    if "CreateSelfTestOpaqueLoop" in proven:
        failures.append("반복문 때문에 못 읽은 생성기를 통과로 셌다")
    if "CreateSelfTestQuiet" not in proven:
        failures.append("다 읽은 조용한 생성기를 통과로 안 셌다")

    if failures:
        print("TONE HEADROOM SELF-TEST FAIL")
        print("\n".join("  " + f for f in failures))
        return 1
    print("TONE HEADROOM SELF-TEST PASS  quiet=상한 clipping=걸림 "
          "phased=실측통과 scaled=풀림 unrolled=값까지맞음 "
          "bindings=값까지맞음 rebound=자리별로맞음 opaque_loop=못봄")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    generators = []
    for relative in SOURCES:
        generators.extend(parse_source(relative))
    results = measure(generators)
    findings = report(results, args.json)
    if not args.json and findings == 0:
        proven = sum(1 for row in results
                     if row[1] < CLIP_LEVEL and not row[0].blind)
        print()
        print("none of the %d fully readable generators clips before the mix"
              % proven)
    return 1 if (args.check and findings) else 0


if __name__ == "__main__":
    raise SystemExit(main())
