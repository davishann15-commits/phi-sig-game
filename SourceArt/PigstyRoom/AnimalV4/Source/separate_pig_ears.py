import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector
root=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-animal-v4')
bpy.ops.wm.open_mainfile(filepath='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/PigstyRoom/RefinementV2/Art/PIG_RealBlackPig_Source.blend')
o=bpy.data.objects['SM_PIG_BlackHead'];islands=json.loads((root/'art/head-islands.json').read_text())
ears=[x for x in islands if x['n']>100 and x['mean'][2]>0]
assert len(ears)==4,'Ear island partition changed'
left=set(i for x in ears if x['mean'][1]>0 for i in x['ids']);right=set(i for x in ears if x['mean'][1]<0 for i in x['ids'])
all_ids=set(range(len(o.data.vertices)));o.data.calc_loop_triangles();total=len(o.data.loop_triangles)
assets=[]
for name,ids,pivot in [('SM_PIG_BlackHead',all_ids-left-right,(0,0,0)),('SM_PIG_LeftEar',left,(-.156,.112,.025)),('SM_PIG_RightEar',right,(-.156,-.112,.025))]:
 data=o.data.copy();bm=bmesh.new();bm.from_mesh(data);bm.verts.ensure_lookup_table();bmesh.ops.delete(bm,geom=[v for v in bm.verts if v.index not in ids],context='VERTS');bm.to_mesh(data);bm.free()
 part=bpy.data.objects.new(name+'_Detached',data);bpy.context.collection.objects.link(part)
 for v in data.vertices:v.co-=Vector(pivot)
 data.update();data.calc_loop_triangles()
 assert all(math.isfinite(c)for v in data.vertices for c in v.co)
 row={'name':name,'file':str(root/'art/meshes'/(name+'.fbx')),'vertices':len(data.vertices),'triangles':len(data.loop_triangles),'uvLayers':len(data.uv_layers),'materialSlots':[m.name for m in data.materials],'hingeCmUnreal':[pivot[0]*100,-pivot[1]*100,pivot[2]*100]}
 pts=[v.co*100 for v in data.vertices];lo=[min(v[k]for v in pts)for k in range(3)];hi=[max(v[k]for v in pts)for k in range(3)]
 row['boundsCm']={'min':[lo[0],-hi[1],lo[2]],'max':[hi[0],-lo[1],hi[2]]}
 export=part.copy();export.data=data.copy();bpy.context.collection.objects.link(export)
 for v in export.data.vertices:v.co*=100
 bpy.context.scene.unit_settings.scale_length=.01;bpy.ops.object.select_all(action='DESELECT');export.select_set(True);bpy.context.view_layer.objects.active=export
 bpy.ops.export_scene.fbx(filepath=row['file'],use_selection=True,global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',use_tspace=True,add_leaf_bones=False,bake_anim=False,path_mode='STRIP',use_triangles=True)
 bpy.data.objects.remove(export,do_unlink=True);bpy.context.scene.unit_settings.scale_length=1;assets.append(row)
assert sum(r['triangles']for r in assets)==total,'Original approved head geometry not preserved'
(root/'art/ear_manifest.json').write_text(json.dumps({'meshes':assets,'originalHeadTriangles':total,'partitionPreservesAllTriangles':True},indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(root/'art/PIG_IndependentEars.blend'))
print('PIG_EAR_EXTRACTION_PASSED triangles='+str(total))
