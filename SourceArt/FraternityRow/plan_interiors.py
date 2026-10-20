"""Draw proposed ground floors. This edits planning files only, not Unreal."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont
OUT=Path(__file__).resolve().parent
SITE=json.loads((OUT/'PLAN.json').read_text())
FONT='/System/Library/Fonts/Supplemental/Arial.ttf'
BOLD='/System/Library/Fonts/Supplemental/Arial Bold.ttf'
def font(n,b=False):return ImageFont.truetype(BOLD if b else FONT,n)
# Local cm: south entrance Y=0, north/rear Y=D. Every partition has a 140 cm opening.
# A 220 cm central hall is reserved from front to rear; stairs occupy side-room space.
def rooms(h):
    W=h['body'][1]-h['body'][0];D=h['body'][3]-h['body'][2]
    c=W/2; l=c-110; r=c+110; y1=D*.47; y2=D*.76
    labels={
      'R1':('Still TV lounge','Chapter room / hearth','Dining room','Kitchen','Closed upper stair'),
      'R2':('Formal lounge','Tidy chapter room','Dining room','Kitchen','Composite stair'),
      'R3':('Hearth lounge','Party chapter room','Long dining tables','Kitchen / serving','Composite stair'),
      'R4':('Hearth lounge','Oak chapter room','Dining room','Kitchen / scullery','Oak stair'),
      'R5':('Front parlor / alcove','Chapter parlor','Dining / food left out','Kitchen','Carved stair'),
    }[h['id']]
    rs=[dict(name=labels[0],rect=[30,l-8,30,y1-8],door=[l,y1-110],side='left'),
        dict(name=labels[1],rect=[r+8,W-30,30,y1-8],door=[r,y1*.42],side='right'),
        dict(name=labels[2],rect=[30,l-8,y1+8,y2-8],door=[l,(y1+y2)/2],side='left'),
        dict(name=labels[3],rect=[r+8,W-30,y1+8,D-30],door=[r,D*.68],side='right'),
        dict(name='Shared WC',rect=[30,l-8,y2+8,D-30],door=[l,(y2+D)/2],side='left')]
    # Stair inset reduces a front room, never the front-to-rear escape lane.
    stair=[max(50,l-300),l-25,80,min(y1-210,600)]
    return dict(id=h['id'],width_cm=W,depth_cm=D,central_hall=[l,r,0,D],
                entry_width_cm=200,rear_exit_width_cm=140,room_door_width_cm=140,
                floor_z_cm=h['floor_z_cm'],
                turret=dict(location='southwest',rect=[30,270,30,270],purpose='Front parlor reading alcove') if h['id']=='R5' else None,
                exterior_access_steps=[s for s in SITE['access_steps'] if s['house']==h['id']],
                rooms=rs,stair=dict(name=labels[4],rect=stair,form='Two flights with a return landing',
                                   floor_to_floor_cm=390 if h['existing'] else 330,
                                   risers=24 if h['existing'] else 20,
                                   nominal_riser_cm=390/24 if h['existing'] else 330/20,
                                   tread_cm=28,flight_clear_width_cm=110,return_landing_cm=110,
                                   minimum_headroom_cm=210,
                                   upper='Closed by a visible solid locked door at the upper landing'),
                parlor_connection=dict(at_y=y1,x0=100,x1=300) if h['id']=='R5' else None,
                vestibule=dict(rect=[l,r,0,220],floor='Slate') if h['id']=='R4' else None,
                floor='Ground floor only; no excavation or terrain cuts',
                circulation='South entry -> clear central hall -> north exit. All five side rooms open off the hall.')
plans=[rooms(h) for h in SITE['houses']]
(OUT/'INTERIOR_PLAN.json').write_text(json.dumps(dict(status='PROPOSED; NOT BUILT OR RUNTIME VERIFIED',units='local centimetres',houses=plans),indent=2)+'\n')
W,H=2200,1680
im=Image.new('RGB',(W,H),'#f6f4ee'); d=ImageDraw.Draw(im)
d.text((55,35),'Five walk-in ground floors',font=font(43,True),fill='#243630')
d.text((58,94),'Proposed room and circulation plan | South entrances below, woods-facing exits above | October 1986',font=font(24),fill='#58655c')
for i,(h,p) in enumerate(zip(SITE['houses'],plans)):
    col=i%3;row=i//3;ox=55+col*720;oy=155+row*700
    d.rectangle([ox,oy,ox+690,oy+660],fill='white',outline='#c1c6bc',width=2)
    d.text((ox+22,oy+18),h['id']+'  '+h['name'],font=font(27,True),fill='#243630')
    d.text((ox+22,oy+57),h['state']+' | '+str(p['width_cm']/100)+' x '+str(p['depth_cm']/100)+' m',font=font(20),fill='#5d665e')
    scale=min(580/p['width_cm'],445/p['depth_cm']);bx=ox+(690-p['width_cm']*scale)/2;by=oy+108
    def pt(x,y):return bx+x*scale,by+(p['depth_cm']-y)*scale
    def rect(r,fill,outline='#726b60',width=2):
        d.rectangle([pt(r[0],r[3]),pt(r[1],r[2])],fill=fill,outline=outline,width=width)
    def text_rect(r,s,size=17):
        x,y=pt((r[0]+r[1])/2,(r[2]+r[3])/2)
        # Human-edited room labels remain within their room's column.
        words=s.split();lines=[];cur=''
        for w in words:
            if len(cur+' '+w)>19:lines.append(cur);cur=w
            else:cur=(cur+' '+w).strip()
        if cur:lines.append(cur)
        d.multiline_text((x,y),'\n'.join(lines),font=font(size,True),fill='#243630',anchor='mm',align='center',spacing=5)
    rect([0,p['width_cm'],0,p['depth_cm']],'#e9e1d2','#3d443c',3)
    rect(p['central_hall'],'#fcf4d6',None)
    for j,r in enumerate(p['rooms']):
        rect(r['rect'],h['color'])
        tr=list(r['rect'])
        if j==0:tr[1]=p['stair']['rect'][0]-20
        text_rect(tr,r['name'],15 if j==0 else 17)
        x,y=r['door'];a=pt(x,y-70);b=pt(x,y+70)
        d.line([a,b],fill='#fcf4d6',width=9)
    # Stairs are drawn in the front room's inner edge, with an open approach from the hall.
    st=p['stair']['rect'];rect(st,'#a58c6a','#574e40')
    for j in range(1,9):
        y=st[2]+(st[3]-st[2])*j/9;d.line([pt(st[0],y),pt(st[1],y)],fill='#efe6d5',width=1)
    mid=(st[0]+st[1])/2
    d.line([pt(mid,st[2]),pt(mid,st[3]-100)],fill='#574e40',width=2)
    if p['parlor_connection']:
        pc=p['parlor_connection'];d.line([pt(pc['x0'],pc['at_y']),pt(pc['x1'],pc['at_y'])],fill=h['color'],width=10)
    if p['vestibule']:
        vr=p['vestibule']['rect'];rect(vr,'#babbb0',None)
    if p['turret']:
        tr=p['turret']['rect'];a=pt(tr[0],tr[3]);b=pt(tr[1],tr[2]);d.arc([a,b],0,360,fill='#5d665e',width=2)
    # Central circulation route; no prop, bar, stair, or door swing may occupy it.
    x=p['width_cm']/2
    d.line([pt(x,80),pt(x,p['depth_cm']-80)],fill='#426f50',width=4)
    for y in [0,p['depth_cm']]:
        half=(p['entry_width_cm'] if y==0 else p['rear_exit_width_cm'])/2
        d.line([pt(x-half,y),pt(x+half,y)],fill='#fcf4d6',width=10)
    d.text(pt(x,-100),'FRONT ENTRY',font=font(16,True),fill='#426f50',anchor='mt')
    d.text(pt(x,p['depth_cm']+50),'REAR EXIT TO OWN YARD',font=font(16,True),fill='#426f50',anchor='mb')
    d.text((ox+22,oy+594),'220 cm hall; 140 cm room openings; shared WC.',font=font(18),fill='#5d665e')
    d.text((ox+22,oy+623),'Visible stair landing closes access to upper floors.',font=font(18),fill='#5d665e')
# Sixth card explains common, measured acceptance rather than inventing another house.
x,y=1495,855
d.rectangle([x,y,x+690,y+660],fill='#e6ebdf',outline='#b3bdad',width=2)
d.text((x+25,y+25),'Shared build rules',font=font(31,True),fill='#243630')
notes=[
    'Ground-floor chapter rooms avoid excavation.',
    'Floors sit on plinths above the existing grade.',
    'Front and rear doors use the campus door system.',
    'All five rooms branch from the clear hall.',
    'Furniture stays outside doors and their swings.',
    'Existing kit: CRTs, pay phones, tables and sofas.',
    'Original fictional composites, paddles and crests.',
    '1986 materials: oak, worn carpet, tile and plaster.',
    'Six shared interior lights across the campus.',
    'New exterior lights: six proposed, all shadowless.',
    'Plan only: native traversal and lighting pending.',
]
for j,n in enumerate(notes):d.text((x+25,y+96+j*44),n,font=font(20),fill='#3b4b40')
d.text((58,1600),'Schematic partition plan, to scale within each card. Stair and furniture placement will be checked against player capsules in Unreal.',font=font(22),fill='#6d4d36')
d.text((58,1635),'SourceArt planning only. No Source, Config, Content, terrain, road, fence or map changes have been made for this work.',font=font(22),fill='#6d4d36')
im.save(OUT/'INTERIOR_PLAN.png')
print(OUT/'INTERIOR_PLAN.png')
