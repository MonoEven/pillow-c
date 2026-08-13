"""One-shot oracle probe for MODE-NUM-001CP.

Pillow 11.3.0 mode I;16 / I;16B getextrema / histogram / convert("L").
"""

from PIL import Image

VALUES = [1000, 50000, 60000, 300, 200, 65535]


def main():
    for mode in ["I;16", "I;16B"]:
        im = Image.new(mode, (3, 2))
        im.putdata(VALUES)
        print(mode, "getextrema ->", im.getextrema())
        hist = im.histogram()
        print(mode, "histogram len ->", len(hist), "nonzero ->",
              [(i, v) for i, v in enumerate(hist) if v][:12])
        l = im.convert("L")
        print(mode, "convert L ->", l.mode, list(l.getdata()), l.tobytes().hex(" "))
        i32 = im.convert("I")
        print(mode, "convert I ->", i32.mode, list(i32.getdata()))
        try:
            f = im.convert("F")
            print(mode, "convert F ->", f.mode, list(f.getdata()))
        except Exception as err:
            print(mode, "convert F ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
