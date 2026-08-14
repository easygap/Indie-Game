"""Create the textured PBR material set for the prologue realism pass.

Requires the textures imported by generate_surface_textures.py. Architecture
materials sample in world space (per-axis variants) so scaled greybox blocks
never stretch their textures; prop materials use mesh UVs so movable physics
objects carry their surface with them.
"""

import os

import unreal


MATERIAL_ROOT = "/Game/Prototype/Materials"
TEXTURE_ROOT = "/Game/Prototype/Textures"

# mapping: XY (floors/ceilings), XZ (walls running along X), YZ (walls along Y),
#          UV (mesh UVs with a tiling multiplier)
# tile: world centimeters per texture repeat (or UV multiplier for UV mapping)
TEXTURED_MATERIALS = {
    # --- apartment ---------------------------------------------------------
    # Tile sizes are the real-world repeat of the surface: a 1K photo over a
    # 115 cm span is ~9 px/cm, which is what makes floors and walls hold up
    # when the camera is a metre away.
    "M_Jangpan":        {"tex": "Jangpan", "mapping": "XY", "tile": 115.0},
    "M_Wallpaper_X":    {"tex": "ApartmentWallpaperV2", "mapping": "XZ", "tile": 165.0,
                         "rough": 0.86, "ao": True, "tint": (0.72, 0.70, 0.65)},
    "M_Wallpaper_Y":    {"tex": "ApartmentWallpaperV2", "mapping": "YZ", "tile": 165.0,
                         "rough": 0.86, "ao": True, "tint": (0.72, 0.70, 0.65)},
    "M_WallpaperCeil":  {"tex": "ApartmentWallpaperV2", "mapping": "XY", "tile": 220.0,
                         "rough": 0.92, "ao": True, "desaturate": 0.82,
                         "tint": (0.48, 0.48, 0.46)},
    "M_WoodFurnitureUV": {"tex": "WoodDark", "mapping": "UV", "tile": 1.0, "rough": 0.55},
    "M_BeddingUV":      {"tex": "Blanket", "mapping": "UV", "tile": 2.0, "rough": 0.95},
    # --- alley -------------------------------------------------------------
    # (M_AsphaltWorld is built by create_wet_asphalt: dew puddles + mirror wet)
    "M_Brick_X":        {"tex": "Brick", "mapping": "XZ", "tile": 210.0, "rough": 0.9},
    "M_Brick_Y":        {"tex": "Brick", "mapping": "YZ", "tile": 210.0, "rough": 0.9},
    "M_Concrete_XY":    {"tex": "Concrete", "mapping": "XY", "tile": 150.0},
    "M_Concrete_X":     {"tex": "Concrete", "mapping": "XZ", "tile": 150.0},
    "M_Concrete_Y":     {"tex": "Concrete", "mapping": "YZ", "tile": 150.0},
    "M_Shutter_X":      {"tex": "Shutter", "mapping": "XZ", "tile": 130.0},
    "M_ConcreteDark_X": {"tex": "Concrete", "mapping": "XZ", "tile": 260.0,
                         "tint": (0.32, 0.33, 0.36)},
    "M_ConcreteDark_Y": {"tex": "Concrete", "mapping": "YZ", "tile": 260.0,
                         "tint": (0.32, 0.33, 0.36)},
    # 수평면용. _X는 월드 좌표를 (X, Z)로 마스킹하므로 Z가 일정한 바닥에서는
    # V가 상수가 되어 텍스처가 한 줄로 잘려 Y 방향으로 무한히 늘어난다.
    # 5층 별관 바닥이 그 상태였고, 프레임마다 보였던 긴 평행 줄무늬가 그것이다.
    "M_ConcreteDark_XY": {"tex": "Concrete", "mapping": "XY", "tile": 260.0,
                          "tint": (0.32, 0.33, 0.36)},
    # --- store -------------------------------------------------------------
    # Shop floors are buffed to a mirror; the photo roughness map is far too
    # matte for that, so this one forces a polished value.
    "M_StoreTileWorld": {"tex": "StoreTile", "mapping": "XY", "tile": 60.0,
                         "force_rough": 0.14},
    "M_StoreCeilWorld": {"tex": "CeilingTile", "mapping": "XY", "tile": 120.0, "rough": 0.8},
    "M_StoreWall_X":    {"tex": "Concrete", "mapping": "XZ", "tile": 150.0,
                         "tint": (1.25, 1.25, 1.22)},
    "M_StoreWall_Y":    {"tex": "Concrete", "mapping": "YZ", "tile": 150.0,
                         "tint": (1.25, 1.25, 1.22)},
    "M_MetalUV":        {"tex": "MetalBrushed", "mapping": "UV", "tile": 1.0,
                         "metallic": 0.85},
    "M_ShelfSteelUV":   {"tex": "MetalBrushed", "mapping": "UV", "tile": 1.0,
                         "tint": (0.60, 0.66, 0.72), "metallic": 0.4, "rough": 0.5},
    # --- villa: corridor, lift car, facade, kitchenette ---------------------
    # A Korean walk-up villa is troweled stucco inside the hallway, 600 mm
    # speckled granite tile underfoot, and 900 mm granite cladding outside.
    # The lift car is hairline stainless over a marble floor. Roughness is
    # forced on the polished surfaces: the photo maps are far too matte to
    # give back the reflections these materials are recognised by.
    "M_Stucco_X":       {"tex": "Stucco", "mapping": "XZ", "tile": 185.0, "rough": 0.93,
                         "tint": (1.05, 1.02, 0.90)},
    "M_Stucco_Y":       {"tex": "Stucco", "mapping": "YZ", "tile": 185.0, "rough": 0.93,
                         "tint": (1.05, 1.02, 0.90)},
    "M_StuccoCeil":     {"tex": "Stucco", "mapping": "XY", "tile": 185.0, "rough": 0.95,
                         "tint": (0.94, 0.93, 0.86)},
    # The terrazzo scans are confetti at their native scale; 화강석 is the same
    # material read at a tenth the chip size with the colour taken out, so the
    # repeat is tightened hard and the chroma is desaturated away.
    "M_GraniteTile_XY": {"tex": "GraniteTile", "mapping": "XY", "tile": 17.0,
                         "desaturate": 0.9, "tint": (0.88, 0.88, 0.86),
                         "force_rough": 0.26},
    "M_GranitePanel_X": {"tex": "GranitePanel", "mapping": "XZ", "tile": 24.0,
                         "desaturate": 0.92, "tint": (0.80, 0.80, 0.78), "rough": 0.58},
    "M_GranitePanel_Y": {"tex": "GranitePanel", "mapping": "YZ", "tile": 24.0,
                         "desaturate": 0.92, "tint": (0.80, 0.80, 0.78), "rough": 0.58},
    "M_MarbleFloor_XY": {"tex": "MarbleFloor", "mapping": "XY", "tile": 130.0,
                         "desaturate": 0.55, "tint": (1.75, 1.75, 1.72),
                         "force_rough": 0.10},
    # Worktops need UV mapping, not world mapping: a world-XY stone smears
    # into stripes the moment it wraps a vertical edge or a splashback.
    "M_CounterStoneUV": {"tex": "MarbleFloor", "mapping": "UV", "tile": 1.3,
                         "desaturate": 0.7, "tint": (2.3, 2.3, 2.25),
                         "force_rough": 0.17},
    "M_StainlessUV":    {"tex": "MetalBrushed", "mapping": "UV", "tile": 1.0,
                         "tint": (1.10, 1.13, 1.16), "metallic": 1.0, "force_rough": 0.21},
    "M_CabMirrorUV":    {"tex": "MetalBrushed", "mapping": "UV", "tile": 1.0,
                         "tint": (1.22, 1.24, 1.28), "metallic": 1.0, "force_rough": 0.14},
    "M_SteelDoorUV":    {"tex": "MetalBrushed", "mapping": "UV", "tile": 1.0,
                          "tint": (0.115, 0.12, 0.132), "metallic": 0.2, "force_rough": 0.42},
    "M_KitchenGlossUV": {"tex": "MetalBrushed", "mapping": "UV", "tile": 1.0,
                          "desaturate": 1.0, "tint": (1.72, 1.70, 1.64),
                          "metallic": 0.0, "force_rough": 0.13},
    # 「없는 층」의 마른 석고 표면. 방향별 월드 매핑을 따로 두어
    # 그레이박스 벽을 늘려도 가루결과 균열의 밀도가 변하지 않는다.
    "M_MissingFloorPlaster_X": {
        "tex": "MissingFloorDryPlaster", "mapping": "XZ", "tile": 138.0,
        "rough": 0.91, "ao": True, "tint": (0.78, 0.76, 0.71),
    },
    "M_MissingFloorPlaster_Y": {
        "tex": "MissingFloorDryPlaster", "mapping": "YZ", "tile": 138.0,
        "rough": 0.91, "ao": True, "tint": (0.78, 0.76, 0.71),
    },
    "M_MissingFloorPlaster_XY": {
        "tex": "MissingFloorDryPlaster", "mapping": "XY", "tile": 138.0,
        "rough": 0.93, "ao": True, "tint": (0.74, 0.73, 0.69),
    },
}

# Lit poster/label materials: texture straight onto mesh UVs.
DECAL_MATERIALS = {
    "M_PosterSale":    {"tex_asset": "T_PosterSale_D", "rough": 0.55, "emissive_scale": 0.06},
    "M_PosterRamyeon": {"tex_asset": "T_PosterRamyeon_D", "rough": 0.55, "emissive_scale": 0.06},
    "M_PosterFlyer":   {"tex_asset": "T_PosterFlyer_D", "rough": 0.75, "flutter": True},
    "M_NoteFridge":    {"tex_asset": "T_NoteFridge_D", "rough": 0.86},
    "M_Note404NotFound": {
        "tex_asset": "T_Note404NotFound_D", "rough": 0.88, "two_sided": True,
    },
    "M_CaptureMercyNote": {
        "tex_asset": "T_CaptureMercyNote_D", "rough": 0.92, "two_sided": True,
    },
    # §20.3's world response. Same pad as the five-capture note, so the same
    # cheap recycled fibre and the same 0.92 roughness; only the words differ.
    "M_MercyNoteUnderDoor": {
        "tex_asset": "T_MercyNoteUnderDoor_D", "rough": 0.92, "two_sided": True,
    },
    # §5.5 채널 5가 저장되지 않는 이유를 손으로 만질 수 있게 하는 라벨. 마스킹
    # 테이프라 종이보다 살짝 매끈하고, 자체 발광은 없다 — 관리실 형광등과
    # 손전등만이 이 글자를 읽게 해준다.
    "M_SignAux5MonitorOnly": {
        "tex_asset": "T_SignAux5MonitorOnly_D", "rough": 0.74,
    },
    "M_SignToilet":    {"tex_asset": "T_SignToilet_D", "rough": 0.4},
    "M_SignAutoDoor":  {"tex_asset": "T_SignAutoDoor_D", "rough": 0.3, "emissive_scale": 0.15},
    "M_PriceStrip":    {"tex_asset": "T_PriceStrip_D", "rough": 0.4, "emissive_scale": 0.03,
                        "tile_u": 2.0},
    "M_SignVilla":     {"tex_asset": "T_SignVilla_D", "rough": 0.4, "emissive_scale": 0.25},
    "M_Plate401":      {"tex_asset": "T_Plate401_D", "rough": 0.35},
    "M_Plate402":      {"tex_asset": "T_Plate402_D", "rough": 0.35},
    "M_Plate403":      {"tex_asset": "T_Plate403_D", "rough": 0.35},
    "M_Plate404":      {"tex_asset": "T_Plate404_D", "rough": 0.35},
    "M_PlateCommon":   {"tex_asset": "T_PlateCommon_D", "rough": 0.35},
    "M_ElevatorPanel": {"tex_asset": "T_ElevatorPanel_D", "rough": 0.3, "emissive_scale": 0.8},
    "M_ClockFace":     {"tex_asset": "T_ClockFace_D", "rough": 0.25, "emissive_scale": 1.6},
    "M_SignLaundry":   {"tex_asset": "T_SignLaundry_D", "rough": 0.45, "emissive_scale": 0.05},
    "M_SignHair":      {"tex_asset": "T_SignHair_D", "rough": 0.45, "emissive_scale": 0.05},
    "M_SignHof":       {"tex_asset": "T_SignHof_D", "rough": 0.45, "emissive_scale": 0.5},
    "M_SignSuper":     {"tex_asset": "T_SignSuper_D", "rough": 0.45, "emissive_scale": 0.05},
    "M_Banner":        {"tex_asset": "T_Banner_D", "rough": 0.7, "flutter": True},
    "M_NoticeA4":      {"tex_asset": "T_NoticeA4_D", "rough": 0.7},
    # Aged paper stock for readable notes. The Korean copy is drawn over these
    # at runtime by the HUD, so the sheets themselves carry no text — only
    # creases, tape, water damage and age.
    # V2 preserves the faulty first paper sheet for provenance while replacing
    # the live materials with the verified, text-free ImageGen paper stock.
    "M_PaperClean":    {"tex_asset": "T_PaperClean_V2_D", "rough": 0.82},
    "M_PaperWet":      {"tex_asset": "T_PaperWet_V2_D", "rough": 0.62},
    "M_PaperFolded":   {"tex_asset": "T_PaperFolded_V2_D", "rough": 0.84},
    "M_PaperOld":      {"tex_asset": "T_PaperOld_V2_D", "rough": 0.86},
    "M_NoticeRent":    {"tex_asset": "T_NoticeRent_D", "rough": 0.72},
    "M_DoorAd":        {"tex_asset": "T_DoorAd_D", "rough": 0.6},
    "M_Calendar":      {"tex_asset": "T_Calendar_D", "rough": 0.7},
    "M_FireBox":       {"tex_asset": "T_FireBox_D", "rough": 0.4, "emissive_scale": 0.08},
    "M_TobaccoNotice": {"tex_asset": "T_TobaccoNotice_D", "rough": 0.5},
    "M_SignPC":        {"tex_asset": "T_SignPC_D", "rough": 0.45, "emissive_scale": 0.05},
    "M_SignKaraoke":   {"tex_asset": "T_SignKaraoke_D", "rough": 0.45, "emissive_scale": 0.45},
    # Villa fittings. The lift readouts are the only thing genuinely emitting
    # in the shaft, so they carry a strong emissive; the rest are plastic.
    "M_DoorLock":      {"tex_asset": "T_DoorLock_D", "rough": 0.34, "emissive_scale": 0.12},
    "M_MeterBox":      {"tex_asset": "T_MeterBox_D", "rough": 0.52},
    "M_Intercom":      {"tex_asset": "T_Intercom_D", "rough": 0.34, "emissive_scale": 0.08},
    "M_LiftCOP":       {"tex_asset": "T_LiftCOP_D", "rough": 0.26, "emissive_scale": 0.03},
    "M_LiftHall":      {"tex_asset": "T_LiftHall_D", "rough": 0.3, "emissive_scale": 1.4},
    "M_SwitchPlate":   {"tex_asset": "T_SwitchPlate_D", "rough": 0.4, "emissive_scale": 0.1},
    # Product labels: printed plastic film, so fairly smooth and unlit-free.
    "M_LabelWater":    {"tex_asset": "T_LabelWater_D", "rough": 0.28},
    "M_LabelGreenTea": {"tex_asset": "T_LabelGreenTea_D", "rough": 0.28},
    "M_LabelBarley":   {"tex_asset": "T_LabelBarley_D", "rough": 0.28},
    "M_LabelSoda":     {"tex_asset": "T_LabelSoda_D", "rough": 0.28},
    "M_LabelSoju":     {"tex_asset": "T_LabelSoju_D", "rough": 0.32},
    "M_LabelRamyeon":  {"tex_asset": "T_LabelRamyeon_D", "rough": 0.42},
    "M_SnackShrimp":   {"tex_asset": "T_SnackShrimp_D", "rough": 0.22},
    "M_SnackPotato":   {"tex_asset": "T_SnackPotato_D", "rough": 0.22},
    "M_SnackSquid":    {"tex_asset": "T_SnackSquid_D", "rough": 0.22},
    "M_SnackCorn":     {"tex_asset": "T_SnackCorn_D", "rough": 0.22},
    # ImageGen scans are BaseColor inputs on authored geometry, not finished
    # materials. Companion N/R/A/W/M maps make them respond to flashlight,
    # Lumen reflections and contact shadowing without baking light into colour.
    "M_WetHoodieUV": {
        "tex_asset": "T_WetHoodie_D", "pbr_stem": "T_WetHoodie",
        "wet_rough": 0.27, "wet_dark": 0.72, "wet_normal_flatten": 0.45,
        "specular": 0.50,
    },
    "M_SubmergedHoodieUV": {
        "tex_asset": "T_WetHoodie_D", "pbr_stem": "T_WetHoodie",
        "tile_u": 1.7, "wet_rough": 0.38, "wet_dark": 0.80,
        "wet_normal_flatten": 0.28, "minimum_wetness": 0.84,
        "specular": 0.28,
    },
    "M_AlleyCatTabbyUV": {
        "tex_asset": "T_AlleyCatTabby_D", "pbr_stem": "T_AlleyCatTabby",
        "specular": 0.32,
    },
    "M_WaterTankMetalUV": {
        "tex_asset": "T_WaterTankGalvanized_D", "pbr_stem": "T_WaterTankGalvanized",
        "tile_u": 2.0, "metal_map": True, "wet_rough": 0.19,
        "wet_dark": 0.76, "wet_normal_flatten": 0.28, "specular": 0.50,
    },
    "M_TankInteriorBiofilmUV": {
        "tex_asset": "T_TankInteriorBiofilm_D", "pbr_stem": "T_TankInteriorBiofilm",
        "tile_u": 3.0, "metal_map": True, "wet_rough": 0.18,
        "wet_dark": 0.70, "wet_normal_flatten": 0.32, "specular": 0.48,
        "wet_waterline_z": 561.0, "wet_transition_cm": 8.0,
    },
    "M_WetServiceHoseUV": {
        "tex_asset": "T_WetServiceHose_D", "pbr_stem": "T_WetServiceHose",
        "tile_u": 6.0, "wet_rough": 0.22, "wet_dark": 0.68,
        "wet_normal_flatten": 0.52, "specular": 0.55,
    },
    "M_WetRungPadUV": {
        "tex_asset": "T_WetRungPad_D", "pbr_stem": "T_WetRungPad",
        "tile_u": 2.0, "wet_rough": 0.24, "wet_dark": 0.72,
        "wet_normal_flatten": 0.45, "specular": 0.52,
    },
    "M_P3CabinetMetalUV": {
        "tex_asset": "T_P3CabinetPaintedSteel_D", "pbr_stem": "T_P3CabinetPaintedSteel",
        "tile_u": 2.2, "wet_rough": 0.38, "wet_dark": 0.84,
        "wet_normal_flatten": 0.22, "specular": 0.50,
    },
    "M_SubmergedPantsUV": {
        "tex_asset": "T_WetHoodie_D", "pbr_stem": "T_WetHoodie",
        "tile_u": 2.5, "wet_rough": 0.36, "wet_dark": 0.70,
        "wet_normal_flatten": 0.34, "minimum_wetness": 0.86,
        "specular": 0.26,
    },
    "M_SubmergedSlippersUV": {
        "tex_asset": "T_WetServiceHose_D", "pbr_stem": "T_WetServiceHose",
        "tile_u": 1.35, "wet_rough": 0.42, "wet_dark": 0.20,
        "base_lift": (0.012, 0.016, 0.022),
        "wet_normal_flatten": 0.42, "minimum_wetness": 0.90,
        "specular": 0.22,
    },
    "M_SubmergedSlipperWearUV": {
        "tex_asset": "T_WetRungPad_D", "pbr_stem": "T_WetRungPad",
        "tile_u": 1.0, "wet_rough": 0.36, "wet_dark": 0.34,
        "wet_normal_flatten": 0.36, "minimum_wetness": 0.82,
        "specular": 0.32,
    },
    "M_MissingFloorListenerPlasterUV": {
        "tex_asset": "T_MissingFloorDryPlaster_D",
        "pbr_stem": "T_MissingFloorDryPlaster", "tile_u": 2.8,
        "specular": 0.16,
    },
}

# ImageGen source is split by Prepare-AIArt.ps1. Evidence sheets remain
# grayscale value masks so one texture controls the irregular wet edge; the
# surface overlays carry authored colour plus a keyed alpha channel.
EVIDENCE_MASK_MATERIALS = {
    "M_ApartmentWallPatina": {
        "tex_asset": "T_ApartmentWallPatina_M", "rough": 0.91,
        "color": (0.065, 0.052, 0.034), "mask_gain": 2.1,
        "specular": 0.16,
    },
    "M_EvidenceSlipperTrail": {
        "tex_asset": "T_EvidenceSlipperTrail_M", "rough": 0.10,
        "color": (0.025, 0.034, 0.038), "mask_gain": 4.0,
    },
    "M_EvidenceCatPawTrail": {
        "tex_asset": "T_EvidenceCatPawTrail_M", "rough": 0.08,
        "color": (0.023, 0.032, 0.036), "mask_gain": 4.4,
    },
    "M_EvidenceHoseDrag": {
        "tex_asset": "T_EvidenceHoseDrag_M", "rough": 0.09,
        "color": (0.026, 0.035, 0.039), "mask_gain": 4.2,
    },
    "M_EvidenceHandSmear": {
        "tex_asset": "T_EvidenceHandSmear_M", "rough": 0.07,
        "color": (0.021, 0.030, 0.034), "mask_gain": 4.0,
    },
    # 5층의 분진 잔흔 네 장. 어두운 콘크리트 바닥 위의 석고 분진은 **살짝**
    # 밝은 얼룩이지 흰 자국이 아니다. 원래 값(0.48~0.68 알베도, 증폭 1.8~2.5)은
    # 바닥의 두 배 밝기에 이진 실루엣이라, CCTV 프레임에서 바닥 위에 떠 있는
    # 도장 자국처럼 읽혔다. 증폭을 낮춰 실루엣을 짙은 심지에만 남기고, 얇은
    # 자리는 substrate 쪽으로 보내 사라지게 한다.
    #
    # substrate는 그 잔흔이 실제로 얹히는 면의 알베도이고, **계산이 아니라
    # 측정**으로 얻는다. 확산맵 평균과 틴트를 곱해 추정했더니 바닥이 0.058로
    # 나왔는데 실측은 0.124였다 — 두 배 이상 어긋났고, 그 값으로는 잔흔의 얇은
    # 자리가 바닥보다 어두워져 석고 분진이 흙때로 읽혔다.
    #
    # 재는 방법: 잔흔 하나를 균일 알베도(0.30)로 굽고 근접 시점 프레임에서
    # 잔흔이 덮은 픽셀과 바로 인접한 바닥의 중앙값 비를 낸다. 비가 r이면
    # 그 면의 실효 알베도는 0.30 / r이다. 바닥은 r=2.43 → 0.124, 벽은
    # 손자국 심지(0.23)가 깨끗한 석고의 0.47배로 나와 0.487이었다.
    "M_MissingFloorHandprints": {
        "tex_asset": "T_MissingFloorHandprints_M", "rough": 0.94,
        "color": (0.23, 0.22, 0.205), "mask_gain": 1.25, "specular": 0.08,
        "substrate": (0.487, 0.474, 0.443),
    },
    # 0.24는 실측 바닥(0.124)의 정확히 두 배다. 그 전의 0.20과 0.22는 둘 다
    # 바닥보다 어두운 값이었고, 그래서 「한 단계 올려도」 프레임이 달라지지
    # 않았다 — 어느 쪽이든 분진이 아니라 때였다. 증폭 1.15는 건드리지 않는다:
    # 실루엣을 짙은 심지에 묶어 두는 그 값이 도장 자국으로 되돌아가지 않게
    # 하는 장치다.
    "M_MissingFloorDragTrails": {
        "tex_asset": "T_MissingFloorDragTrails_M", "rough": 0.96,
        "color": (0.24, 0.23, 0.21), "mask_gain": 1.15, "specular": 0.06,
        "substrate": (0.124, 0.126, 0.132),
    },
    "M_MissingFloorDustJoint": {
        "tex_asset": "T_MissingFloorDustJoint_M", "rough": 0.98,
        "color": (0.26, 0.25, 0.23), "mask_gain": 1.10, "specular": 0.04,
        "substrate": (0.124, 0.126, 0.132),
    },
    # 긁힌 자국은 분진이 아니라 파인 자리다 — 석고보다 어두운 색이 맞고,
    # 얇은 자리는 벽으로 사라져야 한다.
    "M_MissingFloorCavityScratches": {
        "tex_asset": "T_MissingFloorCavityScratches_M", "rough": 0.92,
        "color": (0.34, 0.32, 0.29), "mask_gain": 1.9, "specular": 0.08,
        "substrate": (0.487, 0.474, 0.443),
    },
}

SURFACE_OVERLAY_MATERIALS = {
    "M_DecalDampWallpaper": {
        "tex_asset": "T_DecalDampWallpaper_D", "rough": 0.78,
    },
    "M_DecalRustFasteners": {
        "tex_asset": "T_DecalRustFasteners_D", "rough": 0.66,
    },
    "M_DecalMineralScale": {
        "tex_asset": "T_DecalMineralScale_D", "rough": 0.84,
    },
    "M_DecalRainGrime": {
        "tex_asset": "T_DecalRainGrime_D", "rough": 0.80,
    },
    # Each person card is fixed to an authored viewing cue, receives real
    # scene light, and stays masked/opaque so hair edges cannot sort like a
    # translucent card. The listener front layer also carries conservative
    # N/R/A maps and is paired with a continuous contact-shadow shell.
    "M_SpriteSeo": {"tex_asset": "T_SpriteSeo_D", "rough": 0.82},
    "M_SpriteMok": {"tex_asset": "T_SpriteMok_D", "rough": 0.86},
    "M_SpriteHwang": {"tex_asset": "T_SpriteHwang_D", "rough": 0.88},
    "M_SpriteNarin": {"tex_asset": "T_SpriteNarin_D", "rough": 0.80},
    "M_SpriteListenerFront": {
        "tex_asset": "T_SpriteListenerFront_D",
        "pbr_stem": "T_SpriteListenerFront",
        "rough": 0.86,
        "specular": 0.14,
    },
    "M_SpriteListenerCrawl0": {
        "tex_asset": "T_SpriteListenerCrawl0_D",
        "pbr_stem": "T_SpriteListenerCrawl0",
        "rough": 0.86,
        "specular": 0.14,
    },
    "M_SpriteListenerCrawl1": {
        "tex_asset": "T_SpriteListenerCrawl1_D",
        "pbr_stem": "T_SpriteListenerCrawl1",
        "rough": 0.86,
        "specular": 0.14,
    },
    "M_SpriteListenerCrawl2": {
        "tex_asset": "T_SpriteListenerCrawl2_D",
        "pbr_stem": "T_SpriteListenerCrawl2",
        "rough": 0.86,
        "specular": 0.14,
    },
    "M_SpriteListenerCrawl3": {
        "tex_asset": "T_SpriteListenerCrawl3_D",
        "pbr_stem": "T_SpriteListenerCrawl3",
        "rough": 0.86,
        "specular": 0.14,
    },
    "M_SpriteFinalCavity": {
        "tex_asset": "T_SpriteFinalCavity_D",
        "pbr_stem": "T_SpriteFinalCavity",
        "rough": 0.90,
        "specular": 0.10,
    },
    "M_SpriteMokFinalUpper": {
        "tex_asset": "T_SpriteMokFinalUpper_D",
        "pbr_stem": "T_SpriteMokFinalUpper",
        "rough": 0.84,
        "specular": 0.14,
    },
}

# Emissive signage: the texture *is* the light source.
SIGN_MATERIALS = {
    # At the old 2.2/1.8 multipliers the pale lettering clipped to cyan-white
    # before the camera exposed the alley, erasing the Korean store identity.
    # These remain visibly self-lit while preserving the print and mint band.
    "M_SignMainLit":  {"tex_asset": "T_SignMain_D", "emissive_scale": 0.85},
    "M_SignBladeLit": {"tex_asset": "T_SignBlade_D", "emissive_scale": 0.65},
}

# These materials are bound to the batched convenience-store stock. Unreal
# does not compile the instanced-static-mesh shader permutation implicitly for
# generated assets; without the persisted usage flag the editor substitutes
# its grey default material at runtime even though the texture graph is valid.
INSTANCED_PRODUCT_MATERIALS = {
    "M_BottleBrown",
    "M_BottleGreen",
    "M_CupNoodle",
    "M_FridgeInterior",
    "M_LabelBarley",
    "M_LabelGreenTea",
    "M_LabelRamyeon",
    "M_LabelSoda",
    "M_LabelSoju",
    "M_LabelWater",
    "M_SnackBlue",
    "M_SnackCorn",
    "M_SnackPotato",
    "M_SnackRed",
    "M_SnackShrimp",
    "M_SnackSquid",
    "M_SnackYellow",
    "M_StainlessUV",
}

WRAPPED_LABEL_MATERIALS = {
    "M_LabelBarley",
    "M_LabelGreenTea",
    "M_LabelRamyeon",
    "M_LabelSoda",
    "M_LabelSoju",
    "M_LabelWater",
}


def _expr(material, expression_class, x=-600, y=0):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y
    )
    if expression is None:
        raise RuntimeError(
            f"Could not create {expression_class} on {material.get_path_name()}"
        )
    return expression


def _load_texture(name):
    """Prefers the CC0 photo capture (T_Photo_*) over the procedural fallback."""
    if name.startswith("T_") and not name.startswith("T_Photo_"):
        photo = unreal.load_asset(f"{TEXTURE_ROOT}/T_Photo_{name[2:]}")
        if photo is not None:
            return photo
    texture = unreal.load_asset(f"{TEXTURE_ROOT}/{name}")
    if texture is None:
        raise RuntimeError(f"Missing texture asset: {TEXTURE_ROOT}/{name}")
    return texture


def _make_uv_source(material, mapping, tile, y_offset):
    """Returns an expression producing 2D UVs for the requested mapping."""
    if mapping == "UV":
        coords = _expr(material, unreal.MaterialExpressionTextureCoordinate, -1100, y_offset)
        coords.set_editor_property("u_tiling", tile)
        coords.set_editor_property("v_tiling", tile)
        return coords

    world_position = _expr(
        material, unreal.MaterialExpressionWorldPosition, -1300, y_offset
    )
    mask = _expr(material, unreal.MaterialExpressionComponentMask, -1100, y_offset)
    mask.set_editor_property("r", mapping[0] == "X")
    mask.set_editor_property("g", mapping in ("XY", "YZ"))
    mask.set_editor_property("b", mapping in ("XZ", "YZ"))
    unreal.MaterialEditingLibrary.connect_material_expressions(
        world_position, "", mask, ""
    )

    scale = _expr(material, unreal.MaterialExpressionConstant, -1100, y_offset + 150)
    scale.set_editor_property("r", 1.0 / tile)
    multiply = _expr(material, unreal.MaterialExpressionMultiply, -900, y_offset)
    unreal.MaterialEditingLibrary.connect_material_expressions(mask, "", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(scale, "", multiply, "B")
    return multiply


def _make_panning_uv(material, tiling, speed_x, speed_y, y_offset):
    """Build a restrained time-driven UV layer for water micro-motion."""
    coords = _expr(
        material,
        unreal.MaterialExpressionTextureCoordinate,
        -1500,
        y_offset,
    )
    coords.set_editor_property("u_tiling", tiling)
    coords.set_editor_property("v_tiling", tiling)
    panner = _expr(material, unreal.MaterialExpressionPanner, -1300, y_offset)
    panner.set_editor_property("speed_x", speed_x)
    panner.set_editor_property("speed_y", speed_y)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        coords, "", panner, "Coordinate"
    )
    return panner


def _sample(material, texture, uv_expression, sampler_type, y_offset):
    sample = _expr(material, unreal.MaterialExpressionTextureSample, -650, y_offset)
    sample.set_editor_property("texture", texture)
    sample.set_editor_property("sampler_type", sampler_type)
    if uv_expression is not None:
        unreal.MaterialEditingLibrary.connect_material_expressions(
            uv_expression, "", sample, "UVs"
        )
    return sample


def _recreate_material(assets, tools, name):
    asset_path = f"{MATERIAL_ROOT}/{name}"
    if assets.does_asset_exist(asset_path) and not assets.delete_asset(asset_path):
        raise RuntimeError(f"Could not replace material: {asset_path}")
    material = tools.create_asset(
        name, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
    )
    if material is None:
        raise RuntimeError(f"Could not create material: {asset_path}")
    return material


def create_textured_materials(assets, tools, specs=None):
    created = []
    for name, spec in (specs or TEXTURED_MATERIALS).items():
        material = _recreate_material(assets, tools, name)
        base_name = spec["tex"]
        mapping = spec["mapping"]
        tile = spec["tile"]

        uv_color = _make_uv_source(material, mapping, tile, 0)
        diffuse = _sample(
            material,
            _load_texture(f"T_{base_name}_D"),
            uv_color,
            unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
            0,
        )

        # Colour chain: sample -> optional desaturation -> optional tint.
        # Desaturation is what turns a confetti terrazzo scan into the fine
        # grey speckle of Korean 화강석; a tint alone cannot remove chroma.
        color_source = diffuse
        color_pin = "RGB"
        desaturate = spec.get("desaturate")
        if desaturate is not None:
            fraction = _expr(material, unreal.MaterialExpressionConstant, -650, 300)
            fraction.set_editor_property("r", desaturate)
            grey = _expr(material, unreal.MaterialExpressionDesaturation, -450, 140)
            unreal.MaterialEditingLibrary.connect_material_expressions(
                color_source, color_pin, grey, ""
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                fraction, "", grey, "Fraction"
            )
            color_source = grey
            color_pin = ""

        tint = spec.get("tint")
        if tint:
            tint_constant = _expr(material, unreal.MaterialExpressionConstant3Vector, -650, 220)
            tint_constant.set_editor_property(
                "constant", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0)
            )
            tinted = _expr(material, unreal.MaterialExpressionMultiply, -400, 60)
            unreal.MaterialEditingLibrary.connect_material_expressions(
                color_source, color_pin, tinted, "A"
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                tint_constant, "", tinted, "B"
            )
            unreal.MaterialEditingLibrary.connect_material_property(
                tinted, "", unreal.MaterialProperty.MP_BASE_COLOR
            )
        else:
            unreal.MaterialEditingLibrary.connect_material_property(
                color_source, color_pin, unreal.MaterialProperty.MP_BASE_COLOR
            )

        normal = _sample(
            material,
            _load_texture(f"T_{base_name}_N"),
            _make_uv_source(material, mapping, tile, 420),
            unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
            420,
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            normal, "RGB", unreal.MaterialProperty.MP_NORMAL
        )

        rough_asset = f"T_{base_name}_R"
        forced_rough = spec.get("force_rough")
        if forced_rough is not None:
            rough_constant = _expr(material, unreal.MaterialExpressionConstant, -650, 880)
            rough_constant.set_editor_property("r", forced_rough)
            unreal.MaterialEditingLibrary.connect_material_property(
                rough_constant, "", unreal.MaterialProperty.MP_ROUGHNESS
            )
        elif assets.does_asset_exist(f"{TEXTURE_ROOT}/{rough_asset}"):
            rough_sample = _sample(
                material,
                _load_texture(rough_asset),
                _make_uv_source(material, mapping, tile, 840),
                # Roughness imports use TC_GRAYSCALE.  Sampling them as
                # Linear Color makes the entire material fail compilation
                # and Unreal falls back to the grey checkerboard material.
                unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
                840,
            )
            unreal.MaterialEditingLibrary.connect_material_property(
                rough_sample, "R", unreal.MaterialProperty.MP_ROUGHNESS
            )
        else:
            rough_constant = _expr(material, unreal.MaterialExpressionConstant, -650, 880)
            rough_constant.set_editor_property("r", spec.get("rough", 0.8))
            unreal.MaterialEditingLibrary.connect_material_property(
                rough_constant, "", unreal.MaterialProperty.MP_ROUGHNESS
            )

        if spec.get("ao"):
            ao_asset = f"T_{base_name}_A"
            if assets.does_asset_exist(f"{TEXTURE_ROOT}/{ao_asset}"):
                ao_sample = _sample(
                    material,
                    _load_texture(ao_asset),
                    _make_uv_source(material, mapping, tile, 1120),
                    unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
                    1120,
                )
                unreal.MaterialEditingLibrary.connect_material_property(
                    ao_sample, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION
                )

        metallic = spec.get("metallic")
        if metallic is not None:
            metallic_constant = _expr(material, unreal.MaterialExpressionConstant, -650, 1020)
            metallic_constant.set_editor_property("r", metallic)
            unreal.MaterialEditingLibrary.connect_material_property(
                metallic_constant, "", unreal.MaterialProperty.MP_METALLIC
            )

        unreal.MaterialEditingLibrary.layout_material_expressions(material)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.log(f"[IndieGame] Created textured material: {name}")
        created.append(material)
    return created


def create_flat_texture_materials(
    assets, tools, specs, emissive_only, update_in_place=False
):
    created = []
    skipped = []
    for name, spec in specs.items():
        # Artwork arrives in batches — a generated sheet may not have landed
        # yet. Skipping the material is right: the C++ side already falls back
        # to a flat colour for anything it cannot load, so a half-finished
        # asset run still produces a playable build.
        source_asset = spec["tex_asset"]
        if not (
            assets.does_asset_exist(f"{TEXTURE_ROOT}/{source_asset}")
            or assets.does_asset_exist(f"{TEXTURE_ROOT}/T_Photo_{source_asset[2:]}")
        ):
            skipped.append(name)
            continue

        asset_path = f"{MATERIAL_ROOT}/{name}"
        if update_in_place and assets.does_asset_exist(asset_path):
            # Sign materials are loaded by the prologue scene CDO while this
            # commandlet is running, so deleting their packages fails with a
            # sharing violation. Rebuilding the graph in place keeps those
            # live references valid and still saves the corrected asset.
            material = unreal.load_asset(asset_path)
            if material is None:
                raise RuntimeError(f"Could not load material: {asset_path}")
            unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
        else:
            material = _recreate_material(assets, tools, name)
        material.set_editor_property("two_sided", bool(spec.get("two_sided", False)))
        texture = _load_texture(source_asset)

        uv = None
        tile_u = spec.get("tile_u")
        if tile_u:
            uv = _expr(material, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
            uv.set_editor_property("u_tiling", tile_u)
            uv.set_editor_property("v_tiling", 1.0)

        sample = _sample(
            material, texture, uv, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 0
        )

        # Paper flutter in the pre-dawn wind via world position offset.
        if spec.get("flutter"):
            time_expr = _expr(material, unreal.MaterialExpressionTime, -1300, 700)
            time_scale = _expr(material, unreal.MaterialExpressionConstant, -1300, 840)
            time_scale.set_editor_property("r", 0.42)
            phase = _expr(material, unreal.MaterialExpressionMultiply, -1100, 720)
            unreal.MaterialEditingLibrary.connect_material_expressions(time_expr, "", phase, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(time_scale, "", phase, "B")
            wobble = _expr(material, unreal.MaterialExpressionSine, -950, 720)
            unreal.MaterialEditingLibrary.connect_material_expressions(phase, "", wobble, "")
            amplitude = _expr(material, unreal.MaterialExpressionConstant, -950, 860)
            amplitude.set_editor_property("r", 0.9)
            offset_y = _expr(material, unreal.MaterialExpressionMultiply, -780, 740)
            unreal.MaterialEditingLibrary.connect_material_expressions(wobble, "", offset_y, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(amplitude, "", offset_y, "B")
            zero_a = _expr(material, unreal.MaterialExpressionConstant, -780, 880)
            zero_a.set_editor_property("r", 0.0)
            xy = _expr(material, unreal.MaterialExpressionAppendVector, -620, 760)
            unreal.MaterialEditingLibrary.connect_material_expressions(zero_a, "", xy, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(offset_y, "", xy, "B")
            zero_b = _expr(material, unreal.MaterialExpressionConstant, -620, 900)
            zero_b.set_editor_property("r", 0.0)
            xyz = _expr(material, unreal.MaterialExpressionAppendVector, -470, 780)
            unreal.MaterialEditingLibrary.connect_material_expressions(xy, "", xyz, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(zero_b, "", xyz, "B")
            unreal.MaterialEditingLibrary.connect_material_property(
                xyz, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET
            )

        emissive_scale = spec.get("emissive_scale", 0.0)
        if emissive_only:
            dark = _expr(material, unreal.MaterialExpressionConstant3Vector, -650, 300)
            dark.set_editor_property("constant", unreal.LinearColor(0.02, 0.02, 0.02, 1.0))
            unreal.MaterialEditingLibrary.connect_material_property(
                dark, "", unreal.MaterialProperty.MP_BASE_COLOR
            )
        else:
            if spec.get("pbr_stem"):
                _connect_scan_pbr(material, sample, uv, spec)
            else:
                unreal.MaterialEditingLibrary.connect_material_property(
                    sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
                )
                rough_constant = _expr(material, unreal.MaterialExpressionConstant, -650, 340)
                rough_constant.set_editor_property("r", spec.get("rough", 0.6))
                unreal.MaterialEditingLibrary.connect_material_property(
                    rough_constant, "", unreal.MaterialProperty.MP_ROUGHNESS
                )

        if emissive_scale > 0.0:
            scale_constant = _expr(material, unreal.MaterialExpressionConstant, -650, 500)
            scale_constant.set_editor_property("r", emissive_scale)
            emissive = _expr(material, unreal.MaterialExpressionMultiply, -400, 460)
            unreal.MaterialEditingLibrary.connect_material_expressions(
                sample, "RGB", emissive, "A"
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                scale_constant, "", emissive, "B"
            )
            unreal.MaterialEditingLibrary.connect_material_property(
                emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
            )

        unreal.MaterialEditingLibrary.layout_material_expressions(material)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.log(f"[IndieGame] Created sign material: {name}")
        created.append(material)

    if skipped:
        unreal.log_warning(
            f"[IndieGame] Skipped {len(skipped)} material(s) with no artwork yet: "
            + ", ".join(skipped)
        )
    return created


def create_masked_texture_materials(assets, tools, specs, mask_only):
    """Builds two-sided plane overlays without translucent sorting.

    Evidence masks use R as opacity and a physically wet constant surface.
    Chroma-keyed environmental overlays use RGB for colour and A for opacity.
    The planes sit a few millimetres above authored surfaces, so masked mode
    avoids the halo/sort failures that are especially visible in flashlight
    sweeps.
    """
    created = []
    for name, spec in specs.items():
        source_asset = spec["tex_asset"]
        if not assets.does_asset_exist(f"{TEXTURE_ROOT}/{source_asset}"):
            unreal.log_warning(
                f"[IndieGame] Skipped {name}: missing {source_asset}"
            )
            continue

        material = _recreate_material(assets, tools, name)
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        material.set_editor_property("two_sided", True)
        material.set_editor_property("opacity_mask_clip_value", 0.08)
        texture = _load_texture(source_asset)
        sample = _sample(
            material, texture, None,
            (unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
             if mask_only else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
            0,
        )

        if mask_only:
            gain = _expr(material, unreal.MaterialExpressionConstant, -760, 180)
            gain.set_editor_property("r", spec.get("mask_gain", 4.0))
            amplified = _expr(material, unreal.MaterialExpressionMultiply, -560, 100)
            unreal.MaterialEditingLibrary.connect_material_expressions(
                sample, "R", amplified, "A"
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                gain, "", amplified, "B"
            )
            opacity = _expr(material, unreal.MaterialExpressionSaturate, -380, 100)
            unreal.MaterialEditingLibrary.connect_material_expressions(
                amplified, "", opacity, ""
            )
            unreal.MaterialEditingLibrary.connect_material_property(
                opacity, "", unreal.MaterialProperty.MP_OPACITY_MASK
            )
            color = spec.get("color", (0.025, 0.034, 0.038))
            base = _expr(material, unreal.MaterialExpressionConstant3Vector, -380, -80)
            base.set_editor_property(
                "constant", unreal.LinearColor(color[0], color[1], color[2], 1.0)
            )
            substrate = spec.get("substrate")
            if substrate is None:
                unreal.MaterialEditingLibrary.connect_material_property(
                    base, "", unreal.MaterialProperty.MP_BASE_COLOR
                )
            else:
                # 얇은 잔흔은 바닥으로 사라진다. 검정으로 사라지지 않는다.
                #
                # 앞선 판은 같은 마스크로 알베도를 곱해서 농도를 만들었다.
                # BLEND_MASKED에는 부분 투명이 없으니 얇은 자리를 표현할
                # 방법이 알베도뿐인데, 곱셈은 그것을 **검정 쪽으로** 끌어당겼다.
                # 프레임에서 잰 결과가 그대로 나왔다 — 잔흔이 덮은 픽셀의
                # 중앙값이 바로 인접한 바닥의 0.84배, 즉 석고 분진이 아니라
                # 흙때로 읽혔다.
                #
                # 물리적으로 얇은 가루층은 기질과 가루의 혼합이다. 그래서
                # 기질색에서 잔흔색으로 보간한다. 마스크가 옅은 자리는 기질과
                # 같아져 사라지고, 짙게 쌓인 심지만 밝아진다. 기질값은 그 면의
                # 실제 재질에서 온다 — 별관 바닥은 콘크리트 다크(확산맵 선형
                # 0.180 × 틴트 0.32), 베이 벽은 마른 석고(0.486 × 0.78).
                ground = _expr(
                    material, unreal.MaterialExpressionConstant3Vector, -760, 320
                )
                ground.set_editor_property(
                    "constant",
                    unreal.LinearColor(
                        substrate[0], substrate[1], substrate[2], 1.0
                    ),
                )
                shade = _expr(
                    material,
                    unreal.MaterialExpressionLinearInterpolate,
                    -560,
                    300,
                )
                unreal.MaterialEditingLibrary.connect_material_expressions(
                    ground, "", shade, "A"
                )
                unreal.MaterialEditingLibrary.connect_material_expressions(
                    base, "", shade, "B"
                )
                # 불투명도와 같은 증폭값을 쓴다. 실루엣과 농도가 어긋나면
                # 테두리에 밝은 띠가 생긴다.
                unreal.MaterialEditingLibrary.connect_material_expressions(
                    opacity, "", shade, "Alpha"
                )
                unreal.MaterialEditingLibrary.connect_material_property(
                    shade, "", unreal.MaterialProperty.MP_BASE_COLOR
                )
            specular = _expr(material, unreal.MaterialExpressionConstant, -380, 360)
            specular.set_editor_property("r", spec.get("specular", 0.62))
            unreal.MaterialEditingLibrary.connect_material_property(
                specular, "", unreal.MaterialProperty.MP_SPECULAR
            )
        elif spec.get("pbr_stem"):
            _connect_scan_pbr(material, sample, None, spec)
            unreal.MaterialEditingLibrary.connect_material_property(
                sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK
            )
        else:
            unreal.MaterialEditingLibrary.connect_material_property(
                sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
            )
            unreal.MaterialEditingLibrary.connect_material_property(
                sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK
            )

        if not spec.get("pbr_stem"):
            roughness = _expr(material, unreal.MaterialExpressionConstant, -180, 300)
            roughness.set_editor_property("r", spec.get("rough", 0.75))
            unreal.MaterialEditingLibrary.connect_material_property(
                roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
            )
        unreal.MaterialEditingLibrary.layout_material_expressions(material)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.log(f"[IndieGame] Created masked overlay material: {name}")
        created.append(material)
    return created


def create_carrier_bag_material(assets, tools):
    """Thin printed LDPE without an opaque glass-box silhouette.

    The texture supplies wrinkles and fictional pale-blue print. Opacity stays
    in a narrow physical range, so the selected bottle count remains visible
    in every purchase profile and at the CH03 accident reconstruction.
    """
    source_asset = "T_CarrierBagFilm_D"
    if not assets.does_asset_exist(f"{TEXTURE_ROOT}/{source_asset}"):
        unreal.log_warning(
            f"[IndieGame] Skipped M_CarrierBagFilm: missing {source_asset}"
        )
        return None

    material = _recreate_material(assets, tools, "M_CarrierBagFilm")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    sample = _sample(
        material,
        _load_texture(source_asset),
        None,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
        0,
    )
    _connect_scan_pbr(
        material,
        sample,
        None,
        {
            "pbr_stem": "T_CarrierBagFilm",
            "specular": 0.52,
        },
    )

    opacity_scale = _expr(material, unreal.MaterialExpressionConstant, -620, 220)
    opacity_scale.set_editor_property("r", 0.24)
    opacity_detail = _expr(material, unreal.MaterialExpressionMultiply, -420, 160)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        sample, "R", opacity_detail, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        opacity_scale, "", opacity_detail, "B"
    )
    opacity_floor = _expr(material, unreal.MaterialExpressionConstant, -420, 300)
    opacity_floor.set_editor_property("r", 0.11)
    opacity = _expr(material, unreal.MaterialExpressionAdd, -220, 210)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        opacity_detail, "", opacity, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        opacity_floor, "", opacity, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.log("[IndieGame] Created translucent carrier bag: M_CarrierBagFilm")
    return material


def enable_instanced_product_usage(created):
    """Persist the shader permutation required by batched retail props."""
    by_name = {material.get_name(): material for material in created}
    for name in sorted(INSTANCED_PRODUCT_MATERIALS):
        material = by_name.get(name)
        if material is None:
            material = unreal.load_asset(f"{MATERIAL_ROOT}/{name}")
        if material is None:
            raise RuntimeError(f"Missing instanced product material: {name}")
        material.set_editor_property("used_with_instanced_static_meshes", True)
        if name in WRAPPED_LABEL_MATERIALS:
            # A closed film sleeve has no meaningful exposed back, but making
            # these tiny surfaces two-sided prevents a platform winding-rule
            # difference from turning the wrap invisible. The affected pixel
            # area is negligible compared with the cooler glass behind it.
            material.set_editor_property("two_sided", True)
        unreal.MaterialEditingLibrary.recompile_material(material)
        if material not in created:
            created.append(material)
    return created


def _connect_scan_pbr(material, base_sample, uv, spec):
    """Connect a generated scan as a complete, flashlight-reactive PBR surface."""
    stem = spec["pbr_stem"]
    wet_sample = None
    wet_output = None
    wet_output_pin = "R"
    if "wet_rough" in spec:
        wet_sample = _sample(
            material,
            _load_texture(f"{stem}_W"),
            uv,
            unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
            620,
        )
        wet_output = wet_sample
        if "minimum_wetness" in spec:
            minimum_wetness = _expr(
                material, unreal.MaterialExpressionConstant, -650, 760
            )
            minimum_wetness.set_editor_property(
                "r", spec["minimum_wetness"]
            )
            minimum_blend = _expr(
                material, unreal.MaterialExpressionMax, -470, 720
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                wet_sample, "R", minimum_blend, "A"
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                minimum_wetness, "", minimum_blend, "B"
            )
            wet_output = minimum_blend
            wet_output_pin = ""
        if "wet_waterline_z" in spec:
            transition_width = max(1.0, spec.get("wet_transition_cm", 8.0))
            world_position = _expr(
                material, unreal.MaterialExpressionWorldPosition, -1250, 780
            )
            world_height = _expr(
                material, unreal.MaterialExpressionComponentMask, -1070, 780
            )
            world_height.set_editor_property("r", False)
            world_height.set_editor_property("g", False)
            world_height.set_editor_property("b", True)
            unreal.MaterialEditingLibrary.connect_material_expressions(
                world_position, "", world_height, ""
            )
            transition_top = _expr(
                material, unreal.MaterialExpressionConstant, -1070, 900
            )
            transition_top.set_editor_property(
                "r", spec["wet_waterline_z"] + transition_width * 0.5
            )
            height_below_top = _expr(
                material, unreal.MaterialExpressionSubtract, -890, 820
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                transition_top, "", height_below_top, "A"
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                world_height, "", height_below_top, "B"
            )
            inverse_width = _expr(
                material, unreal.MaterialExpressionConstant, -890, 940
            )
            inverse_width.set_editor_property("r", 1.0 / transition_width)
            normalized_depth = _expr(
                material, unreal.MaterialExpressionMultiply, -710, 840
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                height_below_top, "", normalized_depth, "A"
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                inverse_width, "", normalized_depth, "B"
            )
            submerged_mask = _expr(
                material, unreal.MaterialExpressionSaturate, -530, 840
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                normalized_depth, "", submerged_mask, ""
            )
            local_wet_scale = _expr(
                material, unreal.MaterialExpressionConstant, -710, 1040
            )
            local_wet_scale.set_editor_property("r", 0.35)
            local_wetness = _expr(
                material, unreal.MaterialExpressionMultiply, -530, 1020
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                wet_output, wet_output_pin, local_wetness, "A"
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                local_wet_scale, "", local_wetness, "B"
            )
            submerged_scale = _expr(
                material, unreal.MaterialExpressionConstant, -530, 1120
            )
            submerged_scale.set_editor_property("r", 0.78)
            submerged_wetness = _expr(
                material, unreal.MaterialExpressionMultiply, -350, 900
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                submerged_mask, "", submerged_wetness, "A"
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                submerged_scale, "", submerged_wetness, "B"
            )
            combined_wetness = _expr(
                material, unreal.MaterialExpressionAdd, -170, 940
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                local_wetness, "", combined_wetness, "A"
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                submerged_wetness, "", combined_wetness, "B"
            )
            wet_output = _expr(
                material, unreal.MaterialExpressionSaturate, 10, 940
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                combined_wetness, "", wet_output, ""
            )
            wet_output_pin = ""
        wet_dark = _expr(material, unreal.MaterialExpressionConstant, -420, 700)
        wet_dark.set_editor_property("r", spec.get("wet_dark", 0.75))
        darkened = _expr(material, unreal.MaterialExpressionMultiply, -220, 80)
        unreal.MaterialEditingLibrary.connect_material_expressions(base_sample, "RGB", darkened, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(wet_dark, "", darkened, "B")
        wet_base = _expr(material, unreal.MaterialExpressionLinearInterpolate, 0, 20)
        unreal.MaterialEditingLibrary.connect_material_expressions(base_sample, "RGB", wet_base, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(darkened, "", wet_base, "B")
        unreal.MaterialEditingLibrary.connect_material_expressions(
            wet_output, wet_output_pin, wet_base, "Alpha"
        )
        base_output = wet_base
        if "base_lift" in spec:
            # Black rubber still needs a small diffuse floor underwater; pure
            # texture black loses the entire sole while pale identity stripes
            # remain, creating the illusion of three floating bars.
            lift = spec["base_lift"]
            lift_constant = _expr(
                material, unreal.MaterialExpressionConstant3Vector, 180, 0
            )
            lift_constant.set_editor_property(
                "constant", unreal.LinearColor(lift[0], lift[1], lift[2], 1.0)
            )
            lifted_base = _expr(
                material, unreal.MaterialExpressionAdd, 360, 20
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                wet_base, "", lifted_base, "A"
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                lift_constant, "", lifted_base, "B"
            )
            base_output = lifted_base
        unreal.MaterialEditingLibrary.connect_material_property(
            base_output, "", unreal.MaterialProperty.MP_BASE_COLOR
        )
    else:
        unreal.MaterialEditingLibrary.connect_material_property(
            base_sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
        )

    normal_sample = _sample(
        material,
        _load_texture(f"{stem}_N"),
        uv,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
        180,
    )
    normal_output = normal_sample
    normal_output_pin = "RGB"
    if wet_sample is not None and spec.get("wet_normal_flatten", 0.0) > 0.0:
        flatten_scale = _expr(material, unreal.MaterialExpressionConstant, -420, 820)
        flatten_scale.set_editor_property("r", spec["wet_normal_flatten"])
        flatten_alpha = _expr(material, unreal.MaterialExpressionMultiply, -220, 760)
        unreal.MaterialEditingLibrary.connect_material_expressions(
            wet_output, wet_output_pin, flatten_alpha, "A"
        )
        unreal.MaterialEditingLibrary.connect_material_expressions(flatten_scale, "", flatten_alpha, "B")
        flat_normal = _expr(material, unreal.MaterialExpressionConstant3Vector, -220, 900)
        flat_normal.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
        flattened = _expr(material, unreal.MaterialExpressionLinearInterpolate, 0, 220)
        unreal.MaterialEditingLibrary.connect_material_expressions(normal_sample, "RGB", flattened, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(flat_normal, "", flattened, "B")
        unreal.MaterialEditingLibrary.connect_material_expressions(flatten_alpha, "", flattened, "Alpha")
        normal_output = flattened
        normal_output_pin = ""
    unreal.MaterialEditingLibrary.connect_material_property(
        normal_output, normal_output_pin, unreal.MaterialProperty.MP_NORMAL
    )

    rough_sample = _sample(
        material,
        _load_texture(f"{stem}_R"),
        uv,
        unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
        340,
    )
    rough_output = rough_sample
    rough_output_pin = "R"
    if wet_sample is not None:
        wet_rough = _expr(material, unreal.MaterialExpressionConstant, -220, 1040)
        wet_rough.set_editor_property("r", spec["wet_rough"])
        rough_lerp = _expr(material, unreal.MaterialExpressionLinearInterpolate, 0, 400)
        unreal.MaterialEditingLibrary.connect_material_expressions(rough_sample, "R", rough_lerp, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(wet_rough, "", rough_lerp, "B")
        unreal.MaterialEditingLibrary.connect_material_expressions(
            wet_output, wet_output_pin, rough_lerp, "Alpha"
        )
        rough_output = rough_lerp
        rough_output_pin = ""
    unreal.MaterialEditingLibrary.connect_material_property(
        rough_output, rough_output_pin, unreal.MaterialProperty.MP_ROUGHNESS
    )

    ao_sample = _sample(
        material,
        _load_texture(f"{stem}_A"),
        uv,
        unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
        500,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        ao_sample, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION
    )

    if spec.get("metal_map"):
        metal_sample = _sample(
            material,
            _load_texture(f"{stem}_M"),
            uv,
            unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
            980,
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            metal_sample, "R", unreal.MaterialProperty.MP_METALLIC
        )

    specular = _expr(material, unreal.MaterialExpressionConstant, 0, 1120)
    specular.set_editor_property("r", spec.get("specular", 0.5))
    unreal.MaterialEditingLibrary.connect_material_property(
        specular, "", unreal.MaterialProperty.MP_SPECULAR
    )
    return {
        "normal": normal_output,
        "normal_pin": normal_output_pin,
        "roughness": rough_output,
        "roughness_pin": rough_output_pin,
        "ao": ao_sample,
    }


def create_tank_water_material(assets, tools):
    """Dark translucent tank water for the final silhouette reveal.

    The source image supplies only low-frequency settling ripples and mineral
    specks. Runtime opacity and weak per-pixel refraction decide how much of
    the submerged clothing is readable; no body information is baked in.
    """
    source_asset = "T_TankWaterSurface_D"
    if not assets.does_asset_exist(f"{TEXTURE_ROOT}/{source_asset}"):
        unreal.log_warning(
            f"[IndieGame] Skipped M_TankWaterReveal: missing {source_asset}"
        )
        return None

    material = _recreate_material(assets, tools, "M_TankWaterReveal")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property(
            "translucency_lighting_mode",
            unreal.TranslucencyLightingMode.TLM_SURFACE,
        )
    except Exception:  # noqa: BLE001 - enum/property moved between UE minors
        pass

    sample = _sample(
        material,
        _load_texture(source_asset),
        None,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
        0,
    )
    # Two large, slowly crossing ripple fields avoid an obvious scrolling
    # texture. A full UV crossing takes roughly two to four minutes, so the
    # surface remains ordinary standing water rather than a supernatural lens.
    ripple_a_uv = _make_panning_uv(material, 1.15, 0.0040, 0.0060, 120)
    ripple_b_uv = _make_panning_uv(material, 1.70, -0.0060, 0.0035, 420)
    pbr_nodes = _connect_scan_pbr(
        material,
        sample,
        ripple_a_uv,
        {
            "pbr_stem": "T_TankWaterSurface",
            "specular": 0.28,
        },
    )
    ripple_b_normal = _sample(
        material,
        _load_texture("T_TankWaterSurface_N"),
        ripple_b_uv,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
        560,
    )
    ripple_mix = _expr(
        material,
        unreal.MaterialExpressionLinearInterpolate,
        -160,
        520,
    )
    ripple_weight = _expr(material, unreal.MaterialExpressionConstant, -360, 620)
    ripple_weight.set_editor_property("r", 0.44)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        pbr_nodes["normal"], pbr_nodes["normal_pin"], ripple_mix, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        ripple_b_normal, "RGB", ripple_mix, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        ripple_weight, "", ripple_mix, "Alpha"
    )
    ripple_normalized = _expr(
        material,
        unreal.MaterialExpressionNormalize,
        40,
        520,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        ripple_mix, "", ripple_normalized, ""
    )
    flat_normal = _expr(
        material, unreal.MaterialExpressionConstant3Vector, 40, 690
    )
    flat_normal.set_editor_property(
        "constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0)
    )
    normal_flatten = _expr(
        material, unreal.MaterialExpressionConstant, 220, 680
    )
    normal_flatten.set_editor_property("r", 0.58)
    readable_ripples = _expr(
        material, unreal.MaterialExpressionLinearInterpolate, 380, 560
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        ripple_normalized, "", readable_ripples, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        flat_normal, "", readable_ripples, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        normal_flatten, "", readable_ripples, "Alpha"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        readable_ripples, "", unreal.MaterialProperty.MP_NORMAL
    )

    # A standing-water surface inside a closed tank is not a roof mirror. A
    # stable roughness floor suppresses bright rooftop reflections while the
    # two normal fields and weak IOR variation keep the water visibly present.
    water_roughness = _expr(
        material, unreal.MaterialExpressionConstant, 380, 740
    )
    water_roughness.set_editor_property("r", 0.42)
    unreal.MaterialEditingLibrary.connect_material_property(
        water_roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )

    # About 0.23 opacity at the texture's measured dark range. The lower floor
    # preserves a water sheet and refraction, while the runtime sub-surface
    # key can recover the clothing silhouette without making it look printed
    # directly onto an opaque plane.
    opacity_gain = _expr(material, unreal.MaterialExpressionConstant, -620, 220)
    opacity_gain.set_editor_property("r", 0.25)
    opacity_detail = _expr(material, unreal.MaterialExpressionMultiply, -420, 160)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        sample, "R", opacity_detail, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        opacity_gain, "", opacity_detail, "B"
    )
    opacity_floor = _expr(material, unreal.MaterialExpressionConstant, -420, 300)
    opacity_floor.set_editor_property("r", 0.14)
    opacity = _expr(material, unreal.MaterialExpressionAdd, -220, 210)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        opacity_detail, "", opacity, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        opacity_floor, "", opacity, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY
    )

    # Spatial IOR variation stays between roughly 1.006 and 1.018 for the
    # dark source range. It bends the silhouette slightly without turning the
    # water into a wobbling supernatural lens.
    refraction_gain = _expr(material, unreal.MaterialExpressionConstant, -620, 650)
    refraction_gain.set_editor_property("r", 0.09)
    refraction_detail = _expr(material, unreal.MaterialExpressionMultiply, -420, 630)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        sample, "R", refraction_detail, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        refraction_gain, "", refraction_detail, "B"
    )
    refraction_floor = _expr(material, unreal.MaterialExpressionConstant, -420, 760)
    refraction_floor.set_editor_property("r", 1.006)
    refraction = _expr(material, unreal.MaterialExpressionAdd, -220, 690)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        refraction_detail, "", refraction, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        refraction_floor, "", refraction, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        refraction, "", unreal.MaterialProperty.MP_REFRACTION
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.log("[IndieGame] Created tank reveal water: M_TankWaterReveal")
    return material


def create_wet_asphalt(assets, tools):
    """Dew-wet alley asphalt: large-scale puddle mask flattens the normal and
    drops roughness to a mirror so Lumen reflects the signs and streetlights."""
    material = _recreate_material(assets, tools, "M_AsphaltWorld")

    base_uv = _make_uv_source(material, "XY", 260.0, 0)
    diffuse = _sample(
        material, _load_texture("T_Asphalt_D"), base_uv,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 0)
    normal = _sample(
        material, _load_texture("T_Asphalt_N"),
        _make_uv_source(material, "XY", 260.0, 380),
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 380)
    rough = _sample(
        material, _load_texture("T_Asphalt_R"),
        _make_uv_source(material, "XY", 260.0, 760),
        unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, 760)

    # Puddle mask: the same roughness map read at street scale.
    mask = _sample(
        material, _load_texture("T_Asphalt_R"),
        _make_uv_source(material, "XY", 1150.0, 1140),
        unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, 1140)
    threshold = _expr(material, unreal.MaterialExpressionConstant, -1100, 1320)
    threshold.set_editor_property("r", 0.42)
    below = _expr(material, unreal.MaterialExpressionSubtract, -900, 1240)
    unreal.MaterialEditingLibrary.connect_material_expressions(threshold, "", below, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(mask, "R", below, "B")
    sharpen = _expr(material, unreal.MaterialExpressionConstant, -900, 1380)
    sharpen.set_editor_property("r", 6.0)
    scaled = _expr(material, unreal.MaterialExpressionMultiply, -740, 1260)
    unreal.MaterialEditingLibrary.connect_material_expressions(below, "", scaled, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(sharpen, "", scaled, "B")
    puddle = _expr(material, unreal.MaterialExpressionSaturate, -600, 1260)
    unreal.MaterialEditingLibrary.connect_material_expressions(scaled, "", puddle, "")

    # Base color darkens where wet.
    dark_scale = _expr(material, unreal.MaterialExpressionLinearInterpolate, -420, 120)
    one = _expr(material, unreal.MaterialExpressionConstant, -600, 40)
    one.set_editor_property("r", 1.0)
    wet_dark = _expr(material, unreal.MaterialExpressionConstant, -600, 180)
    wet_dark.set_editor_property("r", 0.45)
    unreal.MaterialEditingLibrary.connect_material_expressions(one, "", dark_scale, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(wet_dark, "", dark_scale, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(puddle, "", dark_scale, "Alpha")
    tinted = _expr(material, unreal.MaterialExpressionMultiply, -240, 60)
    unreal.MaterialEditingLibrary.connect_material_expressions(diffuse, "RGB", tinted, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(dark_scale, "", tinted, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Roughness collapses to a mirror inside puddles.
    mirror = _expr(material, unreal.MaterialExpressionConstant, -420, 820)
    mirror.set_editor_property("r", 0.03)
    rough_mix = _expr(material, unreal.MaterialExpressionLinearInterpolate, -240, 780)
    unreal.MaterialEditingLibrary.connect_material_expressions(rough, "R", rough_mix, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(mirror, "", rough_mix, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(puddle, "", rough_mix, "Alpha")
    unreal.MaterialEditingLibrary.connect_material_property(
        rough_mix, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Standing water lies flat: blend the normal toward straight up.
    flat = _expr(material, unreal.MaterialExpressionConstant3Vector, -420, 480)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    normal_mix = _expr(material, unreal.MaterialExpressionLinearInterpolate, -240, 440)
    unreal.MaterialEditingLibrary.connect_material_expressions(normal, "RGB", normal_mix, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(flat, "", normal_mix, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(puddle, "", normal_mix, "Alpha")
    unreal.MaterialEditingLibrary.connect_material_property(
        normal_mix, "", unreal.MaterialProperty.MP_NORMAL)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.log("[IndieGame] Created wet asphalt: M_AsphaltWorld")
    return material


def create_wet_step(assets, tools):
    """Thin opaque puddle material for footprint meshes.

    The print is dark because the underlying floor is wet, not because it is
    painted black. A low roughness/high specular response lets the same surface
    read under the lift and lobby lights without a translucent sorting fringe.
    """
    material = _recreate_material(assets, tools, "M_WetStep")
    material.set_editor_property("two_sided", True)

    base = _expr(material, unreal.MaterialExpressionConstant3Vector, -600, 0)
    base.set_editor_property(
        "constant", unreal.LinearColor(0.040, 0.045, 0.050, 1.0)
    )
    roughness = _expr(material, unreal.MaterialExpressionConstant, -600, 160)
    roughness.set_editor_property("r", 0.18)
    specular = _expr(material, unreal.MaterialExpressionConstant, -600, 280)
    specular.set_editor_property("r", 0.55)

    unreal.MaterialEditingLibrary.connect_material_property(
        base, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        specular, "", unreal.MaterialProperty.MP_SPECULAR
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.log("[IndieGame] Created wet footprint material: M_WetStep")
    return material


def create_sky_material(assets, tools):
    material = _recreate_material(assets, tools, "M_SkyDawn")
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    world_position = _expr(material, unreal.MaterialExpressionWorldPosition, -1500, 0)

    # Vertical gradient: horizon glow fades into a near-black zenith.
    mask_z = _expr(material, unreal.MaterialExpressionComponentMask, -1300, 0)
    mask_z.set_editor_property("r", False)
    mask_z.set_editor_property("g", False)
    mask_z.set_editor_property("b", True)
    unreal.MaterialEditingLibrary.connect_material_expressions(world_position, "", mask_z, "")

    height_scale = _expr(material, unreal.MaterialExpressionConstant, -1300, 160)
    height_scale.set_editor_property("r", 1.0 / 2600.0)
    height_norm = _expr(material, unreal.MaterialExpressionMultiply, -1100, 40)
    unreal.MaterialEditingLibrary.connect_material_expressions(mask_z, "", height_norm, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(height_scale, "", height_norm, "B")
    height_saturated = _expr(material, unreal.MaterialExpressionSaturate, -950, 40)
    unreal.MaterialEditingLibrary.connect_material_expressions(height_norm, "", height_saturated, "")

    horizon = _expr(material, unreal.MaterialExpressionConstant3Vector, -800, -160)
    horizon.set_editor_property("constant", unreal.LinearColor(0.085, 0.052, 0.075, 1.0))
    zenith = _expr(material, unreal.MaterialExpressionConstant3Vector, -800, 0)
    zenith.set_editor_property("constant", unreal.LinearColor(0.004, 0.008, 0.02, 1.0))
    gradient = _expr(material, unreal.MaterialExpressionLinearInterpolate, -600, -60)
    unreal.MaterialEditingLibrary.connect_material_expressions(horizon, "", gradient, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(zenith, "", gradient, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(height_saturated, "", gradient, "Alpha")

    # A faint warm smear low in the east: dawn is close but not here yet.
    mask_x = _expr(material, unreal.MaterialExpressionComponentMask, -1300, 400)
    mask_x.set_editor_property("r", True)
    mask_x.set_editor_property("g", False)
    mask_x.set_editor_property("b", False)
    unreal.MaterialEditingLibrary.connect_material_expressions(world_position, "", mask_x, "")
    east_scale = _expr(material, unreal.MaterialExpressionConstant, -1300, 560)
    east_scale.set_editor_property("r", 1.0 / 5200.0)
    east_norm = _expr(material, unreal.MaterialExpressionMultiply, -1100, 440)
    unreal.MaterialEditingLibrary.connect_material_expressions(mask_x, "", east_norm, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(east_scale, "", east_norm, "B")
    east_saturated = _expr(material, unreal.MaterialExpressionSaturate, -950, 440)
    unreal.MaterialEditingLibrary.connect_material_expressions(east_norm, "", east_saturated, "")

    inverse_height = _expr(material, unreal.MaterialExpressionOneMinus, -950, 240)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        height_saturated, "", inverse_height, ""
    )
    east_falloff = _expr(material, unreal.MaterialExpressionMultiply, -750, 380)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        east_saturated, "", east_falloff, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        inverse_height, "", east_falloff, "B"
    )

    warm = _expr(material, unreal.MaterialExpressionConstant3Vector, -750, 540)
    warm.set_editor_property("constant", unreal.LinearColor(0.14, 0.05, 0.015, 1.0))
    east_glow = _expr(material, unreal.MaterialExpressionMultiply, -550, 440)
    unreal.MaterialEditingLibrary.connect_material_expressions(east_falloff, "", east_glow, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(warm, "", east_glow, "B")

    sky = _expr(material, unreal.MaterialExpressionAdd, -350, 120)
    unreal.MaterialEditingLibrary.connect_material_expressions(gradient, "", sky, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(east_glow, "", sky, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        sky, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.log("[IndieGame] Created sky material: M_SkyDawn")
    return material


def create_cctv_monitor_material(assets, tools):
    """§14 CCTV 채널 5 — the render-target monitor face.

    Three runtime parameters, and the C++ side sets all three by these exact
    names: `Feed` takes the scene-capture render target, `Static` crossfades to
    snow, `Gain` is the tube's brightness. The default `Feed` texture is engine
    black on purpose — if the binding ever fails the channel is simply dead,
    which is the one wrong state that cannot spoil the reveal.

    The look is not a picture pasted on a plane. It is a small analog tube in a
    dark booth: the feed is desaturated (a cheap IR sensor has no colour),
    tinted to phosphor, cut by 144 scanline pairs, crossed by the slow bright
    sync bar every analog monitor drifts, then vignetted the way a curved tube
    loses its corners. Emissive-only and unlit, so the screen is the light
    source and Lumen carries it onto Mok Hansu's desk.
    """
    material = _recreate_material(assets, tools, "M_CctvChannelFive")
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_UNLIT
    )
    material.set_editor_property("two_sided", False)

    uv = _expr(material, unreal.MaterialExpressionTextureCoordinate, -1800, 0)
    uv.set_editor_property("u_tiling", 1.0)
    uv.set_editor_property("v_tiling", 1.0)

    black = unreal.load_asset("/Engine/EngineResources/Black")
    if black is None:
        black = unreal.load_asset("/Engine/EngineResources/DefaultTexture")
    if black is None:
        raise RuntimeError("Could not load an engine default texture for Feed")
    feed = _expr(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -1550, 0
    )
    feed.set_editor_property("parameter_name", "Feed")
    feed.set_editor_property("texture", black)
    feed.set_editor_property(
        "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        uv, "", feed, "UVs"
    )

    # A 2.8 mm CCTV lens on an IR sensor has no colour at all, so the feed is
    # taken to luminance and given back only the tube's own phosphor cast.
    mono = _expr(material, unreal.MaterialExpressionDesaturation, -1300, 0)
    full = _expr(material, unreal.MaterialExpressionConstant, -1500, 180)
    full.set_editor_property("r", 1.0)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        feed, "RGB", mono, ""
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        full, "", mono, "Fraction"
    )
    phosphor = _expr(material, unreal.MaterialExpressionConstant3Vector, -1300, 200)
    phosphor.set_editor_property(
        "constant", unreal.LinearColor(0.74, 0.86, 0.93, 1.0)
    )
    tinted = _expr(material, unreal.MaterialExpressionMultiply, -1080, 60)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        mono, "", tinted, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        phosphor, "", tinted, "B"
    )

    # 288 visible lines on a CIF channel read as 144 dark/bright pairs.
    v_mask = _expr(material, unreal.MaterialExpressionComponentMask, -1550, 420)
    v_mask.set_editor_property("r", False)
    v_mask.set_editor_property("g", True)
    v_mask.set_editor_property("b", False)
    v_mask.set_editor_property("a", False)
    unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", v_mask, "")

    line_count = _expr(material, unreal.MaterialExpressionConstant, -1550, 560)
    line_count.set_editor_property("r", 144.0)
    scan_phase = _expr(material, unreal.MaterialExpressionMultiply, -1380, 460)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        v_mask, "", scan_phase, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        line_count, "", scan_phase, "B"
    )
    scan_sine = _expr(material, unreal.MaterialExpressionSine, -1220, 460)
    scan_sine.set_editor_property("period", 1.0)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        scan_phase, "", scan_sine, ""
    )
    scan_half = _expr(material, unreal.MaterialExpressionConstant, -1220, 600)
    scan_half.set_editor_property("r", 0.5)
    scan_scaled = _expr(material, unreal.MaterialExpressionMultiply, -1060, 480)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        scan_sine, "", scan_scaled, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        scan_half, "", scan_scaled, "B"
    )
    scan_norm = _expr(material, unreal.MaterialExpressionAdd, -900, 480)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        scan_scaled, "", scan_norm, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        scan_half, "", scan_norm, "B"
    )
    scan_floor = _expr(material, unreal.MaterialExpressionConstant, -900, 620)
    scan_floor.set_editor_property("r", 0.72)
    scan_ceil = _expr(material, unreal.MaterialExpressionConstant, -900, 700)
    scan_ceil.set_editor_property("r", 1.0)
    scan_gain = _expr(material, unreal.MaterialExpressionLinearInterpolate, -740, 520)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        scan_floor, "", scan_gain, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        scan_ceil, "", scan_gain, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        scan_norm, "", scan_gain, "Alpha"
    )

    # The slow sync bar. Nobody in the building ever adjusted the vertical hold.
    time_expr = _expr(material, unreal.MaterialExpressionTime, -1800, 900)
    roll_speed = _expr(material, unreal.MaterialExpressionConstant, -1800, 1020)
    roll_speed.set_editor_property("r", -0.11)
    roll_drift = _expr(material, unreal.MaterialExpressionMultiply, -1620, 940)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        time_expr, "", roll_drift, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roll_speed, "", roll_drift, "B"
    )
    roll_sum = _expr(material, unreal.MaterialExpressionAdd, -1440, 900)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        v_mask, "", roll_sum, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roll_drift, "", roll_sum, "B"
    )
    roll_wrap = _expr(material, unreal.MaterialExpressionFrac, -1280, 900)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roll_sum, "", roll_wrap, ""
    )
    roll_sine = _expr(material, unreal.MaterialExpressionSine, -1120, 900)
    roll_sine.set_editor_property("period", 1.0)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roll_wrap, "", roll_sine, ""
    )
    roll_lobe = _expr(material, unreal.MaterialExpressionSaturate, -960, 900)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roll_sine, "", roll_lobe, ""
    )
    roll_tight = _expr(material, unreal.MaterialExpressionPower, -800, 900)
    roll_tight.set_editor_property("const_exponent", 6.0)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roll_lobe, "", roll_tight, "Base"
    )
    bar_floor = _expr(material, unreal.MaterialExpressionConstant, -800, 1040)
    bar_floor.set_editor_property("r", 1.0)
    bar_peak = _expr(material, unreal.MaterialExpressionConstant, -800, 1120)
    bar_peak.set_editor_property("r", 1.10)
    bar_gain = _expr(material, unreal.MaterialExpressionLinearInterpolate, -640, 940)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        bar_floor, "", bar_gain, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        bar_peak, "", bar_gain, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        roll_tight, "", bar_gain, "Alpha"
    )

    tube_gain = _expr(material, unreal.MaterialExpressionMultiply, -480, 700)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        scan_gain, "", tube_gain, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        bar_gain, "", tube_gain, "B"
    )
    picture = _expr(material, unreal.MaterialExpressionMultiply, -320, 300)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        tinted, "", picture, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        tube_gain, "", picture, "B"
    )

    # 지직임. Animated value noise, one level: this runs on a 34 cm plane and
    # the beat is seven seconds long, so it must not cost like a screen effect.
    snow_scale = _expr(material, unreal.MaterialExpressionConstant, -1800, 1300)
    snow_scale.set_editor_property("r", 320.0)
    snow_uv = _expr(material, unreal.MaterialExpressionMultiply, -1620, 1240)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        uv, "", snow_uv, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_scale, "", snow_uv, "B"
    )
    snow_rate = _expr(material, unreal.MaterialExpressionConstant, -1800, 1420)
    snow_rate.set_editor_property("r", 37.0)
    snow_time = _expr(material, unreal.MaterialExpressionMultiply, -1620, 1400)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        time_expr, "", snow_time, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_rate, "", snow_time, "B"
    )
    snow_pos = _expr(material, unreal.MaterialExpressionAppendVector, -1440, 1300)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_uv, "", snow_pos, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_time, "", snow_pos, "B"
    )
    snow_noise = _expr(material, unreal.MaterialExpressionNoise, -1240, 1300)
    snow_noise.set_editor_property(
        "noise_function", unreal.NoiseFunction.NOISEFUNCTION_VALUE_ALU
    )
    snow_noise.set_editor_property("scale", 1.0)
    snow_noise.set_editor_property("quality", 1)
    snow_noise.set_editor_property("levels", 1)
    snow_noise.set_editor_property("turbulence", False)
    snow_noise.set_editor_property("output_min", 0.02)
    snow_noise.set_editor_property("output_max", 1.0)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_pos, "", snow_noise, "Position"
    )
    snow_tint = _expr(material, unreal.MaterialExpressionConstant3Vector, -1240, 1460)
    snow_tint.set_editor_property(
        "constant", unreal.LinearColor(0.86, 0.92, 1.0, 1.0)
    )
    snow = _expr(material, unreal.MaterialExpressionMultiply, -1020, 1340)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_noise, "", snow, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_tint, "", snow, "B"
    )

    static_amount = _expr(
        material, unreal.MaterialExpressionScalarParameter, -1020, 1500
    )
    static_amount.set_editor_property("parameter_name", "Static")
    static_amount.set_editor_property("default_value", 0.0)
    signal = _expr(material, unreal.MaterialExpressionLinearInterpolate, -160, 500)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        picture, "", signal, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(snow, "", signal, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(
        static_amount, "", signal, "Alpha"
    )

    # A curved tube loses its corners. This is also what keeps the monitor from
    # lighting the booth like a lightbox instead of a screen.
    centre = _expr(material, unreal.MaterialExpressionConstant2Vector, -1550, 1700)
    centre.set_editor_property("r", 0.5)
    centre.set_editor_property("g", 0.5)
    radius = _expr(material, unreal.MaterialExpressionDistance, -1360, 1660)
    unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", radius, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(
        centre, "", radius, "B"
    )
    vignette_slope = _expr(material, unreal.MaterialExpressionConstant, -1360, 1800)
    vignette_slope.set_editor_property("r", 0.92)
    vignette_fall = _expr(material, unreal.MaterialExpressionMultiply, -1180, 1700)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        radius, "", vignette_fall, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        vignette_slope, "", vignette_fall, "B"
    )
    vignette_raw = _expr(material, unreal.MaterialExpressionOneMinus, -1000, 1700)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        vignette_fall, "", vignette_raw, ""
    )
    vignette_clamped = _expr(material, unreal.MaterialExpressionSaturate, -840, 1700)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        vignette_raw, "", vignette_clamped, ""
    )
    vignette = _expr(material, unreal.MaterialExpressionPower, -680, 1700)
    vignette.set_editor_property("const_exponent", 1.6)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        vignette_clamped, "", vignette, "Base"
    )

    shaded = _expr(material, unreal.MaterialExpressionMultiply, -40, 900)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        signal, "", shaded, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        vignette, "", shaded, "B"
    )
    gain = _expr(material, unreal.MaterialExpressionScalarParameter, -40, 1080)
    gain.set_editor_property("parameter_name", "Gain")
    gain.set_editor_property("default_value", 1.0)
    emissive = _expr(material, unreal.MaterialExpressionMultiply, 120, 940)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        shaded, "", emissive, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        gain, "", emissive, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.log("[IndieGame] Created CCTV monitor material: M_CctvChannelFive")
    return material


def run():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    if assets is None or tools is None:
        raise RuntimeError("Unreal editor asset services are unavailable")

    if os.environ.get("IG_WET_STEP_ONLY") == "1":
        wet_step = create_wet_step(assets, tools)
        if not assets.save_loaded_assets([wet_step], False):
            raise RuntimeError("Could not save M_WetStep")
        unreal.log("[IndieGame] Wet footprint material update complete")
        return
    if os.environ.get("IG_CORRIDOR_SIGNAGE_ONLY") == "1":
        names = (
            "M_Note404NotFound",
            "M_CaptureMercyNote",
            "M_MercyNoteUnderDoor",
            "M_SignAux5MonitorOnly",
            "M_Plate402",
            "M_PlateCommon",
        )
        signage = create_flat_texture_materials(
            assets,
            tools,
            {name: DECAL_MATERIALS[name] for name in names},
            False,
            update_in_place=True,
        )
        if len(signage) != len(names):
            raise RuntimeError("Could not build corridor entrance signage materials")
        # The channel-5 tube ships in the same pass as its AUX label: the screen
        # and the reason it is not recorded are one beat, and baking them apart
        # is how the two end up contradicting each other (§19.9 위험 8).
        signage.append(create_cctv_monitor_material(assets, tools))
        if not assets.save_loaded_assets(signage, False):
            raise RuntimeError("Could not save corridor entrance signage materials")
        unreal.log("[IndieGame] Corridor entrance signage material update complete")
        return
    if os.environ.get("IG_MISSING_FLOOR_ONLY") == "1":
        world_names = (
            "M_MissingFloorPlaster_X",
            "M_MissingFloorPlaster_Y",
            "M_MissingFloorPlaster_XY",
        )
        residue_names = (
            "M_MissingFloorHandprints",
            "M_MissingFloorDragTrails",
            "M_MissingFloorDustJoint",
            "M_MissingFloorCavityScratches",
        )
        sprite_names = (
            "M_SpriteSeo",
            "M_SpriteMok",
            "M_SpriteHwang",
            "M_SpriteNarin",
            "M_SpriteListenerFront",
            "M_SpriteListenerCrawl0",
            "M_SpriteListenerCrawl1",
            "M_SpriteListenerCrawl2",
            "M_SpriteListenerCrawl3",
            "M_SpriteFinalCavity",
            "M_SpriteMokFinalUpper",
        )
        missing_floor = create_textured_materials(
            assets,
            tools,
            {name: TEXTURED_MATERIALS[name] for name in world_names},
        )
        missing_floor += create_flat_texture_materials(
            assets,
            tools,
            {
                "M_MissingFloorListenerPlasterUV": DECAL_MATERIALS[
                    "M_MissingFloorListenerPlasterUV"
                ]
            },
            False,
        )
        missing_floor += create_masked_texture_materials(
            assets,
            tools,
            {name: EVIDENCE_MASK_MATERIALS[name] for name in residue_names},
            True,
        )
        missing_floor += create_masked_texture_materials(
            assets,
            tools,
            {name: SURFACE_OVERLAY_MATERIALS[name] for name in sprite_names},
            False,
        )
        if len(missing_floor) != 19 or not assets.save_loaded_assets(
            missing_floor, False
        ):
            raise RuntimeError("Could not save missing-floor visual materials")
        unreal.log("[IndieGame] Missing-floor visual material update complete")
        return
    if os.environ.get("IG_APARTMENT_VISUAL_ONLY") == "1":
        apartment_material_names = (
            "M_Wallpaper_X",
            "M_Wallpaper_Y",
            "M_WallpaperCeil",
        )
        apartment_materials = create_textured_materials(
            assets,
            tools,
            {
                name: TEXTURED_MATERIALS[name]
                for name in apartment_material_names
            },
        )
        apartment_materials += create_masked_texture_materials(
            assets,
            tools,
            {
                "M_ApartmentWallPatina": EVIDENCE_MASK_MATERIALS[
                    "M_ApartmentWallPatina"
                ]
            },
            True,
        )
        if len(apartment_materials) != 4 or not assets.save_loaded_assets(
            apartment_materials, False
        ):
            raise RuntimeError("Could not save apartment visual materials")
        unreal.log("[IndieGame] Apartment visual material update complete")
        return
    if os.environ.get("IG_CAB_MIRROR_ONLY") == "1":
        mirrors = create_textured_materials(
            assets, tools, {"M_CabMirrorUV": TEXTURED_MATERIALS["M_CabMirrorUV"]}
        )
        if not assets.save_loaded_assets(mirrors, False):
            raise RuntimeError("Could not save M_CabMirrorUV")
        unreal.log("[IndieGame] Cab mirror material update complete")
        return
    if os.environ.get("IG_RETAIL_SIGNS_ONLY") == "1":
        signs = create_flat_texture_materials(
            assets, tools, SIGN_MATERIALS, True, update_in_place=True
        )
        if not assets.save_loaded_assets(signs, False):
            raise RuntimeError("Could not save retail sign materials")
        unreal.log("[IndieGame] Retail sign emissive polish complete")
        return
    if os.environ.get("IG_TANK_WATER_ONLY") == "1":
        tank_water = create_tank_water_material(assets, tools)
        if tank_water is None or not assets.save_loaded_assets([tank_water], False):
            raise RuntimeError("Could not save M_TankWaterReveal")
        unreal.log("[IndieGame] Tank water material update complete")
        return
    if os.environ.get("IG_TANK_INTERIOR_ONLY") == "1":
        tank_interior = create_flat_texture_materials(
            assets,
            tools,
            {
                "M_TankInteriorBiofilmUV": DECAL_MATERIALS[
                    "M_TankInteriorBiofilmUV"
                ]
            },
            False,
        )
        if not tank_interior or not assets.save_loaded_assets(
            tank_interior, False
        ):
            raise RuntimeError("Could not save M_TankInteriorBiofilmUV")
        unreal.log("[IndieGame] Tank interior material update complete")
        return
    if os.environ.get("IG_SUBMERGED_CLOTHING_ONLY") == "1":
        names = (
            "M_SubmergedHoodieUV",
            "M_SubmergedPantsUV",
            "M_SubmergedSlippersUV",
            "M_SubmergedSlipperWearUV",
        )
        submerged_clothing = create_flat_texture_materials(
            assets,
            tools,
            {name: DECAL_MATERIALS[name] for name in names},
            False,
        )
        if len(submerged_clothing) != len(names) or not assets.save_loaded_assets(
            submerged_clothing, False
        ):
            raise RuntimeError("Could not save submerged clothing materials")
        unreal.log("[IndieGame] Submerged clothing material update complete")
        return

    created = []
    created += create_textured_materials(assets, tools)
    created += create_flat_texture_materials(assets, tools, DECAL_MATERIALS, False)
    created += create_flat_texture_materials(assets, tools, SIGN_MATERIALS, True)
    created += create_masked_texture_materials(
        assets, tools, EVIDENCE_MASK_MATERIALS, True
    )
    created += create_masked_texture_materials(
        assets, tools, SURFACE_OVERLAY_MATERIALS, False
    )
    carrier_bag = create_carrier_bag_material(assets, tools)
    if carrier_bag is not None:
        created.append(carrier_bag)
    tank_water = create_tank_water_material(assets, tools)
    if tank_water is not None:
        created.append(tank_water)
    created.append(create_wet_asphalt(assets, tools))
    created.append(create_wet_step(assets, tools))
    created.append(create_sky_material(assets, tools))
    enable_instanced_product_usage(created)

    if not assets.save_loaded_assets(created, False):
        raise RuntimeError("Could not save textured materials")
    unreal.log(f"[IndieGame] Textured material pass complete: {len(created)} materials")


if __name__ == "__main__":
    run()
