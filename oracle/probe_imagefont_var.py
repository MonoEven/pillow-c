"""API-FONTVAR-001 oracle probe: Pillow 11.3.0 ImageFont variation surface.

Records Layout (IntEnum), the type-only Axis TypedDict, and TransposedFont
semantics (orientation storage, getbbox normalization, getlength delegation
and the 90/270 error, getmask wrapping) so the AHK facade can assert parity.

Run: F:\\Python\\Python310\\python.exe oracle/probe_imagefont_var.py
"""
import json

from PIL import Image, ImageFont


def main() -> None:
    report = {
        "layout_basic": ImageFont.Layout.BASIC,
        "layout_raqm": ImageFont.Layout.RAQM,
        "axis_is_typed_dict": None,  # Axis is a typing.TypedDict: type-only.
    }
    font = ImageFont.load_default()
    t = ImageFont.TransposedFont(font)
    left, top, right, bottom = font.getbbox("ab")
    report["default_bbox"] = [left, top, right, bottom]
    report["transposed_bbox"] = list(t.getbbox("ab"))
    report["transposed_length"] = t.getlength("ab")
    report["font_length"] = font.getlength("ab")
    r = ImageFont.TransposedFont(font, Image.Transpose.ROTATE_90)
    report["rotated_bbox"] = list(r.getbbox("ab"))
    try:
        r.getlength("ab")
    except Exception as err:
        report["rotated_length"] = f"{type(err).__name__}: {err}"
    f = ImageFont.TransposedFont(font, Image.Transpose.FLIP_LEFT_RIGHT)
    report["flip_bbox"] = list(f.getbbox("ab"))
    report["flip_length"] = f.getlength("ab")
    mask = t.getmask("ab")
    report["getmask_type"] = type(mask).__name__
    report["getmask_size"] = mask.size
    report["orientation_default"] = t.orientation
    report["orientation_stored"] = r.orientation
    print(json.dumps(report, indent=2, default=str))


if __name__ == "__main__":
    main()
