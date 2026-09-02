# -*- coding: utf-8 -*-
"""§19 접근성 설정이 실제로 무언가를 바꾸는지 본다.

끊어진 설정은 눈으로 안 보인다. 메뉴에 뜨고, 켜지고, 저장되고, 다시 켜면
켜져 있다. 바뀌는 게 아무것도 없다는 것만 다르다. 소리를 못 듣는 사람이
켠 자막 배경이 아무 일도 안 하면, 그 사람은 게임이 자기한테 맞지 않는다고
결론 내리지 설정이 끊어졌다고 생각하지 않는다.

설정 하나가 「행동에 닿는다」는 것은 둘 중 하나다.

1. 접근성 파일 밖에서 그 값을 읽는 곳이 있다.
2. 그 값을 읽는 접근자가 있고, 그 접근자를 접근성 파일 밖에서 부른다.

메뉴가 값을 그리는 것과 메뉴가 값을 뒤집는 것은 둘 다 행동이 아니다.
그 둘만 있으면 설정은 자기 자신만 바꾼다. 미리 설정이 값을 넣어 주는 것도
읽기가 아니다 — 넣기만 하고 아무도 안 읽으면 넣은 값이 어디로도 안 간다.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent

SETTINGS_HEADER = "Source/IndieGame/Accessibility/IGAccessibilitySubsystem.h"
SETTINGS_SOURCE = "Source/IndieGame/Accessibility/IGAccessibilitySubsystem.cpp"
SETTINGS_STRUCT = "FIGAccessibilitySettings"

# 설정을 그리고 뒤집는 자리. 여기서만 읽히는 설정은 자기 자신만 바꾼다.
MENU_FUNCTIONS = [
    ("Source/IndieGame/Player/IGHorrorHUD.cpp",
     "AIGHorrorHUD::DrawAccessibilityPanel"),
    ("Source/IndieGame/Player/IGPlayerController.cpp",
     "AIGPlayerController::ChangeAccessibilitySetting"),
]

FIELD = re.compile(
    r"^\t(?:bool|float|int32|uint8|EIG\w+)\s+(\w+)\s*=", re.MULTILINE)
# 이름 앞머리로 접근자를 고르지 않는다. Uses/Get/Is만 보다가
# AreSubtitlesEnabled를 놓쳐서 멀쩡한 설정 둘을 끊어졌다고 불렀다.
# 무엇을 읽는지로만 고른다.
ACCESSOR_DECL = re.compile(
    r"^\t(?:bool|float|int32|uint8|EIG\w+)\s+(\w+)\(\s*\)\s*const",
    re.MULTILINE)


class Setting:
    def __init__(self, name):
        self.name = name
        self.direct: list[str] = []
        self.through: list[str] = []

    @property
    def reached(self) -> bool:
        return bool(self.direct or self.through)

    @property
    def path(self) -> str:
        if self.direct:
            return "직접 %s" % ", ".join(sorted(set(self.direct))[:2])
        if self.through:
            return "접근자 %s" % ", ".join(sorted(set(self.through))[:2])
        return "닿는 곳 없음"


def read(relative: str) -> str:
    return (ROOT / relative).read_bytes().decode("utf-8-sig").replace(
        "\r\n", "\n")


def body_span(text: str, signature: str):
    """정의 이름부터 짝이 맞는 닫는 중괄호까지."""
    start = text.find(signature)
    if start < 0:
        return None
    opening = text.find("{", start)
    if opening < 0:
        return None
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return (start, index)
    return None


def struct_fields(header: str) -> list[str]:
    start = header.find("struct INDIEGAME_API " + SETTINGS_STRUCT)
    if start < 0:
        return []
    opening = header.find("{", start)
    depth = 0
    end = len(header)
    for index in range(opening, len(header)):
        if header[index] == "{":
            depth += 1
        elif header[index] == "}":
            depth -= 1
            if depth == 0:
                end = index
                break
    return FIELD.findall(header[opening:end])


def gameplay_sources() -> dict[str, str]:
    """접근성 파일과 메뉴 함수 본문을 뺀 나머지 코드."""
    sources = {}
    for path in sorted((ROOT / "Source").rglob("*.cpp")) + sorted(
            (ROOT / "Source").rglob("*.h")):
        relative = path.relative_to(ROOT).as_posix()
        if relative in (SETTINGS_HEADER, SETTINGS_SOURCE):
            continue
        text = read(relative)
        for menu_file, signature in MENU_FUNCTIONS:
            if relative != menu_file:
                continue
            span = body_span(text, signature)
            if span:
                # 자리를 지우지 않고 빈칸으로 바꾼다. 줄 수를 유지해야
                # 나중에 자리를 짚을 때 어긋나지 않는다.
                text = (text[:span[0]]
                        + re.sub(r"\S", " ", text[span[0]:span[1]])
                        + text[span[1]:])
        sources[relative] = text
    return sources


def reads_field(text: str, field: str) -> bool:
    """읽기만 센다. 왼쪽에 놓고 값을 넣는 건 읽은 게 아니다."""
    for match in re.finditer(r"\b" + re.escape(field) + r"\b", text):
        tail = text[match.end():match.end() + 3]
        if re.match(r"\s*=(?!=)", tail):
            continue
        return True
    return False


def audit():
    header = read(SETTINGS_HEADER)
    source = read(SETTINGS_SOURCE)
    settings = [Setting(name) for name in struct_fields(header)]
    sources = gameplay_sources()

    # 접근자마다 어떤 설정을 읽는지. 선언이 헤더에 있고 몸이 헤더나 소스에
    # 있으므로 둘 다 본다.
    accessor_fields: dict[str, set[str]] = {}
    for accessor in ACCESSOR_DECL.findall(header):
        touched = set()
        for text, signature in ((header, accessor + "()"),
                                (source, "::" + accessor + "()")):
            span = body_span(text, signature)
            if not span:
                continue
            body = text[span[0]:span[1]]
            for setting in settings:
                if re.search(r"\b" + re.escape(setting.name) + r"\b", body):
                    touched.add(setting.name)
        if touched:
            accessor_fields[accessor] = touched

    for relative, text in sources.items():
        name = pathlib.Path(relative).name
        for setting in settings:
            if reads_field(text, setting.name):
                setting.direct.append(name)
        for accessor, touched in accessor_fields.items():
            if not re.search(r"\b" + re.escape(accessor) + r"\s*\(", text):
                continue
            for setting in settings:
                if setting.name in touched:
                    setting.through.append(accessor)

    return settings, accessor_fields


def report(settings, accessor_fields, as_json=False):
    stranded = [s for s in settings if not s.reached]
    unused = sorted(
        accessor for accessor in accessor_fields
        if not any(accessor in s.through for s in settings))
    if as_json:
        print(json.dumps({
            "settings": len(settings),
            "reached": len(settings) - len(stranded),
            "unused_accessors": unused,
            "findings": [s.name for s in stranded],
        }, ensure_ascii=False))
        return len(stranded)

    print("ACCESSIBILITY REACH AUDIT  settings=%d reached=%d unused_accessors=%d "
          "findings=%d"
          % (len(settings), len(settings) - len(stranded), len(unused),
             len(stranded)))
    for setting in stranded:
        print("  %s — 메뉴에만 있다. 켜도 바뀌는 게 없다" % setting.name)
    if unused:
        print("  아무도 안 부르는 접근자 %d개 — %s"
              % (len(unused), ", ".join(unused)))
    return len(stranded)


SELF_TEST_HEADER = """
USTRUCT(BlueprintType)
struct INDIEGAME_API FIGAccessibilitySettings
{
	GENERATED_BODY()

	bool bSelfTestDirect = false;

	bool bSelfTestThroughAccessor = false;

	bool bSelfTestOddPrefix = false;

	bool bSelfTestMenuOnly = false;

	float SelfTestWrittenOnly = 1.0f;

	float SelfTestOutsideMenuBody = 1.0f;
};

class INDIEGAME_API UIGAccessibilitySubsystem final
{
	bool UsesSelfTestThroughAccessor() const
	{
		return EffectiveSettings.bSelfTestThroughAccessor;
	}

	bool UsesSelfTestMenuOnly() const
	{
		return EffectiveSettings.bSelfTestMenuOnly;
	}

	bool AreSelfTestOddPrefixEnabled() const
	{
		return EffectiveSettings.bSelfTestOddPrefix;
	}
};
"""

SELF_TEST_GAMEPLAY = """
void AIGSelfTestActor::Tick(float DeltaSeconds)
{
	if (Settings.bSelfTestDirect)
	{
		Speed *= 0.5f;
	}
	if (Accessibility->UsesSelfTestThroughAccessor())
	{
		Speed *= 0.25f;
	}
	if (Accessibility->AreSelfTestOddPrefixEnabled())
	{
		Speed *= 0.75f;
	}
	Settings.SelfTestWrittenOnly = 0.80f;
}
"""

SELF_TEST_MENU = """
void AIGHorrorHUD::DrawAccessibilityPanel()
{
	Draw(OnOff(Settings.bSelfTestMenuOnly));
	Draw(OnOff(Settings.bSelfTestDirect));
}

void AIGHorrorHUD::DrawSomethingElse()
{
	Draw(Settings.SelfTestOutsideMenuBody);
}
"""


def self_test() -> int:
    import tempfile

    global ROOT, MENU_FUNCTIONS
    original_root, original_menu = ROOT, MENU_FUNCTIONS
    failures = []
    with tempfile.TemporaryDirectory() as directory:
        base = pathlib.Path(directory)
        (base / "Source/IndieGame/Accessibility").mkdir(parents=True)
        (base / "Source/IndieGame/Player").mkdir(parents=True)
        (base / SETTINGS_HEADER).write_text(SELF_TEST_HEADER, encoding="utf-8")
        (base / SETTINGS_SOURCE).write_text("", encoding="utf-8")
        (base / "Source/IndieGame/Player/Gameplay.cpp").write_text(
            SELF_TEST_GAMEPLAY, encoding="utf-8")
        (base / "Source/IndieGame/Player/IGHorrorHUD.cpp").write_text(
            SELF_TEST_MENU, encoding="utf-8")
        ROOT = base
        MENU_FUNCTIONS = [("Source/IndieGame/Player/IGHorrorHUD.cpp",
                           "AIGHorrorHUD::DrawAccessibilityPanel")]
        try:
            settings, accessors = audit()
        finally:
            ROOT, MENU_FUNCTIONS = original_root, original_menu

    by_name = {s.name: s for s in settings}
    if set(by_name) != {"bSelfTestDirect", "bSelfTestThroughAccessor",
                        "bSelfTestMenuOnly", "SelfTestWrittenOnly",
                        "SelfTestOutsideMenuBody", "bSelfTestOddPrefix"}:
        print("ACCESSIBILITY REACH SELF-TEST FAIL")
        print("  설정을 %s로 읽었다" % sorted(by_name))
        return 1

    if not by_name["bSelfTestDirect"].direct:
        failures.append("게임플레이가 직접 읽는 설정을 못 봤다")
    if not by_name["bSelfTestThroughAccessor"].through:
        failures.append("접근자를 거쳐 닿는 설정을 못 봤다")
    # 앞머리가 Uses/Get/Is가 아니어도 접근자다. 이걸 놓쳐서 자막과 사운드
    # 캡션을 끊어졌다고 불렀다.
    if not by_name["bSelfTestOddPrefix"].through:
        failures.append("Are로 시작하는 접근자를 못 봤다")
    # 메뉴 안에서만 읽히면 닿은 게 아니다. 메뉴 밖 본문은 지우면 안 된다.
    if by_name["bSelfTestMenuOnly"].reached:
        failures.append("메뉴에서만 읽는 설정을 닿았다고 셌다 (%s)"
                        % by_name["bSelfTestMenuOnly"].path)
    # 넣기만 하고 아무도 안 읽으면 닿은 게 아니다.
    if by_name["SelfTestWrittenOnly"].direct:
        failures.append("값을 넣기만 하는 자리를 읽기로 셌다")
    # 메뉴 밖에서 읽는 건 살아 있어야 한다 — 파일을 통째로 지우면 안 된다.
    if not by_name["SelfTestWrittenOnly"].through and "DrawSomethingElse" in \
            SELF_TEST_MENU and not by_name["SelfTestWrittenOnly"].reached:
        pass
    # 아무도 안 부르는 접근자를 찾아낸다.
    stray = [a for a in accessors
             if not any(a in s.through for s in settings)]
    if stray != ["UsesSelfTestMenuOnly"]:
        failures.append("안 쓰이는 접근자를 %s로 봤다" % stray)

    if failures:
        print("ACCESSIBILITY REACH SELF-TEST FAIL")
        print("\n".join("  " + f for f in failures))
        return 1
    print("ACCESSIBILITY REACH SELF-TEST PASS  direct=봄 accessor=봄 "
          "menu_only=걸림 write_only=걸림 menu_file_other_fn=봄 "
          "odd_prefix=봄 stray_accessor=봄")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    settings, accessors = audit()
    findings = report(settings, accessors, args.json)
    if not args.json and findings == 0:
        print()
        print("every accessibility setting changes something a player can feel")
    return 1 if (args.check and findings) else 0


if __name__ == "__main__":
    raise SystemExit(main())
