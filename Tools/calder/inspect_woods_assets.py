"""Read acquired tree geometry/material budgets without modifying source assets."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
records = []
for species in ['EuropeanBeech', 'NorwayMaple']:
    for form in ['Forest_01', 'Forest_02', 'Field_01', 'Sapling_01']:
        name = 'SM_' + species + '_' + form
        path = '/Game/' + species + '/Geometry/SimpleWind/' + name
        mesh = unreal.load_asset(path)
        if not mesh:
            continue
        bounds = mesh.get_bounds()
        materials = []
        for slot in mesh.get_editor_property('static_materials'):
            material = slot.get_editor_property('material_interface')
            materials.append(material.get_path_name() if material else None)
        record = {
            'source': mesh.get_path_name(),
            'lods': mesh.get_num_lods(),
            'triangles': [mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
            'extent': [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
            'centre': [bounds.origin.x, bounds.origin.y, bounds.origin.z],
            'materials': materials,
            'nanite': mesh.get_editor_property('nanite_settings').enabled,
        }
        records.append(record)
        unreal.log('CALDER_TREE_DONOR ' + json.dumps(record))
out = root / 'SourceArt/CalderLandscape/phase2/tree_donors.json'
out.unlink(missing_ok=True)
out.write_text(json.dumps(records, indent=2))
unreal.log('CALDER_TREE_DONOR_INSPECTION PASS count=' + str(len(records)))
