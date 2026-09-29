# -*- coding: utf-8 -*-
"""번역 JSON 한 벌을 원문과 대조해 흔한 실수를 찾는다.

    python Scripts/check_localization.py <번역.json> <culture> [--strict]

원문은 Saved/Localization/context.json(build_localization_context.py가 만든 것)에서
읽는다. 오류는 반드시 고쳐야 하는 것이고, 경고는 사람이 보고 판단한다.

오류
- 원문에 있는 FText 인자({0}, {Name})가 번역에 없다.
- 줄바꿈 수가 다르다.
- 원문이 [ 로 시작하는 소리 자막인데 번역이 여는 괄호로 시작하지 않는다.
- 번역이 비어 있다.
- 원문에 없는 키가 번역에 있다.

경고
- 한국어 글자가 번역에 남아 있다(줄 높이 표본 같은 예외는 빼고).
- 번역이 원문보다 지나치게 길다.
- 용어집의 한국어 용어가 원문에 있는데 그 언어의 용어가 번역에 없다.
- 원문에 없는 대시(—)나 느낌표를 번역이 더했다.
"""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTEXT = os.path.join(ROOT, 'Saved', 'Localization', 'context.json')
ARGUMENT = re.compile(r'\{[A-Za-z0-9_]+\}')
HANGUL = re.compile('[가-힣]')
SOUND_OPEN = {
    'en': ('[',),
    'ja': ('［', '[', '（', '('),
    'zh-Hans': ('［', '[', '（', '('),
    'zh-Hant': ('［', '[', '（', '('),
}
# 글자 수 비율 상한. 넘으면 경고만 한다.
LENGTH_RATIO = {'en': 2.6, 'ja': 1.9, 'zh-Hans': 1.6, 'zh-Hant': 1.6}
# 원문 용어 -> 그 언어에서 반드시 보여야 할 말(여러 개면 하나만 있어도 된다).
GLOSSARY = {
    '위층 사람': {'en': ['one upstairs'], 'ja': ['上の階の人'], 'zh-Hans': ['楼上的人'], 'zh-Hant': ['樓上的人']},
    '튜닝 해머': {'en': ['tuning hammer'], 'ja': ['チューニングハンマー'], 'zh-Hans': ['调音扳手'], 'zh-Hant': ['調音扳手']},
    '조율 수첩': {'en': ['tuning notebook'], 'ja': ['調律の手帳'], 'zh-Hans': ['调音笔记本'], 'zh-Hant': ['調音筆記本']},
    '계량기함': {'en': ['meter box'], 'ja': ['メーターボックス'], 'zh-Hans': ['电表箱'], 'zh-Hant': ['電錶箱']},
    '관리실': {'en': ['management office', 'office'], 'ja': ['管理人室'], 'zh-Hans': ['管理室'], 'zh-Hant': ['管理室']},
    '달빛빌라': {'en': ['Moonlight Villa'], 'ja': ['月光ヴィラ'], 'zh-Hans': ['月光公寓'], 'zh-Hant': ['月光公寓']},
    '열쇠 꾸러미': {'en': ['key ring'], 'ja': ['鍵束'], 'zh-Hans': ['钥匙'], 'zh-Hant': ['鑰匙']},
    '검침 기록지': {'en': ['meter reading log', 'reading log'], 'ja': ['検針記録'], 'zh-Hans': ['抄表记录'], 'zh-Hant': ['抄表紀錄']},
    '민원 대장': {'en': ['complaint ledger'], 'ja': ['苦情受付簿'], 'zh-Hans': ['投诉登记簿'], 'zh-Hant': ['投訴登記簿']},
    '저수조': {'en': ['water tank', 'tank'], 'ja': ['貯水槽'], 'zh-Hans': ['水箱'], 'zh-Hant': ['水塔']},
    '이송 펌프': {'en': ['transfer pump'], 'ja': ['揚水ポンプ'], 'zh-Hans': ['抽水泵'], 'zh-Hant': ['抽水馬達']},
    '우회 밸브': {'en': ['bypass valve'], 'ja': ['バイパスバルブ'], 'zh-Hans': ['旁通阀'], 'zh-Hant': ['旁通閥']},
}
# 번역하지 않는 키(줄 높이 표본). 한국어가 남아도 된다.
EXEMPT_HANGUL = {'IGHUD,LineHeightSample'}


def load_source():
    with open(CONTEXT, encoding='utf-8') as handle:
        return {item['id']: item['ko'] for item in json.load(handle)}


def check(table, culture, source):
    errors = []
    warnings = []
    for key, text in table.items():
        if key not in source:
            errors.append(f'{key}: 원문에 없는 키')
            continue
        ko = source[key]
        if not isinstance(text, str) or text.strip() == '':
            errors.append(f'{key}: 번역 없음')
            continue
        for argument in set(ARGUMENT.findall(ko)):
            if argument not in text:
                errors.append(f'{key}: 인자 {argument} 빠짐')
        if ko.count('\n') != text.count('\n'):
            errors.append(f'{key}: 줄 수 다름 ({ko.count(chr(10))} vs {text.count(chr(10))})')
        if ko.startswith('[') and not text.startswith(SOUND_OPEN[culture]):
            errors.append(f'{key}: 여는 괄호로 시작해야 함')
        if key not in EXEMPT_HANGUL and HANGUL.search(text):
            warnings.append(f'{key}: 한국어 글자가 남음 -> {text}')
        stripped_ko = ARGUMENT.sub('', ko)
        stripped_text = ARGUMENT.sub('', text)
        if len(stripped_ko) >= 6 and len(stripped_text) > len(stripped_ko) * LENGTH_RATIO[culture]:
            warnings.append(f'{key}: 원문보다 많이 김 ({len(stripped_ko)} -> {len(stripped_text)})')
        for term, targets in GLOSSARY.items():
            if term in ko and not any(t.lower() in text.lower() for t in targets[culture]):
                warnings.append(f'{key}: 용어집 "{term}" -> {targets[culture]} 없음')
        if '—' in text and '—' not in ko:
            warnings.append(f'{key}: 원문에 없는 대시')
        if ('!' in text or '！' in text) and '!' not in ko and '！' not in ko:
            warnings.append(f'{key}: 원문에 없는 느낌표')
    return errors, warnings


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    path, culture = sys.argv[1], sys.argv[2]
    strict = '--strict' in sys.argv
    with open(path, encoding='utf-8') as handle:
        table = json.load(handle)
    source = load_source()
    errors, warnings = check(table, culture, source)
    for line in errors:
        print('ERROR ' + line)
    for line in warnings:
        print('WARN  ' + line)
    missing = [key for key in source if key not in table]
    print(f'{culture}: {len(table)}개 번역, 오류 {len(errors)}, 경고 {len(warnings)}, 원문 대비 빠진 키 {len(missing)}')
    if errors or (strict and warnings):
        sys.exit(1)


if __name__ == '__main__':
    main()
