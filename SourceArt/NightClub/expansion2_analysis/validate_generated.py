"""Independent FBX checks on the proposed club-only expansion assets."""
import json,math,pathlib
from fbx_polys import mesh_data,read_fbx,child
B=pathlib.Path(__file__).resolve().parent.parent;O=B/'expansion2_analysis'
manifest=json.load(open(B/'expansion2_geometry/expansion2_manifest.json'))
meshes={};checks={};details={}
for asset in manifest['assets']:
    _,roots=read_fbx(asset['fbx']);settings=child(next(n for n in roots if n['name']=='GlobalSettings'),'Properties70')
    axes={p['props'][0]:p['props'][4:] for p in settings['children']}
    # These Blender exports use a 180-degree model rotation compensated by the
    # global front/coordinate axis conversion, plus a 100x meter-to-cm scale.
    # Assert that exporter contract before reading native centimeter geometry.
    data=mesh_data(asset['fbx'],{'scale':[100,100,100]})
    transforms=data['model_transforms'];axis_ok=axes['UpAxis']==[2] and axes['FrontAxisSign']==[1] and axes['CoordAxisSign']==[-1]
    transform_ok=all(t.get('Lcl Scaling')==[100,100,100] and max(abs(v-w)for v,w in zip(t.get('Lcl Rotation',[999]*3),[0,0,180]))<.001 for t in transforms)
    checks[asset['name']+'_coordinate_contract']=axis_ok and transform_ok
    mesh=next(m for m in data['meshes'] if not m['name'].startswith('UCX'))
    delta=max(abs(mesh['bounds'][s][i]-asset['bounds_cm'][s][i])for s in range(2)for i in range(3))
    triangles=sum(len(p)-2 for p in mesh['polygons'])
    checks[asset['name']+'_bounds_match']=delta<.02
    checks[asset['name']+'_triangle_count']=triangles==asset['triangles']
    meshes[asset['name']]=mesh;details[asset['name']]={'bounds':mesh['bounds'],'bound_error_cm':delta,'triangles':triangles,'models':transforms}

def sub(a,b):return [a[i]-b[i]for i in range(3)]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def dot(a,b):return sum(a[i]*b[i]for i in range(3))
def tris(mesh):
    return [[mesh['vertices'][p[0]],mesh['vertices'][p[i]],mesh['vertices'][p[i+1]]]for p in mesh['polygons']for i in range(1,len(p)-1)]
triangles={k:tris(v)for k,v in meshes.items()}
def ray(key,origin,direction,length):
    hit=None
    for a,b,c in triangles[key]:
        e1,e2=sub(b,a),sub(c,a);p=cross(direction,e2);det=dot(e1,p)
        if abs(det)<1e-10:continue
        inv=1/det;t=sub(origin,a);u=dot(t,p)*inv
        if u < -1e-7 or u>1+1e-7:continue
        q=cross(t,e1);v=dot(direction,q)*inv
        if v < -1e-7 or u+v>1+1e-7:continue
        dist=dot(e2,q)*inv
        if -1e-7<=dist<=length+1e-7 and (hit is None or dist<hit):hit=dist
    return hit

lo='SM_NCR2_LowerRoomShell';up='SM_NCR2_UpperExtensionCollision'
join_y=[-1508,-1490,-1450,-1400,-1350,-1300,-1250,-1200,-1150,-1100,-1050,-1031.5]
checks['lower_join_open_full_height']=all(ray(lo,[-1505,y,z],[1,0,0],40) is None for y in join_y for z in [2,80,170,260,325])
checks['upper_join_open_full_height']=all(ray(up,[-1505,y,z],[1,0,0],40) is None for y in join_y for z in [334,410,490,580,620])
# The 1.2 cm south/north lining is deliberately solid along the perimeter.
# Probe the actual usable opening, excluding only that cosmetic wall thickness.
liner_join_y=[-1506.02675]+[y for y in join_y if y>-1507.8 and y<-1031.2]
checks['upper_cosmetic_liner_join_open']=all(ray('SM_NCR2_UpperWallLining',[-1505,y,z],[1,0,0],40) is None for y in liner_join_y for z in [334,475,620])
checks['lower_notch_wall_preserved']=all(ray(lo,[-1520,y,150],[1,0,0],50) is not None for y in [-1020,-1000,-980])
checks['upper_notch_wall_preserved']=all(ray(up,[-1520,y,475],[1,0,0],50) is not None for y in [-1020,-1000,-980])
for key in ['SM_NCR2_LowerCeilingWithHole','SM_NCR2_UpperSupportFloorWithHole','SM_NCR2_UpperDarkFloorWithHole']:
    samples=[]
    for r in [0,35,65,95,99]:
        for i in range(24):samples.append((-1935+r*math.cos(i*math.pi/12),-1175+r*math.sin(i*math.pi/12)))
    checks[key+'_aperture_clear']=all(ray(key,[x,y,340],[0,0,-1],20) is None for x,y in samples)
    checks[key+'_aperture_ring_solid']=all(ray(key,[-1935+105*math.cos(i*math.pi/24),-1175+105*math.sin(i*math.pi/24),340],[0,0,-1],20) is not None for i in range(48))
    checks[key+'_seam_floor_continuous']=all(ray(key,[x,y,340],[0,0,-1],20) is not None for x in [-1490.1,-1490,-1489.9] for y in [-1480,-1400,-1300,-1200,-1100,-1050])
floor_xy=[(x,y)for x in range(-1480,-870,50)for y in range(-1490,-1040,50)]
checks['lower_wing_grid_floor_solid']=all(ray(lo,[x,y,100],[0,0,-1],110) is not None for x,y in floor_xy)
checks['upper_wing_grid_floor_solid']=all(ray('SM_NCR2_UpperSupportFloorWithHole',[x,y,400],[0,0,-1],80) is not None for x,y in floor_xy)
checks['lower_wing_grid_ceiling_solid']=all(ray('SM_NCR2_LowerCeilingWithHole',[x,y,250],[0,0,1],100) is not None for x,y in floor_xy)
checks['upper_wing_grid_ceiling_solid']=all(ray(up,[x,y,550],[0,0,1],100) is not None for x,y in floor_xy)
checks['preserved_notch_has_no_new_floor']=all(ray(key,[x,y,340],[0,0,-1],360) is None for key in [lo,'SM_NCR2_UpperSupportFloorWithHole'] for x in [-1400,-1200,-1000,-880] for y in [-1005,-990,-978])
checks['native_door_sweeps_clear']=all(r['disjoint']for r in json.load(open(O/'native-door-sweep-clearance.json')))
checks['protected_full_wing_envelope_clear']=not json.load(open(O/'envelope-volume-clearance.json'))['stepped_wing_full_shell']['intersections']

report={'status':'PASS' if all(checks.values()) else 'FAIL','checks':checks,'assets':details,'floor_grid_samples':len(floor_xy),'note':'Independent raw FBX geometry analysis; actual imported map runtime remains required.'}
(O/'generated-geometry-validation.json').write_text(json.dumps(report,indent=2))
print(json.dumps({'status':report['status'],'checks':checks,'floor_grid_samples':len(floor_xy)},indent=2))
assert all(checks.values()),[k for k,v in checks.items()if not v]
