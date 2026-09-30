# ImageGen 프롬프트 기록 — 2026-09-30

## Windows 배포 아이콘

- 도구: gpt-image 스킬(Codex 내장 image_gen, ChatGPT 구독 인증)
- 원본: `Content/SourceArt/AI/ApplicationIcon_20260930.png`
  (SHA-256 `3EED582D726AEA2AEE7FD4F0ACAFCFD84A710581473EB87939975B8979B46D2B`), 같은 이름의 JSON에 프롬프트와 자르기 구역을 적었다
- 등급 PNG: `Build/Windows/ApplicationIcon.png`
- 실행 파일 아이콘: `Build/Windows/Application.ico`
- 후처리: `Scripts/prepare_application_icon.py --crop 0.20 0.12 0.80 0.72`

예전 아이콘은 옛 이야기의 옥상 물탱크 점검구였다. Missing Floor의 중심은 서류에
없는 5층, 붉은 벽돌 빌라 옥상에 무단으로 올린 옥탑방이라 그 방에 불이 하나 켜진
장면으로 바꿨다. 세 구도(건물 모서리를 올려다본 것, 정면, 옥탑방 벽만 가까이 본 것)를
만들어 16·24·32·48px에서 비교했고, 정면 구도가 건물과 옥탑방을 함께 보여 주면서
작은 크기에서도 창 불빛이 남았다. 원본은 그대로 두고 옥탑방과 꼭대기층만 보이게
잘라 쓴다.

```text
Square 1:1 application icon for a Korean first-person horror game. Photographic realism with a bold, symmetrical silhouette that reads at 32x32 pixels. A straight-on view of the top two floors and the roof of a plain four-storey Korean villa made of dark red brick, at 4:30 a.m. under a deep blue pre-dawn sky. Built on the roof, set back from the edge, is a crude unfinished rooftop room made of pale grey sandwich panels on a thin steel frame, the kind of illegal add-on that is not on any building register. It has one small window with a dim warm yellow light behind it; everything else in the image is dark and cold. The brick facade below is almost black with unlit windows and a few air-conditioner units. Composition: rooftop room centered in the upper half, the brick building forms a dark base across the lower half, sky above is smooth and empty. Mood: still, lonely, wrong in a quiet way; no monsters, no silhouettes of people. Palette: near-black brick, navy sky, pale grey panels, one small warm window. Crisp, clean, smooth gradients like a high-end digital photo. Strictly no film grain, no noise, no dithering, no halftone, no stipple, no speckled or gritty texture, no painterly strokes. No text, no numbers, no letters, no signs, no logos, no people, no faces, no border, no frame, no mockup, no watermark.
```

후처리는 자른 구역을 1024×1024로 줄인 뒤 중간톤 감마(0.82), 대비 1.08, 채도 0.90,
작은 반경 언샤프 마스크를 고정값으로 건다. ICO에는 16, 24, 32, 48, 64, 128, 256
픽셀 레벨이 들어 있다. 밝은 작업 표시줄과 어두운 작업 표시줄에서 16~48px을 눈으로
확인했고, 확대해도 필름 입자나 점묘 같은 잡티가 없다.
