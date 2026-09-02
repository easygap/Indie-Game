# -*- coding: utf-8 -*-
"""같은 사실을 두 군데에 적어 두는 것을 잡는다.

세 번 겪고 나서 붙였다.

  1. 냉장고 험이 냉장고 좌표를 다시 적어, 냉장고를 옮기면 험만 남았다
  2. 4층 슬래브 높이가 다섯 파일에 있었다. 밤 2 주석은 그걸 규칙처럼
     적어 두기까지 했다
  3. 로비 CCTV 화면 치수가 씬과 채널 5에 각각 있었다

셋 다 컴파일은 되고 화면도 뜬다. 한쪽만 고쳐진 상태가 화면에서 「고장」으로
보이지 않는 것이 이 종류의 성질이다.

두 가지를 본다.

  **아직 안 묶인 것** — 이름과 값이 모두 같은 선언이 두 파일 이상에 있으면
  같은 사실일 가능성이 높다. 값이 우연히 겹치는 것까지 잡으면 쓸 수 없게
  되므로 이름까지 같을 때만 센다. 그러고도 정당하게 따로인 것은 아래
  `REVIEWED`에 이유와 함께 적는다.

  **묶은 뒤에 다시 풀리는 것** — 한 군데로 모으고 나면 위 검사는 눈이 먼다.
  이름이 하나뿐이라 「두 파일에 같은 이름」이 성립하지 않기 때문이다. 그래서
  이미 주인을 정한 이름은 `DERIVED_ONLY`에 적어 두고, 주인 밖에서 숫자로
  다시 적히는지 따로 본다.

좌표 리터럴은 세지 않는다. FVector의 앞 두 값은 자리이기도 하고 크기이기도
해서, (8, 8) 같은 상자 반칸이 파일마다 겹치는 것을 걸러 낼 방법이 없다.
"""
import argparse
import collections
import pathlib
import re
import sys

PROJECT_ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE_ROOT = PROJECT_ROOT / "Source" / "IndieGame"

DECLARATION = re.compile(
    r"(?:static\s+)?constexpr\s+(?:float|double|int32|int64|uint8|uint32)\s+"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?P<value>[^;]+);")
NUMERIC = re.compile(r"^-?[0-9][0-9]*(?:\.[0-9]+)?f?$")

# 이름을 붙인 자리와, 다른 데 적힌 같은 숫자. 냉장고 험·벽 파괴 지점·공용
# 라이저가 전부 이 모양이었다.
COMPONENTS = r"\s*(?P<x>-?[0-9.]+)f?\s*,\s*(?P<y>-?[0-9.]+)f?\s*,\s*(?P<z>-?[0-9.]+)f?\s*"
VECTOR_DECLARATION = re.compile(
    r"const\s+FVector\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\(" + COMPONENTS + r"\)")
VECTOR_LITERAL = re.compile(r"FVector\s*\(" + COMPONENTS + r"\)")
# 자리를 뜻하는 이름만 본다. 방향과 크기까지 세면 상자 반칸이 파일마다
# 겹쳐서 못 쓰게 된다.
PLACE_SUFFIXES = ("Location", "Point", "Origin", "Center", "Spot")

# 이름과 값이 같지만 서로 다른 사실인 것들. 하나로 묶으면 오히려 틀린다.
REVIEWED = {
    "SampleRateHz":
        "합성기마다 자기 포맷을 든다. 한 곳으로 묶으면 파형 생성기가"
        " 서로의 샘플레이트에 묶인다",
    "SnapshotSchemaVersion":
        "없는 층과 REBIRTH는 스냅샷 구조체가 다르다. 우연히 둘 다 3일 뿐이라"
        " 묶으면 한쪽을 올릴 때 다른 쪽 세이브가 잘못 읽힌다",
    "ReturnPollSeconds":
        "밤 2와 밤 3이 각자 자기 비트의 폴링 간격을 든다. 같은 값인 것은"
        " 우연이고, 한쪽 비트를 조이면 다른 쪽은 그대로여야 한다",
    "TickIntervalSeconds":
        "자비 디렉터와 밤 페이즈 디렉터의 틱 간격. 위와 같은 이유로 따로 둔다",
}

# 주인을 정한 이름. 주인만 숫자를 들고 나머지는 받아 쓴다.
#   owner  — 숫자를 들어도 되는 파일. None이면 어디서도 숫자로 적지 않는다
#   literal— 주인이 들어야 하는 값. 층을 정말 옮기려면 여기도 같이 고친다
DERIVED_ONLY = {
    "FourthFloorZ": {
        "expression": "AIGPrologueWorldScene::FourthFloorZ",
        "owner": "Core/IGPrologueWorldScene.h",
        "literal": "900.0f",
        "why": "403호가 앉은 슬래브. 씬·그레이박스·밤 2·밤 3·퍼즐 2가"
               " 각자 900을 적고 있었다",
    },
    "ScreenWidth": {
        "expression": "AIGPrologueWorldScene::CctvScreenWidth",
        "owner": None,
        "literal": None,
        "why": "로비 모니터 화면. 케이스는 씬이, 렌더 면은 채널 5가 세우는데"
               " 같은 화면이다",
    },
    "ScreenHeight": {
        "expression": "AIGPrologueWorldScene::CctvScreenHeight",
        "owner": None,
        "literal": None,
        "why": "위와 같은 화면의 세로",
    },
    "ReplaySkipDurationSeconds": {
        "expression": "IGReplaySkip::HoldSeconds",
        "owner": None,
        "literal": None,
        "why": "§34.2 재관람 스킵. 에필로그와 다섯째 새벽이 같은 손동작을 쓴다",
    },
    "ReplaySkipRewindMultiplier": {
        "expression": "IGReplaySkip::RewindMultiplier",
        "owner": None,
        "literal": None,
        "why": "위와 같은 손동작의 되감기 배율",
    },
}


def collect(documents):
    """{경로: 본문} → {이름: [(경로, 값), ...]}"""
    found = collections.defaultdict(list)
    for path, text in sorted(documents.items()):
        for match in DECLARATION.finditer(text):
            found[match.group("name")].append(
                (path, match.group("value").strip()))
    return found


def find_duplicates(documents):
    """이름과 숫자 값이 모두 같은 선언이 두 파일 이상에 있는 것."""
    duplicates = {}
    for name, entries in collect(documents).items():
        by_value = collections.defaultdict(set)
        for path, value in entries:
            if NUMERIC.match(value):
                by_value[value.rstrip("f")].add(path)
        for value, paths in by_value.items():
            if len(paths) > 1:
                duplicates[(name, value)] = sorted(paths)
    return duplicates


def find_reauthored(documents, rules=None):
    """주인을 정한 이름이 주인 밖에서 다시 숫자로 적힌 것."""
    findings = []
    declared = collect(documents)
    for name, rule in sorted((rules or DERIVED_ONLY).items()):
        entries = declared.get(name, [])
        derived = 0
        for path, value in entries:
            if not NUMERIC.match(value):
                if value == rule["expression"]:
                    derived += 1
                continue
            if rule["owner"] is not None and path == rule["owner"]:
                if rule["literal"] is not None and value != rule["literal"]:
                    findings.append(
                        "  %s 의 주인이 %s 를 든다. 정해 둔 값은 %s 다"
                        % (name, value, rule["literal"]))
                continue
            findings.append(
                "  %s 이(가) %s 에서 다시 숫자(%s)로 적혔다. %s 를 받아 써라"
                % (name, path, value, rule["expression"]))
        if derived == 0:
            findings.append(
                "  %s 을(를) 받아 쓰는 곳이 없다. 주인을 정해 둔 뜻이 없어졌다"
                % name)
    return findings


# 찾았지만 아직 안 고친 것. 면제가 아니라 **미결**이다 — 면제는 「이대로가
# 맞다」이고 이건 「사람이 정해야 한다」다. 그래서 실패로 세지는 않되 매번
# 화면에 남긴다. 조용히 사라지는 것이 제일 나쁘다.
CARRIED = {}


def find_copied_points(documents):
    """이름을 붙인 자리가 다른 데서 숫자로 다시 적힌 것."""
    named = {}
    for path, text in sorted(documents.items()):
        for match in VECTOR_DECLARATION.finditer(text):
            name = match.group("name")
            if not name.endswith(PLACE_SUFFIXES):
                continue
            triple = tuple(match.group(axis) for axis in ("x", "y", "z"))
            named.setdefault(triple, (name, path, match.start()))

    findings = []
    for path, text in sorted(documents.items()):
        for match in VECTOR_LITERAL.finditer(text):
            triple = tuple(match.group(axis) for axis in ("x", "y", "z"))
            if triple not in named:
                continue
            name, owner, offset = named[triple]
            if path == owner and match.start() <= offset <= match.end():
                continue
            findings.append(
                "  %s 의 자리가 %s 에서 숫자로 다시 적혔다. %s 를 써라"
                % (name, path, name))
    return sorted(set(findings))


def read_sources():
    documents = {}
    for path in sorted(SOURCE_ROOT.rglob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        documents[path.relative_to(SOURCE_ROOT).as_posix()] = (
            path.read_bytes().decode("utf-8-sig", errors="replace"))
    return documents


def run_check():
    documents = read_sources()
    duplicates = find_duplicates(documents)
    findings = []
    for (name, value), paths in sorted(duplicates.items()):
        if name in REVIEWED:
            continue
        findings.append("  %s = %s  →  %s" % (name, value, " / ".join(paths)))

    # 면제가 실제로 쓰이고 있는지도 본다. 다 고친 뒤에 남은 면제는 규칙처럼
    # 읽히고, 다음 사람이 그 이름으로 새 중복을 만든다.
    for name in sorted(set(REVIEWED) - {n for n, _ in duplicates}):
        findings.append(
            "  %s 은(는) 더 이상 중복이 아니다. 면제를 지워라" % name)

    findings.extend(find_reauthored(documents))

    carried = []
    for finding in find_copied_points(documents):
        owner = next((name for name in CARRIED if name in finding), None)
        if owner is None:
            findings.append(finding)
        else:
            carried.append((owner, finding))

    # 미결로 적어 두었는데 실제로는 없어진 것도 지운다. 남아 있으면 다음
    # 사람이 「아직 안 고친 게 있구나」로 읽고 찾으러 간다.
    for name in sorted(set(CARRIED) - {owner for owner, _ in carried}):
        findings.append(
            "  %s 은(는) 더 이상 두 군데에 없다. 미결 목록에서 지워라" % name)

    declarations = sum(len(v) for v in collect(documents).values())
    print("DUPLICATE CONSTANT AUDIT  declarations=%d duplicated=%d "
          "reviewed=%d owned=%d carried=%d findings=%d"
          % (declarations, len(duplicates), len(REVIEWED),
             len(DERIVED_ONLY), len(carried), len(findings)))
    for name in sorted(REVIEWED):
        if name in {n for n, _ in duplicates}:
            print("  면제 %-24s %s" % (name, REVIEWED[name]))
    for name in sorted({owner for owner, _ in carried}):
        print("  미결 %-24s %s" % (name, CARRIED[name]))
    if findings:
        print()
        print("같은 사실이 두 군데에 적혀 있다:")
        for finding in findings:
            print(finding)
        return 1
    print()
    print("every authored constant is written in exactly one place")
    return 0


def run_self_test():
    owned = DERIVED_ONLY["FourthFloorZ"]

    caught = find_duplicates({
        "a.cpp": "constexpr float SomeHeight = 900.0f;",
        "b.cpp": "constexpr float SomeHeight = 900.0f;",
    })
    assert ("SomeHeight", "900.0") in caught, "중복을 놓쳤다"

    assert not find_duplicates({
        "a.h": "static constexpr float SomeHeight = 900.0f;",
        "b.cpp": "constexpr float SomeHeight = Owner::SomeHeight;",
    }), "받아 쓰는 것을 중복으로 셌다"

    assert not find_duplicates({
        "a.cpp": "constexpr float SomeGap = 0.25f;",
        "b.cpp": "constexpr float SomeGap = 0.50f;",
    }), "값이 다른 것을 같은 사실로 셌다"

    assert not find_duplicates({
        "a.cpp": "constexpr float A = 1.0f;\nconstexpr float A = 1.0f;",
    }), "한 파일 안을 두 군데로 셌다"

    # 묶은 뒤에 한쪽만 숫자로 돌아가는 것 — 위 검사는 이걸 못 본다.
    healthy = {
        owned["owner"]: "static constexpr float FourthFloorZ = 900.0f;",
        "b.cpp": "constexpr float FourthFloorZ = %s;" % owned["expression"],
    }
    # 합성 문서에는 이 규칙 하나만 들어 있으므로 그 하나만 물어본다.
    only = {"FourthFloorZ": owned}
    assert not find_reauthored(healthy, only), "멀쩡한 상태를 걸었다"
    assert not find_duplicates(healthy), "멀쩡한 상태를 중복으로 셌다"

    broken = dict(healthy)
    broken["b.cpp"] = "constexpr float FourthFloorZ = 900.0f;"
    assert any("다시 숫자" in f for f in find_reauthored(broken, only)), \
        "주인 밖에서 다시 적힌 것을 놓쳤다"

    moved = dict(healthy)
    moved[owned["owner"]] = "static constexpr float FourthFloorZ = 950.0f;"
    assert any("정해 둔 값은" in f for f in find_reauthored(moved, only)), \
        "주인이 값을 옮긴 것을 놓쳤다"

    orphan = {owned["owner"]: "static constexpr float FourthFloorZ = 900.0f;"}
    assert any("받아 쓰는 곳이 없다" in f for f in find_reauthored(orphan, only)), \
        "아무도 안 받아 쓰는 것을 놓쳤다"

    print("DUPLICATE CONSTANT SELF-TEST PASS  "
          "duplicate=1 derived=0 different_value=0 same_file=0 "
          "reauthored=1 moved=1 orphan=1")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    if arguments.self_test:
        return run_self_test()
    if arguments.check:
        return run_check()
    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
