#!/usr/bin/env python3
"""씬이 요청하는 재질 이름이 실제로 씬에 닿는지 확인한다.

``IGPrologueWorldScene``은 구운 재질을 경로로 직접 열지 않는다.
``LoadTexturedMaterials()``가 이름 배열을 한 번 훑어 ``TexturedMaterials``
맵을 채우고, 그 뒤로는 ``TexMat(TEXT("M_X"), Fallback)``이 그 맵만 본다.
그래서 배열에 없는 이름은 ``.uasset``이 디스크에 멀쩡히 있어도 영원히
폴백으로 그려진다. 컴파일러도 쿠커도 이것을 모른다 — 폴백이 그럴듯한
평면 색이면 플레이 화면에서도 티가 안 난다.

실제로 그렇게 새어 나간 적이 있다. ``e901de5``는 발소리 표면 셋과 판독면
둘, 현관문 강판을 갈았다고 적었지만 이름을 배열에 넣지 않아 전부 이전
표면 그대로였고, ``0f81b9b``이 커밋한 재질 다섯도 같은 이유로 한 번도
쓰이지 않았다.

이 감사는 코드만 읽고 세 가지를 본다.

* ``UNREACHABLE``    - TexMat이 부르는데 배열에 없는 이름. 늘 폴백이 나간다
* ``UNBAKED``        - 배열에 있는데 ``.uasset``이 트리에 없는 이름
* ``SELF_FALLBACK``  - 폴백이 같은 재질을 가리키는 호출. 감싼 의미가 없다

어디서든 돌아간다.

    python3 Scripts/audit_scene_materials.py             # 사람이 읽는 보고
    python3 Scripts/audit_scene_materials.py --json      # 기계가 읽는 보고
    python3 Scripts/audit_scene_materials.py --check     # 발견되면 exit 1
    python3 Scripts/audit_scene_materials.py --self-test # 감사 자체를 검사
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass


PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 이름 배열을 소유한 파일. TexMat 호출은 씬 코드 전체에서 찾는다.
REGISTRY_SOURCE = os.path.join(
    "Source", "IndieGame", "Core", "IGPrologueWorldScene.cpp")
SOURCE_ROOT = os.path.join("Source", "IndieGame")
MATERIAL_DIR = os.path.join("Content", "Prototype", "Materials")

REGISTRY_BEGIN = "void AIGPrologueWorldScene::LoadTexturedMaterials()"
REGISTRY_END = "int32 LoadedCount = 0;"

NAME_PATTERN = re.compile(r'TEXT\("(M_[A-Za-z0-9_]+)"\)')
# TexMat(TEXT("M_X"), Fallback) — 폴백은 멤버 변수이거나 nullptr이다.
CALL_PATTERN = re.compile(
    r'TexMat\(\s*TEXT\("(M_[A-Za-z0-9_]+)"\)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)')
# Member = FindMaterial(TEXT("/Game/Prototype/Materials/M_X.M_X"));
MEMBER_PATTERN = re.compile(
    r'([A-Za-z_][A-Za-z0-9_]*)\s*=\s*FindMaterial\(\s*TEXT\('
    r'"/Game/Prototype/Materials/(M_[A-Za-z0-9_]+)\.')


@dataclass(frozen=True)
class Finding:
    code: str
    name: str
    detail: str
    sites: tuple

    def to_dict(self) -> dict:
        return {
            "code": self.code,
            "name": self.name,
            "detail": self.detail,
            "sites": list(self.sites),
        }


def _line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def parse_registry(registry_text: str) -> list:
    """LoadTexturedMaterials()가 로드하는 이름을 배열 순서대로 돌려준다."""
    begin = registry_text.find(REGISTRY_BEGIN)
    if begin < 0:
        raise RuntimeError(f"{REGISTRY_BEGIN} 를 찾지 못했다")
    end = registry_text.find(REGISTRY_END, begin)
    if end < 0:
        raise RuntimeError(f"{REGISTRY_BEGIN} 안에서 {REGISTRY_END} 를 찾지 못했다")
    return NAME_PATTERN.findall(registry_text[begin:end])


def parse_members(registry_text: str) -> dict:
    """FindMaterial로 채워지는 멤버 변수 -> 그 멤버가 가리키는 재질 이름."""
    return {
        match.group(1): match.group(2)
        for match in MEMBER_PATTERN.finditer(registry_text)
    }


def parse_calls(sources: dict) -> dict:
    """TexMat 호출 이름 -> [(파일:줄, 폴백 식별자)]."""
    calls = {}
    for relative_path, text in sorted(sources.items()):
        for match in CALL_PATTERN.finditer(text):
            name, fallback = match.group(1), match.group(2)
            site = f"{relative_path}:{_line_of(text, match.start())}"
            calls.setdefault(name, []).append((site, fallback))
    return calls


def audit(registry: list, calls: dict, members: dict, baked: set) -> list:
    """세 가지 상태를 순서대로 모아 돌려준다."""
    listed = set(registry)
    findings = []

    for name in sorted(calls):
        if name in listed:
            continue
        sites = tuple(site for site, _ in calls[name])
        state = "구운 재질이 트리에 있다" if name in baked else "아직 굽지 않았다"
        findings.append(Finding(
            "UNREACHABLE", name,
            f"LoadTexturedMaterials()의 배열에 없어 늘 폴백이 나간다 ({state})",
            sites))

    for name in registry:
        if name not in baked:
            sites = tuple(site for site, _ in calls.get(name, ()))
            findings.append(Finding(
                "UNBAKED", name,
                f"{MATERIAL_DIR} 에 .uasset이 없어 로드가 조용히 실패한다",
                sites))

    for name in sorted(calls):
        for site, fallback in calls[name]:
            if members.get(fallback) != name:
                continue
            findings.append(Finding(
                "SELF_FALLBACK", name,
                f"폴백 {fallback}이 같은 재질을 가리킨다 — 멤버를 그대로 넘겨라",
                (site,)))

    return findings


def read_sources(project_root: str) -> dict:
    """씬 코드 전체를 상대 경로 -> 본문으로 읽는다."""
    sources = {}
    root = os.path.join(project_root, SOURCE_ROOT)
    for directory, _, files in os.walk(root):
        for filename in files:
            if not filename.endswith(".cpp"):
                continue
            absolute = os.path.join(directory, filename)
            relative = os.path.relpath(absolute, project_root).replace("\\", "/")
            with open(absolute, "r", encoding="utf-8", errors="replace") as handle:
                sources[relative] = handle.read()
    return sources


def baked_materials(project_root: str) -> set:
    directory = os.path.join(project_root, MATERIAL_DIR)
    if not os.path.isdir(directory):
        return set()
    return {
        os.path.splitext(name)[0]
        for name in os.listdir(directory)
        if name.endswith(".uasset")
    }


def run(project_root: str) -> tuple:
    registry_path = os.path.join(project_root, REGISTRY_SOURCE)
    with open(registry_path, "r", encoding="utf-8") as handle:
        registry_text = handle.read()
    registry = parse_registry(registry_text)
    members = parse_members(registry_text)
    calls = parse_calls(read_sources(project_root))
    findings = audit(registry, calls, members, baked_materials(project_root))
    return registry, calls, findings


# --------------------------------------------------------------------------
# 자기 검사. 감사가 조용히 아무것도 못 찾는 상태로 굳는 것이 이 감사가 막으려는
# 결함과 같은 모양이므로, 세 코드가 각각 켜지고 꺼지는 것을 직접 본다.
# --------------------------------------------------------------------------

SELF_TEST_REGISTRY = """
void AIGPrologueWorldScene::FindMaterials()
{
\tConcreteMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_Concrete.M_Concrete"));
\tScreenGlowMaterial = FindMaterial(TEXT("/Game/Prototype/Materials/M_ScreenGlow.M_ScreenGlow"));
}

void AIGPrologueWorldScene::LoadTexturedMaterials()
{
\tconst TCHAR* MaterialNames[] = {
\t\tTEXT("M_Listed"), TEXT("M_Unbaked"),
\t};

\tint32 LoadedCount = 0;
}
"""

SELF_TEST_CALLS = (
    '\tTexMat(TEXT("M_Listed"), ConcreteMaterial);\n'
    '\tTexMat(TEXT("M_Missing"), ConcreteMaterial);\n'
    '\tTexMat(\n\t\tTEXT("M_ScreenGlow"), ScreenGlowMaterial);\n'
    '\tTexMat(LoopVariable, ConcreteMaterial);\n'
)


def _self_test() -> int:
    failures = []

    def check(label, actual, expected):
        if actual != expected:
            failures.append(f"{label}: {actual!r} != {expected!r}")

    registry = parse_registry(SELF_TEST_REGISTRY)
    check("배열 파싱", registry, ["M_Listed", "M_Unbaked"])
    # FindMaterials의 이름들은 배열 바깥이므로 새어 들어오면 안 된다.
    check("배열 경계", "M_Concrete" in registry, False)

    members = parse_members(SELF_TEST_REGISTRY)
    check("멤버 파싱", members, {
        "ConcreteMaterial": "M_Concrete",
        "ScreenGlowMaterial": "M_ScreenGlow",
    })

    calls = parse_calls({"Fake.cpp": SELF_TEST_CALLS})
    check("호출 파싱", sorted(calls), ["M_Listed", "M_Missing", "M_ScreenGlow"])
    check("줄 번호", calls["M_Missing"][0][0], "Fake.cpp:2")
    # 줄바꿈을 낀 호출도 같은 호출이다. 변수로 부르는 자리는 셀 수 없으므로
    # 애초에 세지 않는다 — 그 자리는 사람이 배열과 맞춰야 한다.
    check("여러 줄 호출", calls["M_ScreenGlow"][0][0], "Fake.cpp:3")

    findings = audit(registry, calls, members, {"M_Listed", "M_ScreenGlow"})
    by_code = {}
    for finding in findings:
        by_code.setdefault(finding.code, []).append(finding.name)
    check("UNREACHABLE", sorted(by_code.get("UNREACHABLE", [])),
          ["M_Missing", "M_ScreenGlow"])
    check("UNBAKED", by_code.get("UNBAKED"), ["M_Unbaked"])
    check("SELF_FALLBACK", by_code.get("SELF_FALLBACK"), ["M_ScreenGlow"])

    # 배선이 온전하면 아무것도 나오지 않아야 한다. 늘 실패하는 감사는
    # 늘 통과하는 감사만큼이나 쓸모가 없다.
    clean = audit(
        ["M_Listed"],
        {"M_Listed": [("Fake.cpp:1", "ConcreteMaterial")]},
        members,
        {"M_Listed"})
    check("무결 상태", clean, [])

    if failures:
        for failure in failures:
            print(f"  FAIL {failure}")
        print(f"SCENE MATERIAL AUDIT SELF-TEST FAIL  {len(failures)} case(s)")
        return 1
    print("PASS scene material audit self-test: registry bounds, member table, "
          "multi-line and non-literal calls, all three codes on and off")
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="발견 항목이 있으면 exit 1")
    parser.add_argument("--json", action="store_true",
                        help="기계가 읽는 보고")
    parser.add_argument("--self-test", action="store_true",
                        help="감사 자체를 합성 입력으로 검사한다")
    arguments = parser.parse_args(argv)

    if arguments.self_test:
        return _self_test()

    registry, calls, findings = run(PROJECT_ROOT)

    if arguments.json:
        print(json.dumps({
            "listed": len(registry),
            "called": len(calls),
            "findings": [f.to_dict() for f in findings],
        }, indent=2, ensure_ascii=False))
    else:
        print(f"SCENE MATERIAL AUDIT  listed={len(registry)} "
              f"called={len(calls)} findings={len(findings)}")
        if not findings:
            print("\nevery material the scene asks for reaches it")
        else:
            print()
            for finding in findings:
                print(f"  [{finding.code}] {finding.name}")
                print(f"      {finding.detail}")
                for site in finding.sites:
                    print(f"      {site}")

    return 1 if (arguments.check and findings) else 0


if __name__ == "__main__":
    sys.exit(main())
