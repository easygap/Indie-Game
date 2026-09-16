"""별관 벽의 압흔. 포획 연출에서 쓰는 손자국 재질과 별도로 반입한다."""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import unreal
import create_textured_materials as materials

task = unreal.AssetImportTask()
task.filename = os.path.join(unreal.Paths.project_dir(), 'PressureSources', 'AnnexPressureMask_20260916.png')
task.destination_path = '/Game/Prototype/Textures'
task.destination_name = 'T_AnnexPressure_M'
task.automated = True
task.replace_existing = True
task.save = False
tools = unreal.AssetToolsHelpers.get_asset_tools()
tools.import_asset_tasks([task])
texture = unreal.load_asset('/Game/Prototype/Textures/T_AnnexPressure_M')
if texture is None:
    raise RuntimeError('별관 압흔 마스크 반입 실패')
texture.set_editor_property('srgb', False)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
texture.set_editor_property('max_texture_size', 1024)
texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
assets = unreal.EditorAssetLibrary
created = materials.create_masked_texture_materials(assets, tools,
    {'M_AnnexPressure': materials.EVIDENCE_MASK_MATERIALS['M_AnnexPressure']}, True)
if len(created) != 1 or not assets.save_loaded_assets([texture] + created, False):
    raise RuntimeError('별관 압흔 저장 실패')
unreal.log_warning('PRESSURE_MARKS PASS material=1 mask=1')
