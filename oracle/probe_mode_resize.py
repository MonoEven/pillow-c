"""One-shot oracle probe for MODE-NUM-001CK.

Pillow 11.3.0 mode I/F Image.resize semantics.
"""

from PIL import Image

I_VALUES = [1000, -2000, 3000, 7]
F_VALUES = [1.5, -2.5, 3.5, 0.25]


def describe(label, im):
    print(label, "mode=%s size=%s data=%s raw=%s" % (
        im.mode, im.size, list(im.getdata()), im.tobytes().hex(" ")))


def main():
    im_i = Image.new("I", (2, 2))
    im_i.putdata(I_VALUES)
    im_f = Image.new("F", (2, 2))
    im_f.putdata(F_VALUES)

    for name, im in [("I", im_i), ("F", im_f)]:
        for resample_name in ["NEAREST", "BILINEAR", "BICUBIC", "LANCZOS"]:
            try:
                out = im.resize((3, 3), resample=getattr(Image.Resampling, resample_name))
                describe("RESIZE %s %s" % (name, resample_name), out)
            except Exception as err:
                print("RESIZE %s %s ERR" % (name, resample_name),
                      type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
