"""Small, baked skin-surface detail for the existing rigged character assets."""
import bpy
import os


def add_skin_detail(index, human, destination):
    key = f'C{index:02d}'
    material = human.data.materials[0]
    nodes, links = material.node_tree.nodes, material.node_tree.links
    bs = nodes.get('Principled BSDF')
    if bs is None:
        raise RuntimeError('Missing skin Principled shader')
    output = next(n for n in nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
    original_surface = output.inputs['Surface'].links[0].from_socket
    coord = nodes.new('ShaderNodeTexCoord')
    noise = nodes.new('ShaderNodeTexNoise')
    noise.inputs['Scale'].default_value = 630.0
    noise.inputs['Detail'].default_value = 2.0
    noise.inputs['Roughness'].default_value = .62
    links.new(coord.outputs['Object'], noise.inputs['Vector'])
    bump = nodes.new('ShaderNodeBump')
    bump.inputs['Strength'].default_value = .30
    bump.inputs['Distance'].default_value = .00012
    links.new(noise.outputs['Fac'], bump.inputs['Height'])
    links.new(bump.outputs['Normal'], bs.inputs['Normal'])
    bs.inputs['Roughness'].default_value = .64
    bs.inputs['Specular IOR Level'].default_value = .25
    temporary = [coord, noise, bump]
    scene = bpy.context.scene
    old_engine, old_samples = scene.render.engine, scene.cycles.samples
    states = [(m, m.show_viewport, m.show_render) for m in human.modifiers if m.type == 'ARMATURE']
    bpy.ops.object.select_all(action='DESELECT')
    human.select_set(True)
    bpy.context.view_layer.objects.active = human
    try:
        scene.render.engine = 'CYCLES'
        scene.cycles.samples = 1
        for mod, _, _ in states:
            mod.show_viewport = False
            mod.show_render = False
        for kind in ('Normal', 'Roughness'):
            image = bpy.data.images.new(key+'_Skin'+kind, width=2048, height=2048, alpha=False)
            image.colorspace_settings.name = 'Non-Color'
            target = nodes.new('ShaderNodeTexImage')
            target.image = image
            nodes.active = target
            temporary.append(target)
            if kind == 'Normal':
                bpy.ops.object.bake(type='NORMAL', normal_space='TANGENT', margin=8, use_clear=True)
            else:
                ramp = nodes.new('ShaderNodeMapRange')
                ramp.inputs['From Min'].default_value = 0.0
                ramp.inputs['From Max'].default_value = 1.0
                ramp.inputs['To Min'].default_value = .59
                ramp.inputs['To Max'].default_value = .71
                links.new(noise.outputs['Fac'], ramp.inputs['Value'])
                emit = nodes.new('ShaderNodeEmission')
                links.new(ramp.outputs['Result'], emit.inputs['Color'])
                links.new(emit.outputs[0], output.inputs['Surface'])
                temporary.extend([ramp, emit])
                bpy.ops.object.bake(type='EMIT', margin=8, use_clear=True)
            image.filepath_raw = os.path.join(destination, key+'_Skin'+kind+'.png')
            image.file_format = 'PNG'
            image.save()
            image.use_fake_user = True
    finally:
        links.new(original_surface, output.inputs['Surface'])
        for node in temporary:
            nodes.remove(node)
        for mod, viewport, render in states:
            mod.show_viewport, mod.show_render = viewport, render
        scene.render.engine, scene.cycles.samples = old_engine, old_samples
    normal_texture = nodes.new('ShaderNodeTexImage')
    normal_texture.image = bpy.data.images[key+'_SkinNormal']
    normal = nodes.new('ShaderNodeNormalMap')
    links.new(normal_texture.outputs['Color'], normal.inputs['Color'])
    links.new(normal.outputs['Normal'], bs.inputs['Normal'])
    rough_texture = nodes.new('ShaderNodeTexImage')
    rough_texture.image = bpy.data.images[key+'_SkinRoughness']
    links.new(rough_texture.outputs['Color'], bs.inputs['Roughness'])
    print('COBBLE_SKIN_DETAIL_READY', key)
