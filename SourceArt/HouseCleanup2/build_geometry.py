import bpy,json,pathlib,numpy as np
from mathutils import Vector,Quaternion
R=pathlib.Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff');B=R/'SourceArt/HouseCleanup2';N=R/'SourceArt/NightClub'
exec((N/'expansion2_geometry/build_expansion2.py').read_text().split('\nassets={}')[0],globals())
B=R/'SourceArt/HouseCleanup2';O=B/'geometry';rows={a['label']:a for a in json.loads((B/'context/actors.json').read_text())};parts=json.loads((B/'context/parts.json').read_text());extra=json.loads((B/'context/extra_parts.json').read_text());manifest=dict(assets=[],materials={},scope=['Small basement utility door only','Basement stair connection floor, sides, ceiling','Great room intrusive trim and adjacent floor lip'],world_baked=True)
fits={}
def copy(label,name,remove=(),trim=()):
 c=rows[label]['meshes'][0];before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=c['fbx']);ob=next(o for o in bpy.data.objects if o not in before and o.type=='MESH'and not o.name.startswith(('UCX','UBX')));d=ob.data;uv=d.uv_layers.active;q=c['quat'];rot=Quaternion((q[3],q[0],q[1],q[2]));sc=Vector(c['scale']);loc=Vector(c['loc'])
 def wp(co):
  p=ob.matrix_world@co;return rot@(Vector((p.x*100,-p.y*100,p.z*100))*sc)+loc
 for i,m in enumerate(d.materials):
  key=m.name;material_info[key]=dict(color=[.45,.42,.36],roughness=.85);manifest['materials'][key]=dict(reuse_asset=c['materials'][i].split('.')[0])
 removed={f for i in remove for f in parts[i]['faces']};trims={f for i in trim for f in parts[i]['faces']};g=Geometry();changed=0
 for f in d.polygons:
  key=d.materials[f.material_index].name;poly=[]
  for li in f.loop_indices:
   p=wp(d.vertices[d.loops[li].vertex_index].co);u=uv.data[li].uv if uv else (0,0);poly.append((*p,float(u[0]),float(u[1])))
  if label=='SM_Basement_p0_m1_Solid':
   if f.index in parts[696]['faces'] and abs((Vector(poly[1][:3])-Vector(poly[0][:3])).cross(Vector(poly[2][:3])-Vector(poly[0][:3])).normalized().x)>.9:
    fits['brick']=np.linalg.lstsq(np.array([[p[1],p[2],1]for p in poly]),np.array([[p[3],p[4]]for p in poly]),rcond=None)[0].tolist();fits['brick_key']=key
   if f.index in parts[691]['faces'] and all(abs(p[2]+361.981)<.1 for p in poly):
    fits['floor']=np.linalg.lstsq(np.array([[p[0],p[1],1]for p in poly]),np.array([[p[3],p[4]]for p in poly]),rcond=None)[0].tolist();fits['floor_key']=key
  if f.index in removed:changed+=1;continue
  polys=[poly]
  if f.index in trims:
   # Only trim the pieces intruding into the open great-room footprint.
   polys=subtract(poly,[((1,0,0),1401.84),((0,1,0),904.86)]);changed+=1
  if label=='SM_Upper_p0_p1_Solid' and f.index in extra[151]['faces']:
   polys=subtract(poly,[((1,0,0),1401.84),((0,1,0),904.86)]);changed+=1
  for p in polys:g.face(p,key,f.use_smooth)
 for o in list(bpy.data.objects):
  if o not in before:bpy.data.objects.remove(o,do_unlink=True)
 save(g,name,label,False);manifest['assets'][-1]['changed_faces']=changed

def save(g,name,label=None,block=False):
 ob=g.make(name);bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob;p=O/'models'/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(p),use_selection=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False);manifest['assets'].append(dict(name=name,fbx=str(p),target_actor=label,collision=block,materials=g.names));print('CLEANUP_ASSET',name,flush=True)
copy('SM_Basement_p0_m1_Solid','SM_Basement_p0_m1_Cleanup2',remove=[634,635,636,639,640,641,687,893,894])
copy('SM_Upper_p0_p0_Solid','SM_Upper_p0_p0_Cleanup2',trim=list(range(3498,3506)))
copy('SM_Upper_p0_p1_Solid','SM_Upper_p0_p1_Cleanup2')
assert all(k in fits for k in ['brick','brick_key','floor','floor_key']),fits
brick=fits['brick_key'];floor=fits['floor_key'];g=Geometry()
# Seamless masonry where the small baked utility leaf and casing used to be.
g.box([-1554,473.88,-362],[-1498.86,583.85,-108.3],brick)
# Continuous threshold support and real jamb/ceiling liners between the original
# stair doorway and the rebuilt black hall, without entering the clear lane.
g.box([-1554,625.68,-374],[-1320,825.66,-361.966],floor)
g.box([-1514.7,573.18,-361.966],[-1360,625.68,-108.30],brick)
g.box([-1514.7,825.66,-361.966],[-1360,880.11,-108.30],brick)
g.box([-1554,625.68,-134.17],[-1320,825.66,-108.30],brick)
# Carry the texture phase from the adjoining existing wall and floor.
new=[]
for poly,k,smooth in g.polys:
 f=np.array(fits['floor'if k==floor else'brick']);new.append(([(p[0],p[1],p[2],*([p[0],p[1],1]if k==floor else[p[1],p[2],1])@f)for p in poly],k,smooth))
g.polys=new;save(g,'SM_HC_BasementStairJoin',block=True)
manifest['uv_fits']=fits;(O/'manifest.json').write_text(json.dumps(manifest,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseCleanup2.blend'));print('CLEANUP_GEOMETRY_PASS',flush=True)
