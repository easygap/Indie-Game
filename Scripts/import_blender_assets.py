"""Blender에서 구운 에셋을 UE 에셋으로 만든다.

Content/SourceArt/Blender/<SM_Name>/manifest.json 하나가 에셋 하나다. FBX는
/Game/Meshes/<SM_Name>으로, 텍스처는 /Game/Prototype/Textures/T_<Name>_{D,N,ORM,E}로,
재질은 공용 마스터 M_IGBakedProp의 인스턴스 MI_<Name>으로 들어가고 메시
슬롯에 바로 걸린다. Glass 슬롯은 기존 M_Glass를 쓴다.

게임 모듈이 없는 콘텐츠 전용 프로젝트(Import-BlenderAssets.ps1이 만든다)에서
돈다. Smart App Control이 게임 DLL을 막아도 여기는 막을 것이 없다.

    UnrealEditor-Cmd ArtImport.uproject -ExecutePythonScript=.../import_blender_assets.py

환경 변수
    IG_BLENDER_SOURCE  manifest 폴더들이 있는 루트(ASCII 경로)
    IG_ONLY            쉼표로 나눈 에셋 이름. 비우면 전부
    IG_BOUNDS_OUT      /Game/Meshes 전체 바운드 JSON을 쓸 경로
"""

import json
import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

import mesh_lod_contract  # noqa: E402

MESH_ROOT = "/Game/Meshes"
TEXTURE_ROOT = "/Game/Prototype/Textures"
MATERIAL_ROOT = "/Game/Prototype/Materials"
MASTER_NAME = "M_IGBakedProp"
GLASS_MATERIAL = f"{MATERIAL_ROOT}/M_Glass"
MASTER_MARKER = "IG_BakedProp_v1"


def log(message):
    unreal.log_warning(f"[BLENDER_IMPORT] {message}")


def _set(target, values):
    for name, value in values:
        try:
            target.set_editor_property(name, value)
        except Exception as error:  # noqa: BLE001 - 마이너마다 이름이 다르다
            log(f"  property skipped {name}: {error}")


# --------------------------------------------------------------------------
# 텍스처
# --------------------------------------------------------------------------

def import_texture(path, asset_name, role):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = TEXTURE_ROOT
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(f"{TEXTURE_ROOT}/{asset_name}")
    if texture is None:
        raise RuntimeError(f"texture import failed: {path}")
    if role == "N":
        _set(texture, (
            ("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP),
            ("srgb", False),
            ("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP),
        ))
    elif role == "ORM":
        _set(texture, (
            ("compression_settings", unreal.TextureCompressionSettings.TC_MASKS),
            ("srgb", False),
            ("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_SPECULAR),
        ))
    else:
        _set(texture, (
            ("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT),
            ("srgb", True),
            ("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD),
        ))
    return texture


# --------------------------------------------------------------------------
# 마스터 재질
# --------------------------------------------------------------------------

def _expr(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def _connect(a, a_pin, b, b_pin):
    unreal.MaterialEditingLibrary.connect_material_expressions(a, a_pin, b, b_pin)


def _connect_property(expression, pin, material_property):
    unreal.MaterialEditingLibrary.connect_material_property(expression, pin, material_property)


def _linear_default_texture():
    """Linear Color 샘플러에 끼울 기본 텍스처. 엔진 노이즈가 sRGB가 아니면 그것,
    아니면 우리가 반입한 ORM 하나."""
    candidate = unreal.load_asset("/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq")
    if candidate is not None and not candidate.get_editor_property("srgb"):
        return candidate
    for asset_path in unreal.EditorAssetLibrary.list_assets(TEXTURE_ROOT, recursive=False):
        if asset_path.split("/")[-1].split(".")[0].endswith("_ORM"):
            texture = unreal.load_asset(asset_path)
            if texture is not None and not texture.get_editor_property("srgb"):
                return texture
    return None


def _normal_default_texture():
    candidate = unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")
    if candidate is not None and not candidate.get_editor_property("srgb"):
        return candidate
    for asset_path in unreal.EditorAssetLibrary.list_assets(TEXTURE_ROOT, recursive=False):
        if asset_path.split("/")[-1].split(".")[0].endswith("_N"):
            texture = unreal.load_asset(asset_path)
            if texture is not None and not texture.get_editor_property("srgb"):
                return texture
    return None


def fill_master_defaults(material):
    """Normal·ORM 텍스처 파라미터에 기본 텍스처를 준다.

    비워 두면 엔진이 sRGB DefaultTexture를 끼우고, 샘플러 타입(Normal / Linear
    Color)과 맞지 않아 마스터가 컴파일에 실패한다. 게임은 그때 기본 회색 재질로
    그려서 인스턴스 전부가 회색이 된다 — 2026-09-08 프롤로그 캡처에서 그랬다.
    """
    normal_default = _normal_default_texture()
    linear_default = _linear_default_texture()
    changed = False
    for expression in unreal.MaterialEditingLibrary.get_material_expressions(material):
        if not isinstance(expression, unreal.MaterialExpressionTextureSampleParameter2D):
            continue
        name = str(expression.get_editor_property("parameter_name"))
        current = expression.get_editor_property("texture")
        if name == "Normal" and normal_default is not None and current is not normal_default:
            expression.set_editor_property("texture", normal_default)
            changed = True
        elif name == "ORM" and linear_default is not None and current is not linear_default:
            expression.set_editor_property("texture", linear_default)
            changed = True
    if normal_default is None or linear_default is None:
        raise RuntimeError("마스터 재질 기본 텍스처를 찾지 못했다 (Normal/ORM)")
    return changed


def ensure_master_material():
    path = f"{MATERIAL_ROOT}/{MASTER_NAME}"
    existing = unreal.load_asset(path)
    if existing is not None:
        # 기본 텍스처가 비어 있던 첫 판을 제자리에서 고친다. 새로 만들면 인스턴스
        # 참조가 흔들린다.
        if fill_master_defaults(existing):
            unreal.MaterialEditingLibrary.recompile_material(existing)
            unreal.EditorAssetLibrary.save_asset(path, False)
            log(f"master material defaults filled: {path}")
        # 마커가 있으면 이미 이 버전으로 만든 것이다. 재질을 매번 새로 만들면
        # 인스턴스 참조가 흔들리고 셰이더도 다시 컴파일된다.
        for expression in unreal.MaterialEditingLibrary.get_material_expressions(existing) if hasattr(unreal.MaterialEditingLibrary, "get_material_expressions") else []:
            if expression.get_editor_property("desc") == MASTER_MARKER:
                return existing
        try:
            if existing.get_editor_property("asset_user_data") is not None:
                pass
        except Exception:  # noqa: BLE001
            pass
        # 마커를 못 읽으면 그냥 쓴다. 표현식 목록 API가 없는 버전이다.
        return existing

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(MASTER_NAME, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("could not create master material")

    base = _expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, -300)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    base.set_editor_property("desc", MASTER_MARKER)
    tint = _expr(material, unreal.MaterialExpressionVectorParameter, -700, -100)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tinted = _expr(material, unreal.MaterialExpressionMultiply, -400, -250)
    _connect(base, "RGB", tinted, "A")
    _connect(tint, "", tinted, "B")
    _connect_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = _expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, 100)
    normal.set_editor_property("parameter_name", "Normal")
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    normal_strength = _expr(material, unreal.MaterialExpressionScalarParameter, -700, 300)
    normal_strength.set_editor_property("parameter_name", "NormalStrength")
    normal_strength.set_editor_property("default_value", 1.0)
    # 노멀 세기: XY만 배율, Z는 그대로. 손전등 아래 요철을 에셋마다 조절한다.
    flat = _expr(material, unreal.MaterialExpressionConstant3Vector, -700, 400)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    scaled = _expr(material, unreal.MaterialExpressionLinearInterpolate, -400, 150)
    _connect(flat, "", scaled, "A")
    _connect(normal, "RGB", scaled, "B")
    _connect(normal_strength, "", scaled, "Alpha")
    _connect_property(scaled, "", unreal.MaterialProperty.MP_NORMAL)

    orm = _expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, 600)
    orm.set_editor_property("parameter_name", "ORM")
    orm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    _connect_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    rough_scale = _expr(material, unreal.MaterialExpressionScalarParameter, -700, 800)
    rough_scale.set_editor_property("parameter_name", "RoughnessScale")
    rough_scale.set_editor_property("default_value", 1.0)
    rough = _expr(material, unreal.MaterialExpressionMultiply, -400, 650)
    _connect(orm, "G", rough, "A")
    _connect(rough_scale, "", rough, "B")
    _connect_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    _connect_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)

    emissive = _expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, 1000)
    emissive.set_editor_property("parameter_name", "Emissive")
    emissive.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    black = unreal.load_asset("/Engine/EngineResources/Black")
    if black is not None:
        emissive.set_editor_property("texture", black)
    emissive_strength = _expr(material, unreal.MaterialExpressionScalarParameter, -700, 1200)
    emissive_strength.set_editor_property("parameter_name", "EmissiveStrength")
    emissive_strength.set_editor_property("default_value", 0.0)
    glow = _expr(material, unreal.MaterialExpressionMultiply, -400, 1050)
    _connect(emissive, "RGB", glow, "A")
    _connect(emissive_strength, "", glow, "B")
    _connect_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    fill_master_defaults(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path, False)
    log(f"master material created: {path}")
    return material


def create_instance(name, master, textures, emissive_strength=1.0):
    path = f"{MATERIAL_ROOT}/{name}"
    instance = unreal.load_asset(path)
    if instance is None:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        instance = tools.create_asset(name, MATERIAL_ROOT, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
        if instance is None:
            raise RuntimeError(f"could not create {path}")
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, master)
    lib = unreal.MaterialEditingLibrary
    lib.set_material_instance_texture_parameter_value(instance, "BaseColor", textures["D"])
    lib.set_material_instance_texture_parameter_value(instance, "Normal", textures["N"])
    lib.set_material_instance_texture_parameter_value(instance, "ORM", textures["ORM"])
    if "E" in textures:
        lib.set_material_instance_texture_parameter_value(instance, "Emissive", textures["E"])
        lib.set_material_instance_scalar_parameter_value(
            instance, "EmissiveStrength", max(1.0, float(emissive_strength)))
    else:
        lib.set_material_instance_scalar_parameter_value(instance, "EmissiveStrength", 0.0)
    lib.update_material_instance(instance)
    unreal.EditorAssetLibrary.save_asset(path, False)
    return instance


# --------------------------------------------------------------------------
# 메시
# --------------------------------------------------------------------------

_LEGACY_FBX_ARMED = False


def use_legacy_fbx_importer():
    """UCX 충돌 껍데기는 예전 FBX 임포터만 읽는다.

    5.8은 FBX를 Interchange로 넘기고, 그 경로는 task.options의 FbxImportUI를
    무시한 채 UCX_ 노드를 버렸다(convex=0). 기능 플래그를 끄면 FbxFactory가
    다시 잡고 UCX가 볼록 충돌로 들어온다(probe_collision2.py로 실측).
    """
    global _LEGACY_FBX_ARMED
    if not _LEGACY_FBX_ARMED:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        _LEGACY_FBX_ARMED = True


def import_fbx(path, name):
    use_legacy_fbx_importer()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_path = f"{MESH_ROOT}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = MESH_ROOT
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    options = unreal.FbxImportUI()
    _set(options, (
        ("import_mesh", True),
        ("import_as_skeletal", False),
        ("import_materials", False),
        ("import_textures", False),
        ("create_physics_asset", False),
        ("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH),
    ))
    sm = options.static_mesh_import_data
    _set(sm, (
        ("combine_meshes", True),
        ("auto_generate_collision", False),
        ("remove_degenerates", True),
        ("generate_lightmap_u_vs", True),
        ("build_nanite", False),
        ("build_reversed_index_buffer", True),
        ("import_uniform_scale", 1.0),
        ("convert_scene", True),
        ("force_front_x_axis", False),
        ("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS),
        ("normal_generation_method", unreal.FBXNormalGenerationMethod.MIKK_T_SPACE),
        ("compute_weighted_normals", True),
        ("one_convex_hull_per_ucx", True),
        ("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE),
    ))
    task.options = options
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(asset_path)
    if mesh is None:
        raise RuntimeError(f"fbx import failed: {path}")
    return mesh


def mesh_class_for(manifest):
    name = manifest["name"]
    wanted = manifest.get("mesh_class")
    cls = mesh_lod_contract.MESH_CLASSES.get(wanted) if wanted else None
    return cls or mesh_lod_contract.classify(name)


def apply_lod_contract(mesh, name, mesh_class):
    """generate_meshes.py의 것과 같은 계약. 절차 메시와 같은 사슬·라이트맵."""
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    plan = [(index + 1, percent, screen) for index, (percent, screen) in enumerate(mesh_class.chain)]
    settings = []
    lod0 = unreal.StaticMeshReductionSettings()
    _set(lod0, (("percent_triangles", 1.0), ("screen_size", 1.0)))
    settings.append(lod0)
    for _, percent, screen in plan:
        entry = unreal.StaticMeshReductionSettings()
        _set(entry, (("percent_triangles", percent), ("screen_size", screen)))
        settings.append(entry)
    options = unreal.StaticMeshReductionOptions()
    _set(options, (("reduction_settings", settings), ("auto_compute_lod_screen_size", False)))
    subsystem.set_lods(mesh, options)

    build = unreal.MeshBuildSettings()
    _set(build, (
        ("recompute_normals", False),
        ("recompute_tangents", False),
        ("use_mikk_t_space", True),
        ("remove_degenerates", True),
        ("generate_lightmap_u_vs", True),
        ("src_lightmap_index", 0),
        ("dst_lightmap_index", 1),
        ("min_lightmap_resolution", mesh_class.lightmap_resolution),
    ))
    subsystem.set_lod_build_settings(mesh, 0, build)
    _set(mesh, (
        ("light_map_resolution", mesh_class.lightmap_resolution),
        ("light_map_coordinate_index", 1),
    ))
    log(f"  {name} LOD chain={mesh_class.lod_count} class={mesh_class.name}")


def assign_materials(mesh, instance):
    glass = unreal.load_asset(GLASS_MATERIAL)
    materials = list(mesh.static_materials)
    for index, slot in enumerate(materials):
        slot_name = str(slot.material_slot_name)
        if slot_name == "Glass" and glass is not None:
            mesh.set_material(index, glass)
        else:
            mesh.set_material(index, instance)
    return [str(s.material_slot_name) for s in materials]


def triangle_count(mesh):
    """소스 모델 LOD0 삼각형 수. 5.8에는 get_number_triangles가 없다."""
    try:
        dynamic = unreal.DynamicMesh()
        read_lod = unreal.GeometryScriptMeshReadLOD()
        read_lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.SOURCE_MODEL)
        read_lod.set_editor_property("lod_index", 0)
        result = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
            mesh, dynamic, unreal.GeometryScriptCopyMeshFromAssetOptions(), read_lod)
        dynamic = result[0] if isinstance(result, tuple) else dynamic
        return dynamic.get_triangle_count()
    except Exception as error:  # noqa: BLE001
        log(f"  triangle count unavailable: {error}")
        return -1


def collision_summary(mesh):
    body = mesh.get_editor_property("body_setup")
    if body is None:
        return "none"
    agg = body.get_editor_property("agg_geom")
    return "convex=%d box=%d" % (
        len(agg.get_editor_property("convex_elems")),
        len(agg.get_editor_property("box_elems")))


# --------------------------------------------------------------------------
# 바운드
# --------------------------------------------------------------------------

def export_bounds(output):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([MESH_ROOT], True)
    exported = {}
    for data in registry.get_assets_by_path(MESH_ROOT, recursive=True):
        asset = data.get_asset()
        if not isinstance(asset, unreal.StaticMesh):
            continue
        b = asset.get_bounds()
        exported[str(data.asset_name)] = {
            "origin": [round(b.origin.x, 4), round(b.origin.y, 4), round(b.origin.z, 4)],
            "extent": [round(b.box_extent.x, 4), round(b.box_extent.y, 4), round(b.box_extent.z, 4)],
        }
    os.makedirs(os.path.dirname(output), exist_ok=True)
    with open(output, "w", encoding="utf-8") as handle:
        json.dump({"meshes": dict(sorted(exported.items()))}, handle, indent=2, ensure_ascii=False)
        handle.write("\n")
    log(f"bounds exported: {len(exported)} meshes -> {output}")


# --------------------------------------------------------------------------

def guard_name_collision(name):
    """이 에셋 이름으로 만들 텍스처·재질이 남의 것과 겹치면 멈춘다.

    SM_SwitchPlate를 반입하다가 간판 텍스처 T_SwitchPlate_D(Create-SignTextures.ps1,
    M_SwitchPlate가 읽는다)를 덮어쓴 적이 있다. 우리 것이라면 MI_<이름>이 같이
    있고, 없이 T_<이름>_D만 있으면 다른 파이프라인의 텍스처다.
    """
    short = name[3:]
    instance = f"{MATERIAL_ROOT}/MI_{short}"
    if unreal.EditorAssetLibrary.does_asset_exist(instance):
        return
    for role in ("D", "N", "ORM", "E"):
        texture = f"{TEXTURE_ROOT}/T_{short}_{role}"
        if unreal.EditorAssetLibrary.does_asset_exist(texture):
            raise RuntimeError(
                f"{name}: {texture} 가 이미 있는데 MI_{short} 가 없다. 다른 파이프라인의 "
                f"텍스처와 이름이 겹친다. 에셋 이름을 바꿔라.")


def import_asset(source_dir, manifest, master):
    name = manifest["name"]
    log(f"importing {name}")
    guard_name_collision(name)
    textures = {}
    for role, filename in manifest["textures"].items():
        textures[role] = import_texture(os.path.join(source_dir, filename), f"T_{name[3:]}_{role}", role)
    instance = create_instance(f"MI_{name[3:]}", master, textures,
                               manifest.get("emissive_strength", 1.0))
    mesh = import_fbx(os.path.join(source_dir, manifest["fbx"]), name)
    slots = assign_materials(mesh, instance)
    apply_lod_contract(mesh, name, mesh_class_for(manifest))
    unreal.EditorAssetLibrary.save_asset(f"{MESH_ROOT}/{name}", False)
    for texture in textures.values():
        unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    b = mesh.get_bounds()
    log(f"  {name} slots={slots} tris={triangle_count(mesh)} lods={mesh.get_num_lods()} "
        f"{collision_summary(mesh)} bounds=({b.origin.x - b.box_extent.x:.1f},{b.origin.y - b.box_extent.y:.1f},"
        f"{b.origin.z - b.box_extent.z:.1f})..({b.origin.x + b.box_extent.x:.1f},{b.origin.y + b.box_extent.y:.1f},"
        f"{b.origin.z + b.box_extent.z:.1f})")
    return name


def main():
    source_root = os.environ.get("IG_BLENDER_SOURCE")
    if not source_root or not os.path.isdir(source_root):
        raise RuntimeError(f"IG_BLENDER_SOURCE is not a folder: {source_root}")
    only = [s.strip() for s in os.environ.get("IG_ONLY", "").split(",") if s.strip()]
    manifests = []
    for entry in sorted(os.listdir(source_root)):
        path = os.path.join(source_root, entry, "manifest.json")
        if not os.path.isfile(path):
            continue
        if only and entry not in only:
            continue
        with open(path, "r", encoding="utf-8") as handle:
            manifests.append((os.path.join(source_root, entry), json.load(handle)))
    if not manifests:
        raise RuntimeError("no manifests found")

    master = ensure_master_material()
    imported = [import_asset(source_dir, manifest, master) for source_dir, manifest in manifests]

    bounds_out = os.environ.get("IG_BOUNDS_OUT")
    if bounds_out:
        export_bounds(bounds_out)
    log(f"BLENDER_IMPORT PASS assets={len(imported)}: {', '.join(imported)}")


main()
