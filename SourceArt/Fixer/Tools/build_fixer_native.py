"""Photo-guided local Fixer assembly using the same native pipeline as Braxton."""
import json, math, traceback
from pathlib import Path
import unreal

assets=unreal.EditorAssetLibrary
sub=unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
source='/Game/Characters/MetaHumans/Braxton/MH_Braxton_Rebuild'
target='/Game/Characters/MetaHumans/Fixer/MH_Fixer'
build='/Game/MetaHumans/Fixer'
root=build+'/MH_Fixer'
report={}
character=None
try:
    # The first local study changed the synthesis UV, which invalidated the
    # downloaded animated texture set. Preserve it and start from a complete
    # source; complexion is adjusted in independent baked material instances.
    old=assets.load_asset(target) if assets.does_asset_exist(target) else None
    if old and not old.has_high_resolution_textures:
        assert assets.rename_asset(target,target+'_SkinStudyBackup'), 'Could not preserve skin study'
    character=assets.load_asset(target) if assets.does_asset_exist(target) else assets.duplicate_asset(source,target)
    assert character and sub.try_add_object_to_edit(character), 'Could not edit isolated Fixer source'
    if assets.get_metadata_tag(character,'FixerFitVersion')!='1':
        constraints=sub.get_body_constraints(character)
        targets={'Height':168.0,'Chest':89.0,'Waist':72.0,'Hip':89.0,
                 'Thigh':47.0,'Calf':32.0,'Bicep':27.0,'Elbow':23.5,
                 'Forearm':24.0,'Wrist':15.5,'Neck':34.0,'Across Shoulder':34.5}
        for index,c in enumerate(constraints):
            c.is_active=str(c.name) in targets
            if c.is_active:c.target_measurement=targets[str(c.name)]
            constraints[index]=c
        sub.set_body_constraints(character,constraints)
        sub.commit_body_state(character)
        report['body_targets_cm']=targets
        before=sub.get_face_landmarks(character)
        assert len(before)==79, 'Unexpected landmark topology'
        # The front/profile photos show a narrow oval lower face, compact
        # rounded chin, thin relaxed lips and a modest, rounded nose tip.
        changes={
            1:(.42,.10,.12),28:(-.42,.10,.12),
            2:(.25,.20,.12),29:(-.25,.20,.12),
            8:(0,.25,.28),12:(0,0,.10),
            3:(.10,-.15,.12),30:(-.10,-.15,.12),
            4:(0,-.28,-.08),62:(0,-.24,.12),
            9:(-.07,-.18,-.06),16:(.07,-.18,-.06),
            31:(-.22,-.04,0),47:(.22,-.04,0),
            5:(0,.14,.10),6:(0,-.10,0),
            23:(-.18,.12,.05),67:(.18,.12,.05),
        }
        sub.translate_face_landmarks(character,list(changes),[unreal.Vector(*changes[i]) for i in changes])
        sub.commit_face_state(character)
        after=sub.get_face_landmarks(character)
        report['max_face_delta_cm']=max(math.dist((a.x,a.y,a.z),(b.x,b.y,b.z)) for a,b in zip(before,after))
        report['face_before']=[[p.x,p.y,p.z] for p in before]
        report['face_after']=[[p.x,p.y,p.z] for p in after]

        settings=character.get_editor_property('skin_settings')
        skin=settings.get_editor_property('skin')
        skin.set_editor_property('roughness',.95)
        settings.set_editor_property('skin',skin)
        freckles=settings.get_editor_property('freckles')
        for k,v in [('density',.32),('strength',.12),('saturation',.40)]:freckles.set_editor_property(k,v)
        freckles.set_editor_property('mask',unreal.MetaHumanCharacterFrecklesMask.TYPE2)
        settings.set_editor_property('freckles',freckles)
        accents=settings.get_editor_property('accents')
        for name in ('nose','cheeks'):
            region=accents.get_editor_property(name);region.set_editor_property('redness',.52)
            accents.set_editor_property(name,region)
        lips=accents.get_editor_property('lips');lips.set_editor_property('saturation',.30)
        lips.set_editor_property('redness',.43);accents.set_editor_property('lips',lips)
        settings.set_editor_property('accents',accents)
        # Do not re-synthesize the source textures. Preserve the locally
        # available high-resolution set and style skin after assembly.

        eyes=character.get_editor_property('eyes_settings')
        for name in ('eye_left','eye_right'):
            eye=eyes.get_editor_property(name);iris=eye.get_editor_property('iris')
            iris.set_editor_property('primary_color_u',.90);iris.set_editor_property('primary_color_v',.56)
            iris.set_editor_property('secondary_color_u',.98);iris.set_editor_property('secondary_color_v',.40)
            iris.set_editor_property('global_tint',unreal.LinearColor(.20,.105,.05,1))
            eye.set_editor_property('iris',iris);eyes.set_editor_property(name,eye)
        sub.commit_eyes_settings(character,eyes)
        collection=sub.get_preview_collection(character)
        for slot,path in [
            ('Hair','/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_Hair_S_Messy'),
            ('Eyebrows','/MetaHumanCharacter/Optional/Grooms/Bindings/Eyebrows/WI_Eyebrows_M_Natural'),
            ('Outfits','/MetaHumanCharacter/Optional/Clothing/WI_DefaultGarment')]:
            item=assets.load_asset(path);assert item, path
            key=collection.try_add_item_from_wardrobe_item(slot,item)
            collection.default_instance.set_single_slot_selection(slot,key)
        for slot in ('Mustache','Beard'):
            collection.default_instance.set_single_slot_selection(slot,unreal.MetaHumanPaletteItemKey())
        sub.on_edit_preview_collection(character)
        assets.set_metadata_tag(character,'FixerFitVersion','1')
        assets.set_metadata_tag(character,'ReferencePhotos','COBBLE IMG_1985.HEIC;IMG_1986.HEIC;IMG_1987.HEIC')
        assets.set_metadata_tag(character,'HeightArtDirectionCm','168 - provisional, shorter and leaner than Braxton')
        assert assets.save_loaded_asset(character)
    report['buildable']=sub.can_build_meta_human(character)
    report['high_res']=character.has_high_resolution_textures
    assert report['buildable'] and report['high_res'], 'Local assembly prerequisites missing'
    params=unreal.MetaHumanCharacterEditorBuildParameters()
    params.pipeline_type=unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    params.pipeline_quality=unreal.MetaHumanQualityLevel.HIGH
    params.absolute_build_path=build;params.common_folder_path=build+'/Common'
    params.enable_wardrobe_item_validation=False
    unreal.log('FIXER_ASSEMBLY_BEGIN')
    sub.build_meta_human(character,params)
    assert assets.save_directory(build,only_if_is_dirty=False,recursive=True)
    assert assets.save_loaded_asset(character)
    report['assets']=list(assets.list_assets(build,recursive=True))
    for name,path in [('FixerNativeBody',root+'/Body/SKM_MH_Fixer_BodyMesh'),('FixerNativeOutfit',root+'/Clothing/MH_Fixer_Outfits')]:
        obj=assets.load_asset(path);assert obj,path
        if name=='FixerNativeOutfit':
            copy_path=root+'/Details/Hoodie/SK_ExportSource'
            obj=assets.load_asset(copy_path) if assets.does_asset_exist(copy_path) else assets.duplicate_asset(path,copy_path)
            assert unreal.SeniorCharacterAssetTools.prepare_braxton_hoodie_export(obj)
            assets.save_loaded_asset(obj)
        task=unreal.AssetExportTask();task.object=obj;task.filename='/private/tmp/'+name+'.fbx'
        task.automated=True;task.prompt=False;task.replace_identical=True;task.options=unreal.FbxExportOption()
        task.options.set_editor_property('level_of_detail',False);task.options.set_editor_property('export_morph_targets',False)
        assert unreal.Exporter.run_asset_export_task(task),name
        report[name]=task.filename
    report['complete']=True
    unreal.log('FIXER_NATIVE_ASSEMBLY_COMPLETE')
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    if character and sub.is_object_added_for_editing(character):sub.remove_object_to_edit(character)
    Path('/private/tmp/fixer_native_build.json').write_text(json.dumps(report,indent=2))
