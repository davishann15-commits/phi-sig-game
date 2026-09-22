"""Read image data and emit geometry only; does not create/edit bitmap assets."""
import json
from pathlib import Path
import numpy as np
from PIL import Image

project=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff')
rgb=np.asarray(Image.open(project/'Content/Lobby/SourceArt/LobbyBackdrop.png').convert('RGB'),dtype=np.int16)
mask=np.asarray(Image.open(project/'SourceArt/Lobby/PhotoMotion/GroundLeafMatte.png').convert('L'))
living=np.asarray(Image.open(project/'SourceArt/Lobby/PhotoMotion/FoliageMatte.png').convert('L'))
r,g,b=rgb[:,:,0],rgb[:,:,1],rgb[:,:,2]
# The stair treads in this source contain no fallen leaves; the line of litter
# starts at their foot. Excluding the treads prevents warm stone being selected.
selected=(mask>100)&(living<=100)
selected[:740] &= ((r-g>5)&(r-b>9)&(r>24))[:740]
selected[:714]=False
remaining=set(zip(*np.nonzero(selected)))
components=[]
while remaining:
    first=min(remaining); remaining.remove(first); todo=[first]; pixels=[]
    while todo:
        y,x=todo.pop();pixels.append((int(y),int(x)))
        for dy,dx in [(0,1),(0,-1),(1,0),(-1,0),(1,1),(1,-1),(-1,1),(-1,-1)]:
            q=(y+dy,x+dx)
            if q in remaining:remaining.remove(q);todo.append(q)
    # Include even the tiny distant specks: never erase an unrepresented pixel.
    ys,xs=zip(*pixels)
    runs=[]
    for y in sorted(set(ys)):
        row=sorted(x for yy,x in pixels if yy==y)
        start=last=row[0]
        for x in row[1:]:
            if x==last+1:last=x
            else:runs.append((start,y,last+1));start=last=x
        runs.append((start,y,last+1))
    # Merge identical runs on consecutive rows without filling any gaps.
    active={}; rects=[]
    for y in range(min(ys),max(ys)+2):
        current={(x,x2) for x,yy,x2 in runs if yy==y}
        for key in list(active):
            if key not in current:
                rects.append((key[0],active.pop(key),key[1],y))
        for key in current:
            if key not in active:active[key]=y
    components.append({'center':[(min(xs)+max(xs)+1)/2,(min(ys)+max(ys)+1)/2],
                       'area':len(pixels),'rects':rects})
rectangles=[]; leaves=[]
for c in components:
    leaves.append([*c['center'],c['area'],len(rectangles),len(c['rects'])])
    rectangles.extend(c['rects'])
header='// Generated from original-photo leaf pixels. Do not hand-edit.\n'
header+='struct FPhotoLeafSource { float X,Y,Area; int32 First,Count; };\n'
header+='struct FPhotoLeafRect { uint16 X1,Y1,X2,Y2; };\n'
header+='static const FPhotoLeafSource PhotoLeafSources[] = {\n'
header+='\n'.join('    {'+','.join(f'{x:.1f}f' for x in v[:3])+','+','.join(str(x) for x in v[3:])+'},' for v in leaves)
header+='\n};\nstatic const FPhotoLeafRect PhotoLeafRects[] = {\n'
header+='\n'.join('    {'+','.join(str(x) for x in v)+'},' for v in rectangles)+'\n};\n'
assert sum((x2-x1)*(y2-y1) for x1,y1,x2,y2 in rectangles)==int(selected.sum())
print(json.dumps({'header':header,'leaves':len(leaves),'rectangles':len(rectangles),
    'pixels':int(selected.sum()),'vertices':len(rectangles)*4,
    'stair_foot_pixels':int(selected[714:734,520:1110].sum()),
    'left_border_pixels':int(selected[714:753,:520].sum()),
    'right_border_pixels':int(selected[714:753,1110:].sum())}))
