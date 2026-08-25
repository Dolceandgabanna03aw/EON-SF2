"""Fine dump of reference dock: x 500-1000, y 810-872, all classes, step 2."""
from PIL import Image

im = Image.open("/tmp/ref1.png").convert("RGBA")
px = im.load()

def cls(p):
    r,g,b,a = p
    if r>195 and g>185 and b>140: return 'C'
    if g>120 and b>90 and g>r+25 and abs(g-b)<90: return 'M'
    if r>230 and g>120 and g<200 and b<130: return 'O'
    if r>200 and g<110 and b<110: return 'R'
    if r>150 and g>120 and b<110: return 'o'
    if r<70 and g<75 and b<70: return 'K'
    if r>100 and g<150 and b<150 and r>g: return 'r'
    return '.'

for y in range(806, 874, 2):
    row = []
    x = 0
    while x < 1000:
        c = cls(px[x,y])
        x0 = x
        while x < 1000 and cls(px[x,y]) == c: x+=1
        if x-x0 >= 4:
            row.append(f"{c}:{x0}-{x}")
    s = ' '.join(row)
    if 'M' in s or 'C' in s or 'O' in s or 'R' in s:
        print(f"y={y:>3} " + s)
