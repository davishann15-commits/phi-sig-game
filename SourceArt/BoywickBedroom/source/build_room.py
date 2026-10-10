"""Photo-reference room. Metres for furniture; house horizontal factor 1.8, height unchanged.
Local mesh Y is reflected at export to compensate for the FBX right/left handed conversion.
Reference photos are kept unmodified and sampled by UV coordinates on the wall art.
"""
import bpy,bmesh,json,math,random,runpy,collections
from pathlib import Path
from mathutils import Vector,Matrix
P=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff');R=P/'SourceArt/BoywickBedroom';D=R/'models';DEST='/Game/SeniorSendOff/House/BoywickBedroom'
DM=runpy.run_path(str(P/'SourceArt/HouseInteractivity/source/generate_damage_v2.py'));G=collections.defaultdict(list);M={};rng=random.Random(2330)
bpy.ops.wm.read_factory_settings(use_empty=True)
doc=dict(assets=[],placements=[],materials={},notes=['All six original photos reviewed.','Photo dimensions inferred using standard furniture proportions; not a measured survey.','House horizontal scale 1.8 and existing ceiling height retained.','One loft-bed unit anchored in the far-left corner from the entrance.'])
exec(compile((P/'SourceArt/ReferenceKitchen/source/helpers.py').read_text().split('def finalize(')[0],'helpers.py','exec'))
def mat(k,c,rough=.7,metal=0,tex=None,normal=None,emission=0,two=False):
 n='M_BWR_'+k;m=bpy.data.materials.new(n);m.diffuse_color=(*c,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 d=dict(base_color=c,roughness=rough,metallic=metal,two_sided=two or any(w in k for w in ['Fabric','Cotton','Shirt','Photo','Paper']))
 if tex:
  d['texture']=str(tex);t=m.node_tree.nodes.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(tex),check_existing=True);m.node_tree.links.new(t.outputs['Color'],p.inputs['Base Color'])
 if normal:d['normal_texture']=str(normal)
 if emission:d['emission']=emission
 M[k]=m;doc['materials'][n]=d;return m
K=P/'SourceArt/ReferenceKitchen/textures';BR=P/'SourceArt/AcrossPosterBedroom';RR=P/'SourceArt/Room204'
metal=mat('BlackSteel',(.013,.015,.018),.4,.65);rubber=mat('Rubber',(.012,.014,.018),.89);silver=mat('BrushedSteel',(.38,.40,.42),.31,.8)
wall=mat('WarmGreyPaint',(.43,.41,.37),.82);white=mat('WhiteEnamel',(.77,.77,.72),.47);ceiling=mat('CeilingPaint',(.72,.72,.69),.93);black=mat('BlackPlastic',(.009,.012,.016),.47)
wood=mat('WalnutWood',(1,1,1),.58,tex=K/'DarkWalnut.png');oak=mat('OakFloorWood',(1,1,1),.67,tex=K/'OakPlanks.png',normal=K/'OakPlanks_Normal.png')
fabric=mat('CharcoalFabric',(.078,.082,.087),.95,normal=RR/'T_R204_BlackQuilt_Normal.png');grey=mat('GreyFabric',(.26,.28,.30),.96,normal=RR/'T_R204_BlackQuilt_Normal.png');cotton=mat('WhiteQuiltedCotton',(.78,.79,.75),.96,normal=RR/'T_R204_WhiteSheet_Normal.png');seam=mat('CottonStitches',(.52,.53,.51),1)
orange=mat('OrangeTowelFabric',(.74,.26,.043),.98,normal=RR/'T_R204_WhiteSheet_Normal.png');beige=mat('TaupeTowelFabric',(.39,.34,.265),.97,normal=RR/'T_R204_WhiteSheet_Normal.png');paper=mat('Paper',(.83,.83,.78),.94);red=mat('RedPrint',(.55,.028,.023),.7);blue=mat('BluePlastic',(.021,.13,.30),.62);green=mat('GreenBottle',(.045,.22,.055),.28);purple=mat('PurpleBottle',(.23,.105,.33),.4);cardboard=mat('Cardboard',(.41,.31,.19),.92);gold=mat('BrassGrommets',(.45,.30,.09),.35,.8)
light=mat('FrostedLight',(.86,.88,.83),.3,emission=1.2);screen=mat('ScreenGlass',(.012,.025,.05),.18,.15,emission=.22)
photos={i:mat('Photo'+str(i),(1,1,1),.86,tex=R/'references'/('IMG_%s.jpeg'%i),two=True) for i in [2327,2328,2329,2330]}
labelmats={k:mat(k,(1,1,1),.7,tex=K/(k+'.png'))for k in ['PeanutLabel','CerealLabel','SnackBox','EnergyLabel','PaperPack']}
clothcolors=[mat('Shirt'+str(i),c,.96,normal=RR/'T_R204_WhiteSheet_Normal.png')for i,c in enumerate([(.12,.17,.24),(.44,.45,.44),(.73,.71,.66),(.51,.27,.24),(.85,.30,.02),(.018,.021,.026),(.60,.52,.13),(.13,.24,.18)])]

def finish(g,mass=10,health=30,interactive=True,damage=False,wallmount=False,scale=1.8,collision=True):
 objects=G[g];mats=[]
 for o in objects:
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);uv_auto(o)
  for v in o.data.vertices:v.co.x*=scale;v.co.y*=-scale
  bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free();o.data.update()
  for m in o.data.materials:
   if m not in mats:mats.append(m)
 for o in objects:
  old=list(o.data.materials);ix=[mats.index(old[p.material_index])for p in o.data.polygons];o.data.materials.clear()
  for m in mats:o.data.materials.append(m)
  for p,i in zip(o.data.polygons,ix):p.material_index=i
 name='SM_BWR_'+g;ob=DM['join_meshes'](objects,name,mats)
 hulls=DM['hulls_for'](objects,name)if interactive or collision else []
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True)
 for h in hulls:h.select_set(True)
 bpy.context.view_layer.objects.active=ob;file=D/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
 lo,hi=DM['bbox'](ob);row=dict(id=g,asset_name=name,intact_fbx=str(file),material_names=[m.name for m in mats],material_slots=[DEST+'/'+m.name+'.'+m.name for m in mats],behavior='furniture' if interactive else 'architecture',interactive=interactive,damage=damage,mass_kg=mass,break_health=health,movable=interactive,wall=wallmount,collision=collision,faces=len(ob.data.polygons),hulls=len(hulls),bounds=[list(lo),list(hi)])
 doc['assets'].append(row)
 for o in [ob,*hulls,*objects]:DM['discard'](o)
 G.pop(g,None);print('BOYWICK_ROOM_MODEL',g,row['faces'],flush=True);return row

def place(g,x,y,z=0,yaw=0,name=None,scale=None):
 d=dict(asset=g,location=[x,y,302.3+z],yaw=yaw,name=name or g)
 if scale:d['scale']=scale
 doc['placements'].append(d)

def pipe_rect(g,c,w,h,m=metal,r=.009):
 x,y,z=c;tube('continuous frame',[(x-w/2,y,z),(x+w/2,y,z),(x+w/2,y,z+h),(x-w/2,y,z+h)],r,m,g,10,True)
def bolt(g,p,axis='Y'):cyl('round frame fastener',p,.006,.004,silver,g,12,axis)
def cloth(g,n,c,w,d,m,amp=.018,folds=6,vertical=False,phase=0):
 nx,ny=32,22;vs=[];fs=[]
 for j in range(ny+1):
  v=j/ny
  for i in range(nx+1):
   u=i/nx;x=(u-.5)*w;y=(v-.5)*d
   f=amp*(math.sin(u*folds*math.tau+phase+v*2)*.55+math.sin(v*15+u*11)*.3+math.cos(u*28-v*8)*.15)*(.3+.7*math.sin(math.pi*u))
   vs.append((c[0]+x,c[1]+f,c[2]+y)if vertical else(c[0]+x,c[1]+y,c[2]+f))
 for j in range(ny):
  for i in range(nx):a=j*(nx+1)+i;fs.append((a,a+1,a+nx+2,a+nx+1))
 o=mesh(n,vs,fs,m,g,True);uv=o.data.uv_layers.new(name='UVMap')
 for p in o.data.polygons:
  for li in p.loop_indices:
   vi=o.data.loops[li].vertex_index;uv.data[li].uv=((vi%(nx+1))/nx,(vi//(nx+1))/ny)
 return o

def pillow(g,c,w=.43,d=.40,h=.12,m=grey,photo=False):
 nx,ny=20,18;v=[];f=[]
 for side in [1,-1]:
  for j in range(ny+1):
   y=j/ny
   for i in range(nx+1):
    x=i/nx;bump=(math.sin(math.pi*x)*math.sin(math.pi*y))**.55;z=side*(.012+h/2*bump)+.005*math.sin(x*43+y*9)*(1-bump)
    v.append((c[0]+(x-.5)*w,c[1]+(y-.5)*d,c[2]+z))
 N=(nx+1)*(ny+1)
 for s in range(2):
  for j in range(ny):
   for i in range(nx):a=s*N+j*(nx+1)+i;f.append((a,a+1,a+nx+2,a+nx+1)if s==0 else(a+nx+1,a+nx+2,a+1,a))
 boundary=list(range(nx+1))+[j*(nx+1)+nx for j in range(1,ny+1)]+[ny*(nx+1)+i for i in range(nx-1,-1,-1)]+[j*(nx+1)for j in range(ny-1,0,-1)]
 for a,b in zip(boundary,boundary[1:]+boundary[:1]):f.append((a,b,b+N,a+N))
 o=mesh('soft cushion with pinched seam',v,f,m,g,True);layer=o.data.uv_layers.new(name='UVMap')
 for p in o.data.polygons:
  for li in p.loop_indices:
   vi=o.data.loops[li].vertex_index%N;u=(vi%(nx+1))/nx;vv=(vi//(nx+1))/ny
   if photo:layer.data[li].uv=(.115+u*.13,1-(.81+vv*.07))
   else:layer.data[li].uv=(u,vv)
 tube('sewn edge piping',[v[i]for i in boundary],.0025,m,g,6,True)
 return o

def photo_panel(g,photo,quad,w,h,fold=False):
 # Quad ordered top-left, top-right, bottom-right, bottom-left in normalized photo coordinates.
 nx,ny=(30,20)if fold else(12,12);v=[];f=[];uvs=[]
 for j in range(ny+1):
  t=j/ny
  for i in range(nx+1):
   s=i/nx;x=(s-.5)*w;z=(.5-t)*h
   y=.012*math.sin(s*29+t*12)*math.sin(s*math.pi)*math.sin(t*math.pi)if fold else 0
   v.append((x,y,z));a=Vector(quad[0]).lerp(Vector(quad[1]),s);b=Vector(quad[3]).lerp(Vector(quad[2]),s);q=a.lerp(b,t)
   if not fold:
    a=Vector(quad[0]).lerp(Vector(quad[1]),1-s);b=Vector(quad[3]).lerp(Vector(quad[2]),1-s);q=a.lerp(b,t)
   uvs.append((q.x,1-q.y))
 for j in range(ny):
  for i in range(nx):a=j*(nx+1)+i;f.append((a,a+1,a+nx+2,a+nx+1))
 o=mesh('reference artwork fabric'if fold else'reference print',v,f,photos[photo],g,fold);uv=o.data.uv_layers.new(name='UVMap')
 for p in o.data.polygons:
  for li in p.loop_indices:uv.data[li].uv=uvs[o.data.loops[li].vertex_index]
 if fold:
  for x in [-w/2+.014,w/2-.014]:
   for z in [-h/2+.014,h/2-.014]:
    pts=[(x+.006*math.cos(a*math.tau/16),-.006,z+.006*math.sin(a*math.tau/16))for a in range(16)];tube('brass fabric eyelet',pts,.002,gold,g,6,True)
 else:box('print backing',(0,.009,0),(w,.006,h),paper,g,.001)

# A single black steel loft frame, with round tubing and real fasteners.
g='LoftFrame';L,W=2.02,1.37
for x in [-L/2,L/2]:
 for y in [-W/2,W/2]:
  tube('powder coated bed post',[(x,y,.012),(x,y,1.93)],.019,metal,g,14);cyl('plastic post cap',(x,y,1.935),.020,.012,black,g);cyl('floor cap',(x,y,.015),.023,.03,rubber,g)
for z in [.27,1.43]:
 for y in [-W/2,W/2]:box('rounded side load rail',(0,y,z),(L,.045,.062),metal,g,.008,4)
 for x in [-L/2,L/2]:box('end load rail',(x,0,z),(.042,W,.061),metal,g,.008,4)
 for i in range(15):box('mattress support slat',(-.93+i*.133,0,z+.018),(.023,W,.02),metal,g,.006)
for y in [-W/2,W/2]:
 for z in [1.73,1.86]:tube('round safety rail',[(-L/2,y,z),(L/2,y,z)],.012,metal,g,12)
 for x in [-.35,.35]:tube('guard rail upright',[(x,y,1.43),(x,y,1.86)],.010,metal,g)
for x in [-L/2,L/2]:
 for z in [.52,.77,1.02,1.27,1.70,1.86]:tube('built in ladder rung',[(x,-W/2,z),(x,W/2,z)],.014,metal,g,12)
 for y in [-W/2,W/2]:
  for z in [.27,1.43,1.73]:bolt(g,(x,y-.02,z))
finish(g,65,160,damage=True);place(g,-2782,-1230,name='Black steel loft bed')

g='QuiltedMattress';box('rounded full mattress',(0,0,.135),(1.965,1.31,.27),cotton,g,.055,6)
for z in [.022,.065,.218,.257]:
 pts=[]
 for i in range(97):
  a=i*math.tau/96;pts.append((.968*math.copysign(abs(math.cos(a))**.19,math.cos(a)),.640*math.copysign(abs(math.sin(a))**.19,math.sin(a)),z))
 tube('stitched mattress edge',pts,.0025,seam,g,6,True)
for j in range(9):
 yy=-.60+j*.15;pts=[(-.93+i*.023,yy+.035*math.sin(i*.31+j*1.9),.272)for i in range(82)];tube('quilted cotton curved stitching',pts,.0014,seam,g,5)
finish(g,30,65,damage=True);place(g,-2782,-1230,147,name='Quilted white mattress')
g='LowDaybed';box('lower upholstered pad',(0,0,.105),(1.97,1.31,.21),fabric,g,.062,6);cloth(g,'rumpled charcoal cover',(0,0,.214),1.98,1.33,fabric,.024,5);finish(g,26,70,damage=True);place(g,-2782,-1230,31,name='Dark futon pad under loft')
g='GreyPillow';pillow(g,(0,0,.065),.42,.72,.13,grey);finish(g,1,8);place(g,-2910,-1230,176,yaw=-4,name='Gray pillow');place(g,-2918,-1220,55,yaw=8,name='Dark daybed pillow')
g='BunchedOrangeTowel';cloth(g,'rumpled orange towel',(0,0,.035),.55,.48,orange,.048,5);cloth(g,'folded towel ridge',(.05,0,.067),.31,.38,orange,.065,3,phase=1);finish(g,.7,8);place(g,-2770,-1239,176,18,'Orange towel')
g='DrapedTowel';cloth(g,'heavy taupe towel hanging',(0,0,.59),.55,1.13,beige,.035,7,True);cloth(g,'fold over rail',(0,.045,1.15),.55,.09,beige,.01,7);finish(g,1.1,8,wallmount=True);place(g,-2690,-1102,25,0,'Towel over loft rail')
# Rounded futon with individual tufted sections.
g='GreyFuton';box('sofa base',(0,0,.21),(2.0,.77,.24),fabric,g,.06,5)
for x in [-.88,.88]:
 for y in [-.29,.29]:cyl('small sofa foot',(x,y,.065),.024,.13,black,g,20)
for x in [-.5,.5]:
 box('soft seat cushion',(x,-.07,.383),(.986,.64,.17),fabric,g,.055,6)
 o=box('back cushion',(x,.31,.67),(.986,.16,.62),fabric,g,.055,6);o.rotation_euler.x=math.radians(-12)
 for xx in [x-.32,x,x+.32]:
  for z in [.54,.76]:ell('recessed tuft button',(xx,.216+(z-.67)*.2,z),(.014,.004,.013),black,g)
for x in [-.97,0,.97]:tube('upholstery seam',[(x,-.385,.39),(x,.16,.39),(x,.19,.52),(x,.27,.95)],.0025,grey,g)
finish(g,44,100,damage=True);place(g,-2708,-657,0,180,'Charcoal gray futon')
g='PatternedPillow';pillow(g,(0,0,.065),.43,.43,.15,photos[2328],True);finish(g,1.2,8);place(g,-2790,-699,53,15,'Patterned teal pillow');place(g,-2720,-689,55,-18,'Patterned teal pillow')
# Rustic plank table, visible grain, joins and lower shelf.
g='CoffeeTable'
for i in range(4):box('individual walnut tabletop plank',(0,-.237+i*.158,.46),(1.32,.152,.045),wood,g,.006,4)
for x in [-.575,.575]:
 for y in [-.225,.225]:box('square timber leg',(x,y,.216),(.078,.078,.432),wood,g,.005)
for y in [-.251,.251]:box('mortised long apron',(0,y,.383),(1.18,.042,.12),wood,g)
for x in [-.60,.60]:box('end apron',(x,0,.383),(.04,.5,.12),wood,g)
box('bottom magazine shelf',(0,0,.11),(1.2,.50,.028),wood,g,.004)
for x in [-.58,.58]:
 for y in [-.23,.23]:bolt(g,(x,y,.484),'Z')
finish(g,28,65,damage=True);place(g,-2695,-878,0,0,'Rustic coffee table')
# TV console and fabric drawer cabinet.
g='Dresser';box('drawer unit skeleton',(0,0,.35),(1.08,.35,.70),black,g,.009)
for r in range(3):
 for c in range(3):
  x=-.352+c*.352;z=.13+r*.224;box('fabric storage drawer',(x,-.18,z),(.34,.028,.21),fabric,g,.006);tube('stitched pull',[(x-.042,-.2,z+.02),(x-.03,-.216,z-.006),(x+.03,-.216,z-.006),(x+.042,-.2,z+.02)],.004,black,g)
box('dark wood dresser top',(0,0,.711),(1.10,.37,.024),wood,g,.005);finish(g,22,54,damage=True);place(g,-2506,-1329,0,180,'Dark fabric dresser')
g='SmallTelevision';box('television bezel',(0,0,.275),(.70,.03,.415),black,g,.009);box('inset display',(0,-.017,.275),(.673,.002,.388),screen,g,.004)
for x in [-.25,.25]:tube('splayed TV stand',[(x-.04,-.075,.008),(x,0,.079),(x+.04,.055,.008)],.007,black,g)
for i in range(9):box('screen news text line',(-.012,-.019,.412-i*.027),(.53-rng.random()*.14,.001,.006),paper,g,0)
for i,c in enumerate([blue,orange,red]):box('screen thumbnail',(-.224+i*.22,-.019,.144),(.19,.001,.06),c,g,0)
finish(g,5,25,damage=True);place(g,-2502,-1324,73,180,'Television')
g='GameConsole';box('console centre',(0,0,.175),(.065,.18,.35),black,g,.009);box('curved white console left',(-.034,0,.178),(.012,.19,.36),white,g,.005);box('curved white console right',(.034,0,.178),(.012,.19,.36),white,g,.005);finish(g,3,20,damage=True);place(g,-2579,-1324,73,180,'White game console')
# White desk, adjustable task lamp, green chair reused from corrected kitchen.
g='WritingDesk';box('white writing surface',(0,0,.74),(1.01,.53,.04),white,g,.009,4)
for x in [-.445,.445]:
 for y in [-.205,.205]:box('tapered desk leg',(x,y,.355),(.044,.044,.71),white,g,.004)
for y in [-.233,.233]:box('desk apron',(0,y,.677),(.93,.025,.086),white,g,.003)
box('small pencil drawer',(0,-.252,.684),(.39,.022,.072),white,g,.004);tube('drawer handle',[(-.05,-.27,.686),(.05,-.27,.686)],.005,silver,g)
finish(g,23,60,damage=True);place(g,-2918,-1000,0,90,'White desk')
g='DeskLamp';cyl('lamp base',(0,0,.015),.08,.025,white,g);tube('jointed lamp arm',[(0,0,.025),(0,.01,.29),(.04,-.12,.46)],.01,white,g,12);box('long LED task head',(.04,-.16,.458),(.045,.26,.024),white,g,.007);box('light strip',(.04,-.16,.444),(.025,.22,.004),light,g,.001);finish(g,1.7,12);place(g,-2940,-1022,76,90,'Adjustable desk lamp')
# Freestanding black wire rack with actual open wire shelves.
g='WireRack'
for x in [-.37,.37]:
 for y in [-.245,.245]:cyl('shelf post',(x,y,.92),.012,1.84,metal,g,16)
for z in [.87,1.19,1.50,1.82]:
 for y in [-.245,.245]:tube('shelf perimeter',[(-.37,y,z),(.37,y,z)],.009,metal,g,10)
 for x in [-.37,.37]:tube('shelf perimeter',[(x,-.245,z),(x,.245,z)],.009,metal,g,10)
 for i in range(25):x=-.355+i*.0295;tube('wire shelf rod',[(x,-.24,z),(x,.24,z)],.0028,metal,g,6)
 for y in [-.17,.17]:tube('wire reinforcement',[(-.37,y,z-.018),(.37,y,z-.018)],.004,metal,g,8)
finish(g,13,60,damage=True);place(g,-2920,-835,0,90,'Black wire storage rack')
g='MiniFridge';box('black refrigerator cabinet',(0,0,.413),(.46,.455,.826),black,g,.016,5);box('door gasket',(0,-.238,.427),(.443,.018,.778),rubber,g,.01);box('rounded fridge door',(0,-.254,.426),(.445,.035,.782),metal,g,.018,5);box('recessed top handle',(0,-.275,.77),(.28,.007,.020),black,g,.006);text('fridge badge','compact',(0,-.276,.691),.012,silver,g);finish(g,32,90,damage=True);place(g,-2918,-835,0,90,'Black mini fridge')
# Open containers, drink cups, bottle necks and labels instead of cylinders with no detail.
def cup(g,m=paper):
 N=32;vs=[];fs=[]
 for z,r in [(0,.030),(.14,.048),(.14,.044),(.008,.027)]:
  for i in range(N):a=i*math.tau/N;vs.append((r*math.cos(a),r*math.sin(a),z))
 for j in range(3):
  for i in range(N):a=j*N+i;b=j*N+(i+1)%N;fs.append((a,b,b+N,a+N))
 fs.append(tuple(range(3*N,4*N)));mesh('hollow tapered drink cup',vs,fs,m,g,True);tube('rolled cup rim',[(.047*math.cos(i*math.tau/32),.047*math.sin(i*math.tau/32),.14)for i in range(32)],.0025,m,g,8,True)
g='DrinkCup';cup(g);text('red cup mark','C',(0,-.041,.05),.049,red,g);finish(g,.16,5,scale=1.3);place(g,-2750,-888,48,10,'Drink cup');place(g,-2638,-871,48,-5,'Drink cup');place(g,-2665,-858,48,15,'Drink cup');place(g,-2710,-912,48,-20,'Drink cup');place(g,-2890,-985,76,90,'Desk drink cup')
g='LiddedCup';cup(g);cyl('drink lid',(0,0,.143),.050,.006,white,g,32);tube('red drinking straw',[(0,0,.145),(.004,-.004,.227)],.0025,red,g,8);text('red cup logo','C',(0,-.041,.054),.042,red,g);finish(g,.3,6,scale=1.3);place(g,-2780,-865,48,-10,'Takeout drink');place(g,-2724,-869,48,25,'Takeout drink')
g='PaperTowelRoll';cyl('embossed towel roll',(0,0,.14),.058,.28,cotton,g,36);cyl('cardboard tube inset',(0,0,.2805),.020,.001,cardboard,g,24);cyl('dark hollow centre',(0,0,.281),.015,.001,black,g,24);cloth(g,'loose paper towel sheet',(.061,-.01,.14),.03,.24,paper,.006,3,True);finish(g,.45,6,scale=1.25);place(g,-2728,-897,48,0,'Paper towels')
g='WaterBottle';cyl('purple reusable bottle',(0,0,.118),.039,.236,purple,g,32);cyl('bottle shoulder',(0,0,.239),.031,.025,purple,g);cyl('screw neck',(0,0,.260),.022,.025,purple,g);cyl('black cap',(0,0,.278),.025,.02,black,g);box('orange sticker',(0,-.039,.11),(.052,.002,.092),orange,g,.003);finish(g,.6,8,scale=1.25);place(g,-2678,-891,48,8,'Purple water bottle')
g='GreenBottle';cyl('green bottle body',(0,0,.095),.028,.19,green,g,24);ell('bottle shoulder',(0,0,.187),(.028,.028,.027),green,g);cyl('bottle neck',(0,0,.225),.011,.065,green,g,20);cyl('cap',(0,0,.259),.012,.008,white,g);cyl('white wrap label',(0,0,.10),.0285,.068,paper,g);finish(g,.3,5,scale=1.25)
for x,y in [(-2896,-945),(-2920,-947),(-2907,-963)]:place(g,x,y,76,90,'Green bottle')
g='SnackJar';cyl('amber snack jar',(0,0,.125),.067,.25,mat('AmberJar',(.25,.12,.025),.23),g,36);cyl('black jar lid',(0,0,.264),.071,.028,black,g,36);flat('photo peanut label',(0,-.068,.13),.10,.125,labelmats['PeanutLabel'],g,True);finish(g,.8,8,scale=1.25);place(g,-2601,-888,48,0,'Large snack jar')
g='Shaker';cyl('black shaker',(0,0,.10),.037,.20,black,g);cyl('shaker lid',(0,0,.21),.04,.027,black,g);box('flip cap',(0,-.012,.229),(.030,.04,.015),black,g,.008);finish(g,.35,6,scale=1.25);place(g,-2610,-915,48,15,'Black shaker')
g='SnackCarton';box('printed cereal carton',(0,0,.135),(.12,.047,.27),cardboard,g,.003);flat('printed front',(0,-.024,.135),.12,.27,labelmats['CerealLabel'],g,True);finish(g,.3,8,scale=1.45)
for y,z in [(-884,151),(-852,120),(-812,89)]:place(g,-2898,y,z,90,'Cereal and snack box')
g='ProteinTub';cyl('protein powder tub',(0,0,.103),.072,.206,white,g,28);cyl('blue ribbed lid',(0,0,.217),.074,.025,blue,g,28);flat('supplement label',(0,-.073,.11),.1,.12,labelmats['PaperPack'],g,True);finish(g,1.4,10,scale=1.3);place(g,-2895,-870,89,90,'Protein powder tub');place(g,-2916,-795,151,90,'Storage tub')
g='SnackBag';o=cloth(g,'crumpled snack packaging',(0,0,.11),.15,.21,labelmats['SnackBox'],.014,4,True);box('bag folded bottom',(0,.008,.025),(.135,.055,.027),cardboard,g,.01);finish(g,.15,5,scale=1.25)
for x,y,z,rot in [(-2890,-875,120,90),(-2913,-828,120,90),(-2926,-794,120,90),(-2860,-815,1,30),(-2640,-723,44,20),(-2830,-790,0,100)]:place(g,x,y,z,rot,'Snack bag')
# Loose belongings, each independent so they can be picked up.
g='Laptop';box('aluminium laptop base',(0,0,.008),(.32,.22,.016),silver,g,.006);box('keyboard surround',(0,0,.017),(.29,.155,.001),black,g,.001)
for j in range(5):
 for i in range(12):box('laptop key',(-.129+i*.023,-.005+j*.025,.018),(.018,.018,.002),grey,g,.001)
box('trackpad',(0,-.075,.018),(.093,.053,.0015),grey,g,.003)
o=box('open display',(0,.098,.108),(.32,.012,.213),silver,g,.005);o.rotation_euler.x=math.radians(-13)
o=box('black glass laptop screen',(0,.086,.108),(.30,.003,.189),screen,g,.002);o.rotation_euler.x=math.radians(-13)
finish(g,1.9,14,damage=True,scale=1.3);place(g,-2578,-716,47,150,'Laptop on futon')
g='Controller';ell('gamepad body',(0,0,.027),(.075,.042,.021),black,g)
for x in [-.055,.055]:ell('controller grips',(x,.014,.018),(.025,.037,.02),black,g)
for x in [-.025,.025]:cyl('thumb stick',(x,-.002,.047),.010,.010,rubber,g,16)
for x,y in [(.047,-.025),(.058,-.018),(.046,-.010),(.035,-.018)]:cyl('gamepad button',(x,y,.045),.003,.004,paper,g,12)
finish(g,.35,7,scale=1.3);place(g,-2790,-724,49,-30,'Game controller')
g='PaperAndEnvelope'
for i in range(3):o=box('loose paper sheet',(i*.006,i*.005,.001+i*.001),(.20,.13,.001),paper,g,0);o.rotation_euler.z=math.radians(i*9)
for j in range(7):box('printed text line',(-.025,-.05+j*.013,.004),(.105,.0015,.0003),grey,g,0)
finish(g,.05,3,scale=1.2);place(g,-2770,-907,48,8,'Loose papers')
g='KeysAndSunglasses';tube('key ring',[(.011*math.cos(i*math.tau/18),.011*math.sin(i*math.tau/18),.005)for i in range(18)],.0018,silver,g,6,True)
for i in range(3):o=box('key',(.020+i*.008,-.012+i*.006,.004),(.034,.008,.003),silver,g,.001);o.rotation_euler.z=i*.45
for x in [-.019,.019]:ell('sunglasses lens',(x,.075,.006),(.016,.011,.003),black,g)
tube('glasses bridge',[(-.004,.074,.007),(.004,.074,.007)],.0015,metal,g);tube('folded glasses arm',[(-.035,.072,.006),(-.019,.035,.008),(.021,.038,.008)],.0015,metal,g)
finish(g,.18,4,scale=1.2);place(g,-2600,-865,48,-18,'Keys and sunglasses')
# Clothing with folded cloth silhouettes, on sofa and floor.
for i,c in enumerate(clothcolors[:6]):
 g='LooseShirt'+str(i);cloth(g,'rumpled shirt body',(0,0,.020),.37,.46,c,.024,4,phase=i);cloth(g,'creased shirt sleeve',(.20,.11,.018),.17,.19,c,.020,3,phase=i);cloth(g,'creased shirt sleeve',(-.20,.11,.017),.17,.19,c,.018,3,phase=i+1);finish(g,.45,5,scale=1.45)
for i,(x,y,z)in enumerate([(-2832,-707,46),(-2808,-695,48),(-2778,-712,45),(-2630,-710,45),(-2555,-720,46),(-2468,-798,1),(-2544,-1064,0),(-2821,-965,0)]):place('LooseShirt'+str(i%6),x,y,z,i*37,'Loose clothing')
# Rounded perforated hamper, lined rubbish bin, footwear and cardboard.
g='LaundryBasket';box('hamper inner body',(0,0,.24),(.37,.27,.48),white,g,.05,6)
for y in [-.14,.14]:
 for i in range(8):
  for j in range(6):ell('small ventilation opening',(-.143+i*.041,y,.073+j*.057),(.007,.003,.015),grey,g)
box('curved hamper lid',(0,0,.49),(.41,.30,.04),white,g,.02,5);box('lid handle',(0,-.012,.514),(.10,.038,.004),grey,g,.011)
finish(g,2.5,20,damage=True,scale=1.5);place(g,-2510,-646,0,0,'White laundry hamper')
g='TrashBin';box('lined trash can',(0,0,.24),(.32,.30,.48),white,g,.038,5);box('dark open bin top',(0,0,.481),(.265,.245,.005),black,g,.02)
for i in range(6):cloth(g,'crumpled paper rubbish',(rng.uniform(-.1,.1),rng.uniform(-.08,.08),.48+rng.random()*.06),.16,.12,paper if i%2 else cardboard,.025,4,phase=i)
finish(g,3,20,damage=True,scale=1.5);place(g,-2610,-1325,0,0,'Overflowing trash can')
g='FlipFlop';o=ell('shaped flip flop sole',(0,0,.012),(.12,.043,.012),rubber,g);tube('rubber thong strap',[(-.042,-.036,.022),(.019,0,.055),(-.042,.036,.022)],.008,black,g,10);finish(g,.22,5,scale=1.35)
for x,y,a in [(-2515,-1170,-35),(-2500,-1125,-20),(-2580,-995,45),(-2604,-991,65)]:place(g,x,y,1,a,'Flip flop')
g='Sneaker';ell('rubber sneaker sole',(0,0,.024),(.142,.058,.026),white,g);ell('fabric sneaker upper',(-.012,0,.063),(.124,.051,.049),grey,g);ell('dark shoe opening',(-.075,0,.098),(.036,.036,.009),black,g)
for i in range(6):tube('white shoelace',[(-.037+i*.014,-.033,.092),(-.03+i*.014,.029,.096)],.0018,white,g,6)
finish(g,.45,7,scale=1.3)
for x,y,a in [(-2805,-1000,25),(-2780,-979,-12),(-2397,-991,-45),(-2400,-946,-15),(-2402,-1025,-75)]:place(g,x,y,0,a,'Sneaker')
g='StorageBox';box('corrugated storage box',(0,0,.24),(.30,.32,.48),cardboard,g,.002);box('packing tape',(0,-.161,.24),(.034,.001,.48),beige,g,0);flat('packing label',(0,-.162,.26),.16,.08,paper,g,True);text('box label','STORAGE',(0,-.164,.25),.018,black,g);finish(g,4,18,damage=True,scale=1.6)
for x,y,z in [(-2394,-1057,0),(-2393,-1057,48),(-2390,-1010,0)]:place(g,x,y,z,90,'Closet storage box')
# Closet fabric drawers and hanging shirts with real shoulders and wire hangers.
g='ClosetDrawerUnit';box('drawer cabinet frame',(0,0,.46),(.83,.40,.92),metal,g,.006)
for row in range(4):
 for col in range(2):
  x=-.207+col*.414;z=.12+row*.223;box('wood effect fabric drawer front',(x,-.21,z),(.391,.020,.205),wood,g,.005);tube('curved drawer pull',[(x-.048,-.225,z+.006),(x-.03,-.24,z-.008),(x+.03,-.24,z-.008),(x+.048,-.225,z+.006)],.004,black,g)
finish(g,17,45,damage=True);place(g,-2398,-752,0,-90,'Closet drawer organizer')
for i,c in enumerate(clothcolors):
 g='HangingShirt'+str(i);outline=[(-.20,.00),(-.32,-.10),(-.29,-.28),(-.19,-.23),(-.18,-.66),(.18,-.66),(.19,-.23),(.29,-.28),(.32,-.10),(.20,0),(.07,.055),(-.07,.055)]
 # Looped fabric front and back with small folds, rotating as one garment.
 vs=[]
 for side in [-1,1]:
  for x,z in outline:vs.append((x,side*.020+.012*math.sin(x*35+z*11+i),z+.72))
 N=len(outline);fs=[tuple(range(N-1,-1,-1)),tuple(range(N,2*N))]+[(k,(k+1)%N,(k+1)%N+N,k+N)for k in range(N)]
 mesh('hanging shirt with sleeves',vs,fs,c,g,True)
 for x in [-.12,-.06,0,.06,.12]:tube('fabric drape crease',[(x,-.029,.08),(x+.01,-.033,.31),(x,-.029,.55)],.003,c,g,5)
 tube('wire clothes hanger',[(-.235,0,.72),(0,0,.83),(.235,0,.72),(-.235,0,.72)],.0024,silver,g,6)
 tube('hanger hook',[(0,0,.83),(0,0,.90),(.02,0,.918),(.035,0,.90)],.003,metal,g,6)
 finish(g,.6,6,wallmount=True,scale=1.45)
for i in range(13):place('HangingShirt'+str(i%8),-2398,-835+i*12.0,88,90,'Hanging closet shirt')
for i in range(6):place('HangingShirt'+str([2,1,5,5,2,1][i]),-2397,-1094+i*17,88,90,'Hanging closet jacket')
# Wall art sampled directly from the user's photographs.
g='FlavortownBanner';photo_panel(g,2329,[(.274,.207),(.740,.294),(.740,.621),(.311,.779)],1.24,.90,True);finish(g,1,9,wallmount=True,scale=1.8);place(g,-2697,-590,164,180,'Flavortown banner')
g='PulpFictionPoster';photo_panel(g,2327,[(.598,.301),(.700,.270),(.704,.371),(.601,.386)],.77,.53);finish(g,.6,6,wallmount=True,scale=1.65);place(g,-2967,-1037,213,90,'Pulp Fiction poster')
g='ActionBronsonPoster';photo_panel(g,2328,[(.468,.317),(.548,.315),(.548,.408),(.468,.408)],.27,.42);finish(g,.3,5,wallmount=True,scale=1.45);place(g,-2448,-899,163,-90,'Action Bronson poster between closets')
g='SmallAlbumPrint';photo_panel(g,2328,[(.833,.161),(.894,.166),(.892,.213),(.831,.209)],.19,.19);finish(g,.15,4,wallmount=True,scale=1.45);place(g,-2448,-667,238,-90,'Small album print')
g='FramedPrint';photo_panel(g,2327,[(.121,.469),(.228,.449),(.24,.519),(.139,.546)],.61,.39)
for x in [-.324,.324]:box('black picture frame',(x,-.004,0),(.035,.019,.435),black,g,.003)
for z in [-.212,.212]:box('black picture frame',(0,-.004,z),(.68,.019,.036),black,g,.003)
finish(g,2,15,damage=True,wallmount=True,scale=1.5);place(g,-2503,-1368,158,0,'Framed wall print')
g='EmptyTVBracket'
for z in [-.10,.10]:box('perforated television wall rail',(0,0,z),(.70,.024,.025),metal,g,.003)
for x in [-.30,0,.30]:box('wall mount upright',(x,-.01,0),(.032,.025,.23),metal,g,.003)
for x in [-.32,.32]:
 for z in [-.10,.10]:bolt(g,(x,-.016,z))
finish(g,2.5,20,wallmount=True,scale=1.5);place(g,-2503,-1367,211,0,'Empty television wall bracket')
g='RopeShelf';box('floating dark shelf',(0,-.075,0),(.46,.15,.018),wood,g,.003)
for x in [-.20,.20]:tube('triangular cord bracket',[(x,-.15,0),(x,0,.21),(x,0,0)],.002,black,g,6)
finish(g,1.8,15,damage=True,wallmount=True,scale=1.5);place(g,-2850,-1367,230,0,'Rope wall shelf');place(g,-2743,-1367,230,0,'Rope wall shelf')
# Striped woven rug using geometry bands, without an additional raster texture.
g='StripedRug';box('woven rug backing',(0,0,.006),(2.07,1.46,.012),mat('RugIvory',(.49,.48,.43),1),g,.012)
for i in range(78):box('woven charcoal stripe',(-1.01+i*.026,0,.013),(.007 if i%3 else .012,1.43,.001),grey,g,0)
finish(g,interactive=False,collision=False);place(g,-2697,-878,.25,0,'Striped rug')
# Architectural kit: reference room shell, two proper open recesses, window trim and blind slats.
g='RoomShell'
def wb(n,lo,hi,m):box(n,tuple((a+b)/2/100 for a,b in zip(lo,hi)),tuple((b-a)/100 for a,b in zip(lo,hi)),m,g,.002)
XW,XE,YS,YN=-2972,-2355,-1371.8,-587;ZF,ZC=302.3,566.1;CF=-2445
# Entire shell is local to the house origin, already cm -> metres; scale is 1 here.
wb('bedroom floor',(XW-10,YS-10,ZF-10),(XE+10,YN+10,ZF),oak)
wb('ceiling',(XW-10,YS-10,ZC),(XE+10,YN+10,ZC+10),ceiling)
wb('far wall',(XW-10,YS-10,ZF),(XW,YN+10,ZC),wall);wb('left wall',(XW-10,YS-10,ZF),(XE+10,YS,ZC),wall);wb('right wall',(XW-10,YN,ZF),(XE+10,YN+10,ZC),wall)
wb('entry wall south',(XE,YS,ZF),(XE+10,-1342,ZC),wall);wb('entry wall north',(XE,-1148,ZF),(XE+10,YN,ZC),wall);wb('door header',(XE,-1342,ZF+216),(XE+10,-1148,ZC),wall)
for y0,y1 in [(-1140,-1120),(-945,-855),(-680,-657)]:wb('closet pier',(CF,y0,ZF),(XE,y1,ZC),wall)
wb('closet header',(CF,-1140,ZF+205),(XE,-657,ZC),wall)
for a,b in [(-1120,-945),(-855,-680)]:
 for y0,y1 in [(a-7,a),(b,b+7)]:wb('white closet casing',(CF-3,y0,ZF),(CF+1,y1,ZF+213),white)
 wb('white casing head',(CF-3,a-7,ZF+205),(CF+1,b+7,ZF+213),white);wb('closet top shelf',(CF+5,a,ZF+181),(XE-1,b,ZF+184),white)
 tube('closet hanging rod',[((XE-40)/100,a/100,(ZF+176)/100),((XE-40)/100,b/100,(ZF+176)/100)],.010,metal,g,12)
for lo,hi in [((XW,YS,ZF),(XW+1.5,YN,ZF+9)),((XW,YS,ZF),(XE,YS+1.5,ZF+9)),((XW,YN-1.5,ZF),(XE,YN,ZF+9)),((CF-1.5,-945,ZF),(CF,-855,ZF+9)),((CF-1.5,-680,ZF),(CF,-657,ZF+9))]:wb('charcoal skirting',lo,hi,black)
finish(g,interactive=False,collision=True,scale=1);doc['placements'].append(dict(asset=g,location=[0,0,0],yaw=0,name='Boy Wick room architecture'))
g='WindowBlinds'
box('dark window recess',(0,.014,.83),(.82,.018,1.64),black,g,.001)
for x in [-.44,.44]:box('painted window casing',(x,0,.83),(.075,.04,1.80),white,g,.004)
for z in [-.027,1.688]:box('window casing horizontal',(0,-.01,z),(.95,.046,.069),white,g,.004)
box('projecting sill',(0,-.045,-.045),(1.00,.12,.035),white,g,.005)
for i in range(62):
 z=.032+i*.0258;box('white venetian blind slat',(0,-.023,z),(.80,.025,.013),white,g,.006,3)
for x in [-.27,.27]:tube('blind ladder cord',[(x,-.041,.017),(x,-.041,1.62)],.0015,beige,g,6)
finish(g,interactive=False,collision=False,scale=1.55);place(g,-2968,-692,57,90,'Window and closed blinds')
g='CeilingDetails';cyl('round light rim',(0,0,0),.25,.02,white,g,40);ell('frosted glass light dome',(0,0,-.015),(.235,.235,.065),light,g);finish(g,interactive=False,collision=False,scale=1);place(g,-2683,-991,260,0,'Ceiling light')
g='CeilingVent';box('square vent flange',(0,0,0),(.44,.44,.018),white,g,.004)
for i in range(5):
 d=.34-i*.052
 for x in [-d/2,d/2]:box('louver',(x,0,-.020-i*.001),(.016,d,.012),white,g,.003)
 for y in [-d/2,d/2]:box('louver',(0,y,-.020-i*.001),(d,.016,.012),white,g,.003)
finish(g,interactive=False,collision=False,scale=1);place(g,-2790,-1100,262,0,'Square ceiling diffuser');place(g,-2480,-1200,262,0,'Entrance air return')
g='SmokeAlarm';cyl('smoke alarm body',(0,0,0),.068,.027,white,g,32,'Y');cyl('test button',(0,-.018,0),.013,.008,white,g,16,'Y')
for i in range(10):a=i*math.tau/10;box('alarm ventilation slot',(.048*math.cos(a),-.016,.048*math.sin(a)),(.008,.002,.004),grey,g,.001)
finish(g,interactive=False,collision=False,scale=1);place(g,-2810,-590,230,180,'Smoke detector')
# Additional small closet details: a leaning broom, tall flattened cartons and wall hooks.
g='ClosetBroom';tube('metal broom handle',[(0,0,.13),(.19,.02,1.41)],.008,silver,g,12);box('black broom head',(0,0,.085),(.27,.045,.04),black,g,.015)
for i in range(23):tube('broom bristles',[(-.125+i*.011,-.013,.075),(-.125+i*.011,-.019,.02)],.0025,grey,g,5)
finish(g,.9,10,scale=1.35);place(g,-2394,-1036,0,90,'Closet broom')
g='LongCarton';box('flat packed carton',(0,0,.58),(.25,.09,1.16),cardboard,g,.001);flat('white carton label',(0,-.046,.62),.18,.90,paper,g,True);text('carton lettering','STORAGE',(0,-.048,.62),.025,black,g);finish(g,3,18,scale=1.45);place(g,-2381,-998,0,90,'Tall carton')
# Reuse the narrow, detailed chair already built for the kitchen.
doc['external']=[dict(asset='/Game/SeniorSendOff/House/ReferenceKitchen/SM_Kitchen_GreenSpindleChair',location=[-2793,-1004,302.3],yaw=-90,name='Green desk chair',mass=7)]
(R/'manifest.json').write_text(json.dumps(doc,indent=2));print('BOYWICK_ROOM_KIT_READY',len(doc['assets']),len(doc['placements']),flush=True)
