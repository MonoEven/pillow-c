"""AUDIT-003: probe the local Pillow build's behavior for every remaining
OPEN format — a junk file reveals accept/dependency/decode behavior."""

import io
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

formats = [
    "AVIF", "BUFR", "DCX", "EPS", "FITS", "FLI", "FPX", "FTEX", "GBR",
    "GRIB", "HDF5", "ICNS", "IMT", "IPTC", "JPEG2000", "MCIDAS", "MIC",
    "MPEG", "PCD", "PIXAR", "PSD", "SUN", "WEBP", "WMF", "XPM", "XVTHUMB",
]
for fmt in formats:
    blob = b"\x00" * 64
    try:
        im = Image.open(io.BytesIO(blob), formats=[fmt])
        im.load()
        print(fmt, "OPENED", im.mode, im.size)
    except Exception as e:
        print(fmt, "ERR", type(e).__name__, str(e)[:110])
