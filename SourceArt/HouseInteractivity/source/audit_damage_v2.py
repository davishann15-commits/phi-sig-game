import os,json,re,hashlib,numpy as np
from io_scene_fbx import parse_fbx
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
d=json.load(open(R+'/extraction-manifest.json'));assets=json.load(open(R+'/inventory/damage-v2-assets.json'));rows={p['asset_name']:p for p in d['pieces']if p['behavior']=='furniture'}
def raw(path):
 tree,ver=parse_fbx.parse(path);obj=next(e for e in tree.elems if e.id==b'Objects');gs=[e for e in obj.elems if e.id==b'Geometry'];g=next(e for e in gs if not e.props[1].startswith(b'UCX'))
 arr=np.array(next(e.props[0]for e in g.elems if e.id==b'Vertices')).reshape((-1,3));poly=np.array(next(e.props[0]for e in g.elems if e.id==b'PolygonVertexIndex'))
 mats=[e.props[1].split(b'\0')[0].decode()for e in obj.elems if e.id==b'Material'];quantized=np.round(arr,5);unique=np.unique(quantized,axis=0);digest=hashlib.sha256(unique.tobytes()).hexdigest()
 models=[e.props[1].split(b'\0')[0].decode() for e in obj.elems if e.id==b'Model' and e.props[2]==b'Mesh'];main=next(n for n in models if not n.startswith('UCX_'));ucx_names_valid=all(n.startswith('UCX_'+main+'_')for n in models if n.startswith('UCX_'))
 return dict(ucx_names_valid=ucx_names_valid,materials=mats,vertices=len(arr),faces=int(sum(poly<0)),hulls=len(gs)-1,digest=digest,finite=bool(np.isfinite(arr).all()),lo=arr.min(axis=0).tolist(),hi=arr.max(axis=0).tolist())
out=[];repair=[];errors=[]
for stem,changes in assets['assets'].items():
 row=rows[stem];base=raw(row['intact_fbx']);stage=[raw(changes[k+'_fbx'])for k in ('damaged','heavily_damaged','broken')]
 problems=[]
 for k,x in zip(('damaged','heavily_damaged','broken'),stage):
  want=[p.split('/')[-1].split('.')[0]for p in changes[k+'_material_slots']]
  got=[re.sub(r'\.\d+$','',m)for m in x['materials']]
  if got!=want:errors.append(dict(asset=stem,stage=k,problem='material slot order',got=got,expected=want))
  if x['hulls']==0:problems.append(k+': zero hulls')
  if not x['ucx_names_valid']:errors.append(dict(asset=stem,stage=k,problem='UCX names do not match mesh node'))
  if not x['finite'] or x['faces']<1:errors.append(dict(asset=stem,stage=k,problem='invalid geometry'))
  if x['digest']==base['digest']:problems.append(k+': visually unchanged vertices')
 if stage[0]['digest']==stage[1]['digest']:problems.append('damage stages1 and2 visually identical')
 if problems:repair.append(dict(asset_name=stem,problems=problems))
 out.append(dict(asset_name=stem,intact=base,stages=dict(zip(('damaged','heavily_damaged','broken'),stage))))
result=dict(audit=out,repair=repair,errors=errors,passed=not repair and not errors,unique_assets=len(out),intact_unchanged=assets.get('intact_unchanged',False))
json.dump(result,open(R+'/inventory/damage-v2-validation.json','w'),indent=2)
print('V2_VALIDATION',len(out),'REPAIRS',len(repair),'ERRORS',len(errors));print(repair);print(errors[:5])
