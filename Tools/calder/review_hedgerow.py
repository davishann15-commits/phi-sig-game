"""Run opt-in hedge fallback checks without changing existing review contracts."""
from pathlib import Path
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FLAGS = ['-CalderWoodsTreeTrial', '-CalderEvergreenAtlasTrial',
         '-CalderRidgeForestTrial', '-CalderFallsJetsTrial', '-CalderFarmTrial',
         '-CalderRockStrataTrial', '-CalderFarmTreeTrial', '-CalderHedgerowTrial']
REVIEWS = ['-CalderHedgerowReview', '-CalderLandscapeReview',
           '-CalderTerrainReview', '-WoodsPassReview', '-CampusWoodsReview']
CASES = [
    ('off', [x for x in FLAGS if x != '-CalderHedgerowTrial'] + REVIEWS,
     ['instances=57704', 'allCalderISMs=24', 'CALDER_HEDGEROW_REVIEW PASS enabled=0'],
     ['CALDER_HEDGEROW PASS', 'CALDER_FARM_BARN_MERGE enabled=1']),
    ('noterrain', FLAGS + ['-NoCalderTerrain', '-CampusLayoutReview',
                          '-WoodsPassReview', '-CampusWoodsReview',
                          '-CalderLandscapeReview', '-CalderHedgerowReview'],
     ['grounds=69240', 'instances=53740', 'campusFrontVertices=1091586',
      'floorSamples=2654', 'allCalderISMs=0', 'CALDER_HEDGEROW_REVIEW PASS enabled=0'],
     ['CALDER_HEDGEROW PASS', 'CALDER_PERSIMMON_TRUNK', 'CALDER_ROCK_STRATA bound=']),
    ('nolandscape', FLAGS + REVIEWS + ['-NoCalderLandscape'],
     ['CALDER_TERRAIN_REVIEW PASS terrain=1 hash=72564759', 'allCalderISMs=3',
      'enabled=0 batches=0 instances=0', 'CALDER_HEDGEROW_REVIEW PASS enabled=0'],
     ['CALDER_HEDGEROW PASS', 'CALDER_PERSIMMON_TRUNK', 'CALDER_ROCK_STRATA bound=']),
    ('nofarm', [x for x in FLAGS if x != '-CalderFarmTrial'] + REVIEWS,
     ['instances=57815', 'allCalderISMs=19', 'CALDER_HEDGEROW_REVIEW PASS enabled=0'],
     ['CALDER_HEDGEROW PASS', 'CALDER_PERSIMMON_TRUNK', 'CALDER_FARM_BATCH']),
    ('server', FLAGS + ['-server', '-NullRHI', '-CalderLandscapeReview', '-CalderHedgerowReview'],
     ['CALDER_PERSIMMON_TRUNK PASS samples=96 hits=96', 'dedicated=1',
      'enabled=0 batches=0 instances=0', 'CALDER_HEDGEROW_REVIEW PASS enabled=0'],
     ['CALDER_HEDGEROW PASS', 'CALDER_FARM_BATCH']),
]

def main():
    results = {}
    for suffix, flags, expected, forbidden in CASES:
        name = 'codex_hedgerow_final_' + suffix
        run = ROOT / 'Saved/CalderWork/runs' / name
        assert not run.exists(), name
        print('START', name, flush=True)
        subprocess.run(['bash', str(ROOT / 'Saved/CalderWork/tools/run_game.sh'),
                        name, '210', '-RenderOffScreen', '-nosound', *flags],
                       cwd=ROOT, check=True)
        log = (run / 'Saved/Logs/SeniorSendoff.log').read_text(errors='replace')
        assert 'CALDER_LANDSCAPE_REVIEW PASS' in log, name
        assert not re.search(r'CALDER_\w+_REVIEW FAIL|CALDER_PERSIMMON_TRUNK FAIL|CALDER_FARM FAIL|Fatal error:', log), name
        for marker in expected:
            assert marker in log, (name, marker)
        for marker in forbidden:
            assert marker not in log, (name, marker)
        results[name] = {'passed': True, 'expected': expected,
                         'forbidden_absent': forbidden,
                         'log': str((run / 'Saved/Logs/SeniorSendoff.log').relative_to(ROOT))}
        receipt = ROOT / 'Saved/CalderWork/hedgerow_trial/fence_plants/final_gates.json'
        receipt.parent.mkdir(parents=True, exist_ok=True)
        receipt.unlink(missing_ok=True)
        receipt.write_text(json.dumps(results, indent=2) + '\n')
        print('PASS', name, flush=True)
    print('ALL FIVE HEDGEROW FALLBACK GATES PASS', flush=True)

if __name__ == '__main__':
    main()
