"""Face the reference arrangement correctly from the real hallway entrance."""
import json
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=R/'manifest.json';d=json.loads(p.read_text())
if not d.get('entry_orientation_corrected'):
 for q in d['placements']:
  if q['asset']=='RoomShell' or q['location'][0]>-2450:continue
  q['location'][1]=-1958.8-q['location'][1]
  q['yaw']=(180-q['yaw'])%360
 for q in d['external']:
  q['location'][1]=-1958.8-q['location'][1];q['yaw']=(180-q['yaw'])%360
 d['entry_orientation_corrected']=True
 p.write_text(json.dumps(d,indent=2))

# Keep the back of the sofa clear of the south wall and the dresser beside the loft.
if not d.get('wall_fit_corrected'):
 for q in d['placements']:
  if q['asset'] in ['GreyFuton','PatternedPillow','Laptop','Controller'] or (q['asset'].startswith('LooseShirt') and q['location'][2]>340) or (q['asset']=='SnackBag' and q['location'][2]>340 and q['location'][0]>-2800):q['location'][1]+=15
  if q['asset'] in ['Dresser','SmallTelevision','GameConsole','FramedPrint','EmptyTVBracket']:q['location'][0]+=36
  if q['asset'] in ['Dresser','SmallTelevision','GameConsole']:q['location'][1]+=8.8
 d['wall_fit_corrected']=True
 p.write_text(json.dumps(d,indent=2))
