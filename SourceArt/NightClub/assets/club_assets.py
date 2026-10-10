"""NightClub modular art. Blender offline only; does not edit Unreal map.
Run Blender --background --python thisfile. Metres -> UE centimetres [X,-Y,Z].
Wall furniture front = UE +Y, Blender -Y. All assets pivot at authored origin.
"""
import bpy, bmesh, os, json, math, collections, random
from mathutils import Vector
R=os.path.dirname(os.path.abspath(__file__)); MODEL=R+'/models'
os.makedirs(MODEL,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1.0
G=collections.defaultdict(list)
MAN={'coordinateSystem':'Blender metres -> Unreal centimetres [100X,-100Y,100Z]. Front = Unreal +Y. Assets unscaled, root house scale = 1.1.','assets':[],'materials':{},'notes':['Use emissive neon materials with bloom; emissive geometry is the neon light source, no per-tube lights.','Simple box collision only for DJ booth / speaker. Pole, guard, gate collision handled explicitly by runtime.','Guard aperture pre-scale radius .8636m; outer guard 1.075m. Open guard access faces UE+Y, 60 degree gap. Separate gate closes that gap.']}
def mat(name,c,rough=.5,metal=0,emission=0):
 m=bpy.data.materials.new('M_NC_'+name);m.diffuse_color=(*c,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF')
 p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 if emission:p.inputs['Emission Color'].default_value=(*c,1);p.inputs['Emission Strength'].default_value=emission
 MAN['materials'][m.name]={'color':list(c),'roughness':rough,'metallic':metal,'emissive_strength':emission,'emissive':list(c) if emission else [0,0,0]}
 return m
BLACK=mat('PowderCoat',(.009,.011,.019),.48,.52)
RUBBER=mat('Rubber',(.005,.006,.008),.88)
ALU=mat('BrushedAluminium',(.28,.31,.37),.25,.95)
CHROME=mat('Chrome',(.62,.67,.73),.14,1)
MIRROR=mat('MirrorTile',(.73,.77,.82),.07,1)
WOOD=mat('DarkWalnut',(.035,.018,.012),.46)
CYAN=mat('NeonCyan',(.03,.68,1),.3,.15,18)
MAGENTA=mat('NeonMagenta',(.95,.028,.40),.3,.15,16)
VIOLET=mat('NeonViolet',(.32,.065,1),.3,.15,20)
AMBER=mat('NeonWarmAmber',(1,.28,.035),.3,.12,14)
TINYWHITE=mat('TinyDisplayWhite',(.56,.71,.85),.36,0,2.5)
GREEN=mat('DisplayGreen',(.03,.62,.21),.36,0,3)
RED=mat('DisplayRed',(.6,.008,.02),.36,0,3)
def mesh(name,v,f,ma,g,smooth=False):
 d=bpy.data.meshes.new(name);d.from_pydata(v,[],f);d.update();o=bpy.data.objects.new(name,d);bpy.context.collection.objects.link(o)
 if ma:d.materials.append(ma)
 for p in d.polygons:p.use_smooth=smooth
 G[g].append(o);return o
def cube(name,loc,size,ma,g,bevel=0,seg=3):
 x,y,z=[a*.5 for a in size];v=[(-x,-y,-z),(x,-y,-z),(x,y,-z),(-x,y,-z),(-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z)]
 o=mesh(name,v,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],ma,g);o.location=loc
 if bevel:
  md=o.modifiers.new('Machined softened edge','BEVEL');md.width=min(bevel,min(size)*.45);md.segments=seg;md.harden_normals=True
  bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.modifier_apply(modifier=md.name);o.select_set(False)
  for p in o.data.polygons:p.use_smooth=True
  o['weighted']=True
 return o
def cyl(name,loc,r,h,ma,g,n=32,axis='Z'):
 bpy.ops.mesh.primitive_cylinder_add(vertices=n,radius=r,depth=h,location=loc);o=bpy.context.object;o.name=name
 if axis=='Y':o.rotation_euler.x=math.pi/2
 if axis=='X':o.rotation_euler.y=math.pi/2
 o.data.materials.append(ma)
 for p in o.data.polygons:p.use_smooth=len(p.vertices)==4
 o['weighted']=True;G[g].append(o);return o
def ball(name,loc,r,ma,g,seg=16,rings=8):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=seg,ring_count=rings,radius=r,location=loc);o=bpy.context.object;o.name=name;o.data.materials.append(ma)
 for p in o.data.polygons:p.use_smooth=True
 G[g].append(o);return o
def tube(name,pts,r,ma,g,sides=12,caps=True):
 pts=[Vector(q) for q in pts];v=[];f=[]
 for i,p in enumerate(pts):
  d=pts[min(i+1,len(pts)-1)]-pts[max(i-1,0)];d.normalize();ref=Vector((0,0,1)) if abs(d.z)<.92 else Vector((0,1,0));u=d.cross(ref).normalized();w=d.cross(u)
  for j in range(sides):a=j*math.tau/sides;v.append(tuple(p+r*(math.cos(a)*u+math.sin(a)*w)))
 for i in range(len(pts)-1):
  for j in range(sides):f.append((i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j))
 if caps:f.extend([tuple(range(sides-1,-1,-1)),tuple((len(pts)-1)*sides+j for j in range(sides))])
 o=mesh(name,v,f,ma,g,True)
 if caps:
  for p in o.data.polygons[-2:]:p.use_smooth=False
 return o
def arc(name,c,r,a0,a1,tuber,ma,g,n=72,plane='XY'):
 pts=[]
 for j in range(n+1):
  a=a0+(a1-a0)*j/n
  pts.append((c[0]+r*math.cos(a),c[1]+r*math.sin(a),c[2]) if plane=='XY' else (c[0]+r*math.cos(a),c[1],c[2]+r*math.sin(a)))
 return tube(name,pts,tuber,ma,g,12,True)
def ring(name,c,rin,rout,h,ma,g,n=96):
 v=[];f=[]
 for z in(c[2]-h/2,c[2]+h/2):
  for r in(rin,rout):
   for j in range(n):a=j*math.tau/n;v.append((c[0]+r*math.cos(a),c[1]+r*math.sin(a),z))
 for j in range(n):k=(j+1)%n;f.extend([(j,k,n+k,n+j),(2*n+j,3*n+j,3*n+k,2*n+k),(n+j,n+k,3*n+k,3*n+j),(j,2*n+j,2*n+k,k)])
 o=mesh(name,v,f,ma,g);o['weighted']=True;return o
# Neon stroke font, circular bends sampled. Letters keep physical tube gaps between strokes.
def circ(cx,cy,rx,ry,a0=0,a1=math.tau,n=28):return [(cx+rx*math.cos(a0+(a1-a0)*j/n),cy+ry*math.sin(a0+(a1-a0)*j/n)) for j in range(n+1)]
def bezier(p0,p1,p2,p3,n=18):
 p0,p1,p2,p3=[Vector(p) for p in(p0,p1,p2,p3)];return [tuple((1-t)**3*p0+3*(1-t)**2*t*p1+3*(1-t)*t*t*p2+t**3*p3) for t in[j/n for j in range(n+1)]]
FONT={
 'A':[[(0,0),(.5,1),(1,0)],[(.23,.43),(.77,.43)]],
 'E':[[(1,1),(0,1),(0,0),(1,0)],[(0,.5),(.82,.5)]],
 'F':[[(0,0),(0,1),(1,1)],[(0,.5),(.82,.5)]],
 'G':[circ(.5,.5,.5,.5,.22,math.tau-.22,36),[(.55,.48),(1,.48),(1,.12)]],
 'H':[[(0,0),(0,1)],[(1,0),(1,1)],[(0,.5),(1,.5)]],
 'I':[[(.1,0),(.9,0)],[(.5,0),(.5,1)],[(.1,1),(.9,1)]],
 'L':[[(0,1),(0,0),(1,0)]],
 'N':[[(0,0),(0,1),(1,0),(1,1)]],
 'O':[circ(.5,.5,.5,.5,n=36)],
 'P':[[(0,0),(0,1),(.58,1)]+circ(.58,.76,.42,.24,math.pi/2,-math.pi/2,18)[1:]+[(0,.52)]],
 'R':[[(0,0),(0,1),(.58,1)]+circ(.58,.76,.42,.24,math.pi/2,-math.pi/2,18)[1:]+[(0,.52)],[(.46,.52),(1,0)]],
 'S':[bezier((.94,.87),(.70,1.06),(.11,1.05),(.07,.76))+bezier((.07,.76),(.01,.50),(.93,.53),(.94,.23))[1:]+bezier((.94,.23),(.96,-.07),(.29,-.03),(.04,.13))[1:]],
 'T':[[(0,1),(1,1)],[(.5,1),(.5,0)]],
 'U':[[(0,1),(0,.28)]+circ(.5,.28,.5,.28,math.pi,math.tau,24)[1:]+[(1,1)]],
}
def neon_text(text,g,ma,height=.33,z=0):
 # Width .66*height, generous tube gaps. Centered signage, size known without font dependencies.
 w=height*.68;gap=height*.31;space=height*.64;length=sum(space if c==' ' else w+gap for c in text)-gap;x=-length/2
 for ci,ch in enumerate(text):
  if ch==' ':x+=space;continue
  for pi,path in enumerate(FONT[ch]):
   # soften corners by quadratic cuts for real bent glass, never round away a whole letter.
   pts=[]
   for j,q in enumerate(path):
    if j==0 or j==len(path)-1:pts.append(q);continue
    p=Vector(path[j-1]);t=Vector(q);n=Vector(path[j+1]);d1=t-p;d2=n-t
    if d1.length<.02 or d2.length<.02 or d1.normalized().dot(d2.normalized())>.97:pts.append(q);continue
    cut=min(.048,d1.length*.18,d2.length*.18);a=t-d1.normalized()*cut;b=t+d2.normalized()*cut
    pts.append(tuple(a))
    for k in range(1,5):u=k/4;v=(1-u)**2*a+2*(1-u)*u*t+u*u*b;pts.append(tuple(v))
   coords=[(x+a*w,-.038,z+(b-.5)*height) for a,b in pts]
   tube('Bent glass '+ch+' '+str(ci)+'-'+str(pi),coords,.0115,ma,g,10)
   # actual rounded glow endpoints, clean glass caps even on open strokes
   if Vector(coords[0])-Vector(coords[-1]) != Vector((0,0,0)):
    ball('Rounded glass endcap',coords[0],.0115,ma,g,12,6);ball('Rounded glass endcap',coords[-1],.0115,ma,g,12,6)
  x+=w+gap
 return length
for name,text,ma,h in [('PhiSig','PHI SIG',CYAN,.43),('OpenLate','OPEN LATE',MAGENTA,.35),('AfterHours','AFTER HOURS',VIOLET,.30)]:
 g='SM_NC_Sign_'+name;width=neon_text(text,g,ma,h)
 # floating black acrylic mounting board, quiet aluminium spacers, fine frame and screws
 cube('Dark acrylic tube sign backing',(0,.015,0),(width+.22,.023,h+.26),BLACK,g,.035,5)
 for x in(-width/2-.06,width/2+.06):
  for z in(-h/2-.09,h/2+.09):
   cyl('Silver mounting screw',(x,-.001,z),.005,.006,ALU,g,16,'Y')
   cube('Mounting standoff',(x,.04,z),(.018,.05,.018),BLACK,g,.004,3)
 # secondary understated tube border and corner breaks
 tube('Left corner accent',[(-width/2-.08,-.031,-h/2-.06),(-width/2-.08,-.031,h/2+.06),(-width/2+.04,-.031,h/2+.06)],.0045,ma,g,8)
 tube('Right corner accent',[(width/2-.04,-.031,-h/2-.06),(width/2+.08,-.031,-h/2-.06),(width/2+.08,-.031,h/2+.06)],.0045,ma,g,8)
for name,ma in [('Cyan',CYAN),('Magenta',MAGENTA),('Violet',VIOLET)]:
 g='SM_NC_WallStrip_'+name
 cube('Tube dark metal channel',(0,.012,0),(2.04,.028,.058),BLACK,g,.008,4)
 tube('Continuous glow tube',[(-1,-.018,0),(1,-.018,0)],.012,ma,g,14)
 for x in(-1.014,1.014):cube('Tube endcap',(x,-.004,0),(.016,.032,.044),ALU,g,.006,3)
 for x in(-.85,.85):cube('Wall standoff',(x,.039,0),(.030,.035,.035),BLACK,g,.004,2)
# Detailed DJ booth. Horizontal equipment face top is Z; booth audience is UE+Y.
g='SM_NC_DJBooth'
cube('Deep walnut booth cabinet',(0,0,.50),(2.62,.88,.96),WOOD,g,.045,5)
cube('Brushed console top',(0,0,1.00),(2.72,.97,.07),BLACK,g,.025,5)
cube('Equipment raised frame',(0,.10,1.057),(2.28,.67,.055),BLACK,g,.025,4)
for x in(-1.28,1.28):cube('Booth metal side upright',(x,-.47,.50),(.042,.055,.93),ALU,g,.008,3)
for z in(.07,.89):cube('Booth horizontal chrome trim',(0,-.472,z),(2.53,.018,.025),ALU,g,.006,3)
for j in range(25):cube('Fine walnut vertical louvre',(-1.20+j*.1,-.465,.48),(.026,.028,.75),WOOD,g,.004,2)
tube('Audience-facing cyan booth line',[(-1.19,-.488,.815),(1.19,-.488,.815)],.006,CYAN,g,10)
for x in(-1.1,1.1):
 for y in(-.34,.34):cyl('Isolated rubber cabinet foot',(x,y,.018),.045,.037,RUBBER,g,20)
for x in(-.74,.74):
 cube('Deck controller shell',(x,.105,1.10),(.64,.62,.042),ALU,g,.020,4)
 cube('Deck black faceplate',(x,.105,1.123),(.616,.598,.012),BLACK,g,.012,3)
 cyl('Jog wheel concentric base',(x,.09,1.14),.158,.030,ALU,g,64)
 cyl('Jog wheel rubber disc',(x,.09,1.16),.143,.021,RUBBER,g,64)
 ring('Jog wheel silver grip edge',(x,.09,1.167),.141,.151,.014,ALU,g,64)
 cyl('Jog wheel central touch disk',(x,.09,1.174),.105,.006,BLACK,g,48)
 for j in range(18):
  a=j*math.tau/18;cube('Jog wheel radial index',(x+.130*math.cos(a),.09+.130*math.sin(a),1.178),(.007,.007,.002),ALU,g,.001,1)
 cube('Deck mini waveform screen',(x,.31,1.135),(.19,.105,.006),BLACK,g,.006,2)
 for j in range(14):cube('Tiny cyan waveform segment',(x-.081+j*.012,.31,1.14),(.006,.012+.065*abs(math.sin(j*1.7)),.002),CYAN,g)
 for j in range(4):
  cube('Deck transport pad',(x-.22+j*.075,-.155,1.14),(.055,.032,.013),MAGENTA if j<2 else CYAN,g,.007,3)
 for y in(.12,.20,.29):cyl('Deck gain knob',(x-.255,y,1.144),.014,.025,BLACK,g,16)
 cube('Deck pitch fader rail',(x+.265,.12,1.14),(.012,.23,.006),RUBBER,g,.002,2)
 cube('Deck pitch fader cap',(x+.265,.16,1.151),(.032,.024,.015),ALU,g,.003,2)
cube('Mixer central black deck',(0,.07,1.12),(.64,.64,.041),BLACK,g,.018,4)
for x in(-.18,-.06,.06,.18):
 for y in(.18,.26,.34):
  cyl('Mixer EQ rotary knob',(x,y,1.162),.021,.030,BLACK,g,20)
  cube('Mixer knob silver indicator',(x,y-.012,1.18),(.003,.011,.002),ALU,g)
 cube('Mixer channel fader slot',(x,-.002,1.148),(.009,.19,.006),RUBBER,g,.003,2)
 cube('Mixer channel fader cap',(x,.02,1.163),(.046,.03,.018),ALU,g,.004,3)
 for k in range(8):cube('Mixer stereo VU meter',(x+.04,.10-k*.019,1.152),(.010,.010,.003),GREEN if k<5 else AMBER if k<7 else RED,g,.002,2)
cube('Crossfader channel',(0,-.19,1.15),(.4,.011,.006),RUBBER,g,.003,2)
cube('Crossfader grip',(.05,-.19,1.161),(.034,.045,.018),ALU,g,.005,3)
# Headphones parked on right deck, curved headband and visibly padded ear cups.
arc('Headphone curved headband',(1.04,.35,1.158),.105,0,math.pi,.011,BLACK,g,28,'XZ')
for x in(.935,1.145):
 cyl('Headphone metallic cup shell',(x,.35,1.16),.038,.040,ALU,g,24,'X')
 cyl('Headphone foam ear cushion',(x-.008 if x<1 else x+.008,.35,1.16),.042,.028,RUBBER,g,24,'X')
# One high quality PA speaker, can instantiate a pair. Audience front UE+Y.
g='SM_NC_PASpeaker'
cube('PA bevel enclosure',(0,0,.65),(.57,.43,1.19),BLACK,g,.047,5)
cube('Speaker front grille',(0,-.221,.66),(.48,.010,1.03),RUBBER,g,.018,3)
for z,r in((.43,.191),(.88,.115)):
 cyl('Cone outer mounting rim',(0,-.234,z),r,.016,ALU,g,64,'Y')
 cyl('Speaker driver rubber suspension',(0,-.245,z),r*.91,.026,RUBBER,g,64,'Y')
 # concave loudspeaker cone geometry, depth and chrome dustcap rim
 v=[];f=[]
 for rr,yy in((r*.88,-.255),(r*.42,-.215),(r*.27,-.255)):
  for j in range(48):a=j*math.tau/48;v.append((rr*math.cos(a),yy,z+rr*math.sin(a)))
 for k in range(2):
  for j in range(48):f.append((k*48+j,k*48+(j+1)%48,(k+1)*48+(j+1)%48,(k+1)*48+j))
 mesh('Shaped loudspeaker cone',v,f,BLACK,g,True)
 d=ball('Speaker centre dust cap',(0,-.254,z),r*.29,RUBBER,g,24,10);d.scale.y=.35
 for a in range(8):t=a*math.tau/8;cyl('Speaker cone bolt',(r*.96*math.cos(t),-.246,z+r*.96*math.sin(t)),.005,.004,BLACK,g,12,'Y')
cube('Speaker bass reflex port',(0,-.233,.17),(.31,.014,.062),BLACK,g,.014,4)
for x in(-.12,0,.12):cube('Bass port internal ridge',(x,-.243,.17),(.014,.026,.04),ALU,g,.002,2)
cube('Speaker tiny front badge',(0,-.251,1.106),(.096,.006,.025),ALU,g,.004,2)
cube('Speaker badge cyan mark',(0,-.256,1.108),(.027,.002,.004),CYAN,g)
for x in(-.15,.15):
 for y in(-.12,.12):cyl('Speaker rubber feet',(x,y,.034),.035,.068,RUBBER,g,20)
# Faceted disco ball mirror mosaic; gaps are actual dark geometry underneath.
g='SM_NC_DiscoBall';rad=.315
ball('Disco ball dark inner globe',(0,0,0),rad-.003,BLACK,g,32,16)
v=[];f=[]
for j in range(1,22):
 a0=math.pi*j/23+.006;a1=math.pi*(j+1)/23-.006
 n=max(8,round(48*math.sin((a0+a1)*.5)))
 for i in range(n):
  b0=math.tau*i/n+.008;b1=math.tau*(i+1)/n-.008;idx=len(v)
  for a,b in((a0,b0),(a1,b0),(a1,b1),(a0,b1)):v.append((rad*math.sin(a)*math.cos(b),rad*math.sin(a)*math.sin(b),rad*math.cos(a)))
  f.append((idx,idx+1,idx+2,idx+3))
mesh('Individual flat polished mirror squares',v,f,MIRROR,g,False)
cyl('Disco ball suspension spindle',(0,0,.407),.010,.17,CHROME,g,24)
cyl('Disco ball motor housing',(0,0,.497),.064,.044,BLACK,g,36)
ring('Disco ball chrome motor band',(0,0,.497),.060,.066,.016,ALU,g,36)
# Chrome fireman pole; bottom pivot, published default height301.5cm for scalable exact install.
g='SM_NC_FirePole';H=3.015
cyl('Single smooth chrome slide pole',(0,0,H/2),.043,H,CHROME,g,64)
ring('Recessed polished floor pole socket',(0,0,.018),.043,.099,.028,CHROME,g,48)
ring('Socket rubber isolation gasket',(0,0,.004),.052,.102,.006,RUBBER,g,48)
for j in range(4):a=j*math.tau/4;cyl('Pole floor anchoring bolt',(.079*math.cos(a),.079*math.sin(a),.035),.006,.006,ALU,g,16)
g='SM_NC_PoleHoleCollar'
ring('Round open floor aperture trim',(0,0,.006),.8636,.940,.028,CHROME,g,128)
ring('Round dark outer collar rubber gasket',(0,0,-.006),.932,.953,.010,RUBBER,g,128)
for j in range(16):a=j*math.tau/16;cyl('Collar countersunk screw',(.916*math.cos(a),.916*math.sin(a),.023),.005,.004,ALU,g,16)
# Upper guard leaves a60deg access opening centered Unreal+Y, Blender-Y.
g='SM_NC_PoleGuard';RADIUS=1.075;HEIGHT=.955;a0=-math.pi/3;a1=math.pi*4/3
for h,r in((HEIGHT,.026),(.34,.017)):
 arc('Rounded black guardrail arc',(0,0,h),RADIUS,a0,a1,r,BLACK,g,84)
for j in range(13):
 a=a0+(a1-a0)*j/12;x=RADIUS*math.cos(a);y=RADIUS*math.sin(a)
 cyl('Guard upright',(x,y,HEIGHT*.5),.018,HEIGHT,BLACK,g,20)
 cyl('Guard upright chrome foot',(x,y,.018),.059,.036,CHROME,g,32)
 for d in(-1,1):cyl('Guard foot anchor bolt',(x+d*.036,y,.039),.004,.005,ALU,g,12)
# Single swing gate pivot at clockwise gap end. Meshlocal hinge at0 for runtimegate.
g='SM_NC_PoleAccessGate';hx=RADIUS*math.cos(a0);hy=RADIUS*math.sin(a0)
pts=[]
for j in range(25):
 a=-math.pi/3-(math.pi/3)*j/24;pts.append((RADIUS*math.cos(a)-hx,RADIUS*math.sin(a)-hy,HEIGHT))
tube('Gate upper matching rounded rail',pts,.026,BLACK,g,12)
pts2=[(x,y,.34) for x,y,z in pts];tube('Gate lower matching rail',pts2,.017,BLACK,g,12)
for j in(0,8,16,24):
 x,y,_=pts[j];cyl('Gate upright',(x,y,HEIGHT*.5),.018,HEIGHT,BLACK,g,20)
for h in(.23,.77):cyl('Gate real chrome hinge barrel',(0,0,h),.028,.09,CHROME,g,24)
x,y,_=pts[-1];cube('Gate latch catch',(x,y,.80),(.10,.065,.085),BLACK,g,.015,3)
cube('Gate latch silver pull',(x,y-.040,.81),(.09,.018,.028),CHROME,g,.008,4)
# Compact stage marker at lower pole, actual dark rubber pad no closedapertureupstairs.
g='SM_NC_PoleLandingPad';cyl('Non-slip dark landing pad',(0,0,.005),.79,.010,RUBBER,g,96)
ring('Cyan landing circle',(0,0,.013),.71,.717,.005,CYAN,g,128)
for j in range(10):
 a=-math.pi/3+j*(math.pi*5/3)/9;cube('Pad subtle chrome marker',(.744*math.cos(a),.744*math.sin(a),.014),(.011,.011,.003),ALU,g,.002,2)

def finish(name,objects):
 for o in objects:
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
  bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='FIXED',ngon_method='BEAUTY');bmesh.ops.recalc_face_normals(bm,faces=bm.faces);bm.to_mesh(o.data);bm.free()
  if o.get('weighted'):
   wm=o.modifiers.new('Baked weighted normals','WEIGHTED_NORMAL');wm.weight=50;wm.keep_sharp=True;bpy.ops.object.modifier_apply(modifier=wm.name)
  if not o.data.uv_layers:
   uv=o.data.uv_layers.new(name='UVMap')
   for p in o.data.polygons:
    axis=max(range(3),key=lambda k:abs(p.normal[k]));aa,bb=((1,2),(0,2),(0,1))[axis]
    for li in p.loop_indices:q=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(q[aa],q[bb])
  o.select_set(False)
 bpy.ops.object.select_all(action='DESELECT')
 for o in objects:o.select_set(True)
 bpy.context.view_layer.objects.active=objects[0]
 if len(objects)>1:bpy.ops.object.join()
 o=bpy.context.object;o.name=name;o.data.name=name;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
 path=MODEL+'/'+name+'.fbx';bpy.ops.export_scene.fbx(filepath=path,use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',use_mesh_modifiers=True,bake_space_transform=False,add_leaf_bones=False)
 v=[o.matrix_world@q.co for q in o.data.vertices];lo=[min(q[k] for q in v) for k in range(3)];hi=[max(q[k] for q in v) for k in range(3)]
 MAN['assets'].append({'name':name,'fbx':path,'triangles':len(o.data.polygons),'vertices':len(o.data.vertices),'boundsMetres':[lo,hi],'boundsUnrealCm':[[100*lo[0],-100*hi[1],100*lo[2]],[100*hi[0],-100*lo[1],100*hi[2]]],'materials':[m.name for m in o.data.materials],'origin':'Authored local origin, metres. Signcenter or furniturefloor or polebottom. Gatehinge0.','collision':'Simple box for DJ booth / speaker only; runtime explicit for pole/guard/gate. No neon collision.','hasCustomNormals':o.data.has_custom_normals})
 return o
OBJS={}
for name,objects in list(G.items()):OBJS[name]=finish(name,objects)
MAN['pole']={'defaultHeightCm':301.5,'radiusCm':4.3,'holeRadiusCm':86.36,'guardRadiusCm':107.5,'guardHeightCm':95.5,'gateHingeCm':[hx*100,-hy*100,0],'gateGapDirectionUE':[0,1,0],'gateGapDegrees':60}
with open(R+'/club_manifest.json','w') as f:json.dump(MAN,f,indent=2)
bpy.ops.wm.save_as_mainfile(filepath=R+'/NightClub_source.blend')
# Isolated product showcase, only exportedassetsabove are inmanifest. Preview is artqualitysupporting evidence.
for o in OBJS.values():o.hide_render=True
def show(name,loc,rot=0):
 o=OBJS[name].copy();o.data=OBJS[name].data;bpy.context.collection.objects.link(o);o.hide_render=False;o.location=loc;o.rotation_euler.z=rot;return o
show('SM_NC_DJBooth',(0,0,0))
show('SM_NC_PASpeaker',(-1.8,.15,0));show('SM_NC_PASpeaker',(1.8,.15,0))
show('SM_NC_Sign_PhiSig',(0,.9,2.15))
show('SM_NC_Sign_OpenLate',(-3.1,.9,2.15),0)
show('SM_NC_Sign_AfterHours',(3.2,.9,2.15),0)
show('SM_NC_DiscoBall',(0,.3,2.70))
for name,x,z in(('SM_NC_WallStrip_Cyan',-2.65,1.52),('SM_NC_WallStrip_Magenta',2.65,1.52),('SM_NC_WallStrip_Violet',0,.48)):
 show(name,(x,.915,z))
def stagecube(name,loc,size,ma):
 o=cube(name,loc,size,ma,'__PREVIEW__',.01,3);return o
stagecube('Preview dark floor',(0,0,-.085),(12,8,.15),mat('PreviewFloor',(.018,.024,.035),.28,.4))
stagecube('Preview rear wall',(0,1,1.6),(12,.10,3.3),BLACK)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=40;scene.render.resolution_x=1800;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
scene.world.color=(.035,.035,.05)
def light(name,loc,color,power,size=4):
 d=bpy.data.lights.new(name,'AREA');d.color=color;d.energy=power;d.shape='DISK';d.size=size;o=bpy.data.objects.new(name,d);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,1))-o.location).to_track_quat('-Z','Y').to_euler()
light('Soft cyan artkey',(-3,-3,3.4),(.19,.65,1),400,4);light('Soft magenta artkey',(3,-2,3.2),(1,.15,.49),330,4);light('Top equipment reflection',(0,-1,3.4),(.65,.77,1),220,3)
def camera(loc,target,path,lens=45):
 d=bpy.data.cameras.new('Preview camera');o=bpy.data.objects.new('Preview camera',d);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();d.lens=lens;scene.camera=o;scene.render.filepath=path;bpy.ops.render.render(write_still=True)
nt=bpy.data.node_groups.new('NightClub art bloom','CompositorNodeTree');scene.compositing_node_group=nt
nt.interface.new_socket(name='Image',in_out='OUTPUT',socket_type='NodeSocketColor')
rl=nt.nodes.new('CompositorNodeRLayers');gl=nt.nodes.new('CompositorNodeGlare');gl.inputs['Type'].default_value='Fog Glow';gl.inputs['Quality'].default_value='High';gl.inputs['Threshold'].default_value=1.8;gl.inputs['Size'].default_value=.25
co=nt.nodes.new('NodeGroupOutput');nt.links.new(rl.outputs['Image'],gl.inputs['Image']);nt.links.new(gl.outputs['Image'],co.inputs['Image'])
camera((6.6,-11.4,4.7),(0,.05,1.36),R+'/club-assets-preview.png',43)
# Poleguard isolated higherdetail image.
for o in bpy.context.scene.objects:
 if o.name.startswith('SM_NC_'):o.hide_render=True
show('SM_NC_PoleHoleCollar',(0,-1,0));show('SM_NC_PoleGuard',(0,-1,0));show('SM_NC_PoleAccessGate',(hx,-1+hy,0));show('SM_NC_FirePole',(0,-1,-2.3))
camera((3.6,-5.7,2.5),(0,-1,.43),R+'/pole-guard-preview.png',55)
print('NIGHTCLUB_ASSETS_PASS assets='+str(len(MAN['assets']))+' triangles='+str(sum(a['triangles'] for a in MAN['assets'])))
