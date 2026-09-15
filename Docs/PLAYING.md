# 소스에서 실행하기

현재는 소스를 받아 빌드한 뒤 실행할 수 있습니다.
Windows 10/11 64비트 환경에서 진행해 주세요.

## 준비할 것

- Unreal Engine 5.8
- Visual Studio의 **C++를 사용한 게임 개발** 워크로드와 Windows SDK
- PowerShell 7
- Git과 Git LFS

## 받기

PowerShell에서 저장할 폴더로 이동한 뒤 실행합니다.

```powershell
git lfs install
git clone https://github.com/easygap/Indie-Game.git
cd Indie-Game
git lfs pull
```

에셋은 Git LFS로 받습니다. 마지막 명령이 끝날 때까지 기다려 주세요.

## 빌드하고 시작하기

프로젝트 폴더에서 다음 명령으로 게임 코드를 빌드합니다.

```powershell
pwsh -NoProfile -File .\Scripts\Build-ArtAssets.ps1 -CodeOnly
```

빌드가 성공하면 실행합니다.

```powershell
.\Scripts\RunGame.bat
```

타이틀 화면에서 **게임 시작**을 선택하면 입주 첫날부터 시작합니다.
저장된 진행이 있다면 **이어하기**를 선택할 수 있습니다.
처음 실행할 때는 셰이더 준비에 시간이 걸릴 수 있습니다.

![없는 층 타이틀 화면. 게임 시작, 설정, 제작 정보, 게임 종료 메뉴](Media/readme/title-menu-first-run-1080.webp)

소리와 밝기를 맞춘 뒤 플레이해 주세요. 조작법과 접근성 설정은
[게임 소개](../README.md#조작)에서 확인할 수 있습니다.

## 실행이 안 될 때

- **엔진을 찾을 수 없다고 나올 때:** Unreal Engine 5.8이 설치되어 있는지 확인해 주세요.
- **빌드가 실패할 때:** Visual Studio의 C++ 게임 개발 도구와 Windows SDK 설치 여부를 확인해 주세요.
  해결되지 않으면 빌드 창의 오류 메시지를 함께 남겨 주세요.
- **화면이나 에셋이 빠져 있을 때:** 프로젝트 폴더에서 `git lfs pull`을 다시 실행해 주세요.

문제가 계속되면 [이슈](https://github.com/easygap/Indie-Game/issues)에
오류 메시지와 Windows 버전을 적어 주세요.
