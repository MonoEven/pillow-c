"""BEHAV-OPEN-001 oracle: PIXAR / XVTHUMB / DCX / HDF5 / BUFR / GRIB and
the .imt non-registration in Pillow 11.3.0."""
import io
import json
import struct

from PIL import Image


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


out = {}

# --- PIXAR: 4-byte magic + 508-byte header, raw dump at 1024 ---
def make_pixar(w, h, data):
    buf = bytearray(1024 + len(data))
    buf[0:4] = b"\x80\xe8\x00\x00"
    struct.pack_into("<H", buf, 416, h)
    struct.pack_into("<H", buf, 418, w)
    struct.pack_into("<H", buf, 424, 14)  # mode = (14, 2) -> RGB
    struct.pack_into("<H", buf, 426, 2)
    buf[1024:] = data
    return bytes(buf)


rgb_data = bytes([(x * 7 + y) % 256 for y in range(2) for x in range(3) for _ in range(3)])
for name, blob in [("pixar_ok", make_pixar(3, 2, rgb_data))]:
    r = capture(lambda blob=blob: (lambda im: (im.mode, list(im.size), im.tobytes().hex()))(Image.open(io.BytesIO(blob), formats=["PIXAR"])))
    out[name] = r
# truncated dump (missing bytes -> raw decode error at load)
r = capture(lambda: (lambda im: im.tobytes())(Image.open(io.BytesIO(make_pixar(3, 2, rgb_data[:6])), formats=["PIXAR"])))
out["pixar_truncated"] = r
# bad magic
r = capture(lambda: Image.open(io.BytesIO(b"\x00" * 1100), formats=["PIXAR"]))
out["pixar_bad_magic"] = r
# bad mode (not 14,2) -> no mode assigned
bad_mode = make_pixar(3, 2, rgb_data)
bad_mode = bad_mode[:424] + struct.pack("<H", 1) + bad_mode[426:]
r = capture(lambda bad_mode=bad_mode: (lambda im: (im.mode, im.size))(Image.open(io.BytesIO(bad_mode), formats=["PIXAR"])))
out["pixar_bad_mode"] = r

# --- XVTHUMB: "P7 332" + comments + W H + indices, RGB332 palette ---
def make_xvthumb(w, h, indices, comments=()):
    lines = ["P7 332"]
    lines += list(comments)
    lines.append(f"{w} {h}")
    return ("\n".join(lines) + "\n").encode() + indices


xv = capture(lambda: (lambda im: (im.mode, list(im.size), im.getpalette()[:24], im.tobytes().hex()))(Image.open(io.BytesIO(make_xvthumb(3, 2, bytes(range(6)), ("# hi",))), formats=["XVTHUMB"])))
out["xvthumb_ok"] = xv
r = capture(lambda: Image.open(io.BytesIO(b"P7 332"), formats=["XVTHUMB"]))
out["xvthumb_eof"] = r
r = capture(lambda: Image.open(io.BytesIO(b"P7 331\n1 1\n\x00"), formats=["XVTHUMB"]))
out["xvthumb_bad_magic"] = r
r = capture(lambda: Image.open(io.BytesIO(b"P7 332\n"), formats=["XVTHUMB"]))
out["xvthumb_no_header"] = r
r = capture(lambda: (lambda im: im.tobytes())(Image.open(io.BytesIO(make_xvthumb(3, 2, b"\x00\x01")), formats=["XVTHUMB"])))
out["xvthumb_truncated"] = r
r = capture(lambda: Image.open(io.BytesIO(b"P7 332\n# c\n# d\n"), formats=["XVTHUMB"]))
out["xvthumb_comments_then_eof"] = r
# .xvthumb extension: unregistered?
out["ext_xvthumb"] = [k for k, v in Image.registered_extensions().items() if v == "XVTHUMB"]
out["ext_imt"] = [k for k, v in Image.registered_extensions().items() if v in ("IMT",)]
out["ext_im"] = [k for k, v in Image.registered_extensions().items() if v == "IM"]

# --- DCX: PCX container ---
def save_pcx(im):
    buf = io.BytesIO()
    im.save(buf, format="PCX")
    return buf.getvalue()


im1 = Image.new("1", (4, 3))
im1.putdata([0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0])
pcx1 = save_pcx(im1)
im2 = Image.new("L", (4, 3))
im2.putdata([(x + y) % 256 for y in range(3) for x in range(4)])
pcx2 = save_pcx(im2)


def make_dcx(frames):
    header = struct.pack("<I", 0x3ADE68B1)
    offsets = []
    pos = 4 + 1024 * 4
    for frame in frames:
        offsets.append(pos)
        pos += len(frame)
    directory = b"".join(struct.pack("<I", off) for off in offsets) + b"\x00" * (1024 - len(offsets)) * 4
    return header + directory + b"".join(frames)


dcx2 = make_dcx([pcx1, pcx2])
r = capture(lambda dcx2=dcx2: (lambda im: (im.format, im.mode, list(im.size), getattr(im, "n_frames", None), getattr(im, "is_animated", None), im.tobytes().hex(), (im.seek(1), im.mode, list(im.size), im.tobytes().hex())))(Image.open(io.BytesIO(dcx2), formats=["DCX"])))
out["dcx_two_frames"] = r
dcx1 = make_dcx([pcx1])
r = capture(lambda dcx1=dcx1: (lambda im: (im.format, im.mode, getattr(im, "n_frames", None), getattr(im, "is_animated", None), im.tobytes().hex()))(Image.open(io.BytesIO(dcx1), formats=["DCX"])))
out["dcx_one_frame"] = r
r = capture(lambda: Image.open(io.BytesIO(b"\x00" * 16), formats=["DCX"]))
out["dcx_bad_magic"] = r
# valid magic but no offsets -> empty directory
r = capture(lambda: Image.open(io.BytesIO(struct.pack("<I", 0x3ADE68B1) + b"\x00" * 4096), formats=["DCX"]))
out["dcx_empty_dir"] = r
# bad inner PCX magic
r = capture(lambda: Image.open(io.BytesIO(struct.pack("<I", 0x3ADE68B1) + struct.pack("<I", 4100) + b"\x00" * (4096 - 4) + b"\x00" * 32), formats=["DCX"]))
out["dcx_bad_inner"] = r
# truncated inner PCX payload
bad_pcx = pcx1[:40]
r = capture(lambda: (lambda im: im.tobytes())(Image.open(io.BytesIO(struct.pack("<I", 0x3ADE68B1) + struct.pack("<I", 4100) + b"\x00" * (4096 - 4) + bad_pcx), formats=["DCX"])))
out["dcx_truncated_inner"] = r

# --- HDF5 / BUFR / GRIB stubs ---
hdf5 = b"\x89HDF\r\n\x1a\n" + b"\x00" * 64
r = capture(lambda hdf5=hdf5: (lambda im: (im.format, im.mode, list(im.size)))(Image.open(io.BytesIO(hdf5), formats=["HDF5"])))
out["hdf5_open"] = r
r = capture(lambda hdf5=hdf5: (lambda im: im.load())(Image.open(io.BytesIO(hdf5), formats=["HDF5"])))
out["hdf5_load"] = r
r = capture(lambda: Image.new("L", (2, 2)).save(io.BytesIO(), format="HDF5"))
out["hdf5_save"] = r
r = capture(lambda: Image.open(io.BytesIO(b"\x00" * 64), formats=["HDF5"]))
out["hdf5_bad_magic"] = r
bufr = b"BUFR" + b"\x00" * 60
r = capture(lambda bufr=bufr: (lambda im: (im.format, im.mode, list(im.size)))(Image.open(io.BytesIO(bufr), formats=["BUFR"])))
out["bufr_open"] = r
r = capture(lambda bufr=bufr: (lambda im: im.load())(Image.open(io.BytesIO(bufr), formats=["BUFR"])))
out["bufr_load"] = r
r = capture(lambda: Image.new("L", (2, 2)).save(io.BytesIO(), format="BUFR"))
out["bufr_save"] = r
r = capture(lambda: Image.open(io.BytesIO(b"ZCZC" + b"\x00" * 60), formats=["BUFR"]))
out["bufr_zczc_open"] = r
r = capture(lambda: Image.open(io.BytesIO(b"XXXX" + b"\x00" * 60), formats=["BUFR"]))
out["bufr_bad_magic"] = r
grib = b"GRIB" + b"\x00\x00\x00\x01" + b"\x00" * 56
r = capture(lambda grib=grib: (lambda im: (im.format, im.mode, list(im.size)))(Image.open(io.BytesIO(grib), formats=["GRIB"])))
out["grib_open"] = r
r = capture(lambda grib=grib: (lambda im: im.load())(Image.open(io.BytesIO(grib), formats=["GRIB"])))
out["grib_load"] = r
r = capture(lambda: Image.new("L", (2, 2)).save(io.BytesIO(), format="GRIB"))
out["grib_save"] = r
r = capture(lambda: Image.open(io.BytesIO(b"GRIB" + b"\x00\x00\x00\x02" + b"\x00" * 56), formats=["GRIB"]))
out["grib_bad_edition"] = r

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_open_simple.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
