"""Braxton-only mesh and cloth refinement; source photographs are read-only."""
import bpy, math, os
from mathutils import Vector

def subdivide(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True); bpy.context.view_layer.objects.active=obj
    mod=obj.modifiers.new('Braxton surface refinement','SUBSURF')
    mod.levels=1;mod.render_levels=1
    mod.uv_smooth='NONE'
    # Subdivide rest-space geometry before the armature deforms it.
    bpy.ops.object.modifier_move_up(modifier=mod.name)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for face in obj.data.polygons: face.use_smooth=True

def refine_braxton(body,shirt,shorts,rig,objects,destination):
    for obj in (shirt,shorts):
        if obj: subdivide(obj)
    height=max(v.co.z for v in body.data.vertices)
    for vertex in shirt.data.vertices:
        p=vertex.co
        if abs(p.x)<height*.15:
            # Soft, asymmetric folds near waist and under the arms.
            waist=math.exp(-((p.z-height*.535)/(height*.075))**2)
            ripple=(math.sin(p.z*110+p.x*35)+.45*math.sin(p.z*191-p.x*53))*.0024*waist
            p.y+=ripple*(1 if p.y>0 else -1)
        else:
            bone=rig.data.bones['lowerarm_l' if p.x>0 else 'lowerarm_r']
            axis=bone.tail_local-bone.head_local
            t=(p-bone.head_local).dot(axis)/axis.length_squared
            if -.15<t<.36:
                center=bone.head_local+axis*t
                radial=p-center
                radial-=axis.normalized()*radial.dot(axis.normalized())
                if radial.length>.001:
                    p+=radial.normalized()*(.0032*math.sin(t*64)+.0018*math.sin(t*113))*math.exp(-((t-.1)/.24)**2)
    shirt.data.update()
    for obj in (shorts,):
        for vertex in obj.data.vertices:
            p=vertex.co
            p.y+=.0018*math.sin(p.z*125+abs(p.x)*40)
        obj.data.update()
    # Ensure cloth and the hood read as the same olive fabric.
    for mat in bpy.data.materials:
        if mat.name.startswith('C01_sage'):
            bs=mat.node_tree.nodes.get('Principled BSDF')
            if bs: bs.inputs['Base Color'].default_value=(.15,.19,.15,1)
    material=shirt.data.materials[0];nodes=material.node_tree.nodes;links=material.node_tree.links
    bs=nodes.get('Principled BSDF');bs.inputs['Roughness'].default_value=.9
    coord=nodes.new('ShaderNodeTexCoord')
    noise=nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=850
    noise.inputs['Detail'].default_value=2
    links.new(coord.outputs['Object'],noise.inputs['Vector'])
    bump=nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.24;bump.inputs['Distance'].default_value=.00022
    links.new(noise.outputs['Fac'],bump.inputs['Height']);links.new(bump.outputs['Normal'],bs.inputs['Normal'])
    image=bpy.data.images.new('C01_ShirtNormal',width=2048,height=2048,alpha=False)
    image.colorspace_settings.name='Non-Color'
    target=nodes.new('ShaderNodeTexImage');target.image=image;nodes.active=target
    bpy.ops.object.select_all(action='DESELECT');shirt.select_set(True);bpy.context.view_layer.objects.active=shirt
    states=[(m,m.show_render,m.show_viewport) for m in shirt.modifiers if m.type=='ARMATURE']
    for m,_,_ in states:m.show_render=False;m.show_viewport=False
    bpy.context.scene.render.engine='CYCLES';bpy.context.scene.cycles.samples=1
    bpy.ops.object.bake(type='NORMAL',normal_space='TANGENT',margin=8,use_clear=True)
    image.filepath_raw=os.path.join(destination,'C01_ShirtNormal.png');image.file_format='PNG';image.save()
    for m,r,v in states:m.show_render=r;m.show_viewport=v
    print('BRAXTON_DETAIL_READY',len(body.data.vertices),len(shirt.data.vertices))
