# -*- coding: utf-8 -*-
"""번역할 문장마다 쓰인 자리의 맥락을 붙여 JSON으로 뽑는다.

UE가 만든 한국어 .po(원문)에서 항목을 읽고, 소스 위치의 앞뒤 코드와 주석을
붙인다. 번역가는 누가 어떤 상황에서 말하는지, 화면에 얼마나 오래 뜨는지 보고
옮긴다. 이미 번역된 언어의 .po가 있으면 그 번역도 함께 싣는다.

    python Scripts/build_localization_context.py
"""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOC = os.path.join(ROOT, "Content", "Localization", "Game")
OUT = os.path.join(ROOT, "Saved", "Localization", "context.json")
CULTURES = ("en", "ja", "zh-Hans", "zh-Hant")


def unquote(text):
    return (text.replace('\\"', '"').replace("\\n", "\n").replace("\\t", "\t")
            .replace("\\\\", "\\"))


def read_po(path):
    """msgctxt·msgid·msgstr와 SourceLocation 주석을 항목 단위로 읽는다."""
    entries = []
    current = {}
    field = None
    with open(path, encoding="utf-8-sig") as handle:
        for raw in handle:
            line = raw.rstrip("\n")
            if not line.strip():
                if current.get("msgctxt") is not None:
                    entries.append(current)
                current = {}
                field = None
                continue
            if line.startswith("#. SourceLocation:"):
                current.setdefault("locations", []).append(line.split(":", 1)[1].strip())
                continue
            if line.startswith("#"):
                continue
            match = re.match(r'(msgctxt|msgid|msgstr) "(.*)"$', line)
            if match:
                field = match.group(1)
                current[field] = unquote(match.group(2))
                continue
            match = re.match(r'"(.*)"$', line)
            if match and field:
                current[field] += unquote(match.group(1))
    if current.get("msgctxt") is not None:
        entries.append(current)
    return [entry for entry in entries if entry.get("msgctxt")]


def code_context(location):
    match = re.match(r"(.+)\((\d+)\)$", location)
    if not match:
        return None, None
    path = os.path.join(ROOT, match.group(1))
    line = int(match.group(2))
    try:
        with open(path, encoding="utf-8", errors="ignore") as handle:
            lines = handle.read().splitlines()
    except OSError:
        return match.group(1), None
    start = max(0, line - 9)
    end = min(len(lines), line + 3)
    snippet = "\n".join(lines[start:end])
    return f"{match.group(1)}:{line}", snippet


def main():
    source_po = os.path.join(LOC, "ko", "Game.po")
    if not os.path.isfile(source_po):
        sys.exit("한국어 .po가 없습니다. Scripts/Localize-Game.ps1 -Gather를 먼저 돌리세요.")
    source = read_po(source_po)
    translations = {}
    for culture in CULTURES:
        path = os.path.join(LOC, culture, "Game.po")
        if os.path.isfile(path):
            translations[culture] = {e["msgctxt"]: e.get("msgstr", "") for e in read_po(path)}
    items = []
    for entry in source:
        location = (entry.get("locations") or [""])[0]
        where, snippet = code_context(location)
        item = {
            "id": entry["msgctxt"],
            "ko": entry.get("msgid", ""),
            "where": where,
            "context": snippet,
        }
        for culture, table in translations.items():
            item[culture] = table.get(entry["msgctxt"], "")
        items.append(item)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as handle:
        json.dump(items, handle, ensure_ascii=False, indent=1)
    print(f"LOCALIZATION_CONTEXT PASS items={len(items)} -> {OUT}")


if __name__ == "__main__":
    main()
