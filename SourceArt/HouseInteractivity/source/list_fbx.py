import bpy,sys,json
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=sys.argv[-1]);print('OBJECTS',[(o.name,len(o.data.polygons),len(o.data.materials)) for o in bpy.context.scene.objects if o.type=='MESH'])
