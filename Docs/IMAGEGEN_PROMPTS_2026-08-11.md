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

## M1 위층 사람 포획 포옹 4단계

- 모드: OpenAI ImageGen 내장 도구, `stylized-concept`
- 참조 1: `Content/SourceArt/AI/SheetListenerEntityAnatomyReference.png`
- 참조 2: `Docs/Media/night1-listener-corridor.png`(런타임 크기·노출 참고 전용)
- 보존 원본: `Content/SourceArt/AI/SheetListenerCaptureEmbracePhases_v1.png`
- 투명 마스터:
  `Content/SourceArt/AI/SheetListenerCaptureEmbracePhases_v1_RGBA.png`
- 원본 SHA-256: `C27CAEBAE413E574AB3BA48AA95979718233C87893492420E8F5DBB9E11C999A`
- 사용 범위: M1 포획 암전의 접촉·접근·닫힘·유지 UI-space 피드백

최종 프롬프트:

```text
Use case: stylized-concept
Asset type: production 2x2 first-person capture-embrace animation sprite sheet for a grounded psychological horror game
Input images: Image 1 is the exact identity, dry gray plaster coating, worn clothing material, proportions, and non-gory tone of the same faceless adult entity; Image 2 is runtime context only for first-person camera scale and neutral-cool corridor exposure, not a background to preserve.
Primary request: create one clean 2x2 sprite sheet showing four sequential phases of the SAME entity gently catching and embracing the player from the player's first-person viewpoint. Only the entity's same two plaster-coated forearms and hands enter the camera frame; never show a face, head, torso, player body, wall, room, or scenery.
Frame order: top-left = first contact, fingertips and forearms just entering from both lower side edges; top-right = both open hands moving inward around the player's shoulders; bottom-left = forearms closing around both side edges in a restrained embrace; bottom-right = calm held-close pose with both arms settled around the screen edges while the central view remains readable for the blackout.
Scene/backdrop: every cell uses the same perfectly flat solid #00ff00 chroma-key background. One uniform color only: no shadows, gradient, texture, reflection, floor plane, lighting variation, or scenery. Add a narrow uniform #00ff00 gutter between cells.
Subject: exactly two anatomically correct adult hands and two forearms total in every cell, same left/right identity and scale across all four cells, five fingers per hand, intact wrists and sleeves, dry gray plaster dust and worn muted clothing continuous from Image 1. The gesture is protective and sorrowful, not violent: no grabbing the throat, no striking, no claws, no injury.
Style/medium: photorealistic grounded game asset, restrained practical-horror realism, crisp opaque silhouette suitable for chroma-key removal.
Composition/framing: player-eye first-person view; arms originate beyond the left and right/lower cell boundaries so no rectangular sleeve ends are visible; keep the middle 35 percent mostly open; no element crosses a cell gutter; consistent camera and lens in all cells.
Lighting/mood: cool neutral soft frontal light matching Image 1's material readability, subtle shading only on the subject.
Constraints: preserve the same entity identity and material in all frames; exact 2x2 ordering; clear progressive motion; hands fully plausible; generous key-color separation; no cast shadow; no contact shadow; no text; no symbols; no watermark; do not use #00ff00 in the subject.
Avoid: extra hands, extra arms, duplicated fingers, fused fingers, missing fingers, disembodied floating hands, aggressive attack pose, neck restraint, gore, blood, exposed wounds, face, body, environment, doors, walls, UI, vignette, motion blur, cropped fingers, internal rectangular cutoffs.
```

처리 결과:

- `remove_chroma_key.py --auto-key border --soft-matte --despill
  --edge-contract 1 --transparent-threshold 12 --opaque-threshold 220 --force`
- 검출 키: `#63f966`
- 투명 픽셀: 1,312,092 / 1,572,516
- 부분 투명 픽셀: 9,864 / 1,572,516
- 파생 프레임: `Content/SourceArt/T_FPCaptureEmbrace0_D.png`부터
  `Content/SourceArt/T_FPCaptureEmbrace3_D.png`, 각 1024×1024 RGBA.
- 흰 격자를 제외한 각 셀의 49.6% 영역만 크롭하고 기존 알파를 보존한 제한적
  green despill을 적용했다. Unreal에서는 UI group·NoMip·Clamp·NeverStream으로
  임포트하며, 화면의 긴 변을 기준으로 정사각 오버스캔해 비율을 늘리지 않는다.

## M5 공동 최종 잔존물 3D 제작 레퍼런스

- 모드: OpenAI ImageGen 내장 생성, `stylized-concept`
- 보존 원본: `Content/SourceArt/AI/SheetFinalCavityRemainsReference_v1.png`
- 원본 SHA-256: `950EA1404E6804C083F4EF060ABA7AE175EB70DAD9BE396333605E889C855297`
- 사용 범위: `SM_FinalCavityClothingShell`, `SM_FinalCavityBoneInsert`,
  `SM_FinalCavityTarp`, `SM_FinalCavityBrokenCaster`의 비율·재질·실루엣 참고
- 제외: 생성본의 피부·머리카락·배경·조명은 정사가 아니며 런타임에 사용하지 않는다.

최종 프롬프트:

```text
Use case: stylized-concept
Asset type: AAA indie horror game 3D production reference sheet
Primary request: a non-graphic, dry, partially skeletonized adult male remains collapsed naturally inside a narrow apartment maintenance-wall cavity, designed as a continuous 3D prop reference rather than a flat sprite
Scene/backdrop: neutral matte mid-gray studio reference backdrop, no environment, no floor reflections
Subject: the same remains shown in four consistent orthographic views (front, left side, back, top). Realistic adult Korean male proportions, compact fetal-like collapse constrained to a cavity, skull and rib-cage silhouette only partially visible beneath a heavily settled charcoal work jacket and faded dark work trousers; one twisted work shoe; the clothing carries most of the readable body volume. A folded dusty blue-gray waterproof tarp edge lies under one side, and a single broken rusted caster wheel from a small tool cart rests near the hip. Everything is completely dry and old.
Style/medium: photorealistic PBR 3D asset turntable render, grounded Korean apartment maintenance realism, production-modeling reference quality
Composition/framing: clean 2x2 reference grid, full prop visible in every view, consistent scale and proportions, generous padding
Lighting/mood: flat neutral softbox lighting that reveals shape and material response without dramatic shadows
Materials/textures: desiccated matte bone, dust-filled woven workwear, cracked rubber shoe sole, powdery gypsum dust, dull polyethylene tarp, oxidized steel caster
Constraints: non-graphic; no blood; no wet tissue; no exposed organs; no fresh corpse; no horror poster lighting; no text; no labels; no watermark; no extra props; no duplicated body parts; anatomically plausible; identical subject and clothing in all four views
Avoid: mannequin smoothness, stick-figure anatomy, zombie styling, gore, cinematic scene composition, baked directional lighting, transparent background
```

## M5 목한수 근접 3D 제작 레퍼런스

- 모드: OpenAI ImageGen 내장 생성, `stylized-concept`
- 보존 원본: `Content/SourceArt/AI/SheetMokHansooConfrontationReference_v1.png`
- 원본 SHA-256: `466FC1AF73CD8852E955022FA5D9FBE1F38A04F6623F318F966F0373FEBCF618`
- 사용 범위: `SM_MokHansooWorkwear`, `SM_MokHansooHeadHands`,
  `SM_MokHansooGypsumBoard`의 체형·복장·파지·표정 참고
- 제외: 원본을 크롭해 `T_SpriteMok_D` 대체로 쓰지 않는다.

최종 프롬프트:

```text
Use case: stylized-concept
Asset type: realistic horror game close-range 3D character production reference sheet
Primary request: Mok Han-su, a weary Korean apartment maintenance manager in his late 50s, holding one chipped gypsum board panel as if he intends to cover a newly opened wall cavity, designed for a continuous 3D character mesh rather than a sprite
Scene/backdrop: neutral matte mid-gray studio reference backdrop, no environment, no floor reflections
Subject: the same man shown in four consistent full-body views (front three-quarter, left profile, back three-quarter, and a close upper-body material view). Average Korean male build, slightly stooped posture from years of physical work, tired guarded face, short thinning salt-and-pepper hair, faded navy maintenance jacket over a gray polo, practical dark trousers, worn work shoes, thin cotton work gloves. Both hands naturally support a narrow broken-edged gypsum board at waist-to-chest height. His expression is fearful and ashamed, not villainous or aggressive.
Style/medium: photorealistic PBR 3D character turntable render, grounded contemporary Korean apartment realism, production-modeling reference quality
Composition/framing: clean 2x2 reference grid, full body visible in the first three views, identical proportions and wardrobe, generous padding
Lighting/mood: flat neutral softbox lighting that reveals anatomy, cloth folds, and material response without dramatic shadows
Materials/textures: faded nylon-cotton jacket, pilled polo fabric, dusty trousers, powdery chipped gypsum and torn paper facing, scuffed rubber shoes, cotton gloves
Constraints: no text; no logos; no watermark; no extra people; no extra props; no weapon; no blood; no monster traits; anatomically plausible hands; identical face, clothing, and board in every view; no baked directional lighting
Avoid: villain pose, action-hero build, glossy fashion styling, cinematic scene composition, sprite sheet animation, caricature, duplicated limbs, transparent background
```

## M5 공동 정면 PBR 디테일 레이어

- 모드: OpenAI ImageGen 내장 생성·편집, `game-asset`
- 참조: `Content/SourceArt/AI/SheetFinalCavityRemainsReference_v1.png`
- 보존 최종본: `Content/SourceArt/AI/FinalCavityFrontBlend_v1.png`
- 최종본 SHA-256: `9C26AAC9D3160CBD73B3183A332BC822FA8B6A1B655D5B24A6D642E1952B6D11`
- 파생: `T_SpriteFinalCavity_{D,N,R,A}`, 1024×1536
- 적용: 정면 105~360cm·내적 0.68 초과에서만 보인다. 같은 위치의 3D 셸은
  숨은 그림자를 유지하고, 범위를 벗어나면 디테일 레이어 대신 3D 셸이 보인다.

최종 생성 프롬프트:

```text
Use case: game-asset
Asset type: front-facing PBR detail cutout for a first-person Korean apartment horror game, to be alpha-blended over an existing continuous 3D shell
Primary request: create one single coherent front view of the dry, partially skeletonized adult male remains from the reference, collapsed upright inside a narrow 120 cm wall cavity. Clothing must carry the silhouette: old charcoal work jacket split slightly at center, folded trousers, one bent work shoe, dusty blue-black tarp edge and one broken cart caster. Only a restrained dry skull and five paired rib arcs are visible through the jacket. No skin, no hair, no wet tissue, no blood, no gore.
Composition: one centered full arrangement from shoe to skull, straight-on camera, modest perspective matching a player standing 1.8 m away, complete silhouette with generous empty margin, no cropping
Materials: matte old cloth, chalky dry bone, powdery gypsum dust, low-sheen tarp, worn steel caster; flashlight-readable microdetail, no glossy plastic
Lighting: neutral soft front-left studio light with subtle occlusion, no cast shadow beyond the subject, preserve game-lighting-compatible values
Background: perfectly uniform pure chroma green #00FF00 edge-to-edge, no gradient, no floor line
Constraints: exactly one subject arrangement, physically connected anatomy and clothing, realistic Korean indie horror production asset, non-graphic, no mannequin, no eyeballs, no exposed flesh, no text, no labels, no border, no grid, no watermark, no props beyond tarp/shoe/caster
Output: clean high-resolution cutout source suitable for chroma-key alpha extraction and masked lit material
```

배경 정리 편집 프롬프트:

```text
Edit only the background and backing of this game cutout. Preserve the exact dry clothed skeleton, skull, rib cage, folded trousers, shoe, tarp edge and broken caster at the same scale, pose, lighting and position. Completely remove the gray concrete/plaster slab, rubble panel and every wall-shaped surface behind or around the subject. Replace every removed pixel with perfectly uniform pure chroma green #00FF00, matching the existing green background edge-to-edge. The finished image must contain only the connected remains, clothing, tarp and caster floating cleanly on green. Keep natural antialiased edges and all internal detail. Do not add or remove bones, do not add skin, hair, blood, gore, shadow, text, border, grid, floor line or any other prop. Output a single high-resolution chroma-key cutout source.
```

## M5 목한수 상반신 PBR 디테일 레이어

- 모드: OpenAI ImageGen 내장 생성, `game-asset`
- 참조: `Content/SourceArt/AI/SheetMokHansooConfrontationReference_v1.png`
- 보존 원본: `Content/SourceArt/AI/MokHansooFinalFrontBlend_v1.png`
- 원본 SHA-256: `B2C6787D66E595F6A32BD6FD653060773097BF560CFE0EEDF0877EAB6FA6547C`
- 파생: `T_SpriteMokFinalUpper_{D,N,R,A}`, 1024×1536
- 적용: 얼굴·재킷은 정면 범위에서만 블렌딩하고 알파를 세로 31~39%에서
  감쇠한다. 실제 95cm 석고보드가 전환선을 가리며 하체·보드·그림자는 3D다.

최종 프롬프트:

```text
Use case: game-asset
Asset type: front-facing PBR character detail cutout for a first-person Korean apartment horror game, to be alpha-blended over an existing continuous 3D character and shadow shell
Primary request: Mok Han-su from the reference, a weary late-50s Korean apartment maintenance manager, standing directly toward the player while holding one chipped gypsum board panel in both hands at lower waist height. Keep his face fully visible and show his navy work jacket, gray undershirt, worn work trousers, practical shoes and dusty hands. He is frightened and ashamed, not aggressive.
Pose: tired slightly hunched stance, uneven weight, elbows naturally bent down, hands visibly gripping the side edges of the board; board top must stay below the navel and must not hide the jacket chest or face
Composition: exactly one full-body person, centered, straight-on camera at eye height, complete shoes and head, generous empty margin, no cropping
Materials: faded navy cotton workwear with seams, wrinkles and gypsum dust; natural late-middle-aged Korean facial structure, short graying hair, subtle under-eye fatigue; chipped matte gray-white gypsum board with realistic 3 cm thickness
Lighting: neutral soft front-left studio light, subtle occlusion, no dramatic rim light, values suitable for a lit masked game material
Background: perfectly uniform pure chroma green #00FF00 edge-to-edge, no gradient, no floor line
Constraints: realistic restrained horror, human proportions, no stylization, no mannequin face, no featureless skin, no smile, no weapon, no blood, no gore, no text, no logo, no border, no grid, no watermark, no extra people or duplicate limbs
Output: clean high-resolution cutout source suitable for chroma-key alpha extraction and close-range hybrid 3D rendering
```
