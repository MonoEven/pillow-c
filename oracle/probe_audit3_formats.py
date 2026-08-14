"""AUDIT-003: pin local behaviors for the codec/dependency formats."""

import io
import os
import struct
import sys

from PIL import Image

# 1. codec round-trips (work locally?)
for fmt in ["WEBP", "JPEG2000", "AVIF"]:
    im = Image.new("RGB", (4, 3))
    im.putdata([(10, 20, 30)] * 12)
    buf = io.BytesIO()
    try:
        im.save(buf, fmt)
        data = buf.getvalue()
        ro = Image.open(io.BytesIO(data))
        ro.load()
        print(fmt, "OK len", len(data), ro.mode, ro.size, ro.getpixel((0, 0)))
    except Exception as e:
        print(fmt, "ERR", type(e).__name__, str(e)[:80])

# 2. ICNS save + open
im = Image.new("RGBA", (16, 16))
im.putdata([(1, 2, 3, 255)] * 256)
buf = io.BytesIO()
try:
    im.save(buf, "ICNS")
    data = buf.getvalue()
    print("ICNS save OK len", len(data), data[:8].hex())
    ro = Image.open(io.BytesIO(data))
    ro.load()
    print("ICNS open:", ro.mode, ro.size, ro.getpixel((0, 0)))
except Exception as e:
    print("ICNS ERR", type(e).__name__, str(e)[:100])

# 3. HDF5/BUFR/GRIB stub opens with valid magic
hdf5 = b"\x89HDF\r\n\x1a\n" + b"\x00" * 64
ro = Image.open(io.BytesIO(hdf5))
print("HDF5 open preload:", ro.mode, ro.size, ro.format)
try:
    ro.load()
    print("HDF5 stub load OK")
except Exception as e:
    print("HDF5 stub load ERR", type(e).__name__, str(e)[:100])

bufr = b"BUFR" + b"\x00" * 64
ro = Image.open(io.BytesIO(bufr))
print("BUFR open preload:", ro.mode, ro.size, ro.format)
try:
    ro.load()
    print("BUFR stub load OK")
except Exception as e:
    print("BUFR stub load ERR", type(e).__name__, str(e)[:100])

grib = b"GRIB" + b"\x00\x00\x00\x01" + b"\x00" * 64  # prefix[7] == 1
ro = Image.open(io.BytesIO(grib))
print("GRIB open preload:", ro.mode, ro.size, ro.format)
try:
    ro.load()
    print("GRIB stub load OK")
except Exception as e:
    print("GRIB stub load ERR", type(e).__name__, str(e)[:100])

# 4. WMF open with a minimal placeable header (drawwmf present?)
hdr = b"\xd7\xcd\xc6\x9a\x00\x00" + struct.pack("<h", 200) * 0 + b"\x00" * 8
wmf = (b"\xd7\xcd\xc6\x9a\x00\x00" + struct.pack("<4h", 0, 0, 100, 100) +
       struct.pack("<h", 96) + b"\x00" * 12 + b"\x01\x00\x09\x00" + b"\x00" * 12)
try:
    ro = Image.open(io.BytesIO(wmf))
    print("WMF open header OK:", ro.mode, ro.size, ro.info.get("dpi"))
    ro.load()
    print("WMF load OK:", ro.size)
except Exception as e:
    print("WMF ERR", type(e).__name__, str(e)[:100])

# 5. WMF save error
buf = io.BytesIO()
try:
    im.save(buf, "WMF")
    print("WMF save OK??")
except Exception as e:
    print("WMF save ERR", type(e).__name__, str(e)[:100])

# 6. FPX (removed?)
try:
    import PIL.FpxImagePlugin  # noqa
    print("FPX plugin importable")
except Exception as e:
    print("FPX import ERR", type(e).__name__, str(e)[:100])

# 7. MPEG open plugin
try:
    import PIL.MpegImagePlugin
    print("MPEG plugin importable, _accept:", PIL.MpegImagePlugin._accept(b"\x00" * 16))
except Exception as e:
    print("MPEG import ERR", type(e).__name__, str(e)[:100])

# 8. EPS save basic + Ghostscript absence for open
im = Image.new("RGB", (2, 2))
buf = io.BytesIO()
try:
    im.save(buf, "EPS")
    print("EPS save OK len", len(buf.getvalue()))
except Exception as e:
    print("EPS save ERR", type(e).__name__, str(e)[:100])
try:
    ro = Image.open(io.BytesIO(buf.getvalue()))
    ro.load()
    print("EPS open OK")
except Exception as e:
    print("EPS open ERR", type(e).__name__, str(e)[:100])

# 9. PDF save basic + open error
buf = io.BytesIO()
try:
    im.save(buf, "PDF")
    print("PDF save OK len", len(buf.getvalue()))
except Exception as e:
    print("PDF save ERR", type(e).__name__, str(e)[:100])
try:
    ro = Image.open(io.BytesIO(buf.getvalue()))
    ro.load()
    print("PDF open OK")
except Exception as e:
    print("PDF open ERR", type(e).__name__, str(e)[:100])

# 10. MPO save
buf = io.BytesIO()
try:
    im.save(buf, "MPO")
    print("MPO save OK len", len(buf.getvalue()))
except Exception as e:
    print("MPO save ERR", type(e).__name__, str(e)[:100])
