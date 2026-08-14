"""BEHAV-OPEN-009 oracle: WMF/EMF open behaviors in Pillow 11.3.0."""
import hashlib
import io
import json
import struct

from PIL import Image

out = {}


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


def metarecord(func, params):
    # record = rdSize (DWORD, in words) + rdFunction (WORD) + params (words)
    size_words = 3 + len(params)
    return struct.pack("<IH", size_words, func) + struct.pack("<%dH" % len(params), *params)


def eof_record():
    return struct.pack("<IHH", 3, 0, 0)


def placeable_wmf(bbox=(0, 0, 100, 100), inch=96, records=b"", sanity=b"\x01\x00\x09\x00"):
    x0, y0, x1, y1 = bbox
    all_records = records + eof_record()
    metaheader = sanity + struct.pack("<H", 0x0300) + struct.pack("<I", 0) + struct.pack("<H", 0) + struct.pack("<I", 0) + struct.pack("<H", 0)
    body = metaheader + all_records
    # patch mtSize (total DWORDs) and mtMaxRecord
    total_dwords = len(body) // 2
    max_record = 3
    pos = 0
    while pos + 4 <= len(all_records):
        size_words = struct.unpack("<I", all_records[pos:pos + 4])[0]
        max_record = max(max_record, size_words)
        pos += size_words * 2
    body = body[:6] + struct.pack("<I", total_dwords) + struct.pack("<H", 0) + struct.pack("<I", max_record) + struct.pack("<H", 0) + body[18:]
    # placeable header with the standard checksum
    head = struct.pack("<I", 0x9AC6CDD7) + struct.pack("<H", 0) + struct.pack("<4h", x0, y0, x1, y1) + struct.pack("<H", inch) + b"\x00" * 4
    checksum = 0
    for i in range(10):
        checksum ^= struct.unpack("<H", head[i * 2:(i + 1) * 2])[0]
    head += struct.pack("<H", checksum)
    return head + body


def open_meta(blob, fmt="WMF"):
    im = Image.open(io.BytesIO(blob), formats=[fmt])
    return (im.format, im.mode, list(im.size), im.info.get("dpi"), im.info.get("wmf_bbox"))


def open_load(blob, fmt="WMF"):
    im = Image.open(io.BytesIO(blob), formats=[fmt])
    try:
        im.load()
        data = im.tobytes()
        return (
            list(im.size),
            hashlib.md5(data).hexdigest(),
            data[:48].hex(),
            sum(1 for b in data if b != 255),
        )
    finally:
        im.close()


# records: map mode anisotropic, window (0,0)-(100,100), rectangle border
records = b""
records += metarecord(0x0103, [8])  # SetMapMode MM_ANISOTROPIC
records += metarecord(0x020B, [0, 0])  # SetWindowOrg (y, x)
records += metarecord(0x020C, [100, 100])  # SetWindowExt (y, x)
records += metarecord(0x041B, [100, 100, 0, 0])  # Rectangle bottom,right,top,left

out["wmf_open"] = capture(lambda: open_meta(placeable_wmf(records=records)))
out["wmf_load"] = capture(lambda: open_load(placeable_wmf(records=records)))
out["wmf_load_empty"] = capture(lambda: open_load(placeable_wmf()))
out["wmf_inch_zero"] = capture(lambda: Image.open(io.BytesIO(placeable_wmf(inch=0)), formats=["WMF"]))
out["wmf_bad_sanity"] = capture(lambda: Image.open(io.BytesIO(placeable_wmf(sanity=b"\x00\x00\x00\x00")), formats=["WMF"]))
out["wmf_bad_magic"] = capture(lambda: Image.open(io.BytesIO(b"\x00" * 64), formats=["WMF"]))
out["wmf_short"] = capture(lambda: Image.open(io.BytesIO(placeable_wmf()[:20]), formats=["WMF"]))
out["wmf_save"] = capture(lambda: (lambda im: im.save(io.BytesIO(), "WMF"))(Image.new("L", (1, 1))))
out["wmf_description"] = Image.open(io.BytesIO(placeable_wmf()), formats=["WMF"]).format_description

# EMF: minimal enhanced metafile header (40 bytes) + " EMF" magic + EMRHEADER
def minimal_emf():
    # EMRHEADER record: type 1, size 88, bounds (0,0,100,100), frame
    # (0,0,100,100) in 0.01mm, then the EMF file header fields
    bounds = struct.pack("<4i", 0, 0, 100, 100)
    frame = struct.pack("<4i", 0, 0, 100, 100)
    emr = struct.pack("<II", 1, 88) + bounds + frame + b"\x20\x45\x4d\x46" + struct.pack("<I", 108) + b"\x00" * 44
    eof = struct.pack("<II", 14, 20) + struct.pack("<III", 0, 0, 0)
    return emr + eof + b"\x00" * (108 - len(emr) - len(eof) + 8)


out["emf_open"] = capture(lambda: open_meta(minimal_emf(), "WMF"))
out["emf_load"] = capture(lambda: open_load(minimal_emf(), "WMF"))

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_open_9.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
