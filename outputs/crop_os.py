from PIL import Image
im = Image.open('outputs/eon_ui_offscreen.png').convert('RGB')
crop = im.crop((655, 84, 806, 214))
crop = crop.resize((crop.width*3, crop.height*3), Image.NEAREST)
crop.save('/tmp/os_led.png')
print('saved', crop.size)
