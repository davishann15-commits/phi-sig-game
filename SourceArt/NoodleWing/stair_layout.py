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
