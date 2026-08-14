"""BEHAV-FONT-002 oracle: ImageFont getmask + TransposedFont in Pillow 11.3.0."""
import io
import json

from PIL import Image, ImageFont

out = {}


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


font = ImageFont.load_default()

def mask_shape(text, mode=""):
    mask = font.getmask(text, mode)
    data = bytes(mask)
    return (mask.size, data.hex())


def transposed(text, orientation):
    tf = ImageFont.TransposedFont(font, orientation)
    mask = tf.getmask(text)
    data = bytes(mask)
    return (mask.size, data.hex())


def transposed_bbox(text, orientation):
    tf = ImageFont.TransposedFont(font, orientation)
    return tf.getbbox(text)


def transposed_length(text, orientation):
    tf = ImageFont.TransposedFont(font, orientation)
    return tf.getlength(text)


out["mask_a"] = capture(lambda: mask_shape("A"))
out["mask_ab"] = capture(lambda: mask_shape("AB"))
out["mask_1"] = capture(lambda: mask_shape("A", "1"))
out["mask_l"] = capture(lambda: mask_shape("A", "L"))
out["mask_bad_mode"] = capture(lambda: mask_shape("A", "XX"))
out["empty_text"] = capture(lambda: mask_shape(""))
out["mask_multi_line"] = capture(lambda: mask_shape("A\nB"))
out["trans_90"] = capture(lambda: transposed("AB", Image.Transpose.ROTATE_90))
out["trans_180"] = capture(lambda: transposed("AB", Image.Transpose.ROTATE_180))
out["trans_270"] = capture(lambda: transposed("AB", Image.Transpose.ROTATE_270))
out["trans_flr"] = capture(lambda: transposed("AB", Image.Transpose.FLIP_LEFT_RIGHT))
out["trans_ftb"] = capture(lambda: transposed("AB", Image.Transpose.FLIP_TOP_BOTTOM))
out["trans_none"] = capture(lambda: transposed("AB", None))
out["bbox_plain"] = capture(lambda: transposed_bbox("AB", None))
out["bbox_90"] = capture(lambda: transposed_bbox("AB", Image.Transpose.ROTATE_90))
out["bbox_270"] = capture(lambda: transposed_bbox("AB", Image.Transpose.ROTATE_270))
out["bbox_flr"] = capture(lambda: transposed_bbox("AB", Image.Transpose.FLIP_LEFT_RIGHT))
out["length_plain"] = capture(lambda: transposed_length("AB", None))
out["length_90"] = capture(lambda: transposed_length("AB", Image.Transpose.ROTATE_90))
out["length_270"] = capture(lambda: transposed_length("AB", Image.Transpose.ROTATE_270))

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_font_mask.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
