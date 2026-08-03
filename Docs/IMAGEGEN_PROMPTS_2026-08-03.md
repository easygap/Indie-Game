# ImageGen 생성 기록 — 2026-08-03

서비스는 OpenAI ImageGen을 사용했습니다. 생성 원본은
`Content/SourceArt/AI/`에 보존하고, 게임에는
`Scripts/Prepare-AIArt.ps1`이 만든 파생 파일만 사용합니다.

## P5 젖은 흔적 마스크

최초 생성:

```text
Create a square 2x2 production texture atlas for a photorealistic first-person Korean everyday-horror game set on a damp rooftop before dawn. Orthographic top-down scan-like presentation, neutral technical asset sheet, no scene perspective.

Canvas and layout:
- exactly four equal quadrants with generous internal padding
- pure matte black background (#000000) across the entire canvas
- no dividers, no borders, no captions, no symbols, no text
- every mark completely contained inside its own quadrant and never touching an edge
- all marks rendered only in physically plausible white-to-gray values on black, suitable as grayscale opacity/roughness masks
- no colored lighting, no cast shadows, no floor texture

Quadrants:
1. top-left: two consecutive partial wet sole prints from a cheap adult Korean rubber bathroom slipper, slightly different water coverage, elongated oval forefoot and shallow heel, readable direction, no shoe object
2. top-right: a short trail of four small wet domestic cat paw prints, natural alternating gait, one print slightly compressed, anatomically correct pads, no cat
3. bottom-left: a curved garden-hose compression and drag mark with a damp coupling impact crescent, irregular pooled edges, no hose object
4. bottom-right: a human palm and finger smear dragged inward across wet painted metal, incomplete fingertips, broken water film, ambiguous and unsettling but non-graphic, no blood

Style:
macro forensic photography converted into clean PBR masks; restrained, irregular, realistic water breakup; subtle specular mottling encoded as gray values; consistent scale and moisture behavior across all four cells; unsettling because it looks ordinary and physically real. No gore, no footprints outside the specified cells, no branding, no watermark.
```

## CH03 옥상 철문 모델링 기준

```text
Create a production modeling reference sheet for a grounded Korean psychological-horror indie game, showing one realistic rooftop steel fire door from an older Korean multi-family villa. 2x2 orthographic/reference layout on a neutral dark-gray studio background, no decorative border.

Canonical dimensions and physical contract: steel door leaf 116 cm wide, 230 cm high, 4.5 cm thick; welded steel frame with 120 cm clear opening and 234 cm clear height; approximately 8 cm frame members; 1.5 cm normal bottom clearance above a worn concrete-and-steel threshold. The lower hinge has visibly sagged by only 6–8 mm, so the lower free corner catches the worn threshold. When released, the leaf rests slightly ajar at exactly 5.44 degrees, producing an 11 cm horizontal opening at the free edge beside the jamb. The 11 cm is NOT a gap underneath the door.

Panel A: straight-on exterior elevation, door nearly closed, showing blue-charcoal gray powder-coated steel, subtle oxidation along seams, three practical welded hinges on the left, restrained Korean villa utility construction, plain latch/hasp mounting points but no keys and no writing.
Panel B: straight-on interior elevation, simple dark tubular pull handle, compact hydraulic closer, shallow pressed-steel reinforcement panels, believable welds and fasteners.
Panel C: clean top-down technical view of the hinge pivot and door resting 5.44 degrees ajar, clearly readable narrow free-edge slit between leaf and jamb; convey dimensions through proportional geometry only, no text, labels, arrows, numerals, or diagrams.
Panel D: close three-quarter detail of the sagged lower hinge and polished scrape mark where the free lower corner catches the threshold, grime gathered naturally in corners, no supernatural damage.

Late-2000s Korean rooftop utility architecture; damp dawn atmosphere; restrained blue-gray, oxidized brown, dirty concrete palette; realistic PBR material reference; utilitarian and slightly neglected, never gothic, fantasy, ornate, cinematic sci-fi, or post-apocalyptic. Keep all four panels visually consistent and modelable. No people, no animals, no blood, no symbols, no Korean or English text, no logos, no watermark.
```

## CH03 열린 자물쇠·관리 열쇠 모델링 기준

```text
Use case: stylized-concept
Asset type: production 3D modeling reference sheet for a story-critical rooftop management-key prop in Unreal Engine
Input image: the supplied rooftop fire-door sheet is a material, wear, lighting, and Korean-villa hardware reference only; do not copy its panel composition or add a second door.

Primary request: create one square 2x2 reference sheet of the same unlocked padlock-and-management-key assembly hanging from the free-side jamb of an older Korean multi-family villa rooftop fire door. This prop must immediately communicate that the door was already unlocked and left ajar, so the player should inspect it but never pick it up or solve a lock puzzle.

Canonical assembly:
- One ordinary 50 mm wide laminated galvanized-steel padlock body, about 28 mm deep and 62 mm tall, maintained but old.
- Its 8 mm round shackle is visibly OPEN: the right shackle leg is lifted approximately 28 mm clear of the body while the left leg remains seated. The padlock hangs loosely from a plain welded frame staple; it does not connect the door leaf to the frame.
- One plain silver management key is still fully inserted into the bottom cylinder.
- A single 42 mm split key ring hangs from that inserted key.
- Exactly three additional ordinary silver utility keys hang from the one ring: one long flat key, one shorter square-shoulder key, and one small cabinet key. Exactly four visible keys total including the inserted key; no duplicate or hidden extra key.
- One small blank oval stamped-steel inventory tag also hangs from the ring. It has no writing, number, logo, paint, color, or symbol.
- Total hanging length from top of shackle to lowest key is approximately 19 cm.
- Restrained late-summer humidity, tiny water beads, rubbed bright contact edges, sparse warm-brown oxidation only inside laminations and at the welded staple. Never heavily rusted, abandoned, broken, bent, or supernatural.

Panel layout:
1. Top-left: exact orthographic front view of the complete assembly, nothing cropped; open shackle, body, inserted key, one ring, exactly three hanging keys, and blank oval tag all separately readable.
2. Top-right: exact orthographic side view proving the 28 mm body depth, lifted open shackle, inserted bottom key, and natural hanging order.
3. Bottom-left: close three-quarter modeling detail of the open shackle hanging from the fixed frame staple; clearly show that the shackle is not locking the door leaf.
4. Bottom-right: restrained first-person gameplay-distance preview beside the same blue-charcoal rooftop frame under dim 04:42 predawn flashlight spill; the assembly is readable without glow, outline, floating UI, hand, or pickup pose.

Style and consistency:
- Grounded late-2000s Korean residential rooftop utility hardware.
- Realistic PBR product/reference photography, cool neutral-gray studio background in the first three panels.
- Match the supplied fire-door reference's blue-charcoal steel, dirty concrete, low-saturation blue-gray palette, restrained wear, and damp dawn atmosphere.
- High silhouette clarity for translation into one low-poly static mesh with a single metal material.

Constraints: exactly one open padlock, exactly one inserted key, exactly one split ring, exactly three additional keys, exactly one blank oval tag; preserve identical geometry and wear across all four panels. No closed padlock, locked hasp, loose key on the floor, duplicate ring, extra key, key card, plastic tag, colored tag, chain, combination dial, modern smart lock, hand, person, cat, blood, gore, supernatural mark, Korean or English text, numerals, captions, arrows, measurements, logos, brands, watermark, red emergency light, dramatic rust, sparks, or cinematic fantasy lighting.
```

슬리퍼 정사 수정:

```text
Edit this existing 2x2 grayscale wet-evidence mask atlas while preserving the exact square layout, pure black background, scale, moisture rendering, and the top-right cat paws, bottom-left hose drag/coupling mark, and bottom-right palm smear unchanged.

Replace only the top-left footwear marks. They must be two consecutive partial wet impressions from a cheap one-piece Korean apartment/bathroom slide slipper, not a sneaker, boot, outdoor shoe, or flip-flop. The outsole silhouette is a simple rounded rectangle / long oval with a broad flat toe and rounded heel. Use only a very shallow sparse anti-slip pattern: a few wide horizontal grooves and small circular drain dimples. No complex diamond tread, no athletic sole anatomy, no brand, no letters. The two impressions should have broken water coverage and a clear walking direction, fully contained in the top-left quadrant.

Keep all artwork white-to-gray on pure matte black and suitable as an opacity/roughness mask. No text, borders, dividers, captions, color, cast shadows, floor texture, objects, blood, branding, or watermark.
```

## CH03 환경 블렌드

최초 생성:

```text
Create a square 2x2 source decal atlas for a photorealistic first-person Korean everyday-horror game set in an aging small apartment villa and rooftop before dawn.

STRICT BACKGROUND AND LAYOUT:
- entire canvas background is one perfectly flat, solid chroma-key magenta: RGB 255, 0, 255
- exactly four equal quadrants, no separators, borders, labels, captions, text, logos, or watermark
- each decal is an isolated irregular surface deposit fully contained with wide empty magenta padding
- absolutely no wall, floor, metal plate, object, lighting scene, cast shadow, or perspective background
- only the deposit/stain itself appears over magenta, photographed straight-on / orthographic
- realistic feathered and broken edges, but avoid magenta color spill inside the deposits

Quadrants:
1. top-left: a horizontal rising-damp tide line stripped from old warm-ivory Korean vinyl wallpaper, gray-green moisture bloom with faint tea-brown edges and sparse black mildew specks, about three times wider than tall, restrained and believable
2. top-right: rust blooms and narrow rain drips associated with two old Phillips screws plus one empty missing-screw hole; include the rusty screw heads and dark circular empty hole as part of the isolated decal, subdued orange-brown, no plate behind them
3. bottom-left: chalky white and pale gray mineral-scale ring fragments with thin rusty water streaks from a rooftop water-tank access rim, irregular broken circumference, no tank or lid
4. bottom-right: long vertical soot-gray rain and grime streaks from a concrete stairwell wall, several fine overlapping drips with one darker damp edge, no concrete background

ART DIRECTION:
Korean low-rise villa, late July humidity, 04:44 blue-hour horror realism, ordinary maintenance neglect rather than fantasy; subdued saturation; physically plausible wetness, corrosion, mineral scale and grime; macro forensic texture quality, production-ready decal source. No gore, no blood, no supernatural symbols, no decorative horror tropes, no readable writing.
```

빗물 때 키잉 수정:

```text
Edit this existing square 2x2 chroma-key surface decal atlas. Preserve the canvas size, exact 2x2 layout, solid RGB 255,0,255 background, and the top-left damp wallpaper deposit, top-right rusted fasteners, and bottom-left mineral scale unchanged.

Replace only the bottom-right rain/grime decal. It must be a clearly opaque neutral soot-gray to charcoal deposit with many physically separate vertical water streaks, fine drips, and a few broader damp runs. The actual streak pixels must contain no magenta, purple, violet, pink, blue, or colored fringe at any point; use strictly neutral grayscale RGB values where R=G=B, ranging about 45 to 150. Give every streak a crisp but naturally broken boundary directly against the solid chroma magenta background, as if it were a clean game decal extraction source. Keep the mark fully contained in the bottom-right quadrant with generous empty padding and make it occupy roughly 70 percent of that cell.

No scene, wall, concrete, floor, perspective, cast shadows, border, divider, text, labels, logos, symbols, people, gore, branding, or watermark.
```

## 증거 소품 모델링 기준

```text
Create a square 2x2 photorealistic hard-surface prop reference sheet for a first-person Korean everyday-horror game. The props belong to the same aging low-rise apartment rooftop at 04:44 before dawn. Studio-neutral orthographic-ish product reference, physically plausible dimensions, subdued color and wear, production concept suitable for procedural 3D modeling.

LAYOUT:
- exactly four equal quadrants, each with one complete isolated prop
- uniform very light neutral gray background, soft contact shadow only
- consistent cool overcast illumination from upper left, no cinematic colored rim light
- each prop shown in a clear three-quarter view with its silhouette unobstructed
- no captions, text, labels, measurements, logos, branding, people, hands, gore, blood, watermark, or dramatic scene background

QUADRANTS:
1. top-left: ordinary adult black acetate horn-rimmed eyeglasses from mid-2010s Korea, slightly thick rectangular rounded frames, temples open, one lens missing and the remaining lens wet with droplets, minor scuffs, not fashionable or luxurious
2. top-right: simple rooftop water-tank inspection/support rod, 118 cm galvanized steel round bar, slightly bent near one end, flattened hooked tip and a small rubber grip, realistic rust freckles and wetness, no fantasy weapon styling
3. bottom-left: inexpensive black 2018-era Android smartphone in a plain matte protective case, screen dark, two realistic diagonal cracks and a chipped corner, moisture beads, no readable UI and no logo
4. bottom-right: thin translucent white Korean convenience-store carry bag lying partly collapsed, narrow fused handles, faint generic pale-blue safety/recycling print shapes with no readable letters, wet creases, sized for two water bottles, no brand

ART DIRECTION:
quiet Korean domestic realism, late-July humidity, mundane evidence rather than horror decoration; restrained wear shared across all props; PBR-ready material detail; believable molded plastic, galvanized metal, glass, and thin polyethylene. Avoid stylization, illustration, product glamour, exaggerated damage, supernatural motifs, grime overload, or vintage historical styling.
```

## 골목 고양이 포즈 기준

```text
Create a production modeling reference sheet for a realistic Korean alley cat used in a grounded first-person psychological horror game.

SUBJECT
- One and the same small adult mackerel-tabby street cat in every panel.
- Short gray-brown coat, charcoal narrow stripes, pale muzzle and throat, amber eyes, intact ears with one tiny old notch on the right ear, lean but healthy body.
- Ordinary recognizable Korean neighborhood stray; sympathetic and cautious, never monstrous, supernatural, aggressive, cute mascot-like, or stylized.

LAYOUT
- Square 2x2 contact sheet, four equal panels with thin neutral divider lines.
- Panel 1: exact left side orthographic silhouette in a low cautious run, all four leg masses readable, tail held slightly low.
- Panel 2: exact front three-quarter view of the same running pose.
- Panel 3: exact left side standing pose, neutral anatomy and proportions.
- Panel 4: rear three-quarter view stepping away, same stripe pattern and right-ear notch visible.
- Keep anatomy, scale, coat pattern, eye color, and body proportions strictly identical across all panels.

STYLE / LIGHTING
- Photoreal animal anatomy and fur, restrained documentary realism.
- Neutral cool-gray seamless studio background, soft overcast key light, faint contact shadow only.
- High silhouette clarity suitable for translating into a low-poly static mesh seen for 0.9–1.35 seconds at pre-dawn.
- No cinematic scene, alley props, collar, text, labels, measurements, logo, watermark, border decoration, blood, injury, fantasy traits, glowing eyes, or extra animals.
- No cropped paws, tail, or ears.
```

## 젖은 후드 원단

```text
Generate one square, seamless, tileable PBR base-color texture for wet charcoal-black cotton hoodie fabric in a grounded realistic psychological horror game.

CONTENT
- Macro close-up of plain medium-weight cotton jersey/fleece knit from an inexpensive dark hoodie.
- Nearly black charcoal dye, very subtle cool blue-gray cast.
- Fabric is fully soaked: compressed nap, irregular darker water saturation, a few broad soft wrinkles and tiny beads caught between fibers.
- Worn domestic clothing, not leather, rubber, denim, satin, tactical fabric, or body armor.
- Low-contrast surface detail that will remain believable on curved human proxy geometry under dim flashlight and through water.
- No seams, stitches, cuffs, zipper, drawstrings, logos, text, blood, dirt clumps, holes, skin, body shape, or objects.

TEXTURE RULES
- Perfectly top-down orthographic material scan.
- Edge-to-edge fabric with no background, horizon, vignette, frame, perspective, directional studio hotspot, or cast shadow.
- Seamless on all four edges with no obvious repeated central motif.
- Physically plausible subdued albedo only: retain fiber color and wet variation, no baked specular highlights, no normal-map purple, no grayscale height-map presentation.
- Photoreal, production texture source, even diffuse cross-polarized lighting, high micro-detail.
```

## 편의점 봉지 박막

```text
Generate one square, seamless, tileable material texture for a thin translucent Korean convenience-store carrier bag used as forensic evidence in a grounded realistic psychological horror game.

CONTENT
- Clear milky low-density polyethylene film seen perfectly flat and top-down.
- Faint cool gray-white plastic with sparse pale desaturated blue generic diagonal micro-stripes and a few tiny abstract square marks; no store name, no logo, no readable lettering, no real brand.
- Irregular soft crinkles, stretched zones, folded stress lines, and slightly cloudy rubbed patches typical of a cheap bag that carried water bottles and was set on wet concrete.
- Restrained visual contrast so bottle silhouettes remain visible through the film.
- Clean enough to match a normal 2018 Korean neighborhood purchase, with only subtle damp handling wear.

TEXTURE RULES
- Edge-to-edge flat plastic film, no bag silhouette, handles, bottle, props, background, horizon, border, frame, perspective, cast shadow, or isolated object.
- Perfectly seamless on all four edges without a central motif.
- Production base-color/opacity-source appearance under even diffuse cross-polarized lighting; no baked bright specular hotspot, no fake transparency checkerboard, no black background, no magenta or green chroma screen.
- Photoreal material scan, fine film micro-detail, cool neutral palette consistent with dim blue-gray pre-dawn horror lighting.
```

## 고등어태비 털 알베도

```text
Generate one square, seamless, tileable photoreal fur base-color texture for the exact same small adult mackerel-tabby Korean alley cat described here: short gray-brown coat, narrow charcoal vertical mackerel stripes, a little warm tan between stripes, lean ordinary street cat.

TEXTURE CONTENT
- Macro coat pattern appropriate for the torso and tail of one realistic cat.
- Short dense guard hairs over softer underfur, not long-haired.
- Narrow organic charcoal stripes with irregular natural edges; stripe spacing about 4–7 cm at real scale.
- Subdued pre-dawn neutral gray-brown palette, readable but not high contrast.
- Clean healthy fur with tiny natural variation, no wounds, bald patches, blood, mud clumps, fleas, collar, skin, face, paws, eyes, ears, body silhouette, or objects.

TEXTURE RULES
- Perfectly top-down orthographic material scan.
- Edge-to-edge fur surface, no background, horizon, vignette, frame, perspective, cast shadow, or central composition.
- Seamless across all four edges, preserving stripe continuity without an obvious repeating tile.
- Even diffuse cross-polarized lighting; albedo only, no baked specular glare, no purple normal-map presentation, no grayscale height-map presentation.
- Photoreal production texture source suitable for a low-poly static cat visible briefly in dim psychological horror lighting.
```

## CH03 수중 인체 포즈 기준

```text
Create a square 2x2 production pose reference sheet for a non-graphic, fully clothed adult human silhouette used in the final reveal of a grounded Korean psychological horror game.

SUBJECT AND CANONICAL OUTFIT
- One and the same 176 cm slim adult Korean male-proportioned figure in every panel.
- Face and skin are never visible.
- Soaked medium-gray zip hoodie over a white T-shirt, black loose training pants, generic black Korean apartment slide slippers with three plain off-white bars.
- The hoodie hood is up and flattened by water. Left sleeve has exactly three small black repair stitches on the inner forearm, subtle but geometrically locatable.
- Left slipper outer heel is visibly worn; right slipper remains on the other foot but is mostly occluded.

CANONICAL POSE
- Back-facing curled side/fetal posture, shoulders rounded, chin tucked away, both arms folded inward near chest.
- Left shoulder slightly higher and restricted; both knees bent toward torso.
- Body is compact and unmistakably human but quiet, not theatrical.
- No exposed face, hands, feet, skin, wounds, blood, decomposition, swelling, gore, medical equipment, rope, restraints, or supernatural deformation.

LAYOUT
- Exactly four equal panels with thin neutral divider lines.
- Top-left: exact orthographic top view of the complete curled pose.
- Top-right: exact orthographic left-side view, clothing silhouette and bent knees readable.
- Bottom-left: rear three-quarter view, hood, shoulders, folded arms, pants, both slippers and worn left heel readable.
- Bottom-right: the same rear three-quarter pose seen just beneath still dark water, only subdued refraction and cloth compression added; preserve exact pose and proportions.
- Keep body proportions, clothing folds, slipper placement, and pose strictly identical in all panels.

STYLE
- Restrained photoreal production reference, ordinary inexpensive 2024 Korean clothing.
- Cool neutral-gray studio background in the first three panels, soft broad overhead light, faint contact shadow only.
- Final panel uses muted blue-gray water with realistic low contrast, no cinematic red light.
- No scene, tank, ladder, props, typography, captions, measurement labels, logos, brands, watermark, dramatic spotlight, jump-scare face, or decorative horror styling.
```

## CH03 물탱크 아연도금 강판

```text
Generate one square, seamless, tileable PBR base-color texture for the galvanized steel shell, service hatch, ladder, and pipework of an ordinary aging Korean villa rooftop water tank in a grounded psychological horror game.

MATERIAL CONTENT
- Plain cool blue-gray galvanized steel with a very subtle fine spangle pattern and muted rolled-sheet grain.
- Late-July pre-dawn dampness expressed only as irregular darker moisture blooms, thin vertical runoff traces, sparse pale mineral freckles, and a few restrained warm-brown oxidation pinpoints around imagined fastener areas.
- Maintained but old residential utility equipment from the late 2010s, not abandoned industrial ruins.
- Low-contrast enough that separate rust-fastener and mineral-scale evidence decals remain readable on top.

TEXTURE RULES
- Perfectly top-down orthographic material scan, edge-to-edge metal only.
- Seamless on all four edges with no central composition and no obvious repeated large stain.
- Even diffuse cross-polarized lighting; base color only.
- No baked specular hotspot, reflections, shadows, normal-map purple, grayscale height-map presentation, perspective, horizon, frame, border, tank silhouette, hatch, ladder, screws, bolts, seams, holes, labels, Korean text, logos, brands, blood, handprints, supernatural symbols, or objects.
- Photoreal production texture source, restrained cool neutral palette consistent with the project's wet charcoal fabric, translucent pale-blue carrier film, gray-brown alley cat, and dim blue-gray flashlight lighting.
```

## P5 사고 프롭 기준

```text
Use case: stylized-concept
Asset type: photoreal production modeling reference sheet for two story-critical game props
Primary request: create a square 2x2 reference sheet for the exact ordinary rooftop service hose and Korean convenience-store carrier bag used in a grounded psychological horror accident reconstruction.

SHARED STYLE
- Restrained photoreal product/reference photography, late-2010s Korean residential utility realism.
- Cool neutral-gray seamless studio background, broad soft overhead light, faint contact shadow only.
- Every object is wet from light summer rain but maintained and mundane, never abandoned-horror decoration.
- Four equal panels with thin neutral divider lines. No captions, measurements, arrows, typography, logos, brands, watermark, people, hands, feet, cat, blood, gore, supernatural symbols, dramatic red light, or scene clutter.

PROP A — SERVICE HOSE
- One continuous 42 mm outside-diameter charcoal-black EPDM rooftop service hose, about 4.5 m visible length.
- Ordinary reinforced rubber with subdued woven grain, gentle realistic bends, small wet beads, sparse shallow abrasion.
- Lower section contains one localized oval compression where a small cat paw briefly pressed it: rubber slightly flattened but not cut, punctured, torn, or clawed.
- Upper end has one dull galvanized-steel quick coupling, about 65 mm wide, with a restrained wet contact ring and a tiny scuff from striking metal.
- No garden nozzle, spray head, faucet, reel, decorative color stripe, branding, or fantasy cable styling.

PROP B — CARRIER BAG
- One thin translucent milky-white Korean convenience-store LDPE carrier bag sized for two 500 mL water bottles.
- Narrow fused loop handles, bottom gusset, soft collapsed walls, faint generic pale-blue diagonal micro-stripes with no readable letters.
- Damp realistic creases and stretched zones; bottle silhouettes must remain visible through the film.
- No real logo, store name, receipt, food, trash, blood, dirt pile, knots, tears, or opaque plastic.

PANEL LAYOUT
1. Top-left: exact orthographic top view of the complete hose route in a loose rising S-curve, lower compressed segment and upper coupling both fully visible, nothing cropped.
2. Top-right: close three-quarter modeling detail of the same upper coupling joined to the hose, showing collar, sleeve, restrained impact scuff, and wet contact ring.
3. Bottom-left: exact front three-quarter view of the empty carrier bag partly collapsed but standing enough to read both handles, gusset, thin side walls, and material folds.
4. Bottom-right: the same bag lying naturally on one side with exactly two plain clear 500 mL water bottles settled inside, one visibly lower water level, no duplicated bag shell, no floating bottles.

Constraints: keep hose diameter, coupling design, bag proportions, handle shape, blue micro-stripe pattern, material wear, and lighting consistent across panels; high silhouette clarity suitable for translation into low-poly static meshes; no cropped object.
```

## P5 젖은 EPDM 호스 알베도

```text
Use case: stylized-concept
Asset type: seamless tileable game material texture
Primary request: generate one square photoreal PBR base-color texture for the exact charcoal-black EPDM reinforced rooftop service hose shown in the accident-prop reference sheet of a grounded Korean psychological horror game.

MATERIAL CONTENT
- Inexpensive late-2010s utility hose rubber, nearly black charcoal with a very subtle cool blue-gray cast.
- Fine restrained cross-woven reinforcement impression under the rubber skin, shallow lengthwise extrusion lines, tiny manufacturing variation, and sparse soft abrasion.
- Light summer-rain wetness shown as irregular darker saturation and a few tiny embedded droplets, but no broad white highlights.
- Low contrast so the separate cat-paw compression mask and galvanized coupling remain the readable evidence.

TEXTURE RULES
- Perfectly top-down orthographic material scan, edge-to-edge rubber only.
- Seamless across all four edges without a central feature or obvious repeated stain.
- Even diffuse cross-polarized lighting; physically plausible base color only.
- No baked specular hotspot, reflection, cast shadow, perspective, horizon, frame, border, tube silhouette, coupling, paw print, claw mark, cut, hole, dirt clump, mud, blood, skin, text, stripe, logo, brand, watermark, normal-map purple, or grayscale height-map presentation.
- Photoreal production texture source consistent with the project's wet charcoal hoodie, cool blue-gray galvanized tank, translucent pale-blue carrier film, and dim pre-dawn flashlight lighting.
```

## P5 상단 발판 파손 기준

이 시트의 아연도금·젖은 리브 고무·클립 부식 표현만 유지한다. 136cm 수직
발판 비례는 아래 `CH03 외부 점검 계단 기준`으로 대체됐으며 런타임 치수로
사용하지 않는다.

```text
Use case: stylized-concept
Asset type: photoreal production modeling reference sheet for a story-critical rooftop ladder failure cluster
Primary request: create a square 2x2 reference sheet of one and the same ordinary galvanized service-ladder top rung, anti-slip rubber pad, and two retaining clips used in a grounded Korean psychological horror accident reconstruction.

CANONICAL ASSEMBLY
- One 136 cm wide galvanized-steel ladder rung with a shallow rounded-rectangle cross-section approximately 10 cm deep and 7 cm high, matching an old residential rooftop water-tank service ladder.
- Centered on the upper face is one 54 cm wide, 8.5 cm deep, 3 mm thick charcoal-black molded rubber anti-slip pad with narrow parallel traction ribs.
- The pad is still attached but its inward long edge has lifted and curled upward by only 8–10 mm after rain; it is not torn, folded in half, detached, or dramatically deformed.
- Exactly two small galvanized retaining clips sit at the two lateral ends of the pad. Both clips show matching restrained warm-brown corrosion at their screw bends; no clip is missing.
- A single generic black Korean apartment slide-slipper wet transfer ends on the pad, partial and directionally readable, but no shoe or foot is present.
- Recent rain: subdued water beads, one thin wet sheen, sparse runoff. Maintained late-2010s residential hardware, not abandoned industrial ruin.

LAYOUT
- Exactly four equal panels with thin neutral divider lines.
- Top-left: exact orthographic top view of the full 136 cm rung; complete rung, centered pad, partial wet transfer, and both retaining clips visible, nothing cropped.
- Top-right: exact orthographic side view across the rung depth, clearly showing the pad's inward edge lifted by 8–10 mm while the outer edge remains seated.
- Bottom-left: close three-quarter modeling detail of one retaining clip, pad end, screw bend, restrained matching corrosion, and wet junction.
- Bottom-right: rear three-quarter view of the complete same assembly under dim cool flashlight-like light, preserving exact dimensions and damage state.

STYLE
- Restrained photoreal product/reference photography on a cool neutral-gray seamless studio background.
- Broad soft overhead light for the first three panels; final panel uses muted blue-gray low light without red accents.
- High silhouette and material clarity for translation into low-poly static meshes.
- Keep rung geometry, pad rib spacing, lift amount, clip count/location, corrosion pattern, and wet transfer identical across panels.

Constraints: no ladder rails, staircase, roof scene, tank, hose, bag, person, hand, foot, cat, blood, gore, supernatural marks, broken metal, missing fastener, warning stripe, labels, measurements, arrows, text, logos, brands, watermark, dramatic sparks, or excessive rust.
```

## P5 젖은 발판 패드 고무 알베도

```text
Use case: stylized-concept
Asset type: seamless tileable game material texture
Primary request: generate one square photoreal PBR base-color texture for the charcoal-black molded rubber anti-slip pad on an ordinary Korean rooftop service-ladder rung in a grounded psychological horror game.

MATERIAL CONTENT
- Inexpensive dense molded rubber, nearly black charcoal with a restrained cool-gray cast.
- Narrow straight parallel traction ribs running vertically through the texture, evenly spaced at realistic small scale.
- Fine molded grain between ribs, slight edge-independent compression polish, tiny scuffs, and sparse late-summer rain droplets.
- Wetness remains low contrast and dark; no broad white reflection or mirror shine.
- Maintained but old late-2010s residential utility hardware, not rotten or abandoned.

TEXTURE RULES
- Perfectly top-down orthographic material scan, edge-to-edge ribbed rubber only.
- Seamless across all four edges; every rib must continue cleanly across the top and bottom edges with no central composition.
- Even diffuse cross-polarized lighting; physically plausible base color only.
- No pad outline, lifted edge, metal rung, retaining clip, screw, footprint, slipper, hose weave, cut, tear, hole, dirt clump, mud, rust, blood, skin, text, stripe color, logo, brand, watermark, baked specular hotspot, reflection, cast shadow, perspective, horizon, frame, border, normal-map purple, or grayscale height-map presentation.
- Photoreal production texture source consistent with the project's dark wet EPDM hose, cool blue-gray galvanized tank, wet charcoal hoodie, and dim pre-dawn flashlight lighting.
```

## CH03 탱크 수면·굴절 알베도

```text
Use case: stylized-concept
Asset type: seamless tileable game material texture for a translucent Unreal Engine water surface
Primary request: generate one square photoreal PBR base-color source texture for the still interior water of an ordinary aging Korean villa rooftop water tank in a grounded psychological horror game.

MATERIAL CONTENT
- Very dark cool blue-gray water with a restrained green-gray cast, consistent with a maintained late-2010s residential storage tank before dawn.
- Low-amplitude overlapping surface undulations and broad shallow interference arcs, as if a metal service lid was just lifted and the water is settling.
- Subtle irregular luminance variation suitable for driving weak per-pixel refraction; changes must remain smooth and low contrast.
- Sparse tiny pale mineral flecks and a few soft suspended specks, never enough to imply contaminated sludge.
- The surface is mostly still and heavy, with no dramatic waves, splash, foam, bubbles, or visible current.

TEXTURE RULES
- Perfectly top-down orthographic material scan, edge-to-edge water only.
- Seamless across all four edges with no central focal ripple, no obvious repeating ring, and no large isolated feature.
- Even diffuse cross-polarized lighting; physically plausible dark base color source.
- No baked white specular hotspot, mirror reflection, reflected sky, reflected lamp, flashlight beam, caustic projection, cast shadow, perspective, horizon, frame, border, tank wall, hatch, ladder, metal, clothing, human silhouette, face, skin, hair, hand, foot, blood, gore, algae mat, trash, insect, supernatural symbol, text, logo, brand, watermark, normal-map purple, or grayscale height-map presentation.
- Restrained photoreal production texture consistent with the project's charcoal wet hoodie, cool galvanized tank, black EPDM hose, ribbed rung pad, and dim blue-gray flashlight lighting.
- Keep the water dark enough that the submerged clothing becomes readable only through runtime flashlight, opacity, and refraction settings rather than being painted into this image.
```

## 1인칭 왼쪽 후드 소매 기준

```text
Use case: stylized-concept
Asset type: photoreal production modeling reference sheet for a first-person static game prop
Primary request: create a square 2x2 reference sheet of one and the same ordinary left forearm sleeve from a charcoal-gray Korean zip hoodie, used as a story-critical identity clue in a grounded psychological horror game.

CANONICAL SLEEVE
- One adult left hoodie sleeve only, approximately 36 cm visible length from cropped lower elbow to cuff.
- Loose inexpensive cotton-poly fleece, charcoal gray with a subdued cool cast, slightly damp and compressed but not dripping or glossy.
- Taper from about 13.5 cm outer width near the cropped elbow to a 10.5 cm rib-knit cuff.
- Natural shallow forearm bend and restrained longitudinal folds; the silhouette must read as cloth around an unseen arm, not a straight metal pipe.
- On the inner forearm, about 6 cm above the cuff, exactly three short parallel hand-repair bar stitches in matte black thread. Each stitch is about 12 mm long, evenly spaced by about 9 mm, crossing one small split seam. The three stitches are distinct geometry references, not painted stripes.
- No hand, fingers, wrist, skin, bone, blood, injury, jewelry, watch, glove, zipper, pocket, logo, brand, text, or emblem. The cuff opening is filled only by deep neutral cloth shadow so no body part is exposed.

LAYOUT
- Exactly four equal panels with thin neutral divider lines.
- Top-left: orthographic inner-side view of the complete sleeve, fully visible, showing all three black stitches and the small repaired seam.
- Top-right: orthographic outer-side view of the same sleeve, preserving identical taper, cuff, folds, and dampness.
- Bottom-left: close three-quarter modeling detail of the cuff and repair area; exactly three stitches, correct spacing, no extra thread.
- Bottom-right: first-person camera preview at a 78-degree horizontal field of view, sleeve rising briefly from the lower-left/lower-center of frame as during a 1.2-second non-blocking clothing presentation; stitch area readable for roughly half a second without a zoom or spotlight.

STYLE
- Restrained photoreal product/reference photography on a cool neutral-gray seamless studio background.
- Broad soft overhead light for the first three panels; final panel uses dim blue-gray apartment light.
- High silhouette and material clarity suitable for translation into a low-poly static mesh sharing the project's wet hoodie material.
- Keep sleeve length, taper, cuff width, fold layout, repaired seam location, and exactly three stitches identical across all panels.

Constraints: no full person, torso, head, face, second arm, right sleeve, hand, wrist, skin, mannequin hand, hanger, hook, room furniture, tank, water surface, dramatic horror lighting, red light, supernatural marks, captions, measurements, arrows, typography, logos, brands, or watermark.
```

## P3 급수 서비스함 모델링 기준

```text
Use case: stylized-concept
Asset type: photoreal production modeling reference sheet for a story-critical interactive water-service cabinet in Unreal Engine
Primary request: create a square 2x2 reference sheet of one and the same open water-supply service cabinet installed in the concrete stair corridor of a maintained late-2010s Korean multi-family villa, used for a pressure-release sequence in a grounded psychological horror game.

CANONICAL CABINET
- A wide recessed double-door utility manifold cabinet, exactly 270 cm wide, 196 cm high, and 18 cm deep.
- Dull pale gray-green painted galvanized steel, maintained but old, with restrained rain humidity, fine scratches at hand height, thin mineral streaks below pipe joints, and sparse warm-brown edge corrosion. It must not look abandoned, industrial-factory-scale, or post-apocalyptic.
- Both steel doors are already open and remain physically attached: left leaf opened about 100 degrees toward the left wall, right leaf opened about 105 degrees toward the right wall. The player must see the complete manifold in one frontal view.
- The right-side latch is bent downward by about 12 degrees. Exactly two latch mounting screws are absent, leaving exactly two small clean-edged empty screw holes with restrained rust halos. No loose screw is present inside the cabinet.
- Inside are four clearly separated real controls connected by ordinary galvanized and dark-gray water pipes:
  1. upper-left direct inlet: one 18 cm dark muted red cast-metal handwheel;
  2. upper-middle reserve-tank inlet: one 18 cm dark muted blue cast-metal handwheel;
  3. lower-middle pressure release: one 12 cm charcoal handwheel feeding a transparent short bleed tube;
  4. lower-right floor drain: one 18 cm dark galvanized handwheel.
- One 16 cm round analog pressure gauge sits above the lower controls, white aged dial, black ticks, thin dark red needle, metal bezel, physically connected to the manifold. Do not render readable numbers or lettering on the dial.
- Pipe outside diameters are approximately 34 mm for the two main inlets and 25 mm for release/drain branches. Use proper elbows, unions, wall clamps, and one shallow catch tray under the bleed tube.
- Four blank narrow off-white metal tag plates sit near the controls. They contain no text because Korean labels will be rendered at runtime.
- All controls must remain reachable and visually distinct at standing first-person eye height. No control is hidden by a door or pipe.

LAYOUT
- Exactly four equal panels with thin neutral divider lines.
- Top-left: exact orthographic frontal view of the complete cabinet, both open doors fully visible, all four controls, gauge, pipe routing, broken latch, and exactly two empty screw holes readable, nothing cropped.
- Top-right: shallow right three-quarter view showing the 18 cm recess depth, right door angle, bent latch, the two empty screw holes, pipe relief, and cabinet mounting.
- Bottom-left: close modeling detail of the pressure gauge, 12 cm pressure-release wheel, transparent bleed tube, catch tray, blank tag plate, pipe unions, and restrained wet mineral residue.
- Bottom-right: first-person gameplay-distance preview under a dim cool flashlight and weak green-gray corridor spill, showing a natural readable control hierarchy without a glowing outline or floating UI.

STYLE
- Restrained photoreal product/reference photography and architectural documentation, cool neutral-gray background for the first three panels.
- Grounded Korean residential maintenance hardware, realistic human scale, believable late-summer humidity.
- Broad soft overhead light for modeling views; bottom-right uses dim blue-gray horror-game lighting without theatrical red light.
- High silhouette and material clarity suitable for translation into several low-poly static meshes while preserving interactive valve rotation.

Constraints: keep cabinet dimensions, pipe routing, wheel sizes/colors/positions, gauge size, door angles, latch bend, exactly two empty screw holes, wear pattern, and control visibility identical across all panels; no person, hand, body, face, blood, gore, monster, supernatural symbol, padlock, combination puzzle, exposed electrical wires, electrical breaker, boiler flame, tank, ladder, cat, bottle, bag, printed Korean, English, numbers, captions, measurements, arrows, text, logos, brands, watermark, glowing control, red emergency lighting, heavy rust, broken pipe, leaking jet, flooding, or dramatic smoke.
```

## P3 회녹색 도장강판 알베도

```text
Use case: stylized-concept
Asset type: seamless tileable game material texture for Unreal Engine static meshes
Primary request: generate one square photoreal PBR base-color source texture for the dull pale gray-green painted galvanized steel of an open water-service cabinet in a maintained late-2010s Korean multi-family villa.

MATERIAL CONTENT
- Old factory-applied pale gray-green utility enamel over galvanized sheet steel, subdued and slightly cool.
- Fine orange-peel paint texture, tiny shallow scratches from maintenance tools, sparse pinhead chips exposing dark gray zinc, faint vertical humidity streaks, and extremely restrained warm-brown oxidation only at a few chip edges.
- Slight late-summer dampness expressed as irregular darker saturation, never a glossy mirror coat.
- Maintained residential hardware: aged and handled, but not abandoned, rotten, burned, or heavily corroded.
- Low contrast so separate valve colors, blank tag plates, pressure gauge, two missing-screw holes, and runtime flashlight remain readable.

TEXTURE RULES
- Perfectly front-facing orthographic material scan, edge-to-edge painted metal only.
- Seamless across all four edges with no central composition, border, panel outline, hinge, corner, seam, latch, screw, screw hole, rivet, pipe, valve, gauge, label plate, door shadow, or isolated hero scratch.
- Even diffuse cross-polarized studio lighting; physically plausible base color only.
- No baked specular hotspot, reflection, cast shadow, perspective, horizon, frame, text, numbers, arrows, Korean, English, logo, brand, watermark, blood, handprint, supernatural symbol, dramatic rust patch, peeling sheet, dent, hole, welding bead, normal-map purple, grayscale height-map presentation, or color checker.
- Restrained photoreal production texture consistent with the project's cool blue-gray galvanized water tank, dark wet EPDM hose, wet charcoal hoodie, dim concrete stairwell, and blue-gray flashlight lighting.
```

## CH03 옥상 물탱크 외피·배관 기준

참조 입력은 `TextureWaterTankGalvanized.png`와
`SheetLadderRungFailureReference.png` 두 장이다.

```text
Use the attached galvanized-metal texture and rooftop hardware sheet only as visual references for material age, corrosion density, blue-gray color, and late-2010s Korean villa construction language.

Create one production modeling reference sheet for a maintained cylindrical galvanized rooftop water tank in a grounded Korean psychological-horror game. Exactly four clean orthographic-style panels in a 2x2 grid: front elevation, side elevation, top view, and a three-quarter cutaway view. The same single tank must be depicted consistently in every panel.

Canonical geometry and proportions:
- outer tank diameter 306 cm
- cylindrical shell height 260 cm
- 16 subtle vertical folded/faceted wall panels
- three shallow reinforcement hoops positioned near 12 cm, 130 cm, and 248 cm above the shell bottom
- top service deck diameter 314 cm, 8 cm thick
- centered circular access opening diameter 104 cm
- separate slightly domed access lid diameter 108 cm, 6 cm thick, with simple crossed stiffening ribs, two hinge ears, and a low rectangular handle
- interior floor is 41 cm above the exterior shell bottom
- access-deck underside is 256 cm above the exterior shell bottom
- residual water depth is 180 cm, leaving the water surface exactly 35 cm below the access-deck underside
- compact realistic plumbing: 89 mm vertical inlet pipe, 76 mm horizontal branch, restrained clamps and flange, 18 cm hand wheel; avoid oversized industrial pipes

Visual direction: utilitarian late-2010s Korean multi-family villa rooftop equipment, blue-gray hot-dip galvanized steel, sparse orange-brown rust at seams and bolt heads, faint mineral streaking below the lid, slightly uneven predawn fluorescent-blue illumination, believable fabrication, restrained use and weathering, no ruin or fantasy styling. The cutaway panel should clearly communicate the inner floor and water proportions through geometry and water tone, without labels.

Presentation: neutral dark charcoal studio background, strong readable silhouettes, near-orthographic camera, consistent scale, crisp hard-surface modeling reference, no dramatic perspective, no text, no letters, no numbers, no arrows, no dimension lines, no logos, no people, no body, no blood, no gore, no loose unrelated props, no extra tanks.
```

## CH03 점검구 난간·U볼트·내부 사다리 기준

참조 입력은 `SheetRooftopWaterTankReference.png`, `SheetEvidenceProps.png`,
`SheetLadderRungFailureReference.png` 세 장이다.

```text
Use case: stylized-concept
Asset type: production modeling reference sheet for two story-critical rooftop water-tank safety assemblies
Input images: Image 1 is the canonical Korean-villa galvanized water tank and material/wear reference; Image 2 is the canonical ordinary black horn-rim glasses reference; Image 3 is the canonical galvanized ladder hardware and restrained corrosion reference.

Primary request: Create one square 2x2 hard-surface modeling reference sheet showing the external access-platform guardrail/U-bolt and the internal tank ladder for the same maintained late-2010s Korean villa rooftop reserve tank. The geometry, scale, metal age, and attachment logic must be consistent across every panel.

CANONICAL EXTERNAL GUARDRAIL
- A compact exposed service platform beside the tank hatch, about 80 cm usable length and 110 cm width.
- Two side guardrails made from 42 mm galvanized round tube, 75 cm above the platform, each with one mid rail and two welded vertical posts.
- The approach side remains completely open; no front crossbar blocks the person climbing onto the platform.
- Tank-side ends terminate against ordinary welded mounting plates without intersecting the curved shell.
- Exactly one small galvanized U-bolt and rectangular clamp plate on the right-hand side rail connection.
- Exactly one pair of ordinary wet black horn-rim glasses hangs from that U-bolt by one temple. The left nose pad is slightly bent. The glasses are evidence, not decoration: no second pair, no case, no loose lenses.

CANONICAL INTERNAL LADDER
- Fixed to the inside cylindrical wall directly below the service hatch.
- Rail center spacing 46 cm, 34 mm round galvanized rails, 25 mm round rungs, seven rungs at 30 cm vertical spacing.
- Rails span about 210 cm from the raised internal floor to just below the access-deck underside.
- Four short 20 cm wall stand-offs with welded mounting feet keep the ladder clear of the curved shell.
- The ladder remains visibly reachable from the hatch but is a restrained residential maintenance fitting, not an oversized industrial cage ladder.
- Dark mineral wetting below the waterline and sparse warm-brown oxidation only at welded feet and fasteners.

PANEL LAYOUT
1. Top-left: exact orthographic side elevation of the complete external platform guardrail beside the curved tank shell, open approach and all posts readable.
2. Top-right: close three-quarter modeling detail of the right-hand rail connection, exactly one U-bolt and plate, with exactly one pair of glasses hanging naturally by one temple.
3. Bottom-left: exact orthographic cutaway elevation of the internal ladder from raised inner floor to hatch underside, all seven rungs and four wall stand-offs visible.
4. Bottom-right: first-person view looking down through the open hatch into the empty tank, showing the internal ladder edge descending through dark blue-gray residual water and the guardrail at frame edge; this panel is for lighting and silhouette only.

Visual direction: grounded Korean residential utility realism, cool blue-gray hot-dip galvanized steel matching Image 1, restrained sparse corrosion matching Image 3, late-July predawn dampness, believable human scale, crisp fabrication, subdued flashlight response, psychological horror through ordinary emptiness rather than damage.

Constraints: same assembly dimensions and attachment positions across panels; no text, letters, numbers, arrows, labels, dimension lines, logo, brand, watermark, person, body, face, skin, hand, foot, blood, gore, monster, supernatural symbol, extra glasses, extra U-bolt, padlock, rope, cage, safety harness, giant industrial stair, floating hardware, broken ladder, detached rail, heavy rust, dramatic red light, fantasy machinery, or unrelated props.
```

구현 검토에서 정중앙 점검구는 외부 사고 단과 2m 이상 떨어지는 것으로
확인됐다. 위 프롬프트와 물탱크 기준 시트의 중앙 개구부는 재질·형상 참고로만
남기며, 런타임 정사는 탱크 중심에서 서쪽으로 96cm 이동한 개구부다. 정확한
오프셋·동쪽 경첩 회전·내부 사다리 X -125cm는 절차 메시와 물리 계약을 따른다.

## CH03 외부 점검 계단 기준

참조 입력은 `SheetRooftopWaterTankReference.png`,
`SheetLadderRungFailureReference.png`,
`SheetTankAccessSafetyHardwareReference.png` 세 장이다. 생성 결과는
`Content/SourceArt/AI/SheetTankExteriorAccessStairReference.png`에 보존한다.

```text
Create a production reference sheet for a grounded late-2010s South Korean villa rooftop water-tank exterior access stair. Match the galvanized zinc, dark wet rubber, restrained corrosion, neutral studio lighting, realistic scale, and sober psychological-horror tone of the three reference sheets.

The structure is one fixed 45-degree industrial ship stair, not a vertical ladder and not a concrete staircase. It has exactly 18 open galvanized tread pans. Total rise 360 cm and horizontal run 340 cm. Each step has exactly 20 cm rise and 20 cm run, a 105 cm usable width, 20 cm tread depth, and 3 cm metal pan thickness. Include two diagonal side stringers, open risers, 42 mm round galvanized handrail tubes about 75 cm above the tread line, four pairs of vertical posts, and believable continuity into the approach and top landing guardrails.

The upper-second tread, index 16 when counted from zero at the bottom, is the accident evidence. It alone has a centered 54 cm wide by 8.5 cm deep ribbed black rubber anti-slip pad whose inner edge is lifted 8–10 mm, retained by exactly two small galvanized clips. The highest tread above it is intact. Keep the defect subtle but readable in the closeup. No broken stair, no gore, no person.

Lay out exactly four clean panels in a 2x2 grid: (1) precise side orthographic showing all 18 treads and the 45-degree travel line, (2) three-quarter full-object product view, (3) closeup of the top three treads clearly showing intact top tread and lifted pad on the upper-second tread, (4) first-person climb preview toward the top landing with the same geometry. Plain charcoal-to-neutral gray background, thin panel dividers, no labels, no dimensions printed, no logos, no decorative text.

Avoid: vertical ladder, solid concrete wedge, enclosed risers, spiral stair, domestic wooden stairs, impossible floating supports, inconsistent step count, extra rubber pads, more or fewer than two clips, exaggerated rust, dramatic colored light, fog, blood, body, hands, people, typography, watermark.
```
