# 예고편 다시 만들기

지금 예고편은 57초, 1920×1080, 30fps다. 한국어판과 영어판은 같은 촬영분에 글자만 다르다.

## 구성

두 참고작(AFTERLIGHT: The Apartment, The Strange Lights)과 예고편 편집자들의 조언을 따랐다.
소리로 열고, 걷는 사람의 시선으로 길게 보여 주고, 붙잡히는 순간 끊는다.

| 시간 | 장면 | 소리 |
|---|---|---|
| 0~2.6초 | 암전 | 위층에서 먹먹하게 세 번 두드리는 소리 |
| 2.6~8.4초 | 새벽 네 시 반, 403호에서 천장을 올려다본다 | 방 소리, 천장이 한 번 삐걱인다 |
| 9.2~20.4초 | 전날 저녁의 4층 복도, 402호 문에 붙은 전단 | 복도 소리와 발소리 |
| 20.4~31.9초 | 골목, 1층 계량기함, 관리실 CCTV | 도시 소리, 안정기 소리 |
| 32.7~47.5초 | 밤 복도, 계단의 귀신, 붙잡히는 순간 | 숨소리, 기는 소리, 붙잡히는 소리 |
| 49.5~57초 | 제목 | 위에서 둘, 쉬고, 하나 |

화면에 나오는 글은 게임에서 실제로 뜨는 자막 두 줄(「천장에서 세 번 두드리는 소리」, 「…4층이 꼭대기인데.」)과
제목뿐이다. 자막 상자는 게임 화면과 같은 모양으로 그린다. 색 보정과 필름 효과는 따로 얹지 않는다.
화면 질감은 게임 자체의 것이다. 이야기 후반의 장소와 결말은 보여 주지 않는다.

## 촬영과 편집

Unreal Engine 5.8, Python(Pillow, numpy, scipy), ffmpeg가 필요하다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Build-ArtAssets.ps1 -CodeOnly
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Trailer/Run-TrailerCapture.ps1
python Scripts/Trailer/cards.py
python Scripts/Trailer/assemble.py --marks
python Scripts/Trailer/compose.py
python Scripts/Trailer/assemble.py ko
python Scripts/Trailer/assemble.py en
```

촬영기(`-IGTrailerCapture`)는 장면마다 카메라를 옮기며 UI 없이 프레임을 한 장씩 남긴다.
장면표는 `AIGListenerGreyboxDirector::AdvanceTrailerCapture`에 있고, `-Shot door-402,capture-front`처럼
몇 장면만 다시 찍을 수 있다. 귀신은 소리를 들어야 움직이므로 쫓기는 두 장면은 녹화를 시작하는 순간
플레이어 자리에서 소리를 낸다.

결과는 `Saved/Trailer/MissingFloor-Trailer.mp4`(한국어)와 `MissingFloor-Trailer-en.mp4`(영어)다.
화면 잡티 때문에 원본이 크므로 배포용과 README용을 따로 인코딩한다.

```powershell
ffmpeg -i MissingFloor-Trailer.mp4 -c:v libx264 -preset slow -crf 22 -tune grain -c:a aac -b:a 160k -movflags +faststart MissingFloor-Trailer-hq.mp4
ffmpeg -i MissingFloor-Trailer.mp4 -vf "scale=1280:720:flags=lanczos,hqdn3d=2:1.5:3:3" -c:v libx264 -preset slow -b:v 1150k -pass 1 -an -f mp4 NUL
ffmpeg -i MissingFloor-Trailer.mp4 -vf "scale=1280:720:flags=lanczos,hqdn3d=2:1.5:3:3" -c:v libx264 -preset slow -b:v 1150k -pass 2 -c:a aac -b:a 96k -movflags +faststart MissingFloor-Trailer-web.mp4
```

README 맨 위 영상은 10MB 아래의 `-web` 파일을 GitHub 편집기에 끌어 넣어 올린다.

## 소리

게임의 WAV만 쓴다. 음악과 나레이션은 없다. `compose.py`가 ffmpeg loudnorm을 두 번 돌려
-19 LUFS, 최대 -1.5 dBTP로 맞추고 측정값을 `Saved/Trailer/audio-loudness.json`에 남긴다.

## 배포 전 확인

- 자막이 게임의 실제 문구와 같은지 본다. 붙잡혀도 게임 속 시각은 되돌아가지 않는다.
- 프레임이 빠지면(`TRAILER_CAPTURE PASS dropped=0`이 아니면) 편집하지 않는다.
- 작은 화면에서도 귀신의 윤곽과 문에 붙은 전단이 알아보이는지 본다.
- 릴리스에 배포용 MP4를 붙이고, README의 예고편 링크를 같은 버전으로 맞춘다.
