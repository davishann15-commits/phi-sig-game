import json,shutil
from pathlib import Path
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-room-v1');O=R/'import';O.mkdir(exist_ok=True)
p={'materials':{},'textures':[],'flip_green_textures':[],'meshes':[],'geometry':{}}
for group in ['pigs','props','geometry','surfaces']:
 mf=R/group/'incremental_manifest.json'
 if not mf.exists():raise RuntimeError('Missing '+str(mf))
 q=json.load(open(mf))
 for t in q.get('textures',[]):
  src=Path(t['path']);shutil.copy2(src,O/src.name);p['textures'].append(src.name)
  if t.get('flipGreenChannel',t.get('role',t.get('kind'))=='Normal'):p['flip_green_textures'].append(src.name)
 mats=q.get('materials',[])
 for m in mats:
  info={'color':m.get('baseColor',[.4,.4,.4]),'roughness':m.get('roughness',.8),'metallic':m.get('metallic',0),'normal_strength':m.get('normalStrength',.4),'two_sided':m.get('twoSided',False)}
  for k,out in [('BaseColor','base_texture'),('Normal','normal_texture'),('Roughness','roughness_texture')]:
   if m.get('textures',{}).get(k):info[out]=Path(m['textures'][k]).stem+'.png'
  if m.get('emissionStrength'):info['emission']=m['emissionStrength']
  if m.get('blendMode')=='BLEND_TRANSLUCENT':info.update(translucent=True,opacity=m.get('opacity',.35))
  if group=='surfaces' and m.get('addressMode')=='Clamp':info['clamp_textures']=True
  if m.get('textures',{}).get('BaseColorAlpha'):info.update(base_texture=m['textures']['BaseColorAlpha']+'.png',alpha_opacity=True,two_sided=True)
  if m.get('blendMode')=='BLEND_MASKED':info.update(masked=True,mask_texture=info['base_texture'],mask_channel='A',clip=.2)
  p['materials'][m['name']]=info
 for m in q.get('meshes',[]):
  src=Path(m['fbx']);shutil.copy2(src,O/src.name)
  p['meshes'].append({'name':m['name'],'file':src.name,'group':group,'boundsCm':m.get('boundsCm'),'complex_collision':m.get('complex_collision',m.get('collision')=='BlockAll')})
 if group=='geometry':p['geometry']=q
if len(p['textures'])!=len(set(p['textures'])):raise RuntimeError('Duplicate textures')
if len(p['meshes'])!=len({m['name'] for m in p['meshes']}):raise RuntimeError('Duplicate meshes')
json.dump(p,open(O/'import_manifest.json','w'),indent=2)
print('PIGSTY_IMPORT_ASSEMBLED',len(p['meshes']),len(p['materials']),len(p['textures']))
