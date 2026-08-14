"""One-shot oracle probe for API-MATH-001 (comparisons and edge ops)."""

from PIL import Image, ImageMath


def main():
    a = Image.new("L", (2, 1))
    a.putdata([7, 9])
    b = Image.new("L", (2, 1))
    b.putdata([2, 12])
    f = Image.new("F", (2, 1))
    f.putdata([1.5, 2.5])

    for expr in ["a == b", "a < b", "a >= b", "f & a", "f << 1", "~a", "int(f)"]:
        try:
            out = ImageMath.unsafe_eval(expr, a=a, b=b, f=f)
            print(repr(expr), "->", getattr(out, "mode", type(out).__name__),
                  list(out.getdata()) if hasattr(out, "getdata") else out)
        except Exception as err:
            print(repr(expr), "ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
