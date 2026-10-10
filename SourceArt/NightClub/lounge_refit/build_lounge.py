"""Grounded modular club lounge art, offline Blender only.
UE +Y is the furniture front, Blender -Y. No map/native/common asset writes.
"""
import bpy,bmesh,os,json,math,collections,random,hashlib
import numpy as np
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion
R=Path(__file__).resolve().parent;MODEL=R/'models';MODEL.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
G=collections.defaultdict(list)
MAN={'coordinateSystem':'Blender metres -> Unreal cm [100X,-100Y,100Z]. Furniture front Unreal+Y; back Unreal-Y.',
 'assets':[],'materials':{},'props':{},'placementScale':1,
 'notes':['Grounded furniture origins local0, all stages keep same origin.',
 'Main right edge joins chaise left edge, exact rear-aligned placement in manifest.',
 'UCX hulls embedded in each FBX. Import simple collision, do not replace with complex-as-simple.',
 'Damage uses capped rigid cuts, exposed foam/wood, loosened backs, final floor-rested recognizable wreck.']}
def mat(name,c,rough=.5,metal=0,source=None):
 m=bpy.data.materials.new(name);m.diffuse_color=(*c,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 row={'color':list(c),'roughness':rough,'metallic':metal}
 if source:row['reuse_asset']=source
 MAN['materials'][name]=row;return m
D='/Game/SeniorSendOff/House/NightClub/'
LEATHER=mat('M_NC_SoftBlackLeather',(.018,.022,.027),.38,.02,D+'M_NC_SoftBlackLeather')
SEAM=mat('M_NC_BlackLeatherSeam',(.006,.008,.010),.66,0,D+'M_NC_BlackLeatherSeam')
ALU=mat('M_NC_BrushedAluminium',(.28,.31,.37),.25,.95,D+'M_NC_BrushedAluminium')
BLACK=mat('M_NC_PowderCoat',(.009,.011,.019),.48,.52,D+'M_NC_PowderCoat')
RUBBER=mat('M_NC_Rubber',(.005,.006,.008),.88,0,D+'M_NC_Rubber')
WOOD=mat('M_NCL_Walnut',(.055,.025,.014),.46)
GRAIN=mat('M_NCL_WalnutGrain',(.025,.012,.009),.60)
FOAM=mat('M_NCL_ExposedFoam',(.38,.29,.15),.99)
RAW=mat('M_NCL_SplinterWood',(.28,.17,.075),.96)
METAL=mat('M_NCL_RawMetal',(.15,.16,.18),.72,.7)
PLUM=mat('M_NCL_PlushPlum',(.080,.012,.035),.82)
TEAL=mat('M_NCL_PlushTeal',(.013,.050,.058),.83)
STITCH=mat('M_NCL_PillowThread',(.10,.12,.13),.9)
SRC=(R.parent/'assets/club_assets.py').read_text()
exec(SRC[SRC.index('def mesh('):SRC.index('# Neon stroke font')])
DS=(R.parent.parent/'HouseInteractivity/source/generate_damage_v2.py').read_text()
exec(DS[DS.index('def bbox('):DS.index('def part_mesh(')])
exec(DS[DS.index('def fracture_plane('):DS.index('def hulls_for(')])

def foot(x,y,g,h=.11):
 cyl('Brushed sectional foot',(x,y,h/2),.024,h,ALU,g,24)
 cyl('Rubber pad on floor',(x,y,.008),.025,.016,RUBBER,g,20)
 # Flared cap makes the metal support read as a real load-bearing foot.
 cyl('Foot top mounting collar',(x,y,h-.004),.032,.018,ALU,g,24)

def leather_cushion(name,center,size,g,damage=False):
 o=cube(name,center,size,LEATHER,g,min(.064,min(size)*.40),7)
 o['damage']=damage;o['kind']='foam';return o

def seat_welt(cx,cy,z,w,d,g):
 r=.045;x0=cx-w/2+r;x1=cx+w/2-r;y0=cy-d/2+r;y1=cy+d/2-r
 pts=[]
 for x,y,a0 in((x1,y1,0),(x0,y1,math.pi/2),(x0,y0,math.pi),(x1,y0,math.pi*1.5)):
  for j in range(9):a=a0+j*math.pi/16;pts.append((x+r*math.cos(a),y+r*math.sin(a),z))
 pts.append(pts[0]);tube('Cushion stitched leather piping',pts,.0027,SEAM,g,8)

def back_cushion(cx,cy,width,g,damage=False):
 # Curved closed back pad with real tuft depressions; no flat painted circles.
 nx,nz=32,22;height=.40;cz=.655;v=[];f=[]
 for back in(False,True):
  for j in range(nz+1):
   t=j/nz;z=cz+(t-.5)*height
   for k in range(nx+1):
    s=k/nx;xx=cx+(s-.5)*width;e=(math.sin(math.pi*s)*math.sin(math.pi*t))**.32
    yy=cy+(.11 if back else -.055*e)
    if not back:
     for bx in(.27,.73):
      for bz in(.33,.73):yy+=.023*math.exp(-(((s-bx)/.055)**2+((t-bz)/.073)**2))
    yy+=(z-cz)*.18;v.append((xx,yy,z))
 n=(nx+1)*(nz+1)
 for j in range(nz):
  for k in range(nx):
   a=j*(nx+1)+k;f.extend([(a,a+1,a+nx+2,a+nx+1),(n+a+nx+1,n+a+nx+2,n+a+1,n+a)])
 for k in range(nx):f.extend([(k,n+k,n+k+1,k+1),(nz*(nx+1)+k+1,n+nz*(nx+1)+k+1,n+nz*(nx+1)+k,nz*(nx+1)+k)])
 for j in range(nz):
  a=j*(nx+1);f.append((a+nx+1,n+a+nx+1,n+a,a));a+=nx;f.append((a,n+a,n+a+nx+1,a+nx+1))
 o=mesh('Contoured tufted back pad',v,f,LEATHER,g,True);o['damage']=damage;o['kind']='foam';o['backpad']=True
 for bx in(.27,.73):
  for bz in(.33,.73):
   xx=cx+(bx-.5)*width;zz=cz+(bz-.5)*height;e=(math.sin(math.pi*bx)*math.sin(math.pi*bz))**.32;yy=cy-.055*e+.023+(zz-cz)*.18
   ob=ball('Deep leather covered tuft button',(xx,yy-.001,zz),.012,SEAM,g,16,8);ob.scale.y=.28
 for z in(cz-height/2+.012,cz+height/2-.012):tube('Back pad double stitch line',[(cx-width/2+.014,cy+(z-cz)*.18-.002,z),(cx+width/2-.014,cy+(z-cz)*.18-.002,z)],.0024,SEAM,g,8)

def section_main():
 g='SM_NCL_SectionalMain'
 for x in(-1.57,1.57):
  for y in(-.35,.35):foot(x,y,g)
 foot(0,.34,g)
 o=cube('Grounded upholstered timber platform',(0,0,.245),(3.50,.95,.29),LEATHER,g,.042,6);o['damage']=True;o['kind']='wood'
 cube('Fine lower satin aluminium rail',(0,-.473,.21),(3.26,.013,.026),ALU,g,.004,3)
 # Only left arm. The right edge opens seamlessly into the separate chaise.
 leather_cushion('Rounded left arm',(-1.66,0,.495),(.18,.95,.53),g,True)
 seat_welt(-1.66,0,.754,.15,.86,g)
 leather_cushion('Continuous upper rear frame',(.04,.365,.58),(3.42,.22,.60),g,True)
 for i,x in enumerate((-1.07,0,1.07)):
  leather_cushion('Individual seat cushion '+str(i),(x,-.115,.36),(1.055,.71,.16),g,i==1)
  seat_welt(x,-.115,.397,1.04,.693,g)
  back_cushion(x,.230,1.048,g,i==1)
 return g,[([0,0,22],[175,47.5,22]),([4,-36.5,61],[171,11,27]),([-166,0,49.5],[9,47.5,26.5])]

def section_chaise():
 g='SM_NCL_SectionalChaise'
 for x in(-.32,.32):
  for y in(-.91,.91):foot(x,y,g)
 o=cube('Grounded chaise timber platform',(0,0,.245),(.90,2.10,.29),LEATHER,g,.042,6);o['damage']=True;o['kind']='wood'
 leather_cushion('Chaise upper rear frame',(0,.94,.58),(.90,.22,.60),g,True)
 # Outer right arm only at the rear lounge seating area; left rear joins main.
 leather_cushion('Rear right chaise arm',(.36,.575,.495),(.18,.95,.53),g,True)
 seat_welt(.36,.575,.754,.15,.86,g)
 leather_cushion('Long chaise cushion',(-.02,-.10,.36),(.83,1.90,.16),g,True)
 seat_welt(-.02,-.10,.397,.815,1.88,g)
 back_cushion(-.04,.805,.79,g,True)
 # A seam at the same front row as the main cushions helps the two modules read as one.
 tube('Chaise matching cushion cross seam',[(-.392,.11,.441),(.328,.11,.441)],.0022,SEAM,g,8)
 cube('Chaise satin lower front rail',(0,-1.043,.21),(.73,.013,.026),ALU,g,.004,3)
 return g,[([0,0,22],[45,105,22]),([0,-94,61],[45,11,27]),([36,-57.5,49.5],[9,47.5,26.5])]

def table(name,w,d,h):
 g=name
 top=cube('Thick hand finished walnut top',(0,0,h-.028),(w,d,.056),WOOD,g,.016,5);top['damage']=True;top['kind']='wood'
 cube('Fine dark top inset underbead',(0,0,h-.059),(w-.06,d-.06,.011),GRAIN,g,.003,3)
 for x in(-w/2+.07,w/2-.07):
  for y in(-d/2+.065,d/2-.065):
   leg=cube('Rectangular powdercoat table leg',(x,y,(h-.06)/2),(.034,.034,h-.06),BLACK,g,.007,4);leg['kind']='metal'
   cube('Rubber table floor foot',(x,y,.004),(.035,.035,.008),RUBBER,g,.0018,3)
 # Real lower shelf and side braces distinguish it from a plain box.
 shelf=cube('Lower walnut storage shelf',(0,0,.12),(w-.12,d-.11,.032),WOOD,g,.01,4);shelf['damage']=True;shelf['kind']='wood'
 for x in(-w/2+.07,w/2-.07):cube('Low transverse metal brace',(x,0,.095),(.026,d-.105,.025),BLACK,g,.003,3)
 rr=random.Random(len(name));
 for i in range(14):
  y=rr.uniform(-d/2+.023,d/2-.023);x0=rr.uniform(-w/2+.025,-w/2+w*.27);x1=rr.uniform(w*.12,w/2-.025)
  pts=[]
  for k in range(13):x=x0+(x1-x0)*k/12;pts.append((x,y+.0018*math.sin(k*.55+i),h+.0002))
  tube('Fine walnut grain',pts,.00065,GRAIN,g,5)
 return g,[([0,0,(h-.06)*50],[w*50,d*50,(h-.06)*50]),([0,0,(h-.028)*100],[w*50,d*50,2.8])]

def ottoman():
 g='SM_NCL_Ottoman'
 for x in(-.24,.24):
  for y in(-.24,.24):foot(x,y,g,.075)
 o=cube('Ottoman upholstered storage base',(0,0,.238),(.65,.65,.32),LEATHER,g,.047,6);o['damage']=True;o['kind']='foam'
 o=leather_cushion('Deep padded ottoman seat',(0,0,.405),(.65,.65,.15),g,True)
 seat_welt(0,0,.45,.63,.63,g)
 for k in(-1,1):
  tube('Soft ottoman top seam',[(k*.108,-.28,.481),(k*.108,.28,.481)],.0019,SEAM,g,8)
  tube('Soft ottoman cross seam',[(-.28,k*.108,.481),(.28,k*.108,.481)],.0019,SEAM,g,8)
 return g,[([0,0,24],[32.5,32.5,24])]

def pillow(g,ma):
 # Closed bulging cloth cushion with pinched perimeter, grounded horizontal pivot.
 n=30;v=[];f=[];w=.46;d=.46;h=.115
 for side in(-1,1):
  for j in range(n+1):
   t=j/n;y=(t-.5)*d
   for k in range(n+1):
    s=k/n;x=(s-.5)*w;e=(math.sin(math.pi*s)*math.sin(math.pi*t))**.44
    v.append((x,y,h/2+side*(.006+.046*e)))
 count=(n+1)**2
 for side in range(2):
  for j in range(n):
   for k in range(n):a=side*count+j*(n+1)+k;q=(a,a+1,a+n+2,a+n+1);f.append(q if side==1 else q[::-1])
 for k in range(n):f.extend([(k,k+1,count+k+1,count+k),(n*(n+1)+k,count+n*(n+1)+k,count+n*(n+1)+k+1,n*(n+1)+k+1)])
 for j in range(n):
  a=j*(n+1);f.extend([(a,count+a,count+a+n+1,a+n+1),(a+n,a+2*n+1,count+a+2*n+1,count+a+n)])
 mesh('Soft cloth pillow',v,f,ma,g,True)
 tube('Pillow edge sewn piping',[(-.23,-.23,h/2),(.23,-.23,h/2),(.23,.23,h/2),(-.23,.23,h/2),(-.23,-.23,h/2)],.0021,STITCH,g,8)
 for y in(-.218,.218):
  for j in range(35):x=-.21+j*.012;tube('Small cloth stitch',[(x,y,.060),(x+.004,y,.060)],.00045,STITCH,g,4)
 return g,[([0,0,5.75],[23,23,5.75])]

def bake(objects):
 for o in objects:
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
  bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='FIXED',ngon_method='BEAUTY');bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
  if o.get('weighted'):
   md=o.modifiers.new('Baked balanced corner normals','WEIGHTED_NORMAL');md.weight=50;md.keep_sharp=True;bpy.ops.object.modifier_apply(modifier=md.name)
  if not o.data.uv_layers:
   uv=o.data.uv_layers.new(name='UVMap')
   for po in o.data.polygons:
    axis=max(range(3),key=lambda i:abs(po.normal[i]));aa,bb=((1,2),(0,2),(0,1))[axis]
    for li in po.loop_indices:q=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(q[aa],q[bb])
  o.select_set(False)

def fit_floor_fragment(ob,half):
 # Rigid turning and translation preserve fragment shape while keeping a wreck
 # away from the room's walls. Never squash vertices to force the footprint.
 coords=np.array([vv.co[:] for vv in ob.data.vertices]);center=coords.mean(axis=0)
 relative=coords-center;best=None
 for degree in range(0,180):
  angle=math.radians(degree);c=math.cos(angle);s=math.sin(angle)
  rot=np.array([[c,-s],[s,c]]);xy=relative[:,:2]@rot.T
  size=xy.max(axis=0)-xy.min(axis=0)
  overflow=np.maximum(size-np.array(half)*2,0)
  score=float((overflow**2).sum())*10000+min(degree,180-degree)*.00001
  if best is None or score<best[0]:best=(score,rot,xy,size)
 assert max(best[3]-np.array(half)*2)<.001,('Fragment too large for base rectangle',ob.name,best[3],half)
 coords[:,:2]=best[2]+center[:2]
 for axis in range(2):
  lo=float(coords[:,axis].min());hi=float(coords[:,axis].max())
  shift=max(0,-half[axis]-lo)+min(0,half[axis]-hi)
  coords[:,axis]+=shift
 for vv,q in zip(ob.data.vertices,coords):vv.co=Vector(q)
 ob.data.update()

def stage_parts(base,stage,seed,half):
 if stage==0:return [copy(o) for o in base]
 rr=random.Random(seed+stage*101);parts=[]
 for i,o in enumerate(base):
  names=' '.join(m.name for m in o.data.materials).lower();kind=o.get('kind','foam' if 'leather' in names else 'wood' if 'walnut' in names else 'metal')
  capmat={'foam':FOAM,'wood':RAW,'metal':METAL}[kind]
  cp=copy(o);cp.data.materials.append(capmat);cap=len(cp.data.materials)-1
  if stage<3:
   if o.get('damage'):
    q=chip(cp,seed+i*37,cap,.045 if stage==1 else .13);discard(cp);cp=q
   if stage==2 and o.get('backpad'):
    lo,hi=bbox(cp);pivot=Vector(((lo.x+hi.x)/2,hi.y,lo.z));rigid(cp,pivot,'X',math.radians(-9))
   parts.append(cp)
  else:
   if any(s in names for s in('seam','stitch','thread')):discard(cp);continue
   lo,hi=bbox(cp)
   if o.get('damage'):
    q=chip(cp,seed+i*37,cap,.14);discard(cp);cp=q
    if max(hi-lo)>.60:
     co,no=fracture_plane(cp,seed+i*39,.50);frags=cut_pair(cp,co,no,cap);discard(cp)
    else:frags=[cp]
   else:frags=[cp]
   for j,frag in enumerate(frags):
    lo,hi=bbox(frag);center=(lo+hi)/2;floor_part(frag,center,0,rr,.045)
    # Fragments rest on the floor. Use small predictable stacking for soft cushions only.
    if kind=='foam':
     for vv in frag.data.vertices:vv.co.z+=.06 if i%2 else .025
    fit_floor_fragment(frag,half);parts.append(frag)
 return parts

def hull_boxes(parts,name,stage,defs):
 hulls=[]
 if stage<3:
  for idx,(cent,ext) in enumerate(defs):
   # Manifest coordinates are UE cm; geometry here remains Blender metres.
   o=cube('UCX_'+name+'_%02d'%idx,(cent[0]/100,-cent[1]/100,cent[2]/100),(ext[0]/50,ext[1]/50,ext[2]/50),None,'__COLLISION__')
   o.name='UCX_'+name+'_%02d'%idx;hulls.append(o)
 else:
  # A handful of bounded convex floor fragments rather than the intact tall collider.
  candidates=sorted(parts,key=lambda ob:np.prod(bbox(ob)[1]-bbox(ob)[0]),reverse=True)[:8]
  for idx,part in enumerate(candidates):
   lo,hi=bbox(part);lo.z=max(0,lo.z);center=(lo+hi)/2;size=hi-lo
   for k in range(3):size[k]=max(size[k],.012)
   o=cube('UCX_'+name+'_%02d'%idx,center,size,None,'__COLLISION__');o.name='UCX_'+name+'_%02d'%idx;hulls.append(o)
 return hulls

OBJS={};AUDIT=[]
def export(name,parts,stage,collisiondefs):
 bake(parts);bpy.ops.object.select_all(action='DESELECT')
 for o in parts:o.select_set(True)
 bpy.context.view_layer.objects.active=parts[0]
 if len(parts)>1:bpy.ops.object.join()
 o=bpy.context.object;o.name=name;o.data.name=name;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
 # Collision bounds from unjoined fragments are needed; joined object bounds supply broken fallback.
 hulls=hull_boxes([o],name,stage,collisiondefs)
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True)
 for h in hulls:h.select_set(True)
 bpy.context.view_layer.objects.active=o;file=str(MODEL/(name+'.fbx'))
 bpy.ops.export_scene.fbx(filepath=file,use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',use_mesh_modifiers=True,bake_space_transform=False,add_leaf_bones=False)
 lo,hi=bbox(o);row={'name':name,'fbx':file,'stage':stage,'triangles':len(o.data.polygons),'vertices':len(o.data.vertices),'materials':[m.name for m in o.data.materials],'boundsUnrealCm':[[100*lo.x,-100*hi.y,100*lo.z],[100*hi.x,-100*lo.y,100*hi.z]],'dimensionsCm':[100*(hi-lo)[k] for k in range(3)],'origin':'Furniture floor center local0; rootscale1. Pillows horizontal grounded pivot.','collision':'Embedded UCX convex simple collision; never complex-as-simple on moving furniture.','collisionHullCount':len(hulls),'collisionBoxesUnrealCm':collisiondefs if stage<3 else 'Ground-rested wreck bounds'}
 MAN['assets'].append(row);OBJS[name]=o
 for h in hulls:bpy.data.objects.remove(h,do_unlink=True)
 o.hide_render=True;o.select_set(False)
 return row

definitions=[(*section_main(),85),(*section_chaise(),60),(*table('SM_NCL_CoffeeTable',.90,.50,.38),19),(*table('SM_NCL_SideTable',.42,.42,.56),12),(*ottoman(),16),(*pillow('SM_NCL_PillowPlum',PLUM),.7),(*pillow('SM_NCL_PillowTeal',TEAL),.7)]
for stem,col,mass in definitions:
 base=G[stem];bake(base);hasstages='Pillow' not in stem;names={}
 for stage,suffix in((0,''),(1,'_Damaged'),(2,'_HeavilyDamaged'),(3,'_Broken')) if hasstages else[(0,'')]:
  name=stem+suffix;half=[col[0][1][0]/100+.02,col[0][1][1]/100+.02];pieces=stage_parts(base,stage,int(hashlib.md5(stem.encode()).hexdigest()[:8],16),half);row=export(name,pieces,stage,col);names[['intact','damaged','heavily_damaged','broken'][stage]]=name
 MAN['props'][stem]={'meshes':names,'massKg':mass,'breakHealth':max(18,mass*.65+18),'origin':'Floor-center local0','collision':'Use embedded UCX hulls. Both sectional modules are independent heavy interactive props.'}
 for o in base:o.hide_render=True;o.hide_set(True)
MAN['sectional']={'main':{'name':'SM_NCL_SectionalMain','placementWorldCm':[-1850,-1440,333.1],'widthCm':350,'depthCm':95,'heightCm':88,'seatHeightCm':44,'joiningEdge':'+X'},'chaise':{'name':'SM_NCL_SectionalChaise','placementWorldCm':[-1630,-1382.5,333.1],'widthCm':90,'depthCm':210,'heightCm':88,'seatHeightCm':44,'joiningEdge':'-X rear95cm'},'yaw':0,'backRearYWorldCm':-1487.5,'joinWorldXcm':-1675}
json.dump(MAN,open(R/'lounge_manifest.json','w'),indent=2)
bpy.ops.wm.save_as_mainfile(filepath=str(R/'NightClubLounge_source.blend'))
# Isolated product preview uses the exact joining offsets, with no scene/map lights changed.
def show(name,loc):
 o=OBJS[name].copy();o.data=OBJS[name].data;bpy.context.collection.objects.link(o);o.hide_render=False;o.hide_set(False);o.location=loc;return o
show('SM_NCL_SectionalMain',(0,0,0));show('SM_NCL_SectionalChaise',(2.20,-.575,0))
show('SM_NCL_CoffeeTable',(.30,-1.25,0));show('SM_NCL_SideTable',(-2.05,.06,0));show('SM_NCL_Ottoman',(-.72,-1.30,0))
p=show('SM_NCL_PillowPlum',(-1.21,.18,.46));p.rotation_euler.x=math.radians(62)
p=show('SM_NCL_PillowTeal',(1.01,.18,.46));p.rotation_euler.x=math.radians(62)
floor=mat('M_NCL_PreviewFloor',(.075,.08,.10),.86);cube('Preview floor',(0,0,-.04),(12,10,.08),floor,'__PREVIEW__')
sc=bpy.context.scene;sc.render.engine='CYCLES';sc.cycles.samples=48;sc.render.resolution_x=1500;sc.render.resolution_y=1000;sc.render.resolution_percentage=100;sc.world.color=(.09,.09,.09);sc.view_settings.view_transform='AgX'
for loc,power,col in(((-3,-4,4),800,(.75,.82,1)),((3,-1,4),700,(1,.78,.60)),((0,3,3),650,(.6,.78,1))):
 d=bpy.data.lights.new('Soft studio','AREA');d.energy=power;d.color=col;d.shape='DISK';d.size=4;o=bpy.data.objects.new('Soft studio',d);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((.30,-.3,.5))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Sectional showcase');o=bpy.data.objects.new('Sectional showcase',d);bpy.context.collection.objects.link(o);o.location=(5.0,-6.8,3.6);o.rotation_euler=(Vector((.35,-.3,.45))-o.location).to_track_quat('-Z','Y').to_euler();d.lens=55;sc.camera=o;sc.render.filepath=str(R/'lounge-preview.png');bpy.ops.render.render(write_still=True)
print('NCL_LOUNGE_ASSETS_PASS meshes='+str(len(MAN['assets']))+' triangles='+str(sum(a['triangles'] for a in MAN['assets'])))
