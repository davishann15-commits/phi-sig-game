import bpy,json,collections
root='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseDoors/'
rows=[]
for name in ['SM_OriginalRoom_Upper_m1_m1_Solid','SM_OriginalRoom_Upper_m1_p0_Solid']:
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 bpy.ops.import_scene.fbx(filepath=root+name+'.fbx')
 for o in bpy.context.scene.objects:
  if o.type!='MESH':continue
  m=o.data;p=list(range(len(m.vertices)))
  def find(i):
   while p[i]!=i:p[i]=p[p[i]];i=p[i]
   return i
  for f in m.polygons:
   for i in f.vertices[1:]:p[find(i)]=find(f.vertices[0])
  groups=collections.defaultdict(list)
  for f in m.polygons:groups[find(f.vertices[0])].append(f)
  for fs in groups.values():
   cs=[o.matrix_world@m.vertices[i].co for i in set(i for f in fs for i in f.vertices)]
   cs=[(c.x*180,-c.y*180,c.z*100) for c in cs]
   lo=[min(c[k] for c in cs) for k in range(3)];hi=[max(c[k] for c in cs) for k in range(3)]
   if hi[2]<300 or lo[2]>560 or hi[1]<-900 or lo[1]>-500:continue
   if not (1100<abs((lo[0]+hi[0])/2)<1650):continue
   rows.append(dict(source=name,lo=lo,hi=hi,mat=m.materials[fs[0].material_index].name,faces=len(fs)))
json.dump(rows,open('/tmp/door_geometry.json','w'),indent=2)
