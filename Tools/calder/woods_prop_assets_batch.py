"""Owned instancing-ready fern/stump materials and conventional distance LODs.

Requires the read-only inspection receipt. Never modifies acquired originals or
loads/saves a map. Near geometry, texture maps, local pivot and bounds stay exact.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
A = unreal.EditorAssetLibrary
E = unreal.MaterialEditingLibrary
S = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
DEST = '/Game/SeniorSendOff/Calder/Woods'
inspection = json.loads((ROOT/'Saved/CalderWork/woods_prop_material_inspection.json').read_text())
for name, expected in inspection['source_sha256'].items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == expected, name
rows = []
for label, source_mesh, screens, fractions in [
    ('Fern', '/Game/Environment/Megascans/BeechFern_vmkpdbeia/Meshes/SM_BeechFern_VarA',
     [1., .15, .045], [1., .25, .05]),
    ('Stump', '/Game/Environment/Megascans/RottenTreeStump_whnpaed/Meshes/SM_RottenTreeStump',
     [1., .15, .045], [1., .20, .05]),
]:
    donor = unreal.load_asset(source_mesh)
    assert donor and donor.get_num_lods() == 1
    source_slots = list(donor.get_editor_property('static_materials'))
    assert len(source_slots) == 1 and isinstance(source_slots[0].material_interface, unreal.Material)
    parent = source_slots[0].material_interface
    material_path = DEST+'/M_CalderWoods'+label
    material = (unreal.load_asset(material_path) if A.does_asset_exist(material_path)
                else A.duplicate_asset(parent.get_path_name().split('.')[0], material_path))
    assert material
    material.set_editor_property('used_with_instanced_static_meshes', True)
    E.recompile_material(material)
    A.save_loaded_asset(material)
    stats = E.get_statistics(material)
    assert stats.num_pixel_shader_instructions > 0 and 0 < stats.num_pixel_texture_samples <= 12
    mesh_path = DEST+'/SM_CalderWoods'+label
    mesh = (unreal.load_asset(mesh_path) if A.does_asset_exist(mesh_path)
            else A.duplicate_asset(source_mesh, mesh_path))
    assert mesh
    mesh.set_material(0, material)
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.enabled = False
    S.set_nanite_settings(mesh, nanite, False)
    options = unreal.StaticMeshReductionOptions()
    reductions = []
    for fraction, screen in zip(fractions, screens):
        value = unreal.StaticMeshReductionSettings()
        value.percent_triangles = fraction
        value.screen_size = screen
        reductions.append(value)
    options.reduction_settings = reductions
    options.auto_compute_lod_screen_size = False
    assert S.set_lods(mesh, options) == 3
    for lod, fraction in enumerate(fractions):
        native = S.get_lod_reduction_settings(mesh, lod)
        native.percent_vertices = fraction
        native.silhouette_importance = unreal.MeshFeatureImportance.HIGHEST
        S.set_lod_reduction_settings(mesh, lod, native)
    S.remove_collisions(mesh)
    mesh.set_editor_property('generate_mesh_distance_field', False)
    A.save_loaded_asset(mesh)
    counts = [mesh.get_num_triangles(lod) for lod in range(3)]
    assert counts[0] == donor.get_num_triangles(0) and counts[0] > counts[1] > counts[2] > 0
    old, new = donor.get_bounds(), mesh.get_bounds()
    error = max(abs(a-b) for a, b in zip(
        [old.origin.x, old.origin.y, old.origin.z, old.box_extent.x, old.box_extent.y, old.box_extent.z],
        [new.origin.x, new.origin.y, new.origin.z, new.box_extent.x, new.box_extent.y, new.box_extent.z]))
    assert error < .001, (label, error)
    row = {'label': label, 'source': donor.get_path_name(), 'asset': mesh.get_path_name(),
           'material': material.get_path_name(), 'triangles': counts, 'screen_sizes': screens,
           'bounds_error_cm': error, 'nanite': False, 'collision': False,
           'instanced_mesh_usage': material.get_editor_property('used_with_instanced_static_meshes'),
           'pixel_instructions': stats.num_pixel_shader_instructions, 'pixel_samples': stats.num_pixel_texture_samples}
    rows.append(row)
    unreal.log('CALDER_WOODS_PROP_ASSET '+json.dumps(row))
for name, expected in inspection['source_sha256'].items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == expected, name
out = ROOT/'SourceArt/CalderLandscape/phase2/woods_props.json'
out.unlink(missing_ok=True)
out.write_text(json.dumps({'scope': 'Owned instancing repair and LOD derivatives; native appearance validation separate',
                           'source_sha256': inspection['source_sha256'], 'source_assets_unchanged': True,
                           'props': rows}, indent=2)+'\n')
unreal.log('CALDER_WOODS_PROP_BATCH PASS props=2 materials=2 sourceAssetsUnchanged=1')
