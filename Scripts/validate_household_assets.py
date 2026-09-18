"""반입된 생활 소품의 LOD·재질·충돌·스트리밍 상태를 읽어서 검사한다."""
import json
from pathlib import Path
import unreal


def main():
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    checks = []
    for name, limit in [('SM_HouseSlipper', 3000), ('SM_AcOutdoorUnit', 9000),
                        ('SM_WitnessWaterBowl', 3000), ('SM_WitnessPrescriptionEnvelope', 3000),
                        ('SM_WitnessStoreRoster', 3000), ('SM_WitnessCigaretteButts', 3000)]:
        mesh = unreal.load_asset('/Game/Meshes/' + name)
        assert mesh, name
        count = subsystem.get_lod_count(mesh)
        tris = [mesh.get_num_triangles(i) for i in range(count)]
        # 80삼각형 약봉투는 축약기가 64개에서 멈춘다. 먼 단계끼리 같을 수는
        # 있지만, 가까운 단계보다 늘거나 전체 사슬이 복제되어서는 안 된다.
        assert count == 4 and all(a >= b > 0 for a, b in zip(tris, tris[1:])), (name, tris)
        assert tris[-1] < tris[0], (name, tris)
        assert 0 < tris[0] <= limit and len(mesh.static_materials) == 1, (name, tris)
        body = mesh.get_editor_property('body_setup').get_editor_property('agg_geom')
        convex = len(body.get_editor_property('convex_elems'))
        assert convex == 1, (name, convex)
        textures = {}
        for role in ['D', 'N', 'ORM']:
            texture = unreal.load_asset('/Game/Prototype/Textures/T_' + name[3:] + '_' + role)
            assert texture and not texture.get_editor_property('never_stream'), (name, role)
            textures[role] = [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]
        checks.append(dict(mesh=name, lod_triangles=tris, material_slots=1,
                           convex_hulls=convex, streaming_texture_sizes=textures))
    target = Path(unreal.Paths.project_dir()) / 'Docs/Performance/Household20260918-assets.json'
    target.write_text(json.dumps(checks, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    unreal.log('HOUSEHOLD_ASSETS PASS meshes=6')


if __name__ == '__main__':
    main()
