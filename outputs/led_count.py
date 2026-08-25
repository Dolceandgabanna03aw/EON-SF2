from PIL import Image
im = Image.open('outputs/eon_ui_offscreen.png').convert('RGB')
def sim(a,b,t=10):
    return abs(a[0]-b[0])<=t and abs(a[1]-b[1])<=t and abs(a[2]-b[2])<=t
colors=[('mint',(0x7f,0xe0,0xc4)),('hot',(0xff,0xb4,0x54)),('red',(0xff,0x4d,0x3d))]
y=131
bands=[]
cur=None
for x in range(660, 800):
    p=im.getpixel((x,y))
    tag=None
    for n,c in colors:
        if sim(p,c): tag=n; break
    if tag!=cur:
        if cur is not None and bands and bands[-1][0]==cur:
            bands[-1][1]=x
        elif cur is not None:
            (bands.append([cur,x]) if bands and bands[-1][0]==cur else None)
        cur=tag
# simpler
groups=[]
last=None
for x in range(660,800):
    p=im.getpixel((x,y))
    tag=None
    for n,c in colors:
        if sim(p,c): tag=n; break
    if tag!=last:
        groups.append([tag,x,x])
        last=tag
    else:
        if groups: groups[-1][2]=x
for g in [g for g in groups if g[0]]:
    print(g[0], 'x'+str(g[1])+'-'+str(g[2]), 'w', g[2]-g[1]+1)
