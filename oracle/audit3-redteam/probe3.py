from PIL import Image

def p(label, fn):
    try:
        r = fn()
        print(f"{label} => OK {r!r}")
    except Exception as e:
        print(f"{label} => {type(e).__name__}: {str(e)!r}")

print("=== P mode explicit BICUBIC vs NEAREST (clean) ===")
pm = Image.new("P",(4,1))
for x in range(4): pm.putpixel((x,0), x)
pal = [0]*768
for i in range(4):
    pal[i*3]=i*60; pal[i*3+1]=i*60; pal[i*3+2]=i*60
pm.putpalette(pal)
print("P nearest:", list(pm.resize((2,1), Image.Resampling.NEAREST).getdata()))
print("P bicubic:", list(pm.resize((2,1), Image.Resampling.BICUBIC).getdata()))
print("P default:", list(pm.resize((2,1)).getdata()))

print("=== 1 mode explicit BICUBIC vs NEAREST ===")
b1 = Image.new("1",(4,1))
for x in range(4): b1.putpixel((x,0), 255 if x%2 else 0)
print("1 nearest:", list(b1.resize((2,1), Image.Resampling.NEAREST).getdata()))
print("1 bicubic:", list(b1.resize((2,1), Image.Resampling.BICUBIC).getdata()))

print("=== LAB->I;16B ===")
p("LAB->I;16B", lambda: Image.new("LAB",(1,1),(50,0,0)).convert("I;16B").mode)

print("=== convert L matrix len4 (correct) ===")
p("convert L matrix len4", lambda: Image.new("L",(1,1),100).convert("L",(2,0,0,0)).getpixel((0,0)))
p("convert L matrix len5", lambda: Image.new("L",(1,1),100).convert("L",(2,0,0,0,0)))

print("=== paste scalar on RGB result ===")
im = Image.new("RGB",(2,2)); im.paste(5,(0,0,1,1)); print("paste 5 RGB:", im.getpixel((0,0)))
im2 = Image.new("RGB",(2,2)); im2.paste((1,2,3),(0,0,1,1)); print("paste (1,2,3) RGB:", im2.getpixel((0,0)))

print("=== paste scalar on L result ===")
im3 = Image.new("L",(2,2)); im3.paste(7,(0,0,1,1)); print("paste 7 L:", im3.getpixel((0,0)))

print("=== getdata band on I ===")
p("getdata I band", lambda: list(Image.new("I",(2,2),5).getdata(0)))
p("getdata band non-int", lambda: Image.new("RGB",(2,2)).getdata(1.5))

print("=== point lut 768 on RGB ===")
p("point 768 lut RGB", lambda: Image.new("RGB",(1,1),(1,2,3)).point([255]*768).getpixel((0,0)))
p("point 256 lut RGB", lambda: Image.new("RGB",(1,1),(1,2,3)).point([255]*256).getpixel((0,0)))

print("=== resize unknown resample vs box ===")
p("resize box bad", lambda: Image.new("L",(4,4)).resize((2,2), box=(1,1)))
p("resize box 3-tuple", lambda: Image.new("L",(4,4)).resize((2,2), box=(0,0,4)))

print("=== crop box len3 ===")
p("crop 3-tuple", lambda: Image.new("L",(4,4)).crop((0,0,4)))
p("crop 2-tuple", lambda: Image.new("L",(4,4)).crop((0,0)))

print("=== split mode 1 ===")
p("split 1", lambda: [x.mode for x in Image.new("1",(2,2)).split()])
p("split LA", lambda: [x.mode for x in Image.new("LA",(2,2)).split()])

print("=== getchannel L ===")
p("getchannel L R", lambda: Image.new("L",(2,2)).getchannel("L").mode)
p("getchannel 0 L", lambda: Image.new("L",(2,2)).getchannel(0).mode)
