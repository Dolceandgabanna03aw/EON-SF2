from PIL import Image
im = Image.open('outputs/eon_ui_offscreen.png').convert('RGB')
def sim(a,b,t=10):
    return abs(a[0]-b[0])<=t and abs(a[1]-b[1])<=t and abs(a[2]-b[2])<=t
colors={'mint':(0x7f,0xe0,0xc4),'hot':(0xff,0xb4,0x54),'red':(0xff,0x4d,0x3d),'bg':(0x0e,0x16,0x13)}
# count per column x in LED band y118-136, to see exact bar extents
for x in range(668, 800, 2):
    counts={k:0 for k in colors}
    for y in range(118, 137):
        p=im.getpixel((x,y))
        for k,c in colors.items():
            if sim(p,c): counts[k]+=1
    top=sorted(counts.items(), key=lambda kv:-kv[1])[0]
    tag=top[0] if top[1]>=12 else ('none' if top[1]==0 else 'mix:'+top[0])
    if tag not in ('none','bg'):
        print(f'x={x:3d} {tag} {counts}')
