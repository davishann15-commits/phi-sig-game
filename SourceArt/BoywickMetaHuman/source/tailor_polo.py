"""Tailor Boy Wick's native weighted garment, retaining its fitted skin weights."""
from pathlib import Path
from math import sin, cos, pi
import json
import bpy
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree
from mathutils.bvhtree import BVHTree

TASK = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BoywickMetaHuman')
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(TASK / 'BoywickOutfit.fbx'), use_anim=False)
rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
outfit = max((o for o in bpy.data.objects if o.type == 'MESH'), key=lambda o: len(o.data.vertices))
for obj in list(bpy.data.objects):
    if obj not in (rig, outfit): bpy.data.objects.remove(obj, do_unlink=True)
original_matrix = outfit.matrix_world.copy()
groups = {g.index: g.name for g in outfit.vertex_groups}
shirt_slots = {i for i,m in enumerate(outfit.data.materials) if 'Shirt' in m.name}
short_slots = {i for i,m in enumerate(outfit.data.materials) if 'Short' in m.name}
if not shirt_slots or not short_slots or len(groups) < 40:
    raise RuntimeError('Expected the native fitted shirt, shorts, and skeleton')
def material(name):
    return bpy.data.materials.new(name)
polo, khaki, collar, placket, button, sole, upper, lace = [material(n) for n in (
    'BW_Polo', 'BW_Khaki', 'BW_Collar', 'BW_PinkPlacket', 'BW_Buttons',
    'BW_SneakerSole', 'BW_SneakerUpper', 'BW_Laces')]
garment_sections = [0 if poly.material_index in shirt_slots else 1
                    for poly in outfit.data.polygons]
outfit.data.materials.clear()
outfit.data.materials.append(polo); outfit.data.materials.append(khaki)
for poly, section in zip(outfit.data.polygons, garment_sections):
    poly.material_index = section
shirt_indices = {i for p in outfit.data.polygons if p.material_index == 0 for i in p.vertices}
shirt_tree = KDTree(len(shirt_indices))
for i in shirt_indices: shirt_tree.insert(outfit.data.vertices[i].co, i)
shirt_tree.balance()
surface = BVHTree.FromPolygons([v.co.copy() for v in outfit.data.vertices],
    [list(p.vertices) for p in outfit.data.polygons if p.material_index == 0])
neck_candidates = [v.co for v in outfit.data.vertices if v.index in shirt_indices and abs(v.co.x) < 8]
neck_z = max(v.z for v in neck_candidates)
neck_points = [v for v in neck_candidates if v.z > neck_z - 2.2]
neck_y = (min(v.y for v in neck_points) + max(v.y for v in neck_points)) * .5
pieces = [outfit]
def create(name, vertices, faces, mat, fixed_bone=None):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces); mesh.update()
    mesh.materials.append(mat)
    obj = bpy.data.objects.new(name, mesh); bpy.context.collection.objects.link(obj)
    for poly in mesh.polygons: poly.use_smooth = True
    uv = mesh.uv_layers.new(name='UVMap')
    for poly in mesh.polygons:
        for loop in poly.loop_indices:
            p = mesh.vertices[mesh.loops[loop].vertex_index].co
            uv.data[loop].uv = (p.x / 30 + .5, p.z / 180)
    weight_groups = {name:obj.vertex_groups.new(name=name) for name in groups.values()}
    if fixed_bone and fixed_bone not in weight_groups:
        weight_groups[fixed_bone] = obj.vertex_groups.new(name=fixed_bone)
    for vertex in mesh.vertices:
        if fixed_bone:
            weight_groups[fixed_bone].add([vertex.index], 1.0, 'REPLACE')
        else:
            found = shirt_tree.find_n(vertex.co, 4)
            weights, total = {}, 0.0
            for _, index, distance in found:
                influence = 1.0 / max(distance, .05) ** 2
                total += influence
                for group in outfit.data.vertices[index].groups:
                    key = groups[group.group]
                    weights[key] = weights.get(key, 0.0) + group.weight * influence
            for key, value in weights.items():
                if value / total > .0001: weight_groups[key].add([vertex.index], value / total, 'REPLACE')
    obj.parent = rig; obj.matrix_world = original_matrix.copy()
    obj.modifiers.new('Boywick native skin', 'ARMATURE').object = rig
    pieces.append(obj)
    return obj

# Folded collar with two visible front points, rather than a painted ring.
vertices, faces = [], []
for ring in range(4):
    for i in range(64):
        a = 2*pi*i/64
        front = max(0.0, -sin(a))
        rx = [6.0, 6.5, 8.5, 8.7][ring]
        ry = [5.4, 5.9, 7.3, 7.5][ring]
        z = neck_z + [.25, 1.0, -3.2, -3.6][ring] - front*([0,0,2.2,2.2][ring])
        vertices.append((rx*cos(a), neck_y+ry*sin(a), z))
        if ring:
            b=(ring-1)*64+i; n=(i+1)%64
            faces.append((b, (ring-1)*64+n, ring*64+n, ring*64+i))
create('Boywick_FoldedPoloCollar', vertices, faces, collar)

def front_y(x,z):
    hit,_,_,_=surface.ray_cast(Vector((x,-100,z)),Vector((0,1,0)),200)
    return hit.y-.18 if hit else neck_y-6.5
# Two folded front points sit over the shirt, with a visible opening between.
for side in (-1,1):
    outline=[(2.1,neck_z-1.2),(7.8,neck_z-1.0),(10.0,neck_z-5.8),
             (5.0,neck_z-9.4),(3.0,neck_z-5.0)]
    verts=[(side*x,front_y(side*x,z)-(.65 if i!=3 else 1.0),z)
           for i,(x,z)in enumerate(outline)]
    create('Boywick_PoloCollarPoint'+str(side),verts,
           [(0,1,2),(0,2,3),(0,3,4)],collar)
# Soft pink inner placket, navy edge, and pearl buttons follow the sewn chest.
for name,width,length,mat,offset in [('PoloPlacketTrim',2.45,12.8,collar,.08),('PoloPinkPlacket',1.8,12.0,placket,.16)]:
    verts,polys=[],[]
    for r in range(16):
        z=neck_z-1.6-r*length/15
        for x in (-width,width):verts.append((x,front_y(x,z)-offset,z))
        if r: b=(r-1)*2;polys.append((b,b+1,b+3,b+2))
    create('Boywick_'+name,verts,polys,mat)
for index in range(3):
    z=neck_z-4.1-index*3.4
    verts=[(0,front_y(0,z)-.42,z)]
    for i in range(16):
        a=i*2*pi/16;verts.append((.25*cos(a),front_y(0,z)-.35,z+.25*sin(a)))
    create('Boywick_PearlButton'+str(index),verts,[(0,1+i,1+(i+1)%16)for i in range(16)],button)

# Rounded canvas sneakers, with a separate rubber sole and laces.
for side in ('l','r'):
    foot = rig.data.bones['foot_'+side].head_local
    ball = rig.data.bones['ball_'+side].head_local
    center_x = (foot.x+ball.x)*.5
    center_y = foot.y-8.2
    for name,mat,rings in [
        ('Sole',sole,[(1.2,1.2,1.0),(6.2,15.5,1.2),(6.5,15.8,2.5),(6.2,15.5,3.7)]),
        ('Upper',upper,[(6.0,15.2,3.5),(5.6,14.6,5.8),(4.5,12.1,8.4),(2.0,5.0,10.0)])]:
        verts,polys=[],[]
        for r,(rx,ry,z)in enumerate(rings):
            for i in range(48):
                a=i*2*pi/48
                # A slightly wider toe gives a realistic low-top silhouette.
                verts.append((center_x+rx*cos(a),center_y+ry*sin(a),z))
                if r:
                    b=(r-1)*48+i;n=(i+1)%48
                    polys.append((b,(r-1)*48+n,r*48+n,r*48+i))
        polys.append(tuple(range(47,-1,-1)))
        polys.append(tuple((len(rings)-1)*48+i for i in range(48)))
        create('Boywick_Sneaker'+name+'_'+side,verts,polys,mat,'foot_'+side)
    for i in range(5):
        y=center_y-4+i*1.6;z=9.25-.1*i
        verts,faces=[],[]
        for j in range(2):
            for k in range(8):
                a=k*2*pi/8
                verts.append((center_x+(-3.1 if j==0 else 3.1),y+.12*cos(a),z+.12*sin(a)))
                if j:faces.append((k,(k+1)%8,8+(k+1)%8,8+k))
        create('Boywick_Lace'+side+str(i),verts,faces,lace,'foot_'+side)

# Joining uses the original garment transform, preserving native centimeter
# geometry and bind matrices for the established Unreal FBX import path.
bpy.ops.object.select_all(action='DESELECT')
for obj in pieces:obj.select_set(True)
bpy.context.view_layer.objects.active=outfit
bpy.ops.object.join()
outfit.matrix_world=Matrix.Identity(4)
outfit.name='SK_Boywick_PoloOutfit';outfit.data.name=outfit.name
bpy.ops.object.select_all(action='DESELECT');outfit.select_set(True);rig.select_set(True)
bpy.context.view_layer.objects.active=outfit
bpy.ops.export_scene.fbx(filepath=str(TASK/'SK_Boywick_PoloOutfit.fbx'),use_selection=True,
    object_types={'ARMATURE','MESH'},add_leaf_bones=False,armature_nodetype='NULL',
    use_armature_deform_only=False,bake_anim=False,apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
bpy.ops.wm.save_as_mainfile(filepath=str(TASK/'Boywick_Polo.blend'))
(TASK/'evidence/tailoring.json').write_text(json.dumps({'vertices':len(outfit.data.vertices),
    'neck_z':neck_z,'materials':[m.name for m in outfit.data.materials],
    'vertex_groups':len(outfit.vertex_groups)},indent=2))
print('BOYWICK_POLO_TAILORED',len(outfit.data.vertices))
