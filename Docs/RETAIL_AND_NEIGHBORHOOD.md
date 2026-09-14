# 편의점과 골목 재검수

2026년 9월 14일 작업 기록. 9월 11일까지 공개된 자료를 우선 찾았고, 엔진 문서는 현재 설치된 UE 5.8과 대조했다. 아래 자료는 열람일이 같다고 해서 같은 시기에 나온 자료는 아니다.

## 사진에서 확인한 것

| 자료 | 게임에 반영한 부분 |
| --- | --- |
| [CU 점포 내부 사진](https://www.jumpoline.com/_jumpo/jumpo_view.asp?webjofrsid=594134) | 회녹색 계산대 상판, 금전함과 직원용 POS, 고객 표시기, 뒤쪽 담배장, 흰 집기 |
| [GS25 마산반월중앙점 사진](https://www.daangn.com/kr/local-profile/gs25-%EB%A7%88%EC%82%B0%EB%B0%98%EC%9B%94%EC%A4%91%EC%95%99%EC%A0%90-24%EC%8B%9C%EC%98%81%EC%97%85-g2yb7z5eo9k4/) | 품목별 진열 구획, 앞줄 맞춤, 밝은 바닥과 천장, 통로를 비우는 집기 배치 |
| [제주삼다수 제품](https://www.jpdc.co.kr/samdasoo/products.htm) | 페트병의 목과 어깨, 손으로 잡는 부분의 가로 리브, 얇은 라벨 |
| [농심 포테토칩](https://brand.nongshim.com/potatochip/main/index) | 봉지의 밀봉선과 공기층, 인쇄 면적, 내용물 사진과 제품명의 비율 |
| [86g 사발면 제품 사진](https://mfront.homeplus.co.kr/item?itemNo=120077262&storeType=EXP) | 낮고 넓은 흰 용기, 종이 뚜껑, 뜯는 탭 |
| [SMAD 35L 온장고](https://www.smadgroup.com/products/35l-straight-glass-countertop-hot-food-display-warmer) | 554×361×311mm 비례, 유리 케이스, 철망 선반, 금속 받침과 고무발 |

사진은 관찰용으로 사용했다. 게임의 상호는 새벽24이고, 포장 원화와 간판은 별도로 만들었다. 매장 치수는 사진에서 실측한 값이 아니다. 플레이어 캡슐, 일반적인 집기 크기, 실제 화면을 함께 보며 정했다.

실제 사진을 입력한 집기 시트와 상품의 여러 각도 자료는 `Content/SourceArt/AI/RetailFixtures_20260914.png`, `generated-images/retail-turnaround-20260914.png`에 있다. 포장 원화는 `Content/SourceArt/Labels/Store/RetailPackaging.png`다. 한글 간판·물병 라벨·가격표는 맑은 고딕으로 조판했다.

## 바뀐 공간과 사물

계산대는 224×64×93cm, 진열대는 240×77.2×169cm다. 매장 바닥을 포함한 계산대 상판 높이는 99cm다. 계산대 앞은 약 110cm, 진열대 사이에는 약 162cm를 남겼다. 진열대 선반은 30cm 간격이며, 같은 상품을 네 개씩 묶고 두 줄로 놓았다. 가격표도 해당 품목 아래에 붙는다. 사발면 두 종류, 봉지 과자 세 종류, 비스킷, 삼각김밥과 음료를 구획별로 나눴다.

계산대 끝 온장고는 손님에게 불투명한 옆판이 보이던 방향을 고쳤다. 제조사 사진을 보고 유리 케이스, 철망 선반, 경첩, 손잡이와 직원 쪽 조절부를 다시 만들었다. 새벽에는 비어 있고 전원이 꺼져 있다. 너비에 맞춰 상판 안쪽으로 10cm 옮겼다. 유리 내부가 불투명하게 나오던 Blender 미리보기도 유리 재질이 있을 때 Cycles를 쓰도록 바꿨다.

나린은 정면 원화에서 만든 163cm 인물이다. 얼굴은 원본 색을 다시 투영해 다듬었다. 이때 묶은 머리의 앞면에도 눈과 입이 찍히던 오류를 발견해, 사진이 입혀지는 깊이를 얼굴 앞쪽으로 제한했다. 머리 뒤쪽은 원래 색을 유지한다. 정면과 양옆 55도, 뒤쪽 100도 확대 화면은 `Content/SourceArt/Blender/SK_NarinClerk/SK_NarinClerk_head_*.png`에 남겼다. 12,000개 삼각형, 3개 본, 6초짜리 호흡 동작을 사용하며 가까운 손님 쪽으로 몸을 조금 돌린다.

남쪽 건물 사이에 두 통로를 열고, 뒷길을 편의점 옆길과 연결했다. 뒷길에는 세탁소 셔터, 계량기, 실외기, 배송 안내문이 있다. 안내문을 읽으면 나린에게 도하의 택배를 물어볼 수 있다. 읽은 사실과 질문한 사실은 저장된다. 뒤쪽 문 닫히는 소리는 해당 위치에서 한 번만 나며, 화면이나 시선을 강제로 돌리지 않는다.

설비실의 자재 덩어리는 각목 받침 위에 얇은 판재 85장을 쌓은 형태로 바꿨다.
[Knauf의 석고보드 자료](https://knauf.com/en-GB/p/product/wallboard-12.5mm-10003_0306)에서 원지와 절단면을 확인해 아이보리 종이 면과 회백색 석고 면을 구분했다. 재료가 한 번에 금속 상자처럼 보이지 않도록 장마다 색을 조금 달리하고 같은 ISM을 사용한다. 문 개구부의 옆면도 투영 방향을 고쳤다. 공동의 긁힌 자국은 실제 뒷벽에 붙였고, 발견 장면을 가리던 배관은 옆으로 옮겼다.

최종 발견 장면의 유해도 다시 제작했다. 기존 다면 원화에서 배경이 분리된 정면 이미지를 만들고 TRELLIS.2를 거쳐 Blender에서 12,000개 삼각형으로 정리했다. 3mm 복셀 처리로 머리 주변의 작은 조각을 합쳤고, 정면·측면·실제 손전등 화면을 확인했다. 접은 방수포는 발밑에 눕히고, 카트 바퀴는 별도로 놓았다. 시선 판정 지점도 새 형상의 머리·가슴·신발 높이에 맞췄다. 원본과 생성 기록은 `Content/SourceArt/AI/FinalCavityFront_20260914.png`와 `Content/SourceArt/Generated/FinalCavityRemains/front-20260914/`에 있다.

## 글과 소리

나린은 플레이어의 질문에 답한다. 처음부터 사건의 의미를 설명하던 대사를 줄였다. 황순금도 직접 들은 것과 기억하는 것까지만 말한다. 조율 수첩은 업무 일정과 벽을 확인한 메모로, 소리 일지는 날짜별 기록으로 고쳤다. 게시글·배송 라벨·부동산 문자·영수증·엔딩 보도에서도 작성자가 알 수 없는 결론이나 지나친 설명을 걷어냈다.

목표 문구에 남아 있던 “마실 물을 해결하자”는 “마실 물을 구하자”로, 결제 뒤의 불필요한 말줄임은 “집으로 돌아가자”로 고쳤다.

도입부의 여덟 항목은 순서와 관계없이 확인할 수 있다. 옥상 문부터 확인했다고 바로 잠들 수 있던 진행 오류도 고쳤다. 뒷골목의 배송 안내문은 필수 여덟 항목에 들어가지 않는다. 초반 목표 문구는 자유롭게 둘러보도록 안내하고, 절반 이상 확인한 뒤에는 남은 일을 알려 준다.

매장 음악은 볼륨과 들리는 범위를 줄이고 반복 사이에 45초를 비웠다. 냉장고 소리는 냉장고 위치에서 들린다. 노크와 발소리를 매장 음악이 멀리서 계속 덮지 않게 조정했다. 공포 연출의 참고는 [Frictional의 2013년 설계 글](https://frictionalgames.com/2013-10-useful-tips-for-horror-game-designers/)과 [2023년 The Bunker 업데이트](https://frictionalgames.com/2023-10-amnesia-the-bunker-halloween-update-launch/)다. 이동과 시선을 유지한 채 소리의 출처를 직접 찾게 하는 쪽으로 적용했다.

## 반입과 성능

[UE 5.8의 ISM 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine)에서 개별 인스턴스의 LOD 지원을 확인했다. 반복 진열물 1,301개는 23개 ISM으로 묶고, 충돌과 개별 Tick을 없앴다. 상품에는 단계별 LOD를 적용하며 16~22m 거리 컬링을 유지한다. [가시성·차폐 문서](https://dev.epicgames.com/documentation/unreal-engine/visibility-and-occlusion-culling-reference-in-unreal-engine?lang=en-US)도 함께 확인했다. 가까이서 집어 드는 물건은 실제 입체를 유지한다.

검수 중 발견한 반입 오류도 고쳤다. 베이크 UV가 FBX의 첫 채널에 없던 문제, 좌우로 뒤집힌 인쇄, 인스턴스용 재질 플래그 누락, 끊어진 벽 셰이더 연결, 간판 원본만 바뀌고 게임 텍스처는 갱신되지 않던 문제다. 캡처 검사는 재질 오류가 있으면 실패한다. 물병 띠의 상품명이 옆을 보던 회전을 고쳤고, 공용 페이지에서 라벨이 차지하는 높이(1/8)에 맞춰 해당 컴포넌트의 스트리밍 거리를 조정했다. 전체 텍스처를 강제로 올린 진단 실행과 기본 설정을 비교해 흐림의 원인을 확인했다. 기본 설정에서 다시 촬영한 결과에도 상품명이 읽힌다. 근거는 [Epic의 텍스처 스트리밍 데이터 문서](https://dev.epicgames.com/documentation/unreal-engine/building-texture-streaming-data-in-unreal-engine?lang=en-US)다.

온장고를 다시 반입한 뒤 금속 받침이 검게 나오는 문제를 더 찾았다. `DIFFUSE / COLOR`로 구우면 금속성 1인 표면의 색이 0으로 저장됐다. 금속성 채널이 높은 영역과 검은 색 텍스처를 대조해 [28개 메시](retail-metal-color-before.json)를 확인했다. 온장고·싱크대·후드·조명 테두리·진열대·문손잡이 등이 해당했다. 직접 굽는 경로와 고밀도 모델에서 옮기는 경로 모두 `Base Color` 입력을 발광 패스로 옮겨 색 자체를 굽도록 수정했다. 관련 빌더 12개를 다시 실행하고 에셋 66개를 게임에 반입했다. `Scripts/audit_metal_base_color.py`로 재검사한 [후보 목록](retail-metal-color-after.json)은 0개다. 256×256 샘플에서 밝은 AO 영역 중 금속성이 높은 면을 추려 집계했다. [수정 전](Media/retail-metal-color-before.png)과 [수정 후](Media/retail-metal-color-after.png) 게임 화면에서 금속 받침과 담배장 프레임을 비교할 수 있다. 금속성 0과 1의 색이 같게 보존되는지는 `Scripts/blender/check_base_color_bake.py`에서 실제 Cycles 베이크로 검사한다. 두 경로 모두 금속성 0과 1의 결과가 같은 RGB 값으로 나와 통과했다. `Build-BlenderAssets.ps1`은 이 검사를 먼저 실행하고, Python 오류도 실패 코드로 처리한다. [Blender의 Principled BSDF](https://docs.blender.org/manual/en/latest/render/shader_nodes/shader/principled.html)와 [베이크 패스 문서](https://docs.blender.org/manual/en/latest/render/cycles/baking.html)를 함께 확인했다.

공용 `M_IGBakedProp`에는 컴파일 오류 없이 남아 있던 문제도 있었다. ORM 기본값으로 넣은 흑백 노이즈 때문에 샘플러가 `Linear Grayscale`로 바뀌어, R의 자기 폐색 값을 거칠기와 금속성에도 복제했다. 원본과 UE에서 다시 내보낸 ORM의 B는 모두 0인데 게임의 Metallic 버퍼에서는 유해가 하얗게 보였다. 기본값을 실제 ORM으로 바꾸고 `Masks` 샘플러를 적용했다. 새 반입에서도 텍스처를 먼저 준비하고 샘플러를 확인한다. 이 수정은 같은 공용 재질을 사용하는 집기·상품·인물에도 적용된다.

[수정 전 금속성 화면](Media/retail-material-metallic-before.png)과 [수정 후 화면](Media/retail-material-metallic-after.png)에서 비교할 수 있다. 흰색은 금속, 검은색은 비금속이다. 수정 후에도 철제 기둥·배관·바퀴는 흰색으로 남으며, 뼈와 의복만 검게 표시된다.

```powershell
pwsh -NoProfile -File Scripts/Create-RetailGraphics.ps1
pwsh -NoProfile -File Scripts/Build-BlenderAssets.ps1 -Only retail_refresh
pwsh -NoProfile -File Scripts/Import-BlenderAssets.ps1
pwsh -NoProfile -File Scripts/Import-RetailRefresh.ps1
pwsh -NoProfile -File Scripts/Build-ArtAssets.ps1 -CodeOnly
pwsh -NoProfile -File Scripts/Run-RetailReview.ps1
pwsh -NoProfile -File Scripts/Run-RetailReview.ps1 -Measure
pwsh -NoProfile -File Scripts/Run-Prologue-Capture.ps1
pwsh -NoProfile -File Scripts/Run-MissingFloor-ArrivalProbe.ps1
pwsh -NoProfile -File Scripts/Validate-Project.ps1
```

메시 반입과 재질 반입은 같은 임시 프로젝트를 사용하므로 차례로 실행한다. 성능 측정은 PNG 저장을 끈 별도 실행이다. 편집기에서의 한 대 측정과 여러 장비의 Shipping 검증은 구분한다. 공포의 강도와 대사의 자연스러움은 자동 검사만으로 확정할 수 없다.

## 실행 결과

UE 5.8 에디터 빌드와 `Validate-Project.ps1`을 통과했다. 도입부 여덟 항목의 자유 순서 진행, 배송 안내문 분기, 전체 야간 진행 검사도 통과했다. 실제 이동 캡처는 두 골목과 편의점 옆길을 모두 걸어서 통과했다. 최종 재질로 편의점 5장, 도입부 12장, 야간 16장을 다시 저장했다. 이동 제동·작은 사물 조준·가림 판정·문 충돌·캐릭터 LOD 검사는 30/60/120fps에서 통과했다.

최종 RTX 3060 8GB / 1080p 측정은 프레임 시간 중앙값 21.046ms, 95백분위 23.447ms다. 약 48fps에 해당하며 60fps 고정에는 못 미친다. [원본 CSV와 측정 조건](Performance/Retail-20260914/README.md)에 수치와 재검수 중 발생한 GPU 오류 한 차례를 함께 기록했다.
