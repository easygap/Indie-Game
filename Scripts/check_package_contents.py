"""배포본 컨테이너에 코드가 경로로 불러오는 에셋이 실제로 들었는지 본다.

check_cook_references.py는 쿠크를 돌리지 않고 설정과 참조만 읽는다. 그래서
설정이 맞아 보여도 쿠커가 조용히 빠뜨린 에셋은 잡지 못한다. 0.2.4까지 HUD
텍스처 8개가 그랬다. Asset Manager 규칙이 Game 설정이 아닌 DefaultEngine.ini에
있어서 언리얼이 읽지 않았는데, 정적 검사는 그 파일을 그대로 읽어 통과시켰다.

이 검사는 실제 배포본의 IoStore 목록(UnrealEditor-Cmd -run=IoStore -List)을
읽는다. 소스의 TEXT("/Game/...") 리터럴과 IGHudTexture 규칙의 에셋 가운데
Content에 실제로 있는 것은 모두 컨테이너에 있어야 한다.

    python Scripts/check_package_contents.py <container.csv>
"""
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LITERAL = re.compile(r'TEXT\("(/Game/[^"]+)"\)')
SPECIFIC = re.compile(r'"(/Game/[^"]+)"')


def package_of(path):
    # /Game/Dir/Name.Name -> /Game/Dir/Name
    return path.split(".", 1)[0].rstrip("/")


def source_paths():
    found = {}
    for base, _, files in os.walk(os.path.join(ROOT, "Source")):
        for name in files:
            if not name.endswith((".cpp", ".h")):
                continue
            full = os.path.join(base, name)
            with open(full, encoding="utf-8-sig", errors="replace") as handle:
                for number, line in enumerate(handle, 1):
                    for match in LITERAL.finditer(line):
                        literal = match.group(1)
                        # 접두사로 이름을 조립하는 자리(…_ , %s)와 폴더 경로는 뺀다.
                        if "%" in literal or literal.endswith(("_", "/")):
                            continue
                        found.setdefault(package_of(literal), f"{os.path.relpath(full, ROOT)}:{number}")
    return found


def rule_paths():
    found = {}
    with open(os.path.join(ROOT, "Config", "DefaultGame.ini"), encoding="utf-8-sig") as handle:
        for line in handle:
            if line.startswith("+PrimaryAssetTypesToScan=") and "SpecificAssets=" in line:
                for match in SPECIFIC.finditer(line.split("SpecificAssets=", 1)[1]):
                    found.setdefault(package_of(match.group(1)), "Config/DefaultGame.ini")
    return found


def exists_in_content(package):
    relative = package[len("/Game/"):]
    base = os.path.join(ROOT, "Content", *relative.split("/"))
    return os.path.exists(base + ".uasset") or os.path.exists(base + ".umap")


def container_packages(csv_path):
    packages = set()
    with open(csv_path, newline="", encoding="utf-8", errors="replace") as handle:
        reader = csv.reader(handle, skipinitialspace=True)
        header = next(reader)
        column = header.index("Filename")
        for row in reader:
            if len(row) <= column:
                continue
            name = row[column].replace("\\", "/")
            marker = "/IndieGame/Content/"
            if marker in name and name.endswith((".uasset", ".umap")):
                packages.add("/Game/" + name.split(marker, 1)[1].rsplit(".", 1)[0])
    return packages


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    shipped = container_packages(argv[1])
    required = dict(source_paths())
    required.update(rule_paths())
    checked = {path: where for path, where in required.items() if exists_in_content(path)}
    missing = sorted(path for path in checked if path not in shipped)
    for path in missing:
        print(f"MISSING {path}  ({checked[path]})")
    status = "PASS" if not missing else "FAIL"
    print(f"PACKAGE_CONTENTS {status} required={len(checked)} missing={len(missing)} shipped_packages={len(shipped)}")
    return 0 if not missing else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
