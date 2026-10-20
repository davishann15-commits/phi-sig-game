import json,pathlib,sys
B=pathlib.Path(__file__).resolve().parent
prefix=sys.argv[1] if len(sys.argv)>1 else ''
meshes=json.load(open(B/(prefix+'polygons.json')))
def vertical_hit(p, a,b,c):
    x,y=p;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
    if abs(den)<1e-8:return None
    u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den
    v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den;w=1-u-v
    if min(u,v,w)<-1e-7:return None
    return u*a[2]+v*b[2]+w*c[2]
points=[(x,y) for x in [-1600,-1500,-1400,-1300,-1200,-1100,-1000,-900,-850] for y in [-1600,-1500,-1400,-1300,-1200,-1100,-1020,-990]]
report={}
for p in points:
    hits=[]
    for m in meshes:
        if m['label'].startswith('NightClub'):continue
        if not all(m['bounds'][0][i]-1e-4<=p[i]<=m['bounds'][1][i]+1e-4 for i in range(2)):continue
        mh=[]
        for inds in m['polygons']:
            a=m['vertices'][inds[0]]
            for i in range(1,len(inds)-1):
                z=vertical_hit(p,a,m['vertices'][inds[i]],m['vertices'][inds[i+1]])
                if z is not None and not any(abs(z-q)<.01 for q in mh):mh.append(z)
        if mh:hits.append({'label':m['label'],'z':sorted(mh)})
    report[str(p)]=hits
(B/(prefix+'vertical-rays.json')).write_text(json.dumps(report,indent=2))
for x in [-1600,-1500,-1400,-1300,-1200,-1100,-1000,-900,-850]:
    print('X',x)
    for y in [-1600,-1500,-1400,-1300,-1200,-1100,-1020,-990]:
        hits=report[str((x,y))]
        print(y,[(r['label'],[round(z,2) for z in r['z']]) for r in hits])
