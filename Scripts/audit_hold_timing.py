# -*- coding: utf-8 -*-
"""§18.7 홀드 완료 시간 편차를 재 본다.

「60fps·30fps 양쪽에서 홀드 완료 시간 편차 ±3% 이내」는 눈으로 읽어서는
지킬 수 없는 줄이다. 홀드는 벽시계 시간으로 흐르고 완료 판정만 프레임에서
일어나므로, 남는 오차는 프레임 하나만큼이다. 그게 문서의 폭 안에 들어오는지는
짧은 홀드일수록 아슬아슬해진다 — 재 보는 수밖에 없다.

여기서는 `UIGInteractionComponent`의 판정 규칙을 그대로 옮긴다. 시작 시각이
프레임 경계 어디에 떨어지느냐를 잘게 훑어 가장 나쁜 경우를 찾는다.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
STORY = ROOT / "Docs/STORY_BIBLE_MISSING_FLOOR.md"

# 시작 위상을 이만큼 잘게 나눠 본다. 프레임 경계에 딱 맞춰 시작하는 경우와
# 한 프레임을 거의 다 흘려보내고 시작하는 경우가 양 끝이다.
PHASE_SAMPLES = 512


def read_tolerance() -> float:
    """문서에서 허용 편차를 읽는다. 숫자를 두 곳에 적지 않는다."""
    text = STORY.read_text(encoding="utf-8-sig")
    match = re.search(
        r"60fps·30fps 양쪽에서 홀드 완료 시간 편차 ±(?P<percent>[0-9.]+)%", text)
    if not match:
        raise SystemExit("§18.7의 홀드 편차 줄을 읽지 못했다")
    return float(match.group("percent")) / 100.0


def read_authored_holds() -> dict[str, float]:
    """코드에 실제로 적혀 있는 홀드 시간을 모은다."""
    holds: dict[str, float] = {}

    door = (ROOT / "Source/IndieGame/Interaction/IGSwingDoor.h").read_text(
        encoding="utf-8-sig")
    match = re.search(r"float QuietOpenHoldSeconds = ([0-9.]+)f;", door)
    if match:
        holds["문 조용히 열기"] = float(match.group(1))

    night_four = (
        ROOT / "Source/IndieGame/Entity/IGMissingFloorNightFourDirector.cpp"
    ).read_text(encoding="utf-8-sig")
    for prompt, label in (
        ("WallBreakPrompt", "망치 스윙 차징"),
        ("EndingAPrompt", "엔딩 A 조율 렌치"),
        ("EndingBPrompt", "엔딩 B 곁에 앉기"),
    ):
        match = re.search(
            re.escape(prompt)
            + r"[\s\S]{0,400}?EIGMissingFloorSource::None,\s*\r?\n"
            r"(?:\s*//[^\r\n]*\r?\n)*\s*([0-9.]+)f,",
            night_four,
        )
        if match:
            holds[label] = float(match.group(1))

    if not holds:
        raise SystemExit("코드에서 홀드 시간을 하나도 찾지 못했다")
    return holds


def completion_time(duration: float, frame_seconds: float, start_phase: float) -> float:
    """한 번의 홀드가 실제로 끝나는 시각.

    `GetHoldProgress`는 `GetTimeSeconds() - ActiveStartTime`으로 흐르고,
    `UpdateActiveInteraction`은 매 프레임 그 값이 1을 넘었는지 본다. 그러니
    완료는 조건을 만족한 첫 프레임의 경과 시각이다.

    start_phase는 홀드가 시작된 시점이 직전 프레임에서 얼마나 지난 뒤인가다.
    """
    elapsed = frame_seconds - start_phase
    for _ in range(100000):
        if elapsed >= duration:
            return elapsed
        elapsed += frame_seconds
    raise SystemExit("홀드가 끝나지 않는다 — 판정 규칙을 확인해라")


def frame_counted_completion(duration: float, frame_seconds: float) -> float:
    """프레임을 세는 잘못된 구현. 자체 검사용이다."""
    frames = int(duration * 60.0)
    return frames * frame_seconds


def worst_deviation(duration: float) -> float:
    """60fps와 30fps에서 나온 완료 시각의 최대 상대 차이."""
    worst = 0.0
    for index in range(PHASE_SAMPLES):
        phase = (index + 0.5) / PHASE_SAMPLES
        at_sixty = completion_time(duration, 1.0 / 60.0, phase / 60.0)
        at_thirty = completion_time(duration, 1.0 / 30.0, phase / 30.0)
        worst = max(worst, abs(at_sixty - at_thirty) / duration)
    return worst


def self_test(tolerance: float) -> None:
    """검사가 나쁜 경우를 실제로 잡는지 먼저 확인한다.

    통과만 보고 끝내면 「검사가 살아 있다」와 「검사가 죽었다」를 구분할 수 없다.
    """
    # 1. 프레임을 세는 구현은 30fps에서 두 배 오래 걸린다.
    drift = abs(
        frame_counted_completion(0.8, 1.0 / 60.0)
        - frame_counted_completion(0.8, 1.0 / 30.0)) / 0.8
    if drift <= tolerance:
        raise SystemExit("자체 검사 실패: 프레임을 세는 구현을 잡지 못한다")

    # 2. 남는 오차는 프레임 하나다. 홀드가 짧아질수록 그 하나가 커진다 —
    #    0.4초짜리를 누가 적어 넣으면 폭을 넘긴다. 그걸 잡는지 본다.
    short = worst_deviation(0.4)
    if short <= tolerance:
        raise SystemExit("자체 검사 실패: 너무 짧은 홀드를 잡지 못한다")

    print(
        "HOLD TIMING SELF-TEST PASS  "
        f"frame_counted={drift * 100:.0f}%  short_hold_0.40s={short * 100:.2f}%")


def main() -> int:
    # 나머지 감사와 같은 손잡이를 쓴다. 자체 검사만 따로 돌릴 수 있어야
    # 「검사가 살아 있는가」와 「코드가 맞는가」를 갈라서 볼 수 있다.
    wants_self_test = "--self-test" in sys.argv
    wants_check = "--check" in sys.argv or not wants_self_test

    tolerance = read_tolerance()
    if wants_self_test or wants_check:
        self_test(tolerance)
    if not wants_check:
        return 0

    holds = read_authored_holds()
    findings = []
    worst_overall = 0.0
    for label, duration in sorted(holds.items(), key=lambda item: item[1]):
        deviation = worst_deviation(duration)
        worst_overall = max(worst_overall, deviation)
        status = "ok" if deviation <= tolerance else "OVER"
        print(
            f"  {label:<20} {duration:.2f}s  최대 편차 {deviation * 100:5.2f}%  {status}")
        if deviation > tolerance:
            findings.append(
                f"{label} {duration:.2f}s — {deviation * 100:.2f}% > "
                f"{tolerance * 100:.0f}%")

    print(
        f"HOLD TIMING AUDIT  holds={len(holds)} "
        f"worst={worst_overall * 100:.2f}% tolerance={tolerance * 100:.0f}% "
        f"findings={len(findings)}")
    if findings:
        for finding in findings:
            print("  " + finding)
        return 1
    print()
    print("every authored hold completes within the §18.7 window at 30 and 60 fps")
    return 0


if __name__ == "__main__":
    sys.exit(main())
