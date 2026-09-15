# 9월 14~15일 장면·조작·퍼즐 수정

전자레인지의 상판 돌출과 편의점 직원 얼굴의 파손을 재현했다. 작은 얼굴에
정면 사진을 덧씌우던 방식은 옆모습을 고치지 못했고, 원본의 뒤집힌 표면을
그대로 줄이면서 볼과 눈 주위에 삼각형 틈이 생겼다.

## 실물과 모델

전자레인지는 삼성 MS20A3010AL의 [제품 사진과 치수](https://www.samsung.com/de/microwave-ovens/solo/mw3000am-solo-full-glass-door-metallic-edge-dual-dial-led-lighting-ms20a3010al-eg/)를
기준으로 다시 만들었다. 폭 440mm, 높이 259mm, 깊이 337.5mm다. 검은 유리문,
오른쪽 다이얼 두 개, 열림 버튼, 옆면 통풍구와 바닥 발을 구분했다.
[설치 안내](https://org-sec-b2c.samsung.com/sec/home-appliances/faq-micro-wave-ovens/)의
방열 공간도 배치에 반영했다. 원본 사진은 형태를 확인하는 참고 자료로만 썼다.

나린은 기존 캐릭터 원화를 gpt-image로 얼굴·목 확대 이미지로 파생한 뒤
TRELLIS로 입체화했다. 실제 사람을 스캔한 자료는 아니다. 전신 원본과 확대
원본을 목에서 연결하고, 얼굴 정면 사진을 별도로 투영하던 처리를 없앴다.
처음에는 복셀 변환과 단순 축소 모두 표면을 손상시켰다. 피부 앞면의 법선이
안팎으로 섞인 것을 확인하고 방향을 맞춘 뒤 Screened Poisson으로 복원했다.
[Blender의 얇은 표면 처리 설명](https://docs.blender.org/UATEST/manual/en/dev/modeling/modifiers/generate/remesh.html)과
[PyMeshLab 필터 문서](https://pymeshlab.readthedocs.io/en/latest/filter_list.html#generate_surface_reconstruction_screened_poisson)를
참고했다. 이 보정은 이번 인물에 맞춘 처리이며, 모든 형태에 자동 적용하지 않는다.

몸체에 약 6천5백, 머리에 약 9천 삼각형을 배분했다. 최종 메시에는 15,486개가
남았다. 팔과 다리의 법선을 각 부위 중심에 맞추고, 옷의 작은 구멍을 닫은 뒤
줄여 소매의 삼각형 틈을 없앴다. 머리는 복셀 변환을 거치지 않는다.
4K 텍스처 UV 면적의 2/3는
머리에 사용하고, 게임에서는 화면 점유율에 따라 기존 네 단계 LOD를 유지한다.
원화는 `Content/SourceArt/AI/NarinHead_20260914.png`, 생성 영수증과 GLB는
`Content/SourceArt/Generated/NarinHead/face-closeup-20260914/`에 있다.

재생성은 `Scripts/blender/rebuild_clerk_head.py`, 리깅은 `rig_store_clerk.py`,
게임 반입은 `Import-BlenderAssets.ps1 -Only SK_NarinClerk,SM_Microwave` 순서다.
표면 복원에는 시스템 Python의 `pymeshlab==2025.7.post1`과 NumPy가 필요하다.
몸과 머리는 다른 광선 거리로 따로 굽고 합친다. 천에는 원본의 깨진 면 노멀을
옮기지 않는다. 결과를 전신과 여섯 각도의 얼굴 확대 렌더, 실제 계산대 화면에서 확인했다.

![계산대의 직원](Media/retail-clerk.png)
![상판 안에 배치한 전자레인지](Media/kitchen-microwave.png)

## 조사와 공포

[Frictional Games의 개발자 설명](https://blog.playstation.com/2023/06/01/amnesia-the-bunker-launches-june-6/)(2023)에서
도구의 물리적 성질, 소음의 대가, 여러 접근 순서를 참고했다.

첫 퍼즐은 공용 조명을 끈 채 무명 회로의 전원을 바꿔 계량기 반응을 비교한다.
스위치만 눌러서는 끝나지 않는다. 켜짐과 꺼짐을 모두 관찰하고 검침표를 대조해야 한다.
두 번째 퍼즐은 먹지 복원과 퇴실 문자 날짜의 모순을 함께 확인해야 진행된다.
문자 사본이 특정 밤에 갑자기 생기던 처리는 없앴다. 날짜 대조를 일찍 마치면
다음 낮에 황순금의 일지도 받을 수 있어, 단서 순서를 고정하지 않는다.

대사와 기록은 직접 본 사실까지만 말한다. 전원이 들어왔다고 거주자의 신원을
확정하거나, 차단기가 내려갔다고 범인을 단정하지 않는다. 벽의 위치, 남겨진
물건, 집주인의 방해를 차례로 확인하면서 조사 범위를 좁힌다.

일반 포획에서는 추격하던 3D 몸을 화면에 남긴다. 상체가 가슴 높이까지 덮치고
시선과 몸높이가 따라간다. 0.65초 뒤부터 암전해 2.15초에 침대로 돌아간다. 가까운 마찰과 끊긴
숨을 추가했다. 모션 감소에서는 강제 시선 이동과 카메라 하강을 생략한다.
회복 뒤 입력 복귀와 포획 누적은 기존 저장 규칙을 따른다.

근접 화면에서 괴물의 얼굴이 삼각형 조각처럼 보이는 원인도 따로 확인했다.
2K 텍스처의 밉 12단계 중 7단계만 올라와, 64px짜리 이미지를 얼굴 전체에
늘이고 있었다. [UE 텍스처 스트리밍 설명](https://dev.epicgames.com/documentation/en-us/unreal-engine/texture-streaming-overview-for-unreal-engine)을
대조한 뒤, 몸을 생성하거나 밤에 깨울 때 10초 동안 미리 읽고 5m 안에서는
5초 기한으로 갱신하도록 바꿨다. 포획 중에는 세 텍스처 모두 12단계가
올라왔는지 실제 렌더링 검사에서 확인한다. 캐릭터 텍스처 그룹도 별도로 지정했다.
전체 텍스처 스트리밍은 계속 켜 두며, 예약 기한이 지나면 엔진이 메모리를 회수할 수 있다.

## 렌더링과 통과 여유

[UE 5.8 Lumen 성능 가이드](https://dev.epicgames.com/documentation/unreal-engine/lumen-performance-guide-for-unreal-engine)와
[Virtual Shadow Maps 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)를
확인했다. 인스턴싱과 LOD 외에도 광원의 적용 거리, 그림자 필터 비용, 거친 표면의
반사 추적 범위, 간접광 계산 방식을 비교했다.

일반 화질은 로컬 그림자 샘플을 4에서 2로 줄이고, 반사 추적 거칠기 상한을
0.25로 조정했다. 로컬 광원에는 영향 반경을 고려한 표시 거리와 페이드를 둔다.
성능 모드는 UE 5.8의 중간 품질을 사용한다. 이 단계의 Lumen Lite와 화면 공간
반사는 일반 화질과 결과가 같지 않으므로 별도로 비교한다.

측정 스크립트는 저장된 사용자 설정에 기대지 않고 품질과 내부 해상도를 매번
지정한다. 끝나면 기존 설정 파일을 복원한다. 일반 화질은 1920×1080에서
100%, 성능 모드는 71% 해상도를 사용한다. 두 모드의 차이에는 업스케일링
비용과 화질 차이도 포함된다.

RTX 3060, DX12, 1080p 출력, 프레임 제한·수직 동기화 해제 상태에서 같은
매장 동선을 실행했다. 화면 저장은 끄고 첫 120프레임을 제외했다. 대조군도
이번 코드와 에셋을 쓰며, 그림자 샘플만 4로 되돌리고 반사 거칠기 강제값을
해제했다. 아래 차이는 이 두 렌더링 설정의 비교이며 에셋 교체 전후 비교가 아니다.

| 설정 | 프레임 중앙값 | 95백분위 | GPU 메모리 중앙값 | 유효 프레임 |
| --- | ---: | ---: | ---: | ---: |
| 일반 화질 대조군, 내부 100% | 21.349ms | 23.458ms | 3,023MiB | 1,363 |
| 일반 화질 수정본, 내부 100% | 19.381ms | 21.733ms | 3,021MiB | 1,442 |
| 성능 모드, 내부 71% | 8.909ms | 10.171ms | 2,454MiB | 3,197 |

일반 화질의 프레임 중앙값은 9.2% 줄었다. 각 설정을 한 차례씩 측정한 결과로,
다른 PC나 게임 전체 구간의 프레임을 보장하지 않는다. CSV 원본·추출 수치·해시는
[대조군](Performance/Revision-20260915-Baseline/summary.json),
[일반 화질](Performance/Revision-20260915-High/summary.json),
[성능 모드](Performance/Revision-20260915-Performance/summary.json)에 묶었다.
각 폴더의 `full-profile.csv.zip`에는 메타데이터까지 포함한 원본이 있다.

재현 명령은 다음과 같다. `-Measure`를 빼면 실제 화면 7장을 저장한다.

```powershell
Scripts/Run-RetailReview.ps1 -Measure -Quality High -Profile baseline -RenderCommands @('r.Shadow.Virtual.SMRT.SamplesPerRayLocal 4', 'r.Lumen.Reflections.MaxRoughnessToTrace -1')
Scripts/Run-RetailReview.ps1 -Measure -Quality High -Profile high
Scripts/Run-RetailReview.ps1 -Measure -Quality Performance -Profile performance
```

캐릭터 충돌 반경을 34cm에서 30cm로 줄여 86cm 문에서 좌우 여유를 각각
13cm 확보했다. 눈높이와 문 크기는 유지했다. 실제 이동 입력으로 문 중심에서
10cm 비껴 진입하는 검사, 달리기 제동, 문 뒤 물체 조작 차단, 닫힌 문 너머
포획 차단을 확인했다. 포획 때 머리 뼈가 눈 위치에서 35~110cm 안에 남고
높이 차가 45cm를 넘지 않는지도 실제 애니메이션을 진행시켜 검사했다.

진행 검사는 공용 조명과 무명 회로를 직접 조작해 전원 전후 관찰·독립 기록
대조를 거친다. 손잡이의 실제 위치 변화도 확인한다. 잘못 고정돼 움직이지 않던
두 손잡이와 계량기 원판을 이동 가능한 컴포넌트로 바꿨다.

## 실행 확인

DX12로 매장 화면 7장을 두 품질에서 각각 저장했다. 상품 1,301개는 23개
인스턴스 묶음으로 그려졌고, 전자레인지 바운드는 상판 안에 들어왔다.
성능 모드에서도 인물 얼굴과 안내문을 확인했으며 작은 글씨와 반사 선명도는 줄어든다.

야간 장면을 처음부터 촬영하고, 포획 장면만 바로 시작하는 경로도 확인했다.
두 경로 모두 색·노멀·ORM 텍스처가 12단계까지 올라왔고 실제 3D 몸과
카메라가 접촉 상태를 유지했다. 전체 진행 검사와 30·60·120fps 조작 검사를
통과했으며 최종 반입 후 60fps 검사를 다시 실행했다.
`Scripts/Validate-Project.ps1` 전체 검사와 Unreal Editor 개발 빌드도 통과했다.

![실제 포획 화면](Media/m1-capture-embrace.gif)

GIF는 PNG를 순서대로 저장한 검수 영상이라 게임의 실시간 프레임 측정에는
쓰지 않는다. 촬영 간격을 유지해 묶었으며 빠르게 재생시키지 않았다.
공포 강도와 퍼즐 체감 난이도, 헤드폰 공간감·실제 패드 진동은 별도의 플레이
평가가 필요하다. 여기서 확인한 항목은 배치, 진행, 입력 복귀, 접촉 연출과 렌더링이다.
