"""Club/102 corner join, reusing the existing chair-room envelope and upstairs footprint."""
import bpy,json,pathlib,math
from mathutils import Vector
from mathutils.bvhtree import BVHTree
B=pathlib.Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/NightClub')
exec((B/'expansion2_geometry/build_expansion2.py').read_text().split('\nassets={}')[0],globals())
O=B/'wrap_refit/geometry';(O/'models').mkdir(parents=True,exist_ok=True)
fresh={r['label']:r for r in json.loads((B/'wrap_refit/context/actors.json').read_text())}
old={r['label']:r for r in json.loads((B/'expansion2_context/actors.json').read_text())}
manifest=dict(assets=[],materials={},main=[[-2212.51019,-1509.02675,.88],[-1490,-973.5,327.03]],
 chair=[[-2750.44,-2146.65,.88],[-2152.04,-1483.4,288.464]],
 connector=[[-2152.04,-1740,.88],[-1970,-1509.02675,288.464]],
 upstairs_restored_to_pre_annex=True,notes=['No east annex on either floor.',
 'Two wide internal joins: main/south return and return/former102.',
 'Preserve102 hallway door and outer front-wall finishes.',
 'Original shared house geometry is not edited.'])
for n,r in material_info.items():manifest['materials'][n]=dict(r,reuse_asset='/Game/SeniorSendOff/House/NightClub/'+n)

def copy(g,label,cuts=(),prior=False):
 r=(old if prior else fresh)[label];src=pathlib.Path(r['meshes'][0]['fbx'])
 if not src.is_absolute():src=B/'expansion2_context/fbx'/src.name
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(src))
 objects=[o for o in bpy.data.objects if o not in before and o.type=='MESH' and not o.name.startswith('UCX')]
 scale=r['scale'];assert all(abs(v)<.001 for v in r['location']+r['rotation']),label
 faces=0
 for o in objects:
  d=o.data;uv=d.uv_layers.active
  for f in d.polygons:
   poly=[]
   for li in f.loop_indices:
    p=o.matrix_world@d.vertices[d.loops[li].vertex_index].co;q=uv.data[li].uv if uv else (0,0)
    poly.append((p.x*100*scale[0],-p.y*100*scale[1],p.z*100*scale[2],q[0],q[1]))
   key=str(d.materials[f.material_index].name).split('.')[0]
   if key not in material_info:
    asset=r['meshes'][0]['materials'][f.material_index];assert asset,(label,key)
    material_info[key]=dict(color=[.65,.65,.62],roughness=.65)
    manifest['materials'][key]=dict(material_info[key],reuse_asset=asset.split('.')[0])
   pieces=[poly]
   for lo,hi in cuts:
    planes=[]
    for axis in range(3):
     n=[0,0,0];n[axis]=1;planes.append((tuple(n),hi[axis]));n=[0,0,0];n[axis]=-1;planes.append((tuple(n),-lo[axis]))
    pieces=[q for p in pieces for q in subtract(p,planes)]
   for p in pieces:g.face(p,key,f.use_smooth)
   faces+=1
 for o in list(bpy.data.objects):
  if o not in before:bpy.data.objects.remove(o,do_unlink=True)
 g.stats=dict(source_actor=label,source_faces=faces,cut_boxes=cuts,world_baked=True)

objects={}
def save(g,name,label=None,collision=False,hidden=False):
 o=g.make(name);objects[name]=o;bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 p=O/'models'/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(p),use_selection=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False)
 o.data.calc_loop_triangles();v=[(q.co.x*100,-q.co.y*100,q.co.z*100) for q in o.data.vertices]
 manifest['assets'].append(dict(name=name,fbx=str(p),target_actor=label,collision=collision,hidden=hidden,materials=g.names,
  bounds_cm=[[min(p[i] for p in v) for i in range(3)],[max(p[i] for p in v) for i in range(3)]],triangles=len(o.data.loop_triangles)))
 print('CLUB_WRAP_ASSET',name,len(o.data.loop_triangles),flush=True)

# Both existing shells are copied; only the intersecting corner is opened.
cut=([ -2225,-1752,.75],[-1970,-1483.4,288.30])
g=Geometry();copy(g,'NightClub_SM_NC_LowerRoomShell',[cut],True)
g.box([-2225,-1740,-10],[-1970,-1483.4,.88],'M_NC_FloorBlack')
g.box([-1970,-1752,.88],[-1958,-1509.02675,306.064],'M_NC_Charcoal')
g.box([-2152.04,-1752,.88],[-1958,-1740,306.064],'M_NC_Charcoal')
g.box([-2152.04,-1752,288.464],[-1958,-1509.02675,306.064],'M_NC_CeilingBlack')
save(g,'SM_NCW_LowerMainAndReturn','NightClub_SM_NC_LowerRoomShell',True)

for label,name,hidden in [('SM_Room102_Shell','SM_NCW_Former102Shell',False),('COL_Room102_Shell','COL_NCW_Former102Shell',True)]:
 g=Geometry();copy(g,label,[cut]);save(g,name,label,hidden,hidden)

# Inexpensive interior linings. The hall-facing102 door, trim and wall are retained.
g=Geometry();z=.88;Z=288.28
for lo,hi in [
 ([-2750.44,-2146.65,z],[-2749.60,-1509.75,Z]),
 ([-2750.44,-2146.65,z],[-2152.04,-2145.80,Z]),
 ([-2152.90,-2146.65,z],[-2152.04,-1752,Z]),
 ([-2750.44,-1510.58,z],[-2555,-1509.75,Z]),
 ([-2347,-1510.58,z],[-2225,-1509.75,Z]),
 ([-2555,-1510.58,237],[-2347,-1509.75,Z]),
 ([-2225.6,-1509.75,z],[-2225,-1483.4,Z]),
 ([-2152.04,-1740.6,z],[-2132.24,-1740,Z])]:g.box(lo,hi,'M_NC_Charcoal')
g.box([-2750.44,-2146.65,288.22],[-2152.04,-1509.75,288.40],'M_NC_CeilingBlack')
save(g,'SM_NCW_LoungeWallLining',collision=False)

g=Geometry();copy(g,'NightClub_SM_NC_LowerDanceFloorInlay',prior=True)
# A continuous dark floor and modest luminous tile accents for the former storage room.
for lo,hi in [([-2749.5,-2145.7,.90],[-2152.04,-1510.6,1.04]),([-2225,-1739.4,.90],[-1970.6,-1483.4,1.04])]:g.box(lo,hi,'M_NC_FloorBlack')
for i in range(7):
 for j in range(7):
  x=-2729.5+i*83.;y=-2125.7+j*85.
  g.box([x,y,1.05],[x+79,y+81,1.11],'M_NC_FloorSlate' if (i+j)%2 else 'M_NC_FloorBlack')
for x in [-2729.5,-2646.5,-2563.5,-2480.5,-2397.5,-2314.5,-2231.5,-2148.5]:
 if x<-2153:g.box([x,-2125.7,1.12],[x+.65,-1528.7,1.17],'M_NC_FloorGlow')
for y in [-2125.7,-2040.7,-1955.7,-1870.7,-1785.7,-1700.7,-1615.7,-1530.7]:g.box([-2729.5,y,1.12],[-2152.7,y+.65,1.17],'M_NC_FloorGlow')
save(g,'SM_NCW_LowerDanceFloorInlay','NightClub_SM_NC_LowerDanceFloorInlay',False)

# Verify real face geometry before import: two corner lanes and the102 door remain open.
def tree(o):return BVHTree.FromPolygons([v.co for v in o.data.vertices],[list(f.vertices) for f in o.data.polygons],all_triangles=False)
trees={k:tree(o) for k,o in objects.items()}
def ray(k,p,d,length):return trees[k].ray_cast(Vector((p[0]/100,-p[1]/100,p[2]/100)),Vector((d[0],-d[1],d[2])),length/100)[0]
checks={}
for key in ['SM_NCW_LowerMainAndReturn','COL_NCW_Former102Shell','SM_NCW_LoungeWallLining']:
 checks[key+'_main_south_join_clear']=all(ray(key,[x,-1440,z],[0,-1,0],220) is None for x in [-2100,-2050,-2000] for z in [20,130,250])
 checks[key+'_lounge_east_join_clear']=all(ray(key,[-2250,y,z],[1,0,0],250) is None for y in [-1680,-1640,-1600] for z in [20,130,250])
checks['former102_door_clear']=all(ray('COL_NCW_Former102Shell',[x,-1400,z],[0,-1,0],170) is None for x in [-2500,-2451,-2400] for z in [20,130,210])
checks['main_east_wall_restored']=ray('SM_NCW_LowerMainAndReturn',[-1520,-1210,150],[1,0,0],90) is not None
checks['return_has_floor']=all(ray('SM_NCW_LowerMainAndReturn',[x,y,90],[0,0,-1],100) is not None for x in [-2120,-2080,-2000] for y in [-1720,-1600,-1500])
checks['lounge_floor_solid']=all(ray('COL_NCW_Former102Shell',[x,y,90],[0,0,-1],100) is not None for x in [-2730,-2510,-2290] for y in [-2120,-1900,-1670])
checks['within_pre_annex_east_edge']=all(a['bounds_cm'][1][0]<=-1477.9 for a in manifest['assets'])
(O/'manifest.json').write_text(json.dumps(manifest,indent=2));(O/'geometry-validation.json').write_text(json.dumps(dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(O/'NightClubWrap_source.blend'))
print('CLUB_WRAP_GEOMETRY',json.dumps(checks),flush=True)
assert all(checks.values()),[k for k,v in checks.items() if not v]
