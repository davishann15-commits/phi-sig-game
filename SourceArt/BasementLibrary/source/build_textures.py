from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import numpy as np, math
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library');D=R/'textures';D.mkdir(exist_ok=True)
rng=np.random.default_rng(2266);N=1024;y,x=np.mgrid[:N,:N]/N
def save(name,a):Image.fromarray(np.uint8(np.clip(a,0,255))).save(D/(name+'.png'))
noise=rng.normal(0,1,(N,N))
for name,color in [('Walnut',[115,65,29]),('Oak',[178,128,60])]:
 grain=np.sin(x*470+np.sin(y*7)*4+np.sin(y*24+x*5)*3)+.55*np.sin(x*1130+np.sin(y*12)*6)
 knot=np.exp(-((x-.65)**2+(y-.32)**2*7)*55);grain+=np.sin(np.sqrt((x-.65)**2+(y-.32)**2*7)*200)*knot*2
 a=np.stack([color[k]+grain*(7 if k<2 else 4)+noise*1.7 for k in range(3)],2);save(name,a)
 save(name+'_Roughness',np.repeat((146+grain*8+noise*4)[:,:,None],3,2))
fine=rng.normal(0,2,(N,N))+np.sin(x*4000)*np.sin(y*3900)*1.4
save('Leather',np.stack([22+fine,23+fine,24+fine],2))
save('Leather_Roughness',np.repeat((100+fine*3)[:,:,None],3,2))
weave=np.sin(x*3200)*3+np.sin(y*3300)*2+noise*1.2
save('GrayFabric',np.stack([129+weave,125+weave,116+weave],2))
floor=75+noise*2+np.sin(x*13+y*9)*3+np.sin(x*90-y*65)*1.3
save('Concrete',np.stack([floor+7,floor+2,floor-7],2))
im=Image.new('RGB',(1024,1024),(126,121,108));d=ImageDraw.Draw(im)
for q in range(8):
 for p in range(6):
  cx=(p+.5)*170.66;cy=(q+.5)*128
  for a in np.linspace(0,2*math.pi,12,endpoint=False):
   ex=cx+math.cos(a)*35;ey=cy+math.sin(a)*30
   d.ellipse([ex-15,ey-23,ex+15,ey+23],outline=(185,177,155),width=3)
  d.ellipse([cx-29,cy-29,cx+29,cy+29],outline=(175,168,148),width=3)
  d.ellipse([cx-8,cy-8,cx+8,cy+8],fill=(186,179,157))
for z in [8,19,32]:d.rectangle([z,z,1023-z,1023-z],outline=(184,176,153),width=3)
a=np.array(im,dtype=float)+rng.normal(0,3,(1024,1024,1));save('DamaskRug',a)
im=Image.new('RGB',(1024,1024),(41,75,42));d=ImageDraw.Draw(im)
for yy in range(-32,1056,32):
 for xx in range(-32,1056,32):
  d.rectangle([xx+2,yy+2,xx+30,yy+30],outline=(132,162,100),width=2)
  cx=xx+16;cy=yy+16
  if (xx//32+yy//32)%2:
   d.polygon([(cx,cy-9),(cx-9,cy),(cx,cy+9),(cx+9,cy)],fill=(120,152,93))
  else:
   for dx,dy in [(-4,0),(4,0),(0,-4)]:d.ellipse([cx+dx-4,cy+dy-4,cx+dx+4,cy+dy+4],fill=(120,152,93))
   d.polygon([(cx-3,cy+8),(cx,cy),(cx+3,cy+8)],fill=(120,152,93))
im.save(D/'GreenCardCloth.png')
im=Image.new('RGB',(128,128),(0,0,0));d=ImageDraw.Draw(im)
for a in range(0,128,16):
 d.line([(a,0),(a,127)],fill=(255,255,255),width=3);d.line([(0,a),(127,a)],fill=(255,255,255),width=2)
im.save(D/'OfficeMeshMask.png')
fontpath='/System/Library/Fonts/Supplemental/Arial.ttf'
def font(n):return ImageFont.truetype(fontpath,n)
im=Image.new('RGB',(768,1024),(234,231,220));d=ImageDraw.Draw(im)
d.text((55,45),'STUDY NOTES',fill=(28,30,28),font=font(28))
for k in range(29):
 s=['Chapter review and practice questions','  Key points, examples and discussion','  Read before the next meeting','  Notes and revision'][k%4]
 d.text((55,108+29*k),s,fill=(48,49,46),font=font(18))
d.line([(75,600),(490,680),(180,810)],fill=(62,70,77),width=3);im.save(D/'PaperNotes.png')
im=Image.new('RGB',(1024,768),(18,23,25));d=ImageDraw.Draw(im)
for c,head in [(0,'KEYBOARD SHORTCUTS'),(1,'QUICK REFERENCE'),(2,'WORK AND STUDY')]:
 d.text((24+c*338,30),head,fill=(117,183,190),font=font(19))
 for k in range(22):
  d.rectangle([26+c*338,78+k*27,90+c*338,99+k*27],outline=(94,127,132),width=1)
  d.text((28+c*338,81+k*27),'CTRL '+chr(65+k),fill=(162,190,181),font=font(11))
  d.text((101+c*338,81+k*27),['Select and edit','Save document','Move between cells','Open reference'][k%4],fill=(173,177,173),font=font(12))
d.text((35,697),'Master your computer - study with ease',fill=(223,213,171),font=font(29));im.save(D/'DeskMat.png')
print('LIBRARY_TEXTURES_READY')
