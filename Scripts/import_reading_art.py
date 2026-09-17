"""세계 안의 배송 라벨과 읽기 화면이 같은 인쇄 원본을 사용하도록 반입한다."""
import os
import unreal

task = unreal.AssetImportTask()
task.filename = os.path.join(unreal.Paths.project_dir(), 'ReadingSources', 'ShippingLabel_20260917.png')
task.destination_path = '/Game/UI/Reading'
task.destination_name = 'T_ShippingLabelRead_D'
task.automated = True
task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset('/Game/UI/Reading/T_ShippingLabelRead_D')
if not texture:
    raise RuntimeError('배송 라벨 원본을 반입하지 못했다')
texture.set_editor_property('srgb', True)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('max_texture_size', 2048)
texture.set_editor_property('power_of_two_mode', unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
if not unreal.EditorAssetLibrary.save_loaded_asset(texture, False):
    raise RuntimeError('배송 라벨 원본 저장 실패')
unreal.log('READING_ART PASS label=1 max_size=2048 compression=BC7')
