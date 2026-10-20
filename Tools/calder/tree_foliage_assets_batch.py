"""Owned local autumn leaf shader; preview candidates only, no map/source saves.

Run after tree_assets_batch.py in an ordinary offscreen editor. Retains the
existing NatureFoliage wind and two-sided normals, using acquired texture data:
Mask R=coverage, G=roughness, B=translucency, verified in MF_TwoSided_Leaves.
"""
import hashlib,importlib.util,json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
E=unreal.MaterialEditingLibrary; A=unreal.EditorAssetLibrary
T=unreal.AssetToolsHelpers.get_asset_tools()
DEST='/Game/SeniorSendOff/Calder/Trees'
NAME='M_CalderTreeFoliage'; PATH=DEST+'/'+NAME
SOURCE='/Game/SeniorSendOff/House/ExteriorNature/M_NatureFoliage'
plan=json.loads((ROOT/'SourceArt/CalderLandscape/phase2/tree_candidates.json').read_text())
source_file=ROOT/'Content/SeniorSendOff/House/ExteriorNature/M_NatureFoliage.uasset'
source_sha=hashlib.sha256(source_file.read_bytes()).hexdigest()
spec=importlib.util.spec_from_file_location('calder_tree_helpers',ROOT/'Tools/calder/phase2_assets.py')
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
master=unreal.load_asset(PATH) if A.does_asset_exist(PATH) else A.duplicate_asset(SOURCE,PATH)
assert master,PATH
for x in list(E.get_material_expressions(master)):
    if str(x.get_editor_property('desc')).startswith('Calder local tree:'):
        E.delete_material_expression(master,x)
def node(kind,**properties):
    x=h.node(master,kind,**properties);x.set_editor_property('desc','Calder local tree: '+kind);return x
def custom(code,inputs,kind):
    x=h.custom(master,code,inputs,kind);x.set_editor_property('desc','Calder local tree: shader');return x
leaf=next(m for m in plan['trees'][0]['materials'] if 'TwoSided' in m['path'])
base_nodes=[x for x in E.get_material_expressions(master) if isinstance(x,unreal.MaterialExpressionTextureSampleParameter2D) and str(x.get_editor_property('parameter_name'))=='BaseMap']
assert base_nodes,'Expected original base-map samples'
for x in base_nodes: x.set_editor_property('texture',unreal.load_asset(leaf['textures']['Albedo']))
for x in E.get_material_expressions(master):
    if isinstance(x,unreal.MaterialExpressionTextureSampleParameter2D) and str(x.get_editor_property('parameter_name'))=='NormalMap':
        x.set_editor_property('texture',unreal.load_asset(leaf['textures']['Normal']))
base=base_nodes[0]
mask=node('TextureSampleParameter2D',parameter_name='CoverageMap',texture=unreal.load_asset(leaf['textures']['Mask']),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
tint=node('VectorParameter',parameter_name='AutumnTint',default_value=unreal.LinearColor(.44,.19,.035,1))
amount=node('ScalarParameter',parameter_name='AutumnAmount',default_value=1.)
gain=node('ScalarParameter',parameter_name='AutumnLuminanceGain',default_value=6.)
# Match the acquired MF_GenerateBranchMask selector: saturate(B^5*25000).
colour=custom('float light=clamp(dot(A.rgb,float3(.2126,.7152,.0722))*Gain,.025,1.1); float b=saturate(M.b); float leaf=saturate(b*b*b*b*b*25000.); return lerp(A.rgb,Tint.rgb*light,saturate(Amount)*leaf);',{'A':base,'M':mask,'Tint':tint,'Amount':amount,'Gain':gain},'CMOT_FLOAT3')
fade=node('PerInstanceFadeAmount')
coverage=custom('return M.r*Fade;',{'M':mask,'Fade':fade},'CMOT_FLOAT1')
rough=custom('return clamp(M.g,.45,.94);',{'M':mask},'CMOT_FLOAT1')
transmission=node('ScalarParameter',parameter_name='TreeTransmission',default_value=.32)
subsurface=custom('return C*M.b*T;',{'C':colour,'M':mask,'T':transmission},'CMOT_FLOAT3')
h.output(colour,'BASE_COLOR');h.output(coverage,'OPACITY_MASK')
h.output(rough,'ROUGHNESS');h.output(subsurface,'SUBSURFACE_COLOR')
master.set_editor_property('used_with_instanced_static_meshes',True)
E.recompile_material(master)
A.save_loaded_asset(master)
rows=[]
for tree in plan['trees']:
    label=tree['label']
    leaf=next(m for m in tree['materials'] if 'TwoSided' in m['path'])
    name='MI_CalderTree_'+label+'_TwoSided';path=DEST+'/'+name
    instance=unreal.load_asset(path) if A.does_asset_exist(path) else T.create_asset(name,DEST,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    assert instance,path
    E.set_material_instance_parent(instance,master)
    # UE5.8 setters return a false placeholder even after assignment; verify
    # actual readback instead (MaterialEditingLibrary.cpp:1483-1516).
    for parameter,key in [('BaseMap','Albedo'),('NormalMap','Normal'),('CoverageMap','Mask')]:
        value=unreal.load_asset(leaf['textures'][key])
        E.set_material_instance_texture_parameter_value(instance,parameter,value)
        assert E.get_material_instance_texture_parameter_value(instance,parameter)==value,parameter
    tone=(.44,.19,.035) if label=='Beech' else (.52,.09,.016)
    E.set_material_instance_vector_parameter_value(instance,'AutumnTint',unreal.LinearColor(*tone,1.))
    actual=E.get_material_instance_vector_parameter_value(instance,'AutumnTint')
    assert max(abs(a-b) for a,b in zip(tone,(actual.r,actual.g,actual.b)))<.00001
    E.set_material_instance_scalar_parameter_value(instance,'AutumnAmount',1.)
    assert abs(E.get_material_instance_scalar_parameter_value(instance,'AutumnAmount')-1.)<.00001
    E.set_material_instance_scalar_parameter_value(instance,'AutumnLuminanceGain',6.)
    assert abs(E.get_material_instance_scalar_parameter_value(instance,'AutumnLuminanceGain')-6.)<.00001
    E.set_material_instance_scalar_parameter_value(instance,'WindStrength',1.3)
    assert abs(E.get_material_instance_scalar_parameter_value(instance,'WindStrength')-1.3)<.00001
    E.update_material_instance(instance);A.save_loaded_asset(instance)
    stats=E.get_statistics(instance)
    assert stats.num_pixel_shader_instructions>0 and 0<stats.num_pixel_texture_samples<=12,(label,stats)
    bound=[]
    for key in ['asset','structural_asset']:
        mesh=unreal.load_asset(tree[key]);slots=list(mesh.get_editor_property('static_materials'))
        count=0
        for slot in slots:
            if slot.material_interface and 'TwoSided' in slot.material_interface.get_name():
                slot.material_interface=instance;count+=1
        assert count>0,mesh.get_path_name()
        mesh.set_editor_property('static_materials',slots);A.save_loaded_asset(mesh)
        bound.append(mesh.get_path_name())
    rows.append({'label':label,'asset':instance.get_path_name(),'pixel_instructions':stats.num_pixel_shader_instructions,'pixel_samples':stats.num_pixel_texture_samples,'preview_meshes':bound,'autumn_tint':tone})
    unreal.log('CALDER_TREE_LOCAL_FOLIAGE '+json.dumps(rows[-1]))
for relative,expected in plan['source_sha256'].items():
    assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected,relative
assert hashlib.sha256(source_file.read_bytes()).hexdigest()==source_sha
out=ROOT/'SourceArt/CalderLandscape/phase2/tree_local_foliage.json'
out.unlink(missing_ok=True)
out.write_text(json.dumps({'scope':'Owned preview candidates only; appearance, wind registration and live forest fit pending','source_foliage_sha256':source_sha,'source_packs_unchanged':True,'materials':rows},indent=2)+'\n')
unreal.log('CALDER_TREE_LOCAL_FOLIAGE_BATCH PASS materials=2 sourceAssetsUnchanged=1 liveForestBound=0')
