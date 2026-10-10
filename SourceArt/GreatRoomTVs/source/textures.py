from PIL import Image,ImageDraw,ImageFont,ImageFilter
from pathlib import Path
import math,random
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs');D=R/'textures';D.mkdir(exist_ok=True)
W,H=1280,720;r=random.Random(2404)
im=Image.new('RGB',(W,H));px=im.load()
for y in range(H):
 t=y/H
 for x in range(W):
  glow=max(0,1-math.hypot((x-870)/660,(y-295)/480));noise=r.uniform(-1,1)
  px[x,y]=tuple(max(0,min(255,int(c+glow*g+noise)))for c,g in zip((16+32*t,28+17*t,39+5*t),(43,18,4)))
d=ImageDraw.Draw(im)
d.ellipse((837,242,865,270),fill=(156,119,76))
for level,base,col in [(0,395,(44,54,58)),(1,450,(27,42,47)),(2,499,(16,31,35))]:
 pts=[(0,H),(0,base)]
 for x in range(0,W+31,30):pts.append((x,base+int(20*math.sin(x*.006+level*2)+15*math.sin(x*.019+level))))
 pts.extend([(W,H)]);d.polygon(pts,fill=col)
# The lake remains dim enough for the warm interior to dominate.
for y in range(545,H,4):
 width=max(1,140-int((y-545)*.37));xc=850+int(10*math.sin(y*.11));d.line((xc-width,y,xc+width,y),fill=(44,51,49),width=1)
for x in range(-15,W,55):
 h=r.randrange(42,95);y=548+r.randrange(-4,12)
 d.line((x,y,x,y-h),fill=(8,20,23),width=3)
 for k in range(4):
  yy=y-h+k*h/5;half=(k+1)*5;d.polygon([(x,yy-5),(x-half,yy+12),(x+half,yy+12)],fill=(8,20,23))
font='/System/Library/Fonts/Supplemental/Arial.ttf';bold='/System/Library/Fonts/Supplemental/Arial Bold.ttf'
def f(n,b=False):return ImageFont.truetype(bold if b else font,n)
d.text((68,51),'8:42',font=f(65),fill=(188,197,194));d.text((68,125),'SUNDAY EVENING',font=f(18),fill=(112,137,141));d.text((68,651),'COPPERSTEP',font=f(17,True),fill=(84,108,112))
im.save(D/'Screen0.png')
# LCD cracks radiate from a local impact, with fine branching rather than a white overlay.
def cracks(image,centers,level):
 q=ImageDraw.Draw(image)
 for center in centers:
  rays=[]
  for i in range(12+level*4):
   a=i*math.tau/(12+level*4)+r.uniform(-.12,.12);length=r.uniform(80,310)*(1+.35*level);pts=[center]
   for t in [.2,.42,.66,1]:pts.append((center[0]+math.cos(a)*length*t+r.uniform(-5,5),center[1]+math.sin(a)*length*t+r.uniform(-5,5)))
   q.line(pts,fill=(117,139,139),width=1 if i%3 else 2);rays.append(pts)
   for j,p in enumerate(pts[1:-1]):
    aa=a+r.choice([-1,1])*.7;ln=r.uniform(18,65);q.line([p,(p[0]+math.cos(aa)*ln,p[1]+math.sin(aa)*ln)],fill=(75,91,97),width=1)
  for ring in [23,52,86]:
   pts=[(center[0]+math.cos(i*math.tau/12)*ring*r.uniform(.84,1.1),center[1]+math.sin(i*math.tau/12)*ring*r.uniform(.84,1.1))for i in range(13)];q.line(pts,fill=(80,96,102),width=1)
a=im.copy();cracks(a,[(940,461)],0);a.save(D/'Screen1.png')
a=im.copy();q=ImageDraw.Draw(a)
for x in range(931,1280):
 height=int(460+160*math.sin(x*.05));q.line((x,0,x,max(0,min(H,height))),fill=(2,5,7),width=1)
for j in range(24):
 x=r.randrange(915,1260);q.line((x,0,x,H),fill=r.choice([(18,61,63),(56,19,37),(74,72,20)]),width=r.choice([1,2,3]))
q.polygon([(862,468),(901,379),(950,408),(1020,539),(999,608),(894,551)],fill=(0,2,3));cracks(a,[(940,461),(1225,46)],1);a.save(D/'Screen2.png')
a=Image.new('RGB',(W,H),(3,6,9));q=ImageDraw.Draw(a)
for i in range(55):
 x=r.randrange(W);y=r.randrange(H);q.line((x,y,x+r.randrange(15,90),y-r.randrange(5,30)),fill=(8,13,17),width=1)
cracks(a,[(940,461),(1225,46),(340,230)],2);a.save(D/'Screen3.png')
print('TV_TEXTURES_READY')
