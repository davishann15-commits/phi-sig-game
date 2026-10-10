"""Re-author the two remaining visual issues without rewriting other mesh files."""
import json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen')
prior=json.loads((R/'manifest.json').read_text());old={a['id']:a for a in prior['assets']}
source=(R/'source/build_kitchen.py').read_text()
start=source.index('def finish(g,');end=source.index('# The photo chairs',start)
source=source[:start]+'''def finish(g,*args,**kw):
 if g not in {'ServingCounter','AutumnBouquet'}:
  a=old[g];manifest['assets'].append(a)
  for o in G.pop(g,[]):DM['discard'](o)
  return a
 a=finalize(g,*args,**kw);print('KITCHEN_REFINED',g,a['faces'],flush=True);return a
'''+source[end:]
start=source.index("bpy.ops.import_scene.fbx(filepath=a['intact_fbx'])")
end=source.index('# Physical UV scale',start)
source=source[:start]+source[end:]
exec(compile(source,'build_kitchen.py','exec'))
d=json.loads((R/'manifest.json').read_text());d['assets'] += [a for a in prior['assets'] if a.get('source_actor')]
d['refresh_assets']=['ServingCounter','AutumnBouquet'];d['refresh_materials']=[];d['refresh_textures']=[]
(R/'manifest.json').write_text(json.dumps(d,indent=2))
