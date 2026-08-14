"""Extract Pillow 11.3.0 RGBA<->RGBa conversion formulas exactly."""
from PIL import Image

# Premultiply: RGBA -> RGBa
print("== premultiply RGBA->RGBa: out[c] for (c, a) ==")
for c, a in [(100, 128), (255, 128), (255, 100), (50, 128), (1, 2), (0, 0), (5, 0), (200, 255), (128, 255), (127, 255), (3, 7)]:
    im = Image.frombytes("RGBA", (1, 1), bytes([c, 0, 0, a]))
    out = im.convert("RGBa")
    print(f"c={c} a={a} -> {list(out.tobytes())}")

# Unpremultiply: RGBa -> RGBA
print("== unpremultiply RGBa->RGBA: out[c] for (p, a) ==")
for p, a in [(49, 128), (99, 128), (5, 0), (0, 0), (100, 0), (255, 255), (200, 200), (1, 0), (7, 3)]:
    im = Image.frombytes("RGBa", (1, 1), bytes([p, 0, 0, a]))
    out = im.convert("RGBA")
    print(f"p={p} a={a} -> {list(out.tobytes())}")
