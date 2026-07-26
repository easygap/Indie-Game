"""Reports the UV bounds of the label sleeve so wrapping can be verified."""

import unreal


def log(message):
    unreal.log_warning(f"[UVPROBE] {message}")


def probe():
    queries = None
    uvs = None
    for attribute in dir(unreal):
        if not attribute.startswith("GeometryScript_"):
            continue
        if "quer" in attribute.lower():
            queries = getattr(unreal, attribute)
        if attribute.lower().endswith("_uvs"):
            uvs = getattr(unreal, attribute)

    log(f"queries={queries} uvs={uvs}")
    if uvs is not None:
        names = [n for n in dir(uvs) if "cylinder" in n or "get_mesh" in n]
        log(f"uv functions of interest: {names}")

    sleeve = unreal.load_asset("/Game/Meshes/SM_LabelSleeve")
    if sleeve is None:
        log("SM_LabelSleeve MISSING")
        return

    mesh = unreal.new_object(unreal.DynamicMesh)
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    lod = unreal.GeometryScriptMeshReadLOD()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        sleeve, mesh, options, lod)

    if queries is not None:
        try:
            uv_min = None
            uv_max = None
            result = queries.get_all_mesh_uvs(mesh, 0) \
                if hasattr(queries, "get_all_mesh_uvs") else None
            log(f"get_all_mesh_uvs -> {type(result)}")
            if result is not None:
                values = result[0] if isinstance(result, tuple) else result
                try:
                    coords = list(values)
                except Exception:  # noqa: BLE001
                    coords = []
                for coord in coords[:20000]:
                    u, v = coord.x, coord.y
                    if uv_min is None:
                        uv_min = [u, v]
                        uv_max = [u, v]
                    uv_min = [min(uv_min[0], u), min(uv_min[1], v)]
                    uv_max = [max(uv_max[0], u), max(uv_max[1], v)]
                log(f"UV bounds min={uv_min} max={uv_max} count={len(coords)}")
        except Exception as error:  # noqa: BLE001
            log(f"UV query failed: {error}")
    log("done")


if __name__ == "__main__":
    probe()
