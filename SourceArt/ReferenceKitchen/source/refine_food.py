from pathlib import Path
p=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen/source/build_kitchen.py');s=p.read_text();a="""
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
"""
s=s.replace('# Wall art uses actual photo crops within modelled frames.',a+'\n# Wall art uses actual photo crops within modelled frames.')
s=s.replace("# Metadata supports safe idempotent installation.","""place('FruitTray',2310,1115,87);place('ServingBowl',1910,1065,87);place('ServingBowl',2005,1070,87);place('UtensilCup',1700,1080,87)
place('GroceryBag',1450,1175,88);place('ClearCup',2070,1060,87);place('PaperProductPack',1780,1065,87);place('ServingBowl',1905,1155,151)
# Metadata supports safe idempotent installation.""")
p.write_text(s)
