# ImageGen 제작 지시서 — 2026-08-20

## 세대 벽지 2종차(`Wallpaper` 표면 되살리기)

- 실행 경로: ChatGPT ImageGen
- 용도 분류: `stylized-concept`
- 입력 이미지: 없음
- 조건화: `Scripts/condition_ai_tiles.py` → `Scripts/generate_ai_pbr_maps.py`
  (이 순서가 아니면 알베도의 밝기 얼룩이 가짜 요철이 된다)

### 왜 이 한 장인가

`Content/SourceArt/Photo/Wallpaper/`에 들어 있는 것은 AmbientCG `Plaster003`,
회벽 스캔이다. `import_photo_textures.py`가 **폴더 이름으로** 에셋 이름을
짓기 때문에 그것이 `T_Photo_Wallpaper_{D,N}`이 됐고, 아무 머티리얼도 그리지
않는다. 절차 생성기 `build_wallpaper`가 굽는 `T_Wallpaper_{D,N}`도 마찬가지로
소비자가 없다(256×256, 균일한 회녹색 얼룩 — 그레이박스 자리표시자다).

지우는 대신 **채우는** 쪽을 택하면, `Wallpaper`라는 표면 이름이 실제 내용을
갖게 된다. 채울 내용은 「두 번째 벽지」다. 지금 세대 벽 머티리얼 셋
(`M_Wallpaper_X`, `M_Wallpaper_Y`, `M_WallpaperCeil`)이 전부 같은
`T_ApartmentWallpaperV2` 한 장을 쓴다. 401·403·주인공 집이 같은 벽지다.
호수를 오가며 비교하는 게임에서 그건 놓친 박자이고, 손전등이 훑을 때
「같은 에셋」으로 읽힌다.

### 무엇을 그리는가

기존 V2는 **크림색 리넨 바탕에 잔잎·덩굴 프린트**다. 두 번째 장은 한눈에
다르되 같은 연식·같은 등급이어야 하므로 **무지 엠보싱 세로 결** 벽지로
간다 — 한국 빌라에서 프린트 벽지 다음으로 흔하고, 무늬가 없어 프린트와
정면으로 대비되며, 어두운 복도광에서도 결이 세로로 읽힌다.

| | 기존 `ApartmentWallpaperV2` | 신규 `ApartmentWallpaperEmboss` |
|---|---|---|
| 성격 | 잔잎·덩굴 프린트 | 무지, 세로 엠보싱 결 |
| 색 | 크림/베이지 | 창백한 회녹 또는 회청 |
| 대비 요소 | 무늬 | 결과 광택차 |

---

## 1. 프롬프트 전문 (그대로 붙여넣기)

아래 세 블록을 **순서대로 이어 붙여** 한 번에 넣는다. 프로젝트의 기존
여섯 장이 이 구성으로 통과했다.

### 1-A. 본문

```text
Use case: stylized-concept
Asset type: seamlessly tiling production albedo texture for plain embossed vinyl wallpaper in a Korean low-rise walk-up villa flat, for a first-person psychological horror game
Primary request: a perfectly front-facing, orthographic 1:1 square scan of aged plain embossed wallpaper from a Korean residential interior wall, covering roughly 165 cm by 165 cm of real wall surface, filling the canvas edge to edge with no background
Subject: an unpatterned vinyl-coated wallpaper whose only structure is a shallow regular vertical emboss — soft rounded ribs roughly 6 mm apart running strictly parallel to the vertical edge of the frame from top to bottom, uninterrupted, of even depth everywhere; over the emboss, a very gentle unevenness of age distributed uniformly across the whole frame: faint broad tonal drift from years of indoor air, a barely perceptible overall dulling, and the softest possible suggestion of the paper hanging flat against plasterboard behind it
Style/medium: restrained photorealistic flat material scan of real wallpaper, physically plausible vinyl coating over paper backing, not an illustration, not concept art, not a pattern swatch
Color palette: pale desaturated grey-green drifting very slightly warmer in the broad tonal variation, the kind of colour that reads as almost-white under room light and clearly cool under a flashlight; extremely low saturation throughout, no true white and no true black
Materials/textures: thin vinyl skin over paper backing, matte with only a whisper of directional sheen along the emboss ribs, the ribs expressed as broad soft rounded relief rather than sharp ridges, paper fibre never visible through the coating
Constraints: the tile must repeat seamlessly on all four edges with the vertical ribs continuing across the top and bottom boundary in exact register and the rib spacing continuing across the left and right boundary; the ribs must be perfectly vertical and perfectly parallel with no drift, no waviness and no perspective convergence; no printed pattern, no flowers, no leaves, no vines, no geometric motif, no damask, no stripe of a different colour, no seam between wallpaper sheets, no overlap join, no wall corner, no skirting, no ceiling line, no switch, no socket, no nail, no picture hook, no furniture, no perspective, no cast shadow; no tears, no peeling, no bubbling, no mould, no water stain, no scuff, no handprint — all damage is applied later as a separate mask and must not be baked in
Avoid: bold wallpaper patterns, textile or grasscloth weave, woodchip or anaglypta chip texture, stucco or plaster relief, high-contrast embossing, glossy vinyl, cream or beige tones that would read as the existing floral paper, dramatic ageing, horror-film decay, warm yellow cast
```

### 1-B. 점묘 억제 (검증된 블록 — 한 글자도 고치지 말 것)

```text
Rendering discipline (critical): this is a flat material scan, not a photograph of a lit scene. All surface variation must come from real physical causes — trowel passes, roller strokes, wear paths, mineral settling, paint flow, footfall polish — and must be expressed as broad soft tonal fields at roughly 5 to 40 cm real-world scale. Resolve every detail with gentle continuous gradients, never with fine dots. Absolutely no per-pixel noise of any kind: no stippling, no dithering, no halftone or screen-door pattern, no pointillist speckle, no sand or dust overlay, no crosshatch, no chromatic speckle, no oversharpened edge halos, no JPEG mosquito artifacts, no added film grain. Prefer slightly soft over crunchy; if uncertain, make it smoother. Keep the entire image inside a narrow luminance band of roughly 25 to 65 percent, with no pure black and no pure white pixels. Flat neutral diffuse capture only: no directional light, no cast shadow, no contact shadow, no specular highlight, no bloom, no vignette, no depth of field, no color grading. Shot as a calibrated flat copy-stand capture at base ISO. No text, no numbers, no letters, no logos, no watermark, no hands, no props, no border, no frame.
```

### 1-C. 타일 균일성 (검증된 블록 — 한 글자도 고치지 말 것)

```text
Tiling uniformity (critical): this texture repeats across a large surface, so it must have no center and no focal point. Statistically identical everywhere — no single area brighter, darker, more worn, or more detailed than any other, no gradient across the frame, no soft glow anywhere, no ponding, no lane, no patch. If you would naturally place one interesting feature, distribute that character evenly across the whole frame instead. Any localized feature will visibly repeat in a grid and ruin the surface.
```

---

## 2. 왜 이렇게 썼는가 — 프롬프트 설계 근거

프롬프트를 고칠 일이 생길 때를 위해 각 요구의 이유를 남긴다.

**「점묘 억제」는 그대로 쓴다.** 2026-08-14에 여섯 장으로 검증했다. 점묘
에너지 0.39~1.37로 들어왔고, 그 전 승인 에셋 `T_MissingFloorDryPlaster_D`가
6.89였다. **5~17배 깨끗하다.** 이 문단이 AI 특유의 점묘화를 실제로 막는
장치이므로 요약하거나 바꿔 쓰지 않는다.

**대가를 미리 계획한다.** 같은 문장이 진짜 요철도 함께 누른다. 검증 여섯
장의 알베도 표준편차가 1.86~3.78로 왔고 승인 대역은 10~12다. **프롬프트로
점묘를 죽이면 노멀을 유도할 신호도 같이 죽는다.** 그래서 조건화 단계의
`detail_gain`이 필수이고, 아래 3절에 이 벽지의 시작값을 적어 뒀다.

**「타일 균일성」이 위치 특징을 금지한다.** 2026-08-14의 철제 계단이
`seamlessly tiling`과 「중앙 통행 레인」을 동시에 요구했다가 중앙 밝은
얼룩을 얻었다. 타일은 자리를 가질 수 없다. 그래서 이 프롬프트도 **찢김·
곰팡이·얼룩·손자국을 전부 금지**한다 — 세대 벽의 손상은 이미
`M_ApartmentWallPatina`(마스크 평면)가 소유한다. 구워 넣으면 격자로 반복되고
마스크와 이중으로 찍힌다.

**세로 결의 「정확히 수직·평행·상하 정합」을 못 박는다.** 심리스 타일에서
결이 미세하게 기울면 이음매에서 어긋남이 눈에 띈다. 무늬 없는 벽지는 그
결이 유일한 구조라 어긋남을 가릴 것이 없다.

**색을 크림/베이지에서 밀어낸다.** 기존 V2와 한눈에 구분되어야 하는 것이
이 에셋의 존재 이유다. `Avoid`에 명시적으로 넣었다.

**조명·그림자·반사를 전부 금지한다.** 알베도는 물성만 담당하고, 조명은
Lumen과 VSM이, N/R/A는 `generate_ai_pbr_maps.py`가 소유한다
(`MISSING_FLOOR_ART_MATRIX.md` 원칙 4).

---

## 3. 받은 뒤에 해야 하는 것

**ImageGen 출력은 그대로 쓸 수 없다.** 프롬프트가 아무리 좋아도 남는 결함
두 가지가 있고, 둘 다 평평한 원본에서는 안 보이다가 벽에 타일링되어
손전등이 지나가는 순간 드러난다(`condition_ai_tiles.py` 서문).

1. 저주파 밝기 필드 — 생성기는 구도를 잡지 않을 수 없어서 한 군데를
   밝힌다. 타일링하면 격자로 반복되어 구운 조명으로 읽힌다.
2. 감기지 않는 가장자리 — 생성기는 이 그림이 타일인 줄 모른다.

### 순서

**배선은 커밋되어 있다**(`Content/SourceArt/AI/`에 원본만 놓으면 된다).
아래는 무엇이 어디에 들어갔는지의 기록이다.

```
원본 위치   Content/SourceArt/AI/
            TextureApartmentWallpaperEmbossedPlainGreyGreen_v1.png

1) Prepare-AIArt.ps1            Source/Target 항목, Size 1024x1024
2) condition_ai_tiles.py        TileSpec: seam="period", blend=0.03,
                                detail_gain=2.0      ← 3)보다 먼저 돈다
3) generate_ai_pbr_maps.py      SurfaceSpec: 0.86 / 0.74~0.92,
                                normal 0.38, rough_detail 0.06, ao 0.55
4) create_textured_materials.py M_WallpaperEmboss_X / _Y,
                                SURFACE_RESPONSE_DEFAULTS 항목
5) IGPrologueWorldScene.cpp     403호가 이 벽지를 쓴다 + LoadTexturedMaterials
6) Test-ArtAssetContract.ps1    크기 목록
7) Build-ArtAssets.ps1          --only 추가, ApartmentVisualOnly 필수 목록

실행
   Scripts/Build-ArtAssets.ps1 -ApartmentVisualOnly
   Scripts/Validate-Project.ps1
```

### 후보가 여러 장일 때 — `--rank`

한 세션이 비슷비슷한 변주를 여러 장 돌려준다. 나란히 놓고 봐서는 무엇이
갈리는지 안 보이므로(스페클, 감김 오차, 밝기, 결 굵기 전부 화면에서
구분되지 않는다) 재서 고른다.

```
python3 Scripts/condition_ai_tiles.py --rank <후보폴더> --coverage-cm 165
```

`--coverage-cm`을 주면 검출한 주기를 **밀리미터로** 환산해 준다. 결
굵기는 이것 없이는 판정할 수 없다 — 헤어라인 엠보싱과 판재 몰딩은 둘 다
「세로 결」이고, 실척을 모르면 같은 그림이다.

눈금으로 쓸 기존 에셋 실측값:

| 파일 | stipple | 비고 |
|---|---|---|
| 2026-08-14 승인 6장 | 0.39~1.37 | 점묘 억제문을 쓴 것 |
| `T_ApartmentWallpaperV2_D` (출시 중) | **5.25** | 이 벽지 옆에 붙는다 |
| `T_MissingFloorDryPlaster_D` | 6.89 | 억제문 이전 |
| `Plaster003` (진짜 사진 스캔) | 9.81 | 입자가 실제로 있음 |

**밝기는 정렬 기준이 아니다.** 창백한 재질은 원래 높게 나온다 — 출시 중인
V2도 픽셀의 99%가 65% 위에 있다. 문제가 되는 것은 밝기 자체가 아니라
프레임 안에서 밝기가 **변하는** 것이고, 그건 `blob`(field spread) 열이
잡는다.

**두 가지를 먼저 확인할 것.**

```
python3 Scripts/condition_ai_tiles.py --report-only \
        --only T_ApartmentWallpaperEmboss
```

- 원본이 1024가 아니면 `Prepare-AIArt.ps1`의 `Size`를 실제 크기로 바꾼다.
  2048로 왔다면 그대로 2048을 쓰는 편이 낫다 — 165cm에 1024면 6.2px/cm이라
  6mm 결이 3.7픽셀이고, 2048이면 7.4픽셀이다.
- 조건화 후 목표는 2026-08-14 실적 대역이다. 얼룩/대비 1.0 이하,
  이음매 비율 1.4 이하.

### `seam`을 `period`로 두는 이유

세로 결은 **규칙 구조**다. `condition_ai_tiles.py`의 정의 그대로 —
crossfade는 "stochastic surfaces with no repeating structure"용이고,
규칙 구조에 쓰면 리듬을 뭉갠다. 철제 계단이 `period`/`blend=0.03`인 이유가
그대로 적용된다: 결 두 줄이 서로 녹아드는 것이 이음매보다 눈에 띈다.

`condition_ai_tiles.py --report-only --only T_ApartmentWallpaperEmboss` 로
먼저 재 보면 얼룩과 이음매 수치가 나온다. 조건화 후 목표는 2026-08-14
실적과 같은 대역이다 — 얼룩/대비 1.0 이하, 이음매 비율 1.4 이하.

### 어느 벽에 붙는가

주인공 집(`BuildApartment`)은 꽃무늬 V2를 그대로 쓰고, **403호**
(`BuildChapterTwoOverlay`)가 이 벽지를 쓴다. 있을 수 없는 그 방이 같은
벽지를 쓰고 있었다 — 그건 「내 집의 복사본」이라고 말하는 셈이고, 이
장면의 요점은 그 반대다. 천장은 원래대로 `M_StuccoCeil`이라 엠보싱
천장 변형은 만들지 않는다.

`TexMat`은 `LoadTexturedMaterials`의 `MaterialNames[]`로 채운 맵만 읽는다.
그 목록에 없는 머티리얼은 씬이 영영 닿을 수 없으므로 함께 등록했다.
아트 빌드 전까지는 `WallMaterial`로 폴백한다.

### `detail_gain` 을 왜 2.0에서 시작하는가

2026-08-14 실적: 철제 계단 3.0, 옥상 방수 3.0, 석고 파편 2.4, 현관문 2.6,
먹지 2.0. **먹지가 2.0인 이유가 그대로 여기 적용된다** — "paper, not plate;
pushing it further starts to read wrong". 벽지도 종이다. 결이 안 읽히면
2.4까지, 그 이상은 비닐 코팅이 금속처럼 번들거리기 시작한다.

---

## 4. 스프라이트(알파 컷아웃)를 뽑을 때

벽지는 타일이라 위 규칙을 쓰지만, 인물·소품 컷아웃은 규칙이 다르다.
`T_Sprite*` 계열이 쓰는 관례는 이렇다.

### 4-A. 배경 지정 (본문 안에 넣는다)

```text
Scene/backdrop: a perfectly flat solid #00ff00 chroma-key background. The background must be one uniform color with no shadows, gradients, texture, reflections, floor plane, lighting variation, or scenery.
Constraints: do not use #00ff00 or any green reflected spill anywhere on the subject; crisp silhouette suitable for chroma removal; generous green padding around the silhouette; no part of the subject cut off by the frame edge; no text, no labels, no numbers, no watermark, no logo, no border, no cast shadow on the green background.
```

### 4-B. 점묘 억제 (컷아웃판)

타일용 1-B를 그대로 쓰면 안 된다. 「flat copy-stand capture」와 「no
directional light」가 인물에는 맞지 않는다. 이렇게 바꿔 쓴다.

```text
Rendering discipline (critical): resolve every surface with gentle continuous gradients, never with fine dots. Absolutely no per-pixel noise of any kind: no stippling, no dithering, no halftone or screen-door pattern, no pointillist speckle, no sand or dust overlay, no crosshatch, no chromatic speckle, no oversharpened edge halos, no JPEG mosquito artifacts, no added film grain. Prefer slightly soft over crunchy; if uncertain, make it smoother. Restrained detail that still reads at 512 px. Soft neutral cool light on the subject only, no rim light, no bloom, no colour grading, no depth of field, no motion blur, no baked contact shadow.
```

### 4-C. 주의 — 추출 도구가 저장소에 없다

`Docs/IMAGEGEN_PROMPTS_2026-08-11.md`와 `Docs/ASSET_POLICY.md`가
`remove_chroma_key.py --auto-key border --soft-matte --despill
--edge-contract 1 --transparent-threshold 12 --opaque-threshold 220 --force`
를 기록하고 있는데, **그 스크립트는 `Scripts/`에 없고 히스토리에도 없다.**
기존 컷아웃은 저장소 밖에서 처리된 것으로 보인다. 컷아웃을 새로 뽑으신다면
그 단계에서 막히므로, 필요하시면 같은 인자 규격으로 스크립트를 작성해
드리겠다.

---

## 5. 이 지시서로 만들지 **않는** 것

- **손상·얼룩·곰팡이·손자국** — 자리가 정해진 흔적은 타일이 아니라 masked
  값 마스크다(`M_ApartmentWallPatina`, `EVIDENCE_MASK_MATERIALS`).
- **한글·숫자·글자** — 인쇄물의 원문은 코드와 런타임 한글이 소유한다
  (원칙 4). 생성 이미지의 잘못된 글자가 증거로 보이면 안 된다.
- **N/R/A 맵** — `generate_ai_pbr_maps.py`가 `_D` 하나에서 파생한다.
  ImageGen에 노멀맵을 요청하지 않는다.
- **또 다른 회벽** — 벽면 네 종류(복도 회벽·외벽·5층 마른 석고·세대 벽지)가
  이미 전부 임자가 있다. `Plaster003`이 남은 이유가 그것이고, 회벽을 한 장
  더 뽑으면 같은 실수를 반복하게 된다.
