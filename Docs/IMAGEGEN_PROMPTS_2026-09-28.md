# 이미지 생성 기록 — 2026-09-28

옥상 사방의 밤 원경으로 GPT Image(ChatGPT 구독 경로)에서 네 장을 뽑았다.
하늘은 투명으로 받아 게임의 하늘과 달이 그림 위로 그대로 보이게 했다. 사람,
차, 읽을 수 있는 글자, 실존 상호는 넣지 않았다. 원본은 `Content/SourceArt/AI/`에
그대로 두고, `Scripts/build_night_view_masks.py`가 네 장을 한 띠로 묶어
`Content/SourceArt/NightView/`에 색 띠와 불빛 마스크를 만든다.

| 원본 | 방위 | 담은 것 |
|---|---|---|
| `SkylineNorth_20260928.png` | 북(+Y) | 산비탈을 따라 올라가는 붉은 벽돌 빌라촌, 교회 십자가 하나, 꼭대기의 낮은 산 능선 |
| `SkylineEast_20260928.png` | 동(+X) | 빌라 옥상 너머 1 km쯤의 아파트 단지. 해 뜨는 쪽이다 |
| `SkylineSouth_20260928.png` | 남(-Y) | 4차로 큰길과 상가 건물, 불 켜진 편의점 하나와 버스 정류장 |
| `SkylineWest_20260928.png` | 서(-X) | 초록 방수 도장 옥상들, 붉은 네온 십자가를 단 작은 교회, 긴 산 능선 |

## 게임에 들어간 방식

- 네 장은 1536×1024로 맞춰 북·동·남·서 순서로 세로로 쌓는다(`NightSkyline_D.png`).
  생성기가 하늘 가장자리에 남긴 반투명 안개는 걷고, 투명한 곳의 색은 가장자리
  색으로 채워 밉맵에서 검은 테두리가 번지지 않게 한다.
- 불빛 마스크(`NightSkyline_M.png`)는 R에 불빛 세기, G에 창마다 다른 문턱값,
  B에 종류(보통 창·늘 켜진 불·TV)를 담는다. 가로등, 붉은 십자가, 항공 장애등,
  편의점과 정류장 줄은 늘 켜진 불로 따로 뺀다.
- `M_NightSkyline`은 판의 UV가 아니라 시선 방향으로 그림을 찾는다. 방위각 90도마다
  한 장이고 이음매 좌우 4도는 두 장을 섞는다. 그림은 무한히 먼 곳에 붙은 셈이라
  옥상을 걸어도 따라 밀리지 않는다.
- 창은 동네가 깨어 있는 정도만큼 켜진다. 입주한 저녁에는 대부분, 04:30에는 몇
  집만 켜져 있다가 15분에 걸쳐 새벽 출근하는 집부터 하나둘 켜진다.
- 하늘이 빈 곳에는 `M_NightSkyGlow`가 지평선의 도시 불빛과 동쪽 끝의 새벽 기운을
  더한다. 그래야 건물 윤곽이 하늘을 등지고 검게 읽힌다.

## 프롬프트

넷 다 `background: transparent`로 뽑았다.

### 북쪽 — 산비탈 빌라촌

> Photorealistic night photograph, wide 3:2 landscape, taken at 4:30 a.m. in late July from the flat rooftop of a five-storey red-brick villa in an old Seoul hillside neighborhood, camera held level so the horizon sits exactly at the vertical middle of the frame, looking toward a hillside densely covered with four-storey red-brick multi-family villas stepping up the slope along narrow lanes, rooftops crowded with blue plastic water tanks, satellite dishes, clothes-drying racks and styrofoam planter boxes, about half of the windows lit in warm tungsten yellow and a few in cold fluorescent white, sparse amber streetlights along the lanes, one small red neon church cross on a rooftop, the dark ridge of a low mountain at the top of the hill, realistic low-light exposure with deep shadows and slight sensor grain, no people, no vehicles, no text, no readable signs, no logos, the entire sky above the skyline fully transparent with a clean hard silhouette edge, no sky, no stars, no moon, no clouds, no glow halo above the rooftops

### 동쪽 — 아파트 단지

> Photorealistic night photograph, wide 3:2 landscape, taken at 4:30 a.m. in late July from the rooftop of a five-storey villa in an old Seoul residential district, camera held level so the horizon sits exactly at the vertical middle of the frame, a foreground band of low red-brick villa rooftops with blue water tanks and TV antennas, and behind them about one kilometre away a row of fifteen- to twenty-five-storey apartment complex towers in pale concrete, the towers mostly dark with many scattered lit windows, vertical columns of dim stairwell lights at the tower ends, small red aviation warning lights on the tower roofs, a few amber streetlights between the buildings, realistic low-light exposure with deep shadows and slight sensor grain, no people, no text, no readable numbers or signs on the towers, no logos, the entire sky above the skyline fully transparent with a clean hard silhouette edge, no sky gradient, no stars, no clouds, no glow halo above the buildings

### 남쪽 — 큰길

> Photorealistic night photograph, wide 3:2 landscape, taken at 4:30 a.m. in late July from the rooftop of a five-storey villa in an old Seoul neighborhood, camera held level so the horizon sits exactly at the vertical middle of the frame, looking across a foreground of low tiled and flat rooftops toward a four-lane main road lined with three- and four-storey commercial buildings, the road lit by tall LED streetlights with a cool white cast and a few older orange sodium lamps, shop fronts shuttered and dark except one small convenience store glow and a lit bus stop shelter, upper-floor windows of the buildings with scattered lights on, tangled power lines crossing the frame, realistic low-light exposure with deep shadows and slight sensor grain, no people, no vehicles, no text, no readable signs, no logos, the entire sky above the skyline fully transparent with a clean hard silhouette edge, no sky, no stars, no clouds, no glow halo above the rooftops

### 서쪽 — 교회와 능선

> Photorealistic night photograph, wide 3:2 landscape, taken at 4:30 a.m. in late July from the rooftop of a five-storey villa in an old Seoul hillside neighborhood, camera held level so the horizon sits exactly at the vertical middle of the frame, looking across a dense jumble of red-brick villa rooftops painted with green urethane waterproof coating, rooftop vegetable planters, clothes lines and blue water tanks, toward a small brick church with a red neon cross on its steeple, and a long dark mountain ridge along the horizon with a few distant lights on its slope, about half of the windows lit in warm tungsten yellow, realistic low-light exposure with deep shadows and slight sensor grain, no people, no text, no readable signs, no logos, the entire sky above the skyline fully transparent with a clean hard silhouette edge, no sky, no stars, no moon, no clouds, no glow halo above the rooftops

## 나온 그림과 다른 점

프롬프트는 04:30 새벽을 적었지만 네 장 모두 해 질 녘처럼 창이 많이 켜지고 벽이
밝게 나왔다. 그래서 그림을 어둡게 눌러 쓰고, 창은 마스크로 걷어 냈다가 시간에
맞게 다시 켠다. 수평선도 장마다 한가운데에서 조금씩 벗어나 있어서, 재질은 네 장을
눈으로 맞춰 본 그림 높이 0.46 줄을 옥상 눈높이의 수평선으로 잡는다.

## 5층 옥탑 외벽 패널

바이블 §1의 5층은 옥상 슬래브 북측에 경량 철골로 무단 증축한 공간인데, 바깥에서 보면
실내 석고를 그대로 쓴 매끈한 상자였다. 한국 옥탑 증축에 가장 흔한 도장 강판 샌드위치
패널 외피를 한 장 뽑았다. 원본은 `Content/SourceArt/AI/AnnexSandwichPanel_20260928.png`.

생성물은 좌우 이음은 맞았지만 위아래를 붙이면 가운데에 옅은 띠가 생겼다.
`Scripts/build_annex_panel_texture.py`가 가로 리브 간격(24 px)의 정수배만큼 세로로 민
사본을 위아래 15%에만 섞어 이음을 지운다. 섞인 띠에서 피스가 흐리게 한 번 더 비치는데,
오래된 피스 구멍처럼 읽혀서 그대로 둔다.

> Use case: texture generation for a real-time game. Create ONE seamless, tileable, perfectly front-facing diffuse albedo texture of the exterior wall of a cheap Korean rooftop extension built from factory-painted steel sandwich panels with an EPS core. The whole image is the wall surface and covers exactly 2000 mm wide by 1333 mm tall: two vertical panels side by side, each exactly 1000 mm wide, with the interlocking vertical panel joints exactly on the left image edge, the exact horizontal centre and the right image edge, so horizontal tiling joins perfectly. Off-white warm light grey paint over steel, very shallow horizontal micro-ribs every 20 mm across both panels, a vertical column of small hex-head self-tapping screws with dark rubber washers along each joint at even 333 mm spacing so the top and bottom image edges fall halfway between screws and vertical tiling is seamless. Restrained realistic weathering from ten years on a Seoul rooftop: faint grey rain streaks, thin orange-brown rust bleeding down from two or three screw heads, light grime, one or two small dents, a few scuffs. Completely flat even diffuse illumination, no directional light, no cast shadows, no ambient-occlusion halos, no perspective, no vignetting, no glossy highlights. No text, no logos, no stickers, no windows, no doors, no border, no reference-sheet layout. Landscape 3:2. High resolution, sharp enough for a 1 m viewing distance.

## 골목 이웃 창 뒤의 방

골목 이웃 건물의 창 여덟 개가 어두운 유리 판이었다. 창 뒤에 방이 있는 것처럼 보이게
하려고(인테리어 매핑) 창 밖에서 정면으로 찍은 1점 투시 방 사진 네 장을 뽑았다. 뒷벽은
화면과 평행하고 소실점은 정중앙, 가구는 뒷벽과 옆벽에만 붙게 했다. 네 장 모두 뒷벽이
사진 폭의 절반쯤(0.47~0.6)을 차지해서 재질은 0.5로 잡는다.
`Scripts/build_room_interior_atlas.py`가 2x2로 묶고, `M_RoomInterior`가 창마다 방 하나를
골라 원근대로 편다. 네 장 모두 공통 문장 뒤에 방마다 다른 한 문장을 붙였다.

공통 문장:

> Use case: interior-mapping texture for a real-time game window. Photorealistic night photograph of the inside of one small room of an ordinary 1990s Korean villa apartment, taken from exactly outside its window looking straight in. Strict one-point perspective: camera centred and level, the back wall perfectly parallel to the image plane and filling about the central half of the frame, the single vanishing point exactly at the image centre, the floor, ceiling and both side walls converging symmetrically toward it, the room opening filling the frame edge to edge. No window frame, no glass, no curtains or blinds in the foreground, nothing sticking out toward the camera; all furniture stands flat against the back wall or tight against the side walls.

### 0번 — 스탠드 켠 침실

> A small bedroom lit only by a warm tungsten bedside lamp on a low table in the back-left corner: a white sliding-door wardrobe covering the back wall, a thin folded blanket on a low bed along the left wall, beige floral wallpaper, yellow vinyl sheet floor, a calendar on the right wall. Realistic low-light exposure, slight sensor grain, lived-in and ordinary. No people, no text, no readable labels, no logos, no watermark. Square 1:1.

### 1번 — TV만 켠 거실

> A living room lit mostly by the cold blue flicker of a television standing on a low cabinet against the centre of the back wall, a little warm spill from a doorway on the right wall, a wall clock above the television, beige wallpaper, light laminate floor, a folded drying rack against the left wall. Realistic low-light exposure, slight sensor grain, lived-in and ordinary. No people, no text, no readable labels, no logos, no watermark. Square 1:1.

### 2번 — 형광등 켠 부엌

> A narrow kitchen lit by a cold white fluorescent ceiling tube: a small white refrigerator and a short run of white wall cabinets and sink against the back wall, a rice cooker with a tiny orange light on the counter, beige wall tiles, a plastic stool against the right wall. Realistic low-light exposure, slight sensor grain, lived-in and ordinary. No people, no text, no readable labels, no logos, no watermark. Square 1:1.

### 3번 — 스탠드 켠 공부방

> A student's study room lit by a single desk lamp: a desk with a closed laptop and stacked books against the back wall, a bookshelf against the right wall, pale grey wallpaper, dark laminate floor, a hoodie hanging on a hook on the left wall. Realistic low-light exposure, slight sensor grain, lived-in and ordinary. No people, no text, no readable labels, no logos, no watermark. Square 1:1.
