"""Bake the portrait photo orientation and face the reference details into the room."""
import json
from pathlib import Path
from PIL import Image,ImageOps
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library');p=R/'manifest.json';d=json.loads(p.read_text())
image=ImageOps.exif_transpose(Image.open(R/'references/IMG_2272.png')).convert('RGB')
assert image.size==(1125,1500)
image.save(R/'textures/IMG_2272.png',exif=b'')
d['materials']['M_Lib_ShelfPhoto']['texture']=str(R/'textures/IMG_2272.png')
cases=[a for a in d['placements'] if a['asset']=='SixShelfBookcase']
for a in d['placements']:
 if a['asset'] in ['DarkThrowPillow','OrangeThrowPillow'] and a['location'][1]<0:a['roll']=62
 if a['asset']=='SmallPottedPlant' and abs(a['location'][2]-(-329.06+157))<.1:
  case=min(cases,key=lambda c:(c['location'][0]-a['location'][0])**2+(c['location'][1]-a['location'][1])**2)
  a['location'][1]=round(case['location'][1]+(-50.4 if case['yaw']==-90 else 50.4),3)
d['refresh_assets']=[];d['refresh_materials']=[];d['refresh_textures']=['IMG_2272']
p.write_text(json.dumps(d,indent=2))
print('REFERENCE_ORIENTATIONS_CORRECTED')
