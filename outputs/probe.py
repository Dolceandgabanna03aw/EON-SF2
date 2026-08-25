"""Probe exact colors at grid points + loose cream scan."""
from PIL import Image
import sys

def loose_cream(p):
    r,g,b,a = p
    return r>170 and g>160 and b>120 and r>g>b and (r-b)>20

def probe(path):
    im = Image.open(path).convert("RGBA")
    W,H = im.size
    px = im.load()
    print(f"=== {path} {W}x{H}")
    # grid probe
    for yf in (0.10, 0.15, 0.20, 0.25, 0.30, 0.35, 0.40, 0.45, 0.50, 0.55, 0.60, 0.70, 0.80, 0.85, 0.90, 0.95):
        y = int(H*yf)
        row = []
        for xf in (0.05, 0.15, 0.25, 0.35, 0.45, 0.55, 0.65, 0.75, 0.85, 0.95):
            x = int(W*xf)
            r,g,b,a = px[x,y]
            row.append(f"({r:3d},{g:3d},{b:3d})")
        print(f"  y={yf:.2f} " + ' '.join(row))
    # count loose-cream per row, bands
    bands = []
    prev = False
    start = 0
    for y in range(H):
        c = 0
        for x in range(0, W, 2):
            if loose_cream(px[x,y]): c+=1
        key = c > 10
        if key != prev:
            if prev: bands.append((start, y-1))
            start = y; prev = key
    if prev: bands.append((start, H-1))
    print("  loose-cream bands: " + ', '.join(f"{a}-{b}({b-a+1})" for a,b in bands if b-a+1>4))

for p in sys.argv[1:]:
    probe(p)

