"""Build the staged spa room in final house world metres, Blender 4/5.

Run: Blender -b --python build_spa_room.py
Unreal conversion is [100*x, -100*y, 100*z]. There is no extra house scaling.
The export contains no water or steam. The native water actor fills the exact
2.95 x 2.50m, 0.22m-radius opening at Z=.73. All visuals have separate simple
collision in COL_SpaRoom, including a walkable basin bottom and four entry steps.
The ceiling underside is 2.78m; its 5cm slab ends below the protected upper floor.
"""
import bpy
import math
import os
import json
import collections
from mathutils import Vector

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(ROOT, 'models')
os.makedirs(OUT, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
groups = collections.defaultdict(list)
colliders = []
materials = {}
pieces = []
lights = []
FLOOR = .025
TUB = (-27.95, -4.25)


def mat(name, color, rough=.5, metal=0):
    m=bpy.data.materials.new('Spa_'+name)
    m.diffuse_color=(*color,1);m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Roughness'].default_value=rough
    p.inputs['Metallic'].default_value=metal
    materials[name]=m
    return m


STONE=mat('Travertine',(.70,.64,.52),.72)
GROUT=mat('Grout',(.40,.37,.32),.90)
WOOD=mat('Walnut',(.17,.080,.031),.40)
BRASS=mat('Brass',(.55,.38,.13),.32,.80)
ACRYLIC=mat('CreamAcrylic',(.84,.83,.73),.22)
METAL=mat('DarkMetal',(.020,.024,.022),.40,.70)
LINEN=mat('Linen',(.77,.72,.61),.90)
SAGE=mat('Sage',(.18,.27,.22),.80)
LEAF=mat('GreenLeaf',(.055,.17,.069),.58)
CLAY=mat('TerraCotta',(.26,.14,.090),.65)
GLASS=mat('Glass',(.60,.76,.74),.12)
ART=mat('Art',(.52,.47,.34),.90)
GLOW=mat('LampGlow',(1,.72,.40),.25)
CITRUS=mat('Citrus',(.74,.39,.035),.60)
STRIPE=mat('TowelStripe',(.21,.28,.24),.90)
glow_bsdf=GLOW.node_tree.nodes.get('Principled BSDF')
glow_bsdf.inputs['Emission Color'].default_value=(1,.65,.30,1)
glow_bsdf.inputs['Emission Strength'].default_value=3
glass_bsdf=GLASS.node_tree.nodes.get('Principled BSDF')
glass_bsdf.inputs['Transmission Weight'].default_value=.65
glass_bsdf.inputs['IOR'].default_value=1.45


def mesh(name, vertices, faces, material, group, smooth=False):
    data=bpy.data.meshes.new(name+'_Mesh');data.from_pydata(vertices,[],faces);data.update()
    o=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(o)
    if material:data.materials.append(material)
    for p in data.polygons:p.use_smooth=smooth
    uv=data.uv_layers.new(name='UVMap')
    for p in data.polygons:
        axis=max(range(3),key=lambda k:abs(p.normal[k]))
        a,b=((1,2),(0,2),(0,1))[axis]
        for loop in p.loop_indices:
            q=data.vertices[data.loops[loop].vertex_index].co
            uv.data[loop].uv=(q[a],q[b])
    if group:groups[group].append(o)
    return o


def box(name, center, size, material, group, bevel=0, smooth=False):
    x,y,z=[v/2 for v in size]
    vertices=[(-x,-y,-z),(x,-y,-z),(x,y,-z),(-x,y,-z),
              (-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z)]
    faces=[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    o=mesh(name,vertices,faces,material,group,smooth);o.location=center
    if bevel:
        modifier=o.modifiers.new('Crafted edge','BEVEL');modifier.width=bevel;modifier.segments=3
        bpy.context.view_layer.objects.active=o;o.select_set(True)
        bpy.ops.object.modifier_apply(modifier=modifier.name);o.select_set(False)
    return o


def collider(name,center,size):
    o=box(name,center,size,METAL,None)
    colliders.append(o);return o


def tube(name,points,radius,material,group,closed=False,sides=8):
    pts=[Vector(v) for v in points];verts=[];faces=[];n=len(pts)
    for i,p in enumerate(pts):
        prev=pts[(i-1)%n] if closed or i else p
        nxt=pts[(i+1)%n] if closed or i<n-1 else p
        tangent=(nxt-prev).normalized()
        ref=Vector((0,0,1)) if abs(tangent.z)<.9 else Vector((0,1,0))
        u=tangent.cross(ref).normalized();v=tangent.cross(u).normalized()
        for j in range(sides):
            a=j*math.tau/sides;verts.append(tuple(p+radius*(math.cos(a)*u+math.sin(a)*v)))
    for i in range(n if closed else n-1):
        for j in range(sides):
            faces.append((i*sides+j,i*sides+(j+1)%sides,
                          ((i+1)%n)*sides+(j+1)%sides,((i+1)%n)*sides+j))
    if not closed:
        faces.append(tuple(range(sides-1,-1,-1)))
        faces.append(tuple((n-1)*sides+j for j in range(sides)))
    return mesh(name,verts,faces,material,group,True)


def cylinder(name,center,radius,depth,material,group,vertices=24,axis='Z'):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=center)
    o=bpy.context.object;o.name=name;o.data.materials.append(material)
    if axis=='X':o.rotation_euler.y=math.pi/2
    if axis=='Y':o.rotation_euler.x=math.pi/2
    for p in o.data.polygons:p.use_smooth=len(p.vertices)==4
    if group:groups[group].append(o)
    return o


def sphere(name,center,size,material,group,segments=16,rings=8):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,radius=1,location=center)
    o=bpy.context.object;o.name=name;o.scale=size;o.data.materials.append(material)
    for p in o.data.polygons:p.use_smooth=True
    if group:groups[group].append(o)
    return o


def text(name,body,center,size,material,group,rotation=(math.pi/2,0,0)):
    curve=bpy.data.curves.new(name,'FONT');curve.body=body;curve.align_x='CENTER'
    curve.size=size;curve.extrude=.0012;curve.bevel_depth=.0006;curve.bevel_resolution=1
    o=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(o)
    o.location=center;o.rotation_euler=rotation;o.data.materials.append(material)
    bpy.context.view_layer.objects.active=o;o.select_set(True)
    bpy.ops.object.convert(target='MESH');o.select_set(False)
    groups[group].append(o);return o


def rounded_rect(width,depth,radius,center=(0,0),segments=12):
    x=width/2;y=depth/2;r=min(radius,x,y);cx,cy=center;points=[]
    for ox,oy,start in ((x-r,y-r,0),(-x+r,y-r,90),(-x+r,-y+r,180),(x-r,-y+r,270)):
        for i in range(segments):
            a=math.radians(start+i*90/segments)
            points.append((cx+ox+r*math.cos(a),cy+oy+r*math.sin(a)))
    return points


def spaced_path(points,pitch):
    """Uniform staves along the straight runs as well as the curved corners."""
    pts=[Vector(p) for p in points]
    lengths=[(pts[(i+1)%len(pts)]-p).length for i,p in enumerate(pts)]
    total=sum(lengths);count=math.ceil(total/pitch);samples=[]
    edge=0;before=0.
    for i in range(count):
        target=i*total/count
        while target>before+lengths[edge]:before+=lengths[edge];edge+=1
        t=(target-before)/lengths[edge]
        samples.append(tuple(pts[edge].lerp(pts[(edge+1)%len(pts)],t)))
    return samples


def loop_shell(name,profiles,center,material,group,close_profile=False,smooth=True):
    """Profiles are width/depth/radius/Z. Circumference is always shared."""
    loops=[rounded_rect(w,d,r,center) for w,d,r,z in profiles]
    n=len(loops[0]);verts=[];faces=[]
    for pts,(_,_,_,z) in zip(loops,profiles):verts.extend((x,y,z) for x,y in pts)
    for i in range(len(loops) if close_profile else len(loops)-1):
        j=(i+1)%len(loops)
        for k in range(n):faces.append((i*n+k,i*n+(k+1)%n,j*n+(k+1)%n,j*n+k))
    return mesh(name,verts,faces,material,group,smooth)


def record(name,center,size,notes=''):
    pieces.append({'name':name,'centerMetres':list(center),'sizeMetres':list(size),'notes':notes})


# Large honed stone tiles with actual thin grout lines and a cream border.
SHELL='SM_SpaRoom_Shell'
box('Stone floor substrate',(-26.925,-2.25,.005),(6.35,11.10,.035),GROUT,SHELL)
collider('Spa floor',(-26.925,-2.25,-.0325),(6.35,11.10,.115))
tile=.80;gap=.006
nx=math.ceil(6.35/tile);ny=math.ceil(11.10/tile)
for i in range(nx):
    for j in range(ny):
        x0=-30.1+i*tile;x1=min(x0+tile,-23.75)
        y0=-7.8+j*tile;y1=min(y0+tile,3.3)
        if x1-x0<.03 or y1-y0<.03:continue
        box('Honed travertine tile',((x0+x1)/2,(y0+y1)/2,.020),
            (x1-x0-gap,y1-y0-gap,.010),STONE,SHELL,.002)

for name,center,size in [
    ('West stone wall',(-30.18,-2.25,1.39),(.16,11.26,2.78)),
    ('North stone wall',(-26.925,3.38,1.39),(6.51,.16,2.78)),
    ('South stone wall',(-26.925,-7.88,1.39),(6.51,.16,2.78)),
    ('East liner south',(-23.76,-6.865,1.39),(.095,2.03,2.78)),
    ('East liner north',(-23.76,-.38,1.39),(.095,7.52,2.78)),
    ('Entry stone header',(-23.76,-4.995,2.44),(.095,1.71,.68)),
    ('Warm spa ceiling',(-26.925,-2.25,2.805),(6.51,11.26,.050))]:
    box(name,center,size,STONE,SHELL,.006)
    collider(name+' collision',center,size)
for y in (-5.85,-4.14):
    box('Entry brass reveal',(-23.813,y,1.04),(.025,.022,2.08),BRASS,SHELL,.004)
box('Entry brass reveal header',(-23.813,-4.995,2.083),(.025,1.71,.025),BRASS,SHELL,.004)
for center,size in [((-30.073,-2.25,.075),(.028,11.10,.10)),((-26.925,3.273,.075),(6.35,.028,.10)),
                    ((-26.925,-7.773,.075),(6.35,.028,.10))]:
    box('Walnut wall base trim',center,size,WOOD,SHELL,.004)


# Tub rim is a hollow annulus; nothing covers the water opening.
TUB_SHELL='SM_SpaRoom_TubAcrylic'
loop_shell('Thick rounded acrylic rim',[
    (3.77,3.27,.48,.735),(3.80,3.30,.50,.82),(3.77,3.27,.48,.875),
    (3.65,3.15,.45,.88),(3.07,2.62,.25,.88),(2.95,2.50,.22,.855),
    (2.95,2.50,.22,.73),(2.96,2.51,.225,.70)],TUB,ACRYLIC,TUB_SHELL,True)
loop_shell('Molded basin tapered sidewalls',[
    (2.95,2.50,.22,.855),(2.95,2.50,.22,.73),(2.92,2.47,.25,.66),
    (2.74,2.29,.36,.41),(2.62,2.17,.40,.245)],TUB,ACRYLIC,TUB_SHELL)
bottom=rounded_rect(2.62,2.17,.40,TUB)
mesh('Basin solid low floor',[(x,y,.245) for x,y in bottom],[tuple(range(len(bottom)))],ACRYLIC,TUB_SHELL)
# Low shell below the floor ensures the basin is closed from below as well.
collider('Tub closed basin bottom',(*TUB,.1225),(3.80,3.30,.245))
for x in (-1.6875,1.6875):collider('Tub side wall',(TUB[0]+x,TUB[1],.565),(.425,3.30,.64))
for y in (-1.45,1.45):collider('Tub end wall',(TUB[0],TUB[1]+y,.565),(2.95,.40,.64))

# Four gently shaped interior seats and headrests, all visible below native water.
for name,center,size in [
    ('West submerged seat',(TUB[0]-1.12,TUB[1],.43),(.46,1.94,.18)),
    ('East submerged seat',(TUB[0]+1.12,TUB[1],.43),(.46,1.94,.18)),
    ('North submerged bench',(TUB[0],TUB[1]+.91,.43),(1.72,.45,.18)),
    ('South submerged bench',(TUB[0],TUB[1]-.91,.43),(1.72,.45,.18))]:
    box(name,center,size,ACRYLIC,TUB_SHELL,.085,True)
    collider(name+' collision',center,size)
for dx,dy,axis in [(-1.44,-.66,'X'),(-1.44,.62,'X'),(1.44,-.66,'X'),(1.44,.62,'X'),(-.65,1.22,'Y'),(.64,1.22,'Y'),(-.65,-1.22,'Y'),(.64,-1.22,'Y')]:
    c=(TUB[0]+dx,TUB[1]+dy,.61)
    cylinder('Stainless spa jet outer',c,.052,.024,BRASS,TUB_SHELL,24,axis)
    cylinder('Dark recessed jet throat',c,.027,.026,METAL,TUB_SHELL,20,axis)
for dx in (-.84,.84):
    box('Charcoal spa headrest',(TUB[0]+dx,TUB[1]+1.14,.81),(.40,.19,.14),METAL,TUB_SHELL,.066,True)
for dx in (-.62,.62):
    cylinder('Basin footwell drain',(TUB[0]+dx,TUB[1],.252),.07,.014,METAL,TUB_SHELL,24)
    for a in range(0,180,45):
        aa=math.radians(a)
        tube('Drain radial brass grille',[(TUB[0]+dx-.056*math.cos(aa),TUB[1]-.056*math.sin(aa),.26),
             (TUB[0]+dx+.056*math.cos(aa),TUB[1]+.056*math.sin(aa),.26)],.002,BRASS,TUB_SHELL,False,4)


TUB_WOOD='SM_SpaRoom_TubWalnut'
loop_shell('Recessed tub exterior cabinet',[(3.71,3.21,.46,.09),(3.71,3.21,.46,.77)],TUB,METAL,TUB_WOOD)
outer=spaced_path(rounded_rect(3.74,3.24,.47,TUB,segments=22),.077)
for i,(x,y) in enumerate(outer):
    prev=Vector(outer[(i-1)%len(outer)]);nxt=Vector(outer[(i+1)%len(outer)])
    tangent=(nxt-prev).normalized();ang=math.atan2(tangent.y,tangent.x)
    slab=box('Vertical walnut tub stave',(x,y,.423),(.068,.034,.682),WOOD,TUB_WOOD,.008)
    slab.rotation_euler.z=ang
loop_shell('Walnut plinth shadowline',[(3.78,3.28,.48,.065),(3.78,3.28,.48,.105)],TUB,WOOD,TUB_WOOD)
# Control panel and brass towel hooks on the accessible east edge.
box('Flush spa control surround',(-26.15,-3.10,.886),(.37,.19,.012),METAL,TUB_WOOD,.035)
box('Spa controller face',(-26.15,-3.10,.894),(.27,.10,.008),SAGE,TUB_WOOD,.016)
for x in (-26.23,-26.15,-26.07):cylinder('Controller brass button',(x,-3.12,.901),.017,.010,BRASS,TUB_WOOD,16)

# Four 18cm risers at the south-east corner preserve the broad east walkway.
step_x=-26.74
step_rows=[]
for i in range(4):
    y=-7.16+i*.36;top=.18*(i+1)
    box('Accessible walnut step '+str(i+1),(step_x,y,top/2), (1.18,.36,top),WOOD,TUB_WOOD,.018)
    box('Step nonslip stone tread',(step_x,y,top+.005),(1.14,.325,.012),STONE,TUB_WOOD,.004)
    box('Step brushed brass nosing',(step_x,y-.16,top+.006),(1.14,.015,.010),BRASS,TUB_WOOD,.003)
    collider('Step '+str(i+1)+' collision',(step_x,y,top/2),(1.18,.36,top))
    step_rows.append({'center':[step_x,y],'topZ':top,'width':1.18,'run':.36})
record('Primary open hot tub',(*TUB,.44),(3.80,3.30,.88),'Open annulus, native water added separately; east walkway clear')


# Walnut west-wall rhythm and a brass-trimmed low bench.
LOUNGE='SM_SpaRoom_Lounge'
box('West walnut accent backing',(-30.052,.36,1.37),(.035,5.12,2.61),METAL,LOUNGE)
for i in range(61):
    y=-2.17+i*.084
    box('West vertical walnut wall slat',(-30.026,y,1.37),(.039,.046,2.56),WOOD,LOUNGE,.006)
box('Walnut bench seat',(-29.62,-1.68,.465),(.58,1.72,.085),WOOD,LOUNGE,.025)
for y in (-2.29,-1.07):
    box('Bench dark steel U leg',(-29.62,y,.235),(.38,.048,.42),METAL,LOUNGE,.010)
box('Bench ivory seat pad',(-29.59,-1.68,.532),(.49,1.55,.062),LINEN,LOUNGE,.026,True)
collider('Bench collision',(-29.62,-1.68,.29),(.60,1.72,.55))


def lounger(name,x,y):
    profile=[(-.98,.31),(-.76,.37),(-.48,.42),(-.20,.45),(.08,.46),
             (.31,.49),(.47,.59),(.64,.77),(.83,.98),(.98,1.09)]
    w=.79;t=.105;verts=[];faces=[];xs=8
    for side in (-1,1):
        for j,(yy,zz) in enumerate(profile):
            for k in range(xs+1):
                u=-1+2*k/xs
                swell=.018*(1-u*u)*math.sin(math.pi*j/(len(profile)-1))
                verts.append((x+u*w/2,y+yy,zz+side*t/2+(swell if side>0 else 0)))
    row=xs+1;n=row*len(profile)
    for side in (0,1):
        for j in range(len(profile)-1):
            for k in range(xs):
                f=(side*n+j*row+k,side*n+j*row+k+1,side*n+(j+1)*row+k+1,side*n+(j+1)*row+k)
                faces.append(f if side else tuple(reversed(f)))
    border=list(range(row))+[j*row+xs for j in range(1,len(profile))]
    border += [(len(profile)-1)*row+k for k in range(xs-1,-1,-1)]
    border += [j*row for j in range(len(profile)-2,0,-1)]
    for i,b in enumerate(border):
        c=border[(i+1)%len(border)];faces.append((b,c,c+n,b+n))
    mesh(name+' tailored continuous cushion',verts,faces,LINEN,LOUNGE,True)
    for side in (-1,1):
        pts=[(x+side*.405,y+yy,zz-.085) for yy,zz in profile]
        tube(name+' curved walnut side rail',pts,.028,WOOD,LOUNGE,False,8)
        tube(name+' linen side welt',[(x+side*.392,y+yy,zz+.01) for yy,zz in profile],.0026,LINEN,LOUNGE,False,5)
        for yy in (-.69,.36):
            box(name+' hardwood leg',(x+side*.335,y+yy,.195),(.066,.072,.36),WOOD,LOUNGE,.009)
        tube(name+' graceful walnut arm',[(x+side*.45,y-.2,.63),(x+side*.45,y+.25,.66),(x+side*.43,y+.48,.68)],.024,WOOD,LOUNGE,False,8)
        for yy in (-.19,.29):
            cylinder(name+' brass joiner',(x+side*.453,y+yy,.633),.018,.015,BRASS,LOUNGE,16,'X')
    for yy,zz in profile[1:-1]:
        tube(name+' cushion transverse stitch',[(x-.37,y+yy,zz+.054),(x,y+yy,zz+.072),(x+.37,y+yy,zz+.054)],.0015,STRIPE,LOUNGE,False,4)
    collider(name+' main seat collider',(x,y-.23,.23),(.82,1.36,.46))
    collider(name+' sloping back collider',(x,y+.71,.68),(.82,.58,.67))
    record(name,(x,y,.54),(.90,2.00,1.15),'Walnut frame and upholstered curved chaise facing south')


lounger('West relaxation chaise',-28.61,1.03)
lounger('East relaxation chaise',-26.97,1.03)
box('Lounge brass side table base',(-27.79,.10,.205),(.36,.36,.41),METAL,LOUNGE,.040)
cylinder('Lounge side table stone top',(-27.79,.10,.425),.31,.037,STONE,LOUNGE,40)


# Rolled/folded towels, shelving, discreet storage, and a robe hook rail.
STORAGE='SM_SpaRoom_TowelStorage'
sx,sy=-29.69,2.55
for x in (sx-.275,sx+.275):
    box('Towel cabinet walnut side',(x,sy,.79),(.061,.63,1.52),WOOD,STORAGE,.012)
box('Towel cabinet walnut back',(sx,sy+.275,.79),(.61,.070,1.52),WOOD,STORAGE,.012)
for z in (.064,1.525):
    box('Towel cabinet walnut cap',(sx,sy,z),(.61,.63,.067),WOOD,STORAGE,.015)
box('Open cabinet shadow backing',(sx,sy+.233,.82),(.49,.018,1.34),METAL,STORAGE,.008)
for z in (.18,.58,.98,1.39):
    box('Cabinet deep walnut shelf',(sx,sy-.035,z),(.58,.68,.035),WOOD,STORAGE,.006)
    box('Shelf brass front lip',(sx,sy-.377,z+.013),(.56,.012,.017),BRASS,STORAGE,.003)
collider('Towel cabinet collision',(sx,sy,.77),(.65,.67,1.54))


def rolled_towel(name,center,radius=.064,length=.26,material=LINEN,axis='Y'):
    c=cylinder(name,center,radius,length,material,STORAGE,24,axis)
    # Small spiral on the visible roll end indicates cloth layers.
    pts=[]
    for i in range(26):
        a=i*math.tau/9;r=radius*(.16+.72*i/25)
        if axis=='Y':pts.append((center[0]+r*math.cos(a),center[1]-length/2-.002,center[2]+r*math.sin(a)))
        else:pts.append((center[0]+r*math.cos(a),center[1]+r*math.sin(a),center[2]+length/2+.002))
    tube(name+' rolled cloth edge',pts,.0017,STRIPE,STORAGE,False,4)


for z in (.65,1.06):
    for dx in (-.14,0,.14):rolled_towel('Cabinet rolled ivory towel',(sx+dx,sy-.075,z),.060,.36)
for i in range(4):
    box('Stacked folded sage towel',(sx,sy-.065,.25+i*.052),(.42,.35,.047),SAGE,STORAGE,.018,True)
    box('Folded towel woven border',(sx,sy-.225,.25+i*.052),(.39,.008,.013),STRIPE,STORAGE,.002)
for dy in (-.30,.25):rolled_towel('Bench spare towel',(-29.59,-1.68+dy,.632),.070,.30)
for i in range(3):
    box('Bench folded linen stack',(-29.59,-2.17,.60+i*.036),(.35,.29,.032),LINEN,STORAGE,.012,True)
box('Walnut wall hook rail',(-30.014,-.66,1.70),(.045,.92,.09),WOOD,STORAGE,.014)
for y in (-.97,-.66,-.35):
    tube('Brass robe hook',[(-29.98,y,1.7),(-29.85,y,1.7),(-29.83,y,1.745)],.008,BRASS,STORAGE,False,8)


# Refreshment bar with cane-like walnut front, stone counter, trays and citrus.
BAR='SM_SpaRoom_RefreshmentBar'
bx,by=-24.12,1.04
box('Refreshment cabinet carcass',(bx,by,.505),(.66,2.48,.96),WOOD,BAR,.020)
box('Bar stone countertop',(bx-.02,by,1.017),(.76,2.59,.058),STONE,BAR,.027)
for i in range(39):
    yy=by-1.18+i*.061
    box('Bar vertical walnut reed',(bx-.343,yy,.53),(.025,.027,.86),WOOD,BAR,.004)
box('Bar recessed brass foot rail',(bx-.405,by,.18),(.025,2.26,.025),BRASS,BAR,.010)
collider('Refreshment bar collision',(bx,by,.535),(.76,2.60,1.07))
for yy in (.44,1.61):
    cylinder('Bar stool linen seat',(-25.11,yy,.66),.225,.10,LINEN,BAR,32)
    for dx in (-.14,.14):
        for dy in (-.14,.14):
            tube('Bar stool brass tapered leg',[(-25.11+dx*1.20,yy+dy*1.20,.035),(-25.11+dx,yy+dy,.62)],.012,BRASS,BAR,False,6)
    tube('Bar stool ring footrest',[(-25.11+.19*math.cos(math.tau*i/32),yy+.19*math.sin(math.tau*i/32),.25) for i in range(32)],.009,BRASS,BAR,True,6)
    collider('Bar stool collision',(-25.11,yy,.35),(.46,.46,.70))
for y in (.35,1.28):
    box('Brass bar wall shelf',(-23.889,y,1.78),(.22,.69,.018),BRASS,BAR,.006)
    for yy in (y-.24,y+.24):
        tube('Shelf floating bracket',[(-23.8,yy,1.62),(-23.985,yy,1.775)],.008,BRASS,BAR,False,6)


def glass(name,x,y,z,r=.036,h=.115):
    # Open rim and interior lip; the preview material is clear green glass.
    loop_shell(name,[(r*2,r*2,r*.9,z),(r*2,r*2,r*.9,z+h),
                    (r*1.82,r*1.82,r*.82,z+h),(r*1.82,r*1.82,r*.82,z+.013)],(x,y),GLASS,BAR,True)


box('Bar brass serving tray',(-24.18,.21,1.06),(.41,.76,.020),BRASS,BAR,.050)
for x,y in [(-24.28,.03),(-24.10,.07),(-24.28,.26),(-24.10,.30),(-24.27,.48),(-24.10,.51)]:glass('Open drinking tumbler',x,y,1.075)
for y in (.17,.48,1.06,1.37):glass('Glass on brass floating shelf',-23.97,y,1.795,.033,.13)
cylinder('Citrus bowl broad base',(-24.19,1.48,1.084),.22,.055,ACRYLIC,BAR,40)
for dx,dy,zz in [(-.08,-.03,1.15),(.06,-.09,1.15),(.09,.06,1.15),(-.09,.09,1.15),(.0,.0,1.23)]:
    sphere('Fresh citrus',(-24.19+dx,1.48+dy,zz),(.055,.052,.054),CITRUS,BAR,12,6)
cylinder('Infused water carafe body',(-24.17,2.05,1.26),.088,.33,GLASS,BAR,32)
cylinder('Carafe brushed brass neck',(-24.17,2.05,1.44),.045,.045,BRASS,BAR,24)
box('Small walnut hospitality menu',(-24.20,.91,1.095),(.30,.20,.052),WOOD,BAR,.018)
record('Refreshment bar',(bx,by,.53),(.76,2.60,1.07),'Walnut reeds, stone counter, clear glassware, citrus and brass shelving')


# Sculpted indoor greenery with readable curved leaves, all merged per group.
PLANTS='SM_SpaRoom_Plants'
def leaf(name,start,tip,width,bend=0):
    start=Vector(start);tip=Vector(tip);axis=tip-start
    side=axis.cross(Vector((0,0,1))).normalized()
    if side.length<.1:side=Vector((1,0,0))
    verts=[];faces=[]
    for i in range(7):
        t=i/6;center=start+axis*t+Vector((0,0,bend*math.sin(t*math.pi)))
        ww=width*math.sin(t*math.pi)**.80
        verts.extend([tuple(center-side*ww),tuple(center+Vector((0,0,.006))),tuple(center+side*ww)])
    for i in range(6):
        for j in (0,1):faces.append((i*3+j,(i+1)*3+j,(i+1)*3+j+1,i*3+j+1))
    return mesh(name,verts,faces,LEAF,PLANTS,True)


def plant(name,x,y,height=1.55):
    loop_shell(name+' ceramic planter',[(.45,.45,.22,.025),(.55,.55,.26,.39),
         (.57,.57,.27,.44),(.51,.51,.24,.44),(.45,.45,.22,.13)],(x,y),CLAY,PLANTS,True)
    cylinder(name+' dark soil',(x,y,.402),.24,.010,METAL,PLANTS,24)
    tube(name+' woody trunk',[(x,y,.35),(x+.045,y+.025,height*.59),(x-.015,y+.045,height*.88)],.025,WOOD,PLANTS,False,8)
    for j in range(19):
        a=j*2.40;z=.50+(height-.6)*j/19;r=.34+.16*math.sin(j*1.1)**2
        tip=(max(-30.01,min(-23.90,x+math.cos(a)*r)),
             max(-7.67,min(3.14,y+math.sin(a)*r)),z+.15)
        st=(x+.025*math.sin(j),y+.025*math.cos(j),z-.12)
        tube(name+' leaf stem',[st,tip],.006,LEAF,PLANTS,False,5)
        leaf(name+' sculpted leaf',st,tip,.10,.075)
    collider(name+' pot collision',(x,y,.22),(.54,.54,.44))


plant('West spa rubber tree',-29.63,-.22,1.80)
plant('North bar greenery',-24.32,2.77,1.60)


# Architectural sconces, recessed ceiling lamps and abstract framed wall art.
DECOR='SM_SpaRoom_ArchitecturalDetails'
for yy in (-6.38,-3.30,.68,2.52):
    x=-29.975
    box('Brass sconce mounting plate',(-30.078,yy,1.78),(.022,.11,.37),BRASS,DECOR,.020)
    box('Sconce diffused opal glass',(x,yy,1.79),(.075,.080,.29),GLOW,DECOR,.035)
    for z in (1.635,1.945):box('Sconce brass cap',(x,yy,z),(.10,.102,.036),BRASS,DECOR,.012)
    lights.append({'name':'West warm spa sconce','type':'Point','blenderMetres':[-29.80,yy,1.80],
                   'unrealCm':[-2980,-yy*100,180],'colorRGB':[1,.77,.48],'intensityLumens':260,'attenuationMetres':3.20})
for x,y in [(-28.25,-6.85),(-25.18,-6.85),(-28.5,-.25),(-25.6,-.25),(-28.4,2.17),(-25.6,2.17)]:
    cylinder('Recessed brass ceiling trim',(x,y,2.765),.078,.026,BRASS,DECOR,32)
    cylinder('Ceiling soft light lens',(x,y,2.746),.060,.017,GLOW,DECOR,32)
    lights.append({'name':'Spa downlight','type':'Spot','blenderMetres':[x,y,2.51],
                   'unrealCm':[x*100,-y*100,251],'unrealRotation':[-90,0,0],
                   'colorRGB':[1,.84,.64],'intensityLumens':430,'attenuationMetres':4.0,'outerConeDegrees':70})

# North composition: pale textile art, sage relief arches, thin brass frame.
ax,ay,az=-27.62,3.268,1.89
box('North art linen ground',(ax,ay,az),(2.14,.026,.74),LINEN,DECOR,.008)
for x in (ax-1.09,ax+1.09):box('Art vertical brass frame',(x,ay-.015,az),(.017,.018,.79),BRASS,DECOR,.003)
for z in (az-.388,az+.388):box('Art horizontal brass frame',(ax,ay-.015,z),(2.19,.018,.017),BRASS,DECOR,.003)
for x,z,r,color in [(ax-.55,az-.06,.23,SAGE),(ax+.05,az+.02,.30,ART),(ax+.64,az-.03,.19,SAGE)]:
    disc=cylinder('Quiet botanical art medallion',(x,ay-.024,z),r,.006,color,DECOR,48,'Y')
    box('Art linen overlap',(x+r*.38,ay-.032,z-r*.45),(r*.9,.008,r*.78),LINEN,DECOR,.006)
text('North art lettering','EXHALE',(ax,ay-.034,az-.285),.072,BRASS,DECOR)

# A clean bronze wordmark above the tub, facing the entrance.
box('Spa sign walnut plaque',(-30.005,-4.66,1.955),(.047,1.57,.52),WOOD,DECOR,.025)
text('Soak wall wordmark','SOAK',(-29.977,-4.66,1.94),.245,BRASS,DECOR,(math.pi/2,0,math.pi/2))
text('Spa plaque small line','STAY AWHILE',(-29.975,-4.66,1.805),.053,LINEN,DECOR,(math.pi/2,0,math.pi/2))


def export(objects,name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    if len(objects)>1:bpy.ops.object.join()
    o=bpy.context.object;o.name=name
    path=os.path.join(OUT,name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=path,use_selection=True,apply_unit_scale=True,
        axis_forward='-Z',axis_up='Y',bake_space_transform=False)
    verts=[o.matrix_world@v.co for v in o.data.vertices]
    bounds=[[min(v[k] for v in verts) for k in range(3)],[max(v[k] for v in verts) for k in range(3)]]
    row={'name':name,'fbx':path,'triangles':sum(len(p.vertices)-2 for p in o.data.polygons),
         'vertices':len(o.data.vertices),'materialSlots':[m.name for m in o.data.materials],
         'boundsMetres':bounds,'visualCollision':'NoCollision' if name.startswith('SM_') else 'BlockAll complex-as-simple'}
    print('SPA_EXPORTED',json.dumps(row))
    return row


manifest={'coordinateSystem':'world Blender metres -> Unreal centimetres [100x,-100y,100z]',
    'houseScaleAlreadyBaked':True,'floorZ':FLOOR,'ceilingZ':2.78,'ceilingTopZ':2.83,
    'roomBoundsMetres':{'min':[-30.10,-7.80,FLOOR],'max':[-23.75,3.30,2.78]},
    'entry':{'eastWallX':-23.76,'gapY':[-5.85,-4.14],'headerZ':2.10},
    'hotTub':{'centerMetres':[-27.95,-4.25],'exteriorSizeMetres':[3.80,3.30],
              'rimZ':.88,'innerOpeningSizeMetres':[2.95,2.50],'innerCornerRadiusMetres':.22,
              'basinFloorZ':.245,'waterZ':.73,'waterUnrealCm':[-2795,425,73],
              'waterSizeCm':[295,250],'waterCornerRadiusCm':22,'waterDepthCm':48.5,
              'steps':step_rows,'waterMeshExported':False},
    'walkways':{'eastOfTubClearWidthMetres':2.24,'eastWalkwayX':[-26.05,-23.81],
                'stepsLocated':'south-east corner, leaving east entry and long room axis clear'},
    'materials':{m.name:{'diffusePreviewRGB':list(m.diffuse_color[:3]),'uvUnits':'metres planar projection',
                       'requiresTwoSided':m==LEAF} for m in materials.values()},
    'nativeLightAnchors':lights,'pieces':pieces,'exports':[]}
for name,objects in list(groups.items()):manifest['exports'].append(export(objects,name))
manifest['exports'].append(export(colliders,'COL_SpaRoom'))
bpy.data.objects['COL_SpaRoom'].hide_render=True
bpy.data.objects['COL_SpaRoom'].display_type='WIRE'
manifest['visualTriangleCount']=sum(r['triangles'] for r in manifest['exports'] if r['name'].startswith('SM_'))
with open(os.path.join(ROOT,'layout.json'),'w') as f:json.dump(manifest,f,indent=2)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT,'SpaRoom_source.blend'))
print('SPA_BUILD_COMPLETE',manifest['visualTriangleCount'],'visual triangles')
