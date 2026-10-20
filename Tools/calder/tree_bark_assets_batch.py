"""Local opaque bark materials for unbound candidates; acquired assets read-only.
Run after tree_assets_batch and tree_foliage_assets_batch. No map saves.
"""
import hashlib,json,shutil
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST='/Game/SeniorSendOff/Calder/Trees'
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary;T=unreal.AssetToolsHelpers.get_asset_tools()
plan=json.loads((ROOT/'SourceArt/CalderLandscape/phase2/tree_candidates.json').read_text())
parent_path='/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured'
parent_file=ROOT/'Content/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.uasset'
parent_sha=hashlib.sha256(parent_file.read_bytes()).hexdigest()
parent=unreal.load_asset(parent_path);assert parent
backup=ROOT/'Saved/CalderWork/tree_candidate_trials/local_foliage_baseline';backup.mkdir(exist_ok=True)
for p in (ROOT/'Content/SeniorSendOff/Calder/Trees').glob('*.uasset'):
    if not (backup/p.name).exists(): shutil.copy2(p,backup/p.name)
rows=[]
for tree in plan['trees']:
    replacements={}
    for m in tree['materials']:
        original=m['path'].split('.')[-1]
        if 'TwoSided' in original or not all(m['textures'].get(k) for k in ['Albedo','Normal']): continue
        suffix=original.split('_',2)[-1]
        name='MI_CalderTree_'+tree['label']+'_'+suffix; path=DEST+'/'+name
        material=unreal.load_asset(path) if A.does_asset_exist(path) else T.create_asset(name,DEST,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        assert material,path
        E.set_material_instance_parent(material,parent)
        for parameter,key in [('BaseMap','Albedo'),('NormalMap','Normal')]:
            value=unreal.load_asset(m['textures'][key]);assert value,m['textures'][key]
            E.set_material_instance_texture_parameter_value(material,parameter,value)
            assert E.get_material_instance_texture_parameter_value(material,parameter)==value,parameter
        for parameter,value in [('UVScale',1.),('NormalStrength',.8),('Roughness',.84)]:
            E.set_material_instance_scalar_parameter_value(material,parameter,value)
            assert abs(E.get_material_instance_scalar_parameter_value(material,parameter)-value)<.00001,parameter
        for parameter,value in [('Tint',unreal.LinearColor(1,1,1,1)),('Emission',unreal.LinearColor(0,0,0,1))]:
            E.set_material_instance_vector_parameter_value(material,parameter,value)
            actual=E.get_material_instance_vector_parameter_value(material,parameter)
            assert max(abs(getattr(actual,k)-getattr(value,k)) for k in ['r','g','b'])<.00001,parameter
        E.update_material_instance(material);A.save_loaded_asset(material)
        stats=E.get_statistics(material)
        assert stats.num_pixel_shader_instructions>0 and 0<stats.num_pixel_texture_samples<=12
        replacements[original]=material;replacements[name]=material
        row={'source':m['path'],'asset':material.get_path_name(),'pixel_instructions':stats.num_pixel_shader_instructions,'pixel_samples':stats.num_pixel_texture_samples}
        rows.append(row);unreal.log('CALDER_TREE_LOCAL_BARK '+json.dumps(row))
    for key in ['asset','structural_asset']:
        mesh=unreal.load_asset(tree[key]);slots=list(mesh.get_editor_property('static_materials'));changed=0
        for slot in slots:
            if slot.material_interface and slot.material_interface.get_name() in replacements:
                slot.material_interface=replacements[slot.material_interface.get_name()];changed+=1
        assert changed>0,tree[key]
        mesh.set_editor_property('static_materials',slots);A.save_loaded_asset(mesh)
for relative,expected in plan['source_sha256'].items():
    assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected,relative
assert hashlib.sha256(parent_file.read_bytes()).hexdigest()==parent_sha
out=ROOT/'SourceArt/CalderLandscape/phase2/tree_local_bark.json';out.unlink(missing_ok=True)
out.write_text(json.dumps({'scope':'Opaque local bark on unbound preview candidates; geometry and appearance not accepted','source_parent_sha256':parent_sha,'source_packs_unchanged':True,'materials':rows},indent=2)+'\n')
unreal.log('CALDER_TREE_LOCAL_BARK_BATCH PASS materials='+str(len(rows))+' sourceAssetsUnchanged=1 liveForestBound=0')
