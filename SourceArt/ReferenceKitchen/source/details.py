"""Additional observed kitchen details. Executed within the model authoring context."""
tan=mat('PeanutButter',(.39,.23,.10),.87)
apple=mat('RedAppleSkin',(.32,.027,.015),.28)
yellow=mat('YellowAppleSkin',(.45,.32,.08),.36)
podfoil=mat('CoffeePodFoil',(.23,.13,.045),.33,.40)
bluecloth=mat('BlueCleaningCloth',(.09,.17,.21),.96)

g='CoffeeMaker'
box('keurig curved case',(0,0,.20),(.235,.28,.34),plastic,g,.039,6)
box('water reservoir',(-.11,.05,.21),(.067,.20,.27),screen,g,.018,5)
box('base drip tray',(0,-.135,.035),(.235,.13,.032),plastic,g,.012,5)
for x in [-.085,-.056,-.028,0,.028,.056,.085]:box('drip tray grille',(x,-.148,.054),(.004,.10,.004),chrome,g,.001)
box('recessed cup space',(0,-.145,.16),(.14,.009,.17),black,g,.010)
cyl('coffee spout',(0,-.16,.245),.012,.028,chrome,g)
tube('raised lid handle',[(-.06,-.10,.361),(-.06,-.15,.374),(.06,-.15,.374),(.06,-.10,.361)],.006,chrome,g,10)
for x in [-.034,0,.034]:cyl('round top control',(x,-.01,.375),.010,.003,steel,g,16)
ell('blue brew light',(.09,-.125,.298),(.006,.003,.005),blue,g)
text('coffee maker brand','KEURIG',(0,-.146,.298),.015,paper,g)
finish(g,4,23,xy_scale=1.1)

g='WaterDispenser'
box('slim freestanding dispenser',(0,0,.53),(.295,.32,1.04),plastic,g,.037,6)
box('brushed stainless lower panel',(0,-.169,.36),(.24,.016,.48),steel,g,.018)
box('recessed dispenser cavity',(0,-.172,.76),(.22,.012,.27),black,g,.01)
for x,c in [(-.055,blue),(.055,red)]:
 box('tap lever',(x,-.185,.847),(.027,.036,.020),c,g,.006)
 tube('dispenser spout',[(x,-.177,.823),(x,-.214,.823),(x,-.214,.80)],.006,chrome,g,10)
box('drip tray',(0,-.191,.637),(.21,.09,.019),steel,g,.008)
for x in [-.08,-.04,0,.04,.08]:box('tray drainage slot',(x,-.204,.65),(.005,.055,.003),black,g,.001)
for x in [-.06,.06]:ell('status indicator',(x,-.166,.98),(.003,.002,.003),blue if x<0 else red,g)
finish(g,14,42,xy_scale=1.0)

g='PeanutButterJar'
cyl('peanut butter jar',(0,0,.066),.046,.127,tan,g,40)
cyl('red ridged screw lid',(0,0,.14),.048,.021,rust,g,40)
for i in range(40):
 a=i*math.tau/40;box('lid rib',(.048*math.cos(a),.048*math.sin(a),.14),(.002,.002,.018),rust,g,.0004)
flat('multicolour jar label',(0,-.047,.071),.074,.086,labels['PeanutLabel'],g,True)
finish(g,.8,15,xy_scale=1.0)

g='BasketWithApples'
for z in [.012,.08,.145]:tube('wire basket rim',[(-.13,-.095,z),(.13,-.095,z),(.13,.095,z),(-.13,.095,z)],.0025,chrome,g,8,True)
for i in range(12):
 x=-.12+i*.022;tube('wire basket strand',[(x,-.095,.145),(x,-.095,.012),(x,.095,.012),(x,.095,.145)],.002,chrome,g,8)
for i in range(8):
 x=(i%3-1)*.078;y=(i//3-1)*.063;z=.062+(i//6)*.052
 ell('irregular apple',(x,y,z),(.037,.035,.036),apple if i%3 else yellow,g)
 tube('apple stem',[(x,y,z+.029),(x+.004,y,z+.045)],.0018,wood,g,6)
finish(g,2.3,19,xy_scale=1.1)

g='CoffeePods'
for i in range(12):
 x=(i%4-1.5)*.042;y=(i//4-1)*.041;z=.022
 cyl('small coffee capsule',(x,y,z),.018,.035,paper,g,16)
 cyl('printed foil coffee lid',(x,y,z+.019),.020,.003,podfoil if i%2 else bluepack,g,16)
finish(g,.5,13,xy_scale=1.1)

for g,label,w,h in [('CocoaBox','CocoaLabel',.16,.205),('ForksBox','ForkLabel',.19,.07),('RedBullCase','EnergyLabel',.39,.18)]:
 box('printed carton',(0,0,h/2),(w,.17,h),paper,g,.004)
 flat('product front label',(0,-.087,h/2),w*.97,h*.95,labels[label],g,True)
 flat('product top label',(0,0,h+.002),w*.97,.165,labels[label],g)
 box('folded carton seam',(0,.01,h+.003),(w,.005,.002),cardboard,g,.0004)
 finish(g,5 if g=='RedBullCase' else .7,22 if g=='RedBullCase' else 14,xy_scale=1.1)

g='BreakfastSupplies'
carton(g,.41,.20)
for i in range(9):
 x=(i%3-1)*.108;y=(i//3-1)*.083;z=.12+(i%2)*.035
 box('individual wrapped breakfast bar',(x,y,z),(.09,.073,.09),paper,g,.004)
 flat('breakfast bar wrapper',(x,y-.038,z),.089,.08,labels['SnackBox'],g,True)
box('foam takeaway box',(0,0,.282),(.25,.17,.055),paper,g,.013,5)
tube('takeaway clamshell seam',[(-.12,-.08,.278),(.12,-.08,.278),(.12,.08,.278),(-.12,.08,.278)],.0016,white,g,8,True)
finish(g,3,20,xy_scale=1.1)

g='RectangularCerealCanister'
box('clear rectangular storage jar',(0,0,.18),(.15,.13,.34),wrap,g,.014,4)
box('black cereal jar lid',(0,0,.363),(.16,.14,.028),plastic,g,.012,5)
for i in range(140):
 x=rng.uniform(-.059,.059);y=rng.uniform(-.049,.049);z=rng.uniform(.025,.26)
 ell('individual cereal flakes',(x,y,z),(.005,.004,.003),crumb if i%3 else soil,g)
finish(g,1.5,18,xy_scale=1.1)

g='DoubleCerealDispenser'
box('steel dispenser stand',(0,0,.045),(.39,.24,.087),steel,g,.008)
for x in [-.105,.105]:
 cyl('transparent long cereal hopper',(x,0,.23),.083,.31,wrap,g,32)
 cyl('black canister top',(x,0,.395),.091,.026,plastic,g,32)
 cyl('black base cap',(x,0,.065),.079,.026,plastic,g,32)
 cyl('dispensing knob',(x,-.10,.12),.022,.025,plastic,g,20,'Y')
 for i in range(80):
  a=rng.random()*math.tau;rr=.065*math.sqrt(rng.random());ell('cereal pieces',(x+rr*math.cos(a),rr*math.sin(a),rng.uniform(.09,.26)),(.006,.005,.004),crumb,g)
finish(g,4.5,26,xy_scale=1.1)

g='ClosedStorageBox';box('sealed supply carton',(0,0,.16),(.40,.34,.32),cardboard,g,.002)
box('taped top',(0,0,.322),(.038,.345,.002),linen,g,0)
text('marker pen on carton','KITCHEN',(0,-.173,.19),.048,black,g)
text('marker pen supplies','SUPPLIES',(0,-.173,.12),.033,black,g)
finish(g,6,25,xy_scale=1.3)

g='NapkinBasket'
box('woven basket',(0,0,.065),(.27,.18,.13),linen,g,.014)
for z in [.025,.05,.075,.10,.125]:tube('woven horizontal strands',[(-.137,-.092,z),(.137,-.092,z),(.137,.092,z),(-.137,.092,z)],.0025,wood,g,7,True)
for i in range(5):box('stack of white napkins',((i-2)*.005,0,.139+i*.003),(.235,.145,.003),paper,g,.001)
finish(g,1.2,17,xy_scale=1.1)

g='CleaningCloth';v=[];f=[]
for j in range(9):
 for i in range(11):v.append(((i/10-.5)*.25,(j/8-.5)*.20,.009+.014*math.sin(i*.8)*math.cos(j*.7)))
for j in range(8):
 for i in range(10):k=j*11+i;f.append((k,k+1,k+12,k+11))
mesh('folded soft cleaning rag',v,f,bluecloth,g,True);finish(g,.25,12,xy_scale=1.0)

g='SmokeDetector';cyl('white ceiling smoke detector',(0,0,0),.073,.035,white,g,40,'Y')
for x in [-.04,-.02,0,.02,.04]:box('smoke detector vents',(x,-.02,0),(.007,.006,.057),black,g,.002)
ell('detector led',(.051,-.024,0),(.003,.003,.003),red,g)
finish(g,.4,14,wall=True,xy_scale=1.0)

g='ClosetWallDecoration'
for z,m in [(.06,bluepack),(0,paper),(-.06,rust)]:box('red white blue wall pennant',(0,0,z),(.045,.015,.064),m,g,.003)
text('small wall ornament','USA',(0,-.011,-.014),.027,black,g)
finish(g,.3,13,wall=True,xy_scale=1.0)

scuff=labeled('WallScuffs','WallScuffs');manifest['materials'][scuff.name]['opacity_mask']=str(R/'textures/WallScuffs_Mask.png')
g='WallWear';flat('scuffed and chipped paint',(0,0,.20),.7,.38,scuff,g,True);finish(g,damage=False)

g='TVCableWallHole';ell('dark uneven cable wall hole',(0,0,0),(.024,.007,.019),black,g)
for i in range(11):
 a=i*math.tau/11;ell('broken plaster hole rim',(.024*math.cos(a),-.001,.019*math.sin(a)),(.005,.005,.004),paper,g)
finish(g,damage=False,xy_scale=1.0)
