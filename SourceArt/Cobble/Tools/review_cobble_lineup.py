"""Render the actual eight fitted model assets together at their authored heights."""
import bpy
from mathutils import Vector
from pathlib import Path

source = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/Cobble')
heights = (1.84, 1.76, 1.88, 1.82, 1.78, 1.86, 1.70, 1.80)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
for index, height in enumerate(heights, 1):
    key = f'C{index:02d}'
    with bpy.data.libraries.load(str(source/key/(key+'.blend')), link=False) as (data, loaded):
        loaded.objects = [name for name in data.objects if name not in ('ReviewCamera','Key','Fill','Rim') and not name.endswith('_Arms')]
    imported = [obj for obj in loaded.objects if obj]
    for obj in imported:
        bpy.context.collection.objects.link(obj)
    rig = next(obj for obj in imported if obj.type == 'ARMATURE')
    body = next(obj for obj in imported if obj.name.startswith(key+'_Body') and obj.type == 'MESH' and obj.name == key+'_Body')
    scale = height/max(v.co.z for v in body.data.vertices)
    rig.location.x = (index-4.5)*.67
    rig.scale = (scale,)*3
    for obj in imported:
        obj.hide_render = False
        obj.hide_set(False)
    bpy.context.scene.frame_set(1)

scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 32
scene.render.resolution_x = 1920
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.view_settings.view_transform = 'AgX'
scene.world.color = (.045,.045,.045)
for name, location, energy, size in [('Key',(-3,-4,5),700,6),('Fill',(3,-2,3),350,5),('Rim',(0,2,4),450,5)]:
    light = bpy.data.lights.new(name,'AREA')
    light.energy, light.size = energy,size
    obj = bpy.data.objects.new(name,light)
    bpy.context.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (Vector((0,0,1))-obj.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.mesh.primitive_plane_add(size=200)
plane = bpy.context.object
plane.location.z = -.003
material = bpy.data.materials.new('Neutral studio floor')
material.diffuse_color = (.035,.04,.045,1)
plane.data.materials.append(material)
camera = bpy.data.cameras.new('LineupCamera')
obj = bpy.data.objects.new('LineupCamera',camera)
bpy.context.collection.objects.link(obj)
obj.location = (0,-9,1.06)
obj.rotation_euler = (Vector((0,0,1.06))-obj.location).to_track_quat('-Z','Y').to_euler()
camera.type = 'ORTHO'
camera.ortho_scale = 5.8
scene.camera = obj
scene.render.filepath = str(source/'Cobble_Height_Lineup.png')
bpy.ops.render.render(write_still=True)
print('COBBLE_LINEUP_READY', scene.render.filepath)
