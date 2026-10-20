"""Separate four rear sash grilles without changing source UVs or room geometry."""
import bpy,json,runpy,re
from pathlib import Path
from mathutils import Vector
B=Path(__file__).resolve().parent
s=runpy.run_path(str(B.parent/'HouseInteractivity/source/extract_interactivity.py'))
aa=json.load(open(B/'context/actors.json'));rows=[]
for label in ('SM_Upper_p0_m1_Solid','SM_Upper_p0_p0_Solid'):
 a=next(a for a in aa if a['label']==label);c=a['components'][0]
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.fbx(filepath=str(B/'context/fbx'/(c['mesh'].split('/')[-1].split('.')[0]+'.fbx')))
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith(('UCX_','UBX_')))
 parts=s['components'](o);removed=set()
 def local(v):return Vector(((v[0]-c['loc'][0])/c['scale'][0]/100,-(v[1]-c['loc'][1])/c['scale'][1]/100,(v[2]-c['loc'][2])/c['scale'][2]/100))
 def export(ob,stem):
  bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
  f=B/'models'/(stem+'.fbx');bpy.ops.export_scene.fbx(filepath=str(f),use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False);return str(f)
 for pane in aa:
  if pane.get('fixture',{}).get('fixture_id','').replace('_Glass','_Solid').split('__')[0]!=label:continue
  pc=pane['components'][0];lo,hi=map(local,pc['pane_bounds']);mn=Vector([min(lo[i],hi[i]) for i in range(3)]);mx=Vector([max(lo[i],hi[i]) for i in range(3)])
  chosen=[]
  for p in parts:
   # Only the white internal vertical/horizontal sash bars; outer jambs stay.
   if not any('PaintWhite' in m for m in p['materials']):continue
   if abs(p['center'].y-(mn.y+mx.y)/2)>.16 or p['size'].y>.20:continue
   if p['lo'].x<mn.x-.085 or p['hi'].x>mx.x+.085 or p['lo'].z<mn.z-.085 or p['hi'].z>mx.z+.085:continue
   vertical=p['size'].x<.06 and p['size'].z>.35 and mn.x+.08<p['center'].x<mx.x-.08
   horizontal=p['size'].x>.35 and p['size'].z<.06 and mn.z+.04<p['center'].z<mx.z-.04
   if vertical or horizontal:chosen.append(p)
  if not chosen:
   print('PANE_LOCAL',mn,mx)
   for p in parts:
    if abs(p['center'].y-(mn.y+mx.y)/2)<.2 and mn.x-.1<p['center'].x<mx.x+.1 and mn.z-.2<p['center'].z<mx.z+.2:print('PART',p['lo'],p['hi'],p['materials'])
  fs={f for p in chosen for f in p['faces']};assert fs,(label,pane['label'],'no sash bars')
  removed|=fs;stem='SM_Breakaway_'+pane['fixture']['fixture_id'].replace('__','_')
  ob,slots=s['submesh'](o,fs,stem,local(pc['loc']))
  mats=[c['materials'][k] for k in slots]
  rows.append(dict(kind='frame',actor=pane['label'],asset=stem,fbx=export(ob,stem),materials=mats,parts=len(chosen),faces=len(fs)))
  bpy.data.objects.remove(ob,do_unlink=True)
 stem='SM_Connected_'+label[3:]+'_SashCleared'
 kept=set(range(len(o.data.polygons)))-removed
 ob,slots=s['submesh'](o,kept,stem)
 rows.append(dict(kind='remainder',actor=label,asset=stem,fbx=export(ob,stem),materials=[c['materials'][k] for k in slots],original_faces=len(o.data.polygons),retained_faces=len(kept),removed_faces=len(removed)))
(B/'sash-manifest.json').write_text(json.dumps(rows,indent=2));print('REAR_SASH_SEPARATION',[(r['actor'],r.get('parts',0)) for r in rows])
