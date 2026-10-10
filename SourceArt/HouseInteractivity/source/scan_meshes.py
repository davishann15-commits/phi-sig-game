import bpy,json,sys,os,collections,glob
args=sys.argv[sys.argv.index('--')+1:]
out=args[0]; paths=args[1:]
def parts(o):
 m=o.data;p=list(range(len(m.vertices)))
 def f(x):
  while p[x]!=x:p[x]=p[p[x]];x=p[x]
  return x
 for poly in m.polygons:
  for v in poly.vertices[1:]:p[f(v)]=f(poly.vertices[0])
 groups=collections.defaultdict(list)
 for poly in m.polygons:groups[f(poly.vertices[0])].append(poly.index)
 ps=[]
 for fs in groups.values():
  cs=[o.matrix_world@m.vertices[v].co for v in {v for fi in fs for v in m.polygons[fi].vertices}]
  lo=[min(c[k] for c in cs) for k in range(3)];hi=[max(c[k] for c in cs) for k in range(3)]
  ps.append(dict(faces=len(fs),lo=lo,hi=hi,materials=sorted({m.materials[m.polygons[fi].material_index].name if m.materials[m.polygons[fi].material_index] else '' for fi in fs})))
 return ps
report={}
for path in paths:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 if path.endswith('.blend'):bpy.ops.wm.open_mainfile(filepath=path)
 else:bpy.ops.import_scene.fbx(filepath=path)
 report[path]=[{ 'name':o.name,'faces':len(o.data.polygons),'parts':parts(o)} for o in bpy.context.scene.objects if o.type=='MESH']
json.dump(report,open(out,'w'),indent=2)
print('SCAN_DONE',out,len(report))
