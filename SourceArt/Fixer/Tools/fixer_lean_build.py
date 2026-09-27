"""Local reference-guided face and lean-body revision of the Fixer."""
import json, traceback
from pathlib import Path
import unreal

A=unreal.EditorAssetLibrary
S=unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
SOURCE='/Game/Characters/MetaHumans/Fixer/MH_Fixer'
CHARACTER='/Game/Characters/MetaHumans/Fixer/MH_Fixer_Lean'
BUILD='/Game/MetaHumans/FixerLean'
ROOT=BUILD+'/MH_Fixer_Lean'
OLD='/Game/MetaHumans/Fixer/MH_Fixer'
report={}; character=None
try:
    character=A.load_asset(CHARACTER) if A.does_asset_exist(CHARACTER) else A.duplicate_asset(SOURCE,CHARACTER)
    assert character and S.try_add_object_to_edit(character)
    assert unreal.SeniorCharacterAssetTools.prepare_fixer_lean_source(character), 'Could not enable editable body proportions'
    # Keep height, shoulder breadth, head and neck unchanged. Reduce girth.
    targets={'Height':168.0,'Chest':81.0,'Waist':64.0,'Hip':84.0,
             'Thigh':40.0,'Calf':28.0,'Bicep':22.5,'Elbow':21.0,
             'Forearm':20.5,'Wrist':14.5,'Neck':34.0,'Across Shoulder':34.5}
    constraints=S.get_body_constraints(character)
    for i,c in enumerate(constraints):
        c.is_active=str(c.name) in targets
        if c.is_active:c.target_measurement=targets[str(c.name)]
        constraints[i]=c
    S.set_body_constraints(character,constraints)
    S.commit_body_state(character)
    if A.get_metadata_tag(character,'FixerReferenceFaceVersion') not in ('2','3'):
        before=S.get_face_landmarks(character)
        # Paired edits: narrower oval jaw, less-full lips, compact rounded
        # nose, relaxed smaller eye openings and higher/straighter brows.
        # Centimetres in the existing native face's +Y-forward coordinates.
        changes={
            1:(.40,.08,-.10),28:(-.40,.08,-.10),
            2:(.32,-.12,-.10),29:(-.32,-.12,-.10),
            3:(.12,-.18,-.16),30:(-.12,-.18,-.16),
            43:(-.22,0,-.10),63:(.22,0,-.10),
            62:(0,-.22,-.25),8:(0,0,-.12),
            23:(-.12,.06,.10),66:(.12,.06,.10),
            4:(0,-.32,.34),5:(0,-.28,-.23),
            9:(.05,-.28,.27),16:(-.05,-.28,.27),
            31:(-.06,-.23,-.15),47:(.06,-.23,-.15),
            64:(0,-.48,.16),65:(0,-.18,.10),
            45:(-.17,-.12,.08),67:(.17,-.12,.08),
            6:(0,-.14,0),
            20:(0,-.05,-.10),44:(0,-.05,-.10),
            35:(0,-.04,-.08),51:(0,-.04,-.08),
            36:(0,-.04,-.08),52:(0,-.04,-.08),
            37:(0,0,.08),53:(0,0,.08),
            10:(0,.06,.25),26:(0,.06,.25),
            24:(0,.06,.27),77:(0,.06,.27),
            25:(0,.04,.08),78:(0,.04,.08),
        }
        S.translate_face_landmarks(character,list(changes),[unreal.Vector(*changes[i]) for i in changes])
        S.commit_face_state(character)
        A.set_metadata_tag(character,'FixerReferenceFaceVersion','2')
        after=S.get_face_landmarks(character)
        report['face_before']=[[v.x,v.y,v.z] for v in before]
        report['face_after']=[[v.x,v.y,v.z] for v in after]
    if A.get_metadata_tag(character,'FixerReferenceFaceVersion')!='3':
        start=S.get_face_landmarks(character)
        # Match anatomical targets iteratively. A single sculpt API delta is
        # strongly softened by the face solver, so its input is not its result.
        target_deltas={
            1:(.38,.05,-.08),28:(-.38,.05,-.08),
            2:(.26,-.12,-.12),29:(-.26,-.12,-.12),
            3:(.12,-.16,-.13),30:(-.12,-.16,-.13),
            62:(0,-.12,-.28),8:(0,0,-.08),
            4:(0,-.25,.28),5:(0,-.23,-.28),
            9:(.05,-.20,.20),16:(-.05,-.20,.20),
            31:(-.04,-.18,-.14),47:(.04,-.18,-.14),
            64:(0,-.62,.15),65:(0,-.20,.12),
            45:(-.18,-.12,.06),67:(.18,-.12,.06),
            24:(0,.05,.52),77:(0,.05,.52),
            10:(0,.04,.12),26:(0,.04,.12),
            25:(0,0,.04),78:(0,0,.04),
        }
        targets_face={i:unreal.Vector(start[i].x+d[0],start[i].y+d[1],start[i].z+d[2]) for i,d in target_deltas.items()}
        for iteration in range(7):
            now=S.get_face_landmarks(character);moves=[]
            for i in target_deltas:
                t=targets_face[i];p=now[i]
                moves.append(unreal.Vector((t.x-p.x)*1.6,(t.y-p.y)*1.6,(t.z-p.z)*1.6))
            S.translate_face_landmarks(character,list(target_deltas),moves)
        S.commit_face_state(character)
        end=S.get_face_landmarks(character)
        report['reference_face_actual_delta_cm']={str(i):[end[i].x-start[i].x,end[i].y-start[i].y,end[i].z-start[i].z] for i in target_deltas}
        A.set_metadata_tag(character,'FixerReferenceFaceVersion','3')
    assert unreal.SeniorCharacterAssetTools.bake_fixer_local_shape(character), 'Local shape bake failed'
    report['face_landmarks']=[[v.x,v.y,v.z] for v in S.get_face_landmarks(character)]
    assert A.save_loaded_asset(character)
    report['targets_cm']=targets
    report['saved_constraints']={str(c.name):c.target_measurement for c in S.get_body_constraints(character) if c.is_active}
    assert S.can_build_meta_human(character) and character.has_high_resolution_textures, 'Local source is not buildable; no cloud request made'
    params=unreal.MetaHumanCharacterEditorBuildParameters()
    params.pipeline_type=unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    params.pipeline_quality=unreal.MetaHumanQualityLevel.HIGH
    params.absolute_build_path=BUILD;params.common_folder_path=BUILD+'/Common'
    params.enable_wardrobe_item_validation=False
    S.build_meta_human(character,params)
    body=A.load_asset(ROOT+'/Body/SKM_MH_Fixer_Lean_BodyMesh')
    outfit=A.load_asset(ROOT+'/Clothing/MH_Fixer_Lean_Outfits')
    face=A.load_asset(ROOT+'/Face/SKM_MH_Fixer_Lean_FaceMesh')
    assert body and outfit,'Lean assembly meshes missing'
    # Reuse the approved complexion and fabric, not new default white overrides.
    oldbody=A.load_asset(OLD+'/Body/SKM_MH_Fixer_BodyMesh')
    oldmats=list(oldbody.get_editor_property('materials'))
    newmats=list(body.get_editor_property('materials'))
    for i,slot in enumerate(newmats):
        if i<len(oldmats):slot.material_interface=oldmats[i].material_interface
        newmats[i]=slot
    body.set_editor_property('materials',newmats)
    A.save_loaded_asset(body);A.save_loaded_asset(outfit)
    oldface=A.load_asset(OLD+'/Face/SKM_MH_Fixer_FaceMesh')
    oldmats=list(oldface.get_editor_property('materials'))
    mats=list(face.get_editor_property('materials'))
    for i,slot in enumerate(mats):
        if i<len(oldmats):slot.material_interface=oldmats[i].material_interface
        mats[i]=slot
    face.set_editor_property('materials',mats);A.save_loaded_asset(face)
    # Keep brown irises, but let their natural pattern show instead of an
    # almost-black global multiplier. Independent copies leave Braxton alone.
    E=unreal.MaterialEditingLibrary
    for i,slot in enumerate(mats):
        material=slot.material_interface
        if not isinstance(material,unreal.MaterialInstanceConstant):continue
        if 'Iris Color Multiply' not in {str(n) for n in E.get_vector_parameter_names(material)}:continue
        eye_path=ROOT+'/Face/Materials/MI_FixerReferenceEye_'+str(i)
        eye=A.load_asset(eye_path) if A.does_asset_exist(eye_path) else A.duplicate_asset(material.get_path_name().split('.')[0],eye_path)
        E.set_material_instance_vector_parameter_value(eye,'Iris Color Multiply',unreal.LinearColor(.65,.42,.25,1))
        E.set_material_instance_scalar_parameter_value(eye,'Pupil Dilation',.88)
        E.update_material_instance(eye);A.save_loaded_asset(eye)
        slot.material_interface=eye;mats[i]=slot
    face.set_editor_property('materials',mats);A.save_loaded_asset(face)
    # Export the newly fitted outfit for re-projecting the existing shirt print.
    task=unreal.AssetExportTask();task.object=outfit
    task.filename='/private/tmp/FixerLeanOutfit.fbx'
    task.automated=True;task.prompt=False;task.replace_identical=True
    task.options=unreal.FbxExportOption();task.options.level_of_detail=False
    task.options.export_morph_targets=False
    assert unreal.Exporter.run_asset_export_task(task)
    for obj,name in [(body,'FixerLeanBody'),(face,'FixerLeanFace')]:
        task.object=obj;task.filename='/private/tmp/'+name+'.fbx'
        assert unreal.Exporter.run_asset_export_task(task)
    # Body-space sneaker attachment needs only the change in native foot origin.
    report['body']=body.get_path_name();report['outfit']=outfit.get_path_name()
    report['complete']=bool(A.save_directory(BUILD,only_if_is_dirty=True,recursive=True))
    unreal.log('FIXER_LEAN_BUILD_COMPLETE '+json.dumps(report))
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error']);raise
finally:
    if character and S.is_object_added_for_editing(character):S.remove_object_to_edit(character)
    Path('/private/tmp/fixer_lean_report.json').write_text(json.dumps(report,indent=2))
