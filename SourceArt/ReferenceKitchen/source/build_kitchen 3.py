"""Photo matched kitchen, model dimensions in metres. House XY=1.8, Z=1.
The original walls, entrances and upper floors are preserved. Individual props
use fitted rigid body hulls and four damage stages, not one combined room body.
"""
import bpy,bmesh,json,math,random,runpy,collections
from pathlib import Path
from mathutils import Vector,Matrix
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen');D=R/'models';D.mkdir(exist_ok=True)
DEST='/Game/SeniorSendOff/House/ReferenceKitchen'
DM=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/generate_damage_v2.py');S=DM['S'];G=collections.defaultdict(list);M={};rng=random.Random(2254)
manifest=dict(assets=[],placements=[],materials={},scale=[1.8,1.8,1],floor=0,notes=['All nine unique photos reviewed.','Dimensions inferred from photos and existing shell; no surveyed dimensions supplied.','Existing house XY scale of 1.8 retained.'])
bpy.ops.wm.read_factory_settings(use_empty=True)
def mat(k,c,rough=.65,metal=0,tex=None,opacity=None,emission=0):
 n='M_Kitchen_'+k;m=bpy.data.materials.new(n);m.diffuse_color=(*c,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal;d=dict(base_color=list(c),roughness=rough,metallic=metal)
 if tex:
  f=R/'textures'/(tex+'.png');d['texture']=str(f);t=m.node_tree.nodes.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(f));m.node_tree.links.new(t.outputs['Color'],p.inputs['Base Color']);rf=f.with_name(f.stem+'_Roughness.png')
  if rf.exists():d['roughness_texture']=str(rf)
 if opacity is not None:p.inputs['Alpha'].default_value=opacity;d['opacity']=opacity
 if emission:d['emission']=emission
 M[k]=m;manifest['materials'][n]=d;return m
wood=mat('DarkWalnutWood',(1,1,1),.58,tex='DarkWalnut');steel=mat('BrushedStainlessMetal',(1,1,1),.36,.85,'BrushedSteel');chrome=mat('PolishedChromeMetal',(.52,.55,.57),.20,.93);black=mat('BlackMetal',(.009,.01,.012),.43,.45);brownblack=mat('CherryChairWood',(.035,.009,.008),.36);green=mat('GreenSeatVinyl',(1,1,1),.48,tex='GreenVinyl');white=mat('WhiteAppliancePaint',(.76,.77,.73),.48);paper=mat('PaperWhite',(.85,.85,.79),.92);towel=mat('EmbossedPaperTowel',(1,1,1),.98,tex='PaperTowel');rubber=mat('Rubber',(.011,.012,.014),.9);plastic=mat('BlackPlastic',(.014,.018,.02),.5);screen=mat('DarkScreenGlass',(.007,.013,.018),.14,.18);glass=mat('ClearGuardGlass',(.78,.88,.89),.08,.1,opacity=.12);wrap=mat('ClearPlasticWrap',(.68,.72,.70),.3,opacity=.18);blue=mat('BlueStatusLed',(.018,.22,1),.25,emission=3);blueplastic=mat('CoolerBluePlastic',(.025,.09,.115),.71);orange=mat('TennesseeOrange',(.80,.21,.013),.8);cardboard=mat('CardboardPaper',(.37,.255,.14),.96);leaf=mat('OliveLeaves',(.07,.095,.024),.8);rust=mat('RustFabricPetal',(.40,.072,.017),.9);gold=mat('GoldFabricPetal',(.63,.29,.028),.87);dark=mat('FlowerCenter',(.065,.030,.009),1);linen=mat('BurlapFabric',(.42,.31,.16),.99);soil=mat('BreadCrust',(.48,.23,.066),.82);crumb=mat('BreadCrumb',(.68,.54,.30),.9);floor=mat('OakPlankWood',(1,1,1),.67,tex='OakPlanks');red=mat('RedExitLight',(.8,.012,.009),.5,emission=2);stain=mat('FloorWear',(.09,.075,.049),.95);bluepack=mat('BluePackaging',(.012,.11,.29),.52);goldframe=mat('AgedGoldFrameMetal',(.30,.22,.091),.6,.7)
exec(compile((R/'source/helpers.py').read_text(),'helpers.py','exec'))
def labeled(k,tex):return mat(k,(1,1,1),.78,tex=tex)
labels={k:labeled(k,k)for k in ['FoilBox','ZipBox','SnackBox','PaperPack','CerealLabel']}
arts={k:labeled(k,k)for k in ['CityPrint','CafePrint','RulesPrint']}
def bolt(g,c,r=.006,axis='Y'):cyl('exposed round screw',c,r,.003,chrome,g,12,axis)
def finish(g,*args,**kw):
 a=finalize(g,*args,**kw);print('KITCHEN_MODEL',g,a['faces'],flush=True);return a
# A curved spindle chair with green upholstery, turned legs, stretchers and piping.
g='GreenSpindleChair'
box('seat timber rim',(0,0,.43),(.45,.46,.045),brownblack,g,.025,4);box('green upholstered cushion',(0,-.015,.468),(.426,.437,.055),green,g,.025,5)
for sx in [-1,1]:
 for sy in [-1,1]:
  a=(sx*.19,sy*.18,.415);b=(sx*.215,sy*.205,.025);tube('slightly splayed leg',[b,(sx*.198,sy*.193,.25),a],.021,brownblack,g,10)
  for z in [.09,.13,.35]:ell('turned leg bead',(sx*(.211-.05*z),sy*.20,z),(.025,.025,.018),brownblack,g)
for yy in [-.19,.19]:tube('seat stretcher',[(-.204,yy,.17),(.204,yy,.17)],.014,brownblack,g)
for xx in [-.203,.203]:tube('seat stretcher',[(xx,-.19,.18),(xx,.19,.18)],.014,brownblack,g)
arc=[(.225*math.cos(math.pi*i/16),.205+.012*math.sin(math.pi*i/16),.53+.39*math.sin(math.pi*i/16))for i in range(17)]
tube('arched cherry crown',[(-.225,.205,.45),*list(reversed(arc)),(.225,.205,.45)],.02,brownblack,g,10)
for i in range(7):
 x=(i-3)*.058;z=.53+.39*math.sqrt(max(0,1-(x/.225)**2));tube('curved thin back spindle',[(x*.83,.198,.465),(x*.91,.21,.66),(x,.213,z)],.009,brownblack,g,8)
tube('seat seam',[(-.192,-.206,.466),(.192,-.206,.466),(.206,.18,.466),(-.206,.18,.466)],.0023,green,g,closed=True);finish(g,9,26)
# Three full size tables join without a seam in the walking surface.
g='DiningTable';box('satin walnut tabletop',(0,0,.747),(1.70,.78,.045),wood,g,.012,4)
for x in [-.73,.73]:
 for y in [-.29,.29]:box('square walnut leg',(x,y,.358),(.078,.078,.716),wood,g,.008)
for y in [-.315,.315]:box('mortised apron',(0,y,.66),(1.52,.035,.13),wood,g)
for x in [-.76,.76]:box('end apron',(x,0,.66),(.035,.60,.13),wood,g)
finish(g,48,65)
# Commercial buffet with separately modelled recessed pans and angled glass.
g='ServingCounter';w=1.20
box('buffet cabinet',(0,0,.455),(w,.72,.74),steel,g,.009)
for x in [-w/2+.08,w/2-.08]:
 for y in [-.27,.27]:cyl('adjustable foot',(x,y,.10),.024,.20,chrome,g);cyl('rubber foot',(x,y,.025),.03,.022,rubber,g)
for x in [-.295,.295]:
 box('inset cabinet door',(x,-.365,.47),(.576,.012,.65),steel,g,.004);tube('recessed pull',[(x-.10,-.377,.69),(x+.10,-.377,.69)],.005,chrome,g)
# Top rail border and pan dividers, leaving recessed wells rather than a flat lid.
for y in [-.348,.348]:box('rolled steel worktop lip',(0,y,.862),(w+.02,.06,.035),steel,g,.008)
for x in [-.602,0,.602]:box('well divider',(x,0,.862),(.035,.64,.035),steel,g,.004)
for x in [-.30,.30]:
 box('deep food pan base',(x,0,.718),(.532,.52,.018),steel,g,.02)
 for xx in [x-.267,x+.267]:box('pan side',(xx,0,.78),(.012,.52,.15),steel,g,.01)
 for yy in [-.267,.267]:box('pan end',(x,yy,.78),(.546,.012,.15),steel,g,.01)
for x in [-.565,.565]:
 tube('sneeze guard legs',[(x,.285,.86),(x,.29,1.49),(x,-.16,1.19)],.014,chrome,g,8);bolt(g,(x,-.18,1.19))
box('raised back shelf',(0,.245,1.50),(w,.23,.027),steel,g,.005)
o=box('angled clear safety glass',(0,.065,1.348),(w-.04,.51,.007),glass,g,.001);o.rotation_euler.x=math.radians(34)
for x in [-.565,.565]:tube('glass side frame',[(x,-.147,1.206),(x,.273,1.49)],.009,chrome,g)
tube('glass front trim',[(-.58,-.147,1.206),(.58,-.147,1.206)],.009,chrome,g)
finish(g,145,150)
g='PanLid';box('domed pan lid',(0,0,.015),(.52,.51,.028),steel,g,.014,4);tube('pan lid handle',[(-.045,0,.028),(-.045,0,.064),(.045,0,.064),(.045,0,.028)],.006,chrome,g);finish(g,1.8,18)
# Air-cooled maker and insulated bin match the ice unit in the entry photo.
g='IceMachine'
for x in [-.35,.35]:
 for y in [-.28,.28]:cyl('leveling foot',(x,y,.06),.026,.12,rubber,g)
box('insulated bin',(0,0,.61),(.83,.75,1.03),steel,g,.018,4)
o=box('dark sloped bin lid',(0,-.33,1.02),(.73,.045,.40),plastic,g,.023,4);o.rotation_euler.x=math.radians(-20)
tube('bin lid grip',[(-.23,-.419,.915),(.23,-.419,.915)],.016,plastic,g,10)
box('ice maker upper',(0,.015,1.455),(.84,.72,.55),steel,g,.014,4);box('upper front stainless face',(0,-.354,1.455),(.77,.018,.43),steel,g,.008)
box('digital bezel',(-.27,-.37,1.53),(.088,.012,.07),plastic,g,.004);box('blue lcd',(-.27,-.379,1.54),(.061,.004,.029),blue,g,.003);text('machine temp','ICE',(-.27,-.383,1.522),.015,paper,g)
for i in range(15):box('side ventilation slot',(.427,.015,1.38+i*.015),(.004,.38,.006),black,g,.001)
for c in [(-.385,-.372,1.24),(.385,-.372,1.68)]:bolt(g,c)
text('maker plate','ICE SYSTEM',(0,-.371,1.34),.024,plastic,g);finish(g,155,180)
g='IceMachinePallet'
for x in [-.30,0,.30]:box('pallet bearer',(x,0,.042),(.09,.79,.084),wood,g)
for y in [-.30,-.10,.10,.30]:box('pallet plank',(0,y,.097),(.87,.17,.033),wood,g,.003)
finish(g,12,35)
# Molded utility sink, exposed drain and faucet; basin is genuinely hollow.
g='UtilitySink'
for x in [-.245,.245]:
 for y in [-.205,.205]:tube('utility sink leg',[(x*1.06,y*1.06,0),(x,y,.69)],.020,white,g,10)
box('basin bottom',(0,0,.54),(.47,.40,.025),white,g,.04,4)
for x in [-.265,.265]:box('molded basin side',(x,0,.664),(.045,.47,.24),white,g,.018,4)
for y in [-.23,.23]:box('molded basin wall',(0,y,.664),(.55,.045,.24),white,g,.018,4)
for y in [-.252,.252]:box('rounded lip',(0,y,.799),(.61,.04,.035),white,g,.015)
for x in [-.285,.285]:box('rounded lip',(x,0,.799),(.04,.48,.035),white,g,.012)
box('faucet deck',(0,.23,.814),(.42,.10,.026),white,g)
tube('arched faucet',[(0,.22,.83),(0,.22,.98),(0,.16,1.04),(0,.045,1.01),(0,.045,.955)],.010,chrome,g,10)
for x in [-.08,.08]:cyl('tap valve',(x,.22,.85),.018,.052,chrome,g);tube('tap handle',[(x-.029,.22,.877),(x+.029,.22,.877)],.005,chrome,g)
cyl('drain strainer',(0,0,.555),.027,.005,chrome,g)
tube('visible pvc trap',[(0,0,.54),(0,0,.36),(0,.08,.32),(0,.19,.36),(0,.19,.44),(0,.30,.44)],.024,white,g,10)
for x in [-.05,.05]:tube('supply pipe',[(x,.20,.825),(x,.24,.40),(x,.28,.22)],.007,chrome,g)
finish(g,35,58)
g='TopFreezerFridge';box('refrigerator case',(0,0,.872),(.73,.70,1.70),white,g,.02,5)
for x in [-.285,.285]:
 for y in [-.245,.245]:cyl('adjustable foot',(x,y,.028),.022,.045,rubber,g)
box('freezer gasket',(0,-.358,1.425),(.713,.018,.532),rubber,g,.012);box('freezer door',(0,-.382,1.425),(.705,.046,.52),white,g,.02,5)
box('fridge gasket',(0,-.357,.585),(.713,.018,1.12),rubber,g,.012);box('fridge door',(0,-.382,.585),(.705,.046,1.107),white,g,.02,5)
for z,h in [(1.44,.235),(.79,.47)]:tube('curved silver handle',[(-.25,-.41,z-h/2),(-.25,-.464,z-h*.35),(-.25,-.472,z+h*.35),(-.25,-.41,z+h/2)],.012,chrome,g,10)
box('brand badge',(.23,-.409,1.575),(.095,.005,.025),steel,g,.002);text('brand','COLD',(.23,-.413,1.568),.014,plastic,g)
for c in [(-.20,-.408,1.16),(.18,-.408,1.15)]:box('worn small magnet sticker',c,(.018,.004,.04),bluepack,g,.001)
finish(g,92,135)
g='TennesseeCoolerBag';box('insulated soft cooler',(0,0,.19),(.42,.32,.38),white,g,.065,6);flat('orange T face',(0,-.164,.24),.33,.22,labels['CerealLabel'],g,True)
tube('carry handle',[(-.11,0,.36),(-.10,0,.43),(.10,0,.43),(.11,0,.36)],.009,paper,g);tube('stitched seams',[(-.18,-.13,.04),(-.18,-.13,.33),(.18,-.13,.33),(.18,-.13,.04)],.003,white,g);finish(g,2,18)
# Stainless prep rack and black media rack have open shelves.
for g,wide,deep,h,m in [('StainlessApplianceRack',.98,.60,.87,steel),('MicrowaveCart',.67,.39,.80,black)]:
 for x in [-wide/2+.025,wide/2-.025]:
  for y in [-deep/2+.025,deep/2-.025]:box('square leg',(x,y,h/2),(.029,.029,h),m,g,.003);cyl('rubber cap',(x,y,.014),.02,.022,rubber,g)
 for z in [.09,.39,h]:box('shelf',(0,0,z),(wide,deep,.026),m,g,.004)
 for z in [.09,.39]:
  for y in [-deep/2,deep/2]:box('shelf skirt',(0,y,z-.023),(wide,.014,.04),m,g,.002)
 finish(g,24 if wide>.7 else 14,45)
for g,col in [('WhiteMicrowave',white),('SilverMicrowave',steel)]:
 box('microwave body',(0,0,.15),(.48,.34,.285),col,g,.012)
 box('front surround',(0,-.177,.15),(.48,.025,.26),plastic,g,.008);box('dark glazed door',(-.05,-.192,.15),(.335,.008,.217),screen,g,.01)
 for i in range(7):box('keypad row',(.187,-.194,.11+i*.014),(.042,.003,.008),white,g,.001)
 box('digital display',(.185,-.196,.218),(.053,.002,.026),black,g,.001);text('clock','12:00',(.185,-.199,.21),.011,blue,g)
 tube('door grip',[(-.215,-.201,.045),(-.215,-.221,.08),(-.215,-.221,.22),(-.215,-.201,.25)],.007,col,g)
 for i in range(12):box('air vent',(0,.172,.10+i*.008),(.28,.003,.003),black,g,.001)
 finish(g,12,30)
g='AirFryer';box('round appliance',(0,0,.16),(.24,.27,.31),plastic,g,.055,6);box('steel drawer',(0,-.129,.095),(.21,.02,.12),steel,g,.021);box('drawer handle',(0,-.161,.12),(.10,.03,.03),plastic,g,.01);box('display',(0,-.128,.25),(.09,.01,.03),screen,g,.004);text('air fryer temperature','400',(0,-.138,.24),.018,blue,g);finish(g,5,22)
g='Toaster';box('toaster shell',(0,0,.09),(.26,.18,.18),plastic,g,.025,5)
for y in [-.05,.05]:box('bread slot',(0,y,.184),(.20,.026,.006),black,g,.006)
cyl('browning dial',(.132,0,.078),.017,.009,chrome,g,20,'X');box('lever',(.14,-.04,.14),(.018,.035,.018),white,g,.004);finish(g,2.5,18)
g='TV';box('flat television',(0,0,0),(1.12,.058,.66),plastic,g,.014);box('reflective screen',(0,-.034,0),(1.075,.009,.609),screen,g,.007);box('tv wall mount',(0,.055,0),(.30,.072,.29),black,g,.005);ell('standby led',(.42,-.04,-.316),(.003,.002,.002),blue,g);finish(g,16,65,wall=True)
g='SmallBlackCabinet';box('black supply cabinet',(0,0,.25),(.44,.41,.50),black,g,.009)
for z in [.11,.27,.425]:box('recessed drawer',(0,-.21,z),(.408,.012,.135),plastic,g,.005);box('label slot',(0,-.22,z),(.08,.004,.032),steel,g,.002)
finish(g,22,45)
g='TowerFan';cyl('fan round foot',(0,0,.016),.115,.029,plastic,g);box('tower',(0,0,.39),(.125,.115,.73),plastic,g,.043,6)
for i in range(38):box('grille horizontal louver',(0,-.059,.10+i*.014),(.083,.006,.002),steel,g,.0005)
cyl('top control',(0,0,.76),.018,.004,black,g);finish(g,4,25)
g='TrashCan';cyl('trash body',(0,0,.24),.135,.48,steel,g,40);cyl('black rim',(0,0,.483),.139,.017,black,g,40);cyl('open dark well',(0,0,.482),.116,.018,plastic,g,40);finish(g,4,20)
# Coolers, black stools, supply carton and bags from the dining end wall.
for g,w,col in [('WhiteCooler',.84,white),('RollingBlueCooler',.54,blueplastic)]:
 box('cooler body',(0,0,.20),(w,.39,.36),col,g,.045,5);box('thick cooler lid',(0,0,.403),(w+.018,.409,.065),white,g,.024,4)
 for x in [-w*.31,w*.31]:box('latch',(x,-.207,.355),(.045,.018,.061),plastic,g,.005)
 for x in [-w/2-.009,w/2+.009]:tube('side carry handle',[(x,-.08,.31),(x,-.08,.24),(x,.08,.24),(x,.08,.31)],.010,col,g)
 if g.startswith('Rolling'):
  for x in [-.24,.24]:cyl('cooler wheel',(x,.16,.044),.061,.026,rubber,g,20,'X')
  tube('telescopic trolley', [(-.13,.195,.39),(-.13,.195,.69),(.13,.195,.69),(.13,.195,.39)],.011,black,g)
 finish(g,18 if w>.7 else 10,35)
g='BlackBarStool';box('stool seat',(0,0,.715),(.32,.32,.045),plastic,g,.02)
for x in [-.12,.12]:
 for y in [-.12,.12]:tube('splayed stool leg',[(x*1.4,y*1.4,0),(x,y,.69)],.017,black,g,8)
for y in [-.145,.145]:tube('stool rung',[(-.145,y,.26),(.145,y,.26)],.009,black,g)
for x in [-.145,.145]:tube('stool rung',[(x,-.145,.25),(x,.145,.25)],.009,black,g)
finish(g,6,22)
def carton(g,w=.34,h=.24):
 box('cardboard bottom',(0,0,.008),(w,.27,.016),cardboard,g,.002)
 for x in [-w/2,w/2]:box('box side',(x,0,h/2),(.012,.28,h),cardboard,g,.001)
 for y in [-.135,.135]:box('carton side',(0,y,h/2),(w,.01,h),cardboard,g,.001)
 for x in [-w/2,w/2]:o=box('open folded flap',(x,0,h+.035),(.12,.27,.008),cardboard,g,.001);o.rotation_euler.y=(-1 if x<0 else 1)*.75
carton('SuppliesBox')
for i in range(5):
 x=(i%3-.8)*.085;y=(i//3-.5)*.1
 for z in [.10,.11,.12,.13]:cyl('nested white bowls',(x,y,z),.05,.012,paper,'SuppliesBox',20)
for x in [-.10,.04]:cyl('paper roll',(x,.06,.22),.035,.26,paper,'SuppliesBox');cyl('roll core',(x,.06,.351),.012,.002,cardboard,'SuppliesBox')
finish('SuppliesBox',6,22)
g='PaperGroceryBag';box('filled brown bag',(0,0,.15),(.22,.13,.30),cardboard,g,.017)
for y in [-.066,.066]:tube('bag cord handle',[(-.05,y,.29),(-.045,y,.39),(.045,y,.39),(.05,y,.29)],.003,cardboard,g)
finish(g,2,16)
g='BlackBag';ell('soft black tote',(0,0,.16),(.18,.12,.18),plastic,g)
for y in [-.07,.07]:tube('tote handle',[(-.07,y,.24),(-.065,y,.40),(.065,y,.40),(.07,y,.24)],.006,black,g)
finish(g,3,18)
# Sculpted fall flower arrangements. Petals are curved, stems and leaves distinct.
def blossom(g,c,r,m):
 cx,cy,cz=c
 for i in range(12):
  a=i*math.tau/12;dx,dy=math.cos(a),math.sin(a);side=Vector((-dy,dx,0));base=Vector(c);v=[tuple(base+Vector((dx,dy,0))*t*r+side*s*r*.23+Vector((0,0,math.sin(t*math.pi)*r*.28)))for t,s in [(0,0),(.5,-1),(1,0),(.5,1)]];mesh('curved fabric petal',v,[(0,1,2,3)],m,g)
 ell('seed head',(cx,cy,cz+.008),(r*.29,r*.29,.020),dark,g)
g='AutumnBouquet';cyl('burlap vase',(0,0,.13),.105,.25,linen,g,24)
for i in range(20):
 a=i*math.tau/20;x,y=.103*math.cos(a),.103*math.sin(a);tube('burlap folds',[(x*.8,y*.8,0),(x*1.08,y*1.08,.18),(x*.65,y*.65,.27)],.007,linen,g)
for i in range(11):
 a=i*2.399;x,y=math.cos(a)*(.045+.025*(i%3)),math.sin(a)*(.045+.027*(i%3));z=.34+(i%4)*.034;tube('flower stems',[(0,0,.14),(x*.6,y*.6,.28),(x,y,z)],.0035,leaf,g);blossom(g,(x,y,z),.049 if i%3 else .06,gold if i%2 else rust)
 for k in range(2):
  c=Vector((x*.8,y*.8,.27+k*.06));d=Vector((math.cos(a+.5*k),math.sin(a+.5*k),.3))*.07;mesh('pointed autumn leaf',[tuple(c),tuple(c+d*.5+Vector((-.013,.018,0))),tuple(c+d),tuple(c+d*.5-Vector((-.013,.018,0)))],[(0,1,2,3)],leaf if i%2 else rust,g)
tube('raffia bow',[(-.06,-.103,.23),(0,-.12,.20),(.09,-.13,.22),(.04,-.15,.18),(0,-.12,.20),(-.07,-.14,.16)],.008,linen,g);finish(g,2.5,20)
# Table and serving clutter remains separate and throwable.
g='PaperTowelRoll';cyl('embossed towel cylinder',(0,0,.145),.049,.29,towel,g,32);cyl('cardboard core',(0,0,.292),.019,.004,cardboard,g,24);cyl('dark core opening',(0,0,.295),.011,.004,dark,g,20);o=flat('loose towel tail',(.071,0,.15),.047,.22,towel,g,True);o.rotation_euler.z=.3;finish(g,.45,12)
g='CrumpledNapkin';vv=[];ff=[]
for j in range(6):
 for i in range(7):vv.append(((i/6-.5)*.16,(j/5-.5)*.13,.008+.012*math.sin(i*1.4)*math.cos(j*1.1)+.008*rng.random()))
for j in range(5):
 for i in range(6):k=j*7+i;ff.append((k,k+1,k+8,k+7))
mesh('creased paper',vv,ff,paper,g);finish(g,.12,8)
g='ClearCup';cyl('clear plastic tumbler',(0,0,.064),.037,.125,wrap,g,24);cyl('rolled cup rim',(0,0,.13),.039,.006,white,g,24);finish(g,.15,12)
g='BreadBag';ell('sliced loaf',(0,0,.055),(.18,.073,.05),soil,g)
for i in range(13):tube('cut slice crease',[(-.15+i*.024,-.067,.056),(-.15+i*.024,0,.10),(-.15+i*.024,.067,.056)],.0016,crumb,g)
ell('crinkled clear bread wrap',(0,0,.058),(.197,.087,.062),wrap,g);box('bag label',(0,-.083,.049),(.075,.002,.049),bluepack,g,.001);text('loaf label','BREAD',(0,-.086,.044),.014,paper,g);tube('twist tie',[(.19,-.013,.06),(.21,0,.06),(.20,.013,.06)],.003,orange,g);finish(g,.7,14)
for g,tex,w,h in [('FoilCarton','FoilBox',.31,.054),('StorageBagCarton','ZipBox',.31,.072),('SnackVarietyBox','SnackBox',.37,.37),('PaperProductPack','PaperPack',.28,.16)]:
 box('printed cardboard box',(0,0,h/2),(w,.14 if h<.1 else .22,h),paper,g,.003);flat('printed package face',(0,-(.073 if h<.1 else .113),h/2),w*.97,h*.91,labels[tex],g,True);finish(g,.5 if h<.1 else 3,15)
g='CoffeePump';cyl('insulated black airpot',(0,0,.18),.083,.36,plastic,g,32);cyl('lid',(0,0,.375),.091,.035,black,g);box('pump lever',(0,-.012,.411),(.075,.11,.02),plastic,g,.008);tube('pouring spout',[(0,-.075,.36),(0,-.12,.36),(0,-.12,.337)],.014,plastic,g);finish(g,2.5,20)
g='CerealCanister';cyl('clear cereal hopper',(0,0,.19),.075,.33,wrap,g,32);cyl('black lid',(0,0,.367),.081,.027,plastic,g)
for i in range(90):
 a=rng.uniform(0,math.tau);r=.062*math.sqrt(rng.random());z=rng.uniform(.045,.27);ell('cereal flake',(r*math.cos(a),r*math.sin(a),z),(.006,.005,.004),crumb,g)
flat('orange Tennessee label',(0,-.077,.16),.082,.07,labels['CerealLabel'],g,True);finish(g,1.8,18)
g='WireBasket'
for z in [.015,.07,.14]:tube('basket rim',[(-.11,-.08,z),(.11,-.08,z),(.11,.08,z),(-.11,.08,z)],.0025,chrome,g,6,True)
for i in range(10):x=-.1+i*.022;tube('basket grid',[(x,-.08,.14),(x,-.08,.015),(x,.08,.015),(x,.08,.14)],.002,chrome,g,6)
for i in range(6):y=-.075+i*.03;tube('basket grid',[(-.11,y,.14),(-.11,y,.015),(.11,y,.015),(.11,y,.14)],.002,chrome,g,6)
finish(g,.8,18)
g='CondimentBottle';cyl('bottle',(0,0,.08),.027,.15,orange,g,24);cyl('bottle neck',(0,0,.17),.015,.04,orange,g);cyl('black screw cap',(0,0,.193),.018,.014,black,g);finish(g,.4,12)
g='Saucepan';cyl('pan body',(0,0,.05),.08,.10,steel,g,32);tube('pan handle',[(.065,0,.08),(.19,0,.09)],.011,black,g);finish(g,1.5,18)
g='WaterPitcher';cyl('green translucent pitcher',(0,0,.13),.065,.26,green,g);tube('jug handle',[(.06,0,.23),(.10,0,.23),(.10,0,.06),(.06,0,.06)],.008,green,g);finish(g,1,16)

g='FruitTray'
box('foil tray bottom',(0,0,.009),(.50,.31,.018),steel,g,.02)
for y in [-.165,.165]:box('crimped foil edge',(0,y,.05),(.53,.014,.09),steel,g,.007)
for x in [-.265,.265]:box('crimped foil edge',(x,0,.05),(.014,.33,.09),steel,g,.007)
for i in range(30):
 x=rng.uniform(-.21,.21);y=rng.uniform(-.12,.12);o=box('cut melon chunk',(x,y,.03+rng.random()*.025),(.032,.031,.034),orange if i%3 else gold,g,.006);o.rotation_euler.z=rng.random()*6.28
finish(g,1.2,16)
g='ServingBowl';cyl('ceramic bowl base',(0,0,.024),.045,.04,white,g)
for i in range(4):cyl('curved bowl side',(0,0,.038+i*.008),.06+i*.004,.011,white,g,32)
cyl('open bowl hollow',(0,0,.069),.06,.009,crumb,g,32);finish(g,.6,14)
g='UtensilCup';cyl('silver utensil holder',(0,0,.07),.037,.14,steel,g)
for i in range(8):
 a=i*.79;x,y=.023*math.cos(a),.023*math.sin(a);tube('utensil handle',[(x,y,.05),(x*1.6,y*1.6,.23)],.0024,chrome,g);ell('spoon bowl',(x*1.6,y*1.6,.245),(.008,.003,.015),chrome,g)
finish(g,.8,18)
g='GroceryBag';ell('wrinkled white grocery bag',(0,0,.045),(.105,.072,.045),paper,g)
for y in [-.045,.045]:tube('grocery bag loop',[(-.05,y,.054),(-.053,y,.092),(.025,y,.094),(.04,y,.055)],.006,paper,g)
finish(g,.4,12)

# Wall art uses actual photo crops within modelled frames.
for g,tex,w,h,m in [('CityWallPrint','CityPrint',.46,.34,black),('CafeWallPrint','CafePrint',.29,.43,goldframe),('RulesWallPrint','RulesPrint',.24,.34,goldframe)]:
 box('picture back',(0,0,0),(w,.028,h),wood,g,.003);flat('reference photograph',(0,-.017,0),w-.045,h-.045,arts[tex],g,True)
 for x in [-w/2,w/2]:box('mitered frame side',(x,-.014,0),(.024,.022,h+.02),m,g,.004)
 for z in [-h/2,h/2]:box('mitered frame rail',(0,-.014,z),(w,.022,.024),m,g,.004)
 finish(g,1.8,25,wall=True)
g='Outlet';box('ivory outlet plate',(0,0,0),(.069,.008,.113),white,g,.007)
for z in [-.022,.022]:
 for x in [-.009,.009]:box('outlet slot',(x,-.005,z),(.003,.002,.01),black,g,.0005)
 cyl('ground hole',(0,-.005,z-.013),.003,.002,black,g,12,'Y')
bolt(g,(0,-.006,0),.002);finish(g,.25,12,wall=True)
g='PowerCord';pts=[(0,0,.012),(.09,.04,.012),(.30,.065,.011),(.38,.12,.010),(.31,.27,.01),(.16,.32,.01),(.04,.26,.01),(.10,.08,.01),(.34,-.05,.01),(.50,-.015,.01)];tube('loose cord loop',pts,.004,white,g,8);box('plug',(.5,-.015,.012),(.027,.022,.019),white,g,.004);finish(g,.3,12)
g='HangingTVCables';tube('tv cord sag',[(0,0,0),(0,-.025,-.16),(.03,-.04,-.42),(.12,-.03,-.62),(.14,0,-.68)],.003,black,g,7);tube('signal loop',[(.05,0,-.01),(.09,-.04,-.18),(.17,-.055,-.24),(.21,0,-.04)],.002,black,g);finish(g,.3,12,wall=True)
# Slatted blinds: actual curved slats, ladder cords, sill-fitted outer frame.
g='WindowBlinds'
for i in range(36):
 z=.06+i*.04;o=box('tilted ivory blind slat',(0,0,z),(.70,.024,.012),white,g,.004,2);o.rotation_euler.x=math.radians(16)
for x in [-.26,.26]:
 tube('front ladder cord',[(x,-.017,.04),(x,-.017,1.50)],.0018,paper,g,6);tube('back ladder cord',[(x,.012,.04),(x,.012,1.50)],.0018,paper,g,6)
 for i in range(36):tube('ladder rung',[(x,-.017,.06+i*.04),(x,.012,.06+i*.04)],.001,paper,g,5)
box('headrail',(0,0,1.53),(.73,.055,.055),white,g,.011);box('bottomrail',(0,0,.043),(.71,.035,.026),white,g,.009);tube('pull cord',[(.34,-.01,1.52),(.35,-.015,.35)],.0017,paper,g);ell('pull tassel',(.35,-.015,.32),(.008,.008,.02),white,g);finish(g,2,25)
# Closed six panel leaf in world source coordinates for ExistingHouseDoor.
g='ClosedClosetDoor';cx,cy=11.442,-.58
box('closed door slab',(cx,cy,1.105),(.042,.92,2.20),white,g,.007)
for yy in [cy-.225,cy+.225]:
 for z,h in [(.35,.48),(1.08,.65),(1.86,.40)]:
  box('raised beveled panel',(cx+.024,yy,z),(.013,.345,h),white,g,.006)
  for y in [yy-.182,yy+.182]:box('panel molding stile',(cx+.030,y,z),(.008,.015,h+.03),paper,g,.004)
  for zz in [z-h/2-.01,z+h/2+.01]:box('panel molding rail',(cx+.030,yy,zz),(.008,.375,.018),paper,g,.004)
for z in [.25,1.09,1.96]:cyl('hinge knuckle',(cx+.028,cy-.46,z),.011,.08,chrome,g,20)
cyl('round knob rose',(cx+.036,cy+.355,1.02),.031,.012,chrome,g,24,'X');ell('silver closet knob',(cx+.058,cy+.355,1.02),(.025,.032,.023),chrome,g)
a=finish(g,damage=False);a['behavior']='door';a['hinge']=[cx*180,(.58-.46)*180,0];a['doorway_center']=[cx*180,.58*180,110];a['closed_yaw']=0;a['open_yaw']=-90;a['panel_center']=[cx*180,.58*180,110];a['panel_half_extent']=[4,83,110]
# Restore original local coords: Native door multiplies original world mesh XY by1.8.
bpy.ops.import_scene.fbx(filepath=a['intact_fbx']);o=next(o for o in bpy.context.selected_objects if o.type=='MESH');bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
for v in o.data.vertices:v.co.x/=1.8;v.co.y/=1.8
bpy.ops.export_scene.fbx(filepath=a['intact_fbx'],use_selection=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False);bpy.data.objects.remove(o,do_unlink=True)
# Thin visual floor overlay; existing floor collision and room boundaries unchanged.
g='KitchenOakFloor'
for x0,x1,y0,y1 in [(7.38,16.24,-7.0,-3.38),(11.53,16.24,-3.38,.74),(12.11,16.24,.74,3.80)]:box('reference oak floor',((x0+x1)/2,(y0+y1)/2,.003),((x1-x0),(y1-y0),.005),floor,g,0)
finish(g,damage=False);place(g,0,0)
# Detailed layout. World centimetres, as in the existing saved kitchen.
for i,y in enumerate([-205,101,407]):place('DiningTable',2735,y,0,90,'joined walnut dining table')
for yc in [-205,101,407]:
 for dy in [-97,0,97]:
  place('GreenSpindleChair',2591,yc+dy,0,-90,'green upholstered spindle chair');place('GreenSpindleChair',2879,yc+dy,0,90,'green upholstered spindle chair')
place('GreenSpindleChair',2735,-420,0,0);place('GreenSpindleChair',2735,622,0,180)
for x in [1825,2041,2257]:place('ServingCounter',x,1115,0,180,'stainless buffet and glass sneeze guard')
for x in [1771,1879,1987,2095]:place('PanLid',x,1115,86)
for x in [2150,2265,2320]:place('BreadBag',x,1077,87,rng.uniform(-20,20))
for x in [1727,1900,2100,2300]:place('AutumnBouquet',x,1160,151,0)
for x in [2215,2265,2300]:place('StorageBagCarton',x,1158,151,0)
place('FoilCarton',2280,1160,159);place('CoffeePump',2360,1160,151)
for x in [1730,1770,1810]:place('CerealCanister',x,1160,151)
place('PaperTowelRoll',2175,1160,151);place('SnackVarietyBox',1660,1060,0);place('SnackVarietyBox',1650,1100,0)
place('IceMachinePallet',1729,680,0);place('IceMachine',1729,680,11);place('SuppliesBox',1610,690,0)
place('UtilitySink',1995,655,0);place('TowerFan',2110,540,0,-90)
place('TopFreezerFridge',1410,927,0,-90);place('TennesseeCoolerBag',1410,927,175,-90)
place('StainlessApplianceRack',1410,1112,0,-90);place('WhiteMicrowave',1440,1150,41,-90);place('SuppliesBox',1355,1070,42)
place('AirFryer',1450,1140,88,-90);place('Toaster',1410,1050,88,-90);place('WireBasket',1410,1090,88);place('WaterPitcher',1410,1180,88)
for y in [1080,1160,1200]:place('ClearCup',1460,y,88)
place('CondimentBottle',1450,1130,88);place('TrashCan',1510,972,0);place('PowerCord',1540,1070,.2,14)
place('TV',2092,400,173,-90,'wall mounted television');place('HangingTVCables',2097,430,143,-90)
place('MicrowaveCart',2140,390,0,-90);place('SilverMicrowave',2140,390,83,-90);place('AutumnBouquet',2140,390,113)
for y in [361,419]:place('PaperProductPack',2140,y,42,-90)
place('Saucepan',2140,388,12);place('SmallBlackCabinet',2140,18,0,-90);place('PowerCord',2150,530,.2,-36)
place('WhiteCooler',2290,-625,0);place('RollingBlueCooler',2455,-625,0)
for x in [2570,2640]:place('BlackBarStool',x,-630,0)
place('PaperGroceryBag',2660,-613,73);place('BlackBag',2640,-610,72);place('SuppliesBox',2190,-630,0);place('SnackVarietyBox',2365,-610,0);place('PowerCord',2195,-572,.2)
for y in [-625,-530]:place('GreenSpindleChair',2820,y,0,0,'stacked spare chair')
for y in [-205,407]:place('AutumnBouquet',2735,y,78);place('CrumpledNapkin',2765,y+74,78);place('ClearCup',2700,y+35,78)
for y in [89,112,138]:place('PaperTowelRoll',2740,y,78)
place('CityWallPrint',2910,650,176,90);place('CafeWallPrint',2410,1247,183,180);place('RulesWallPrint',2100,1247,190,180);place('CafeWallPrint',1880,618,184,0)
# North-window blinds and east-wall blinds retain their matching existing glass.
for x,z in [(1660,53),(2660,43)]:place('WindowBlinds',x,1254,z)
for y in [-326,340,997]:place('WindowBlinds',2920,y,46,90)
for x,y,z,yaw in [(2067,480,58,-90),(2067,195,53,-90),(1425,1255,58,180),(1480,1255,118,180),(1820,618,89,0)]:place('Outlet',x,y,z,yaw)
# Entry exit sign, recessed fire cabinet and wall switch are modelled individually.
g='ExitSign';box('exit sign enclosure',(0,0,0),(.40,.045,.16),white,g,.012);box('white luminous face',(0,-.027,0),(.37,.006,.134),paper,g,.004);text('red exit','EXIT',(0,-.032,-.038),.11,red,g);finish(g,3,26,wall=True);place(g,1500,619,235,0)
g='FireCabinet';box('recessed red cabinet',(0,0,0),(.24,.025,.38),rust,g,.005);box('dark glass inset',(0,-.016,0),(.19,.006,.31),screen,g,.003);cyl('extinguisher cylinder',(0,-.03,-.045),.045,.19,rust,g);box('cabinet handle',(.08,-.04,0),(.012,.016,.10),chrome,g,.004);finish(g,7,30,wall=True);place(g,1540,619,135,0)
place('FruitTray',2310,1115,87);place('ServingBowl',1910,1065,87);place('ServingBowl',2005,1070,87);place('UtensilCup',1700,1080,87)
place('GroceryBag',1450,1175,88);place('ClearCup',2070,1060,87);place('PaperProductPack',1780,1065,87);place('ServingBowl',1905,1155,151)
# Metadata supports safe idempotent installation.
(R/'manifest.json').write_text(json.dumps(manifest,indent=2));print('KITCHEN_KIT_READY',len(manifest['assets']),len(manifest['placements']),flush=True)
