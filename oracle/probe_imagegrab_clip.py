"""One-shot oracle probe for API-GRAB-001 (clipboard semantics)."""

from PIL import Image, ImageGrab


def main():
    # Empty clipboard path.
    try:
        result = ImageGrab.grabclipboard()
        print("empty clipboard ->", result)
    except Exception as err:
        print("empty clipboard ERR", type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
