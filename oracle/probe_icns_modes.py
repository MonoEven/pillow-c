import io
from PIL import Image

for mode in ["L", "LA", "RGB", "RGBA", "P", "1", "I;16"]:
    try:
        im = Image.new(mode, (20, 10))
        if mode == "1":
            im.putdata([(x + y) % 2 for y in range(10) for x in range(20)])
        if mode == "I;16":
            im.putdata([(x * 1000 + y * 3000) % 65536 for y in range(10) for x in range(20)])
        buf = io.BytesIO()
        im.save(buf, format="ICNS")
        r = Image.open(io.BytesIO(buf.getvalue()))
        b = r.tobytes()
        print(mode, "OK", r.mode, r.size, len(b))
    except Exception as e:
        print(mode, "FAIL", type(e).__name__, e)
