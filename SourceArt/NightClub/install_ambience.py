"""Called by the nightclub map installer; imports audio and configures ambience.

No editor launch, map load, or map save occurs here. The calling installer owns
the complete scene transaction and protected-actor audit. All coordinates below
are CURRENT saved-house world centimetres, so this actor has identity transform.
"""
from pathlib import Path
import json
import wave


def install_ambience(unreal, assets=None):
    project = Path(unreal.Paths.project_dir()).resolve()
    source = project / 'SourceArt' / 'NightClub' / 'audio'
    wav_file = source / 'NC_OriginalFiller_120BPM.wav'
    destination = '/Game/SeniorSendOff/House/NightClub'
    asset_name = 'NC_OriginalFiller_120BPM'
    actor_label = 'NightClub_Ambience'

    if not (project / 'SeniorSendoff.uproject').is_file():
        raise RuntimeError('Nightclub ambience requires the SeniorSendoff Unreal project')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or world.get_name() != 'Chapter01_House':
        raise RuntimeError('Caller must load Chapter01_House before installing nightclub ambience')
    runtime_class = unreal.load_class(None, '/Script/SeniorSendoff.HouseNightClub')
    if not runtime_class:
        raise RuntimeError('Build HouseNightClub before running the nightclub installer')
    if not wav_file.is_file():
        raise RuntimeError('Missing original nightclub filler WAV: ' + str(wav_file))

    with wave.open(str(wav_file), 'rb') as recording:
        sound_info = {
            'duration_seconds': recording.getnframes() / recording.getframerate(),
            'sample_rate': recording.getframerate(),
            'channels': recording.getnchannels(),
            'sample_width': recording.getsampwidth(),
        }
    if sound_info != {
        'duration_seconds': 16.0, 'sample_rate': 44100,
        'channels': 2, 'sample_width': 2,
    }:
        raise RuntimeError('Unexpected filler audio format: ' + repr(sound_info))

    task = unreal.AssetImportTask()
    task.filename = str(wav_file)
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.EditorAssetLibrary.load_asset(destination + '/' + asset_name)
    if not sound or not isinstance(sound, unreal.SoundWave):
        raise RuntimeError('Nightclub WAV did not import as a SoundWave')
    sound.set_editor_property('looping', True)
    sound.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
    # PCM is small for this one16s song (~2.7MiB), cheap to decode, and supports
    # seeking. PlatformSpecific would prevent reliable multiplayer late joins.
    sound.set_editor_property('sound_asset_compression_type', unreal.SoundAssetCompressionType.PCM)
    sound.set_editor_property('compression_quality', 80)
    sound.set_editor_property('volume', 1.0)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError('Could not save the looping nightclub SoundWave')

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    matches = [actor for actor in actors.get_all_level_actors()
               if actor.get_actor_label() == actor_label]
    if len(matches) > 1:
        raise RuntimeError('Duplicate nightclub ambience actors require review')
    if matches:
        club = matches[0]
        if club.get_class().get_path_name() != runtime_class.get_path_name():
            raise RuntimeError('Nightclub ambience label belongs to another actor class')
    else:
        club = actors.spawn_actor_from_class(runtime_class, unreal.Vector(0, 0, 0))
    if not club:
        raise RuntimeError('Could not create HouseNightClub')
    club.set_actor_label(actor_label)
    club.set_folder_path('House/NightClub/Runtime')
    club.set_actor_location(unreal.Vector(0, 0, 0), False, False)
    club.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
    club.set_actor_scale3d(unreal.Vector(1, 1, 1))

    settings = {
        'lower_room_min': unreal.Vector(-2212.51019, -1509.02675, .88),
        'lower_room_max': unreal.Vector(-1636.8, -973.5, 327.03),
        'upper_room_min': unreal.Vector(-2212.51019, -1509.02675, 332.53),
        'upper_room_max': unreal.Vector(-1636.8, -973.5, 622.71),
        'lower_door_center': unreal.Vector(-2228.35, -1375.2, 110),
        'upper_door_center': unreal.Vector(-2228.35, -1386, 448.58),
        'lower_door_inward_normal': unreal.Vector(1, 0, 0),
        'upper_door_inward_normal': unreal.Vector(1, 0, 0),
        'pole_hole_center': unreal.Vector(-1935, -1175, 332.53),
        'pole_hole_radius': 100.0,
        'door_fade_width': 95.0,
        'door_half_width': 100.0,
        'music_loop': sound,
        'music_bpm': 120.0,
        'music_loop_seconds': 16.0,
        'music_volume': .30,
        'hole_music_gain': .10,
        'moving_light_intensity': 155.0,
        'strobe_light_intensity': 35.0,
        'strobe_enabled': True,
        'club_effects_enabled': True,
    }
    for name, value in settings.items():
        club.set_editor_property(name, value)
    tags = list(club.get_editor_property('tags'))
    for tag in ('SSO_NightClub', 'SSO_NightClubAmbience'):
        if tag not in [str(existing) for existing in tags]:
            tags.append(unreal.Name(tag))
    club.set_editor_property('tags', tags)
    club.refresh_club()

    def vec(value):
        return [value.x, value.y, value.z]

    audit = {
        'actor': actor_label,
        'class': club.get_class().get_path_name(),
        'transform': {'location': vec(club.get_actor_location()),
                      'scale': vec(club.get_actor_scale3d())},
        'sound_asset': sound.get_path_name(),
        'sound_format': sound_info,
        'looping': bool(sound.get_editor_property('looping')),
        'loading_behavior': str(sound.get_editor_property('loading_behavior')),
        'compression': str(sound.get_editor_property('sound_asset_compression_type')),
        'bounds': {name: vec(club.get_editor_property(name))
                   for name in ('lower_room_min', 'lower_room_max',
                                'upper_room_min', 'upper_room_max')},
        'door_centers': {'lower': vec(club.get_editor_property('lower_door_center')),
                         'upper': vec(club.get_editor_property('upper_door_center'))},
        'pole_hole': {'center': vec(club.get_editor_property('pole_hole_center')),
                      'radius': club.get_editor_property('pole_hole_radius')},
        'light_count': len(club.get_editor_property('club_lights')),
        'music_bpm': club.get_editor_property('music_bpm'),
        'music_loop_seconds': club.get_editor_property('music_loop_seconds'),
    }
    if audit['light_count'] != 6 or not audit['looping']:
        raise RuntimeError('Nightclub ambience audit failed: ' + json.dumps(audit))
    unreal.log('NIGHTCLUB_AMBIENCE_CONFIGURED ' + json.dumps(audit))
    return {'actor': club, 'music': sound, 'audit': audit}
