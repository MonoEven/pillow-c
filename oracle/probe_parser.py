"""BEHAV-PARSER-001 oracle: ImageFile.Parser feed/close behaviors in Pillow 11.3.0."""
import io
import json

from PIL import Image, ImageFile

out = {}


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


def png_bytes():
    im = Image.new("RGB", (2, 2), (10, 20, 30))
    buf = io.BytesIO()
    im.save(buf, "PNG")
    return buf.getvalue()


png = png_bytes()


def feed_close(chunks, close_extra=None):
    p = ImageFile.Parser()
    for chunk in chunks:
        p.feed(chunk)
    im = p.close()
    result = (im.format, im.mode, list(im.size), im.tobytes().hex())
    if close_extra:
        im.close()
    return result


out["png_whole"] = capture(lambda: feed_close([png]))
out["png_pieces"] = capture(lambda: feed_close([png[:5], png[5:20], png[20:]]))
out["png_bytewise"] = capture(lambda: feed_close([png[i:i + 1] for i in range(len(png))]))
out["empty_close"] = capture(lambda: ImageFile.Parser().close())
out["garbage"] = capture(lambda: feed_close([b"\x00\x01\x02 not an image"]))
out["truncated_png"] = capture(lambda: feed_close([png[:30]]))
out["extra_garbage_after"] = capture(lambda: feed_close([png, b"\x00garbage"]))
# feed after close (finished): close() consumes the data; a later feed is ignored
def feed_after_close():
    p = ImageFile.Parser()
    p.feed(png)
    im1 = p.close()
    p.feed(b"\x00\x01")
    return (im1.format, p.close().format)
out["feed_after_close"] = capture(feed_after_close)
# reset semantics
def reset_fresh():
    p = ImageFile.Parser()
    p.reset()
    return True
out["reset_fresh"] = capture(reset_fresh)
def reset_after_feed():
    p = ImageFile.Parser()
    p.feed(png[:10])
    p.reset()
    return True
out["reset_after_feed"] = capture(reset_after_feed)
# context manager
def context_manager():
    with ImageFile.Parser() as p:
        p.feed(png)
    return p.close().format
out["context"] = capture(context_manager)
# empty feed then close with no data
def empty_feed():
    p = ImageFile.Parser()
    p.feed(b"")
    return p.close().format
out["empty_feed"] = capture(empty_feed)

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_parser.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
