from pathlib import Path
import json
P=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff');R=P/'SourceArt/BoywickBedroom'
s=(R/'source/build_room.py').read_text()
exec(compile(s.split('# A single black steel loft frame')[0],'helpers','exec'))
old=json.loads((R/'manifest.json').read_text())
exec(compile(s[s.index("g='PulpFictionPoster'"):s.index("g='EmptyTVBracket'")],'prints','exec'))
exec(compile(s[s.index("g='PatternedPillow'"):s.index('# Rustic plank table')],'pillow','exec'))
exec(compile(s[s.index("g='DrapedTowel'"):s.index('# Rounded futon')],'towel','exec'))
t=(R/'source/build_details.py').read_text()
exec(compile(t[t.index("g='DeskPhoto'"):t.index("g='TowerFan'")],'deskphoto','exec'))
new={a['id']:a for a in doc['assets']}
for i,a in enumerate(old['assets']):
 if a['id']in new:old['assets'][i]=new[a['id']]
if not old.get('visual_fit_corrected'):
 for q in old['placements']:
  if q['asset']=='PatternedPillow':q['location'][2]-=14
  if q['asset']in ['DrapedTowel','HangingLaundryBag']:q['yaw']=0
  # The chair remains usable; the sofa edge leaves a full capsule-width entry lane.
  if q['asset'] in ['GreyFuton','PatternedPillow','Laptop','Controller'] or (q['asset'].startswith('LooseShirt') and q['location'][2]>340) or (q['asset']=='SnackBag' and q['location'][2]>340 and q['location'][0]>-2800):q['location'][0]-=22
 old['visual_fit_corrected']=True
if not old.get('cloth_rest_corrected'):
 for q in old['placements']:
  if q['asset']=='PatternedPillow':q['location'][2]-=6
  if q['asset']=='DrapedTowel':q['location'][2]+=3
 old['cloth_rest_corrected']=True
i=0
for q in old['placements']:
 if q['asset'].startswith('LooseShirt') and q['location'][2]>340:
  q['location'][2]=351.3+(i%3)*1.2;i+=1
old['refresh_meshes']=list(new)
(R/'manifest.json').write_text(json.dumps(old,indent=2))
