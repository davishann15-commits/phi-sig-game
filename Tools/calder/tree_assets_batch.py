"""Create unbound conventional tree candidates from acquired, read-only donors.

Run with the ordinary editor closed. Only owned Calder/Trees assets are saved;
no map, source tree, shared ExteriorTree mesh or source material is changed.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST = '/Game/SeniorSendOff/Calder/Trees'
A = unreal.EditorAssetLibrary
S = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
T = unreal.AssetToolsHelpers.get_asset_tools()
SPECS = [
    ('Beech', '/Game/EuropeanBeech/Geometry/SimpleWind/SM_EuropeanBeech_Field_01', [5, 5, 5]),
    ('Maple', '/Game/NorwayMaple/Geometry/SimpleWind/SM_NorwayMaple_Field_01', [4, 5, 5]),
]
# Guard all assets in each acquired pack, including materials/textures. Read only.
def hashes():
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for pack in ['EuropeanBeech', 'NorwayMaple']
            for p in (ROOT/'Content'/pack).rglob('*.uasset')}
before = hashes()
assert before, 'Acquired tree packs missing'
A.make_directory(DEST)
rows=[]
for label, source_path, source_lods in SPECS:
    source=unreal.load_asset(source_path)
    assert source, source_path
    name='SM_CalderTreeCandidate_'+label
    mesh=unreal.load_asset(DEST+'/'+name) if A.does_asset_exist(DEST+'/'+name) else A.duplicate_asset(source_path, DEST+'/'+name)
    assert mesh, name
    mesh.set_editor_property('allow_cpu_access', True)
    nanite=mesh.get_editor_property('nanite_settings')
    nanite.enabled=False
    S.set_nanite_settings(mesh,nanite,False)
    if mesh.get_num_lods()>1:
        assert S.remove_lods(mesh)
    for destination_lod, source_lod in enumerate(source_lods):
        assert S.set_lod_from_static_mesh(mesh,destination_lod,source,source_lod,True)==destination_lod
        factor=([.55,.20,.06] if label=='Beech' else [1.,.50,.12])[destination_lod]
        if factor<1.:
            reduction=S.get_lod_reduction_settings(mesh,destination_lod)
            reduction.percent_triangles *= factor
            reduction.percent_vertices *= factor
            S.set_lod_reduction_settings(mesh,destination_lod,reduction)
    assert S.set_lod_screen_sizes(mesh,[1.0,.35,.12])
    S.remove_collisions(mesh)
    mesh.set_editor_property('generate_mesh_distance_field',False)
    A.save_loaded_asset(mesh)
    triangles=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())]
    assert len(triangles)==3 and triangles[0]<=20000 and all(triangles[i]>triangles[i+1]>=100 for i in range(2)), triangles
    assert not mesh.get_editor_property('nanite_settings').enabled
    bounds=mesh.get_bounds()
    materials=[]
    for slot in mesh.static_materials:
        material=slot.material_interface
        if not material: continue
        vectors={str(k):[value.r,value.g,value.b,value.a] for k in unreal.MaterialEditingLibrary.get_vector_parameter_names(material) for value in [unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material,k)]}
        scalars={str(k):float(unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material,k)) for k in unreal.MaterialEditingLibrary.get_scalar_parameter_names(material)}
        textures={str(k):(value.get_path_name() if value else None) for k in unreal.MaterialEditingLibrary.get_texture_parameter_names(material) for value in [unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(material,k)]}
        materials.append({'path':material.get_path_name(),'vectors':vectors,'scalars':scalars,'textures':textures})
    structural_path=DEST+'/'+name+'_Structural'
    structural=unreal.load_asset(structural_path) if A.does_asset_exist(structural_path) else A.duplicate_asset(DEST+'/'+name,structural_path)
    structural.set_editor_property('allow_cpu_access',True)
    nanite=structural.get_editor_property('nanite_settings'); nanite.enabled=False
    S.set_nanite_settings(structural,nanite,False)
    if structural.get_num_lods()>1: assert S.remove_lods(structural)
    assert S.set_lod_from_static_mesh(structural,0,source,3,True)==0
    structural.set_editor_property('generate_mesh_distance_field',False)
    S.remove_collisions(structural)
    A.save_loaded_asset(structural)
    rows.append({'label':label,'source':source_path,'source_lods':source_lods,'asset':mesh.get_path_name(), 'triangles':triangles,'nanite':False,'collision':'none','bound_in_game':False,'structural_asset':structural.get_path_name(),'structural_triangles':structural.get_num_triangles(0),
                 'bounds_min':[bounds.origin.x-bounds.box_extent.x,bounds.origin.y-bounds.box_extent.y,bounds.origin.z-bounds.box_extent.z],'bounds_max':[bounds.origin.x+bounds.box_extent.x,bounds.origin.y+bounds.box_extent.y,bounds.origin.z+bounds.box_extent.z],'materials':materials})
    unreal.log('CALDER_TREE_CANDIDATE '+label+' triangles='+str(triangles))
assert hashes()==before, 'Read-only acquired tree pack changed'
out=ROOT/'SourceArt/CalderLandscape/phase2/tree_candidates.json'
out.unlink(missing_ok=True)
out.write_text(json.dumps({'scope':'Unbound conventional candidates; appearance and runtime cost not accepted','source_sha256':before,'trees':rows},indent=2)+'\n')
unreal.log('CALDER_TREE_CANDIDATES PASS count=2 originalPacksUnchanged=1 boundInGame=0')
