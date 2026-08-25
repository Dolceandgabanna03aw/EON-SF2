"""Vertical band scan: per-row cream/mint pixel counts, compressed into bands."""
from PIL import Image
import sys

def is_cream(p):
    r,g,b,a = p
    return r>195 and g>185 and b>140 and (r-b)>25 and (r-g)>5

def is_mint(p):
    r,g,b,a = p
    return g>120 and b>90 and g>r+25 and abs(g-b)<80 and r<160

def scan(path):
    im = Image.open(path).convert("RGBA")
    W,H = im.size
    px = im.load()
    print(f"=== {path} {W}x{H}")
    bands = []
    prev = (0,0)
    start = 0
    for y in range(H):
        c = m = 0
        for x in range(0, W, 2):
            p = px[x,y]
            if is_cream(p): c+=1
            elif is_mint(p): m+=1
        key = (c>15, m>15)
        if key != prev:
            if prev: bands.append((start, y-1, prev))
            start = y
            prev = key
    bands.append((start, H-1, prev))
    for (y0,y1,(c,m)) in bands:
        if y1-y0 < 2: continue
        tag = ("CREAM" if c else "") + (" MINT" if m else "") or "dark"
        print(f"  y {y0:>4}-{y1:>4} ({y1-y0+1:>3}) {tag}")

for p in sys.argv[1:]:
    scan(p)

