"""Layout region analysis: find cream panels, mint strips, dark chassis in a UI screenshot."""
from PIL import Image
import sys

def is_cream(p):
    r,g,b,a = p
    return r>195 and g>185 and b>140 and (r-b)>25 and (r-g)>5

def analyze(path):
    im = Image.open(path).convert("RGBA")
    W,H = im.size
    px = im.load()
    print(f"=== {path} {W}x{H}")
    rects = []
    for y in range(H):
        x = 0
        while x < W:
            if is_cream(px[x,y]):
                x0 = x
                while x < W and is_cream(px[x,y]): x+=1
                rects.append((x0,y,x-1,y))
            else:
                x+=1
    rects.sort(key=lambda r:(r[1],r[0]))
    boxes = []
    for r in rects:
        placed = False
        for b in boxes:
            if r[1] <= b[3]+3 and r[3] >= b[1]-3 and r[0] <= b[2]+3 and r[2] >= b[0]-3:
                b[0]=min(b[0],r[0]); b[1]=min(b[1],r[1]); b[2]=max(b[2],r[2]); b[3]=max(b[3],r[3])
                placed = True; break
        if not placed:
            boxes.append(list(r))
    sig = [b for b in boxes if (b[2]-b[0])>80 and (b[3]-b[1])>40]
    for b in sorted(sig, key=lambda b:(b[1],b[0])):
        print(f"  cream  x:{b[0]:>4}-{b[2]:>4}  y:{b[1]:>4}-{b[3]:>4}  ({b[2]-b[0]+1}x{b[3]-b[1]+1})")

    for frac in (0.5, 0.85, 0.9):
        y = int(H*frac)
        seg = []
        for x in range(0, W, 4):
            r,g,b,a = px[x,y]
            seg.append('C' if is_cream((r,g,b,a)) else ('K' if (r<70 and g<70 and b<70) else '.'))
        line = ''.join(seg)
        out, i = [], 0
        while i < len(line):
            j = i
            while j < len(line) and line[j]==line[i]: j+=1
            out.append(f"{line[i]}:{i*4}-{j*4}")
            i = j
        print(f"  y={y:>4}: " + ' '.join(out)[:600])

for p in sys.argv[1:]:
    analyze(p)

