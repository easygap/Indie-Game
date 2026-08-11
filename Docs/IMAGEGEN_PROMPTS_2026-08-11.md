# ImageGen 생성 기록 — 2026-08-11

## M0 1인칭 두드리기 오른손 4단계

- 모드: OpenAI ImageGen 내장 도구, `stylized-concept`
- 참조 1: `Content/SourceArt/AI/SheetFirstPersonSleeveReference.png`
- 참조 2: `Docs/Media/night3-annex-doorway.png`
- 보존 원본: `Content/SourceArt/AI/SheetFirstPersonKnockPhases_v1.png`
- 투명 마스터: `Content/SourceArt/AI/SheetFirstPersonKnockPhases_v1_RGBA.png`
- 원본 SHA-256: `36C3E5BA1829C36D0B8CE967DFEF5719768E3AC02ABD444F088437F448DD58E1`
- 사용 범위: M0 Q/B 노크의 준비·예비·접촉·반동 UI-space 피드백

최종 프롬프트:

```text
Use case: stylized-concept
Asset type: production game animation sprite sheet for a first-person psychological horror game
Input images: Image 1 is the exact charcoal hoodie sleeve material and repair-stitch style reference; Image 2 is runtime context only for first-person camera scale and dark grounded tone, not a background to preserve.
Primary request: create one clean 2x2 sprite sheet showing four sequential phases of the same player's RIGHT forearm and bare fist performing one short door knock: (top-left) raised ready pose, (top-right) compact wind-up, (bottom-left) knuckles-forward contact pose, (bottom-right) small recoil pose.
Scene/backdrop: every cell must use the same perfectly flat solid #00ff00 chroma-key background. The background must be one uniform color with no shadows, gradients, texture, reflections, floor plane, lighting variation, or scenery. Add a narrow uniform #00ff00 gutter between cells.
Subject: one anatomically plausible young-adult right hand with five correct fingers, closed natural fist, attached to one forearm. The forearm enters from the lower-right edge of each cell. Match Image 1's washed charcoal hoodie, ribbed cuff, longitudinal seam, and three short black repair stitches near the inside forearm. Keep hand identity, proportions, sleeve, camera angle, and lighting exactly consistent across all four cells.
Style/medium: photorealistic grounded game render, realistic skin and fabric, restrained detail that survives a 512px frame, no glossy concept-art finish.
Composition/framing: equal-size 2x2 cells, same first-person perspective and scale in every cell, complete hand and enough sleeve visible, generous green padding around the silhouette, no part cut off except the sleeve naturally leaving through the lower-right cell edge.
Lighting/mood: soft neutral cool light on the subject only, subdued Korean apartment horror tone, no cast shadow on the green background.
Constraints: no text, no labels, no numbers, no watermark, no logo; do not use #00ff00 or green reflected spill anywhere on the hand or sleeve; crisp silhouette suitable for chroma removal; preserve the exact sleeve cues from Image 1.
Avoid: extra hands, detached wrist, duplicated or missing fingers, open palm, weapon, door, wall, environment, blood, injury, jewelry, nail polish, strong motion blur, dramatic colored rim light, baked contact shadow.
```

처리 결과:

- `remove_chroma_key.py --auto-key border --soft-matte --despill
  --edge-contract 1 --transparent-threshold 12 --opaque-threshold 220 --force`
- 검출 키: `#0ce30b`
- 투명 픽셀: 1,240,136 / 1,572,516
- 부분 투명 픽셀: 5,092 / 1,572,516
- 파생 프레임: `Content/SourceArt/T_FPHandKnock0_D.png`부터
  `Content/SourceArt/T_FPHandKnock3_D.png`, 각 768×768 RGBA. 원 셀을 좌상단
  63%에 맞추고 우측·하단 투명 여백을 남겨 런타임 타일 경계를 화면 밖으로 보낸다.
- 2차 처리는 기존 알파를 보존하고 경계의 잔류 초록 성분만 제한한다.

## M0 소매 연장 v2 편집

- 모드: OpenAI ImageGen 내장 편집
- 참조 1: `Content/SourceArt/AI/SheetFirstPersonKnockPhases_v1.png`
- 참조 2: `Saved/NightCapture/chase/frame_00002.png`(절단면 진단 전용)
- 보존 원본: `Content/SourceArt/AI/SheetFirstPersonKnockPhases_v2.png`
- 투명 마스터: `Content/SourceArt/AI/SheetFirstPersonKnockPhases_v2_RGBA.png`
- 원본 SHA-256: `412D9173E7EE8AFD2270CF45008E3904AC0530DF2D0CDFCFEBCBC3A6DD9241E7`
- 선택 사유: v1의 손·포즈는 승인하되 화면 안쪽에서 끝난 소매 단면을 제거하기
  위해, 손을 셀 좌상단 쪽에 재배치하고 같은 소매를 우하단 모서리까지 연장했다.

최종 편집 프롬프트:

```text
Edit/reference brief for a production game sprite sheet.

Image 1 is the approved 2x2 animation sheet. Preserve the exact same young-adult RIGHT hand identity, anatomy, skin, charcoal hoodie fabric, ribbed cuff, seam, three black repair stitches, four knock phases, cool neutral lighting, green key color, 2x2 ordering, and photorealistic grounded rendering.
Image 2 shows the runtime problem only: the lower sleeve ends in a visible rectangular cutoff inside the screen. Do not copy the corridor, UI, entity, lighting, or background from Image 2.

Recompose all four cells so the complete hand stays the same apparent size relative to the other frames, but sits farther toward each cell's upper-left quadrant. Extend the forearm and charcoal hoodie sleeve naturally at least 55% farther toward the lower-right. The sleeve must continue all the way out through the LOWER-RIGHT CORNER of every cell; it must not terminate or show a horizontal/vertical cut edge anywhere inside the green field. Keep enough sleeve length so a runtime camera can place the fist near screen center while the sleeve exit remains beyond the bottom-right screen edge.

Use the same perfectly flat, uniform solid #00ff00 chroma background in every cell and a narrow #00ff00 gutter. No shadow, gradient, texture, scenery, floor, or reflected green in the background. Crisp silhouette for chroma removal.

Keep all four equal cells and exact phase order:
top-left raised ready,
top-right compact wind-up,
bottom-left knuckles-forward contact,
bottom-right small recoil.
The forearm angle and sleeve length must remain continuous and consistent between frames. Preserve five correct fingers and one attached wrist in each frame.

No text, labels, numbers, watermark, logo, door, wall, environment, blood, jewelry, extra hands, duplicated fingers, detached wrist, open palm, weapon, strong motion blur, cast shadow, green spill, or baked contact shadow. Do not change the art style.
```

처리 결과:

- 검출 키: `#13de16`
- 투명 픽셀: 1,203,656 / 1,572,516
- 부분 투명 픽셀: 5,840 / 1,572,516
