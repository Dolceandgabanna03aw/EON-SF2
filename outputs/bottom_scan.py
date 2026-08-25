"""Examine bottom region of 22.56 screenshot for the dock: scan rows 640-830 for mintDeep runs and displayBg boxes."""
from PIL import Image
import sys

def near(p, target, tol=40):
    return all(abs(p[i]-target[i])<=tol for i in range(3))

im = Image.open(sys.argv[1]).convert("RGBA")
W,H = im.size
px = im.load()
print(f"{W}x{H}")
MINTDEEP=(52,169,132); DISPBG=(14,22,19); MINT=(127,224,196)
for y in range(600, H, 2):
    md = xs = []
    dg = []
    mt = []
    for x in range(0, W):
        p = px[x,y][:3]
        if near(p, MINTDEEP, 45): xs.append(x)
        elif near(p, DISPBG, 30): dg.append(x)
        elif near(p, MINT, 45): mt.append(x)
    def runs(arr):
        if not arr: return []
        out=[]; s=arr[0]; pv=arr[0]
        for v in arr[1:]:
            if v-pv>3: out.append((s,pv)); s=v
            pv=v
        out.append((s,pv))
        return [(a,b) for a,b in out if b-a>8]
    if runs(xs) or runs(dg) or len(mt)>20:
        print(f"y={y}: mintDeep={runs(xs)} dispbg={runs(dg)} mintN={len(mt)}")

