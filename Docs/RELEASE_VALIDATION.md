# REBIRTH G3~G6 릴리스 검증 절차

이 문서는 `FEASIBILITY.md`의 출시 게이트를 실제로 실행하고 증거를
보관하는 절차다. 정적 검사 성공은 G3~G6의 대체 증거가 아니며, 현재
커밋과 다른 빌드·로그·캡처는 재사용하지 않는다.

현재 판정은 `G3 BLOCKED / G4 BLOCKED / G5 BLOCKED / G6 BLOCKED`다.
UE 5.8과 Win64 C++ 빌드 환경에서 같은 SHA의 런타임·수동·초견·Shipping
증거를 새로 만들기 전에는 이 문서의 절차 작성 자체를 완료 증거로 쓰지
않는다.

## 현재 환경에서 가능한 범위

| 구분 | 지금 실행 가능 | 이 결과가 승인하는 범위 |
|---|---|---|
| 엔진 불필요 | 깨끗한 단일 SHA에서 `Run-Rebirth-ReleaseValidation.ps1 -StaticOnly`, 개별 정적 스크립트, `git diff --check`, 문서·라이선스 목록 검토 | G2 회귀 사전 점검. G3~G6은 승인하지 않음 |
| UE 5.8·C++ 빌드 도구 필요 | 릴리스 하네스의 Development 빌드·Map Check·A/B 자동 런타임, S1~S8, R1~R4 실제 입력, PIE, 실제 `SaveGame`, 충돌·가독성 검증 | G3 |
| UE 빌드와 사람 필요 | 초견 플레이, 퍼즐 시간·이해도·공포 반응, 최종 아트·문서·자막·음향·접근성 확인 | G4·G5 |
| UE 패키징과 별도 실행 환경 필요 | Win64 Shipping 빌드, 여섯 필수 장비 성능 측정, 클린 VM 스냅샷 또는 별도 물리 PC 실행, 세이브 마이그레이션, 크래시·라이선스·배포물 검사. 개발 PC의 새 사용자 계정은 사전 점검만 인정 | G6 |

회사나 CI에서 화면·소리를 내지 않는 반복 사전 점검은
`Run-Rebirth-BackgroundRuntimeValidation.ps1`을 사용한다. 이 스크립트는
`UnrealEditor-Cmd`, `-nullrhi -nosound -RenderOffscreen -unattended`와 숨김
프로세스로 Map Check와 A/B 종단을 실행하고 표시 창 0개를 확인한다. dirty
작업 트리에서도 회귀 탐색용으로 실행할 수 있지만 clean SHA·Shipping 증거를
잠그지 않으므로 정식 G3 또는 G6 증거는 아니다.

Editor DLL 실행만 조직 정책으로 차단되고 Shipping EXE는 허용되는 환경은
`-SkipEditorRuntimeValidation -SkipMapCheck`를 사용할 수 있다. 이 조합은
저장·자유 경로·체크포인트·Editor A/B를 `NOT_RUN`으로 남기지만 Shipping
패키징과 격리된 A/B 실행은 유지한다. `-SkipRuntimeValidation`은 종전대로
Editor와 Shipping 런타임을 모두 끄므로 둘을 혼동하지 않는다. 어느 Skip
조합도 최상위 `PARTIAL`이며 clean 환경·수동 게이트를 승인하지 않는다.

`Resolve-UnrealEditor.ps1`이 프로젝트와 일치하는 UE 5.8 실행 파일을
찾지 못하면 릴리스 하네스는 종료 코드 2와 `BLOCKED` 요약을 남긴다.
Full 실행은 에디터를 시작하기 전에 Windows System32 또는 이 UE 설치의
공식 Engine AppLocal 위치에서 `msvcp140_2.dll`과
`vcruntime140_1.dll`을 확인한다. 두 파일이 모두 `14.50.35719.0` 이상이고,
Engine AppLocal 경로를 쓸 때는 ThirdParty 원본과 해시까지 일치해야 한다.
두 경로 모두 충족하지 못하면 `vc_runtime_prerequisite` 단계를 `BLOCKED`로
기록하고 종료 코드 2로 끝낸다. 하네스는 관리자 권한 설치나 임시 DLL
우회를 시도하지 않는다. 요약에 기록된 UE 5.8 redist 설치 또는 공식 엔진
파일 복구를 화면 사용이 허용된 유지보수 시간에 수행한 뒤 새 clean SHA
검증을 시작한다.
`-StaticOnly`도 실행 전후 같은 깨끗한 SHA를 잠근다. dirty 상태는
`BLOCKED`, 실행 중 변경은 `FAIL`이며, 잠금에 성공한 정적 단계만 PASS,
최상위 결과는 `PARTIAL`이다. 어느 경우에도 G3 이후를 통과 처리하지 않는다.

저장소 절대 경로에 한글 등 비 ASCII 문자가 있으면 Full 하네스는 clean
소스를 `%LOCALAPPDATA%\IndieGame\AsciiBuild\Release\<run-id>`에 미러링해
Development Editor/Game과 Shipping을 빌드한다. `.git`, `Saved`,
`Intermediate`, `Binaries`, `DerivedDataCache`, `.vs`는 미러 입력에서
제외하며, 원본 저장소의 SHA·작업 트리 잠금이 계속 증거 기준이다. 빌드된
Win64 바이너리만 원본 런타임 검증용으로 되돌리고, 요약에는 실제
`buildProjectFile`과 `usingAsciiBuildMirror`를 기록한다.

## 2026-08-12 없는 층 v3.3 자동 사전 검증 현황

이 절은 재관람 스킵과 엔딩 C 「매물」 구현의 dirty 작업 트리 회귀 기록이다.
아래 결과는 G3~G6이나 출시 승인을 대신하지 않는다.

- `Test-MissingFloor-ReleaseEndingContract.ps1`:
  `MISSING_FLOOR_RELEASE_ENDING_CONTRACT PASS replay_skip=1 ending_c=1
  scoped_retry=1 audio=1 runtime_probe=1`.
- `Test-MissingFloor-M5RevealContract.ps1`: 기존 ImageGen 원본 4장·근접 3D/PBR
  레이어·공동 리빌·세 엔딩 정적 계약 통과.
- UE 5.8.1 `IndieGameEditor Win64 Development`: UHT 포함 16액션 재빌드 성공.
  한글 절대 경로의 UBT 인자 분리를 피하려고 같은 프로젝트 상위 경로를 로컬
  `subst`로 짧게 매핑했으며 소스 복사·정책 우회·엔진 변경은 하지 않았다.
- `Run-MissingFloor-Greybox.bat`: 약 51초, C 진입→시간 정지→P5 마스킹 제거→
  밤4 원자적 롤백→P5 재해결→공동 개방→엔딩 A까지 진행하고
  `MISSINGFLOOR_GREYBOX PASS`와 M6 6버스 PASS를 함께 기록했다.
- `Run-MissingFloor-EndingPreview.ps1`: 실제 D3D12 오프스크린 렌더 두 건 통과.
  - `Saved/Validation/MissingFloorEndingPreview/listing-1920x1080-default.png`
  - `Saved/Validation/MissingFloorEndingPreview/comment-1280x720-text-200-reduced.png`
  두 PNG는 요청 해상도와 일치하고, HUD가 보고한 모든 글리프·패널 경계가
  Canvas 안이다.

남은 승인 항목은 실제 입력으로 포획부터 재시도까지 7.2초 호흡 확인,
헤드폰·TV·모노의 벽지 롤러 청감, 초견 플레이어의 매물→후기 전환 이해도,
clean SHA Shipping 캡처와 성능이다.

## 2026-08-06 자동 사전 검증 현황

이 절은 현재 작업의 위치를 기록할 뿐 아래 G3~G6 합격 조건을 완화하지
않는다.

- `Saved/Validation/RebirthRelease/20260806T072107035Z_51232/summary.json`:
  현재 변경 불가 dirty 회귀 스냅샷의 Full 하네스에서 요청한 자동 단계가
  모두 개별 PASS했다. 세 타깃 빌드, Map Check, Editor·Shipping A/B,
  Build/Cook/Stage/Package/Archive, Editor와 패키징 Shipping 저장 46프로세스,
  네 해상도 입력·HUD 및 실행 후 88파일 무변조를 한 실행에서 확인했다.
  아카이브는 919,396,939바이트이고 내부 EXE SHA-256은
  `45261B5215729AEEBDEB5341A4668D5EEE5E83F4C0C53E7576994155FDABCBDA`다.
  첫 통합 실행 `20260806T065855451Z_55376`은 46프로세스 성공 뒤 Editor 모드
  요약의 빈 배열이 `$null`로 접히는 StrictMode 결함을 검출했다. 명시적 배열
  초기화와 정적 회귀 규칙을 추가하고 저장 단독 실행
  `20260806T071113203Z_39204` 및 위 Full 실행을 새로 통과했다. 소스는 실행
  동안 변하지 않았지만 dirty 입력이므로 최상위는 `PARTIAL`이며 clean SHA나
  수동 G3~G6 PASS로 승격하지 않는다.
- `Saved/Validation/RebirthRelease/20260805T235228055Z_29780/summary.json`:
  `-AllowDirtyWorktree`로 고정한 변경 불가 스냅샷에서 요청한 자동 단계 17개가
  모두 개별 PASS했다. 정적 계약, UE 5.8.1 해석, Engine AppLocal CRT,
  Editor/Game Development 빌드, 저장 46프로세스, CH02 자유 경로 5개,
  체크포인트 10프로세스, Map Check 오류·경고 0, Editor A/B 종단,
  Shipping Build/Cook/Stage/Package/Archive와 패키지 A/B 실행을 포함한다.
  dirty 입력 때문에 최상위는 `PARTIAL`이며 clean SHA의 정식 자동 후보나
  수동 게이트 PASS가 아니다.
- Shipping 아카이브는
  `Saved/StagedBuilds/RebirthShipping/20260805T235228055Z_29780/`에 있다.
  내부 Shipping EXE를 서로 다른 `-UserDir`로 A/B 각각 실제 실행해 종료 코드
  0과 `ShippingRuntime_EndingA.txt`, `ShippingRuntime_EndingB.txt`의 정확한
  PASS 영수증을 받았다. 두 실행 전후 파일 수·크기·SHA-256도 모두 같아
  아카이브 변경과 새 파일 생성은 0건이다. 루트 런처 SHA-256은
  `62FEE0CD7F668239E71819B12629EC95933BA0ABDD68EB51C9201767A0182145`이며
  `4:44 AM / 1.0.0 / easygap`, 기준 아이콘과 AppLocal CRT 계약을 통과했다.
- 이 실행 전에 clean SHA 자동 종단이 5층 계단참의 벽과 160cm 연결문이
  겹친 실제 캡슐 충돌을 검출했다. 벽·바닥·천장 범위를 분리하고 문 양쪽의
  54cm 캡슐 여유를 컴파일 타임 계약으로 고정한 뒤, 바닥 41지점과
  34×96cm 캡슐 9구간, Editor A/B와 위 전체 dirty 회귀 실행을 다시
  통과했다.
- 실제 D3D12 오프스크린 `-IGCaptureCH03`은 2026-08-06 종료 코드 0과
  `visual_stills=5`를 기록했다. 새 5층 프레임은 통로를 가리던 비충돌
  비닐을 벽면으로 옮기고 20Hz 저비용 흔들림과 약한 비상등을 적용한 뒤
  `Docs/Media/ch03-fifth-floor-doorway.png`로 보존했다. 이후 재링크된 unsigned
  Editor DLL은 회사 Enterprise Application Control 이벤트 3077/3033으로
  로드가 차단됐다. 보안 정책을 끄거나 신뢰 경로·파일명을 이용해 우회하지
  않으며, 새 clean SHA의 전체 자동·수동 증거는 계속 별도로 요구한다.
- 이후 CH03 신규 기상의 1회성 렌즈 물방울을 절차 BGRA 텍스처와 알파
  블렌딩으로 구현했다. 정적 검사는 단일 타이머 예약, 복원·그레이박스·일반
  캡처 차단, 챕터 카드 아래가 아닌 월드 위/HUD 아래 렌더 순서, 모션 감소 시
  이동 0을 확인한다. 얇은 비대칭 수막으로 보정한 최신 소스는 UE 5.8
  Editor·Shipping UBT와 코드 전용 재스테이지
  `20260806T124336_lens_polish`를 통과했다. 전용 하네스는 아카이브를 두
  격리 복사본으로 나눠 실제 Shipping D3D12 1280×720을 실행했고 기본
  `11.56px`, 모션 감소 `0px`, 최소 HUD 알파 `0.720`, 안전 영역 1과 PNG
  4장을 확인했다. 프레임은 `Docs/Media/ch03-lens-droplet-*.png`, 원본 요약은
  `Saved/Validation/LensDropletCapture/20260806T124336_lens_polish_retry/summary.json`
  에 있다. 같은 아카이브의 최신 A/B 종단과 88파일 무변경도
  `Saved/Validation/RebirthLensRelease/20260806T124336_lens_polish/summary.json`
  에서 통과했다. 이는 고정 GPU 계약의 PASS이며 사람 자유 입력·다중
  디스플레이 감마를 포함한 전체 G5 승인을 대신하지 않는다.
- 이후 M0~M5 생산 오디오 라우터를 구현했다. 정적 계약 79개는 M1b의
  `-18센트/-8%`, M2 `proven_elsewhere`, M3 보행 반응, 분리된 M4 압력층,
  최초 개방과 손전등 복귀 양쪽의 심박 포함 6초 침묵, M5 45초와 단일
  물방울을 고정한다. 최신 UE 5.8 Editor·Shipping 빌드는 성공했다.
  `20260806T025202630Z_44524` dirty Shipping 전용 회귀는 같은 생산 생성기
  8개를 실제 PCM으로 렌더링하는 `audio_synthesis`를 포함해 A/B 종료 코드
  0과 정확한 영수증을 받았다. Cook/Stage/Package, 제품 메타데이터·아이콘,
  AppLocal CRT와 88개 아카이브 파일의 실행 전후 SHA-256 무변경도 통과했다.
  Editor 런타임·Map Check를 의도적으로 건너뛴 `PARTIAL`이므로 clean SHA
  전체 하네스와 실제 청감 증거는 여전히 필요하다.
- CH03 5층과 옥상의 지나친 암부를 보정한 뒤 Editor/Game Development와
  Shipping UBT를 다시 통과했다. 새 Editor DLL의 Cook 실행은 조직
  Application Control 오류 4551로 차단됐으므로 정책을 우회하지 않았다.
  대신 `Content/`, `Config/`, `Build/`, 플러그인과 프로젝트 설명자 변경이
  0건임을 먼저 확인하고, 직전 검증 Cook을 사용한 코드 전용 재스테이지
  `20260806T121300_codeonly`를 만들었다. `.ucas/.utoc` 네 파일은 직전
  아카이브와 SHA-256이 같고, 포장 바이트만 달라진 `.pak`은 양쪽을 풀어
  1,735개 페이로드의 경로·크기·SHA-256이 모두 같음을 확인했다.
  최신 Shipping EXE는 기준 EXE와 다른 해시를 가지며 A/B 격리 실행의 정확한
  영수증, 제품 메타데이터·아이콘·AppLocal CRT와 88파일 실행 전후 무변경을
  통과했다. 증거는
  `Saved/Validation/RebirthReleaseCodeOnly/20260806T121300_codeonly/`에 있다.
  이 경로는 코드 회귀와 GPU 촬영을 허용하는 사전 증거일 뿐 clean SHA의
  완전 Cook/Stage/Package를 대신하지 않는다.
- 같은 최신 Shipping EXE의 D3D12 오프스크린 CH03 촬영은 종료 코드 0과
  1280×720 PNG 5장을 만들었다. 5층 프레임의 5% 미만 암부 비율은
  80.3%에서 67.5%, 옥상 프레임은 75.7%에서 67.2%로 줄었고 98% 초과
  하이라이트는 각각 0%와 0.002%다. 계단·160cm 연결문·탱크·사다리가
  읽히면서도 검은 여백과 방향 압박을 유지하므로 추가 증광은 하지 않는다.
  촬영 원본은
  `Saved/Validation/ShippingVisual/20260806T123100_lighting_audio/`에 있고,
  영향받은 5층·옥상·탱크 리빌 프레임을 `Docs/Media/`에 갱신했다. 렌즈
  기본/모션 감소의 고정 D3D12 비교도 위 별도 하네스에서 승인했으며, 실제
  사람 입력과 디스플레이별 감마 승인은 계속 G5 수동 게이트다.
- `20260806T130501_shipping_persistence`는 기존 46개 저장 스파이크를 실제
  패키징 Shipping 프로세스로 다시 실행했다. 각 쓰기 프로세스가 남긴 실제
  SaveGame을 다음 읽기 프로세스가 복원했고 CH01 암전 4, 고양이 선택 10,
  CH02 시간 4, P5 8, P3 14, 엔딩 A/B 6개 영수증이 정확히 일치했다. 요약은
  `Saved/Validation/RebirthShippingSpikes/20260806T130501_shipping_persistence/summary.json`
  이며 `processCount=46`, `archiveFileCount=88`, `archiveUnchanged=true`다.
  이는 dirty 코드 전용 아카이브의 자동 사전 증거이며 clean SHA G3를
  대신하지 않는다.
- `Run-Rebirth-FrontendShippingProbe.ps1`는 1280×720·1600×900·1920×1080·
  2560×1440 각각에서 비시뮬레이션 `InputKey` 11회와 HUD 배치
  샘플 11개의 실제 글리프 경계를 검사한다. 같은 실행에서 저장 데이터가
  없는 첫 실행 타이틀, 화면·접근성 설정과 기본·최대 자막 프레임을 PNG로 캡처하고 크기·
  SHA-256을 요약에 고정한다. 최대 자막 프레임은 화자명이 있는
  200% 하단 기기 메시지 3줄, 무손실 `이어짐`, 위쪽 환경음 레인과 80%
  안전 영역을 함께 검사하고 해상도별 PNG·SHA-256을 보존한다. 첫 시도는
  눌림·뗌을 같은 틱에 넣은
  하네스 결함을 `keyboard_f10_open` 실패 영수증으로 검출했고, 물리 입력과
  같은 누름 프레임→입력 처리→상태 확인→뗌 순서로 수정했다. 수정된 최종
  소스는 Editor/Game Development/Shipping 빌드와 88파일 재스테이지
  `20260806T133117_frontend_contract`, 제품 메타데이터·아이콘을 통과했다.
  해당 재스테이지의 내부 Shipping EXE는 조직 Application Control 오류
  4551로 프로세스 생성 전에 차단됐고, 이 시점의 네 해상도 영수증은
  `NOT_RUN`이었다. 정책을 우회하지 않고 이후 완전 Cook/Package 전체 실행
  `20260806T072107035Z_51232`에서 같은 하네스를 다시 실행해 네 해상도를
  모두 PASS했다. 현재 계약은 입력 이벤트 44개, HUD 배치 샘플
  44개, 대화 사례 8개, 설정 화면 캡처 8장이며 해상도별 PNG와
  실행 전후 패키지 무변조를 보존한다.
  Full 하네스의 `shipping_persistence_spikes`와
  `shipping_frontend_input_hud`는 패키징 A/B 뒤, 최종 아카이브 무변조 검사
  전에 실행되므로 생략된 clean 실행은 자동 후보 PASS가 될 수 없다.
  대화 소스는 정적 계약 109개, UE 5.8 Editor 빌드와 Development D3D12
  1280×720·1600×900·1920×1080·2560×1440 프로브도 통과했다.
  초기 50ms 전환 프레임의 낮은 대비를
  증거로 잘못 캡처하던 하네스도 240ms 진입이 끝난 뒤 촬영하도록 수정했다.
  네 영수증과 원본 PNG는
  `Saved/Validation/DialogueHudMultiResolution_20260806T064728473Z/`에 있다.
  자동 Shipping 입력·HUD PASS는 실제 게임패드 조작, 사람 청감과 초견,
  디스플레이별 감마를 포함한 G5 수동 승인을 대신하지 않는다.

## 증거 보관 규칙

검증을 시작하기 전에 작업 트리가 깨끗한지 확인하고 현재 커밋의 40자리
SHA를 고정한다. 릴리스 하네스는 실행별 원본을 다음 위치에 만든다.

```text
Saved/Validation/RebirthRelease/<UTC+PID>/
  summary.json
  StaticContracts.log
  EngineResolution.log
  DevelopmentEditorBuild.log
  DevelopmentGameBuild.log
  PersistenceSpikes.log
  PersistenceSpikes/
    summary.json
    BoundaryBeforeWrite.log ... BoundaryAfterRead.log
    CatChoiceWrite_CapLeft.log ... CatChoiceRead_PassedBy.log
    CH02TimeWrite_0.log ... CH02TimeRead_1.log
    P5Write_0.log ... P5Read_3.log
    P3Write_0.log ... P3Read_6.log
    EndingWrite_A.log ... EndingVerify_B.log
    SaveSnapshots/
      BoundaryBeforeWrite.sav ... CatChoiceWrite_PassedBy.sav
      CH02TimeWrite_0.sav ... EndingCommit_B.sav
  CH02FreedomSpikes.log
  CH02FreedomSpikes/
    summary.json
    P1ThenP2.log ... SkipBoth.log
  CheckpointAnchorSpikes.log
  CheckpointAnchorSpikes/
    summary.json
    AnchorWrite_CH02Corridor.log ... AnchorRead_CH03Roof.log
    SaveSnapshots/
      AnchorWrite_CH02Corridor.sav ... AnchorWrite_CH03Roof.sav
  MapCheck_Prologue_Morning.log
  RebirthRelease_EndingA.log
  RebirthRelease_EndingB.log
  ShippingPackage.log
  ShippingExecutableMetadataSync.log
  ShippingExecutableMetadata.log
  ShippingApplicationIcon.log
  ShippingApplicationIcon.png
  ShippingArchiveManifest.json
  ShippingRuntime_EndingA.txt
  ShippingRuntime_EndingB.txt
  ShippingRuntimeUser_A/
  ShippingRuntimeUser_B/
Saved/Validation/RebirthRelease/Latest.json
Saved/StagedBuilds/RebirthShipping/<UTC+PID>/
```

`Latest.json`은 편의용 포인터일 뿐 증거가 아니며 실행별 폴더를 기준으로
삼는다. `StaticOnly` 또는 Skip 옵션이 하나라도 있으면 최상위 결과는
`PARTIAL`이다. Skip 없는 `summary.json` PASS도 **자동 단계만** 성공했다는
뜻이며 S1~S8·R1~R4 수동 항목을 승인하지 않는다. 요약의
`releaseEligible`은 수동 G3~G6이 남아 있으므로 항상 false이고,
`automatedReleaseCandidateEligible`만 자동 범위의 완결성을 나타낸다.
요약 스키마 v3는 실제 빌드 미러 경로, VC++ 런타임 사전 점검 결과,
Shipping 실행 파일 VERSIONINFO 동기화·검증 로그와 아이콘 증거 경로·해시를
기록하고, 패키지 A/B 실제 실행의 영수증·실행 파일 해시·격리 데이터 루트를
`shippingRuntimeResults`로 보존한다. 이전 v2 요약은
현재 하네스의 새 실행 결과로 대체해야 한다.

`Saved/`는 Git에서 제외된다. 릴리스 승인 때는 해당 실행 폴더, Shipping
산출물, 수동 G3 증거와 G4~G6 자료를 SHA별 읽기 전용 외부 보관소나 CI
아티팩트에 복사하고 별도 `manifest.json`에 보관 위치를 남긴다. 영상은
편집본이 아니라 처음부터 끝까지 이어진 원본을 보존한다.
파일명은 `<gate>-<case>-<UTC YYYYMMDDTHHMMSSZ>.<ext>` 형식을 사용하며
`latest.log`처럼 덮어쓰는 이름을 사용하지 않는다.

`manifest.json`에는 최소 다음 값을 기록한다.

```json
{
  "commit_sha": "<40자리 SHA>",
  "worktree_clean": true,
  "engine_association": "5.8",
  "engine_build_version": "<Build.version의 버전과 changelist>",
  "target": "IndieGame",
  "configuration": "Shipping 또는 Development",
  "map": "/Game/Maps/Prologue_Morning",
  "save_schema": 3,
  "command_line": "<실행한 전체 명령>",
  "started_at_utc": "<ISO 8601>",
  "machine": "<OS/CPU/GPU/RAM>",
  "result": "PASS | FAIL | BLOCKED",
  "exit_code": 0,
  "artifacts": [
    { "path": "<상대 경로>", "sha256": "<SHA-256>" }
  ],
  "executed_by": "<실행자 식별자>",
  "approved_by": "<승인자 식별자>",
  "archive_location": "<외부 보관 위치>"
}
```

- 소스·Content·Config 또는 빌드 설정이 바뀌면 새 SHA로 다시 검증한다.
- 실패 로그와 크래시 덤프도 삭제하지 않는다. 수정 전후 증거를 서로 다른
  UTC 파일로 남긴다.
- `PASS`는 해당 게이트의 필수 파일이 모두 있고 SHA-256이 일치할 때만 쓴다.
- 매니페스트·로그·세이브 픽스처·패키지 해시는 해당 릴리스 지원 기간
  동안 보관한다. 초견 원본 영상은 동의한 기간과 출시 후 90일 중 더 짧은
  기간까지만 보관하고, 익명 집계 CSV와 해시는 지원 기간 동안 남긴다.
- 초견 참여자는 `P01` 같은 익명 ID만 사용한다. 이름·연락처·음성 동의서
  같은 개인정보는 저장소와 릴리스 증거 폴더 밖에 보관한다.
- 과거 `Docs/Media` 캡처와 구버전 플레이 영상은 설명 자료이며 릴리스
  증거가 아니다.
- 자동 UE 로그의 Ensure·Fatal·Unhandled Exception·Assertion과
  `Log*: Error:`는 기본적으로 실패다. 현재 자동 G3 허용목록은 비어 있다.
  예외가 필요하면 하네스에 정확한 정규식과 허용 사유를 함께 추가하고,
  같은 내용을 매니페스트 승인 목록에도 기록한 새 SHA에서 다시 실행한다.

## 공통 사전 점검

1. `git status --short`가 비어 있는지 확인하고 `git rev-parse HEAD`를
   `manifest.json`의 SHA로 사용한다.
2. Git LFS 파일을 모두 받은 깨끗한 복제본에서 시작한다.
3. 아래 하네스 사전 점검을 실행한다. `Validate-Project.ps1`과 두 REBIRTH
   정적 오라클을 별도 PowerShell 프로세스로 호출하고 실행별
   `StaticContracts.log`, 로그 SHA-256과 `summary.json`을 만든다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\Run-Rebirth-ReleaseValidation.ps1 -StaticOnly
git diff --check
```

   `-StaticOnly`도 실행 전 `source_state_pre`, 종료 전
   `source_state_post`에서 같은 clean SHA를 확인한다. 개별 스크립트를
   직접 실행할 때는 콘솔만 출력하므로 호출하는 터미널에서
   원문과 종료 코드를 따로 캡처한다. 하네스는 정적 원문을 보존하지만
   `-StaticOnly` 자체는 릴리스 후보가 아니므로 최상위 `PARTIAL`이다.
4. 전체 하네스가 `Resolve-UnrealEditor.ps1`로 UE 5.8을 찾고 실제
   `Build.version`을 요약에 기록한다. 찾지 못하면 이후 단계는 `NOT_RUN`,
   전체 결과는 `BLOCKED`다.
5. Full은 UE 5.8 해석 직후 VC++ 런타임 두 파일이 최소
   `14.50.35719.0`인지 검사한다. 미달이면 에디터·빌드·Cook을 시작하지
   않고 `vc_runtime_prerequisite=BLOCKED`와 실제 버전을 요약에 남긴다.
6. Full은 엔진 해석 뒤 빌드 전에 작업 트리가 깨끗한지, 시작 SHA와
   같은지 확인하고 종료 직전 다시 확인한다. dirty 상태는 기존 HEAD의
   증거가 아니며 빌드 전에 `BLOCKED`, 실행 중 변경은 `FAIL`이다.
7. 같은 SHA와 같은 콘텐츠로 만든 후보만 G3~G6에서 사용한다. PIE에서
   수정한 뒤 이전 패키지나 영상을 승인 자료로 섞지 않는다.

## 검증 경계와 대표 범위

순수 상태 검사는 빠른 회귀 오라클이고 UE 실행 증거가 아니다. 반대로
대표 경로 몇 개가 통과해도 미방문 상태의 논리 조합을 증명하지 않는다.
아래 두 층을 각각 통과해야 한다.

| 층 | 전수 또는 대표 범위 | 통과 증거 | 증명하지 못하는 것 |
|---|---|---|---|
| 순수 상태 전수검사 | 진실 12개, C5 `2^9×2^3=4,096`, 최종 게이트 128, P5 첫 순서쌍 110·자기 재선택 11·신원 6순서·원자료 92·관찰 저장 24,576, P3 원자료 32. A/B/C·결제·고양이·순서·탱크·손전등을 곱한 7,200개 라우터 상태와 양 엔딩 14,400개, JSON 왕복 21,600개 | `Test-Rebirth-NarrativeContract.ps1`과 `Test-Rebirth-RouteMatrix.ps1`의 종료 코드 0, 현재 SHA의 원문 출력과 종료 코드. 후자는 P3 저장 경계 7개, 엔딩 저장 경계 6개, P5 첫 진실 순서 24개와 R1~R4 정의 포함 여부도 검사 | 충돌, 시야, 음향 가독성, 애니메이션 중복, 실제 UE `SaveGame`, 프레임 타이밍 |
| UE 대표 경로 | R1 직행(A·지갑·P1~P4 생략·P5 능동 대조·엔딩 A), R2 탐색(C·후드 카드·고양이 물/대기·P1→P5·엔딩 B), R3 역순(B·P2→P1·탱크 선행·신원 보조식), R4 오답/왕복(P1~P4 오답 3회, P5 무효쌍·자기쌍, 이탈·재진입, 무등화 탱크) | 경로별 새 빈 절대 `-UserDir`, `0 / false / unset` 시작 덤프, 같은 콘텐츠의 무편집 입력 영상, 절대 경로 로그, 시작·종료 세이브 SHA-256, C1~C6·`NarrativeDebt=0` 종료 덤프와 데이터 루트 삭제 기록 | 7,200개 조합 전체와 초견 이해율 |

`Run-Rebirth-Greybox.bat`의 `REBIRTH_GREYBOX PASS`는 P3·P5 라우터의
대표 런타임 증거일 뿐 S1~S8과 R1~R4를 대신하지 않는다. 이 스크립트는
고정 로그 `Saved/Logs/RebirthGreybox.log`를 실행마다 지우고 다시 쓰므로,
실행 직후 현재 SHA의 증거 폴더로 복사하고 종료 코드·명령행·엔진 버전과
파일 SHA-256을 함께 남긴다. 과거 캡처나 구버전 로그는 현재 커밋의 합격
증거로 재사용하지 않는다.

## G3 — 엔진 런타임

G3는 다음 항목이 모두 끝나야 통과한다. 자동 범위는 Shipping을 건너뛴
하네스로 먼저 실행한다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\Run-Rebirth-ReleaseValidation.ps1 -SkipShippingPackage
```

### S1~S8 수동 합격 사례

아래 절차는 `FEASIBILITY.md`의 S1~S8 합격 기준을 실제 입력과 최종
콘텐츠로 판정하는 단일 기준이다. 모든 사례는 최종 충돌 크기와 출시 후보
입력 설정으로 실행하고, 로그 첫 줄에 커밋 SHA·맵·원문 실행 인자·세이브
스키마를 기록한다.

| ID | 절차 | 통과 기준 |
|---|---|---|
| S1 착의 소매 | 새 세이브에서 CH01 첫 퇴실을 실행하고 소매 수선과 빈 행거·신발턱을 캡처한다. 즉시 재입장, 체크포인트 저장·종료·불러오기 뒤 다시 퇴실한다. 같은 절차를 CH02·CH03에 반복한 뒤 403호 형체와 탱크 왼팔을 나란히 캡처한다. | 장마다 착의 시작 이벤트 1회, 전체 3회. 재입장·로드 재생 0회, 후드·슬리퍼 복제 0개. 세 땀의 위치와 방향이 네 노출 지점에서 일치하고 일반 FOV에서 식별된다. |
| S2 11cm 문·고양이 | 최종 옥상 메시에서 문을 걸림 상태로 놓고 자유단과 닫힘 평면 사이의 수평 틈을 실측한다. 문짝 아래 높이도 별도로 재어 전체가 뜨지 않았는지 확인한다. 열린 자물쇠·삽입 열쇠 1개·추가 열쇠 3개를 확인한 뒤 문 입력이 바뀌지 않는지 기록한다. 고양이 충돌체를 자유단의 굽은 경로로 진입 20회·이탈 20회 통과시킨다. 사람 캡슐은 중앙에서 입력만으로 20회 접근한 뒤 문을 당겨 20회 통과하고 매번 손을 놓는다. | 자유단 간격 `11±1cm`, 문짝 하단 바닥 틈 2cm 미만, 고양이 40/40 통과, 데칼·몸 관통 0회. 사람은 당기기 전 0/20, 연 뒤 20/20 통과하고 매번 걸림 상태로 복귀한다. 열쇠 획득·인벤토리 변화·“열쇠로 열기” 프롬프트는 0회이며 문 옆 우회·내비게이션 탈출 0회다. |
| S3 P3 중단·재개 | 미조작, 두 유입 차단, 압력 해제 직후, 6초 대기 중간, 0kPa 첫 틱, 두 번째 틱, 본배수 직후의 7지점에서 저장·종료·불러온다. 각 세이브에서 정답을 완료하고, 별도 실행에서 가능한 순서 오류를 3회 만든 뒤 이탈·재진입한다. 원자료 다섯 개도 모든 부분집합으로 주입한다. | 밸브·게이지·남은 대기·수위·오답·힌트 단계가 로드 전후 동일하다. 밀폐 중 자동 감압 0회, 두 번째 0kPa 틱 전 본배수 성공 0회, 막힘·게임오버 0회다. 증거 중복 0회이며 32개 부분집합 중 명세한 10개에서만 `T_NEGLIGENCE`가 참이다. |
| S4 엔딩 공통 복원 | 최종 선택 직전 세이브를 복제해 A와 B를 각각 실제 입력으로 선택한다. `Choice→ActualStateRestored→Found0731→CommonDiscoveryCard→BranchCoda` 이벤트와 화면을 기록한다. 공통 카드 직전·직후 저장을 다시 불러 두 분기를 재확인한다. | A/B의 `Choice`는 각 선택값으로 공통 prefix보다 먼저 1회다. `ActualStateRestored→Found0731→CommonDiscoveryCard` 세 이벤트는 순서·내용·7월 31일 04:00 표기가 양쪽에서 동일하게 각 1회고, `BranchCoda`는 공통 prefix 뒤에 1회다. 뚜껑·점검봉·안경은 실제 위치에 각각 1개, 인체와 미래 문서는 0개이며 P5 증거·선택 액터의 충돌과 상호작용도 0이다. 복합가스 측정기→환기→하네스→두 구조자→점검구의 다섯 소리는 양쪽에서 같은 순서로 정확히 1회다. `T_FOUND_0731`은 실제 상태 복원 전 0, 공통 발견에서만 1이다. Coda는 A만 04:44 잔류, B만 가족 귀환·수용이며 로드 중복·교차 재생은 0회다. |
| S5 소지품 인과 연속성 | 구매 A/B/C를 각각 새 데이터 루트에서 시작한다. 편의점 귀환 프롭, C의 자동 하차·재파지, 04:33 병 개봉·음용·재밀봉, 고양이 물 용기의 세 결과(`cap+먼저 귀가 / cap+기다림 / cup`), 04:43 사다리 앞 봉지 배치, CH02 봉지·병 단서와 옥상 사고 현장·회수까지 상태와 화면을 이어서 기록한다. 폰·결제 수단·지갑 조건도 경로 전후 대조한다. | 세 프로필의 프롭·영수증·소지 상태가 선택과 일치하고 순간 이동·복제·소실이 0회다. C 자동 동작은 진행 입력을 잠그지 않는다. 개봉 병은 04:33 뒤 선택에 맞는 마개 상태다. 먼저 귀가하면 선택한 뚜껑/컵이 남고, 병뚜껑을 쓰고 기다리면 뚜껑은 병으로 돌아가며 젖은 원형 흔적만 남는다. CH02와 옥상의 병·뚜껑·봉지 증거는 같은 상태를 참조하고 폰·결제·지갑은 명세 밖에서 바뀌지 않는다. |
| S6 CH02 도움 요청·P1/P2 전달 | CH02 진입 직후 실제 전화 시도와 이웃 반응을 순서대로 실행하고 P1→P2와 P2→P1을 각각 진행한다. P1 메모와 P2의 CH01 영수증/404호 폰 원승인을 실제로 다시 읽는다. 각 단말에서 오답을 3회 내고, 시·분 선택 중과 압박 1~3단계에서 저장·종료·재실행한다. 한글·영문 자막, 자막 끔, 공포음 방향 표시, 노트북 스피커·헤드폰에서 폰 UI, 통화 실패 이유, 이웃 응답, P1 알람 메시·P2 PBR POS·세그먼트·키패드·셔터 상태를 녹화한다. CH01 계산대와 CH02 P2 전환 순간도 가까이서 녹화한다. | 모든 경로에서 지운이 도움을 청한 사실과 혼자 움직이는 이유가 대사·행동으로 성립한다. 메모를 닫거나 직원 호출만 해서는 해결되지 않고, P1 `05:10`·P2 `04:31` 확인 뒤에만 각 진실/중복 영수증이 1회 생긴다. 오답은 3단계에서 고정되고 P2 셔터 하단은 205cm 이상이며 출입을 막지 않는다. 로드 전후 표시값·압박·충돌이 동일하다. P1은 둥근 알람 외형과 `알람 시각 확인`, P2는 금전함·프린터 슬롯·3×4 키패드와 `원거래 시각 복원`으로 즉시 구분되며, POS 원본 PBR 재질·반사와 두 단말의 접지 그림자가 유지된다. CH02 전환 뒤 계산대 본체는 한 벌만 보이며 깜빡임·겹침·이중 그림자가 없다. 장식 키패드는 상호작용하지 않고 1.5cm 깊이의 큰 시·분·확인 버튼만 입력을 받는다. 키패드는 본체 외곽 안에 있고 표시창과 최소 2cm 떨어져야 한다. P1/P2 순서가 바뀌어도 누락·중복이 0회고 자막·UI·셔터와 소리 중 최소 두 채널로 필수 정보가 전달된다. |
| S7 엔딩 최종 연출 | S4와 같은 선택 직전 세이브에서 A/B를 다시 실행해 0.8초 실제 상태 프롭 복원, 안전 개방음, 세 번 긁힘과 0.6초 잔향, A/B별 폴리, 날짜 카드, A의 잔류와 B의 가족 귀환·수용 화면을 최종 아트·오디오로 녹화한다. 모노 폴드다운과 자막 켬·끔도 대조한다. | 프롭 복원은 선택 뒤 0.8초 안에 한 번만 발생하고 안전 개방음과 공통 발견은 양쪽에서 동일하다. A/B 폴리와 분기 화면은 교차 재생되지 않으며 가족은 B의 수용을 이해할 수 있게 제시된다. 무음 단절·클리핑·자막 선행 스포일러·임시 이미지가 0건이다. |
| S8 사고 물리 리허설 | 최종 충돌체와 최종 카메라로 세 번째 돌풍부터 고양이 진입, 호스 밟힘, 발판 이동, 지운의 추락, 뚜껑·탱크·안경·병·봉지의 최종 정지 상태와 고양이 이탈까지 실제 시간 순서로 20회 실행한다. 정상 FOV, 흔들림 감소, 30fps 하한에서 각각 시야선과 충돌 로그를 남긴다. | 고양이 생존과 호스 원인은 독립 흔적으로 읽히고 지운의 발이 발판을 벗어나 낙하하는 원인이 매회 같은 순서로 성립한다. 뚜껑·호스·발판·몸·프롭의 관통, 공중 순간 이동, 카메라 밖 필수 원인과 경로 봉쇄가 모두 0회이며 20/20에서 P5 네 진실에 필요한 최종 상태가 일치한다. |

1. 하네스가 UHT/UBT를 포함한 `IndieGameEditor Win64 Development`와
   `IndieGame Win64 Development`를 빌드하고 실행별 원본 로그와 종료 코드를
   보존한다. 경고를 숨기기 위해 로그를 잘라 내지 않는다.
2. Development 빌드 뒤 `run_rebirth_savegame_roundtrips.py`가 고유
   `-UserDir`에서 CH02 P1/P2 중단 상태 두 개, P5 조사·확정 네 경계와 P3
   일곱 경계를 각각 별도 쓰기·읽기 프로세스로 검증하고, A/B를 각각 선택
   전 쓰기→공통 발견 커밋→재실행 검증으로 확인한다. 쓰기와 공통 발견
   커밋 직후의 `.sav` 복제본도 보존한다. CH01 첫 계단 암전은 직전 4층
   스냅샷을 만든 직후 `MemoryBoundary`·`BagPlacedAtLadder`·원샷 비트를
   일부러 변경해도 저장본에 섞이지 않는지, CH02 기상 완료 시 쓰는 `Woke`
   저장에는 `Loop.Started / AlarmStopped / Standing`과 완료 원샷만 한 번
   남고 과도 태그는 사라졌는지를 전환 전·후 각 쓰기/읽기 4프로세스로
   추가 확인한다. 이어서 병뚜껑 선귀가/기다림, 종이컵 선귀가/기다림,
   그냥 지나감 다섯 상태를 각각 쓰기·CH02 읽기 10프로세스로 재구성해
   저장값, 생산 CH01 프롭과 CH02 월드의 단일 복원 프롭을 대조한다.
   두 표현 모두 표시·무충돌·상호작용 해제를 만족하고, 지나감은 0개이며
   장 전환 후 CH01 임시 액터가 중복되지 않아야 한다. 상위 하네스는
   총 46개 결과의
   로그·세이브 경로와
   SHA-256·커밋 SHA,
   읽기 뒤 원래 슬롯 삭제, 공통 prefix 1회성·분기 배타성 마커를 다시
   검사한다.
3. `Run-Rebirth-CH02FreedomSpikes.ps1`이 P1→P2, P2→P1, P1 생략,
   P2 생략, 양쪽 생략을 각각 빈 `-UserDir`의 독립 프로세스로 실행한다.
   해결한 퍼즐만 해결 플래그·오답 압박을 남기고, 생략 경로는 정확히 두
   진실만으로 C3에 합류해 CH03 인계까지 도달해야 한다. 양쪽 생략은
   404호 플래너와 관리 민원을 실제 대체 원자료로 사용한다.
4. `Run-Rebirth-CheckpointAnchorSpikes.ps1`이 CH02 복도·편의점과 CH03
   404호·침수 복도·옥상 다섯 체크포인트를 각각 쓰기 프로세스와 읽기
   프로세스로 실행한다. 읽기는 저장된 실제 맵으로 `IGResumeSave` 재진입한
   뒤 올바른 챕터 디렉터가 생성됐는지, 저자 앵커의 위치·회전인지, 축소
   30×92cm 캡슐이 벽과 겹치지 않는지, 34×96cm 플레이어 중심 아래
   90~104cm에 `Pawn` 차단 바닥이 있는지 확인한다. 다섯 `.sav` 복제본과
   열 개 로그의 SHA-256을 보존하고 성공한 읽기 뒤 원래 슬롯을 삭제한다.
5. 하네스의 Map Check가 `Prologue_Morning`에서 오류 0으로 끝나는지
   확인한다.
6. A/B 자동 런타임 로그가 `collision_route`, `audio_synthesis`, `audio_queue`,
   `s5_item_continuity`, `p3_p5`, `savegame_v3`, 해당 엔딩과 `complete`
   마커를 모두 갖는지 확인한다.
   A/B 모두 CH01 구매·결제·고양이 선택의 실제 이벤트 핸들러부터 CH02
   P1/P2·관리 민원과 CH03 인계까지 같은 세션에서 통과해야 한다. A는
   P1→P2, B는 P2→P1 순서로 진행한다. 두 경로는 P1/P2의 실제
   `시 / 분 / 확인` 버튼 6개와 P1 알람/P2 POS 고유 외형 계약, 오답 상한 두 개,
   P2 셔터 하단 205cm와 04:31 폰 승인 기록 패널을 확인하고, 장별 착의
   중복 0회와 정적 소매 세 땀을 확인한다. 자동 로그에는
   `approval_screen=1 / cat_aftermath=1 / authored_housings=2 /
   layered_displays=2 / time_entry_physical=2 /
   pressure_caps=2`와 각자의
   `route_order=p1_p2 / p2_p1`가 모두 있어야 한다. S5는
   구매 A/B/C×마개 유실/재밀봉×사고 봉지/CH02 재활용 마대의
   12건을 실제 `AIGItemContinuityDressing` 액터로 생성해 상태·태그·
   사고 위치·싱글턴과 파괴 후 중복 0을 검사한다. A/B 모두
   세 상태 옥상문의 고양이·사람 캡슐 sweep과 엔딩 공통 탱크 뚜껑
   인스턴스 무복제·실루엣 숨김·충돌 해제를 확인한다.
   자동 런타임은 아파트→복도→상행 계단→5층→옥상→물탱크 상부의
   바닥 지지점 41개와 실제 34×96 캡슐의 평면 여유 구간 9개,
   M0/M1/M1b/M2/M3/M4 바람·압력/M5 생산 생성기 19개를 각각 4,096샘플
   렌더링한 PCM의 무음 0·클리핑 0·M5 45.05초 종료와 오디오 컴포넌트
   생성/정지, 같은 프로세스 안의
   실제 디스크 v3 저장·복원·삭제를 검사한다. 계단 `step-up` 자체는
   고정 높이 sweep으로 합격시키지 않고 S8 실제 입력으로 확인한다.
   `-nullrhi`이므로 전체 이동 공간, 실제 청감·렌더링·화면 가독성,
   실제 문 통과 입력이나 사람 입력을 증명하지 않는다. CH01 종단 경로는
   `종이컵+기다림` 결과 프롭의 표시·무충돌·상호작용 해제를 검사한다.
   별도 저장 하네스도 다섯 선택의 CH01·CH02 물리 표현을 10프로세스로
   검사하며 위 dirty 회귀 스냅샷에서는 모두 통과했다. 다만 최신 clean SHA의
   정식 증거는 아니고, 어느 자동 경로도 세 용기 결과의 최종 화면을 증명하지
   않는다. 04:33·04:43의 시간 연출, 손 프롭의 보이는 소멸,
   봉지·병의 시각적 동일성, 폰·결제·지갑 상태도 수동 승인이 필요하다.
   필수 PASS 마커가 있더라도 비허용 Ensure·Error·Fatal이 하나라도
   있으면 해당 자동 단계는 실패다. 하네스의 현재 허용목록은 0개다.
7. `FEASIBILITY.md`의 S1~S8을 위 수동 합격 사례대로 실제 종료·재실행까지
   수행한다. 각 사례에 무편집 영상, 시작·종료 세이브, 이벤트 로그와
   결과표를 남긴다.
8. R1~R4를 실제 입력으로 완주한다. R1, R2, R3, R4마다 재사용하지 않는
   고유 절대 `-UserDir`를 지정하고, 실행 전에 해당 Windows 데이터 루트를
   새 빈 폴더로 만든다. 슬롯 이름만 바꾸거나 같은 `Saved` 폴더에서 세이브를
   지우는 방식은 격리 증거가 아니다. 각 실행의 첫 입력 전에 C1~C6,
   `NarrativeDebt`, P1~P5 관찰·확정, 구매·결제·고양이 상태, 엔딩 선택과
   일회성 사건이 모두 `0 / false / unset`인지 덤프한다. 새 게임의
   시작 세이브와 완주 세이브의 경로·크기·SHA-256, 종료 상태 덤프를 남기고,
   프로세스 종료 뒤 데이터 루트를 삭제해 경로가 남지 않았다는 정리 기록도
   보존한다. 각 경로는 C1~C6, `NarrativeDebt=0`, 선택한 구매·결제·고양이
   상태, P5 네 진실과 엔딩 이벤트 순서를 덤프한다.
9. `STORY_BIBLE_REBIRTH.md` §15의 공간 왕복·사고 물리 회귀를 최종
   충돌체로 실행하고 공중 낙하·관통·진행 불능을 0으로 만든다.
10. 새 세이브와 v3 중단 세이브에서 종료·재실행을 확인한다. 에디터 세션
   안의 리셋만으로 저장 복원을 통과 처리하지 않는다.
`Run-Rebirth-ReleaseValidation.ps1`은 정적 계약, Development 빌드,
Map Check, A/B 무렌더 자동 런타임과 Shipping UAT를 묶는다. S1~S8의 수동 전체,
R1~R4 실제 입력, 시청각 가독성, 성능과 별도 PC 실행은 자동화하지 않는다.
각 수동 절차의 원문 인자·종료 코드·영상을 매니페스트에 개별 기록한다.
절차가 문서에 있거나 자동 하네스가 PASS라는 사실만으로 G3를 완료하지 않는다.

어느 하나라도 빠지면 G3는 `미검증` 또는 `실패`다. 자동 그레이박스 한 번의
PASS만으로 “런타임 통과”라고 쓰지 않는다.

성능 측정은 G6의 Shipping 후보에서만 승인하며 G3 합격 조건에 포함하지
않는다. Development·Editor 성능 수치는 G3 진단 자료로 남길 수 있지만
`PERFORMANCE.md`의 출시 성능 판정을 대신하지 않는다.

## G4 — 초견 플레이

G3를 통과한 같은 SHA의 후보로 **진짜 초견 최소 16명**의 첫 플레이를
진행한다. 결제·소지품에 따른 정보 노출 편향을 분리하기 위해
`Wallet` 코호트 8명과 `Hood-card (no-wallet)` 코호트 8명을 채운다.
두 코호트에서 각각 선착순 적격 4명을 결과를 보기 전에 고정해
`core_panel_id` 8명으로 잠근다. `STORY_BIBLE_REBIRTH.md` §15에서
집단 이름 없이 단순히 `8명 중`이라고 쓴 공통 기준만 이 고정 코어
패널로 판정하고, 전체 16명 결과와 코호트별 결과도 함께 공개한다.
탈락자나 실패자를 보고 더 유리한 사람으로 코어 패널을 교체하지 않는다.

### 분모 잠금

- `결제 수단 2종 모두에서 8명 중`은 코어 패널로 줄이지 않고
  `Wallet` 8명과 `Hood-card(no-wallet)` 8명 **각 코호트 전체**를
  독립 분모로 판정한다.
- `선택해 시작한 퍼즐마다 초견 8명 중`은 퍼즐별 서로 다른 초견
  시작자 8명이 찰 때까지 추가 모집한다. 그 퍼즐을 시작하지 않은 사람,
  반대 엔딩 재생과 2회차 플레이는 분모에 넣지 않는다.
- `퍼즐을 완료한 플레이어 6명 이상`, `P3를 푼 플레이어 6명 이상`,
  `마대의 병을 본 플레이어 6명`, `A를 본 플레이어`, `엔딩 B를 본
  플레이어`처럼 자격 집단이 적힌 기준은 해당 자격을 실제로 충족한
  전체 사람을 분모로 두고, 문서가 요구한 최소 적격 인원도 별도로
  채운다. 선택 직전 세이브로 반대 엔딩을 본 기록은 엔딩별 사후 이해
  기준에는 쓸 수 있지만 최초 엔딩 선택이나 초견 이해 기준에는 쓰지 않는다.
- `2회차 CCTV를 본 플레이어 6명`은 아래의 CH01부터 재완주한 별도
  2회차 집단만 사용한다.
- 한 사람이 여러 자격 집단에 들어갈 수는 있지만, 어떤 조건부 분모도
  코어 패널 8명으로 임의 치환하거나 미관찰자를 실패/성공으로 채우지 않는다.

### 진행 전

- 참여자는 스토리와 퍼즐 정답을 모르는 사람으로 모집한다.
- `first_time_attestation=true`를 시작 전에 받고, 과거 데모·영상·방송·
  정답 문서 노출이 확인된 사람은 표본에 넣지 않는다. 실행 불능처럼
  사전 등록한 기술적 제외 사유만 `exclusion_reason`으로 남길 수 있으며,
  이해도·공포 점수·엔딩 선택을 본 뒤 제외하지 않는다.
- 조작표와 안전·중단 안내만 제공하고, 단서 위치·정답·엔딩 의미는 말하지
  않는다.
- 참가자마다 새 빈 데이터 루트와 고유 `-UserDir`를 사용한다. 기존
  세이브·설정·진실·힌트·일회성 상태가 없는지 시작 덤프와 해시로 확인한다.
- 익명 ID, 하드웨어, 출력 장치, 난이도, 접근성 설정, 시작 세이브
  체크섬, 코호트, 코어 패널 포함 여부와 첫 플레이 여부를 기록한다.
- 코어 패널은 각 코호트에서 사전 적격 조건을 먼저 만족한 4명으로 고정하고
  등록 시각과 잠금된 명단 해시를 남긴다.
- 경로 성향을 배정할 때는 “천천히 조사”, “목표 위주 이동”처럼 행동
  성향만 말하고 퍼즐·장소·증거 이름은 알려 주지 않는다.
- 첫 선택은 참가자가 자연스럽게 고르게 한다. 전체 초견 최초 완주에서
  엔딩 A와 B가 **각각 최소 3명**이어야 한다. 어느 한쪽이 3명 미만이면
  16명을 넘겨 새 초견 참가자를 추가 모집하되, 부족한 엔딩을 고르도록
  지시하지 않는다. 기존 참가자의 반대 엔딩 재생과 체크포인트 재생은
  최초 엔딩 표본에 넣지 않는다.

### 진행 중

- 진행자가 막힘을 해결해 주지 않는다. 도움 요청 시각과 질문 원문만
  기록하고, 안전 문제나 실행 불능이 아니면 답하지 않는다.
- CH01 이탈 시각, P1~P5 진입·해결·포기 시각, 힌트 단계, 오답 수,
  왕복 횟수, 첫 엔딩 선택, 크래시·진행 불능을 기록한다.
- 화면과 게임 오디오를 끊지 않은 원본 영상으로 남긴다. 얼굴·마이크를
  녹화할 때는 별도 동의를 받는다.

### 완료 직후

1. 다른 엔딩이나 정답을 보여 주기 전에
   `STORY_BIBLE_REBIRTH.md` §15의 핵심 질문과 감정 질문을 받는다.
2. 진행자의 설명·유도 질문·정답 공개·다른 엔딩 재생 전에 첫 응답 원문과
   제출 시각을 잠그고 SHA-256을 남긴다. 잠금 뒤에는 원문을 수정하지 않고
   정정은 별도 레코드로만 추가한다.
3. 첫 응답을 잠근 뒤 선택 직전 세이브로 다른 엔딩을 보여 주고, 두 엔딩의
   공통 현실과 차이를 다시 묻는다. 두 번째 응답은 첫 응답과 분리하며
   초견 이해율이나 최초 엔딩 표본에 합산하지 않는다.
4. **별도의 진짜 2회차 CCTV 표본을 최소 6명** 확보한다. 첫 회차가 끝난
   참여자 중 동의한 사람을 새 빈 데이터 루트에서 CH01부터 엔딩까지
   다시 플레이하게 하며, 체크포인트·정답 제공·개발자 조작을 쓰지 않는다.
   CCTV를 실제로 발견·해석한 시각, 경로, 오답과 이해 응답을
   `second_run_cctv.csv`에 기록한다. 반대 엔딩만 재생하거나 CCTV 장면만
   불러온 세션은 이 6명에 포함하지 않는다.
5. §15의 퍼즐 중앙값, 이해율, 공포·감정, 개연성, 자유도 기준을
   `participants.csv`, `puzzle_metrics.csv`, `comprehension.csv`,
   `issues.csv`로 집계한다. 코어 패널 8명, 전체 초견, Wallet,
   Hood-card(no-wallet), 퍼즐별 초견 시작자, P3 완료자, 마대 관찰자,
   최초 엔딩 A/B, 엔딩별 전체 시청자와 2회차 CCTV를 서로 다른 열과
   분모로 유지한다.
6. 진행 불능·크래시·필수 단서 미노출은 평균값으로 덮지 않고 개별 P0
   이슈로 남긴다.

전체 16명, 코호트별 8명, 고정 코어 패널 8명, 퍼즐별 초견 시작자 8명,
§15가 정한 조건부 적격 인원, 최초 엔딩별 3명과 진짜 2회차 CCTV 6명 중
하나라도 부족하거나, 개발자가 설명한 뒤 정답률을 재는 방식은 G4
증거가 아니다.

## G5 — 콘텐츠·접근성

G5는 최종 아트·오디오·UI가 들어간 후보에서 다음 항목을 확인한다.

현재 자동 사전 점검은 `Scripts/Test-Rebirth-AccessibilityContract.ps1`이
담당한다. 저장 가능한 설정 구조, P3/P4 자동 힌트 임계값, P1~P4 수동 힌트,
80/55/45초 압박 주기, P3/P4 압박 시계·P4 반복·완료의 저장 배선과 결합
왕복 오라클, 카메라·손전등 감소, 방향 파형, 관찰 뒤 P5 자동 연결, 독립된
음성 자막·비언어음 자막 게이트, 85~200% 크기·0~100% 배경 불투명도·
80~100% 안전 영역을 검사한다. `Test-Rebirth-DialogueContract.ps1`은
속마음·대화·음성·기기 채널, 화자명, 우선순위 중단 복귀, 중복 병합, 실제
한글 글리프 폭 기반 2~3줄 무손실 페이지 분할과 읽기 시간, 위아래 HUD 레인,
모션 감소 전환, CH02 실제 기기 메시지를 별도로 검사한다. 두 검사는
720p/900p/1080p/1440p 경계 배치, 안전 개방·엔딩 B 암전 자막의 무겹침,
홀드 길이·토글 상태기와
키보드·게임패드 기본 매핑, 마지막 실제 입력
장치에 따른 상호작용·힌트·설정·엔딩 안내 전환을 검사한다. 이 검사는
F10/Menu 패널의 입력 배선과 항목 존재도
검사하지만 화면 가독성, 실제 프로세스 재실행 저장, 실제 멀미·광과민 반응,
자막 타이밍·실물 게임패드와 출력 장치 조합을 승인하지 않으므로 아래 사람
검증을 대체하지 않는다.

CH02의 04:31 원승인은 일반 종이 패널을 쓰지 않고 검은 휴대폰 알림 화면으로
분리한다. 정적 사전 점검은 본문 19px·발신/상태 15px·푸른 읽지 않음 점,
720p/900p/1080p/1440p의 폰 프레임·알림 본문·닫기 안내 경계와 종이·영수증·
휴대폰에서 마지막 입력 장치 하나만 안내하는 계약을 검사한다. 실제 폰 메시와
배경의 대비·거리별 판독성은 여전히 사람 검증 대상이다.

무창 QA 실행에서는 저장값을 건드리지 않는
`-IGAccessibilityPreset=Story|Standard|Silent`, `-IGReducedMotion`,
`-IGReducedFlicker`, `-IGFearDirection`, `-IGAutoConnectEvidence`,
`-IGToggleHolds`, `-IGHoldScale=0.25..1.0`, `-IGNoSubtitles`,
`-IGNoSoundCaptions`, `-IGCaptionScale=0.85..2.00`,
`-IGCaptionBackground=0.00..1.00`, `-IGCaptionSafeArea=0.80..1.00`을 사용할
수 있다. 명령행 값은 해당 프로세스의 유효 설정에만 합성한다.

수동 판정은 Microsoft [XAG 101 Text display](https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/101)과
[XAG 104 Subtitles and captions](https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/104)의
글자 크기·확대·왼쪽 정렬·화자 식별·비언어음·두 줄 우선·불투명 배경
원칙과 UE 5.8의 DPI Scaling·Safe Zone을 기준으로 한다. 자동 수치는 사람의
실제 독해·청취 속도 검증을 대신하지 않는다.

- 최종 메시와 재질에 디버그 도형·임시 텍스트·누락 텍스처가 없고,
  소매 세 땀·발자국·호스·발판·안경·탱크 실루엣이 일반 FOV에서 읽힌다.
- M0~M5, 생활 베드와 필수 큐가 상태에 맞게 전환되며 무음 단절·중복
  스팅·클리핑이 없다. 공간 전환은 1.6초 이상 이어지고, 최초 탱크 개방과
  손전등 복귀 리빌은 재생 중이던 심박까지 끊은 6초 침묵 뒤 심박 한 번만
  들린다. M5는 45초 동안 유지되고 빈 마지막 음을 물방울 한 번이 닫는다.
- 한글 문서·폰·영수증·자막이 1280×720과 목표 해상도에서 잘리거나
  겹치지 않고, 입력 장치가 바뀌면 안내도 일치한다.
- 속마음·기기 메시지·음성 자막은 화자를 혼동하지 않고, 환경음 자막과
  동시에 떠도 서로 겹치지 않는다. 200%에서 본문은 버려지지 않고 다음
  페이지로 이어지며 노트·설정·챕터 카드 뒤에는 남은 읽기 시간부터 재개한다.
- 필수 정보는 색 하나로만 구분하지 않고 형태·텍스트·소리를 함께 쓴다.
  본문·자막은 배경과 구분되며 텍스트 크기·안전 영역 설정 뒤에도 잘리지
  않는다.
- 이야기/기본/침묵 난이도에서 정답과 엔딩 조건은 같고 힌트·압박 간격만
  달라진다.
- `카메라 흔들림 감소`, `공포음 방향 표시`, `자동 연결`을 켜고 껐을 때
  필수 정보가 사라지거나 자동 연결 범위를 넘어 진실이 조기 확정되지 않는다.
- 신규 CH03 시작에서는 렌즈 물방울이 한 번만 나타나고, 같은 저장을 불러온
  복원 진입과 일반 문서 캡처에서는 0회여야 한다. 기본 설정에서는 짧게 아래로
  미끄러지고 모션 감소에서는 같은 위치에서 페이드만 하며, 어느 경우에도
  목표·조준점·상호작용·자막의 대비와 판독성을 낮추지 않는다.
- 헤드폰 스테레오, 노트북 스피커, TV/일반 스테레오와 모노 폴드다운에서
  P2 호출, P3 블리드, P4 물소리, P5 긁힘의 순서와 의미를 구분한다.
- 키보드·마우스와 게임패드에서 타이틀의 새 게임·이어하기, 일시정지·복귀,
  조사, 홀드, 기록 닫기, 설정, 크레딧, 정상 종료, 엔딩 선택, 재시작과 저장
  복원이 모두 가능하다. 저장이 없거나 현재 스키마와 호환되지 않으면
  이어하기는 비활성 상태여야 한다. 호환 저장이 있는 새 게임은 삭제 경고와
  두 번째 확인 전에는 슬롯을 지우지 않으며, 취소·항목 이동 뒤에도 저장이
  그대로 남아 있어야 한다.
- 화면 설정에서 720p·1080p·1440p, 전체 화면·테두리 없는 창·창 모드,
  Low/High, VSync와 30/60/무제한 프레임을 키보드·마우스·게임패드로 모두
  변경한다. 적용 뒤 유지 확인, 명시적 되돌리기, Esc/B/View 취소와 10초
  무입력 자동 복원이 해상도·화면 모드·품질·VSync·프레임 제한을 함께
  되돌리는지 확인한다.
- 저장 폴더를 읽기 전용으로 만든 실패 주입에서 새 게임은 기존 상태를
  초기화하지 않고 멈추며, 저장·불러오기 실패 문구와 Error 로그가 각각
  한 번 나타나야 한다. 권한을 복구한 뒤 같은 세션에서 재시도할 수 있어야 한다.
- 연속 홀드는 길이 조절 또는 토글 대체 입력을 제공하고, 카메라 흔들림
  감소 상태에서는 필수 시선 이동을 강제하지 않는다. 점멸·왜곡을 줄여도
  퍼즐과 공포 사건의 발생 여부를 알 수 있어야 한다.
- 설정은 재실행 뒤 유지되고 초기화 기능이 작동한다.

기본 조합, 모든 보조 기능을 켠 이야기 모드, 보조 기능을 끈 침묵 모드,
자동 연결만 켠 모드를 최소 경계 조합으로 실행한다. 그 외 조합은
쌍대 조합으로 모든 설정의 켜짐/꺼짐이 다른 설정과 한 번 이상 함께
검증되게 한다. 결과는 해상도·입력·출력 장치와 함께 표로 보존한다.

## G6 — Shipping 패키징·성능

1. G3와 G5를 통과한 같은 SHA에서 전체 하네스를 고유한 보관 경로로
   실행한다.

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\Run-Rebirth-ReleaseValidation.ps1 -ArchiveDirectory "<고유 절대 경로>"
   ```

   기본 보관 위치는 실행 ID별로 고유하다. `-ArchiveDirectory`를 직접
   지정했다면 기존 파일이 없는 경로만 허용한다. 하네스는 UAT
   `BuildCookRun`에 `-clientconfig=Shipping`을 명시해
   Build/Cook/Stage/Package하고 prerequisites와 UE 5.8 공식 AppLocal CRT를
   포함한다. 현재
   `DefaultGame.ini`의 에디터 기본값도
   `PPBC_Shipping / FullRebuild=True / ForDistribution=True`로 잠겨 있다.
   그래도 수동 메뉴 패키지는 clean SHA·실행 ID·명령행·로그가 고정되지
   않으므로 정식 증거로 섞지 않는다. `ShippingPackage.log`, UAT 종료 코드,
   EXE·PAK·UTOC·UCAS의 존재와 0바이트 여부를 확인하고 파일별 경로·크기·
   SHA-256을 `ShippingArchiveManifest.json`에 남긴다. 아카이브의 실제
   내부 `IndieGame-Win64-Shipping.exe`의 제품 VERSIONINFO를 루트
   `IndieGame.exe`에 동기화하고 `OriginalFilename`을 루트 파일명으로
   보정한다. 그 뒤 FileDescription·FileVersion·ProductName·ProductVersion·
   CompanyName·LegalCopyright·InternalName·OriginalFilename이
   `4:44 AM / 1.0.0 / easygap` 계약과 일치하며 엔진 빌드 문자열이 노출되지
   않는지 검사한다. 내부 게임 EXE와 같은 폴더의 `msvcp140_2.dll`과
   `vcruntime140_1.dll`이 모두 14.50.35719.0 이상인지 확인하고 경로·버전·
   SHA-256을 매니페스트에 기록한다. 같은 `IndieGame.exe`에서 32px 대표
   아이콘을 추출해
   기준 ICO와 1,024픽셀을
   전부 대조하며, 추출 PNG·검증 로그와 각각의 SHA-256도 같은 실행의
   매니페스트와 요약에 기록한다. 그 직후 내부
   `IndieGame-Win64-Shipping.exe`를 A/B 각각 별도 `-UserDir`에서
   `-nullrhi -nosound -RenderOffscreen`으로 실제 실행한다. 각 프로세스는
   종료 코드 0과 정확한 `REBIRTH_PACKAGED_RUNTIME PASS` 영수증을 남겨야
   하며, 영수증·실행 파일 SHA-256과 데이터 루트를 요약에 기록한다. 시작
   거부, 시간 초과, 비정상 종료, 누락·불일치 영수증은 자동 하네스 실패다.
   A/B가 끝나면 최초 매니페스트의 모든 파일 수·크기·SHA-256을 다시 검사해
   패키지가 실행 중 바뀌거나 매니페스트 밖 파일을 만든 경우도 실패시킨다.
   이 개발 PC의 무렌더 패키지 실행은 3항의 클린 환경과 4항의 실제 입력
   완주를 대신하지 않는다. 스토어용
   distribution 서명·인증은 G6 이후 플랫폼 게이트에서 별도로 처리하며,
   VERSIONINFO 동기화 뒤 서명해 서명된 파일을 다시 수정하지 않는다.
2. 방금 만든 정확히 같은 Shipping 아카이브를 `PERFORMANCE.md`의 여섯
   필수 장비와 잠긴 해상도·품질 조합에서 각각 세 번 측정한다. p95,
   1% low, hitch, RAM, VRAM, 설치 크기와 `Saved` 증가량을 원본
   Unreal Insights·MemReport·실행 로그와 함께 보존한다. 한 장비라도
   빠지면 G6은 `BLOCKED`, 기준을 넘으면 `FAIL`이다.
3. Unreal Editor, Visual Studio, Windows용 UE 빌드 도구와 개발용 DLL이
   설치되지 않은 **클린 VM 스냅샷 또는 별도 물리 PC**에서 압축을 풀고
   실행한다. VM은 실행 전 기준 스냅샷 ID와 설치 프로그램 목록을, 물리
   PC는 OS 빌드와 설치 프로그램 목록을 증거로 남긴다. 개발 PC 안의 새
   Windows 사용자 계정 실행은 사전 점검일 뿐 G6의 클린 환경 증거로
   인정하지 않는다.
4. 새 세이브로 CH01부터 A/B를 각각 완주하고, 선택 직전 저장 복원과
   재시작 입력을 확인한다.
5. `SAVE_COMPATIBILITY.md`에서 잠근 v1·v2·v3 세이브 픽스처를 각각
   복사해 마이그레이션, 슬롯 선택, 최신 자동저장 복원을 확인한다.
   픽스처의 출처 빌드·스키마·SHA-256과 버전별 기대 상태 오라클을
   대조하며, 개인 세이브나 현재 코드로 손 편집한 구버전 표기를 증거로
   사용하지 않는다.
6. 필수 LFS 에셋·맵·폰트·자막·오디오가 패키지에 포함됐는지 확인하고
   누락 텍스처, 기본 머티리얼, 참조 실패를 0으로 만든다.
7. 정상 종료와 강제 종료 뒤 재실행을 확인하고 크래시 덤프·Fatal/Error
   로그·손상 세이브를 검사한다. Crash, Fatal과 Ensure는 0건이어야 하며,
   Error는 원인·영향·허용 사유가 매니페스트의 승인 목록에 없는 항목이
   하나라도 있으면 실패다.
8. `ASSET_POLICY.md`의 라이선스와 실제 포함 파일을 대조하고 배포물의
   크레딧·고지·개인정보·디버그 파일을 최종 확인한다.

UAT 성공만으로 G6를 통과 처리하지 않는다. 패키지 자체를 별도 환경에서
실행하고 두 엔딩·세이브·배포 파일 증거까지 있어야 한다. 플랫폼 스토어
인증은 이 저장소의 G6 이후 별도 게이트다.

## 최종 판정

각 게이트는 `PASS`, `FAIL`, `BLOCKED` 중 하나로 기록한다. `미실행`,
`일부 확인`, `과거 빌드 통과`는 모두 PASS가 아니다. 최종 출시 승인은
G1~G6가 같은 커밋의 증거로 모두 PASS이고, 열린 P0/P1 출시 차단 이슈가
0개일 때만 내린다.
