import bpy,json,os,runpy
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');n=json.load(open(R+'/context/actors.json'));d=json.load(open(R+'/extraction-manifest.json'));aa={a['label']:a for a in n['actors']};out=[]
for p in d['pieces']:
 if p['behavior']!='window':continue
 lo,hi=map(Vector,p['bounds_blender_metres']);sz=hi-lo;scale=p['source_transform']['scale'];width=max(sz.x*scale[0],sz.y*scale[1]);height=sz.z*scale[2]
 if width<.64 or height<1.80:continue
 label=p['source_actor_label'].replace('_Glass','_Solid');a=aa[label];c=a['components'][0];info=n['meshes'][c['mesh']];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=info['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'));parts=s['components'](o);axis=0 if sz.x>sz.y else 1;normal=1-axis
 # Any bar lying inside the actual glass opening, at the facade depth.
 found=[]
 for q in parts:
  if not(lo[normal]-.22<q['center'][normal]<hi[normal]+.22):continue
  if q['hi'][axis]<lo[axis]+.03 or q['lo'][axis]>hi[axis]-.03 or q['hi'].z<lo.z+.03 or q['lo'].z>hi.z-.03:continue
  if all(q['lo'][k]>=lo[k]-.08 and q['hi'][k]<=hi[k]+.08 for k in (axis,2)):found.append(dict(lo=s['raw'](q['lo']),hi=s['raw'](q['hi']),size=s['raw'](q['size']),materials=q['materials'],faces=len(q['faces'])))
 out.append(dict(window_id=p['id'],shell=label,glass_bounds=p['bounds_blender_metres'],candidate_internal_geometry=found))
json.dump(out,open(R+'/inventory/fixed-window-frame-audit.json','w'),indent=2);print('FIXED_WINDOW_SCAN_DONE',len(out))
