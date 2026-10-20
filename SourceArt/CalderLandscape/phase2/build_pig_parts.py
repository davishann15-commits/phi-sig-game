"""Bake acquired pig anatomy/UVs into static parts with native joint lengths.

No skeletal mesh is used at runtime. Source FBX and Fab textures are read-only.
Faces retain their UVs/material slots, with closed inset collars at joints.
The physical capsule, AI, room and locomotion are separate from this visual rig.
"""
import json
import heapq
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "pig_parts"
OUT.mkdir(exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(ROOT / "pig.fbx"))
source = next(o for o in bpy.context.scene.objects if o.type == "MESH" and o.name.endswith("LOD0"))
armature = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
source_world = [source.matrix_world @ v.co for v in source.data.vertices]
native_min = min(-p.y*85 for p in source_world)
native_max = max(-p.y*85 for p in source_world)
# Match the original animal's longitudinal envelope inside the unchanged room
# and weapon box. Preserve the donor anatomy with one continuous affine fit.
x_fit = (85+69.5)/(native_max-native_min)
x_shift = -69.5-native_min*x_fit


def base(point):
    return Vector((-point.y * 85.0*x_fit+x_shift, point.x * 85.0, point.z * 90.0 + .678445))


def bone(name):
    return base(armature.matrix_world @ armature.data.bones[name].head_local)


points = [base(source.matrix_world @ v.co) for v in source.data.vertices]
groups = {g.index: g.name for g in source.vertex_groups}
head_groups = {"PIG_ Neck", "PIG_ Neck1", "PIG_ Head", "PIG_ Queue de cheval 1", "Pig_Nose"}
leg_groups = {}
hip_names, knee_names = [], []
for i, (side, front) in enumerate([("R", True), ("L", True), ("R", False), ("L", False)]):
    prefix = "PIG_ " + side + " "
    upper = "UpperArm" if front else "Thigh"
    lower = ["Forearm", "Hand", "Finger0"] if front else ["Calf", "HorseLink", "Foot"]
    hip_names.append(prefix + upper)
    knee_names.append(prefix + lower[0])
    leg_groups[prefix + upper] = "Upper" + str(i)
    for name in lower:
        leg_groups[prefix + name] = "Lower" + str(i)


def part_for_group(name):
    return "Head" if name in head_groups else leg_groups.get(name, "Body")


weights = []
for vertex in source.data.vertices:
    totals = {}
    for g in vertex.groups:
        part = part_for_group(groups[g.group])
        totals[part] = totals.get(part, 0.0) + g.weight
    total = sum(totals.values())
    weights.append({k: v / total for k, v in totals.items()} if total else {"Body": 1.0})


def affine(anchor, target, direction, desired):
    rotation = direction.rotation_difference(desired)
    scale = desired.length / direction.length
    return lambda p: target + rotation @ (p - anchor) * scale


neck = bone("PIG_ Neck")
pivots = {"Body": Vector(), "Head": bone("PIG_ Head")}
rotations, lengths, toes, bends, sole_offsets = {}, {}, {}, {}, {}
for i in range(4):
    hip, knee = bone(hip_names[i]), bone(knee_names[i])
    limb_points = [p for p, w in zip(points, weights) if w.get("Lower" + str(i), 0) > .5]
    low = min(p.z for p in limb_points)
    soles = [p for p in limb_points if p.z < low + .7]
    toe = sum(soles, Vector()) / len(soles)
    for name, anchor, direction in [("Upper" + str(i), hip, knee - hip), ("Lower" + str(i), knee, toe - knee)]:
        pivots[name] = anchor
        rotations[name] = direction.rotation_difference(Vector((0, 0, -1)))
        lengths[name] = direction.length
    toes[i] = toe
    ray = (toe - hip).normalized()
    bends[i] = ((knee - hip) - ray * (knee - hip).dot(ray)).normalized()

canonical = points
faces = {name: set() for name in ["Body", "Head", "LeftEar", "RightEar"] + list(leg_groups.values())}
face_owner = []
adjacent_faces = [[] for _ in points]
for poly in source.data.polygons:
    for vi in poly.vertices:adjacent_faces[vi].append(poly.index)
    totals = {}
    for vi in poly.vertices:
        for part, weight in weights[vi].items():
            totals[part] = totals.get(part, 0.0) + weight
    owner = max(totals, key=totals.get)
    centre = sum((points[i] for i in poly.vertices), Vector()) / len(poly.vertices)
    if owner == "Head" and centre.x > 48 and centre.z > 66 and abs(centre.y) > 10:
        owner = "RightEar" if centre.y > 0 else "LeftEar"
    faces[owner].add(poly.index)
    face_owner.append(owner)

for ear in ["LeftEar", "RightEar"]:
    indices = {v for fi in faces[ear] for v in source.data.polygons[fi].vertices}
    if len(indices) < 12:
        raise RuntimeError("Ear segmentation has too few vertices: " + ear)
    bottom = min(canonical[i].z for i in indices)
    root = [canonical[i] for i in indices if canonical[i].z < bottom + 2]
    pivots[ear] = sum(root, Vector()) / len(root)

report = {"source": "/Game/AnimalVarietyPack/Pig/Meshes/SK_Pig", "runtime": "rigid static parts", "parts": {}}
print("NATIVE_PIG centre belly", min(p.z for p in points if abs(p.x) < 20 and abs(p.y) < 10))
original_uv = source.data.uv_layers.active.data
skin = {p.index for p in source.data.polygons if p.material_index == 0}
ears = faces["LeftEar"] | faces["RightEar"]
faces["Donor"] = skin
pivots["Donor"] = Vector()


def clip(bm, at, normal, positive):
    result = bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
                                   dist=.00001, plane_co=at, plane_no=normal,
                                   clear_inner=positive, clear_outer=not positive)
    edges = [e for e in result["geom_cut"] if isinstance(e, bmesh.types.BMEdge) and e.is_boundary]
    if edges:
        bmesh.ops.holes_fill(bm, edges=edges, sides=0)


for part in faces:
    # Skin-weight ownership extends a rear thigh up over the back. A rigid
    # thigh must instead finish inside the torso at its actual articulation.
    # Cut closed surfaces at those joints, with short inset overlap collars.
    selected = (faces[part] if "Ear" in part else skin - ears) & skin
    if part == "Donor":
        selected = skin
    elif part.startswith("Upper") or part.startswith("Lower"):
        selected &= faces["Upper" + part[-1]] | faces["Lower" + part[-1]]
    indices = sorted({vi for fi in selected for vi in source.data.polygons[fi].vertices})
    remap = {vi: i for i, vi in enumerate(indices)}
    polygons = [source.data.polygons[i] for i in sorted(selected)]
    mesh = bpy.data.meshes.new("CalderPig_" + part)
    mesh.from_pydata([canonical[i] for i in indices], [], [[remap[i] for i in p.vertices] for p in polygons])
    mesh.update()
    uv = mesh.uv_layers.new(name="UVMap")
    for material in source.data.materials:
        mesh.materials.append(material)
    for new, old in zip(mesh.polygons, polygons):
        new.material_index = old.material_index
        new.use_smooth = True
        for new_loop, old_loop in zip(new.loop_indices, old.loop_indices):
            uv.data[new_loop].uv = original_uv[old_loop].uv
    bm = bmesh.new()
    bm.from_mesh(mesh)
    owner_layer = bm.faces.layers.int.new("SourceLimb")
    for face, polygon in zip(bm.faces, polygons):
        name = face_owner[polygon.index]
        face[owner_layer] = int(name[-1]) + 1 if name.startswith(("Upper", "Lower")) else 0
    # FBX duplicates vertices at UV/normal seams. Close geometric seams before
    # capping the articulation cuts; otherwise a UV island is mistaken for a
    # physical hole and receives a large spurious skin polygon.
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.001)
    if part == "Body":
        clip(bm, (50, 0, neck.z), (1, 0, .15), False)
        bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
                              dist=.00001,plane_co=(0,0,43),plane_no=(0,0,1))
        bmesh.ops.delete(bm,geom=[f for f in bm.faces if f[owner_layer] and f.calc_center_median().z < 43-.00001],context="FACES")
    elif part == "Head":
        clip(bm, (47, 0, neck.z), (1, 0, .15), True)
    elif part.startswith("Upper") or part.startswith("Lower"):
        i = int(part[-1])
        clip(bm, (50, 0, neck.z), (1, 0, .15), False)
        clip(bm, (0, 0, 0), (1, 0, 0), i < 2)
        clip(bm, (0, 0, 0), (0, 1, 0), i % 2 == 1)
        hip, knee, toe = pivots["Upper" + str(i)], pivots["Lower" + str(i)], toes[i]
        for axis, half in [(0, 14), (1, 10)]:
            normal = Vector((0, 0, 0))
            normal[axis] = 1
            low, high = hip.copy(), hip.copy()
            low[axis] = min(hip[axis], knee[axis], toe[axis]) - half
            high[axis] = max(hip[axis], knee[axis], toe[axis]) + half
            clip(bm, low, normal, True)
            clip(bm, high, normal, False)
        if part.startswith("Upper"):
            clip(bm, (0, 0, 44), (0, 0, 1), False)
            clip(bm, (0, 0, knee.z - 2), (0, 0, 1), True)
        else:
            clip(bm, (0, 0, knee.z + 2), (0, 0, 1), False)
    seeds = {i: set() for i in range(5)}
    if part == "Body":
        for vertex in bm.verts:
            p = vertex.co
            if abs(p.x + .15*(p.z-neck.z) - 50) < .01:
                seeds[4].add(vertex)
            elif p.z < 48 and any(e.is_boundary for e in vertex.link_edges):
                i = min(range(4),key=lambda j:(p.x-pivots["Upper"+str(j)].x)**2+(p.y-pivots["Upper"+str(j)].y)**2)
                seeds[i].add(vertex)
    edges = [e for e in bm.edges if e.is_boundary] if part != "Donor" else []
    if edges:
        bmesh.ops.holes_fill(bm, edges=edges, sides=0)
    # Preserve authored skin winding. Recalculating an open ear/eye island can
    # choose its inward side; holes_fill already follows neighbouring winding.
    bm.normal_update()
    if part == "Body":
        bm.verts.ensure_lookup_table()
        bm.verts.index_update()
        colours = bm.loops.layers.float_color.new("JointBlend")
        influences = {v: (0.,0) for v in bm.verts}
        for joint, starts in seeds.items():
            distances = {v:0. for v in starts}
            queue = [(0.,v.index,v) for v in starts]
            heapq.heapify(queue)
            while queue:
                distance, _, v = heapq.heappop(queue)
                if distance != distances[v] or distance >= 22: continue
                weight = 1 - distance/22
                weight = weight*weight*(3-2*weight)
                if weight > influences[v][0]: influences[v] = (weight,joint)
                for edge in v.link_edges:
                    other = edge.other_vert(v)
                    candidate = distance + edge.calc_length()
                    if candidate < 22 and candidate < distances.get(other,1e9):
                        distances[other] = candidate
                        heapq.heappush(queue,(candidate,other.index,other))
        for face in bm.faces:
            for loop in face.loops:
                weight, joint = influences[loop.vert]
                loop[colours] = (weight,float(joint&1),float(bool(joint&2)),float(bool(joint&4)))
        report["joint_blend_vertices"] = sum(w>0 for w,_ in influences.values())
    topology = {"boundary_edges": sum(e.is_boundary for e in bm.edges),
                "nonmanifold_edges": sum(not e.is_manifold for e in bm.edges),
                "volume_cm3": bm.calc_volume(signed=True)}
    bm.to_mesh(mesh)
    bm.free()
    if not mesh.polygons:
        raise RuntimeError("Empty clipped part: " + part)
    for vertex in mesh.vertices:
        p = vertex.co
        if part.startswith("Upper") and p.z > 43:
            p.x = pivots[part].x + (p.x - pivots[part].x) * .93
            p.y = pivots[part].y + (p.y - pivots[part].y) * .93
        elif part.startswith("Lower") and p.z > pivots[part].z - 2:
            p.x = pivots[part].x + (p.x - pivots[part].x) * .93
            p.y = pivots[part].y + (p.y - pivots[part].y) * .93
        elif part == "Head" and p.x + .15 * (p.z - neck.z) < 50:
            p.y *= .94
            p.z = neck.z + (p.z - neck.z) * .94
        local = p - pivots[part]
        if part in rotations:
            local = rotations[part] @ local
        vertex.co = local / 100
    if part.startswith("Lower"):
        bottom = min(v.co.z for v in mesh.vertices)
        sole = [v.co for v in mesh.vertices if v.co.z < bottom + .002]
        centre = sum(sole, Vector()) / len(sole)
        offset = Vector((0, 0, -lengths[part] / 100)) - centre
        for vertex in mesh.vertices:
            vertex.co += offset
    coordinates = [v.co.copy() for v in mesh.vertices]
    if part.startswith("Lower"):
        sole_offsets[int(part[-1])] = offset * 100
    if part == "Donor":
        front = max(p.x for p in points)
        packed = []
        for p, original in zip(points, weights):
            values = dict(original)
            # The donor's thigh group reaches over the fat barrel. Retain that
            # upper mass on the torso while the lower muscle bends with the
            # procedural limb; extreme sleep poses must not flatten the back.
            for i in range(4):
                key="Upper"+str(i)
                if key not in values:continue
                end=60. if i<2 else 72.
                t=max(0.,min(1.,(end-p.z)/(end-43.)))
                retained=t*t*(3-2*t)
                moved=values[key]*(1-retained)
                values[key]-=moved
                values["Body"]=values.get("Body",0.)+moved
            if p.x > front-2 and abs(p.y)<10:
                values = {"Head":1.}
            elif p.z < 10:
                leg = min(range(4),key=lambda i:(p-toes[i]).length_squared)
                values = {"Lower"+str(leg):1.}
            else:
                vi = len(packed)
                adjoining = adjacent_faces[vi]
                for ear in ["LeftEar","RightEar"]:
                    share = sum(fi in faces[ear] for fi in adjoining) / max(1,len(adjoining))
                    if share:
                        values = {k:w*(1-share) for k,w in values.items()}
                        values[ear] = share
            ids = {"Body":0,"Head":1,"LeftEar":10,"RightEar":11}
            for i in range(4):
                ids["Upper"+str(i)] = 2+i*2
                ids["Lower"+str(i)] = 3+i*2
            row = sorted([(ids[k],v) for k,v in values.items() if v>0],key=lambda a:-a[1])[:4]
            total = sum(w for _,w in row)
            row = [(j,w/total) for j,w in row]
            row += [(0,0.)]*(4-len(row))
            packed.append(row)
        # The uncut donor retains the same native vertices after welding; use
        # nearest authored positions for per-loop UV data at merged UV seams.
        from mathutils.kdtree import KDTree
        tree = KDTree(len(points))
        for i,p in enumerate(points):tree.insert(p,i)
        tree.balance()
        layers = [mesh.uv_layers.new(name="RigidWeights%d"%i) for i in range(4)]
        for loop in mesh.loops:
            p = mesh.vertices[loop.vertex_index].co*100
            _, index, distance = tree.find(p)
            if distance>.01:raise RuntimeError("Donor vertex no longer matches source")
            for influence,layer in enumerate(layers):
                joint,weight = packed[index][influence]
                # UE's FBX importer flips V. This channel stores numeric data.
                layer.data[loop.index].uv = (joint,1-weight)
        report["contact_proof"] = {"snout_head_weight":min(packed[i][0][1] if packed[i][0][0]==1 else 0 for i,p in enumerate(points) if p.x>front-2 and abs(p.y)<10),
                                   "hoof_lower_weight":min(packed[i][0][1] if packed[i][0][0] in [3,5,7,9] else 0 for i,p in enumerate(points) if p.z<10),
                                   "meaning":"Visible snout and sole vertices use the exact rigid part transforms measured by the motion smoke."}
    obj = bpy.data.objects.new("SM_CalderPig_" + part, mesh)
    bpy.context.collection.objects.link(obj)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    # This FBX axis preset imports Blender X as UE X and negates Blender Y.
    # Check installed bounds against canonical bounds to catch axis mistakes.
    for vertex in mesh.vertices:
        x, y, z = vertex.co
        vertex.co = (x, -y, z)
    # The reflection changes handedness. Reverse winding as well so the
    # exported skin remains outward-facing; bounds alone cannot catch this.
    for polygon in mesh.polygons:
        polygon.flip()
    mesh.update()
    destination = OUT / (obj.name + ".fbx")
    destination.unlink(missing_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(destination), use_selection=True, object_types={"MESH"},
                             add_leaf_bones=False, bake_anim=False, axis_forward="-Y", axis_up="Z")
    report["parts"][part] = {"file": destination.name, "pivot_cm": list(pivots[part]), "vertices": len(coordinates),
                              "triangles": sum(len(p.vertices) - 2 for p in mesh.polygons),
                              "topology": topology,
                              "bounds_cm": [[min(v[axis] * 100 for v in coordinates) for axis in range(3)],
                                             [max(v[axis] * 100 for v in coordinates) for axis in range(3)]]}

target = OUT / "parts.json"
target.unlink(missing_ok=True)
target.write_text(json.dumps(report, indent=2))
header = ROOT.parents[2] / "Source/SeniorSendoff/HouseCalderPigData.inl"
left, right = pivots["LeftEar"] - pivots["Head"], pivots["RightEar"] - pivots["Head"]
header.unlink(missing_ok=True)
header.write_text("// Generated by SourceArt/CalderLandscape/phase2/build_pig_parts.py.\n"
                  "namespace HouseCalderPigData {\n"
                  "inline FVector EarRoot(int32 Index) { return Index == 0 ? FVector(%0.6f,%0.6f,%0.6f) : FVector(%0.6f,%0.6f,%0.6f); }\n"
                  "}\n" % (*left, *right))
def cpp(v):
    return "FVector(%0.6f,%0.6f,%0.6f)" % tuple(v)
data = "\nnamespace HouseCalderPigData {\n"
data += "inline FVector HeadRoot() { return " + cpp(pivots["Head"]) + "; }\n"
data += "inline FVector Neck() { return " + cpp(neck) + "; }\n"
front = max(p.x for p in points)
nose = [p for p in points if p.x > front - 2 and abs(p.y) < 10]
data += "inline FVector Snout() { return " + cpp(sum(nose, Vector()) / len(nose) - pivots["Head"]) + "; }\n"
for method, values, typename in [
    ("Hip", [cpp(pivots["Upper" + str(i)]) for i in range(4)], "FVector"),
    ("Bend", [cpp(bends[i]) for i in range(4)], "FVector"),
    ("UpperLength", ["%0.6ff" % lengths["Upper" + str(i)] for i in range(4)], "float"),
    ("LowerLength", ["%0.6ff" % lengths["Lower" + str(i)] for i in range(4)], "float"),
    ("ToUpper", ["FQuat(%0.9f,%0.9f,%0.9f,%0.9f)" % (rotations["Upper"+str(i)].x,rotations["Upper"+str(i)].y,rotations["Upper"+str(i)].z,rotations["Upper"+str(i)].w) for i in range(4)], "FQuat"),
    ("ToLower", ["FQuat(%0.9f,%0.9f,%0.9f,%0.9f)" % (rotations["Lower"+str(i)].x,rotations["Lower"+str(i)].y,rotations["Lower"+str(i)].z,rotations["Lower"+str(i)].w) for i in range(4)], "FQuat"),
    ("Knee", [cpp(pivots["Lower"+str(i)]) for i in range(4)], "FVector"),
    ("SoleOffset", [cpp(sole_offsets[i]) for i in range(4)], "FVector"),
]:
    data += "inline " + typename + " " + method + "(int32 Index) { const " + typename + " Values[] = {" + ",".join(values) + "}; return Values[Index]; }\n"
data += "}\n"
with header.open("a") as f:
    f.write(data)
print("CALDER_PIG_PARTS PASS", target)
