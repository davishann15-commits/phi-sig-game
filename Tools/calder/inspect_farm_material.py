"""Read-only compile/budget/provenance audit for the existing farm shader inputs.

Use ExecutePythonScript with AllowCommandletRendering and the ordinary editor
closed. Nothing is imported, modified or saved as an Unreal asset.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
parent='Content/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.uasset'
files=[parent]
for name in ['rough_wood','stacked_stone_wall','cracked_concrete','withered_grass']:
    for kind in ['A','N']:
        files.append(f'Content/SeniorSendOff/Calder/Textures/{name}/T_{name}_{kind}.uasset')
before={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in files}
material=unreal.load_asset('/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured')
assert material and material.get_editor_property('used_with_instanced_static_meshes')
stats=unreal.MaterialEditingLibrary.get_statistics(material)
assert stats.num_pixel_shader_instructions>0 and 0<stats.num_pixel_texture_samples<=12
for name,sha in before.items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==sha,name
report={'pixel_instructions':stats.num_pixel_shader_instructions,
        'pixel_samples':stats.num_pixel_texture_samples,'ISM_enabled':True,
        'assets_saved':False,'source_sha256':before,
        'scope':'Existing shader budget/provenance only; native farm/visual acceptance separate'}
out=ROOT/'SourceArt/CalderLandscape/phase2/farm_material_audit.json'
out.unlink(missing_ok=True);out.write_text(json.dumps(report,indent=2)+'\n')
unreal.log(f'CALDER_FARM_MATERIAL PASS instructions={stats.num_pixel_shader_instructions} samples={stats.num_pixel_texture_samples} originalsUnchanged=9')
