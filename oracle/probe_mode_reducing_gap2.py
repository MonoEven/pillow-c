"""One-shot oracle probe for MODE-NUM-001CN (factor>1 cases).

Pillow 11.3.0 mode I/F/I;16 resize reducing_gap with an actual reduce step.
"""

from PIL import Image

I_VALUES = [i * 37 % 4000 for i in range(24 * 24)]
F_VALUES = [((i * 13) % 100) / 7.0 - 3.0 for i in range(24 * 24)]
I16_VALUES = [(i * 977) % 60000 for i in range(24 * 24)]


def describe(label, im):
    print(label, "mode=%s size=%s data=%s" % (
        im.mode, im.size, list(im.getdata())))


def main():
    for name, mode, values in [("I", "I", I_VALUES), ("F", "F", F_VALUES), ("I;16", "I;16", I16_VALUES)]:
        im = Image.new(mode, (24, 24))
        im.putdata(values)
        for resample_name in ["NEAREST", "BILINEAR", "BICUBIC"]:
            resample = getattr(Image.Resampling, resample_name)
            try:
                out = im.resize((3, 3), resample=resample, reducing_gap=2.0)
                describe("RG %s %s gap=2" % (name, resample_name), out)
            except Exception as err:
                print("RG %s %s gap=2 ERR" % (name, resample_name),
                      type(err).__name__, "|", str(err))

        # reduce() directly on I;16 to capture the ModeError boundary.
        try:
            im.reduce(2)
            print("REDUCE %s ACCEPTED" % name)
        except Exception as err:
            print("REDUCE %s ERR" % name, type(err).__name__, "|", str(err))


if __name__ == "__main__":
    main()
