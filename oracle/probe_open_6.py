"""BEHAV-OPEN-006 oracle: MIC (Microsoft Image Composer) open behaviors in Pillow 11.3.0."""
import io
import json
import sys

sys.path.insert(0, "oracle")
from ole_builder import MAGIC, build_cfb
import olefile
from PIL import Image

out = {}


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


def tiff_bytes(mode, size, value):
    im = Image.new(mode, size, value)
    buf = io.BytesIO()
    im.save(buf, "TIFF")
    return buf.getvalue()


def mic_blob(streams):
    return build_cfb(list(streams.items()))


def open_mic(blob):
    im = Image.open(io.BytesIO(blob), formats=["MIC"])
    data = im.tobytes()
    info = {k: (v.hex() if isinstance(v, bytes) else str(v)) for k, v in getattr(im, "info", {}).items()}
    return (
        im.format,
        im.mode,
        list(im.size),
        data.hex(),
        getattr(im, "n_frames", None),
        getattr(im, "is_animated", None),
        info,
    )


tiff_l = tiff_bytes("L", (2, 2), 10)
small_tiff = tiff_bytes("L", (1, 1), 9)
tiff_rgb = tiff_bytes("RGB", (2, 1), 5)

# sanity: our container parses with olefile and the MIC plugin finds it
blob = mic_blob({"picture.ACI/Image": tiff_l})
ole = olefile.OleFileIO(io.BytesIO(blob))
out["self_listdir"] = ole.listdir()
out["self_stream_size"] = len(ole.openstream(["picture.ACI", "Image"]).read())

out["mic_l"] = capture(lambda: open_mic(mic_blob({"picture.ACI/Image": tiff_l})))
out["mic_small_stream"] = capture(lambda: open_mic(mic_blob({"pic.ACI/Image": small_tiff})))
out["mic_two_frames"] = capture(lambda: open_mic(mic_blob({"one.ACI/Image": tiff_l, "two.ACI/Image": tiff_rgb})))
# n_frames quirk: seek(0) runs TiffImageFile._open which resets _n_frames
# to the TIFF IFD count (1), so multi-ACI files report n_frames 1 and
# seek(1) raises the sequence EOFError
def mic_two_frames_meta():
    im = Image.open(io.BytesIO(mic_blob({"one.ACI/Image": tiff_l, "two.ACI/Image": tiff_rgb})), formats=["MIC"])
    try:
        return (im.n_frames, im.is_animated, im.tell())
    finally:
        im.close()
out["mic_two_frames_meta"] = capture(mic_two_frames_meta)
out["mic_seek1"] = capture(lambda: (lambda im: (im.seek(1), im.tell()))(Image.open(io.BytesIO(mic_blob({"one.ACI/Image": tiff_l, "two.ACI/Image": tiff_rgb})), formats=["MIC"])))
# the second frame's TIFF is only parsed when seeked: a broken second
# frame does not affect the first-frame open
out["mic_broken_second_frame"] = capture(lambda: open_mic(mic_blob({"one.ACI/Image": tiff_l, "two.ACI/Image": b"\x01\x02\x03\x04"})))
# a realistic truncation: a valid container cut in half
full = mic_blob({"pic.ACI/Image": tiff_l})
out["mic_truncated_realistic"] = capture(lambda: Image.open(io.BytesIO(full[: len(full) // 2]), formats=["MIC"]))
out["mic_bad_magic"] = capture(lambda: Image.open(io.BytesIO(b"\x00" * 64), formats=["MIC"]))
out["mic_valid_ole_no_image"] = capture(lambda: Image.open(io.BytesIO(mic_blob({"foo.txt/Data": b"x" * 5000})), formats=["MIC"]))
out["mic_lowercase_aci"] = capture(lambda: Image.open(io.BytesIO(mic_blob({"pic.aci/Image": tiff_l})), formats=["MIC"]))
out["mic_no_image_child"] = capture(lambda: Image.open(io.BytesIO(mic_blob({"pic.ACI/Other": b"x" * 5000})), formats=["MIC"]))
out["mic_broken_tiff"] = capture(lambda: Image.open(io.BytesIO(mic_blob({"pic.ACI/Image": b"\x01\x02\x03\x04"})), formats=["MIC"]))
header = bytearray(b"\x00" * 512)
header[0:8] = MAGIC
out["mic_truncated_ole"] = capture(lambda: Image.open(io.BytesIO(bytes(header)), formats=["MIC"]))
out["mic_save"] = capture(lambda: (lambda im: im.save(io.BytesIO(), "MIC"))(Image.new("L", (1, 1))))

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_open_6.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
