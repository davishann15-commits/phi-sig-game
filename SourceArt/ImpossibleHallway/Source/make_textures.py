"""Deterministic seamless worn surfaces for the 180 cm hallway period."""
from pathlib import Path
import json
import numpy as np
from PIL import Image, ImageFilter

OUT=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ImpossibleHallway/Art')
OUT.mkdir(parents=True,exist_ok=True)
N=1024
rng=np.random.default_rng(38117)
y,x=np.mgrid[0:N,0:N]/N

def periodic_noise(grid,blur=0):
    small=rng.normal(.5,.18,(grid,grid));small=np.clip(small,0,1)
    image=Image.fromarray((small*255).astype('uint8')).resize((N,N),Image.Resampling.BICUBIC)
    if blur:image=image.filter(ImageFilter.GaussianBlur(blur))
    value=np.asarray(image).astype(float)/255
    # Match opposing edges without introducing a bright frame.
    for k in range(12):
        weight=(12-k)/24
        a=value[k,:].copy();b=value[-1-k,:].copy()
        value[k,:]=a*(1-weight)+b*weight;value[-1-k,:]=b*(1-weight)+a*weight
        a=value[:,k].copy();b=value[:,-1-k].copy()
        value[:,k]=a*(1-weight)+b*weight;value[:,-1-k]=b*(1-weight)+a*weight
    return value

def save(name,color,rough,height,strength):
    rgb=np.clip(color,0,255).astype('uint8')
    Image.fromarray(rgb,'RGB').save(OUT/(name+'_BaseColor.png'))
    Image.fromarray(np.clip(rough,0,255).astype('uint8'),'L').save(OUT/(name+'_Roughness.png'))
    dx=(np.roll(height,-1,1)-np.roll(height,1,1))*strength
    dy=(np.roll(height,-1,0)-np.roll(height,1,0))*strength
    normal=np.stack((-dx,-dy,np.ones_like(dx)),2)
    normal/=np.linalg.norm(normal,axis=2,keepdims=True)
    Image.fromarray(np.clip((normal*.5+.5)*255,0,255).astype('uint8'),'RGB').save(OUT/(name+'_Normal.png'))

coarse=periodic_noise(7,2); medium=periodic_noise(48); fine=periodic_noise(512)
# UV V grows from the skirting toward ceiling: concentrated dampness at base.
damp=(1-y)**3*(.4+.6*coarse)
streak=(np.sin(x*np.pi*74+coarse*5)*.5+.5)**10*(1-y)**2
chip=np.clip((medium-.65)*8,0,1)*(1-y)**2
paint=np.stack([85+coarse*14,89+coarse*15,75+coarse*11],2)
paint-=damp[...,None]*25+streak[...,None]*15
paint+=chip[...,None]*19+(fine[...,None]-.5)*7
height=medium*.08+fine*.025+chip*.12
save('IH_T_DampOlivePlaster',paint,207+fine*20-chip*15,height,7)

grain=periodic_noise(32); tiny=periodic_noise(512)
plank=(x*8).astype(int); seam=((x*8)%1<.017)|((x*8)%1>.983)
tones=np.array([0,4,-3,2,-1,5,-4,1])[np.minimum(plank,7)]
longgrain=np.sin(x*950+grain*11)*2.0+np.sin(x*2110+grain*15)*.6
wood=np.stack([46+tones+grain*13,40+tones+grain*10,31+tones+grain*8],2)
wood+=longgrain[...,None]+(tiny[...,None]-.5)*4
wood[seam]=[20,18,15]
# Worn joints exactly tile at module boundaries.
joint=((y<.008)|(y>.992))
wood[joint]*=.72
scruff=np.clip((grain-.63)*7,0,1)
wood+=scruff[...,None]*9
save('IH_T_OldFloor',wood,173+tiny*30-scruff*31,grain*.06+seam*.12+joint*.1,5)

ceil=periodic_noise(12,3); detail=periodic_noise(256)
color=np.stack([71+ceil*15,73+ceil*14,65+ceil*12],2)+(detail[...,None]-.5)*5
save('IH_T_DampCeiling',color,222+detail*10,ceil*.04+detail*.03,4)
files=sorted(p.name for p in OUT.glob('IH_T_*.png'))
json.dump({'size':[N,N],'textures':files,'seed':38117,'surfacePatternPeriodCm':180},open(OUT/'textures.json','w'),indent=2)
print('IMPOSSIBLE_HALL_TEXTURES_READY',len(files))
