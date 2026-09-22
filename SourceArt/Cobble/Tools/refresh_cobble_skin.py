"""Re-bake only the facial material; preserve final geometry, rigs and animations."""
import bpy, sys, os
from mathutils import Vector
WORK = '/Users/Stewart/Documents/Codex/2026-09-11/c/work'
sys.path.insert(0, WORK)
from build_cobble import export, CAST, OUT
from cobble_face_texture import apply_reference_face

args = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [str(i) for i in range(1,9)]
for arg in args:
    index = int(arg)
    key = f'C{index:02d}'
    dest = OUT+'/'+key
    bpy.ops.wm.open_mainfile(filepath=dest+'/'+key+'.blend')
    human = bpy.data.objects[key+'_Body']
    rig = human.parent
    old_material = human.data.materials[0]
    idle = bpy.data.actions['AN_'+key+'_Idle']
    apply_reference_face(index,human,rig,dest)
    new_material = human.data.materials[0]
    for obj in bpy.data.objects:
        if obj.type == 'MESH':
            for slot in obj.material_slots:
                if slot.material == old_material:
                    slot.material = new_material
    if old_material.users == 0:
        bpy.data.materials.remove(old_material)
    new_material.name = key+'_Skin'
    meshes = [obj for obj in bpy.data.objects if obj.type == 'MESH' and obj.parent == rig]
    body = [obj for obj in meshes if not obj.name.endswith('_Arms')]
    arms = [obj for obj in meshes if obj.name.endswith('_Arms')]
    height = max(vertex.co.z for vertex in human.data.vertices)
    scale = CAST[index-1]['h']/height
    export(dest+'/SK_'+key+'.fbx',rig,body,scale)
    export(dest+'/SK_'+key+'_Arms.fbx',rig,arms,scale)
    rig.animation_data_create()
    rig.animation_data.action = idle
    bpy.context.scene.frame_set(1)
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = 640,960
    scene.cycles.samples = 24
    bpy.ops.wm.save_as_mainfile(filepath=dest+'/'+key+'.blend')
    scene.render.filepath = dest+'/'+key+'_Review.png'
    bpy.ops.render.render(write_still=True)
    camera = scene.camera
    center = Vector((0,-.04,rig.data.bones['head'].head_local.z+.065))
    camera.location=(0,-3,center.z)
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=height*.28
    scene.render.resolution_x,scene.render.resolution_y=512,640
    scene.render.filepath=dest+'/T_'+key+'.png'
    bpy.ops.render.render(write_still=True)
    center.z += .005
    camera.location=(0,-2.5,center.z)
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=.43
    scene.render.resolution_x,scene.render.resolution_y=768,960
    scene.cycles.samples=48
    scene.render.filepath=dest+'/'+key+'_FaceFront.png'
    bpy.ops.render.render(write_still=True)
    camera.location=(1.1,-2.2,center.z)
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=dest+'/'+key+'_FaceAngle.png'
    bpy.ops.render.render(write_still=True)
    print('COBBLE_SKIN_REFRESHED',key)
