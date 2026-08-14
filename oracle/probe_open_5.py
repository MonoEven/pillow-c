"""BEHAV-OPEN-005 oracle: FLI/FLC open behaviors in Pillow 11.3.0."""
import io
import json
import struct

from PIL import Image


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


out = {}


def subchunk(chunk_type, payload):
    return struct.pack("<I", 6 + len(payload)) + struct.pack("<H", chunk_type) + payload


def palette_chunk(palette_entries, shift=0):
    payload = struct.pack("<h", len(palette_entries))
    for start, n, rgb in palette_entries:
        payload += bytes([start, n])
        for r, g, b in rgb:
            payload += bytes([r << shift & 0xFF, g << shift & 0xFF, b << shift & 0xFF])
    return subchunk(4, payload)


def fli(w, h, frames, magic=0xAF11, n_frames=None, palette=None, duration=70, prefix=None):
    header = struct.pack("<I", 0) + struct.pack("<H", magic)
    header += struct.pack("<H", n_frames if n_frames is not None else len(frames))
    header += struct.pack("<HH", w, h)
    header += struct.pack("<H", 0)  # padding
    header += struct.pack("<H", 0)  # flags at 14
    header += struct.pack("<I", duration)
    header += struct.pack("<HH", 0, 0)  # 20:22 zero
    header = header[:128]
    header += b"\x00" * (128 - len(header))
    assert len(header) == 128
    body = b""
    if prefix:
        body += struct.pack("<I", 6 + len(prefix)) + struct.pack("<H", 0xF100) + prefix
    if palette:
        # the palette is a COLOR subchunk inside the first frame
        frames = [palette] + frames
    # frame 0 chunk: F1FA (16-byte header: size, magic, count, 8 reserved)
    frame_payload = b"".join(frames)
    body += struct.pack("<I", 16 + len(frame_payload)) + struct.pack("<H", 0xF1FA) + struct.pack("<H", len(frames)) + b"\x00" * 8 + frame_payload
    return header + body


def open_bytes(blob, fmt):
    im = Image.open(io.BytesIO(blob), formats=[fmt])
    data = im.tobytes()
    pal = list(im.getpalette()) if im.mode == "P" else None
    info = {k: (v.hex() if isinstance(v, bytes) else str(v)) for k, v in getattr(im, "info", {}).items()}
    return (im.mode, list(im.size), data.hex(), pal, info)


# --- a 4x2 frame: BLACK then COPY of indices 0..7 ---
frames = [subchunk(13, b""), subchunk(16, bytes(range(8)))]
r = capture(lambda: open_bytes(fli(4, 2, frames, palette=palette_chunk([(0, 3, [(1, 2, 3), (4, 5, 6), (7, 8, 9)])])), "FLI"))
out["fli_copy"] = r
# BRUN frame: per row [packetcount byte + runs]; a fill run is
# [count][value] (count < 128); a literal run is [count|0x80][bytes]
brun = bytes([0x00, 0x02, 7, 0x02, 9]) + bytes([0x00, 0x02, 3, 0x02, 1])
out["fli_brun"] = capture(lambda: open_bytes(fli(4, 2, [subchunk(15, brun)], palette=palette_chunk([(0, 3, [(1, 2, 3), (4, 5, 6), (7, 8, 9)])])), "FLI"))
# LC frame: y=0, count=2; row0: 1 packet chunk 4 bytes; row1: 1 packet run 4
lc = struct.pack("<hh", 0, 2) + bytes([1, 0, 4, 10, 11, 12, 13]) + bytes([1, 0, 0xFC, 9])
out["fli_lc"] = capture(lambda: open_bytes(fli(4, 2, [subchunk(12, lc)], palette=palette_chunk([(0, 3, [(1, 2, 3), (4, 5, 6), (7, 8, 9)])])), "FLI"))
# SS2 frame: lines=2; per line 1 packet: skip 0, count 2 words (4 bytes)
ss2 = struct.pack("<h", 2) + struct.pack("<h", 1) + bytes([0, 2, 0, 1, 2, 3]) + struct.pack("<h", 1) + bytes([0, 2, 4, 5, 6, 7])
out["fli_ss2"] = capture(lambda: open_bytes(fli(4, 2, [subchunk(7, ss2)], palette=palette_chunk([(0, 3, [(1, 2, 3), (4, 5, 6), (7, 8, 9)])])), "FLI"))
# unknown chunk type (>= 10 bytes remain after the header, so the decoder
# reaches the default branch instead of the sub-10-byte overrun check)
out["fli_unknown_chunk"] = capture(lambda: open_bytes(fli(4, 2, [subchunk(99, b"\x00")], palette=palette_chunk([(0, 3, [(1, 2, 3), (4, 5, 6), (7, 8, 9)])])), "FLI"))
out["fli_unknown_chunk_room"] = capture(lambda: open_bytes(fli(4, 2, [subchunk(99, b"\x00\x01\x02\x03")], palette=palette_chunk([(0, 3, [(1, 2, 3), (4, 5, 6), (7, 8, 9)])])), "FLI"))
# FLC palette: a real COLOR_256 (type 11) chunk, values unshifted — the
# plugin applies the left-2 shift itself
pal_flc = struct.pack("<h", 1) + bytes([0, 2]) + bytes([16, 32, 48, 20, 40, 60])
out["fli_palette_flc"] = capture(lambda: open_bytes(fli(4, 2, [subchunk(16, bytes(range(8)))], palette=subchunk(11, pal_flc)), "FLI"))
# frame declares more bytes than the file holds -> truncated, N bytes not processed
out["fli_truncated"] = capture(lambda: open_bytes(fli(4, 2, [subchunk(16, bytes(range(8)))])[:-4], "FLI"))
# framesize > available without a COPY chunk: the decoder reports it needs
# more data and the driver hits EOF
out["fli_truncated_nocopy"] = capture(lambda: open_bytes(fli(4, 2, [subchunk(13, b"")], palette=palette_chunk([(0, 3, [(1, 2, 3), (4, 5, 6), (7, 8, 9)])]))[:-2], "FLI"))
# complete frame but the COPY chunk data runs past the frame end (the
# subchunk is padded so the decoder reaches the COPY branch instead of the
# sub-10-byte overrun check): it consumes up to the COPY chunk, then EOF
out["fli_copy_short"] = capture(lambda: open_bytes(fli(4, 2, [subchunk(16, bytes([1, 2]) + b"\x00\x00\x00\x00")], palette=palette_chunk([(0, 3, [(1, 2, 3), (4, 5, 6), (7, 8, 9)])])), "FLI"))
# a zero-size subchunk followed by enough bytes reaches the BROKEN branch
zero_chunk = struct.pack("<I", 0) + struct.pack("<H", 13)
out["fli_broken"] = capture(lambda: open_bytes(fli(4, 2, [zero_chunk, subchunk(13, b"")], palette=palette_chunk([(0, 3, [(1, 2, 3), (4, 5, 6), (7, 8, 9)])])), "FLI"))
# an F100 prefix chunk: the palette walk skips it, but the decode tile is
# pinned at offset 128, so the prefix chunk itself reaches the decoder
out["fli_prefix"] = capture(lambda: open_bytes(fli(4, 2, [subchunk(13, b"")], palette=palette_chunk([(0, 3, [(1, 2, 3), (4, 5, 6), (7, 8, 9)])]), prefix=b"\x01\x02"), "FLI"))
# errors
out["fli_bad_magic"] = capture(lambda: Image.open(io.BytesIO(b"\x00" * 64), formats=["FLI"]))
bad = bytearray(fli(4, 2, [subchunk(13, b"")]))
bad[20] = 1
out["fli_bad_flags"] = capture(lambda: Image.open(io.BytesIO(bytes(bad)), formats=["FLI"]))
out["fli_missing_frame_size"] = capture(lambda: Image.open(io.BytesIO(fli(4, 2, [])), formats=["FLI"]))
# n_frames and duration
r = capture(lambda: (lambda im: (im.format, getattr(im, "n_frames", None), getattr(im, "is_animated", None), im.info))(Image.open(io.BytesIO(fli(4, 2, [subchunk(13, b"")], n_frames=2)), formats=["FLI"])))
out["fli_meta"] = r
r = capture(lambda: (lambda im: im.info["duration"])(Image.open(io.BytesIO(fli(4, 2, [subchunk(13, b"")], duration=70)), formats=["FLI"])))
out["fli_duration"] = r
r = capture(lambda: (lambda im: im.info["duration"])(Image.open(io.BytesIO(fli(4, 2, [subchunk(13, b"")], magic=0xAF12, duration=140)), formats=["FLI"])))
out["fli_duration_flc"] = r

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_open_5.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
