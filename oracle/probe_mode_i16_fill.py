"""One-shot oracle probe for MODE-NUM-001CO.

Pillow 11.3.0 mode I;16 / I;16B transform/rotate NEAREST fill semantics.
"""

from PIL import Image


def main():
    im = Image.new("I;16", (3, 2))
    im.putdata([1000, 50000, 60000, 300, 200, 65535])
    imb = Image.new("I;16B", (3, 2))
    imb.putdata([1000, 50000, 60000, 300, 200, 65535])

    for name, src in [("I;16", im), ("I;16B", imb)]:
        for fill in [300, 65535, (300,), (50000, 70000), 7.5, "red"]:
            try:
                out = src.transform((4, 3), Image.AFFINE, (1.0, 0.0, 0.5, 0.0, 1.0, 0.5),
                                    resample=Image.Resampling.NEAREST, fillcolor=fill)
                print(name, "fill=", repr(fill), "data=", list(out.getdata())[:2],
                      "raw=", out.tobytes()[:4].hex(" "))
            except Exception as err:
                print(name, "fill=", repr(fill), "ERR", type(err).__name__, "|", str(err))

        try:
            out = src.rotate(45.0, resample=Image.Resampling.NEAREST, fillcolor=300)
            print(name, "ROT fill=300 data=", list(out.getdata())[:3],
                  "raw=", out.tobytes()[:6].hex(" "))
        except Exception as err:
            print(name, "ROT fill=300 ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
