"""반입된 현관·관리실 소품의 LOD와 재질, 실제 삼각형 수를 검사한다."""
import json
import os
import unreal

editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
results=[]
for name in ('SM_VideoIntercom','SM_HangingWorkVest','SM_ShippingLabels'):
    mesh=unreal.load_asset('/Game/Meshes/'+name)
    if not isinstance(mesh,unreal.StaticMesh):
        raise RuntimeError('반입 메시 없음: '+name)
    lods=editor.get_lod_count(mesh)
    if lods!=4:
        raise RuntimeError(f'{name}: LOD가 네 단계가 아님 ({lods})')
    dynamic=unreal.DynamicMesh()
    lod=unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property('lod_type',unreal.GeometryScriptLODType.SOURCE_MODEL)
    lod.set_editor_property('lod_index',0)
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        mesh,dynamic,unreal.GeometryScriptCopyMeshFromAssetOptions(),lod)
    triangles=dynamic.get_triangle_count()
    if not 0<triangles<=3000:
        raise RuntimeError(f'{name}: 근접 메시 예산 초과 ({triangles})')
    slots=list(mesh.static_materials)
    if len(slots)!=1:
        raise RuntimeError(f'{name}: 재질 슬롯 수가 하나가 아님')
    material=slots[0].material_interface
    if not isinstance(material,unreal.MaterialInstanceConstant):
        raise RuntimeError(f'{name}: 소품 재질 인스턴스 없음')
    strength=unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material,'EmissiveStrength')
    if abs(strength)>.0001:
        raise RuntimeError(f'{name}: 발광이 남음 ({strength})')
    textures={}
    for parameter in ('BaseColor','Normal','ORM'):
        texture=unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(material,parameter)
        if not texture or texture.get_editor_property('never_stream'):
            raise RuntimeError(f'{name}: {parameter} 텍스처 스트리밍 확인 실패')
        textures[parameter]=texture.get_path_name()
    results.append(dict(mesh=name,lods=lods,lod0_triangles=triangles,material_slots=len(slots),
        emissive_strength=strength,streaming_textures=textures))
with open(os.environ['IG_ENTRY_AUDIT_OUT'],'w',encoding='utf-8') as handle:
    json.dump(results,handle,ensure_ascii=False,indent=2)
    handle.write('\n')
unreal.log('ENTRY_ASSETS PASS meshes=3 lods=4 budget=3000 emissive=0 streaming=1')
