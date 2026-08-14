"""One-shot oracle probe for API-PATH-001 (method set + semantics)."""

from PIL import ImagePath


def main():
    p = ImagePath.Path([(1, 1), (5, 5)])
    for name in ["line", "curve", "arc", "ellipse", "rectangle", "close",
                 "move", "compact", "getbbox", "tolist", "transform", "map"]:
        print(name, hasattr(p, name))

    q = ImagePath.Path([(0, 0), (10, 0), (10, 10), (0, 10)])
    q.line([(0, 0)])
    print("close-via-line tolist:", q.tolist())

    r = ImagePath.Path([(0, 0)])
    r.line([(10, 10)], relative=True)
    r.line([(20, 20)])
    print("line rel/abs:", r.tolist())

    c = ImagePath.Path([(0, 0)])
    c.curve([(10, 10), (20, 10), (30, 0)])
    print("curve:", c.tolist(), c.tolist(False))
    print("compact(0.5):", c.compact(0.5).tolist())
    print("compact(0):", c.compact(0).tolist())

    t = ImagePath.Path([(0, 0), (10, 0)])
    t.transform((2.0, 0.0, 5.0, 0.0, 2.0, 5.0))
    print("transform:", t.tolist())

    a = ImagePath.Path([(0, 0)])
    a.arc([(10, 10)], 0, 90)
    print("arc(0..90):", a.tolist(), a.tolist(False))

    e = ImagePath.Path([(0, 0)])
    e.ellipse([(4, 0), (0, 4)])
    print("ellipse:", e.tolist())

    rect = ImagePath.Path([(0, 0)])
    rect.rectangle([(1, 2), (5, 6)])
    print("rectangle:", rect.tolist(), rect.getbbox())

    m = ImagePath.Path([(0, 0), (10, 10)])
    print("map:", m.map((1.0, 0.0, 1.0, 0.0, 1.0, 2.0)))


if __name__ == "__main__":
    main()
