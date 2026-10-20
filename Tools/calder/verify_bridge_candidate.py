"""Independent numeric checks for the isolated Ashby Lane authority candidate.

Does not replace the full authority review, collision review or real walking.
No source, map, test limit or baseline is modified.
"""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / 'Saved/CalderWork/plan/landform.py'
PRE_ROAD = ROOT / 'Saved/CalderWork/codex_bridge_pre_promote_20261016_2048/landform.py'
CANDIDATE = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'Saved/CalderWork/bridge_authority_candidate/landform.py'


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


base = load(PRE_ROAD if PRE_ROAD.exists() else PLAN, 'ashby_before')
assert not hasattr(base, 'ASHBY_ROAD_WEST'), 'A preserved pre-road baseline is required.'
land = load(CANDIDATE, 'ashby_candidate')
rows = []


def check(name, passed, detail):
    rows.append({'name': name, 'status': 'PASS' if passed else 'FAIL', 'detail': detail})
    print(rows[-1], flush=True)


def inside(point, polygon):
    signs = [(b[0]-a[0])*(point[1]-a[1])-(b[1]-a[1])*(point[0]-a[0])
             for a, b in zip(polygon, polygon[1:]+polygon[:1])]
    return not (min(signs) < -.001 and max(signs) > .001)


export = land.exports()
roads = export['roads']
for road, curve in zip(roads[:2], (land.ASHBY_ROAD_WEST, land.ASHBY_ROAD_EAST)):
    cum = land.polyline_cum(road['points'])
    length = cum[-1]
    worst = (0., None)
    collisions = []
    count = 0
    for across in (-275., 0., 275.):
        previous = None
        for k in range(math.ceil(length/10.)+1):
            s = min(k*10., length)
            p, tangent = land.polyline_at(road['points'], cum, s)
            x, y = p[0]-tangent[1]*across, p[1]+tangent[0]*across
            # Same published finished-paving datum, independently evaluated.
            offset = 3.-15.*(1.-land.S(s/300.))
            if road['mode'] == 0:
                offset -= 15.*land.S((s-(length-300.))/300.)
            z = land.ground(x, y)+offset
            if previous:
                distance = math.dist((x, y), previous[:2])
                grade = abs(z-previous[2])/distance*100. if distance > .001 else 0.
                worst = max(worst, (grade, (x, y, across)), key=lambda row: row[0])
            previous = (x, y, z)
            count += 1
    for k in range(math.ceil(length/10.)+1):
        p, tangent = land.polyline_at(road['points'], cum, min(k*10., length))
        for across in range(-320, 321, 20):
            x, y = p[0]-tangent[1]*across, p[1]+tangent[0]*across
            building = land.footprint_at(x, y)
            if building:
                collisions.append((x, y, building))
    check(road['name']+' finished paving <=12%', worst[0] <= 12., {'samples': count, 'worst': worst})
    check(road['name']+' road and level verges clear buildings', not collisions,
          {'stations_cm': 10, 'width_step_cm': 20, 'intersections': collisions[:10]})

survey = json.loads((ROOT/'SourceArt/CalderLandscape/phase1/stair_surfaces.json').read_text())
overlaps = [(stair['name'], endpoint, road['name']) for stair in survey['stairs'] for endpoint in ('foot', 'head')
            for road in roads if any(inside(stair[endpoint], polygon) for polygon in road['polygons'])]
check('all 78 existing stair endpoint paving datums remain outside new roads', not overlaps, overlaps)

bed = [(x, y) for x in range(24800, 25201, 20) for y in range(-7300, -6699, 20)]
worst_bed = max(abs(base.ground(x, y)-land.ground(x, y)) for x, y in bed)
check('creek bed preserved under bridge', worst_bed == 0., {'samples': len(bed), 'max_change_cm': worst_bed})

crossing = next(row for row in export['crossings'] if row['kind'] == 'road')
check('bridge centreline joins at finished paving level', max(abs(z-crossing['deck']) for z in crossing['ends']) <= .5,
      {'ends': crossing['ends'], 'deck': crossing['deck']})
clearances = []
for x, y in bed:
    along, across = x-crossing['centre'][0], y-crossing['centre'][1]
    soffit = crossing['deck']-35.-300.*(along/1000.)**2-.02*min(abs(across), 275.)
    clearances.append(soffit-land.ground(x, y))
check('central creek bed has standing clearance under arch', min(clearances) >= 200.,
      {'samples': len(clearances), 'min_clearance_cm': min(clearances), 'required_cm': 200.})

report = {'candidate_sha256': hashlib.sha256(CANDIDATE.read_bytes()).hexdigest(),
          'baseline_sha256': hashlib.sha256((PRE_ROAD if PRE_ROAD.exists() else PLAN).read_bytes()).hexdigest(), 'checks': rows,
          'status': 'FAIL' if any(row['status'] == 'FAIL' for row in rows) else 'PASS'}
output = CANDIDATE.parent/'road_checks.json'
output.write_text(json.dumps(report, indent=2))
raise SystemExit(report['status'] != 'PASS')
