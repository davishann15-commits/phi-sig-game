"""Side-by-side comparison image: two 1280x720 engine captures halved and placed next to each other with captions.

Pure Python (reads PNGs with annotate.read_png, writes with plandraw.Canvas).
Usage: python3 compose.py <left.png> <left caption> <right.png> <right caption> <out.png> [title]
"""
import sys
sys.path.insert(0, '.')
from annotate import read_png
from plandraw import Canvas


def half(w, h, px):
    """2x2 box filter: (w/2, h/2, rgb bytes)."""
    hw, hh = w // 2, h // 2
    out = bytearray(hw * hh * 3)
    for y in range(hh):
        r0, r1 = (2 * y) * w * 3, (2 * y + 1) * w * 3
        for x in range(hw):
            i = 2 * x * 3
            for c in range(3):
                out[(y * hw + x) * 3 + c] = (px[r0 + i + c] + px[r0 + i + 3 + c] + px[r1 + i + c] + px[r1 + i + 3 + c]) // 4
    return hw, hh, out


def main(left, lcap, right, rcap, dst, title=''):
    lw, lh, lpx = half(*read_png(left))
    rw, rh, rpx = half(*read_png(right))
    band = 46
    W, H = lw + rw + 12, max(lh, rh) + band * 2
    c = Canvas(0, W * 100.0, 0, H * 100.0, px_per_m=1.0)   # 1 px per metre-unit: world == pixels * 100
    c.rect(0, W * 100.0, 0, H * 100.0, (24, 22, 20))
    for (x0, w, h, px) in ((0, lw, lh, lpx), (lw + 12, rw, rh, rpx)):
        for y in range(h):
            row = (band + y) * c.w * 3 + x0 * 3
            c.px[row:row + w * 3] = px[y * w * 3:(y + 1) * w * 3]
    c.text(W * 50.0, 1500.0, title, (235, 225, 200), 2, center=True)
    c.text(lw * 50.0, (H - band // 2) * 100.0, lcap, (235, 225, 200), 2, center=True)
    c.text((lw + 12 + rw / 2) * 100.0, (H - band // 2) * 100.0, rcap, (235, 225, 200), 2, center=True)
    c.save(dst)


if __name__ == '__main__':
    main(*sys.argv[1:7]) if len(sys.argv) > 6 else main(*sys.argv[1:6])
