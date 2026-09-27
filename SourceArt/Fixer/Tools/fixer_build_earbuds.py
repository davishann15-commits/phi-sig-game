"""Blender authoring: fitted white wired earbuds on Fixer's native skeleton.

Uses saved face landmarks and the exported new body/tee. The ears stay rigid
to the head; the cables blend through neck and torso weights, without physics
that could fling them away when the lobby turntable is moved.
"""
import bpy, math, json
from pathlib import Path
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

OUT=Path('/private/tmp/fixer_lean_details');OUT.mkdir(exist_ok=True)
report=json.loads(Path('/private/tmp/fixer_lean_report.json').read_text())
landmarks=report.get('face_landmarks') or report['face_after']
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath='/private/tmp/FixerLeanBody.fbx',use_anim=False)
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
body_meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
bpy.ops.import_scene.fbx(filepath='/private/tmp/FixerLeanOutfit.fbx',use_anim=False)
outfit_meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and o not in body_meshes]
points=[];polys=[]
for ob in outfit_meshes:
    start=len(points);points.extend(ob.matrix_world@v.co for v in ob.data.vertices)
    polys.extend([i+start for i in p.vertices] for p in ob.data.polygons)
bvh=BVHTree.FromPolygons(points,polys)
def native(v):return Vector((v[0]*.01,-v[1]*.01,v[2]*.01))
def material(name,color,rough):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
    bsdf=m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value=(*color,1)
    bsdf.inputs['Roughness'].default_value=rough
    return m
white=material('M_FixerEarbudWhite',(.80,.80,.77),.28)
rubber=material('M_FixerEarbudWire',(.69,.70,.68),.51)
vent=material('M_FixerEarbudVent',(.035,.04,.043),.72)
parts=[]
def ellipsoid(name,center,scale,mat,weight):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=12,location=center)
    ob=bpy.context.object;ob.name=name;ob.scale=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    ob.data.materials.append(mat)
    for p in ob.data.polygons:p.use_smooth=True
    ob['weights']=json.dumps(weight);parts.append(ob)
    return ob
def wire(name,pts,radius,mat,weights=None):
    cu=bpy.data.curves.new(name,'CURVE');cu.dimensions='3D'
    cu.resolution_u=12;cu.bevel_depth=radius;cu.bevel_resolution=3
    sp=cu.splines.new('BEZIER');sp.bezier_points.add(len(pts)-1)
    for bp,pos in zip(sp.bezier_points,pts):
        bp.co=pos;bp.handle_left_type='AUTO';bp.handle_right_type='AUTO'
    ob=bpy.data.objects.new(name,cu);bpy.context.collection.objects.link(ob)
    bpy.context.view_layer.objects.active=ob;ob.select_set(True)
    bpy.ops.object.convert(target='MESH');ob=bpy.context.object
    ob.data.materials.append(mat)
    for p in ob.data.polygons:p.use_smooth=True
    ob['weights']=json.dumps(weights or {});parts.append(ob)
    ob.select_set(False)
    return ob
def front(x,z,offset=.006):
    hit,n,index,d=bvh.ray_cast(Vector((x,-.6,z)),Vector((0,1,0)),1.3)
    return Vector((x,hit.y-offset,z)) if hit else Vector((x,-.11,z))
neck=(rig.matrix_world@rig.data.bones['neck_01'].head_local).z
head=(rig.matrix_world@rig.data.bones['head'].head_local).z
join=front(.005,neck-.16,.011)
for sign,index,lobe in [(-1,69,54),(1,73,38)]:
    # Seat the speaker low in the concha, between the ear-root and lobe
    # landmarks. The upper root alone put it above the actual ear canal.
    ear=native(landmarks[index])*.45+native(landmarks[lobe])*.55
    ear.x+=sign*.0016;ear.y-=.0010;ear.z-=.0010
    label='L' if sign>0 else 'R'
    ellipsoid('Earbud '+label,ear,(.0039,.0054,.0070),white,{'head':1})
    # Black side vents are inset and small, not a black stripe across the ear.
    ellipsoid('Acoustic vent '+label,ear+Vector((sign*.0036,-.0005,.0018)),
              (.00035,.0013,.0025),vent,{'head':1})
    stem_end=ear+Vector((sign*.0003,-.0020,-.0185))
    wire('Earbud stem '+label,[ear,ear+Vector((0,-.001,-.009)),stem_end],.00185,white,{'head':1})
    # Loose cables drop beside the jaw, then curve forward over the collar.
    collar=front(sign*.055,neck-.030,.007)
    drop=Vector((ear.x*.91,ear.y-.009,neck+.035))
    chest=front(sign*.033,neck-.105,.009)
    pts=[stem_end,stem_end+Vector((0,-.001,-.012)),drop,collar,chest,join]
    wire('Earbud cord '+label,pts,.0009,rubber)
    if sign==1:
        remote=front(.030,neck-.094,.010)
        ellipsoid('Inline remote',remote,(.0020,.0017,.0130),white,{'spine_05':1})
        ellipsoid('Remote button',remote+Vector((0,-.0016,.0004)),(.0012,.0004,.0045),white,{'spine_05':1})
ellipsoid('Cable Y junction',join,(.0021,.0020,.0040),rubber,{'spine_05':1})
end=front(.115,neck-.42,.007)
wire('Lower earbud cable',[join,front(.026,neck-.22,.010),front(.063,neck-.31,.009),end],.0010,rubber)
# All vertices use native skeleton names, with smooth weights at head/neck.
for ob in parts:
    prescribed=json.loads(ob.get('weights','{}'))
    for v in ob.data.vertices:
        p=ob.matrix_world@v.co
        if prescribed:weights=prescribed
        elif p.z>=neck+.02:
            t=max(0.,min(1.,(p.z-neck-.02)/max(.01,head-neck-.02)))
            weights={'head':t,'neck_01':1-t}
        elif p.z>=neck-.07:
            t=max(0.,min(1.,(p.z-neck+.07)/.09))
            weights={'neck_01':t,'spine_05':1-t}
        elif p.z>=neck-.24:weights={'spine_05':1}
        elif p.z>=neck-.36:
            t=(p.z-neck+.36)/.12;weights={'spine_05':t,'spine_04':1-t}
        else:weights={'spine_04':.5,'spine_03':.5}
        for bone,w in weights.items():
            if w<.0001:continue
            vg=ob.vertex_groups.get(bone) or ob.vertex_groups.new(name=bone)
            vg.add([v.index],w,'REPLACE')
bpy.ops.object.select_all(action='DESELECT')
for ob in parts:ob.select_set(True)
bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join()
mesh=parts[0];mesh.name='SK_FixerWiredEarbuds'
# Bake world coordinates, then use exactly the imported native bind space.
for v in mesh.data.vertices:v.co=rig.matrix_world.inverted()@(mesh.matrix_world@v.co)
mesh.parent=rig;mesh.matrix_parent_inverse=Matrix.Identity(4);mesh.matrix_basis=Matrix.Identity(4)
mesh.matrix_world=rig.matrix_world.copy()
mod=mesh.modifiers.new('Native body pose','ARMATURE');mod.object=rig
export=mesh.copy();export.data=mesh.data.copy();bpy.context.collection.objects.link(export)
export.name='SK_FixerWiredEarbuds_UEExport'
for v in export.data.vertices:v.co*=100
bpy.ops.object.select_all(action='DESELECT');export.select_set(True);rig.select_set(True)
bpy.context.view_layer.objects.active=export
bpy.ops.export_scene.fbx(filepath=str(OUT/'SK_FixerWiredEarbuds.fbx'),use_selection=True,
    object_types={'ARMATURE','MESH'},add_leaf_bones=False,armature_nodetype='NULL',
    use_armature_deform_only=False,bake_anim=False,apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
bpy.data.objects.remove(export,do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'FixerWiredEarbuds.blend'))
print('FIXER_EARBUDS_AUTHORED',len(mesh.data.vertices),'neck',neck,'head',head)
