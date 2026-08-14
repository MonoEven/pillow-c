"""One-shot oracle probe for API-PATH-001 (Pillow 11 Path surface)."""

from PIL import ImagePath


def main():
    # Constructor forms.
    for xy in [
        [(1, 1), (5, 5)],
        [1, 1, 5, 5],
        [1.5, 2.5, 6.5, 7.5],
        [],
    ]:
        try:
            p = ImagePath.Path(xy)
            print("ctor", repr(xy), "->", p.tolist(), p.getbbox())
        except Exception as err:
            print("ctor", repr(xy), "ERR", type(err).__name__, "|", str(err))

    # tolist(flat) with only move/line points: coded form?
    p = ImagePath.Path([(1, 1), (5, 5)])
    print("tolist False:", p.tolist(False))

    # compact on straight points (merges collinear); mutates in place
    # and returns the point count.
    c = ImagePath.Path([(0, 0), (2, 2), (4, 4), (6, 6), (6, 10), (0, 10)])
    n0 = c.compact(0)
    print("compact(0) ->", n0, c.tolist())
    c2 = ImagePath.Path([(0, 0), (2, 2), (4, 4), (6, 6), (6, 10), (0, 10)])
    n2 = c2.compact(2.0)
    print("compact(2.0) ->", n2, c2.tolist())
    c3 = ImagePath.Path([(0, 0), (2, 2), (4, 4), (6, 6), (6, 10), (0, 10)])
    print("compact(-1):", end=" ")
    try:
        print(c3.compact(-1))
    except Exception as err:
        print("ERR", type(err).__name__, "|", str(err))

    # transform.
    t = ImagePath.Path([(0, 0), (10, 0)])
    t.transform((2.0, 0.0, 5.0, 0.0, 2.0, 5.0))
    print("transform:", t.tolist())
    try:
        t.transform((1.0, 0.0, 0.0, 0.0, 1.0))
    except Exception as err:
        print("transform bad ERR", type(err).__name__, "|", str(err))

    # map with a callable.
    m = ImagePath.Path([(0, 0), (10, 10)])
    try:
        print("map:", m.map(lambda x, y: (x + 1, y + 2)))
    except Exception as err:
        print("map ERR", type(err).__name__, "|", str(err))

    # getbbox empty.
    print("empty bbox:", ImagePath.Path([]).getbbox())


if __name__ == "__main__":
    main()
