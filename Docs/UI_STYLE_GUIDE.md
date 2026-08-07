# 4:44 AM UI 스타일 가이드

기준일: 2026-08-06
적용 범위: 인게임 대화, 내면 독백, 기기 메시지, 음성 자막, 비언어음 캡션

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
