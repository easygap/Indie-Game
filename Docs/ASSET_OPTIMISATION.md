# 에셋 고도화 계약 — 아틀라스 · 리토폴로지 · LOD · 물리 배치

이 문서는 「없는 층」의 에셋이 **실제 게임에서 돌아가는 상태**를 무엇으로
정의하고, 그것을 어떤 자동 검사로 지키는지 적는다. 셋 다 사람이 눈으로
보고 넘어가면 반드시 새는 종류의 것이라 전부 스크립트가 판정한다.

---

## 1. 물리적 타당성 — `Scripts/audit_world_geometry.py`

건물·골목·편의점·CH03 옥상은 레벨 에디터가 아니라 C++ 안의 센티미터
좌표로 조립된다. 빠르지만 눈으로는 검증이 불가능하다. 선반이 벽 속으로
3cm 들어가도, 병이 상판 위 4cm에 떠 있어도 컴파일러도 엔진도 아무 말을
하지 않는다. 플레이어만 "저건 물건이 저렇게 있을 수 없는데"라고 느낀다.

검사기는 UE 없이 `IGPrologueWorldScene.cpp`와 `IGThirdMorningDirector.cpp`의
배치 호출을 다시 평가해 상자를 복원한다. 범위 for, 카운트 for, 배열 상수,
로컬 람다, 삼항 연산자까지 해석해 현재 배치식의 **78.8%**를 복원한다.

| 코드 | 뜻 | 판정 |
|---|---|---|
| `FLOATING` | 아래에 받치는 것이 없고 옆·위로도 닿는 것이 없다 | 오류 |
| `EMBEDDED` | 구조물 안에 완전히 잠겨 어느 면도 표면에 닿지 않는다 | 오류 |
| `SUNK` | 서 있는 바닥면보다 밑동이 제 높이의 1/4 넘게 내려갔다 | 경고 |

```
python3 Scripts/audit_world_geometry.py            # 사람이 읽는 보고
python3 Scripts/audit_world_geometry.py --json     # 기계가 읽는 보고
python3 Scripts/audit_world_geometry.py --check    # 오류가 있으면 1로 종료
python3 Scripts/audit_world_geometry.py --coverage # 복원 못 한 배치식 목록
```

`--check`는 `Validate-Project.ps1`이 호출한다.

**판정에서 빼야 할 배치**는 호출 위에 이유와 함께 주석으로 표시한다.
숨긴 충돌 프록시처럼 보이지 않는 것이 정상인 경우가 그렇다.

```cpp
// physics-audit: intentional hidden collision proxy inside the frame
CreateBlock(...);
```

검사기가 스스로 무시하는 것은 셋뿐이고, 전부 이유가 분명하다.

- **Movable 블록** — 떨어지는 영수증, 올라오는 수위처럼 비트가 옮기는
  것. 저작 좌표는 시작 자세이지 놓인 자리가 아니다.
- **저작 메시 오버라이드** — `FVector(100.0f)`는 크기가 아니라 그 에셋
  자신의 100% 배율이므로 상자 부피에 아무 의미가 없다.
- **평가 불가능한 회전** — 런타임 벡터에서 나온 `Rotation()` 같은 것.
  대각선 길이로 감싼 보수적 경계만 남겨 이웃의 접촉 판정에는 쓰고,
  자기 자신은 판정하지 않는다.

---

## 2. 텍스처 아틀라스 — `Scripts/build_texture_atlas.py`

호수판·고지문·제품 라벨·상가 간판 51장은 전부 작고, 전부 타일링하지
않고, 편의점과 4층 복도에서는 대부분이 한 화면에 같이 있다. 그런데 각각이
자기 텍스처·자기 머티리얼·자기 드로우콜을 들고 있었다.

- 계약: `Scripts/texture_atlas_contract.py` (Pillow도 `unreal`도 안 씀)
- 페이지: 2048px, 엔트리마다 8px **가장자리 연장** 여백
- 산출물: `Content/SourceArt/Atlas/T_PrintAtlas*_D.png` + `print_atlas.json`
- 임포트: `Scripts/import_texture_atlas.py` (Clamp, BC7, 밉 상한)
- 머티리얼: `create_textured_materials.py`가 매니페스트를 읽어
  `UV0 × scale + bias`로 한 페이지를 샘플한다

패킹은 MaxRects(best-short-side-fit)이고 **입력 순서와 무관하게 같은
결과**를 낸다. 페이지 가장자리에는 여백을 두지 않는다 — 옆에 아무것도
없고 샘플러가 Clamp하기 때문이다. 엔트리끼리는 언제나 여백 두 칸 떨어진다.

```
python3 Scripts/build_texture_atlas.py             # 굽기
python3 Scripts/build_texture_atlas.py --check     # 최신인지만 확인
python3 Scripts/build_texture_atlas.py --self-test # 패커 자체 검증
```

`--self-test`는 합성 텍스처로 배치 겹침·여백·페이지 이탈·UV 왕복·순서
독립성·페이지 수 하한을 전부 검사한다. 원본 아트는 Git LFS에 있으므로
LFS를 받지 않은 체크아웃에서도 이 검증만은 돌아간다. 원본이 포인터
파일이면 패커는 쓰레기 페이지를 굽지 않고 그렇다고 말하고 멈춘다.

**아틀라스는 전부 선택 사항이다.** 매니페스트가 없으면 머티리얼은 예전처럼
각자 텍스처를 쓴다. 보이는 것은 같고 드로우콜만 는다.

아틀라스에서 빼는 텍스처는 `ATLAS_EXCLUSIONS`에 이유와 함께 적는다
(U로 타일링하는 가격 띠, 1K 파사드 간판, 밉이 없는 UI 텍스처).

---

## 3. 리토폴로지와 LOD — `Scripts/mesh_lod_contract.py`

이전 계약은 두 가지가 비어 있었다. 절차 메시는 엔진 **LOD 그룹**만
요청하고 끝났고, "근접 조사용"으로 표시한 히어로 프롭 28종은 **아무것도
요청하지 않아** LOD가 하나뿐인 채로 나갔다. 방 건너편에서도 전밀도로
그려졌다는 뜻이다. 포토그래메트리 스캔 15종도 마찬가지로 스캔된 밀도
그대로였다.

이제 세 등급 모두 같은 계약을 받는다.

| 등급 | LOD0 예산 | LOD 체인 (비율 @ 화면 크기) | 라이트맵 |
|---|---|---|---|
| `hero` | 12,000 tri | 55% @ 0.28 · 25% @ 0.09 · 10% @ 0.025 | 64 |
| `prop` | 3,000 tri | 45% @ 0.20 · 18% @ 0.06 · 7% @ 0.015 | 32 |
| `large` | 9,000 tri | 60% @ 0.14 · 30% @ 0.045 · 12% @ 0.010 | 96 |

- **리토폴로지**는 스태틱 메시가 만들어지기 *전에* 다이내믹 메시에서
  일어난다. 그래야 충돌 헐과 LOD 체인이 둘 다 예산에 맞춘 지오메트리에서
  파생된다.
- **인쇄면 메시**(라벨 띠, 컵 슬리브, 포스트잇, 원장, 달력)는 단순화하지
  않는다. UV 이음매를 넘어 용접하면 인쇄물이 병을 돌아가며 뭉개진다.
- **라이트맵 UV**를 채널 1에 굽는다. 없으면 패커가 UV0로 폴백하는데,
  그 UV0에는 인쇄 아트가 올라가 있다.
- 스캔 프롭은 LOD0 자체를 예산 비율로 줄인 뒤 같은 곡선을 붙인다.

`validate_baked_art_assets.py`가 메시마다 LOD 개수, LOD0 삼각형 수,
라이트맵 채널 존재, 인쇄면 밀도 하한을 확인한다. LOD가 하나뿐인 메시는
이제 실패다.

---

## 실행 순서

```
Scripts/Build-ArtAssets.ps1
  Prepare-AIArt.ps1
  condition_ai_tiles.py
  generate_ai_pbr_maps.py
  build_texture_atlas.py        ← 아틀라스 굽기 (에디터 밖)
  --- 에디터 ---
  generate_meshes.py            ← 리토폴로지 + LOD 체인
  generate_surface_textures.py
  import_texture_atlas.py       ← 페이지 임포트
  create_textured_materials.py  ← 아틀라스를 읽는 머티리얼
  apply_photo_prop_lods.py      ← 스캔 프롭 예산 + 체인
  validate_baked_art_assets.py  ← 전부 확인

Scripts/Validate-Project.ps1
  audit_world_geometry.py --check
  build_texture_atlas.py --self-test
```

## 아직 안 된 것

- 아틀라스 페이지 PNG는 **아직 굽지 않았다.** 원본이 Git LFS에 있어
  `git lfs pull` 없이는 패커가 돌 수 없다. 패커·계약·임포트·머티리얼
  경로와 자체 검증은 전부 들어가 있고, LFS가 있는 기계에서
  `Scripts/build_texture_atlas.py` 한 번이면 페이지와 매니페스트가 나온다.
- 리토폴로지·LOD 계약의 실제 삼각형 수치는 UE 5.8에서
  `Build-ArtAssets.ps1`을 돌려야 확정된다. 예산 자체는 코드에 있고
  검사기가 강제하지만, 어느 메시가 실제로 얼마나 줄었는지는 그 실행의
  로그가 증거다.
- 정적 물리 검사의 복원율 78.8%. 나머지는 런타임 상태에 의존하는
  배치식이다(엔진 벡터에서 온 회전, 컴포넌트 경계에서 계산한 위치).
  `--coverage`가 그 목록을 그대로 찍는다.
