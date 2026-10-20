"""Schematic floor plans for the Tier A buildings (rooms as rectangles in building-local cm, entrance marked).

Each plan is drawn north-up in the building's own frame: local +X along the entrance facade's width, local +Y pointing
OUT of the entrance (so the door is at the top). Grid squares are 1 m. Rooms follow the facade's 360 cm window module
so every lit window belongs to a real room.
"""
import sys
sys.path.insert(0, '.')
from plandraw import Canvas

ROOM_COL = {'public': (236, 214, 150), 'circ': (220, 220, 214), 'service': (190, 190, 200), 'private': (200, 220, 240),
            'locked': (170, 160, 160), 'signature': (240, 190, 120)}

def plan(path, title, w, d, rooms, doors, stairs, notes):
    pad = 400
    c = Canvas(-pad, w + pad, -pad, d + pad, px_per_m=12.0, engine_view=False)
    c.rect(-pad, w + pad, -pad, d + pad, (245, 243, 236))
    for x in range(0, int(w) + 1, 100): c.line((x, 0), (x, d), (235, 232, 222))
    for y in range(0, int(d) + 1, 100): c.line((0, y), (w, y), (235, 232, 222))
    for (x0, x1, y0, y1, name, kind) in rooms:
        c.rect(x0, x1, y0, y1, ROOM_COL[kind], outline=(70, 60, 50))
        parts = name.split('/')
        for k, p in enumerate(parts):
            c.text((x0 + x1) / 2, (y0 + y1) / 2 - (k - (len(parts) - 1) / 2) * 90, p, (40, 30, 20), 1, center=True)
    c.poly([(0, 0), (w, 0), (w, d), (0, d)], None, outline=(30, 20, 10), width=3)
    for (x, y, label) in doors:
        c.rect(x - 60, x + 60, y - 25, y + 25, (60, 120, 60)); c.text(x, y + 70, label, (30, 90, 30), 1, center=True)
    for (x0, x1, y0, y1) in stairs:
        c.rect(x0, x1, y0, y1, (150, 150, 165), outline=(60, 60, 80))
        for k in range(int(y0) + 30, int(y1), 30): c.line((x0, k), (x1, k), (90, 90, 110))
    c.text(-pad + 40, d + pad - 60, title, (20, 20, 20), 2, world=True)
    for k, n in enumerate(notes): c.text(-pad + 40, -pad + 60 + 70 * k, n, (60, 50, 40), 1, world=True)
    return c.save(path)

# ---------------------------------------------------------------- Mercer Library (3400 x 5000 footprint, entrance on its E face)
# local frame: width along the E facade = 5000 (the building's Y extent), depth 3400.
W, D = 5000, 3400
plan('fp_library_g.png', 'MERCER LIBRARY - GROUND FLOOR (TIER A)', W, D, [
    (1900, 3100, 2300, 3400, 'LOBBY', 'circ'),
    (1900, 3100, 1500, 2300, 'CIRCULATION/DESK', 'signature'),
    (3100, 5000, 1900, 3400, 'CARD CATALOG/HALL', 'signature'),
    (0, 1900, 1900, 3400, 'PERIODICALS/MICROFICHE', 'public'),
    (0, 1900, 0, 1900, 'REFERENCE/READING ROOM', 'signature'),
    (3100, 5000, 0, 1900, 'CARRELS/STUDY', 'public'),
    (1900, 3100, 0, 700, 'STAFF/WORKROOM', 'locked'),
], [(2500, 3400, 'MAIN DOORS (E)')], [(1950, 2350, 700, 1400), (2650, 3050, 700, 1400)],
   ['GREEN-SHADED CARREL LAMPS, OAK TABLES, DATE STAMP AT THE DESK', 'STAIR UP TO THE STACKS; UPPER FLOORS: 2 OPEN (STACKS), 3-5 LOCKED (SPECIAL COLL.)'])
plan('fp_library_2.png', 'MERCER LIBRARY - FLOOR 2: STACKS (TIER A)', W, D, [
    (0, 5000, 2200, 3400, 'STACKS ROWS (WALKABLE)', 'public'),
    (0, 5000, 1200, 2200, 'STACKS ROWS', 'public'),
    (0, 1900, 0, 1200, 'GROUP STUDY', 'public'),
    (3100, 5000, 0, 1200, 'CARRELS ALONG WINDOWS', 'public'),
    (1900, 3100, 0, 700, 'COPY ROOM', 'service'),
], [], [(1950, 2350, 700, 1400), (2650, 3050, 700, 1400)], ['STEEL SHELVING, KICK STOOLS, BOOK CARTS, A MIMEO FLYER ON THE STAIR DOOR'])

# ---------------------------------------------------------------- Harlow Hall dorm (3800 x 2600, entrance N)
W, D = 3800, 2600
rooms = []
x = 160
k = 0
while x + 360 <= W - 160:
    rooms.append((x, x + 360, 1600, 2600, f'1{k:02d}', 'private')); rooms.append((x, x + 360, 0, 1000, f'1{k + 50:02d}', 'private')); x += 360; k += 1
rooms = [r for r in rooms if not (1600 <= r[0] <= 2200)]
plan('fp_harlow_g.png', 'HARLOW HALL (DORM) - GROUND FLOOR (TIER A)', W, D, rooms + [
    (0, 3800, 1000, 1600, 'DOUBLE-LOADED CORRIDOR (CINDERBLOCK, LINOLEUM)', 'circ'),
    (1600, 2240, 1600, 2600, 'LOBBY/RA DESK', 'signature'),
    (1600, 2240, 0, 1000, 'TV LOUNGE/VENDING', 'signature'),
], [(1920, 2600, 'FRONT DOORS (N)')], [(160, 520, 1050, 1550), (3280, 3640, 1050, 1550)],
   ['STUDENT ROOMS ALIGN WITH THE 360 CM WINDOW MODULE', 'FLOORS 1-2 OPEN; 3-6 LOCKED STAIR DOORS (QUIET HOURS SIGN)', 'FLOOR 2: COMMUNAL BATHROOM + LAUNDRY AT THE EAST END'])

# ---------------------------------------------------------------- Hollis Science Hall (4600 x 3200, entrance N)
W, D = 4600, 3200
plan('fp_science_g.png', 'HOLLIS SCIENCE HALL - GROUND FLOOR (TIER A)', W, D, [
    (1700, 2900, 2200, 3200, 'LOBBY/TROPHY+SPECIMEN CASE', 'signature'),
    (0, 1700, 2000, 3200, 'TEACHING LAB 101/FUME HOODS', 'signature'),
    (2900, 4600, 2000, 3200, 'TEACHING LAB 102', 'public'),
    (0, 4600, 1500, 2000, 'CORRIDOR', 'circ'),
    (800, 3800, 0, 1500, 'TIERED LECTURE HALL 110/CHALKBOARD EQUATIONS', 'signature'),
    (0, 800, 0, 1500, 'PREP ROOM', 'locked'), (3800, 4600, 0, 1500, 'OFFICES', 'locked'),
], [(2300, 3200, 'FRONT DOORS (N, ONTO THE QUAD)')], [(1750, 2100, 1550, 1950), (2500, 2850, 1550, 1950)],
   ['FLOOR 2 OPEN: FACULTY CORRIDOR, A DARKROOM, THE PHYSICS LAB'])

# ---------------------------------------------------------------- Goose Union (3900 x 3900, entrances W and N)
W, D = 3900, 3900
plan('fp_union_g.png', 'GOOSE UNION - GROUND FLOOR (TIER A)', W, D, [
    (1300, 2600, 2600, 3900, 'LOBBY/INFO DESK/RIDE BOARD', 'signature'),
    (0, 1300, 2000, 3900, 'GOOSE PIT/SNACK BAR', 'signature'),
    (2600, 3900, 2600, 3900, 'MAILROOM/PO BOXES', 'public'),
    (2600, 3900, 1300, 2600, 'TV LOUNGE/CRTS REPLAY', 'signature'),
    (0, 2600, 0, 2000, 'GAME ROOM/POOL+PINBALL', 'signature'),
    (2600, 3900, 0, 1300, 'HOMECOMING COMMITTEE', 'public'),
    (1300, 2600, 2000, 2600, 'HALL', 'circ'),
], [(1950, 3900, 'WEST DOORS (TOWER PLAZA)'), (3900, 1950, 'NORTH DOORS')], [(1350, 1750, 2050, 2550)],
   ['FLOOR 2 OPEN: BALLROOM SET FOR THE HOMECOMING DANCE, STUDENT GOV OFFICES'])
print('ok')
