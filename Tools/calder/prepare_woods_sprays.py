"""Build branch atlases from already-approved CC0 leaf scans, retaining UVs.

This is deterministic texture preparation for the existing code-built foliage.
Raw downloads are read-only. No new placement, batch, collision or RNG draw.
"""
import hashlib
import io
import json
import math
from pathlib import Path
import random
import zipfile

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
RAW = Path('/Users/Stewart/SeniorSendoffCC0Raw/ambientcg')
OUT = ROOT/'SourceArt/CalderLandscape/phase2/woods_sprays'
OUT.mkdir(exist_ok=True)
SIZE = 1024
report = []


def source(set_name, columns, rows):
    path = RAW/set_name/(set_name+'_2K-PNG.zip')
    with zipfile.ZipFile(path) as archive:
        names=archive.namelist()
        def load(suffix):
            name=next(n for n in names if n.endswith(suffix))
            return Image.open(io.BytesIO(archive.read(name))).copy()
        colour=load('_Color.png').convert('RGBA')
        opacity=load('_Opacity.png').convert('L')
        normal=load('_NormalDX.png').convert('RGB')
    colour.putalpha(opacity)
    cells=[]
    for y in range(rows):
        for x in range(columns):
            box=(x*colour.width//columns,y*colour.height//rows,(x+1)*colour.width//columns,(y+1)*colour.height//rows)
            c=colour.crop(box)
            bounds=c.getchannel('A').point(lambda v:255 if v>64 else 0).getbbox()
            if not bounds:raise RuntimeError('Empty leaf cell')
            c=c.crop(bounds)
            n=normal.crop(box).crop(bounds).convert('RGBA')
            n.putalpha(c.getchannel('A'))
            cells.append((c,n))
    return cells,hashlib.sha256(path.read_bytes()).hexdigest()


def leaf(canvas, normals, pair, base, tip, stem_right):
    colour,normal=pair
    alpha=np.array(colour.getchannel('A'))
    edge=alpha[:,-3:] if stem_right else alpha[:,:3]
    yy,_=np.where(edge>64)
    stem=(colour.width-1 if stem_right else 0,float(yy.mean()) if len(yy) else colour.height*.5)
    direction=(tip[0]-base[0],tip[1]-base[1])
    angle=math.atan2(direction[1],direction[0])-(math.pi if stem_right else 0)
    scale=math.hypot(*direction)/max(1,colour.width-1)
    c,s=math.cos(angle),math.sin(angle)
    bx=base[0]-scale*(c*stem[0]-s*stem[1])
    by=base[1]-scale*(s*stem[0]+c*stem[1])
    inverse=(c/scale,s/scale,(-c*bx-s*by)/scale,
             -s/scale,c/scale,(s*bx-c*by)/scale)
    args=((SIZE,SIZE),Image.Transform.AFFINE,inverse)
    warped=colour.transform(*args,resample=Image.Resampling.BICUBIC)
    canvas.alpha_composite(warped)
    wn=normal.transform(*args,resample=Image.Resampling.BILINEAR)
    data=np.array(wn,dtype=np.float32)
    x,y=data[:,:,0]/127.5-1,data[:,:,1]/127.5-1
    data[:,:,0]=(c*x-s*y+1)*127.5
    data[:,:,1]=(s*x+c*y+1)*127.5
    wn=Image.fromarray(np.clip(data,0,255).astype(np.uint8),'RGBA')
    normals.alpha_composite(wn)


for species,set_name,columns,rows,stem_right,selection in [
    ('Oak','LeafSet012',2,4,True,list(range(8))),
    ('Maple','LeafSet028',4,4,False,[0,2,3,4,5,7,9,11,15]),
]:
    cells,raw_hash=source(set_name,columns,rows)
    canvas=Image.new('RGBA',(SIZE,SIZE),(84,55,26,0))
    normals=Image.new('RGBA',(SIZE,SIZE),(128,128,255,255))
    draw=ImageDraw.Draw(canvas)
    rng=random.Random(20261016+(0 if species=='Oak' else 2))
    def p(x,y):return (x*4,y*4)
    draw.line([p(126,246),p(137,178),p(118,93),p(139,16)],fill=(59,39,23,255),width=6)
    count=0
    for index in range(4):
        for sign in [-1,1]:
            start=(128+math.sin(index*2.1)*9,217-index*45)
            end=(start[0]+sign*rng.uniform(52,69),start[1]+rng.uniform(-26,-9))
            draw.line([p(*start),p(*end)],fill=(64,42,23,255),width=4)
            base=p(*end)
            tip=p(end[0]+sign*30,end[1]-40)
            leaf(canvas,normals,cells[selection[count%len(selection)]],base,tip,stem_right)
            count+=1
    leaf(canvas,normals,cells[selection[-1]],p(132,50),p(137,9),stem_right)
    for kind,img in [('A',canvas),('N',normals.convert('RGB'))]:
        path=OUT/('T_CalderWoods'+species+'_'+kind+'.png')
        path.unlink(missing_ok=True)
        img.save(path)
    report.append({'species':species,'source':set_name,'source_url':'https://ambientcg.com/view?id='+set_name,
                   'licence':'CC0','raw_sha256':raw_hash,'leaves_per_spray':9,'size':SIZE})
path=OUT/'sources.json'
path.unlink(missing_ok=True)
path.write_text(json.dumps(report,indent=2))
print('CALDER_WOODS_SPRAYS PASS species=2 photographicLeaves=18 source=CC0')
