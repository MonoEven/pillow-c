"""One-shot oracle probe for MODE-NUM-001CJ.

Pillow 11.3.0 mode I/F PERSPECTIVE/QUAD/MESH transform semantics.
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

    perspective = (1.0, 0.05, 0.5, -0.05, 1.0, 0.5, 0.0005, 0.0005)
    quad = (0.5, 0.5, 2.5, 0.5, 2.5, 1.5, 0.5, 1.5)
    mesh = [((0, 0, 2, 2), (0.0, 0.0, 2.5, 0.0, 2.5, 1.5, 0.0, 1.5))]

    for name, im in [("I", im_i), ("F", im_f)]:
        for resample_name in ["NEAREST", "BILINEAR", "BICUBIC"]:
            resample = getattr(Image.Resampling, resample_name)
            try:
                out = im.transform((3, 2), Image.PERSPECTIVE, perspective, resample=resample)
                describe("PERSPECTIVE %s %s" % (name, resample_name), out)
            except Exception as err:
                print("PERSPECTIVE %s %s ERR" % (name, resample_name),
                      type(err).__name__, "|", str(err))

            try:
                out = im.transform((3, 2), Image.QUAD, quad, resample=resample)
                describe("QUAD %s %s" % (name, resample_name), out)
            except Exception as err:
                print("QUAD %s %s ERR" % (name, resample_name),
                      type(err).__name__, "|", str(err))

            try:
                out = im.transform((3, 2), Image.MESH, mesh, resample=resample)
                describe("MESH %s %s" % (name, resample_name), out)
            except Exception as err:
                print("MESH %s %s ERR" % (name, resample_name),
                      type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
