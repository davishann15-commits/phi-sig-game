import json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen');p=R/'manifest.json';d=json.loads(p.read_text())
if d.get('buffet_shift_cm',0)<30:
 assets={'ServingCounter','PanLid','BreadBag','AutumnBouquet','StorageBagCarton','FoilCarton','CoffeePump','RectangularCerealCanister','CerealCanister','PeanutButterJar','CocoaBox','BreakfastSupplies','PaperTowelRoll','FruitTray','ServingBowl','UtensilCup','ClearCup','PaperProductPack'}
 for row in d['placements']:
  if row['asset'] in assets and row['location'][0]>1550 and row['location'][1]>=1060:row['location'][0]+=30
 d['buffet_shift_cm']=30;p.write_text(json.dumps(d,indent=2))
