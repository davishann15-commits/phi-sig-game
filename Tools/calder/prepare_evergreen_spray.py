"""Photograph-based needle frond for the unchanged existing evergreen cards.

Uses healthy green needle cutouts from the acquired kit, never its skeletal/PVE
runtime tree. Source NT packs normal XY in RG and translucency in B, as verified
by the exported MF_ReconstructZ and MF_TwoSided_Leaves graphs. No source writes.
"""
from collections import deque
import hashlib
import json
import math
from pathlib import Path
import random
import numpy as np
from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT/'Saved/CalderWork/evergreen_screening'
OUT = ROOT/'SourceArt/CalderLandscape/phase2/woods_sprays'
SIZE = 1024
colour = Image.open(SOURCE/'T_Norway_Spruce_Foliage_01_CA.tga').convert('RGBA')
packed = np.array(Image.open(SOURCE/'T_Norway_Spruce_Foliage_01_NT.tga').convert('RGB'), dtype=np.float32)
xy = packed[:,:,:2]/127.5-1
z = np.sqrt(np.clip(1-np.sum(xy*xy, axis=2), 0, 1))
normal_data = np.dstack(((xy+1)*127.5, (z+1)*127.5)).clip(0,255).astype(np.uint8)
normal_source = Image.fromarray(normal_data, 'RGB').convert('RGBA')
normal_source.putalpha(colour.getchannel('A'))
small = np.array(colour.resize((512,512)))
mask = small[:,:,3]>90
seen = np.zeros(mask.shape, bool)
cells = []
boxes = []
for y,x in zip(*np.where(mask)):
    if seen[y,x]: continue
    queue = deque([(int(x),int(y))]);seen[y,x] = True;points = []
    while queue:
        xx,yy = queue.popleft();points.append((xx,yy))
        for nx,ny in [(xx-1,yy),(xx+1,yy),(xx,yy-1),(xx,yy+1)]:
            if 0<=nx<512 and 0<=ny<512 and mask[ny,nx] and not seen[ny,nx]:
                seen[ny,nx] = True;queue.append((nx,ny))
    if len(points)<100: continue
    pixels = np.array([small[yy,xx,:3] for xx,yy in points]);mean = pixels.mean(axis=0)
    if not (mean[1]>mean[0]*1.06 and mean[1]>mean[2]*1.2): continue
    xs,ys = zip(*points)
    box = (max(0,min(xs)*8-8),max(0,min(ys)*8-8),min(4096,(max(xs)+1)*8+8),min(4096,(max(ys)+1)*8+8))
    if (box[2]-box[0])/(box[3]-box[1])<4: continue
    component = np.zeros((512,512),dtype=np.uint8)
    for xx,yy in points: component[yy,xx] = 255
    gate = Image.fromarray(component,'L').resize(colour.size,Image.Resampling.NEAREST).crop(box)
    c,n = colour.crop(box),normal_source.crop(box)
    isolated_alpha = ImageChops.multiply(c.getchannel('A'),gate)
    c.putalpha(isolated_alpha);n.putalpha(isolated_alpha)
    cells.append((c,n));boxes.append(box)
assert len(cells)>=4, boxes
canvas = Image.new('RGBA',(SIZE,SIZE),(32,45,26,0))
normals = Image.new('RGBA',(SIZE,SIZE),(128,128,255,255))
draw = ImageDraw.Draw(canvas)
rng = random.Random(2026101707)
count = 0
def needle(base,tip):
    global count
    c,n = cells[count%len(cells)];count+=1
    alpha = np.array(c.getchannel('A'))
    yy,_ = np.where(alpha[:,-12:]>64)
    stem = (c.width-7,float(yy.mean()) if len(yy) else c.height*.5)
    angle = math.atan2(tip[1]-base[1],tip[0]-base[0])-math.pi
    scale = math.hypot(tip[0]-base[0],tip[1]-base[1])/max(1,c.width-1)
    cs,sn = math.cos(angle),math.sin(angle)
    bx = base[0]-scale*(cs*stem[0]-sn*stem[1]);by=base[1]-scale*(sn*stem[0]+cs*stem[1])
    inverse = (cs/scale,sn/scale,(-cs*bx-sn*by)/scale,-sn/scale,cs/scale,(sn*bx-cs*by)/scale)
    canvas.alpha_composite(c.transform((SIZE,SIZE),Image.Transform.AFFINE,inverse,resample=Image.Resampling.BICUBIC))
    warped = n.transform((SIZE,SIZE),Image.Transform.AFFINE,inverse,resample=Image.Resampling.BILINEAR)
    data = np.array(warped,dtype=np.float32);nx=data[:,:,0]/127.5-1;ny=data[:,:,1]/127.5-1
    data[:,:,0]=(cs*nx-sn*ny+1)*127.5;data[:,:,1]=(sn*nx+cs*ny+1)*127.5
    normals.alpha_composite(Image.fromarray(data.clip(0,255).astype(np.uint8),'RGBA'))
def twig(start,end,pairs,width):
    draw.line([start,end],fill=(58,43,27,255),width=width)
    dx,dy=end[0]-start[0],end[1]-start[1];length=math.hypot(dx,dy);ux,uy=dx/length,dy/length
    for i in range(pairs):
        t=(i+.8+rng.uniform(-.2,.2))/(pairs+1);base=(start[0]+dx*t,start[1]+dy*t)
        for sign in [-1,1]:
            size=rng.uniform(35,48)*(1-.15*t)
            tip=(base[0]+(-uy*sign*.94+ux*.34)*size,base[1]+(ux*sign*.94+uy*.34)*size)
            needle(base,tip)
    needle((end[0]-ux*16,end[1]-uy*16),(end[0]+ux*21,end[1]+uy*21))
draw.line([(512,978),(504,732),(521,455),(509,58)],fill=(58,43,27,255),width=8)
for row in range(6):
    y=855-row*135
    for sign in [-1,1]:
        start=(512+math.sin(row*1.9)*8,y)
        reach=310*(1-row*.115)*rng.uniform(.90,1.08)
        end=(start[0]+sign*reach,y-rng.uniform(75,130))
        twig(start,end,42 if row<3 else 36,4)
twig((509,258),(510,57),25,3)
files=[]
for kind,img in [('A',canvas),('N',normals.convert('RGB'))]:
    path=OUT/('T_CalderWoodsEvergreen_'+kind+'.png');path.unlink(missing_ok=True);img.save(path)
    files.append({'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
manifest={'scope':'Acquired needle-photo derived atlas trial for existing evergreen foliage cards; botanical/whole-woods acceptance separate',
          'acquired_source_sha256':json.loads((SOURCE/'inspection.json').read_text())['source_sha256'],'verified_material_graphs':31,
          'seed':2026101707,'size':SIZE,'needles':count,'green_source_boxes':boxes,'normal_decode':'RG*2-1; Z=sqrt(saturate(1-dot(XY,XY))); NT.B is translucency, not normal Z',
          'sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [SOURCE/'T_Norway_Spruce_Foliage_01_CA.tga',SOURCE/'T_Norway_Spruce_Foliage_01_NT.tga']},
          'alpha_coverage_at_0_32':float((np.array(canvas.getchannel('A'))>=82).mean()),
          'files':files,'source_licence':'Existing acquired Norway spruce kit; not CC0','runtime_bound_by_default':False,'trial_switch':'-CalderEvergreenAtlasTrial'}
out=OUT/'evergreen_manifest.json';out.unlink(missing_ok=True);out.write_text(json.dumps(manifest,indent=2)+'\n')
print('CALDER_EVERGREEN_SPRAY '+json.dumps(manifest))
