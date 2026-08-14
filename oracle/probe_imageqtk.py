"""One-shot oracle probe for API-QTTK-001.

Pillow 11.3.0 ImageQt / ImageTk behavior without the toolkits.
"""

from PIL import Image


def main():
    # ImageQt import path.
    try:
        import PIL.ImageQt as ImageQt
        print("ImageQt import OK, names:", sorted(n for n in dir(ImageQt) if not n.startswith("_"))[:8])
        im = Image.new("L", (2, 2))
        try:
            qim = ImageQt.ImageQt(im)
            print("ImageQt(im) ->", type(qim).__name__)
        except Exception as err:
            print("ImageQt(im) ERR", type(err).__name__, "|", str(err))
    except Exception as err:
        print("ImageQt import ERR", type(err).__name__, "|", str(err))

    # ImageTk import path.
    try:
        import PIL.ImageTk as ImageTk
        print("ImageTk import OK, names:", sorted(n for n in dir(ImageTk) if not n.startswith("_"))[:8])
        im = Image.new("L", (2, 2))
        try:
            photo = ImageTk.PhotoImage(im)
            print("PhotoImage(im) ->", type(photo).__name__)
        except Exception as err:
            print("PhotoImage(im) ERR", type(err).__name__, "|", str(err))
    except Exception as err:
        print("ImageTk import ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
