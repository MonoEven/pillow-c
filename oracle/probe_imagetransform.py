"""API-TRANSFORMCLS-001 oracle probe: Pillow 11.3.0 PIL.ImageTransform surface.

Records the module/class-object surface — the base Transform (data storage,
getdata AttributeError, transform routing) and the five method-constant
subclasses — so the AHK facade can assert exact parity.

Run: F:\\Python\\Python310\\python.exe oracle/probe_imagetransform.py
"""
import json

from PIL import Image, ImageTransform


def main() -> None:
    report = {}
    try:
        ImageTransform()
    except Exception as err:
        report["module_call"] = f"{type(err).__name__}: {err}"

    base = ImageTransform.Transform([9, 9])
    report["base_data"] = base.data
    try:
        base.getdata()
    except Exception as err:
        report["base_getdata"] = f"{type(err).__name__}: {err}"

    report["affine_getdata"] = ImageTransform.AffineTransform((1, 0, 0, 0, 1, 0)).getdata()
    report["extent_getdata"] = ImageTransform.ExtentTransform((0, 0, 2, 2)).getdata()
    report["perspective_getdata"] = ImageTransform.PerspectiveTransform(
        (1, 0, 0, 0, 1, 0, 0, 0)
    ).getdata()
    report["quad_getdata"] = ImageTransform.QuadTransform((0, 0, 1, 0, 1, 1, 0, 1)).getdata()
    report["mesh_getdata"] = ImageTransform.MeshTransform(
        [[(0, 0, 2, 2), (0, 0, 1, 0, 1, 1, 0, 1)]]
    ).getdata()

    im = Image.new("L", (4, 4), 7)
    t = ImageTransform.AffineTransform((1, 0, 0, 0, 1, 0))
    r1 = t.transform((4, 4), im)
    r2 = im.transform((4, 4), Image.Transform.AFFINE, (1, 0, 0, 0, 1, 0))
    report["affine_identity_bytes_equal"] = r1.tobytes() == r2.tobytes()
    report["extent_bytes_equal"] = (
        ImageTransform.ExtentTransform((0, 0, 2, 2)).transform((2, 2), im).tobytes()
        == im.transform((2, 2), Image.Transform.EXTENT, (0, 0, 2, 2)).tobytes()
    )
    print(json.dumps(report, indent=2, default=str))


if __name__ == "__main__":
    main()
