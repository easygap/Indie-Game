# 포획 장면과 안내 표시 점검 — 2026-09-17

## 손이 누구의 것인지 알기 어려웠던 이유

포획 때 쓰던 원본은 화면 아래 양쪽에서 팔이 앞으로 뻗는 구도였다.
추격자의 몸과는 따로 그렸고, 침대에 돌아와서도 같은 그림을 거꾸로 재생했다.
괴물이 잡는 것인지 플레이어가 손을 내미는 것인지 불분명했다.

[수정 전 침대 복귀에 남아 있던 손 그림](Media/ux-before-wake.gif)

포획·기상 HUD에서 네 장의 손 이미지를 읽고 그리는 코드를 제거했다.
이제 복도에서 쫓아오던 실제 몸이 다가오고, 암전 뒤 침대의 시야가 돌아온다.
기상 페이드가 끝나기 전에는 움직일 수 없고, 목표나 조작 안내도 나타나지 않는다.
재도전이 반복되면 기상 시간은 기존대로 짧아진다.

![수정 후 실제 포획과 침대 복귀](Media/readme/night-listener-chase.gif)

[1920×1080 촬영본](Media/ux-capture-recovery-1920x1080.gif) ·
[1280×800 촬영본](Media/ux-capture-recovery-1280x800.gif)

연속 PNG를 찍은 게임 시각으로 GIF의 간격을 맞췄다. 촬영 중 저장 작업 때문에
1920×1080은 33장, 1280×800은 64장이 남았다. 게임의 프레임률을 측정한 영상은 아니다.

이번에는 `imagegen` 지침과 기존 생성 원본을 확인한 뒤, 잘못된 시점의 그림을
게임에서 빼는 쪽으로 고쳤다. 새 이미지는 생성하지 않았다. 그림의 화질을
높여도 몸과 떨어져 움직이는 문제가 해결되지는 않는다.

## 안내가 필요한 순간

| 상황 | 표시 |
|---|---|
| 새 목표를 받음 | 7초 후 사라짐 |
| 입주 초반의 기본 조작 | 20초 후, 또는 첫 조사가 끝나면 숨김 |
| F1 / 패드 십자키 위쪽 | 목표·조작법을 10초간 다시 표시 |
| 다시 보기 키를 한 번 더 누름 | 바로 닫힘 |
| 문서·기록·메뉴·포획 | 수동 안내를 닫고 해당 화면에 집중 |

밤에는 기존의 시계와 조작 안내를 같은 키로 확인한다.

표시가 끝나기 전 0.45초 동안 서서히 사라진다. 다시 보기 키는 설정에서
바꿀 수 있으며 화면의 키 이름도 함께 바뀐다. 문서의 닫기 안내도 실제
상호작용 키를 따르게 하고 종이 위에서 읽히도록 글자를 진하게 했다.

![안내가 자동으로 사라진 실제 게임 화면](Media/readme/ux-quiet-1920x1080.webp)

![F1으로 다시 연 목표와 조작법](Media/readme/ux-guide-keyboard-1920x1080.webp)

[패드 안내](Media/ux-guide-gamepad-1920x1080.png) ·
[문서를 읽는 화면](Media/ux-reading-1280x800.png) ·
[첫 조작 안내](Media/ux-tutorial-1920x1080.png)

## 참고한 자료와 적용 범위

- [Silent Hill: Townfall 개발진 설명 — 2026-09-16](https://blog.playstation.com/2026/09/16/silent-hill-townfall-creators-break-down-ps5-features-out-september-25/):
  제한된 1인칭 시야와 공간 소리, 플레이어 행동에 반응하는 긴장감을 설명한다.
  이 게임에서는 접촉하는 몸과 침대 복귀가 한 사건으로 읽히도록 화면 구성을 정리했다.
- [The Last of Us Part II 접근성 안내](https://www.playstation.com/en-us/games/the-last-of-us-part-ii/accessibility/):
  힌트 표시 빈도와 버튼으로 요청하는 방식을 확인했다. 이번 기본 조작·목표 안내의
  자동 숨김과 다시 보기 설계에 참고했다. 2026년 신기능으로 소개하는 자료는 아니다.
- [UE 애니메이션 최적화](https://dev.epicgames.com/documentation/unreal-engine/animation-optimization-in-unreal-engine):
  화면에 보이는 중요한 동작과 먼 곳의 갱신 비용을 구분하는 기준을 확인했다.
  이번 수정에서는 근접 포획 애니메이션의 갱신 빈도를 낮추지 않았다.

7·20·10초는 이 게임에서 정한 값이다. 참고 작품의 수치라고 주장하지 않는다.
최적화는 쓰지 않는 손 텍스처 네 장의 HUD 로드와 합성 경로를 없앤 범위다.
이번 확인만으로 전체 게임의 프레임률이 개선됐다고 결론 내리지 않는다.

## 재현

`Scripts/Run-ImmersionReview.ps1`은 실제 게임을 실행하고 플레이어 컨트롤러에
키 입력을 넣는다. 처음 안내, 시간 만료, 다시 보기, 닫기, 키 변경, 패드 입력,
읽기 화면, 일시정지를 확인한다. 설정 파일은 실행 뒤 원래대로 복구한다.

| 확인 | 결과 |
|---|---|
| UE 5.8 Development 빌드 | 통과 |
| 1920×1080 / 1280×800 안내 검사 | 각 15개 항목 통과, 캡처 5장씩 확인 |
| 두 해상도 포획·복귀 | 실제 3D 접촉, 텍스처 준비, 입력 복귀 통과 |
| 조작·충돌·포획 런타임 검사 | `REALISM_PROBE PASS failures=0` |
| 밤 전체 게임플레이 검사 | `MISSINGFLOOR_GREYBOX PASS` |
| 프로젝트 검사 | 통과 |

```powershell
pwsh -NoProfile -File Scripts/Run-ImmersionReview.ps1
pwsh -NoProfile -File Scripts/Run-ImmersionReview.ps1 -Width 1280 -Height 800
pwsh -NoProfile -File Scripts/Run-CaptureRecoveryReview.ps1
pwsh -NoProfile -File Scripts/Run-CaptureRecoveryReview.ps1 -Width 1280 -Height 800
python Scripts/assemble_capture_recovery.py
python Scripts/assemble_capture_recovery.py --resolution 1280x800
pwsh -NoProfile -File Scripts/Run-GameplayRealismProbe.ps1
pwsh -NoProfile -File Scripts/Validate-Project.ps1
```

1920×1080·1280×800 검수는 Windows D3D12에서 한다. 실제 패드의 진동 강도,
헤드폰 청음, 처음 플레이하는 사람의 공포 체감은 이 자동 검사에 포함되지 않는다.
