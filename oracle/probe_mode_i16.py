"""One-shot oracle probe for MODE-NUM-001CM.

Pillow 11.3.0 mode I;16 / I;16B resize, transform, and rotate semantics.
"""

from PIL import Image

VALUES = [1000, 50000, 60000, 300, 200, 65535]


def describe(label, im):
    print(label, "mode=%s size=%s data=%s raw=%s" % (
        im.mode, im.size, list(im.getdata()), im.tobytes().hex(" ")))


def main():
    im = Image.new("I;16", (3, 2))
    im.putdata(VALUES)
    imb = Image.new("I;16B", (3, 2))
    imb.putdata(VALUES)

    for name, src in [("I;16", im), ("I;16B", imb)]:
        for resample_name in ["NEAREST", "BILINEAR", "BICUBIC"]:
            resample = getattr(Image.Resampling, resample_name)
            try:
                out = src.resize((4, 3), resample=resample)
                describe("RESIZE %s %s" % (name, resample_name), out)
            except Exception as err:
                print("RESIZE %s %s ERR" % (name, resample_name),
                      type(err).__name__, "|", str(err))

        for resample_name in ["NEAREST", "BILINEAR", "BICUBIC"]:
            resample = getattr(Image.Resampling, resample_name)
            try:
                out = src.transform((4, 3), Image.AFFINE, (1.0, 0.0, 0.5, 0.0, 1.0, 0.5), resample=resample)
                describe("AFFINE %s %s" % (name, resample_name), out)
            except Exception as err:
                print("AFFINE %s %s ERR" % (name, resample_name),
                      type(err).__name__, "|", str(err))

        try:
            out = src.rotate(45, resample=Image.Resampling.BILINEAR)
            describe("ROT45 %s BILINEAR" % name, out)
        except Exception as err:
            print("ROT45 %s BILINEAR ERR" % name, type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
