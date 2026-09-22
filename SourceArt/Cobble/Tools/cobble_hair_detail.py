"""Reference-guided hair silhouettes and exportable, head-weighted strand meshes."""

import math
import random
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree


_STYLES = {
    1: dict(color=(.027, .020, .015), lift=.014, nape=.035, side=1.055, count=250, wave=.0045, front=.034),
    2: dict(color=(.050, .027, .014), lift=.006, nape=.002, side=1.015, count=105, wave=.0030, front=.037),
    3: dict(color=(.073, .029, .013), lift=.009, nape=.022, side=1.045, count=120, wave=.0057, front=.024),
    4: dict(color=(.021, .010, .006), lift=.011, nape=-.007, side=.96, count=100, wave=.0020, front=.067),
    5: dict(color=(.029, .017, .011), lift=.010, nape=.012, side=1.065, count=112, wave=.0020, front=.023),
    6: dict(color=(.021, .011, .007), lift=.012, nape=.006, side=.98, count=110, wave=.0034, front=.053),
    7: dict(color=(.022, .012, .007), lift=.008, nape=-.005, side=.98, count=102, wave=.0029, front=.058),
    8: dict(color=(.037, .019, .010), lift=.012, nape=.003, side=1.04, count=142, wave=.0070, front=.032),
}


def _clamp(value, low=0., high=1.):
    return max(low, min(high, value))


def _smooth(value):
    value = _clamp(value)
    return value * value * (3 - 2 * value)


def _material(name, color):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.diffuse_color = (*color, 1)
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = .90
    if 'Specular IOR Level' in shader.inputs:
        shader.inputs['Specular IOR Level'].default_value = .08
    if 'Anisotropic IOR Level' in shader.inputs:
        shader.inputs['Anisotropic IOR Level'].default_value = .46
    return mat


def _eye_height(human, rig):
    eyes = [o for o in bpy.data.objects if o.type == 'MESH' and o.parent == rig and
            ('low-poly' in o.name.lower() or 'low_poly' in o.name.lower()) and not o.name.endswith('_Arms')]
    if eyes:
        return sum(v.co.z for v in eyes[0].data.vertices) / len(eyes[0].data.vertices)
    return max(v.co.z for v in human.data.vertices) - .112


class _Groom:
    def __init__(self, index, hair, rig, human):
        self.index, self.hair, self.rig, self.human = index, hair, rig, human
        self.style = _STYLES[index]
        self.rng = random.Random(8423 + index * 113)
        self.eye = _eye_height(human, rig)
        self.top = max(v.co.z for v in human.data.vertices)
        self.scale = _clamp((self.top - self.eye) / .112, .80, 1.20)
        self.center = Vector((0, rig.data.bones['head'].tail_local.y, self.eye + .029 * self.scale))
        self.vertices, self.faces, self.material_ids = [], [], []
        self._reshape_base()
        self.tree = BVHTree.FromPolygons([v.co for v in hair.data.vertices],
                                        [tuple(p.vertices) for p in hair.data.polygons], all_triangles=False)
        self.radii = Vector((max(abs(v.co.x) for v in hair.data.vertices),
                             max(abs(v.co.y - self.center.y) for v in hair.data.vertices),
                             max(v.co.z - self.center.z for v in hair.data.vertices)))
        self.radii.x = max(self.radii.x, .08 * self.scale)
        self.radii.y = max(self.radii.y, .095 * self.scale)
        self.radii.z = max(self.radii.z, .085 * self.scale)

    def _reshape_base(self):
        style, index, s, cy = self.style, self.index, self.scale, self.center.y
        scalp = BVHTree.FromPolygons([v.co for v in self.human.data.vertices],
                                    [tuple(p.vertices) for p in self.human.data.polygons], all_triangles=False)
        for vertex in self.hair.data.vertices:
            v = vertex.co
            above = _smooth((v.z - self.eye - .025 * s) / (.09 * s))
            front = _smooth((cy - v.y) / (.085 * s))
            back = _smooth((v.y - cy) / (.080 * s))
            side = _smooth(abs(v.x) / (.08 * s))
            # Flatten the generic pointed crown and shape the longer/shorter nape.
            v.z -= .007 * s * above
            v.z -= style['nape'] * s * back * (1 - above)
            v.x *= 1 + (style['side'] - 1) * side
            if index in (4, 6):
                v.z += (.012 if index == 4 else .006) * s * above * front
            if index == 5 and front > .25:
                # Central curtains expose a narrow center and fall toward both temples.
                floor = self.eye + s * (.017 + .044 * math.exp(-(v.x / (.017 * s)) ** 2))
                v.z = max(v.z, floor)
            if index in (1, 2, 3, 8) and front > .72 and abs(v.x) < .07 * s:
                v.z -= (.012 if index == 1 else .007) * s * (1 - above)
                v.z += (.004 if index in (1, 2) else .007) * s * math.sin(v.x * 166 / s + .4) * (1 - above)
                if index == 1:
                    v.z = max(v.z, self.eye + s * (.036 + .014 * math.exp(-(v.x / (.021 * s)) ** 2)))
                elif index == 8:
                    # A cropped, broken hairline, not a flat shelf above hanging curls.
                    edge = (.053 + .0035 * math.sin(v.x * 207 / s + .6) +
                            .0025 * math.sin(v.x * 431 / s) - .005 * abs(v.x) / (.07 * s))
                    v.z += .020 * s * (1 - above)
                    v.z = max(v.z, self.eye + edge * s)
            # Small scalp-level unevenness supports natural light response beneath strands.
            v.z += style['wave'] * .27 * s * above * math.sin(v.x * 137 / s + v.y * 59 / s)
            if v.z > self.eye + .035 * s:
                radial = (v - self.center).normalized()
                hit, _, _, distance = scalp.ray_cast(self.center, radial, .35 * s)
                if hit is not None and (v - self.center).length < distance + .006 * s:
                    v = hit + radial * .006 * s
                    vertex.co = v
        for polygon in self.hair.data.polygons:
            polygon.use_smooth = True
        # Imported hair cards use a real opacity atlas. Preserve that edge breakup.
        for material in self.hair.data.materials:
            if not material or not material.use_nodes:
                continue
            shader = next((n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if shader:
                for link in list(shader.inputs['Base Color'].links):
                    material.node_tree.links.remove(link)
                shader.inputs['Base Color'].default_value = (*[c * .32 for c in style['color']], 1)
                shader.inputs['Roughness'].default_value = .90
                if 'Specular IOR Level' in shader.inputs:
                    shader.inputs['Specular IOR Level'].default_value = .08
        self.hair.data.update()

    def surface(self, phi, theta, lift=0.):
        direction = Vector((math.sin(theta) * math.cos(phi),
                            math.sin(theta) * math.sin(phi), math.cos(theta)))
        hit, normal, _, _ = self.tree.ray_cast(self.center, direction, .4 * self.scale)
        if hit is None:
            hit = self.center + Vector((direction.x * self.radii.x,
                                       direction.y * self.radii.y, direction.z * self.radii.z))
        return hit + direction * lift * self.scale

    def strand(self, points, width, material=0, thickness=.22):
        points = [Vector(p) for p in points]
        start = len(self.vertices)
        count = len(points)
        for i, point in enumerate(points):
            t = i / (count - 1)
            tangent = (points[min(i + 1, count - 1)] - points[max(i - 1, 0)]).normalized()
            normal = (point - self.center).normalized()
            across = tangent.cross(normal).normalized()
            if across.length < .1:
                across = tangent.cross(Vector((0, 1, 0))).normalized()
            normal = across.cross(tangent).normalized()
            radius = width * self.scale * (.38 + .62 * math.sin(math.pi * t) ** .50) * (1 - .93 * t ** 4)
            # A narrow diamond cross-section gives light-catching strands with no open backs.
            self.vertices.extend([tuple(point - across * radius),
                                  tuple(point + normal * radius * thickness),
                                  tuple(point + across * radius),
                                  tuple(point - normal * radius * thickness * .3)])
        for i in range(count - 1):
            for j in range(4):
                self.faces.append((start + i * 4 + j, start + i * 4 + (j + 1) % 4,
                                   start + (i + 1) * 4 + (j + 1) % 4, start + (i + 1) * 4 + j))
                self.material_ids.append(material)
        self.faces.extend([(start + 3, start + 2, start + 1, start),
                           tuple(start + (count - 1) * 4 + j for j in range(4))])
        self.material_ids.extend([material, material])

    def surface_locks(self):
        rng, index, style = self.rng, self.index, self.style
        for i in range(style['count']):
            phi = rng.uniform(-math.pi, math.pi)
            theta = rng.uniform(.08, .85)
            frontness = max(0, -math.sin(phi))
            end = min(theta + rng.uniform(.67, 1.00), 1.90 - frontness * .46)
            if index == 5:
                # Crown flow starts beside the center line and opens toward either side.
                phi += .12 * math.cos(phi)
            phase = rng.uniform(0, math.tau)
            paths = []
            for j in range(10):
                t = j / 9
                angle = phi + (.14 if index in (4, 6, 7) else .045) * t
                theta_t = theta + (end - theta) * t
                if index in (3, 8):
                    angle += (.10 if index == 3 else .16) * math.sin(t * math.tau * 1.10 + phase) * math.sin(math.pi * t)
                lift = .0010 + style['lift'] * .35 * math.sin(math.pi * t)
                point = self.surface(angle, theta_t, lift)
                radial = (point - self.center).normalized()
                tangent = radial.cross(Vector((0, 0, 1))).normalized()
                point += tangent * (style['wave'] * self.scale * math.sin(math.tau * t + phase) * math.sin(math.pi * t))
                paths.append(point)
            self.strand(paths, rng.uniform(.0010, .0017), rng.randrange(5))
            # Fine strands follow some larger locks, not free-floating random wires.
            if i % 3 == 0:
                offset = (paths[4] - self.center).normalized() * .0009 * self.scale
                self.strand([p + offset for p in paths], .00040, 5, .36)

    def fringe(self):
        rng, index, s, cy = self.rng, self.index, self.scale, self.center.y
        for i in range(28 if index != 8 else 38):
            x = rng.uniform(-.066, .066) * s
            root = self.surface(-math.pi / 2 + x / (.10 * s), rng.uniform(.50, .85), .002)
            end_x = x + rng.uniform(-.009, .009) * s
            end_z = self.eye + self.style['front'] * s + rng.uniform(-.009, .008) * s
            end_y = self.surface(-math.pi / 2 + x / (.105 * s), 1.18).y - .009 * s
            if index == 1:
                end_x += .014 * s * math.tanh(x / (.025 * s))
                end_z += .010 * s * math.exp(-(x / (.024 * s)) ** 2)
            elif index == 5:
                sign = 1 if x >= 0 else -1
                root = self.surface(-math.pi / 2 + sign * .075, rng.uniform(.35, .68), .003)
                end_x = sign * rng.uniform(.051, .086) * s
                end_z = self.eye + rng.uniform(-.025, .025) * s
            elif index in (4, 6, 7):
                end_x += (.020 if index == 7 else -.016) * s
                end_y += .019 * s
            end = Vector((end_x, end_y, end_z))
            control = root.lerp(end, .53) + Vector((0, -.012 * s, self.style['lift'] * 1.4 * s))
            if index == 1:
                control.x += (.013 if x > 0 else -.010) * s
                control.z += .009 * s
            if index in (4, 6):
                control.z += .014 * s
            paths = []
            phase = rng.uniform(0, math.tau)
            for j in range(11):
                t = j / 10
                point = (1 - t) ** 2 * root + 2 * (1 - t) * t * control + t * t * end
                if index in (1, 2, 3, 8):
                    point.x += math.sin(math.tau * t + phase) * self.style['wave'] * s * math.sin(math.pi * t)
                    point.y += math.cos(math.tau * t + phase) * self.style['wave'] * .55 * s * math.sin(math.pi * t)
                if index == 8:
                    point.x += .006 * s * math.sin(math.tau * 1.15 * t + phase) * math.sin(math.pi * t)
                    point.z += .007 * s * math.sin(math.tau * 1.15 * t) * math.sin(math.pi * t) + .004 * s * t ** 3
                paths.append(point)
            self.strand(paths, rng.uniform(.0018, .0028) if index in (1, 8) else rng.uniform(.0012, .0022), rng.randrange(5))
            self.strand([p + Vector((0, -.0007 * s, .0003 * s)) for p in paths], .00040, 5, .36)

    def curls(self):
        # Irregular partial curls sit in the hair mass; no uniform floating spiral lattice.
        rng, s = self.rng, self.scale
        count = 38 if self.index == 3 else 88
        for i in range(count):
            phi, theta = rng.uniform(-math.pi, math.pi), rng.uniform(.25, 1.77)
            if math.sin(phi) < -.60:
                theta = min(theta, 1.35)
            root = self.surface(phi, theta, .001)
            normal = (root - self.center).normalized()
            along = normal.cross(Vector((0, 0, 1))).normalized()
            cross = normal.cross(along).normalized()
            radius = rng.uniform(.0040, .0090) * s
            twist = rng.uniform(.55, .94) * math.tau
            paths = []
            for j in range(12):
                t = j / 11
                a = twist * t
                paths.append(root + along * (radius * math.sin(a)) + cross * (radius * (1 - math.cos(a))) +
                             normal * (.012 * s * math.sin(math.pi * t) + .003 * s * t))
            self.strand(paths, rng.uniform(.0009, .0015), rng.randrange(5), .26)

    def nape(self):
        if self.index not in (1, 3, 5, 6, 8):
            return
        for i in range(20):
            phi = self.rng.uniform(.12, math.pi - .12)
            start = self.rng.uniform(1.00, 1.30)
            end = self.rng.uniform(1.80, 2.11)
            path = []
            for j in range(9):
                t = j / 8
                point = self.surface(phi + .055 * math.sin(math.pi * t), start + (end - start) * t,
                                     .0015 + .003 * math.sin(math.pi * t))
                point.x += .002 * self.scale * math.sin(math.tau * t + i) * math.sin(math.pi * t)
                path.append(point)
            self.strand(path, self.rng.uniform(.0010, .0016), self.rng.randrange(5))

    def cropped_curls(self):
        """Short, overlapping S-shaped clumps close to the C08 hair mass."""
        rng, s = self.rng, self.scale
        # Each lock covers about 15--30 mm of scalp and returns into the hair mass.
        # No closed ringlets, long forehead tendrils, or floating spiral loops.
        for i in range(300):
            phi = rng.uniform(-math.pi, math.pi)
            theta = rng.uniform(.05, 1.66)
            if math.sin(phi) < -.48:
                theta = min(theta, 1.22)
            length = rng.uniform(.16, .30)
            phase = rng.uniform(0, math.tau)
            path = []
            for j in range(9):
                t = j / 8
                angle = phi + .065 * math.sin(math.tau * t + phase) * math.sin(math.pi * t)
                point = self.surface(angle, theta + length * t,
                                     .0007 + rng.uniform(.004, .006) * math.sin(math.pi * t))
                path.append(point)
            self.strand(path, rng.uniform(.0024, .0042), rng.randrange(5), .50)
            if i % 3 == 0:
                offset = (path[4] - self.center).normalized() * .0005 * s
                self.strand([p + offset for p in path], .00040, 5, .30)
        # Dense compact edge clumps finish just below the base silhouette, above brows.
        for i in range(44):
            x = (-.069 + .138 * (i + rng.uniform(.1, .9)) / 44) * s
            phi = -math.pi / 2 + x / (.100 * s)
            root = self.surface(phi, 1.08, .001)
            edge = (.053 + .0035 * math.sin(x * 207 / s + .6) +
                    .0025 * math.sin(x * 431 / s) - .005 * abs(x) / (.07 * s))
            end = Vector((x + rng.uniform(-.003, .003) * s,
                          self.surface(phi, 1.28).y - .001 * s,
                          self.eye + (edge - rng.uniform(.002, .005)) * s))
            control = root.lerp(end, .52) + Vector((rng.uniform(-.004, .004) * s, -.004 * s, .003 * s))
            path = []
            for j in range(9):
                t = j / 8
                point = (1 - t) ** 2 * root + 2 * (1 - t) * t * control + t * t * end
                point.x += .0025 * s * math.sin(math.tau * t) * math.sin(math.pi * t)
                path.append(point)
            self.strand(path, rng.uniform(.0022, .0036), rng.randrange(5), .50)

    def finish(self):
        name = f'C{self.index:02d}_HairDetail'
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.vertices, [], self.faces)
        mesh.update()
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)
        for i, factor in enumerate((.70, .84, 1., 1.15, 1.28, 1.10)):
            mesh.materials.append(_material(f'{name}_{i}', tuple(c * factor * .34 for c in self.style['color'])))
        for face, material in zip(mesh.polygons, self.material_ids):
            face.material_index = material
            face.use_smooth = True
        obj.parent = self.rig
        obj.matrix_parent_inverse = Matrix.Identity(4)
        obj.matrix_basis = Matrix.Identity(4)
        obj.vertex_groups.new(name='head').add(list(range(len(mesh.vertices))), 1, 'REPLACE')
        modifier = obj.modifiers.new('Skeleton', 'ARMATURE')
        modifier.object = self.rig
        return obj

    def braxton_sweep(self):
        # Follow the parted crown out to each temple, with fine parallel fibers.
        # All geometry is skinned to the head; the nape follows the side reference.
        rng,s=self.rng,self.scale
        for i in range(260):
            side=-1 if i%2 else 1
            row=rng.uniform(0,1)
            root_phi=-math.pi/2+side*rng.uniform(.02,.20)
            root_theta=.18+row*.72
            end_phi=-math.pi/2+side*rng.uniform(.65,1.50)
            end_theta=1.30+row*.25
            points=[]
            for j in range(15):
                t=j/14
                phi=root_phi+(end_phi-root_phi)*math.sin(t*math.pi/2)
                theta=root_theta+(end_theta-root_theta)*t
                point=self.surface(phi,theta,.001+(.007+row*.004)*math.sin(math.pi*t))
                point.z+=.007*s*math.sin(math.pi*t)
                point.y-=.005*s*math.sin(math.pi*t)*(1-row)
                points.append(point)
            self.strand(points,rng.uniform(.0007,.0016),rng.randrange(5),.38)
            for offset in (-.0007,.0007):
                self.strand([p+Vector((offset*s,-.00035*s,.00045*s)) for p in points],.00019,5,.45)
        self.nape()
        # Small irregular locks soften the front silhouette without hanging wires.
        for i in range(120):
            x=(-.065+i*.13/119)*s
            phi=-math.pi/2+x/(.11*s)
            root=self.surface(phi,.76,.003)
            end=self.surface(phi+( .12 if x>0 else -.12),1.52,.002)
            end.z=self.eye+s*(.038+.014*math.exp(-(x/(.021*s))**2))
            nearest=self.tree.find_nearest(end)
            if nearest[0] is not None:
                end=nearest[0]+nearest[1]*.0012*s
            control=root.lerp(end,.5)+Vector((.012*s*(1 if x>0 else -1),-.009*s,.010*s))
            points=[(1-t)**2*root+2*t*(1-t)*control+t*t*end for t in [j/12 for j in range(13)]]
            for j, point in enumerate(points):
                nearest=self.tree.find_nearest(point)
                if nearest[0] is not None:
                    points[j]=nearest[0]+nearest[1]*(.0014+.001*math.sin(math.pi*j/12))*s
            self.strand(points,rng.uniform(.0005,.001),rng.randrange(5),.30)


def refine_hair(index, hair, rig, human, destination):
    """Modify the fitted hair base and return additional real, skinned detail meshes.

    Call once after baking the modeling targets and before exporting. This supersedes
    the old builder curls() call for characters 3 and 8. No reference image is modified.
    """
    if index not in _STYLES:
        raise ValueError('Character index must be 1..8')
    if hair.get('cobble_hair_refined'):
        raise RuntimeError('Hair refinement has already been applied to this object')
    groom = _Groom(index, hair, rig, human)
    if index == 1:
        groom.braxton_sweep()
    elif index == 8:
        groom.cropped_curls()
    else:
        groom.surface_locks()
        groom.fringe()
        groom.nape()
        if index == 3:
            groom.curls()
    detail = groom.finish()
    hair['cobble_hair_refined'] = True
    print('COBBLE_HAIR_REFINED', index, len(detail.data.vertices), 'vertices',
          sum(len(p.vertices) - 2 for p in detail.data.polygons), 'triangles')
    return [detail]
