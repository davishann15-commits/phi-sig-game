"""Conservative front/side-reference proportion pass, isolated from game C01."""
import unreal, json
from pathlib import Path

path='/Game/Characters/MetaHumans/Braxton/MH_Braxton_Working'
assets=unreal.EditorAssetLibrary
character=assets.load_asset(path)
sub=unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not character or not sub.try_add_object_to_edit(character):
    raise RuntimeError('Braxton MetaHuman is unavailable for editing')
try:
    # Preserve the original state as an inspectable Unreal asset before fitting.
    backup='/Game/Characters/MetaHumans/Braxton/MH_Braxton_BaseBackup'
    if not assets.does_asset_exist(backup):
        assets.duplicate_asset(path,backup)
    if assets.get_metadata_tag(character,'BraxtonFaceFitVersion')!='1':
        points=sub.get_face_landmarks(character)
        if len(points)!=79: raise RuntimeError('Unexpected MetaHuman landmark layout')
        # Front photo: eye line ~1060, mouth ~1420, chin ~1730 px.
        # Fit relative feature spacing; do not interpret camera tilt as anatomy.
        # X is face lateral, Y forward, Z vertical, in Unreal centimeters.
        deltas=[]
        for p in points:
            x,y,z=p.x,p.y,p.z
            front=max(0.0,min(1.0,(y-3.0)/7.0))
            mouth=max(0.0,1.0-abs(z-153.6)/3.2)*front
            jaw=max(0.0,1.0-abs(z-152.0)/4.0)
            cheek=max(0.0,1.0-abs(z-158.0)/3.0)
            dx=-x*(.065*jaw+.045*cheek)
            dz=.90*mouth
            dy=.20*jaw*front
            if abs(x)<2.2 and z<151.0 and y>9:
                dz-=.35
                dy+=.20
            deltas.append(unreal.Vector(dx,dy,dz))
        sub.translate_face_landmarks(character=character,
            landmark_indices=list(range(len(points))),deltas=deltas)
        sub.commit_face_state(character)
        assets.set_metadata_tag(character,'BraxtonFaceFitVersion','1')
        assets.set_metadata_tag(character,'ProductionStatus','Reference proportion study. Skin, hair, rig and likeness approval pending; not gameplay-ready.')
    if not assets.save_loaded_asset(character): raise RuntimeError('Saving face fit failed')
    Path('/Users/Stewart/Documents/Codex/2026-09-11/c/work/braxton_metahuman_fitted_landmarks.json').write_text(json.dumps(
        [[p.x,p.y,p.z] for p in sub.get_face_landmarks(character)],indent=2))
    unreal.log('BRAXTON_FACE_FIT_SAVED: '+path)
finally:
    sub.remove_object_to_edit(character)

unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([character])
unreal.EditorAssetLibrary.sync_browser_to_objects([path])
unreal.log('BRAXTON_PREVIEW_OPENED')
