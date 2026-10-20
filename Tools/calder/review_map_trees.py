"""Run the map-wide tree fallback/server checks without changing old reviews."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT/'Saved/CalderWork'
assert len(sys.argv) == 3 and all(re.fullmatch(r'[A-Za-z0-9_-]+', name) for name in sys.argv[1:])
tag = sys.argv[1]
output = WORK/'map_tree_replacement'/(tag+'_gates.json')
assert not output.exists(), output
common = ['-RenderOffScreen', '-nosound', '-CalderWoodsTreeTrial', '-CalderEvergreenAtlasTrial',
          '-CalderRidgeForestTrial', '-CalderFallsJetsTrial', '-CalderFarmTrial', '-CalderRockStrataTrial',
          '-CalderFarmTreeTrial', '-CalderHedgerowTrial', '-CalderFarmSolidTrial',
          '-CalderMapTreesReview', '-CalderLandscapeReview']
cases = {'noterrain': ['-NoCalderTerrain', '-CampusLayoutReview', '-WoodsPassReview', '-CampusWoodsReview'],
         'nolandscape': ['-NoCalderLandscape', '-CalderTerrainReview', '-WoodsPassReview', '-CampusWoodsReview'],
         'server': ['-server', '-NullRHI']}
receipt = {'scope': 'Tree rollout fallbacks and native dedicated physics; not multiplayer or full-master acceptance',
           'launcher_sha256': hashlib.sha256((WORK/'tools/run_game.sh').read_bytes()).hexdigest(),
           'cases': [], 'passed': False}
for case, extra in cases.items():
    name = tag+'_'+case
    run = WORK/'runs'/name
    assert not run.exists(), run
    print('CALDER_MAP_TREES_GATE_START '+name, flush=True)
    args = ['bash', str(WORK/'tools/run_game.sh'), name, '300', *common, *extra]
    with (WORK/(name+'_launcher.log')).open('w') as stream:
        subprocess.run(args, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True)
    log = (run/'Saved/Logs/SeniorSendoff.log').read_text()
    native = (run/'Saved/CalderMapTreesReview/result.txt').read_text()
    landscape = (run/'Saved/CalderLandscapeReview/result.txt').read_text()
    assert native.startswith('PASS ') and 'CALDER_LANDSCAPE_REVIEW PASS ' in landscape, (name, native, landscape)
    assert not any(s in log for s in ['Assertion failed:', 'CALDER_MAP_TREE_FAILED', 'CALDER_TREE_FIT_REJECT']), name
    if case == 'server':
        assert 'enabled=1 ' in native and 'legacyBatches=0 ' in native
        reference = WORK/'runs'/sys.argv[2]/'Saved/CalderMapTreesReview'
        for file in ['poses.csv', 'bodies.csv']:
            assert (reference/file).read_bytes() == (run/'Saved/CalderMapTreesReview'/file).read_bytes(), (name, file)
    else:
        assert 'enabled=0 ' in native and 'replacements=0 ' in native
        if case == 'noterrain':
            for marker in ['grounds=69240', 'instances=53740', 'campusFrontVertices=1091586', 'floorSamples=2654', 'crc=0f3b2bb7']:
                assert marker in log, (name, marker)
            assert 'allCalderISMs=0 ' in landscape
        else:
            assert 'allCalderISMs=3 ' in landscape
    receipt['cases'].append({'name': name, 'native': native, 'landscape': landscape, 'arguments': args})
    output.unlink(missing_ok=True)
    output.write_text(json.dumps(receipt, indent=2)+'\n')
    print('CALDER_MAP_TREES_GATE_PASS '+name, flush=True)
receipt['passed'] = True
output.unlink()
output.write_text(json.dumps(receipt, indent=2)+'\n')
print('CALDER_MAP_TREES_GATES PASS cases='+str(len(cases)), flush=True)
