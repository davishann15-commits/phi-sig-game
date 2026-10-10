import json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen')
p=R/'manifest.json';d=json.loads(p.read_text());counts={}
for row in d['placements']:
 a=row['asset'];counts[a]=counts.get(a,0)+1;i=counts[a]
 if a=='WhiteCooler':row['location'][0]=2395
 elif a=='RollingBlueCooler':row['location'][0]=2550
 elif a=='BlackBarStool':row['location'][0]=[2650,2710][i-1]
 elif a=='ClosedStorageBox':row['location'][0]=2820 if row['location'][1]==-640 else 2220
 elif a=='PaperGroceryBag':row['location']=[2830,-632,33]
 elif a=='BlackBag':row['location']=[2800,-627,33]
 elif a=='DoubleCerealDispenser':row['location']=[2220,-630,33]
 elif a=='RedBullCase':row['location'][0]=2290
 elif a=='PowerCord' and row['location'][1]==-572:row['location'][0]=2210
 elif a=='GreenSpindleChair' and row['name']=='stacked spare chair':row['location'][0]=2890
p.write_text(json.dumps(d,indent=2));print('STORAGE_PLACEMENT_CORRECTED')
