"""One-shot oracle probe for MODE-NUM-001CQ.

Pillow 11.3.0 mode I;16 / I;16B entropy / getcolors / ImageStat.
"""

from PIL import Image, ImageStat

VALUES = [1000, 50000, 60000, 300, 200, 50000]


def main():
    for mode in ["I;16", "I;16B"]:
        im = Image.new(mode, (3, 2))
        im.putdata(VALUES)
        try:
            print(mode, "entropy ->", im.entropy())
        except Exception as err:
            print(mode, "entropy ERR", type(err).__name__, "|", str(err))
        try:
            print(mode, "getcolors ->", im.getcolors())
        except Exception as err:
            print(mode, "getcolors ERR", type(err).__name__, "|", str(err))
        try:
            st = ImageStat.Stat(im)
            print(mode, "stat extrema ->", st.extrema, "count ->", st.count,
                  "mean ->", st.mean, "median ->", st.median)
        except Exception as err:
            print(mode, "stat ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
