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
