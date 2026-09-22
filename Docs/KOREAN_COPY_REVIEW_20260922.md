# 한국어 문구와 배포 점검

확인 날짜: 2026년 9월 22일

## 참고한 자료

- [소미의 《미제사건은 끝내야 하니까》 게임 소개](https://store.steampowered.com/app/2676840/_/?l=koreana): 주인공이 처한 상황과 플레이어가 할 일을 먼저 설명하는 구성을 참고했다. 소개에 쓰인 대사처럼 인물의 말은 실제로 소리 내어 읽어도 자연스러워야 한다.
- [소미의 공식 소식, 2026년 8월 18일](https://steamcommunity.com/app/2676840?l=koreana): 개발자가 직접 쓴 최근 한국어 안내를 확인했다. 게임 속 대사와 메뉴 안내에 같은 말투를 억지로 적용하지 않는다.
- [《골목길: 귀흔》 개발자 인터뷰, 2026년 1월 6일](https://newsroom.smilegate.com/bbs/board.php?bo_table=indiegame&wr_id=45): 익숙한 한국 동네를 배경으로 삼은 공포 게임의 소개 방식과 제작 의도를 확인했다.
- [《골목길: 귀흔》 공식 상점](https://store.onstove.com/ko/games/101317): 실제 플레이 장면 옆에 조작과 추격을 설명하는 구성을 참고했다. 자동 번역 안내가 있는 페이지이므로 개별 단어와 말투의 기준으로 쓰지는 않았다.
- [Epic Games의 Unreal Engine 5.8 패키징 문서](https://dev.epicgames.com/documentation/ko-kr/unreal-engine/packaging-your-project): 코드 빌드뿐 아니라 게임 데이터까지 묶은 실행 파일을 만들어 확인한다.

README에는 이 프로젝트에서 직접 찍은 화면을 사용한다. 이야기의 설정과 인물 관계는 유지하면서 메뉴와 조사 문구를 다듬었다.

## 문구를 고친 기준

메뉴는 선택했을 때 무엇이 달라지는지 알 수 있게 쓴다. 주인공의 메모는 보고 들은 일을 자기 말로 적는다. 공사 서류와 영수증에는 실제 서류에서 쓰는 용어를 남긴다.

| 이전 문구 | 바꾼 문구 |
|---|---|
| 주변부 비네트 | 화면 가장자리 어둡게 |
| 노크 시각 대체 | 노크를 화면으로 표시 |
| 홀드 길이 | 길게 누르는 시간 |
| 키 다시 묶기 | 키 설정 |
| 자동 저장 복원 | 자동 저장 불러오기 |
| 배관 청음 | 배관에서 들은 소리 |
| 수위계의 지시자는 위쪽에 있었다 | 물 높이를 가리키는 바늘이 위쪽에 있었다 |
| 봉인된 새벽에는 거절된다 | 밤이 진행되는 동안에는 불러올 수 없습니다 |
| 길게 운다. 속이 비었다. | 오래 울린다. 안이 비어 있다. |
| 공동 벽 — 망치질 | 벽을 망치로 두드리기 |
| Gamepad Face Button Bottom | A |

## 함께 고친 문제

- 설정에 예전 주인공 이름이 남아 있어 현재 이야기와 맞지 않았다. 이름 없이 자막 기능을 설명하도록 고쳤다.
- 현재 게임에 쓰이지 않는 예전 퍼즐의 ‘P5 단서 자동 연결’ 항목을 메뉴에서 뺐다.
- 난이도가 구현돼 있었지만 메뉴에서 고를 수 없었다. 접근성 설정에 쉬움·보통·어려움·추격 없음을 연결하고, 저장과 진행 중인 적의 반응에도 바로 적용했다.
- 화면 설정의 메뉴 범위가 한 줄씩 밀려 조작 설정과 돌아가기의 표시가 어긋났다. 모든 설정 행의 표시 위치와 클릭 위치를 검사하도록 보완했다.
- 모든 소리를 조절하는 항목이 ‘노크 음량’으로 표시돼 있었다. ‘전체 소리’로 바꾸고 배경 음악·환경음과의 관계를 설명했다.
- 처음 소리를 맞출 때 ‘겨우 들릴 정도’로 낮추라는 안내를 고쳤다. 노크를 알아들을 수 있으면서 부담스럽지 않은 크기로 맞추도록 안내한다.
- 화면 전체의 깜빡임을 측정하지 않은 상태에서 안전 기준을 충족했다고 단정하던 문구를 뺐다. 화면 흔들림과 깜빡임을 줄이는 설정은 그대로 제공한다.
- 조작 설정에서도 게임패드 버튼을 A·B·LT처럼 표시한다. 키보드의 벽 듣기는 ‘없음’ 대신 현재 조사 키를 길게 누르도록 안내한다. 720p 화면에서 조작 목록의 글자도 키웠다.
- ‘어려움’으로 바꿨다가 되돌려도 적의 반응 단계가 남지 않도록 했다. 같은 동작을 실제 적에게 적용하는 검사도 추가했다.

![자막 설정](Media/readme/settings-accessibility-20260922.webp)

<table>
  <tr>
    <td width="50%"><img src="Media/readme/settings-audio-20260922.webp" alt="전체 소리와 배경 음악을 따로 맞추는 화면"></td>
    <td width="50%"><img src="Media/readme/settings-controls-20260922.webp" alt="키보드와 게임패드 조작 설정"></td>
  </tr>
</table>

## 확인 결과

Windows 11 / Ryzen 9 7900X / RTX 3060 / 메모리 32GB에서 확인했다.

- Unreal Engine 5.8의 Editor·Shipping 빌드와 Windows 패키징을 완료했다. 이후 검사는 배포 폴더의 `IndieGame.exe`로 실행했다.
- 1280×720, 1600×900, 1920×1080, 2560×1440에서 메뉴 입력 44회와 화면 배치 44회를 검사했다. 기본 자막과 200% 자막, 플레이 안내, 소리와 밝기, 난이도, 키 설정을 포함해 실제 화면 36장을 저장했다. 720p와 1080p의 설정 화면에서 문구와 줄 배치도 확인했다.
- 입주 첫날의 조사를 진행하고 자동 저장이 끝난 뒤 종료했다. 새 프로세스에서 해당 저장을 불러와 조사 기록, 취침 가능 상태, 낮의 적 비활성 상태를 확인했다.
- 렌더링을 끈 자동 진행 검사로 퍼즐과 밤 진행, 붙잡힌 뒤 복귀, 결말 분기 검사를 끝까지 실행했다. 사람이 직접 완주한 결과와는 구분한다.
- 실제 소리 장치에서 45개 검사를 통과했다. 8.256초 믹서 녹음은 최대 −3.426 dBFS, RMS −23.128 dBFS였고 잘린 샘플은 없었다. 이 값은 전체 플레이의 LUFS 측정값이 아니다.
- 화면 밖에서 실행하는 검사가 비활성 창 음소거 때문에 처음에는 무음으로 녹음됐다. 검사 모드에서만 비활성 음량을 열고 다시 확인했다.
- `Scripts/Validate-Project.ps1` 전체 검사를 통과했다.
- README는 이미지 9개, 합계 약 2.6MiB다. GitHub Markdown으로 렌더링한 본문을 390px와 1200px 너비에서 확인했고, 이미지 누락과 모바일 가로 넘침은 없었다.
- ZIP에 든 게임 파일 89개를 원본과 SHA-256으로 대조했다. 실행 검사 전후 배포 폴더의 파일도 바뀌지 않았다. 빌드에 사용한 코드·설정 등 186개 파일이 작업 폴더와 일치한다.

수치와 파일 해시는 [Windows 검사 결과](Validation/Windows-20260922.json)에 보관한다.
직접 실행하려면 [Windows 테스트 빌드](https://github.com/easygap/Indie-Game/releases/tag/v1.0.0-test.20260922)를 받으면 된다.

처음 하는 사람의 체감 난이도와 소리 방향 구분, 다른 PC·입력 장치에서의 성능,
전체 플레이의 음량 측정은 남아 있다. [출시 전 확인 항목](MISSING_FLOOR_ACCEPTANCE.md)에
그대로 남겼으며 이번 배포는 테스트 빌드로 표시한다.

## README와 GitHub 소개

2026년 9월 22일에 아래 자료를 추가로 확인했다.

- [《골목길: 귀흔》의 한국어 게임 소개](https://store.steampowered.com/app/4181410/The_Alley/?l=koreana): 2026년 7월 23일 출시작이다. 장르와 플레이어가 하는 일을 먼저 설명하고, 실제 화면을 함께 보여주는 구성을 참고했다.
- [GitHub의 README 안내](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-readmes): 처음 보는 사람이 프로젝트의 용도와 시작 방법을 찾을 수 있도록 구성했다.
- [GitHub Topics 안내](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/classifying-your-repository-with-topics): 게임 장르와 플랫폼, 사용한 엔진에 맞춰 태그를 달았다. 비공개 저장소는 접근 권한이 있는 사람에게만 검색 결과로 보인다.
- [Star 안내](https://docs.github.com/en/get-started/exploring-projects-on-github/saving-repositories-with-stars)와 [알림 설정 안내](https://docs.github.com/en/subscriptions-and-notifications/get-started/configuring-notifications): 다시 찾아보기는 Star, 새 버전 알림은 Watch로 구분해 안내했다.

분위기를 설명하는 소제목을 줄이고 게임 소개, 다운로드, 줄거리, 플레이, 조작 순서로 정리했다.
‘빌드’는 플레이어가 읽는 안내에서 ‘테스트 버전’으로 바꿨다.
첫 화면에는 실제 추격 장면과 다운로드 링크를 두고, 긴 조작표와 설정 화면은 접어서 볼 수 있게 했다.
문제 제보와 플레이 소감도 한국어 양식으로 바로 연결한다.

GitHub 소개에는 게임 이름과 장르를 적고 다운로드 페이지를 연결했다.
태그는 `horror-game`, `indie-game`, `unreal-engine`, `first-person`, `singleplayer`, `cpp`, `korean`, `windows`를 사용했다.
