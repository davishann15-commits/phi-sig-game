"""Predict pair separation using native arrival CSVs and a proposed3s water brake.
This is design data, not evidence of actual pond behavior or physical collision.
"""
import argparse,bisect,csv,json,math,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
folder=ROOT/'Saved/CalderWork/runs'/args.tag/'Saved/CalderWildlifeSkyPathsReview';report=folder/'proposed_water_braking_audit.json';assert not report.exists()
rows=list(csv.DictReader((folder/'goose_motion.csv').open(encoding='utf-8-sig')));groups={m:[] for m in range(8)}
for r in rows:groups[int(r['member'])].append((float(r['scene']),tuple(float(r[k]) for k in ('x','y','z'))))
assert all(len(v)>100 for v in groups.values())
def point(member,scene):
 g=groups[member];ts=[q[0] for q in g]
 if scene<ts[0]:return None
 if scene>=ts[-1]:
  a,b=g[-2:];delta=tuple(b[1][i]-a[1][i] for i in range(3));size=math.hypot(*delta);direction=tuple(v/size for v in delta);age=scene-ts[-1];t=min(3.,age);distance=250*t-(250-35)*t*t/6+35*max(0.,age-3)
  return (b[1][0]+direction[0]*distance,b[1][1]+direction[1]*distance,b[1][2])
 hi=bisect.bisect_left(ts,scene)
 if hi==0:return g[0][1]
 a,b=g[hi-1:hi+1];t=(scene-a[0])/(b[0]-a[0]);return tuple(a[1][i]+t*(b[1][i]-a[1][i]) for i in range(3))
start=min(v[0][0] for v in groups.values());end=max(v[-1][0] for v in groups.values())+3.;worst={'distance_cm':float('inf')};scene=start;checks=0
while scene<=end:
 points=[point(m,scene) for m in range(8)]
 for a in range(8):
  for b in range(a+1,8):
   if points[a] is None or points[b] is None:continue
   distance=math.dist(points[a],points[b]);checks+=1
   if distance<worst['distance_cm']:worst={'distance_cm':distance,'scene':scene,'members':[a,b],'points':[points[a],points[b]]}
 scene+=.05
result={'design_passed':worst['distance_cm']>=120.,'scope':'Design prediction from native arrivals plus proposed3s brake250to35cm/s; no live pond behavior acceptance','pair_checks':checks,'minimum_allowed_cm':120.,'worst':worst,'live_behavior_accepted':False,'whole_phase3_complete':False}
report.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k not in ('scope','worst')},separators=(',',':'))+' worst_cm='+str(round(worst['distance_cm'],4)))
