"""정면 사진의 눈·피부를 얼굴 전면에 투영한다. 측면과 뒷면은 원래 재질을 쓴다."""
import bpy
import numpy as np
import ig_blender_lib as ig


def apply(mesh, filename):
    image = bpy.data.images.load(filename, check_existing=True)
    lower, upper = ig.bounds(mesh)
    height = upper.z - lower.z
    count = len(mesh.data.vertices)
    positions = np.empty(count*3, dtype=np.float32)
    normals = np.empty(count*3, dtype=np.float32)
    indices = np.empty(len(mesh.data.loops), dtype=np.int32)
    mesh.data.vertices.foreach_get('co', positions)
    mesh.data.vertices.foreach_get('normal', normals)
    mesh.data.loops.foreach_get('vertex_index', indices)
    positions = positions.reshape((-1,3))[indices]
    normals = normals.reshape((-1,3))[indices]
    # 사진은 머리끝 y=20, 신발 밑 y=1475인 1024×1536 정면 원본이다.
    uv = mesh.data.uv_layers.new(name='FacePhoto')
    projected = np.column_stack((.5+positions[:,0]*1455/(height*1024),61/1536+positions[:,2]*1455/(height*1536)))
    uv.data.foreach_set('uv', projected.ravel())
    weight = np.clip((-normals[:,1]-.15)/.55,0,1)*np.clip((positions[:,2]/height-.835)/.05,0,1)
    colors = np.column_stack((weight,weight,weight,np.ones_like(weight)))
    mask = mesh.data.color_attributes.new(name='FacePhotoWeight', type='FLOAT_COLOR', domain='CORNER')
    mask.data.foreach_set('color', colors.ravel())
    for material in mesh.data.materials:
        nodes=material.node_tree.nodes; links=material.node_tree.links
        bsdf=next(n for n in nodes if n.type=='BSDF_PRINCIPLED')
        source=bsdf.inputs['Base Color'].links[0].from_socket
        coords=nodes.new('ShaderNodeUVMap');coords.uv_map='FacePhoto'
        sample=nodes.new('ShaderNodeTexImage');sample.image=image;sample.extension='CLIP'
        blend=nodes.new('ShaderNodeMixRGB');blend.blend_type='MIX'
        weights=nodes.new('ShaderNodeVertexColor');weights.layer_name='FacePhotoWeight'
        links.new(coords.outputs['UV'],sample.inputs['Vector'])
        links.new(weights.outputs['Color'],blend.inputs[0])
        links.new(source,blend.inputs[1]);links.new(sample.outputs['Color'],blend.inputs[2])
        links.new(blend.outputs[0],bsdf.inputs['Base Color'])
