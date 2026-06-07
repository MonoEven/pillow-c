from __future__ import annotations

import json
import sys
from pathlib import Path

import PIL
from PIL import Image


def image_from_flat(mode: str, data: list[int]) -> Image.Image:
    bands = len(mode)
    if mode == "L":
        width = len(data)
    else:
        width = len(data) // bands
    return Image.frombytes(mode, (width, 1), bytes(data))


def main() -> int:
    cases = {
        "python": sys.version.split()[0],
        "pillow": PIL.__version__,
        "blend": [
            {
                "name": "L alpha 0.25",
                "mode": "L",
                "alpha": 0.25,
                "left": [0, 10, 128, 250],
                "right": [255, 100, 64, 0],
            },
            {
                "name": "L alpha 0.5",
                "mode": "L",
                "alpha": 0.5,
                "left": [0, 10, 128, 250],
                "right": [255, 100, 64, 0],
            },
            {
                "name": "RGB alpha 0.25",
                "mode": "RGB",
                "alpha": 0.25,
                "left": [10, 20, 30, 100, 110, 120],
                "right": [250, 240, 230, 1, 2, 3],
            },
            {
                "name": "RGB alpha 1.25 clips",
                "mode": "RGB",
                "alpha": 1.25,
                "left": [10, 20, 30, 100, 110, 120],
                "right": [250, 240, 230, 1, 2, 3],
            },
            {
                "name": "RGB negative alpha clips",
                "mode": "RGB",
                "alpha": -0.25,
                "left": [10, 20, 30],
                "right": [250, 240, 230],
            },
        ],
        "rgb_to_l": [
            {
                "name": "ITU-R 601-2 luma",
                "rgb": [
                    0,
                    0,
                    0,
                    255,
                    255,
                    255,
                    255,
                    0,
                    0,
                    0,
                    255,
                    0,
                    0,
                    0,
                    255,
                    10,
                    20,
                    30,
                    123,
                    45,
                    67,
                    250,
                    128,
                    1,
                ],
            }
        ],
        "alpha_composite_rgba": [
            {
                "name": "RGBA source over destination",
                "dst": [
                    0,
                    0,
                    0,
                    0,
                    10,
                    20,
                    30,
                    255,
                    100,
                    110,
                    120,
                    128,
                    200,
                    10,
                    30,
                    64,
                    1,
                    2,
                    3,
                    4,
                ],
                "src": [
                    255,
                    0,
                    0,
                    0,
                    200,
                    210,
                    220,
                    255,
                    50,
                    60,
                    70,
                    128,
                    10,
                    240,
                    30,
                    192,
                    250,
                    240,
                    230,
                    5,
                ],
            }
        ],
        "crop": [
            {
                "name": "RGB inside box",
                "mode": "RGB",
                "size": [3, 2],
                "box": [1, 0, 3, 2],
                "source": [
                    10,
                    20,
                    30,
                    40,
                    50,
                    60,
                    70,
                    80,
                    90,
                    100,
                    110,
                    120,
                    130,
                    140,
                    150,
                    160,
                    170,
                    180,
                ],
            },
            {
                "name": "RGB outside box zero fills",
                "mode": "RGB",
                "size": [3, 2],
                "box": [-1, -1, 2, 2],
                "source": [
                    10,
                    20,
                    30,
                    40,
                    50,
                    60,
                    70,
                    80,
                    90,
                    100,
                    110,
                    120,
                    130,
                    140,
                    150,
                    160,
                    170,
                    180,
                ],
            },
            {
                "name": "RGB zero width box",
                "mode": "RGB",
                "size": [3, 2],
                "box": [1, 1, 1, 2],
                "source": [
                    10,
                    20,
                    30,
                    40,
                    50,
                    60,
                    70,
                    80,
                    90,
                    100,
                    110,
                    120,
                    130,
                    140,
                    150,
                    160,
                    170,
                    180,
                ],
            },
        ],
        "paste": [
            {
                "name": "RGB inside point",
                "mode": "RGB",
                "target_size": [4, 3],
                "source_size": [3, 2],
                "box": [1, 1],
                "target": [10] * (4 * 3 * 3),
                "source": [
                    1,
                    2,
                    3,
                    4,
                    5,
                    6,
                    7,
                    8,
                    9,
                    11,
                    12,
                    13,
                    14,
                    15,
                    16,
                    17,
                    18,
                    19,
                ],
            },
            {
                "name": "RGB negative point clips",
                "mode": "RGB",
                "target_size": [4, 3],
                "source_size": [3, 2],
                "box": [-1, -1],
                "target": [10] * (4 * 3 * 3),
                "source": [
                    1,
                    2,
                    3,
                    4,
                    5,
                    6,
                    7,
                    8,
                    9,
                    11,
                    12,
                    13,
                    14,
                    15,
                    16,
                    17,
                    18,
                    19,
                ],
            },
            {
                "name": "RGB lower-right point clips",
                "mode": "RGB",
                "target_size": [4, 3],
                "source_size": [3, 2],
                "box": [3, 2],
                "target": [10] * (4 * 3 * 3),
                "source": [
                    1,
                    2,
                    3,
                    4,
                    5,
                    6,
                    7,
                    8,
                    9,
                    11,
                    12,
                    13,
                    14,
                    15,
                    16,
                    17,
                    18,
                    19,
                ],
            },
        ],
        "transpose": [
            {
                "name": "RGB all transpose methods",
                "mode": "RGB",
                "size": [3, 2],
                "source": [
                    10,
                    20,
                    30,
                    40,
                    50,
                    60,
                    70,
                    80,
                    90,
                    100,
                    110,
                    120,
                    130,
                    140,
                    150,
                    160,
                    170,
                    180,
                ],
                "methods": [
                    "FLIP_LEFT_RIGHT",
                    "FLIP_TOP_BOTTOM",
                    "ROTATE_90",
                    "ROTATE_180",
                    "ROTATE_270",
                    "TRANSPOSE",
                    "TRANSVERSE",
                ],
            },
            {
                "name": "RGB zero width rotate 90",
                "mode": "RGB",
                "size": [0, 1],
                "source": [],
                "methods": ["ROTATE_90"],
            }
        ],
    }

    for case in cases["blend"]:
        left = image_from_flat(case["mode"], case["left"])
        right = image_from_flat(case["mode"], case["right"])
        case["expected"] = list(Image.blend(left, right, case["alpha"]).tobytes())

    for case in cases["rgb_to_l"]:
        image = image_from_flat("RGB", case["rgb"])
        case["expected"] = list(image.convert("L").tobytes())

    for case in cases["alpha_composite_rgba"]:
        dst = image_from_flat("RGBA", case["dst"])
        src = image_from_flat("RGBA", case["src"])
        case["expected"] = list(Image.alpha_composite(dst, src).tobytes())

    for case in cases["crop"]:
        image = Image.frombytes(case["mode"], tuple(case["size"]), bytes(case["source"]))
        out = image.crop(tuple(case["box"]))
        case["expected_size"] = list(out.size)
        case["expected"] = list(out.tobytes())

    for case in cases["paste"]:
        target = Image.frombytes(case["mode"], tuple(case["target_size"]), bytes(case["target"]))
        source = Image.frombytes(case["mode"], tuple(case["source_size"]), bytes(case["source"]))
        target.paste(source, tuple(case["box"]))
        case["expected"] = list(target.tobytes())

    for case in cases["transpose"]:
        image = Image.frombytes(case["mode"], tuple(case["size"]), bytes(case["source"]))
        expected = {}
        for method_name in case["methods"]:
            method = getattr(Image.Transpose, method_name)
            out = image.transpose(method)
            expected[method_name] = {
                "method": int(method),
                "size": list(out.size),
                "bytes": list(out.tobytes()),
            }
        case["expected"] = expected

    out_path = Path(__file__).with_name("pillow_oracle.json")
    out_path.write_text(json.dumps(cases, indent=2) + "\n", encoding="utf-8")
    print(out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
