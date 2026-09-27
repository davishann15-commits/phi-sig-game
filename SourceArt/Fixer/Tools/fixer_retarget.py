"""Retarget independent Fixer idle/walk assets using the established rig chain."""
import unreal, json, traceback
from pathlib import Path
LEAN='-FixerLeanDetails' in unreal.SystemLibrary.get_command_line()
ROOT='/Game/MetaHumans/FixerLean/MH_Fixer_Lean' if LEAN else '/Game/MetaHumans/Fixer/MH_Fixer'
assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
report={}
try:
    source=assets.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
    target=assets.load_asset(ROOT+('/Body/SKM_MH_Fixer_Lean_BodyMesh' if LEAN else '/Body/SKM_MH_Fixer_BodyMesh'))
    assert source and target
    folder=ROOT+'/Animation'
    def create(name,cls,factory):
        return assets.load_asset(folder+'/'+name) if assets.does_asset_exist(folder+'/'+name) else tools.create_asset(name,folder,cls,factory)
    rigs=[]
    for name,mesh in [('IK_Manny',source),('IK_Fixer',target)]:
        rig=create(name,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
        c=unreal.IKRigController.get_controller(rig)
        c.set_skeletal_mesh(mesh)
        assert c.apply_auto_generated_retarget_definition()
        c.apply_auto_fbik()
        assets.save_loaded_asset(rig)
        rigs.append(rig)
    rtg=create('RTG_Fixer',unreal.IKRetargeter,unreal.IKRetargetFactory())
    c=unreal.IKRetargeterController.get_controller(rtg)
    c.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE,rigs[0])
    c.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET,rigs[1])
    c.remove_all_ops();c.add_default_ops()
    c.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    for i in range(c.get_num_retarget_ops()):
        name=str(c.get_op_name(i))
        if 'Solve' in name or 'Run IK' in name:c.set_retarget_op_enabled(i,False)
    assets.save_loaded_asset(rtg)
    inputs=unreal.IKRetargetBatchOperationInputs()
    inputs.source_mesh=source;inputs.target_mesh=target;inputs.ik_retarget_asset=rtg
    inputs.target_path=folder;inputs.prefix='FX_'
    inputs.include_referenced_assets=False;inputs.overwrite_existing_files=True
    inputs.assets_to_retarget=[assets.find_asset_data(p) for p in [
        '/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle',
        '/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd']]
    results=unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    assert len(results)==2
    assets.save_directory(folder,only_if_is_dirty=False,recursive=True)
    report={'complete':True,'animations':[str(r.package_name) for r in results]}
    unreal.log('FIXER_ANIMATIONS_COMPLETE')
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
    raise
finally:Path('/private/tmp/fixer_animation_report.json').write_text(json.dumps(report,indent=2))
