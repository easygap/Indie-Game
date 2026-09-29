# -*- coding: utf-8 -*-
"""번역 JSON을 언어별 .po에 써 넣고 흔한 실수를 막는다.

번역은 Localization/Translations/<culture>.json에 {"네임스페이스,키": "번역"}으로 둔다.
이 스크립트는 Content/Localization/Game/<culture>/Game.po의 msgstr만 바꾸고 나머지는
UE가 쓴 그대로 둔다. 다음을 어기면 멈춘다.

- 원문에 있는 FText 인자({0}, {Name})가 번역에 그대로 있어야 한다.
- 원문의 줄바꿈 수와 번역의 줄바꿈 수가 같아야 한다(화면 배치가 줄 수를 전제한다).
- 원문이 [소리]처럼 대괄호로 시작하면 번역도 그 언어의 소리 표기로 시작해야 한다.
- 빈 번역이 남으면 안 된다(-AllowMissing으로 끌 수 있다).

    python Scripts/apply_localization.py [--allow-missing]
"""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOC = os.path.join(ROOT, "Content", "Localization", "Game")
TRANSLATIONS = os.path.join(ROOT, "Localization", "Translations")
CULTURES = ("en", "ja", "zh-Hans", "zh-Hant")
ARGUMENT = re.compile(r"\{[A-Za-z0-9_]+\}")
# 소리 자막을 여는 괄호. 한국어는 [ ], 일본어·중국어는 전각 괄호를 쓴다.
SOUND_OPEN = {"en": ("[",), "ja": ("［", "["), "zh-Hans": ("［", "["), "zh-Hant": ("［", "[")}


def escape(text):
    return (text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
            .replace("\t", "\\t"))


def unquote(text):
    return (text.replace('\\"', '"').replace("\\n", "\n").replace("\\t", "\t")
            .replace("\\\\", "\\"))


def rewrite(path, table, culture, allow_missing):
    with open(path, encoding="utf-8-sig") as handle:
        lines = handle.read().split("\n")
    output = []
    problems = []
    context = None
    source = None
    index = 0
    filled = 0
    while index < len(lines):
        line = lines[index]
        match = re.match(r'msgctxt "(.*)"$', line)
        if match:
            context = unquote(match.group(1))
        match = re.match(r'msgid "(.*)"$', line)
        if match:
            source = unquote(match.group(1))
            # 여러 줄 msgid
            while index + 1 < len(lines) and re.match(r'"(.*)"$', lines[index + 1]):
                output.append(line)
                index += 1
                line = lines[index]
                source += unquote(re.match(r'"(.*)"$', line).group(1))
        if line.startswith('msgstr "') and context:
            translated = table.get(context)
            # msgstr가 여러 줄이면 이어진 줄을 건너뛴다.
            while index + 1 < len(lines) and re.match(r'"(.*)"$', lines[index + 1]):
                index += 1
            if translated is None or translated == "":
                if not allow_missing:
                    problems.append(f"{culture} {context}: 번역 없음")
                output.append('msgstr ""')
            else:
                for argument in ARGUMENT.findall(source or ""):
                    if argument not in translated:
                        problems.append(f"{culture} {context}: 인자 {argument} 빠짐")
                if (source or "").count("\n") != translated.count("\n"):
                    problems.append(f"{culture} {context}: 줄 수 다름 ({(source or '').count(chr(10))} vs {translated.count(chr(10))})")
                if (source or "").startswith("[") and not translated.startswith(SOUND_OPEN[culture]):
                    problems.append(f"{culture} {context}: 소리 자막 괄호 없음")
                output.append(f'msgstr "{escape(translated)}"')
                filled += 1
            context = None
            source = None
            index += 1
            continue
        output.append(line)
        index += 1
    with open(path, "w", encoding="utf-8-sig", newline="\n") as handle:
        handle.write("\n".join(output))
    return filled, problems


def main():
    allow_missing = "--allow-missing" in sys.argv
    all_problems = []
    for culture in CULTURES:
        source = os.path.join(TRANSLATIONS, f"{culture}.json")
        target = os.path.join(LOC, culture, "Game.po")
        if not os.path.isfile(source):
            print(f"{culture}: 번역 파일 없음, 건너뜀")
            continue
        with open(source, encoding="utf-8") as handle:
            table = json.load(handle)
        filled, problems = rewrite(target, table, culture, allow_missing)
        all_problems += problems
        print(f"{culture}: {filled}개 적용, 문제 {len(problems)}개")
    for problem in all_problems[:80]:
        print("  " + problem)
    if all_problems:
        sys.exit(1)
    print("APPLY_LOCALIZATION PASS")


if __name__ == "__main__":
    main()
