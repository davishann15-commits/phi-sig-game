from pathlib import Path
p=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BoywickBedroom/source/build_room.py')
exec(compile(p.read_text().split('# A single black steel loft frame')[0],str(p),'exec'))
doc=json.loads((R/'manifest.json').read_text())
g='AirPurifier';cyl('white purifier body',(0,0,.13),.086,.26,white,g,32);cyl('dark top grille',(0,0,.263),.072,.006,black,g,32)
for i in range(10):
 for j in range(14):a=j*math.tau/14;ell('perforated air intake',(.087*math.cos(a),.087*math.sin(a),.026+i*.012),(.002,.002,.003),grey,g)
finish(g,1.5,10,scale=1.3);place(g,-2920,-1065,76,0,'Desk air purifier')
g='TissueBox';box('tissue box',(0,0,.045),(.15,.12,.09),white,g,.009);box('oval tissue opening',(0,0,.091),(.10,.028,.002),grey,g,.012);cloth(g,'protruding tissue',(0,0,.117),.10,.08,paper,.026,3);finish(g,.22,5,scale=1.3);place(g,-2930,-1004,76,90,'Tissue box')
g='DeskPhoto';photo_panel(g,2327,[(.121,.469),(.228,.449),(.24,.519),(.139,.546)],.15,.10)
for x in [-.083,.083]:box('photo frame',(x,-.007,0),(.015,.012,.124),white,g,.003)
for z in [-.056,.056]:box('photo frame',(0,-.007,z),(.178,.012,.016),white,g,.003)
box('frame prop stand',(0,.035,-.034),(.08,.067,.012),black,g,.003);finish(g,.25,6,scale=1.4);place(g,-2940,-969,84,90,'Desk photograph')
g='TowerFan';cyl('fan base',(0,0,.018),.13,.035,black,g,32);box('tower fan body',(0,0,.48),(.14,.115,.89),black,g,.054,6)
for i in range(36):box('fan grille slat',(0,-.058,.14+i*.019),(.088,.004,.008),grey,g,.003)
box('fan control panel',(0,-.06,.862),(.063,.003,.035),screen,g,.008);finish(g,4.5,23,damage=False,scale=1.3);place(g,-2930,-1110,0,90,'Black tower fan')
g='HangingLaundryBag';o=cloth(g,'silver laundry bag front',(0,0,.33),.23,.55,cotton,.025,7,True);cloth(g,'laundry bag back',(0,.055,.33),.23,.55,grey,.018,6,True);tube('drawstring cord',[(-.08,0,.57),(0,0,.69),(.08,0,.57)],.0025,white,g);finish(g,.6,6,wallmount=True,scale=1.5);place(g,-2795,-1097,75,0,'Hanging laundry bag')
g='ChairJacket';cloth(g,'black jacket hanging over chair',(0,0,.30),.44,.59,fabric,.033,6,True);cloth(g,'fold over chair top',(0,.06,.59),.43,.12,fabric,.018,5);finish(g,.9,8,wallmount=True,scale=1.3);place(g,-2759,-1004,35,90,'Jacket draped over desk chair')
g='MilkCarton';box('blue drink carton',(0,0,.070),(.058,.062,.14),blue,g,.002);box('white carton top',(0,0,.146),(.058,.06,.02),paper,g,.004);cyl('screw top',(.009,0,.165),.011,.019,white,g,20);flat('carton front label',(0,-.032,.075),.04,.08,labelmats['PaperPack'],g,True);finish(g,.3,5,scale=1.25);place(g,-2745,-858,48,0,'Blue drink carton')
g='Wallet';box('leather wallet',(0,0,.012),(.11,.08,.024),mat('WalletLeather',(.072,.034,.019),.75),g,.008);finish(g,.16,5,scale=1.25);place(g,-2777,-914,49,-20,'Wallet')
g='Phone';box('phone case',(0,0,.005),(.075,.145,.01),black,g,.007);box('phone screen',(0,0,.011),(.069,.131,.001),screen,g,.005);finish(g,.20,8,scale=1.2);place(g,-2619,-856,48,22,'Phone')
g='PowerCords';tube('charger cord',[(0,0,.012),(.05,.018,.012),(.10,-.005,.012),(.16,.026,.012),(.23,.045,.012),(.24,.14,.01),(.12,.23,.015),(-.04,.17,.012)],.0024,black,g,6);box('charging brick',(-.027,0,.014),(.046,.037,.026),black,g,.005);finish(g,.25,5,scale=1.3);place(g,-2604,-877,48,-80,'Phone charger')
g='PowerOutlet';box('outlet wall plate',(0,0,0),(.077,.011,.119),white,g,.006)
for z in [-.026,.026]:
 for x in [-.010,.010]:box('plug socket slot',(x,-.006,z),(.003,.002,.012),grey,g,.001)
 cyl('earth pin',(0,-.007,z-.014),.0028,.002,grey,g,12,'Y')
finish(g,interactive=False,collision=False,scale=1);place(g,-2968,-1089,24,90,'Wall power outlet');place(g,-2449,-898,30,-90,'Closet wall outlet')
# Preserve the final orientation adjustments whenever this authoring chain is rerun.
for a in doc['placements']:
 if a['asset']=='GreyFuton':a['yaw']=0
 if a['asset']=='Laptop':a['yaw']=-18
 if a['asset']=='PatternedPillow':a['roll']=65;a['location'][1]=-646 if a['location'][0]<-2750 else-636;a['location'][2]=382.3
 if a['asset'].startswith('HangingShirt'):a['yaw']=0
 if a['asset']in['StorageBox','LongCarton']:a['yaw']=-90
(R/'manifest.json').write_text(json.dumps(doc,indent=2));print('BOYWICK_DETAILS_READY',len(doc['assets']),len(doc['placements']),flush=True)
