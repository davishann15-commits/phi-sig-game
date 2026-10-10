from pathlib import Path
import numpy as np
from PIL import Image,ImageFilter,ImageDraw
O=Path(__file__).parent/'textures';O.mkdir(exist_ok=True)
rng=np.random.default_rng(23107);n=2048

def blur_noise(scale,amp):
    size=max(8,n//scale)
    a=rng.normal(128,amp,(size,size)).clip(0,255).astype('uint8')
    return np.asarray(Image.fromarray(a).resize((n,n),Image.Resampling.BICUBIC).filter(ImageFilter.GaussianBlur(scale*.20)),float)-128
coarse=blur_noise(160,22);medium=blur_noise(21,28);fine=blur_noise(2,25)
# Pig hide is near-black with subtle irregular vascular/tan grime variation.
creases=Image.new('L',(n,n),128);d=ImageDraw.Draw(creases)
for i in range(430):
 x,y=rng.integers(0,n,2);ln=int(rng.integers(14,125));sl=float(rng.uniform(-.7,.7))
 p=[(int(x+j*sl+4*np.sin(j*.055)),int(y+j)) for j in range(0,ln,3)]
 d.line(p,fill=int(rng.integers(85,114)),width=int(rng.integers(1,3)))
creases=np.asarray(creases.filter(ImageFilter.GaussianBlur(.8)),float)-128
hair=Image.new('L',(n,n),128);d=ImageDraw.Draw(hair)
for i in range(28000):
 x,y=rng.integers(0,n,2);ln=int(rng.integers(4,23));sl=int(rng.integers(-4,5))
 d.line((x,y,x+sl,y+ln),fill=int(rng.integers(75,116)),width=1)
hair=np.asarray(hair.filter(ImageFilter.GaussianBlur(.25)),float)-128
pore=np.zeros((n,n),float)
for i in range(28000):
 x,y=rng.integers(3,n-3,2);pore[y-1:y+2,x-1:x+2]-=rng.uniform(.15,.55)
pore=np.asarray(Image.fromarray(np.clip(128+pore*50,0,255).astype('uint8')).filter(ImageFilter.GaussianBlur(.4)),float)-128
base=21+coarse*.19+medium*.065+fine*.01+hair*.03
base=np.stack([base*.97+medium*.004,base,base*1.025],-1)
height=medium*.018+fine*.009+creases*.04+hair*.014+pore*.033
rough=182+coarse*.35+medium*.22+fine*.025-hair*.08

def save_set(prefix,base,rough,height,strength):
 Image.fromarray(base.clip(4,50).astype('uint8'),'RGB').save(O/(prefix+'_BaseColor.png'))
 Image.fromarray(rough.clip(0,255).astype('uint8'),'L').save(O/(prefix+'_Roughness.png'))
 dy,dx=np.gradient(height);normal=np.stack([-dx*strength,-dy*strength,np.ones_like(dx)],-1);normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
 Image.fromarray(((normal*.5+.5)*255).clip(0,255).astype('uint8'),'RGB').save(O/(prefix+'_Normal.png'))
save_set('PIG_T_BlackHide',base,rough,height,.27)
# Leathery wet snout: tighter pores, shallow dry lines, less uniform gloss.
snout=17+coarse*.10+medium*.026+fine*.015
save_set('PIG_T_BlackSnout',np.stack([snout*1.12,snout*1.03,snout],-1),142+coarse*.30+medium*.35, fine*.018+pore*.13+creases*.036,.4)
# Hoof horn longitudinal striations with worn but not chrome surface.
y,x=np.indices((n,n));striation=np.sin(x*.53+np.sin(y*.008)*2)+np.sin(x*.09+np.sin(y*.002)*3)*.6
horn=18+coarse*.18+striation*1.6
save_set('PIG_T_HoofKeratin',np.stack([horn*1.10,horn,horn*.90],-1),112+medium*.2+striation*9,striation*.21+fine*.015,.5)
print('Nine 2048 maps ready: hide, damp snout, striated hoof')
