# 편의점 포장과 공사 구역 근접 점검

이 검수에서 놓친 뒷면 인쇄·수첩 이름표·사발면 형상은 [후속 점검](PRINT_SHAPE_REVIEW_20260916.md)에서 고쳤다. 아래 성능 표는 첫 수정 당시의 기록이다.

2026년 9월 16일. 비교 기준은 `795561d`다. 이번에는 냉장 진열대, 관리실 자재, 5층 공사 구역을 가까이서 확인했다. 아래 비교 사진은 같은 위치에서 촬영한 실제 게임 화면이다.

## 포장과 생활 소품

삼각김밥 네 종류의 글씨가 좌우로 뒤집혀 있었다. 포장도 검은 삼각기둥에 인쇄만 붙인 모양이었다. 실제 포장 사진에서 접힌 밑면, 뜯는 띠, 작은 비닐 탭을 확인하고 imagegen으로 앞·뒤·사선 참조와 김 표면을 만들었다. Blender에서 모서리를 둥글리고 포장 끝을 접었다. 제품명과 보관 안내는 폰트로 조판했다.

| 이전 | 수정 후 |
| --- | --- |
| ![뒤집힌 상품 글씨](Media/readme/detail-before-kimbap-front.webp) | ![포장과 글씨 방향 수정](Media/readme/detail-kimbap-front.webp) |

진열된 48개는 네 종류의 ISM을 사용한다. 포장의 반사는 거칠기와 노멀맵으로 표현하며, 투명 껍질을 한 겹 더 그리지 않는다. 가까이서 읽는 포장과 도료 통은 1K 텍스처를 사용한다.

관리실의 페인트 통은 뚜껑도 손잡이도 없는 원통이었다. 4L 용기 제작 규격인 지름 167mm, 높이 190mm를 참고해 뚜껑과 말린 테두리, 내려놓은 철사 손잡이를 만들었다. 별도 라벨 면이 먼 거리의 LOD에서 본체에 파묻히는 문제도 발견해 본체 표면에 직접 인쇄하도록 바꿨다.

![도료 통 근접 화면](Media/readme/detail-can-label.webp)

## 공사 자재와 단서

석고보드는 종이 겉지와 흰 코어를 구분했다. 국내 12.5T 규격을 참고한 절단재이며, 관리실의 세워 둔 판재와 5층의 세 자재 더미에 함께 사용한다. 적재된 85장은 ISM 한 개로 묶는다. 바닥에는 같은 큰 파편 무늬가 반복되고 있었는데, 사진 기반 콘크리트를 바탕으로 쓰고 자재 주변에 얇은 파편을 놓았다. 파편은 한 묶음의 메시를 재사용하며 플레이어 충돌은 없다.

| 이전 | 수정 후 |
| --- | --- |
| ![민무늬 판재와 종이 상자 모양의 단서](Media/readme/detail-before-annex-floor.webp) | ![자재 단면과 실제 형태를 갖춘 단서](Media/readme/detail-annex-floor.webp) |

‘작업 장갑’은 종이 상자 모양으로 남아 있었다. 실제 면장갑 사진과 imagegen 삼면도를 참고해 빈 손가락 다섯 개, 편직 무늬, 뚫린 손목을 가진 모델로 교체했다. 수첩에도 표지·책등·종이 단면·이름표를 만들었다. 수첩과 조율 렌치는 없어진 받침대의 높이에 남아 있어 새 자재 윗면으로 내려놓았다.

![장갑과 수첩](Media/readme/detail-glove-close.webp)

보양 비닐은 얇은 상자를 기울인 형태에서 주름과 처진 가장자리를 가진 한 겹의 메시로 바꿨다. 바닥 비닐의 모서리를 높이 기울이지 않고 바닥에 눕혔다. 비닐 네 장만 표면 반사를 계산하는 반투명 재질을 사용하며 그림자와 충돌은 끈다. 상품 포장에는 이 재질을 사용하지 않는다.

## 참고한 자료와 원본

자료는 9월 16일에 확인했다. 발표 시점과 내용이 확인되는 공식 기술 문서와 개발자 인터뷰를 골랐다. 장르의 유행을 하나로 단정하기보다, 눈앞의 사물이 이야기와 조사 내용에 맞는지에 적용했다.

- [삼각김밥 실제 포장 사진](https://www.fatsecret.kr/칼로리-영양소/cu/참치마요-삼각김밥/1개): 포장의 접힘과 개봉 구조. 게임의 상표와 라벨은 별도로 만들었다.
- [일반 석고보드 적재 사진](https://m.gundalin.com/product/일반-석고보드/126/), [KCC 석고보드 자료](https://www.kccworld.co.kr/data/product/KCC_%EC%84%9D%EA%B3%A0%EB%B3%B4%EB%93%9C_%EC%B9%B4%EB%8B%A4%EB%A1%9C%EA%B7%B8.pdf): 종이 겉지·흰 절단면·판재 두께.
- [4L 용기 제작 규격](https://www.paintstins.com/sale-41373209-1-gallon-white-paint-tin-cans-round-container-4-liter-package-metal-cans.html), [국내 판매 도료 통 사진](https://global.gmarket.co.kr/item?goodsCode=1954558316): 용기 비례와 접힌 손잡이.
- [면장갑 제품 사진](https://prod.danawa.com/info/?pcode=9812046): 손가락 길이, 편직 방향, 손목 마감.
- [UE 5.8 ISM](https://dev.epicgames.com/documentation/unreal-engine/instanced-static-mesh-component-in-unreal-engine): 같은 메시와 재질을 공유하는 자재·파편·상품을 묶고 인스턴스마다 거리와 LOD를 처리한다.
- [UE 텍스처 스트리밍](https://dev.epicgames.com/documentation/unreal-engine/texture-streaming-overview-for-unreal-engine): 표면의 화면 크기와 UV 밀도를 기준으로 필요한 밉을 선택한다. 작은 소품 전체를 무작정 4K로 올리지 않았다.
- [UE 5.8 반투명 조명 방식](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/ETranslucencyLightingMode): 보양 비닐의 표면 반사를 위한 설정. 비용은 수정 전후 같은 경로로 측정한다.
- [Silent Hill: Townfall 개발자 인터뷰, 2026-07-30](https://blog.playstation.com/2026/07/30/silent-hill-townfall-developers-discuss-the-scottish-setting-retro-technology-first-person-combat/): 익숙한 실제 장소, 직접 다루는 도구, 화면 밖 소리의 역할을 설명한다. 이번에는 단서의 이름과 형태, 물건을 놓은 이유가 맞는지 점검하는 기준으로 참고했다.

기본 imagegen 도구를 사용했다. 생성 원본 다섯 장과 프롬프트는 [포장·판재 기록](../Content/SourceArt/AI/DetailMaterials_20260916.json), [장갑 기록](../Content/SourceArt/AI/DetailGlovePrompts_20260916.json)에 모았다. 모델과 구운 텍스처는 `Content/SourceArt/Blender/`에 있다. 외부 상품 사진은 참조에만 쓰고 배포 파일에는 넣지 않았다.

재생성은 `Scripts/Build-BlenderAssets.ps1 -Only detail_props`, 반입은 `Scripts/Import-BlenderAssets.ps1 -Only`에 기록의 메시 이름을 전달한다. 예전 `store_products` 전체 빌드가 수정 전 삼각김밥을 다시 덮어쓰던 경로도 정리했다.

생성 원본: [포장 삼면도](../Content/SourceArt/AI/KimbapConstructionStudy_20260916.png), [김 표면](../Content/SourceArt/AI/KimbapFilmAlbedo_20260916.png), [석고보드 겉지](../Content/SourceArt/AI/GypsumPaperAlbedo_20260916.png), [장갑 삼면도](../Content/SourceArt/AI/WorkGloveStudy_20260916.png), [면 편직](../Content/SourceArt/AI/WorkGloveKnit_20260916.png).

## 화면과 성능 확인

`Run-DetailReview.ps1`로 실제 게임의 16개 시점을 1920×1080으로 촬영했다. 마지막 네 장은 라벨·장갑·단면·비닐을 보기 위해 카메라를 낮춘 검수 화면이다. 시작 자막이 첫 촬영 위치를 덮어쓰던 문제를 발견해 사진 촬영은 시작 연출이 끝난 뒤 진행하도록 했다. 수정 전 잘못 찍힌 첫 사진은 비교 자료에서 제외했다.

삼각김밥 네 종류와 새 소품 일곱 개를 반입했다. 최고 단계의 삼각형 수는 포장 각 718개, 판재 156개, 도료 통 2,768개, 장갑 2,874개, 수첩 504개, 파편 묶음 404개, 비닐 각 864개다. 소품 LOD 네 단계를 만들고, 파편은 9~14m에서 거리 컬링한다. 가까이서 글씨를 읽는 포장과 도료 통도 텍스처 크기를 1K로 제한했다.

성능은 사진 저장을 끈 상태에서 기존의 12개 시점을 같은 순서로 돌며 측정했다. RTX 3060 8GB, Ryzen 9 7900X, Windows 11, UE 5.8 Development 실행이다. 처음 120프레임을 제외했고 VSync와 프레임 제한은 껐다. 측정 중 다른 빌드·모델 렌더링·게임 검사는 실행하지 않았다.

| 항목 | 수정 전 높음 | 수정 후 높음 | 수정 후 높음 재측정 | 수정 후 성능 설정 |
| --- | ---: | ---: | ---: | ---: |
| 분석 프레임 | 4,348 | 4,004 | 3,866 | 7,544 |
| 프레임 시간 중앙값 | 13.829ms | 15.182ms | 15.217ms | 7.922ms |
| 프레임 시간 95백분위 | 16.389ms | 17.373ms | 18.632ms | 10.103ms |
| 가장 느린 프레임 | 28.486ms | 27.943ms | 40.960ms | 26.450ms |
| 33.33ms 초과 프레임 | 0 | 0 | 56 | 0 |
| GPU 시간 중앙값 | 13.307ms | 14.556ms | 14.692ms | 6.859ms |
| 드로 콜 중앙값 | 313 | 317 | 317 | 286 |
| GPU 메모리 중앙값 | 2,993MiB | 2,992MiB | 2,993MiB | 2,423MiB |

높음은 내부 해상도 100%, 성능 설정은 71%다. 높음 설정의 프레임 시간은 약 10% 늘었다. 반투명 GPU 항목의 평균은 0.076ms에서 0.255ms로 늘었으며, 다른 렌더링 항목에서도 비용 증가가 관측됐다. 비닐 재질만으로 전체 증가를 설명할 수는 없다. 메모리는 거의 같았지만, 높음 재측정에서 56프레임의 일시적인 지연이 있었으므로 안정적인 60fps를 보장하는 결과로 해석하지 않는다.

이 수치는 이번에 점검한 구역의 고정 시점 측정이다. 전체 플레이의 최저 성능이나 출하 빌드 성능을 뜻하지 않는다. 원본 CSV와 요약은 [수정 전](Performance/Detail20260916-BeforeHigh/summary.json), [수정 후](Performance/Detail20260916-AfterHigh/summary.json), [높음 재측정](Performance/Detail20260916-AfterHighRepeat/summary.json), [성능 설정](Performance/Detail20260916-AfterPerformance/summary.json)에 보관했다.

## 진행·충돌 검사

아래 검사를 수정한 에셋과 코드로 실행해 통과했다.

- `Build-ArtAssets.ps1 -CodeOnly`: Win64 Development 에디터 빌드.
- `Run-DetailReview.ps1`: 실제 배치 수 확인, 단서 세 개의 지지면 높이와 시선 판정, 화면 16장 촬영. 수첩·렌치·장갑 밑면과 자재 윗면의 차이는 각각 +0.010cm, −0.020cm, +0.085cm였다.
- `Run-MissingFloor-Greybox.bat`: 밤별 진행, 단서 수집, 추격과 결말까지 전체 진행 검사.
- `Run-MissingFloor-ArrivalProbe.ps1`: 실제 시작 경로와 자유 조사 순서, 배달 분기, 통로·문·표적 24개 검사. 실패 0개.
- `Run-GameplayRealismProbe.ps1 -FrameRate 30 / 60 / 120`: 각 프레임 속도의 이동·충돌·잡힘 동작 검사. 세 실행 모두 실패 0개.
- `Run-MissingFloor-CctvFeedProbe.ps1`: D3D12에서 채널 5의 생성·해제와 실제 픽셀 확인. 최대 밝기 0.9807, 밝기 하한을 넘긴 픽셀 비율 0.2865.
- `Validate-Project.ps1`, `git diff --check`: 저장소 계약·에셋·배치·문서 검사와 공백 오류 검사.

새로 촬영한 사진과 비교용 사진의 표시용 WebP 20장은 합계 약 1.46MiB다. 원본 PNG는 `Docs/Media/`에 따로 보관했다.
