# 인쇄 방향과 소품 형상 재점검 — 2026-09-16

지적한 화면은 앞서 저장한 게임 캡처였다. 실행 파일이 오래돼서 생긴 문제로 돌릴 수 없다. 앞면 글씨만 보고 통과시켰던 검수를 보완해 네 종류의 삼각김밥 앞·뒷면, 진열 상태, 사발면 밑면, 별관의 단서 소품을 다시 확인했다.

## 이번에 고친 부분

| 대상 | 발견한 문제 | 변경 |
| --- | --- | --- |
| 삼각김밥 4종 | 뒷면 라벨의 사다리꼴 메시가 인쇄 첫 글자를 잘랐다. 전체 메시 반전으로 인쇄를 맞추면 개봉 탭 위치까지 뒤집힌다. | 뒷면 인쇄 영역을 직사각형으로 확보했다. 인쇄 UV만 보정하고 탭 1·2·3의 위치는 따로 배치했다. |
| 페인트 통·수첩 | 물체 전체를 뒤집는 방식이 손잡이와 책등 방향에도 영향을 줬다. 수첩 이름표는 인쇄판의 뒷면이 위를 향했다. | 인쇄 UV 보정으로 구조와 인쇄를 분리했다. 이름표는 인쇄된 앞면이 위를 향하도록 돌렸다. |
| 사발면 2종 | 용기 주변의 수직 막대 48개가 좁아지는 몸통 밖으로 튀어나와 밑면이 빗살처럼 보였다. | 용기 표면에 0.35mm 깊이의 성형 홈을 붙여 만들었다. 개당 삼각형 수는 2,564개에서 2,244개로 줄었다. |
| 조율 렌치 | 검은 손잡이와 작은 상자처럼 생긴 머리 사이의 축이 제대로 보이지 않았다. | 목재 손잡이, 이어진 금속 축, 굽은 목과 별 모양 소켓을 다시 만들었다. 단일 구운 재질, 1,700삼각형, LOD 4단계다. |
| 가벽의 손자국 | 여러 손이 도장처럼 반복됐고, 얇은 상자 옆면에 인쇄가 늘어나 검은 테두리가 생겼다. | 접촉이 끊긴 손바닥 흔적 두 개를 생성해 평면에 배치했다. 벽이 열리면 함께 숨겨지는 동작은 유지했다. |
| 별관 판재 더미 | 얇은 판 85장이 각각 충돌·그림자·거리장 계산에 참여했다. | 보이는 판은 유지하고 더미 3개에 단순한 충돌·그림자 형상을 하나씩 쓴다. 작은 석고 조각도 거리장 계산에서 제외했다. |

조율 렌치를 살펴볼 때의 독백은 “오빠 렌치다. 늘 공구 가방에 넣어 두던 건데.”로 바꿨다. 물건을 알아보는 이유를 대사에 남겼다.

## 사진과 제작 자료

- [Howard Piano Industries A-5A 실물 사진과 치수](https://www.howardpianoindustries.com/piano-tuning-hammer/): 목재 손잡이와 금속 축의 연결, 열린 별 소켓, 굽은 각도를 확인했다. 사진 두 장을 imagegen에 전달해 같은 물체의 여러 방향 참고도를 만든 뒤 Blender 모델에 반영했다.
- [농심 김치사발면 공식 사진](https://brand.nongshim.com/kimchiramyun/main/index): 아래로 좁아지는 흰 용기와 얕은 성형 홈을 확인했다. 기존 게임용 뚜껑 인쇄는 유지했다.
- [벽에 남은 실제 손자국](https://unsplash.com/photos/a-group-of-hand-prints-on-a-wall-8LzW2EliVk8): 손바닥 전체가 고르게 찍히지 않는 접촉 형태를 참고했다. 사진 자체를 게임에 넣지 않고 별도 마스크를 생성했다.
- [조율 렌치 생성 참고도](../Content/SourceArt/AI/TuningLeverStudy_20260916.png), [압흔 마스크](../Content/SourceArt/AI/AnnexPressureMask_20260916.png), [사용한 프롬프트](../Content/SourceArt/AI/PrintReviewPrompts_20260916.json). imagegen의 내장 도구로 생성했다. 참고도는 게임 캡처가 아니다.

2026-09-16에 확인한 [UE 5.8 Lumen 성능 안내](https://dev.epicgames.com/documentation/unreal-engine/lumen-performance-guide-for-unreal-engine)는 거리장에 참여하는 인스턴스 수와 겹침을 줄이는 방법을 설명한다. [Virtual Shadow Maps 안내](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)도 그림자를 그리는 물체 수와 캐시 비용을 다룬다. 이 장면에서는 판재를 더미 단위로 묶는 방법을 선택했다. 작은 소품 전부를 고비용 그림자나 거리장으로 처리할 필요는 없다는 판단이다.

[Silent Hill: Townfall 개발진 인터뷰(2026-07-30)](https://blog.playstation.com/2026/07/30/silent-hill-townfall-developers-discuss-the-scottish-setting-retro-technology-first-person-combat/)도 확인했다. 실제 장소의 구체적인 생활 흔적과 플레이 중 사용하는 물건에 집중한다는 설명을 참고했다. 이번 수정 범위는 화면에서 어색했던 물건과 흔적, 렌치의 독백이다. 이 작업으로 전체 스토리나 퍼즐 구성을 새로 만들었다고 보지는 않는다.

## 실제 게임 화면

아래 파일은 UE 게임 실행에서 저장한 1920×1080 캡처다. `print-before-*`가 수정 전, `print-*`가 수정 후다. 삼각김밥 앞·뒷면 여덟 장은 실제 게임용 메시를 진열대 앞에 옮겨 돌려 보는 **검사용 배치**다. 정상 진열 상태는 `rice-shelf`와 `rice-distance`에서 확인한다.

| 확인할 부분 | 수정 전 | 수정 후 |
| --- | --- | --- |
| 삼각김밥 뒷면 글자 잘림 | [전주비빔](Media/print-before-rice-b-back.png) | [전주비빔](Media/print-rice-b-back.png) |
| 앞면 글씨와 개봉 번호 | [참치마요](Media/print-before-rice-a-front.png) | [참치마요](Media/print-rice-a-front.png) |
| 정상 진열 | [냉장 진열대](Media/print-before-rice-shelf.png) | [냉장 진열대](Media/print-rice-shelf.png) |
| 사발면 밑면 | [용기](Media/print-before-store-floor.png) | [용기](Media/print-store-floor.png) |
| 조율 렌치 | [근거리](Media/print-before-wrench-near.png) | [근거리](Media/print-wrench-near.png) |
| 가벽의 손자국 | [벽](Media/print-before-wall-traces.png) | [벽](Media/print-wall-traces.png) |

수첩 이름표는 [별도 근접 화면](Media/print-notebook-close.png)에서도 확인한다. 이 한 장은 캐릭터 충돌과 분리한 검사 카메라로 실제 배치된 표지에 접근해 찍는다. 표지 텍스처는 512px에서 1024px로 올렸다.

기존 근접 검사 마지막 네 시점도 같은 방식으로 바꿨다. 낮춘 카메라가 캐릭터 충돌에 밀려 비닐 대신 옆의 카트를 찍던 문제가 있었다. 성능 비교에 쓰는 앞의 12개 시점은 그대로다.

편의점·로비·4층 복도·별관 바닥과 엘리베이터 앞도 촬영했다. 이 시점들에서는 타일의 뒤집힘이나 떠 있는 모서리는 보이지 않았다. 바닥 전체를 새로 만들지는 않았다.

## 성능 측정

같은 PC에서 같은 12개 시점을 순서대로 촬영하되, 이미지 저장은 끄고 CSV 프로파일만 남겼다. Ryzen 9 7900X, RTX 3060 8GB, Windows 11, UE 5.8 Development, High, 1920×1080, 내부 해상도 100%, 수직 동기화와 프레임 제한 해제 조건이다. 처음 120프레임은 집계에서 뺐다. 수정 전 기준은 `3362f79`다.

| 측정 | 분석 프레임 | 프레임 중앙값 | 프레임 p95 | GPU 중앙값 | 드로 콜 중앙값 | 33.33ms 초과 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| [수정 전](Performance/Print20260916-BeforeHigh/summary.json) | 4,290 | 14.121ms | 16.612ms | 13.492ms | 319 | 0 |
| [수정 후](Performance/Print20260916-AfterHigh/summary.json) | 4,160 | 14.548ms | 16.936ms | 14.033ms | 317 | 0 |
| [수정 후 재측정](Performance/Print20260916-AfterHighRepeat/summary.json) | 3,981 | 14.576ms | 17.730ms | 13.957ms | 320 | 94 |
| [그림자 설정 비교용 실행](Performance/Print20260916-CoarsePagesTrial/summary.json) | 4,234 | 14.259ms | 17.781ms | 13.764ms | 317 | 1 |

판재의 계산 대상과 사발면 삼각형은 줄었지만, 전체 프레임 시간은 개선되지 않았다. 수정 후 두 번 모두 중앙값이 약 3% 늘었다. 재측정에서는 최대 60.193ms 지연도 나왔다. 그 구간은 GPU 시간이 함께 늘었으나, 원인은 이번 측정만으로 확인하지 못했다. 따라서 이번 변경을 FPS 개선으로 설명하지 않는다.

비교용 실행에서는 `r.Shadow.Virtual.NonNanite.IncludeInCoarsePages=0`만 추가했다. UE 5.8 엔진 코드에 남아 있는 옵션이며, 저해상도 그림자 페이지에서 일반 메시를 제외한다. 중앙값은 조금 줄었지만 p95는 나아지지 않았고, 안개와 반투명 물체가 쓰는 그림자 정보에도 영향을 줄 수 있어 프로젝트 설정에는 반영하지 않았다. 각 폴더에 요약, 프레임별 CSV, 전체 프로파일 압축본을 함께 보관했다.

## 재현

최종 코드와 에셋으로 Win64 Development 빌드, 프로젝트 검사, 진행·단서·추격·결말 검사, 시작 경로와 통로·문·표적 24개 검사, 30·60·120fps 이동·충돌·잡힘 검사를 통과했다. 단서 세 개의 밑면과 판재 윗면 차이는 수첩 +0.010cm, 렌치 0.000cm, 장갑 +0.085cm였고, 모두 시선 판정에 맞았다. 근접 캡처 16장과 인쇄·형상 캡처 19장을 저장했다.

```powershell
& ./Scripts/Build-BlenderAssets.ps1 -Only @('detail_props', 'retail_refresh', 'tuning_tool')
pwsh -NoProfile -File Scripts/Import-BlenderAssets.ps1 -Only SM_TriangleKimbapA,SM_TriangleKimbapB,SM_TriangleKimbapC,SM_TriangleKimbapD,SM_WorkPaintCan,SM_TunerNotebook,SM_RetailCupBeef,SM_RetailCupKimchi,SM_TuningHammer
pwsh -NoProfile -File Scripts/Import-PressureMarks.ps1
pwsh -NoProfile -File Scripts/Build-ArtAssets.ps1 -CodeOnly
pwsh -NoProfile -File Scripts/Run-PrintShapeReview.ps1
pwsh -NoProfile -File Scripts/Run-DetailReview.ps1
pwsh -NoProfile -File Scripts/Validate-Project.ps1
```
