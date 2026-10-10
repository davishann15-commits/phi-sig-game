from pathlib import Path
from PIL import Image,ImageDraw
import numpy as np,math
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library/textures')
rng=np.random.default_rng(2272);N=1024;y,x=np.mgrid[:N,:N]/N
def save(name,a):Image.fromarray(np.uint8(np.clip(a,0,255))).save(R/(name+'.png'))
for name,color in [('Walnut',[91,56,30]),('Oak',[164,123,69])]:
 # Quiet, irregular open grain and fine pores rather than repeating waves.
 xp=x+.003*np.sin(y*7)+.001*np.sin(y*31)
 grain=np.interp((xp%1).ravel(),np.linspace(0,1,360),rng.normal(0,1,360)).reshape(N,N)
 fine=np.interp((xp%1).ravel(),np.linspace(0,1,1900),rng.normal(0,1,1900)).reshape(N,N)
 knots=np.exp(-((x-.71)**2*200+(y-.45)**2*7));grain+=np.sin(np.sqrt((x-.71)**2*12+(y-.45)**2)*130)*knots*.4
 noise=rng.normal(0,.7,(N,N));a=np.stack([color[k]+grain*3+fine*1.1+noise for k in range(3)],2)
 save(name,a);save(name+'_Roughness',np.repeat((148+grain*4+noise*2)[:,:,None],3,2))
im=Image.new('RGB',(1024,1024),(119,113,99));d=ImageDraw.Draw(im)
for row in range(6):
 for col in range(5):
  cx=102.4+col*204.8;cy=85.3+row*170.7
  for sign in [-1,1]:
   for direction in [-1,1]:
    for off in [0,8]:
     pts=[(cx+sign*(9+32*math.sin(t*math.pi)),cy+direction*(8+52*t)+off) for t in np.linspace(0,1,25)]
     d.line(pts,fill=(158,150,131),width=2)
   for t in [.2,.45,.7]:
    ex=cx+sign*(15+29*math.sin(t*math.pi));ey=cy-60+120*t
    d.ellipse([ex-9,ey-4,ex+9,ey+4],outline=(177,168,145),width=2)
  for a in range(8):
   t=a*math.tau/8;ex=cx+math.cos(t)*14;ey=cy+math.sin(t)*14
   d.ellipse([ex-8,ey-8,ex+8,ey+8],outline=(177,168,145),width=2)
  d.ellipse([cx-5,cy-5,cx+5,cy+5],fill=(185,176,151))
for z in [6,11,27,35]:d.rectangle([z,z,1023-z,1023-z],outline=(170,160,139),width=2)
for k in range(32,992,24):
 for cx,cy in [(k,20),(k,1004),(20,k),(1004,k)]:d.ellipse([cx-5,cy-5,cx+5,cy+5],outline=(175,165,144),width=2)
a=np.array(im,dtype=float)+rng.normal(0,1.3,(1024,1024,1));save('DamaskRug',a)
print('POLISHED_TEXTURES_READY')
