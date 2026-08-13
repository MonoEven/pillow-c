"""One-shot oracle probe for MODE-NUM-001CH (fill acceptance matrix)."""

from PIL import Image


def main():
    f = Image.new("F", (2, 2))
    i = Image.new("I", (2, 2))
    cases = [(7.5,), (300,), "red", "blue"]
    for label, im in [("F", f), ("I", i)]:
        for bad in cases:
            try:
                out = im.transform((4, 3), Image.AFFINE, (2.0, 0.0, 0.0, 0.0, 2.0, 0.0),
                                   resample=Image.Resampling.NEAREST, fillcolor=bad)
                print(label, repr(bad), "ACCEPTED data=", list(out.getdata())[:2],
                      "raw=", out.tobytes()[:8].hex(" "))
            except Exception as err:
                print(label, repr(bad), "ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
