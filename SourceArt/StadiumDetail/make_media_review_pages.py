"""Create contact pages for actual audio/temporal images; no inspection verdict."""
from pathlib import Path
import argparse,json
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args()
r=json.loads((ROOT/'Saved/StadiumDetailWork'/(a.tag+'_sound_pcm.json')).read_text());assert r['native_and_pcm_passed']
out=ROOT/'Saved/StadiumDetailWork'/(a.tag+'_media_pages');out.mkdir(exist_ok=False)
for start in range(0,len(r['images']),6):
 group=r['images'][start:start+6];sheet=Image.new('RGB',(1280,1164),(20,20,20));draw=ImageDraw.Draw(sheet)
 for i,item in enumerate(group):
  path=ROOT/item['file']
  with Image.open(path) as source:preview=source.copy()
  preview.thumbnail((640,360));x=i%2*640;y=i//2*388;sheet.paste(preview,(x,y));draw.text((x+4,y+364),path.name,fill='white')
 target=out/f'{start//6+1:02d}.jpg';sheet.save(target,quality=96);print(target)
