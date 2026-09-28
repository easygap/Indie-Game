# -*- coding: utf-8 -*-
"""골목 이웃 창에 넣을 방 사진 네 장을 2x2 아틀라스로 묶는다.

칸 순서가 곧 M_RoomInterior의 방 번호다. 1번(TV 방)은 재질이 파랗게 깜박인다.
사진은 창 밖에서 정면으로 찍은 1점 투시라 가운데를 잘라 정사각형으로 맞추기만 한다.

    python Scripts/build_room_interior_atlas.py
"""

import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AI = os.path.join(ROOT, "Content", "SourceArt", "AI")
OUT_DIR = os.path.join(ROOT, "Content", "SourceArt", "RoomInterior")
ROOMS = (
    "RoomInteriorLamp_20260928.png",
    "RoomInteriorTv_20260928.png",
    "RoomInteriorKitchen_20260928.png",
    "RoomInteriorStudy_20260928.png",
)
CELL = 1024


def square(image):
    width, height = image.size
    side = min(width, height)
    left = (width - side) // 2
    top = (height - side) // 2
    return image.crop((left, top, left + side, top + side)).resize((CELL, CELL), Image.LANCZOS)


def main():
    atlas = Image.new("RGB", (CELL * 2, CELL * 2))
    for index, name in enumerate(ROOMS):
        path = os.path.join(AI, name)
        if not os.path.isfile(path):
            sys.exit(f"방 사진이 없습니다: {path}")
        cell = square(Image.open(path).convert("RGB"))
        atlas.paste(cell, ((index % 2) * CELL, (index // 2) * CELL))
    os.makedirs(OUT_DIR, exist_ok=True)
    atlas.save(os.path.join(OUT_DIR, "RoomInteriors_D.png"))
    print(f"ROOM_INTERIOR_ATLAS PASS rooms={len(ROOMS)} size={CELL * 2}")


if __name__ == "__main__":
    main()
