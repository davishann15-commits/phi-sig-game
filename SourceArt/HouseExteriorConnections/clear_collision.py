"""Clip only wall triangles inside measured window apertures; keep every other face."""
import bpy,json,runpy
from pathlib import Path
from mathutils import Vector
B=Path(__file__).resolve().parent;O=B/'models';O.mkdir(exist_ok=True)
s=runpy.run_path(str(B.parent/'HouseInteractivity/source/extract_interactivity.py'))
aa=json.load(open(B/'context/actors.json'));panes=[]
for a in aa:
 for c in a['components']:
  if 'pane_bounds' in c:panes.append((a['fixture']['fixture_id'],*map(Vector,c['pane_bounds'])))
def clip(poly,axis,value,greater):
 out=[]
 for a,b in zip(poly,poly[1:]+poly[:1]):
  da=(a[axis]-value)*(1 if greater else -1);db=(b[axis]-value)*(1 if greater else -1)
  if da>=-1e-8:out.append(a)
  if (da>1e-8 and db< -1e-8) or (da< -1e-8 and db>1e-8):out.append(a+(b-a)*(da/(da-db)))
 return out

def subtract(poly,lo,hi):
 # Partition the original surface into six outside half spaces. Only the
 # portion inside ALL six bounds is discarded; there is no solid Boolean.
 inside=poly;out=[]
 for axis in range(3):
  for value,greater in [(lo[axis],True),(hi[axis],False)]:
   outside=clip(inside,axis,value,not greater)
   if len(outside)>=3:out.append(outside)
   inside=clip(inside,axis,value,greater)
   if len(inside)<3:return out
 return out
rows=[]
for a in aa:
 visible_target=a['label'] in ('SM_Upper_p0_m1_Solid','SM_Upper_p0_p0_Solid')
 if not((a['label'].startswith('COL_') and 'Solid' in a['label']) or visible_target):continue
 c=a['components'][0];path=B/'context/fbx'/(c['mesh'].split('/')[-1].split('.')[0]+'.fbx')
 if not path.exists():continue
 assert abs(c['quat'][3]-1)<.0001
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(path))
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith(('UCX_','UBX_')))
 ps=s['components'](o);facecuts={};cuts=[]
 def local(v):return Vector(((v.x-c['loc'][0])/c['scale'][0]/100,-(v.y-c['loc'][1])/c['scale'][1]/100,(v.z-c['loc'][2])/c['scale'][2]/100))
 for p in ps:
  if (p['size'].z<.05 and not visible_target) or min(p['size'].x,p['size'].y)>.60:continue
  hits=[]
  for name,wlo,whi in panes:
   size=whi-wlo;axis=0 if size.x<size.y else 1
   if size.z<10 or size[1-axis]<20:continue
   lo=wlo.copy();hi=whi.copy();lo[axis]-=145;hi[axis]+=145
   lo.z-=.3;hi.z+=.3;lo[1-axis]-=.3;hi[1-axis]+=.3
   l=local(lo);h=local(hi);cl=Vector([min(l[i],h[i]) for i in range(3)]);ch=Vector([max(l[i],h[i]) for i in range(3)])
   if not all(p['lo'][i]<ch[i] and p['hi'][i]>cl[i] for i in range(3)):continue
   for f in p['faces']:facecuts.setdefault(f,[]).append((cl,ch))
   hits.append(name)
  if hits:cuts.append(dict(panes=hits,bounds=[list(p['lo']),list(p['hi'])]))
 if not cuts:continue
 verts=[];faces=[];preserved=0
 for f in o.data.polygons:
  poly=[o.matrix_world@o.data.vertices[v].co for v in f.vertices];parts=[poly]
  for lo,hi in facecuts.get(f.index,[]):
   nxt=[]
   for part in parts:
    if any(max(v[i] for v in part)<=lo[i]+1e-8 or min(v[i] for v in part)>=hi[i]-1e-8 for i in range(3)):nxt.append(part)
    else:nxt.extend(subtract(part,lo,hi))
   parts=nxt
  if f.index not in facecuts:preserved+=1
  for part in parts:
   for k in range(1,len(part)-1):
    tri=[part[0],part[k],part[k+1]]
    if (tri[1]-tri[0]).cross(tri[2]-tri[0]).length<1e-9:continue
    idx=len(verts);verts.extend(tri);faces.append((idx,idx+1,idx+2))
 stem='COL_Connected_'+a['label'][4:];me=bpy.data.meshes.new(stem);me.from_pydata(verts,[],faces);me.update();me.materials.append(bpy.data.materials.new('WorldGridMaterial'));ob=bpy.data.objects.new(stem,me);bpy.context.collection.objects.link(ob)
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
 fbx=O/(stem+'.fbx');bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
 rows.append(dict(actor=a['label'],source_mesh=c['mesh'],asset=stem,fbx=str(fbx),custom_collision=visible_target,cuts=cuts,untouched_faces=preserved,original_faces=len(o.data.polygons),clipped_faces=len(faces)))
(B/'collision-manifest.json').write_text(json.dumps(rows,indent=2));print('CONNECTED_COLLISION_CUTS',len(rows))
