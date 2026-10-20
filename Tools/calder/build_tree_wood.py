"""Blender background export of owned wood-only geometry, preserving UV0.
Input comes from export_tree_wood.py. No acquired asset is modified.
"""
import hashlib,json
from pathlib import Path
import bpy
ROOT=Path(__file__).resolve().parents[2]
INPUT=ROOT/'Saved/CalderWork/tree_wood'
OUT=ROOT/'SourceArt/CalderLandscape/phase2/tree_wood';OUT.mkdir(exist_ok=True)
report=[]
for label in ['Beech','Maple']:
    source=INPUT/(label+'.json');data=json.loads(source.read_text())
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    vertices=[];normals=[];uvs=[];faces=[];face_materials=[]
    for material_index,section in enumerate(data['sections']):
        offset=len(vertices)
        vertices.extend([(x/100.,-y/100.,z/100.) for x,y,z in section['vertices']])
        normals.extend([(x,-y,z) for x,y,z in section['normals']])
        uvs.extend([(u,1.-v) for u,v in section['uvs']])
        indices=section['triangles']
        for i in range(0,len(indices),3):
            faces.append((offset+indices[i],offset+indices[i+2],offset+indices[i+1]))
            face_materials.append(material_index)
    name='SM_CalderTreeWood_'+label
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
    for section in data['sections']:
        obj.data.materials.append(bpy.data.materials.new(section['material'].split('.')[-1]))
    for polygon,index in zip(mesh.polygons,face_materials):
        polygon.material_index=index;polygon.use_smooth=True
    uv=mesh.uv_layers.new(name='UVMap')
    for loop in mesh.loops: uv.data[loop.index].uv=uvs[loop.vertex_index]
    mesh.normals_split_custom_set_from_vertices(normals)
    obj.select_set(True);bpy.context.view_layer.objects.active=obj
    dest=OUT/(name+'.fbx');dest.unlink(missing_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(dest),use_selection=True,object_types={'MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z')
    canonical=[v for section in data['sections'] for v in section['vertices']]
    row={'label':label,'file':dest.name,'triangles':len(faces),'vertices':len(vertices),'materials':[s['material'] for s in data['sections']], 'bounds_cm':[[min(v[a] for v in canonical) for a in range(3)],[max(v[a] for v in canonical) for a in range(3)]],'export_json_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'fbx_sha256':hashlib.sha256(dest.read_bytes()).hexdigest()}
    report.append(row);print('CALDER_TREE_WOOD_FBX',json.dumps(row))
p=OUT/'wood_build.json';p.unlink(missing_ok=True);p.write_text(json.dumps({'scope':'Owned wood-only derivation; no runtime binding or visual acceptance','trees':report},indent=2)+'\n')
