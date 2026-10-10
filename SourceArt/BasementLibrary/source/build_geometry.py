import bpy,json,re
from pathlib import Path
from mathutils import Vector
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library')
# Load the shared modeling primitives, without executing the furniture build.
source=(R/'source/build_library.py').read_text();exec(compile(source.split('def swivel(')[0],str(R/'source/build_library.py'),'exec'))
doc=json.loads((R/'manifest.json').read_text());native=json.loads((R/'context/actors.json').read_text())
geometry_ids={'EntryWallDoorClear','Collision_EntryWallDoorClear','ServiceAnnexShell','MechanicalDoorSource'}
doc['assets']=[a for a in doc['assets'] if a['id'] not in geometry_ids]
doc['placements']=[p for p in doc['placements'] if p['asset']!='ServiceAnnexShell']
lookup={}
for a in native:
 for c in a['components']:
  for p in c['materials']:
   if p:lookup[p.rsplit('/',1)[-1].split('.')[0]]=p
def export_world(parts,name,kind='architecture',source_actor=None,unscale=False):
 mats=[]
 for o in parts:
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);uv_auto(o)
  if unscale:
   for v in o.data.vertices:v.co.x/=1.8;v.co.y/=1.8
  for m in o.data.materials:
   if m not in mats:mats.append(m)
 for o in parts:
  old=list(o.data.materials);ix=[mats.index(old[p.material_index])for p in o.data.polygons];o.data.materials.clear()
  for m in mats:o.data.materials.append(m)
  for p,i in zip(o.data.polygons,ix):p.material_index=i
 ob=DM['join_meshes'](parts,name,mats);bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
 f=R/'models'/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(f),use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
 paths=[]
 for m in mats:
  k=re.sub(r'\.\d+$','',m.name)
  paths.append(lookup.get(k,DEST+'/'+k+'.'+k))
 row=dict(id=name.replace('SM_Lib_','').replace('COL_Lib_',''),asset_name=name,intact_fbx=str(f),material_slots=paths,material_names=[m.name for m in mats],behavior=kind,source_actor=source_actor,damage=False,vertices=len(ob.data.vertices),faces=len(ob.data.polygons))
 if name.startswith('COL_'):row['id']='Collision_'+row['id']
 doc['assets'].append(row)
 for o in [ob,*parts]:DM['discard'](o)
 return row
# Rebuild only the entrance end wall's door-sized region, retaining the rest of
# the original imported shell, UVs, normals, light fixtures and adjacent stairs.
xmin,xmax=-27.88,-26.14;bottom,top=-3.278,-1.20
cutbottom=-3.325  # Extend the opening below the floor so the capsule clears the sill.
audit=[]
for label in ['SM_Basement_p0_m2_Solid','COL_Basement_p0_m2_Solid']:
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 bpy.ops.import_scene.fbx(filepath=str(R/'context/fbx'/(label+'.fbx')))
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith(('UCX_','UBX_','USP_','UCP_')))
 if not o.data.materials:o.data.materials.append(bpy.data.materials.new('Collider'))
 o.scale.x*=1.8;o.scale.y*=1.8;bpy.context.view_layer.update()
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 ps=S['components'](o);parts=[];cut=[];remove=[]
 for p in ps:
  lo,hi,sz=p['lo'],p['hi'],p['size'];iswall=sz.y<.6 and lo.y< -7.65 and hi.y< -7.5 and lo.x<xmin and hi.x>xmax and hi.z>bottom and lo.z<top
  oldtable=label.startswith('SM') and any(any(t in m for t in ['WoodOak','FeltGreen','GameCloth'])for m in p['materials']) and lo.x<-26 and hi.x>-28.6 and lo.y<-3.8 and hi.y>-6.3 and hi.z<-2.40
  if oldtable:remove.extend(p['faces']);continue
  q,used=S['submesh'](o,p['faces'],'retained')
  if not iswall:parts.append(q);continue
  cap=0
  left=DM['clipped'](q,Vector((xmin,0,0)),Vector((1,0,0)),False,cap);right=DM['clipped'](q,Vector((xmax,0,0)),Vector((1,0,0)),True,cap)
  middle=DM['clipped'](q,Vector((xmin,0,0)),Vector((1,0,0)),True,cap)
  if middle:
   qq=DM['clipped'](middle,Vector((xmax,0,0)),Vector((1,0,0)),False,cap);DM['discard'](middle);middle=qq
  if middle:
   above=DM['clipped'](middle,Vector((0,0,top)),Vector((0,0,1)),True,cap);below=DM['clipped'](middle,Vector((0,0,cutbottom)),Vector((0,0,1)),False,cap)
   parts.extend(q for q in [above,below]if q);DM['discard'](middle)
  parts.extend(q for q in [left,right]if q);DM['discard'](q);cut.extend(p['faces'])
 if not cut:raise RuntimeError('End wall was not found in '+label)
 name=('SM' if label.startswith('SM')else 'COL')+'_Lib_EntryWallDoorClear';export_world(parts,name,source_actor=label)
 audit.append(dict(source=label,door_wall_faces=len(cut),removed_old_table_faces=len(remove),opening_world_cm=[xmin*100,xmax*100,cutbottom*100,top*100]))
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
g='ServiceAnnexShell';wallpaint=mat('ServiceWallPaint',(.47,.46,.40),.93)
box('utility north wall',(-27.20,-11.32,-2.135),(3.60,.15,2.31),wallpaint,g,.003)
for x in [-29.0,-25.40]:box('utility side wall',(x,-9.64,-2.135),(.15,3.36,2.31),wallpaint,g,.003)
box('utility ceiling',(-27.20,-9.64,-.955),(3.67,3.44,.065),white,g,.002)
box('utility floor',(-27.20,-9.64,-3.31),(3.67,3.44,.039),floor,g,.002)
# Dark metal jamb, pale outer trim and shallow kick plate follow the photograph.
for x in [xmin+.037,xmax-.037]:box('mechanical door jamb',(x,-7.881,-2.225),(.075,.20,2.105),black,g,.004)
box('door lintel',((xmin+xmax)/2,-7.881,-1.175),(xmax-xmin,.20,.075),black,g,.004)
for x in [xmin-.027,xmax+.027]:box('outer white casing',(x,-7.715,-2.20),(.051,.04,2.20),white,g,.005)
box('top white casing',((xmin+xmax)/2,-7.715,-1.11),(xmax-xmin+.105,.04,.060),white,g,.005)
box('fire control panel',(-28.913,-9.2,-1.78),(.09,.40,.53),red,g,.015)
box('fire control inset',(-28.855,-9.2,-1.78),(.015,.25,.31),black,g,.002)
tube('service conduit',[(-28.895,-9.2,-1.49),(-28.895,-9.2,-1.04),(-27.2,-9.2,-1.04)],.014,silver,g)
box('utility light',(-27.25,-9.6,-1.009),(.64,.16,.018),warm,g,.003)
row=export_world(G[g],'SM_Lib_ServiceAnnexShell');G.pop(g);doc['placements'].append(dict(asset=row['id'],name='Mechanical service alcove',location=[0,0,0],yaw=0))
g='MechanicalDoorSource';cx=(xmin+xmax)/2;w=xmax-xmin-.16;h=2.052;zc=bottom+h/2;y=-7.88
box('brown solid service leaf',(cx,y,zc),(w,.076,h),brown,g,.010)
box('metal kick plate',(cx,y+.040,bottom+.16),(w-.12,.004,.25),black,g,.005)
box('lock face',(xmax-.235,y+.054,bottom+.95),(.057,.024,.14),black,g,.005)
cyl('silver lever pivot',(xmax-.235,y+.077,bottom+.95),.025,.032,silver,g,axis='Y')
tube('silver service-door lever',[(xmax-.235,y+.096,bottom+.95),(xmax-.33,y+.096,bottom+.945),(xmax-.35,y+.09,bottom+.939)],.009,silver,g)
box('service sign',(cx,y+.041,top-.39),(.99,.002,.47),paper,g,.001)
for textbody,z,size in [('MECHANICAL ROOM',top-.275,.071),('FIRE PANEL INSIDE',top-.355,.057),('AUTHORIZED ACCESS',top-.47,.038)]:text('service sign text',textbody,(cx,y+.044,z),size,red,g,rotation=(math.pi/2,0,math.pi))
for z in [bottom+.20,bottom+1.05,top-.13]:cyl('door hinge',(xmin+.08,y,z),.011,.09,silver,g)
row=export_world(G[g],'SM_Lib_MechanicalDoorSource','door',unscale=True);G.pop(g)
row.update(hinge=[(xmin+.08)*100,788,-327.8],doorway_center=[cx*100,788,zc*100],closed_yaw=0,open_yaw=95,panel_center=[cx*100,788,zc*100],panel_half_extent=[w*50,4.5,h*50],locked=False)
for name,value in manifest['materials'].items():doc['materials'].setdefault(name,value)
doc['geometry_audit']=audit
doc['refresh_assets']=sorted(set(doc.get('refresh_assets',[]))|geometry_ids)
(R/'manifest.json').write_text(json.dumps(doc,indent=2));(R/'geometry-audit.json').write_text(json.dumps(audit,indent=2))
print('LIBRARY_GEOMETRY_READY',len(doc['assets']),flush=True)
