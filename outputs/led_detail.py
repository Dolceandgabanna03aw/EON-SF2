from PIL import Image
im = Image.open('outputs/eon_ui_offscreen.png').convert('RGB')
def sim(a,b,t=10):
    return abs(a[0]-b[0])<=t and abs(a[1]-b[1])<=t and abs(a[2]-b[2])<=t
# Switch local x672-795, LED band y118-140
for y in range(116, 142, 2):
    line=''
    for x in range(668, 800, 2):
        p=im.getpixel((x,y))
        c='.'
        if sim(p,(0x7f,0xe0,0xc4)): c='m'
        elif sim(p,(0xff,0xb4,0x54)): c='H'
        elif sim(p,(0xff,0x4d,0x3d)): c='R'
        elif sim(p,(0x0e,0x16,0x13)): c='B'
        elif sum(p)>600: c='o'
        line+=c
    print(f'{y:3d} {line}')
