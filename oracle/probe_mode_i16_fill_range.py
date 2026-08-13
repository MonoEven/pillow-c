"""One-shot oracle probe for MODE-NUM-001CO (out-of-range fills)."""

from PIL import Image


def main():
    im = Image.new("I;16", (3, 2))
    im.putdata([1, 2, 3, 4, 5, 6])
    for fill in [70000, -5, 70000.0, (70000,), "red"]:
        try:
            out = im.transform((2, 2), Image.AFFINE, (2, 0, 0, 0, 2, 0),
                               resample=Image.Resampling.NEAREST, fillcolor=fill)
            print("fill", repr(fill), "->", list(out.getdata()), out.tobytes().hex(" "))
        except Exception as err:
            print("fill", repr(fill), "ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
