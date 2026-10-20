"""Install three additional interior views only after actual stage5 acceptance.

Prepared recipe; run after advance_stage.py has changed default5 to6, with no
engine/build running. All earlier camera poses and native reviews stay exact.
"""
from pathlib import Path
import hashlib, json, sys

ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / 'SourceArt/StadiumDetail'
sys.path.insert(0, str(ART))
from stage_runner import idle, manifest, NAMES


def replace_file(path, data):
    if path.exists():
        path.unlink()
    path.write_text(data)


def main():
    idle()
    work = ROOT / 'Saved/StadiumDetailWork'
    src = ROOT / 'Source/SeniorSendoff'
    draft = ART / 'Draft'
    advance = json.loads((work / 'stage_5_accepted_advance_6.json').read_text())
    frozen = manifest()
    destination = work / 'stage6_interior_integration.json'
    assert not destination.exists()
    assert advance['new_default_stage'] == 6 and advance['new_source_config_map_sha'] == frozen
    assert 'int32 Step(){int32 Value=6;' in (src / 'HouseStadiumDetail.cpp').read_text()
    assert len(NAMES) == 31
    for path in draft.glob('HouseStadiumDetail*'):
        if path.suffix in ('.h', '.cpp'):
            assert path.read_bytes() == (src / path.name).read_bytes(), path.name
    views = json.loads((Path(__file__).parent / 'close_views.json').read_text())
    assert views['introduced_stage'] == 6 and len(views['views']) == 3
    interior_spec = json.loads((Path(__file__).parent / 'interior_candidate_receipt.json').read_text())
    interior_candidate = Path(__file__).parent / 'HouseStadiumDetailInteriors_candidate.cpp'
    interior = src / 'HouseStadiumDetailInteriors.cpp'
    assert hashlib.sha256(interior_candidate.read_bytes()).hexdigest() == interior_spec['candidate_sha']
    assert hashlib.sha256(interior.read_bytes()).hexdigest() == interior_spec['runtime_preimage_sha']
    light_spec = json.loads((Path(__file__).parent / 'stadium_light_anchor_fix.json').read_text())
    legacy_stadium = src / 'HouseCampusInteriorsStadium.cpp'
    assert hashlib.sha256(legacy_stadium.read_bytes()).hexdigest() == light_spec['runtime_preimage_sha']
    legacy_stadium_after = legacy_stadium.read_text()
    for item in light_spec['replacements']:
        assert legacy_stadium_after.count(item['old']) == 1
        legacy_stadium_after = legacy_stadium_after.replace(item['old'], item['new'], 1)

    camera = src / 'HouseExterior.cpp'
    before = camera.read_text()
    assert all(view['name'] not in before for view in views['views'])
    last = next(line for line in before.splitlines() if 'TEXT("detail_stadium_dedication")' in line)
    vector = lambda values: 'FVector(' + ','.join(f'{v:.3f}' for v in values) + ')'
    lines = ['    // ' + views['added_comment']]
    for view in views['views']:
        assert view['eye'][2] - view['floor_z'] == 170
        lines.append('    { TEXT("' + view['name'] + '"), ' + vector(view['eye']) + ', ' + vector(view['target']) + ', ' + str(view['fov']) + '.f },')
    after = before.replace(last, last + '\n' + '\n'.join(lines), 1)
    assert [line for line in after.splitlines() if line not in lines] == before.splitlines()

    review = src / 'HouseStadiumDetailReview.cpp'
    review_before = review.read_text()
    marker = '  if(HouseStadiumDetail::Step()>=6)'
    assert review_before.count(marker) == 1
    poses = ','.join('{' + ','.join(f'{v:.3f}' for v in [*view['eye'][:2], view['floor_z']]) + '}' for view in views['views'])
    audit = '  if(HouseStadiumDetail::Step()>=6){const FVector Views[]={' + poses + '};for(const FVector& P:Views){CheckFloor(P,TEXT("interior-close-view-floor"));FHitResult H;++Sweeps;if(W->SweepSingleByChannel(H,P+FVector(0,0,97),P+FVector(0,0,98),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,96),Q)){++Blocked;OK=false;if(Problems.Num()<32)Problems.Add(FString::Printf(TEXT("interior view capsule blocked at=%s component=%s"),*P.ToString(),*GetNameSafe(H.GetComponent())));}}}\n'
    review_after = review_before.replace(marker, audit + marker, 1)

    runner = ART / 'stage_runner.py'
    runner_before = runner.read_text()
    needle = "    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/concourse_close_views.json').read_text())['views']]"
    assert runner_before.count(needle) == 1
    runner_after = runner_before.replace(needle, needle + " + [\n    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/interior_close_views.json').read_text())['views']]", 1)

    recovery = ART / 'recovery.py'
    recovery_before = recovery.read_text()
    old_count = '(21 if int(stage) < 4 else 28 if int(stage) == 4 else 31)'
    assert recovery_before.count(old_count) == 1
    recovery_after = recovery_before.replace(old_count, '(21 if int(stage) < 4 else 28 if int(stage) == 4 else 31 if int(stage) == 5 else 34)', 1)
    needle = "    added_names = ('detail_stadium_portal_east', 'detail_stadium_portal_west', *[view['name'] for view in field_views['views']], *[view['name'] for view in concourse_views['views']])"
    assert recovery_after.count(needle) == 1
    recovery_after = recovery_after.replace(needle, "    interior_views = json.loads((ROOT / 'SourceArt/StadiumDetail/interior_close_views.json').read_text())\n" + needle[:-1] + ", *[view['name'] for view in interior_views['views']])", 1)
    assert recovery_after.count('== 12\n') == 1
    recovery_after = recovery_after.replace('== 12\n', '== 15\n', 1)
    needle = "and concourse_views['added_comment'] not in line]"
    assert recovery_after.count(needle) == 1
    recovery_after = recovery_after.replace(needle, "and concourse_views['added_comment'] not in line\n                and interior_views['added_comment'] not in line]", 1)

    # All preconditions are checked above, before any runtime mutation.
    replace_file(camera, after)
    replace_file(review, review_after)
    replace_file(draft / review.name, review_after)
    replace_file(interior, interior_candidate.read_text())
    replace_file(draft / interior.name, interior_candidate.read_text())
    replace_file(legacy_stadium, legacy_stadium_after)
    replace_file(ART / 'interior_close_views.json', json.dumps(views, indent=2) + '\n')
    replace_file(draft / 'interior_views.inl', '\n'.join(lines) + '\n')
    replace_file(runner, runner_after)
    replace_file(recovery, recovery_after)
    receipt = {'stage': 6, 'before_source_config_map_sha': frozen,
               'after_source_config_map_sha': manifest(), 'added_views': views,
               'all_prior_camera_lines_preserved': True, 'legacy_routes_changed': False,
               'new_floor_and_capsule_checks': 3, 'build_pending': True,
               'interior_geometry_candidate': interior_spec,
               'stadium_only_light_anchor_fix': light_spec}
    destination.write_text(json.dumps(receipt, indent=2) + '\n')
    print('Three interior close views installed; build then all34 dusk/night views and actual floor/capsule proof required.')


if __name__ == '__main__':
    main()
