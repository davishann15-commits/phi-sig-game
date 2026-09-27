"""Fit the Fixer's reference sneakers to the native assembled body in Blender.

Reads the existing C03 source without altering it. All output is in /private/tmp.
The two primary static exports use native body space and can attach to foot bones
with KeepWorldTransform after matching the Body component transform.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

SOURCE = '/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/Cobble/C03/C03.blend'
BODY = '/private/tmp/FixerNativeBody.fbx'
OUT = Path('/private/tmp/fixer_details')
OUT.mkdir(parents=True, exist_ok=True)

def material(name, color, roughness=.72):
    m=bpy.data.materials.new(name)
    m.diffuse_color=(*color,1)
    m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Roughness'].default_value=roughness
    return m

def path_mesh(name, points, radius, mat):
    data=bpy.data.curves.new(name,'CURVE')
    data.dimensions='3D'
    data.resolution_u=2
    data.bevel_depth=radius
    data.bevel_resolution=2
    spl=data.splines.new('POLY')
    spl.points.add(len(points)-1)
    for p,co in zip(spl.points,points): p.co=(*co,1)
    obj=bpy.data.objects.new(name,data)
    bpy.context.collection.objects.link(obj)
    data.materials.append(mat)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.convert(target='MESH')
    return obj

def sole_ring(name,cx,mat):
    # Athletic foam sole with a rounded heel, wider toe and slightly lifted toe.
    verts=[]
    n=64
    levels=[(-.011, .98),(-.004,1.03),(.016,1.025),(.027,.96)]
    for z,sz in levels:
        for i in range(n):
            a=2*math.pi*i/n
            y=-.086+.139*math.cos(a)
            width=.058*(.89+.11*math.sin(a)**2)
            x=cx+width*math.sin(a)*sz
            lift=.013*max(0,(-y-.155)/.072)**2
            verts.append((x,-.086+(y+.086)*sz,z+lift))
    faces=[]
    for j in range(len(levels)-1):
        for i in range(n): faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
    faces.append(tuple(reversed(range(n))))
    faces.append(tuple((len(levels)-1)*n+i for i in range(n)))
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata(verts,[],faces)
    mesh.materials.append(mat)
    obj=bpy.data.objects.new(name,mesh)
    bpy.context.collection.objects.link(obj)
    for p in mesh.polygons:p.use_smooth=True
    return obj

assert Path(BODY).is_file(), 'Waiting on FixerNativeBody.fbx'
bpy.ops.wm.open_mainfile(filepath=SOURCE)
old_rig=bpy.data.objects['C03_Rig']
source=bpy.data.objects['C03_Body.shoes05']
source_matrices={s:old_rig.matrix_world@old_rig.data.bones['foot_'+s.lower()].matrix_local for s in ('L','R')}
source_ankles={s:old_rig.matrix_world@old_rig.data.bones['foot_'+s.lower()].head_local for s in ('L','R')}
source_balls={s:old_rig.matrix_world@old_rig.data.bones['ball_'+s.lower()].head_local for s in ('L','R')}

prior=set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=BODY,use_anim=False)
added=[o for o in bpy.data.objects if o not in prior]
rig=next(o for o in added if o.type=='ARMATURE')
body=next(o for o in added if o.type=='MESH' and len(o.vertex_groups)>100)
target_ankles={s:rig.matrix_world@rig.data.bones['foot_'+s.lower()].head_local for s in ('L','R')}
target_balls={s:rig.matrix_world@rig.data.bones['ball_'+s.lower()].head_local for s in ('L','R')}

upper=source.data.materials[0].copy()
upper.name='M_FixerSneakerUpper'
upper.use_nodes=True
nodes=upper.node_tree.nodes
links=upper.node_tree.links
p=nodes.get('Principled BSDF')
p.inputs['Roughness'].default_value=.78
tex=next((n for n in nodes if n.type=='TEX_IMAGE' and n.image),None)
if tex:
    # Preserve woven/stitched detail but make all original green accents gray.
    bw=nodes.new('ShaderNodeRGBToBW')
    tint=nodes.new('ShaderNodeMixRGB')
    tint.blend_type='MULTIPLY'
    tint.inputs[0].default_value=1
    tint.inputs[2].default_value=(.66,.66,.64,1)
    links.new(tex.outputs['Color'],bw.inputs[0])
    links.new(bw.outputs[0],tint.inputs[1])
    links.new(tint.outputs[0],p.inputs['Base Color'])
    tex.image.filepath=bpy.path.abspath(tex.image.filepath)
foam=material('M_FixerShoeFoam',(.67,.67,.63),.88)
lace=material('M_FixerShoeLace',(.62,.62,.59),.92)
rubber=material('M_FixerShoeRubber',(.14,.145,.14),.94)
sockmat=material('M_FixerAnkleSock',(.69,.68,.64),.94)
objects=[]
report={'source':SOURCE,'target':BODY,'exports':{},'texture':tex.image.filepath if tex else None}

for side,sgn in [('L',1),('R',-1)]:
    data=source.data.copy()
    shoe=bpy.data.objects.new('FixerSneaker_'+side,data)
    bpy.context.collection.objects.link(shoe)
    bm=bmesh.new();bm.from_mesh(data)
    bmesh.ops.delete(bm,geom=[v for v in bm.verts if v.co.x*sgn<0 or v.co.z>.116],context='VERTS')
    bm.to_mesh(data);bm.free()
    data.materials.clear();data.materials.append(upper)
    for p in data.polygons:p.use_smooth=True
    parts=[shoe]
    # Source shoe already includes the sock, tongue, stitched upper and collar.
    cx=source_ankles[side].x
    bvh=BVHTree.FromPolygons([v.co.copy() for v in data.vertices],[list(p.vertices) for p in data.polygons])
    def top(x,y):
        loc,_,_,_=bvh.ray_cast(Vector((x,y,.32)),Vector((0,0,-1)),.4)
        return loc.z+.002 if loc else .06
    parts.append(sole_ring('FixerFoam_'+side,cx,foam))
    for j,y in enumerate([-.132,-.112,-.092,-.072,-.052]):
        half=.026-j*.001
        points=[]
        for k in range(9):
            f=k/8
            x=cx-half+2*half*f
            yy=y+.004*math.sin(f*math.pi)
            points.append((x,yy,top(x,yy)+.0025))
        parts.append(path_mesh('FixerLace_%s_%s'%(side,j),points,.0014,lace))
    # Fit the ankle and retain the natural shoe direction in native body space.
    target=target_ankles[side]
    old=source_ankles[side]
    # A slight reduction suits his slimmer build while keeping realistic feet.
    fit=Vector((1.04,1.02,.90))
    sv=source_balls[side]-old;tv=target_balls[side]-target
    yaw=math.atan2(tv.y,tv.x)-math.atan2(sv.y,sv.x)
    rot=Matrix.Rotation(yaw,3,'Z')
    for part in parts:
        for v in part.data.vertices:
            world=part.matrix_world@v.co
            d=world-old
            if d.y<-.065:
                a=min(1,(-d.y-.065)/.105)
                d.y-=.014*a*a*(3-2*a)
            v.co=target+rot@Vector((d.x*fit.x,d.y*fit.y,d.z*fit.z))+Vector((0,0,-.006))
            if v.co.z>.105:v.co.z=.105+(v.co.z-.105)*.23
        part.matrix_world=Matrix.Identity(4)
    # Smooth closed ankle cuff replaces the legacy tall sock's jagged rim.
    sv=[];sf=[];n=64
    rings=[(.083,.0305,.0345),(.097,.031,.034),(.114,.030,.0325),
           (.118,.0305,.033),(.120,.0295,.032),(.118,.028,.0305),(.114,.028,.0305)]
    for z,rx,ry in rings:
        for i in range(n):
            a=2*math.pi*i/n
            rib=.00028*math.cos(a*32)
            sv.append((target.x+(rx+rib)*math.cos(a),target.y+(ry+rib)*math.sin(a),z))
    for j in range(len(rings)-1):
        for i in range(n):sf.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
    sm=bpy.data.meshes.new('FixerAnkleSock_'+side);sm.from_pydata(sv,[],sf);sm.materials.append(sockmat)
    sock=bpy.data.objects.new('FixerAnkleSock_'+side,sm);bpy.context.collection.objects.link(sock)
    for p in sm.polygons:p.use_smooth=True
    parts.append(sock)
    bpy.ops.object.select_all(action='DESELECT')
    for part in parts:part.select_set(True)
    bpy.context.view_layer.objects.active=shoe
    bpy.ops.object.join()
    shoe.name='SM_FixerSneakerBody_'+side
    shoe.data.name=shoe.name
    objects.append(shoe)
    bpy.ops.object.select_all(action='DESELECT');shoe.select_set(True)
    bpy.context.view_layer.objects.active=shoe
    bpy.ops.export_scene.fbx(filepath=str(OUT/(shoe.name+'.fbx')),use_selection=True,object_types={'MESH'},
        add_leaf_bones=False,bake_anim=False,apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',path_mode='COPY',embed_textures=True)
    report['exports'][side]={'name':shoe.name,'vertices':len(shoe.data.vertices),'target_ankle':list(target),
        'bounds':[(min(v.co[i] for v in shoe.data.vertices),max(v.co[i] for v in shoe.data.vertices)) for i in range(3)]}
    local=shoe.copy();local.data=shoe.data.copy()
    local.name='SM_FixerSneaker_'+side
    bpy.context.collection.objects.link(local)
    boneworld=rig.matrix_world@rig.data.bones['foot_'+side.lower()].matrix_local
    for v in local.data.vertices:v.co=(boneworld.inverted()@v.co)*.01
    bpy.ops.object.select_all(action='DESELECT');local.select_set(True)
    bpy.context.view_layer.objects.active=local
    bpy.ops.export_scene.fbx(filepath=str(OUT/(local.name+'.fbx')),use_selection=True,object_types={'MESH'},
        add_leaf_bones=False,bake_anim=False,apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',path_mode='COPY',embed_textures=True)
    bpy.data.objects.remove(local,do_unlink=True)

for o in list(bpy.data.objects):
    if o not in objects and o not in added:bpy.data.objects.remove(o,do_unlink=True)
for o in added:o.hide_render=True;o.hide_set(True)
# Save all original texture pixels with the authored asset for future revisions.
bpy.ops.outliner.orphans_purge(do_recursive=True)
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'FixerSneakers.blend'))
(OUT/'details_report.json').write_text(json.dumps(report,indent=2))
print('FIXER_DETAILS_READY',json.dumps(report))
