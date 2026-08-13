"""One-shot oracle probe for MODE-NUM-001CL.

Pillow 11.3.0 mode I/F boxed Image.resize and Image.thumbnail semantics.
"""

from PIL import Image

I_VALUES = [1000, -2000, 3000, 7]
F_VALUES = [1.5, -2.5, 3.5, 0.25]

I_BIG = [100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200]
F_BIG = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2]


def describe(label, im):
    print(label, "mode=%s size=%s data=%s raw=%s" % (
        im.mode, im.size, list(im.getdata()), im.tobytes().hex(" ")))


def main():
    im_i = Image.new("I", (2, 2))
    im_i.putdata(I_VALUES)
    im_f = Image.new("F", (2, 2))
    im_f.putdata(F_VALUES)

    for name, im in [("I", im_i), ("F", im_f)]:
        for resample_name in ["NEAREST", "BILINEAR", "BICUBIC"]:
            resample = getattr(Image.Resampling, resample_name)
            try:
                out = im.resize((3, 3), resample=resample, box=(0.5, 0.5, 1.5, 1.5))
                describe("BOX %s %s" % (name, resample_name), out)
            except Exception as err:
                print("BOX %s %s ERR" % (name, resample_name),
                      type(err).__name__, "|", str(err))

    # Thumbnail: 4x3 source -> (2, 2), BICUBIC (in-place mutation).
    ti = Image.new("I", (4, 3))
    ti.putdata(I_BIG)
    tf = Image.new("F", (4, 3))
    tf.putdata(F_BIG)
    for name, im in [("I", ti), ("F", tf)]:
        for resample_name in ["BILINEAR", "BICUBIC"]:
            copy = im.copy()
            copy.thumbnail((2, 2), resample=getattr(Image.Resampling, resample_name))
            describe("THUMB %s %s" % (name, resample_name), copy)


if __name__ == "__main__":
    main()
