"""403호 창문·블라인드·현관 벽 설비. 씬(BuildApartment)의 상자 조립을 메시로 바꾼다.

기준: Content/SourceArt/AI/SheetOneroomKitchenAppliancesReference.png 우하(인터폰·스위치),
인터폰 앞면 텍스처는 panels/IntercomFace.png. 좌표계는 씬 그대로(cm → m).

    SM_ApartmentWindow  북쪽 벽 미닫이 창틀. 원점 (-100, 212.5, 100) 창 아래 중심. 유리는 없다 —
                        장면에서 두 창짝 안쪽에 유리를 따로 끼운다
    SM_VenetianBlind    걷어 올린 베네시안 블라인드. 원점은 헤드레일 윗면 중심 (-100, 207, 206)
    SM_VideoIntercom    비디오 인터폰. 원점은 벽면 바닥 중심 (62, -215, 130), 앞면 +Y
    SM_WallSwitch       2구 스위치. 원점 (90, -215, 123), 앞면 +Y. (T_SwitchPlate_D 간판 텍스처와 이름이 겹쳐 SwitchPlate를 쓰지 않는다)
    SM_ShoeCabinet      현관 신발장 80 x 34 x 113. 원점 바닥 중심 (48, -198, 0), 문이 +Y

    blender -b --factory-startup --python Scripts/blender/build_apartment_fixtures.py -- <out_dir> [window|blind|intercom|switch|shoe ...]
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import ig_blender_lib as ig  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
PANELS = os.path.join(REPO, "Content", "SourceArt", "AI", "panels")
FACING_POS_Y = (0.0, 0.0, math.pi)   # image_quad는 -Y를 본다. yaw 180이면 +Y.


def mats():
    return {
        "pvc": ig.mat_plastic("WhitePvc", (0.90, 0.90, 0.88), roughness=0.36, bump=0.01),
        "alu": ig.mat_metal("Aluminium", (0.66, 0.66, 0.65), roughness=0.42, streak=0.08),
        "slat": ig.mat_plastic("BlindSlat", (0.92, 0.92, 0.90), roughness=0.55, bump=0.01),
        "white": ig.mat_plastic("WhitePlastic", (0.88, 0.88, 0.86), roughness=0.42, bump=0.01),
        "gloss": ig.mat_painted_steel("GlossWhite", (0.86, 0.86, 0.84), roughness=0.16, wear=0.08, bump=0.008,
                                      dirt_color=(0.55, 0.53, 0.5)),
        "dark": ig.mat_plastic("DarkPlastic", (0.02, 0.02, 0.022), roughness=0.5),
        "chrome": ig.mat_metal("Chrome", (0.86, 0.86, 0.86), roughness=0.22, streak=0.05, anisotropic=False),
        "led": ig.mat_emissive("StatusLed", (0.2, 1.0, 0.4), strength=4.0),
        "intercom": ig.mat_image_uv("IntercomFace", os.path.join(PANELS, "IntercomFace.png"), roughness=0.4),
    }


# --------------------------------------------------------------------------
# 창틀
# --------------------------------------------------------------------------

def build_window(out_root):
    """씬 창틀: 머리 130x5x7 Z 196, 밑틀 Z 104, 세로틀 X -163/-37, 가운데 세로·가로 살.
    로컬 원점은 (-100, 212, 100). 창 안쪽 폭 X ±0.60, 높이 Z 0.075..0.925."""
    ig.reset_scene()
    m = mats()
    parts = []
    depth = 0.10
    # 바깥 틀: 머리·밑틀·양 세로틀. 흰 PVC 프로파일, 모서리 2 mm.
    parts.append(ig.box("head", (1.30, depth, 0.07), location=(0.0, 0.0, 0.96), bevel=0.002, segments=1,
                        material=m["pvc"]))
    parts.append(ig.box("sill", (1.30, depth, 0.07), location=(0.0, 0.0, 0.04), bevel=0.002, segments=1,
                        material=m["pvc"]))
    for x in (-0.63, 0.63):
        parts.append(ig.box("jamb", (0.07, depth, 0.99), location=(x, 0.0, 0.50), bevel=0.002, segments=1,
                            material=m["pvc"]))
    # 밑틀 안쪽 창턱: 실내로 2 cm 나온 턱과 물끊기.
    parts.append(ig.box("stool", (1.34, 0.08, 0.015), location=(0.0, -0.02, 0.0775), bevel=0.002, segments=1,
                        material=m["pvc"]))
    # 미닫이 두 짝. 안짝(-Y)과 바깥짝(+Y)이 다른 레일에 있고 가운데서 겹친다.
    sash_w, sash_h = 0.62, 0.85
    for index, (x, y) in enumerate(((-0.30, -0.031), (0.30, 0.008))):
        zc = 0.075 + sash_h * 0.5
        frame_parts = []
        for sx in (x - sash_w * 0.5 + 0.02, x + sash_w * 0.5 - 0.02):
            frame_parts.append(ig.box(f"sash{index}_stile", (0.04, 0.022, sash_h), location=(sx, y, zc),
                                      bevel=0.0015, segments=1, material=m["pvc"]))
        for sz in (zc - sash_h * 0.5 + 0.02, zc + sash_h * 0.5 - 0.02):
            frame_parts.append(ig.box(f"sash{index}_rail", (sash_w - 0.08, 0.022, 0.04), location=(x, y, sz),
                                      bevel=0.0015, segments=1, material=m["pvc"]))
        parts.extend(frame_parts)
    # 중앙 잠금대와 크리센트는 실물 70mm 받침·95mm 손잡이를 따른다.
    # 유리 위에 떠 있던 손잡이를 두 창짝이 맞물리는 곳으로 옮긴다.
    parts.append(ig.box("meeting_stile", (0.045, 0.040, 0.85), location=(0.0, -0.025, 0.50),
                        bevel=0.0015, segments=1, material=m["pvc"]))
    parts.append(ig.box("crescent_base", (.025,.006,.070), location=(-.004,-.049,.50),
                        bevel=.007, segments=3, material=m["alu"]))
    parts.append(ig.cylinder("crescent_pivot", .013, .010, location=(-.004,-.058,.50),
                            rotation=(math.pi/2,0,0),segments=16,material=m["alu"]))
    parts.append(ig.pipe("crescent_hook", [(-.004,-.060,.48),(.016,-.060,.486),(.020,-.060,.510),(.007,-.060,.521)],
                         .004, resolution=6, corner_radius=.007,corner_steps=4,material=m["alu"]))
    parts.append(ig.box("crescent_lever", (.012,.014,.060), location=(-.004,-.069,.473),
                        bevel=.004,segments=2,material=m["alu"]))
    for z in (.475,.525):
        parts.append(ig.cylinder("latch_screw",.0025,.002,location=(-.004,-.053,z),
                                rotation=(math.pi/2,0,0),segments=8,material=m["dark"]))
    # 고무 가스켓은 유리 가장자리에만 얇게 둘러진다.
    for x,y in ((-.30,-.031),(.30,.008)):
        for sx in (x-.269,x+.269):
            parts.append(ig.box("glazing_seal",(.003,.005,.767),(sx,y-.002,.50),material=m["dark"]))
        for z in (.1165,.8835):
            parts.append(ig.box("glazing_seal",(.535,.005,.003),(x,y-.002,z),material=m["dark"]))
    # 위아래 레일 홈: 머리·밑틀 안쪽에 얕은 두 줄.
    for z in (0.075 + 0.004, 0.925 - 0.004):
        for y in (-0.031, 0.008):
            parts.append(ig.box("track", (1.20, 0.006, 0.008), location=(0.0, y, z), material=m["alu"]))
    return ig.build_asset(
        "SM_ApartmentWindow", "prop", parts, out_root,
        collision_parts=[],
        notes=("403호 북쪽 벽 미닫이 창틀 130 x 10 x 99. 원점 (-100, 212.5, 100) 창틀 아래 중심(2개 레일과 중앙 크리센트), 실내 쪽 -Y. "
               "두 유리는 장면에서 각 창짝의 가스켓 안에 배치한다. WindowReference_20260916.json 참조. 충돌 없음."),
        texture_size=1024)


# --------------------------------------------------------------------------
# 블라인드
# --------------------------------------------------------------------------

def build_blind(out_root):
    """걷어 올린 상태. 로컬 Z 0이 헤드레일 윗면(씬 206), 아래로 내려간다."""
    ig.reset_scene()
    m = mats()
    parts = []
    parts.append(ig.box("headrail", (1.34, 0.07, 0.08), location=(0.0, 0.0, -0.04), bevel=0.003, segments=1,
                        material=m["white"]))
    for x in (-0.66, 0.66):
        parts.append(ig.box("bracket", (0.02, 0.06, 0.09), location=(x, 0.005, -0.045), material=m["alu"]))
    # 슬랫 뭉치: 6 mm 피치로 겹친 얇은 판 열 장, 그 밑에 바닥 레일.
    for i in range(10):
        z = -0.08 - 0.004 - i * 0.006
        parts.append(ig.box("slat", (1.30, 0.06, 0.0025), location=(0.0, -0.005, z), material=m["slat"]))
    parts.append(ig.box("bottom_rail", (1.30, 0.06, 0.015), location=(0.0, -0.005, -0.15), bevel=0.002,
                        segments=1, material=m["white"]))
    # 사다리 테이프 둘: 뭉치를 세로로 감싼 흰 띠.
    for x in (-0.45, 0.45):
        for y in (-0.036, 0.026):
            parts.append(ig.box("ladder", (0.01, 0.002, 0.08), location=(x, y, -0.12), material=m["slat"]))
    # 오른쪽 끝 조작줄과 틸트 봉. 줄 끝은 씬 Z 122(로컬 -0.84).
    parts.append(ig.cylinder("cord", 0.003, 0.76, location=(0.64, -0.02, -0.08 - 0.38), segments=8,
                             material=m["white"]))
    parts.append(ig.cylinder("tassel", 0.008, 0.035, location=(0.64, -0.02, -0.84 + 0.0175), segments=10,
                             material=m["white"]))
    parts.append(ig.cylinder("wand", 0.004, 0.50, location=(0.58, -0.03, -0.08 - 0.25), segments=8,
                             material=m["white"]))
    return ig.build_asset(
        "SM_VenetianBlind", "prop", parts, out_root,
        collision_parts=[],
        notes=("403호 창 위 베네시안 블라인드, 걷어 올린 상태. 원점은 헤드레일 윗면 중심(씬 (-100, 207, 206)), "
               "로컬 Z는 아래로 -0.84까지. 실내 쪽 -Y. 충돌 없음."),
        texture_size=1024)


# --------------------------------------------------------------------------
# 인터폰·스위치
# --------------------------------------------------------------------------

def build_intercom(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    w, d, h = 0.20, 0.035, 0.31
    body = ig.box("body", (w, d, h), location=(0.0, d * 0.5, h * 0.5), bevel=0.004, segments=2,
                  material=m["white"])
    parts.append(body)
    # 앞면 이미지(화면·스피커·통화/열림 버튼). 앞면 +Y.
    parts.append(ig.image_quad("face", (w - 0.012, h - 0.012), (0.0, d + 0.0005, h * 0.5), m["intercom"],
                               rotation=FACING_POS_Y))
    # 화면 유리 살짝 돌출, 대기 LED 하나.
    parts.append(ig.box("screen_glass", (0.11, 0.002, 0.075), location=(0.0, d + 0.0018, h - 0.075),
                        material=m["dark"]))
    parts.append(ig.cylinder("led", 0.0025, 0.001, location=(0.075, d + 0.0015, 0.035),
                             rotation=(math.pi * 0.5, 0.0, 0.0), segments=10, material=m["led"]))
    return ig.build_asset(
        "SM_VideoIntercom", "prop", parts, out_root,
        collision_parts=[],
        notes="403호 현관 벽 비디오 인터폰 20 x 3.5 x 31. 원점은 벽면 바닥 중심(씬 (62, -215, 130)), 앞면 +Y. 충돌 없음.",
        texture_size=1024, preview_yaw=200.0)


def build_switch(out_root):
    ig.reset_scene()
    m = mats()
    parts = []
    plate = ig.box("plate", (0.10, 0.012, 0.10), location=(0.0, 0.006, 0.05), bevel=0.003, segments=2,
                   material=m["white"])
    parts.append(plate)
    for x in (-0.024, 0.024):
        parts.append(ig.box("rocker", (0.036, 0.006, 0.062), location=(x, 0.012 + 0.003, 0.05),
                            rotation=(math.radians(4.0) * (1 if x < 0 else -1), 0.0, 0.0), bevel=0.0015,
                            segments=1, material=m["white"]))
    return ig.build_asset(
        "SM_WallSwitch", "prop", parts, out_root,
        collision_parts=[],
        notes="403호 현관 2구 스위치 10 x 1.8 x 10. 원점 벽면 바닥 중심(씬 (90, -215, 123)), 앞면 +Y. 충돌 없음.",
        texture_size=512, preview_yaw=200.0)


# --------------------------------------------------------------------------
# 신발장
# --------------------------------------------------------------------------

def build_shoe_cabinet(out_root):
    """씬 몸통 80x32x110(Y -214..-182), 문 두 줄, 상판 84x34x3 Z 110..113. 문이 +Y."""
    ig.reset_scene()
    m = mats()
    parts = []
    body = ig.box("carcass", (0.80, 0.32, 1.04), location=(0.0, 0.0, 0.06 + 0.52), bevel=0.003, segments=1,
                  material=m["gloss"])
    parts.append(body)
    parts.append(ig.box("plinth", (0.76, 0.28, 0.06), location=(0.0, -0.02, 0.03), material=m["dark"]))
    top = ig.box("top", (0.84, 0.34, 0.03), location=(0.0, 0.0, 1.115), bevel=0.004, segments=2,
                 material=m["gloss"])
    parts.append(top)
    # 문 넷: 두 줄 x 두 짝. 손잡이 대신 윗변에 손가락 홈.
    door_h = 0.50
    for zc in (0.06 + 0.02 + door_h * 0.5, 0.06 + 0.02 + door_h + 0.02 + door_h * 0.5):
        for xc in (-0.195, 0.195):
            door = ig.box("door", (0.37, 0.018, door_h), location=(xc, 0.16 + 0.009, zc), bevel=0.0015,
                          segments=1, material=m["gloss"])
            groove = ig.box("groove", (0.30, 0.02, 0.014), location=(xc, 0.16 + 0.012, zc + door_h * 0.5 - 0.01))
            ig.boolean(door, groove, "DIFFERENCE")
            parts.append(door)
            parts.append(ig.box("groove_back", (0.30, 0.004, 0.014), location=(xc, 0.16 + 0.002, zc + door_h * 0.5 - 0.01),
                                material=m["dark"]))
    return ig.build_asset(
        "SM_ShoeCabinet", "prop", parts, out_root,
        collision_parts=[[body, top]],
        notes="403호 현관 신발장 84 x 34 x 113. 원점 바닥 중심(씬 (48, -198, 0)), 문이 +Y(방 쪽). 유광 흰 문 넷, 손가락 홈.",
        texture_size=1024, preview_yaw=210.0)


BUILDERS = {
    "window": build_window,
    "blind": build_blind,
    "intercom": build_intercom,
    "switch": build_switch,
    "shoe": build_shoe_cabinet,
}


def main():
    out_root = ig.out_root_from_argv()
    only = [a for a in sys.argv[sys.argv.index("--") + 2:]] if "--" in sys.argv else []
    for key, builder in BUILDERS.items():
        if not only or key in only:
            builder(out_root)


main()
