"""매장 마감, 조판한 간판·라벨, 공용 인쇄 아틀라스를 함께 갱신한다."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import unreal
import create_textured_materials as materials
from import_texture_atlas import import_texture_atlas


def run():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    texture_names = ["T_SignMain_D", "T_SignBlade_D", "T_RetailTobaccoAd_D", "T_NeighborhoodDelivery_D", "T_LabelWater1L_D", "T_LabelWater2L_D"]
    texture_names += [f"T_RetailPrice{sku}_D" for sku in ("Potato", "Shrimp", "Corn", "CupBeef", "CupKimchi", "Biscuit")]
    for name in texture_names:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(unreal.Paths.project_content_dir(), "SourceArt", name+".png")
        task.destination_path = "/Game/Prototype/Textures"
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    import_texture_atlas()
    finish_specs = {name: spec for name, spec in materials.TEXTURED_MATERIALS.items() if spec.get("retail_finish")}
    updated = materials.create_textured_materials(assets, tools, finish_specs, update_in_place=True)
    # 아틀라스의 사각형 위치가 바뀌므로 모든 인쇄 재질을 같은 패스로 갱신한다.
    updated += materials.create_flat_texture_materials(assets, tools, materials.DECAL_MATERIALS, False, update_in_place=True)
    updated += materials.create_flat_texture_materials(assets, tools, materials.SIGN_MATERIALS, True, update_in_place=True)
    materials.OPTICAL_PROP_MATERIALS = {"M_RetailPET": materials.OPTICAL_PROP_MATERIALS["M_RetailPET"]}
    updated += materials.create_optical_prop_materials(assets, tools, update_in_place=True)
    updated += materials.create_masked_texture_materials(assets, tools, {"M_MissingFloorCavityScratches": materials.EVIDENCE_MASK_MATERIALS["M_MissingFloorCavityScratches"]}, True)
    master = unreal.load_asset("/Game/Prototype/Materials/M_IGBakedProp")
    master.set_editor_property("used_with_instanced_static_meshes", True)
    updated.append(master)
    for material in updated:
        if material.get_name() in materials.INSTANCED_PRODUCT_MATERIALS:
            material.set_editor_property("used_with_instanced_static_meshes", True)
        if material.get_name() in materials.WRAPPED_LABEL_MATERIALS:
            material.set_editor_property("two_sided", True)
        unreal.MaterialEditingLibrary.recompile_material(material)
    if len(finish_specs) != 5 or not assets.save_loaded_assets(updated, False):
        raise RuntimeError("매장 재질 저장 실패")
    unreal.log_warning(f"RETAIL_REFRESH PASS materials={len(updated)} finishes={len(finish_specs)}")


if __name__ == "__main__":
    run()
