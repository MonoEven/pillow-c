# MM BigTIFF Tiled Extension Design

## Status and intent

`FMT-TIFF-003AF` is an explicit standards extension, not a Pillow 11.3.0
positive-parity claim. Pillow 11.3.0 accepts the `MM 00 2B` prefix in
`PREFIXES`, but both `TiffImageFile._open()` and
`ImageFileDirectory_v2.__init__()` detect BigTIFF with `ifh[2] == 43`.
Consequently, Pillow interprets the MM offset-size field as an ordinary TIFF
IFD pointer (`524288`) and rejects even its own raw `I;16B, big_tiff=True`
output. Compressed `I;16B` saves are emitted as ordinary little-endian TIFF.

The runtime will support valid MM BigTIFF tiled files as a documented superset
without raising the Pillow-compatibility percentage. The behavior follows the
TIFF BigTIFF byte-order rules and Pillow's existing `OPEN_INFO` mode mapping.

## Native design

One header decoder recognizes `II 2B 00` and `MM 00 2B`, validates offset size
8 and reserved value 0, and returns the byte order. The existing BigTIFF IFD
locator, tiled parser, and frame counter all use that result for 16/32/64-bit
fields; no parallel MM parser is introduced.

Eight-bit chunky and planar samples remain byte-identical. MM unsigned 16-bit
grayscale maps to public/native `I;16B` and retains big-endian sample bytes.
MM signed 32-bit `I` and IEEE float32 `F` map to the existing native `I` and
`F` modes, so every decoded sample is converted from file big-endian order to
the DLL's little-endian in-memory representation after raw/PackBits/LZW/zlib
tile decoding. CMYK remains byte-identical.

Malformed headers, offsets, counts, tile ranges, and unsupported mode shapes
continue to return `PILLOW_C_INVALID_ARGUMENT`; there is no WIC fallback,
silent degradation, AHK pixel loop, or new public ABI.

## Verification design

The raw and facade tests extend existing fixture builders with an endian
parameter and register one matrix test each. Together they cover chunky byte
modes, planar byte modes, numeric modes, CMYK, raw/PackBits/LZW/Adobe Deflate,
mode/size/frame-count, and exact public bytes. Existing little-endian tests
remain unchanged call sites and prove that the generalized implementation does
not regress.

Release x64 is rebuilt. Raw and facade targets run serially, followed by TIFF
filters, the full AHK directory suite with 240 seconds, source/DLL export
parity, and DLL SHA-256. The checkpoint, detailed ledger, ABI, and testing
documents record both the Pillow rejection evidence and extension results.

