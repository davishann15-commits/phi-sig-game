"""Source-derived Fraternity Row plan; writes planning artifacts only, never Unreal assets."""
from pathlib import Path
import csv
import hashlib
import json
import math
from datetime import datetime, timezone
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
FONT = Path('/System/Library/Fonts/Supplemental/Arial.ttf')
BOLD = Path('/System/Library/Fonts/Supplemental/Arial Bold.ttf')
def font(size, bold=False):
    return ImageFont.truetype(str(BOLD if bold else FONT), size)

houses = [
    dict(id='R1', existing=True, name='Quiet red brick', letters='ΩΞΛΗ',
         body=[-18625,-16175,-2545,-295], parcel=[-18740,-15750],
         state='Dark house', yard='Overgrown garden and old swing set',
         color='#b8bdc4', interior='Entry and stair hall; chapter room; TV lounge; dining; kitchen; shared WC.'),
    dict(id='R2', existing=True, name='Tidy clapboard', letters='ΨΗΞΡ',
         body=[-15300,-12500,-2530,-430], parcel=[-15750,-12000],
         state='Quiet and cared for', yard='Raked lawn, stone patio, garden shed',
         color='#c9d9cf', interior='Central stair; orderly lounge; chapter and dining rooms; kitchen; shared WC.'),
    dict(id='R3', existing=True, name='Brick Colonial', letters='ΗΞΩΡ',
         body=[-11350,-8850,-2600,-400], parcel=[-12000,-8500],
         state='Party wreckage', yard='Volleyball, garage, couch and muddy paths',
         color='#dcb8a3', interior='Foyer and composite stair; hearth lounge; dining; kitchen; ground-floor party chapter room; WC.'),
    dict(id='R4', existing=False, name='Tudor Revival', letters='ΛΩΗΞ',
         body=[-8400,-6800,450,1850], parcel=[-8500,-6700],
         state='Moderate party', yard='Picnic patio, grill and lean-to',
         color='#ddc58e', interior='Slate vestibule; oak hall; hearth lounge; chapter and dining room; kitchen; shared WC.'),
    dict(id='R5', existing=False, name='Queen Anne', letters='ΡΞΗΨ',
         body=[-7080,-5680,-1700,-200], parcel=[-6700,-3900],
         state='Party interrupted', yard='Food still out, fire ring and small shed',
         color='#d8b8c7', interior='Entry stair; connected parlors; turret alcove; dining and kitchen; shared WC.'),
]
# Fence coordinates and gate are copied from HouseWestField.h, not redesigned.
fence = dict(y=4520, west=-19300, east=-3900, gate_west=-7320, gate_east=-6780)
path = [[-8100,-3900],[-8100,0],[-6550,0],[-6550,2200],[-6350,3100],[-7050,3100],[-7050,4520]]
porches = [dict(house='R4', rect=[-8400,-6800,150,450]),
           dict(house='R5', rect=[-7080,-5680,-1850,-1700]),
           dict(house='R5', rect=[-7230,-7080,-1850,-1150])]
access_steps=[]
for h in houses:
    cx=(h['body'][0]+h['body'][1])/2;rear=h['body'][3]
    h['floor_z_cm']=30 if h['existing'] or h['id']=='R5' else -50
    stair_x=cx+80 if h['id']=='R5' else cx
    access_steps.append(dict(house=h['id'],side='rear',rect=[stair_x-200,stair_x+200,rear,rear+(380 if h['existing'] else 408 if h['id']=='R5' else 500)],
                             risers=6 if h['existing'] else 7,
                             nominal_riser_cm=15 if h['existing'] else 106.61/7 if h['id']=='R5' else 111.2/7,
                             tread_cm=28,top_landing_cm=120,bottom_landing_cm=120))
    if not h['existing']:
        access_steps.append(dict(house=h['id'],side='front',rect=[cx-200,cx+200,-1990 if h['id']=='R5' else 122,-1850 if h['id']=='R5' else 150],
                                 risers=6 if h['id']=='R5' else 2,nominal_riser_cm=15 if h['id']=='R5' else 33.0275/2,tread_cm=28,
                                 top_landing='Uses the front porch',bottom_landing='Existing grade; no new slab'))
retained_hedge=dict(rect=[-18860,-18740,-4100,5100],kind='Existing solid hedge; retain; context clipped to diagram north edge',height_cm=190)
parking_replacement=dict(
    authorized=True,rect=[-7200,-3700,-2500,-300],lot_approach=[-5800,-5200,-4190,-2500],
    replacement='Queen Anne chapter house, front lawns, porch approaches and the unchanged pedestrian connection to the main house',
    remove=['Lot asphalt and its dedicated entrance apron','Parking cars and bay-specific muscle-car placement',
            'Bay paint and accessible-parking symbols','Curbs, wheel stops and parking signs',
            'Lot-specific props, mulch and shrubs','Both west-lot light poles, their lights and pier bases'],
    preserve=['Main-house perimeter walk and connection','Main-house east driveway',
              'Existing road and sidewalk','All original tree transforms and site planting RNG/NoGo'],
    built=False)
commons_paths=[
    dict(name='Retained house approach',points=[[-5510,-3900],[-5510,-2800],[-5510,-1400],[-4320,-1400],[-4320,-600],[-4320,-195],[-3480,-195]]),
    dict(name='Tudor lawn approach',points=[[-5510,-2800],[-7600,-2800],[-7600,122]]),
    dict(name='Queen Anne lawn approach',points=[[-6380,-2800],[-6380,-2020]])]
commons_props=[dict(kind='Tudor picnic table',rect=[-8730,-8270,-1020,-680]),
               dict(kind='Queen Anne abandoned food table',rect=[-6100,-5800,-2550,-2350])]
retained_parking_walk=[[-5510,-2800],[-5510,-1400],[-4320,-1400],[-4320,-600],[-4320,-195],[-3480,-195]]
outbuildings = [
    dict(house='R1',kind='Garden shelter',rect=[-16680,-16180,3950,4330]),
    dict(house='R2',kind='Garden shed',rect=[-14900,-14300,3920,4350]),
    dict(house='R3',kind='Garage',rect=[-10500,-9750,3850,4380]),
    dict(house='R4',kind='Lean-to',rect=[-8240,-7790,3890,4320]),
    dict(house='R5',kind='Small shed',rect=[-5140,-4700,3970,4350]),
]
# These are NEW yard dividers, not edits to the existing woods fence.
dividers = [
    dict(x=-15750,kind='Low picket',gaps=[[700,1150],[3100,3550]]),
    dict(x=-12000,kind='Chain link',gaps=[[700,1150],[3100,3550]]),
    dict(x=-8500,kind='Split rail',gaps=[[100,650],[3100,3600]]),
    dict(x=-6700,kind='Broken hedge with rail sections',gaps=[[1050,2150],[3000,3920]]),
]

# Use the game's archived native transform receipt, including front street trees.
tree_path = ROOT/'Saved/CampusMasterWork/runs/p8z_reviews/Saved/WoodsPassReview/original_trees.csv'
trees=[]
for line in tree_path.read_text().splitlines()[1:]:
    batch,index,transform=line.split(',',2)
    x,y,z=map(float,transform.split('|')[0].split(','))
    if -19800<=x<=4800 and -6100<=y<=5100:
        trees.append(dict(batch=batch,index=int(index),x=x,y=y,z=z))

probes=[(x,y) for x in [-17500,-12500,-7000,-4400] for y in [2100,2800,3700]]
review_route=[[-4500,2900],[-5700,3200],[-7050,3800],[-7050,4200],[-7050,4830],[-7050,5480],[-6800,5900]]
def inside(p,r,pad=0):
    return r[0]-pad<=p[0]<=r[1]+pad and r[2]-pad<=p[1]<=r[3]+pad
def distance_segment(p,a,b):
    dx,dy=b[0]-a[0],b[1]-a[1]
    t=max(0,min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/(dx*dx+dy*dy)))
    return math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dy)
new_rects=[h['body'] for h in houses if not h['existing']]+[p['rect'] for p in porches]+[b['rect'] for b in outbuildings]+[s['rect'] for s in access_steps]+[p['rect'] for p in commons_props]
conflicts=[]
for t in trees:
    for r in new_rects:
        if inside((t['x'],t['y']),r,100): conflicts.append(['tree_envelope',t,r])
for p in probes:
    for r in new_rects:
        if inside(p,r,100): conflicts.append(['fixed_ground_probe',p,r])
path_tree_min=min((min(distance_segment((t['x'],t['y']),a,b) for a,b in zip(path,path[1:])),t['batch'],t['index']) for t in trees)
if path_tree_min[0]<180:conflicts.append(['access_path_tree_clearance',path_tree_min])
commons_tree_min=min(distance_segment((t['x'],t['y']),a,b) for t in trees for cp in commons_paths for a,b in zip(cp['points'],cp['points'][1:]))
if commons_tree_min<180:conflicts.append(['commons_path_tree_clearance',commons_tree_min])
for prop in commons_props:
    r=prop['rect']
    for cp in commons_paths:
        for a,b in zip(cp['points'],cp['points'][1:]):
            n=max(1,int(math.dist(a,b)/20))
            if any(inside((a[0]+(b[0]-a[0])*i/n,a[1]+(b[1]-a[1])*i/n),r,120) for i in range(n+1)):
                conflicts.append(['commons_prop_path_clearance',prop['kind'],cp['name']])
path_probe_intersections=[p for p in probes if min(distance_segment(p,a,b) for a,b in zip(path,path[1:]))<=90]
for divider in dividers:
    for y in range(100,4521,20):
        if any(a<=y<=b for a,b in divider['gaps']):continue
        p=(divider['x'],y)
        if min(distance_segment(p,a,b) for a,b in zip(review_route,review_route[1:]))<120:
            conflicts.append(['divider_fixed_walk_clearance',p]);break
        if any(math.hypot(p[0]-t['x'],p[1]-t['y'])<100 for t in trees):
            conflicts.append(['divider_tree_root',p]);break

data=dict(created_utc=datetime.now(timezone.utc).isoformat(), units='world centimeters',
          status='REVISED LOT PLAN; R3 native walks passed but R5 canopy clipping requires this relocation; R4 native integration pending',
          no_map_expansion=True,no_road_change=True,no_fence_change=True,no_ground_change=True,
          houses=houses,porches=porches,access_steps=access_steps,outbuildings=outbuildings,
          retained_hedge=retained_hedge,yard_dividers=dividers,
          main_house_parking=parking_replacement,commons_paths=commons_paths,commons_props=commons_props,
          retained_parking_walk=retained_parking_walk,
          fence=fence,access_path=path,path_width_cm=180,
          access_path_treatment='Noncolliding ground-conforming visual dirt/wear only; no raised paving or terrain edits',
          preserved_native_tree_csv=str(tree_path.relative_to(ROOT)),trees=trees,
          fixed_west_field_probes=probes,fixed_walk_route=review_route,
          plan_checks=dict(conflicts=conflicts,min_path_tree_distance_cm=path_tree_min[0],
                           min_commons_path_tree_distance_cm=commons_tree_min,
                           path_probe_intersections=path_probe_intersections,
                           path_probe_policy='Must remain noncolliding at existing grade'),
          removed_existing_parking_lights=2,new_real_exterior_lights=6,proposed_exterior_total=57,shared_interior_pool=6)
(OUT/'PLAN.json').write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n')

W,H=2000,1450
im=Image.new('RGB',(W,H),'#f6f4ee');d=ImageDraw.Draw(im)
d.text((65,40),'Fraternity Row inside the existing map',fill='#1b2927',font=font(45,True))
d.text((68,103),'Three existing houses + two new houses | Existing grass and former parking become the row | October 1986',fill='#4c5955',font=font(23))
xmin,xmax=-19800,4800;ymin,ymax=-6100,5100
s=1840/(xmax-xmin);mx,my=80,175
def xy(x,y):return (mx+(x-xmin)*s,my+(ymax-y)*s)
def rect(r,fill,outline=None,width=2):
    a=xy(r[0],r[3]);b=xy(r[1],r[2]);d.rectangle([a,b],fill=fill,outline=outline,width=width)
def line(points,fill,width=2):d.line([xy(*p) for p in points],fill=fill,width=width)
def label(x,y,text,size=20,fill='#23302c',bold=False,anchor='mm'):
    d.text(xy(x,y),text,fill=fill,font=font(size,bold),anchor=anchor)
rect([xmin,xmax,ymin,ymax],'#dce3ce','#b9c4b2')
rect([-19300,-3900,100,4520],'#d4dfbf')
rect([-19300,-3900,4070,4520],'#aabb96')
label(-15500,4810,'Existing woods edge and fence',23,bold=True)
for h in houses:
    rect([*h['parcel'],100,4520],h['color'])
    # Wild rear third is shown as texture, never as a new collision wall.
    rear=h['body'][3];wild=rear+(4520-rear)*.67
    for j in range(12):
        x=h['parcel'][0]+(j+.5)*(h['parcel'][1]-h['parcel'][0])/12
        line([[x,wild],[x+110,4520]],'#889b76',1)
    if h['id'] in ['R1','R2','R3']:
        label(sum(h['parcel'])/2,1950,h['id']+' backyard',21,bold=True)
    else:
        label(sum(h['parcel'])/2,3650,h['id']+' yard',18,bold=True)
for divider in dividers:
    for y in range(150,4450,160):
        if any(a-100<=y<=b for a,b in divider['gaps']):continue
        line([[divider['x'],y],[divider['x'],y+80]],'#7e7b70',3)
rect(retained_hedge['rect'],'#61704f')
rect([-32000,8700,-5550,-4150],'#535959')
rect([-32000,8700,-4150,-3900],'#bcbcb2')
label(-12000,-4820,'EXISTING FRATERNITY PARK DRIVE',24,fill='white',bold=True)
label(-12000,-5700,'Road and sidewalk stay in place',19)

# The user's new instruction releases the parking footprint for Row lawns.
rect(parking_replacement['rect'],'#cadbb6','#809b6c',3)
# Revised lot house is labelled directly in its footprint.
# Keep the diagram clear around the new steps and shared path.
# Main-house access stays; no car bays or raised curbs remain.
rect([-4490,-3400,-285,-105],'#d1cbb9','#968c77',1)
for r,text in [([-3398,3338,-2198,1720],'MAIN HOUSE'),
               ([-1506,1506,1720,3820],'Basketball court')]:
    rect(r,'#e8dcd6','#9e6a62',3)
    label((r[0]+r[1])/2,(r[2]+r[3])/2,text,19,bold=True,fill='#764b44')
    label((r[0]+r[1])/2,(r[2]+r[3])/2-360,'PROTECTED',16,fill='#764b44')

for p in porches:rect(p['rect'],'#efe6d2','#746e63')
for st in access_steps:rect(st['rect'],'#e5d6b5','#746e63')
for b in outbuildings:
    rect(b['rect'],'#8c8371','#423f38',2)
for p in commons_props:rect(p['rect'],'#a59570','#665c43',2)
for h in houses:
    rect(h['body'],h['color'],'#423f38',4)
    x=(h['body'][0]+h['body'][1])/2;y=(h['body'][2]+h['body'][3])/2
    label(x,y+300,h['id']+('  KEEP' if h['existing'] else '  ADD'),22,bold=True)
    label(x,y-50,h['letters'],23,bold=True)
    if h['existing']:
        line([[x,h['body'][2]],[x,-3900]],'#8c8371',5)
    # South-facing entry arrow.
    line([[x,h['body'][2]-60],[x,h['body'][2]-290]],'#26332f',3)

# Draw tree roots after house envelopes so accidental overlaps remain obvious.
for t in trees:
    x,y=xy(t['x'],t['y']);r=10 if t['batch']=='Big' else 8
    d.ellipse([x-r,y-r,x+r,y+r],fill='#426f50',outline='#183d2b',width=2)
    d.ellipse([x-22,y-22,x+22,y+22],outline='#658665',width=1)

line(path,'#fff9df',12);line(path,'#77684c',3)
for cp in commons_paths:line(cp['points'],'#fff9df',12);line(cp['points'],'#77684c',3)
for x,y in probes:
    a,b=xy(x,y);d.line((a-5,b-5,a+5,b+5),fill='#4d6885',width=2);d.line((a-5,b+5,a+5,b-5),fill='#4d6885',width=2)
line(review_route,'#4d6885',2)
line([[-19300,4520],[-7320,4520]],'#354039',7)
line([[-6780,4520],[-3900,4520]],'#354039',7)
label(-7100,4850,'Existing gate',18)
label(-4400,-3000,'Former parking\nwalk kept clear',17)
label(-10300,3400,'Ground stays unchanged',19)

# Scale and compass.
line([[-18500,-3450],[-16500,-3450]],'#273a32',4)
for x in [-18500,-17500,-16500]:line([[x,-3530],[x,-3370]],'#273a32',3)
label(-17500,-3200,'20 metres',17)
label(-19200,4000,'N',23,bold=True)
line([[-19200,3500],[-19200,3900]],'#273a32',3)

legend_y=1050
d.text((70,legend_y),'HOUSE AND BACKYARD STORIES',fill='#26332f',font=font(25,True))
for i,h in enumerate(houses):
    x=70+i*390
    d.rectangle([x,legend_y+45,x+27,legend_y+72],fill=h['color'],outline='#423f38')
    d.text((x+38,legend_y+43),h['id']+'  '+h['name'],fill='#26332f',font=font(21,True))
    d.text((x,legend_y+86),h['state'],fill='#26332f',font=font(20))
    words=h['yard'].split();lines=[];cur=''
    for word in words:
        if len(cur)+len(word)>31:lines.append(cur);cur=word
        else:cur=(cur+' '+word).strip()
    if cur:lines.append(cur)
    for k,text in enumerate(lines):d.text((x,legend_y+119+k*27),text,fill='#4c5955',font=font(18))
d.text((70,1320),'Green circles: original tree roots kept. Blue marks: fixed review probes and walking route kept.',fill='#34473d',font=font(21))
d.text((70,1354),'Revised plan: Queen Anne moves into the former lot to clear existing canopies. Native validation follows.',fill='#6d4d36',font=font(20))
d.text((70,1387),'No map expansion. Existing road, woods fence, gate, terrain and main house stay. Revised footprint awaits native acceptance.',fill='#6d4d36',font=font(20))
im.save(OUT/'PLAN.png')
print('PLAN',OUT/'PLAN.png')
print('STATIC CHECKS',json.dumps(data['plan_checks']))
