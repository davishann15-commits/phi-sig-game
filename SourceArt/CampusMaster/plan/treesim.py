#!/usr/bin/env python3
"""Offline replica of the FExBuilder::Rng(20261009) consumers in HouseExterior.cpp (FExSite::Vehicles + FExSite::Plants).

Purpose: check, WITHOUT running Unreal, whether a planned change to the NoGo list / vehicle rows / ExViews would change the
site-tree sequence (and therefore WOODS_PASS_ORIGINAL_TREES crc=0f3b2bb7).

Usage:
  python3 -I treesim.py                      # baseline: simulate, compare with the evidence CSV, print campus-side report
  python3 -I treesim.py --csv <original_trees.csv>
Edit EXTRA_NOGO / REMOVED_NOGO at the bottom (or import this module) to try a layout change: the script reports the first
grid cell whose Free() decision flips (which is where the random stream would diverge).

Mirrors (file:line in Source/SeniorSendoff/HouseExterior.cpp):
  Streets() NoGo 1197-1201, Courtyard() NoGo 1223/1235, Neighbours() 1238-1248 (+ HouseExteriorCampus.cpp:44,203 and
  HouseFraternityRow.cpp:162), Vehicles() 1323-1336 (Car() NoGo 1116), Plants() 1270-1312, ExViews 121-246,
  HouseFallJam::Inside HouseFallJam.cpp:14/1372, FRandomStream (UE_5.8 Core/Public/Math/RandomStream.h:115-121,362-364).
"""
import struct, sys, math

def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]

FMA = True  # clang arm64 contracts A+(B-A)*f into one fmadd; set False to test the unfused variant


class Stream:
    def __init__(self, seed):
        self.seed = seed & 0xffffffff
        self.draws = 0

    def frand(self):
        self.seed = (self.seed * 196314165 + 907633515) & 0xffffffff
        self.draws += 1
        bits = 0x3F800000 | (self.seed >> 9)
        return f32(struct.unpack('<f', struct.pack('<I', bits))[0] - 1.0)


def R(s, a, b):
    a = f32(a); b = f32(b)
    f = s.frand()
    d = f32(b - a)
    return f32(a + d * f) if FMA else f32(a + f32(d * f))


E = 32000.0
def StreetY(y): return y - 1200.0
FrontSidewalkInnerY = StreetY(-2700.0)

# ---------------------------------------------------------------- ExViews that take part (not nature_/detail_)
VIEWS = [
    ("great_entrance", 1184, 624), ("great_fireplace", -267, 614), ("gallery_hall", -1317, -752),
    ("rear_hall_upper", -2395, 693), ("deck", 1250, 1180), ("stair_window", -1789, 1109),
    ("balcony_out", -19, -1198), ("porch_facade", -10, 2297), ("porch_end", -1832, 2188),
    ("deck_edge", -500, 1500), ("patio_doors", 380, 1650), ("front_street", -300, StreetY(-4500)),
    ("front_walk", -20, -2600), ("front_lawn_overview", 4600, -6050), ("lawn_toward_road", -36.65, -2180),
    ("street_along", -7200, StreetY(-3650)), ("right_end", 6200, -2800), ("back_court", 800, 3400),
    ("back_stair", -2600, 3200), ("left_rear", -5500, 3500), ("left_front", -5600, -4300),
    ("high", 7000, -8000), ("yard_walk", -3900, -2400),
]

def near_view(x, y):
    for _, vx, vy in VIEWS:
        if abs(vx - x) < 900.0 and abs(vy - y) < 900.0:
            return True
    return False

def fall_jam_inside(x, y):
    return 4050.0 < x < 8400.0 and -3660.0 < y < -100.0

# ---------------------------------------------------------------- campus blocks (HouseExteriorCampus.cpp:121-130)
CAMPUS_BLOCKS = [
    (-14800, -8100, 5200, 3000), (-8700, -7900, 3800, 2600), (-4850, -7000, 2100, 1700), (9600, -7700, 3500, 2500),
    (14500, -8450, 4300, 3600), (-14500, -12500, 4800, 3000), (-8700, -13100, 3600, 3000), (-14800, -17300, 4400, 3500),
    (-9300, -18500, 3500, 4300), (-14300, -22700, 4800, 3700), (-7900, -24100, 4200, 3100), (10000, -12800, 3000, 3400),
    (15000, -14200, 4200, 3900), (11000, -19100, 3300, 3200), (15500, -22300, 4200, 3600), (2100, -23000, 7300, 2900),
]

def baseline_nogo_before_vehicles():
    """(tag, X0, X1, Y0, Y1) in the order they are added before Vehicles()."""
    n = []
    a = lambda tag, x0, x1, y0, y1: n.append((tag, float(x0), float(x1), float(y0), float(y1)))
    # Streets() 1197-1201
    a("street:FraternityParkDr+verges", -E, 8700, StreetY(-4700), StreetY(-2600))
    a("street:PatHeadSummitt", 8300, 10700, -E, E)
    a("street:eastDrive", 3300, 4200, StreetY(-3000), 2000)
    a("street:rearLot", 3300, 6700, 1800, 4600)
    a("street:rearLotSpur", 6500, 8800, 2400, 3200)
    a("street:westLot", -7300, -3600, StreetY(-3000), -200)
    a("street:healthCentreLot", -9100, 100, StreetY(-7500), StreetY(-4500))
    a("street:courtStrip", -1500, 1500, 1800, 3900)
    a("street:frontWalk", -500, 420, FrontSidewalkInnerY - 80, -1400)
    a("street:sideWalk", 3200, 3600, FrontSidewalkInnerY - 80, -1100)
    a("street:frontLawn", -3550, 3300, FrontSidewalkInnerY, -2550)
    a("street:houseBlock", -3800, 4500, -2550, 2200)
    # Courtyard() 1223, 1235
    a("court", -3500, 3500, 1750, 4320)
    a("court:a", 1700, 2600, 2000, 3200)
    a("court:dumpster", 5800, 6400, 4100, 4500)
    # Neighbours() 1241 campus footprints (+150 pad, HouseExteriorCampus.cpp:44) then stadium (203)
    for i, (x, y, w, d) in enumerate(CAMPUS_BLOCKS):
        a("campus:block%02d" % i, x - w / 2 - 150, x + w / 2 + 150, y - d / 2 - 150, y + d / 2 + 150)
    a("campus:stadium", 2400 - 5600 - 250, 2400 + 5600 + 250, -13300 - 3800 - 300, -13300 + 3800 + 800)
    # 1242 fraternity row (HouseFraternityRow.cpp:162)
    a("frat:0", -11600, -8600, -3720, 100); a("frat:1", -15550, -12250, -3700, 100); a("frat:2", -18850, -15950, -3750, 100)
    # 1246-1247 campus walk bands
    for x in (-11200.0, -6100.0, 7200.0, 12800.0):
        a("campusWalkNS:%d" % x, x - 350, x + 350, -27300, -5800)
    for y in (-6000.0, -10300.0, -16100.0, -21100.0):
        a("campusWalkEW:%d" % y, -18100, 18300, y - 350, y + 350)
    return n

VEHICLE_ROWS = [  # HouseExterior.cpp:1330-1335  (X0, X1, Step, Y, Skip, tag)
    (-9000, 6000, 1300, StreetY(-4150), .45, "kerbFar"),
    (-6750, -3950, 500, -2150, .3, "westLotLower"),
    (-6750, -3950, 500, -650, .3, "westLotUpper"),
    (3800, 6300, 500, 4100, .4, "rearLot"),
    (-8550, -250, 500, StreetY(-7050), .35, "healthLotSouth"),
    (-8550, -250, 500, StreetY(-4950), .35, "healthLotNorth"),
]


def run(extra_nogo=(), removed_tags=(), views=None, vehicle_rows=None, record=None):
    global VIEWS
    saved_views = VIEWS
    if views is not None:
        VIEWS = views
    s = Stream(20261009)
    nogo = [g for g in baseline_nogo_before_vehicles() if g[0] not in removed_tags]
    nogo += [(t, float(a), float(b), float(c), float(d)) for (t, a, b, c, d) in extra_nogo]
    cars = []
    for (x0, x1, step, y, skip, tag) in (vehicle_rows or VEHICLE_ROWS):
        x = float(x0)
        while x <= x1:
            if s.frand() < f32(skip):
                x += step; continue
            R(s, -2, 2)
            cars.append((tag, x, y))
            nogo.append(("car:%s@%d" % (tag, x), x - 500, x + 500, y - 300, y + 300))
            x += step
    for name, vx, vy in VIEWS:
        nogo.append(("view:" + name, f32(vx - 800), f32(vx + 800), f32(vy - 800), f32(vy + 800)))

    def free(x, y):
        for g in nogo:
            if g[1] < x < g[2] and g[3] < y < g[4]:
                return False, g[0]
        return True, None

    big, small = [], []
    def tree(x, y, is_big, src):
        if near_view(x, y):
            return
        sc = R(s, .85, 1.25)
        yaw = f32(s.frand() * f32(360.0))
        sz = f32(sc * R(s, .9, 1.1))
        (big if is_big else small).append((x, y, yaw, sc, sz, src))

    def shrubs(x0, x1, y, step):
        x = float(x0)
        while x <= x1:
            s.frand(); R(s, -15, 15); R(s, .8, 1.2); R(s, .8, 1.2); R(s, .8, 1.15)
            x += step
    shrubs(-1300, -570, -2390, 160); shrubs(500, 1240, -2390, 160)
    shrubs(-3300, -2650, -2550, 160); shrubs(-2150, -1550, -2550, 160)
    shrubs(1480, 3250, -2480, 160)
    y = -1900.0
    while y <= 1300: s.frand(); y += 180
    y = -1360.0
    while y <= -100: s.frand(); y += 180
    for (x, y, b) in [(-3000, -2600, True), (2200, -2650, True), (-4200, 1200, True), (4300, 800, True),
                      (-3170, 4450, False), (3200, 4450, True), (-4800, -2600, False)]:
        tree(float(x), float(y), b, "fixed")
    x = -18000.0
    while x < 8000:
        if abs(x + 5500) < 800 or abs(x - 3750) < 800 or abs(x) < 1500:
            x += 2400; continue
        tree(f32(x + R(s, -200, 200)), StreetY(-2600), False, "streetNear")
        tree(f32(f32(x + 1200) + R(s, -200, 200)), StreetY(-4700), False, "streetFar")
        x += 2400
    cells = []
    X = -27000.0
    while X < 27000:
        Y = -27000.0
        while Y < 27000:
            px = f32(X + R(s, -600, 600)); py = f32(Y + R(s, -600, 600))
            dist = max(abs(px), abs(py))
            chance = f32(.42) if dist > 19500.0 else f32(.16)
            cell = {"cell": (X, Y), "p": (px, py), "dist": dist}
            if s.frand() > chance:
                cell["state"] = "chance-miss"; cells.append(cell); Y += 1900; continue
            if dist <= 19500.0:
                ok, why = free(px, py)
                if not ok:
                    cell["state"] = "blocked"; cell["by"] = why; cells.append(cell); Y += 1900; continue
            is_big = s.frand() < f32(.5)
            if not near_view(px, py) and fall_jam_inside(px, py):
                s.frand(); s.frand(); s.frand()
                cell["state"] = "fallJamDummy"; cells.append(cell); Y += 1900; continue
            if near_view(px, py):
                cell["state"] = "nearView"; cells.append(cell)
                Y += 1900; continue
            tree(px, py, is_big, "grid")
            cell["state"] = "tree-" + ("big" if is_big else "small"); cells.append(cell)
            Y += 1900
        X += 1900
    VIEWS = saved_views
    return {"big": big, "small": small, "cells": cells, "cars": cars, "nogo": nogo, "draws": s.draws}


def load_csv(path):
    out = {"Big": [], "Small": [], "Distant": []}
    with open(path) as fh:
        next(fh)
        for line in fh:
            batch, idx, rest = line.rstrip("\n").split(",", 2)
            t, r, sc = rest.split("|")
            x, y, z = map(float, t.split(",")); _, yaw, _ = map(float, r.split(",")); sx, sy, sz = map(float, sc.split(","))
            out[batch].append((x, y, z, yaw, sx, sz))
    return out


def compare(sim, csv):
    """The CSV order is: Plants() trees, then HouseCampus::RearTrees, then HouseWoods::FitTrees removals/additions (Y>=4600 only)."""
    bad = 0
    for key, mine in (("Big", sim["big"]), ("Small", sim["small"])):
        ref = csv[key]; j = 0; matched = 0
        for (x, y, yaw, sc, sz, src) in mine:
            if j < len(ref) and abs(ref[j][0] - x) < .01 and abs(ref[j][1] - y) < .01:
                d = (ref[j][3] - yaw + 540) % 360 - 180
                # HouseWoods::FitTrees rewrites scale (and Z) for Y >= YMin=4600, so only compare scale south of that.
                if abs(d) > .01 or (y < 4600 and (abs(ref[j][4] - sc) > 1e-5 or abs(ref[j][5] - sz) > 1e-5)):
                    bad += 1
                j += 1; matched += 1
            elif y >= 4600:
                continue  # removed by FitTrees (TreeAllowed) - only possible in the woods
            else:
                bad += 1
        print("%s: simulated=%d matched-in-order=%d csvTotal=%d" % (key, len(mine), matched, len(ref)))
    return bad


def same(a, b):
    """True when two runs produce the identical Plants() tree sequence (=> identical crc, since nothing else changed)."""
    k = lambda r: [x[:5] for x in r["big"]] + [None] + [x[:5] for x in r["small"]]
    return k(a) == k(b)


def first_divergence(a, b):
    for ca, cb in zip(a["cells"], b["cells"]):
        if ca["state"] != cb["state"] or ca["p"] != cb["p"]:
            return ca["cell"], ca["p"], ca["state"], cb["state"]
    return None


def campus_report(sim, y_max=-5550.0):
    nogo = sim["nogo"]
    print("Blocked grid candidates with Y < %.0f (must STAY inside some NoGo):" % y_max)
    for c in sim["cells"]:
        if c["state"] == "blocked" and c["p"][1] < y_max:
            x, y = c["p"]
            print("  (%9.1f,%9.1f) by %s" % (x, y, " + ".join(g[0] for g in nogo if g[1] < x < g[2] and g[3] < y < g[4])))
    print("Free grid candidates with Y < %.0f inside Chebyshev 19500 (must STAY outside every NoGo):" % y_max)
    for c in sim["cells"]:
        if c["state"].startswith("tree") and c["p"][1] < y_max and c["dist"] <= 19500:
            print("  (%9.1f,%9.1f) %s" % (c["p"][0], c["p"][1], c["state"]))


if __name__ == "__main__":
    import json
    csv_path = "/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/WoodsPass/evidence/pass2_final/Saved/WoodsPassReview/original_trees.csv"
    if "--csv" in sys.argv:
        csv_path = sys.argv[sys.argv.index("--csv") + 1]
    base = run()
    print("draws on Rng(20261009):", base["draws"])
    print("mismatches vs evidence CSV (expect 0):", compare(base, load_csv(csv_path)))
    if "--report" in sys.argv:
        campus_report(base)
    if "--whatif" in sys.argv:
        # JSON: {"extra_nogo": [[tag,x0,x1,y0,y1],...], "removed_tags": [...], "add_views": [[name,x,y],...],
        #        "vehicle_rows": [[x0,x1,step,y,skip,tag],...]}   (all optional)
        spec = json.load(open(sys.argv[sys.argv.index("--whatif") + 1]))
        views = VIEWS + [tuple(v) for v in spec.get("add_views", [])]
        rows = [tuple(r) for r in spec["vehicle_rows"]] if "vehicle_rows" in spec else None
        trial = run(extra_nogo=[tuple(g) for g in spec.get("extra_nogo", [])], removed_tags=spec.get("removed_tags", []),
                    views=views, vehicle_rows=rows)
        ok = same(base, trial) and trial["draws"] == base["draws"]
        print("WHATIF crc-safe:", ok, "" if ok else "first divergence: %s" % (first_divergence(base, trial),))
