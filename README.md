# 없는 층

실종된 오빠를 찾아 낡은 빌라를 돌아다니는 **1인칭 공포 게임**입니다.
오빠가 쓰던 주소는 501호인데, 찾아간 빌라에는 5층이 없습니다.

Windows PC · 한국어 · 싱글플레이 · 개발 중

[테스트 버전 받기](https://github.com/easygap/Indie-Game/releases/tag/v1.0.0-test.20260922) · [조작법](#조작) · [문제 제보](https://github.com/easygap/Indie-Game/issues/new?template=bug_report.yml)

![밤의 복도에서 적에게 쫓기다 붙잡히는 게임 플레이](Docs/Media/readme/night-listener-chase.gif)

## 다운로드

현재 받을 수 있는 파일은 **2026년 9월 22일 테스트 버전**입니다.

1. [Windows용 ZIP 파일(727MB)](https://github.com/easygap/Indie-Game/releases/download/v1.0.0-test.20260922/MissingFloor-Windows-20260922.zip)을 받으세요.
2. 압축을 모두 푼 뒤 `IndieGame.exe`를 실행합니다.

언리얼 엔진을 따로 설치할 필요는 없습니다. 진행 상황은 자동으로 저장됩니다.
실행이 안 되거나 PC 사양이 궁금하면 [실행 안내](Docs/PLAYING.md)를 확인해 주세요.

## 줄거리

주인공 유담은 달빛빌라 403호로 이사해 이웃들에게 오빠 소식을 묻습니다.
그날 새벽 네 시 반, 천장에서 누군가 두드리는 소리가 들립니다.

<table>
  <tr>
    <td width="50%"><img src="Docs/Media/readme/game-corridor-day.webp" alt="낮의 달빛빌라 4층 복도"></td>
    <td width="50%"><img src="Docs/Media/readme/game-corridor-night.webp" alt="새벽 네 시 반의 달빛빌라 4층 복도"></td>
  </tr>
  <tr>
    <td>낮의 복도</td>
    <td>새벽 4시 30분</td>
  </tr>
</table>

## 플레이

- **동네에서 단서 찾기** — 집과 골목, 편의점을 둘러보며 사람들과 이야기하고 물건을 살펴봅니다.
- **소리를 줄이며 움직이기** — 뛰거나 문을 급하게 열면 적에게 들킬 수 있습니다. 문을 천천히 열거나 잠깐 숨을 참으면 소리를 줄일 수 있습니다.
- **듣고 살펴보며 퍼즐 풀기** — 벽에 귀를 대고 소리를 듣거나, 관리실의 서류와 CCTV를 확인합니다. 계량기함처럼 평소에는 지나칠 곳에도 단서가 있습니다.

찾은 단서는 낮에 `Tab`으로 다시 볼 수 있습니다. 진행이 막히면 `H`로 힌트를 확인하세요.

![이삿짐을 아직 다 풀지 못한 403호 방](Docs/Media/readme/game-bedroom.webp)

<table>
  <tr>
    <td width="50%"><img src="Docs/Media/readme/game-alley.webp" alt="빌라 앞 골목과 편의점"></td>
    <td width="50%"><img src="Docs/Media/readme/game-store.webp" alt="편의점 계산대에서 만난 직원"></td>
  </tr>
  <tr>
    <td>빌라 앞 골목</td>
    <td>동네 편의점</td>
  </tr>
</table>

<table>
  <tr>
    <td width="50%"><img src="Docs/Media/readme/p1-meter-cabinet.webp" alt="호실 표시와 검침표가 붙은 계량기함"></td>
    <td width="50%"><img src="Docs/Media/readme/game-booth.webp" alt="서류와 CCTV가 놓인 관리실 책상"></td>
  </tr>
  <tr>
    <td>계량기함</td>
    <td>관리실</td>
  </tr>
</table>

[집과 동네를 둘러보는 장면 더 보기 · GIF 4.9MB](Docs/Media/readme/readme-route-preview.gif)

## 조작

`WASD`와 마우스로 움직이고, `E`로 물건을 살펴보거나 문을 엽니다.
문 앞에서 `E`를 길게 누르면 조용히 열 수 있습니다.

게임 안에서는 `F1`로 조작법을 다시 볼 수 있습니다. 게임패드는 십자키 위쪽입니다.
키보드 키와 게임패드 버튼은 설정에서 바꿀 수 있습니다.

<details>
<summary>전체 조작법 펼치기</summary>

| 키 | 동작 |
|---|---|
| `W A S D` · 마우스 | 이동 · 둘러보기 |
| `왼쪽 Shift` · `C` · `Space` | 달리기 · 앉기 · 점프 |
| `E` | 물건 살펴보기 · 문 열기 |
| `E` 길게 | 문을 조용히 열기 · 벽에 귀 대기 |
| `Q` · `왼쪽 Ctrl` | 두드리기 · 숨 참기 |
| `F` | 손전등 |
| `F1` | 목표와 조작법 다시 보기 |
| `Tab` · `H` | 기록 보기(낮) · 힌트 |
| `← →` · 마우스 휠 | 문서 페이지 넘기기 |
| `Esc` · `F10` | 일시정지 · 접근성 설정 |

</details>

## 난이도와 설정

쫓기는 게 부담스럽다면 난이도를 **추격 없음**으로 바꿔 보세요.
적에게 쫓기거나 붙잡히지 않고 퍼즐과 이야기를 끝까지 진행할 수 있습니다.
쉬움·보통·어려움도 있으며, 플레이 도중 `F10`에서 바꿀 수 있습니다.

설정에서 다음 항목도 조절할 수 있습니다.

- 자막 크기, 표시 시간, 배경 진하기
- 소리가 나는 방향을 화면으로 표시하고 노크를 진동으로 알리기
- 화면 흔들림과 빛 깜빡임 줄이기
- 버튼을 길게 누르는 대신 한 번씩 눌러 조작하기
- 배경 음악과 환경음 크기, 헤드폰·스피커 선택

어두운 장면과 갑자기 튀어나오는 적, 큰 소리가 나옵니다.
처음 시작할 때 소리 크기와 밝기를 편하게 맞춰 주세요.

<details>
<summary>자막 설정 화면 보기</summary>

![자막 크기와 배경 진하기를 바꾸는 게임 설정 화면](Docs/Media/readme/settings-accessibility-20260922.webp)

</details>

## 플레이해 보셨다면

어디가 무서웠는지, 어디서 길을 잃었는지 [플레이 소감](https://github.com/easygap/Indie-Game/issues/new?template=feedback.yml)을 남겨 주세요.
오류는 [문제 제보](https://github.com/easygap/Indie-Game/issues/new?template=bug_report.yml)에 적어 주시면 확인하겠습니다.

다음에 다시 찾아보고 싶다면 페이지 위쪽의 **Star**를 눌러 두세요.
새 버전 알림은 **Watch → Custom → Releases**에서 받을 수 있습니다.

언리얼 엔진 5.8과 C++로 만들고 있습니다. [소스에서 직접 빌드하기](Docs/PLAYING.md#소스에서-빌드하기)
