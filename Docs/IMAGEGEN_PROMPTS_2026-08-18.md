# ImageGen 제작 기록 — 2026-08-18

## 한국 저층 빌라 외벽 스터코

- 실행 경로: OpenAI 내장 ImageGen
- 용도 분류: `game-asset`
- 입력 이미지: 없음
- 보존 원본:
  `Content/SourceArt/AI/TextureKoreanVillaStucco_v1.png`
- 원본 SHA-256:
  `474A8D005146B499FDB39CBAE62FC17F790EC71E9D833E4864A387DBA959BA57`
- BaseColor 파생본:
  `Content/SourceArt/T_KoreanVillaStucco_D.png`
- BaseColor SHA-256:
  `60171906AB0B4D13176D9516024C585DA1A4EC672100D92B331E1C365BC9BFDB`
- PBR 파생본: `T_KoreanVillaStucco_N.png`,
  `T_KoreanVillaStucco_R.png`, `T_KoreanVillaStucco_A.png`
- 런타임: `/Game/Prototype/Textures/T_KoreanVillaStucco_{D,N,R,A}`와
  `/Game/Prototype/Materials/M_VillaStucco_{X,Y}`
- 적용 경계: 생성본은 무문자·무조명 외벽 BaseColor만 담당한다. 노멀,
  거칠기, 차폐, 월드 매핑, 조명과 그림자는 프로젝트 파이프라인과 UE가
  담당한다. 실존 상호·건물·인물을 참조하거나 포함하지 않는다.

### 최종 프롬프트

```text
Use case: production game environment texture. Asset type: seamless tileable PBR albedo source texture, square. Create a photorealistic close-up orthographic surface of an older Korean low-rise villa exterior wall: pale cool-gray cement render and fine stucco, subtle aggregate grain, restrained age variation, faint rain streaks, a few hairline repaired cracks, tiny dark grime near pores, slight uneven roller patches. The surface must look maintained but 15–25 years old, suitable for a narrow Seoul residential alley at predawn. Flat diffuse capture, uniform neutral illumination, no directional shadows, no perspective, no corners, no pipes, no windows, no objects, no people. Edge-to-edge seamless tiling with no obvious repeated focal mark. No text, no Korean characters, no signage, no brands, no logos, no watermark. Natural low-contrast albedo only; do not bake specular highlights, ambient occlusion, or lighting into the image. Preserve realistic centimeter-scale material detail and neutral color calibration.
```
