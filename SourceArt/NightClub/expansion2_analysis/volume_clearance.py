"""Exact triangle-vs-box clearance for candidate extensions, not actor AABBs."""
import json,pathlib,sys
B=pathlib.Path(__file__).resolve().parent
prefix=sys.argv[1] if len(sys.argv)>1 else ''
meshes=json.load(open(B/(prefix+'polygons.json')))
def sub(a,b):return [a[i]-b[i] for i in range(3)]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def dot(a,b):return sum(a[i]*b[i] for i in range(3))
def intersects(tri,box):
    if any(max(p[i] for p in tri)<box[0][i]-1e-5 or min(p[i] for p in tri)>box[1][i]+1e-5 for i in range(3)):return False
    center=[(box[0][i]+box[1][i])/2 for i in range(3)];half=[(box[1][i]-box[0][i])/2 for i in range(3)]
    pts=[sub(p,center) for p in tri];edges=[sub(pts[(i+1)%3],pts[i]) for i in range(3)]
    bases=[[1,0,0],[0,1,0],[0,0,1]]
    axes=bases+[cross(edges[0],edges[1])]+[cross(e,b) for e in edges for b in bases]
    for axis in axes:
        if dot(axis,axis)<1e-12:continue
        projections=[dot(p,axis) for p in pts];radius=dot([abs(v) for v in axis],half)
        if min(projections)>radius+1e-5 or max(projections)<-radius-1e-5:return False
    return True
candidates={
 'stepped_wing_ground_interior':[[-1490,-1509.02675,.88],[-860,-1030,327.03]],
 'stepped_wing_upper_interior':[[-1490,-1509.02675,332.53],[-860,-1030,622.71]],
 'stepped_wing_full_shell':[[-1490,-1521.02675,-10],[-838,-1008,640.3]],
 'full_rectangle_bad_ne_corner':[[-1490,-1509.02675,.88],[-860,-973.5,327.03]],
 'south_candidate_shell':[[-2140,-1770,-10],[-860,-1509.03,640.3]],
}
report={}
for label,box in candidates.items():
    found=[]
    for mesh in meshes:
        if mesh['label'].startswith('NightClub'):continue
        if any(mesh['bounds'][1][i]<box[0][i]-1e-5 or mesh['bounds'][0][i]>box[1][i]+1e-5 for i in range(3)):continue
        hits=[]
        for pi,inds in enumerate(mesh['polygons']):
            if any(intersects([mesh['vertices'][inds[0]],mesh['vertices'][inds[i]],mesh['vertices'][inds[i+1]]],box) for i in range(1,len(inds)-1)):
                points=[mesh['vertices'][i] for i in inds]
                hits.append({'polygon':pi,'bounds':[[min(p[i] for p in points)for i in range(3)],[max(p[i] for p in points)for i in range(3)]]})
        if hits:found.append({'label':mesh['label'],'collision':mesh['collision'],'visible':mesh['visible'],'face_count':len(hits),'first_faces':hits[:12]})
    report[label]={'box':box,'intersections':found}
(B/(prefix+'volume-clearance.json')).write_text(json.dumps(report,indent=2))
print(json.dumps({k:{'box':v['box'],'intersections':[{p:q for p,q in x.items() if p!='first_faces'} for x in v['intersections']]} for k,v in report.items()},indent=2))
