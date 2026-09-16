"""금속성만 바꿔도 원래 색이 보존되는지 실제 Cycles 베이크로 검사한다."""

import os
import sys
import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ig_blender_lib as ig


def bake_sample(output, metallic, from_high):
    ig.reset_scene()
    material = bpy.data.materials.new('색보존검사')
    shader = ig._principled(material)
    shader.inputs['Base Color'].default_value = (.35, .22, .12, 1)
    shader.inputs['Metallic'].default_value = metallic
    material['ig_kind'] = 'baked'
    bpy.ops.mesh.primitive_plane_add(size=1)
    source = bpy.context.object
    ig.assign_material(source, material)
    name = f'color_{int(metallic)}_{int(from_high)}'
    folder = os.path.join(output, name)
    if from_high:
        source.location.z = .005
        bpy.ops.mesh.primitive_plane_add(size=1)
        low = bpy.context.object
        maps = ig.bake_from_high(low, source, name, folder, size=32,
                                ao_samples=1, cage_extrusion=.02, max_ray_distance=.05)
    else:
        maps = ig.bake_textures(source, name, folder, size=32, ao_samples=1)
    image = bpy.data.images.load(maps['D'], check_existing=False)
    center = (16*32 + 16)*4
    return tuple(image.pixels[center:center+3])


output = ig.out_root_from_argv('Saved/BaseColorBakeCheck')

# 인쇄 원본의 종횡비가 달라도 정사각형 베이크의 UV 배치는 같아야 한다.
uv_samples = []
for width, height in ((256, 256), (256, 64)):
    ig.reset_scene()
    material = bpy.data.materials.new('UV비율검사')
    ig._principled(material)
    image_node = material.node_tree.nodes.new('ShaderNodeTexImage')
    image_node.image = bpy.data.images.new('원본인쇄', width, height)
    material.node_tree.nodes.active = image_node
    ob = ig.box('인쇄판', (1., .1, 1.8), material=material)
    ig.uv_smart(ob)
    uv_samples.append([tuple(loop.uv) for loop in ob.data.uv_layers['UVMap'].data])
if any(abs(a-b) > 1e-5 for left, right in zip(*uv_samples) for a, b in zip(left, right)):
    raise RuntimeError('인쇄 원본의 종횡비가 정사각형 베이크 UV를 변형함')
print('UV_BAKE PASS square_and_wide_source_same_layout=1', flush=True)

expected = tuple(1.055 * value**(1/2.4) - .055 for value in (.35, .22, .12))
for from_high in (False, True):
    nonmetal = bake_sample(output, 0., from_high)
    metal = bake_sample(output, 1., from_high)
    # PNG를 다시 읽은 값은 sRGB다. 8비트 반올림 오차만 허용한다.
    if any(abs(value-reference) > 2/255 for result in (nonmetal, metal)
           for value, reference in zip(result, expected)):
        raise RuntimeError(f'금속 베이크 색 손실: from_high={from_high}, nonmetal={nonmetal}, metal={metal}')
    print(f'BASE_COLOR_BAKE PASS from_high={int(from_high)} nonmetal={nonmetal} metal={metal}', flush=True)
