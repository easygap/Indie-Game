# Blender · TRELLIS.2 에셋 파이프라인

2026-09-04부터 소품·설비·인물은 지오메트리 스크립트 상자 조립 대신 Blender에서
만든다. 두 갈래다.

- **딱딱한 것**(문·가구·설비·소품): `Scripts/blender/build_*.py`가 Blender
  헤드리스에서 절차 모델링하고 Cycles로 PBR을 굽는다.
- **살아 있는 것**(위층 사람·고양이·인물·유해): 기준 시트 한 칸을 ComfyUI
  네이티브 TRELLIS.2에 넣어 형상을 뽑고, `refine_generated.py`가 게임 메시로
  다듬는다.

둘 다 `Content/SourceArt/Blender/<SM_Name>/`에 FBX·텍스처·미리보기·manifest를
남기고, `Import-BlenderAssets.ps1`이 UE 에셋으로 만들어 `Content/`에 넣는다.

## 왜 에디터가 아닌가

2026-08-25부터 Smart App Control이 이 저장소의 `UnrealEditor-IndieGame.dll`을
막는다. 게임 모듈을 싣는 실행은 전부 안 된다. 그런데 메시·텍스처·재질
에셋은 게임 클래스를 참조하지 않는다. 게임 모듈이 없는 콘텐츠 전용 프로젝트
`%LOCALAPPDATA%\IndieGame\ArtImport`는 22초에 부팅하고, 거기서 만든 uasset을
`Content/`에 복사하면 그대로 쓰인다. `Import-BlenderAssets.ps1`이 이 흐름을
전부 자동으로 한다. 게임 화면 캡처는 여전히 못 하므로 에셋 확인은 Blender
미리보기 렌더로 한다.

## 도구 위치

| 도구 | 위치 | 비고 |
|---|---|---|
| Blender 5.2.0 LTS | `%LOCALAPPDATA%\Programs\Blender-5.2\Blender Foundation\Blender 5.2\blender.exe` | Downloads의 서명된 MSI를 `msiexec /a`로 푼 포터블. 관리자 권한 불필요. `IG_BLENDER`로 덮어쓸 수 있다 |
| ComfyUI 0.34.6 | `..\lunia_z\Reference\Tools\ComfyUI-0.34.6` | 옆 프로젝트가 2026-09-08에 받아 검증한 설치와 모델(15GB)을 빌려 쓴다. `IG_COMFYUI`로 덮어쓸 수 있다 |
| 콘텐츠 전용 UE 프로젝트 | `%LOCALAPPDATA%\IndieGame\ArtImport` | `Import-BlenderAssets.ps1`이 만들고 관리한다 |

## 명령

```powershell
# 1. 절차 빌더 전부(또는 -Only unit_door, SM_MailboxUnit …)
powershell -File Scripts\Build-BlenderAssets.ps1

# 2. 생성 경로: 서버를 띄우고, 기준 이미지 한 장으로 형상을 뽑고, 다듬는다
powershell -File Scripts\Start-ComfyNative.ps1
python Scripts\generate_3d_comfy.py --image Content\SourceArt\Generated\X\source-front.png --name X --route trellis --resolution 1024
blender -b --factory-startup --python Scripts\blender\refine_generated.py -- --glb Content\SourceArt\Generated\X\trellis1024-s56\raw\pbr_00001_.glb --name SM_X --mesh-class hero --height 172 --yaw 0 --probe   # 축 확인
blender -b --factory-startup --python Scripts\blender\refine_generated.py -- --glb … --name SM_X --mesh-class hero --height 172 --yaw 0 --vertex-ao

# 3. UE 반입(콘텐츠 전용 프로젝트 → Content 복사 → Docs/mesh_bounds.json 갱신)
powershell -File Scripts\Import-BlenderAssets.ps1            # 전부
powershell -File Scripts\Import-BlenderAssets.ps1 -Only SM_X # 골라서

# 4. 게임 코드가 그 메시를 쓰게 고쳤으면 컴파일과 감사
powershell -File Scripts\Build-ArtAssets.ps1 -CodeOnly
python Scripts\audit_world_geometry.py --check
```

## 배치 확인

에디터를 못 여는 동안 「메시가 씬 제자리에, 제 크기와 방향으로 들어갔는지」는
Blender로 본다. 지오메트리 감사가 푼 상자 전부(벽·바닥·상자 소품)와 저작 메시를
UE 좌표 그대로 세워 EEVEE로 찍는 것이라, 감사가 보는 세계와 같은 것을 본다.

```
python Scripts/audit_world_geometry.py --export-layout Saved/scene_layout.json
blender -b --factory-startup --python Scripts/blender/render_scene_layout.py -- Saved/scene_layout.json Saved/scene_preview [view ...]
```

카메라 자리는 `render_scene_layout.py`의 `VIEWS`에 있다(편의점 다섯, 403호 셋,
복도·로비·골목, 그리고 천장을 잘라 낸 평면도 셋). 상자는 재질 이름으로 색을
정하고 저작 메시는 저장된 `.blend`와 구운 D/ORM을 쓴다. 폴백 블록아웃은 빼야
메시가 보이므로, 저작 메시 `if` 뒤의 `else` 블록에는 반드시
`// physics-audit: intentional 저작 메시가 없을 때만 짓는 폴백이다. …` 표식을
호출 위 8줄 안에 둔다. 표식은 같은 평면 감사도 읽는다 — if/else 두 가지를 동시에
세워 놓고 서로 깜빡인다고 잡는 오탐이 그것으로 사라진다.

Blender 원본이 없는 메시(스캔 소품, 예전 지오메트리 스크립트 메시)는 구운
바운드 상자로만 서고, 감독이 스폰하는 소품(쪽지·병)은 CreateBlock이 아니라
안 보인다. 금속은 하늘이 없으면 검게 죽으므로 월드를 밝은 회색으로 둔다.

## 좌표와 원점 규약

- 빌더는 **UE 좌표로** 그린다. m 단위, X·Y·Z가 UE의 cm/100이다.
  `build_probe_axes.py`로 실측한 결과 기본 FBX 경로는 Y만 뒤집히므로,
  `build_asset`이 결합 직후 정점 Y를 거울 반전한다. UE가 다시 뒤집어 빌더가
  적은 좌표가 그대로 UE 좌표가 된다. 굽기는 반전 뒤에 하므로 노멀맵도 UE가
  보는 기하 그대로다.
- 원점은 바닥 중심, 앞면은 -Y가 기본이다. 씬 좌표계가 다른 것(냉장고·주방은
  앞면 -X, 천장 등은 윗면 원점, 문짝은 힌지 축)은 manifest의 `origin`·`notes`에
  적고, 씬 코드가 그 관례대로 놓는다.
- 씬은 `CreateBlock(원점, FVector(100, 100, 100), nullptr, 충돌, 메시)`로 놓는다.
  크기 100은 배율 1이고, `nullptr` 재질은 「메시가 들고 온 인스턴스를 써라」다.
  `audit_world_geometry.py`는 `PropMesh(TEXT("SM_X"))` 바인딩과
  `Docs/mesh_bounds.json`으로 실제 상자를 세운다.

## 재질과 텍스처

- 절차 재질(도장 강판·스테인리스·플라스틱·고무·유리·발광·테라조)은
  `ig_blender_lib.py`의 `mat_*`다. 한 에셋의 baked 재질은 전부 한 장의
  `_D`(sRGB) / `_N`(DirectX, G 뒤집음) / `_ORM`(AO·거칠기·금속성) / `_E`로
  굽히고, UE 마스터 `M_IGBakedProp`의 인스턴스 `MI_<Name>`이 슬롯 `Baked`에
  걸린다. 유리는 슬롯 `Glass`로 남겨 기존 `M_Glass`가 걸린다.
- 생성 시트를 붙이려면 `image_quad` + `mat_image_uv`. 별도 UV 층 `ImageUV`로
  읽어서 스마트 UV가 활성 층을 새로 펴도 이미지가 제자리에 들어간다.
- 한글 글자는 `text_mesh`(맑은 고딕)로 얇은 기하를 세운다. 텍스처에 글자를
  굽지 않는다는 규칙은 그대로다 — 글자는 기하다.
- 발광은 8비트 이미지에서 1.0에서 잘리므로 manifest의 `emissive_strength`가
  인스턴스의 `EmissiveStrength`로 간다.

## 충돌·LOD

- 충돌은 빌더가 `UCX_<Name>_NN` 볼록 껍데기로 낸다. 5.8은 FBX를 Interchange로
  넘기고 그 경로는 UCX를 버리므로, 임포트 스크립트가
  `Interchange.FeatureFlags.Import.FBX 0`으로 예전 FBX 임포터를 잡는다.
- LOD는 `mesh_lod_contract.py`와 같은 사슬이다. manifest의 `mesh_class`
  (hero 12000 / prop 3000 / large 9000)가 우선한다.

## 생성 경로의 규칙

- 라이선스: TRELLIS.2(MIT)·DINOv3(Meta 제한 허가)·BiRefNet(MIT)만 쓴다.
  Hunyuan3D는 커뮤니티 라이선스가 한국을 제외하므로 쓰지 않는다. 모델 파일
  해시는 lunia_z의 `SourceArt/Manifests/ComfyNative.20260908.json`에 있다.
- 입력은 우리 기준 시트(`Content/SourceArt/AI/Sheet*.png`)의 한 칸이다. 생성
  기록은 `Content/SourceArt/Generated/<name>/<trial>/generation.json`에 남는다.
- `refine_generated.py`가 하는 일: 회전(`--yaw`, `--rot-x`)으로 정면을 -Y·머리를
  +X에, 한 축을 실제 cm에(`--length/--width/--height`), 배경 잘라내기
  (`--clip-y-*`, `--shift`, `--squash`), 저밀도 데시메이트(예산), 고밀도→저밀도
  굽기. 털처럼 얇은 조각으로 깨진 표면은 `--voxel-remesh 0.006`으로 녹인다.
  위층 사람처럼 정점색 AO를 읽는 재질은 `--vertex-ao`.
- 축이 헷갈리면 `--probe`(주축·양끝 높이)와 `--views`(정면·측면·위 세 장)로
  본 뒤 정한다. 생성물은 입력 그림에 따라 눕거나 대각선으로 나온다.

## 지금까지 바꾼 것

| 에셋 | 경로 | 씬 |
|---|---|---|
| SM_UnitDoorLeaf / L / Frame | 절차 | 401·402·403 현관, AIGSwingDoor. 문짝은 판 앞으로 4 mm 안쪽만 나온다 |
| SM_UnitDoorHardware / L | 절차 | 레버·도어락. 문짝과 원점이 같아 같은 자리에 충돌 없이 놓는다. 문짝에서 뗀 이유는 `build_unit_door.py` 머리말 |
| SM_FireExtinguisherBox / SM_FireExtinguisher | 절차 | 복도 남쪽 벽, 밤1 낙하 물리 소품 |
| SM_MailboxUnit | 절차 | 로비 북쪽 벽 |
| SM_CeilingLightRing / Dome | 절차 | 복도 넷·로비 둘. 돔 슬롯은 씬이 M_LightPanel로 덮는다 |
| SM_FridgeBody / Door | 절차 | AIGFridge |
| SM_KitchenBaseRun 등 주방 7종 | 절차 | BuildApartment |
| SM_ApartmentWindow / SM_VenetianBlind | 절차 | 403호 북쪽 창. 유리는 씬 발광판, 블라인드 원점은 헤드레일 윗면 |
| SM_VideoIntercom / SM_WallSwitch / SM_ShoeCabinet | 절차 | 403호 현관 벽(Y -215). 인터폰 앞면은 생성 패널 그림 |
| SM_StoreCoolerBank / Door, SM_StoreGondola, SM_StoreCounter, SM_CardTerminal, SM_HotSnackWarmer, SM_ChestFreezer, SM_OpenShowcase, SM_RamyeonRack | 절차 | BuildStore. 선반 윗면 높이가 씬 상품 배치와 같다. 열린 칸 문짝은 힌지 원점에 yaw 120. 단말기·온장고는 상판 위(Z 99) 별도 메시라 계산대 바운드가 상판에서 끝난다 |
| SM_VillaWindow | 절차 | 골목 빌라 파사드 창 열다섯 자리. 유리 판은 씬 상자 |
| SM_UtilityPole | 절차 | 골목 전주 둘. 분전함은 스캔 소품 |
| SM_ListenerEntityCrawl | 생성 | 위층 사람. 정점 AO, 석고 재질은 그대로 |
| SM_AlleyCatRun | 생성 | 골목 고양이. 구운 털 색을 MI로 쓴다 |
| SM_MokHansooFigure | 생성 | 밤4 목한수 통짜. 조각 셋과 카드를 대체 |
| SM_FinalCavityRemains | 생성 | 밤4 공동 유해 통짜. 조각 넷과 카드를 대체 |
