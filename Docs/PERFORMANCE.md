# Windows 출시 성능·지원 계약

문서 버전: `Windows-v1`

기준일: `2026-08-06`

지원 대상: `IndieGame Win64 Shipping`

이 문서는 Windows 첫 출시에서 바꾸지 않을 지원 범위와 성능 합격선을
정한다. 아래 수치는 **출시 목표**이며 측정 결과가 아니다. 현재 SHA의
Shipping 후보를 지정된 장비에서 측정하고 원본 증거를 남기기 전까지
성능 게이트는 `BLOCKED`다. 목표표를 작성했다는 사실을 PASS로 해석하지
않는다.

지원 범위나 합격선을 바꾸려면 문서 버전을 올리고 G5 화면 검증과 G6
성능·패키지 검증을 새 후보로 다시 실행한다. 결과가 기준에 미달하면
사양이나 수치를 조용히 낮추지 않고 최적화, 지원 범위 변경, 출시 차단 중
하나를 이슈로 결정한다.

## 출시 사양

| 구분 | 최소 사양 | 권장 사양 |
|---|---|---|
| 운영체제 | Windows 10 22H2 또는 Windows 11 Home/Pro 25H2, 64비트 | Windows 11 Home/Pro 25H2, 64비트 |
| CPU | Intel Core i5-8400 또는 AMD Ryzen 5 2600 | Intel Core i5-12400 또는 AMD Ryzen 5 5600 |
| GPU | NVIDIA GeForce GTX 1060 6GB 또는 AMD Radeon RX 580 8GB, DirectX 12 | NVIDIA GeForce RTX 2060 6GB 또는 AMD Radeon RX 6600 8GB, DirectX 12 |
| 시스템 메모리 | 16GB | 16GB 이상 |
| 저장 장치 | SSD 필수, 설치 전 여유 공간 12GB 이상 | SSD 필수, 설치 전 여유 공간 12GB 이상 |
| 기본 성능 목표 | 1920×1080, Low, 30fps | 1920×1080, High, 60fps |

동급 장비는 CPU 코어 구성, GPU 전용 메모리와 DirectX 12 지원이 표의
기준보다 낮지 않을 때만 같은 등급으로 취급한다. 내장 GPU, HDD, ARM
Windows, Steam Deck/Linux 호환 계층은 Windows-v1 지원 범위가 아니다.

### 필수 장비 축

표에서 `또는`으로 지원한다고 쓴 OS·CPU·GPU 계열을 한 조합의 결과로
대신하지 않는다. Windows-v1 PASS에는 최소 다음 여섯 장비가 필요하다.
CPU와 GPU는 표의 정확한 모델 또는 사전 등록한 동급 모델을 쓰며, 동급
판정 근거를 결과 매니페스트에 남긴다.

| 장비 ID | OS | CPU 등급 | GPU 계열 | 필수 성능 경로 |
|---|---|---|---|---|
| `MIN-W10-NV` | Windows 10 22H2, 최신 누적 업데이트 | 최소 Intel | NVIDIA GTX 1060 6GB급 | 720p Low 30, 1080p Low 30 |
| `MIN-W10-AMD` | Windows 10 22H2, 최신 누적 업데이트 | 최소 AMD | AMD RX 580 8GB급 | 720p Low 30, 1080p Low 30 |
| `MIN-W11-NV` | Windows 11 Home/Pro 25H2, 최신 누적 업데이트 | 최소 Intel | NVIDIA GTX 1060 6GB급 | 720p Low 30, 1080p Low 30 |
| `MIN-W11-AMD` | Windows 11 Home/Pro 25H2, 최신 누적 업데이트 | 최소 AMD | AMD RX 580 8GB급 | 720p Low 30, 1080p Low 30 |
| `REC-W11-NV` | Windows 11 Home/Pro 25H2, 최신 누적 업데이트 | 권장 Intel | NVIDIA RTX 2060 6GB급 | 1080p High 60, 1440p High 30 |
| `REC-W11-AMD` | Windows 11 Home/Pro 25H2, 최신 누적 업데이트 | 권장 AMD | AMD RX 6600 8GB급 | 1080p High 60, 1440p High 30 |

Windows 10과 11, NVIDIA와 AMD의 기능 경계도 각 장비에서 새 게임,
재실행, 해상도/창 모드 전환, 자막·문서 UI, 저장·불러오기와 양 엔딩까지
확인한다. 한 OS나 한 GPU 공급사 결과가 빠지면 그 대안은 지원 목록에서
제거하거나 게이트를 `BLOCKED`로 유지한다.

Windows-v1의 Windows 11 인증 범위는 Home/Pro 25H2로 한정한다.
26H1을 포함한 다른 기능 업데이트는 실행될 수 있어도 출시 지원으로
광고하지 않는다. 새 Windows 11 기능 업데이트를 지원하려면 해당 버전의
NVIDIA·AMD 기능 검사와 최소/권장 성능 경로를 추가하고 문서 버전을 올린다.

## 해상도·화면 지원 매트릭스

| 출력 해상도 | 지원 상태 | 인증 프리셋과 목표 |
|---|---|---|
| 1280×720 | 지원 | 최소 사양, Low, 30fps. UI·자막·문서의 최소 가독성 기준 |
| 1920×1080 | 주 지원 | 최소 사양 Low 30fps와 권장 사양 High 60fps를 모두 인증 |
| 2560×1440 | 지원 | 권장 사양, High, 30fps. UI·자막·문서 배치도 별도 확인 |
| 3840×2160 | 미인증 | 실행 선택지가 노출되더라도 성능·가독성·메모리를 보증하지 않으며 출시 지원 해상도로 광고하지 않음 |

Windows-v1 인증 화면비는 16:9다. 창 모드, 테두리 없는 창 모드와 전체
화면에서 위 세 지원 해상도를 각각 확인한다. 초광폭, HDR과 다중 모니터는
출시 인증 범위 밖이며 필수 정보가 잘리는 명백한 결함만 P0/P1로 처리한다.
첫 실행 기본값은 1920×1080 High, 60fps, VSync, 동적 해상도 끔과
`r.ScreenPercentage=100`이다. 네이티브 화면 설정은 세 지원 해상도와 세
창 모드, Low/High, VSync, 30/60/무제한 프레임만 노출한다. 변경 적용 뒤
10초 유지 확인이 없으면 이전 해상도·화면 모드·품질·VSync·프레임 제한을
전부 복원해야 하며, 이 자동 복원도 각 GPU 계열 기능 검사에 포함한다.

성능 인증은 동적 해상도를 끄고 `r.ScreenPercentage=100`인 네이티브
렌더링으로 진행한다. TSR을 사용하더라도 내부 해상도를 낮춰 기준을
통과시키지 않는다. 출시 설정에 별도 TSR Quality 프리셋을 제공할 수
있지만 네이티브 인증 결과를 대체하지 않는다. VSync와 프레임 제한은
측정 중 끄고, 화면 찢김 확인은 VSync를 켠 별도 기능 검사에서 수행한다.

## 성능 합격선

프레임 시간 `p95`는 워밍업을 제외한 프레임의 95백분위 값이다.
`1% low`는 가장 느린 1% 프레임 시간의 산술 평균을 밀리초로 계산한 뒤
`1000 / 평균 프레임 시간`으로 환산한다. 평균 fps만으로 합격시키지 않는다.

| 인증 경로 | 프레임 시간 p95 | 1% low | 워밍업 뒤 단일 hitch | 게임 프로세스 메모리 | 전용 VRAM |
|---|---:|---:|---:|---:|---:|
| 최소 사양, 1280×720 Low 30 | 33.33ms 이하 | 25fps 이상 | 100ms 초과 0건 | peak committed 12.0GB 이하 | peak 5.5GB 이하 |
| 최소 사양, 1920×1080 Low 30 | 33.33ms 이하 | 25fps 이상 | 100ms 초과 0건 | peak committed 12.0GB 이하 | peak 5.5GB 이하 |
| 권장 사양, 1920×1080 High 60 | 16.67ms 이하 | 50fps 이상 | 50ms 초과 0건 | peak committed 12.0GB 이하 | peak 5.5GB 이하 |
| 권장 사양, 2560×1440 High 30 | 33.33ms 이하 | 25fps 이상 | 100ms 초과 0건 | peak committed 12.0GB 이하 | peak 5.5GB 이하 |

로딩 화면과 의도적으로 멈춘 엔딩 정지 컷은 프레임 통계에서 분리하되
멈춘 이유와 구간을 원본 trace에 주석으로 남긴다. 플레이 가능한 상태의
스트리밍, 체크포인트, 저장 완료, 문 열림과 음악 전환은 제외하지 않는다.
셰이더 컴파일이나 에셋 누락 때문에 생긴 hitch는 워밍업 사유로 숨길 수
없다.

설치된 Shipping 배포물의 총 파일 크기는 8.0GB 이하여야 한다. 설치 전
12GB 여유 공간 요구에는 압축 해제·prerequisite·업데이트 임시 공간을
포함한다. 사용자 세이브, 크래시 덤프와 로그는 설치 크기에 포함하지
않지만 정상 1회 완주 뒤 `Saved` 폴더가 1.0GB를 넘으면 실패다.

## 측정 절차

1. 검증할 40자리 Git SHA와 작업 트리 clean 상태를 고정하고 G3·G5를
   통과한 같은 SHA에서 G6 하네스가 만든 Win64 Shipping 패키지를 사용한다.
   Development/Editor 수치는 진단용이며 출시 승인을 대신하지 않는다.
2. Windows 업데이트, GPU 드라이버 버전, CPU/GPU/RAM, 저장 장치, 출력
   모드와 게임 설정을 결과 매니페스트에 기록한다. 전원 모드는 AC 전원과
   Windows `최고 성능`으로 고정하고 불필요한 오버레이·녹화 프로그램은
   끈다.
3. `필수 장비 축`의 여섯 장비에서 각자 지정된 프리셋을 실행한다.
   게임을 실행한 뒤 2분간 워밍업하고, 다음 장면을 포함한 동일 입력
   경로를 각 프리셋마다 세 번 실행한다. 재부팅 뒤 첫 실행도 별도로
   보존한다.
   - CH01 집→엘리베이터→현관→골목→편의점 왕복
   - CH02 403호 복도, 문서·폰 UI, 셔터와 추적 압박
   - CH03 사고 현장→계단→옥상 탱크→P3→P5→엔딩 선택
4. 세 반복 중 어느 하나라도 표의 합격선을 넘으면 실패다. 가장 좋은
   반복만 골라 제출하지 않는다.
5. `stat unit`, `stat gpu`, Unreal Insights의 CPU/GPU/frame/loadtime/
   memory trace, `MemReport`, 실행 로그와 화면 설정 캡처를 원본 그대로
   보존한다. GPU 캡처가 지원되지 않는 장비는 도구명과 누락 사유를
   기록하되 프레임·메모리 측정은 생략하지 않는다.
6. 결과 폴더의 모든 파일에 SHA-256을 계산하고 커밋 SHA, 패키지
   매니페스트, 장비 정보, 각 반복의 p95·1% low·최대 hitch·RAM·VRAM·
   설치 크기를 `performance-result.json`에 기록한다.

첫 실행 셰이더 준비와 재실행의 차이는 별도 행으로 남긴다. 반복 측정
사이에 설정, 드라이버, 패키지 또는 맵이 바뀌면 같은 표본으로 합치지
않는다.

## 2026-08-06 적용 구조

아래 표는 목표가 아니라 현재 소스와 자동 검증이 강제하는 구현 계약이다.
성능 수치 PASS와는 구분한다.

| 영역 | 현재 구현 | 회귀 합격 조건 |
|---|---|---|
| 정적 메시 LOD | `generate_meshes.py`가 근접 조사용 hero를 제외한 제작 메시를 `SmallProp`/`LargeProp` LOD 그룹으로 빌드한다. LOD 전환은 고정 거리 대신 화면 점유율을 따른다. | `validate_baked_art_assets.py`에서 모든 non-hero 메시가 LOD 2개 이상이어야 한다. |
| 사진측량 프롭 LOD | 수입된 50개 Static Mesh를 크기 용도에 따라 `SmallProp`/`LargeProp`으로 분류하고, 기존 LOD가 없을 때만 축소 LOD를 빌드·저장한다. 2026-08-06 적용 결과 50개를 갱신했다. | UAsset 감사에서 50개 모두 LOD 2개 이상이어야 한다. 5만 vertex 이상은 Nanite 검토 대상으로만 기록하며 자동 전환하지 않는다. |
| 편의점 반복 재고 | 담배·과자·컵라면·냉장식품·음료 1,122개를 메시/머티리얼별 `UInstancedStaticMeshComponent` 최대 24개로 묶는다. | 런타임에서 `instances=1122`, `batches<=24`, 무충돌, Static mobility와 16~22m start/end 컬링 값을 직접 검사한다. |
| 작은 프롭의 Lumen 비용 | 위 재고는 직접광·머티리얼·화면 추적은 유지하고 distance-field 장면 기여, 데칼 수신, 내비게이션과 그림자 중복을 줄인다. 몸체처럼 실루엣에 필요한 배치만 그림자를 남긴다. | `REBIRTH_RELEASE PASS store_instancing` 영수증이 없으면 A/B 런타임 검증이 실패한다. |
| 유휴 CPU | 손전등은 켜진 동안만, 스트레스 컴포넌트는 공포 값·심박 억제 상태가 실제로 진행되는 동안만 Tick한다. 상호작용 탐색은 기존 10~15Hz 타이머를 유지한다. | Component Tick은 기본 활성 상태로 시작할 수 없으며 정적 검증이 `bStartWithTickEnabled=true`를 차단한다. |
| 설정 HUD | 화면·접근성 설정은 공용 `IGSettingsMenuLayout`에서 해상도별 좌표와 포인터 판정을 계산한다. 색·표면 토큰과 하나의 임시 라운드 마스크를 재사용하며, 메뉴가 닫힌 프레임에는 설정 셋·글리프를 그리지 않는다. | 프런트엔드 계약이 렌더·히트 테스트의 공용 좌표 사용을 검사하고, Shipping 프로브가 720p~1440p 설정 PNG 8장과 키보드·게임패드 경로를 통과해야 한다. 프레임 비용 PASS는 별도 Shipping trace로 판정한다. |
| 하단 대화·자막 HUD | 별도 Widget 트리와 raw binding을 늘리지 않고 기존 네이티브 HUD 한 경로에서 이벤트로 큐만 갱신한다. 대화 6개·환경음 4개로 메모리를 제한하고, 한글 줄바꿈 결과는 페이지·크기·폭이 달라질 때만 다시 계산한다. 숨김 상태에서는 패널·글리프를 그리지 않는다. | 정적 대화 계약, 720p~1440p 경계 오라클과 실제 D3D12 프런트엔드 프로브가 안전 영역·최대 3줄·두 레인·200% 배율을 통과해야 한다. 성능 PASS는 별도 Shipping trace로 판정한다. |
| 텍스처·PSO | 텍스처 스트리밍과 component/global-shader PSO precache를 Shipping 설정에 명시한다. VRAM 풀 크기는 장비 실측 전에는 고정하지 않는다. | `DefaultEngine.ini`의 네 설정과 `stat Streaming`, `stat PSOPrecache`, Insights 증거를 함께 확인한다. |
| 품질 확장성 | Volumetric Fog의 프로젝트 우선순위 고정을 제거해 Low에서는 엔진 scalability가 비활성화하고 High에서는 낮은 해상도 볼륨으로 유지한다. | `DefaultEngine.ini`에 루트 `r.VolumetricFog` 값이 다시 들어오면 정적 검증이 실패한다. |

편의점의 16~22m 값은 **LOD 전환값이 아니라 개별 소형 재고의
per-instance start/end cull 범위**다. 재질이 `PerInstanceFadeAmount`를
소비할 때만 이 구간이 시각적 fade가 되며, 그렇지 않은 불투명 재질은
22m에서 GPU cull된다. 매장 외벽, 조명, 상호작용 물병과 모든 서사
증거물은 이 배치에 넣지 않는다. 따라서 멀리서 매장 실루엣과 빛은 남고,
읽을 수 없는 포장지만 사라진다. fade 재질을 도입하면 PSO·overdraw와
팝 감소를 같은 Shipping 장면에서 A/B 측정한 뒤 채택한다.

현재 월드는 `BeginPlay`에서 절차적으로 조립되므로 에디터가 미리 굽는
HLOD 클러스터를 적용하지 않는다. 향후 환경을 저장된 Level/World
Partition 셀로 이전하면 그때 HLOD를 빌드하고 셀 경계 이동 hitch와
occlusion 결과를 다시 측정한다. 지금 HLOD를 켜는 것은 실제 생성 프롭을
포함하지 못해 복잡도만 늘린다.

Nanite도 기능 이름만 보고 일괄 적용하지 않는다. 현재 생성 프롭은 저폴리
메시이며 화면 크기 기반 LOD가 더 예측 가능하다. 사진측량 메시처럼 실제
삼각형·머티리얼 비용이 큰 에셋은 `Nanite`와 전통 LOD 두 후보를 동일
Shipping 경로에서 비교하고 GPU 시간, fallback mesh, 메모리와 그림자
결과가 나아질 때만 전환한다.

정식 프로파일은 추측 대신 Unreal Insights와 `stat unit`, `stat gpu`,
`stat RHI`, `stat InitViews`, `stat Streaming`, `stat PSOPrecache`를 함께
쓴다. 특히 첫 편의점 진입 전후의 draw call/primitive 수, CH03 탱크 공개
직전의 PSO `Too Late`/miss, 세 챕터의 texture-pool over budget 여부를
별도 북마크로 남긴다.

관련 UE 5.8 기준은 [성능 프로파일링](https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-performance-profiling-and-configuration-in-unreal-engine),
[Static Mesh LOD](https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-and-using-lods-in-unreal-engine),
[Instanced Static Mesh](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine),
[가시성·오클루전 컬링](https://dev.epicgames.com/documentation/en-us/unreal-engine/visibility-and-occlusion-culling-in-unreal-engine),
[텍스처 스트리밍](https://dev.epicgames.com/documentation/en-us/unreal-engine/texture-streaming-configuration-in-unreal-engine),
[PSO precaching](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine),
[Actor Tick](https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-ticking-in-unreal-engine),
[Scalability](https://dev.epicgames.com/documentation/unreal-engine/scalability-in-unreal-engine?lang=en-US),
[Memory Insights](https://dev.epicgames.com/documentation/en-us/unreal-engine/memory-insights-in-unreal-engine),
[DPI Scaling](https://dev.epicgames.com/documentation/en-us/unreal-engine/dpi-scaling-in-unreal-engine),
[Safe Zones](https://dev.epicgames.com/documentation/unreal-engine/umg-safe-zones-in-unreal-engine?lang=en-US),
[UI 최적화](https://dev.epicgames.com/documentation/unreal-engine/optimization-guidelines-for-umg-in-unreal-engine)을 따른다.

## 2026-08-06 로컬 D3D12 스모크

이 결과는 현재 개발 PC의 `UnrealEditor-Cmd -game`, 1920×1080 High,
D3D12, offscreen 실행이다. Shipping 패키지나 최소·권장 장비 결과가 아니며
G6 판정을 바꾸지 않는다. CSV profiler가 수집한 2,514프레임 중 맵 로드와
초기 준비 700프레임을 제외한 1,814프레임을 같은 산식으로 계산했다.

| 항목 | 측정값 |
|---|---:|
| 프레임 시간 p95 / p99 / 최대 | 24.43ms / 29.23ms / 34.88ms |
| 1% low | 32.54fps |
| 50ms 초과 / 100ms 초과 hitch | 0 / 0 |
| Game Thread p95 | 4.98ms |
| Render Thread critical path p95 | 23.75ms |
| GPU p95 / 최대 | 13.86ms / 14.98ms |
| RHI draw calls 평균 / p95 | 145.08 / 155 |
| primitives 평균 | 13,953.31 |
| 프로세스 working set 최대 | 3,687MB |
| 전용 GPU 메모리 최대 | 2,735.80MB |
| texture wanted mip 충족 평균 | 99.98% |
| Graphics PSO hitch | 0건 |

GPU에는 60fps 예산 이내의 여유가 있었지만 전체 프레임 p95는 16.67ms를
넘었다. Editor·CSV 계측 오버헤드와 Render Thread 비용이 섞였어도 합격선은
완화하지 않는다. 이 결과는 1080p High 30fps 스모크만 통과한 것으로
기록하고, 60fps는 같은 SHA의 Shipping 패키지를 권장 장비에서 세 번
측정할 때까지 미승인으로 둔다.

원본은 `Saved/Validation/PerformanceOptimization/runtime_d3d12_smoke/`의
CSV, `csv-stats.json`, `csv-critical-stats.json`, `render-stats.json`,
`RebirthRelease_EndingA_D3D12.utrace`와 오류 0건의
`RebirthRelease_EndingA_D3D12_trace.log`다. trace SHA-256은
`E0177A74C3281C1AA52602F1F12AFC16E616637373306EB7207AC47B250BEA21`이다.

## 런타임 예산 규칙

- Actor Tick은 기본 비활성화하고 이벤트 또는 제한 주기 타이머를 사용한다.
- 플레이 중 동기 에셋 로드는 금지하며 챕터 데이터의 soft reference를
  미리 비동기 로드한다.
- 상호작용 trace는 플레이어당 하나, 10~15Hz를 기본값으로 한다.
- 공간 오디오는 virtualization과 streaming을 사용하고 동시 활성 음성을
  관리한다.
- 일반 소품 텍스처는 1K, hero 소품은 2K를 출발점으로 하며 4K는 화면
  점유와 메모리 근거가 있을 때만 사용한다.
- 반복되는 비상호작용 정적 메시를 개별 컴포넌트로 생성하지 않는다.
  메시·머티리얼·그림자 정책이 같은 항목은 ISM으로 묶고, 작은 프롭에는
  거리 fade/cull을 지정한다.
- 일반 Static Mesh LOD는 월드 거리 숫자가 아니라 화면 점유율로 고른다.
  서사 증거물은 근접 판독 LOD를 보존하고 자동 축소 결과를 눈으로 확인한다.
- Lumen을 사용할 때 shadow-casting movable light의 수와 영향 반경을
  장면별로 제한한다.
- 정적 고밀도 환경 메시에는 Nanite를 비교 검토하되 저폴리 프롭에
  일괄 적용하지 않고, 플레이 캡슐용 단순 collision을 별도로 제공한다.

## 판정

여섯 필수 장비의 지정 인증 경로, OS·GPU 공급사별 기능 검사, 설치
크기와 `Saved` 증가량이 모두 같은 SHA에서 PASS여야 Windows-v1 성능
G6 성능 항목을 통과한다. 한 장비·OS·GPU 공급사·해상도가 누락되면
`PARTIAL`이 아니라 `BLOCKED`, 수치가 초과되면 `FAIL`이다.

현재 dirty 회귀 스냅샷의 검증용 Shipping 패키지는
`Saved/StagedBuilds/RebirthShipping/20260806T072107035Z_51232/`에 있으며
88파일·919,396,939바이트로 자동 A/B·저장·HUD 검증을 통과했다. 그러나 이
계약이 요구하는 clean SHA 후보와 최소/권장 여섯 장비 원본 trace,
`performance-result.json`이 없으므로 판정은 **BLOCKED**다.
