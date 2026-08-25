"""구운 스태틱 메시의 실제 바운드를 JSON으로 뽑는다.

디렉터가 세우는 소품은 저작 메시를 그대로 쓰는 것이 많다. 그 소품이 책상을
뚫는지 바닥에 박히는지 보려면 메시가 실제로 몇 센티인지 알아야 하는데,
`generate_meshes.py`를 읽어 계산하는 방법은 못 미덥다 — 일부 메시는
`append_indexed_surface`로 정점을 직접 쌓고, 불리언과 라뜨도 섞인다. 원시
도형만 세면 조용히 작게 잡히고, 그러면 감사가 「겹치지 않는다」고 잘못
말한다.

그래서 소스가 아니라 **결과**를 읽는다. 에디터가 로드한 에셋의 바운드가
런타임이 실제로 쓰는 값이므로 추정이 없다.

    UnrealEditor-Cmd <uproject> -run=pythonscript
        -script="Scripts/export_mesh_bounds.py"

또는 `-ExecutePythonScript=`로 돌린다. 결과는 프로젝트 루트의
`Docs/mesh_bounds.json`에 쓴다.
"""

import json
import os

import unreal


MESH_ROOT = "/Game/Meshes"
OUTPUT_RELATIVE = os.path.join("Docs", "mesh_bounds.json")


def _bounds_of(mesh):
    """(원점, 반경) 또는 None. 5.x 마이너마다 이름이 달라 폴백을 둔다."""
    for getter in ("get_bounds", "get_bounding_box"):
        method = getattr(mesh, getter, None)
        if method is None:
            continue
        try:
            value = method()
        except Exception:
            continue
        origin = getattr(value, "origin", None)
        extent = getattr(value, "box_extent", None)
        if origin is not None and extent is not None:
            return (origin, extent), getter
        # FBox로 돌아오는 경우: min/max에서 직접 만든다.
        minimum = getattr(value, "min", None)
        maximum = getattr(value, "max", None)
        if minimum is not None and maximum is not None:
            origin = unreal.Vector(
                (minimum.x + maximum.x) * 0.5,
                (minimum.y + maximum.y) * 0.5,
                (minimum.z + maximum.z) * 0.5)
            extent = unreal.Vector(
                (maximum.x - minimum.x) * 0.5,
                (maximum.y - minimum.y) * 0.5,
                (maximum.z - minimum.z) * 0.5)
            return (origin, extent), getter
    return None, None


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([MESH_ROOT], True)
    assets = registry.get_assets_by_path(MESH_ROOT, recursive=True)

    exported = {}
    skipped = []
    getter_used = set()
    for data in assets:
        asset = data.get_asset()
        if not isinstance(asset, unreal.StaticMesh):
            continue
        name = str(data.asset_name)
        result, getter = _bounds_of(asset)
        if result is None:
            skipped.append(name)
            continue
        getter_used.add(getter)
        origin, extent = result
        exported[name] = {
            "origin": [round(origin.x, 4), round(origin.y, 4), round(origin.z, 4)],
            "extent": [round(extent.x, 4), round(extent.y, 4), round(extent.z, 4)],
        }

    project_root = unreal.Paths.project_dir()
    output = os.path.join(project_root, OUTPUT_RELATIVE)
    os.makedirs(os.path.dirname(output), exist_ok=True)
    with open(output, "w", encoding="utf-8") as handle:
        json.dump(
            {"meshes": dict(sorted(exported.items()))},
            handle, indent=2, ensure_ascii=False)
        handle.write("\n")

    # 읽지 못한 메시를 조용히 넘기지 않는다. 바운드가 빠진 메시는 감사에서
    # 미검사로 남고, 미검사는 통과가 아니다.
    unreal.log_warning(
        "[MESHBOUNDS] exported %d mesh(es) via %s, skipped %d%s"
        % (len(exported), ",".join(sorted(getter_used)) or "none",
           len(skipped), (": " + ", ".join(sorted(skipped))) if skipped else ""))


main()
