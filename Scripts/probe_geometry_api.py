"""One-shot probe: report which Geometry Script Python entry points exist.

Run once after enabling the GeometryScripting plugin so generate_meshes.py can
be written against the real API surface instead of guesses.
"""

import unreal


def dump(class_name, keyword_filter=None):
    holder = getattr(unreal, class_name, None)
    if holder is None:
        unreal.log_warning(f"[PROBE] MISSING class: {class_name}")
        return
    names = sorted(
        name for name in dir(holder)
        if not name.startswith("_")
        and (keyword_filter is None or any(k in name for k in keyword_filter))
    )
    unreal.log_warning(f"[PROBE] {class_name}: {len(names)} entries")
    for name in names:
        unreal.log_warning(f"[PROBE]   {class_name}.{name}")


def probe():
    for type_name in (
        "DynamicMesh",
        "GeometryScriptPrimitiveOptions",
        "GeometryScriptRevolveOptions",
        "GeometryScriptCopyMeshToAssetOptions",
        "GeometryScriptCreateNewStaticMeshAssetOptions",
        "GeometryScriptCalculateNormalsOptions",
        "GeometryScriptSimpleCollisionOptions",
    ):
        unreal.log_warning(
            f"[PROBE] type {type_name}: "
            f"{'OK' if hasattr(unreal, type_name) else 'MISSING'}")

    dump("GeometryScript_Primitives", ["append"])
    dump("GeometryScript_NewAssetUtils")
    dump("GeometryScript_AssetUtils", ["static_mesh", "StaticMesh"])
    dump("GeometryScript_MeshNormals", ["normals", "tangents", "smooth"])
    dump("GeometryScript_MeshUVs", ["uv", "project", "repack"])
    dump("GeometryScript_MeshEdits", ["bevel", "offset", "extrude"])
    dump("GeometryScript_MeshDeformers", ["bend", "twist", "displace"])
    dump("GeometryScript_MeshBooleans", ["boolean", "append", "union"])
    dump("GeometryScript_MeshTransforms", ["transform", "translate", "scale"])
    dump("GeometryScript_Collision", ["collision"])
    dump("GeometryScript_MeshSelection", ["select"])
    dump("GeometryScript_MeshModeling", ["bevel", "offset", "solidify"])
    dump("GeometryScript_MeshRepair", ["weld", "compact"])

    unreal.log_warning("[PROBE] complete")


if __name__ == "__main__":
    probe()
