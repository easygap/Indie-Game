"""실물 운송장과 생성 인쇄 원본을 참고한 얇은 배송 라벨 묶음."""
import math
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig

ig.reset_scene()
paper=ig.mat_plastic('라벨뒷면',(.76,.75,.70),roughness=.91,bump=0)
ink=ig.mat_image_uv('운송장인쇄',os.path.join(os.path.dirname(__file__),
    '../../Content/SourceArt/AI/ShippingLabel_20260917.png'),roughness=.89)
parts=[]
for i in range(3):
    parts.append(ig.box('종이',(.180,.130,.0004),
        ((i-1)*.0008,(i-1)*.0005,.0002+i*.00045),
        rotation=(0,0,(i-2)*.007),material=paper))
parts.append(ig.image_quad('윗장인쇄',(.180,.130),(.0008,.0005,.00135),ink,
    rotation=(-math.pi/2,0,0),thickness=.0001))
ig.build_asset('SM_ShippingLabels','prop',parts,ig.out_root_from_argv(),
    collision_parts=[parts[:3]],texture_size=1024,origin='bottom-center',mirror_print_uv=True,
    notes='실물 운송장 사진과 ShippingLabel_20260917 인쇄 원본 참조. 18×13cm 세 장, 전체 두께 약1.5mm. 윗면만 인쇄, 밑면은 무지. 계약서 왼쪽에 떨어뜨려 놓는다. 게임의 배송 단서와 이름·주소·요청 문구 일치.')
