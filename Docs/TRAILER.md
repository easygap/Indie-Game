# 예고편 다시 만들기

현재 예고편은 36.7초, 1920×1080, 30fps다. 첫 화면부터 게임 장면을 보여 주고, 상황을 설명하는 자막은 세 줄만 사용한다. 별도의 색 보정과 입자는 얹지 않는다.

## 촬영과 편집

Unreal Engine 5.8, Python, ffmpeg가 필요하다. Python에서는 Pillow, numpy, scipy를 사용한다.

```powershell
python -m pip install Pillow numpy scipy
pwsh -NoProfile -File Scripts/Build-ArtAssets.ps1 -CodeOnly
pwsh -NoProfile -File Scripts/Trailer/Run-TrailerCapture.ps1
python Scripts/Trailer/cards.py Docs/Media/social-preview.png
python Scripts/Trailer/assemble.py --marks
python Scripts/Trailer/compose.py
python Scripts/Trailer/assemble.py MissingFloor-Trailer.mp4
```

결과는 `Saved/Trailer/MissingFloor-Trailer.mp4`다. 프레임은 같은 폴더의 장면별 하위 폴더에 남는다. 촬영기는 실제 게임 공간과 귀신의 애니메이션을 30fps 고정 간격으로 기록한다. 카메라 경로와 장면 상태는 촬영을 위해 지정하므로, 플레이어가 처음부터 끝까지 조작한 실황 영상은 아니다.

## 소리

게임의 WAV 효과음을 장면의 동작에 맞춰 편집한다. 별도 음악과 나레이션은 없다. 환경음은 공간을 이어 주고, 노크와 다가오는 발소리 사이에는 여백을 둔다. 촬영은 무음으로 진행하므로 실제 게임 믹서를 그대로 녹음한 소리는 아니다.

`compose.py`가 ffmpeg의 loudnorm을 두 번 실행해 -18 LUFS, 최대 -2 dBTP를 목표로 조절한다. 첫 측정값은 `Saved/Trailer/audio-loudness.json`에 남는다. 배포 전에는 인코딩된 MP4도 ebur128로 확인한다.

## 배포 전 확인

- 첫 장면과 자막이 실제 게임 내용과 맞는지 확인한다. 붙잡혀도 게임 속 시각은 되돌아가지 않는다.
- 프레임이 빠지면 편집을 중단한다. 마지막 프레임을 늘여 길이를 맞추지 않는다.
- 작은 화면에서도 귀신의 실루엣과 조작 대상이 구분되는지 본다.
- 릴리스에 MP4를 첨부하고 README의 예고편 링크를 같은 버전으로 맞춘다.

참고한 제작자의 글과 적용 이유는 [9월 29일 점검 기록](REVIEW_20260929.md)에 있다.