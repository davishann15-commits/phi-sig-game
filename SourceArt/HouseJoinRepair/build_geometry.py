import bpy,json,pathlib,re,numpy as np
from mathutils import Vector,Quaternion
B=pathlib.Path(__file__).resolve().parent
N=B.parent/'NightClub';exec((N/'expansion2_geometry/build_expansion2.py').read_text().split('\nassets={}')[0],globals())
B=pathlib.Path(__file__).resolve().parent;O=B/'geometry';(O/'models').mkdir(parents=True,exist_ok=True)
rows={a['label']:a for a in json.load(open(B/'context/actors.json'))};parts=json.load(open(B/'context/parts.json'));manifest=dict(assets=[],materials={},notes=['Local staircase facing, floor thresholds, and banner brackets only.'])
# Existing asset/material identifiers are reused; all output vertices are world-baked.
def copy(label,overlay_id=None,grey_id=None):
 r=rows[label];c=r['meshes'][0];before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=c['fbx']);o=next(o for o in bpy.data.objects if o not in before and o.type=='MESH'and not o.name.startswith(('UCX','UBX')));d=o.data;uv=d.uv_layers.active
 q=c['quat'];rot=Quaternion((q[3],q[0],q[1],q[2]));sc=Vector(c['scale']);loc=Vector(c['loc'])
 def wp(co):
  p=o.matrix_world@co;return rot@(Vector((p.x*100,-p.y*100,p.z*100))*sc)+loc
 overlay=set(parts[overlay_id]['faces'])if overlay_id is not None else set();grey=set(parts[grey_id]['faces'])if grey_id is not None else set()
 fit=None
 if overlay:
  f=d.polygons[min(overlay)];ps=[wp(d.vertices[d.loops[li].vertex_index].co) for li in f.loop_indices];xx=np.array([[p.x,p.z,1]for p in ps]);yy=np.array([list(uv.data[li].uv)for li in f.loop_indices]);fit=np.linalg.lstsq(xx,yy,rcond=None)[0]
 overlaymat=str(d.materials[d.polygons[min(overlay)].material_index].name)if overlay else None
 for i,ma in enumerate(d.materials):
  name=ma.name;material_info[name]=dict(color=[.5,.45,.35],roughness=.8);manifest['materials'][name]=dict(reuse_asset=c['materials'][i].split('.')[0])
 g=Geometry()
 bodycaps=set()
 for index in ([2362,2363,2364]if label.startswith('SM_Entrance')else[3375]):bodycaps.update(parts[index]['faces'])
 trimcaps=set(parts[2309 if label.startswith('SM_Entrance')else 3390]['faces'])
 for f in d.polygons:
  key=d.materials[f.material_index].name;ps=[]
  for li in f.loop_indices:
   p=wp(d.vertices[d.loops[li].vertex_index].co);u=uv.data[li].uv.copy()if uv else (0,0)
   if f.index in overlay:p.y=919.5
   if f.index in bodycaps:
    p.y=min(p.y,919.4)
    if all(wp(d.vertices[d.loops[j].vertex_index].co).y>918 for j in f.loop_indices):key=overlaymat;u=np.array([p.x,p.z,1])@fit
   if f.index in trimcaps:
    if label.startswith('SM_Entrance'):p.y=min(p.y,924.4)
    else:p.y-=36.62
   if f.index in grey and p.y>882:
    key=overlaymat;u=np.array([p.x,p.z,1])@fit
   ps.append((*p,u[0],u[1]))
  g.face(ps,key,f.use_smooth)
 for ob in list(bpy.data.objects):
  if ob not in before:bpy.data.objects.remove(ob,do_unlink=True)
 return g

def save(g,name,label=None,block=False):
 o=g.make(name);bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;p=O/'models'/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(p),use_selection=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False);manifest['assets'].append(dict(name=name,fbx=str(p),target_actor=label,collision=block,materials=g.names));print('JOIN_ASSET',name,flush=True)
for label,n,ov,gr in [('SM_Entrance_p0_m2_Solid','SM_HJ_EntranceStairAligned',2368,2367),('SM_Upper_p0_m2_Solid','SM_HJ_UpperStairAligned',3408,None)]:save(copy(label,ov,gr),n,label)
# Continuous support beneath the two door thresholds. It meets both existing
# floors, without bridging any open part of the stairs.
key='HJ_HallCoating';c=rows['SM_Upper_p0_m2_Solid']['meshes'][0];sourcekey='HFP_HallCoating';idx=next(i for i,p in enumerate(c['materials'])if 'HFP_HallCoating' in p);manifest['materials'][key]=dict(reuse_asset=c['materials'][idx].split('.')[0]);material_info[key]=dict(color=[.1,.09,.07],roughness=.8)
g=Geometry();g.box([-2535.28,868,329.8],[-2337.28,946,332.53],key);g.box([-2534.51,868,-4],[-2362.03,946,.99],key);save(g,'SM_HJ_StairDoorThresholds',block=True)
# Small real brackets connect the banners to the gallery wall and handrail.
key='HJ_Bracket';material_info[key]=dict(color=[.04,.04,.04],roughness=.65);manifest['materials'][key]=dict(reuse_asset='/Game/SeniorSendOff/House/Materials/MI_SSO_MetalDark')
g=Geometry()
for x in [-68,68]:g.box([x-1.5,-572,429.6],[x+1.5,-543,434.6],key)
for x in [-1101,-998]:g.box([x-1.5,-550,487.5],[x+1.5,-543,494],key)
save(g,'SM_HJ_GalleryBannerBrackets')
(O/'manifest.json').write_text(json.dumps(manifest,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseJoinRepairs.blend'));print('HOUSE_JOIN_GEOMETRY_PASS',flush=True)
