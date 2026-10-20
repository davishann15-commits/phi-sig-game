"""Fill measured vacant ground-floor space inside the authored exterior. Upstairs stays intact."""
import bpy,json,pathlib,math,re
B=pathlib.Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/NightClub')
exec((B/'expansion2_geometry/build_expansion2.py').read_text().split('\nassets={}')[0],globals())
W=B/'exterior_fill';O=W/'geometry';(O/'models').mkdir(parents=True,exist_ok=True)
rows={r['label']:r for r in json.loads((W/'context/actors.json').read_text())}
manifest=dict(assets=[],materials={},lower_regions=[
 [[-2750.44,-2188,.88],[-2152.04,-1483.4,288.464]],
 [[-2212.51019,-1740,.88],[-1432,-1509.02675,288.464]],
 [[-3388,-2188,.88],[-1432,-1522,288.464]],
 [[-1490,-1509.02675,.88],[-1432,-1030,327.03]]],notes=['Inside existing exterior footprint only.','No upstairs changes.','Hallways, neighboring rooms, existing doors and pole remain.','No new real lights.'])

for n in ['M_CP_WornFloor','M_CP_WornCharcoal','M_CP_NeonFloorLine','M_CP_WornUpholsteredPanel','M_CP_ScuffedMetal']:
 material_info[n]=dict(color=[.06,.05,.07],roughness=.75);manifest['materials'][n]=dict(reuse_asset='/Game/SeniorSendOff/House/NightClub/ConnectionPolish/'+n)

def copy(g,label,cuts=()):
 r=rows[label]; assert all(abs(v)<.001 for v in r['location']+r['rotation']),label
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=r['meshes'][0]['fbx']);sc=r['scale']
 for o in [o for o in bpy.data.objects if o not in before and o.type=='MESH' and not o.name.startswith('UCX')]:
  d=o.data;uv=d.uv_layers.active
  for f in d.polygons:
   ps=[]
   for li in f.loop_indices:
    p=o.matrix_world@d.vertices[d.loops[li].vertex_index].co;q=uv.data[li].uv if uv else(0,0)
    ps.append((p.x*100*sc[0],-p.y*100*sc[1],p.z*100*sc[2],q[0],q[1]))
   key=str(d.materials[f.material_index].name).split('.')[0]
   asset=r['meshes'][0]['materials'][f.material_index]
   material_info.setdefault(key,dict(color=[.06,.05,.07],roughness=.75))
   manifest['materials'][key]=dict(reuse_asset=asset.split('.')[0])
   pieces=[ps]
   for lo,hi in cuts:
    planes=[]
    for axis in range(3):
     n=[0,0,0];n[axis]=1;planes.append((tuple(n),hi[axis]));n=[0,0,0];n[axis]=-1;planes.append((tuple(n),-lo[axis]))
    pieces=[q for p in pieces for q in subtract(p,planes)]
   for p in pieces:g.face(p,key,f.use_smooth)
 for o in list(bpy.data.objects):
  if o not in before:bpy.data.objects.remove(o,do_unlink=True)
 g.stats=dict(source_actor=label,cuts=cuts)

def save(g,n,target=None,block=False):
 o=g.make(n);bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 p=O/'models'/(n+'.fbx');bpy.ops.export_scene.fbx(filepath=str(p),use_selection=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False)
 o.data.calc_loop_triangles();manifest['assets'].append(dict(name=n,fbx=str(p),target_actor=target,collision=block,materials=g.names,triangles=len(o.data.loop_triangles)));print('CLUB_FILL_ASSET',n,len(o.data.loop_triangles),flush=True)

maincuts=[([-2282,-1753,-11],[-1477,-1509.02,327.04]),([-1490.02,-1522,-11],[-1477,-1030,327.04])]
loungecuts=[([-2771,-2167,-1],[-2748.5,-1522,288.48]),([-2771,-2167,-1],[-2131,-2144.7,288.48]),([-2154,-2167,-1],[-2131,-1516,288.48])]
g=Geometry();copy(g,'NightClub_SM_NC_LowerRoomShell',maincuts);save(g,'SM_NCF_LowerOpenedShell','NightClub_SM_NC_LowerRoomShell',True)
for label,n,block in [('SM_Room102_Shell','SM_NCF_LoungeOpenedShell',False),('COL_Room102_Shell','COL_NCF_LoungeOpenedShell',True)]:
 g=Geometry();copy(g,label,loungecuts);save(g,n,label,block)
g=Geometry();copy(g,'NightClub_SM_NCW_LoungeWallLining',loungecuts);save(g,'SM_NCF_RetainedLoungeLining','NightClub_SM_NCW_LoungeWallLining')
g=Geometry();copy(g,'NightClub_Polish_ArchitecturalDetails',[([-2226,-1760,0],[-1470,-1489,320]),([-1512,-1525,0],[-1470,-1030,320]),([-2780,-2180,0],[-2737,-1522,288.48]),([-2780,-2180,0],[-2131,-2128,288.48]),([-2171,-2180,0],[-2131,-1516,288.48])]);save(g,'SM_NCF_RetainedClubDetails','NightClub_Polish_ArchitecturalDetails')

# Union rectangles are partitioned into nonoverlapping cells. Floor support continues
# without convex collision hulls or face seams, while the upstairs hole is untouched.
main=[[-2212.51019,-1509.02675,.88],[-1490,-973.5,327.03]]
rects=[main]+manifest['lower_regions']
xs=sorted(set(r[j][0]for r in rects for j in(0,1)));ys=sorted(set(r[j][1]for r in rects for j in(0,1)))
g=Geometry();floor=Geometry()
for x,X in zip(xs,xs[1:]):
 for y,Y in zip(ys,ys[1:]):
  covering=[r for r in rects if r[0][0]<=(x+X)/2<=r[1][0] and r[0][1]<=(y+Y)/2<=r[1][1]]
  if not covering:continue
  floor.box([x,y,.92],[X,Y,1.14],'M_CP_WornFloor')
  # Additional support and ceiling only beyond the original main/chair/return floors.
  old=[main,[[-2750.44,-2146.65,.88],[-2152.04,-1483.4,288.464]],[[-2225,-1740,.88],[-1970,-1483.4,288.464]]]
  # Current shell return was removed; restore that support separately below.
  in_main=main[0][0] <= (x+X)/2 <= main[1][0] and main[0][1] <= (y+Y)/2 <= main[1][1]
  in_chair=old[1][0][0] <= (x+X)/2 <= old[1][1][0] and old[1][0][1] <= (y+Y)/2 <= old[1][1][1]
  if not in_main and not in_chair:
   top=min(r[1][2]for r in covering);g.box([x,y,-10],[X,Y,.88],'M_NC_FloorBlack');g.box([x,y,top],[X,Y,top+5.5],'M_NC_CeilingBlack')
# Small split cells crossing the previous lounge south border are partitioned by its original Y.
# A broad additional base under every added volume ensures no incidental source seam can open.
for lo,hi in manifest['lower_regions']:
 g.box([lo[0],lo[1],-12],[hi[0],hi[1],.88],'M_NC_FloorBlack')
# Cross the original shell's rounded FBX coordinates with continuous support;
# independent collision triangles must not end on the exact internal join.
g.box([-2212.51019,-1513,-12],[-1432,-1505,.88],'M_NC_FloorBlack')
g.box([-1494,-1509.02675,-12],[-1486,-1030,.88],'M_NC_FloorBlack')
# Low ceiling under the existing upstairs shell over the new connected front band.
g.box([-3388,-2188,288.464],[-1432,-1522,294],'M_NC_CeilingBlack')
g.box([-2212.51019,-1522,288.464],[-1432,-1509.02675,294],'M_NC_CeilingBlack')
# Finish the change in ceiling height with a charcoal lintel beneath the existing
# upstairs slab. It covers the exposed original wall header without editing that
# floor or any neighboring shell, and leaves 2.88 m clear throughout the opening.
g.box([-2250,-1522,288],[-1428,-1505,332.6],'M_CP_WornCharcoal')
g.box([-2247,-1522,280],[-2198,-1484,288.464],'M_CP_WornCharcoal')
# Black inner faces touch the brick shell's interior, never change the outdoor outline.
for lo,hi in [
 ([-3392,-2192,.88],[-3388,-1522,288.464]),
 ([-3392,-2192,.88],[-1432,-2188,288.464]),
 ([-1432,-2192,.88],[-1428,-1030,327.03]),
 ([-3388,-1522,.88],[-2750.44,-1518,288.464]),
 ([-1490,-1030,.88],[-1432,-1026,327.03])]:g.box(lo,hi,'M_CP_WornCharcoal')
save(g,'SM_NCF_ExteriorFillShell',block=True)
# Emissive joint grid shares the same world spacing and wear as the earlier floor.
for x in range(-3360,-1432,80):
 for y in range(-2160,-970,80):
  if any(r[0][0]<x<r[1][0] and r[0][1]<y<r[1][1]for r in rects):
   if x+78 < -1432 and any(r[0][0]<x+78<r[1][0] and r[0][1]<y<r[1][1]for r in rects):floor.box([x,y,1.15],[x+78,y+.4,1.19],'M_CP_NeonFloorLine')
   if any(r[0][0]<x<r[1][0] and r[0][1]<y+78<r[1][1]for r in rects):floor.box([x,y,1.15],[x+.4,y+78,1.19],'M_CP_NeonFloorLine')
save(floor,'SM_NCF_ContinuousDanceFloor','NightClub_SM_NC_LowerDanceFloorInlay')
# Trim and acoustic panels sit against the new boundary, never inside walking lanes.
g=Geometry()
for lo,hi in [([-3387,-2187,1.2],[-1433,-2185,13]),([-3387,-2187,1.2],[-3385,-1523,13]),([-1435,-2187,1.2],[-1433,-1031,13]),([-3387,-1525,1.2],[-2751,-1523,13])]:g.box(lo,hi,'M_CP_ScuffedMetal')
for x in(-3240,-3060,-2880,-1800,-1600):g.box([x-54,-2186,85],[x+54,-2181,213],'M_CP_WornUpholsteredPanel')
for y in(-1690,-1950):g.box([-3387,y-60,85],[-3382,y+60,215],'M_CP_WornUpholsteredPanel')
save(g,'SM_NCF_NewPerimeterDetails')
(O/'manifest.json').write_text(json.dumps(manifest,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(O/'ClubExteriorFill_source.blend'))
