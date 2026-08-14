"""BEHAV-OPEN-007 oracle: PCD (Kodak PhotoCD) open behaviors in Pillow 11.3.0."""
import hashlib
import io
import json

from PIL import Image

out = {}


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


def ycc_chunk(c):
    # 2 rows of Y (768 each) + subsampled Cb/Cr (384 each) = 2304 bytes
    chunk = bytearray(2304)
    for i in range(768):
        chunk[i] = (c * 3 + i) % 256            # row0 Y
        chunk[768 + i] = (c * 5 + i + 64) % 256  # row1 Y
    for i in range(384):
        chunk[1536 + i] = (c * 7 + i // 2) % 256   # Cb
        chunk[1920 + i] = (c * 11 + i // 3) % 256  # Cr
    return bytes(chunk)


def pcd_blob(ycc_data, orientation=0, header_patch=None):
    # sector 0 = filler, sector 1 = the PCD header (magic at its start,
    # orientation byte at offset 1538), sectors 2..95 = filler, then the
    # image data at 96*2048
    header = bytearray(2048)
    header[0:4] = b"PCD_"
    header[1538] = orientation & 3
    if header_patch:
        header_patch(header)
    body = b"\x00" * 2048 + bytes(header)
    body += b"\x00" * (96 * 2048 - len(body))
    return body + ycc_data


full_ycc = b"".join(ycc_chunk(c) for c in range(256))


def open_pcd(blob):
    im = Image.open(io.BytesIO(blob), formats=["PCD"])
    data = im.tobytes()
    rows = []
    row_len = im.size[0] * 3
    for r in [0, 1, 2, im.size[1] - 3, im.size[1] - 2, im.size[1] - 1]:
        rows.append(data[r * row_len:(r + 1) * row_len].hex())
    return (
        im.format,
        im.mode,
        list(im.size),
        hashlib.md5(data).hexdigest(),
        rows,
        getattr(im, "n_frames", None),
        getattr(im, "is_animated", None),
    )


out["pcd_base"] = capture(lambda: open_pcd(pcd_blob(full_ycc, 0)))
out["pcd_orient1"] = capture(lambda: open_pcd(pcd_blob(full_ycc, 1)))
out["pcd_orient2"] = capture(lambda: open_pcd(pcd_blob(full_ycc, 2)))
out["pcd_orient3"] = capture(lambda: open_pcd(pcd_blob(full_ycc, 3)))
out["pcd_bad_magic"] = capture(lambda: Image.open(io.BytesIO(pcd_blob(full_ycc, 0, lambda h: h.__setitem__(slice(0, 4), b"NOPE"))), formats=["PCD"]))
out["pcd_short_header"] = capture(lambda: Image.open(io.BytesIO(b"\x00" * 2048 + b"PCD_"), formats=["PCD"]))
out["pcd_save"] = capture(lambda: (lambda im: im.save(io.BytesIO(), "PCD"))(Image.new("L", (1, 1))))
# truncations: cut the data at several offsets
for cut in [0, 1, 100, 2303, 2304, 2305, 4607, 4608, 10000]:
    out[f"pcd_trunc_{cut}"] = capture(
        lambda cut=cut: open_pcd(pcd_blob(full_ycc[: 256 * 2304 - cut] if cut else b""))
    )
# a file whose data is fine but shorter than the full 256 chunks
out["pcd_half_data"] = capture(lambda: open_pcd(pcd_blob(full_ycc[: 128 * 2304])))

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_open_7.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
