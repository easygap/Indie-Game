"""흑연 질감과 정확한 글자 마스크를 합쳐 진행에 따라 드러나는 종이 재질을 만든다."""
import os
import unreal

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
edit = unreal.MaterialEditingLibrary
root = '/Game/Prototype'
textures = []
for filename, name, is_mask in [('ComplaintRubbing_20260917.png','T_ComplaintRubbing_D',False),
                                ('ComplaintImpressionMask.png','T_ComplaintImpression_M',True)]:
    task = unreal.AssetImportTask()
    task.filename = os.path.join(unreal.Paths.project_dir(), 'BoothSources', filename)
    task.destination_path = root + '/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(root + '/Textures/' + name)
    if not texture: raise RuntimeError('접수철 텍스처 반입 실패: ' + name)
    texture.set_editor_property('srgb', not is_mask)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS if is_mask else unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property('max_texture_size', 1024)
    texture.set_editor_property('power_of_two_mode', unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
    texture.set_editor_property('never_stream', False)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    textures.append(texture)

material = unreal.load_asset(root + '/Materials/M_ComplaintImpression')
if material is None:
    material = tools.create_asset('M_ComplaintImpression', root + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
edit.delete_all_material_expressions(material)
material.set_editor_property('two_sided', False)
# 이 장면에서 픽셀 애니메이션 플래그를 켜면 Basepass 비용이 늘었다.
# 짧은 복원 동작은 기본 TSR로 처리한다. 비교 기록: Docs/BOOTH_REVIEW_20260917.md
material.set_editor_property('has_pixel_animation', False)

def node(kind, **props):
    result = edit.create_material_expression(material, getattr(unreal, 'MaterialExpression' + kind))
    for key, value in props.items(): result.set_editor_property(key, value)
    return result

def wire(source, target, pin, output=''):
    if not edit.connect_material_expressions(source, output, target, pin):
        raise RuntimeError('재질 핀 연결 실패: ' + pin)

def scalar(value): return node('Constant', r=value)

def math_node(kind, a, b):
    result=node(kind);wire(a,result,'A');wire(b,result,'B');return result

uv=node('TextureCoordinate')
rub=node('TextureSample', texture=textures[0])
mask=node('TextureSample', texture=textures[1], sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
wire(uv,rub,'UVs');wire(uv,mask,'UVs')
v=node('ComponentMask',r=False,g=True,b=False,a=False);wire(uv,v,'')
reveal=node('ScalarParameter',parameter_name='Reveal',default_value=0.0)
# 0일 때 첫 띠 바로 위, 1/3마다 한 줄씩, 1일 때 마지막 띠까지 지난다.
front=math_node('Add',math_node('Multiply',reveal,scalar(.69)),scalar(.23))
edge=math_node('Multiply',math_node('Subtract',front,v),scalar(90))
gate=node('Clamp');wire(edge,gate,'')
rub_r=node('ComponentMask',r=True,g=False,b=False,a=False);wire(rub,rub_r,'')
dark=node('OneMinus');wire(rub_r,dark,'')
glyph=node('ComponentMask',r=True,g=False,b=False,a=False);wire(mask,glyph,'')
gap=node('OneMinus');wire(math_node('Multiply',glyph,scalar(.90)),gap,'')
ink=math_node('Multiply',math_node('Multiply',dark,gap),gate)
ink=math_node('Multiply',ink,scalar(.88))
base=node('Constant3Vector',constant=unreal.LinearColor(.78,.75,.67,1))
white=node('OneMinus');wire(ink,white,'')
color=math_node('Multiply',base,white)
form=node('ComponentMask',r=False,g=True,b=False,a=False);wire(mask,form,'')
form_white=node('OneMinus');wire(math_node('Multiply',form,scalar(.70)),form_white,'')
color=math_node('Multiply',color,form_white)
edit.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
edit.connect_material_property(scalar(.94),'',unreal.MaterialProperty.MP_ROUGHNESS)
edit.connect_material_property(scalar(.14),'',unreal.MaterialProperty.MP_SPECULAR)
edit.layout_material_expressions(material)
edit.recompile_material(material)
if not assets.save_loaded_assets(textures + [material], False): raise RuntimeError('접수철 재질 저장 실패')
unreal.log_warning('BOOTH_IMPRESSION PASS textures=2 material=1')
