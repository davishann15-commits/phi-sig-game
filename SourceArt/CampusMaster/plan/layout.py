"""Calder State master layout (Phase 1) as data, with a checker and a plan renderer.

Coordinates: Unreal cm, ground -60 on the campus side. Rects are (x0, x1, y0, y1). Faces: N = +Y (toward the street and the
house), S = -Y, E = +X, W = -X. The checker enforces the master prompt's fixed anchors and review invariants.
"""
import json, math, sys
sys.path.insert(0, '.')
from plandraw import Canvas

TREES = json.load(open('campus_trees.json'))

# ------------------------------------------------------------------ fixed anchors
STADIUM = (2400, -13300, 5600, 3800)            # centre and outer radii of the bowl wall
TOWER_PLINTH = (-9395, -8605, -11195, -10405)
STREET = (-32000, 8700, -5550, -4150)
FAR_SIDEWALK = (-32000, 8700, -5800, -5550)
FREE_ROAM_CORRIDOR = (3000, 3200, -7000, -5000)
CAMERAS = {'detail_clocktower_base': (-8480, -9720), 'detail_clocktower_mid': (-420, -8100), 'high': (7000, -8000),
           'front_lawn_overview': (4600, -6050), 'detail_campus_quad': (7000, -6400), 'detail_campus_kiosk': (3150, -6020)}
HOMECOMING_KEEP = {'kiosk plaza': (2100, 2700, -6630, -6030), 'gate path': (2250, 2550, -8930, -6600), 'booths+gates': (1400, 3500, -9300, -8800),
                   'bonfire': (3330, 4170, -8120, -7280), 'float': (3800, 5600, -8800, -7700), 'trailer': (300, 1700, -8600, -7700)}
TOWER_SIGHT = ((2400, -3750), (-9000, -10800))   # z limit = 140 + 0.287*d (cm, d from the camera)

# ------------------------------------------------------------------ the plan
# kind: library, admin, dorm, health, dining, science, humanities, union, classroom, athletics, arts, music, engineering,
#       plant, chapel. tier: A/B/C. style: 0 brick, 1 limestone, 2 warm brick. faces: entrance faces in order of importance.
BUILDINGS = [
    dict(id='LIB',  name='MERCER LIBRARY',        kind='library',     tier='A', rect=(-19000, -15600, -13300, -8300), h=2120, style=1, faces='E',  legacy=0),
    dict(id='MAIN', name='OLD MAIN',              kind='admin',       tier='B', rect=(-15200, -11400, -9300, -6700),  h=2540, style=0, faces='NS', legacy=2),
    dict(id='HAR',  name='HARLOW HALL',           kind='dorm',        tier='A', rect=(-10600, -6800, -9200, -6600),   h=2540, style=1, faces='N',  legacy=1),
    dict(id='HLTH', name='STUDENT HEALTH CENTER', kind='health',      tier='C', rect=(-4200, -1800, -8300, -6700),    h=1260, style=2, faces='NW', legacy=4),
    dict(id='COM',  name='THE COMMONS',           kind='dining',      tier='B', rect=(11000, 14200, -9600, -6700),    h=1300, style=2, faces='WN', legacy=3),
    dict(id='ASH',  name='ASHBY HALL',            kind='dorm',        tier='B', rect=(14700, 18900, -10300, -6700),   h=2540, style=0, faces='NW', legacy=5),
    dict(id='SCI',  name='HOLLIS SCIENCE HALL',   kind='science',     tier='A', rect=(-15200, -10600, -17400, -14200), h=2540, style=2, faces='N', legacy=6),
    dict(id='HUM',  name='CALDER HALL',           kind='humanities',  tier='B', rect=(-19000, -15800, -18600, -14400), h=2120, style=0, faces='E', legacy=7),
    dict(id='UNI',  name='GOOSE UNION',           kind='union',       tier='A', rect=(-8200, -4300, -14600, -10700),  h=1700, style=1, faces='WN', legacy=8),
    dict(id='KER',  name='KERR HALL',             kind='classroom',   tier='B', rect=(11000, 14400, -15400, -11400),  h=2540, style=2, faces='W', legacy=9),
    dict(id='FLD',  name='VARSITY FIELDHOUSE',    kind='athletics',   tier='C', rect=(15000, 18900, -16800, -12000),  h=1700, style=0, faces='W', legacy=10),
    dict(id='ART',  name='WHITCOMB FINE ARTS',    kind='arts',        tier='C', rect=(-13250, -9050, -21000, -18000), h=1700, style=1, faces='N', legacy=11),
    dict(id='MUS',  name='BAND AND MUSIC HALL',   kind='music',       tier='C', rect=(-7750, -4150, -20250, -17450),  h=1500, style=2, faces='NE', legacy=12),
    dict(id='ENG',  name='PRATT ENGINEERING',     kind='engineering', tier='C', rect=(-10000, -6400, -24750, -21950), h=2120, style=0, faces='N', legacy=13),
    dict(id='PLT',  name='PHYSICAL PLANT',        kind='plant',       tier='C', rect=(11750, 15950, -21250, -18250),  h=1300, style=2, faces='W', legacy=14),
    dict(id='ALU',  name='ALUMNI HALL',           kind='chapel',      tier='C', rect=(-19000, -15600, -24500, -21800), h=1500, style=1, faces='N', legacy=15),
]

# Roads (polylines with a width) and paved areas. Top Z -58 (lots and campus roads), walks -56.5.
ROADS = [
    dict(id='CALDER DR (BOULEVARD)', pts=[(9500, -5550), (9500, -9600)], w=1600, median=(9420, 9580, -9300, -6200)),
    dict(id='CALDER DR', pts=[(9500, -9600), (9500, -17200)], w=1600),
    dict(id='CALDER DR CURVE', arc=((7700, -17200), 1800, 0, -90), w=1600),        # centre, radius, from deg, to deg
    dict(id='STADIUM DR', pts=[(7700, -19000), (-1000, -19000)], w=1600),
    dict(id='BUS LOOP', circle=((-1000, -19300), 1100), w=0),
]
LOTS = [
    dict(id='SOUTH LOT', rect=(-1400, 8300, -25200, -20700), entry=(3600, 4400, -20700, -19800),
         islands=[(3034, -21572), (7347, -21896), (6958, -24532), (5559, -24805), (-884, -24705)]),
    dict(id='HEALTH VISITOR LOT', rect=(-6400, -4400, -8400, -6500), entry=(-5000, -4400, -6500, -5800), islands=[]),
    dict(id='ASHBY PERMIT LOT', rect=(15000, 18900, -11800, -10900), entry=(14400, 15000, -11600, -11100), islands=[]),
]
WALKS = [  # (pts, width)
    ([(-19000, -6500), (8450, -6500)], 300),            # front promenade (west of the drive)
    ([(10550, -6500), (19000, -6500)], 300),            # front promenade (east of the drive)
    ([(-15600, -10800), (-9395, -10800)], 400),         # quad axis: library steps to the tower plaza
    ([(-8605, -10800), (-8200, -10800)], 400),          # tower plaza to the union's west door (short)
    ([(-13300, -6650), (-13300, -6500)], 300),          # Old Main north door
    ([(-13300, -9300), (-13300, -14200)], 300),         # Old Main south door - quad - Science Hall
    ([(-15200, -13600), (-10200, -9600)], 250),         # quad diagonals
    ([(-15200, -9600), (-10200, -13600)], 250),
    ([(-8700, -6600), (-8700, -6500)], 300),            # Harlow north door
    ([(-11000, -9200), (-11000, -6500)], 250),          # between Old Main and Harlow to the quad
    ([(-17400, -13300), (-17400, -14400)], 250),        # library - Calder Hall
    ([(-6250, -10700), (-6250, -9300), (-8700, -9300)], 250),   # union north door to Harlow's back and the tower plaza
    ([(-4300, -13300), (-3300, -13300)], 300),          # union east to the stadium's west gate (K20)
    ([(-6000, -14600), (-6000, -17450)], 250),          # union south to the band hall
    ([(-9050, -19500), (-7750, -19500)], 250),          # fine arts - band hall
    ([(-11150, -18000), (-11150, -17400)], 250),        # fine arts - science hall
    ([(-8200, -21950), (-8200, -21000)], 250),          # engineering - fine arts
    ([(-17300, -21800), (-17300, -18600)], 250),        # alumni hall - calder hall
    ([(-15600, -23000), (-10000, -23000)], 250),        # alumni - engineering
    ([(-3100, -8300), (-3100, -9400), (-1200, -9800)], 250),    # health center to the stadium forecourt
    ([(10550, -8000), (11000, -8000)], 300),            # commons west door
    ([(14200, -8000), (14700, -8000)], 300),            # commons - ashby
    ([(10550, -13400), (11000, -13400)], 300),          # kerr west door
    ([(10550, -14400), (15000, -14400)], 250),          # kerr - fieldhouse
    ([(8000, -13300), (8450, -13300)], 250),            # stadium east gate (K0) to the drive sidewalk
    ([(2839, -17100), (2839, -18200)], 400),            # stadium south gate to the bus bay
    ([(10550, -19700), (11750, -19700)], 250),          # plant
]
SIDEWALKS = [  # along Calder Drive (both sides) to the curve
    ([(8575, -5800), (8575, -17200)], 250), ([(10425, -5800), (10425, -17200)], 250),
]
PLAZAS = [  # (centre, radius) paved circles
    ((-9000, -10800), 900),      # clock tower plaza
    ((-15400, -10800), 600),     # library steps forecourt
]
GATEPOSTS = [(8350, -6150), (10650, -6150)]   # brick piers flanking the entrance; sign wall east of the drive
SOUTH_EDGE = (-32000, 32000, -29800)          # visible fence + hedgerow line (collides) before the void at -32000

# ------------------------------------------------------------------ checker
def inside(px, py, r, m=0):
    return r[0] - m <= px <= r[1] + m and r[2] - m <= py <= r[3] + m

def overlap(a, b, m=0):
    return a[0] - m < b[1] and b[0] - m < a[1] and a[2] - m < b[3] and b[2] - m < a[3]

def ellipse_r(px, py):
    cx, cy, rx, ry = STADIUM
    return math.hypot((px - cx) / rx, (py - cy) / ry)

def seg_rect(p, q, w):
    x0, x1 = sorted((p[0], q[0])); y0, y1 = sorted((p[1], q[1]))
    return (x0 - w / 2, x1 + w / 2, y0 - w / 2, y1 + w / 2)

def road_rects():
    out = []
    for r in ROADS:
        if 'pts' in r:
            for p, q in zip(r['pts'], r['pts'][1:]): out.append((r['id'], seg_rect(p, q, r['w'])))
        if 'arc' in r:
            (cx, cy), rad, a0, a1 = r['arc']
            for k in range(12):
                t0 = math.radians(a0 + (a1 - a0) * k / 12); t1 = math.radians(a0 + (a1 - a0) * (k + 1) / 12)
                p = (cx + rad * math.cos(t0), cy + rad * math.sin(t0)); q = (cx + rad * math.cos(t1), cy + rad * math.sin(t1))
                out.append((r['id'], seg_rect(p, q, r['w'])))
        if 'circle' in r:
            (cx, cy), rad = r['circle']; out.append((r['id'], (cx - rad, cx + rad, cy - rad, cy + rad)))
    return out

def check():
    problems, hidden = [], set()
    R = road_rects()
    for i, b in enumerate(BUILDINGS):
        r = b['rect']
        if r[3] > -5550: problems.append(f"{b['id']} reaches Y >= -5550 (ExteriorNear rule)")
        for j, c in enumerate(BUILDINGS[i + 1:]):
            if overlap(r, c['rect'], 300): problems.append(f"{b['id']} within 3 m of {c['id']}")
        for rid, rr in R:
            if overlap(r, rr, 200): problems.append(f"{b['id']} within 2 m of road {rid}")
        for l in LOTS:
            if overlap(r, l['rect'], 200): problems.append(f"{b['id']} within 2 m of lot {l['id']}")
        if overlap(r, TOWER_PLINTH, 300): problems.append(f"{b['id']} within 3 m of the tower plinth")
        if overlap(r, FREE_ROAM_CORRIDOR, 100): problems.append(f"{b['id']} blocks the free-roam corridor")
        for k, a in HOMECOMING_KEEP.items():
            if overlap(r, a, 100): problems.append(f"{b['id']} hits homecoming {k}")
        for cam, p in CAMERAS.items():
            if inside(p[0], p[1], r, 200): problems.append(f"{b['id']} contains camera {cam}")
        # stadium: sample the rect against the wall ellipse with a 4 m margin
        for k in range(21):
            for m in range(21):
                px = r[0] + (r[1] - r[0]) * k / 20; py = r[2] + (r[3] - r[2]) * m / 20
                if ellipse_r(px, py) < 1.0 + 400 / 3800:
                    problems.append(f"{b['id']} within 4 m of the stadium wall"); break
            else: continue
            break
        # clock tower sight line height
        (ax, ay), (bx, by) = TOWER_SIGHT
        for k in range(101):
            t = k / 100; px = ax + (bx - ax) * t; py = ay + (by - ay) * t
            if inside(px, py, r):
                zlim = 140 + .287 * math.hypot(px - ax, py - ay)
                if b['h'] - 60 > zlim: problems.append(f"{b['id']} (top {b['h'] - 60}) rises into the tower sight line (limit {zlim:.0f})")
                break
        for t in TREES:
            if inside(t['x'], t['y'], r, 120 * t['s']): hidden.add((t['id'], b['id']))
    for rid, rr in R:
        for t in TREES:
            if inside(t['x'], t['y'], rr, 60): hidden.add((t['id'], rid))
        for cam, p in CAMERAS.items():
            if inside(p[0], p[1], rr): problems.append(f"road {rid} under camera {cam}")
    for l in LOTS:
        for t in TREES:
            if inside(t['x'], t['y'], l['rect'], 60) and not any(math.hypot(t['x'] - ix, t['y'] - iy) < 50 for ix, iy in l['islands']):
                hidden.add((t['id'], l['id']))
        for k, a in HOMECOMING_KEEP.items():
            if overlap(l['rect'], a): problems.append(f"lot {l['id']} hits homecoming {k}")
        for rid, rr in R:
            if rid != 'STADIUM DR' and overlap(l['rect'], rr, 0) and l['id'] == 'SOUTH LOT': problems.append(f"lot {l['id']} overlaps road {rid}")
        if overlap(l['rect'], FREE_ROAM_CORRIDOR): problems.append(f"lot {l['id']} on the free-roam corridor")
    for pts, w in WALKS:
        for p, q in zip(pts, pts[1:]):
            rr = seg_rect(p, q, w)
            for t in TREES:
                if inside(t['x'], t['y'], rr, 20): problems.append(f"walk {pts} runs over tree {t['id']}")
            if any(ellipse_r(x, y) < 1.0 for x, y in ((rr[0], rr[2]), (rr[1], rr[2]), (rr[0], rr[3]), (rr[1], rr[3]), ((rr[0]+rr[1])/2, (rr[2]+rr[3])/2))):
                if not (abs(p[0] - 2839) < 1 or abs(p[0] - 8000) < 1 or abs(q[0] - (-3300)) < 1):
                    problems.append(f"walk {pts} enters the stadium")
    return problems, sorted(hidden)

# ------------------------------------------------------------------ drawing
def draw(path, title):
    c = Canvas(-21000, 21000, -30500, -3000, px_per_m=3.0)
    GR, ASPH, CONC, LOTC = (200, 210, 172), (88, 88, 92), (206, 202, 192), (118, 118, 122)
    c.rect(c.xmin, c.xmax, c.ymin, c.ymax, GR)
    c.grid(5000)
    c.rect(*STREET, ASPH); c.rect(*FAR_SIDEWALK, CONC); c.rect(-21000, 8450, -4150, -3900, CONC)
    c.rect(8700, 10300, -5550, -3000, ASPH)
    for rid, rr in road_rects():
        if rid == 'BUS LOOP':
            c.circle(-1000, -19300, 1100, ASPH)
        else: c.rect(*rr, ASPH)
    m = ROADS[0]['median']; c.rect(m[0], m[1], m[2], m[3], (120, 150, 90))
    for l in LOTS:
        c.rect(*l['rect'], LOTC, outline=(60, 60, 60))
        x0, x1, y0, y1 = l['rect']
        for x in range(int(x0) + 250, int(x1), 260): c.line((x, y0 + 50), (x, y0 + 550), (230, 230, 230)); c.line((x, y1 - 550), (x, y1 - 50), (230, 230, 230))
        c.rect(*l['entry'], LOTC)
        for ix, iy in l['islands']: c.circle(ix, iy, 320, (120, 150, 90), outline=(220, 220, 210))
        c.text((x0 + x1) / 2, (y0 + y1) / 2, l['id'], (255, 255, 255), 2, center=True)
    for pts, w in WALKS + SIDEWALKS:
        for p, q in zip(pts, pts[1:]): c.line(p, q, CONC, world_width=w)
    for (cx, cy), r in PLAZAS: c.circle(cx, cy, r, CONC)
    cx, cy, rx, ry = STADIUM
    c.ellipse(cx, cy, rx, ry, (176, 116, 96), outline=(60, 30, 20)); c.ellipse(cx, cy, rx * .69, ry * .69, (86, 140, 76))
    c.text(cx, cy, 'CALDER STATE STADIUM', (255, 255, 255), 2, center=True)
    c.rect(100, 4700, -10000, -9250, (40, 40, 50))
    for b in BUILDINGS:
        col = {'A': (150, 60, 50), 'B': (165, 105, 80), 'C': (175, 140, 120)}[b['tier']]
        c.rect(*b['rect'], col, outline=(50, 25, 15))
        x0, x1, y0, y1 = b['rect']
        for f in b['faces']:
            ex, ey = {'N': ((x0 + x1) / 2, y1), 'S': ((x0 + x1) / 2, y0), 'E': (x1, (y0 + y1) / 2), 'W': (x0, (y0 + y1) / 2)}[f]
            c.circle(ex, ey, 220, (240, 200, 60), outline=(60, 40, 0))
        words = b['name'].split(' ')
        lines = [' '.join(words[:2]), ' '.join(words[2:])] if len(b['name']) > 14 else [b['name']]
        for k, ln in enumerate(lines):
            if ln: c.text((x0 + x1) / 2, (y0 + y1) / 2 + (k - (len(lines) - 1) / 2) * 520, ln, (255, 255, 255), 2, center=True)
        c.text(x0 + 300, y0 + 350, b['tier'], (255, 230, 120), 2)
    c.circle(-9000, -10800, 420, (230, 220, 190), outline=(60, 50, 40))
    c.text(-9000, -11650, 'CLOCK TOWER', (20, 20, 20), 2, center=True)
    for t in TREES:
        col = {'red': (200, 40, 30), 'orange': (230, 130, 30)}.get(t['tone'], (60, 110, 50))
        c.circle(t['x'], t['y'], 150 * t['s'], col)
    for x, y in GATEPOSTS: c.rect(x - 120, x + 120, y - 120, y + 120, (120, 60, 50))
    c.line((SOUTH_EDGE[0], SOUTH_EDGE[2]), (SOUTH_EDGE[1], SOUTH_EDGE[2]), (90, 70, 50), world_width=200)
    c.text(9500, -6900, 'CALDER DR', (255, 255, 255), 2, center=True)
    c.text(3300, -19000, 'STADIUM DR', (255, 255, 255), 2, center=True)
    c.text(-12700, -11900, 'MAIN QUAD', (40, 70, 30), 3, center=True)
    c.text(-6000, -4850, 'FRATERNITY PARK DR', (240, 240, 240), 2, center=True)
    c.text(-20700, -30200, title, (20, 20, 20), 2)
    c.save(path)

if __name__ == '__main__':
    problems, hidden = check()
    print('PROBLEMS:', len(problems)); [print('  -', p) for p in problems]
    print('TREES HIDDEN:', len(hidden)); [print('  -', h) for h in hidden]
    draw('plan_v1.png', 'CALDER STATE - PROPOSED LAYOUT V1 (ENGINE TOP VIEW, +Y DOWN). GOLD DOTS = ENTRANCES. TIER LETTER IN CORNER.')
