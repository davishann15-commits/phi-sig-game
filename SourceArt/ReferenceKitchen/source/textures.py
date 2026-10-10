from PIL import Image,ImageDraw,ImageFont,ImageOps
from pathlib import Path
import numpy as np
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen');D=R/'textures';D.mkdir(exist_ok=True);rng=np.random.default_rng(2254);N=1024;y,x=np.mgrid[:N,:N]
def save(n,a):Image.fromarray(np.uint8(np.clip(a,0,255))).save(D/(n+'.png'))
# Fine grain and satin scratches; no baked room lighting.
grain=np.sin(x*.047+4*np.sin(y*.008))+ .4*np.sin(x*.21+np.sin(y*.02)*2)
base=np.array([75,44,29])+grain[:,:,None]*np.array([9,6,4])+rng.normal(0,1.3,(N,N,1));save('DarkWalnut',base);save('DarkWalnut_Roughness',np.ones((N,N))*145+grain*8)
a=np.ones((N,N,3))*np.array([154,157,158])+rng.normal(0,1,(N,N,1));a+=rng.normal(0,2,(N,1,1));save('BrushedSteel',a);save('BrushedSteel_Roughness',np.ones((N,N))*96+rng.normal(0,8,(N,1)))
a=np.ones((N,N,3))*np.array([18,68,49])+rng.normal(0,2,(N,N,1));save('GreenVinyl',a)
# One tile represents 4.8 x 3.2 metres: 20cm boards with varied long joints.
# Colours are deliberately neutral brown; the kitchen's warm light supplies warmth.
F=2048;fy,fx=np.mgrid[:F,:F];a=np.zeros((F,F,3),dtype=np.float32);height=np.zeros((F,F),dtype=np.float32);rough=np.zeros((F,F),dtype=np.float32)
for row in range(16):
 y0=row*128;yy,xx=np.mgrid[:128,:F];start=-int(rng.integers(80,650))
 while start<F:
  length=int(rng.integers(450,960));end=min(F,start+length);left=max(0,start);u=xx[:,left:end]-start;v=yy[:,left:end]
  warp=3*np.sin(u*.007+row)+1.5*np.sin(u*.017+.6*row)
  grain=2.6*np.sin((v+warp)*.21)+1.2*np.sin((v+warp)*.58)+.65*np.sin((v+warp)*1.31)
  tone=float(rng.uniform(-14,13));base=np.array([124,101,74])+tone*np.array([1,.85,.65]);chunk=base+grain[:,:,None]*np.array([1,.83,.61])+rng.normal(0,.9,(*u.shape,1))
  relief=grain*.10;rr=165+grain*1.8+rng.normal(0,2,u.shape)
  if rng.random()<.52:
   kx=rng.uniform(35,max(36,length-35));ky=rng.uniform(24,105);radius=np.sqrt(((u-kx)/1.9)**2+(v-ky)**2)
   knots=np.sin(radius*.7)*np.exp(-radius/22)*11+np.exp(-radius*radius/58)*23
   chunk-=knots[:,:,None]*np.array([1,.82,.61]);relief-=knots*.09
  # Small irregular scuffs distributed within individual boards.
  for _ in range(3):
   sx=rng.uniform(0,length);sy=rng.uniform(6,121);scar=np.exp(-((u-sx)/rng.uniform(28,110))**2-((v-sy)/rng.uniform(1,3))**2)*rng.uniform(2,8)
   chunk+=scar[:,:,None];rr+=scar
  if start>=0:
   chunk[:,max(0,start-left):max(0,start-left)+2]*=.54;relief[:,:2]-=1.5
  chunk[:2]*=.65;chunk[-1:]*=.81;relief[:2]-=1.4
  a[y0:y0+128,left:end]=chunk;height[y0:y0+128,left:end]=relief;rough[y0:y0+128,left:end]=rr
  start+=length
save('OakPlanks',a);save('OakPlanks_Roughness',rough)
gy,gx=np.gradient(height);normal=np.stack([-gx*.22,gy*.22,np.ones_like(gx)],axis=-1);normal/=np.linalg.norm(normal,axis=-1,keepdims=True);save('OakPlanks_Normal',(normal*.5+.5)*255)
save('PaperTowel',np.ones((N,N,3))*235 + (np.sin(x*.31)*np.sin(y*.31))[:,:,None]*6)
for name,img,q in [('CityPrint','IMG_2258',(628,70,636,184,795,169,784,61)),('CafePrint','IMG_2256',(963,216,954,365,1049,359,1058,195)),('RulesPrint','IMG_2256',(347,317,347,423,384,409,382,309))]:
 im=ImageOps.exif_transpose(Image.open(R/'reference'/(img+'.png')));im.transform((512,512 if name=='CityPrint' else 768),Image.Transform.QUAD,q,Image.Resampling.BICUBIC).save(D/(name+'.png'))
font='/System/Library/Fonts/Supplemental/Arial Bold.ttf'
for name,col,title,sub in [('FoilBox',(24,38,86),'Aluminum Foil','75 SQ FT'),('ZipBox',(22,89,148),'Ziploc','SLIDER STORAGE'),('SnackBox',(21,67,117),'classic mix','54 VARIETY PACK'),('PaperPack',(235,91,25),'Paper Cups','PREMIUM'),('CerealLabel',(239,111,23),'T','TENNESSEE'),('PeanutLabel',(26,104,50),'Jif','CREAMY PEANUT BUTTER'),('CocoaLabel',(24,93,173),'Swiss Miss','MILK CHOCOLATE'),('EnergyLabel',(218,220,216),'Red Bull','ENERGY DRINK 24 CANS'),('BreadLabel',(35,112,65),'Nature\u2019s Own','HONEY WHEAT'),('ForkLabel',(44,119,177),'Plastic Forks','100 COUNT'),('CoolerLabel',(233,232,221),'IGLOO','OUTDOOR SERIES')]:
 im=Image.new('RGB',(1024,512),col);d=ImageDraw.Draw(im)
 if name=='PeanutLabel':
  d.rectangle((140,40,320,370),fill=(191,40,27));d.rectangle((320,40,650,370),fill=(244,239,216));d.rectangle((650,40,810,370),fill=(43,118,136));d.text((205,60),'Jif',font=ImageFont.truetype(font,285),fill=(16,50,94))
 elif name=='EnergyLabel':
  d.rectangle((520,0,1024,512),fill=(32,48,103));d.text((65,75),title,font=ImageFont.truetype(font,115),fill=(198,31,33));d.ellipse((125,228,280,380),fill=(225,171,42));d.ellipse((280,228,435,380),fill=(196,35,29))
 elif name=='BreadLabel':
  d.rectangle((0,155,1024,355),fill=(228,213,158));d.text((40,175),title,font=ImageFont.truetype(font,115),fill=(35,100,53))
 else:
  d.rectangle((18,18,1005,493),outline='white',width=5);d.text((45,105),title,font=ImageFont.truetype(font,105 if len(title)>5 else 250),fill='white')
 d.text((55,402),sub,font=ImageFont.truetype(font,35),fill=(242,235,215) if name!='BreadLabel' else (35,100,53))
 for i in range(9):d.line((710+i*13,448,710+i*13,490),fill=(12,18,22),width=3+i%3)
 im.save(D/(name+'.png'))
# Sparse dirt/chipped-paint marks, transparent rather than square decals.
im=Image.new('RGB',(512,512),(75,70,61));mask=Image.new('L',(512,512));dr=ImageDraw.Draw(mask)
for _ in range(95):
 px=int(rng.integers(30,482));py=int(rng.integers(60,490));w=int(rng.integers(2,25));h=int(rng.integers(1,4));dr.ellipse((px,py,px+w,py+h),fill=int(rng.integers(18,110)))
im.save(D/'WallScuffs.png');mask.save(D/'WallScuffs_Mask.png')
print('KITCHEN_TEXTURES_READY')
