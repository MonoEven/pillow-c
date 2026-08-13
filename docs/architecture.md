# Architecture

`pillow_c.dll` is the native imaging core for the Pillow-to-AHK project.

The design goal is not "C code behind an AHK wrapper". The DLL should own the hot path: image allocation, pixel buffers, metadata, and whole-image or region operations. AHK should present the Python-like API while avoiding per-pixel loops and repeated fine-grained `DllCall` crossings.

## Behavior Authority

Behavior is constrained by:

```text
Python: F:\Python\Python310\python.exe
Pillow: 11.3.0
```

When behavior is unclear:

1. Query local Python/Pillow with small fixtures.
2. Inspect local installed Pillow sources where available.
3. Inspect Pillow 11.3.0 source for the native algorithm.
4. Lock the behavior with `ahktest` before optimizing.

Do not infer behavior from an existing AHK implementation.

## Native Dependencies

The project uses Windows Imaging Component for selected codec paths and
statically compiles official LittleCMS 2.17 for Pillow-compatible built-in
sRGB/LAB color conversion. LittleCMS source, headers, provenance, and license
live under `third_party/lcms2`; `pillow_c.dll` has no external LittleCMS runtime
dependency. Vendored C translation units compile without project-specific
source edits, while `src/pillow_c_cms.cpp` owns profile/transform lifetime and
the image-handle routing boundary.

## Current Native Module Ownership

As of `ARCH-MOD-012` (2026-08-06), the operations family is physically
decomposed beyond its first extraction. `pillow_c_ops.cpp` retains arithmetic,
conversion, palette/compositing, ImageChops, point/LUT, quantization, and
spatial implementations, while `pillow_c_ops_statistics.cpp` owns histogram,
entropy, extrema, bbox, projection, getcolors, and autocontrast algorithms plus
all eleven corresponding public exports. Their private seam is
`pillow_c_ops_internal.h`; it exposes only LUT application/support and the
histogram/autocontrast calls still consumed by equalize. No implementation is
included across translation units, no export forwarding shell was added, and
all hot loops remain native. The same operations module now also owns the
additive `pillow_c_image_quantize_options` route for bounded
`MEDIANCUT`/`MAXCOVERAGE`/`FASTOCTREE` and k-means refinement. The units are
6,960 and 1,079 lines; the private seam is 37 lines. Release x64 has
`0 Warning(s), 0 Error(s)`; structural operations are `13/13`; the quantize
raw/facade targeted pair is `2/2`; the full AHK suite is `2630/2630` in
`29906ms`; source/DLL exports are `446/446`; and DLL SHA-256 is
`FE00E99AE5524552D45AC49824C9EA413AA63A88BB2F8DB447F08AC38A55BF56`.
The architecture ownership remains native-first; this bounded compatibility
slice raises the replacement-readiness estimate to `60% ±4%` while
reference-palette dither and libimagequant parity remain open.

JPEG remains physically split across seven
independently compiled Modules rather than being represented by one large
translation unit:

- `pillow_c_codec_jpeg_decode.cpp`: WIC/libjpeg decode, draft-mode routing, and
  native JPEG open exports.
- `pillow_c_codec_jpeg_common.cpp`: marker, DCT, quantization, Huffman,
  progressive-scan, restart-marker, and entropy primitives shared by encoders.
- `pillow_c_codec_jpeg_encode_l.cpp`: grayscale baseline/progressive encoders.
- `pillow_c_codec_jpeg_encode_rgb.cpp`: RGB, qtables, keep-rgb, and sampled
  baseline/progressive encoders.
- `pillow_c_codec_jpeg_encode_cmyk.cpp`: CMYK/YCCK baseline and progressive
  block preparation and entropy encoders.
- `pillow_c_codec_jpeg_save.cpp`: public save-option routing, WIC baseline
  save path, restart-marker dispatch, and metadata composition dispatch.
- `pillow_c_codec_jpeg_metadata.cpp`: JPEG metadata ABI exports.

`pillow_c_codec_jpeg_internal.h` is the private C++ seam shared by those
Modules. `src/pillow_c.cpp` contains only the shared internal include and no
implementation body or forwarding export shell. The project file lists every
JPEG Module explicitly, so a missing implementation cannot be hidden behind
the include-only translation unit. The current JPEG Module sizes are 38,996,
57,238, 14,095, 89,078, 41,171, 44,786, and 5,088 bytes respectively; the
private seam header is 13,266 bytes. The JPEG structural ownership test checks
source ownership and project membership, while raw JPEG is `212/212` and
facade JPEG is `219/219` after the split. Release x64 has `0 Warning(s),
0 Error(s)`; source/DLL export parity is `445/445` with zero difference; the
full AHK suite is `2627/2627` in `34375ms`; and the current DLL SHA-256 is
`988DAA0F12507201F4AF8B01C889703FAD69614839868A71E3C6DB9ABD670462`.
This architecture packet does not increase the `59% ±4%` compatibility
estimate.

Architecture wave `ARCH-MOD-002`, 2026-08-06: the initial JPEG extraction
packet moved the complete JPEG hot path out of the original monolith into a
single independent translation unit. That packet was later deepened by
`ARCH-MOD-011` into the seven-module ownership listed above; the old
single-unit file is no longer part of the Release project. This is historical
architecture evidence, not an assertion about the current source layout.

Architecture wave `ARCH-MOD-003`, 2026-08-06: the complete PNG codec is now a
real independent translation unit, `src/pillow_c_codec_png.cpp`. It owns PNG
header/chunk parsing, text/ICC/EXIF/XMP/gamma/chromaticity/transparency
metadata, zlib inflate/stored-deflate helpers shared with TIFF, WIC decode,
native PNG chunk encoding, every PNG save-options path, and every PNG public
export. ICO save remained in the main unit as a container concern and reused
PNG palette-mode/custom-encoder behavior through two explicit internal seams;
GIF also remained in the main unit at this wave and later moved under
`ARCH-MOD-006`. `pillow_c.cpp`
is about 30,895 lines and the PNG unit about 6,482 lines. Release x64 builds
with `0 Warning(s), 0 Error(s)`; PNG raw/facade tests pass `221/221`; the full
AHK suite passes `2616/2616` in `18016ms`; source/DLL export parity is
`445/445` with zero difference; and the rebuilt DLL SHA-256 is
`6236EE06518E445F2830D81D4F4D8C4F2F148FDACE05A1A3ECF8B1BCFFF1BCEB`.
This is architecture ownership evidence, not additional Pillow compatibility
percentage.

Architecture wave `ARCH-MOD-004`, 2026-08-06: the complete ImageDraw and
default-font implementation is now a real independent translation unit,
`src/pillow_c_draw.cpp`. It owns flood-fill, shape clipping, bitmap-mask alpha
composition, polygon/line/ellipse/arc/chord/pieslice/rounded-rectangle raster
loops, default-font glyph data, text layout, font handles, and all draw/text/
font exports. The main unit contains no duplicate draw/font implementation or
forwarding export shell. The draw unit is 6,310 lines and the current main unit
is 15,261 lines. Raw draw/default-font tests pass `35/35` and `6/6`; facade
ImageDraw/ImageFont tests pass `57/57` and `4/4`; the public ABI, status codes,
handle ownership, pointer lifetime, and no-AHK-pixel-loop rule are unchanged.
This is architecture ownership evidence, not additional Pillow compatibility
percentage.

Architecture wave `ARCH-MOD-005`, 2026-08-06: the complete legacy codec group
is now a real independent translation unit, `src/pillow_c_codec_legacy.cpp`.
It owns BMP, PPM/Netpbm, QOI, TGA, XBM, and ICO parsing/writing, ICO directory
selection and DIB/PNG payload composition, and all corresponding public
exports. ICO reuses the explicit PNG custom-mode/encoder seams and native
resize seam; no PNG body is copied and no AHK pixel loop was introduced. The
legacy unit is 3,237 lines; the main unit retains no duplicate legacy codec
bodies or exports. The new Mode-1 size, LE int32, and int32-to-uint16 helpers
are internal seams only. Targeted raw/facade filters pass BMP `8/8`, PPM
`11/11`, QOI `5/5`, TGA `11/11`, XBM `9/9`, and ICO `24/24`. The current
Release build has `0 Warning(s), 0 Error(s)`, source/DLL exports are `445/445`
with zero difference, the full suite passes `2621/2621` in `27734ms`, and the
DLL SHA-256 is
`5FE477FD9D8F45473010D908D87B6903D9A2C931807AD4578A7F818454D364AF`.
This is architecture ownership evidence, not additional Pillow compatibility
percentage.

Architecture wave `ARCH-MOD-006`, 2026-08-06: the complete GIF codec is now a
real independent translation unit, `src/pillow_c_codec_gif.cpp`. It owns GIF
byte parsing, sub-blocks, LZW decode/encode, local/global palettes, frame
composition, disposal and transparency state, comments/loop metadata, indexed
single-frame writes, optimized animation rectangles, and every GIF public
export. Shared RGB/L and GIF palette quantization is reused through explicit
internal seams because it also serves Image.quantize and belongs to the
forthcoming operations unit. No GIF body or forwarding export shell remains
in `pillow_c.cpp`. The GIF unit is 2,836 lines and the main unit is 12,482
lines. Raw/facade GIF filters pass `52/52` and `51/51`; the full suite passes
`2622/2622` in `28485ms`; Release x64 has zero warnings/errors; exports remain
`445/445`; and DLL SHA-256 is
`9B84DFA634A14EAEC4ABD3607446C05DFC8D878BB8EE8B1BD46880695FB566FD`.
This is architecture ownership evidence, not additional Pillow compatibility
percentage.

Architecture wave `ARCH-MOD-007`, 2026-08-06: the operations family is now a
real independent translation unit, `src/pillow_c_ops.cpp`. It owns whole-image
arithmetic and conversion, palette/compositing, ImageChops, point/ImageOps,
statistics, crop/paste/transpose, quantization, and the public fill/getpixel/
putpixel exports. Shared GIF quantization crosses the seam through explicit
internal declarations; mode-name ownership remains in the main ABI/core unit.
The ops unit is 7,993 lines and `src/pillow_c.cpp` is 4,606 lines. The main
unit contains no duplicate operations bodies or forwarding export shells. Raw
targeted operations pass fill `2/2`, get/putpixel `2/2`, paste `13/13`,
transpose `12/12`, point LUT `6/6`, and handle `3/3`; facade fill passes `1/1`
and get/putpixel `2/2`. Release x64 has `0 Warning(s), 0 Error(s)`; the full
suite passes `2623/2623` in `29453ms`; source/DLL exports remain `445/445`;
and the rebuilt DLL SHA-256 is
`50C0FCB6CCABBA75098C5CB90732F540624EBD4BD562F24EE852E18D4E900EBE`.
This is architecture ownership evidence, not additional Pillow compatibility
percentage.

Architecture wave `ARCH-MOD-008`, 2026-08-06: metadata ownership is now a real
independent translation unit, `src/pillow_c_metadata.cpp`. It owns EXIF
orientation and typed-entry parsing, all EXIF serializers and public exports,
generic resolution/hotspot/DIB metadata accessors, PNG gamma/sRGB/
chromaticity/text/ICC/EXIF/XMP/transparency accessors, and the shared metadata
blob copy seam. The EXIF helper and metadata ABI blocks were physically moved
out of `pillow_c.cpp`; the main unit contains no duplicate implementation or
forwarding export shell. The metadata unit is 3,218 lines and the main unit is
2,127 lines. Structural ownership is `1/1`; raw EXIF is `23/23`; raw PNG
metadata is `12/12`; the combined raw metadata filter is `182/182`; facade PNG
metadata is `12/12`; facade JPEG metadata/open is `25/25`; and the full suite
is `2624/2624` in `28796ms`. Release x64 has `0 Warning(s), 0 Error(s)`;
source/DLL exports remain `445/445` with zero difference; and the rebuilt DLL
SHA-256 is
`BF103E627C2DAD72C8781AD42A289818F1074F4FC53D915555F4CBC72BEBE31D`.
This is architecture ownership evidence, not additional Pillow compatibility
percentage.

Architecture wave `ARCH-MOD-009`, 2026-08-06: raw codec and buffer ownership is
now a real independent translation unit, `src/pillow_c_raw.cpp`. It owns raw
mode decode/encode specifications, native raw pixel packing/unpacking, Mode-1
bit packing, raw byte sizing, FromBuffer source alias refresh/detach lifetime,
byte transfer implementation, and all eight raw/buffer public exports. The
existing codec, ops, draw, filters, and CMS Modules consume the refresh/detach,
Mode-1 size, and raw numeric seams through `pillow_c_internal.h`; no public
Interface, status code, image ownership, source-pointer lifetime, or AHK facade
route changed. `pillow_c.cpp` fell from 2,127 to 956 lines and
`pillow_c_raw.cpp` is 1,189 lines. The main unit contains no duplicate raw
Implementation or forwarding export shell. Structural ownership is `1/1`; raw
bytes `3/3`; raw numeric/Mode-1 `8/8`; raw FromBuffer map/alias `4/4`; raw
readonly refresh/detach `41/41`; facade Image.FromBuffer `32/32`; facade
FromBytes `5/5`; facade ToBytes `1/1`; Release x64 has `0 Warning(s), 0
Error(s)`; full AHK passes `2625/2625` in `28422ms`; source/DLL exports remain
`445/445` with zero difference; and the rebuilt DLL SHA-256 is
`0756B2D899800BA9E0A81B95C220E9E712F694F44C99280B2B88F2BFCF49A269`. This is
architecture ownership evidence, not additional Pillow compatibility
percentage.

Architecture wave `ARCH-MOD-010`, 2026-08-06: the remaining monolith is closed
by deepening the existing `src/pillow_c_core.cpp` Module with shared numeric,
shape/mask/palette, mode-name, mode-string, and mode ABI Implementation, and
adding `src/pillow_c_effects.cpp` for native linear/radial gradient and
Mandelbrot/noise/spread effect loops plus all corresponding public exports.
`pillow_c.cpp` is now a one-line include-only translation unit; it has no
Implementation body or forwarding export shell. The public Interface, status
codes, image-handle ownership, source-pointer lifetime, and internal Seam
contracts are unchanged. Core is 679 lines and effects is 371 lines. Structural
ownership is `1/1`; raw linear/radial/effects are `1/1`, `1/1`, and `3/3`;
facade linear/radial/effects are `1/1`, `1/1`, and `4/4`; Release x64 has
`0 Warning(s), 0 Error(s)`; full AHK passes `2626/2626` in `28843ms`;
source/DLL exports remain `445/445` with zero difference; and the rebuilt DLL
SHA-256 is
`69FF7A140E8EA0E3AA5E1B75BF394D1ADA8C9B5271709242A89497C1D23DF484`. This is
architecture ownership evidence, not additional Pillow compatibility
percentage.

Architecture wave `ARCH-MOD-011`, 2026-08-06: the JPEG monolith extraction is
now physically decomposed into seven independently compiled Modules connected
by `src/pillow_c_codec_jpeg_internal.h`. Decode, shared codec mathematics and
marker machinery, grayscale encode, RGB encode, CMYK encode, save routing, and
metadata exports each have separate source ownership. The public ABI remains
unchanged; `pillow_c.cpp` remains a one-line include-only translation unit;
there is no duplicated JPEG body and no forwarding export shell. The native
seam uses explicit `optimize == 1` normalization where an integer public
option crosses into a boolean encoder implementation, preserving Pillow's
default baseline Huffman route after the split. Structural ownership is
`1/1`; raw JPEG is `212/212`; facade JPEG is `219/219`; Release x64 is
`0 Warning(s), 0 Error(s)`; the full AHK suite is `2627/2627` in `34375ms`;
source/DLL export parity is `445/445` with zero difference; and the rebuilt
DLL SHA-256 is
`988DAA0F12507201F4AF8B01C889703FAD69614839868A71E3C6DB9ABD670462`. This
architecture packet does not increase the `59% ±4%` compatibility estimate.

`FMT-JPEG-003BH` extends requested-mode draft ownership to YCbCr on a bounded
4:2:2 RGB JPEG. WIC returns reduced Y at output resolution and Cb/Cr at half
horizontal resolution. The DLL applies libjpeg-turbo's h2v1 3/4-1/4 triangle
filter with its exact alternating rounding and interleaves Y/Cb/Cr in one
native route. The facade only normalizes the mode and transfers image/
metadata lifetime. No RGB conversion, resize, fallback, or AHK pixel loop is
used.

`FMT-JPEG-003BI` extends the same requested-mode route to a bounded RGB 4:2:0
JPEG. WIC supplies full-size reduced Y/Cb/Cr planes for the covered target,
so the DLL performs one contiguous native interleave and keeps all image bytes
inside the handle. The facade only owns mode normalization and handle/
metadata lifetime; there is no RGB conversion, resize, fallback, or AHK
pixel loop.

`FMT-JPEG-003BJ` verifies that the same decoder-native requested-mode route
selects scale 4 for the bounded 4:2:0 input and exposes exact `12x8` YCbCr
bytes. No production route or ABI change was needed; the facade continues to
transfer the DLL-owned handle and metadata lifetime without pixel loops.

`FMT-JPEG-003BK` verifies that the same decoder-native requested-mode route
selects scale 8 for the bounded 4:2:0 input and exposes exact `6x4` YCbCr
bytes. No production route or ABI change was needed; the facade continues to
transfer the DLL-owned handle and metadata lifetime without pixel loops.

`FMT-JPEG-003BL` verifies the same requested-mode route at decoder scale 4 on
the stable RGB 4:2:2 fixture and exposes exact `12x8` YCbCr bytes through the
existing native h2v1 path. The facade remains responsible only for mode
normalization, handle transfer, metadata lifetime, and Pillow's second-call
no-op semantics; no conversion, resize, fallback, or AHK pixel loop is used.

`FMT-JPEG-003BM` extends that route to decoder scale 8. WIC supplies reduced
`6x4 / 3x4 / 3x4` Y/Cb/Cr planes, and Pillow duplicates each chroma sample
across two horizontal output pixels instead of applying the fancy filter used
at scales 2/4. The DLL owns this nearest interleave and exposes one contiguous
YCbCr image handle; the facade route and lifetime model remain unchanged.

`FMT-JPEG-003BN` verifies the same requested-mode route at decoder scale 1 on
the stable RGB 4:2:2 fixture. The existing DLL-owned planar decode/interleave
already exposes exact Pillow `48x32` YCbCr bytes, while the facade remains
limited to argument normalization, handle transfer, metadata lifetime, and
Pillow's second-call no-op semantics. No production route or ABI changed.

`FMT-JPEG-003BO` extends the requested-mode route to WIC's full-scale RGB
4:2:0 planar shape. The DLL accepts half-width/half-height Cb/Cr planes and
performs Pillow-compatible h2v2 fancy reconstruction directly into its
contiguous YCbCr image buffer. Existing full-size interleave, h2v1 fancy, and
scale-8 h2v1 nearest branches remain distinct; the facade and ABI shape are
unchanged.

`FMT-JPEG-003BP` verifies the requested-mode route at decoder scale 4 for
RGB-JPEG-to-L. WIC's complete reduced planar request remains inside the DLL;
the DLL retains the exact `12x8` Y plane and releases temporary Cb/Cr storage.
The facade still handles only argument normalization, handle transfer,
metadata lifetime, and second-call no-op semantics. No production route or
ABI changed.

`FMT-JPEG-003BQ` verifies the adjacent decoder scale-8 route for the same
RGB 4:2:2 fixture. The existing planar/Y path returns exact Pillow `L 6x4`
bytes, and the facade remains a thin requested-mode/lifetime route. Raw and
facade coverage is green; no native source, ABI, or DLL artifact changed.

Acceleration baseline, 2026-08-06: the official Pillow 11.3.0 build/source
references are `setup.py` and `src/PIL/features.py`, which enumerate the
native dependency surfaces `zlib`, `jpeg`, `tiff`, `freetype`, `raqm`, `lcms`,
`webp`, `jpeg2000`, `imagequant`, and `avif` (with JPEG and zlib required).
The local native baseline is MSVC v143 Release x64, C++17, Level 4 warnings,
function-level linking, intrinsic functions, whole-program optimization,
COM/WIC (`windowscodecs.lib` and `ole32.lib`), and vendored lcms2. The
one-day acceleration route is to add one official backend at a time behind
the existing native image ABI: libjpeg-turbo 3.1.1 for JPEG, libpng/zlib for
PNG, libtiff for TIFF, libwebp for WebP, OpenJPEG 2.5.3 for JPEG2000, libavif
plus an explicit AV1 backend for AVIF, and FreeType/HarfBuzz/FriBidi/Raqm for
text. Pillow's private `_imaging` C internals are semantic references, not a
drop-in ABI; each backend still needs raw/facade tests and a reproducible
Release x64 build.
The local Pillow 11.3.0 authority reports libjpeg-turbo 3.1.1, OpenJPEG 2.5.3,
libwebp 1.5.0, libavif 1.3.0, zlib-ng 2.2.4, libtiff 4.7.0, lcms2 2.17,
FreeType 2.13.3, and Raqm 0.10.1 with FriBidi 1.0.5/HarfBuzz 11.2.1; this is
the version lock for a same-day backend batch.

`FMT-JPEG-003BG` adds a requested-mode draft ABI for RGB-JPEG-to-L decode.
The packed transform cannot provide reduced gray and WIC's planar transform
requires a complete Y/Cb/Cr request, so the DLL allocates reduced temporary
chroma planes and returns the decoder's Y plane directly as L. The facade only
normalizes the proven mode request and transfers handle/metadata lifetime.
There is no RGB conversion, resize, fallback, or AHK pixel loop.

`FMT-JPEG-003BF` extends decoder-native draft ownership to ordinary RGB JPEGs.
WIC's transform supplies reduced BGR directly; the DLL owns the single
contiguous B/R swap needed for the public RGB buffer. The facade only admits
same-mode RGB/CMYK JPEG requests, invokes the existing draft ABI, swaps handle
ownership, reapplies metadata, and returns the tuple. No full-size resize,
fallback, repeated fine-grained DllCall, or AHK pixel loop is involved.

`FMT-JPEG-003BE` extends that ownership boundary to JPEG-plugin draft decode.
The DLL chooses a JPEG denominator and asks WIC's
`IWICBitmapSourceTransform::CopyPixels` for decoder-native reduced CMYK bytes;
it does not decode full-size and resize, and it has no fallback branch. The
facade validates the bounded public arguments, replaces the opened handle,
reapplies native metadata, frees the old handle, records one-shot draft state,
and returns the Pillow-compatible mode/box. AHK performs no pixel loop.

`FMT-JPEG-003BD` broadens the real-fixture YCCK ownership proof to an odd
`17x11` ImageMagick/libjpeg-turbo source with APP14 transform `2` and distinct
`2x2/1x1/1x1/2x2` sampling. The DLL owns four-component decode, CMYK byte
storage, DQT metadata, and quality/qtables keep normalization to transform-0
`1x1` CMYK output. The facade only resolves the public keep sentinel and owns
image/Buffer lifetime; it performs no marker parsing or pixel loop.

`FMT-JPEG-003BC` locks both sides of the APP13 structured read boundary. The
same native state machine accepts exactly 14 available field bytes and ignores
13 without partial scalar mutation; the facade observes either complete DLL
state through its existing scalar call. This preserves a native-owned,
all-or-nothing metadata boundary without AHK byte inspection.

`FMT-JPEG-003BB` corrects the native APP13 structured-resource boundary. The
four exported ResolutionInfo fields occupy only bytes `0..13`; requiring 16
bytes incorrectly discarded a Pillow-readable 15-byte duplicate. The DLL now
checks the exact 14-byte read span before decoding and retains full ownership
of parsing and scalar state. The facade route remains a one-call Map
materialization with no byte or pixel loop.

`FMT-JPEG-003BA` confirms the APP13 ownership boundary for duplicate
structured ResolutionInfo resources. The DLL parser walks both `8BIM`
resources and replaces all four scalar fields when the second valid `0x03ED`
arrives; the facade performs one scalar metadata call and materializes only
the final `Info["photoshop"][1005]` Map. No APP13 bytes or image pixels are
parsed in AHK, and no new ABI is needed.

`META-002DG` closes the current binary-tag read/write matrix with XMLPacket
tag 700. The existing native serializer owns EXIF type/count/offset/padding
and native codecs own saves/reopens. The facade adds only exact BYTE routing
and Buffer lifetime; it deliberately does not map EXIF tag 700 into
codec-level XMP state, so no AHK parsing, pixel loop, or new export appears.

`META-002DF` adds four unregistered binary tags to the facade BYTE assignment
route while retaining their TIFF UNDEFINED open routes. The existing native
serializer owns the exact 86-byte blob, mixed inline/out-of-line storage, odd
count padding, and payload copies; native JPEG/PNG codecs own explicit saves
and reopens. The facade only dispatches the exact tags and normalizes Buffer
lifetime, with no AHK pixel loop or new export.

`META-002DE` adds the remaining bounded Pillow-registered type-7 family to the
facade UNDEFINED assignment route. The existing native serializer owns the
exact 92-byte blob, tag ordering, offsets, padding, and payload copies; native
JPEG/PNG codecs own explicit saves/reopens. The facade only selects the exact
four tags and normalizes Buffer lifetime, with no AHK pixel loop or new
export. EXIF tag 34675 remains distinct from codec-level `icc_profile`.

`META-002DD` adds a mixed registered UNDEFINED/BYTE EXIF family to the bounded
facade write/read route. The existing native serializer owns the exact
86-byte blob, type/count dispatch, offset calculation, and alignment; native
JPEG/PNG codecs own explicit saves/reopens. The facade only admits the exact
five tags, selects the native UNDEFINED or BYTE serializer, and normalizes
Buffer lifetime. BYTE read/write sets remain symmetric at 30/30, with no AHK
pixel loop or new export.

`META-002DC` adds OriginalRawFileData and IlluminantData1/2/3 to the bounded
facade `Image.Exif` BYTE-array write/read route while retaining TIFF
UNDEFINED/type-7 open routes. The existing native serializer owns mixed
inline/out-of-line values, odd-length alignment, and the exact 88-byte blob;
native JPEG/PNG codecs own explicit saves/reopens. The facade dispatches by
serialized EXIF type and normalizes Buffer lifetime, with no AHK pixel loop
or new export.

`META-002DB` adds DNG OpcodeList1/2/3 to the bounded facade `Image.Exif`
BYTE-array write/read route while retaining their TIFF UNDEFINED/type-7 open
route. The existing native serializer owns the exact 80-byte blob and native
JPEG/PNG codecs own explicit saves/reopens. The facade dispatches by serialized
EXIF type and splits its 23-tag write allowlist into equivalent `20+3` switch
branches because AHK caps case parameters at 20. No AHK pixel loop or new
export is introduced.

`META-002DA` completes the bounded native-readable BYTE-array tag set on the
facade write side by adding `PhotoshopInfo` 34377 and `TimeCodes` 51043. The
existing native serializer owns tag ordering, count-8 offsets, payload copies,
and the exact 60-byte EXIF blob; native JPEG/PNG codecs own explicit saves and
reopens. The facade only admits the two tags and normalizes Buffer lifetime.
Read/write allowlists are now 20/20 with zero difference, with no AHK pixel
loop or new export.

`META-002CZ` batches four oracle-equivalent XP EXIF BYTE-text tags onto the
bounded facade `Image.Exif` write/read route. The existing native serializer
owns tag ordering, counts, offsets, payload copies, and the exact 118-byte
EXIF blob; native JPEG/PNG codecs own explicit saves and reopens. The facade
only admits tags 40092 through 40095 and normalizes Buffer lifetime. UTF-16LE
payloads remain bytes, with no AHK pixel/text loop or new export.

`META-002CY` batches two oracle-equivalent DNG embedded-profile tags onto the
bounded facade `Image.Exif` BYTE-array write/read route. The existing native
serializer owns tag ordering, offsets, payload copies, and the exact 60-byte
EXIF blob; native JPEG/PNG codecs own explicit saves and reopens. The facade
only admits tags 50831 and 50833 and normalizes Buffer lifetime. These EXIF
bytes are not implicitly promoted to codec-level ICC metadata or ImageCms,
and no AHK pixel loop or new export is introduced.

`META-002CX` batches four oracle-equivalent DNG version/model/CFA tags onto
the bounded facade `Image.Exif` BYTE-array write/read route. The existing
native serializer owns tag ordering and inline counts `4/4/4/3` in the exact
68-byte EXIF blob; native JPEG/PNG codecs own explicit saves and reopens. The
facade only admits tags 50706, 50707, 50709, and 50710 and normalizes Buffer
lifetime, with no AHK pixel loop or new export.

`META-002CW` batches five oracle-equivalent DNG digest/identifier tags onto
the bounded facade `Image.Exif` BYTE-array write/read route. The existing
native serializer owns exact EXIF entry ordering, offsets, payload copies,
and the combined 160-byte blob; native JPEG/PNG codecs own explicit saves and
reopens. The facade only admits tags 50969, 50972, 50973, 50781, and 51111,
normalizes Buffer lifetime, and maps type-1 readback. TIFF 50969 type-7
metadata remains independently dispatched through UNDEFINED, with no AHK
pixel loop or new export.

`META-002CV` extends the bounded facade `Image.Exif` write/read routing for
`ProfileGainTableMap` 52525. Pillow's write-side Buffer/bytes representation
is BYTE/type-1, count-8, so the facade sends it through the existing generic
native BYTE-array EXIF serializer and reads JPEG/PNG EXIF through the matching
native parser; TIFF type-7 open metadata remains on the existing UNDEFINED
route. Native JPEG/PNG saves and opens own codec, buffer, and metadata work;
the facade only normalizes the tag/value and manages lifetime, with no AHK
pixel loop or new export.

`META-002CU` extends the bounded 52525 path to inline type-7/count-4
`ProfileGainTableMap` metadata while retaining the count-8 out-of-line route.
Native TIFF parsing owns inline extraction, exact `{4,8}` count gating, EXIF
serialization, and `pillow_c_exif_undefined_tag` readback; the facade reuses
its existing read-only 52525 enumeration. No AHK TIFF parser or pixel loop is
introduced.

`META-002CT` extends the bounded native TIFF UNDEFINED metadata boundary to
type-7/count-8 `ProfileGainTableMap` 52525. Native parsing enforces the exact
count, serializes the bytes into the existing EXIF blob, and reuses
`pillow_c_exif_undefined_tag`; the facade only extends its read-only UNDEFINED
enumeration. Gain-map interpretation/application, other counts, and writeback
remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002CS` completes the bounded three-type `DefaultCropOrigin` 50719
readback matrix by adding type-3/count-2 SHORT metadata through the native
ushort-array parser and existing EXIF ushort-array ABI. The facade only
extends its read-only ushort-array enumeration; RATIONAL, LONG, and SHORT
remain explicitly type-dispatched, with no AHK TIFF parser or pixel loop.

`META-002CR` extends `DefaultCropOrigin` 50719 metadata readback to the bounded
type-4/count-2 LONG shape through the native uint-array parser and existing
EXIF uint-array ABI. Native dispatch explicitly separates 50719 type 5 from
type 4 so a LONG entry is not consumed by the earlier RATIONAL route; the
facade only extends its read-only uint-array enumeration. No AHK TIFF parser
or pixel loop is introduced.

`META-002CQ` completes the bounded three-type `DefaultCropSize` 50720
readback matrix by adding type-3/count-2 SHORT metadata through the native
ushort-array parser and existing EXIF ushort-array ABI. The facade only
extends its read-only ushort-array enumeration; RATIONAL, LONG, and SHORT
remain explicitly type-dispatched, with no AHK TIFF parser or pixel loop.

`META-002CP` extends `DefaultCropSize` 50720 metadata readback to the bounded
type-4/count-2 LONG shape through the native uint-array parser and existing
EXIF uint-array ABI. Native dispatch explicitly separates 50720 type 5 from
type 4 so a LONG entry is not consumed by the earlier RATIONAL route; the
facade only extends its read-only uint-array enumeration. No AHK TIFF parser
or pixel loop is introduced.

`META-002CO` extends the bounded native TIFF RATIONAL-array metadata boundary
to type-5/count-2 `DefaultCropSize` 50720. Native parsing reuses exact type/
count validation, EXIF serialization, and
`pillow_c_exif_rational_array_tag`; the facade only extends its read-only
RATIONAL-array tag enumeration. Crop-size interpretation/application, other
values/counts/types, and writeback remain separate; no AHK TIFF parser or
pixel loop is introduced.

`META-002CN` extends the bounded native TIFF RATIONAL-array metadata boundary
to type-5/count-2 `DefaultCropOrigin` 50719. Native parsing reuses exact type/
count validation, EXIF serialization, and
`pillow_c_exif_rational_array_tag`; the facade only extends its read-only
RATIONAL-array tag enumeration. Crop interpretation/application, other
counts, and writeback remain separate; no AHK TIFF parser or pixel loop is
introduced.

`META-002CM` extends the bounded native TIFF RATIONAL-array metadata boundary
to type-5/count-2 `DefaultScale` 50718. Native parsing reuses exact type/count
validation, EXIF serialization, and `pillow_c_exif_rational_array_tag`; the
facade only extends its read-only RATIONAL-array tag enumeration. Scale
interpretation/application, other counts, and writeback remain separate; no
AHK TIFF parser or pixel loop is introduced.

`META-002CL` extends the bounded native TIFF scalar-integer metadata boundary
to type-3/count-1 `WhiteLevel` 50717. Native dispatch admits the tag through
the existing unsigned integer parser only when the TIFF value type is SHORT,
then reuses EXIF serialization and `pillow_c_exif_uint_tag`; the facade only
extends its read-only integer tag enumeration. White-level interpretation/
application, other types/counts, and writeback remain separate; no AHK TIFF
parser or pixel loop is introduced.

`META-002CK` extends the bounded native TIFF SRATIONAL scalar metadata
boundary to type-10/count-1 `BlackLevelDeltaV` 50716. Native parsing reuses
exact type/count validation, EXIF serialization, and
`pillow_c_exif_signed_rational_tag`; the facade only extends its read-only
SRATIONAL scalar tag enumeration. Delta interpretation/application, other
values/counts/types, and writeback remain separate; no AHK TIFF parser or
pixel loop is introduced.

`META-002CJ` extends the bounded native TIFF SRATIONAL-array metadata boundary
to type-10/count-2 `BlackLevelDeltaH` 50715. Native parsing reuses exact
range/count validation, EXIF serialization, and
`pillow_c_exif_signed_rational_array_tag`; the facade only extends its read-
only SRATIONAL-array tag enumeration. Delta interpretation/application, other
counts, and writeback remain separate; no AHK TIFF parser or pixel loop is
introduced.

`META-002CI` extends the bounded native TIFF RATIONAL-array metadata boundary
to type-5/count-4 `BlackLevel` 50714. Native parsing reuses exact range/count
validation, EXIF serialization, and `pillow_c_exif_rational_array_tag`; the
facade only extends its read-only RATIONAL-array tag enumeration. Black-level
interpretation/application, other counts, and writeback remain separate; no
AHK TIFF parser or pixel loop is introduced.

`META-002CH` extends the bounded native TIFF SHORT-array metadata boundary to
type-3/count-2 `BlackLevelRepeatDim` 50713. Native parsing reuses exact range/
count validation, EXIF serialization, and `pillow_c_exif_ushort_array_tag`;
the facade only extends its read-only SHORT-array tag enumeration. Black-level
interpretation/application, other counts, and writeback remain separate; no
AHK TIFF parser or pixel loop is introduced.

`META-002CG` extends the bounded native TIFF SHORT-array metadata boundary to
type-3/count-4 `LinearizationTable` 50712. Native parsing reuses exact range/
count validation, EXIF serialization, and `pillow_c_exif_ushort_array_tag`;
the facade only extends its read-only SHORT-array tag enumeration. Table
application, other counts, and writeback remain separate; no AHK TIFF parser
or pixel loop is introduced.

`META-002CF` extends the bounded native TIFF scalar-integer metadata boundary
to type-4/count-1 `RowInterleaveFactor` 50975. Native dispatch admits the tag
through the existing integer parser only when the TIFF value type is LONG,
then reuses EXIF serialization and `pillow_c_exif_uint_tag`; the facade only
extends its read-only integer tag enumeration. Row-interleave interpretation/
application, other types, and writeback remain separate; no AHK TIFF parser or
pixel loop is introduced.

`META-002CE` extends the bounded native TIFF scalar-integer metadata boundary
to type-4/count-1 `SubTileBlockSize` 50974. Native dispatch admits the tag
through the existing integer parser only when the TIFF value type is LONG,
then reuses EXIF serialization and `pillow_c_exif_uint_tag`; the facade only
extends its read-only integer tag enumeration. Sub-tile interpretation/
application, other types, and writeback remain separate; no AHK TIFF parser or
pixel loop is introduced.

`META-002CD` extends the bounded native TIFF BYTE-array metadata boundary to
type-1/count-16 `RawDataUniqueID` 50781. Native parsing reuses exact range/
count validation, EXIF serialization, and `pillow_c_exif_byte_array_tag`; the
facade only extends its read-only BYTE-array tag enumeration. Identifier
interpretation/validation, other counts, and writeback remain separate; no AHK
TIFF parser or pixel loop is introduced.

`META-002CC` extends the bounded native TIFF BYTE-array metadata boundary to
type-1/count-16 `OriginalRawFileDigest` 50973. Native parsing reuses exact
range/count validation, EXIF serialization, and
`pillow_c_exif_byte_array_tag`; the facade only extends its read-only BYTE-
array tag enumeration. Digest interpretation/validation, other counts, and
writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002CB` extends the bounded native TIFF BYTE-array metadata boundary to
type-1/count-16 `RawImageDigest` 50972. Native parsing reuses exact range/count
validation, EXIF serialization, and `pillow_c_exif_byte_array_tag`; the facade
only extends its read-only BYTE-array tag enumeration. Digest interpretation/
validation, other counts, and writeback remain separate; no AHK TIFF parser or
pixel loop is introduced.

`META-002CA` extends the bounded native TIFF UNDEFINED metadata boundary to
type-7/count-16 `PreviewSettingsDigest` 50969. Native parsing reuses exact
range/count validation, EXIF serialization, and
`pillow_c_exif_undefined_tag`; the facade only extends its read-only UNDEFINED
tag enumeration. Digest interpretation/validation, other counts, and
writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BZ` extends the bounded native TIFF UNDEFINED metadata boundary to
type-7/count-8 `OpcodeList3` 51022. Native parsing reuses exact range/count
validation, EXIF serialization, and `pillow_c_exif_undefined_tag`; the facade
only extends its read-only UNDEFINED tag enumeration. Opcode decoding or
application, other counts, and writeback remain separate; no AHK TIFF parser
or pixel loop is introduced.

`META-002BY` extends the bounded native TIFF UNDEFINED metadata boundary to
type-7/count-8 `OpcodeList2` 51009. Native parsing reuses exact range/count
validation, EXIF serialization, and `pillow_c_exif_undefined_tag`; the facade
only extends its read-only UNDEFINED tag enumeration. Opcode decoding or
application, other counts, and writeback remain separate; no AHK TIFF parser
or pixel loop is introduced.

`META-002BX` extends the bounded native TIFF UNDEFINED metadata boundary to
type-7/count-8 `OpcodeList1` 51008. Native parsing reuses exact range/count
validation, EXIF serialization, and `pillow_c_exif_undefined_tag`; the facade
only extends its read-only UNDEFINED tag enumeration. Opcode decoding or
application, other counts, and writeback remain separate; no AHK TIFF parser
or pixel loop is introduced.

`META-002BW` extends the bounded native TIFF UNDEFINED metadata boundary to
type-7/count-8 `OriginalRawFileData` 50828. Native parsing reuses exact range/
count validation, EXIF serialization, and `pillow_c_exif_undefined_tag`; the
facade only extends its read-only UNDEFINED tag enumeration. Original-file
decoding/interpretation, other counts, and writeback remain separate; no AHK
TIFF parser or pixel loop is introduced.

`META-002BV` extends the bounded native TIFF scalar-integer metadata boundary
to type-3/count-1 `ColorimetricReference` 50879. Native dispatch admits the
tag through the existing integer parser only when the TIFF value type is
SHORT, then reuses EXIF serialization and `pillow_c_exif_uint_tag`; the facade
only extends its read-only integer tag enumeration. Colorimetric
interpretation/application, other types, and writeback remain separate; no
AHK TIFF parser or pixel loop is introduced.

`META-002BU` extends the bounded native TIFF SRATIONAL-array metadata boundary
to type-10/count-9 `CurrentPreProfileMatrix` 50834. Native parsing reuses exact
range/count and nonzero-denominator validation, EXIF serialization, and
`pillow_c_exif_signed_rational_array_tag`; the facade only extends its
read-only matrix enumeration. Matrix interpretation/application, other
dimensions, and writeback remain separate; no AHK TIFF parser or pixel loop is
introduced.

`META-002BT` extends the bounded native TIFF BYTE-array metadata boundary to
type-1/count-8 `CurrentICCProfile` 50833. Native parsing reuses exact range/
count validation, EXIF serialization, and `pillow_c_exif_byte_array_tag`; the
facade only extends its read-only tag enumeration and does not synthesize
current-image ICC metadata. Profile validation/application, other counts, and
writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BS` extends the bounded native TIFF SRATIONAL-array metadata boundary
to type-10/count-9 `AsShotPreProfileMatrix` 50832. Native parsing reuses exact
range/count and nonzero-denominator validation, EXIF serialization, and
`pillow_c_exif_signed_rational_array_tag`; the facade only extends its
read-only matrix enumeration. Matrix interpretation/application, other
dimensions, and writeback remain separate; no AHK TIFF parser or pixel loop is
introduced.

`META-002BR` extends the bounded native TIFF BYTE-array metadata boundary to
type-1/count-8 `AsShotICCProfile` 50831. Native parsing reuses exact range/
count validation, EXIF serialization, and `pillow_c_exif_byte_array_tag`; the
facade only extends its read-only tag enumeration and does not synthesize
current-image ICC metadata. Profile validation/application, other counts, and
writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BQ` extends the bounded native TIFF `MaskedAreas` 50830 LONG-array
metadata boundary from count 4 to count 8. Native parsing keeps an explicit
allowed-count set `{4,8}` with range validation, EXIF serialization, and the
dynamic required-count `pillow_c_exif_uint_array_tag` ABI; facade routing needs
no further change. Mask interpretation/normalization, other counts, and
writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BP` extends the bounded native TIFF LONG-array metadata boundary to
type-4/count-4 `MaskedAreas` 50830. Native parsing reuses exact range/count
validation, EXIF serialization, and `pillow_c_exif_uint_array_tag`; the facade
only extends its read-only tag enumeration. Multiple rectangles, mask
interpretation/normalization, other counts/types, and writeback remain
separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BO` extends the bounded native TIFF metadata boundary for
`ActiveArea` 50829 to its type-3/count-4 SHORT alternate form. The entry loop
uses TIFF type as part of tag dispatch, preserving the covered type-4/count-4
LONG route while making the SHORT parser reachable. Existing EXIF
serialization and `pillow_c_exif_ushort_array_tag` expose the integer tuple;
the facade only enumerates the tag through its read-only SHORT route. No AHK
TIFF parser, pixel loop, or new ABI surface is introduced.

`META-002BN` extends the bounded native TIFF LONG-array metadata boundary to
type-4/count-4 `ActiveArea` 50829. Native parsing reuses exact range/count
validation, EXIF serialization, and `pillow_c_exif_uint_array_tag`; the facade
only extends its read-only tag enumeration. Active-area interpretation/
normalization, alternate SHORT form, other counts/types, and writeback remain
separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BM` extends the bounded native TIFF ASCII metadata boundary to
type-2/count-9 `CameraLabel` 51092. Native parsing reuses exact range
validation, EXIF serialization, and `pillow_c_exif_ascii_tag`; the facade only
extends its read-only tag enumeration. Camera-label interpretation, non-ASCII
encodings, other counts/types, arbitrary payload families, and writeback
remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BL` extends the bounded native TIFF metadata boundary for
`OriginalDefaultCropSize` 51091 to its type-4/count-2 LONG alternate form.
The entry loop now uses TIFF type as part of tag dispatch, preserving the
covered type-5/count-2 RATIONAL route while making the LONG parser reachable.
Existing EXIF serialization and `pillow_c_exif_uint_array_tag` expose the
integer tuple; the facade only enumerates the tag through its read-only route.
No AHK TIFF parser, pixel loop, or new ABI surface is introduced.

`META-002BK` extends the bounded native TIFF RATIONAL-array metadata boundary
to type-5/count-2 `OriginalDefaultCropSize` 51091. Native parsing reuses exact
range/count validation, EXIF serialization, and
`pillow_c_exif_rational_array_tag`; the facade only extends its read-only tag
enumeration. Crop-size interpretation/normalization, alternate counts/types,
and writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BJ` extends the bounded native TIFF LONG-array metadata boundary to
type-4/count-2 `OriginalBestQualityFinalSize` 51090. Native parsing reuses
exact range/count validation, EXIF serialization, and
`pillow_c_exif_uint_array_tag`; the facade only extends its read-only tag
enumeration. Image-size interpretation/normalization, alternate counts/types,
and writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BI` extends the bounded native TIFF LONG-array metadata boundary to
type-4/count-2 `OriginalDefaultFinalSize` 51089. Native parsing reuses exact
range/count validation, EXIF serialization, and
`pillow_c_exif_uint_array_tag`; the facade only extends its read-only tag
enumeration. Image-size interpretation/normalization, alternate counts/types,
and writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BH` extends the bounded native TIFF ASCII metadata boundary to
type-2/count-10 `ReelName` 51081. Native parsing reuses exact range validation,
EXIF serialization, and `pillow_c_exif_ascii_tag`; the facade only extends its
read-only tag enumeration. Reel/timeline interpretation, non-ASCII encodings,
arbitrary payload families, and writeback remain separate; no AHK TIFF parser
or pixel loop is introduced.

`META-002BG` extends the bounded native TIFF scalar-RATIONAL metadata
boundary to type-5/count-1 `TStop` 51058. Native parsing reuses exact
range/count validation, EXIF serialization, and
`pillow_c_exif_rational_tag`; the facade only extends its read-only tag
enumeration. T-stop interpretation/normalization, arrays, arbitrary counts,
and writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BF` extends the bounded native TIFF scalar-SRATIONAL metadata
boundary to type-10/count-1 `FrameRate` 51044. Native parsing reuses exact
range/count validation, EXIF serialization, and
`pillow_c_exif_signed_rational_tag`; the facade only extends its read-only tag
enumeration. Frame-rate interpretation/normalization, arrays, arbitrary
counts, and writeback remain separate; no AHK TIFF parser or pixel loop is
introduced.

`META-002BE` extends the bounded native TIFF BYTE-array metadata boundary to
type-1/count-8 `TimeCodes` 51043. Native parsing reuses exact range/count
validation, EXIF serialization, and `pillow_c_exif_byte_array_tag`; the facade
only extends its read-only tag enumeration. Timecode interpretation, multiple
timecodes, arbitrary counts, and writeback remain separate; no AHK TIFF parser
or pixel loop is introduced.

`META-002BD` extends the bounded native TIFF `NoiseProfile` 51041 DOUBLE-array
metadata boundary to count 4, completing exact counts 2/4/6/8. Native parsing
still uses exact allowed counts with range validation, bit-preserving EXIF
serialization, and the dynamic required-count
`pillow_c_exif_double_array_tag` ABI; facade routing needs no change. Noise-
model interpretation, other counts, and writeback remain separate; no AHK TIFF
parser or pixel loop is introduced.

`META-002BC` extends the bounded native TIFF `NoiseProfile` 51041 DOUBLE-array
metadata boundary from count 6 to counts 2 and 8. Native parsing still uses
exact allowed counts with range validation, bit-preserving EXIF serialization,
and the existing dynamic required-count `pillow_c_exif_double_array_tag` ABI;
facade routing needs no change. Noise-model interpretation, other counts, and
writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BB` adds a bounded native TIFF DOUBLE-array metadata boundary for
type-12/count-6 `NoiseProfile` 51041. Native parsing reuses exact range/count
validation, bit-preserving EXIF serialization, and the dynamic required-count
`pillow_c_exif_double_array_tag` ABI; the facade only extends its read-only
tag enumeration. Noise-model interpretation, arbitrary counts, and writeback
remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002BA` extends the bounded native TIFF FLOAT-array metadata boundary for
`ProfileToneCurve` 50940 from count 6 to count 18, representing nine control
points. Native parsing still uses exact allowed counts with range validation,
bit-preserving EXIF serialization, and the existing dynamic required-count
`pillow_c_exif_float_array_tag` ABI; facade routing needs no change. Curve
evaluation, arbitrary control-point counts, and writeback remain separate; no
AHK TIFF parser or pixel loop is introduced.

`META-002AZ` composes the bounded native TIFF LONG-array and FLOAT-array
metadata boundaries for `ProfileHueSatMapDims` 50937 count 3 plus
`ProfileHueSatMapData1` 50938 and `ProfileHueSatMapData2` 50939 count 54.
Native parsing still uses exact allowed counts with range validation,
bit-preserving EXIF serialization, and existing dynamic required-count ABIs;
facade routing needs no change. DNG map interpretation/allocation, arbitrary
dimensions/counts, and writeback remain separate; no AHK TIFF parser or pixel
loop is introduced.

`META-002AY` composes the bounded native TIFF LONG-array and FLOAT-array
metadata boundaries for `ProfileLookTableDims` 50981 count 3 and
`ProfileLookTableData` 50982 count 54. Native parsing still uses exact allowed
counts with range validation, bit-preserving EXIF serialization, and the
existing dynamic required-count ABIs; facade routing needs no change. DNG
look-table interpretation/allocation, arbitrary dimensions/counts, and
writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002AX` extends the bounded native TIFF FLOAT-array metadata boundary for
`ProfileLookTableData` 50982 from count 6 to count 18. Native parsing still
uses exact allowed counts with range validation, bit-preserving EXIF
serialization, and `pillow_c_exif_float_array_tag`; the facade's dynamic
required-count route needs no change. DNG look-table interpretation/allocation,
arbitrary counts, and writeback remain separate; no AHK TIFF parser or pixel
loop is introduced.

`META-002AW` extends the native TIFF FLOAT-array metadata boundary to
type-11/count-6 `ProfileLookTableData` 50982. Native parsing reuses exact
range/count validation, bit-preserving EXIF serialization, and
`pillow_c_exif_float_array_tag`; the facade only extends its read-only tag
enumeration. DNG look-table interpretation/allocation, broader table counts,
and writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002AV` extends the native TIFF LONG-array metadata boundary to
type-4/count-3 `ProfileLookTableDims` 50981. Native parsing reuses exact
range/count validation, EXIF serialization, and
`pillow_c_exif_uint_array_tag`; the facade only extends its read-only tag
enumeration. DNG look-table interpretation/allocation, FLOAT data, and
writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002AU` extends the native TIFF scalar-RATIONAL metadata boundary to
type-5/count-1 `NoiseReductionApplied` 50935. Native parsing validates the
payload range and serializes the exact numerator/denominator pair through the
existing `pillow_c_exif_rational_tag` route; the facade only extends its
read-only tag enumeration. Noise-reduction interpretation and writeback
remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002AT` extends the native TIFF FLOAT-array metadata boundary to one
type-11/count-6 `ProfileToneCurve` 50940 representing three control points.
Native parsing reuses exact range/count validation, bit-preserving EXIF
serialization, and `pillow_c_exif_float_array_tag`; the facade only extends
its read-only tag enumeration. Curve evaluation and writeback remain
separate; no AHK TIFF parser or pixel loop is introduced.

`META-002AS` extends the native TIFF FLOAT-array metadata boundary to
type-11/count-6 `ProfileHueSatMapData2` 50939. Native parsing reuses the exact
range/count validation, bit-preserving EXIF serializer, and
`pillow_c_exif_float_array_tag`; the facade only extends its read-only tag
enumeration. DNG profile-map interpretation/allocation and writeback remain
separate; no AHK TIFF parser or pixel loop is introduced.

`META-002AR` adds a native TIFF FLOAT-array metadata boundary for
type-11/count-6 `ProfileHueSatMapData1` 50938. Native parsing validates the
exact payload range and count, preserves each float32 bit pattern through the
shared EXIF serializer, and exposes host-order floats through
`pillow_c_exif_float_array_tag`. The facade only performs required-count ABI
routing and `NumGet(..., "Float")` materialization into a separate read-only
map. DNG profile-map interpretation/allocation and writeback remain separate;
no AHK TIFF parser or pixel loop is introduced.

`META-002AQ` reuses the native LONG-array metadata boundary for TIFF
type-4/count-3 `ProfileHueSatMapDims` 50937. Native parsing validates the exact
payload range and bounded count before the shared EXIF serializer and
`pillow_c_exif_uint_array_tag` ABI expose three host-order integers. The facade
only extends its read-only tag enumeration. DNG profile-map interpretation/
allocation and FLOAT map data remain separate; no AHK TIFF parser or pixel
loop is introduced.

`META-002AP` reuses the native DOUBLE-array metadata boundary for TIFF
type-12/count-92 `RPCCoefficientTag` 50844. Native parsing validates the exact
payload range and bounded count before the shared EXIF serializer and
`pillow_c_exif_double_array_tag` ABI expose host-order doubles. The facade only
extends its read-only tag enumeration. RPC model interpretation/evaluation and
writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002AO` batches the same native ASCII metadata route for TIFF
`GDAL_METADATA` 42112 and `GDAL_NODATA` 42113. Native parsing validates both
payload ranges, removes only TIFF NUL terminators, and serializes the exact
strings through the existing EXIF ASCII ABI. The facade only extends its
read-only tag enumeration. XML parsing, nodata numeric interpretation, and
writeback remain separate; no AHK TIFF parser or pixel loop is introduced.

`META-002AN` reuses the native SHORT-array metadata boundary for TIFF
type-3/count-8 `GeoKeyDirectoryTag` 34735. Native parsing validates the exact
payload range and bounded count before the shared EXIF serializer and
`pillow_c_exif_ushort_array_tag` ABI expose eight host-order integers. The
facade only extends its read-only tag enumeration. GeoKey interpretation,
referenced parameter resolution, and writeback remain separate; no AHK TIFF
parser or pixel loop is introduced.

`META-002AM` reuses the native ASCII metadata boundary for TIFF
type-2/count-15 `GeoAsciiParamsTag` 34737. Native parsing validates the payload
range, removes only the TIFF NUL terminator, preserves the trailing GeoTIFF
`|`, and serializes the exact public string through the existing EXIF ASCII
ABI. The facade only extends its read-only tag enumeration. GeoTIFF parameter
interpretation and writeback remain separate; no AHK TIFF parser or pixel loop
is introduced.

`META-002AL` reuses the native floating-array metadata boundary for TIFF
type-12/count-3 `GeoDoubleParamsTag` 34736. Native parsing validates the exact
payload range and bounded count before the shared EXIF serializer and
`pillow_c_exif_double_array_tag` ABI expose three host-order doubles. The
facade only extends its read-only tag enumeration. GeoTIFF interpretation,
coordinate transforms, and writeback remain separate; no AHK TIFF parser or
pixel loop is introduced.

`META-002AK` reuses the native floating-array metadata boundary for TIFF
type-12/count-16 `ModelTransformationTag` 34264. Native parsing validates the
exact payload range and count before the shared EXIF serializer and
`pillow_c_exif_double_array_tag` ABI expose sixteen host-order doubles. The
facade only extends its read-only tag enumeration. GeoTIFF interpretation,
coordinate transforms, and writeback remain separate; no AHK TIFF parser or
pixel loop is introduced.

`META-002AJ` reuses the native floating-array metadata boundary for TIFF
type-12/count-6 `ModelTiepointTag` 33922. Native parsing validates the exact
payload range and count before the shared EXIF serializer and
`pillow_c_exif_double_array_tag` ABI expose six host-order doubles. The facade
only extends its read-only tag enumeration. GeoTIFF interpretation, coordinate
transforms, and writeback remain separate; no AHK TIFF parser or pixel loop is
introduced.

`META-002AI` adds a reusable native floating-array metadata boundary. TIFF
type-12/count-3 `ModelPixelScaleTag` 33550 is range-validated, decoded, and
serialized as exact IEEE-754 values inside the DLL-owned EXIF blob. The new
`pillow_c_exif_double_array_tag` required-count ABI exposes host-order doubles;
the facade performs only one bulk DLL read plus AHK array materialization and
keeps the route read-only. GeoTIFF coordinate interpretation, transforms, and
writeback remain explicit gaps; no AHK TIFF parser or pixel loop is introduced.

`META-002AH` extends DLL-owned TIFF BYTE-array metadata to `PhotoshopInfo`
tag 34377. Native parsing validates and copies exact bytes into the TIFF EXIF
blob; the facade only enumerates the ID through the established required-count
byte-array ABI. 8BIM resource interpretation, Photoshop/IPTC info synthesis,
and save/writeback remain explicit gaps, and no AHK byte parser or pixel loop
is introduced.

`META-002AG` extends DLL-owned TIFF UNDEFINED metadata to `IptcNaaInfo` tag
33723. Native parsing validates and copies exact type-7 bytes into the TIFF
EXIF blob; the facade only enumerates the ID through the established
required-count undefined-tag ABI. IPTC field parsing and save/writeback remain
explicit gaps, and no AHK byte parser or pixel loop is introduced.

`META-001FD` removes the synthetic count-2 ceiling from non-pixel-source TIFF
tile byte-range metadata. The native L-strip recognizer validates both 324/325
LONG-array payload ranges and requires equal counts greater than one before
choosing strip decoding; the EXIF serializer then carries that exact count.
Actual pixel bytes continue to come only from valid scalar 273/279 strip
entries, and the facade reuses the required-count ABI without AHK pixel loops.

`META-001FC` removes the synthetic fixed-count ceiling from standard
multi-strip TIFF metadata. Native 273/279 LONG-array serialization now follows
the validated source entry count whenever it is greater than one; count-1
continues through the scalar map. The existing required-count ABI and facade
length-based route therefore materialize arbitrary valid multi-strip array
lengths without AHK pixel loops or duplicated scalar entries.

`META-001FB` extends the standard multi-strip TIFF metadata path from four to
five LONG values. Native serialization carries the exact count through the
existing required-count ABI, and the facade's length-based route materializes
all values without AHK pixel loops or scalar duplication. Earlier count
shapes and independently bounded tile arrays remain unchanged.

`META-001FA` extends the standard multi-strip TIFF metadata path from three to
four LONG values. Native serialization carries the exact count through the
existing required-count ABI, and the facade's length-based route materializes
all values without AHK pixel loops or scalar duplication. Earlier count
shapes and independently bounded tile arrays remain unchanged.

`META-001EZ` extends the standard multi-strip TIFF metadata path from two to
three LONG values. The native EXIF builder serializes the exact entry count;
the existing required-count ABI and facade array route materialize all values
without AHK pixel loops. Count-1 scalar behavior and the count-2 array path
remain unchanged, while unrelated tile-array counts stay independently
bounded.

`META-001EY` extends DLL-owned TIFF metadata serialization to the standard
two-strip shape: LONG/count-2 `StripOffsets` 273 and `StripByteCounts` 279 are
preserved alongside exact WIC-decoded pixels. The facade routes the existing
required-count ABI and only creates `UintArrayTags` when more than one value
is present, keeping count-1 TIFFs represented solely by scalar tags. No AHK
pixel loop or duplicate metadata representation is introduced.

`META-001EX` composes the generalized LONG-array metadata route with the
native L-strip TIFF opener. A bounded 324/325 LONG/count-2 pair is recognized
as metadata while valid 273/279 strip entries remain the sole pixel source,
avoiding WIC's rejection of the fixture as tiled storage. The DLL serializes
both arrays into its TIFF EXIF blob; the facade only calls the existing
required-count ABI and stores the AHK Arrays. Count-1 324/325 entries continue
through the scalar route, and actual tiled decoding remains a separate gap.

`META-001EW` extends the native TIFF metadata boundary with a generalized
LONG/type-4 array reader and EXIF serializer. The bounded open route admits
IFD0 `MaskSubArea` 52536 only at count 4, validates the referenced payload,
and serializes exact unsigned values into the DLL-owned TIFF EXIF blob. The
new `pillow_c_exif_uint_array_tag` required-count ABI materializes those values
without AHK pixel loops; the facade only routes them into its distinct
`UintArrayTags` collection. Mask interpretation/application, other counts,
and public LONG-array writeback remain out of scope and cannot be silently
serialized by the facade.

`META-001EV` extends the native TIFF UNDEFINED metadata boundary to IFD0
`IlluminantData1` 52533, `IlluminantData2` 52534, and `IlluminantData3` 52535.
The DLL validates each type-7 payload range before serializing exact inline or
out-of-line bytes into its TIFF EXIF blob. The facade only enumerates the IDs
through `pillow_c_exif_undefined_tag`; TIFF parsing, illuminant interpretation,
write-side type inference, and pixel traversal stay native or out of scope.

`META-001EU` extends the native TIFF signed-rational-array metadata boundary
to IFD0 `CameraCalibration3` 52530, `ColorMatrix3` 52531, and `ForwardMatrix3`
52532. The DLL requires type `10`, count `9`, valid payload ranges, and nine
nonzero denominators before serializing each matrix into its TIFF EXIF blob.
The facade only enumerates the IDs through
`pillow_c_exif_signed_rational_array_tag`; TIFF parsing, matrix interpretation,
write-side type inference, and pixel traversal stay native or out of scope.

`META-001ET` extends the native TIFF scalar-integer metadata boundary to IFD0
`CalibrationIlluminant3` 52529. The DLL admits the covered SHORT type-3,
count-1 entry through the bounded TIFF uint parser and serializes value `23`
into its TIFF EXIF blob. The facade only enumerates the ID through
`pillow_c_exif_uint_tag`; TIFF parsing, illuminant interpretation, write-side
type inference, and pixel traversal stay native or out of scope.

`META-001ES` extends the native TIFF ASCII metadata boundary to IFD0
`SemanticName` 52526 and `SemanticInstanceID` 52528. The DLL validates each
type-2 payload range and NUL termination before serializing the exact strings
into its TIFF EXIF blob. The facade only enumerates the IDs through
`pillow_c_exif_ascii_tag`; TIFF parsing, semantic-mask interpretation,
write-side type inference, and pixel traversal stay native or out of scope.

`META-001ER` extends the native TIFF ASCII metadata boundary to IFD0
`EnhanceParams` 51182. The DLL validates the type-2 payload range and NUL
termination before serializing `gain=1` into its TIFF EXIF blob. The facade
only enumerates the ID through `pillow_c_exif_ascii_tag`; TIFF parsing,
enhancement interpretation, write-side type inference, and pixel traversal
stay native or out of scope.

`META-001EQ` extends the native TIFF scalar-integer metadata boundary to IFD0
`DepthUnits` 51180 and `DepthMeasureType` 51181. The DLL admits the covered
SHORT type-3/count-1 entries through the bounded TIFF uint parser and
serializes values `1` / `2` into its TIFF EXIF blob. The facade only enumerates
the IDs through `pillow_c_exif_uint_tag`; TIFF parsing, depth interpretation,
write-side type inference, and pixel traversal stay native or out of scope.

`META-001EP` extends the native TIFF scalar-RATIONAL metadata boundary to
IFD0 `DepthNear` 51178 and `DepthFar` 51179. The DLL validates each type-5,
count-1 payload range and denominator before serializing the exact pair into
its TIFF EXIF blob. The facade only enumerates the IDs through the existing
`pillow_c_exif_rational_tag` ABI; TIFF parsing, depth-map interpretation,
write-side type inference, and pixel traversal stay native or out of scope.

`META-001EO` extends the native TIFF scalar-integer metadata boundary to IFD0
`DepthFormat` 51177. The DLL admits the covered SHORT type `3`, count `1`
entry through the existing bounded TIFF uint parser and serializes value `1`
into its TIFF EXIF blob. The facade only enumerates the ID through the existing
`pillow_c_exif_uint_tag` ABI; TIFF parsing, depth-map interpretation,
write-side type inference, and pixel traversal stay native or out of scope.

`META-001EN` extends the native TIFF RATIONAL-array metadata boundary to IFD0
`DefaultUserCrop` 51125. The DLL requires type `5`, count `4`, valid payload
ranges, and nonzero denominators before serializing the four pairs into its
TIFF EXIF blob. The facade only enumerates the tag through the existing bulk
ABI and materializes nested pairs; TIFF parsing, crop interpretation/
application, write-side type inference, and pixel traversal stay native or
out of scope.

`META-001EM` extends the native TIFF scalar-RATIONAL metadata boundary to
IFD0 `RawToPreviewGain` 51112. The DLL validates the numerator, nonzero
denominator, and payload range before serializing the entry into its TIFF EXIF
blob; the facade only enumerates the ID through the existing rational ABI.
TIFF parsing, DNG rendering interpretation, write-side type inference, and
pixel traversal stay native or out of scope.

`META-001EL` extends the native TIFF BYTE-array metadata boundary to IFD0
`NewRawImageDigest` 51111. The DLL validates the entry range and serializes
its bytes into the TIFF EXIF blob; the facade only enumerates the ID through
the existing byte-array ABI and materializes a `Buffer`. TIFF parsing, digest
interpretation/validation, write-side type inference, and pixel traversal
stay native or out of scope.

`META-001EK` extends the native TIFF scalar-integer metadata boundary to IFD0
`DefaultBlackRender` 51110. The DLL validates and serializes the scalar entry
into its TIFF EXIF blob; the facade only enumerates the ID through the existing
uint-tag ABI. TIFF parsing, DNG rendering interpretation, write-side type
inference, and pixel traversal stay native or out of scope.

`META-001EJ` extends the native TIFF scalar-SRATIONAL metadata boundary to
IFD0 `BaselineExposureOffset` 51109. The DLL validates the signed numerator,
nonzero denominator, and payload range before serializing the entry into its
TIFF EXIF blob; the facade only enumerates the ID through the existing signed-
rational ABI. TIFF parsing, DNG exposure interpretation, write-side type
inference, and pixel traversal stay native or out of scope.

`META-001EI` extends the native TIFF scalar-integer metadata boundary to IFD0
`ProfileHueSatMapEncoding` 51107 and `ProfileLookTableEncoding` 51108. The DLL
validates and serializes the scalar entries into its TIFF EXIF blob; the
facade only enumerates the two IDs through the existing uint-tag ABI. TIFF
parsing, DNG profile-table interpretation, write-side type inference, and
pixel traversal stay native or out of scope.

`META-001EH` extends the native TIFF scalar-integer metadata boundary to IFD0
`ProfileEmbedPolicy` 50941 and `PreviewColorSpace` 50970. The DLL validates
and serializes the scalar entries into its TIFF EXIF blob; the facade only
enumerates the two IDs through the existing uint-tag ABI. TIFF parsing, DNG
profile/preview interpretation, write-side type inference, and pixel traversal
stay native or out of scope.

`META-001EG` extends the signed-rational-array metadata boundary to TIFF IFD0
`ForwardMatrix1` 50964 and `ForwardMatrix2` 50965. The DLL requires type `10`,
count `9`, valid payload ranges, and nonzero signed denominators, then
serializes both arrays into its TIFF EXIF blob. The facade only enumerates the
two tags through the existing bulk ABI and materializes nested pairs; TIFF
parsing, DNG color interpretation, write-side type inference, and pixel
traversal stay native or out of scope.

`META-001EF` extends the signed-rational-array metadata boundary to TIFF IFD0
`CameraCalibration1/2` 50723/50724 and `ReductionMatrix1/2` 50725/50726. The
DLL requires type `10`, count `9`, valid payload ranges, and nonzero signed
denominators, then serializes the four arrays into its TIFF EXIF blob. The
facade only enumerates tags `50721..50726` through the existing bulk ABI and
materializes nested pairs; TIFF parsing, DNG color interpretation, write-side
type inference, and pixel traversal stay native or out of scope.

`META-001EE` adds a reusable signed-rational-array metadata boundary. The DLL
reads TIFF type `10` arrays into signed 32-bit numerator/denominator storage,
requires exactly nine nonzero-denominator pairs for IFD0 `ColorMatrix1`
50721 and `ColorMatrix2` 50722, and serializes them into the DLL-owned TIFF
EXIF blob. `pillow_c_exif_signed_rational_array_tag` exposes the typed array
with a two-call required-count ABI. The facade only materializes the returned
pairs in `GetExif()` / `getexif()`; TIFF parsing, DNG color interpretation,
write-side type inference, and pixel traversal remain native or out of scope.

`META-001ED` carries the native TIFF RATIONAL-array boundary to IFD0
`AsShotWhiteXY` and `LensInfo`. The DLL requires type `5`, count `2` for tag
`50729` and count `4` for tag `50736`, validates nonzero denominators, and
serializes both into the existing TIFF EXIF blob. The facade only enumerates
the two native rational-array entries during `GetExif()` / `getexif()`
materialization; DNG color/lens interpretation, public write-side inference,
TIFF parsing, and pixel traversal remain outside AHK.

`META-001EC` carries the native TIFF RATIONAL-array boundary to IFD0
`AnalogBalance` and `AsShotNeutral`. The DLL requires type `5`, count `3`,
valid nonzero denominators, and serializes tags `50727` / `50728` into the
existing TIFF EXIF blob. The facade only enumerates the two native rational-
array entries during `GetExif()` / `getexif()` materialization; DNG color
interpretation, public write-side inference, TIFF parsing, and pixel traversal
remain outside AHK.

`META-001EB` carries the native TIFF scalar-SRATIONAL boundary to IFD0
`BaselineExposure` and `ShadowScale`. The DLL requires one type-10 value with
a nonzero denominator and serializes tags `50730` / `50739` into the existing
TIFF EXIF blob. The facade only enumerates the two native signed-rational
entries during `GetExif()` / `getexif()` materialization; DNG exposure/shadow
interpretation, public write-side inference, TIFF parsing, and pixel traversal
remain outside AHK.

`META-001EA` carries the native TIFF scalar-RATIONAL boundary to IFD0
`AntiAliasStrength` and `BestQualityScale`. The DLL requires one type-5 value
with a nonzero denominator and serializes tags `50738` / `50780` into the
existing TIFF EXIF blob. The facade only enumerates the two native rational
entries during `GetExif()` / `getexif()` materialization; DNG anti-alias/
quality interpretation, public write-side inference, TIFF parsing, and pixel
traversal remain outside AHK.

`META-001DZ` carries the native TIFF scalar-RATIONAL boundary to IFD0
`LinearResponseLimit` and `ChromaBlurRadius`. The DLL requires one type-5
value with a nonzero denominator and serializes tags `50734` / `50737` into
the existing TIFF EXIF blob. The facade only enumerates the two native
rational entries during `GetExif()` / `getexif()` materialization; DNG
response/chroma interpretation, public write-side inference, TIFF parsing,
and pixel traversal remain outside AHK.

`META-001DY` carries the native TIFF scalar-RATIONAL boundary to IFD0
`BaselineNoise` and `BaselineSharpness`. The DLL requires one type-5 value and
serializes tags `50731` / `50732` into the existing TIFF EXIF blob. The facade
only enumerates the two native rational entries during `GetExif()` /
`getexif()` materialization; DNG noise/sharpness interpretation, public write-
side inference, TIFF parsing, and pixel traversal remain outside AHK.

`META-001DX` carries the native TIFF scalar-integer boundary to IFD0
`CalibrationIlluminant1` and `CalibrationIlluminant2`. The DLL requires one
SHORT value and serializes tags `50778` / `50779` into the existing TIFF EXIF
blob. The facade only enumerates the two native integer entries during
`GetExif()` / `getexif()` materialization; DNG calibration interpretation,
public write-side inference, TIFF parsing, and pixel traversal remain outside
AHK.

`META-001DW` carries the native TIFF scalar-integer boundary to IFD0
`CFALayout` and `MakerNoteSafety`. The DLL requires one SHORT value and
serializes tags `50711` / `50741` into the existing TIFF EXIF blob. The facade
only enumerates the two native integer entries during `GetExif()` /
`getexif()` materialization; DNG interpretation, public write-side inference,
TIFF parsing, and pixel traversal remain outside AHK.

`META-001DV` carries the native TIFF ASCII boundary to IFD0
`CameraSerialNumber` and `ProfileCopyright`. The DLL requires type `2`,
validates inline/out-of-line ranges, and copies NUL-terminated values into the
existing TIFF EXIF blob. The facade only enumerates tags `50735` / `50942` for
native ASCII readback; DNG interpretation, public write-side inference, TIFF
parsing, and pixel traversal remain outside AHK.

`META-001DU` carries the native TIFF ASCII boundary to IFD0
`PreviewSettingsName` and `PreviewDateTime`. The DLL requires type `2`,
validates inline/out-of-line ranges, and copies NUL-terminated values into the
existing TIFF EXIF blob. The facade only enumerates tags `50968` / `50971` for
native ASCII readback; DNG interpretation, public write-side inference, TIFF
parsing, and pixel traversal remain outside AHK.

`META-001DT` carries the native TIFF ASCII boundary to IFD0
`PreviewApplicationName` and `PreviewApplicationVersion`. The DLL requires
type `2`, validates inline/out-of-line ranges, and copies NUL-terminated values
into the existing TIFF EXIF blob. The facade only enumerates tags `50966` /
`50967` for native ASCII readback; DNG interpretation, public write-side
inference, TIFF parsing, and pixel traversal remain outside AHK.

`META-001DS` carries the native TIFF ASCII boundary to IFD0
`AsShotProfileName` and `ProfileName`. The DLL requires type `2`, validates
inline/out-of-line ranges, and copies NUL-terminated values into the existing
TIFF EXIF blob. The facade only enumerates tags `50934` / `50936` for native
ASCII readback; DNG interpretation, public write-side inference, TIFF parsing,
and pixel traversal remain outside AHK.

`META-001DR` carries the native TIFF ASCII boundary to IFD0
`CameraCalibrationSignature` and `ProfileCalibrationSignature`. The DLL
requires type `2`, validates inline/out-of-line ranges, and copies
NUL-terminated values into the existing TIFF EXIF blob. The facade only
enumerates tags `50931` / `50932` for native ASCII readback; DNG
interpretation, public write-side inference, TIFF parsing, and pixel traversal
remain outside AHK.

`META-001DQ` carries the native TIFF ASCII boundary to IFD0
`UniqueCameraModel` and `OriginalRawFileName`. The DLL requires type `2`,
validates inline/out-of-line ranges, and copies NUL-terminated values into the
existing TIFF EXIF blob. The facade only enumerates tags `50708` / `50827` for
native ASCII readback; DNG interpretation, public write-side inference, TIFF
parsing, and pixel traversal remain outside AHK.

`META-001DP` carries the native TIFF BYTE-array boundary to IFD0
`LocalizedCameraModel` and `CFAPlaneColor`. The DLL requires type `1`,
validates inline/out-of-line ranges, and copies exact bytes into the existing
TIFF EXIF blob. The facade only enumerates tags `50709` / `50710` for native
byte-array readback; DNG interpretation, public write-side inference, TIFF
parsing, and pixel traversal remain outside AHK.

`META-001DO` carries the native TIFF BYTE-array boundary to IFD0 `DNGVersion`
and `DNGBackwardVersion`. The DLL requires type `1`, validates and copies each
four-byte inline value into the existing TIFF EXIF blob, and the facade only
enumerates tags `50706` / `50707` for native byte-array readback. DNG payload
interpretation, public write-side inference, TIFF parsing, and pixel traversal
remain outside AHK.

`META-001DN` carries the native TIFF type-7 UNDEFINED boundary to IFD0
`UserComment` and `ImageSourceData`. The DLL requires type `7`, validates the
inline/out-of-line payload range, and copies exact bytes into the existing
TIFF EXIF blob. The facade adds tags `37510` / `37724` to native UNDEFINED
readback while retaining the separate type-1 UserComment byte-array/save
lifecycle; no TIFF parsing, payload interpretation, or pixel traversal occurs
in AHK.

`META-001DM` carries the native TIFF UNDEFINED boundary to IFD0 `CFAPattern`
and `DeviceSettingDescription`. The DLL validates both type-7 payloads and
copies their exact inline/out-of-line bytes into the existing TIFF EXIF blob;
the facade only enumerates tags `41730` / `41995` and invokes native UNDEFINED
readback, with no TIFF parsing, payload interpretation, or pixel traversal in
AHK.

`META-001DL` carries the native TIFF UNDEFINED boundary to IFD0
`ComponentsConfiguration` and `MakerNote`. The DLL validates both type-7
payloads and copies their exact inline/out-of-line bytes into the existing
TIFF EXIF blob; the facade only enumerates tags `37121` / `37500` and invokes
native UNDEFINED readback, with no TIFF parsing, MakerNote interpretation, or
pixel traversal in AHK.

`META-001DK` carries the native TIFF UNDEFINED boundary to IFD0 `OECF` and
`SpatialFrequencyResponse`. The DLL validates both type-7 payloads and copies
their exact inline/out-of-line bytes into the existing TIFF EXIF blob; the
facade only enumerates tags `34856` / `41484` and invokes native UNDEFINED
readback, with no TIFF parsing or pixel traversal in AHK.

`META-001DJ` carries the native TIFF scalar-integer boundary to the IFD0
LONG sensitivity family `34865..34869`. The DLL validates count-one LONG
entries and serializes all five exact unsigned values into the existing TIFF
EXIF blob; the facade only enumerates those tag IDs and invokes native integer
readback, with no TIFF parsing or pixel traversal in AHK.

`META-001DI` carries the native TIFF scalar-integer boundary to IFD0
`ISOSpeedRatings` / `PhotographicSensitivity` and `SensitivityType`. The DLL
validates their scalar SHORT entries and serializes both values into the
existing TIFF EXIF blob; the facade only enumerates tags `34855` / `34864`
and invokes native integer readback, with no TIFF parsing or pixel traversal
in AHK.

`META-001DH` completes the native TIFF `SubjectArea` count boundary. The DLL
now validates standard SHORT counts 2/3/4, including inline and out-of-line
payloads, and serializes their unsigned values into the existing TIFF EXIF
blob. The facade reuses its existing tag-37396 native array readback and adds
no parsing or pixel traversal.

`META-001DG` carries the native TIFF SHORT-array boundary to IFD0
`SubjectArea`. The DLL validates the exact four-value shape and serializes its
unsigned values into the existing TIFF EXIF blob; the facade only enumerates
tag `37396` and invokes native ushort-array readback, with no TIFF parsing or
pixel traversal in AHK.

`META-001DF` carries the native TIFF ASCII boundary to IFD0
`SecurityClassification` and `ImageHistory`. The DLL validates and copies both
NUL-terminated values into the existing TIFF EXIF blob; the facade only
enumerates tags `37394` / `37395` and invokes native ASCII readback, with no
TIFF parsing or pixel traversal in AHK.

`META-001DE` carries the native TIFF ASCII boundary to IFD0
`SpectralSensitivity`. The DLL validates and copies the NUL-terminated value
into the existing TIFF EXIF blob; the facade only enumerates tag `34852` and
invokes native ASCII readback, with no TIFF parsing or pixel traversal in AHK.

`META-001DD` carries the native TIFF array boundary to IFD0 `SubjectLocation`.
The DLL validates its exact two-value SHORT shape and serializes the values
into the existing TIFF EXIF blob; the facade only enumerates tag `41492` and
invokes native ushort-array readback, with no TIFF parsing or pixel traversal
in AHK.

`META-001DC` carries the native TIFF scalar-RATIONAL boundary to IFD0
`CompressedBitsPerPixel` and `ExposureIndex`. The DLL validates and serializes
both exact numerator/denominator pairs into the existing TIFF EXIF blob; the
facade only enumerates their tag IDs and never parses TIFF structures or
traverses pixels.

`META-001DB` completes the bounded top-level camera SRATIONAL trio inside the
native TIFF boundary. The DLL validates IFD0 `ExposureBiasValue` and
serializes its exact signed pair into the existing TIFF EXIF blob. The facade
already enumerates tag `37380`, so no new AHK routing or parsing was required.

`META-001DA` keeps TIFF camera-metering metadata inside the native boundary.
The DLL validates IFD0 `ShutterSpeedValue` and `BrightnessValue` scalar
SRATIONAL entries and serializes their exact signed numerator/denominator
pairs into the existing TIFF EXIF blob. The facade only enumerates the two tag
IDs and invokes native signed-rational readback, with no TIFF parsing or pixel
traversal in AHK.

`META-001CZ` carries the same native TIFF boundary to IFD0 `SubjectDistance`
and `FocalLength`. The DLL validates and serializes exact scalar-RATIONAL
pairs into its existing TIFF EXIF blob; the facade only enumerates the two tag
IDs, with no TIFF parsing or pixel traversal in AHK.

`META-001CY` extends that native TIFF metadata boundary to IFD0
`ApertureValue` and `MaxApertureValue`. The DLL validates and serializes both
RATIONAL entries into the existing TIFF EXIF blob; the facade only adds their
IDs to the established rational enumeration and never parses TIFF structures
or traverses pixels.

`META-001CX` keeps TIFF camera-exposure metadata on the same native-first
boundary. The DLL parses IFD0 `ExposureTime` and `FNumber` RATIONAL entries and
serializes their exact numerator/denominator pairs into the existing TIFF EXIF
blob. `Image.Exif.FromImage` only enumerates the two newly supported tag IDs
and materializes the already-bulk-copied metadata; it performs no TIFF parsing
or pixel traversal, and `Info["exif"]` remains absent as in Pillow 11.3.0.

`META-003EL` keeps ICC named-color processing native. One coarse query reads
both colorant-table tags, reports independent presence/count/required slots,
and copies present names into fixed 256-byte native slots. The facade only
performs two-phase buffer routing and materializes/caches metadata-name arrays;
it does not parse ICC structures or cross the DLL once per name.

`META-003EM` keeps CLUT classification native. One call evaluates all four
rendering intents across input/output/proof directions through LittleCMS and
returns a 12-slot matrix. The facade converts that small metadata matrix to the
Pillow-shaped intent-keyed `Map` and caches it; no profile parsing, transform
construction, or pixel traversal occurs in AHK.

`META-003EN` applies the same coarse boundary to intent support. One native
call evaluates all four intents across input/output/proof directions for the
public `intent_supported` map, avoiding twelve facade-to-DLL crossings. The
explicit `is_intent_supported` instance method reuses the established scalar
native query so LittleCMS and native domain validation remain authoritative.

`META-003A` exposes that dependency through the first public `ImageCms`
vertical slice. `pillow_c.dll` allocates and frees an opaque sRGB profile and
queries its description; `ahk/pillow.ahk` owns only argument normalization,
the `CmsProfile` wrapper, and deterministic/idempotent close. No profile or
metadata bytes are traversed in AHK, and the facade never sees `cmsHPROFILE`.

`META-003B` reuses the same deep ownership boundary for built-in LAB
profiles. The facade validates the case-sensitive public profile name and
normalizes the optional numeric color temperature; the DLL validates the
LittleCMS white point, creates the Lab2 profile, owns it, and serves its name.
No new AHK data loop or raw LittleCMS pointer exposure is introduced.

`META-003C` adds XYZ profile creation behind the same boundary. Optional
color temperature remains facade-only ignored input for non-LAB profiles,
matching Pillow; the DLL performs XYZ allocation/name/lifetime work.

`META-003D` crosses the first public ImageCms pixel boundary. The facade
passes one image handle and two opaque profile handles; the DLL builds the
LittleCMS transform, executes all RGB-to-LAB rows, and returns an owned image.
Output ICC serialization is one native bulk query/copy. AHK performs no
per-pixel work and never receives a LittleCMS pointer.

`META-003E` adds the reverse LAB-to-RGB pair inside the same transform
boundary. Internal LAB bytes remain in the established LittleCMS-compatible
representation, so no facade conversion loop is needed; output ICC bytes are
again one native bulk serialization.

`META-003F` extends profile ownership from built-ins to caller ICC memory.
AHK supplies one Buffer crossing; LittleCMS copies it in read mode, and the DLL
owns the resulting profile after the Buffer is released. The facade exposes
the distinct `ImageCmsProfile` wrapper type without exposing memory IO or
native pointers.

`META-003G` adds reusable transform ownership. Profiles are consumed only at
native build time; the opaque transform retains LittleCMS state and output ICC
bytes for repeated allocating apply calls. The facade wrapper owns only the
transform handle, public mode names, argument normalization, and image/Info
lifetime. Repeated pixel traversal remains entirely in the DLL.

`META-003H` makes that ownership boundary bidirectional for the established
LAB/RGB pair. One retained transform now executes LAB-to-RGB rows repeatedly
after both profiles close; the facade uses the stored mode names to route the
same coarse apply call and attaches one bulk-copied sRGB ICC Buffer. No reverse
pixel conversion or profile traversal moves into AHK.

`META-003I` adds RGB-to-RGB same-mode transforms, including profiles reopened
from caller ICC memory. Build consumes profile handles once and retains native
LittleCMS state plus output ICC bytes; allocating repeat apply remains one
coarse DLL call per image after profiles and the source Buffer are gone. AHK
only owns public mode properties, image/Info objects, and transform lifetime.

`META-003J` adds the non-allocating counterpart for RGB-to-RGB. The DLL
refreshes/detaches readonly views and runs LittleCMS with each row as both input
and output, preserving the native image handle. AHK routes one coarse call,
merges the bulk output ICC Buffer into existing Info, and maps Pillow's `None`
return to the established empty value; it never traverses pixels.

`META-003K` carries RGB-to-RGB support into the one-shot profile-to-profile
path. The DLL creates short-lived LittleCMS state, allocates one output image,
and executes complete row traversal. The facade performs only argument
normalization, handle routing, and one bulk output-profile serialization.

`META-003L` adds a coarse one-shot in-place sibling. Transform creation,
readonly-view detachment, same-storage row traversal, and transform deletion
all occur inside one DLL call. The facade only applies Pillow's return contract
and merges one bulk output ICC Buffer into existing Info.

`META-003M` extends retained transform ownership to LAB-to-LAB. D50 and 6500K
profiles are consumed only at build; the DLL retains LittleCMS state and output
ICC bytes, allocates LAB results, and traverses all rows. Public/internal LAB
byte mapping remains native, so the facade performs no channel conversion.

`META-003N` carries LAB-to-LAB support into one-shot profile-to-profile. The
DLL creates and deletes LittleCMS state, allocates the LAB result, and owns
complete row traversal. The facade only routes handles and installs one bulk
output-profile Buffer while preserving source lifetime.

`META-003O` adds the reusable LAB-to-LAB non-allocating sibling. The DLL
detaches mutable storage and executes LittleCMS with identical row pointers;
the facade reuses its generic same-mode in-place route and only merges the
bulk output ICC Buffer into caller Info.

`META-003P` carries LAB-to-LAB into one-shot in-place conversion. Transform
creation/deletion, view detachment, and same-storage LAB row traversal stay
inside one DLL call; the facade only applies return and bulk ICC Info semantics.

`META-003Q` admits relative-colorimetric intent `1` at the reusable transform
build boundary for the established RGB-to-LAB pair. LittleCMS intent selection,
retained transform state, repeat row traversal, and output-profile bytes remain
DLL-owned; the facade only validates numeric intent `0`/`1`, rejects nonzero
flags, and routes the existing coarse build/apply/lifetime calls. One-shot APIs
were intentionally left narrower for their own bounded children.

`META-003R` carries relative-colorimetric intent `1` into allocating one-shot
RGB-to-LAB conversion. The DLL owns temporary LittleCMS state, output image
allocation, every row, and transform deletion; the facade only narrows
admission to this non-in-place mode pair, routes handles, and serializes one
output ICC Buffer. Other one-shot mode pairs remain explicit children rather
than inheriting intent support speculatively.

`META-003S` adds the reverse allocating LAB-to-RGB relative-colorimetric pair
through the same ownership boundary. Exact reverse pixel traversal and the
temporary transform stay in the DLL; the facade expands only the mode-changing
non-in-place intent admission and attaches one 588-byte output ICC Buffer.
Same-mode and in-place intent behavior remain separate work packets.

`META-003T` adds allocating RGB-to-RGB relative-colorimetric conversion with a
memory-opened output profile. Profile memory is copied into DLL-owned profile
state; temporary transform creation, exact identity row traversal, allocation,
and deletion stay native. The facade only extends non-in-place admission and
attaches the already-native-serialized output ICC. LAB same-mode and in-place
intent behavior remain separate.

`META-003U` completes allocating relative-colorimetric admission with the
D50-to-6500K LAB-to-LAB pair. Temporary LittleCMS state, LAB row traversal,
allocation, and deletion remain native; the facade adds only the final non-in-
place mode-pair admission and bulk ICC attachment. Allocating intent `0`/`1`
now spans all four established pairs while in-place behavior remains separate.

`META-003V` carries relative-colorimetric intent `1` into one-shot RGB-to-RGB
in-place conversion. Temporary transform creation/deletion, view detachment,
and identical-pointer row traversal stay inside the DLL. The facade only admits
the covered same-mode route, preserves the image object, returns the Python
`None` analogue, and attaches one bulk 588-byte output ICC Buffer.

`META-003W` completes one-shot in-place relative-colorimetric support with the
D50-to-6500K LAB-to-LAB pair. The same native ownership boundary performs view
detachment and identical-pointer LAB traversal; the facade now treats intent
`0`/`1` uniformly after independent pair and in-place validation, preserving
the object and merging one bulk 572-byte ICC Buffer.

`META-003X` opens saturation intent `2` at the reusable RGB-to-LAB transform
boundary. LittleCMS intent selection, retained state, repeated row traversal,
and output-profile storage remain DLL-owned; the facade narrows admission to
the exact pair and routes the existing coarse build/apply/lifetime calls.

`META-003Y` adds absolute-colorimetric intent `3` through that same retained
RGB-to-LAB boundary. The pair now supports Pillow intents `0..3` without
changing the opaque transform ABI or moving any profile/pixel traversal into
AHK; other pairs retain their independently proven intent bounds.

`META-003Z` carries saturation intent `2` into the allocating one-shot RGB-to-
LAB boundary. Temporary transform creation/deletion, output allocation, and
complete row traversal remain DLL-owned; the facade only admits the exact
non-in-place pair, routes profile/image handles, and attaches one bulk 572-byte
output ICC Buffer. Intent `3` and other one-shot pairs remain independently
bounded.

`META-003AA` completes intents `0..3` for that allocating RGB-to-LAB boundary
with absolute-colorimetric intent `3`. The same coarse native call continues to
own temporary transform lifetime, allocation, and all rows; the facade only
widens argument admission for the exact non-in-place pair and performs the
existing bulk ICC attachment.

`META-003AB` carries saturation intent `2` into the allocating one-shot LAB-to-
RGB reverse boundary. Temporary transform lifetime, output allocation, and
exact reverse row traversal stay in the DLL; the facade only widens admission
for that non-in-place pair and attaches the existing bulk 588-byte sRGB ICC
Buffer.

`META-003AC` completes intents `0..3` across both allocating mode-changing
pairs with LAB-to-RGB absolute-colorimetric intent `3`. Temporary transform
lifetime, output allocation, and exact reverse row traversal remain DLL-owned;
the facade only aligns non-in-place argument admission and keeps bulk ICC
attachment unchanged.

`META-003AD` carries saturation intent `2` into allocating RGB-to-RGB with a
memory-opened output profile. Profile memory is copied into native profile
state; temporary transform lifetime, identity row traversal, allocation, and
deletion remain DLL-owned. The facade only admits the non-in-place pair and
attaches the existing bulk 588-byte ICC Buffer.

`META-003AE` completes intents `0..3` for allocating RGB-to-RGB with absolute-
colorimetric intent `3`. The same DLL boundary continues to own copied profile
state, temporary transform lifetime, allocation, identity row traversal, and
deletion; the facade only aligns non-in-place admission and bulk ICC attachment.

`META-003AF` carries saturation intent `2` into allocating D50-to-6500K LAB-
to-LAB. Temporary transform lifetime, allocation, exact identity LAB row
traversal, and deletion remain DLL-owned; the facade only admits the non-in-
place same-mode pair and attaches the existing bulk 572-byte ICC Buffer.

`META-003AG` completes intents `0..3` across all four allocating one-shot pairs
with D50-to-6500K LAB-to-LAB absolute-colorimetric intent `3`. Temporary
transform lifetime, allocation, exact identity LAB row traversal, and deletion
remain DLL-owned; the facade only aligns non-in-place admission and attaches
the existing bulk 572-byte ICC Buffer.

`META-003AH` opens saturation intent `2` for one-shot RGB-to-RGB in-place
conversion with a memory-opened output profile. Temporary transform lifetime,
view detachment, identical-pointer RGB row traversal, and deletion remain DLL-
owned; the facade only widens exact in-place admission, retains the image
handle, and attaches one bulk 588-byte ICC Buffer.

`META-003AI` completes intents `0..3` for that one-shot RGB-to-RGB in-place
boundary with absolute-colorimetric intent `3`. The same native call continues
to own temporary transform lifetime, view detachment, identical-pointer row
traversal, and deletion; the facade only aligns exact in-place admission and
bulk ICC attachment.

`META-003AJ` opens saturation intent `2` for one-shot D50-to-6500K LAB-to-LAB
in-place conversion. Temporary transform lifetime, view detachment, identical-
pointer LAB row traversal, and deletion remain DLL-owned; the facade only
widens exact in-place admission, retains the image handle, and attaches one
bulk 572-byte ICC Buffer.

`META-003AK` completes intents `0..3` across both legal one-shot in-place pairs
with D50-to-6500K LAB-to-LAB absolute-colorimetric intent `3`. The same native
call continues to own temporary transform lifetime, view detachment,
identical-pointer row traversal, and deletion; the facade only aligns exact in-
place admission and bulk ICC attachment.

`META-003AL` opens saturation intent `2` for reusable LAB-to-RGB transforms.
Retained LittleCMS state, repeated reverse row traversal, result allocation,
serialized output-profile bytes, and deletion remain DLL-owned; the facade only
widens exact build admission and routes coarse build/apply/lifetime calls.

`META-003AM` completes intents `0..3` across both mode-changing reusable pairs
with LAB-to-RGB absolute-colorimetric intent `3`. The same native transform
boundary owns retained state, repeated reverse row traversal, allocation,
serialized profile bytes, and deletion; the facade only aligns exact build
admission.

`META-003AN` opens saturation intent `2` for reusable RGB-to-RGB transforms
with a memory-opened output profile. Copied profile state, retained LittleCMS
state, repeated identity row traversal, result allocation, serialized output-
profile bytes, and deletion remain DLL-owned; the facade only widens exact
build admission and routes coarse transform calls.

`META-003AO` completes intents `0..3` for that reusable RGB-to-RGB boundary
with absolute-colorimetric intent `3`. The same native transform boundary owns
copied profile state, retained state, repeated identity row traversal,
allocation, serialized profile bytes, and deletion; the facade only aligns
exact build admission.

`META-003AP` opens saturation intent `2` for reusable D50-to-6500K LAB-to-LAB
transforms. Retained LittleCMS state, repeated identity LAB row traversal,
result allocation, serialized output-profile bytes, and deletion remain DLL-
owned; the facade only widens exact build admission and routes coarse transform
calls.

`META-003AQ` completes intents `0..3` across all four established reusable
pairs with D50-to-6500K LAB-to-LAB absolute-colorimetric intent `3`. The same
native transform boundary owns retained state, repeated identity row traversal,
allocation, serialized profile bytes, and deletion; the facade only aligns
exact build admission.

`META-003AR` opens the first bounded nonzero-flag route at the reusable
transform boundary: RGB/sRGB-to-LAB/LAB, perceptual intent `0`, and
LittleCMS/Pillow black-point-compensation flag `8192` (`0x2000`). Retained
LittleCMS state, repeated row traversal, result allocation, serialized output-
profile bytes, and deletion remain DLL-owned. The facade only admits the exact
pair/intent/flag combination and routes coarse build/apply/lifetime calls; all
other nonzero-flag surfaces remain explicit children.

`META-003AS` mirrors that bounded perceptual/BPC route for reusable
LAB/LAB-to-RGB/sRGB transforms. Retained reverse-transform state, repeated LAB
row traversal, RGB allocation, serialized sRGB profile bytes, and deletion
remain DLL-owned. The facade only adds the exact reverse pair to admission;
same-mode and other flag surfaces remain explicit children.

`META-003AT` extends reusable perceptual/BPC admission to RGB/RGB with a
memory-opened output sRGB profile. Copied profile state, retained transform
state, repeated identity traversal, RGB allocation, serialized output-profile
bytes, and deletion remain DLL-owned. The facade adds only the exact same-mode
pair while preserving caller Buffer/profile lifetime independence.

`META-003AU` completes reusable perceptual/BPC admission across all four
established pairs with D50-to-6500K LAB/LAB. Retained transform state, repeated
identity LAB traversal, LAB allocation, serialized output-profile bytes, and
deletion remain DLL-owned. The facade adds only the final same-mode pair; one-
shot and other-intent flag surfaces remain explicit children.

`META-003AV` opens reusable relative-colorimetric/BPC admission for
RGB/sRGB-to-LAB/LAB. Retained transform state, repeated RGB-to-LAB traversal,
LAB allocation, serialized output-profile bytes, and deletion remain DLL-
owned. The facade adds only the exact pair/intent/flag combination; remaining
relative and one-shot flag surfaces stay explicit children.

`META-003AW` mirrors reusable relative-colorimetric/BPC admission for
LAB/LAB-to-RGB/sRGB. Retained reverse-transform state, repeated LAB traversal,
RGB allocation, serialized output-profile bytes, and deletion remain DLL-
owned. The facade adds only the reverse pair; same-mode relative and one-shot
flag surfaces remain explicit children.

`META-003AX` opens reusable same-mode relative-colorimetric/BPC admission for
RGB/sRGB-to-RGB/memory-opened-sRGB. Retained transform state, repeated RGB
traversal, RGB allocation, serialized output-profile bytes, and deletion stay
DLL-owned after the source profile Buffer and both profile handles are
released. The facade adds only the exact pair/intent/flag route; LAB/LAB
relative and one-shot flag surfaces remain explicit children.

`META-003AY` completes reusable relative-colorimetric/BPC admission across all
four established pairs with D50-to-6500K LAB/LAB. Retained transform state,
repeated LAB traversal, LAB allocation, serialized output-profile bytes, and
deletion remain DLL-owned after both profiles release. The facade adds only
the final exact pair/intent/flag route; saturation/absolute and one-shot flag
surfaces remain explicit children.

`META-003AZ` opens reusable saturation/BPC admission for RGB/sRGB-to-LAB/LAB.
Retained transform state, repeated RGB-to-LAB traversal, LAB allocation,
serialized output-profile bytes, and deletion remain DLL-owned after both
profiles release. The facade adds only the exact pair/intent/flag route;
remaining saturation, absolute, and one-shot flag surfaces stay explicit
children.

`META-003BA` mirrors reusable saturation/BPC admission for LAB/LAB-to-RGB/sRGB,
completing both mode-changing pairs. Retained reverse-transform state,
repeated LAB traversal, RGB allocation, serialized output-profile bytes, and
deletion remain DLL-owned after both profiles release. The facade adds only
the reverse pair; same-mode saturation, absolute, and one-shot flag surfaces
stay explicit children.

`META-003BB` opens reusable same-mode saturation/BPC admission for RGB/sRGB-to-
RGB/memory-opened-sRGB. Retained transform state, repeated RGB traversal, RGB
allocation, serialized output-profile bytes, and deletion stay DLL-owned after
the source profile Buffer and both profile handles are released. The facade
adds only the exact pair/intent/flag route; LAB/LAB saturation, absolute, and
one-shot flag surfaces remain explicit children.

`META-003BC` completes reusable saturation/BPC admission across all four
established pairs with D50-to-6500K LAB/LAB. Retained transform state, repeated
LAB traversal, LAB allocation, serialized output-profile bytes, and deletion
remain DLL-owned after both profiles release. The facade adds only the final
exact pair/intent/flag route; absolute-colorimetric and one-shot flag surfaces
remain explicit children.

`META-003BD` opens reusable absolute-colorimetric/BPC admission for RGB/sRGB-
to-LAB/LAB. Retained transform state, repeated RGB-to-LAB traversal, LAB result
allocation, serialized output-profile bytes, and deletion remain DLL-owned
after both profiles release. The facade adds only the exact RGB-to-LAB intent-
3/flag route; the other three reusable pairs and one-shot flag surfaces remain
explicit children.

`META-003BE` extends reusable absolute-colorimetric/BPC admission to LAB/LAB-
to-RGB/sRGB, completing both mode-changing pairs. Retained transform state,
repeated LAB-to-RGB traversal, RGB result allocation, serialized output-profile
bytes, and deletion remain DLL-owned after both profiles release. The facade
adds only the reverse exact pair/intent/flag route; both same-mode reusable
pairs and one-shot flag surfaces remain explicit children.

`META-003BF` extends reusable absolute-colorimetric/BPC admission to RGB/sRGB-
to-RGB/memory-opened-sRGB. Retained transform state, repeated identity RGB
traversal, RGB result allocation, serialized output-profile bytes, and deletion
remain DLL-owned after the source profile Buffer and both profile handles are
released. The facade adds only this same-mode pair/intent/flag route; LAB/LAB
and one-shot flag surfaces remain explicit children.

`META-003BG` completes reusable absolute-colorimetric/BPC admission across all
four established pairs with D50-to-6500K LAB/LAB. Retained transform state,
repeated identity LAB traversal, LAB result allocation, serialized output-
profile bytes, and deletion remain DLL-owned after both profiles release. The
facade adds only the final exact pair/intent/flag route. All reusable pairs now
support intents `0..3` plus BPC; one-shot/in-place flags remain separate.

`META-003BH` opens allocating one-shot perceptual/BPC admission for RGB/sRGB-
to-LAB/LAB. The DLL constructs and deletes LittleCMS state, allocates the LAB
result, and executes every row in one call. The facade only admits the exact
non-in-place pair/intent/flag and attaches one bulk-serialized ICC Buffer; it
does not traverse pixels. Other one-shot pairs/intents and in-place flags remain
explicit children.

`META-003BI` completes the two mode-changing one-shot perceptual/BPC routes by
adding LAB/LAB-to-RGB/sRGB. The same coarse DLL call constructs and deletes
LittleCMS state, allocates the RGB result, and executes every row before either
profile can be released. The facade expands only the exact non-in-place pair/
intent/flag admission and attaches one bulk-serialized sRGB ICC Buffer; it
performs no pixel traversal. Same-mode one-shot pairs, other intents/flags, and
in-place BPC remain explicit children.

`META-003BJ` adds the RGB/sRGB-to-RGB/memory-opened-sRGB same-mode one-shot
perceptual/BPC route. The output profile copies caller ICC memory into native
LittleCMS ownership before the call; the DLL then creates transient transform
state, allocates and fills the identity RGB result, and destroys transform
state in one crossing. The facade only admits this pair and attaches a bulk
serialized ICC Buffer, so profile memory, both profiles, and the result all
retain independent lifetimes without an AHK pixel loop.

`META-003BK` completes one-shot perceptual/BPC admission across all four
established pairs with D50-to-6500K LAB/LAB. Transient LittleCMS state, LAB
result allocation, every row, and transform deletion remain inside one DLL
call, and the result survives both profile releases. The facade adds only the
final exact non-in-place pair/intent/flag and one bulk-serialized LAB ICC
Buffer; other one-shot intents and all in-place BPC behavior remain explicit
children.

`META-003BL` opens one-shot relative-colorimetric/BPC admission with RGB/sRGB-
to-LAB/LAB. The existing coarse DLL call owns temporary LittleCMS state, LAB
result allocation, complete row traversal, and transform deletion; the result
survives both profile releases while the source remains unchanged. The facade
only admits this exact non-in-place pair/intent/flag and attaches one bulk-
serialized LAB ICC Buffer. Other relative one-shot pairs and all in-place BPC
behavior remain explicit children.

`META-003BM` adds the reverse LAB/LAB-to-RGB/sRGB relative-colorimetric/BPC
route, completing the two mode-changing one-shot pairs. The DLL owns temporary
LittleCMS state, RGB result allocation, every row, and deletion in the same
coarse call; the result survives both profile releases and source storage is
unchanged. The facade only extends exact admission and bulk-attaches the sRGB
ICC Buffer. Same-mode relative one-shot pairs and all in-place BPC behavior
remain explicit children.

`META-003BN` adds RGB/sRGB-to-RGB/memory-opened-sRGB relative-colorimetric/BPC.
The output profile copies caller ICC memory into native LittleCMS ownership;
the DLL then creates transient transform state, allocates and fills the RGB
identity result, and destroys state in one crossing. The facade adds only this
exact admission and bulk ICC attachment, preserving independent profile
memory/profile/result lifetimes without pixel traversal. LAB/LAB relative and
all in-place BPC behavior remain explicit children.

`META-003BO` completes one-shot relative-colorimetric/BPC admission across all
four established pairs with D50-to-6500K LAB/LAB. Transient LittleCMS state,
LAB result allocation, every row, and transform deletion remain inside one DLL
call, and the result survives both profile releases. The facade adds only the
final exact non-in-place pair/intent/flag and one bulk-serialized LAB ICC
Buffer; saturation/absolute one-shot and all in-place BPC behavior remain
explicit children.

`META-003BP` opens one-shot saturation/BPC admission with RGB/sRGB-to-LAB/LAB.
The existing coarse DLL call owns temporary LittleCMS state, LAB result
allocation, complete row traversal, and transform deletion; the result survives
both profile releases while the source remains unchanged. The facade only
admits this exact non-in-place pair/intent/flag and attaches one bulk-serialized
LAB ICC Buffer. Other saturation one-shot pairs and all in-place BPC behavior
remain explicit children.

`META-003BQ` adds the reverse LAB/LAB-to-RGB/sRGB saturation/BPC route,
completing both mode-changing one-shot pairs. Temporary LittleCMS state, RGB
result allocation, complete row traversal, and transform deletion remain in one
coarse DLL call; the result survives both profile releases while LAB source
storage and caller Info stay unchanged. The facade extends only exact admission
and bulk-attaches the serialized sRGB ICC Buffer. Same-mode saturation and all
in-place BPC behavior remain explicit children.

`META-003BR` adds RGB/sRGB-to-RGB/memory-opened-sRGB saturation/BPC. Caller ICC
memory is copied into native profile ownership, then temporary LittleCMS state,
identity RGB result allocation, all row traversal, and transform deletion stay
inside one coarse DLL call. The result survives profile memory and both profile
releases; the facade adds only exact admission and one bulk ICC attachment.
LAB-to-LAB saturation and all in-place BPC behavior remain explicit children.

`META-003BS` completes one-shot saturation/BPC across all four established
pairs with D50-to-6500K LAB/LAB. Temporary LittleCMS state, identity LAB result
allocation, all row traversal, and transform deletion stay in one coarse DLL
call, and the result survives both profile releases. The facade adds only the
final exact admission and one bulk LAB ICC attachment. Absolute one-shot and
all in-place BPC behavior remain explicit children.

`META-003BT` opens one-shot absolute-colorimetric/BPC admission with RGB/sRGB-
to-LAB/LAB. Temporary LittleCMS state, LAB result allocation, every row, and
transform deletion remain inside one coarse DLL call; the result survives both
profile releases while source pixels and caller Info remain unchanged. The
facade adds only exact admission and one bulk LAB ICC attachment. The next BU
slice adds the reverse pair; same-mode absolute/BPC and all in-place BPC
behavior remain explicit children.

`META-003BU` extends that one-shot absolute-colorimetric/BPC route to LAB/LAB-
to-RGB/sRGB, completing both mode-changing pairs. Temporary LittleCMS state,
RGB result allocation, every row, and transform deletion remain inside one
coarse DLL call; the result survives both profile releases while source pixels
and caller Info remain unchanged. The facade adds only exact admission and one
bulk sRGB ICC attachment. Same-mode absolute/BPC and all in-place BPC behavior
remain explicit children.

`META-003BV` adds one-shot RGB/sRGB-to-RGB/memory-opened-sRGB absolute-
colorimetric/BPC. Profile memory is copied into the native profile handle;
temporary LittleCMS state, identity RGB allocation, every row, and transform
deletion remain inside one coarse DLL call. The result survives profile memory
and both profile releases; the facade adds only exact admission and one bulk
sRGB ICC attachment. LAB-to-LAB absolute/BPC and all in-place BPC behavior
remain explicit children.

`META-003BW` adds one-shot D50-to-6500K LAB/LAB absolute-colorimetric/BPC and
closes intents `0..3` plus BPC across the four established allocating pairs.
Temporary LittleCMS state, identity LAB allocation, every row, and transform
deletion stay inside one coarse DLL call; the result survives both profile
releases. The facade adds only the final exact admission and one bulk LAB ICC
attachment. All in-place BPC behavior remains an explicit separate family.

`META-003BX` opens that in-place BPC family with RGB/sRGB-to-RGB/memory-opened-
sRGB at perceptual intent `0`. The DLL builds one temporary LittleCMS transform,
detaches a buffer view when required, and transforms every RGB row against the
same native storage before deleting the transform. The image handle and owned
data pointer remain stable for ordinary owned images. The facade only admits
the exact pair/intent/flag, maps Pillow's `None` to `""`, preserves caller Info,
and attaches one bulk 588-byte sRGB ICC Buffer. LAB and other in-place BPC
intents remain explicit separate children.

`META-003BY` completes perceptual in-place BPC across both established same-
mode pairs with D50-to-6500K LAB/LAB. The same DLL boundary builds and deletes
temporary LittleCMS state while transforming every LAB row against unchanged
native storage; the image handle/data pointer and exact identity bytes remain
stable after both profiles close. The facade expands only the exact intent-0
BPC admission, preserves caller Info, maps `None` to `""`, and attaches one
bulk 572-byte LAB ICC Buffer. In-place BPC intents `1..3` remain separate.

`META-003BZ` opens relative-colorimetric in-place BPC with RGB/sRGB-to-RGB/
memory-opened-sRGB at intent `1`. The DLL admits that exact intent/flag pair,
builds and deletes temporary LittleCMS state, and transforms every RGB row
against the same native storage while preserving the image handle/data pointer.
The facade only extends the matching pair/intent/flag admission, preserves
caller Info, maps `None` to `""`, and attaches one bulk 588-byte sRGB ICC
Buffer. LAB/LAB relative BPC and intents `2..3` remain explicit children.

`META-003CA` completes relative-colorimetric in-place BPC across both
established same-mode pairs with D50-to-6500K LAB/LAB at intent `1`. Temporary
LittleCMS state and every identical-pointer LAB row remain DLL-owned while the
image handle/data pointer and exact identity bytes stay stable after both
profiles close. The facade adds only the matching LAB admission, existing
`None` mapping, caller Info preservation, and one bulk 572-byte ICC Buffer.
In-place saturation and absolute-colorimetric BPC remain explicit children.

`META-003CB` opens saturation in-place BPC with RGB/sRGB-to-RGB/memory-opened-
sRGB at intent `2`. Temporary transform lifetime and every identical-pointer
RGB row stay DLL-owned while the source profile Buffer, profile handles, image
handle, and data-pointer lifetimes remain independent. The facade adds only
the exact RGB admission, existing `None` mapping, caller Info preservation,
and one bulk 588-byte ICC Buffer. LAB/LAB saturation and absolute BPC remain
explicit children.

`META-003CC` completes saturation in-place BPC across both established same-
mode pairs with D50-to-6500K LAB/LAB at intent `2`. Temporary LittleCMS state
and every identical-pointer LAB row remain DLL-owned while image storage and
profile lifetimes stay independent. The facade adds only the matching LAB
admission, existing `None` mapping, caller Info preservation, and one bulk
572-byte ICC Buffer. Absolute-colorimetric in-place BPC remains separate.

`META-003CD` opens absolute-colorimetric in-place BPC with RGB/sRGB-to-RGB/
memory-opened-sRGB at intent `3`. Temporary transform lifetime and every
identical-pointer RGB row remain DLL-owned while source profile memory,
profile handles, image handle, and data pointer remain independent. The facade
adds only the exact RGB admission, existing `None` mapping, caller Info
preservation, and one bulk 588-byte ICC Buffer. LAB/LAB absolute BPC remains
an explicit child.

`META-003CE` completes absolute-colorimetric in-place BPC across both
established same-mode pairs with D50-to-6500K LAB/LAB at intent `3`.
Temporary LittleCMS state and every identical-pointer LAB row remain DLL-owned
while image storage and profile lifetimes stay independent. The facade adds
only the matching LAB admission, existing `None` mapping, caller Info
preservation, and one bulk 572-byte ICC Buffer. Both same-mode pairs now cover
intents `0..3` plus BPC; proofing and broader ImageCms surfaces remain separate.

`META-003CF` opens reusable proofing with a dedicated
`pillow_c_cms_proof_transform_build` ABI for bounded RGB/sRGB input, output,
and proof profiles using perceptual render intent, absolute-colorimetric proof
intent, and `SOFTPROOFING`. LittleCMS proof-transform construction, serialized
output-profile ownership, repeated whole-image application, and all RGB rows
remain DLL-owned. The facade only validates the exact defaults, creates the
existing transform wrapper, and reuses bulk apply/ICC routing. In-place proof
application, gamut checking, and broader proof matrices remain separate.

`META-003CG` adds the first mode-changing proof route, RGB/sRGB-to-LAB/LAB,
without changing the ABI. The DLL selects LittleCMS RGB and Lab8 formatters,
owns every row and the 572-byte output ICC serialization, and keeps the
transform usable after profile and serialized-memory release. The facade only
admits the mode pair and retains the existing coarse build/apply boundary.

`META-003CH` adds the reverse LAB/LAB-to-RGB/sRGB proof route. Repeated native
apply produces Pillow's exact bounded RGB bytes and distinct 588-byte ICC
Buffers while source pixels and Info remain unchanged. Profile-memory and
transform lifetime stay below the same opaque ownership boundary.

`META-003CI` completes default soft-proofing across all four established
RGB/LAB mode pairs with D50-to-6500K LAB/LAB and an sRGB proof profile. The
native builder selects Lab8 for both sides, repeated apply remains identity for
the bounded Pillow oracle, and the facade performs no pixel traversal. Raw ABI
tests now pin `pillow_c.dll` for the process, matching the facade's existing
module-lifetime rule so opaque handles never cross an AutoHotkey implicit DLL
unload/reload boundary.

`META-003CJ` through `META-003CM` extend that same native proof-transform
boundary to relative-colorimetric render intent `1` across all four established
RGB/LAB mode pairs. LittleCMS still owns transform construction and every
repeat-apply row; output-profile serialization remains a single native bulk
copy. The facade only admits the new intent for the established pairs and
retains profile, transform, image, and Info lifetime routing without adding
pixel loops. Saturation/absolute intents, other flags, in-place proof apply,
gamut checking, and broader modes remain separate gaps.

`META-003CN` through `META-003CQ` repeat that bounded matrix for saturation
render intent `2`. All four established RGB/LAB pairs now run intents `0..2`
through the same opaque LittleCMS transform, native row traversal, and bulk ICC
ownership path. AHK widens only constructor admission; no transform or pixel
work moves into the facade. Absolute intent remained separate at that boundary.

`META-003CR` opens reusable absolute-colorimetric soft proofing with the bounded
RGB/sRGB-to-RGB/sRGB pair at render intent `3`, proof intent `3`, and
`SOFTPROOFING=16384`. The existing native proof builder owns transform
construction, every repeat-apply row, output allocation, serialized ICC bytes,
and lifetime after all profile objects and their source memory are released.
The facade only admits this exact mode/intent combination and attaches the
native output-profile Buffer; other absolute mode pairs, flags, in-place proof
apply, gamut checking, and broader modes remain separate gaps.

`META-003CS` extends the same absolute-colorimetric native route from RGB/sRGB
input to LAB/LAB output. LittleCMS selects `TYPE_RGB_8` and `TYPE_Lab_8`, owns
all repeat-apply rows and output allocation, and serializes a 572-byte output
profile after the original profiles and proof-profile memory are released.
AHK widens only the exact RGB-to-LAB constructor admission; LAB-input pairs,
other flags, in-place proof apply, and gamut checking remain separate.

`META-003CT` adds the reverse LAB/LAB-to-RGB/sRGB absolute soft-proof route.
The opaque native transform selects Lab8 input and RGB8 output, owns exact
repeat-apply traversal plus 588-byte ICC serialization, and remains usable
after input/output/proof profile release. The facade only widens constructor
admission and lifetime routing; LAB/LAB output is the remaining matrix gap.

`META-003CU` completes absolute-colorimetric soft proofing with D50-to-6500K
LAB/LAB. Reusable proof transforms now cover render intents `0..3` across all
four established RGB/LAB pairs through one opaque LittleCMS builder and the
same native apply/output-profile/free paths. AHK performs only mode/intent
admission and object lifetime work.

`META-003CV` opens reusable gamut-check soft proofing for the bounded D50-to-
6500K LAB/LAB pair at render intent `0`, proof intent `3`, and
`SOFTPROOFING|GAMUTCHECK=20480` (`0x5000`). LittleCMS owns alarm-color
classification and every row: out-of-gamut samples become its default Lab8
alarm `[127,255,255]`, while the bounded in-gamut sample remains unchanged.
The facade widens only this exact constructor admission and keeps profile,
transform, image, Info, and output-ICC lifetimes above the opaque ABI. Other
gamut-check mode pairs and intents, explicit alarm-code configuration,
proof-transform in-place application, path/file-like inputs, and broader modes
remain explicit later gaps.

`META-003CW` extends that exact gamut-check boundary from LAB/LAB output to
RGB/sRGB output. LittleCMS maps the same two out-of-gamut LAB samples to its
default RGB8 alarm `[127,127,127]`, preserves the other bounded conversions,
owns output allocation and every row, and serializes a 588-byte sRGB profile.
The facade only admits LAB-to-RGB at intent `0` with flags `0x5000`; other
gamut-check intents and mode pairs remain separate.

`META-003CX` admits the adjacent relative-colorimetric render intent `1` for
that same LAB/LAB-to-RGB/sRGB gamut-check route. The native proof transform
retains identical bounded RGB alarm and lifetime behavior; AHK changes only
constructor admission. Saturation/absolute gamut-check intents and other mode
pairs remain separate.

`META-003CY` admits saturation render intent `2` for that same LAB/LAB-to-
RGB/sRGB gamut-check route. LittleCMS retains exact RGB alarm output,
allocation, row traversal, ICC serialization, and profile-independent
lifetime; the facade widens only the intent guard. Absolute intent and other
gamut-check mode pairs remain separate.

`META-003CZ` admits absolute-colorimetric render intent `3` for that same
LAB/LAB-to-RGB/sRGB gamut-check route, completing intents `0..3` for this
mode pair. LittleCMS continues to own gamut classification, exact RGB alarm
rows, result allocation, ICC serialization, and transform lifetime; the AHK
facade widens only the exact constructor guard. Other gamut-check mode pairs
remain separate.

`META-003DA` extends D50-to-6500K LAB/LAB gamut checking from perceptual
intent `0` to relative-colorimetric intent `1`. LittleCMS owns exact Lab8
alarm rows, transform allocation, row traversal, ICC serialization, and
profile-independent lifetime; the facade adds only the matching constructor
admission. Saturation/absolute intents and other mode pairs remain separate.

`META-003DB` adds saturation intent `2` to that same D50-to-6500K LAB/LAB
gamut-check route. Exact Lab8 alarm rows and every hot/lifetime-critical path
remain LittleCMS/DLL-owned; AHK changes only the bounded constructor guard.
Absolute intent and other gamut-check mode pairs remain separate.

`META-003DC` adds absolute-colorimetric intent `3`, completing intents `0..3`
for D50-to-6500K LAB/LAB gamut checking and, together with CW-CZ, for both
established LAB-input proof mode pairs. LittleCMS/DLL ownership is unchanged;
RGB-input gamut-check mode pairs remain separate.

`META-003DD` opens RGB-input gamut checking with an sRGB-to-sRGB perceptual
intent `0` identity route. Profile-memory-independent transform ownership,
all RGB rows, result allocation, and ICC serialization stay native; the
facade only admits the exact constructor tuple. Other RGB-input intents and
RGB-to-LAB remain separate.

`META-003DE` adds relative-colorimetric intent `1` to the same RGB/sRGB-to-
RGB/sRGB identity gamut-check route. Native ownership and the facade boundary
remain unchanged; saturation/absolute intents and RGB-to-LAB stay separate.

`META-003DF` adds saturation intent `2` to the same RGB/sRGB-to-RGB/sRGB
identity gamut-check route. LittleCMS remains the native row processor and
transform owner; the facade only admits the exact tuple. Absolute intent and
RGB-to-LAB gamut checking stay separate.

`META-003DG` adds absolute-colorimetric intent `3` and completes render intents
`0..3` for RGB/sRGB-to-RGB/sRGB identity gamut checking. Native LittleCMS
continues to own row traversal, allocation, output-profile serialization, and
transform lifetime; RGB-to-LAB gamut checking stays separate.

`META-003DH` opens RGB/sRGB-to-LAB/LAB gamut checking at perceptual intent
`0`. The existing native proof-transform owner now emits LAB rows and 572-byte
output profiles for this exact tuple; the facade only normalizes and validates
arguments. Later RGB-to-LAB intents remain separate.

`META-003DI` adds relative-colorimetric intent `1` to that RGB/sRGB-to-LAB/LAB
gamut-check route. Native ownership and facade responsibilities stay
unchanged; saturation and absolute intents remain separate.

`META-003DJ` adds saturation intent `2` to the same RGB/sRGB-to-LAB/LAB gamut-
check route. LittleCMS still owns every LAB row, result allocation, serialized
output profile, and transform lifetime; absolute intent remained separate.

`META-003DK` adds absolute-colorimetric intent `3` to that RGB/sRGB-to-LAB/LAB
gamut-check route. All four established RGB/LAB proof mode pairs now support
gamut-check render intents `0..3`; LittleCMS remains the sole owner of gamut
classification, row traversal, result allocation, output-profile
serialization, and transform lifetime.

`META-003DL` adds a narrow profile-header query to the native CMS owner.
`pillow_c_cms_profile_default_intent` reads the rendering intent directly from
the LittleCMS profile handle; `ImageCms.getDefaultIntent` performs only object
validation, one ABI call, and integer return. Serialized profile memory is not
retained by the facade or needed after a memory-opened profile is created.

`META-003DM` adds the adjacent profile capability query. The DLL validates the
bounded intent/direction domain and delegates the complete support decision to
LittleCMS `cmsIsIntentSupported`; the facade performs one call and maps native
boolean `0/1` to Pillow's public `-1/1` convention. No support matrix is
duplicated in AHK.

`META-003DN` keeps combined profile-information composition inside the native
CMS owner. The DLL queries LittleCMS description/copyright fields and emits
Pillow's exact CRLF-separated UTF-8 text through one two-pass ABI; the facade
only validates the opaque profile, makes two coarse calls, and decodes the
result. No profile metadata is split or recomposed in AHK.

`META-003DO` exposes the adjacent public copyright field through its own native
query. LittleCMS reads `cmsInfoCopyright` and the DLL appends Pillow's trailing
LF; the facade again performs only object validation, two coarse ABI calls,
and UTF-8 decoding. Built-in and memory-opened profiles remain independent of
the serialized source Buffer.

`META-003DP` adds the profile manufacturer field while preserving Pillow's
explicit missing-tag semantics. LittleCMS reports the optional sRGB
manufacturer tag absent; the DLL maps that proven condition to Pillow's exact
empty-field LF result and owns the two-pass UTF-8 output. The facade still only
validates, calls, and decodes, and no optional-tag rule is duplicated in AHK.

`META-003DQ` applies the same native ownership rule to the distinct public
profile model field. LittleCMS reads `cmsInfoModel`; when the optional sRGB tag
is absent, the DLL emits Pillow's exact empty-field LF. The facade retains only
profile validation, two coarse calls, and UTF-8 decoding.

`META-003DR` exposes Pillow's distinct profile-description method without
duplicating either metadata work or ABI calls. The facade delegates to the
existing native-backed profile-name route because both public methods read the
same LittleCMS description field and append the same LF. Description bytes,
UTF-8 serialization, and memory-opened profile lifetime remain native-owned.

`META-003DS` extends profile creation to an actual ASCII ICC path without
moving file I/O or profile bytes into AHK. The DLL delegates opening and file
lifetime to LittleCMS, so the Windows source-file lock lasts exactly as long as
the native profile handle. The facade contributes only String routing,
`filename` preservation, handle wrapping, and existing close lifetime.

`META-003DT` preserves Pillow's separate Windows non-ASCII path strategy. A
UTF-16 DLL entry reads the complete ICC file, closes it, and opens LittleCMS
from native memory, making source deletion independent of profile lifetime.
The facade only classifies ASCII versus non-ASCII String routing and maps the
non-ASCII branch's `filename` to None; file bytes never traverse AHK.

`META-003DU` handles an actual caller-owned AHK File as facade argument
normalization: one bulk read advances the stream to EOF without closing it,
then the established native memory-open route owns the resulting profile.
This is not a metadata or pixel loop; profile parsing, bytes, and lifetime stay
inside LittleCMS/DLL after the single buffer handoff.

`META-003DV` replaces single-owner CMS wrapper assumptions with native atomic
reference ownership. `getOpenProfile(CmsProfile)` retains and wraps the exact
same pointer, exposes the source object as `profile`, and never serializes or
clones ICC bytes. Each AHK wrapper releases one reference, so closing the source
or public wrapper first cannot invalidate the other.

`META-003DW` keeps serialization ownership unchanged: the public
`ImageCmsProfile.tobytes()` spelling is a facade alias over the existing
two-call native profile-byte query. LittleCMS/DLL still owns ICC serialization,
the facade allocates one result Buffer, and no AHK metadata or pixel loop is
introduced. The low-level `CmsProfile` deliberately remains without `tobytes()`.

`META-003DX` exposes low-level sRGB profile attributes without duplicating ICC
parsing in AHK. Property access routes to the established coarse native
description/copyright/manufacturer/model/default-intent queries; the facade only
removes high-level helper newlines and maps absent optional strings to the
project's None value.

`META-003DY` deepens the same native-owned profile model with one immutable
header query. LittleCMS supplies signatures, versions, and matrix-shaper state;
the facade converts four-byte signatures once and caches the six-value header
object only after checking that the native handle remains live.

`META-003DZ` keeps profile colorimetry in the DLL. The native wrapper records
whether LittleCMS created the sRGB profile directly, because Pillow exposes
nominal D50 for that object but ICC fixed-point values after serialization and
reopen. The media-white-point export reads/derives XYZ and xyY natively; the
facade only constructs and caches the public nested Array value.

`META-003EA` reuses that native provenance boundary for correlated color
temperature. The DLL obtains the same nominal-or-serialized white point,
converts XYZ to xyY, and runs LittleCMS temperature conversion in one scalar
query. The facade only checks lifetime, dispatches once, and caches the public
number; ICC parsing and color math never move into AHK.

`META-003EB` batches the three sRGB colorant tags at the same boundary. One
native call reads red, green, and blue XYZ values and derives all three xyY
triples, naturally preserving live built-in precision versus serialized ICC
fixed-point precision. The facade performs one coarse crossing and caches three
small nested Arrays; it does not parse profile bytes or perform color math.

`META-003EC` keeps chromatic-adaptation parsing and derivation native as well.
The DLL reads the nine-double LittleCMS `chad` tag, treats its rows as XYZ
triples, and derives the matching xyY rows before one bulk return. The facade
only assembles the two public 3x3 Arrays and caches after a live-handle check;
it never synthesizes a missing matrix or computes chromaticity.

`META-003ED` follows Pillow's distinct primary calculation instead of aliasing
colorant tags. The DLL creates a temporary XYZ profile and transforms all three
RGB double unit vectors at relative intent with optimizations/cache disabled,
then derives xyY and returns the three primaries in one batch. The facade only
normalizes lifetime and caches the three nested public Arrays.

`META-003EE` keeps optional profile-tag interpretation inside the same native
boundary. One batch independently reads the LittleCMS media-black-point and
luminance tags, derives xyY only for present XYZ payloads, and returns explicit
presence slots without synthesizing defaults. The facade caches both public
properties after one coarse call and maps absent values to the established AHK
`None` value; profile parsing and color math remain outside AHK.

`META-003EF` reads the distinct ICC chromaticity tag directly instead of
deriving it from colorants or transformed primaries. The DLL returns the three
xyY records in tag order and preserves built-in floating-point versus reopened
s15Fixed16 precision. The facade performs one live-handle query and caches the
three small Arrays without profile parsing or color calculation.

`META-003EG` batches the remaining bounded profile-header identity fields.
LittleCMS returns creation time, flags, manufacturer/model signatures, and
profile ID in one native crossing. The facade preserves Pillow 11.3.0's direct
zero-based `tm_mon` exposure, maps the date to a six-field AHK object, and
caches signatures plus the 16-byte ID; it never parses the ICC header itself.

`META-003EH` batches three optional ICC signature tags: perceptual and
saturation rendering-intent gamut plus technology. The DLL preserves
independent tag presence and raw four-byte values. The facade performs one
coarse call, converts only present signatures, caches all three properties,
and does not invent defaults for the bounded sRGB profiles where all are absent.

`META-003EI` batches the two remaining bounded optional ICC text tags behind
one native UTF-8 query/copy ABI. LittleCMS owns `textDescriptionType`/`textType`
decoding for screening description and character target. The facade only
allocates returned buffers when a tag is present, converts UTF-8 once, and
caches both properties; it does not inspect serialized ICC bytes or invent
text for the bounded sRGB profiles where both tags are absent.

`META-003EJ` batches ICC measurement/viewing structs and viewing-condition text
behind one native call shape. LittleCMS owns tag parsing; the DLL returns coarse
presence, enum, numeric, and UTF-8 slots. The facade constructs Pillow-shaped
objects only for present tags, maps the bounded public labels, and caches all
three properties without reading ICC bytes in AHK.

`META-003EK` batches the remaining bounded header attribute mask and optional
colorimetric-intent image-state signature. LittleCMS reads both in native code;
the facade converts only a present four-byte signature and caches both values.
No serialized header or tag bytes cross into AHK for interpretation.

## Native Model

- Opaque image handles are allocated and released by `pillow_c.dll`.
- Pixels live in DLL-owned contiguous row-major byte buffers.
- Handles carry width, height, stride, mode, and channel metadata.
- Core operations work on whole handles or bulk buffers.
- Return values are stable status codes; AHK maps them to exceptions.

## AHK-Specific ABI Rules

- Prefer one coarse `DllCall` per operation.
- Expose native handles so chained operations can keep intermediates in DLL memory.
- Expose data pointers only with explicit lifetime rules.
- Provide output-handle `*_into` variants for allocation reuse.
- Avoid callback-heavy designs on hot paths.

## Wrapper Direction

The `pillow.ahk` facade layer should feel close to Python Pillow:

- `Pillow.Image.New(...)`
- `Pillow.Image.LinearGradient(...)`
- `Pillow.Image.RadialGradient(...)`
- `Pillow.Image.EffectMandelbrot(...)`
- `Pillow.Image.EffectNoise(...)`
- `Pillow.Image.FromBytes(...)`
- `Pillow.Image.Open(...)`
- `image.ToBytes(...)`
- `image.Save(...)`
- `image.Fill(...)`
- `image.Crop(...)`
- `image.Resize(..., box := ...)`
- `image.Thumbnail(...)`
- `image.Reduce(...)`
- `image.Filter(Pillow.ImageFilter.Kernel(...))`
- `image.Transform(...)`
- `image.TransformAffine(...)`
- `image.Rotate(...)`
- `image.Transpose(...)`
- `image.Convert(...)`, including RGB matrix conversion to `L` or `RGB`
- `image.Point(...)`, including single-band `1`/`L`/`P` target-mode point LUTs
- `Pillow.ImageOps.Invert(...)`
- `Pillow.ImageOps.Grayscale(...)`
- `Pillow.ImageOps.Mirror(...)`
- `Pillow.ImageOps.Flip(...)`
- `Pillow.ImageOps.Deform(...)`
- `Pillow.ImageOps.Posterize(...)`
- `Pillow.ImageOps.Solarize(...)`
- `Pillow.ImageOps.Colorize(...)`
- `Pillow.ImageOps.Equalize(...)`
- `Pillow.ImageOps.Autocontrast(...)`
- `Pillow.ImageOps.Crop(...)`
- `Pillow.ImageOps.Expand(...)`
- `Pillow.ImageOps.Scale(...)`
- `Pillow.ImageOps.Contain(...)`
- `Pillow.ImageOps.Cover(...)`
- `Pillow.ImageOps.Fit(...)`
- `Pillow.ImageOps.Pad(...)`
- `Pillow.ImageChops.Constant(...)`
- `Pillow.ImageChops.Duplicate(...)`
- `Pillow.ImageChops.Invert(...)`
- `Pillow.ImageChops.Difference(...)`
- `Pillow.ImageChops.Multiply(...)`
- `Pillow.ImageChops.Screen(...)`
- `Pillow.ImageChops.SoftLight(...)`
- `Pillow.ImageChops.HardLight(...)`
- `Pillow.ImageChops.Overlay(...)`
- `Pillow.ImageChops.Lighter(...)`
- `Pillow.ImageChops.Darker(...)`
- `Pillow.ImageChops.Add(...)`
- `Pillow.ImageChops.Subtract(...)`
- `Pillow.ImageChops.AddModulo(...)`
- `Pillow.ImageChops.SubtractModulo(...)`
- `Pillow.ImageChops.LogicalAnd(...)`
- `Pillow.ImageChops.LogicalOr(...)`
- `Pillow.ImageChops.LogicalXor(...)`
- `Pillow.ImageChops.Offset(...)`
- `Pillow.ImageFilter.Kernel(...)`
- `Pillow.ImageFilter.BLUR(...)`
- `Pillow.ImageFilter.CONTOUR(...)`
- `Pillow.ImageFilter.DETAIL(...)`
- `Pillow.ImageFilter.EDGE_ENHANCE(...)`
- `Pillow.ImageFilter.EDGE_ENHANCE_MORE(...)`
- `Pillow.ImageFilter.EMBOSS(...)`
- `Pillow.ImageFilter.FIND_EDGES(...)`
- `Pillow.ImageFilter.SHARPEN(...)`
- `Pillow.ImageFilter.SMOOTH(...)`
- `Pillow.ImageFilter.SMOOTH_MORE(...)`
- `Pillow.ImageFilter.RankFilter(...)`
- `Pillow.ImageFilter.MinFilter(...)`
- `Pillow.ImageFilter.MedianFilter(...)`
- `Pillow.ImageFilter.MaxFilter(...)`
- `Pillow.ImageFilter.ModeFilter(...)`
- `Pillow.ImageFilter.BoxBlur(...)`
- `Pillow.ImageFilter.GaussianBlur(...)`
- `Pillow.ImageFilter.UnsharpMask(...)`
- `Pillow.ImageEnhance.Color(...).Enhance(...)`
- `Pillow.ImageEnhance.Contrast(...).Enhance(...)`
- `Pillow.ImageEnhance.Brightness(...).Enhance(...)`
- `Pillow.ImageEnhance.Sharpness(...).Enhance(...)`
- `Pillow.ImageStat.Stat(...)`
- `Pillow.ImageColor.getrgb(...)`
- `Pillow.ImageColor.getcolor(...)`
- `Pillow.ImageSequence.Iterator(...)`
- `Pillow.ImageSequence.all_frames(...)`
- `image.GetBands(...)`
- `image.GetChannel(...)`
- `image.Split(...)`
- `image.PutAlpha(...)`
- `Pillow.Image.Composite(...)`
- `image.AlphaComposite(...)`
- `Pillow.Image.Eval(...)`
- `Pillow.ImageChops.Blend(...)`
- `Pillow.ImageChops.Composite(...)`
- `image.GetBbox(...)`
- `image.GetProjection(...)`
- `image.GetColors(...)`
- `image.GetData(...)`
- `image.PutData(...)`
- `image.RemapPalette(...)`
- `image.GetPixel(...)`
- `image.PutPixel(...)`
- `image.Entropy(...)`
- static helpers such as `Pillow.Image.Blend(...)` and `Pillow.Image.Merge(...)`

AHK owns ergonomics and lifetime. The DLL owns image bytes and transformations.

Explicit-palette numeric conversion also stays native: P/PA-to-I writes one
rounded fixed-point luma value per pixel into little-endian int32 storage, and
P/PA-to-F writes unrounded weighted luma into little-endian float32 storage.
Both routes ignore palette alpha and PA pixel A, reuse the existing allocating
and `_into` convert-mode ABI, and require no AHK pixel loop.

Explicit-palette P/PA-to-mode-1 conversion also remains a single native call.
It uses Pillow's direct weighted RGB threshold, ignores palette/pixel alpha,
and deliberately bypasses Floyd-Steinberg even when the caller requests or
defaults to that dither. Other supported source modes continue through the
DLL-owned error-diffusion kernel.

P/PA-to-RGBX expansion reuses the native four-byte RGBA palette loop. P takes
the fourth byte from RGBA palette alpha (or 255 for RGB/empty palettes), while
PA takes it from pixel A; RGB always comes from palette lookup. Empty palettes
therefore produce black without a facade-side default-palette expansion.

RGBX common-target conversion is likewise native: RGB drops X, RGBA replaces
X with 255, and L/LA run only RGB through the fixed-point luma kernel with
opaque LA alpha. A dedicated RGBX-to-RGBA branch keeps RGB PNG-transparency
metadata semantics isolated from the always-opaque RGBX contract.

RGBX color-space conversion also remains DLL-owned: CMYK subtracts the RGB
bytes and writes K=0, while YCbCr reuses the Pillow-compatible 6-bit lookup
kernel. Both paths stride four source bytes and ignore X.

RGBA-to-YCbCr shares that same native lookup traversal. It strides four source
bytes, passes only R/G/B into the exact RGB-to-YCbCr kernel, and ignores alpha,
matching composed `RGBA -> RGB -> YCbCr` without an intermediate image or an
AHK pixel loop; legal empty shapes retain their dimensions.

Mode-1/L/LA-to-YCbCr uses a distinct direct native traversal. L/LA reads L,
ignores LA alpha, and writes `[L,128,128]`; mode 1 first promotes each logical
sample to 0/255 and writes the same shape. No intermediate image or AHK pixel
loop is created, and legal empty shapes retain their dimensions.

Mode-1/L/LA-to-RGBX also stays in one DLL-owned traversal. All three sources
expand grayscale into the first three bytes; mode 1 promotes logical samples
to 0/255, mode 1 and L write X=`255`, and LA preserves its alpha byte as X.
This matches Pillow's direct conversion semantics, including packed mode-1
input and the deliberate difference between direct LA-to-RGBX and composition
through RGB, without an intermediate image or facade pixel loop.

RGB/RGBA-to-RGBX uses a separate native RGB-like traversal. It copies R/G/B
without color conversion and always writes X=`255`; RGBA alpha is deliberately
ignored, matching Pillow's direct and RGB-composed results. Legal empty shapes
return empty RGBX handles through the same allocating and `_into` ABI.

Numeric I/F-to-LA/RGB/RGBA/RGBX is also DLL-owned. One shared traversal applies
the established numeric-to-L truncation and clipping rules, including NaN and
infinity mappings. LA writes the clipped byte plus alpha=`255`; RGB targets
replicate it into three channels, and RGBA/RGBX append alpha/X=`255`. The route
supports allocating and `_into` calls without a temporary L/RGB image.

Numeric I/F-to-mode-1 is also fully DLL-owned. NONE clips each numeric sample
with the same I/F-to-L rules and applies the mode-1 threshold directly. Floyd
feeds that clipped byte into the shared native error-diffusion state machine;
the facade default selects the same Floyd route. Neither path allocates a
temporary L image, and public raw packing stays in the existing mode-1 codec.

Numeric I/F-to-CMYK uses Pillow's direct grayscale target semantics in one
DLL-owned traversal. Each numeric sample is clipped with the I/F-to-L rules,
then C/M/Y are written as zero and K as `255-L`. This is intentionally not the
RGB-composed CMYK representation, and requires no intermediate L or RGB image.

Numeric I/F-to-YCbCr uses Pillow's separate direct-grayscale target semantics.
One DLL-owned traversal applies the same I/F-to-L truncation and clipping,
then writes `[L,128,128]`. This matches composition through L and intentionally
differs from the lookup-sensitive RGB-composed route, without allocating an
intermediate L or RGB image.

Numeric I/F-to-HSV follows the same direct-grayscale architecture. The native
loop applies I/F-to-L truncation and clipping, then writes `[0,0,L]`. Pillow's
direct, L-composed, and RGB-composed routes agree exactly for this grayscale
result, so no intermediate image or RGB-to-HSV call is required.

Numeric I/F-to-LAB remains one DLL-owned traversal but uses an exact lookup
boundary. The loop clips each numeric sample to L, indexes the 256-byte
`PILLOW_L_TO_LAB_L` table, and writes internal `[L*,128,128]`; existing LAB raw
packing exposes neutral A/B as signed zero bytes. The lookup avoids per-pixel
floating color math and preserves Pillow's fixed rounding at all input bytes.

Mode-1/L/LA-to-LAB shares that same traversal and lookup boundary. Mode 1
promotes logical samples to 0/255, L indexes directly, and LA reads only its L
byte while ignoring alpha. No temporary L/RGB image or additional color math
is introduced, and signed LAB raw packing remains centralized at the raw ABI.

RGB/RGBA/RGBX-to-LAB uses a separate exact color-management path inside the
DLL. Process-lifetime LittleCMS 2.17 perceptual sRGB-to-Lab2 transforms are
created once with mutable pixel caching disabled. RGB rows use the three-byte
formatter; RGBA and RGBX rows use the four-byte formatter and ignore their
fourth channel. LittleCMS writes directly into target-owned internal LAB rows,
so allocating and `_into` conversion share the same hot loop without an
intermediate image or facade pixel traversal. Zero-pixel shapes bypass the
color engine while preserving target metadata and dimensions.

LAB-to-RGB/RGBA/RGBX uses the reverse process-lifetime LittleCMS 2.17
perceptual Lab2-to-sRGB pair with mutable pixel caching disabled. LAB raw decode
has already translated signed a/b bytes into the internal public channel
representation consumed by `TYPE_Lab_8`; the transform therefore handles
Pillow-compatible rounding and gamut clipping directly from source-owned rows.
RGB targets use `TYPE_RGB_8`. RGBA and RGBX share `TYPE_RGBA_8`, with the
target buffer prefilled to opaque `255` before LittleCMS overwrites RGB, so
both four-byte targets exactly match composition through RGB. Allocating and
`_into` conversions share this DLL-owned traversal, and zero-pixel shapes skip
the color engine.

Direct LAB-to-1/L/LA/P/PA/I/F/CMYK/YCbCr/HSV deliberately does not reuse that
valid RGB route. Pillow 11.3.0 only enters its ImageCms LAB branch for RGB,
RGBA, and RGBX targets; the ten covered non-RGB targets fall through
unsupported core conversion and raise `conversion from LAB to RGB not
supported`, including empty images. Native mode-pair validation for these
targets therefore runs before the generic empty-pixel success path, making
allocating and `_into` calls reject consistently. Mode-1 default/NONE/Floyd
calls enter the dither ABI, which validates the dither value and delegates LAB
sources to the same plain mode-pair gate before any pixel traversal. The
facade only normalizes the known public error text; it does not build an
intermediate RGB image or perform an AHK-side pixel traversal. Explicit caller
composition through LAB-to-RGB remains available as a distinct operation.

LAB-to-P option routing preserves Pillow's split between WEB conversion and
ADAPTIVE quantization. WEB/default, unknown palette values, dither, and colors
remain call-boundary inputs to the unsupported direct LAB conversion and do
not trigger an RGB intermediate. ADAPTIVE calls the existing native quantize
ABI. Its P target shape is checked first; zero-pixel sources then clear palette
metadata and succeed before colors/source-mode validation, while nonempty LAB
distinguishes invalid colors from wrong mode at the facade boundary. The
resulting empty P handle is DLL-allocated and wrapped with copied source info;
no AHK pixel traversal or synthetic success object is used. Public
LAB `Image.Quantize` with omitted method or any explicit Pillow method constant
now exposes that same native empty-image route directly: the facade performs
only bounded nonempty error normalization and derived-handle lifetime/info
wrapping, while the DLL still owns allocation, palette state, and zero-pixel
validation order. For LIBIMAGEQUANT this preserves Pillow's LAB mode/empty
precedence even when the optional algorithm dependency is unavailable; it does
not advertise native nonempty libimagequant support.

`MODE-COLOR-001BO` keeps integer kmeans as a facade validation/routing input
for this LAB boundary, not a hidden AHK algorithm. Negative values reject
before colors/mode/empty handling like Pillow; nonnegative values enter the
same native zero-pixel path or nonempty wrong-mode normalization, so DLL
ownership of allocation and palette state remains unchanged.

`MODE-COLOR-001BP` places supplied Image palettes ahead of that kmeans gate.
P palettes reach the shared source-mode rejection and non-P Image palettes
reach the shared palette-mode rejection for both nonempty and legal empty LAB
sources, without entering palette mapping or zero-pixel quantization. Calls
with an omitted palette retain the existing LAB native route, so this
precedence change adds no AHK pixel traversal or synthetic image and does not
alter DLL ownership.

`MODE-COLOR-001BQ` keeps explicit dither on that palette-omitted LAB route.
Pillow ignores dither when no reference palette is supplied, so NONE,
FLOYDSTEINBERG, and invalid integer/string values all preserve the same facade
validation and DLL-owned zero-pixel allocation. No native dither kernel runs,
and supplied-palette validation remains a separate earlier branch.

`MODE-COLOR-001BR` handles scalar noninteger kmeans before that route. Negative
Floats, nonnegative Floats, and Strings receive Pillow-compatible validation
errors in the facade before colors or empty-image handling; values are never
coerced or sent through the integer-only native quantize ABI. Integer kmeans
continues to use DLL-owned empty allocation, and supplied palettes retain their
earlier shared validation branch.

`MODE-NUM-001P` verifies DLL-owned one-band numeric histograms feed the facade
ImageStat object; production AHK derives scalar properties from 256 bins and
contains no per-pixel loop.

`MODE-NUM-001Q` keeps logical ImageChops work in the DLL and confirms its
mode-1 gate rejects numeric `I`/`F` handles before the logical pixel loops.
The facade routes all three methods through shared numeric-aware status
normalization; it adds no AHK pixel loop.

`MODE-NUM-001R` verifies the DLL-owned ImageChops invert loop complements
numeric `I`/`F` storage exactly. Byte-level complement composes to a 32-bit
sample bitwise complement, while the facade only wraps the derived handle and
adds no AHK pixel loop.

`MODE-NUM-001S` verifies the DLL-owned ImageChops offset loop indexes numeric
pixels with four-byte storage channels and copies one whole sample per
destination pixel. The facade only validates offsets and wraps the derived
handle; no AHK pixel loop is added.

`MODE-NUM-001T` verifies the DLL-owned ImageChops Constant route uses numeric
sources only for dimensions, allocates one-channel mode `L`, clips the fill,
and initializes the complete buffer. The facade adds no allocation or pixel
loop.

`MODE-NUM-001U` verifies the DLL-owned full-image copy route preserves all
four bytes of each numeric `I`/`F` sample in a distinct allocation. The facade
only routes `ImageChops.Duplicate` and wraps the derived handle; source
mutation and destruction do not affect the duplicate, and no AHK pixel loop
is added.

`MODE-NUM-001V` verifies the DLL-owned composite loop follows Pillow's
storage-byte semantics for numeric `I`/`F` images: a partial mode-L mask
blends each of the four stored bytes independently with exact Pillow rounding.
Both facade entry points route the same native handles and add no AHK pixel
loop.

`MODE-NUM-001W` verifies the DLL-owned masked-paste loop mutates owned numeric
`I`/`F` target storage in place, preserving its allocation while blending the
four stored bytes independently. Source and mask remain unchanged; the facade
only routes handles and adds no AHK pixel loop.

`MODE-NUM-001X` keeps scalar numeric Paste fills in the DLL while fixing the
facade boundary: mode `I` scalars are packed as signed int32 and mode `F`
scalars as float32 before `pillow_c_image_paste_color` fills the rectangle.
The facade performs only argument normalization and adds no AHK pixel loop.

`MODE-NUM-001Y` keeps one-element numeric color-sequence Paste fills in that
same DLL route. For mode `I` and `F`, the facade treats its established tuple
analogue `Array` like Pillow's one-element tuple, validates exactly one
component through `ColorBuffer`, and packs signed int32 or float32 before the
native rectangle fill. No AHK pixel loop is added.

`MODE-NUM-001Z` rejects multi-element numeric color Arrays at the public
`Image.Paste` boundary before entering the DLL, using Pillow's distinct mode
`I` and tuple-like mode `F` errors while preserving target allocation and
bytes. Valid four-byte colors still flow to the existing DLL-owned rectangle
fill, and no AHK pixel loop is added.

`MODE-NUM-001AA` applies that proven tuple validation only to public
`ImageDraw.Point`: invalid mode `I`/`F` multi-element Arrays are rejected before
the native call with Pillow's mode-specific errors, preserving allocation and
bytes. Valid point sets and packed colors still execute in one
`pillow_c_image_draw_points` call with no AHK pixel loop.

`MODE-NUM-001AB` applies the same bounded validation only to public
`ImageDraw.Line`, before either native straight-line or curve-joint dispatch.
Invalid numeric Arrays preserve allocation and bytes; valid packed colors and
complete rasterization remain in one DLL call with no AHK pixel loop.

`MODE-NUM-001AC` applies the bounded validation only to
`ImageDraw.Rectangle`'s fill branch while leaving outline handling unchanged.
Invalid numeric fill Arrays preserve allocation and bytes before the native
call; valid fill/outline rasterization remains DLL-owned with no AHK pixel loop.

`MODE-NUM-001AD` applies the matching validation to Rectangle's outline branch
with fill unset. Invalid numeric outline Arrays preserve allocation and bytes;
fill handling and valid native rectangle rasterization remain unchanged.

`MODE-NUM-001AE` applies the bounded validation only to
`ImageDraw.Ellipse`'s fill branch while leaving outline handling unchanged.
Invalid numeric fill Arrays preserve allocation and bytes before native
dispatch; valid fill/outline rasterization remains one DLL-owned operation
with no AHK pixel loop.

`MODE-NUM-001AF` applies the matching validation to Ellipse's outline branch
with fill unset. Invalid numeric outline Arrays preserve allocation and bytes;
fill handling and valid native ellipse rasterization remain unchanged.

`MODE-NUM-001AG` applies the proven tuple validation only to public
`ImageDraw.Arc` fill normalization. Invalid numeric fill Arrays preserve
allocation and bytes before native dispatch; valid packed colors and complete
arc rasterization remain one DLL-owned operation with no AHK pixel loop.

`MODE-NUM-001AH` applies the bounded validation only to
`ImageDraw.Chord`'s fill branch while leaving outline handling unchanged.
Invalid numeric fill Arrays preserve allocation and bytes before native
dispatch; valid chord rasterization remains DLL-owned with no AHK pixel loop.

`MODE-NUM-001AI` applies the matching validation to Chord's outline branch
with fill unset. Invalid numeric outline Arrays preserve allocation and bytes;
fill handling and valid native chord rasterization remain unchanged.

`MODE-NUM-001AJ` applies the bounded validation only to
`ImageDraw.Pieslice`'s fill branch while leaving outline handling unchanged.
Invalid numeric fill Arrays preserve allocation and bytes before native
dispatch; valid pieslice rasterization remains DLL-owned with no AHK pixel loop.

`MODE-NUM-001CG` pins border-unset above-neighbor-distance fill-all traversal
in numeric Floodfill. Threshold `8.0` remains below the I/F initial distances
but above the zero neighbor's scalar distance, so the DLL writes the seed,
admits that neighbor, and reaches the final sample through its native queue
without reallocation or an AHK pixel loop.

`MODE-NUM-001CF` pins border-unset below-neighbor-distance seed-only traversal
in numeric Floodfill. Per-mode thresholds `6.0` / `6.25` remain below the zero
neighbor's scalar background distance, so the DLL writes the seed, rejects
that neighbor, and leaves the disconnected final I/F sample unchanged without
reallocation or an AHK pixel loop.

`MODE-NUM-001CE` pins border-unset neighbor-distance equality traversal in
numeric Floodfill. Per-mode thresholds `7.0` / `7.25` are below the initial
value/background distances but equal the zero neighbor's scalar distance from
background, so the DLL writes the seed, admits that neighbor with `<=`, and
fills all three I/F samples without reallocating or using an AHK pixel loop.

`MODE-NUM-001CD` pins nonmatching packed scalar-border plus finite above-
initial-distance no-op precedence in numeric Floodfill. Per-mode thresholds
`17.0` / `9.75` reach the corrected DLL-owned scalar comparison and return
before seed mutation or supplied-border traversal; no further production or
ABI change was required after `MODE-NUM-001CC`.

`MODE-NUM-001CC` replaces byte-wise numeric Floodfill distance with a DLL-owned
mode-aware scalar 1-norm for `I` and `F`. The helper is shared by the initial
no-op comparison and border-unset neighbor admission; byte-storage modes retain
the existing channel-wise path. Exact equality now returns before mutation or
supplied-border traversal, with no facade pixel loop or ABI expansion.

`MODE-NUM-001CB` pins nonmatching packed scalar-border plus finite positive-
below-distance threshold fill-all traversal in numeric Floodfill. The facade
passes `1.0` unchanged, while DLL-owned comparison and supplied-border
traversal fill all I/F samples because distances `16` / `8.75` exceed the
threshold; no production or ABI change was required.

`MODE-NUM-001CA` pins nonmatching packed scalar-border plus zero-threshold
fill-all traversal in numeric Floodfill. The facade packs `300` / `2.5` into
mode-sized storage, while DLL-owned supplied-border traversal fills all I/F
samples after the initial nonzero distance exceeds `0.0`; no production or ABI
change was required.

`MODE-NUM-001BZ` pins nonmatching packed scalar-border plus finite negative-
threshold fill-all traversal in numeric Floodfill. The facade packs `300` /
`2.5` into mode-sized storage, while DLL-owned supplied-border traversal fills
all I/F samples after the initial ordered distance comparison is false; no
production or ABI change was required.

`MODE-NUM-001BY` pins nonmatching packed scalar-border plus positive-infinity
no-op precedence in numeric Floodfill. DLL-owned initial-distance comparison
returns before mutation or supplied-border traversal, while facade identity and
mode-sized packing remain intact; no production or ABI change was required.

`MODE-NUM-001BX` pins nonmatching packed scalar-border plus quiet-NaN fill-all
traversal in numeric Floodfill. The facade reuses mode-sized scalar packing,
while DLL-owned supplied-border traversal mutates all I/F samples after the
ordered NaN comparison is false; no production or ABI change was required.

`MODE-NUM-001BW` pins nonmatching packed scalar-border plus negative-infinity
fill-all traversal in numeric Floodfill. The facade packs `300` / `2.5` into
mode-sized storage, while DLL-owned supplied-border traversal mutates all I/F
samples in place; no production or ABI change was required.

`MODE-NUM-001BV` completes negative-infinity numeric Floodfill Array-border
shape coverage with empty Arrays. Empty/one/multi-element Arrays share the
facade-owned live sentinel and native non-null/zero-size ABI; all traversal and
writes remain DLL-owned with no production or ABI change.

`MODE-NUM-001BU` pins one-element Array-border plus negative-infinity fill-all
traversal in numeric Floodfill. Source Array length remains facade-only over
BT's supplied-incomparable-border sentinel; native traversal and writes remain
DLL-owned with no production or ABI change.

`MODE-NUM-001BT` pins multi-element Array-border plus negative-infinity
fill-all traversal in numeric Floodfill. The facade maps the Array to the
existing supplied-incomparable-border sentinel, while native seed mutation and
neighbor traversal remain DLL-owned with no production or ABI change.

`MODE-NUM-001BS` pins matching scalar-border plus negative-infinity seed-only
precedence in numeric Floodfill. Native initial comparison, seed write, and
matching-border stop remain DLL-owned; existing routes need no production or
ABI change.

`MODE-NUM-001BR` completes positive-infinity numeric Floodfill Array-border
shape coverage with empty Arrays. Empty/one/multi-element Arrays share the
facade-owned live sentinel and native non-null/zero-size ABI; the DLL-owned
initial-distance comparison returns before mutation or traversal. Existing
routes pass without production changes, AHK pixel loops, rebuilds, or new
exports.

`MODE-NUM-001BQ` extends positive-infinity numeric Floodfill coverage to one-
element Array borders. Source Array length remains facade-only over BP's shared
non-null/zero-size native sentinel; the DLL-owned initial-distance comparison
returns before any seed write or sentinel traversal. Existing routes pass
without production changes, AHK pixel loops, rebuilds, or new exports.

`MODE-NUM-001BP` composes positive infinity with numeric multi-element Array
borders in `ImageDraw.Floodfill`. The facade keeps the shared Array sentinel
alive while raw passes the same non-null/zero-size incomparable-border ABI. The
DLL-owned initial-distance comparison returns before any seed write or sentinel
traversal. Existing routes pass without production changes, AHK pixel loops,
rebuilds, or new exports.

`MODE-NUM-001BO` pins matching scalar-border plus positive-infinity precedence
in numeric `ImageDraw.Floodfill`. Packed scalar border bytes and the facade
scalar route reach the existing native call unchanged; the DLL-owned initial-
distance comparison returns before any seed write or supplied-border traversal.
Existing routes pass without production changes, AHK pixel loops, rebuilds, or
new exports.

`MODE-NUM-001BN` completes quiet-NaN numeric Floodfill Array-border shape
coverage with empty Arrays. Empty/one/multi-element Arrays share the same
facade-owned live sentinel and native non-null/zero-size ABI; DLL-owned
supplied-border traversal fills all I/F samples. Existing routes pass without
production changes, AHK pixel loops, rebuilds, or new exports.

`MODE-NUM-001BM` extends quiet-NaN numeric Floodfill coverage to one-element
Array borders. The source Array length remains facade-only over BL's shared
non-null/zero-size native sentinel; DLL-owned supplied-border traversal fills
all I/F samples. Existing routes pass without production changes, AHK pixel
loops, rebuilds, or new exports.

`MODE-NUM-001BL` composes quiet NaN with numeric multi-element Array borders
in `ImageDraw.Floodfill`. The facade keeps the Array sentinel alive while raw
passes the same non-null/zero-size incomparable-border ABI. Native supplied-
border traversal fills all I/F samples without consulting NaN after the
initial seed decision. Existing routes pass without production changes, AHK
pixel loops, rebuilds, or new exports.

`MODE-NUM-001BK` pins matching scalar-border plus quiet-NaN precedence in
numeric `ImageDraw.Floodfill`. Raw packed border bytes and facade scalar
values reach the existing supplied-border route unchanged; the initial NaN
comparison writes the seed and the matching zero border stops traversal.
Existing routes pass without production changes, AHK pixel loops, rebuilds,
or new exports.

`MODE-NUM-001BJ` pins quiet-NaN threshold semantics in numeric
`ImageDraw.Floodfill`. Raw and facade construct IEEE binary64 quiet NaN and
pass it unchanged; ordered DLL-owned initial and neighbor comparisons remain
false, so only the seed is written. Existing routes pass without production
changes, AHK pixel loops, rebuilds, or new exports.

`MODE-NUM-001BI` pins negative-infinity threshold semantics in numeric
`ImageDraw.Floodfill`. Raw and facade negate the already-verified IEEE positive
infinity and pass it unchanged; DLL-owned initial and neighbor comparisons
write only the seed. Existing routes pass without production changes, AHK
pixel loops, rebuilds, or new exports.

`MODE-NUM-001BH` pins positive-infinity threshold semantics in numeric
`ImageDraw.Floodfill`. Raw and facade construct IEEE binary64 infinity and pass
it unchanged; the DLL-owned initial distance comparison returns before seed
mutation. Existing routes pass without production changes, AHK pixel loops,
rebuilds, or new exports.

`MODE-NUM-001BG` completes the empty/one/multi-element numeric Array-border
matrix at threshold `-1.0`. Empty Arrays retain the same non-null/zero-size
sentinel through the facade DllCall, selecting DLL-owned supplied-border
traversal. The reused raw and extended facade routes pass without production
changes, AHK pixel loops, rebuilds, or new exports.

`MODE-NUM-001BF` extends numeric `ImageDraw.Floodfill` negative-threshold
coverage to one-element Array borders. Array length remains a facade-side
normalization detail: the retained non-null/zero-size sentinel selects the
same DLL-owned supplied-border traversal proven by BE. The reused raw and
extended facade routes pass without production changes, AHK pixel loops,
rebuilds, or new exports.

`MODE-NUM-001BE` composes the numeric `ImageDraw.Floodfill` incomparable-
Array-border route with threshold `-1.0`. The facade retains a non-null/zero-
size border sentinel for the DllCall; the native supplied-border branch fills
through scalar samples without applying the negative threshold. Existing raw
and facade routes pass without production changes, AHK pixel loops, rebuilds,
or new exports.

`MODE-NUM-001BD` composes the numeric `ImageDraw.Floodfill` scalar-border
branch with threshold `-1.0`. The native loop writes the I/F seed and then
uses the supplied border comparison for neighbors; the matching scalar zero
stops traversal independently of the negative threshold. Existing raw and
facade routes pass without production changes, AHK pixel loops, rebuilds, or
new exports.

`MODE-NUM-001BC` pins negative-threshold seed-only mutation in the native
numeric `ImageDraw.Floodfill` loop. With border unset and threshold `-1.0`, the
DLL writes the I/F seed value but admits no neighbor because color distances
are nonnegative. Existing raw and facade routes pass without production
changes, AHK pixel loops, rebuilds, or new exports.

`MODE-NUM-001BB` aligns invalid-threshold normalization with Pillow's empty-
value precedence. The facade detects an empty I/F value Array before threshold
validation and supplies native threshold `0.0` only for that DLL-owned no-op;
nonempty values retain the numeric-threshold requirement. No AHK pixel loop,
native rebuild, or new export is introduced.

`MODE-NUM-001BA` aligns invalid-border normalization with Pillow's empty-value
precedence. Once the facade recognizes an empty I/F value Array, it omits
border normalization and dispatches the existing native empty-value sentinel
with border unset; nonempty values retain all border validation. The DLL still
owns the no-op, with no AHK pixel loop, native rebuild, or new export.

`MODE-NUM-001AZ` completes empty numeric border Array composition with the
existing empty-value/incomparable-border states. The facade maps `[]` to the
same live non-null/zero-size border sentinel, while the reused raw ABI proof
confirms empty-value precedence. No production change, rebuild, AHK pixel loop,
or new export is introduced.

`MODE-NUM-001AY` locks one-element numeric border Arrays into AX's existing
empty-value/incomparable-border composition. The facade maps `[0]` / `[0.0]`
to the same live non-null/zero-size border sentinel, while the reused raw ABI
proof confirms empty-value precedence. No production change, rebuild, AHK
pixel loop, or new export is introduced.

`MODE-NUM-001AX` composes the existing empty-value and incomparable-border
states for numeric `ImageDraw.Floodfill`. The facade keeps live non-null/zero-
size buffers for both an empty I/F value Array and one nonempty multi-element
border Array; the DLL's empty-value precedence returns before inspecting the
border sentinel. Existing raw and facade routes pass without production
changes, AHK pixel loops, or new exports.

`MODE-NUM-001AW` removes the absent-border restriction from AU/AV's empty-
value sentinel. Pillow evaluates `_color_diff(value, background)` before its
border branch, so an empty numeric tuple raises the caught `IndexError` and
returns without inspecting even a valid scalar border. The native no-op now
precedes border validation and traversal, while the facade only forwards the
empty I/F Array sentinel with the caller's packed scalar border. No AHK pixel
loop or new export is introduced.

`MODE-NUM-001AV` removes the threshold-zero restriction from AU's absent-
border empty-value sentinel. Pillow raises and catches the empty tuple's
`IndexError` before evaluating threshold, so the native no-op state and facade
route now accept the already-validated numeric threshold without branching on
its value. No AHK pixel loop or new export is introduced.

`MODE-NUM-001AU` adds an explicit empty-value no-op state to the existing
numeric `ImageDraw.Floodfill` ABI. A live non-null value pointer with size zero,
border absent, and threshold zero returns success without touching native
storage; the facade routes only the matching empty I/F Array combination to
that sentinel. No AHK pixel loop is introduced, and all nonempty traversal
remains DLL-owned.

`MODE-NUM-001AT` locks one-element numeric `ImageDraw.Floodfill` value Arrays
to Pillow's tuple packing semantics. AO's length gate admits the single item,
the shared numeric `ColorBuffer` unwraps it as signed int32 or float32, and the
existing DLL Floodfill owns the complete no-border traversal while preserving
allocation and exact sample bytes. No AHK pixel loop, production change,
native rebuild, or new export is introduced.

`MODE-NUM-001AS` locks empty numeric `ImageDraw.Floodfill` border Arrays to
Pillow's incomparable-object behavior. The facade's existing all-Array route
passes a live non-null/zero-size sentinel to AP's native traversal, so empty
mode `I`/`F` Arrays fill through scalar zero while preserving allocation and
exact sample bytes. The raw sentinel proof is reused; no AHK pixel loop,
production change, native rebuild, or new export is introduced.

`MODE-NUM-001AR` locks numeric `ImageDraw.Floodfill` scalar-border composition
across both runtime surfaces. Facade `PasteColorBuffer` packs mode `I` as
signed int32 and mode `F` as float32; the existing ordinary native border
comparison stops at the matching four-byte sample while preserving allocation.
All traversal remains DLL-owned with no AHK pixel loop or production change.

`MODE-NUM-001AQ` extends AP's numeric `ImageDraw.Floodfill` facade routing to
one-element border Arrays. Pillow keeps those Arrays' tuple analogue as an
incomparable object, so AHK now passes the existing live non-null/zero-size
sentinel instead of packing the sole item as a scalar border. Allocation and
exact I/F sample bytes remain DLL-owned; no AHK pixel loop, native change, or
new export is introduced.

`MODE-NUM-001AP` keeps numeric `ImageDraw.Floodfill` traversal DLL-owned while
adding an explicit supplied-but-incomparable border state to the existing ABI.
AHK routes bounded multi-element mode `I`/`F` border Arrays as a live non-null
sentinel with size zero; C++ rejects pixels already equal to the value but
never compares samples with that sentinel, so the queue fills through scalar
zero samples without reallocating the image. No AHK pixel loop or new export
is introduced.

`MODE-NUM-001AO` applies bounded value validation to `ImageDraw.Floodfill`
with border unset and threshold zero. Invalid numeric value Arrays preserve
allocation and bytes before native dispatch; the flood-fill queue remains
DLL-owned with no AHK pixel loop.

`MODE-NUM-001AN` applies bounded fill validation to `ImageDraw.Bitmap` with a
valid mode `1` mask. Invalid numeric fill Arrays preserve target and mask
allocation/bytes before native dispatch; valid bitmap compositing remains
DLL-owned with no AHK pixel loop.

`MODE-NUM-001AM` applies the matching validation to
`ImageDraw.RoundedRectangle`'s outline branch with fill unset. Invalid numeric
outline Arrays preserve allocation and bytes; fill handling and valid native
rounded-rectangle rasterization remain unchanged.

`MODE-NUM-001AL` applies the bounded validation to
`ImageDraw.RoundedRectangle`'s fill branch while leaving outline handling
unchanged. Invalid numeric fill Arrays preserve allocation and bytes before
native dispatch; valid rounded-rectangle rasterization remains DLL-owned with
no AHK pixel loop.

`MODE-NUM-001AK` applies the matching validation to Pieslice's outline branch
with fill unset. Invalid numeric outline Arrays preserve allocation and bytes;
fill handling and valid native pieslice rasterization remain unchanged.

`FMT-TIFF-001AX` completes the strict full-binary+single-ASCII `270`/`315`
family for uncompressed single-frame `I;16B`; native owns all layout and the
existing facade route needs no change.

`FMT-TIFF-001AW` admits the strict single-frame uncompressed `I;16B`
DPI+ICC+XMP+ImageDescription composition. Native owns the sorted IFD and all
ASCII/RATIONAL/XMP/ICC/strip offsets; existing facade routing needs no change.

`FMT-TIFF-001AV` completes the validated XMP+ASCII family for the single-frame
uncompressed `I;16B` writer. The shared deep guard now accepts one tag
`270`/`315` or the exact pair `{270,315}`, while retaining compression, frame-
count, DPI, XMP-presence, and ICC-absence constraints. Native owns external/
inline ASCII, RATIONAL, XMP, and strip layout. Existing facade two-tag
`TiffInfo` routing already targets the entries ABI, so no wrapper production
change or AHK pixel path was needed.

`FMT-TIFF-001AU` completes the validated XMP+single-ASCII family for the
single-frame uncompressed `I;16B` writer. The shared deep guard accepts one
tag `270` or `315` while retaining frame-count, compression, DPI, XMP-
presence, and ICC-absence constraints. Native owns inline/out-of-line ASCII,
RATIONAL, XMP, and strip layout. Existing facade `TiffInfo` normalization
already targets the singular metadata-ASCII ABI, so no wrapper production
change or AHK pixel path was needed.

`FMT-TIFF-001AT` composes XMP with ImageDescription on the single-frame
uncompressed `I;16B` writer. The deep native guard admits only DPI, non-empty
XMP, one tag `270`, and no ICC; native owns the sorted big-endian IFD, external
ASCII, RATIONAL, XMP, and strip layout. Existing facade two-entry `TiffInfo`
normalization already targets the singular metadata-ASCII ABI, so no wrapper
production change or AHK pixel path was needed.

`FMT-TIFF-001AS` completes the validated ICC+ASCII family for the single-frame
uncompressed `I;16B` writer. The same deep native guard now accepts one tag
`270`/`315` or the exact pair `{270,315}`, while retaining compression, frame-
count, DPI, ICC-presence, and XMP-absence constraints. Native owns sorted IFD,
external/inline ASCII, RATIONAL, ICC, and strip layout. Existing facade ICC+
two-tag `TiffInfo` routing already targets the entries ABI; no wrapper
production change or AHK pixel path was needed.

`FMT-TIFF-001AR` completes the two validated single-ASCII branches for ICC on
the single-frame uncompressed `I;16B` writer. Native now uses one deep guard
for tag `270` or `315`, still constrained by compression, frame count, DPI,
ICC presence, and XMP absence. The Artist layout keeps tag `315` inline,
followed by native RATIONAL/ICC/strip payload ownership. Existing facade ICC+
`TiffInfo[315]` routing already targets the metadata-ASCII ABI; no wrapper
production change or AHK pixel path was needed.

`FMT-TIFF-001AQ` composes ICC with ImageDescription on the single-frame
uncompressed `I;16B` writer. Native emits a sorted 14-entry big-endian IFD,
external tag-270 ASCII, two RATIONAL payloads, UNDEFINED ICC, and the unchanged
raw strip while preserving `I;16B` storage. The admission guard includes
compression, frame-count, DPI, binary-presence, and exact tag checks, so the
partial metadata rule cannot spill into compressed routes. Existing facade
ICC+`TiffInfo[270]` routing already targets the metadata-ASCII ABI; no wrapper
production change or AHK pixel path was needed.

`FMT-TIFF-001AP` composes both binary metadata payloads on the single-frame
uncompressed `I;16B` writer. Native emits a sorted 14-entry big-endian IFD,
two RATIONAL payloads, BYTE XMP, UNDEFINED ICC, and the unchanged raw strip
while preserving `I;16B` storage. Existing facade ICC+`TiffInfo[700]` routing
already dispatches through the metadata-ex ABI, so no wrapper production
change or AHK pixel path was needed.

`FMT-TIFF-001AO` composes both supported ASCII tags on the single-frame
uncompressed `I;16B` writer. Native emits a sorted 14-entry big-endian IFD,
external tag-270 ASCII, inline tag-315 ASCII, two RATIONAL payloads, and the
unchanged raw strip while preserving `I;16B` storage. Existing facade routing
already dispatches this `TiffInfo` map through the metadata-ASCII entries ABI,
so no wrapper production change or AHK pixel path was needed.

`FMT-TIFF-001AN` adds DPI+Artist as an explicit configuration of the single-
frame uncompressed `I;16B` writer. Native emits a sorted 13-entry big-endian
IFD with inline tag-315 ASCII, two RATIONAL payloads, and the unchanged raw
strip while preserving `I;16B` storage. The facade recognizes single-frame
`TiffInfo[315]` plus DPI and routes it through the existing metadata-ASCII ABI.
Description/combined subsets and uncompressed multiframe remain separate; no
AHK pixel path was added.

`FMT-TIFF-001AM` adds DPI+ImageDescription as an explicit configuration of the
single-frame uncompressed `I;16B` writer. Native emits a sorted 13-entry big-
endian IFD, the external tag-270 ASCII payload, two RATIONAL payloads, and the
unchanged raw strip while preserving `I;16B` storage. The facade recognizes
single-frame `TiffInfo[270]` plus DPI and routes it through the existing
metadata-ASCII ABI. Artist/combined subsets and uncompressed multiframe remain
separate; no AHK pixel path was added.

`FMT-TIFF-001AL` adds DPI+XMP as another explicit configuration of the
single-frame uncompressed `I;16B` writer. Native emits a sorted 13-entry big-
endian IFD, two RATIONAL payloads, one BYTE XMP payload, and the unchanged raw
strip while preserving `I;16B` storage. The facade recognizes single-frame
`TiffInfo[700]` plus DPI and routes it through the existing frames metadata
ABI. ICC/ASCII combinations and uncompressed multiframe remain separate; no
AHK pixel path was added.

`FMT-TIFF-001AK` adds DPI+ICC as another explicit configuration of the
single-frame uncompressed `I;16B` writer. Native emits a sorted 13-entry big-
endian IFD, two RATIONAL payloads, one UNDEFINED ICC payload, and the unchanged
raw strip while preserving `I;16B` storage. The facade now routes ICC+DPI
through the existing frames metadata ABI instead of allowing the DPI-only
branch to omit ICC. XMP/ASCII subsets and uncompressed multiframe remain
separate; no AHK pixel path was added.

`FMT-TIFF-001AJ` admits DPI-only as a third explicit configuration of the AI
single-frame uncompressed `I;16B` writer. The same native big-endian layout
engine emits a sorted 12-entry IFD, two external RATIONAL values, and the
unchanged raw strip; file and reopened storage remain `I;16B`. Existing facade
DPI normalization already routes through the native options ABI, so no AHK
production or pixel-path change was needed. Partial ICC/XMP/ASCII subsets and
uncompressed multiframe remain separate boundaries.

`FMT-TIFF-001AI` extends the dedicated single-frame uncompressed `I;16B`
writer instead of normalizing through the little-endian shared writer. Native
now builds a sorted 16-entry big-endian IFD, preserves the raw big-endian
strip, and owns external Description, DPI rationals, XMP, and ICC layout. The
file and reopened image remain `I;16B`; source storage is unchanged. The
facade routes the exact single-frame `TiffInfo`+`IccProfile` combination
through the existing generalized frames ABI. Other uncompressed metadata
subsets and multiframe saves remain separate; no AHK pixel path was added.

`FMT-TIFF-001AH` composes AD's ICC/XMP payloads and AG's two ASCII entries in
one compressed I16-family normalization call. The DLL preserves all four
validated payload lifetimes across temporary little-endian copies and lets
the shared writer own IFD ordering, external data offsets, LZW strips, and DPI
for every frame. Source handles and reopened Info/GetExif values remain exact.
Partial metadata+ASCII and uncompressed metadata remain separate boundaries;
no facade production or AHK pixel path change was introduced.

`FMT-TIFF-001AG` composes AE and AF through the generalized ASCII-entry ABI.
The DLL accepts the exact validated tag set `{270,315}` on compressed I16-
family normalization, preserves both value arrays across temporary little-
endian copies, and lets the shared writer place out-of-line `Hello\0` and
inline `Ada\0` in each IFD beside exact LZW and DPI data. Source handles and
reopened GetExif values remain exact. ICC/XMP+ASCII and uncompressed metadata
remain explicit later boundaries; no facade production or AHK pixel path
change was introduced.

`FMT-TIFF-001AF` generalizes AE's single-ASCII normalized route from
ImageDescription-only to either ImageDescription or Artist. The DLL keeps the
same temporary little-endian copies and shared writer; Artist remains a
four-byte inline IFD value while source handles, LZW, DPI, and GetExif stay
exact. Multiple ASCII entries and metadata-plus-ASCII remain explicit later
boundaries, with no facade production or AHK pixel path change.

`FMT-TIFF-001AE` routes one ImageDescription ASCII entry through compressed
`I;16B` normalization. The DLL preserves the validated tag/value arrays across
temporary little-endian copies and lets the shared writer own NUL-inclusive
ASCII layout in both IFDs. The old uncompressed `I;16B` writer now explicitly
rejects any metadata instead of silently dropping it. Artist, multiple ASCII,
and metadata-plus-ASCII remain separate; no facade production or AHK pixel
path changed.

`FMT-TIFF-001AD` composes the AB/AC ICC and XMP routes inside the same
compressed `I;16B` normalization call. The DLL sends both validated payloads
through temporary little-endian copies and the shared IFD/DPI/LZW writer,
laying out tag `700` and tag `34675` independently in every frame. Source
handles and reopened metadata remain exact; facade production and AHK pixel
paths remain unchanged.

`FMT-TIFF-001AC` adds XMP-only composition to that same compressed `I;16B`
normalization architecture. The DLL carries tag-700 bytes and lifetime through
the temporary little-endian copies and shared IFD/DPI/LZW writer; AD later
composes simultaneous ICC+XMP. Both reopened
frames expose exact XMP Info/GetExif and samples; no facade production branch
or AHK pixel loop was introduced.

`FMT-TIFF-001AB` composes homogeneous two-frame `I;16B` LZW normalization
with DPI and ICC in the existing native metadata ABI. The DLL keeps its
temporary little-endian copies, passes the validated ICC pointer and size into
the shared IFD writer, and writes UNDEFINED tag `34675` in both frames before
reopening exact `I;16` samples and metadata. Caller handles remain unchanged;
the existing facade route adds no production branch or AHK pixel loop.

`FMT-TIFF-001AA` generalizes Z's compressed normalization from homogeneous
`I;16B` lists to mixed `I;16`/`I;16B` lists. Native counts the 16-bit family,
copies every frame into temporary DLL-owned storage, and swaps bytes only for
the big-endian members before using the shared compression/IFD/DPI writer.
Both base/append orderings preserve caller modes and bytes while reopening as
exact little-endian `I;16` frames. Bounded PackBits/LZW strips match Pillow;
native Deflate keeps valid 19-byte stored blocks. No facade production change
or AHK pixel loop was introduced.

`FMT-TIFF-001Z` extends compressed multiframe+DPI TIFF saves to homogeneous
public `I;16B` frame lists. The DLL refreshes and validates every source,
creates temporary native-owned copies, byte-swaps each frame to little-endian
`I;16`, and delegates compression, IFD chaining, and DPI layout to the shared
multiframe writer. Caller handles remain `I;16B` with unchanged bytes; reopened
initial and selected frames are `I;16` with exact samples and DPI. PackBits and
LZW match Pillow's bounded strips exactly; native Deflate retains its valid
19-byte stored blocks versus Pillow's 16-byte streams. No AHK pixel loop or
facade production branch was added.

`FMT-TIFF-001Y` composes two-frame mode `I` and mode `F` TIFF saves with DPI
and PackBits, LZW, or Adobe Deflate entirely through the native multiframe
writer. The bounded fixtures keep one strip per frame, no Predictor, exact
signed-int32/float32 bytes, and DPI in initial and selected handles. Y exposed
that the native PackBits encoder treated every two-byte run as a literal;
`append_packbits_encoded_row` now uses libtiff's Base/Literal/Run/LiteralRun
state transitions, including conditional literal-run-literal coalescing, so
the numeric PackBits strips are byte-identical to Pillow without moving any
pixel loop into AHK. LZW is also byte-identical for the bounded strips;
Deflate keeps valid DLL-owned stored blocks.

`FMT-TIFF-001X` extends the same `I;16` multiframe+DPI composition to LZW and
Adobe Deflate. Native LZW matches Pillow's 12-byte strips exactly; native
Deflate keeps the documented valid 19-byte stored-block representation versus
Pillow's 16-byte compressed streams. Both decode to exact samples with DPI in
initial and selected states. This was coverage-only, with no production or
ABI change.

`FMT-TIFF-001W` proves the generalized native multiframe options writer
already composes little-endian `I;16`, PackBits, and DPI in one call. Both
IFDs carry exact Pillow-compatible ten-byte row strips and resolution tags;
the native early parsers preserve bytes and DPI at initial and selected frame
states. The facade only routes compression/DPI and handle lifetime. This was
coverage-only, with no production or ABI change.

`FMT-TIFF-001V` closes the corresponding frame-0 path for the native early
numeric parser shared by modes `I` and `F`. The initial handle now parses
IFD0 resolution before return, so facade `Image.Open` receives DPI without a
seek while exact signed-int32/float32 bytes remain DLL-owned. No facade
production route, AHK pixel loop, or ABI symbol was added.

`FMT-TIFF-001U` closes the corresponding frame-0 path for the native early
`I;16` parser. It now parses IFD0 resolution before returning the initial
handle, so facade `Image.Open` receives DPI immediately without requiring a
seek. Exact two-byte samples and EXIF tags stay DLL-owned; no facade
production route, AHK pixel loop, or ABI symbol was added.

`FMT-TIFF-001T` extends the same selected-IFD resolution ownership to the
native early numeric frame parser shared by modes `I` and `F`. Native TIFF
save/open keeps signed-int32 and float32 bytes exact; the selected handle now
receives its own DPI before return, and facade `Seek(1)` only refreshes that
DLL-owned metadata. No facade production route, AHK pixel loop, or ABI symbol
was added.

`FMT-TIFF-001S` extends selected-IFD resolution ownership to the native early
`I;16` frame parser. The multiframe writer already emits DPI tags in every
IFD; `open_tiff_frame_image(path, 1, ...)` now parses those tags before its
numeric-frame early return and populates the selected handle's DPI fields.
Facade `Seek(1)` therefore refreshes `Info["dpi"]` from the DLL-owned handle,
while exact two-byte samples and TIFF EXIF tags remain native-owned. No facade
production route, AHK pixel loop, or ABI symbol was added.

`FMT-TIFF-001R` proves the generalized multiframe route composes LZW, DPI,
ICC, XMP, and two ASCII entries in one native write. Its raw tracer also
exposed that WIC-backed `open_tiff_frame_image` parsed resolution only for
IFD0; selected RGB frames now parse their own IFD resolution and populate the
native handle's DPI fields. Facade Info therefore reflects DLL-owned selected
frame metadata rather than relying on retained frame-0 state.

`FMT-TIFF-001Q` replaces the internal singular ASCII layout with a bounded
entry-array writer. Native validates and de-duplicates tags, emits IFD entries
in tag order, computes each inline or out-of-line value independently, and
writes all entries for every frame. The old singular export adapts one entry
to the shared core; the facade packs only tags, pointers, and sizes and keeps
the UTF-8/NUL Buffers alive for one DLL call.

`FMT-TIFF-001P` extends the singular ASCII save_all route to TIFF Artist tag
`315` and proves its NUL-inclusive four-byte payload is stored directly in
each IFD value field. Native validates the tag and owns inline packing; the
facade passes the selected tag plus one UTF-8/NUL Buffer through the existing
coarse call.

`FMT-TIFF-001O` extends the coarse multiframe metadata writer with bounded
TIFF ASCII tag `270` (`ImageDescription`). Native owns TIFF type `2`, the
NUL-inclusive count, per-frame payload offsets, validation, and writes; the
facade only accepts the proven `TiffInfo` Map shape, creates one NUL-terminated
UTF-8 Buffer, keeps it alive for the call, and rejects unknown tags. Both
frames are written in one DLL call with no AHK pixel loop.

`FMT-TIFF-001N` proves the same coarse metadata-ex call lays out ICC and XMP
together for every frame. Native owns both payload offsets and types while
facade composes the existing `IccProfile` and bounded `TiffInfo` arguments.

`FMT-TIFF-001M` extends the coarse multiframe metadata writer with XMP tag
`700`. Native lays out BYTE payloads in every IFD and owns bounds/lifetime;
facade accepts only the bounded Pillow-compatible `TiffInfo` Map tag `700`
shape and routes one DLL call. Direct TIFF `xmp=` remains ignored like Pillow.

`FMT-TIFF-001L` extends the native multiframe writer with explicit ICC payload
ownership. Each IFD receives an UNDEFINED `34675` entry and a DLL-laid-out
payload, including embedded NUL bytes. The facade converts byte-like input to
one Buffer and passes its pointer/length through one coarse metadata call;
native reopen already supplies per-frame Info/GetExif state.

`FMT-TIFF-001K` extends the same coarse route across LZW and Adobe Deflate
with DPI. Native owns both compression strategies and all resolution-tag IFD
layout; facade option strings and the DPI pair remain call-boundary inputs.

`FMT-TIFF-001J` proves the generalized multiframe options route composes
PackBits and DPI in one native operation. The same DLL-owned frame loop writes
compressed strips and per-frame resolution tags; the facade does not inspect
or rewrite TIFF bytes.

`FMT-TIFF-001I` exposes the existing native multiframe DPI-capable writer
through one coarse options export. The DLL writes XResolution, YResolution,
and ResolutionUnit in every frame IFD while retaining independent frame bytes
and links. The facade only validates the DPI pair, normalizes an optional
compression value, packs handles, and performs one DLL call.

`FMT-TIFF-001H` batches LZW and Adobe Deflate through the same multiframe
compression ABI. Native owns both strip encoders and all frame iteration;
facade compression strings map to integer strategy values. Native Deflate
remains a valid zlib stored-block strategy, so decoded semantics and prefix
match while compressed strip size differs from Pillow's compressor.

`FMT-TIFF-001G` composes multipage TIFF with PackBits through one new coarse
native ABI. The export normalizes the compression enum and delegates to the
existing frame-vector writer, so row encoding, strip sizing, IFD links, and
decoded bytes remain DLL-owned. The facade only maps the public compression
option and passes image handles.

`FMT-TIFF-001F` verifies the generalized frame-vector route beyond two frames.
Native layout precomputes three IFD/pixel regions, links IFD0 to IFD1 and IFD1
to IFD2, terminates IFD2 with zero, and preserves each exact RGB strip. The
facade passes a two-element append handle array and seeks all native frames.

`FMT-TIFF-001E` verifies per-frame geometry independence for one RGB `2x1`
base and RGB `1x2` append frame. Native layout generation derives width,
height, RowsPerStrip, strip offsets, and next-IFD placement from each handle;
facade seek refreshes the public `Size` from each opened native frame.

`FMT-TIFF-001D` verifies per-frame mode independence for one same-size L/RGB
multipage TIFF and corrects the shared native L IFD shape. Layout counting and
entry emission now both omit SamplesPerPixel for single-channel L while
writing PlanarConfiguration `1`; the following RGB frame keeps its own
three-sample layout and exact bytes. Facade routing remains handle-only.

`FMT-TIFF-001C` extends the same generalized multipage TIFF proof to RGBA.
Native IFD generation owns four 8-bit samples, unassociated-alpha
`ExtraSamples=2`, contiguous interleaved strips, and frame links; the facade
continues to pass handles and expose native seek without alpha loops.

`FMT-TIFF-001B` verifies that the generalized DLL-owned multipage TIFF writer
already carries same-size RGB frames without a mode-specific facade branch.
Each frame receives an RGB IFD with three 8-bit samples, contiguous planar
configuration, one raw interleaved strip, and a linked next-IFD offset; the
facade only normalizes `save_all` / `append_images`, passes handle lifetimes,
and reuses native frame-count/open/seek paths.

`FMT-JPEG-003AZ` moves duplicate ordinary Photoshop resource precedence into
the DLL. Native parser state keeps the first key position but replaces its
byte value on later occurrences of the same resource code, matching Python
dictionary last-value semantics before the facade enumerates metadata.

`FMT-JPEG-003AY` verifies that separately recognized Photoshop APP13 markers
merge into one DLL-owned metadata view in marker order. An ordinary resource
from the first marker and structured ResolutionInfo from the second remain
simultaneously visible through their existing ABIs; keep-normalized saves
omit both markers. No production or ABI change was required.

`FMT-JPEG-003AX` verifies that the ordinary byte-resource enumeration and the
structured ResolutionInfo scalar ABI compose into one facade Photoshop Map
when both records share one APP13 marker. The DLL remains the authority for
both values and keep-normalized marker omission; no production or ABI change
was required.

`FMT-JPEG-003AW` extends DLL-owned Photoshop APP13 parsing with structured
ResolutionInfo resource `0x03ED`. Native code decodes the two unsigned 16.16
fixed-point resolutions and the displayed-unit fields into typed image-handle
state, preserves that state through metadata copies, and exposes it through
one scalar ABI call. The facade only composes the returned values into
`Info["photoshop"][0x03ED]`; it does not decode resource bytes or fixed-point
numbers. Keep-normalized JPEG saves continue to omit APP13 like Pillow 11.3.0.

`FMT-JPEG-003AV` adds DLL-owned Photoshop APP13 resource parsing. Native code
recognizes `Photoshop 3.0\0`, walks aligned `8BIM` records, stores integer
resource IDs and byte values on image handles, and propagates them through
metadata copies. The facade only enumerates the count/indexed ABI and builds
`Info["photoshop"]`; it does not parse JPEG bytes.

`FMT-JPEG-003AU` verifies unknown APP13 markers remain outside DLL-owned JPEG
metadata state and are not copied by keep-normalized native saves, matching
Pillow's `quality="keep"` / `qtables="keep"` behavior. Source DQT ownership,
RGB decode, and output dimensions remain native; AHK only routes the two keep
spellings and lifetimes.

`META-002AE` verifies DLL-owned JPEG ICC count validation also maps homogeneous
two-marker declared counts `0` and `255` to state `2` because neither equals
the two collected markers. AHK only maps that native state to its `""` None
analogue; marker parsing, sorting, count comparison, and state ownership remain
native.

`META-002AD` verifies DLL-owned JPEG ICC count validation maps singleton
declared counts `0` and `255` to state `2` because neither equals the one
collected marker. AHK only maps that native state to its `""` None analogue.

`META-002AC` verifies DLL-owned JPEG ICC finalization ignores the nominal
singleton sequence range at both representative high values `2` and `255`
when declared count matches one collected marker. AHK only materializes the
state-1 metadata Buffers `A` / `B`.

`META-002AB` verifies the DLL-owned JPEG ICC finalizer deliberately uses the
sorted marker count rather than validating the nominal sequence range: a
singleton `0/1:A` remains state `1` with byte `A`. AHK only materializes the
native metadata Buffer.

`META-002AA` extends DLL-owned JPEG metadata from binary profile presence to a
tri-state: no public key, byte profile (including legal empty bytes), or public
`None` for a recognized incomplete marker set. The image handle preserves that
state across native copies; AHK routes state `2` to its established `""` None
analogue without parsing markers or reconstructing bytes.

`META-002Z` verifies DLL-owned pending ICC markers preserve a zero-length
middle fragment in a declared three-fragment set and collate the nonempty
prefix/suffix as exact public bytes `AB`; AHK only materializes metadata.

`META-002Y` verifies DLL-owned pending ICC markers preserve a zero-length final
fragment while retaining the nonempty prefix; AHK only materializes metadata.

`META-002X` verifies DLL-owned pending ICC markers preserve a zero-length first
fragment and collate the nonempty tail; AHK only materializes the returned
metadata Buffer.

`META-002W` verifies zero-length explicit JPEG ICC input is omitted by the DLL
save route and by public facade normalization; production AHK writes no marker.

`META-002V` verifies DLL-owned deferred JPEG ICC marker sorting permissively
collates duplicate `1/2` fragments like Pillow; AHK only materializes the
returned metadata Buffer.

`FMT-JPEG-002B2CS` verifies CR's complete 1505-byte file is emitted by one DLL
call; production AHK has no marker or pixel loop.

`FMT-JPEG-002B2CR` verifies default-4:2:0 progressive rows-6 over-scan DRI
state without RST; AHK only routes options and hashes test output.

`FMT-JPEG-002B2CQ` verifies CP's complete 1757-byte file is emitted by one DLL
call; production AHK has no marker or pixel loop.

`FMT-JPEG-002B2CP` verifies source-4:2:2 progressive rows-6 over-scan DRI
state without RST; AHK only routes options and hashes test output.

`FMT-JPEG-002B2CO` verifies CN's complete file is emitted by one DLL call;
production AHK has no marker or pixel loop.

`FMT-JPEG-002B2CN` verifies native default-4:2:0 progressive over-scan DRI
state without RST; AHK only routes options and hashes test output.

`FMT-JPEG-002B2CM` verifies CL's complete marker stream is emitted by one DLL
call; production AHK performs no marker or pixel loop.

`FMT-JPEG-002B2CL` verifies that native progressive DRI state accepts an
interval larger than the complete source-4:2:2 scan without emitting RST;
AHK only routes options and performs test-only hashing.

`FMT-JPEG-002B2CK` verifies CJ's complete default-4:2:0 rows-4 serialization.
One DLL call emits the exact metadata, DQT/SOF2, ten scans, DRI, entropy, empty
restart state, and EOI; production AHK has no marker or pixel loop.

`FMT-JPEG-002B2CJ` verifies that default-4:2:0 h2v2 coefficients and component-
local Huffman ownership compose with rows-4 DRI intervals. One existing DLL
save call produces all ten Pillow-exact DHT/SOS and entropy streams with DRI
`[12,24,12,24,12,24]` and ten empty restart arrays; the facade only normalizes
options and performs no marker or pixel loop.

`FMT-JPEG-002B2CI` verifies CH's complete source-4:2:2 optimized-progressive
rows-4 JPEG serialization. One existing DLL save call emits exact APP0/EXIF/
ICC/COM, source DQT, SOF2, ten DHT/SOS scans, six DRI changes, empty restart
state, entropy, and EOI bytes; raw and facade whole-file hashes match Pillow.
Production AHK still performs only option/metadata normalization and no marker
or pixel loop.

`FMT-JPEG-002B2CH` verifies the whole-scan restart boundary on source-4:2:2
optimized-progressive output. With `restart_marker_rows=4`, one existing DLL
save call emits six scan-local DRI changes, Pillow-exact component-local DHT/
SOS and entropy bytes, and no terminal RST in any of the ten scans. Production
AHK still performs only option/metadata normalization and no marker or pixel
loop.

`FMT-JPEG-002B2CG` verifies CF's complete default-4:2:0 optimized-progressive
rows-3 JPEG serialization. One existing DLL save call emits exact APP0/EXIF/
ICC/COM, source DQT, SOF2, ten DHT/SOS scans, DRI/RST transitions, entropy,
and EOI bytes; raw and facade whole-file hashes match Pillow. Production AHK
still performs only argument/metadata normalization and no marker/pixel loop.

`FMT-JPEG-002B2CF` verifies that default-4:2:0 h2v2 coefficients and component-
local Huffman ownership compose with rows-3 restart intervals. One existing
DLL save call produces all ten Pillow-exact DHT/SOS and entropy streams with
DRI `[9,18,9,18,9,18]` and sampling-local empty/RST0 state; the facade only
normalizes options and performs no marker/pixel loop.

`FMT-JPEG-002B2CE` verifies CD's complete source-4:2:2 optimized-progressive
rows-3 JPEG serialization. One existing DLL save call emits exact APP0/EXIF/
ICC/COM, source DQT, SOF2, ten DHT/SOS scans, DRI/RST transitions, entropy,
and EOI bytes; raw and facade whole-file hashes match Pillow. Production AHK
still performs only argument/metadata normalization and no marker/pixel loop.

`FMT-JPEG-002B2CD` verifies that source-4:2:2 component-local Huffman ownership
composes with a three-row restart interval and short tail. One existing DLL
save call produces all ten Pillow-exact DHT/SOS and entropy streams with DRI
`[9,18,9,18,9,18]` and RST0-only scans; the facade only normalizes options and
performs no marker/pixel loop.

`FMT-JPEG-002B2CC` verifies CB's complete default-4:2:0 optimized-progressive
rows-2 JPEG serialization. One existing DLL save call emits exact APP0/EXIF/
ICC/COM, source DQT, SOF2, ten DHT/SOS scans, DRI/RST transitions, entropy,
and EOI bytes; raw and facade whole-file hashes match Pillow. Production AHK
still performs only argument/metadata normalization and no marker/pixel loop.

`FMT-JPEG-002B2CB` verifies that h2v2 sampled coefficients and component-local
Huffman ownership compose with default-4:2:0 rows-2 restart intervals. One
existing DLL save call produces all ten Pillow-exact DHT/SOS and entropy
streams with DRI `[6,12,6,12,6,12]` and sampling-local empty/RST0 state; the
facade only normalizes options and performs no marker/pixel loop.

`FMT-JPEG-002B2CA` verifies BZ's complete source-4:2:2 optimized-progressive
rows-2 JPEG serialization. One existing DLL save call emits exact APP0/EXIF/
ICC/COM, source DQT, SOF2, ten DHT/SOS scans, DRI/RST transitions, entropy,
and EOI bytes; raw and facade whole-file hashes match Pillow. Production AHK
still performs only argument/metadata normalization and no marker/pixel loop.

`FMT-JPEG-002B2BZ` verifies that the shared sampled RGB progressive encoder's
component-local Huffman ownership also composes with source-4:2:2 rows-2
restart intervals. One existing DLL save call produces all ten Pillow-exact
DHT/SOS and entropy streams with DRI `[6,12,6,12,6,12]` and RST0-only scans;
the facade only normalizes options and performs no marker/pixel loop.

`FMT-JPEG-002B2BY` verifies BX's complete default-4:2:0 optimized-progressive
rows-1 JPEG serialization. One existing DLL save call emits exact APP0/EXIF/
ICC/COM, source DQT, SOF2, ten DHT/SOS scans, DRI/RST transitions, entropy,
and EOI bytes; raw and facade whole-file hashes match Pillow. Production AHK
still performs only argument/metadata normalization and no marker/pixel loop.

`FMT-JPEG-002B2BX` verifies that the shared sampled RGB progressive encoder's
component-local Huffman ownership also composes with default-4:2:0 h2v2
coefficients. One existing DLL save call produces all ten Pillow-exact DHT/SOS
and entropy streams with scan-local DRI/RST state; the facade only normalizes
the omitted subsampling route and performs no marker/pixel loop.

`FMT-JPEG-002B2BW` verifies BV's complete source-4:2:2 optimized-progressive
rows-1 JPEG serialization. One existing DLL save call emits the exact APP0/
EXIF/ICC/COM, source DQT, SOF2, ten DHT/SOS scans, DRI/RST transitions,
entropy, and EOI bytes; raw and facade whole-file hashes match Pillow.
Production AHK still performs only argument/metadata normalization and no
marker or pixel loop.

`FMT-JPEG-002B2BV` makes sampled RGB progressive optimized-Huffman ownership
component-local. The DLL gathers separate Cr/Cb AC-first and final-refine
frequencies, builds each table independently, and redefines AC table id 1
before the matching component scan. The existing ten-scan schedule and native
entropy/restart state remain intact while all ten DHT/SOS/entropy streams now
match Pillow. The facade still performs one save call and no marker/pixel loop.

`FMT-JPEG-002B2BU` verifies BR's complete source-4:2:2 optimized rows-1 JPEG
serialization. One existing DLL save call emits the exact APP0/EXIF/ICC/COM,
source DQT, SOF0, optimized DHT, DRI/SOS, entropy/RST0..RST2, and EOI bytes;
raw and facade whole-file hashes match Pillow. Production AHK still performs
only argument/metadata normalization and no marker or pixel loop.

`FMT-JPEG-002B2BT` verifies BS's complete default-4:2:0 optimized rows-1 JPEG
serialization. One existing DLL save call emits the exact APP0/EXIF/ICC/COM,
source DQT, SOF0, optimized DHT, DRI/SOS, entropy/RST0, and EOI bytes; raw and
facade whole-file hashes match Pillow. Production AHK still performs only
argument/metadata normalization and no marker or pixel loop.

`FMT-JPEG-002B2BS` corrects RGB h2v2 sampled-block preparation inside the
native JPEG encoder. For default 4:2:0 output, Cb/Cr 2x2 averaging now
alternates libjpeg-turbo's `1,2` division bias by chroma column instead of
using a constant `2`; the resulting coefficients feed the existing DLL-owned
optimized-Huffman frequency pass and entropy writer. BR's h2v1 cadence remains
separate and unchanged. The facade still performs only option/metadata
normalization and one save call.

`FMT-JPEG-002B2BR` corrects RGB h2v1 sampled-block preparation inside the
native JPEG encoder. For source-preserving 4:2:2 output, Cb/Cr horizontal
averaging now alternates libjpeg-turbo's `0,1` division bias by chroma column;
the resulting coefficients feed the existing DLL-owned optimized-Huffman
frequency pass and entropy writer. The facade still performs only option/
metadata normalization and one save call. Default RGB h2v2 sampling remains a
separate codec-strategy child rather than sharing an unproven rounding rule.

`FMT-JPEG-002B2BQ` verifies that native JFIF insertion composes with BP's
exact quality-keep progressive rows-2 XMP/core-metadata serialization in one
DLL call. The DLL writes unit-1 JFIF density 300x150 before APP14, then retains
every BP metadata, codec, restart, entropy, and EOI byte. Production AHK only
normalizes/routes; hashing remains test-only.

`FMT-JPEG-002B2BP` verifies that native EXIF/XMP/ICC/COM insertion composes
with BO's exact quality-keep progressive rows-2 serialization in one DLL call.
The DLL writes APP14, EXIF APP1, XMP APP1, ICC APP2, and COM before DQT, then
retains every BO codec and entropy byte. Production AHK only normalizes/routes;
hashing remains test-only.

`FMT-JPEG-002B2BO` verifies that native EXIF/ICC/COM insertion composes with
BN's exact quality-keep progressive rows-2 serialization in one DLL call. The
DLL writes APP14, EXIF APP1, ICC APP2, and COM before DQT, then retains every
BN codec and entropy byte. Production AHK only normalizes/routes; hashing
remains test-only.

`FMT-JPEG-002B2BN` verifies AZ's native quality-keep progressive rows-2 JPEG as
one complete byte-identical DLL-owned serialization. The existing call writes
APP14, source DQT, SOF2, all DHT/DRI/SOS markers, 18 entropy streams, 108
restart markers, and EOI. Production AHK only normalizes/routes; hashing
remains test-only.

`FMT-JPEG-002B2BM` verifies that native JFIF insertion composes with BL's exact
progressive rows-2 XMP/core-metadata serialization in one DLL call. The DLL
writes unit-1 JFIF density 300x150 before APP14, then retains all BL metadata,
codec, and entropy bytes. Production AHK only normalizes/routes; hashing
remains test-only.

`FMT-JPEG-002B2BL` verifies that native EXIF/XMP/ICC/COM insertion composes
with BK's exact progressive rows-2 serialization in one DLL call. The DLL
writes APP14, EXIF APP1, XMP APP1, ICC APP2, and COM before DQT, then retains
all BK codec and entropy bytes. Production AHK only normalizes/routes; hashing
remains test-only.

`FMT-JPEG-002B2BK` verifies that native EXIF/ICC/COM insertion composes with
BJ's exact progressive rows-2 serialization in one DLL call. The DLL writes
APP14, EXIF APP1, ICC APP2, and COM before DQT, then retains all BJ codec and
entropy bytes. Production AHK only normalizes/routes; hashing remains test-only.

`FMT-JPEG-002B2BJ` verifies AY's native web-low progressive rows-2 JPEG as one
complete byte-identical DLL-owned serialization. The existing call writes
APP14, split DQT, SOF2, all DHT/DRI/SOS markers, 18 entropy streams, 66
restart markers, and EOI without an AHK marker loop. Production AHK only
normalizes arguments and routes the call; whole-file hashing remains test-only.

`FMT-JPEG-002B2BI` verifies that the native JFIF/DPI insertion layer composes
with BH's exact web-low optimized rows-2 XMP/core-metadata serialization in
one DLL call. The DLL writes unit-1 JFIF density 300x150 before APP14, EXIF
APP1, XMP APP1, ICC APP2, and COM, then retains BH's exact codec stream.
Production AHK only normalizes arguments and routes the call; whole-file
hashing remains test-only.

`FMT-JPEG-002B2BH` verifies that the native XMP insertion layer composes with
BG's exact web-low optimized rows-2 core-metadata serialization in one DLL
call. The DLL writes APP14, EXIF APP1, XMP APP1, ICC APP2, and COM before DQT,
then retains BG's exact DQT/SOF0/DHT/DRI/SOS/restart/EOI stream. Production
AHK only normalizes arguments and routes the call; whole-file hashing remains
test-only.

`FMT-JPEG-002B2BG` verifies that the native EXIF/ICC/COM insertion layer
composes with BF's exact web-low optimized rows-2 serialization in one DLL
call. The DLL writes APP14, explicit EXIF APP1, ICC APP2, and COM before DQT,
then retains BF's exact DQT/SOF0/DHT/DRI/SOS/restart/EOI stream. Production
AHK only normalizes arguments and routes the call; whole-file hashing remains
test-only.

`FMT-JPEG-002B2BF` verifies AX's native web-low optimized rows-2 JPEG as one
complete byte-identical DLL-owned serialization. The existing call writes
APP14, split DQT, SOF0, optimized DHTs, DRI/SOS, entropy/restart markers, and
EOI without an AHK marker loop. Production AHK only normalizes arguments and
routes the call; whole-file hashing remains test-only.

`FMT-JPEG-002B2BE` verifies that the native JFIF/DPI layer composes with BD's
exact optimized rows-2 XMP/core-metadata serialization in one DLL call. The
DLL writes unit-1 JFIF density 300x150 before APP14, EXIF APP1, XMP APP1,
ICC APP2, and COM, then retains the exact DQT/SOF0/DHT/DRI/SOS/restart/EOI
stream. Production AHK only normalizes arguments and routes the call;
whole-file hashing remains test-only.

`FMT-JPEG-002B2BD` verifies that the native XMP marker layer composes with
BC's exact optimized rows-2 core-metadata serialization in one DLL call. The
DLL writes APP14, EXIF APP1, XMP APP1, ICC APP2, and COM before DQT, then
retains the exact DQT/SOF0/DHT/DRI/SOS/restart/EOI stream. Production AHK only
normalizes arguments and routes the call; whole-file hashing remains test-only.

`FMT-JPEG-002B2BC` verifies that the native JPEG metadata insertion layer and
BB's complete optimized rows-2 serialization compose without an AHK marker
loop. One existing save call writes APP14, explicit EXIF APP1, ICC APP2, and
COM before DQT, then preserves BB's exact DQT/SOF0/DHT/DRI/SOS/restart/EOI
stream. Native and facade whole-file bytes match Pillow; SHA-256 remains
test-only.

`FMT-JPEG-002B2BB` verifies the complete native JPEG serialization layer for
BA's source-DQT/default-CMYK-1x1 quality-keep optimized rows-2 route. The
existing one-call facade output is byte-identical to Pillow across APP14, split
DQT segments, SOF0, optimized DHTs, DRI/SOS, entropy/restart placement, and
EOI. Full-file SHA-256 remains test-only; production AHK performs no marker or
byte loop.

`FMT-JPEG-002B2BA` verifies the native optimized baseline restart layer at a
second row cadence for source-DQT/default-CMYK-1x1 quality-keep output. The
facade passes rows `2` through the existing one-call save ABI; native MCU
geometry derives DRI `26`, frequency collection and entropy output share six
DC predictor reset boundaries, and restart markers remain DLL-owned. Exact
DHT/SOS/entropy parsing stays test-only.

`FMT-JPEG-002B2AZ` verifies the native progressive restart layer at a second
row cadence for source-DQT/default-CMYK-1x1 quality-keep output. Rows `2`
become DRI `26` in all 18 scans; optimized frequency collection and entropy
output share six restart boundaries per scan. The facade remains one-call
routing, and exact DHT/SOS/entropy parsing stays test-only.

`FMT-JPEG-002B2AY` verifies the native progressive restart layer at a second
row cadence. Rows `2` become DRI `14` for interleaved/downsampled scans and
`26` for true-size C-only scans; optimized frequency collection and entropy
output share those boundaries across all 18 scans. The facade remains one-call
routing, and exact DHT/SOS/entropy parsing stays test-only.

`FMT-JPEG-002B2AX` verifies the native optimized baseline restart layer at a
second row cadence. The facade passes rows `2` through the existing one-call
save ABI; native MCU geometry derives DRI `14`, frequency collection and
entropy output share DC predictor resets, and restart markers remain inside
the DLL-owned stream. Exact DHT/SOS/entropy parsing stays test-only.

`FMT-JPEG-002B2AW` verifies that two existing native JPEG strategy layers
compose without a new route: AV's libjpeg-compatible CMYK h2v2 sample
preparation feeds AS's integer FDCT, optimized-Huffman builder, baseline
entropy writer, and row-restart state. The public web-low precedence route
still resolves options in the facade and performs one native save call; exact
DHT/SOS/entropy parsing remains test-only.

`FMT-JPEG-002B2AV` aligns native CMYK 4:2:0 sample preparation with libjpeg's
h2v2 downsampler. Each M/Y/K 2x2 sum now receives the alternating output-sample
bias `1,2,1,2...` before division by four, replacing floor-only averaging;
full-resolution C and non-h2v2 sampling paths are unchanged. The existing
real-YCCK facade route remains one native call, and exact DHT/SOS/entropy
assertions stay in tests rather than introducing production JPEG-byte work in
AHK.

`FMT-JPEG-002B2AU` aligns both initial progressive entropy stages with
libjpeg. AC-first frequency/output passes retain EOBRUN across blocks and share
flush boundaries at new nonzero symbols, restart/end boundaries, and run
`0x7fff`; scan-local restart intervals enter frequency collection so optimized
DHT construction sees the emitted symbol stream. DC-first frequency/output
passes use signed arithmetic right shift for negative coefficients rather than
the magnitude-based truncation required by AC-first. The existing real-YCCK
facade route remains one native call; AHK neither parses nor rewrites JPEG bytes
in production.

`FMT-JPEG-002B2AT` aligns the shared progressive AC-refine statistics pass with
the entropy-output pass. Both now retain libjpeg-compatible EOBRUN and
correction-bit state across blocks, flushing it before a newly nonzero
coefficient, at restart/end boundaries, at EOBRUN `0x7fff`, or before the
1000-bit correction buffer limit. Per-scan restart intervals enter the
frequency pass, so optimized DHT construction sees the exact symbol stream the
DLL later emits. The existing facade and additive save ABI remain one-call
routing; AHK neither parses nor rewrites JPEG bytes in production.

`FMT-JPEG-002B2AS` moves the shared JPEG coefficient and optimized-Huffman hot
paths onto libjpeg-compatible native algorithms. `jpeg_fdct_quantize_samples`
now uses the two-pass integer `JDCT_ISLOW` transform and divides by the
quantization table scaled by eight, so all 676 blocks of the bounded real-YCCK
fixture match Pillow's quantized coefficients exactly. The optimized table
builder uses libjpeg's 257-symbol procedure, including pseudo-symbol 256,
high-symbol tie selection, 16-bit length redistribution, and original
code-size symbol ordering. DHT payload and entropy generation remain entirely
inside the DLL; the facade only resolves quality/qtables/subsampling and makes
the existing one-call save dispatch.

`FMT-JPEG-002B2AR` keeps quality precedence at the facade normalization
boundary. Quality keep/preset is resolved before lower-priority qtables or
subsampling values are parsed, so shadowed valid and invalid caller values
cannot affect routing. The resolved qtables/sampling, DPI, XMP, comment, ICC,
EXIF, and restart interval still enter one existing DLL call; AHK performs no
JPEG-byte rewrite or pixel traversal.

`FMT-JPEG-002B2AQ` first isolated the shared floating-FDCT half-tie drift rather
than adding a fixture-specific pixel correction. Its scaled-ULP stabilization
proved the immediate qtables-sentinel case; `FMT-JPEG-002B2AS` subsequently
supersedes that floating transform with integer `JDCT_ISLOW` across the shared
encoder. The AQ facade generalization of AP's baseline metadata sentinel
condition to qtables keep/preset remains in place; encoding is still one DLL
call.

`FMT-JPEG-002B2AP` keeps DPI/JFIF composition on AO's same one-call DLL
route. The facade normalizes the DPI pair and forwards it with resolved
qtables plus all metadata; native owns JFIF-before-APP14 ordering, density
patch/readback, marker insertion, restart entropy, and output pixels.

`FMT-JPEG-002B2AO` extends the same one-call real-YCCK architecture to XMP
plus core metadata. The facade still resolves keep/preset qtables and sampling
and dispatches the existing additive qtables metadata restart export. The DLL
owns EXIF/XMP/ICC/COM ordering, exact XMP storage/readback, DQT/SOF/DHT,
restart entropy, and output pixels; AHK adds no JPEG byte or pixel loop.

`FMT-JPEG-002B2AN` keeps opened real-YCCK quality-sentinel metadata restart
saves on the existing native qtables pipeline. The facade resolves keep or
`web_low` into concrete qtables and CMYK sampling, validates the bounded
no-XMP/no-DPI baseline core-metadata composition, and dispatches one additive
DLL call. APP14 normalization, EXIF/ICC/COM insertion, DQT/SOF/DHT writing,
restart entropy structure, and reopen pixels remain DLL-owned; AHK performs no
JPEG marker rewriting or per-pixel work.

`FMT-JPEG-002B2AM` applies the sampled-progressive dual-view architecture to
RGB. The native RGB preparer keeps MCU-padded Y/Cb/Cr vectors for interleaved
DC scans. The progressive encoder derives a separate true-size raster Y vector
for every Y-only AC scan, so component scan order and row-restart intervals do
not inherit right/bottom MCU padding. The facade already forwards the public
default-4:2:0 progressive restart call directly to this encoder. DCT/
quantization, optimized table construction, DQT/SOF/DHT/DRI/RST output, and
reopen decoding all remain native; AHK adds no byte editing or pixel traversal.

`FMT-JPEG-002B2AL` extends the DLL-owned sampled progressive CMYK restart
architecture to non-MCU-aligned real-YCCK input. Interleaved DC scans retain
the MCU-padded C block sequence required by JPEG MCU order; single-component C
AC scans consume a separate true-size raster block view, so 4:2:0 row restart
intervals and markers follow Pillow's 13x13 component grid instead of the
internal 14x14 padded grid. The facade resolves quality keep or quality preset
precedence before strategy selection and forwards only normalized qtables,
sampling, codec, and restart arguments. Allocation, DCT/quantization, DQT/SOF/
DHT/DRI/RST emission, and reopen decoding remain native, with no AHK byte or
pixel traversal.

`FMT-JPEG-002B2AK` keeps opened-CMYK restart keep/preset execution inside the
same DLL-owned qtables restart architecture. The facade resolves source or
`web_low` qtables, Pillow's CMYK default-versus-explicit sampling rules, and
implicit opened COM, then forwards baseline/optimized/progressive calls to the
existing additive metadata/DPI restart export. Native code owns DQT/SOF/DHT/
DRI/RST output and reopened pixels; AHK performs no JPEG-byte editing, entropy
work, or pixel traversal. Guards limit the new sentinel route to CMYK while
RGB/L preset-restart expansion and broader YCCK matrices stay separate.

`FMT-JPEG-002B2AJ` composes explicit XMP with that real-subsampling CMYK custom-
qtables restart route without changing DLL architecture. The same additive
metadata/DPI-capable qtables restart export owns both APP14/XMP/DQT and JFIF/
APP14/EXIF/XMP/ICC/COM/DQT ordering, sampled SOF geometry, entropy, and DRI/RST
output. The facade removes only its XMP exclusion and forwards normalized
arguments; keep/preset sentinels remain bounded separately, and no AHK JPEG-
byte editing, entropy work, or pixel traversal occurs.

`FMT-JPEG-002B2AI` composes DPI/JFIF and explicit comment/ICC/EXIF metadata with
that real-subsampling CMYK custom-qtables restart route without changing DLL
architecture. The existing additive metadata/DPI-capable qtables restart
export owns JFIF/APP14/metadata/DQT ordering, sampled SOF geometry, entropy,
and DRI/RST output. The facade removes only the prior no-DPI baseline term and
forwards normalized arguments; XMP and keep/preset remain bounded separately,
and no AHK JPEG-byte editing, entropy work, or pixel traversal occurs.

`FMT-JPEG-002B2AH` composes explicit comment/ICC/EXIF metadata with that real-
subsampling CMYK custom-qtables restart route without changing DLL
architecture. The existing additive metadata/DPI-capable qtables restart
export already owns APP14/metadata/DQT ordering, sampled SOF geometry, entropy,
and DRI/RST output. The facade admits only the bounded no-DPI/no-XMP baseline
combination and forwards normalized arguments; no AHK JPEG-byte editing,
entropy work, or pixel traversal occurs.

`FMT-JPEG-002B2AG` composes explicit DPI/JFIF with that real-subsampling CMYK
custom-qtables restart route without changing DLL architecture. The existing
additive DPI-capable qtables metadata restart export already owns APP0/JFIF
ordering, DQT, sampled SOF geometry, entropy, and DRI/RST output. The facade
only admits the bounded combination and forwards normalized arguments; no AHK
JPEG-byte editing, entropy work, or pixel traversal occurs.

`FMT-JPEG-002B2AF` keeps real-subsampling CMYK custom-qtables restart geometry
inside the DLL. The shared encoder derives row intervals from sampled MCU
width and assigns C-only progressive scans their block-local DRI interval,
emitting a new DRI segment only when scan geometry changes. The facade merely
opens the bounded no-DPI route and normalizes arguments; no AHK byte editing,
entropy work, or pixel traversal occurs.

`FMT-JPEG-002B2AE` keeps CMYK custom-qtables restart+DPI composition inside the
DLL. A new additive export supplies DPI to the existing shared qtables metadata
restart encoder, which writes JFIF before APP14 and then owns DQT, SOF,
baseline/optimized/progressive entropy, DRI/RST, and metadata patching. The
facade only normalizes the bounded strategy and arguments; it performs no JPEG
byte editing or pixel traversal. Existing non-DPI facade calls use the same new
entry point with `has_dpi=0`, while the legacy export remains ABI-stable.

LAB-to-PA option routing deliberately remains outside that P-only adaptive
quantize architecture. Pillow ignores dither, palette, and colors for PA and
always selects the unsupported direct LAB conversion, including empty images.
The facade therefore normalizes this mode-pair error before generic dither
validation, while existing native plain conversion remains the raw rejection
authority; no RGB intermediate, synthetic output, or AHK pixel loop is added.

RGB/RGBA/RGBX numeric conversion is DLL-owned as well: I writes rounded
fixed-point RGB luma into little-endian int32 slots, while F writes unrounded
weighted RGB luma into little-endian float32 slots. The shared loop steps by
source channels and ignores RGBA alpha or RGBX X.

Byte-grayscale numeric promotion stays DLL-owned too. One loop promotes mode
`1` logical pixels to 0/255, promotes `L` bytes directly, and reads only the L
byte from `LA` while ignoring alpha. It writes little-endian int32 or float32
targets and preserves legal empty shapes through the existing conversion ABI.

YCbCr numeric conversion is also a single native operation, but follows
Pillow's composed route rather than promoting Y directly. The loop first clips
each YCbCr pixel through the shared RGB lookup kernel, then applies the same
rounded int32 or unrounded float32 RGB luma kernels used by RGB-like sources.

CMYK numeric conversion extends the existing DLL-owned CMYK loop. The shared
CMYK-to-RGB helper applies black-channel scaling with Pillow-compatible
255-denominator rounding, after which I and F use the same rounded or
unrounded RGB luma writes without a second image traversal.

HSV numeric conversion is likewise DLL-owned. One loop expands each pixel
through the established float-based hue-sector helper and immediately applies
the shared rounded int32 or unrounded float32 RGB luma write, preserving exact
HSV-to-RGB composition without an intermediate image.

HSV common luminance conversion follows the same native composition boundary.
One loop expands HSV through the hue-sector helper, computes rounded fixed-point
RGB luma, and writes either L or `[L,255]` LA bytes without an intermediate
image or facade pixel traversal; legal zero-width or zero-height shapes remain
empty native handles.

HSV-to-RGBA conversion also stays inside one native traversal. Each HSV pixel
passes through the hue-sector-sensitive RGB helper and receives opaque alpha
`255`, exactly matching Pillow's composed `HSV -> RGB -> RGBA` bytes without
an intermediate image or facade pixel loop; legal empty shapes preserve their
dimensions.

HSV-to-RGBX reuses that same native four-channel traversal. The hue-sector RGB
bytes are unchanged and the fourth byte is X=`255`, exactly matching Pillow's
composed `HSV -> RGB -> RGBX` route without an intermediate image or facade
pixel loop; empty shapes retain their dimensions.

HSV mode-1 conversion stays in the native dither kernels. NONE expands HSV and
applies the weighted RGB threshold at `128000`; Floyd-Steinberg and the facade
default expand each HSV pixel inside the existing error-diffusion traversal.
This preserves Pillow's RGB-composed behavior without a temporary RGB/L image
or a facade pixel loop.

YCbCr mode-1 conversion uses the same ownership boundary but first applies the
established clipped YCbCr-to-RGB lookup kernel. NONE thresholds weighted RGB;
Floyd-Steinberg/default feed truncated RGB luma into native error diffusion.
Direct Y is intentionally not used, and no intermediate RGB image is allocated.

YCbCr-to-RGBA conversion is likewise one native operation. Each source pixel
passes through the same clipped YCbCr-to-RGB lookup kernel and receives opaque
alpha `255` in the destination, exactly matching Pillow's composed
`YCbCr -> RGB -> RGBA` bytes without an intermediate image or facade pixel
traversal. Legal zero-width or zero-height shapes remain empty native handles.

YCbCr-to-RGBX reuses that same native four-channel traversal. The clipped RGB
bytes are unchanged and the fourth byte is X=`255`, exactly matching Pillow's
composed `YCbCr -> RGB -> RGBX` route without an intermediate image or facade
pixel loop; empty shapes retain their dimensions.

YCbCr-to-CMYK stays inside the native CMYK traversal. Each pixel first uses
the clipped YCbCr-to-RGB helper, then writes `255-R/G/B` and K=`0`, exactly
matching Pillow's composed `YCbCr -> RGB -> CMYK` bytes without a temporary
image or facade pixel loop; legal empty shapes preserve their dimensions.

HSV-to-CMYK also stays inside the native CMYK traversal. Each pixel first uses
the hue-sector-sensitive HSV-to-RGB helper, then writes `255-R/G/B` and K=`0`,
exactly matching Pillow's composed `HSV -> RGB -> CMYK` bytes without a
temporary image or facade pixel loop; legal empty shapes preserve dimensions.

YCbCr-to-HSV composes the clipped YCbCr-to-RGB lookup helper with the exact
RGB-to-HSV rounding helper in one native traversal. It matches Pillow's
composed `YCbCr -> RGB -> HSV` bytes without an intermediate image or facade
pixel loop, and legal empty shapes preserve their dimensions.

HSV-to-YCbCr composes the hue-sector-sensitive HSV-to-RGB helper with the
exact RGB-to-YCbCr lookup helper in one native traversal. It matches Pillow's
composed `HSV -> RGB -> YCbCr` bytes without an intermediate image or facade
pixel loop, and legal empty shapes preserve their dimensions.

CMYK-to-RGBX extends the existing black-channel-scaled CMYK-to-RGB traversal
and appends X=`255`. It preserves Pillow's 255-denominator RGB rounding without
an intermediate image or facade pixel loop; legal empty shapes retain their
dimensions.

CMYK-to-YCbCr composes that same black-channel-scaled RGB expansion with the
exact RGB-to-YCbCr lookup helper in one native traversal. It matches Pillow's
composed bytes without an intermediate image or facade pixel loop and preserves
legal empty shapes.

RGBX mode-1 conversion stays in the dither kernels: NONE applies the weighted
RGB threshold, while Floyd-Steinberg and the facade default apply native error
diffusion. Both paths stride four source bytes and ignore X.

Core `1`, `L`, `LA`, `RGB`, `RGBA`, `P`, and `CMYK` conversion paths, native BMP/PPM/QOI/TGA/XBM/PNG/JPEG/TIFF/GIF file open/save plus ICO open and custom/default-size PNG-backed ICO save plus BMP-backed ICO save via `bitmap_format="bmp"`, native GIF later-frame local-rectangle composition with disposal `1`/`2`/`3`, transparent-pixel read-side coverage, `ImageSequence.Iterator` live seek-state parity, selected save-side optimized local-rectangle GIF animation writes including caller-provided transparency in optimized and `optimize=False` bounded fixtures, native GIF transparency metadata and P-mode single-frame transparency save options, native `1`/`L`/`P` linear and radial gradient generation, native Mandelbrot, noise, and spread effect generation, `CMYK` handle/raw-byte operations, narrow `I` and `F` scalar storage foundations, mode `1` bit-packed byte import/export and logical chops, native `P` RGB/RGBA palette metadata including Pillow-style `L.putpalette(...) -> P`, native P/L palette remapping, histogram/extrema/entropy/bounding-box/projection/color-count scans, histogram-backed `ImageStat.Stat` properties with mode `1`/`L` mask support, fixed-LUT and histogram-derived `ImageOps` transforms for supported Pillow modes including mode `1` invert, `ImageOps.deform` dispatch through native MESH transforms, `ImageOps.colorize` L-to-RGB mapping, `Image.eval`/`Image.point` LUT mapping, `Image.Filter(ImageFilter.Kernel(...))`, `Image.Filter(ImageFilter.RankFilter(...))`, `Image.Filter(ImageFilter.ModeFilter(...))`, `Image.Filter(ImageFilter.BoxBlur(...))`, `Image.Filter(ImageFilter.GaussianBlur(...))`, `Image.Filter(ImageFilter.UnsharpMask(...))`, `Image.Filter(ImageFilter.Color3DLUT(...))`, ImageEnhance composition over native blend/convert/filter/stat operations, ImageDraw rectangle, rounded rectangle, bitmap, floodfill, ellipse, arc, chord, pieslice, line, point, polygon, and printable ASCII default-font single-line plus multiline text anchors and bounded strokes for mutation/bbox paths, `ImageFont.LoadDefault()` native handle plumbing for current text metrics/draw calls plus default font metadata and variant cloning, `ImageOps.crop`/`ImageOps.expand`, direct box-aware and `reducing_gap`-aware `Image.resize`, scaled, proportional, fitted, padded, and integer-reduced resize helpers, current `ImageChops` helpers and binary operations including verified `LA` and `CMYK`, mask compositing, image paste, masked paste, color-source paste, RGBA alpha compositing including in-place destination/source geometry, masked equalize, masked and preserve-tone autocontrast histograms, L-band split, L-band merge, Python-like AFFINE/EXTENT/PERSPECTIVE/QUAD/MESH transform dispatch, general NEAREST/BILINEAR/BICUBIC affine, perspective, quad, and mesh transforms, NEAREST/BILINEAR/BICUBIC affine rotate, and all current Pillow resize and filter hot paths are single native operations so the wrapper does not fall back to per-pixel AHK loops as mode coverage grows.

`ImageStat.Stat` currently supports image inputs, image inputs with same-size mode `1` or `L` masks, and precomputed histogram lists. Its properties follow Pillow's histogram-derived formulas for `extrema`, `count`, `sum`, `sum2`, `mean`, `median`, `rms`, `var`, and `stddev`. A mask on a histogram list is rejected because the native mask path requires image storage.

`ImageOps.equalize` and `ImageOps.autocontrast` currently implement common histogram/LUT paths with mode `1` or `L` masks for supported `L`/`RGB` images. Equalize also mirrors Pillow's mode `P` special case by converting through the native RGB palette and returning `RGB`. Autocontrast also supports `cutoff`, `ignore`, and Pillow's `preserve_tone` mode.

Resize behavior follows Pillow 11.3.0 for the supported 8-bit modes, including verified CMYK coverage. `NEAREST` uses Pillow's affine-scale coordinate progression. `BOX`, `BILINEAR`, `HAMMING`, `BICUBIC`, and `LANCZOS` use separable two-pass filtering with Pillow-style fixed-point coefficient normalization. `Image.Resize(size, resample, box)` maps directly to the native box-resize path so AHK does not need a crop intermediate before resampling. `Image.Resize(size, resample, box, reducingGap)` also stays in the DLL: for large downsampling it computes Pillow's safe reduce box, performs native integer reduction, and runs final box resize against the reduced temporary. `Image.Thumbnail(...)` follows Pillow's aspect-preserving in-place API at the facade layer and delegates the actual resampling to those native resize paths. Non-NEAREST `LA` and `RGBA` resize use premultiplied color internally and preserve identity resizes as byte copies.

`Image.frombytes` and `Image.tobytes` keep raw byte import/export in the DLL for common interop layouts such as mode `1` bit-packed rows, direct `CMYK`, direct little-endian `I`/`I;32`, Pillow-compatible unsigned 16-bit and endian 32-bit `I` decode aliases, `I;16B` encode, direct little-endian `F`/`F;32F`, endian `F;32BF`/`F;32NF` decode, `F;32NF` encode, BGR, BGRA, ARGB, ABGR, RGBX, BGRX, LAB signed-channel packing, direct interleaved PA, and bottom-up stride-based source rows. Mode `1` stays unpacked as one byte per pixel inside the native handle for fast bulk operations and memory sharing, while facade `ToBytes()` returns Pillow's bit-packed external representation. Mode `LAB` stores public L/A/B bytes and xors A/B with `0x80` only at DLL raw import/export boundaries; default facade `ToBytes()` selects that native raw encoder without an AHK pixel loop. Mode `PA` stores direct P/A bytes and keeps explicit RGB/RGBA palette metadata on the native handle; facade palette access uses bulk DLL calls. Palette conversion stays in DLL-owned loops: P-to-PA initializes A from palette alpha or 255; P-to-LA uses palette luma/alpha; P-to-YCbCr uses palette RGB; PA-to-P drops pixel A while retaining the palette; PA-to-RGBA uses pixel A without combining palette alpha; PA-to-RGB/L discards alpha; PA-to-LA pairs fixed-point palette luma with pixel A; and PA-to-CMYK/YCbCr/HSV resolves palette RGB through existing native color kernels while ignoring alpha. Mode `I` stays as four-byte native storage while the facade exposes signed int32 `GetPixel`, `PutPixel`, `GetData`, and `PutData` semantics. Mode `F` stays as four-byte float32 storage while the facade exposes scalar float `GetPixel`, `PutPixel`, `GetData`, and `PutData` semantics.

`Image.open` and `image.save` expose native file-format paths for BMP, PPM, QOI, TGA, XBM, PNG, JPEG, TIFF, and GIF, with native ICO open plus PNG-backed and BMP-backed ICO save. The BMP layer parses and writes uncompressed Windows BMP files, including Pillow-compatible 24-bit RGB save bytes, 8-bit grayscale BMP, and Pillow-style 32-bit RGBA BMP saves that reopen as RGB. The PPM layer parses plain and binary Netpbm `P1`/`P2`/`P3`/`P4`/`P5`/`P6` files into `1`, `L`, `I`, and `RGB` handles, including PBM/Pillow mode `1` bit inversion, Pillow-style low-`maxval` scaling, and high-bit-depth grayscale PGM as little-endian 32-bit `I` storage. Native save writes Pillow's default binary `P4`/`P5`/`P6` output for `1`, `L`, and `RGB`; mode `I` saves as high-bit-depth `P5` with `maxval=65535` and big-endian 16-bit samples clipped to `0..65535`, matching Pillow's `I;16B` PGM path. The QOI layer implements native RGB/RGBA Quite OK Image decode/encode with Pillow-compatible byte output. The TGA layer implements Pillow-compatible uncompressed and RLE `L`, `RGB`, `RGBA`, and 24-bit color-mapped `P` Truevision files, including bottom-left save order, BGR/BGRA/palette-BGR storage, TGA 2.0 footer bytes, row-bounded RLE packets, and top/bottom plus left/right origin handling on open. The facade maps `Image.Save(..., { Rle: true })` and `compression="tga_rle"` to the native TGA RLE option path. The XBM layer implements mode `1` X11 bitmap open/save with Pillow's low-bit-first file byte order while keeping the DLL's unpacked mode `1` storage internally; open exposes integer hotspot pairs as `info["hotspot"]`, and `Image.Save(..., { Hotspot: [x, y] })` writes Pillow-style `im_x_hot`/`im_y_hot` through the native options path. The ICO layer uses WIC decode inside the DLL and returns the largest available icon frame as a public `RGBA` handle; PNG-backed save writes Pillow's default icon sizes `16/24/32/48/64/128/256` when each size fits inside the source image, and `Image.Save(..., { Sizes: [...] })` routes custom requested pairs through the native options path with Pillow-style sorting, de-duplication, skipping, and thumbnail-contained LANCZOS resizing. `Image.Save(..., { BitmapFormat: "bmp" })` uses native DIB-backed ICO entries for Pillow's `1`/`L`/`P`/`RGB`/`RGBA` save modes, including doubled DIB heights and raw 1-bit AND masks for non-32-bit entries; other `bitmap_format` strings fall back to PNG-backed output like Pillow. ICO `append_images` and caller-selected frame open size remain future surfaces. The PNG layer keeps `L`, `LA`, `P`, `RGB`, and `RGBA` decode/encode work inside the DLL; WIC handles the common codec path while native chunk writing preserves Pillow-style `LA`, palette-mode PNG semantics, stored `compress_level=0`, and metadata chunks that WIC does not reliably preserve for this project. Facade `compress_level=0` uses the native chunk writer for stored zlib output across those supported PNG modes, facade `dpi`/`Dpi` writes a PNG `pHYs` chunk through the extensible `pillow_c_image_save_png_options` ABI, and PNG open reads that `pHYs` metadata back into `info["dpi"]`. Nonzero compression levels currently reuse the existing encoder path unless metadata forces native chunk writing; a tuned native deflate strategy is still future work. The JPEG layer keeps lossy `L` and `RGB` decode/encode in the DLL through WIC, with component-count probing so grayscale JPEGs reopen as `L` instead of being promoted by the wrapper. Facade `quality` and `dpi`/`Dpi` route through `pillow_c_image_save_jpeg_options`: quality stays in WIC's encoder property bag, while JFIF density is patched in native code after the encoder releases the file so wrapper code does not perform byte-level JPEG edits. JPEG open reuses the native marker scan to expose Pillow-style `info["dpi"]`, `info["jfif"]`, `info["jfif_version"]`, `info["jfif_unit"]`, and `info["jfif_density"]` without AHK-side byte parsing. The TIFF layer keeps WIC-backed `L`, `RGB`, and `RGBA` open/save in the DLL for lossless interchange and exposes native frame count plus frame-open ABI so the facade can implement `n_frames`, `is_animated`, `tell`, `seek`, and `ImageSequence.Iterator` for basic multiframe TIFF files. The GIF layer opens frame `0` as mode `P` with palette metadata, opens verified later frames as composited `RGB` logical-screen images through native GIF block parsing and LZW decode including local-rectangle disposal `1`/`2`/`3` and transparent-pixel behavior, saves `P` directly, saves exact-color `L`/`RGB` single-frame images by native palette quantization, saves exact-color `RGBA` single-frame images when the effective palette fits 256 entries with `alpha == 0` mapped to one transparent index and partial alpha treated as opaque, saves bounded default single-frame `RGB` and `RGBA` images above 256 effective colors through deterministic weighted median-cut style fallbacks while preserving the verified RGBA transparency rule, and exposes native frame count, frame-open, metadata, single-frame save, and animation save ABI including optimized local-rectangle writes, explicit `include_color_table`/`optimize`, caller-provided transparency for optimized and `optimize=False` bounded fixtures, the bounded logical-screen `background` byte path, and the first bounded post-`disposal=2` transparency-aware re-diff path. The facade updates `info["duration"]`, `info["loop"]`, `info["background"]`, and `disposal_method` from DLL-parsed GIF blocks on open and seek, preserves `info["transparency"]` only while the current GIF frame remains P-mode like Pillow, matches Pillow's `ImageSequence.Iterator` live seek-state references for a complex transparent local-rectangle GIF fixture, and maps `Image.Save(..., { SaveAll, AppendImages, Duration, Loop, Disposal, Transparency, Background })` to a single native GIF animation write for the supported same-size `P`-mode sequence path, including the covered local-rectangle differencing, caller transparency fixtures, bounded background-byte fixture, and the first covered post-restore re-diff fixture. Local Pillow 11.3.0 source/probes still show that `background` is metadata rather than the driver of the next optimized bbox on the covered fixtures; the sharper remaining GIF animation work is the next bounded `disposal=2` edge case after the now-covered transparency-aware re-diff path. Same-size exact-color `L`/`LA`/`RGB`/`RGBA` animation frames and bounded lossy mixed RGB/RGBA animation frames now quantize to temporary native P images before the animation writer; GIF save ignores `dither` like Pillow, `Image.Convert("1", dither)` supports NONE and Floyd-Steinberg through the native dither exports, and public `Image.Quantize` covers exact-color and palette-mapping paths, while full Pillow quantize algorithm parity (median-cut/octree/libimagequant public behavior) remains a future surface.

The GIF metadata ABI now has an extended form that also reports Graphic Control Extension transparency. The facade exposes that as `info["transparency"]`, and `Image.Save(..., "GIF", { Transparency: index })` writes a native single-frame P-mode GIF89a transparency extension with the index masked to one byte.

Current GIF save quantization has two covered extensions beyond the exact-color paths: RGB single-frame images with more than 256 unique colors and RGBA single-frame images with more than 256 effective colors both use deterministic weighted median-cut style fallbacks in the DLL. The compatibility target for these slices is approximate reopened RGB pixels for bounded Pillow 11.3.0 fixtures plus the verified RGBA `alpha == 0` transparency boundary, not byte-identical GIF palette order. GIF save ignores `dither` like Pillow, and animation frames now quantize through the exact-color and bounded lossy mixed RGB/RGBA native paths; full Pillow quantize algorithm parity remains future work.

`Image.getdata` and `Image.putdata` expose Pillow-like pixel sequence ergonomics while keeping the native handle as the storage authority. `GetData` exports a bulk byte snapshot, and `PutData` packs AHK values once before calling the native `put_data` prefix-writer instead of crossing the DLL boundary per pixel.

`ImageColor.getrgb` and `ImageColor.getcolor` live in the AHK facade because color parsing is call-boundary normalization, not an image hot path. The facade follows Pillow's named CSS colors, hex, `rgb(...)`, `rgba(...)`, `hsl(...)`, and `hsv(...)` string parsing, converts to the target mode once, and then passes caller-packed bytes into native fill, draw, paste, expand, pad, and transform operations.

`ImageOps.expand`, `ImageOps.pad`, and related facade fill parsing support Pillow-style scalar and tuple fill colors for current core modes, including `LA` single-value fills as transparent luminance and two-value `[l, a]` fills before dispatching to native geometry paths.

`ImageDraw.Draw(image).Rectangle(...)` mutates the image handle in place through a native rectangle path. Fill, outline, width, inclusive coordinates, clipping, and reversed-coordinate rejection are modeled from Pillow 11.3.0's `ImageDraw.rectangle` wrapper and `ImagingDrawRectangle` C implementation. `ImageDraw.Draw(image).RoundedRectangle(...)` keeps Pillow's rounded-rectangle composition inside one native call, using native pieslice/arc corners plus rectangle bars and native rectangle/ellipse fallbacks for degenerate cases. `ImageDraw.Draw(image).Bitmap(...)` follows Pillow's `ImagingFill2` mask rules for mode `1`, `L`, and `RGBA` masks while keeping color fill and alpha blending in the DLL. `ImageDraw.floodfill(...)` keeps Pillow's Python flood-fill semantics in a native queue walk, including threshold matching, border-mode filling, seed negative-coordinate normalization, and no-op out-of-range seeds. `ImageDraw.Draw(image).Ellipse(...)` uses Pillow 11.3.0's integer span generator for fill and outline widths while keeping clipping in the native hot path. `ImageDraw.Draw(image).Arc(...)`, `ImageDraw.Draw(image).Chord(...)`, and `ImageDraw.Draw(image).Pieslice(...)` reuse that span foundation plus Pillow's clip-ellipse half-plane tree, including angle normalization, full-circle delegation to ellipse paths, width handling, chord/pie side outlines, center joins, and clipping. `ImageDraw.Draw(image).Line(...)` covers Pillow's ordinary multi-segment line path, including `width <= 1` final endpoint draw, `width > 1` quadrilateral segment filling, clipping behavior from `draw_lines`/`ImagingDrawLine`/`ImagingDrawWideLine`, and `joint="curve"` rounded joints for wide polylines by adding Pillow-style pieslice joints and gap-cover lines inside the DLL. `ImageDraw.Draw(image).Point(...)` batches Pillow's individual point writes, allowing empty and single-point coordinate lists while clipping out-of-bounds points. `ImageDraw.Draw(image).Polygon(...)` covers fill, `width <= 1` outlines, two-point line-like polygons, and Pillow's mask-assisted wide outline path using a native polygon mask plus `width * 2 - 1` wide-line strokes clipped to the filled polygon interior. `ImageDraw.Draw(image).Text(...)`, `TextLength(...)`, `TextBbox(...)`, `MultilineText(...)`, and `MultilineTextBbox(...)` now accept the initial `Pillow.ImageFont.LoadDefault()` facade object, which owns a DLL font handle; current glyph coverage is Pillow 11.3.0's embedded default font for printable ASCII only. Single-line and multiline default-font text support bounded `stroke_width`/`stroke_fill`, single-line text supports Pillow anchor pairs, and multiline text preserves Pillow's stroke-aware line spacing, trailing-empty-line bbox behavior, `align="left"|"center"|"right"` drawing offsets, and horizontal-text anchors `l/m/r` + `a/m/d/s` in one DLL call. The default font facade also exposes Pillow-compatible `getmetrics`, `getname`, and `font_variant` behavior for that embedded default font. The AHK facade only packs colors, manages handle lifetime, and dispatches one DLL call for covered draw operations.

`Image.reduce` supports native integer block downsampling for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`, including Pillow-style output-size ceiling, optional box regions, and allocation-avoiding `_into` calls. `LA` and `RGBA` reduce through Pillow's premultiplied-alpha semantics before converting back to the public mode.

`ImageFilter.Kernel` currently supports Pillow's 3x3 and 5x5 kernel path for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. Native filtering copies border pixels unchanged, applies Pillow's vertical kernel flip, and uses Pillow-style half-up rounding before clipping. Pillow's fixed-kernel built-ins `BLUR`, `CONTOUR`, `DETAIL`, `EDGE_ENHANCE`, `EDGE_ENHANCE_MORE`, `EMBOSS`, `FIND_EDGES`, `SHARPEN`, `SMOOTH`, and `SMOOTH_MORE` reuse the same native kernel path.

`ImageFilter.RankFilter` and the `MinFilter`, `MedianFilter`, and `MaxFilter` helpers currently support arbitrary positive odd sizes for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. Native rank filtering clamps source coordinates at image edges, matching Pillow's rank-filter behavior for windows larger than the image.

`ImageFilter.ModeFilter` currently supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. It follows Pillow's single-band `ModeFilter.c` semantics independently per channel: outside-image pixels are ignored, even sizes use `size // 2` as the radius, sparse values with counts of one or two preserve the original pixel, and ties keep the smaller pixel value.

`ImageFilter.BoxBlur` currently supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. It uses Pillow's separable fixed-point box blur, including fractional radius weighting and endpoint edge extension. This native path is the shared primitive for Gaussian blur and future unsharp-mask work instead of adding AHK-side pixel loops.

`ImageFilter.GaussianBlur` currently supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. It follows Pillow's three-pass Gaussian approximation by transforming the requested radius into a BoxBlur radius, running horizontal passes before vertical passes, and preserving allocation-avoiding `_into` behavior for native callers.

`ImageFilter.UnsharpMask` currently supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. It reuses the native GaussianBlur path, then applies Pillow's per-channel `abs(original - blurred) > threshold` rule with integer `percent` strength and byte clipping.

`ImageFilter.Color3DLUT` currently supports source modes with at least three bands, including `RGB`, `RGBA`, and `CMYK`, and target modes with enough bands for the table. The native path follows Pillow's fixed-point table preparation and trilinear interpolation, keeps channel order as channels-first within the flattened table, and preserves the fourth input band when a 3-channel table writes to a 4-band target.

`ImageEnhance.Brightness`, `Contrast`, `Sharpness`, and `Color` follow Pillow's degenerate-image plus blend model. The facade composes existing native image operations instead of AHK pixel loops: blend, mode conversion, histogram mean, `ImageFilter.SMOOTH`, alpha reinsertion, and byte clipping all stay in the native path. `Color` supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`; RGBA uses Pillow's `LA` intermediate mode so alpha is preserved while color is desaturated, while CMYK uses Pillow's `L` to `CMYK` degenerate path.

`Image.rotate` currently supports Pillow-style geometry, expansion, center, translate, and fill color for `NEAREST`, `BILINEAR`, and `BICUBIC`, including verified CMYK fill-color packing and sampling plus `LA`/`RGBA` premultiplied filtered sampling. Additional rotate resamplers should build on the same affine ABI instead of adding wrapper loops.

`Image.Transform(...)` covers Pillow's `AFFINE`, `EXTENT`, `PERSPECTIVE`, `QUAD`, and `MESH` methods through native paths for `NEAREST`, `BILINEAR`, and `BICUBIC`. `Image.TransformAffine` remains as the lower-level affine convenience entry, CMYK is covered by the same channel-generic native geometry path, and `LA`/`RGBA` filtered transforms sample color in premultiplied-alpha space.

## Performance Direction

Scalar behavior must stay tested before SIMD or threading is introduced. Later optimizations should happen behind the same ABI so wrapper code does not change when the native backend improves.
