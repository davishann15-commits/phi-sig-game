"""Tuck the reference card chairs beneath the table and preserve a clear aisle."""
import json,math
from pathlib import Path
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library')
p=R/'manifest.json';d=json.loads(p.read_text());i=0
for a in d['placements']:
 if a['asset']=='OctagonalCardTable':
  a['location']=[-3004.2+1.80*180,773.1-1.33*180,-329.06];a['scale']=[.86,.86,1]
 if a['name'].startswith('Card table chair '):
  i+=1;t=math.radians([45,135,225,315][int(a['name'].rsplit(' ',1)[1])-1])
  u=1.80+math.sin(t)*.90;v=1.33+math.cos(t)*.90
  a['location']=[round(-3004.2+u*180,3),round(773.1-v*180,3),-329.06];a['scale']=[.88,.88,1]
assert i==4
d['notes']=[n for n in d['notes'] if not n.startswith('Card table footprint')]+['Card table footprint and tucked chairs fitted to preserve a full player-width east aisle.']
p.write_text(json.dumps(d,indent=2))
print('CARD_AREA_CLEARANCE_FITTED')
