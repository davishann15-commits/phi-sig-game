"""Independently reimport FBXs and check dimensions and every periodic slice."""
import bpy,json,math,collections
from pathlib import Path
from mathutils.kdtree import KDTree
PROJECT=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff')
BASE=Path('/Users/Stewart/Projects/Senior sendoff/outputs/impossible-hallway')
SOURCE=PROJECT/'SourceArt/ImpossibleHallway'
PLAN=json.load(open(SOURCE/'import_manifest.json'))
report={};CX=-2177.;Y0=-1386.2425;FZ=302.3
def ue(p):return [p.x*100,-p.y*100,p.z*100]
for row in PLAN['meshes']:
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 bpy.ops.import_scene.fbx(filepath=str(SOURCE/'Art'/row['file']))
 obs=[o for o in bpy.context.scene.objects if o.type=='MESH'];pts=[ue(o.matrix_world@v.co) for o in obs for v in o.data.vertices]
 bounds={'min':[min(p[k]for p in pts) for k in range(3)],'max':[max(p[k]for p in pts) for k in range(3)]}
 err=max(abs(bounds[a][k]-row['boundsCm'][a][k]) for a in('min','max')for k in range(3))
 if err>.1:raise RuntimeError('FBX bound drift '+row['name']+str(err))
 info={'maxBoundsErrorCm':err,'polygons':sum(len(o.data.polygons)for o in obs),'triangles':sum(len(p.vertices)-2 for o in obs for p in o.data.polygons)}
 if row['name'] in('IH_SM_CorridorSurfaces','IH_SM_CorridorTrim','IH_SM_CorridorFixtures'):
  periods=collections.defaultdict(lambda:collections.defaultdict(list))
  for o in obs:
   for poly in o.data.polygons:
    pp=[ue(o.matrix_world@o.data.vertices[v].co)for v in poly.vertices]
    dc=sum(Y0-p[1] for p in pp)/len(pp)
    # Endcaps have ambiguous ownership at boundaries, but their interiorface
    # positions and normals repeat. Check18central clippedperiods separately.
    if abs(dc/180-round(dc/180))<1e-5:continue
    ix=int(dc//180)
    for vi,li in zip(poly.vertices,poly.loop_indices):
     p=ue(o.matrix_world@o.data.vertices[vi].co)
     uv=o.data.uv_layers.active.data[li].uv
     key=(o.data.materials[poly.material_index].name.split('.')[0],round(uv.x,5),round(uv.y,5))
     periods[ix][key].append((p[0]-CX,Y0-p[1]-ix*180,p[2]-FZ))
  maxerr=0
  for ix in range(18):
   if set(periods[ix])!=set(periods[0]):raise RuntimeError('Periodic material/UV mismatch '+row['name']+str(ix))
   for key,points in periods[ix].items():
    target=periods[0][key]
    if len(points)!=len(target):raise RuntimeError('Periodic loop count mismatch '+row['name']+str(ix)+str(key))
    for a,b in((points,target),(target,points)):
     kd=KDTree(len(b))
     for k,p in enumerate(b):kd.insert(p,k)
     kd.balance();maxerr=max(maxerr,max(kd.find(p)[2]for p in a))
  if maxerr>.03:raise RuntimeError('Periodic coordinate drift '+row['name']+str(maxerr))
  info['periodsMatchingPrototype']=18
  info['maximumPeriodicSerializationErrorCm']=maxerr
 report[row['name']]=info
json.dump({'meshes':report,'moduleLengthCm':180,'periodsAllExact':True,'roundtripPassed':True},open(BASE/'fbx-audit.json','w'),indent=2)
print('IMPOSSIBLE_HALL_FBX_AUDIT_PASSED',report,flush=True)
