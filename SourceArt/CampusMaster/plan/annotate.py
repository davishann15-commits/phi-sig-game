"""Label the in-game top-down capture (detail_plan_campus, orthographic, unlit) with the plan's names.

Pure Python: reads the 8-bit RGB/RGBA PNG the engine writes, draws labels with plandraw's 5x7 font, writes a PNG.
Usage: python3 annotate.py <capture.png> <out.png>
The capture's frame: centre (0, -17000), 23700 cm tall over 720 px, 1280 px wide, +Y down (the engine's top view).
"""
import struct
import sys
import zlib

sys.path.insert(0, '.')
from plandraw import Canvas

BUILDINGS = [  # id, label, x0, x1, y0, y1  (HouseCampusLayout.cpp, revision 2)
    ('LIB', 'MERCER LIBRARY', -19000, -15600, -13300, -8300), ('MAIN', 'OLD MAIN', -15200, -11400, -9700, -7100),
    ('HAR', 'HARLOW HALL', -10600, -6800, -9450, -7100), ('HLTH', 'HEALTH', -4200, -1800, -8700, -7100),
    ('COM', 'THE COMMONS', 11000, 14200, -10000, -7100), ('ASH', 'ASHBY HALL', 14700, 18900, -10400, -7100),
    ('SCI', 'HOLLIS SCIENCE', -15200, -10600, -17400, -14200), ('HUM', 'CALDER HALL', -19000, -15800, -18600, -14400),
    ('UNI', 'GOOSE UNION', -8200, -4300, -14600, -10700), ('KER', 'KERR HALL', 11000, 14400, -15400, -11400),
    ('FLD', 'FIELDHOUSE', 15000, 18900, -17000, -12200), ('ART', 'WHITCOMB ARTS', -13250, -9050, -21400, -18400),
    ('MUS', 'MUSIC', -7750, -4150, -20250, -17450), ('ENG', 'PRATT ENG', -10000, -6400, -24750, -21950),
    ('PLT', 'PHYSICAL PLANT', 11750, 15950, -21250, -18250), ('ALU', 'ALUMNI HALL', -19000, -15600, -24500, -21800),
]
LABELS = [  # text, x, y
    ('CALDER STATE STADIUM', 2400, -13300), ('CLOCK TOWER', -9000, -11500), ('SOUTH LOT', 3450, -22800),
    ('CALDER DR', 9500, -12000), ('STADIUM DR', 4000, -19000), ('BUS LOOP', -1000, -20700), ('WEST RD', -20380, -15000),
    ('SOUTH LANE', -6000, -26900), ('VISITOR LOT', -5400, -8900), ('ASHBY LOT', 16950, -11900), ('PROMENADE', -15500, -6200),
    ('THE QUAD', -12400, -12300), ('FRATERNITY PARK DR', -14000, -5100), ('TICKET PLAZA', 2400, -9550), ('PLANT YARD', 13100, -22500),
]


def read_png(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos, idat, w = 8, b'', 0
    while pos < len(data):
        n, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b'IHDR':
            w, h, depth, ctype, _, _, interlace = struct.unpack('>IIBBBBB', body)
            assert depth == 8 and ctype in (2, 6) and interlace == 0, (depth, ctype, interlace)
            bpp = 3 if ctype == 2 else 4
        elif kind == b'IDAT':
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * bpp
    out = bytearray(w * h * 3)
    prev = bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        for x in range(w):
            out[(y * w + x) * 3:(y * w + x) * 3 + 3] = line[x * bpp:x * bpp + 3]
        prev = line
    return w, h, out


def main(src, dst):
    w, h, px = read_png(src)
    ppm = h / 23700.0 * 100.0
    half_w = w / 2 / (ppm / 100.0)
    c = Canvas(-half_w, half_w, -17000 - 11850, -17000 + 11850, px_per_m=ppm)
    for y in range(min(h, c.h)):   # the canvas may be a pixel larger than the capture: copy row by row
        n = min(w, c.w) * 3
        c.px[y * c.w * 3:y * c.w * 3 + n] = px[y * w * 3:y * w * 3 + n]
    for bid, name, x0, x1, y0, y1 in BUILDINGS:
        c.text((x0 + x1) / 2, (y0 + y1) / 2 + 150, name, (255, 240, 200), 1, center=True, bg=(40, 30, 25))
    for text, x, y in LABELS:
        c.text(x, y, text, (20, 20, 20), 1, center=True, bg=(240, 236, 220))
    c.text(-half_w + 300, -17000 - 11850 + 400, 'CALDER STATE - CAMPUS PLAN (IN-GAME TOP VIEW, REVISION 2)', (20, 20, 20), 2, bg=(240, 236, 220))
    c.save(dst)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
