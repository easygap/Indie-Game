"""Author the prologue's hero props as real static meshes with Geometry Script.

Everything here is modelled procedurally — lathed bottles and tubs, swept
handles, beveled shells, boolean recesses — and baked into /Game/Meshes as
StaticMesh assets with simple collision. The scene then places these instead
of stacking engine cubes, which is what makes the props read as objects.

Run with: UnrealEditor-Cmd <uproject> -ExecutePythonScript=.../generate_meshes.py
"""

import math
import os

import unreal


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
XFORM = unreal.GeometryScript_MeshTransforms
BOOL_LIB = unreal.GeometryScript_MeshBooleans
SELECT = unreal.GeometryScript_MeshSelection
MODEL = unreal.GeometryScript_MeshModeling
REPAIR = unreal.GeometryScript_MeshRepair
COLLISION = unreal.GeometryScript_Collision
NEW_ASSET = unreal.GeometryScript_NewAssetUtils
NORMALS = find_library("normal")
QUERIES = find_library("quer")
UVS = find_library("uv")
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
            uvs.append((u, v))

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


HERO_MESHES = {
    "SM_AlleyCatRun",
    "SM_FirstPersonHoodieSleeve",
    "SM_HornRimGlasses",
    "SM_InspectionRod",
    "SM_CrackedPhone",
    "SM_CarrierBagCollapsed",
    "SM_LadderFailureRung",
    "SM_LadderRungPadLifted",
    "SM_LadderRungRetainingClips",
    "SM_P3ValveWheelLarge",
    "SM_P3ValveWheelSmall",
    "SM_P3PressureGauge",
    "SM_OfferingWaterBowl",
    "SM_CupSleeve",
    "SM_LabelSleeve",
    "SM_StickyNote76mm",
    "SM_ListenerEntityCrawl",
    "SM_FinalCavityClothingShell",
    "SM_FinalCavityBoneInsert",
    "SM_FinalCavityTarp",
    "SM_FinalCavityBrokenCaster",
    "SM_MokHansooWorkwear",
    "SM_MokHansooHeadHands",
    "SM_MokHansooGypsumBoard",
    "SM_TuningHammer",
    "SM_TunerToolCart",
    "SM_ComplaintLedger",
    "SM_CalendarJournal",
}

LARGE_PROP_PREFIXES = (
    "SM_RooftopWaterTank",
    "SM_RooftopTank",
    "SM_TankInternal",
    "SM_TankAccess",
    "SM_TankExterior",
    "SM_RooftopFireDoor",
    "SM_P3ServiceCabinet",
)


def apply_lod_contract(static_mesh, asset_name):
    """Keep inspection props full-detail; use engine LOD groups elsewhere."""
    if asset_name in HERO_MESHES:
        log(f"  {asset_name} LOD0 preserved (story-critical close inspection)")
        return
    group = "LargeProp" if asset_name.startswith(LARGE_PROP_PREFIXES) else "SmallProp"
    try:
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if subsystem is None:
            raise RuntimeError("StaticMeshEditorSubsystem unavailable")
        subsystem.set_lod_group(static_mesh, group, True)
        log(f"  {asset_name} LOD group={group}")
    except Exception as error:  # noqa: BLE001 - UE minor versions expose different groups
        log(f"  LOD group skipped for {asset_name}: {error}")


def bake(mesh, asset_name, add_collision=True, weld_edges=True):
    """Bakes the dynamic mesh into /Game/Meshes/<asset_name>."""
    if weld_edges:
        weld_options = unreal.GeometryScriptWeldEdgesOptions()
        weld_options.set_editor_property("tolerance", 0.01)
        weld_options.set_editor_property("only_unique_pairs", False)
        REPAIR.weld_mesh_edges(mesh, weld_options)
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


def build_drink_bottle():
    """Wider soda/tea bottle so cooler stock is not all one silhouette."""
    mesh = new_mesh()
    profile = [
        (0.0, 0.0), (2.9, 0.0), (3.6, 0.8), (3.6, 5.0),
        (3.25, 6.2), (3.6, 7.4), (3.6, 11.5), (3.0, 13.4),
        (2.0, 15.0), (1.55, 16.2), (1.7, 16.7), (1.5, 17.1),
        (0.0, 17.1),
    ]
    revolve(mesh, profile, steps=40)
    return bake(mesh, "SM_DrinkBottle")


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


def build_offering_water_bowl():
    """Shallow stainless water bowl with a real open interior.

    The lobby offering is read from standing eye height and later reappears as
    accident evidence.  A capped cylinder looked like a black puck, so the
    revolved profile keeps a weighted base, tapered wall, rolled lip and an
    unobstructed cavity for the separate water surface.
    """
    mesh = new_mesh()
    profile = [
        (0.0, 0.0), (8.8, 0.0), (9.8, 0.7), (11.1, 4.6),
        (11.8, 7.5), (12.2, 8.0), (12.0, 8.6), (11.3, 8.7),
        (10.7, 7.7), (10.0, 4.9), (8.4, 1.2), (0.0, 1.2),
    ]
    revolve(mesh, profile, steps=48, smooth=True, scale_to_fill=True)
    return bake(mesh, "SM_OfferingWaterBowl", add_collision=False)


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


def build_snack_bag():
    """Pillow-shaped snack bag: rounded body with flat crimped seals."""
    mesh = new_mesh()
    box(mesh, (16.0, 5.5, 20.0), location=(0.0, 0.0, 3.0), scale_to_fill=True)
    bevel_all(mesh, distance=1.5, angle_threshold=20.0)
    box(mesh, (16.4, 0.5, 2.6), location=(0.0, 0.0, 14.2), scale_to_fill=True)
    box(mesh, (16.4, 0.5, 2.6), location=(0.0, 0.0, -8.2), scale_to_fill=True)
    return bake(mesh, "SM_SnackBag")


def build_alarm_clock():
    """Clock radio with one continuous, load-bearing lower chassis.

    Separate feet are physically plausible at inspection distance but, under
    the nearby bedside lamp, their gap projected a second dark rectangle below
    the clock and made it read as hovering.  The release silhouette therefore
    uses the broad chassis itself as the contact surface.  Its exact -0.40 cm
    lower bound is shared with runtime placement; there is no empty padding or
    hidden air gap below the visible mesh.
    """
    mesh = new_mesh()
    box(mesh, (13.0, 8.5, 6.4), location=(0.0, 0.0, 3.2))
    bevel_all(mesh, distance=0.5)

    plinth = new_mesh()
    box(plinth, (12.2, 7.7, 0.65), location=(0.0, 0.0, -0.075))
    bevel_all(plinth, distance=0.16)
    union(mesh, plinth)

    recess = new_mesh()
    box(recess, (9.6, 1.4, 4.2), location=(0.0, 4.3, 3.4))
    subtract(mesh, recess)
    for offset in (-3.6, 0.0, 3.6):
        button = new_mesh()
        box(button, (2.4, 3.0, 0.7), location=(offset, -0.6, 6.5))
        bevel_all(button, distance=0.22)
        options = unreal.GeometryScriptMeshBooleanOptions()
        BOOL_LIB.apply_mesh_boolean(
            mesh, xf(), button, xf(),
            unreal.GeometryScriptBooleanOperation.UNION, options)
    return bake(mesh, "SM_AlarmClock")


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


def build_lamp_shade():
    """Street lamp dish: a curved, flanged reflector rather than a cone."""
    mesh = new_mesh()
    profile = [
        (0.0, 12.0), (3.0, 11.6), (8.0, 9.4), (14.0, 6.0),
        (19.0, 2.6), (22.0, 0.6), (23.0, 0.0), (22.6, -0.9),
        (18.6, 1.1), (13.2, 4.6), (7.4, 8.2), (2.8, 10.4),
        (0.0, 10.8),
    ]
    revolve(mesh, profile, steps=40)
    return bake(mesh, "SM_LampShade")


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


def build_kimbap_pack():
    """Clear kimbap tray: rounded shallow box with a lidded step."""
    mesh = new_mesh()
    box(mesh, (18.0, 9.0, 4.2), location=(0.0, 0.0, 2.1))
    bevel_all(mesh, distance=0.6)
    box(mesh, (16.4, 7.8, 1.4), location=(0.0, 0.0, 4.6))
    return bake(mesh, "SM_KimbapPack")


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


def build_sandwich_pack():
    """Triangular sandwich wedge pack."""
    mesh = new_mesh()
    polygon = [(-6.5, 0.0), (6.5, 0.0), (0.0, 9.5)]
    points = [unreal.Vector2D(x, y) for x, y in polygon]
    PRIM.append_simple_extrude_polygon(
        mesh, prim_options(), xf(rotation=(-90.0, 0.0, 0.0)),
        points, 4.5, 1, True,
        unreal.GeometryScriptPrimitiveOriginMode.BASE)
    bevel_all(mesh, distance=0.3)
    return bake(mesh, "SM_SandwichPack")


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


def build_first_person_hoodie_sleeve():
    """Thirty-six-centimetre static left sleeve for the outfit reveal.

    Two overlapping cloth masses create a shallow forearm bend instead of the
    previous engine cylinder. The cuff and its raised ribs remain part of the
    same material group; the three black repair stitches stay separate runtime
    geometry so the identity count cannot be lost in a texture.
    """
    mesh = new_mesh()

    # A loose 13.5 cm upper sleeve and tapered 11.8 cm forearm overlap across
    # seven centimetres. Opposing pitches produce a restrained natural bend.
    ellipsoid(
        mesh,
        (13.5, 12.4, 24.0),
        location=(0.0, 0.0, 7.0),
        rotation=(6.0, 0.0, 0.0),
        steps=32)
    ellipsoid(
        mesh,
        (11.8, 11.0, 18.0),
        location=(1.4, 0.0, -7.0),
        rotation=(-4.0, 0.0, 0.0),
        steps=28)

    # A 10.5 cm rib-knit cuff closes below frame, so no hand or skin mesh is
    # required. Three shallow bands keep it readable at the 78-degree FOV.
    cylinder(
        mesh,
        5.25,
        7.0,
        location=(1.8, 0.0, -18.0),
        steps=36)
    for rib_z in (-17.0, -15.2, -13.4):
        revolve(
            mesh,
            [
                (5.20, rib_z - 0.22),
                (5.42, rib_z - 0.22),
                (5.42, rib_z + 0.22),
                (5.20, rib_z + 0.22),
            ],
            steps=36,
            transform=xf(location=(1.8, 0.0, 0.0)))
    return bake(mesh, "SM_FirstPersonHoodieSleeve", add_collision=False)


def build_listener_entity_crawl():
    """Anatomical static shell for 「없는 층」의 위층 사람.

    The approved ImageGen sheet fixes a real 176 cm adult in a forearm-supported
    crawl.  This mesh deliberately remains one frozen, disconnected-shell pose:
    the pawn moves as a whole, so a skeletal pipeline would add cost without
    improving the silhouette seen in the flashlight.  Unlike the six engine
    blocks it replaces, every major anatomical chain remains readable.
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

    return bake(mesh, "SM_ListenerEntityCrawl", add_collision=False)


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


def build_p3_service_cabinet_shell():
    """Open 270 x 196 cm Korean-villa water service cabinet.

    The cabinet is already open, so it never becomes another key or lock
    puzzle. Its two door leaves, shallow recess and bent latch are one static
    silhouette; the two missing screws stay separate dark runtime geometry so
    their exact count remains readable under the flashlight.
    """
    mesh = new_mesh()

    # Recess, rolled frame and back plate. Local +Y points toward the player.
    box(mesh, (270.0, 4.0, 196.0), location=(0.0, -14.0, 0.0))
    box(mesh, (270.0, 18.0, 10.0), location=(0.0, -5.0, 93.0))
    box(mesh, (270.0, 18.0, 10.0), location=(0.0, -5.0, -93.0))
    box(mesh, (10.0, 18.0, 186.0), location=(-130.0, -5.0, 0.0))
    box(mesh, (10.0, 18.0, 186.0), location=(130.0, -5.0, 0.0))

    # Two attached 135 cm leaves swing toward the corridor without hiding a
    # control. Their angles match the approved reference and leave the centre
    # front completely open for the first-person interaction trace.
    left_angle = math.radians(100.0)
    box(
        mesh,
        (135.0, 2.2, 190.0),
        location=(
            -135.0 + math.cos(left_angle) * 67.5,
            math.sin(left_angle) * 67.5,
            0.0),
        rotation=(0.0, 100.0, 0.0))
    right_angle = math.radians(75.0)
    box(
        mesh,
        (135.0, 2.2, 190.0),
        location=(
            135.0 + math.cos(right_angle) * 67.5,
            math.sin(right_angle) * 67.5,
            0.0),
        rotation=(0.0, 75.0, 0.0))

    # A small bent latch remains attached beside the two empty screw holes.
    box(
        mesh,
        (2.5, 18.0, 5.0),
        location=(138.0, 20.0, -6.0),
        rotation=(0.0, 0.0, -12.0))
    return bake(mesh, "SM_P3ServiceCabinetShell", add_collision=False)


def build_p3_service_manifold():
    """34/25 mm water lines, unions and catch tray sharing cabinet origin."""
    mesh = new_mesh()

    # Main 34 mm manifold and the two upper inlet risers.
    _append_round_path(mesh, 1.7, [(-104.0, 0.0, 0.0), (104.0, 0.0, 0.0)], 16)
    for x_coord in (-75.0, -25.0):
        _append_round_path(
            mesh,
            1.7,
            [(x_coord, 0.0, -1.0), (x_coord, 0.0, 78.0)],
            16)
        for z_coord in (-2.5, 20.0, 51.0, 74.0):
            cylinder(
                mesh,
                2.35,
                2.6,
                location=(x_coord, 0.0, z_coord),
                steps=24)

    # Pressure-release and floor-drain branches use a credible 25 mm OD.
    for x_coord in (30.0, 85.0):
        _append_round_path(
            mesh,
            1.25,
            [(x_coord, 0.0, 1.0), (x_coord, 0.0, -79.0)],
            14)
        for z_coord in (-2.0, -38.0, -77.0):
            cylinder(
                mesh,
                1.85,
                2.2,
                location=(x_coord, 0.0, z_coord),
                steps=20)

    # Gauge takeoff on the right side. The housing remains a separate mesh so
    # its face and moving needle can use different materials.
    _append_round_path(
        mesh,
        1.25,
        [(72.0, 0.0, 0.0), (80.0, 0.0, 8.0), (80.0, 0.0, 37.0)],
        14)

    # Shallow stainless catch tray below the transparent bleed tube.
    box(mesh, (34.0, 16.0, 1.6), location=(30.0, 0.0, -84.0))
    box(mesh, (34.0, 1.4, 4.0), location=(30.0, -7.3, -82.8))
    box(mesh, (34.0, 1.4, 4.0), location=(30.0, 7.3, -82.8))
    box(mesh, (1.4, 16.0, 4.0), location=(13.7, 0.0, -82.8))
    box(mesh, (1.4, 16.0, 4.0), location=(46.3, 0.0, -82.8))
    return bake(mesh, "SM_P3ServiceManifold", add_collision=False)


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


def build_p3_pressure_gauge():
    """Sixteen-centimetre analog gauge housing with a separate live face."""
    mesh = new_mesh()
    cylinder(mesh, 8.0, 3.6, location=(0.0, 0.0, -1.8), steps=40)
    bezel_profile = []
    for index in range(11):
        angle = math.tau * index / 10.0
        bezel_profile.append((7.45 + math.cos(angle) * 0.62,
                              1.9 + math.sin(angle) * 0.62))
    revolve(mesh, bezel_profile, steps=40)
    cylinder(mesh, 1.45, 4.0, location=(0.0, 0.0, -5.8), steps=24)
    return bake(mesh, "SM_P3PressureGauge", add_collision=False)


def build_submerged_hoodie_curl():
    """Anatomically readable wet hoodie for the CH03 side-lying body.

    All clothing groups share one origin.  The face and skin remain hidden,
    but the head-in-hood, neck transition, shoulder line, elbows, forearms and
    covered hands are separate masses.  Negative space around the folded arms
    is intentional: at hatch distance it must read as a person wearing clothes,
    not one mathematically smooth fabric bundle.
    """
    mesh = new_mesh()

    # Ribcage and waist. The head is offset toward the floor side of the pose,
    # leaving a real neck notch in top view instead of stacking concentric
    # ovals into one anonymous bundle.
    ellipsoid(
        mesh, (68.0, 34.0, 21.0), location=(2.0, -3.0, 0.0),
        rotation=(0.0, 2.0, -2.0), steps=32)
    ellipsoid(
        mesh, (36.0, 40.0, 20.0), location=(25.0, -3.0, 2.0),
        rotation=(0.0, -2.0, -2.0), steps=30)
    ellipsoid(
        mesh, (24.0, 30.0, 18.0), location=(-28.0, 1.0, -2.0),
        rotation=(0.0, 2.0, 0.0), steps=24)

    # One cowl and one hooded head. The previous extra hood-lip ellipsoid read
    # as a second head once refracted by the water, so the silhouette is kept
    # deliberately simple here. The face is turned into the tank floor.
    ellipsoid(
        mesh, (18.0, 25.0, 12.0), location=(38.0, -12.0, 4.0),
        rotation=(0.0, -18.0, 1.0), steps=24)
    ellipsoid(
        mesh, (28.0, 24.0, 23.0), location=(50.0, -22.0, 7.0),
        rotation=(0.0, -20.0, 7.0), steps=30)

    # Sleeves fold independently across the chest. Only the continuous tubes
    # and small covered hands are needed; large endpoint spheres made elbows
    # look like detached balls in the first runtime review.
    arm_paths = (
        (6.2, [(25.0, -13.0, 5.0), (7.0, -29.0, 9.0)]),
        (5.1, [(7.0, -29.0, 9.0), (24.0, -12.0, 14.0)]),
        (6.1, [(24.0, 10.0, 4.0), (1.0, 22.0, 8.0)]),
        (5.0, [(1.0, 22.0, 8.0), (17.0, 5.0, 13.0)]),
    )
    for radius, path in arm_paths:
        _append_round_path(mesh, radius, path)
    ellipsoid(mesh, (11.0, 7.0, 5.5), location=(25.0, -11.0, 14.0),
              rotation=(0.0, 24.0, 0.0), steps=18)
    ellipsoid(mesh, (11.0, 7.0, 5.5), location=(18.0, 5.0, 13.0),
              rotation=(0.0, -22.0, 0.0), steps=18)

    # Raised cloth folds sit at compression points rather than randomly across
    # the body, so flashlight highlights reinforce anatomy.
    for location, yaw, length in (
            ((-17.0, -11.0, 10.0), 18.0, 17.0),
            ((-14.0, 10.0, 10.2), -18.0, 16.0),
            ((8.0, -10.0, 11.5), 29.0, 13.0),
            ((9.0, 9.0, 11.8), -27.0, 13.0)):
        ellipsoid(
            mesh, (length, 3.2, 2.2), location=location,
            rotation=(0.0, yaw, 0.0), steps=16)
    return bake(mesh, "SM_SubmergedHoodieCurl", add_collision=False)


def build_submerged_pants_curl():
    """Loose training pants in a compact, readable side-lying foetal pose."""
    mesh = new_mesh()

    # Pelvis stays attached to the hoodie hem without swallowing the thigh
    # roots. A shallow waistband breaks the two material groups cleanly.
    ellipsoid(
        mesh, (40.0, 34.0, 20.0), location=(-30.0, 4.0, -3.0),
        rotation=(0.0, 2.0, 0.0), steps=28)
    ellipsoid(
        mesh, (9.0, 32.0, 3.5), location=(-22.0, 3.0, 6.5),
        rotation=(0.0, 2.0, 0.0), steps=20)

    # Both thighs bend toward the abdomen and both shins return toward the
    # slippered feet. This Z-shaped continuity fits the 104 cm hatch while
    # preserving the pelvis -> thigh -> knee -> shin chain in silhouette.
    leg_segments = (
        ((-34.0, -2.0, -2.0), (2.0, 27.0, 1.0), 23.0, 17.0),
        ((2.0, 27.0, 1.0), (31.0, 6.0, -4.0), 18.0, 14.0),
        ((-34.0, 10.0, 2.0), (-1.0, 42.0, 5.0), 24.0, 18.0),
        ((-1.0, 42.0, 5.0), (34.0, 26.0, -1.0), 19.0, 14.0),
    )
    for start, end, width, height in leg_segments:
        delta_x = end[0] - start[0]
        delta_y = end[1] - start[1]
        length = math.hypot(delta_x, delta_y) + width * 0.45
        location = (
            (start[0] + end[0]) * 0.5,
            (start[1] + end[1]) * 0.5,
            (start[2] + end[2]) * 0.5)
        yaw = math.degrees(math.atan2(delta_y, delta_x))
        ellipsoid(
            mesh, (length, width, height), location=location,
            rotation=(0.0, yaw, 0.0), steps=28)

    # Knee caps are cloth-covered anatomical joints, not detached spheres.
    ellipsoid(mesh, (24.0, 22.0, 16.0), location=(2.0, 27.0, 1.0),
              rotation=(0.0, -18.0, 0.0), steps=22)
    ellipsoid(mesh, (25.0, 23.0, 17.0), location=(-1.0, 42.0, 5.0),
              rotation=(0.0, -12.0, 0.0), steps=22)

    # Gathered cuffs overlap the sock-shaped foot volumes.  The ankles remain
    # covered while still reading as narrower joints between shin and slipper.
    ellipsoid(
        mesh, (16.0, 13.0, 10.0), location=(32.0, 6.0, -5.0),
        rotation=(0.0, -8.0, 0.0), steps=20)
    ellipsoid(
        mesh, (16.0, 13.0, 10.0), location=(35.0, 26.0, -3.0),
        rotation=(0.0, -3.0, 0.0), steps=20)

    for location, yaw in (((-14.0, 13.0, 10.0), 39.0),
                          ((-12.0, 27.0, 12.0), 44.0),
                          ((16.0, 17.0, 6.0), -36.0),
                          ((18.0, 34.0, 8.0), -25.0)):
        ellipsoid(mesh, (12.0, 2.6, 1.8), location=location,
                  rotation=(0.0, yaw, 0.0), steps=14)
    return bake(mesh, "SM_SubmergedPantsCurl", add_collision=False)


def build_submerged_slippers_curl():
    """Two grounded slide slippers with soles, covered feet and upper straps."""
    mesh = new_mesh()
    slipper_specs = (
        ((45.0, 6.0, -8.0), -8.0),
        ((48.0, 26.0, -7.0), -3.0),
    )
    for location, yaw in slipper_specs:
        # A beveled slab gives the sole a flat contact edge; ellipsoid-only
        # slippers looked like detached stones in the old reveal.
        sole = new_mesh()
        box(sole, (30.0, 12.0, 2.6), location=location,
            rotation=(0.0, yaw, 0.0))
        bevel_all(sole, distance=0.9)
        union(mesh, sole)
        ellipsoid(
            mesh, (23.0, 10.0, 7.0),
            location=(location[0] - 6.0, location[1], location[2] + 5.0),
            rotation=(0.0, yaw, 0.0), steps=20)
        # A beveled rectangular upper is instantly recognisable as a slide;
        # a round upper became another anonymous ball under refraction.
        upper = new_mesh()
        box(upper, (11.0, 13.0, 4.5),
            location=(location[0] + 4.0, location[1], location[2] + 6.0),
            rotation=(0.0, yaw, 0.0))
        bevel_all(upper, distance=0.8)
        union(mesh, upper)

        # The evidence stripes are separate runtime material pieces, but the
        # first upper carries matching shallow ribs.  The pale pieces therefore
        # sit on a physical strap instead of reading as bars suspended in water.
        if location == slipper_specs[0][0]:
            for stripe_x in (46.0, 48.4, 50.8):
                rib = new_mesh()
                box(
                    rib, (1.6, 10.0, 0.7),
                    location=(stripe_x, location[1], 0.45),
                    rotation=(0.0, yaw, 0.0))
                bevel_all(rib, distance=0.18)
                union(mesh, rib)
    return bake(mesh, "SM_SubmergedSlippersCurl", add_collision=False)


def build_rooftop_water_tank_shell():
    """306 cm galvanized reserve-tank shell with a canonical inner floor.

    The exterior wall remains open at the top so the separate access deck and
    lid can keep their gameplay states. Runtime retains invisible segmented
    collision, while this mesh replaces the sixty-four visible engine boxes.
    """
    mesh = new_mesh()

    # Four-centimetre hollow wall, 260 cm tall, centred on local Z. The actor is
    # placed at world Z 470: shell bottom 340, shell top 600.
    revolve(
        mesh,
        [
            (149.0, -130.0),
            (153.0, -130.0),
            (153.0, 130.0),
            (149.0, 130.0),
        ],
        steps=64,
        smooth=True,
        scale_to_fill=True)

    # Sixteen restrained folded seams make the same facets readable without
    # widening the collision envelope or becoming decorative industrial ribs.
    for seam_index in range(16):
        angle_degrees = seam_index * 22.5
        angle = math.radians(angle_degrees)
        box(
            mesh,
            (1.4, 2.2, 256.0),
            location=(
                math.cos(angle) * 153.7,
                math.sin(angle) * 153.7,
                0.0),
            rotation=(0.0, angle_degrees, 0.0))

    # Hoops sit 12, 130 and 248 cm above the exterior shell bottom.
    for hoop_z in (-118.0, 0.0, 118.0):
        revolve(
            mesh,
            [
                (152.7, hoop_z - 2.5),
                (157.0, hoop_z - 2.5),
                (157.0, hoop_z + 2.5),
                (152.7, hoop_z + 2.5),
            ],
            steps=64,
            smooth=False,
            scale_to_fill=True)

    # The inner walking surface is 41 cm above the shell bottom. Its top is
    # local Z -89 / world Z 381, fixing the 2.15 m internal-height contract.
    cylinder(
        mesh,
        149.0,
        2.0,
        location=(0.0, 0.0, -91.0),
        steps=64)
    return bake(mesh, "SM_RooftopWaterTankShell", add_collision=False)


def build_tank_internal_lining():
    """Non-colliding wet inner skin for the tank reveal.

    The separate lining keeps the exterior galvanized material clean while a
    dedicated UV/PBR set carries mineral scale and biofilm below the lid. It
    sits two centimetres behind the proven collision envelope, so neither the
    tank silhouette nor puzzle traversal changes.
    """
    mesh = new_mesh()
    revolve(
        mesh,
        [
            (145.0, 382.0),
            (147.0, 382.0),
            (147.0, 596.0),
            (145.0, 596.0),
        ],
        steps=64,
        smooth=True,
        scale_to_fill=True)
    cylinder(
        mesh,
        144.5,
        0.8,
        location=(0.0, 0.0, 381.2),
        steps=64)
    return bake(mesh, "SM_TankInternalLining", add_collision=False)


def build_rooftop_tank_pipe_cluster():
    """Real-scale 89/76 mm inlet, elbow, branch, flanges and clamps."""
    mesh = new_mesh()

    # The main inlet stands just outside the tank's diagonal. A short upper
    # elbow enters the shell without clipping through the access deck.
    _append_round_path(
        mesh,
        4.45,
        [
            (128.0, -128.0, 242.0),
            (128.0, -128.0, 500.0),
            (126.0, -126.0, 520.0),
            (116.0, -116.0, 536.0),
            (105.0, -105.0, 540.0),
        ],
        sides=18)

    # A 76 mm service branch terminates at the separate 18 cm handwheel. The
    # wheel is placed by the director so the existing authored asset is reused.
    _append_round_path(
        mesh,
        3.8,
        [
            (128.0, -128.0, 302.0),
            (154.0, -128.0, 302.0),
            (181.0, -128.0, 302.0),
        ],
        sides=16)

    # Restrained collars and two wall clamps. Every cylinder is centimetre
    # scale; the former 26/46 cm greybox fittings cannot return in this path.
    for flange_z in (250.0, 302.0, 492.0):
        cylinder(
            mesh,
            6.1,
            2.2,
            location=(128.0, -128.0, flange_z - 1.1),
            steps=32)
    for clamp_z in (340.0, 438.0):
        revolve(
            mesh,
            [
                (4.3, clamp_z - 1.2),
                (5.8, clamp_z - 1.2),
                (5.8, clamp_z + 1.2),
                (4.3, clamp_z + 1.2),
            ],
            steps=32,
            transform=xf(location=(128.0, -128.0, 0.0)),
            scale_to_fill=True)
    return bake(mesh, "SM_RooftopTankPipeCluster", add_collision=False)


def build_tank_internal_ladder():
    """Seven-rung ladder fixed below the service hatch inside the tank."""
    mesh = new_mesh()

    # Rails sit 20 cm inboard of the curved +X wall. World-space Z values are
    # deliberate: raised inner floor 381, deck underside 596.
    for rail_y in (-23.0, 23.0):
        _append_round_path(
            mesh,
            1.7,
            [(-125.0, rail_y, 386.0), (-125.0, rail_y, 590.0)],
            sides=14)
    for rung_index in range(7):
        rung_z = 400.0 + rung_index * 30.0
        _append_round_path(
            mesh,
            1.25,
            [(-125.0, -23.0, rung_z), (-125.0, 23.0, rung_z)],
            sides=12)

    # Two upper and two lower stand-offs are the only wall attachments. The
    # mounting feet stay small enough to read through water without clutter.
    for rail_y in (-23.0, 23.0):
        for bracket_z in (405.0, 575.0):
            _append_round_path(
                mesh,
                1.7,
                [(-125.0, rail_y, bracket_z),
                 (-145.0, rail_y, bracket_z)],
                sides=14)
            box(
                mesh,
                (1.2, 10.0, 10.0),
                location=(-145.0, rail_y, bracket_z))
    return bake(mesh, "SM_TankInternalLadder", add_collision=False)


def build_tank_access_guard_rail():
    """Open-approach 42 mm service-platform rails with one evidence U-bolt."""
    mesh = new_mesh()

    # Two continuous U-shaped sides give the exposed 80 cm platform a readable
    # 75 cm guard height. Nothing crosses the approach edge at local X -65.
    for rail_y in (-105.0, 105.0):
        _append_round_path(
            mesh,
            2.1,
            [
                (-65.0, rail_y, 0.0),
                (-65.0, rail_y, 70.0),
                (-60.0, rail_y, 75.0),
                (0.0, rail_y, 75.0),
                (4.0, rail_y, 70.0),
                (4.0, rail_y, 0.0),
            ],
            sides=16)
        _append_round_path(
            mesh,
            2.1,
            [(-65.0, rail_y, 38.0), (4.0, rail_y, 38.0)],
            sides=16)
        for post_x in (-65.0, 4.0):
            foot = new_mesh()
            box(
                foot,
                (10.0, 10.0, 1.5),
                location=(post_x, rail_y, 0.75))
            bevel_all(foot, distance=1.0)
            union(mesh, foot)

    # Exactly one clamp plate and one U-bolt live on the player's right-hand
    # rail. The existing glasses mesh hangs from this physical loop at runtime.
    plate = new_mesh()
    box(plate, (10.0, 1.6, 12.0), location=(2.0, 107.0, 57.0))
    bevel_all(plate, distance=0.8)
    union(mesh, plate)
    _append_round_path(
        mesh,
        0.6,
        [
            (0.0, 105.0, 52.0),
            (0.0, 112.0, 52.0),
            (0.0, 116.0, 56.0),
            (0.0, 112.0, 60.0),
            (0.0, 105.0, 60.0),
        ],
        sides=10)
    return bake(mesh, "SM_TankAccessGuardRail", add_collision=False)


def build_tank_access_deck():
    """Tank roof with a west-side opening reachable from the access stair."""
    mesh = new_mesh()
    cylinder(mesh, 157.0, 8.0, location=(0.0, 0.0, -4.0), steps=64)
    opening = new_mesh()
    cylinder(opening, 52.0, 14.0, location=(-96.0, 0.0, -7.0), steps=48)
    subtract(mesh, opening)

    # A low rolled collar explains the lid seal and keeps the opening readable
    # through the flashlight without turning the rim into a waist-high barrier.
    collar = new_mesh()
    cylinder(collar, 57.0, 7.0, location=(-96.0, 0.0, 3.0), steps=48)
    collar_opening = new_mesh()
    cylinder(collar_opening, 52.0, 11.0, location=(-96.0, 0.0, 1.0), steps=48)
    subtract(collar, collar_opening)
    union(mesh, collar)
    return bake(mesh, "SM_TankAccessDeck", add_collision=False)


def build_tank_access_lid():
    """Liftable galvanized service lid sized for one-person inspection."""
    mesh = new_mesh()
    cylinder(mesh, 54.0, 6.0, location=(0.0, 0.0, -3.0), steps=48)

    # Shallow crossed ribs prevent the plate reading as an engine cylinder.
    for yaw in (0.0, 90.0):
        rib = new_mesh()
        box(rib, (76.0, 4.5, 3.2), location=(0.0, 0.0, 4.2),
            rotation=(0.0, yaw, 0.0))
        bevel_all(rib, distance=0.75)
        union(mesh, rib)

    # Fixed handle and two hinge ears. These are geometry, not a decorative
    # normal map, so the open state remains legible as a single physical prop.
    handle_section = []
    for index in range(10):
        angle = math.tau * index / 10.0
        handle_section.append((math.cos(angle) * 1.2, math.sin(angle) * 1.2))
    sweep(
        mesh,
        handle_section,
        [(-13.0, 0.0, 5.5), (-13.0, 0.0, 14.0),
         (13.0, 0.0, 14.0), (13.0, 0.0, 5.5)])
    for hinge_y in (-16.0, 16.0):
        hinge = new_mesh()
        box(hinge, (9.0, 8.0, 8.0), location=(49.0, hinge_y, 2.0))
        bevel_all(hinge, distance=1.0)
        union(mesh, hinge)
    return bake(mesh, "SM_TankAccessLid", add_collision=True)


def build_rooftop_service_hose():
    """Continuous 42 mm EPDM hose following the canonical P5 final state."""
    mesh = new_mesh()
    path = [
        (188.0, -181.0, 248.0),
        (166.0, -175.0, 250.0),
        (146.0, -169.0, 276.0),
        (118.0, -161.0, 320.0),
        (86.0, -151.0, 366.0),
        (52.0, -139.0, 414.0),
        (17.0, -126.0, 462.0),
        (-20.0, -113.0, 510.0),
        (-58.0, -100.0, 555.0),
        (-91.0, -86.0, 590.0),
        (-118.0, -72.0, 622.0),
    ]
    _append_round_path(mesh, 2.1, path, sides=14)

    # The cat compression is a shallow rubber deformation in the physical
    # hose, while the existing evidence mask supplies the readable wet edge.
    ellipsoid(
        mesh,
        (27.0, 7.2, 3.2),
        location=(166.0, -175.0, 249.4),
        rotation=(0.0, 18.0, 0.0),
        steps=20)
    return bake(mesh, "SM_RooftopServiceHose", add_collision=False)


def build_hose_coupling():
    """65 mm galvanized quick coupling for the upper P5 impact source."""
    mesh = new_mesh()
    cylinder(mesh, 3.25, 10.0, location=(0.0, 0.0, -5.0), steps=32)
    for radius, height, z in ((3.65, 1.8, -5.4), (4.15, 2.2, 2.8)):
        collar = new_mesh()
        cylinder(collar, radius, height, location=(0.0, 0.0, z), steps=32)
        union(mesh, collar)
    for side in (-1.0, 1.0):
        lug = new_mesh()
        box(lug, (2.2, 2.5, 3.2), location=(side * 3.9, 0.0, 3.8))
        bevel_all(lug, distance=0.45)
        union(mesh, lug)
    return bake(mesh, "SM_HoseCoupling", add_collision=False)


def build_carrier_bag_collapsed():
    """Thin 12 x 18 x 20 cm LDPE bag shared by all purchase profiles.

    Runtime non-uniform scaling preserves the A/B/C capacity silhouette. The
    bottle state remains separate gameplay geometry, so the bag never bakes a
    branch-specific count or water level into its mesh.
    """
    mesh = new_mesh()

    # Five thin shells leave the top physically open and avoid the duplicated
    # translucent cube that made the old proxy read as a glass container.
    box(mesh, (0.28, 17.6, 19.4), location=(-5.86, 0.0, -0.2))
    box(mesh, (0.28, 17.6, 19.4), location=(5.86, 0.0, -0.2))
    box(mesh, (11.6, 0.28, 19.4), location=(0.0, -8.86, -0.2))
    box(mesh, (11.6, 0.28, 19.4), location=(0.0, 8.86, -0.2))
    box(mesh, (11.6, 17.6, 0.35), location=(0.0, 0.0, -9.72))

    # Two fused loop handles, one on each broad face. A low polygon sweep is
    # intentional: translucent sorting stays stable and the silhouette remains
    # readable when Profile C lies on its side.
    handle_section = []
    for index in range(8):
        angle = math.tau * index / 8.0
        handle_section.append((math.cos(angle) * 0.42, math.sin(angle) * 0.42))
    for face_y in (-8.72, 8.72):
        sweep(
            mesh,
            handle_section,
            [(-4.0, face_y, 8.7), (-4.0, face_y, 17.0),
             (4.0, face_y, 17.0), (4.0, face_y, 8.7)])

    # Soft gusset masses make the bottom collapse around bottle feet instead
    # of staying perfectly rectangular under the translucent material.
    ellipsoid(
        mesh, (10.8, 15.8, 2.1), location=(0.0, 0.0, -8.9),
        rotation=(0.0, 0.0, 4.0), steps=20)
    return bake(mesh, "SM_CarrierBagCollapsed", add_collision=True)


def build_rooftop_fire_door_leaf():
    """116 x 230 cm steel leaf for the CH03 rooftop threshold defect.

    The pivot stays at the actor origin in runtime, while the mesh remains
    centred so the fallback cube and authored asset share the same transform
    contract.  The sag itself is deliberately read through the worn lower
    corner and the 5.44 degree rest state instead of skeletal deformation.
    """
    mesh = new_mesh()

    # A plausible 45 mm folded-steel leaf replaces the 12 cm greybox slab.
    shell = new_mesh()
    box(shell, (4.5, 116.0, 230.0))
    bevel_all(shell, distance=0.55, angle_threshold=22.0)
    union(mesh, shell)

    # Shallow pressed reinforcement frames on both faces.  They keep the
    # silhouette utilitarian and readable under a flashlight without adding
    # a second material slot or ornamental residential-door language.
    for face_x in (-2.42, 2.42):
        for panel_z, panel_height in ((57.0, 72.0), (-54.0, 60.0)):
            for side_y in (-1.0, 1.0):
                box(
                    mesh,
                    (0.34, 1.8, panel_height),
                    location=(face_x, side_y * 39.0, panel_z))
            for side_z in (-1.0, 1.0):
                box(
                    mesh,
                    (0.34, 78.0, 1.8),
                    location=(
                        face_x,
                        0.0,
                        panel_z + side_z * panel_height * 0.5))

    # Three welded hinge barrels share the left/negative-Y edge.  A slightly
    # polished lower barrel and corner plate provide the authored sag clue.
    for hinge_z in (-86.0, 0.0, 86.0):
        cylinder(
            mesh,
            2.2,
            14.0,
            location=(0.0, -59.1, hinge_z - 7.0),
            steps=28)
        box(
            mesh,
            (0.65, 8.5, 9.0),
            location=(-2.5, -53.8, hinge_z))
    box(mesh, (0.7, 10.0, 5.5), location=(-2.55, 52.0, -111.2))

    # Interior tubular pull handle and compact closer.  These are part of the
    # one movable leaf, so no handle can be left behind during state changes.
    handle_section = []
    for index in range(10):
        angle = math.tau * index / 10.0
        handle_section.append((math.cos(angle) * 0.75,
                               math.sin(angle) * 0.75))
    sweep(
        mesh,
        handle_section,
        [(4.0, 41.0, -16.0), (7.0, 41.0, -16.0),
         (7.0, 41.0, 22.0), (4.0, 41.0, 22.0)])
    closer = new_mesh()
    box(closer, (5.8, 25.0, 8.0), location=(4.5, -18.0, 100.0))
    bevel_all(closer, distance=0.65)
    union(mesh, closer)
    sweep(
        mesh,
        [(-0.45, -0.45), (0.45, -0.45),
         (0.45, 0.45), (-0.45, 0.45)],
        [(7.8, -7.0, 100.0), (8.2, 12.0, 106.0),
         (8.2, 34.0, 106.0)])

    # Plain exterior hasp mounting points stay key-free.  The interactive key
    # bunch remains a separate runtime prop and therefore cannot be doubled.
    box(mesh, (0.9, 13.0, 4.2), location=(-2.75, 45.0, -2.0))
    cylinder(mesh, 1.05, 1.1, location=(-3.3, 45.0, -2.55), steps=20)
    return bake(mesh, "SM_RooftopFireDoorLeaf", add_collision=True)


def build_rooftop_fire_door_frame():
    """120 x 234 cm clear opening, rolled frame and worn low threshold."""
    mesh = new_mesh()

    # The frame shares the fully-closed leaf centre.  With the leaf bottom at
    # -115 cm, the threshold top at -115.4 leaves 4 mm before the authored
    # worn corner; the visible scrape explains where the sag catches.
    for jamb_y in (-64.0, 64.0):
        jamb = new_mesh()
        # Keep the full frame depth on the closed/outside side of the leaf.
        # Centring it on X=0 would visually consume seven centimetres of the
        # authored free-edge slit even though the gameplay plane is correct.
        box(jamb, (14.0, 8.0, 234.0), location=(7.0, jamb_y, 0.5))
        bevel_all(jamb, distance=0.55)
        union(mesh, jamb)
    box(mesh, (14.0, 136.0, 8.0), location=(7.0, 0.0, 121.5))
    box(mesh, (14.0, 136.0, 1.2), location=(7.0, 0.0, -116.0))

    # Localized polished wear plate at the free/right corner.  No damage is
    # exaggerated beyond what an old Korean rooftop utility door can cause.
    box(mesh, (14.4, 15.0, 0.35), location=(7.0, 52.0, -115.23))
    return bake(mesh, "SM_RooftopFireDoorFrame", add_collision=False)


def build_rooftop_unlocked_padlock_keys():
    """Open rooftop padlock with one inserted and three hanging keys.

    The entire assembly is one inspectable static prop.  Keeping the padlock,
    ring and keys in one asset prevents a reload or authored/fallback mix from
    duplicating a key and accidentally suggesting an inventory lock puzzle.
    """
    mesh = new_mesh()

    # Ordinary 50 x 28 x 62 mm laminated galvanized padlock body.
    body = new_mesh()
    box(body, (2.8, 5.0, 6.2))
    bevel_all(body, distance=0.38, angle_threshold=22.0)
    union(mesh, body)
    for lamination_z in (-2.45, -1.75, -1.05, -0.35, 0.35, 1.05, 1.75, 2.45):
        box(mesh, (2.92, 5.12, 0.14), location=(0.0, 0.0, lamination_z))

    # Eight-millimetre shackle: left leg seated, right leg visibly open with
    # 28 mm of air above the body.  The welded staple is part of the same
    # non-pickup presentation and never joins the door leaf to the frame.
    _append_round_path(
        mesh,
        0.40,
        [(0.0, -1.65, 2.8), (0.0, -1.65, 5.7),
         (0.0, -1.25, 6.55), (0.0, 0.0, 7.05),
         (0.0, 1.25, 6.55), (0.0, 1.65, 5.9)],
        sides=12)
    box(mesh, (2.2, 1.1, 3.0), location=(1.5, -1.2, 7.2))
    bevel_all(mesh, distance=0.18, angle_threshold=20.0)

    # The inserted key is the fourth visible key.  Its small bow connects the
    # only 42 mm split ring to the bottom cylinder without a loose duplicate.
    box(mesh, (0.24, 0.82, 2.2), location=(0.0, 0.0, -3.6))
    ellipsoid(mesh, (0.32, 2.0, 2.2), location=(0.0, 0.0, -4.55), steps=20)

    ring_path = []
    for index in range(25):
        angle = math.tau * index / 24.0
        ring_path.append((
            0.0,
            math.cos(angle) * 2.1,
            -6.5 + math.sin(angle) * 2.1))
    _append_round_path(mesh, 0.18, ring_path, sides=8)

    # Exactly three additional utility keys, deliberately distinct in length
    # and shoulder shape but generic and free of text or brand geometry.
    key_specs = (
        (-1.15, -9.15, 5.0, 0.72, -5.0),
        (0.0, -8.65, 4.0, 0.90, 2.0),
        (1.15, -8.15, 3.0, 0.64, 7.0),
    )
    for key_index, (key_y, blade_z, blade_length, blade_width, roll) in enumerate(key_specs):
        head_z = -6.2 - key_index * 0.10
        ellipsoid(
            mesh,
            (0.30, blade_width * 2.15, 1.85),
            location=(0.0, key_y, head_z),
            rotation=(0.0, 0.0, roll),
            steps=18)
        box(
            mesh,
            (0.22, blade_width, blade_length),
            location=(0.0, key_y, blade_z),
            rotation=(0.0, 0.0, roll))
        # Two restrained teeth on one side make each blade read as a key at
        # gameplay distance without adding another key-like silhouette.
        for tooth_index in range(2):
            box(
                mesh,
                (0.24, 0.34 + key_index * 0.04, 0.34),
                location=(
                    0.0,
                    key_y + blade_width * 0.55,
                    blade_z - blade_length * 0.27 + tooth_index * 0.72),
                rotation=(0.0, 0.0, roll))

    # One blank stamped-steel inventory tag.  It carries no semantic text.
    ellipsoid(
        mesh,
        (0.22, 2.5, 4.6),
        location=(0.0, 2.65, -8.2),
        rotation=(0.0, 0.0, -7.0),
        steps=20)
    return bake(mesh, "SM_RooftopUnlockedPadlockKeys", add_collision=True)


def build_tank_exterior_access_stair():
    """Eighteen-tread 45-degree stair matching the hidden gameplay steps.

    Runtime places the local origin at the roof-floor height under tread zero.
    Tread 16 is deliberately omitted because the separate failure cluster owns
    that upper-second tread and its two evidence materials.
    """
    mesh = new_mesh()

    for tread_index in range(18):
        if tread_index == 16:
            continue
        tread = new_mesh()
        box(
            tread,
            (20.0, 105.0, 3.0),
            location=(
                tread_index * 20.0,
                0.0,
                (tread_index + 1) * 20.0 - 1.5))
        bevel_all(tread, distance=0.55, angle_threshold=18.0)
        union(mesh, tread)

    # The pair of 45-degree stringers runs below the tread pans. Their 340 cm
    # horizontal/vertical span follows the exact centers of tread 0 and 17.
    stringer_length = math.sqrt(340.0 ** 2 + 340.0 ** 2)
    for side in (-1.0, 1.0):
        stringer = new_mesh()
        box(
            stringer,
            (stringer_length, 6.0, 12.0),
            location=(170.0, side * 49.0, 180.0),
            rotation=(45.0, 0.0, 0.0))
        bevel_all(stringer, distance=1.1, angle_threshold=18.0)
        union(mesh, stringer)

    # Four post pairs and two rail lines sit 75/38 cm above the tread line.
    # The final 15 cm fans outward to the platform's existing +/-105 cm posts,
    # so the climb and landing read as one fabricated assembly.
    for side in (-1.0, 1.0):
        stair_y = side * 56.0
        platform_y = side * 105.0
        _append_round_path(
            mesh,
            2.1,
            [(0.0, stair_y, 95.0),
             (340.0, stair_y, 435.0),
             (355.0, platform_y, 455.0)],
            sides=16)
        _append_round_path(
            mesh,
            2.1,
            [(0.0, stair_y, 58.0),
             (340.0, stair_y, 398.0),
             (355.0, platform_y, 418.0)],
            sides=16)
        for post_index in (0, 6, 12, 17):
            post_x = post_index * 20.0
            tread_top = (post_index + 1) * 20.0
            _append_round_path(
                mesh,
                2.1,
                [(post_x, stair_y, tread_top),
                 (post_x, stair_y, tread_top + 75.0)],
                sides=16)
            foot = new_mesh()
            box(
                foot,
                (10.0, 10.0, 1.5),
                location=(post_x, stair_y, tread_top + 0.75))
            bevel_all(foot, distance=0.8, angle_threshold=18.0)
            union(mesh, foot)

    return bake(mesh, "SM_TankExteriorAccessStair", add_collision=False)


def build_ladder_failure_rung():
    """The upper-second 105 x 20 cm galvanized tread pan."""
    mesh = new_mesh()
    box(mesh, (20.0, 105.0, 3.0))
    bevel_all(mesh, distance=0.55, angle_threshold=18.0)
    return bake(mesh, "SM_LadderFailureRung", add_collision=False)


def build_ladder_rung_pad_lifted():
    """Exactly 54 cm of ribbed rubber with an 8--10 mm lifted inner edge."""
    mesh = new_mesh()
    # The 54 cm span stays continuous across the rung. A 5 cm seated depth plus
    # a 3.5 cm inward flap makes the failure run along the correct long edge;
    # a 15-degree pitch raises its free edge by 9.1 mm.
    box(mesh, (5.0, 54.0, 0.30), location=(-1.75, 0.0, 1.66))
    box(
        mesh,
        (3.5, 54.0, 0.30),
        location=(2.50, 0.0, 2.10),
        rotation=(15.0, 0.0, 0.0))
    bevel_all(mesh, distance=0.10, angle_threshold=18.0)
    return bake(mesh, "SM_LadderRungPadLifted", add_collision=False)


def build_ladder_rung_retaining_clips():
    """Exactly two shallow galvanized clips sharing the rung local origin."""
    mesh = new_mesh()
    for side in (-1.0, 1.0):
        clip_y = side * 29.0
        clip = new_mesh()
        box(clip, (8.8, 4.2, 1.4), location=(0.0, clip_y, 2.35))
        box(clip, (1.4, 4.2, 3.2), location=(-4.4, clip_y, 1.30))
        box(clip, (1.4, 4.2, 3.2), location=(4.4, clip_y, 1.30))
        bevel_all(clip, distance=0.35, angle_threshold=18.0)
        union(mesh, clip)
        cylinder(
            mesh,
            0.85,
            0.55,
            location=(0.0, clip_y, 3.05),
            steps=20)
    return bake(mesh, "SM_LadderRungRetainingClips", add_collision=False)


def build_horn_rim_glasses():
    """Wet black horn-rim glasses used by the P5 identity comparison.

    Lens panes stay open in the static-proxy mesh. The thick wet frame is the
    identity read; omitting transparent panes avoids a second material slot
    and keeps flashlight sorting deterministic.
    """
    mesh = new_mesh()
    for side in (-1.0, 1.0):
        rim = new_mesh()
        box(rim, (6.1, 4.15, 0.55), location=(side * 3.35, 0.0, 0.32))
        bevel_all(rim, distance=0.48)
        opening = new_mesh()
        box(opening, (5.15, 3.20, 1.2), location=(side * 3.35, 0.0, 0.32))
        bevel_all(opening, distance=0.34)
        subtract(rim, opening)
        union(mesh, rim)

    box(mesh, (1.45, 0.62, 0.55), location=(0.0, 0.15, 0.32))
    bevel_all(mesh, distance=0.12)
    temple_section = [(-0.18, -0.18), (0.18, -0.18),
                      (0.18, 0.18), (-0.18, 0.18)]
    for side in (-1.0, 1.0):
        temple = new_mesh()
        sweep(
            temple,
            temple_section,
            [(side * 6.15, 0.0, 0.32),
             (side * 6.55, 2.0, 0.38),
             (side * 6.72, 8.0, 0.52),
             (side * 6.30, 10.3, -0.15)])
        union(mesh, temple)
    return bake(mesh, "SM_HornRimGlasses", add_collision=True)


def build_inspection_rod():
    """118 cm galvanized tank rod with a modest hooked service end."""
    mesh = new_mesh()
    section = []
    for index in range(10):
        angle = math.tau * index / 10.0
        section.append((math.cos(angle) * 0.58, math.sin(angle) * 0.58))
    sweep(
        mesh,
        section,
        [(0.0, 0.0, 0.0), (1.2, 0.0, 2.4), (3.8, 0.0, 4.5),
         (5.4, 0.0, 7.0), (5.4, 0.0, 108.5)])
    # The rubber grip is baked into the silhouette. Runtime keeps the same
    # metal material in the static-proxy path, so no extra material slot is
    # required for correctness.
    cylinder(mesh, 1.22, 9.5, location=(5.4, 0.0, 108.5), steps=24)
    return bake(mesh, "SM_InspectionRod", add_collision=False)


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
    build_drink_bottle,
    build_cup_noodle,
    build_cup_sleeve,
    build_cup_lid,
    build_kimchi_tub,
    build_offering_water_bowl,
    build_milk_carton,
    build_snack_bag,
    build_alarm_clock,
    build_lever_handle,
    build_lamp_shade,
    build_stool,
    build_basket,
    build_kimbap_pack,
    build_sandwich_pack,
    build_label_sleeve,
    build_sticky_note_76mm,
    build_capture_mercy_note,
    build_alley_cat_run,
    build_first_person_hoodie_sleeve,
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
    build_p3_service_cabinet_shell,
    build_p3_service_manifold,
    build_p3_large_valve_wheel,
    build_p3_small_valve_wheel,
    build_p3_pressure_gauge,
    build_submerged_hoodie_curl,
    build_submerged_pants_curl,
    build_submerged_slippers_curl,
    build_rooftop_water_tank_shell,
    build_tank_internal_lining,
    build_rooftop_tank_pipe_cluster,
    build_tank_internal_ladder,
    build_tank_access_guard_rail,
    build_tank_access_deck,
    build_tank_access_lid,
    build_rooftop_service_hose,
    build_hose_coupling,
    build_carrier_bag_collapsed,
    build_rooftop_fire_door_leaf,
    build_rooftop_fire_door_frame,
    build_rooftop_unlocked_padlock_keys,
    build_tank_exterior_access_stair,
    build_ladder_failure_rung,
    build_ladder_rung_pad_lifted,
    build_ladder_rung_retaining_clips,
    build_horn_rim_glasses,
    build_inspection_rod,
    build_cracked_phone,
)


def run():
    log(f"normals library: {NORMALS}, queries library: {QUERIES}")
    builders = BUILDERS
    if os.environ.get("IG_ALARM_CLOCK_ONLY") == "1":
        builders = (build_alarm_clock,)
    elif os.environ.get("IG_CORRIDOR_SIGNAGE_ONLY") == "1":
        builders = (build_capture_mercy_note,)
    elif os.environ.get("IG_SUBMERGED_CLOTHING_ONLY") == "1":
        builders = (
            build_submerged_hoodie_curl,
            build_submerged_pants_curl,
            build_submerged_slippers_curl,
        )
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
    built = 0
    for builder in builders:
        try:
            builder()
            built += 1
        except Exception as error:  # noqa: BLE001 - report and keep going
            log(f"FAILED {builder.__name__}: {error}")
    log(f"complete: {built}/{len(builders)} meshes")


if __name__ == "__main__":
    run()
