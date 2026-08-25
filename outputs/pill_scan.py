from PIL import Image
im = Image.open('/tmp/dbg.png').convert('RGB')
def sim(a,b,t=8):
    return abs(a[0]-b[0])<=t and abs(a[1]-b[1])<=t and abs(a[2]-b[2])<=t

# Print rows in bus area showing where displayBg pill columns are
print("displayBg pill columns per row (bus area x400-800):")
for y in range(60, 320, 2):
    xs=[x for x in range(400,800) if sim(im.getpixel((x,y)),(0x0e,0x16,0x13))]
    if xs:
        print(f"y={y:3d} x{min(xs)}-{max(xs)} n={len(xs)}")

print("led colors:")
for name,c in [('ledHot',(0xff,0xb4,0x54)),('ledRed',(0xff,0x4d,0x3d)),('ledMint',(0x7f,0xe0,0xc4))]:
    n=sum(1 for yy in range(0,820,2) for xx in range(0,820,2) if sim(im.getpixel((xx,yy)),c,10))
    print(name,n)
