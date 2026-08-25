"""Find mintDeep (LOAD button / tabs) and displayBg (LED digit boxes) bounding regions."""
from PIL import Image
import sys

def near(p, target, tol=40):
    return all(abs(p[i]-target[i])<=tol for i in range(3))

def find(path, targets):
    im = Image.open(path).convert("RGBA")
    W,H = im.size
    px = im.load()
    print(f"=== {path} {W}x{H}")
    for name, tgt in targets:
        xs, ys = [], []
        for y in range(0, H, 2):
            for x in range(0, W, 2):
                if near(px[x,y][:3], tgt):
                    xs.append(x); ys.append(y)
        if xs:
            print(f"  {name}: x {min(xs)}-{max(xs)}, y {min(ys)}-{max(ys)}, n={len(xs)}")
        else:
            print(f"  {name}: none")

targets = [
    ("mintDeep(LOAD/tab)", (52,169,132)),
    ("mintText(7fe0c4)", (127,224,196)),
    ("displayBg(0e1613)", (14,22,19)),
    ("hotOrange(FF9452)", (255,148,82)),
    ("creamKnob(fff)", (255,255,255)),
]
for p in sys.argv[1:]:
    find(p, targets)

