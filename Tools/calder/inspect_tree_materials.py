"""Read compiled candidate-tree material budgets; no asset or map saves."""
import hashlib
import json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
plan=json.loads((ROOT/'SourceArt/CalderLandscape/phase2/tree_candidates.json').read_text())
rows=[]
for tree in plan['trees']:
    mesh=unreal.load_asset(tree['asset'])
    for slot in mesh.static_materials:
        material=slot.material_interface
        if not material or any(x['asset']==material.get_path_name() for x in rows): continue
        stats=unreal.MaterialEditingLibrary.get_statistics(material)
        row={'asset':material.get_path_name(), 'pixel_instructions':stats.num_pixel_shader_instructions,
             'vertex_instructions':stats.num_vertex_shader_instructions, 'pixel_samples':stats.num_pixel_texture_samples,
             'texture_parameters':[str(k) for k in unreal.MaterialEditingLibrary.get_texture_parameter_names(material)]}
        rows.append(row)
        unreal.log('CALDER_TREE_MATERIAL '+json.dumps(row))
for relative,expected in plan['source_sha256'].items():
    assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected,relative
out=ROOT/'SourceArt/CalderLandscape/phase2/tree_materials.json'
out.unlink(missing_ok=True)
out.write_text(json.dumps({'materials':rows,'source_pack_sha_unchanged':True,'scope':'Read-only acquired-material statistics; no woods binding or budget waiver'},indent=2)+'\n')
unreal.log('CALDER_TREE_MATERIAL_INSPECTION PASS materials='+str(len(rows)))
