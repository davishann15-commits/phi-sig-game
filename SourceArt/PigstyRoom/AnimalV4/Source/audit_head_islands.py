import bpy,json
from pathlib import Path
root=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-animal-v4')
bpy.ops.wm.open_mainfile(filepath='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/PigstyRoom/RefinementV2/Art/PIG_RealBlackPig_Source.blend')
o=bpy.data.objects['SM_PIG_BlackHead'];d=o.data
adj=[set() for v in d.vertices]
for e in d.edges: a,b=e.vertices;adj[a].add(b);adj[b].add(a)
seen=set();islands=[]
for i in range(len(adj)):
 if i in seen:continue
 todo=[i];seen.add(i);ids=[]
 while todo:
  n=todo.pop();ids.append(n)
  for v in adj[n]:
   if v not in seen:seen.add(v);todo.append(v)
 xyz=[d.vertices[i].co for i in ids]
 islands.append(dict(n=len(ids),ids=ids,min=[min(v[k]for v in xyz)for k in range(3)],max=[max(v[k]for v in xyz)for k in range(3)],mean=[sum(v[k]for v in xyz)/len(xyz)for k in range(3)]))
(root/'art/head-islands.json').write_text(json.dumps(islands))
print(json.dumps([{k:v for k,v in x.items()if k!='ids'}for x in islands if x['n']>100]))
