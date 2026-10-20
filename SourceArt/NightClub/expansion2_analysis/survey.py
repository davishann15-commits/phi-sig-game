"""Polygon-based horizontal sections of the fresh saved-map FBX exports.

Writes only expansion2_analysis. No editor, build, or game operations.
"""
import json, math, pathlib, sys
from fbx_polys import mesh_data
B=pathlib.Path(__file__).resolve().parent.parent
O=B/'expansion2_analysis';O.mkdir(exist_ok=True)
context=B/(sys.argv[1] if len(sys.argv)>1 else 'expansion2_context')
prefix=sys.argv[2] if len(sys.argv)>2 else ''
rows=json.load(open(context/'actors.json'))
geometries=[];validation=[]
for row in rows:
    for comp in row['meshes']:
        if not comp.get('fbx'):continue
        result=mesh_data(comp['fbx'],row)
        for mesh in result['meshes']:
            if mesh['name'].startswith('UCX'):continue
            mesh.update(label=row['label'],component=comp['component'],asset=comp['asset'],collision=comp['collision'],visible=comp['visible'])
            geometries.append(mesh)
            validation.append(dict(label=row['label'],bounds=mesh['bounds'],actor_bounds=row['bounds'],maximum_bound_difference=max(abs(mesh['bounds'][side][i]-row['bounds'][side][i]) for side in range(2) for i in range(3)),transforms=result['model_transforms']))

def section(mesh,z):
    lines=[]
    for inds in mesh['polygons']:
        points=[mesh['vertices'][i] for i in inds]
        if min(p[2] for p in points)>z+1e-5 or max(p[2] for p in points)<z-1e-5:continue
        hits=[]
        for i,a in enumerate(points):
            b=points[(i+1)%len(points)]
            if (a[2]<z and b[2]>z) or (b[2]<z and a[2]>z):
                t=(z-a[2])/(b[2]-a[2]);hits.append([a[j]+t*(b[j]-a[j]) for j in range(2)])
            elif abs(a[2]-z)<1e-5:hits.append(a[:2])
        unique=[]
        for p in hits:
            if not any(sum((p[j]-q[j])**2 for j in range(2))<1e-8 for q in unique):unique.append(p)
        if len(unique)>=2:
            a,b=max(((a,b) for i,a in enumerate(unique) for b in unique[i+1:]),key=lambda pair:sum((pair[0][j]-pair[1][j])**2 for j in range(2)))
            if max(a[0],b[0]) < -2300 or min(a[0],b[0])>-650 or max(a[1],b[1])<-2200 or min(a[1],b[1])>-850:continue
            lines.append([a,b])
    return lines

report={'fresh_context':str(context/'actors.json'),'validation':validation,'sections':{}}
palette=['#7f1d1d','#1d4ed8','#a16207','#15803d','#7e22ce','#0e7490','#be185d']
for name,z in [('ground',120),('upper',460),('ground_ceiling',320),('upper_ceiling',630)]:
    groups=[];svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1650" height="1350" viewBox="-2300 -2200 1650 1350">','<rect x="-2300" y="-2200" width="1650" height="1350" fill="#fff"/>']
    svg+=['<rect x="-2212.51" y="-1509.03" width="722.51" height="535.53" fill="#fde68a" fill-opacity=".35" stroke="black" stroke-width="3"/>']
    for n,mesh in enumerate(geometries):
        lines=section(mesh,z)
        if not lines:continue
        groups.append({'label':mesh['label'],'collision':mesh['collision'],'visible':mesh['visible'],'lines':lines})
        color=palette[n%len(palette)]
        opacity='.85' if mesh['visible'] else '.45'
        for a,b in lines:svg.append(f'<path d="M {a[0]} {a[1]} L {b[0]} {b[1]}" stroke="{color}" stroke-width="1.5" opacity="{opacity}"><title>{mesh["label"]}</title></path>')
    for x in range(-2200,-600,100):svg.append(f'<path d="M{x} -2200V-850" stroke="gray" opacity=".1"/><text x="{x}" y="-870" font-size="14">{x}</text>')
    for y in range(-2200,-800,100):svg.append(f'<path d="M-2300 {y}H-650" stroke="gray" opacity=".1"/><text x="-2280" y="{y}" font-size="14">{y}</text>')
    svg.append('</svg>');(O/(prefix+name+'.svg')).write_text('\n'.join(svg))
    report['sections'][name]={'z':z,'groups':groups}
(O/(prefix+'survey.json')).write_text(json.dumps(report,indent=2))
(O/(prefix+'polygons.json')).write_text(json.dumps(geometries))
print(json.dumps({'meshes':len(geometries),'actor_bound_discrepancies':[{k:v for k,v in r.items() if k!='transforms'} for r in validation if r['maximum_bound_difference']>.1],'section_counts':{k:sum(len(g['lines']) for g in v['groups']) for k,v in report['sections'].items()}},indent=2))
