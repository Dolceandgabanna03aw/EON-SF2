from PIL import Image

im = Image.open('outputs/eon_ui_offscreen.png').convert('RGB')

def sim(a, b, t=12):
    return abs(a[0]-b[0]) <= t and abs(a[1]-b[1]) <= t and abs(a[2]-b[2]) <= t

def classify(p):
    if sim(p, (0x0e,0x16,0x13)): return 'B'   # displayBg pill
    if sim(p, (0x7f,0xe0,0xc4)): return 'm'   # mint/ledMint
    if sim(p, (0xff,0xb4,0x54)): return 'H'   # ledHot
    if sim(p, (0xff,0x4d,0x3d)): return 'R'   # ledRed
    if sim(p, (0x17,0x19,0x16)): return '.'   # body2 background
    return 'o'                                 # other

x0, y0, x1, y1 = 655, 84, 806, 214
step = 4
for y in range(y1, y0, -step):
    line = ''
    for x in range(x0, x1, step):
        line += classify(im.getpixel((x, y)))
    print(f'{y:3d} {line}')

print()
from collections import Counter
c = Counter()
for y in range(y0, y1):
    for x in range(x0, x1):
        c[classify(im.getpixel((x, y)))] += 1
print(dict(c))
