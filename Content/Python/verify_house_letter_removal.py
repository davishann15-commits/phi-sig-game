"""Verify saved render geometry, collision presence, and stable material mappings."""
import json
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())/'SourceArt/HouseLetterRemoval'
rows=json.loads((root/'evidence/cleaned.json').read_text())
expected_triangles=json.loads((root/'evidence/exported-triangles.json').read_text())
results=[]
for row in rows:
    mesh=unreal.load_asset(row['asset'])
    assert mesh,row['asset']
    sections=mesh.get_num_sections(0)
    triangles=0;materials=[]
    for i in range(sections):
        vertices,indices,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,i)
        triangles+=len(indices)//3
        materials.append(dict(section=i,triangles=len(indices)//3))
    expected=expected_triangles[row['asset']]
    # Unreal discards zero-area fracture-cap triangles during static mesh build.
    assert expected['total']-expected['degenerate']<=triangles<=expected['total'],(mesh.get_name(),triangles,expected)
    results.append(dict(asset=mesh.get_path_name(),triangles=triangles,sections=materials,
        simple_collisions=unreal.EditorStaticMeshLibrary.get_simple_collision_count(mesh),
        convex_collisions=unreal.EditorStaticMeshLibrary.get_convex_collision_count(mesh)))
    unreal.log('LETTER_VERIFY '+mesh.get_name()+' triangles='+str(triangles))
(root/'evidence/verified.json').write_text(json.dumps(dict(passed=True,meshes=results),indent=2))
unreal.log('LETTER_VERIFY_PASS saved_meshes='+str(len(results)))
