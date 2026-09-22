"""Photo-guided likeness pass on the isolated review MetaHuman, never C01."""
import unreal, json, math
from pathlib import Path

OUT=Path('/Users/Stewart/Documents/Codex/2026-09-11/c/work')
assets=unreal.EditorAssetLibrary
path='/Game/Characters/MetaHumans/Braxton/MH_Braxton_Working'
character=assets.load_asset(path)
backup='/Game/Characters/MetaHumans/Braxton/MH_Braxton_BeforeRefinement02'
if not assets.does_asset_exist(backup):
    saved=assets.duplicate_asset(path,backup)
    if not saved or not assets.save_loaded_asset(saved): raise RuntimeError('Backup failed')
sub=unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([character])
if not sub.is_object_added_for_editing(character): raise RuntimeError('Cannot edit Braxton')

# The photos show dark brown irises, not the amber default.
eyes=character.get_editor_property('eyes_settings')
for side in ('eye_left','eye_right'):
    eye=eyes.get_editor_property(side)
    iris=eye.iris
    iris.global_tint=unreal.LinearColor(.18,.10,.055,1)
    iris.global_saturation=1.0
    eye.iris=iris
    pupil=eye.pupil
    pupil.dilation=1.04
    eye.pupil=pupil
    eyes.set_editor_property(side,eye)
sub.commit_eyes_settings(character,eyes)

if assets.get_metadata_tag(character,'BraxtonFaceFitVersion')!='2':
    sub.remove_face_rig(character)
    before=sub.get_face_landmarks(character)
    # Sparse, paired edits avoid repeatedly constraining every untouched landmark.
    # Reference: broader squared chin, fuller lower cheeks, shorter philtrum,
    # wider mouth, less pinched lids, slightly narrower nostril wings.
    delta={
        3:(-.42,.12,.05),30:(.42,.12,.05),
        43:(.32,.08,.03),63:(-.32,.08,.03),62:(0,.20,.04),
        2:(-.20,.12,.05),29:(.20,.12,.05),
        32:(.32,.05,.32),48:(-.32,.05,.32),
        4:(0,.05,.24),5:(0,-.06,.28),50:(0,.02,.28),
        9:(-.12,.03,.26),16:(.12,.03,.26),
        31:(.12,-.03,.28),47:(-.12,-.03,.28),
        34:(.12,.02,.28),49:(-.12,.02,.28),
        45:(-.12,0,.02),67:(.12,0,.02),
        20:(0,.04,.12),44:(0,.04,.12),
        37:(0,.10,-.10),53:(0,.10,-.10),
    }
    targets={i:unreal.Vector(before[i].x+d[0],before[i].y+d[1],before[i].z+d[2]) for i,d in delta.items()}
    for _ in range(3):
        points=sub.get_face_landmarks(character)
        indices=list(targets)
        offsets=[unreal.Vector((targets[i].x-points[i].x)*.7,(targets[i].y-points[i].y)*.7,(targets[i].z-points[i].z)*.7) for i in indices]
        sub.translate_face_landmarks(character,indices,offsets)
    sub.commit_face_state(character)
    after=sub.get_face_landmarks(character)
    changes=[math.sqrt((a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2) for a,b in zip(after,before)]
    if max(changes)>1.5: raise RuntimeError('Unexpectedly large face edit; source not saved')
    (OUT/'braxton_refinement02_landmarks.json').write_text(json.dumps({'before':[[p.x,p.y,p.z] for p in before],'after':[[p.x,p.y,p.z] for p in after],'maximum_change_cm':max(changes)},indent=2))
    assets.set_metadata_tag(character,'BraxtonFaceFitVersion','2')
    unreal.log('BRAXTON_FACE_REFINED max_cm='+str(max(changes)))

collection=sub.get_preview_collection(character)
for slot,relative in [('Hair','Hair/WI_Hair_S_SideSweptFringe'),('Eyebrows','Eyebrows/WI_Eyebrows_M_SlightArch'),('Mustache','Mustaches/WI_Mustache_S_Stubble'),('Beard','Beards/WI_Beard_S_Stubble')]:
    item=assets.load_asset('/MetaHumanCharacter/Optional/Grooms/Bindings/'+relative)
    if not item: raise RuntimeError('Missing groom '+relative)
    key=collection.try_add_item_from_wardrobe_item(slot,item)
    collection.default_instance.set_single_slot_selection(slot,key)
sub.on_edit_preview_collection(character)
assets.set_metadata_tag(character,'ProductionStatus','Photo-guided likeness revision 2. Review character only; not approved as final or installed in lobby.')
if not assets.save_loaded_asset(character): raise RuntimeError('Save failed')
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).close_all_editors_for_asset(character)
unreal.log('BRAXTON_REFINEMENT02_SAVED')
