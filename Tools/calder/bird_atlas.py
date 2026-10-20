"""Original code-painted shared goose/bat palette; no reference pixels."""
import hashlib,json,math,struct,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'SourceArt/CalderLandscape/phase3/sky';OUT.mkdir(exist_ok=True)
target=OUT/'T_CalderBirdAtlas.png';assert not target.exists()
colours=[(115,103,87),(29,30,30),(189,184,166),(216,213,198),
         (70,65,57),(39,39,37),(86,52,32),(141,67,37),
         (55,34,28),(77,42,30),(19,17,14),(8,8,7),
         (129,119,99),(92,85,73),(168,158,136),(44,43,40)]
raw=bytearray()
for y in range(256):
 raw.append(0)
 for x in range(256):
  cell=(y//64)*4+x//64;u=(x%64)/64;v=(y%64)/64
  grain=math.sin(x*12.9898+y*78.233)*43758.5453;grain=grain-math.floor(grain)
  barb=math.sin((u+.045*math.sin(v*19))*math.pi*28)
  shade=.94+.07*grain+.045*barb
  raw.extend(max(0,min(255,round(c*shade))) for c in colours[cell])
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',256,256,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
target.write_bytes(png)
(OUT/'atlas.json').write_text(json.dumps({'original_code_painted':True,'reference_pixels':False,'dimensions':[256,256],'cells':colours,'sha256':hashlib.sha256(png).hexdigest()},indent=2)+'\n')
print('BIRD_ATLAS code-painted 256x256 cells=16')
