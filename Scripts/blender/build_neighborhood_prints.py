"""실물 전단을 참고한 자석 광고와 A4 임대 안내문. 앞면만 인쇄한다."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import ig_blender_lib as ig


def magnet():
    ig.reset_scene()
    source = os.path.join(os.path.dirname(__file__), '../../Content/SourceArt/AI/DoorMagnet_20260916.png')
    ink = ig.mat_image_uv('ChickenCoupon', source, roughness=.52)
    rubber = ig.mat_plastic('MagnetBacking', (.035, .032, .028), roughness=.76, bump=0)
    # 앞면 -Y, 두께 0.6mm. 뒷면과 마구리는 고무색이라 글자가 반복되지 않는다.
    card = ig.image_quad('magnet', (.09, .065), (0, 0, 0), ink, thickness=.0006)
    card.data.materials.append(rubber)
    for face in card.data.polygons:
        if face.normal.y > -.5:
            face.material_index = 1
    ig.build_asset('SM_DoorDeliveryMagnet', 'prop', [card], ig.out_root_from_argv(),
                   collision_parts=[], texture_size=512, mirror_print_for_ue=True, origin="center",
                   notes='90×65×0.6mm 자석 광고. 앞 -Y, 원점 중앙. SourceArt/AI/DoorMagnet_20260916의 실물 참조·생성 기록. 단면 인쇄, 고무색 뒷면.')


def rental_notice():
    ig.reset_scene()
    source = os.path.join(os.path.dirname(__file__), '../../Content/SourceArt/AI/RentalNotice_20260916.png')
    ink = ig.mat_image_uv('RentalPrint', source, roughness=.86)
    blank = ig.mat_plastic('PaperBack', (.79, .80, .77), roughness=.9, bump=0)
    paper = ig.image_quad('paper', (.21, .297), (0, 0, 0), ink, thickness=.00015)
    paper.data.materials.append(blank)
    for face in paper.data.polygons:
        if face.normal.y > -.5:
            face.material_index = 1
    # 테이프는 위쪽 두 모서리에만 붙인다. 원본 글자와 별도인 재질까지 함께 굽는다.
    tape = ig.mat_plastic('Tape', (.70, .71, .66), roughness=.28, bump=0)
    parts = [paper]
    for x in (-.080, .080):
        parts.append(ig.box('tape', (.018, .00005, .009), (x, -.00011, .135), material=tape))
    ig.build_asset('SM_RentalNoticeA4', 'prop', parts, ig.out_root_from_argv(),
                   collision_parts=[], texture_size=512, mirror_print_for_ue=True, origin="center",
                   notes='A4 21×29.7cm, 종이 0.15mm. 앞 -Y, 원점 중앙. RentalNotice_20260916의 실물 참조. 앞면만 인쇄, 위쪽 테이프 2개, 바람 변형 없음.')


magnet()
rental_notice()

# 이웃 메모의 작은 글씨도 실제 종이에 쓴다. 두꺼운 상자에 자막만 붙이지 않는다.
ig.reset_scene()
ink = ig.mat_image_uv('NeighborMemo', os.path.join(os.path.dirname(__file__), '../../Content/SourceArt/AI/NeighborMemo_20260916.png'), roughness=.88)
blank = ig.mat_plastic('MemoBack', (.81, .81, .78), roughness=.9, bump=0)
memo = ig.image_quad('memo', (.148, .105), (0, 0, 0), ink, thickness=.00015)
memo.data.materials.append(blank)
for face in memo.data.polygons:
    if face.normal.y > -.5:
        face.material_index = 1
ig.build_asset('SM_NeighborMemo402', 'prop', [memo], ig.out_root_from_argv(),
               collision_parts=[], texture_size=512, mirror_print_for_ue=True, origin="center",
               notes='402호 입주 메모. A6 가로 14.8×10.5cm, 종이 0.15mm. 앞 -Y, 원점 중앙. 글자와 읽기 문구 일치. 종이에 충돌 없음. 런타임의 별도 읽기 상자는 Visibility만 판정한다.')

# 달력의 날짜·요일은 build_apartment_calendar.py의 결정적 인쇄 원본을 쓴다.
ig.reset_scene()
ink = ig.mat_image_uv('Calendar2025', os.path.join(os.path.dirname(__file__), '../../Content/SourceArt/UtilityPrints/ApartmentCalendar2025.png'), roughness=.86)
back = ig.mat_plastic('CalendarBack', (.74, .73, .67), roughness=.9, bump=0)
paper = ig.image_quad('calendar', (.31, .42), (0, 0, 0), ink, thickness=.0018)
paper.data.materials.append(back)
for face in paper.data.polygons:
    if face.normal.y > -.5:
        face.material_index = 1
clip = ig.mat_metal('CalendarClip', (.36, .36, .33), roughness=.48, streak=0)
rail = ig.box('binding', (.312, .0028, .004), (0, 0, .209), material=clip)
ig.build_asset('SM_ApartmentCalendar2025', 'prop', [paper, rail], ig.out_root_from_argv(),
               collision_parts=[], texture_size=512, mirror_print_for_ue=True, origin="center",
               notes='없는 층 현재 시점 2025년 7월. 31×42cm, 종이 묶음 1.8mm, 상단 철심 2.8mm. 앞 -Y, 원점 중앙. 과거 단서의 2024년 달력과 분리.')
