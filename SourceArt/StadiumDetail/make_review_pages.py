"""Readable six-image review pages only; this never marks screenshots accepted."""
from pathlib import Path
import argparse,json
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args()
receipt=json.loads((ROOT/'Saved/StadiumDetailWork'/(a.tag+'_capture.json')).read_text())
out=ROOT/'Saved/StadiumDetailWork'/(a.tag+'_review_pages');out.mkdir(exist_ok=False)
index=[]
for run in receipt['runs']:
 folder=ROOT/'Saved/CalderWork/runs'/run['tag']/'Saved/HouseExteriorViews'
 paths=sorted(folder.glob('*.png'));assert len(paths)==len(run['frames'])
 for start in range(0,len(paths),6):
  group=paths[start:start+6];sheet=Image.new('RGB',(1280,3*388),(20,20,20));draw=ImageDraw.Draw(sheet)
  for i,path in enumerate(group):
   with Image.open(path) as source:preview=source.copy()
   preview.thumbnail((640,360));x=i%2*640;y=i//2*388;sheet.paste(preview,(x,y));draw.text((x+4,y+364),path.stem,fill='white')
  target=out/f'{run["tag"]}_{start//6+1:02d}.jpg';sheet.save(target,quality=96)
  index.append({'page':str(target.relative_to(ROOT)),'actual_images':[str(x.relative_to(ROOT)) for x in group]})
(out/'pages.json').write_text(json.dumps({'tag':a.tag,'inspection_pending':True,'pages':index},indent=2)+'\n')
print(out)
