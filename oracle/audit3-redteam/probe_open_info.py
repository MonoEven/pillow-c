import io, struct, zlib, os
from PIL import Image, ImageDraw
import PIL

out = r"C:\Users\78442\AppData\Local\Temp\pillow-audit"
os.makedirs(out, exist_ok=True)

def dump(fmt, path):
    try:
        im = Image.open(path)
        im.load()
        info = {k: (repr(v)[:120]) for k, v in im.info.items()}
        print(f"=== {fmt} === mode={im.mode} format={im.format} size={im.size}")
        for k in sorted(info):
            print(f"    info[{k!r}] = {info[k]}")
        return im
    except Exception as e:
        print(f"=== {fmt} === ERROR {type(e).__name__}: {e}")

# --- PNG with full metadata ---
p = os.path.join(out, "meta.png")
im = Image.new("RGB", (4,4), (1,2,3))
from PIL import PngImagePlugin
pnginfo = PngImagePlugin.PngInfo()
pnginfo.add_text("Comment", "hi there")
pnginfo.add_text("Author", "auditor", zip=True)
im.save(p, "PNG", pnginfo=pnginfo, dpi=(100, 200), icc_profile=b"fakeiccp", exif=b"Exif\x00\x00fake")
dump("PNG metadata", p)

# PNG with gamma/srgb (via pnginfo chunks)
p2 = os.path.join(out, "meta2.png")
pnginfo2 = PngImagePlugin.PngInfo()
pnginfo2.add(b"gAMA", struct.pack(">I", 45455))
pnginfo2.add(b"sRGB", b"\x00")
pnginfo2.add(b"cHRM", struct.pack(">8I", 31270, 32900, 64000, 33000, 30000, 60000, 15000, 6000))
im2 = Image.new("RGB", (2,2), (5,6,7))
im2.save(p2, "PNG", pnginfo=pnginfo2)
dump("PNG gamma/srgb/chrm", p2)

# --- JPEG ---
p3 = os.path.join(out, "meta.jpg")
im3 = Image.new("RGB", (8,8), (10,20,30))
im3.save(p3, "JPEG", quality=80, dpi=(72,72), exif=b"Exif\x00\x00fake", icc_profile=b"fakeicc", comment=b"mycomment", progressive=True, subsampling=1)
dump("JPEG metadata", p3)

# --- GIF ---
p4 = os.path.join(out, "meta.gif")
im4 = Image.new("P", (8,8), 1)
im4.save(p4, "GIF", transparency=0, duration=500, loop=3, comment=b"gifcomment", background=1, disposal=2)
dump("GIF metadata", p4)

# --- TIFF ---
p5 = os.path.join(out, "meta.tiff")
im5 = Image.new("RGB", (8,8), (10,20,30))
im5.save(p5, "TIFF", dpi=(300,300), icc_profile=b"fakeicc", compression="tiff_lzw",
         description="desc", software="soft", artist="art", copyright="copy",
         resolution=300.0, resolution_unit=2)
dump("TIFF metadata", p5)

# --- BMP ---
p6 = os.path.join(out, "meta.bmp")
im6 = Image.new("RGB", (8,8), (10,20,30))
im6.save(p6, "BMP", dpi=(96,96))
dump("BMP metadata", p6)

# --- ICO ---
p7 = os.path.join(out, "meta.ico")
im7 = Image.new("RGBA", (16,16), (1,2,3,4))
im7.save(p7, "ICO", sizes=[(16,16),(32,32)])
dump("ICO metadata", p7)

# --- TGA ---
p8 = os.path.join(out, "meta.tga")
im8 = Image.new("RGB", (4,4), (1,2,3))
im8.save(p8, "TGA", rle=True)
dump("TGA metadata", p8)

# --- PNG tRNS transparency ---
p9 = os.path.join(out, "trns.png")
im9 = Image.new("RGBA", (2,2), (1,2,3,4))
im9.save(p9, "PNG", transparency=(1,2,3))
dump("PNG rgb transparency", p9)

# --- JPEG quantization/ adobe info keys ---
p10 = os.path.join(out, "meta2.jpg")
im10 = Image.new("CMYK", (8,8), (10,20,30,40))
im10.save(p10, "JPEG", quality=90)
dump("JPEG CMYK metadata", p10)

# --- GIF version / extension / disposal keys ---
p11 = os.path.join(out, "meta2.gif")
im11 = Image.new("P", (8,8), 1)
frames = [im11, Image.new("P", (8,8), 2)]
frames[0].save(p11, "GIF", save_all=True, append_images=frames[1:], duration=[100,200], loop=0, disposal=[1,2])
dump("GIF anim metadata", p11)

print("---- done ----")
