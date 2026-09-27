"""Fit Runner's own MetaHuman against the front, profile, and back references.

The eyes are hidden by glasses, so this fits the visible silhouette and does
not invent precise eye shape or an exact real-world height.
"""
import json
import math
from pathlib import Path
import unreal

PATH = '/Game/Characters/MetaHumans/Runner/MH_Runner_Working'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(PATH)
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not character or not sub.try_add_object_to_edit(character):
    raise RuntimeError('Runner source MetaHuman unavailable')

try:
    if assets.get_metadata_tag(character, 'RunnerFitVersion') == '1':
        unreal.log('RUNNER_META_ALREADY_FITTED')
    else:
        backup = '/Game/Characters/MetaHumans/Runner/MH_Runner_BeforeFit'
        if not assets.does_asset_exist(backup):
            copy = assets.duplicate_asset(PATH, backup)
            if not copy or not assets.save_loaded_asset(copy):
                raise RuntimeError('Could not back up Runner source')

        constraints = sub.get_body_constraints(character)
        targets = {
            # 190 cm is art direction only. The photographs have no reliable ruler.
            'Height': 190.0,
            'Inseam': 83.0,
            'Shoulder Height': 163.0,
            'Chest': 96.0,
            'Waist': 77.0,
            'Hip': 94.0,
            'Thigh': 53.0,
            'Calf': 36.0,
            'Across Shoulder': 39.0,
            'Neck to Waist': 41.0,
        }
        for constraint in constraints:
            if constraint.name in targets:
                constraint.is_active = True
                constraint.target_measurement = targets[constraint.name]
        sub.set_body_constraints(character, constraints)
        sub.commit_body_state(character)
        unreal.log('RUNNER_BODY_FIT ' + str([(c.name, c.target_measurement) for c in constraints if c.is_active]))

        before = sub.get_face_landmarks(character)
        if len(before) != 79:
            raise RuntimeError('Unexpected MetaHuman face landmark topology')
        # Conservative visible-feature fit: Runner has a narrower/longer face
        # than the broad default, a compact mouth, and a modest straight nose.
        # Use geometry-aware weights so the fit remains symmetric and smooth.
        offsets = []
        for p in before:
            x, y, z = p.x, p.y, p.z
            cheek = max(0.0, 1.0 - abs(z - 158.5) / 4.5)
            jaw = max(0.0, 1.0 - abs(z - 153.0) / 5.0)
            forehead = max(0.0, 1.0 - abs(z - 166.0) / 5.0)
            front = max(0.0, min(1.0, (y - 3.0) / 7.0))
            dx = -x * (0.040 * cheek + 0.048 * jaw + 0.012 * forehead)
            dz = -0.38 * jaw + 0.28 * forehead
            dy = 0.10 * cheek * front
            # Slightly project the central bridge/tip in the profile image.
            if abs(x) < 2.0 and 158.0 < z < 164.5 and y > 8.0:
                dy += 0.22
            offsets.append(unreal.Vector(dx, dy, dz))
        sub.translate_face_landmarks(character, list(range(len(before))), offsets)
        sub.commit_face_state(character)
        after = sub.get_face_landmarks(character)
        drift = max(math.dist((a.x,a.y,a.z),(b.x,b.y,b.z)) for a,b in zip(before, after))
        if drift > 1.25:
            raise RuntimeError('Face fit drift too large: ' + str(drift))

        Path('/Users/Stewart/Documents/Codex/2026-09-11/c/work/runner/runner_meta_landmarks.json').write_text(
            json.dumps({'before': [[p.x,p.y,p.z] for p in before],
                        'after': [[p.x,p.y,p.z] for p in after],
                        'max_delta_cm': drift}, indent=2))
        assets.set_metadata_tag(character, 'RunnerFitVersion', '1')
        assets.set_metadata_tag(character, 'HeightArtDirectionCm', '190')
        assets.set_metadata_tag(character, 'ProductionStatus',
            'Photo-guided MetaHuman source; glasses obscure eye details; height is provisional')
        if not assets.save_loaded_asset(character):
            raise RuntimeError('Could not save fitted Runner')
        unreal.log('RUNNER_META_FIT_SAVED max_face_delta_cm=' + str(drift))
finally:
    sub.remove_object_to_edit(character)
