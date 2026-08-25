from PIL import Image
im = Image.open('/tmp/dbg.png').convert('RGB')
def sim(a,b,t=10):
    return abs(a[0]-b[0])<=t and abs(a[1]-b[1])<=t and abs(a[2]-b[2])<=t
def region(name,c):
    pts=[(xx,yy) for yy in range(0,820) for xx in range(0,820,2) if sim(im.getpixel((xx,yy)),c)]
    if not pts: print(name,'NONE'); return
    xs=[p[0] for p in pts]; ys=[p[1] for p in pts]
    print(f"{name}: n={len(pts)} x{min(xs)}-{max(xs)} y{min(ys)}-{max(ys)}")
region('ledHot',(0xff,0xb4,0x54))
region('ledRed',(0xff,0x4d,0x3d))
region('ledMint',(0x7f,0xe0,0xc4))
