"""API-READONLY-001 oracle probe: Pillow 11.3.0 Image.readonly semantics.

Records the fget/fset shape — readonly = (im and im.readonly) or _readonly,
the setter stores _readonly directly, the default is 0, frombuffer aliases
set the core flag, and setting readonly=False on a frombuffer image does NOT
clear the core flag.

Run: F:\\Python\\Python310\\python.exe oracle/probe_image_readonly.py
"""
import json

from PIL import Image


def main() -> None:
    report = {}
    im = Image.new("L", (2, 2))
    report["new_readonly"] = im.readonly
    report["new__readonly"] = im._readonly
    im.readonly = True
    report["after_true"] = [im.readonly, im._readonly]
    im.readonly = False
    report["after_false"] = [im.readonly, im._readonly]

    fb = Image.frombuffer("L", (4, 4), bytes(range(16)))
    report["frombuffer"] = [fb.readonly, fb._readonly, fb.im.readonly]
    fb.readonly = False
    report["frombuffer_set_false"] = [fb.readonly, fb._readonly, fb.im.readonly]
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
