# ImageGen 제작 기록 — 2026-08-12 소리·밝기 보정

## 최초 실행 보정판 벽면

- 실행 경로/모드: OpenAI 내장 ImageGen, 신규 이미지 생성
- 입력 이미지: 없음
- 용도: Unreal Engine 첫 실행 소리·밝기 보정 UI의 무문자 배경 텍스처
- 생성 원본: `Content/SourceArt/AI/TextureAudioCalibrationWall_v1.png`
- 파생 텍스처: `Content/SourceArt/T_AudioCalibrationWall_D.png`
- 런타임: `/Game/Prototype/Textures/T_AudioCalibrationWall_D`
- 원본 크기/SHA-256: 1254×1254,
  `4F1A21E7E0C90EE5564170C0149BF13F875860A48B79A612124F6DD8589C595B`
- 파생 크기/SHA-256: 1024×1024,
  `C0A9280AE6DD0F4BBEBC107ECE024C3785377106B63B5328DE354798A10E3C43`
- 적용 경계: ImageGen은 벽의 광물 결·롤러 자국·미세 보수 흔적만 담당한다.
  한글, 눈금, 계조, 선택 상태는 모두 런타임 HUD가 렌더링한다.

### 최종 프롬프트

```text
Use case: production game UI texture for a first-run audio and brightness calibration screen in a Korean psychological horror game.
Asset type: seamless-feeling, textless square background plate, to be imported as an Unreal Engine UI texture.
Subject and composition: a perfectly front-facing close view of an old apartment-villa interior wall, almost black painted plaster, filling the entire square edge to edge. Subtle paint roller marks, fine mineral grain, tiny age scratches and faint repaired patches. Keep the center calm enough for overlaid UI. Preserve delicate shadow information at roughly three low-darkness levels so it is useful for a brightness calibration test.
Style and mood: restrained cinematic realism, oppressive but elegant, grounded contemporary Korean villa atmosphere, tactile practical-surface photography rather than fantasy or illustration.
Lighting and palette: completely flat soft diffuse light with no directional cast shadows; blue-black charcoal base with sparse warm-gray flecks; very low contrast overall but still readable in dark tones; no crushed-black blob, no bright highlights.
Framing and technical constraints: orthographic/front-on appearance, square composition, no perspective lines, no border, no frame, no vignette, no depth-of-field, no isolated objects. Texture should tile or crop cleanly and remain useful behind Korean runtime typography.
Do not include: any words, Hangul, letters, numbers, symbols, logos, signatures, watermarks, UI elements, labels, doors, handles, people, creatures, furniture, paper, post-it notes, windows, lamps, graffiti, stains shaped like faces, or obvious focal objects.
```
