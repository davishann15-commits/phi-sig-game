"""Continuous native hinge sweeps from current world component geometry."""
import json,math,pathlib
from fbx_polys import mesh_data
B=pathlib.Path(__file__).resolve().parent.parent;O=B/'expansion2_analysis'
rows=json.load(open(B/'expansion2_doors/actors.json'))
box=[[-1490,-1521.02675,-10],[-838,-1008,640.3]]
def swept_bounds(points,hinge,angle):
    angle=math.radians(angle);lo=min(0,angle);hi=max(0,angle);out=[]
    for p in points:
        x,y=p[0]-hinge[0],p[1]-hinge[1]
        angles=[lo,hi]
        for base in [math.atan2(-y,x),math.atan2(x,y)]:
            angles.extend(base+k*math.pi for k in range(-3,4) if lo<=base+k*math.pi<=hi)
        out.extend([[hinge[0]+x*math.cos(t)-y*math.sin(t),hinge[1]+x*math.sin(t)+y*math.cos(t),p[2]] for t in angles])
    return [[min(p[i] for p in out) for i in range(3)],[max(p[i] for p in out) for i in range(3)]]
def corner_world(c,p):
    p=[p[i]*c['scale'][i] for i in range(3)]
    pitch,yaw,roll=[math.radians(v)for v in c['rotation']]
    cp,sp=math.cos(pitch),math.sin(pitch);cy,sy=math.cos(yaw),math.sin(yaw);cr,sr=math.cos(roll),math.sin(roll);x,y,z=p;loc=c['location']
    return [loc[0]+cp*cy*x+(sr*sp*cy-cr*sy)*y+(-cr*sp*cy-sr*sy)*z,loc[1]+cp*sy*x+(sr*sp*sy+cr*cy)*y+(-cr*sp*sy+sr*cy)*z,loc[2]+sp*x-sr*cp*y+cr*cp*z]
report=[]
for row in rows:
    angle=(row['open_yaw']-row['closed_yaw']+180)%360-180
    for c in row['meshes']+row['boxes']:
        if c['component']=='Approach' or ('fbx' not in c and 'NO_COLLISION' in c['collision']):continue
        if 'fbx' in c:
            p=B/'expansion2_doors/fbx'/pathlib.Path(c['fbx']).name
            meshes=mesh_data(p,c)['meshes'];points=[v for m in meshes for v in m['vertices']]
        else:
            e=c['extent'];points=[corner_world(c,[sx*e[0],sy*e[1],sz*e[2]]) for sx in [-1,1] for sy in [-1,1]for sz in [-1,1]]
        bounds=swept_bounds(points,row['location'],angle)
        gaps=[max(box[0][i]-bounds[1][i],bounds[0][i]-box[1][i]) for i in range(3)]
        clear=max(gaps)>0
        report.append({'actor':row['label'],'component':c['component'],'collision':c['collision'],'hinge':row['location'],'angular_range_degrees':angle,'continuous_swept_bounds':bounds,'candidate_box':box,'separating_axis_clearance_cm':max(gaps),'disjoint':clear})
(O/'native-door-sweep-clearance.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
assert all(r['disjoint'] for r in report),'Native door swept geometry reaches candidate wing'
