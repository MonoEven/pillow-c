"""Behavioral-parity audit probe: classify every boundary format against local Pillow 11.3.0.

For each format family recorded as a boundary in BNDRY-001/FMT-UNREC-001, this
probe records what Pillow's own build actually does — save across the L/RGB/
RGBA/P mode matrix and reopen of its own output — plus the exact error text
when it fails. Classification: works-in-Pillow (must implement), errors-in-
Pillow (must match the message), dependency-gated (match Pillow's not-enabled
message shape).

Run: F:\\Python\\Python310\\python.exe oracle/probe_format_boundary_matrix.py
"""
import io
import json

from PIL import Image, features


def capture_save(im, fmt):
    buf = io.BytesIO()
    try:
        im.save(buf, fmt)
        return {"status": "ok", "bytes": len(buf.getvalue())}
    except Exception as err:
        return {"status": f"{type(err).__name__}", "message": str(err)}


def capture_open(data):
    try:
        im = Image.open(io.BytesIO(data))
        im.load()
        return {"status": "ok", "mode": im.mode, "size": list(im.size)}
    except Exception as err:
        return {"status": f"{type(err).__name__}", "message": str(err)[:160]}


def main() -> None:
    report = {"features": {}}
    for name in ["webp", "webp_anim", "webp_mux", "jpg_2000", "avif", "libtiff", "zlib"]:
        try:
            report["features"][name] = features.check(name)
        except Exception as err:
            report["features"][name] = f"err {err}"

    report["modules"] = {}
    for mod in ["numpy", "olefile", "astropy", "tkinter", "pygrib", "h5py"]:
        try:
            __import__(mod)
            report["modules"][mod] = True
        except Exception:
            report["modules"][mod] = False

    formats = [
        "BLP", "BUFR", "DIB", "GRIB", "HDF5", "IM", "MSP", "PALM", "SPIDER", "WMF",
        "FITS", "FPX", "FTEX", "GBR", "IMT", "IPTC", "MCIDAS", "MIC", "MPEG", "PCD",
        "PIXAR", "XVTHUMB",
        "WEBP", "AVIF", "JPEG2000", "PDF", "PSD", "DDS", "PCX", "ICNS", "SGI", "SUN",
        "EPS", "MPO", "FLI", "DCX", "XPM",
    ]
    for fmt in formats:
        entry = {}
        for mode in ["L", "RGB", "RGBA", "P"]:
            im = Image.new(mode, (8, 6))
            if mode == "P":
                im.putpalette(list(range(256)) * 3)
            entry[f"save_{mode}"] = capture_save(im, fmt)
        # reopen Pillow's own L output when save succeeded
        im = Image.new("L", (8, 6))
        buf = io.BytesIO()
        try:
            im.save(buf, fmt)
            entry["reopen_own_l"] = capture_open(buf.getvalue())
        except Exception:
            entry["reopen_own_l"] = capture_open(b"not-a-real-image")
        report[fmt] = entry

    with open(r".codex\boundary-matrix.json", "w", encoding="utf-8") as fp:
        json.dump(report, fp, indent=2)
    print("written .codex/boundary-matrix.json")


if __name__ == "__main__":
    main()
