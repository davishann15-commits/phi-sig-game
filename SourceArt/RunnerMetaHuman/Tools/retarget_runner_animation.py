"""Retarget the same Manny idle/walk set used by Braxton to Runner's body."""
import unreal

ROOT = '/Game/MetaHumans/RunnerRebuild/MH_Runner_Working'
FOLDER = ROOT + '/Animation'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
source = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
target = unreal.load_asset(ROOT + '/Body/SKM_MH_Runner_Working_BodyMesh')
if not source or not target:
    raise RuntimeError('Animation retarget source or Runner body missing')

def create(name, kind, factory):
    path = FOLDER + '/' + name
    return unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,FOLDER,kind,factory)

rigs = []
for name, mesh in [('IK_Manny',source),('IK_Runner',target)]:
    rig = create(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    controller = unreal.IKRigController.get_controller(rig)
    controller.set_skeletal_mesh(mesh)
    if not controller.apply_auto_generated_retarget_definition():
        raise RuntimeError('Cannot generate IK rig ' + name)
    controller.apply_auto_fbik()
    assets.save_loaded_asset(rig)
    rigs.append(rig)

retargeter = create('RTG_Runner', unreal.IKRetargeter, unreal.IKRetargetFactory())
controller = unreal.IKRetargeterController.get_controller(retargeter)
controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE,rigs[0])
controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET,rigs[1])
controller.remove_all_ops()
controller.add_default_ops()
controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
for index in range(controller.get_num_retarget_ops()):
    name = str(controller.get_op_name(index))
    if 'Solve' in name or 'Run IK' in name:
        controller.set_retarget_op_enabled(index,False)
assets.save_loaded_asset(retargeter)

inputs = unreal.IKRetargetBatchOperationInputs()
inputs.source_mesh = source
inputs.target_mesh = target
inputs.ik_retarget_asset = retargeter
inputs.target_path = FOLDER
inputs.prefix = 'RN_'
inputs.include_referenced_assets = False
inputs.overwrite_existing_files = True
inputs.assets_to_retarget = [assets.find_asset_data(path) for path in [
    '/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle',
    '/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd',
]]
results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
if len(results) != 2:
    raise RuntimeError('Runner retarget did not produce both animations')
assets.save_directory(FOLDER,only_if_is_dirty=False,recursive=True)
unreal.log('RUNNER_ANIMATIONS_COMPLETE ' + str([r.package_name for r in results]))
