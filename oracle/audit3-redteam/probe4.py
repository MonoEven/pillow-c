from PIL import Image

def p(label, fn):
    try:
        r = fn()
        print(f"{label} => OK {r!r}")
    except Exception as e:
        print(f"{label} => {type(e).__name__}: {str(e)!r}")

rgb = (100, 100, 100)
print("=== matrix conversions (source RGB) ===")
p("RGB->L 4-tuple", lambda: Image.new("RGB",(1,1),rgb).convert("L",(1.0,0.0,0.0,0.0)).getpixel((0,0)))
p("RGB->L 12-tuple", lambda: Image.new("RGB",(1,1),rgb).convert("L",(1.0,0.0,0.0,0.0)*3).getpixel((0,0)))
p("RGB->RGB 4-tuple", lambda: Image.new("RGB",(1,1),rgb).convert("RGB",(1.0,0.0,0.0,0.0)).getpixel((0,0)))
p("RGB->RGB 12-tuple", lambda: Image.new("RGB",(1,1),rgb).convert("RGB",(1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0)).getpixel((0,0)))

print("=== matrix conversions (source L) ===")
p("L->L 4-tuple", lambda: Image.new("L",(1,1),100).convert("L",(2.0,0.0,0.0,0.0)).getpixel((0,0)))
p("L->L 12-tuple", lambda: Image.new("L",(1,1),100).convert("L",(2.0,0.0,0.0,0.0)*3).getpixel((0,0)))
p("L->RGB 4-tuple", lambda: Image.new("L",(1,1),100).convert("RGB",(2.0,0.0,0.0,0.0)).getpixel((0,0)))
p("L->RGB 12-tuple", lambda: Image.new("L",(1,1),100).convert("RGB",(2.0,0.0,0.0,0.0)*3).getpixel((0,0)))

print("=== convert matrix empty / non-numeric ===")
p("convert empty tuple", lambda: Image.new("RGB",(1,1)).convert("RGB",()))
p("convert matrix None-ish", lambda: Image.new("RGB",(1,1)).convert("RGB", None))
