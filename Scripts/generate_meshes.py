"""Author the prologue's hero props as real static meshes with Geometry Script.

Everything here is modelled procedurally — lathed bottles and tubs, swept
handles, beveled shells, boolean recesses — and baked into /Game/Meshes as
StaticMesh assets with simple collision. The scene then places these instead
of stacking engine cubes, which is what makes the props read as objects.

Run with: UnrealEditor-Cmd <uproject> -ExecutePythonScript=.../generate_meshes.py
"""

import math

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
    return unreal.Transform(
        location=unreal.Vector(*location),
        rotation=unreal.Rotator(*rotation),
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


def bake(mesh, asset_name, add_collision=True):
    """Bakes the dynamic mesh into /Game/Meshes/<asset_name>."""
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
    """500 mL PET bottle: ribbed base, gripped waist, shoulder, threaded neck."""
    mesh = new_mesh()
    profile = [
        (0.0, 0.0), (2.5, 0.0), (3.15, 0.7), (3.3, 1.8),
        (3.05, 2.9), (3.3, 4.0), (3.05, 5.1), (3.3, 6.2),
        (3.3, 9.4), (2.85, 10.6), (2.85, 11.6), (3.3, 12.8),
        (3.3, 14.4), (2.75, 15.8), (1.95, 17.0), (1.5, 17.9),
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
    # A closed cross-section, not a line: AppendRevolvePolygon needs at least
    # three vertices, and a printed band has thickness anyway. Outer face sits
    # 0.4 mm proud of the cup wall so it never z-fights the foam.
    # UNIT sized, exactly like SM_LabelSleeve: base radius 1, top radius 1.28
    # (the cup's own taper), height 1. The scene scales it to centimetres.
    #
    # This is not a style choice. The cylindrical UV projection normalises
    # against the mesh's own extent, so a sleeve authored at real centimetres
    # comes out with the artwork tiled dozens of times around the cup — which
    # is exactly what happened: the label rendered as vertical red streaks.
    # Authoring at unit size is what makes the bottle labels read, and the cup
    # has to follow the same rule.
    profile = [
        (1.000, 0.0), (1.283, 1.0), (1.271, 1.0), (0.988, 0.0),
    ]
    revolve(mesh, profile, steps=48)

    project = None
    if UVS is not None:
        project = (getattr(UVS, "set_mesh_u_vs_from_cylinder_projection", None)
                   or getattr(UVS, "set_mesh_uvs_from_cylinder_projection", None))
    if project is not None:
        selection = unreal.GeometryScriptMeshSelection()
        # Centre the projection on the band so V spans it 0..1.
        project(mesh, 0, xf(location=(0.0, 0.0, 0.5)), selection, 89.0)
    else:
        log("  cylinder UV projection unavailable; cup sleeve UVs left as generated")
    return bake(mesh, "SM_CupSleeve", add_collision=False)


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


def build_snack_bag():
    """Pillow-shaped snack bag: rounded body with flat crimped seals."""
    mesh = new_mesh()
    box(mesh, (16.0, 5.5, 20.0), location=(0.0, 0.0, 3.0), scale_to_fill=True)
    bevel_all(mesh, distance=1.5, angle_threshold=20.0)
    box(mesh, (16.4, 0.5, 2.6), location=(0.0, 0.0, 14.2), scale_to_fill=True)
    box(mesh, (16.4, 0.5, 2.6), location=(0.0, 0.0, -8.2), scale_to_fill=True)
    return bake(mesh, "SM_SnackBag")


def build_alarm_clock():
    """Clock radio: beveled shell, recessed LED face, button row, feet."""
    mesh = new_mesh()
    box(mesh, (13.0, 8.5, 6.4), location=(0.0, 0.0, 3.2))
    bevel_all(mesh, distance=0.5)
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
    for foot_x, foot_y in ((-5.0, -3.0), (5.0, -3.0), (-5.0, 3.0), (5.0, 3.0)):
        foot = new_mesh()
        cylinder(foot, 0.7, 0.5, location=(foot_x, foot_y, -0.4))
        options = unreal.GeometryScriptMeshBooleanOptions()
        BOOL_LIB.apply_mesh_boolean(
            mesh, xf(), foot, xf(),
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
    PRIM.append_cylinder(
        mesh, prim_options(scale_to_fill=True), xf(), 1.0, 1.0, 48, 1, False,
        unreal.GeometryScriptPrimitiveOriginMode.BASE)

    # AppendCylinder ignores the ScaleToFill UV mode, so its UVs come out
    # scaled by world size — on a unit cylinder that collapses the whole
    # label into one texel. Project them explicitly instead: U wraps the
    # axis once, V runs 0..1 up the band.
    # The binding spells UVs as "u_vs"; accept either form.
    project = None
    if UVS is not None:
        project = (getattr(UVS, "set_mesh_u_vs_from_cylinder_projection", None)
                   or getattr(UVS, "set_mesh_uvs_from_cylinder_projection", None))
    if project is not None:
        selection = unreal.GeometryScriptMeshSelection()
        project(mesh, 0, xf(location=(0.0, 0.0, 0.5)), selection, 89.0)
    else:
        log("  cylinder UV projection unavailable; sleeve UVs left as generated")
    return bake(mesh, "SM_LabelSleeve", add_collision=False)


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
    """Wet hoodie, hood and fully covered folded arms for the CH03 reveal.

    All submerged-body meshes share the same origin. Runtime can therefore
    overlap the three material groups at one transform without skeletal skin,
    exposed hands or component-by-component primitive assembly.
    """
    mesh = new_mesh()

    # Back and shoulders form the first read through dark tank water. The
    # silhouette follows the approved ImageGen sheet but retains the exact
    # gameplay footprint of the existing greybox reveal.
    ellipsoid(
        mesh, (92.0, 46.0, 25.0), location=(8.0, 4.0, 0.0),
        rotation=(0.0, 16.0, -3.0), steps=32)
    ellipsoid(
        mesh, (34.0, 42.0, 24.0), location=(34.0, 4.0, 2.0),
        rotation=(0.0, 12.0, -4.0), steps=24)

    # Raised shoulder, tucked hood and compressed crown. No separate face or
    # skin mesh exists, so no camera angle can accidentally reveal one.
    ellipsoid(
        mesh, (31.0, 29.0, 29.0), location=(62.0, 15.0, 3.0),
        rotation=(0.0, 9.0, 0.0), steps=24)
    ellipsoid(
        mesh, (36.0, 34.0, 22.0), location=(66.0, 18.0, 10.0),
        rotation=(0.0, 14.0, 8.0), steps=24)
    ellipsoid(
        mesh, (19.0, 20.0, 18.0), location=(50.0, 11.0, 0.0),
        rotation=(0.0, 12.0, 0.0), steps=20)

    # Both arms fold inward. Slightly enlarged cuffs overlap the sleeve tubes
    # and hide the hands completely, even under refraction or close flashlight.
    arm_paths = (
        (6.75, [(30.0, -10.0, 1.0), (7.0, -38.0, -2.0)]),
        (5.75, [(7.0, -38.0, -2.0), (-20.0, -24.0, -5.0)]),
        (6.75, [(31.0, 20.0, 1.0), (4.0, 44.0, -2.0)]),
        (5.75, [(4.0, 44.0, -2.0), (-25.0, 31.0, -5.0)]),
    )
    for radius, path in arm_paths:
        _append_round_path(mesh, radius, path)
        ellipsoid(
            mesh,
            (radius * 2.12, radius * 2.12, radius * 1.72),
            location=path[-1],
            steps=18)
    ellipsoid(mesh, (17.0, 13.0, 10.0), location=(-23.0, -22.0, -5.0), steps=18)
    ellipsoid(mesh, (17.0, 13.0, 10.0), location=(-28.0, 29.0, -5.0), steps=18)
    return bake(mesh, "SM_SubmergedHoodieCurl", add_collision=False)


def build_submerged_pants_curl():
    """Loose black training pants in the same back-facing curled pose."""
    mesh = new_mesh()
    ellipsoid(
        mesh, (54.0, 44.0, 28.0), location=(-34.0, -3.0, -2.0),
        rotation=(0.0, 12.0, 0.0), steps=28)

    leg_paths = (
        (10.0, [(-30.0, -8.0, -2.0), (-63.0, -42.0, -4.0)]),
        (8.25, [(-63.0, -42.0, -4.0), (-107.0, -21.0, -6.0)]),
        (10.0, [(-33.0, 8.0, -2.0), (-62.0, 38.0, -3.0)]),
        (8.25, [(-62.0, 38.0, -3.0), (-102.0, 27.0, -6.0)]),
    )
    for radius, path in leg_paths:
        _append_round_path(mesh, radius, path)
        ellipsoid(
            mesh,
            (radius * 2.18, radius * 2.05, radius * 1.72),
            location=path[-1],
            steps=18)

    # Long gathered cuffs overlap the sock-shaped foot volumes in the slipper
    # group. This deliberately removes the ankle skin visible in the reference.
    ellipsoid(
        mesh, (23.0, 18.0, 12.0), location=(-105.0, -21.0, -6.0),
        rotation=(0.0, -19.0, 0.0), steps=20)
    ellipsoid(
        mesh, (23.0, 18.0, 12.0), location=(-101.0, 27.0, -6.0),
        rotation=(0.0, 11.0, 0.0), steps=20)
    return bake(mesh, "SM_SubmergedPantsCurl", add_collision=False)


def build_submerged_slippers_curl():
    """Black slide slippers plus fully covered feet for the identity reveal."""
    mesh = new_mesh()
    slipper_specs = (
        ((-115.0, -17.0, -7.0), -19.0),
        ((-111.0, 25.0, -7.0), 11.0),
    )
    for location, yaw in slipper_specs:
        # Thin worn sole, black sock/covered foot, and a broad upper strap.
        ellipsoid(mesh, (31.0, 17.0, 5.5), location=location,
                  rotation=(0.0, yaw, 0.0), steps=24)
        ellipsoid(
            mesh, (25.0, 12.0, 7.5),
            location=(location[0] + 1.5, location[1], location[2] + 4.5),
            rotation=(0.0, yaw, 0.0), steps=20)
        ellipsoid(
            mesh, (12.5, 18.0, 6.0),
            location=(location[0] + 4.0, location[1], location[2] + 7.0),
            rotation=(0.0, yaw, 0.0), steps=20)
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
    build_alley_cat_run,
    build_first_person_hoodie_sleeve,
    build_p3_service_cabinet_shell,
    build_p3_service_manifold,
    build_p3_large_valve_wheel,
    build_p3_small_valve_wheel,
    build_p3_pressure_gauge,
    build_submerged_hoodie_curl,
    build_submerged_pants_curl,
    build_submerged_slippers_curl,
    build_rooftop_water_tank_shell,
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
    built = 0
    for builder in BUILDERS:
        try:
            builder()
            built += 1
        except Exception as error:  # noqa: BLE001 - report and keep going
            log(f"FAILED {builder.__name__}: {error}")
    log(f"complete: {built}/{len(BUILDERS)} meshes")


if __name__ == "__main__":
    run()
