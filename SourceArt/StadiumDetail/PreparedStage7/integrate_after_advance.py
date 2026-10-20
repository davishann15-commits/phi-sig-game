"""Prepared mood/audio install; requires actual stage6 acceptance/advance7 first."""
from pathlib import Path
import hashlib, json, sys

ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / 'SourceArt/StadiumDetail'
PREP = Path(__file__).resolve().parent
sys.path.insert(0, str(ART))
from stage_runner import idle, manifest, NAMES, sha


def replace_once(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new, 1)


def write(path, text):
    if path.exists():
        path.unlink()
    path.write_text(text)


def main():
    idle()
    work = ROOT / 'Saved/StadiumDetailWork'
    src = ROOT / 'Source/SeniorSendoff'
    draft = ART / 'Draft'
    frozen = manifest()
    advance = json.loads((work / 'stage_6_accepted_advance_7.json').read_text())
    assert advance['new_default_stage'] == 7 and advance['new_source_config_map_sha'] == frozen
    assert 'int32 Step(){int32 Value=7;' in (src / 'HouseStadiumDetail.cpp').read_text()
    assert len(NAMES) == 34
    for path in draft.glob('HouseStadiumDetail*'):
        if path.suffix in ('.h', '.cpp'):
            assert path.read_bytes() == (src / path.name).read_bytes(), path.name
    specification = json.loads((PREP / 'mood_candidate_receipt.json').read_text())
    mood = PREP / 'HouseStadiumDetailMood_candidate.cpp'
    recorder = PREP / 'StadiumSoundReview_candidate.inc'
    assert hashlib.sha256(mood.read_bytes()).hexdigest() == specification['candidate_sha']
    assert hashlib.sha256(recorder.read_bytes()).hexdigest() == specification['prepared_recorder_inc_sha']
    assert hashlib.sha256((src / 'HouseStadiumDetailMood.cpp').read_bytes()).hexdigest() == specification['runtime_preimage_sha']
    destination = work / 'stage7_mood_integration.json'
    assert not destination.exists()

    header = src / 'HouseStadiumDetail.h'
    h = header.read_text()
    h = replace_once(h, 'class USoundWaveProcedural;', 'class USoundWaveProcedural;\nclass USoundSubmix;\nclass UWorld;')
    h = replace_once(h, 'int32 Step();', 'int32 Step();\nUSoundSubmix* SoundReviewMix(UWorld* World,int32 Voice);')
    h = replace_once(h, '    virtual void EndPlay(const EEndPlayReason::Type Reason) override;', '    virtual void EndPlay(const EEndPlayReason::Type Reason) override;\n    USoundSubmix* RegisteredSoundReviewMix(int32 Voice)const;')
    h = replace_once(h, 'private:\n', 'private:\n    USoundWaveProcedural* FreshMoodWave(int32 Voice);\n')
    h = replace_once(h, '    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Sounds;', '    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Sounds;\n    UPROPERTY(Transient) TArray<TObjectPtr<USoundSubmix>> SoundReviewSubmixes;')
    h = replace_once(h, '    TArray<FVector> ProgramOrigins;', '    TArray<FVector> ProgramOrigins;\n    TArray<FVector> ProgramAlong;\n    TArray<int32> QueuedSoundBytes;\n    int32 SoundGeneration=0;')

    fall = src / 'HouseCampusFall.cpp'
    f = fall.read_text()
    f = replace_once(f, '#include "Sound/SoundWaveProcedural.h"', '#include "Sound/SoundWaveProcedural.h"\n#include "Sound/SoundSubmix.h"')
    f = replace_once(f, '    Drums->SetSound(DrumWave);', '    if(auto* Mix=HouseStadiumDetail::SoundReviewMix(GetWorld(),3))DrumWave->SoundSubmixObject=Mix;\n    Drums->SetSound(DrumWave);')
    # Only the enabled stage7 branch is restrained/located beyond the empty field.
    f = replace_once(f, '    Drums->SetWorldLocation(FVector(HouseCampus::StadiumX, HouseCampus::StadiumY, 120.f));', '    Drums->SetWorldLocation(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7?FVector(10000,-11000,250):FVector(HouseCampus::StadiumX, HouseCampus::StadiumY, 120.f));')
    assert f.count('Step()>=7?.10f:1.f') == 2
    f = f.replace('Step()>=7?.10f:1.f', 'Step()>=7?.025f:1.f')

    views = json.loads((PREP / 'program_close_view.json').read_text())
    assert views['introduced_stage'] == 7 and len(views['views']) == 1
    view = views['views'][0]
    assert view['eye'][2] - view['floor_z'] == 170
    camera = src / 'HouseExterior.cpp'
    c = camera.read_text()
    assert view['name'] not in c
    last = next(line for line in c.splitlines() if 'TEXT("detail_stadium_wood_lockers")' in line)
    vector = lambda values: 'FVector(' + ','.join(f'{v:.3f}' for v in values) + ')'
    lines = ['    // ' + views['added_comment'], '    { TEXT("' + view['name'] + '"), ' + vector(view['eye']) + ', ' + vector(view['target']) + ', ' + str(view['fov']) + '.f },']
    new_camera = replace_once(c, last, last + '\n' + '\n'.join(lines))
    assert [line for line in new_camera.splitlines() if line not in lines] == c.splitlines()

    review = src / 'HouseStadiumDetailReview.cpp'
    r = review.read_text()
    marker = '  if(HouseStadiumDetail::Step()>=6){MinTread='
    assert r.count(marker) == 1
    pose = '{' + ','.join(f'{v:.3f}' for v in [*view['eye'][:2], view['floor_z']]) + '}'
    audit = '  if(HouseStadiumDetail::Step()>=7){const FVector P=' + pose + ';CheckFloor(P,TEXT("moving-program-view-floor"));FHitResult H;++Sweeps;if(W->SweepSingleByChannel(H,P+FVector(0,0,97),P+FVector(0,0,98),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,96),Q)){++Blocked;OK=false;if(Problems.Num()<32)Problems.Add(FString::Printf(TEXT("program view capsule blocked at=%s component=%s"),*P.ToString(),*GetNameSafe(H.GetComponent())));}}\n'
    r = r.replace(marker, audit + marker, 1)

    runner = ART / 'stage_runner.py'
    s = runner.read_text()
    needle = "    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/interior_close_views.json').read_text())['views']]"
    s = replace_once(s, needle, needle + " + [\n    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/program_close_view.json').read_text())['views']]")
    recovery = ART / 'recovery.py'
    rec = recovery.read_text()
    rec = replace_once(rec, '31 if int(stage) == 5 else 34)', '31 if int(stage) == 5 else 34 if int(stage) == 6 else 35)')
    needle = "    added_names = ('detail_stadium_portal_east', 'detail_stadium_portal_west', *[view['name'] for view in field_views['views']], *[view['name'] for view in concourse_views['views']], *[view['name'] for view in interior_views['views']])"
    rec = replace_once(rec, needle, "    program_views = json.loads((ROOT / 'SourceArt/StadiumDetail/program_close_view.json').read_text())\n" + needle[:-1] + ", *[view['name'] for view in program_views['views']])")
    rec = replace_once(rec, '== 15\n', '== 16\n')
    rec = replace_once(rec, "and interior_views['added_comment'] not in line]", "and interior_views['added_comment'] not in line\n                and program_views['added_comment'] not in line]")

    # Final requested sign alignment: the existing Geese wall mark must sit above the new oak cap.
    crest = json.loads((PREP / 'room_crest_finish.json').read_text())
    legacy = src / 'HouseCampusInteriorsStadium.cpp'
    look_file = src / 'HouseStadiumDetailLook.cpp'
    assert sha(legacy) == crest['legacy_preimage_sha'] and sha(look_file) == crest['look_preimage_sha']
    legacy_after = replace_once(legacy.read_text(), crest['legacy_old'], crest['legacy_new'])
    look_after = replace_once(look_file.read_text(), crest['look_old'], crest['look_new'])

    # Direct owner follow-up: darker and more worn timber, applied in the final finish stage.
    wood = json.loads((PREP / 'wood_age_finish.json').read_text())
    interior = src / 'HouseStadiumDetailInteriors.cpp'
    assert sha(interior) == wood['interior_preimage_sha']
    interior_after = interior.read_text()
    for item in wood['interior_edits']:interior_after = replace_once(interior_after,item['old'],item['new'])
    for item in wood['look_edits']:look_after = replace_once(look_after,item['old'],item['new'])

    # All preconditions above are read-only. Every runtime write below has a fresh mtime.
    for path, content in ((header,h), (src / 'HouseStadiumDetailMood.cpp',mood.read_text()+'\n'+recorder.read_text()), (review,r)):
        write(path, content)
        write(draft / path.name, content)
    write(fall, f)
    write(legacy, legacy_after)
    write(interior, interior_after)
    write(draft / interior.name, interior_after)
    write(look_file, look_after)
    write(draft / look_file.name, look_after)
    write(camera, new_camera)
    write(draft / 'program_view.inl', '\n'.join(lines)+'\n')
    write(ART / 'program_close_view.json', json.dumps(views,indent=2)+'\n')
    write(runner, s)
    write(recovery, rec)
    destination.write_text(json.dumps({'stage':7, 'before_source_config_map_sha':frozen,
        'after_source_config_map_sha':manifest(), 'all_prior_camera_lines_preserved':True,
        'legacy_routes_changed':False, 'added_views':views, 'build_pending':True,
        'audio_temporal_native_acceptance_pending':True,
        'snare_stage7_gain':.025, 'snare_stage7_position':[10000,-11000,250]},indent=2)+'\n')
    print('Prepared mood/audio candidate installed. Build, actual audio/temporal review, all35 views and all final gates remain.')


if __name__ == '__main__':
    main()
