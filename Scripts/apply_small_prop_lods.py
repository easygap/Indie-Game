"""기존 절차 메시의 작은 원형 부품에도 공통 LOD 계약을 적용한다."""
import unreal
import mesh_lod_contract


def run():
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for name in mesh_lod_contract.SMALL_ROUND_CHAINS:
        mesh = unreal.load_asset('/Game/Meshes/' + name)
        if not mesh:
            raise RuntimeError(f'LOD 대상 메시 누락: {name}')
        settings = [unreal.StaticMeshReductionSettings(percent_triangles=1., screen_size=1.)]
        for _, percent, screen in mesh_lod_contract.lod_plan(name):
            settings.append(unreal.StaticMeshReductionSettings(percent_triangles=percent, screen_size=screen))
        options = unreal.StaticMeshReductionOptions(reduction_settings=settings, auto_compute_lod_screen_size=False)
        subsystem.set_lods(mesh, options)
        if subsystem.get_lod_count(mesh) != 4:
            raise RuntimeError(f'LOD 생성 실패: {name}')
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        unreal.log(f'SMALL_PROP_LOD PASS {name} chain={mesh_lod_contract.lod_plan(name)}')
