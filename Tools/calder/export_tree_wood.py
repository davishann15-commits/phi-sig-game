"""Read CPU buffers from owned structural donors; export non-leaf geometry.
No assets or map are saved. Data supports a separate wood-only reducer.
"""
import hashlib,json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'Saved/CalderWork/tree_wood';OUT.mkdir(exist_ok=True)
plan=json.loads((ROOT/'SourceArt/CalderLandscape/phase2/tree_candidates.json').read_text())
S=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for tree in plan['trees']:
    mesh=unreal.load_asset(tree['structural_asset']);assert mesh and mesh.get_editor_property('allow_cpu_access')
    file=ROOT/'Content'/Path(tree['structural_asset'].split('.')[0].removeprefix('/Game/')).with_suffix('.uasset')
    before=hashlib.sha256(file.read_bytes()).hexdigest();rows=[]
    for section in range(mesh.get_num_sections(0)):
        slot=S.get_lod_material_slot(mesh,0,section);material=mesh.get_material(slot)
        assert material,section
        if 'TwoSided' in material.get_name() or 'Impostor' in material.get_name(): continue
        vertices,triangles,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,section)
        assert len(vertices)==len(normals)==len(uvs) and len(triangles)%3==0
        rows.append({'material':material.get_path_name(),'vertices':[[v.x,v.y,v.z] for v in vertices], 'triangles':list(triangles),'normals':[[v.x,v.y,v.z] for v in normals],'uvs':[[v.x,v.y] for v in uvs]})
        unreal.log('CALDER_TREE_WOOD_SECTION '+tree['label']+' '+material.get_name()+' triangles='+str(len(triangles)//3))
    assert rows and hashlib.sha256(file.read_bytes()).hexdigest()==before
    (OUT/(tree['label']+'.json')).write_text(json.dumps({'label':tree['label'],'source':tree['structural_asset'],'source_sha256':before,'sections':rows},separators=(',',':'))+'\n')
for relative,expected in plan['source_sha256'].items():
    assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected,relative
unreal.log('CALDER_TREE_WOOD_EXPORT PASS sourcesUnchanged=1')
