import bpy
import math
from mathutils import Vector
from pathlib import Path

OUT = Path('/Users/Stewart/Projects/Senior sendoff/outputs/house-mask')
OUT.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

def material(name, color, roughness, metallic=.05):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Roughness'].default_value = roughness
    bsdf.inputs['Metallic'].default_value = metallic
    return mat

oxblood = material('Mask_Oxblood_Leather', (.052, .012, .016), .46)
black = material('Mask_Charcoal_Leather', (.006, .004, .005), .59)
red = material('Mask_Worn_Red_Highlights', (.095, .020, .026), .50)
edge = material('Mask_Dark_Seams', (.003, .002, .003), .65)
metal = material('Mask_Tarnished_Brass', (.115, .088, .054), .38, .72)
scar = material('Mask_Scarred_Leather', (.15, .037, .039), .7)

def smooth(obj):
    for face in obj.data.polygons:
        face.use_smooth = True
    return obj

def apply_modifier(obj, modifier):
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=modifier.name)

def ellipsoid(name, location, scale, mat):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    return smooth(obj)

def curve(name, points, radius, mat):
    data = bpy.data.curves.new(name, 'CURVE')
    data.dimensions = '3D'
    data.resolution_u = 32
    data.bevel_depth = radius
    data.bevel_resolution = 3
    spline = data.splines.new('BEZIER')
    spline.bezier_points.add(len(points)-1)
    for point, xyz in zip(spline.bezier_points, points):
        point.co = xyz
        point.handle_left_type = 'AUTO'
        point.handle_right_type = 'AUTO'
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(mat)
    return obj

def leather_front_y(x, z):
    width = next((w for row_z, w in rows if abs(row_z-z) < .02), .124)
    t = min(abs(x)/width, .99)
    return -.026-.052*math.sqrt(1-t*t)-.003

# A broad leather face plate with a flatter brow and tapered lower cheeks.
# The front bends around the face; the back remains open around the wearer's eyes.
rows = [(-.082,.088),(-.071,.110),(-.052,.121),(-.028,.128),
        (0,.132),(.030,.131),(.058,.124),(.084,.112),(.102,.090)]
columns = 32
verts = []
for back in (False, True):
    for z, width in rows:
        for col in range(columns+1):
            t = (col/columns)*2-1
            x = width*t
            front_y = -.026-.052*math.sqrt(max(0,1-t*t))*(.92+.08*math.cos(z*12))
            crown_z = z-.014*abs(t)**1.8 if z > .084 else z
            verts.append((x, .009 if back else front_y, crown_z))
faces = []
stride = columns+1
layer = stride*len(rows)
for row in range(len(rows)-1):
    for col in range(columns):
        a = row*stride+col
        faces.append((a,a+1,a+stride+1,a+stride))
        b = layer+a
        faces.append((b+stride,b+stride+1,b+1,b))
for col in range(columns):
    faces.append((col,layer+col,layer+col+1,col+1))
    a = (len(rows)-1)*stride+col
    faces.append((a+1,layer+a+1,layer+a,a))
for row in range(len(rows)-1):
    a = row*stride
    faces.append((a+stride,layer+a+stride,layer+a,a))
    b = row*stride+columns
    faces.append((b,layer+b,layer+b+stride,b+stride))
geometry = bpy.data.meshes.new('Mask_Face_Geometry')
geometry.from_pydata(verts, [], faces)
geometry.update()
shell = bpy.data.objects.new('Mask_Face_Shell', geometry)
bpy.context.collection.objects.link(shell)
shell.data.materials.append(oxblood)
shell.data.materials.append(black)
for side in (-1, 1):
    bpy.ops.mesh.primitive_cylinder_add(vertices=72, radius=1, depth=1, location=(side*.058, 0, .008))
    cutter = bpy.context.object
    cutter.name = 'Eye_Cutout_Temp'
    cutter.scale = (.039, .019, .24)
    cutter.rotation_euler[0] = math.pi/2
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    modifier = shell.modifiers.new('Open_Eye', 'BOOLEAN')
    modifier.operation = 'DIFFERENCE'
    modifier.solver = 'EXACT'
    modifier.object = cutter
    apply_modifier(shell, modifier)
    bpy.data.objects.remove(cutter, do_unlink=True)

bevel = shell.modifiers.new('Soft_Edges', 'BEVEL')
bevel.width = .0018
bevel.segments = 3
apply_modifier(shell, bevel)
weighted = shell.modifiers.new('Weighted_Normals', 'WEIGHTED_NORMAL')
apply_modifier(shell, weighted)

# The real mask has a dark lower half, not extra cheek plates.
for face in shell.data.polygons:
    center = face.center
    if center.y > -.01 or (center.z < .028 and abs(center.x) > .036):
        face.material_index = 1

for side in (-1, 1):
    points = []
    for i in range(33):
        angle = 2*math.pi*i/32
        points.append((side*.058+.0395*math.cos(angle), -.080, .008+.0195*math.sin(angle)))
    curve('Mask_Eye_Leather_Rim', points, .0018, edge)
    # Hooded, uneven brows make the otherwise open eye holes read as a glare.
    curve('Mask_Heavy_Eye_Hood', [(side*.018,-.081,.039), (side*.043,-.083,.035),
          (side*.073,-.078,.033), (side*.102,-.062,.048)], .0033, black)
    curve('Mask_Brow_Wear', [(side*.018,-.082,.056), (side*.054,-.077,.064),
                              (side*.093,-.063,.057)], .0010, red)
    # Asymmetric cut leather panel, with hand-sewn closures below each eye.
    cheek = [(side*.111,leather_front_y(side*.111,-.006),-.006),
             (side*.103,leather_front_y(side*.103,-.038),-.038),
             (side*.080,leather_front_y(side*.080,-.066),-.066),
             (side*.053,leather_front_y(side*.053,-.076),-.076)]
    curve('Mask_Cheek_Join', cheek, .0015, edge)
    for n in range(5):
        x = side*(.106-.009*n)
        z = -.019-.012*n
        y = leather_front_y(x,z)-.002
        curve('Mask_Hand_Stitch', [(x-side*.005,y,z-.003),
              (x+side*.005,y,z+.003)], .0010, metal)
    # Fasteners at the brow and strap roots.
    for x,z in ((.109,.061),(.117,-.024)):
        ellipsoid('Mask_Rivet_Shadow', (side*x,leather_front_y(side*x,z)-.002,z),
                  (.0044,.0016,.0044), edge)
        ellipsoid('Mask_Tarnished_Rivet', (side*x,leather_front_y(side*x,z)-.004,z),
                  (.0027,.0015,.0027), metal)

# A crooked tear across the left brow, closed with a few crude stitches.
curve('Mask_Brow_Tear', [(-.088,-.068,.078),(-.074,-.074,.057),
      (-.062,-.079,.043)], .0017, edge)
for i in range(3):
    x = -.080+i*.008
    z = .068-i*.010
    curve('Mask_Brow_Stitch', [(x-.005,-.081,z-.002),
          (x+.004,-.081,z+.004)], .0011, metal)

# A tapered curved beak, deep enough to read from the side in the house.
stations = [
    (-.020, -.054, .049, .034),
    (-.060, -.082, .046, .036),
    (-.115, -.116, .038, .032),
    (-.185, -.155, .030, .025),
    (-.255, -.192, .023, .019),
    (-.315, -.207, .014, .014),
    (-.334, -.207, .006, .008),
]
verts = []
faces = []
segments = 24
for z, cy, rx, ry in stations:
    for i in range(segments):
        angle = 2*math.pi*i/segments
        verts.append((rx*math.cos(angle), cy+ry*math.sin(angle), z))
for row in range(len(stations)-1):
    for i in range(segments):
        a = row*segments+i
        b = row*segments+(i+1)%segments
        faces.append((a,b,b+segments,a+segments))
faces.append(tuple(reversed(tuple(range(segments)))))
faces.append(tuple((len(stations)-1)*segments+i for i in range(segments)))
mesh = bpy.data.meshes.new('Mask_Beak_Geometry')
mesh.from_pydata(verts, [], faces)
mesh.update()
beak = bpy.data.objects.new('Mask_Long_Beak', mesh)
bpy.context.collection.objects.link(beak)
beak.data.materials.append(oxblood)
smooth(beak)
subsurf = beak.modifiers.new('Soft_Beak_Profile', 'SUBSURF')
subsurf.levels = 2
apply_modifier(beak, subsurf)

curve('Mask_Center_Seam', [(0,-.082,.074), (0,-.095,.019), (0,-.112,-.072),
                           (0,-.151,-.174), (0,-.194,-.274), (0,-.216,-.322)], .0015, red)
for side in (-1, 1):
    curve('Mask_Beak_Wear', [(side*.024,-.108,-.090), (side*.022,-.145,-.157),
                             (side*.015,-.186,-.244), (side*.008,-.216,-.311)], .0011, red)
    # Recessed breathing slits run down the taper, with small oxidized rims.
    for z,x,y in ((-.132,.031,-.144),(-.165,.027,-.168),(-.199,.023,-.188)):
        curve('Mask_Beak_Vent', [(side*(x-.004),y+.001,z-.004),
              (side*(x+.004),y+.001,z+.004)], .0010, edge)
        curve('Mask_Vent_Edge', [(side*(x-.005),y,z-.005),
              (side*(x+.002),y,z+.002)], .00045, metal)
    curve('Mask_Black_Strap', [(side*.116,.018,.013), (side*.142,.042,.001),
                               (side*.178,.073,.002), (side*.188,.112,.008)], .0042, black)
    ellipsoid('Mask_Strap_Buckle', (side*.143,.044,.001),
              (.006,.003,.008), metal)

# Uneven, shallow scrapes keep the red leather worn without covering the face.
for points in [
    [(-.034,-.110,-.084),(-.030,-.130,-.111),(-.025,-.144,-.135)],
    [(.015,-.158,-.174),(.020,-.170,-.190),(.017,-.184,-.211)],
    [(-.012,-.200,-.268),(-.008,-.207,-.286),(-.011,-.212,-.304)],
    [(.072,-.071,.079),(.081,-.069,.071),(.092,-.061,.067)],
]:
    curve('Mask_Leather_Scar', points, .0008, scar)

# Convert curves before FBX export so the pickup and equipped prop are identical.
for obj in list(bpy.context.scene.objects):
    if obj.type == 'CURVE':
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.convert(target='MESH')

model = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
bpy.ops.object.select_all(action='DESELECT')
for obj in model: obj.select_set(True)
bpy.context.view_layer.objects.active = shell
bpy.ops.object.join()
shell = bpy.context.object
shell.name = 'SM_HouseOxbloodMask'

# Unreal's Interchange FBX importer applies the metre-to-centimetre conversion.
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'HouseOxbloodMask.blend'))
bpy.ops.export_scene.fbx(filepath=str(OUT/'SM_HouseOxbloodMask.fbx'),
                         use_selection=True, global_scale=1,
                         apply_unit_scale=True, axis_forward='-Z', axis_up='Y',
                         add_leaf_bones=False, bake_anim=False)

# Simple turntable render for visual review without importing the game project.
bpy.ops.mesh.primitive_plane_add(size=3, location=(0, .1, -.36))
plane = bpy.context.object
plane.name = 'Preview_Ground'
plane.data.materials.append(material('Preview_Backdrop', (.23,.22,.21), .9))
camera_data = bpy.data.cameras.new('Preview_Camera')
camera = bpy.data.objects.new('Preview_Camera', camera_data)
bpy.context.collection.objects.link(camera)
camera.location = (0, -.8, .08)
target = Vector((0, -.08, -.09))
camera.rotation_euler = (target - camera.location).to_track_quat('-Z','Y').to_euler()
camera_data.type = 'ORTHO'
camera_data.ortho_scale = .61
bpy.context.scene.camera = camera
light_data = bpy.data.lights.new('Preview_Softbox','AREA')
light = bpy.data.objects.new('Preview_Softbox',light_data)
bpy.context.collection.objects.link(light)
light.location = (-.35,-.48,.4)
light.data.energy = 28
light.data.shape = 'DISK'
light.data.size = .65
bpy.context.scene.render.engine = 'BLENDER_EEVEE'
bpy.context.scene.render.resolution_x = 900
bpy.context.scene.render.resolution_y = 900
bpy.context.scene.render.resolution_percentage = 100
bpy.context.scene.world.color = (.08,.07,.07)
bpy.context.scene.render.image_settings.file_format = 'PNG'
bpy.context.scene.render.filepath = str(OUT/'MaskPreview.png')
bpy.ops.render.render(write_still=True)
camera.location = (.31, -.72, .12)
camera.rotation_euler = (target - camera.location).to_track_quat('-Z','Y').to_euler()
bpy.context.scene.render.filepath = str(OUT/'MaskPreview-ThreeQuarter.png')
bpy.ops.render.render(write_still=True)

# A clean, transparent profile icon for the in-game equipment button.
plane.hide_render = True
bpy.context.scene.render.film_transparent = True
camera.location = (0, -.8, .08)
camera.rotation_euler = (target - camera.location).to_track_quat('-Z','Y').to_euler()
camera_data.ortho_scale = .56
bpy.context.scene.render.resolution_x = 512
bpy.context.scene.render.resolution_y = 512
bpy.context.scene.render.filepath = str(OUT/'T_HouseMaskIcon.png')
bpy.ops.render.render(write_still=True)
