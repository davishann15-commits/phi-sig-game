import bpy,math
from mathutils import Vector
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
mats={n:bpy.data.materials.new(n) for n in ['PaintWhite','PaintCream','MetalBrass']}
def box(n,c,s,mat,bevel=.003):
 bpy.ops.mesh.primitive_cube_add(size=1,location=(c[0]/100,-c[1]/100,c[2]/100));o=bpy.context.object;o.name=n;o.dimensions=Vector(s)/100;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(mats[mat]);m=o.modifiers.new('Soft edges','BEVEL');m.width=bevel;m.segments=3;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=m.name)
box('Leaf',(0,87,103),(5.6,174,206),'PaintWhite')
for side in [-1,1]:
 for y in [45,129]:
  for z,h in [(39,53),(110,59),(176,37)]:
   box('Panel molding',(side*3,y,z),(1.3,66,h),'PaintWhite')
   box('Panel center',(side*3.7,y,z),(.7,60,h-6),'PaintCream')
 box('Handle plate',(side*3.6,157,101),(1.5,5,18),'MetalBrass')
 box('Handle stem',(side*6,157,102),(4,2.8,2.8),'MetalBrass')
 box('Lever',(side*8,151,102),(2.5,14,2.8),'MetalBrass',.006)
 for z in [25,103,181]:box('Hinge',(side*3,2,z),(2,4,10),'MetalBrass')
bpy.ops.object.select_all(action='SELECT');bpy.context.view_layer.objects.active=bpy.context.selected_objects[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_PorchAccessFittedLeaf';bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.export_scene.fbx(filepath='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/DoorRepair/SM_PorchAccessFittedLeaf.fbx',use_selection=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
