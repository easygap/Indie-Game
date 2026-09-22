# 없는 층 실행 안내

## Windows에서 플레이하기

1. [Windows 테스트 빌드](https://github.com/easygap/Indie-Game/releases/tag/v1.0.0-test.20260922)에서 `MissingFloor-Windows-20260922.zip`을 받습니다.
2. ZIP 파일의 **압축을 모두 풉니다.** 실행 파일 옆의 `Engine`, `IndieGame` 폴더도 함께 있어야 합니다.
3. `IndieGame.exe`를 실행하고 **게임 시작**을 선택합니다.
4. 소리 크기와 밝기를 맞춘 뒤 시작합니다. 조작법은 `F1`, 난이도와 접근성 설정은 `F10`으로 다시 열 수 있습니다.

Unreal Engine이나 Visual Studio를 설치할 필요는 없습니다.
저장된 진행이 있으면 타이틀 화면의 **이어하기**로 계속할 수 있습니다.

![없는 층 타이틀 화면](Media/readme/title-menu-first-run-1080.webp)

Windows 11, Ryzen 9 7900X, RTX 3060, 메모리 32GB 환경에서 확인한 테스트 빌드입니다.
다른 PC의 성능을 확인하기 전이므로 최소 사양을 확정하지 않았습니다.
저장소가 비공개인 동안에는 초대받은 계정으로 로그인해야 다운로드할 수 있습니다.

## 실행이 안 될 때

- **DLL이 없다고 나올 때:** 압축을 모두 풀었는지 먼저 확인해 주세요. 같은 폴더의 `Engine/Extras/Redist/en-us/vc_redist.x64.exe`로 필요한 실행 구성 요소를 설치할 수 있습니다.
- **화면이 끊길 때:** 설정에서 그래픽 품질과 해상도를 낮춰 보세요.
- **소리가 작을 때:** 게임의 전체 소리와 Windows 볼륨 믹서를 확인해 주세요. 배경 음악과 환경음도 따로 조절할 수 있습니다.

문제가 계속되면 [이슈](https://github.com/easygap/Indie-Game/issues)에
오류 메시지, Windows 버전, 그래픽카드와 문제가 생긴 장면을 남겨 주세요.

## 소스에서 빌드하기

직접 수정하거나 빌드하려면 Unreal Engine 5.8, Visual Studio의 **C++를 사용한 게임 개발** 도구와 Windows SDK, PowerShell 7, Git LFS가 필요합니다.

```powershell
git lfs install
git clone https://github.com/easygap/Indie-Game.git
cd Indie-Game
git lfs pull
pwsh -NoProfile -File .\Scripts\Build-ArtAssets.ps1 -CodeOnly
.\Scripts\RunGame.bat
```

게임 데이터까지 묶은 Windows 배포 파일은 다음 명령으로 만듭니다.

```powershell
pwsh -NoProfile -File .\Scripts\Package-Windows.ps1
```

완료되면 배포 폴더 경로가 출력됩니다. `Windows/IndieGame.exe`가 실행 파일입니다.
