"""Author the prologue's hero props as real static meshes with Geometry Script.

Everything here is modelled procedurally — lathed bottles and tubs, swept
handles, beveled shells, boolean recesses — and baked into /Game/Meshes as
StaticMesh assets with simple collision. The scene then places these instead
of stacking engine cubes, which is what makes the props read as objects.

Run with: UnrealEditor-Cmd <uproject> -ExecutePythonScript=.../generate_meshes.py
"""

import math
import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

import mesh_lod_contract  # noqa: E402


MESH_ROOT = "/Game/Meshes"


# ---------------------------------------------------------------------------
# Small helpers around the Geometry Script bindings
# ---------------------------------------------------------------------------

def log(message):
    # LogPython Display is filtered from captured stdout in this project's
    # headless runs, so progress is reported at warning level.
    unreal.log_warning(f"[MESHGEN] {message}")


def find_library(*name_fragments):
    """Locates a GeometryScript_* library class by name fragments."""
    for attribute in dir(unreal):
        if not attribute.startswith("GeometryScript_"):
            continue
        lowered = attribute.lower()
        if all(fragment.lower() in lowered for fragment in name_fragments):
            return getattr(unreal, attribute)
    return None


PRIM = unreal.GeometryScript_Primitives
BOOL_LIB = unreal.GeometryScript_MeshBooleans
SELECT = unreal.GeometryScript_MeshSelection
MODEL = unreal.GeometryScript_MeshModeling
REPAIR = unreal.GeometryScript_MeshRepair
COLLISION = unreal.GeometryScript_Collision
NEW_ASSET = unreal.GeometryScript_NewAssetUtils
NORMALS = find_library("normal")
QUERIES = find_library("quer")
SIMPLIFY = find_library("simplif")
DEFORM = getattr(unreal, "GeometryScript_MeshDeformers", None)
BAKE = getattr(unreal, "GeometryScript_Bake", None)
# MeshBasicEditFunctions is exported to Python under its ScriptName,
# ``GeometryScript_MeshEdits`` (the C++ class name is not reflected verbatim).
EDITS = getattr(unreal, "GeometryScript_MeshEdits", None)


def new_mesh():
    try:
        return unreal.new_object(unreal.DynamicMesh)
    except Exception:  # noqa: BLE001 - binding differences across versions
        return unreal.DynamicMesh()


def prim_options(scale_to_fill=False):
    """Primitive options. scale_to_fill normalizes UVs to 0..1, which is what
    printed-artwork meshes (label sleeves, bags, cup wrappers) need."""
    options = unreal.GeometryScriptPrimitiveOptions()
    try:
        options.set_editor_property("polygroup_mode",
                                    unreal.GeometryScriptPrimitivePolygroupMode.PER_FACE)
    except Exception:  # noqa: BLE001
        pass
    if scale_to_fill:
        options.set_editor_property(
            "uv_mode", unreal.GeometryScriptPrimitiveUVMode.SCALE_TO_FILL)
    return options


def xf(location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0)):
    """Builds a transform from Unreal's familiar (pitch, yaw, roll) tuple.

    ``unreal.Rotator`` exposes its positional constructor in reflected struct
    order (roll, pitch, yaw), unlike C++ ``FRotator(Pitch, Yaw, Roll)``.  The
    mesh authoring calls below deliberately use the C++ order shared by the
    runtime scene code, so pass named fields here to keep vertical seams,
    cabinet leaves and stair stringers on their intended axes.
    """
    return unreal.Transform(
        location=unreal.Vector(*location),
        rotation=unreal.Rotator(
            pitch=rotation[0],
            yaw=rotation[1],
            roll=rotation[2]),
        scale=unreal.Vector(*scale))


def revolve(mesh, profile, steps=64, smooth=True, transform=None, scale_to_fill=False):
    """Lathes a (radius, height) profile around the Z axis."""
    options = unreal.GeometryScriptRevolveOptions()
    for name, value in (("hard_normals", not smooth), ("hard_normal_angle", 45.0),
                        ("revolve_degrees", 360.0), ("full_revolve_end_caps", True)):
        try:
            options.set_editor_property(name, value)
        except Exception:  # noqa: BLE001
            pass
    points = [unreal.Vector2D(radius, height) for radius, height in profile]
    # Radius here is an extra offset of the whole profile from the axis; the
    # profile already carries real radii, so it must stay at zero.
    return PRIM.append_revolve_polygon(
        mesh, prim_options(scale_to_fill), transform or xf(), points, options, 0.0, steps)


def box(mesh, size, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), origin_center=True,
        scale_to_fill=False):
    """Appends a box; size is (x, y, z) in centimeters."""
    options = prim_options(scale_to_fill)
    origin = (unreal.GeometryScriptPrimitiveOriginMode.CENTER if origin_center
              else unreal.GeometryScriptPrimitiveOriginMode.BASE)
    return PRIM.append_box(
        mesh, options, xf(location, rotation),
        size[0], size[1], size[2], 1, 1, 1, origin)


def cylinder(mesh, radius, height, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0),
             steps=32, capped=True):
    return PRIM.append_cylinder(
        mesh, prim_options(), xf(location, rotation),
        radius, height, steps, 1, capped,
        unreal.GeometryScriptPrimitiveOriginMode.BASE)


def append_indexed_surface(mesh, positions, normals, uvs, triangles):
    """Append an explicitly indexed surface with authored 0..1 UVs.

    Geometry Script's cylinder projection uses transform scale as its UV
    dimensions. That is useful for architecture, but it silently turned thin
    product sleeves into dozens of narrow texture islands. Printed packaging
    needs a deterministic seam, so these small meshes carry their UVs directly.
    """
    if EDITS is None:
        raise RuntimeError("Geometry Script basic-edit library is unavailable")

    buffers = unreal.GeometryScriptSimpleMeshBuffers()
    buffers.set_editor_property(
        "vertices", [unreal.Vector(*position) for position in positions])
    buffers.set_editor_property(
        "normals", [unreal.Vector(*normal) for normal in normals])
    buffers.set_editor_property(
        "uv0", [unreal.Vector2D(*uv) for uv in uvs])
    buffers.set_editor_property(
        "triangles", [unreal.IntVector(*triangle) for triangle in triangles])
    # Python omits the C++ output reference (NewTriangleIndicesList) from the
    # call signature and returns it in a tuple, so MaterialID is argument 3.
    EDITS.append_buffers_to_mesh(mesh, buffers, 0, False)
    return mesh


def append_wrapped_label_surface(mesh, rings, segments=64):
    """Build a single outward-facing shrink sleeve with one clean back seam.

    ``rings`` is a bottom-to-top sequence of ``(radius_cm, z_cm, v)``. A seam
    vertex is deliberately duplicated at U=0/1; this is what lets panoramic
    label artwork wrap exactly once without tearing across random triangles.

    U는 각도를 **거꾸로** 따라간다. 언리얼은 왼손 좌표계라 ``(cos, sin)``으로
    도는 것이 바깥에서 보면 시계 방향이고, 그대로 U를 매기면 밖에 선 사람
    기준으로 글자가 오른쪽에서 왼쪽으로 흐른다 — 인쇄면 전체가 좌우로
    뒤집힌다. 편의점 음료 라벨과 컵라면 띠가 실제로 그렇게 나오고 있었다.
    ``1 - u``로 뒤집으면 U=0.5(아트의 한가운데)는 제자리에 있으므로 호출부가
    잡아 둔 요각은 그대로 쓸 수 있고, 좌우만 바로 선다.
    """
    if len(rings) < 2:
        raise ValueError("A wrapped label needs at least two profile rings")

    positions = []
    normals = []
    uvs = []
    triangles = []
    ring_stride = segments + 1

    for ring_index, (radius, z, v) in enumerate(rings):
        if ring_index == 0:
            adjacent_radius, adjacent_z, _ = rings[1]
            radial_slope = (adjacent_radius - radius) / max(adjacent_z - z, 1.0e-4)
        elif ring_index == len(rings) - 1:
            previous_radius, previous_z, _ = rings[-2]
            radial_slope = (radius - previous_radius) / max(z - previous_z, 1.0e-4)
        else:
            previous_radius, previous_z, _ = rings[ring_index - 1]
            adjacent_radius, adjacent_z, _ = rings[ring_index + 1]
            radial_slope = (adjacent_radius - previous_radius) / max(
                adjacent_z - previous_z, 1.0e-4)

        normal_z = -radial_slope
        normal_length = math.sqrt(1.0 + normal_z * normal_z)
        for segment in range(segments + 1):
            u = segment / segments
            angle = math.tau * u
            cosine = math.cos(angle)
            sine = math.sin(angle)
            positions.append((cosine * radius, sine * radius, z))
            normals.append((cosine / normal_length, sine / normal_length,
                            normal_z / normal_length))
            uvs.append((1.0 - u, v))

    for ring_index in range(len(rings) - 1):
        lower = ring_index * ring_stride
        upper = (ring_index + 1) * ring_stride
        for segment in range(segments):
            lower_left = lower + segment
            lower_right = lower_left + 1
            upper_left = upper + segment
            upper_right = upper_left + 1
            triangles.append((lower_left, lower_right, upper_right))
            triangles.append((lower_left, upper_right, upper_left))

    return append_indexed_surface(mesh, positions, normals, uvs, triangles)


def sweep(mesh, polygon2d, path_points, steps_ignored=None):
    """Sweeps a 2D cross-section polygon along a 3D polyline path."""
    polygon = [unreal.Vector2D(x, y) for x, y in polygon2d]
    path = [xf(point) for point in path_points]
    return PRIM.append_sweep_polygon(
        mesh, prim_options(), xf(), polygon, path, False, False, 0.0, 0.0)


def ellipsoid(mesh, size, location=(0.0, 0.0, 0.0),
              rotation=(0.0, 0.0, 0.0), steps=24):
    """Appends a smooth ellipsoid with normalized UVs.

    A lathed half-circle is more stable across UE Python binding versions than
    the sphere convenience overload, and non-uniform transform scale gives the
    exact anatomical mass needed by the cat and other organic silhouettes.
    """
    profile = []
    rings = 14
    for ring in range(rings + 1):
        angle = -math.pi * 0.5 + math.pi * ring / rings
        profile.append((math.cos(angle) * 0.5, math.sin(angle) * 0.5))
    return revolve(
        mesh,
        profile,
        steps=steps,
        transform=xf(location, rotation, size),
        scale_to_fill=True)


def bevel_all(mesh, distance=0.35, angle_threshold=25.0):
    """Rounds every sharp edge — the single biggest 'not a cube' upgrade."""
    try:
        result = SELECT.select_mesh_sharp_edges(mesh, angle_threshold)
        selection = result[1] if isinstance(result, tuple) else result
        options = unreal.GeometryScriptMeshBevelSelectionOptions()
        options.set_editor_property("bevel_distance", distance)
        options.set_editor_property("subdivisions", 3)
        options.set_editor_property("round_weight", 1.0)
        MODEL.apply_mesh_bevel_edge_selection(mesh, selection, options)
    except Exception as error:  # noqa: BLE001
        log(f"  bevel skipped: {error}")
    return mesh


def subtract(mesh, tool_mesh):
    options = unreal.GeometryScriptMeshBooleanOptions()
    return BOOL_LIB.apply_mesh_boolean(
        mesh, xf(), tool_mesh, xf(),
        unreal.GeometryScriptBooleanOperation.SUBTRACT, options)


def union(mesh, part_mesh):
    options = unreal.GeometryScriptMeshBooleanOptions()
    return BOOL_LIB.apply_mesh_boolean(
        mesh, xf(), part_mesh, xf(),
        unreal.GeometryScriptBooleanOperation.UNION, options)


def recompute_normals(mesh, smooth_angle=45.0):
    if NORMALS is None:
        return mesh
    for candidate in ("recompute_normals", "compute_split_normals",
                      "set_mesh_to_face_normals"):
        function = getattr(NORMALS, candidate, None)
        if function is None:
            continue
        try:
            options = unreal.GeometryScriptCalculateNormalsOptions()
            return function(mesh, options)
        except Exception:  # noqa: BLE001
            continue
    return mesh


def fuse_shells(mesh, asset_name):
    """겹쳐 놓은 껍질들을 자가 합집합으로 한 장의 닫힌 표면에 녹인다.

    타원체·튜브를 그냥 append한 몸은 스침광 아래에서 부품이 서로 파고드는
    교차 타원 자국을 드러낸다 — 조립품이라는 자백이다. 합집합이 그 자국을
    지우고 덩어리를 위상적으로 잇는다. 실패하면 False를 돌려주고 메시는
    조립 상태 그대로 남는다.
    """
    function = getattr(BOOL_LIB, "apply_mesh_self_union", None)
    options_class = getattr(unreal, "GeometryScriptMeshSelfUnionOptions", None)
    if function is None or options_class is None:
        log(f"  {asset_name} self-union skipped (no binding)")
        return False
    try:
        function(mesh, options_class())
        return True
    except Exception as error:  # noqa: BLE001 - keep the assembled mesh
        log(f"  {asset_name} self-union failed: {error}")
        return False


def soften_shell_seams(mesh, asset_name, iterations=5, alpha=0.22):
    """합집합이 남긴 접합 능선을 몇 번의 스무딩으로 살처럼 잇는다."""
    function = getattr(DEFORM, "apply_iterative_smoothing_to_mesh", None) \
        if DEFORM is not None else None
    options_class = getattr(
        unreal, "GeometryScriptIterativeMeshSmoothingOptions", None)
    selection_class = getattr(unreal, "GeometryScriptMeshSelection", None)
    if function is None or options_class is None or selection_class is None:
        log(f"  {asset_name} seam smoothing skipped (no binding)")
        return
    options = options_class()
    _set_properties(options, (
        ("num_iterations", iterations),
        ("alpha", alpha),
    ))
    try:
        # 빈 선택은 EmptyBehavior 기본값대로 메시 전체다.
        function(mesh, selection_class(), options)
    except Exception as error:  # noqa: BLE001
        log(f"  {asset_name} seam smoothing failed: {error}")


def plaster_grain(mesh, asset_name, layers):
    """법선 방향 펄린 변위로 매끈한 마네킹 피부를 마른 석고로 바꾼다.

    layers는 (진폭 cm, 주파수 1/cm, 시드) 튜플이다. apply_perlin_noise_to_mesh
    무인자 이름은 주파수를 제곱하던 5.6 호환용이라 2가 붙은 쪽을 쓴다.
    """
    function = getattr(DEFORM, "apply_perlin_noise_to_mesh2", None) \
        if DEFORM is not None else None
    options_class = getattr(unreal, "GeometryScriptPerlinNoiseOptions", None)
    layer_class = getattr(unreal, "GeometryScriptPerlinNoiseLayerOptions", None)
    selection_class = getattr(unreal, "GeometryScriptMeshSelection", None)
    if (function is None or options_class is None or layer_class is None
            or selection_class is None):
        log(f"  {asset_name} plaster grain skipped (no binding)")
        return
    for magnitude, frequency, seed in layers:
        layer = layer_class()
        _set_properties(layer, (
            ("magnitude", magnitude),
            ("frequency", frequency),
            ("random_seed", seed),
        ))
        options = options_class()
        _set_properties(options, (
            ("base_layer", layer),
            ("apply_along_normal", True),
        ))
        try:
            function(mesh, selection_class(), options)
        except Exception as error:  # noqa: BLE001
            log(f"  {asset_name} plaster grain failed: {error}")
            return


def bake_vertex_occlusion(mesh, asset_name, occlusion_rays=64):
    """자기 그림자를 정점색으로 굽는다. R=1이 트인 면, 0이 골이다.

    재질의 cavity_dust가 이 값을 읽어 골에 분진을 앉히고 폐색을 심화한다.
    실패해도 그대로 두면 되는 폴백이다: 정점색 없는 메시는 흰색으로 읽혀
    분진이 정확히 0이 된다.
    """
    if BAKE is None:
        log(f"  {asset_name} vertex AO skipped (no bake library)")
        return False
    bake_function = getattr(BAKE, "bake_vertex", None)
    maker = getattr(BAKE, "make_bake_type_ambient_occlusion", None)
    target_options_class = getattr(
        unreal, "GeometryScriptBakeTargetMeshOptions", None)
    source_options_class = getattr(
        unreal, "GeometryScriptBakeSourceMeshOptions", None)
    output_class = getattr(unreal, "GeometryScriptBakeOutputType", None)
    vertex_options_class = getattr(
        unreal, "GeometryScriptBakeVertexOptions", None)
    if None in (bake_function, maker, target_options_class,
                source_options_class, output_class, vertex_options_class):
        log(f"  {asset_name} vertex AO skipped (incomplete bake bindings)")
        return False
    try:
        output = output_class()
        # output_mode 기본값 RGBA: AO 한 종이 네 채널에 같이 실린다.
        output.set_editor_property("rgba", maker(occlusion_rays))
        bake_function(
            mesh, xf(), target_options_class(),
            mesh, xf(), source_options_class(),
            output, vertex_options_class())
    except Exception as error:  # noqa: BLE001
        log(f"  {asset_name} vertex AO bake failed: {error}")
        return False

    has_colors = getattr(QUERIES, "get_has_vertex_colors", None) \
        if QUERIES is not None else None
    if has_colors is None or not has_colors(mesh):
        log(f"  {asset_name} vertex AO produced no colors")
        return False
    # 굽기가 흰 판때기로 끝나지 않았는지 표본으로 확인해 로그에 남긴다.
    # 이 최소/평균이 곧 골 깊이의 감사 기록이다. 삼각형 ID 공간에는 구멍이
    # 있을 수 있어 valid 플래그가 거르게 둔다.
    try:
        id_space = _triangle_count(mesh) or 0
        samples = []
        for triangle_id in range(0, id_space, max(1, id_space // 96)):
            result = QUERIES.get_triangle_vertex_colors(mesh, triangle_id)
            colors, valid = result[1:4], result[4]
            if valid:
                samples.extend(color.r for color in colors)
        if samples:
            log(f"  {asset_name} vertex AO min={min(samples):.3f} "
                f"avg={sum(samples) / len(samples):.3f} over {len(samples)}")
    except Exception:  # noqa: BLE001 - stats are a report, not a gate
        pass
    return True


# The budgets, the reduction curve and the class of every mesh live in
# mesh_lod_contract so the release validator checks the same numbers the bake
# applied. HERO_MESHES is re-exported because callers already import it here.
HERO_MESHES = mesh_lod_contract.HERO_MESHES


def _triangle_count(mesh):
    """5.8에서 get_num_triangles가 사라져 폴백 사슬로 센다.

    get_num_triangle_i_ds는 구멍을 포함한 ID 공간이라 실제보다 클 수 있는데,
    예산 판정에는 과대평가가 안전한 방향이다: 예산 안이라고 잘못 믿는 대신
    단순화를 한 번 더 돌게 된다.
    """
    for holder, name in (
        (QUERIES, "get_num_triangles"),
        (mesh, "get_triangle_count"),
        (QUERIES, "get_num_triangle_i_ds"),
    ):
        function = getattr(holder, name, None) if holder is not None else None
        if function is None:
            continue
        try:
            return function(mesh) if holder is not mesh else function()
        except Exception:  # noqa: BLE001 - try the next binding
            continue
    return None


def retopologise(mesh, asset_name):
    """Brings LOD0 inside its triangle budget before the asset is created.

    These meshes are booleans of lathes and sweeps: the profile resolution that
    makes a bottle shoulder read at 30 cm survives into the far LOD as tens of
    thousands of triangles that never cover a pixel. Simplification runs on the
    dynamic mesh, before the static mesh exists, so the collision hull and the
    LOD chain are both derived from the budgeted geometry rather than from a
    density nobody chose.

    Meshes carrying hand-placed printed artwork keep their UV seams: a label
    band welded across its seam smears the artwork around the bottle.
    """
    if SIMPLIFY is None:
        log(f"  {asset_name} retopology skipped (no simplification library)")
        return None

    budget = mesh_lod_contract.triangle_budget(asset_name)
    before = _triangle_count(mesh)
    if before is None:
        log(f"  {asset_name} retopology skipped (no triangle count binding)")
        return None
    if before <= budget:
        return before

    options = None
    for factory in ("GeometryScriptSimplifyMeshOptions",):
        options_class = getattr(unreal, factory, None)
        if options_class is not None:
            options = options_class()
            break
    if options is not None:
        for name, value in (
            ("method", getattr(
                getattr(unreal, "EGeometryScriptRemoveMeshSimplificationType", None),
                "QEM", None)),
            ("auto_compact", True),
        ):
            if value is None:
                continue
            try:
                options.set_editor_property(name, value)
            except Exception:  # noqa: BLE001
                pass

    for candidate in ("apply_simplify_to_triangle_count",
                      "apply_simplify_to_polygon_count"):
        function = getattr(SIMPLIFY, candidate, None)
        if function is None:
            continue
        try:
            if options is not None:
                function(mesh, budget, options)
            else:
                function(mesh, budget)
            after = _triangle_count(mesh)
            log(f"  {asset_name} retopology {before} -> {after} tris "
                f"(budget {budget})")
            return after
        except Exception as error:  # noqa: BLE001 - report, keep the mesh
            log(f"  {asset_name} simplification failed: {error}")
            return before
    log(f"  {asset_name} retopology skipped (no usable simplify entry point)")
    return before


def _set_properties(target, values):
    applied = 0
    for name, value in values:
        try:
            target.set_editor_property(name, value)
            applied += 1
        except Exception:  # noqa: BLE001 - property names drift between minors
            pass
    return applied


def apply_lod_contract(static_mesh, asset_name):
    """Builds the authored LOD chain and the lightmap channel.

    Every mesh gets the same treatment, hero props included. "Story-critical
    close inspection" is a reason to give LOD0 a generous budget, not a reason
    to render the full-density mesh from across a room — which is what
    exempting them from LODs entirely had been doing.
    """
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        log(f"  {asset_name} LOD chain skipped (no StaticMeshEditorSubsystem)")
        return

    mesh_class = mesh_lod_contract.classify(asset_name)
    plan = mesh_lod_contract.lod_plan(asset_name)

    reduction_options = getattr(unreal, "StaticMeshReductionOptions", None)
    reduction_settings = getattr(unreal, "StaticMeshReductionSettings", None)
    if reduction_options is not None and reduction_settings is not None:
        settings = []
        # set_lods는 넘긴 배열을 LOD0부터의 전체 목록으로 읽는다. 체인만
        # 넘기면 LOD0까지 첫 항목의 비율로 깎인 채 LOD가 하나 모자라게 된다.
        # 생성 메시는 retopologise가 이미 예산을 맞췄으므로 LOD0은 100%다.
        lod0 = reduction_settings()
        _set_properties(lod0, (
            ("percent_triangles", 1.0),
            ("screen_size", 1.0),
        ))
        settings.append(lod0)
        for _, percent, screen in plan:
            entry = reduction_settings()
            _set_properties(entry, (
                ("percent_triangles", percent),
                ("screen_size", screen),
            ))
            settings.append(entry)
        options = reduction_options()
        _set_properties(options, (
            ("reduction_settings", settings),
            ("auto_compute_lod_screen_size", False),
        ))
        try:
            subsystem.set_lods(static_mesh, options)
            log(f"  {asset_name} LOD chain={mesh_class.lod_count} "
                f"class={mesh_class.name}")
        except Exception as error:  # noqa: BLE001
            log(f"  {asset_name} LOD chain failed: {error}")
    else:
        # Fall back to the engine group rather than shipping a single LOD.
        group = "LargeProp" if mesh_class is mesh_lod_contract.LARGE else "SmallProp"
        try:
            subsystem.set_lod_group(static_mesh, group, True)
            log(f"  {asset_name} LOD group={group} (no reduction bindings)")
        except Exception as error:  # noqa: BLE001
            log(f"  LOD group skipped for {asset_name}: {error}")

    # Static props in a Lumen scene still bake a lightmap channel; without one
    # the packer falls back to UV0 and the printed artwork bleeds into it.
    build_settings = getattr(unreal, "MeshBuildSettings", None)
    if build_settings is not None:
        settings = build_settings()
        _set_properties(settings, (
            ("recompute_normals", False),
            ("recompute_tangents", True),
            ("use_mikk_t_space", True),
            ("remove_degenerates", True),
            ("generate_lightmap_u_vs", True),
            ("src_lightmap_index", 0),
            ("dst_lightmap_index", 1),
            ("min_lightmap_resolution", mesh_class.lightmap_resolution),
        ))
        try:
            subsystem.set_lod_build_settings(static_mesh, 0, settings)
        except Exception as error:  # noqa: BLE001
            log(f"  {asset_name} lightmap UV settings skipped: {error}")
    _set_properties(static_mesh, (
        ("light_map_resolution", mesh_class.lightmap_resolution),
        ("light_map_coordinate_index", 1),
    ))


def bake(mesh, asset_name, add_collision=True, weld_edges=True):
    """Bakes the dynamic mesh into /Game/Meshes/<asset_name>."""
    if weld_edges:
        weld_options = unreal.GeometryScriptWeldEdgesOptions()
        weld_options.set_editor_property("tolerance", 0.01)
        weld_options.set_editor_property("only_unique_pairs", False)
        REPAIR.weld_mesh_edges(mesh, weld_options)
    if not mesh_lod_contract.preserves_uv_seams(asset_name):
        retopologise(mesh, asset_name)
    recompute_normals(mesh)

    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    for name, value in (("enable_recompute_normals", False),
                        ("enable_recompute_tangents", True),
                        ("enable_nanite", False)):
        try:
            options.set_editor_property(name, value)
        except Exception:  # noqa: BLE001
            pass

    asset_path = f"{MESH_ROOT}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    result = NEW_ASSET.create_new_static_mesh_asset_from_mesh(mesh, asset_path, options)
    static_mesh = result[0] if isinstance(result, tuple) else result
    if static_mesh is None:
        raise RuntimeError(f"Failed to create {asset_path}")

    if add_collision:
        try:
            collision_options = unreal.GeometryScriptCollisionFromMeshOptions()
            try:
                collision_options.set_editor_property(
                    "method",
                    unreal.GeometryScriptCollisionGenerationMethod.CONVEX_HULLS)
            except Exception:  # noqa: BLE001
                pass
            COLLISION.set_static_mesh_collision_from_mesh(
                mesh, static_mesh, collision_options)
        except Exception as error:  # noqa: BLE001
            log(f"  collision skipped for {asset_name}: {error}")

    apply_lod_contract(static_mesh, asset_name)

    if QUERIES is not None:
        try:
            bounds = QUERIES.get_mesh_bounding_box(mesh)
            log(f"  {asset_name} bounds min={bounds.min} max={bounds.max}")
        except Exception:  # noqa: BLE001
            pass

    unreal.EditorAssetLibrary.save_asset(asset_path, False)
    log(f"built {asset_path}")
    return static_mesh


# ---------------------------------------------------------------------------
# The props
# ---------------------------------------------------------------------------

def build_water_bottle():
    """500 mL PET bottle with a straight real-world label zone."""
    mesh = new_mesh()
    profile = [
        (0.0, 0.0), (2.45, 0.0), (3.08, 0.45), (3.25, 1.10),
        (3.25, 1.80), (3.06, 2.15), (3.25, 2.55),
        (3.06, 3.35), (3.25, 3.75), (3.06, 4.55), (3.25, 4.95),
        (3.08, 5.75), (3.25, 6.20),
        # Commercial wrap labels sit on a straight band rather than bridging
        # deep grip grooves, so the film can contact the PET all the way round.
        (3.25, 7.15), (3.25, 12.85), (3.22, 14.10),
        (2.78, 15.25), (2.02, 16.65), (1.5, 17.9),
        (1.42, 18.4), (1.6, 18.7), (1.42, 19.0), (1.6, 19.3),
        (1.42, 19.6), (1.68, 20.0), (1.5, 20.3), (0.0, 20.3),
    ]
    revolve(mesh, profile, steps=48)
    return bake(mesh, "SM_WaterBottle")


def build_bottle_cap():
    mesh = new_mesh()
    profile = [(0.0, 0.0), (1.62, 0.0), (1.7, 0.25), (1.7, 1.35),
               (1.55, 1.6), (0.0, 1.6)]
    revolve(mesh, profile, steps=32)
    return bake(mesh, "SM_BottleCap")


def build_soju_bottle():
    """360 mL soju: straight shoulder-less body and a long slim neck."""
    mesh = new_mesh()
    profile = [
        (0.0, 0.0), (3.1, 0.0), (3.35, 0.5), (3.35, 12.6),
        (3.2, 13.6), (2.3, 15.4), (1.5, 17.0), (1.28, 18.6),
        (1.28, 20.4), (1.5, 20.8), (1.35, 21.2), (0.0, 21.2),
    ]
    revolve(mesh, profile, steps=40)
    return bake(mesh, "SM_SojuBottle")


def build_cup_noodle():
    """Tapered cup ramyeon with a rolled rim and a domed foil lid."""
    mesh = new_mesh()
    profile = [
        (0.0, 0.0), (3.6, 0.0), (3.9, 0.6), (4.6, 5.0),
        (5.35, 9.6), (5.6, 10.2), (5.35, 10.7), (5.1, 10.4),
        (4.4, 5.2), (3.7, 0.9), (0.0, 0.9),
    ]
    revolve(mesh, profile, steps=40, scale_to_fill=True)
    lid = new_mesh()
    revolve(lid, [(0.0, 10.5), (5.3, 10.4), (5.5, 10.7), (0.0, 11.1)], steps=40)
    options = unreal.GeometryScriptMeshBooleanOptions()
    BOOL_LIB.apply_mesh_boolean(
        mesh, xf(), lid, xf(),
        unreal.GeometryScriptBooleanOperation.UNION, options)
    return bake(mesh, "SM_CupNoodle")


def build_cup_sleeve():
    """Tapered label band that wraps the cup ramyeon wall.

    The cup is a cone, so a straight cylinder sleeve would either float off
    the wall at the bottom or cut into it at the top. This matches the cup
    profile's radii (3.95 at the base of the printed area, 5.30 just under
    the rim) with a hair of clearance, and carries its own cylindrical UVs so
    the label art wraps once around without shearing.

    The alternative — putting the label material on the cup mesh itself —
    is what was there before, and it painted the foil lid and the underside
    with the ramyeon artwork too, because the lid is unioned into the same
    mesh and therefore shares material slot 0.
    """
    mesh = new_mesh()
    # Real centimetres, matching the cup wall from local Z 1.6 to 8.8. The
    # roughly 1 mm radial clearance is deliberate: the old sub-millimetre gap
    # collapsed in the depth buffer and let the opaque foam cup erase its own
    # printed film. V=1 is the bottom; V=0 is the top of the artwork.
    append_wrapped_label_surface(
        mesh,
        [(4.18, 0.0, 1.0), (5.36, 7.2, 0.0)],
        segments=64)
    return bake(
        mesh, "SM_CupSleeve", add_collision=False, weld_edges=False)


def build_cup_lid():
    # Kept deliberately shallow and a hair above the cup's own domed top: it
    # is a foil colour pass over geometry that already exists, not a second
    # lid. Overlapping the two produced a black ring at grazing angles.
    """Foil lid disc for the cup ramyeon, with the rolled-over crimp edge.

    Sits on top of the cup as its own prop so it can carry a foil material
    instead of the label.
    """
    mesh = new_mesh()
    profile = [
        (0.0, 10.74), (5.12, 10.70), (5.46, 10.84),
        (5.40, 10.98), (5.02, 10.90), (0.0, 10.94),
    ]
    revolve(mesh, profile, steps=48)
    return bake(mesh, "SM_CupLid", add_collision=False)


def build_kimchi_tub():
    mesh = new_mesh()
    profile = [
        (0.0, 0.0), (7.2, 0.0), (7.6, 0.5), (8.6, 10.0),
        (8.9, 11.0), (9.2, 11.4), (9.2, 12.4), (8.7, 12.6),
        (8.4, 11.6), (8.1, 10.2), (7.1, 0.9), (0.0, 0.9),
    ]
    revolve(mesh, profile, steps=44)
    return bake(mesh, "SM_KimchiTub")


def build_milk_carton():
    """Gable-top carton: extruded house profile stood upright."""
    mesh = new_mesh()
    polygon = [(-3.5, 0.0), (3.5, 0.0), (3.5, 14.0), (0.0, 18.4), (-3.5, 14.0)]
    points = [unreal.Vector2D(x, y) for x, y in polygon]
    PRIM.append_simple_extrude_polygon(
        mesh, prim_options(), xf(rotation=(-90.0, 0.0, 0.0)),
        points, 7.0, 1, True,
        unreal.GeometryScriptPrimitiveOriginMode.BASE)
    bevel_all(mesh, distance=0.18)
    return bake(mesh, "SM_MilkCarton")


def build_lever_handle():
    """Door lever on a rose plate, swept as a real tube."""
    mesh = new_mesh()
    profile = [(0.0, 0.0), (3.4, 0.0), (3.6, 0.4), (3.6, 1.4),
               (3.2, 1.8), (0.0, 1.8)]
    revolve(mesh, profile, steps=28)
    neck = new_mesh()
    cylinder(neck, 1.5, 3.4, location=(0.0, 0.0, 1.4))
    options = unreal.GeometryScriptMeshBooleanOptions()
    BOOL_LIB.apply_mesh_boolean(
        mesh, xf(), neck, xf(),
        unreal.GeometryScriptBooleanOperation.UNION, options)
    lever = new_mesh()
    section = [(-1.05, -0.85), (1.05, -0.85), (1.25, 0.0), (1.05, 0.85), (-1.05, 0.85)]
    path = [(0.0, 0.0, 4.4), (0.0, -2.6, 4.7), (0.0, -6.6, 5.0),
            (0.0, -10.4, 4.4), (0.0, -12.2, 3.2)]
    sweep(lever, section, path)
    BOOL_LIB.apply_mesh_boolean(
        mesh, xf(), lever, xf(),
        unreal.GeometryScriptBooleanOperation.UNION, options)
    return bake(mesh, "SM_LeverHandle")


def build_stool():
    """Round bar stool: dished seat, tapered column, weighted disc base."""
    mesh = new_mesh()
    profile = [
        (0.0, 0.0), (17.0, 0.0), (17.5, 1.4), (16.0, 2.2),
        (4.0, 2.6), (3.2, 8.0), (3.0, 44.0), (5.5, 46.0),
        (14.0, 47.2), (15.5, 49.0), (15.0, 51.6), (12.0, 52.6),
        (0.0, 52.8),
    ]
    revolve(mesh, profile, steps=44)
    return bake(mesh, "SM_Stool")


def build_basket():
    """Shopping basket: tapered shell hollowed out, with a rolled rim."""
    mesh = new_mesh()
    box(mesh, (40.0, 30.0, 22.0), location=(0.0, 0.0, 11.0))
    bevel_all(mesh, distance=1.4)
    cavity = new_mesh()
    box(cavity, (36.0, 26.0, 20.0), location=(0.0, 0.0, 13.0))
    bevel_all(cavity, distance=1.0)
    subtract(mesh, cavity)
    return bake(mesh, "SM_Basket")


def build_label_sleeve():
    """Open unit cylinder used as a wrap-around label band on bottles.

    Radius 1 / height 1 so the scene can scale it per bottle; UVs run around
    the circumference, which is exactly how the label art is authored.
    """
    mesh = new_mesh()
    append_wrapped_label_surface(
        mesh,
        [(1.0, 0.0, 1.0), (1.0, 1.0, 0.0)],
        segments=64)
    return bake(
        mesh, "SM_LabelSleeve", add_collision=False, weld_edges=False)


def build_sticky_note_76mm():
    """A 76 mm adhesive note: top edge flush, lower edge barely released."""
    mesh = new_mesh()
    columns = 6
    rows = 8
    width = 7.6
    height = 7.6
    positions = []
    normals = []
    uvs = []
    triangles = []

    for row in range(rows + 1):
        v = row / rows
        release = max(0.0, (v - 0.72) / 0.28)
        # Only the unglued lower 21 mm lifts, peaking at 1.8 mm. This remains
        # a memo sheet, not the thick card that the old cube implied.
        outward_x = -0.18 * release * release
        for column in range(columns + 1):
            u = column / columns
            y = (u - 0.5) * width
            z = (0.5 - v) * height
            positions.append((outward_x, y, z))
            normals.append((-1.0, 0.0, 0.0))
            uvs.append((u, v))

    stride = columns + 1
    for row in range(rows):
        for column in range(columns):
            top_left = row * stride + column
            top_right = top_left + 1
            bottom_left = top_left + stride
            bottom_right = bottom_left + 1
            triangles.append((top_left, top_right, bottom_right))
            triangles.append((top_left, bottom_right, bottom_left))

    append_indexed_surface(mesh, positions, normals, uvs, triangles)
    return bake(
        mesh, "SM_StickyNote76mm", add_collision=False, weld_edges=False)


def build_capture_mercy_note():
    """복도 방향 끝만 살짝 든 18×11cm 낱장 메모를 만든다."""
    mesh = new_mesh()
    columns = 8
    rows = 6
    width = 18.0
    depth = 11.0
    positions = []
    normals = []
    uvs = []
    triangles = []

    for row in range(rows + 1):
        v = row / rows
        leading_release = max(0.0, (v - 0.76) / 0.24)
        for column in range(columns + 1):
            u = column / columns
            x = (u - 0.5) * width
            y = (0.5 - v) * depth
            # 종이 두께만큼 틈을 두어 Z-fighting을 막고, 끝 26mm만 들어
            # 코팅지처럼 보이지 않으면서 복도 빛을 받게 한다.
            side_wave = 0.025 * math.sin(u * math.pi * 2.0)
            z = 0.06 + side_wave + 0.22 * leading_release * leading_release
            positions.append((x, y, z))
            normals.append((0.0, 0.0, 1.0))
            # 플레이어가 +Y를 보면 카메라 오른쪽은 월드 -X가 된다.
            # U만 뒤집어 필기는 좌우가 바로 보이고, 든 끝은 실제 종이의
            # 가까운 면에 그대로 남게 한다.
            uvs.append((1.0 - u, v))

    stride = columns + 1
    for row in range(rows):
        for column in range(columns):
            top_left = row * stride + column
            top_right = top_left + 1
            bottom_left = top_left + stride
            bottom_right = bottom_left + 1
            triangles.append((top_left, bottom_left, bottom_right))
            triangles.append((top_left, bottom_right, top_right))

    append_indexed_surface(mesh, positions, normals, uvs, triangles)
    return bake(
        mesh, "SM_CaptureMercyNote", add_collision=False, weld_edges=False)


def build_alley_cat_run():
    """One pooled mackerel-tabby in a cautious low run.

    The ImageGen four-view sheet fixes the adult proportions and coat identity.
    This single disconnected-shell static mesh replaces six engine primitives;
    no skeletal animation pipeline is introduced for a 0.9--1.35 second trace.
    """
    mesh = new_mesh()

    # Torso, chest, neck and head preserve the lean Korean-alley-cat profile.
    ellipsoid(mesh, (49.0, 16.0, 18.0), location=(-2.0, 0.0, 25.0))
    ellipsoid(mesh, (23.0, 17.0, 24.0), location=(17.0, 0.0, 28.0))
    ellipsoid(mesh, (12.0, 13.0, 13.0), location=(22.0, 0.0, 36.0))
    ellipsoid(mesh, (17.0, 14.5, 15.5), location=(29.0, 0.0, 40.5))
    ellipsoid(mesh, (8.5, 12.0, 7.0), location=(36.0, 0.0, 37.5))

    # Side-readable triangular ears. A shallow extrusion gives them enough
    # thickness for oblique alley views without turning them into cones.
    ear_polygon = [
        unreal.Vector2D(-3.2, 0.0),
        unreal.Vector2D(3.2, 0.0),
        unreal.Vector2D(0.5, 9.0),
    ]
    for ear_x, ear_y in ((25.0, -4.0), (32.0, 0.2)):
        PRIM.append_simple_extrude_polygon(
            mesh,
            prim_options(scale_to_fill=True),
            xf(location=(ear_x, ear_y, 46.0), rotation=(-90.0, 0.0, 0.0)),
            ear_polygon,
            4.0,
            1,
            True,
            unreal.GeometryScriptPrimitiveOriginMode.BASE)

    limb_section = []
    for index in range(8):
        angle = math.tau * index / 8.0
        limb_section.append((math.cos(angle) * 2.45, math.sin(angle) * 2.45))
    leg_paths = (
        [(18.0, -3.0, 24.0), (23.0, -3.0, 11.0), (31.0, -3.0, 3.0)],
        [(13.0, 3.0, 23.0), (9.0, 3.0, 11.0), (3.0, 3.0, 4.0)],
        [(-18.0, -3.0, 23.0), (-13.0, -3.0, 10.0), (-5.0, -3.0, 3.0)],
        [(-14.0, 3.0, 23.0), (-24.0, 3.0, 13.0), (-31.0, 3.0, 5.0)],
    )
    for path in leg_paths:
        sweep(mesh, limb_section, path)
        foot = path[-1]
        ellipsoid(
            mesh,
            (8.5, 5.5, 3.2),
            location=(foot[0] + 2.0, foot[1], 1.6),
            steps=16)

    tail_section = []
    for index in range(10):
        angle = math.tau * index / 10.0
        tail_section.append((math.cos(angle) * 2.2, math.sin(angle) * 2.2))
    sweep(
        mesh,
        tail_section,
        [(-25.0, 0.0, 29.0), (-36.0, 0.5, 27.0),
         (-45.0, 1.5, 22.0), (-52.0, 2.0, 20.0)])
    return bake(mesh, "SM_AlleyCatRun", add_collision=False)


def _append_round_path(mesh, radius, path, sides=12):
    """Appends one soft clothing tube without exposing a skin joint."""
    section = []
    for index in range(sides):
        angle = math.tau * index / sides
        section.append((math.cos(angle) * radius, math.sin(angle) * radius))
    sweep(mesh, section, path)


def build_listener_entity_crawl():
    """Anatomical static shell for 「없는 층」의 위층 사람.

    2026-09-08부터 정식 에셋은 여기가 아니라 TRELLIS.2 생성 → Blender 다듬기
    경로(Scripts/generate_3d_comfy.py, Scripts/blender/refine_generated.py)다.
    이 빌더는 생성 산출물이 없을 때의 폴백이며 Content/Meshes의
    SM_ListenerEntityCrawl을 덮어쓰지 않도록 아트 빌드에서 부르지 않는다.
    같은 사정이 SM_AlleyCatRun, SM_MokHansoo*, SM_FinalCavity*에도 있다.

    The approved ImageGen sheet fixes a real 176 cm adult in a forearm-supported
    crawl.  This mesh deliberately remains one frozen pose: the pawn moves as a
    whole, so a skeletal pipeline would add cost without improving the
    silhouette seen in the flashlight.

    조립 자체는 예전 그대로 타원체와 튜브지만, 완성은 조립품이 아니다:
    자가 합집합이 부품을 한 장의 닫힌 표면으로 녹이고, 스무딩이 접합 능선을
    살로 잇고, 펄린 결이 매끈한 마네킹 피부를 마른 석고로 바꾼다. 끝으로
    정점 AO를 구워 골에 분진이 앉을 자리(재질의 cavity_dust)를 남긴다.
    """
    mesh = new_mesh()

    # Ribcage, abdomen and pelvis overlap just enough to stay human while
    # retaining shallow waist/shoulder notches under grazing light.
    ellipsoid(mesh, (58.0, 38.0, 27.0), location=(13.0, 0.0, 1.0),
              rotation=(0.0, 0.0, -8.0), steps=36)
    ellipsoid(mesh, (38.0, 32.0, 23.0), location=(-21.0, 0.0, -8.0),
              rotation=(0.0, 0.0, -3.0), steps=32)
    ellipsoid(mesh, (48.0, 17.0, 14.0), location=(24.0, 0.0, 9.0),
              rotation=(0.0, 0.0, -7.0), steps=28)

    # A real neck bridge prevents the old floating ball-head read.  The face
    # is one smooth plaster ellipsoid: no eye, mouth, nose or separate hair.
    _append_round_path(
        mesh, 7.0,
        [(36.0, 0.0, 12.0), (46.0, 0.0, 22.0)], sides=16)
    ellipsoid(mesh, (23.0, 20.0, 28.0), location=(57.0, 0.0, 27.0),
              rotation=(0.0, 0.0, -8.0), steps=36)
    ellipsoid(mesh, (5.0, 18.5, 21.0), location=(68.2, 0.0, 25.5),
              rotation=(0.0, 0.0, -8.0), steps=28)

    # Both arms carry weight from shoulder through elbow to the floor.  Each
    # radius narrows at the anatomical joint instead of ending in a cube.
    arm_paths = (
        (7.2, [(28.0, -17.0, 6.0), (43.0, -25.0, -10.0)]),
        (5.8, [(43.0, -25.0, -10.0), (66.0, -25.0, -20.0)]),
        (7.2, [(28.0, 17.0, 6.0), (43.0, 25.0, -10.0)]),
        (5.8, [(43.0, 25.0, -10.0), (66.0, 25.0, -20.0)]),
    )
    for radius, path in arm_paths:
        _append_round_path(mesh, radius, path, sides=16)

    # Broken legs trail with different, restrained bends. They remain clothed
    # and continuous from pelvis to foot; no gore or dislocated fantasy pose.
    leg_paths = (
        (9.2, [(-28.0, -10.0, -9.0), (-57.0, -18.0, -18.0)]),
        (6.8, [(-57.0, -18.0, -18.0), (-98.0, -22.0, -27.0)]),
        (9.2, [(-28.0, 10.0, -8.0), (-61.0, 14.0, -16.0)]),
        (6.8, [(-61.0, 14.0, -16.0), (-103.0, 6.0, -29.0)]),
    )
    for radius, path in leg_paths:
        _append_round_path(mesh, radius, path, sides=18)
    ellipsoid(mesh, (20.0, 17.0, 13.0), location=(-57.0, -18.0, -18.0),
              rotation=(0.0, 8.0, 0.0), steps=24)
    ellipsoid(mesh, (20.0, 17.0, 13.0), location=(-61.0, 14.0, -16.0),
              rotation=(0.0, -8.0, 0.0), steps=24)
    ellipsoid(mesh, (25.0, 12.0, 8.0), location=(-110.0, -22.0, -29.0),
              rotation=(0.0, 2.0, 0.0), steps=24)
    ellipsoid(mesh, (25.0, 12.0, 8.0), location=(-115.0, 5.0, -31.0),
              rotation=(0.0, -7.0, 0.0), steps=24)

    # 손과 손가락이 오기 전에 큰 덩어리를 먼저 녹이고 결을 얹는다. 손가락은
    # 반지름 1.15에 서로 0.1cm 간격이라, 결 변위를 함께 받으면 이웃끼리
    # 붙어 벙어리장갑이 된다. 큰 덩어리에만 결을 얹고 손은 매끈하게 남기는
    # 배치는 석고가 손끝에서 매끄럽게 굳은 것처럼도 읽힌다.
    fused = fuse_shells(mesh, "SM_ListenerEntityCrawl")
    if fused:
        soften_shell_seams(mesh, "SM_ListenerEntityCrawl")
        plaster_grain(mesh, "SM_ListenerEntityCrawl", (
            (0.5, 0.021, 7),    # 넓은 굴곡: 어깨 폭 단위의 낮은 융기
            (0.16, 0.11, 23),   # 잔 결: 손전등 스침광에 걸리는 마른 요철
        ))

    for side in (-1.0, 1.0):
        hand_y = side * 25.0
        ellipsoid(mesh, (18.0, 11.0, 5.0), location=(74.0, hand_y, -21.0),
                  rotation=(0.0, 0.0, 0.0), steps=24)
        # Long tuner fingers remain closed plaster geometry. Their unequal
        # lengths keep the hand from reading as a mitten at capture distance.
        for finger_index, (finger_y, finger_length) in enumerate((
                (-3.6, 10.5), (-1.2, 12.0), (1.2, 11.4), (3.6, 9.4))):
            y = hand_y + side * finger_y
            z = -21.8 + (finger_index % 2) * 0.25
            _append_round_path(
                mesh, 1.15,
                [(78.0, y, z), (78.0 + finger_length, y, z - 0.35)],
                sides=10)

    if fused:
        # 두 번째 합집합이 손목-손-손가락을 팔에 위상으로 잇는다.
        fuse_shells(mesh, "SM_ListenerEntityCrawl")

    # 예산까지 먼저 줄이고 나서 정점 AO를 굽는다. 순서가 반대면 단순화가
    # 구운 색을 다시 뭉갠다. bake()는 예산 안 메시의 재단순화를 건너뛴다.
    retopologise(mesh, "SM_ListenerEntityCrawl")
    recompute_normals(mesh)
    bake_vertex_occlusion(mesh, "SM_ListenerEntityCrawl")

    # 합집합이 끝난 표면에 용접을 다시 돌리면 정점색 오버레이만 다칠 수
    # 있어, 융합에 성공했을 때는 끈다.
    return bake(mesh, "SM_ListenerEntityCrawl", add_collision=False,
                weld_edges=not fused)


def build_final_cavity_clothing_shell():
    """Dry clothing-led human volume for the night-four cavity reveal.

    The ImageGen turntable fixes a 176 cm adult folded into a 120 cm stud bay.
    Clothing carries the silhouette; the separate bone insert only occupies the
    open jacket line.  This is one close-range authored static asset, never a
    corpse card or a stack of runtime engine primitives.
    """
    mesh = new_mesh()

    # Split jacket panels leave a narrow, irregular opening for the rib insert.
    # Their overlap at shoulder and waist keeps the body volume continuous.
    for side in (-1.0, 1.0):
        ellipsoid(
            mesh, (30.0, 38.0, 72.0),
            location=(2.0, side * 18.0, 108.0),
            rotation=(0.0, side * 3.0, side * -5.0), steps=30)
    ellipsoid(mesh, (31.0, 75.0, 20.0), location=(1.0, 0.0, 137.0),
              rotation=(0.0, 0.0, -2.0), steps=30)
    ellipsoid(mesh, (31.0, 63.0, 39.0), location=(4.0, 0.0, 72.0),
              rotation=(0.0, 0.0, 2.0), steps=28)

    # Sleeves settle across the abdomen instead of hanging as detached rods.
    arm_paths = (
        [(1.0, -30.0, 128.0), (-5.0, -35.0, 98.0),
         (-12.0, -10.0, 77.0)],
        [(1.0, 30.0, 127.0), (-7.0, 34.0, 101.0),
         (-13.0, 11.0, 79.0)],
    )
    for path in arm_paths:
        _append_round_path(mesh, 8.2, path, sides=16)
        wrist = path[-1]
        ellipsoid(mesh, (15.0, 12.0, 9.0), location=wrist, steps=18)

    # Both legs fold tightly under the pelvis.  Continuous swept trouser legs
    # and compressed knees preserve the pelvis->knee->ankle read in silhouette.
    leg_paths = (
        [(5.0, -18.0, 73.0), (-2.0, -34.0, 49.0),
         (-8.0, -43.0, 18.0)],
        [(6.0, 18.0, 72.0), (0.0, 35.0, 48.0),
         (-6.0, 42.0, 17.0)],
    )
    for path in leg_paths:
        _append_round_path(mesh, 12.0, path, sides=18)
        knee = path[1]
        ankle = path[-1]
        ellipsoid(mesh, (27.0, 24.0, 23.0), location=knee, steps=22)
        ellipsoid(mesh, (32.0, 18.0, 12.0),
                  location=(ankle[0] - 7.0, ankle[1], 9.0),
                  rotation=(0.0, -4.0, 0.0), steps=22)

    # Compression folds are geometry so grazing flashlight light remains live.
    for location, yaw, length in (
            ((-14.0, -20.0, 118.0), 18.0, 20.0),
            ((-14.0, 20.0, 113.0), -20.0, 19.0),
            ((-15.0, -20.0, 62.0), 34.0, 17.0),
            ((-15.0, 21.0, 59.0), -31.0, 16.0)):
        ellipsoid(mesh, (3.0, length, 2.1), location=location,
                  rotation=(0.0, yaw, 0.0), steps=16)
    return bake(mesh, "SM_FinalCavityClothingShell", add_collision=False)


def build_final_cavity_bone_insert():
    """Restrained skull-and-rib insert, dry and non-graphic by construction."""
    mesh = new_mesh()

    # A proportioned skull with shallow eye cavities. No skin, hair, teeth fan,
    # wet surface or gore is authored into this release prop.
    ellipsoid(mesh, (23.0, 20.0, 27.0), location=(-4.0, 0.0, 161.0),
              rotation=(0.0, 0.0, -8.0), steps=36)
    for eye_y in (-5.2, 5.2):
        cutter = new_mesh()
        ellipsoid(cutter, (9.0, 6.0, 7.0),
                  location=(-14.0, eye_y, 164.0), steps=20)
        subtract(mesh, cutter)
    ellipsoid(mesh, (12.0, 14.0, 8.0), location=(-7.0, 0.0, 148.0),
              rotation=(0.0, 0.0, -5.0), steps=20)

    # Five paired ribs curve from the sternum into the clothing opening. The
    # jacket hides their endpoints, avoiding the assembled classroom-skeleton
    # read while retaining real chest proportions.
    _append_round_path(mesh, 1.35,
                       [(-15.0, 0.0, 137.0), (-15.0, 0.0, 101.0)], sides=12)
    for rib_index in range(5):
        z = 133.0 - rib_index * 7.0
        reach = 19.0 + rib_index * 1.6
        for side in (-1.0, 1.0):
            _append_round_path(
                mesh,
                1.15,
                [(-15.0, side * 1.5, z),
                 (-16.5, side * reach * 0.60, z - 2.0),
                 (-9.0, side * reach, z - 6.0)],
                sides=10)
    return bake(mesh, "SM_FinalCavityBoneInsert", add_collision=False)


def build_final_cavity_tarp():
    """One dusty waterproof-sheet edge compressed behind the remains."""
    mesh = new_mesh()
    ellipsoid(mesh, (4.0, 30.0, 146.0), location=(18.0, 39.0, 88.0),
              rotation=(0.0, 0.0, -4.0), steps=28)
    ellipsoid(mesh, (5.0, 88.0, 22.0), location=(12.0, 5.0, 16.0),
              rotation=(0.0, 3.0, 0.0), steps=28)
    for y, z, roll in ((31.0, 52.0, -9.0), (38.0, 91.0, 6.0),
                       (34.0, 128.0, -5.0)):
        ellipsoid(mesh, (3.0, 22.0, 5.0), location=(-1.0, y, z),
                  rotation=(0.0, 0.0, roll), steps=16)
    return bake(mesh, "SM_FinalCavityTarp", add_collision=False)


def build_final_cavity_broken_caster():
    """A single snapped tool-cart caster, scale clue and causal evidence."""
    mesh = new_mesh()
    profile = []
    for index in range(13):
        angle = math.tau * index / 12.0
        profile.append((6.2 + math.cos(angle) * 1.45,
                        math.sin(angle) * 1.45))
    revolve(mesh, profile, steps=36,
            transform=xf(location=(-10.0, -43.0, 12.0),
                         rotation=(90.0, 0.0, 0.0)))
    cylinder(mesh, 1.8, 4.0, location=(-10.0, -45.0, 12.0),
             rotation=(90.0, 0.0, 0.0), steps=24)
    box(mesh, (3.0, 14.0, 3.0), location=(-4.0, -43.0, 18.0),
        rotation=(0.0, 18.0, 0.0))
    box(mesh, (3.0, 5.0, 10.0), location=(1.0, -43.0, 22.0),
        rotation=(0.0, 18.0, 0.0))
    return bake(mesh, "SM_FinalCavityBrokenCaster", add_collision=False)


def build_mok_hansoo_workwear():
    """Close-range 3D maintenance-worker silhouette for Mok Han-su."""
    mesh = new_mesh()
    # Shoes and slightly uneven legs establish a tired, non-confrontational
    # stance. All joints overlap under workwear; no mannequin gaps remain.
    for side, lean in ((-1.0, -2.0), (1.0, 2.0)):
        _append_round_path(
            mesh, 9.0,
            [(0.0, side * 13.0, 7.0),
             (lean, side * 12.0, 48.0),
             (1.0, side * 14.0, 91.0)], sides=18)
        ellipsoid(mesh, (31.0, 15.0, 10.0),
                  location=(-7.0, side * 13.0, 6.0), steps=22)
    ellipsoid(mesh, (27.0, 46.0, 31.0), location=(1.0, 0.0, 91.0), steps=28)
    ellipsoid(mesh, (32.0, 58.0, 67.0), location=(2.0, 0.0, 130.0),
              rotation=(0.0, 0.0, -2.0), steps=32)
    ellipsoid(mesh, (32.0, 66.0, 19.0), location=(1.0, 0.0, 151.0),
              rotation=(0.0, 0.0, -2.0), steps=28)

    # Both arms genuinely support the gypsum panel at waist/chest height.
    for side in (-1.0, 1.0):
        _append_round_path(
            mesh, 7.7,
            [(0.0, side * 28.0, 148.0),
             (-4.0, side * 34.0, 124.0),
             (-18.0, side * 39.0, 109.0)], sides=16)
    return bake(mesh, "SM_MokHansooWorkwear", add_collision=False)


def build_mok_hansoo_head_hands():
    """Face and gloved hands kept separate for a matte skin/cotton material."""
    mesh = new_mesh()
    _append_round_path(mesh, 6.2, [(1.0, 0.0, 151.0),
                                  (0.0, 0.0, 161.0)], sides=16)
    ellipsoid(mesh, (23.0, 20.0, 27.0), location=(-1.0, 0.0, 174.0),
              rotation=(0.0, 0.0, -4.0), steps=34)
    ellipsoid(mesh, (6.0, 5.0, 7.0), location=(-12.0, 0.0, 174.0), steps=18)
    for side in (-1.0, 1.0):
        ellipsoid(mesh, (13.0, 10.0, 8.0),
                  location=(-19.0, side * 40.0, 108.0),
                  rotation=(0.0, side * 8.0, 0.0), steps=20)
    return bake(mesh, "SM_MokHansooHeadHands", add_collision=False)


def build_mok_hansoo_gypsum_board():
    """Broken-edged 95 x 43 cm board held by Mok, with real thickness."""
    mesh = new_mesh()
    box(mesh, (3.0, 95.0, 43.0), location=(-23.0, 0.0, 112.0))
    # Small missing bites stop the silhouette reading as a perfect UI rectangle.
    for location, size, roll in (
            ((-23.0, -42.0, 132.0), (7.0, 13.0, 9.0), 16.0),
            ((-23.0, 44.0, 96.0), (7.0, 11.0, 10.0), -13.0),
            ((-23.0, 16.0, 134.0), (7.0, 10.0, 7.0), 8.0)):
        cutter = new_mesh()
        box(cutter, size, location=location, rotation=(0.0, 0.0, roll))
        subtract(mesh, cutter)
    bevel_all(mesh, distance=0.18)
    return bake(mesh, "SM_MokHansooGypsumBoard", add_collision=False)


def build_tuning_hammer():
    """Compact L-shaped piano tuning lever from the approved prop reference.

    A tuning hammer is not a listening wand.  The 27 cm handle, short offset
    steel shank and square socket remain readable as one close-inspection prop
    without inventing an animation or a second material slot.
    """
    mesh = new_mesh()

    # Rounded 17 cm grip with a subtle heel and ferrule.  The whole prop is
    # modelled in centimetres so the runtime can use unit scale.
    cylinder(mesh, 1.35, 17.0, location=(0.0, 0.0, 0.0), steps=24)
    cylinder(mesh, 1.52, 0.9, location=(0.0, 0.0, 0.0), steps=24)
    cylinder(mesh, 0.92, 1.2, location=(0.0, 0.0, 17.0), steps=24)

    # Steel neck and the characteristic right-angle head.  A short angled
    # transition keeps the union from reading as two intersecting primitives.
    _append_round_path(
        mesh,
        0.48,
        [(0.0, 0.0, 18.0), (0.0, 0.0, 24.5), (1.6, 0.0, 25.8)],
        sides=14)
    _append_round_path(
        mesh,
        0.58,
        [(1.6, 0.0, 25.8), (5.1, 0.0, 25.8)],
        sides=14)

    # Replaceable star-tip socket, kept as a restrained square tool head.
    socket = new_mesh()
    box(socket, (2.2, 1.65, 1.65), location=(5.65, 0.0, 25.8))
    bevel_all(socket, distance=0.18)
    union(mesh, socket)
    return bake(mesh, "SM_TuningHammer", add_collision=True)


def build_tuner_tool_cart():
    """45 x 34 x 78 cm two-tier cart from the ImageGen hero-prop sheet."""
    mesh = new_mesh()

    # Two shallow trays, four narrow posts and a rear push handle.  Parts stay
    # restrained enough to read as a working piano technician's cart, not a
    # hospital trolley or a solid programmer-art block.
    for tray_z in (17.0, 55.0):
        tray = new_mesh()
        box(tray, (45.0, 34.0, 2.0), location=(0.0, 0.0, tray_z))
        bevel_all(tray, distance=0.35)
        union(mesh, tray)
        for side_x in (-1.0, 1.0):
            box(mesh, (1.2, 34.0, 4.0),
                location=(side_x * 21.9, 0.0, tray_z + 2.7))
        for side_y in (-1.0, 1.0):
            box(mesh, (42.6, 1.2, 4.0),
                location=(0.0, side_y * 16.4, tray_z + 2.7))

    for x in (-20.5, 20.5):
        for y in (-14.5, 14.5):
            cylinder(mesh, 0.9, 54.0, location=(x, y, 6.0), steps=16)
            cylinder(
                mesh, 3.8, 2.0, location=(x, y, 3.8),
                rotation=(0.0, 90.0, 0.0), steps=20)

    # Handle rises from the rear posts and returns across the cart width.
    _append_round_path(
        mesh, 0.95,
        [(-20.5, 14.5, 55.0), (-20.5, 14.5, 75.0),
         (20.5, 14.5, 75.0), (20.5, 14.5, 55.0)],
        sides=14)
    return bake(mesh, "SM_TunerToolCart", add_collision=True)


def build_complaint_ledger():
    """Closed A4 complaint ledger with a real page block and cover thickness."""
    mesh = new_mesh()
    pages = new_mesh()
    box(pages, (21.0, 29.7, 1.8), location=(0.0, 0.0, 0.0))
    bevel_all(pages, distance=0.28)
    union(mesh, pages)
    for cover_z in (-1.15, 1.15):
        cover = new_mesh()
        box(cover, (22.0, 30.7, 0.5), location=(-0.25, 0.0, cover_z))
        bevel_all(cover, distance=0.22)
        union(mesh, cover)
    box(mesh, (1.25, 30.7, 2.8), location=(-10.75, 0.0, 0.0))
    return bake(mesh, "SM_ComplaintLedger", add_collision=True)


def build_calendar_journal():
    """Door-hung 20 x 27 cm calendar-back sound journal, blank at runtime."""
    mesh = new_mesh()
    backing = new_mesh()
    box(backing, (20.0, 1.0, 27.0), location=(0.0, 0.0, 0.0))
    bevel_all(backing, distance=0.24)
    union(mesh, backing)
    # The binding is physical; Korean handwriting remains runtime text.
    for x in (-6.0, -2.0, 2.0, 6.0):
        cylinder(
            mesh, 0.42, 1.8, location=(x, -0.1, 13.6),
            rotation=(90.0, 0.0, 0.0), steps=12)
    box(mesh, (18.4, 0.35, 1.0), location=(0.0, -0.68, 11.8))
    return bake(mesh, "SM_CalendarJournal", add_collision=True)


def _build_p3_valve_wheel(asset_name, diameter, rim_thickness, hub_radius):
    """Five-spoke cast handwheel lying in XY with its axis on local Z."""
    mesh = new_mesh()
    major_radius = diameter * 0.5 - rim_thickness
    profile = []
    for index in range(13):
        angle = math.tau * index / 12.0
        profile.append((
            major_radius + math.cos(angle) * rim_thickness,
            math.sin(angle) * rim_thickness))
    revolve(mesh, profile, steps=40)
    cylinder(
        mesh,
        hub_radius,
        max(2.6, rim_thickness * 3.4),
        location=(0.0, 0.0, -max(1.3, rim_thickness * 1.7)),
        steps=28)

    spoke_length = major_radius - hub_radius * 0.45
    for spoke_index in range(5):
        angle_degrees = spoke_index * 72.0
        angle = math.radians(angle_degrees)
        box(
            mesh,
            (spoke_length, rim_thickness * 0.9, rim_thickness * 1.2),
            location=(
                math.cos(angle) * spoke_length * 0.5,
                math.sin(angle) * spoke_length * 0.5,
                0.0),
            rotation=(0.0, angle_degrees, 0.0))
    return bake(mesh, asset_name, add_collision=True)


def build_p3_large_valve_wheel():
    """Eighteen-centimetre direct/reserve/drain handwheel."""
    return _build_p3_valve_wheel("SM_P3ValveWheelLarge", 18.0, 0.82, 2.35)


def build_p3_small_valve_wheel():
    """Twelve-centimetre pressure-release handwheel."""
    return _build_p3_valve_wheel("SM_P3ValveWheelSmall", 12.0, 0.58, 1.65)


def build_cracked_phone():
    """Plain 2018-era phone body; cracks remain separate screen overlays."""
    mesh = new_mesh()
    box(mesh, (7.2, 14.6, 0.92), location=(0.0, 0.0, 0.46))
    bevel_all(mesh, distance=0.48)
    camera = new_mesh()
    box(camera, (2.0, 2.0, 0.34), location=(-2.0, 5.3, 1.04))
    bevel_all(camera, distance=0.24)
    union(mesh, camera)
    return bake(mesh, "SM_CrackedPhone", add_collision=True)


BUILDERS = (
    build_water_bottle,
    build_bottle_cap,
    build_soju_bottle,
    build_cup_noodle,
    build_cup_sleeve,
    build_cup_lid,
    build_kimchi_tub,
    build_milk_carton,
    build_lever_handle,
    build_stool,
    build_basket,
    build_label_sleeve,
    build_sticky_note_76mm,
    build_capture_mercy_note,
    build_alley_cat_run,
    build_listener_entity_crawl,
    build_final_cavity_clothing_shell,
    build_final_cavity_bone_insert,
    build_final_cavity_tarp,
    build_final_cavity_broken_caster,
    build_mok_hansoo_workwear,
    build_mok_hansoo_head_hands,
    build_mok_hansoo_gypsum_board,
    build_tuning_hammer,
    build_tuner_tool_cart,
    build_complaint_ledger,
    build_calendar_journal,
    build_p3_large_valve_wheel,
    build_p3_small_valve_wheel,
    build_cracked_phone,
)


def run():
    log(f"normals library: {NORMALS}, queries library: {QUERIES}")
    builders = BUILDERS
    if os.environ.get("IG_CORRIDOR_SIGNAGE_ONLY") == "1":
        builders = (build_capture_mercy_note,)
    elif os.environ.get("IG_LABEL_SLEEVE_ONLY") == "1":
        builders = (build_label_sleeve, build_cup_sleeve)
    elif os.environ.get("IG_MISSING_FLOOR_ONLY") == "1":
        builders = (
            build_listener_entity_crawl,
            build_final_cavity_clothing_shell,
            build_final_cavity_bone_insert,
            build_final_cavity_tarp,
            build_final_cavity_broken_caster,
            build_mok_hansoo_workwear,
            build_mok_hansoo_head_hands,
            build_mok_hansoo_gypsum_board,
            build_tuning_hammer,
            build_tuner_tool_cart,
            build_complaint_ledger,
            build_calendar_journal,
        )
    # Blender/TRELLIS.2 경로가 정식이 된 메시는 여기서 다시 굽지 않는다.
    # 다시 구우면 Content/Meshes의 생성 에셋을 타원체 조립으로 덮어쓴다.
    # 판정은 소스 폴더에 manifest가 있느냐다(Scripts/Import-BlenderAssets.ps1).
    blender_root = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "Content", "SourceArt", "Blender"))
    superseded = {
        "build_tuning_hammer": "SM_TuningHammer",
        "build_complaint_ledger": "SM_ComplaintLedger",
        "build_alley_cat_run": "SM_AlleyCatRun",
        "build_listener_entity_crawl": "SM_ListenerEntityCrawl",
        "build_final_cavity_clothing_shell": "SM_FinalCavityRemains",
        "build_final_cavity_bone_insert": "SM_FinalCavityRemains",
        "build_final_cavity_tarp": "SM_FinalCavityRemains",
        "build_final_cavity_broken_caster": "SM_FinalCavityRemains",
        "build_mok_hansoo_workwear": "SM_MokHansooFigure",
        "build_mok_hansoo_head_hands": "SM_MokHansooFigure",
        "build_mok_hansoo_gypsum_board": "SM_MokHansooFigure",
        # 컵라면 세 조각은 Blender에서 다시 만들었다(build_store_products.py).
        "build_cup_noodle": "SM_CupNoodle",
        "build_cup_sleeve": "SM_CupSleeve",
        "build_cup_lid": "SM_CupLid",
    }
    built = 0
    skipped = 0
    for builder in builders:
        replacement = superseded.get(builder.__name__)
        if replacement and os.path.isfile(os.path.join(blender_root, replacement, "manifest.json")):
            log(f"skip {builder.__name__}: {replacement} comes from Content/SourceArt/Blender")
            skipped += 1
            continue
        try:
            builder()
            built += 1
        except Exception as error:  # noqa: BLE001 - report and keep going
            log(f"FAILED {builder.__name__}: {error}")
    log(f"complete: {built}/{len(builders)} meshes ({skipped} superseded by Blender)")


if __name__ == "__main__":
    run()
