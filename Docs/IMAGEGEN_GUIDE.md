# gpt-image 프롬프트 기준

게임에 들어가는 이미지를 gpt-image로 만들 때 붙이는 블록과 절차다. 생성 이미지는
그냥 두면 픽셀 단위로 자글자글한 점묘가 끼고, 그 점이 노멀맵으로 유도되면 손전등
아래에서 모래처럼 반짝인다. 아래 블록은 2026-08-14 표면 여섯 장과 2026-08-20 벽지에서
실측으로 검증했다(점묘 에너지 0.39~1.37, 기존 승인 에셋 6.89).

원본은 `Content/SourceArt/AI/<이름>_<날짜>.png`에 두고 같은 이름의 JSON에 도구·날짜·
프롬프트·SHA-256·용도를 적는다. 에셋 대장(`ASSET_POLICY.md`)에도 한 절을 남긴다.

## 타일 재질

프롬프트 끝에 두 블록을 그대로 붙인다. 문장을 고치면 다시 재야 한다.

### 점묘 억제

```text
Rendering discipline (critical): this is a flat material scan, not a photograph of a lit scene. All surface variation must come from real physical causes — trowel passes, roller strokes, wear paths, mineral settling, paint flow, footfall polish — and must be expressed as broad soft tonal fields at roughly 5 to 40 cm real-world scale. Resolve every detail with gentle continuous gradients, never with fine dots. Absolutely no per-pixel noise of any kind: no stippling, no dithering, no halftone or screen-door pattern, no pointillist speckle, no sand or dust overlay, no crosshatch, no chromatic speckle, no oversharpened edge halos, no JPEG mosquito artifacts, no added film grain. Prefer slightly soft over crunchy; if uncertain, make it smoother. Keep the entire image inside a narrow luminance band of roughly 25 to 65 percent, with no pure black and no pure white pixels. Flat neutral diffuse capture only: no directional light, no cast shadow, no contact shadow, no specular highlight, no bloom, no vignette, no depth of field, no color grading. Shot as a calibrated flat copy-stand capture at base ISO. No text, no numbers, no letters, no logos, no watermark, no hands, no props, no border, no frame.
```

### 타일 균일성

```text
Tiling uniformity (critical): this texture repeats across a large surface, so it must have no center and no focal point. Statistically identical everywhere — no single area brighter, darker, more worn, or more detailed than any other, no gradient across the frame, no soft glow anywhere, no ponding, no lane, no patch. If you would naturally place one interesting feature, distribute that character evenly across the whole frame instead. Any localized feature will visibly repeat in a grid and ruin the surface.
```

점묘 억제 블록은 진짜 요철까지 같이 누른다. 받은 알베도의 표준편차가 1.9~3.8로
오는데 승인 대역은 10~12다. 노멀을 유도할 신호가 모자라므로
`Scripts/condition_ai_tiles.py`의 `detail_gain`으로 되살린다(계단 3.0, 방수 3.0,
문 2.6). 조건화는 `generate_ai_pbr_maps.py`보다 먼저 돌아야 한다. 남은 얼룩이
노멀에서 가짜 기하가 되기 때문이다.

## 인물·소품 컷아웃

타일 블록의 「flat copy-stand capture」와 「no directional light」는 인물에 맞지 않는다.
배경과 점묘 억제를 이렇게 바꿔 쓴다.

```text
Scene/backdrop: a perfectly flat solid #00ff00 chroma-key background. The background must be one uniform color with no shadows, gradients, texture, reflections, floor plane, lighting variation, or scenery.
Constraints: do not use #00ff00 or any green reflected spill anywhere on the subject; crisp silhouette suitable for chroma removal; generous green padding around the silhouette; no part of the subject cut off by the frame edge; no text, no labels, no numbers, no watermark, no logo, no border, no cast shadow on the green background.
```

```text
Rendering discipline (critical): resolve every surface with gentle continuous gradients, never with fine dots. Absolutely no per-pixel noise of any kind: no stippling, no dithering, no halftone or screen-door pattern, no pointillist speckle, no sand or dust overlay, no crosshatch, no chromatic speckle, no oversharpened edge halos, no JPEG mosquito artifacts, no added film grain. Prefer slightly soft over crunchy; if uncertain, make it smoother. Restrained detail that still reads at 512 px. Soft neutral cool light on the subject only, no rim light, no bloom, no colour grading, no depth of field, no motion blur, no baked contact shadow.
```

## 장면·인쇄물·아이콘

사진 같은 장면(아이콘, 원경, 방 안)은 위 블록 대신 한 줄로 충분하다.
`Clean, smooth tonal gradients like a sharp modern digital photograph. Absolutely no film grain, no noise, no dithering, no halftone dots, no stippled or speckled texture.`
인쇄물(전단, 스티커)은 「사진처럼 깨끗하고 매끈한 인쇄면」과 필름 그레인·노이즈·
디더링·망점 금지를 한국어로 적었다. 받은 뒤에는 원본 해상도로 잘라 확대해 보고,
하늘처럼 평평한 곳의 고주파 편차(3×3 평균과의 차)를 잰다. 2026-09-30 아이콘
원본은 하늘에서 1.65였다.

## 만들지 않는 것

- 의미 있는 한글·숫자. 인쇄물 원문과 게임 속 숫자는 코드와 런타임 글자가 맡는다.
  생성 이미지의 틀린 글자가 증거처럼 보이면 안 된다. 짧은 헤드라인은 예외로 두되
  받은 뒤 한 글자씩 확인한다.
- N/R/A 맵. `generate_ai_pbr_maps.py`가 `_D` 한 장에서 만든다.
- 자리가 정해진 흔적(손자국, 얼룩). 타일이 아니라 마스크로 따로 만든다.
