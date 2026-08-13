"""One-shot oracle probe for MODE-NUM-001CI.

Pillow 11.3.0 mode I/F Image.rotate semantics.
"""

from PIL import Image

I_VALUES = [1000, -2000, 3000, 7, -8, 9]
F_VALUES = [1.5, -2.5, 3.5, 0.25, -0.125, 2.0]


def describe(label, im):
    print(label, "mode=%s size=%s data=%s raw=%s" % (
        im.mode, im.size, list(im.getdata()), im.tobytes().hex(" ")))


def main():
    im_i = Image.new("I", (3, 2))
    im_i.putdata(I_VALUES)
    im_f = Image.new("F", (3, 2))
    im_f.putdata(F_VALUES)

    for name, im in [("I", im_i), ("F", im_f)]:
        for resample_name in ["NEAREST", "BILINEAR", "BICUBIC"]:
            for expand in [False, True]:
                try:
                    out = im.rotate(45.0, resample=getattr(Image.Resampling, resample_name), expand=expand)
                    describe("ROT45 %s %s expand=%s" % (name, resample_name, expand), out)
                except Exception as err:
                    print("ROT45 %s %s expand=%s ERR" % (name, resample_name, expand),
                          type(err).__name__, "|", str(err))

        try:
            out = im.rotate(45.0, resample=Image.Resampling.NEAREST, fillcolor=-33 if name == "I" else 7.5)
            describe("ROT45 %s NEAREST fill" % name, out)
        except Exception as err:
            print("ROT45 %s NEAREST fill ERR" % name, type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
