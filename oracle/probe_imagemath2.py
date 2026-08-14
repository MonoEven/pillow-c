"""One-shot oracle probe for API-MATH-001 (rounding + unsafe_eval)."""

from PIL import Image, ImageMath


def main():
    a = Image.new("L", (2, 1))
    a.putdata([7, 9])
    b = Image.new("L", (2, 1))
    b.putdata([2, -2])
    f = Image.new("F", (2, 1))
    f.putdata([7.5, 9.5])
    i = Image.new("I", (2, 1))
    i.putdata([7, -9])

    for expr in ["a / b", "float(a) / 2", "f / 2", "i / 2", "i % 3", "a % b"]:
        try:
            out = ImageMath.unsafe_eval(expr, a=a, b=b, f=f, i=i)
            print(repr(expr), "->", out.mode, list(out.getdata()))
        except Exception as err:
            print(repr(expr), "ERR", type(err).__name__, "|", str(err))

    for expr in ["sin(0.5)", "log(2)", "pow(2, 3)", "e", "pi", "atan2(1, 1)",
                 "sqrt(9)", "exp(1)", "cos(0)"]:
        try:
            out = ImageMath.unsafe_eval(expr)
            print(repr(expr), "->", type(out).__name__, out)
        except Exception as err:
            print(repr(expr), "ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
