# 이미지 생성 기록 — 2026-09-04

Blender 절차 모델링의 비례·재질 기준과, 가전 조작 패널 텍스처를 위해 GPT
Image로 네 장을 뽑았다. 전부 가상 브랜드이고 실존 로고·상호는 없다. 원본은
`Content/SourceArt/AI/`에 그대로 두고, 패널 시트만 `panels/`에 네 칸으로
잘라 Blender 빌더가 `image_quad`로 붙인다.

| 원본 | 용도 | 비고 |
|---|---|---|
| `SheetVillaCorridorFixturesReference.png` | 세대 현관문·우편함·소화전함·천장 등 비례 기준 | `build_unit_door.py`, `build_lobby_mailboxes.py`, `build_corridor_fixtures.py`, `build_ceiling_light.py` |
| `SheetOneroomKitchenAppliancesReference.png` | 소형 냉장고·빌트인 주방·전자레인지·인터폰 기준 | `build_fridge.py`, `build_kitchen.py` |
| `SheetStoreFixturesReference.png` | 편의점 음료 냉장고·곤돌라·평대 냉동고 기준 | `build_store_fixtures.py` |
| `SheetApplianceControlPanels.png` | 도어락 키패드·전자레인지 패널·세탁기 패널·인터폰 앞면 텍스처 | `panels/*.png`로 잘라 세탁기·전자레인지에 사용 |

## 프롬프트

### 복도 설비 기준 시트

> Photorealistic product reference sheet, 2x2 grid with thick white gutters, pure white background, even soft studio light, no people, no text captions. Top-left: a Korean villa apartment entrance steel door (방화문) seen exactly front-on, dark charcoal matte painted steel leaf 84 cm wide and 200 cm tall in a slim powder-coated frame, a narrow brushed stainless vertical inlay strip with five small dark square inlays, a stainless lever handle on a round rose, a matte black digital keypad door lock mounted above the handle, a small round door viewer at eye level, a small unit number plate above the frame, a rubber sweep at the bottom. Top-right: a wall-mounted Korean apartment lobby mailbox unit (우편함), 3 columns by 3 rows of dark blue-grey powder-coated steel boxes, each with a horizontal mail slot with a lip, a small cam lock keyhole and a tiny blank name-card window, front-on. Bottom-left: a red Korean corridor fire hose cabinet (소화전) front-on, painted steel box with a recessed door, a small rectangular glass window, a chrome pull handle, worn paint edges. Bottom-right: a flush-mount round LED ceiling light for a Korean stairwell landing, 26 cm diameter, satin white polycarbonate diffuser dome on a thin stainless base ring, shown from slightly below. Consistent scale hints, slightly worn everyday realism, no logos.

### 원룸 주방 기준 시트

> Photorealistic product reference sheet, 2x2 grid with thick white gutters, pure white background, even soft studio light, no people, no text captions, no logos. Top-left: a compact Korean studio-apartment two-door refrigerator, white glossy enamel, freezer on top, 158 cm tall and 72 cm wide, slim recessed integrated handles on the right edge, visible rubber door gaskets, slightly rounded door edges, seen front-on from slightly above. Top-right: a Korean built-in kitchenette run seen straight on: white high-gloss slab cabinet doors with a finger-pull groove instead of handles, a dark speckled stone worktop, an inset stainless sink with a gooseneck mixer tap, a black glass induction hob, a stainless slim range hood, matching white gloss wall units with an LED strip under them, a front-loading drum washer built in under the worktop with a round glass porthole door. Bottom-left: a small countertop microwave oven front-on, brushed stainless body, dark glass door with a chrome handle on the right, a vertical control panel with a small display and touch buttons. Bottom-right: a video door intercom wall unit and a light switch bank front-on, white plastic, a small dark screen, a speaker grille and two buttons. Everyday worn realism, consistent scale.

### 편의점 집기 기준 시트

> Photorealistic product reference sheet, 2 cells side by side with a thick white gutter, pure white background, even soft studio light, no people, no logos, no readable brand names. Left cell: a Korean convenience store reach-in drinks cooler bank front-on, three framed glass doors with black anodized aluminium frames and long vertical chrome handles, a lit white header light box above the doors, interior lit by cool LED strips, five wire shelves per door with translucent price rails on the shelf edges, bottles and cans lined up facing forward. Right cell: a Korean convenience store double-sided gondola shelving unit front-on, light grey steel frame with a slotted back panel, five adjustable steel shelves with raised front lips and white plastic price strips, snack bags and cup noodles on the shelves, a chest freezer with a sliding glass top beside it. Everyday realism, slightly worn.

### 가전 조작 패널 텍스처

`ASSET_STYLE.md`의 공통 스타일 규칙을 앞에 붙였다.

> [이번 이미지] 2x2 격자 시트, 칸 사이는 굵은 흰 여백, 각 칸은 완전 정면 평면 텍스처. 좌상: 아파트 현관 디지털 도어락의 앞면 터치 키패드 패널, 세로 비율 9:24, 무광 검정 플라스틱 위 검정 유광 터치 유리, 0~9와 * # 숫자가 아주 희미하게 비치는 배열, 아래에 작은 카드 터치 표시와 소형 LED 하나. 우상: 소형 전자레인지 세로 조작 패널, 비율 9:20, 무광 진회색 플라스틱, 상단 작은 검정 LED 창, 아래로 「해동」「데우기」「시작」「취소」 한글 터치 버튼, 회전 다이얼 없음. 좌하: 드럼세탁기 가로 조작 패널, 비율 54:11, 흰색 유광 플라스틱, 왼쪽 전원 버튼, 가운데 작은 LED 창, 오른쪽 「표준」「울」「헹굼」「탈수」 한글 코스 표시와 원형 다이얼 자리. 우하: 비디오 인터폰 벽부 본체 앞면, 비율 7:11, 흰색 무광 플라스틱, 위쪽 작은 검정 화면, 아래 스피커 구멍 격자와 「통화」「열림」 한글 버튼 두 개. 네 칸의 조명, 그레인, 광택은 완전히 동일해야 해.

## 생성 3D (TRELLIS.2, 2026-09-08)

새 이미지는 만들지 않았다. 기존 기준 시트의 한 칸을 그대로 ComfyUI 네이티브
TRELLIS.2에 넣어 형상을 뽑았다. 어느 칸을 썼는지와 서버·시드·그래프 해시는
`Content/SourceArt/Generated/<이름>/<시도>/generation.json`에 있다.

| 입력 칸 | 결과 | 다듬기 |
|---|---|---|
| `SheetListenerEntityAnatomyReference.png` 좌하(정면 3/4) | `SM_ListenerEntityCrawl` | 길이 190, yaw 90, 정점 AO |
| `SheetAlleyCatPoseReference.png` 우상(정면 3/4) | `SM_AlleyCatRun` | 길이 85, yaw 88.2, 복셀 리메시 6 mm |
| `SheetMokHansooConfrontationReference_v1.png` 좌상(정면) | `SM_MokHansooFigure` | 높이 172 |
| `SheetFinalCavityRemainsReference_v1.png` 좌상(정면) | `SM_FinalCavityRemains` | 높이 144, 깊이 절반, 벽감 뒤 잘라냄 |
