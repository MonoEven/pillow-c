"""FMT-UNREC-001 oracle probe: Pillow 11.3.0 behavior per unrecorded format.

Records what Pillow's own 11.3.0 build does for the format families the
AUDIT-002 re-audit found neither implemented nor recorded: save BLP/BUFR/
DIB/GRIB/HDF5/IM/MSP/PALM/SPIDER/WMF and open FITS/FPX/FTEX/GBR/IMT/IPTC/
MCIDAS/MIC/MPEG/PCD/PIXAR/SPIDER/WMF/XVTHUMB. Pillow supports BLP/DIB/IM/
SPIDER through its own C/numpy plugins and errors per-mode or per-handler
on the rest; the AHK native ABI implements neither codec, so the facade
records them as explicit documented codec boundaries (fail-loud
`Pillow image file format is unsupported`).

Run: F:\\Python\\Python310\\python.exe oracle/probe_format_unrecorded.py
"""
import io
import json

from PIL import Image


def main() -> None:
    im = Image.new("L", (2, 2), 7)
    report = {}
    for fmt in ["BLP", "BUFR", "DIB", "GRIB", "HDF5", "IM", "MSP", "PALM", "SPIDER", "WMF"]:
        buf = io.BytesIO()
        try:
            im.save(buf, fmt)
            report[f"save_{fmt}"] = f"ok {len(buf.getvalue())} bytes"
        except Exception as err:
            report[f"save_{fmt}"] = f"{type(err).__name__}: {err}"
    for fmt in ["FITS", "MPEG", "PCD", "WMF", "BLP"]:
        try:
            Image.open(io.BytesIO(b"xxxx")).load()
            report[f"open_{fmt}"] = "ok"
        except Exception as err:
            report[f"open_{fmt}"] = f"{type(err).__name__}: {err}".split("<_io.BytesIO")[0].strip()
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
