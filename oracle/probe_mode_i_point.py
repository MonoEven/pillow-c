"""One-shot oracle probe for MODE-I-001B.

Pillow 11.3.0 point() table semantics across I, I;16, and F modes.
"""

from PIL import Image


def probe(label, im, table):
    try:
        out = im.point(table)
        print(label, "->", list(out.getdata()))
    except Exception as err:
        print(label, "ERR", type(err).__name__, "|", str(err))


def main():
    table = [i * 100 for i in range(256)]

    im = Image.new("I", (2, 2))
    im.putdata([-1, 7, 300, 0])
    probe("I", im, table)
    probe("I callable", im, lambda x: x + 1)

    i16 = Image.new("I;16", (2, 2))
    i16.putdata([0, 1, 513, 65535])
    probe("I;16", i16, table)

    f = Image.new("F", (2, 2))
    f.putdata([1.5, -2.5, 3.5, 0.0])
    probe("F", f, table)

    l = Image.new("L", (2, 2))
    l.putdata([0, 7, 200, 255])
    probe("L", l, table)


if __name__ == "__main__":
    main()
