"""Build Boy Wick's own editable MetaHuman, assembled rig, and locomotion set."""
import json
import math
import traceback
from pathlib import Path
import unreal

TASK = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BoywickMetaHuman')
SOURCE = '/Game/Characters/MetaHumans/Boywick/MH_Boywick'
BUILD = '/Game/MetaHumans/Boywick'
ROOT = BUILD + '/MH_Boywick'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
report = {}
character = None
try:
    character = assets.load_asset(SOURCE)
    if character and assets.get_metadata_tag(character, 'BoywickRigVersion') != '4':
        # This is our first-pass duplicate, backed up under this task's before/.
        # The native pale skin textures and existing rig come from a private
        # copy of the finished Braxton source; the original is never edited.
        assets.delete_asset(SOURCE)
        character = None
    character = character or assets.duplicate_asset(
        '/Game/Characters/MetaHumans/Braxton/MH_Braxton_Rebuild', SOURCE)
    if not character or not editor.try_add_object_to_edit(character):
        raise RuntimeError('Cannot open the independent Boy Wick MetaHuman')
    # These are art-direction measurements, not estimates of the photographed person.
    targets = {'Height': 166.0, 'Chest': 113.0, 'Waist': 108.0, 'Hip': 112.0,
               'Inseam': 72.0, 'Thigh': 65.0, 'Calf': 40.0,
               'Across Shoulder': 44.0, 'Neck to Waist': 39.0,
               'Shoulder Height': 140.0, 'Bicep': 38.0, 'Forearm': 30.0,
               'Neck': 42.0, 'Fat': 0.65, 'Masculine/Feminine': -1.0}
    if assets.get_metadata_tag(character, 'BoywickRigVersion') != '4':
        if not unreal.SeniorCharacterAssetTools.prepare_boywick_source(character):
            raise RuntimeError('Cannot prepare the native parametric body')
        preset = assets.load_asset('/MetaHumanCharacter/Optional/Presets/Victor')
        if not editor.try_add_object_to_edit(preset):
            raise RuntimeError('Cannot read the Victor base preset')
        try:
            coefficients = editor.get_face_model_coefficients(preset)
        finally:
            editor.remove_object_to_edit(preset)
        editor.set_face_model_coefficients(character, coefficients)
        editor.commit_face_state(character)
        constraints = editor.get_body_constraints(character)
        for i, constraint in enumerate(constraints):
            constraint.is_active = str(constraint.name) in targets
            if constraint.is_active:
                constraint.target_measurement = targets[str(constraint.name)]
            constraints[i] = constraint
        editor.set_body_constraints(character, constraints)
        editor.commit_body_state(character)
        before = editor.get_face_landmarks(character)
        # Full cheeks and a softer lower face. Work relative to this character's
        # head so resizing the body cannot move the sculpt to the wrong feature.
        bottom = min(p.z for p in before)
        top = max(p.z for p in before)
        span = top - bottom
        offsets = []
        for p in before:
            h = (p.z - bottom) / span
            cheek = max(0.0, 1.0 - abs(h - .46) / .25)
            jaw = max(0.0, 1.0 - abs(h - .22) / .19)
            side = min(1.0, abs(p.x) / 4.5)
            front = max(0.0, min(1.0, (p.y - 1.0) / 8.0))
            offsets.append(unreal.Vector(p.x * (.065 * cheek + .045 * jaw),
                                         .52 * cheek * side * front,
                                         -.13 * jaw))
        editor.translate_face_landmarks(character, list(range(len(before))), offsets)
        editor.commit_face_state(character)
        skin = character.get_editor_property('skin_settings')
        accents = skin.accents
        accents.lips.redness = .38
        accents.lips.saturation = .18
        accents.lips.lightness = .51
        accents.cheeks.redness = .53
        skin.accents = accents
        skin.skin.roughness = .94
        skin.freckles.strength = .08
        editor.commit_skin_settings(character, skin)
        assets.set_metadata_tag(character, 'BoywickFitVersion', '1')
    collection = editor.get_preview_collection(character)
    hair = assets.load_asset('/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_Hair_M_SideSweptFringe')
    if not hair:
        raise RuntimeError('Layered MetaHuman groom is not available')
    hair_key = collection.try_add_item_from_wardrobe_item('Hair', hair)
    collection.default_instance.set_single_slot_selection('Hair', hair_key)
    editor.on_edit_preview_collection(character)
    assets.set_metadata_tag(character, 'DisplayName', 'Boy Wick')
    assets.set_metadata_tag(character, 'ArtDirection', 'Short stocky adult; blond scoop fringe; navy patterned polo; khaki shorts')
    if assets.get_metadata_tag(character, 'BoywickRigVersion') != '4':
        if not unreal.SeniorCharacterAssetTools.bake_boywick_local_shape(character):
            raise RuntimeError('Cannot bake the locally fitted native DNA')
        assets.set_metadata_tag(character, 'BoywickRigVersion', '4')
        unreal.log('BOYWICK_LOCAL_RIG_COMPLETE')
    editor.assemble_for_preview(character)
    assets.save_loaded_asset(character)
    report['constraints'] = [(str(c.name), c.target_measurement)
                            for c in editor.get_body_constraints(character) if c.is_active]
    report['buildable'] = editor.can_build_meta_human(character)
    if not report['buildable']:
        raise RuntimeError('MetaHuman is not ready for assembly')
    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    params.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    params.absolute_build_path = BUILD
    params.common_folder_path = BUILD + '/Common'
    params.enable_wardrobe_item_validation = False
    unreal.log('BOYWICK_NATIVE_ASSEMBLY_BEGIN')
    editor.build_meta_human(character, params)
    assets.save_directory(BUILD, only_if_is_dirty=False, recursive=True)
    body = assets.load_asset(ROOT + '/Body/SKM_MH_Boywick_BodyMesh')
    outfit = assets.load_asset(ROOT + '/Clothing/MH_Boywick_Outfits')
    bp = assets.load_asset(ROOT + '/BP_MH_Boywick')
    if not body or not outfit or not bp:
        raise RuntimeError('Assembled MetaHuman is incomplete')
    report['body_bounds'] = str(body.get_bounds())
    # Export the native weighted garment to tailor its polo details without
    # replacing skinning with a rigid attachment.
    for obj, name in [(body, 'BoywickBody.fbx'), (outfit, 'BoywickOutfit.fbx')]:
        task = unreal.AssetExportTask()
        task.object = obj
        task.filename = str(TASK / name)
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.SkeletalMeshExporterFBX()
        options = unreal.FbxExportOption()
        options.level_of_detail = False
        options.collision = False
        task.options = options
        if not unreal.Exporter.run_asset_export_task(task):
            raise RuntimeError('Cannot export ' + name)
    editor.remove_object_to_edit(character)
    character = None
    folder = ROOT + '/Animation'
    source_mesh = assets.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
    def create(name, kind, factory):
        return assets.load_asset(folder + '/' + name) or tools.create_asset(name, folder, kind, factory)
    rigs = []
    for name, mesh in [('IK_Manny', source_mesh), ('IK_Boywick', body)]:
        rig = create(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
        control = unreal.IKRigController.get_controller(rig)
        control.set_skeletal_mesh(mesh)
        if not control.apply_auto_generated_retarget_definition():
            raise RuntimeError('Cannot generate ' + name)
        control.apply_auto_fbik()
        assets.save_loaded_asset(rig)
        rigs.append(rig)
    retarget = create('RTG_Boywick', unreal.IKRetargeter, unreal.IKRetargetFactory())
    control = unreal.IKRetargeterController.get_controller(retarget)
    control.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, rigs[0])
    control.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, rigs[1])
    control.remove_all_ops()
    control.add_default_ops()
    control.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    for i in range(control.get_num_retarget_ops()):
        if any(word in str(control.get_op_name(i)) for word in ('Solve', 'Run IK')):
            control.set_retarget_op_enabled(i, False)
    assets.save_loaded_asset(retarget)
    batch = unreal.IKRetargetBatchOperationInputs()
    batch.source_mesh = source_mesh
    batch.target_mesh = body
    batch.ik_retarget_asset = retarget
    batch.target_path = folder
    batch.prefix = 'BW_'
    batch.include_referenced_assets = False
    batch.overwrite_existing_files = True
    batch.assets_to_retarget = [assets.find_asset_data(path) for path in [
        '/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle',
        '/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd',
        '/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd']]
    result = unreal.IKRetargetBatchOperation.run_batch_retarget(batch)
    if len(result) != 3:
        raise RuntimeError('Locomotion retarget did not produce three animations')
    assets.save_directory(folder, only_if_is_dirty=False, recursive=True)
    report['animations'] = [str(r.package_name) for r in result]
    report['status'] = 'PASS'
    unreal.log('BOYWICK_NATIVE_ASSEMBLY_COMPLETE')
except Exception:
    report['status'] = 'FAIL'
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
finally:
    if character and editor.is_object_added_for_editing(character):
        editor.remove_object_to_edit(character)
    (TASK / 'evidence/assembly.json').write_text(json.dumps(report, indent=2))
    frames = [0]
    def finish(dt):
        frames[0] += 1
        if frames[0] > 8:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
    handle = unreal.register_slate_post_tick_callback(finish)
