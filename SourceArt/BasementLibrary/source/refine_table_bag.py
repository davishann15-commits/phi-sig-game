"""The worktable reference has a loose gray jacket over a black nylon bag."""
import json,math
from pathlib import Path
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library')
saved=json.loads((R/'manifest.json').read_text())
exec(compile((R/'source/build_library.py').read_text().split('def swivel(')[0],str(R/'source/build_library.py'),'exec'))
nylon=mat('NylonFabric',(.015,.018,.020),.64)
g='GrayBackpack'
box('soft black nylon bag',(0,0,.079),(.46,.30,.158),nylon,g,.067,6)
for x in [-.13,.13]:tube('loose carry loop',[(x,-.12,.09),(x,-.22,.035),(x,-.25,.06),(x,-.16,.15)],.010,nylon,g)
for y in [-.15,.15]:tube('zipper seam',[(-.16,y,.12),(-.06,y*1.05,.16),(.16,y,.12)],.0025,silver,g)
# Continuous cloth with asymmetric overlapping folds and soft draped edges.
nx,ny=32,26;v=[];f=[]
for j in range(ny+1):
 t=j/ny;y=(t-.5)*.37
 for i in range(nx+1):
  q=i/nx;x=(q-.5)*.49
  edge=math.sin(q*math.pi)*math.sin(t*math.pi)
  z=.074+.105*edge+.021*math.sin(x*45+y*21)*edge+.012*math.sin(y*62-x*15)*edge
  v.append((x,y,z))
for j in range(ny):
 for i in range(nx):k=j*(nx+1)+i;f.append((k,k+1,k+nx+2,k+nx+1))
coat=mesh('wrinkled gray jacket',v,f,graycloth,g,True)
b=coat.modifiers.new('cloth thickness','SOLIDIFY');b.thickness=.002;bpy.context.view_layer.objects.active=coat;bpy.ops.object.modifier_apply(modifier=b.name)
tube('folded jacket sleeve',[(-.18,-.13,.12),(-.22,-.03,.17),(-.13,.12,.15),(-.03,.16,.17)],.034,graycloth,g,sides=12)
tube('jacket stitched hem',[(x,-.183,.078+.013*math.sin(x*30))for x in [-.24,-.18,-.12,-.06,0,.06,.12,.18,.24]],.0017,linen,g)
row=finalize(g,5,24)
saved['assets']=[a for a in saved['assets'] if a['id']!=g]+[row]
saved['materials'].update({k:v for k,v in manifest['materials'].items() if k=='M_Lib_NylonFabric'})
saved['refresh_assets']=sorted(set(saved.get('refresh_assets',[]))|{g})
(R/'manifest.json').write_text(json.dumps(saved,indent=2))
print('WORKTABLE_BAG_REFINED',flush=True)
