# 생활 소품과 겹침 수정

2026년 9월 17~18일 작업. 입주 경로의 방, 복도, 골목, 편의점과 옥상에서 찍은 실제 게임 화면을 대조했다. 아래 비교 사진은 밝기나 사물 위치를 보정하지 않은 캡처다. 표시용 WebP를 누르거나 `Docs/Media/`의 같은 이름 PNG를 열면 원본과 비교할 수 있다.

## 창틀을 뚫던 침대

사용자가 표시한 부분은 실제 겹침이었다. 침대 머리판이 돌출된 창틀까지 들어가 있었다. 프레임, 침구, 눕기 판정을 창에서 12cm 떨어뜨렸다. 실행 중 메시 범위를 재면 가장 가까운 창 손잡이까지 2.90cm, 발치 협탁까지 5.27cm가 남는다.

| 수정 전 | 수정 후 |
|---|---|
| ![창틀에 파고든 머리판](Media/readme/household-before-bed.webp) | ![창틀과 떨어진 머리판](Media/readme/bedroom-after-bed.webp) |

옆 통로로 걸어가는 검사와 옆·발치·베개 쪽에서 눕기를 조준하는 검사를 다시 통과했다. 창틀과 협탁의 간격도 침실 실행 검사에 넣었다. [창 쪽 근접 화면](Media/bedroom-after-pillow.png), [발치](Media/bedroom-after-foot.png), [방 전체](Media/bedroom-after-room.png)에서 배치를 확인할 수 있다.

## 실물에 맞춰 다시 만든 물건

[무인양품 면 슬리퍼](https://www.muji.us/products/cotton-pile-open-toe-slippers-jkaj2a6s), [LG 실외기 설치 안내](https://www.lge.co.kr/story/user-guide/air-conditioners-install-guide), [IKEA 반려동물 그릇](https://www.ikea.com/de/en/p/utsadd-pet-bowl-metal-40638833/)의 사진을 확인한 뒤 imagegen에 참고 이미지로 넣었다. 만들어진 여러 각도의 참고도에서 형태를 대조하고 Blender 모델에 반영했다. 슬리퍼의 면 원단도 imagegen으로 생성해 재질에 구웠다.

| 수정 전 | 수정 후 |
|---|---|
| ![뾰족하고 얇은 슬리퍼](Media/readme/household-before-slipper-low.webp) | ![둥근 밑창과 열린 면 갑피](Media/readme/household-after-slipper-low.webp) |
| ![막힌 상자였던 물그릇](Media/readme/household-before-bowl-close.webp) | ![안쪽 벽과 물이 있는 스테인리스 그릇](Media/readme/household-after-bowl-close.webp) |
| ![닫힌 판처럼 보이던 실외기 팬](Media/readme/household-before-condenser-front.webp) | ![철망 안에 들어간 팬과 벽으로 이어진 배관](Media/readme/household-after-condenser-front.webp) |

슬리퍼는 27×10cm 크기에 밑창, 발바닥 쿠션, 앞이 뚫린 갑피를 따로 만들었다. 물리 충돌은 밑창만 감싸도록 해 두 짝 모두 바닥에서 기울거나 떨리지 않고 멈추는지 확인했다.

물그릇은 지름 18cm, 높이 4.6cm다. 얇은 테두리, 안쪽 벽, 고무 바닥을 만들고 문선에서 떨어뜨렸다. 수면은 낮은 거칠기의 고정 면이다. 투명한 물의 굴절이나 유체 계산은 사용하지 않는다. 그릇을 조사하면 목격 기록이 한 번만 남고, 누르다 놓으면 취소된다.

실외기는 80×30×55cm 외함 안에 팬을 넣고 철망, 측면 통풍구, 서비스 덮개를 만들었다. 받침과 배관을 모델에 포함하고 벽과 맞닿게 배치했다. 여섯 대의 받침 두 곳과 배관 한 곳, 총 18곳에서 벽 접촉을 검사한다. 창을 가리던 외함을 내렸고 중복 받침 16개와 벽에서 뜬 검은 얼룩 면 네 개를 제거했다. [측면](Media/household-after-condenser-side.png)과 [빌라 정면](Media/household-after-facade.png)도 따로 촬영했다.

참고 이미지와 프롬프트는 [생활 소품](../Content/SourceArt/AI/HouseholdStudy_20260917.json), [물그릇·원단](../Content/SourceArt/AI/HouseholdMaterials_20260917.json)에 있다. 사진 원본을 게임 텍스처로 붙여 넣지는 않았다.

## 보이는 단서와 대사 맞추기

목격 소품 세 가지는 물건의 형태와 대사가 맞지 않았다. 약봉투는 경계석에 묻혀 있었고, 근무표는 온장고 밑에 끼어 있었다. 담배꽁초 자리에는 상자만 놓여 있었다.

[국내 약봉투 인쇄 견본](https://printingkorea.net/front/productlist.php?code=005004), [A4 클립보드 제품 사진](https://www.elizabethrichards.com.au/products/a4-wooden-clipboard), [실제 담배꽁초 사진](https://museumoflitter.org/cigarette-butt-litter/)을 imagegen의 형태 참고로 사용했다. [생성 참고도와 입력 기록](../Content/SourceArt/AI/WitnessPropsStudy_20260917.json)을 남겼다. 종이에 들어갈 한글은 직접 조판해 잘못된 글자와 좌우 반전을 막았다.

| 수정 전 | 수정 후 |
|---|---|
| ![경계석에 가려진 약봉투](Media/readme/witness-before-pills-close.webp) | ![바닥에서 읽을 수 있는 종이 약봉투](Media/readme/witness-after-pills-close.webp) |
| ![온장고 아래의 빈 종이](Media/readme/witness-before-roster-close.webp) | ![온장고와 떨어진 실제 교대표](Media/readme/witness-after-roster-close.webp) |
| ![담배 대신 놓여 있던 상자](Media/readme/witness-before-butts-close.webp) | ![눌러 끈 꽁초 여섯 개](Media/readme/witness-after-butts-close.webp) |

약봉투는 11×18cm 종이에 벌어진 덮개를 달았다. 인쇄된 이름을 보고 “서일영이라고 적혀 있다”고 말한다. 후속 대화도 “골목에 떨어진 약봉투에 서일영 씨 이름이 있었어요”로 고쳤다. 봉투만 보고 지난해부터 약을 먹었다고 단정하던 대사는 없앴다.

근무표에는 게임 시점인 2025년 7월 마지막 주 날짜와 교대 시간을 적었다. 나린은 평일 야간, 주말 오후 근무로 구분된다. 목격 대사도 평일 밤 근무를 확인하는 내용으로 맞췄다. 꽁초는 필터·종이·꺼진 재를 구분하고 대사에 나오는 수대로 여섯 개를 모델링했다. 물그릇을 바닥으로 옮긴 데 맞춰 후일담의 창턱 언급도 고쳤다.

[9월 16일 공개된 Townfall 제작진 글](https://blog.playstation.com/2026/09/16/silent-hill-townfall-creators-break-down-ps5-features-out-september-25/)의 환경 단서와 관찰 중심 진행을 참고했다. 이번에 적용한 것은 눈으로 확인한 물건이 후속 답변의 근거가 되도록 맞추는 방식이다. 새 퍼즐이나 포획 연출을 추가한 작업은 아니다.

## 렌더링과 에셋 비용

UE 5.8의 [인스턴스 메시 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine)를 확인해 실외기 여섯 대를 한 ISM 컴포넌트로 묶었다. 인스턴스마다 LOD가 바뀌고 55m 밖에서는 숨긴다. 움직이지 않는 외장 설비라 물리 충돌과 내비게이션 영향도 껐다.

새 소품은 각각 한 메시·한 재질로 굽고, 단순 충돌 한 개와 스트리밍 텍스처를 쓴다. 엔진 반입 후 직접 읽은 삼각형 수는 아래와 같다. 약봉투는 원본부터 80개여서 축약기가 64개에서 멈춘다.

| 소품 | LOD0 → LOD1 → LOD2 → LOD3 | 텍스처 |
|---|---|---|
| 슬리퍼 | 2,856 → 1,286 → 514 → 200 | 1K |
| 실외기 | 8,304 → 4,982 → 2,492 → 996 | 2K |
| 물그릇 | 2,448 → 1,102 → 440 → 172 | 1K |
| 약봉투 | 80 → 64 → 64 → 64 | 1K |
| 근무표 | 668 → 300 → 120 → 64 | 2K |
| 꽁초 여섯 개 | 1,008 → 454 → 182 → 70 | 512px |

[반입 검사 수치](Performance/Household20260918-assets.json)에 재질 수·충돌 수·텍스처 크기도 기록했다. [Virtual Shadow Maps](https://dev.epicgames.com/documentation/unreal-engine/virtual-shadow-maps-in-unreal-engine)와 [Significance Manager](https://dev.epicgames.com/documentation/en-us/unreal-engine/significance-manager-in-unreal-engine)도 검토했다. 이번에는 움직이는 소품의 그림자 캐시를 강제로 고정하거나 별도의 중요도 관리기를 추가하지 않았다. 실외기의 반복 컴포넌트와 중복 형상부터 줄였다.

첫 측정에서 비용 증가를 확인한 뒤 작은 목격 소품 네 개의 렌더링 범위를 더 줄였다. [메시 거리장 설정 문서](https://dev.epicgames.com/documentation/unreal-engine/mesh-distance-fields-properties-in-unreal-engine)를 대조해 이 소품들의 거리장·간접광 기여를 끄고 표시 거리를 16m로 제한했다. 얇은 약봉투와 근무표는 그림자를 만들지 않으며, 그릇과 꽁초는 직접광 그림자를 유지한다. 조사할 수 있는 거리는 이보다 훨씬 짧아서 상호작용 중 사라지지 않는다.

## 성능 측정

Ryzen 9 7900X·RTX 3060, UE 5.8.1, D3D12, 1920×1080, 높음, 화면 비율 100%에서 측정했다. VSync와 프레임 제한을 끄고 같은 아홉 시점으로 이동했다. PNG 저장 비용은 제외했고 초기 120프레임을 버렸다.

17일 수정 전 측정과 18일 수정 후 측정의 차이가 커서, 18일에 수정 전 코드·에셋으로 다시 빌드해 재측정했다. 비교용 빌드에서도 같은 카메라 검사 코드를 썼다. 측정 뒤 수정본 열두 파일을 복원하고 SHA256 일치를 확인한 다음 다시 빌드했다.

| 항목 | 수정 전, 18일 재측정 | 교체 직후 | 조명 비용 조정 후 |
|---|---:|---:|---:|
| 분석 프레임 | 3,212 | 3,115 | 3,204 |
| 프레임 시간 중앙값 | 12.434ms | 13.439ms | 12.857ms |
| 프레임 시간 p95 | 17.091ms | 18.611ms | 18.777ms |
| GPU 시간 중앙값 | 11.788ms | 12.859ms | 12.229ms |
| GPU 시간 p95 | 16.035ms | 17.439ms | 16.620ms |
| 드로 콜 중앙값 | 291 | 300 | 298 |
| 그린 삼각형 중앙값 | 22,760 | 18,852 | 18,664.5 |
| GPU 메모리 중앙값 | 2,941.4MiB | 2,943.8MiB | 2,943.5MiB |
| 33.33ms 초과 프레임 | 30 | 36 | 48 |
| 100ms 초과 프레임 | 1 | 6 | 3 |

조명 비용 조정으로 교체 직후보다 중앙값이 내려갔지만, 수정 전보다 약 0.42ms 더 든다. p95와 긴 지연은 개선됐다고 할 수 없다. 카메라가 다른 공간으로 바로 옮겨 가는 구간도 포함한 측정이며, 연속 보행과 패키지 빌드의 60fps를 보장하는 결과는 아니다. 이 부분은 남은 성능 과제다.

원본 CSV, 분석에 쓴 프레임, 요약을 모두 보관했다: [17일 기준](Performance/Household20260917-BeforeHigh/), [18일 기준](Performance/Household20260918-BeforeHigh/), [교체 직후](Performance/Household20260918-AfterHigh/), [동일 수정본 반복](Performance/Household20260918-AfterHighRepeat/), [최종](Performance/Household20260918-FinalHigh/). 반복 측정의 프레임 중앙값은 13.251ms였다.

## 확인 방법

침실 여섯 시점, 생활 소품 아홉 시점, 목격 소품 여섯 시점을 촬영한다. 침대 옆 통로와 눕기 조준, 실내화의 물리 안정성, 실외기 벽 접촉을 검사한다. 물그릇·약봉투·근무표·꽁초는 실제 조사 입력을 누르고 놓아 취소, 완료, 중복 기록 방지를 확인한다.

```powershell
pwsh -NoProfile -File Scripts/Run-BedroomReview.ps1 -Profile Household20260918
pwsh -NoProfile -File Scripts/Run-HouseholdReview.ps1 -Profile FinalLighting20260918
pwsh -NoProfile -File Scripts/Run-WitnessPropReview.ps1
pwsh -NoProfile -File Scripts/Run-HouseholdReview.ps1 -Measure -Profile FinalHigh20260918
pwsh -NoProfile -File Scripts/Validate-Project.ps1
```

`-Before`는 파일명과 검사 조건을 구분한다. 예전 코드와 에셋을 불러오는 옵션은 아니다. 입주 진행, 밤 진행, 조작·문 충돌·포획 검사도 실행했다. [검사 기록](Performance/Household20260918-checks.txt)에 결과와 캡처 해시를 모았다.
