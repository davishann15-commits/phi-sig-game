# The scanned entry is the stem of a T: red north, blue south.
# Both framed openings share x=2458; the old east-facing red entry is closed.
ENTRY_X0=2366.;ENTRY_X1=2550.;ENTRY_H=235.
RED_ENTRY_Y=-917.;BLUE_ENTRY_Y=-370.

g='RedWalls'
for a,b in [
 ((2310,-1585),(3206,-1585)),((3206,-1585),(3206,-680)),
 ((2606,-680),(3206,-680)),
 ((2310,RED_ENTRY_Y),(ENTRY_X0,RED_ENTRY_Y)),
 ((ENTRY_X1,RED_ENTRY_Y),(2606,RED_ENTRY_Y)),
 ((2310,-1585),(2310,-1445)),((2310,-1255),(2310,-917)),
 ((2606,-917),(2606,-680))
]:wall(g,a,b,RED)
wall(g,(ENTRY_X0,RED_ENTRY_Y),(ENTRY_X1,RED_ENTRY_Y),RED,F+ENTRY_H,C,False)
wall(g,(2310,-1445),(2310,-1255),RED,F+230,C,False)
for lo,hi in [([2320,-1580,C-25],[3198,-1500,C]),([3110,-1500,C-25],[3198,-688,C]),([2615,-765,C-25],[3110,-688,C])]:bb(g,'boxed ceiling beam',lo,hi,CREAM)
save(g,'structure');place(g,g,(0,0,0))

g='BlueWalls'
for a,b in [
 ((2310,-370),(2310,424)),((3206,-370),(3206,442)),
 ((2606,433),(3206,433)),
 ((2310,BLUE_ENTRY_Y),(ENTRY_X0,BLUE_ENTRY_Y)),
 ((ENTRY_X1,BLUE_ENTRY_Y),(3206,BLUE_ENTRY_Y))
]:wall(g,a,b,BLUE)
wall(g,(ENTRY_X0,BLUE_ENTRY_Y),(ENTRY_X1,BLUE_ENTRY_Y),BLUE,F+ENTRY_H,C,False)
for y in [-290,80,350]:bb(g,'blue lounge ceiling beam',[2318,y-28,C-27],[3198,y+28,C],CREAM)
save(g,'structure');place(g,g,(0,0,0))

g='HallWalls'
for a,b in [
 ((1775,-669),(2310,-669)),((1775,-390),(2310,-390)),
 ((2310,-917),(2310,-669)),((2310,-390),(2310,-370)),
 ((2310,424),(2310,1214)),((2310,1214),(2606,1214)),
 ((2606,1195),(2606,1214)),((2606,-680),(2606,-370))
]:wall(g,a,b,CREAM)
# The retained basement doorway ends at z=-97.966 while the wing ceiling
# is z=-54. Extend its east face to the ceiling without lowering the opening.
# Match the existing wall depth and overlap its top by 2 mm to seal the seam.
bb(g,'flush original entrance upper infill',
   [1754.28,-678.15,-98.166],[1774.08,-381.,C+.2],CREAM)
# The original access cut also removed the old head jamb. Restore a 12 mm
# leaf clearance and line the opening; a continuous face conceals the join
# between the retained wall and the new ceiling-height extension.
ENTRY_TOP=-134.1
bb(g,'entry lower header infill',[1740.,-600.,ENTRY_TOP],[1774.08,-429.,-97.9],CREAM)
bb(g,'continuous flush entrance header face',
   [1774.08,-678.15,ENTRY_TOP],[1774.48,-381.,C+.2],CREAM)
bb(g,'lined entry head jamb',[1740.,-600.,ENTRY_TOP],[1776.,-429.,ENTRY_TOP+2.5],TRIM)
for y in [-600.,-429.]:
 box(g,'entry side casing',(1776.,y,(F+ENTRY_TOP)/2),(3,7,ENTRY_TOP-F+7),TRIM,.35)
box(g,'entry head casing',(1776.,-514.5,ENTRY_TOP+3.5),(3,178,7),TRIM,.35)
# Cream hall faces over the red/blue partitions. No second solid wall over
# either aperture; the actual collision follows the open framed doorway.
for y,face in [(RED_ENTRY_Y,RED_ENTRY_Y+T/2+.3),(BLUE_ENTRY_Y,BLUE_ENTRY_Y-T/2-.3)]:
 for xa,xb,z0 in [(2310,ENTRY_X0,F),(ENTRY_X1,2606,F),(ENTRY_X0,ENTRY_X1,F+ENTRY_H)]:
  bb(g,'cream corridor partition face',[xa,face-.25,z0],[xb,face+.25,C],CREAM)
  if z0==F:bb(g,'corridor skirting',[xa,face-1,F],[xb,face+1,F+9],TRIM)
bb(g,'closed old red entry corridor face',[2596.3,-917,F],[2596.8,-680,C],CREAM)
bb(g,'closed old entry skirting',[2595.7,-917,F],[2597.2,-680,F+9],TRIM)
save(g,'structure');place(g,g,(0,0,0))

g='BasementTrim'
# Retain the laundry and mechanical utility casings.
for x,y0,y1,h in [(2606,490,650,233),(2310,-1445,-1255,230)]:
 for y in [y0,y1]:box(g,'door architrave',(x,y,F+h/2),(24,9,h+8),TRIM,1)
 box(g,'door lintel',(x,(y0+y1)/2,F+h+3),(24,y1-y0+9,9),TRIM,1)
# Opposing doorways: lined jambs and casing on both room/hall faces.
for y in [RED_ENTRY_Y,BLUE_ENTRY_Y]:
 for x in [ENTRY_X0,ENTRY_X1]:
  box(g,'ivory opening jamb',(x,y,F+ENTRY_H/2),(3,T+2,ENTRY_H),TRIM,.4)
  for side in [-1,1]:box(g,'doorway side casing',(x,y+side*(T/2+1.3),F+ENTRY_H/2),(9,3,ENTRY_H+8),TRIM,.7)
 box(g,'lined opening header',((ENTRY_X0+ENTRY_X1)/2,y,F+ENTRY_H),(ENTRY_X1-ENTRY_X0,T+2,3),TRIM,.4)
 for side in [-1,1]:box(g,'doorway head casing',((ENTRY_X0+ENTRY_X1)/2,y+side*(T/2+1.3),F+ENTRY_H+3),(ENTRY_X1-ENTRY_X0+9,3,9),TRIM,.7)
 bb(g,'flush doorway threshold',[ENTRY_X0+1.5,y-T/2-.5,F+.13],[ENTRY_X1-1.5,y+T/2+.5,F+.3],WOOD,.08)
for z in [F+277,F+285]:tube(g,'service conduit',[(1800,-651,z),(2260,-651,z),(2287,-620,z),(2287,1110,z)],1.25,TRIM,12)
for y in range(-500,1100,180):box(g,'conduit saddle',(2288,y,F+281),(4,3,14),CHROME,.7)
save(g,'detail');place(g,g,(0,0,0))
