"""API-PALETTE-001 oracle probe: Pillow 11.3.0 PIL.ImagePalette surface.

Records the deterministic public generator outputs (wedge/negative/sepia/
make_linear_lut/make_gamma_lut), the raw/getcolor allocation semantics, and
the load() boundary so the AHK facade can assert exact parity.

Run: F:\\Python\\Python310\\python.exe oracle/probe_imagepalette.py
"""
import json

from PIL import ImagePalette


def main() -> None:
    wedge = ImagePalette.wedge("RGB")
    negative = ImagePalette.negative("RGB")
    negative_l = ImagePalette.negative("L")
    sepia = ImagePalette.sepia()
    linear255 = ImagePalette.make_linear_lut(0, 255)
    linear128 = ImagePalette.make_linear_lut(0, 128.0)
    linear240 = ImagePalette.make_linear_lut(0, 240.0)
    gamma = ImagePalette.make_gamma_lut(0.5)

    report = {
        "wedge_head9": list(wedge.tobytes())[:9],
        "wedge_tail9": list(wedge.tobytes())[-9:],
        "negative_head6": list(negative.tobytes())[:6],
        "negative_tail6": list(negative.tobytes())[-6:],
        "negative_l_tail3": list(negative_l.tobytes())[-3:],
        "sepia_head9": list(sepia.tobytes())[:9],
        "sepia_tail9": list(sepia.tobytes())[-9:],
        "linear255_head3": linear255[:3],
        "linear255_tail3": linear255[-3:],
        "linear128_mid4": linear128[127:131],
        "linear240_mid4": linear240[127:131],
        "linear240_tail3": linear240[-3:],
        "gamma_head5": gamma[:5],
        "gamma_mid4": gamma[127:131],
        "gamma_tail3": gamma[-3:],
        "raw_tobytes_error": _capture(lambda: ImagePalette.raw("RGB", bytes(range(12))).tobytes()),
        "raw_getdata": _getdata(ImagePalette.raw("RGB", list(range(12)))),
        "getcolor_alloc": _getcolor_alloc(),
        "getcolor_alpha_error": _capture(
            lambda: ImagePalette.ImagePalette("RGB", list(range(12))).getcolor((1, 2, 3, 128))
        ),
        "getcolor_full_error": _capture(
            lambda: ImagePalette.ImagePalette("RGB", ImagePalette.wedge("RGB").palette).getcolor((1, 2, 3))
        ),
        "getcolor_spec_error": _capture(
            lambda: ImagePalette.ImagePalette("RGB", list(range(12))).getcolor("red")
        ),
        "linear_black_error": _capture(lambda: ImagePalette.make_linear_lut(1, 255)),
        "rgba_append": _rgba_append(),
        "defaults": _defaults(),
        "save_layout": _save_layout(),
        "load_boundary": None,  # load() parses GIMP/Adobe text files; the facade fails loudly.
    }
    print(json.dumps(report, indent=2))


def _capture(fn):
    try:
        fn()
    except Exception as err:
        return f"{type(err).__name__}: {err}"
    return "no error"


def _getdata(palette):
    return palette.getdata()


def _getcolor_alloc():
    p = ImagePalette.ImagePalette("RGB", list(range(12)))
    return [p.getcolor((3, 4, 5)), p.getcolor((1, 2, 3)), len(p.palette),
            p.getcolor((9, 9, 9)), len(p.palette),
            p.getcolor((10, 20, 30, 255)), len(p.palette)]


def _rgba_append():
    p = ImagePalette.ImagePalette("RGBA", [])
    return [p.getcolor((1, 2, 3)), list(p.palette)]


def _defaults():
    p = ImagePalette.ImagePalette()
    return {"mode": p.mode, "rawmode": p.rawmode, "dirty": p.dirty, "len": len(p.palette)}


def _save_layout():
    import io

    class KeepOpen(io.StringIO):
        def close(self) -> None:
            self.flush()

    p = ImagePalette.ImagePalette("RGB", list(range(12)))
    fp = KeepOpen()
    p.save(fp)
    text = fp.getvalue().replace("\n", "\\n")
    return text[:80] + "..." + text[-80:]


if __name__ == "__main__":
    main()
