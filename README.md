# 4시 44분

> 물이 떨어졌다. 편의점에 다녀왔다.
> 다음 날 새벽 4시 44분, 냉장고에는 또 물이 없다.

![새벽 4시 44분에 알람 소리로 시작되는 실제 게임 화면](Docs/Media/prologue-bedroom.png)

> **개발 상태:** REBIRTH 서사·정적 계약까지만 확인했으며 UE 런타임,
> 초견 플레이, 최종 콘텐츠·접근성과 Shipping 패키징은 아직 승인하지
> 않았습니다. 아래 이미지와 영상은 과거 개발 빌드의 참고 자료이며 현재
> 커밋의 릴리스 증거가 아닙니다.

**4시 44분**은 낡은 한국 빌라와 새벽 골목을 무대로 만든 싱글 플레이
1인칭 현실 공포 게임입니다.

낯선 괴물을 계속 보여 주기보다, 매일 지나던 복도와 편의점이 조금씩
틀어지는 순간에 집중했습니다. 플레이어는 세 번의 아침을 반복하며
영수증, 휴대폰, 관리 기록과 생활 흔적을 직접 대조하고, 마지막에는
이미 일어난 발견을 마주한 채 기억 속 뚜껑을 다시 닫을지, 잠시 붙들고
04:44 이후를 받아들일지 선택하게 됩니다.

| 구분 | 내용 |
|---|---|
| 장르 | 싱글 플레이 · 1인칭 · 현실/심리 공포 |
| 배경 | 한국의 오래된 빌라, 새벽 골목, 무인 편의점 |
| 플레이 범위 | CH01–CH03, 선택에 따른 두 가지 결말 |
| 엔진 | Unreal Engine 5.8 / C++ |
| 플랫폼 | Windows |

## 개발 프리뷰 실행하기

1. 저장소를 내려받고 `git lfs pull`로 LFS 에셋을 받습니다.
2. 아래 실행 환경의 UE 5.8과 C++ 빌드 도구를 설치합니다.
3. [Scripts/RunEditor.bat](Scripts/RunEditor.bat)을 실행해 C++ 모듈을
   빌드합니다.
4. 에디터에서 Play를 누르거나, 빌드 뒤
   [Scripts/RunGame.bat](Scripts/RunGame.bat)을 실행합니다.
5. 첫 화면에서 알람 시계를 바라보고 `E`를 누릅니다.

이 프로젝트는 아직 별도의 패키지 실행 파일이 아니라 UE 5.8 프로젝트로
제공됩니다. `RunGame.bat`은 엔진을 찾아 개발 게임을 시작할 뿐 C++ 빌드,
실행 성공이나 릴리스 품질을 판정하지 않습니다. 첫 에디터 실행은 C++
모듈과 셰이더를 준비하느라 창이 뜨기까지 조금 걸릴 수 있습니다.

중간 챕터를 바로 확인하고 싶다면 아래 실행 파일을 사용하면 됩니다.

| 실행 파일 | 시작 위치 |
|---|---|
| [RunGame-Chapter2.bat](Scripts/RunGame-Chapter2.bat) | 두 번째 아침 |
| [RunGame-Chapter3.bat](Scripts/RunGame-Chapter3.bat) | 세 번째 아침 |
| [RunEditor.bat](Scripts/RunEditor.bat) | Unreal Editor |

엔진 없이 정적 사전 점검만 재현하려면 다음 명령을 사용합니다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\Run-Rebirth-ReleaseValidation.ps1 -StaticOnly
```

이 모드는 정적 단계 로그와 `summary.json`을 만들고 최상위 결과를
`PARTIAL`로 기록하며, 빌드·Map Check·A/B 런타임·Shipping은
`NOT_RUN`으로 남깁니다. UE 5.8이 있는 제작 환경의 전체
하네스도 최종 초견·시청각·접근성·별도 PC 패키지 검증을 대신하지 않습니다.
승인 절차는 [G3~G6 릴리스 검증 절차](Docs/RELEASE_VALIDATION.md)를
따릅니다.

첫 플레이는 변화의 기준이 되는 CH01부터 시작하는 것을 권장합니다.

## 평범한 하루를 먼저 기억하는 게임

첫 번째 아침에는 알람을 끄고, 빈 냉장고를 확인하고, 지갑을 챙겨
편의점에서 생수를 사 옵니다. 방에서 편의점까지의 길을 직접 걷게 한
이유는 단순합니다. 정상적인 공간을 기억해야 다음 날 사라진 문 하나,
꺼진 형광등 하나도 이상하게 느껴지기 때문입니다.

<table>
  <tr>
    <td width="50%"><img src="Docs/Media/prologue-corridor.png" alt="빌라 4층 복도 실제 게임 화면"></td>
    <td width="50%"><img src="Docs/Media/prologue-elevator.png" alt="빌라 엘리베이터 실제 게임 화면"></td>
  </tr>
  <tr>
    <td align="center">매일 지나가는 4층 복도</td>
    <td align="center">층마다 소리와 움직임이 달라지는 엘리베이터</td>
  </tr>
</table>

두 번째 아침부터는 같은 동선에서 한 번에 한 가지 규칙만 바뀝니다.
어제 없던 403호, 누르지 않은 층에서 멈추는 엘리베이터, 사람이 없는데
두 번 울리는 편의점 차임처럼 설명할 수는 있지만 납득하기 어려운
상황을 차례로 배치했습니다.

![따뜻한 스탠드 너머에 누군가 누워 있는 403호 실제 게임 화면](Docs/Media/ch02-mirror-room.png)

## 한국에서 본 듯한 공간

붉은 벽돌, 외벽 배관, 공동현관, 우편함, 화강석 바닥, 좁은 계단과
옥상 철문까지 한국의 오래된 빌라에서 익숙하게 볼 수 있는 요소를
기준으로 공간을 구성했습니다.

골목에는 길고양이, 돌풍 뒤에 구르는 마른 낙엽, 승용차와 배달
오토바이가 정해진 경로를 따라 등장합니다. 반복이 진행되면 이 작은
생활 사건들의 순서와 소리, 또는 존재 자체가 달라집니다.

![붉은 벽돌 빌라와 새벽24 편의점이 이어지는 새벽 골목 실제 게임 화면](Docs/Media/prologue-alley.png)

편의점도 단순한 밝은 방으로 만들지 않았습니다. 냉장 쇼케이스의 상품과
가격표, 셀프 계산대, 입장 차임, 매장 징글과 영수증 출력음이 함께
작동합니다. 익숙한 공간이 충분히 정상적으로 보이기 때문에 점장 목소리와
생활음이 사라지는 순간이 더 크게 느껴집니다.

![새벽24 무영로점 음료 냉장고와 가격표 실제 게임 화면](Docs/Media/prologue-store.png)

## 읽고 비교하는 조사

핵심 단서는 자동으로 수집되는 로그가 아니라 게임 안에서 직접 펼쳐
읽는 물건입니다. 같은 시각이 찍힌 두 장의 영수증, 읽지 않은 가족의
문자, 비어 있는 안전 확인란을 연결하면 세 번의 아침이 반복되는 이유를
알 수 있습니다.

<table>
  <tr>
    <td width="52%"><img src="Docs/Media/ch02-receipt-0444.png" alt="한국 편의점 형식 영수증 실제 게임 화면"></td>
    <td width="48%"><img src="Docs/Media/ch02-lobby-offering.png" alt="구버전 공동현관 의례 장면 개발 캡처"></td>
  </tr>
  <tr>
    <td align="center">게임 안에서 확대해 읽는 04:44 영수증</td>
    <td align="center">구버전 캡처 — 현재 정사는 마른 밥그릇 자국·끊긴 소금선 바깥 물그릇</td>
  </tr>
</table>

영수증은 한국 편의점 감열지 구성을 기준으로 새로 만들었습니다.
가상 점포와 상품, 거래 번호, 수량·단가·부가세, 마스킹 카드, 승인 번호,
일시불 표기와 바코드까지 한 장의 결제 내역처럼 읽히도록 정렬했습니다.
상호, 인물, 사업 정보, 주소, 전화번호와 결제 정보는 모두 허구입니다.

## 세 번의 아침

| 챕터 | 스포일러 없는 소개 |
|---|---|
| CH01 · 물이 없다 | 평범한 기상과 편의점 심부름으로 공간과 생활 소리를 익힙니다. |
| CH02 · 집이 아니다 | 기억과 다른 옆집, 복도, 엘리베이터와 영수증을 조사합니다. |
| CH03 · 물이 온다 | 물에 잠긴 집을 벗어나 반복되는 계단과 옥상으로 향합니다. |

<table>
  <tr>
    <td width="33%"><img src="Docs/Media/ch03-full-fridge.png" alt="생수로 가득 찬 냉장고 실제 게임 화면"></td>
    <td width="33%"><img src="Docs/Media/ch03-stair-up.png" alt="반복되는 4층 계단 실제 게임 화면"></td>
    <td width="33%"><img src="Docs/Media/ch03-roof-tank.png" alt="옥상 물탱크 실제 게임 화면"></td>
  </tr>
  <tr>
    <td align="center">가득 찬 냉장고</td>
    <td align="center">끝나지 않는 4층</td>
    <td align="center">새벽의 옥상</td>
  </tr>
</table>

## 소리와 연출

사운드는 분위기를 채우는 배경이 아니라 진행을 읽는 단서로 사용했습니다.

- 알람, 발소리, 호흡, 심박과 물소리
- 냉장고·형광등의 전기음, 도어락, 문 마찰음과 금속 울림
- 편의점 차임, 계산음, 영수증 출력음과 매장 징글
- 고양이, 바람과 낙엽, 승용차와 배달 오토바이의 공간 이동음
- 반복이 깊어질수록 음정과 속도가 달라지는 생활음
- 중요한 순간에 배경음을 덜어 내는 의도적인 침묵

조명도 방의 주황색 스탠드, 복도의 낡은 형광등, 편의점의 차가운 백색광,
손전등과 옥상 새벽빛이 서로 다른 감정을 만들도록 나눴습니다. 유리와
거울의 과한 그림자, 좁은 문틀과 자동문 문턱 충돌, 가벼운 소품이
과하게 튀는 물리 반응도 실제 플레이 동선을 기준으로 조정했습니다.

## 조작법

| 입력 | 동작 |
|---|---|
| `W A S D` | 이동 |
| 마우스 | 시점 |
| `E` | 조사, 집기, 문 열기, 문서 읽기, 계산 |
| `E` 길게 | 진행 바가 있는 상호작용 |
| `F` | 손전등 켜기·끄기 |
| `Esc` | 마우스 커서 전환 |
| `R` | 엔딩에서 CH03 다시 시작 |
| `M` | 엔딩에서 처음부터 시작 |
| `Alt + F4` | 종료 |

## 실행 환경

- Windows 10 또는 Windows 11
- Unreal Engine 5.8
- Visual Studio 2026의 **Game development with C++** 워크로드
- Windows SDK

출시 인증 OS 버전과 최소·권장 하드웨어는
[Windows 출시 성능·지원 계약](Docs/PERFORMANCE.md)을 기준으로 합니다.

실행 스크립트는 프로젝트의 `EngineAssociation`과 일치하는 Unreal
Editor를 표준 설치 경로, 레지스트리와 Epic Launcher 설치 목록에서
찾습니다. 소스 빌드처럼 별도 위치를 쓴다면 환경 변수
`IG_UNREAL_EDITOR`에 실제 `UnrealEditor.exe` 경로를 지정하면 됩니다.

에디터에서 직접 열려면 [IndieGame.uproject](IndieGame.uproject)을
실행하고 UE 5.8을 선택합니다. 모듈을 다시 빌드할지 묻는 창이 나오면
**Yes**를 선택한 뒤 상단 **Play** 버튼을 누르면 됩니다.

## 더 보기

- [CH01 실제 플레이 영상](Docs/Media/prologue-walkthrough.mp4)
- [REBIRTH 스토리 마스터](Docs/STORY_BIBLE_REBIRTH.md) — 단일 제작 정사와 서사·공포·음악·이미지·대화 통합 기준
- [출시 타당성 제작 계약](Docs/FEASIBILITY.md) — Keep/Static Proxy/Cut 범위와 S1~S8 승인 기준
- [G3~G6 릴리스 검증 절차](Docs/RELEASE_VALIDATION.md) — 실행 순서, 증거 보관, 초견·접근성·Shipping 체크리스트
- [Windows 출시 성능·지원 계약](Docs/PERFORMANCE.md) — 최소·권장 사양, 해상도와 성능 합격선
- [세이브 호환성 출시 계약](Docs/SAVE_COMPATIBILITY.md) — v1~v3 픽스처, 마이그레이션 오라클과 클린 환경 검증
- [현재 구현 상태](Docs/IMPLEMENTATION_STATUS.md) — REBIRTH 반영 범위와 남은 출시 차단 항목
- [구버전 스토리 바이블 보관본](Docs/STORY_BIBLE.md) — REBIRTH 이전 기록, 제작 기준 아님
- [스토리와 공포 연출 설계](Docs/STORY_DIRECTION.md) — 결말 포함
- [사용 에셋과 라이선스](Docs/ASSET_POLICY.md)
