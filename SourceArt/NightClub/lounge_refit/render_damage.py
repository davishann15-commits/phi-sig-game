"""Offline actual authored four-stage contact render; no asset/map edits."""
import bpy,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(R/'NightClubLounge_source.blend'))
for ob in bpy.context.scene.objects:ob.hide_render=True
sc=bpy.context.scene
for i,suff in enumerate(('', '_Damaged','_HeavilyDamaged','_Broken')):
 source=bpy.data.objects['SM_NCL_SectionalMain'+suff]
 ob=source.copy();ob.data=source.data;bpy.context.collection.objects.link(ob);ob.hide_render=False;ob.hide_set(False);ob.location=((i-1.5)*4.05,0,0)
 d=bpy.data.curves.new('Stage label','FONT');d.body=('INTACT','DAMAGED','HEAVY DAMAGE','BROKEN')[i];d.size=.25;d.align_x='CENTER';d.align_y='CENTER';o=bpy.data.objects.new('Stage label',d);bpy.context.collection.objects.link(o);o.location=(ob.location.x,-.45,1.23);o.rotation_euler.x=math.pi/2
 m=bpy.data.materials.new('Label white');m.diffuse_color=(.8,.8,.8,1);d.materials.append(m)
bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,-.05));ob=bpy.context.object;ob.dimensions=(23,13,.10);m=bpy.data.materials.new('Ground');m.diffuse_color=(.13,.14,.16,1);ob.data.materials.append(m)
sc.render.engine='CYCLES';sc.cycles.samples=32;sc.render.resolution_x=2400;sc.render.resolution_y=650;sc.render.resolution_percentage=100;sc.world.color=(.10,.10,.10);sc.view_settings.view_transform='AgX'
for loc,power,size in(((0,-6,6),2400,10),((2,3,5),1800,8),((-5,-4,4),1000,5)):
 d=bpy.data.lights.new('Soft preview light','AREA');d.energy=power;d.shape='DISK';d.size=size;o=bpy.data.objects.new('Soft preview light',d);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,.50))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Damage preview');o=bpy.data.objects.new('Damage preview',d);bpy.context.collection.objects.link(o);o.location=(4,-20,8);o.rotation_euler=(Vector((0,0,.50))-o.location).to_track_quat('-Z','Y').to_euler();d.type='ORTHO';d.ortho_scale=17;sc.camera=o;sc.render.filepath=str(R/'damage-preview.png');bpy.ops.render.render(write_still=True)
print('NCL_DAMAGE_RENDER_PASS')
