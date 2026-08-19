# ImageGen 제작 기록 — 2026-08-14

## 발소리 표면 3종 · 퍼즐 판독면 2종 · 세대 현관문 1종

- 실행 경로: 로컬 ChatGPT 소프트웨어의 ImageGen (프로젝트 동일 작업공간)
- 용도 분류: `stylized-concept`
- 입력 이미지: 없음
- 조건화: `Scripts/condition_ai_tiles.py` (신규) — flat-field, 심리스화,
  디테일 게인. `generate_ai_pbr_maps.py`보다 **먼저** 돌아야 한다.

### 왜 이 여섯 장인가

README 규칙 2가 「발밑도 규칙입니다」인데, 발소리 표면 5종 중 셋이 화면에서
콘크리트와 구분되지 않았다. 소리는 다른데 그림이 같으면 「어느 바닥을
고르느냐」는 선택이 아니라 우연이 된다.

| 대상 | 태그 | 이전 재질 |
|---|---|---|
| 철제 계단 | `Footstep.MetalStair` | `M_Concrete_XY` |
| 옥상 방수층 | `Footstep.Rooftop` | `M_Concrete_XY` |
| 5층 석고 파편 | `Footstep.GypsumDebris` | `M_ConcreteDark_XY` |

나머지 셋은 퍼즐 판독면과 이 게임의 중심 오브젝트다. P1 계량기 문자판은
단색 원판이었고, P2 먹지는 정서본과 같은 낡은 종이를 쓰고 있었으며 —
「두 기록이 다르다」가 물건 단계에서 이미 무너진 상태였다 — 세대 현관문은
브러시드 스테인리스로 그려지고 있었다.

### 원본과 파생

| ImageGen 원본 | 파생 | SHA-256 |
|---|---|---|
| `TextureVillaStairCheckerPlatePaintedSteel.png` | (증빙 전용, 미사용) | `064D84D76ACDBBB93565CE298EEDE431C67463B3C954D97A9D12279E9C9A0F04` |
| `TextureVillaStairCheckerPlatePaintedSteel_v2.png` | `T_MissingFloorSteelStair_{D,N,R,A}` | `3A973BD9FAF581955F61AD577C293694A60B5738B4D2E9D697CF030A5C088A82` |
| `TextureRooftopUrethaneWaterproofing.png` | `T_RooftopWaterproofing_{D,N,R,A}` | `42116A76598BDCBB9A933BAA4C679C8A97981AD27731B1703F89C53DD04A5FE5` |
| `TextureRooftopAnnexConcreteGypsumDebris.png` | `T_MissingFloorGypsumDebris_{D,N,R,A}` | `6A6FA67CB3407BF18322010FA7F98A273AFE2FD7234AF21DEDB105B79BAFF6B8` |
| `TextureUtilityMeterDialFaceBlank.png` | `T_UtilityMeterDial_{D,N,R,A}` | `E3E3E3F9949ACEA716ECB3000C294922C34A8682A1BE6CD443C1853850A45F98` |
| `TextureComplaintLedgerCarbonPaperBlank.png` | `T_CarbonPaper_{D,N,R,A}` | `2F22C7CE8E9DD7E5A2F93D05DAA2A16AD8A40497903D26B4707D8D0BD52BC59E` |
| `TextureApartmentEntranceDoorCharcoalSteel.png` | `T_UnitDoorPaintedSteel_{D,N,R,A}` | `A3E5A30007597E439284602800B988839C06C879BFA322214C53A7F019E481C2` |

### 적용 경계

ImageGen은 알베도의 물성만 담당한다. 조명·그림자·반사·블렌딩은 Lumen과
VSM이, N/R/A는 `generate_ai_pbr_maps.py`가, 계량기 지침과 먹지의 눌린
원문은 코드와 런타임 한글이 소유한다(`MISSING_FLOOR_ART_MATRIX.md` 원칙 4).
다이얼의 드럼 창은 **비어 있는 채로** 들어오며, 다섯 번째가 돌지 않는다는
사실은 `FifthMeterDisc`가 계속 소유한다.

---

## 공통 프롬프트 블록

모든 프롬프트 뒤에 그대로 붙였다.

### 점묘 억제

```text
Rendering discipline (critical): this is a flat material scan, not a photograph of a lit scene. All surface variation must come from real physical causes — trowel passes, roller strokes, wear paths, mineral settling, paint flow, footfall polish — and must be expressed as broad soft tonal fields at roughly 5 to 40 cm real-world scale. Resolve every detail with gentle continuous gradients, never with fine dots. Absolutely no per-pixel noise of any kind: no stippling, no dithering, no halftone or screen-door pattern, no pointillist speckle, no sand or dust overlay, no crosshatch, no chromatic speckle, no oversharpened edge halos, no JPEG mosquito artifacts, no added film grain. Prefer slightly soft over crunchy; if uncertain, make it smoother. Keep the entire image inside a narrow luminance band of roughly 25 to 65 percent, with no pure black and no pure white pixels. Flat neutral diffuse capture only: no directional light, no cast shadow, no contact shadow, no specular highlight, no bloom, no vignette, no depth of field, no color grading. Shot as a calibrated flat copy-stand capture at base ISO. No text, no numbers, no letters, no logos, no watermark, no hands, no props, no border, no frame.
```

**결과: 통했다.** 여섯 장 전부 점묘 에너지 0.39~1.37로 들어왔다. 기존 승인
에셋인 `T_MissingFloorDryPlaster_D`가 6.89인 것과 비교하면 5~17배 깨끗하다.

**대가도 있었다.** 같은 문장이 진짜 요철도 함께 눌렀다. 여섯 장의 알베도
표준편차가 1.86~3.78로 왔고 승인 에셋 대역은 10~12다. 노멀을 유도할 신호가
없어서 조건화 단계의 `detail_gain`으로 되살렸다(계단 3.0, 방수 3.0,
석고 2.4, 문 2.6, 먹지 2.0). **점묘를 억제하는 프롬프트를 쓸 때는 게인을
함께 계획해야 한다.**

### 타일 균일성

```text
Tiling uniformity (critical): this texture repeats across a large surface, so it must have no center and no focal point. Statistically identical everywhere — no single area brighter, darker, more worn, or more detailed than any other, no gradient across the frame, no soft glow anywhere, no ponding, no lane, no patch. If you would naturally place one interesting feature, distribute that character evenly across the whole frame instead. Any localized feature will visibly repeat in a grid and ruin the surface.
```

이 블록은 첫 장을 뽑은 **뒤에** 추가했다. 첫 프롬프트가 `seamlessly tiling`과
「중앙 통행 레인」을 함께 요구했는데 그 둘은 양립할 수 없다 — 타일은 위치
특징을 가질 수 없다. ImageGen은 그 모순을 중앙의 흐릿한 밝은 얼룩으로
타협하면서 마모 표현은 아예 포기했다. 통행 마모나 물 고임처럼 **자리가 정해진
흔적은 타일이 아니라 masked 값 마스크**로 따로 만든다(§ASSET_STYLE 흔적 규칙).

---

## 개별 프롬프트

### 1. 철제 계단 디딤판

```text
Use case: stylized-concept
Asset type: seamlessly tiling production albedo texture for painted steel stair treads in a first-person Korean horror game
Primary request: a perfectly top-down, orthographic 1:1 square scan of an old painted steel checker-plate stair tread from a Korean walk-up villa stairwell, covering roughly 1 meter by 1 meter of real surface, filling the canvas edge to edge with no background
Subject: raised diamond anti-slip pattern in regular rows, each diamond roughly 30 mm long and 3 mm proud, worn noticeably flatter and smoother along a central walking lane where decades of feet have polished the paint down to bare gray steel
Color palette: desaturated blue-gray industrial paint over cooler bare steel where worn, with very restrained warm oxidation only in a few small isolated spots, low saturation throughout
Materials/textures: matte alkyd paint over rolled steel, gentle brush-lap variation, thinly worn paint edges that fade rather than flake into hard shapes, faint mineral dust settled between diamonds
Constraints: the tile must repeat seamlessly on all four edges with the diamond rows continuing across the boundary; no rust holes, no dramatic corrosion, no blood, no footprints, no drag marks, no handprints, no debris, no bolts, no welds, no stair nosing, no edge of the tread, no wall, no railing, no perspective
Avoid: heavy orange rust, sci-fi metal plating, glossy chrome, high-contrast grime, horror clichés, dramatic weathering, wet look
```

`central walking lane` 요구가 위 「타일 균일성」과 모순이었고, 결과의 중앙
얼룩이 그 대가다. 조건화가 제거했으며 통행 마모는 별도 마스크로 남긴다.

### 2. 옥상 우레탄 방수층

```text
Use case: stylized-concept
Asset type: seamlessly tiling production albedo texture for a Korean rooftop urethane waterproof coating
Primary request: a perfectly top-down, orthographic 1:1 square scan of an aged green urethane waterproofing membrane on a Korean apartment villa rooftop, covering roughly 1.5 meters by 1.5 meters of real surface, filling the canvas edge to edge with no background
Subject: a thick brushed-on liquid membrane with visible roller and brush lap lines running in slightly inconsistent directions, an even scatter of small pale mineral blooms of varied size distributed uniformly across the entire frame, and two or three fine hairline stress cracks that follow the slab beneath
Color palette: heavily desaturated moss green, drifting cooler and grayer where the blooms sit, with faint chalky white efflorescence; absolutely no saturated or emerald green
Materials/textures: soft rubbery matte coating, slightly uneven thickness with gentle rolling relief, a few embedded dust particles fully sealed under the coating rather than sitting on top
Constraints: the tile must repeat seamlessly on all four edges; no drains, no parapet, no pipes, no antenna, no plants, no moss growth, no puddled water, no reflections, no sky, no footprints, no perspective
Avoid: bright chroma-key green, cartoon flatness, peeling sheets, dramatic decay, wet glossy sheen, high-contrast staining
```

### 3. 5층 석고 파편 바닥

```text
Use case: stylized-concept
Asset type: seamlessly tiling production albedo texture for a debris-strewn raw concrete slab in an illegally built rooftop annex
Primary request: a perfectly top-down, orthographic 1:1 square scan of a dark unfinished concrete slab lightly scattered with broken dry gypsum board fragments and settled plaster powder, covering roughly 1.5 meters by 1.5 meters of real surface, filling the canvas edge to edge with no background
Subject: the dark slab remains clearly dominant and readable; broken gypsum pieces of mixed sizes from 2 cm chips up to 15 cm angular shards lie sparsely and evenly across the whole frame, showing both their pale gray paper face and their soft crumbling white core at the break; a thin even veil of fine plaster powder
Color palette: dark cool gray concrete as the base, pale bone-ivory gypsum, no warm tones, no color casts, everything low saturation
Materials/textures: rough troweled concrete with shallow pitting, matte chalky gypsum with soft fractured edges
Constraints: the tile must repeat seamlessly on all four edges; the debris must stay sparse enough that the concrete underneath is visible across most of the surface; no footprints, no drag marks, no handprints, no body, no blood, no tools, no timber, no nails, no wires, no plastic sheeting, no perspective
Avoid: an even continuous white layer, snow-like coverage, high-contrast bright white chips, dramatic rubble, demolition-site chaos, dust clouds
```

발자국과 끌림 자국은 굽지 않는다. `UIGSettledDustComponent`가 런타임 ISM으로
그리므로 구워 넣으면 이중으로 찍힌다.

### 4. 계량기 문자판

```text
Use case: stylized-concept
Asset type: production albedo texture for the dial face of an old analogue utility meter in a Korean apartment lobby
Primary request: a perfectly front-facing, orthographic 1:1 square image of the circular dial face of an aged analogue electricity meter, with the circular face filling the entire square canvas edge to edge and no surrounding background, housing, or glass
Subject: a cream-white enamel dial plate with a plain rectangular recessed window across the middle where a mechanical digit drum would sit, left completely blank and empty; a fine engraved circular tick scale near the outer rim with small evenly spaced graduation marks; one narrow deep-red painted arc segment on the lower right; a small central spindle boss
Style/medium: restrained photorealistic instrument face, physically plausible enamel and printed ink, not an illustration and not a UI mockup
Color palette: aged cream and warm off-white, faded charcoal engraving, one muted deep red arc, very low saturation
Materials/textures: slightly yellowed enamel over stamped metal, faint concentric machining under the enamel, a light even film of settled dust in the recessed window, tiny handling scuffs near the rim
Constraints: absolutely no numbers, no digits, no letters, no Hangul, no manufacturer name, no model number, no units, no logo, no needle, no pointer, no glass, no reflection, no housing, no screws, no perspective, no cast shadow; the digit window must remain completely blank so Korean text and the reading can be composited at runtime
Avoid: steampunk gauges, ornate dials, dramatic rust, cracked glass, glowing elements, high-contrast grime
```

### 5. 먹지

```text
Use case: stylized-concept
Asset type: production albedo texture for a used sheet of carbon paper resting under a building complaint ledger
Primary request: a perfectly front-facing, orthographic portrait sheet at exactly 1:1.414 aspect ratio, a single used A4 carbon paper sheet filling the entire canvas edge to edge with no surrounding background
Subject: the coated face of an old carbon transfer sheet, its waxy pigment layer unevenly depleted from repeated use, showing broad soft areas where the coating has been pressed thin and now catches light differently from the untouched areas; one gentle diagonal curl relaxation across the lower third; slightly frayed and darkened handled edges; a few faint fingertip smudges near the top corners
Style/medium: restrained photorealistic paper scan, physically plausible waxed carbon coating, not concept art
Color palette: deep prussian blue-black shifting toward a cooler graphite sheen where worn, extremely low saturation, no true black
Materials/textures: thin waxy pigment over lightweight tissue paper, the paper fiber faintly visible only where the coating has thinned, gentle sheen variation rather than gloss
Constraints: absolutely no words, no Hangul, no letters, no numbers, no dates, no legible writing, no readable impressions, no ruled lines, no stamps, no logo, no watermark, no hands, no pen, no desk, no perspective, no cast shadow; keep the central 70 percent calm and even so pressed handwriting can be composited at runtime
Avoid: dramatic tearing, burned edges, heavy crumpling, high-contrast stains, horror clichés, glossy plastic look, blue-purple oversaturation
```

A4 비율은 유지한다(`724×1024`). 22×30.7cm 원장 아래에 UV로 붙는 종이라
정사각으로 리샘플하면 섬유와 접힘이 함께 늘어난다. 아트 계약에 이 예외를
명시했다.

### 6. 세대 현관문 도장 강판

```text
Use case: stylized-concept
Asset type: seamlessly tiling production albedo texture for the painted steel leaf of a Korean apartment entrance door
Primary request: a perfectly front-facing, orthographic 1:1 square scan of matte powder-coated charcoal steel from an aging Korean residential entrance door leaf, covering roughly 60 cm by 60 cm of real surface, filling the canvas edge to edge with no background
Subject: a flat painted steel panel with a very shallow orange-peel finish from factory powder coating, a few long faint settling marks in the paint, and gently uneven sheen where hands and cleaning cloths have polished the surface over years
Color palette: dark neutral charcoal with a barely perceptible cool cast, extremely low saturation, no blue tint and no brown tint
Materials/textures: matte powder coat over cold-rolled steel, soft micro-undulation of the orange peel expressed as broad gentle relief rather than fine bumps, a small number of shallow scratches that read as thin soft lines and not as bright cuts
Constraints: the tile must repeat seamlessly on all four edges; no panel moldings, no raised frames, no recessed rectangles, no handle, no lever, no lock, no keypad, no peephole, no hinge, no number plate, no brushed metal band, no rivets, no door edge, no wall, no perspective; the surface must stay uniform enough to tile without any recognizable single feature repeating
Avoid: brushed stainless hairline, glossy automotive paint, dramatic dents, heavy rust, wood grain, high-contrast scuffing, wet look
```

발치 긁힘과 손잡이 손때는 이 타일에 넣지 않는다. 자리가 정해진 마모이므로
기존 masked 잔흔 평면 방식으로 따로 얹는다.

---

## 조건화 결과

`condition_ai_tiles.py`가 flat-field·심리스화·게인을 수행하고 앞뒤를 함께
잰다. 얼룩 판정은 절대 편차가 아니라 **그 표면의 대비 대비 편차**다 —
게인을 넣으면 잔여 필드와 재질이 함께 곱해지므로 절대값으로는 모든 게인
타일이 반려된다.

| 에셋 | 얼룩/대비 | 이음매 비율 H/V | 최종 stddev |
|---|---|---|---|
| 철제 계단 | 2.33 → 0.61 | 2.89/3.52 → 0.85/1.37 | 8.14 |
| 옥상 방수 | 3.97 → 3.62 | 3.11/2.88 → 0.86/0.89 | 7.73 |
| 석고 파편 | 2.81 → 2.32 | 2.32/2.33 → 0.95/1.01 | 9.04 |
| 계량기 문자판 | 1.89 → 1.74 | — (물체) | 12.62 |
| 먹지 | 4.86 → 3.25 | — (물체) | 5.84 |
| 현관문 강판 | 1.49 → 1.05 | 1.69/1.41 → 0.93/0.95 | 4.78 |

방수의 3.62는 얼룩이 아니라 **롤러 랩 패치 자체**다. 200~400px라 64px 측정
격자에 잡히는 것이며, 2×2 타일 미리보기(`Docs/Media/tiles/`)가 그 판정을
담당한다. 숫자가 파일을 통과시키고 그림이 승인을 통과시킨다.

## 재현 경로

```powershell
.\Scripts\Prepare-AIArt.ps1 -OnlySource @('TextureVillaStairCheckerPlatePaintedSteel')
python Scripts\condition_ai_tiles.py --only T_MissingFloorSteelStair
python Scripts\generate_ai_pbr_maps.py --only T_MissingFloorSteelStair
.\Scripts\Build-ArtAssets.ps1 -MissingFloorOnly
.\Scripts\Test-ArtAssetContract.ps1
```

조건화는 파괴적 in-place 변환이다. 다시 돌리려면 **반드시 ImageGen 원본에서**
시작해야 하며, `--force`는 이미 조건화된 파일에 대해 거부한다. 그 가드가 없던
동안 계단 타일이 게인 9배로 뭉개지고 주기가 64에서 132로 오검출됐다.


---

## 계단 v2 — 숫자가 통과시킨 것을 그림이 반려했다

v1은 조건화 뒤 모든 계약을 통과했다. 점묘 0.39, 이음매 비율 0.85/1.37,
게인 3.0으로 stddev를 8.14까지 올려 승인 대역에 넣었고, V5 히스토그램도
밴드 안이었다. 그런데 손전등 프레임(`Docs/Media/v5-steel_stair.png`)을
확대해 보니 **디딤판이 매끈한 판**이었고 먼 단에는 검은 점박이만 있었다.
다이아몬드는 어디에도 없었다.

숫자는 거짓말을 하지 않았다. 다만 **게인은 대비를 옮길 뿐 만들지 못한다.**
v1 원본의 1~99% 계조 범위는 117..146, 전체의 11%였다. 3배로 곱해도
분포의 모양은 그대로이고, 게임 조명과 tint를 거치면 3% 미만으로 내려앉는다.
`stddev 8.14`는 승인 대역에 들어왔다는 사실만 말했지, 그 8.14가 무엇으로
이루어졌는지는 말하지 않았다.

원인은 프롬프트였다. v1은 릴리프를 `3 mm proud`라는 **치수**로 요구했는데,
같은 프롬프트의 점묘 억제 블록이 「부드럽게, 확신이 없으면 더 매끄럽게」를
요구하고 있었다. 두 문장이 충돌했고 생성기는 매끄러운 쪽을 택했다.

v2는 릴리프를 **명암 범위로** 요구했다.

```text
Tonal requirement (important): this is a high-relief surface and the image must show that. The lightest diamond crowns and the darkest channels between them should span a wide tonal range — roughly 30 to 75 percent luminance — so the pattern is legible from three metres away in a dark stairwell. Do not flatten the pattern into a subtle or uniform grey plate. Smoothness applies to noise, never to the tread pattern itself.
```

마지막 문장이 핵심이다. 점묘 억제와 릴리프 요구는 원래 모순이 아니라
**적용 대상이 다른 두 요구**인데, v1은 그것을 구분해 주지 않았다.

커버리지도 55cm로 고정해 타일 배율과 일치시켰다. v1은 「1m에 30mm 다이아몬드」를
요구했지만 실제로는 1m에 16개(62mm)가 나왔고, 그 불일치를 `tile` 값으로
사후 보정해야 했다.

| | v1 | v2 |
|---|---|---|
| 원본 stddev | 3.48 | **19.56** |
| 1~99% 계조 범위 | 117..146 (11%) | 84..180 (**38%**) |
| 원본 저주파 얼룩 | 8 | **1** |
| detail_gain | 3.0 (효과 없음) | **불필요** |
| normal_strength | 0.88 | **0.62** |
| 조건화 후 이음매 비율 | 0.85/1.37 | **0.69/0.69** |

게인이 사라지고 노멀 강도가 **내려간** 것이 개선의 표시다. 0.88은 약한
스캔을 떠받치던 값이었고, 19.6짜리 스캔에 그대로 쓰면 판이 골판지가 된다.
0.62는 비슷한 대비를 가진 건식 석고와 같은 값이다.

균일성 블록도 효과가 확인됐다. v1의 저주파 얼룩 8이 v2에서는 1로 왔다 —
16×16 평균이 분해할 수 있는 하한이다. 이 때문에 조건화 판정에 바닥값을
넣어야 했다: 1이 2로 바뀌는 것은 반올림인데 상대비로는 두 배라 정상적인
타일이 반려됐다. 4/255 미만의 얼룩은 어떤 대비에서도 보이지 않으므로
그 아래에서는 절대값이 판단한다.

### 남은 것

v2의 소스 아트와 계약은 끝났지만 **UAsset 임포트가 밀려 있다.** 이 기계의
Windows 무결성 정책이 새로 빌드된 서명 없는 모듈을 거부하고 있어
(`GetLastError=4551`) UE 단계가 실행되지 않는다. 게임 안의 계단은 아직
v1 텍스처다. 정책이 풀린 뒤 아래를 돌리면 v2가 들어가고 새 프레임이 나온다.

```powershell
.\Scripts\Build-ArtAssets.ps1 -MissingFloorOnly
.\Scripts\Run-MissingFloor-NightHistogram.ps1 -ReportOnly
```
