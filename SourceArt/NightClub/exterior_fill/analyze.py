import pathlib,json,sys
B=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(B/'expansion2_analysis'))
from fbx_polys import mesh_data
exec((B/'expansion2_analysis/volume_clearance.py').read_text().split('candidates={')[0].split('meshes=json.load(open(B/(prefix+\'polygons.json\')))')[1],globals())
W=B/'exterior_fill';rows=json.loads((W/'context/actors.json').read_text());meshes=[]
for r in rows:
 for m in r['meshes']:
  if m.get('fbx'):
   for g in mesh_data(m['fbx'],r)['meshes']:meshes.append(dict(g,label=r['label'],collision=m['collision'],visible=m['visible']))
(W/'context/polygons.json').write_text(json.dumps(meshes))
candidates={
 'main_south_seam':[[-2212.4,-1522,4],[-1432,-1509.03,286]],
 'main_east_upper_corner':[[-1490,-1030,4],[-1432,-973.5,326]],
 'west_lounge_gap':[[-3380,-2180,4],[-2750,-1522,286]],
 'south_club_gap':[[-2131,-2180,4],[-1440,-1522,286]],
 'east_main_gap':[[-1489,-1508,4],[-1440,-1030,326]],
 'south_mid_gap':[[-2750,-2180,4],[-2131,-2146,286]],
}
report={}
for label,box in candidates.items():
 hits=[]
 for m in meshes:
  if m['label'].startswith('NightClub') or m['label'] in ['SM_Room102_Shell','COL_Room102_Shell']:continue
  fs=[]
  for pi,inds in enumerate(m['polygons']):
   if any(intersects([m['vertices'][inds[0]],m['vertices'][inds[i]],m['vertices'][inds[i+1]]],box) for i in range(1,len(inds)-1)):
    pts=[m['vertices'][i]for i in inds];fs.append(dict(index=pi,bounds=[[min(p[j]for p in pts)for j in range(3)],[max(p[j]for p in pts)for j in range(3)]]))
  if fs:hits.append(dict(label=m['label'],faces=fs))
 report[label]=dict(box=box,hits=hits)
(W/'context/clearance.json').write_text(json.dumps(report,indent=2))
print(json.dumps({k:{'box':v['box'],'hits':[{**h,'faces':h['faces'][:12]}for h in v['hits']]}for k,v in report.items()},indent=2))
