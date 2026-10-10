from pathlib import Path
import subprocess,concurrent.futures
from PIL import Image,ImageOps,ImageDraw
B=Path(__file__).resolve().parent
src=Path('/Users/Stewart/Projects/Senior sendoff/noodle');out=B/'references/noodle';out.mkdir(exist_ok=True)
def cv(p):
 d=out/(p.stem+'.jpg')
 if not d.exists():subprocess.run(['sips','-s','format','jpeg','-Z','1200',str(p),'--out',str(d)],capture_output=True,check=True)
 return d
files=sorted(p for p in src.glob('IMG_*.HEIC') if ' ' not in p.stem)
with concurrent.futures.ThreadPoolExecutor(max_workers=3)as ex:imgs=list(ex.map(cv,files))
for page in range(0,len(imgs),12):
 canvas=Image.new('RGB',(1440,1280),'#202026');d=ImageDraw.Draw(canvas)
 for j,p in enumerate(imgs[page:page+12]):
  im=Image.open(p);im.thumbnail((470,285));x=(j%3)*480;y=(j//3)*320
  canvas.paste(im,(x+(480-im.width)//2,y+28));d.text((x+10,y+6),p.stem,fill='white')
 canvas.save(out/('contact_%02d.jpg'%page))
