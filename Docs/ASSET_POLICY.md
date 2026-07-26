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
| `/Game/Photo/Props/*` (15종: old_bed_frame, side_table_01, metal_office_desk, painted_wooden_chair_01, modern_wooden_cabinet, desk_lamp_arm_01, electric_stove, street_lamp_01, trashbag, cardboard_box_01, steel_frame_shelves_01, CashRegister_01, plastic_crate_01, utility_box_01, wine_bottles_01) | polyhaven.com (포토그래메트리 스캔) | CC0 1.0 | 2026-07-19 | 원본(glTF, 1K 텍스처) | `Content/SourceArt/PhotoProps/` | 실물 스캔 소품 |
| `/Game/Meshes/SM_*` (16종: 생수병·소주병·음료병·병뚜껑·컵라면·김치통·우유팩·스낵봉지·알람시계·레버손잡이·가로등갓·바스툴·장바구니·김밥팩·샌드위치팩·라벨슬리브) | 직접 제작 (UE5 Geometry Script 절차 모델링) | 프로젝트 소유 | 2026-07-25 | 원본 | `Scripts/generate_meshes.py` | 회전체·베벨·불리언·스윕 |
| `/Game/Prototype/Textures/T_Label* · T_Snack*` | 직접 제작 (System.Drawing) — 가상 브랜드, 실제 상표 미사용 | 프로젝트 소유 | 2026-07-25 | 원본 | `Scripts/Create-LabelTextures.ps1` | 제품 라벨·봉지 아트 |
| `/Game/Prototype/Textures/T_Sign* · T_Poster* · T_Note*` | 직접 제작 (System.Drawing + 시스템 폰트) | 프로젝트 소유 | 2026-07-19 | 원본 | `Scripts/Create-SignTextures.ps1` | 한글 간판·포스터 |
| `/Game/Prototype/Textures/T_(Jangpan·Wallpaper·…)_{D,N,R}` | 직접 제작 (절차 생성) | 프로젝트 소유 | 2026-07-19 | 원본 | `Scripts/generate_surface_textures.py` | 사진 텍스처 폴백 |

ambientCG 자료는 CC0 1.0(상업적 사용·수정·재배포 허용, 출처 표기 불요)이며, 원본 zip은 `Content/SourceArt/PhotoZips/`에 증빙으로 보관합니다.

## 생성형 이미지(ImageGen) 아트워크

전부 ChatGPT ImageGen(사용자 계정, 로컬 브라우저)으로 생성하고
`Scripts/Prepare-AIArt.ps1`로 크롭·리샘플·미세문구 재작성했습니다. 원본은
`Content/SourceArt/AI/`에 그대로 보관합니다.

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

한 번의 생성에 5분이 걸리므로 낱장 대신 **격자 시트**로 묶어 뽑고 슬라이스합니다.
25장의 텍스처를 8회 생성으로 만들었습니다.

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

### CH02 종이 시트 생성 기록

- 서비스: ChatGPT ImageGen
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

프로젝트 코드의 공개 라이선스는 저장소 소유자가 별도로 선택합니다. 선택 전까지 저작권 고지만으로 공개 사용 권한을 추정하지 않습니다.
