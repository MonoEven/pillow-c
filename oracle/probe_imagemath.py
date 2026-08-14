"""One-shot oracle probe for API-MATH-001.

Pillow 11.3.0 ImageMath.eval / unsafe_eval semantics.
"""

from PIL import Image, ImageMath


def main():
    a = Image.new("L", (2, 2))
    a.putdata([10, 20, 30, 40])
    b = Image.new("L", (2, 2))
    b.putdata([2, 5, 3, 4])
    rgb = Image.new("RGB", (2, 2))
    rgb.putdata([(10, 20, 30), (40, 50, 60), (70, 80, 90), (100, 110, 120)])

    # Binary ops on L.
    for expr in ["a + b", "a - b", "a * b", "a / b", "a & b", "a | b", "a ^ b",
                 "min(a, b)", "max(a, b)", "abs(a - b)", "a >> 1", "a << 2",
                 "float(a) * b", "a + 1", "a * 3", "-a", "convert(a, 'F')"]:
        try:
            out = ImageMath.eval(expr, a=a, b=b)
            print(repr(expr), "->", out.mode, list(out.getdata())[:4],
                  "raw=", (out.tobytes()[:12]).hex(" "))
        except Exception as err:
            print(repr(expr), "ERR", type(err).__name__, "|", str(err))

    # Mode rules.
    for expr in ["a + rgb", "rgb + a", "a + float(b)"]:
        try:
            out = ImageMath.eval(expr, a=a, rgb=rgb, b=b)
            print(repr(expr), "->", out.mode, list(out.getdata())[:2])
        except Exception as err:
            print(repr(expr), "ERR", type(err).__name__, "|", str(err))

    # Constants and functions.
    for expr in ["e", "pi", "sin(pi/2)", "log(e)", "pow(a, 2)"]:
        try:
            out = ImageMath.eval(expr, a=a)
            print(repr(expr), "->", type(out).__name__,
                  out if not hasattr(out, "mode") else out.mode,
                  (list(out.getdata())[:4] if hasattr(out, "getdata") else ""))
        except Exception as err:
            print(repr(expr), "ERR", type(err).__name__, "|", str(err))

    # unsafe_eval mask variables.
    mask = Image.new("L", (2, 2))
    mask.putdata([255, 0, 255, 0])
    try:
        out = ImageMath.unsafe_eval("a + 5", a=a)
        print("unsafe_eval(a+5) ->", out.mode, list(out.getdata())[:4])
    except Exception as err:
        print("unsafe_eval ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
