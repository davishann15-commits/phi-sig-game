import json,math,struct
from pathlib import Path
from PIL import Image,ImageDraw
import numpy as np
B=Path(__file__).resolve().parent
polys=json.load(open(B/'context/polygons.json'))
for name,z0,z1 in [('basement',-360,-170),('main',1,170),('upper',345,490)]:
 im=Image.new('RGB',(1500,1600),'#ebeae4');d=ImageDraw.Draw(im);S=.42
 def pt(p):return ((p[0]-900)*S+100,(p[1]+2200)*S+60)
 for row in polys:
  ps=row['p'];zs=[p[2]for p in ps]
  if min(zs)>z1 or max(zs)<z0:continue
  if max(zs)-min(zs)<.1 and min(zs)>z0+20:continue
  col='#ddd3bf'if max(zs)-min(zs)<.1 else '#454748'
  d.polygon([pt(p)for p in ps],fill=col)
 for x in range(1000,4200,200):
  d.line([pt((x,-2200)),pt((x,1500))],fill='#acb4b5',width=1);d.text(pt((x,-2300)),str(x),fill='black')
 for y in range(-2200,1600,200):
  d.line([pt((900,y)),pt((4200,y))],fill='#acb4b5',width=1);d.text(pt((800,y)),str(y),fill='black')
 d.text((15,15),name+' X right, Y down, Unreal world centimetres',fill='black');im.save(B/'context'/('plan_'+name+'.png'))
# GLB scan top down. Triangles sampled with original scan texture colours.
for f in (B/'scans').glob('*.glb'):
 data=f.read_bytes();n=struct.unpack_from('<I',data,12)[0];g=json.loads(data[20:20+n]);blen=struct.unpack_from('<I',data,20+n)[0];blob=data[28+n:28+n+blen]
 def acc(i):
  a=g['accessors'][i];v=g['bufferViews'][a['bufferView']];dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']];dims={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];return np.frombuffer(blob,dtype=dtype,count=a['count']*dims,offset=v.get('byteOffset',0)+a.get('byteOffset',0)).reshape(-1,dims)
 tris=[]
 for m in g['meshes']:
  for p in m['primitives']:
   vs=acc(p['attributes']['POSITION']);ix=acc(p['indices']).reshape(-1,3)
   for tri in vs[ix]:
    if tri[:,1].min()<.5:tris.append(tri)
 arr=np.array(tris);v=arr.reshape(-1,3);lo=v.min(0);hi=v.max(0);S=min(1300/(hi[0]-lo[0]),1400/(hi[2]-lo[2]));im=Image.new('RGB',(1450,1550),'white');d=ImageDraw.Draw(im)
 # floor lower gray; vertical faces dark. Lines preserve disconnected geometry for dimensions.
 order=np.argsort(arr[:,:,1].mean(1))
 for i in order:
  t=arr[i];h=t[:,1].mean();normal=np.cross(t[1]-t[0],t[2]-t[0]);normal/=max(np.linalg.norm(normal),1e-12);val=int(np.clip(190+h*18,30,220));col=(val,val,val)if abs(normal[1])>.6 else(55,63,70)
  d.polygon([(60+(p[0]-lo[0])*S,60+(p[2]-lo[2])*S)for p in t],fill=col)
 for x in range(math.ceil(lo[0]),math.ceil(hi[0])):
  X=60+(x-lo[0])*S;d.line([(X,40),(X,1500)],fill='#e8d0d0');d.text((X,12),str(x),fill='red')
 for z in range(math.ceil(lo[2]),math.ceil(hi[2])):
  Z=60+(z-lo[2])*S;d.line([(40,Z),(1400,Z)],fill='#e8d0d0');d.text((12,Z),str(z),fill='red')
 d.text((50,1520),f.stem+' / GLB X right, Z down, metres',fill='black');im.save(B/'context'/(f.stem+'_plan.png'))
