# 실체감 수정·검증 기록

2026-09-14 작업. 자료는 요청한 9월 11일 이전에 공개된 사진과 개발 기법을
기준으로 골랐고, 엔진 API는 설치된 Unreal Engine 5.8에서 확인했다.

## 사진에서 게임까지

위층 사람은 기존 생성물을 다시 깎는 것으로 끝내지 않았다. 실제 낮은 포복
사진에서 몸통과 팔의 지지 관계를 보고, 살아 있는 조각상 사진에서 얼굴에
얇게 덮인 분장의 표면을 참고했다. 두 사진으로 gpt-image 기준 이미지를
만든 뒤 같은 인물의 앞·옆·뒤·위와 얼굴·주먹 확대 시트를 생성했다.

| 자료 | 채택한 부분 | 출처 |
|---|---|---|
| Marines low crawl to objective, 2010-09-24 | 낮은 골반, 비대칭 팔꿈치, 바닥을 짚는 자세 | [Wikimedia Commons / 미 해병대 사진](https://commons.wikimedia.org/wiki/File:Marines_low_crawl_to_objective_(5101777843).jpg), Lance Cpl. Thomas W. Provost, 미국 연방정부 저작물 |
| Statue Vivante 19, 2004-11-13 | 얼굴을 덮는 얇은 흰 분장과 주름 | [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Statue_Vivante_19.JPG), SophieMalraye / OlivierDaraam, 파일 페이지의 PD-self 공개 선언 |

생성 원본은 `Content/SourceArt/AI/ListenerPhotoAnchor_20260914.png`와
`ListenerTurnaround_20260914.png`다. 기존 사진의 인물이나 군복을 게임에
복제하지 않고, 낡은 회색 면옷과 눈을 가린 석고 피막을 가진 가상 인물을
만들었다. 다각도 시트는 형상 검수용이며 실제 다중 시점 스캔 데이터는 아니다.
형상 생성에는 기준 이미지 한 장을 사용했다.

Blender에서는 생성된 원본의 방향을 맞추고 4mm 복셀로 표면을 정리했다.
몸에서 떨어진 발 모양 조각을 제거한 뒤 12,000삼각형으로 줄였다. 관절을
정점 통계로만 추정하던 방식에서, 옆·위 렌더를 확인해 지정한 좌표로 바꿨다.
손목과 발목의 목표 위치를 두 관절 역운동학으로 계산해 키프레임에 구웠다.
런타임에는 구워 둔 21개 뼈와 네 동작만 재생한다.

목한수는 기존 석고보드 자세를 유지하면서 3mm 복셀, 12,000삼각형으로 다시
다듬었다. 피부와 면 작업복의 노멀을 부드럽게 만들고 금속성은 없앴다.
대치·검수·퇴장 코드가 서로 다른 정면 회전을 쓰던 오류도 하나로 맞췄다.
목한수는 현재 정적 메시이며 보행용 리그를 새로 만든 것은 아니다.

## 렌더링과 비용

| 적용 | 게임에서 달라지는 점 | 근거 |
|---|---|---|
| 괴물 스켈레탈 LOD 4단계 | 화면 점유율 0.30 / 0.12 / 0.045에서 단순화. 실제 반입 정점 수 14,400 → 9,377 → 5,242 → 2,930 | [Epic 스켈레탈 LOD](https://dev.epicgames.com/documentation/en-us/unreal-engine/skeletal-mesh-lods-in-unreal-engine) |
| 기존 정적 LOD·상품 인스턴싱·밉 스트리밍 유지 | 반복 상품마다 별도 고밀도 메시와 고해상도 텍스처를 상주시킬 필요가 없다 | [Epic 인스턴싱](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine), [텍스처 스트리밍](https://dev.epicgames.com/documentation/en-us/unreal-engine/texture-streaming-overview-for-unreal-engine) |
| 표면별 노멀·거칠기 재조정 | 벽지와 장판의 과장된 요철, 금속의 거친 얼룩 반사, 편의점 바닥의 번들거림 완화 | [Epic 물리 기반 재질](https://dev.epicgames.com/documentation/unreal-engine/physically-based-materials-in-unreal-engine) |
| 영화 그레인 텍스처 경로 수정 | 설치된 엔진에서 로드되지 않던 v1을 실제 로드되는 v3 리소스로 교체 | UE 5.8 에셋 로드와 D3D12 실행 로그 |

임포스터도 [Epic 공식 자료](https://dev.epicgames.com/documentation/en-us/unreal-engine/impostor-baker-plugin-in-unreal-engine)에서
비교했다. 복도에서 가까이 마주치고 돌아보는 괴물은 사진판으로 바꾸면
옆면·접지·손전등 그림자가 깨진다. 이번에는 인물의 스켈레탈 LOD를 만들고
기존 배경·소품 최적화를 유지했다. 임포스터 베이크나 새 가상 지오메트리
전환을 적용했다고 주장하지 않는다.

## 조작과 공포 연출

- 컨트롤러가 메뉴 처리 후 자신의 Tick을 꺼서 UE의 `PlayerTick`과 입력
  처리가 함께 멈췄다. 실제 W·Shift 키 검사에서 수정 전 이동 속도가 0이었다.
  플레이 중 Tick을 유지하도록 고쳐 이동과 카메라 입력이 계속 처리된다.
- 달리기 제동을 900에서 1800cm/s²로 올렸다. 속도는 460cm/s로 유지했다.
  급히 멈춰 문을 조작할 때 문 너비만큼 지나치는 일을 줄인다.
- 작은 물체를 잡아 주는 구체 스캔에 눈에서 물체까지의 가림 검사를 추가했다.
  문틀 모서리나 벽 너머 물건이 선택되지 않는다.
- 위층 사람의 덮침·포획에도 가까운 거리에서 가림을 검사한다. 닫힌 문
  반대쪽에 있다는 이유만으로 잡히는 일을 막는다. 소리를 듣는 규칙은 유지한다.
- 조용히 문을 닫아도 마지막 쿵 소리가 일반 닫기와 같던 문제를 고쳤다.
  저작 문짝에 임시 단색 재질이 남는 문제도 함께 수정했다.
- 준비만 되어 있던 녹음 베드를 방·골목·편의점·밤 복도·계단·위층에 연결했다.
  같은 복도 녹음을 층마다 다른 지점에서 시작해 소리가 겹쳐 붙지 않게 했다.
  기존 노크·추격·갑작스러운 소리와 소음 판정은 각 상태에 연결된 채 유지한다.
- 파일이 없던 엔딩 배경 세 장을 실제 UI 텍스처로 반입했다. 원본 그림만
  있고 게임에서 로드하지 못하던 상태를 해소했다.

공포 설계 참고는 Frictional Games의
[9 Years, 9 Lessons on Horror](https://frictionalgames.com/2019-10-9-years-9-lessons-on-horror/)
(2019)다. 오래된 자료로 구분해 읽었다. 소리의 의미와 플레이어 행동의 결과가
일치해야 긴장이 쌓인다는 관점에서, 닫힌 문을 무시하는 포획과 조용한 닫기의
큰 효과음을 우선 고쳤다. 기존 밤 1~5의 기록·퍼즐·엔딩을 전면 집필한 작업은
아니다. 무서운 정도와 서사 이해도는 초견 플레이에서 별도로 평가해야 한다.

## 재현

```powershell
pwsh -NoProfile -File Scripts/Build-ArtAssets.ps1 -CodeOnly
pwsh -NoProfile -File Scripts/Run-GameplayRealismProbe.ps1 -FrameRate 30
pwsh -NoProfile -File Scripts/Run-GameplayRealismProbe.ps1 -FrameRate 60
pwsh -NoProfile -File Scripts/Run-GameplayRealismProbe.ps1 -FrameRate 120
pwsh -NoProfile -File Scripts/Run-Prologue-Capture.ps1
pwsh -NoProfile -File Scripts/Validate-Project.ps1
```

모델 재생성 명령은 [Blender 파이프라인](BLENDER_PIPELINE.md)에 있다.
프로브는 별도 `-IGGameplayRealismProbe` 실행에서만 작은 검사 공간을 만들고
종료한다. 실제 게임의 캐릭터·입력·상호작용·괴물 클래스를 그대로 사용한다.
정상 플레이에 검사 액터를 배치하지 않는다.

## 검증 결과

| 검사 | 결과 |
|---|---|
| UE 5.8 Development Editor C++ 빌드 | 통과 |
| `Validate-Project.ps1` 전체 | 통과. 소품 크기·재질·조명 위치·동일 평면 겹침 검사 포함 |
| 실제 키·상호작용·포획·스켈레탈 LOD 프로브 | 30 / 60 / 120fps 모두 8개 검사 통과 |
| 없는 층 통합 런타임 프로브 | 통과. 음향 버스, 벽 울림, 손전등 분진, 포획 복귀, 녹음의 침묵, 밤 2·3 귀환, 응답 노크와 CCTV 해제 포함 |
| 밤 5 런타임 프로브 | 통과. 메뉴 네 배치, 30초 음향 두 번, 종료 후 재선택, 저장 파일 미생성 |
| D3D12 1920×1080 | 프롤로그 12장, 밤 스틸 17장, 소화기·추격 연속 프레임을 실제 게임에서 캡처 |

| 고정 시뮬레이션 프레임률 | 달리기 제동 거리 | 정지 시간 |
|---|---:|---:|
| 30fps | 66.67cm | 0.300초 |
| 60fps | 62.50cm | 0.267초 |
| 120fps | 60.62cm | 0.258초 |

에셋이 실제 로드되는지와 배치는 D3D12 캡처·실행 로그로 확인했다. 로그에
프로젝트 에셋 로드 실패나 재질 컴파일 오류는 없다. 녹음 베드 세 구역도
`recorded=1`로 로드됐다. `aqProf`, `Vtune`, `WinPixGpuCapturer`는 설치되지
않은 선택적 엔진 프로파일러 라이브러리이며 게임 에셋 누락과 구분했다.

통합 검사에서는 손전등이 이미 켜진 채 새 분진 검사를 시작하는 문제를
수정했다. 0.35초마다 갱신하는 기존 앵커를 0.25초 뒤 읽던 검사를, 빔을 새로
켜서 시작하도록 맞췄다. 이제 220개 분진과 2배 밀도를 실제로 확인한다.
타격 후 벽 울림도 PCM을 생성해 검사했고 빈 벽 1.51초, 찬 벽 0.37초로
분리됐다. 주관적인 청취·공포 평가는 이 자동 검사로 대신하지 않는다.

계산대 검수 동선은 진열대 모서리를 더 돌아가도록 조정했다. 연속 프레임은
엔진 공용 Saved가 아닌 프로젝트 `Saved/NightCapture`에 저장한다. 새 추격
GIF는 이번 몸과 동작을 사용하며 README 표시본도 다시 만들었다.

- 실행 로그: `Saved/Logs/GameplayRealism-{30,60,120}.log`,
  `MissingFloorRealism-20260914.log`, `NightFiveRealism-20260914.log`
- 빌드·정적 검사: `Build-Realism-Final-20260914.log`,
  `Validate-Realism-Final-20260914.log`
- 화면: [복도](Media/prologue-corridor.png), [계산대](Media/prologue-store-counter.png),
  [위층 사람](Media/night1-listener-corridor.png),
  [목한수](Media/night4-mok-confrontation.png), [추격](Media/readme/night-listener-chase.gif)

성능 측정은 이미지 저장을 끈 `-IGCaptureMetricsOnly` 동선으로 따로 수행한다.
현재 장비의 Development 결과이며 [Shipping 출시 성능 계약](PERFORMANCE.md)의
여섯 장비 인증을 대체하지 않는다.

## 현재 장비의 성능 측정

Ryzen 9 7900X, RTX 3060 8GB, 드라이버 595.79에서 UE 5.8 Development Editor를
1080p D3D12로 실행했다. 기본 High 설정에서 VSync와 프레임 제한을 해제했다.
빌드·이미지 생성·프로젝트 검사·GIF 변환은 모두 끝낸 뒤 측정했다.

2,400프레임을 수집하고 초기 500프레임을 제외한 1,900프레임을 집계했다.
복도·추격·계단·밤 3 통로를 지나 밤 4 옥상 조작부 부근까지의 표본이다.
스크립트 동선은 목한수 대치까지 완주했지만 공동과 목한수 구간은 이 CSV의
수집 종료 뒤이므로 아래 성능 수치에 포함되지 않는다.

| 항목 | 중앙값 | 95백분위 | 최댓값 |
|---|---:|---:|---:|
| 전체 프레임 | 16.50ms | 18.74ms | 29.08ms |
| GPU | 15.81ms | 18.21ms | 21.58ms |
| 게임 스레드 | 2.15ms | 2.61ms | 5.30ms |
| 드로 콜 | 595 | 1,097 | 1,793 |
| GPU 로컬 메모리 | 3,166MiB | 3,213MiB | 3,294MiB |

이 표본에서 33.33ms를 넘은 프레임은 없었다. 모든 프레임이 16.67ms 이내인
60fps 고정 상태는 아니다. 캡처를 켠 별도 실행에서는 PNG 읽기·압축·저장 때문에
긴 멈춤이 생겼으므로 그 실행을 게임 성능으로 집계하지 않았다.

원본 [CSV 압축본](Performance/Realism-20260914/full-profile.csv.zip),
[분석 대상 프레임](Performance/Realism-20260914/frames.csv),
[수치·원본 SHA-256](Performance/Realism-20260914/summary.json)을 보존했다.
`Scripts/summarize_runtime_profile.py`로 같은 집계를 다시 만들 수 있다.

```powershell
$editor = & .\Scripts\Resolve-UnrealEditor.ps1 -Commandlet
& $editor (Join-Path (Get-Location).Path 'IndieGame.uproject') `
    -game -unattended -nosplash -NoLoadingScreen -RenderOffscreen -d3d12 -nosound `
    -Windowed -ResX=1920 -ResY=1080 -ForceRes -IGListenerGreybox -IGNightCapture `
    -IGCaptureMetricsOnly -IGSkipFrontend -csvCaptureFrames=2400 -csvGpuStats `
    '-ExecCmds=t.MaxFPS 0,r.VSync 0'
```

CSV 저장 위치는 종료 로그의 `Writing CSV to file`에서 확인한다.
