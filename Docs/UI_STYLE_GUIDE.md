# UI 스타일 가이드 — 대화·자막 레이어

기준일: 2026-08-06 (적용 범위 각주 2026-08-10)
적용 범위: 인게임 대화, 내면 독백, 기기 메시지, 음성 자막, 비언어음 캡션

> **이 문서는 대화·자막 레이어(§19의 L1·L2)만 다룬다.** 화면 계층, 조사 기록
> 화면, 목표 제시 정책, 힌트 정책, 리셋 UX, 온보딩, 접근성 8항목을 포함한
> 전체 UI·UX 계약은
> [STORY_BIBLE_MISSING_FLOOR.md §19](STORY_BIBLE_MISSING_FLOOR.md)이며,
> 충돌하면 그쪽이 우선한다. 아래의 표면·타이포·반응형 계약은 「없는 층」에서
> 그대로 계승한다.

## 시각 방향

이 게임의 UI는 별도의 화려한 레이어가 아니라 새벽의 어두운 생활 공간 위에
잠깐 맺히는 **습윤한 광학 유리**처럼 보여야 한다. 화면을 가리는 검은 사각형,
굵은 상태 막대, 고채도 네온, 과한 블러와 게임 밖의 장식은 사용하지 않는다.

- 기본 대화창은 화면 폭 약 56%를 시작점으로 하되 1080p에서는 840px을 넘지
  않는다. 환경을 계속 읽을 수 있는 하단 중앙의 음수 공간을 우선한다.
- 표면은 charcoal과 oxidized green 사이의 저채도 smoked glass다. ImageGen
  원본에서 파생한 미세 필름은 낮은 알파로만 섞고 텍스트 대비를 해치지 않는다.
- 화자명은 본문보다 작은 캡슐 안에 둔다. 채널 색은 청록, 내면 독백은 청회색,
  위험 우선순위는 억제된 적갈색만 사용한다.
- 비언어음은 대화창과 합치지 않는다. 내용 폭에 맞는 별도 캡슐과 코드로 그린
  파형을 사용해 대사와 구분한다.
- 본문은 따뜻한 ivory, 보조 문자는 낮은 명도의 청록으로 제한한다. 배경 불투명도
  42% 미만에서만 외곽선을 켜므로 기본 화면에서 글자가 두껍게 번지지 않는다.

## 반응형·접근성 계약

| 항목 | 기본 | 허용 범위·동작 |
|---|---:|---|
| 자막 크기 | 100% | 85~200%. 125% 초과 시 페이지당 3줄 |
| 표면 불투명도 | 82% | 0~100%. 텍스트 가독성 설정과 시각 질감을 분리 |
| 안전 영역 | 90% | 80~100%. 720p~1440p 네 해상도에서 실측 |
| 본문 줄간격 | 132% | 3줄일 때 136% |
| 등장·퇴장 | 240ms / 160ms | 동작 감소 사용 시 위치 이동 제거 |
| 음성/비언어음 | 독립 | 자막과 소리 캡션 토글을 별도로 유지 |

200% 화면은 기본 UI를 단순 확대해 자르는 방식이 아니다. 안전 영역 안에서
다시 줄바꿈하고 무손실 다음 페이지를 만든다. 화자 표시, 비언어음 파형, 배경
불투명도 설정은 확대 상태에서도 유지한다.

## 구현·성능 계약

- 대화 큐와 접근성 상태는 기존 이벤트 기반 네이티브 HUD를 유지한다. 단일 하단
  레이어 때문에 전체 CommonUI 이관을 강제하지 않는다.
- 64px 런타임 9-slice 마스크 하나를 재사용한다. 라운드 표면마다 새 텍스처나
  위젯을 만들지 않는다.
- `T_HudDialogueFilm_D`는 `TEXTUREGROUP_UI`, `NoMipmaps`, `NeverStream`으로
  임포트한다. 월드 텍스처 스트리밍 풀과 LOD 변화에 영향을 주지 않는다.
- 글자 폭과 줄바꿈은 대화가 바뀌거나 해상도·접근성 배율이 바뀔 때만 다시
  계산한다. 프레임마다 원문을 재분석하는 raw binding은 두지 않는다.
- 패널 렌더링은 대화 또는 소리 캡션이 보일 때만 실행한다. 숨겨진 상태의 UI
  렌더 비용은 0이다.

## 아트·검증 근거

- 콘셉트: `Content/SourceArt/AI/DialogueHUDConcept_v1.png`
- 표면 원본: `Content/SourceArt/AI/TextureHudDialogueFilm.png`
- 파생 PNG: `Content/SourceArt/T_HudDialogueFilm_D.png`
- 런타임 UAsset: `Content/Prototype/Textures/T_HudDialogueFilm_D.uasset`
- 기본 Shipping 캡처: `Docs/Media/dialogue-hud-default-1080.png`
- 접근성 200% 캡처: `Docs/Media/dialogue-hud-accessibility-200-1080.png`

자동 검증은 1280×720, 1600×900, 1920×1080, 2560×1440에서 기본형과
200%형을 각각 렌더한다. 각 화면의 실제 글리프 경계, 안전 영역, 화자, 무손실
이어짐, 키보드·게임패드 입력 경로를 Shipping 실행 파일에서 확인한다.

참조한 엔진·접근성 기준:

- Unreal Engine 5.8 UMG Styling: 9-slice와 UI 텍스처 스타일링
- Unreal Engine CommonUI Design Guidelines: 기존 단일 레이어 HUD의 이관 판단
- Unreal Engine UMG Optimization Guidelines: 이벤트 기반 갱신과 raw binding 회피
- Unreal Engine DPI Scaling / Safe Zones: 해상도와 안전 영역 대응
- Xbox Accessibility Guideline 104: 화자 식별, 비언어음 구분, 조절 가능한 배경

## 설정 화면 계약

기준일: 2026-08-12

적용 범위: 화면·성능, 접근성, 설정 내 하위 화면

설정은 항목 수를 한 화면에 모두 펼치는 중앙 정렬 목록으로 만들지 않는다.
왼쪽 카테고리 레일과 오른쪽 옵션·설명 영역을 고정하고, 현재 카테고리에
속한 항목만 펼친다. 플레이어는 값을 바꾸기 전에 해당 설정이 화면·입력·공포
연출에 미치는 영향을 같은 화면에서 읽을 수 있어야 한다.

- 선택 상태는 색상 하나에 의존하지 않는다. 선택 표면, 좌측 3px 인디케이터,
  본문 명도와 글자 역할을 함께 바꾼다.
- 항목명은 왼쪽, 값은 오른쪽에 정렬한다. 조절 가능한 값만 `- 값 +`으로
  표시하며, 열기·적용·닫기 같은 동작을 숫자 설정처럼 보이게 하지 않는다.
- 화면 설정은 `화면 / 성능 / 플레이 보조 / 변경 사항`, 접근성은
  `게임 진행 / 움직임 / 정보 안내 / 자막 / 입력 / 관리`로 묶는다.
- 접근성 변경은 즉시 저장하고, 화면 모드·해상도·품질은 명시적 적용 뒤
  10초 확인을 거친다. 두 저장 정책을 헤더에서 플레이어에게 알린다.
- 자막 카테고리는 크기·배경 농도·안전 영역을 실제 캡션 표면으로 미리 본다.
- 카테고리 클릭은 값을 변경하지 않고 첫 항목으로 포커스만 옮긴다. 키보드,
  마우스, 게임패드는 같은 기능과 순서를 제공한다.
- 카테고리 옆에는 설명 없는 항목 수를 노출하지 않는다. 정보 구조는 레이블과
  현재 선택 표면으로 충분히 전달하고, 숫자는 실제 설정값에만 사용한다.
- 지원 해상도에서 카테고리·옵션의 포인터 판정은 렌더 좌표와 같은
  `IGSettingsMenuLayout`을 사용한다. 최소 카테고리 높이는 48px, 옵션 높이는
  52px이며 720p에서도 축소하지 않는다. 글자는 실제 글리프 높이로 수직 중앙
  정렬하고 제목–부제는 최소 8px, 설명 줄은 148% 간격을 확보한다.

이번 구조는 Xbox Accessibility Guidelines의 텍스트·입력·탐색·움직임
요구사항, Ghost of Yōtei의 옵션별 짧은 효과 설명, Assassin's Creed Shadows의
시각·오디오·안내·조작 분류를 비교해 이 게임의 네이티브 Canvas HUD에 맞게
적용했다. UMG로 이관할 경우에도 동일한 정보 구조를 유지하고, 정적 셸은
Invalidation Box, 변경되는 값과 포커스만 이벤트 기반 갱신 대상으로 둔다.

Shipping 시각 증거는 프런트엔드 프로브가 720p·900p·1080p·1440p에서 화면
설정과 접근성 설정을 각각 PNG로 남긴다. 레이아웃 경계, 키보드·게임패드 입력,
카테고리 전환과 실제 글리프가 동시에 통과해야 설정 화면 변경을 승인한다.
첫 프레임을 시간으로 추정하지 않고 비동기 에셋·셰이더 컴파일을 완료한 뒤
안정화 프레임을 기다린다. 엔진 준비 문구가 남은 캡처는 제품 화면으로 쓰지 않는다.

### 설정 타이포·오버플로 계약

- 제목은 고운바탕 Bold, 선택 항목은 Pretendard SemiBold, 본문·값·설명은
  Pretendard Regular를 사용한다. 바탕 계열은 제목에만 제한해 분위기를 만들고,
  조작 중 빠르게 훑는 정보는 산세리프로 유지한다.
- 설정 본문은 1080p 기준 18px을 기준으로 잡고 720p에서도 임의의 시스템 폰트로
  바뀌지 않는다. 폰트 파일과 OFL 원문을 패키지 실행 파일 옆 `UI/Fonts`에 함께
  스테이징한다.
- 제목·상태·카테고리·항목명·값·하단 입력 안내는 각 컨테이너의 남은 폭으로
  실제 글리프를 측정한 뒤 필요한 경우에만 축소한다. 설명은 두 줄을 우선하고
  내용이 남으면 세 번째 줄을 사용하며 문자열을 생략하지 않는다.
- 자막 미리보기는 200%에서도 안전 영역 폭으로 다시 줄바꿈하고 표면 높이를
  줄 수에 맞춰 늘린다. 자동 프로브는 화면 바깥뿐 아니라 행·헤더·푸터·미리보기
  컨테이너를 벗어난 글리프도 별도 실패로 처리한다.
- Shipping 증거는 접근성의 5행 자막 그룹과 입력 그룹, 화면 설정의 3행 성능
  그룹을 의도적으로 선택한다. 가장 여유 있는 첫 항목만 캡처해 통과시키지 않는다.
- 왼쪽 레일의 배경색은 본문 구간에만 칠한다. 헤더·푸터까지 관통하는 세로 면은
  텍스트 클리핑처럼 보이는 가짜 경계를 만들기 때문에 금지한다.

## 타이틀·시스템 메뉴 계약

기준일: 2026-08-12

타이틀은 웹 랜딩 페이지처럼 카드와 설명 문구를 쌓지 않는다. 비 오고 난 새벽의
무영로 빌라를 오른쪽에 두고, 조작 정보는 어두운 왼쪽 여백에만 놓는다. 고운바탕
Bold는 작품명에만, Pretendard는 메뉴와 상태에만 사용한다. 색은 젖은 먹색,
낡은 상아색, 콘크리트 회색, 산화 적갈색 한 가지 포커스로 제한한다.

- 첫 실행에는 사용할 수 없는 `이어하기`를 회색으로 남기지 않고 숨긴다. 첫
  항목은 `게임 시작`이며, 호환 자동 저장이 생긴 뒤에만 `이어하기 / 새 게임`을
  함께 표시한다.
- 선택 상태는 색상만으로 전달하지 않는다. 건물 단면을 닮은 세로 기준선,
  길어진 층 눈금, 낮은 명도의 면 채움, SemiBold 글자 네 신호를 같이 사용한다.
- 행 높이는 720p에서도 44px 아래로 줄이지 않는다. 네이티브 HUD와 마우스
  판정은 모두 `IGFrontendMenuLayout`의 동일한 좌표를 소비한다.
- 타이틀 원화에는 글자·로고·UI를 굽지 않는다. 런타임에서 현지화 가능한 제목과
  메뉴를 합성하고, 16:9 외 화면은 좌측 조작 여백을 우선해 cover-crop한다.
- 진입은 전체 그룹의 320ms 불투명도 변화 한 번만 사용한다. 이동·반복 맥동·
  글리치·깜빡임은 없으며 `동작 감소`에서는 전환을 즉시 완료한다.
- 일시정지는 별도 배경으로 교체하지 않고 멈춘 세계를 어둡게 남긴다. 플레이어가
  메뉴를 닫았을 때 위치 관계를 다시 찾을 필요가 없어야 한다.
- 설정·소리/밝기 보정은 각각의 정보 구조를 유지한다. 타이틀 장식선을 뒤에
  겹쳐 두 번째 포커스처럼 보이게 하지 않는다.

에셋 경계:

- 생성 원본: `Content/SourceArt/AI/TitleBackgroundMissingFloor_v1.png`
- 파생 PNG: `Content/SourceArt/T_TitleBackground_D.png` (1920×1080)
- 런타임: `Content/UI/Textures/T_TitleBackground_D.uasset`
- Shipping 첫 실행 캡처: `Docs/Media/title-menu-first-run-1080.png`
- 생성·해시·프롬프트: `Docs/IMAGEGEN_PROMPTS_2026-08-12.md`

비교 기준은 Xbox Accessibility Guidelines 101·102·112·113·115·117의 텍스트,
대비, 선형 탐색, 명확한 포커스, 위험 동작, 움직임 제어와 Unreal Engine 5.8의
UMG Optimization/Invalidation 지침이다. 최신 공포 타이틀 화면에서 공통적으로
보이는 환경 중심의 얕은 메뉴 계층은 참고하되, 이 작품의 한국 주거지·소리·
새벽 시간이라는 서사 근거가 없는 장식은 들여오지 않는다.

- <https://learn.microsoft.com/en-us/xbox/accessibility/guidelines>
- <https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/101>
- <https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/102>
- <https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/112>
- <https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/113>
- <https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/115>
- <https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/117>
- <https://dev.epicgames.com/documentation/unreal-engine/optimization-guidelines-for-umg-in-unreal-engine>
- <https://dev.epicgames.com/documentation/unreal-engine/invalidation-in-slate-and-umg-for-unreal-engine>
