"""BEHAV-PALETTE-002 oracle: ImagePalette.load behaviors in Pillow 11.3.0."""
import json
import os
import tempfile

from PIL import ImagePalette

out = {}


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


def write(name, data):
    path = os.path.join(tempfile.gettempdir(), f"palette_load_{name}.gpl")
    with open(path, "wb") as f:
        f.write(data)
    return path


def load_shape(path):
    data, rawmode = ImagePalette.load(path)
    return (len(data), rawmode, data[:12].hex())


gpl = (
    b"GIMP Palette\n"
    b"Name: test\n"
    b"Columns: 4\n"
    b"# comment\n"
    b"  0  10  20  30\n"
    b"1 40 50 60\n"
)
out["gpl"] = capture(lambda: load_shape(write("gpl", gpl)))
# 256 entries exactly
gpl_full = b"GIMP Palette\n" + b"".join(b"%d %d %d %d\n" % (i, i, i, i + 1) for i in range(256))
out["gpl_full"] = capture(lambda: load_shape(write("gplfull", gpl_full)))
# > 256 entries: the parser stops at 768 bytes
gpl_300 = b"GIMP Palette\n" + b"".join(b"%d %d %d %d\n" % (i, i, i, i + 1) for i in range(300))
out["gpl_300"] = capture(lambda: load_shape(write("gpl300", gpl_300)))
# non-numeric entry -> ValueError -> falls through to the next parser
out["gpl_bad_int"] = capture(lambda: load_shape(write("gplbad", b"GIMP Palette\n0 1 x 3\n")))
# long line -> SyntaxError "bad palette file"
out["gpl_long_line"] = capture(lambda: load_shape(write("gpllong", b"GIMP Palette\n" + b"x" * 120 + b"\n")))

teragon = b"# simple\n0 1 2 3\n5 9\n"
out["teragon"] = capture(lambda: load_shape(write("teragon", teragon)))
out["teragon_bad"] = capture(lambda: load_shape(write("teragonbad", b"0 x y z\n")))

ggr = (
    b"GIMP Gradient\n"
    b"Name: g\n"
    b"3\n"
    b"0.0 0.5 1.0 0.0 0.0 0.0 1.0 1.0 0.0 0.0 1.0 0 0\n"
    b"0.5 0.75 1.0 1.0 0.0 0.0 1.0 0.0 1.0 0.0 1.0 0 0\n"
    b"0.75 1.0 1.0 0.0 1.0 0.0 1.0 0.0 0.0 1.0 1.0 1 0\n"
)
out["ggr"] = capture(lambda: load_shape(write("ggr", ggr)))
out["ggr_hsv"] = capture(lambda: load_shape(write("ggrhsv", b"GIMP Gradient\nName: h\n1\n0.0 0.5 1.0 0.0 0.0 0.0 1.0 1.0 0.0 0.0 1.0 0 1\n")))
out["ggr_bad_magic"] = capture(lambda: load_shape(write("ggrbad", b"not a gradient\n")))

out["garbage"] = capture(lambda: load_shape(write("garbage", b"\x01\x02\x03 not a palette at all\n")))
out["missing"] = capture(lambda: ImagePalette.load(os.path.join(tempfile.gettempdir(), "palette_load_missing_xyz.gpl")))

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_palette_load.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
