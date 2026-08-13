"""One-shot oracle probe for MODE-NUM-001CN.

Pillow 11.3.0 mode I/F/I;16 resize reducing_gap semantics.
"""

from PIL import Image

I_VALUES = [i * 37 % 4000 for i in range(64)]
F_VALUES = [((i * 13) % 100) / 7.0 - 3.0 for i in range(64)]
I16_VALUES = [(i * 977) % 60000 for i in range(64)]


def describe(label, im):
    print(label, "mode=%s size=%s data=%s" % (
        im.mode, im.size, list(im.getdata())))


def main():
    for name, mode, values in [("I", "I", I_VALUES), ("F", "F", F_VALUES), ("I;16", "I;16", I16_VALUES)]:
        im = Image.new(mode, (8, 8))
        im.putdata(values)
        for resample_name in ["NEAREST", "BILINEAR", "BICUBIC"]:
            resample = getattr(Image.Resampling, resample_name)
            for gap in [2.0, 4.0]:
                try:
                    out = im.resize((3, 3), resample=resample, reducing_gap=gap)
                    describe("RG %s %s gap=%s" % (name, resample_name, gap), out)
                except Exception as err:
                    print("RG %s %s gap=%s ERR" % (name, resample_name, gap),
                          type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
