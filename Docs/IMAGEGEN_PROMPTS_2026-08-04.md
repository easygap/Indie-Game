# ImageGen 생성 기록 — 2026-08-04

텍스처 원본은 이미지 생성 도구로 제작한 뒤 `Content/SourceArt/AI/`에
보존합니다. 게임에는
`Scripts/Prepare-AIArt.ps1`이 정규화한 1K BaseColor와
`Scripts/generate_ai_pbr_maps.py`가 만든 PBR 동반 채널만 사용합니다.

## CH03 물탱크 내부 습윤 표면

- 저장소 보존본: `Content/SourceArt/AI/TextureTankInteriorBiofilm.png`
- 파생: `T_TankInteriorBiofilm_{D,N,R,A,W,M}`
- 적용: `M_TankInteriorBiofilmUV` → `SM_TankInternalLining`
- 모델링 계약: 내경 290cm, 외경 294cm, 바닥 Z 381.2cm, 벽 상단 Z 596cm,
  비충돌·그림자 비투사. 기존 탱크 충돌과 진행 상태는 바꾸지 않습니다.

```text
Use case: stylized-concept
Asset type: tileable game texture source for a UE 5.8 first-person Korean everyday-horror game
Primary request: a seamless photorealistic material scan of the INSIDE surface of an aging galvanized rooftop reserve-water tank in a small Korean villa, after years below the waterline
Scene/backdrop: orthographic flat material capture filling the entire square canvas
Subject: low-contrast cool gray galvanized steel mostly obscured by restrained cloudy mineral deposits, pale calcite tide residue, subtle dark olive-gray biofilm veils, sparse narrow iron-orange rust rivulets, irregular wet patches and fine maintenance wear
Style/medium: physically plausible PBR material-source photography, grounded 2026 indie environmental realism, ordinary building neglect rather than decorative horror
Composition/framing: perfectly square, front-facing orthographic surface, uniform detail distribution, seamless/tileable edges, no single focal mark, no perspective
Lighting/mood: flat neutral diffuse reference lighting with no baked directional shadow, no specular hotspot, subdued predawn blue-gray color response
Materials/textures: fine zinc crystal grain still faintly visible beneath deposits; believable wet/dry roughness variation; micro detail at real-world 1 meter coverage
Constraints: seamless on all four edges; no objects, pipes, bolts, seams, labels, writing, symbols, blood, gore, fingerprints, faces, silhouettes, creatures, watermark, frame or border
Avoid: fantasy corrosion, dramatic contrast, bright green algae, neon color, repeating obvious blobs, large holes, baked lighting, ambient-occlusion corners
```

원화는 공포 이미지를 직접 그리는 데 쓰지 않았습니다. 평범한 관리 부실의
물성을 제공하고, 공포는 수면·인체 실루엣·손전등·음향·발견 순서의 런타임
조합에서 만들도록 역할을 분리했습니다.
