"""One-shot oracle probe for MODE-NUM-001CH (math isolation).

Derive Pillow 11.3.0 mode I/F transform resampling math with 1-D cases.
"""

import struct

from PIL import Image


def run(label, mode, values, size, matrix, out_size, resample):
    im = Image.new(mode, size)
    im.putdata(values)
    out = im.transform(out_size, Image.AFFINE, matrix,
                       resample=resample)
    print(label, "mode=%s" % out.mode, "data=%s" % list(out.getdata()),
          "raw=%s" % out.tobytes().hex(" "))


def main():
    f_row = [1.0, 2.0, 4.0, 8.0, 16.0]
    f_col = [1.0, 3.0, 9.0, 27.0, 81.0]
    i_row = [100, 200, 400, 800, 1600]
    i_col = [100, 300, 900, 2700, 8100]

    # Horizontal-only 2x upscale: output 10x1.
    for mode, values in [("F", f_row), ("I", i_row)]:
        for res in [Image.Resampling.BILINEAR, Image.Resampling.BICUBIC]:
            run("H %s %s" % (mode, res.name), mode, values, (5, 1),
                (0.5, 0.0, 0.0, 0.0, 1.0, 0.0), (10, 1), res)

    # Vertical-only 2x upscale: output 1x10.
    for mode, values in [("F", f_col), ("I", i_col)]:
        for res in [Image.Resampling.BILINEAR, Image.Resampling.BICUBIC]:
            run("V %s %s" % (mode, res.name), mode, values, (1, 5),
                (1.0, 0.0, 0.0, 0.0, 0.5, 0.0), (1, 10), res)

    # Fractional offset (0.25) horizontal with identity scale: 6x1 -> 4x1.
    for mode, values in [("F", f_row), ("I", i_row)]:
        for res in [Image.Resampling.BILINEAR, Image.Resampling.BICUBIC]:
            run("O %s %s" % (mode, res.name), mode, values, (5, 1),
                (1.0, 0.0, 0.25, 0.0, 1.0, 0.0), (4, 1), res)


if __name__ == "__main__":
    main()
