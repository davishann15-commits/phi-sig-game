import bpy,bmesh,glob,collections,json,os,math,sys
root='/tmp/sso_house_original_meshes'
only_stem=next((arg.split('=',1)[1] for arg in sys.argv if arg.startswith('--only-stem=')),None)
outdir=next((arg.split('=',1)[1] for arg in sys.argv if arg.startswith('--outdir=')),'/tmp/sso_extracted_house')
os.makedirs(outdir,exist_ok=True)
manifest=[]
def partmesh(o,keep,name):
 new=o.data.copy();new.name=name+'Mesh';ob=bpy.data.objects.new(name,new);bpy.context.collection.objects.link(ob);ob.matrix_world=o.matrix_world.copy()
 bm=bmesh.new();bm.from_mesh(new);bm.faces.ensure_lookup_table();drop=[f for f in bm.faces if (f.index not in keep)]
 bmesh.ops.delete(bm,geom=drop,context='FACES');bm.to_mesh(new);bm.free();new.update()
 for v in bpy.context.selected_objects:v.select_set(False)
 ob.select_set(True);bpy.context.view_layer.objects.active=ob
 bpy.ops.export_scene.fbx(filepath=outdir+'/'+name+'.fbx',use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',bake_space_transform=False)
 bpy.data.objects.remove(ob,do_unlink=True)
for path in sorted(glob.glob(root+'/*.fbx')):
 stem=os.path.basename(path)[:-4]
 if only_stem and stem!=only_stem:continue
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 bpy.ops.import_scene.fbx(filepath=path)
 o=next((v for v in bpy.context.scene.objects if v.type=='MESH' and len(v.data.vertices)>100),None)
 if not o:continue
 m=o.data;parent=list(range(len(m.vertices)))
 def find(x):
  while parent[x]!=x:parent[x]=parent[parent[x]];x=parent[x]
  return x
 for f in m.polygons:
  for v in f.vertices[1:]:parent[find(v)]=find(f.vertices[0])
 groups=collections.defaultdict(list)
 for f in m.polygons:groups[find(f.vertices[0])].append(f.index)
 parts=[]
 for fs in groups.values():
  vs={v for fi in fs for v in m.polygons[fi].vertices};cs=[o.matrix_world@m.vertices[v].co for v in vs]
  lo=[min(c[k] for c in cs) for k in range(3)];hi=[max(c[k] for c in cs) for k in range(3)];sz=[hi[k]-lo[k] for k in range(3)]
  mat=m.materials[m.polygons[fs[0]].material_index].name.split('.')[0] if fs else ''
  parts.append(dict(fs=fs,lo=lo,hi=hi,sz=sz,mat=mat))
 candidates=[]
 for p in parts:
  s=p['sz'];w=max(s[0],s[1]);t=min(s[0],s[1]);mat=p['mat']
  thickness_limit=.22 if stem=='SM_Basement_p0_m1_Solid' and mat=='MI_SSO_BHAlcoveDoor' else .125
  if .7<=w<=1.55 and t<=thickness_limit and 1.9<=s[2]<=2.5 and mat in ('MI_SSO_PaintWhite','MI_SSO_PaintGrey','MI_SSO_PaintCream','MI_SSO_BHAlcoveDoor','MI_SSO_S03ConnectionDoorCream'):
   candidates.append(p)
 if stem=='SM_Basement_p0_m1_Solid':
  assert len(candidates)==1 and candidates[0]['mat']=='MI_SSO_BHAlcoveDoor',candidates
 if not candidates:continue
 removed=set();stem=os.path.basename(path)[:-4]
 for i,c in enumerate(candidates):
  lo=c['lo'];hi=c['hi'];major=0 if c['sz'][0]>c['sz'][1] else 1;normal=1-major
  keep=set()
  for p in parts:
   if p['lo'][major] < lo[major]-.07 or p['hi'][major] > hi[major]+.07:continue
   if p['lo'][normal] < lo[normal]-.16 or p['hi'][normal] > hi[normal]+.16:continue
   if p['lo'][2]<lo[2]-.05 or p['hi'][2]>hi[2]+.05:continue
   keep.update(p['fs'])
  if keep & removed:
   print('OVERLAP',stem,i,len(keep & removed));continue
  removed.update(keep)
  name='SM_OriginalLeaf_'+stem[3:]+'_'+str(i+1).zfill(2)
  partmesh(o,keep,name)
  manifest.append(dict(id=name,sourceMesh=stem,fbx=name+'.fbx',fbxBounds=[lo,hi],floor=('basement' if 'Basement' in stem else 'upper' if 'Upper' in stem else 'entrance'),faces=len(keep),material=c['mat']))
  print('EXTRACTED',name,len(keep),[round(v,2) for v in lo],[round(v,2) for v in hi])
 restname='SM_OriginalRoom_'+stem[3:]
 partmesh(o,set(range(len(m.polygons)))-removed,restname)
manifest.sort(key=lambda x:x['id']);json.dump(manifest,open(outdir+'/manifest.json','w'),indent=2)
print('EXTRACTED_TOTAL',len(manifest))
