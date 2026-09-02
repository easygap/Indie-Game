# -*- coding: utf-8 -*-
"""계약의 문자열 바늘이 실제로 무언가를 지키는지 본다.

계약 대부분은 「이 파일에 이 문자열이 있다」로 값을 지킨다. 그 문자열이 대상
파일에 두 번 이상 나오면 한 군데를 지워도 통과한다. 검사가 있는데 안 잡는
상태이고, 화면에서는 초록불이다.

가장 나빴던 것은 설계값을 맨 숫자로 찾던 자리였다. 3300줄 바이블에서 `0.6`은
열일곱 번, `1.2초`는 열두 번 나온다. §27을 통째로 지워도 그 줄들은 통과했다.

무엇을 세는가

  대상이 **문서**이고 바늘이 **값처럼 생긴 짧은 문자열**인 경우만 본다.
  코드 쪽 존재 검사(「이 파일이 이 함수를 부른다」)는 여러 번 나오는 것이
  정상이라 세지 않는다. 값은 한 번 적히고 한 번 검사되어야 한다.

바늘을 고치는 방법은 값을 지우는 것이 아니라 **값이 무엇을 정하는지까지
적힌 줄**로 바꾸는 것이다. 그러면 값이 바뀔 때 검사가 잡고, 문장이 사라질
때도 잡는다.
"""
import argparse
import pathlib
import re
import sys

PROJECT_ROOT = pathlib.Path(__file__).resolve().parent.parent
SCRIPTS_ROOT = PROJECT_ROOT / "Scripts"

BINDING = re.compile(
    r"\$(?P<var>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*"
    r"(?:Read-Source|Read-ProjectText)\s*(?:\()?\s*'(?P<path>[^']+)'")
CALL = re.compile(
    r"(?:Require-All|Assert-ContainsAll)\s+\$(?P<var>[A-Za-z0-9_]+)\s+@\("
    r"(?P<body>[\s\S]*?)\n\)\s*(?P<label>'[^']*')?",
    re.M)
NEEDLE = re.compile(r"'((?:[^']|'')*)'")
# 값처럼 생긴 것: 숫자로 시작하고 단위가 붙거나 안 붙거나.
VALUE_LIKE = re.compile(
    r"^[0-9][0-9.]*\s*(?:ms|s|초|%|°|cm|m|dB|Hz|배|개|BPM|fps)?$")


def find_weak(scripts, read_target):
    """(스크립트, 대상, 라벨, 바늘, 횟수) 목록."""
    weak = []
    for name, text in sorted(scripts.items()):
        bound = {m.group("var"): m.group("path") for m in BINDING.finditer(text)}
        if not bound:
            continue
        for call in CALL.finditer(text):
            relative = bound.get(call.group("var"))
            if relative is None or not relative.startswith("Docs/"):
                continue
            body = read_target(relative)
            if body is None:
                continue
            label = (call.group("label") or "''").strip("'")
            for match in NEEDLE.finditer(call.group("body")):
                needle = match.group(1).replace("''", "'")
                if not VALUE_LIKE.match(needle.strip()):
                    continue
                hits = body.count(needle)
                if hits != 1:
                    weak.append((name, relative, label, needle, hits))
    return weak


def read_scripts():
    scripts = {}
    for path in sorted(SCRIPTS_ROOT.glob("*.ps1")):
        scripts[path.name] = path.read_bytes().decode("utf-8-sig", errors="replace")
    return scripts


def make_reader():
    cache = {}

    def read(relative):
        if relative not in cache:
            target = PROJECT_ROOT / relative
            cache[relative] = (
                target.read_bytes().decode("utf-8-sig", errors="replace")
                if target.exists() else None)
        return cache[relative]

    return read


def run_check():
    weak = find_weak(read_scripts(), make_reader())
    print("WEAK NEEDLE AUDIT  contracts=%d findings=%d"
          % (len(read_scripts()), len(weak)))
    if weak:
        print()
        print("설계값을 맨 숫자로 찾고 있다. 값이 무엇을 정하는지까지 적힌 줄로 바꿔라:")
        for name, relative, label, needle, hits in weak:
            print("  %s  [%s] %r → %s 안에서 %d회"
                  % (name, label, needle, relative, hits))
        return 1
    print()
    print("every authored value is pinned by a line that says what it decides")
    return 0


def run_self_test():
    document = (
        "지연은 40ms 이하다.\n"
        "표: | 조작감 | 120ms 버퍼, 홀드 취소 0.6배 되감기 |\n"
        "다른 자리에도 0.6이 있고 또 0.6이 있다.\n")
    read = lambda relative: document if relative == "Docs/D.md" else None

    weak_script = {
        "Weak.ps1":
            "$story = Read-Source 'Docs/D.md'\n"
            "Assert-ContainsAll $story @(\n\t'0.6'\n) 'design'\n",
    }
    found = find_weak(weak_script, read)
    assert len(found) == 1 and found[0][4] == 3, "여러 번 나오는 값을 놓쳤다"

    missing_script = {
        "Missing.ps1":
            "$story = Read-Source 'Docs/D.md'\n"
            "Assert-ContainsAll $story @(\n\t'99ms'\n) 'design'\n",
    }
    assert find_weak(missing_script, read)[0][4] == 0, "없는 값을 놓쳤다"

    strong_script = {
        "Strong.ps1":
            "$story = Read-Source 'Docs/D.md'\n"
            "Assert-ContainsAll $story @(\n"
            "\t'120ms 버퍼, 홀드 취소 0.6배 되감기'\n) 'design'\n",
    }
    assert not find_weak(strong_script, read), "문장 바늘을 약하다고 셌다"

    code_script = {
        "Code.ps1":
            "$source = Read-Source 'Source/IndieGame/A.cpp'\n"
            "Assert-ContainsAll $source @(\n\t'0.6'\n) 'runtime'\n",
    }
    assert not find_weak(code_script, read), "코드 대상까지 셌다"

    print("WEAK NEEDLE SELF-TEST PASS  repeated=1 missing=1 sentence=0 code=0")
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
