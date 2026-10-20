"""Tiny pure-Python plan renderer (no PIL/numpy): world-space rectangles, polygons, lines, circles and 5x7 text to PNG.

World coordinates are Unreal cm (X east, Y north-ish). The image is drawn north-up: +Y is UP the image, +X to the right,
so plans read like a map. Usage: c = Canvas(xmin, xmax, ymin, ymax, px_per_m); c.rect(...); c.save('out.png').
"""
import math
import struct
import zlib

FONT = {  # 5x7, rows top to bottom, bit 4 = left column
    'A': [14, 17, 17, 31, 17, 17, 17], 'B': [30, 17, 17, 30, 17, 17, 30], 'C': [14, 17, 16, 16, 16, 17, 14], 'D': [28, 18, 17, 17, 17, 18, 28],
    'E': [31, 16, 16, 30, 16, 16, 31], 'F': [31, 16, 16, 30, 16, 16, 16], 'G': [14, 17, 16, 23, 17, 17, 15], 'H': [17, 17, 17, 31, 17, 17, 17],
    'I': [14, 4, 4, 4, 4, 4, 14], 'J': [7, 2, 2, 2, 2, 18, 12], 'K': [17, 18, 20, 24, 20, 18, 17], 'L': [16, 16, 16, 16, 16, 16, 31],
    'M': [17, 27, 21, 21, 17, 17, 17], 'N': [17, 17, 25, 21, 19, 17, 17], 'O': [14, 17, 17, 17, 17, 17, 14], 'P': [30, 17, 17, 30, 16, 16, 16],
    'Q': [14, 17, 17, 17, 21, 18, 13], 'R': [30, 17, 17, 30, 20, 18, 17], 'S': [15, 16, 16, 14, 1, 1, 30], 'T': [31, 4, 4, 4, 4, 4, 4],
    'U': [17, 17, 17, 17, 17, 17, 14], 'V': [17, 17, 17, 17, 17, 10, 4], 'W': [17, 17, 17, 21, 21, 21, 10], 'X': [17, 17, 10, 4, 10, 17, 17],
    'Y': [17, 17, 10, 4, 4, 4, 4], 'Z': [31, 1, 2, 4, 8, 16, 31], '0': [14, 17, 19, 21, 25, 17, 14], '1': [4, 12, 4, 4, 4, 4, 14],
    '2': [14, 17, 1, 2, 4, 8, 31], '3': [31, 2, 4, 2, 1, 17, 14], '4': [2, 6, 10, 18, 31, 2, 2], '5': [31, 16, 30, 1, 1, 17, 14],
    '6': [6, 8, 16, 30, 17, 17, 14], '7': [31, 1, 2, 4, 8, 8, 8], '8': [14, 17, 17, 14, 17, 17, 14], '9': [14, 17, 17, 15, 1, 2, 12],
    ' ': [0] * 7, '-': [0, 0, 0, 31, 0, 0, 0], '.': [0, 0, 0, 0, 0, 12, 12], ':': [0, 12, 12, 0, 12, 12, 0], '/': [1, 1, 2, 4, 8, 16, 16],
    '(': [2, 4, 8, 8, 8, 4, 2], ')': [8, 4, 2, 2, 2, 4, 8], ',': [0, 0, 0, 0, 12, 4, 8], "'": [12, 12, 4, 8, 0, 0, 0], '#': [10, 10, 31, 10, 31, 10, 10],
    '+': [0, 4, 4, 31, 4, 4, 0], '=': [0, 0, 31, 0, 31, 0, 0], '>': [8, 4, 2, 1, 2, 4, 8], '<': [2, 4, 8, 16, 8, 4, 2], '?': [14, 17, 1, 2, 4, 0, 4],
    '!': [4, 4, 4, 4, 4, 0, 4], '&': [12, 18, 20, 8, 21, 18, 13], '_': [0, 0, 0, 0, 0, 0, 31],
}


class Canvas:
    def __init__(self, xmin, xmax, ymin, ymax, px_per_m=2.0, bg=(236, 232, 220), engine_view=True):
        # engine_view: like the editor's top view (+X right, +Y DOWN), so the campus (-Y) is at the top, as seen from the house.
        self.engine_view = engine_view
        self.xmin, self.xmax, self.ymin, self.ymax = xmin, xmax, ymin, ymax
        self.s = px_per_m / 100.0  # px per cm
        self.w = int((xmax - xmin) * self.s) + 1
        self.h = int((ymax - ymin) * self.s) + 1
        self.px = bytearray(bytes(bg) * (self.w * self.h))

    # world -> pixel (north up)
    def P(self, x, y):
        return (x - self.xmin) * self.s, ((y - self.ymin) if self.engine_view else (self.ymax - y)) * self.s

    def _set(self, x, y, c, a=1.0):
        if 0 <= x < self.w and 0 <= y < self.h:
            i = (y * self.w + x) * 3
            if a >= 1.0:
                self.px[i:i + 3] = bytes(c)
            else:
                for k in range(3):
                    self.px[i + k] = int(self.px[i + k] + (c[k] - self.px[i + k]) * a)

    def fill_px_rect(self, x0, y0, x1, y1, c, a=1.0):
        for y in range(max(0, int(y0)), min(self.h, int(math.ceil(y1)))):
            for x in range(max(0, int(x0)), min(self.w, int(math.ceil(x1)))):
                self._set(x, y, c, a)

    def rect(self, x0, x1, y0, y1, c, a=1.0, outline=None):
        (ax, ay), (bx, by) = self.P(min(x0, x1), min(y0, y1)), self.P(max(x0, x1), max(y0, y1))
        self.fill_px_rect(min(ax, bx), min(ay, by), max(ax, bx), max(ay, by), c, a)
        if outline:
            self.poly([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], None, outline=outline)

    def poly(self, pts, c, a=1.0, outline=None, width=1):
        P = [self.P(*p) for p in pts]
        if c is not None:
            ys = [p[1] for p in P]
            for y in range(max(0, int(min(ys))), min(self.h, int(max(ys)) + 1)):
                xs = []
                for i in range(len(P)):
                    (x0, y0), (x1, y1) = P[i], P[(i + 1) % len(P)]
                    if (y0 <= y + .5 < y1) or (y1 <= y + .5 < y0):
                        xs.append(x0 + (y + .5 - y0) * (x1 - x0) / (y1 - y0))
                xs.sort()
                for k in range(0, len(xs) - 1, 2):
                    for x in range(max(0, int(xs[k] + .5)), min(self.w, int(xs[k + 1] + .5))):
                        self._set(x, y, c, a)
        if outline:
            for i in range(len(pts)):
                self.line(pts[i], pts[(i + 1) % len(pts)], outline, width)

    def line(self, a, b, c, width=1, world_width=None):
        (x0, y0), (x1, y1) = self.P(*a), self.P(*b)
        if world_width:
            width = max(1, world_width * self.s)
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        r = width / 2.0
        for i in range(n + 1):
            t = i / n
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            self.fill_px_rect(x - r + .5, y - r + .5, x + r + .5, y + r + .5, c)

    def circle(self, cx, cy, r_cm, c, a=1.0, outline=None):
        px, py = self.P(cx, cy)
        r = r_cm * self.s
        for y in range(int(py - r) - 1, int(py + r) + 2):
            for x in range(int(px - r) - 1, int(px + r) + 2):
                d = math.hypot(x + .5 - px, y + .5 - py)
                if c is not None and d <= r:
                    self._set(x, y, c, a)
                elif outline and r - 1.2 <= d <= r + .2:
                    self._set(x, y, outline)

    def ellipse(self, cx, cy, rx, ry, c, a=1.0, outline=None, seg=96):
        pts = [(cx + math.cos(2 * math.pi * k / seg) * rx, cy + math.sin(2 * math.pi * k / seg) * ry) for k in range(seg)]
        self.poly(pts, c, a, outline)

    def text(self, x, y, s, c=(20, 20, 20), scale=2, center=False, world=True, bg=None):
        if world:
            x, y = self.P(x, y)
        s = str(s).upper()
        w = len(s) * 6 * scale - scale
        if center:
            x -= w / 2
            y -= 7 * scale / 2
        if bg:
            self.fill_px_rect(x - 2, y - 2, x + w + 2, y + 7 * scale + 2, bg, .85)
        for k, ch in enumerate(s):
            rows = FONT.get(ch, FONT['?'])
            for r in range(7):
                for col in range(5):
                    if rows[r] >> (4 - col) & 1:
                        self.fill_px_rect(x + (k * 6 + col) * scale, y + r * scale, x + (k * 6 + col + 1) * scale, y + (r + 1) * scale, c)

    def grid(self, step_cm, c=(210, 205, 192), label=True):
        x = math.ceil(self.xmin / step_cm) * step_cm
        while x <= self.xmax:
            self.line((x, self.ymin), (x, self.ymax), c)
            if label:
                self.text(x + 150, self.ymin + 300, int(x), (150, 145, 130), 1)
            x += step_cm
        y = math.ceil(self.ymin / step_cm) * step_cm
        while y <= self.ymax:
            self.line((self.xmin, y), (self.xmax, y), c)
            if label:
                self.text(self.xmin + 150, y + 150, int(y), (150, 145, 130), 1)
            y += step_cm

    def save(self, path):
        raw = b''.join(b'\x00' + bytes(self.px[y * self.w * 3:(y + 1) * self.w * 3]) for y in range(self.h))
        def chunk(t, d):
            return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', self.w, self.h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
        open(path, 'wb').write(png)
        return path
