# Asset and License Policy

외부 에셋은 다운로드 시점에 상업적 게임 배포, 수정, 플랫폼 패키징 권한을 확인한 뒤 저장소에 추가합니다. 출처를 기억에 의존하지 않습니다.

## 반입 규칙

- 라이선스 원문 또는 영수증을 프로젝트 외부의 안전한 보관소에도 보존합니다.
- 재배포 금지 에셋은 원본 파일을 공개 저장소에 올리지 않습니다.
- 생성형 AI 결과물은 사용한 서비스, 생성 날짜와 당시 약관을 기록합니다.
- 사람의 얼굴·목소리·상표·실제 주소가 포함된 자료는 별도 초상권/상표권/개인정보 검토를 거칩니다.
- 코드 라이선스와 콘텐츠 라이선스를 구분합니다.

## 에셋 대장

| 프로젝트 경로 | 제작자/출처 | 라이선스 | 취득일 | 수정 여부 | 증빙 위치 | 비고 |
|---|---|---|---|---|---|---|
| `/Game/Prototype/Textures/T_Photo_Brick_*` | ambientCG.com — Bricks090 | CC0 1.0 | 2026-07-19 | 원본(1K JPG) | `Content/SourceArt/PhotoZips/` | 골목 적벽돌 |
| `/Game/Prototype/Textures/T_Photo_Asphalt_*` | ambientCG.com — Asphalt012 | CC0 1.0 | 2026-07-19 | 원본(1K JPG) | `Content/SourceArt/PhotoZips/` | 골목 노면 |
| `/Game/Prototype/Textures/T_Photo_Concrete_*` | ambientCG.com — Concrete034 | CC0 1.0 | 2026-07-19 | 원본(1K JPG) | `Content/SourceArt/PhotoZips/` | 외벽/매장 벽 |
| `/Game/Prototype/Textures/T_Photo_StoreTile_*` | ambientCG.com — Tiles101 | CC0 1.0 | 2026-07-19 | 원본(1K JPG) | `Content/SourceArt/PhotoZips/` | 편의점 바닥 |
| `/Game/Prototype/Textures/T_Photo_Jangpan_*` | ambientCG.com — WoodFloor051 | CC0 1.0 | 2026-07-19 | 원본(1K JPG) | `Content/SourceArt/PhotoZips/` | 원룸 장판 |
| `/Game/Prototype/Textures/T_Photo_Wallpaper_*` | ambientCG.com — Plaster003 | CC0 1.0 | 2026-07-19 | 원본(1K JPG) | `Content/SourceArt/PhotoZips/` | 원룸 벽지 |
| `/Game/Prototype/Textures/T_Photo_CeilingTile_*` | ambientCG.com — OfficeCeiling005 | CC0 1.0 | 2026-07-19 | 원본(1K JPG) | `Content/SourceArt/PhotoZips/` | 매장 천장 |
| `/Game/Prototype/Textures/T_Photo_MetalBrushed_*` | ambientCG.com — Metal032 | CC0 1.0 | 2026-07-19 | 원본(1K JPG) | `Content/SourceArt/PhotoZips/` | 금속 표면 |
| `/Game/Prototype/Textures/T_Photo_Blanket_*` | ambientCG.com — Fabric022 | CC0 1.0 | 2026-07-19 | 원본(1K JPG) | `Content/SourceArt/PhotoZips/` | 침구 원단 |
| `/Game/Prototype/Textures/T_Photo_WoodDark_*` | ambientCG.com — Wood067 | CC0 1.0 | 2026-07-19 | 원본(1K JPG) | `Content/SourceArt/PhotoZips/` | 가구 목재 |
| `/Game/Prototype/Textures/T_Photo_VillaBrick_*` | ambientCG.com — Bricks059 | CC0 1.0 | 2026-09-11 | 원본(2K JPG) | `Content/SourceArt/PhotoZips/` | 빌라 파사드 적벽돌. `M_VillaBrick_{X,Y}` |
| `/Game/Audio/S_*` (67종: 발소리 6면·문·노크·위층 사람·놀람·베드) | OpenGameArt rubberduck 「100 CC0 SFX」 1·2·wood-metal, Kenney 「Impact Sounds」, Owlish Media 「Sound Effects Pack」 | CC0 1.0 | 2026-09-11 | 가공(피치·저역·겹침·되울림·루프 이음) | `Content/SourceArt/Audio/manifest.json`, `Scripts/curate_cc0_audio.py` | 원본 팩은 `Saved/AudioCC0/`에 두고 저장소에는 가공본만 둔다. 게임은 `IGAudio::Sample`로 찾고 없으면 합성기 |
| `/Game/Meshes/SK_ListenerCrawler`, `A_ListenerCrawler_{Crawl,Listen,Bang,Lunge}`, `DA_IGCharacterLODs` | 직접 제작 (사진 참고 → gpt-image 기준 이미지 → TRELLIS.2 → Blender 표면 정리·접지 리깅) | 프로젝트 소유 | 2026-09-14 | 새 원본으로 교체 | `Content/SourceArt/Blender/SK_ListenerCrawler/`, `Scripts/blender/rig_crawler.py` | 12,000삼각형, 21개 뼈, 네 동작, 스켈레탈 LOD 네 단계. `Docs/BLENDER_PIPELINE.md` 「리깅된 인물」 |
| `Content/SourceArt/Reference/Listener/*` | Wikimedia Commons — 미 육군·해병대 낮은 포복 사진, 살아 있는 조각상 사진 | 파일별 공개 저작물·CC 라이선스, 참고 전용 | 2026-09-11 | 원본 | 파일명이 Commons 파일명 | 이번에 채택한 두 사진의 정확한 출처와 공개 조건은 `Docs/REALISM_REVIEW_2026-09-14.md`. 다른 사진의 라이선스까지 같은 것으로 간주하지 않는다 |
| `Content/SourceArt/AI/ListenerPhotoAnchor_20260914.png`, `ListenerTurnaround_20260914.png` | OpenAI 내장 ImageGen, gpt-image 스킬 흐름 | 생성 원본, 프로젝트 제작 자료 | 2026-09-14 | 원본 | `Docs/REALISM_REVIEW_2026-09-14.md` | 가상 인물의 기준 이미지와 다각도·확대 시트. 게임 패키지에 직접 포함하지 않는다 |
| `/Game/Photo/Props/*` (15종: old_bed_frame, side_table_01, metal_office_desk, painted_wooden_chair_01, modern_wooden_cabinet, desk_lamp_arm_01, electric_stove, street_lamp_01, trashbag, cardboard_box_01, steel_frame_shelves_01, CashRegister_01, plastic_crate_01, utility_box_01, wine_bottles_01) | polyhaven.com (포토그래메트리 스캔) | CC0 1.0 | 2026-07-19 | 원본(glTF, 1K 텍스처) | `Content/SourceArt/PhotoProps/` | 실물 스캔 소품 |
| `/Game/Meshes/SM_*` (56종: 기존 49종 + M5 공동 잔존물 4종·목한수 근접 대치 3종) | 직접 제작 (UE5 Geometry Script 절차 모델링) | 프로젝트 소유 | 2026-08-05 | 원본 | `Scripts/generate_meshes.py` | 회전체·베벨·불리언·스윕. 생활 소품·생물·설비·인체·사고 프롭과 M5의 건조한 의복/골격/방수포/캐스터·목한수 작업복/머리와 손/석고보드를 ImageGen 비율 기준과 실제 치수 계약에 맞춰 절차 메시로 재구성 |
| `/Game/Meshes/SM_UnitDoorLeaf·L, SM_UnitDoorHardware·L, SM_UnitDoorFrame, SM_FireExtinguisherBox, SM_FireExtinguisher, SM_MailboxUnit, SM_CeilingLightRing·Dome, SM_FridgeBody·Door, SM_KitchenBaseRun, SM_DrumWasher, SM_KitchenWallUnits, SM_RangeHood, SM_Microwave, SM_KitchenSink, SM_InductionHob, SM_Wardrobe, SM_WallAirConditioner, SM_TrafficCone, SM_StoreCoolerBank·Door, SM_StoreGondola, SM_StoreCounter, SM_CardTerminal, SM_HotSnackWarmer, SM_ChestFreezer, SM_OpenShowcase, SM_RamyeonRack, SM_ApartmentWindow, SM_VenetianBlind, SM_VideoIntercom, SM_WallSwitch, SM_ShoeCabinet, SM_VillaWindow, SM_UtilityPole, SM_GasMeterBox, SM_AcOutdoorUnit, SM_ConvexMirror, SM_CupNoodle·Sleeve·Lid, SM_SnackBoxA~D, SM_TriangleKimbapA~D, SM_RiceBowlPack, SM_TobaccoCabinet, SM_WindowBar, SM_HotWaterDispenser, SM_TrashBin` (58종) | 직접 제작 (Blender 5.2 헤드리스 절차 모델링, Cycles 베이크) | 프로젝트 소유 | 2026-09-04 | 원본 | `Content/SourceArt/Blender/<이름>/`, `Scripts/blender/build_*.py` | 실제 치수·베벨·UCX 충돌·구운 D/N/ORM. `Docs/BLENDER_PIPELINE.md` |
| `/Game/Meshes/SM_ListenerEntityCrawl, SM_AlleyCatRun, SM_MokHansooFigure, SM_FinalCavityRemains` | 직접 제작 (기준 시트 한 칸 → ComfyUI 네이티브 TRELLIS.2 형상 생성 → Blender 다듬기) | 프로젝트 소유. TRELLIS.2 가중치 MIT(Microsoft, Comfy-Org 재포장), DINOv3 Meta 제한 허가, BiRefNet MIT | 2026-09-08 | 원본 | `Content/SourceArt/Generated/<이름>/<시도>/generation.json`, `Scripts/generate_3d_comfy.py`, `Scripts/blender/refine_generated.py` | 입력은 우리 기준 시트뿐. Hunyuan3D는 한국 제외 라이선스라 쓰지 않는다 |
| `/Game/Prototype/Textures/T_<Blender 에셋>_{D,N,ORM,E}` · `/Game/Prototype/Materials/M_IGBakedProp, MI_*` | 직접 제작 (Cycles 베이크, UE 마스터 재질 인스턴스) | 프로젝트 소유 | 2026-09-04 | 원본 | `Scripts/import_blender_assets.py` | 에셋마다 한 세트. ORM은 AO·거칠기·금속성 채널 |
| `/Game/Prototype/Textures/T_Label* · T_Snack*` | 직접 제작 (System.Drawing) — 가상 브랜드, 실제 상표 미사용 | 프로젝트 소유 | 2026-07-25 | 원본 | `Scripts/Create-LabelTextures.ps1` | 제품 라벨·봉지 아트 |
| `Content/SourceArt/Labels/Store/*.png` (담뱃갑 8·과자 상자 4·삼각김밥 4·즉석밥 2·청소년 판매 금지 띠) | 직접 제작 (PIL) — 가상 브랜드, 실제 상표·전화번호 미사용, 담뱃갑 경고면은 글자만 | 프로젝트 소유 | 2026-09-08 | 원본 | `Scripts/create_store_product_art.py` | Blender 빌더가 상품 앞면에 붙여 굽는 입력. 게임 텍스처로 직접 쓰지 않는다 |
| `/Game/Prototype/Textures/T_Sign* · T_Poster* · T_Note*` | 직접 제작 (System.Drawing + 시스템 폰트) | 프로젝트 소유 | 2026-07-19 | 원본 | `Scripts/Create-SignTextures.ps1` | 한글 간판·포스터 |
| `/Game/Prototype/Textures/T_(Jangpan·Wallpaper·…)_{D,N,R}` | 직접 제작 (절차 생성) | 프로젝트 소유 | 2026-07-19 | 원본 | `Scripts/generate_surface_textures.py` | 사진 텍스처 폴백 |
| `Source/IndieGame/UI/Fonts/Pretendard-{Regular,SemiBold}.otf` | orioncactus / Pretendard 1.3.9 | SIL Open Font License 1.1 | 2026-08-12 | 원본 | `Source/IndieGame/UI/Fonts/OFL-Pretendard.txt` | 설정·대화·본문·보조문구. 패키지 실행 파일 옆 `UI/Fonts`에 NonUFS 스테이징 |
| `Source/IndieGame/UI/Fonts/GowunBatang-Bold.ttf` | Yanghee Ryu / Google Fonts | SIL Open Font License 1.1 | 2026-08-12 | 원본 | `Source/IndieGame/UI/Fonts/OFL-GowunBatang.txt` | 타이틀·장면 제목 전용. 패키지 실행 파일 옆 `UI/Fonts`에 NonUFS 스테이징 |

ambientCG 자료는 CC0 1.0(상업적 사용·수정·재배포 허용, 출처 표기 불요)이며, 원본 zip은 `Content/SourceArt/PhotoZips/`에 증빙으로 보관합니다.

두 한글 서체는 OFL 1.1에 따라 상업적 사용·수정·재배포가 가능하며 폰트 파일과
라이선스 원문을 함께 배포한다. Pretendard는 내비게이션과 긴 문장의 중립적
가독성을, 고운바탕 Bold는 「없는 층」 제목의 문학적 긴장만 담당한다. 의미 있는
본문을 생성 이미지에 굽지 않는 기존 원칙은 그대로 유지한다.

## 생성형 이미지(ImageGen) 아트워크

OpenAI ImageGen으로 생성하고 각 항목의 생성 방식과 날짜를 아래 기록에
남겼습니다. `Scripts/Prepare-AIArt.ps1`로 크롭·리샘플·미세문구 재작성을
진행하며, 원본은 `Content/SourceArt/AI/`에 그대로 보관합니다.

| 원본 | 산출 텍스처 | 내용 |
|---|---|---|
| `LabelWater_raw.png` | `T_LabelWater_D` | 새벽샘물 생수 라벨 (미세문구 재작성) |
| `LabelRamyeon_raw.png` | `T_LabelRamyeon_D` | 왕라면 컵라면 라벨 |
| `SignMain_raw.png` | `T_SignMain_D` | 새벽24 무영로점 파사드 간판 |
| `SheetSnacks.png` | `T_SnackShrimp_D` · `T_SnackPotato_D` · `T_SnackSquid_D` · `T_SnackCorn_D` | 과자 봉지 4종 (새우빵·감자스낵·오징어칩·콘스낵) |
| `SheetBottles.png` | `T_LabelGreenTea_D` · `T_LabelBarley_D` · `T_LabelSoda_D` · `T_LabelSoju_D` | 음료 라벨 4종 (산들녹차·구수한보리·톡소다·새벽이슬) |
| `SheetSigns.png` | `T_SignLaundry_D` · `T_SignHair_D` · `T_SignHof_D` · `T_SignSuper_D` · `T_SignPC_D` · `T_SignKaraoke_D` | 골목 상가 간판 6종 |
| `SheetPosters.png` | `T_PosterSale_D` · `T_PosterRamyeon_D` · `T_PosterFlyer_D` · `T_NoticeRent_D` | 편의점·골목 인쇄물 4종 |
| `SheetPaper.png` | 없음 | 초기 종이 시트로 분류했으나 실제 결과물이 주황색 새우 스낵 봉지 이미지였음. 원본 추적을 위해 보존하되 게임과 머티리얼에서는 **미사용** |
| `SheetPaperNotes_v2.png` | `T_PaperClean_V2_D` · `T_PaperWet_V2_D` · `T_PaperFolded_V2_D` · `T_PaperOld_V2_D` | CH02 문서용 빈 종이 4종(깨끗함·젖음·접힘·낡음). 한국어와 영수증 정보는 런타임 텍스트로 표시 |
| `SheetHorrorEvidenceMasks.png` | `T_EvidenceSlipperTrail_M` · `T_EvidenceCatPawTrail_M` · `T_EvidenceHoseDrag_M` · `T_EvidenceHandSmear_M` | P5 능동 대조용 젖은 흔적 마스크. 검정 바탕을 머티리얼의 불투명도·습윤 거칠기 입력으로 사용 |
| `SheetHorrorSurfaceBlends.png` | `T_DecalDampWallpaper_D` · `T_DecalRustFasteners_D` · `T_DecalMineralScale_D` · `T_DecalRainGrime_D` | CH03 벽지·계단실·탱크 환경 블렌드. 마젠타 키 제거 후 RGBA 마스크드 오버레이로 사용 |
| `SheetEvidenceProps.png` | 직접 텍스처로 사용하지 않음 | 뿔테 안경·점검봉·금 간 휴대폰·편의점 봉지의 통일된 형상/재질 기준. 앞의 세 소품을 `generate_meshes.py` 정적 메시로 재구성 |
| `SheetAlleyCatPoseReference.png` | 직접 텍스처로 사용하지 않음 | 동일한 고등어태비의 좌측 달리기·정면 3/4·정지·후면 3/4 비례 기준. `SM_AlleyCatRun` 정적 메시로 재구성 |
| `SheetFirstPersonSleeveReference.png` | 직접 텍스처로 사용하지 않음 | 36cm 왼쪽 후드 소매, 13.5cm 상완부→10.5cm 커프 테이퍼, 안쪽 아래팔의 검은 실 세 땀 기준. 손·피부 없이 `SM_FirstPersonHoodieSleeve` 정적 메시로 재구성 |
| `SheetFirstPersonKnockPhases_v1.png` · `v1_RGBA.png` · `SheetFirstPersonKnockPhases_v2.png` · `v2_RGBA.png` | `T_FPHandKnock0_D` · `T_FPHandKnock1_D` · `T_FPHandKnock2_D` · `T_FPHandKnock3_D` | M0 Q/B 두드리기의 같은 오른손·후드 소매 준비/예비/접촉/반동 4단계. v1은 화면 안 소매 절단면 때문에 증빙 전용, v2가 런타임 원본. UI-space 전용 RGBA이며 문·벽·인물·동물·배경을 평면으로 대체하지 않음 |
| `SheetListenerCaptureEmbracePhases_v1.png` · `v1_RGBA.png` | `T_FPCaptureEmbrace0_D` · `T_FPCaptureEmbrace1_D` · `T_FPCaptureEmbrace2_D` · `T_FPCaptureEmbrace3_D` | M1 포획의 같은 위층 사람 두 팔 접촉/접근/닫힘/유지 4단계. 건식 회색 석고와 낡은 옷을 기존 해부 기준에서 보존한 UI-space RGBA이며 얼굴·몸통·배경·충돌을 대체하지 않음 |
| `SheetP3ServiceCabinetReference.png` | 직접 텍스처로 사용하지 않음 | 270×196×18cm 열린 급수 서비스함, 18/12cm 밸브, 16cm 압력계, 수직 블리드 튜브와 래치의 빈 나사 구멍 정확히 두 개 기준. 정적 메시 5종으로 분리해 조작 상태를 유지 |
| `SheetRooftopFireDoorReference.png` | 직접 텍스처로 사용하지 않음 | 한국 빌라 옥상의 116×230×4.5cm 철문과 120×234cm 문틀, 하부 경첩 처짐·문턱 마찰 흔적 기준. `SM_RooftopFireDoorLeaf`와 `SM_RooftopFireDoorFrame`으로 재구성하고 11cm는 문 아래 높이가 아닌 자유단의 수평 열림으로 계산 |
| `SheetRooftopUnlockedPadlockKeysReference.png` | 직접 텍스처로 사용하지 않음 | 문틀 고리에 열린 채 걸린 50mm 적층 자물쇠, 삽입 열쇠 1개, 고리 1개, 추가 열쇠 정확히 3개와 무문자 금속 태그 기준. `SM_RooftopUnlockedPadlockKeys` 한 메시로 묶어 열쇠 획득·잠금 퍼즐·프롭 복제를 만들지 않음 |
| `TextureP3CabinetPaintedSteel.png` | `T_P3CabinetPaintedSteel_D` | 회녹색 도장 아연강판과 억제된 습기·잔흠집 알베도. `M_P3CabinetMetalUV`로 P3 서비스함 문짝과 외함에 적용 |
| `TextureAlleyCatTabby.png` | `T_AlleyCatTabby_D` | 고등어태비 단모와 좁은 줄무늬의 저채도 알베도. `M_AlleyCatTabbyUV`로 단일 풀 고양이에 적용 |
| `TextureWetHoodieFabric.png` | `T_WetHoodie_D` | 물에 잠긴 검정 면 후드 원단. 현재 소매와 탱크 복장의 동일 재질 계약에 사용 |
| `TextureCarrierBagFilm.png` | `T_CarrierBagFilm_D` | 가상 옅은 청색 무늬가 있는 편의점 LDPE 박막. CH01/CH02 운반 프롭과 CH03 사고 봉지에 같은 반투명 머티리얼 적용 |
| `SheetSubmergedBodyPoseReference.png` · `SheetSubmergedBodyAnatomyReference_v2.png` | 직접 텍스처로 사용하지 않음 | CH03 최종 리빌의 동일 인물 4시점 기준. v2는 얼굴·피부·상처를 노출하지 않으면서 후드 속 머리, 목·어깨, 위팔·팔꿈치·아래팔·가려진 손, 골반·허벅지·무릎·정강이·발목·슬리퍼가 이어지는 인체 실루엣을 고정한다. 후드·하의·슬리퍼는 공통 원점의 정적 메시 3종으로 재구성 |
| `TextureWaterTankGalvanized.png` | `T_WaterTankGalvanized_D` | 저채도 청회색 아연도금 강판과 습윤 흘러내림의 타일형 알베도. 탱크 외피·보강띠·배관·사다리·점검구에 `M_WaterTankMetalUV`로 공통 적용 |
| `TextureTankInteriorBiofilm.png` | `T_TankInteriorBiofilm_D` | 수면 아래 아연강판의 석회 침착·얇은 바이오필름·국부 녹을 억제된 알베도로 사용. 전용 N/R/A/W/M 채널과 `M_TankInteriorBiofilmUV`를 거쳐 비충돌 내부 라이닝 `SM_TankInternalLining`에만 적용한다. 월드 Z 561cm를 중심으로 8cm 습윤 전이를 두어 수중부와 35cm 공기층의 반사·거칠기를 분리한다 |
| `SheetRooftopWaterTankReference.png` | 직접 텍스처로 사용하지 않음 | 외경 306cm·높이 260cm의 16절 외피, 보강띠 3줄, 89/76mm 배관, 18cm 밸브와 내부 바닥·잔수 비례 기준. `SM_RooftopWaterTankShell`과 `SM_RooftopTankPipeCluster`로 재구성 |
| `SheetTankAccessSafetyHardwareReference.png` | 직접 텍스처로 사용하지 않음 | 열린 접근부의 42mm 상부 난간·U볼트 1개·걸린 안경 1쌍과 탱크 내부 7단 사다리의 형상·부착·수면 아래 판독 기준. `SM_TankAccessGuardRail`과 `SM_TankInternalLadder`로 재구성 |
| `SheetTankExteriorAccessStairReference.png` | 직접 텍스처로 사용하지 않음 | 20cm 단차·20cm 진행의 18단 45도 외부 계단, 오픈 트레드, 양측 스트링거·42mm 난간과 상단 두 번째 사고 단의 공간 관계 기준. 정확한 단 수와 좌표는 `SM_TankExteriorAccessStair` 절차 메시로 고정 |
| `SheetAccidentPropsReference.png` | 직접 텍스처로 사용하지 않음 | P5의 42mm EPDM 호스·65mm 커플링·2병용 LDPE 봉지 형상 기준. 호스 경로와 봉지는 절차 정적 메시로 재구성하고 병 수·수위는 런타임 상태로 유지 |
| `TextureWetServiceHoseRubber.png` | `T_WetServiceHose_D` | 젖은 검정 EPDM 보강 고무의 저대비 타일형 알베도. `M_WetServiceHoseUV`로 연속 호스 메시와 42mm 폴백에 공통 적용 |
| `SheetLadderRungFailureReference.png` | 직접 텍스처로 사용하지 않음 | P5 상단 두 번째 발판의 젖은 리브 고무·양 끝 고정 클립 2개·억제된 부식 디테일 기준. 과거 수직 발판 비례는 폐기하고 정확한 몸체 치수와 위치는 외부 계단 계약을 따른다 |
| `TextureWetRungPadRubber.png` | `T_WetRungPad_D` | 젖은 흑연색 세로 리브 고무의 저대비 타일형 알베도. `M_WetRungPadUV`로 들뜬 패드와 정확한 치수의 폴백에 공통 적용 |
| `TextureTankWaterSurface.png` | `T_TankWaterSurface_D` | 최종 리빌용 어두운 청회색 탱크 수면. 이미지에는 인체를 굽지 않고 낮은 물결·광도 변화·미세 광물 입자만 둔다. 알베도는 고정하고 전용 노멀을 서로 다른 배율·방향으로 두 번 천천히 이동시켜 `M_TankWaterReveal`의 반사와 약한 굴절만 변화시킨다 |
| 위 재질 스캔 10종의 `T_*_D` | `T_*_{N,R,A}` 30종 · `T_*_W` 6종 · 금속 마스크 2종 | `generate_ai_pbr_maps.py`로 생성하는 PBR 동반 채널 38종. 젖음은 알베도·거칠기·노멀 블렌드에 함께 사용하며 외부 녹과 내부 석회·바이오필름·녹 피복을 비금속으로 분리 |
| `ApplicationIcon_raw.png` | `Build/Windows/ApplicationIcon.png` · `Application.ico` | 물탱크 점검구·새벽빛 모티프의 Windows 배포 아이콘. `prepare_application_icon.py`로 1024px 등급 PNG와 16~256px 7단계 ICO를 생성 |
| `DialogueHUDConcept_v1.png` | UI 아트 디렉션 기준 이미지 | 실제 Shipping 캡처의 디버그형 대화창을 낮은 하단 점유율, 분리된 환경음 캡슐, 작은 화자 태그와 습기 낀 smoked-glass 재질로 재설계한 시안. 런타임 텍스트를 굽지 않고 색·여백·질감 기준만 사용 |
| `TextureHudDialogueFilm.png` | `T_HudDialogueFilm_D` | 대화창 표면의 저대비 charcoal/oxidized-green 미세 필름 스캔. UI 그룹·NoMipmaps·비스트리밍으로 임포트하고 런타임 둥근 마스크 안에서 낮은 알파로만 사용 |
| `TextureMissingFloorJournalPaper_v1.png` | `T_MissingFloorJournalPaper_D` | 「듣는 것들」 전체 화면의 무문자 장부 종이 표면. 16:9 정면 스캔 질감만 쓰고 모든 한글·출처 카드·썸네일·교차선은 런타임이 그린다. UI 그룹·NoMipmaps·Clamp·비스트리밍으로 임포트 |
| `TextureAudioCalibrationWall_v1.png` | `T_AudioCalibrationWall_D` | 첫 실행 소리·밝기 보정판의 무문자 청흑색 빌라 벽면. 저대비 광물 결만 낮은 알파로 사용하고 한글·눈금·암부 계조는 전부 런타임 HUD가 그린다. UI 그룹·NoMipmaps·Clamp·비스트리밍으로 임포트 |
| `ApartmentVisualTarget_v1.png` | 원룸 비주얼 아트 디렉션 기준 이미지 | 실제 Shipping 원룸 캡처의 카메라·동선·가구·HUD는 유지하고, 주황 스탠드와 청록 새벽광, 낡은 벽지·장판의 물성, 국부 습기 흔적만 보강한 목표 시안. 런타임 텍스처로 직접 사용하지 않음 |
| `TextureApartmentWallpaperVintage.png` | `T_ApartmentWallpaperV2_D` | 2000년대 초 한국 빌라의 저가 아이보리 엠보싱 벽지 알베도. 전용 N/R/A 채널과 `M_Wallpaper_X/Y/Ceil`에 연결 |
| `MaskApartmentWallPatina.png` | `T_ApartmentWallPatina_M` | 원룸 하부 모서리에 제한한 습기·들뜸 마스크. `M_ApartmentWallPatina`의 불투명도·색·거칠기 변화에 사용하고 충돌 없는 근거리 평면으로 배치 |
| `SheetMissingFloorEnvironmentReference.png` | 직접 텍스처로 사용하지 않음 | 「없는 층」의 같은 빌라 외관·4F 복도·불법 5층·옥상 통로의 카메라·재료·노출 기준. 실제 충돌 지오메트리는 코드가 소유 |
| `SheetListenerEntityAnatomyReference.png` | `SM_ListenerEntityCrawl` | 위층 사람의 정·측·상·3/4 인체 연결 기준. 얼굴·피부·고어 없이 연속 3D 메시와 건식 석고 PBR로 재구성 |
| `ListenerEntityFrontCutout.png` | `T_SpriteListenerFront_{D,N,R,A}` · `M_SpriteListenerFront` | 위층 사람의 정면 복도 판독용 PBR 레이어. 1.6m에서 켜고 활성화 뒤 1.25m까지 유지하며, 3D 셸·그림자는 계속 보존하고 측면에서는 숨김 |
| `SheetListenerEntityCrawlPhases.png` | `T_SpriteListenerCrawl0..3_{D,N,R,A}` · `M_SpriteListenerCrawl0..3` | 같은 인물의 좌우 팔꿈치 지지·중앙 지지·회복 네 자세. 속도 연동 1.6~6fps, 정지 프레임 유지, 응답 노크 대기 시 중앙 자세 고정 |
| `TextureMissingFloorDryPlaster.png` | `T_MissingFloorDryPlaster_{D,N,R,A}` | 불법 5층과 존재의 건식 석고 표면. 조명 없는 타일 알베도에서 PBR 동반 채널 생성 |
| `SheetMissingFloorResidueMasks.png` | `T_MissingFloorHandprints_M` · `T_MissingFloorDragTrails_M` · `T_MissingFloorDustJoint_M` · `T_MissingFloorCavityScratches_M` | 손자국·끌림·분진 이음·공동 긁힘 값 마스크. 실표면에 masked 블렌드 |
| `SheetMissingFloorDistantCharacters.png` | `T_SpriteSeo_D` · `T_SpriteMok_D` · `T_SpriteHwang_D` · `T_SpriteNarin_D` | 접근 불가 12m 이상 고정 컷용 RGBA 인물. 현재 서일영만 런타임 배치, 나머지는 근접 대용 방지를 위해 미배치 |
| `SheetMissingFloorHeroPropsReference.png` | `SM_TuningHammer` · `SM_TunerToolCart` · `SM_ComplaintLedger` · `SM_CalendarJournal` | 조율 렌치·공구 카트·민원 원장·달력 일지의 실제 두께·접지·시차를 가진 3D 프롭 기준 |
| `TextureVillaStairCheckerPlatePaintedSteel.png` | 미사용 (증빙 보존) | v1. 계약은 전부 통과했으나 손전등 프레임에서 디딤판이 매끈한 판으로 보여 폐기. 계조 범위가 11%뿐이라 게인으로도 살아나지 않았다 |
| `TextureVillaStairCheckerPlatePaintedSteel_v2.png` | `T_MissingFloorSteelStair_{D,N,R,A}` | 도장 체커플레이트 철제 계단 디딤판. `Footstep.MetalStair` 표면이 복도 콘크리트로 그려지던 것을 교체한다. 55cm 타일에 다이아몬드 16개(피치 34mm), 거칠기 0.68로 콘크리트(0.9+)와 손전등 반사가 갈린다 |
| `TextureRooftopUrethaneWaterproofing.png` | `T_RooftopWaterproofing_{D,N,R,A}` | 옥상 녹색 우레탄 방수 도막. `Footstep.Rooftop` 전용이며 1.5m 타일. 물 고임 자국은 이미지에 굽지 않고 균일 분포로만 둔다 |
| `TextureRooftopAnnexConcreteGypsumDebris.png` | `T_MissingFloorGypsumDebris_{D,N,R,A}` | 5층 슬래브의 석고 파편. `Footstep.GypsumDebris` 전용. 발자국·끌림은 `UIGSettledDustComponent`가 런타임 ISM으로 그리므로 굽지 않는다 |
| `TextureUtilityMeterDialFaceBlank.png` | `T_UtilityMeterDial_{D,N,R,A}` | P1 계량기 문자판. 눈금·붉은 호·스핀들 보스만 담고 드럼 창은 비어 있다. 지침과 「다섯 번째가 돌지 않는다」는 `FifthMeterDisc`가 소유한다 |
| `TextureComplaintLedgerCarbonPaperBlank.png` | `T_CarbonPaper_{D,N,R,A}` | P2 먹지. 왁스 안료가 눌린 자리에서 얇아지는 광택 차이만 담는다. 눌린 원문 한글은 굽지 않는다. A4 비율 724×1024로 원장 메시에 UV 매핑 |
| `TextureApartmentEntranceDoorCharcoalSteel.png` | `T_UnitDoorPaintedSteel_{D,N,R,A}` | 세대 현관문 문짝의 무광 도장 강판. 브러시드 스테인리스를 대체하며 밴드·인레이·레버·도어록·도어스코프는 기존 3D 기하를 유지한다. 발치 마모는 타일이 아니라 별도 masked 평면 |
| `SheetVillaCorridorFixturesReference.png` | 직접 텍스처로 사용하지 않음 | 세대 현관문·우편함 3x3·소화전함·천장 LED 등의 비례·재질 기준. Blender 절차 메시로 재구성 (2026-09-04) |
| `SheetOneroomKitchenAppliancesReference.png` | 직접 텍스처로 사용하지 않음 | 소형 냉장고·빌트인 주방·전자레인지·인터폰 기준. `SM_FridgeBody·Door`, 주방 7종으로 재구성 |
| `SheetStoreFixturesReference.png` | 직접 텍스처로 사용하지 않음 | 편의점 음료 냉장고·곤돌라·평대 냉동고 기준. `build_store_fixtures.py` 7종으로 재구성 (2026-09-08) |
| `SheetApplianceControlPanels.png` | `panels/{DoorLockKeypad,MicrowavePanel,WasherPanel,IntercomFace}.png` | 도어락 키패드·전자레인지·세탁기·인터폰 앞면 정면 텍스처. 세탁기·전자레인지 메시에 `image_quad`로 붙여 굽는다. 한글은 헤드라인급이라 검수만 했다 |

## 아틀라스·LOD·물리 배치 계약

인쇄 아트 51장은 `Content/SourceArt/Atlas/`의 공유 페이지로 묶고, 절차
메시와 스캔 프롭은 등급별 삼각형 예산과 저작 LOD 체인을 받는다. 코드로
배치한 월드 지오메트리는 물리적으로 불가능한 배치가 없는지 검사를 통과해야
한다. 세 계약의 수치·검사·실행 순서는 `ASSET_OPTIMISATION.md`에 있다.

한 번의 생성에 5분이 걸리므로 낱장 대신 **격자 시트**로 묶어 뽑고 슬라이스합니다.
현재 134장의 파생 텍스처(기존 85장 + 없는 층 PBR·마스크·디테일 48장 + 최초 실행 보정 배경 1장)와
증거·생물·설비·인체·사고 프롭 기준 시트를 관리합니다. 2026-08-06
추가분은 생성 실패를 그대로 채택하지 않고 슬리퍼 밑창과 빗물 때를 각각
한 차례 수정 생성했습니다.

운용 규칙:

1. **가상 브랜드만.** 프롬프트에 실존 상표·로고·브랜드 색 조합을 요구하지 않으며,
   생성물에 실존으로 보이는 이름이 섞여 들어오면 `Prepare-AIArt.ps1`의 패치
   단계에서 덮어쓴다. 새벽샘물 라벨의 제조원/판매원·본문 카피가 그 사례다.
2. **헤드라인 한글은 검수, 본문 한글은 재작성.** ImageGen은 짧은 한글 제목은
   정확히 렌더하지만 본문은 지어낸다. 제목은 눈으로 확인하고, 의미가 있는
   본문은 Malgun Gothic으로 직접 다시 그린다.
3. **원본을 지우지 않는다.** `Content/SourceArt/AI/`의 `*_raw.png`는 어떤 픽셀이
   생성물이고 어떤 픽셀이 우리 것인지 추적하기 위한 증빙이다.
4. 생성형 이미지의 저작권 지위는 관할에 따라 다르므로, 배포 전 이 표의 항목을
   다시 검토한다.
5. **정사 물증을 그림으로만 고정하지 않는다.** 흔적은 마스크드 평면과
   거칠기 변화로, 안경·점검봉·휴대폰은 실제 정적 메시와 런타임 상태로
   구현한다. 기준 시트는 형상·마모 참고이며 카메라에 직접 노출하지 않는다.

### 원룸 비주얼 패스 생성 기록

- 서비스: OpenAI ImageGen 내장 도구
- 생성일: 2026-08-06
- 보존 원본: `Content/SourceArt/AI/ApartmentVisualTarget_v1.png`,
  `TextureApartmentWallpaperVintage.png`, `MaskApartmentWallPatina.png`
- SHA-256: 목표 시안
  `01562DEE4BCD53C9A44663AEADB81BAB5B582FABD26D82071BBE91FDFB55CBA5`,
  벽지 `464803BCFE96602303AF292E03FC5308C46C972EC9249B6C78CFAB5CFA91BB03`,
  파티나 `1C8D8B6E1F9849DE838A67BC3D712E71B6ECA5F39D2412A815377BDC04773D17`
- 목표 시안 프롬프트: 실제 Shipping 원룸의 카메라·기하·동선·소품과 HUD를
  보존하고, 따뜻한 텅스텐 조명과 차가운 새벽광, 오래된 벽지·장판의 미세
  물성만 보강한다. 괴물·고어·새 가구·새 출입구는 추가하지 않는다.
- 재질 프롬프트: 벽지는 조명과 그림자를 굽지 않은 이음매 없는 아이보리
  엠보싱 알베도, 파티나는 검정 무손상·흰색 손상의 그레이스케일 마스크로
  생성한다. 의미 있는 문자·상표·인물은 포함하지 않는다.
- 적용: `Build-ArtAssets.ps1 -ApartmentVisualOnly`가 5개 텍스처와 4개
  머티리얼을 빌드·감사한다. 파티나는 두 곳에만 국부 배치하고 9.5m에서
  컬링하며, 조명·상호작용·충돌·서사 상태에는 영향을 주지 않는다.

### Windows 배포 아이콘 생성 기록

- 서비스: OpenAI ImageGen built-in
- 생성일: 2026-08-05
- 보존 원본: `Content/SourceArt/AI/ApplicationIcon_raw.png`
- 파생: `Build/Windows/ApplicationIcon.png`, `Build/Windows/Application.ico`
- 처리: `Scripts/prepare_application_icon.py`가 정사각 크롭, 중간톤 감마,
  대비·채도·샤프닝을 고정값으로 적용하고 16/24/32/48/64/128/256px
  ICO 디렉터리를 만든다.
- 검수: 최종 32px 레벨을 최근접 확대해 원형 점검구 실루엣, 검은 내부와
  하단의 억제된 적색 반사가 남는지 확인했다.
- 전체 프롬프트: `Docs/IMAGEGEN_PROMPTS_2026-08-05.md`

### CH03 흔적·환경·증거 소품 생성 기록

- 서비스: OpenAI ImageGen
- 생성일: 2026-08-03
- 보존 원본: `Content/SourceArt/AI/SheetHorrorEvidenceMasks.png`,
  `SheetHorrorSurfaceBlends.png`, `SheetEvidenceProps.png`
- 파생: 흔적 마스크 4장, 환경 오버레이 4장, 절차 정적 메시 3종
- 적용: `Scripts/Build-ArtAssets.ps1`이 소스 분리, PBR 파생, 텍스처 임포트,
  마스크드 머티리얼 생성과 Geometry Script 메시 베이크를 순서대로 수행한다.
  마지막 UAsset 감사에서 LOD·해상도·압축·재질 입력과 텍스처 연결을 검사한다.
- 런타임: CH03 디렉터가 전용 에셋을 우선 로드하고, 아직 베이크되지 않은
  개발 환경에서는 기존 정적 프록시로만 폴백한다. 에셋 누락이 진행을 막거나
  기본 머티리얼 사각형을 노출하지 않는다.
- 전체 생성·수정 프롬프트: `Docs/IMAGEGEN_PROMPTS_2026-08-03.md`

### 골목 고양이·복장·봉지 재질 생성 기록

- 서비스: OpenAI ImageGen
- 생성일: 2026-08-03
- 보존 원본: `SheetAlleyCatPoseReference.png`, `TextureAlleyCatTabby.png`,
  `TextureWetHoodieFabric.png`, `TextureCarrierBagFilm.png`
- 파생: 1024 알베도 3장, Geometry Script 정적 고양이 1종
- 적용: 고양이는 0.9~1.35초 풀 이벤트용 단일 정적 메시로 사용하고,
  애니메이션 파이프라인을 추가하지 않는다. 후드와 봉지는 각각 CH03 신원
  대조와 CH01~CH03 소지품 연속성에 동일 머티리얼을 공유한다.
- 폴백: 메시·머티리얼 미베이크 환경에서는 기존 여섯 도형 고양이,
  침구 원단, 유리 봉지 프록시를 유지해 진행을 차단하지 않는다.

### 1인칭 착의 소매 생성 기록

- 서비스: OpenAI ImageGen
- 생성일: 2026-08-03
- 보존 원본: `Content/SourceArt/AI/SheetFirstPersonSleeveReference.png`
- 파생: `SM_FirstPersonHoodieSleeve` Geometry Script 정적 메시 1종
- 적용: 약 36cm 길이, 상단 13.5cm에서 10.5cm 커프로 좁아지는 왼쪽
  소매를 완만히 굽히고 `M_WetHoodieUV`를 탱크 착의와 공유한다. 검은 실
  세 땀은 텍스처에 굽지 않고 소매에 부착된 별도 기하로 유지한다.
- 연출 계약: 손·손목·피부를 만들지 않으며 기존 1.2초 비차단 제시와 78도
  수평 시야각을 유지한다. 확대·스포트라이트·입력 잠금으로 물증을 강요하지 않는다.
- 폴백: 정식 메시가 없을 때만 기존 원기둥 소매와 동일한 세 땀 기하를 함께
  사용한다. 정식 치수와 원기둥용 축척을 섞지 않는다.

### P3 급수 서비스함 생성 기록

- 서비스: OpenAI ImageGen
- 생성일: 2026-08-03
- 보존 원본: `Content/SourceArt/AI/SheetP3ServiceCabinetReference.png`,
  `Content/SourceArt/AI/TextureP3CabinetPaintedSteel.png`
- 파생: `T_P3CabinetPaintedSteel_D`, `M_P3CabinetMetalUV`,
  `SM_P3ServiceCabinetShell`, `SM_P3ServiceManifold`,
  `SM_P3ValveWheelLarge`, `SM_P3ValveWheelSmall`, `SM_P3PressureGauge`
- 적용: 270×196×18cm 열린 외함과 34/25mm 배관, 18/12cm 오륜
  손잡이, 16cm 계기를 실제 치수로 분리했다. 압력 숫자·바늘·블리드 수위와
  네 조작 상태는 이미지에 굽지 않고 기존 런타임 상태를 그대로 사용한다.
- 물증: 굽은 래치 옆 빈 나사 구멍은 정확히 두 개의 별도 어두운 기하로
  유지한다. 텍스처의 우연한 점이나 부식 자국을 나사 구멍으로 세지 않는다.
- 폴백: 다섯 메시 중 하나라도 없으면 기존 외함·배관·원기둥 조작부 전체로
  전환한다. 정식/프록시 부품을 섞거나 조작 좌표와 저장 경계를 바꾸지 않는다.

### CH03 옥상 철문 생성 기록

- 서비스: OpenAI ImageGen 내장 도구
- 생성일: 2026-08-03
- 보존 원본: `Content/SourceArt/AI/SheetRooftopFireDoorReference.png`
- 파생: `SM_RooftopFireDoorLeaf`, `SM_RooftopFireDoorFrame` Geometry Script
  정적 메시 2종
- 적용: 문짝을 116×230×4.5cm 접이강판으로 고치고 120×234cm 유효
  개구부, 3개 힌지, 문턱 마찰판, 손잡이와 도어클로저를 한 세계관의 생활
  설비로 구성한다. 걸림 상태는 힌지 축을 보존한 5.441396도 회전이며
  자유단 중심이 닫힘 평면에서 수평 11cm 떨어진 값으로 실측한다.
- 충돌: 바닥 아래 전체가 11cm 뜨는 이전 프록시를 폐기했다. 7cm 지름의
  고양이 캡슐은 자유단과 문설주 사이 곡선 경로로 왕복하고, 사람 캡슐은
  중앙에서 문을 당기기 전 차단된다. 문설주 정지 볼륨은 시각 문틀과 별개로
  좁은 우회로를 막는다.
- 폴백: 메시가 하나라도 없으면 같은 4.5cm 문짝과 120×234cm 문틀 프록시를
  사용한다. 11cm 측정식, 세 상태, 열쇠 조사 위치와 충돌 경로는 바꾸지 않는다.

### CH03 열린 자물쇠·관리 열쇠 생성 기록

- 서비스: OpenAI ImageGen 내장 도구
- 생성일: 2026-08-03
- 보존 원본:
  `Content/SourceArt/AI/SheetRooftopUnlockedPadlockKeysReference.png`
- 참조 원본: `Content/SourceArt/AI/SheetRooftopFireDoorReference.png`를
  재질·습도·마모 일관성 기준으로만 사용
- 파생: `SM_RooftopUnlockedPadlockKeys` Geometry Script 정적 메시 1종
- 적용: 50×28×62mm 적층 자물쇠와 8mm 열린 걸쇠, 삽입된 관리 열쇠 1개,
  42mm 고리 1개, 추가 열쇠 정확히 3개와 무문자 금속 태그를 한 메시로
  구성한다. 자물쇠는 문짝을 잠그지 않고 문틀 고리에 열린 채 걸려 있다.
- 상호작용: “확인하기”만 제공하며 획득·인벤토리·잠금 해제 동작을 만들지
  않는다. 조사 뒤에도 옥상문 입력은 열쇠 사용이 아니라 손잡이 당기기다.
- 폴백: 미베이크 환경은 작은 자물쇠 몸체와 열린 걸쇠·삽입 줄기·열쇠
  3개를 같은 좌표에 구성한다. 프롭 수와 문 상태는 바뀌지 않는다.

### CH03 수중 인체 포즈 생성 기록

- 서비스: OpenAI ImageGen
- 생성일: 2026-08-03, 인체 실루엣 개정 2026-08-07
- 보존 원본: `Content/SourceArt/AI/SheetSubmergedBodyPoseReference.png`,
  `Content/SourceArt/AI/SheetSubmergedBodyAnatomyReference_v2.png`
- 파생: `SM_SubmergedHoodieCurl`, `SM_SubmergedPantsCurl`,
  `SM_SubmergedSlippersCurl` Geometry Script 정적 메시 3종
- 적용: 세 메시가 같은 로컬 원점을 사용하며 CH03 디렉터가 점검구 바로 아래
  하나의 공유 변환에 겹쳐 젖은 후드·검은 하의·검은 슬리퍼 재질을 분리
  적용한다. 후드는 탱크 수중 전용 타일링·습윤 하한을 가진
  `M_SubmergedHoodieUV`, 하의는 기존 젖은 후드 원단의 PBR 채널을 타일링한
  `M_SubmergedPantsUV`, 슬리퍼는 젖은 EPDM
  고무 채널을 재해석한 `M_SubmergedSlippersUV`, 뒤꿈치 마모와 세 줄은
  리브 고무 채널의 `M_SubmergedSlipperWearUV`를 사용한다. 세 재질은 습윤도
  하한을 0.82~0.90으로 고정해 수중에서 건조한 프록시처럼 보이지 않으며,
  별도 생성 이미지를 늘리지 않고 승인된 ImageGen 원본의 물성 채널을
  재사용한다. 얼굴과 피부는 메시 자체에 만들지 않되 후드 속 머리와 목 전이,
  어깨선, 분리된 팔꿈치·가려진 손, 골반·무릎·정강이·발목의 연결과 팔다리
  사이 음영 공간을 실제 기하로 만든다. 세 땀·왼발 뒤꿈치
  마모·슬리퍼 세 줄은 별도 기하로 유지한다. 신원 조사 볼륨은 별도 좌표를
  복사하지 않고 이 공유 인체 변환과 같은 소매 로컬 오프셋에서 계산하므로,
  포즈나 배치를 바꿔도 보이는 세 땀과 상호작용 지점이 함께 이동한다.
- 폴백: 세 메시 중 하나라도 미베이크면 기존 구·원통 그레이박스 전체로만
  전환한다. 그레이박스도 동일한 세 전용 재질을 사용하며, 개별 머티리얼이
  없을 때에만 침구·플라스틱·젖은 종이 재질로 되돌아간다. 정식 그룹과
  그레이박스가 겹쳐 보이지 않는다.

### CH03 물탱크 금속·점검구 생성 기록

- 서비스: OpenAI ImageGen
- 생성일: 2026-08-03
- 보존 원본: `Content/SourceArt/AI/TextureWaterTankGalvanized.png`,
  `Content/SourceArt/AI/SheetRooftopWaterTankReference.png`,
  `Content/SourceArt/AI/SheetTankAccessSafetyHardwareReference.png`
- 파생: `T_WaterTankGalvanized_D`, `M_WaterTankMetalUV`,
  `SM_RooftopWaterTankShell`, `SM_RooftopTankPipeCluster`,
  `SM_TankInternalLadder`, `SM_TankAccessGuardRail`, `SM_TankAccessDeck`,
  `SM_TankAccessLid`
- 적용: 외경 306cm·높이 260cm의 속 빈 16절 외피와 연속 보강띠 3줄,
  외경 89/76mm 배관, 18cm 밸브, 사다리·상판·점검구가 같은 금속 재질을
  공유한다. 내부 바닥 Z 381cm, 점검구 하단 Z 596cm, 수면 Z 561cm로
  고정해 내부 높이 2.15m·잔수 1.80m·상부 여유 35cm를 동시에 만족한다.
  내부에는 46cm 레일 간격·7단·30cm 간격 사다리를 실제로 두고, 외부
  상부 발판에는 접근부가 열린 양측 난간과 U볼트 1개를 둔다. 검은 뿔테
  안경은 옥상 바닥 프롭이 아니라 그 U볼트에 한쪽 안경다리로 걸린 동일
  조사 액터다.
  사람이 들 수 없던 3.1m 전체 상판은 지름 104cm 개구부와 지름 108cm
  1인 점검구로 교체한다. 개구부 중심은 탱크 중심에서 서쪽 서비스면으로
  96cm 치우치며, 폭 87cm 상부 착지부는 개구부 서쪽 테두리에서 끝난다.
  상단 두 번째 단에서 테두리까지 약 107cm가 되어 04:44의 발·골반·상체
  자세가 한 사람의 도달 범위 안에 들어온다. 내부 7단 사다리도 같은 개구부
  아래 서쪽 내벽으로 이동한다. 엔딩의 동일 뚜껑 인스턴스 복원 계약은 유지한다.
- 폴백: 미베이크 환경은 같은 외경의 16개 충돌 패널, 현실 치수 배관,
  네 장의 상판 프록시와 원기둥 점검구를 쓴다. 정식 외피가 로드되면 패널은
  충돌만 남기고 숨겨 이중 실루엣을 만들지 않는다. 진행·상호작용 위치는 같다.

### P5 호스·커플링·봉지 생성 기록

- 서비스: OpenAI ImageGen
- 생성일: 2026-08-03
- 보존 원본: `SheetAccidentPropsReference.png`,
  `TextureWetServiceHoseRubber.png`
- 파생: `T_WetServiceHose_D`, `M_WetServiceHoseUV`,
  `SM_RooftopServiceHose`, `SM_HoseCoupling`, `SM_CarrierBagCollapsed`
- 적용: 14cm 원통 다섯 개를 42mm 연속 EPDM 호스로 교체하고 상단에
  65mm 아연도금 커플링을 별도 배치한다. 봉지는 한 개의 얇은 열린 메시를
  A/B/C 구매 크기에 맞춰 비균일 스케일하며 병 수·수위·마개는 기존 런타임
  상태 프롭을 그대로 사용한다.
- 폴백: 미베이크 환경도 42mm 원통만 사용하고, 봉지는 기존 얇은 벽과
  손잡이 조합을 유지한다. 정식 봉지 메시가 로드되면 기존 벽을 함께 만들지
  않아 반투명 외피가 중복되지 않는다.

### P5 상단 발판 파손 생성 기록

- 서비스: OpenAI ImageGen
- 생성일: 2026-08-03
- 보존 원본: `SheetTankExteriorAccessStairReference.png`,
  `SheetLadderRungFailureReference.png`,
  `TextureWetRungPadRubber.png`
- 파생: `T_WetRungPad_D`, `M_WetRungPadUV`,
  `SM_TankExteriorAccessStair`,
  `SM_LadderFailureRung`, `SM_LadderRungPadLifted`,
  `SM_LadderRungRetainingClips`
- 적용: 옥상 바닥부터 상판까지 20cm 진행·20cm 단차의 18단 오픈 계단을
  실제 이동 충돌과 같은 원점에 둔다. 상단 두 번째인 16번 단만 20×105×3cm
  금속 몸체, 54×8.5×0.3cm 리브 고무 패드, 양 끝 고정 클립 2개로 분리한다.
  패드의 안쪽 긴 변만 8~10mm 들뜨며 젖은 슬리퍼 전이 흔적은 패드 위 조사
  평면으로 유지한다.
- 연결: 계단 끝의 착지부는 월드 X 2265~2352cm만 차지하고, 서쪽으로
  96cm 치우친 지름 104cm 점검구의 바깥 테두리 X 2352cm에서 정확히 끝난다.
  정중앙 점검구나 탱크 안으로 58cm 파고드는 착지부로 되돌리지 않는다.
- 폴백: 네 메시 중 하나라도 없으면 정식 그룹 전체를 쓰지 않고 18개 충돌
  단과 같은 대각선 난간, 정확한 치수의 얇은 패드 프록시로 전환한다. 흔적 조사 평면은 어느
  경로에서도 발판 안에 파묻히지 않으며, 주황색 전체 클립 대신 작은 부식
  나사 머리 두 개만 보인다.

### CH03 탱크 수면·굴절 생성 기록

- 서비스: OpenAI ImageGen
- 생성일: 2026-08-03
- 보존 원본: `Content/SourceArt/AI/TextureTankWaterSurface.png`
- 파생: `T_TankWaterSurface_D`, `M_TankWaterReveal`
- 적용: 기존 불투명 청색 블록을 270cm 단일 수평 평면으로 교체한다.
  텍스처의 낮은 광도 변화는 약 1.006~1.018 범위의 약한 굴절에 사용하고,
  투명도는 약 0.25 부근으로 제한해 손전등이 닿기 전 인체를 설명하지 않는다.
- 렌더링: 평면은 그림자를 드리우지 않고 투명 정렬 우선순위 2를 사용한다.
  큐브 폴백은 평면 메시가 없는 경우에만 남기며, 전용 머티리얼 미베이크
  환경에서는 기존 `M_WaterBlue`로 진행과 리빌 타이밍을 유지한다.

### CH02 공동현관 제물 물그릇 생성 기록

- 제작: 프로젝트 소유 Geometry Script 절차 메시
- 생성일: 2026-08-05
- 파생: `SM_OfferingWaterBowl` 정적 메시 1종
- 형상: 외경 24.4cm·높이 8.7cm의 얕은 스테인리스 그릇이다. 48분할 회전체
  프로파일로 무게 받침, 벌어진 측벽, 말린 림과 실제 열린 내부를 만들고,
  충돌 없는 증거 프롭으로 베이크한다. 막힌 원기둥 프록시는 정식 베이크가
  없을 때만 사용한다.
- 수면: 별도 수평 수면을 내부 Z 7.72cm에 배치하고 그림자를 끈다. 두 개의
  끊긴 비발광 반사만 더해 어두운 복도에서도 물로 읽히되 상호작용 표식처럼
  빛나지 않게 한다.
- 주변 흔적: 쌀·숟가락·향을 제거한 CH02 상태에서 물그릇은 끊긴 마른
  공양그릇 자국 밖에 놓인다. 쓸린 소금은 대칭 띠나 돌무더기가 아니라 높이
  0.5cm 미만의 서로 다른 크기 8덩이와 낱알로 구성하고, 보행과 닦인 흔적이
  세 곳에서 원형 자국을 끊는다. 물그릇·소금·마른 자국은 모두 비충돌이다.

### CH02 종이 시트 생성 기록

- 서비스: OpenAI ImageGen
- 생성일: 2026-07-26
- 보존 원본: `Content/SourceArt/AI/SheetPaperNotes_v2.png`
- 원본 크기: 1254×1254
- 파생 파일:
  - `Content/Prototype/Textures/T_PaperClean_V2_D.uasset`
  - `Content/Prototype/Textures/T_PaperWet_V2_D.uasset`
  - `Content/Prototype/Textures/T_PaperFolded_V2_D.uasset`
  - `Content/Prototype/Textures/T_PaperOld_V2_D.uasset`
- 적용: `Scripts/Prepare-AIArt.ps1`이 2×2 시트를 네 장으로 분리하고,
  `Scripts/create_textured_materials.py`가 `M_PaperClean`, `M_PaperWet`,
  `M_PaperFolded`, `M_PaperOld` 머티리얼에 V2 텍스처를 연결합니다.
- 이전 `Content/SourceArt/AI/SheetPaper.png`와 V1 파생 텍스처는 삭제하지
  않고 증빙을 위해 보존하지만, 현재 머티리얼에서는 사용하지 않습니다.

사용한 프롬프트 전문:

```text
[공통 스타일 규칙 - 반드시 지켜줘] 이건 사실적인 PBR 렌더링 기반 1인칭 공포게임에 쓸 종이 텍스처야. 여러 장을 따로 만들어도 전부 같은 세계에 있는 것처럼 보여야 해. 실제 종이를 스튜디오에서 완전 정면으로 촬영한 팩샷 사진처럼 만들어줘. 대형 소프트박스 조명, 그림자 거의 없음, 원근 왜곡 없이 완전 정면 평면. 종이의 섬유결, 눌림, 습기와 세월의 물성이 섬세하게 보여야 해. 배경은 순백색이고 색은 채도를 낮춘 실제 재료 톤이어야 해. 실존 상표나 로고는 절대 금지. 이번 이미지는 Unreal Engine 5 생활공포게임의 문서 배경용 원본 시트다. 전체 캔버스는 정확한 1:1 정사각형, 2열×2행의 엄격한 격자. 각 칸 사이는 굵고 깨끗한 흰 여백으로 완전히 분리하고, 네 칸의 조명·카메라·그레인·노출은 완전히 동일하게 유지한다. 각 칸 중앙에 세로 A4 비율(1:1.414)의 빈 종이 한 장을 완전 정면으로 놓고 셀 높이의 약 90%를 채운다. 좌상단: 깨끗하지만 아주 약간 따뜻한 흰색 복사용지, 미세한 종이 섬유만. 우상단: 아래 가장자리에서 번진 현실적인 물얼룩과 옅은 조수선, 살짝 물결친 가장자리의 젖은 종이. 좌하단: 한 번 세로로, 한 번 가로로 접었다 펼친 부드러운 십자 접힘 자국이 있는 종이. 우하단: 누렇게 바랜 낡은 종이, 모서리 마모와 아주 약한 갈색 반점, 찢어지거나 구멍 나지 않음. 네 종이 모두 완전히 비어 있어야 한다. 글자, 숫자, 도장, 선, 그림, 로고, 워터마크, 테이프, 클립, 손, 소품을 절대 넣지 말 것. 종이 밖 배경에는 그림자, 그라데이션, 바닥면, 반사, 질감이 없어야 한다. 게임에서 한국어 문자를 런타임으로 올릴 예정이므로 넓고 깨끗한 중앙 여백을 유지한다.
```

### 「없는 층」 환경·인물·흔적·핵심 소품 생성 기록

- 서비스: OpenAI ImageGen 내장 도구
- 생성일: 2026-08-10
- 보존 원본: `SheetMissingFloorEnvironmentReference.png`,
  `SheetListenerEntityAnatomyReference.png`, `TextureMissingFloorDryPlaster.png`,
  `SheetMissingFloorResidueMasks.png`, `SheetMissingFloorDistantCharacters.png`,
  `SheetMissingFloorHeroPropsReference.png`, `ListenerEntityFrontCutout.png`,
  `SheetListenerEntityCrawlPhases.png`
- SHA-256:
  - 환경 `041295A5CAC88B7180B58E8E0106DC0C62282B61FE48F9DFD2D3F6A912729AD8`
  - 인체 `0C327F322224173879F1D19872A4F79F5CE725295155BBB60C337295B50823D7`
  - 석고 `35D842AF7D9BE620E9CB22D10A8D60E2A2189CED72283B2EF848226C199DFDC3`
  - 흔적 `201B650D2B052F26F8AE8961D17A681C249721E99CF01862B2922F3817FDA4D7`
  - 인물 `0E8879B58588798184FCFC406348154E2A3AB33E5D88E26259A40DFC348633C1`
  - 소품 `DEB5620CBDAC8C0CCE6EC782613077A76DB6EFFCB4522E380E1B48E08869DC84`
  - 정면 인체 `80B4CDD471BD163E424E5291C0EDD03B4A0E9E9D376C4BD330FFF4FA47A50338`
  - 기어오기 4단계 `C8DAE7F12E5C047ED277BC556E4043F2052321359ED628CE534267C2F35CAD8A`
- 처리: `Prepare-AIArt.ps1`이 흔적·인물·동작 시트를 분리하고 마젠타/초록 키를
  알파로 변환한다. `generate_ai_pbr_maps.py`가 석고와 정면 인체 5장 각각의
  N/R/A를 만들며,
  `generate_meshes.py`는 인체와 네 핵심 소품을 실제 치수의 3D 메시로
  재구성한다. `create_textured_materials.py`가 PBR·masked 머티리얼을 만든다.
- 적용 경계: 환경 시트는 방향 기준, 건식 석고는 PBR, 흔적은 값 마스크,
  위층 사람과 근접 소품은 3D다. 위층 사람 정면 레이어는 3D 접지 셸을
  보존한 정면 LOD다. 1.6m에서 켜고 1.25m까지 히스테리시스로 유지하며,
  측면에서는 반드시 숨긴다.
  나머지 스프라이트는 원거리 접근 불가 인물만 허용한다.
- 전체 프롬프트: `Docs/IMAGEGEN_PROMPTS_2026-08-10.md`
- 배치·거리·LOD 합격표: `Docs/MISSING_FLOOR_ART_MATRIX.md`

### 발소리 표면·퍼즐 판독면·현관문 생성 기록

- 서비스: 로컬 ChatGPT 소프트웨어의 ImageGen (프로젝트 동일 작업공간)
- 생성일: 2026-08-14
- 보존 원본: `TextureVillaStairCheckerPlatePaintedSteel.png`,
  `TextureRooftopUrethaneWaterproofing.png`,
  `TextureRooftopAnnexConcreteGypsumDebris.png`,
  `TextureUtilityMeterDialFaceBlank.png`,
  `TextureComplaintLedgerCarbonPaperBlank.png`,
  `TextureApartmentEntranceDoorCharcoalSteel.png`
- 파생: 알베도 6장과 PBR 동반 채널 18장, 머티리얼 6종
- 신규 파이프라인 단계: `Scripts/condition_ai_tiles.py`. ImageGen 스캔은
  타일이 될 수 없다 — 생성기는 구도를 만들고, 그 저주파 밝기 얼룩은 타일
  격자마다 반복되어 조명을 구운 것으로 읽힌다(`MISSING_FLOOR_ART_MATRIX.md`
  원칙 4가 금지하는 것). 이 단계가 채널별 flat-field로 얼룩과 색 캐스트를
  함께 지우고, 규칙 패턴은 주기를 찾아 정수배로 자른 뒤 좁은 크로스페이드로,
  확률적 표면은 넓은 크로스페이드로 이음매를 없앤다. **`generate_ai_pbr_maps.py`
  보다 먼저 돌아야 한다** — 노멀이 알베도에서 유도되므로 남은 얼룩은 가짜
  기하가 된다.
- 게인의 근거: 점묘 억제 프롬프트가 진짜 요철도 함께 눌러서 알베도 표준편차가
  1.86~3.78로 왔다(승인 에셋 대역 10~12). 노멀에 다이아몬드가 남지 않아
  `detail_gain`으로 되살렸고, 얼룩 판정은 절대 편차가 아니라 대비 대비
  편차로 잰다.
- 검증: 아트 계약(raw 56·scans 20·pbr_maps 80), `ART_TARGETED_BUILD PASS
  target=MissingFloor assets=71 uasset_audit=1`, V5 8지점 전부 밴드 안,
  `MISSINGFLOOR_GREYBOX PASS`, `Validate-Project.ps1` 전 계약 무회귀.
- 승인 경계: 세 바닥이 실제 플레이에서 **눈으로 구분되는지**는 사람의
  판단이다. 계약은 프레임이 깨지지 않았다는 것까지만 말한다.
- 전체 프롬프트: `Docs/IMAGEGEN_PROMPTS_2026-08-14.md`

### M0 1인칭 두드리기 손 생성 기록

- 서비스/모드: OpenAI ImageGen 내장 도구, `stylized-concept`
- 생성일: 2026-08-11
- 보존 원본: `Content/SourceArt/AI/SheetFirstPersonKnockPhases_v1.png`
- 투명 마스터: `Content/SourceArt/AI/SheetFirstPersonKnockPhases_v1_RGBA.png`
- 원본 SHA-256:
  `36C3E5BA1829C36D0B8CE967DFEF5719768E3AC02ABD444F088437F448DD58E1`
- 승인 v2 원본/투명 마스터:
  `Content/SourceArt/AI/SheetFirstPersonKnockPhases_v2.png`,
  `Content/SourceArt/AI/SheetFirstPersonKnockPhases_v2_RGBA.png`
- 승인 v2 SHA-256:
  `412D9173E7EE8AFD2270CF45008E3904AC0530DF2D0CDFCFEBCBC3A6DD9241E7`
- 파생: `Content/SourceArt/T_FPHandKnock0_D.png`부터
  `T_FPHandKnock3_D.png`, 런타임 `/Game/Prototype/Textures/T_FPHandKnock0_D`
  부터 `T_FPHandKnock3_D`
- 선택: v1 런타임 캡처에서 소매가 화면 안쪽 직선으로 끝나 v2 편집으로 같은
  손·네 포즈·세 땀을 유지한 채 소매를 각 셀 우하단 모서리까지 연장했다.
- 처리: ImageGen의 균일 초록 배경을 공식 `remove_chroma_key.py`의
  border auto-key·soft matte·despill·1px edge contract로 RGBA화했다.
  `Prepare-AIArt.ps1`이 2×2 시트를 투명 여백을 포함한 768px 네 장으로
  분리하고 알파를 보존한 제한적 2차 green despill을 적용한다. 이 여백은
  소매가 화면 안쪽의 사각 경계에서 잘리지 않게 타일 끝을 화면 밖으로 보낸다.
  `generate_surface_textures.py`는
  UI group·NoMip·Clamp·NeverStream으로 임포트한다.
- 적용 경계: 유효 노크의 준비·접촉·반동만 카메라 UI-space에서 알파
  블렌딩한다. 기존 3D/PBR 세계, 충돌, 조명과 접촉 그림자는 교체하지 않는다.
  흔들림 감소는 접촉 정지 프레임을 사용한다.
- 프롬프트 전문: `Docs/IMAGEGEN_PROMPTS_2026-08-11.md`

### M1 포획 포옹 생성 기록

- 서비스/모드: OpenAI ImageGen 내장 도구, `stylized-concept`
- 생성일: 2026-08-11
- 보존 원본: `Content/SourceArt/AI/SheetListenerCaptureEmbracePhases_v1.png`
- 투명 마스터:
  `Content/SourceArt/AI/SheetListenerCaptureEmbracePhases_v1_RGBA.png`
- 원본 SHA-256:
  `C27CAEBAE413E574AB3BA48AA95979718233C87893492420E8F5DBB9E11C999A`
- 파생: `Content/SourceArt/T_FPCaptureEmbrace0_D.png`부터
  `T_FPCaptureEmbrace3_D.png`, 각 1024×1024 RGBA. 런타임
  `/Game/Prototype/Textures/T_FPCaptureEmbrace0_D`부터
  `T_FPCaptureEmbrace3_D`
- 참조 경계: `SheetListenerEntityAnatomyReference.png`는 동일 인물의 석고·
  의복·비례, `night1-listener-corridor.png`는 1인칭 노출과 크기만 참고했다.
  배경·복도·UI는 생성물에 보존하지 않았다.
- 처리: 균일 초록 배경을 공식 `remove_chroma_key.py`의 border auto-key,
  soft matte, despill, 1px edge contract로 제거했다. 2×2 셀의 흰 격자를
  제외해 크롭하고 알파 보존 2차 despill 뒤 UI group·NoMip·Clamp·
  NeverStream으로 임포트했다.
- 적용 경계: 포획 암전의 1.2초에만 UI-space 알파 블렌딩한다. 위층 사람의
  월드 3D 셸, 벽, 그림자, 충돌, AI와 카메라는 각 런타임 시스템이 소유한다.
  모션 감소에서는 닫힌 정지 프레임만 사용한다.
- 프롬프트 전문: `Docs/IMAGEGEN_PROMPTS_2026-08-11.md`

## 2026-08-11 없는 층 M5 공동 리빌 원본

- `Content/SourceArt/AI/SheetFinalCavityRemainsReference_v1.png`
  - 도구/모드: OpenAI ImageGen 내장 생성, `stylized-concept`
  - SHA-256: `950EA1404E6804C083F4EF060ABA7AE175EB70DAD9BE396333605E889C855297`
  - 용도: 건조한 부분 골격, 내려앉은 작업복, 방수포와 캐스터의 3D 비율·재질 참고.
    생성본의 피부·머리카락과 스튜디오 배경은 사용하지 않는다.
  - 런타임: 4개 Geometry Script 정적 메시로 재구성하며 원본 PNG를 카메라나
    재질에 직접 노출하지 않는다.
- `Content/SourceArt/AI/SheetMokHansooConfrontationReference_v1.png`
  - 도구/모드: OpenAI ImageGen 내장 생성, `stylized-concept`
  - SHA-256: `466FC1AF73CD8852E955022FA5D9FBE1F38A04F6623F318F966F0373FEBCF618`
  - 용도: 목한수의 평균 체형, 작업복, 두 손 석고보드 파지와 피로한 표정 참고.
  - 런타임: 작업복·머리/손·석고보드 3개 근접 3D 메시로 재구성한다. 기존
    `T_SpriteMok_D`는 12m 이상 접근 불가 원거리 규칙을 유지한다.
- `Content/SourceArt/AI/FinalCavityFrontBlend_v1.png`
  - 도구/모드: OpenAI ImageGen 내장 생성·편집, `game-asset`
  - SHA-256: `9C26AAC9D3160CBD73B3183A332BC822FA8B6A1B655D5B24A6D642E1952B6D11`
  - 용도: 절차 3D 셸에서 부족한 건조한 의복·골격의 정면 판독 정보.
  - 파생: `T_SpriteFinalCavity_{D,N,R,A}`와 `M_SpriteFinalCavity`. 105~360cm·
    정면 내적 0.68 초과에서만 보이며, 숨은 3D 셸의 실제 그림자를 유지한다.
- `Content/SourceArt/AI/MokHansooFinalFrontBlend_v1.png`
  - 도구/모드: OpenAI ImageGen 내장 생성, `game-asset`
  - SHA-256: `B2C6787D66E595F6A32BD6FD653060773097BF560CFE0EEDF0877EAB6FA6547C`
  - 용도: 목한수의 피로한 얼굴과 낡은 재킷을 근접 정면에서 판독하기 위한 보강.
  - 파생: `T_SpriteMokFinalUpper_{D,N,R,A}`와 `M_SpriteMokFinalUpper`. 세로
    31~39%에서 알파를 없애 실제 3D 석고보드·하체·접지·그림자를 보존한다.

네 원본 모두 프로젝트 제작 레퍼런스·파생 소스이며 외부 인물·브랜드·상표를
참조하지 않은 생성물이다. 최종 런타임 형상과 배치, 거리·각도·재질 선택은
프로젝트 코드가 소유한다. 전체 프롬프트와 생성·편집 이력은
`Docs/IMAGEGEN_PROMPTS_2026-08-11.md`에 보존한다.

## 2026-09-17 관리실 문구류

- `BoothStationeryStudy_20260917.png`: 알파의 근영사 장부 제품 사진과 STAEDTLER 노리스 연필 사진을 형태 참고로 사용한 내장 imagegen 생성물. 외부 사진은 제품 형태를 확인하는 데 쓰며 게임에 원본을 넣지 않는다.
- `ComplaintRubbing_20260917.png`: 글자 없는 흑연 문지름 질감. 실제 한글은 별도 마스크로 합성한다.
- 두 생성 원본과 최종 프롬프트는 `Content/SourceArt/AI/BoothPrompts_20260917.json`에 연결되어 있다.
- 접수철의 손글씨는 나눔손글씨 펜체를 사용한다. [Google Fonts 원본](https://github.com/google/fonts/blob/main/ofl/nanumpenscript/METADATA.pb), 글꼴 파일과 SIL OFL 고지는 `Scripts/fonts/NanumPenScript/`에 보존한다. 글꼴을 수정하지 않았으며 게임에서는 구운 인쇄 이미지를 사용한다.
- 실물 참고 링크, 모델 치수, 전후 화면은 [관리실 점검 기록](BOOTH_REVIEW_20260917.md)에 정리한다.

## 2026-09-17 로비 분전반

- `CircuitPanelReference_20260917.png`: LS EBS32Fb 제품 실물 사진을 구조 참고로 전달해 내장 imagegen으로 만든 정면·사선·확대 시트다. 사진의 상표나 인증 표시는 모델에 옮기지 않았다.
- `CorridorCabinetReference_20260917.png`: KDM의 매입형 금속 함 사진과 로비 분전반 시트를 함께 참고해 만든 닫힌 함체 시트다. 별도 JSON에 프롬프트를 보존한다. `SM_CorridorCircuitCabinet`의 문틈·힌지·잠금쇠와 매입 깊이에 사용했다. 한글 이름표는 `SM_CorridorCircuitPrint`로 분리했다.
- 참고 사진 주소와 사용한 프롬프트는 `Content/SourceArt/AI/CircuitPanelReference_20260917.json`에 남긴다. 판매 사진 원본은 배포 에셋에 포함하지 않는다.
- Blender에서 `SM_LobbyCircuitPanel`, `SM_CircuitToggle`, `SM_CircuitPanelPrints`로 제작했다. 한글 이름표는 `Scripts/build_circuit_prints.py`로 만들고 인쇄 UV만 보정했다.
- 전원 상태, 회전축, 그림자와 반사는 게임이 계산한다. 생성 참고 이미지를 분전반 앞면에 붙이지 않는다.
- 실제 게임 화면과 검증 결과는 [분전반 점검 기록](CIRCUIT_REVIEW_20260917.md)에 정리한다.

프로젝트 코드의 공개 라이선스는 저장소 소유자가 별도로 선택합니다. 선택 전까지
저작권 고지만으로 공개 사용 권한을 추정하지 않습니다.

## 2026-08-12 타이틀·엔딩 C 환경 키아트

- 도구/모드: OpenAI 내장 ImageGen, `stylized-concept`
- 보존 원본: `Content/SourceArt/AI/TitleBackgroundMissingFloor_v1.png`
- 원본 SHA-256:
  `4831357AA6439F9CF93CC3D5CC4664DDF8EB995D59759E1F319504246CD57D39`
- 파생: `Content/SourceArt/T_TitleBackground_D.png`, 1920×1080 RGBA
- 런타임: `/Game/UI/Textures/T_TitleBackground_D`, `TEXTUREGROUP_UI`,
  `NoMipmaps`, `Clamp`, `NeverStream`
- 권리/참조: 외부 작품·로고·실존 건물·인물을 입력하지 않은 환경 생성물이다.
  UI 글자와 브랜드는 포함하지 않는다.
- 적용 경계: 타이틀/타이틀에서 연 크레딧의 배경과 엔딩 C의 무브랜드 매물
  외관 크롭만 담당한다. 방 번호·매물명·후기는 네이티브 한글 UI가 소유한다.
  일시정지는 현재 월드 화면을 유지하고, 메뉴·포커스·현지화 문자는 C++ HUD가
  소유한다.
- 프롬프트 전문: `Docs/IMAGEGEN_PROMPTS_2026-08-12.md`

## 2026-08-18 한국 빌라 외벽 스터코

- 도구/모드: OpenAI 내장 ImageGen, `game-asset`
- 보존 원본: `Content/SourceArt/AI/TextureKoreanVillaStucco_v1.png`
- 원본 SHA-256:
  `474A8D005146B499FDB39CBAE62FC17F790EC71E9D833E4864A387DBA959BA57`
- 파생: `Content/SourceArt/T_KoreanVillaStucco_{D,N,R,A}.png`
- 런타임: `/Game/Prototype/Textures/T_KoreanVillaStucco_{D,N,R,A}`와
  `/Game/Prototype/Materials/M_VillaStucco_{X,Y}`
- 적용 경계: 생성물은 한글·상표·사물·조명·그림자가 없는 외벽 BaseColor
  원본만 담당한다. N/R/A와 월드 매핑은 재현 가능한 로컬 스크립트가 만들며,
  UE의 실제 광원과 Lumen이 최종 명암을 계산한다.
- 프롬프트 전문: `Docs/IMAGEGEN_PROMPTS_2026-08-18.md`
