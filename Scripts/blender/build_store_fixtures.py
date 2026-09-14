"""편의점 집기. 씬(BuildStore)의 상자 조립을 실제 집기 메시로 바꾼다.

기준: Content/SourceArt/AI/SheetStoreFixturesReference.png (음료 냉장고, 곤돌라,
평대 냉동고). 좌표계는 씬 그대로(cm → m)이고, 선반 상판 높이는 씬이 상품을
올려놓는 값을 정확히 지킨다 — 병·컵·봉지는 씬 코드가 그 높이에 그대로 놓는다.
원점은 각 집기 발자국의 바닥 중심이고 Z 0이 매장 바닥 윗면(씬 Z 6)이다.

    SM_StoreCoolerBank  동쪽 벽 6칸 워크인 음료 냉장고. 두 번째 칸은 문이 없다(열린 칸)
    SM_StoreCoolerDoor  열린 칸의 문짝. 원점이 힌지 축(문틀 바깥선 바닥)
    SM_StoreGondola     양면 곤돌라 5단, 두 대
    SM_StoreCounter     계산대 몸통·상판·발치 홈. 바운드가 상판 윗면(씬 Z 99)에서 끝난다
    SM_CardTerminal     상판 위 카드 단말기. 원점 상판 윗면 (2500, -268, 99)
    SM_HotSnackWarmer   계산대 온장고. 원점 상판 윗면 (2510, -198, 99), 앞면 -Y
    SM_ChestFreezer     서쪽 유리벽 앞 아이스크림 평대 냉동고
    SM_OpenShowcase     남쪽 벽 오픈 쇼케이스(김밥·샌드위치)
    SM_RamyeonRack      창가 라면 코너 선반

    blender -b --factory-startup --python Scripts/blender/build_store_fixtures.py -- <out_dir> [cooler|door|gondola|counter|freezer|showcase|rack ...]
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402

FLOOR_Z = 6.0   # 매장 바닥 윗면(씬 cm)


def mats():
    return {
        "black": ig.mat_painted_steel("BlackFrame", (0.02, 0.02, 0.022), roughness=0.42, wear=0.12, bump=0.04),
        "grey": ig.mat_painted_steel("LightGreySteel", (0.70, 0.70, 0.68), roughness=0.48, wear=0.18),
        "white": ig.mat_painted_steel("WhiteEnamel", (0.86, 0.86, 0.84), roughness=0.30, wear=0.10, bump=0.02),
        "liner": ig.mat_plastic("Liner", (0.72, 0.73, 0.72), roughness=0.55),
        "laminate": ig.mat_plastic("Laminate", (0.60, 0.58, 0.54), roughness=0.38, bump=0.01),
        "dark": ig.mat_plastic("DarkPlastic", (0.02, 0.02, 0.022), roughness=0.5),
        "price": ig.mat_plastic("PriceRail", (0.90, 0.90, 0.88), roughness=0.35),
        "steel": ig.mat_metal("Stainless", (0.78, 0.78, 0.77), roughness=0.36, streak=0.1),
        "chrome": ig.mat_metal("Chrome", (0.86, 0.86, 0.86), roughness=0.2, streak=0.05, anisotropic=False),
        "alu": ig.mat_metal("Aluminium", (0.62, 0.62, 0.62), roughness=0.4, streak=0.08),
        "glass": ig.mat_glass("Glass"),
        "header": ig.mat_emissive("HeaderLight", (1.0, 0.98, 0.94), strength=2.6),
        "led": ig.mat_emissive("LedStrip", (0.9, 0.95, 1.0), strength=4.0),
        "heat": ig.mat_emissive("HeatLamp", (1.0, 0.42, 0.10), strength=5.0),
        "screen": ig.mat_emissive("Screen", (0.30, 0.55, 0.85), strength=3.0),
    }


def local(origin_cm):
    """씬 cm 좌표를 이 에셋의 로컬 m로 바꾸는 함수를 만든다."""
    ox, oy = origin_cm

    def convert(x, y, z):
        return ((x - ox) / 100.0, (y - oy) / 100.0, (z - FLOOR_Z) / 100.0)
    return convert


def span(lo_cm, hi_cm):
    """씬 cm 구간 → (중심 m, 길이 m). Z에는 쓰지 말 것(바닥 기준이 다르다)."""
    return ((lo_cm + hi_cm) * 0.5 / 100.0, (hi_cm - lo_cm) / 100.0)


# --------------------------------------------------------------------------
# 음료 냉장고
# --------------------------------------------------------------------------

BAY_CENTERS = [-630.0, -552.0, -474.0, -396.0, -318.0, -240.0]   # 씬 Y
OPEN_BAY = 1
COOLER_ORIGIN = (2925.0, -430.0)
DOOR_W = 0.80      # 문짝 바깥 폭(문틀 포함)
DOOR_H = 2.12      # 바닥에서 문틀 윗선까지
FRONT_X = -0.25    # 문틀 중심면(씬 X 2900)


def cooler_door_parts(m, y0, name_prefix, x0=0.0, with_handle=True):
    """문짝 하나. y0은 힌지 쪽 바깥선(로컬 Y), 문짝은 +Y로 뻗고 유리는 -X를 본다.
    x0이 문틀 중심면이다(문짝 에셋은 0, 냉장고 안에서는 FRONT_X)."""
    parts = []
    jamb_w, rail_h, depth = 0.06, 0.04, 0.08
    z0, z1 = 0.0, DOOR_H
    # 검정 아노다이징 알루미늄 문틀: 세로 둘, 가로 둘.
    for y in (y0 + jamb_w * 0.5, y0 + DOOR_W - jamb_w * 0.5):
        parts.append(ig.box(f"{name_prefix}_jamb", (depth, jamb_w, z1 - z0),
                            location=(x0, y, (z0 + z1) * 0.5), bevel=0.004, segments=1, material=m["black"]))
    for z in (z0 + rail_h * 0.5, z1 - rail_h * 0.5):
        parts.append(ig.box(f"{name_prefix}_rail", (depth, DOOR_W - jamb_w * 2.0, rail_h),
                            location=(x0, y0 + DOOR_W * 0.5, z), bevel=0.004, segments=1, material=m["black"]))
    # 유리. 문틀보다 1 cm 앞(-X)에 든다. 두께 1 cm짜리 진짜 판이다.
    parts.append(ig.box(f"{name_prefix}_glass", (0.01, DOOR_W - jamb_w * 2.0 + 0.02, z1 - z0 - rail_h * 2.0 + 0.02),
                        location=(x0 - 0.01, y0 + DOOR_W * 0.5, (z0 + z1) * 0.5), material=m["glass"]))
    if with_handle:
        # 세로 크롬 손잡이. 자유단(+Y) 쪽, 씬이 두던 자리(칸 중심 +28)와 같다.
        hy = y0 + DOOR_W - 0.12
        parts.append(ig.cylinder(f"{name_prefix}_handle", 0.012, 0.44, location=(x0 - 0.055, hy, 1.04),
                                 segments=16, material=m["chrome"]))
        for z in (0.84, 1.24):
            parts.append(ig.box(f"{name_prefix}_handle_foot", (0.03, 0.02, 0.02),
                                location=(x0 - 0.04, hy, z), material=m["chrome"]))
    return parts


def build_cooler(out_root):
    ig.reset_scene()
    m = mats()
    L = local(COOLER_ORIGIN)
    parts, groups = [], []
    half_w = 2.40                       # 씬 Y -670..-190
    x_back, x_front = 0.25, -0.30       # 뒷판 앞면(씬 2950), 앞면 최전방
    top = 2.20

    # 캐비닛 껍데기: 뒷판, 양 끝판, 지붕, 하부 그릴 받침.
    back = ig.box("back", (0.05, half_w * 2.0, top), location=(x_back - 0.025, 0.0, top * 0.5),
                  material=m["black"])
    parts.append(back)
    ends = []
    # 끝판 바깥선은 정확히 씬 벽면(Y -680 / -180)에 닿는다.
    for y in (-half_w - 0.05, half_w + 0.05):
        end = ig.box("end", (0.55, 0.10, top), location=(-0.025, y, top * 0.5), bevel=0.005, segments=1,
                     material=m["black"])
        parts.append(end)
        ends.append(end)
    roof = ig.box("roof", (0.55, half_w * 2.0 + 0.20, 0.06), location=(-0.025, 0.0, top + 0.03), bevel=0.004,
                  segments=1, material=m["black"])
    parts.append(roof)
    # 상단 라이트 박스: 문틀 윗선(2.12)과 지붕 윗면(2.26) 사이, 앞면 전체가 빛나는 유백색 판.
    header = ig.box("header_box", (0.14, half_w * 2.0, 0.14), location=(x_front + 0.07, 0.0, 2.19),
                    material=m["black"])
    parts.append(header)
    header_face = ig.box("header_face", (0.004, half_w * 2.0 - 0.02, 0.11), location=(x_front + 0.002, 0.0, 2.19),
                         material=m["header"])
    parts.append(header_face)
    # 바닥 그릴: 검정 몸통에 가로 슬랫.
    base = ig.box("base", (0.53, half_w * 2.0, 0.20), location=(-0.015, 0.0, 0.10), material=m["dark"])
    parts.append(base)
    for z in (0.05, 0.10, 0.15):
        parts.append(ig.box("louvre", (0.012, half_w * 2.0 - 0.04, 0.018), location=(x_front + 0.006, 0.0, z),
                            material=m["black"]))
    groups.append([back])
    groups.append([ends[0]])
    groups.append([ends[1]])
    groups.append([roof, header])
    groups.append([base])

    # 칸마다 라이너, 선반 셋, 가격표, LED, 문.
    for index, bay_y in enumerate(BAY_CENTERS):
        cy = (bay_y - COOLER_ORIGIN[1]) / 100.0
        bay_parts = []
        liner = ig.box("liner", (0.03, 0.72, 1.90), location=(x_back - 0.065, cy, 0.20 + 0.95), material=m["liner"])
        bay_parts.append(liner)
        for y in (cy - 0.36 - 0.015, cy + 0.36 + 0.015):
            bay_parts.append(ig.box("bay_wall", (0.40, 0.03, 1.90), location=(0.0, y, 0.20 + 0.95),
                                    material=m["liner"]))
        shelves = []
        for shelf_z in (60.0, 105.0, 150.0):
            zc = (shelf_z - FLOOR_Z) / 100.0            # 판 중심. 윗면이 씬 +1.5
            plate = ig.box("shelf", (0.44, 0.68, 0.03), location=(0.0, cy, zc), bevel=0.003, segments=1,
                           material=m["grey"])
            shelves.append(plate)
            # 가격표: 어두운 캐리어에 흰 띠. 씬 X 2902.6 / 2901.2.
            bay_parts.append(ig.box("carrier", (0.022, 0.66, 0.06), location=(-0.224, cy, zc + 0.03),
                                    material=m["dark"]))
            bay_parts.append(ig.box("strip", (0.008, 0.62, 0.042), location=(-0.238, cy, zc + 0.03),
                                    material=m["price"]))
        bay_parts.extend(shelves)
        # 칸 조명: 헤더 밑에 붙은 LED 바.
        bay_parts.append(ig.box("bay_led", (0.03, 0.60, 0.02), location=(-0.20, cy, 2.10), material=m["led"]))
        parts.extend(bay_parts)
        if index == OPEN_BAY:
            # 열린 칸: 문틀 세로대만 남기고(문짝은 SM_StoreCoolerDoor) 선반마다 충돌.
            for plate in shelves:
                groups.append([plate])
            groups.append([liner])
        else:
            door = cooler_door_parts(m, cy - DOOR_W * 0.5, f"door{index}", x0=FRONT_X)
            parts.extend(door)
            groups.append(door + [liner])
    # 문 사이 세로 멀리언. 칸 경계(칸 중심 ±39)에 6 cm.
    for y in [(BAY_CENTERS[0] - COOLER_ORIGIN[1]) / 100.0 - 0.40] + \
             [((a + b) * 0.5 - COOLER_ORIGIN[1]) / 100.0 for a, b in zip(BAY_CENTERS, BAY_CENTERS[1:])] + \
             [(BAY_CENTERS[-1] - COOLER_ORIGIN[1]) / 100.0 + 0.40]:
        parts.append(ig.box("mullion", (0.08, 0.06, DOOR_H), location=(FRONT_X, y, DOOR_H * 0.5), bevel=0.004,
                            segments=1, material=m["black"]))
    return ig.build_asset(
        "SM_StoreCoolerBank", "large", parts, out_root,
        collision_parts=groups,
        notes=("편의점 동쪽 벽 6칸 음료 냉장고. 앞면 -X, 원점은 발자국 바닥 중심(씬 (2925, -430, 6)). "
               "선반 윗면은 씬 Z 61.5/106.5/151.5. 두 번째 칸(씬 Y -552)은 문이 없고 "
               "SM_StoreCoolerDoor를 힌지 (2900, -592, 6)에 yaw 120으로 놓는다. 열린 칸은 선반만 충돌."),
        texture_size=2048, preview_yaw=60.0)


def build_cooler_door(out_root):
    ig.reset_scene()
    m = mats()
    parts = cooler_door_parts(m, 0.0, "door")
    return ig.build_asset(
        "SM_StoreCoolerDoor", "prop", parts, out_root,
        collision_parts=[parts],
        notes=("음료 냉장고 열린 칸의 문짝. 원점은 힌지 축 바닥(문틀 바깥선, 씬 X 2900 문틀 중심면). "
               "닫힌 자세는 문짝이 +Y로 80 cm, 유리는 -X. yaw 120으로 통로 쪽에 젖혀 놓는다."),
        texture_size=1024)


# --------------------------------------------------------------------------
# 곤돌라
# --------------------------------------------------------------------------

TIER_Z = (30.0, 60.0, 90.0, 120.0, 150.0)   # 씬 Z, 판 중심


def build_gondola(out_root):
    from build_retail_refresh import gondola
    gondola(out_root)


def build_counter(out_root):
    from build_retail_refresh import counter
    counter(out_root)


def build_terminal(out_root):
    """카드 단말기. 원점은 상판 윗면 중심(씬 (2500, -268, 99)), 화면이 손님 쪽 -Y."""
    ig.reset_scene()
    m = mats()
    parts = []
    parts.append(ig.box("terminal", (0.12, 0.09, 0.05), location=(0.0, 0.0, 0.025), bevel=0.004, segments=1,
                        material=m["dark"]))
    # 화면 머리는 몸통 앞(-Y) 위에서 28도 젖혀진다.
    tx, ty, tz = 0.0, -0.03, 0.07
    parts.append(ig.box("terminal_head", (0.10, 0.02, 0.07), location=(tx, ty, tz),
                        rotation=(-math.radians(28.0), 0.0, 0.0), bevel=0.002, segments=1, material=m["dark"]))
    parts.append(ig.box("terminal_screen", (0.08, 0.002, 0.05), location=(tx, ty - 0.010, tz + 0.005),
                        rotation=(-math.radians(28.0), 0.0, 0.0), material=m["screen"]))
    parts.append(ig.box("keypad", (0.08, 0.05, 0.004), location=(0.0, 0.015, 0.052), material=m["dark"]))
    return ig.build_asset(
        "SM_CardTerminal", "prop", parts, out_root,
        collision_parts=[],
        notes="계산대 카드 단말기 12 x 11 x 11. 원점 상판 윗면 중심(씬 (2500, -268, 99)), 화면 -Y. 충돌 없음.",
        texture_size=512)


def build_warmer(out_root):
    """SMAD 35L 제품 사진의 유리 케이스·철망·받침 구조. 출처는 검수 문서에 남긴다."""
    ig.reset_scene()
    m = mats()
    # 새벽에는 비어 있고 전원도 꺼져 있다. 유리 안에 불투명 몸통을 겹치지 않는다.
    parts = [ig.box("스테인리스받침", (.550, .340, .082), (0, 0, .059),
                    bevel=.002, segments=2, material=m["steel"])]
    for x in (-.240, .240):
        for y in (-.140, .140):
            parts.append(ig.cylinder("고무발", .013, .018, (x, y, .009), segments=12, material=m["dark"]))
    parts.append(ig.box("상부유리", (.554, .350, .006), (0, 0, .307), material=m["glass"]))
    for x in (-.275, .275):
        parts.append(ig.box("측면유리", (.004, .342, .206), (x, 0, .202), material=m["glass"]))
    for y in (-.1715, .1715):
        parts.append(ig.box("유리문", (.544, .004, .204), (0, y, .202), material=m["glass"]))
        parts.append(ig.cylinder("문손잡이", .010, .007, (0, math.copysign(.177, y), .132),
                                 rotation=(math.pi/2, 0, 0), segments=16, bevel=.001, material=m["chrome"]))
        for z in (.125, .282):
            parts.append(ig.box("문경첩", (.014, .012, .021), (-.264, y, z),
                                bevel=.001, material=m["chrome"]))
    for x in (-.258, .258):
        for y in (-.153, .153):
            parts.append(ig.box("유리고정쇠", (.010, .010, .024), (x, y, .292), material=m["chrome"]))
            parts.append(ig.cylinder("고정나사", .0035, .002, (x, y, .310), segments=10, material=m["chrome"]))
    # 선반은 철망 한 장과 양쪽 지지대로 구성한다. 가열부는 그 아래에 들어간다.
    for x in (-.251, .251):
        parts.append(ig.box("선반지지대", (.006, .300, .012), (x, 0, .113), material=m["steel"]))
    for y in (-.145, .145):
        parts.append(ig.cylinder("선반테두리", .0025, .506, (0, y, .121),
                                 rotation=(0, math.pi/2, 0), segments=8, material=m["chrome"]))
    for index in range(27):
        parts.append(ig.cylinder("철망세로", .0013, .290, (-.247 + index*.019, 0, .121),
                                 rotation=(math.pi/2, 0, 0), segments=6, material=m["chrome"]))
    for index in range(9):
        parts.append(ig.cylinder("철망가로", .0013, .500, (0, -.132 + index*.033, .119),
                                 rotation=(0, math.pi/2, 0), segments=6, material=m["chrome"]))
    # 전원 스위치와 온도 조절 손잡이는 직원이 서는 +Y 면에 둔다.
    parts.append(ig.box("스위치테두리", (.026, .005, .017), (-.204, .172, .061), material=m["dark"]))
    switch_mat = ig.mat_plastic("꺼진스위치", (.17, .025, .015), roughness=.42, bump=0)
    parts.append(ig.box("전원스위치", (.018, .003, .011), (-.204, .176, .061), material=switch_mat))
    parts.append(ig.cylinder("온도조절손잡이", .015, .010, (-.151, .175, .061),
                             rotation=(math.pi/2, 0, 0), segments=20, bevel=.001, material=m["dark"]))
    parts.append(ig.box("손잡이눈금", (.0015, .001, .008), (-.151, .1805, .065), material=m["price"]))
    return ig.build_asset(
        "SM_HotSnackWarmer", "prop", parts, out_root,
        collision_parts=[parts],
        notes="SMAD 35L 제품 사진을 참고한 55.4 x 36.2 x 31.1cm 온장고. 유리 케이스·철망 선반·고무발. 앞면 -Y, 조절부 +Y. 새벽에는 전원이 꺼진 빈 상태.",
        texture_size=1024, preview_yaw=30.0)


# --------------------------------------------------------------------------
# 평대 냉동고
# --------------------------------------------------------------------------

def build_freezer(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    body = ig.box("body", (0.58, 1.10, 0.74), location=(0.0, 0.0, 0.06 + 0.37), bevel=0.012, segments=3,
                  material=m["white"])
    parts.append(body)
    parts.append(ig.box("plinth", (0.54, 1.06, 0.06), location=(0.0, 0.0, 0.03), material=m["dark"]))
    # 윗테와 두 장의 슬라이딩 유리 뚜껑(살짝 겹친다).
    rim = ig.box("rim", (0.58, 1.10, 0.04), location=(0.0, 0.0, 0.82), bevel=0.004, segments=1,
                 material=m["alu"])
    parts.append(rim)
    cavity = ig.box("rim_cut", (0.50, 1.02, 0.06), location=(0.0, 0.0, 0.82))
    ig.boolean(rim, cavity, "DIFFERENCE")
    parts.append(ig.box("lid_a", (0.50, 0.53, 0.008), location=(0.0, -0.26, 0.835), material=m["glass"]))
    parts.append(ig.box("lid_b", (0.50, 0.53, 0.008), location=(0.0, 0.26, 0.845), material=m["glass"]))
    for y in (-0.26, 0.26):
        parts.append(ig.box("lid_frame", (0.50, 0.53, 0.012), location=(0.0, y, 0.84 if y < 0 else 0.85),
                            material=m["alu"]))
        cut = ig.box("lid_frame_cut", (0.46, 0.49, 0.02), location=(0.0, y, 0.845))
        ig.boolean(parts[-1], cut, "DIFFERENCE")
    parts.append(ig.box("lid_grip", (0.46, 0.03, 0.02), location=(0.0, 0.01, 0.86), material=m["alu"]))
    # 온도 조절부: 한쪽 끝 아래 검정 패널과 작은 표시창.
    parts.append(ig.box("control", (0.10, 0.06, 0.05), location=(0.0, 0.53, 0.20), bevel=0.003, segments=1,
                        material=m["dark"]))
    parts.append(ig.box("control_led", (0.03, 0.004, 0.015), location=(0.0, 0.562, 0.21), material=m["screen"]))
    parts.append(ig.box("lock", (0.03, 0.01, 0.03), location=(0.0, 0.556, 0.80), material=m["chrome"]))
    return ig.build_asset(
        "SM_ChestFreezer", "prop", parts, out_root,
        collision_parts=[parts],
        notes="편의점 아이스크림 평대 냉동고 58 x 110 x 86. 원점 바닥 중심(씬 (2445, -545, 6)). 긴 축이 Y.",
        texture_size=1024)


# --------------------------------------------------------------------------
# 오픈 쇼케이스
# --------------------------------------------------------------------------

def build_showcase(out_root):
    ig.reset_scene()
    m = mats()
    L = local((2700.0, -660.0))
    parts = []
    # 껍데기: 등판(벽 쪽 -Y), 양 옆, 지붕, 바닥. 앞(+Y)이 열려 있다.
    back = ig.box("back", (2.40, 0.06, 1.70), location=(0.0, -0.17, 0.85), bevel=0.004, segments=1,
                  material=m["white"])
    parts.append(back)
    for x in (-1.17, 1.17):
        parts.append(ig.box("side", (0.06, 0.34, 1.70), location=(x, 0.0, 0.85), bevel=0.004, segments=1,
                            material=m["white"]))
    parts.append(ig.box("roof", (2.28, 0.34, 0.06), location=(0.0, 0.0, 1.67), material=m["white"]))
    parts.append(ig.box("floor", (2.28, 0.34, 0.06), location=(0.0, 0.0, 0.03), material=m["white"]))
    # 앞 캐노피: 지붕 앞에 살짝 내려온 띠, 그 밑에 LED.
    parts.append(ig.box("canopy", (2.28, 0.03, 0.12), location=(0.0, 0.16, 1.58), bevel=0.003, segments=1,
                        material=m["white"]))
    parts.append(ig.box("led", (2.24, 0.03, 0.012), location=(0.0, 0.12, 1.595), material=m["led"]))
    # 라이너: 안쪽 벽은 스테인리스.
    parts.append(ig.box("liner", (2.28, 0.01, 1.58), location=(0.0, -0.135, 0.85), material=m["steel"]))
    # 선반 셋과 가격표. 판 윗면 씬 71.5/106.5/141.5.
    for tier in (70.0, 105.0, 140.0):
        zc = (tier - FLOOR_Z) / 100.0
        parts.append(ig.box("shelf", (2.28, 0.26, 0.03), location=(0.0, 0.06, zc), bevel=0.003, segments=1,
                            material=m["steel"]))
        parts.append(ig.box("price", (2.28, 0.03, 0.05), location=(0.0, 0.135, zc + 0.03), material=m["price"]))
    # 하단 그릴.
    for z in (0.10, 0.16):
        parts.append(ig.box("grille", (2.20, 0.01, 0.02), location=(0.0, 0.175, z), material=m["dark"]))
    return ig.build_asset(
        "SM_OpenShowcase", "large", parts, out_root,
        collision_parts=[[p for p in parts if p.name.startswith(("back", "side", "roof", "floor", "canopy"))]],
        notes=("편의점 남쪽 벽 오픈 쇼케이스 240 x 40 x 170. 앞면 +Y(통로 쪽), 원점 바닥 중심(씬 (2700, -660, 6)). "
               "선반 윗면 씬 Z 71.5/106.5/141.5, 김밥·샌드위치는 씬 코드가 Y -655에 놓는다."),
        texture_size=2048, preview_yaw=210.0)


# --------------------------------------------------------------------------
# 라면 선반
# --------------------------------------------------------------------------

def build_rack(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    parts.append(ig.box("back", (0.70, 0.03, 1.60), location=(0.0, -0.145, 0.80), material=m["grey"]))
    for x in (-0.335, 0.335):
        parts.append(ig.box("upright", (0.03, 0.32, 1.60), location=(x, 0.0, 0.80), bevel=0.003, segments=1,
                            material=m["grey"]))
    for tier in (16.0, 46.0, 76.0, 106.0, 136.0, 166.0):
        zc = (tier - FLOOR_Z) / 100.0
        parts.append(ig.box("tier", (0.70, 0.32, 0.03), location=(0.0, 0.0, zc), bevel=0.003, segments=1,
                            material=m["grey"]))
        if tier < 166.0:
            parts.append(ig.box("lip", (0.70, 0.015, 0.04), location=(0.0, 0.1525, zc + 0.02),
                                material=m["price"]))
    return ig.build_asset(
        "SM_RamyeonRack", "prop", parts, out_root,
        collision_parts=[parts],
        notes=("편의점 창가 라면 선반 70 x 32 x 162. 앞면 +Y, 원점 바닥 중심(씬 (2452, -664, 6)). "
               "단 윗면 씬 Z 17.5/47.5/…/167.5, 컵은 씬 코드가 놓는다."),
        texture_size=1024, preview_yaw=210.0)


BUILDERS = {
    "cooler": build_cooler,
    "door": build_cooler_door,
    "gondola": build_gondola,
    "counter": build_counter,
    "terminal": build_terminal,
    "warmer": build_warmer,
    "freezer": build_freezer,
    "showcase": build_showcase,
    "rack": build_rack,
}


def main():
    out_root = ig.out_root_from_argv()
    only = [a for a in sys.argv[sys.argv.index("--") + 2:]] if "--" in sys.argv else []
    for key, builder in BUILDERS.items():
        if not only or key in only:
            builder(out_root)


if __name__ == "__main__":
    main()
