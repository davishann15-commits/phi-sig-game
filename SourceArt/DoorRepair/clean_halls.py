import bpy,json,collections,bmesh,os
root='/tmp/'
rows=[]
for name in ['current_westhall','current_easthall']:
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 bpy.ops.import_scene.fbx(filepath=root+name+'.fbx')
 for o in bpy.context.scene.objects:
  if o.type!='MESH' or o.name.startswith('UCX_'):continue
  m=o.data;p=list(range(len(m.vertices)))
  def find(i):
   while p[i]!=i:p[i]=p[p[i]];i=p[i]
   return i
  for f in m.polygons:
   for i in f.vertices[1:]:p[find(i)]=find(f.vertices[0])
  groups=collections.defaultdict(list)
  for f in m.polygons:groups[find(f.vertices[0])].append(f)
  drop=[]
  for fs in groups.values():
   cs=[o.matrix_world@m.vertices[i].co for i in set(i for f in fs for i in f.vertices)]
   cs=[(c.x*180,-c.y*180,c.z*100) for c in cs]
   lo=[min(c[k] for c in cs) for k in range(3)];hi=[max(c[k] for c in cs) for k in range(3)]
   if hi[2]<300 or lo[2]>560 or hi[1]<-900 or lo[1]>-500:continue
   if not (1100<abs((lo[0]+hi[0])/2)<1650):continue
   mat=m.materials[fs[0].material_index].name
   if 'MetalBrass' in mat and max(hi[k]-lo[k] for k in range(3))<20:drop.extend(f.index for f in fs)
  bm=bmesh.new();bm.from_mesh(m);bm.faces.ensure_lookup_table();bmesh.ops.delete(bm,geom=[bm.faces[i] for i in drop],context='FACES');bm.to_mesh(m);bm.free()
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
  bpy.ops.export_scene.fbx(filepath='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/DoorRepair/SM_Clean_'+name+'.fbx',use_selection=True,axis_forward='-Z',axis_up='Y')
  print('REMOVED_KNOB_FACES',name,len(drop))
json.dump(rows,open('/tmp/hall_parts.json','w'),indent=2)
