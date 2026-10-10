"""Noodle reference wing. Clean modular meshes, distinct furniture and loose items.
Author in current Unreal world cm; imported at unit scale. No existing art is changed.
"""
import bpy,math,json,random,collections,pathlib,numpy as np
from mathutils import Vector
B=pathlib.Path(__file__).resolve().parent;O=B/'art';(O/'models').mkdir(parents=True,exist_ok=True);(O/'textures').mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True);G=collections.defaultdict(list);MAN=dict(assets=[],materials={},placements=[],lights=[],doors=[],route=[]);rng=random.Random(2393)
def mat(n,c,r=.6,metal=0,tex=None,period=100,em=0):
 n='M_NW_'+n;m=bpy.data.materials.new(n);m.diffuse_color=(*c,1);MAN['materials'][n]=dict(color=list(c),roughness=r,metallic=metal,texture=tex,period=period,emissive=em);return m
RED=mat('DustyRed',(.20,.060,.055),.7,tex='paint',period=125)
BLUE=mat('NavyBlue',(.014,.029,.063),.48,tex='paint',period=125)
CREAM=mat('UtilityCream',(.48,.455,.40),.78,tex='paint',period=125)
CEIL=mat('Ceiling',(.65,.64,.60),.88,tex='paint',period=140)
BRICK=mat('YellowBlock',(.67,.55,.31),.72,tex='brick',period=81.28)
CONC=mat('WornConcrete',(.20,.18,.14),.87,tex='concrete',period=180)
WOOD=mat('WarmWood',(.21,.092,.036),.63,tex='wood',period=160)
TRIM=mat('IvoryTrim',(.59,.57,.50),.54,tex='paint',period=90)
BLACK=mat('WornSteel',(.014,.017,.018),.48,.65,tex='metal',period=80)
CHROME=mat('BrushedChrome',(.43,.46,.49),.27,.91)
TREAD=mat('AggregateTreads',(.23,.24,.23),.9,tex='concrete',period=95)
WHITE=mat('ApplianceEnamel',(.69,.68,.62),.36,.1,tex='paint',period=90)
RUBBER=mat('BlackRubber',(.005,.006,.008),.86)
GLASS=mat('SmokedGlass',(.014,.025,.035),.10,.58)
FELT=mat('PoolFelt',(.008,.23,.14),.98,tex='fabric',period=55)
LEATHER=mat('ChestnutLeather',(.18,.069,.029),.42,tex='leather',period=65)
TAN=mat('ReclinerTan',(.31,.27,.19),.96,tex='fabric',period=75)
PAPER=mat('Paper',(.76,.73,.65),.92)
CLOTH=mat('CharcoalCloth',(.04,.044,.049),.98,tex='fabric',period=45)
CLOTHWHITE=mat('LaundryWhite',(.48,.49,.45),.98,tex='fabric',period=45)
ORANGE=mat('DetergentOrange',(.83,.115,.008),.36)
GREEN=mat('ChairGreen',(.013,.075,.046),.52)
GOLD=mat('OldBrass',(.35,.23,.075),.34,.75)
LIGHT=mat('Fluorescent',(.75,.79,.72),.45,em=2.5)
LED=mat('BlueLCD',(.025,.26,.65),.3,em=1.1)
YELLOW=mat('CableYellow',(.65,.50,.014),.64)
POOLWHITE=mat('BilliardWhite',(.77,.76,.67),.24)
POOLYELLOW=mat('BilliardYellow',(.8,.58,.009),.24)
RUG=mat('RugOatmeal',(.34,.29,.20),.98,tex='fabric',period=75)
PLANKS=[mat('FloorOak'+str(i),[v*(.67+i*.078)for v in(.24,.13,.074)],.66,tex='wood',period=200)for i in range(8)]

def mesh(g,n,vs,fs,ma,smooth=False,uvs=None):
 d=bpy.data.meshes.new(n);d.from_pydata([(x/100,-y/100,z/100)for x,y,z in vs],[],[tuple(reversed(f)) for f in fs]);d.materials.append(ma);d.update();o=bpy.data.objects.new(n,d);bpy.context.collection.objects.link(o);G[g].append(o)
 for f in d.polygons:f.use_smooth=smooth
 if uvs:
  uv=d.uv_layers.new(name='UVMap')
  for f in d.polygons:
   for li in f.loop_indices:uv.data[li].uv=uvs[d.loops[li].vertex_index]
 return o

def box(g,n,c,sz,ma,b=0,seg=3):
 x,y,z=[v/2 for v in sz];vs=[(c[0]+a,c[1]+bb,c[2]+cc)for a,bb,cc in[(-x,-y,-z),(x,-y,-z),(x,y,-z),(-x,y,-z),(-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z)]]
 o=mesh(g,n,vs,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],ma)
 if b:
  md=o.modifiers.new('Soft manufactured edges','BEVEL');md.width=min(b,min(sz)*.45)/100;md.segments=seg;md.harden_normals=True;bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.modifier_apply(modifier=md.name);o.select_set(False)
  for p in o.data.polygons:p.use_smooth=True
  md=o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL');md.keep_sharp=True;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=md.name)
 return o

def bb(g,n,lo,hi,ma,b=0):return box(g,n,[(x+y)/2 for x,y in zip(lo,hi)],[y-x for x,y in zip(lo,hi)],ma,b)
def tube(g,n,pts,r,ma,sides=12):
 pts=[Vector(p)for p in pts];vs=[];fs=[]
 for i,p in enumerate(pts):
  d=(pts[min(i+1,len(pts)-1)]-pts[max(i-1,0)]).normalized();ref=Vector((0,0,1))if abs(d.z)<.95 else Vector((0,1,0));u=d.cross(ref).normalized();v=d.cross(u)
  for j in range(sides):a=j*math.tau/sides;vs.append(tuple(p+r*(math.cos(a)*u+math.sin(a)*v)))
 for i in range(len(pts)-1):
  for j in range(sides):fs.append((i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j))
 fs.extend([tuple(range(sides-1,-1,-1)),tuple((len(pts)-1)*sides+j for j in range(sides))]);return mesh(g,n,vs,fs,ma,True)
def cyl(g,n,c,r,h,ma,axis='Z',sides=32):
 d={'X':(h/2,0,0),'Y':(0,h/2,0),'Z':(0,0,h/2)}[axis];return tube(g,n,[tuple(c[i]-d[i]for i in range(3)),tuple(c[i]+d[i]for i in range(3))],r,ma,sides)
def sphere(g,n,c,sz,ma):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=12,radius=1,location=(c[0]/100,-c[1]/100,c[2]/100));o=bpy.context.object;o.name=n;o.scale=Vector([s/200 for s in sz]);o.data.materials.append(ma)
 for p in o.data.polygons:p.use_smooth=True
 G[g].append(o);return o

def ring(g,n,c,r,th,ma,plane='XY',count=64):
 pts=[]
 for i in range(count+1):a=i*math.tau/count;pts.append((c[0]+r*math.cos(a),c[1]+r*math.sin(a),c[2])if plane=='XY'else(c[0]+r*math.cos(a),c[1],c[2]+r*math.sin(a)))
 return tube(g,n,pts,th,ma,10)
def text(g,n,txt,c,size,ma,rot=(90,0,180)):
 cu=bpy.data.curves.new(n,'FONT');cu.body=txt;cu.size=size/100;cu.extrude=.0006;cu.align_x='CENTER';cu.align_y='CENTER';cu.resolution_u=2;o=bpy.data.objects.new(n,cu);bpy.context.collection.objects.link(o);o.location=(c[0]/100,-c[1]/100,c[2]/100);o.rotation_euler=[math.radians(v)for v in rot];o.data.materials.append(ma);bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.convert(target='MESH');o.select_set(False);G[g].append(o);return o

def save(g,kind='prop',mass=20):
 parts=G.pop(g);bpy.ops.object.select_all(action='DESELECT')
 for o in parts:
  bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);o.select_set(False)
  if not o.data.uv_layers:
   uv=o.data.uv_layers.new(name='UVMap')
   for f in o.data.polygons:
    axis=max(range(3),key=lambda i:abs(f.normal[i]));i,j=([0,1]if axis==2 else[1,2]if axis==0 else[0,2]);period=MAN['materials'][o.data.materials[f.material_index].name].get('period',100)/100
    for li in f.loop_indices:v=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(v[i]/period,v[j]/period)
 for o in parts:o.select_set(True)
 bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_NW_'+g;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');o.data.calc_loop_triangles();path=O/'models'/(o.name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False)
 MAN['assets'].append(dict(name=o.name,fbx=str(path),materials=[m.name for m in o.data.materials],kind=kind,mass=mass,triangles=len(o.data.loop_triangles)));bpy.data.objects.remove(o,do_unlink=True);print('NOODLE_ART',g,flush=True)
def place(g,label,p,yaw=0,scale=(1,1,1),kind=None,mass=None):MAN['placements'].append(dict(asset='SM_NW_'+g,label='NW_'+label,pos=list(p),yaw=yaw,scale=scale,kind=kind,mass=mass))
def light(label,p,power,radius=430,temp=3500):MAN['lights'].append(dict(label='NW_'+label,pos=p,power=power,radius=radius,temperature=temp))

# Tiling micro-surface masks. Brick mortar, paint pores, warm wood grain and scuffed concrete.
N=512;yy,xx=np.mgrid[:N,:N];u=xx/N;v=yy/N;rr=np.random.default_rng(2400)
for key in ['paint','concrete','wood','brick','metal','fabric','leather']:
 noise=rr.normal(0,1,(N,N));broad=np.sin(u*23+np.sin(v*11))*np.cos(v*17-u*5);h=noise*.05;mul=np.ones((N,N));rough=np.ones((N,N))*.7
 if key=='paint':mul=1+.026*noise+.018*broad;h=noise*.025;rough=.7+.035*noise
 elif key=='concrete':mul=1+.1*noise+.11*broad;h=noise*.065;rough=.85+.05*noise
 elif key=='wood':
  grain=np.sin(v*125+np.sin(u*6)*3)+.5*np.sin(v*561+u*11);mul=1+.06*grain+.02*noise;h=grain*.08;rough=.65+grain*.045
 elif key=='brick':
  course=np.floor(v*4);bx=(u*2+(course%2)*.5)%1;by=(v*4)%1;mortar=(bx<.018)|(by<.035);mul=np.where(mortar,.68,1+.032*noise+.02*broad);h=np.where(mortar,-.9,noise*.06);rough=.75+noise*.03
 elif key=='metal':mul=1+.09*noise;h=noise*.02;rough=.43+noise*.06
 elif key=='fabric':weave=(np.sin(u*1200)*np.cos(v*1200));mul=1+.04*weave+.023*noise;h=weave*.15;rough=.95+noise*.025
 else:mul=1+.028*noise+.045*broad;h=noise*.07+np.sin(u*215+np.sin(v*81))*np.sin(v*192)*.05;rough=.43+noise*.04
 if key in['wood','metal','concrete']:
  for j in range(80):
   x,y=rr.integers(0,N,2);ln=rr.integers(3,55);ang=rr.uniform(0,6.28)
   for t in range(ln):X=int(x+math.cos(ang)*t)%N;Y=int(y+math.sin(ang)*t)%N;mul[Y,X]*=1.15;rough[Y,X]=.83
 dx=np.roll(h,1,axis=1)-np.roll(h,-1,axis=1);dy=np.roll(h,1,axis=0)-np.roll(h,-1,axis=0);norm=np.stack([dx,dy,np.ones_like(dx)],axis=2);norm/=np.linalg.norm(norm,axis=2)[:,:,None]
 for suffix,rgb in [('Detail',np.repeat(np.clip(mul*.5,0,1)[:,:,None],3,axis=2)),('Normal',norm*.5+.5),('Roughness',np.repeat(np.clip(rough,0,1)[:,:,None],3,axis=2))]:
  px=np.concatenate([rgb,np.ones((N,N,1))],axis=2).astype('float32');im=bpy.data.images.new('T_NW_'+key+'_'+suffix,N,N,alpha=True);im.pixels.foreach_set(px.ravel());im.filepath_raw=str(O/'textures'/(im.name+'.png'));im.file_format='PNG';im.save()

# Basement rooms match the scan's T-connection and red / blue / laundry order.
F=-361.966;C=-54.;T=18
rooms={'Red':(RED,[(2310,3206,-1585,-917),(2606,3206,-917,-680)]),'Blue':(BLUE,[(2310,3206,-370,424)]),'Laundry':(CREAM,[(2606,3206,442,1195)]),'Hall':(CREAM,[(1760,2310,-669,-390),(2310,2606,-917,-370),(2310,2606,424,1214)])}
for rn,(ma,rects)in rooms.items():
 g=rn+'Floors'
 for x0,x1,y0,y1 in rects:
  bb(g,'solid floor',[x0,y0,F-16],[x1,y1,F],CONC if rn in['Hall','Laundry']else WOOD)
  bb(g,'ceiling',[x0,y0,C],[x1,y1,C+12],CEIL if rn!='Red'else RED)
  if rn in['Red','Blue']:
   for yi,y in enumerate(range(int(y0),int(y1),28)):
    xs=list(range(int(x0)-205+(yi%4)*51,int(x1)+205,205))
    for xi,x in enumerate(xs[:-1]):
     left=max(x0,x);right=min(x1,xs[xi+1])
     if right-left>1:bb(g,'individual oak plank',[left+.07,y+.06,F+.01],[right-.07,min(y+28,y1)-.06,F+.12],PLANKS[(yi*3+xi*7)%8])
 save(g,'structure');place(g,rn+'Floors',(0,0,0))

# Wall segment helper with white/black skirting and door-width omissions.
def wall(g,a,b,ma,z0=F,z1=C,trim=True):
 x0,y0=a;x1,y1=b
 if abs(x1-x0)<.1:lo=[x0-T/2,min(y0,y1),z0];hi=[x0+T/2,max(y0,y1),z1]
 else:lo=[min(x0,x1),y0-T/2,z0];hi=[max(x0,x1),y0+T/2,z1]
 bb(g,'painted partition',lo,hi,ma)
 if trim:
  bb(g,'worn baseboard',[lo[0]-1,lo[1]-1,z0],[hi[0]+1,hi[1]+1,z0+9],TRIM)

g='LaundryWalls'
for a,b in[((2606,442),(2606,490)),((2606,650),(2606,1195)),((2606,1195),(3206,1195)),((3206,442),(3206,1195))]:wall(g,a,b,CREAM)
wall(g,(2606,490),(2606,650),CREAM,F+233,C,False);bb(g,'laundry perimeter soffit',[2614,1100,C-24],[3198,1187,C],CREAM)
save(g,'structure');place(g,g,(0,0,0))
# Photo/scan T junction: opposite red and blue entrances, same room envelopes.
exec((B/'junction_layout.py').read_text(),globals())

# Direct basement entrance flight; paired upper flights. No perimeter galleries.
levels=[F,0.,333.1];g='StairStructure'
Y0=-170.;Y1=230.;LANES=[(1474.,1737.),(1771.,2019.)]
# Six entrance-facing risers meet a quarter-turn landing, then fourteen
# risers climb north to the unchanged main landing. No basement underpass.
LOW_RISE=-F/20.;LOW_MID=F+6*LOW_RISE
LOW_X0=1555.;LOW_X1=1771.;LOW_Y0=423.;LOW_Y1=628.;LOW_TURN=410.
flights=[(0.,166.55,333.1,10,10)]
bb(g,'basement concrete slab',[1384,-389,F-15],[2040,630,F],CONC)
for z in levels[1:]:
 bb(g,'full width floor landing',[1460,-658,z-12],[2038,Y0,z],TREAD)
# Thresholds meet the retained house floor without a gap.
bb(g,'main flush doorway threshold',[2034,-620,-12],[2100,-448,0],TREAD)
bb(g,'upper flush doorway threshold',[2034,-417,321.1],[2090,-241,333.1],TREAD)
def channel(a,b,w=5,h=14):
 # Rectangular folded-steel stringer, not a diamond or a round beam.
 vs=[(p[0]+dx,p[1],p[2]+dz)for p in[a,b]for dx,dz in[(-w/2,-h/2),(w/2,-h/2),(w/2,h/2),(-w/2,h/2)]]
 mesh(g,'black steel channel',vs,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],BLACK)
# The first tread faces the west basement door (door center y=539).
for i in range(6):
 x=LOW_X0+i*(LOW_X1-LOW_X0)/6;end=x+(LOW_X1-LOW_X0)/6;zt=F+(i+1)*LOW_RISE
 bb(g,'entrance facing gray tread',[x,LOW_Y0,zt-3.5],[end,LOW_Y1,zt],TREAD,.25)
 bb(g,'entrance closed steel riser',[x-.6,LOW_Y0,zt-LOW_RISE-1],[x+.6,LOW_Y1,zt-.5],BLACK,.1)
 box(g,'entrance anti slip nosing',(x+1.7,(LOW_Y0+LOW_Y1)/2,zt+.07),(3.3,LOW_Y1-LOW_Y0,.22),BLACK,.07)
 bb(g,'entrance underside pan',[x,LOW_Y0,zt-4.5],[end,LOW_Y1,zt-3.5],BLACK)
# Folded channels run along x on the entrance flight.
for y in[LOW_Y0+2,LOW_Y1-2]:
 vs=[(x,y+dy,z+dz)for x,z in[(LOW_X0,F-5),(LOW_X1,LOW_MID-5)]for dy,dz in[(-2.5,-7),(2.5,-7),(2.5,7),(-2.5,7)]]
 mesh(g,'entrance steel channel',vs,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],BLACK)
bb(g,'basement quarter turn landing',[LOW_X1,LOW_TURN,LOW_MID-12],[2019,LOW_Y1,LOW_MID],TREAD)
for i in range(14):
 going=(LOW_TURN-Y0)/14;y=LOW_TURN-(i+1)*going;zt=LOW_MID+(i+1)*LOW_RISE
 bb(g,'basement return gray tread',[1771,y,zt-3.5],[2019,y+going,zt],TREAD,.25)
 bb(g,'basement return closed riser',[1771,y+going-.6,zt-LOW_RISE-1],[2019,y+going+.6,zt-.5],BLACK,.1)
 box(g,'basement return nosing',(1895,y+going-1.7,zt+.07),(248,3.3,.22),BLACK,.07)
 bb(g,'basement return underside pan',[1771,y,zt-4.5],[2019,y+going,zt-3.5],BLACK)
for x in[1773,2017]:channel((x,LOW_TURN,LOW_MID-5),(x,Y0,-5))
# The upper pair stays in its existing position.
for z,mid,top,n0,n1 in flights:
 for lane,(x0,x1) in enumerate(LANES):
  n=n0 if lane==0 else n1;base=z if lane==0 else mid;rise=((mid-z)if lane==0 else(top-mid))/n;going=(Y1-Y0)/n
  for i in range(n):
   yy=Y0+i*going if lane==0 else Y1-(i+1)*going;zt=base+(i+1)*rise
   bb(g,'gray aggregate tread',[x0,yy,zt-3.5],[x1,yy+going,zt],TREAD,.25)
   lead=yy if lane==0 else yy+going
   bb(g,'closed steel riser',[x0,lead-.6,zt-rise-1],[x1,lead+.6,zt-.5],BLACK,.1)
   box(g,'steel anti slip nosing',((x0+x1)/2,lead+(1.7 if lane==0 else -1.7),zt+.07),(x1-x0,3.3,.22),BLACK,.07)
   bb(g,'underside steel pan',[x0,yy,zt-4.5],[x1,yy+going,zt-3.5],BLACK)
  for x in[x0+2,x1-2]:
   a=(x,Y0,z-5)if lane==0 else(x,Y1,mid-5);b=(x,Y1,mid-5)if lane==0 else(x,Y0,top-5);channel(a,b)
 bb(g,'solid half turn landing',[1460,Y1,mid-12],[2038,434,mid],TREAD)
 box(g,'half landing steel fascia',(1749,Y1,mid-8),(578,3,16),BLACK,.25)
 # Landing edge support, fixed to masonry at each side.
 for x in[1470,2024]:box(g,'landing angle support',(x,335,mid-13),(6,198,8),BLACK,.4)
# Flush yellow masonry enclosure. The basement entrance faces the short
# quarter-turn flight; upper levels end at the compact paired-stair turn.
wall(g,(1467,-389),(1467,474),BRICK,F,-50,False)
wall(g,(1467,-666),(1467,474),BRICK,-50,634,False)
wall(g,(1467,605),(1467,630),BRICK,F,634,False)
wall(g,(1467,474),(1467,605),BRICK,F+236,634,False)
wall(g,(1467,630),(2040,630),BRICK,F,634,False)
wall(g,(1467,-389),(2028,-389),BRICK,F,-62,False)
wall(g,(1460,-666),(2040,-666),BRICK,-50,634,False)
wall(g,(1467,443),(2040,443),BRICK,166.55,634,False)
for z0,z1,d0,d1,dh in[(F,0,999,1000,0),(0,333.1,-620,-448,239),(333.1,634,-417,-241,237)]:
 if d0>900:wall(g,(2028,-389),(2028,630),BRICK,z0,z1,False)
 else:
  wall(g,(2028,-666),(2028,d0),BRICK,z0,z1,False);wall(g,(2028,d1),(2028,630),BRICK,z0,z1,False);wall(g,(2028,d0),(2028,d1),BRICK,z0+dh,z1,False)
bb(g,'shaft ceiling',[1450,-666,633],[2042,633,640],CEIL)
save(g,'structure');MAN['assets'][-1]['material_overrides']={'M_NW_AggregateTreads':'/Game/SeniorSendOff/House/Materials/MI_SSO_S03StairTreadSteel','M_NW_WornConcrete':'/Game/SeniorSendOff/House/Materials/MI_SSO_S03WornConcrete'};place(g,g,(0,0,0))

g='StairRails'
def rail(a,b,z0,z1):
 A=Vector((a[0],a[1],z0));Z=Vector((b[0],b[1],z1));count=math.ceil((Z-A).length/18)
 for i in range(count+1):
  p=A+(Z-A)*(i/count)
  box(g,'square steel baluster',(p.x,p.y,p.z+52),(2.1,2.1,102),BLACK,.15)
 tube(g,'smooth round handrail',[tuple(A+Vector((0,0,104))),tuple(Z+Vector((0,0,104)))],2.5,BLACK,20)
 tube(g,'lower guard rail',[tuple(A+Vector((0,0,12))),tuple(Z+Vector((0,0,12)))],1.4,BLACK,12)
 for p in[A,Z]:
  box(g,'newel square post',(p.x,p.y,p.z+52),(4,4,104),BLACK,.35)
  box(g,'bolted post foot',(p.x,p.y,p.z+1),(10,10,2),BLACK,.3)
# Inner guards follow the new direct route, never crossing the doorway.
rail((LOW_X0,428),(LOW_X1,428),F,LOW_MID)
rail((1776,LOW_TURN),(1776,Y0),LOW_MID,0.)
rail((LOW_X1,428),(1776,LOW_TURN),LOW_MID,LOW_MID)
# South-wall rail follows the entrance flight and sweeps around the turn.
tube(g,'entrance and turn continuous wall rail',[(LOW_X0-20,621,F+96),(LOW_X0-20,596,F+96),(LOW_X0,596,F+96),(LOW_X1,596,LOW_MID+96),(1985,596,LOW_MID+96),(1991,590,LOW_MID+96),(1991,LOW_TURN,LOW_MID+96),(1991,Y0,96),(1991,Y0-20,96),(2019,Y0-20,96)],2.3,BLACK,20)
for t in[.12,.6,.92]:
 x=LOW_X0+(LOW_X1-LOW_X0)*t;h=F+(LOW_MID-F)*t+96
 tube(g,'entrance wall rail bracket',[(x,621,h-10),(x,596,h-10),(x,596,h)],1.1,BLACK,10)
 cyl(g,'entrance wall bracket rosette',(x,621,h-10),3.4,1.4,BLACK,'Y',16)
for t in[.05,.32,.66,.94]:
 y=LOW_TURN+(Y0-LOW_TURN)*t;h=LOW_MID+(0-LOW_MID)*t+96
 tube(g,'lower return wall bracket',[(2019,y,h-10),(1991,y,h-10),(1991,y,h-1)],1.1,BLACK,10)
 cyl(g,'lower return bracket rosette',(2019,y,h-10),3.4,1.4,BLACK,'X',16)
for z,mid,top,n0,n1 in flights:
 rail((1732,Y0),(1732,Y1),z,mid);rail((1776,Y1),(1776,Y0),mid,top)
 # Round wall rails: curved ends turn back into the wall; standoffs really touch it.
 for xx,wallx,base,end in[(1504,1476,z,mid),(1991,2019,top,mid)]:
  pts=[(wallx,Y0-23,base+96),(xx-3 if xx<1700 else xx+3,Y0-23,base+96),(xx,Y0-17,base+96),(xx,Y0,base+96),(xx,Y1,end+96),(xx,Y1+17,end+96),(xx-3 if xx<1700 else xx+3,Y1+23,end+96),(wallx,Y1+23,end+96)]
  tube(g,'wall rail with curved returns',pts,2.3,BLACK,20)
  for t in[.06,.35,.66,.94]:
   y=Y0+(Y1-Y0)*t;h=base+(end-base)*t+96
   tube(g,'wall handrail bracket',[(wallx,y,h-10),(xx,y,h-10),(xx,y,h-1)],1.1,BLACK,10)
   cyl(g,'wall bracket rosette',(wallx,y,h-10),3.4,1.4,BLACK,'X',16)
 # Close only the small central well at its turning end.
 rail((1732,Y1),(1776,Y1),mid,mid)
for z in levels[1:]:rail((1732,Y0),(1776,Y0),z,z)
# At the final floor the west side has no up-flight, so guard the full open edge.
rail((1468,Y0),(1732,Y0),levels[2],levels[2])
save(g,'structure');place(g,g,(0,0,0))

g='StairHardware'
# Wall fixtures above the actual turning landings and basement approach.
for label,y,z,power in [('Lower',619,F+265,720),('Turn',431,LOW_MID+237,940),('Upper',431,166.55+235,940)]:
 box(g,'fluorescent enamel tray',(1670,y,z),(126,7,12),WHITE,1)
 for dz in[-2.1,2.1]:cyl(g,'opal fluorescent tube',(1670,y-5,z+dz),1.45,118,LIGHT,'X')
 for x in[1611,1729]:box(g,'fixture end cap',(x,y-5,z),(3,5,10),CHROME,.5)
 light('StairLamp_'+label,[1625,539,F+218]if label=='Lower'else[1710,y-70,z],power,440 if label=='Lower'else 415,4100)
# Keep the existing light budget; move the former underpass fixture to the
# return flight, so it lights the walking surface instead of unused floor.
box(g,'return flight fluorescent tray',(2012,110,100),(7,116,11),WHITE,.8)
for dz in[-2,2]:cyl(g,'return flight opal tube',(2007,110,100+dz),1.4,107,LIGHT,'Y')
light('StairLamp_BasementFoot',[1880,100,65],680,350,4100)
# Dark skirting on level landings, white ceiling hatch and surface conduit.
for z in levels[1:]:
 box(g,'black landing skirting',(1477,-410,z+5),(1.2,485,10),BLACK)
 box(g,'end landing skirting',(1748,-656,z+5),(542,1.2,10),BLACK)
box(g,'ceiling access hatch',(1610,-352,630),(95,82,3),BLACK,1)
box(g,'hatch painted panel',(1610,-352,628),(90,77,2),CREAM,1)
for x in[1568,1652]:cyl(g,'hatch screw',(x,-381,626.5),.65,.6,CHROME)
for z in[0,333.1]:
 tube(g,'exposed electrical conduit',[(1470,-641,z+280),(1965,-641,z+280),(2017,-615,z+265)],.8,TRIM,12)
 for x in[1550,1780,1940]:box(g,'conduit saddle',(x,-641,z+280),(2,3,5),CHROME,.4)
for y in[-620,-448]:box(g,'main doorway casing',(2058,y,120),(96,8,240),TRIM,1)
box(g,'main opening lintel',(2058,-534,243),(96,183,9),TRIM,1)
# Deep lined returns meet the original upstairs casing at its retained hinge.
for y in[-417,-241]:box(g,'upper doorway lined return',(2047,y,453),(58,7,240),TRIM,.6)
box(g,'upper doorway lined header',(2047,-329,574),(58,183,8),TRIM,.6)
save(g,'detail');place(g,g,(0,0,0))

# Local steel leaf. Existing door actor provides shared E interaction and replicated swing.
g='FireDoor';box(g,'painted steel leaf',(82.5,0,118),(165,5,236),CREAM,1.25)
for side in[-1,1]:
 box(g,'satin kickplate',(82.5,side*2.75,17),(156,.35,28),CHROME,.3)
 cyl(g,'lever rosette',(148,side*3.0,108),3,1,CHROME,'Y')
 tube(g,'return lever',[(148,side*3,108),(148,side*7,108),(133,side*7,108),(131,side*4,108)],1.1,CHROME,16)
 box(g,'door closer',(122,side*5,220),(30,6,6),CHROME,1)
 tube(g,'closer arm',[(122,side*6,223),(99,side*15,231),(86,side*2,233)],.65,BLACK,12)
 for z in[30,116,209]:cyl(g,'hinge knuckle',(.2,side*2.4,z),1.2,12,CHROME)
save(g,'door');MAN['doors'].append(dict(label='NW_MainStairDoor',pos=[2083,-450,0],yaw=-90,width=165,height=236,center=[2083,-532.5,118],asset='SM_NW_FireDoor',swing=90))
# Another properly hinged door closes laundry opening.
MAN['doors'].append(dict(label='NW_LaundryDoor',pos=[2606,651,F],yaw=-90,width=150,height=230,center=[2606,576,F+115],asset='SM_NW_FireDoor',scale=[150/165,1,230/236],swing=-90))

# Pool table: recessed felt bed, cushion rails, six open dark pockets, brass diamonds, leather baskets.
g='BilliardTable';box(g,'solid walnut apron',(0,0,74),(292,158,27),WOOD,5)
for x in[-107,107]:
 for y in[-50,50]:box(g,'tapered pedestal',(x,y,38),(28,31,76),WOOD,3);box(g,'plinth foot',(x,y,5),(37,41,10),WOOD,2)
box(g,'dark pocket bed',(0,0,90),(308,175,12),BLACK,4)
box(g,'green slate playing surface',(0,0,95),(263,132,5),FELT,2)
# Rails split around pocket openings.
for y in[-77,77]:
 for x in[-77,77]:box(g,'polished side rail',(x,y,98),(126,21,8),WOOD,4);box(g,'felt cushion',(x,y*.84,100),(125,7,8),FELT,2)
for x in[-143,143]:box(g,'short cushion rail',(x,0,98),(20,129,8),WOOD,4);box(g,'short felt cushion',(x*.92,0,100),(7,116,8),FELT,2)
for x,y in[(-137,-71),(0,-75),(137,-71),(-137,71),(0,75),(137,71)]:
 cyl(g,'black pocket throat',(x,y,94),8.7,13,RUBBER);ring(g,'leather pocket binding',(x,y,99),8.8,1.5,LEATHER)
 for i in range(8):a=i*math.tau/8;tube(g,'hanging pocket leather',[(x+7*math.cos(a),y+7*math.sin(a),95),(x+5*math.cos(a),y+5*math.sin(a),72)],.6,LEATHER,8)
for y in[-78,78]:
 for x in[-102,-51,51,102]:box(g,'sight diamond',(x,y,102.2),(1.6,3,.15),GOLD,.4)
save(g,'prop',260);place(g,'Red_BilliardTable',[2765,-1230,F],90,(1.2,1.2,1))
# Pool balls and cues remain separate loose objects.
for g,ma in [('CueBall',POOLWHITE),('YellowBall',POOLYELLOW)]:sphere(g,'polished billiard ball',(0,0,3),(6,6,6),ma);save(g,'prop',.18)
place('CueBall','Red_CueBall',[2740,-1330,F+100]);place('YellowBall','Red_YellowBall',[2790,-1240,F+100])
g='PoolCue';tube(g,'tapered maple cue',[(0,0,0),(0,0,145)],.75,WOOD,16);cyl(g,'cue butt grip',(0,0,23),1,40,BLACK);cyl(g,'chalked tip',(0,0,145),.75,1,GREEN);save(g,'prop',.6)
for i in range(5):place('PoolCue','Red_WallCue_'+str(i),[2950+i*19,-1563,F+44])
g='CueRack';box(g,'cue rack base',(0,0,45),(145,13,9),WOOD,3);box(g,'arched cue rack top',(0,0,211),(145,13,7),WOOD,3)
for x in[-68,68]:box(g,'rack side stile',(x,0,128),(7,9,169),WOOD,2)
for x in[-52,-26,0,26,52]:ring(g,'cue retaining ring',(x,-7,211),2.4,.65,GOLD)
save(g,'static');place(g,'Red_CueRack',[2990,-1570,F])
# Fabric recliner with padded arms, segmented back and pulled-out footrest.
g='TanRecliner';box(g,'recliner base',(0,0,24),(91,89,43),TAN,11,5);box(g,'seat cushion',(0,-7,48),(68,68,17),TAN,10,5);box(g,'padded back',(0,34,83),(75,24,67),TAN,12,5)
for x in[-43,43]:box(g,'rounded arm',(x,-1,63),(23,85,34),TAN,10,5)
box(g,'back pillow',(0,20,99),(67,13,29),TAN,8,5);box(g,'front footrest',(0,-57,28),(74,30,15),TAN,6,5)
for x in[-31,31]:box(g,'recliner lever frame',(x,-27,10),(4,55,4),BLACK,1)
save(g,'prop',65)
place('TanRecliner','Red_ReclinerA',[3124,-939,F],-90,(1.1,1.1,1.05));place('TanRecliner','Red_ReclinerB',[3124,-780,F],-90,(1.1,1.1,1.05))
# Brown leather couch modules, gently bulged cushions and visible piping.
g='LeatherSofa'
box(g,'leather lower base',(0,0,22),(265,93,32),LEATHER,9,5)
for x in[-84,0,84]:
 box(g,'separate relaxed seat',(x,-8,46),(80,76,20),LEATHER,9,6);box(g,'plump recliner back',(x,34,80),(82,29,67),LEATHER,12,6)
 tube(g,'seat welt seam',[(x-34,-42,48),(x-37,-38,51),(x-37,24,51),(x+37,24,51),(x+37,-38,51),(x+34,-42,48)],.32,GOLD,8)
for x in[-131,131]:box(g,'soft leather arm',(x,-2,58),(20,94,37),LEATHER,9,6)
for x in[-113,113]:
 for y in[-32,32]:box(g,'sofa foot',(x,y,5),(9,9,10),BLACK,2)
save(g,'prop',110)
place('LeatherSofa','Blue_SectionalLong',[2655,74,F],90,(1.75,1,1));place('LeatherSofa','Blue_SectionalReturn',[2870,-201,F],180,(1.27,1,1))
# Simple brown couch on red wall, reused geometry and a smaller loose cushion.
place('LeatherSofa','Red_Sofa',[2390,-1160,F],90,(.85,.9,.9))
g='Pillow';box(g,'squashed throw pillow',(0,0,10),(57,48,18),TAN,8,6);save(g,'prop',1.1)
place('Pillow','Blue_PillowA',[2647,-60,F+58],25);place('Pillow','Blue_PillowB',[2870,-245,F+58],-18)
# Live-edge counters and console tables with thin steel legs.
g='Counter';box(g,'live edge oak top',(0,0,104),(322,50,7),WOOD,3,5)
for x in[-144,144]:
 for y in[-15,15]:box(g,'slender powder coated leg',(x,y,50),(5,5,100),BLACK,1)
 box(g,'lower brace',(x,0,18),(5,39,5),BLACK,1)
for j in range(24):sphere(g,'natural edge swell',(-154+j*13,-24,104),(14,3,5),WOOD)
save(g,'prop',70);place('Counter','Blue_Bar',[2339,36,F],90,(1.5,1,1));place('Counter','Red_Console',[2868,-702,F],0,(.95,.7,.82));place('Counter','Red_HighTable',[2456,-1465,F],0,(.65,1,1))
# Detailed brown seat stools copied as individual interactable pieces.
g='LeatherStool'
for x in[-24,24]:
 for y in[-23,23]:tube(g,'tubular stool leg',[(x,y,0),(x*.85,y*.85,75)],1.6,BLACK,12)
ring(g,'stool foot ring',(0,0,28),27,1.4,BLACK)
box(g,'brown stool seat',(0,0,78),(58,53,10),LEATHER,7,5)
for x in[-24,24]:tube(g,'back uprights',[(x,20,72),(x,27,113)],1.6,BLACK,12)
box(g,'curved brown backrest',(0,28,105),(58,9,31),LEATHER,6,5)
save(g,'prop',12)
for i,y in enumerate([-180,-65,55,175]):place('LeatherStool','Blue_BarStool'+str(i),[2435,y,F],-90)
place('LeatherStool','Red_HighStoolA',[2415,-1368,F]);place('LeatherStool','Red_HighStoolB',[2525,-1400,F],15)
# Low media console and television. Mesh face fronts -Y.
g='TVConsole';box(g,'oak console case',(0,0,26),(237,42,48),WOOD,2)
for x in[-84,84]:box(g,'sliding side door',(x,-22,29),(64,2,40),WOOD,.8)
bb(g,'open black component bay',[-46,-23,10],[46,-21,46],RUBBER)
for z in[16,28]:box(g,'media receiver',(0,-23.5,z),(65,4,9),BLACK,1)
for x in[-12,0,12]:cyl(g,'receiver control',(x,-26,28),1,1,CHROME,'Y')
for x in[-100,100]:box(g,'console foot',(x,0,2),(6,28,5),BLACK,1)
save(g,'prop',55);place(g,'Blue_MediaConsole',[2850,387,F])
g='TV';box(g,'wall mounting bracket',(0,8,0),(60,5,31),BLACK,2);box(g,'television rear',(0,2,0),(174,7,100),RUBBER,3,5);box(g,'slim display bezel',(0,-2,0),(182,3,105),BLACK,1.6,5);box(g,'black reflective screen',(0,-3.6,0),(175,.3,98),GLASS,.2)
cyl(g,'standby indicator',(80,-4,-50),.35,.2,LED,'Y',12);save(g,'wall',22)
place('TV','Blue_MainTV',[2840,418,F+187]);place('TV','Blue_SecondTV',[3192,96,F+180],-90,(.8,.8,.8));place('TV','Red_TV',[2319,-1478,F+198],90,(.72,.72,.72))
# Arcade cabinet: shaped sides, recessed display, speakers, controls, coin door.
g='Arcade'
outline=[(-38,0),(40,0),(40,192),(17,204),(-30,204),(-32,176),(-20,145),(-4,126),(-44,113)]
for x in[-37,37]:
 vs=[(x-2,y,z)for y,z in outline]+[(x+2,y,z)for y,z in outline];n=len(outline);fs=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n)for i in range(n)];mesh(g,'arcade silhouette',vs,fs,BLACK)
box(g,'arcade front',(0,-37,49),(72,5,98),BLACK,2);box(g,'recessed monitor',(0,-14,154),(62,7,54),GLASS,2);box(g,'control deck',(0,-27,119),(72,44,7),WOOD,2);box(g,'glowing marquee',(0,-30,190),(69,3,23),BLUE,1)
text(g,'arcade marquee','ARCADE',(0,-32,190),10,LIGHT)
for x in[-22,12]:
 cyl(g,'joystick shaft',(x,-32,125),.8,9,CHROME);sphere(g,'joystick ball',(x,-32,131),(7,7,7),ORANGE)
 for a in[0,1,2]:cyl(g,'arcade button',(x+7+a*5,-23+(a%2)*6,124),2.2,1.2,POOLYELLOW if a%2 else ORANGE)
box(g,'coin service panel',(0,-40,50),(36,2,49),CHROME,2)
for x in[-8,8]:box(g,'coin return',(x,-42,56),(8,2,12),RUBBER,.8)
# Pixel maze rendered as actual tiny colored tiles within the display.
for x in range(-27,28,6):
 for z in range(134,178,6):
  if (x+z)%4 or x in[-27,27]:box(g,'screen maze segment',(x,-18.1,z),(4,.1,3),LED,.1)
for x,y,z in[(-15,-18.2,153),(9,-18.2,165),(21,-18.2,140)]:sphere(g,'screen game sprite',(x,y,z),(5,.1,5),POOLYELLOW)
save(g,'prop',130);place(g,'Blue_Arcade',[3110,330,F],-12)

# Laundry machines and round doors, modeled as independent complete appliances.
def machine(g,stack=False):
 for k in range(2 if stack else 1):
  z=k*106;box(g,'enamel cabinet',(0,0,z+52),(78,80,104),WHITE,2.4,5)
  box(g,'front control fascia',(0,-41,z+93),(75,3,15),WHITE,1)
  cyl(g,'program dial',(19,-43.5,z+94),5,3,CHROME,'Y')
  box(g,'digital readout',(-10,-43,z+94),(19,.5,6),GLASS,.4)
  text(g,'digital minutes','0:45',(-10,-43.5,z+94),3,LED)
  for xx in[-21,-14,-7,0]:cyl(g,'cycle button',(xx,-43.5,z+84),.75,1,CHROME,'Y',12)
  cyl(g,'black door gasket',(0,-42,z+51),28,3,RUBBER,'Y',48)
  ring(g,'round polished porthole',(0,-44,z+51),25,2.5,CHROME,'XZ')
  cyl(g,'recessed drum',(0,-43.9,z+51),22,.4,GLASS,'Y',48)
  ring(g,'inner drum',(0,-44.2,z+51),16,1,CHROME,'XZ')
  box(g,'door grip',(22,-48,z+51),(5,4,15),WHITE,2)
  for j in range(12):a=j*math.tau/12;sphere(g,'drum perforation',(12*math.cos(a),-44.4,z+51+12*math.sin(a)),(1,.1,1),BLACK)
  box(g,'lower maintenance flap',(-24,-40.8,z+12),(18,1,13),WHITE,1)
  for xx in[-27,27]:cyl(g,'adjustable appliance foot',(xx,24,z+1),4,3,RUBBER)
  text(g,'machine brand','COMMERCIAL',(-13,-43.2,z+102),2.6,BLACK)
 for y in[-19,0,19]:box(g,'top vent seam',(0,y,(210 if stack else 104)+.1),(63,.15,.1),BLACK)
g='StackedLaundry';machine(g,True);save(g,'prop',190)
place(g,'Laundry_Stacked_A',[2730,1140,F]);place(g,'Laundry_Stacked_B',[2840,1140,F])
g='FrontWasher';machine(g);save(g,'prop',84);place(g,'Laundry_FrontWasher',[2950,1140,F])
g='TopWasher';box(g,'washer enamel body',(0,0,48),(79,78,96),WHITE,2,5);box(g,'rear instrument panel',(0,34,103),(79,11,22),WHITE,2)
for xx in[-24,0,24]:cyl(g,'wash dial',(xx,27,105),4.4,2,CHROME,'Y')
cyl(g,'open top gasket',(0,-2,96.5),27,2,RUBBER);cyl(g,'wash basket',(0,-2,97),24,1,CHROME);ring(g,'drum rim',(0,-2,98),24,1.1,CHROME)
box(g,'raised washer lid',(0,33,135),(75,3,71),WHITE,2);box(g,'lid inner rim',(0,31,136),(65,1,62),CREAM,2)
save(g,'prop',90);place(g,'Laundry_TopLoad',[3060,1140,F])
g='OpenPorthole';ring(g,'swinging washer rim',(0,0,0),26,3,WHITE,'XZ');ring(g,'door chrome',(0,0,0),22,1.5,CHROME,'XZ');cyl(g,'door tinted glass',(0,0,0),21,.8,GLASS,'Y',48);save(g,'detail');place(g,'Laundry_OpenDoor',[2810,1076,F+51],58)
# Utility sink with open basin, faucet and wall cupboards.
g='UtilitySink'
for x in[-45,45]:
 for y in[-24,24]:box(g,'sink leg',(x,y,43),(7,7,86),CHROME,1)
box(g,'basin bottom',(0,0,63),(110,67,5),WHITE,3)
for x in[-53,53]:box(g,'basin side',(x,0,78),(6,67,29),WHITE,2)
for y in[-31,31]:box(g,'basin wall',(0,y,78),(110,5,29),WHITE,2)
ring(g,'drain',(0,0,66),4,1,CHROME);tube(g,'curved faucet',[(0,30,92),(0,30,118),(0,12,124),(0,3,118)],2,CHROME,20)
for x in[-15,15]:cyl(g,'tap valve',(x,30,95),3.4,6,CHROME);tube(g,'cross tap',[(x-6,30,99),(x+6,30,99)],1,CHROME)
tube(g,'sink P trap',[(0,0,62),(0,0,40),(0,12,33),(0,29,45)],3.5,CHROME)
save(g,'prop',46);place(g,'Laundry_UtilitySink',[3155,846,F],-90)
g='WallCabinet';box(g,'cabinet case',(0,0,0),(115,37,75),WHITE,1.4)
for x in[-29,29]:
 box(g,'shaker cupboard door',(x,-20,0),(55,3,72),WHITE,.8);box(g,'recessed door field',(x,-21.8,0),(42,.4,57),CREAM,.5);tube(g,'cupboard pull',[(x+18,-24,-5),(x+18,-24,5)],1.1,CHROME)
save(g,'wall',35);place(g,'Laundry_WallCabinets',[3175,846,F+230],-90)
# Detergent bottles with a real open grip.
g='Detergent';box(g,'orange jug',(0,0,12),(15,11,24),ORANGE,4,5);cyl(g,'pouring neck',(-3,0,26),3.8,5,ORANGE);cyl(g,'measuring cap',(-3,0,29),4.5,4,BLUE)
tube(g,'jug open handle',[(6,0,23),(12,0,24),(13,0,12),(7,0,10)],2,ORANGE,16);box(g,'paper label',(0,-5.6,12),(10,.2,13),PAPER,1);text(g,'detergent label','WASH',(0,-5.9,13),2.4,BLUE);save(g,'prop',2.2)
for i,p in enumerate([(2710,1130,F+211),(2750,1151,F+211),(2940,1157,F+104),(3153,870,F+93)]):place(g,'Laundry_Detergent'+str(i),p,(-12+i*15))
g='LaundryBasket';
for x in[-27,27]:
 for y in[-19,19]:tube(g,'basket upright',[(x,y,2),(x*1.15,y*1.2,53)],1.4,WHITE,10)
for z in[2,12,22,32,42,53]:
 sc=1+z/350;tube(g,'basket open lattice',[(-27*sc,-19*sc,z),(27*sc,-19*sc,z),(27*sc,19*sc,z),(-27*sc,19*sc,z),(-27*sc,-19*sc,z)],1.4,WHITE,10)
for x in range(-25,27,8):
 for y in[-19,19]:tube(g,'basket vertical lattice',[(x,y,2),(x*1.15,y*1.2,53)],.9,WHITE,8)
box(g,'basket base',(0,0,2),(54,38,3),WHITE,2);save(g,'prop',1.5)
place(g,'Laundry_Hamper',[2690,886,F],14);place(g,'Laundry_Hamper2',[2713,760,F],-8)
g='LaundryPile';
for j in range(4):
 vs=[];fs=[]
 for iy in range(11):
  for ix in range(11):x=(ix-5)*5;y=(iy-5)*4;z=3+j*1.1+6*math.exp(-(x*x+y*y)/450)+1.5*math.sin(x*.39+y*.27+j);vs.append((x+j*2,y+j*2,z))
 for iy in range(10):
  for ix in range(10):a=iy*11+ix;fs.append((a,a+1,a+12,a+11))
 mesh(g,'crumpled garment',vs,fs,CLOTHWHITE if j%2 else CLOTH,True)
save(g,'prop',1)
for i,p in enumerate([(2670,892,F+19),(2705,758,F+19),(2980,1020,F),(3080,1050,F),(2880,928,F),(2600,806,F)]):place(g,'Laundry_Garments'+str(i),p,i*59)
g='DryerSheet';mesh(g,'wrinkled dryer sheet',[(-7,-8,0),(7,-8,.4),(7,8,.3),(-7,8,0),(0,0,1.4)],[(0,1,4),(1,2,4),(2,3,4),(3,0,4)],CLOTHWHITE);save(g,'prop',.02)
for i in range(17):place(g,'Laundry_Sheet'+str(i),[rng.uniform(2650,3100),rng.uniform(730,1080),F+.15],rng.uniform(0,360))
g='UtilityDetail'
for x in[2730,2840,2950,3060]:tube(g,'washer hose',[(x,1178,F+20),(x+18,1180,F+115),(x+26,1182,F+116)],1.8,BLACK)
tube(g,'dryer metal vent',[(2780,1180,F+205),(2780,1180,F+266),(3179,1180,F+266)],8,CHROME,24)
for x in range(2800,3160,30):cyl(g,'vent band',(x,1180,F+266),8.4,1,BLACK,'X')
save(g,'detail');place(g,g,(0,0,0))
# Beige hall clutter, boards and furniture remain movable individual items.
g='Box';box(g,'corrugated box base',(0,0,1),(53,42,2),PAPER)
for x in[-26,26]:box(g,'open box side',(x,0,20),(1,42,40),PAPER)
for y in[-20,20]:box(g,'open box side',(0,y,20),(53,1,40),PAPER)
for y in[-32,32]:box(g,'folded box flap',(0,y,40),(53,23,1),PAPER)
save(g,'prop',1.5);place(g,'Hall_OpenBox',[2550,1150,F],17)
g='SpindleChair';box(g,'green shaped seat',(0,0,47),(53,48,6),GREEN,7,5)
for x in[-21,21]:
 for y in[-18,18]:tube(g,'turned chair leg',[(x*1.12,y*1.12,0),(x,y,46)],2,BLACK,16)
 tube(g,'high chair back',[(x,19,47),(x,23,99)],2.1,BLACK,16)
for x in[-13,0,13]:tube(g,'chair spindle',[(x,19,51),(x,23,97)],1.4,BLACK,12)
tube(g,'arched crest',[(-21,23,95),(-14,24,102),(14,24,102),(21,23,95)],2.5,BLACK,16)
save(g,'prop',9)
for i,y in enumerate([724,814,908]):place(g,'Hall_StoredChair'+str(i),[2360,y,F],90)
place(g,'Red_GreenChair',[2392,-1013,F],-90)
g='SideTable';box(g,'side table top',(0,0,58),(56,56,6),WOOD,2)
for x in[-22,22]:
 for y in[-22,22]:box(g,'wood table leg',(x,y,27),(5,5,54),WOOD,1)
save(g,'prop',11);place(g,'Red_SideTable',[3101,-1084,F])
g='TableLamp';cyl(g,'lamp base',(0,0,2),12,4,GOLD);cyl(g,'lamp stem',(0,0,26),2.3,47,GOLD)
vs=[];fs=[]
for z,r in[(37,24),(67,13)]:
 for i in range(40):a=i*math.tau/40;vs.append((r*math.cos(a),r*math.sin(a),z))
for i in range(40):fs.append((i,(i+1)%40,(i+1)%40+40,i+40))
mesh(g,'pleated linen shade',vs,fs,CLOTHWHITE,True);ring(g,'shade hem',(0,0,37),24,.5,GOLD);save(g,'prop',3);place(g,'Red_TableLamp',[3101,-1084,F+61]);light('Red_Lamp',[3101,-1084,F+113],170,230,2700)
# Rugs have bound edges and visible narrow weave ribs, kept flat under seats.
for g,w,d in[('BlueRug',370,335),('RedRug',290,170)]:
 box(g,'bound rug',(0,0,.7),(w,d,1.4),RUG,.6)
 for x in range(-int(w/2)+5,int(w/2)-5,4):box(g,'woven rib',(x,0,1.42),(.45,d-9,.06),CLOTHWHITE)
 save(g,'detail')
place('BlueRug','Blue_AreaRug',[2840,46,F+.1]);place('RedRug','Red_AreaRug',[2765,-1230,F+.2],90,(1.3,1.25,1))
# Room photos serve as wall-art textures via UV windows, keeping their original files intact.
for g,photo,uv,w,h in [('RedDogs','IMG_0193',(0.06,.10,.94,.90),112,82),('RedWildlife','IMG_0194',(.08,.08,.92,.92),118,74),('RedSport','IMG_0195',(.06,.10,.94,.90),80,106)]:
 ma=mat(g+'Print',(.5,.5,.5),.9);MAN['materials'][ma.name]['photo']=str(B/'references/noodle'/(photo+'.jpg'))
 box(g,'dark wooden picture frame',(0,0,0),(w+8,4,h+8),WOOD,1);u0,v0,u1,v1=uv
 mesh(g,'framed photograph',[(-w/2,-2.1,-h/2),(w/2,-2.1,-h/2),(w/2,-2.1,h/2),(-w/2,-2.1,h/2)],[(0,1,2,3)],ma,False,[(u1,v0),(u0,v0),(u0,v1),(u1,v1)])
 save(g,'wall',5)
place('RedDogs','Red_DogsPicture',[3189,-1220,F+195],-90);place('RedWildlife','Red_WildlifePicture',[2815,-1570,F+196],180);place('RedSport','Red_SportsPicture',[2700,-696,F+202])
g='HallFlag'
for i in range(13):box(g,'flag stripe',(0,-.03*i,40-i*6),(151,.18,6.04),PAPER if i%2 else RED)
box(g,'blue canton',(-46,-.6,21),(60,.2,42),BLUE)
for iy in range(5):
 for ix in range(6):sphere(g,'embroidered star',(-72+ix*10,-.8,4+iy*8),(2,.1,2),PAPER)
for x in[-74,74]:cyl(g,'flag grommet',(x,-1,40),1,1,GOLD,'Y')
save(g,'wall',1);place(g,'Hall_USFlag',[2320,1072,F+190],90)
# Shut mechanical closet beside pool area, shallow casing fits existing footprint.
g='MechanicalCloset';bb(g,'mechanical door closing side opening',[-3,-91,0],[3,91,228],CREAM,1)
for z in[55,153]:
 for zz in range(z-32,z+33,6):box(g,'louvered door slat',(4,0,zz),(2,152,2),TRIM,.3)
sphere(g,'brass closet knob',(7,71,99),(4,7,7),GOLD);save(g,'structure');place(g,'Red_MechanicalCloset',[2310,-1350,F])
# Recessed lights and simple hall utilities. Moderate localized lights, few shadow casters.
g='CeilingFixtures'
for rn,ps,temp,power in [('Red',[(2520,-1455),(2920,-1420),(2910,-916)],3900,850),('Blue',[(2465,-130),(2880,-110),(2880,267)],4200,900),('Laundry',[(2800,715),(2990,1040)],4500,1200),('Hall',[(2000,-525),(2455,-570),(2448,1030)],4000,800)]:
 for i,(x,y)in enumerate(ps):
  cyl(g,'recessed trim',(x,y,C-1),13,2,WHITE);cyl(g,'recessed diffuser',(x,y,C-2),10,1,LIGHT);light(rn+'_Ceiling'+str(i),[x,y,C-19],power,400 if rn!='Hall' else 330,temp)
# Simple wall outlets and switches in the new rooms only.
for x,y,yaw in[(3195,-1370,0),(3195,-880,0),(3195,40,0),(2617,696,0),(2321,933,0)]:
 box(g,'duplex outlet plate',(x,y,F+42),(1,8,13),WHITE,.5)
 for z in[F+39,F+45]:
  for yy in[y-1.3,y+1.3]:box(g,'recessed socket',(x-.6,yy,z),(1,.6,2),BLACK)
save(g,'detail');place(g,g,(0,0,0))
# Two half-depth hall fire exits, glowing red letters inside a metal housing.
EXIT=mat('ExitRed',(.65,.001,.001),.4,em=2)
g='ExitSign';box(g,'exit sign housing',(0,0,0),(58,5,24),CREAM,1);box(g,'exit face',(0,-2.7,0),(55,.3,21),WHITE,.3);text(g,'exit letters','EXIT',(0,-3,0),16,EXIT);save(g,'wall',4)
place(g,'Hall_Exit',[1776.6,-526,F+267],90);place(g,'Stair_Main_Exit',[2020,-534,270],90);place(g,'Stair_Upper_Exit',[2020,-329,600],90)
# Physical, explicitly walkable route for later in-game automation.
MAN['route']=[dict(name='Basement existing entrance',p=[1680,-514,F]),dict(name='New hall',p=[2100,-525,F]),dict(name='Red room',p=[2570,-1030,F]),dict(name='Blue lounge',p=[2500,-220,F]),dict(name='Laundry',p=[2800,580,F]),dict(name='Basement stairs',p=[1510,534,F]),dict(name='Main entrance',p=[2110,-534,0]),dict(name='Upper entrance',p=[2130,-329,333.1])]
# The blue room's distinctive wall plaque, drinks ledge, and stored cable.
g='FordPlaque';box(g,'walnut plaque frame',(0,0,0),(143,4,37),WOOD,2);box(g,'black plaque inset',(0,-2.2,0),(129,.8,24),BLACK,1);text(g,'individual spaced lettering','F  O  R  D',(0,-3,0),17,PAPER);save(g,'wall',5);place(g,'Blue_FordPlaque',[3193,96,F+258],-90)
g='SportsPennant';box(g,'mounted sports print frame',(0,0,0),(78,3,108),WOOD,1);box(g,'deep red print background',(0,-1.7,0),(70,.3,100),RED);text(g,'sports print heading','TENNESSEE',(0,-2,33),7,PAPER);text(g,'sports print monogram','T',(0,-2,0),35,ORANGE);text(g,'sports print footer','VOLUNTEERS',(0,-2,-34),6,PAPER);save(g,'wall',3);place(g,'Blue_SportsPrint',[3194,-196,F+201],-90)
g='UtilityCable';tube(g,'coiled extension cord',[(42*math.cos(i*.13),29*math.sin(i*.13),1.8+i*.008)for i in range(200)],.75,YELLOW,8);tube(g,'loose cord lead',[(42,0,1),(65,-5,1),(90,-35,1)],.75,YELLOW,8);box(g,'cord plug',(90,-35,2),(6,3,3),YELLOW,.8);save(g,'prop',2);place(g,'Hall_CoiledCable',[2510,1010,F],10)
g='FireExtinguisher';cyl(g,'red cylinder',(0,0,22),8,40,RED);sphere(g,'rounded base',(0,0,3),(16,16,8),RED);sphere(g,'rounded shoulder',(0,0,42),(16,16,10),RED);box(g,'extinguisher label',(0,-8.1,23),(10,.1,20),PAPER);cyl(g,'brass valve',(0,0,47),2.6,6,GOLD);tube(g,'black hose',[(3,0,49),(11,0,43),(12,0,13)],1.5,BLACK);box(g,'squeeze handle',(0,0,52),(17,5,2),BLACK,.8);save(g,'wall',6);place(g,'Hall_FireExtinguisher',[2319,497,F+117],90)
g='StairNorthernPractical'
for z in[260,594]:
 box(g,'fluorescent rear tray',(1830,-653,z),(105,6,9),WHITE,1)
 cyl(g,'fluorescent opal tube',(1830,-647,z),1.6,100,LIGHT,'X')
 light('StairNorthern'+str(z),[1880,-547,z],580,320,4300)
save(g,'detail');place(g,g,(0,0,0))
(O/'manifest.json').write_text(json.dumps(MAN,indent=2));print('NOODLE_ART_PASS assets='+str(len(MAN['assets']))+' items='+str(len(MAN['placements'])),flush=True)
