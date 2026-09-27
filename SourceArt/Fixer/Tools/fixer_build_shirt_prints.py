"""Author thin pineapple shirt prints fitted and weighted to native Fixer tee."""
import bpy, math, json, argparse, sys
from pathlib import Path
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

parser=argparse.ArgumentParser()
parser.add_argument('--source',default='/private/tmp/FixerNativeOutfit.fbx')
parser.add_argument('--output',default='/private/tmp/fixer_details')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
OUT=Path(args.output);OUT.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=args.source,use_anim=False)
rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
garments=[o for o in bpy.data.objects if o.type=='MESH']
verts=[];faces=[];weight_records=[]
for g in garments:
    offset=len(verts)
    verts.extend([g.matrix_world@v.co for v in g.data.vertices])
    faces.extend([[i+offset for i in p.vertices] for p in g.data.polygons])
    names={vg.index:vg.name for vg in g.vertex_groups}
    weight_records.extend([{names[w.group]:w.weight for w in v.groups} for v in g.data.vertices])
bvh=BVHTree.FromPolygons(verts,faces)
tree=KDTree(len(verts))
for i,p in enumerate(verts):tree.insert(p,i)
tree.balance()
neck=rig.matrix_world@rig.data.bones['neck_01'].head_local

def mat(name,rgb):
    m=bpy.data.materials.new(name);m.diffuse_color=(*rgb,1);m.use_nodes=True
    n=m.node_tree.nodes.get('Principled BSDF');n.inputs['Base Color'].default_value=(*rgb,1)
    n.inputs['Roughness'].default_value=.93
    return m
pink=mat('M_FixerPrintCoral',(.61,.23,.35))
teal=mat('M_FixerPrintTeal',(.18,.46,.42))
cream=mat('M_FixerPrintCream',(.65,.65,.59))
dark=mat('M_FixerPrintNavy',(.017,.025,.049))
pieces=[]

def surface(x,z,back):
    y=.60 if back else -.60
    direction=Vector((0,-1 if back else 1,0))
    hit,normal,_,_=bvh.ray_cast(Vector((x,y,z)),direction,1.3)
    if hit is None: raise RuntimeError('Shirt print misses cloth at '+str((x,z,back)))
    return Vector((x,hit.y+(.004 if back else -.004),z))

def poly(name,coords,back,material):
    mesh=bpy.data.meshes.new(name)
    points=[surface(x,z,back) for x,z in coords]
    if 'pattern' in name:
        for p in points:p.y+=(.00025 if back else -.00025)
    indices=tuple(range(len(coords)))
    # XZ silhouettes are clockwise viewed from the front (-Y).
    # Reverse the front so single-sided Unreal materials face outward.
    mesh.from_pydata(points,[],[indices if back else tuple(reversed(indices))])
    mesh.materials.append(material)
    ob=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(ob)
    pieces.append(ob)
    return ob

def pineapple(back):
    width=.134 if back else .036
    scale=width/.134
    cx=0 if back else .088
    cz=neck.z-(.252 if back else .17)
    rx=width*.37;rz=.083*scale
    # Rounded printed fruit silhouette.
    coords=[]
    for i in range(48):
        a=2*math.pi*i/48
        coords.append((cx+rx*math.sin(a)*(1+.1*math.cos(a)),cz+rz*math.cos(a)))
    poly('Pineapple fruit',coords,back,pink)
    # Small curved pattern marks retain individual printed detail at close range.
    for row in range(12):
        yy=(row/11*2-1)*rz*.91
        for col in range(-4,5):
            xx=(col+(row%2)*.5)*rx*.24
            if (xx/rx)**2+(yy/rz)**2>.79:continue
            w=.0032*scale;h=.0051*scale
            color=[cream,teal,dark][(row+col)%3]
            poly('Pineapple pattern',[(cx+xx-w,cz+yy),(cx+xx,cz+yy+h),
                (cx+xx+w,cz+yy),(cx+xx,cz+yy-h)],back,color)
    base=cz+rz*.91
    for side in [-1,1]:
        for j in range(4):
            reach=(.053-j*.009)*scale
            rise=(.026+j*.009)*scale
            start=.010*j*scale
            upper=[];lower=[]
            for k in range(10):
                t=k/9
                x=cx+side*(reach*t)
                z=base+start+rise*math.sin(t*math.pi*.67)
                thickness=.006*scale*math.sin(t*math.pi)
                upper.append((x,z+thickness));lower.append((x,z-thickness))
            poly('Pineapple leaf',upper+list(reversed(lower)),back,teal)
    poly('Pineapple crown',[(cx-.006*scale,base+.063*scale),(cx,base+.089*scale),
        (cx+.006*scale,base+.063*scale),(cx,base+.045*scale)],back,teal)
    return cx,cz,scale

pineapple(False)
cx,cz,scale=pineapple(True)

def text_print(text,z,width,font_file,back=True):
    cu=bpy.data.curves.new(text,'FONT');cu.body=text;cu.size=.05;cu.align_x='CENTER'
    cu.resolution_u=8
    if Path(font_file).is_file():cu.font=bpy.data.fonts.load(font_file)
    ob=bpy.data.objects.new(text,cu);bpy.context.collection.objects.link(ob)
    bpy.context.view_layer.objects.active=ob;ob.select_set(True)
    bpy.ops.object.convert(target='MESH')
    bpy.context.view_layer.update()
    xs=[v.co.x for v in ob.data.vertices]
    actual=max(xs)-min(xs);s=width/actual
    for v in ob.data.vertices:
        # Back lettering reads correctly from behind; horizontal axis reverses.
        v.co=surface(-v.co.x*s,z+v.co.y*s,back)
    ob.data.materials.append(cream);pieces.append(ob)
    ob.select_set(False)

text_print('Myrtle Beach',cz-.113,.174,'/System/Library/Fonts/Supplemental/Georgia.ttf')
text_print('South Carolina',cz-.134,.119,'/System/Library/Fonts/Supplemental/Georgia Italic.ttf')
bpy.ops.object.select_all(action='DESELECT')
for p in pieces:p.select_set(True)
bpy.context.view_layer.objects.active=pieces[0]
bpy.ops.object.join()
prints=pieces[0];prints.name='SK_FixerShirtPrints';prints.data.name=prints.name
groups={}
for v in prints.data.vertices:
    world=prints.matrix_world@v.co
    near=tree.find_n(world,3)
    total=0;weights={}
    for _,i,d in near:
        influence=1/max(d,.003)**2;total+=influence
        for name,w in weight_records[i].items():weights[name]=weights.get(name,0)+w*influence
    for name,w in weights.items():
        if w/total<.005:continue
        if name not in groups:groups[name]=prints.vertex_groups.new(name=name)
        groups[name].add([v.index],w/total,'REPLACE')
    v.co=rig.matrix_world.inverted()@world
prints.parent=rig;prints.matrix_parent_inverse=Matrix.Identity(4);prints.matrix_basis=Matrix.Identity(4)
prints.matrix_world=rig.matrix_world.copy()
mod=prints.modifiers.new('Native skeleton','ARMATURE');mod.object=rig
# The project's established native importer uses convert_scene_unit=False.
# Blender's ALL scale export writes centimeter skeleton channels, but converts
# geometry into declared meter units. Unreal then reads those positions as cm.
# Expand ONLY the export mesh vertices by 100; native bone transforms stay intact.
# Keep the editable source blend in physically correct meter space for fitting.
export_mesh=prints.copy();export_mesh.data=prints.data.copy()
export_mesh.name='SK_FixerShirtPrints_UEExport'
bpy.context.collection.objects.link(export_mesh)
for v in export_mesh.data.vertices:v.co*=100.0
bpy.ops.object.select_all(action='DESELECT');export_mesh.select_set(True);rig.select_set(True)
bpy.context.view_layer.objects.active=export_mesh
bpy.ops.export_scene.fbx(filepath=str(OUT/'SK_FixerShirtPrints.fbx'),use_selection=True,
    object_types={'ARMATURE','MESH'},add_leaf_bones=False,armature_nodetype='NULL',
    use_armature_deform_only=False,bake_anim=False,apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
bpy.data.objects.remove(export_mesh,do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'FixerShirtPrints.blend'))
print('FIXER_PRINTS_READY',len(prints.data.vertices),[m.name for m in prints.data.materials])
