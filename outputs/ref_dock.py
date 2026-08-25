"""Examine the reference dock region: crop, print per-row color runs to understand label/display/button placement."""
from PIL import Image
import sys

im = Image.open("/tmp/ref1.png").convert("RGBA")
W,H = im.size
px = im.load()

def cls(p):
    r,g,b,a = p
    if r>195 and g>185 and b>140: return 'C'
    if g>120 and b>90 and g>r+25: return 'M'
    if r>200 and g>140 and b<120: return 'O'   # orange LEDs
    if r<60 and g<70 and b<60: return 'K'
    if r>200 and g<120 and b<120: return 'R'
    return '.'

print("dock rows (y 800-880), x 0-1000 step 10:")
for y in range(800, 880, 4):
    row = []
    x = 0
    while x < W:
        c = cls(px[x,y])
        x0 = x
        while x < W and cls(px[x,y]) == c: x+=1
        if x-x0 >= 10:
            row.append(f"{c}:{x0}-{x}")
    print(f"y={y:>3} " + ' '.join(row))

