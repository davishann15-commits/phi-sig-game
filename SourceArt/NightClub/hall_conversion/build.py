"""Replace the hallway illusion with a finite upstairs club wing inside the house."""
import bpy,json,pathlib,math
from mathutils import Vector
from mathutils.bvhtree import BVHTree
B=pathlib.Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/NightClub')
exec((B/'expansion2_geometry/build_expansion2.py').read_text().split('\nassets={}')[0],globals())
W=B/'hall_conversion';O=W/'geometry';(O/'models').mkdir(parents=True,exist_ok=True)
rows={r['label']:r for r in json.loads((W/'context/actors.json').read_text())}
manifest=dict(assets=[],materials={},upper_regions=[
 [[-2739,-2188,332.53],[-1432,-1540.6,622.71]],
 [[-2552.4,-1540.8,332.53],[-2228.3,-1509,622.71]],
 [[-2130,-1541,332.53],[-1540,-1495,622.71]]],
 notes=['Finite upstairs club wing within current exterior footprint.','Only scoped room partitions cut; all doors and pole retained.','No edits to Boywick, Pigsty, other rooms or ground club footprint.','Existing six native club lights reused; three infinite-hall lamps removed.'])
for n in ['M_CP_WornFloor','M_CP_WornCharcoal','M_CP_NeonFloorLine','M_CP_WornUpholsteredPanel','M_CP_ScuffedMetal','M_CP_BrushedEdges','M_CP_NeonCyan','M_CP_NeonPink','M_CP_ChippedWalnut','M_CP_Rubber']:
 material_info[n]=dict(color=[.06,.05,.07],roughness=.75)
 manifest['materials'][n]=dict(reuse_asset='/Game/SeniorSendOff/House/NightClub/ConnectionPolish/'+n)

def copy(g,label,cuts=()):
 r=rows[label];assert all(abs(v)<.001 for v in r['location']+r['rotation']),label
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=r['meshes'][0]['fbx']);sc=r['scale'];count=0
 for o in [o for o in bpy.data.objects if o not in before and o.type=='MESH' and not o.name.startswith('UCX')]:
  d=o.data;uv=d.uv_layers.active
  for f in d.polygons:
   ps=[]
   for li in f.loop_indices:
    p=o.matrix_world@d.vertices[d.loops[li].vertex_index].co;q=uv.data[li].uv if uv else(0,0)
    ps.append((p.x*100*sc[0],-p.y*100*sc[1],p.z*100*sc[2],q[0],q[1]))
   key=str(d.materials[f.material_index].name).split('.')[0];asset=r['meshes'][0]['materials'][f.material_index]
   material_info.setdefault(key,dict(color=[.06,.05,.07],roughness=.75));manifest['materials'][key]=dict(reuse_asset=asset.split('.')[0])
   pieces=[ps]
   for lo,hi in cuts:
    planes=[]
    for axis in range(3):
     n=[0,0,0];n[axis]=1;planes.append((tuple(n),hi[axis]));n=[0,0,0];n[axis]=-1;planes.append((tuple(n),-lo[axis]))
    pieces=[q for p in pieces for q in subtract(p,planes)]
   for p in pieces:g.face(p,key,f.use_smooth)
   count+=1
 for o in list(bpy.data.objects):
  if o not in before:bpy.data.objects.remove(o,do_unlink=True)
 g.stats=dict(source_actor=label,cut_boxes=cuts,source_polygons=count)
objects={}
def save(g,n,target=None,block=False):
 for key in g.names:
  if key not in manifest['materials']:manifest['materials'][key]=dict(reuse_asset='/Game/SeniorSendOff/House/NightClub/'+key)
 o=g.make(n);objects[n]=o;bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 p=O/'models'/(n+'.fbx');bpy.ops.export_scene.fbx(filepath=str(p),use_selection=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False)
 o.data.calc_loop_triangles();manifest['assets'].append(dict(name=n,fbx=str(p),target_actor=target,collision=block,triangles=len(o.data.loop_triangles),source=g.stats));print('CLUB_HALL_ASSET',n,len(o.data.loop_triangles),flush=True)

# A 5.9 m broad join into the pole room, and removal of the former end-room east partition.
# Leave the original headers and hallway-facing door wall intact.
opening=([-2130,-1544,332.50],[-1540,-1495,600])
partition=([-2080,-2047,332.50],[-2056,-1540.55,600])
for lab,n,block in [('HallEnd_SM_HallEnd_ThreeRoomsShell','SM_NCH_OpenedHallEndShell',False),('HallEnd_COL_HallEnd_ThreeRoomsShell','COL_NCH_OpenedHallEndShell',True)]:
 g=Geometry();copy(g,lab,[opening,partition]);save(g,n,lab,block)
g=Geometry();copy(g,'NightClub_SM_NC_UpperWallLining',[opening]);save(g,'SM_NCH_OpenedUpperLining','NightClub_SM_NC_UpperWallLining')
g=Geometry();copy(g,'NightClub_SM_NCR_UpperExtensionCollision',[opening]);save(g,'COL_NCH_OpenedUpperExtension','NightClub_SM_NCR_UpperExtensionCollision',True)
g=Geometry();copy(g,'HallEnd_SM_HallEnd_ThreeRoomsTrim',[opening,partition,([-2740,-2047,332],[-1432,-1540.5,355]),([-2212.54,-1509.05,332.4],[-1489.98,-973.48,352])]);save(g,'SM_NCH_RetainedTrim','HallEnd_SM_HallEnd_ThreeRoomsTrim')
# Existing upper polish wall panels must follow the newly opened mouth.
g=Geometry();copy(g,'NightClub_Polish_ArchitecturalDetails',[opening]);save(g,'SM_NCH_RetainedClubDetail','NightClub_Polish_ArchitecturalDetails')

# Continuous load-bearing floor, an enclosed finite perimeter and a black ceiling.
g=Geometry()
g.box([-2739,-2188,327.2],[-1432,-1540.6,332.53],'M_NC_FloorBlack')
g.box([-2130,-1544,327.2],[-1540,-1495,332.53],'M_NC_FloorBlack')
g.box([-2739,-2188,622.71],[-1432,-1540.6,628.21],'M_NC_CeilingBlack')
# Close the old hallucination's far opening against the real front boundary.
g.box([-2751,-2200,332.53],[-1420,-2188,628.21],'M_CP_WornCharcoal')
g.box([-2751,-2188,332.53],[-2739,-2040,628.21],'M_CP_WornCharcoal')
g.box([-1432,-2188,332.53],[-1420,-1531,628.21],'M_CP_WornCharcoal')
g.box([-1540,-1543,332.53],[-1420,-1531,628.21],'M_CP_WornCharcoal')
# Finish the original room surfaces without extending into neighboring rooms.
g.box([-2739,-2046,333.1],[-2737.8,-1540.8,622.65],'M_CP_WornCharcoal')
g.box([-2739,-2188,621.7],[-1432,-1540.6,622.7],'M_NC_CeilingBlack')
# Original north door wall: lining split around the preserved door aperture.
for lo,hi in [([-2739,-1542,333.1],[-2498.7,-1540.8,622.65]),([-2290.7,-1542,333.1],[-2228.4,-1540.8,622.65]),([-2498.7,-1542,572.5],[-2290.7,-1540.8,622.65]),([-2228.4,-1542,333.1],[-2130,-1540.8,622.65])]:g.box(lo,hi,'M_CP_WornCharcoal')
# Scuffed lintel visually unifies the different source partitions.
g.box([-2130,-1543,600],[-1540,-1495,622.71],'M_CP_WornCharcoal')
save(g,'SM_NCH_FiniteWingStructure',block=True)

# Worn PBR floor plus thin luminous grid on the new dance area. The pole aperture is untouched.
g=Geometry();g.box([-2738,-2187,332.6],[-1433,-1540.6,333.1],'M_CP_WornFloor')
g.box([-2130,-1544,332.6],[-1540,-1495,333.1],'M_CP_WornFloor')
for x in range(-2160,-1433,80):g.box([x,-2160,333.12],[x+.42,-1570,333.17],'M_CP_NeonFloorLine')
for y in range(-2160,-1570,80):g.box([-2160,y,333.12],[-1460,y+.42,333.17],'M_CP_NeonFloorLine')
save(g,'SM_NCH_UpstairsDanceFloor')
# Acoustic panels, screws, brass seams and low picture-frame ribbons, all batched decor.
g=Geometry()
for x in [-2640,-2480,-2320,-1940,-1780,-1620]:
 g.box([x-56,-2186,420],[x+56,-2182,559],'M_CP_WornUpholsteredPanel')
 for xx in [x-57,x+56.5]:g.box([xx,-2181.5,422],[xx+.5,-2181,557],'M_CP_BrushedEdges')
for y in [-1750,-1910]:g.box([-2736.5,y-57,420],[-2732,y+57,559],'M_CP_WornUpholsteredPanel')
for lo,hi in [([-2737,-2187,333.2],[-1433,-2185,345]),([-2737,-2187,333.2],[-2735,-1543,345]),([-1435,-2187,333.2],[-1433,-1543,345])]:g.box(lo,hi,'M_CP_ScuffedMetal')
for x in [-2720,-2470,-2180,-1860,-1530]:
 g.box([x,-2187,604],[x+3,-1542,611],'M_CP_ScuffedMetal')
 g.box([x+.9,-2150,603.5],[x+2.1,-1580,604],'M_CP_NeonFloorLine')
# Symmetric trim framing a single wide connection; no freestanding bars in the opening.
for x in[-2134,-1540]:g.box([x,-1544,333.1],[x+4,-1495,600],'M_CP_ScuffedMetal')
g.box([-2130,-1544,596],[-1540,-1495,600],'M_CP_ScuffedMetal')
# Downstairs DJ backdrop gains recessed panels, a slim shelf and cabling without moving the booth.
for x in [-2810,-2650,-2490,-2330]:
 g.box([x-55,-2185,86],[x+55,-2181,234],'M_CP_WornUpholsteredPanel')
 for z in[86,234]:g.box([x-55,-2180,z],[x+55,-2179,z+.5],'M_CP_BrushedEdges')
for z in[240,244]:g.box([-2870,-2180,z],[-2250,-2179,z+.6],'M_CP_NeonFloorLine')
g.box([-2870,-2178,83],[-2250,-2157,87],'M_CP_ChippedWalnut')
save(g,'SM_NCH_ClubFinishDetails')
# Exact geometry probes for all new permanent floor support and the wide passage.
def tree(o):return BVHTree.FromPolygons([v.co for v in o.data.vertices],[list(f.vertices)for f in o.data.polygons],all_triangles=False)
trees={n:tree(o)for n,o in objects.items()}
def ray(n,p,d,l):return trees[n].ray_cast(Vector((p[0]/100,-p[1]/100,p[2]/100)),Vector((d[0],-d[1],d[2])),l/100)[0]
checks={}
for n in ['COL_NCH_OpenedHallEndShell','COL_NCH_OpenedUpperExtension','SM_NCH_FiniteWingStructure','SM_NCH_OpenedUpperLining','SM_NCH_RetainedClubDetail']:
 checks[n+'_main_join_clear']=all(ray(n,[x,-1400,z],[0,-1,0],300)is None for x in[-2100,-1950,-1800,-1570] for z in[354,465,577])
 checks[n+'_wing_join_clear']=all(ray(n,[-2250,y,z],[1,0,0],450)is None for y in[-1730,-1840,-1990]for z in[354,465,577])
checks['wing_floor_supported']=all(ray('SM_NCH_FiniteWingStructure',[x,y,345],[0,0,-1],25)is not None for x in[-2690,-2400,-2100,-1800,-1470]for y in[-2160,-2000,-1770,-1565])
checks['finite_far_wall']=ray('SM_NCH_FiniteWingStructure',[-2390,-2100,465],[0,-1,0],150)is not None
checks['east_wall_closed']=ray('SM_NCH_FiniteWingStructure',[-1500,-1900,465],[1,0,0],150)is not None
checks['old_door_clear']=all(ray('SM_NCH_FiniteWingStructure',[x,-1475,z],[0,-1,0],100)is None for x in[-2450,-2395,-2340] for z in[360,465,550])
manifest['geometry_checks']=checks
(O/'manifest.json').write_text(json.dumps(manifest,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(O/'ClubHallConversion.blend'))
print('CLUB_HALL_GEOMETRY',json.dumps(checks),flush=True);assert all(checks.values()),[k for k,v in checks.items()if not v]
