from PIL import Image
im = Image.open('outputs/eon_ui_offscreen.png').convert('RGB')
for x in range(660, 780, 10):
    p = im.getpixel((x, 131))
    print(x, '#%02x%02x%02x' % p)
