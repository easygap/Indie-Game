"""공용부 벽·천장·바닥 재질에 때 마스크를 입혀 다시 만든다.

Build-GrimeMaterials.ps1이 콘텐츠 전용 프로젝트(ArtImport)에서 돌린다.
재질 그래프는 retail_surface_contract.author가 전부 다시 짠다.
"""

from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.SystemLibrary.get_project_directory())
sys.path.insert(0, str(ROOT / "Scripts"))
import retail_surface_contract  # noqa: E402

ASSETS = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
FINISHES = (
    ("M_Stucco_X", "landing_wall"),
    ("M_Stucco_Y", "landing_wall"),
    ("M_StuccoCeil", "landing_ceiling"),
    ("M_StuccoDado_X", "landing_dado"),
    ("M_StuccoDado_Y", "landing_dado"),
    ("M_GraniteTile_XY", "granite"),
)

for name, finish in FINISHES:
    material = unreal.load_asset(f"/Game/Prototype/Materials/{name}")
    if not material:
        raise RuntimeError(f"공용부 재질이 없다: {name}")
    retail_surface_contract.author(material, finish)
    ASSETS.save_loaded_asset(material)

unreal.log(f"GRIME_MATERIALS PASS materials={len(FINISHES)}")
