# 현관 인쇄물과 복도 표면 점검

2026년 9월 16일. 비교 기준은 `3dc4401`이다. 소화전 글씨와 현관 광고를 시작으로 방·복도·골목·편의점·옥상 화면을 살폈다. 아래 이미지는 Blender 미리보기가 아닌 실제 게임 캡처다.

## 현관 광고와 소화전 글씨

기존 소화전 모델의 인쇄가 좌우로 뒤집혀 있었다. Blender에서 글자 방향을 보정해 다시 굽고 반입했다. 본편 복도와 필로티에 쓰는 발신기·소화기도 가까이서 확인했다. 이전 소화전 모델은 별도 검수 위치에 놓아 확인했으며, 본편 설비를 다시 이 상자로 바꾸지는 않았다.

| 기존 모델의 인쇄 방향 수정 | 본편 복도 설비 |
| --- | --- |
| ![정방향으로 읽히는 소화전 글씨](Media/readme/door-print-legacy-fire.webp) | ![복도 발신기와 소화기](Media/readme/door-print-fire-corridor.webp) |

403호 광고는 방 안쪽에 붙은 17cm 정사각형 판이었다. 두께도 8mm나 됐고, 여러 업종의 문구를 한 장에 몰아넣었다. 국내 인쇄소의 실물 자석 광고 사진을 참고해 imagegen으로 새 인쇄물을 만들었다. 골목에 있는 ‘왕발통닭’의 90×65mm 광고 한 장으로 바꾸고, 두께는 0.6mm로 줄였다. 인쇄는 앞면에만 넣고 뒤와 옆은 고무색으로 처리했다.

광고는 복도 쪽 철판에 붙는다. 문을 열 때도 같은 피벗을 따라 움직이며, 실제 게임에서 닫힌 상태와 95도 열린 상태의 위치를 검사했다. 문 앞 바닥에 놓여 있던 16mm 두께의 가짜 종이도 없앴다.

| 이전: 방 안쪽의 두꺼운 광고판 | 수정: 복도 쪽의 작은 자석 광고 |
| --- | --- |
| ![이전 현관 안쪽](Media/readme/door-print-before-inside.webp) | ![현관 바깥쪽 자석 광고](Media/readme/door-print-outside.webp) |

## 벽에 붙은 종이와 읽기 판정

골목의 임대 안내문 두 장은 A4보다 훨씬 큰 58×78cm 판에 두께가 2cm였다. 그중 하나는 벽이 없는 틈에 떠 있었다. 실제 주택가 안내문 사진을 참고한 A4 인쇄물로 교체하고, 위쪽 모서리 두 곳에 작은 테이프를 붙였다. 종이는 0.15mm다. 두 장 모두 벽면에 닿는지 게임 충돌 검사로 확인했다. 임대 금액도 입주 계약서와 같은 보증금 500만 원·월세 40만 원으로 맞췄다.

| 안내문 앞면 | 옆에서 본 벽과 종이 |
| --- | --- |
| ![A4 임대 안내문](Media/readme/door-print-rental-front.webp) | ![벽에 붙인 얇은 종이](Media/readme/door-print-rental-side.webp) |

402호 메모는 화면에 아무 글씨도 없는 판을 읽게 하고 있었다. A6 크기의 손글씨 원본을 imagegen으로 만든 뒤, 실제 종이와 읽기 문구를 맞췄다. 메모에는 새벽에 위층에서 물건 끄는 소리가 들린다는 이웃의 말을 적었다.

메모를 겨눠도 ‘귀를 기울인다’가 먼저 잡히는 문제도 있었다. 보이지 않는 듣기 영역이 종이 앞을 가로막고 있었다. 듣기는 귀 높이, 읽기는 종이 위치로 나눴다. 종이를 두껍게 만들지 않고 별도의 얇은 상자로 읽기만 판정한다. 이 상자는 플레이어 이동을 막지 않는다. 듣기 후 화면에 없는 우편을 언급하던 독백은 삭제했다.

| 이전: 글씨 없는 메모 | 수정: 화면에서 읽히는 손글씨 |
| --- | --- |
| ![빈 메모가 붙은 이웃집 문](Media/readme/door-print-before-neighbor-note.webp) | ![402호 주민의 메모](Media/readme/door-print-neighbor-note.webp) |

방의 달력은 현재 입주 시점인 2025년 7월로 맞췄다. 날짜와 요일은 코드로 조판해 1일 화요일부터 31일 목요일까지 정확히 배치했다. 달력을 두꺼운 상자에 감싸던 방식도 얇은 종이 묶음과 상단 철심으로 바꿨다. 과거 사건 자료의 2024년 날짜는 유지했다.

![2025년 7월 달력](Media/readme/door-print-calendar.webp)

## 타일 위의 검은 자국

현관 앞 화강석에 검은 붓자국처럼 보이는 얼룩이 있었다. 어두운 콘크리트용 끌림 자국을 가로로 늘려 올려놓은 것이 원인이었다. 늘어짐을 없애고, 화강석 무늬와 줄눈이 비치는 옅은 자국으로 바꿨다. 색만 얹는 DBuffer 재질을 사용해 바닥의 요철과 거칠기를 덮어쓰지 않는다.

| 이전 | 수정 후 |
| --- | --- |
| ![현관 앞의 검은 줄무늬](Media/readme/door-print-before-floor.webp) | ![돌 무늬와 줄눈을 유지한 바닥](Media/readme/door-print-floor.webp) |

새 인쇄물 네 종류는 각각 재질 한 개, 512px 색상·노멀·거칠기 묶음을 사용한다. 광고와 메모는 12삼각형, 달력은 24삼각형, 테이프가 붙은 안내문은 36삼각형이다. 이 작은 면을 더 축약해 모서리가 무너지는 일을 피하고, 광고는 6.5m, 안내문은 14m 밖에서 숨긴다. 바닥 자국은 16m 밖에서 숨긴다. 종이와 광고에 불필요한 그림자·거리장·충돌을 붙이지 않았다.

## 참고 자료와 원본

실물 사진은 모양·크기·배치의 참고로 사용했다. 해당 사진을 게임 텍스처로 재배포하지 않는다. 생성 이미지와 프롬프트, 참조 페이지 주소는 저장소에 함께 보관했다.

- [두손기획 종이 자석 광고](https://dsp114.com/mlangprintauto/msticker/), [와우프레스 자석 제품](https://m.wowpress.co.kr/ordr/prod/dets?ProdNo=40093), [24프린트 치킨 광고 제품](https://24print.co.kr/shop/shopdetail.html?branduid=86&mcode=002&scode=&search=&sort=view&type=X&xcode=018): 국내 광고 인쇄의 구성과 얇은 고무 뒷면을 참고했다.
- [이코노미스트의 주택가 임대 안내문 사진](https://economist.co.kr/article/view/ecn202502190001): 흰 복사용지와 단순한 안내 문구, 실제 부착 모습을 참고했다.
- Epic의 [DBuffer 데칼](https://dev.epicgames.com/documentation/en-us/unreal-engine/decal-materials-in-unreal-engine), [메시 데칼](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-mesh-decals-in-unreal-engine), [거리·가림 판정](https://dev.epicgames.com/documentation/en-us/unreal-engine/visibility-and-occlusion-culling-in-unreal-engine), [Lumen 제약](https://dev.epicgames.com/documentation/unreal-engine/lumen-technical-details-in-unreal-engine): 9월 16일 확인한 UE 5.8 문서를 바탕으로 바닥 자국의 표면 처리와 작은 장식의 표시 거리를 정했다.
- [2026년 7월 30일 Townfall 제작진 인터뷰](https://blog.playstation.com/2026/07/30/silent-hill-townfall-developers-discuss-the-scottish-setting-retro-technology-first-person-combat/): 실제로 있을 법한 장소와 생활 물건이 이야기를 뒷받침하는 접근을 참고했다. 이번에는 광고의 상호, 계약 금액, 달력, 이웃의 메모가 같은 동네와 사건을 가리키도록 맞췄다.

원본과 생성 기록:

- [현관 광고](../Content/SourceArt/AI/DoorMagnet_20260916.png) · [프롬프트·출처](../Content/SourceArt/AI/DoorMagnet_20260916.json)
- [임대 안내문](../Content/SourceArt/AI/RentalNotice_20260916.png) · [프롬프트·출처](../Content/SourceArt/AI/RentalNotice_20260916.json)
- [402호 손글씨 메모](../Content/SourceArt/AI/NeighborMemo_20260916.png) · [프롬프트·출처](../Content/SourceArt/AI/NeighborMemo_20260916.json)
- [달력 조판](../Scripts/build_apartment_calendar.py) · [Blender 빌더](../Scripts/blender/build_neighborhood_prints.py)

## 게임 반영 확인

- 새 인쇄물 네 종류와 기존 소화전 모델을 Blender에서 내보내고 UE 에셋으로 반입했다. C++ Development Editor 빌드도 통과했다.
- 방·골목·편의점·복도·옥상 순회 화면 21장을 살펴봤다. 이 과정에서 찾은 빈 메모까지 수정한 뒤, 인쇄물 전용 화면 11장을 다시 촬영했다.
- 광고의 실제 크기·바깥 방향·문 부착 상태, 열린 문을 따라 움직이는 위치, 임대 안내문 두 장의 벽 접촉을 실행 중 검사했다.
- 402호 메모와 듣기 영역을 각각 겨냥하는 시선 검사, 입주 진행과 자유 탐색 순서, 문·통로의 캡슐 이동 검사 24개가 통과했다.
- 전체 밤 진행 검사와 `Scripts/Validate-Project.ps1`이 통과했다. 종이 두께 검사에는 예전의 8mm 광고·16mm 종이·20mm 안내판을 잡는 회귀 사례를 추가했다.

정적 인쇄면 검사는 저작 메시의 글자 방향까지 판정하지 않는다. 재질을 풀지 못하는 배치 26건과 복합 스캔 원본 6종도 자동 검사의 한계로 남아 있다. 이번 글자·표면·부착 방향은 게임 화면과 실행 중 검사로 별도 확인했다. 이번 이야기 수정은 생활 소품의 내용과 402호 상호작용에 해당한다.

[검사 결과 발췌](Performance/DoorPrint20260916-checks.txt)

## 성능 측정

RTX 3060 8GB·Ryzen 9 7900X·Windows 11에서 UE 5.8 Development 게임을 D3D12·1920×1080으로 실행했다. 높은 품질은 렌더 해상도 100%, 성능 설정은 71%와 TSR을 쓴다. 이전 작업과 같은 승강장·복도 순회 경로를 재생하고 첫 120프레임을 제외했다. 측정하는 동안 빌드·에셋 변환·다른 게임 실행을 겹치지 않았다.

| 설정 | 이전 중앙값 / p95 | 수정 후 중앙값 / p95 | 수정 후 GPU 중앙값 |
| --- | ---: | ---: | ---: |
| 높은 품질 | 16.61 / 19.57ms | 15.76 / 18.25ms | 15.23ms |
| 성능 | 7.55 / 9.22ms | 7.75 / 9.51ms | 7.20ms |

높은 품질의 중앙값은 0.85ms 줄었고, 성능 설정은 0.20ms 늘었다. 드로 콜 중앙값은 각각 393·350으로 이전과 같다. GPU 메모리 중앙값은 높은 품질 2,895MiB, 성능 설정 2,391MiB였다.

높은 품질은 여전히 60fps를 항상 유지하지 못한다. 분석한 3,991프레임 중 54프레임이 33.33ms를 넘었고, 최댓값은 35.59ms였다. 성능 설정의 최댓값은 23.04ms였으며 33.33ms 초과 프레임은 없었다. 중앙값 개선만으로 모든 구간의 끊김이 해결됐다고 볼 수는 없다. 두 설정 모두 100ms를 넘는 프레임은 없었다.

프레임별 CSV와 원본 압축본을 요약 파일 옆에 보관했다.

- [높은 품질 결과](Performance/DoorPrint20260916-AreaHigh/summary.json)
- [성능 설정 결과](Performance/DoorPrint20260916-AreaPerformance/summary.json)
