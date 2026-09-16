# 옥상 계단과 저수조 점검

2026년 9월 16일. 비교 기준은 `6e98464`다. 방·복도·관리실·골목·편의점·옥상 화면을 다시 살피면서, 계단 철판과 저수조에서 확인한 형태·배치·조작 문제를 수정했다. 아래 화면은 실제 게임 캡처다.

## 계단

기존 계단은 같은 밝은 무늬가 반복됐고, 세로 면에서는 무늬가 늘어났다. 체크철판 시공 사진을 참고해 imagegen으로 도장 원본을 만들었다. Blender에서 3.2mm 철판을 접고, 34mm 간격의 돌기를 만든 뒤 노멀맵으로 구웠다. 단의 앞면은 같은 도장으로 마감했다. 이제 밝은 점을 찍어 놓은 무늬 대신 조명 방향에 따라 요철이 드러난다.

| 이전 | 수정 후 |
| --- | --- |
| ![기존 철판 무늬](Media/readme/roof-before-stair.webp) | ![새 계단 마감](Media/readme/roof-stair-down.webp) |

벽에는 지름 38mm 손잡이와 고정 브래킷을 달았다. 실제 화면에서 각져 보이던 곡면도 다시 다듬었다. 보행 충돌은 기존 계단이 맡고, 얇은 마감판과 손잡이에는 플레이어를 붙잡는 충돌을 추가하지 않았다.

![계단과 벽 손잡이](Media/readme/roof-stair-handrail.webp)

## 저수조와 배관

표찰에는 2,000L라고 나오는데 몸체는 지름 306cm, 높이 260cm였다. 원통으로만 계산해도 약 19,000L다. 실물 2톤 탱크의 130×160cm 규격을 참고해 몸체를 다시 만들고, 1m 높이 기단도 20cm로 낮췄다. 새 모델은 정상 수위 150cm에서 약 1,991L다. 뚜껑·점검구·넘침관·수위계를 붙였다.

원통 비례는 국내 제작사의 FRP 탱크 사진에서, 금속 마감과 접속부는 STS 탱크 제작 사진에서 참고했다. 두 자료를 바탕으로 imagegen에서 정면·사선·뒷면 참조를 만들고 Blender로 모델링했다. 특정 판매 제품을 그대로 복제한 모델은 아니다.

| 이전 | 수정 후 |
| --- | --- |
| ![큰 저수조와 막힌 원통 배관](Media/readme/roof-before-tank.webp) | ![새 저수조와 연결 배관](Media/readme/roof-tank-wide.webp) |

표찰 없이 빈 공간을 겨눠 읽던 문제도 고쳤다. 용량 표찰과 읽기 판정을 같은 자리에 놓고, ‘저수조 살펴보기’로 문구를 줄였다. 기록에는 실제 수위계에서 본 상태를 남긴다. 한글과 수치는 폰트로 조판하고 게임 화면에서 방향과 가독성을 확인했다.

![저수조 표찰과 수위계](Media/readme/roof-tank-plate.webp)

배수관은 탱크 하단에서 왼쪽 배수관으로 이어지고, 우회관은 오른쪽 입수관에서 탱크 옆면으로 이어진다. 관이 허공에서 끊기던 부분을 없애고 굽은 연결부·유니언·플랜지·지지대를 만들었다. 두 밸브에는 배관 이름과 열림 방향을 붙였다.

밸브를 열면 손잡이가 0.75초 동안 225도 돌아가고 축이 6.5mm 나온다. 진행 기록의 열림·닫힘 상태를 따라 움직이고, 목표 각도에 닿으면 회전 갱신을 멈춘다. 잘못된 순서의 경보와 10초 인터록, 복구 후 재조작, 마지막 펌프 음향의 재생 상태까지 같은 실행에서 확인했다.

| 조작 전 | 배수를 연 뒤 |
| --- | --- |
| ![닫힌 배수 밸브](Media/readme/roof-valve-closed.webp) | ![열린 배수 밸브](Media/readme/roof-valve-open.webp) |

## 렌더링과 검증

계단 마감 20개는 ISM 한 개로 묶었다. 마감판 하나는 216삼각형이며, 돌기 150개의 세부 형상은 노멀맵에만 남는다. 기존 원통 배관 10개는 재질 하나를 쓰는 메시 한 개로 교체했다. 새 배관은 3,828삼각형, 저수조는 6,376삼각형, 손잡이는 1,636삼각형이다. 표찰은 각각 12삼각형이다.

계단 마감에 중복 그림자와 거리장을 만들지 않고, 작은 표찰은 10~12m 밖에서 숨긴다. 움직이지 않는 설비는 Static으로 두고, 밸브 두 개와 펌프 선택 손잡이만 움직일 수 있게 했다.

실행 검사는 다음 범위를 확인했다.

- Blender 원본·FBX·텍스처와 UE 에셋 7종을 만들고 Development Editor 빌드를 통과했다.
- 방·복도·관리실·골목·편의점·옥상 순회 화면 21장과 새 설비 화면 10장을 촬영해 살폈다. 편의점에서는 얼굴과 목의 경계, 전자레인지, 상품 뒷면, 바닥 줄눈도 확인했다.
- 새 계단 마감 20개의 배치와 충돌·그림자 설정, 저수조 표찰과 밸브를 겨냥하는 시선 판정이 통과했다.
- 밸브의 225도 회전, 잘못된 순서의 경보, 10초 뒤 재조작, 펌프 표시등과 음향 재생 상태를 확인했다.
- 입주 진행과 자유 탐색, 통로 24곳의 캡슐 이동, 30·60·120fps 조작 검사, 전체 밤 진행 검사가 통과했다.
- `Scripts/Validate-Project.ps1` 전체 검사와 `git diff --check`가 통과했다.

이번 퍼즐 수정은 저수조를 읽고 밸브를 조작하는 과정에 집중했다. 화면 검수 범위는 위의 31개 시점이며, 자유 탐색의 모든 시야와 조작 조합까지 포함하지는 않는다.

[검사 결과 발췌](Performance/RoofUtility20260916-checks.txt)

## 성능 측정

RTX 3060 8GB·Ryzen 9 7900X·Windows 11에서 UE 5.8 Development 게임을 D3D12·1920×1080으로 실행했다. 높은 품질은 Scalability 2·렌더 해상도 100%, 성능 설정은 Scalability 1·71%와 TSR을 쓴다. 스크린샷 촬영 없이 구간별로 약 66초를 측정하고 첫 120프레임을 제외했다. 측정 중 빌드·Blender·이미지 변환·다른 게임 검사는 실행하지 않았다.

| 구간·설정 | 분석 프레임 | 중앙값 | p95 | 최댓값 | 33.33ms 초과 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 옥상·높은 품질 | 4,180 | 15.28ms | 16.95ms | 29.75ms | 0 |
| 옥상·성능 | 8,540 | 7.55ms | 8.81ms | 92.39ms | 1 |
| 승강장·복도·높은 품질, 첫 측정 | 3,702 | 16.26ms | 32.24ms | 44.46ms | 149 |
| 승강장·복도·높은 품질, 재측정 | 4,071 | 15.76ms | 18.10ms | 24.60ms | 0 |

옥상 GPU 시간 중앙값은 높은 품질 14.85ms·성능 설정 7.00ms, GPU 메모리는 각각 2,896MiB·2,326MiB였다. 높은 품질은 p95가 16.67ms를 넘어 60fps를 항상 유지하지 못했다. 성능 설정에서도 92.39ms 지연이 한 번 있었다.

복도 첫 측정에서는 약 48~59초에 GPU 시간이 길어졌다. 코드를 바꾸지 않고 같은 경로로 다시 실행한 결과, 기존 `6e98464`의 중앙값 15.76ms·p95 18.25ms와 비슷한 15.76ms·18.10ms가 나왔다. 일회성 지연의 원인은 미확인이라 첫 결과도 함께 보관했다. 재측정의 드로 콜 중앙값은 396으로 기존보다 3개 늘었고, GPU 메모리 중앙값은 약 1MiB 늘었다.

각 폴더에 프레임별 CSV와 전체 원본 압축본을 보관했다.

- [옥상·높은 품질](Performance/RoofUtility20260916-RoofHigh/summary.json)
- [옥상·성능](Performance/RoofUtility20260916-RoofPerformance/summary.json)
- [복도·첫 측정](Performance/RoofUtility20260916-AreaHigh/summary.json)
- [복도·재측정](Performance/RoofUtility20260916-AreaHighRepeat/summary.json)

## 참고 자료

9월 16일에 확인한 자료다. 신작 하나의 방식을 공포게임 전체의 유행으로 일반화하지 않고, 이 게임의 공간과 조작에 맞는 부분을 골랐다.

- [체크철판 계단 실물](https://www.materiel-elevage-online.fr/28208-tole-larmee-epaisseur-5-7mm-acier-lamine-a-chaud.html): 철판을 접은 모서리와 표면 요철.
- [으뜸프라스틱 2톤 원통 탱크](https://imbp.co.kr/bbs/board.php?bo_table=notice&wr_id=383): 마지막 사진의 1,300×1,600mm 탱크 비례.
- [으뜸프라스틱 STS 탱크 제작](https://imbp.co.kr/bbs/board.php?bo_table=notice&wr_id=293): 금속 표면과 접속 플랜지.
- [SOTECH 벽 손잡이 브래킷](https://so-handel.de/a124-2024.html): 벽면 고정 구조.
- [Unreal Engine 5.8 ISM 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine): 같은 메시를 묶고 인스턴스별 LOD를 사용하는 방식. 계단 마감에 적용했다.
- [Unreal Engine 5.8 Virtual Shadow Maps](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine): 움직이는 물체와 렌더 상태 변경이 그림자 캐시에 미치는 영향. 설비의 이동 여부와 갱신 범위를 나눴다.
- [Silent Hill: Townfall 개발자 소개, 2026-02-12](https://blog.playstation.com/2026/02/12/silent-hill-townfall-reveals-first-person-gameplay-in-a-new-trailer/): 1인칭 시점에서 직접 다루는 도구와 주변 상황을 읽는 퍼즐을 강조한다. 이번 수정에서는 표찰·수위·배관·손잡이 움직임이 퍼즐의 상태를 설명하도록 했다.

생성 프롬프트와 참조 범위는 [원본 기록](../Content/SourceArt/AI/RoofUtility_20260916.json)에, 재생성 절차는 `Scripts/blender/build_roof_utility.py`와 `build_roof_stair_tread.py`에 남겼다. 외부 사진은 배포 에셋에 넣지 않았다.
