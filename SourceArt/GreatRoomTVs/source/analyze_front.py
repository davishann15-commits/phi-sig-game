import bpy,json,collections
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs')
rows=[]
for label in ['SM_Entrance_m1_p0_Solid','SM_Upper_p0_m1_Solid','SM_Upper_p0_p0_Solid']:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.fbx(filepath=str(R/'placement-fix'/(label+'.fbx')))
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'))
 planes=collections.defaultdict(lambda:dict(faces=0,bounds=[1e9,1e9,-1e9,-1e9],material=set()))
 point=[]
 for p in o.data.polygons:
  vs=[o.matrix_world @ o.data.vertices[i].co for i in p.vertices]
  xs=[v.x for v in vs];ys=[v.y for v in vs];zs=[v.z for v in vs]
  if max(xs)-min(xs)<.002 and min(zs)<3.7 and max(zs)>1.3 and min(ys)<.0 and max(ys)>-2.3 and 5.3<abs(sum(xs)/len(xs))<7.5:
   x=round(sum(xs)/len(xs),4);d=planes[x];d['faces']+=1;d['bounds']=[min(d['bounds'][0],min(ys)),min(d['bounds'][1],min(zs)),max(d['bounds'][2],max(ys)),max(d['bounds'][3],max(zs))];d['material'].add(o.data.materials[p.material_index].name)
   if min(ys)<=-1.055<=max(ys) and min(zs)<=2.4<=max(zs):point.append(dict(x=x,bounds=[min(ys),min(zs),max(ys),max(zs)],material=o.data.materials[p.material_index].name))
 rows.append(dict(label=label,matrix=[list(v) for v in o.matrix_world],planes=[dict(x=x,**{k:sorted(v) if isinstance(v,set) else v for k,v in d.items()}) for x,d in planes.items()],at_tv_center=point))
(R/'placement-fix/front-planes.json').write_text(json.dumps(rows,indent=2))
print('WALL_PLANES_READY')
