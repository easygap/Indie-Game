# ImageGen 제작 기록 — 2026-08-12

## 5회 포획 뒤 401호 실물 메모 종이

- 실행 경로: 내장 ImageGen
- 용도 분류: `stylized-concept`
- 입력 이미지: 없음
- 생성 원본:
  `Content/SourceArt/AI/TextureCaptureMercyNotePaper_D.png`
- 파생 텍스처:
  `Content/SourceArt/T_CaptureMercyNote_D.png`
- 원본 SHA-256:
  `FEC6D7DDF6611A8E355FDABDE6B1B4AD607ADCB66EF66FAB35238E4AED7FFB99`
- 적용 경계: ImageGen은 종이 섬유·접힘·손때만 담당한다. 플레이에 필요한
  `소리를 줄여라. 걔는 눈이 없어.` 문구는 `Create-SignTextures.ps1`에서
  시스템 한글 폰트로 합성해 철자와 명암 대비를 고정한다.

### 최종 프롬프트

```text
Use case: stylized-concept
Asset type: production source albedo texture for a loose paper note in a first-person Korean psychological horror game
Primary request: create a perfectly front-facing, edge-to-edge square sheet of cheap warm gray-beige recycled memo paper, plausibly torn from an elderly resident's small household notepad. The paper will later slide out from under apartment 401's door after the player's fifth capture.
Style/medium: restrained photorealistic game texture, physically plausible paper base color, not concept art and not a photographed mockup
Composition/framing: orthographic 1:1 paper surface fills the entire canvas; no surrounding background; leave the central 75 percent clean for later programmatic Korean handwriting
Lighting/mood: flat neutral diffuse capture lighting with no directional shadow, suitable as a lit material albedo in Unreal Engine
Color palette: aged warm gray, faint nicotine-yellow edge variation, very low saturation
Materials/textures: fine recycled fibers, subtle fingertip oils, two faint pressure dents from hurried writing, one soft horizontal fold, very slight uneven torn-fiber variation at the top edge while all four canvas edges remain usable
Constraints: no words, no Hangul, no letters, no numbers, no symbols, no ruled lines, no printed pattern, no illustration, no blood, no gore, no logo, no watermark, no hand, no pen, no floor, no door, no tape, no perspective, no border, no frame, no cast shadow, no transparency; readable material detail after downsampling to 1024x1024
Avoid: pristine stationery, dramatic stains, horror clichés, burned edges, heavy crumpling, high-contrast dirt, vignette
```

## 타이틀 배경 — 무영로 새벽 빌라

- 실행 경로: 내장 ImageGen
- 용도 분류: `stylized-concept`
- 입력 이미지: 없음
- 생성 원본: `Content/SourceArt/AI/TitleBackgroundMissingFloor_v1.png`
- 파생 텍스처: `Content/SourceArt/T_TitleBackground_D.png` (1920×1080)
- 런타임 에셋: `Content/UI/Textures/T_TitleBackground_D.uasset`
- 원본 SHA-256:
  `4831357AA6439F9CF93CC3D5CC4664DDF8EB995D59759E1F319504246CD57D39`
- 적용 경계: 생성물은 건물·골목·빛·젖은 노면만 담당한다. 제목, 메뉴, 포커스,
  입력 안내는 모두 런타임에서 그려 현지화·확대·`이어하기` 숨김을 보장한다.

### 최종 프롬프트

```text
Use case: stylized-concept
Asset type: 16:9 game title-screen background key art for a grounded Korean psychological horror game
Primary request: a photorealistic, restrained environmental portrait of an aging five-storey Korean multi-family villa just before dawn, seen from the wet alley at a slightly low eye level; the building should feel inhabited and ordinary before it feels unsettling
Scene/backdrop: 2024 Seoul outskirts, humid late-summer pre-dawn, practical Korean villa architecture, narrow residential alley, overhead utility wires, subtle rain residue and a shallow foreground puddle
Subject: the villa occupies the right two-thirds of frame; a rooftop water tank is only a barely readable silhouette; exactly one fourth-floor window has a small believable tungsten household glow; no people or figures
Style/medium: cinematic photorealistic environment still, grounded location photography, natural lens perspective, restrained film grain, production-ready game key art rather than fantasy concept art
Composition/framing: wide 16:9 establishing shot; preserve quiet dark negative space across the left third for runtime title and vertical menu; keep the foreground puddle visible near the lower edge but do not form readable symbols or numbers in the reflection
Lighting/mood: blue-black predawn ambient light, concrete gray and desaturated blue-gray, one tiny warm amber window; shadows retain subtle texture; unease through ordinary spatial details, not horror effects
Materials/textures: weathered but maintained concrete, modest tile and metal railings, damp asphalt, realistic patch repairs, faint mineral staining
Constraints: absolutely no text, numbers, signage, logos, watermark, UI, title, characters, faces, silhouettes, ghosts, monsters, blood, handprints, religious architecture, or implausible extra floors; architecturally believable Korean residential details; leave the left third uncluttered and dark enough for pale runtime text
Avoid: cyberpunk neon, teal-orange grading, excessive fog, fisheye distortion, Dutch angle, abandoned ruin, dramatic lightning, jump-scare imagery, glossy AI-surreal surfaces, painterly fantasy look
```
