"""Refresh only redesigned mesh families and record photo correction checks."""
import json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen')
d=json.loads((R/'manifest.json').read_text());before=json.loads((R/'before-corrections/manifest.json').read_text())
prior={a['id'] for a in before['assets']};now={a['id'] for a in d['assets']}
changed={'GreenSpindleChair','ServingCounter','IceMachine','StainlessApplianceRack','MicrowaveCart','Toaster','BlackBarStool','AutumnBouquet','BreadBag','KitchenOakFloor'}
d['refresh_assets']=sorted(changed | (now-prior))
d['refresh_materials']=list(d['materials'])
d['refresh_textures']=sorted({Path(v).stem for m in d['materials'].values() for k,v in m.items() if k in ('texture','roughness_texture','normal_texture','opacity_mask')})
# Model generation completed before the chair spacing adjustment. Match the source.
for p in d['placements']:
 if p['asset']!='GreenSpindleChair':continue
 x,y,z=p['location']
 if x==2591:p['location'][0]=2620
 elif x==2879:p['location'][0]=2850
 elif x==2735 and y==-420:p['location'][1]=-397
 elif x==2735 and y==622:p['location'][1]=600
assets={a['id']:a for a in d['assets']};chair=assets['GreenSpindleChair'];lo,hi=chair['bounds_blender_metres'];size=[round((h-l)*100,2) for l,h in zip(lo,hi)]
assert 44<size[0]<51 and 88<size[2]<95
assert sum(p['asset']=='ServingCounter' for p in d['placements'])==5
assert next(p for p in d['placements'] if p['asset']=='TopFreezerFridge')['location'][1]==1112
assert next(p for p in d['placements'] if p['asset']=='StainlessApplianceRack')['location'][1]==927
audit=dict(chair_dimensions_cm=size,stainless_serving_cart_count=5,corrected_fridge_and_table_order=True,photograph_details_added=sorted(now-prior),floor_tile_metres=[4.8,3.2],floor_board_width_cm=20)
(R/'corrections-audit.json').write_text(json.dumps(audit,indent=2));(R/'manifest.json').write_text(json.dumps(d,indent=2))
print('CORRECTIONS_READY',len(d['placements']),len(d['refresh_assets']))
