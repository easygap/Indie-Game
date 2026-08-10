# ImageGen 프롬프트 기록 — 없는 층 비주얼 패스

- 서비스: OpenAI ImageGen 내장 도구
- 생성일: 2026-08-10
- 공통 금지: 실존 상표·로고·주소·워터마크·가짜 한글·고어·과장된 판타지
- 공통 톤: 2020년대 한국 생활공포, 저채도 청회색과 낡은 아이보리,
  현실적인 재료·치수, 같은 35mm 카메라와 억제된 영화적 대비

## 환경 레퍼런스

`SheetMissingFloorEnvironmentReference.png`

```text
Create a strict 2x2 photoreal production reference sheet for one Korean
low-rise villa horror game: dusk brick villa exterior, narrow fourth-floor
corridor, illegal lightweight-steel fifth-floor room on an existing concrete
roof slab, and rooftop passage beside a galvanized water tank. Same building,
materials, lens and restrained color grade in all panels. Plausible Korean
doors, railings, pipes, fluorescent fixtures and human scale. No people, no
text, no signage, no impossible geometry, no floating objects, no portal,
no fantasy architecture, no dramatic baked shadows that would become texture.
```

## 위층 사람 해부 레퍼런스

`SheetListenerEntityAnatomyReference.png`

```text
Create a strict four-view anatomy and silhouette reference of the same prone
adult human figure for a realistic first-person Korean sound-horror game:
front/ground view, side, top, and three-quarter. The figure supports the torso
on both forearms and drags two injured clothed legs. Continuous head, neck,
clavicles, ribcage, pelvis, elbows, hands with long individual fingers, knees,
shins and feet; restrained natural proportions. Entire surface dry pale
plaster and worn clothing, face fully featureless, no exposed skin, no blood,
no gore, no rubber creature, no mannequin joints, neutral studio background.
```

## 건식 석고 PBR 원본

`TextureMissingFloorDryPlaster.png`

```text
Seamless square base-color scan of old dry off-white gypsum plaster in an
unfinished Korean rooftop annex. Fine mineral grain, shallow hairline cracks,
subtle trowel variation and sparse grey dust. Flat even cross-polarized studio
lighting, no directional shadow, no highlight, no perspective, no objects,
no stains shaped like faces, no text. Low saturation, physically plausible
and tileable on every edge.
```

## 위층 사람 정면 가독성 레이어

`ListenerEntityFrontCutout.png`

기준 이미지: `SheetListenerEntityAnatomyReference.png`

```text
Using the provided four-view anatomy sheet as the exact character and costume
reference, create one isolated production cutout for a photoreal Korean
psychological-horror game. A continuous anatomically believable adult male in
the same prone forearm-supported crawl, viewed straight-on from a low eye level
with a slight three-quarter offset. Both shoulders, forearms, hands, torso,
bent knees, lower legs and shoes remain visibly connected. The faceless
plaster-coated head is lowered but the neck connection is unmistakable.
Ordinary faded patterned pajamas are completely coated in dry gray-beige
plaster dust. No gore, exposed anatomy, mutation, mannequin smoothness or
floating body parts. Fit the entire silhouette with generous margin, hands
closest to camera and legs visible behind. Use a perfectly uniform #00FF00
background with no ground plane, shadow, green spill, prop, text, border or
panel. Neutral soft studio lighting, photoreal PBR fabric and plaster detail.
```

이 원본은 3D 대체물이 아니다. 초록 키 제거 후 D/N/R/A를 생성해 정면
복도 화각의 사람 판독 LOD로만 쓴다. 1.6m에서 활성화하고 1.25m까지
히스테리시스로 유지하되 측면에서는 연속 3D 셸로 즉시 전환한다.

## 위층 사람 4단계 기어오기 시트

`SheetListenerEntityCrawlPhases.png`

기준 이미지: `SheetListenerEntityAnatomyReference.png`,
`ListenerEntityFrontCutout.png`

```text
Create a production sprite atlas for the exact same faceless plaster-coated
adult Korean apartment resident shown in the reference images. Orthographic-
feeling eye-level front camera, centered full body, crawling low on elbows and
dragging the legs. A clean 2 by 2 grid with four distinct consecutive
locomotion phases: upper-left left elbow planted/right shoulder advancing;
upper-right weight centered/chest lowest; lower-left right elbow planted/left
shoulder advancing; lower-right recovery/legs dragging. Preserve the same body
proportions, charcoal pajama clothing, cracked dry plaster coating, blank
featureless face, restrained Korean psychological-horror realism, and
identical camera scale in all four cells. Each body must fit entirely inside
its cell with generous equal padding and must not touch grid borders. Flat
perfectly uniform chroma-key green background RGB 0,255,0 in every cell, no
floor, no cast shadow, no glow, no text, no labels, no borders, no extra limbs,
no duplicated body parts, no gore, no watermark. Photoreal game asset source,
physically plausible anatomy and weight distribution, consistent identity and
lighting across all four poses.
```

네 칸은 각각 `T_SpriteListenerCrawl0..3_{D,N,R,A}`로 분리한다. 이동
속도에 따라 1.6~6fps로 순환하고, 정지하면 현재 체중 지지 자세를 유지한다.
응답 노크 성공 시에는 1번 자세에 고정해 괴물이 얼어붙는 것이 아니라
두 팔꿈치를 짚고 다음 대답을 기다리는 사람으로 읽히게 한다.

## 흔적 값 마스크

`SheetMissingFloorResidueMasks.png`

```text
Strict 2x2 square sheet of high-resolution grayscale game masks on pure black,
separated by clean gutters: overlapping exhausted handprints, long body drag
trails, a dusty floor-to-wall joint deposit, and frantic inner-cavity scratch
clusters. White means maximum residue, black means none, soft antialiased
edges, realistic scale variation. No lighting, no color, no background texture,
no text, no frame, no gore and no complete human silhouette.
```

## 원거리 인물 스프라이트

`SheetMissingFloorDistantCharacters.png`

```text
Strict 2x2 full-body photoreal character cutout sheet on one perfectly uniform
chroma-magenta background: a guilty middle-aged Korean man in subdued casual
clothes looking up from across an alley; an elderly Korean male landlord behind
management-office glass; an 81-year-old Korean woman in practical indoor
clothes; a young Korean convenience-store worker in an unbranded uniform.
Same cloudy daylight, 85mm lens, feet fully visible, restrained poses, no text,
no logos, no floor shadow baked into the background, no cropped limbs.
```

## 핵심 소품 레퍼런스

`SheetMissingFloorHeroPropsReference.png`

```text
Strict 2x2 photoreal prop reference sheet with the same neutral softbox light
and metric scale: a compact two-tier piano tuner's tool cart with four wheels;
a correct L-shaped piano tuning hammer with grip, offset steel shank and square
socket; a worn Korean apartment complaint ledger with real page thickness and
no readable writing; a blank calendar-back spiral sound journal. Isolated on
neutral grey, three-quarter product views, no brands, no fake letters, no
floating objects, no dramatic shadow, no fantasy tools, no listening wand.
```

파생·배치 규칙은 `MISSING_FLOOR_ART_MATRIX.md`, 소유·증빙 규칙은
`ASSET_POLICY.md`를 따른다.

## 「듣는 것들」 기록 종이 표면

- 원본: `Content/SourceArt/AI/TextureMissingFloorJournalPaper_v1.png`
- 임포트 원본: `Content/SourceArt/T_MissingFloorJournalPaper_D.png`
- 런타임: `/Game/Prototype/Textures/T_MissingFloorJournalPaper_D`
- 생성 방식: OpenAI ImageGen 내장 도구, 신규 이미지 생성
- 원본·임포트 SHA-256:
  `4FB7FA486F1A35B433006D1C327B2D07304D560F1CCC0FD04AC509EDBF0B28D8`

```text
Use case: game UI production texture
Asset type: full-screen journal surface texture for a first-person Korean psychological horror game
Primary request: an unfolded, physically believable Korean apartment maintenance ledger made from slightly aged off-white paper, prepared as a clean surface behind a three-lane evidence journal
Scene/backdrop: only the paper surface, seen perfectly straight-on like a high-resolution flatbed scan; no desk, hands, clips, objects, or surrounding scene
Subject: one continuous landscape paper spread with three extremely subtle vertical bookkeeping lanes; faint blue-gray ruling, a few faded red ledger guide marks, a restrained center fold, tiny fiber grain, pressure dents, erased graphite haze, slightly darkened handled edges, and realistic uneven paper tone
Style/medium: photorealistic tactile paper scan, contemporary premium indie game UI production asset, restrained Korean everyday realism, not concept art
Composition/framing: 16:9 landscape, edge-to-edge paper, large uninterrupted safe areas for runtime Korean text and cards; the three lane regions should remain calm and readable
Lighting/mood: neutral flat archival scan lighting with minimal directional shadow; quiet, worn, intimate, unsettling only through age and use
Color palette: warm gray ivory paper, desaturated blue-gray rules, extremely sparse muted oxide red accents
Materials/textures: real cellulose fibers, shallow folds and handling wear; no glossy coating
Constraints: absolutely no readable text, no letters, no Hangul, no numbers, no symbols, no icons, no logos, no trademarks, no watermark; no pre-rendered UI frames or cards; no perspective; no page curl; no strong cast shadows; no dramatic vignette; no stains resembling blood; no torn edges; no repeated obvious texture pattern; keep all important marks away from the outer 6 percent safe margin
Output intent: opaque bitmap source that will be imported into Unreal Engine as a non-streaming UI texture; runtime code will render all Korean copy, evidence cards, dividers, selection states, and accessibility scaling on top
```

이 이미지는 완성 UI나 추론 정보가 아니다. 런타임은 출처 카드·한글·썸네일·
페이지 수·확정된 교차선만 별도로 그리며, 이미지 안 선은 레인 정렬의 낮은
물성 기준으로만 사용한다.
