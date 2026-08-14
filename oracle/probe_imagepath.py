"""One-shot oracle probe for API-PATH-001.

Pillow 11.3.0 ImagePath.Path surface and semantics.
"""

from PIL import ImagePath


def main():
    print("names:", sorted(n for n in dir(ImagePath.Path) if not n.startswith("_")))

    p = ImagePath.Path([(1, 1), (5, 5)])
    print("tolist flat:", p.tolist())
    print("tolist coded:", p.tolist(False))
    print("getbbox:", p.getbbox())

    p2 = ImagePath.Path([(0, 0)])
    print("empty-ish bbox:", p2.getbbox())
    p3 = ImagePath.Path([])
    print("empty bbox:", p3.getbbox(), "tolist:", p3.tolist())

    q = ImagePath.Path([(0, 0), (10, 0), (10, 10), (0, 10)])
    q.close()
    print("closed tolist:", q.tolist())

    r = ImagePath.Path([(0, 0)])
    r.line([(10, 10)], relative=True)
    r.line([(20, 20)])
    print("line rel/abs:", r.tolist())

    c = ImagePath.Path([(0, 0)])
    c.curve([(10, 10), (20, 10), (30, 0)])
    print("curve:", c.tolist())
    flat = c.compact(0.5)
    print("compact(0.5):", flat.tolist())
    flat2 = c.compact(0)
    print("compact(0):", flat2.tolist())

    t = ImagePath.Path([(0, 0), (10, 0)])
    t.transform((2.0, 0.0, 5.0, 0.0, 2.0, 5.0))
    print("transform:", t.tolist())

    m = ImagePath.Path([(0, 0), (10, 10)])
    print("map bbox:", m.getbbox())
    a = ImagePath.Path([(0, 0)])
    a.arc([(10, 10)], 0, 90)
    print("arc(0..90):", a.tolist())

    e = ImagePath.Path([(0, 0)])
    e.ellipse([(4, 0), (0, 4)])
    print("ellipse:", e.tolist())

    rect = ImagePath.Path([(0, 0)])
    rect.rectangle([(1, 2), (5, 6)])
    print("rectangle:", rect.tolist(), rect.getbbox())


if __name__ == "__main__":
    main()
