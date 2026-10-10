"""Reference library kit. Dimensions are photo estimates, in real metres.
Horizontal dimensions are baked at the house's 1.8 scale; heights stay 1.0.
Props have individual rigid bodies, fitted hulls and independent damage meshes.
"""
import bpy,bmesh,math,json,random,runpy,collections
from pathlib import Path
from mathutils import Vector,Matrix
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library')
D=R/'models';D.mkdir(exist_ok=True)
DEST='/Game/SeniorSendOff/House/BasementLibrary'
DM=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/generate_damage_v2.py')
S=DM['S'];G=collections.defaultdict(list);M={};manifest={'assets':[],'placements':[],'materials':{},'scale':[1.8,1.8,1],'floor':-329.06,'notes':['Photo dimensions inferred; existing room shell footprint preserved.','Front of local kit is Blender -Y, UE +Y.','All ten unique reference photos reviewed.']}
bpy.ops.wm.read_factory_settings(use_empty=True)
rng=random.Random(2266)
def mat(k,c,rough=.7,metal=0,tex=None,mask=None,emission=0):
 n='M_Lib_'+k;m=bpy.data.materials.new(n);m.diffuse_color=(*c,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 d=dict(base_color=list(c),roughness=rough,metallic=metal)
 if tex:
  f=Path(tex) if str(tex).startswith('/') else R/'textures'/(tex+'.png');d['texture']=str(f)
  t=m.node_tree.nodes.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(f));m.node_tree.links.new(t.outputs['Color'],p.inputs['Base Color'])
  rf=f.with_name(f.stem+'_Roughness.png')
  if rf.exists():d['roughness_texture']=str(rf)
 if mask:d['opacity_mask']=str(R/'textures'/(mask+'.png'))
 if emission:p.inputs['Emission Color'].default_value=(*c,1);p.inputs['Emission Strength'].default_value=emission;d['emission']=emission
 M[k]=m;manifest['materials'][n]=d;return m
wood=mat('WalnutWood',(1,1,1),.53,tex='Walnut');oak=mat('OakWood',(1,1,1),.55,tex='Oak');leather=mat('BlackLeather',(1,1,1),.4,tex='Leather');fabric=mat('GrayFabric',(1,1,1),.94,tex='GrayFabric')
black=mat('BlackMetal',(.013,.015,.018),.59,.55);plastic=mat('BlackPlastic',(.008,.009,.01),.44);casepaint=mat('BlackCaseWood',(.012,.013,.014),.71);silver=mat('SilverMetal',(.44,.47,.49),.28,.9);rubber=mat('Rubber',(.009,.01,.01),.87);white=mat('WhitePlastic',(.67,.68,.66),.52);ivory=mat('IvoryPaint',(.55,.48,.30),.68)
paper=mat('Paper',(.83,.82,.77),.94);notes=mat('NotesPaper',(1,1,1),.94,tex='PaperNotes');meshmat=mat('OfficeMeshFabric',(.025,.027,.029),.94,mask='OfficeMeshMask');rug=mat('DamaskFabric',(1,1,1),.97,tex='DamaskRug');greencloth=mat('GreenCardFabric',(1,1,1),.96,tex='GreenCardCloth');floor=mat('WornConcrete',(1,1,1),.93,tex='Concrete')
linen=mat('LinenFabric',(.63,.59,.48),.95);blue=mat('UmbrellaBlueFabric',(.013,.10,.36),.8);graycloth=mat('GrayClothesFabric',(.1,.115,.13),.97);soil=mat('PlantSoil',(.045,.028,.013),1);leaf=mat('Leaves',(.049,.10,.022),.85);leaflight=mat('LightLeaves',(.11,.2,.044),.89);screen=mat('GlassScreen',(.005,.007,.009),.15,.1);gold=mat('BrassMetal',(.35,.22,.08),.35,.8);brown=mat('BrownDoorWood',(.14,.078,.041),.6);red=mat('Red',(.46,.018,.014),.62);warm=mat('WarmLight',(.8,.7,.46),.5,emission=3);matdesk=mat('PrintedDeskMat',(1,1,1),.93,tex='DeskMat');cardboard=mat('CardboardWood',(.36,.24,.11),.95)
chipcolors=[mat('Chip'+s,c,.42)for s,c in [('White',(.8,.79,.73)),('Red',(.5,.02,.025)),('Blue',(.015,.035,.19)),('Green',(.018,.14,.055))]]
bookmats=[mat('BookPaper%02d'%i,c,.78)for i,c in enumerate([(.12,.019,.018),(.008,.025,.08),(.2,.34,.06),(.55,.38,.025),(.018,.022,.025),(.48,.43,.31),(.38,.19,.04),(.09,.07,.02),(.36,.018,.034),(.008,.1,.08)])]
def mesh(n,v,f,m,g,smooth=False):
 me=bpy.data.meshes.new(n);me.from_pydata(v,[],f);me.update();o=bpy.data.objects.new(n,me);bpy.context.collection.objects.link(o)
 if m:me.materials.append(m)
 for p in me.polygons:p.use_smooth=smooth
 if g:G[g].append(o)
 return o
def box(n,c,s,m,g,bevel=.006,segments=3):
 x,y,z=[a/2 for a in s];o=mesh(n,[(-x,-y,-z),(x,-y,-z),(x,y,-z),(-x,y,-z),(-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z)],[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],m,g);o.location=c
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 if bevel:
  b=o.modifiers.new('Rounded joinery','BEVEL');b.width=min(bevel,min(s)*.46);b.segments=segments;bpy.ops.object.modifier_apply(modifier=b.name)
  for p in o.data.polygons:p.use_smooth=True
  try:
   b=o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL');b.keep_sharp=True;bpy.ops.object.modifier_apply(modifier=b.name)
  except RuntimeError:pass
 o.select_set(False);return o
def cyl(n,c,r,h,m,g,vertices=24,axis='Z'):
 bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=h,location=c);o=bpy.context.object;o.name=n
 if axis=='Y':o.rotation_euler.x=math.pi/2
 if axis=='X':o.rotation_euler.y=math.pi/2
 o.data.materials.append(m)
 for p in o.data.polygons:p.use_smooth=len(p.vertices)==4
 G[g].append(o);return o
def ell(n,c,s,m,g):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=20,ring_count=12,location=c);o=bpy.context.object;o.name=n;o.scale=s;o.data.materials.append(m)
 for p in o.data.polygons:p.use_smooth=True
 G[g].append(o);return o
def tube(n,pts,r,m,g,sides=7,closed=False):
 pts=[Vector(p)for p in pts];v=[];f=[];N=len(pts)
 for i,p in enumerate(pts):
  t=(pts[(i+1)%N]if closed or i<N-1 else pts[i])-(pts[(i-1)%N]if closed or i else pts[i]);t.normalize();ref=Vector((0,0,1))if abs(t.z)<.92 else Vector((0,1,0));u=t.cross(ref).normalized();w=t.cross(u)
  for j in range(sides):a=j*math.tau/sides;v.append(tuple(p+r*(math.cos(a)*u+math.sin(a)*w)))
 for i in range(N if closed else N-1):
  for j in range(sides):f.append((i*sides+j,i*sides+(j+1)%sides,((i+1)%N)*sides+(j+1)%sides,((i+1)%N)*sides+j))
 if not closed:f += [tuple(range(sides-1,-1,-1)),tuple((N-1)*sides+j for j in range(sides))]
 return mesh(n,v,f,m,g,True)
def text(n,s,c,size,m,g,rotation=(math.pi/2,0,0),align='CENTER'):
 cu=bpy.data.curves.new(n,'FONT');cu.body=s;cu.align_x=align;cu.size=size;cu.extrude=.0003;cu.resolution_u=2;o=bpy.data.objects.new(n,cu);bpy.context.collection.objects.link(o);o.location=c;o.rotation_euler=rotation;cu.materials.append(m)
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target='MESH');o=bpy.context.object;o.select_set(False);G[g].append(o);return o
def flat(n,c,w,h,m,g,vertical=False,uv=None):
 x,y,z=c;v=[(x-w/2,y,z-h/2),(x+w/2,y,z-h/2),(x+w/2,y,z+h/2),(x-w/2,y,z+h/2)]if vertical else [(x-w/2,y-h/2,z),(x+w/2,y-h/2,z),(x+w/2,y+h/2,z),(x-w/2,y+h/2,z)]
 o=mesh(n,v,[(0,1,2,3)],m,g);layer=o.data.uv_layers.new(name='UVMap')
 for i,p in enumerate(uv or [(0,0),(1,0),(1,1),(0,1)]):layer.data[i].uv=p
 return o
def uv_auto(o):
 if o.data.uv_layers:return
 layer=o.data.uv_layers.new(name='UVMap')
 for p in o.data.polygons:
  ax=max(range(3),key=lambda k:abs(p.normal[k]));axes=[k for k in range(3)if k!=ax]
  for li in p.loop_indices:
   q=o.data.vertices[o.data.loops[li].vertex_index].co;layer.data[li].uv=(q[axes[0]]*1.4,q[axes[1]]*1.4)
def finalize(g,mass=12,health=24,movable=True,wall=False,damage=True):
 objects=G[g]
 for o in objects:
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);uv_auto(o)
  for v in o.data.vertices:v.co.x*=1.8;v.co.y*=1.8
 mats=[]
 for o in objects:
  for m in o.data.materials:
   if m not in mats:mats.append(m)
 for o in objects:
  old=list(o.data.materials);ix=[mats.index(old[p.material_index])for p in o.data.polygons];o.data.materials.clear()
  for m in mats:o.data.materials.append(m)
  for p,i in zip(o.data.polygons,ix):p.material_index=i
 name='SM_Lib_'+g;ob=DM['join_meshes'](objects,name,mats);hulls=DM['hulls_for'](objects,name) if damage else []
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True)
 for h in hulls:h.select_set(True)
 bpy.context.view_layer.objects.active=ob;file=D/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
 lo,hi=DM['bbox'](ob)
 row=dict(id=g,asset_name=name,intact_fbx=str(file),material_slots=[DEST+'/'+m.name+'.'+m.name for m in mats],material_names=[m.name for m in mats],bounds_blender_metres=[list(lo),list(hi)],behavior='furniture' if damage else 'architecture',mass_kg=mass,break_health=health,movable=movable,wall=wall,damage=damage,hulls=len(hulls),vertices=len(ob.data.vertices),faces=len(ob.data.polygons))
 manifest['assets'].append(row)
 for o in [ob,*hulls,*objects]:DM['discard'](o)
 G.pop(g,None);return row
def place(g,u,v,h=0,yaw=0,name=None):
 manifest['placements'].append(dict(asset=g,name=name or g,location=[round(-3004.2+u*180,3),round(773.1-v*180,3),round(-329.06+h*100,3)],yaw=yaw))
def swivel(g,gray=False):
 for i in range(5):
  a=i*math.tau/5;dx=math.sin(a);dy=math.cos(a);tube('swept caster spoke',[(0,0,.19),(dx*.15,dy*.15,.13),(dx*.31,dy*.31,.075)],.022,plastic,g)
  for off in [-.018,.018]:cyl('dual caster', (dx*.31+off,dy*.31,.046),.041,.026,rubber,g,axis='X',vertices=16)
 cyl('gas cylinder',(0,0,.30),.033,.31,silver,g);cyl('gas shroud',(0,0,.25),.046,.20,plastic,g)
 box('soft seat',(0,-.025,.48),(.51,.48,.10),fabric if gray else plastic,g,.045,5)
 if gray:
  o=ell('rounded fabric back',(0,.22,.72),(.255,.047,.29),fabric,g);o.rotation_euler.x=-.1
 else:
  nx,ny=14,18;vv=[];ff=[]
  for j in range(ny+1):
   t=j/ny;w=.215+.025*math.sin(t*math.pi)
   for i in range(nx+1):xx=(i/nx-.5)*w*2;vv.append((xx,.22+.04*math.sin(t*math.pi)+xx*xx*.3,.54+.46*t))
  for j in range(ny):
   for i in range(nx):k=j*(nx+1)+i;ff.append((k,k+1,k+nx+2,k+nx+1))
  o=mesh('woven curved mesh',vv,ff,meshmat,g,True);uv=o.data.uv_layers.new(name='UVMap')
  for p in o.data.polygons:
   for li in p.loop_indices:q=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(q.x*19,q.z*19)
  edge=[(-.215,.22,.54),(-.239,.255,.66),(-.235,.25,.94),(-.207,.22,1.0),(.207,.22,1.0),(.235,.25,.94),(.239,.255,.66),(.215,.22,.54)]
  tube('continuous back frame',edge,.018,plastic,g,closed=True)
  tube('lumbar support',[(-.21,.262,.66),(0,.286,.64),(.21,.262,.66)],.022,plastic,g)
 tube('back support',[(0,.20,.43),(0,.30,.51),(0,.28,.68)],.022,black,g)
 for s in [-1,1]:
  tube('curved arm support',[(s*.20,.12,.43),(s*.30,.14,.64),(s*.29,-.23,.64),(s*.21,-.18,.43)],.015,plastic,g)
  box('arm rest',(s*.29,-.04,.67),(.065,.32,.028),plastic,g,.013)
 finalize(g,14,28)
swivel('MeshOfficeChair');swivel('GrayOfficeChair',True)
def tuft(g,w,depth,club=False):
 for x in [-w/2+.08,w/2-.08]:
  for y in [-depth/2+.09,depth/2-.09]:box('tapered walnut foot',(x,y,.10),(.065,.065,.20),wood,g,.012)
 box('leather base',(0,0,.275),(w-.06,depth-.02,.22),leather,g,.055,5)
 box('seat cushion',(0,-.065,.445),(w-.21,depth-.17,.15),leather,g,.058,6)
 for x in [-w/2+.055,w/2-.055]:box('padded arm',(x,-.015,.48),(.13,depth,.43),leather,g,.054,6)
 ww=w-.20;nx=56;ny=20;vs=[];fs=[];cols=4 if club else 7
 for j in range(ny+1):
  z=.48+j/ny*.40
  for i in range(nx+1):
   x=(i/nx-.5)*ww;ind=sum(.027*math.exp(-(((x-((a+.5)/cols-.5)*ww)/.044)**2+((z-(.58+b*.18))/.045)**2))for a in range(cols)for b in range(2))
   vs.append((x,depth*.32+ind,z))
 for j in range(ny):
  for i in range(nx):k=j*(nx+1)+i;fs.append((k,k+nx+1,k+nx+2,k+1))
 o=mesh('button tufted back',vs,fs,leather,g,True);b=o.modifiers.new('leather cushion thickness','SOLIDIFY');b.thickness=.065;bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.modifier_apply(modifier=b.name);o.select_set(False)
 for a in range(cols):
  for b in range(2):ell('deep leather button',(((a+.5)/cols-.5)*ww,depth*.32+.024,.58+b*.18),(.010,.006,.010),leather,g)
 tube('seat rolled piping',[(-ww/2,-depth*.4,.45),(ww/2,-depth*.4,.45),(ww/2,depth*.18,.45),(-ww/2,depth*.18,.45)],.0035,plastic,g,closed=True)
 finalize(g,85 if not club else 48,85 if not club else 55)
tuft('LeatherLoveseat',1.76,.79);tuft('LeatherClubChair',.86,.83,True)
for g in ['GrayArmchair']:
 for x in [-.32,.32]:
  for y in [-.30,.30]:box('dark foot',(x,y,.095),(.07,.07,.19),wood,g,.01)
 box('cloth base',(0,0,.27),(.81,.82,.28),fabric,g,.045,5)
 box('seat',(0,-.04,.45),(.61,.62,.15),fabric,g,.057,6)
 for x in [-.355,.355]:box('wide fabric arm',(x,0,.49),(.15,.82,.42),fabric,g,.062,5)
 o=box('fabric back',(0,.30,.72),(.66,.15,.47),fabric,g,.065,6);o.rotation_euler.x=-.09
 for x in [-.3,.3]:tube('cloth seam',[(x,-.33,.47),(x,.21,.47),(x,.22,.86)],.0025,linen,g)
 finalize(g,45,55)
for g,m,size in [('DarkThrowPillow',graycloth,(.48,.40,.12)),('TanThrowPillow',fabric,(.50,.46,.15)),('OrangeThrowPillow',graycloth,(.48,.44,.12))]:
 box('soft pillow',(0,0,size[2]/2),size,m,g,min(size)*.43,6)
 tube('pillow piping',[(size[0]/2*math.cos(a),size[1]/2*math.sin(a),size[2]/2)for a in [i*math.tau/36 for i in range(36)]],.0022,m,g,closed=True)
 if g=='OrangeThrowPillow':
  cyl('orange medallion',(0,0,.124),.14,.001,chipcolors[1],g,vertices=40);text('orange badge','T',(0,-.06,.126),.14,paper,g,rotation=(0,0,0))
 finalize(g,2,12)
def shelf(g):
 for x in [-.36,.36]:box('case side',(x,0,.945),(.022,.30,1.89),casepaint,g,.003)
 box('case back',(0,.14,.945),(.72,.013,1.89),casepaint,g,.001)
 for z in [.012,.31,.61,.91,1.22,1.55,1.879]:box('shelf',(0,0,z),(.74,.30,.021),casepaint,g,.003)
 finalize(g,85,85)
shelf('SixShelfBookcase')
titles=['FREEDOM','NEW STORIES','HISTORY','THE MIND','PHILOSOPHY','COLLECTION','LEVIATHAN','WORLD ATLAS','JOURNAL','TRAILBLAZERS','PLATO','THE GAME']
for variant in range(5):
 g='BookShelfContents%d'%variant;x=-.31
 for k in range(7+variant):
  w=.027+(k%3)*.012;h=.19+(k%4)*.016;m=bookmats[(k+variant*3)%len(bookmats)];c=(x+w/2,.005,h/2)
  box('paper block',c,(w-.006,.17,h-.012),paper,g,.001)
  for xx in [x+.001,x+w-.001]:box('book cover',(xx,.004,h/2),(.003,.18,h),m,g,.0008)
  box('printed spine',(x+w/2,-.085,h/2),(w,.008,h),m,g,.001)
  if w>.03:text('book spine title',titles[(k+variant)%len(titles)],(x+w/2,-.090,.043),.006,linen,g,rotation=(math.pi/2,0,math.pi/2))
  x+=w+.004
  if x>.23:break
 finalize(g,5,18)
g='StackedBooks'
for k in range(6):
 z=.020+k*.031;box('cream pages',(0,0,z),(.26,.19,.025),paper,g,.001)
 for zz in [z-.014,z+.014]:box('hard cover',(0,0,zz),(.27,.20,.003),bookmats[k],g,.001)
 text('stacked titles',titles[(k+4)%12],(0,-.101,z-.004),.007,linen,g)
finalize(g,6,18)
g='CoffeeChest'
box('walnut chest',(0,0,.28),(1.08,.54,.27),wood,g,.018)
box('overhanging chest lid',(0,0,.437),(1.13,.59,.047),wood,g,.018)
for x in [-.47,.47]:
 for y in [-.20,.20]:box('chest leg',(x,y,.11),(.048,.048,.22),wood,g,.006)
for y in [-.268,.268]:tube('case horizontal seam',[(-.50,y,.23),(.50,y,.23)],.0025,black,g)
finalize(g,65,72)
def side(g,round=False,woodtop=False,h=.57):
 if round:cyl('round top',(0,0,h-.018),.25,.036,oak if woodtop else casepaint,g,vertices=40)
 else:box('square top',(0,0,h-.014),(.48,.43,.028),casepaint,g,.003)
 for x in [-.19,.19]:
  for y in [-.16,.16]:box('slender metal leg',(x,y,h/2),(.022,.022,h),black,g,.003)
 finalize(g,12,30)
side('BlackSideTable');side('RoundWoodSideTable',True,True);side('PlantRoundStand',True,False,.54)
g='DistressedCreamConsole'
box('walnut console top',(0,0,.845),(1.30,.43,.042),wood,g,.010)
for x in [-.57,.57]:
 for y in [-.155,.155]:box('cream leg',(x,y,.42),(.067,.067,.84),ivory,g,.004)
box('shelf below',(0,0,.12),(1.21,.38,.03),ivory,g,.005)
for x in [-.30,.30]:
 box('drawer',(x,-.17,.685),(.574,.052,.20),ivory,g,.008)
 for zz in [.617,.752]:box('drawer mold edge',(x,-.201,zz),(.51,.007,.011),ivory,g,.003)
 ell('black cup drawer pull',(x,-.215,.68),(.065,.024,.026),black,g)
for k in range(18):
 x=rng.uniform(-.6,.6);box('paint chipped edge',(x,-.203,.80),(.013,.001,.004),wood,g,.0002)
finalize(g,85,82)
g='TrestlePlankWorktable'
for i in range(4):box('plank top',(-.80+i*.54,0,.776),(.536,.82,.046),wood,g,.007)
for x in [-.80,.80]:
 for yy in [-.27,.27]:
  o=box('splayed pale wood trestle leg',(x,yy,.37),(.11,.075,.74),ivory,g,.006);o.rotation_euler.y=.14 if x<0 else -.14
 box('trestle beam',(x,0,.59),(.105,.70,.08),ivory,g,.006)
box('lower stretcher',(0,0,.23),(1.65,.045,.065),ivory,g,.004)
finalize(g,100,100)
g='RoundedOakStudyTable'
ww=1.86;dd=1.03;radius=.21;pts=[]
for cx,cy,st in [(ww/2-radius,dd/2-radius,0),(-ww/2+radius,dd/2-radius,90),(-ww/2+radius,-dd/2+radius,180),(ww/2-radius,-dd/2+radius,270)]:
 for k in range(9):a=math.radians(st+k*90/8);pts.append((cx+radius*math.cos(a),cy+radius*math.sin(a)))
vv=[(x,y,z)for z in [.743,.777]for x,y in pts];nn=len(pts);ff=[tuple(range(nn-1,-1,-1)),tuple(nn+i for i in range(nn))]+[(i,(i+1)%nn,(i+1)%nn+nn,i+nn)for i in range(nn)]
mesh('oval-ended oak desk',vv,ff,oak,g)
for x in [-.61,.61]:
 box('desk foot',(x,0,.034),(.07,.80,.068),black,g,.01);box('desk pedestal',(x,0,.38),(.07,.07,.70),black,g,.009);box('top brace',(x,0,.72),(.06,.77,.035),black,g,.004)
box('modesty panel',(0,.24,.54),(1.26,.028,.30),plastic,g,.004)
finalize(g,105,100)
g='OctagonalCardTable'
cyl('octagonal oak tabletop',(0,0,.732),.82,.06,oak,g,vertices=8)
cyl('raised patterned felt',(0,0,.770),.625,.018,greencloth,g,vertices=8)
# UV aligns the geometric octagon with an actual repeating club/diamond weave.
for a in range(8):
 t=a*math.tau/8+math.pi/8;x=math.cos(t)*.71;y=math.sin(t)*.71
 o=box('dark cup rail',(x,y,.775),(.285,.139,.033),wood,g,.006);o.rotation_euler.z=t+math.pi/2
 for off in [-.054,.054]:
  cyl('inset cup well',(x-math.sin(t)*off,y+math.cos(t)*off,.794),.032,.003,plastic,g,vertices=24)
for x in [-.43,.43]:
 for y in [-.43,.43]:box('game-table leg',(x,y,.35),(.045,.045,.70),wood,g,.005)
box('leg crossbar',(0,0,.18),(.91,.045,.035),wood,g,.003)
finalize(g,85,95)
g='WhitePlasticDrawerTower'
for x in [-.205,.205]:box('white upright',(x,0,.705),(.044,.46,1.41),white,g,.008)
for k in range(6):
 z=.12+k*.225;box('clear drawer',(0,-.008,z),(.38,.39,.18),mat('DrawerPlastic%d'%k,(.35,.38,.35),.47),g,.015)
 box('white drawer frame',(0,0,z+.10),(.44,.46,.043),white,g,.015)
 box('drawer pull',(0,-.222,z+.02),(.105,.018,.035),white,g,.012)
box('tower lid',(0,0,1.40),(.46,.48,.05),white,g,.025)
finalize(g,24,38)
g='BrownLeatherBarStool'
box('brown stool seat',(0,0,.745),(.46,.42,.052),brown,g,.034,5)
o=box('curved stool back',(0,.18,.92),(.45,.061,.25),brown,g,.058,6);o.rotation_euler.x=-.14
for x in [-.18,.18]:
 for y in [-.15,.15]:box('stool steel leg',(x,y,.37),(.018,.018,.74),black,g,.002)
tube('foot rail',[(-.18,-.15,.18),(.18,-.15,.18),(.18,.15,.18),(-.18,.15,.18)],.008,black,g,closed=True)
finalize(g,12,25)
g='CurvedWoodDeskChair'
box('wooden seat',(0,0,.48),(.46,.44,.035),wood,g,.015)
for x in [-.18,.18]:
 for y in [-.16,.16]:box('wood chair leg',(x,y,.245),(.036,.036,.49),oak,g,.005)
for x in [-.18,.18]:tube('swept back upright',[(x,.16,.45),(x,.21,.74),(x*.8,.25,1.0)],.020,oak,g)
tube('curved top crest',[(-.2,.25,.93),(-.17,.26,1.00),(0,.28,1.03),(.17,.26,1.00),(.2,.25,.93)],.031,wood,g)
for s in [-1,1]:tube('carved scroll back',[(s*.13,.245,.93),(s*.06,.24,.94),(0,.235,.83),(s*.13,.23,.75),(s*.09,.22,.70)],.018,wood,g)
finalize(g,14,34)
def lamp(g,floorlamp=False):
 h=1.55 if floorlamp else .48;r=.20 if floorlamp else .13
 cyl('lamp base',(0,0,.025),.14 if floorlamp else .10,.035,black,g,vertices=32);cyl('lamp stem',(0,0,h*.45),.014,h*.88,black,g)
 vs=[];fs=[];N=40
 for zz,rr in [(h-.25,r),(h,r*.72)]:vs.extend([(rr*math.cos(i*math.tau/N),rr*math.sin(i*math.tau/N),zz)for i in range(N)])
 for i in range(N):fs.append((i,(i+1)%N,(i+1)%N+N,i+N))
 mesh('woven lamp shade',vs,fs,linen,g,True);ell('warm bulb',(0,0,h-.12),(.032,.032,.045),warm,g)
 tube('lamp power cable',[(0,.03,.02),(.10,.12,.006),(.26,.21,.006)],.003,plastic,g)
 finalize(g,12 if floorlamp else 4,30)
lamp('FloorLamp',True);lamp('TableLamp')
g='TallGrassPlant'
cyl('basket pot',(0,0,.12),.15,.24,wood,g,vertices=32);cyl('soil',(0,0,.238),.141,.006,soil,g,vertices=32)
for k in range(72):
 a=rng.random()*math.tau;rad=rng.uniform(.02,.12);height=rng.uniform(.50,1.02);bend=rng.uniform(.06,.30)
 pts=[(math.cos(a)*(rad+bend*t*t),math.sin(a)*(rad+bend*t*t),.23+height*t)for t in [i/9 for i in range(10)]]
 tube('arching grass blade',pts,.0026,leaf if k%3 else leaflight,g,sides=4)
for z in [.03+i*.014 for i in range(15)]:tube('woven basket rope',[(.152*math.cos(a),.152*math.sin(a),z)for a in [i*math.tau/40 for i in range(40)]],.003,linen,g,closed=True)
finalize(g,5,22)
g='SmallPottedPlant'
cyl('white ceramic pot',(0,0,.055),.059,.11,white,g,vertices=24);cyl('soil',(0,0,.109),.05,.005,soil,g)
for k in range(24):
 a=k*2.4;h=.12+(k%5)*.018;xx=math.cos(a)*.07;yy=math.sin(a)*.07;tube('leaf stem',[(0,0,.105),(xx*.7,yy*.7,h)],.0017,leaf,g)
 o=ell('oval plant leaf',(xx,yy,h),(.032,.017,.009),leaflight if k%4 else leaf,g);o.rotation_euler.z=a
finalize(g,2,12)
for g,w,h in [('WallTelevision',1.22,.69),('DeskMonitor',.66,.40),('SmallMonitor',.52,.31)]:
 z=h/2;box('rear enclosure',(0,.025,z),(w,.065,h),plastic,g,.015)
 box('black bezel',(0,-.017,z),(w-.013,.016,h-.014),plastic,g,.008)
 flat('dark glass',(0,-.026,z),w-.038,h-.04,screen,g,True)
 if g!='WallTelevision':
  cyl('monitor stand',(0,.04,-.075),.028,.15,black,g);box('monitor foot',(0,0,-.145),(.30,.20,.016),black,g,.01)
  for k in range(13):box('back ventilation',(k*.020-.12,.059,z-.06),(.010,.004,.07),rubber,g,.001)
 else:ell('red standby LED',(0,-.026,.011),(.003,.002,.002),red,g)
 finalize(g,22 if g=='WallTelevision' else 8,42,wall=g=='WallTelevision')
g='DeskMouse';ell('computer mouse',(0,0,.022),(.033,.060,.022),plastic,g);tube('mouse cord',[(0,.05,.01),(.02,.14,.005),(.15,.24,.005)],.0018,plastic,g);finalize(g,1,12)
g='DeskOrganizer';box('organizer base',(0,0,.009),(.13,.17,.018),white,g,.001)
for x in [-.064,.064,0]:box('organizer divider',(x,0,.07),(.003,.17,.14),white,g,.0005)
for y in [-.083,.00,.083]:box('organizer wall',(0,y,.06),(.13,.003,.12),white,g,.0005)
finalize(g,1,12)
g='DeskRiser';box('walnut riser top',(0,0,.08),(.43,.29,.025),wood,g,.003)
for x in [-.19,.19]:box('riser side',(x,0,.04),(.025,.29,.08),wood,g,.003)
box('red notebook',(0,-.01,.101),(.32,.22,.016),red,g,.002);box('notebook pages',(0,-.01,.10),(.315,.213,.009),paper,g,.001);finalize(g,3,18)
g='PaperScatter'
for k in range(7):
 o=flat('loose study sheet',(rng.uniform(-.22,.22),rng.uniform(-.16,.16),.002+k*.001),.21,.29,notes,g);o.rotation_euler.z=rng.uniform(-.35,.35)
finalize(g,1,12)
for g,m in [('BlueMarker',blue),('BlackMarker',white),('DeskPen',plastic)]:
 rr=.006 if 'Marker'in g else .004;cyl('barrel',(0,0,rr),rr,.14,m,g,vertices=12,axis='Y');cyl('dark cap',(0,-.056,rr),rr*1.12,.035,plastic,g,vertices=12,axis='Y');finalize(g,1,12)
g='PaperCup';cyl('cup',(0,0,.06),.041,.12,paper,g,vertices=28);cyl('cup dark opening',(0,0,.121),.036,.001,soil,g,vertices=24)
for a in [i*math.tau/8 for i in range(8)]:ell('cup blue leaf',(.042*math.sin(a),.042*math.cos(a),.079),(.009,.003,.015),blue,g)
finalize(g,1,12)
g='OpenPokerChipCase'
box('ribbed silver lower case',(0,0,.026),(.54,.29,.052),silver,g,.012)
box('black felt lining',(0,0,.053),(.52,.267,.004),plastic,g,.001)
lid=box('open silver lid',(0,.145,.175),(.54,.025,.30),silver,g,.010);lid.rotation_euler.x=-.06
box('lid felt',(0,.129,.175),(.515,.004,.269),plastic,g,.001)
for x in [-.20,.20]:box('silver latch',(x,-.153,.043),(.04,.016,.022),silver,g,.004)
tube('case handle',[(-.05,-.166,.04),(-.05,-.178,.055),(.05,-.178,.055),(.05,-.166,.04)],.005,black,g)
for k in range(7):
 x=(k-3)*.066
 for j in range(12):
  o=cyl('striped casino chip',(x,-.105+j*.017,.073),.027,.014,chipcolors[k%4],g,vertices=16,axis='Y')
  for a in [0,math.pi/2,math.pi,math.pi*1.5]:box('white chip edge',(x+math.cos(a)*.027,-.105+j*.017,.073+math.sin(a)*.027),(.007,.013,.006),paper,g,.001)
finalize(g,9,32)
g='ClosedSilverCase';box('silver case',(0,0,.039),(.50,.29,.078),silver,g,.010)
for x in [-.18,.18]:box('latch',(x,-.15,.055),(.03,.012,.024),silver,g,.003)
finalize(g,7,25)
g='FoldedBlueWhiteUmbrella'
vs=[];fs=[];N=12
for j in range(13):
 t=j/12;rr=.064*(math.sin(t*math.pi*.6)**.9)+.003
 for i in range(N):a=i*math.tau/N;rad=rr*(1 if i%2 else .65);vs.append((rad*math.cos(a),-.43+t*.86,.070+rad*math.sin(a)))
for j in range(12):
 for i in range(N):k=j*N+i;fs.append((k,j*N+(i+1)%N,(j+1)*N+(i+1)%N,k+N))
o=mesh('folded canopy',vs,fs,blue,g,True);o.data.materials.append(paper)
for p in o.data.polygons:p.material_index=1 if p.index%12 in [2,3,8,9] else 0
tube('umbrella shaft',[(0,-.51,.07),(0,.49,.07)],.007,silver,g);tube('crooked grip',[(0,.49,.07),(0,.53,.07),(.03,.55,.07),(.06,.53,.07),(.06,.49,.07)],.016,plastic,g)
finalize(g,2,18)
g='GrayBackpack'
box('soft bag',(0,0,.135),(.39,.27,.27),graycloth,g,.080,6);box('front zipped pocket',(0,-.13,.11),(.30,.05,.16),graycloth,g,.046,5)
for x in [-.1,.1]:tube('slack shoulder strap',[(x,.1,.23),(x,.23,.12),(x,.12,.018)],.014,plastic,g)
tube('zipper', [(-.14,-.16,.19),(0,-.17,.20),(.14,-.16,.19)],.003,silver,g);finalize(g,5,24)
g='FrameRails';box('loose long frame rail',(0,0,.009),(1.15,.035,.018),casepaint,g,.003);box('second rail',(0,.05,.020),(1.02,.04,.019),casepaint,g,.003);finalize(g,3,20)
g='OpenCardboardBox'
box('box bottom',(0,0,.005),(.40,.31,.01),cardboard,g,.001)
for x in [-.20,.20]:box('cardboard side',(x,0,.15),(.009,.32,.30),cardboard,g,.001)
for y in [-.155,.155]:box('cardboard front',(0,y,.15),(.40,.009,.30),cardboard,g,.001)
o=box('splayed flap',(0,-.22,.26),(.40,.14,.005),cardboard,g,.001);o.rotation_euler.x=-.6
finalize(g,2,16)
g='PartyGameStack'
for k in range(6):
 z=.018+k*.036;box('game box',(0,0,z),(.20,.145,.032),plastic,g,.004);text('party game spine','PARTY PACK',(0,-.074,z-.004),.009,paper,g)
box('white game box',(.23,0,.03),(.18,.15,.06),paper,g,.002);text('game name','INCOHEARENT',(.23,-.077,.028),.012,plastic,g);finalize(g,3,20)
g='CardDeck';box('red playing-card box',(0,0,.01),(.065,.09,.02),paper,g,.002);text('card pack','CARDS',(0,-.045,.008),.011,red,g);finalize(g,1,12)
g='RedCoffeeBook';box('hardback red book',(0,0,.011),(.24,.17,.022),red,g,.002);text('cover title','THE WOODS',(0,0,.023),.021,paper,g,rotation=(0,0,0));finalize(g,2,16)
g='DeskMat';flat('printed desk pad',(0,0,.002),.85,.48,matdesk,g);finalize(g,1,12)
def photo_art(g,w,h,photo,coords):
 ma=mat(g+'Photo',(1,1,1),.93,tex=R/'references'/('IMG_'+str(photo)+'.png'))
 box('frame backing',(0,0,h/2),(w,.025,h),wood,g,.004)
 for x in [-w/2,w/2]:box('frame upright',(x,-.02,h/2),(.019,.022,h+.024),wood,g,.004)
 for z in [-.008,h+.008]:box('frame rail',(0,-.02,z),(w,.022,.019),wood,g,.004)
 uv=[(x/1500,1-y/1125)for x,y in coords]
 flat('reference photograph',(0,-.032,h/2),w-.025,h-.02,ma,g,True,uv)
 finalize(g,8 if w>.6 else 3,32,wall=True)
photo_art('LighthouseCanvas',.88,1.12,2267,[(389,451),(634,441),(592,101),(376,169)])
# The supplied HEIC views use landscape 1500x1125 for these wall photographs.
photo_art('FramedHistoricPhoto',.32,.40,2270,[(1218,344),(1263,333),(1255,255),(1218,273)])
photo_art('FramedGroupPhoto',.20,.15,2267,[(1012,905),(1040,912),(1048,864),(1022,856)])
g='GlassWhiteboard';box('clear whiteboard',(0,0,.43),(1.04,.016,.86),mat('WhiteboardGlass',(.37,.36,.30),.19),g,.005)
for x in [-.50,.50]:
 for z in [.025,.835]:cyl('standoff',(x,-.011,z),.013,.009,silver,g,vertices=16,axis='Y')
finalize(g,15,40,wall=True)
for g,sz in [('LongRug',(1.5,3.25)),('EntryRug',(1.8,2.45)),('StudyRug',(1.76,2.10))]:
 box('woven rug base',(0,0,.003),(sz[0],sz[1],.006),linen,g,.002);flat('damask woven surface',(0,0,.0065),*sz,rug,g)
 finalize(g,12,22)
g='LibraryFloor';box('worn concrete inset',(0,0,0),(3.442,12.12,.006),floor,g,.001);finalize(g,damage=False)
g='TVLooseCables'
for off in [0,.04,-.04]:tube('hanging TV cable',[(off,0,.65),(off,.06,.30),(off+.09,.12,.05),(off+.17,.08,0)],.0025,plastic,g)
finalize(g,damage=False)
g='CeilingHatchHardware'
# These overlays detail the three existing ceiling hatches without duplicate frames.
for x in [-.28,.28]:box('steel hatch hinge',(x,-.33,0),(.055,.022,.009),silver,g,.003)
box('recessed pull',(0,.31,-.009),(.039,.022,.016),ivory,g,.005)
for y in [-.385,.385]:box('dark recessed perimeter',(0,y,.002),(.61,.004,.001),black,g,.0004)
for x in [-.306,.306]:box('dark recessed perimeter',(x,0,.002),(.004,.77,.001),black,g,.0004)
finalize(g,damage=False)
g='StudyPendant'
for x in [-.23,.23]:
 for y in [-.12,.12]:tube('pendant corner',[(x,y,0),(x,y,.21)],.009,black,g)
for z in [0,.21]:tube('pendant perimeter',[(-.23,-.12,z),(.23,-.12,z),(.23,.12,z),(-.23,.12,z)],.009,black,g,closed=True)
for x in [-.15,0,.15]:
 tube('diamond lattice',[(x-.065,-.12,0),(x,-.12,.21),(x+.065,-.12,0)],.004,black,g);ell('pendant bulb',(x,0,.105),(.02,.02,.049),warm,g)
tube('ceiling hanger',[(0,0,.21),(0,0,.55)],.010,black,g);box('ceiling plate',(0,0,.55),(.14,.10,.022),black,g,.004)
finalize(g,18,40,movable=False)
# --- Placement from the photo sequence: entry game area, worktable, lounge, study.
place('LibraryFloor',1.721,6.06,-.003)
place('OctagonalCardTable',1.92,1.33)
for i,a in enumerate([45,135,225,315]):
 aa=math.radians(a);u=1.92+math.sin(aa)*1.01;v=1.33+math.cos(aa)*1.01
 yaw=-math.degrees(math.atan2(1.92-u,-(1.33-v)))
 place('GrayOfficeChair' if i==2 else 'MeshOfficeChair',u,v,yaw=yaw,name='Card table chair '+str(i+1))
for i,v in enumerate([.77,1.95]):place('GrayArmchair',.46,v,yaw=-90,name='Entrance gray armchair '+str(i+1))
place('RoundWoodSideTable',.36,1.35);place('TableLamp',.36,1.35,.57);place('SmallPottedPlant',.49,1.39,.57)
place('TanThrowPillow',.44,.8,.51,yaw=-75);place('DarkThrowPillow',.44,1.97,.51,yaw=-72)
for side,vs in [(0,[3.95,8.13,10.45]),(1,[2.27,7.9,10.15])]:
 for i,v in enumerate(vs):
  u=.155 if side==0 else 3.30;yaw=-90 if side==0 else 90
  place('SixShelfBookcase',u,v,yaw=yaw,name=('West' if side==0 else 'East')+' bookcase '+str(i+1))
  for k,h in enumerate([.032,.331,.631,.931,1.241,1.571]):
   if k in [1,4]:place('StackedBooks',u+(.022 if side==0 else -.022),v,h,yaw=yaw,name='Stacked shelf volumes')
   else:place('BookShelfContents'+str((i+side+k)%5),u,v,h,yaw=yaw,name='Shelf books')
  place('SmallPottedPlant',u,v-.17,1.57)
place('TrestlePlankWorktable',1.115,3.04)
place('DeskMat',1.60,3.07,.802,yaw=90);place('FoldedBlueWhiteUmbrella',1.55,3.1,.809,yaw=90)
place('OpenPokerChipCase',.78,3.06,.805,yaw=10);place('CardDeck',.94,2.81,.805,yaw=15)
place('GrayBackpack',.28,3.09,.805,yaw=-15);place('PaperScatter',1.29,3.08,.81,yaw=15);place('FrameRails',1.01,3.0,.815,yaw=-10)
place('MeshOfficeChair',1.06,3.75,yaw=10,name='Worktable back chair');place('MeshOfficeChair',1.35,2.37,yaw=170,name='Worktable front chair')
place('EntryRug',1.66,3.08,.007)
place('BlackSideTable',3.24,4.1,yaw=90);place('PaperScatter',3.24,4.1,.572);place('MeshOfficeChair',2.63,4.15,yaw=85,name='Side desk chair')
place('LeatherLoveseat',.48,6.10,yaw=-90)
place('DarkThrowPillow',.45,5.60,.535,yaw=-80);place('OrangeThrowPillow',.43,6.62,.535,yaw=-100)
place('CoffeeChest',1.63,6.15,yaw=90);place('RedCoffeeBook',1.66,6.07,.463,yaw=13)
place('LeatherClubChair',1.21,7.25,yaw=0);place('TanThrowPillow',1.21,7.37,.54,yaw=18)
place('PlantRoundStand',.31,7.18);place('TallGrassPlant',.31,7.18,.54)
place('LongRug',1.15,6.26,.007)
place('MeshOfficeChair',2.15,5.55,yaw=80,name='Lounge office chair');place('MeshOfficeChair',2.17,6.91,yaw=100,name='TV side chair')
place('LighthouseCanvas',.014,6.06,1.03,yaw=-90)
place('DistressedCreamConsole',3.245,6.19,yaw=90);place('WallTelevision',3.43,6.13,1.20,yaw=90)
place('TVLooseCables',3.419,6.13,.79,yaw=90)
place('PartyGameStack',3.16,6.20,.870,yaw=90);place('FramedGroupPhoto',3.15,5.80,.875,yaw=90)
place('OpenCardboardBox',3.18,6.36,.144,yaw=90);place('ClosedSilverCase',3.18,5.97,.144,yaw=90)
place('BlackSideTable',3.22,5.08);place('BlackSideTable',3.22,7.26);place('FloorLamp',3.25,7.24)
place('BrownLeatherBarStool',3.15,8.63,yaw=15)
place('WhitePlasticDrawerTower',3.22,9.54);place('StackedBooks',3.22,9.54,1.43)
place('BlackSideTable',3.22,10.93);place('SmallMonitor',3.25,10.9,.722,yaw=90);place('PaperScatter',3.19,10.94,.575)
place('RoundedOakStudyTable',2.12,10.62);place('StudyRug',2.12,10.68,.007)
place('DeskMonitor',2.02,10.66,.93,yaw=180);place('SmallMonitor',2.07,10.80,.93,yaw=180)
place('DeskRiser',2.11,10.17,.783);place('DeskOrganizer',2.53,10.61,.783)
place('PaperScatter',1.92,10.12,.79,yaw=-10);place('PaperScatter',2.59,10.3,.792,yaw=15)
place('DeskMouse',1.54,10.7,.788,yaw=-15);place('DeskPen',1.80,10.10,.788,yaw=50)
place('BlueMarker',1.52,10.29,.788,yaw=90);place('BlackMarker',2.46,10.17,.788,yaw=130);place('PaperCup',2.76,10.26,.785)
place('CurvedWoodDeskChair',2.24,11.55,yaw=0);place('MeshOfficeChair',3.17,10.62,yaw=90,name='Study side chair')
place('MeshOfficeChair',1.16,10.62,yaw=-90,name='Study aisle chair')
place('GlassWhiteboard',3.42,11.27,1.13,yaw=90)
place('StudyPendant',2.12,10.65,1.725)
for i,v in enumerate([.50,1.25,2.06]):place('FramedHistoricPhoto',.013,v,1.52 if i<2 else 1.0,yaw=-90)
for i,v in enumerate([8.86,9.25,9.64]):place('FramedHistoricPhoto',.013,v,1.39,yaw=-90)
place('FramedHistoricPhoto',3.43,3.91,1.45,yaw=90)
for y in [283.5,-292.5,-904.5]:
 manifest['placements'].append(dict(asset='CeilingHatchHardware',name='Ceiling crawlspace access details',location=[-2700,y,-101.35],yaw=0))
(R/'manifest.json').write_text(json.dumps(manifest,indent=2))
print('LIBRARY_KIT_READY',len(manifest['assets']),len(manifest['placements']),flush=True)
