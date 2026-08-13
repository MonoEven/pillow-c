"""One-shot oracle probe for MODE-NUM-001CH (fill error parity)."""

from PIL import Image


def main():
    f = Image.new("F", (2, 2))
    i = Image.new("I", (2, 2))
    for label, im in [("F", f), ("I", i)]:
        for bad in [(1, 2), "red"]:
            try:
                im.transform((4, 3), Image.AFFINE, (2.0, 0.0, 0.0, 0.0, 2.0, 0.0),
                             resample=Image.Resampling.NEAREST, fillcolor=bad)
                print(label, repr(bad), "ACCEPTED")
            except Exception as err:
                print(label, repr(bad), "ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
