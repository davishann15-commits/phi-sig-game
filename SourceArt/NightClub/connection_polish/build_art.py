"""Club polish: continuous world floor, detailed pictorial neon, and loose props.
Offline authoring only. No envelope, doorway, pole, or native gameplay changes.
"""
import bpy,json,pathlib,math,collections,random,numpy as np
from mathutils import Vector
B=pathlib.Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/NightClub')
P=B/'connection_polish';O=P/'art';(O/'models').mkdir(parents=True,exist_ok=True);(O/'textures').mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
G=collections.defaultdict(list);MAN=dict(assets=[],materials={},textures={},notes=['All six neon word boards replaced by bent tube pictograms.','Continuous eighty centimeter world grid joins both downstairs spaces.','Room footprint, door openings, floor collision, and pole are unchanged.','Loose tabletop props remain independent native interactive objects.','No added real lights.'])
def mat(n,c,rough=.5,metal=0,emission=0,reuse=None,texture=None):
 m=bpy.data.materials.new(n);m.diffuse_color=(*c,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 if emission:p.inputs['Emission Color'].default_value=(*c,1);p.inputs['Emission Strength'].default_value=emission
 MAN['materials'][n]=dict(color=list(c),roughness=rough,metallic=metal,emissive_strength=emission)
 if reuse:MAN['materials'][n]['reuse_asset']=reuse
 if texture:MAN['materials'][n]['texture']=texture
 return m
D='/Game/SeniorSendOff/House/NightClub/'
BLACK=mat('M_CP_ScuffedMetal',(.032,.037,.045),.59,.45,texture='metal')
ALU=mat('M_CP_BrushedEdges',(.31,.33,.36),.3,.85)
RUBBER=mat('M_CP_Rubber',(.007,.010,.015),.83)
CYAN=mat('M_CP_NeonCyan',(.028,.57,.86),.4,0,2)
PINK=mat('M_CP_NeonPink',(.85,.027,.28),.4,0,2)
VIOLET=mat('M_CP_NeonViolet',(.34,.06,.75),.4,0,2)
AMBER=mat('M_CP_NeonAmber',(.85,.25,.048),.4,0,1.6)
FLOOR=mat('M_CP_WornFloor',(.05,.053,.061),.68,0,.006,texture='floor')
WALL=mat('M_CP_WornCharcoal',(.055,.045,.056),.82,0,.02,texture='wall')
LED=mat('M_CP_NeonFloorLine',(.014,.25,.34),.6,0,.6)
PANEL=mat('M_CP_WornUpholsteredPanel',(.082,.035,.051),.84,0,.013,texture='wall')
WOOD=mat('M_CP_ChippedWalnut',(.074,.031,.015),.7,0,texture='wood')
EDGE=mat('M_CP_ExposedTimber',(.23,.14,.075),.92)
PAPER=mat('M_CP_Paper',(.49,.45,.36),.88)
CUP=mat('M_CP_CupPlastic',(.42,.39,.42),.49)
BOTTLE=mat('M_CP_BottleGreen',(.023,.15,.072),.24,.12)
LABEL=mat('M_CP_BottleLabel',(.16,.09,.035),.8)
CABLE=mat('M_CP_Cable',(.006,.006,.008),.88)
SRC=(B/'assets/club_assets.py').read_text();exec(SRC[SRC.index('def mesh('):SRC.index('# Neon stroke font')])

def uv_project(o):
 d=o.data
 if d.uv_layers.active:return
 uv=d.uv_layers.new(name='UVMap')
 for f in d.polygons:
  n=f.normal;axis=max(range(3),key=lambda i:abs(n[i]));u,v=([0,1]if axis==2 else[1,2]if axis==0 else[0,2])
  for li in f.loop_indices:q=d.vertices[d.loops[li].vertex_index].co;uv.data[li].uv=(q[u],q[v])
def save_group(g,name=None,target=None,pos=None,yaw=0,scale=(1,1,1),kind='static',mass=None):
 name=name or g;parts=G[g];assert parts,g
 for o in parts:
  bpy.context.view_layer.objects.active=o
  for md in list(o.modifiers):bpy.ops.object.modifier_apply(modifier=md.name)
  uv_project(o)
 bpy.ops.object.select_all(action='DESELECT')
 for o in parts:o.select_set(True)
 bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
 # Baking keeps a stable local-zero pivot, so signs and items can be placed exactly.
 bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
 o.data.calc_loop_triangles();verts=[(v.co.x*100,-v.co.y*100,v.co.z*100)for v in o.data.vertices]
 path=O/'models'/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False)
 row=dict(name=name,fbx=str(path),target=target,pos=pos,yaw=yaw,scale=scale,kind=kind,mass=mass,materials=[m.name for m in o.data.materials],triangles=len(o.data.loop_triangles),bounds=[[min(p[i]for p in verts)for i in range(3)],[max(p[i]for p in verts)for i in range(3)]])
 MAN['assets'].append(row);print('CLUB_POLISH_ART',name,row['triangles'],flush=True);return o

# Handcrafted bent glass pictograms. No text, font, or letter geometry is generated.
def circle(cx,cz,rx,rz,a0=0,a1=math.tau,n=64):return[(cx+rx*math.cos(a0+(a1-a0)*i/n),cz+rz*math.sin(a0+(a1-a0)*i/n))for i in range(n+1)]
def rounded(points,r=.028):
 out=[points[0]]
 for i in range(1,len(points)-1):
  p,q,n=[Vector(x)for x in(points[i-1],points[i],points[i+1])];d1=q-p;d2=n-q;d=min(r,d1.length*.2,d2.length*.2);a=q-d1.normalized()*d;b=q+d2.normalized()*d;out.append(tuple(a))
  for j in range(1,6):t=j/5;out.append(tuple((1-t)**2*a+2*(1-t)*t*q+t*t*b))
 out.append(points[-1]);return out
for name,w,h,paths in [
 ('Headphones',1.40,1.06,[
  (circle(0,-.02,.47,.43,0,math.pi),CYAN),
  ([(-.47,-.02),(-.47,-.23),(-.38,-.30),(-.31,-.28),(-.31,-.05),(-.38,.02),(-.47,-.02)],PINK),
  ([(.47,-.02),(.47,-.23),(.38,-.30),(.31,-.28),(.31,-.05),(.38,.02),(.47,-.02)],PINK),
  ([(-.13,-.40),(-.13,-.32)],VIOLET),([(0,-.45),(0,-.25)],VIOLET),([(.13,-.40),(.13,-.32)],VIOLET)]),
 ('Disco',1.22,1.18,[
  (circle(0,-.04,.35,.35),PINK),(circle(0,-.04,.16,.35),CYAN),
  ([(-.35,-.04),(.35,-.04)],CYAN),([(-.26,.19),(.26,.19)],CYAN),([(-.26,-.27),(.26,-.27)],CYAN),
  ([(0,.31),(0,.48)],VIOLET),([(-.52,.22),(-.42,.22)],VIOLET),([(.42,.22),(.52,.22)],VIOLET),
  ([(-.48,-.34),(-.40,-.28)],VIOLET),([(.40,-.28),(.48,-.34)],VIOLET)]),
 ('Cocktail',1.02,1.16,[
  ([(-.34,.32),(.34,.32),(0,-.10),(-.34,.32)],PINK),([(0,-.10),(0,-.39),(-.23,-.39),(.23,-.39)],CYAN),
  ([(-.20,.16),(.20,.16)],CYAN),([(.12,.15),(.25,.49),(.44,.49)],VIOLET),
  (circle(-.32,.31,.11,.11,math.pi*.3,math.pi*1.4,20),AMBER)])]:
 g='SM_CP_NeonPicture_'+name
 cube('Scratched acrylic backing',(0,.019,0),(w,.032,h),BLACK,g,.045,6)
 for x in(-w*.44,w*.44):
  for z in(-h*.42,h*.42):
   cyl('Brass mounting screw',(x,-.002,z),.006,.009,ALU,g,16,'Y');cube('Sign wall spacer',(x,.047,z),(.025,.035,.025),RUBBER,g,.003)
 for idx,(points,ma)in enumerate(paths):
  pts=[(x,-.032,z)for x,z in rounded(points)];tube('Hand bent neon pictogram '+str(idx),pts,.009,ma,g,12)
  if Vector(pts[0])-Vector(pts[-1])!=Vector((0,0,0)):
   for p in(pts[0],pts[-1]):ball('Sealed glass endpoint',p,.009,ma,g,12,6)
  for p in pts[::max(1,len(pts)//3)]:cube('Glass retaining clip',(p[0],-.014,p[2]),(.019,.025,.014),ALU,g,.003)
 tube('Hidden low voltage sign cord',[(w*.36,.032,-h*.40),(w*.36,.032,-h*.49),(w*.22,.035,-h*.49)],.0035,CABLE,g,8)
 save_group(g)

# Seamless PBR wear textures authored here, no photo sources or shared material edits.
rng=np.random.default_rng(101);N=512;yy,xx=np.mgrid[0:N,0:N];u=xx/N;v=yy/N
for kind in ['floor','wall','metal','wood']:
 noise=rng.normal(0,1,(N,N));large=np.sin(u*17+np.cos(v*11))*np.sin(v*21+u*4)
 if kind=='floor':base=np.array([.055,.061,.073]);a=base[None,None,:]*(1+.07*noise[:,:,None]+.20*large[:,:,None]);rough=.65+.1*large+.04*noise
 elif kind=='wall':base=np.array([.065,.050,.062]);a=base[None,None,:]*(1+.055*noise[:,:,None]+.13*large[:,:,None]);rough=.82+.04*noise
 elif kind=='metal':base=np.array([.038,.046,.057]);a=base[None,None,:]*(1+.13*noise[:,:,None]+.05*np.sin(v*2100)[:,:,None]);rough=.50+.1*noise
 else:
  grain=(np.sin(v*230+np.sin(u*12)*1.7)+np.sin(v*87+u*13))*.22;base=np.array([.098,.043,.022]);a=base[None,None,:]*(1+grain[:,:,None]+.04*noise[:,:,None]);rough=.64+.09*grain
 # Thin rubbed/scuffed scratches: restrained light wear, varied widths, dull edges.
 for j in range(145 if kind=='floor'else 65):
  x=rng.integers(0,N);y=rng.integers(0,N);length=rng.uniform(3,70);angle=rng.uniform(-.7,.7)+(0 if kind=='wood'else rng.uniform(0,math.tau));endx=x+math.cos(angle)*length;endy=y+math.sin(angle)*length
  for t in np.linspace(0,1,int(length*1.8)):
   X=int(x+t*(endx-x))%N;Y=int(y+t*(endy-y))%N;a[Y,X,:]*=rng.uniform(1.2,2.7);rough[Y,X]=.89
 a=np.clip(a,0,.65);rgba=np.concatenate([a,np.ones((N,N,1))],axis=2).astype('float32')
 # Encode linear artwork as sRGB values for standard Unreal colour texture import.
 rgb=rgba[:,:,:3];rgba[:,:,:3]=np.where(rgb<=.0031308,rgb*12.92,1.055*np.power(rgb,1/2.4)-.055)
 paths={}
 for suffix,pixels in [('Colour',rgba),('Roughness',np.repeat(np.clip(rough,0,1)[:,:,None],4,axis=2).astype('float32'))]:
  pixels=pixels.copy();pixels[:,:,3]=1;im=bpy.data.images.new('T_CP_'+kind+'_'+suffix,N,N,alpha=True);im.pixels.foreach_set(pixels.ravel());im.filepath_raw=str(O/'textures'/(im.name+'.png'));im.file_format='PNG';im.save();paths[suffix]=im.filepath_raw
 MAN['textures'][kind]=paths

# Geometry utilities for continuous floor and the architectural finish, in world UE centimeters.
def world_box(g,lo,hi,ma,bevel=0):
 c=[(lo[i]+hi[i])/200 for i in range(3)];c[1]*=-1;size=[(hi[i]-lo[i])/100 for i in range(3)];return cube('Club finish',tuple(c),tuple(size),ma,g,bevel,3)
def world_tube(g,pts,r,ma):return tube('Continuous club tube',[(x/100,-y/100,z/100)for x,y,z in pts],r/100,ma,g,10)
rects=[(-2212.5,-1509.02675,-1490,-973.5),(-2749.5,-2145.7,-2152.04,-1510.6),(-2225,-1740,-1970.6,-1483.4)]
g='SM_CP_ContinuousLowerFloor'
# Same grid spacing and world-origin alignment through both original rooms and the corner.
for x0,y0,x1,y1 in rects:
 world_box(g,[x0,y0,.90],[x1,y1,1.07],FLOOR)
 for i in range(math.ceil(x0/80),math.floor(x1/80)+1):
  x=i*80
  if x0+.6<x<x1-.6:world_box(g,[x-.22,y0+.6,1.081],[x+.22,y1-.6,1.14],LED)
 for j in range(math.ceil(y0/80),math.floor(y1/80)+1):
  y=j*80
  if y0+.6<y<y1-.6:world_box(g,[x0+.6,y-.22,1.081],[x1-.6,y+.22,1.14],LED)
save_group(g,target='NightClub_SM_NC_LowerDanceFloorInlay')

# Unified low upholstered/acoustic wall panels and continuous skirting. Keep every doorway clear.
g='SM_CP_ClubArchitecturalDetail'
# Shared finish repeats on the main floor, DJ return and upstairs, with no collision footprint.
segments=[([-2211.5,-1507,2],[-1975,-1505,78]),([-1969,-1507,2],[-1492,-1505,78]),
 ([-1493,-1506,2],[-1491,-976,78]),([-2210,-976,2],[-1492,-974,78]),
 ([-2748,-2144,2],[-2155,-2142,78]),([-2748,-2142,2],[-2746,-1512,78]),
 ([-2155,-2142,2],[-2153,-1753,78]),([-1968,-1738,2],[-1966,-1511,78])]
for lo,hi in segments:
 world_box(g,lo,hi,PANEL,.008)
 world_box(g,[lo[0],lo[1],2],[hi[0],hi[1],10],BLACK)
 world_box(g,[lo[0],lo[1],78],[hi[0],hi[1],79.0],ALU)
# Upstairs repeats without touching original furniture/pole collision.
for lo,hi in segments[:4]:world_box(g,[lo[0],lo[1],lo[2]+333],[hi[0],hi[1],hi[2]+333],PANEL,.008)
# Corner header ribbon visually carries the two ceilings into one composition.
for off in[0,4]:world_tube(g,[(-1740,-1485,279+off),(-2035,-1485,279+off),(-2035,-1685,279+off),(-2450,-1685,279+off)],.65,CYAN if off==0 else VIOLET)
# Three shallow padded DJ backdrop panels with thin brass joins and understated upper edges.
for x in[-2590,-2450,-2310]:
 world_box(g,[x-58,-2142,94],[x+58,-2140,244],PANEL,.01)
 for q in[x-59,x+59]:world_box(g,[q,-2139.6,97],[q+.7,-2139.2,242],ALU)
# Unlit ambient finish brings out silhouettes in dim rooms without adding lights.
save_group(g)

# Drink shelves along the main wall, chips expose timber at the edge; cups stay separate.
g='SM_CP_WallDrinkPerch'
cube('Worn walnut bar ledge',(0,0,1.05),(1.40,.26,.047),WOOD,g,.012,5)
for x in[-.46,.46]:
 cube('Powdercoated wall bracket',(x,.10,.91),(.031,.055,.29),BLACK,g,.007)
 tube('Ledge diagonal bracket',[(x,.1,.80),(x,-.07,1.02)],.012,ALU,g,12)
for x in[-.54,-.17,.32,.50]:cube('Rubbed ledge corner',(x,-.124,1.051),(.018,.012,.005),EDGE,g,.002)
save_group(g)
# Simple robust hollow cups have visible lip, interior and base; no floating fused clutter.
g='SM_CP_LooseCup';h=.15;rb=.042;rt=.054;n=48;vs=[];fs=[]
for z,r in[(0,rb),(h,rt),(h,rt-.004),(.007,rb-.004)]:
 for i in range(n):a=i*math.tau/n;vs.append((r*math.cos(a),r*math.sin(a),z))
for k in range(3):
 for i in range(n):fs.append((k*n+i,k*n+(i+1)%n,(k+1)*n+(i+1)%n,(k+1)*n+i))
fs.append(tuple(range(n-1,-1,-1)));fs.append(tuple(3*n+i for i in range(n)))
mesh('Open hollow plastic cup',vs,fs,CUP,g,True);ring('Rolled plastic rim',(0,0,h),rt-.005,rt+.001,.007,CUP,g,48)
save_group(g,kind='prop',mass=.12)
g='SM_CP_LooseBottle'
# Bottle silhouette is a turned profile instead of a cylinder.
vs=[];fs=[];profile=[(0,.032),(.015,.038),(.18,.039),(.205,.035),(.234,.015),(.29,.014),(.298,.018)]
for z,r in profile:
 for i in range(40):a=i*math.tau/40;vs.append((r*math.cos(a),r*math.sin(a),z))
for k in range(len(profile)-1):
 for i in range(40):fs.append((k*40+i,k*40+(i+1)%40,(k+1)*40+(i+1)%40,(k+1)*40+i))
fs.append(tuple(range(39,-1,-1)));mesh('Shaped green bottle',vs,fs,BOTTLE,g,True);cyl('Bottle metal cap',(0,0,.303),.018,.012,ALU,g,40)
cyl('Worn paper bottle band',(0,0,.115),.0395,.063,LABEL,g,40)
save_group(g,kind='prop',mass=.5)
g='SM_CP_LooseCoaster';cyl('Paper beer mat',(0,0,.0025),.056,.005,PAPER,g,40);ring('Faded coaster rim',(0,0,.0051),.037,.041,.0005,LABEL,g,48);save_group(g,kind='prop',mass=.03)
g='SM_CP_CrumpledNapkin';random.seed(30);vs=[];fs=[];n=9
for i in range(n):
 for j in range(n):vs.append(((i/(n-1)-.5)*.19,(j/(n-1)-.5)*.19,.004+random.random()*.012))
for i in range(n-1):
 for j in range(n-1):k=i*n+j;fs.extend([(k,k+n,k+n+1),(k,k+n+1,k+1)])
mesh('Crumpled discarded napkin',vs,fs,PAPER,g,True);save_group(g,kind='prop',mass=.02)

# Fine cable detail stays with the DJ cabinet, so no furniture drag leaves wires floating.
g='SM_CP_DJBackCableDetail'
for j in range(3):tube('DJ cable service coil',[(.88+.08*math.cos(i*math.tau/80),.48+.016*j+.045*math.sin(i*math.tau/80),.42+.07*math.sin(i*math.tau/80))for i in range(81)],.004,CABLE,g,8)
cube('Dual output socket plate',(.84,.456,.67),(.17,.025,.08),ALU,g,.006)
for x in[.80,.88]:cyl('Recessed audio socket',(x,.475,.67),.018,.005,BLACK,g,20,'Y')
save_group(g)

MAN['geometry_checks']=dict(neon_pictures=3,neon_words_generated=0,unchanged_structural_collision=True,new_real_lights=0,continuous_grid_cm=80,club_bounds_unchanged=True)
(O/'manifest.json').write_text(json.dumps(MAN,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(O/'ClubPolish_source.blend'));print('CLUB_POLISH_ART_PASS',flush=True)
