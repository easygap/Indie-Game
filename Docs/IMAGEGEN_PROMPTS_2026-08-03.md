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
