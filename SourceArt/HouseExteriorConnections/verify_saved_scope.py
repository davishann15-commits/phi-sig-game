"""Compare saved scene placement and fixtures with the pre-edit snapshot."""
import json,re
from pathlib import Path

B = Path(__file__).resolve().parent
before = {r['label']: r for r in json.loads((B/'context/actors.json').read_text())}
after = {r['label']: r for r in json.loads((B/'context/actors-after.json').read_text())}
targets = {r['actor']: r for r in json.loads((B/'collision-manifest.json').read_text())}
expected_added = {'COL_Connected_Rendered_'+k[3:] for k, r in targets.items() if r.get('custom_collision')}
sashes={r['actor']:r for r in json.loads((B/'sash-manifest.json').read_text())}
problems = []
if set(before)-set(after): problems.append('Existing actors removed: '+str(set(before)-set(after)))
if set(after)-set(before) != expected_added: problems.append('Unexpected new actors: '+str(set(after)-set(before)))
for label, a in before.items():
    if label not in after: continue
    z = after[label]
    for key in ('name', 'cls', 'fixture', 'door'):
        normalize=lambda v:re.sub(r' \(0x[0-9a-fA-F]+\)', '', json.dumps(v,sort_keys=True))
        av=a.get(key);zv=z.get(key)
        if key=='fixture' and sashes.get(label,{}).get('kind')=='frame':
            av=dict(av);zv=dict(zv);av.pop('breakaway_frame_mesh');zv.pop('breakaway_frame_mesh')
        if normalize(av) != normalize(zv): problems.append(label+' changed '+key)
    comps={c['name']:c for c in z['components']}
    extra=set(comps)-{c['name'] for c in a['components']}
    if extra and not(sashes.get(label,{}).get('kind')=='frame' and extra=={'BreakawayWindowGrille'}):problems.append(label+' unexpected component '+str(extra))
    for c in a['components']:
        if c['name'] not in comps:problems.append(label+' lost component '+c['name']);continue
        d=comps[c['name']]
        for key in ('name', 'loc', 'quat', 'scale', 'visible', 'materials', 'collision', 'mesh'):
            if key == 'mesh' and label in targets and not targets[label].get('custom_collision'): continue
            if key in ('mesh','materials') and sashes.get(label,{}).get('kind')=='remainder':continue
            if key == 'collision' and targets.get(label, {}).get('custom_collision'): continue
            if c[key] != d[key]: problems.append(label+' changed '+key)
    if label not in sashes or sashes[label]['kind']!='frame':
      if max(abs(a['bounds'][side][axis]-z['bounds'][side][axis]) for side in range(2) for axis in range(3)) > .01:
        problems.append(label+' changed overall bounds')
    elif extra:
        frame=comps['BreakawayWindowGrille'];body=comps['OriginalFixture']
        for key in ('loc','quat','scale'):
            if frame[key]!=body[key]:problems.append(label+' misplaced breakaway grille '+key)
result = dict(pass_check=not problems, original_actors=len(before), added_exact_collision_actors=len(expected_added), problems=problems)
(B/'saved-scope-check.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
if problems: raise SystemExit(1)
