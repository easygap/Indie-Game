# 계량기·관리실·석재 마감 점검

2026년 9월 15일. 같은 날 진행한 [출입 동선과 바닥 수정](SPATIAL_REALISM_20260915.md)에 이어, 관리실과 설비를 가까이서 살펴보고 수정했다. 이번 성능 비교의 기준 커밋은 `9551c93`이다.

## 발견한 문제와 수정

| 대상 | 확인한 문제 | 게임에 반영한 내용 |
| --- | --- | --- |
| 계량기 | 실제 전력량계와 다른 둥근 문자판, 세워진 원판, 함체에 묻힌 부품 | 열린 검침창, 검은 절연 뒷판, 코일, 숫자창, 수평 회전판을 따로 모델링 |
| 검침값 | 같은 계기 형태만 반복 | 다섯 누적 검침값과 호실 명판을 공용 인쇄 아틀라스로 표시 |
| 계량기 관찰 | 전원 상태와 주변 계기의 움직임이 맞지 않음 | 공용 회로와 이름 없는 회로를 각각 반영하고, 원판 관찰에 1.2초 유지 입력 적용 |
| 관리실 책상 | 속이 꽉 찬 금속 상자와 검은 모니터 덩어리 | 다리 아래가 빈 책상, 받침이 있는 보안 모니터, 분리된 녹화기로 교체 |
| 모니터 | 네 색상 사각형이 CCTV 화면 역할을 함 | 같은 맵의 현관·골목·계단·복도 캡처를 네 분할 화면으로 사용 |
| CCTV 조작 | 화면 아래의 별도 상자를 누름 | 모니터 전면의 INPUT 버튼으로 외부 입력 전환 |
| 화면 방향 | 화면 면이 뒤를 향해 단면 재질이 보이지 않음 | 일반 화면과 외부 입력 화면을 함께 돌려 정상 방향으로 표시 |
| 녹화기 | 장치 형태와 상태를 화면에서 알 수 없음 | USB·BNC 단자, 고무발, 전원 표시등을 모델링. HDD 없음과 꺼진 녹화등을 조사 문장에 맞춤 |
| 종이와 달력 | 무늬 없는 판과 읽기 화면의 내용이 따로 존재 | 문자 사본·영수증·지난해 7월 달력을 실제 표면에 인쇄. 달력의 26일 표시와 문장 일치 |
| 열쇠 | 검은 상자, 습득 뒤에도 남는 물건 | 고리·열쇠 두 개·이름표를 만들고 습득 상태에 따라 표시와 상호작용 종료 |
| 관리실 밸브 | 배관에서 떨어진 조작물 | 토출관에 몸체와 축을 연결하고, 열고 닫을 때 손잡이가 회전하도록 수정 |
| 옥상 저수조 | 금속이 자갈처럼 보이고 기초 옆면에 바닥 무늬가 늘어남 | 얇은 스테인리스 외장에 맞춘 색상·금속성·거칠기, 기초의 면 방향 투영 |
| 주방 상판 | 큰 입자와 강한 명암이 반복됨 | 옅고 작은 칩이 있는 인조대리석 색상 원본으로 교체 |
| 기둥·계단·외벽 | 한쪽 면에 맞춘 재질을 옆면까지 늘여 사용 | 화강석, 계단 챌판, 어두운 콘크리트 테두리, 적벽돌에 면 방향 투영 적용 |

새 계량기는 단상 유도형 계기의 구조를 참고한 게임 소품이다. 특정 제품의 인증 치수나 내부 결선을 재현한 모델은 아니다. 기기의 구조와 관찰 행동이 맞도록 만들었다.

## 실물 자료와 생성 원본

계량기는 [실제 검침함 사진](https://smartcontentcenter.tistory.com/1297)과 [유도형 계기 근접 사진](https://thomson.tistory.com/1233)을 살펴봤다. 녹화기는 [한화비전 SRD-440](https://www.hanwhavision.com/ko/products/video-recorder/dvr/dvr-ch4/srd-440/), 모니터는 [SMT-1931 제품 사진](https://www.hanwhavision.com/mea/products/product-details/smt-1931)을 참고했다.

저수조는 [실제 원통형 물탱크 사진](https://blog.daara.co.kr/sell_view.html?bc=21&cid=samjung&key=&no=53377&page=1&search=&url=%2Findex.html)의 얇고 매끈한 외장을 대조했다. 상판은 [LX HIMACS 2025 자료](https://img.lxhausys.com/2024/content/files/202508211101/HIMACS%202025%20Reference%20Book.pdf)의 G004 White Quartz 견본과 [공식 연마 안내](https://himacs-fabrication.lxhausys.com/10-sanding-finishing)를 참고했다. 색상 칩을 깊게 팬 요철로 만들지 않았다.

실물 사진을 입력해 `imagegen`으로 계량기와 녹화기의 여러 각도 참조 시트, 스테인리스와 상판의 색상 원본을 만들었다. [생성 원본과 실제 프롬프트](../Content/SourceArt/AI/UtilityFixtures_20260915.json)를 보존했다. 녹화기 시트에 포함된 마우스는 이번에 모델링하지 않았다. 열쇠는 기존 [자물쇠·열쇠 참조 시트](../Content/SourceArt/AI/SheetRooftopUnlockedPadlockKeysReference.png)를 다시 대조했다. 외부 제품 사진은 게임 텍스처로 배포하지 않는다.

Blender에서 만든 새 메시 여섯 종은 검침함·계기·회전판·모니터·녹화기·열쇠다. [모델링 스크립트](../Scripts/blender/build_utility_fixtures.py)와 각 모델의 `.blend`, FBX, PBR 원본을 함께 보존했다. 모델을 만드는 것에서 끝내지 않고 UE 반입 후 앞·옆면에서 글자 방향, 상판 접촉, 부품 관통을 확인했다.

## 자료를 적용한 기준

- [Epic의 UE 5.8 Virtual Shadow Maps 안내](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)는 VSM 사용 시 별도의 화면 공간 접촉 그림자가 대체로 불필요하다고 설명한다. 장면 조명과 달빛의 Contact Shadow를 끄고 VSM 그림자와 광원 크기는 유지했다.
- [AMD의 Unreal Engine 성능 가이드](https://gpuopen.com/learn/unreal-engine-performance-guide/)에 따라 같은 동선·해상도·설정으로 비교하고, PNG 저장 비용을 제외한 CSV를 남겼다. 준비 구간 120프레임을 제외했다.
- [Epic의 MegaLights 안내](https://dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine)도 검토했다. 현재 프로젝트는 하드웨어 레이 트레이싱을 사용하지 않으므로 이번 변경에 MegaLights를 넣지는 않았다. UE 5.8 소스에서 기존 Clustered Deferred 경로가 폐기 대상으로 남아 있는 것도 확인했으며 해당 옵션을 켜지 않았다.
- [2026년 7월 29일 Silent Hill: Townfall 개발진 설명](https://blog.playstation.com/2026/07/29/silent-hill-townfall-hands-on-report/)에서 장소와 인물에 연결된 퍼즐, 소리를 이용한 행동 선택, 직접 촬영한 장소 자료를 참고했다. 이번에는 계량기·밸브·녹화기를 실제로 관찰하고 조작할 수 있도록 맞췄다. 이 한 사례를 전체 업계의 유행으로 일반화하지 않는다.

새 메시에는 기존 4단계 LOD 규칙을 적용했다. 작은 원판은 그림자를 만들지 않으며 6.5m 밖에서는 회전 갱신을 보내지 않는다. 표면 원본은 최대 1K와 밉 체인·텍스처 스트리밍을 사용한다. 색상 지도에서 금속의 높이를 추측해 노멀을 만들지 않는다.

일반 CCTV는 실제 게임 캡처 한 장을 사용하며 화면에 `일시 정지`를 표시한다. 상시 SceneCapture는 0개다. 외부 입력 5번의 기존 연출만 352×288, 초당 12회로 잠깐 렌더한 뒤 자원을 해제한다. 기존 네 색상 판도 실시간 카메라를 쓰지 않았으므로, 이를 카메라 네 대의 렌더 비용을 절감한 변경으로 계산하지 않았다.

## 실제 화면과 검사

[계량기](Media/utility-meter-wide.png) · [근접 관찰](Media/utility-meter-close.png) · [관리실 정면](Media/utility-booth-front.png) · [측면과 종이 배치](Media/utility-booth-side.png) · [배관 밸브](Media/utility-booth-valve.png) · [저수조](Media/utility-tank-steel.png) · [주방 상판](Media/utility-countertop.png)

UE 5.8 에디터 개발 빌드를 완료했다. `Run-FixtureReview.ps1 -BakeCctv`로 설비 7개 시점과 CCTV 원본 4장을, `Run-RetailReview.ps1 -Spatial`로 매장·골목·실내·옥상 21개 시점을 캡처했다. D3D12, 1920×1080, High 설정이다. 마지막 설비 검수에서는 갱신된 CCTV 아틀라스가 실제 모니터에 보이는 것도 확인했다.

- 출하 시작 경로와 캐릭터 출입 검사 15개 통과. 캡슐 지름 60cm·높이 192cm, 문 중앙과 좌우 편차를 포함한다.
- 30·60·120fps의 이동·계단·문·붙잡힘 검사 통과.
- 전체 그레이박스 진행 검사 통과. 계량기의 전원 전후 비교, 수평 회전과 거리 제한, 관리실 밸브와 기록 대조를 검사했다.
- 새 열쇠에는 단순 충돌을 추가했다. 실제 조준에 쓰는 가시성 트레이스로 선택한 뒤 습득하고, 물건·충돌·상호작용이 사라지며 두 문이 열리는 검사까지 통과했다.
- CCTV 외부 입력의 실제 렌더 검사 통과. 352×288 렌더타깃을 입력 때 할당하고 66회 캡처한 뒤 해제했으며, 두 번째 재생은 거부했다.
- `Validate-Project.ps1` 통과. 디렉터 소품의 회전을 검사 상자에 반영하고 동적 재질의 부모도 읽도록 감사 도구를 수정했다. 기존에 기록된 창문 장식과 창틀의 겹침 1건은 미결 목록에 남아 있다.

[검사 기록](Performance/Utility20260915-checks.txt)에 실행 결과와 새 모델의 LOD 반입 기록을 남겼다. 원본 이미지를 README에 그대로 넣지 않고 `optimize_readme_media.py`로 표시용 사진을 갱신했다.

## 성능 측정

RTX 3060, 에디터 게임 실행, D3D12, 1920×1080 출력. 모든 측정은 같은 매장·골목 동선을 사용하며 첫 120프레임을 제외했다. 성능 모드는 기존 Medium 설정과 내부 해상도 71%를 사용한다.

| 측정 | 프레임 중앙값 | 프레임 p95 | GPU 중앙값 | GPU 메모리 중앙값 |
| --- | ---: | ---: | ---: | ---: |
| 수정 전 High | 19.349ms | 21.812ms | 18.779ms | 3,024.5MiB |
| 수정 후 High 첫 실행 | 19.231ms | 46.674ms | 18.815ms | 3,022.4MiB |
| 수정 후 High 재측정 | 18.923ms | 21.337ms | 18.305ms | 3,024.3MiB |
| 수정 후 성능 모드 | 9.187ms | 10.974ms | 8.380ms | 2,462.2MiB |

High 재측정의 중앙값은 약 0.43ms 짧아졌다. 차이가 작아 큰 성능 개선이라고 단정하지 않는다. 이번 에셋 교체 후에도 이 동선의 렌더 비용과 메모리는 대체로 같은 수준이며, High는 60fps 목표 시간인 16.67ms를 넘는다. 성능 모드는 측정한 모든 프레임이 13.082ms 이하였다.

첫 High 실행에서는 124프레임이 33.33ms를 넘었다. 조명뿐 아니라 기본 패스·그림자·반사·TSR 등 여러 GPU 항목이 함께 느려졌고, 같은 설정의 재측정에서는 재현되지 않았다. 원인은 확정하지 못했다. 첫 결과를 버리지 않고 [수정 전](Performance/Utility20260915-Before/summary.json), [첫 실행](Performance/Utility20260915-After/summary.json), [High 재측정](Performance/Utility20260915-HighRepeat/summary.json), [성능 모드](Performance/Utility20260915-Performance/summary.json)에 프레임별 수치와 전체 CSV 압축본을 함께 남겼다. 이 결과는 해당 동선의 측정이며 전체 게임이나 배포용 실행 파일의 성능을 보장하지 않는다.
