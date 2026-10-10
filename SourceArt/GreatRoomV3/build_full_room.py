"""Rebuild the three great-room furniture groups from IMG_2171..IMG_2175.

Dimensions are real metres. Furniture positions are baked to the current house
layout with its 1.8 horizontal scale. Seat heights retain the house's normal
vertical scale; the upper upholstery is fitted to the photo proportions.
Blender [x,y,z] maps to Unreal [100x,-100y,100z].
"""
import bpy
import bmesh
import collections
import json
import math
import os
import sys
from mathutils import Matrix, Vector

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)
import furniture_geometry as furniture
HORIZONTAL = 1.8
FT = 0.3048
manifest = {'scale': [1.8,1.8,1.0], 'sectionalRealFeet': [14,9],
            'oppositeSofaRealFeet': 6.2,
            'heightFit': {'seatPivotMetres': .52, 'upperUpholsteryMultiplier': 1.4},
            'pieces': [], 'clearedChunks': {}}

def clear():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)

def export(objects, name):
    if not objects:
        raise RuntimeError('Empty export ' + name)
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    if len(objects)>1:bpy.ops.object.join()
    obj=bpy.context.object
    obj.name=name
    bpy.ops.export_scene.fbx(filepath=os.path.join(ROOT,name+'.fbx'),
        use_selection=True, apply_unit_scale=True, axis_forward='-Z',axis_up='Y')
    print('GR3_EXPORT',name,len(obj.data.polygons))

def parts(obj):
    mesh=obj.data; parent=list(range(len(mesh.vertices)))
    def find(i):
        while parent[i]!=i:
            parent[i]=parent[parent[i]]; i=parent[i]
        return i
    for f in mesh.polygons:
        for v in f.vertices[1:]: parent[find(v)]=find(f.vertices[0])
    groups=collections.defaultdict(list)
    for f in mesh.polygons:groups[find(f.vertices[0])].append(f.index)
    for ids in groups.values():
        vs={v for i in ids for v in mesh.polygons[i].vertices}
        coords=[obj.matrix_world@mesh.vertices[v].co for v in vs]
        lo=[min(v[k] for v in coords) for k in range(3)]
        hi=[max(v[k] for v in coords) for k in range(3)]
        yield ids,[(a+b)/2 for a,b in zip(lo,hi)],hi

# Preserve wall fixtures, porch benches, the approved door meshes, and the
# corrected porch bench. Remove all original room seating, low tables and rugs.
for prefix in ('SMF','COLF'):
    for tile in ('m1_m1','m1_p0','p0_m1','p0_p0'):
        clear()
        original=f'{prefix}_Entrance_{tile}_Solid'
        baseline=f'{prefix}_Entrance_p0_m1_BenchClear' if tile=='p0_m1' else original
        bpy.ops.import_scene.fbx(filepath=os.path.join(ROOT,'Baseline',baseline+'.fbx'))
        obj=next(o for o in bpy.context.scene.objects if o.type=='MESH')
        remove=set()
        tower_vertices=set()
        for ids,(x,y,z),hi in parts(obj):
            if prefix=='SMF' and tile=='p0_p0' and 5.35<x<5.81 and -7.06<y<-6.72 and hi[2]<.75:
                names={obj.data.materials[obj.data.polygons[i].material_index].name.split('.')[0] for i in ids}
                if names <= {'MI_SSO_MetalDark','MI_SSO_PaintGrey'}:
                    tower_vertices.update(v for i in ids for v in obj.data.polygons[i].vertices)
            seating=(-9.7<=x<=9.7 and -4.4<=y<=1.2 and hi[2]<1.3)
            # The individual portraits on wall composites start at 1.425m.
            consoles=(-8.3<x<8.3 and -8.45<y<-7.1 and hi[2]<1.42)
            stray_chair=(10.8<abs(x)<12.1 and 3.0<y<4.5 and hi[2]<1.3)
            if seating or consoles or stray_chair:remove.update(ids)
        if tower_vertices:
            delta=obj.matrix_world.inverted().to_3x3()@Vector((.27,-1.256,0))
            for i in tower_vertices:obj.data.vertices[i].co+=delta
            manifest['tuckedHeater']={'centerMetres':[5.85,-8.15,.35], 'vertices':len(tower_vertices)}
        if remove:
            bm=bmesh.new(); bm.from_mesh(obj.data); bm.faces.ensure_lookup_table()
            bmesh.ops.delete(bm,geom=[bm.faces[i] for i in remove],context='FACES')
            bm.to_mesh(obj.data); bm.free(); obj.data.update()
        target=f'{prefix}_GR3_{tile}_Cleared'
        manifest['clearedChunks'][original]={'asset':target if obj.data.polygons else None,
            'removedFaces':len(remove),'remainingFaces':len(obj.data.polygons)}
        if obj.data.polygons:export([obj],target)

clear()
materials={}
def mat(name,color):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);materials[name]=m;return m
BRONZE=mat('BronzeLeather',(.20,.115,.055))
COGNAC=mat('CognacLeather',(.25,.12,.052))
OLIVE=mat('OliveLeather',(.105,.097,.062))
DARK=mat('OxbloodLeather',(.10,.035,.02))
WOOD=mat('Walnut',(.13,.065,.025))
METAL=mat('BlackMetal',(.018,.016,.014))
GREEN=mat('GreenCord',(.05,.075,.045))
CHECKER=mat('CheckerCloth',(.65,.5,.25))
IVORY=mat('IvoryCloth',(.75,.72,.63))
BRASS=mat('Brass',(.55,.39,.12))
WHITE=mat('Ceramic',(.84,.84,.78))
groups=collections.defaultdict(list)
collisions=[]
stitch_materials={
    'BronzeLeather':mat('BronzeStitch',(.094,.052,.027)),
    'CognacLeather':mat('CognacStitch',(.17,.079,.035)),
    'OliveLeather':mat('OliveStitch',(.045,.055,.028)),
    'OxbloodLeather':mat('OxbloodStitch',(.055,.019,.014)),
    'GreenCord':mat('GreenStitch',(.014,.032,.020)),
    'IvoryCloth':mat('IvoryStitch',(.37,.34,.28)),
    'CheckerCloth':mat('CheckerStitch',(.33,.235,.125))}

def box(name,center,size,material,bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1,location=center)
    o=bpy.context.object;o.name=name;o.scale=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    o.data.materials.append(material)
    if bevel:
        b=o.modifiers.new('Rounded edge','BEVEL');b.width=bevel;b.segments=3
        bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=b.name)
    return o

def place(objects,group,name,center,yaw=0,scale=HORIZONTAL):
    bpy.context.view_layer.update()
    transform=Matrix.Translation(Vector((center[0],center[1],0))) @ Matrix.Rotation(math.radians(yaw),4,'Z') @ Matrix.Diagonal((scale,scale,1,1))
    for o in objects:
        o.matrix_world=transform@o.matrix_world
    bpy.context.view_layer.update()
    groups[group].extend(objects)
    bounds=[o.matrix_world@v.co for o in objects for v in o.data.vertices]
    lo=[min(v[k] for v in bounds) for k in range(3)]
    hi=[max(v[k] for v in bounds) for k in range(3)]
    manifest['pieces'].append({'name':name,'center':[center[0],center[1]],'yaw':yaw,
        'boundsMetres':[lo,hi]})
    return transform

def blocker(name,center,size,yaw=0):
    o=box(name,(center[0],center[1],size[2]/2),size,METAL)
    o.rotation_euler.z=math.radians(yaw);collisions.append(o)

def normalize_footprint(objects,width,depth):
    # The soft leather surface swells a few millimetres beyond its cage. Keep
    # the user's measured overall dimensions exact after that soft shaping.
    bpy.context.view_layer.update()
    points=[o.matrix_world@v.co for o in objects for v in o.data.vertices]
    lo=[min(v[k] for v in points) for k in range(2)]
    hi=[max(v[k] for v in points) for k in range(2)]
    cx,cy=[(a+b)/2 for a,b in zip(lo,hi)]
    fix=Matrix.Diagonal((width/(hi[0]-lo[0]),depth/(hi[1]-lo[1]),1,1)) @ Matrix.Translation(Vector((-cx,-cy,0)))
    for o in objects:o.matrix_world=fix@o.matrix_world

def fit_upholstery_height(objects):
    # The architecture is stretched across the floor only. Keep the measured
    # footprint and normal seat height, while giving the backs their taller
    # reference silhouette instead of widening every cushion into a low bench.
    bpy.context.view_layer.update()
    for o in objects:
        inverse=o.matrix_world.inverted()
        for v in o.data.vertices:
            p=o.matrix_world@v.co
            if p.z>.52:p.z=.52+(p.z-.52)*1.4
            v.co=inverse@p
        o.data.update()

def sofa(name,center,width,material,yaw=0,group='SM_GR3_NearSeating',seat_count=3):
    objs=furniture.sofa(name,width=width,depth=.98,height=.94,material=material,wood=WOOD,metal=BRASS,seat_count=seat_count)
    normalize_footprint(objs,width,.98)
    fit_upholstery_height(objs)
    place(objs,group,name,center,yaw)
    blocker(name+' collision',center,(width*HORIZONTAL,.98*HORIZONTAL,1.16),yaw)

# 14ft long, 9ft overall chaise depth: porch-side back, facing the column side.
section_width=14*FT; section_depth=9*FT
section_center=(6.4,-4.82)
sofa('14 foot sectional',section_center,section_width,BRONZE,180,seat_count=5)
# Chaise is at the near-fireplace end (+X); the main sofa supplies its back.
chaise_width=.94; extra=section_depth-.98
chaise_center=(6.4+(section_width/2-chaise_width/2)*HORIZONTAL,
               -4.82+(.98/2+extra/2)*HORIZONTAL)
chaise_parts=furniture.chaise('Sectional soft chaise',width=.94,length=extra,
                            height=.94,material=BRONZE,wood=WOOD,metal=BRASS,side='left')
normalize_footprint(chaise_parts,.94,extra)
fit_upholstery_height(chaise_parts)
place(chaise_parts,'SM_GR3_NearSeating','9 foot chaise',chaise_center,180)
blocker('Chaise collision',chaise_center,(.94*HORIZONTAL,extra*HORIZONTAL,.85))

# Matching measured 6.2ft sofa faces the sectional from the column side.
sofa('Near opposite 6.2 foot sofa',(6.4,2.25),6.2*FT,BRONZE,0)

for name,center,yaw,material in [
    ('Near sofa green pillow',(5.32,2.0),-9,GREEN),
    ('Near sofa checked pillow',(6.60,1.92),14,CHECKER),
    ('Sectional ivory pillow',(7.0,-4.58),12,IVORY),
    ('Sectional green pillow',(3.38,-4.62),-9,GREEN)]:
    ps=furniture.pillow(name,material,size=(.46,.46,.17))
    for o in ps:
        o.rotation_euler.x=math.radians(85)
        o.location.z+=.64
    place(ps,'SM_GR3_NearSeating',name,center,yaw)

# Six inward-facing chairs: four cognac across the room, olive at each end.
middle=(.15,-1.50)
chair_specs=[('Cognac porch left',(-1.0,-3.45),180,COGNAC),
             ('Cognac porch right',(1.30,-3.45),180,COGNAC),
             ('Cognac column left',(-1.0,.45),-5,COGNAC),
             ('Cognac column right',(1.30,.45),5,COGNAC),
             ('Olive near end',(2.65,-1.5),-90,OLIVE),
             ('Olive far end',(-2.35,-1.5),90,OLIVE)]
for name,c,yaw,m in chair_specs:
    objs=furniture.club_chair(name,material=m,wood=WOOD,metal=BRASS,width=.82,depth=.93,height=.94)
    normalize_footprint(objs,.82,.93)
    fit_upholstery_height(objs)
    place(objs,'SM_GR3_Conversation',name,c,yaw)
    blocker(name+' collision',c,(.82*HORIZONTAL,.93*HORIZONTAL,1.16),yaw)
place(furniture.round_table('Middle ring coffee table',WOOD,METAL,diameter=1.02,height=.46),
      'SM_GR3_Conversation','Middle round table',middle)
blocker('Middle table collision',middle,(1.50,1.50,.47))
foot=box('Small olive footstool',(0,0,.28),(.35,.44,.47),OLIVE,.075)
place([foot],'SM_GR3_Conversation','Olive footstool',(.15,-3.15))
blocker('Footstool collision',(.15,-3.15),(.35*HORIZONTAL,.44*HORIZONTAL,.5))

# Far dark modular sofas form a U with an open end toward the far fireplace.
# Both side runs use the user's 6.2ft sofa length; the rear run is wider.
sofa('Far fireplace rear sofa',(-5.05,-1.55),3.65,DARK,-90,'SM_GR3_FarSeating',5)
sofa('Far fireplace porch side sofa',(-7.58,-4.00),6.2*FT,DARK,180,'SM_GR3_FarSeating',3)
sofa('Far fireplace column side sofa',(-7.58,.90),6.2*FT,DARK,0,'SM_GR3_FarSeating',3)

# Two extra tables, low end tables, a black stool, and the door-bay consoles.
place(furniture.round_table('Porch side round table',WOOD,METAL,diameter=1.02,height=.46),
      'SM_GR3_Tables','Second round table',(1.35,-5.35))
blocker('Second table collision',(1.35,-5.35),(1.50,1.50,.47))
for name,c in [('Near opposite sofa end table',(8.73,2.3)),
               ('Far sofa side table',(-8.65,2.45))]:
    place(furniture.end_table(name,WOOD,METAL,width=.53,depth=.53,height=.56),
          'SM_GR3_Tables',name,c)
    blocker(name+' collision',c,(.53*HORIZONTAL,.53*HORIZONTAL,.57))
place(furniture.stool('Tall black stool',METAL),'SM_GR3_Tables','Tall black stool',(-2.2,2.45))
blocker('Stool collision',(-2.2,2.45),(.4*HORIZONTAL,.4*HORIZONTAL,.65))

def lamp(center,group):
    # The real consoles have a slender dark stem and a square off-white shade.
    x,y=center
    objects=[box('Lamp base',(x,y,.79),(.27,.21,.035),METAL,.009),
             box('Lamp stem',(x,y,1.00),(.035,.035,.42),METAL,.005),
             box('Square fabric lampshade',(x,y,1.27),(.42,.32,.32),IVORY,.012)]
    groups[group].extend(objects)

def trophy(center,h=.37):
    x,y=center;z=.77
    objects=[box('Trophy walnut base',(x,y,z+.025),(.17,.14,.05),WOOD,.006),
        box('Trophy column',(x,y,z+h/2),(.025,.025,h-.06),BRASS,.006)]
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=8,radius=1,location=(x,y,z+h))
    top=bpy.context.object;top.name='Trophy cup';top.scale=(.075,.07,.055);top.data.materials.append(BRASS);objects.append(top)
    groups['SM_GR3_Consoles'].extend(objects)

for x in (-6.5,0.0,6.3):
    name='Porch bay walnut console '+str(x)
    place(furniture.console(name,WOOD,METAL,width=1.35,depth=.4,height=.76),
          'SM_GR3_Consoles',name,(x,-8.02))
    blocker(name+' collision',(x,-8.02),(1.35*HORIZONTAL,.4*HORIZONTAL,.78))
    lamp((x,-8.02),'SM_GR3_Consoles')
    trophy((x-.63,-8.02),.46);trophy((x+.66,-8.02),.30)

place(furniture.console('Near fireplace lamp table',WOOD,METAL,width=.75,depth=.40,height=.76),
      'SM_GR3_Consoles','Near fireplace lamp table',(12.0,1.75),90)
blocker('Near lamp console collision',(12.0,1.75),(.75*HORIZONTAL,.4*HORIZONTAL,.78),90)
lamp((12.0,1.75),'SM_GR3_Consoles')

# Restrained table contents visible in the references.
for c in [(.07,-1.4),(1.35,-5.3)]:
    x,y=c
    if y<-4:
        ps=furniture.pillow('Ivory pillow on spare table',IVORY,size=(.50,.40,.12))
        for o in ps:o.location.z+=.53
        place(ps,'SM_GR3_Tables','Table pillow',c,12)
    else:
        groups['SM_GR3_Conversation'].append(box('Small book on coffee table',(x,y,.495),(.27,.19,.04),WOOD,.005))
        bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=.04,depth=.12,location=(x+.32,y-.1,.53))
        cup=bpy.context.object;cup.name='Ceramic cup';cup.data.materials.append(WHITE);groups['SM_GR3_Conversation'].append(cup)

for name,objects in list(groups.items()):
    for o in objects:
        if any(s in o.name.lower() for s in ('piping','seam','welt')):
            for i,m in enumerate(o.data.materials):
                if m and m.name in stitch_materials:o.data.materials[i]=stitch_materials[m.name]
    export(objects,name)
export(collisions,'COL_GR3_Furniture')

# Thin eased rug edges catch contact shading without obstructing the player.
for name,center,size in [('SM_GR3_RoseRug',(6.4,-1.55),(7.5,7.3)),
                         ('SM_GR3_GreenRug',middle,(4.0,4.8)),
                         ('SM_GR3_FarRug',(-7.5,-1.55),(4.7,6.1))]:
    clear()
    rug_material=mat('GreenRug' if 'Green' in name else 'RoseRug',(.5,.4,.3))
    o=box(name,(*center,.008),(*size,.012),rug_material,.004)
    uv=o.data.uv_layers.active
    for face in o.data.polygons:
        for i in face.loop_indices:
            p=o.data.vertices[o.data.loops[i].vertex_index].co
            uv.data[i].uv=(p.x/size[0]+.5,p.y/size[1]+.5)
    export([o],name)
    manifest['pieces'].append({'name':name,'center':list(center),'sizeMetres':list(size)})

with open(os.path.join(ROOT,'layout_manifest.json'),'w') as f:json.dump(manifest,f,indent=2)
print('GR3_FULL_LAYOUT_BUILT',len(manifest['pieces']),'pieces')
