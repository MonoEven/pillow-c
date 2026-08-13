"""One-shot oracle probe for MODE-F-001B.

Pillow 11.3.0 mode F point() linear-callable semantics.
"""

import struct

from PIL import Image


def main():
    f = Image.new("F", (2, 2))
    f.putdata([1.5, -2.5, 3.5, 0.0])
    out = f.point(lambda x: 2 * x + 5)
    data = list(out.getdata())
    print("2x+5 ->", data)
    print("2x+5 bytes ->", struct.pack("<4f", *data).hex(" "))

    out = f.point(lambda x: -1.5)
    print("constant ->", list(out.getdata()))

    out = f.point(lambda x: 0.5 * x)
    print("half ->", list(out.getdata()))

    try:
        f.point([0, 1, 2])
    except Exception as err:
        print("list ERR", type(err).__name__, "|", str(err))

    try:
        f.point(lambda x: x * x)
    except Exception as err:
        print("quadratic ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
