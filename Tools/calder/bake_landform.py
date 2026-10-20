"""Canonical baker with continuous grade-break support.

Bake the Calder landform (Saved/CalderWork/plan/landform.py, the authority) into generated C++ data the game reads at
load (Source/SeniorSendoff/HouseCalderLandformData.inl). Deterministic: the same file on every machine.

What it writes:
  - the 2 m relief lattice (321 x 321 corners over +-32000, the C++ lattice exactly), in tenths of a millimetre;
  - every lattice cell a declared wall crosses, split along those walls into convex pieces, each vertex's relief taken on
    its own side of the wall (lattice corners exactly; points on a wall line a hair off it toward the piece's side), so
    retaining walls are true steps and the ground, its collision and every placer agree;
  - the building pads (block Id -> DZ), the tower lift;
  - the walls (name, kind, ends), the stairs and ramps, the reserved mystery spots.
Usage: python3 -I Tools/calder/bake_landform.py   (prints a summary; writes the .inl, deleting it first because of iCloud)
"""
import hashlib, importlib.util, math, os, sys, time

PLAN = '/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/CalderWork/plan'
PLAN_INPUT = os.environ.get('CALDER_PLAN_FILE', os.path.join(PLAN, 'landform.py'))
if 'CALDER_PLAN_FILE' in os.environ and 'BAKE_OUT' not in os.environ:
    raise SystemExit('An isolated CALDER_PLAN_FILE requires an explicit BAKE_OUT; active Source will not be overwritten.')
OUT = os.environ.get('BAKE_OUT', '/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Source/SeniorSendoff/HouseCalderLandformData.inl')
HALF, CELL = 32000.0, 200.0
LINES = int(2 * HALF / CELL) + 1
EPS = 2.0   # cm off a wall line, toward a piece's side

t0 = time.time()
sys.path.insert(0, PLAN)
spec = importlib.util.spec_from_file_location('landform', PLAN_INPUT)
L = importlib.util.module_from_spec(spec)
spec.loader.exec_module(L)
src_hash = hashlib.sha256(open(PLAN_INPUT, 'rb').read()).hexdigest()
print(f'landform.py loaded in {time.time() - t0:.1f} s (sha256 {src_hash[:16]})', flush=True)


def corner(i):
    return -HALF + i * CELL


# ------------------------------------------------------------------ the lattice
t1 = time.time()
lattice = [0] * (LINES * LINES)
for j in range(LINES):
    y = corner(j)
    for i in range(LINES):
        lattice[j * LINES + i] = int(round(L.relief(corner(i), y) * 100.0))   # 0.1 mm
print(f'lattice {LINES}x{LINES} in {time.time() - t1:.1f} s; relief {min(lattice) / 100:.1f}..{max(lattice) / 100:.1f} cm', flush=True)

# ------------------------------------------------------------------ walls and the cells they cross
W = L.walls()
segs = []
for name, kind, a, b in list(W) + list(getattr(L, 'grade_breaks', lambda: [])()):
    if math.hypot(b[0] - a[0], b[1] - a[1]) < 1.0:
        continue
    segs.append((name, kind, (float(a[0]), float(a[1])), (float(b[0]), float(b[1]))))
physical_segs = [segment for segment in segs if segment[1] != 'grade break']


def clip(poly, A, B, C):
    out = []
    n = len(poly)
    for k in range(n):
        p, q = poly[k], poly[(k + 1) % n]
        fp, fq = A * p[0] + B * p[1] + C, A * q[0] + B * q[1] + C
        if fp >= 0.0:
            out.append(p)
        if (fp >= 0.0) != (fq >= 0.0):
            t = fp / (fp - fq)
            out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
    return out


def area(poly):
    return 0.5 * sum(poly[k][0] * poly[(k + 1) % len(poly)][1] - poly[(k + 1) % len(poly)][0] * poly[k][1] for k in range(len(poly)))


def seg_hits_box(a, b, x0, y0, x1, y1):
    """The segment a-b meets the open box (Liang-Barsky)."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    t0_, t1_ = 0.0, 1.0
    for p, q in ((-dx, a[0] - x0), (dx, x1 - a[0]), (-dy, a[1] - y0), (dy, y1 - a[1])):
        if abs(p) < 1e-12:
            if q < 0:
                return False
            continue
        t = q / p
        if p < 0:
            t0_ = max(t0_, t)
        else:
            t1_ = min(t1_, t)
        if t0_ > t1_:
            return False
    return t1_ - t0_ > 1e-9


def seg_hits_poly(a, b, poly):
    """The segment crosses the convex polygon's interior (both sides of its line have polygon area, and it enters it)."""
    nx, ny = -(b[1] - a[1]), b[0] - a[0]
    c = -(nx * a[0] + ny * a[1])
    side = [nx * p[0] + ny * p[1] + c for p in poly]
    if max(side) <= 1e-6 or min(side) >= -1e-6:
        return False
    # the segment's extent along its own line must overlap the polygon's
    L2 = (b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2
    ts = [((p[0] - a[0]) * (b[0] - a[0]) + (p[1] - a[1]) * (b[1] - a[1])) / L2 for p in poly]
    return max(ts) > 0.0 and min(ts) < 1.0


def seg_dist(px, py, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    L2 = dx * dx + dy * dy
    t = max(0.0, min(1.0, ((px - a[0]) * dx + (py - a[1]) * dy) / L2)) if L2 > 0 else 0.0
    return math.hypot(px - a[0] - dx * t, py - a[1] - dy * t)


cells = {}
for k, (_, _, a, b) in enumerate(segs):
    i0 = max(0, int(math.floor((min(a[0], b[0]) + HALF) / CELL)) - 1)
    i1 = min(LINES - 2, int(math.floor((max(a[0], b[0]) + HALF) / CELL)) + 1)
    j0 = max(0, int(math.floor((min(a[1], b[1]) + HALF) / CELL)) - 1)
    j1 = min(LINES - 2, int(math.floor((max(a[1], b[1]) + HALF) / CELL)) + 1)
    for j in range(j0, j1 + 1):
        for i in range(i0, i1 + 1):
            # closed box (1 cm slack): a wall lying ON a lattice line marks the cells on both sides of it
            if seg_hits_box(a, b, corner(i) - 1.0, corner(j) - 1.0, corner(i + 1) + 1.0, corner(j + 1) + 1.0):
                cells.setdefault((i, j), []).append(k)

corner_z = lambda i, j: lattice[j * LINES + i] / 100.0
t2 = time.time()
pieces = {}   # cell index -> [ [(x, y, z), ...], ... ]


def split_line(polys_, A, B, C):
    """Every polygon the line A*x + B*y + C = 0 runs through, cut in two along it."""
    out_ = []
    for p_ in polys_:
        side = [A * v[0] + B * v[1] + C for v in p_]
        if max(side) > 1e-6 and min(side) < -1e-6:
            for s_ in (1.0, -1.0):
                q = clip(p_, s_ * A, s_ * B, s_ * C)
                if len(q) >= 3 and abs(area(q)) > 0.5:
                    out_.append(q)
        else:
            out_.append(p_)
    return out_


def stepped(px_, py_):
    """The relief steps at this point (it differs by more than 5 cm round a 2 cm ring): a wall's line runs on here."""
    zs = [L.relief(px_ + EPS * math.cos(t_ * math.pi / 4.0), py_ + EPS * math.sin(t_ * math.pi / 4.0)) for t_ in range(8)]
    return max(zs) - min(zs) > 5.0


# Pass 1: each wall cell's convex pieces (geometry). Free wall ends inside the cell (an end that meets no other wall here,
# where the step really closes past the end): the cell is cut along every wall's line right across it, and square to the
# wall through each free end; the end takes the low side's height in every piece round it (pass 3), so the step tapers to
# nothing at the end and no crack opens.
geom, end_zs = {}, {}
FP_RECTS = [tuple(b_['rect']) for b_ in L.BLOCKS.values()]
for (i, j), ks in sorted(cells.items()):
    x0, y0 = corner(i), corner(j)
    # the cell starts as the lattice's own two triangles (split on its (I,J)-(I+1,J+1) diagonal, as a plain cell is), so a
    # piece away from the walls is exactly the plain lattice surface and the cell's corners interpolate as they would
    polys = [[(x0, y0), (x0 + CELL, y0), (x0 + CELL, y0 + CELL)], [(x0, y0), (x0 + CELL, y0 + CELL), (x0, y0 + CELL)]]
    free_ends = []
    for k in ks:
        if segs[k][1] == 'grade break':
            continue   # a continuous slope change has no free retaining-wall end
        _, _, a, b = segs[k]
        for e in (a, b):
            if not (x0 + 0.5 < e[0] < x0 + CELL - 0.5 and y0 + 0.5 < e[1] < y0 + CELL - 0.5):
                continue
            if any(k2 != k and seg_dist(e[0], e[1], segs[k2][2], segs[k2][3]) < 1.0 for k2 in ks):
                continue   # a corner: the step turns along the other wall
            L_ = math.hypot(b[0] - a[0], b[1] - a[1])
            ux, uy = (b[0] - a[0]) / L_, (b[1] - a[1]) / L_
            if e == a:
                ux, uy = -ux, -uy
            bx_, by_ = e[0] + ux * EPS, e[1] + uy * EPS
            if abs(L.relief(bx_ - uy * EPS, by_ + ux * EPS) - L.relief(bx_ + uy * EPS, by_ - ux * EPS)) > 3.0:
                continue   # the step runs on past the end (the wall was declared short): its line carries it across
            free_ends.append((k, e))
    free_walls = {k for k, _ in free_ends}
    for k, e in free_ends:
        _, _, a, b = segs[k]
        L_ = math.hypot(b[0] - a[0], b[1] - a[1])
        ux, uy = (b[0] - a[0]) / L_, (b[1] - a[1]) / L_
        polys = split_line(polys, ux, uy, -(ux * e[0] + uy * e[1]))
    for k in ks:
        _, _, a, b = segs[k]
        nx, ny = -(b[1] - a[1]), b[0] - a[0]
        c = -(nx * a[0] + ny * a[1])
        if k in free_walls:
            polys = split_line(polys, nx, ny, c)
            continue
        nxt = []
        for p in polys:
            if seg_hits_poly(a, b, p):
                for s in (1.0, -1.0):
                    q = clip(p, s * nx, s * ny, s * c)
                    if len(q) >= 3 and abs(area(q)) > 0.5:
                        nxt.append(q)
            else:
                nxt.append(p)
        polys = nxt
    # a building's faces through the cell split it too: the ground outside a face is then drawn from its own side (the
    # lattice corners inside a footprint carry one face's outside ground, which at a corner can be the other face's:
    # beside the library's north-west corner the court's ground dipped 1.9 m toward the west face's, 2026-10-15)
    for fr in FP_RECTS:
        if fr[0] > x0 + CELL + 1.0 or fr[1] < x0 - 1.0 or fr[2] > y0 + CELL + 1.0 or fr[3] < y0 - 1.0:
            continue
        for a, b in (((fr[0], fr[2]), (fr[1], fr[2])), ((fr[1], fr[2]), (fr[1], fr[3])), ((fr[1], fr[3]), (fr[0], fr[3])),
                     ((fr[0], fr[3]), (fr[0], fr[2]))):
            if not seg_hits_box(a, b, x0 - 1.0, y0 - 1.0, x0 + CELL + 1.0, y0 + CELL + 1.0):
                continue
            nx, ny = -(b[1] - a[1]), b[0] - a[0]
            c = -(nx * a[0] + ny * a[1])
            nxt = []
            for p in polys:
                if seg_hits_poly(a, b, p):
                    for s_ in (1.0, -1.0):
                        q = clip(p, s_ * nx, s_ * ny, s_ * c)
                        if len(q) >= 3 and abs(area(q)) > 0.5:
                            nxt.append(q)
                else:
                    nxt.append(p)
            polys = nxt
    for k, e in free_ends:
        _, _, a, b = segs[k]
        L_ = math.hypot(b[0] - a[0], b[1] - a[1])
        nx_, ny_ = -(b[1] - a[1]) / L_, (b[0] - a[0]) / L_
        end_zs.setdefault((i, j), {})[e] = round(min(L.relief(e[0] + nx_ * EPS, e[1] + ny_ * EPS), L.relief(e[0] - nx_ * EPS, e[1] - ny_ * EPS)) * 100.0) / 100.0
    geom[(i, j)] = polys

# Pass 2: one conforming mesh. Every vertex snaps to 0.01 cm (the .inl's precision), so a point that two cells (or two
# pieces) compute is one point; then every vertex of every piece in a cell and its eight neighbours, and every wall end in
# the cell, is put into each piece edge it lies on (no T-junction anywhere: a piece's edge has a vertex wherever any other
# piece's corner touches it).
SNAP = 100.0


def pkey(x, y):
    return (int(round(x * SNAP)), int(round(y * SNAP)))


def kxy(k):
    return (k[0] / SNAP, k[1] / SNAP)


def karea(p):
    return 0.5 * sum(p[k][0] * p[(k + 1) % len(p)][1] - p[(k + 1) % len(p)][0] * p[k][1] for k in range(len(p))) / (SNAP * SNAP)


for c_ in list(geom):
    out_ = []
    for p in geom[c_]:
        q = []
        for (vx, vy) in p:
            k_ = pkey(vx, vy)
            if not q or q[-1] != k_:
                q.append(k_)
        while len(q) > 1 and q[0] == q[-1]:
            q.pop()
        if len(q) >= 3 and abs(karea(q)) > 0.25:
            if karea(q) < 0:
                q.reverse()        # anticlockwise
            out_.append(q)
    geom[c_] = out_

wall_end_keys = {}
for k, (_, _, a, b) in enumerate(segs):
    for e in (a, b):
        wall_end_keys[pkey(e[0], e[1])] = k


def on_edge(pk, ak, bk, tol=1.5):
    """pk lies on the open edge ak-bk (in 0.01 cm units: within tol of the line, more than tol from either end)."""
    ex, ey = bk[0] - ak[0], bk[1] - ak[1]
    L2 = ex * ex + ey * ey
    if L2 <= 0:
        return None
    t = ((pk[0] - ak[0]) * ex + (pk[1] - ak[1]) * ey) / L2
    Ln = math.sqrt(L2)
    if t * Ln <= tol or (1.0 - t) * Ln <= tol:
        return None
    if abs((pk[0] - ak[0]) * ey - (pk[1] - ak[1]) * ex) / Ln > tol:
        return None
    return t


inserted = 0
for (i, j) in sorted(geom):
    x0k, y0k = int(round(corner(i) * SNAP)), int(round(corner(j) * SNAP))
    x1k, y1k = x0k + int(CELL * SNAP), y0k + int(CELL * SNAP)
    cand = set()
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            for p in geom.get((i + di, j + dj), ()):
                cand.update(p)
    cand.update(wall_end_keys)
    cand = [k_ for k_ in cand if x0k - 2 <= k_[0] <= x1k + 2 and y0k - 2 <= k_[1] <= y1k + 2]
    polys = geom[(i, j)]
    for n_, p in enumerate(polys):
        q = []
        for m_ in range(len(p)):
            a_, b_ = p[m_], p[(m_ + 1) % len(p)]
            q.append(a_)
            on = []
            for k_ in cand:
                if k_ == a_ or k_ == b_:
                    continue
                t = on_edge(k_, a_, b_)
                if t is not None:
                    on.append((t, k_))
            for _t, k_ in sorted(on):
                if k_ not in q:
                    q.append(k_)
                    inserted += 1
        polys[n_] = q

# Pass 3: heights. The ground is ONE surface except across a declared wall (and round a building's footprint, a hole the
# shell closes). So the heights at a vertex are found per sector: the pieces round a vertex that meet along an edge which
# is not on a wall (nor in a footprint) are joined, and each joined group gets one height:
#   - a group that meets a plain (unsplit) cell: that cell's own edge (linear between its corners; a lattice corner exactly);
#   - no wall round the vertex (fewer than two separating edges): one height for every piece there: a lattice corner
#     exactly, a wall's end (one wall edge ends here) the low side's height (the step tapers to nothing at the end),
#     anywhere else the relief at the point;
#   - between walls: the relief a little way into the group's own sector (along its bisector, clear of the walls).
# Pieces that meet off a wall therefore always agree at their shared vertices, and their shared edge is one line: no crack.
t3 = time.time()
cell_of = {}
inc_list = []                     # (cell, piece, vertex index)
pt_inc = {}                       # vertex key -> [incidence ids]
edge_inc = {}                     # (key a, key b) sorted -> [(incidence at a, incidence at b, cell)]
for (i, j), polys in sorted(geom.items()):
    for n_, p in enumerate(polys):
        base = len(inc_list)
        for m_ in range(len(p)):
            inc_list.append(((i, j), n_, m_))
            pt_inc.setdefault(p[m_], []).append(base + m_)
        for m_ in range(len(p)):
            a_, b_ = p[m_], p[(m_ + 1) % len(p)]
            ia, ib = base + m_, base + (m_ + 1) % len(p)
            if a_ <= b_:
                edge_inc.setdefault((a_, b_), []).append((ia, ib, (i, j)))
            else:
                edge_inc.setdefault((b_, a_), []).append((ib, ia, (i, j)))

# walls by 16 m bucket, for the separator test
WB = {}
for k, (_, _, a, b) in enumerate(segs):
    for bi in range(int(math.floor(min(a[0], b[0]) / 1600.0)) - 1, int(math.floor(max(a[0], b[0]) / 1600.0)) + 2):
        for bj in range(int(math.floor(min(a[1], b[1]) / 1600.0)) - 1, int(math.floor(max(a[1], b[1]) / 1600.0)) + 2):
            WB.setdefault((bi, bj), []).append(k)
RECTS = [b_['rect'] for b_ in L.BLOCKS.values()]


def line_dist(px, py, a, b):
    L_ = math.hypot(b[0] - a[0], b[1] - a[1])
    return abs((px - a[0]) * (b[1] - a[1]) - (py - a[1]) * (b[0] - a[0])) / L_ if L_ > 0 else math.hypot(px - a[0], py - a[1])


def edge_kind(ak, bk):
    """'wall' when the edge lies on a declared wall, 'fp' when it lies in (or on) a building footprint, 'edge' on the
    walkable square's own edge (the outer land meets it there: the sides of a wall that runs on out of the square stay
    apart), else ''."""
    (ax_, ay_), (bx_, by_) = kxy(ak), kxy(bk)
    mx, my = (ax_ + bx_) * 0.5, (ay_ + by_) * 0.5
    if (abs(ax_) >= HALF - 0.01 and abs(bx_) >= HALF - 0.01) or (abs(ay_) >= HALF - 0.01 and abs(by_) >= HALF - 0.01):
        return 'edge'
    for k in WB.get((int(math.floor(mx / 1600.0)), int(math.floor(my / 1600.0))), ()):
        if segs[k][1] == 'grade break':
            continue   # join the sectors across this edge at one exact height
        _, _, a, b = segs[k]
        if seg_dist(mx, my, a, b) < 0.5 and line_dist(ax_, ay_, a, b) < 1.0 and line_dist(bx_, by_, a, b) < 1.0:
            return 'wall'
    for r in RECTS:
        if r[0] - 0.5 <= mx <= r[1] + 0.5 and r[2] - 0.5 <= my <= r[3] + 0.5:
            return 'fp'
    return ''


EK = {e: edge_kind(*e) for e in edge_inc}
parent = list(range(len(inc_list)))


def find(a):
    while parent[a] != a:
        parent[a] = parent[parent[a]]
        a = parent[a]
    return a


def union(a, b):
    ra, rb = find(a), find(b)
    if ra != rb:
        parent[max(ra, rb)] = min(ra, rb)


def plain_edge_z(ci, cj, px, py):
    """A plain cell's own surface on its boundary (linear between its corners along the edge; its corners exactly)."""
    x0_, y0_ = corner(ci), corner(cj)
    fx, fy = (px - x0_) / CELL, (py - y0_) / CELL
    fx, fy = min(max(fx, 0.0), 1.0), min(max(fy, 0.0), 1.0)
    A_, B_, C_, D_ = corner_z(ci, cj), corner_z(ci + 1, cj), corner_z(ci + 1, cj + 1), corner_z(ci, cj + 1)
    if abs(fx) < 1e-9 and abs(fy) < 1e-9:
        return A_
    z = A_ + (B_ - A_) * fx + (C_ - B_) * fy if fx >= fy else A_ + (C_ - D_) * fx + (D_ - A_) * fy
    return round(z * 100.0) / 100.0


plain_z = {}                      # incidence root -> plain height (filled after the unions)
plain_links = []                  # (incidence, z)
unmatched = 0
for e, lst in edge_inc.items():
    if EK[e]:
        continue
    if len(lst) >= 2:
        for (ia, ib, _c) in lst[1:]:
            union(lst[0][0], ia)
            union(lst[0][1], ib)
        continue
    (ia, ib, (ci, cj)) = lst[0]
    (ax_, ay_), (bx_, by_) = kxy(e[0]), kxy(e[1])
    x0_, y0_ = corner(ci), corner(cj)
    nb = None
    if abs(ax_ - x0_) < 0.02 and abs(bx_ - x0_) < 0.02:
        nb = (ci - 1, cj)
    elif abs(ax_ - x0_ - CELL) < 0.02 and abs(bx_ - x0_ - CELL) < 0.02:
        nb = (ci + 1, cj)
    elif abs(ay_ - y0_) < 0.02 and abs(by_ - y0_) < 0.02:
        nb = (ci, cj - 1)
    elif abs(ay_ - y0_ - CELL) < 0.02 and abs(by_ - y0_ - CELL) < 0.02:
        nb = (ci, cj + 1)
    if nb is None:
        unmatched += 1            # an inner edge with no partner (a dropped sliver): left to the per-point rules
        continue
    if nb in geom or not (0 <= nb[0] < LINES - 1 and 0 <= nb[1] < LINES - 1):
        if nb in geom:
            unmatched += 1
        continue
    plain_links.append((ia, plain_edge_z(nb[0], nb[1], ax_, ay_)))
    plain_links.append((ib, plain_edge_z(nb[0], nb[1], bx_, by_)))
for ia, z in plain_links:
    plain_z[find(ia)] = z


def sample_relief(px, py):
    return round(L.relief(px, py) * 100.0) / 100.0


def wedge(ci_, n_, m_):
    p = geom[ci_][n_]
    P_, Nx, Pv = kxy(p[m_]), kxy(p[(m_ + 1) % len(p)]), kxy(p[(m_ - 1) % len(p)])
    d1 = (Nx[0] - P_[0], Nx[1] - P_[1])
    d2 = (Pv[0] - P_[0], Pv[1] - P_[1])
    l1, l2 = math.hypot(*d1) or 1.0, math.hypot(*d2) or 1.0
    d1, d2 = (d1[0] / l1, d1[1] / l1), (d2[0] / l2, d2[1] / l2)
    ang = math.atan2(d1[0] * d2[1] - d1[1] * d2[0], d1[0] * d2[0] + d1[1] * d2[1])
    if ang < 0:
        ang += 2 * math.pi
    bx_, by_ = d1[0] + d2[0], d1[1] + d2[1]
    lb = math.hypot(bx_, by_)
    if lb < 1e-6:
        bx_, by_ = -d1[1], d1[0]
    else:
        bx_, by_ = bx_ / lb, by_ / lb
    return ang, (bx_, by_)


zinc = [0.0] * len(inc_list)
n_end = n_sector = 0
for P, ids in pt_inc.items():
    px, py = kxy(P)
    seps, wall_rays = set(), []
    for iid in ids:
        ci_, n_, m_ = inc_list[iid]
        p = geom[ci_][n_]
        for Q in (p[(m_ + 1) % len(p)], p[(m_ - 1) % len(p)]):
            ek = EK.get((P, Q) if P <= Q else (Q, P), '')
            if ek and Q not in seps:
                seps.add(Q)
                if ek == 'wall':
                    wall_rays.append(Q)
    roots = {}
    for iid in ids:
        roots.setdefault(find(iid), []).append(iid)
    lattice_pt = (P[0] % int(CELL * SNAP) == int(HALF * SNAP) % int(CELL * SNAP) and
                  P[1] % int(CELL * SNAP) == int(HALF * SNAP) % int(CELL * SNAP))
    if len(seps) < 2:
        # one height for every piece at this point
        z = None
        for r_ in roots:
            if r_ in plain_z:
                z = plain_z[r_]
                break
        if z is None:
            if lattice_pt:
                z = corner_z(int(round((px + HALF) / CELL)), int(round((py + HALF) / CELL)))
            elif len(wall_rays) == 1:
                ex_, ey_ = kxy(wall_rays[0])
                L_ = math.hypot(ex_ - px, ey_ - py) or 1.0
                nx_, ny_ = -(ey_ - py) / L_, (ex_ - px) / L_
                z = round(min(L.relief(px + nx_ * EPS, py + ny_ * EPS), L.relief(px - nx_ * EPS, py - ny_ * EPS)) * 100.0) / 100.0
                n_end += 1
            else:
                z = sample_relief(px, py)
        for iid in ids:
            zinc[iid] = z
        continue
    for r_, group in roots.items():
        if r_ in plain_z:
            z = plain_z[r_]
        else:
            sx = sy = tot = 0.0
            for iid in group:
                ang, (bx_, by_) = wedge(*inc_list[iid])
                sx += bx_ * ang
                sy += by_ * ang
                tot += ang
            ls = math.hypot(sx, sy)
            if ls < 1e-9:
                ang, (sx, sy) = wedge(*inc_list[group[0]])
                ls = 1.0
            half = min(tot, 2 * math.pi - 1e-6) * 0.5
            d = min(50.0, max(EPS, 0.5 / max(math.sin(min(half, math.pi / 2)), 1e-3)))
            z = sample_relief(px + sx / ls * d, py + sy / ls * d)
            n_sector += 1
        for iid in group:
            zinc[iid] = z

# Pass 4: every piece must fan from its vertex 0 (the game draws and samples a piece as the fan from vertex 0): a vertex 0
# in line with a neighbouring edge (a point pass 2 put on an edge) would give a flat, missing triangle and lose that
# point's height. Such a piece starts at a vertex that fans cleanly, or (none does) is cut into triangles, each a piece.


def fan_ok(v):
    x0_, y0_ = v[0][0], v[0][1]
    for k in range(1, len(v) - 1):
        ax_, ay_ = v[k][0] - x0_, v[k][1] - y0_
        bx_, by_ = v[k + 1][0] - x0_, v[k + 1][1] - y0_
        cr = ax_ * by_ - ay_ * bx_
        if cr <= 1e-3 * math.hypot(ax_, ay_) * math.hypot(bx_, by_):
            return False
    return True


def ear_clip(v):
    v = list(v)
    out_ = []
    guard = 0
    while len(v) > 3 and guard < 200:
        guard += 1
        n = len(v)
        done = False
        for k in range(n):
            a_, b_, c_ = v[k - 1], v[k], v[(k + 1) % n]
            cr = (b_[0] - a_[0]) * (c_[1] - a_[1]) - (b_[1] - a_[1]) * (c_[0] - a_[0])
            if cr <= 1e-3 * math.hypot(b_[0] - a_[0], b_[1] - a_[1]) * math.hypot(c_[0] - a_[0], c_[1] - a_[1]):
                continue
            rest = v[:k] + v[k + 1:]
            if abs(area([(q[0], q[1]) for q in rest])) < 1.0:
                continue
            out_.append([a_, b_, c_])
            v = rest
            done = True
            break
        if not done:
            break
    out_.append(v)
    return out_


pieces = {}   # cell index -> [ [(x, y, z), ...], ... ]
n_rot = n_split = 0
for (i, j), polys in sorted(geom.items()):
    out_polys = []
    for n_, p in enumerate(polys):
        verts = []
        for m_, k_ in enumerate(p):
            x_, y_ = kxy(k_)
            verts.append((x_, y_, None, k_))
        out_polys.append(verts)
    pieces[(LINES - 1) * j + i] = out_polys
# heights from the incidences
for iid, (ci_, n_, m_) in enumerate(inc_list):
    v = pieces[(LINES - 1) * ci_[1] + ci_[0]][n_][m_]
    pieces[(LINES - 1) * ci_[1] + ci_[0]][n_][m_] = (v[0], v[1], zinc[iid])
for cell in sorted(pieces):
    out = []
    for v in pieces[cell]:
        if fan_ok(v):
            out.append(v)
            continue
        for r in range(1, len(v)):
            w_ = v[r:] + v[:r]
            if fan_ok(w_):
                out.append(w_)
                n_rot += 1
                break
        else:
            tris = ear_clip(v)
            out.extend(tris)
            n_split += 1
    pieces[cell] = out
print(f'{len(pieces)} wall cells, {sum(len(v) for v in pieces.values())} pieces in {time.time() - t2:.1f} s '
      f'({inserted} T-junction points inserted, {len(pt_inc)} vertices, {n_end} wall-end points, {n_sector} sector heights, '
      f'{n_rot} pieces re-started, {n_split} cut into triangles, {unmatched} unmatched inner edges)', flush=True)


# Pass 5: the crack check (the game's own surface, emulated): along every piece edge, the edge's height against the ground
# just across it (the neighbouring piece, or the plain cell's lattice triangle), except on a declared wall (a built wall
# closes that step) and in a building footprint (a hole the shell closes). Any > 3 cm fails the bake.
def baked_piece_z(polys, x, y, at=None):
    """The game's PieceRelief at (x, y); with `at`, the plane of the triangle that holds (x, y) evaluated at `at` (the
    crack check reads the ground across an edge ON the edge, so a steep sliver beside it is not taken for a gap)."""
    best = (1e9, 0.0)
    ex_, ey_ = at if at else (x, y)
    for poly in polys:
        for k in range(1, len(poly) - 1):
            A_, B_, C_ = poly[0], poly[k], poly[k + 1]
            den = (B_[1] - C_[1]) * (A_[0] - C_[0]) + (C_[0] - B_[0]) * (A_[1] - C_[1])
            if abs(den) < 1e-6:
                continue
            wa = ((B_[1] - C_[1]) * (x - C_[0]) + (C_[0] - B_[0]) * (y - C_[1])) / den
            wb = ((C_[1] - A_[1]) * (x - C_[0]) + (A_[0] - C_[0]) * (y - C_[1])) / den
            wc = 1.0 - wa - wb
            o = max(-wa, -wb, -wc)
            if o <= 1e-4 or o < best[0]:
                va = ((B_[1] - C_[1]) * (ex_ - C_[0]) + (C_[0] - B_[0]) * (ey_ - C_[1])) / den
                vb = ((C_[1] - A_[1]) * (ex_ - C_[0]) + (A_[0] - C_[0]) * (ey_ - C_[1])) / den
                z = va * A_[2] + vb * B_[2] + (1.0 - va - vb) * C_[2]
                if o <= 1e-4:
                    return z
                best = (o, z)
    return best[1]


def baked_relief(x, y, at=None):
    fx, fy = (x + HALF) / CELL, (y + HALF) / CELL
    i_, j_ = min(max(int(math.floor(fx)), 0), LINES - 2), min(max(int(math.floor(fy)), 0), LINES - 2)
    p = pieces.get((LINES - 1) * j_ + i_)
    if p:
        return baked_piece_z(p, x, y, at)
    u, v = fx - i_, fy - j_
    tri_lo = u >= v
    if at:
        u, v = (at[0] + HALF) / CELL - i_, (at[1] + HALF) / CELL - j_
    A_, C_ = corner_z(i_, j_), corner_z(i_ + 1, j_ + 1)
    if tri_lo:
        B_ = corner_z(i_ + 1, j_)
        return A_ + (B_ - A_) * u + (C_ - B_) * v
    D_ = corner_z(i_, j_ + 1)
    return A_ + (C_ - D_) * u + (D_ - A_) * v


def near_wall(x, y, r=1.0):
    for k in WB.get((int(math.floor(x / 1600.0)), int(math.floor(y / 1600.0))), ()):
        if segs[k][1] != 'grade break' and seg_dist(x, y, segs[k][2], segs[k][3]) < r:
            return True
    return False


def in_rects(x, y, pad=2.0):
    return any(r[0] - pad <= x <= r[1] + pad and r[2] - pad <= y <= r[3] + pad for r in RECTS)


t5 = time.time()
CRACKS = []
n_chk = 0
for cell, polys in pieces.items():
    for poly in polys:
        n = len(poly)
        cx = sum(v[0] for v in poly) / n
        cy = sum(v[1] for v in poly) / n
        for m_ in range(n):
            a_, b_ = poly[m_], poly[(m_ + 1) % n]
            ex, ey = b_[0] - a_[0], b_[1] - a_[1]
            Ln = math.hypot(ex, ey)
            if Ln < 0.05:
                continue
            nx, ny = ey / Ln, -ex / Ln
            for f in (0.02, 0.1, 0.25, 0.5, 0.75, 0.9, 0.98):
                x, y = a_[0] + ex * f, a_[1] + ey * f
                if (cx - x) * nx + (cy - y) * ny > 0:
                    ox, oy = -nx, -ny
                else:
                    ox, oy = nx, ny
                if abs(x) > HALF - 0.1 and abs(x + ox) > HALF or abs(y) > HALF - 0.1 and abs(y + oy) > HALF:
                    continue           # the square's own edge: the outer land meets it (farland_check)
                if in_rects(x, y) or near_wall(x, y):
                    continue
                n_chk += 1
                z_in = a_[2] + (b_[2] - a_[2]) * f
                z_out = baked_relief(x + ox * 0.05, y + oy * 0.05, (x, y))
                if abs(z_out - z_in) > 0.5:
                    CRACKS.append((abs(z_out - z_in), x, y))
CRACKS.sort(reverse=True)
big = [c_ for c_ in CRACKS if c_[0] > 3.0]
if os.environ.get('BAKE_DIAGNOSTICS'):
    import gzip, json
    # Diagnostic geometry only. This never bypasses the crack gate or writes
    # active Source and can be inspected without repeating the full bake.
    with gzip.open(os.environ['BAKE_DIAGNOSTICS'], 'wt') as diagnostic:
        json.dump({'source_sha256': src_hash, 'cracks': CRACKS, 'pieces': pieces,
                   'lattice': lattice, 'segments': segs}, diagnostic)
print(f'crack check: {n_chk} edge samples off walls and footprints, {len(CRACKS)} over 0.5 cm, {len(big)} over 3 cm '
      f'(worst {CRACKS[0][0]:.2f} cm at ({CRACKS[0][1]:.0f}, {CRACKS[0][2]:.0f}))' if CRACKS else
      f'crack check: {n_chk} edge samples off walls and footprints, none over 0.5 cm', f'in {time.time() - t5:.1f} s', flush=True)
for c_ in big[:20]:
    print('  CRACK %.1f cm at (%.1f, %.1f)' % c_)
if big and os.environ.get('BAKE_FORCE') != '1':
    print('CRACK CHECK FAIL: %d samples over 3 cm; nothing written (BAKE_FORCE=1 writes it anyway)' % len(big))
    sys.exit(1)

# ------------------------------------------------------------------ pads, lift, stairs, ramps, reserves
pads = {bid: float(spec[0]) for bid, spec in L.BUILDINGS.items()}   # DZ = pad Z - (-60), per block Id
# The entrance aprons as the landform levels them (the layout's own rects, clipped by APRON_CLIP where a stair needs the
# room): block, face, depth out from the face, level (relief). The game builds its aprons and pads this deep.
aprons = []
for a in L.WALKS['aprons']:
    ar = list(a['rect'])
    if a['block'] in L.APRON_CLIP:
        c = L.APRON_CLIP[a['block']]
        ar = [max(ar[0], c[0]), min(ar[1], c[1]), max(ar[2], c[2]), min(ar[3], c[3])]
    depth = (ar[3] - ar[2]) if a['face'] in 'NS' else (ar[1] - ar[0])
    aprons.append((a['block'], a['face'], float(depth), float(L.BUILDINGS[a['block']][0])))
lift = float(L.LIFT)
reserves = []
for t in L.TERRACES:
    if getattr(t, 'kind', '') == 'reserve':
        xs = [p[0] for p in t.poly]
        ys = [p[1] for p in t.poly]
        cx, cy = (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0
        reserves.append((t.name, cx, cy, max(xs) - min(xs), max(ys) - min(ys), float(t.level(cx, cy))))


# stairs: the placed ones and every steep run of a walk at a wall (check_grades), each along its walk
WI = L.WallIndex(L.walls())
try:
    L.check_grades(WI)
except Exception as e:   # the checks need the whole module state; the stairs are optional for the bake
    print('check_grades failed:', e)
RUNS = {r['id']: r for r in L.WALKS['runs']}


def walk_dir_width(x, y):
    best = (1e18, (1.0, 0.0), 300.0)
    for r in L.WALKS['runs']:
        pts = r['polyline']
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            d = seg_dist(x, y, a, b)
            if d < best[0]:
                Lr = math.hypot(b[0] - a[0], b[1] - a[1]) or 1.0
                best = (d, ((b[0] - a[0]) / Lr, (b[1] - a[1]) / Lr), float(r.get('width', 300.0)))
    return best


RISER, TREAD = 16.7, 33.0


def stair_span_at_wall(sx, sy, dx, dy, zl, zh):
    """A stair at a wall projects onto the wall's LOW side along the walk: its head at the wall line, its foot out on the
    low ground, long enough for the rise at 16.7 cm risers and 33 cm treads (plus a 30 cm top landing)."""
    lo_ahead = L.relief(sx + dx * 100.0, sy + dy * 100.0) < L.relief(sx - dx * 100.0, sy - dy * 100.0)
    ux, uy = (dx, dy) if lo_ahead else (-dx, -dy)   # pointing down the stair, onto the low side
    n = max(1, int(math.ceil((zh - zl) / RISER)))
    run = (n - 1) * TREAD + 30.0
    hx, hy = sx - ux * 15.0, sy - uy * 15.0                     # head: just back on the high side of the wall
    fx, fy = sx + ux * run, sy + uy * run                       # foot: out on the low side
    return (hx, hy, zh, fx, fy, zl)


stairs = []   # (name, head x, y, relief, foot x, y, relief, width)
for name, (sx, sy), zl, zh in L.STAIRS:
    d, (dx, dy), wd = walk_dir_width(sx, sy)
    stairs.append((name,) + stair_span_at_wall(sx, sy, dx, dy, zl, zh) + (max(wd, 400.0),))
# the sunken courts' exits (landform.py COURT_EXITS: a straight stair along the court up over its end wall)
stairs.extend(getattr(L, 'court_exit_records', lambda: [])())
for wid, rr in getattr(L, 'WALK_STAIRS', []):
    (mx, my) = rr[6]
    if any(math.hypot(mx - s_[1], my - s_[2]) < 400.0 or math.hypot(mx - s_[4], my - s_[5]) < 400.0 for s_ in stairs):
        continue
    r_ = RUNS[wid]
    pts = r_['polyline']
    cum = L.polyline_cum(pts)
    (x0, y0), _t = L.polyline_at(pts, cum, rr[0])
    (x1, y1), _t = L.polyline_at(pts, cum, rr[1])
    z0, z1 = rr[2], rr[3]
    wd = float(r_.get('width', 300.0))
    if math.hypot(x1 - x0, y1 - y0) < 260.0 and abs(z1 - z0) > 30.0:
        # a step at a wall: the stair projects onto the low side
        dxu, dyu = (x1 - x0), (y1 - y0)
        Lr = math.hypot(dxu, dyu) or 1.0
        stairs.append(('walk %s stair' % wid,) + stair_span_at_wall(mx, my, dxu / Lr, dyu / Lr, min(z0, z1), max(z0, z1)) + (wd,))
    else:
        # a flight down a steep slope: head at the high end, foot at the low end
        if z0 < z1:
            x0, y0, z0, x1, y1, z1 = x1, y1, z1, x0, y0, z0
        stairs.append(('walk %s flight' % wid, x0, y0, z0, x1, y1, z1, wd))
frozen = [tuple(map(float, r)) for r in getattr(L, 'ZERO_RECTS', L.FROZEN_RECTS)]
hidden_trees = []
try:
    L.check_trees(WI)
    hidden_trees = [(float(x), float(y), why) for (_src, x, y, why) in L.TREE_ROWS]
except Exception as e:
    print('check_trees failed:', e)


def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


KINDS = ['wall', 'rock band', 'rock cut', 'rock ledge', 'dam', 'parapet / terrace wall', 'pad-court', 'terrace', 'lot', 'apron',
         'reserve', 'road abutment']


def kind_id(k):
    return KINDS.index(k) if k in KINDS else 0


# ------------------------------------------------------------------ write
lines = []
w = lines.append
w('// GENERATED by Tools/calder/bake_landform.py from Saved/CalderWork/plan/landform.py')
w(f'// (sha256 {src_hash}). Do not edit: change landform.py and bake again. Included by HouseCalderLandform.cpp only.')
w('namespace HouseCalderLandformData')
w('{')
w(f'constexpr int32 Lines = {LINES};')
w(f'constexpr float TowerLift = {lift:.1f}f;')
w('/** Relief at every lattice corner, row by row (Y), in tenths of a millimetre. */')
w(f'static const int32 Lattice[{LINES * LINES}] = {{')
for j in range(LINES):
    w(','.join(str(v) for v in lattice[j * LINES:(j + 1) * LINES]) + ',')
w('};')
# pieces: cell index, number of pieces; then for each piece its vertex count and vertices
flat_cells, flat_verts = [], []
for cell in sorted(pieces):
    ps = pieces[cell]
    flat_cells.append(f'{{{cell},{len(flat_verts)},{len(ps)}}}')
    for p in ps:
        flat_verts.append(f'{{{len(p)},0,0}}')
        for (x, y, z) in p:
            flat_verts.append(f'{{{x:.2f}f,{y:.2f}f,{z:.2f}f}}')
w('struct FCellRef { int32 Cell, First, Pieces; };')
w('struct FPieceVert { float X, Y, Z; };   // a piece starts with {count,0,0}, then its vertices (anticlockwise)')
w(f'static const FCellRef WallCells[{max(1, len(flat_cells))}] = {{')
for k in range(0, len(flat_cells), 12):
    w(','.join(flat_cells[k:k + 12]) + ',')
if not flat_cells:
    w('{-1,0,0},')
w('};')
w(f'constexpr int32 NumWallCells = {len(flat_cells)};')
w(f'static const FPieceVert PieceVerts[{max(1, len(flat_verts))}] = {{')
for k in range(0, len(flat_verts), 8):
    w(','.join(flat_verts[k:k + 8]) + ',')
if not flat_verts:
    w('{0,0,0},')
w('};')
w('struct FPad { const TCHAR* Id; float DZ; };')
w(f'static const FPad Pads[{len(pads)}] = {{')
for bid in sorted(pads):
    w(f'    {{ TEXT({cstr(bid)}), {pads[bid]:.2f}f }},')
w('};')
w(f'constexpr int32 NumPads = {len(pads)};')
w('/** The entrance aprons the landform levels: block Id, face, depth out from the face (cm), level (relief, cm). */')
w('struct FApron { const TCHAR* Id; TCHAR Face; float Depth, Level; };')
w(f'static const FApron Aprons[{max(1, len(aprons))}] = {{')
for bid, face, depth, level in aprons:
    w(f"    {{ TEXT({cstr(bid)}), TEXT('{face}'), {depth:.1f}f, {level:.1f}f }},")
if not aprons:
    w("    { TEXT(\"\"), TEXT('N'), 0.f, 0.f },")
w('};')
w(f'constexpr int32 NumAprons = {len(aprons)};')
w('struct FWall { const TCHAR* Name; int32 Kind; float AX, AY, BX, BY; };')
w('/** Kinds: ' + ', '.join(f'{k} {n}' for k, n in enumerate(KINDS)) + ' */')
w(f'static const FWall Walls[{max(1, len(physical_segs))}] = {{')
for name, kind, a, b in physical_segs:
    w(f'    {{ TEXT({cstr(name)}), {kind_id(kind)}, {a[0]:.1f}f, {a[1]:.1f}f, {b[0]:.1f}f, {b[1]:.1f}f }},')
w('};')
w(f'constexpr int32 NumWalls = {len(physical_segs)};')
w('/** A stair or flight: its head (top) and foot (bottom) in XY with the relief there (cm above today\'s grade), and its')
w(' *  width. Steps are 14-18 cm risers on 30+ cm treads, laid between head and foot. */')
w('struct FStair { const TCHAR* Name; float HX, HY, HZ, FX, FY, FZ, Width; };')
w(f'static const FStair Stairs[{max(1, len(stairs))}] = {{')
for name, hx, hy, hz, fx, fy, fz, wd in stairs:
    w(f'    {{ TEXT({cstr(name)}), {hx:.1f}f, {hy:.1f}f, {hz:.1f}f, {fx:.1f}f, {fy:.1f}f, {fz:.1f}f, {wd:.1f}f }},')
w('};')
w(f'constexpr int32 NumStairs = {len(stairs)};')
w('/** The landform\'s zero rectangles (the frozen zones and the street wall\'s verge): X0, X1, Y0, Y1. */')
w(f'static const float Frozen[{len(frozen)}][4] = {{')
for r in frozen:
    w(f'    {{ {r[0]:.1f}f, {r[1]:.1f}f, {r[2]:.1f}f, {r[3]:.1f}f }},')
w('};')
w(f'constexpr int32 NumFrozen = {len(frozen)};')
w('/** Site trees (TreeA/TreeB) the landform hides (a wall, stair, ramp, water, a reserve or a lot): their XY. */')
w(f'static const float HiddenTrees[{max(1, len(hidden_trees))}][2] = {{')
for x, y, why in hidden_trees:
    w(f'    {{ {x:.1f}f, {y:.1f}f }},   // {why}')
if not hidden_trees:
    w('    { 1e9f, 1e9f },')
w('};')
w(f'constexpr int32 NumHiddenTrees = {len(hidden_trees)};')
w('struct FRamp { const TCHAR* Name; float AX, AY, BX, BY, Z0, Z1; };')
w(f'static const FRamp Ramps[{max(1, len(L.RAMPS))}] = {{')
for name, run, z0, z1, _landing in L.RAMPS:
    (ax, ay), (bx, by) = run[0], run[-1]
    w(f'    {{ TEXT({cstr(name)}), {ax:.1f}f, {ay:.1f}f, {bx:.1f}f, {by:.1f}f, {z0:.1f}f, {z1:.1f}f }},')
w('};')
w(f'constexpr int32 NumRamps = {len(L.RAMPS)};')
w('struct FReserve { const TCHAR* Name; float X, Y, SX, SY, Z; };')
w(f'static const FReserve Reserves[{max(1, len(reserves))}] = {{')
for name, x, y, sx, sy, z in reserves:
    w(f'    {{ TEXT({cstr(name)}), {x:.1f}f, {y:.1f}f, {sx:.1f}f, {sy:.1f}f, {z:.1f}f }},')
if not reserves:
    w('    { TEXT(""), 0.f, 0.f, 0.f, 0.f, 0.f },')
w('};')
w(f'constexpr int32 NumReserves = {len(reserves)};')
# ------------------------------------------------------------------ the builders' data (landform.py exports())
# Water, roadside, edges, spots and levels, computed by landform.py on the game's own ground (this bake's lattice and
# pieces), written as plain tables; HouseCalderLandform.h reads them (Saved/CalderWork/plan/data_exports.md).
t6 = time.time()


def baked_abs(x, y):
    return L.grade_fast(x, y) + baked_relief(x, y)


EX = L.exports(baked_abs)
# the lowest walkable ground the game has: every lattice corner and piece vertex outside the footprints (the surface is
# linear between them)
low = (1e9, None)
for j in range(LINES):
    yv = corner(j)
    for i in range(LINES):
        xv = corner(i)
        if any(r[0] < xv < r[1] and r[2] < yv < r[3] for r in RECTS):
            continue
        z = L.grade_fast(xv, yv) + corner_z(i, j)
        if z < low[0]:
            low = (z, (xv, yv))
for cell_, polys_ in pieces.items():
    for poly_ in polys_:
        for (xv, yv, zv) in poly_:
            if any(r[0] < xv < r[1] and r[2] < yv < r[3] for r in RECTS):
                continue
            z = L.grade_fast(xv, yv) + zv
            if z < low[0]:
                low = (z, (xv, yv))
print(f'exports in {time.time() - t6:.1f} s: lowest walkable ground {low[0]:.1f} at {low[1]}; loop {len(EX["loop"]["pts"])} points, '
      f'{len(EX["stones"])} stones, {len(EX["guardrails"])} guardrails, {len(EX["ditches"])} ditches, {len(EX["basins"])} basins, '
      f'{len(EX["culverts"])} culverts, {len(EX["needs_rail"])} rails, {len(EX["closures"])} closures, {len(EX["hedges"])} hedge '
      f'segments', flush=True)


def f1(v):
    return f'{v:.1f}f'


def f2(v):
    return f'{v:.2f}f'


w('// ===================================================================== the builders\' data (landform.py exports())')
w('// Absolute Z (UE cm: today\'s grade + relief) unless a name says relief. Valid with the Calder terrain on.')
w(f'constexpr float PondWaterZ = {f2(EX["pond"]["water"])};')
w(f'static const float PondOutline[{len(EX["pond"]["outline"])}][2] = {{')
for x, y in EX['pond']['outline']:
    w(f'    {{ {f1(x)}, {f1(y)} }},')
w('};')
w('/** A water centreline point: XY, the water surface (absolute), the water\'s half-width, its depth at the centre, and the')
w(' *  arc length from the line\'s start. */')
w('struct FWaterPt { float X, Y, WaterZ, HalfWidth, Depth, S; };')
cum_c = L.polyline_cum(L.CREEK)
w(f'static const FWaterPt CreekPts[{len(EX["creek"])}] = {{')
for k, p in enumerate(EX['creek']):
    w(f'    {{ {f1(p["x"])}, {f1(p["y"])}, {f2(p["water"])}, {f1(p["half"])}, {f1(p["depth"])}, {f1(cum_c[k])} }},')
w('};')
w(f'static const FWaterPt GlenPts[{len(EX["glen"])}] = {{')
for p in EX['glen']:
    w(f'    {{ {f1(p["x"])}, {f1(p["y"])}, {f2(p["water"])}, {f1(p["half"])}, {f1(p["depth"])}, {f1(p["s"])} }},')
w('};')
w('/** A ledge the water drops over: the lip (the water\'s width there, A-B), the water above and below it, the declared')
w(' *  rock wall\'s extent, the line the curtain falls from (the falls: the caprock overhang\'s edge), the flow direction. */')
w('struct FLedge { const TCHAR* Name; float AX, AY, BX, BY, TopZ, BottomZ, WallAX, WallAY, WallBX, WallBY, CurtainY, FlowX, FlowY; };')
w('static const FLedge Ledges[2] = {')
fa, ca, rh = EX['falls'], EX['cascade'], EX['rock_house']
for nm, l_, cy_ in (('Calder Falls', fa, rh['overhang_y']), ('the cascade', ca, ca['a'][1])):
    w(f'    {{ TEXT({cstr(nm)}), {f1(l_["a"][0])}, {f1(l_["a"][1])}, {f1(l_["b"][0])}, {f1(l_["b"][1])}, {f2(l_["top"])}, '
      f'{f2(l_["bottom"])}, {f1(l_["wall_a"][0])}, {f1(l_["wall_a"][1])}, {f1(l_["wall_b"][0])}, {f1(l_["wall_b"][1])}, '
      f'{f1(cy_)}, {l_["flow"][0]:.4f}f, {l_["flow"][1]:.4f}f }},')
w('};')
pp = EX['plunge_pool']
w('struct FPool { float X, Y, Radius, WaterZ, Depth, DeepFromY; };')
w(f'static const FPool PlungePool = {{ {f1(pp["centre"][0])}, {f1(pp["centre"][1])}, {f1(pp["radius"])}, {f2(pp["water"])}, '
  f'{f1(pp["depth"])}, {f1(pp["deep_from_y"])} }};')
hw_ = EX['headwall']
w('struct FHeadwall { float X, Y, FlowX, FlowY, WaterZ, InvertZ, WaterHalf, FloorHalf, WingWX, WingEX, GrateWidth; };')
w(f'static const FHeadwall Headwall = {{ {f1(hw_["centre"][0])}, {f1(hw_["centre"][1])}, {hw_["flow"][0]:.4f}f, '
  f'{hw_["flow"][1]:.4f}f, {f2(hw_["water"])}, {f2(hw_["invert"])}, {f1(hw_["water_half"])}, {f1(hw_["floor_half"])}, '
  f'{f1(hw_["wing_w"][0])}, {f1(hw_["wing_e"][0])}, {f1(hw_["grate_w"])} }};')
CROSS_KINDS = ['culvert', 'arch', 'timber', 'road']
w('/** Kinds: 0 the loop\'s causeway with a pipe culvert, 1 the stone-arch footbridge on the dam, 2 the timber footbridge. */')
w('struct FCrossing { const TCHAR* Name; int32 Kind; float X, Y, AxisX, AxisY, Back, On, Width, DeckZ, WaterZ, InvertZ, EndZ0, EndZ1, Pipe; };')
w(f'static const FCrossing Crossings[{len(EX["crossings"])}] = {{')
for c in EX['crossings']:
    w(f'    {{ TEXT({cstr(c["name"])}), {CROSS_KINDS.index(c["kind"])}, {f1(c["centre"][0])}, {f1(c["centre"][1])}, '
      f'{c["axis"][0]:.4f}f, {c["axis"][1]:.4f}f, {f1(c["back"])}, {f1(c["on"])}, {f1(c["width"])}, {f2(c["deck"])}, '
      f'{f2(c["water"])}, {f2(c["invert"])}, {f2(c["ends"][0])}, {f2(c["ends"][1])}, {f1(c["pipe"])} }},')
w('};')
dm = EX['dam']
w(f'static const float DamCrest[{len(dm["crest"])}][2] = {{ ' + ', '.join(f'{{ {f1(x)}, {f1(y)} }}' for x, y in dm['crest']) + ' };')
w(f'constexpr float DamCrestZ = {f2(dm["crest_z"])}, DamCrestWidth = {f1(dm["width"])}, DamNotchWidth = {f1(dm["notch"][1])}, '
  f'DamOutletZ = {f2(dm["outlet_pool"])};')
rc = EX['race']
w(f'static const float RacePts[{len(rc["pts"])}][2] = {{ ' + ', '.join(f'{{ {f1(x)}, {f1(y)} }}' for x, y in rc['pts']) + ' };')
w(f'constexpr float RaceWaterZ = {f2(rc["water"])}, RaceHalfWidth = {f1(rc["half"])}, RaceFloorHalfWidth = {f1(rc["floor_half"])};')
stone_names = []
for st in EX['stones']:
    if st['crossing'] not in stone_names:
        stone_names.append(st['crossing'])
w(f'static const TCHAR* const StoneCrossings[{len(stone_names)}] = {{ ' + ', '.join(f'TEXT({cstr(n_)})' for n_ in stone_names) + ' };')
w('struct FStone { int32 Crossing; float X, Y, TopZ, BedZ, Radius, Yaw; };')
w(f'static const FStone Stones[{len(EX["stones"])}] = {{')
for st in EX['stones']:
    w(f'    {{ {stone_names.index(st["crossing"])}, {f1(st["x"])}, {f1(st["y"])}, {f2(st["top"])}, {f2(st["bed"])}, '
      f'{f1(st["r"])}, {f1(st["yaw"])} }},')
w('};')
dk = EX['dock']
rail_from = max(0.0, (L.RAIL_DROP - (L.DOCK_DECK - L.WP)) / 0.17)
w('struct FDock { float LandX, LandY, EdgeX, EdgeY, TipX, TipY, AxisX, AxisY, Width, DeckZ, WaterZ, TipDepth, RailFromEdge; };')
w(f'static const FDock Dock = {{ {f1(dk["land"][0])}, {f1(dk["land"][1])}, {f1(dk["edge"][0])}, {f1(dk["edge"][1])}, '
  f'{f1(dk["tip"][0])}, {f1(dk["tip"][1])}, {dk["axis"][0]:.4f}f, {dk["axis"][1]:.4f}f, {f1(dk["width"])}, {f2(dk["deck"])}, '
  f'{f2(dk["water"])}, {f1(dk["tip_depth"])}, {f1(rail_from)} }};')
w('struct FRockHouse { float X0, X1, FaceY, OverhangY, ShelfY, WetY, FloorZ, PoolZ, PathAX, PathAY, PathBX, PathBY, RailAX, RailAY, RailBX, RailBY; };')
w(f'static const FRockHouse RockHouse = {{ {f1(rh["x0"])}, {f1(rh["x1"])}, {f1(rh["face_y"])}, {f1(rh["overhang_y"])}, '
  f'{f1(rh["shelf_y"])}, {f1(rh["wet_y"])}, {f2(rh["floor"])}, {f2(rh["pool"])}, {f1(rh["path"][0][0])}, {f1(rh["path"][0][1])}, '
  f'{f1(rh["path"][1][0])}, {f1(rh["path"][1][1])}, {f1(rh["rail"][0][0])}, {f1(rh["rail"][0][1])}, {f1(rh["rail"][1][0])}, '
  f'{f1(rh["rail"][1][1])} }};')
# paths: the pond loop (closed) and the Crow Ledge trail
STRETCH = ['path', 'dam', 'arch', 'culvert']
lp = EX['loop']
paths = [('Gander Pond walking loop', lp['pts'], lp['kinds'] + ['path'], lp['width'], True),
         ('the Crow Ledge trail', EX['crow']['trail'], ['path'] * len(EX['crow']['trail']), 150.0, False)]
w('/** A landform path\'s vertex and the kind of the stretch from it to the next: 0 gravel on the ground (the drape paves it),')
w(' *  1 on the dam\'s crest walk, 2 over the arch footbridge, 3 over the causeway. */')
w('struct FPathVert { float X, Y; int32 Stretch; };')
flat = []
refs = []
for nm, pts_, kinds_, wd, closed in paths:
    refs.append((nm, len(flat), len(pts_), wd, closed))
    for (x, y), k in zip(pts_, kinds_):
        flat.append(f'{{ {f1(x)}, {f1(y)}, {STRETCH.index(k)} }}')
w(f'static const FPathVert PathVerts[{len(flat)}] = {{')
for k in range(0, len(flat), 6):
    w('    ' + ', '.join(flat[k:k + 6]) + ',')
w('};')
w('struct FPathRef { const TCHAR* Name; int32 First, Count; float Width; bool bClosed; };')
w(f'static const FPathRef Paths[{len(refs)}] = {{')
for nm, first, cnt, wd, closed in refs:
    w(f'    {{ TEXT({cstr(nm)}), {first}, {cnt}, {f1(wd)}, {"true" if closed else "false"} }},')
w('};')
w(f'constexpr int32 NumPaths = {len(refs)};')
# Optional road paving: dummy storage keeps the pre-road data buildable while
# the runtime endpoint survey is refreshed. NumRoads remains exactly zero there.
road_points, road_poly_points, road_polys, road_refs = [], [], [], []
for road in EX.get('roads', []):
    first_point, first_poly = len(road_points), len(road_polys)
    road_points.extend(road['points'])
    for polygon in road['polygons']:
        road_polys.append((len(road_poly_points), len(polygon)))
        road_poly_points.extend(polygon)
    road_refs.append((road['name'], first_point, len(road['points']), first_poly,
                      len(road['polygons']), road['mode'], road['width']))
w(f'static const float RoadPoints[{max(1,len(road_points))}][2] = {{')
for x,y in road_points:
    w(f'    {{ {f2(x)}, {f2(y)} }},')
if not road_points: w('    { 0.f, 0.f },')
w('};')
w(f'static const float RoadPolyPoints[{max(1,len(road_poly_points))}][2] = {{')
for x,y in road_poly_points:
    w(f'    {{ {f2(x)}, {f2(y)} }},')
if not road_poly_points: w('    { 0.f, 0.f },')
w('};')
w('struct FRoadPoly { int32 First, Count; };')
w(f'static const FRoadPoly RoadPolys[{max(1,len(road_polys))}] = {{')
for first,count in road_polys: w(f'    {{ {first}, {count} }},')
if not road_polys: w('    { 0, 0 },')
w('};')
w('struct FRoadRef { const TCHAR* Name; int32 FirstPoint, PointCount, FirstPoly, PolyCount, Mode; float Width; };')
w(f'static const FRoadRef Roads[{max(1,len(road_refs))}] = {{')
for name,first,count,poly,poly_count,mode,width in road_refs:
    w(f'    {{ TEXT({cstr(name)}), {first}, {count}, {poly}, {poly_count}, {mode}, {f1(width)} }},')
if not road_refs: w('    { TEXT(""), 0, 0, 0, 0, 0, 0.f },')
w('};')
w(f'constexpr int32 NumRoads = {len(road_refs)};')
cr = EX['crow']
w(f'static const float CrowLedgeOutline[4][2] = {{ ' + ', '.join(f'{{ {f1(x)}, {f1(y)} }}' for x, y in cr['ledge']) + ' };')
w(f'constexpr float CrowLedgeZ = {f2(cr["level"])};')
# roadside runs: one point pool
RS_KINDS = ['guardrail', 'ditch', 'rail']
rpts, rrefs = [], []
for kind_, lst in (('guardrail', EX['guardrails']), ('ditch', EX['ditches']), ('rail', EX['needs_rail'])):
    for r_ in lst:
        nm = r_.get('road') or r_.get('what')
        val = r_.get('drop', r_.get('rise', 0.0))
        flag = 1 if r_.get('wall') else 0
        if kind_ == 'ditch' and r_.get('walk'):
            flag |= 2
        rrefs.append((nm, RS_KINDS.index(kind_), r_['side'], val, len(rpts), len(r_['pts']), flag))
        rpts += r_['pts']
w('/** Roadside runs: Kind 0 a W-beam guardrail (a drop over 1.5 m within 3 m of a road), 1 a ditch on the uphill side of a')
w(' *  cut, 2 a drop of 75 cm+ beside a walk/road/structure that needs a 90 cm rail. Side +1 left of the line\'s direction,')
w(' *  -1 right, 0 across its end. Value: the drop (or the cut\'s rise), cm. Flags: 1 the drop or cut is a declared wall')
w(' *  (its parapet guards it / its weep holes drain it), 2 a walk\'s ditch. Points: RoadsidePts[First .. First+Count). */')
w(f'static const float RoadsidePts[{max(1, len(rpts))}][2] = {{')
for k in range(0, len(rpts), 6):
    w('    ' + ', '.join(f'{{ {f1(x)}, {f1(y)} }}' for x, y in rpts[k:k + 6]) + ',')
w('};')
w('struct FRoadsideRef { const TCHAR* Name; int32 Kind, Side; float Value; int32 First, Count, Flags; };')
w(f'static const FRoadsideRef Roadside[{max(1, len(rrefs))}] = {{')
for nm, k_, sd, val, first, cnt, flag in rrefs:
    w(f'    {{ TEXT({cstr(nm)}), {k_}, {sd}, {f1(val)}, {first}, {cnt}, {flag} }},')
w('};')
w(f'constexpr int32 NumRoadside = {len(rrefs)};')
w('struct FBasin { const TCHAR* Name; float X, Y, Z; bool bLot; };')
w(f'static const FBasin Basins[{max(1, len(EX["basins"]))}] = {{')
for b_ in EX['basins']:
    w(f'    {{ TEXT({cstr(b_["road"])}), {f1(b_["x"])}, {f1(b_["y"])}, {f2(b_["z"])}, {"true" if b_["kind"] != "road sag" else "false"} }},')
w('};')
w(f'constexpr int32 NumBasins = {len(EX["basins"])};')
w('struct FCulvertX { const TCHAR* Ditch; const TCHAR* Under; float X, Y, AxisX, AxisY, Length, RoadZ; int32 Side; };')
w(f'static const FCulvertX Culverts[{max(1, len(EX["culverts"]))}] = {{')
for c in EX['culverts']:
    w(f'    {{ TEXT({cstr(c["ditch"])}), TEXT({cstr(c["under"])}), {f1(c["x"])}, {f1(c["y"])}, {c["axis"][0]:.4f}f, '
      f'{c["axis"][1]:.4f}f, {f1(c["length"])}, {f2(c["z"])}, {c["side"]} }},')
w('};')
w(f'constexpr int32 NumCulverts = {len(EX["culverts"])};')
# edges
CL_KINDS = ['rock band', 'rockslide across Calder Dr', 'boulder choke over Calder Branch']
cpts, crefs = [], []
for c in EX['closures']:
    crefs.append((c['edge'], CL_KINDS.index(c['kind']), len(cpts), len(c['pts'])))
    cpts += c['pts']
w('/** Edge closures (Kind 0 an Unwalkable rock band, 1 a rockslide across Calder Dr, 2 a boulder choke over Calder Branch),')
w(' *  3 m in from the square\'s edge: points ClosurePts[First .. First+Count). */')
w(f'static const float ClosurePts[{max(1, len(cpts))}][2] = {{')
for k in range(0, len(cpts), 6):
    w('    ' + ', '.join(f'{{ {f1(x)}, {f1(y)} }}' for x, y in cpts[k:k + 6]) + ',')
w('};')
w('struct FClosureRef { const TCHAR* Edge; int32 Kind, First, Count; };')
w(f'static const FClosureRef Closures[{max(1, len(crefs))}] = {{')
for e_, k_, first, cnt in crefs:
    w(f'    {{ TEXT({cstr(e_)}), {k_}, {first}, {cnt} }},')
w('};')
w(f'constexpr int32 NumClosures = {len(crefs)};')
hedge_ids = [h['id'] for h in L.SITE['hedges']]
w(f'static const TCHAR* const HedgeNames[{len(hedge_ids)}] = {{ ' + ', '.join(f'TEXT({cstr(n_)})' for n_ in hedge_ids) + ' };')
w('struct FHedgeSeg { int32 Hedge; float AX, AY, BX, BY, BottomZ, TopZ; bool bFrozen; };')
w(f'static const FHedgeSeg HedgeSegs[{len(EX["hedges"])}] = {{')
for h in EX['hedges']:
    w(f'    {{ {hedge_ids.index(h["hedge"])}, {f1(h["a"][0])}, {f1(h["a"][1])}, {f1(h["b"][0])}, {f1(h["b"][1])}, '
      f'{f2(h["bottom"])}, {f2(h["top"])}, {"true" if h["frozen"] else "false"} }},')
w('};')
w(f'constexpr int32 NumHedgeSegs = {len(EX["hedges"])};')
gaps_ = [(hedge_ids.index(hid), g[0], g[1], g[2]) for hid, gl in EX['hedge_gaps'].items() for g in gl]
w('struct FHedgeGap { int32 Hedge; float From, To; const TCHAR* Why; };')
w(f'static const FHedgeGap HedgeGaps[{max(1, len(gaps_))}] = {{')
for hi, a_, b_, why in gaps_:
    w(f'    {{ {hi}, {f1(a_)}, {f1(b_)}, TEXT({cstr(why)}) }},')
w('};')
w(f'constexpr int32 NumHedgeGaps = {len(gaps_)};')
w('struct FSpot { const TCHAR* Name; float X, Y, Z, Yaw, GroundZ; };')
w(f'static const FSpot Overlooks[{len(EX["overlooks"])}] = {{')
for o in EX['overlooks']:
    w(f'    {{ TEXT({cstr(o["name"])}), {f1(o["x"])}, {f1(o["y"])}, {f2(o["z"])}, {f1(o["yaw"])}, {f2(baked_abs(o["x"], o["y"]))} }},')
w('};')
w(f'static const FSpot EyeSpots[{len(EX["eyes"])}] = {{')
for e_ in EX['eyes']:
    w(f'    {{ TEXT({cstr(e_["id"] + ": " + e_["desc"])}), {f1(e_["x"])}, {f1(e_["y"])}, {f2(e_["eye"])}, {f1(e_["yaw"])}, '
      f'{f2(e_["ground"])} }},')
w('};')
w(f'constexpr float LowestWalkableZ = {f2(low[0])}, LowestWalkableX = {f1(low[1][0])}, LowestWalkableY = {f1(low[1][1])};')
w(f'constexpr float FallLimitZ = {f2(EX["levels"]["fall_limit"])}, MinWalkableZ = {f2(EX["levels"]["min_walk_rule"])};')
w('}')
if os.path.exists(OUT):
    os.remove(OUT)
open(OUT, 'w').write('\n'.join(lines) + '\n')
print(f'wrote {OUT} ({os.path.getsize(OUT) / 1e6:.2f} MB): walls={len(physical_segs)} gradeBreaks={len(segs)-len(physical_segs)} wallCells={len(pieces)} pads={len(pads)} '
      f'aprons={len(aprons)} stairs={len(stairs)} frozen={len(frozen)} hiddenTrees={len(hidden_trees)} ramps={len(L.RAMPS)} reserves={len(reserves)} in {time.time() - t0:.1f} s')
