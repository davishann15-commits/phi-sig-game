import bpy, json, math, pathlib
from mathutils import Vector
from mathutils.bvhtree import BVHTree

B = pathlib.Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/NightClub')
O = B / 'expansion2_geometry'
(O / 'models').mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
X0, X1, X2 = -2212.51019, -1490., -860.
Y0, Y1, Y2 = -1509.02675, -973.5, -1030.
rows = {r['label']:r for r in json.load(open(B / 'expansion2_context/actors.json'))}
material_info = json.load(open(B / 'expansion_geometry/expansion_manifest.json'))['materials']
manifest = dict(assets=[], materials={}, coordinates='World-baked UE centimeters; actor location/rotation zero, scale one.',
    footprint=dict(base_cm=[[X0,Y0],[X1,Y1]],east_wing_cm=[[X1,Y0],[X2,Y2]],
                   original_area_m2=(X1-X0)*(Y1-Y0)/10000, added_area_m2=(X2-X1)*(Y2-Y0)/10000),
    heights_cm=dict(lower_floor=.88, lower_ceiling=327.03, upper_collision_floor=332.53,
                    upper_finish_floor=333.10, upper_ceiling=622.71),
    pole_aperture=dict(center_cm=[-1935,-1175],inradius_cm=100,sides=48,unchanged=True),
    notes=['Only club-specific geometry copied/extended. No original HallEnd or house asset changed.',
           'Do not generate a single convex hull. Collision=true assets require UseComplexAsSimple.',
           'Old east wall removed over full Y[-1509.02675,-1030] opening on both floors.',
           'Doors, door trim, pole rim/rail and other house actors remain separate and untouched.'])
for n,r in material_info.items():
    manifest['materials'][n] = dict(r, reuse_asset='/Game/SeniorSendOff/House/NightClub/'+n)

def mat(name):
    m=bpy.data.materials.get(name)
    r=material_info[name]
    if not m:m=bpy.data.materials.new(name)
    m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*r['color'],1)
    p.inputs['Roughness'].default_value=r.get('roughness',.5)
    p.inputs['Metallic'].default_value=r.get('metallic',0)
    if r.get('emissive'):
        p.inputs['Emission Color'].default_value=(*r['emissive'],1)
        p.inputs['Emission Strength'].default_value=r.get('emissive_strength',1)
    m.diffuse_color=(*r['color'],1);return m

def clip(poly,n,d,inside=True):
    out=[]
    for a,b in zip(poly,poly[1:]+poly[:1]):
        da=sum(n[i]*a[i] for i in range(3))-d;db=sum(n[i]*b[i] for i in range(3))-d
        ia=da<=1e-7 if inside else da>=-1e-7;ib=db<=1e-7 if inside else db>=-1e-7
        if ia:out.append(a)
        if ia!=ib:
            t=da/(da-db);out.append(tuple(a[i]+t*(b[i]-a[i]) for i in range(len(a))))
    return out

def subtract(poly,planes):
    pieces=[];remaining=poly
    for n,d in planes:
        if len(remaining)<3:break
        outside=clip(remaining,n,d,False)
        if len(outside)>=3:pieces.append(outside)
        remaining=clip(remaining,n,d,True)
    return pieces

class Geometry:
    def __init__(self):self.polys=[];self.names=[];self.stats={}
    def face(self,poly,m,smooth=False):
        if len(poly)<3:return
        if m not in self.names:self.names.append(m)
        self.polys.append((poly,m,smooth))
    def source(self,label,remove_boxes=(),open_seams=()):
        r=rows[label];src=B/'expansion2_context/fbx'/pathlib.Path(r['meshes'][0]['fbx']).name
        before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(src))
        objects=[o for o in bpy.data.objects if o not in before and o.type=='MESH' and not o.name.startswith('UCX')]
        count=removed=split=0
        for o in objects:
            data=o.data;uv=data.uv_layers.active
            for f in data.polygons:
                poly=[]
                for li in f.loop_indices:
                    p=o.matrix_world@data.vertices[data.loops[li].vertex_index].co
                    q=uv.data[li].uv if uv else (0.,0.)
                    poly.append((p.x*100,-p.y*100,p.z*100,q[0],q[1]))
                count+=1
                if any(all(all(lo[i]-.03<=p[i]<=hi[i]+.03 for i in range(3)) for p in poly) for lo,hi in remove_boxes):
                    removed+=1;continue
                pieces=[poly]
                for axis,position,other_axis,lo,hi in open_seams:
                    if all(abs(p[axis]-position)<.03 for p in poly):
                        n1=[0,0,0];n1[other_axis]=1
                        n2=[0,0,0];n2[other_axis]=-1
                        pieces=[q for p in pieces for q in subtract(p,[(tuple(n1),hi),(tuple(n2),-lo)])]
                        split+=1
                name=str(data.materials[f.material_index].name).split('.')[0]
                if name not in material_info:raise ValueError(name)
                for p in pieces:self.face(p,name,f.use_smooth)
        for o in bpy.data.objects:
            if o not in before:bpy.data.objects.remove(o,do_unlink=True)
        self.stats=dict(source_actor=label,source_fbx=str(src),source_faces=count,removed_faces=removed,seam_faces_split=split)
    def box(self,lo,hi,m,omit=()):
        x,y,z=lo;X,Y,Z=hi
        pts=[(x,y,z),(X,y,z),(X,Y,z),(x,Y,z),(x,y,Z),(X,y,Z),(X,Y,Z),(x,Y,Z)]
        fs={'bottom':[0,3,2,1],'top':[4,5,6,7],'west':[0,4,7,3],'east':[1,2,6,5],'south':[0,1,5,4],'north':[3,7,6,2]}
        for key,ids in fs.items():
            if key in omit:continue
            axis=2 if key in ['bottom','top'] else 0 if key in ['west','east'] else 1
            u,v=([0,1] if axis==2 else [1,2] if axis==0 else [0,2])
            # Reversal compensates for UE-to-Blender Y reflection. UV repeats every meter.
            self.face([(*pts[i],pts[i][u]/100,pts[i][v]/100) for i in reversed(ids)],m)
    def make(self,name):
        vv=[];ff=[];slots=[];uvs=[];smooth=[]
        for poly,m,s in self.polys:
            ff.append(list(range(len(vv),len(vv)+len(poly))));slots.append(self.names.index(m));smooth.append(s)
            vv.extend([(p[0]/100,-p[1]/100,p[2]/100) for p in poly]);uvs.extend([p[3:5] for p in poly])
        mesh=bpy.data.meshes.new(name);mesh.from_pydata(vv,[],ff);mesh.update()
        for n in self.names:mesh.materials.append(mat(n))
        uv=mesh.uv_layers.new(name='UVMap')
        for f,slot,s in zip(mesh.polygons,slots,smooth):
            f.material_index=slot;f.use_smooth=s
            for li in f.loop_indices:uv.data[li].uv=uvs[mesh.loops[li].vertex_index]
        o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o);return o

assets={}
def export(g,name,label,collision):
    o=g.make(name);bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    path=O/'models'/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,global_scale=1,apply_unit_scale=True,
        axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',
        use_mesh_modifiers=True,add_leaf_bones=False)
    o.data.calc_loop_triangles();p=[(v.co.x*100,-v.co.y*100,v.co.z*100) for v in o.data.vertices]
    entry=dict(name=name,fbx=str(path),target_actor=label,collision=collision,
        collision_mode='UseComplexAsSimple' if collision else 'NoCollision',materials=g.names,
        origin_cm=[0,0,0],bounds_cm=[[min(v[i] for v in p) for i in range(3)],[max(v[i] for v in p) for i in range(3)]],
        triangles=len(o.data.loop_triangles),source_preservation=g.stats)
    manifest['assets'].append(entry);assets[name]=o
    print('EXPANSION2_ASSET',name,entry['triangles'],entry['bounds_cm'])
    return o

# Copy the current lower shell, removing only the old east wall, and join the old floor side.
g=Geometry();g.source('NightClub_SM_NC_LowerRoomShell',
    remove_boxes=[([X1,Y0-12,.88],[X1+12,Y1+12,332.53])],
    open_seams=[(0,X1+12,1,Y0-12,Y2+22)])
g.box([X1+12,Y0-12,-10],[X2+22,Y2+22,.88],'M_NC_FloorBlack',omit=['west'])
g.box([X1,Y2,.88],[X1+12,Y1+12,332.53],'M_NC_Charcoal')
g.box([X1,Y0-12,.88],[X2,Y0,332.53],'M_NC_Charcoal')
g.box([X2,Y0-12,.88],[X2+22,Y2+22,332.53],'M_NC_Charcoal')
g.box([X1,Y2,.88],[X2,Y2+22,332.53],'M_NC_Charcoal')
lower=export(g,'SM_NCR2_LowerRoomShell','NightClub_SM_NC_LowerRoomShell',True)

# Preserve all original slab aperture faces and UVs. Remove only the newly internal east side.
for old,new,zlo,zhi,m,col in [
    ('NightClub_SM_NC_LowerCeilingWithHole','SM_NCR2_LowerCeilingWithHole',327.03,332.53,'M_NC_CeilingBlack',True),
    ('NightClub_SM_NC_UpperDarkFloorWithHole','SM_NCR2_UpperDarkFloorWithHole',332.60,333.10,'M_NC_FloorBlack',False),
    ('NightClub_SM_NCR_UpperSupportFloorWithHole','SM_NCR2_UpperSupportFloorWithHole',327.10,332.53,'M_NC_FloorBlack',True)]:
    g=Geometry();g.source(old,open_seams=[(0,X1,1,Y0,Y2)])
    g.box([X1,Y0,zlo],[X2,Y2,zhi],m,omit=['west'])
    export(g,new,old,col)

# This collision shell extends only the previously claimed eastern part and the new wing.
g=Geometry();g.source('NightClub_SM_NCR_UpperExtensionCollision',
    remove_boxes=[([X1,Y0-12,332.53],[X1+22,Y1+12,640.3])])
g.box([X1,Y2,332.53],[X1+22,Y1+12,640.3],'M_NC_Charcoal')
g.box([X1,Y0-12,332.53],[X2,Y0,640.3],'M_NC_Charcoal')
g.box([X2,Y0-12,332.53],[X2+22,Y2+22,640.3],'M_NC_Charcoal')
g.box([X1,Y2,332.53],[X2,Y2+22,640.3],'M_NC_Charcoal')
g.box([X1,Y0,622.71],[X2,Y2,640.3],'M_NC_CeilingBlack',omit=['west'])
uppercoll=export(g,'SM_NCR2_UpperExtensionCollision','NightClub_SM_NCR_UpperExtensionCollision',True)

g=Geometry();g.source('NightClub_SM_NC_UpperWallLining',
    remove_boxes=[([X1-1.2,Y0,333.10],[X1,Y1,622.71])])
g.box([X1-1.2,Y2,333.10],[X1,Y1,622.71],'M_NC_Charcoal')
g.box([X1,Y0,333.10],[X2,Y0+1.2,622.71],'M_NC_Charcoal')
g.box([X2-1.2,Y0,333.10],[X2,Y2,622.71],'M_NC_Charcoal')
g.box([X1,Y2-1.2,333.10],[X2,Y2,622.71],'M_NC_Charcoal')
g.box([X1,Y0,621.6],[X2,Y2,622.7],'M_NC_CeilingBlack',omit=['west'])
export(g,'SM_NCR2_UpperWallLining','NightClub_SM_NC_UpperWallLining',False)

# Continuous original meter-like grid spacing; no interior border at the removed wall.
g=Geometry();px=(X1-X0-56)/8;py=(Y1-Y0-56)/6
regions=[(X0+26,Y0+26,X2-26,Y2-26),(X0+26,Y2-26,X1-26,Y1-26)]
def clipped_box(lo,hi,m):
    for x,y,X,Y in regions:
        a=[max(lo[0],x),max(lo[1],y),lo[2]];b=[min(hi[0],X),min(hi[1],Y),hi[2]]
        if b[0]>a[0]+.001 and b[1]>a[1]+.001:g.box(a,b,m)
for i in range(math.ceil((X2-X0-52)/px)):
    for j in range(6):
        x=X0+28+i*px;y=Y0+28+j*py
        clipped_box([x,y,.90],[x+px-3,y+py-3,1.04],'M_NC_FloorBlack' if (i+j)%2 else 'M_NC_FloorSlate')
for i in range(math.ceil((X2-X0-52)/px)+1):
    x=X0+26+i*px;clipped_box([x,Y0+26,1.05],[x+1,Y1-26,1.12],'M_NC_FloorGlow')
for j in range(7):
    y=Y0+26+j*py;clipped_box([X0+26,y,1.05],[X2-26,y+1,1.12],'M_NC_FloorGlow')
# Discreet brushed-metal skirting is part of the no-collision floor finish; do not span doorway.
for z in [.88,333.10]:
    h=6.;t=.9
    for lo,hi in [([X0,Y0,z],[X2,Y0+t,z+h]),([X2-t,Y0,z],[X2,Y2,z+h]),
                  ([X1,Y2-t,z],[X2,Y2,z+h]),([X1-t,Y2,z],[X1,Y1,z+h]),
                  ([X0,Y1-t,z],[X1,Y1,z+h]),([X0,Y0,z],[X0+t,-1467,z+h]),
                  ([X0,-1288,z],[X0+t,Y1,z+h])]:
        g.box(lo,hi,'M_NC_Trim')
export(g,'SM_NCR2_LowerDanceFloorInlay','NightClub_SM_NC_LowerDanceFloorInlay',False)

# Geometry acceptance probes use the exported meshes' exact source vertices.
def tree(o):return BVHTree.FromPolygons([v.co for v in o.data.vertices],[list(f.vertices) for f in o.data.polygons],all_triangles=False)
def ray(t,p,d,cm):
    p=Vector((p[0]/100,-p[1]/100,p[2]/100));d=Vector((d[0],-d[1],d[2]));return t.ray_cast(p,d,cm/100)[0]
checks={};tl=tree(lower);tu=tree(uppercoll)
checks['lower_join_clear']=all(ray(tl,[-1550,y,160],[1,0,0],150) is None for y in [Y0+3,-1400,-1200,Y2-3])
checks['upper_join_clear']=all(ray(tu,[-1550,y,475],[1,0,0],150) is None for y in [Y0+3,-1400,-1200,Y2-3])
checks['notch_wall_preserved_lower']=ray(tl,[-1550,-995,160],[1,0,0],100) is not None
checks['notch_wall_preserved_upper']=ray(tu,[-1550,-995,475],[1,0,0],100) is not None
checks['lower_wing_floor_present']=ray(tl,[-1100,-1220,100],[0,0,-1],150) is not None
for key in ['SM_NCR2_LowerCeilingWithHole','SM_NCR2_UpperSupportFloorWithHole','SM_NCR2_UpperDarkFloorWithHole']:
    t=tree(assets[key]);checks[key+'_hole_clear']=all(ray(t,[-1935+dx,-1175+dy,340],[0,0,-1],20) is None for dx,dy in [(0,0),(90,0),(-90,0),(0,90),(0,-90)])
    checks[key+'_wing_continuous']=all(ray(t,[x,-1200,340],[0,0,-1],20) is not None for x in [-1491,-1489,-1100,-862])
checks['lower_old_door_clear']=ray(tl,[-2290,-1380,120],[1,0,0],120) is None
checks['all_origin_identity']=all(tuple(o.location)==(0.,0.,0.) and tuple(o.scale)==(1.,1.,1.) for o in assets.values())
assert all(checks.values()),checks
manifest['geometry_checks']=checks
(O/'expansion2_manifest.json').write_text(json.dumps(manifest,indent=2))
(O/'geometry_validation.json').write_text(json.dumps(checks,indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(O/'NightClubExpansion2_source.blend'))
print('EXPANSION2_GEOMETRY_PASS',json.dumps(checks))

# Cutaway render is offline art inspection only; it is not an in-game screenshot.
for o in bpy.context.scene.objects:o.hide_render=True
scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('PreviewWorld');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.055,.065,.085,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.6
def preview_copy(o,shift):
    q=o.copy();q.data=o.data.copy();bpy.context.collection.objects.link(q);q.hide_render=False;q.location=Vector(shift);return q
# Place lower and upper plans side by side, roofs hidden to show the shaped volume and aperture.
for n in ['SM_NCR2_LowerRoomShell','SM_NCR2_LowerDanceFloorInlay']:
    q=preview_copy(assets[n],(0,0,0))
    if n.endswith('LowerDanceFloorInlay'):
        # Upper skirting is omitted from this downstairs-only cutaway copy.
        vs=[v for v in q.data.vertices if v.co.z<1.]
        import bmesh
        bm=bmesh.new();bm.from_mesh(q.data);bmesh.ops.delete(bm,geom=[v for v in bm.verts if v.co.z>1.],context='VERTS');bm.to_mesh(q.data);bm.free()
for n in ['SM_NCR2_UpperDarkFloorWithHole','SM_NCR2_UpperWallLining','SM_NCR2_UpperExtensionCollision']:
    q=preview_copy(assets[n],(0,-7.9,-3.331))
    import bmesh
    bm=bmesh.new();bm.from_mesh(q.data)
    # Remove ceiling faces and show all room walls only to 1.1 m in cutaway.
    bmesh.ops.delete(bm,geom=[f for f in bm.faces if all(v.co.z>6.20 for v in f.verts)],context='FACES')
    for v in bm.verts:
        if v.co.z>4.43:v.co.z=4.43
    bm.to_mesh(q.data);bm.free()
# Downstairs walls cut down to see the grid; geometry in FBX remains full-height.
q=next(o for o in bpy.context.scene.objects if o.name.startswith('SM_NCR2_LowerRoomShell.') and not o.hide_render)
for v in q.data.vertices:
    if v.co.z>1.1:v.co.z=1.1
def area(name,pos,power,size,color):
    d=bpy.data.lights.new(name,'AREA');d.energy=power;d.shape='DISK';d.size=size;d.color=color
    o=bpy.data.objects.new(name,d);bpy.context.collection.objects.link(o);o.location=pos
    o.rotation_euler=(Vector((-15.5,8,0))-o.location).to_track_quat('-Z','Y').to_euler()
area('PreviewSoftbox',(-17,7,15),5000,12,(.70,.80,1))
area('PreviewFill',(-8,15,8),3000,10,(1,.72,.48))
d=bpy.data.cameras.new('PreviewCamera');cam=bpy.data.objects.new('PreviewCamera',d);bpy.context.collection.objects.link(cam)
cam.location=(-8,-6,16);cam.rotation_euler=(Vector((-15.6,8.4,0))-cam.location).to_track_quat('-Z','Y').to_euler();d.type='ORTHO';d.ortho_scale=23;scene.camera=cam
scene.render.filepath=str(O/'expansion2-cutaway.png');scene.view_settings.view_transform='AgX';bpy.ops.render.render(write_still=True)
print('EXPANSION2_PREVIEW_DONE')
