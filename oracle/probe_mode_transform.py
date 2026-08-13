"""One-shot oracle probe for MODE-NUM-001CH.

Pillow 11.3.0 mode I/F Image.transform AFFINE/EXTENT semantics.
"""

import struct

from PIL import Image

I_VALUES = [1000, -2000, 3000, 7, -8, 9]
F_VALUES = [1.5, -2.5, 3.5, 0.25, -0.125, 2.0]


def describe(label, im):
    data = list(im.getdata())
    raw = im.tobytes()
    print(label, "mode=%s size=%s data=%s raw=%s" % (im.mode, im.size, data, raw.hex(" ")))


def main():
    im_i = Image.new("I", (3, 2))
    im_i.putdata(I_VALUES)
    im_f = Image.new("F", (3, 2))
    im_f.putdata(F_VALUES)

    # AFFINE: identity-ish translate so interpolation samples interior pixels.
    matrix = (1.0, 0.0, 0.5, 0.0, 1.0, 0.5)
    for name, im in [("I", im_i), ("F", im_f)]:
        for resample_name in ["NEAREST", "BILINEAR", "BICUBIC"]:
            try:
                out = im.transform(
                    (4, 3), Image.AFFINE, matrix,
                    resample=getattr(Image.Resampling, resample_name))
                describe("AFFINE %s %s" % (name, resample_name), out)
            except Exception as err:
                print("AFFINE %s %s ERR" % (name, resample_name),
                      type(err).__name__, "|", str(err))

        for resample_name in ["NEAREST", "BILINEAR"]:
            try:
                out = im.transform(
                    (4, 3), Image.EXTENT, (0.5, 0.5, 2.5, 1.5),
                    resample=getattr(Image.Resampling, resample_name))
                describe("EXTENT %s %s" % (name, resample_name), out)
            except Exception as err:
                print("EXTENT %s %s ERR" % (name, resample_name),
                      type(err).__name__, "|", str(err))

    # Fill behaviors.
    try:
        out = im_f.transform((4, 3), Image.AFFINE, (2.0, 0.0, 0.0, 0.0, 2.0, 0.0),
                             resample=Image.Resampling.NEAREST, fillcolor=7.5)
        describe("AFFINE F fillcolor=7.5", out)
    except Exception as err:
        print("AFFINE F fillcolor ERR", type(err).__name__, "|", str(err))

    try:
        out = im_i.transform((4, 3), Image.AFFINE, (2.0, 0.0, 0.0, 0.0, 2.0, 0.0),
                             resample=Image.Resampling.NEAREST, fillcolor=-33)
        describe("AFFINE I fillcolor=-33", out)
    except Exception as err:
        print("AFFINE I fillcolor ERR", type(err).__name__, "|", str(err))

    # Default fill (no fillcolor) with out-of-range sampling.
    try:
        out = im_f.transform((2, 2), Image.AFFINE, (1.0, 0.0, 5.0, 0.0, 1.0, 5.0),
                             resample=Image.Resampling.NEAREST)
        describe("AFFINE F default fill OOB", out)
    except Exception as err:
        print("AFFINE F default fill OOB ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
