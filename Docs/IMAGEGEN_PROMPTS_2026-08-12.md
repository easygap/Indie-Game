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
