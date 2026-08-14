# Native ABI

All exported functions currently return an integer status code:

```text
0   success
-1  null pointer
-2  invalid length
-3  invalid argument
-4  allocation failed
-5  mismatch
```

`pillow_c_status_message` maps status codes to stable UTF-8 text for wrapper exceptions.

Current native ownership after `ARCH-MOD-012` includes a second-level
operations split. `pillow_c_ops.cpp` owns arithmetic, conversion,
palette/compositing, ImageChops, point/LUT, quantization, and spatial exports;
`pillow_c_ops_statistics.cpp` owns histogram, entropy, extrema, bbox,
projection, getcolors, and autocontrast algorithms plus eleven unchanged DLL
exports. `pillow_c_ops_internal.h` is a private C++ seam, not a DLL ABI header.
Existing exported names, signatures, status codes, image-handle ownership
rules, source-pointer lifetimes, and facade routes remain unchanged; the
quantize packet adds `pillow_c_image_quantize_options`, and the bounded JPEG
marker-stream packets add `pillow_c_image_save_jpeg_extra_options`,
`pillow_c_image_save_jpeg_metadata_extra_encode_options`, and
`pillow_c_image_save_jpeg_qtables_metadata_extra_encode_options`, plus
`pillow_c_image_save_jpeg_metadata_restart_marker_extra_encode_options` and
`pillow_c_image_save_jpeg_metadata_keep_rgb_extra_encode_options` and
`pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_extra_encode_options`.
Release x64 builds with `0 Warning(s), 0 Error(s)`; source/DLL export parity is
`467/467`; the full AHK suite is `2810/2810`; and the current DLL SHA-256 is
`6604522ED0B4458DF25B5A4BE213E0D3959327A4AECF884AB240AD09E7E6C898`
(BEHAV-IM-001: the raw codec gains the per-row planar `;L` modes
(RGB;L/RGBA;L/LA;L/CMYK;L) on encode and decode, `I;32S`, and one
deliberate new export `pillow_c_image_get_raw_bytes_oriented` —
the existing `pillow_c_image_get_raw_bytes` keeps orientation 1 —
so the facade IM route can write Pillow's bottom-up planar
payloads; the DIB packet's mode-1 BMP branches ride the same slice).
`FMT-TIFF-003BG` changes no ABI: the BigTIFF save_all composition (chained
numeric multi-frame and per-frame metadata) is a lock-in over the existing
frames/metadata writers, verified against Pillow 11.3.0 ctypes.
`FMT-TIFF-003BJ` likewise changes no ABI: the mixed-size chained BigTIFF
frames lock-in completes the bounded BigTIFF save family over the
existing per-frame writer, verified against Pillow 11.3.0 ctypes.
`API-IMG-001D` changes no ABI: the facade `Image.GetIm()` accessor
returns the existing native handle with Pillow's closed-image error.
`API-IMG-001E` changes no ABI: the facade `ToQImage()`/`ToQPixmap()`/
`Show()` methods are explicit documented boundaries (no Qt binding or
viewer registry in this runtime) with Pillow-shaped errors.

## Mode I/F Point Transform ABI Behavior

`MODE-I-001B` adds one public export:
`pillow_c_image_point_transform(source, scale, offset, out_image)`.
Existing exported names, signatures, status codes, image-handle
ownership, source-pointer lifetimes, and facade routes remain
unchanged. The export applies `scale * x + offset` per little-endian
int32 sample with double math and C-cast truncation (mirroring Pillow's
C `point_transform` semantics) for `PILLOW_C_MODE_I` images, and per
little-endian float32 sample for `PILLOW_C_MODE_F` images
(`MODE-F-001B`, the same export name and signature — fractional scales
such as `0.5` are exact); other modes return
`PILLOW_C_INVALID_ARGUMENT`. The facade routes linear AHK callables on
mode I or F through this export and rejects list tables, non-linear
callables, the `modeName` output-mode parameter, and I;16 inputs with
Pillow's `point operation not supported for this mode` message.

No facade lifetime rule, fallback, or AHK per-pixel loop was added
beyond the mode I/F point family above.

## Numeric Transform Interpolation ABI Behavior

`MODE-NUM-001CH` adds no public export. Existing exported names,
signatures, status codes, image-handle ownership, source-pointer
lifetimes, and facade routes remain unchanged. The shared
`transform_with_mapper_into` loop used by the affine, perspective,
quad, and mesh exports now routes `PILLOW_C_MODE_I` and
`PILLOW_C_MODE_F` sources through per-sample helpers
(`bilinear_transform_numeric_sample`,
`bicubic_transform_numeric_sample`,
`write_transform_numeric_sample`) instead of interpolating the four
storage bytes as independent byte channels: NEAREST already whole-copied
4-byte samples, bilinear/bicubic now interpolate ONE 32-bit sample with
the same geometry as the byte-mode path, mode F stores the float32 cast
and mode I stores the int32 truncation toward zero. The facade
`TransformFillBuffer` packs numeric fill colors as one int32/float32
sample (scalars, single-element Arrays, and color names through the
grayscale map) with Pillow's `color must be int or single-element
tuple` / `must be real number, not tuple` rejections. Only the
AFFINE/EXTENT routes are verified against Pillow 11.3.0 for numeric
modes; rotate interpolation, I;16, and the other transform families
remain separate.

`MODE-NUM-001CI` adds no public export either: the rotate
bilinear/bicubic loops (`rotate_bilinear_into`,
`rotate_bicubic_into`) route `PILLOW_C_MODE_I` and `PILLOW_C_MODE_F`
through the same per-sample helpers (Pillow 11.3.0's `rotate()` builds
the same affine matrix as `rotate_affine_geometry` and dispatches
through the AFFINE transform path), while the rotate NEAREST path
already whole-copied samples with equivalent geometry; the rotate fill
reuses the numeric `TransformFillBuffer` packing.

`MODE-NUM-001CJ` adds no public export either and completes the
numeric transform family: `mesh_transform_image_into` routes
`PILLOW_C_MODE_I` and `PILLOW_C_MODE_F` through the same per-sample
helpers (its own per-byte channel loop was the last holdout), while the
perspective and quad exports already shared the
`transform_with_mapper_into` numeric branch.

`MODE-NUM-001CK` adds no public export either and covers numeric
resize: `ResampleCoefficients` retains the unquantized normalized
double weights alongside the 22-bit fixed-point ones, and
`resize_filter_box_into` gains a numeric two-pass branch that resamples
one 32-bit sample per pixel for `PILLOW_C_MODE_I` and
`PILLOW_C_MODE_F` with Pillow 11.3.0 `Resample.c` 32bpc semantics
(float32 intermediates for F, round-half-away after each pass for I,
no clipping) across BILINEAR/BICUBIC/LANCZOS/BOX/HAMMING.

`MODE-NUM-001CL` changes no ABI at all: the boxed numeric resize route
(`pillow_c_image_resize_box`) is served by the same
`MODE-NUM-001CK` numeric branch, and the facade `Thumbnail` aspect
math routes through `pillow_c_image_resize`; the DLL SHA-256 is
unchanged.

`MODE-NUM-001CM` adds no public export either and covers I;16 sample
semantics: the resize filter gains a uint16 two-pass branch for
`PILLOW_C_MODE_I16` replicating Pillow 11.3.0's 16bpc ROUND_UP plus
per-byte CLIP8 writes, while bilinear/bicubic transforms (affine,
perspective, quad, mesh, rotate) on `PILLOW_C_MODE_I16`/
`PILLOW_C_MODE_I16B` and filter resizes on `PILLOW_C_MODE_I16B` now
return `PILLOW_C_INVALID_ARGUMENT` as explicit documented boundaries
instead of interpolating storage bytes (Pillow itself emits
byte-channel garbage there). NEAREST paths keep whole-copying samples.

`MODE-NUM-001CN` adds no public export either and completes the
numeric resize family: `supports_reduce_mode` accepts
`PILLOW_C_MODE_I` and `PILLOW_C_MODE_F`, and `reduce_image_into` gains
the numeric block-average branch (I stores `ROUND_UP(sum / count)`, F
stores the float32 cast, partial-edge corner multipliers — Pillow
11.3.0 `Reduce.c` 32bpc semantics). The I;16/I;16B reduce step stays
rejected with `PILLOW_C_INVALID_ARGUMENT` (Pillow's `image has wrong
mode`), surfaced by a facade guard that reproduces the factor check.

`MODE-NUM-001CO` changes no ABI at all: the facade
`TransformFillBuffer` gains the I;16/I;16B uint16 packing branch
(UShort little-endian for I;16, byte-swapped big-endian for I;16B),
while the native 2-byte fill copy already worked; the DLL SHA-256 is
unchanged.

`MODE-NUM-001CP` adds no public export either and covers I;16
statistics/conversion: `extrema_image_numeric` gains the uint16 scan
branch for `PILLOW_C_MODE_I16` and returns
`PILLOW_C_INVALID_ARGUMENT` for `PILLOW_C_MODE_I16B` (Pillow's
`image has wrong mode`), and `convert_image_mode_into` gains the
I;16/I;16B source branch for I/F/L targets with Pillow 11.3.0
`Convert.c` semantics (exact I/F copies; L = 255 when the high byte is
nonzero, else the low byte). The I;16 `histogram()` storage-read
artifact remains an explicit documented boundary (the facade rejects
it).

`MODE-NUM-001CQ` adds no public export either and closes the I;16
boundary slice: the native entropy and getcolors entry points return
`PILLOW_C_INVALID_ARGUMENT` for `PILLOW_C_MODE_I16`/
`PILLOW_C_MODE_I16B` (Pillow's `image has wrong mode` for getcolors,
a documented fail-loud boundary for the layout-dependent entropy
misreads), and the facade `ImageStat.Stat` inherits the histogram
boundary through its histogram route.

`API-IMG-001F` changes no ABI at all: the facade `Image.Im` property
returns the native handle (the `ImagingCore` analogue boundary, AHK
case-insensitivity serving `im`) with Pillow's closed-image error; the
DLL SHA-256 is unchanged.

`BNDRY-001` changes no ABI at all either: the remaining-item boundary
ledger records the dependency-gated formats (failing loudly with
`Pillow image file format is unsupported`), the libimagequant
dependency error, and the codec-strategy parity non-goals (APNG/PNG
compression strategy, dither exact parity beyond the FLOYDSTEINBERG
slices, qtables beyond two tables, malformed marker streams, explicit
YCCK encoding, the META-002 tail, and the whole-file parity policy) as
explicit documented boundaries; the DLL SHA-256 is unchanged. The
completion definition is met.

## ImageMath RPN ABI Behavior

`API-MATH-001` adds one public export:
`pillow_c_image_math_rpn(images, constants, constant_floats,
slot_kinds, slot_count, program, program_size, out_image)`. Existing
exported names, signatures, status codes, image-handle ownership,
source-pointer lifetimes, and facade routes remain unchanged. The
export evaluates a per-pixel RPN stack machine over L/I/F samples with
Pillow 11.3.0 ImageMath semantics: program opcodes are `1` PUSH (followed
by a 1-based slot byte: kind 0 image, kind 1 constant with the
floatness flag), `2`-`17` binary ops (add/sub/mul/div/mod/and/or/xor/
shl/shr/eq/ne/lt/le/gt/ge), `18`-`22` unary (neg/not/abs/min/max),
`23` float, `24` int, and `25` convert (followed by a target-mode byte
1/8/9). Integral results store int32 (C truncation division, C
remainder, arithmetic shifts), floating results store float32; the
output mode is I or F accordingly. Non-L/I/F operand modes, malformed
programs, empty stacks, and float bitwise/shift operands return
`PILLOW_C_INVALID_ARGUMENT`; mixed image sizes return
`PILLOW_C_MISMATCH`. The facade `ImageMath` class compiles expressions
to this RPN (tokenizer, shunting-yard, scalar evaluator for
constant-only expressions) and surfaces Pillow's error messages.

## ImageGrab ABI Behavior

`API-GRAB-001` adds two public exports:
`pillow_c_image_grab(left, top, right, bottom, all_screens,
include_layered, out_image)` and
`pillow_c_image_grab_clipboard(out_image)`. Existing exported names,
signatures, status codes, image-handle ownership, source-pointer
lifetimes, and facade routes remain unchanged; the project now links
gdi32.lib and user32.lib. The grab export BitBlts the screen region
(CAPTUREBLT when include_layered) into a compatible bitmap, reads it
as a top-down 24bpp DIB, and stores RGB byte order; empty regions
return an empty RGB image, GDI failures return
`PILLOW_C_INVALID_ARGUMENT` (the facade maps to Pillow's
`screen grab failed`). The clipboard export decodes the CF_DIB global
with Pillow 11.3.0 semantics: 24/32bpp -> RGB (bottom-up row flip,
BGR->RGB swap, 32bpp alpha dropped), 8bpp -> L index grayscale, 1bpp
-> packed mode 1, DWORD-aligned strides; an empty/text clipboard
returns status `0` with a null handle (the facade's None analogue).

`API-PATH-001` changes no ABI at all: the facade `Pillow.ImagePath.Path`
object (constructor/tolist/getbbox/compact/transform/map) mirrors
Pillow 11.3.0's simplified path semantics; the DLL SHA-256 is
unchanged.

`API-QTTK-001` changes no ABI at all either: the facade
`Pillow.ImageQt`/`Pillow.ImageTk` stub surfaces record the
dependency-gated boundaries (no Qt binding, no Tk interpreter) with
Pillow-shaped errors; the DLL SHA-256 is unchanged.

`API-FILE-001` changes no ABI at all either: the facade
`Pillow.ImageFile` module surface records `MAXBLOCK`/`SAFEBLOCK` and
the `ERRORS` table exactly, exposes the `LOAD_TRUNCATED_IMAGES`
default (False; enabling it is a fail-loud boundary because the
native decoders decode whole files strictly, matching Pillow's
default) and the plain `PyCodecState` object, and fails loudly on the
incremental/plugin protocol (the `ImageFile` base object, `Parser`,
`StubImageFile`, `StubHandler`, `PyCodec`, `PyDecoder`, `PyEncoder`,
and the deprecated `raise_oserror` helper — exposed as
`ReportOSError(code)` because AHK identifiers beginning with "Raise"
lex as the `raise` keyword at call sites, and a parameter named
`error` would shadow the AHK `Error` class); the DLL SHA-256 is
unchanged.

`API-PALETTE-001` changes no ABI at all either: the facade
`Pillow.ImagePalette` covers the ImagePalette class (fields, lazy
`colors`, copy/getdata/tobytes/tostring/save/getcolor with the
RGBA-alpha rules, the raw-palette ValueError, the image
special-color skip, and the 256-color allocation error) and the
deterministic generators raw/negative/sepia/wedge/make_linear_lut/
make_gamma_lut exactly; `random()` shares Pillow's shape/range with
the RNG stream a documented boundary, and `load()` with the
GimpPaletteFile/GimpGradientFile/PaletteFile parser classes is a
documented fail-loud boundary; the DLL SHA-256 is unchanged.

`API-TRANSFORMCLS-001` changes no ABI at all either: the facade
`Pillow.ImageTransform` covers the base `Transform` class (data
storage, the getdata AttributeError shape, transform routing through
the existing `Image.Transform` seam) and the five method-constant
subclasses (AffineTransform/ExtentTransform/PerspectiveTransform/
QuadTransform/MeshTransform); constructing the module class fails
loudly as Pillow's module is not callable; the DLL SHA-256 is
unchanged.

`API-FONTVAR-001` changes no ABI at all either: the facade
`Pillow.ImageFont.TransposedFont` covers orientation storage, the
exact getbbox (0, 0, w, h) normalization with the 90/270 swap, the
getlength delegation and the 90/270 ValueError, and `Layout` covers
BASIC/RAQM; getmask is a documented boundary (no mask objects
exist — text rasterizes through the native draw seam) and Axis is
Pillow's type-only TypedDict; the DLL SHA-256 is unchanged.

`API-READONLY-001` changes no ABI at all either: the facade
`Image.ReadOnly` property adds Pillow 11.3.0's setter and the
`(im and im.readonly) or _readonly` OR-semantics over the existing
`pillow_c_image_readonly` export; the DetachBufferView write-detach
model stays the documented replacement for Pillow's `_ensure_mutable`
raise; the DLL SHA-256 is unchanged.

`FMT-UNREC-001` changes no ABI at all either: the previously
unrecorded format families (save BLP/BUFR/DIB/GRIB/HDF5/IM/MSP/PALM/
SPIDER/WMF and open FITS/FPX/FTEX/GBR/IMT/IPTC/MCIDAS/MIC/MPEG/PCD/
PIXAR/SPIDER/WMF/XVTHUMB) are now explicit documented codec
boundaries — the native ABI implements neither codec family, so open
and save fail loudly with the documented unsupported message; the DLL
SHA-256 is unchanged.

No facade lifetime rule, fallback, or AHK per-pixel loop was added
beyond the numeric transform family above.

## CUR Save With Hotspot ABI Behavior

`FMT-ICO-002G` adds one public export:
`pillow_c_image_save_cur_options(image, path, has_hotspot, hotspot_x,
hotspot_y)`. Existing exported names, signatures, status codes,
image-handle ownership, source-pointer lifetimes, and facade routes
remain unchanged. The writer emits the ICO type-2 container (magic
`00 00 02 00`, one entry) with a single DIB payload from the shared ICO
DIB encoder and the hotspot in the entry's planes/bit_count fields;
width/height are bounded to 256 and the hotspot to 0..65535.
`open_cur_image` now attaches `has_hotspot`/`hotspot_x`/`hotspot_y` from
the selected entry's planes/bit_count fields through the existing
`pillow_c_image_metadata_hotspot` export. Pillow 11.3.0 registers no
CUR save, so this is a documented standards extension.

No facade lifetime rule, fallback, or AHK per-pixel loop was added
beyond the CUR save family above.

## ICO Non-Exact Thumbnail Source Selection ABI Behavior

`FMT-ICO-001B` changes no exported name or signature. Existing
`pillow_c_image_save_ico_frames_format_options` status, handle
ownership, and synchronous pointer lifetimes remain unchanged. The
fallback inside `save_ico_images_with_sizes` now caps the proportional
LANCZOS fit at the last provided image's own dimensions (Pillow's
`thumbnail()` semantics: exact-size sources win, otherwise the LAST
provided image is thumbnailed, never upscaled), and skips the resize
when the capped size equals the source. Sizes larger than the base
image and larger than 256 remain skipped. Grayscale PNG payloads remain
a documented WIC reopen boundary.

No facade lifetime rule, fallback, or AHK per-pixel loop was added
beyond the ICO source-selection family above.

`FMT-ICO-001C` changes no ABI: the broader ICO multi-source matrix
(per-source modes, first same-size PNG wins, bmp duplicate bit-depths)
is a lock-in over the existing frames writer, verified against Pillow
11.3.0 ctypes. 24-bit RGB and grayscale PNG payloads remain a
documented WIC reopen boundary.

## TIFF BigTIFF Save Bilevel ABI Behavior

`FMT-TIFF-003BI` changes no exported name or signature. Existing
`pillow_c_image_save_tiff_bigtiff(_frames_metadata_ascii_entries_
options)`, `pillow_c_image_open_tiff`, and
`pillow_c_image_frame_count_tiff` status, handle ownership, and
synchronous path/source-file lifetime contracts remain unchanged.

The frames metadata writer now accepts `PILLOW_C_MODE_1` frames: an
eight-entry IFD with NO 258 tag, photometric 1, and strips packed
MSB-first per row via `tiff_pack_mode_one_pixels` (packed rows also feed
the compression branches with `tiff_uncompressed_row_stride`). The
BigTIFF strip parser recognizes the same layout (no 258, photometric 1,
samples 1, planar 1) and unpacks the packed rows into the established
0/255-per-pixel mode-1 storage convention, keeping the existing
`get_mode1_raw_bytes_image` re-pack semantics intact.

No facade lifetime rule, fallback, or AHK per-pixel loop was added
beyond the BigTIFF bilevel save family above.

## TIFF BigTIFF Save Palette ABI Behavior

`FMT-TIFF-003BH` changes no exported name or signature. Existing
`pillow_c_image_save_tiff_bigtiff(_frames_metadata_ascii_entries_
options)`, `pillow_c_image_open_tiff`, and
`pillow_c_image_frame_count_tiff` status, handle ownership, and
synchronous path/source-file lifetime contracts remain unchanged.

The frames metadata writer now accepts `PILLOW_C_MODE_P` frames:
channels 1, photometric 3, and a full 256-entry channel-major ColorMap
320 blob (SHORT[768], each palette byte written as `byte << 8`, zeros
beyond the stored `palette_rgb` triplets) placed even-aligned after the
IFD with the 320 entry (type 3, count 768, u64 offset) emitted in
ascending tag order. The BigTIFF strip parser recognizes the same layout
(photometric 3, bits 8, samples 1, planar 1) and converts the
channel-major SHORT colormap into byte RGB `palette_rgb` triplets with
`value >> 8`, mirroring the classic palette route.

No facade lifetime rule, fallback, or AHK per-pixel loop was added
beyond the BigTIFF palette save family above.

## TIFF BigTIFF Save Exif ABI Behavior

`FMT-TIFF-003BF` adds two public exports:
`pillow_c_image_patch_tiff_bigtiff_exif_entries` (the same 69-argument
family signature as the classic
`pillow_c_image_patch_tiff_exif_entries` export) and
`pillow_c_image_patch_tiff_bigtiff_exif_bytes`. Existing exported names,
signatures, status codes, image-handle ownership, source-pointer
lifetimes, and facade routes remain unchanged; the classic patch's
family classifier and bytes-form blob parser are extracted into the
shared private `build_tiff_patch_exif_entries` (inline limit 4/8) and
`parse_tiff_patch_exif_blob_families` seams, and the classic routes
delegate to them with no behavior change.

The BigTIFF patch rewrites a single-frame BigTIFF save (`II 2B 00`, IFD0
at 16, next-IFD 0) with the patched families merged in ascending tag
order: 20-byte entries with 64-bit counts and eight-byte value fields,
inline values <= 8 bytes, the inline `273` strip offset and out-of-line
64-bit offsets shifted by the IFD-growth delta, and new blobs appended
after the grown IFD before the old region. Collisions keep the base
entry. All pointer arguments are consumed synchronously for the call.

No facade lifetime rule, fallback, or AHK per-pixel loop was added
beyond the BigTIFF exif save family above.

## TIFF BigTIFF Save Metadata ABI Behavior

`FMT-TIFF-003BE` adds one public export:
`pillow_c_image_save_tiff_bigtiff_frames_metadata_ascii_entries_options`
with the same parameter shape as the classic
`pillow_c_image_save_tiff_frames_metadata_ascii_entries_options` export
(images, count, path, has_dpi, dpi_x, dpi_y, compression, icc bytes,
xmp bytes, ascii tag/pointer/size arrays). Existing exported names,
signatures, status codes, image-handle ownership, source-pointer
lifetimes, and facade routes remain unchanged; the plain frames writer
now delegates to the generalized internal metadata writer.

The writer emits Pillow 11.3.0's BigTIFF metadata layout: ascending
20-byte entries, inline ASCII <= 8 bytes (NUL-terminated values are
zero-padded in the value field), inline RATIONAL 282/283 as
numerator/denominator-1 pairs, SHORT 296 unit 2, XMP tag 700 as type-1
BYTE and ICC tag 34675 as type-7 UNDEFINED (inline <= 8, else
even-aligned LONG8-offset blobs appended after each IFD and before the
strip), with dpi/icc/xmp/ascii metadata written to every frame's IFD.
All pointer arguments are consumed synchronously for the call. Numeric
modes combined with compression still return
`PILLOW_C_INVALID_ARGUMENT`.

No facade lifetime rule, fallback, or AHK per-pixel loop was added
beyond the BigTIFF metadata save family above.

## TIFF Numeric BigTIFF Strip Save/Open ABI Behavior

`FMT-TIFF-003BD` changes no exported name or signature. Existing
`pillow_c_image_open_tiff`, `pillow_c_image_open_tiff_frame`,
`pillow_c_image_frame_count_tiff`, and
`pillow_c_image_save_tiff_bigtiff(_frames_compression_options)` status,
handle ownership, and synchronous path/source-file lifetime contracts
remain unchanged.

`save_tiff_bigtiff_frames_image_with_compression` now accepts I16, I16B,
I, F, and CMYK frames. I16/I/F write a count-1 `258` BitsPerSample (16 or
32) with no `277` SamplesPerPixel; I and F add `339` SampleFormat 2 or 3;
CMYK writes count-4 8-bit bits with photometric 5; I16B pixels are swapped
to little-endian strips through `tiff_i16b_to_i16_pixels`. Numeric modes
combined with any compression return `PILLOW_C_INVALID_ARGUMENT` because
Pillow's `big_tiff`+compression falls back to classic TIFF upstream.

`parse_tiff_bigtiff_strip_image_for_ifd` recognizes the same layouts and
returns `PILLOW_C_MODE_I16`/`I16B`/`I`/`F`/`CMYK`. Big-endian 32-bit I/F
strips are byte-swapped once into little-endian DLL storage; big-endian
16-bit strips keep raw bytes under `PILLOW_C_MODE_I16B`. A 32-bit strip
without the `339` tag is not recognized and open returns
`PILLOW_C_INVALID_ARGUMENT`. The facade mirrors Pillow by routing numeric
`big_tiff`+compression saves through the classic TIFF writer.

No export, facade lifetime rule, fallback, or AHK per-pixel loop was
added beyond the numeric save/open family above.

## TIFF Big-Endian BigTIFF ABI Behavior

`FMT-TIFF-003AF` changes no exported name or signature. Existing
`pillow_c_image_open_tiff`, `pillow_c_image_open_tiff_frame`, and
`pillow_c_image_frame_count_tiff` status, handle ownership, and synchronous
path/source-file lifetime contracts remain unchanged.

The private `parse_tiff_bigtiff_header` seam validates `II 2B 00` or
`MM 00 2B`, the required eight-byte offset size, the zero reserved field, and
the first LONG8 IFD offset. That byte order is consumed by all BigTIFF scalar,
SHORT-array, LONG8-array, IFD-location, frame-selection, and frame-count reads.
The two-frame matrix proves that a big-endian LONG8 next-IFD pointer reaches
frame 1 and terminates at zero.

For MM numeric tiled images, unsigned 16-bit grayscale returns
`PILLOW_C_MODE_I16B` with unchanged file bytes. Signed `PILLOW_C_MODE_I` and
float `PILLOW_C_MODE_F` samples are normalized into little-endian DLL storage
after native tile decoding. Eight-bit chunky/planar samples, RGBX, LA, and
CMYK are not swapped. No new pointer, facade lifetime rule, fallback, or AHK
per-pixel loop was added. Pillow 11.3.0 itself rejects valid MM BigTIFF due to
its one-byte `ifh[2] == 43` check; the ABI behavior is therefore documented as
a standards extension.

## TIFF BigTIFF Per-Frame Metadata ABI Behavior

`FMT-TIFF-003AG`/`FMT-TIFF-003AH`/`FMT-TIFF-003AI`/`FMT-TIFF-003AJ`/
`FMT-TIFF-003AK`/`FMT-TIFF-003AL`/`FMT-TIFF-003AM`/`FMT-TIFF-003AN`/
`FMT-TIFF-003AO`/`FMT-TIFF-003AP`/`FMT-TIFF-003AQ`/`FMT-TIFF-003AR`/
`FMT-TIFF-003AS`/`FMT-TIFF-003AU` change no exported name or signature.
Existing `pillow_c_image_open_tiff`, `pillow_c_image_open_tiff_frame`,
`pillow_c_image_frame_count_tiff`, `pillow_c_image_metadata_tiff_exif`,
`pillow_c_image_metadata_tiff_icc_profile`, and
`pillow_c_image_metadata_xmp` status, ownership, and synchronous
source-file lifetime contracts remain unchanged.

For a Pillow-valid little-endian BigTIFF tiled image, opening any frame now
attaches that frame's own IFD metadata through the private
`attach_tiff_bigtiff_metadata_for_ifd` seam: tag `34675` bytes fill
`tiff_icc_profile`, tag `700` bytes fill `xmp`, and the bounded
`build_tiff_bigtiff_common_ascii_exif_for_ifd` fills `tiff_exif` with the two
undefined tags plus the scalar orientation tag through the shared EXIF
serializer. `FMT-TIFF-003AI` extends the same seam with
`parse_tiff_bigtiff_resolution_for_ifd`, which reads RATIONAL `282`/`283`
(count 1, inline eight-byte value field) and SHORT `296` with the decoded
byte order, rejects zero denominators, and requires the explicit unit-2 trio
before filling `has_dpi`, `dpi_x`, and `dpi_y`; the existing
`pillow_c_image_metadata_resolution` export then surfaces facade
`Info["dpi"]` unchanged. `FMT-TIFF-003AJ` adds
`read_tiff_bigtiff_ascii_entry_value` for type-2 entries (64-bit counts,
inline value field, LONG8 offsets, NUL stripping) and serializes the
`tiff_common_ascii_tag` set into `tiff_exif` alongside orientation and the
undefined tags, so the facade's existing ascii-tag parsing surfaces
`exif[270]`/`exif[305]`/`exif[315]` with no facade change.
`FMT-TIFF-003AK` adds `read_tiff_bigtiff_uint_scalar_entry_value` for count-1
SHORT/LONG/LONG8 scalars, normalizing uint32-fitting LONG8 into the
serializer's LONG shape, and serializes the `tiff_common_uint_tag` set, so
the plain tiled fixture always yields a `HasExif` blob whose base uint tags
surface as facade integers. `FMT-TIFF-003AL` adds
`read_tiff_bigtiff_uint_array_entry_value` for type-4/type-16 arrays with
64-bit counts, inline and LONG8-offset layouts, and uint32-fitting LONG8
validation, and serializes LONG8 `324`/`325`, type-4 `273`/`279`, and the
bounded 50719/50720/50829/50830/50937/50981/51089/51090/51091/52536
families with the classic expected-count rules, so the plain tiled fixture's
`exif[324]`/`exif[325]` arrays surface as facade integer arrays.
`FMT-TIFF-003AM` adds `read_tiff_bigtiff_ushort_array_entry_value` for
type-3 arrays with 64-bit counts, inline and LONG8-offset layouts, and
serializes the classic SHORT-array tag set (291/297/301/320/321/336/342/530/
34735/37396/41492/42081/50712/50713, type-3 50719/50720/50829, and
multi-channel `258`) with the classic size rules, so facade `GetExif()`
exposes `exif[530]`/`exif[34735]`/`exif[258]` arrays unchanged.
`FMT-TIFF-003AN` adds `read_tiff_bigtiff_rational_entry_value` (count-1
type-5, inline eight-byte value field, zero-denominator rejection) and
`read_tiff_bigtiff_rational_array_entry_value` (type-5 arrays, 64-bit counts,
inline and LONG8-offset layouts), serializing the classic scalar rational tag
set (286/287/33434/33437/37122/37378/37381/37382/37386/41483/41486/41487/
41493/41988/42240/50731/50732/50734/50737/50738/50780/50935/51058/51112/
51178/51179), the rational-array set (318/319/529/532/42034/42082/50714/
50718/type-5 50719/50720/50727/50728/50729/50736/type-5 51091/51125) with the
classic expected-size rules, and the unit-2 `282`/`283`/`296` resolution trio
into `tiff_exif`, so `exif[282]`/`exif[283]`/`exif[296]` surface alongside the
DPI seam. `FMT-TIFF-003AO` adds
`read_tiff_bigtiff_signed_rational_entry_value` and
`read_tiff_bigtiff_signed_rational_array_entry_value` for the type-10 scalar
set (37377/37379/37380/50716/50730/50739/51044/51109) and array set (50715/
50721-50726/50832/50834/50964/50965/52530-52532) with two's-complement 32-bit
halves. `FMT-TIFF-003AP` adds `read_tiff_bigtiff_double_array_entry_value` and
`read_tiff_bigtiff_float_array_entry_value` for type-12 arrays (33550/33922/
34264/34736/50844/51041) and type-11 arrays (50938/50939/50940/50982) with
inline and LONG8-offset layouts. `FMT-TIFF-003AQ` serializes the
`tiff_common_byte_array_tag` set (34377/40092-40095/50706/50707/50709/50710/
50781/50831/50833/50972/50973/51043/51111) and the extended undefined list
(type-7 37510/37724 plus 347/33723/34856/36864/37121/37500/40960/41484/41728/
41729/41730/41995/50828/50969/51008/51009/51022/52525/52533-52535) through
the existing blob reader with the classic size rules. Rationals stay
`[num, den]` pairs in the DLL/facade contract (Pillow 11.3.0 exposes floats —
recorded divergence), doubles/floats surface as numeric arrays, and
byte/undefined blobs as byte buffers, all through the existing EXIF exports
with no facade change. `FMT-TIFF-003AR` proves the same metadata surface
from an `MM` BigTIFF header: `parse_tiff_bigtiff_header` already decodes the
byte order and every BigTIFF metadata reader consumes it, so no native
change was required (Pillow 11.3.0 itself rejects valid MM BigTIFF, so this
is a standards extension). `FMT-TIFF-003AS` locks in the malformed-metadata
contract with no native change: truncated out-of-line blobs, count-overflow
rationals, invalid-type entries, zero-count blobs, and zero denominators all
open with exact pixels while the malformed tag is skipped, because every
BigTIFF metadata reader already validates type, count, offset, and
denominator before accepting an entry. Pillow 11.3.0 reinterprets
invalid-type tags by tag semantics and exposes zero denominators as `nan`;
the DLL keeps the classic strict-type convention and skips those entries —
documented divergence. `FMT-TIFF-003AU` adds the same one-level flattening
on the BigTIFF route: the BigTIFF entry-collection loop moves into the
private `collect_tiff_bigtiff_exif_entries` seam reusing the shared
`TiffExifCollector` struct (moved above both builders), the builder captures
type-4 `34665`/`34853` offsets (LONG8 scalars normalize to type 4 through
the existing reader), bounds-checks each sub-IFD with a 64-bit count read,
a 20-byte-entry span check, and a 4096-entry cap, and re-collects one
sub-IFD level with the bounded GPS tag sets; per-frame attachment makes the
flattening per-frame automatically. The blob reader `read_tiff_bigtiff_blob_entry_value`
accepts BYTE/type-1 and UNDEFINED/type-7 entries with 64-bit counts; counts
of eight or fewer bytes are inline in the eight-byte value field, and larger
blobs are located through the LONG8 offset, matching the local Pillow 11.3.0
oracle. Each returned frame handle carries only its own IFD's metadata;
Pillow's stale `info["icc_profile"]` when a later IFD lacks tag 34675 and
Pillow's no-unit/cm-unit/absent-tag DPI defaults are upstream behaviors this
ABI does not reproduce. The facade XMP-refresh deletion covers TIFF as well
as PNG/JPEG, so seeking to a frame without XMP removes the previous frame's
`Info["xmp"]` like Pillow. No new pointer, facade lifetime rule, fallback, or
AHK per-pixel loop was added.

## TIFF BigTIFF Save and Strip Open ABI Behavior

`FMT-TIFF-003AZ` adds one public export,
`pillow_c_image_save_tiff_bigtiff`, raising source/DLL export parity from
`455/455` to `456/456` with zero difference. It writes Pillow 11.3.0's
exact `big_tiff=True` strip layout (`II 2B 00`, offset size 8, IFD0 at 16,
64-bit counts, 20-byte entries, LONG 273/279, inline SHORT bits) for the
uncompressed single-frame L/RGB/RGBA/LA matrix and rejects other modes.
`FMT-TIFF-003BA` adds `pillow_c_image_save_tiff_bigtiff_compression_options`
(export parity `457/457`) reusing the existing PackBits/LZW/Adobe-Deflate
encoders, and extends the strip open route to decode those compression
tags through the shared decoder seam; Pillow 11.3.0's
`big_tiff`+compression falls back to classic TIFF upstream, so the save
side is a standards extension. `FMT-TIFF-003BB` adds
`pillow_c_image_save_tiff_bigtiff_frames_compression_options` (export
parity `458/458`) writing standard chained-IFD multi-frame BigTIFF with
same-mode frames, per-frame strip offsets, and u64 next pointers;
Pillow's own save_all output is also chain-linked (each page's inline
header is a writer artifact), which `FMT-TIFF-003BC` locks in with
oracle-layout fixtures and no native change. The private
`parse_tiff_bigtiff_strip_image_for_ifd` open route accepts
the same layout, and the open/frame-count dispatchers fall back to it
when the tiled parser rejects the shape, so Pillow-written strip BigTIFFs
reopen natively. Existing open/save exports, handle ownership, and
synchronous path lifetime contracts remain unchanged; the facade routes
`big_tiff: true` through the new exports with single-frame and
option-composition guards.

## TIFF Save exif= ABI Behavior

`FMT-TIFF-003AV` adds one public export,
`pillow_c_image_patch_tiff_exif_entries`, raising source/DLL export parity
from `453/453` to `454/454` with zero difference. It takes a saved classic
single-frame TIFF path plus the caller's EXIF tag families (ascii values,
uint scalars with type normalization, rational pairs, signed rational
pairs, short arrays, byte arrays, and undefined blobs) and post-patches
IFD0: entries are merged in ascending tag order, the inline 273/strip
offset and every out-of-line offset are shifted by the IFD0 growth delta,
and the new blobs are appended before the old blob region. IFD0-tag
collisions keep the base entry; multi-frame files return
`PILLOW_C_INVALID_ARGUMENT`; zero denominators and malformed layouts are
rejected. `FMT-TIFF-003AW` extends the same export with type-5 count-N
rational arrays and type-4 count-N LONG arrays (inline/out-of-line
layouts, zero-denominator rejection, bounds-checked offsets) without
changing its name or the existing parameter layout beyond appending the
new groups. The facade `Image.Save` routes `exif=<Image.Exif>` through the
existing save seams and then this export, matching Pillow 11.3.0's
direct-into-IFD0 `exif=` layout and the unit-2 282/283/296 reopened
`Info["dpi"]` behavior; scalar exif composes with every TIFF compression
like Pillow, and array exif plus compression keeps working where libtiff
fails upstream (benign superset). `FMT-TIFF-003AX` changes no native code:
the facade mirrors Pillow 11.3.0's `tiffinfo` precedence by dropping
`exif` whenever `tiffinfo` is set, and the patch export's collision rule
keeps a dpi-saved base's 282/283/296 trio when patched ascii tags are
added. `FMT-TIFF-003AY` adds one more public export,
`pillow_c_image_patch_tiff_exif_bytes`, raising source/DLL export parity
from `454/454` to `455/455` with zero difference. It parses a
caller-supplied EXIF blob (`Exif\0\0`-prefixed or bare MM TIFF) into the
same bounded tag families — ascii with NUL stripping, uint scalars,
rational/signed-rational scalars, rational/short/LONG arrays, byte and
undefined blobs, with a 4096-entry cap and a 0xFFFFFF-count cap — and
patches IFD0 through the same vector path; double/float/LONG8 blob
entries are skipped. The facade `Image.Save` accepts a Buffer for
`exif=` and routes it through the bytes export, matching Pillow 11.3.0's
bytes-form IFD0 parsing. Existing save exports, handle ownership, and
synchronous path lifetime contracts remain unchanged.

## TIFF Classic ExifIFD/GPSInfo Sub-IFD ABI Behavior

`FMT-TIFF-003AT` changes no exported name or signature. Existing
`pillow_c_image_open_tiff`, `pillow_c_image_open_tiff_frame`, and
`pillow_c_image_metadata_tiff_exif` status, ownership, and synchronous
source-file lifetime contracts remain unchanged.

For a classic TIFF whose IFD0 carries type-4 ExifIFD (`34665`) or GPSInfo
(`34853`) pointers, the native EXIF builder now follows each pointer to its
sub-IFD, bounds-checks the count and 12-byte-entry span, and flattens one
sub-IFD level into the serialized `tiff_exif` blob through the existing
EXIF exports. The entry-collection loop moved into the private
`TiffExifCollector` struct plus `collect_tiff_exif_entries` seam (a
mechanical extraction, no behavior change), and new bounded GPS tag sets
(ascii 1/3/9/10/12/14/16/18/19/23/25/27/28, uint 5/7/11/29/30/31, rational
6/13/15/17/21/24/26, count-3 rational arrays 2/4/20/22) join the
classification chain. Facade `GetExif()` exposes the flattened tags
alongside the pointer values; Pillow 11.3.0 keeps sub-IFD tags only in
`Exif.get_ifd(...)`, so the flat-map container is a documented divergence
and `get_ifd()` itself is an explicit boundary. No new pointer, facade
lifetime rule, fallback, or AHK per-pixel loop was added.

## TIFF BigTIFF Wide-Mode ABI Behavior

`FMT-TIFF-003AE` changes no exported name or signature. Existing TIFF open,
frame-open, and frame-count status, ownership, and synchronous source-file
lifetime contracts remain unchanged. The little-endian BigTIFF tiled parser
now recognizes `PILLOW_C_MODE_I16`, `PILLOW_C_MODE_I`, `PILLOW_C_MODE_F`, and
`PILLOW_C_MODE_CMYK` through the existing handles.

Tag 339 selects signed integer or float32 storage for 32-bit grayscale;
unsigned 16-bit grayscale maps to two-byte `I;16`, and Photometric 5 with four
8-bit samples maps to CMYK. Raw, PackBits, TIFF LZW, and Adobe Deflate all
retain exact storage bytes in the DLL. No new pointer, facade lifetime rule,
fallback, or AHK per-pixel loop was added.

## TIFF BigTIFF Compressed Planar Storage ABI Behavior

`FMT-TIFF-003AD` changes no exported name or signature. The existing TIFF open,
frame-open, and frame-count routes retain their status codes, handle ownership,
and synchronous source-file lifetime. The internal BigTIFF parser is renamed to
`parse_tiff_bigtiff_tiled_image_for_ifd` to reflect its chunky and planar
ownership; this name is not exported.

For `PlanarConfiguration=2`, PackBits, TIFF LZW, and Adobe Deflate now cover
`L`, `RGB`, `RGBA`, `RGBX`, and `LA`. Every plane tile is range-checked and
decoded in the DLL. RGBX returns three-channel `PILLOW_C_MODE_RGB` and skips
the fourth X plane; compressed LA returns `PILLOW_C_MODE_LA` while preserving
Pillow 11.3.0/libtiff's observed zero alpha plane. Raw planar RGBX/LA remain
rejected because Pillow itself raises `unknown raw mode` for those shapes. No
new pointer, facade lifetime rule, fallback, or AHK per-pixel loop was added.

## TIFF BigTIFF Planar-Separate ABI Behavior

`FMT-TIFF-003AC` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)`,
`pillow_c_image_open_tiff_frame(const char*, int, PillowCImage**)`, and
`pillow_c_image_frame_count_tiff(const char*, int*)` routes retain their
status codes, handle ownership, and synchronous source-file lifetime.

For bounded little-endian BigTIFF tiled `L`, `RGB`, and `RGBA`, the parser now
accepts `PlanarConfiguration=2`, validates plane-major LONG8 tile arrays, and
derives one tile plane per sample. Each tile uses a one-channel native stride;
the DLL clips edge tiles and writes samples into interleaved image storage.
The Pillow 11.3.0 oracle rejects the probed planar `LA` shape during load, so
this ABI behavior does not claim `LA`. No new pointer, facade lifetime rule,
fallback, or AHK per-pixel loop was introduced.

## TIFF BigTIFF RGBX ABI Behavior

`FMT-TIFF-003AB` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)`,
`pillow_c_image_open_tiff_frame(const char*, int, PillowCImage**)`, and
`pillow_c_image_frame_count_tiff(const char*, int*)` routes retain their
status codes, handle ownership, and synchronous source-file lifetime. The
little-endian BigTIFF parser now recognizes the Pillow-compatible chunky
tiled RGBX shape with `Photometric=2`, `BitsPerSample=[8,8,8,8]`,
`SamplesPerPixel=4`, explicit `ExtraSamples=0`, and
`PlanarConfiguration=1`.

Tile payload validation uses four input bytes per pixel, while the returned
image owns three-channel `PILLOW_C_MODE_RGB` storage. Each native tile-row
copy skips the fourth X byte and clips right/bottom edge tiles. Missing tag
338 is not treated as RGBX: Pillow 11.3.0 maps that distinct four-channel
shape to `RGBA/RGBA`, so unsupported/malformed shapes remain explicitly
rejected or routed by their own mode rule. No new pointer, facade lifetime
rule, fallback, or AHK per-pixel loop was introduced.

## TIFF BigTIFF Orientation ABI Behavior

`FMT-TIFF-003AA` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)`,
`pillow_c_image_open_tiff_frame(const char*, int, PillowCImage**)`, and
`pillow_c_image_frame_count_tiff(const char*, int*)` routes retain their
status codes, handle ownership, and synchronous source-file lifetime. The
little-endian BigTIFF parser now reads scalar tag `274` (SHORT/LONG/LONG8)
and applies Orientation 2–8 through the existing native transform helper after
tile decoding. The DLL owns transformed allocation, dimension swaps, and all
pixel copies; the facade only observes the resulting mode, size, and bytes.
No new export, pointer lifetime, or AHK per-pixel loop was introduced.

The covered fixture remains a 4×3, 2×2 row-major chunky tiled BigTIFF across
public `L`, `RGB`, `RGBA`, and `LA`. Orientations 2–4 retain dimensions and
5–8 swap them. Values above 8 return `PILLOW_C_INVALID_ARGUMENT` explicitly;
planar BigTIFF and broader modes remain separate gap packets. Chunky RGBX
storage is covered by `FMT-TIFF-003AB`.

## TIFF Tiled Mode-L ABI Behavior

`FMT-TIFF-003L` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)` route now natively
recognizes the bounded uncompressed tiled mode-`L` shape with 2×2 tiles and
row-major clipped edge reconstruction. The existing
`pillow_c_image_frame_count_tiff(const char*, int*)` route recognizes the same
single-frame tiled shape and returns `1`, allowing the synchronous facade
`Image.Open` initialization to complete even when WIC cannot count the minimal
fixture. Ordinary strip TIFFs, strip orientation transforms, and multi-frame
TIFF frame counts retain their previous native/WIC dispatch. The parser owns
all tile allocation, range validation, and pixel copies in the DLL; no pointer
outlives the call, no handle ownership rule changes, and no AHK pixel loop is
introduced. Compressed tiles, planar-separate tiles, other tiled modes,
multi-frame tiled files, and BigTIFF remain outside this ABI slice.

## TIFF Tiled Mode-RGB ABI Behavior

`FMT-TIFF-003M` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)` route now shares the
native uncompressed chunky tile parser with mode `L` and recognizes the
bounded 8-bit RGB shape: `Photometric=2`, `BitsPerSample=[8,8,8]`,
`SamplesPerPixel=3`, `PlanarConfiguration=1`, 2×2 row-major tiles, and clipped
right/bottom edges. The existing
`pillow_c_image_frame_count_tiff(const char*, int*)` route recognizes the same
single-frame tiled shape and returns `1`, allowing synchronous facade
initialization without a WIC frame-count dependency.

The DLL owns RGB allocation, tile range validation, row-stride calculation,
and all pixel copies. No pointer outlives the call, no handle ownership rule
changes, and no AHK per-pixel loop is introduced. RGBA tiled storage,
compressed tiles, planar-separate tiles, multi-frame tiled files, tiled
orientation transforms, and BigTIFF remain outside this ABI slice.

## TIFF Tiled Mode-RGBA ABI Behavior

`FMT-TIFF-003N` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)` route shares the
native uncompressed chunky tile parser with modes `L` and `RGB` and recognizes
the bounded 8-bit RGBA shape: `Photometric=2`,
`BitsPerSample=[8,8,8,8]`, `SamplesPerPixel=4`, `ExtraSamples=2`,
`PlanarConfiguration=1`, 2×2 row-major tiles, and clipped right/bottom edges.
The existing `pillow_c_image_frame_count_tiff(const char*, int*)` route
recognizes the same single-frame tiled shape and returns `1`.

The DLL owns RGBA allocation, tile range validation, four-byte row-stride
calculation, and all pixel copies. No pointer outlives the call, no handle
ownership rule changes, and no AHK per-pixel loop is introduced. Compressed
tiles, planar-separate tiles, multi-frame tiled files, tiled orientation
transforms, and BigTIFF remain outside this ABI slice.

## TIFF Tiled Mode-LA ABI Behavior

`FMT-TIFF-003O` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)` route shares the
native uncompressed chunky tile parser with modes `L`, `RGB`, and `RGBA` and
recognizes the bounded 8-bit LA shape: `Photometric=1`,
`BitsPerSample=[8,8]` stored inline, `SamplesPerPixel=2`,
`ExtraSamples=2`, `PlanarConfiguration=1`, 2×2 row-major tiles, and clipped
right/bottom edges. The existing
`pillow_c_image_frame_count_tiff(const char*, int*)` route recognizes the same
single-frame tiled shape and returns `1`.

The DLL owns LA allocation, tile range validation, two-byte row-stride
calculation, and all pixel copies. The input tile descriptors and source file
bytes are borrowed only during the synchronous open call; the returned image
owns its copied storage and is released through the existing image-free ABI.
No pointer outlives the call, no handle ownership rule changes, and no AHK
per-pixel loop is introduced. Compressed tiles, planar-separate storage,
multi-frame tiled files, tiled orientation transforms, and BigTIFF remain
outside this ABI slice.

## TIFF Tiled RGBX Storage ABI Behavior

`FMT-TIFF-003P` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)` route shares the
native uncompressed chunky tile parser with modes `L`, `RGB`, `RGBA`, and
`LA`, and recognizes the bounded 8-bit RGBX-storage shape:
`Photometric=2`, `BitsPerSample=[8,8,8,8]`, `SamplesPerPixel=4`,
`ExtraSamples=0`, `PlanarConfiguration=1`, 2×2 row-major tiles, and clipped
right/bottom edges. Pillow exposes the decoded image as public mode `RGB`
with rawmode `RGBX`; the native destination is three-channel RGB and the
fourth X byte in each tile pixel is discarded during the DLL-owned copy.

The existing `pillow_c_image_frame_count_tiff(const char*, int*)` route
recognizes the same single-frame tiled shape and returns `1`, allowing the
synchronous facade to initialize `n_frames` without WIC frame-count
dependency. Tile allocation, four-byte input-stride traversal, range checks,
clipping, and output lifetime remain inside the DLL. The input file bytes are
borrowed only during the synchronous open call; the returned image is released
through the existing image-free ABI. No pointer outlives the call, no handle
ownership or facade lifetime rule changes, no export is added, and no AHK
per-pixel loop is introduced. Compressed tiles, planar-separate storage,
multi-frame tiled files, tiled orientation transforms, and BigTIFF remain
outside this ABI slice.

## TIFF PackBits Tiled Matrix ABI Behavior

`FMT-TIFF-003Q` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)` and
`pillow_c_image_frame_count_tiff(const char*, int*)` routes now recognize
Compression `32773` (`PackBits`) for the bounded 4×3, 2×2 row-major chunky
tiled matrix: `L`, `RGB`, `RGBA`, `LA`, and RGBX storage exposed as public
`RGB` with rawmode `RGBX`. Each LONG tile offset/count pair is range-checked;
the compressed tile source is borrowed only during the synchronous call,
decoded to its exact full-tile byte size in a DLL-owned reusable buffer, and
then copied through the existing clipped edge path. RGBX continues to omit the
fourth X byte from each public RGB pixel.

The returned image remains DLL-owned and is released through the existing
image-free ABI. No pointer outlives the call, no status code or handle
ownership rule changes, and no facade buffer retention or AHK per-pixel loop
is introduced. Invalid PackBits tiles return `PILLOW_C_INVALID_ARGUMENT`
explicitly. LZW/Adobe Deflate tiled compression is covered by the following
same-ABI extension; planar-separate storage, multi-frame tiled files, tiled
orientation transforms, and BigTIFF remain outside this ABI slice.

## TIFF LZW/Adobe Deflate Tiled Matrix ABI Behavior

`FMT-TIFF-003R` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)` and
`pillow_c_image_frame_count_tiff(const char*, int*)` routes now recognize
Compression `5` (TIFF LZW) and `8` (Adobe Deflate) for the bounded 4×3, 2×2
row-major chunky tiled matrix: `L`, `RGB`, `RGBA`, `LA`, and RGBX storage
exposed as public `RGB` with rawmode `RGBX`. Each LONG tile offset/count pair
is range-checked; the compressed tile source is borrowed only during the
synchronous call; and each complete tile is decoded to its exact full-tile
byte size in a DLL-owned reusable buffer before the existing clipped row-copy
path. RGBX continues to omit the fourth X byte from each public RGB pixel.

The returned image remains DLL-owned and is released through the existing
image-free ABI. No pointer outlives the call, no status code or handle
ownership rule changes, and no facade buffer retention or AHK per-pixel loop
is introduced. Invalid LZW or Deflate tiles return
`PILLOW_C_INVALID_ARGUMENT` explicitly. No export, ABI signature, or facade
lifetime rule changed; planar-separate storage, multi-frame tiled files,
tiled orientation transforms, and BigTIFF remain outside this ABI slice.

## TIFF Planar-Separate Tiled Matrix ABI Behavior

`FMT-TIFF-003S` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)` and
`pillow_c_image_frame_count_tiff(const char*, int*)` routes now recognize the
bounded single-frame, uncompressed, 4×3, 2×2 tiled matrix with
`PlanarConfiguration=2` for public `L`, `RGB`, `RGBA`, and `LA` storage. The
LONG `TileOffsets` and `TileByteCounts` arrays are validated as
`tile_columns * tile_rows * samples_per_pixel` entries and are interpreted in
plane-major order: all tiles for plane 0, then all tiles for plane 1, and so
on.

The input file and tile descriptors are borrowed only during the synchronous
open call. The DLL allocates the public image and copies each valid edge
sample into interleaved DLL-owned storage while decoding the single-channel
plane tiles. The returned image is released through the existing image-free
ABI; no pointer outlives the call, no status code or handle ownership rule
changes, and no AHK per-pixel loop is introduced. RGBX planar storage,
compressed planar tiles, multi-frame tiled files, tiled orientation transforms,
and BigTIFF remain separate boundaries.

## TIFF Compressed Planar-Separate Tiled Matrix ABI Behavior

`FMT-TIFF-003T` changes no exported name or signature. The same existing
TIFF open and frame-count exports accept PackBits (`32773`), TIFF LZW (`5`),
and Adobe Deflate (`8`) on the bounded planar-separate 4×3, 2×2 tiled matrix
for public `L`, `RGB`, `RGBA`, and `LA` storage. Each plane tile is
range-checked and decoded through the existing native PackBits/LZW/zlib seam
before the DLL interleaves clipped samples into the returned image.

Compressed source bytes are borrowed only during synchronous open. The
returned image remains DLL-owned and is released through the existing
image-free ABI. No pointer outlives the call, no status code, export,
signature, facade lifetime rule, or handle ownership rule changes, and no AHK
per-pixel loop is introduced. RGBX planar storage, multi-frame tiled files,
tiled orientation transforms, and BigTIFF remain separate boundaries.

## TIFF Two-Frame Chunky Tiled ABI Behavior

`FMT-TIFF-003U` changes no exported name or signature. The existing
`pillow_c_image_open_tiff_frame(const char*, int, PillowCImage**)` route now
uses the native per-IFD chunky parser for supported tiled frame indices beyond
zero, and the existing
`pillow_c_image_frame_count_tiff(const char*, int*)` route walks the complete
next-IFD chain for the recognized tiled route. Repeated IFD offsets are
explicitly rejected with `PILLOW_C_INVALID_ARGUMENT`; malformed chains do not
silently degrade to a one-frame result or a WIC path.

For each selected frame, the file bytes and IFD descriptors are borrowed only
during synchronous open. Pixel allocation, tile range validation, edge
clipping, and per-IFD EXIF/ICC/XMP/resolution extraction remain in the DLL.
Every successful frame open returns a new DLL-owned image handle released
through the existing image-free ABI. No facade buffer retention, new pointer
lifetime, export, signature, or AHK per-pixel loop is introduced. This packet
covers two-frame uncompressed chunky tiled `L`, `RGB`, `RGBA`, and `LA`;
three-frame tiled files, compressed multi-frame tiled files, tiled orientation,
RGBX planar storage, and BigTIFF remain separate boundaries.

## TIFF Three-Frame Chunky Tiled ABI Behavior

`FMT-TIFF-003V` changes no exported name or signature. It proves that the
same `pillow_c_image_open_tiff_frame` and
`pillow_c_image_frame_count_tiff` routes handle a third chained IFD for the
bounded uncompressed chunky tiled 4×3, 2×2 matrix covering `L`, `RGB`, `RGBA`,
and `LA`. Frame count walks all three validated IFD links; frame index `2`
returns a new image handle populated by the existing per-IFD native parser.

The file bytes, IFD descriptors, and tile payloads are borrowed only during
the synchronous open call. Allocation, edge clipping, and pixel copies remain
inside the DLL, and the returned handle is released through the existing
image-free ABI. No export, signature, status code, pointer lifetime, facade
buffer retention rule, or AHK per-pixel loop changes. Tiled orientation,
compressed multi-frame tiled files, RGBX planar storage, and BigTIFF remain
separate boundaries.

## TIFF Chunky Tiled Orientation ABI Behavior

`FMT-TIFF-003W` changes no exported name or signature. The existing
`pillow_c_image_open_tiff(const char*, PillowCImage**)` and
`pillow_c_image_frame_count_tiff(const char*, int*)` routes now accept
Orientation `2` through `8` on the bounded uncompressed chunky tiled 4×3,
2×2 matrix for `L`, `RGB`, `RGBA`, and `LA` storage. After the DLL reconstructs
the clipped tile grid, the shared `apply_tiff_orientation_transform` helper
performs the existing mirror, transpose, transverse, and 90/180/270-degree
pixel transforms in native-owned buffers.

The returned image remains DLL-owned and is released through the existing
image-free ABI. The source bytes and tile descriptors are borrowed only during
synchronous open; transformed allocation, channel copies, and dimension
changes remain inside the DLL. No pointer lifetime, status code, handle
ownership, facade retention rule, export, or AHK per-pixel loop changes.
BigTIFF, compressed multi-frame tiled files, and RGBX planar storage remain
separate boundaries.

## Additive Quantize Options ABI

The current Release x64 DLL exports:

```cpp
extern "C" __declspec(dllexport) int pillow_c_image_quantize_options(
    const PillowCImage* source,
    int colors,
    int method,
    int kmeans,
    int dither,
    PillowCImage** out_image);
```

`method` uses Pillow's public constants `MEDIANCUT=0`, `MAXCOVERAGE=1`,
`FASTOCTREE=2`, and `LIBIMAGEQUANT=3`. The native route owns all image
collection, palette generation, palette assignment, RGBA palette-alpha
materialization, and bounded k-means refinement. `dither` is retained in the
ABI for Pillow-compatible argument routing; Pillow 11.3.0's algorithm core
does not pass it into `self.im.quantize`, so it does not alter this native
method route. Dither on the separate reference-palette conversion path remains
an open `QUANT-001` boundary. `LIBIMAGEQUANT` returns
`PILLOW_C_INVALID_ARGUMENT` in this build because the optional dependency is
not linked; the facade surfaces that boundary explicitly. The output is a
DLL-owned `P` image and must be released through the existing image-free ABI.
No AHK per-pixel loop or new ownership exception is introduced.

## Additive JPEG Metadata, Restart, And Extra ABI

The current Release x64 DLL also exports:

```cpp
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_restart_marker_extra_encode_options(
    const PillowCImage* image,
    const char* path,
    int quality,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* comment,
    std::size_t comment_size,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    const int* qtables,
    std::size_t qtable_count,
    int subsampling,
    int progressive,
    int optimize,
    int restart_marker_blocks,
    int restart_marker_rows,
    const std::uint8_t* extra,
    std::size_t extra_size);
```

All source pointers are borrowed only until the synchronous call returns. A
nonzero byte size requires a non-null pointer; `qtable_count` is zero when no
custom qtables are supplied and is bounded to one or two tables otherwise.
`restart_marker_blocks` and `restart_marker_rows` are mutually exclusive;
zero means the option is absent. The bounded native surface accepts `L`,
`RGB`, and `CMYK`, with DPI, progressive, optimize, bounded subsampling, one/
two custom qtables, explicit comment/ICC/EXIF/XMP, and raw `extra` composition.
The encoder writes the metadata/extra group once inside the DLL in this order:

```text
explicit EXIF -> raw extra bytes -> generated XMP -> generated ICC -> explicit COM -> DQT -> DRI
```

The facade keeps every normalized `BinaryBuffer` rooted through `DllCall`.
`keep_rgb`, quality keep/presets, qtables keep/presets, three/four-table
qtables, malformed marker streams, and broader exact byte/entropy parity are
explicit boundaries; no option is silently dropped. This additive export
raises source/DLL parity to `452/452`; the full AHK suite is `2650/2650`; and
the DLL SHA-256 is
`27937210D81D2DEE52E879CFF4EA6BC68C42E8B41953C51F4CF7E8A19E7F8011`.

## Additive JPEG Extra Marker-Stream ABI

The current Release x64 DLL exports:

```cpp
extern "C" __declspec(dllexport) int pillow_c_image_save_jpeg_extra_options(
    const PillowCImage* image,
    const char* path,
    int quality,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    int subsampling,
    int progressive,
    int optimize,
    const std::uint8_t* extra,
    std::size_t extra_size);
```

The export borrows `image`, `path`, and `extra` only for the duration of the
call and returns the standard status code. `extra_size == 0` is an empty
marker stream; a nonzero size requires a non-null `extra` pointer. The native
encoder writes the bounded `L`, `RGB`, or `CMYK` JPEG first, then inserts the
caller-provided marker bytes after the leading JFIF/Adobe header sequence and
before DQT. It does not parse or rewrite the marker stream. The facade roots
its `Pillow.Image.BinaryBuffer` until `DllCall` returns, so no caller Buffer
pointer outlives the call.

This bounded ABI rejects explicit comment/ICC/EXIF/XMP metadata, qtables,
keep-rgb, restart-marker combinations, unsupported modes, and invalid native
encoder option combinations with `PILLOW_C_INVALID_ARGUMENT`; no option is
silently discarded. The export remains part of the current `452/452` parity
set; the current DLL SHA-256 is
`27937210D81D2DEE52E879CFF4EA6BC68C42E8B41953C51F4CF7E8A19E7F8011`.

## Additive JPEG Metadata Plus Extra ABI

The current Release x64 DLL also exports:

```cpp
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_extra_encode_options(
    const PillowCImage* image,
    const char* path,
    int quality,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* comment,
    std::size_t comment_size,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    int subsampling,
    int progressive,
    int optimize,
    const std::uint8_t* extra,
    std::size_t extra_size);
```

The export borrows all image, path, and metadata/`extra` source pointers only
for the duration of the call. A nonzero size requires a non-null pointer; an
empty field is passed as size `0`. The bounded native encoder writes `L`,
`RGB`, or `CMYK`, then the DLL inserts one ordered marker group after
JFIF/Adobe headers and before DQT:

```text
explicit EXIF -> raw extra bytes -> generated XMP -> generated ICC -> explicit COM
```

`extra` is copied byte-for-byte without parsing or normalization. Generated
XMP and ICC are packetized by the existing native metadata helpers, and the
facade keeps each `BinaryBuffer` rooted through `DllCall`. The ABI carries no
qtables, keep-rgb, or restart-marker parameters; those compositions remain
explicit facade boundaries and are not silently dropped. Raw/facade
metadata-plus-extra tracers and metadata reopen tests are green. The export
remains part of the current `452/452` parity set; the full AHK suite is
`2650/2650`; and the current DLL SHA-256 is
`27937210D81D2DEE52E879CFF4EA6BC68C42E8B41953C51F4CF7E8A19E7F8011`.

## Additive JPEG QTables Plus Extra ABI

The current Release x64 DLL also exports:

~~~cpp
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_metadata_extra_encode_options(
    const PillowCImage* image,
    const char* path,
    int quality,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* comment,
    std::size_t comment_size,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    const int* qtables,
    std::size_t qtable_count,
    int subsampling,
    int progressive,
    int optimize,
    const std::uint8_t* extra,
    std::size_t extra_size);
~~~

All image, path, metadata, qtable, and `extra` pointers are borrowed only
until the synchronous call returns. A nonzero size requires a non-null
pointer. `qtables` points to `qtable_count * 64` native `int` values, and this
bounded route accepts one or two tables for `L`, `RGB`, and `CMYK`. It reuses
the existing native qtables encoders, then inserts one ordered marker group
after JFIF/Adobe headers and before DQT:

~~~text
explicit EXIF -> raw extra bytes -> generated XMP -> generated ICC -> explicit COM
~~~

`extra` is copied byte-for-byte without parsing or normalization. Progressive,
optimize, DPI, and bounded subsampling are passed to the existing native
encoder strategies. The ABI deliberately has no keep-rgb or restart-marker
parameters; those combinations remain separate explicit boundaries. The
facade roots qtable and metadata/extra Buffers through `DllCall`. Raw and
facade tests cover `L`, `RGB`, and `CMYK`, marker ordering and payload
preservation, progressive/optimized options, and qtables+XMP composition.
The export remains part of the current `452/452` parity set; the full AHK
suite is `2650/2650`; and the current DLL SHA-256 is
`27937210D81D2DEE52E879CFF4EA6BC68C42E8B41953C51F4CF7E8A19E7F8011`.

## Additive JPEG Keep-RGB Plus Extra ABI

The current Release x64 DLL also exports:

```cpp
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_keep_rgb_extra_encode_options(
    const PillowCImage* image,
    const char* path,
    int quality,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* comment,
    std::size_t comment_size,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    int subsampling,
    int progressive,
    int optimize,
    int keep_rgb,
    const std::uint8_t* extra,
    std::size_t extra_size);
```

All image, path, metadata, and `extra` pointers are borrowed only until the
synchronous call returns. A nonzero byte size requires a non-null pointer;
`keep_rgb` must be `1`. The bounded RGB route reuses the DLL-owned RGB
component encoder and writes Adobe APP14 transform `0`, then the single native
marker group before DQT. The bounded CMYK route preserves the existing
`keep_rgb` alias to the ordinary CMYK encoder. Marker composition remains:

```text
APP14/APP0 -> explicit EXIF -> raw extra bytes -> generated XMP -> generated ICC -> explicit COM -> DQT
```

The facade roots every normalized metadata/`extra` `BinaryBuffer` through
`DllCall`. This additive ABI carries no qtables or restart-marker parameters;
the facade rejects those keep-rgb combinations explicitly, so no option is
silently discarded. Raw and facade tests cover APP14/extra/DQT ordering, raw
payload preservation, SOF0 output, and RGB component count. The export is part
of the current `452/452` parity set; the full AHK suite is `2650/2650`; and
the current DLL SHA-256 is
`27937210D81D2DEE52E879CFF4EA6BC68C42E8B41953C51F4CF7E8A19E7F8011`.

## Additive JPEG QTables Keep-RGB Plus Extra ABI

The current Release x64 DLL also exports:

```cpp
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_extra_encode_options(
    const PillowCImage* image,
    const char* path,
    int quality,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* comment,
    std::size_t comment_size,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    const int* qtables,
    std::size_t qtable_count,
    int subsampling,
    int progressive,
    int optimize,
    int keep_rgb,
    const std::uint8_t* extra,
    std::size_t extra_size);
```

The export borrows all image, path, metadata, qtable, and `extra` pointers
only for the synchronous call. A nonzero byte size requires a non-null
pointer; `qtables` points to `qtable_count * 64` native `int` values, and this
bounded route accepts one or two tables. `keep_rgb` must be `1`. The covered
route is RGB with the DLL-owned qtables/keep-rgb encoder; the existing
no-qtables CMYK keep-rgb alias remains on its prior export. Restart-marker
composition is intentionally not carried by this ABI.

The native route preserves Adobe APP14 transform `0`, inserts raw `extra`
byte-for-byte with the existing metadata packetizers, and keeps the marker
group before DQT. The facade roots every normalized `BinaryBuffer` through
`DllCall` and raises explicit errors for unsupported restart-marker or
three/four-table combinations. Raw/facade tests and the full suite are
`1313/1313`, `1337/1337`, and `2650/2650`; source/DLL exports are `452/452`;
the current DLL SHA-256 is
`27937210D81D2DEE52E879CFF4EA6BC68C42E8B41953C51F4CF7E8A19E7F8011`.

JPEG ownership remains seven independently compiled Modules:
`pillow_c_codec_jpeg_decode.cpp` owns decode/open and draft
routes; `pillow_c_codec_jpeg_common.cpp` owns marker, DCT, quantization,
Huffman, progressive-scan, restart-marker, and entropy seams;
`pillow_c_codec_jpeg_encode_l.cpp`, `pillow_c_codec_jpeg_encode_rgb.cpp`, and
`pillow_c_codec_jpeg_encode_cmyk.cpp` own the grayscale, RGB/keep-rgb/qtables,
and CMYK/YCCK encoders; `pillow_c_codec_jpeg_save.cpp` owns public save-option
routing and metadata composition; and `pillow_c_codec_jpeg_metadata.cpp`
owns JPEG metadata exports. Their private C++ contract is
`pillow_c_codec_jpeg_internal.h`; it is not a DLL ABI header. `src/pillow_c.cpp`
contains only `#include "pillow_c_internal.h"`. No public export name,
signature, status code, handle ownership, source-pointer lifetime, or facade
route changed in this architecture packet; the additive keep-rgb-plus-extra
export is documented above and leaves all existing exports unchanged.

Release x64 currently has `0 Warning(s), 0 Error(s)`. Source and DLL exports
are `452/452` with zero difference. The current DLL SHA-256 is
`27937210D81D2DEE52E879CFF4EA6BC68C42E8B41953C51F4CF7E8A19E7F8011`.

`ARCH-MOD-001` preserves the public ABI while moving implementation behind
explicit internal seams. `pillow_c_internal.h` and `pillow_c_wic_internal.h`
are shared only by native translation units; `pillow_c_core.cpp` owns image
shape/size validation and the core allocation/free exports;
`pillow_c_memory.cpp` owns endian helpers, UTF-8 path conversion, and binary
file IO; `pillow_c_metadata.cpp` owns EXIF serialization;
`pillow_c_codec_wic.cpp` owns shared WIC factory/palette/format/frame helpers;
`pillow_c_codec_tiff.cpp` owns the complete TIFF parser/writer, IFD layout,
PackBits/LZW/Deflate strip codecs, metadata routes, and TIFF exports; and
`pillow_c_abi.cpp` owns status/version and image getter/data adapters. The AHK
facade's handle ownership and pointer lifetime contract are unchanged. The
monolith no longer retains duplicate bodies for these responsibilities.
`src/pillow_c.cpp` is about 46,059 lines after extraction from about 51,579
lines.

After the migration, Release x64 source/DLL export parity is `445/445` with
zero difference and zero build warnings/errors. The current DLL SHA-256 is
`33DC056E15794A5E451BD430BAAE9696AC8FE0960B793A8859C8A233699E2AA`.

`ARCH-MOD-002` was the historical JPEG extraction packet that first moved the
codec out of the original monolith. `ARCH-MOD-011` is the current physical
decomposition and is recorded above; the old single-unit layout is no longer
compiled by the Release project.

`ARCH-MOD-003` keeps the exported ABI unchanged while moving the complete PNG
codec and all PNG exports into `src/pillow_c_codec_png.cpp`. That translation
unit owns PNG parsing, metadata-sensitive WIC decode, zlib helpers, native
chunk encoding, all PNG save option families, and the existing PNG export
symbols. The main unit's ICO saver reuses PNG mode/encoder behavior through
the explicit internal C++ seams `pillow_c_png_custom_mode_spec` and
`pillow_c_png_encode_custom_image` declared in `pillow_c_internal.h`; these
are not DLL exports. The TIFF codec reuses the PNG-owned zlib internal seams
`pillow_c_inflate_zlib_deflate` and `pillow_c_append_zlib_stored`. PNG public
signatures, status codes, handle ownership, and pointer lifetimes are
unchanged. `src/pillow_c.cpp` is about 30,895 lines and the PNG unit is about
6,482 lines. Release x64 builds with `0 Warning(s), 0 Error(s)`; source/DLL
export parity remains `445/445` with zero difference; and the rebuilt DLL
SHA-256 is
`6236EE06518E445F2830D81D4F4D8C4F2F148FDACE05A1A3ECF8B1BCFFF1BCEB`.

`ARCH-MOD-004` preserves the public ABI while moving the complete
ImageDraw/default-font implementation and all draw/text/font exports into
`src/pillow_c_draw.cpp` (6,310 lines). `ARCH-MOD-005` preserves the same ABI
while moving BMP, PPM/Netpbm, QOI, TGA, XBM, and ICO implementations and their
exports into `src/pillow_c_codec_legacy.cpp` (3,237 lines). The main unit is
15,261 lines after these extractions. Public export names, parameter layouts,
status codes, handle ownership, and pointer lifetimes are unchanged. The
legacy unit consumes only explicit internal seams for Mode-1 raw sizing, LE
int32 decoding, int32-to-uint16 clipping, PNG custom encoding, and native
resizing; none of those seams are DLL exports.

Release x64 verification after the legacy extraction is `0 Warning(s),
0 Error(s)`; source/DLL export parity is `445/445` with zero difference; the
full AHK suite passes `2621/2621` in `27734ms`; and the current DLL SHA-256 is
`5FE477FD9D8F45473010D908D87B6903D9A2C931807AD4578A7F818454D364AF`.

`ARCH-MOD-006` preserves the public ABI while moving the complete GIF reader,
LZW codec, frame compositor, metadata parser, indexed/animation writers, and
all GIF exports into `src/pillow_c_codec_gif.cpp` (2,836 lines). The internal
C++ Interface adds exact RGB/L and GIF palette quantization seams plus the
existing buffer-refresh seam; none is a DLL export. The main unit is 12,482
lines and retains no duplicate GIF body or public forwarding shell. Export
names, signatures, status codes, handle ownership, and pointer lifetimes are
unchanged. Release x64 builds with `0 Warning(s), 0 Error(s)`; source/DLL
export parity is `445/445` with zero difference; full AHK is `2622/2622` in
`28485ms`; and DLL SHA-256 is
`9B84DFA634A14EAEC4ABD3607446C05DFC8D878BB8EE8B1BD46880695FB566FD`.

`ARCH-MOD-007` preserves the public ABI while moving the operations family
into `src/pillow_c_ops.cpp`: whole-image arithmetic/conversion,
palette/compositing, ImageChops, point/ImageOps, statistics, crop/paste/
transpose, quantization, and public fill/getpixel/putpixel exports. The ops
unit consumes explicit internal seams for core shape/buffer lifetime, masks,
color kernels, and shared quantization; `pillow_c.cpp` retains mode-name
ownership and no operations forwarding shell. The ops unit is 7,993 lines and
the main unit is 4,606 lines. Export names, parameter layouts, status codes,
handle ownership, and pointer lifetimes are unchanged. Release x64 builds with
`0 Warning(s), 0 Error(s)`; source/DLL exports remain `445/445` with zero
difference; full AHK is `2623/2623` in `29453ms`; and the rebuilt DLL SHA-256 is
`50C0FCB6CCABBA75098C5CB90732F540624EBD4BD562F24EE852E18D4E900EBE`.

`ARCH-MOD-008` preserves the public ABI while moving metadata ownership into
`src/pillow_c_metadata.cpp`. The module now contains the EXIF orientation and
typed-entry parsers, all EXIF byte serializers and exports, generic resolution/
hotspot/DIB accessors, PNG gamma/sRGB/chromaticity/text/ICC/EXIF/XMP/
transparency accessors, and the shared metadata blob copy seam. The main unit
contains none of these implementation bodies or public export definitions;
there is no forwarding export layer. `pillow_c.cpp` is 2,127 lines and
`pillow_c_metadata.cpp` is 3,218 lines. The internal C++ seam remains private
to native translation units; no DLL export name, parameter layout, status code,
handle ownership, or pointer lifetime changed.

The structural ownership test passes `1/1`; raw EXIF passes `23/23`; raw PNG
metadata passes `12/12`; the combined raw metadata filter passes `182/182`;
facade PNG metadata passes `12/12`; facade JPEG metadata/open passes `25/25`;
and the full suite passes `2624/2624` in `28796ms`. Release x64 builds with
`0 Warning(s), 0 Error(s)`; source/DLL export parity remains `445/445` with
zero difference; and the current DLL SHA-256 is
`BF103E627C2DAD72C8781AD42A289818F1074F4FC53D915555F4CBC72BEBE31D`.

`ARCH-MOD-009` preserves the public ABI while moving raw codec and buffer
ownership into `src/pillow_c_raw.cpp`. The Module owns raw mode specifications,
native raw pixel packing/unpacking, Mode-1 bit packing, raw size validation,
FromBuffer source alias refresh/detach lifetime, and these existing public
exports: `pillow_c_image_frombuffer_raw`, `pillow_c_image_refresh_buffer`,
`pillow_c_image_detach_buffer`, `pillow_c_image_set_bytes`,
`pillow_c_image_set_raw_bytes`, `pillow_c_image_put_data`,
`pillow_c_image_get_bytes`, and `pillow_c_image_get_raw_bytes`. The shared
internal Interface now resolves through the raw Module for
`pillow_c_refresh_const_buffer_view_image`,
`pillow_c_detach_buffer_view_image`, `pillow_c_checked_mode1_raw_size`, the
little-endian int/float read/write seams, and raw numeric rounding/clipping
seams consumed by legacy codecs, filters, draw, ops, and CMS. These are private
native seams, not DLL exports. Public names, parameter layouts, status codes,
image-handle ownership, source-pointer lifetime, and the AHK facade contract
are unchanged; the main unit retains no duplicate implementation or forwarding
export shell. `pillow_c.cpp` is 956 lines and `pillow_c_raw.cpp` is 1,189 lines.

Release x64 after this extraction has `0 Warning(s), 0 Error(s)`; source/DLL
export parity is `445/445` with zero difference; the full AHK suite passes
`2625/2625` in `28422ms`; and the current DLL SHA-256 is
`0756B2D899800BA9E0A81B95C220E9E712F694F44C99280B2B88F2BFCF49A269`.
This architecture packet does not increase the `59% ±4%` compatibility
estimate.

`ARCH-MOD-010` preserves the public ABI while closing the remaining monolith.
The existing `src/pillow_c_core.cpp` Module now owns the shared numeric,
shape/mask/palette, mode-name, mode-string, and mode ABI Implementation; the
new `src/pillow_c_effects.cpp` Module owns native linear/radial gradient and
Mandelbrot/noise/spread effect loops plus all corresponding public exports.
`pillow_c.cpp` is a one-line include-only translation unit. No exported name,
parameter layout, status code, image-handle ownership, source-pointer lifetime,
or internal Seam Interface changed, and no forwarding export shell remains.

Release x64 after this extraction has `0 Warning(s), 0 Error(s)`; raw
linear/radial/effects pass `1/1`, `1/1`, and `3/3`; facade linear/radial/effects
pass `1/1`, `1/1`, and `4/4`; the full AHK suite passes `2626/2626` in
`28843ms`; source/DLL export parity is `445/445` with zero difference; and the
current DLL SHA-256 is
`69FF7A140E8EA0E3AA5E1B75BF394D1ADA8C9B5271709242A89497C1D23DF484`.
This architecture packet does not increase the `59% ±4%` compatibility
estimate.

`ARCH-MOD-011` preserves the public ABI while physically decomposing the JPEG
implementation into seven independently compiled Modules. Decode/open is in
`pillow_c_codec_jpeg_decode.cpp`; shared marker/DCT/quantization/Huffman/
progressive/restart machinery is in `pillow_c_codec_jpeg_common.cpp`;
grayscale, RGB, and CMYK/YCCK encoders are in the three encode Modules;
public save routing is in `pillow_c_codec_jpeg_save.cpp`; and JPEG metadata
exports are in `pillow_c_codec_jpeg_metadata.cpp`. The private seam header is
`pillow_c_codec_jpeg_internal.h`; no symbol from it is exported. The seam
normalizes public integer `optimize` values explicitly at boolean encoder
calls, so the default `-1` path remains baseline Huffman encoding. The
include-only `pillow_c.cpp` has no JPEG implementation or forwarding shell.
Structural ownership passes `1/1`; raw/facade JPEG filters pass `212/212` and
`219/219`; Release x64 has `0 Warning(s), 0 Error(s)`; the full suite passes
`2627/2627` in `34375ms`; source/DLL exports remain `445/445` with zero
difference; and the rebuilt DLL SHA-256 is
`988DAA0F12507201F4AF8B01C889703FAD69614839868A71E3C6DB9ABD670462`.
This architecture packet does not increase the `59% ±4%` compatibility
estimate.

`FMT-TIFF-001AZ` changes no exported signature or export count. The existing
`pillow_c_image_save_tiff_frames_metadata_ex_options` ABI now owns the bounded
three-frame uncompressed big-endian `I;16B` DPI/ICC/XMP layout: three linked
IFDs, frame-local type-1 XMP/type-7 ICC payloads, per-frame resolution, and
exact native strips. The facade only routes `save_all`, seeks frames, and owns
frame lifetime. Raw/facade targeted composition tests pass `3/3` each; TIFF
filters pass `301/301` raw and `299/299` facade; the full suite passes
`2615/2615` in `19750ms`; source/DLL exports remain `445/445`; and the DLL
SHA-256 is
`33DC056E15794A5E451BD430BAAE9696AC8FE0960B793A8859C8A233699E2AA`.

`FMT-TIFF-001AY` changes no exported signature or export count. The existing
`pillow_c_image_save_tiff_frames_metadata_ex_options` ABI now owns the bounded
two-frame uncompressed big-endian `I;16B` DPI/ICC/XMP layout: each IFD carries
type-1 XMP, type-7 ICC, per-frame resolution, and an exact native strip, with
the repeated IFD blocks and alignment padding emitted inside the DLL. The
facade only routes `save_all` and owns frame lifetime. Source/DLL exports are
`445/445`, zero difference; Release x64 built with zero warnings/errors; DLL
SHA-256 is
`4870243958A0CB738CD123BB23472CABABD0B554ED6FCEA83106FA33E046B869`.

`FMT-JPEG-003BH` changes no signature or export count. The existing
`pillow_c_image_open_jpeg_draft_mode` now accepts the bounded requested
`YCbCr` mode for the stable RGB 4:2:2 fixture. WIC supplies Y `24x16` and
Cb/Cr `12x16`; the DLL applies libjpeg-turbo 3.1.1's exact h2v1 fancy
upsampling and interleaves the output as DLL-owned YCbCr bytes. RGB conversion,
resize, and fallback routes are not used. Release x64 builds with zero
warnings/errors; source/DLL exports remain `445/445`, zero difference, and
DLL SHA-256 is
`75172FA128F0A0CCBA892014C2444CB7BFA517B3CC06B9DA9F5B0DD52B5C58B8`.

`FMT-JPEG-003BI` changes no signature or export count. The existing
`pillow_c_image_open_jpeg_draft_mode` now accepts the bounded requested
`YCbCr` mode for a native-generated RGB 4:2:0 input whose reduced WIC
Y/Cb/Cr planes are all `24x16`; the DLL directly interleaves them. RGB
conversion, resize, and fallback routes are not used. Release x64 builds with
zero warnings/errors; source/DLL exports remain `445/445`, zero difference,
and DLL SHA-256 is
`6CD7A23D500A6C8D37AC3600CC2769186A9CA42F360A7CB305D021FBD56F6FA1`.

`FMT-JPEG-003BJ` changes no signature, native source, or DLL artifact. The
existing `pillow_c_image_open_jpeg_draft_mode` route selects scale 4 for the
bounded RGB 4:2:0 input and exposes exact YCbCr `12x8` bytes through the same
requested-mode ABI. Source/DLL exports remain `445/445`, zero difference, and
the Release x64 DLL SHA-256 remains
`6CD7A23D500A6C8D37AC3600CC2769186A9CA42F360A7CB305D021FBD56F6FA1`.

`FMT-JPEG-003BK` changes no signature, native source, or DLL artifact. The
existing `pillow_c_image_open_jpeg_draft_mode` route selects scale 8 for the
bounded RGB 4:2:0 input and exposes exact YCbCr `6x4` bytes through the same
requested-mode ABI. Source/DLL exports remain `445/445`, zero difference, and
the Release x64 DLL SHA-256 remains
`6CD7A23D500A6C8D37AC3600CC2769186A9CA42F360A7CB305D021FBD56F6FA1`.

`FMT-JPEG-003BL` changes no signature, export count, native source, or DLL
artifact. The existing `pillow_c_image_open_jpeg_draft_mode` route selects
decoder scale 4 for the stable RGB 4:2:2 fixture and exposes exact Pillow
YCbCr `12x8` bytes through the native h2v1 plane path. RGB conversion, resize,
fallback, and AHK pixel loops are not used. Source/DLL exports remain
`445/445`, zero difference; the Release x64 DLL SHA-256 remains
`6CD7A23D500A6C8D37AC3600CC2769186A9CA42F360A7CB305D021FBD56F6FA1`.

`FMT-JPEG-003BM` changes no signature or export count. The existing
`pillow_c_image_open_jpeg_draft_mode` route now distinguishes Pillow's
scale-dependent 4:2:2 chroma reconstruction: decoder scales 2/4 retain exact
h2v1 fancy filtering, while decoder scale 8 duplicates each reduced Cb/Cr
sample across two horizontal output pixels inside the DLL. The covered WIC
planes are `6x4 / 3x4 / 3x4`; RGB conversion, resize, fallback, and AHK pixel
loops are not used. Release x64 builds with zero warnings/errors; source/DLL
exports remain `445/445`, zero difference; DLL SHA-256 is
`E9C69DDDC99210311F4B543C20E7B0ABA801FE88A0C845AA1402446A3BCBE43C`.

`FMT-JPEG-003BN` changes no signature, export count, native source, or DLL
artifact. The existing `pillow_c_image_open_jpeg_draft_mode` route selects
decoder scale 1 for the stable RGB 4:2:2 fixture and exposes exact Pillow
YCbCr `48x32` bytes through DLL-owned planar decode/interleave. RGB conversion,
resize, fallback, and AHK pixel loops are not used. Source/DLL exports remain
`445/445`, zero difference; the Release x64 DLL SHA-256 remains
`E9C69DDDC99210311F4B543C20E7B0ABA801FE88A0C845AA1402446A3BCBE43C`.

`FMT-JPEG-003BO` changes no signature or export count. The existing
`pillow_c_image_open_jpeg_draft_mode` route now accepts WIC's full-scale RGB
4:2:0 planar shape `48x32 / 24x16 / 24x16` and reconstructs Cb/Cr through
DLL-owned h2v2 3/4-1/4 fancy filtering with exact fused `+8/+7` rounding.
RGB conversion, resize, fallback, and AHK pixel loops are not used. Release
x64 builds with zero warnings/errors; source/DLL exports remain `445/445`,
zero difference; DLL SHA-256 is
`F8BCA8B5A57D28241FA996DF686253DF3655F68FA97609D6577E1C15936EC92A`.

`FMT-JPEG-003BP` changes no signature, export count, native source, or DLL
artifact. The existing `pillow_c_image_open_jpeg_draft_mode` route selects
decoder scale 4 for the stable RGB 4:2:2 fixture and returns the exact Pillow
L `12x8` Y-plane bytes. RGB conversion, resize, fallback, and AHK pixel loops
are not used. Source/DLL exports remain `445/445`, zero difference; the
Release x64 DLL SHA-256 remains
`F8BCA8B5A57D28241FA996DF686253DF3655F68FA97609D6577E1C15936EC92A`.

`FMT-JPEG-003BQ` changes no signature, export count, native source, or DLL
artifact. The same `pillow_c_image_open_jpeg_draft_mode` route selects decoder
scale 8 for the stable RGB 4:2:2 fixture and returns exact Pillow L `6x4`
Y-plane bytes. RGB conversion, resize, fallback, and AHK pixel loops are not
used. Source/DLL exports remain `445/445`, zero difference; the Release x64
DLL SHA-256 remains
`F8BCA8B5A57D28241FA996DF686253DF3655F68FA97609D6577E1C15936EC92A`.

`FMT-JPEG-003BG` adds:

```text
pillow_c_image_open_jpeg_draft_mode(
    path,
    mode,
    target_width,
    target_height,
    out_image,
    out_scale)
```

The bounded requested-mode contract accepts an RGB JPEG source and mode `L`
for decoder-native grayscale draft. WIC requires a complete reduced planar
`{8bppY,8bppCb,8bppCr}` request; the DLL returns Y as the L image and owns the
temporary Cb/Cr buffers. Packed gray, lone-Y planar requests, RGB conversion,
resize, and fallback routes are not used. Release x64 builds with zero
warnings/errors; source/DLL exports are `445/445`, zero difference, and DLL
SHA-256 is
`C8EEEFE67A4EDCB7484F24064BB5986C70A0270FCD6AC8E6E2D42E84DB922EDD`.

`FMT-JPEG-003BF` changes no signature or export count. The existing
`pillow_c_image_open_jpeg_draft` now accepts three-component JPEGs. WIC's
source transform rejects reduced `24bppRGB`, so the DLL explicitly requests
its supported reduced `24bppBGR` and swaps B/R in one contiguous native hot
loop before exposing the RGB image. It does not full-decode/resize and has no
fallback. Release x64 builds with zero warnings/errors; source/DLL exports
remain `444/444`, zero difference, and DLL SHA-256 is
`A837676696AF28A1FB5FF500AA0BEC5532629DB997CC4B8322A3A669063B1155`.

`FMT-JPEG-003BE` adds:

```text
pillow_c_image_open_jpeg_draft(
    path,
    target_width,
    target_height,
    out_image,
    out_scale)
```

`path` is UTF-8 and `target_width` / `target_height` must be positive. On
success, the DLL selects one scale from `{8,4,2,1}`, decodes through
`IWICBitmapSourceTransform::CopyPixels`, returns the DLL-owned reduced image
through `out_image`, and writes the selected denominator through `out_scale`.
The covered CMYK/YCCK target `8x5` selects scale 2 and returns CMYK `9x6` with
Pillow-compatible reduced bytes. There is no resize fallback; ordinary
`pillow_c_image_open_jpeg` continues to request scale 1. Release x64 builds
with zero warnings/errors; source/DLL exports are `444/444`, zero difference,
and DLL SHA-256 is
`901CA0B96ECA45305BCF43FD81A4F348E08FAB7E98B76AD7366402EFE4534235`.

`FMT-JPEG-003BD` changes no signature, production source, or facade route.
The existing native YCCK decoder opens the project-owned odd `17x11` APP14
transform-2 fixture with `2x2/1x1/1x1/2x2` sampling as Pillow-compatible
CMYK bytes. Existing qtables keep exports normalize quality/qtables keep saves
to APP14 transform `0`, preserve both source DQT tables, and write `1x1` CMYK
components. No rebuild was required; source/DLL exports remain `443/443`, zero
difference, and Release x64 DLL SHA-256 remains
`6E871350292B5B9D07B2336F4E0BDEB205B2B3653DC81515B4C0E5C24CD4F831`.

`FMT-JPEG-003BC` changes no signature, facade production route, or native
source. The prior `>=14` APP13 structured-resource check is now bracketed by
raw/facade tests: 14-byte duplicates replace the scalar state and 13-byte
duplicates leave the previous valid state intact. No rebuild was required;
source/DLL exports remain `443/443`, zero difference, and Release x64 DLL
SHA-256 remains
`6E871350292B5B9D07B2336F4E0BDEB205B2B3653DC81515B4C0E5C24CD4F831`.

`FMT-JPEG-003BB` changes no signature but broadens the valid structured
ResolutionInfo payload contract from 16 bytes to 14 bytes, the exact span read
for XResolution, DisplayedUnitsX, YResolution, and DisplayedUnitsY. A valid
resource followed by a 15-byte duplicate now exposes the duplicate tuple
`(72.25,4,96.5,5)` through the existing scalar export. Release x64 rebuilt
with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`6E871350292B5B9D07B2336F4E0BDEB205B2B3653DC81515B4C0E5C24CD4F831`.

`FMT-JPEG-003BA` changes no exported signature, production facade route, or
native source. The existing APP13 parser overwrites its DLL-owned structured
ResolutionInfo scalar state on each valid `0x03ED` resource, and the existing
`pillow_c_image_metadata_jpeg_photoshop_resolution_info` export exposes the
last values `(72.25,4,96.5,5)` for facade nested-Map materialization. No
rebuild was required; source/DLL exports remain `443/443`, zero difference,
and the current Release x64 DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002DG` changes no exported signature and no native source. The existing
generic BYTE EXIF serializer/parser owns the exact 38-byte tag-700 blob with
count 5 at offset 26; the facade extends BYTE read/write routes symmetrically
to 35/35 while retaining TIFF UNDEFINED open enumeration. Native JPEG/PNG
codecs own saves/reopens, and EXIF tag 700 is not promoted to codec-level XMP.
No rebuild was required; source/DLL exports remain `443/443`, zero difference,
and the current Release x64 DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002DF` changes no exported signature and no native source. The existing
generic BYTE EXIF serializer/parser owns one exact 86-byte blob for tags
34856/37121/37500/41484 with counts `6/4/5/5`, inline 37121, and offsets
`62/68/74`; the facade extends BYTE read/write routes symmetrically to 34/34
while retaining TIFF UNDEFINED open enumeration. Native JPEG/PNG codecs own
saves/reopens with no AHK pixel loop. No rebuild was required; source/DLL
exports remain `443/443`, zero difference, and the current Release x64 DLL
SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002DE` changes no exported signature and no native source. The existing
generic UNDEFINED EXIF serializer/parser owns one exact 92-byte type-7 blob
for tags 347/33723/34675/37724 with counts `5/6/5/5` and offsets
`62/68/74/80`. The facade only adds the exact four tags to its bounded
UNDEFINED assignment route; native JPEG/PNG codecs own saves/reopens with no
AHK pixel loop. No rebuild was required; source/DLL exports remain `443/443`,
zero difference, and the current Release x64 DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002DD` changes no exported signature and no native source. The existing
generic EXIF serializer/parser owns one exact 86-byte mixed-type blob:
40960/41730 are UNDEFINED/type-7 with counts 4/4, while
41728/41729/41995 are BYTE/type-1 with counts 1/1/5; tag 41995 uses offset 74
and the blob ends with one alignment byte. The facade adds exact write-side
type routing and extends the BYTE read/write sets to 30/30 with zero
difference. Native JPEG/PNG codecs own saves/reopens with no AHK pixel loop.
No rebuild was required; source/DLL exports remain `443/443`, zero difference,
and the current Release x64 DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002DC` changes no exported signature and no native source. The existing
generic BYTE-array EXIF serializer/parser owns the exact 88-byte combined
blob for DNG tags 50828 and 52533/52534/52535, including inline count-4,
out-of-line counts `8/5/6`, offsets `62/70/76`, and the count-5 alignment
byte. The facade admits type-1 write/read while retaining those tags on TIFF
type-7 UNDEFINED open routes. BYTE read/write sets are both 27 tags with zero
difference. Native JPEG/PNG codecs own saves/reopens with no AHK pixel loop.
No rebuild was required; source/DLL exports remain `443/443`, zero difference,
and the current Release x64 DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002DB` changes no exported signature and no native source. The existing
generic BYTE-array EXIF serializer/parser owns the exact 80-byte combined
blob for DNG OpcodeList tags 51008, 51009, and 51022, including count-8 values
at offsets 50/58/66; the facade now admits write-side Buffer values and
enumerates type-1 readback while retaining the same tags on its TIFF type-7
UNDEFINED route. AHK limits one switch case to 20 parameters, so the exact
23-tag BYTE allowlist is represented as `20+3` equivalent true branches;
read/write sets are both 23 tags with zero difference. Native JPEG/PNG codecs
own saves/reopens with no AHK pixel loop. No rebuild was required; source/DLL
exports remain `443/443`, zero difference, and the current Release x64 DLL
SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002DA` changes no exported signature and no native source. The existing
generic BYTE-array EXIF serializer/parser owns the exact 60-byte combined
blob for `PhotoshopInfo` 34377 and `TimeCodes` 51043, including count-8 values
at offsets 38/46; the facade now admits those final two native-readable tags
for write-side Buffer values. The bounded BYTE-array read/write allowlists are
both 20 tags with zero set difference. Native JPEG/PNG codecs own explicit
saves and reopens with no AHK pixel loop. Other tags/counts/types and implicit
preservation remain out of scope. No rebuild was required; source/DLL exports
remain `443/443`, zero difference, and the current Release x64 DLL SHA-256
remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002CZ` changes no exported signature and no native source. The existing
generic BYTE-array EXIF serializer/parser owns the exact 118-byte combined
blob for XP tags 40092 through 40095, including counts `10/8/16/16`, offsets
`62/72/80/96`, and exact UTF-16LE+NUL payload bytes; the facade now admits
those exact tags for write-side Buffer values. Native JPEG/PNG codecs own
explicit saves and reopens with no AHK pixel or text-decoding loop. Other
tags/counts/types and implicit preservation remain out of scope. No rebuild
was required; source/DLL exports remain `443/443`, zero difference, and the
current Release x64 DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002CY` changes no exported signature and no native source. The existing
generic BYTE-array EXIF serializer/parser owns the exact 60-byte combined
blob for DNG embedded-profile tags 50831 and 50833, including count-8 values
at offsets 38/46; the facade now admits those exact tags for write-side Buffer
values. Native JPEG/PNG codecs own explicit saves and reopens with no AHK
pixel loop. EXIF profile-tag bytes remain distinct from codec-level ICC
metadata and ImageCms color management. Other tags/counts/types and implicit
preservation remain out of scope. No rebuild was required; source/DLL exports
remain `443/443`, zero difference, and the current Release x64 DLL SHA-256
remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002CX` changes no exported signature and no native source. The existing
generic BYTE-array EXIF serializer/parser owns the exact 68-byte combined
blob for DNG tags 50706, 50707, 50709, and 50710, including inline value
placement for counts `4/4/4/3`; the facade now admits those exact tags for
write-side Buffer values. Native JPEG/PNG codecs own explicit saves and
reopens with no AHK pixel loop. Other tags/counts/types, implicit preservation,
TIFF save, malformed payloads, and metadata interpretation remain out of
scope. No rebuild was required; source/DLL exports remain `443/443`, zero
difference, and the current Release x64 DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002CW` changes no exported signature and no native source. The existing
generic `pillow_c_exif_entries_byte_array_bytes` serializer and
`pillow_c_exif_byte_array_tag` parser already produce and read Pillow's
BYTE/type-1, count-16 representation for DNG tags 50969, 50972, 50973, 50781,
and 51111; the facade now admits those exact tags through the write/read route
while retaining 50969's separate TIFF type-7 open-side route. Explicit
JPEG/PNG save and reopen own codec work in the DLL with no AHK pixel loop.
Other tags/counts/types, implicit preservation, TIFF save, malformed payloads,
and digest interpretation remain out of scope. No rebuild was required;
source/DLL exports remain `443/443`, zero difference, and the current Release
x64 DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002CV` changes no exported signature and no native source. The existing
generic `pillow_c_exif_entries_byte_array_bytes` serializer and
`pillow_c_exif_byte_array_tag` parser already produce and read Pillow's
BYTE/type-1, count-8 representation for `ProfileGainTableMap` 52525; the
facade now admits 52525 through that write/read route while retaining the
separate TIFF type-7 open-side route. Explicit JPEG/PNG save and reopen own
the codec work in the DLL with no AHK pixel loop. Other values/counts/types,
implicit preservation, TIFF save, malformed payloads, gain-map interpretation,
and arbitrary tags remain out of scope. No rebuild was required; source/DLL
exports remain `443/443`, zero difference, and the current Release x64 DLL
SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002CU` changes no exported signature. Existing
`pillow_c_exif_undefined_tag` readback and TIFF type-7 EXIF serialization now
also cover inline IFD0 `ProfileGainTableMap` 52525 with exactly four bytes,
while retaining the `META-002CT` eight-byte route. Native admits only counts
four or eight for 52525; the facade already enumerates the tag through its
read-only UNDEFINED route. Gain-map interpretation/application, other counts/
types, writeback, malformed payloads, and arbitrary tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

`META-002CT` changes no exported signature. Existing
`pillow_c_exif_undefined_tag` readback and TIFF type-7 EXIF serialization now
also cover bounded IFD0 `ProfileGainTableMap` 52525 with exactly eight bytes.
The facade adds 52525 to the established read-only UNDEFINED route. Gain-map
interpretation/application, other values/counts/types, writeback, malformed
payloads, and arbitrary UNDEFINED tags remain out of scope. Release x64 builds
with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`250360EA186F9EB4A6296762B68C3E0FBEC51F45D2D7D452A60254716F40F90B`.

`META-002CS` changes no exported signature. Existing
`pillow_c_exif_ushort_array_tag` readback and TIFF type-3 EXIF serialization
now also cover bounded IFD0 `DefaultCropOrigin` 50719 with exactly two SHORT
values. Existing explicit dispatch keeps the RATIONAL, LONG, and SHORT forms
independent; the facade adds 50719 to its read-only ushort-array enumeration.
Crop interpretation/application, other values/counts/types, writeback,
malformed payloads, and arbitrary ushort arrays remain out of scope. Release
x64 builds with zero warnings/errors; source/DLL exports remain `443/443`,
zero difference; SHA-256 is
`41A4F6E7B0466195870D253D56B235542041D7BA1D8EF6A66763A9C0DCDC5FD1`.

`META-002CR` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` readback and TIFF type-4 EXIF serialization now
also cover bounded IFD0 `DefaultCropOrigin` 50719 with exactly two LONG values.
Native TIFF dispatch explicitly gates the existing 50719 RATIONAL route to
type 5 so type 4 reaches the uint-array route; the facade adds 50719 to its
read-only uint-array enumeration. Crop interpretation/application, other
values/counts/types, writeback, malformed payloads, and arbitrary uint arrays
remain out of scope. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`0A1D444C12E8F4F1868C727ADDD8659E712CEEFCEA93F5EE246FADDAC6A15497`.

`META-002CQ` changes no exported signature. Existing
`pillow_c_exif_ushort_array_tag` readback and TIFF type-3 EXIF serialization
now also cover bounded IFD0 `DefaultCropSize` 50720 with exactly two SHORT
values. The facade adds 50720 to its read-only ushort-array enumeration;
existing explicit dispatch keeps the RATIONAL, LONG, and SHORT forms
independent. Crop-size interpretation/application, other values/counts/types,
writeback, malformed payloads, and arbitrary ushort arrays remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`A90061B241C951A9EB5347A663BCF783F03D330950BF055E1C646009FF4648D5`.

`META-002CP` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` readback and TIFF type-4 EXIF serialization now
also cover bounded IFD0 `DefaultCropSize` 50720 with exactly two LONG values.
Native TIFF dispatch explicitly gates the existing 50720 RATIONAL route to
type 5 so type 4 reaches the uint-array route; the facade adds 50720 to its
read-only uint-array enumeration. Crop-size interpretation/application, other
values/counts/types, writeback, malformed payloads, and arbitrary uint arrays
remain out of scope. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`D7F6F25EF289740291878406F9B2F71FB51045EA8D86508556FB6EAD0F559F2A`.

`META-002CO` changes no exported signature. Existing
`pillow_c_exif_rational_array_tag` readback and TIFF type-5 EXIF serialization
now also cover bounded IFD0 `DefaultCropSize` 50720 with exactly two RATIONAL
values. The facade adds 50720 to the established read-only RATIONAL-array
route. Crop-size interpretation/application, other values/counts/types,
arbitrary payload families, writeback, malformed payloads, and arbitrary
RATIONAL arrays remain out of scope. Release x64 builds with zero warnings/
errors; source/DLL exports remain `443/443`, zero difference; SHA-256 is
`6246F478156903A9A8A5AFA2E67682D6D6062608AC3F7DA985B9F451BDD4AA3A`.

`META-002CN` changes no exported signature. Existing
`pillow_c_exif_rational_array_tag` readback and TIFF type-5 EXIF serialization
now also cover bounded IFD0 `DefaultCropOrigin` 50719 with exactly two
RATIONAL values. The facade adds 50719 to the established read-only RATIONAL-
array route. Crop interpretation/application, other values/counts/types,
arbitrary payload families, writeback, malformed payloads, and arbitrary
RATIONAL arrays remain out of scope. Release x64 builds with zero warnings/
errors; source/DLL exports remain `443/443`, zero difference; SHA-256 is
`83CCF587C943CF623C7128AB7AB902C02CA326390C5AE5A8167B2EAB66572364`.

`META-002CM` changes no exported signature. Existing
`pillow_c_exif_rational_array_tag` readback and TIFF type-5 EXIF serialization
now also cover bounded IFD0 `DefaultScale` 50718 with exactly two RATIONAL
values. The facade adds 50718 to the established read-only RATIONAL-array
route. Scale interpretation/application, other values/counts/types, arbitrary
payload families, writeback, malformed payloads, and arbitrary RATIONAL arrays
remain out of scope. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`1506BDB1A43C07E006A498F8159D0A146E2847107F6107296DB5222E9F9FD082`.

`META-002CL` changes no exported signature. Existing TIFF scalar-integer EXIF
serialization and `pillow_c_exif_uint_tag` readback now also cover bounded
IFD0 `WhiteLevel` 50717 only when stored as TIFF type 3/count 1. The facade
adds 50717 to the established read-only integer route. White-level
interpretation/application, other values/counts/types, arbitrary payload
families, writeback, malformed payloads, and arbitrary SHORT tags remain out
of scope. Release x64 builds with zero warnings/errors; source/DLL exports
remain `443/443`, zero difference; SHA-256 is
`D0B6E765558584F92F6E1640B777B639E42CF5D7232649CB9EA3A49CD5897E21`.

`META-002CK` changes no exported signature. Existing
`pillow_c_exif_signed_rational_tag` readback and TIFF type-10 EXIF serialization
now also cover bounded IFD0 `BlackLevelDeltaV` 50716 with exactly one
SRATIONAL value. The facade adds 50716 to the established read-only signed-
rational scalar route. Delta interpretation/application, other values/counts/
types, arbitrary payload families, writeback, malformed payloads, and
arbitrary SRATIONAL values remain out of scope. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`6EE08274AC0FCE33DC47A5FCA7FC2133C6C328A930A2FDC286919D5C7DB4E60D`.

`META-002CJ` changes no exported signature. Existing
`pillow_c_exif_signed_rational_array_tag` readback and TIFF type-10 EXIF
serialization now also cover bounded IFD0 `BlackLevelDeltaH` 50715 with
exactly two SRATIONAL values. The facade adds 50715 to the established read-
only SRATIONAL-array route. Delta interpretation/application, other values/
counts/types, arbitrary payload families, writeback, malformed payloads, and
arbitrary SRATIONAL arrays remain out of scope. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`C361674E869131CC09CD70DCAC21B786CFF9E7FF496C72C06AB2874D672AF726`.

`META-002CI` changes no exported signature. Existing
`pillow_c_exif_rational_array_tag` readback and TIFF type-5 EXIF serialization
now also cover bounded IFD0 `BlackLevel` 50714 with exactly four RATIONAL
values. The facade adds 50714 to the established read-only RATIONAL-array
route. Black-level interpretation/application, other values/counts/types,
arbitrary payload families, writeback, malformed payloads, and arbitrary
RATIONAL arrays remain out of scope. Release x64 builds with zero warnings/
errors; source/DLL exports remain `443/443`, zero difference; SHA-256 is
`CDADE4E2A3019A6766D8930BA72D907B9067C798B16E411DD84A6DA285E94974`.

`META-002CH` changes no exported signature. Existing
`pillow_c_exif_ushort_array_tag` readback and TIFF type-3 EXIF serialization now
also cover bounded IFD0 `BlackLevelRepeatDim` 50713 with exactly two SHORT
values. The facade adds 50713 to the established read-only SHORT-array route.
Black-level interpretation/application, other values/counts/types, arbitrary
payload families, writeback, malformed payloads, and arbitrary SHORT arrays
remain out of scope. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`695E064262A52A218368A3DA7CD5FE10B113CC7949F99B58AC7DFB3EF891BE4B`.

`META-002CG` changes no exported signature. Existing
`pillow_c_exif_ushort_array_tag` readback and TIFF type-3 EXIF serialization now
also cover bounded IFD0 `LinearizationTable` 50712 with exactly four SHORT
values. The facade adds 50712 to the established read-only SHORT-array route.
Table application, other values/counts/types, arbitrary payload families,
writeback, malformed payloads, and arbitrary SHORT arrays remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`B1967A33E132A7EC74EA633A2829453FFB113CD07691C700A009260325265D2D`.

`META-002CF` changes no exported signature. Existing TIFF scalar-integer EXIF
serialization and `pillow_c_exif_uint_tag` readback now also cover bounded IFD0
`RowInterleaveFactor` 50975 only when stored as TIFF type 4/count 1. The facade
adds 50975 to the established read-only integer route. Row-interleave
interpretation/application, other values/counts/types, arbitrary payload
families, writeback, malformed payloads, and arbitrary LONG tags remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`7A6F5FF56DF7FC8559F8CAE389614445D9F664284DA0646CBA21787EF07F12C0`.

`META-002CE` changes no exported signature. Existing TIFF scalar-integer EXIF
serialization and `pillow_c_exif_uint_tag` readback now also cover bounded IFD0
`SubTileBlockSize` 50974 only when stored as TIFF type 4/count 1. The facade
adds 50974 to the established read-only integer route. Sub-tile interpretation/
application, other values/counts/types, arbitrary payload families, writeback,
malformed payloads, and arbitrary LONG tags remain out of scope. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`875143AD68ABCFB903F1CB06483E69387598A8C8734A0577EBA95612745C39E7`.

`META-002CD` changes no exported signature. Existing
`pillow_c_exif_byte_array_tag` readback and TIFF type-1 EXIF serialization now
also cover bounded IFD0 `RawDataUniqueID` 50781 with exactly 16 bytes. The
facade adds 50781 to the established read-only BYTE-array route. Identifier
interpretation/validation, other counts/types, arbitrary payload families,
writeback, malformed payloads, and arbitrary BYTE tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`5FBD24C732E2B73E0A1B2FBD28E818457CA3F59F38E086859AB807636911E304`.

`META-002CC` changes no exported signature. Existing
`pillow_c_exif_byte_array_tag` readback and TIFF type-1 EXIF serialization now
also cover bounded IFD0 `OriginalRawFileDigest` 50973 with exactly 16 bytes.
The facade adds 50973 to the established read-only BYTE-array route. Digest
interpretation/validation, other counts/types, arbitrary payload families,
writeback, malformed payloads, and arbitrary BYTE tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`DE7D42A30F17CB0FA9A748D701D69BF026DBF84B311B9836016D4377287475D2`.

`META-002CB` changes no exported signature. Existing
`pillow_c_exif_byte_array_tag` readback and TIFF type-1 EXIF serialization now
also cover bounded IFD0 `RawImageDigest` 50972 with exactly 16 bytes. The
facade adds 50972 to the established read-only BYTE-array route. Digest
interpretation/validation, other counts/types, arbitrary payload families,
writeback, malformed payloads, and arbitrary BYTE tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`792BFCFB09823D981366D875ADD2128C650A38842E069A8EEE6D1E18F582A577`.

`META-002CA` changes no exported signature. Existing
`pillow_c_exif_undefined_tag` readback and TIFF type-7 EXIF serialization now
also cover bounded IFD0 `PreviewSettingsDigest` 50969 with exactly 16 bytes.
The facade adds 50969 to the established read-only UNDEFINED route. Digest
interpretation/validation, other counts/types, arbitrary payload families,
writeback, malformed payloads, and arbitrary UNDEFINED tags remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`F06D8C96A6F6EF9FBA0D11E50DC17B83E807CB403C194FB9476A9BECBDEA937D`.

`META-002BZ` changes no exported signature. Existing
`pillow_c_exif_undefined_tag` readback and TIFF type-7 EXIF serialization now
also cover bounded IFD0 `OpcodeList3` 51022 with exactly eight bytes. The
facade adds 51022 to the established read-only UNDEFINED route. Opcode
decoding/application, other counts/types, arbitrary payload families,
writeback, malformed payloads, and arbitrary UNDEFINED tags remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`D600F073D8DEC391194B3785B81071A14484317E809BF01F33973BA866C9A174`.

`META-002BY` changes no exported signature. Existing
`pillow_c_exif_undefined_tag` readback and TIFF type-7 EXIF serialization now
also cover bounded IFD0 `OpcodeList2` 51009 with exactly eight bytes. The
facade adds 51009 to the established read-only UNDEFINED route. Opcode
decoding/application, other counts/types, arbitrary payload families,
writeback, malformed payloads, and arbitrary UNDEFINED tags remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`C543E5341058B6EDACF063C0F58C71A758F2341CBDE9713073E00E2B4582E954`.

`META-002BX` changes no exported signature. Existing
`pillow_c_exif_undefined_tag` readback and TIFF type-7 EXIF serialization now
also cover bounded IFD0 `OpcodeList1` 51008 with exactly eight bytes. The
facade adds 51008 to the established read-only UNDEFINED route. Opcode
decoding/application, other counts/types, arbitrary payload families,
writeback, malformed payloads, and arbitrary UNDEFINED tags remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`64D75DA69A5557066AD3B071BBD7C51821EC7F0487C72E7A1E42FA62ECDDED85`.

`META-002BW` changes no exported signature. Existing
`pillow_c_exif_undefined_tag` readback and TIFF type-7 EXIF serialization now
also cover bounded IFD0 `OriginalRawFileData` 50828 with exactly eight bytes.
The facade adds 50828 to the established read-only UNDEFINED route. Original-
file decoding/interpretation, other counts/types, arbitrary payload families,
writeback, malformed payloads, and arbitrary UNDEFINED tags remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`995D76AB0C77C005DE40DB5EB6E36540C62DD618C170B58D12986B415EE8E00A`.

`META-002BV` changes no exported signature. Existing TIFF scalar-integer EXIF
serialization and `pillow_c_exif_uint_tag` readback now also cover bounded IFD0
`ColorimetricReference` 50879 only when stored as TIFF type 3/count 1. The
facade adds 50879 to the established read-only integer route. Colorimetric
interpretation/application, other counts/types, arbitrary payload families,
writeback, malformed values, and arbitrary SHORT tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`7E4ACDC7F08C285ABAB29879A0D766D6C882FB32781A5A1506571D8B634D106A`.

`META-002BU` changes no exported signature. Existing
`pillow_c_exif_signed_rational_array_tag` readback and TIFF type-10 EXIF
serialization now also cover bounded IFD0 `CurrentPreProfileMatrix` 50834
with exactly nine signed rational values. The facade adds 50834 to the
established read-only signed-rational-array route. Matrix interpretation/
application, other dimensions/types, arbitrary payload families, writeback,
malformed rationals, and arbitrary SRATIONAL tags remain out of scope. Release
x64 builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`FD655F6E558AAAF25A3C353618CCC047FE9CCAB0966B251D02BF5973C4DBA5DB`.

`META-002BT` changes no exported signature. Existing
`pillow_c_exif_byte_array_tag` readback and TIFF type-1 EXIF serialization now
also cover bounded IFD0 `CurrentICCProfile` 50833 with exactly eight bytes.
The facade adds 50833 to the established read-only BYTE-array route without
synthesizing `Info["icc_profile"]`. ICC validation/application, other counts/
types, arbitrary payload families, writeback, malformed payloads, and
arbitrary BYTE tags remain out of scope. Release x64 builds with zero warnings/
errors; source/DLL exports remain `443/443`, zero difference; SHA-256 is
`5EF8D42F9C0A749D37A1596D4BA4A242BD5BE5CE41A8665F5902028C938C5DD5`.

`META-002BS` changes no exported signature. Existing
`pillow_c_exif_signed_rational_array_tag` readback and TIFF type-10 EXIF
serialization now also cover bounded IFD0 `AsShotPreProfileMatrix` 50832 with
exactly nine signed rational values. The facade adds 50832 to the established
read-only signed-rational-array route. Matrix interpretation/application,
other dimensions/types, arbitrary payload families, writeback, malformed
rationals, and arbitrary SRATIONAL tags remain out of scope. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`FBD112B12870F0C106C5184FAD26FE23C2C24F5063C0015CFC6E764BABCC44E4`.

`META-002BR` changes no exported signature. Existing
`pillow_c_exif_byte_array_tag` readback and TIFF type-1 EXIF serialization now
also cover bounded IFD0 `AsShotICCProfile` 50831 with exactly eight bytes. The
facade adds 50831 to the established read-only BYTE-array route without
synthesizing `Info["icc_profile"]`. ICC validation/application, other counts/
types, arbitrary payload families, writeback, malformed payloads, and
arbitrary BYTE tags remain out of scope. Release x64 builds with zero warnings/
errors; source/DLL exports remain `443/443`, zero difference; SHA-256 is
`E801D2E72A4683B59DE718F204E83B9D62B6A0A585C13A41463AC00F1D3A7578`.

`META-002BQ` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` readback and TIFF type-4 EXIF serialization now
cover bounded IFD0 `MaskedAreas` 50830 with exactly four or eight unsigned LONG
values, representing one or two uninterpreted rectangles. The facade's
dynamic read-only LONG-array route is unchanged. Mask interpretation/
normalization, other rectangle counts/types, arbitrary payload families,
writeback, malformed arrays, and arbitrary LONG tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`12AFB52CA0375E5A926330DED25490E2AFA436C9CCB9D122C3EFCDBC85116B47`.

`META-002BP` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` readback and TIFF type-4 EXIF serialization now
also cover bounded IFD0 `MaskedAreas` 50830 with exactly four unsigned LONG
values representing one uninterpreted rectangle. The facade adds 50830 to the
established read-only LONG-array route. Multiple rectangles, mask
interpretation/normalization, other counts/types, arbitrary payload families,
writeback, malformed arrays, and arbitrary LONG tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`935BEAF26BAD20D9D19435BE70EC2C2C925FE8A567ADCC2B05F9A68CC42185F0`.

`META-002BO` changes no exported signature. Existing
`pillow_c_exif_ushort_array_tag` readback and TIFF type-3 EXIF serialization
now also cover bounded IFD0 `ActiveArea` 50829 with exactly four unsigned SHORT
values. TIFF metadata dispatch admits tag 50829 through the SHORT-array branch
only for type 3 and through the LONG-array branch only for type 4. The facade
adds 50829 to the established read-only SHORT-array route. Active-area
interpretation/normalization, other counts/types, arbitrary payload families,
writeback, malformed arrays, and arbitrary SHORT tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`4F358ECF4C0672369E7157E1CB0E1943D2B64C4585998C56ED1FBD1F4FAEF283`.

`META-002BN` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` readback and TIFF type-4 EXIF serialization now
also cover bounded IFD0 `ActiveArea` 50829 with exactly four unsigned LONG
values. The facade adds 50829 to the established read-only LONG-array route.
Active-area interpretation/normalization, alternate SHORT form, other counts/
types, arbitrary payload families, writeback, malformed arrays, and arbitrary
LONG tags remain out of scope. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`BF3B4D8B718C02AC3B9BD9747AF1ADC103EC20D691702E838541BCE04494D4C5`.

`META-002BM` changes no exported signature. Existing `pillow_c_exif_ascii_tag`
readback and TIFF type-2 EXIF serialization now also cover bounded IFD0
`CameraLabel` 51092 with exactly nine bytes including the trailing NUL. The
facade adds 51092 to the established read-only ASCII route. Camera-label
interpretation, non-ASCII encodings, other counts/types, arbitrary payload
families, writeback, malformed strings, and arbitrary ASCII tags remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`6359CFB3DFD63B0C9D47DF7DE814F4F212DCF1198ECCDCF5F258113B7B3DEF55`.

`META-002BL` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` readback and TIFF type-4 EXIF serialization now
also cover bounded IFD0 `OriginalDefaultCropSize` 51091 with exactly two
unsigned LONG values. TIFF metadata dispatch reads the entry type so tag 51091
uses the existing RATIONAL-array route for type 5 and LONG-array route for
type 4. The facade enumerates 51091 through both read-only routes and only the
matching serialized type materializes. Crop-size interpretation/normalization,
other counts/types, arbitrary payload families, writeback, malformed arrays,
and arbitrary LONG tags remain out of scope. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`4F8415CF5F80FDA284A0011F591255AECE49737D5BF68FA674425B355436F7DC`.

`META-002BK` changes no exported signature. Existing
`pillow_c_exif_rational_array_tag` readback and TIFF type-5 EXIF serialization
now also cover bounded IFD0 `OriginalDefaultCropSize` 51091 with exactly two
unsigned rational values. The facade adds 51091 to the established read-only
RATIONAL-array route. Crop-size interpretation/normalization, alternate counts
or types, arbitrary payload families, writeback, malformed arrays, and
arbitrary RATIONAL tags remain out of scope. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`46C943988AD2C5448E4459DA4A258B5040E570ADA1058950833D8C4517097F61`.

`META-002BJ` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` readback and TIFF type-4 EXIF serialization now
also cover bounded IFD0 `OriginalBestQualityFinalSize` 51090 with exactly two
unsigned LONG values. The facade adds 51090 to the established read-only
LONG-array route. Image-size interpretation/normalization, alternate counts or
types, arbitrary payload families, writeback, malformed arrays, and arbitrary
LONG tags remain out of scope. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`5F392D7940A11C6D802E91CBED8BF32BB5369E0E31140B0A3862DDE44127CC8D`.

`META-002BI` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` readback and TIFF type-4 EXIF serialization now
also cover bounded IFD0 `OriginalDefaultFinalSize` 51089 with exactly two
unsigned LONG values. The facade adds 51089 to the established read-only
LONG-array route. Image-size interpretation/normalization, alternate counts or
types, arbitrary payload families, writeback, malformed arrays, and arbitrary
LONG tags remain out of scope. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`7FB1A351E254E8592592DAC374B2D07F40AED028FABA548273B09E2AF85B71CB`.

`META-002BH` changes no exported signature. Existing `pillow_c_exif_ascii_tag`
readback and TIFF type-2 EXIF serialization now also cover bounded IFD0
`ReelName` 51081 with exactly ten bytes including the trailing NUL. The facade
adds 51081 to the established read-only ASCII route. Reel/timeline
interpretation, non-ASCII encodings, arbitrary payload families, writeback,
malformed strings, and arbitrary ASCII tags remain out of scope. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`EE6220F222AC8D0CD59D6A6C6C25411231FC9F80400B7E16F2547B4F6EE9736E`.

`META-002BG` changes no exported signature. Existing
`pillow_c_exif_rational_tag` readback and TIFF type-5 EXIF serialization now
also cover bounded IFD0 `TStop` 51058 with exactly one rational value. The
facade adds 51058 to the established read-only rational route. T-stop
interpretation/normalization, arrays, arbitrary counts, writeback, malformed
payload families, and arbitrary RATIONAL tags remain out of scope. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`387990225767EA05C7C319AFBBA98D10B4776FD16D595FC1FEBD5A1FBBE3CD04`.

`META-002BF` changes no exported signature. Existing
`pillow_c_exif_signed_rational_tag` readback and TIFF type-10 EXIF
serialization now also cover bounded IFD0 `FrameRate` 51044 with exactly one
signed rational value. The facade adds 51044 to the established read-only
signed-rational route. Frame-rate interpretation/normalization, arrays,
arbitrary counts, writeback, malformed payload families, and arbitrary
SRATIONAL tags remain out of scope. Release x64 builds with zero warnings/
errors; source/DLL exports remain `443/443`, zero difference; SHA-256 is
`6B07E5B4A172C4E5D680AFAC0814F738D1E649C02ED572657A39F66540872B16`.

`META-002BE` changes no exported signature. Existing
`pillow_c_exif_byte_array_tag` readback and TIFF type-1 EXIF serialization now
also cover bounded IFD0 `TimeCodes` 51043 with exactly eight bytes. The facade
adds 51043 to the established read-only byte-array route. Timecode
interpretation, multiple timecodes, arbitrary counts, writeback, malformed
payload families, and arbitrary BYTE tags remain out of scope. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`D9ABA496C6A31656C34B942FE9609470ABA377EB27FE8B7CD5AAFE55F844D216`.

`META-002BD` changes no exported signature. Existing
`pillow_c_exif_double_array_tag` readback and TIFF type-12 EXIF serialization
now also cover bounded IFD0 `NoiseProfile` 51041 with exactly four double
values, completing accepted counts 2/4/6/8. The facade already routes 51041
through the ABI's dynamic required-count protocol. Noise-model interpretation,
other counts, writeback, malformed payload families, and arbitrary DOUBLE tags
remain out of scope. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`09C6C3CD5E5864F903A1D1ECA6A8094E87562EB03B1605252D3FAADE322B7048`.

`META-002BC` changes no exported signature. Existing
`pillow_c_exif_double_array_tag` readback and TIFF type-12 EXIF serialization
now also cover bounded IFD0 `NoiseProfile` 51041 with exactly two or eight
double values, while count 6 remains intact. The facade already routes 51041
through the ABI's dynamic required-count protocol. Noise-model interpretation,
other counts, writeback, malformed payload families, and arbitrary DOUBLE tags
remain out of scope. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`177426C6BEACB6971D7E7111008E1B0075F0B5AA4360678D92AE56671E89261C`.

`META-002BB` changes no exported signature. Existing
`pillow_c_exif_double_array_tag` readback and TIFF type-12 EXIF serialization
now also cover bounded IFD0 `NoiseProfile` 51041 with exactly six double
values. The facade adds 51041 to the established read-only dynamic required-
count route. Noise-model interpretation, arbitrary counts, writeback,
malformed payload families, and arbitrary DOUBLE tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`6D54586EE0C0FD10E6FFA0803E831A1CFC3F8BC7C9E8223D64C47F9559E2A980`.

`META-002BA` changes no exported signature. Existing
`pillow_c_exif_float_array_tag` readback and TIFF type-11 EXIF serialization
now also cover bounded IFD0 `ProfileToneCurve` 50940 with exactly 18 float32
values representing nine control points, while the count-6 route remains
intact. The facade already uses the ABI's dynamic required-count protocol and
needs no new tag routing. Curve evaluation, arbitrary control-point counts,
writeback, malformed payload families, and arbitrary FLOAT tags remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`E05427246016849891808FA38AF4E53E8EC5713B228A3896E617A3CEAF17DD92`.

`META-002AZ` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` and `pillow_c_exif_float_array_tag` readback plus
TIFF EXIF serialization now compose bounded IFD0 `ProfileHueSatMapDims` 50937
LONG/count-3 `(6,3,1)` with `ProfileHueSatMapData1` 50938 and
`ProfileHueSatMapData2` 50939 as FLOAT/count-54 arrays. The facade already uses
both ABIs' dynamic required-count protocols and needs no new tag routing. DNG
map interpretation/allocation, arbitrary dimensions/counts, writeback,
malformed payload families, and arbitrary tags remain out of scope. Release
x64 builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`C9C8F0BF21CA3F6B99DE40AEBCDDDF633C3F88D83C30E2C78395028FE269181A`.

`META-002AY` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` and `pillow_c_exif_float_array_tag` readback plus
TIFF EXIF serialization now compose bounded IFD0 `ProfileLookTableDims` 50981
LONG/count-3 `(6,3,1)` with `ProfileLookTableData` 50982 FLOAT/count-54. The
facade already uses both ABIs' dynamic required-count protocols and needs no
new tag routing. DNG look-table interpretation/allocation, arbitrary table
dimensions/counts, writeback, malformed payload families, and arbitrary tags
remain out of scope. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`474370B7068E785B4C90B3D4FC7EB08F65ACA65429533A89B2155C320770F5AB`.

`META-002AX` changes no exported signature. Existing
`pillow_c_exif_float_array_tag` readback and TIFF type-11 EXIF serialization
now also cover bounded IFD0 `ProfileLookTableData` 50982 with exactly 18
float32 values, while the previously covered count-6 route remains intact.
The facade already uses the ABI's dynamic required-count protocol and needs no
new tag routing. DNG look-table interpretation/allocation, arbitrary table
dimensions/counts, writeback, malformed payload families, and arbitrary FLOAT
tags remain out of scope. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`4DDD2E0207F2A4C92FA542B58D0660A6A84C093CA1A21E5E1E1104DC0C26F1E6`.

`META-002AW` changes no exported signature. Existing
`pillow_c_exif_float_array_tag` readback and TIFF type-11 EXIF serialization
now also cover one bounded IFD0 `ProfileLookTableData` 50982 entry with
exactly six float32 values. The facade only adds 50982 to the established
read-only `FloatArrayTags` route. DNG look-table interpretation/allocation,
broader table dimensions/counts, writeback, malformed payload families, and
arbitrary FLOAT tags remain out of scope. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`A196C4BE9A353E5A104EAE2A528B31DA0BA053056D4C2D5969BE0766D4E9B03B`.

`META-002AV` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` readback and TIFF EXIF serialization now also
cover one bounded IFD0 `ProfileLookTableDims` 50981 LONG/type-4 entry with
exactly three values. The facade only adds 50981 to the established read-only
required-count route. DNG look-table interpretation/allocation, FLOAT data,
writeback, malformed payload families, and arbitrary LONG arrays remain out
of scope. Release x64 builds with zero warnings/errors; source/DLL exports
remain `443/443`, zero difference; SHA-256 is
`D3C5327D0027B9292427B7187AB8016A58BF9B7470DBD204FC623ECFA13F2BD5`.

`META-002AU` changes no exported signature. Existing
`pillow_c_exif_rational_tag` readback and TIFF type-5 EXIF serialization now
also cover one bounded IFD0 `NoiseReductionApplied` 50935 RATIONAL/count-1
entry. The facade only adds 50935 to the established read-only rational route.
Noise-reduction interpretation, writeback, malformed payload families, and
arbitrary RATIONAL tags remain out of scope. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`8BCFDA444FC3DD98B21D5A01AF6C63ADA3BDA22858D0A7B60AE3DAD91E82AD77`.

`META-002AT` changes no exported signature. Existing
`pillow_c_exif_float_array_tag` readback and TIFF type-11 EXIF serialization
now also cover one bounded IFD0 `ProfileToneCurve` 50940 with exactly six
float32 values representing three control points. The facade only adds 50940
to the established read-only `FloatArrayTags` route. Curve evaluation,
arbitrary control-point counts, writeback, malformed payload families, and
arbitrary FLOAT tags remain out of scope. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`EDF2043BBEC611FEBA4261687FBE0A4907DCECC07FCC5639A641BAC91E941373`.

`META-002AS` changes no exported signature. Existing
`pillow_c_exif_float_array_tag` readback and TIFF type-11 EXIF serialization
now also cover IFD0 `ProfileHueSatMapData2` 50939 when the source contains
exactly six in-range float32 values. The facade only adds 50939 to the
established read-only `FloatArrayTags` route. DNG map interpretation/
allocation, writeback, malformed payload families, and arbitrary FLOAT tags
remain out of scope. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`0DF7F6FB0637D9151F336EA21A60F3891960D25A88DD56FCB00C00B57BBCFB5E`.

`META-002AR` adds
`pillow_c_exif_float_array_tag(exif, exif_size, tag, out_has_tag, out_values,
out_value_count, out_value_required)`. It follows the existing required-count
array protocol and copies host-order IEEE-754 `float` values into the caller's
buffer. Native TIFF parsing and EXIF serialization admit IFD0
`ProfileHueSatMapData1` 50938 only when the source is FLOAT/type-11 with
exactly six in-range values; float32 bit patterns remain intact across the
TIFF-to-EXIF route. The facade reads the ABI with `NumGet(..., "Float")` into
a separate read-only `FloatArrayTags` map. DNG map interpretation/allocation,
writeback, malformed payload families, and arbitrary FLOAT tags remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports are
`443/443`, zero difference; SHA-256 is
`885E91AA9C7A2665A2064DF55499F65C0B97D00841440D91797FEED45186729C`.

`META-002AQ` changes no exported signature. Existing
`pillow_c_exif_uint_array_tag` readback and TIFF EXIF serialization now also
cover IFD0 `ProfileHueSatMapDims` 50937 when the source entry is LONG/type-4,
has exactly three values, and its payload is fully in range. The facade only
adds 50937 to the established read-only required-count route. DNG profile-map
interpretation/allocation, FLOAT map data, writeback, malformed payload
families, and arbitrary LONG arrays remain out of scope. Release x64 builds
with zero warnings/errors; source/DLL exports remain `442/442`, zero
difference; SHA-256 is
`269AE21A0052C16CE1C34EC9CDAFE1FA5A31732B47421D2BE34F0F9028020ABA`.

`META-002AP` changes no exported signature. Existing
`pillow_c_exif_double_array_tag` readback and TIFF EXIF serialization now also
cover IFD0 `RPCCoefficientTag` 50844 when the source entry is DOUBLE/type-12,
has exactly 92 values, and its payload is fully in range. The facade only adds
50844 to the established read-only required-count route. RPC model
interpretation/evaluation, writeback, malformed payload families, and
arbitrary DOUBLE tags remain out of scope. Release x64 builds with zero
warnings/errors; source/DLL exports remain `442/442`, zero difference;
SHA-256 is
`F59E6E398D92A50D6815A3932531392A3994DBB4AFC117E7AA31D45C3C8E92D4`.

`META-002AO` changes no exported signature. Existing `pillow_c_exif_ascii_tag`
readback and TIFF EXIF serialization now also cover IFD0 `GDAL_METADATA`
42112 and `GDAL_NODATA` 42113 when each source entry is ASCII/type-2 and its
payload is fully in range. The native reader removes only each TIFF NUL
terminator; the facade adds both IDs to the established read-only string
route. XML parsing, nodata numeric interpretation, writeback, malformed
payload families, and arbitrary ASCII tags remain out of scope. Release x64
builds with zero warnings/errors; source/DLL exports remain `442/442`, zero
difference; SHA-256 is
`D5DBA342EB4F5AFE5AF7ABA04F1A55A22DEFC412F9CF0996D55FF8772AB1195B`.

`META-002AN` changes no exported signature. Existing
`pillow_c_exif_ushort_array_tag` readback and TIFF EXIF serialization now also
cover IFD0 `GeoKeyDirectoryTag` 34735 when the source entry is SHORT/type-3,
has exactly eight values, and its payload is fully in range. The facade only
adds 34735 to the established read-only required-count route. GeoKey
interpretation, referenced parameter resolution, writeback, malformed payload
families, and arbitrary SHORT arrays remain out of scope. Release x64 builds
with zero warnings/errors; source/DLL exports remain `442/442`, zero
difference; SHA-256 is
`DE431941CBB09198CB17730D4256E9CCEB75E3B9663E196468E9E4F6FE6BF771`.

`META-002AM` changes no exported signature. Existing `pillow_c_exif_ascii_tag`
readback and TIFF EXIF serialization now also cover IFD0
`GeoAsciiParamsTag` 34737 when the source entry is ASCII/type-2 and its payload
is fully in range. The native reader omits the TIFF NUL terminator while
preserving the trailing GeoTIFF `|`; the facade only adds 34737 to the
established read-only string route. GeoTIFF interpretation, delimiter-based
parameter lookup, writeback, malformed payload families, and arbitrary ASCII
tags remain out of scope. Release x64 builds with zero warnings/errors;
source/DLL exports remain `442/442`, zero difference; SHA-256 is
`7242AEA0D0FC8A69A9BDEEB7F05E5C8E753F7FCFC415BACAEDAC1740B3BD4C91`.

`META-002AL` changes no exported signature. Existing
`pillow_c_exif_double_array_tag` readback and TIFF EXIF serialization now also
cover IFD0 `GeoDoubleParamsTag` 34736 when the source entry is DOUBLE/type-12,
has exactly three values, and its payload is fully in range. The facade only
adds 34736 to the established read-only required-count route. GeoTIFF
interpretation, coordinate transforms, writeback, malformed payload families,
and arbitrary floating-point tags remain out of scope. Release x64 builds with
zero warnings/errors; source/DLL exports remain `442/442`, zero difference;
SHA-256 is
`0C38092979CDEEC528AD94E7745761910AC6D561564B57E0A7B2598F0960F73B`.

`META-002AK` changes no exported signature. Existing
`pillow_c_exif_double_array_tag` readback and TIFF EXIF serialization now also
cover IFD0 `ModelTransformationTag` 34264 when the source entry is
DOUBLE/type-12, has exactly sixteen values, and its payload is fully in range.
The facade only adds 34264 to the established read-only required-count route.
GeoTIFF interpretation, coordinate transforms, writeback, malformed payload
families, and arbitrary DOUBLE tags remain out of scope. Release x64 builds
with zero warnings/errors; source/DLL exports remain `442/442`, zero
difference; SHA-256 is
`F1A3D3518671B4F57BF028E64660F973E890C3788FF01FD4674E2D46D161820C`.

`META-002AJ` changes no exported signature. Existing
`pillow_c_exif_double_array_tag` readback and TIFF EXIF serialization now also
cover IFD0 `ModelTiepointTag` 33922 when the source entry is DOUBLE/type-12,
has exactly six values, and its payload is fully in range. The facade only
adds 33922 to the established read-only required-count route. GeoTIFF
interpretation, coordinate transforms, writeback, malformed payload families,
and arbitrary DOUBLE tags remain out of scope. Release x64 builds with zero
warnings/errors; source/DLL exports remain `442/442`, zero difference;
SHA-256 is
`C506D9BA6465843EE44DF934E48286BC3BF1FF35A15F6EC3FDEA5260D9285D86`.

`META-002AI` adds one export:

```cpp
int pillow_c_exif_double_array_tag(
    const uint8_t* exif,
    size_t exif_size,
    int tag,
    int* out_has_tag,
    double* out_values,
    size_t out_value_count,
    size_t* out_value_required);
```

The export reads TIFF DOUBLE/type-12 arrays from a Pillow-style `Exif\0\0`
payload. It follows the established required-count contract: a present tag
sets `out_has_tag=1` and `out_value_required` before returning `-1` when
`out_values` is null; a sufficiently sized caller buffer receives host-order
IEEE-754 doubles; a short buffer returns `-2`; an absent or differently typed
tag returns success with `out_has_tag=0` and required count zero. The
`META-002AI` bounded TIFF route serializes IFD0 `ModelPixelScaleTag` 33550 only
when the source entry is DOUBLE/count 3 and fully in range. GeoTIFF interpretation,
writeback, malformed payload families, and arbitrary DOUBLE tags remain out of
scope. Release x64 builds with zero warnings/errors; source/DLL exports are
`442/442`, zero difference; SHA-256 is
`648CB69FC634E4C33BB86698C674AF555C32072BDCEEC144707AC95B7212C8C8`.

`META-002AH` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0 `PhotoshopInfo`
tag 34377 when it is a bounded TIFF BYTE/type-1 array. The established
`pillow_c_exif_byte_array_tag` required-count ABI returns exact bytes; the
facade maps them only into `GetExif()` / `getexif()` and does not synthesize
`Info["photoshop"]` or `Info["iptc"]`. 8BIM resource interpretation,
save/writeback, malformed payload families, and arbitrary TIFF tags remain out
of scope. Release x64 builds with zero warnings/errors; source/DLL exports
remain `441/441`, zero difference; SHA-256 is
`0374A7AE74A4227D4041D5A08A4BB96F711B931F5C3A5505013F2E2E54720202`.

`META-002AG` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0 `IptcNaaInfo` tag
33723 when it is a bounded TIFF UNDEFINED/type-7 entry. The established
`pillow_c_exif_undefined_tag` required-count ABI returns exact bytes; the
facade maps them only into `GetExif()` / `getexif()` and does not synthesize
an `Info["iptc"]` key. IPTC record interpretation, save/writeback, JPEG APP13,
and arbitrary TIFF tags remain out of scope. Release x64 builds with zero
warnings/errors; source/DLL exports remain `441/441`, zero difference;
SHA-256 is
`AF347A62BB8B3B15DDA76AB7946D50205652E20C41F1AFD72896003086183D19`.

`META-001FD` changes no exported signature. The native L-strip recognizer now
treats IFD0 `TileOffsets` 324 and `TileByteCounts` 325 as non-pixel-source
metadata when both are TIFF LONG/type-4 arrays, both referenced payloads are
in range, their counts are equal, and that count is greater than one. Existing
`pillow_c_image_metadata_tiff_exif` serialization and
`pillow_c_exif_uint_array_tag` required-count readback preserve the exact
count. Count-1 remains scalar; actual tiled pixel decoding remains separate.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`441/441`, zero difference; SHA-256 is
`ED14677939A8C54A58A724D99EBABE5CC5C6ECF2F55F7A43A6A2EF62B585E235`.

`META-001FC` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now preserve IFD0 `StripOffsets`
273 and `StripByteCounts` 279 as TIFF LONG/type-4 arrays for every valid entry
count greater than one. The established `pillow_c_exif_uint_array_tag` ABI
returns the native reader's exact required count; count-1 remains available
only through `pillow_c_exif_uint_tag`. The 182-byte six-strip fixture proves
the first formerly rejected count while counts 1 through 5 remain regression
covered. Compression, malformed strip ranges, writeback, and arbitrary tags
remain out of scope. Release x64 builds with zero warnings/errors; source/DLL
exports remain `441/441`, zero difference; SHA-256 is
`1F11AE2A5BB584B73A93C9798FDA1A09D05C0CCC0076EB8F666E8E6BA4E43DF5`.

`META-001FB` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now preserve IFD0 `StripOffsets`
273 and `StripByteCounts` 279 when each is a TIFF LONG/type-4 count-5 array in
the bounded valid five-strip L fixture. The established
`pillow_c_exif_uint_array_tag` ABI returns all five unsigned values while
count-1/count-2/count-3/count-4 behavior remains unchanged. Other counts,
compression, malformed strip ranges, writeback, and arbitrary tags remain out
of scope. Release x64 builds with zero warnings/errors; source/DLL exports
remain `441/441`, zero difference; SHA-256 is
`A7BBA1529EB2D2D8D26097A631D8BD306D44CC8A3EB9A201B0DF2BE3500B1C72`.

`META-001FA` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now preserve IFD0 `StripOffsets`
273 and `StripByteCounts` 279 when each is a TIFF LONG/type-4 count-4 array in
the bounded valid four-strip L fixture. The established
`pillow_c_exif_uint_array_tag` ABI returns all four unsigned values while
count-1/count-2/count-3 behavior remains unchanged. Other counts,
compression, malformed strip ranges, writeback, and arbitrary tags remain out
of scope. Release x64 builds with zero warnings/errors; source/DLL exports
remain `441/441`, zero difference; SHA-256 is
`6DE3FE177B939B06D49233D069F21908337E5E4107667B8656B9B1D2BA7F7703`.

`META-001EZ` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now preserve IFD0 `StripOffsets`
273 and `StripByteCounts` 279 when each is a TIFF LONG/type-4 count-3 array in
the bounded valid three-strip L fixture. The established
`pillow_c_exif_uint_array_tag` ABI returns all three unsigned values while
count-1 and count-2 behavior remains unchanged. Other counts, compression,
malformed strip ranges, writeback, and arbitrary tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`441/441`, zero difference; SHA-256 is
`9B47AA8572DBF44332A56B12732EE1FBAC35F7CE546D033ED4B3C6D515EB5EC9`.

`META-001EY` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now preserve IFD0 `StripOffsets`
273 and `StripByteCounts` 279 when each is a TIFF LONG/type-4 count-2 array in
the bounded valid two-strip L fixture. The established
`pillow_c_exif_uint_array_tag` required-count ABI returns both arrays;
count-1 entries remain scalar through `pillow_c_exif_uint_tag`, and the facade
only materializes the array form when more than one value is present. Other
counts, compression, malformed strip ranges, writeback, and arbitrary tags
remain out of scope. Release x64 builds with zero warnings/errors; source/DLL
exports remain `441/441`, zero difference; SHA-256 is
`71C9903631C8D7A3697156CFAB4F296D6E2389DC2A013656AE7219CCE91DEDCD`.

`META-001EX` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now preserve IFD0 `TileOffsets` 324
and `TileByteCounts` 325 when both are TIFF LONG/type-4 count-2 arrays in the
bounded strip-based L fixture. The established
`pillow_c_exif_uint_array_tag` required-count ABI returns both unsigned
arrays; count-1 entries remain available through `pillow_c_exif_uint_tag`.
The native L-strip open route recognizes the bounded metadata pair so WIC does
not reject it as actual tiled storage, but pixel bytes continue to come from
the valid StripOffsets/StripByteCounts entries. Actual tiled decoding,
writeback, other counts, and arbitrary tags remain out of scope. Release x64
builds with zero warnings/errors; source/DLL exports remain `441/441`, zero
difference; SHA-256 is
`B1A6B7078923BA0C3727F955B1B591797420ACA98A7E2B51D4318992F14A2C32`.

`META-001EW` adds one export:

```cpp
int pillow_c_exif_uint_array_tag(
    const uint8_t* exif,
    size_t exif_size,
    int tag,
    int* out_has_tag,
    uint32_t* out_values,
    size_t out_value_count,
    size_t* out_value_required);
```

The export reads TIFF LONG/type-4 arrays from a Pillow-style `Exif\0\0`
payload. It follows the established required-count contract: a present tag
sets `out_has_tag=1` and `out_value_required` before returning `-1` when
`out_values` is null; a sufficiently sized caller buffer receives host-order
unsigned 32-bit values; a short buffer returns `-2`; an absent or differently
typed tag returns success with `out_has_tag=0` and required count zero. The
bounded TIFF open route uses it for IFD0 `MaskSubArea` 52536 only when the
source entry is LONG/count 4 and fully in range. Public LONG-array writeback,
mask interpretation, other counts, and arbitrary tags remain out of scope.
Release x64 builds with zero warnings/errors; source/DLL exports are
`441/441`, zero difference; SHA-256 is
`57EAEE953EA7D1B3270FF60B83B2595E6A36E9EF3FFEEDA4B77592A01FF6DA4F`.

`META-001EV` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0 `IlluminantData1`
52533, `IlluminantData2` 52534, and `IlluminantData3` 52535 when each is a
bounded TIFF UNDEFINED type-7 entry. The established
`pillow_c_exif_undefined_tag` required-count ABI returns exact inline or
out-of-line bytes. Illuminant interpretation and public write-side type
inference remain unchanged. Release x64 builds with zero warnings/errors;
source/DLL exports remain `440/440`, zero difference; SHA-256 is
`999AC5A31CE4C771598CA143B4CF4FAA6D4EDEA495628C8EE859F3511683B322`.

`META-001EU` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0
`CameraCalibration3` 52530, `ColorMatrix3` 52531, and `ForwardMatrix3` 52532
only when each is TIFF SRATIONAL type `10`, count `9`, fully in range, and has
nine nonzero signed denominators. The established
`pillow_c_exif_signed_rational_array_tag` required-count ABI returns their
parallel signed 32-bit numerator/denominator arrays. Matrix interpretation and
public write-side type inference remain unchanged. Release x64 builds with
zero warnings/errors; source/DLL exports remain `440/440`, zero difference;
SHA-256 is
`C3DE15D28FF950967039DD6AF1A3F28C4C74AF729D9AE7BF2DC9DB00F76BB070`.

`META-001ET` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0
`CalibrationIlluminant3` 52529 when it is a scalar TIFF integer entry; the
covered fixture uses SHORT type `3`, count `1`, value `23`. The established
`pillow_c_exif_uint_tag` ABI returns the unsigned scalar. Illuminant
interpretation and public write-side type inference remain unchanged. Release
x64 builds with zero warnings/errors; source/DLL exports remain `440/440`,
zero difference; SHA-256 is
`AC8F2763A8CB0BB0BD194AADBC47B6CAD765724650AC3BB3BFEF5B713209371D`.

`META-001ES` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0 `SemanticName`
52526 and `SemanticInstanceID` 52528 when they are bounded, NUL-terminated
TIFF ASCII entries. The established `pillow_c_exif_ascii_tag` ABI returns the
exact strings. Semantic-mask interpretation and public write-side type
inference remain unchanged. Release x64 builds with zero warnings/errors;
source/DLL exports remain `440/440`, zero difference; SHA-256 is
`5EF9D7E038B52B2E44318943A7185D517468AF99883EF119133B3C014653BE65`.

`META-001ER` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0 `EnhanceParams`
51182 when it is a bounded TIFF ASCII entry with a terminating NUL; the
covered type-2/count-7 payload returns `gain=1` through the established
`pillow_c_exif_ascii_tag` ABI. Enhancement interpretation and public
write-side type inference remain unchanged. Release x64 builds with zero
warnings/errors; source/DLL exports remain `440/440`, zero difference;
SHA-256 is
`0324036328D67B153503661DDB77AB0742E74E63AF7FF582C367DB9FEE13F3CE`.

`META-001EQ` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0 `DepthUnits` 51180
and `DepthMeasureType` 51181 when they are scalar TIFF integer entries; the
covered fixture uses SHORT type `3`, count `1`, values `1` / `2`. The
established `pillow_c_exif_uint_tag` ABI returns each unsigned scalar. Depth
interpretation and public write-side type inference remain unchanged. Release
x64 builds with zero warnings/errors; source/DLL exports remain `440/440`,
zero difference; SHA-256 is
`B7D3D97DB7D2589CB4DD8D6FAE9BA27081961A1C0C1F52381229AE12D31603D7`.

`META-001EP` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0 `DepthNear` 51178
and `DepthFar` 51179 when they are scalar TIFF RATIONAL entries with in-range
payloads and nonzero denominators. The established
`pillow_c_exif_rational_tag` ABI returns exact unsigned pairs. Depth
interpretation and public write-side type inference remain unchanged. Release
x64 builds with zero warnings/errors; source/DLL exports remain `440/440`,
zero difference; SHA-256 is
`A8B6B7AEB648214EEA06DD3F8C96BD640440D31EF917679237DD2C2EF831742C`.

`META-001EO` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0 `DepthFormat`
51177 when it is a scalar TIFF integer entry; the covered fixture uses SHORT
type `3`, count `1`, value `1`. The established `pillow_c_exif_uint_tag` ABI
returns the unsigned scalar. Depth interpretation and public write-side type
inference remain unchanged. Release x64 builds with zero warnings/errors;
source/DLL exports remain `440/440`, zero difference; SHA-256 is
`31DBF42A0908ABED726E6093D75BF73E732237D911064158FD1B7E11414124D9`.

`META-001EN` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0 `DefaultUserCrop`
51125 only when it is TIFF RATIONAL type `5`, count `4`, fully in range, and
has four nonzero denominators. The established
`pillow_c_exif_rational_array_tag` required-count ABI returns parallel
unsigned 32-bit numerator/denominator arrays. Public write-side type inference
remains unchanged. Release x64 builds with zero warnings/errors; source/DLL
exports remain `440/440`, zero difference; SHA-256 is
`4B32B99FDF9526E607E6C7C0F8159DCC35AF8415A5065EA4FAFA99AB1705F17D`.

`META-001EM` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0 `RawToPreviewGain`
51112 when it is a scalar RATIONAL entry with a valid payload range and
nonzero denominator. The established `pillow_c_exif_rational_tag` export
returns its exact unsigned pair. Public write-side type inference remains
unchanged. Release x64 builds with zero warnings/errors; source/DLL exports
remain `440/440`, zero difference; SHA-256 is
`357F8B89DA7910145A0A204CA363BDB35264C9FCEB158F191716E1293D69E2DF`.

`META-001EL` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0
`NewRawImageDigest` 51111 when it is a TIFF BYTE-array entry admitted by the
common byte-array route. The established `pillow_c_exif_byte_array_tag`
required-count ABI returns its exact bytes. Public write-side type inference
remains unchanged. Release x64 builds with zero warnings/errors; source/DLL
exports remain `440/440`, zero difference; SHA-256 is
`456BBA568BBFD5326BAA0F6BE46055BA78B5093E6BBDB988CE84BE6E94959933`.

`META-001EK` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0
`DefaultBlackRender` 51110 when it is a scalar integer entry admitted by the
common TIFF uint route. The established `pillow_c_exif_uint_tag` export
returns its exact unsigned value. Public write-side type inference remains
unchanged. Release x64 builds with zero warnings/errors; source/DLL exports
remain `440/440`, zero difference; SHA-256 is
`5801FD3430FAC67E031D3D98861D2A82AD1D42B7C27CBED3077411FF4E8BCA7B`.

`META-001EJ` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0
`BaselineExposureOffset` 51109 when it is a scalar SRATIONAL entry with a
valid payload range and nonzero denominator. The established
`pillow_c_exif_signed_rational_tag` export returns its exact signed pair.
Public write-side type inference remains unchanged. Release x64 builds with
zero warnings/errors; source/DLL exports remain `440/440`, zero difference;
SHA-256 is
`733883A4A6BE43B9042A0F1782D73E66DBD32C090F40BAA62A207BFAD8F6F460`.

`META-001EI` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0
`ProfileHueSatMapEncoding` 51107 and `ProfileLookTableEncoding` 51108 when
each is a scalar integer entry admitted by the common TIFF uint route. The
established `pillow_c_exif_uint_tag` export returns their exact unsigned
values. Public write-side type inference remains unchanged. Release x64
builds with zero warnings/errors; source/DLL exports remain `440/440`, zero
difference; SHA-256 is
`CD59FE9346A70B4468C472514AC8278ADC98EDF8C5F1CB966D37F21A67C761B1`.

`META-001EH` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs now include IFD0
`ProfileEmbedPolicy` 50941 and `PreviewColorSpace` 50970 when each is a scalar
integer entry admitted by the common TIFF uint route. The established
`pillow_c_exif_uint_tag` export returns their exact unsigned values. Public
write-side type inference remains unchanged. Release x64 builds with zero
warnings/errors; source/DLL exports remain `440/440`, zero difference;
SHA-256 is
`3923CFEC64FABB5823DC6CBABDAA3A611DD97C667D84BEEBE26C8170AEE23BC5`.

`META-001EG` changes no exported signature. The existing
`pillow_c_image_metadata_tiff_exif` route now includes IFD0 `ForwardMatrix1`
50964 and `ForwardMatrix2` 50965 only when each is TIFF SRATIONAL type `10`,
count `9`, fully in range, and has nine nonzero signed denominators. The
existing `pillow_c_exif_signed_rational_array_tag` required-count ABI returns
their parallel signed 32-bit numerator/denominator arrays. Public write-side
type inference remains unchanged. Release x64 builds with zero warnings/
errors; source/DLL exports remain `440/440`, zero difference; SHA-256 is
`0F84990C1AD7D85024D4437205D701F02820C5C32EAA329099BE2845180E302A`.

`META-001EF` changes no exported signature. The existing
`pillow_c_image_metadata_tiff_exif` route now includes IFD0
`CameraCalibration1/2` 50723/50724 and `ReductionMatrix1/2` 50725/50726 only
when each is TIFF SRATIONAL type `10`, count `9`, fully in range, and has nine
nonzero signed denominators. The existing
`pillow_c_exif_signed_rational_array_tag` required-count ABI returns their
parallel signed 32-bit numerator/denominator arrays. Public write-side type
inference remains unchanged. Release x64 builds with zero warnings/errors;
source/DLL exports remain `440/440`, zero difference; SHA-256 is
`4CC2EFACC00C3F718C1081E797E99D131252DF720DC9A198086023A2F2A6BC81`.

`META-001EE` adds `pillow_c_exif_signed_rational_array_tag`. The existing
`pillow_c_image_metadata_tiff_exif` route now includes IFD0 `ColorMatrix1`
50721 and `ColorMatrix2` 50722 only when each is TIFF SRATIONAL type `10`,
count `9`, fully in range, and has nine nonzero signed denominators. Internal
EXIF serialization now supports type-10 arrays, while public write-side type
inference remains unchanged. The new readback export returns parallel signed
32-bit numerator/denominator arrays through the required-count ABI described
below. Release x64 builds with zero warnings/errors; source/DLL exports are
`440/440`, zero difference; SHA-256 is
`FC352ACA7327DD77B85F9CB48743248C6BC0058746E0EE018B5DE6B6647E7639`.

`META-001ED` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `AsShotWhiteXY` tag
`50729=[[3127/10000],[3290/10000]]` when it is a RATIONAL count-2 entry and
`LensInfo` tag `50736=[[24/1],[70/1],[28/10],[4/1]]` when it is a RATIONAL
count-4 entry. Values use the existing rational-array EXIF serialization route
without DNG color/lens interpretation. Public write-side type inference
remains unchanged; absent, malformed, wrong-type, wrong-count, out-of-range,
or zero-denominator entries remain absent. Release x64 builds with zero
warnings/errors; source/DLL exports remain `439/439`, zero difference;
SHA-256 is
`2A57BFD683396184AA89F2E35B54DE33DBC468911CA3DB4C235D3401386D3F12`.

`META-001EC` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `AnalogBalance` tag `50727=[[2/1],[1/1],[3/2]]` and
`AsShotNeutral` tag `50728=[[1/2],[1/1],[2/3]]` when each is a RATIONAL array
entry (TIFF type `5`, count `3`) with a valid payload range and nonzero
denominators. Values use the existing rational-array EXIF serialization route
without DNG color interpretation. Public write-side type inference remains
unchanged; absent, malformed, wrong-type, wrong-count, out-of-range, or zero-
denominator entries remain absent. Release x64 builds with zero warnings/
errors; source/DLL exports remain `439/439`, zero difference; SHA-256 is
`76A70C52BAFF46A86B849AA1A2286AE69BA07CCEC122E093B9203D2841671481`.

`META-001EB` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `BaselineExposure` tag `50730=-3/2` and `ShadowScale`
tag `50739=5/4` when each is a scalar SRATIONAL entry (TIFF type `10`, count
`1`) with a valid payload range and nonzero denominator. Values use the
existing signed-rational EXIF serialization route without DNG interpretation.
Public write-side type inference remains unchanged; absent, malformed, wrong-
type, non-scalar, out-of-range, or zero-denominator entries remain absent.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`439/439`, zero difference; SHA-256 is
`448BDF085FCF0A3AB436FE74F5176C206E48C9AF53246AB8F5953AE1B0FDDC7B`.

`META-001EA` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `AntiAliasStrength` tag `50738=4/5` and
`BestQualityScale` tag `50780=9/8` when each is a scalar RATIONAL entry (TIFF
type `5`, count `1`) with a valid payload range and nonzero denominator.
Values use the existing unsigned-rational EXIF serialization route without DNG
interpretation. Public write-side type inference remains unchanged; absent,
malformed, wrong-type, non-scalar, out-of-range, or zero-denominator entries
remain absent. Release x64 builds with zero warnings/errors; source/DLL
exports remain `439/439`, zero difference; SHA-256 is
`787511F3B8F4E4CD655FB82FF76F5A54336473AEDD670DFF047B7BBF523BDC42`.

`META-001DZ` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `LinearResponseLimit` tag `50734=3/4` and
`ChromaBlurRadius` tag `50737=7/3` when each is a scalar RATIONAL entry (TIFF
type `5`, count `1`) with a valid payload range and nonzero denominator.
Values use the existing unsigned-rational EXIF serialization route without DNG
interpretation. Public write-side type inference remains unchanged; absent,
malformed, wrong-type, non-scalar, out-of-range, or zero-denominator entries
remain absent. Release x64 builds with zero warnings/errors; source/DLL
exports remain `439/439`, zero difference; SHA-256 is
`D63D9B718A5FB91E97BD37DF061FC3DA31536A2434806A026B59A9D5E6D16FBB`.

`META-001DY` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `BaselineNoise` tag `50731=3/2` and
`BaselineSharpness` tag `50732=5/4` when each is a scalar RATIONAL entry (TIFF
type `5`, count `1`) with a valid payload range and nonzero denominator.
Values use the existing unsigned-rational EXIF serialization route without DNG
interpretation. Public write-side type inference remains unchanged; absent,
malformed, wrong-type, non-scalar, out-of-range, or zero-denominator entries
remain absent. Release x64 builds with zero warnings/errors; source/DLL
exports remain `439/439`, zero difference; SHA-256 is
`9B8F454E583ABE7912E5215BD5712457AD94D3526705B62176961A5C6692980B`.

`META-001DX` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `CalibrationIlluminant1` tag `50778=17` and
`CalibrationIlluminant2` tag `50779=21` when each is a scalar SHORT entry
(TIFF type `3`, count `1`). Values use the existing unsigned-integer EXIF
serialization route without DNG calibration interpretation. Public write-side
type inference remains unchanged; absent, malformed, wrong-type, or non-scalar
entries remain absent. Release x64 builds with zero warnings/errors;
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`AD1AB30FD1A6855ECD9868EB75724A017CE644553F39E3437C96559917BFF808`.

`META-001DW` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `CFALayout` tag `50711=2` and `MakerNoteSafety` tag
`50741=1` when each is a scalar SHORT entry (TIFF type `3`, count `1`). Values
use the existing unsigned-integer EXIF serialization route without DNG
interpretation. Public write-side type inference remains unchanged; absent,
malformed, wrong-type, or non-scalar entries remain absent. Release x64 builds
with zero warnings/errors; source/DLL exports remain `439/439`, zero
difference; SHA-256 is
`FDF150A2D011F812CA29AE1EA3B31BA18E7CB00F875FE517EA3CD2C103303466`.

`META-001DV` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `CameraSerialNumber` tag `50735` and `ProfileCopyright`
tag `50942` when each has TIFF ASCII type `2`, a positive count, and a valid
inline or out-of-line payload range. Values are preserved up to the first NUL
or declared count (`SN1` / `copyright` in the covered fixture) without DNG
interpretation. Public write-side type inference remains unchanged; absent,
malformed, wrong-type, zero-count, or out-of-range TIFF entries remain absent.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`439/439`, zero difference; SHA-256 is
`FE32ACE5FC6C4719E0B3CAE88C57A44C0672E5665AF816AA45683F144B51B286`.

`META-001DU` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `PreviewSettingsName` tag `50968` and `PreviewDateTime`
tag `50971` when each has TIFF ASCII type `2`, a positive count, and a valid
inline or out-of-line payload range. Values are preserved up to the first NUL
or declared count (`SET` / `2026:08:05 12:34:56` in the covered fixture)
without DNG interpretation. Public write-side type inference remains
unchanged; absent, malformed, wrong-type, zero-count, or out-of-range TIFF
entries remain absent. Release x64 builds with zero warnings/errors;
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`14FF2E76A893DD74CBAA4CEA82802BC3DF79CCC7807AA8F8D432773C49ECD448`.

`META-001DT` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `PreviewApplicationName` tag `50966` and
`PreviewApplicationVersion` tag `50967` when each has TIFF ASCII type `2`, a
positive count, and a valid inline or out-of-line payload range. Values are
preserved up to the first NUL or declared count (`APP` / `1.2.3` in the
covered fixture) without DNG interpretation. Public write-side type inference
remains unchanged; absent, malformed, wrong-type, zero-count, or out-of-range
TIFF entries remain absent. Release x64 builds with zero warnings/errors;
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`8EF7C717F11C6A560E79E0226A6BE08ABEAFE1FDE24E4A0B28E0B781EF325CFB`.

`META-001DS` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `AsShotProfileName` tag `50934` and `ProfileName` tag
`50936` when each has TIFF ASCII type `2`, a positive count, and a valid inline
or out-of-line payload range. Values are preserved up to the first NUL or
declared count (`ASP` / `profile` in the covered fixture) without DNG
interpretation. Public write-side type inference remains unchanged; absent,
malformed, wrong-type, zero-count, or out-of-range TIFF entries remain absent.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`439/439`, zero difference; SHA-256 is
`DC1F10F9AF4885B77BCC96747594B1306AB31F59C0741BC1404BEE3683412F4D`.

`META-001DR` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `CameraCalibrationSignature` tag `50931` and
`ProfileCalibrationSignature` tag `50932` when each has TIFF ASCII type `2`,
a positive count, and a valid inline or out-of-line payload range. Values are
preserved up to the first NUL or declared count (`CAL` / `profile` in the
covered fixture) without DNG interpretation. Public write-side type inference
remains unchanged; absent, malformed, wrong-type, zero-count, or out-of-range
TIFF entries remain absent. Release x64 builds with zero warnings/errors;
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`AB900BB1D9AD154313C68FCAF557F194E7EBA0200543D56D997DD0DAB6090CE9`.

`META-001DQ` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `UniqueCameraModel` tag `50708` and
`OriginalRawFileName` tag `50827` when each has TIFF ASCII type `2`, a positive
count, and a valid inline or out-of-line payload range. Values are preserved
up to the first NUL or declared count (`CAM` / `raw.dng` in the covered
fixture) without DNG interpretation. Public write-side type inference remains
unchanged; absent, malformed, wrong-type, zero-count, or out-of-range TIFF
entries remain absent. Release x64 builds with zero warnings/errors;
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`A3623563D62A1AC7B6A647CCA6D075AC95191928396EF86B9793B28A577F6FF3`.

`META-001DP` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `LocalizedCameraModel` tag `50709` and `CFAPlaneColor`
tag `50710` when each has TIFF BYTE type `1`, a positive count, and a valid
inline or out-of-line payload range. Exact bytes `[67,65,77,0,255]` and
`[0,1,2]` are preserved without DNG interpretation. Public write-side type
inference remains unchanged; absent, malformed, wrong-type, zero-count, or
out-of-range TIFF entries remain absent. Release x64 builds with zero
warnings/errors; source/DLL exports remain `439/439`, zero difference;
SHA-256 is
`654C1AEADF64FF64526EF041B980D0108BEB9855CC9543A727AE56BFBA34E39A`.

`META-001DO` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `DNGVersion` tag `50706` and `DNGBackwardVersion` tag
`50707` when each has TIFF BYTE type `1`, a positive count, and a valid inline
or out-of-line payload range. The covered four-byte values `[1,6,0,0]` and
`[1,4,0,0]` are preserved exactly without DNG interpretation. Public
write-side type inference remains unchanged; absent, malformed, wrong-type,
zero-count, or out-of-range TIFF entries remain absent. Release x64 builds
with zero warnings/errors; source/DLL exports remain `439/439`, zero
difference; SHA-256 is
`2F3228B64ABA7AF1E861F7830CDE98B704B78A9B0050B8F878D2DF09741DA468`.

`META-001DN` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `UserComment` tag `37510` and `ImageSourceData` tag
`37724` when each has TIFF UNDEFINED type `7`, a positive count, and a valid
inline or out-of-line payload range. Exact bytes `[65,66,0,255]` and
`[73,83,68,0,255]` are preserved without payload interpretation. Type-1
UserComment write-side inference remains unchanged; absent, malformed,
wrong-type, zero-count, or out-of-range TIFF entries remain absent. Release
x64 builds with zero warnings/errors; source/DLL exports remain `439/439`,
zero difference; SHA-256 is
`13231A1195E4B5EEDB3E0690EC3853FB08D6303A50DFF7D57EDE3ED835E1FD76`.

`META-001DM` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `CFAPattern` tag `41730` and
`DeviceSettingDescription` tag `41995` when each has TIFF UNDEFINED type `7`,
a positive count, and a valid inline or out-of-line payload range. Exact bytes
`[2,2,0,1]` and `[68,69,86,0,255]` are preserved without payload
interpretation; absent, malformed, wrong-type, zero-count, or out-of-range
entries remain absent. Release x64 builds with zero warnings/errors;
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`83F36D5C7E535214A7D6C08D3A121C6CCE461FB05E24222B56CE9490B8297FB2`.

`META-001DL` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `ComponentsConfiguration` tag `37121` and `MakerNote`
tag `37500` when each has TIFF UNDEFINED type `7`, a positive count, and a
valid inline or out-of-line payload range. Exact bytes `[1,2,3,0]` and
`[77,75,0,255,1,2]` are preserved without MakerNote interpretation; absent,
malformed, wrong-type, zero-count, or out-of-range entries remain absent.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`439/439`, zero difference; SHA-256 is
`076456D7BE3BCC45989C6B1536C7E774A0A41048DE2335BAE8C63480564E15E5`.

`META-001DK` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `OECF` tag `34856` and `SpatialFrequencyResponse` tag
`41484` when each has TIFF UNDEFINED type `7`, a positive count, and a valid
inline or out-of-line payload range. Exact bytes `[1,0,2,255,3]` and `[9,0,8]`
are preserved; absent, malformed, wrong-type, zero-count, or out-of-range
entries remain absent. Release x64 builds with zero warnings/errors;
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`EC0733C628CD70DBE9F0CAEB4E184B0A212687E5FE74870DB24CD6CCB2D13D17`.

`META-001DJ` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `StandardOutputSensitivity` 34865,
`RecommendedExposureIndex` 34866, `ISOSpeed` 34867,
`ISOSpeedLatitudeyyy` 34868, and `ISOSpeedLatitudezzz` 34869 when each has TIFF
LONG type `4`, count `1`, and a valid inline value. Exact unsigned values
`100001` / `200002` / `300003` / `400004` / `500005` are preserved; absent,
malformed, wrong-type, or wrong-count entries remain absent. Release x64
builds with zero warnings/errors; source/DLL exports remain `439/439`, zero
difference; SHA-256 is
`F3915887A3CC9C8E8E273FE0FF90A7BF7ED26B905221A6898183DF6FCA3B5926`.

`META-001DI` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `ISOSpeedRatings` / `PhotographicSensitivity` tag
`34855` and `SensitivityType` tag `34864` when each has TIFF SHORT type `3`,
count `1`, and a valid inline value. The unsigned values are preserved exactly
(`400` / `3` in the covered fixture); absent, malformed, wrong-type, or
wrong-count entries remain absent. Release x64 builds with zero warnings/
errors; source/DLL exports remain `439/439`, zero difference; SHA-256 is
`51122D7BA4D7729761A67DA01661C902E201A7A8201B4364B95017B0A30DBF0A`.

`META-001DH` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `SubjectArea` tag `37396` for all standard TIFF SHORT
counts `2`, `3`, and `4` when the inline or out-of-line value range is valid.
The count-2 `[7,9]` and count-3 `[7,9,11]` fixtures complement the previously
covered count-4 shape; absent, malformed, wrong-type, or other-count entries
remain absent. Release x64 builds with zero warnings/errors; source/DLL
exports remain `439/439`, zero difference; SHA-256 is
`92B809C293235D145C7A4661FE9F522F7114D27BA9F0931399689871D9547FD1`.

`META-001DG` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `SubjectArea` tag `37396` when it has TIFF SHORT type
`3`, count `4`, and a valid out-of-line value range. The four unsigned 16-bit
values are preserved exactly (`[7,9,11,13]` in the covered fixture); absent,
malformed, wrong-type, or wrong-count entries remain absent. Release x64
builds with zero warnings/errors; source/DLL exports remain `439/439`, zero
difference; SHA-256 is
`AE4857AB6077BD0A497A38BF610992EFF6F55C73D9704907CC3DDE7841A040CA`.

`META-001DF` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `SecurityClassification` tag `37394` and `ImageHistory`
tag `37395` when each has TIFF ASCII type `2`, a positive count, and an
in-range payload. Each string is preserved up to the first NUL or its declared
count (`secret` / `edited` in the covered fixture); absent, malformed,
wrong-type, or invalid-range entries remain absent. Release x64 builds with
zero warnings/errors; source/DLL exports remain `439/439`, zero difference;
SHA-256 is
`E4F413F6B281C89446867AD175106DAA10E943BECE955A594BD67727EEB7E6F0`.

`META-001DE` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `SpectralSensitivity` tag `34852` when it has TIFF ASCII
type `2`, a positive count, and an in-range payload. The string is preserved
up to the first NUL or the declared count (`spec42` before the terminator in
the covered fixture); absent, malformed, wrong-type, or invalid-range entries
remain absent. Release
x64 builds with zero warnings/errors; source/DLL exports remain `439/439`,
zero difference; SHA-256 is
`26FAE27A742E16D04A222082183F1BDC82A94122F6D6D6875F2127827426E75B`.

`META-001DD` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `SubjectLocation` tag `41492` when it has TIFF SHORT type
`3`, count `2`, and a valid inline or out-of-line value range. The two unsigned
16-bit values are preserved exactly (`[7,9]` in the covered fixture); absent,
malformed, wrong-type, or wrong-count entries remain absent. Release x64
builds with zero warnings/errors; source/DLL exports remain `439/439`, zero
difference; SHA-256 is
`8BF87B038AD9BD815697DBA1D0D982FA7A438A23B662E74C527ABDE17247F52F`.

`META-001DC` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `CompressedBitsPerPixel` tag `37122` and `ExposureIndex`
tag `41493` when each has TIFF RATIONAL type `5`, count `1`, and a valid
nonzero denominator. Numerator/denominator pairs are preserved exactly
(`24/10` and `200/1` in the covered fixture); absent, malformed, wrong-type,
or wrong-count entries remain absent. Release x64 builds with zero warnings/
errors; source/DLL exports remain `439/439`, zero difference; SHA-256 is
`D8CBED6CF819880BD5026DEDD189764108E6C66063236E6F83E76695CEC6626B`.

`META-001DB` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `ExposureBiasValue` tag `37380` when it has TIFF
SRATIONAL type `10`, count `1`, and a valid nonzero signed denominator. The
signed numerator/denominator pair is preserved exactly (`-1/2` in the covered
fixture); absent, malformed, wrong-type, or wrong-count entries remain absent.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`439/439`, zero difference; SHA-256 is
`E5FB99F2E3763ABCEF004D9D20629763A9CB78411DD322DD396657CA9C2834BD`.

`META-001DA` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `ShutterSpeedValue` tag `37377` and `BrightnessValue`
tag `37379` when each has TIFF SRATIONAL type `10`, count `1`, and a valid
nonzero signed denominator. Signed numerator/denominator pairs are preserved
exactly (`-3/2` and `7/4` in the covered fixture); absent, malformed,
wrong-type, or wrong-count entries remain absent. Release x64 builds with zero
warnings/errors; source/DLL exports remain `439/439`, zero difference;
SHA-256 is
`EA91ABC0F5E4B96ACBC63661307B51C17EAD240785E12EAB390F912DB77D676A`.

`META-001CZ` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `SubjectDistance` tag `37382` and `FocalLength` tag
`37386` when each has TIFF RATIONAL type `5`, count `1`, and a valid nonzero
denominator. Numerator/denominator pairs are preserved exactly (`125/10` and
`50/1` in the covered fixture); absent, malformed, wrong-type, or wrong-count
entries remain absent. Release x64 builds with zero warnings/errors;
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`7478F8E56D39FF187E6FE1F916AD87DD45360E0BC60F440C90BA769907C59D21`.

`META-001CY` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `ApertureValue` tag `37378` and `MaxApertureValue` tag
`37381` when each has TIFF RATIONAL type `5`, count `1`, and a valid nonzero
denominator. Numerator/denominator pairs are preserved exactly (`28/10` and
`4/1` in the covered fixture); absent, malformed, wrong-type, or wrong-count
entries remain absent. Release x64 builds with zero warnings/errors;
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`D59D2A116DDAAA21B90B24468BC0EA94D23D1D922959420BE5DC09A44D0CF34B`.

`META-001CX` changes no exported signature. Existing
`pillow_c_image_metadata_tiff_exif` blobs produced while opening bounded TIFF
IFD0 data now include `ExposureTime` tag `33434` and `FNumber` tag `33437`
when each has TIFF RATIONAL type `5`, count `1`, and a valid nonzero
denominator. Numerator/denominator pairs are preserved exactly (`1/125` and
`14/5` in the covered fixture); absent, malformed, wrong-type, or wrong-count
entries remain absent from the serialized blob. `Info["exif"]` remains a facade
metadata rule and is not synthesized by this ABI. Release x64 builds with zero
warnings/errors; source/DLL exports remain `439/439`, zero difference;
SHA-256 is
`59C182E4C7353D41113CECF25755DB07399431B1B5CEEF56ACC6FE2EACAB9B1F`.

`META-003EN` adds:

```text
pillow_c_cms_profile_intent_support(
    profile,
    out_values,
    value_count)
```

The profile and output array are required; `value_count` must be `12`.
Values are intent-major for intents `0..3`, with each three-value row ordered
input, output, proof. Every slot is native boolean `0` or `1` from
`cmsIsIntentSupported`. The existing scalar
`pillow_c_cms_profile_intent_supported` remains the instance-method route and
retains bounded intent/direction validation. Release x64 builds with zero
warnings/errors; source/DLL exports are `439/439`, zero difference; SHA-256 is
`A9FB9E89C4E684B6E3532D6863621961F0C7A5B95C2003F0C849B76AA459FDED`.

`META-003EM` adds:

```text
pillow_c_cms_profile_clut(
    profile,
    out_values,
    value_count)
```

The profile and output array are required; `value_count` must be `12`.
Values are intent-major for intents `0..3`, with each three-value row ordered
input, output, proof. Every slot is native boolean `0` or `1` from
`cmsIsCLUT`. Release x64 builds with zero warnings/errors; source/DLL exports
are `438/438`, zero difference; SHA-256 is
`4973418EFF0114EBF9C9E820472B4F80377FF3885622558EDC32E09E4806D42C`.

`META-003EL` adds:

```text
pillow_c_cms_profile_colorant_tables(
    profile,
    out_present,
    present_count,
    out_counts,
    count_count,
    out_table,
    table_size,
    out_table_required,
    out_table_out,
    table_out_size,
    out_table_out_required)
```

The profile, two-slot presence/count arrays, and both required-size pointers
are required; both counts must be `2`. Slots are input colorant table then
output colorant table. Output buffers may be null only with size zero for the
query phase. A present name occupies one zero-padded `cmsMAX_PATH` (`256`)
byte slot, so each required size is `count * 256`; a present empty table stays
distinguishable as `present=1`, `count=0`, `required=0`. Missing tags retain
independent zero presence/count/required slots. Undersized output returns
`-2`; a named-color read failure returns `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports were `437/437`, zero difference; SHA-256
was `3B44D76135E56D3E6C0A6B01809BD7C742A0C1B62168FF6A72D5B7B665DF6DDF`.

`META-003EK` adds:

```text
pillow_c_cms_profile_attributes_and_colorimetric_intent(
    profile,
    out_attributes,
    out_colorimetric_intent_present,
    out_colorimetric_intent)
```

All pointers are required. The 64-bit ICC header attributes are returned
directly. The optional `ciis` signature has an independent presence/value pair;
missing tags return zero slots without a synthesized signature. Release x64
builds with zero warnings/errors; source/DLL exports are `436/436`, zero
difference; SHA-256 is
`8F89C79B39791134190F5C86279E3D3C3FCC26970F4C568156E9D4D86AD30654`.

`META-003EJ` adds:

```text
pillow_c_cms_profile_condition_tags(
    profile,
    out_present,
    present_count,
    out_codes,
    code_count,
    out_values,
    value_count,
    out_viewing_description,
    viewing_description_size,
    out_viewing_description_required)
```

The profile, arrays, and required-size pointer are required; counts must be
`3`, `4`, and `10`. Presence slots are ICC measurement condition, ICC viewing
condition, and viewing-condition description. Codes are measurement observer,
geometry, measurement illuminant type, and viewing illuminant type. Doubles are
measurement backing XYZ/flare followed by viewing illuminant XYZ and surround
XYZ. The description buffer may be null only with size zero for the query
phase; present `cmsMLU` text is returned as NUL-terminated UTF-8. Missing tags
retain zero presence/code/value/required slots. Undersized text output returns
`-2`; undecodable present text returns `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports are `435/435`, zero difference; SHA-256 is
`DA35205BCAD86A9433557014EA99CAF0C1302DB700C35D21DA3B06CC0BB9F69C`.

`META-003EI` adds:

```text
pillow_c_cms_profile_optional_text_tags(
    profile,
    out_present,
    present_count,
    out_screening_description,
    screening_description_size,
    out_screening_description_required,
    out_target,
    target_size,
    out_target_required)
```

The profile, two-slot presence array, and both required-size pointers are
required; `present_count` must be `2`. Output buffers may be null only with a
zero size for the query phase. Slots are screening description then character-
target text. LittleCMS decodes both ICC text encodings to `cmsMLU`; present
values are exposed as NUL-terminated UTF-8 with required sizes including NUL.
Missing tags keep independent zero presence/required slots and receive no
synthesized text. Undersized output returns `-2`, while a present tag that
cannot be decoded returns `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports are `434/434`, zero difference; SHA-256 is
`170B5D9A6147745601AB83E26F20E9F3822DDE3903E260BA8643F81CCA19D8DC`.

`META-003EH` adds:

```text
pillow_c_cms_profile_optional_signatures(
    profile,
    out_present,
    present_count,
    out_values,
    value_count)
```

All pointers are required. Both counts must be `3`; other lengths return `-2`.
Slots are perceptual rendering-intent gamut, saturation rendering-intent
gamut, and technology. Each present signature is returned as a `uint32`;
missing tags retain independent zero presence/value slots, with no synthesized
gamut or technology. Release x64 builds with zero warnings/errors; source/DLL
exports are `433/433`, zero difference; SHA-256 is
`87CA443DEC300C00391534CAA95F53F1A65FE786747995BDADC329B79827D9D2`.

`META-003EG` adds:

```text
pillow_c_cms_profile_header_identity(
    profile,
    out_year,
    out_month,
    out_day,
    out_hour,
    out_minute,
    out_second,
    out_flags,
    out_manufacturer,
    out_model,
    out_profile_id,
    profile_id_size)
```

All pointers are required and `profile_id_size` must be `16`; other lengths
return `-2`. The date fields intentionally expose Pillow 11.3.0's direct
`struct tm` mapping: year is `tm_year + 1900`, while month remains the zero-
based `tm_mon`. Invalid header time returns `-3`. Flags, manufacturer/model
signatures, and the 16-byte profile ID come from LittleCMS in the same call.
Release x64 builds with zero warnings/errors; source/DLL exports are `432/432`,
zero difference; SHA-256 is
`7E06A77903BEC6A45B229400A9612CB44A09C67CA6E2FEEB142606AE5B5FBB8A`.

`META-003EF` adds:

```text
pillow_c_cms_profile_chromaticity(
    profile,
    out_present,
    out_values,
    value_count)
```

All pointers are required and `value_count` must be `9`; other lengths return
`-2`. A present ICC chromaticity tag returns red, green, and blue xyY triples
in that order. A missing tag returns success with `out_present=0` and nine
zeroed doubles; no primary transform or default chromaticity is synthesized.
Release x64 builds with zero warnings/errors; source/DLL exports are `431/431`,
zero difference; SHA-256 is
`BB429CE6CFC1C951945B792081A8AB161A0346A4F2CBA3F65543CAF3DACA7023`.

`META-003EE` adds:

```text
pillow_c_cms_profile_optional_xyz_tags(
    profile,
    out_present,
    present_count,
    out_values,
    value_count)
```

All pointers are required. `present_count` must be `2` and `value_count` must
be `12`; other lengths return `-2`. Presence/value slots are ordered as media
black point then luminance. Each present tag returns XYZ followed by derived
xyY in six doubles. Missing tags retain independent zero presence and value
slots; no default black point or luminance is synthesized. Release x64 builds
with zero warnings/errors; source/DLL exports are `430/430`, zero difference;
SHA-256 is
`7022AA1168CF0D8EB2113A2C02EE43CAAE460DD2D22008AE22D1E29B5F3A5807`.

`META-003ED` adds:

```text
pillow_c_cms_profile_rgb_primaries(
    profile,
    out_present,
    present_count,
    out_values,
    value_count)
```

All pointers are required. `present_count` must be `3` and `value_count` must
be `18`; other lengths return `-2`. Matrix-shaper profiles transform red,
green, and blue double unit vectors from `TYPE_RGB_DBL` to `TYPE_XYZ_DBL` at
relative intent with `cmsFLAGS_NOCACHE|cmsFLAGS_NOOPTIMIZE`, then return each
XYZ plus derived xyY in six-double slots. Non-matrix profiles return zero
presence slots. Release x64 builds with zero warnings/errors; source/DLL exports
are `429/429`, zero difference; SHA-256 is
`652D5D7587D39E13CBEBC8BFDEEDA70E9D79BC3884E16C61AAA788AEBA95C07B`.

`META-003EC` adds:

```text
pillow_c_cms_profile_chromatic_adaptation(
    profile,
    out_present,
    out_values,
    value_count)
```

All pointers are required and `value_count` must be `18`; other lengths return
`-2`. The first nine doubles are the row-major 3x3 XYZ `chad` matrix and the
next nine are row-major xyY values derived from each XYZ row. A missing tag
returns success with `out_present=0` and zeroed values; no identity matrix is
synthesized. Release x64 builds with zero warnings/errors; source/DLL exports
are `428/428`, zero difference; SHA-256 is
`145125212DD172068C201D93F0BA0D83F49A84CBC0A74D4005E2D6BFC1920AC7`.

`META-003EB` adds:

```text
pillow_c_cms_profile_rgb_colorants(
    profile,
    out_present,
    present_count,
    out_values,
    value_count)
```

All pointers are required. `present_count` must be `3` and `value_count` must
be `18`; other lengths return `-2`. Slots are red, green, blue; each six-double
value is XYZ followed by xyY. Missing tags retain a zero presence/value slot;
present tags are read and converted with LittleCMS in one call. Release x64
builds with zero warnings/errors; source/DLL exports are `427/427`, zero
difference; SHA-256 is
`B9588372B3558E2AF7CAE9EBF259979914F15BDBCE6F9AA6876B5F951B26AE1A`.

`META-003EA` adds:

```text
pillow_c_cms_profile_media_white_point_temperature(
    profile,
    out_present,
    out_temperature)
```

All pointers are required. A missing ICC media-white-point tag returns success
with `out_present=0` and zero temperature; present data returns `out_present=1`.
The query reuses the provenance-aware native media white point, derives xyY,
and converts it with `cmsTempFromWhitePoint`; an unconvertible white point
returns invalid argument. Release x64 builds with zero warnings/errors;
source/DLL exports are `426/426`, zero difference; SHA-256 is
`8578C4AB19C614E11359F6295928FC1F1E217BA70EEA174F30E5B2384EB9363D`.

`META-003DZ` adds:

```text
pillow_c_cms_profile_media_white_point(
    profile,
    out_present,
    out_xyz_x,
    out_xyz_y,
    out_xyz_z,
    out_xyy_x,
    out_xyy_y,
    out_xyy_luminance)
```

All pointers are required. A missing ICC media-white-point tag returns success
with `out_present=0` and zeroed values; present data returns `out_present=1`.
Native-created sRGB profiles retain nominal LittleCMS D50 provenance, while
serialized/opened profiles expose ICC s15Fixed16 tag values. xyY is derived in
the DLL with `cmsXYZ2xyY`. Release x64 builds with zero warnings/errors;
source/DLL exports are `425/425`, zero difference; SHA-256 is
`62C3DB2C5B6180784F31AC57735E36B6F5D7F07DC63ADC960D7B5F2C4DADC99D`.

`META-003DY` adds:

```text
pillow_c_cms_profile_header(
    profile,
    out_device_class,
    out_color_space,
    out_connection_space,
    out_encoded_version,
    out_version,
    out_is_matrix_shaper)
```

The immutable query returns LittleCMS four-byte signatures as big-endian
`uint32`, encoded ICC version as `uint32`, floating profile version as `double`,
and matrix-shaper state as `0/1`. All pointers are required; null returns `-1`.
Release x64 builds with zero warnings/errors; source/DLL exports are `424/424`
with zero set difference, and DLL SHA-256 is
`8E9219B6B39517D365F62C620BC1EE0382D33A0AF391DFD5C7BA4F97036DFA4B`.

`META-003DX` changes no native signature or export. Low-level sRGB
`CmsProfile` properties reuse `pillow_c_cms_profile_name`,
`pillow_c_cms_profile_copyright`, `pillow_c_cms_profile_manufacturer`,
`pillow_c_cms_profile_model`, and `pillow_c_cms_profile_default_intent`.
Source/Release x64 exports remain `423/423` with zero set difference, and DLL
SHA-256 remains
`9BE524D2B9F1BF769310D8579557C0984DCE8794D13BD107225C03E854608ED1`.

`META-003DW` changes no native signature or export. Public
`ImageCmsProfile.tobytes()` delegates to the established
`pillow_c_cms_profile_bytes(profile, out, out_size, required)` query/copy ABI.
Source/Release x64 exports remain `423/423` with zero set difference, and DLL
SHA-256 remains
`9BE524D2B9F1BF769310D8579557C0984DCE8794D13BD107225C03E854608ED1`.

`META-003DV` adds `pillow_c_cms_profile_retain(profile)` and changes
`pillow_c_cms_profile_free(profile)` from unconditional destruction to one
atomic reference release. Newly owned profiles start at reference count one;
retain adds one; only the final free closes LittleCMS and deletes the wrapper.
This lets an existing `CmsProfile` and its public `ImageCmsProfile` wrapper use
the exact same pointer while either close order remains valid. Release x64
builds with zero warnings/errors; source/DLL exports are `423/423` with zero
set difference, and DLL SHA-256 is
`9BE524D2B9F1BF769310D8579557C0984DCE8794D13BD107225C03E854608ED1`.

`META-003DU` changes no native signature or export. The facade's bounded AHK
File-like branch bulk-reads the remaining stream bytes and reuses
`pillow_c_cms_profile_open_memory`, whose independent LittleCMS ownership is
already established. Source/Release x64 exports remain `422/422` with zero set
difference, and DLL SHA-256 remains
`C152B20A2FEA82ADE6FC0C107B0F1853EA55C9419E74C871103D61CE1B076298`.

`META-003DT` adds
`pillow_c_cms_profile_open_file_wide(path, out_profile)` for Pillow's distinct
non-ASCII Windows path branch. It opens the UTF-16 path in binary mode, reads
the complete bounded file into DLL memory, closes the file, and creates an
independent LittleCMS profile through `cmsOpenProfileFromMem`; zero/oversized
files return `-2`, file/read/profile failures return `-3`, and allocation
failure returns `-4`. The source can be deleted immediately while the exact
description/default-intent profile remains live. Release x64 builds with zero
warnings/errors; source/DLL exports are `422/422` with zero set difference,
and DLL SHA-256 is
`C152B20A2FEA82ADE6FC0C107B0F1853EA55C9419E74C871103D61CE1B076298`.

`META-003DS` adds
`pillow_c_cms_profile_open_file(path, out_profile)` for one bounded absolute
ASCII ICC path. Null pointers return `-1`; an unreadable or invalid profile
returns `-3`. LittleCMS `cmsOpenProfileFromFile(path, "r")` owns the file and
profile lifetime, including Pillow-exact Windows deletion locking until the
existing `pillow_c_cms_profile_free` closes the handle. The opened 588-byte
sRGB profile returns exact `sRGB built-in\n`. Release x64 builds with zero
warnings/errors; source/DLL exports are `421/421` with zero set difference,
and DLL SHA-256 is
`CC4249651999235AD3D666341A32CAAAD6DD7010A504CCD6F8B1B220EAF0EF27`.

`META-003DR` changes no native signature or export. The distinct public
`ImageCms.getProfileDescription` facade method reuses
`pillow_c_cms_profile_name(profile, out, out_size, out_required)`, whose
LittleCMS description query and two-pass UTF-8/NUL contract already return
exact `sRGB built-in\n` for built-in and memory-opened sRGB profiles after
source-memory release. Source/Release x64 exports remain `420/420` with zero
set difference, and DLL SHA-256 remains
`36828F30514995CD98435BDD8C25DF67FEB7A94164377C7562C773ED56C14DEA`.

`META-003DQ` adds
`pillow_c_cms_profile_model(profile, out, out_size, out_required)`. It queries
LittleCMS `cmsInfoModel` in `en-US` and appends Pillow's trailing LF. The
built-in sRGB profile legally omits that optional tag, so a zero-length query
explicitly serializes Pillow's empty-field result `\n` plus the trailing NUL.
Built-in and memory-opened profiles preserve that result after source-memory
release. Release x64 builds with zero warnings/errors; source/DLL exports are
`420/420` with zero set difference, and DLL SHA-256 is
`36828F30514995CD98435BDD8C25DF67FEB7A94164377C7562C773ED56C14DEA`.

`META-003DP` adds
`pillow_c_cms_profile_manufacturer(profile, out, out_size, out_required)`. It
queries LittleCMS `cmsInfoManufacturer` in `en-US` and appends Pillow's trailing
LF. The built-in sRGB profile legally omits that optional tag, so a zero-length
LittleCMS query explicitly serializes Pillow's empty-field result `\n` plus
the trailing NUL; this is part of the public two-pass contract, not error
suppression. Built-in and memory-opened profiles preserve that result after
source-memory release. Release x64 builds with zero warnings/errors; source/
DLL exports are `419/419` with zero set difference, and DLL SHA-256 is
`773C252013B3DA54A342CBF84C70597072AD0341DA72335C4460B802092E48D7`.

`META-003DO` adds
`pillow_c_cms_profile_copyright(profile, out, out_size, out_required)`. It
queries LittleCMS `cmsInfoCopyright` in `en-US`, appends Pillow's trailing LF,
and follows the existing two-pass UTF-8 buffer contract including the trailing
NUL in `out_required`. Built-in and memory-opened sRGB profiles return exact
`No copyright, use freely\n` after source-memory release. Release x64 builds
with zero warnings/errors; source/DLL exports are `418/418` with zero set
difference, and the current DLL SHA-256 is
`F76BF5219E05A577EE696DF4EBEBD8A0353314208BA0ED41CEB5606C96D7A69F`.

`META-003DN` adds
`pillow_c_cms_profile_info(profile, out, out_size, out_required)`. It queries
LittleCMS description and copyright fields in `en-US`, joins nonempty values
with `\r\n\r\n`, appends the final `\r\n\r\n`, and follows the existing
two-pass UTF-8/NUL contract. Built-in and memory-opened sRGB profiles return
exact `sRGB built-in\r\n\r\nNo copyright, use freely\r\n\r\n` after source-
memory release. Release x64 builds with zero warnings/errors; source/DLL
exports are `417/417` with zero set difference, and DLL SHA-256 is
`570E2737B412E5351A6BAB7E7EBA3400EB60912BCABE3BD0DB5A556BDD964BA9`.

`META-003DM` adds
`pillow_c_cms_profile_intent_supported(profile, intent, direction,
out_supported)`. It accepts intents `0..3` and LittleCMS input/output/proof
directions `0..2`, rejects values outside that bounded domain with `-3`, and
returns `cmsIsIntentSupported` as native boolean `0/1`; the facade maps that
to Pillow's public `-1/1` convention. Release x64 builds with zero warnings/
errors; source/DLL exports are `416/416` with zero set difference, and the
current DLL SHA-256 is
`D8CC164C7F1672B36DB0CB87DF80EB449E3C76576901119D40514D5CA7C12E06`.

`META-003DL` adds
`pillow_c_cms_profile_default_intent(profile, out_intent)`. It validates the
opaque profile and output pointer, then reads the ICC header rendering intent
through `cmsGetHeaderRenderingIntent`; the caller retains profile ownership.
Built-in and memory-opened sRGB profiles return exact intent `0` after source-
memory release. Release x64 builds with zero warnings/errors; source/DLL
exports are `415/415` with zero set difference, and the current DLL SHA-256 is
`E9EB12B3B9AD7418CA5F3381AD522850FAEE95B552AA8E2DC6E190F006969EA2`.

`META-003DK` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits RGB/sRGB-to-LAB/
LAB at absolute-colorimetric render intent `3`, absolute proof intent `3`, and
`SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). This completes gamut-check
render intents `0..3` for all four established RGB/LAB proof mode pairs;
LittleCMS owns LAB rows, allocation, traversal, transform lifetime, and
serialized 572-byte output profiles. Release x64 builds with zero warnings/
errors; source/DLL exports remain `414/414` with zero set difference, and the
current DLL SHA-256 is
`E0F0FBB92CA7F5DCDB102923E40391AC26D3A21A6808B1A4AD31F43787359FAC`.

`META-003DJ` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits RGB/sRGB-to-LAB/
LAB at saturation render intent `2`, absolute proof intent `3`, and
`SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS owns LAB rows,
allocation, traversal, transform lifetime, and serialized 572-byte output
profiles. Absolute intent for this pair remained `-3`. Release x64 builds with
zero warnings/errors; source/DLL exports remain `414/414` with zero set
difference, and the current DLL SHA-256 is
`20435AF3C33EADB46C32EE3E6AA5E9FB34CBDC49D3E492E29BA770D1B491DD64`.

`META-003DI` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits RGB/sRGB-to-LAB/
LAB at relative-colorimetric render intent `1`, absolute proof intent `3`, and
`SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS owns LAB rows,
allocation, traversal, transform lifetime, and serialized 572-byte output
profiles. Later intents for this pair remain `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `414/414` with zero set difference,
and the current DLL SHA-256 is
`1D0A1D39AEE132C08C7394D66B7AE0F3C4582A3CCBA34F07A1C50A403DD56B55`.

`META-003DH` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits RGB/sRGB-to-LAB/
LAB at perceptual render intent `0`, absolute proof intent `3`, and
`SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS owns LAB rows,
allocation, traversal, transform lifetime, and serialized 572-byte output
profiles. Later intents for this pair and other RGB-input gamut-check mode
pairs remain `-3`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `414/414` with zero set difference, and the current DLL SHA-256
is `25105F88F8CF5A7C652E7634F58382A60B434520F7D54D9569CD6CDFC2CFB352`.

`META-003DG` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits RGB/sRGB-to-RGB/
sRGB at absolute-colorimetric render intent `3`, absolute proof intent `3`,
and `SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`), completing render intents
`0..3` for that pair. LittleCMS owns identity rows, allocation, traversal,
transform lifetime, and serialized 588-byte output profiles. Other RGB-input
gamut-check mode pairs remain `-3`. Release x64 builds with zero warnings/
errors; source/DLL exports remain `414/414` with zero set difference, and the
current DLL SHA-256 is
`756A717649DE278C54C4B52D94052ED6797C37A0C9746BDF2F91038A281EED2D`.

`META-003DF` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits RGB/sRGB-to-RGB/
sRGB at saturation render intent `2`, absolute proof intent `3`, and
`SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS owns identity rows,
allocation, traversal, transform lifetime, and serialized 588-byte output
profiles. Other new RGB-input gamut-check combinations remain `-3`. Release
x64 builds with zero warnings/errors; source/DLL exports remain `414/414`
with zero set difference, and the current DLL SHA-256 is
`B35D3CE8A4B91A621CC9D4F8B1DA2A10FFA753C082A8B41208A4A10F69F5DEC0`.

`META-003DE` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits RGB/sRGB-to-RGB/
sRGB at relative-colorimetric render intent `1`, absolute proof intent `3`,
and `SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS owns identity
rows, allocation, traversal, and serialized 588-byte output profiles. Other
new RGB-input gamut-check combinations remain `-3`. Release x64 builds with
zero warnings/errors; source/DLL exports remain `414/414` with zero set
difference, and the current DLL SHA-256 is
`96A878B9B180D44BD5F0FEC273AC28C34AE0CE1DCE39665CD71964F9F65F6D51`.

`META-003DD` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits RGB/sRGB-to-RGB/
sRGB at perceptual render intent `0`, absolute proof intent `3`, and
`SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS owns identity rows,
allocation, traversal, and serialized 588-byte output-profile bytes after all
profiles and serialized profile memory are released. Other RGB-input gamut-
check combinations remain `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `414/414` with zero set difference, and the current
DLL SHA-256 is
`9BFDDFEF8A8360E82619452DB3532D97B17DB1F8679DB845DE617EA06B117530`.

`META-003DC` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits D50-to-6500K
LAB/LAB at absolute-colorimetric render intent `3`, absolute proof intent `3`,
and `SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). Both established LAB-input
mode pairs now admit render intents `0..3`; LittleCMS owns alarm rows,
allocation, traversal, and serialized output profiles. RGB-input gamut-check
mode pairs remain `-3`. Release x64 builds with zero warnings/errors; source/
DLL exports remain `414/414` with zero set difference, and the current DLL
SHA-256 is
`19BCE546A1986595033EE82F4A883E47FD7C4C2B5F3F8184E5ABCAFEA3A397E9`.

`META-003DB` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits D50-to-6500K
LAB/LAB at saturation render intent `2`, absolute proof intent `3`, and
`SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS owns exact Lab8 alarm
rows, transform allocation, traversal, and serialized 572-byte output-profile
bytes. Absolute LAB/LAB gamut checking remains `-3`. Release x64 builds with
zero warnings/errors; source/DLL exports remain `414/414` with zero set
difference, and the current DLL SHA-256 is
`5FA6EC3BFE4C6E4DD59BAA65AEF1BF8E43C1E1B152595179E4C18B7554DA7F82`.

`META-003DA` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits D50-to-6500K
LAB/LAB at relative-colorimetric render intent `1`, absolute proof intent `3`,
and `SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS owns exact Lab8
alarm rows, transform allocation, traversal, and serialized 572-byte output-
profile bytes. Other new LAB/LAB gamut-check intents remain `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `414/414` with
zero set difference, and the current DLL SHA-256 is
`DB35967AC2A04168B45685753BABA8146B45F82AE8A0372339AD7E07F9C57E6C`.

`META-003CZ` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits LAB/LAB-to-RGB/
sRGB at absolute-colorimetric render intent `3`, absolute proof intent `3`,
and `SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS owns gamut
classification, exact RGB8 alarm rows, allocation, and serialized profile
bytes. Other new gamut-check mode pairs remain `-3`. Release x64 builds with
zero warnings/errors; source/DLL exports remain `414/414` with zero set
difference, and the current DLL SHA-256 is
`94CA5DF10ADC9826825EF13E44642553E3C6C675146774A095188C4FCEB0AD7B`.

`META-003CY` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits LAB/LAB-to-RGB/
sRGB at saturation render intent `2`, absolute proof intent `3`, and
`SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS owns alarm
classification, output rows, allocation, and serialized profile bytes. Other
new gamut-check intents/mode pairs remain `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `414/414`, and the current DLL
SHA-256 is
`391CFC8C6EC50CC4F7DB0331D1A0E3D9E268F9B21A8FA699E8E35D8BD1B63848`.

`META-003CX` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits LAB/LAB-to-RGB/
sRGB at relative-colorimetric render intent `1`, absolute proof intent `3`,
and `SOFTPROOFING|GAMUTCHECK` (`20480`, `0x5000`). LittleCMS continues to own
default RGB8 alarms, all rows, allocation, and serialized output-profile
bytes. Other new gamut-check intents and mode pairs remain `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `414/414` with zero
set difference, and the post-CX DLL SHA-256 is
`41A9469CDFEA84B830B39D8E7443A345EBF2F9BEF6518C2B157F748AA4C7B58E`.

`META-003CW` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits LAB/LAB-to-RGB/
sRGB at perceptual render intent `0`, absolute proof intent `3`, and flags
`cmsFLAGS_SOFTPROOFING|cmsFLAGS_GAMUTCHECK` (`20480`, `0x5000`). LittleCMS
owns default RGB8 alarm `[127,127,127]`, gamut classification, output rows,
and serialized 588-byte output-profile ownership. Other gamut-check intents
and mode pairs remain `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `414/414` with zero set difference, and the current
post-CW DLL SHA-256 is
`45E74FB5FBF11D8C05212FA14F904CDE01E730D2A531264F7FEAFE46D426BA8C`.

`META-003CV` changes no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now additionally admits D50-to-6500K
LAB/LAB when rendering intent is perceptual `0`, proof intent is absolute-
colorimetric `3`, the proof profile is RGB, and flags are exactly
`cmsFLAGS_SOFTPROOFING|cmsFLAGS_GAMUTCHECK` (`20480`, `0x5000`). The transform
uses LittleCMS's default Lab8 alarm color `[127,255,255]`; transform allocation,
gamut classification, repeat-apply rows, and serialized output-profile
ownership remain native. Other gamut-check mode pairs and intents, explicit
alarm-code configuration, path/file-like inputs, and proof-transform in-place
apply remain separate and return `-3`. Release x64 builds with zero warnings/
errors; source/DLL exports remain `414/414` with zero set difference, and the
post-CV DLL SHA-256 is
`CE7DC2A13AEC9F369E902A5E15C7D0099ADDD1155840467DB0639F8FF18444DA`.

`META-003CJ` through `META-003CU` change no signature or export count.
Existing `pillow_c_cms_proof_transform_build` now admits relative-colorimetric
intent `1` and saturation intent `2` for all four established RGB/LAB mode
pairs, in addition to perceptual intent `0` and absolute-colorimetric intent
`3` for the same complete matrix. Profile color spaces must match the requested
modes, the proof profile must be RGB, proof intent must be absolute-
colorimetric `3`, and flags must be exactly `cmsFLAGS_SOFTPROOFING` (`16384`,
`0x4000`). Transform allocation, format selection, repeat-apply rows, and
serialized output-profile ownership remain native. Other flags except the
bounded `META-003CV` route, path/file-like inputs, and other modes still return
`-3`; proof-transform
in-place apply remains a separate compatibility surface.
The post-CU Release x64 build had source/DLL exports `414/414` with zero set
difference and DLL SHA-256
`6586FA42C426B355D16F1C4B9E979699D50F54797AD257C1A873C4498F907DF8`.

`META-003CG` through `META-003CI` change no signature or export count. Existing
`pillow_c_cms_proof_transform_build` now admits all four established RGB/LAB
mode pairs when input/output profile color spaces match the requested modes,
the proof profile is RGB, rendering intent is perceptual `0`, proof intent is
absolute-colorimetric `3`, and flags equal `cmsFLAGS_SOFTPROOFING` (`16384`,
`0x4000`). It selects `TYPE_RGB_8` or `TYPE_Lab_8` independently for input and
output, owns the LittleCMS transform plus serialized output-profile bytes, and
reuses existing native apply/output-profile/free exports after profile and
serialized-memory release. Other modes, intents, flags, gamut checking, and
path/file-like inputs still return `-3`. Release x64 builds with zero warnings
and errors; source/DLL exports remain `414/414` with zero set difference, and
that post-CI DLL SHA-256 is
`A4281086CCFD2529A449356CA17DCB90AE6C6EACE428ECF86E2E656C950067AE`.

`META-003CF` adds
`pillow_c_cms_proof_transform_build(input_profile, output_profile,
proof_profile, input_mode, output_mode, rendering_intent,
proof_rendering_intent, flags, out_transform)`. The bounded admission is
RGB-to-RGB across three RGB profiles with perceptual intent `0`, absolute-
colorimetric proof intent `3`, and `cmsFLAGS_SOFTPROOFING` (`16384`, `0x4000`).
It calls `cmsCreateProofingTransform`, owns serialized output-profile bytes in
the existing transform handle, and reuses existing apply/output-profile/free
exports after all three profile handles and serialized profile memory release.
Other modes, intents, flags, gamut checking, and path/file-like inputs return
`-3`. Release x64 builds with zero warnings/errors; source/DLL exports are
`414/414`, zero set difference, and DLL SHA-256 is
`994C4E68A62F2D4D1585ED8A2D0BC616A105848329839032F2FE4F2C232D0822`.

`META-003CE` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for D50-to-6500K
LAB-to-LAB with absolute-colorimetric intent `3`, completing intents `0..3`
plus BPC across both established same-mode pairs. The LAB route mutates the
same image handle and owned data pointer and remains valid after both profile
handles release. Mode-changing in-place behavior, proofing, and other nonzero
flags still return `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `413/413`, zero set difference, and DLL SHA-256 is
`A6FC4F9CDD52D3683A8B84CAE1C0BD7B00CAE900E1AEAB40F81758A9DC4CA428`.

`META-003CD` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for RGB/sRGB-to-RGB/
memory-opened-sRGB with absolute-colorimetric intent `3`, in addition to
intents `0..2` plus BPC across both established same-mode pairs. The RGB route
mutates the same image handle and owned data pointer and remains valid after
source profile memory and both profile handles release. LAB/LAB absolute BPC,
mode-changing in-place behavior, proofing, and other nonzero flags still
return `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`1942EE638692A297ECD0B7DDDC0EFFEEF51E5421AEC81BFB46E49F78833AA75D`.

`META-003CC` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for saturation intent `2`
across both established same-mode pairs: RGB-to-RGB and LAB-to-LAB. The LAB
route mutates the same image handle and owned data pointer and remains valid
after both profile handles release. Intent `3` with BPC, mode-changing in-place
behavior, proofing, and other nonzero flags still return `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`DA44080F6FA2F1B407B251F2474D411222F7B170A58C5D4C8B7D95C8305E6402`.

`META-003CB` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for RGB/sRGB-to-RGB/
memory-opened-sRGB with saturation intent `2`, in addition to perceptual and
relative BPC across both established same-mode pairs. The RGB route mutates
the same image handle and owned data pointer and remains valid after source
profile memory and both profile handles release. LAB/LAB saturation BPC,
intent `3` with BPC, mode-changing in-place behavior, proofing, and other
nonzero flags still return `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `413/413`, zero set difference, and DLL SHA-256 is
`024A970B5AB947E6B0143ECD1FA6ADC270D864B0ED7E711F69F6348E020BE359`.

`META-003CA` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for relative-
colorimetric intent `1` across both established same-mode pairs: RGB-to-RGB
and LAB-to-LAB. The LAB route mutates the same image handle and owned data
pointer and remains valid after both profile handles release. Intents `2..3`
with BPC, mode-changing in-place behavior, proofing, and other nonzero flags
still return `-3`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `413/413`, zero set difference, and DLL SHA-256 is
`4D419E74375E79C5BF7BCA98BFCC2F4280B088037CD48A10FF77AEB1023B9216`.

`META-003BZ` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for RGB/sRGB-to-RGB/
memory-opened-sRGB with relative-colorimetric intent `1`, in addition to
perceptual BPC across both established same-mode pairs. The transform mutates
the same image handle and owned data pointer; source profile memory and both
profile handles may be released after the call. LAB/LAB relative BPC, intents
`2..3` with BPC, mode-changing in-place behavior, proofing, and other nonzero
flags still return `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `413/413`, zero set difference, and DLL SHA-256 is
`A3B84337DADD346ED2FB3EA60186F831D2D86F190304F52CA79AA713DB5E8E0F`.

`META-003BY` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for perceptual intent `0`
across both established same-mode pairs: RGB-to-RGB and LAB-to-LAB. The LAB
route transforms the same image handle and owned data pointer and remains valid
after both profiles release. Intents `1..3`, mode-changing in-place behavior,
proofing, and other nonzero flags still return `-3`. Release x64 builds with
zero warnings/errors; source/DLL exports remain `413/413`, zero set difference,
and DLL SHA-256 is
`1B89C158A8347B2365CBEA2995D15D6AFC39EDB54F8D610034C5811D28815728`.

`META-003BX` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) only for RGB/sRGB-to-RGB/
memory-opened-sRGB with perceptual intent `0`. The transform mutates the same
image handle and owned data pointer; the source profile Buffer and both profile
handles may be released after the call. LAB, intents `1..3`, other nonzero
flags, proofing, and mode-changing in-place transforms still return `-3`.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`413/413`, zero set difference, and DLL SHA-256 is
`039166C0E006A238013FB551F1A9E226F6EF7B8C403FBF2395AEC31AA434C595`.

`META-003BW` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
intents `0..3` across all four established RGB/LAB pairs. In-place BPC behavior
and other nonzero flags still return `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `413/413`, zero set difference, and
DLL SHA-256 is
`E3E9C988F5A82E80339E7716C502DC97B932CFFEEB15337B0D2DE59C09FD7722`.

`META-003BV` admitted the RGB-to-RGB allocating one-shot absolute-
colorimetric/BPC pair. It changed no signature or export count; its bounded
Release x64 artifact had SHA-256
`6F254FC54336DF10867E6E18BD6EB363A73A0D905272C088547070B0C9AAC034`.

`META-003BU` completed both mode-changing allocating one-shot absolute-
colorimetric/BPC pairs. It changed no signature or export count; its bounded
Release x64 artifact had SHA-256
`B78A0BAF25C4E555EC6148CBCEC816EB684910F4AB1CAB923B9F73503B0EBDD4`.

`META-003BT` first admitted the RGB/sRGB-to-LAB/LAB half of allocating one-shot
absolute-colorimetric/BPC. It changed no signature or export count; its bounded
Release x64 artifact had SHA-256
`13713869393CEE107FD9597B9BFCB55159CA17461C6A0CD62E64976523278A6A`.

`META-003BS` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
saturation intent `2` across all four established pairs: RGB-to-LAB,
LAB-to-RGB, RGB-to-RGB, and LAB-to-LAB. Absolute intent, in-place flag behavior,
and other nonzero flags still return `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `413/413`, zero set difference, and
DLL SHA-256 is
`FF747CF87374210D6F5AA110C1BC51F519E601C43BD836A8B393A32D350CF6C1`.

`META-003BR` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
RGB/sRGB-to-RGB/memory-opened-sRGB with saturation intent `2`, in addition to
both mode-changing saturation/BPC pairs. LAB-to-LAB saturation, absolute
intent, in-place flag behavior, and other nonzero flags still return `-3`.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`413/413`, zero set difference, and DLL SHA-256 is
`D4C124C26CD858E1C9E9E77A26C39E096EF8E9F5025833DC36D8AD9891E5EE43`.

`META-003BQ` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
LAB/LAB-to-RGB/sRGB with saturation intent `2`, completing both mode-changing
saturation/BPC pairs. Same-mode saturation, absolute intent, in-place flag
behavior, and other nonzero flags still return `-3`. Release x64 builds with
zero warnings/errors; source/DLL exports remain `413/413`, zero set difference,
and DLL SHA-256 is
`0FF2388CF03A0AC036F7990FA428AAF1063E69DA0AA31282EDCFADD23344635F`.

`META-003BP` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
RGB/sRGB-to-LAB/LAB with saturation intent `2`, in addition to perceptual and
relative/BPC across all four established pairs. Other saturation one-shot
pairs, absolute intent, in-place flag behavior, and other nonzero flags still
return `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`955894B342D929BBE706D1494344D781061EFF8DDB942E5763AE491DD5E7F221`.

`META-003BO` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
relative-colorimetric intent `1` across all four established pairs: RGB-to-LAB,
LAB-to-RGB, RGB-to-RGB, and LAB-to-LAB. Other non-perceptual one-shot intents,
in-place flag behavior, and other nonzero flags still return `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`BC78D6575B41E9EE0D647E83422152D7946E00B0D0D7D7C7AB7B11C62E132281`.

`META-003BN` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
RGB/sRGB-to-RGB/memory-opened-sRGB with relative-colorimetric intent `1`, in
addition to both mode-changing relative/BPC pairs. Relative LAB-to-LAB, other
non-perceptual intents, in-place flag behavior, and other nonzero flags still
return `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`787BB7513BAEE4F3E5139BBE07C73CB8F204F014686E77A656A9FC55353B8647`.

`META-003BM` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for both allocating one-
shot mode-changing pairs with relative-colorimetric intent `1`: RGB-to-LAB and
LAB-to-RGB. Relative RGB-to-RGB and LAB-to-LAB, other non-perceptual intents,
in-place flag behavior, and other nonzero flags still return `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`B1C4292B6D129155E899719347248B0FA0656A5548E1139502FC6D9AE12984BD`.

`META-003BL` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
RGB/sRGB-to-LAB/LAB with relative-colorimetric intent `1`, in addition to the
four established perceptual/BPC pairs. Other relative one-shot pairs, other
non-perceptual intents, in-place flag behavior, and other nonzero flags still
return `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`57A1024D9C8C8D5EFBF0F2A21651D35AD387F279E01E6162F76BB425AF799DD0`.

`META-003BK` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
perceptual intent `0` across all four established pairs: RGB-to-LAB,
LAB-to-RGB, RGB-to-RGB, and LAB-to-LAB. Other one-shot intents, in-place flag
behavior, and other nonzero flags still return `-3`. Release x64 builds with
zero warnings/errors; source/DLL exports remain `413/413`, zero set difference,
and DLL SHA-256 is
`042C0DD7D51EAE67082178EF48EB663F8DA729BE4AA2E7BF752E39378A7C8BA1`.

`META-003BJ` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
RGB/sRGB-to-RGB/memory-opened-sRGB with perceptual intent `0`. LAB-to-LAB,
other one-shot intents, in-place flag behavior, and other nonzero flags still
return `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`88B83F8AE354FB6F1771DB84ECAADFECECE584925528DD074F47328DACC0CC0F`.

`META-003BI` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
LAB/LAB-to-RGB/sRGB with perceptual intent `0`, completing both established
mode-changing pairs for this exact one-shot intent/flag. Other one-shot pairs/
intents, in-place flag behavior, and other nonzero flags still return `-3`.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`413/413`, zero set difference, and DLL SHA-256 is
`133A9875DC0706C1E3D3A3EE69E9C761BD6DBC01AF2FD89C60AB6C8F5599746B`.

`META-003BH` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for allocating one-shot
RGB/sRGB-to-LAB/LAB with perceptual intent `0`. Other one-shot pairs/intents,
in-place flag behavior, and other nonzero flags still return `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`D3055F3760C03796E8215BDF1AE8791CEE61746C61B2EF38EE1D5D348A602EA7`.

`META-003BG` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for intents `0..3` across
all four established reusable pairs: RGB-to-LAB, LAB-to-RGB, RGB-to-RGB, and
LAB-to-LAB. Other nonzero flags and one-shot/in-place flag behavior still
return `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`A80DA2A1C8CB266858A18B1799F9EE61F70B321E849995B0F5D15F737EEB7147`.

`META-003BF` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for absolute-colorimetric
intent `3` on both mode-changing pairs and reusable RGB-to-RGB, in addition to
intents `0..2` across all four established pairs. Absolute-colorimetric intent
with this flag on LAB-to-LAB, other nonzero flags, and one-shot flag behavior
still return `-3`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `413/413`, zero set difference, and DLL SHA-256 is
`F9B52490F8557074EA72068D2351E8F271AEB1F1184DCF742B282502ADCA968B`.

`META-003BE` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for absolute-colorimetric
intent `3` on both mode-changing reusable pairs, in addition to intents `0..2`
across all four established pairs. Absolute-colorimetric intent with this flag
on RGB-to-RGB and LAB-to-LAB, other nonzero flags, and one-shot flag behavior
still return `-3`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `413/413`, zero set difference, and DLL SHA-256 is
`E118088CC55492410774BCDA44FCA0A8360B52914F6987E24258743B99EA6624`.

`META-003BD` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for absolute-colorimetric
intent `3` on reusable RGB/sRGB-to-LAB/LAB, in addition to intents `0..2`
across all four established reusable pairs. Absolute-colorimetric intent with
this flag on LAB-to-RGB, RGB-to-RGB, and LAB-to-LAB, other nonzero flags, and
one-shot flag behavior still return `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `413/413`, zero set difference,
and DLL SHA-256 is
`43E9F4E3A46131DEB57540F7C692CCF58A55296AA40ECAA7FB16BBBBCC977603`.

`META-003BC` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for perceptual, relative-
colorimetric, and saturation intents `0..2` across all four established
reusable pairs. Absolute-colorimetric intent with this flag, other nonzero
flags, and one-shot flag behavior still return `-3`. Release x64 builds with
zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`5BC61E57225926A3528E28FD9B685723AE01A4D9525831B7FFEE9A2EB76B2D9C`.

`META-003BB` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for saturation intent `2`
on reusable RGB/sRGB-to-RGB/memory-opened-sRGB as well as both mode-changing
pairs. LAB/LAB saturation, absolute-colorimetric intent with this flag, other
nonzero flags, and one-shot flag behavior still return `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero
set difference, and DLL SHA-256 is
`A40D84E96D877509CFE00F5954EC4294232D154E63700A061812E1264CA6DCD4`.

`META-003BA` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for saturation intent `2`
on both mode-changing reusable pairs, in addition to perceptual and relative-
colorimetric intent coverage across all four established reusable pairs.
Same-mode saturation, absolute-colorimetric intent with this flag, other
nonzero flags, and one-shot flag behavior still return `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero
set difference, and DLL SHA-256 is
`EADC87096D89902FD73DAE04F97596A3FBE97F89863628BAC930F92D5A2C313B`.

`META-003AZ` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for saturation intent `2`
on reusable RGB/sRGB-to-LAB/LAB, in addition to perceptual and relative-
colorimetric intent coverage across all four established reusable pairs.
Other saturation pairs, absolute-colorimetric intent with this flag, other
nonzero flags, and one-shot flag behavior still return `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero
set difference, and DLL SHA-256 is
`A180FD81089BFCABF9F375F8325A25A624A79F80986159BB4BB5DDC4DDC9505F`.

`META-003AY` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for relative-colorimetric
intent `1` across all four established reusable pairs, matching the existing
four-pair perceptual/BPC route. Saturation and absolute-colorimetric intents
with this flag, other nonzero flags, and one-shot flag behavior still return
`-3`. Release x64 builds with zero warnings/errors; source/DLL exports remain
`413/413`, zero set difference, and DLL SHA-256 is
`855712FE0FF147089DA4F57DDC1F89814C2E2D17B2266D4AE82069939511FEF5`.

`META-003AX` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for relative-colorimetric
intent `1` on reusable RGB/sRGB-to-RGB/memory-opened-sRGB, in addition to both
mode-changing pairs and perceptual intent `0` across all four established
pairs. LAB/LAB relative/BPC, other nonzero flags, remaining intents with this
flag, and one-shot flag behavior still return `-3`. Release x64 builds with
zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`131BB72367D5296BF4A75A2D444D1B0255A424D666F60B919D6D103728605D3A`.

`META-003AW` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for relative-colorimetric
intent `1` on both mode-changing reusable pairs, in addition to perceptual
intent `0` across all four pairs. Same-mode relative/BPC, other nonzero flags,
remaining intents with this flag, and one-shot flag behavior still return
`-3`. Release x64 builds with zero warnings/errors; source/DLL exports remain
`413/413`, zero set difference, and DLL SHA-256 is
`7803C98276EE73B524B55BBEA8F040C17A8FB08F02D1D2EDF3D4CDE72C0BE225`.

`META-003AV` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for relative-colorimetric
intent `1` on reusable RGB/sRGB-to-LAB/LAB, in addition to perceptual intent
`0` across all four established reusable pairs. Other nonzero flags, remaining
intent/pair combinations with this flag, and one-shot flag behavior still
return `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`0524291D2A58709EF2A0463D98EAB4E0D2FDE06A15F59768FF83B1C97401A76E`.

`META-003AU` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts perceptual-intent (`0`)
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) across all four
established reusable pairs. Other nonzero flags, other intents with this flag,
and one-shot flag behavior still return `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `413/413`, zero set difference, and
DLL SHA-256 is
`BA4E8CAC692E7AFF483551D6BBE2D1E1906DD661CF5FA45580BCA4E53A78F852`.

`META-003AT` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts perceptual-intent (`0`)
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for reusable RGB/RGB as
well as both mode-changing pairs. LAB/LAB with this flag, other nonzero flags,
other intents with this flag, and one-shot flag behavior still return `-3`.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`413/413`, zero set difference, and DLL SHA-256 is
`26FB915D689B5A5C9BFFFD786408BA7CD4FC609843FCB0EB7E874E74451164F5`.

`META-003AS` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts perceptual-intent (`0`)
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) for both established
mode-changing reusable pairs, `RGB/sRGB -> LAB/LAB` and
`LAB/LAB -> RGB/sRGB`. Other nonzero flags, intents with this flag, same-mode
pairs with this flag, and one-shot flag behavior still return `-3`. Release
x64 builds with zero warnings/errors; source/DLL exports remain `413/413`, zero
set difference, and DLL SHA-256 is
`C301A54FB0CF19DAA9850C4DB3F0AE7B370F3EA28BBBDDDDAC89AE2956096120`.

`META-003AR` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts the exact
`RGB/sRGB -> LAB/LAB`, perceptual-intent (`0`),
`cmsFLAGS_BLACKPOINTCOMPENSATION` (`8192`, `0x2000`) combination in addition
to all previously covered flags-`0` routes. Other nonzero flags, intents with
this flag, pairs with this flag, and one-shot flag behavior still return `-3`.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`413/413`, zero set difference, and DLL SHA-256 is
`5022FA192187DC881048D4D6982203D07084F91315F9C2EC782AA2A86FC5A5ED`.

`META-003AQ` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts all Pillow rendering intents `0..3`
across all four established reusable pairs while still requiring flags `0`.
Nonzero flags remain rejected with `-3`. Release x64 builds with zero warnings/
errors; source/DLL exports remain `413/413`, zero set difference, and DLL
SHA-256 is
`5958951C963A441633262D5A69E7C0845B277983E6AB2B9B2E5091763FB1C468`.

`META-003AP` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts saturation rendering intent `2`
across all four reusable pairs while still requiring flags `0`. Reusable LAB/
LAB intent `3` and nonzero flags remain rejected with `-3`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`A212108E787C112C4B8A4A79CD07866E86888BB586D9C2D09E20C86DD99DA417`.

`META-003AO` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts all Pillow rendering intents `0..3`
for reusable RGB/RGB as well as both mode-changing pairs while still requiring
flags `0`. Reusable LAB/LAB intents `2`/`3` and nonzero flags remain rejected
with `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`2555122CE65CC6EFB330FB5886E1C1D08646B42F4AD1F03800014D39677A1674`.

`META-003AN` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts saturation rendering intent `2` for
reusable RGB/RGB as well as both mode-changing pairs while still requiring
flags `0`. Reusable RGB/RGB intent `3`, LAB/LAB intents `2`/`3`, and nonzero
flags remain rejected with `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `413/413`, zero set difference, and DLL SHA-256 is
`E757CFF604A83E7F9DB5E62748DA0866A015E4030D1DFFD52ECC0396049FB5C6`.

`META-003AM` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts all Pillow rendering intents `0..3`
for both mode-changing reusable pairs, RGB-to-LAB and LAB-to-RGB, while still
requiring flags `0`. Same-mode reusable intents `2`/`3` and nonzero flags remain
rejected with `-3`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `413/413`, zero set difference, and DLL SHA-256 is
`68DD191A38DDC994D54C92063F3F6BD3EEA26EB9227BD32CDE069FB936766A3F`.

`META-003AL` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts saturation rendering intent `2` for
the reusable LAB/LAB-profile-to-RGB/sRGB pair as well as RGB-to-LAB while still
requiring flags `0`. Reusable LAB-to-RGB intent `3`, same-mode intents `2`/`3`,
and nonzero flags remain rejected with `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `413/413`, zero set difference, and
DLL SHA-256 is
`7DC94CEE64953E580B4FF528EF1FE4F883A7782C8FD252312F1F6E46E2895BF5`.

`META-003AK` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts all Pillow
rendering intents `0..3` for both legal same-mode pairs, RGB/RGB and LAB/LAB,
while still requiring flags `0`. Mode-changing in-place pairs and nonzero flags
remain rejected with `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `413/413`, zero set difference, and DLL SHA-256 is
`9EAA6510AB9D48C0E3CC81A2F095FF8ECF83294D3131DEA83BCC9607E79268B6`.

`META-003AJ` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts saturation
rendering intent `2` for LAB/LAB as well as RGB/RGB while still requiring flags
`0`. LAB/LAB intent `3`, mode-changing in-place pairs, and nonzero flags remain
rejected with `-3`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `413/413`, zero set difference, and DLL SHA-256 is
`67DB2646A0BF9D0F224A5A9E034DD885200765F1300E2171D8B154383F3AD0AD`.

`META-003AI` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts all Pillow
rendering intents `0..3` for RGB/RGB while still requiring flags `0`. LAB/LAB
intents `2`/`3`, mode-changing in-place pairs, and nonzero flags remain rejected
with `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`984CCCB8B0F98E1C0BA4C56F2E524E351DABD7B473ABE824CE65A32FF8E82E0D`.

`META-003AH` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts saturation
rendering intent `2` for RGB/RGB while still requiring flags `0`. RGB/RGB
intent `3`, LAB/LAB intents `2`/`3`, mode-changing in-place pairs, and nonzero
flags remain rejected with `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `413/413`, zero set difference, and DLL SHA-256 is
`C9EE1E7AC81CD5167851643A72838DB819ED0C351C27622EEC056EDB8E8E414E`.

`META-003AG` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts all Pillow rendering
intents `0..3` across all four established allocating pairs while still
requiring flags `0`. In-place intents `2`/`3` and nonzero flags remain rejected
with `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`B3B4C87F9DE2E1D248B2589582E82E3D50F6348F4D9041ECC11FED709DC418F5`.

`META-003AF` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts saturation rendering
intent `2` across all four established allocating pairs while still requiring
flags `0`. LAB/LAB allocating absolute intent `3`, in-place intents `2`/`3`,
and nonzero flags remain rejected with `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `413/413`, zero set difference, and
DLL SHA-256 is
`BDEC3B6A5E806D0C27A13EC831275277958750F0FC0197AFAD168864978037CA`.

`META-003AE` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts all Pillow rendering
intents `0..3` for allocating RGB/RGB as well as both mode-changing pairs,
while still requiring flags `0`. LAB/LAB allocating intents `2`/`3`, in-place
intents `2`/`3`, and nonzero flags remain rejected with `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`119D575872616F90F27A69BD5D66BEE20BB99B2DFA0EC80160A7B6BD796C6A15`.

`META-003AD` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts saturation rendering
intent `2` for allocating RGB/RGB in addition to both mode-changing pairs,
while still requiring flags `0`. LAB/LAB allocating saturation, same-mode
absolute intent `3`, in-place intents `2`/`3`, and nonzero flags remain
rejected with `-3`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `413/413`, zero set difference, and DLL SHA-256 is
`320E5DFA1A75D541663FA1E203EF9A029F693B66A55D84F27B8C951BBFD363FA`.

`META-003AC` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts all Pillow rendering
intents `0..3` for both allocating mode-changing pairs, RGB/RGB-profile-to-
LAB/LAB-profile and LAB/LAB-profile-to-RGB/RGB-profile, while still requiring
flags `0`. Same-mode and in-place intents `2`/`3` plus nonzero flags remain
rejected with `-3`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `413/413`, zero set difference, and DLL SHA-256 is
`7ED4248BD3567D1DCD9DAEDBEFA409F494DB836EEF23C6881C554FB6EA71508A`.

`META-003AB` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts saturation rendering
intent `2` for both allocating mode-changing pairs, RGB/RGB-profile-to-LAB/
LAB-profile and LAB/LAB-profile-to-RGB/RGB-profile, while still requiring
flags `0`. AC above adds reverse intent `3`; same-mode and in-place intents
`2`/`3` plus nonzero flags remain rejected
with `-3`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `413/413`, zero set difference, and DLL SHA-256 is
`FF6A23B754663EC415B516163824064DEBD29766CAD83CE74155BA59C5B3CB33`.

`META-003AA` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts all Pillow rendering
intents `0..3` for the allocating RGB/RGB-profile-to-LAB/LAB-profile pair
while still requiring flags `0`. Other established allocating pairs remain
limited to intents `0`/`1`; in-place intents `2`/`3` and nonzero flags remain
rejected with `-3`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `413/413`, zero set difference, and DLL SHA-256 is
`BF16B7CA7182BD19D24961B84DC278475F65D6A8FBF92E17214C730B82929685`.

`META-003Z` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts saturation rendering
intent `2` only for the allocating RGB/RGB-profile-to-LAB/LAB-profile pair
while still requiring flags `0`. Other established allocating pairs remain
limited to intents `0`/`1`; AA above adds intent `3` to the same pair. In-place
intents `2`/`3` and nonzero flags remain rejected with `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `413/413`, zero set difference, and DLL SHA-256 is
`93DF64E845F43421F8F45151F5A0665EE8C1C5173B6D4AC0A2BFB49E35605C65`.

`META-003Y` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts all Pillow rendering intents `0..3`
for the reusable RGB/RGB-profile-to-LAB/LAB-profile pair while still requiring
flags `0`. Other established pairs remain limited to intents `0`/`1`; nonzero
flags remain rejected with `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `413/413`, zero set difference, and DLL SHA-256 is
`95105B01C1B252AEDB6BAC8D9AD974F2E2F28ADBC61C74E8D03DD3726D22BB49`.

`META-003X` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts saturation rendering intent `2`
only for the reusable RGB/RGB-profile-to-LAB/LAB-profile pair while still
requiring flags `0`; Y above adds intent `3` to that pair. Other established
pairs remain intent `0`/`1`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`4458CEFD629534307A29EDAC34F0D60C548BF188C864032C8DFCB1C2DCAD6C45`.

`META-003W` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts relative-
colorimetric rendering intent `1` for both legal same-mode pairs, RGB/RGB and
LAB/LAB, while still requiring flags `0`. Mode-changing pairs, intents `2`/`3`,
and nonzero flags remain rejected with `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `413/413`, zero set difference, and
DLL SHA-256 is
`2BF08CFD0D4FF7ADAE7BE52E9A2990AAFFF02C6253ED5C3B6055333ACEC8076A`.

`META-003V` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts relative-
colorimetric rendering intent `1` initially for RGB/RGB while still requiring
flags `0`; W above adds LAB/LAB. Intents `2`/`3`, nonzero flags, and mode-
changing in-place pairs remain rejected with `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `413/413`, zero set difference, and DLL SHA-256 is
`694A49B193366C86E26B60A83EEF6181EC7392EECDDF7681579AF0C359F973DC`.

`META-003U` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts relative-colorimetric
rendering intent `1` across all four established allocating pairs: RGB/LAB,
LAB/RGB, RGB/RGB, and LAB/LAB. The in-place sibling, intents `2`/`3`, and
nonzero flags remain rejected with `-3`. Release x64 builds with zero warnings/
errors; source/DLL exports remain `413/413`, zero set difference, and DLL
SHA-256 is
`1CECC50202ACA44A144614F6CCB64E5EE387EEC0712FDA15DC0F529C3127C1CB`.

`META-003T` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts relative-colorimetric
rendering intent `1` for allocating RGB/RGB as well as the two mode-changing
pairs. U above subsequently adds LAB/LAB; the in-place sibling, intents `2`/`3`,
and nonzero flags remain rejected with `-3`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `413/413`, zero set difference, and DLL SHA-256 is
`42A4AD167F1F5F1522EA6FD9748A455F37A1A2E053DB0F30A4EA9DB8AB698EC5`.

`META-003S` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts relative-colorimetric
rendering intent `1` for both allocating mode-changing pairs: RGB/sRGB-to-LAB/
LAB and LAB/LAB-to-RGB/sRGB. T above subsequently adds RGB/RGB; LAB/LAB
one-shot, the in-place sibling, intents `2`/`3`, and nonzero flags remain
rejected with `-3`. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`F485C52DA03EC7E5C12366F0261A8A734CE5EDDAEE369B0ED326709B0564CD68`.

`META-003R` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts relative-colorimetric
rendering intent `1` initially for the allocating RGB/sRGB-to-LAB/LAB pair
while still requiring flags `0`; S above adds the reverse mode-changing pair.
Same-mode one-shot pairs, the in-place sibling, intents `2`/`3`, and nonzero
flags remain rejected with `-3`. Release x64 builds with
zero warnings/errors; source/DLL exports remain `413/413`, zero set difference,
and DLL SHA-256 is
`95C5C6849BAEA28D26CA37E40EF7E49D3FEFE163D32248B1C234E64D1348A5E3`.

`META-003Q` changes no signature or export count. Existing
`pillow_c_cms_transform_build` now accepts rendering intent `0` or `1` for the
established RGB/sRGB-to-LAB/LAB reusable transform pair while still requiring
flags `0`. Other intents and nonzero flags remain rejected with `-3`; Q left
one-shot profile-to-profile exports intent-`0`-only, and R above separately
opens allocating RGB-to-LAB intent `1`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `413/413`, zero set difference, and
DLL SHA-256 is
`7A31411CD5CBA65AC69018274FF0F438BB00115E42AFFD00242258860A67D4E2`.

`META-003P` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image_in_place` now accepts LAB/LAB source/
output mode and LAB profile spaces, selects `TYPE_Lab_8` for both input and
output, and performs same-storage row traversal inside one temporary transform.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`413/413`, zero set difference, and DLL SHA-256 is
`21FB8FCB5790DE8AEFDE7A80A03585055154B3DE199BF15726F2F5E8752A4818`.

`META-003O` changes no signature or export count. Existing
`pillow_c_cms_transform_apply_in_place` now accepts an established LAB/LAB
transform and matching LAB image in addition to RGB/RGB, detaches/refreshes
readonly Buffer-backed storage, and runs all rows with identical input/output
pointers. Mode-changing pairs remain `-3`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `413/413`, zero set difference, and
DLL SHA-256 is
`346A02F825C6D84995D0B61E48DCA0BFE903CF6EDCFACD8C48F9F01C5F0435AD`.

`META-003N` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts LAB/LAB source/output mode
plus LAB-profile pairs, selects `TYPE_Lab_8 -> TYPE_Lab_8`, allocates a new
owned LAB image, and traverses all rows while preserving source storage.
Perceptual intent and flags `0` remain the only accepted options. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`05D52211CC02AD3211E4BC530F67F4CFB5A48FE737636EAA346CD1B3D4212FBA`.

`META-003M` changes no signature or export count. Reusable transform build/apply
now accepts LAB/LAB mode plus LAB-profile pairs, selects
`TYPE_Lab_8 -> TYPE_Lab_8`, retains output profile bytes after profiles close,
and allocates owned LAB results for repeat native row traversal. Release x64
builds with zero warnings/errors; source/DLL exports remain `413/413`, zero set
difference, and DLL SHA-256 is
`DB72B63D489E28B8E97BBFEDD22BF6F389407903AC052C83D7F8E3F96833F551`.

`META-003L` adds
`pillow_c_cms_profile_to_profile_image_in_place(image, input, output,
output_mode, intent, flags)`. The bounded ABI accepts only RGB/RGB source/mode
and RGB profile spaces with perceptual intent/flags `0`, creates one temporary
LittleCMS transform, detaches/refreshes caller storage, executes every row with
identical input/output pointers, and deletes transform state before return.
Release x64 builds with zero warnings/errors; source/DLL exports are `413/413`,
zero set difference, and DLL SHA-256 is
`E000DE6664FA98AE1293BC6C2713EB2971A9C70279B70474706F618870CA5C8C`.

`META-003K` changes no signature or export count. Existing
`pillow_c_cms_profile_to_profile_image` now accepts RGB/RGB source/output mode
plus RGB-profile pairs, selects `TYPE_RGB_8 -> TYPE_RGB_8`, allocates a new
owned RGB image, and traverses all rows while preserving source storage.
Perceptual intent and flags `0` remain the only accepted options. Release x64
builds with zero warnings/errors; source/DLL exports remain `412/412`, zero set
difference, and DLL SHA-256 is
`435763AE8D4B0A7EEDA62A9073B7A983E8155624E44022F70012B8F86B33BF99`.

`META-003J` adds `pillow_c_cms_transform_apply_in_place(transform, image)`.
It accepts only an established RGB/RGB transform and matching RGB image,
returns `-1` for null handles and `-3` for mode-changing/mismatched pairs,
detaches and refreshes readonly Buffer-backed storage, then calls LittleCMS
for every row with identical input/output pointers. It allocates no image and
retains the caller handle. Release x64 builds with zero warnings/errors;
source/DLL exports are `412/412`, zero set difference, and DLL SHA-256 is
`75898676451C19C6DFC8F4D57AE94F2211F2DA0F8DFDBE132D80F43A9BE67613`.

`META-003I` changes no signature or export count. Reusable transform build now
accepts RGB/RGB mode plus RGB-profile pairs, selects
`TYPE_RGB_8 -> TYPE_RGB_8`, and retains the serialized output ICC profile after
built-in/memory-opened profiles and the caller ICC Buffer are released. The
existing allocating apply validates the same-mode pair, creates an owned RGB
image, and executes every row through LittleCMS. Release x64 builds with zero
warnings/errors; source/DLL exports remain `411/411`, zero set difference, and
DLL SHA-256 is
`588BE122B6341DE05A58AA2C298104D002C77C78DDA70311DBBE7DD7D6DDDD2E`.

`META-003H` changes no signature or export count. The existing reusable
transform build/apply ABI now accepts the exact LAB/LAB-to-RGB/sRGB pair with
perceptual intent and flags `0`, selecting `TYPE_Lab_8 -> TYPE_RGB_8` and
retaining the serialized 588-byte sRGB output profile independently from both
input profiles. Apply validates the stored reverse pair, refreshes source
storage, allocates an owned RGB image, and traverses every row in native code.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`411/411`, zero set difference, and DLL SHA-256 is
`4F7A0035D4DB9E55206BB050AF8455754E923938456834A11C0B9EFD0FE36E29`.

`META-003G` adds four opaque reusable-transform symbols:
`pillow_c_cms_transform_build`, `pillow_c_cms_transform_apply`,
`pillow_c_cms_transform_output_profile_bytes`, and
`pillow_c_cms_transform_free`. Build accepts the bounded RGB/sRGB-to-LAB/LAB
pair with intent/flags `0`, creates one LittleCMS transform, and stores native
input/output mode ids plus serialized output ICC bytes. Profiles may close
immediately. Apply refreshes source storage, allocates an owned LAB image, and
runs every row through the retained transform. The output-profile query is a
two-pass bulk copy; free deletes LittleCMS and wrapper state. Release x64
builds with zero warnings/errors; source/DLL exports are `411/411`, zero set
difference, and DLL SHA-256 is
`F44BC7520328F2636BB6BFB0B7FBD00A2C42FE16983822DAE0CA53CD969DB9AD`.

`META-003F` adds `pillow_c_cms_profile_open_memory(data, size, out)`.
Null pointers return `-1`; zero or larger-than-LittleCMS lengths return `-2`;
invalid ICC data returns `-3`. Valid memory is opened with
`cmsOpenProfileFromMem` and passed to shared `own_cms_profile`. LittleCMS
copies read-mode memory, so the opaque handle is independent from caller
Buffer lifetime. Release x64 builds with zero warnings/errors; source/DLL
exports are `407/407`, zero set difference, and DLL SHA-256 is
`3EB41071735EAF2910A72F8654F3D27D57C3127A452339D19057D37D483479F1`.

`META-003E` changes no signature or export count. The existing
`pillow_c_cms_profile_to_profile_image` also accepts the exact reverse pair:
LAB source/internal storage plus built-in LAB input profile to RGB output plus
built-in sRGB output profile, with intent/flags `0`. It selects
`TYPE_Lab_8 -> TYPE_RGB_8`, returns an owned RGB image, and retains source
storage. Release x64 builds with zero warnings/errors; source/DLL exports
remain `406/406`, zero set difference, and DLL SHA-256 is
`55EB6CACE37F53BE334667529E4492FA23F64A50CE288D9CD853C7B82A802E6A`.

`META-003D` adds `pillow_c_cms_profile_to_profile_image` and
`pillow_c_cms_profile_bytes`. The bounded transform accepts an RGB image,
built-in sRGB input profile, built-in LAB output profile, output mode LAB,
perceptual intent `0`, and flags `0`; it rejects other combinations with
`-3`, refreshes attached readonly source storage, creates one LittleCMS
transform, and traverses every row in native code. The returned LAB handle
retains the existing internal LCMS a/b representation; public raw `LAB`
serialization performs the established XOR conversion. Profile bytes use a
two-pass `cmsSaveProfileToMem` contract and report exact required size without
a NUL. Release x64 builds with zero warnings/errors; source/DLL exports are
`406/406`, zero set difference, and DLL SHA-256 is
`E483B8741FE459F83950BE8C453B226953AB526796540A13EBE6D21A34E03085`.

`META-003C` adds `pillow_c_cms_profile_create_xyz`, which wraps
`cmsCreateXYZProfile` through shared `own_cms_profile`. The existing
two-pass name and free exports return exact `XYZ identity built-in\n` and
own the complete lifetime. Release x64 builds with zero warnings/errors;
source/DLL exports are `404/404`, with zero set difference, and DLL SHA-256
is `ECF493E6F7974F0130DB00CBE4215DFD4A0EC0A02C83BE022C98089218C4D0CD`.

`META-003B` adds
`pillow_c_cms_profile_create_lab(double color_temperature, ...)`. Non-finite
temperatures return `-3`; non-positive values request LittleCMS default D50;
positive values must pass `cmsWhitePointFromTemp` before
`cmsCreateLab2Profile`. Ownership is centralized in `own_cms_profile`, and
the A profile-name/free exports are reused unchanged. Default and 6500K
requests both expose exact `Lab identity built-in\n`. Release x64 builds
with zero warnings/errors; source/DLL exports are `403/403`, with zero set
difference, and DLL SHA-256 is
`64B5497BC1A23DD9C062B15587A14E44F7C85D91703CFC8055BDA9C8621826FB`.

`META-003A` adds three public ImageCms lifecycle symbols:
`pillow_c_cms_profile_create_srgb`, `pillow_c_cms_profile_name`, and
`pillow_c_cms_profile_free`. The create call returns a DLL-owned opaque
`PillowCCmsProfile` wrapping LittleCMS 2.17 `cmsHPROFILE`. The name call uses a
two-pass UTF-8 buffer contract, returns `PILLOW_C_OK` for a null output probe,
and includes the trailing NUL in `out_required`; the sRGB profile yields exact
public text `sRGB built-in\n`. The free call closes the LittleCMS profile and
deletes the opaque wrapper. Release x64 builds with zero warnings/errors;
source/DLL exports are `402/402`, with zero set difference, and DLL SHA-256 is
`05F61A2793F56B7673D6EDD4FD944D48D84CE2DFAAD06AF50CFC1A85421BC1E0`.

`FMT-TIFF-001AX` adds no symbol. The metadata-ASCII export accepts one
uncompressed `I;16B` frame with DPI, ICC, XMP, and tag `315`; output is 550
bytes with DPI `194/202`, XMP `210`, ICC `534`, strip `542`. Release is clean,
exports `399/399`, SHA-256
`053B555079CFFEDB4E0E75C350CF683BF821A082DCAE825C6CEB1879D4673E59`.

`FMT-TIFF-001AW` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_options` accepts one
uncompressed `I;16B` frame with DPI, non-empty ICC/XMP, tag `270`, and no
Artist. It emits 556 bytes with Description `194`, DPI `200/208`, XMP `216`,
ICC `540`, strip `548`, and reopens as `I;16B`. Release x64 built with zero
warnings/errors; exports remain `399/399`; SHA-256 is
`4B06A3C27570CF35284C1AC1916A2B42EA13ECCB28CFDFF1BC8D999BF3F7773C`.

`FMT-TIFF-001AV` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_entries_options` now accepts
one uncompressed `I;16B` frame with DPI, non-empty XMP, the exact validated
ASCII set `{270,315}`, and no ICC. The writer emits a 548-byte file with 15
sorted IFD entries, external `Hello\0` at offset `194`, inline `Ada\0`, DPI
offsets `200/208`, BYTE XMP offset `216`, raw strip offset `540`, and reopened
mode `I;16B`. The shared XMP+ASCII admission remains constrained to
uncompressed single-frame output and only the validated one/two-tag set.
Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL exports remain
`399/399`, and DLL SHA-256 is
`B4EF7519F3E16437EEB4A90224281A888D556DD3C54FF12D30334141C6919B28`.

`FMT-TIFF-001AU` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_options` now accepts one
uncompressed `I;16B` frame with DPI, non-empty XMP, one validated ASCII tag
`270` or `315`, and no ICC. The Artist fixture emits a 530-byte file with 14
sorted IFD entries, inline `Ada\0`, DPI offsets `182/190`, BYTE XMP offset
`198`, raw strip offset `522`, and reopened mode `I;16B`. The shared
XMP+single-ASCII admission remains constrained to uncompressed single-frame
output. Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL
exports remain `399/399`, and DLL SHA-256 is
`71B3A33D96CCBE4000477572ACCCA274542E2A65837C04B1C40048426A4AEB5E`.

`FMT-TIFF-001AT` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_options` now accepts one
uncompressed `I;16B` frame with DPI, non-empty XMP, one tag-270 ASCII value,
and no ICC. The writer emits a 536-byte file with 14 sorted IFD entries,
external `Hello\0` at offset `182`, DPI offsets `188/196`, BYTE XMP offset
`204`, raw strip offset `528`, and reopened mode `I;16B`. Admission remains
limited to this exact uncompressed single-frame configuration. Release x64
rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL exports remain `399/399`,
and DLL SHA-256 is
`E643014A8FF90F7DF552D598E052E359625FAEE16A0FA48DE12D6741DB6D32F0`.

`FMT-TIFF-001AS` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_entries_options` now accepts
one uncompressed `I;16B` frame with DPI, non-empty ICC, the exact validated
ASCII set `{270,315}`, and no XMP. The writer emits a 232-byte file with 15
sorted IFD entries, external `Hello\0`, inline `Ada\0`, DPI offsets `200/208`,
UNDEFINED ICC offset `216`, raw strip offset `224`, and reopened mode `I;16B`.
The shared ICC+ASCII admission remains constrained to uncompressed single-
frame output and only the validated one/two-tag set. Release x64 rebuilt with
`0 Warning(s), 0 Error(s)`; source/DLL exports remain `399/399`, and DLL
SHA-256 is
`B5A93E491DDEA00D8EC4C8DC34C39EE9434BBAFA0740071DEBFE9BDECC1E0377`.

`FMT-TIFF-001AR` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_options` now accepts one
uncompressed `I;16B` frame with DPI, non-empty ICC, one validated ASCII tag
`270` or `315`, and no XMP. The Artist fixture emits a 214-byte file with 14
sorted IFD entries, inline `Ada\0`, DPI offsets `182/190`, UNDEFINED ICC
offset `198`, raw strip offset `206`, and reopened mode `I;16B`. Admission is
still explicitly limited to uncompressed single-frame output; compressed and
other combinations remain rejected. Release x64 rebuilt with `0 Warning(s),
0 Error(s)`; source/DLL exports remain `399/399`, and DLL SHA-256 is
`815E6936CD5D7DC3F22268E35B3E7C144B31498654CB95D75C2DBEECF4940F43`.

`FMT-TIFF-001AQ` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_options` now accepts one
uncompressed `I;16B` frame with DPI, non-empty ICC, one tag-270 ASCII value,
and no XMP. The dedicated big-endian writer emits a 220-byte file with 14
sorted IFD entries, Description offset `182`, DPI offsets `188/196`, UNDEFINED
ICC offset `204`, raw strip offset `212`, and reopened mode `I;16B`. The
partial binary+ASCII admission is explicitly limited to this uncompressed
single-frame configuration; compressed and other combinations remain
rejected. Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL
exports remain `399/399`, and DLL SHA-256 is
`E556B39212C51BF8C5096DFD25CE04F2070863FD7FD1E58289554CB298B7850E`.

`FMT-TIFF-001AP` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ex_options` now accepts one
uncompressed `I;16B` frame with DPI, non-empty ICC and XMP, and no ASCII. The
dedicated big-endian writer emits a 538-byte file with 14 sorted IFD entries,
DPI offsets `182/190`, BYTE XMP offset `198`, UNDEFINED ICC offset `522`, raw
strip offset `530`, and reopened mode `I;16B`. Binary+ASCII combinations and
uncompressed multiframe remain rejected. Release x64 rebuilt with `0
Warning(s), 0 Error(s)`; source/DLL exports remain `399/399`, and DLL SHA-256
is `44F8A1B59900DD92247DF2A31E56B1B2263D9FC2D4CCE45837A3145840202683`.

`FMT-TIFF-001AO` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_entries_options` now accepts
one uncompressed `I;16B` frame with DPI, exact ASCII tag set `{270,315}`, and
no ICC/XMP. The dedicated big-endian writer emits a 212-byte file with 14
sorted IFD entries, external `Hello\0`, inline `Ada\0`, DPI offsets `188/196`,
raw strip offset `204`, and reopened mode `I;16B`. Binary+ASCII combinations
and uncompressed multiframe remain rejected. Release x64 rebuilt with `0
Warning(s), 0 Error(s)`; source/DLL exports remain `399/399`, and DLL SHA-256
is `03D69209EF9EB07964A9B483FC0018B4DFA966A1DF130EA6CCF9C650BB8391B7`.

`FMT-TIFF-001AN` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_options` now accepts one
uncompressed `I;16B` frame with DPI, one tag-315 ASCII value, and no ICC/XMP.
The dedicated big-endian writer emits a 194-byte file with 13 sorted IFD
entries, inline `Ada\0`, DPI offsets `170/178`, raw strip offset `186`, and
reopened mode `I;16B`. Other ASCII combinations and uncompressed multiframe
remain rejected. Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/
DLL exports remain `399/399`, and DLL SHA-256 is
`4D501BCA2D12C97AA656339A8AE2BC60066303C2DECCAAB9BB2AFB2AD89C9011`.

`FMT-TIFF-001AM` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_options` now accepts one
uncompressed `I;16B` frame with DPI, one tag-270 ASCII value, and no ICC/XMP.
The dedicated big-endian writer emits a 200-byte file with 13 sorted IFD
entries, Description offset `170`, DPI offsets `176/184`, raw strip offset
`192`, and reopened mode `I;16B`. Other ASCII subsets and uncompressed
multiframe remain rejected. Release x64 rebuilt with `0 Warning(s), 0
Error(s)`; source/DLL exports remain `399/399`, and DLL SHA-256 is
`94ED2CE0CA2151A47DA4C2BFDE0ED922D717908E4BE4FD384E7F7A812C46F6DD`.

`FMT-TIFF-001AL` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_options` now accepts one
uncompressed `I;16B` frame with DPI, non-empty XMP, and no ICC/ASCII. The
dedicated big-endian writer emits a 518-byte file with 13 sorted IFD entries,
DPI offsets `170/178`, BYTE XMP offset `186`, raw strip offset `510`, and
reopened mode `I;16B`. Other metadata subsets and uncompressed multiframe
remain rejected. Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/
DLL exports remain `399/399`, and DLL SHA-256 is
`6E523F767A679733489B27668FBF5AEB4278A5E57C02C63655F47FF154AD4AE8`.

`FMT-TIFF-001AK` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_options` now accepts one
uncompressed `I;16B` frame with DPI, non-empty ICC, and no XMP/ASCII. The
dedicated big-endian writer emits a 202-byte file with 13 sorted IFD entries,
DPI offsets `170/178`, UNDEFINED ICC offset `186`, raw strip offset `194`, and
reopened mode `I;16B`. Other metadata subsets and uncompressed multiframe
remain rejected. Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/
DLL exports remain `399/399`, and DLL SHA-256 is
`E2759F99E0F9B50F1ADCFCAF0B2BB85D2DF7D06A0EBE29BED43BE27EDF097BB3`.

`FMT-TIFF-001AJ` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_options` and the shared frames-options route now
accept one uncompressed `I;16B` frame with DPI and no ICC/XMP/ASCII payloads.
The dedicated big-endian writer emits a 182-byte file with 12 sorted IFD
entries, RATIONAL offsets `158/166`, raw strip offset `174`, and reopened mode
`I;16B`. Other partial metadata subsets and uncompressed multiframe remain
rejected. Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL
exports remain `399/399`, and DLL SHA-256 is
`62A84B20D6EC34EFFB944B0A7E73303496FC8A1AD395511F7CA5B98321977598`.

`FMT-TIFF-001AI` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_entries_options` now accepts
one uncompressed `I;16B` frame only for either the existing bare save or the
exact DPI+ICC+XMP+two-ASCII composition. The dedicated big-endian writer now
emits a 16-entry IFD, external Description/DPI/XMP/ICC payloads, and an exact
raw big-endian strip; reopen remains `I;16B`. Other uncompressed metadata
subsets and multiframe saves remain rejected. Release x64 rebuilt with
`0 Warning(s), 0 Error(s)`; source/DLL exports remain `399/399`, and DLL
SHA-256 is
`8D23C9F10BCA13E3AA63C9D703C53D70127E2F8D9AE1E128474D01845D7E6CA9`.

`FMT-TIFF-001AH` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_entries_options` now accepts
the exact compressed-I16B full composition of two validated ASCII entries,
non-empty ICC, and non-empty XMP. The covered two-frame LZW+DPI route writes
exact type/count/payload layouts for tags `270`, `315`, `700`, and `34675` in
both IFDs while preserving caller modes/bytes. Partial metadata+ASCII,
uncompressed metadata, and other counts/tags remain rejected. Release x64
rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL exports remain `399/399`,
and DLL SHA-256 is
`CA98C6A7BA635438C202FBC89EE7F341204E4995313FA2A2424BE437693D2063`.

`FMT-TIFF-001AG` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_entries_options` now accepts
either one valid tag `270`/`315` or the exact de-duplicated two-entry set
`{270,315}` through compressed `I;16B`/`I;16` temporary normalization. The
covered two-frame LZW+DPI route writes exact out-of-line `Hello\0` and inline
`Ada\0` values in both IFDs while preserving caller modes/bytes. ICC/XMP+
ASCII, other counts/tags, and uncompressed metadata remain rejected. Release
x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL exports remain
`399/399`, and DLL SHA-256 is
`703A00252FCEDF2C6244E6AEBFC18E9C0891A0A394C895EC046BFE0DB3F77067`.

`FMT-TIFF-001AF` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_options` now accepts exactly
one ASCII entry whose tag is either `270` or `315`, with ICC/XMP absent,
through compressed `I;16B`/`I;16` temporary normalization. The covered Artist
route writes inline type `2`, count `4`, exact `Ada\0` in both IFDs and
preserves caller modes/bytes. Multiple ASCII entries, metadata+ASCII, and
uncompressed metadata remain rejected. Release x64 rebuilt with `0 Warning(s),
0 Error(s)`; source/DLL exports remain `399/399`, and DLL SHA-256 is
`36F2542CA039D955BFA5B47D1BD41A3C31380092BE9DDEFD65E02AE3A7514F30`.

`FMT-TIFF-001AE` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ascii_options` now carries exactly
one tag-`270` ASCII value, with ICC/XMP absent, through compressed `I;16B`/
`I;16` temporary normalization into the shared writer. The covered two-frame
LZW+DPI route writes type `2`, count `6`, exact `Hello\0` in both IFDs and
preserves caller modes/bytes. Tag `315` is admitted by AF; multiple ASCII
entries and metadata+ASCII remain rejected. The uncompressed `I;16B` writer now explicitly rejects
all metadata rather than omitting it. Release x64 rebuilt with `0 Warning(s),
0 Error(s)`; source/DLL exports remain `399/399`, and DLL SHA-256 is
`A80222381976DA923E3EE4EFDF3FD8D1DB1E30FD61B002C4C0E472B800372FB0`.

`FMT-TIFF-001AD` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ex_options` now carries simultaneous
valid ICC and XMP payloads through the compressed `I;16B`/`I;16` temporary-
normalization branch into the shared TIFF writer. The compatibility-covered
route is a homogeneous two-frame `I;16B` pair with LZW and DPI `(300,150)`;
both IFDs contain exact BYTE tag `700` and UNDEFINED tag `34675`, and caller
modes/bytes remain unchanged. Tag-270-only ASCII is admitted by AE; other
ASCII and uncompressed multiframe/DPI expansion remain rejected. Release x64
rebuilt with `0 Warning(s), 0 Error(s)`; source/
DLL exports remain `399/399`, and DLL SHA-256 is
`629DD662C6ACDCA4A8805493FF0ED866274533D370279DBF05C9D06CD84D3AEA`.

`FMT-TIFF-001AC` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_ex_options` now carries a valid
XMP-only payload through the compressed `I;16B`/`I;16` temporary-normalization
branch into the shared TIFF writer. The compatibility-covered route is a
homogeneous two-frame `I;16B` pair with LZW and DPI `(300,150)`; both IFDs
contain exact BYTE tag `700`, and caller modes/bytes remain unchanged.
Simultaneous ICC+XMP is admitted by AD; tag-270-only ASCII by AE; other ASCII and uncompressed
multiframe/DPI expansion remain rejected. Release x64 rebuilt with
`0 Warning(s), 0 Error(s)`; source/DLL
exports remain `399/399`, and DLL SHA-256 is
`29770E7CAC1A59C4D6455405A9A7A39F54A8DFD517E4D5D6A607F9D9C4C7365A`.

`FMT-TIFF-001AB` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_metadata_options` now carries a valid ICC
payload through the compressed `I;16B`/`I;16` temporary-normalization branch
into the shared TIFF writer. The compatibility-covered route is a homogeneous
two-frame `I;16B` pair with LZW and DPI `(300,150)`; both IFDs contain exact
UNDEFINED tag `34675`, and caller modes/bytes remain unchanged. XMP-only is
admitted by AC, combined composition by AD, and tag-270-only ASCII by AE; other ASCII and
uncompressed multiframe/DPI expansion remain rejected. Release x64 rebuilt
with `0 Warning(s), 0 Error(s)`; source/DLL exports remain `399/399`, and DLL
SHA-256 is
`58D1896D7F7C6B99AB68D2F1BB1D8D027512F011BA7DEC793E9C90D92990E30F`.

`FMT-TIFF-001AA` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_options` now accepts compressed frame arrays
whose members are any mixture of `I;16` and `I;16B`. The native temporary-copy
route swaps only `I;16B` members, normalizes every delegated frame to `I;16`,
and preserves caller modes/bytes. Other mixed mode families, uncompressed
multiframe/DPI expansion and non-270 ASCII remain rejected; ICC/XMP-only,
combined metadata, and tag-270 ASCII are admitted by AB/AC/AD/AE. Release x64 rebuilt
with `0 Warning(s), 0 Error(s)`; source/DLL exports remain `399/399`, and DLL
SHA-256 is
`3C90BAEA740D3DA52D91891777F6E39E3994A57B1752584C362D5EBF8B4ECF2F`.

`FMT-TIFF-001Z` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_options` now accepts homogeneous compressed
mode `I;16B` frame arrays with optional DPI. Native creates one temporary
little-endian `I;16` copy per frame, preserving caller modes/bytes, then reuses
the shared PackBits/LZW/Adobe Deflate and IFD/DPI writer. Mixed-endian arrays,
uncompressed multiframe/DPI expansion and non-270 ASCII remain rejected; ICC/
XMP-only, combined metadata, and tag-270 ASCII are admitted by AB/AC/AD/AE.
Release x64 rebuilt with
`0 Warning(s), 0 Error(s)`; source/DLL
exports remain `399/399`, and DLL SHA-256 is
`395E0D5D1CE3394556031B6C25C5A77FA380361927D26DC4A3064568278072CB`.

`FMT-TIFF-001Y` adds no symbol or signature. Existing
`pillow_c_image_save_tiff_frames_options` composes mode `I` or `F`, DPI, and
compression `32773`, `5`, or `8` for both frames. The native PackBits writer
now follows libtiff's state strategy for two-byte runs and
literal-run-literal coalescing, making the bounded numeric PackBits strips
byte-identical to Pillow. LZW strips are also exact; native Deflate keeps
valid 27-byte stored blocks while all routes preserve exact samples, no
Predictor, and DPI. Release x64 rebuilt with `0 Warning(s), 0 Error(s)`;
source/DLL exports remain `399/399`, and DLL SHA-256 is
`F877A0057B8D54ACD2DD11A32622BDCE37BD22D185E222862E9B5AB00E2E7CBB`.

`FMT-TIFF-001X` adds no symbol or implementation change. Existing
`pillow_c_image_save_tiff_frames_options` composes mode `I;16`, DPI, and
compression `5` or `8` for both frames. LZW strips match Pillow at 12 bytes;
native Deflate uses valid 19-byte stored blocks versus Pillow's 16-byte
streams, with exact decoded samples and DPI. No rebuild was required;
source/DLL exports remain `399/399`, and DLL SHA-256 remains
`A61C6E56CA121828C29F19222157E950924D9BC2290BFAC2737E756409E003F5`.

`FMT-TIFF-001W` adds no symbol or implementation change. Existing
`pillow_c_image_save_tiff_frames_options` composes mode `I;16`, compression
`32773`, and DPI for both frames; existing open/frame and metadata-resolution
exports preserve exact samples and DPI. No rebuild was required; source/DLL
exports remain `399/399`, and DLL SHA-256 remains
`A61C6E56CA121828C29F19222157E950924D9BC2290BFAC2737E756409E003F5`.

`FMT-TIFF-001V` adds no symbol. The early frame-0 numeric branch in
`open_tiff_frame_image` now calls `parse_tiff_resolution` before returning
mode `I` or `F` handles. Existing `pillow_c_image_metadata_resolution`
therefore reports IFD0 DPI on both initial numeric handles. Release x64
rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL exports remain `399/399`,
and DLL SHA-256 is
`A61C6E56CA121828C29F19222157E950924D9BC2290BFAC2737E756409E003F5`.

`FMT-TIFF-001U` adds no symbol. The early frame-0 `I;16` branch in
`open_tiff_frame_image` now calls `parse_tiff_resolution` before returning,
so existing `pillow_c_image_metadata_resolution` reports IFD0 DPI on the
initial native handle. Release x64 rebuilt with `0 Warning(s), 0 Error(s)`;
source/DLL exports remain `399/399`, and DLL SHA-256 is
`0C23441D2E57C4300FEB6025D3A3DDF9F7C21786618928D30705246EBF5581FE`.

`FMT-TIFF-001T` adds no symbol. The early selected-frame numeric branch in
`open_tiff_frame_image` now calls `parse_tiff_resolution_for_ifd` before
returning mode `I` or `F` handles. Existing
`pillow_c_image_metadata_resolution` therefore reports DPI for both numeric
selected-frame modes, while their exact four-byte samples remain unchanged.
Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL exports remain
`399/399`, and DLL SHA-256 is
`2AA796C0723CB49838E4D899D42BF9725F00F4A1F603659C0595A9EFF32040D4`.

`FMT-TIFF-001S` adds no symbol. The early selected-frame `I;16` branch in
`open_tiff_frame_image` now calls `parse_tiff_resolution_for_ifd` before
returning its native handle, so the existing
`pillow_c_image_metadata_resolution` export reports DPI for nonzero `I;16`
IFDs. The writer and facade continue to use the existing generalized
multiframe options ABI. Release x64 rebuilt with `0 Warning(s), 0 Error(s)`;
source/DLL exports remain `399/399`, and DLL SHA-256 is
`906397FBD41F7BA0CB19A2B6A08BCF3582CF419C80E1041A47C366D6C447DA20`.

`FMT-TIFF-001R` adds no symbol. WIC-backed `open_tiff_frame_image` now parses
X/YResolution and ResolutionUnit from the selected IFD instead of populating
`PillowCImage.has_dpi` only for frame `0`; existing
`pillow_c_image_metadata_resolution` therefore reports DPI for selected RGB
TIFF frames. The generalized ASCII-entry export simultaneously composes LZW,
DPI, ICC, XMP, and tags `270`/`315`. Release x64 rebuilt with `0 Warning(s),
0 Error(s)`; source/DLL exports remain `399/399`, and DLL SHA-256 is
`6E658A0051D5E8BF7346691872B39A176B1C13F7DB1F0B2F8CF2498B2DFF7997`.

`FMT-TIFF-001Q` adds
`pillow_c_image_save_tiff_frames_metadata_ascii_entries_options(images,
image_count, path, has_dpi, dpi_x, dpi_y, compression, icc_profile,
icc_profile_size, xmp, xmp_size, ascii_tags, ascii_values, ascii_sizes,
ascii_count)`. The bounded array route requires one or two unique entries,
accepts tags `270` and `315`, requires non-null NUL-terminated payloads with
32-bit-representable nonzero sizes, and composes optional ICC/XMP plus existing
DPI/compression. Native sorts by tag, owns every IFD entry/count/offset,
handles inline and out-of-line payloads independently, and writes the arrays
to every frame. The singular export remains and adapts to the same core.
Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL exports are
`399/399`, and DLL SHA-256 is
`DB5C38C111E322C1BDBB66B9643C2E85CF54C1A3BC991BA5D1AFDF98CBA4291A`.

`FMT-TIFF-001P` adds no symbol. The existing
`pillow_c_image_save_tiff_frames_metadata_ascii_options` bounded `ascii_tag`
set now includes `315` (`Artist`) in addition to `270` (`ImageDescription`).
The existing inline branch stores the four NUL-inclusive bytes for `"Ada"`
directly in each IFD value field. Release x64 rebuilt with `0 Warning(s),
0 Error(s)`; source/DLL exports remain `398/398`, and DLL SHA-256 is
`D0B49F66FA232749D7E22AAB39D2CEED838D7B48AF01D4E890A1F1F4A2401B0F`.

`FMT-TIFF-001O` adds
`pillow_c_image_save_tiff_frames_metadata_ascii_options(images, image_count,
path, has_dpi, dpi_x, dpi_y, compression, icc_profile, icc_profile_size, xmp,
xmp_size, ascii_tag, ascii_value, ascii_size)`. The bounded ASCII route accepts
tag `270`, a non-null nonempty payload whose final byte is NUL, and a size that
fits TIFF's 32-bit count. It writes TIFF ASCII type `2` into every IFD, stores
payloads up to four bytes inline and larger payloads at native-owned offsets,
and composes optional ICC/XMP plus the existing DPI/compression options.
Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL exports are
`398/398`, and DLL SHA-256 is
`95E388362A7930CDC95CA49B03C7F4867DB0AE983D32803D322D9601005E9C85`.

`FMT-TIFF-001N` adds no export or implementation change. Existing
`pillow_c_image_save_tiff_frames_metadata_ex_options` accepts simultaneous
nonempty ICC and XMP payloads and writes both in every IFD. No rebuild was
required; source/DLL exports remain `397/397`, and DLL SHA-256 remains
`D2F771BCFDD56D6EB70993FCC503DD60E77350CCFC1EC61A998D80CD6DF50EBB`.

`FMT-TIFF-001M` adds
`pillow_c_image_save_tiff_frames_metadata_ex_options(images, image_count,
path, has_dpi, dpi_x, dpi_y, compression, icc_profile, icc_profile_size, xmp,
xmp_size)`. At least one metadata payload must be nonempty; nonempty payloads
require non-null pointers and 32-bit-representable sizes. Existing DPI and
compression rules apply. XMP is written as TIFF BYTE tag `700` in every IFD;
ICC remains UNDEFINED tag `34675`. Release x64 built with `0 Warning(s),
0 Error(s)`; source/DLL exports are `397/397`, and DLL SHA-256 is
`D2F771BCFDD56D6EB70993FCC503DD60E77350CCFC1EC61A998D80CD6DF50EBB`.

`FMT-TIFF-001L` adds
`pillow_c_image_save_tiff_frames_metadata_options(images, image_count, path,
has_dpi, dpi_x, dpi_y, compression, icc_profile, icc_profile_size)`. Images,
path, and a non-null/nonempty ICC payload are required. DPI and compression
use the existing multiframe rules. Native writes ICC bytes as TIFF UNDEFINED
tag `34675` in every IFD; payloads up to four bytes are inline and larger
payloads use validated 32-bit offsets. Release x64 built with `0 Warning(s),
0 Error(s)`; source/DLL exports are `396/396`, and DLL SHA-256 is
`684BF0C50053C043CD3127C22C0772F907C94F440589C429D6A7BAE14DC25D0C`.

`FMT-TIFF-001K` adds no export or implementation change. Existing
`pillow_c_image_save_tiff_frames_options` composes `has_dpi=1` with compression
`5` or `8` for every frame. LZW uses 14-byte strips; native Deflate keeps its
valid 23-byte `0x78 0x9C` stored-block representation while decoded bytes and
DPI tags match Pillow. No rebuild was required; source/DLL exports remain
`395/395`, and DLL SHA-256 remains
`F347533B35A6E6AE3243247AEF5639F7C24CDBB8CDF5062987ABB61A96C7832F`.

`FMT-TIFF-001J` adds no export or implementation change. Existing
`pillow_c_image_save_tiff_frames_options` accepts `has_dpi=1`, positive
`dpi_x`/`dpi_y`, and compression `32773` in one call. Every frame receives
PackBits strip encoding plus X/YResolution and ResolutionUnit tags. No rebuild
was required; source/DLL exports remain `395/395`, and DLL SHA-256 remains
`F347533B35A6E6AE3243247AEF5639F7C24CDBB8CDF5062987ABB61A96C7832F`.

`FMT-TIFF-001I` adds
`pillow_c_image_save_tiff_frames_options(images, image_count, path, has_dpi,
dpi_x, dpi_y, compression)`. `images` and `path` are required and
`image_count` must be nonzero. When `has_dpi != 0`, both DPI values must be
positive finite numbers; native writes XResolution, YResolution, and
ResolutionUnit `2` into every frame IFD. Compression uses the existing native
values `0`/`1`, `32773`, `5`, and `8`. Release x64 rebuilt with `0 Warning(s),
0 Error(s)`; source/DLL exports are `395/395`, and DLL SHA-256 is
`F347533B35A6E6AE3243247AEF5639F7C24CDBB8CDF5062987ABB61A96C7832F`.

`FMT-TIFF-001H` adds no export or implementation change. Existing
`pillow_c_image_save_tiff_frames_compression_options` accepts compression `5`
and `8` for two-frame LZW and Adobe Deflate. Both preserve exact decoded
bytes. LZW emits 14-byte strips matching the bounded Pillow fixture; native
Deflate emits valid `0x78 0x9C` zlib stored blocks of 23 bytes while Pillow's
compressor emits 20 bytes, with broad compressed-byte parity still excluded.
No rebuild was required; source/DLL exports remain `394/394`, and DLL SHA-256
remains `2C5F6E6929FB0900F8E70D2FF5F2E6095A6D242ACCAE395B6318F7D5A1FFF29F`.

`FMT-TIFF-001G` adds
`pillow_c_image_save_tiff_frames_compression_options(images, image_count,
path, compression)`. `images` and `path` are required; `image_count` must be
nonzero. Compression accepts native values `0`/`1` (none), `32773` (PackBits),
`5` (LZW), and `8` (Adobe Deflate); unsupported values return `-3`. Each image
is validated by the existing TIFF save path. The export currently has no DPI
parameters and delegates encoding/layout to the existing multiframe writer.
Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL exports are
`394/394`, and DLL SHA-256 is
`2C5F6E6929FB0900F8E70D2FF5F2E6095A6D242ACCAE395B6318F7D5A1FFF29F`.

`FMT-TIFF-001F` adds no export or implementation change. The existing
`pillow_c_image_save_tiff_frames` ABI accepts one RGB base plus two append
handles, emits two nonzero next-IFD links followed by a zero terminator, and
preserves three exact strips. Existing frame-count/open APIs expose all three
frames. No rebuild was required; source/DLL exports remain `393/393`, and DLL
SHA-256 remains
`2189246D2C8C390C236DD4595A909BF72084DF8448C430A1216FC095A2D3A06A`.

`FMT-TIFF-001E` adds no export or implementation change. The existing
`pillow_c_image_save_tiff_frames` ABI accepts an RGB `2x1` base plus RGB `1x2`
append frame and emits independent dimensions, RowsPerStrip values, strip
offsets, and exact bytes. Existing open-frame APIs expose `2x1` then `1x2`.
No rebuild was required; source/DLL exports remain `393/393`, and DLL SHA-256
remains `2189246D2C8C390C236DD4595A909BF72084DF8448C430A1216FC095A2D3A06A`.

`FMT-TIFF-001D` changes TIFF bytes but adds no export or signature. For native
L-frame serialization, `pillow_c_image_save_tiff_frames` and the single-frame
save routes now omit SamplesPerPixel tag `277` and write PlanarConfiguration
tag `284` with value `1`, matching Pillow 11.3.0. A following RGB frame keeps
its independent RGB IFD and strip. Release x64 rebuilt with `0 Warning(s),
0 Error(s)`; source/DLL exports remain `393/393`, and DLL SHA-256 is
`2189246D2C8C390C236DD4595A909BF72084DF8448C430A1216FC095A2D3A06A`.

`FMT-TIFF-001C` adds no export or implementation change. The existing
`pillow_c_image_save_tiff_frames` ABI accepts the bounded same-size two-frame
RGBA pair and emits linked little-endian IFDs with four 8-bit samples,
ExtraSamples `2`, and exact eight-byte interleaved strips. Existing frame
count/open/seek APIs expose both RGBA frames. No native source changed and no
rebuild was required; source/DLL exports remain `393/393`, and DLL SHA-256
remains `BE715A737810DC3D43799A6775D4E629D3B0AB1582F7F6E06A22CF18C3B20EE3`.

`FMT-TIFF-001B` adds no export or implementation change. The existing
`pillow_c_image_save_tiff_frames(images, image_count, path)` ABI accepts the
bounded same-size two-frame RGB pair and emits linked little-endian RGB IFDs
with exact interleaved frame bytes. Existing frame-count/open APIs and facade
seek expose both frames. No native source changed and no rebuild was required;
source/DLL exports remain `393/393`, and DLL SHA-256 remains
`BE715A737810DC3D43799A6775D4E629D3B0AB1582F7F6E06A22CF18C3B20EE3`.

`FMT-JPEG-003AZ` changes ordinary Photoshop resource enumeration semantics
without adding or changing a signature. When recognized `8BIM` records repeat
the same ordinary resource code, native parser state preserves that code's
first enumeration position and replaces its bytes with the last encountered
value. Thus the existing count/index ABI exposes one entry for duplicate code
`0x0404`, carrying bytes `CD` for ordered values `AB`, `CD`, matching Pillow
11.3.0 before facade materialization. Both keep routes omit APP13. Release x64
rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL exports remain `393/393`
and DLL SHA-256 is
`BE715A737810DC3D43799A6775D4E629D3B0AB1582F7F6E06A22CF18C3B20EE3`.

`FMT-JPEG-003AY` adds no export or implementation change. Two ordered,
separately recognized Photoshop APP13 markers merge through the existing
metadata state: ordinary resource `0x0404` bytes `AB` from the first and
structured ResolutionInfo `0x03ED` from the second remain simultaneously
available. Both keep routes omit all APP13 markers. Source/DLL exports remain
`393/393` and DLL SHA-256 remains
`114E2F544DA1848E35D828EEC5246075C11753945B440A57ADC26360D76510B6`.

`FMT-JPEG-003AX` adds no export or implementation change. One recognized
Photoshop APP13 containing ordinary resource `0x0404` bytes `AB` followed by
structured ResolutionInfo `0x03ED` is exposed through the existing ordinary
count/index ABI and the structured scalar ABI simultaneously. Both keep
routes omit APP13. Source/DLL exports remain `393/393` and DLL SHA-256 remains
`114E2F544DA1848E35D828EEC5246075C11753945B440A57ADC26360D76510B6`.

`FMT-JPEG-003AW` adds
`pillow_c_image_metadata_jpeg_photoshop_resolution_info(image, out_has,
out_x_resolution, out_displayed_units_x, out_y_resolution,
out_displayed_units_y)`. Every pointer is required; a null pointer returns
`-1`. On success, `out_has` is `1` for a valid Photoshop `8BIM` resource code
`0x03ED` with at least 14 data bytes and `0` otherwise; this minimum was
corrected from 16 by `FMT-JPEG-003BB` because the last exposed field ends at
byte 13. The two resolutions are DLL-decoded unsigned 16.16 values returned as
`double`; displayed units are returned as integers. Absent metadata returns
zero-valued fields. The
structured resource remains excluded from the ordinary byte-resource
enumeration. Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; source/DLL
exports are `393/393` and DLL SHA-256 is
`114E2F544DA1848E35D828EEC5246075C11753945B440A57ADC26360D76510B6`.

`FMT-JPEG-003AV` adds
`pillow_c_image_metadata_jpeg_photoshop_resource_count(image, out_count)` and
`pillow_c_image_metadata_jpeg_photoshop_resource(image, index, out_code,
out_value, out_value_size, out_value_required)`. The first reports DLL-owned
ordinary Photoshop `8BIM` resources. The indexed export returns the integer
resource code and supports a null-buffer size query; an undersized buffer
returns `-2`, an invalid index returns `-3`, and required pointers return `-1`
when null. Native parsing owns Photoshop header/signature checks, Pascal-name
and data alignment, bounds, storage, and copy lifetime. Special ResolutionInfo
code `0x03ED` remains excluded from this byte route and is exposed by the
structured `FMT-JPEG-003AW` export. Release x64 rebuilt with `0 Warning(s),
0 Error(s)`; source/DLL exports are `392/392` and
DLL SHA-256 is
`1C0EB831B28942B7A9579B655DA77AAD258AF5CFE694CDBBE65D426A991F19FD`.

`FMT-JPEG-003AU` adds no ABI or implementation change. Existing JPEG parsing
ignores unknown APP13 metadata and existing keep-normalized qtables encoding
omits it while preserving DQT, mode, and size. Exports/DLL remain `390/390`
and `5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

`META-002AE` adds no ABI or implementation change. Existing deferred ICC
count validation maps both `1/0:A + 2/0:B` and `1/255:A + 2/255:B` to state
`2`; the byte blob remains absent. Exports/DLL remain `390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

`META-002AD` adds no ABI or implementation change. Existing deferred ICC
count validation maps singleton `1/0:A` and `1/255:B` to state `2`; the byte
blob remains absent. Exports/DLL remain `390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

`META-002AC` adds no ABI or implementation change. Existing deferred ICC
count-based finalization accepts singleton `2/1:A` and `255/1:B` as state `1`;
the existing blob export returns bytes `A` / `B`. Exports/DLL remain `390/390`
and `5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

`META-002AB` adds no ABI or implementation change. Existing deferred ICC
sorting/count finalization accepts singleton `0/1:A` as state `1`, and the
existing blob export returns byte `A`, matching Pillow 11.3.0. Exports/DLL
remain `390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

`META-002AA` adds
`pillow_c_image_metadata_jpeg_icc_profile_state(image, out_state)`. It returns
state `0` when JPEG `info` has no ICC key, `1` when the existing profile blob
export owns bytes (including a legal zero-length profile), and `2` when Pillow
would expose `icc_profile=None` for a recognized incomplete marker set. Null
image/state pointers return `-1`. Parser metadata, image handles, and native
metadata copies preserve the state; the existing blob export keeps its binary
presence contract. Release x64 rebuilt with zero warnings/errors; exports are
`390/390` and DLL SHA-256 is
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

`META-002Z` adds no export or implementation change. Existing deferred ICC
marker storage preserves the zero-length middle fragment in `1/3:A`,
`2/3:empty`, `3/3:B`, and the finalizer exposes exact bytes `AB`. Exports/DLL
remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

`MODE-NUM-001CG` adds no export or implementation change. With border unset,
the existing raw ABI receives IEEE binary64 threshold `8.0`; the mode-aware
comparison admits zero neighbors at distances `7.0` / `7.25`, and the native
queue fills all three I/F samples in place. Exports/DLL remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

`MODE-NUM-001CF` adds no export or implementation change. With border unset,
the existing raw ABI receives IEEE binary64 thresholds `6.0` / `6.25`; the
mode-aware comparison rejects the zero neighbor whose scalar distance is
`7.0` / `7.25`, leaving the native queue with a seed-only I/F result. Exports/
DLL remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

`MODE-NUM-001CE` adds no export or implementation change. With border unset,
the existing raw ABI receives IEEE binary64 thresholds `7.0` / `7.25`; the
`MODE-NUM-001CC` mode-aware neighbor comparison admits the zero sample at
equality and the native queue fills all three I/F samples in place. Exports/DLL
remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

`MODE-NUM-001CD` adds no export or implementation change. The existing raw ABI
receives mode-sized packed scalar borders and IEEE binary64 thresholds `17.0`
/ `9.75`; the `MODE-NUM-001CC` mode-aware initial comparison returns before
mutation because scalar distances `16` / `8.75` are below those thresholds.
Exports/DLL remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

`MODE-NUM-001CC` changes implementation behind the existing
`pillow_c_image_draw_floodfill` export without changing its signature or adding
symbols. Numeric `I` and `F` initial and border-unset neighbor distances now
decode one signed-int32/float32 sample and compute scalar absolute difference,
instead of summing four storage-byte differences. Equality against per-mode
threshold `16.0` / `8.75` therefore returns before mutation, matching Pillow.
Release x64 rebuilt with `0 Warning(s), 0 Error(s)`; exports remain `389/389`
and DLL SHA-256 is
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

`MODE-NUM-001CB` adds no export or implementation change. The existing raw ABI
receives mode-sized packed scalar borders (`int32` `300`, `float32` `2.5`) and
IEEE binary64 `1.0` unchanged; DLL-owned supplied-border traversal fills all
I/F samples after the initial distances exceed the finite positive threshold,
while preserving allocation. Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001CA` adds no export or implementation change. The existing raw ABI
receives mode-sized packed scalar borders (`int32` `300`, `float32` `2.5`) and
IEEE binary64 `0.0` unchanged; DLL-owned supplied-border traversal fills all
I/F samples in place after the initial nonzero distance comparison while
preserving allocation. Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BZ` adds no export or implementation change. The existing raw ABI
receives mode-sized packed scalar borders (`int32` `300`, `float32` `2.5`) and
IEEE binary64 `-1.0` unchanged; DLL-owned supplied-border traversal fills all
I/F samples in place after the initial ordered comparison is false while
preserving allocation. Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BY` adds no export or implementation change. The existing raw ABI
receives mode-sized packed scalar borders (`int32` `300`, `float32` `2.5`) and
IEEE binary64 positive infinity unchanged; DLL-owned initial-distance
comparison returns before mutation or supplied-border traversal. Exports/DLL
remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BX` adds no export or implementation change. The existing raw ABI
receives mode-sized packed scalar borders (`int32` `300`, `float32` `2.5`) and
IEEE binary64 quiet NaN unchanged; DLL-owned supplied-border traversal fills
all I/F samples in place while preserving allocation. Exports/DLL remain
`389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BW` adds no export or implementation change. The existing raw ABI
receives mode-sized packed scalar borders (`int32` `300`, `float32` `2.5`) and
IEEE binary64 negative infinity unchanged; DLL-owned supplied-border traversal
fills all I/F samples in place while preserving allocation. Exports/DLL remain
`389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BV` adds no export or implementation change. Empty numeric Array
borders share BT/BU's live non-null border pointer with `border_size == 0`;
source Array length remains outside the ABI. The reused raw sentinel proof and
extended facade exact-byte proof pass unchanged with negative infinity,
completing this threshold's Array-shape matrix. Exports/DLL remain `389/389`
and `8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BU` adds no export or implementation change. One-element numeric
Array borders share BT's live non-null border pointer with `border_size == 0`;
source Array length remains outside the ABI. The reused raw sentinel proof and
extended facade exact-byte proof pass unchanged with negative infinity.
Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BT` adds no export or implementation change. Multi-element
numeric Array borders continue through the existing live non-null border
pointer with `border_size == 0`, while IEEE binary64 negative infinity reaches
the native `double thresh` unchanged. The supplied-incomparable-border branch
fills all I/F samples through DLL-owned traversal. Exports/DLL remain
`389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BS` adds no export or implementation change. Packed scalar border
bytes and IEEE negative infinity use the existing ABI; native code writes the
seed then stops on the matching border. Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BR` adds no export or implementation change. Empty numeric Array
borders share BQ/BP's live non-null border pointer with `border_size == 0`;
source Array length remains outside the ABI. The reused raw sentinel proof and
extended facade exact-byte proof pass unchanged with positive infinity,
completing the empty/one/multi-element shape matrix. Exports and DLL remain
`389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BQ` adds no export or implementation change. One-element numeric
Array borders share BP's live non-null border pointer with `border_size == 0`;
source Array length remains outside the ABI. The reused raw sentinel proof and
extended facade exact-byte proof pass unchanged with positive infinity. Exports
and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BP` adds no export or implementation change. Multi-element numeric
Array borders continue through the existing live non-null border pointer with
`border_size == 0`, while IEEE binary64 positive infinity reaches `double
thresh` unchanged. The native initial-distance comparison returns before seed
mutation or sentinel traversal; extended raw/facade exact-byte proofs pass
unchanged. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BO` adds no export or implementation change. Packed scalar border
bytes continue through the existing supplied-border parameters while IEEE
binary64 positive infinity reaches `double thresh` unchanged. The native
initial-distance comparison returns before seed mutation or border traversal;
extended raw/facade exact-byte proofs pass unchanged. Exports and DLL remain
`389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BN` adds no export or implementation change. Empty numeric Array
borders share BL/BM's live non-null border pointer with `border_size == 0`;
source Array length remains outside the ABI. The reused raw sentinel proof and
extended facade exact-byte proof pass unchanged with quiet NaN, completing the
empty/one/multi-element Array shape matrix. Exports and DLL remain `389/389`
and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BM` adds no export or implementation change. One-element numeric
Array borders share BL's live non-null border pointer with `border_size == 0`;
the source Array length is intentionally not encoded in the ABI. The reused
raw sentinel proof and extended facade exact-byte proof pass unchanged with
quiet NaN. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BL` adds no export or implementation change. Nonempty multi-
element numeric Array borders use the existing live non-null border pointer
with `border_size == 0`, while IEEE binary64 quiet NaN reaches `double thresh`
unchanged. The initial comparison writes the seed and native supplied-border
traversal fills all scalar samples because the sentinel is incomparable.
Extended raw/facade exact-byte proofs pass unchanged. Exports and DLL remain
`389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BK` adds no export or implementation change. Packed scalar border
bytes use the existing non-null supplied-border ABI while IEEE binary64 quiet
NaN reaches `double thresh` unchanged. The initial ordered comparison writes
the seed; matching signed-int32/float32 zero border samples then stop native
neighbor traversal. Extended raw/facade exact-byte proofs pass unchanged.
Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BJ` adds no export or implementation change. With border unset,
IEEE binary64 quiet NaN reaches the existing `double thresh` ABI unchanged.
All ordered initial and neighbor distance comparisons against NaN are false,
so native Floodfill writes only the seed. Extended raw/facade exact-byte proofs
pass unchanged. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BI` adds no export or implementation change. With border unset,
IEEE binary64 negative infinity reaches the existing `double thresh` ABI
unchanged. Neither finite initial nor neighbor distances are at most that
threshold, so native Floodfill writes only the seed. Extended raw/facade exact-
byte proofs pass unchanged. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BH` adds no export or implementation change. With border unset,
IEEE binary64 positive infinity reaches the existing `double thresh` ABI
unchanged. The finite value/background distance is at most that threshold, so
native Floodfill returns before seed mutation. Extended raw/facade exact-byte
proofs pass unchanged. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BG` adds no export or implementation change. Empty numeric border
Arrays share the same live non-null border pointer with `border_size == 0` as
one/multi-element Arrays; the source length remains outside the ABI. The reused
raw negative-threshold sentinel proof and extended facade exact-byte proof pass
unchanged. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BF` adds no export or implementation change. One-element numeric
border Arrays share BE's live non-null border pointer with `border_size == 0`;
the source Array length is intentionally not encoded in the ABI. The reused
raw negative-threshold sentinel proof and extended facade exact-byte proof pass
unchanged. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BE` adds no export or implementation change. A live non-null
border pointer with `border_size == 0` remains the supplied-but-incomparable
numeric-border state. At `thresh == -1.0`, the existing native Floodfill takes
its supplied-border branch and fills every scalar neighbor that differs from
the value; threshold distance is not consulted. Raw/facade exact-byte proofs
pass unchanged. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BD` adds no export or implementation change. With valid packed
numeric value and scalar-border samples plus `thresh == -1.0`, the existing
native Floodfill writes the seed and then uses border comparison for neighbor
admission. The matching signed-int32/float32 zero border stops traversal, so
raw/facade exact-byte proofs pass unchanged. Exports and DLL remain `389/389`
and `8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BC` adds no export or implementation change. With a valid packed
numeric value, border unset, and `thresh == -1.0`, the existing native
Floodfill writes the seed and rejects every neighbor because its one-norm
distance cannot be at most a negative threshold. Raw/facade exact-byte proofs
pass unchanged. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BB` adds no export or native implementation change. The facade
detects empty numeric value Arrays before threshold validation, skips the
invalid public threshold only for that route, and passes `0.0` to the existing
threshold-independent native no-op. Nonempty values retain numeric-threshold
validation. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001BA` adds no export or native implementation change. The raw ABI
now explicitly proves that `value_size == 0` returns success even when a
supplied border length is invalid. For empty numeric value Arrays, the facade
therefore skips border normalization and calls the native no-op with border
unset; nonempty values retain existing validation. Exports and DLL remain
`389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001AZ` adds no export or native implementation change. Empty numeric
border Arrays use AX's already-covered `border != nullptr && border_size == 0`
state, and the facade keeps the zero-length sentinel buffer alive for the
call. The reused raw dual-sentinel and extended facade tests pass unchanged.
Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001AY` adds no export or native implementation change. One-element
numeric border Arrays use AX's already-covered `border != nullptr &&
border_size == 0` state; the source Array length is intentionally a facade-
side normalization detail and is not encoded in the ABI. The reused raw dual-
sentinel proof and extended facade test pass unchanged. Exports and DLL remain
`389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001AX` adds no export or native implementation change. A live non-
null value pointer with `value_size == 0` and a live non-null border pointer
with `border_size == 0` compose the already-covered empty-value and supplied-
but-incomparable-border states. Empty-value precedence returns success before
border validation or traversal. The facade keeps both sentinel buffers alive
for the call. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001AW` extends AU/AV's existing empty-value state without adding an
export: after the required non-null image/value checks, `value_size == 0`
returns success before validating or traversing any supplied border. This
matches Pillow's `_color_diff(value, background)` call and caught empty-tuple
`IndexError` before border handling. The facade removes only the absent-border
routing restriction; bounded scalar borders remain normally packed and owned
for the duration of the call. Release x64 was rebuilt with zero warnings and
errors; source/DLL exports remain `389/389`, and DLL SHA-256 is
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

`MODE-NUM-001AV` extends AU's existing empty-value state without adding an
export: `value != nullptr && value_size == 0 && border == nullptr &&
border_size == 0` is now a no-op independently of the numeric `thresh` value.
This matches Pillow's empty-tuple `IndexError` catch before threshold use. The
facade removes only the threshold-zero routing restriction. Release x64 was
rebuilt; source/DLL exports remain `389/389`, and DLL SHA-256 is
`EABD8291824F88BCAAAD0D92C277500DD6F488C8DFD55DBA62CB1AE57623C9E3`.

`MODE-NUM-001AU` extends `pillow_c_image_draw_floodfill` without adding an
export: `value != nullptr && value_size == 0 && border == nullptr &&
border_size == 0 && thresh == 0.0` explicitly represents Pillow's bounded
empty-tuple value no-op. It returns success before reading value bytes or
walking pixels. The facade supplies a live sentinel only for empty numeric
Arrays in that exact public combination. Release x64 was rebuilt; source/DLL
exports remain `389/389`, and DLL SHA-256 is
`AE26A1E30AB59D5A28D498C17A1D55F0E36FB428C5C32CF7AC710E485BFC2F45`.

`MODE-NUM-001AT` adds no export or implementation change. Public one-element
mode `I`/`F` Floodfill value Arrays are unpacked by the facade's existing
numeric `ColorBuffer` into a four-byte signed-int32/float32 scalar. The
existing `pillow_c_image_draw_floodfill` value contract then owns no-border
traversal and preserves allocation. Exports and DLL remain `389/389` and
`B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

`MODE-NUM-001AS` adds no export or implementation change. Public empty mode
`I`/`F` Floodfill border Arrays already route through a live non-null buffer
with `border_size == 0`, selecting AP's supplied-but-incomparable native state.
The reused raw sentinel proof and facade empty-Array proof preserve allocation
and exact int32/float32 storage. Exports and DLL remain `389/389` and
`B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

`MODE-NUM-001AR` adds no export or implementation change. Bounded scalar mode
`I`/`F` Floodfill borders are packed as signed-int32/float32 colors by the
facade and passed with `border_size == channels`, selecting the ordinary native
border comparison rather than AP's zero-size sentinel. The matching sample
stops traversal without reallocating storage. Exports and DLL remain `389/389`
and `B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

`MODE-NUM-001AQ` adds no export or native implementation change. Public
numeric `ImageDraw.Floodfill` now routes one-element mode `I`/`F` border
Arrays, like the already-covered multi-element Arrays, to AP's explicit
non-null/zero-size incomparable-border state. Scalar border colors continue
through ordinary packed-color comparison. Exports and DLL remain `389/389` and
`B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

`MODE-NUM-001AP` extends the existing `pillow_c_image_draw_floodfill` contract
without adding an export: `border != nullptr && border_size == 0` explicitly
means that a border was supplied but is incomparable with scalar image
samples. This state uses border-mode traversal, rejects pixels already equal
to the fill value, and never reads or compares the sentinel buffer as a color.
The facade keeps a live sentinel buffer for bounded mode `I`/`F` border
Arrays. Exports remain `389/389`; rebuilt DLL SHA-256 is
`B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

`MODE-NUM-001AO` adds no ABI or native implementation change. Public
`ImageDraw.Floodfill` now rejects multi-element mode `I`/`F` value Arrays
during value-local facade normalization with Pillow's tuple errors. Border
handling and the native flood-fill queue are unchanged. Exports remain
`389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AN` adds no ABI or native implementation change. Public
`ImageDraw.Bitmap` now rejects multi-element mode `I`/`F` fill Arrays during
Bitmap-local facade normalization with Pillow's tuple errors. Valid mask
handling and native bitmap compositing are unchanged. Exports remain
`389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AM` adds no ABI or native implementation change. Public
`ImageDraw.RoundedRectangle` now rejects multi-element mode `I`/`F` outline
Arrays during outline-local facade normalization with Pillow's tuple errors
when fill is unset. Fill handling and valid native rounded-rectangle drawing
are unchanged. Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AL` adds no ABI or native implementation change. Public
`ImageDraw.RoundedRectangle` now rejects multi-element mode `I`/`F` fill
Arrays during fill-local facade normalization with Pillow's tuple errors when
outline is unset. Outline handling and valid native rounded-rectangle drawing
are unchanged. Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AK` adds no ABI or native implementation change. Public
`ImageDraw.Pieslice` now rejects multi-element mode `I`/`F` outline Arrays
during outline-local facade normalization with Pillow's tuple errors when fill
is unset. Fill handling and valid native pieslice drawing are unchanged.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AJ` adds no ABI or native implementation change. Public
`ImageDraw.Pieslice` now rejects multi-element mode `I`/`F` fill Arrays during
fill-local facade normalization with Pillow's tuple errors when outline is
unset. Outline handling and valid native pieslice drawing are unchanged.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AI` adds no ABI or native implementation change. Public
`ImageDraw.Chord` now rejects multi-element mode `I`/`F` outline Arrays during
outline-local facade normalization with Pillow's tuple errors when fill is
unset. Fill handling and valid native chord drawing are unchanged. Exports
remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AH` adds no ABI or native implementation change. Public
`ImageDraw.Chord` now rejects multi-element mode `I`/`F` fill Arrays during
fill-local facade normalization with Pillow's tuple errors when outline is
unset. Outline handling and valid native chord drawing are unchanged. Exports
remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AG` adds no ABI or native implementation change. Public
`ImageDraw.Arc` now rejects multi-element mode `I`/`F` fill Arrays during
Arc-local facade normalization with Pillow's tuple errors. Valid native arc
drawing is unchanged. Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AF` adds no ABI or native implementation change. Public
`ImageDraw.Ellipse` now rejects multi-element mode `I`/`F` outline Arrays
during outline-local facade normalization with Pillow's tuple errors when fill
is unset. Fill handling and valid native ellipse drawing are unchanged.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AE` adds no ABI or native implementation change. Public
`ImageDraw.Ellipse` now rejects multi-element mode `I`/`F` fill Arrays during
fill-local facade normalization with Pillow's tuple errors when outline is
unset. Outline handling and valid native ellipse drawing are unchanged.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AD` adds no ABI or native implementation change. Public
`ImageDraw.Rectangle` now rejects multi-element mode `I`/`F` outline Arrays
during outline-local facade normalization with Pillow's tuple errors when fill
is unset. Fill handling and valid native rectangle drawing are unchanged.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AC` adds no ABI or native implementation change. Public
`ImageDraw.Rectangle` now rejects multi-element mode `I`/`F` fill Arrays during
fill-local facade normalization with Pillow's tuple errors; outline handling is
unchanged and valid drawing still uses `pillow_c_image_draw_rectangle`.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AB` adds no ABI or native implementation change. Public
`ImageDraw.Line` now rejects multi-element mode `I`/`F` Arrays during
Line-local facade normalization with Pillow's tuple errors, before either
`pillow_c_image_draw_line` or `_joint`. Valid line drawing remains native.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001AA` adds no ABI or native implementation change. Public
`ImageDraw.Point` now rejects multi-element mode `I`/`F` Arrays during
Point-local facade normalization with Pillow's tuple errors, before the
existing `pillow_c_image_draw_points` call. Valid point drawing remains native.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001Z` adds no ABI or native implementation change. Public
`Image.Paste` now rejects multi-element mode `I`/`F` Arrays during facade
argument normalization with Pillow's mode-specific tuple errors, before the
existing `pillow_c_image_paste_color` call. Valid numeric fills remain native.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001Y` adds no ABI or native implementation change. Existing
`pillow_c_image_paste_color` already fills numeric targets from one four-byte
color in place; the facade now routes one-element mode `I`/`F` Arrays through
the same signed-int32/float32 `ColorBuffer` packing used by scalar colors.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001X` adds no ABI or native implementation change. Existing
`pillow_c_image_paste_color` already fills numeric targets from one four-byte
color in place; the facade now packs scalar mode `I` as signed int32 and mode
`F` as float32 before calling it. Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001W` adds no ABI or implementation change. Existing
`pillow_c_image_paste_masked` mutates numeric `I`/`F` target storage in place,
preserves its data pointer, blends four stored bytes per pixel with exact
Pillow rounding, and leaves source/mask bytes unchanged. Exports remain
`389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001V` adds no ABI or implementation change. Existing
`pillow_c_image_composite` and `_into` treat numeric `I`/`F` storage as four
bytes per pixel for partial mode-L masks, matching Pillow's exact byte blend
and rounding. Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001U` adds no ABI or implementation change. Existing
`pillow_c_image_copy` allocates an independent image and copies all four bytes
of each numeric `I`/`F` sample; the duplicate remains valid after source
mutation or destruction. Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001T` adds no ABI or implementation change. Existing
`pillow_c_image_constant` allocates same-size mode `L` storage and performs a
clipped DLL-owned fill independently of numeric source mode/content. Exports
remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001S` adds no ABI or implementation change. Existing
`pillow_c_image_offset` and `_into` copy `channels=4` bytes per numeric pixel,
matching Pillow's complete-sample wrap semantics for modes `I` and `F`.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001R` adds no ABI or implementation change. Existing
`pillow_c_image_chops_invert` and `_into` complement all stored bytes, which is
exactly Pillow's 32-bit bitwise sample complement for modes `I` and `F`.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001Q` adds no ABI or native implementation change. Existing logical
ImageChops exports reject matching numeric `I`/`F` handles with `-3`; the
facade now maps that status to exact `image has wrong mode`. Exports remain
`389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`MODE-NUM-001P` adds no ABI/implementation change. Existing native numeric
histograms already drive Pillow-compatible nonempty `ImageStat.Stat` values in
the facade. Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`META-002Y` adds no ABI/implementation change. Existing deferred ICC marker
storage and the profile-presence bit preserve an empty final fragment while
retaining the nonempty prefix. Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`META-002X` adds no ABI/implementation change. Existing deferred ICC marker
storage and the profile-presence bit preserve an empty first fragment while
collating the nonempty tail. Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`META-002W` adds no ABI/implementation change. Existing JPEG metadata save
routing omits zero-length ICC input and reopens without public ICC metadata.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`META-002V` adds no ABI/implementation change. Existing deferred ICC marker
sorting matches Pillow's permissive duplicate-sequence `AB` collation.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CS` adds no ABI/implementation change. The generalized save
route emits CR's complete 1505-byte marker stream exactly. Exports remain
`389/389`; DLL hash remains `7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CR` adds no ABI/implementation change. Existing default-4:2:0
progressive encoding emits exact rows-6 over-scan output. Exports remain
`389/389`; DLL hash remains `7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CQ` adds no ABI/implementation change. The generalized save
route emits CP's complete 1757-byte marker stream exactly. Exports remain
`389/389`; DLL hash remains `7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CP` adds no ABI/implementation change. Existing source-4:2:2
progressive encoding emits exact rows-6 over-scan output. Exports remain
`389/389`; DLL hash remains `7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CO` adds no ABI/implementation change. Existing generalized
save emits CN's complete 1505-byte file. Exports remain `389/389`; DLL hash
remains `7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CN` adds no ABI/implementation change. Existing default-4:2:0
progressive encoding emits exact rows-5 DRI/DHT/SOS/entropy with no RST.
Exports remain `389/389`; DLL hash remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CM` adds no ABI/implementation change. Existing generalized
save emits CL's complete 1757-byte file. Exports remain `389/389`; DLL hash
remains `7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CL` adds no ABI or implementation change. The existing source-
4:2:2 progressive path emits exact rows-5 DRI/DHT/SOS/entropy with no RST.
Exports remain `389/389`; DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CK` adds no export, signature, or implementation change. The
existing generalized save export emits CJ's complete 1505-byte Pillow file;
AHK only routes options and hashes test output. Exports remain `389/389`; no
rebuild was required; DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CJ` adds no export, signature, or implementation change. The
existing default-4:2:0 progressive path already composes h2v2 coefficients,
component-local Huffman tables, and rows-4 DRI state. All ten DHT/SOS payloads,
DRI `[12,24,12,24,12,24]`, ten empty restart arrays, and 736 entropy bytes
match Pillow. AHK performs only facade routing and test-only parsing/hashing.
Source/Release exports remain `389/389`; no rebuild was required and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CI` adds no export, signature, or implementation change. The
existing generalized qtables/metadata/restart export already emits CH's
complete 1757-byte Pillow file, including APP0, EXIF/ICC/COM, DQT, SOF2, all
ten DHT/SOS scans, six DRI changes, empty restart state, entropy, and EOI. AHK
performs only facade routing and test-only whole-file hashing. Source/Release
exports remain `389/389`; no rebuild was required and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CH` adds no export, signature, or implementation change. The
existing source-4:2:2 progressive path already handles a rows-4 restart
interval equal to every complete scan: DRI changes are
`[12,24,12,24,12,24]`, all ten restart arrays are empty, and Pillow's exact
DHT/SOS plus 973 entropy bytes are emitted without a terminal RST. AHK
performs only facade routing and test-only parsing/hashing. Source/Release
exports remain `389/389`; no rebuild was required and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CG` adds no export, signature, or implementation change. The
existing generalized qtables/metadata/restart export already emits CF's
complete 1517-byte Pillow file, including APP0, EXIF/ICC/COM, DQT, SOF2, all
ten DHT/SOS scans, DRI/RST state, entropy, and EOI. AHK performs only facade
routing and test-only hashing. Source/Release exports remain `389/389`; no
rebuild was required and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CF` adds no export, signature, or implementation change. The
existing default-4:2:0 progressive path already composes h2v2 coefficients,
component-local Huffman tables, and rows-3 restart state. Through the existing
qtables/metadata/restart export, all ten DHT/SOS payloads, DRI
`[9,18,9,18,9,18]`, mixed empty/RST0 scan state, and 748 entropy bytes match
Pillow. AHK performs only facade routing and test-only parsing/hashing.
Source/Release exports remain `389/389`; no rebuild was required and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CE` adds no export, signature, or implementation change. The
existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits CD's complete 1785-byte Pillow file, including APP0,
EXIF/ICC/COM, DQT, SOF2, all ten DHT/SOS scans, scan-local DRI/RST state,
entropy, and EOI. AHK performs only facade routing and test-only hashing.
Source/Release exports remain `389/389`; no rebuild was required and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CD` adds no export, signature, or implementation change. The
existing restart-aware sampled RGB progressive path already handles the
source-4:2:2 three-row interval and short tail with component-local Huffman
tables. Through the existing qtables/metadata/restart export, all ten DHT/SOS
payloads, DRI `[9,18,9,18,9,18]`, RST0-only scan state, and 1001 entropy bytes
match Pillow. AHK performs only facade routing and test-only parsing/hashing.
Source/Release exports remain `389/389`; no rebuild was required and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CC` adds no export, signature, or implementation change. The
existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits CB's complete 1518-byte Pillow file, including APP0,
EXIF/ICC/COM, DQT, SOF2, all ten DHT/SOS scans, scan-local DRI/RST state,
entropy, and EOI. AHK performs only facade routing and test-only hashing.
Source/Release exports remain `389/389`; no rebuild was required and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CB` adds no export, signature, or implementation change. The
existing restart-aware sampled RGB progressive path already composes h2v2
coefficients and component-local Huffman tables with default-4:2:0 rows-2
restart state. Through the existing qtables/metadata/restart export, all ten
DHT/SOS payloads, DRI `[6,12,6,12,6,12]`, mixed empty/RST0 scan state, and
749 entropy bytes match Pillow. AHK performs only facade routing and test-only
parsing/hashing. Source/Release exports remain `389/389`; no rebuild was
required and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2CA` adds no export, signature, or implementation change. The
existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits BZ's complete 1786-byte Pillow file, including APP0,
EXIF/ICC/COM, DQT, SOF2, all ten DHT/SOS scans, scan-local DRI/RST state,
entropy, and EOI. AHK performs only facade routing and test-only hashing.
Source/Release exports remain `389/389`; no rebuild was required and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2BZ` adds no export, signature, or implementation change. The
existing restart-aware sampled RGB progressive path already composes BV's
component-local Cr/Cb Huffman tables with source-4:2:2 rows-2 restart state.
Through the existing qtables/metadata/restart export, all ten DHT/SOS payloads,
DRI values `[6,12,6,12,6,12]`, RST0-only scan state, and 1002 entropy bytes
match Pillow. AHK performs only facade routing and test-only parsing/hashing.
Source/Release exports remain `389/389`; no rebuild was required and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2BY` adds no export, signature, or implementation change. The
existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits BX's complete 1552-byte Pillow file, including APP0,
EXIF/ICC/COM, DQT, SOF2, all ten DHT/SOS scans, scan-local DRI/RST state,
entropy, and EOI. AHK performs only facade routing and test-only hashing.
Source/Release exports remain `389/389`; no rebuild was required and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2BX` adds no export, signature, or implementation change. The
existing restart-aware sampled RGB progressive path already composes BV's
component-local Cr/Cb tables with BS's h2v2 sampled coefficients on omitted/
default-4:2:0 output. Through the existing qtables/metadata/restart export, all
ten DHT/SOS payloads, scan-local DRI/RST state, and 783 entropy bytes match
Pillow. AHK performs only facade routing and test-only parsing/hashing. Source/
Release exports remain `389/389`; no rebuild was required and DLL SHA-256
remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2BW` adds no export, signature, or implementation change. The
existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits BV's complete 1836-byte Pillow file, including APP0,
explicit EXIF APP1/ICC APP2, implicit COM, source DQT, SOF2, all ten DHT/SOS
scans, six DRI changes, thirty RST markers, entropy, and EOI. AHK performs only
facade routing and test-only whole-file hashing, with no marker or pixel loop.
Source/Release exports remain `389/389`; no rebuild was required and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2BV` adds no export or signature. Behind the existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route, restart-aware sampled RGB progressive output now collects separate Cr
and Cb AC-first and final-refine Huffman frequencies, builds four component-
local tables, and redefines AC table id 1 before each Cr/Cb scan. This replaces
two combined chroma tables with four Pillow-compatible DHT segments while the
ten-scan schedule, SOS selectors, DRI/RST state, sampled coefficients, metadata
patch, and facade ABI remain unchanged. All ten DHT/SOS payloads and 1052
entropy bytes for the bounded source-4:2:2 route are exact. AHK performs only
argument/metadata normalization and test-only parsing/hashing. Release x64
rebuilt with zero warnings/errors; source/Release exports remain `389/389`,
and DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

`FMT-JPEG-002B2BU` adds no export, signature, or implementation change. The
existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits BR's complete 1446-byte Pillow file, including APP0,
explicit EXIF APP1/ICC APP2, implicit COM, source DQT, SOF0, all four DHTs,
DRI/SOS, RST0/RST1/RST2, entropy, and EOI. AHK performs only facade routing
and test-only hashing, with no marker or pixel loop. Source/Release exports
remain `389/389`; no rebuild was required and DLL SHA-256 remains
`85249326EFFCFCEE05F8A00FBAAB0172FB3CD959B8A4FD29FA8F8B09E655C038`.

`FMT-JPEG-002B2BT` adds no export, signature, or implementation change. The
existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits BS's complete 1190-byte Pillow file, including APP0,
explicit EXIF APP1/ICC APP2, implicit COM, source DQT, SOF0, all four DHTs,
DRI/SOS, RST0, entropy, and EOI. AHK performs only facade routing and test-only
whole-file hashing, with no marker or pixel loop. Source/Release exports remain
`389/389`; no rebuild was required and DLL SHA-256 remains
`85249326EFFCFCEE05F8A00FBAAB0172FB3CD959B8A4FD29FA8F8B09E655C038`.

`FMT-JPEG-002B2BS` adds no export or signature. It extends sampled-block
preparation behind the existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route: RGB h2v2 (`h_samp=2,v_samp=2`) chroma downsampling now alternates
libjpeg-turbo's `1,2,1,2...` division bias by output column instead of using
constant bias `2`. Optimized frequency collection and entropy encoding consume
the exact Pillow 4:2:0 coefficients while remaining DLL-owned. BR's h2v1
branch remains unchanged; no facade routing or AHK marker/pixel loop changed.
Release x64 rebuilt with zero warnings/errors; source/Release exports remain
`389/389`, and DLL SHA-256 is
`85249326EFFCFCEE05F8A00FBAAB0172FB3CD959B8A4FD29FA8F8B09E655C038`.

`FMT-JPEG-002B2BR` adds no export or signature. It corrects sampled-block
preparation behind the existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route: RGB h2v1 (`h_samp=2,v_samp=1`) chroma downsampling now alternates
libjpeg-turbo's `0,1,0,1...` division bias by output column. Optimized
frequency collection and entropy encoding therefore consume the exact Pillow
4:2:2 coefficients while remaining DLL-owned. No facade routing or AHK marker/
pixel loop changed; default RGB h2v2 remains the separate
`FMT-JPEG-002B2BS` boundary. Release x64 rebuilt with zero warnings/errors;
source/Release exports remain `389/389`, and DLL SHA-256 is
`7A00F5EA1255AF5C64B2C0C86DDD377C55E130D81D5698AF2C8982D56D769C93`.

`FMT-JPEG-002B2BQ` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts unit-1 JFIF density 300x150 before APP14 while retaining
BP's exact EXIF/XMP/ICC/COM metadata and quality-keep progressive rows-2 codec
suffix. The result is Pillow's complete 10972-byte file with 47 non-RST
markers, 108 RST markers, 18 scans, and EOI. AHK performs only facade routing
and test-only hashing, with no marker or pixel loop. Source/Release exports
remain `389/389`; no rebuild was required and DLL SHA-256 remains
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BP` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts EXIF APP1, XMP APP1, ICC APP2, and COM after APP14 and
before DQT while retaining BO's exact quality-keep progressive rows-2 codec
suffix. The result is Pillow's complete 10954-byte file with 46 non-RST
markers, 108 RST markers, 18 scans, and EOI. AHK performs only facade routing
and test-only hashing, with no marker or pixel loop. Source/Release exports
remain `389/389`; no rebuild was required and DLL SHA-256 remains
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BO` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts EXIF APP1, ICC APP2, and COM after APP14 and before DQT
while retaining BN's exact quality-keep progressive rows-2 codec suffix. The
result is Pillow's complete 10597-byte file with 45 non-RST markers, 108 RST
markers, 18 scans, and EOI. AHK performs only facade routing and test-only
hashing, with no marker or pixel loop. Source/Release exports remain `389/389`;
no rebuild was required and DLL SHA-256 remains
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BN` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits AZ's complete 10511-byte quality-keep progressive rows-2
Pillow file, including APP14, source DQT, SOF2, all DHT/DRI/SOS markers, 18
entropy streams, 108 restart markers, and EOI. AHK performs only facade routing
and test-only hashing, with no marker or pixel loop. Source/Release exports
remain `389/389`; no rebuild was required and DLL SHA-256 remains
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BM` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts unit-1 JFIF density 300x150 before APP14, EXIF APP1, XMP
APP1, ICC APP2, and COM while retaining BL's exact 18-scan progressive stream.
It emits the complete 3081-byte Pillow file byte-for-byte; AHK performs only
facade routing and test-only hashing, with no marker or pixel loop. Source/
Release exports remain `389/389`; no rebuild was required and DLL SHA-256
remains
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BL` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts EXIF APP1, XMP APP1, ICC APP2, and COM after APP14 and
before DQT while retaining BK's exact 18-scan progressive stream. It emits the
complete 3063-byte Pillow file byte-for-byte; AHK performs only facade routing
and test-only hashing, with no marker or pixel loop. Source/Release exports
remain `389/389`; no rebuild was required and DLL SHA-256 remains
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BK` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts EXIF APP1, ICC APP2, and COM after APP14 and before DQT
while retaining BJ's exact 18-scan progressive stream. It emits the complete
2706-byte Pillow file byte-for-byte; AHK performs only facade routing and
test-only hashing, with no marker or pixel loop. Source/Release exports remain
`389/389`; no rebuild was required and DLL SHA-256 remains
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BJ` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits AY's complete 2620-byte progressive rows-2 Pillow file,
including APP14, split DQT, SOF2, all DHT/DRI/SOS markers, 18 entropy streams,
66 restart markers, and EOI. AHK performs only facade routing and test-only
hashing, with no marker or pixel loop. Source and Release exports remain
`389/389`; no native rebuild was required and the DLL retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BI` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts unit-1 JFIF density 300x150 before APP14, EXIF APP1,
XMP APP1, ICC APP2, and COM while retaining BH's exact web-low optimized
rows-2 codec/metadata stream. It emits the complete 2320-byte Pillow file
byte-for-byte; AHK performs only facade routing and test-only hashing, with no
marker or pixel loop. Source and Release exports remain `389/389`; no native
rebuild was required and the DLL retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BH` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts explicit XMP APP1 after EXIF APP1 and before ICC APP2/
COM while retaining BG's exact web-low optimized rows-2 codec/core metadata.
It emits the complete 2302-byte Pillow file byte-for-byte; AHK performs only
facade routing and test-only hashing, with no marker or pixel loop. Source and
Release exports remain `389/389`; no native rebuild was required and the DLL
retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BG` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts explicit EXIF APP1, ICC APP2, and COM after APP14 and
before BF's exact web-low optimized rows-2 codec stream. It emits the complete
1945-byte Pillow file byte-for-byte; AHK performs only facade routing and
test-only hashing, with no marker or pixel loop. Source and Release exports
remain `389/389`; no native rebuild was required and the DLL retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BF` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits AX's complete 1859-byte web-low optimized rows-2 file,
including exact APP14, split DQT, SOF0, optimized DHTs, DRI/SOS,
entropy/restart placement, and EOI. AHK performs only facade routing and
test-only hashing, with no marker or pixel loop. Source and Release exports
remain `389/389`; no native rebuild was required and the DLL retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BE` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts unit-1 JFIF density 300x150 before APP14, then explicit
EXIF APP1, XMP APP1, ICC APP2, and COM before DQT while retaining BD's exact
optimized rows-2 codec stream. It emits the complete 10676-byte Pillow file
byte-for-byte; AHK performs only facade routing and test-only hashing, with no
marker or pixel loop. Source and Release exports remain `389/389`; no native
rebuild was required and the DLL retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BD` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts explicit XMP APP1 after EXIF APP1 and before ICC APP2/
COM while retaining BC's exact optimized rows-2 codec and core metadata. It
emits the complete 10658-byte Pillow file byte-for-byte; AHK performs only
facade routing and test-only hashing, with no marker or pixel loop. Source and
Release exports remain `389/389`; no native rebuild was required and the DLL
retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BC` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already inserts explicit EXIF APP1, ICC APP2, and COM after APP14 and
before DQT while retaining BB's exact optimized rows-2 codec stream. It emits
the complete 10301-byte Pillow file byte-for-byte; AHK performs only facade
routing and test-only hashing, with no marker or pixel loop. Source and Release
exports remain `389/389`; no native rebuild was required and the DLL retains
SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BB` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already emits BA's complete 10215-byte Pillow file, including exact
APP14, split DQT segments, SOF0, optimized DHTs, DRI/SOS, entropy/restart
placement, and EOI. AHK performs only facade routing and test-only hashing.
Source and Release exports remain `389/389`; no native rebuild was required
and the DLL retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2BA` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already preserves source DQT values/default CMYK 1x1, maps quality-keep
optimized baseline rows-2 to DRI `26`, resets DC predictors at six restart
boundaries, and emits Pillow's exact two DHT payloads plus 9888-byte entropy
stream. AHK performs only facade routing; no marker or pixel loop was added.
Source and Release exports remain `389/389`; no native rebuild was required
and the DLL retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2AZ` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already maps the bounded opened real-YCCK quality-keep progressive
rows-2 case to DRI `26` in all 18 scans, applies six restart boundaries per
scan in optimized frequency collection and entropy output, and emits Pillow's
exact 17 DHT payloads plus all entropy streams. AHK performs only facade
routing. Source and Release exports remain `389/389`; no native rebuild was
required and the DLL retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2AY` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already maps progressive rows-2 to scan-local DRI `14/26`, applies the
matching restart boundaries in optimized frequency collection and entropy
output, and emits Pillow's exact 17 DHT payloads plus all 18 entropy streams
for the bounded opened real-YCCK web-low fixture. AHK performs only facade
routing. Source and Release exports remain `389/389`; no native rebuild was
required and the DLL retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2AX` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already maps rows-2 to DRI `14`, resets optimized baseline DC predictors
at the three Pillow-compatible restart boundaries, and emits the exact
1562-byte entropy stream for the bounded opened real-YCCK web-low fixture. AHK
performs only facade routing; no marker or pixel loop was added. Source and
Release exports remain `389/389`; no native rebuild was required and the DLL
retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2AW` adds no export, signature, or implementation change. The
existing `pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route already composes AV's libjpeg-compatible CMYK h2v2 downsampling with
AS's integer FDCT and optimized-Huffman generation. It emits Pillow's exact
two DHT payloads, DRI `7`, SOS, 1578-byte entropy stream, and six restart
markers for the bounded opened real-YCCK web-low optimized rows-1 fixture.
AHK performs only facade routing; no marker or pixel loop was added. Source and
Release exports remain `389/389`; no native rebuild was required and the DLL
retains SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2AV` adds no export or signature. Shared CMYK h2v2 downsampling
now matches libjpeg's alternating `1,2,1,2...` rounding bias across output
samples instead of using unbiased floor division for every 2x2 average. The
correction applies to downsampled M/Y/K planes while full-resolution C remains
unchanged. The existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route therefore emits Pillow's exact 17 DHT payloads and all 18 entropy streams
for the bounded opened real-YCCK web-low progressive rows-1 fixture without an
AHK marker or pixel loop. Source/Release exports remain `389/389`; Release x64
rebuilt with zero warnings/errors and has SHA-256
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.

`FMT-JPEG-002B2AU` adds no export or signature. Shared progressive AC-first
frequency collection and entropy output now retain cross-block EOBRUN, flushing
before a new nonzero symbol, at restart and scan-end boundaries, or at run
`0x7fff`; each frequency call receives the same scan-local restart interval as
its output pass. Progressive DC-first statistics/output now use a dedicated
signed arithmetic right shift so negative coefficients match libjpeg successive
approximation instead of truncating toward zero. The existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route therefore emits Pillow's exact 17 DHT payloads and all 18 entropy streams
for the bounded opened real-YCCK quality-keep rows-1 fixture without an AHK
marker or pixel loop. Source/Release exports remain `389/389`; Release x64
rebuilt with zero warnings/errors and has SHA-256
`E7594BCEFA4B4DAF473E548587AF3C0DDA99BC87BB9999B781927463883F4E63`.

`FMT-JPEG-002B2AT` adds no export or signature. Shared progressive AC-refine
frequency collection now accepts each scan's restart interval and accumulates
cross-block EOBRUN plus buffered correction-bit counts using libjpeg's
`0x7fff` run and 1000-bit buffer flush boundaries. The matching entropy
writer retains the actual correction bits and flushes the same state before
new symbols, restart markers, and scan end. Existing keep-RGB qtables metadata
restart exports therefore emit Pillow's exact 13 DHT payloads and all 14 scan
entropy streams for the bounded fixture without an AHK marker/pixel loop.
Source/Release exports remain `389/389`; Release x64 rebuilt with zero
warnings/errors and has SHA-256
`A0BFD6BDB9BB029658D9DFCD0B2D8AE7E143D467192A8AF23006C195C39335C7`.

`FMT-JPEG-002B2AS` adds no export or signature. It replaces the shared floating
JPEG FDCT/quantization implementation with libjpeg-turbo-compatible two-pass
integer `JDCT_ISLOW` and `qtable * 8` division, and replaces the priority-tree
optimized Huffman builder with libjpeg's 257-symbol algorithm: pseudo-symbol
256, high-symbol tie selection, 16-bit code-length redistribution, original
code-size symbol ordering, and canonical assignment. The existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
route now emits Pillow's exact two DHT payloads and entropy bytes for the
bounded real-YCCK quality-keep optimized row-restart case. No AHK marker or
pixel loop was added. Source/Release exports remain `389/389`; Release x64 was
rebuilt twice with zero warnings/errors and has SHA-256
`F125A45607C6541EBE0984DF07898656EF0D96F99F6A4A144F60365B5ABF91F9`.

`FMT-JPEG-002B2AR` adds no export or signature and changes no native code. For
opened real-YCCK saves, the facade now applies quality keep/preset precedence
before parsing lower-priority qtables and subsampling values. Valid
keep/preset/custom conflicts and invalid shadowed strings therefore resolve to
the same source-or-preset qtables plus sampling parameters before the existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
call. The DLL continues to own DQT/SOF, JFIF, EXIF/XMP/ICC/COM, restart
entropy, buffers, and output pixels. Source/Release exports remain `389/389`;
the Release x64 DLL was not rebuilt and retains SHA-256
`82227795573AD9EF3D95C90316F99D48C5461887B1C9A7782DBDF0BE97A4C817`.

`FMT-JPEG-002B2AQ` added no export or signature but isolated shared native JPEG
FDCT half-tie drift. Its floating implementation snapped a coefficient within
a scaled 64-double-ULP tolerance of a half integer before `std::round`; the
covered real-YCCK M coefficient is mathematically `-27/54=-0.5` but had
evaluated as `-0.4999999999999997`. `FMT-JPEG-002B2AS` later supersedes that
floating implementation with integer `JDCT_ISLOW`. The existing qtables
metadata/DPI restart export and AQ facade routing continue to own qtables
keep/preset output. Source/Release exports remain `389/389`.

`FMT-JPEG-002B2AP` adds no export or signature. AO's opened real-YCCK
quality keep/`web_low` baseline restart route now forwards explicit
`dpi=(300,150)` with XMP and core metadata through
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`.
The DLL writes and reopens unit-1 JFIF density before APP14 and retains the
complete metadata/restart encode. The facade removes only AO's no-DPI term;
source/Release exports remain `389/389` and the DLL was not rebuilt.

`FMT-JPEG-002B2AO` adds no export or signature. The same opened real-YCCK
quality keep/`web_low` baseline restart route now accepts explicit XMP plus
comment/ICC/EXIF through
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`.
The DLL writes EXIF APP1 before XMP APP1, then ICC APP2 and COM before DQT, and
reopened handles expose exact XMP through `pillow_c_image_metadata_xmp`.
Facade routing removes only AN's XMP exclusion while retaining its no-DPI,
resolved-quality-sentinel, core-metadata, and baseline conditions. No AHK
marker/pixel loop was added; source and Release x64 exports remain `389/389`,
and the DLL was not rebuilt.

`FMT-JPEG-002B2AN` adds no export or signature. Opened real-YCCK
`quality="keep"` and `quality="web_low"` baseline restart saves with explicit
comment/ICC/EXIF resolve source-or-preset qtables and CMYK sampling in the
facade, then enter the existing
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options`
export. The DLL continues to own APP14-transform-0 normalization, EXIF APP1,
ICC APP2, COM, DQT, SOF0, DHT, DRI/RST emission, and output pixels. The facade
change only admits the proven no-XMP/no-DPI quality-sentinel plus core-metadata
combination through that additive ABI; no AHK marker or pixel loop was added.
Source and Release x64 DLL export counts remain `389` / `389`, and the DLL
was not rebuilt for this facade-only slice.

`FMT-JPEG-002B2AM` adds no export or signature. The existing progressive
restart ABI continues to route default RGB 4:2:0 output through
`save_jpeg_rgb_progressive_huffman(...)`. That encoder now retains its MCU-
padded Y block vector for interleaved DC first/refine scans while extracting a
true `ceil(width/8) * ceil(height/8)` raster Y block view for every Y-only AC
frequency and entropy scan. Row-restart DRI for those scans is derived from
the true luma block-column count; the covered `100x100` route therefore uses
DRI `13` and twelve RST markers per Y scan instead of padded DRI `14`. The
facade already routed the public call into this DLL path and changed no
production code. BV later upgrades the shared path to ten component-local DHT
segments; exact default-4:2:0 entropy parity remains separate. Release x64 was
rebuilt with zero warnings/errors, and source/Release x64 export counts remain
`389` / `389`.

`FMT-JPEG-002B2AL` adds no export or signature. The existing additive
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options(...)`
owns all six resolved real-YCCK restart shapes. Native CMYK progressive save
now keeps its MCU-padded C block vector for the interleaved DC scans while
building a true `ceil(width/8) * ceil(height/8)` raster C-block view for the
single-component AC scans. Row restart DRI for those scans is derived from the
true C block-column count, so the covered `100x100` 4:2:0 route writes DRI
`13` and twelve RST markers per C scan instead of DRI `28` over padded blocks.
The facade makes `quality="keep"` replace simultaneous caller qtables and
subsampling options with source qtables plus Pillow's CMYK default `-1`
sampling; the existing quality-preset branch continues to replace both with
the selected preset. No AHK JPEG-byte or pixel loop is introduced. Release
x64 was rebuilt with zero warnings/errors; source/Release x64 export counts
remain `389` / `389`.

`FMT-JPEG-002B2AK` changes no native source, project input, exported
signature, or DLL. The existing additive
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options(...)`
already owns the six resolved opened-CMYK keep/preset native shapes: source or
`web_low` qtables, default/4:2:2/4:2:0 sampling, baseline/optimized/
progressive entropy, implicit opened COM, and DRI/RST output. The facade parses
qtables preset strings through the existing preset table registry, normalizes
omitted CMYK quality-keep sampling to `-1`, admits bounded CMYK keep/preset
restart calls, and requests implicit opened COM for preset routes. It performs
no JPEG-byte editing or pixel traversal. No Release x64 rebuild was required,
and source/Release x64 export counts remain `389` / `389`.

`FMT-JPEG-002B2AJ` changes no native source, project input, exported signature,
or DLL. The existing additive
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options(...)`
already accepts DPI, comment/ICC/EXIF/XMP buffers, custom qtables, real
`subsampling=1/2`, and restart options together and owns the covered
APP14/XMP/DQT and JFIF/APP14/EXIF/XMP/ICC/COM/DQT marker sequences, sampled
SOF geometry, baseline/optimized/progressive entropy, and DRI/RST output. The
facade removes only `!xmpOption.Set` from the bounded strategy guard and still
excludes keep/preset sentinels; it performs no JPEG-byte editing or pixel
traversal. No Release x64 rebuild was required, and source/Release x64 export
counts remain `389` / `389`.

`FMT-JPEG-002B2AI` changes no native source, project input, exported signature,
or DLL. The existing additive
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options(...)`
already accepts DPI, comment/ICC/EXIF buffers, custom qtables, real
`subsampling=1/2`, and restart options together and owns JFIF/APP14/metadata/
DQT ordering, sampled SOF geometry, baseline/optimized/progressive entropy,
and DRI/RST output. The facade removes only AH's no-DPI baseline condition;
XMP and keep/preset paths remain excluded. It performs no JPEG-byte editing or
pixel traversal. No Release x64 rebuild was required, and source/Release x64
export counts remain `389` / `389`.

`FMT-JPEG-002B2AH` changes no native source, project input, exported signature,
or DLL. The existing additive
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options(...)`
already accepts comment/ICC/EXIF buffers, custom qtables, real
`subsampling=1/2`, and restart options together and owns APP14/metadata/DQT
ordering, sampled SOF geometry, baseline/optimized/progressive entropy, and
DRI/RST output. The facade now permits the bounded no-DPI/no-XMP CMYK baseline
combination through a precise strategy condition; it performs no JPEG-byte
editing or pixel traversal. No Release x64 rebuild was required, and
source/Release x64 export counts remain `389` / `389`.

`FMT-JPEG-002B2AG` changes no native source, project input, exported signature,
or DLL. The existing additive
`pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options(...)`
already accepts DPI, custom qtables, real `subsampling=1/2`, and restart
options together and owns APP0/JFIF ordering, sampled SOF geometry, DQT,
baseline/optimized/progressive entropy, and DRI/RST output. The facade now
permits that bounded CMYK combination by narrowing only its strategy guard;
it performs no JPEG-byte editing or pixel traversal. No Release x64 rebuild
was required, and source/Release x64 export counts remain `389` / `389`.

`FMT-JPEG-002B2AF` changes no exported signature. The existing
`pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options(...)`
and additive DPI-capable variant now route bounded CMYK custom qtables with
real `subsampling=1/2` into the DLL-owned restart encoder. Native row restart
intervals use sampled C MCU width; progressive C-only scans receive a separate
block interval and emit DRI changes when scan geometry changes. The facade
permits this route only for custom qtables without DPI and performs no JPEG
byte editing or pixel traversal. Release x64 rebuilt with `0 Warning(s), 0
Error(s)`; source/Release x64 export counts remain `389` / `389`.

`FMT-JPEG-002B2AE` adds the ABI:

```text
pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options(
    image, path, quality, has_dpi, dpi_x, dpi_y,
    comment, comment_size, icc_profile, icc_profile_size,
    exif, exif_size, xmp, xmp_size,
    qtables, qtable_count, subsampling, progressive, optimize,
    restart_marker_blocks, restart_marker_rows)
```

The additive export delegates to `save_jpeg_image_with_qtables_metadata_options`
with caller DPI plus the existing qtables/metadata/restart options. It writes
JFIF unit-1 density before APP14 while preserving default-1x1 CMYK DQT,
baseline/optimized/progressive entropy, DRI/RST structure, metadata patching,
and exact bounded reopen bytes. The legacy non-DPI restart export remains
unchanged. The facade routes all custom-qtables restart calls through this
export with `has_dpi=0/1`, but permits explicit DPI only for bounded CMYK
custom-qtables restart output; no AHK JPEG-byte or pixel loop is introduced.
Release x64 was rebuilt with `0 Warning(s), 0 Error(s)`; source/Release x64
export counts are `389` / `389`.

`MODE-COLOR-001BR` changes no native signature or implementation. Public LAB
`Image.Quantize` now reproduces Pillow's palette-omitted scalar
noninteger-kmeans validation before colors, source mode, or empty-image
handling. Negative Floats retain `kmeans must not be negative`; nonnegative
Floats expose the integer-interpretation TypeError text; Strings expose the
comparison TypeError text. These calls reject in the facade and never enter
`pillow_c_image_quantize(...)`; integer kmeans and supplied Image palettes keep
their existing routes. No scalar coercion, AHK pixel loop, native rebuild, or
ABI change was introduced; source/Release x64 export counts remain `388` /
`388`.

`MODE-COLOR-001BQ` changes no native signature or implementation. With palette
omitted, Pillow ignores dither and preserves the covered LAB validation and
empty-image route even for invalid integer/string dither values. Public LAB
`Image.Quantize` therefore allows explicit dither through the existing
`pillow_c_image_quantize(...)` path: negative kmeans remains a facade error;
nonempty calls normalize colors/wrong-mode ordering; legal zero-pixel images
return DLL-allocated empty P handles with empty RGB palettes. Supplied palettes
remain excluded and continue through their separate validation branch. The
native export has no dither parameter because no dither pixel operation occurs
in this slice. No AHK pixel loop, native rebuild, or ABI change was introduced;
source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001BP` changes no native signature or implementation. Public LAB
`Image.Quantize` now gives supplied Image palettes precedence over colors,
integer kmeans, source emptiness, and native quantize routing. P palettes reach
the existing facade source-mode validation and non-P Image palettes reach the
existing palette-mode validation; both reject before any palette mapping or
zero-pixel operation. The LAB negative-kmeans gate is therefore limited to an
omitted palette. Existing `pillow_c_image_quantize(...)` coverage remains the
raw authority for palette-omitted empty LAB images, but no native export is
called by the covered supplied-palette cases. No AHK pixel loop, native rebuild,
or ABI change was introduced; source/Release x64 export counts remain `388` /
`388`.

`MODE-COLOR-001BO` changes no native signature or implementation. Public LAB
`Image.Quantize` now applies Pillow's negative-integer kmeans error before
colors/mode/empty handling. Nonnegative integer kmeans values retain the
covered LAB path: nonempty invalid colors precede wrong mode, while legal
empty images route through existing `pillow_c_image_quantize(...)` and return
DLL-allocated empty P handles with empty RGB palettes. The native export has no
kmeans parameter because no nonempty LAB algorithm is entered; this slice only
normalizes validation and routes zero-pixel allocation. No AHK pixel loop,
native rebuild, or ABI change was introduced; source/Release x64 export counts
remain `388` / `388`.

`MODE-COLOR-001BN` changes no native signature or implementation. Although
the local Pillow 11.3.0 build reports libimagequant unavailable, LAB mode,
colors, and zero-pixel validation precede that dependency boundary. Public
explicit `Quantize.LIBIMAGEQUANT` therefore uses the existing
`pillow_c_image_quantize(...)` route for legal empty images and returns
DLL-allocated empty P handles with empty RGB palettes; nonempty invalid colors
precede the wrong-mode error. The facade adds LIBIMAGEQUANT to the bounded
resolved-method set while retaining kmeans zero and omitted palette/dither.
This does not claim native nonempty libimagequant algorithm support. No AHK
pixel loop, synthetic output, native rebuild, or ABI change was introduced;
source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001BM` changes no native signature or implementation. Public LAB
`Image.Quantize` now treats explicit `Quantize.FASTOCTREE` like the other
covered methods for this validation boundary: nonempty invalid colors precede
wrong mode, while legal empty images bypass colors/source-mode validation
through the existing `pillow_c_image_quantize(...)` export and return
DLL-allocated empty P handles with empty RGB palettes. The facade adds
FASTOCTREE to the bounded resolved-method set while retaining kmeans zero and
omitted palette/dither. No AHK pixel loop, synthetic output, native rebuild,
or ABI change was introduced; source/Release x64 export counts remain `388` /
`388`.

`MODE-COLOR-001BL` changes no native signature or implementation. Public LAB
`Image.Quantize` now treats explicit `Quantize.MAXCOVERAGE` like omitted method
and MEDIANCUT for the covered validation boundary: nonempty invalid colors
precede wrong mode, while legal empty images bypass colors/source-mode
validation through the existing `pillow_c_image_quantize(...)` export and
return DLL-allocated empty P handles with empty RGB palettes. The facade adds
MAXCOVERAGE to the bounded resolved-method set while retaining kmeans zero and
omitted palette/dither. No AHK pixel loop, synthetic output, native rebuild,
or ABI change was introduced; source/Release x64 export counts remain `388` /
`388`.

`MODE-COLOR-001BK` changes no native signature or implementation. Public LAB
`Image.Quantize` now treats explicit `Quantize.MEDIANCUT` like the omitted
method: nonempty invalid colors precede the wrong-mode error, while legal
empty images bypass colors/source-mode validation through the existing
`pillow_c_image_quantize(...)` export and return DLL-allocated empty P handles
with empty RGB palettes. The facade keys the bounded route on resolved
MEDIANCUT and retains the existing kmeans-zero plus omitted palette/dither
boundary. No AHK pixel loop, synthetic output, native rebuild, or ABI change
was introduced; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001BJ` changes no native signature or implementation. Existing
raw tests already prove that `pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` reject LAB -> PA for nonempty and
legal empty images. Pillow 11.3.0 additionally ignores public dither, palette,
and colors arguments for this mode pair and raises `conversion from LAB to RGB
not supported` for all 192 bounded size/option combinations. The facade now
recognizes PA in its early LAB mode-pair error gate before generic dither
validation, without calling the P-only adaptive quantize route, composing
through RGB, or adding an AHK pixel loop. No native rebuild or ABI change was
required; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001BI` changes no native signature or implementation. Public
default-argument LAB `Image.Quantize` now routes legal zero-pixel images
through the existing `pillow_c_image_quantize(...)` export whose validation
order was established by `MODE-COLOR-001BH`. Nonempty LAB calls normalize
invalid colors to `bad number of colors` before the valid-color `image has
wrong mode` error; legal empty shapes allow colors `0`, `1`, `256`, and `257`,
receive the DLL-allocated empty P handle and empty RGB palette, and copy facade
info through normal derived-handle wrapping. The bounded facade route applies
only when method, palette, and dither are omitted and kmeans is zero. No AHK
pixel loop, synthetic output, native rebuild, or ABI change was introduced;
source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001BH` changes no native signature. The existing
`pillow_c_image_quantize(...)` and `pillow_c_image_quantize_into(...)` routes
now validate the caller-owned P target shape and then treat zero-pixel sources
as a successful empty quantization before validating `colors` or the source
mode. The empty path clears target RGB/alpha palette metadata and returns an
empty P image for allocating and `_into` calls, including LAB sources and
colors `0`, `1`, `256`, and `257`; nonempty RGB/L colors and mode validation is
unchanged. This matches Pillow 11.3.0's LAB -> P `Palette.ADAPTIVE` path.
Facade `Image.Convert` now accepts the Python-order dither, palette, and colors
slots, exposes `Pillow.Palette.WEB` / `ADAPTIVE`, routes empty LAB ADAPTIVE
conversion through the native quantize export, and normalizes nonempty colors
versus wrong-mode errors. WEB/default and unknown palette values keep the
existing exact LAB conversion error. No AHK pixel loop or implicit RGB
composition was added. Release x64 was rebuilt; source/Release x64 export
counts remain `388` / `388`.

`MODE-COLOR-001BG` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now include `LAB` -> `P` and
`LAB` -> `PA` in the explicit invalid mode-pair gate. Validation occurs before
generic empty-pixel success, so nonempty plus `(0,1)`, `(1,0)`, and `(0,0)`
sources all return `PILLOW_C_INVALID_ARGUMENT`; allocating calls return no
output handle and `_into` calls reject the caller-owned target. This matches
Pillow 11.3.0 direct default conversion, which does not compose through RGB
and raises `conversion from LAB to RGB not supported`; explicit caller
composition through RGB remains valid and separate. Facade `Image.Convert`
extends the plain LAB public-error normalization to P/PA without an AHK pixel
loop. Release x64 was rebuilt; source/Release x64 export counts remain `388` /
`388`.

`MODE-COLOR-001BF` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now include `LAB` -> mode `1`,
`LAB` -> `I`, and `LAB` -> `F` in the explicit invalid mode-pair gate.
Validation occurs before generic empty-pixel success, so nonempty plus `(0,1)`,
`(1,0)`, and `(0,0)` sources all return `PILLOW_C_INVALID_ARGUMENT`;
allocating calls return no output handle and `_into` calls reject the
caller-owned target. The existing `pillow_c_image_convert_mode_dither(...)`
and `pillow_c_image_convert_mode_dither_into(...)` routes validate supported
NONE/Floyd values and then delegate LAB mode-1 sources to the same plain
mode-pair gate, giving allocating and `_into` calls identical rejection order.
This matches Pillow 11.3.0 default, NONE, and Floyd mode-1 conversion plus
plain I/F conversion, all of which raise `conversion from LAB to RGB not
supported`. Facade `Image.Convert` normalizes that exact error before mode-1
dither dispatch and for plain I/F targets, without an AHK pixel loop. Release
x64 was rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001BE` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now include `LAB` -> `CMYK`,
`LAB` -> `YCbCr`, and `LAB` -> `HSV` in the explicit invalid mode-pair gate.
Validation occurs before generic empty-pixel success, so nonempty plus `(0,1)`,
`(1,0)`, and `(0,0)` sources all return `PILLOW_C_INVALID_ARGUMENT`;
allocating calls return no output handle and `_into` calls reject the
caller-owned target. This matches Pillow 11.3.0 direct conversion, which does
not compose through RGB and raises `conversion from LAB to RGB not supported`.
Facade `Image.Convert` extends the same exact public error normalization used
for L/LA, without an AHK pixel loop. Release x64 was rebuilt; source/Release
x64 export counts remain `388` / `388`.

`MODE-COLOR-001BD` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports explicitly reject `LAB` -> `L`
and `LAB` -> `LA` with `PILLOW_C_INVALID_ARGUMENT`. Validation now occurs
before the generic empty-pixel success path, so nonempty plus `(0,1)`, `(1,0)`,
and `(0,0)` sources all reject consistently; allocating calls return no output
handle and `_into` calls leave the caller-owned target as an invalid operation.
This matches Pillow 11.3.0, whose direct public conversions do not compose
through RGB and raise `conversion from LAB to RGB not supported`. Facade
`Image.Convert` normalizes that exact public error for the plain L/LA route,
without an AHK pixel loop. Release x64 was rebuilt; source/Release x64 export
counts remain `388` / `388`.

`MODE-COLOR-001BC` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `LAB` sources to
`RGB`, `RGBA`, and `RGBX` through process-lifetime perceptual Lab2 -> sRGB
LittleCMS 2.17 transforms using `cmsFLAGS_NOCACHE`. Source rows use internal
`[L,A+128,B+128]` bytes established by LAB raw decode; LittleCMS performs gamut
clipping and writes RGB directly into target-owned rows. Four-byte targets are
prefilled with `255`, so RGBA alpha and RGBX X are opaque and direct output
matches LAB -> RGB -> target composition. Allocating and `_into` routes share
the same traversal, legal empty shapes skip the color engine, and failure to
construct any process-lifetime LAB transform returns the explicit allocation
status. The facade uses its existing `Image.Convert` route with no AHK pixel
loop. Release x64 was rebuilt; source/Release x64 export counts remain `388` /
`388`, and no external LittleCMS DLL is required.

`MODE-COLOR-001BB` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `RGB`, `RGBA`, and
`RGBX` sources to `LAB` through official LittleCMS 2.17 code compiled statically
into `pillow_c.dll`. Process-lifetime perceptual sRGB -> Lab2 transforms use
`cmsFLAGS_NOCACHE`; RGB uses `TYPE_RGB_8`, while RGBA and RGBX share
`TYPE_RGBA_8` and ignore the fourth byte. Output is written directly into
target-owned internal `[L,A+128,B+128]` storage and remains exposed as signed
raw `[L,A,B]`; allocating and `_into` paths share the same row traversal, legal
empty shapes skip the color engine, and transform-construction failure returns
the explicit allocation status. The facade uses its existing `Image.Convert`
route with no AHK pixel loop. Release x64 was rebuilt; source/Release x64 export
counts remain `388` / `388`, and no external LittleCMS DLL is required.

`MODE-COLOR-001BA` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert mode `1`, `L`, and
`LA` sources to `LAB` through the exact grayscale L* lookup introduced by
`MODE-COLOR-001AZ`. Mode 1 promotes logical samples to 0/255, L indexes
directly, and LA ignores alpha; all write internal `[L*,128,128]`, exposed as
signed raw `[L*,0,0]`. Allocating and `_into` routes share one DLL-owned
traversal, legal empty shapes are preserved, and the facade uses its existing
`Image.Convert` route without an AHK pixel loop. Release x64 was rebuilt;
source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AZ` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert numeric `I` and
`F` sources to `LAB` in one DLL-owned traversal. Each sample first uses
established I/F-to-L truncation and clipping, then indexes the exact
`PILLOW_L_TO_LAB_L` table and writes internal `[L*,128,128]`; the existing LAB
raw boundary exposes those bytes as `[L*,0,0]`. This matches Pillow's direct,
L-composed, and grayscale-RGB-composed routes including lookup-sensitive
rounding, NaN, and infinities. Allocating and `_into` routes share the helper,
legal empty shapes are preserved, and the facade uses its existing
`Image.Convert` route without an AHK pixel loop. Release x64 was rebuilt;
source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AY` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert numeric `I` and
`F` sources to `HSV` in the existing DLL-owned direct-grayscale target
traversal. Each sample first uses established I/F-to-L truncation and clipping,
then writes `[0,0,L]`. This matches Pillow's direct route and composition
through both L and RGB. NaN and `-Inf` produce V=`0`; `+Inf` produces V=`255`.
Allocating and `_into` routes share the helper, legal empty shapes are
preserved, and the facade uses its existing `Image.Convert` route without an
AHK pixel loop. Release x64 was rebuilt; source/Release x64 export counts
remain `388` / `388`.

`MODE-COLOR-001AX` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert numeric `I` and
`F` sources to `YCbCr` in the existing DLL-owned direct-grayscale target
traversal. Each sample first uses established I/F-to-L truncation and clipping,
then writes `[L,128,128]`. This matches Pillow's direct route and composition
through L, not composition through RGB at lookup-sensitive values. NaN and
`-Inf` produce Y=`0`; `+Inf` produces Y=`255`. Allocating and `_into` routes
share the helper, legal empty shapes are preserved, and the facade uses its
existing `Image.Convert` route without an AHK pixel loop. Release x64 was
rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AW` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert numeric `I` and
`F` sources to `CMYK` in the existing DLL-owned CMYK target traversal. Each
sample first uses established I/F-to-L truncation and clipping, then writes
`[0,0,0,255-L]`. This matches Pillow's direct route and composition through L,
not composition through RGB. NaN and `-Inf` produce K=`255`; `+Inf` produces
K=`0`. Allocating and `_into` routes share the helper, legal empty shapes are
preserved, and the facade uses its existing `Image.Convert` route without an
AHK pixel loop. Release x64 was rebuilt; source/Release x64 export counts
remain `388` / `388`.

`MODE-COLOR-001AV` changes no native signature. The existing
`pillow_c_image_convert_mode_dither(...)` and
`pillow_c_image_convert_mode_dither_into(...)` exports now convert numeric `I`
and `F` sources to mode `1` under dither `0` (NONE) and `3`
(FLOYDSTEINBERG). Both paths first apply established I/F-to-L truncation and
clipping without allocating an intermediate image. NONE thresholds the clipped
byte at `>=128`; Floyd feeds it into the existing native error-diffusion state
machine. NaN and `-Inf` map to `0`, while `+Inf` maps to `255`; direct output
matches composition through L. Allocating and `_into` routes share the helper,
legal empty shapes remain supported, and the facade default remains Floyd.
Release x64 was rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AU` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert numeric `I` and
`F` sources to `LA` in the shared DLL-owned numeric-source color traversal.
The loop reuses established I/F-to-L truncation and clipping, writes the
resulting byte as L, and appends alpha=`255`. NaN and `-Inf` map to `0`, while
`+Inf` maps to `255`; direct conversion matches composition through L.
Allocating and `_into` routes share the helper, and legal `(0,1)`, `(1,0)`,
and `(0,0)` sources preserve shape with empty targets. The facade uses the
existing `Image.Convert` route without an AHK pixel loop. Release x64 was
rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AT` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert numeric `I` and
`F` sources to `RGB` and `RGBA` in the shared DLL-owned numeric-source color
traversal. The loop reuses established I/F-to-L truncation and clipping,
replicates the resulting byte into RGB, and writes RGBA alpha=`255`. NaN and
`-Inf` map to `0`, while `+Inf` maps to `255`; direct conversion matches
composition through L. Allocating and `_into` routes share the helper, and
legal `(0,1)`, `(1,0)`, and `(0,0)` sources preserve shape with empty targets.
The facade uses the existing `Image.Convert` route without an AHK pixel loop.
Release x64 was rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AS` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert numeric `I` and
`F` sources to `RGBX` in one DLL-owned traversal. The loop reuses established
I/F-to-L truncation and clipping, replicates the resulting byte into RGB, and
writes X=`255`. Negative and overflow values, fractional F samples, NaN, and
infinities match Pillow: NaN and `-Inf` map to `0`, while `+Inf` maps to `255`.
Allocating and `_into` routes share the helper, and legal `(0,1)`, `(1,0)`,
and `(0,0)` sources preserve shape with empty targets. The facade uses the
existing `Image.Convert` route without an AHK pixel loop. Release x64 was
rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AR` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `RGB` and `RGBA`
sources to `RGBX` in one DLL-owned traversal. R/G/B bytes are copied directly
and X is always `255`; RGBA alpha is ignored. Varied-alpha duplicate RGB
pixels prove direct RGBA conversion matches `RGBA -> RGB -> RGBX`, while RGB
also matches composition. Legal `(0,1)`, `(1,0)`, and `(0,0)` sources preserve
shape with empty targets. The facade uses the existing `Image.Convert` route
without an AHK pixel loop. Release x64 was rebuilt; source/Release x64 export
counts remain `388` / `388`.

`MODE-COLOR-001AQ` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert mode `1` sources
to `RGBX` in the shared grayscale four-channel traversal. Each internal
logical sample is promoted from zero/nonzero to 0/255, then written as
`[value,value,value,255]`. A packed `0xA5,0x5A` fixture exactly matches direct
Pillow and composed mode 1 -> RGB -> RGBX bytes. Legal `(0,1)`, `(1,0)`, and
`(0,0)` sources preserve shape with empty targets. The facade uses the existing
`Image.Convert` route without an AHK pixel loop. Release x64 was rebuilt;
source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AP` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `L` and `LA`
sources to `RGBX` in one DLL-owned traversal. L writes `[L,L,L,255]`; LA
writes `[L,L,L,A]`, preserving alpha as Pillow's public X byte. Direct L
conversion matches `L -> RGB -> RGBX`, while direct LA conversion differs from
`LA -> RGB -> RGBX` because the composed route writes X=`255`. Legal `(0,1)`,
`(1,0)`, and `(0,0)` sources preserve shape with empty targets. The facade
uses the existing `Image.Convert` route without an AHK pixel loop. Release x64
was rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AO` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert mode `1` sources
to `YCbCr` in the shared direct grayscale traversal. Each internal logical
sample is promoted from zero/nonzero to 0/255, then written as
`[value,128,128]`. A packed `0xA5,0x5A` fixture exactly matches direct Pillow
and composed mode 1 -> RGB -> YCbCr bytes. Legal `(0,1)`, `(1,0)`, and `(0,0)`
sources preserve shape with empty targets. The facade uses the existing
`Image.Convert` route without an AHK pixel loop. Release x64 was rebuilt;
source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AN` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `L` and `LA`
sources to `YCbCr` in one DLL-owned traversal. Each pixel writes
`[L,128,128]`; LA alpha is ignored. This is Pillow's direct grayscale route,
not RGB lookup composition: exhaustive L values `0..255` match direct Pillow
bytes and expose differences from `L -> RGB -> YCbCr` at lookup-sensitive
values. Legal `(0,1)`, `(1,0)`, and `(0,0)` sources preserve shape with empty
targets. The facade uses the existing `Image.Convert` route without an AHK
pixel loop. Release x64 was rebuilt; source/Release x64 export counts remain
`388` / `388`.

`MODE-COLOR-001AM` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `RGBA` sources to
`YCbCr` in the shared RGB/RGBX traversal. The loop strides by the source's four
channels and passes only R/G/B to `rgb_to_ycbcr_u8(...)`, so alpha is ignored
while exact lookup rounding is preserved. Direct Pillow conversion exactly
matches composed `RGBA -> RGB -> YCbCr`, including duplicate RGB pixels with
different alpha. Legal `(0,1)`, `(1,0)`, and `(0,0)` sources preserve shape
with empty targets. The facade uses the existing `Image.Convert` route without
an AHK pixel loop. Release x64 was rebuilt; source/Release x64 export counts
remain `388` / `388`.

`MODE-COLOR-001AL` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `CMYK` sources to
`YCbCr` inside the existing DLL-owned CMYK/RGB traversal. Each pixel uses
`cmyk_to_rgb_u8(...)`, preserving black-channel scaling and 255-denominator
rounding, then `rgb_to_ycbcr_u8(...)` for exact lookup rounding. Direct Pillow
conversion exactly matches composed `CMYK -> RGB -> YCbCr`. Legal `(0,1)`,
`(1,0)`, and `(0,0)` sources preserve shape with empty targets. The facade uses
the existing `Image.Convert` route without an AHK pixel loop. Release x64 was rebuilt;
source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AK` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports convert `CMYK` sources to
`RGBX` inside the existing DLL-owned CMYK/RGB traversal. Each pixel uses
`cmyk_to_rgb_u8(...)`, preserving black-channel scaling and 255-denominator
rounding, then writes X=`255`. Direct Pillow conversion exactly matches
composed `CMYK -> RGB -> RGBX`. Legal `(0,1)`, `(1,0)`, and `(0,0)` sources
preserve shape with empty targets. The facade uses the existing `Image.Convert`
route without an AHK pixel loop. Source/Release x64 export counts remain
`388` / `388`.

`MODE-COLOR-001AJ` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports convert `HSV` sources to
`YCbCr` in one DLL-owned traversal. Each pixel first uses
`hsv_to_rgb_u8(...)`, preserving hue-sector-sensitive RGB bytes, then uses
`rgb_to_ycbcr_u8(...)` for exact lookup rounding. Direct Pillow conversion
exactly matches composed `HSV -> RGB -> YCbCr`. Legal `(0,1)`, `(1,0)`, and
`(0,0)` sources preserve shape with empty targets. The facade uses the existing
`Image.Convert` route without an AHK pixel loop. Source/Release x64 export
counts remain `388` / `388`.

`MODE-COLOR-001AI` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports convert `YCbCr` sources to
`HSV` in one DLL-owned traversal. Each pixel first uses
`ycbcr_to_rgb_u8(...)`, preserving clipped lookup-kernel RGB bytes, then uses
`rgb_to_hsv_u8(...)` for exact H/S/V rounding. Direct Pillow conversion
exactly matches composed `YCbCr -> RGB -> HSV`. Legal `(0,1)`, `(1,0)`, and
`(0,0)` sources preserve shape with empty targets. The facade uses the existing
`Image.Convert` route without an AHK pixel loop. Source/Release x64 export
counts remain `388` / `388`.

`MODE-COLOR-001AH` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports convert `HSV` sources to
`CMYK` inside the shared DLL-owned CMYK loop. Each pixel first uses
`hsv_to_rgb_u8(...)`, preserving hue-sector-sensitive RGB bytes, then writes
C/M/Y as `255-R/G/B` and K=`0`. Direct Pillow conversion exactly matches
composed `HSV -> RGB -> CMYK`. Legal `(0,1)`, `(1,0)`, and `(0,0)` sources
preserve shape with empty targets. The facade uses the existing `Image.Convert`
route without an AHK pixel loop. Source/Release x64 export counts remain
`388` / `388`.

`MODE-COLOR-001AG` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports convert `YCbCr` sources to
`CMYK` inside the shared DLL-owned CMYK loop. Each pixel first uses
`ycbcr_to_rgb_u8(...)`, preserving clipped lookup-kernel RGB bytes, then writes
C/M/Y as `255-R/G/B` and K=`0`. Direct Pillow conversion exactly matches
composed `YCbCr -> RGB -> CMYK`. Legal `(0,1)`, `(1,0)`, and `(0,0)` sources
preserve shape with empty targets. The facade uses the existing `Image.Convert`
route without an AHK pixel loop. Source/Release x64 export counts remain
`388` / `388`.

`MODE-COLOR-001AF` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `HSV` sources to
`RGBX` through the shared four-channel HSV branch. Each pixel uses
`hsv_to_rgb_u8(...)`, preserving the established hue-sector-sensitive RGB
bytes, then appends X=`255`. Direct Pillow conversion exactly matches composed
`HSV -> RGB -> RGBX`. Legal `(0,1)`, `(1,0)`, and `(0,0)` sources preserve
shape with empty targets. The facade uses the existing `Image.Convert` route
without an AHK pixel loop. Release x64 was rebuilt; source/Release x64 export
counts remain `388` / `388`.

`MODE-COLOR-001AE` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `YCbCr` sources
to `RGBX` through the shared four-channel YCbCr branch. Each pixel uses
`ycbcr_to_rgb_u8(...)`, preserving the established clipped lookup-kernel
bytes, then appends X=`255`. Direct Pillow conversion exactly matches composed
`YCbCr -> RGB -> RGBX`. Legal `(0,1)`, `(1,0)`, and `(0,0)` sources preserve
shape with empty targets. The facade uses the existing `Image.Convert` route
without an AHK pixel loop. Release x64 was rebuilt; source/Release x64 export
counts remain `388` / `388`.

`MODE-COLOR-001AD` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `HSV` sources to
`RGBA` in one DLL-owned loop. Each pixel first uses `hsv_to_rgb_u8(...)`,
preserving the established hue-sector-sensitive RGB bytes, then appends opaque
alpha `255`. Direct Pillow conversion exactly matches composed
`HSV -> RGB -> RGBA`. Legal `(0,1)`, `(1,0)`, and `(0,0)` sources preserve
shape with empty targets. The facade uses the existing `Image.Convert` route
without an AHK pixel loop. Release x64 was rebuilt; source/Release x64 export
counts remain `388` / `388`.

`MODE-COLOR-001AC` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `YCbCr` sources
to `RGBA` in one DLL-owned loop. Each pixel first uses
`ycbcr_to_rgb_u8(...)`, preserving the established clipped lookup-kernel
bytes, then appends opaque alpha `255`. Direct Pillow conversion exactly
matches composed `YCbCr -> RGB -> RGBA`. Legal `(0,1)`, `(1,0)`, and `(0,0)`
sources preserve shape with empty targets. The facade uses the existing
`Image.Convert` route without an AHK pixel loop. Release x64 was rebuilt;
source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AB` changes no native signature. The existing
`pillow_c_image_convert_mode_dither(...)` and
`pillow_c_image_convert_mode_dither_into(...)` exports now convert `YCbCr`
sources to mode `1` under dither NONE and Floyd-Steinberg. NONE expands each
YCbCr pixel with `ycbcr_to_rgb_u8(...)` and applies weighted RGB threshold
`128000`; Floyd performs the same clipped RGB expansion inside the existing
native error-diffusion loop and uses truncated RGB luma rather than direct Y.
Facade default conversion already selects Floyd-Steinberg. Legal `(0,1)`,
`(1,0)`, and `(0,0)` sources preserve shape with empty targets. The facade
uses the existing `Image.Convert` route without an AHK pixel loop. Release x64
was rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001AA` changes no native signature. The existing
`pillow_c_image_convert_mode_dither(...)` and
`pillow_c_image_convert_mode_dither_into(...)` exports now convert `HSV`
sources to mode `1` under dither NONE and Floyd-Steinberg. NONE expands each
HSV pixel with `hsv_to_rgb_u8(...)` and applies weighted RGB threshold
`128000`; Floyd expands HSV inside the existing native error-diffusion loop and
uses truncated RGB luma exactly like Pillow's composed RGB route. Facade
default conversion already selects Floyd-Steinberg. Legal `(0,1)`, `(1,0)`,
and `(0,0)` sources preserve shape with empty targets. The facade uses the
existing `Image.Convert` route without an AHK pixel loop. Release x64 was
rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001Z` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `HSV` sources to
`L` and `LA` targets in one DLL-owned loop. Each pixel first uses
`hsv_to_rgb_u8(...)`, preserving the established hue-sector float behavior,
then `rgb_luma_u8(...)` writes rounded fixed-point luma; LA appends alpha 255.
Legal `(0,1)`, `(1,0)`, and `(0,0)` sources produce shape-preserving empty
targets. The facade uses the existing `Image.Convert` route without an AHK
pixel loop. Release x64 was rebuilt; source/Release x64 export counts remain
`388` / `388`.

`MODE-COLOR-001Y` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `HSV` sources to
numeric `I` and `F` targets in one DLL-owned loop. Each pixel first uses
`hsv_to_rgb_u8(...)`, preserving the established hue-sector float behavior;
I then stores rounded fixed-point RGB luma as little-endian int32, while F
stores unrounded weighted RGB luma as little-endian float32. Legal empty
sources produce empty targets. The facade uses the existing `Image.Convert`
route without an AHK pixel loop. Release x64 was rebuilt; source/Release x64
export counts remain `388` / `388`.

`MODE-COLOR-001X` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `CMYK` sources to
numeric `I` and `F` targets inside the existing DLL-owned CMYK loop. Each pixel
first uses `cmyk_to_rgb_u8(...)`, including Pillow-compatible black-channel
scaling and 255-denominator rounding; I then stores rounded fixed-point RGB
luma as little-endian int32, while F stores unrounded weighted RGB luma as
little-endian float32. Legal empty sources produce empty targets. The facade
uses the existing `Image.Convert` route without an AHK pixel loop. Release x64
was rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001W` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now convert `YCbCr` sources to
numeric `I` and `F` targets in one DLL-owned loop. Pillow's direct route first
clips YCbCr through RGB rather than promoting Y directly; the loop therefore
reuses `ycbcr_to_rgb_u8(...)`, then writes rounded fixed-point RGB luma as
little-endian int32 for I or unrounded weighted RGB luma as little-endian
float32 for F. Legal empty sources produce empty targets. The facade uses the
existing `Image.Convert` route without an AHK pixel loop. Release x64 was
rebuilt; source/Release x64 export counts remain `388` / `388`.

`MODE-COLOR-001V` changes no native signature. The existing
`pillow_c_image_convert_mode(...)` and
`pillow_c_image_convert_mode_into(...)` exports now promote mode `1`, `L`, and
`LA` sources to numeric `I` and `F` targets in one DLL-owned loop. Mode `1`
logical pixels become `0` or `255`, `L` bytes promote directly, and `LA` uses
only L while ignoring alpha. `I` stores little-endian int32 values and `F`
stores corresponding little-endian float32 values; legal empty sources produce
empty targets. The facade uses the existing `Image.Convert` route without an
AHK pixel loop. Release x64 was rebuilt; source/Release x64 export counts remain
`388` / `388`.

`META-002Q` changes no native signature but changes duplicate standard JPEG XMP
APP1 precedence. `apply_jpeg_xmp_metadata(...)` now overwrites the image
handle's cached XMP packet for each matching
`http://ns.adobe.com/xap/1.0/\0` segment, so the last packet matches Pillow
11.3.0 through `pillow_c_image_metadata_xmp(...)`, facade `Info["xmp"]`, and
`getxmp()`. Existing save routing remains unchanged: implicit quality/qtables
keep writes no XMP, while explicit `xmp=` writes one caller packet. Extended
XMP, malformed packets, arbitrary APP1 copying, and broader schemas remain
separate. Release x64 was rebuilt; source/Release x64 export counts remain
`388` / `388`.

`FMT-JPEG-003AT` changes no native signature but changes JPEG open metadata
precedence. `read_jpeg_metadata(...)` now overwrites the image handle's cached
comment for every encountered COM marker, so the last ordered payload matches
Pillow 11.3.0. `pillow_c_image_metadata_jpeg_comment(...)` consequently exposes
that last value, and existing qtables metadata save exports can emit it once;
the facade uses the same DLL-owned value for implicit `quality="keep"` and
`qtables="keep"` saves. Physical preservation of every COM segment, arbitrary
APP/unknown markers, non-RGB matrices, and exact entropy bytes remain separate.
Release x64 was rebuilt; source/Release x64 export counts remain `388` / `388`.

`FMT-JPEG-003AS` changes no native signature or implementation. The facade now
routes bounded opened RGB JPEG `qtables="keep"`, explicit integer
`subsampling=0/1/2`, and `restart_marker_rows=1` through the existing
`pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options(...)`.
The batched baseline 4:4:4, optimized 4:2:2, and progressive+optimize 4:2:0
calls receive `quality=-1`, two source qtables, implicit opened COM, explicit
ICC/EXIF, normalized subsampling, and row interval `1`. They write
SOF0/SOF0/SOF2, DRI `[6]` / `[3]` / `[3,6,3,6,3,6]`, and sampling-local RST
geometry. Baseline uses four standard DHT segments, optimized output uses four
compact segments, and progressive output now uses ten component-local segments
across ten scans after BV. Exact default-4:2:0 DHT/entropy byte emission,
quality combinations, keep-rgb, L/CMYK/YCCK, broader intervals, and arbitrary
marker copying remain separate. No Release rebuild was required;
source/Release x64 DLL export counts remain `388` / `388`, and the DLL remains
current from `FMT-JPEG-002B2AD`.

`FMT-JPEG-003AR` changes no native signature or implementation. The facade now
routes bounded opened RGB JPEG `qtables="keep"` optimized baseline and
progressive+optimize row-restart saves through the existing
`pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options(...)`
for both explicit source `subsampling="keep"` (`subsampling=1`) and omitted
default sampling (`subsampling=-1`). All four routes receive `quality=-1`, two
source qtables, implicit opened COM, explicit ICC/EXIF, and
`restart_marker_rows=1`. Optimized output writes SOF0, four compact DHT
segments, DRI `3`, and sampling-local restart counts. Progressive output writes
SOF2, ten SOS scans, DRI changes `[3,6,3,6,3,6]`, and scan-local restart
geometry; `progressive=True,optimize=False/True` is byte-identical in Pillow
11.3.0 and native progressive dispatch owns both public shapes. BV later
upgrades the shared encoder to ten component-local DHT segments and closes
exact source-4:2:2 per-scan DHT/SOS/entropy parity; default 4:2:0 exact parity
remains separate. Explicit integer subsampling is covered later by
`FMT-JPEG-003AS`; quality combinations, keep-rgb, L/CMYK/YCCK, broader
intervals, and arbitrary marker copying remain out of scope. No Release
rebuild was required; source/Release x64 DLL export counts
remain `388` / `388`, and the DLL remains current from `FMT-JPEG-002B2AD`.

`FMT-JPEG-003AQ` changes no native signature or implementation. Opened RGB
JPEG `qtables="keep"` with omitted subsampling now normalizes to native default
subsampling `-1` for both plain and bounded baseline restart saves instead of
copying source SOF sampling. The restart facade route calls the existing
`pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options(...)`
with `quality=-1`, two source qtables, `subsampling=-1`, implicit opened COM,
explicit ICC/EXIF, and `restart_marker_rows=1`. The export preserves DQT,
writes default 4:2:0 SOF sampling, standard DHT payload lengths
`[29,179,29,179]`, DRI `3`, and one `RST0`, and inserts EXIF/ICC/COM before
DQT. The raw regression exercises that exact ABI shape directly. Explicit
`subsampling="keep"` remains the source-4:2:2 branch covered by
`FMT-JPEG-003AP`; progressive/optimized qtables-keep restart, explicit integer
subsampling, quality combinations, keep-rgb, L/CMYK/YCCK, and arbitrary marker
copying remain outside this increment. No Release rebuild was required;
source/Release x64 DLL export counts remain `388` / `388`, and the DLL remains
current from `FMT-JPEG-002B2AD`.

`FMT-JPEG-003AP` changes no native signature or implementation. For the bounded
opened RGB JPEG `qtables="keep", subsampling="keep"` baseline restart route,
the facade resolves source qtables, source 4:2:2 subsampling, and implicit
opened COM before calling the existing
`pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options(...)`.
The export receives `quality=-1`, two source qtables, subsampling `1`, explicit
ICC/EXIF, and `restart_marker_rows=1`; it preserves DQT/SOF, writes standard
DHT payload lengths `[29,179,29,179]`, normalizes the row interval to DRI `3`,
emits `RST0,RST1,RST2`, and inserts EXIF/ICC/COM before DQT. The raw regression
exercises the ABI with opened custom-DQT source metadata. Pillow 11.3.0 uses
default 4:2:0 sampling when `subsampling` is omitted from this qtables-keep
combination, so that distinct branch remains outside this increment together
with progressive/optimized qtables-keep restart, keep-rgb, L/CMYK/YCCK, and
arbitrary marker copying. No Release rebuild was required; source/Release x64
DLL export counts remain `388` / `388`, and the DLL remains current from
`FMT-JPEG-002B2AD`.

`FMT-JPEG-003AO` changes no native signature or implementation. The facade now
resolves opened RGB JPEG `quality="keep"` / `subsampling="keep"` to source DQT
tables and native subsampling metadata before calling
`pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options(...)`.
The existing export receives `quality=-1`, source qtables, source 4:2:2
subsampling, implicit opened COM, explicit ICC/EXIF, and
`restart_marker_rows=1`; it preserves DQT/SOF, normalizes the row interval to
DRI `3`, emits `RST0,RST1,RST2`, and inserts metadata before DQT. The raw test
exercises this ABI directly. Progressive/optimized keep restart,
`qtables="keep"`, L/CMYK/YCCK keep restart, arbitrary marker copying, and
broader interval matrices remain outside this increment. No Release rebuild
was required; source/Release x64 DLL export counts remain `388` / `388` and
the DLL remains current from `FMT-JPEG-002B2AD`.

`FMT-JPEG-002B2AD` changes no signature and extends
`pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options(image,
path, quality, comment, comment_size, icc_profile, icc_profile_size, exif,
exif_size, xmp, xmp_size, qtables, qtable_count, subsampling, progressive,
optimize, restart_marker_blocks, restart_marker_rows)` to bounded
default-1x1 mode `CMYK`. One or two caller qtables remain contiguous
natural-order signed-int tables; the covered two-table route assigns table `0`
to C and table `1` to M/Y/K. A nonzero block interval is used directly; a row
interval is normalized as `ceil(width/8) * restart_marker_rows` and must fit
DRI's 16-bit range. The export routes native baseline, optimized, or
progressive CMYK encoding, then inserts optional EXIF/XMP/ICC/COM once after
APP14 and before DQT. Explicit CMYK subsampling with a nonzero restart interval
is rejected. DPI, keep-rgb, quality/qtables keep sentinels, quality presets,
broader interval matrices, exact libjpeg entropy/DHT byte parity, and YCCK
remain outside this increment. Release x64 was rebuilt with `0 Warning(s), 0
Error(s)`, and source/Release x64 DLL export counts remain `388` / `388`.

`FMT-JPEG-002B2AC` changes no signature and extends
`pillow_c_image_save_jpeg_metadata_restart_marker_progressive_options(image,
path, quality, comment, comment_size, icc_profile, icc_profile_size, exif,
exif_size, xmp, xmp_size, restart_marker_blocks, restart_marker_rows)` to
bounded default-1x1 mode `CMYK`. The export accepts one nonnegative block or
row interval family, derives CMYK row intervals as
`ceil(width/8) * restart_marker_rows`, writes one DRI before the first of 18
SOS scans, and starts each scan's cyclic restart sequence at `RST0`.
Interleaved DC-first boundaries reset C/M/Y/K predictors together; DC-refine
and the existing single-component AC-first/AC-refine writers byte-align and
emit restart markers at the same interval. Optional EXIF/XMP/ICC/COM is
validated and inserted once after APP14 and before DQT. QTables, DPI, real
subsampling, keep-rgb, broader interval matrices, exact libjpeg entropy/DHT
byte parity, and YCCK remain outside this increment. Release x64 was rebuilt
with `0 Warning(s), 0 Error(s)`, and source/Release x64 DLL export counts
remain `388` / `388`.

`FMT-JPEG-002B2AB` changes no signature and extends
`pillow_c_image_save_jpeg_metadata_restart_marker_encode_options(image, path,
quality, comment, comment_size, icc_profile, icc_profile_size, exif,
exif_size, xmp, xmp_size, restart_marker_blocks, restart_marker_rows,
optimize)` to bounded default-1x1 mode `CMYK` when `optimize` is nonzero.
Optional EXIF/XMP/ICC/COM is validated and inserted in Pillow order. CMYK row
intervals derive through the existing `ceil(width/8)` geometry; optimized
frequency collection resets C/M/Y/K DC predictors at matching intervals, and
entropy writes DRI/RST through the baseline CMYK encoder. The same metadata ABI
continues to reject CMYK when optimize is false or progressive routing is
requested. QTables, DPI, progressive CMYK restart output, keep-rgb, real
subsampling, broader matrices, and YCCK remain outside this increment. Release
x64 was rebuilt with `0 Warning(s), 0 Error(s)`, and source/Release x64 DLL
export counts remain `388` / `388`.

`FMT-JPEG-002B2AA` changes no signature and extends the existing
`pillow_c_image_save_jpeg_restart_marker_blocks_options(image, path, quality,
restart_marker_blocks)` and
`pillow_c_image_save_jpeg_restart_marker_rows_options(image, path, quality,
restart_marker_rows)` exports to bounded default-1x1 mode `CMYK` baseline
JPEG. Block intervals must fit DRI's 16-bit range. CMYK row intervals derive
as `ceil(width/8) * restart_marker_rows`, matching four-component `1x1` MCU
geometry. A nonzero interval writes DRI before SOS; entropy byte-aligns at each
reached interval, emits cyclic `RST0` through `RST7`, and resets all C/M/Y/K DC
predictors. The covered quality-95 block/row outputs preserve APP14 transform
`0`, table-0 C/M/Y/K SOF0 components, standard DHTs, and exact reopened CMYK
bytes. The plain metadata restart export and optimize-false metadata encode
calls remain explicitly restricted to mode `L` or `RGB`; `FMT-JPEG-002B2AB`
later opens only optimized CMYK metadata restart output. CMYK qtables, DPI,
progressive, keep-rgb, real subsampling, broader interval matrices, and YCCK
remain outside this ABI increment. Release x64 was rebuilt with `0 Warning(s), 0 Error(s)`,
and source/Release x64 DLL export counts remain `388` / `388`.

`FMT-JPEG-002B2Z` adds
`pillow_c_image_save_jpeg_keep_rgb_restart_marker_encode_options(image, path,
quality, comment, comment_size, icc_profile, icc_profile_size, exif,
exif_size, xmp, xmp_size, qtables, qtable_count, progressive, optimize,
restart_marker_blocks, restart_marker_rows)`. The additive export is restricted
to mode `RGB` and implies `keep_rgb=True`. `qtables == nullptr` with
`qtable_count == 0` selects default RGB-component tables; otherwise the caller
passes one or two contiguous natural-order 64-entry signed-int tables. It
validates optional EXIF/XMP/ICC/COM buffers, encode flags, qtables, and one
mutually exclusive nonnegative restart family before writing output. Row
intervals use `ceil(width/8) * restart_marker_rows`, matching keep-RGB's
R/G/B `1x1` component geometry, and must fit DRI's 16-bit interval. Baseline
output writes DRI before SOS, resets optimized DC frequency collection at
matching boundaries, and emits cyclic RST markers. Progressive output writes
one stable DRI before its first SOS and passes the same interval to all
fourteen interleaved and single-component scans; every scan starts at `RST0`.
Metadata is inserted once after APP14 transform `0` and before DQT. The shared
progressive AC-refinement encoder also now partitions buffered correction bits
at each 16-zero ZRL boundary, eliminating bad-Huffman warnings on the covered
larger fixture. CMYK, DPI, quality/qtables keep sentinels, broader restart
matrices, and exact libjpeg DHT/entropy byte parity remain outside this ABI.
Release x64 was rebuilt with `0 Warning(s), 0 Error(s)`, and source/Release x64
DLL export counts are now `388` / `388`.

`FMT-JPEG-002B2Y` adds
`pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options(image,
path, quality, comment, comment_size, icc_profile, icc_profile_size, exif,
exif_size, xmp, xmp_size, qtables, qtable_count, subsampling, progressive,
optimize, restart_marker_blocks, restart_marker_rows)`. The additive export
accepts one or two natural-order 64-entry quantization tables, optional
EXIF/XMP/ICC/COM buffers, explicit RGB subsampling, encode-strategy flags, and
one mutually exclusive restart interval family. It refreshes readonly source
buffers and routes the shared mode `L` or RGB qtables encoder without a DPI
surface. Mode `L` uses qtable `0` and normalizes row intervals to 8x8 block
columns. Baseline RGB derives row intervals from its actual horizontal and
vertical sampling factors, writes DRI before SOS, and passes the interval to
interleaved entropy encoding. Optimized baseline frequency collection resets
Y, Cb, and Cr DC predictors at matching restart boundaries. Progressive RGB
uses its existing scan-geometry intervals, including luma-only width changes,
and emits a new DRI only when the active interval changes. Metadata is inserted
once before DQT in Pillow order. The covered L, RGB 4:4:4 optimized, and RGB
4:2:2 progressive fixtures preserve custom DQT/SOF/DRI/RST structure and reopen
through the DLL. CMYK, keep-rgb, quality presets/keep sentinels, DPI, broader
restart matrices, and exact libjpeg entropy/DHT byte parity remain outside this
ABI. Release x64 was rebuilt with `0 Warning(s), 0 Error(s)`, and
source/Release x64 DLL export counts are now `387` / `387`.

`FMT-JPEG-002B2X` adds
`pillow_c_image_save_jpeg_metadata_restart_marker_progressive_options(image,
path, quality, comment, comment_size, icc_profile, icc_profile_size, exif,
exif_size, xmp, xmp_size, restart_marker_blocks, restart_marker_rows)`. The
additive export validates the same explicit metadata and mutually exclusive
interval families as the existing restart metadata routes, refreshes readonly
source buffers, and selects native progressive mode `L` or default-4:2:0 mode
`RGB` encoding. Mode `L` converts row intervals to block intervals and writes
one stable DRI value before its first of six SOS scans. RGB block intervals stay
constant across all scans; RGB row intervals derive interleaved/chroma width
from MCU columns and luma-only width from MCU columns times horizontal sampling,
writing a new DRI only when the active scan interval changes. The covered RGB
row fixture writes DRI sequence `[3,6,3,6,3,6]` across ten scans. Progressive
DC frequency collection resets at restart boundaries, DC-first/DC-refine and
AC-first/AC-refine entropy scans byte-align and emit cyclic RST markers, and
each SOS scan starts its sequence at `RST0`. Explicit EXIF/XMP/ICC/COM is then
inserted once in Pillow order. Pillow 11.3.0 emits byte-identical output for the
covered `progressive=True,optimize=False/True` combinations, so the facade uses
this progressive ABI for either optimize value. QTables/custom subsampling,
keep-rgb, CMYK, broader interval matrices, and exact libjpeg entropy/DHT byte
parity remain outside this ABI. Release x64 was rebuilt with `0 Warning(s), 0
Error(s)`, and source/Release x64 DLL export counts are now `386` / `386`.

`FMT-JPEG-002B2W` adds
`pillow_c_image_save_jpeg_metadata_restart_marker_encode_options(image, path,
quality, comment, comment_size, icc_profile, icc_profile_size, exif,
exif_size, xmp, xmp_size, restart_marker_blocks, restart_marker_rows,
optimize)`. The additive export extends the bounded metadata restart route with
an encode-strategy flag: nonzero `optimize` selects native optimized-Huffman
baseline output and zero uses the existing standard-Huffman core. Metadata and
interval validation remain identical to the prior export. Optimized mode `L`
frequency collection resets the DC predictor every DRI block interval; default
`4:2:0` mode `RGB` converts the MCU interval to a luma-block interval and uses
the MCU interval directly for Cb/Cr, matching the entropy writer's Y/Cb/Cr
predictor resets. The covered L output has optimized DHT payload lengths
`[20,31]`; covered RGB output deterministically has `[22,44,22,36]`. Pillow
11.3.0/libjpeg writes `[22,44,23,36]` for that RGB fixture because its
coefficient stream includes chroma DC category `0`; the native ABI does not
inject an unused symbol solely for exact DHT byte-shape parity. Both routes
write DRI `3`, emit one `RST0`, and reopen through the DLL, while the RGB route
preserves explicit EXIF/XMP/ICC/COM metadata. Existing standard-Huffman restart
exports remain unchanged. Progressive output, qtables/custom subsampling,
keep-rgb, CMYK, broader interval matrices, and exact libjpeg entropy/DHT byte
parity remain outside this ABI. Release x64 was rebuilt with `0 Warning(s), 0
Error(s)`, and source/Release x64 DLL export counts are now `385` / `385`.

`FMT-JPEG-002B2V` adds
`pillow_c_image_save_jpeg_metadata_restart_marker_options(image, path,
quality, comment, comment_size, icc_profile, icc_profile_size, exif,
exif_size, xmp, xmp_size, restart_marker_blocks, restart_marker_rows)`. The
bounded export accepts baseline mode `L` or default-4:2:0 mode `RGB`, validates
all explicit metadata before creating output, rejects simultaneous nonzero
block and row intervals, converts rows through the shared native MCU geometry
helper, and writes standard-Huffman restart entropy through the shared core.
It then inserts EXIF APP1, XMP APP1, ICC APP2, and COM once in Pillow order;
unlike the plain restart exports, it does not first patch the opened image's
source comment. Negative or overflowing intervals, unsupported modes, invalid
metadata, and conflicting interval families return explicit status errors.
QTables/custom subsampling, optimized/progressive output, keep-rgb, and CMYK
remain outside this ABI. Release x64 was rebuilt with `0 Warning(s), 0
Error(s)`, and source/Release x64 DLL export counts are now `384` / `384`.

`FMT-JPEG-002B2U` adds
`pillow_c_image_save_jpeg_restart_marker_rows_options(image, path, quality,
restart_marker_rows)`. The bounded export accepts owned or refreshed mode `L`
and `RGB` images on the baseline standard-Huffman route. It derives MCU columns
as `(width + 7) / 8` for L and `(width + 15) / 16` for default `4:2:0` RGB,
multiplies that width by `restart_marker_rows`, validates the result against
the 16-bit DRI range, and delegates to the existing restart-block encoder. A
negative row count, unsupported mode, invalid width, or overflowing interval
returns `PILLOW_C_INVALID_ARGUMENT`; zero writes ordinary baseline output
without DRI. Metadata, custom qtables/subsampling, optimized/progressive
output, keep-rgb, and CMYK remain outside this bounded export. Release x64 was
rebuilt with `0 Warning(s), 0 Error(s)`, and source/Release x64 DLL export
counts are now `383` / `383`.

`FMT-JPEG-002B2T` changes no signature and extends the existing
`pillow_c_image_save_jpeg_restart_marker_blocks_options` export to bounded
mode `RGB` images. The RGB path uses default `4:2:0` sampling and standard
luminance/chrominance Huffman tables, writes the 16-bit DRI interval before
SOS, byte-aligns at each reached interleaved MCU interval, emits cyclic restart
markers, and resets all Y/Cb/Cr DC predictors. Metadata, custom qtables or
subsampling, optimized/progressive output, keep-rgb, CMYK, row-based markers,
and broader interval matrices remain outside this bounded route. Release x64
was rebuilt with `0 Warning(s), 0 Error(s)`, and source/Release x64 DLL export
counts remain `382` / `382`.

`FMT-JPEG-002B2S` adds
`pillow_c_image_save_jpeg_restart_marker_blocks_options(image, path, quality,
restart_marker_blocks)`. The bounded export accepts an owned or refreshed
mode `L` image and writes a baseline standard-Huffman JPEG. A nonzero
`restart_marker_blocks` value is stored as the 16-bit DRI interval; entropy is
byte-aligned at each reached MCU interval, cyclic `RST0` through `RST7`
markers are emitted, and the DC predictor resets before the next MCU. A zero
value writes ordinary baseline output without DRI. Metadata, custom qtables,
optimized/progressive output, CMYK, and row-based restart markers are not
part of this export. Release x64 was rebuilt with `0 Warning(s), 0 Error(s)`,
and source/Release x64 DLL export counts are now `382` / `382`.

`FMT-JPEG-003AN` adds no export and changes no native implementation. Opened
mode `CMYK` JPEG `subsampling="keep"` is a facade sentinel normalization:
when `pillow_c_image_metadata_jpeg_subsampling` reports `-1` for a
four-component JPEG, the facade passes explicit default subsampling `-1`
instead of applying the RGB missing-metadata rejection. With
`quality="keep"`, stored qtables continue through the existing
`pillow_c_image_save_jpeg_qtables_encode_options` route; the existing raw real-
YCCK keep test remains the native companion. The sentinel is excluded from
facade guards for explicit CMYK subsampling `0`/`1`/`2`, while other modes
retain the existing error. No Release x64 rebuild was required, and source/DLL
export counts remain `381` / `381`.

`META-001CV` adds no export and reuses the existing TIFF EXIF metadata blob
for opened L-mode TIFF handles whose IFD0 carries bounded `CompositeImage`
scalar integer tag `42080`, exactly-two-value `CompositeImageCount`
SHORT-array tag `42081`, and exactly-two-value
`CompositeImageExposureTimes` RATIONAL-array tag `42082`. The canonical
fixture stores tag `42080` as SHORT count `1`; the shared scalar route also
accepts LONG count `1`, matching a follow-up Pillow 11.3.0 probe. Native TIFF
open serializes the three tags through the existing typed EXIF blob routes,
and the facade enumerates them through
`Image.Exif.FromImage()` for public `Image.GetExif()` / `getexif()` readback
without exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts
remain `381` / `381`.

`META-001CU` adds no export and reuses the existing TIFF EXIF metadata blob
for opened L-mode TIFF handles whose IFD0 carries bounded `Gamma` tag `42240`
as TIFF RATIONAL type `5`, count `1`. Native TIFF open serializes the tag
through the scalar rational route, and the facade enumerates it through
`Image.Exif.FromImage()` for public `Image.GetExif()` / `getexif()`
readback without exposing TIFF `Info["exif"]`. Source and Release x64 DLL
export counts remain `381` / `381`.

Facade-only compatibility notes: `API-IMG-001A` adds
`Image.FormatDescription` / `Image.format_description` in `ahk/pillow.ahk` by
mapping the existing facade `Image.Format` value to Pillow 11.3.0's format
description strings. `API-IMG-001B` adds `Image.HasTransparencyData` /
`Image.has_transparency_data` by mapping the current facade mode and
`Info["transparency"]` metadata to Pillow 11.3.0's object-property boundary.
`API-IMG-001C` adds `Image.GetChildImages()` / `Image.get_child_images()` as
the bounded empty-list object API for currently implemented image handles.
`MODE-NUM-001L` routes instance `Image.GetBands()` / `Image.getbands()`
through the shared facade mode-info table so numeric modes expose Pillow-style
`I` / `F` band names instead of an empty list. `MODE-NUM-001M` updates the
facade `ImageStat.Stat` constructor to reject unmasked empty numeric `I` and
`F` images with Pillow's `min/max not given` error before using the native
histogram route. `MODE-NUM-001N` updates the facade `ImageOps.Invert`,
`ImageOps.Posterize`, and `ImageOps.Solarize` error mapping for numeric modes
`I` and `F`: the existing native ImageOps LUT gate already returns
`PILLOW_C_INVALID_ARGUMENT` for those modes, and the facade now exposes
Pillow-style `not supported for mode ...` messages without changing the
exported ABI. `MODE-NUM-001O` updates the facade `ImageOps.Equalize` and
`ImageOps.Autocontrast` error mapping for numeric modes `I` and `F`: the
existing native equalize/autocontrast gates already return
`PILLOW_C_INVALID_ARGUMENT` for those modes, and the facade now exposes
Pillow-style `not supported for mode ...` or `image has wrong mode` messages
according to the bounded local Pillow 11.3.0 boundary. These slices added or
changed no native ABI symbol.
`FMT-JPEG-002B2R` is also facade-only: opened RGB JPEG
`Image.Save(..., "JPEG", {qtables:"keep", keep_rgb:true})` now routes to the
existing native qtables keep-rgb export with default RGB keep-rgb subsampling
instead of copying the opened sampled YCbCr subsampling. The existing native
route already preserves opened DQT tables and opened COM/comment metadata for
this shape, so no export or DLL rebuild was required.
`API-JPEG-001` changes no export signature. The existing
`pillow_c_image_metadata_jpeg_subsampling` export now also recognizes bounded
four-component `C/M/Y/K` SOF sampling, and the facade uses that value to
resolve opened CMYK JPEG `subsampling="keep"` metadata saves while passing the
opened COM/comment to the existing metadata encode route when public `comment`
is omitted. Source and Release x64 DLL export counts remain `379` / `379`.
`API-PNG-001` is facade-only. `PngInfo.add(cid, data)` now stores string
chunk IDs instead of silently returning, and `Image.Save(..., "PNG",
{pnginfo: ...})` raises the Pillow 11.3.0 string/private-chunk error message
`can't concat str to bytes` when such a private chunk would be emitted. Buffer
chunk IDs continue to route through the existing native PNG custom-chunk
exports. No native ABI symbol was added, no native rebuild was required, and
source/Release x64 DLL export counts remain `379` / `379`.
`API-STATUS-001` is facade-only. The audited metadata calls that pass real
output buffers now call `Pillow.CheckStatus(status)` directly instead of using
the two-call size-probe `if status != -1` idiom: JPEG qtable count,
per-table JPEG qtable reads, JPEG subsampling, and PNG chromaticity. This
changes facade error propagation only; native signatures, status codes, and
export counts are unchanged at `379` / `379`.
`ROBUST-001` adds no export and changes no signature. The internal
`inflate_zlib_deflate` helper now requires an `expected_max` cap and stops
stored/fixed/dynamic deflate output before it can grow past that bound. PNG
open uses Pillow 11.3.0's `PngImagePlugin.MAX_TEXT_CHUNK` value (`1048576`)
for compressed `zTXt`, compressed `iTXt`, and `iCCP` metadata; cap-exceeded
payloads make `pillow_c_image_open_png` return `PILLOW_C_INVALID_ARGUMENT`
while malformed compressed metadata keeps the previous ignored-metadata
behavior. TIFF Adobe Deflate decode passes the expected strip byte count as
the same cap. Release x64 was rebuilt after native code changed, and
source/Release x64 DLL export counts remain `379` / `379`.
`ROBUST-002` adds no export and changes no signature. It tightens existing
C ABI behavior: `open_gif_composited_frame_image` now catches allocation
failure from file reads, GIF color-table allocation, and frame decode work;
`gif_lzw_decode_indices` caps appended output at the remaining expected pixel
count; BMP open rejects the `INT_MIN` top-down height before negating it; JPEG
metadata scanning requires a `0xFF` marker prefix and JFIF density patching
stops at SOS/EOI; and `pillow_c_exif_orientation_bytes` treats a null output
buffer with size `0` as the normal two-call size probe, returning
`PILLOW_C_OK` after setting the required byte count. Release x64 was rebuilt
after native code changed, and source/Release x64 DLL export counts remain
`379` / `379`.
`META-002A` adds `pillow_c_image_metadata_xmp` as the shared raw XMP metadata
export for opened PNG and JPEG handles. Native PNG open extracts the bounded
`iTXt` key `XML:com.adobe.xmp`, native JPEG open extracts APP1
`http://ns.adobe.com/xap/1.0/\0` payloads, and the facade maps the copied
bytes to `Info["xmp"]` before parsing the bounded `Image.getxmp()` nested-map
shape.
`META-002B` adds
`pillow_c_image_save_jpeg_metadata_xmp_encode_options(image, path, quality,
has_dpi, dpi_x, dpi_y, comment, comment_size, icc_profile, icc_profile_size,
exif, exif_size, xmp, xmp_size, subsampling, progressive, optimize)` for the
bounded explicit JPEG `xmp=` save route. The export writes a Pillow-style APP1
XMP segment with header `http://ns.adobe.com/xap/1.0/\0`, ordered after EXIF
and before ICC/COM metadata, then existing open metadata exposes it through
`pillow_c_image_metadata_xmp`. Source and Release x64 DLL export counts are now
`374` / `374`.
`META-002C` adds
`pillow_c_image_save_jpeg_qtables_metadata_xmp_encode_options(image, path,
quality, has_dpi, dpi_x, dpi_y, comment, comment_size, icc_profile,
icc_profile_size, exif, exif_size, xmp, xmp_size, qtables, qtable_count,
subsampling, progressive, optimize)` for the bounded explicit JPEG
`qtables + xmp=` save route. The export preserves the existing qtables
metadata argument shape, writes a Pillow-style APP1 XMP segment before DQT
payloads, and reuses the existing open metadata path so reopened images expose
the bytes through `pillow_c_image_metadata_xmp`. Source and Release x64 DLL
export counts are now `375` / `375`.
`META-002D` adds
`pillow_c_image_save_jpeg_metadata_keep_rgb_xmp_encode_options(image, path,
quality, has_dpi, dpi_x, dpi_y, comment, comment_size, icc_profile,
icc_profile_size, exif, exif_size, xmp, xmp_size, subsampling, progressive,
optimize, keep_rgb)` for the bounded explicit JPEG no-qtables
`keep_rgb + xmp=` save route. The export preserves the existing keep-rgb
metadata argument shape, writes a Pillow-style APP14 Adobe transform `0` plus
APP1 XMP before DQT payloads, and reuses the existing open metadata path so
reopened images expose the bytes through `pillow_c_image_metadata_xmp`. Source
and Release x64 DLL export counts are now `376` / `376`.
`META-002E` adds
`pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_xmp_encode_options(image,
path, quality, has_dpi, dpi_x, dpi_y, comment, comment_size, icc_profile,
icc_profile_size, exif, exif_size, xmp, xmp_size, qtables, qtable_count,
subsampling, progressive, optimize, keep_rgb)` for the bounded explicit JPEG
`qtables + keep_rgb + xmp=` save route. The export preserves the existing
qtables keep-rgb argument shape, writes APP14 Adobe transform `0` plus APP1
XMP before DQT payloads, uses RGB component IDs with caller qtables, and
reuses the existing open metadata path so reopened images expose the bytes
through `pillow_c_image_metadata_xmp`. Source and Release x64 DLL export
counts are now `377` / `377`.
`META-002F` adds no export and reuses the existing
`pillow_c_image_metadata_xmp` raw byte route. Native PNG open continues to
attach the exact `XML:com.adobe.xmp` iTXt payload bytes; the facade
`Image.GetXmp()` / `Image.getxmp()` parser now matches Pillow 11.3.0 for the
bounded repeated RDF sequence shape by returning arrays for repeated sibling
names and scalar strings for text-only leaves. The ABI signature and export
count did not change.
`META-002G` adds `pillow_c_image_metadata_tiff_icc_profile` as the bounded TIFF
ICC profile metadata export for opened TIFF handles. Native TIFF open copies
IFD0 tag `34675` only when it is TIFF type `7` (`UNDEFINED`) into DLL-owned
ICC metadata, exposes the bytes through that export, and also serializes the
same bytes into the existing `pillow_c_image_metadata_tiff_exif` blob as tag
`34675`. The facade maps the opened TIFF bytes to `Info["icc_profile"]` and
enumerates `getexif()[34675]` without changing TIFF `Info["exif"]` absence.
Source and Release x64 DLL export counts are now `381` / `381`.
`META-002H` adds no export and reuses the same TIFF ICC metadata ABI for
native numeric/high-bit TIFF early-open handles. Native `I`, `F`, and `I;16`
TIFF opens now attach bounded IFD0 tag `34675` bytes when stored as TIFF
UNDEFINED type `7`, and expose the same bytes through
`pillow_c_image_metadata_tiff_icc_profile` and the existing TIFF EXIF blob.
Source and Release x64 DLL export counts remain `381` / `381`.
`META-002I` adds no export and reuses the same TIFF ICC metadata ABI for
selected RGB TIFF frame handles. Native `pillow_c_image_open_tiff_frame` now
attaches bounded tag `34675` bytes from the selected IFD when stored as TIFF
UNDEFINED type `7`, and exposes the same frame bytes through
`pillow_c_image_metadata_tiff_icc_profile` and the existing TIFF EXIF blob.
Source and Release x64 DLL export counts remain `381` / `381`.
`META-002J` adds no export and reuses the same TIFF ICC metadata ABI for
selected little-endian `I;16` TIFF frame handles. Native
`pillow_c_image_open_tiff_frame` now parses nonzero `I;16` frames from the
selected IFD before WIC fallback, preserving mode `I;16` and exposing bounded
tag `34675` bytes through `pillow_c_image_metadata_tiff_icc_profile` and the
existing TIFF EXIF blob. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-002K` adds no export and reuses the same TIFF ICC metadata ABI for
selected little-endian numeric `I` and `F` TIFF frame handles. Native
`pillow_c_image_open_tiff_frame` now parses nonzero numeric frames from the
selected IFD before WIC fallback, preserving mode `I` or `F`, preserving raw
32-bit sample bytes, and exposing bounded tag `34675` bytes through
`pillow_c_image_metadata_tiff_icc_profile` and the existing TIFF EXIF blob.
Source and Release x64 DLL export counts remain `381` / `381`.
`META-002L` adds no export and reuses the same TIFF ICC metadata ABI for
opened TIFF handles whose IFD0 `ICCProfile` tag `34675` is stored as TIFF BYTE
type `1`. Native TIFF ICC parsing now accepts only bounded BYTE type `1` and
UNDEFINED type `7` payloads for tag `34675`, exposes the bytes through
`pillow_c_image_metadata_tiff_icc_profile`, and serializes the same bytes into
the existing TIFF EXIF blob. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-002M` adds no export and reuses the existing raw XMP metadata ABI plus
the existing TIFF EXIF blob for opened TIFF handles whose IFD0 XMP tag `700`
is stored as TIFF BYTE type `1`. Native TIFF open copies the bounded tag `700`
payload into `pillow_c_image_metadata_xmp` metadata and serializes the same
bytes into `pillow_c_image_metadata_tiff_exif` as an UNDEFINED tag so the
facade can map `Info["xmp"]`, `getxmp()`, and `getexif()[700]`. Source and
Release x64 DLL export counts remain `381` / `381`.
`META-002N` adds no export and reuses the existing raw XMP metadata ABI plus
the existing TIFF EXIF blob for native `I;16`, `I`, and `F` TIFF early-open
handles whose IFD0 XMP tag `700` is stored as TIFF BYTE type `1`. Native
numeric and `I;16` TIFF early returns now copy bounded tag `700` payloads into
`pillow_c_image_metadata_xmp` metadata before returning, while the existing
TIFF EXIF blob route continues to expose public `getexif()[700]` readback.
Source and Release x64 DLL export counts remain `381` / `381`.
`META-002O` adds no export and reuses the existing raw XMP metadata ABI plus
the existing TIFF EXIF blob for selected nonzero little-endian `I;16`, `I`,
and `F` TIFF frame handles whose selected IFD XMP tag `700` is stored as TIFF
BYTE type `1`. Native `pillow_c_image_open_tiff_frame` now attaches bounded
selected-IFD tag `700` bytes to `pillow_c_image_metadata_xmp` before returning
from the numeric and `I;16` frame parsers, while the existing TIFF EXIF blob
route exposes public `getexif()[700]` readback for that selected frame. Source
and Release x64 DLL export counts remain `381` / `381`.
`META-002P` adds no export and reuses the existing raw XMP metadata ABI plus
the existing TIFF EXIF blob for opened RGB TIFF handles whose IFD0 XMP tag
`700` is stored as TIFF UNDEFINED type `7`. Native TIFF XMP parsing now
accepts bounded tag `700` payloads stored as TIFF BYTE type `1` or UNDEFINED
type `7`, attaches the bytes to `pillow_c_image_metadata_xmp`, and leaves
public `getexif()[700]` readback on the existing TIFF EXIF blob route. Source
and Release x64 DLL export counts remain `381` / `381`.
`META-001AX` adds no export and reuses the existing TIFF EXIF metadata blob
for opened L-mode TIFF handles whose IFD0 carries bounded fax scalar tags
`326` (`BadFaxLines`), `327` (`CleanFaxData`), and `328`
(`ConsecutiveBadFaxLines`). Native TIFF open serializes those tags through the
existing scalar integer route, and the facade enumerates them through
`Image.Exif.FromImage()` for public `Image.GetExif()` / `Image.getexif()`
readback without exposing TIFF `Info["exif"]`. Source and Release x64 DLL
export counts remain `381` / `381`.
`META-001AY` adds no export and reuses the existing TIFF EXIF metadata blob
for opened L-mode TIFF handles whose IFD0 carries bounded fax option scalar
tags `292` (`Group3Options`) and `293` (`Group4Options`). Native TIFF open
serializes those tags through the existing scalar integer route, and the facade
enumerates them through `Image.Exif.FromImage()` for public
`Image.GetExif()` / `Image.getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001AZ` adds no export and reuses the existing TIFF EXIF metadata blob
for opened L-mode TIFF handles whose IFD0 carries bounded free block scalar
tags `288` (`FreeOffsets`) and `289` (`FreeByteCounts`). Native TIFF open
serializes those tags through the existing scalar integer route, and the facade
enumerates them through `Image.Exif.FromImage()` for public
`Image.GetExif()` / `Image.getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BA` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded uncompressed single-strip L-mode TIFF handles whose IFD0 carries
bounded tile shape scalar tags `322` (`TileWidth`) and `323` (`TileLength`).
Native TIFF open recognizes that bounded shape before the WIC decoder path and
serializes those tags through the existing scalar integer route; the facade
enumerates them through `Image.Exif.FromImage()` for public
`Image.GetExif()` / `Image.getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BB` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries bounded tile
byte range scalar tags `324` (`TileOffsets`) and `325` (`TileByteCounts`).
Native TIFF EXIF serialization admits those tags through the existing scalar
integer route; the facade enumerates them through `Image.Exif.FromImage()` for
public `Image.GetExif()` / `Image.getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BC` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `JPEGTables`
tag `347` as TIFF UNDEFINED type `7`. Native TIFF EXIF serialization admits
that tag through the bounded UNDEFINED payload route; the facade enumerates it
through `Image.Exif.FromImage()` for public `Image.GetExif()` /
`getexif()` readback without exposing TIFF `Info["exif"]`. Source and Release
x64 DLL export counts remain `381` / `381`.
`META-001BD` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `JPEGTables`
tag `347` as TIFF BYTE type `1`. Native TIFF EXIF serialization admits that
tag through the bounded BYTE-or-UNDEFINED payload route; the facade's existing
`Image.Exif.FromImage()` undefined-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BE` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `ExifVersion`
tag `36864` as TIFF UNDEFINED type `7`. Native TIFF EXIF serialization admits
that tag through the bounded UNDEFINED payload route; the facade's existing
`Image.Exif.FromImage()` undefined-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BF` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`DateTimeOriginal` tag `36867` as TIFF ASCII type `2`. Native TIFF EXIF
serialization admits that tag through the bounded ASCII payload route; the
facade's `Image.Exif.FromImage()` ASCII-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BG` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`DateTimeDigitized` tag `36868` as TIFF ASCII type `2`. Native TIFF EXIF
serialization admits that tag through the bounded ASCII payload route; the
facade's `Image.Exif.FromImage()` ASCII-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BH` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `SubSecTime`
tag `37520` as TIFF ASCII type `2`. Native TIFF EXIF serialization admits
that tag through the bounded ASCII payload route; the facade's
`Image.Exif.FromImage()` ASCII-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BI` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`SubSecTimeOriginal` tag `37521` as TIFF ASCII type `2`. Native TIFF EXIF
serialization admits that tag through the bounded ASCII payload route; the
facade's `Image.Exif.FromImage()` ASCII-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BJ` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`SubSecTimeDigitized` tag `37522` as TIFF ASCII type `2`. Native TIFF EXIF
serialization admits that tag through the bounded ASCII payload route; the
facade's `Image.Exif.FromImage()` ASCII-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BK` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `OffsetTime`
tag `36880` as TIFF ASCII type `2`. Native TIFF EXIF serialization admits
that tag through the bounded ASCII payload route; the facade's
`Image.Exif.FromImage()` ASCII-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF `Info["exif"]`.
Source and Release x64 DLL export counts remain `381` / `381`.
`META-001BL` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`OffsetTimeOriginal` tag `36881` as TIFF ASCII type `2`. Native TIFF EXIF
serialization admits that tag through the bounded ASCII payload route; the
facade's `Image.Exif.FromImage()` ASCII-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF `Info["exif"]`.
Source and Release x64 DLL export counts remain `381` / `381`.
`META-001BM` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`OffsetTimeDigitized` tag `36882` as TIFF ASCII type `2`. Native TIFF EXIF
serialization admits that tag through the bounded ASCII payload route; the
facade's `Image.Exif.FromImage()` ASCII-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF `Info["exif"]`.
Source and Release x64 DLL export counts remain `381` / `381`.
`META-001BN` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `ExifIFD`
pointer tag `34665` as TIFF LONG type `4`. Native TIFF EXIF serialization
admits that tag through the bounded scalar integer route, and the facade's
`Image.Exif.FromImage()` integer-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF `Info["exif"]`.
Nested EXIF IFD traversal remains outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `381` / `381`.
`META-001BO` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `GPSInfo`
pointer tag `34853` as TIFF LONG type `4`. Native TIFF EXIF serialization
admits that tag through the bounded scalar integer route, and the facade's
`Image.Exif.FromImage()` integer-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF `Info["exif"]`.
Nested GPS IFD traversal remains outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `381` / `381`.
`META-001BP` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`ImageUniqueID` tag `42016` as TIFF ASCII type `2`. Native TIFF EXIF
serialization admits that tag through the bounded ASCII payload route, and the
facade's `Image.Exif.FromImage()` ASCII-tag enumeration exposes it for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF `Info["exif"]`.
Source and Release x64 DLL export counts remain `381` / `381`.
`META-001BQ` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `XPComment` tag
`40092` as TIFF BYTE type `1`. Native TIFF EXIF serialization admits that tag
through the bounded BYTE-array route, and the facade's
`Image.Exif.FromImage()` byte-array enumeration exposes a copied Buffer for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001BR` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `XPAuthor` tag
`40093` as TIFF BYTE type `1`. Native TIFF EXIF serialization admits that tag
through the bounded BYTE-array route, and the facade's
`Image.Exif.FromImage()` byte-array enumeration exposes a copied Buffer for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001BS` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `XPKeywords`
tag `40094` as TIFF BYTE type `1`. Native TIFF EXIF serialization admits that
tag through the bounded BYTE-array route, and the facade's
`Image.Exif.FromImage()` byte-array enumeration exposes a copied Buffer for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001BT` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `XPSubject` tag
`40095` as TIFF BYTE type `1`. Native TIFF EXIF serialization admits that tag
through the bounded BYTE-array route, and the facade's
`Image.Exif.FromImage()` byte-array enumeration exposes a copied Buffer for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001BU` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`FlashPixVersion` tag `40960` as TIFF UNDEFINED type `7`. Native TIFF EXIF
serialization admits that tag through the bounded UNDEFINED route, and the
facade's `Image.Exif.FromImage()` undefined-tag enumeration exposes a copied
Buffer for public `Image.GetExif()` / `getexif()` readback without exposing
TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BV` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `ColorSpace`
tag `40961` as TIFF SHORT type `3`, count `1`. Native TIFF EXIF serialization
admits that tag through the bounded scalar integer route, and the facade's
`Image.Exif.FromImage()` scalar-tag enumeration exposes integer `1` for public
`Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BW` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`PixelXDimension` tag `40962` as TIFF LONG type `4`, count `1`. Native TIFF
EXIF serialization admits that tag through the bounded scalar integer route,
and the facade's `Image.Exif.FromImage()` scalar-tag enumeration exposes
integer `2` for public `Image.GetExif()` / `getexif()` readback without
exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-001BX` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`PixelYDimension` tag `40963` as TIFF LONG type `4`, count `1`. Native TIFF
EXIF serialization admits that tag through the bounded scalar integer route,
and the facade's `Image.Exif.FromImage()` scalar-tag enumeration exposes
integer `1` for public `Image.GetExif()` / `getexif()` readback without
exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-001BY` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`RelatedSoundFile` tag `40964` as TIFF ASCII type `2`, count `13`. Native
TIFF EXIF serialization admits that tag through the bounded ASCII route, and
the facade's `Image.Exif.FromImage()` ASCII-tag enumeration exposes
`SOUND000.WAV` for public `Image.GetExif()` / `getexif()` readback without
exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-001BZ` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `FlashEnergy`
tag `41483` as TIFF RATIONAL type `5`, count `1`. Native TIFF EXIF
serialization admits that tag through the bounded scalar RATIONAL route, and
the facade's `Image.Exif.FromImage()` rational-tag enumeration exposes
`[9, 2]` for public `Image.GetExif()` / `getexif()` readback without exposing
TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-001CA` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`FocalPlaneXResolution` tag `41486` and `FocalPlaneYResolution` tag `41487`
as TIFF RATIONAL type `5`, count `1`. Native TIFF EXIF serialization admits
those tags through the bounded scalar RATIONAL route, and the facade's
`Image.Exif.FromImage()` rational-tag enumeration exposes `[300, 7]` and
`[600, 11]` for public `Image.GetExif()` / `getexif()` readback without
exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-001CB` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`FocalPlaneResolutionUnit` tag `41488` as TIFF SHORT type `3`, count `1`.
Native TIFF EXIF serialization admits that tag through the bounded scalar
integer route, and the facade's `Image.Exif.FromImage()` integer-tag
enumeration exposes integer `3` for public `Image.GetExif()` / `getexif()`
readback without exposing TIFF `Info["exif"]`. Source and Release x64 DLL
export counts remain `381` / `381`.
`META-001CC` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `SensingMethod`
tag `41495` as TIFF SHORT type `3`, count `1`. Native TIFF EXIF serialization
admits that tag through the bounded scalar integer route, and the facade's
`Image.Exif.FromImage()` integer-tag enumeration exposes integer `2` for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001CD` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `FileSource`
tag `41728` as TIFF UNDEFINED type `7`, count `1`. Native TIFF EXIF
serialization admits that tag through the bounded UNDEFINED route, and the
facade's `Image.Exif.FromImage()` undefined-tag enumeration exposes a copied
Buffer for public `Image.GetExif()` / `getexif()` readback without exposing
TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CE` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `SceneType`
tag `41729` as TIFF UNDEFINED type `7`, count `1`. Native TIFF EXIF
serialization admits that tag through the bounded UNDEFINED route, and the
facade's `Image.Exif.FromImage()` undefined-tag enumeration exposes a copied
Buffer for public `Image.GetExif()` / `getexif()` readback without exposing
TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CF` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`CustomRendered` tag `41985` as TIFF SHORT type `3`, count `1`. Native TIFF
EXIF serialization admits that tag through the bounded scalar integer route,
and the facade's `Image.Exif.FromImage()` integer-tag enumeration exposes
integer `1` for public `Image.GetExif()` / `getexif()` readback without
exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-001CG` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `ExposureMode`
tag `41986` as TIFF SHORT type `3`, count `1`. Native TIFF EXIF serialization
admits that tag through the bounded scalar integer route, and the facade's
`Image.Exif.FromImage()` integer-tag enumeration exposes integer `2` for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001CH` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `WhiteBalance`
tag `41987` as TIFF SHORT type `3`, count `1`. Native TIFF EXIF serialization
admits that tag through the bounded scalar integer route, and the facade's
`Image.Exif.FromImage()` integer-tag enumeration exposes integer `1` for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001CI` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`DigitalZoomRatio` tag `41988` as TIFF RATIONAL type `5`, count `1`. Native
TIFF EXIF serialization admits that tag through the bounded scalar RATIONAL
route, and the facade's `Image.Exif.FromImage()` rational-tag enumeration
exposes `[3, 2]` for public `Image.GetExif()` / `getexif()` readback without
exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-001CJ` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`FocalLengthIn35mmFilm` tag `41989` as TIFF SHORT type `3`, count `1`. Native
TIFF EXIF serialization admits that tag through the bounded scalar integer
route, and the facade's `Image.Exif.FromImage()` integer-tag enumeration
exposes integer `35` for public `Image.GetExif()` / `getexif()` readback
without exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts
remain `381` / `381`.
`META-001CK` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`SceneCaptureType` tag `41990` as TIFF SHORT type `3`, count `1`. Native TIFF
EXIF serialization admits that tag through the bounded scalar integer route,
and the facade's `Image.Exif.FromImage()` integer-tag enumeration exposes
integer `3` for public `Image.GetExif()` / `getexif()` readback without
exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-001CL` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `GainControl`
tag `41991` as TIFF SHORT type `3`, count `1`. Native TIFF EXIF serialization
admits that tag through the bounded scalar integer route, and the facade's
`Image.Exif.FromImage()` integer-tag enumeration exposes integer `2` for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001CM` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `Contrast` tag
`41992` as TIFF SHORT type `3`, count `1`. Native TIFF EXIF serialization
admits that tag through the bounded scalar integer route, and the facade's
`Image.Exif.FromImage()` integer-tag enumeration exposes integer `1` for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001CN` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `Saturation`
tag `41993` as TIFF SHORT type `3`, count `1`. Native TIFF EXIF serialization
admits that tag through the bounded scalar integer route, and the facade's
`Image.Exif.FromImage()` integer-tag enumeration exposes integer `2` for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001CO` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `Sharpness`
tag `41994` as TIFF SHORT type `3`, count `1`. Native TIFF EXIF serialization
admits that tag through the bounded scalar integer route, and the facade's
`Image.Exif.FromImage()` integer-tag enumeration exposes integer `2` for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001CP` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`SubjectDistanceRange` tag `41996` as TIFF SHORT type `3`, count `1`. Native
TIFF EXIF serialization admits that tag through the bounded scalar integer
route, and the facade's `Image.Exif.FromImage()` integer-tag enumeration
exposes integer `3` for public `Image.GetExif()` / `getexif()` readback
without exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts
remain `381` / `381`.
`META-001CQ` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`CameraOwnerName` tag `42032` as TIFF ASCII type `2`, count `6`. Native TIFF
EXIF serialization admits that tag through the bounded ASCII route, and the
facade's `Image.Exif.FromImage()` ASCII-tag enumeration exposes string `owner`
for public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001CR` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`BodySerialNumber` tag `42033` as TIFF ASCII type `2`, count `7`. Native TIFF
EXIF serialization admits that tag through the bounded ASCII route, and the
facade's `Image.Exif.FromImage()` ASCII-tag enumeration exposes string
`body42` for public `Image.GetExif()` / `getexif()` readback without exposing
TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-001CT` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries `LensMake`
tag `42035`, `LensModel` tag `42036`, and `LensSerialNumber` tag `42037`
as TIFF ASCII type `2`. Native TIFF EXIF serialization admits those tags
through the bounded ASCII route, and the facade's `Image.Exif.FromImage()`
ASCII-tag enumeration exposes strings `make42`, `model42`, and `lens42` for
public `Image.GetExif()` / `getexif()` readback without exposing TIFF
`Info["exif"]`. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001CS` adds no export and reuses the existing TIFF EXIF metadata blob
for bounded strip-based L-mode TIFF handles whose IFD0 carries
`LensSpecification` tag `42034` as TIFF RATIONAL type `5`, count `4`. Native
TIFF EXIF serialization admits that tag through the bounded RATIONAL-array
route, and the facade's `Image.Exif.FromImage()` rational-array enumeration
exposes `[[1,2],[3,4],[5,6],[7,8]]` for public `Image.GetExif()` /
`getexif()` readback without exposing TIFF `Info["exif"]`. Source and Release
x64 DLL export counts remain `381` / `381`.
`META-001C` adds native EXIF ASCII IFD0 tag support for the bounded
`Image.Exif` Make-tag lifecycle. `pillow_c_exif_entries_bytes` serializes an
orientation value plus zero or more ASCII tag/value entries into Pillow-style
`Exif\0\0` bytes, and `pillow_c_exif_ascii_tag` parses one ASCII tag from such
a blob.
`META-001D` is a facade routing expansion over those same exports: opened
JPEG/PNG `Info["exif"]` blobs are queried for bounded common ASCII IFD0 tags
`271`, `272`, `305`, and `306`. It adds no ABI symbol and requires no native
rebuild.
`META-001E` adds native scalar integer IFD0 tag support for bounded
`Image.Exif` integer lifecycle. `pillow_c_exif_entries_typed_bytes` serializes
mixed ASCII plus scalar integer entries, and `pillow_c_exif_uint_tag` parses
one SHORT/LONG scalar integer tag from a Pillow-style EXIF blob. Source and
Release x64 DLL export counts were `345`.
`META-001F` adds native rational IFD0 tag support for the bounded
`Image.Exif` rational lifecycle. `pillow_c_exif_entries_full_bytes` serializes
mixed ASCII, scalar integer, and TIFF RATIONAL entries, and
`pillow_c_exif_rational_tag` parses one RATIONAL scalar tag from a
Pillow-style EXIF blob. Source and Release x64 DLL export counts were `347`.
`META-001G` adds native SHORT array IFD0 tag support for bounded
`Image.Exif` tag `530` (`YCbCrSubSampling`).
`pillow_c_exif_entries_short_array_bytes` serializes mixed ASCII, scalar
integer, TIFF RATIONAL, and TIFF SHORT array entries, and
`pillow_c_exif_ushort_array_tag` parses one SHORT array tag from a
Pillow-style EXIF blob. Source and Release x64 DLL export counts were `349`.
`META-001H` adds native BYTE array IFD0 tag support for bounded `Image.Exif`
tag `40091` (`XPTitle`). `pillow_c_exif_entries_byte_array_bytes` serializes
mixed ASCII, scalar integer, TIFF RATIONAL, TIFF SHORT array, and TIFF BYTE
array entries, and `pillow_c_exif_byte_array_tag` parses one BYTE array tag
from a Pillow-style EXIF blob. Source and Release x64 DLL export counts are
now `351`.
`META-001I` adds native SRATIONAL IFD0 tag support for bounded `Image.Exif`
tag `37380` (`ExposureBiasValue`).
`pillow_c_exif_entries_signed_rational_bytes` serializes mixed ASCII, scalar
integer, TIFF RATIONAL, TIFF SHORT array, TIFF BYTE array, and TIFF SRATIONAL
entries, and `pillow_c_exif_signed_rational_tag` parses one SRATIONAL scalar
tag from a Pillow-style EXIF blob. Source and Release x64 DLL export counts
are now `353`.
`META-001J` adds native UNDEFINED IFD0 tag support for bounded `Image.Exif`
tag `36864` (`ExifVersion`).
`pillow_c_exif_entries_undefined_bytes` serializes mixed ASCII, scalar integer,
TIFF RATIONAL, TIFF SHORT array, TIFF BYTE array, TIFF SRATIONAL, and TIFF
UNDEFINED entries, and `pillow_c_exif_undefined_tag` parses one UNDEFINED byte
payload from a Pillow-style EXIF blob. Source and Release x64 DLL export counts
are now `355`.
`META-001K` adds no new export. It extends the covered facade/native contract
for the existing BYTE-array route to bounded `Image.Exif` tag `37510`
(`UserComment`) and documents that out-of-line odd-length TIFF BYTE and
UNDEFINED payloads are padded to an even byte boundary in the serialized TIFF
data area. Source and Release x64 DLL export counts remained `355` at that
slice.
`META-001L` adds `pillow_c_image_metadata_tiff_exif` for reopened TIFF handles.
The export exposes DLL-owned Pillow-style `Exif\0\0` bytes containing bounded
IFD0 common ASCII tags `271`, `272`, `305`, and `306`; identity Orientation
`274=1` is included, while transformed TIFF Orientation values `2..8` remain
hidden after native open-side pixel orientation. The facade consumes this blob
for `Image.GetExif()` / `Image.getexif()` without adding `Info["exif"]`.
Source and Release x64 DLL export counts are now `378` / `378`.
`META-001M` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 ASCII tag `270` (`ImageDescription`). The native TIFF parser now includes
tag `270`, and the facade enumerates it from the existing blob without exposing
`Info["exif"]`. Source and Release x64 DLL export counts remain `379` / `379`.
`META-001N` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 DPI resolution tags. Native TIFF open reads `XResolution` tag `282` and
`YResolution` tag `283` as TIFF RATIONAL values, plus `ResolutionUnit` tag
`296` as a SHORT/LONG scalar when the unit is inches (`2`), then serializes
them into the existing `pillow_c_image_metadata_tiff_exif` `Exif\0\0` blob.
The facade already parses those tags from that blob for `Image.GetExif()` /
`Image.getexif()` while keeping TIFF `Info["exif"]` absent. Source and Release
x64 DLL export counts remain `379` / `379`.
`META-001O` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 ASCII tags `315` (`Artist`), `316` (`HostComputer`), and `33432`
(`Copyright`). Native TIFF open reads those tags through the existing ASCII
entry parser, serializes them into `pillow_c_image_metadata_tiff_exif`, and the
facade enumerates them through the existing `Image.Exif.FromImage()` parser
without exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts
remain `379` / `379`.
`META-001P` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 ASCII tags `269` (`DocumentName`) and `285` (`PageName`). Native TIFF open
reads those tags through the existing ASCII entry parser, serializes them into
`pillow_c_image_metadata_tiff_exif`, and the facade enumerates them through the
existing `Image.Exif.FromImage()` parser without exposing TIFF `Info["exif"]`.
Source and Release x64 DLL export counts remain `379` / `379`.
`META-001Q` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 scalar integer tag `531` (`YCbCrPositioning`). Native TIFF open reads that
tag through the bounded scalar integer parser, serializes it into
`pillow_c_image_metadata_tiff_exif`, and the facade enumerates it through the
existing `Image.Exif.FromImage()` parser without exposing TIFF `Info["exif"]`.
Source and Release x64 DLL export counts remain `379` / `379`.
`META-001R` adds no symbol and extends that same TIFF metadata blob to bounded
core IFD0 scalar integer tags `256` (`ImageWidth`) and `257` (`ImageLength`).
Native TIFF open reads those tags through the bounded scalar integer parser,
serializes them into `pillow_c_image_metadata_tiff_exif`, and the facade
enumerates them through the existing `Image.Exif.FromImage()` parser without
exposing TIFF `Info["exif"]`. Source and Release x64 DLL export counts remain
`379` / `379`.
`META-001S` adds no symbol and extends that same TIFF metadata blob to bounded
scalar IFD0 layout tags `258` (`BitsPerSample`), `259` (`Compression`), `262`
(`PhotometricInterpretation`), `273` (`StripOffsets`), `278`
(`RowsPerStrip`), `279` (`StripByteCounts`), and `284`
(`PlanarConfiguration`) when those tags are scalar SHORT/LONG entries. Native
TIFF open reads those tags through the bounded scalar integer parser,
serializes them into `pillow_c_image_metadata_tiff_exif`, and the facade
enumerates them through `Image.Exif.FromImage()` without exposing TIFF
`Info["exif"]`. Multi-value layout tags such as RGB `BitsPerSample` arrays and
`SamplesPerPixel` remain outside this ABI behavior slice. Source and Release
x64 DLL export counts remain `379` / `379`.
`META-001T` adds no symbol and extends that same TIFF metadata blob to bounded
RGB layout tag shapes. Native TIFF open serializes `BitsPerSample` tag `258`
as a SHORT array when the IFD0 entry has exactly three values, keeps the
previous scalar `258` behavior for single-sample TIFFs, and serializes scalar
`SamplesPerPixel` tag `277` through the bounded scalar integer parser. The
facade materializes multi-value `258` as an array and enumerates `277` through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. RGBA/LA/CMYK
layout arrays, `ExtraSamples` tag `338`, and broader array-tag lifecycle remain
outside this ABI behavior slice. Source and Release x64 DLL export counts
remain `379` / `379`.
`META-001U` adds no symbol and extends that same TIFF metadata blob to bounded
RGBA layout tag shapes. Native TIFF open serializes `BitsPerSample` tag `258`
as a SHORT array when the IFD0 entry has exactly four values, keeps the
previous scalar and RGB-array `258` behavior, and serializes scalar
`ExtraSamples` tag `338` through the bounded scalar integer parser. The facade
enumerates `338` through `Image.Exif.FromImage()` without exposing TIFF
`Info["exif"]`; the existing multi-value `258` parser covers the RGBA array
shape. LA/CMYK, multi-extra-sample shapes, and broader array-tag lifecycle
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `379` / `379`.
`META-001V` adds no symbol and extends that same TIFF metadata blob to bounded
LA layout tag shapes. Native TIFF open serializes `BitsPerSample` tag `258` as
a SHORT array when the IFD0 entry has exactly two values, while keeping the
previous scalar, RGB-array, and RGBA-array `258` behavior. The existing scalar
integer route carries `SamplesPerPixel` tag `277` and `ExtraSamples` tag `338`,
and the facade materializes the two-value `258` array through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. CMYK,
multi-extra-sample shapes, and broader array-tag lifecycle remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `379` /
`379`.
`META-001W` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `YCbCrSubSampling` tag `530` readback. Native TIFF open serializes tag
`530` as a SHORT array only when the IFD0 entry has exactly two values. The
facade already enumerates tag `530` through `Image.Exif.FromImage()` without
exposing TIFF `Info["exif"]`. Broader TIFF array-valued tags and full TIFF tag
lifecycle remain outside this ABI behavior slice. Source and Release x64 DLL
export counts remain `379` / `379`.
`META-001X` adds no symbol and extends that same TIFF metadata blob to bounded
numeric TIFF `SampleFormat` tag `339` readback. Native numeric TIFF open now
populates `pillow_c_image_metadata_tiff_exif` before its early return and
serializes scalar tag `339` as `2` for mode `I` and `3` for mode `F` through
the existing scalar integer route. The facade enumerates tag `339` through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader numeric
TIFF tag lifecycle, numeric orientation transforms, and arbitrary SampleFormat
shapes remain outside this ABI behavior slice. Source and Release x64 DLL
export counts remain `379` / `379`.
`META-001Y` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 rational tags `286` (`XPosition`) and `287` (`YPosition`) readback.
Native TIFF open parses those tags as TIFF RATIONAL type `5`, count `1`, and
serializes them into `pillow_c_image_metadata_tiff_exif`; the facade enumerates
them through `Image.Exif.FromImage()` as `[numerator, denominator]` arrays
without exposing TIFF `Info["exif"]`. Broader rational tag lifecycle, TIFF
writeback of caller EXIF objects, arbitrary rational tags, and nested IFDs
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `379` / `379`.
`META-001Z` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 scalar tag `254` (`NewSubfileType`) readback. Native TIFF open serializes
tag `254=0` through the existing scalar integer route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Nonzero
subfile-class open semantics, multipage NewSubfileType policy, broader TIFF
tag lifecycle, and arbitrary scalar tags remain outside this ABI behavior
slice. Source and Release x64 DLL export counts remain `379` / `379`.
`META-001AA` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 scalar tag `266` (`FillOrder`) readback. Native TIFF open
serializes tag `266=1` through the existing scalar integer route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Reverse
bit-order pixel semantics for `FillOrder=2`, broader TIFF tag lifecycle, and
arbitrary scalar tags remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `379` / `379`.
`META-001AB` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 scalar tag `263` (`Thresholding`) readback. Native TIFF open
serializes tag `263=1` through the existing scalar integer route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
threshold interpretation, TIFF tag lifecycle, and arbitrary scalar tags remain
outside this ABI behavior slice. Source and Release x64 DLL export counts
remain `379` / `379`.
`META-001AC` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 scalar tags `264` (`CellWidth`) and `265` (`CellLength`)
readback. Native TIFF open serializes tags `264=5` and `265=7` through the
existing scalar integer route into `pillow_c_image_metadata_tiff_exif`; the
facade enumerates them through `Image.Exif.FromImage()` without exposing TIFF
`Info["exif"]`. Broader cell-size interpretation, TIFF tag lifecycle, and
arbitrary scalar tags remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `379` / `379`.
`META-001AD` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 scalar tag `255` (`SubfileType`) readback. Native TIFF open
serializes tag `255=1` through the existing scalar integer route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
subfile interpretation, TIFF tag lifecycle, and arbitrary scalar tags remain
outside this ABI behavior slice. Source and Release x64 DLL export counts
remain `379` / `379`.
`META-001AE` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 scalar tags `280` (`MinSampleValue`) and `281`
(`MaxSampleValue`) readback. Native TIFF open serializes tags `280=0` and
`281=255` through the existing scalar integer route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates them through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
sample-value interpretation, TIFF tag lifecycle, and arbitrary scalar tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `379` / `379`.
`META-001AF` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 SHORT-array tag `297` (`PageNumber`) readback. Native TIFF open
serializes tag `297=(3,7)` through the existing two-value SHORT-array route
into `pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
page-number interpretation, TIFF tag lifecycle, and arbitrary array-valued
tags remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `379` / `379`.
`META-001AG` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 scalar tag `317` (`Predictor`) readback. Native TIFF open
serializes tag `317=1` through the existing scalar integer route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Predictor
decompression semantics for non-identity values, broader TIFF tag lifecycle,
and arbitrary scalar tags remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `379` / `379`.
`META-001AH` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 ASCII tag `337` (`TargetPrinter`) readback. Native TIFF open
serializes tag `337="Printer Alpha"` through the existing ASCII route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
target-printer interpretation, TIFF tag lifecycle, and arbitrary ASCII tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `379` / `379`.
`META-001AI` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 SHORT-array tag `321` (`HalftoneHints`) readback. Native TIFF
open serializes tag `321=(2,3)` through the existing two-value SHORT-array
route into `pillow_c_image_metadata_tiff_exif`; the facade enumerates it
through `Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`.
Broader halftone-hints interpretation, TIFF tag lifecycle, and arbitrary
array-valued tags remain outside this ABI behavior slice. Source and Release
x64 DLL export counts remain `379` / `379`.
`META-001AJ` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 scalar tag `332` (`InkSet`) readback. Native TIFF open serializes
tag `332=1` through the existing scalar integer route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader ink-set
interpretation, TIFF tag lifecycle, and arbitrary scalar tags remain outside
this ABI behavior slice. Source and Release x64 DLL export counts remain
`379` / `379`.
`META-001AK` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 scalar tag `334` (`NumberOfInks`) readback. Native TIFF open
serializes tag `334=4` through the existing scalar integer route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
number-of-inks interpretation, TIFF tag lifecycle, and arbitrary scalar tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `379` / `379`.
`META-001AL` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 SHORT-array tag `336` (`DotRange`) readback. Native TIFF open
serializes tag `336=(0,255)` through the existing two-value SHORT-array route
into `pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
dot-range interpretation, TIFF tag lifecycle, and arbitrary array-valued tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `379` / `379`.
`META-001AM` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 scalar tag `290` (`GrayResponseUnit`) readback. Native TIFF open
serializes tag `290=2` through the existing scalar integer route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
gray-response interpretation, TIFF tag lifecycle, and arbitrary scalar tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `379` / `379`.
`META-001AN` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 ASCII tag `333` (`InkNames`) readback. Native TIFF open serializes
tag `333="Cyan"` through the existing ASCII route into
`pillow_c_image_metadata_tiff_exif`; the facade enumerates it through
`Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
ink-name lists, TIFF tag lifecycle, and arbitrary ASCII tags remain outside
this ABI behavior slice. Source and Release x64 DLL export counts remain
`379` / `379`.
`META-001AO` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 SHORT-array tag `301` (`TransferFunction`) readback. Native TIFF
open serializes tag `301=(0,128,255)` through the existing SHORT-array route
only when the IFD0 entry has exactly three values; the facade enumerates it
through `Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
transfer-function curves, TIFF tag lifecycle, and arbitrary array-valued tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `379` / `379`.
`META-001AP` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 SHORT-array tag `291` (`GrayResponseCurve`) readback. Native TIFF
open serializes tag `291=(0,128,255)` through the existing SHORT-array route
only when the IFD0 entry has exactly three values; the facade enumerates it
through `Image.Exif.FromImage()` without exposing TIFF `Info["exif"]`. Broader
gray-response curves, TIFF tag lifecycle, and arbitrary array-valued tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `379` / `379`.
`META-001AQ` adds `pillow_c_exif_rational_array_tag` and extends that same
TIFF metadata blob to bounded IFD0 RATIONAL-array tag `318` (`WhitePoint`)
readback. Native TIFF open serializes tag `318=(1/2,3/4)` as TIFF RATIONAL
type `5`, count `2`, only when the IFD0 entry has exactly two nonzero
denominator values; the facade enumerates it through `Image.Exif.FromImage()`
as `[[1, 2], [3, 4]]` without exposing TIFF `Info["exif"]`. Broader
rational-array tags, TIFF tag lifecycle, caller EXIF writeback, and arbitrary
RATIONAL arrays remain outside this ABI behavior slice. Source and Release x64
DLL export counts are now `380` / `380`.
`META-001AR` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 RATIONAL-array tag `319` (`PrimaryChromaticities`) readback. Native TIFF
open serializes tag `319=(1/2,3/4,5/6,7/8,9/10,11/12)` as TIFF RATIONAL type
`5`, count `6`, only when the IFD0 entry has exactly six nonzero denominator
values; the facade enumerates it through `Image.Exif.FromImage()` as `[[1, 2],
[3, 4], [5, 6], [7, 8], [9, 10], [11, 12]]` without exposing TIFF
`Info["exif"]`. Broader chromaticity interpretation, TIFF tag lifecycle, caller
EXIF writeback, and arbitrary RATIONAL arrays remain outside this ABI behavior
slice. Source and Release x64 DLL export counts remain `380` / `380`.
`META-001AS` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 SHORT-array tag `320` (`ColorMap`) readback. Native TIFF open serializes
tag `320` as TIFF SHORT type `3`, count `768`, only when the IFD0 entry has
exactly 768 values; the facade enumerates it through
`Image.Exif.FromImage()` as a 768-value integer array without exposing TIFF
`Info["exif"]`. Broader ColorMap validation, high-bit palette TIFFs,
non-frame-0 ColorMap metadata, TIFF tag lifecycle, caller EXIF writeback, and
arbitrary SHORT arrays remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `380` / `380`.
`META-001AT` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 RATIONAL-array tag `529` (`YCbCrCoefficients`) readback. Native
TIFF open serializes tag `529=(1/2,3/4,5/6)` as TIFF RATIONAL type `5`, count
`3`, only when the IFD0 entry has exactly three nonzero denominator values;
the facade enumerates it through `Image.Exif.FromImage()` as `[[1, 2], [3,
4], [5, 6]]` without exposing TIFF `Info["exif"]`. Broader YCbCr color
interpretation, TIFF tag lifecycle, caller EXIF writeback, and arbitrary
RATIONAL arrays remain outside this ABI behavior slice. Source and Release x64
DLL export counts remain `380` / `380`.
`META-001AU` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 RATIONAL-array tag `532` (`ReferenceBlackWhite`) readback. Native
TIFF open serializes tag `532=(0/1,255/1,1/2,3/4,5/6,7/8)` as TIFF RATIONAL
type `5`, count `6`, only when the IFD0 entry has exactly six nonzero
denominator values; the facade enumerates it through `Image.Exif.FromImage()`
as `[[0, 1], [255, 1], [1, 2], [3, 4], [5, 6], [7, 8]]` without exposing TIFF
`Info["exif"]`. Broader reference black/white color interpretation, TIFF tag
lifecycle, caller EXIF writeback, and arbitrary RATIONAL arrays remain outside
this ABI behavior slice. Source and Release x64 DLL export counts remain
`380` / `380`.
`META-001AV` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 SHORT-array tag `342` (`TransferRange`) readback. Native TIFF open
serializes tag `342=(0,255,1,254,2,253)` as TIFF SHORT type `3`, count `6`,
only when the IFD0 entry has exactly six values; the facade enumerates it
through `Image.Exif.FromImage()` as `[0, 255, 1, 254, 2, 253]` without
exposing TIFF `Info["exif"]`. Broader transfer-range interpretation, TIFF tag
lifecycle, caller EXIF writeback, and arbitrary SHORT arrays remain outside
this ABI behavior slice. Source and Release x64 DLL export counts remain
`380` / `380`.
`META-001AW` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 scalar tags `340` (`SMinSampleValue`) and `341`
(`SMaxSampleValue`) readback. Native TIFF open serializes `340=0` and
`341=255` as TIFF SHORT type `3`, count `1`, through the existing scalar
integer route; the facade enumerates both through `Image.Exif.FromImage()` as
scalar integers without exposing TIFF `Info["exif"]`. Broader signed sample
range interpretation, non-SHORT sample-value shapes, TIFF tag lifecycle,
caller EXIF writeback, and arbitrary scalar tags remain outside this ABI
behavior slice. Source and Release x64 DLL export counts remain `380` /
`380`.
`META-001AX` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 fax scalar tags `326` (`BadFaxLines`), `327` (`CleanFaxData`),
and `328` (`ConsecutiveBadFaxLines`) readback. Native TIFF open serializes
those values through the existing scalar integer route; the facade enumerates
them through `Image.Exif.FromImage()` as scalar integers without exposing TIFF
`Info["exif"]`. Broader fax decoding/semantics, non-scalar fax tag shapes,
TIFF tag lifecycle, caller EXIF writeback, and arbitrary scalar tags remain
outside this ABI behavior slice. Source and Release x64 DLL export counts
remain `381` / `381`.
`META-001AY` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 fax option scalar tags `292` (`Group3Options`) and `293`
(`Group4Options`) readback. Native TIFF open serializes those values through
the existing scalar integer route; the facade enumerates them through
`Image.Exif.FromImage()` as scalar integers without exposing TIFF
`Info["exif"]`. Broader Group3/Group4 fax decoding/semantics, non-scalar
option tag shapes, compression behavior, TIFF tag lifecycle, caller EXIF
writeback, and arbitrary scalar tags remain outside this ABI behavior slice.
Source and Release x64 DLL export counts remain `381` / `381`.
`META-001AZ` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 free block scalar tags `288` (`FreeOffsets`) and `289`
(`FreeByteCounts`) readback. Native TIFF open serializes those values through
the existing scalar integer route; the facade enumerates them through
`Image.Exif.FromImage()` as scalar integers without exposing TIFF
`Info["exif"]`. Broader free block lifecycle semantics, non-scalar free block
tag shapes, free-list validation, TIFF tag lifecycle, caller EXIF writeback,
and arbitrary scalar tags remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `381` / `381`.
`META-001BA` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 tile shape scalar tags `322` (`TileWidth`) and `323`
(`TileLength`) readback for uncompressed single-strip L TIFF files whose pixel
storage remains strip-based. Native TIFF open recognizes that bounded shape
before WIC, preserves DLL-owned pixels, and serializes those values through the
existing scalar integer route; the facade enumerates them through
`Image.Exif.FromImage()` as scalar integers without exposing TIFF
`Info["exif"]`. Actual tiled TIFF storage/decoding semantics, `TileOffsets` /
`TileByteCounts`, non-scalar tile tag shapes, TIFF tag lifecycle, caller EXIF
writeback, and arbitrary scalar tags remain outside this ABI behavior slice.
Source and Release x64 DLL export counts remain `381` / `381`.
`META-001BB` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 tile byte range scalar tags `324` (`TileOffsets`) and `325`
(`TileByteCounts`) readback for strip-based L TIFF files. Native TIFF EXIF
serialization writes those values through the existing scalar integer route;
the facade enumerates them through `Image.Exif.FromImage()` as scalar integers
without exposing TIFF `Info["exif"]`. Actual tiled TIFF storage/decoding
semantics, non-scalar tile byte-range tag shapes, TIFF tag lifecycle, caller
EXIF writeback, and arbitrary scalar tags remain outside this ABI behavior
slice. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001BC` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `JPEGTables` tag `347` readback for strip-based L TIFF files
when the tag is stored as TIFF UNDEFINED type `7`. Native TIFF EXIF
serialization writes those bytes through the bounded UNDEFINED payload route;
the facade enumerates them through `Image.Exif.FromImage()` as a copied Buffer
without exposing TIFF `Info["exif"]`. JPEGTables-driven JPEG-in-TIFF decoding,
malformed table validation, actual TIFF JPEG compression, TIFF tag lifecycle,
caller EXIF writeback, and arbitrary UNDEFINED tags remain outside this ABI
behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BD` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `JPEGTables` tag `347` readback for strip-based L TIFF files
when the tag is stored as TIFF BYTE type `1`. Native TIFF EXIF serialization
writes those bytes through the bounded BYTE-or-UNDEFINED payload route; the
facade enumerates them through the existing `Image.Exif.FromImage()`
undefined-tag list as a copied Buffer without exposing TIFF `Info["exif"]`.
JPEGTables-driven JPEG-in-TIFF decoding, malformed table validation, actual
TIFF JPEG compression, TIFF tag lifecycle, caller EXIF writeback, and
arbitrary BYTE/UNDEFINED tags remain outside this ABI behavior slice. Source
and Release x64 DLL export counts remain `381` / `381`.
`META-001BE` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `ExifVersion` tag `36864` readback for strip-based L TIFF files
when the tag is stored as TIFF UNDEFINED type `7`. Native TIFF EXIF
serialization writes those bytes through the bounded UNDEFINED payload route;
the facade enumerates them through the existing `Image.Exif.FromImage()`
undefined-tag list as a copied Buffer without exposing TIFF `Info["exif"]`.
EXIF object serialization/save lifecycle remains covered by `META-001J`, and
arbitrary UNDEFINED tags remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `381` / `381`.
`META-001BF` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `DateTimeOriginal` tag `36867` readback for strip-based L TIFF
files when the tag is stored as TIFF ASCII type `2`. Native TIFF EXIF
serialization writes the string through the bounded ASCII payload route; the
facade enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag
list without exposing TIFF `Info["exif"]`. Nested EXIF IFD
DateTimeOriginal handling and arbitrary ASCII tags remain outside this ABI
behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BG` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `DateTimeDigitized` tag `36868` readback for strip-based L TIFF
files when the tag is stored as TIFF ASCII type `2`. Native TIFF EXIF
serialization writes the string through the bounded ASCII payload route; the
facade enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag
list without exposing TIFF `Info["exif"]`. Nested EXIF IFD
DateTimeDigitized handling and arbitrary ASCII tags remain outside this ABI
behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BH` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `SubSecTime` tag `37520` readback for strip-based L TIFF files
when the tag is stored as TIFF ASCII type `2`. Native TIFF EXIF serialization
writes the string through the bounded ASCII payload route; the facade
enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag list
without exposing TIFF `Info["exif"]`. Nested EXIF IFD SubSecTime handling,
SubSecTimeOriginal/SubSecTimeDigitized siblings, and arbitrary ASCII tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `381` / `381`.
`META-001BI` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `SubSecTimeOriginal` tag `37521` readback for strip-based L TIFF
files when the tag is stored as TIFF ASCII type `2`. Native TIFF EXIF
serialization writes the string through the bounded ASCII payload route; the
facade enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag
list without exposing TIFF `Info["exif"]`. Nested EXIF IFD
SubSecTimeOriginal handling, SubSecTime/SubSecTimeDigitized sibling lifecycle,
and arbitrary ASCII tags remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `381` / `381`.
`META-001BJ` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `SubSecTimeDigitized` tag `37522` readback for strip-based L TIFF
files when the tag is stored as TIFF ASCII type `2`. Native TIFF EXIF
serialization writes the string through the bounded ASCII payload route; the
facade enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag
list without exposing TIFF `Info["exif"]`. Nested EXIF IFD
SubSecTimeDigitized handling, SubSecTime/SubSecTimeOriginal sibling lifecycle,
and arbitrary ASCII tags remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `381` / `381`.
`META-001BK` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `OffsetTime` tag `36880` readback for strip-based L TIFF files
when the tag is stored as TIFF ASCII type `2`. Native TIFF EXIF serialization
writes the string through the bounded ASCII payload route; the facade
enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag list
without exposing TIFF `Info["exif"]`. Nested EXIF IFD OffsetTime handling,
OffsetTimeOriginal/OffsetTimeDigitized sibling lifecycle, and arbitrary ASCII
tags remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `381` / `381`.
`META-001BL` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `OffsetTimeOriginal` tag `36881` readback for strip-based L TIFF
files when the tag is stored as TIFF ASCII type `2`. Native TIFF EXIF
serialization writes the string through the bounded ASCII payload route; the
facade enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag
list without exposing TIFF `Info["exif"]`. Nested EXIF IFD
OffsetTimeOriginal handling, OffsetTime/OffsetTimeDigitized sibling lifecycle,
and arbitrary ASCII tags remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `381` / `381`.
`META-001BM` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `OffsetTimeDigitized` tag `36882` readback for strip-based L TIFF
files when the tag is stored as TIFF ASCII type `2`. Native TIFF EXIF
serialization writes the string through the bounded ASCII payload route; the
facade enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag
list without exposing TIFF `Info["exif"]`. Nested EXIF IFD
OffsetTimeDigitized handling, OffsetTime/OffsetTimeOriginal sibling lifecycle,
and arbitrary ASCII tags remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `381` / `381`.
`META-001BN` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `ExifIFD` pointer tag `34665` readback for strip-based L TIFF
files when the tag is stored as TIFF LONG type `4`. Native TIFF EXIF
serialization writes the offset value through the bounded scalar integer
route; the facade enumerates it through the existing
`Image.Exif.FromImage()` integer-tag list without exposing TIFF
`Info["exif"]`. Nested EXIF IFD traversal and sub-IFD tag materialization
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `381` / `381`.
`META-001BO` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `GPSInfo` pointer tag `34853` readback for strip-based L TIFF
files when the tag is stored as TIFF LONG type `4`. Native TIFF EXIF
serialization writes the offset value through the bounded scalar integer
route; the facade enumerates it through the existing
`Image.Exif.FromImage()` integer-tag list without exposing TIFF
`Info["exif"]`. Nested GPS IFD traversal and sub-IFD tag materialization
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `381` / `381`.
`META-001BP` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `ImageUniqueID` tag `42016` readback for strip-based L TIFF
files when the tag is stored as TIFF ASCII type `2`. Native TIFF EXIF
serialization writes the string through the bounded ASCII payload route; the
facade enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag
list without exposing TIFF `Info["exif"]`. Arbitrary ASCII tags remain outside
this ABI behavior slice. Source and Release x64 DLL export counts remain
`381` / `381`.
`META-001BQ` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `XPComment` tag `40092` readback for strip-based L TIFF files
when the tag is stored as TIFF BYTE type `1`. Native TIFF EXIF serialization
writes the copied bytes through the bounded BYTE-array route; the facade
enumerates them through the existing `Image.Exif.FromImage()` byte-array list
without exposing TIFF `Info["exif"]`. Other XP* tags and arbitrary BYTE tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `381` / `381`.
`META-001BR` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `XPAuthor` tag `40093` readback for strip-based L TIFF files
when the tag is stored as TIFF BYTE type `1`. Native TIFF EXIF serialization
writes the copied bytes through the bounded BYTE-array route; the facade
enumerates them through the existing `Image.Exif.FromImage()` byte-array list
without exposing TIFF `Info["exif"]`. Other XP* tags and arbitrary BYTE tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `381` / `381`.
`META-001BS` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `XPKeywords` tag `40094` readback for strip-based L TIFF files
when the tag is stored as TIFF BYTE type `1`. Native TIFF EXIF serialization
writes the copied bytes through the bounded BYTE-array route; the facade
enumerates them through the existing `Image.Exif.FromImage()` byte-array list
without exposing TIFF `Info["exif"]`. Other XP* tags and arbitrary BYTE tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `381` / `381`.
`META-001BT` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `XPSubject` tag `40095` readback for strip-based L TIFF files
when the tag is stored as TIFF BYTE type `1`. Native TIFF EXIF serialization
writes the copied bytes through the bounded BYTE-array route; the facade
enumerates them through the existing `Image.Exif.FromImage()` byte-array list
without exposing TIFF `Info["exif"]`. Other XP* tags and arbitrary BYTE tags
remain outside this ABI behavior slice. Source and Release x64 DLL export
counts remain `381` / `381`.
`META-001BU` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `FlashPixVersion` tag `40960` readback for strip-based L TIFF
files when the tag is stored as TIFF UNDEFINED type `7`. Native TIFF EXIF
serialization writes the copied bytes through the bounded UNDEFINED route; the
facade enumerates them through the existing `Image.Exif.FromImage()`
undefined-tag list without exposing TIFF `Info["exif"]`. EXIF object
serialize/save lifecycle for tag `40960`, arbitrary TIFF UNDEFINED tags, and
nested IFDs remain outside this ABI behavior slice. Source and Release x64 DLL
export counts remain `381` / `381`.
`META-001BV` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `ColorSpace` tag `40961` readback for strip-based L TIFF files
when the tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF
serialization writes the scalar through the bounded integer route; the facade
enumerates it through the existing `Image.Exif.FromImage()` scalar-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `40961`, arbitrary TIFF scalar tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001BW` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `PixelXDimension` tag `40962` readback for strip-based L TIFF
files when the tag is stored as TIFF LONG type `4`, count `1`. Native TIFF
EXIF serialization writes the scalar through the bounded integer route; the
facade enumerates it through the existing `Image.Exif.FromImage()` scalar-tag
list without exposing TIFF `Info["exif"]`. EXIF object serialize/save
lifecycle for tag `40962`, arbitrary TIFF scalar tags, and nested IFDs remain
outside this ABI behavior slice. Source and Release x64 DLL export counts
remain `381` / `381`.
`META-001BX` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `PixelYDimension` tag `40963` readback for strip-based L TIFF
files when the tag is stored as TIFF LONG type `4`, count `1`. Native TIFF
EXIF serialization writes the scalar through the bounded integer route; the
facade enumerates it through the existing `Image.Exif.FromImage()` scalar-tag
list without exposing TIFF `Info["exif"]`. EXIF object serialize/save
lifecycle for tag `40963`, arbitrary TIFF scalar tags, and nested IFDs remain
outside this ABI behavior slice. Source and Release x64 DLL export counts
remain `381` / `381`.
`META-001BY` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `RelatedSoundFile` tag `40964` readback for strip-based L TIFF
files when the tag is stored as TIFF ASCII type `2`, count `13`. Native TIFF
EXIF serialization writes the string through the bounded ASCII route; the
facade enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag
list without exposing TIFF `Info["exif"]`. EXIF object serialize/save
lifecycle for tag `40964`, arbitrary TIFF ASCII tags, and nested IFDs remain
outside this ABI behavior slice. Source and Release x64 DLL export counts
remain `381` / `381`.
`META-001BZ` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `FlashEnergy` tag `41483` readback for strip-based L TIFF files
when the tag is stored as TIFF RATIONAL type `5`, count `1`. Native TIFF EXIF
serialization writes the rational through the bounded scalar RATIONAL route;
the facade enumerates it through the existing `Image.Exif.FromImage()`
rational-tag list without exposing TIFF `Info["exif"]`. EXIF object
serialize/save lifecycle for tag `41483`, arbitrary TIFF rational tags, and
nested IFDs remain outside this ABI behavior slice. Source and Release x64 DLL
export counts remain `381` / `381`.
`META-001CA` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `FocalPlaneXResolution` tag `41486` and
`FocalPlaneYResolution` tag `41487` readback for strip-based L TIFF files when
the tags are stored as TIFF RATIONAL type `5`, count `1`. Native TIFF EXIF
serialization writes the rationals through the bounded scalar RATIONAL route;
the facade enumerates them through the existing `Image.Exif.FromImage()`
rational-tag list without exposing TIFF `Info["exif"]`. EXIF object
serialize/save lifecycle for tags `41486` and `41487`, arbitrary TIFF rational
tags, and nested IFDs remain outside this ABI behavior slice. Source and
Release x64 DLL export counts remain `381` / `381`.
`META-001CB` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `FocalPlaneResolutionUnit` tag `41488` readback for strip-based L
TIFF files when the tag is stored as TIFF SHORT type `3`, count `1`. Native
TIFF EXIF serialization writes the scalar through the bounded integer route;
the facade enumerates it through the existing `Image.Exif.FromImage()`
integer-tag list without exposing TIFF `Info["exif"]`. EXIF object
serialize/save lifecycle for tag `41488`, arbitrary TIFF scalar tags, and
nested IFDs remain outside this ABI behavior slice. Source and Release x64 DLL
export counts remain `381` / `381`.
`META-001CC` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `SensingMethod` tag `41495` readback for strip-based L TIFF files
when the tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF
serialization writes the scalar through the bounded integer route; the facade
enumerates it through the existing `Image.Exif.FromImage()` integer-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `41495`, arbitrary TIFF scalar tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CD` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `FileSource` tag `41728` readback for strip-based L TIFF files
when the tag is stored as TIFF UNDEFINED type `7`, count `1`. Native TIFF EXIF
serialization writes the payload through the bounded UNDEFINED route; the
facade enumerates it through the existing `Image.Exif.FromImage()`
undefined-tag list without exposing TIFF `Info["exif"]`. EXIF object
serialize/save lifecycle for tag `41728`, arbitrary TIFF UNDEFINED tags, and
nested IFDs remain outside this ABI behavior slice. Source and Release x64 DLL
export counts remain `381` / `381`.
`META-001CE` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `SceneType` tag `41729` readback for strip-based L TIFF files
when the tag is stored as TIFF UNDEFINED type `7`, count `1`. Native TIFF EXIF
serialization writes the payload through the bounded UNDEFINED route; the
facade enumerates it through the existing `Image.Exif.FromImage()`
undefined-tag list without exposing TIFF `Info["exif"]`. EXIF object
serialize/save lifecycle for tag `41729`, arbitrary TIFF UNDEFINED tags, and
nested IFDs remain outside this ABI behavior slice. Source and Release x64 DLL
export counts remain `381` / `381`.
`META-001CF` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `CustomRendered` tag `41985` readback for strip-based L TIFF
files when the tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF
EXIF serialization writes the scalar through the bounded integer route; the
facade enumerates it through the existing `Image.Exif.FromImage()`
integer-tag list without exposing TIFF `Info["exif"]`. EXIF object
serialize/save lifecycle for tag `41985`, arbitrary TIFF scalar tags, and
nested IFDs remain outside this ABI behavior slice. Source and Release x64 DLL
export counts remain `381` / `381`.
`META-001CG` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `ExposureMode` tag `41986` readback for strip-based L TIFF files
when the tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF
serialization writes the scalar through the bounded integer route; the facade
enumerates it through the existing `Image.Exif.FromImage()` integer-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `41986`, arbitrary TIFF scalar tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CH` adds no symbol and extends that same TIFF metadata blob to
bounded IFD0 `WhiteBalance` tag `41987` readback for strip-based L TIFF files
when the tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF
serialization writes the scalar through the bounded integer route; the facade
enumerates it through the existing `Image.Exif.FromImage()` integer-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `41987`, arbitrary TIFF scalar tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CI` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `DigitalZoomRatio` tag `41988` readback for strip-based L TIFF files when
the tag is stored as TIFF RATIONAL type `5`, count `1`. Native TIFF EXIF
serialization writes the rational through the bounded scalar RATIONAL route;
the facade enumerates it through the existing `Image.Exif.FromImage()`
rational-tag list without exposing TIFF `Info["exif"]`. EXIF object
serialize/save lifecycle for tag `41988`, arbitrary TIFF rational tags, and
nested IFDs remain outside this ABI behavior slice. Source and Release x64 DLL
export counts remain `381` / `381`.
`META-001CJ` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `FocalLengthIn35mmFilm` tag `41989` readback for strip-based L TIFF files
when the tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF
serialization writes the scalar through the bounded integer route; the facade
enumerates it through the existing `Image.Exif.FromImage()` integer-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `41989`, arbitrary TIFF scalar tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CK` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `SceneCaptureType` tag `41990` readback for strip-based L TIFF files when
the tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF
serialization writes the scalar through the bounded integer route; the facade
enumerates it through the existing `Image.Exif.FromImage()` integer-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `41990`, arbitrary TIFF scalar tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CL` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `GainControl` tag `41991` readback for strip-based L TIFF files when the
tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF
serialization writes the scalar through the bounded integer route; the facade
enumerates it through the existing `Image.Exif.FromImage()` integer-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `41991`, arbitrary TIFF scalar tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CM` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `Contrast` tag `41992` readback for strip-based L TIFF files when the tag
is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF serialization
writes the scalar through the bounded integer route; the facade enumerates it
through the existing `Image.Exif.FromImage()` integer-tag list without exposing
TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for tag `41992`,
arbitrary TIFF scalar tags, and nested IFDs remain outside this ABI behavior
slice. Source and Release x64 DLL export counts remain `381` / `381`.
`META-001CN` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `Saturation` tag `41993` readback for strip-based L TIFF files when the
tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF
serialization writes the scalar through the bounded integer route; the facade
enumerates it through the existing `Image.Exif.FromImage()` integer-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `41993`, arbitrary TIFF scalar tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CO` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `Sharpness` tag `41994` readback for strip-based L TIFF files when the
tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF
serialization writes the scalar through the bounded integer route; the facade
enumerates it through the existing `Image.Exif.FromImage()` integer-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `41994`, arbitrary TIFF scalar tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CP` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `SubjectDistanceRange` tag `41996` readback for strip-based L TIFF files
when the tag is stored as TIFF SHORT type `3`, count `1`. Native TIFF EXIF
serialization writes the scalar through the bounded integer route; the facade
enumerates it through the existing `Image.Exif.FromImage()` integer-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `41996`, arbitrary TIFF scalar tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CQ` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `CameraOwnerName` tag `42032` readback for strip-based L TIFF files when
the tag is stored as TIFF ASCII type `2`, count `6`. Native TIFF EXIF
serialization writes the string through the bounded ASCII route; the facade
enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `42032`, arbitrary TIFF ASCII tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CR` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `BodySerialNumber` tag `42033` readback for strip-based L TIFF files when
the tag is stored as TIFF ASCII type `2`, count `7`. Native TIFF EXIF
serialization writes the string through the bounded ASCII route; the facade
enumerates it through the existing `Image.Exif.FromImage()` ASCII-tag list
without exposing TIFF `Info["exif"]`. EXIF object serialize/save lifecycle for
tag `42033`, arbitrary TIFF ASCII tags, and nested IFDs remain outside this
ABI behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`META-001CT` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `LensMake` / `LensModel` / `LensSerialNumber` tag `42035` / `42036` /
`42037` readback for strip-based L TIFF files when the tags are stored as TIFF
ASCII type `2`. Native TIFF EXIF serialization writes the strings through the
bounded ASCII route; the facade enumerates them through the existing
`Image.Exif.FromImage()` ASCII-tag list without exposing TIFF `Info["exif"]`.
EXIF object serialize/save lifecycle for tags `42035`/`42036`/`42037`, arbitrary
TIFF ASCII tags, and nested IFDs remain outside this ABI behavior slice. Source
and Release x64 DLL export counts remain `381` / `381`.
`META-001CS` adds no symbol and extends that same TIFF metadata blob to bounded
IFD0 `LensSpecification` tag `42034` readback for strip-based L TIFF files
when the tag is stored as TIFF RATIONAL type `5`, count `4`. Native TIFF EXIF
serialization writes the four rationals through the bounded rational-array
route; the facade enumerates it through the existing
`Image.Exif.FromImage()` rational-array list without exposing TIFF
`Info["exif"]`. EXIF object serialize/save lifecycle for tag `42034`,
arbitrary TIFF rational-array tags, and nested IFDs remain outside this ABI
behavior slice. Source and Release x64 DLL export counts remain `381` /
`381`.
`FMT-TIFF-004` adds no symbol and changes only the existing TIFF LZW codec
semantics behind `pillow_c_image_open_tiff`,
`pillow_c_image_open_tiff_frame`, and
`pillow_c_image_save_tiff_compression_options`. The decoder now accepts
Pillow/libtiff early-change LZW strips, and the encoder writes the same
dictionary-full clear/reset boundary as Pillow for bounded `I;16` fixtures.
Source and Release x64 DLL export counts remain `379` / `379`.
`FMT-TIFF-005` adds no symbol and tightens the existing TIFF palette metadata
parser behind `pillow_c_image_open_tiff` / `pillow_c_image_open_tiff_frame`.
Frame-0 mode `P` open now requires an explicitly found valid ColorMap tag
`320` with type SHORT and count `768` before reading palette bytes; malformed
ColorMap shapes return `-3` instead of treating TIFF offset `0` as palette
data. Source and Release x64 DLL export counts remain `379` / `379`.
`FMT-PNG-004AJ` adds `pillow_c_image_save_png_text_entries_custom_chunks_options`
for the bounded PNG route that mixes ordinary `PngInfo.add_text(...)` entries
with multiple Pillow-style private custom chunks before and after `IDAT`.
`FMT-PNG-004AK` adds
`pillow_c_image_save_png_text_entries_custom_chunks_kind_options` for the
matching batch-private PNG route mixed with compressed `zTXt`, uncompressed
`iTXt`, compressed `iTXt`, or language-keyed `iTXt` entries.
`FMT-PNG-004AL` adds
`pillow_c_image_save_png_metadata_custom_chunks_options` for the generalized
PNG metadata route mixed with multiple Pillow-style private custom chunks. The
first facade-proven route composes ordinary text plus `icc_profile` with
pre- and post-`IDAT` private chunks.
`FMT-PNG-004AM` adds no export and reuses that generalized batch route for
ordinary text plus explicit `exif` with pre- and post-`IDAT` private chunks,
including the Pillow-compatible text, pre-private, `eXIf`, `IDAT`,
post-private ordering.
`FMT-PNG-004AN` also adds no export and reuses the same batch route for
ordinary text plus explicit `icc_profile` plus explicit `exif` with pre- and
post-`IDAT` private chunks, including the Pillow-compatible `iCCP`, text,
pre-private, `eXIf`, `IDAT`, post-private ordering.
`FMT-PNG-004AO` also adds no export and reuses the same batch route for
advanced text kinds (`zTXt`, uncompressed `iTXt`, compressed `iTXt`, and
uncompressed language-keyed `iTXt`) plus explicit `icc_profile` plus explicit
`exif` with pre- and post-`IDAT` private chunks, including the
Pillow-compatible `iCCP`, selected text, pre-private, `eXIf`, `IDAT`,
post-private ordering.
`FMT-PNG-004AP` also adds no export and reuses the same batch route for
compressed language-keyed `iTXt` plus explicit `icc_profile` plus explicit
`exif` with pre- and post-`IDAT` private chunks. Native code removed a stale
invalid-argument rejection for that text shape; the ABI signature and export
count did not change.
`FMT-PNG-004AQ` also adds no export and reuses the same batch route for the
first proven multi-private metadata/options shape with compressed
language-keyed `iTXt`, explicit `icc_profile`, explicit `exif`, RGB `tRNS`,
and optimized IDAT output. Only facade routing changed in this slice; the ABI
signature and export count did not change.
`FMT-PNG-004AR` also adds no export and reuses the existing generalized PNG
metadata route for language-keyed `iTXt` plus explicit `icc_profile` plus
explicit `exif` without custom chunks. The raw ABI was already capable through
`pillow_c_image_save_png_metadata_options`; the facade now no longer rejects
that exact shape before calling the DLL. The ABI signature and export count did
not change.
`FMT-PNG-004AS` also adds no export and reuses the existing generalized PNG
metadata route for language-keyed `iTXt` plus explicit `icc_profile`, explicit
`exif`, and RGB `tRNS` without custom chunks. The raw ABI was already capable
through `pillow_c_image_save_png_metadata_options`; the facade now no longer
rejects that exact shape before calling the DLL. The ABI signature and export
count did not change.
`FMT-PNG-004AT` also adds no export and reuses the existing generalized PNG
metadata route for no-custom advanced text kinds (`zTXt`, plain `iTXt`,
compressed `iTXt`, and language-keyed `iTXt`) plus explicit `icc_profile`,
explicit `exif`, RGB `tRNS`, and optimized IDAT output. The raw ABI was
already capable through `pillow_c_image_save_png_metadata_options`; the facade
now no longer rejects this same-route optimize/text-kind batch before calling
the DLL. The ABI signature and export count did not change.
`FMT-PNG-001AC` adds no export and reuses the existing PNG text save/open ABI
for bounded NUL-free bytes-valued `PngInfo.add_text`. Native save now accepts
Latin-1 high bytes in `tEXt`/`zTXt` values, native open converts `tEXt`/`zTXt`
Latin-1 metadata to UTF-8 for the indexed text metadata export, and the facade
copies `Buffer` values before passing them as NUL-terminated raw bytes. The ABI
signature and export count did not change; Release x64 was rebuilt after
native behavior changed.
`FMT-PNG-001AD` adds
`pillow_c_image_save_png_text_entries_value_sizes_options` for bounded
bytes-valued PNG `PngInfo.add_text` entries whose values contain embedded NUL
bytes. The export accepts the existing text-entry key/value pointer arrays plus
a parallel `std::size_t*` value-size array and a parallel `int*` compressed
array, supports only `tEXt` / `zTXt` text entries in this slice, writes exact
value bytes instead of using `strlen`, and leaves the older NUL-terminated
text-entry exports unchanged. Source and Release x64 DLL export counts are now
`379` / `379`.
`FMT-GIF-004T` adds `pillow_c_image_gif_comment`,
`pillow_c_image_save_gif_comment`, and
`pillow_c_image_save_gif_animation_comment` for bounded GIF comment metadata.
`pillow_c_image_gif_comment(path, frame_index, out_has_comment, out_comment,
out_comment_size, out_comment_required)` probes and copies the frame comment
bytes discovered by native GIF metadata parsing. Passing a null `out_comment`
with size `0` is valid only for the size probe; when a comment exists, callers
must provide a buffer at least `out_comment_required` bytes long.
`pillow_c_image_save_gif_comment(image, path, comment, comment_size)` writes a
single-frame GIF89a comment extension before image data.
`pillow_c_image_save_gif_animation_comment(images, image_count, path,
durations_ms, duration_count, loop, disposals, disposal_count, comment,
comment_size)` writes a comment extension after the NETSCAPE loop block and
before the first frame GCE/image block while preserving the existing duration,
loop, and disposal argument shapes.
`FMT-GIF-004U` adds `pillow_c_image_save_gif_comment_options` for the bounded
single-frame P-mode GIF save combination of comment bytes plus integer
transparency metadata.
`pillow_c_image_save_gif_comment_options(image, path, has_transparency,
transparency, comment, comment_size)` writes the comment extension before the
Graphic Control Extension when `has_transparency` is nonzero, preserving
reopened comment bytes and the transparent palette index. `has_transparency`
must be `0` or `1`; the covered transparency route is mode `P`.
`FMT-GIF-004V` adds
`pillow_c_image_save_gif_animation_comment_metadata_options` for the bounded
P-mode GIF animation save combination of comment bytes plus integer
transparency metadata.
`pillow_c_image_save_gif_animation_comment_metadata_options(images,
image_count, path, durations_ms, duration_count, loop, disposals,
disposal_count, include_color_table, optimize, has_transparency, transparency,
comment, comment_size)` preserves the existing animation metadata/options
argument shapes and appends comment bytes. The covered route writes the comment
extension after NETSCAPE and before the first frame GCE, writes both covered
P-mode frame GCEs with caller transparency when frame 0 actually uses the
caller transparent index, and preserves reopened comments on both frames.
Source and Release x64 DLL export counts are now `363`.
`FMT-GIF-004W` adds
`pillow_c_image_save_gif_animation_comment_background_options` for the bounded
P-mode GIF animation save combination of comment bytes plus integer
background/transparency metadata and animation options.
`pillow_c_image_save_gif_animation_comment_background_options(images,
image_count, path, durations_ms, duration_count, loop, disposals,
disposal_count, background, include_color_table, optimize, has_transparency,
transparency, comment, comment_size)` preserves the existing animation
metadata/options argument shapes and appends comment bytes. `background` is
the logical-screen background index, `include_color_table` and `optimize` use
the existing tri-state GIF option convention (`-1` unset/default, `0` false,
`1` true), and `has_transparency` must be `0` or `1`. The covered route writes
the comment extension after NETSCAPE and before the first frame GCE, writes the
logical-screen background byte, composes `include_color_table=True`,
`optimize=False`, and caller transparency inside the DLL, and preserves
reopened comments/background metadata. Source and Release x64 DLL export
counts are now `364`.
`FMT-ICO-001A` adds
`pillow_c_image_save_ico_frames_format_options(images, image_count, path,
sizes, size_count, has_sizes, bitmap_format)` for bounded ICO
`append_images` save routing. `images` is an array of `PillowCImage*` handles
whose first element is the base image and remaining elements are Pillow-style
append images. The route uses default ICO sizes when `has_sizes` is `0`, uses
explicit caller pairs when `has_sizes` is nonzero, writes an empty ICO
directory for explicit `sizes=[]`, sorts/deduplicates requested sizes, skips
sizes larger than the base image or `256x256`, uses the first provided image
with the exact requested size, and otherwise thumbnails the last provided
image. Exact lowercase `bitmap_format="bmp"` keeps the existing DIB-backed
payload route; other values write PNG-backed entries. Existing single-image
ICO exports delegate through the same helper. Source and Release x64 DLL
export counts are now `365`.
`FMT-ICO-002A` adds
`pillow_c_image_open_ico_size(path, width, height, out_image)` for Pillow's
public ICO selected-size load path. It reuses the WIC-backed ICO decoder,
requires an exact positive frame size, returns invalid argument for missing
sizes, and returns a public `RGBA` handle for the selected frame. The facade
routes opened-ICO `image.Size := [w,h]` through this export and swaps handles
without AHK pixel loops. Source and Release x64 DLL export counts are now
`366`.
`FMT-ICO-002B` adds
`pillow_c_image_ico_sizes(path, out_sizes, out_pair_count, out_required)` for
Pillow's public ICO `ico.sizes()` and `ico.getimage(...)` route. The export
uses WIC metadata to enumerate sorted unique frame sizes without decoding
pixels. The facade routes `image.ico.sizes()` through this export and routes
`image.ico.getimage([w,h])` through `pillow_c_image_open_ico_size` when the
pair exists, otherwise through the existing largest-frame
`pillow_c_image_open_ico` path to match Pillow's missing-size fallback. Source
and Release x64 DLL export counts are now `367`.
`FMT-ICO-002C` adds no export. It corrects the existing ICO open contract for
duplicate-size entries by parsing the ICO directory before WIC decode,
applying Pillow's color-depth-ascending and square-descending entry ordering,
and decoding the selected payload through a one-entry ICO wrapper. Existing
`pillow_c_image_open_ico` and `pillow_c_image_open_ico_size` signatures are
unchanged; source and Release x64 DLL export counts remain `367`.
`FMT-ICO-002D` adds
`pillow_c_image_ico_payload_format(path, width, height,
require_requested_size, out_format, out_size, out_required)` for bounded ICO
embedded payload format metadata. It applies the same Pillow-compatible ICO
entry ordering as native open, returns `"PNG"` plus a terminating NUL for
PNG-backed payloads, and returns required size `0` for payloads without a
covered public payload format such as DIB-backed entries. Source and Release
x64 DLL export counts were then `368`.
`FMT-ICO-002E` adds
`pillow_c_image_ico_payload_dib_metadata(path, width, height,
require_requested_size, out_has_dib, out_has_dpi, out_dpi_x, out_dpi_y,
out_compression)` for bounded DIB-backed ICO payload metadata. It applies the
same Pillow-compatible ICO entry ordering as native open, returns
`out_has_dib == 0` for PNG-backed payloads, and for selected DIB payloads
parses BITMAPINFOHEADER compression plus positive pixels-per-meter resolution
as Pillow-style DPI. Source and Release x64 DLL export counts are now `369`.
`FMT-ICO-002F` adds `pillow_c_image_open_cur(path, out_image)` for bounded
DIB-backed CUR open and `pillow_c_image_metadata_dib_compression(image,
out_has_compression, out_compression)` for handle-level DIB compression
metadata. Native CUR open parses a CUR directory, applies the same
Pillow-compatible selected-entry ordering as ICO open, rejects PNG-backed CUR
payloads until that surface is covered, wraps the selected DIB payload as a
single-entry ICO for WIC decode, returns a public `RGBA` handle, and stores
positive DIB DPI plus BITMAPINFOHEADER compression so the facade can expose
Pillow-compatible `Info["dpi"]` and `Info["compression"]`. Source and Release
x64 DLL export counts are now `371`.

## Modes

Current mode IDs:

```text
1 L
2 LA
3 RGB
4 RGBA
5 1
6 P
7 CMYK
8 I
9 F
10 RGBX
11 I;16
12 I;16B
13 YCbCr
14 HSV
15 LAB
16 PA
```

`pillow_c_mode_from_string`, `pillow_c_mode_name`, `pillow_c_image_create_mode`, and `pillow_c_image_mode` keep handles mode-aware. Channel count is storage layout; mode is wrapper-visible Pillow semantics. `pillow_c_image_create_mode` accepts Pillow-style empty mode-aware image shapes where width or height is zero. Shared mode-aware target shape checks use the same allow-empty size validation, so `_into` exports can validate empty matching targets for public paths that otherwise support empty output.

The legacy `pillow_c_image_create(width, height, channels, ...)` maps channel count `1`, `2`, `3`, and `4` to `L`, `LA`, `RGB`, and `RGBA`, and remains a non-empty channel-count creation API.

Mode `1` uses one unpacked byte per pixel internally for native operations and data-pointer sharing. `pillow_c_image_set_raw_bytes` and `pillow_c_image_get_raw_bytes` expose Pillow's external bit-packed row format for raw mode `1`.

Mode `P` uses one palette index byte per pixel internally. RGB palette metadata lives on the image handle and is exposed through `pillow_c_image_put_palette_rgb` and `pillow_c_image_get_palette_rgb`; optional palette alpha metadata is exposed through `pillow_c_image_put_palette_rgba`, `pillow_c_image_get_palette_rgba`, and `pillow_c_image_palette_alpha_mode`. `pillow_c_image_put_palette_rgb` and `pillow_c_image_put_palette_rgba` also mirror Pillow's `L.putpalette(...)` behavior by converting an `L` handle to mode `P` while keeping its one-byte pixel indexes. Same-mode pixel-copy, point/LUT, reorder, expand, offset, resize, transform, and rotate paths preserve that palette so later conversion still resolves indexes like Pillow.

Mode `PA` uses mode ID `16` and two direct bytes per pixel: palette index P
and per-pixel alpha A. Mode-name/create/raw/fill/getpixel exports provide exact
interleaved storage and legal empty images. The existing RGB/RGBA palette
put/get and palette-alpha-mode exports accept PA handles while preserving PA
mode and pixel bytes; RGB palettes expand to opaque RGBA on read, and RGBA
palette alpha remains distinct from each pixel's A byte. The facade's existing
PA ModeInfo supplies P/A bands, while PutPalette/GetPalette route through the
same bulk native palette ABI. No AHK per-pixel loop or new export is introduced.
`MODE-COLOR-001I` extends the existing `pillow_c_image_convert_mode` and
`pillow_c_image_convert_mode_into` behavior without adding a symbol. P -> PA
copies indexes and initializes A from RGBA palette alpha or 255 for RGB
palettes. PA -> P drops each pixel's A and preserves palette RGB/alpha bytes.
PA -> RGBA resolves RGB from the palette and writes only the PA pixel's A,
ignoring palette alpha rather than combining it. Empty P/PA sources use the
same route so P/PA targets retain palette metadata. Common PA -> RGB/L/LA
targets are covered by `MODE-COLOR-001J`; other PA targets, transparency
metadata, codecs, broader operations, frombuffer aliasing, and quantization
remain separate surfaces. Source/Release x64 DLL exports remain `388` / `388`.

`MODE-COLOR-001J` further extends those same convert-mode exports without a
new symbol. PA -> RGB resolves palette RGB and discards both alpha sources;
PA -> L applies the shared Pillow-compatible 16-bit fixed-point luma to the
palette RGB and also discards alpha; PA -> LA writes that luma plus the PA
pixel's A byte, ignoring palette alpha. Empty PA images convert to legal empty
RGB/L/LA targets. CMYK/YCbCr/HSV are covered by `MODE-COLOR-001K`; mode 1,
numeric, implicit/default palette errors, metadata, codecs, broader
operations, frombuffer aliasing, and quantization remain separate. Export
counts remain `388` / `388`.

`MODE-COLOR-001K` extends the same convert-mode exports without adding a
symbol. PA -> CMYK resolves palette RGB into inverted C/M/Y with K=0;
PA -> YCbCr uses the existing Pillow-compatible 6-bit lookup-table kernel;
PA -> HSV uses the existing Pillow-compatible float/fmod/round kernel. All
three ignore palette alpha and PA pixel A, and empty PA inputs produce legal
empty targets. Mode 1, numeric, implicit/default palette errors, metadata,
codecs, broader operations, frombuffer aliasing, and quantization remain
separate. Export counts remain `388` / `388`.

`MODE-COLOR-001L` extends the same convert-mode exports without adding a
symbol. P -> LA resolves palette RGB through the shared fixed-point luma and
writes RGBA palette alpha, or 255 for RGB palettes. P -> YCbCr ignores palette
alpha and uses the existing Pillow-compatible 6-bit lookup-table kernel.
Empty P inputs produce legal empty targets. Mode 1, numeric, implicit/default
palette errors, metadata, codecs, broader operations, frombuffer aliasing,
and quantization remain separate. Export counts remain `388` / `388`.

`MODE-COLOR-001M` extends the same `pillow_c_image_convert_mode` and
`pillow_c_image_convert_mode_into` exports without adding a symbol. Explicit-
palette P/PA -> I resolves palette RGB and writes rounded fixed-point luma to
one little-endian signed-int32 storage slot per pixel. P/PA -> F writes
`(299R + 587G + 114B) / 1000.0F` to one little-endian float32 storage slot
without first rounding through mode L. Both routes ignore RGBA palette alpha
and PA pixel A; legal empty sources produce empty numeric targets. Mode 1/
dither, implicit/default palette errors, metadata, codecs, broader operations,
frombuffer aliasing, and quantization remain separate. Export counts remain
`388` / `388`.

`MODE-COLOR-001N` extends the existing `pillow_c_image_convert_mode_dither`
and `pillow_c_image_convert_mode_dither_into` exports without adding a symbol.
Explicit-palette P/PA -> mode 1 resolves palette RGB and writes 255 exactly
when `299R + 587G + 114B >= 128000`, otherwise 0. Pillow's direct palette
converter ignores dither for this source family, so NONE, Floyd-Steinberg, and
the facade default use the same DLL-owned threshold loop; L/LA/RGB/RGBA/CMYK
sources retain the existing Floyd-Steinberg error-diffusion path. Palette alpha
and PA pixel A are ignored, and legal empty sources produce empty targets.
Implicit/default palette behavior, metadata, codecs, broader operations,
frombuffer aliasing, and quantization remain separate. Export counts remain
`388` / `388`.

`MODE-COLOR-001P` extends `pillow_c_image_convert_mode` and
`pillow_c_image_convert_mode_into` without adding a symbol. P/PA -> RGBX shares
the existing four-byte RGBA palette expansion loop: RGB comes from the palette
or black for an empty palette; P writes RGBA palette alpha or opaque 255, while
PA writes pixel A. Legal empty inputs produce empty RGBX targets. The bounded
`MODE-COLOR-001O` audit also confirms that the existing empty-palette P/PA ->
RGB/RGBA behavior already matches Pillow and requires no ABI change. Other
RGBX conversions, metadata, codecs, broader operations, frombuffer aliasing,
and quantization remain separate. Export counts remain `388` / `388`.

`MODE-COLOR-001Q` extends the same convert-mode exports without adding a
symbol. RGBX -> RGB drops X; RGBX -> RGBA copies RGB and writes alpha 255;
RGBX -> L reuses the shared fixed-point RGB luma; RGBX -> LA pairs that luma
with alpha 255. Every loop strides four source bytes and ignores X, including
legal empty images. RGBX -> RGBA uses a dedicated branch so RGB PNG
transparency metadata cannot affect its opaque alpha. CMYK/YCbCr are covered
by `MODE-COLOR-001R`, numeric targets by `MODE-COLOR-001S`, and mode-1 by
`MODE-COLOR-001T`; metadata, codecs, broader operations, frombuffer aliasing,
and quantization remain separate. Export counts remain `388` / `388`.

`MODE-COLOR-001R` extends the same convert-mode exports without adding a
symbol. RGBX -> CMYK ignores X, subtracts the first three bytes from 255, and
writes K=0. RGBX -> YCbCr ignores X and reuses the exact Pillow-compatible
6-bit lookup-table kernel. Both loops step by the four-channel source stride,
including legal empty images. Numeric targets are covered by
`MODE-COLOR-001S`; mode-1 is covered by `MODE-COLOR-001T`, while metadata,
codecs, broader operations, frombuffer aliasing, and quantization remain
separate. Export counts remain `388` / `388`.

`MODE-COLOR-001S` extends the same convert-mode exports without adding a
symbol. RGBX -> I ignores X and writes the shared rounded fixed-point RGB luma
as little-endian int32. RGBX -> F ignores X and writes
`rgb_luma_1000 / 1000.0F` as little-endian float32, preserving Pillow's
unrounded weighted luma and float32 narrowing. The loop steps by the
four-channel source stride, including legal empty images. Mode-1 is covered by
`MODE-COLOR-001T`; metadata, codecs, broader operations, frombuffer aliasing,
and quantization remain separate. Export counts remain `388` / `388`.

`MODE-COLOR-001T` extends the existing convert-mode dither exports without
adding a symbol. RGBX -> mode 1 ignores X. NONE uses the shared weighted RGB
threshold and writes white exactly when `299R + 587G + 114B >= 128000`;
Floyd-Steinberg and the facade default use the existing native error-diffusion
loop with four-channel source stride. The pinned packed output is
`[0x00,0xA0]` for NONE and `[0x50,0xA0]` for Floyd/default; legal empty images
produce empty targets. Metadata, codecs, broader operations, frombuffer
aliasing, and quantization remain separate. Export counts remain `388` / `388`.

`MODE-COLOR-001U` extends the existing convert-mode exports without adding a
symbol. RGB and RGBA now share the RGBX numeric target loop for I/F; the loop
steps by `source->channels`, and the luma helpers read only RGB so RGBA alpha is
ignored. I writes rounded fixed-point luma as little-endian int32, while F
writes `(299R + 587G + 114B) / 1000.0F` as little-endian float32 without first
rounding through L. Legal empty images produce empty targets. Grayscale and
color-space sources, metadata, codecs, broader operations, frombuffer aliasing,
and quantization remain separate. Export counts remain `388` / `388`.

Mode `CMYK` uses four direct channel bytes per pixel. The current verified CMYK foundation covers mode mapping, raw byte import/export, getdata/putdata facade packing, getpixel/putpixel, copy, `ImageChops.invert`, and non-logical `ImageChops` binary operations.

Mode `I` uses four bytes per pixel as a little-endian signed 32-bit Pillow integer storage slot. The current verified `I` surface is intentionally narrow: mode mapping, byte-size/data export, raw `I` byte import/export, Pillow-compatible unsigned 16-bit raw decode aliases, `I;16B` raw encode, high-bit-depth Netpbm grayscale open/save, bounded TIFF save/open for frame 0 including uncompressed, PackBits, TIFF LZW, and Adobe Deflate, bounded IFD0 TIFF ICC/XMP metadata fixtures, bounded selected nonzero TIFF frame open for ICC/XMP metadata fixtures, facade scalar `getpixel`/`putpixel`/`getdata`/`putdata` semantics, facade `GetBands()` / `getbands()` band-name materialization, numeric `getextrema()` through `pillow_c_image_get_extrema_numeric`, one-band numeric `histogram()` and entropy through `pillow_c_image_histogram` / `pillow_c_image_entropy`, scalar numeric `getcolors()` through `pillow_c_image_getcolors_numeric`, `convert("L")` through `pillow_c_image_convert_mode`, bounded `ImageFilter.Kernel` through `pillow_c_image_filter_kernel`, bounded rank filters through `pillow_c_image_filter_rank`, bounded `ImageFilter.ModeFilter` wrong-mode rejection through `pillow_c_image_filter_mode`, and bounded BoxBlur/GaussianBlur/UnsharpMask wrong-mode rejection through the native blur/unsharp filter exports. General `I` arithmetic, conversion beyond the covered `L` target, unsupported numeric filter algorithms, public `I;16*` behavior beyond the first-class little-endian `I;16` mode, and broader file-format participation are future ABI surfaces.

Mode `F` uses four bytes per pixel as a little-endian 32-bit float storage slot. The current verified `F` surface is intentionally narrow: mode mapping, byte-size/data export, raw `F`/`F;32F` byte import/export, bounded TIFF save/open for frame 0 including uncompressed, PackBits, TIFF LZW, and Adobe Deflate, bounded IFD0 TIFF ICC/XMP metadata fixtures, bounded selected nonzero TIFF frame open for ICC/XMP metadata fixtures, facade scalar `getpixel`/`putpixel`/`getdata`/`putdata` semantics, facade `GetBands()` / `getbands()` band-name materialization, numeric `getextrema()` through `pillow_c_image_get_extrema_numeric`, one-band numeric `histogram()` and entropy through `pillow_c_image_histogram` / `pillow_c_image_entropy`, scalar numeric `getcolors()` through `pillow_c_image_getcolors_numeric`, `convert("L")` through `pillow_c_image_convert_mode`, bounded `ImageFilter.Kernel` wrong-mode rejection through `pillow_c_image_filter_kernel`, bounded rank filters through `pillow_c_image_filter_rank`, bounded `ImageFilter.ModeFilter` wrong-mode rejection through `pillow_c_image_filter_mode`, and bounded BoxBlur/GaussianBlur/UnsharpMask wrong-mode rejection through the native blur/unsharp filter exports. General `F` arithmetic, conversion beyond the covered `L` target, unsupported numeric filter algorithms, broader NaN/Inf behavior outside the covered extrema/histogram/convert/entropy/getcolors fixtures, and broader file-format participation are future ABI surfaces.

Mode `I;16` uses two bytes per pixel as little-endian unsigned 16-bit grayscale storage. The current verified public surface is bounded to `pillow_c_mode_from_string("I;16")`, `pillow_c_mode_name(11)`, `pillow_c_image_create_mode`, direct byte import/export, `Image.FromBytes("I;16", ...)`, `Image.ToBytes()`, little-endian TIFF save/open for frame `0` with uncompressed, PackBits, TIFF LZW, and Adobe Deflate strip handling, bounded IFD0 TIFF ICC/XMP metadata fixtures, and bounded selected nonzero TIFF frame open for ICC/XMP metadata fixtures. It does not claim `I;16N`, arithmetic, filters, broad conversion, predictor variants, or non-frame-0 special parsing beyond the separately covered ICC/XMP frame fixtures.

Mode `I;16B` uses two bytes per pixel as big-endian unsigned 16-bit grayscale storage. The current verified public surface is bounded to `pillow_c_mode_from_string("I;16B")`, `pillow_c_mode_name(12)`, `pillow_c_image_create_mode`, direct byte import/export, `Image.FromBytes("I;16B", ...)`, `Image.ToBytes()`, frame-0 uncompressed big-endian TIFF save/open, and compressed TIFF save via PackBits, TIFF LZW, Adobe Deflate, or `tiff_deflate`. The uncompressed TIFF writer emits the bounded Pillow-compatible single-strip `MM` IFD shape with BitsPerSample `16`, Compression `1`, PhotometricInterpretation `1`, RowsPerStrip equal to image height, PlanarConfiguration `1`, no SamplesPerPixel tag, and no SampleFormat tag. Compressed public `I;16B` saves match Pillow 11.3.0 by byte-swapping into a temporary little-endian `I;16` TIFF, writing Compression `32773`, `5`, or `8`, and reopening as mode `I;16` with preserved numeric samples. `I;16B` DPI, multipage save, `I;16N`, arithmetic, filters, broad conversion, predictor variants, and non-frame-0 special parsing remain future ABI surfaces.

Mode `RGBX` uses four direct channel bytes per pixel with Pillow-visible band names `R`, `G`, `B`, and `X`. The current verified public `RGBX` surface includes raw byte import/export, `Image.frombuffer(..., "raw", "RGBX", stride, orientation)` alias/detach behavior, P/PA palette expansion into RGBX, RGBX-to-HSV, and RGBX-to-RGB/RGBA/L/LA/CMYK/YCbCr/I/F/mode-1 conversion while ignoring X. Quantizing targets, general operations, format save/open paths, and draw/filter coverage remain future mode-expansion work.

Mode `YCbCr` uses mode ID `13` and three direct `Y`, `Cb`, and `Cr` bytes per
pixel. `pillow_c_mode_from_string`, `pillow_c_mode_name`,
`pillow_c_image_create_mode`, and the existing raw-byte exports provide exact
construction/frombytes/tobytes storage, including empty images. Existing
`pillow_c_image_convert_mode` and `_into` routes convert `RGB <-> YCbCr` and
`RGBX -> YCbCr` inside
the DLL using Pillow 11.3.0 `ConvertYCbCr.c` semantics: coefficient lookup
tables are generated once at 6-bit scale, each coefficient contribution uses
Pillow's `value * coefficient * 64 + 0.5` integer conversion, combined values
use arithmetic shift by six, and reverse RGB channels use Pillow clipping.
The same existing exports also cover Pillow's direct `YCbCr -> L` and
`YCbCr -> LA` targets: L copies the Y byte and LA writes Y plus alpha `255`.
Empty images preserve their shape through all covered targets. No AHK per-pixel
loop or new export is introduced. File codecs, non-RGB conversion beyond L/LA,
broad operations, color management, and frombuffer aliasing remain outside this
mode slice. Source/Release x64 DLL exports remain `388` / `388`.

Mode `HSV` uses mode ID `14` and three direct `H`, `S`, and `V` bytes per
pixel. `pillow_c_mode_from_string`, `pillow_c_mode_name`,
`pillow_c_image_create_mode`, and the existing raw-byte exports provide exact
construction/frombytes/tobytes storage, including empty images. Existing
`pillow_c_image_convert_mode` and `_into` routes convert `RGB <-> HSV` inside
the DLL using Pillow 11.3.0 `Convert.c` semantics: RGB conversion keeps the
reference float intermediates and double-literal sector offsets before
narrowing, then applies `fmod` and truncation; reverse conversion uses float
sector fractions with `floor` and C-style `round` for `p`, `q`, and `t`.
RGBA and RGBX sources use the same RGB helper while ignoring their fourth byte;
CMYK sources first use the existing Pillow-compatible black-scaled RGB helper,
then the HSV helper. Mode `1`, L, and LA sources use one grayscale loop that
writes `[0,0,V]`; mode `1` maps nonzero to V=255, L/LA read the first channel,
and LA alpha is ignored. Mode `P` sources resolve each index through the native
RGB palette and reuse the RGB-to-HSV helper; RGBA palette alpha is ignored.
Empty images preserve shape. No AHK per-pixel loop or new export is introduced.
Numeric sources, PA mode, implicit/default palette edges, file codecs, broad
operations, color management, and frombuffer aliasing remain outside this mode
slice. Source/Release x64 DLL exports remain `388` / `388`.

Mode `LAB` uses mode ID `15` and three public `L`, `A`, and `B` bytes per
pixel. `pillow_c_mode_from_string`, `pillow_c_mode_name`,
`pillow_c_image_create_mode`, fill/getpixel, and the existing raw-byte exports
provide construction, public tuple storage, exact frombytes/tobytes behavior,
and legal empty images. Pillow represents a/b as signed bytes at external raw
boundaries: native raw decode and encode xor A and B with `0x80`, while the
handle stores the public channel bytes used by fill and getpixel. The facade's
default LAB `ToBytes()` routes through `pillow_c_image_get_raw_bytes`; no AHK
per-pixel loop or new export is introduced. Pillow 11.3.0 routes RGB/LAB
conversion through ImageCms/LittleCMS built-in profiles rather than core
`Convert.c`; bounded RGB/RGBA/RGBX-to-LAB and LAB-to-RGB/RGBA/RGBX are now
covered by the statically linked LittleCMS 2.17 path, while direct LAB-to-L/LA
and LAB-to-CMYK/YCbCr/HSV rejection parity is covered separately. Other LAB
targets, general ICC color management, codecs, broader pixel/data operations,
and frombuffer aliasing remain separate surfaces.
Source/Release x64 DLL exports remain `388` / `388`.

## Export Groups

Infrastructure:

- `pillow_c_abi_version`
- `pillow_c_status_message`
- `pillow_c_mode_from_string`
- `pillow_c_mode_name`

Buffer primitives:

- `pillow_c_blend_u8`
- `pillow_c_rgb_to_l`
- `pillow_c_alpha_composite_rgba`

Font lifecycle and default-font metrics:

- `pillow_c_font_load_default`
- `pillow_c_font_free`
- `pillow_c_font_getlength`
- `pillow_c_font_getbbox`
- `pillow_c_font_getbbox_anchor`
- `pillow_c_font_getmetrics`
- `pillow_c_font_getname`
- `pillow_c_font_variant`

Image lifecycle and metadata:

- `pillow_c_image_create`
- `pillow_c_image_create_mode`
- `pillow_c_image_frombuffer_raw`
- `pillow_c_image_free`
- `pillow_c_image_width`
- `pillow_c_image_height`
- `pillow_c_image_mode`
- `pillow_c_image_exif_orientation`
- `pillow_c_exif_orientation_bytes`
- `pillow_c_exif_entries_bytes`
- `pillow_c_exif_entries_typed_bytes`
- `pillow_c_exif_entries_full_bytes`
- `pillow_c_exif_entries_short_array_bytes`
- `pillow_c_exif_entries_byte_array_bytes`
- `pillow_c_exif_entries_signed_rational_bytes`
- `pillow_c_exif_entries_undefined_bytes`
- `pillow_c_exif_ascii_tag`
- `pillow_c_exif_uint_tag`
- `pillow_c_exif_rational_tag`
- `pillow_c_exif_rational_array_tag`
- `pillow_c_exif_signed_rational_array_tag`
- `pillow_c_exif_signed_rational_tag`
- `pillow_c_exif_ushort_array_tag`
- `pillow_c_exif_byte_array_tag`
- `pillow_c_exif_undefined_tag`
- `pillow_c_image_metadata_resolution`
- `pillow_c_image_metadata_png_gamma`
- `pillow_c_image_metadata_png_srgb`
- `pillow_c_image_metadata_png_chromaticity`
- `pillow_c_image_metadata_png_text_count`
- `pillow_c_image_metadata_png_text`
- `pillow_c_image_metadata_png_icc_profile`
- `pillow_c_image_metadata_png_exif`
- `pillow_c_image_metadata_tiff_exif`
- `pillow_c_image_metadata_tiff_icc_profile`
- `pillow_c_image_metadata_xmp`
- `pillow_c_image_metadata_jpeg_comment`
- `pillow_c_image_metadata_jpeg_icc_profile`
- `pillow_c_image_metadata_jpeg_exif`
- `pillow_c_image_metadata_jpeg_qtable_count`
- `pillow_c_image_metadata_jpeg_qtable`
- `pillow_c_image_metadata_jpeg_subsampling`
- `pillow_c_image_metadata_png_transparency`
- `pillow_c_image_metadata_png_transparency_table`
- `pillow_c_image_metadata_png_rgb_transparency`
- `pillow_c_image_metadata_hotspot`
- `pillow_c_image_metadata_dib_compression`
- `pillow_c_image_channels`
- `pillow_c_image_stride`
- `pillow_c_image_size`
- `pillow_c_image_data`
- `pillow_c_image_readonly`
- `pillow_c_image_refresh_buffer`
- `pillow_c_image_detach_buffer`
- `pillow_c_image_set_bytes`
- `pillow_c_image_put_palette_rgb`
- `pillow_c_image_put_palette_rgba`
- `pillow_c_image_get_palette_rgb`
- `pillow_c_image_get_palette_rgba`
- `pillow_c_image_palette_alpha_mode`
- `pillow_c_image_remap_palette`
- `pillow_c_image_set_raw_bytes`
- `pillow_c_image_put_data`
- `pillow_c_image_open_bmp`
- `pillow_c_image_save_bmp`
- `pillow_c_image_open_ppm`
- `pillow_c_image_save_ppm`
- `pillow_c_image_open_qoi`
- `pillow_c_image_save_qoi`
- `pillow_c_image_open_tga`
- `pillow_c_image_save_tga`
- `pillow_c_image_save_tga_options`
- `pillow_c_image_open_xbm`
- `pillow_c_image_save_xbm`
- `pillow_c_image_save_xbm_options`
- `pillow_c_image_open_ico`
- `pillow_c_image_open_cur`
- `pillow_c_image_open_ico_size`
- `pillow_c_image_ico_sizes`
- `pillow_c_image_ico_payload_format`
- `pillow_c_image_ico_payload_dib_metadata`
- `pillow_c_image_save_ico`
- `pillow_c_image_save_ico_options`
- `pillow_c_image_save_ico_format_options`
- `pillow_c_image_save_ico_frames_format_options`
- `pillow_c_image_open_png`
- `pillow_c_image_save_png`
- `pillow_c_image_save_png_compress_level`
- `pillow_c_image_save_png_options`
- `pillow_c_image_save_png_transparency_options`
- `pillow_c_image_save_png_transparency_table_options`
- `pillow_c_image_save_png_rgb_transparency_options`
- `pillow_c_image_save_png_rgb_transparency_bytes_options`
- `pillow_c_image_save_png_text_options`
- `pillow_c_image_save_png_text_entries_options`
- `pillow_c_image_save_png_text_entries_value_sizes_options`
- `pillow_c_image_save_png_text_entries_chunk_options`
- `pillow_c_image_save_png_text_entries_chunk_rgb_transparency_options`
- `pillow_c_image_save_png_text_entries_ex_options`
- `pillow_c_image_save_png_text_entries_kind_options`
- `pillow_c_image_save_png_text_entries_itxt_options`
- `pillow_c_image_save_png_text_entries_icc_options`
- `pillow_c_image_save_png_text_entries_exif_options`
- `pillow_c_image_save_png_text_entries_icc_exif_options`
- `pillow_c_image_save_png_icc_options`
- `pillow_c_image_save_png_exif_options`
- `pillow_c_image_save_png_icc_exif_options`
- `pillow_c_image_save_png_interlace_options`
- `pillow_c_image_save_png_gamma_options`
- `pillow_c_image_save_png_gama_options`
- `pillow_c_image_save_png_chunk_options`
- `pillow_c_image_save_png_chunk_icc_options`
- `pillow_c_image_save_png_chunk_exif_options`
- `pillow_c_image_save_png_chunk_rgb_transparency_options`
- `pillow_c_image_save_png_chunk_rgb_transparency_bytes_options`
- `pillow_c_image_save_png_custom_chunks_options`
- `pillow_c_image_save_png_text_entries_custom_chunks_options`
- `pillow_c_image_save_png_text_entries_custom_chunks_kind_options`
- `pillow_c_image_save_png_metadata_custom_chunks_options`
- `pillow_c_image_save_png_optimize_options`
- `pillow_c_image_save_png_metadata_options`
- `pillow_c_image_open_jpeg`
- `pillow_c_image_open_jpeg_draft`
- `pillow_c_image_open_jpeg_draft_mode`
- `pillow_c_image_save_jpeg`
- `pillow_c_image_save_jpeg_quality`
- `pillow_c_image_save_jpeg_options`
- `pillow_c_image_save_jpeg_subsampling_options`
- `pillow_c_image_save_jpeg_encode_options`
- `pillow_c_image_save_jpeg_extra_options`
- `pillow_c_image_save_jpeg_restart_marker_blocks_options`
- `pillow_c_image_save_jpeg_restart_marker_rows_options`
- `pillow_c_image_save_jpeg_metadata_restart_marker_options`
- `pillow_c_image_save_jpeg_metadata_restart_marker_encode_options`
- `pillow_c_image_save_jpeg_metadata_restart_marker_progressive_options`
- `pillow_c_image_save_jpeg_encode_keep_rgb_options`
- `pillow_c_image_save_jpeg_qtables_encode_options`
- `pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options`
- `pillow_c_image_save_jpeg_keep_rgb_restart_marker_encode_options`
- `pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options`
- `pillow_c_image_save_jpeg_metadata_options`
- `pillow_c_image_save_jpeg_metadata_subsampling_options`
- `pillow_c_image_save_jpeg_metadata_encode_options`
- `pillow_c_image_save_jpeg_metadata_xmp_encode_options`
- `pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options`
- `pillow_c_image_save_jpeg_metadata_keep_rgb_xmp_encode_options`
- `pillow_c_image_save_jpeg_qtables_metadata_encode_options`
- `pillow_c_image_save_jpeg_qtables_metadata_xmp_encode_options`
- `pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options`
- `pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_xmp_encode_options`
- `pillow_c_image_open_tiff`
- `pillow_c_image_open_tiff_frame`
- `pillow_c_image_frame_count_tiff`
- `pillow_c_image_save_tiff`
- `pillow_c_image_save_tiff_options`
- `pillow_c_image_save_tiff_compression_options`
- `pillow_c_image_save_tiff_frames`
- `pillow_c_image_open_gif`
- `pillow_c_image_open_gif_frame`
- `pillow_c_image_frame_count_gif`
- `pillow_c_image_gif_metadata`
- `pillow_c_image_gif_metadata_ex`
- `pillow_c_image_gif_comment`
- `pillow_c_image_save_gif`
- `pillow_c_image_save_gif_options`
- `pillow_c_image_save_gif_comment`
- `pillow_c_image_save_gif_comment_options`
- `pillow_c_image_save_gif_animation`
- `pillow_c_image_save_gif_animation_options`
- `pillow_c_image_save_gif_animation_metadata_options`
- `pillow_c_image_save_gif_animation_background_options`
- `pillow_c_image_save_gif_animation_comment`
- `pillow_c_image_save_gif_animation_comment_metadata_options`
- `pillow_c_image_save_gif_animation_comment_background_options`
- `pillow_c_image_linear_gradient`
- `pillow_c_image_radial_gradient`
- `pillow_c_image_effect_mandelbrot`
- `pillow_c_image_effect_noise`
- `pillow_c_image_effect_spread`
- `pillow_c_image_fill`
- `pillow_c_image_getpixel`
- `pillow_c_image_putpixel`
- `pillow_c_image_draw_rectangle`
- `pillow_c_image_draw_ellipse`
- `pillow_c_image_draw_arc`
- `pillow_c_image_draw_chord`
- `pillow_c_image_draw_pieslice`
- `pillow_c_image_draw_rounded_rectangle`
- `pillow_c_image_draw_bitmap`
- `pillow_c_image_draw_floodfill`
- `pillow_c_image_draw_line`
- `pillow_c_image_draw_line_joint`
- `pillow_c_image_draw_points`
- `pillow_c_image_draw_polygon`
- `pillow_c_image_draw_text`
- `pillow_c_image_draw_text_anchor`
- `pillow_c_image_draw_text_stroke`
- `pillow_c_image_draw_text_anchor_stroke`
- `pillow_c_image_draw_text_font`
- `pillow_c_image_draw_text_font_stroke`
- `pillow_c_image_draw_text_font_anchor`
- `pillow_c_image_draw_text_font_anchor_stroke`
- `pillow_c_image_draw_multiline_text`
- `pillow_c_image_draw_multiline_text_align`
- `pillow_c_image_draw_multiline_text_anchor`
- `pillow_c_image_draw_multiline_text_align_stroke`
- `pillow_c_image_draw_multiline_text_anchor_stroke`
- `pillow_c_image_draw_multiline_text_font`
- `pillow_c_image_draw_multiline_text_font_align`
- `pillow_c_image_draw_multiline_text_font_align_stroke`
- `pillow_c_image_draw_multiline_text_font_anchor`
- `pillow_c_image_draw_multiline_text_font_anchor_stroke`
- `pillow_c_image_textlength`
- `pillow_c_image_textbbox`
- `pillow_c_image_textbbox_stroke`
- `pillow_c_image_textbbox_anchor`
- `pillow_c_image_textbbox_anchor_stroke`
- `pillow_c_image_textbbox_font_anchor`
- `pillow_c_image_textbbox_font_anchor_stroke`
- `pillow_c_image_multiline_textbbox`
- `pillow_c_image_multiline_textbbox_align`
- `pillow_c_image_multiline_textbbox_align_f64`
- `pillow_c_image_multiline_textbbox_align_stroke`
- `pillow_c_image_multiline_textbbox_align_stroke_f64`
- `pillow_c_image_multiline_textbbox_anchor_f64`
- `pillow_c_image_multiline_textbbox_anchor_stroke_f64`
- `pillow_c_image_multiline_textbbox_font`
- `pillow_c_image_multiline_textbbox_font_align`
- `pillow_c_image_multiline_textbbox_font_align_stroke`
- `pillow_c_image_multiline_textbbox_font_align_f64`
- `pillow_c_image_multiline_textbbox_font_align_stroke_f64`
- `pillow_c_image_multiline_textbbox_font_anchor_f64`
- `pillow_c_image_multiline_textbbox_font_anchor_stroke_f64`
- `pillow_c_image_get_bytes`
- `pillow_c_image_get_raw_bytes`
- `pillow_c_image_tobitmap`
- `pillow_c_image_histogram`
- `pillow_c_image_histogram_masked`
- `pillow_c_image_entropy`
- `pillow_c_image_get_extrema`
- `pillow_c_image_get_extrema_numeric`
- `pillow_c_image_getbbox`
- `pillow_c_image_getprojection`
- `pillow_c_image_getcolors`
- `pillow_c_image_getcolors_numeric`

Image operations:

- `pillow_c_image_copy`
- `pillow_c_image_constant`
- `pillow_c_image_chops_invert`
- `pillow_c_image_blend`
- `pillow_c_image_composite`
- `pillow_c_image_difference`
- `pillow_c_image_multiply`
- `pillow_c_image_screen`
- `pillow_c_image_soft_light`
- `pillow_c_image_hard_light`
- `pillow_c_image_overlay`
- `pillow_c_image_lighter`
- `pillow_c_image_darker`
- `pillow_c_image_add`
- `pillow_c_image_subtract`
- `pillow_c_image_add_modulo`
- `pillow_c_image_subtract_modulo`
- `pillow_c_image_logical_and`
- `pillow_c_image_logical_or`
- `pillow_c_image_logical_xor`
- `pillow_c_image_offset`
- `pillow_c_image_point_lut`
- `pillow_c_image_point_lut_mode`
- `pillow_c_image_invert`
- `pillow_c_image_posterize`
- `pillow_c_image_solarize`
- `pillow_c_image_colorize`
- `pillow_c_image_equalize`
- `pillow_c_image_equalize_masked`
- `pillow_c_image_autocontrast`
- `pillow_c_image_get_channel`
- `pillow_c_image_split_bands`
- `pillow_c_image_put_alpha_value`
- `pillow_c_image_put_alpha_image`
- `pillow_c_image_convert_mode`
- `pillow_c_image_convert_mode_dither`
- `pillow_c_image_convert_matrix`
- `pillow_c_image_quantize`
- `pillow_c_image_quantize_palette`
- `pillow_c_image_merge_bands`
- `pillow_c_image_rgb_to_l`
- `pillow_c_image_alpha_composite_rgba`
- `pillow_c_image_alpha_composite_rgba_in_place`
- `pillow_c_image_crop`
- `pillow_c_image_expand`
- `pillow_c_image_resize`
- `pillow_c_image_resize_box`
- `pillow_c_image_resize_reducing_gap`
- `pillow_c_image_reduce`
- `pillow_c_image_filter_kernel`
- `pillow_c_image_filter_rank`
- `pillow_c_image_filter_mode`
- `pillow_c_image_filter_box_blur`
- `pillow_c_image_filter_gaussian_blur`
- `pillow_c_image_filter_unsharp_mask`
- `pillow_c_image_filter_color_3d_lut`
- `pillow_c_image_transform_affine`
- `pillow_c_image_transform_perspective`
- `pillow_c_image_transform_quad`
- `pillow_c_image_transform_mesh`
- `pillow_c_image_rotate`
- `pillow_c_image_contain`
- `pillow_c_image_cover`
- `pillow_c_image_fit`
- `pillow_c_image_pad`
- `pillow_c_image_paste`
- `pillow_c_image_paste_masked`
- `pillow_c_image_paste_color`
- `pillow_c_image_transpose`

Reusable target operations:

- `pillow_c_image_linear_gradient_into`
- `pillow_c_image_radial_gradient_into`
- `pillow_c_image_remap_palette_into`
- `pillow_c_image_copy_into`
- `pillow_c_image_constant_into`
- `pillow_c_image_chops_invert_into`
- `pillow_c_image_blend_into`
- `pillow_c_image_composite_into`
- `pillow_c_image_difference_into`
- `pillow_c_image_multiply_into`
- `pillow_c_image_screen_into`
- `pillow_c_image_soft_light_into`
- `pillow_c_image_hard_light_into`
- `pillow_c_image_overlay_into`
- `pillow_c_image_lighter_into`
- `pillow_c_image_darker_into`
- `pillow_c_image_add_into`
- `pillow_c_image_subtract_into`
- `pillow_c_image_add_modulo_into`
- `pillow_c_image_subtract_modulo_into`
- `pillow_c_image_logical_and_into`
- `pillow_c_image_logical_or_into`
- `pillow_c_image_logical_xor_into`
- `pillow_c_image_offset_into`
- `pillow_c_image_point_lut_into`
- `pillow_c_image_point_lut_mode_into`
- `pillow_c_image_invert_into`
- `pillow_c_image_posterize_into`
- `pillow_c_image_solarize_into`
- `pillow_c_image_colorize_into`
- `pillow_c_image_equalize_into`
- `pillow_c_image_equalize_masked_into`
- `pillow_c_image_autocontrast_into`
- `pillow_c_image_get_channel_into`
- `pillow_c_image_put_alpha_value_into`
- `pillow_c_image_put_alpha_image_into`
- `pillow_c_image_convert_mode_into`
- `pillow_c_image_convert_mode_dither_into`
- `pillow_c_image_convert_matrix_into`
- `pillow_c_image_quantize_into`
- `pillow_c_image_quantize_palette_into`
- `pillow_c_image_merge_bands_into`
- `pillow_c_image_rgb_to_l_into`
- `pillow_c_image_alpha_composite_rgba_into`
- `pillow_c_image_crop_into`
- `pillow_c_image_expand_into`
- `pillow_c_image_resize_into`
- `pillow_c_image_resize_box_into`
- `pillow_c_image_resize_reducing_gap_into`
- `pillow_c_image_reduce_into`
- `pillow_c_image_filter_kernel_into`
- `pillow_c_image_filter_rank_into`
- `pillow_c_image_filter_mode_into`
- `pillow_c_image_filter_box_blur_into`
- `pillow_c_image_filter_gaussian_blur_into`
- `pillow_c_image_filter_unsharp_mask_into`
- `pillow_c_image_filter_color_3d_lut_into`
- `pillow_c_image_transform_affine_into`
- `pillow_c_image_transform_perspective_into`
- `pillow_c_image_transform_quad_into`
- `pillow_c_image_transform_mesh_into`
- `pillow_c_image_rotate_into`
- `pillow_c_image_transpose_into`

`pillow_c_image_equalize` and `pillow_c_image_equalize_into` implement Pillow's `ImageOps.equalize` for `L` and `RGB`; mode `P` sources are converted through their RGB palette first and produce an `RGB` target. `pillow_c_image_equalize_masked` and `pillow_c_image_equalize_masked_into` accept a same-size mode `1` or `L` mask handle after the source handle. A null mask keeps full-image histogram behavior. The equalize exports refresh attached readonly `frombuffer` source views, and masked equalize refreshes both source and mask views, before building histograms or LUTs; facade ImageOps equalize relies on this native refresh and does not perform wrapper-level pre-refresh. Numeric modes `I` and `F` return `PILLOW_C_INVALID_ARGUMENT`; `MODE-NUM-001O` is a facade-only mapping layer over that existing native rejection.

`pillow_c_image_point_lut`, `pillow_c_image_point_lut_mode`, `pillow_c_image_point_lut_into`, and `pillow_c_image_point_lut_mode_into` refresh attached readonly `frombuffer` source views before reading pixels, matching Pillow 11.3.0's `Image.point(...)` source-read behavior while leaving the source view readonly and returning or reusing owned target storage. The target-mode path accepts single-band `1`, `L`, and `P` sources targeting `1`, `L`, or `P`; same-mode calls reuse `pillow_c_image_point_lut` behavior. LUT length remains `source_channels * 256`, `_into` targets must already match the output shape and mode, `P -> P` preserves the RGB palette, and `P -> 1/L` keeps the core palette metadata like Pillow 11.3.0.

`pillow_c_image_put_data` accepts already-packed mode-sized pixel bytes plus a pixel count, writes that prefix into the image in row-major order, and leaves any remaining pixels unchanged. The AHK facade owns Python-like `putdata` value coercion before making this single native call.

`pillow_c_image_draw_rectangle` mutates one image handle in place for the first ImageDraw native primitive. It accepts inclusive integer coordinates, optional caller-packed fill and outline colors, and an outline width. The behavior follows Pillow 11.3.0's `ImageDraw.rectangle`: fill is applied first, outline is skipped when `width <= 0`, `right < left` or `bottom < top` returns `-3`, and drawing is clipped to the image bounds. The AHK facade owns Pillow-style scalar/tuple color packing before making this single native call. Exported ImageDraw mutators detach active readonly `frombuffer` buffer views before writing, so source-buffer aliases are refreshed once, `readonly` is cleared, and later caller-buffer mutation no longer affects the drawn image.

`pillow_c_image_draw_ellipse` mutates one image handle in place for Pillow `ImageDraw.ellipse` calls. It accepts inclusive integer bounding-box coordinates, optional caller-packed fill and outline colors, and an outline width. The implementation follows Pillow 11.3.0's `ellipseNew` integer span generator: fill is applied first with the native full-width ellipse rule, outline is skipped when `width == 0`, reversed coordinates return `-3`, and drawing is clipped to the image bounds.

`pillow_c_image_draw_arc` mutates one image handle in place for Pillow `ImageDraw.arc` calls. It accepts inclusive integer bounding-box coordinates, start/end angles in degrees, a caller-packed stroke color, and a width. The implementation follows Pillow 11.3.0's `ImagingDrawArc` and `arcNew` paths: angles are normalized before drawing, full-circle arcs reuse the native ellipse outline path, equal start/end angles are a no-op, and ordinary arcs use the Pillow clip-ellipse half-plane tree over integer ellipse spans. Reversed coordinates return `-3`, `width <= 0` is a no-op, and drawing is clipped to the image bounds.

`pillow_c_image_draw_chord` mutates one image handle in place for Pillow `ImageDraw.chord` calls. It accepts inclusive integer bounding-box coordinates, start/end angles in degrees, optional caller-packed fill and outline colors, and an outline width. The implementation follows Pillow 11.3.0's `ImagingDrawChord` path: angles are normalized, full-circle chords delegate to the native ellipse path, fill uses the chord clip tree with Pillow's full-width fill rule, and outline draws the chord line plus clipped ellipse boundary. Equal start/end angles and `width == 0` outlines are no-ops; reversed coordinates return `-3`.

`pillow_c_image_draw_pieslice` mutates one image handle in place for Pillow `ImageDraw.pieslice` calls. It accepts inclusive integer bounding-box coordinates, start/end angles in degrees, optional caller-packed fill and outline colors, and an outline width. The implementation follows Pillow 11.3.0's `ImagingDrawPieslice` path: angles are normalized, full-circle pieslices delegate to the native ellipse path, fill uses the pie clip tree, and outline draws both radial sides, the center join ellipse, and the clipped curved boundary. Equal start/end angles and `width == 0` outlines are no-ops; reversed coordinates return `-3`.

`pillow_c_image_draw_rounded_rectangle` mutates one image handle in place for Pillow `ImageDraw.rounded_rectangle` calls. It accepts inclusive integer bounding-box coordinates, a finite non-negative radius, optional caller-packed fill and outline colors, an outline width, and a corners bitmask in top-left, top-right, bottom-right, bottom-left order. The implementation follows Pillow 11.3.0's wrapper composition: radius-zero and no-corner cases delegate to rectangle, fully joined all-corner cases delegate to ellipse, and ordinary rounded rectangles draw native pieslice/arc corner spans plus native rectangle bars in one DLL call. Reversed coordinates and invalid masks return `-3`; drawing is clipped to image bounds.

`pillow_c_image_draw_bitmap` mutates one image handle in place for Pillow `ImageDraw.bitmap` calls. It accepts an inclusive destination origin, a native bitmap/mask image handle, and a caller-packed fill color. The implementation follows Pillow 11.3.0's `ImagingDrawBitmap`/`ImagingFill2` path: mode `1` masks write the fill color where nonzero, mode `L` and `RGBA` masks alpha-blend the fill color, other mask modes return `-3`, color length must match the destination channel count, and drawing is clipped to the destination bounds.

`pillow_c_image_draw_floodfill` mutates one image handle in place for Pillow `ImageDraw.floodfill` calls. It accepts a seed coordinate, caller-packed value color, optional caller-packed border color, and a threshold. The implementation follows Pillow 11.3.0's Python flood-fill semantics while moving the queue walk into C++: seed pixels use Pillow coordinate normalization, out-of-range seeds are no-ops, no-border mode fills pixels whose 1-norm color difference from the seed background is within `thresh`, and ordinary border mode fills pixels that are neither the fill value nor the border value. A non-null border pointer with `border_size == 0` is the explicit supplied-but-incomparable state: it retains border-mode traversal and fill-value rejection but skips border-buffer comparison entirely.

`pillow_c_image_draw_line` mutates one image handle in place for ordinary Pillow `ImageDraw.line` calls. It accepts a pointer to packed `int x, y` pairs, a point count, a caller-packed color, and a width. The current verified path supports `width <= 1` Bresenham-style segments with the final endpoint draw, plus `width > 1` segment filling through Pillow's wide-line quadrilateral rules. Multi-segment wide lines draw each segment separately like Pillow's C core, clipped to image bounds.

`pillow_c_image_draw_line_joint` extends the line path with a `joint_curve` flag for Pillow `ImageDraw.line(..., joint="curve")`. It first draws the ordinary native wide polyline, then for `width > 4` and non-straight interior vertices adds Pillow-style filled pieslice joints. For `width > 8`, it also adds Pillow's narrow gap-cover line between the calculated tangent points. The implementation follows Pillow 11.3.0's `ImageDraw.line` wrapper angle, flipped-arc, and `coord_at_angle` rules while keeping all intermediate drawing in one DLL call from AHK.

`pillow_c_image_draw_points` mutates one image handle in place for Pillow `ImageDraw.point` calls. It accepts a pointer to packed `int x, y` pairs, a point count, and a caller-packed color. Empty point lists are a no-op, single points are valid, out-of-bounds points are clipped away, and color length must match the image channel count.

`pillow_c_image_draw_polygon` mutates one image handle in place for Pillow `ImageDraw.polygon` calls. It accepts packed `int x, y` vertices, optional caller-packed fill and outline colors, and an outline width. The verified path supports fill and outlines using Pillow's scanline edge rules and closed-outline behavior, clips to the image bounds, and accepts two-point line-like polygons. For `width > 1`, it follows Pillow 11.3.0's wrapper strategy: fill a same-size mode `1` polygon mask, draw `width * 2 - 1` wide outline segments, and apply those segments only where the mask is nonzero so the outline does not expand outside the polygon.

`pillow_c_font_load_default`, `pillow_c_font_free`, `pillow_c_font_getlength`, `pillow_c_font_getbbox`, `pillow_c_font_getbbox_anchor`, `pillow_c_font_getmetrics`, `pillow_c_font_getname`, and `pillow_c_font_variant` establish the initial native `ImageFont` handle ABI. The current font backend embeds Pillow 11.3.0's default font masks and metrics for printable ASCII (`0x20..0x7E`), reports default `FreeTypeFont` metadata as ascent/descent `10/3` and name `Aileron`/`Regular`, clones independent default-font handles through `font_variant`, and rejects non-ASCII text with `-3`. `pillow_c_image_draw_text` keeps the legacy implicit-default-font call, while `pillow_c_image_draw_text_font` accepts an explicit native font handle so the AHK facade can pass `Pillow.ImageFont.LoadDefault()` into draw calls. The single-line anchor exports (`pillow_c_image_draw_text_anchor`, `pillow_c_image_draw_text_font_anchor`, `pillow_c_image_textbbox_anchor`, and `pillow_c_image_textbbox_font_anchor`) support Pillow's default-font anchor pairs with horizontal `l/m/r` and vertical `a/t/m/b/d/s`. The single-line stroke exports add bounded default-font `stroke_width`/`stroke_fill` drawing and bbox expansion. `pillow_c_image_draw_multiline_text`, `pillow_c_image_draw_multiline_text_font`, `pillow_c_image_multiline_textbbox`, and `pillow_c_image_multiline_textbbox_font` keep the legacy left-aligned multiline path. The `_align` variants add an integer alignment id (`0=left`, `1=center`, `2=right`) for Pillow-style default-font multiline text with integer `spacing`, including trailing empty-line bbox behavior and center/right per-line drawing offsets. The legacy aligned bbox exports return integer-expanded boxes, while `_align_f64` exports preserve Pillow's fractional bbox coordinates for center/right alignment. The multiline anchor exports (`pillow_c_image_draw_multiline_text_anchor`, `pillow_c_image_draw_multiline_text_font_anchor`, `pillow_c_image_multiline_textbbox_anchor_f64`, and `pillow_c_image_multiline_textbbox_font_anchor_f64`) mirror Pillow's horizontal-text multiline anchor handling for horizontal `l/m/r` and vertical `a/m/d/s`; vertical `t/b` anchors return invalid argument like Pillow's unsupported multiline-anchor path. The multiline stroke exports add the same bounded default-font stroke path and use Pillow's stroke-aware line spacing (`10 + spacing + 2 * stroke_width`) for drawing and bbox calculations. Full FreeType loading, Unicode glyph coverage, `justify`, direction/features/language, and variation axes/names remain future ABI surfaces.

`pillow_c_image_remap_palette` and `pillow_c_image_remap_palette_into` implement Pillow's `Image.remap_palette` for mode `P` and `L` images over RGB palettes. The native path builds the new palette from `dest_map`, remaps all one-byte pixels in one pass, returns a mode `P` image, and uses Pillow's default grayscale source palette for `L` inputs. RGBA palette remapping is intentionally outside the current RGB palette ABI.

`pillow_c_image_put_palette_rgba` accepts normalized RGBA palette bytes plus an alpha-mode id: `0` means no stored alpha metadata, `1` means Pillow `RGBA` palette semantics, and `2` means Pillow `RGBX` palette semantics. `pillow_c_image_get_palette_rgba` returns RGB plus stored alpha bytes, or `255` alpha when no alpha metadata exists. `pillow_c_image_palette_alpha_mode` lets the AHK facade reproduce Pillow's rawmode restrictions, including `getpalette("RGBX")`/`getpalette("BGRX")` rejection for true `RGBA` palettes.

`pillow_c_image_resize_box` and `pillow_c_image_resize_box_into` expose Pillow-style `Image.resize(..., box=...)` sampling directly against a source region. The current ABI accepts finite positive-area boxes contained inside the source image, supports the same resampling IDs as `pillow_c_image_resize`, preserves same-mode palettes, uses premultiplied color sampling for `LA` and `RGBA`, returns `-3` for invalid boxes, and returns `-5` for `_into` target shape or mode mismatches. Resize source reads refresh active readonly `frombuffer` buffer views before sampling while keeping the source view attached.

`pillow_c_image_resize_reducing_gap` and `pillow_c_image_resize_reducing_gap_into` expose Pillow-style `Image.resize(..., box=..., reducing_gap=...)` as one native operation. `reducing_gap` must be finite and at least `1.0`; when the computed factor is greater than one for either axis, the DLL computes Pillow's safe reduce box, runs native integer `reduce`, then runs final box resize against the reduced temporary. Modes `1` and `P` force `NEAREST` like Pillow, so palette images avoid reduce and preserve palette metadata. Invalid boxes or gaps return `-3`, and `_into` target shape or mode mismatches return `-5`. Like ordinary resize and box resize, reducing-gap source reads refresh active readonly `frombuffer` buffer views before sampling.

`pillow_c_image_set_raw_bytes` and `pillow_c_image_get_raw_bytes` implement common Pillow raw decoder/encoder modes without AHK-side byte reordering. The current raw decode support covers `1`->`1`, `L`->`L`, `LA`->`LA`, `CMYK`->`CMYK`, `I`/`I;32`/`I;32B`/`I;32N` raw input into mode `I`, `I;16`/`I;16B`/`I;16N` raw input into mode `I`, direct `I;16` raw input into mode `I;16`, `F`/`F;32F`/`F;32BF`/`F;32NF` raw input into mode `F`, `RGB` target raw modes `RGB`, `RGBX`, `BGR`, `BGRX`, `XBGR`, `RGBA` target raw modes `RGBA`, `BGRA`, `ARGB`, `ABGR`, `BGR`, and public `RGBX` target rawmode `RGBX`. Mode `1` raw bytes are bit-packed most-significant-bit first per row, while native image storage remains one byte per pixel. Mode `I` raw bytes are stored as little-endian 32-bit slots; 16-bit raw decoders expand unsigned samples into those 32-bit slots, and big/native-endian 32-bit aliases normalize into the same internal storage. Mode `I;16` raw bytes are stored as direct little-endian 16-bit slots. Mode `F` raw bytes are stored as little-endian float32 slots, with big/native-endian aliases normalized during decode. Decode accepts a non-negative source stride, where `0` means tightly packed, and negative orientation reads rows bottom-up. Raw encode support covers matching direct modes, mode `I` to `I;16B` with Pillow-style `0..65535` clipping, direct `I;16`, direct `F`/`F;32F` plus native-endian `F;32NF`, public `RGBX` rawmode `RGBX`, and common `RGB`/`RGBA` BGR-family packers; callers can first pass a null output pointer to query the required byte size. Raw byte reads refresh active readonly `frombuffer` buffer views before copying or encoding while keeping the source view attached.

`pillow_c_image_frombuffer_raw` creates a mode-aware handle from raw decoder
bytes and can record a caller-owned external buffer as a readonly buffer view.
For Pillow raw mapmodes `L`, `RGBA`, and `RGBX`, the export derives the output
handle mode from the raw mode before allocation, so a requested `RGB`
constructor with rawmode `L` produces an `L` handle, rawmode `RGBA` produces an
`RGBA` handle, and rawmode `RGBX` produces a public `RGBX` handle.
`pillow_c_image_refresh_buffer` bulk-refreshes the native storage from that
external buffer using the stored raw mode, stride, and orientation;
`pillow_c_image_detach_buffer` performs that refresh once and clears the
readonly view so later source-buffer mutations no longer affect the image.
`pillow_c_image_readonly` reports whether the handle still has an active
readonly buffer view. The DLL does not own the external pointer; the AHK facade
must keep the source `Buffer` alive for as long as the view remains readonly.
Direct `RGB` rawmode follows Pillow's copy semantics: even if a raw DLL caller
passes a nonzero `alias_source`, `RGB` storage is owned by the new handle,
`pillow_c_image_readonly` reports `0`, and later source-buffer mutations do not
refresh into the image. Current public facade coverage is the bounded
Pillow-compatible `L` alias/detach lifecycle, raw mapmode
`L`/`RGBA`/`RGBX` override aliases, direct `RGB` copy semantics, direct `RGBA`
stride/orientation aliasing, readonly detach before native ImageDraw and Paste
writes, readonly refresh before native Resize/Thumbnail sampling, and readonly
refresh before native raw byte, histogram, getbbox, getprojection, getcolors,
entropy, getextrema, convert, getpixel, crop, full-image copy, band extraction,
merge-bands, ImageFilter, point/LUT, ImageOps LUT, ImageChops unary/binary,
blend/composite, offset, transpose, PNG save, JPEG save, GIF save, and the
remaining native save paths for BMP, TIFF frames, PPM/PGM, TGA, QOI, XBM, and
ICO reads, backed by these native bulk paths. `pillow_c_image_entropy`
refreshes attached readonly `frombuffer` source and mask views before computing
entropy; `pillow_c_image_get_extrema` refreshes an attached readonly source
view before computing byte-band extrema; `pillow_c_image_convert_mode*`
refreshes an attached readonly source view before converting;
`pillow_c_image_getpixel` refreshes an attached readonly source view before
reading one pixel; `pillow_c_image_crop` and `pillow_c_image_crop_into` refresh
an attached readonly source view before copying crop pixels; `pillow_c_image_copy`
and `pillow_c_image_copy_into` refresh an attached readonly source view before
copying full-image pixels; `pillow_c_image_get_channel`,
`pillow_c_image_get_channel_into`, and `pillow_c_image_split_bands` refresh an
attached readonly source view before copying band pixels;
`pillow_c_image_merge_bands` and `pillow_c_image_merge_bands_into` refresh
attached readonly source band views before interleaving pixels; native filter
exports refresh attached readonly source views before sampling pixels; native
blend/composite exports refresh attached readonly source and mask views before
reading pixels; native offset exports refresh attached readonly source views
before reading pixels; native transpose exports refresh attached readonly
source views before reading pixels; native PNG, JPEG, GIF, BMP, TIFF, PPM/PGM,
TGA, QOI, XBM, and ICO save exports refresh attached readonly source views
before encoding pixels.

`pillow_c_image_open_bmp` and `pillow_c_image_save_bmp` are the first native file-format entry points. Paths are UTF-8 strings from AHK and are opened through Windows wide-path APIs. The current BMP support is intentionally uncompressed Windows BMP: open accepts 8-bit indexed/grayscale, 24-bit BGR, and 32-bit BGRA; save supports `L`, `RGB`, and `RGBA`. `RGB` save bytes match Pillow's 24-bit BMP output, `L` saves with a grayscale palette, and `RGBA` saves as 32-bit BGRA like Pillow, which opens back as `RGB`.

`pillow_c_image_open_ppm` and `pillow_c_image_save_ppm` keep Netpbm PBM/PGM/PPM files in the DLL. Native open supports plain `P1` and binary `P4` bitmap as mode `1`, plain `P2` and binary `P5` grayscale as mode `L` when `maxval <= 255`, high-bit-depth `P2`/`P5` grayscale as mode `I`, and plain `P3` plus binary `P6` truecolor as mode `RGB` when `maxval < 65536`. Non-255 8-bit samples are scaled to 8-bit with Pillow's half-even rounding. High-bit-depth grayscale samples are scaled to Pillow's `0..65535` `I` range with half-even rounding and stored as little-endian 32-bit values; `maxval=65535` keeps the sample value directly. Binary over-range samples clamp through the scaling path, while plain over-range samples reject like Pillow. PBM bits are inverted relative to Pillow's external mode `1` raw bytes: Netpbm bit `1` is black and Pillow raw bit `1` is white. Native save writes Pillow-style binary `P4`/`P5`/`P6` headers for `1`, `L`, `I`, and `RGB` handles. Mode `I` writes `P5`, `maxval=65535`, and big-endian unsigned 16-bit samples after clipping signed int32 pixels to Pillow's `0..65535` PGM save range.

`pillow_c_image_open_qoi` and `pillow_c_image_save_qoi` keep Quite OK Image encode/decode inside the DLL. The current path supports Pillow-compatible `RGB` and `RGBA` QOI files, writes Pillow's default colorspace byte, and rejects unsupported modes such as `L`, `P`, and `CMYK` with `-3`.

`pillow_c_image_open_tga`, `pillow_c_image_save_tga`, and `pillow_c_image_save_tga_options` keep Truevision TGA decode/encode inside the DLL. The current path supports Pillow-compatible uncompressed and RLE `L`, `RGB`, `RGBA`, and 24-bit color-mapped `P` files: default save writes uncompressed image types `3`, `2`, and `1`; option save with `rle != 0` writes image types `11`, `10`, and `9`; both save paths write TGA 2.0 footer bytes, bottom-left origin rows, BGR/BGRA channel order for color images, and BGR palette entries for `P`. RLE encoding is row-bounded to match Pillow's TGA encoder packet boundaries. Open accepts 8-bit grayscale, 24/32-bit truecolor, and 8-bit indexed files with 24-bit color maps for both uncompressed and RLE files, handles top/bottom plus left/right origin descriptor bits, preserves RGB palette metadata on `P` handles, and rejects truncated RLE packets with `-2`. Non-24-bit palettes and advanced TGA metadata remain future surfaces.

`pillow_c_image_open_xbm`, `pillow_c_image_save_xbm`, and `pillow_c_image_save_xbm_options` keep X11 bitmap files in the DLL for Pillow mode `1` images. Native save writes Pillow-style `im_width`, `im_height`, and `im_bits[]` text with low-bit-first XBM bytes and 15 byte literals per line. The options path writes `im_x_hot` and `im_y_hot` between height and bits for non-negative integer hotspot pairs, matching Pillow's stable round-trippable XBM metadata path. Native open accepts ordinary Pillow XBM headers, decodes low-bit-first file bytes into the DLL's unpacked mode `1` storage where nonzero bits become `255`, stores non-negative integer hotspot pairs on the image handle, and reports truncated bitmap data with `-2`.

`pillow_c_image_open_ico`, `pillow_c_image_open_cur`,
`pillow_c_image_open_ico_size`,
`pillow_c_image_ico_sizes`, `pillow_c_image_ico_payload_format`,
`pillow_c_image_ico_payload_dib_metadata`, `pillow_c_image_save_ico`,
`pillow_c_image_save_ico_options`, `pillow_c_image_save_ico_format_options`,
and `pillow_c_image_save_ico_frames_format_options` keep the current ICO path
inside the DLL. Native default open parses the ICO directory, applies Pillow's
color-depth/square entry ordering, decodes the selected payload through WIC,
and returns a public `RGBA` handle; `pillow_c_image_open_ico_size` applies the
same ordering for an exact caller-selected positive frame size and returns
invalid argument when that size is absent; `pillow_c_image_ico_sizes`
enumerates sorted unique available frame sizes through WIC metadata for facade
`image.ico.sizes()` and `image.ico.getimage(...)` routing;
`pillow_c_image_ico_payload_format` applies the same selected-entry logic and
reports covered embedded PNG payload format metadata for facade
`ico.getimage(...)` results; `pillow_c_image_ico_payload_dib_metadata` applies
the same selected-entry logic, returns no DIB metadata for PNG-backed payloads,
and reports DIB compression plus positive DPI for facade `ico.getimage(...)`
child `Info`.
`pillow_c_image_open_cur` parses a CUR directory, rejects currently uncovered
PNG-backed cursor payloads, decodes a selected DIB-backed payload through the
same one-entry ICO wrapper used by ICO open, returns mode `RGBA`, and stores
DIB compression/DPI metadata for facade `Image.Open(... ".cur")` routing.
`pillow_c_image_metadata_dib_compression(image, out_has_compression,
out_compression)` exposes that handle-level compression metadata and returns
`out_compression == -1` when absent.

Native save writes PNG-backed ICO entries generated by the native PNG chunk
writer, with ICO directory bit depth set to Pillow's PNG-backed default `32`.
The default save path mirrors Pillow's built-in square icon sizes `16`, `24`,
`32`, `48`, `64`, `128`, and `256`, skipping any size that does not fit inside
the base image and using native LANCZOS resize for generated frames. The size
options path accepts a pointer to `size_count` integer pairs, sorts and
de-duplicates requested pairs like Pillow's `sorted(set(sizes))`, skips pairs
larger than the base image or ICO's `256x256` limit, and uses thumbnail-style
contained LANCZOS resizing for non-exact boxes. A zero explicit `size_count`
writes an empty ICO directory. `pillow_c_image_save_ico_format_options` adds a
`has_sizes` flag so callers can distinguish default sizes from explicit
`sizes=[]`, and only exact lowercase `bitmap_format="bmp"` selects
Pillow-style DIB-backed ICO payloads. The BMP-backed path supports Pillow's
`1`, `L`, `P`, `RGB`, and `RGBA` modes, writes doubled DIB heights, BGRA/BGR
pixel rows, palette entries where Pillow writes them, and raw 1-bit zero AND
masks for non-32-bit entries; other bitmap format strings fall back to
PNG-backed entries like Pillow. The multi-image frames export takes the base
image as `images[0]`, append images after it, uses the first exact source-size
match for each requested ICO entry, and otherwise thumbnails the last provided
image.

The current verified path covers facade `Image.Open(..., "ICO")`, opened-ICO
`image.Size := [w,h]` selected frame loading, `image.ico.sizes()`,
`image.ico.getimage([w,h])` exact-size and missing-size fallback behavior,
duplicate-size open color-depth selection, embedded PNG payload `Format`
metadata and DIB-backed payload `Info["dpi"]` / `Info["compression"]`
metadata on `ico.getimage(...)` results, `Image.Save(..., "ICO")`,
`Image.Save(..., "ICO", { Sizes: [...] })`, `Image.Save(..., "ICO",
{ BitmapFormat: "bmp" })`, `Image.Save(..., "ICO", { AppendImages: [...] })`,
raw DLL round-tripping for `RGBA`, default and selected-size open, native size
inventory, default multi-entry directories, custom size directories,
DIB-backed `RGBA`/`RGB` entries, uppercase `"BMP"` fallback, and append-image
exact source-size PNG-backed entries. CUR semantics, non-exact append-image
thumbnail tests, duplicate-size save bit-depth generation, nonzero DIB
compression metadata, and broader mixed-mode multi-source matrices remain
future surfaces.

`pillow_c_image_open_png`, `pillow_c_image_save_png`,
`pillow_c_image_save_png_compress_level`, `pillow_c_image_save_png_options`,
`pillow_c_image_save_png_transparency_options`,
`pillow_c_image_save_png_transparency_table_options`,
`pillow_c_image_save_png_rgb_transparency_options`,
`pillow_c_image_save_png_rgb_transparency_bytes_options`,
`pillow_c_image_save_png_text_options`, and
`pillow_c_image_save_png_text_entries_options` plus
`pillow_c_image_save_png_text_entries_chunk_options` plus
`pillow_c_image_save_png_text_entries_chunk_rgb_transparency_options` plus
`pillow_c_image_save_png_text_entries_ex_options`,
`pillow_c_image_save_png_text_entries_kind_options`,
`pillow_c_image_save_png_text_entries_itxt_options`,
`pillow_c_image_save_png_text_entries_icc_options`,
`pillow_c_image_save_png_text_entries_exif_options`,
`pillow_c_image_save_png_text_entries_icc_exif_options`, and
`pillow_c_image_save_png_icc_options` plus
`pillow_c_image_save_png_exif_options` plus
`pillow_c_image_save_png_icc_exif_options` plus
`pillow_c_image_save_png_interlace_options` plus
`pillow_c_image_save_png_gamma_options` plus
`pillow_c_image_save_png_gama_options` plus
`pillow_c_image_save_png_chunk_options` plus
`pillow_c_image_save_png_chunk_icc_options` plus
`pillow_c_image_save_png_chunk_exif_options` plus
`pillow_c_image_save_png_chunk_rgb_transparency_options` plus
`pillow_c_image_save_png_chunk_rgb_transparency_bytes_options` plus
`pillow_c_image_save_png_custom_chunks_options` plus
`pillow_c_image_save_png_text_entries_custom_chunks_options` plus
`pillow_c_image_save_png_text_entries_custom_chunks_kind_options` plus
`pillow_c_image_save_png_metadata_custom_chunks_options` plus
`pillow_c_image_save_png_optimize_options` plus
`pillow_c_image_save_png_metadata_options` keep PNG decode/encode
inside the DLL.
All PNG save exports that read source pixels refresh an active readonly
`frombuffer` view before encoding. This covers both the default WIC writer and
the custom PNG metadata/private-chunk writers, so
`Image.frombuffer(...).Save(..., "PNG")` samples current caller bytes while
leaving the view attached and readonly. `BYTES-001AC` added no export; source
and Release x64 DLL export counts remain `364` / `364`.
The current PNG path supports `L`, `LA`, `P`, `RGB`, and `RGBA` image handles.
Native open converts supported source PNG pixel formats into the DLL's
row-major public modes, preserves short RGB palettes for `P`, reads `pHYs` DPI
metadata into the handle, scans `gAMA` chunks into optional gamma metadata,
scans bounded `sRGB` chunks into optional rendering-intent metadata,
scans bounded `cHRM` chunks into optional chromaticity metadata,
scans uncompressed `tEXt`, bounded compressed `zTXt`, plus uncompressed and
bounded compressed `iTXt` chunks into indexed UTF-8 key/value metadata, scans
bounded `iCCP` chunks into decompressed ICC profile bytes, scans bounded
`eXIf` chunks into `Exif\0\0`-prefixed EXIF bytes, scans bounded XMP bytes
from `iTXt` key `XML:com.adobe.xmp` into the shared XMP metadata slot, and
scans bounded P-mode, grayscale, and RGB truecolor `tRNS` transparency
metadata.

For P-mode `tRNS`, native open parses `PLTE`/`tRNS`, decodes through RGBA when
needed, remaps decoded pixels back to original palette indexes in C++, stores
palette alpha metadata, and records the first fully transparent palette index
for Pillow-compatible `Info["transparency"]` exposure. For grayscale `tRNS`,
native open strips the `tRNS` chunk from the decode copy only for the
accepted 8-bit grayscale path so WIC does not zero the transparent sample,
stores the transparent grayscale scalar, and leaves the source `L` bytes
unchanged. For RGB truecolor `tRNS`, native open forces 8-bit color type `2`
PNGs into public `RGB` storage instead of WIC's transparency-expanded `RGBA`
surface, stores the transparent RGB tuple, and leaves the source RGB bytes
unchanged. For PNG files containing compressed text ancillary chunks, native
open may also feed WIC a decode-only memory copy with `zTXt` and compressed
`iTXt` chunks removed; the original file remains the source for native text
metadata scanning, so `Info` / `Text` exposure is unchanged while WIC cannot
hang on those chunks before the DLL reaches its own metadata path.

`pillow_c_image_metadata_png_gamma` exposes optional `gAMA` with
`out_has_gamma` set to `0` or `1` and `out_gamma` set to
`raw_int / 100000.0` when present, matching Pillow 11.3.0's
`Image.info["gamma"]`; missing gamma returns `out_has_gamma == 0` and
`out_gamma == 0.0`. `pillow_c_image_metadata_png_srgb` exposes optional
`sRGB` with `out_has_srgb` set to `0` or `1` and `out_srgb` set to the
single-byte rendering-intent value, matching the bounded Pillow 11.3.0
`Image.info["srgb"]` behavior. Missing `sRGB` returns `out_has_srgb == 0` and
`out_srgb == 0`. `pillow_c_image_metadata_png_chromaticity` exposes optional
`cHRM` with `out_has_chromaticity` set to `0` or `1`; callers pass a pointer
to at least eight doubles and a value count, and present metadata is returned
as the eight Pillow-compatible chromaticity values divided by `100000.0`.
`pillow_c_image_metadata_png_text_count` returns the number
of currently stored PNG text entries, and `pillow_c_image_metadata_png_text`
reads one zero-based entry into caller-provided UTF-8 key and value buffers
using the existing required-size pattern: it sets both required byte counts
including NUL, returns `-1` for a size probe with null output buffers, returns
`-2` when either buffer is too small, and returns `-3` for an out-of-range
index. Bounded `zTXt` and compressed `iTXt` open metadata, including the
dynamic-Huffman compressed text fixture from `FMT-PNG-001AB`, reuse these text
exports. Duplicate PNG text keywords are preserved as ordered indexed entries
in the native ABI; the facade collapses duplicate keys to the final value in
`Info` and `Text`, matching the bounded Pillow 11.3.0 oracle. No new ABI entry
was added for `FMT-PNG-001D`, `FMT-PNG-001E`, `FMT-PNG-001F`, or
`FMT-PNG-001AB`.
`pillow_c_image_metadata_png_icc_profile` exposes optional PNG ICC profile
bytes. Callers pass `out_has_profile`, an optional output byte buffer,
`out_profile_size`, and `out_profile_required`; missing metadata returns
`out_has_profile == 0`, `out_profile_required == 0`, and success. Present
metadata sets `out_has_profile == 1` and `out_profile_required` to the exact
profile byte length, returns `-1` for a size probe with a null output buffer,
returns `-2` when the buffer is too small, and copies the decompressed profile
bytes on success.
`pillow_c_image_metadata_png_exif` exposes optional PNG EXIF bytes. Callers
pass `out_has_exif`, an optional output byte buffer, `out_exif_size`, and
`out_exif_required`; missing metadata returns `out_has_exif == 0`,
`out_exif_required == 0`, and success. Present metadata sets
`out_has_exif == 1` and `out_exif_required` to the exact Pillow-style byte
length including the leading `Exif\0\0` header, returns `-1` for a size probe
with a null output buffer, returns `-2` when the buffer is too small, and
copies the EXIF bytes on success.
`pillow_c_image_metadata_xmp` exposes optional raw XMP bytes already attached
to an image handle. PNG open attaches the exact UTF-8 `iTXt`
`XML:com.adobe.xmp` value bytes for the bounded uncompressed or zlib-compressed
text route; JPEG open attaches APP1 XMP payload bytes after the XMP namespace
header; TIFF open attaches bounded IFD0 tag `700` BYTE or UNDEFINED payloads
for the covered RGB route, BYTE payloads for native `I;16`, `I`, and `F`
early-open handles, and BYTE payloads for selected nonzero little-endian
`I;16`, `I`, and `F` frame handles opened through
`pillow_c_image_open_tiff_frame`. The ABI
follows the same size-probe pattern as binary PNG/JPEG metadata exports:
missing metadata returns `out_has_xmp == 0`,
`out_xmp_required == 0`, and success; present metadata sets `out_has_xmp == 1`,
returns `-1` for a null output buffer probe, returns `-2` when the buffer is
too small, and copies the raw bytes on success.
`META-002F` changes only the facade parser shape over those same raw bytes:
repeated RDF sibling names such as `dc:creator/rdf:Seq/rdf:li` materialize as
arrays, text-only leaves materialize as scalar strings, and attribute-bearing
leaves remain maps. The raw metadata ABI remains byte-preserving and
unchanged.
`pillow_c_image_metadata_tiff_exif` exposes optional Pillow-style EXIF bytes
already attached to a TIFF image handle. Callers pass `out_has_exif`, an
optional output byte buffer, `out_exif_size`, and `out_exif_required`; missing
metadata returns `out_has_exif == 0`, `out_exif_required == 0`, and success.
Present metadata sets `out_has_exif == 1` and `out_exif_required` to the exact
Pillow-style byte length including the leading `Exif\0\0` header, returns
`-1` for a size probe with a null output buffer, returns `-2` when the buffer
is too small, and copies the EXIF bytes on success. The bounded native TIFF
open parser populates this blob with common ASCII IFD0 tags `269`, `270`,
`271`, `272`, `285`, `305`, `306`, `315`, `316`, and `33432`, DPI tags `282`,
`283`, and `296` when the resolution unit is inches, scalar integer tag `531`,
core scalar integer tags `256` and `257`, scalar layout tags `258`, `259`,
`262`, `273`, `278`, `279`, and `284`, RGB three-value SHORT array tag `258`,
RGBA four-value SHORT array tag `258`, scalar `SamplesPerPixel` tag `277`, and
scalar `ExtraSamples` tag `338`, SHORT-array `YCbCrSubSampling` tag `530`,
numeric `SampleFormat` tag `339`, position rational tags `286`/`287`, scalar
`NewSubfileType` tag `254`, scalar `FillOrder` tag `266`, and scalar
`Thresholding` tag `263`, plus scalar `CellWidth`/`CellLength` tags
`264`/`265`, scalar `SubfileType` tag `255`, scalar
`MinSampleValue`/`MaxSampleValue` tags `280`/`281`, SHORT-array `PageNumber`
tag `297`, scalar `Predictor` tag `317`, ASCII `TargetPrinter` tag `337`,
SHORT-array `HalftoneHints` tag `321`, scalar `InkSet` tag `332`, scalar
`NumberOfInks` tag `334`, SHORT-array `DotRange` tag `336`, scalar
`GrayResponseUnit` tag `290`, ASCII `InkNames` tag `333`, SHORT-array
`TransferFunction` tag `301`, SHORT-array `GrayResponseCurve` tag `291`,
RATIONAL-array `WhitePoint` tag `318`, RATIONAL-array
`PrimaryChromaticities` tag `319`, palette SHORT-array `ColorMap` tag `320`,
RATIONAL-array `YCbCrCoefficients` tag `529`, RATIONAL-array
`ReferenceBlackWhite` tag `532`, SHORT-array `TransferRange` tag `342`,
scalar fax tags `326`/`327`/`328`, scalar fax option tags `292`/`293`, scalar
free block tags `288`/`289`, and scalar `SMinSampleValue` /
`SMaxSampleValue` tags `340`/`341`, plus
UNDEFINED `ICCProfile` tag `34675` when the same tag is present as bounded
TIFF ICC metadata, plus UNDEFINED XMP tag `700` when the same tag is present
as bounded TIFF XMP metadata.
It includes
Orientation `274` only when the stored value is identity `1`. TIFF Orientation
values `2..8` continue to be represented as open-side pixel transforms with
hidden orientation metadata under the existing bounded native TIFF policy.
`pillow_c_image_metadata_tiff_icc_profile` exposes optional ICC profile bytes
already attached to a TIFF image handle. Callers pass `out_has_icc`, an
optional output byte buffer, `out_icc_size`, and `out_icc_required`; missing
metadata returns `out_has_icc == 0`, `out_icc_required == 0`, and success.
Present metadata sets `out_has_icc == 1` and `out_icc_required` to the exact
ICC payload length, returns `-1` for a size probe with a null output buffer,
returns `-2` when the buffer is too small, and copies the ICC bytes on success.
The bounded native TIFF open parser populates this metadata from the selected
IFD's tag `34675` when the tag type is `7` (`UNDEFINED`); covered paths
include the existing RGB TIFF route, native `I`, `F`, and `I;16` early-open
handles, and RGB plus little-endian `I;16`, `I`, and `F` TIFF frame handles
opened through `pillow_c_image_open_tiff_frame`.
TIFF ICC save/writeback, ICC color transforms, non-type-7 ICC tag shapes,
nonzero-frame `I;16B` ICC metadata, compressed numeric frame ICC fixtures,
broader multipage metadata preservation, and arbitrary TIFF UNDEFINED tags are
outside this ABI slice.
`pillow_c_image_exif_orientation` returns the parsed bounded orientation
metadata already attached to an image handle, or `0` when no bounded
orientation tag is present. JPEG/PNG paths attach orientation from EXIF bytes;
TIFF frame `0` currently attaches IFD0 Orientation only for value `1`, matching
the bounded identity-orientation metadata child. TIFF Orientation values `2`
through `8` are handled as open-side pixel transforms and intentionally leave
this metadata value at `0`, matching Pillow's hidden-orientation result for the
bounded fixtures. Broader TIFF tag exposure remains outside this ABI surface.
`pillow_c_exif_orientation_bytes` serializes the bounded Pillow
`Image.Exif().tobytes()` shape used by the facade `Pillow.Image.Exif` object.
Callers pass an orientation integer, optional output byte buffer, output size,
and required-size pointer. Orientation `0` means an empty EXIF object and
requires 20 bytes:
`45 78 69 66 00 00 4d 4d 00 2a 00 00 00 08 00 00 00 00 00 00`.
Orientations `1..65535` require 32 bytes and write a single big-endian TIFF
SHORT tag `0x0112`; for example orientation `3` serializes as
`45 78 69 66 00 00 4d 4d 00 2a 00 00 00 08 00 01 01 12 00 03 00 00 00 01 00 03 00 00 00 00 00 00`.
Passing a null output buffer with size `0` is the size probe and returns `0`
after setting the required size; a null output buffer with nonzero size returns
`-1`. The export returns `-2` when the supplied buffer is too small and `-3`
for negative or larger-than-16-bit orientation values. It does not model full
EXIF dictionaries, nested IFDs, MakerNotes, TIFF tags, XMP, IPTC, or ImageCms.
`pillow_c_exif_entries_bytes` is the generalized native ASCII serializer used
by `META-001C` and `META-001D`. Callers pass an orientation integer plus
parallel arrays of integer ASCII tags and NUL-terminated UTF-8/ASCII value
pointers. The export writes a big-endian TIFF IFD0 inside a Pillow-style
`Exif\0\0` blob, sorts entries by tag like Pillow 11.3.0, stores orientation
as SHORT tag `274`, and stores non-orientation entries as TIFF ASCII type `2`
with NUL terminators and even-byte value padding. Tag `274` must use the
orientation parameter rather than the ASCII arrays. The required-size pattern
matches other metadata exports: a size probe with a null output buffer returns
`-1` after setting the required byte count, a short buffer returns `-2`, and
invalid orientation or tag values return `-3`.
`pillow_c_exif_entries_typed_bytes` extends that serializer for `META-001E`.
After the orientation and ASCII arrays, callers pass parallel integer tag,
unsigned integer value, and TIFF type arrays plus an integer-entry count. The
bounded type values are `3` for SHORT scalar entries and `4` for LONG scalar
entries. The export sorts ASCII, integer, and orientation entries together by
tag, writes SHORT values inline in the first two value bytes, writes LONG
values inline in all four value bytes, and rejects tag `274` in the integer
array because orientation remains the dedicated parameter. Invalid type values,
out-of-range SHORT values, or invalid tag values return `-3`; null arrays for a
nonzero count return `-1`; the output size-probe and short-buffer behavior
matches `pillow_c_exif_entries_bytes`.
`pillow_c_exif_entries_full_bytes` extends the typed serializer for
`META-001F`. After the typed integer arrays, callers pass parallel rational tag,
unsigned numerator, and unsigned denominator arrays plus a rational-entry
count. Each rational entry is serialized as TIFF RATIONAL type `5`, count `1`,
with an out-of-line eight-byte numerator/denominator payload. The export sorts
ASCII, integer, rational, and orientation entries together by tag; rejects tag
`274` in the rational array; rejects zero denominators with `-3`; returns `-1`
for null rational arrays when the rational count is nonzero; and keeps the same
required-size probe and short-buffer behavior as the earlier EXIF serializers.
The existing `pillow_c_exif_entries_typed_bytes` ABI is unchanged and delegates
to the full serializer with zero rational entries.
`pillow_c_exif_entries_short_array_bytes` extends the full serializer for
`META-001G`. After the rational arrays, callers pass parallel SHORT-array tag,
value-offset, and value-count arrays plus one flat unsigned integer value
buffer and its total element count. Each entry is serialized as TIFF SHORT type
`3` with the supplied count; arrays with one or two values are stored inline in
the four-byte IFD value field, and longer arrays are stored out-of-line in the
TIFF payload area. The export sorts ASCII, integer, rational, SHORT-array, and
orientation entries together by tag; rejects tag `274`, zero-length arrays,
values above `65535`, and invalid tag values with `-3`; returns `-1` for null
required arrays when the corresponding count is nonzero; returns `-2` when an
offset/count pair exceeds the flat value buffer; and keeps the same
required-size probe and short-buffer behavior as the earlier EXIF serializers.
The existing `pillow_c_exif_entries_full_bytes` ABI is unchanged and delegates
to the SHORT-array serializer with zero SHORT-array entries.
`pillow_c_exif_entries_byte_array_bytes` extends the SHORT-array serializer for
`META-001H`. After the SHORT-array arrays, callers pass parallel BYTE-array
tag, value-offset, and value-count arrays plus one flat byte value buffer and
its total byte count. Each entry is serialized as TIFF BYTE type `1` with the
supplied count; arrays with one to four bytes are stored inline in the IFD
value field, and longer arrays are stored out-of-line in the TIFF payload area.
Out-of-line BYTE payloads with an odd supplied count reserve and emit one zero
padding byte after the caller bytes so subsequent TIFF payload offsets remain
even, while the IFD count field remains the supplied byte count.
The export sorts ASCII, integer, rational, SHORT-array, BYTE-array, and
orientation entries together by tag; rejects tag `274`, zero-length arrays,
and invalid tag values with `-3`; returns `-1` for null required arrays when
the corresponding count is nonzero; returns `-2` when an offset/count pair
exceeds the flat value buffer; and keeps the same required-size probe and
short-buffer behavior as the earlier EXIF serializers. The existing
`pillow_c_exif_entries_short_array_bytes` ABI is unchanged and delegates to the
BYTE-array serializer with zero BYTE-array entries.
`pillow_c_exif_entries_signed_rational_bytes` extends the BYTE-array
serializer for `META-001I`. After the BYTE-array arrays, callers pass parallel
SRATIONAL tag, signed 32-bit numerator, and signed 32-bit denominator arrays
plus a signed-rational-entry count. Each entry is serialized as TIFF SRATIONAL
type `10`, count `1`, with an out-of-line eight-byte numerator/denominator
payload. The export sorts ASCII, integer, rational, SHORT-array, BYTE-array,
SRATIONAL, and orientation entries together by tag; rejects tag `274`, zero
denominators, and invalid tag values with `-3`; returns `-1` for null
SRATIONAL arrays when the signed-rational count is nonzero; and keeps the same
required-size probe and short-buffer behavior as the earlier EXIF serializers.
The existing `pillow_c_exif_entries_byte_array_bytes` ABI is unchanged and
delegates to the signed-rational serializer with zero SRATIONAL entries.
`pillow_c_exif_entries_undefined_bytes` extends the SRATIONAL serializer for
`META-001J`. After the SRATIONAL arrays, callers pass parallel UNDEFINED tag,
value-offset, and value-count arrays plus one flat byte value buffer and its
total byte count. Each entry is serialized as TIFF UNDEFINED type `7` with the
supplied count; arrays with one to four bytes are stored inline in the IFD value
field, and longer arrays are stored out-of-line in the TIFF payload area.
Out-of-line UNDEFINED payloads with an odd supplied count reserve and emit one
zero padding byte after the caller bytes so subsequent TIFF payload offsets
remain even, while the IFD count field remains the supplied byte count. The
export sorts ASCII, integer, rational, SHORT-array, BYTE-array, SRATIONAL,
UNDEFINED, and orientation entries together by tag; rejects tag `274`,
zero-length arrays, and invalid tag values with `-3`; returns `-1` for null
required arrays when the corresponding count is nonzero; returns `-2` when an
offset/count pair exceeds the flat value buffer; and keeps the same
required-size probe and short-buffer behavior as the earlier EXIF serializers.
The existing `pillow_c_exif_entries_signed_rational_bytes` ABI is unchanged and
delegates to the UNDEFINED serializer with zero UNDEFINED entries.
`pillow_c_exif_ascii_tag` parses one TIFF ASCII type `2` tag from a
Pillow-style EXIF blob. Callers pass the EXIF bytes, tag integer,
`out_has_tag`, optional output char buffer, output size, and required-size
pointer. Missing or non-ASCII tags return success with `out_has_tag == 0`.
Present tags set `out_has_tag == 1`, report the NUL-inclusive required length,
and copy the value without the EXIF terminator bytes beyond the first NUL.
`pillow_c_exif_uint_tag` parses one TIFF SHORT type `3` or LONG type `4` scalar
tag from a Pillow-style EXIF blob. Callers pass the EXIF bytes, tag integer,
`out_has_tag`, and `out_value`. Missing tags, non-integer tags, or integer
array tags with count other than `1` return success with `out_has_tag == 0`;
present scalar integer tags set `out_has_tag == 1` and return the unsigned
32-bit value. Invalid requested tag values return `-3`.
`pillow_c_exif_rational_tag` parses one TIFF RATIONAL type `5` scalar tag from
a Pillow-style EXIF blob. Callers pass the EXIF bytes, tag integer,
`out_has_tag`, `out_numerator`, and `out_denominator`. Missing tags,
non-rational tags, rational tags with count other than `1`, or value offsets
outside the TIFF payload return success with `out_has_tag == 0`; present
rational tags set `out_has_tag == 1` and return the unsigned 32-bit numerator
and denominator. Invalid requested tag values return `-3`.
`pillow_c_exif_rational_array_tag` parses one TIFF RATIONAL type `5` array tag
from a Pillow-style EXIF blob. Callers pass the EXIF bytes, tag integer,
`out_has_tag`, optional `uint32_t` numerator and denominator output buffers,
output pair capacity, and `out_required_count`. Missing tags, non-RATIONAL
tags, zero-count tags, or value offsets outside the TIFF payload return
success with `out_has_tag == 0`. Present tags set `out_has_tag == 1` and
report the required rational-pair count; null output buffers return `-1`, a
too-small capacity returns `-2`, and valid buffers receive unsigned 32-bit
numerator and denominator values at matching indexes. Invalid requested tag
values return `-3`.
`pillow_c_exif_signed_rational_array_tag` parses one TIFF SRATIONAL type `10`
array tag from a Pillow-style EXIF blob. Callers pass the EXIF bytes, tag
integer, `out_has_tag`, optional `int32_t` numerator and denominator output
buffers, output pair capacity, and `out_required_count`. Missing tags,
non-SRATIONAL tags, zero-count tags, zero denominators, or value ranges outside
the TIFF payload return success with `out_has_tag == 0`. Present tags set
`out_has_tag == 1` and report the required pair count; null output buffers
return `-1`, a too-small capacity returns `-2`, and valid buffers receive
signed 32-bit numerator and denominator values at matching indexes. Invalid
requested tag values return `-3`.
`pillow_c_exif_signed_rational_tag` parses one TIFF SRATIONAL type `10` scalar
tag from a Pillow-style EXIF blob. Callers pass the EXIF bytes, tag integer,
`out_has_tag`, `out_numerator`, and `out_denominator`. Missing tags,
non-SRATIONAL tags, SRATIONAL tags with count other than `1`, or value offsets
outside the TIFF payload return success with `out_has_tag == 0`; present
signed rational tags set `out_has_tag == 1` and return the signed 32-bit
numerator and denominator. Invalid requested tag values return `-3`.
`pillow_c_exif_ushort_array_tag` parses one TIFF SHORT type `3` array tag from
a Pillow-style EXIF blob. Callers pass the EXIF bytes, tag integer,
`out_has_tag`, an optional `uint32_t` output buffer, output element capacity,
and `out_required_count`. Missing tags, non-SHORT tags, zero-count tags, or
value offsets outside the TIFF payload return success with `out_has_tag == 0`.
Present tags set `out_has_tag == 1` and report the required element count; a
null output buffer returns `-1`, a too-small buffer including zero capacity
returns `-2`, and a valid buffer receives the unsigned 16-bit values widened
to `uint32_t`. Invalid requested tag values return `-3`.
`pillow_c_exif_byte_array_tag` parses one TIFF BYTE type `1` array tag from a
Pillow-style EXIF blob. Callers pass the EXIF bytes, tag integer,
`out_has_tag`, an optional byte output buffer, output byte capacity, and
`out_required_count`. Missing tags, non-BYTE tags, zero-count tags, or value
offsets outside the TIFF payload return success with `out_has_tag == 0`.
Present tags set `out_has_tag == 1` and report the required byte count; a null
output buffer returns `-1`, a too-small buffer including zero capacity returns
`-2`, and a valid buffer receives the raw bytes. Invalid requested tag values
return `-3`.
`pillow_c_exif_undefined_tag` parses one TIFF UNDEFINED type `7` array tag from
a Pillow-style EXIF blob. Callers pass the EXIF bytes, tag integer,
`out_has_tag`, an optional byte output buffer, output byte capacity, and
`out_required_count`. Missing tags, non-UNDEFINED tags, zero-count tags, or
value offsets outside the TIFF payload return success with `out_has_tag == 0`.
Present tags set `out_has_tag == 1` and report the required byte count; a null
output buffer returns `-1`, a too-small buffer including zero capacity returns
`-2`, and a valid buffer receives the raw bytes. Invalid requested tag values
return `-3`.
`pillow_c_image_metadata_png_transparency`
exposes optional scalar PNG
transparency for `P` and `L` images with `out_has_transparency` set to `0` or
`1` and `out_transparency` set to the palette index, grayscale sample, or `-1`
when absent. Grayscale `tRNS` reuses this existing export; no new ABI entry was
added for `FMT-PNG-002D`.
`pillow_c_image_metadata_png_transparency_table` exposes optional P-mode PNG
byte-table transparency with the same blob size-probe pattern used by ICC,
EXIF, and JPEG metadata. Missing metadata returns `out_has_transparency == 0`,
`out_transparency_required == 0`, and success. Present metadata sets
`out_has_transparency == 1` and `out_transparency_required` to the exact table
byte length, returns `-1` for a size probe with a null output buffer, returns
`-2` when the buffer is too small, and copies the alpha table on success.
`pillow_c_image_metadata_png_rgb_transparency` exposes optional RGB `tRNS`
metadata with `out_has_transparency` set to `0` or `1`; when present, `out_r`,
`out_g`, and `out_b` contain the transparent RGB tuple, and missing metadata
returns `-1` for each component.

The covered text slices match Pillow 11.3.0's open behavior for `tEXt` keyword
`Author` with value `Ada`, uncompressed `iTXt` keyword `Comment` with value
`Hello UTF8`, and bounded `zTXt` keyword `Note` with value
`Compressed hello`, plus bounded compressed `iTXt` keyword
`CompressedComment` with value `Zip hello UTF8`, plus dynamic-Huffman
compressed text keywords `DynamicNote` and `DynamicComment` whose values are
`"Compressed hello dynamic huffman "` repeated five times, plus duplicate
`tEXt` keyword `Author` where the facade exposes the final duplicate value
through `Info["Author"]` and `Text["Author"]`. Save-side text coverage currently
matches Pillow 11.3.0 `PngInfo.add_text(...)` fixtures: native save writes a
single uncompressed `tEXt` payload `Author\0Ada` before `IDAT`, duplicate
uncompressed entries write ordered `tEXt` payloads `Author\0Ada` then
`Author\0Grace` before `IDAT`, and one compressed entry writes a `zTXt`
payload prefix `Note\0\0` followed by a zlib stream that inflates to
`Compressed hello`. `FMT-PNG-001AC` extends the same NUL-terminated text value
ABI to NUL-free Latin-1 byte values: `bytes([99,97,102,233])` writes raw
`tEXt` bytes `caf\xe9`, `zip=True` writes compressed `zTXt`, and native reopen
converts `tEXt`/`zTXt` Latin-1 metadata to UTF-8 before exposing it through
`pillow_c_image_metadata_png_text`. `FMT-PNG-001AD` adds the value-size
text-entry ABI for embedded-NUL byte values:
`pillow_c_image_save_png_text_entries_value_sizes_options` receives parallel
key pointer, value pointer, value-size, and compression arrays, writes exact
`tEXt` value bytes or compressed `zTXt` value bytes for entries such as
`[97,0,98]`, and lets reopened metadata flow through the same indexed metadata
export with explicit byte-count decoding in the facade.
Save-side international text coverage matches
`PngInfo.add_itxt(...)` fixtures: native save writes an uncompressed `iTXt`
payload prefix `Comment\0\0\0\0\0` followed by UTF-8 value bytes
`[67,97,102,101,32,226,152,131]` before `IDAT`, and writes a compressed
`iTXt` payload for keyword `CompressedComment` with compression flag `1`,
compression method `0`, empty language tag, empty translated keyword, and a
stored-zlib stream that inflates to `Zip hello UTF8`. The language-keyed
fixture writes `Comment\0\0\0fr\0Commentaire\0Bonjour UTF8` for
`PngInfo.add_itxt("Comment", "Bonjour UTF8", "fr", "Commentaire")`. Native
reopen surfaces those chunks through the existing PNG text metadata ABI, and
the facade exposes duplicate keys with final-value semantics. The current
compressed text inflater covers stored, fixed-Huffman, and dynamic-Huffman
zlib/deflate payload shapes used by the bounded PNG text fixtures; this is PNG
metadata coverage, not a general compression API.
Save-side ICC coverage matches one Pillow 11.3.0 `icc_profile` fixture:
native save writes an `iCCP` payload with keyword `ICC Profile`, compression
method `0`, and a stored-zlib stream that inflates to the caller-provided
profile bytes before `IDAT`. Native reopen surfaces the decompressed bytes
through `pillow_c_image_metadata_png_icc_profile`, and the facade exposes them
as `Info["icc_profile"]`. The bounded combined `pnginfo` plus `icc_profile`
save fixture writes chunk order `IHDR`, `iCCP`, `tEXt`, `IDAT`, `IEND` and
reopens with both text metadata and caller ICC bytes intact.
Save-side EXIF coverage matches one Pillow 11.3.0 `exif` fixture: native save
accepts `Exif\0\0`-prefixed bytes from `Image.Exif().tobytes()`, writes an
`eXIf` payload containing only the TIFF payload before `IDAT`, and native
reopen restores the `Exif\0\0` header. The bounded fixture stores orientation
tag `6`, which is also reflected through the existing EXIF orientation ABI.
The facade exposes reopened bytes as `Info["exif"]`. The bounded combined
`pnginfo` plus `exif` save fixture writes chunk order `IHDR`, `tEXt`, `eXIf`,
`IDAT`, `IEND` and reopens with both text metadata and Pillow-style EXIF bytes
intact. The bounded combined `icc_profile` plus `exif` save fixture writes
chunk order `IHDR`, `iCCP`, `eXIf`, `IDAT`, `IEND` and reopens with both
decompressed ICC profile bytes and Pillow-style EXIF bytes intact.
The covered transparency slices match P-mode `tRNS` open, scalar P-mode
`Image.save(..., transparency=1)`, P-mode byte-table
`Image.save(..., transparency=bytes([255,128,0,64]))`, RGB truecolor `tRNS`
open, grayscale `tRNS` open, scalar L-mode
`Image.save(..., transparency=10)`, and RGB tuple
`Image.save(..., transparency=(10,20,30))` fixtures: P-mode scalar and
grayscale metadata are exposed as scalar `Info["transparency"]` values, P-mode
table metadata is exposed as `Info["transparency"]` bytes, while RGB metadata
is exposed as `Info["transparency"] == [10, 20, 30]` for the bounded fixtures.
Native `P -> RGBA` conversion uses stored palette alpha metadata, native
`L -> RGBA` conversion sets alpha `0` for pixels matching the stored grayscale
sample, and native `RGB -> RGBA` conversion sets alpha `0` for pixels matching
the stored RGB transparency tuple and `255` otherwise.

Native save writes valid PNG files from supported modes after any required
RGB/BGR channel packing inside C++; `LA` and `P` default saves use native PNG
chunk writing so they reopen with Pillow-style mode and palette semantics.
`pillow_c_image_save_png_compress_level` accepts Pillow-style `-1` default plus
`0..9`; level `0` writes native stored zlib output for supported modes, and
level `1` routes through the native PNG chunk writer with an `IDAT` zlib
header `[0x78,0x01]`, matching the bounded Pillow 11.3.0
`Image.save(..., compress_level=1)` RGB fixture's zlib header. Levels `2..9`
currently reuse the existing encoder path unless another option forces the
custom writer. When a covered custom-writer route receives an explicit level,
the native zlib header selector matches Pillow's bounded header classes:
levels `0..1` use `[0x78,0x01]`, levels `2..5` use `[0x78,0x5E]`, level `6`
uses `[0x78,0x9C]`, and levels `7..9` use `[0x78,0xDA]`.
`pillow_c_image_save_png_options` is the extensible PNG save-options ABI used
by the facade for `compress_level` and `dpi`; positive DPI values are converted
to PNG `pHYs` pixels-per-meter values with Pillow's rounding formula, and
invalid or partial DPI values return `-3`. The same bounded
`compress_level=1` custom-writer route is used by this options export.
`pillow_c_image_save_png_transparency_options` preserves those arguments and
appends `has_transparency` plus `transparency`; `has_transparency` must be `0`
or `1`. Scalar transparency is currently supported for P-mode indexes in the
attached palette and for L-mode grayscale samples in `0..255`. For P-mode
saves, the native PNG chunk writer emits a `tRNS` chunk immediately after
`PLTE`, using opaque alpha for preceding palette entries and `0` at the
requested index, matching the covered Pillow 11.3.0 `[255,0]` payload for
`transparency=1`. For L-mode saves, it emits a two-byte big-endian grayscale
sample payload before `IDAT`, matching the covered Pillow 11.3.0 `[0,10]`
payload for `transparency=10`.
`pillow_c_image_save_png_transparency_table_options` preserves the same PNG
options arguments and appends `const uint8_t* transparency_table` plus
`size_t transparency_table_size`; the pointer must be non-null when the size is
nonzero, the source image must be mode `P`, and the table must contain `1..256`
bytes. The native PNG chunk writer emits a `tRNS` chunk immediately after
`PLTE`, truncating the table to the attached palette size like Pillow. The
bounded covered fixture writes payload `[255,128,0,64]`, reopens with the same
byte-table metadata, and applies those alpha values during native
`P -> RGBA` conversion.
`pillow_c_image_save_png_rgb_transparency_options` preserves the same PNG
options arguments and appends `has_transparency` plus `r`, `g`, and `b` channel
values. `has_transparency` must be `0` or `1`; when present, channel values must
be in `0..255` and the source image must be mode `RGB`. The native PNG chunk
writer emits a six-byte big-endian truecolor `tRNS` payload before `IDAT`,
matching the covered Pillow 11.3.0 `[0,10,0,20,0,30]` payload for
`transparency=(10,20,30)`.
`pillow_c_image_save_png_rgb_transparency_bytes_options` preserves the same PNG
options arguments and appends `const uint8_t* transparency` plus
`size_t transparency_size`; the pointer must be non-null, the size must be
exactly `3`, and the source image must be mode `RGB`. The export treats those
three bytes as `(r,g,b)`, reuses the truecolor `tRNS` writer, and matches
Pillow 11.3.0 `transparency=bytes([10,20,30])` behavior for the bounded RGB
fixture.
`pillow_c_image_save_png_text_options` preserves the same PNG options arguments
and appends `key` plus `value` NUL-terminated strings for one uncompressed
`tEXt` chunk. Keys remain validated as bounded PNG text keywords; `tEXt` and
`zTXt` values are interpreted as Latin-1 bytes, while `iTXt` values remain
UTF-8. `pillow_c_image_save_png_text_entries_options`
preserves the same PNG options arguments and appends `const char* const* keys`,
`const char* const* values`, and `size_t text_count` for one or more ordered
uncompressed `tEXt` chunks. `pillow_c_image_save_png_text_entries_ex_options`
preserves the same PNG options arguments and appends `const char* const* keys`,
`const char* const* values`, `const int* compressed`, and `size_t text_count`;
entries with compressed flag `0` emit ordered `tEXt` chunks, and entries with
compressed flag `1` emit ordered `zTXt` chunks with compression method `0` and
stored-zlib payloads. `pillow_c_image_save_png_text_entries_kind_options`
preserves the same PNG options arguments and appends
`const char* const* keys`, `const char* const* values`, `const int* kinds`,
`const int* compressed`, and `size_t text_count`; kind `0` keeps the existing
`tEXt`/`zTXt` behavior, and kind `1` emits `iTXt` chunks with compression
method `0`, empty language tag, and empty translated keyword. For kind `1`,
compressed flag `0` writes raw UTF-8 text and compressed flag `1` writes a
stored-zlib text payload with the `iTXt` compression flag set to `1`.
`pillow_c_image_save_png_text_entries_itxt_options` preserves that argument
list and appends `const char* const* langs` plus
`const char* const* translated_keys` before `size_t text_count`; kind `1`
emits `iTXt` chunks with caller-provided ASCII language tags and UTF-8
translated-keyword bytes, while kind `0` requires empty language and translated
keyword strings.
`pillow_c_image_save_png_text_entries_icc_options` preserves the
`text_entries_kind_options` argument list and appends
`const uint8_t* icc_profile` plus `size_t icc_profile_size`; it requires a
nonzero text count, non-null text arrays, a non-null ICC pointer, and nonzero
ICC size. It emits `iCCP` before the ordered text chunks and before `IDAT`,
matching the bounded Pillow 11.3.0 `pnginfo` plus `icc_profile` fixture.
`pillow_c_image_save_png_text_entries_exif_options` preserves the
`text_entries_kind_options` argument list and appends `const uint8_t* exif`
plus `size_t exif_size`; it requires a nonzero text count, non-null text
arrays, a non-null EXIF pointer, nonzero EXIF size, and `Exif\0\0`-prefixed
bytes. It emits ordered text chunks before `eXIf` and before `IDAT`, matching
the bounded Pillow 11.3.0 `pnginfo` plus `exif` fixture.
`pillow_c_image_save_png_text_entries_icc_exif_options` preserves the
`text_entries_kind_options` argument list and appends
`const uint8_t* icc_profile`, `size_t icc_profile_size`,
`const uint8_t* exif`, and `size_t exif_size`; it requires a nonzero text
count, non-null text arrays, non-null ICC and EXIF pointers, nonzero ICC and
EXIF sizes, and `Exif\0\0`-prefixed EXIF bytes. It emits `iCCP` before the
ordered text chunks, emits text before `eXIf`, and emits all three metadata
groups before `IDAT`, matching the bounded Pillow 11.3.0 chunk order `IHDR`,
`iCCP`, text, `eXIf`, `IDAT`, `IEND`.
The current bounded writer accepts ASCII keywords, requires
each non-empty keyword to be at most 79 bytes, accepts UTF-8 value bytes for
kind `1`, emits text chunks before `IDAT` in caller order, returns `-2` for
zero text count on the multi-entry exports, and otherwise returns `-3` for
invalid text, kind, compression flags, or non-ASCII language tags. Non-ASCII
`tEXt`/`zTXt` values and broad metadata combinations remain future text
metadata surfaces for these older text-entry-only exports. Compressed
language-keyed `iTXt` is covered through the generalized PNG metadata/custom
chunk route, not through this legacy text-only route.
`pillow_c_image_save_png_icc_options` preserves the same PNG options arguments
and appends `const uint8_t* icc_profile` plus `size_t icc_profile_size`;
the profile pointer must be non-null and the size must be nonzero. The native
PNG chunk writer emits the `iCCP` chunk before `IDAT` with keyword
`ICC Profile`, compression method `0`, and stored-zlib compressed profile
bytes. `pillow_c_image_save_png_exif_options` preserves the same PNG options
arguments and appends `const uint8_t* exif` plus `size_t exif_size`; the EXIF
pointer must be non-null, the size must be nonzero, and the bytes must start
with `Exif\0\0`. The native PNG chunk writer emits the `eXIf` chunk before
`IDAT` with only the TIFF payload after stripping the header.
`pillow_c_image_save_png_icc_exif_options` preserves the same PNG options
arguments and appends `const uint8_t* icc_profile`, `size_t icc_profile_size`,
`const uint8_t* exif`, and `size_t exif_size`; both pointers must be non-null,
both sizes must be nonzero, and the EXIF bytes must start with `Exif\0\0`.
The native PNG chunk writer emits `iCCP` before `eXIf` and before `IDAT`,
matching the covered Pillow 11.3.0 combined ICC plus EXIF fixture.
`pillow_c_image_save_png_interlace_options` preserves the same PNG options
arguments and appends `int interlace`; the bounded Pillow-compatible behavior
accepts the value as a no-op and writes a non-interlaced PNG with IHDR
interlace byte `0` through the native custom PNG writer.
`pillow_c_image_save_png_gamma_options` preserves the same PNG options
arguments and appends `double gamma`; the bounded Pillow-compatible behavior
accepts the value as a no-op and writes a PNG without a `gAMA` chunk through
the native custom PNG writer.
`pillow_c_image_save_png_gama_options` preserves the same PNG options arguments
and appends `uint32_t gama_raw`; the bounded explicit `pnginfo` behavior writes
a four-byte big-endian `gAMA` chunk before `IDAT`, matching local Pillow 11.3.0
`PngInfo.add(b"gAMA", struct.pack(">I", 45455))` for the covered RGB fixture.
This is separate from the save-side `gamma` option, which remains a no-op.
`pillow_c_image_save_png_chunk_options` preserves the same leading PNG options
arguments and appends a four-byte chunk type pointer, a chunk data pointer, and
a data length. The current bounded route accepts fixed-size public metadata
chunks `gAMA` with four bytes, `sRGB` with one byte, `cHRM` with thirty-two
bytes, RGB `bKGD` with six bytes, P-mode `bKGD` with one byte, and P-mode
`hIST` with exactly two bytes per palette entry, standard `tIME` with exactly
seven bytes, RGB `sBIT` with exactly three bytes, and standard `cICP` with
exactly four bytes, plus Pillow-style private chunk types whose second type
byte is lowercase. The covered private fixture writes `vpAg` before `IDAT`,
reopens with preserved RGB bytes, and exposes no
unknown private chunk through PNG text/info metadata. RGB `bKGD` is emitted
before `IDAT`, and `tIME`/`sBIT`/`cICP` are emitted after `IHDR` and before
`IDAT`. P-mode `bKGD` and `hIST` are deferred until after `PLTE`, and after
`tRNS` when palette transparency is present, then emitted before `IDAT`.
Private chunks may also carry a zero-length payload; the writer emits the
zero-length chunk and only requires a non-null data pointer when the payload
length is nonzero. Unsupported public/standard chunk types, wrong fixed
payload sizes for `gAMA`/`sRGB`/`cHRM`/`bKGD`, wrong palette-derived `hIST`
payload sizes, wrong `tIME`/`sBIT`/`cICP` payload sizes, `after_idat` on this
pre-`IDAT` route, standalone multiple private chunks on this single-chunk route,
and metadata/transparency
combinations remain explicit boundaries. Standalone multiple private custom
chunks use
`pillow_c_image_save_png_custom_chunks_options`.
`pillow_c_image_save_png_text_entries_chunk_options` preserves the same leading
PNG options arguments and appends `const char* const* keys`,
`const char* const* values`, `size_t text_count`, then a four-byte chunk type
pointer, a chunk data pointer, and a data length. It requires non-null text
arrays, nonzero text count, and one supported pre-`IDAT` chunk. The
bounded covered fixture combines `cHRM` with one ordinary `tEXt` entry and
writes chunk order `IHDR`, `cHRM`, `tEXt`, `IDAT`, `IEND`, preserving RGB
bytes and reopening with both chromaticity and text metadata. Compressed text,
`iTXt`, ICC/EXIF, transparency, `after_idat`, multiple custom chunks mixed
with text metadata, and public/standard arbitrary chunk rules remain future ABI
surfaces.
`pillow_c_image_save_png_text_entries_chunk_rgb_transparency_options`
preserves the same leading PNG options arguments and appends the same ordered
text arrays, bounded chunk type/data pair, then `has_transparency`, `r`, `g`,
and `b`. It requires non-null text arrays, nonzero text count, one supported
pre-`IDAT` chunk, `has_transparency` set to `0` or `1`, and source mode
`RGB` when transparency is present. The bounded covered fixture combines
`cHRM`, one ordinary `tEXt` entry, and RGB tuple transparency, writing chunk
order `IHDR`, `cHRM`, `tEXt`, `tRNS`, `IDAT`, `IEND`, preserving RGB bytes and
reopening with chromaticity, text, and RGB transparency metadata. Compressed
text, `iTXt`, ICC/EXIF, RGB bytes-valued transparency with text, `after_idat`,
multiple custom chunks mixed with text/transparency, and public/standard
arbitrary chunk rules remain future ABI surfaces for this route.
`pillow_c_image_save_png_chunk_icc_options` preserves the same leading PNG
options arguments and appends a four-byte chunk type pointer, a chunk data
pointer and length, then `const uint8_t* icc_profile` plus
`size_t icc_profile_size`. It requires one supported pre-`IDAT` chunk and
non-null, nonempty ICC bytes. The bounded covered fixture combines `cHRM` with
ICC metadata and writes chunk order `IHDR`, `iCCP`, `cHRM`, `IDAT`, `IEND`,
preserving RGB bytes and reopening with both chromaticity and decompressed ICC
metadata. EXIF, text, `iTXt`, transparency, `after_idat`, multiple custom
chunks, and public/standard arbitrary chunk rules remain future ABI surfaces
for this route.
`pillow_c_image_save_png_chunk_exif_options` preserves the same leading PNG
options arguments and appends a four-byte chunk type pointer, a chunk data
pointer and length, then `const uint8_t* exif` plus `size_t exif_size`. It
requires one supported pre-`IDAT` chunk and non-null, nonempty Pillow-style
EXIF bytes beginning with `Exif\0\0`. The bounded covered fixture combines
`cHRM` with EXIF metadata and writes chunk order `IHDR`, `cHRM`, `eXIf`,
`IDAT`, `IEND`, preserving RGB bytes and reopening with both chromaticity and
restored Pillow-style EXIF metadata. Text, `iTXt`, transparency, `after_idat`,
multiple custom chunks mixed with EXIF metadata, and public/standard arbitrary
chunk rules remain future ABI surfaces for this route.
`pillow_c_image_save_png_chunk_rgb_transparency_options` preserves the same
leading PNG options arguments and appends a four-byte chunk type pointer, a
chunk data pointer and length, then `has_transparency`, `r`, `g`, and `b`.
It requires one supported pre-`IDAT` chunk, `has_transparency` set to `0`
or `1`, and source mode `RGB` when transparency is present. The bounded
covered fixture combines `cHRM` with RGB tuple transparency and writes chunk
order `IHDR`, `cHRM`, `tRNS`, `IDAT`, `IEND`, preserving RGB bytes and
reopening with both chromaticity and RGB transparency metadata. Text, `iTXt`,
ICC/EXIF, `after_idat`, multiple custom chunks mixed with transparency, and
public/standard arbitrary chunk rules remain future ABI surfaces for this
route.
`pillow_c_image_save_png_chunk_rgb_transparency_bytes_options` preserves the
same leading PNG options and bounded chunk arguments, then appends
`const uint8_t* transparency` plus `size_t transparency_size`. It requires
exactly three RGB transparency bytes and reuses the native `cHRM` plus RGB
`tRNS` writer. The bounded covered fixture combines `cHRM` with RGB
bytes-valued transparency and writes chunk order `IHDR`, `cHRM`, `tRNS`,
`IDAT`, `IEND`, preserving RGB bytes and reopening with both chromaticity and
RGB transparency metadata. Text, `iTXt`, ICC/EXIF, `after_idat`, multiple
custom chunks, and public/standard arbitrary chunk rules remain future ABI
surfaces for this route.
`pillow_c_image_save_png_optimize_options` preserves the same PNG options
arguments and appends `int optimize`. Nonzero `optimize` writes through the
native custom PNG writer with an `IDAT` zlib header `[0x78,0xDA]`, matching the
bounded Pillow 11.3.0 `Image.save(..., optimize=True)` RGB fixture's zlib
header, while reopening preserves RGB bytes and exposes no
`Info["optimize"]`. `optimize == 0` delegates to the ordinary PNG options path.
This is not full PNG compression-strategy parity; exact compressed bytes,
compression search behavior, all-level deflate-size parity, and optimize
combinations remain future work.
`pillow_c_image_save_png_metadata_options` is the generalized PNG metadata
save route for covered bounded surfaces. It preserves the same leading PNG
options arguments, then accepts ordered text key/value/kind/compression/lang
arrays, optional ICC profile and EXIF buffers, one supported pre-`IDAT` chunk
type/data pair, a metadata `flags` word, optional raw `gAMA`, scalar
transparency, optional P-mode transparency table bytes, and optional RGB
transparency channel values. The currently defined flags are `0x01` for an
explicit `gAMA` chunk, `0x02` for placing the supported custom chunk after ICC,
`0x04` for text before EXIF ordering, `0x08` for the bounded optimize zlib
header path, `0x10` for scalar transparency, `0x20` for RGB transparency, and
`0x40` for placing a supported private custom chunk after `IDAT` and before
`IEND`. Unknown flag bits, mutually exclusive transparency encodings,
null pointers with nonzero sizes, and zero-sized non-null metadata buffers
other than custom chunk payloads return invalid-argument or invalid-length
status codes. The route reuses the
native PNG chunk planner/writer and is the preferred ABI for future covered
PNG metadata combinations; older narrow PNG metadata exports remain available
for ABI stability. `FMT-PNG-004B` did not add a new export; it uses this route
to write `icc_profile` plus RGB tuple transparency as `IHDR`, `iCCP`, `tRNS`,
`IDAT`, `IEND`, reopening with both ICC bytes and RGB transparency metadata.
`FMT-PNG-004C` also uses this route without adding an export; it writes `exif`
plus RGB tuple transparency as `IHDR`, `tRNS`, `eXIf`, `IDAT`, `IEND`, strips
the stored `eXIf` payload's leading `Exif\0\0` header, and reopens with both
Pillow-style EXIF bytes and RGB transparency metadata.
`FMT-PNG-004D` uses the same route without adding an export to combine
`icc_profile`, `exif`, and RGB tuple transparency as `IHDR`, `iCCP`, `tRNS`,
`eXIf`, `IDAT`, `IEND`, reopening with ICC bytes, Pillow-style EXIF bytes,
and RGB transparency metadata. This slice required only facade routing because
the generalized native writer already produced the covered Pillow-compatible
chunk order.
`FMT-PNG-004E` uses the same route without adding an export to combine ordinary
`PngInfo.add_text("Author", "Ada")` text and RGB tuple transparency as `IHDR`,
`tEXt`, `tRNS`, `IDAT`, `IEND`, reopening with text and RGB transparency
metadata. This slice required only facade routing because the generalized
native writer already produced the covered Pillow-compatible chunk order.
`FMT-PNG-004F` uses the same route without adding an export to combine
compressed `PngInfo.add_text("Note", "Compressed hello", zip=True)` text and
RGB tuple transparency as `IHDR`, `zTXt`, `tRNS`, `IDAT`, `IEND`, reopening
with text and RGB transparency metadata. This slice also required only facade
routing because the generalized native writer already honored per-entry
compression flags and produced the covered Pillow-compatible chunk order.
`FMT-PNG-004G` uses the same route without adding an export to combine
uncompressed `PngInfo.add_itxt("Comment", "Cafe " Chr(0x2603))` text and RGB
tuple transparency as `IHDR`, `iTXt`, `tRNS`, `IDAT`, `IEND`, reopening with
text and RGB transparency metadata. This slice required only facade routing
because the generalized native writer already honored text kind `1` entries
and produced the covered Pillow-compatible chunk order.
`FMT-PNG-004H` uses the same route without adding an export to combine
compressed `PngInfo.add_itxt("CompressedComment", "Zip hello UTF8", "", "",
true)` text and RGB tuple transparency as `IHDR`, `iTXt`, `tRNS`, `IDAT`,
`IEND`, reopening with text and RGB transparency metadata. This slice required
only facade routing because the generalized native writer already honored
per-entry compression flags for text kind `1`.
`FMT-PNG-004I` uses the same route without adding an export to combine
language-keyed `PngInfo.add_itxt("Comment", "Bonjour UTF8", "fr",
"Commentaire")` text and RGB tuple transparency as `IHDR`, `iTXt`, `tRNS`,
`IDAT`, `IEND`, reopening with text and RGB transparency metadata. This slice
required only facade routing because the generalized native writer already
honored language and translated-keyword pointer arrays.
`FMT-PNG-004J` uses the same route without adding an export to combine
ordinary `PngInfo.add_text("Author", "Ada")` text and RGB byte-form
transparency as `IHDR`, `tEXt`, `tRNS`, `IDAT`, `IEND`, reopening with text
and RGB transparency metadata. This slice required only facade byte-form
normalization because the ABI already carries RGB transparency channel values.
`FMT-PNG-004K` uses the same route without adding an export to batch the
remaining same-route RGB byte-form transparency tails for ICC, EXIF, ICC+EXIF,
compressed `tEXt`, uncompressed `iTXt`, compressed `iTXt`, and language-keyed
`iTXt` metadata groups. This also required only facade byte-form
normalization because the ABI already carries RGB transparency channel values.
`FMT-PNG-004AS` uses the same route without adding an export to combine
language-keyed `PngInfo.add_itxt("Comment", "Bonjour UTF8", "fr",
"Commentaire")`, `icc_profile`, `exif`, and RGB tuple transparency as `IHDR`,
`iCCP`, `iTXt`, `tRNS`, `eXIf`, `IDAT`, `IEND`, reopening with text, ICC,
Pillow-style EXIF, and RGB transparency metadata. This required only facade
guard narrowing because the generalized native writer already produced the
covered Pillow-compatible chunk order.
`FMT-PNG-004AT` uses the same route without adding an export to batch
no-custom advanced text kinds (`zTXt`, plain `iTXt`, compressed `iTXt`, and
language-keyed `iTXt`) with `icc_profile`, `exif`, RGB tuple transparency, and
`optimize=True` as `IHDR`, `iCCP`, selected text chunk, `tRNS`, `eXIf`,
optimized `IDAT`, `IEND`, reopening with text, ICC, Pillow-style EXIF, RGB
transparency metadata, and native `RGB -> RGBA` transparency conversion. This
required only facade optimize-guard narrowing because the generalized native
writer already produced the covered Pillow-compatible chunk order and zlib
header marker `[0x78,0xDA]`.
`FMT-PNG-004L` uses the same route without adding an export to batch
`optimize=True` with ordinary metadata groups: ordinary `tEXt`, ICC, EXIF,
ICC+EXIF, and ordinary `tEXt` plus ICC plus EXIF. The existing
`PNG_METADATA_OPTIMIZE` flag writes the bounded optimized IDAT zlib header
`[0x78,0xDA]` while preserving the covered metadata chunk orders. Scalar/table
transparency, optimize with transparency, optimize with custom chunks,
optimize with compressed text or `iTXt`, broader partial alpha table public
behavior, full `getexif()` object behavior, and broader
text/transparency/metadata preservation remain future PNG metadata surfaces.
`FMT-PNG-004M` uses the same route without adding an export to batch
`compress_level=9` with ordinary metadata groups: ordinary `tEXt`, ICC, EXIF,
ICC+EXIF, and ordinary `tEXt` plus ICC plus EXIF. The existing leading
`compress_level` ABI argument now reaches the native PNG custom writer for
this generalized route, so the bounded high-compression metadata fixtures
write IDAT zlib header `[0x78,0xDA]` while preserving the covered metadata
chunk orders. Exact compressed byte/size parity and full deflate strategy
parity remain future PNG compression surfaces.
`FMT-PNG-004N` uses the same route without adding an export to batch one
explicit safe `cHRM` custom chunk plus advanced text-kind entries: compressed
`tEXt`, uncompressed `iTXt`, compressed `iTXt`, and language-keyed `iTXt`.
The existing chunk type/data pointers and text kind/compression/lang arrays
write `cHRM` before the selected text chunk and before `IDAT`, while reopened
PNG handles expose chromaticity and text metadata through the existing PNG
metadata exports.
`FMT-PNG-004O` uses the same route without adding an export to batch
`optimize=True` plus explicit safe `cHRM` and those same advanced text-kind
entries. The existing `PNG_METADATA_OPTIMIZE` flag writes IDAT zlib header
`[0x78,0xDA]` while preserving `cHRM` before the selected text chunk and before
`IDAT`.
`FMT-PNG-004Q` uses the same route without adding an export to batch explicit
safe `cHRM` plus advanced text-kind entries and RGB tuple transparency. The
existing chunk type/data pointers, text kind/compression/lang arrays, and
`PNG_METADATA_RGB_TRANSPARENCY` flag write `cHRM` before the selected
`zTXt`/`iTXt` chunk, then truecolor `tRNS` before `IDAT`, reopening with
chromaticity, text, and RGB transparency metadata and native RGB-to-RGBA alpha
conversion.
`FMT-PNG-004R` uses the same route without adding an export for the matching
RGB byte-form transparency tail on that custom advanced text family. The facade
normalizes `Buffer(3)` transparency into the existing RGB channel arguments,
so the ABI shape and `PNG_METADATA_RGB_TRANSPARENCY` flag are unchanged.
`FMT-PNG-004S` uses the same route without adding an export to batch explicit
safe `cHRM` plus advanced text-kind entries with ICC and/or EXIF. The existing
chunk type/data pointers, text kind/compression/lang arrays, optional ICC/EXIF
buffers, `PNG_METADATA_CHUNK_AFTER_ICC`, and `PNG_METADATA_TEXT_BEFORE_EXIF`
flags write ICC before `cHRM` when present and text before `eXIf` when present.
The ABI shape is unchanged.
`FMT-PNG-004T` uses the same route without adding an export to batch
`optimize=True` with explicit safe `cHRM` plus advanced text-kind entries and
ICC and/or EXIF. The existing `PNG_METADATA_OPTIMIZE` flag composes with
`PNG_METADATA_CHUNK_AFTER_ICC` and `PNG_METADATA_TEXT_BEFORE_EXIF`, preserving
ICC before `cHRM`, selected text before `eXIf`, and the bounded optimized IDAT
zlib header `[0x78,0xDA]`. The ABI shape is unchanged.
`FMT-PNG-004U` uses the same route without adding an export to batch explicit
safe `cHRM`, advanced text-kind entries, ICC and/or EXIF, and RGB tuple
transparency. The existing chunk type/data pointers, text arrays, optional
ICC/EXIF buffers, `PNG_METADATA_CHUNK_AFTER_ICC`,
`PNG_METADATA_TEXT_BEFORE_EXIF`, and `PNG_METADATA_HAS_RGB_TRANSPARENCY` flags
compose to write ICC before `cHRM`, selected text before `eXIf`, and truecolor
`tRNS` before `IDAT`. The ABI shape is unchanged.
`FMT-PNG-004V` uses the same route without adding an export to batch
`optimize=True` with explicit safe `cHRM`, advanced text-kind entries, ICC
and/or EXIF, and RGB tuple transparency. The existing
`PNG_METADATA_OPTIMIZE`, `PNG_METADATA_CHUNK_AFTER_ICC`,
`PNG_METADATA_TEXT_BEFORE_EXIF`, and `PNG_METADATA_HAS_RGB_TRANSPARENCY` flags
compose to write optimized IDAT header `[0x78,0xDA]`, ICC before `cHRM`,
selected text before `eXIf`, and truecolor `tRNS` before `IDAT`. The ABI shape
is unchanged.
`FMT-PNG-004W` uses the same route without adding an export to batch
`compress_level=6` with explicit safe `cHRM`, advanced text-kind entries, ICC
and/or EXIF, and RGB tuple transparency. The leading `compress_level` argument
already composes with `PNG_METADATA_CHUNK_AFTER_ICC`,
`PNG_METADATA_TEXT_BEFORE_EXIF`, and
`PNG_METADATA_HAS_RGB_TRANSPARENCY`, writing IDAT header `[0x78,0x9C]`, ICC
before `cHRM`, selected text before `eXIf`, and truecolor `tRNS` before
`IDAT`. The ABI shape is unchanged. Other compression strategy details,
interlace/gamma combinations, multiple custom chunks mixed with other metadata,
and public/standard arbitrary chunk rules remain future
PNG metadata surfaces.
`FMT-PNG-004X` changes no signature and adds no export; it expands the existing
pre-`IDAT` chunk validator used by these PNG routes to accept Pillow-style
private chunk types where the second type byte is lowercase, with focused raw
and facade coverage for `PngInfo.add(b"vpAg", bytes([1,2,3,4,255]))`.
`FMT-PNG-004Y` changes no signature and adds no export; it uses the new
`0x40` metadata flag to write one Pillow-style private chunk such as `b"vpAg"`
after `IDAT` and before `IEND` for
`PngInfo.add(b"vpAg", bytes([5,4,3,2,1]), after_idat=True)`. The after-IDAT
path is intentionally limited to private chunk types; mixed metadata and
public/standard arbitrary chunk placement remain future PNG metadata surfaces.
Multiple private custom chunk batches are covered separately by
`FMT-PNG-004AA`.
`FMT-PNG-004Z` changes no signature and adds no export; it expands custom
chunk validation so a present private chunk type may carry
`chunk_data_size == 0` with a null or non-null data pointer. The native writer
emits a zero-length chunk for `PngInfo.add(b"vpAg", b"")` before `IDAT`, and
for the `0x40` after-IDAT route it emits the same zero-length chunk after
`IDAT` and before `IEND`.
`pillow_c_image_save_png_custom_chunks_options` is the narrow batch-private
chunk route added for `FMT-PNG-004AA`. It preserves the leading PNG options
arguments, then accepts `const uint8_t* chunk_types` as contiguous four-byte
chunk type records, `const uint8_t* const* chunk_data`,
`const size_t* chunk_data_sizes`, `const int* chunk_after_idat`, and
`size_t chunk_count`. `chunk_count` must be nonzero, the four array pointers
must be non-null, each `chunk_after_idat` value must be `0` or `1`, and each
chunk type must be a Pillow-style private chunk type whose second byte is
lowercase. Nonzero chunk payload sizes require non-null data pointers; zero
payload sizes are allowed. The native writer emits all pre-`IDAT` private
chunks in caller order before image data, emits all `after_idat` chunks in
caller order after image data and before `IEND`, preserves image bytes, and
does not surface unknown private chunks through PNG text/info metadata on
reopen. Public/standard arbitrary chunk types, open-side unknown chunk
preservation, APNG, and mixed multiple private chunks with text/ICC/EXIF/
transparency remain outside this export.
`pillow_c_image_save_png_text_entries_custom_chunks_options` is the bounded
mixed text plus batch-private chunk route added for `FMT-PNG-004AJ`. It
preserves the leading PNG options arguments, then accepts
`const char* const* keys`, `const char* const* values`, and `size_t text_count`
for ordinary `tEXt` entries, followed by the same contiguous four-byte
`chunk_types`, `chunk_data`, `chunk_data_sizes`, `chunk_after_idat`, and
`chunk_count` arrays used by `pillow_c_image_save_png_custom_chunks_options`.
Both counts must be nonzero; required arrays must be non-null; each
`chunk_after_idat` value must be `0` or `1`; chunk types must be Pillow-style
private chunk types whose second byte is lowercase; nonzero payload sizes
require non-null data pointers; zero payload sizes are allowed. The writer
matches the covered Pillow 11.3.0 order for the bounded fixture:
`IHDR`, ordinary `tEXt`, all pre-`IDAT` private chunks in caller order, `IDAT`,
all `after_idat` private chunks in caller order, `IEND`. Compressed text,
`iTXt`, ICC, EXIF, transparency, APNG, and public/standard arbitrary chunk
combinations remain separate future surfaces.
`pillow_c_image_save_png_text_entries_custom_chunks_kind_options` extends that
batch-private route for `FMT-PNG-004AK`. It preserves the leading PNG options
arguments, then accepts ordered text arrays
`const char* const* keys`, `const char* const* values`, `const int* kinds`,
`const int* compressed`, `const char* const* langs`,
`const char* const* translated_keys`, and `size_t text_count`, followed by the
same contiguous four-byte `chunk_types`, `chunk_data`, `chunk_data_sizes`,
`chunk_after_idat`, and `chunk_count` arrays used by
`pillow_c_image_save_png_custom_chunks_options`. Both counts must be nonzero;
required arrays must be non-null; each `chunk_after_idat` value must be `0` or
`1`; chunk types must be Pillow-style private chunk types whose second byte is
lowercase; nonzero payload sizes require non-null data pointers; zero payload
sizes are allowed. Text kind `0` writes `tEXt` or compressed `zTXt`; text kind
`1` writes uncompressed or compressed `iTXt`, using the provided language and
translated-keyword arrays. The covered fixtures write `IHDR`, selected text
chunk, all pre-`IDAT` private chunks in caller order, `IDAT`, all
after-`IDAT` private chunks in caller order, `IEND`, preserve image bytes, and
reopen with selected text metadata while hiding unknown private chunks. ICC,
EXIF, transparency, APNG, public/standard arbitrary chunk combinations, and
compressed language-keyed `iTXt` remain separate future surfaces.
`pillow_c_image_save_png_metadata_custom_chunks_options` generalizes the
covered PNG metadata route for `FMT-PNG-004AL` by replacing the single custom
chunk type/data pair with the batch-private chunk arrays from
`pillow_c_image_save_png_custom_chunks_options`. It preserves the leading PNG
options arguments, then accepts ordered text arrays
`const char* const* keys`, `const char* const* values`, `const int* kinds`,
`const int* compressed`, `const char* const* langs`,
`const char* const* translated_keys`, and `size_t text_count`, optional
`icc_profile` and `exif` buffers, contiguous four-byte `chunk_types`,
`chunk_data`, `chunk_data_sizes`, `chunk_after_idat`, `chunk_count`, then the
metadata `flags`, raw `gAMA`, scalar transparency, optional P-mode
transparency table, and RGB transparency channel values. The batch route
requires `chunk_count > 0` and non-null chunk arrays; each private chunk type
must be Pillow-style private with a lowercase second byte; each
`chunk_after_idat` entry must be `0` or `1`; nonzero payload sizes require a
non-null data pointer; zero-length payloads are allowed. It supports the
already-covered metadata option flags for text, ICC/EXIF, gAMA, optimize, and
transparency, but rejects the single-chunk placement flags
`PNG_METADATA_CHUNK_AFTER_ICC` and `PNG_METADATA_CHUNK_AFTER_IDAT`; after-IDAT
placement is per private chunk through `chunk_after_idat`. The first
facade-proven routes write ordinary text plus ICC plus multiple private chunks
as `IHDR`, `iCCP`, `tEXt`, all pre-`IDAT` private chunks in caller order,
`IDAT`, all after-`IDAT` private chunks in caller order, `IEND`; ordinary text
plus EXIF plus multiple private chunks as `IHDR`, `tEXt`, all pre-`IDAT`
private chunks in caller order, `eXIf`, `IDAT`, all after-`IDAT` private
chunks in caller order, `IEND`; and ordinary text plus ICC plus EXIF plus
multiple private chunks as `IHDR`, `iCCP`, `tEXt`, all pre-`IDAT` private
chunks in caller order, `eXIf`, `IDAT`, all after-`IDAT` private chunks in
caller order, `IEND`; and advanced `zTXt`, uncompressed `iTXt`, compressed
`iTXt`, or uncompressed language-keyed `iTXt` plus ICC plus EXIF plus multiple
private chunks as `IHDR`, `iCCP`, selected text chunk, all pre-`IDAT` private
chunks in caller order, `eXIf`, `IDAT`, all after-`IDAT` private chunks in
caller order, `IEND`. The same route also covers compressed language-keyed
`iTXt` plus ICC plus EXIF plus multiple private chunks, and the first
compressed language-keyed batch with RGB transparency and `optimize=True`.
These preserve image bytes and reopen with the covered text plus ICC, EXIF,
and transparency metadata while hiding unknown private chunks. Interlace,
gamma, APNG, public/standard arbitrary chunk combinations, and open-side
unknown chunk preservation remain separate future surfaces until covered by
focused tests and facade routing.
`FMT-PNG-004AB` adds the `pillow_c_image_metadata_png_srgb` export and extends
the fixed-size public pre-`IDAT` chunk validator to accept `sRGB` with exactly
one payload byte. The covered standalone `PngInfo.add(b"sRGB", bytes([0]))`
fixture writes `IHDR`, `sRGB`, `IDAT`, `IEND`, preserves RGB bytes, and
reopens with `Info["srgb"] == 0` while leaving PNG text metadata empty.
Facade routing intentionally keeps `sRGB` combinations with text, ICC, EXIF,
transparency, optimize, compression, DPI, interlace, and save-side gamma as
explicit future boundaries.
`FMT-PNG-004AC` changes no signature and adds no export; it extends the same
single-chunk PNG route to accept RGB `bKGD` with exactly six payload bytes. The
covered standalone `PngInfo.add(b"bKGD", bytes([0,10,0,20,0,30]))` fixture
writes `IHDR`, `bKGD`, `IDAT`, `IEND`, preserves RGB bytes, and reopens without
exposing `bKGD` through PNG text/info metadata.
`FMT-PNG-004AD` changes no signature and adds no export; it extends that
mode-aware `bKGD` handling to P-mode one-byte payloads. The covered standalone
`PngInfo.add(b"bKGD", bytes([1]))` P-mode fixture writes `bKGD` after `PLTE`,
or after `tRNS` when palette transparency is present, then before `IDAT`.
Reopen preserves P-mode bytes and does not expose `bKGD` through PNG text/info
metadata.
`FMT-PNG-004AE` changes no signature and adds no export; it extends the same
public chunk validation and palette deferral path to the standard P-mode
`hIST` chunk. The covered fixture uses a three-entry palette and
`PngInfo.add(b"hIST", bytes([0,1,0,2,0,3]))`, requiring the payload length to
match `2 * palette_entries`. Standalone facade save writes `hIST` after `PLTE`
and before `IDAT`; the raw generalized metadata route also covers scalar
palette transparency, writing `PLTE`, `tRNS`, `hIST`, `IDAT`. Reopen preserves
P-mode bytes and does not expose `hIST` through PNG text/info metadata.
`FMT-PNG-004AF` changes no signature and adds no export; it extends the same
public chunk validation path to the standard `tIME` chunk with exactly seven
payload bytes. The covered standalone fixture uses
`PngInfo.add(b"tIME", bytes([0x07,0xE8,6,27,12,34,56]))`, writes `IHDR`,
`tIME`, `IDAT`, `IEND`, preserves RGB bytes on reopen, and does not expose
`tIME` through PNG text/info metadata.
`FMT-PNG-004AG` changes no signature and adds no export; it extends the same
public chunk validation path to the standard RGB `sBIT` chunk with exactly
three payload bytes. The covered standalone fixture uses
`PngInfo.add(b"sBIT", bytes([8,7,6]))`, writes `IHDR`, `sBIT`, `IDAT`, `IEND`,
preserves RGB bytes on reopen, and does not expose `sBIT` through PNG
text/info metadata.
`FMT-PNG-004AH` changes no signature and adds no export; it extends the same
public chunk validation path to the standard `cICP` chunk with exactly four
payload bytes. The covered standalone fixture uses
`PngInfo.add(b"cICP", bytes([1,13,0,1]))`, writes `IHDR`, `cICP`, `IDAT`,
`IEND`, preserves RGB bytes on reopen, and does not expose `cICP` through PNG
text/info metadata.
`FMT-PNG-004AI` changes no signature and adds no export; it extends the same
public pre-`IDAT` chunk route to the standard `sPLT` chunk for a bounded valid
suggested-palette payload. The covered standalone fixture uses
`PngInfo.add(b"sPLT", b"pal\0\x08" + bytes([10,20,30,255,0,5]))`, writes
`IHDR`, `sPLT`, `IDAT`, `IEND`, preserves RGB bytes on reopen, and does not
expose `sPLT` through PNG text/info metadata. Release x64 was rebuilt after
native validation changed, and source/DLL export counts remain `341` / `341`
`pillow_c_*` names.
`FMT-PNG-001AB` changes no signature and adds no export; it extends the
existing PNG text metadata ABI to dynamic-Huffman compressed `zTXt` and
compressed `iTXt` open payloads. The WIC decode input may be a memory copy with
compressed text chunks removed, but text metadata still comes from the original
file and is exposed through `pillow_c_image_metadata_png_text_count` and
`pillow_c_image_metadata_png_text`. Release x64 was rebuilt after native code
changed, and source/DLL export counts remain `341` / `341` `pillow_c_*` names.
`FMT-PNG-001AC` changes no signature and adds no export; it extends the
existing PNG text save/open ABI to bounded NUL-free Latin-1 byte values for
`PngInfo.add_text(..., bytes)`. For the covered route, native `tEXt`/`zTXt`
save accepts Latin-1 high bytes, and native open converts `tEXt`/`zTXt`
Latin-1 metadata to UTF-8 before the existing indexed metadata export copies
key/value strings to callers. Release x64 was rebuilt after native code
changed, and source/DLL export counts remained `378` / `378` `pillow_c_*`
names.
`FMT-PNG-001AD` adds
`pillow_c_image_save_png_text_entries_value_sizes_options(image, path,
compress_level, dpi_x, dpi_y, keys, values, value_sizes, compressed,
text_count)` for the bounded embedded-NUL PNG text-value route. `keys` and
`values` follow the existing pointer-array convention, `value_sizes` supplies
the exact byte count for each value, and `compressed` selects `tEXt` (`0`) or
`zTXt` (`1`) for each entry. The export rejects invalid compression flags
through the shared PNG text validation and currently does not cover `iTXt`,
custom chunks, ICC/EXIF, transparency, optimize, or other metadata
combinations. Release x64 was rebuilt after native code changed, and
source/DLL export counts are now `379` / `379` `pillow_c_*` names.

`pillow_c_image_open_jpeg`, `pillow_c_image_open_jpeg_draft`,
`pillow_c_image_open_jpeg_draft_mode`,
`pillow_c_image_save_jpeg`,
`pillow_c_image_save_jpeg_quality`, `pillow_c_image_save_jpeg_options`,
`pillow_c_image_save_jpeg_subsampling_options`,
`pillow_c_image_save_jpeg_encode_options`,
`pillow_c_image_save_jpeg_extra_options`,
`pillow_c_image_save_jpeg_restart_marker_blocks_options`,
`pillow_c_image_save_jpeg_restart_marker_rows_options`,
`pillow_c_image_save_jpeg_metadata_restart_marker_options`,
`pillow_c_image_save_jpeg_metadata_restart_marker_encode_options`,
`pillow_c_image_save_jpeg_metadata_restart_marker_progressive_options`,
`pillow_c_image_save_jpeg_encode_keep_rgb_options`,
`pillow_c_image_save_jpeg_qtables_encode_options`,
`pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options`,
`pillow_c_image_save_jpeg_keep_rgb_restart_marker_encode_options`,
`pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options`,
`pillow_c_image_save_jpeg_metadata_options`, and
`pillow_c_image_save_jpeg_metadata_subsampling_options`, and
`pillow_c_image_save_jpeg_metadata_encode_options`, and
`pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options`, and
`pillow_c_image_save_jpeg_qtables_metadata_encode_options` keep JPEG decode/encode
inside the DLL through WIC plus native marker patching. The current JPEG path
supports lossy `L`, `RGB`, and bounded `CMYK` open image handles. Native open
probes the JPEG frame component count before decoding so one-component JPEGs
reopen as `L`, three-component JPEGs reopen as public RGB byte order, and the
covered four-component Adobe transform `0` fixture plus the covered APP14
Adobe transform `2` YCCK fixtures reopen as public `CMYK` byte order through
WIC `32bppCMYK`. `FMT-JPEG-003C` first included both the earlier patched `2x1`
fixture and Pillow's real `100x100` `pil_sample_cmyk.jpg` fixture copied into
`ahk\fixtures\pil_sample_cmyk_ycck.jpg`; that earlier open-only slice added
coverage only and no new
ABI symbol. The same native segment scan now captures
EXIF orientation, JFIF DPI/density metadata, bounded COM comment bytes, one
or more complete same-count `ICC_PROFILE` APP2 payloads, `Exif\0\0` APP1
bytes, bounded APP1 XMP payload bytes after
`http://ns.adobe.com/xap/1.0/\0`, and JPEG DQT segments as natural-order
quantization-table metadata.
For RGB JPEGs, the same scan maps SOF luma/chroma sampling to Pillow
subsampling metadata: `1x1` maps to `0`, `2x1` maps to `1`, `2x2` maps to
`2`, and unsupported shapes report `-1`.
All JPEG save exports that read source pixels refresh an active readonly
`frombuffer` view before encoding. This covers the shared default/WIC save
helper, qtables/optimized helper, keep-rgb helper, and qtables keep-rgb helper,
so `Image.frombuffer(...).Save(..., "JPEG")` samples current caller bytes while
leaving the view attached and readonly. `BYTES-001AD` added no export; source
and Release x64 DLL export counts remain `364` / `364`.
Native save accepts `L`, `RGB`, bounded baseline `CMYK`, bounded baseline CMYK
DPI/JFIF, bounded baseline CMYK qtables, bounded CMYK DPI/JFIF plus qtables
with or without metadata, bounded optimized CMYK, bounded progressive CMYK,
bounded CMYK metadata plus optimized output, bounded baseline CMYK DPI plus
metadata, bounded CMYK DPI plus optimized metadata, and bounded CMYK DPI plus
progressive metadata, bounded CMYK baseline DPI plus subsampling, bounded
CMYK DPI qtables metadata plus optimize/progressive saves, bounded CMYK DPI
sampled progressive qtables saves, and bounded no-qtables CMYK `keep_rgb=True`
optimized progressive aliases with optional DPI and optional explicit
comment/ICC/EXIF metadata.
The `L` and default
non-advanced `RGB` paths still use WIC where appropriate, packing RGB to WIC's
BGR encoder format inside C++; alpha and palette modes return `-3`.
The bounded `FMT-JPEG-003D` CMYK save path does not use WIC's CMYK encoder,
because WIC writes incompatible APP14/SOF sampling metadata for the covered
fixture. Instead, the existing JPEG save exports route mode `CMYK`, baseline
quality saves through a native encoder that inverts samples to match Pillow's
`CMYK;I` JPEG raw mode, writes APP14 Adobe transform `0`, one DQT table `0`,
SOF0 components `C/M/Y/K` with `1x1` sampling and qtable `0`, standard
luminance DHTs, one four-component SOS, and EOI. `FMT-JPEG-002B2O` extends the
same native encoder behind the existing qtables export for bounded baseline
CMYK qtables saves: one custom qtable writes DQT table `0` for all components;
two custom qtables write DQT tables `0` and `1`, with `C` selecting table `0`
and `M/Y/K` selecting table `1`. `FMT-JPEG-003E` extends the same native
encoder behind the existing encode-options export for bounded CMYK
`optimize=True` saves by collecting Huffman frequencies across C/M/Y/K blocks
and writing compact optimized DC/AC DHT tables. `FMT-JPEG-003F` verifies that
the existing metadata export also composes this CMYK baseline encoder with
APP1 EXIF, APP2 ICC, and COM marker patching after APP14 for bounded baseline
metadata saves. `FMT-JPEG-003K` extends the same native CMYK baseline encoder
behind `pillow_c_image_save_jpeg_options` for bounded `quality=95` plus
`dpi=(300,150)` saves by writing APP0/JFIF density before APP14, then reopening
with exact CMYK bytes and DPI/JFIF metadata. `FMT-JPEG-003L` extends the
existing metadata export for bounded baseline CMYK `quality=95` plus
`dpi=(300,150)`, comment, single-segment ICC, and EXIF metadata by letting the
CMYK baseline DPI encoder write APP0/JFIF before APP14 and then inserting APP1,
APP2, and COM after APP14. `FMT-JPEG-003M` extends the existing metadata encode
export for bounded baseline CMYK `quality=95` plus `dpi=(300,150)`,
`optimize=True`, comment, single-segment ICC, and EXIF metadata by reusing the
same JFIF APP0 writer, optimized CMYK DHT path, and APP14-aware metadata
patcher. `FMT-JPEG-003N` extends the same metadata encode export for bounded
progressive CMYK `quality=95` plus `dpi=(300,150)`, comment, single-segment
ICC, and EXIF metadata by reusing the JFIF APP0 writer and APP14-aware
metadata patcher before the existing 18-scan CMYK progressive script.
`FMT-JPEG-003O` extends the existing qtables exports for bounded baseline CMYK
`quality=95` plus `dpi=(300,150)`, one or two custom qtables, and optional
comment, single-segment ICC, and EXIF metadata by reusing the JFIF APP0 writer
before APP14 and the APP14-aware metadata patcher before the custom DQT
segments. `FMT-JPEG-003S` extends the existing encode-options export for
bounded baseline CMYK `quality=95`, `dpi=(300,150)`, and `subsampling=0`,
`1`, or `2` by keeping `C` as the full-resolution component, floor-downsampling
inverted `M/Y/K` samples, writing APP0/JFIF before APP14, and reopening with
Pillow-compatible CMYK bytes plus DPI/JFIF metadata. `FMT-JPEG-003T` extends
the existing keep-rgb encode export for bounded non-metadata CMYK
`keep_rgb=True` saves by treating keep-rgb as an alias into the same native
CMYK encode-options path for baseline DPI/subsampling, optimized baseline, and
progressive output. `FMT-JPEG-003AM` extends that same no-qtables keep-rgb
alias family to the bounded combined `progressive=True,optimize=True` case,
with optional DPI and optional comment/ICC/EXIF metadata. `FMT-JPEG-003U`
extends the existing metadata keep-rgb encode export for bounded CMYK
`keep_rgb=True` plus comment/ICC/EXIF metadata when qtables are absent.
`FMT-JPEG-003V` extends the existing qtables keep-rgb
encode export for bounded CMYK `keep_rgb=True` plus custom qtables when DPI,
metadata, and subsampling are absent. `FMT-JPEG-003W` adds a qtables metadata
keep-rgb export for bounded CMYK `keep_rgb=True` plus custom qtables and
comment/ICC/EXIF metadata when DPI and real subsampling are absent.
`FMT-JPEG-003X` extends that same qtables metadata keep-rgb export to the
bounded DPI case, writing APP0/JFIF before APP14 and preserving
APP1/APP2/COM metadata before custom DQT segments. `FMT-JPEG-003Y` extends
the non-metadata qtables keep-rgb export for baseline real subsampling `1`
and `2`, and `FMT-JPEG-003Z` extends the qtables metadata keep-rgb export for
the matching no-DPI baseline real subsampling route. `FMT-JPEG-003AA` extends
the existing qtables keep-rgb exports to the bounded DPI plus baseline real
subsampling route, with or without comment/ICC/EXIF metadata, writing
APP0/JFIF before APP14 and preserving sampled SOF0 shape. `FMT-JPEG-003AB`
extends the same existing qtables keep-rgb exports to bounded `optimize=True`
plus real subsampling `1` or `2`, with or without comment/ICC/EXIF metadata.
`FMT-JPEG-003AD` extends the same keep-rgb qtables exports to bounded no-DPI
sampled progressive output, with or without comment/ICC/EXIF metadata.
`FMT-JPEG-003AE` extends the direct qtables exports to the matching bounded
no-DPI non-keep-rgb sampled progressive output, with or without
comment/ICC/EXIF metadata. `FMT-JPEG-003AC` covers no-DPI non-keep-rgb qtables
metadata with baseline real subsampling `1` or `2` through the existing qtables
metadata export and facade route. `FMT-JPEG-003AF` extends the same sampled
progressive qtables route to bounded DPI/JFIF output, including direct,
metadata, and keep-rgb public surfaces. Optimized baseline non-keep-rgb
sampled qtables metadata and explicit YCCK save surfaces remain unsupported
bounded paths and return `-3` or are rejected by the facade.
`FMT-JPEG-003C` real-fixture keep-save coverage adds a bounded keep-style
route for opened CMYK JPEGs with preserved DQT metadata: Pillow 11.3.0
normalizes the real APP14 transform `2` YCCK fixture to ordinary CMYK APP14
transform `0` when saving with `quality="keep"`, preserves the two source DQT
tables exactly, writes SOF0 `C/M/Y/K` qtable selectors `[0,1,1,1]` with `1x1`
sampling, and reopens as mode `CMYK`. The facade maps bounded
`quality: "keep"` and `qtables: "keep"` on opened CMYK JPEGs to the existing
native qtables save route with `quality == -1`.
`FMT-JPEG-003C` RGB keep-save coverage extends that route to bounded opened
RGB JPEGs: the facade combines stored native qtables and stored native
subsampling when saving with `quality="keep"` or `qtables="keep"`, so
4:2:2 and 4:2:0 source files preserve both DQT payloads and SOF0 sampling
through the existing qtables encoder. The `subsampling="keep"` facade extension
does not add an ABI symbol; it resolves the public keep sentinel through
`pillow_c_image_metadata_jpeg_subsampling` on an opened JPEG handle before
calling the same qtables save route. For explicit-quality metadata saves that
also use public `subsampling="keep"`, the facade resolves the same native
subsampling metadata and passes opened COM/comment into the existing
`pillow_c_image_save_jpeg_metadata_subsampling_options` route only when the
caller omits `comment`; explicit comment values remain caller-controlled. The
follow-up RGB progressive keep coverage uses the same ABI path with
`progressive == 1`: opened RGB
`quality="keep", progressive=True` saves preserve the stored qtables and stored
4:2:0 subsampling, write SOF2 with ten RGB progressive SOS scans, and rely on
the existing opened-comment patcher for COM carry-through.
The same existing qtables metadata route composes keep-style opened RGB JPEG
saves with caller-supplied ICC/EXIF and opened COM/comment bytes: the DLL
continues to receive explicit COM/ICC/EXIF buffers, and the facade supplies the
opened COM only when the public `comment` option is omitted, so explicit empty
comments remain caller-controlled.
`FMT-JPEG-003C` L keep-save coverage uses the same qtable metadata ABI and
existing `pillow_c_image_save_jpeg_qtables_encode_options` route for bounded
opened mode `L` JPEGs. The facade now accepts `quality="keep"` or
`qtables="keep"` for opened L JPEG handles, supplies the one stored luminance
qtable with `quality == -1`, and preserves the source DQT payload plus the
one-component SOF0/SOS shape without adding a new export.
`FMT-JPEG-003H`
verifies
that the existing metadata encode export
composes the optimized CMYK encoder with the same APP1/APP2/COM marker patching
after APP14 for bounded metadata plus `optimize=True` saves. `FMT-JPEG-003I`
verifies that the same export composes the CMYK progressive encoder with
metadata patching after APP14 for bounded metadata plus `progressive=True`
saves. `FMT-JPEG-003J` verifies that the existing qtables metadata export
composes the CMYK qtables baseline encoder with metadata patching after APP14
for bounded qtables plus metadata saves.
`pillow_c_image_save_jpeg_quality` accepts an integer quality value, maps `-1`
to the encoder default, clamps other values into WIC's `0..100` quality range,
and is retained for ABI compatibility. `pillow_c_image_save_jpeg_options` is
the existing JPEG save-options ABI used by the facade for `quality` and `dpi`;
DPI values follow Pillow's `round()` behavior, positive rounded pairs write
JFIF `units=1` density values, and any non-positive rounded component writes
Pillow's default `units=0, density=1x1` metadata. For bounded mode `CMYK`,
this export routes plain baseline DPI saves through the native CMYK encoder and
emits APP0/JFIF before APP14. CMYK DPI combined with metadata is routed through
`pillow_c_image_save_jpeg_metadata_options`; CMYK DPI combined with optimized
metadata or progressive metadata is routed through
`pillow_c_image_save_jpeg_metadata_encode_options`. CMYK DPI with qtables is
routed through `pillow_c_image_save_jpeg_qtables_encode_options`; CMYK DPI with
qtables plus metadata is routed through
`pillow_c_image_save_jpeg_qtables_metadata_encode_options`. CMYK DPI with
baseline subsampling is routed through `pillow_c_image_save_jpeg_encode_options`.
CMYK keep-rgb without metadata or qtables, including the bounded
progressive-plus-optimized alias case from `FMT-JPEG-003AM`, is routed through
`pillow_c_image_save_jpeg_encode_keep_rgb_options`; CMYK keep-rgb with
comment/ICC/EXIF metadata and no qtables, including the bounded
progressive-plus-optimized alias case from `FMT-JPEG-003AM`, is routed through
`pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options`; CMYK keep-rgb with
qtables and no metadata/subsampling is routed through
`pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options`; CMYK keep-rgb with
qtables plus comment/ICC/EXIF metadata and no real subsampling is routed
through `pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options`,
with DPI either absent or present as a JFIF APP0 density. QTables combined
with baseline real subsampling plus keep-rgb and no metadata are routed through
`pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options`; the same export
also routes the bounded `optimize=True` sampled qtables keep-rgb case. QTables
plus comment/ICC/EXIF metadata and baseline real subsampling with keep-rgb are
routed through `pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options`;
the same metadata export also routes the bounded `optimize=True` sampled
qtables keep-rgb metadata case. `FMT-JPEG-003AD` routes the bounded no-DPI
`progressive=True` sampled qtables keep-rgb cases, with or without metadata,
through the same keep-rgb qtables exports. `FMT-JPEG-003AE` routes the matching
direct no-DPI non-keep-rgb qtables cases through
`pillow_c_image_save_jpeg_qtables_encode_options` and
`pillow_c_image_save_jpeg_qtables_metadata_encode_options`. `FMT-JPEG-003AF`
routes the matching bounded DPI sampled progressive qtables cases, including
direct, metadata, and keep-rgb public surfaces, through the same qtables ABI
shapes. Explicit YCCK save behavior remains rejected for bounded CMYK.
`pillow_c_image_save_jpeg_subsampling_options` preserves the JPEG options
arguments and appends an integer `subsampling` value. The bounded Pillow 11.3.0
mapping is `0 -> 4:4:4`, `1 -> 4:2:2`, and `2 -> 4:2:0`, implemented through
WIC's `JpegYCrCbSubsampling` encoder option for RGB and through the native
CMYK baseline encoder for bounded CMYK; other values return `-3`.
`pillow_c_image_save_jpeg_encode_options` preserves the same JPEG options
arguments and appends `subsampling`, `progressive`, and `optimize` integers.
`subsampling` uses the same `-1`/`0`/`1`/`2` mapping as
`pillow_c_image_save_jpeg_subsampling_options`; `progressive` and `optimize`
are tri-state values where `-1` means unset/default, `0` means false, and `1`
requests the true option. `progressive == 1` is accepted for bounded mode `L`
when `subsampling == -1`; the DLL uses a native grayscale progressive JPEG
encoder with quality-scaled luminance quantization, scan-local optimized
Huffman table generation, progressive DC/AC first scans, progressive
refinement scans, and JFIF/DQT/SOF2/DHT/SOS/EOI marker emission. The covered
`FMT-JPEG-002B2D` fixture writes SOF2, one luminance component, six
Pillow-compatible grayscale SOS headers, and reopens as mode `L`.
`progressive == 1` is also accepted for bounded mode `RGB` when `subsampling`
is `-1`, `0`, `1`, or `2`; the DLL uses a native RGB progressive JPEG encoder
with RGB-to-YCbCr conversion, optional chroma downsampling, luminance and
chrominance quantization, scan-local optimized Huffman tables, interleaved
progressive DC first/refinement scans, component AC first/refinement scans, and
JFIF/DQT/SOF2/DHT/SOS/EOI marker emission. The covered `FMT-JPEG-002B2E`
fixture writes Pillow-compatible default 4:2:0 sampling for `subsampling == -1`,
4:4:4 sampling for `subsampling == 0`, 4:2:2 sampling for `subsampling == 1`,
and 4:2:0 sampling for `subsampling == 2`, with ten RGB progressive SOS
headers and mode `RGB` reopen. `optimize == 1` composes with this RGB
progressive route because the progressive encoder already writes scan-local
optimized Huffman tables. Other unsupported mode/option combinations still
return `-3`. `optimize == 1` is accepted for the bounded mode `L` path when
`subsampling == -1`; the DLL uses a native grayscale baseline JPEG
encoder with quality-scaled luminance quantization, optimized Huffman frequency
collection, pseudo-symbol guarded DHT generation, entropy bit writing, and
JFIF/DQT/SOF0/DHT/SOS/EOI marker emission. The covered `FMT-JPEG-002B2A`
fixture writes baseline SOF0, one SOS scan, two compact DHT tables, and reopens
as mode `L`. `optimize == 1` is also accepted for bounded mode `RGB` when
`subsampling` is `-1`, `0`, `1`, or `2`; the DLL uses a native RGB encoder with
RGB-to-YCbCr conversion, optional chroma downsampling, luminance/chrominance
quantization, four optimized Huffman tables, interleaved Y/Cb/Cr entropy, and
JFIF/DQT/SOF0/DHT/SOS/EOI marker emission. The covered
`FMT-JPEG-002B2B` fixture writes baseline SOF0, one SOS scan, 4:4:4 sampling,
four compact DHT tables, and reopens as mode `RGB`. The covered
`FMT-JPEG-002B2C` fixture writes Pillow-compatible default 4:2:0 sampling for
`subsampling == -1`, 4:2:2 sampling for `subsampling == 1`, and 4:2:0 sampling
for `subsampling == 2`, with one SOS scan, four compact DHT tables, and mode
`RGB` reopen. Bounded mode `CMYK` accepts baseline `subsampling == -1`, `0`,
`1`, or `2` when `optimize != 1` and `progressive != 1`; `-1` and `0` write
all four components with `1x1` sampling, `1` writes `C` as `2x1` with `M/Y/K`
`1x1`, and `2` writes `C` as `2x2` with `M/Y/K` `1x1`. The DLL uses the
native CMYK baseline encoder with `CMYK;I` sample inversion and floor
downsampling of inverted `M/Y/K` samples for the subsampled cases; the covered
`FMT-JPEG-003S` fixture preserves APP0/JFIF DPI metadata and reopens the
expected CMYK bytes. `optimize == 1` is also accepted for bounded mode `CMYK` when
`subsampling == -1` and `progressive != 1`; the DLL uses the native CMYK
baseline encoder with `CMYK;I` sample inversion, quality-scaled luminance
quantization, optimized Huffman frequency collection across C/M/Y/K blocks,
APP14 transform `0`, SOF0 `C/M/Y/K` components, one SOS, and EOI. The covered
`FMT-JPEG-003E` fixture writes compact DHT payload lengths `[19,38]` and
reopens exact CMYK bytes. `progressive == 1` is accepted for bounded mode
`CMYK` when `subsampling == -1` and no qtables are requested; the plain
encode-options route also accepts `optimize == 1` for the covered
`FMT-JPEG-003AI` no-DPI/no-metadata/no-keep-rgb surface and the
`FMT-JPEG-003AL` DPI/no-metadata/no-keep-rgb surface, and the keep-rgb
encode-options routes accept the same combined progressive-plus-optimized
native CMYK output for `FMT-JPEG-003AM`. When `has_dpi == 1`,
APP0/JFIF density remains before APP14. The DLL uses the native CMYK
progressive encoder with `CMYK;I` sample inversion, quality-scaled luminance
quantization, scan-local optimized Huffman tables, APP14 transform `0`, SOF2
`C/M/Y/K` components, and the covered 18-scan CMYK progressive script. The
covered `FMT-JPEG-003G`, `FMT-JPEG-003AI`, `FMT-JPEG-003AL`, and
`FMT-JPEG-003AM` fixtures write 18 SOS scans and reopen as mode `CMYK` within
the established progressive JPEG lossy tolerance. No ABI symbol was added for
`FMT-JPEG-003AL` or `FMT-JPEG-003AM`. Exact libjpeg
entropy byte parity and
broader YCCK save semantics remain future JPEG codec-strategy surfaces and
continue to fail explicitly instead of silently writing incompatible WIC output.
`pillow_c_image_save_jpeg_encode_keep_rgb_options` preserves the same path,
format, quality, and DPI arguments as `pillow_c_image_save_jpeg_encode_options`
and appends `subsampling`, `progressive`, `optimize`, and `keep_rgb`. The
bounded `FMT-JPEG-002B2G` route accepts mode `RGB`, `keep_rgb == 1`, and
`subsampling == -1` or `0`; `subsampling == 1` or `2` returns `-3`, matching
Pillow's unsupported keep-rgb/subsampling combinations. Baseline output writes
APP14 Adobe transform `0`, omits APP0/JFIF when DPI is absent, writes one DQT table, emits SOF0
component IDs `R/G/B` with `1x1` sampling and qtable `0`, and writes one RGB
SOS. `optimize == 1` keeps the same marker shape while writing optimized DHT
tables. `progressive == 1` writes SOF2 with the same RGB component IDs and 14
Pillow-compatible SOS scans. `FMT-JPEG-002B2Q` extends the same RGB route for
`has_dpi == 1`: native output writes APP0/JFIF density before APP14, then
keeps the existing RGB component IDs, `1x1` sampling, and SOS shape; reopened
handles expose DPI/JFIF metadata through `pillow_c_image_metadata_resolution`.
For bounded mode `CMYK`, `keep_rgb == 1` is
accepted as a Pillow-compatible alias to the native CMYK encode-options path
when no metadata or qtables are requested. The covered `FMT-JPEG-003T` surface
routes baseline `dpi=(300,150)` with `subsampling == 0` or `1`, optimized
baseline output, and progressive output through the existing CMYK encoders.
`FMT-JPEG-003AM` keeps the same export shape and extends this alias to bounded
`progressive == 1` plus `optimize == 1`, with or without DPI. It writes APP14
transform `0`, optional APP0/JFIF density before APP14, one DQT, SOF2 `C/M/Y/K`
components, the 18-scan CMYK progressive script, and reopens within the
established progressive tolerance. QTables remain separate bounded surfaces
outside this no-qtables keep-rgb coverage.
`pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options` preserves the same
metadata buffer arguments as `pillow_c_image_save_jpeg_metadata_encode_options`
and appends `subsampling`, `progressive`, `optimize`, and `keep_rgb`. The
bounded `FMT-JPEG-002B2N` route accepts mode `RGB`, `keep_rgb == 1`,
`subsampling == -1` or `0`, optional `optimize == 1`, and optional
`progressive == 1`. It first writes the JPEG through the native keep-rgb
encoder, then inserts APP1 EXIF, APP2 ICC, and COM metadata after the initial
APP14 Adobe segment and before DQT, preserving Pillow 11.3.0's no-JFIF
keep-rgb metadata order. The facade routes non-qtables JPEG metadata plus
`keep_rgb=True` to this export. `FMT-JPEG-003U` extends the same export to
bounded mode `CMYK` when `keep_rgb == 1` and qtables are absent: baseline,
DPI/subsampling, optimized baseline, and progressive metadata output dispatch
through the existing native CMYK encode-options path before APP1/APP2/COM
metadata patching. `FMT-JPEG-003AM` extends this no-qtables metadata alias to
the bounded `progressive == 1` plus `optimize == 1` case, with or without DPI.
The covered CMYK route preserves APP14 transform `0`, keeps APP0/JFIF before
APP14 for DPI saves, inserts metadata after APP14 and before DQT, writes SOF2
`C/M/Y/K` components with 18 progressive SOS scans, and reopens with comment,
ICC, and EXIF metadata. CMYK qtables plus `keep_rgb` remains a separate bounded
ABI surface.
`pillow_c_image_save_jpeg_metadata_keep_rgb_xmp_encode_options` preserves the
same metadata keep-rgb argument shape and inserts `const uint8_t* xmp` plus
`size_t xmp_size` before the trailing `subsampling`, `progressive`,
`optimize`, and `keep_rgb` integers. The bounded `META-002D` route accepts
mode `RGB`, `keep_rgb == 1`, `subsampling == -1` or `0`, no qtables, and an
explicit XMP payload. It writes the JPEG through the native keep-rgb encoder,
then composes metadata after APP14 Adobe transform `0` and before DQT, with
APP1 EXIF before APP1 XMP and ICC/COM after XMP when those optional metadata
buffers are supplied. Reopened JPEG handles expose the exact XMP bytes through
`pillow_c_image_metadata_xmp`. QTables plus `keep_rgb + xmp` remains a
separate bounded surface.
`pillow_c_image_save_jpeg_qtables_encode_options` preserves the same path,
format, quality, and DPI arguments as `pillow_c_image_save_jpeg_encode_options`
and inserts `const int* qtables` plus `size_t qtable_count` before the trailing
`subsampling`, `progressive`, and `optimize` integers. The qtables buffer is
packed as one or two contiguous 64-entry signed 32-bit integer tables in
natural order. Values must be in `1..255`; invalid table counts, null buffers,
short facade tables, or out-of-range values return `-3`. For caller-supplied
qtables, ordinary integer quality values apply Pillow-style quality scaling;
`quality == -1` preserves the supplied table values exactly and is the native
route used by the bounded `quality="keep"` / `qtables="keep"` facade path for
opened JPEGs with stored DQT metadata. `FMT-JPEG-002C` also uses this same ABI
shape for Pillow JPEG quality preset strings: the facade packs the preset
quantization tables from Pillow 11.3.0, applies the preset subsampling,
overrides explicit `qtables`/`subsampling` options, and calls the qtables
export with `quality == -1`; no new DLL export is involved. The bounded
`FMT-JPEG-002B2H` baseline route accepts mode `RGB`, `qtable_count == 1` or
`2`, `optimize == 1`, `progressive != 1`, and `subsampling == -1`, `0`, `1`,
or `2`. The DLL quality-scales the custom qtables, writes DQT table `0` for Y
and table `1` for Cb/Cr when present, stores DQT entries in zigzag order,
writes APP0/JFIF, SOF0 Y/Cb/Cr table selectors, optimized DHT tables, one
SOS, and EOI through the native RGB optimized-Huffman encoder. The
`FMT-JPEG-002B2I` progressive route uses the same ABI shape and accepts
`progressive == 1` with `optimize == -1`, `0`, or `1`, writing APP0/JFIF,
custom DQT payloads, SOF2 Y/Cb/Cr table selectors, ten Pillow-compatible RGB
progressive SOS scans, and EOI through the native RGB progressive encoder. The
`FMT-JPEG-003C` opened RGB progressive quality-keep facade route reuses this
same encoder with `quality == -1`, caller `progressive == 1`, stored qtables,
and stored subsampling. The
`FMT-JPEG-002B2L` default-Huffman route also uses this ABI shape for bounded
mode `RGB` when `progressive != 1` and `optimize != 1`: it writes APP0/JFIF,
custom DQT payloads, SOF0 default/explicit RGB sampling, the standard
luminance/chrominance DHT payloads, one SOS, and EOI through the native RGB
baseline encoder. `FMT-JPEG-002B2M` extends the same export to bounded mode
`L`, `qtable_count == 1` or `2`, and `subsampling == -1`; the second qtable is
ignored for grayscale output, only DQT table `0` is written, default-Huffman
baseline output uses standard DHT payload lengths `[29,179]`, `optimize == 1`
writes compact optimized DHTs, and `progressive == 1` writes SOF2 with six
grayscale SOS scans. `FMT-JPEG-002B2O` extends the same export to bounded mode
`CMYK`, `qtable_count == 1` or `2`, `subsampling == -1`, `progressive != 1`,
and `optimize != 1`; the DLL quality-scales the custom qtables, writes APP14
transform `0`, DQT table `0` for all components or optional table `1` for
`M/Y/K`, standard luminance DHT payload lengths `[29,179]`, one
four-component SOS, and EOI through the native CMYK baseline encoder.
`FMT-JPEG-003O` extends this same bounded CMYK qtables route to `has_dpi == 1`
with positive `dpi_x`/`dpi_y`, writing APP0/JFIF density before APP14 and then
the same custom DQT/SOF0/DHT/SOS shape. QTables plus metadata for the covered
RGB, mode `L`, and bounded baseline CMYK routes are handled by
`pillow_c_image_save_jpeg_qtables_metadata_encode_options`; the same
`FMT-JPEG-003O` surface covers bounded CMYK DPI plus qtables metadata by
inserting APP1 EXIF, APP2 ICC, and COM after APP14 and before the custom DQT
segments. `FMT-JPEG-003P` extends the same non-metadata CMYK qtables export to
bounded `optimize=True`, `progressive=True`, and combined
`optimize=True,progressive=True` output: optimized baseline uses the CMYK
baseline encoder with optimized DHT tables, while progressive output writes
custom DQT payloads before the covered CMYK 18-scan SOF2/SOS script.
`FMT-JPEG-003Q` composes that bounded non-DPI CMYK qtables optimize/progressive
support with the existing APP1/APP2/COM metadata patcher through
`pillow_c_image_save_jpeg_qtables_metadata_encode_options`, inserting metadata
after APP14 and before the custom DQT segments. `FMT-JPEG-003R` extends the
same qtables metadata export to bounded CMYK `dpi=(300,150)` plus
`optimize=True`, `progressive=True`, or both by preserving APP0/JFIF before
APP14 and inserting APP1/APP2/COM after APP14 and before the custom DQT
segments. `FMT-JPEG-003AC` covers bounded no-DPI non-keep-rgb CMYK qtables
metadata with baseline `subsampling=1` or `2`: the same export writes sampled
SOF0 shape, standard DHTs, one SOS, and reopened metadata through the existing
sampled CMYK qtables encoder plus metadata patcher. `FMT-JPEG-003AE` covers
the no-DPI non-keep-rgb CMYK qtables metadata progressive route with
`subsampling=1` or `2`, writing sampled SOF2 shape, 18 scans, and reopened
metadata through the existing sampled CMYK progressive qtables encoder plus
metadata patcher. `FMT-JPEG-003AF` covers the bounded DPI sampled progressive
qtables metadata route through the same export, preserving APP0/JFIF before
APP14 and metadata before DQT. Optimized baseline non-keep-rgb metadata
sampled qtables and YCCK qtables remain unsupported bounded surfaces and return
`-3` through current native/facade routing.
`pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options` preserves the same
leading encode-options arguments, then appends the packed qtables pointer,
`qtable_count`, `subsampling`, `progressive`, `optimize`, and `keep_rgb`. The
qtables buffer layout and validation match
`pillow_c_image_save_jpeg_qtables_encode_options`. The covered
`FMT-JPEG-002B2K` surface accepts mode `RGB`, one or two qtables, `keep_rgb ==
1`, and `subsampling == -1` or `0`; DPI and metadata remain rejected for this
bounded route. Baseline and optimized output write APP14 Adobe transform `0`,
omit APP0/JFIF, write DQT table `0` plus table `1` when two qtables are
provided, emit SOF0 component IDs `R/G/B` with qtable selectors `[0,1,1]` for
two tables or `[0,0,0]` for one table, and write one RGB SOS. Progressive
output writes SOF2 with the same RGB component IDs and 14
Pillow-compatible keep-rgb SOS scans. `FMT-JPEG-002B2R` uses this existing
export for opened RGB `qtables="keep"` plus `keep_rgb == 1` by passing the
opened DQT tables and `subsampling == -1`; this preserves opened COM/comment
metadata, writes APP14 transform `0`, omits JFIF, and keeps `R/G/B` component
IDs with `1x1` sampling even if the opened source JPEG used sampled YCbCr
subsampling. `FMT-JPEG-003V` extends the same export
to bounded mode `CMYK` when `keep_rgb == 1`, DPI and metadata are absent, and
subsampling is default: baseline, optimized baseline, progressive, and
combined progressive plus optimized qtables output dispatch through the
existing native CMYK qtables encode-options path. The covered CMYK route writes
APP14 transform `0`, omits APP0/JFIF, writes custom DQT table `0` for `C` and
table `1` for `M/Y/K` when present, and reopens as mode `CMYK` within qtables
tolerance. `FMT-JPEG-003Y` extends the same export to bounded baseline
`subsampling=1` and `2` for CMYK `keep_rgb == 1` qtables saves when DPI,
metadata, progressive, and optimize are absent. The native route keeps `C`
full-resolution, floor-downsamples inverted `M/Y/K` samples, writes SOF0
sampling `C=2x1` or `C=2x2` with `M/Y/K=1x1`, writes standard DHT payloads,
and reopens with the covered sampled CMYK bytes. CMYK qtables plus metadata
plus `keep_rgb` is routed through
`pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options`; sampled
qtables plus DPI is covered by `FMT-JPEG-003AA` for the bounded baseline
keep-rgb routes, and optimized sampled qtables plus `keep_rgb` is covered by
`FMT-JPEG-003AB`; progressive sampled qtables plus `keep_rgb` is covered by
`FMT-JPEG-003AD` without DPI and by `FMT-JPEG-003AF` with bounded DPI.
`pillow_c_image_save_jpeg_metadata_options` preserves the JPEG options
arguments and appends optional `comment`, `icc_profile`, and `exif` byte
buffers plus sizes. It first saves through the existing JPEG options path,
patches JFIF density when requested, then inserts APP1 EXIF, APP2 ICC, and COM
markers after APP0/JFIF for WIC-backed `L`/`RGB` output, after APP14 for native
CMYK baseline output, or after the leading APP0/JFIF plus APP14 sequence for
bounded native CMYK DPI output. The ICC writer emits one or more APP2
`ICC_PROFILE\0` segments with sequence numbers starting at `1` and a shared
segment count up to `255`; each segment carries at most `65519` caller ICC
bytes so the JPEG segment length stays within `65535`. Before the first SOF,
the JPEG metadata scan collects complete APP2 ICC payloads, sorts them
lexicographically at SOF, checks that
the sorted first fragment's count byte equals the collected fragment count,
and exposes the joined bytes after each 14-byte ICC marker header
through the existing `pillow_c_image_metadata_jpeg_icc_profile` export. No ABI
symbol was added for `FMT-JPEG-001A`. The covered `FMT-JPEG-003F` surface
accepts mode
`CMYK`, `quality=95`, no DPI, no subsampling, no qtables, no `keep_rgb`, no
`progressive=True`, and no `optimize=True`, writing APP14, APP1, APP2, COM,
DQT, SOF0, DHT, SOS order and reopening as exact mode `CMYK` bytes with
comment, ICC, and EXIF metadata. `FMT-JPEG-003L` extends the same export to
bounded CMYK `quality=95`, `dpi=(300,150)`, comment, single-segment ICC, and
EXIF metadata, writing APP0/JFIF, APP14, APP1, APP2, COM, DQT, SOF0, DHT, SOS
order and reopening with exact CMYK bytes plus DPI/JFIF/comment/ICC/EXIF
metadata.
`pillow_c_image_save_jpeg_metadata_subsampling_options` adds the same trailing
`subsampling` integer to the metadata save path so facade callers can combine
bounded comment/ICC/EXIF metadata with the covered subsampling controls.
`pillow_c_image_save_jpeg_metadata_encode_options` preserves the metadata save
arguments and appends `subsampling`, `progressive`, and `optimize` integers
with the same tri-state semantics as `pillow_c_image_save_jpeg_encode_options`.
It routes through the selected native/WIC JPEG encoder path and then inserts
APP1 EXIF, APP2 ICC, and COM markers after APP0/JFIF in the bounded Pillow
11.3.0 order. The covered `FMT-JPEG-002B2F` surface accepts RGB metadata plus
`optimize == 1` baseline SOF0 output and RGB metadata plus
`progressive == 1,optimize == 1` SOF2 output, preserving JFIF, EXIF, ICC,
comment, default 4:2:0 sampling, optimized/progressive scan shape, and reopened
metadata without an AHK pixel loop. The covered `FMT-JPEG-003H` surface also
accepts bounded mode `CMYK`, `quality=95`, `optimize == 1`,
`progressive != 1`, and no DPI/subsampling/qtables/keep-rgb, writing APP14,
APP1, APP2, COM, DQT, SOF0, compact optimized DHT payloads `[19,38]`, one
four-component SOS, exact mode `CMYK` reopen bytes, and reopened metadata
through the same export shape. The covered `FMT-JPEG-003M` surface accepts the
same bounded optimized CMYK metadata path with `has_dpi == 1` and
`dpi=(300,150)`, writing APP0/JFIF before APP14, then APP1/APP2/COM after
APP14, and reopening with exact CMYK bytes plus DPI/JFIF/comment/ICC/EXIF
metadata. The covered `FMT-JPEG-003I` surface accepts
bounded mode `CMYK`, `quality=95`, `progressive == 1`, `optimize != 1`, and no
DPI/subsampling/qtables/keep-rgb, writing APP14, APP1, APP2, COM, DQT, SOF2,
the 18-scan CMYK progressive script, mode `CMYK` reopen bytes, and reopened
metadata through the same export shape. The covered `FMT-JPEG-003N` surface
accepts the same bounded progressive CMYK metadata path with `has_dpi == 1`
and `dpi=(300,150)`, writing APP0/JFIF before APP14, then APP1/APP2/COM after
APP14, and reopening with CMYK bytes plus DPI/JFIF/comment/ICC/EXIF metadata.
`FMT-JPEG-003AJ` extends this same export and facade route to bounded mode
`CMYK`, `quality=95`, `progressive == 1`, `optimize == 1`, explicit
comment/ICC/EXIF metadata, no DPI/subsampling/qtables/keep-rgb, writing APP14,
APP1, APP2, COM, DQT, SOF2, the 18-scan CMYK progressive script, mode `CMYK`
reopen bytes, and reopened metadata. No ABI symbol was added for this covered
combination.
`FMT-JPEG-003AK` extends the same export and facade route to the bounded
`dpi=(300,150)` variant of that no-qtables/no-keep-rgb CMYK optimized
progressive metadata save. The existing path preserves APP0/JFIF density before
APP14, inserts APP1 EXIF, APP2 ICC, and COM after APP14 and before DQT, writes
SOF2 `1x1` CMYK components plus the 18-scan CMYK progressive script, and
reopens with CMYK bytes plus DPI/JFIF/comment/ICC/EXIF metadata. No ABI symbol
was added for this covered combination.
`pillow_c_image_save_jpeg_qtables_metadata_encode_options` preserves the same
leading metadata encode arguments, then appends the packed qtables pointer,
`qtable_count`, `subsampling`, `progressive`, and `optimize`. The qtables
buffer layout and validation match `pillow_c_image_save_jpeg_qtables_encode_options`.
The covered `FMT-JPEG-002B2J`, `FMT-JPEG-002B2L`, `FMT-JPEG-002B2M`,
`FMT-JPEG-003J`, `FMT-JPEG-003Q`, `FMT-JPEG-003R`, `FMT-JPEG-003AC`,
`FMT-JPEG-003AE`, and `FMT-JPEG-003AF`
surfaces accept mode `RGB`, `L`, or bounded `CMYK` as described above, one or
two qtables, metadata
buffers, and optimized, progressive, or default-Huffman baseline output where
covered. The export first
writes the JPEG through the native qtables encoder and then inserts APP1 EXIF,
APP2 ICC, and COM markers after APP0/JFIF for `RGB`/`L` routes or after APP14
for the bounded CMYK route, preserving Pillow 11.3.0 marker order before DQT.
For the bounded CMYK DPI plus qtables route, APP0/JFIF remains before APP14,
and metadata is inserted after APP14 and before the custom DQT segments.
The facade routes JPEG `qtables` plus metadata to this export for the covered
RGB, mode `L`, and bounded CMYK qtables families, including the
`FMT-JPEG-003AC` no-DPI CMYK qtables metadata baseline real-subsampling route,
the `FMT-JPEG-003AE` no-DPI CMYK qtables metadata progressive
real-subsampling route, and the `FMT-JPEG-003AF` DPI progressive
real-subsampling route. `FMT-JPEG-003AG` extends the same existing export and
facade route to the bounded optimized non-keep-rgb CMYK qtables metadata
baseline real-subsampling case, with optional DPI, while exact optimized
entropy table/byte parity remains outside the ABI guarantee.
`pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options` preserves
the same leading qtables metadata encode arguments and appends a `keep_rgb`
integer. `FMT-JPEG-002B2P` extends this existing export to bounded mode `RGB`
with `keep_rgb == 1`, no DPI, default subsampling, one or two custom qtables,
and explicit comment/ICC/EXIF metadata. The route writes through the native
RGB-component qtables keep-rgb encoder, then inserts APP1 EXIF, APP2 ICC, and
COM after APP14 Adobe transform `0` and before DQT, preserving RGB SOF
component IDs and caller qtable selectors. Reopened JPEG handles expose the
comment, ICC, and EXIF bytes through the existing metadata exports. The
`FMT-JPEG-003W`, `FMT-JPEG-003X`, `FMT-JPEG-003Z`,
`FMT-JPEG-003AA`, `FMT-JPEG-003AB`, `FMT-JPEG-003AD`, and
`FMT-JPEG-003AF` surfaces accept bounded mode `CMYK`,
one or two qtables,
`keep_rgb == 1`, and `subsampling == -1` or `0`, with DPI either absent or
present as a JFIF APP0 density; they also accept `subsampling == 1` or `2`
for the non-progressive, non-optimized baseline metadata route, with DPI
either absent or present, and for the progressive metadata route covered by
`FMT-JPEG-003AD` without DPI and `FMT-JPEG-003AF` with bounded DPI. It
delegates to the existing native CMYK qtables
metadata encoder, so baseline and optimized output write optional APP0/JFIF,
APP14, APP1/APP2/COM metadata, custom DQT tables, SOF0, DHT, and one SOS,
while progressive and combined progressive plus optimized output write
optional APP0/JFIF, APP14, metadata, custom DQT tables, SOF2, and the covered
18-scan CMYK progressive script. For sampled baseline or progressive routes,
`C` remains full-resolution, `M/Y/K` are floor-downsampled, SOF writes `C=2x1`
or `C=2x2` with `M/Y/K=1x1`, APP0/JFIF carries DPI for the covered baseline
and progressive DPI routes, and metadata remains before DQT. The route reopens as mode
`CMYK` within the established qtables tolerance and preserves DPI/JFIF when
supplied plus comment, ICC, and EXIF metadata.

`pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_xmp_encode_options`
preserves the same leading qtables metadata keep-rgb argument shape and inserts
`const uint8_t* xmp` plus `size_t xmp_size` before the qtables pointer/count.
The bounded `META-002E` route accepts mode `RGB`, `keep_rgb == 1`,
`subsampling == -1` or `0`, one or two caller qtables, and an explicit XMP
payload. It writes the JPEG through the native qtables keep-rgb encoder, then
composes metadata after APP14 Adobe transform `0` and before DQT, with APP1
EXIF before APP1 XMP and ICC/COM after XMP when those optional metadata buffers
are supplied. Reopened JPEG handles expose the exact XMP bytes through
`pillow_c_image_metadata_xmp`.

`pillow_c_image_metadata_jpeg_comment`,
`pillow_c_image_metadata_jpeg_icc_profile`,
`pillow_c_image_metadata_jpeg_exif`, and the shared
`pillow_c_image_metadata_xmp` export expose optional JPEG metadata bytes
already attached to an image handle. Each uses the same required-size pattern
as the PNG binary metadata exports: missing metadata returns `out_has_* == 0`,
`out_*_required == 0`, and success; present metadata sets `out_has_* == 1`,
returns `-1` for a size probe with a null output buffer, returns `-2` when the
buffer is too small, and copies the exact stored byte payload on success. The
facade maps these to `Info["comment"]`, `Info["icc_profile"]`, and
`Info["exif"]` as JPEG-specific `Buffer` values, while XMP maps through the
shared `Info["xmp"]` route for JPEG and PNG images. `META-001CW` changes no
signature: when JPEG open encounters multiple
valid EXIF APP1 payloads, the stored EXIF bytes are the first full
`Exif\0\0` payload followed by each subsequent payload after its repeated
six-byte header. Orientation parsing retains the first nonzero orientation, so
the covered orientation-3 then orientation-6 fixture exposes the combined blob
through `pillow_c_image_metadata_jpeg_exif` and reports orientation `3` through
`pillow_c_image_exif_orientation`. Keep-style non-explicit saves still omit
opened EXIF, while explicitly passing the combined blob writes one APP1 through
the existing metadata save exports. Arbitrary split points, malformed TIFF,
nested IFD merging, physical APP1 preservation, and non-RGB matrices remain
outside this behavior slice. Source/Release x64 DLL export counts remain `388`
/ `388`.
`META-002R` also changes no signature: JPEG open defers ICC APP2 assembly
until the first SOF and mirrors Pillow 11.3.0's full-payload
sort plus sorted-first-count rule. The covered file-order pair `2/3:B,
1/2:A` therefore exposes `AB` through
`pillow_c_image_metadata_jpeg_icc_profile`; implicit keep-style saves remain
ICC-free, and explicitly supplying `AB` writes one standard `1/1` APP2 through
the existing metadata save exports. Duplicate sequence numbers, missing-first
sets, zero sequence/count matrices, physical APP2 preservation, alternate
marker placement, malformed headers, and non-RGB fixtures remain outside this
behavior slice. Source/Release x64 DLL export counts remain `388` / `388`.
`META-002S` changes no signature and fixes that first-SOF boundary explicitly:
native open finalizes and clears pending ICC fragments at the first SOF and
does not collect later APP2 markers for public metadata. The covered standard
`1/1` APP2 immediately after baseline SOF0 therefore leaves
`pillow_c_image_metadata_jpeg_icc_profile` absent, while explicit ICC save
still writes one normal pre-SOF APP2 through the existing metadata exports.
Progressive/other SOF families, APP2 after SOS, multiple late fragments, late
EXIF/XMP/COM placement, physical marker preservation, malformed headers, and
non-RGB fixtures remain outside this behavior slice. Source/Release x64 DLL
export counts remain `388` / `388`.
`META-002T` changes no signature and makes malformed recognized ICC input fail
explicitly: before the first SOF, an APP2 payload that starts with the complete
12-byte `ICC_PROFILE\0` identifier but lacks either of the two required
sequence/count bytes causes native JPEG metadata parsing to fail. The covered
13-byte header-plus-sequence fixture therefore makes raw open return invalid
argument and facade explicit-JPEG open raise instead of silently dropping the
marker. Unrelated APP2 remains ignored, and APP2 after first-SOF finalization
remains ignored under `META-002S`. Header-only and legal empty-profile fixtures,
truncated JPEG segment framing, broader malformed matrices, and non-RGB cases
remain outside this behavior slice. Source/Release x64 DLL export counts remain
`388` / `388`.
`META-002U` changes no signature and makes the existing JPEG ICC metadata
export preserve legal empty-profile presence. Native JPEG ICC collation now
sets an explicit presence bit after the sorted fragment-count check succeeds,
even when the assembled byte count is zero; image open and metadata copy retain
that bit. `pillow_c_image_metadata_jpeg_icc_profile` therefore returns success
with `out_has_profile == 1` and `out_profile_required == 0` for the covered
pre-SOF `ICC_PROFILE\0,1,1` empty marker, while absent ICC remains `0/0`.
Facade `Info["icc_profile"]` maps the present-empty result to a zero-size
`Buffer`. The export signature, all other metadata-blob presence rules, and
implicit keep-save behavior are unchanged. Source/Release x64 DLL export
counts remain `388` / `388`.
The facade's bounded
`Pillow.Image.Exif` object uses `pillow_c_exif_entries_signed_rational_bytes`
before entering the existing JPEG metadata save exports, so explicit JPEG
`exif` object saves still pass raw bytes into the native marker writer and do
not introduce an AHK-side metadata rewrite. `META-001C` additionally has the facade call
`pillow_c_exif_ascii_tag` against opened JPEG/PNG `Info["exif"]` blobs for the
bounded Make tag `271`; `META-001D` extends that facade enumeration to bounded
common ASCII IFD0 tags `271`, `272`, `305`, and `306` without changing the
export contract. `META-001E` adds `pillow_c_exif_entries_typed_bytes` for
mixed string/integer object serialization and `pillow_c_exif_uint_tag` for
bounded scalar integer readback of tags `256`, `257`, `296`, and `531`.
`META-001F` adds `pillow_c_exif_entries_full_bytes` for mixed
string/integer/rational object serialization and `pillow_c_exif_rational_tag`
for bounded rational readback of tags `282` and `283`.
`META-001G` adds `pillow_c_exif_entries_short_array_bytes` for mixed
string/integer/rational/SHORT-array object serialization and
`pillow_c_exif_ushort_array_tag` for bounded SHORT-array readback of tag
`530`.
`META-001H` adds `pillow_c_exif_entries_byte_array_bytes` for mixed
string/integer/rational/SHORT-array/BYTE-array object serialization and
`pillow_c_exif_byte_array_tag` for bounded BYTE-array readback of tag `40091`.
`META-001I` adds `pillow_c_exif_entries_signed_rational_bytes` for mixed
string/integer/rational/SHORT-array/BYTE-array/SRATIONAL object serialization
and `pillow_c_exif_signed_rational_tag` for bounded SRATIONAL readback of tag
`37380`.
`META-001J` adds `pillow_c_exif_entries_undefined_bytes` for mixed
string/integer/rational/SHORT-array/BYTE-array/SRATIONAL/UNDEFINED object
serialization and `pillow_c_exif_undefined_tag` for bounded UNDEFINED readback
of tag `36864`.
`META-001K` adds no symbol; the existing BYTE-array object serialization and
readback route now also covers bounded `UserComment` tag `37510`, including
Pillow-compatible even-byte padding for the odd-length out-of-line
`b"comment"` payload.
`META-001L` adds `pillow_c_image_metadata_tiff_exif` as the TIFF-specific raw
EXIF metadata copy route for bounded IFD0 common ASCII readback. The facade
uses the copied blob to build the public bounded `Image.Exif` object for TIFF
without exposing `Info["exif"]`, matching Pillow's reopened TIFF object shape.
`META-001M` adds no symbol; the same TIFF metadata blob now also carries
bounded IFD0 ASCII tag `270` (`ImageDescription`) for public
`Image.GetExif()` / `Image.getexif()` readback. The source and Release x64 DLL
export counts remain `379` / `379`.
`META-001N` through `META-001AP` also add no symbols; the same TIFF metadata
blob now carries bounded reopened TIFF DPI tags `282`/`283`/`296`, extended
common ASCII tags `315`/`316`/`33432`, document/page ASCII tags `269`/`285`,
scalar integer tag `531`, core scalar integer tags `256`/`257`, and scalar
layout tags `258`/`259`/`262`/`273`/`278`/`279`/`284`, plus RGB layout array
tag `258`, RGBA/LA layout array tag `258`, scalar tags `277` and `338`,
SHORT-array `YCbCrSubSampling` tag `530`, numeric `SampleFormat` tag `339`,
position rational tags `286`/`287`, scalar `NewSubfileType` tag `254`, and
scalar `FillOrder` tag `266`, scalar `Thresholding` tag `263`, and scalar
`CellWidth`/`CellLength` tags `264`/`265`, scalar `SubfileType` tag `255`, and
scalar `MinSampleValue`/`MaxSampleValue` tags `280`/`281`, plus SHORT-array
`PageNumber` tag `297`, scalar `Predictor` tag `317`, ASCII `TargetPrinter`
tag `337`, SHORT-array `HalftoneHints` tag `321`, and scalar `InkSet` tag
`332`, scalar `NumberOfInks` tag `334`, SHORT-array `DotRange` tag `336`,
scalar `GrayResponseUnit` tag `290`, ASCII `InkNames` tag `333`,
SHORT-array `TransferFunction` tag `301`, SHORT-array
`GrayResponseCurve` tag `291`, for
public `Image.GetExif()` /
`Image.getexif()` readback. Source and Release x64 DLL export counts remain
`379` / `379`.
`META-001AQ` adds `pillow_c_exif_rational_array_tag`; the same TIFF metadata
blob now also carries bounded RATIONAL-array `WhitePoint` tag `318` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts are now `380` / `380`.
`META-001AR` adds no symbol; the same TIFF metadata blob now also carries
bounded RATIONAL-array `PrimaryChromaticities` tag `319` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `380` / `380`.
`META-001AS` adds no symbol; the same TIFF metadata blob now also carries
bounded palette SHORT-array `ColorMap` tag `320` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `380` / `380`.
`META-001AT` adds no symbol; the same TIFF metadata blob now also carries
bounded RATIONAL-array `YCbCrCoefficients` tag `529` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `380` / `380`.
`META-001AU` adds no symbol; the same TIFF metadata blob now also carries
bounded RATIONAL-array `ReferenceBlackWhite` tag `532` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `380` / `380`.
`META-001AV` adds no symbol; the same TIFF metadata blob now also carries
bounded SHORT-array `TransferRange` tag `342` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`380` / `380`.
`META-001AW` adds no symbol; the same TIFF metadata blob now also carries
bounded scalar `SMinSampleValue` and `SMaxSampleValue` tags `340`/`341` for
public `Image.GetExif()` / `Image.getexif()` readback, and source/Release x64
DLL export counts remain `380` / `380`.
`META-001AX` adds no symbol; the same TIFF metadata blob now also carries
bounded fax scalar tags `326`/`327`/`328` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001AY` adds no symbol; the same TIFF metadata blob now also carries
bounded fax option scalar tags `292`/`293` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001AZ` adds no symbol; the same TIFF metadata blob now also carries
bounded free block scalar tags `288`/`289` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001BA` adds no symbol; the same TIFF metadata blob now also carries
bounded tile shape scalar tags `322`/`323` for public `Image.GetExif()` /
`Image.getexif()` readback on the bounded native L TIFF route, and
source/Release x64 DLL export counts remain `381` / `381`.
`META-001BB` adds no symbol; the same TIFF metadata blob now also carries
bounded tile byte range scalar tags `324`/`325` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001BC` adds no symbol; the same TIFF metadata blob now also carries
bounded `JPEGTables` UNDEFINED tag `347` bytes for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001BD` adds no symbol; the same TIFF metadata blob now also carries
bounded `JPEGTables` BYTE tag `347` bytes for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001BE` adds no symbol; the same TIFF metadata blob now also carries
bounded `ExifVersion` UNDEFINED tag `36864` bytes for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BF` adds no symbol; the same TIFF metadata blob now also carries
bounded `DateTimeOriginal` ASCII tag `36867` strings for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BG` adds no symbol; the same TIFF metadata blob now also carries
bounded `DateTimeDigitized` ASCII tag `36868` strings for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BH` adds no symbol; the same TIFF metadata blob now also carries
bounded `SubSecTime` ASCII tag `37520` strings for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001BI` adds no symbol; the same TIFF metadata blob now also carries
bounded `SubSecTimeOriginal` ASCII tag `37521` strings for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BJ` adds no symbol; the same TIFF metadata blob now also carries
bounded `SubSecTimeDigitized` ASCII tag `37522` strings for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BK` adds no symbol; the same TIFF metadata blob now also carries
bounded `OffsetTime` ASCII tag `36880` strings for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001BL` adds no symbol; the same TIFF metadata blob now also carries
bounded `OffsetTimeOriginal` ASCII tag `36881` strings for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BM` adds no symbol; the same TIFF metadata blob now also carries
bounded `OffsetTimeDigitized` ASCII tag `36882` strings for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BN` adds no symbol; the same TIFF metadata blob now also carries
bounded `ExifIFD` pointer tag `34665` LONG offsets for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BO` adds no symbol; the same TIFF metadata blob now also carries
bounded `GPSInfo` pointer tag `34853` LONG offsets for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BP` adds no symbol; the same TIFF metadata blob now also carries
bounded `ImageUniqueID` ASCII tag `42016` strings for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BQ` adds no symbol; the same TIFF metadata blob now also carries
bounded `XPComment` BYTE-array tag `40092` bytes for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001BR` adds no symbol; the same TIFF metadata blob now also carries
bounded `XPAuthor` BYTE-array tag `40093` bytes for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001BS` adds no symbol; the same TIFF metadata blob now also carries
bounded `XPKeywords` BYTE-array tag `40094` bytes for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001BT` adds no symbol; the same TIFF metadata blob now also carries
bounded `XPSubject` BYTE-array tag `40095` bytes for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001BU` adds no symbol; the same TIFF metadata blob now also carries
bounded `FlashPixVersion` UNDEFINED tag `40960` bytes for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BV` adds no symbol; the same TIFF metadata blob now also carries
bounded `ColorSpace` scalar tag `40961` as integer `1` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BW` adds no symbol; the same TIFF metadata blob now also carries
bounded `PixelXDimension` scalar tag `40962` as integer `2` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BX` adds no symbol; the same TIFF metadata blob now also carries
bounded `PixelYDimension` scalar tag `40963` as integer `1` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BY` adds no symbol; the same TIFF metadata blob now also carries
bounded `RelatedSoundFile` ASCII tag `40964` as `SOUND000.WAV` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001BZ` adds no symbol; the same TIFF metadata blob now also carries
bounded `FlashEnergy` RATIONAL tag `41483` as `[9, 2]` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CA` adds no symbol; the same TIFF metadata blob now also carries
bounded `FocalPlaneXResolution` / `FocalPlaneYResolution` RATIONAL tags
`41486` / `41487` as `[300, 7]` / `[600, 11]` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001CB` adds no symbol; the same TIFF metadata blob now also carries
bounded `FocalPlaneResolutionUnit` scalar tag `41488` as `3` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CC` adds no symbol; the same TIFF metadata blob now also carries
bounded `SensingMethod` scalar tag `41495` as `2` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CD` adds no symbol; the same TIFF metadata blob now also carries
bounded `FileSource` UNDEFINED tag `41728` as a copied Buffer `[3]` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CE` adds no symbol; the same TIFF metadata blob now also carries
bounded `SceneType` UNDEFINED tag `41729` as a copied Buffer `[1]` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CF` adds no symbol; the same TIFF metadata blob now also carries
bounded `CustomRendered` scalar tag `41985` as `1` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CG` adds no symbol; the same TIFF metadata blob now also carries
bounded `ExposureMode` scalar tag `41986` as `2` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001CH` adds no symbol; the same TIFF metadata blob now also carries
bounded `WhiteBalance` scalar tag `41987` as `1` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001CI` adds no symbol; the same TIFF metadata blob now also carries
bounded `DigitalZoomRatio` RATIONAL tag `41988` as `[3, 2]` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CJ` adds no symbol; the same TIFF metadata blob now also carries
bounded `FocalLengthIn35mmFilm` scalar tag `41989` as `35` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CK` adds no symbol; the same TIFF metadata blob now also carries
bounded `SceneCaptureType` scalar tag `41990` as `3` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CL` adds no symbol; the same TIFF metadata blob now also carries
bounded `GainControl` scalar tag `41991` as `2` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001CM` adds no symbol; the same TIFF metadata blob now also carries
bounded `Contrast` scalar tag `41992` as `1` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001CN` adds no symbol; the same TIFF metadata blob now also carries
bounded `Saturation` scalar tag `41993` as `2` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001CO` adds no symbol; the same TIFF metadata blob now also carries
bounded `Sharpness` scalar tag `41994` as `2` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-001CP` adds no symbol; the same TIFF metadata blob now also carries
bounded `SubjectDistanceRange` scalar tag `41996` as `3` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CQ` adds no symbol; the same TIFF metadata blob now also carries
bounded `CameraOwnerName` ASCII tag `42032` as `owner` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CR` adds no symbol; the same TIFF metadata blob now also carries
bounded `BodySerialNumber` ASCII tag `42033` as `body42` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CT` adds no symbol; the same TIFF metadata blob now also carries
bounded `LensMake` / `LensModel` / `LensSerialNumber` ASCII tags `42035` /
`42036` / `42037` as `make42`, `model42`, and `lens42` for public
`Image.GetExif()` / `Image.getexif()` readback, and source/Release x64 DLL
export counts remain `381` / `381`.
`META-001CS` adds no symbol; the same TIFF metadata blob now also carries
bounded `LensSpecification` RATIONAL-array tag `42034` as
`[[1,2],[3,4],[5,6],[7,8]]` for public `Image.GetExif()` /
`Image.getexif()` readback, and source/Release x64 DLL export counts remain
`381` / `381`.
`META-002G` adds `pillow_c_image_metadata_tiff_icc_profile`; TIFF open now
copies bounded IFD0 `ICCProfile` tag `34675` bytes when stored as TIFF
UNDEFINED type `7`, exposes them as TIFF ICC profile metadata, and also
serializes tag `34675` into the existing TIFF EXIF blob for public
`Image.GetExif()` / `Image.getexif()` readback. Source/Release x64 DLL export
counts are now `381` / `381`.
`META-002H` adds no symbol; the same TIFF ICC metadata export and TIFF EXIF
blob route now also cover bounded native `I`, `F`, and `I;16` TIFF early-open
handles carrying IFD0 `ICCProfile` tag `34675` as TIFF UNDEFINED type `7`.
Source/Release x64 DLL export counts remain `381` / `381`.
`META-002I` adds no symbol; the same TIFF ICC metadata export and TIFF EXIF
blob route now also cover bounded RGB TIFF frame handles carrying selected-IFD
`ICCProfile` tag `34675` as TIFF UNDEFINED type `7`, including facade
`Seek(1)` metadata refresh. Source/Release x64 DLL export counts remain
`381` / `381`.
`META-002J` adds no symbol; `pillow_c_image_open_tiff_frame` now parses
selected nonzero little-endian `I;16` TIFF frames before WIC fallback and
reuses the same TIFF ICC metadata export plus TIFF EXIF blob for selected-IFD
`ICCProfile` tag `34675` as TIFF UNDEFINED type `7`. Source/Release x64 DLL
export counts remain `381` / `381`.
`META-002K` adds no symbol; `pillow_c_image_open_tiff_frame` now parses
selected nonzero little-endian numeric `I` and `F` TIFF frames before WIC
fallback and reuses the same TIFF ICC metadata export plus TIFF EXIF blob for
selected-IFD `ICCProfile` tag `34675` as TIFF UNDEFINED type `7`.
Source/Release x64 DLL export counts remain `381` / `381`.
`META-002L` adds no symbol; TIFF ICC parsing now accepts IFD0 `ICCProfile`
tag `34675` stored as TIFF BYTE type `1` in addition to the existing
UNDEFINED type `7` shape, and reuses the same TIFF ICC metadata export plus
TIFF EXIF blob. Source/Release x64 DLL export counts remain `381` / `381`.
`META-002M` adds no symbol; TIFF XMP parsing now accepts IFD0 XMP tag `700`
stored as TIFF BYTE type `1`, reuses `pillow_c_image_metadata_xmp` for
`Info["xmp"]` / `getxmp()`, and reuses the TIFF EXIF blob for public
`Image.GetExif()` / `Image.getexif()` tag `700` readback. Source/Release x64
DLL export counts remain `381` / `381`.
`META-002N` adds no symbol; the same raw XMP metadata export and TIFF EXIF
blob route now also cover bounded native `I;16`, `I`, and `F` TIFF early-open
handles carrying IFD0 XMP tag `700` as TIFF BYTE type `1`. Source/Release x64
DLL export counts remain `381` / `381`.
`META-002O` adds no symbol; the same raw XMP metadata export and TIFF EXIF
blob route now also cover selected nonzero little-endian `I;16`, `I`, and `F`
TIFF frame handles carrying selected-IFD XMP tag `700` as TIFF BYTE type `1`,
including facade `Seek(1)` metadata refresh. Source/Release x64 DLL export
counts remain `381` / `381`.
`META-002P` adds no symbol; TIFF XMP parsing now accepts IFD0 XMP tag `700`
stored as TIFF UNDEFINED type `7` in addition to the existing bounded BYTE
type `1` shape, and reuses the same raw XMP metadata export plus TIFF EXIF
blob route for `Info["xmp"]`, `getxmp()`, and public tag `700` readback.
Source/Release x64 DLL export counts remain `381` / `381`.

For `FMT-JPEG-003C` opened-comment keep preservation, non-explicit-metadata
JPEG save exports that already write through the native encoder patch an
opened handle's stored `jpeg_comment` back into the saved file after successful
encoding. This includes the keep-style qtables route used by opened RGB JPEG
`quality="keep"` / `qtables="keep"` facade saves, including the same route when
the caller also requests `progressive=True`. Explicit JPEG metadata exports
remain caller-controlled and do not implicitly reuse the stored comment, so
callers can still write no COM segment or a different COM segment by taking an
explicit metadata route. Opened JPEG ICC and EXIF payloads are likewise not
implicitly carried by keep-style non-explicit routes; callers must pass
`icc_profile` and `exif` explicitly to use the existing metadata exports. No
new export was added for this behavior, and the Release x64 DLL still exposes
`330` `pillow_c_*` exports.

`pillow_c_image_metadata_jpeg_qtable_count` and
`pillow_c_image_metadata_jpeg_qtable` expose JPEG DQT metadata already
attached to an image handle. The count export writes the number of stored
64-entry quantization tables. The table export accepts a zero-based table
index, an `int*` output buffer, and an output value count; callers must provide
space for at least `64` integers. Tables are returned in natural order with
values already de-zigzagged from the file payload. Missing or out-of-range
tables return `-3`, and short output buffers return `-2`. The facade uses
these exports only for bounded keep-style JPEG saves from opened images, so
the original table bytes can stay on the DLL path without AHK pixel loops.
`pillow_c_image_metadata_jpeg_subsampling` exposes parsed JPEG SOF sampling
metadata as Pillow's integer subsampling option for bounded RGB
`Y/Cb/Cr` component IDs and bounded CMYK `C/M/Y/K` component IDs. It writes
`0` for 4:4:4, `1` for 4:2:2, `2` for 4:2:0, and `-1` when the opened JPEG
has no recognized RGB or CMYK subsampling shape. The facade uses this with the
qtable metadata exports for bounded opened RGB JPEG `quality="keep"` /
`qtables="keep"` saves, and to resolve public `subsampling="keep"` before
entering those same native qtables or metadata subsampling save routes. For
`API-JPEG-001`, the same export resolves opened CMYK
`subsampling="keep"` before the facade enters
`pillow_c_image_save_jpeg_metadata_encode_options` with explicit ICC metadata
and an omitted public `comment`, so the opened COM/comment stays on the DLL
path without an AHK pixel loop. The
facade also normalizes Pillow JPEG subsampling preset aliases before entering
the existing integer ABI: `"4:4:4"`, `"web_high"`, `"web_very_high"`,
`"web_maximum"`, `"high"`, and `"maximum"` map to `0`; `"4:2:2"` maps to
`1`; and `"4:2:0"`, `"4:1:1"`, `"web_low"`, `"web_medium"`, `"low"`, and
`"medium"` map to `2`. This alias expansion adds no DLL export.
`FMT-JPEG-002C` separately normalizes Pillow quality preset strings by packing
their preset qtables and applying the preset subsampling before entering the
same qtables save exports with `quality == -1`; those quality presets override
caller `qtables` and `subsampling` options in the facade before the ABI call.

`pillow_c_image_metadata_resolution` exposes resolution metadata already attached to an image handle. It returns a boolean DPI flag plus double `dpi_x`/`dpi_y`, and returns JFIF version, density unit, and density pair when JPEG APP0 JFIF metadata is present. Non-JPEG images report `jfif=0`; images without usable DPI report `has_dpi=0` without clearing other handle metadata. PNG pHYs, JPEG JFIF density, and bounded TIFF IFD0 inch-based Resolution tags all report through this shared metadata ABI when present.

`pillow_c_image_metadata_hotspot` exposes XBM hotspot metadata already attached to an image handle. It returns a boolean hotspot flag plus integer `x` and `y` coordinates; images without hotspot metadata report `has_hotspot=0` and zero coordinates.

`pillow_c_image_open_tiff`, `pillow_c_image_open_tiff_frame`,
`pillow_c_image_frame_count_tiff`, `pillow_c_image_save_tiff`,
`pillow_c_image_save_tiff_options`,
`pillow_c_image_save_tiff_compression_options`, and
`pillow_c_image_save_tiff_frames` keep TIFF decode/encode inside the DLL. The
current TIFF path supports lossless `1`, `L`, `LA`, `P`, `RGB`, `RGBA`,
bounded `CMYK`, bounded numeric `I`/`F`, and bounded little-endian `I;16`
image handles for the verified
route. Native open converts
supported TIFF pixel formats into the DLL's public row-major byte order
through WIC; WIC BlackWhite / 1bpp indexed TIFF pixels map to internal mode
`1` with one byte per pixel using Pillow-visible `0` or `255` values, WIC
8bpp indexed TIFF pixels map to mode `P` with one palette index byte per
pixel, and WIC 32bpp CMYK pixels map to mode `CMYK` with four direct channel
bytes per pixel. `pillow_c_image_open_tiff` opens frame `0`;
`pillow_c_image_open_tiff_frame` opens a zero-based frame index and returns
`-3` for negative or out-of-range frames. Native open reads the original file
bytes and parses the TIFF header/selected IFD after successful decode; failure
to read those bytes returns `-3` rather than silently dropping metadata.
Selected-IFD TIFF EXIF, ICC, and XMP metadata are attached to the returned frame
handle. Nonzero little-endian `I;16`, `I`, and `F` frames are parsed from the
selected IFD before WIC fallback so their mode and raw sample bytes are
preserved for the covered ICC/XMP fixtures.
Orientation transforms, DPI resolution metadata, and palette ColorMap
replacement remain bounded to frame `0`.

For frame 0, the native chunky tiled parser recognizes uncompressed and
PackBits-compressed 8-bit `L`, `RGB`, `RGBA`, and `LA` tiles plus RGBX storage
exposed as public `RGB`. It validates the complete classic-TIFF tile grid and
LONG `TileOffsets`/`TileByteCounts`, decodes PackBits once per full tile inside
the DLL, clips right/bottom edge copies, and removes RGBX X bytes. The same
native route also recognizes the bounded little-endian BigTIFF shape: magic
`43`, offset size `8`, one 20-byte-entry IFD, scalar SHORT/LONG/LONG8 fields,
and LONG8 `TileOffsets`/`TileByteCounts` for uncompressed chunky `L`, `RGB`,
`RGBA`, and `LA`. BigTIFF allocation, 64-bit tile range checks, clipping, and
pixel copies remain inside the DLL. The native frame-count route returns `1`
for these bounded single-frame tiled shapes. TIFF LZW and Adobe Deflate tiled
payloads, planar-separate storage, multi-frame tiled files, and BigTIFF
Orientation combinations remain outside this route.

For frame-0 mode `P`, native open also requires and parses ColorMap tag `320`
as exactly `768` SHORT values in red/green/blue planes, converting each value
with `value >> 8` so Pillow-style `byte * 256` ColorMap entries round-trip
exactly instead of using WIC's quantized palette. Missing or malformed
ColorMap tags return `-3` before any palette bytes are read. For frame-0 mode
`LA`, native open
recognizes the bounded Pillow IFD0 shape with BitsPerSample `[8,8]`,
PhotometricInterpretation `1`, SamplesPerPixel `2`, and ExtraSamples `2`,
asks WIC for RGBA pixels, then keeps the gray and alpha bytes as public `LA`
storage inside the DLL. Orientation tag value `1` is attached to the existing
orientation metadata slot exposed by `pillow_c_image_exif_orientation`;
Orientation tag values `2`, `3`, `4`, `5`, `6`, `7`, and `8` apply
Pillow-compatible pixel transforms inside the DLL and leave that metadata slot
unset. Values `5`, `6`, `7`, and `8` update width/height/stride on the
returned handle because they swap dimensions. Native TIFF frame-0 open also
parses the bounded DPI tags `XResolution` (`282`) and `YResolution` (`283`) as
one RATIONAL each when `ResolutionUnit` (`296`) is SHORT value `2`, and
attaches those inch-based values to `pillow_c_image_metadata_resolution`.
Native TIFF frame-0 open also recognizes bounded little-endian 32-bit numeric
IFDs with BitsPerSample `32`, Compression `1`, `32773`, `5`, or `8`,
PhotometricInterpretation `1`, StripOffsets/StripByteCounts for one strip,
PlanarConfiguration `1`, and SampleFormat `2` or `3`; those paths decode
uncompressed, PackBits, TIFF LZW, or zlib Deflate strip bytes and return mode
`I` or mode `F` handles with preserved four-byte sample storage. The TIFF LZW
decoder follows Pillow/libtiff early-change code-width growth, including the
9-to-10-bit transition at the next free dictionary code `511`. Native TIFF
frame-0 open also recognizes the bounded little-endian public `I;16` IFD shape
with BitsPerSample `16`, Compression `1`, `32773`, `5`, or `8`,
PhotometricInterpretation `1`, StripOffsets/StripByteCounts for one strip,
RowsPerStrip equal to image height for compressed strips, PlanarConfiguration
`1`, no SamplesPerPixel, and no SampleFormat. It decodes uncompressed,
PackBits, TIFF LZW, or zlib Deflate strip bytes and returns mode `I;16` with
preserved two-byte sample storage, including Pillow-written LZW strips that
cross the early-change dictionary-width boundary. Broader TIFF
tag lifecycles, compression families beyond the bounded PackBits/LZW/Adobe
Deflate children, high-bit plugin modes, compression/mode combinations beyond
the verified RGB, CMYK, and numeric routes, non-frame-0 numeric special
parsing, and non-frame-0 native ColorMap parsing remain outside this bounded
route.

`pillow_c_image_frame_count_tiff` returns WIC's decoded frame count after
validating the container. Native save writes little-endian baseline TIFF bytes
through a DLL-owned IFD writer, including chained uncompressed IFDs for
`pillow_c_image_save_tiff_frames`. `pillow_c_image_save_tiff_options`
currently adds a bounded DPI option shape after the UTF-8 path: `has_dpi`,
`dpi_x`, and `dpi_y`; when `has_dpi != 0`, both DPI values must be positive
finite numbers and are written as IFD0 RATIONAL numerator/denominator pairs
with denominator `1`, plus `ResolutionUnit=2`.
`pillow_c_image_save_tiff_compression_options` adds an `int compression` after
the same DPI fields; compression `0` and `1` write uncompressed tag `1`,
compression `32773` writes tag `259` as PackBits and encodes each scanline
with PackBits, compression `5` writes tag `259` as LZW and encodes the
prepared single strip with TIFF clear/EOI codes plus MSB-first 9-to-12-bit
codes using Pillow/libtiff early-change width growth; at dictionary exhaustion
the encoder emits a clear code as soon as the next free code reaches `4094`,
then restarts at 9-bit codes. Compression `8` writes tag `259` as Adobe
Deflate and stores the
prepared single strip in a zlib stream with a `0x78 0x9C` header. The
compressed StripByteCounts value records the encoded byte length. Other
compression values return `-3`; mode `CMYK`, numeric modes `I` and `F`, and
mode `I;16` are accepted for the same bounded compression codes through this
single-image compression export. Mode `LA` is also covered by the same bounded
compression route.

The multipage export accepts a pointer array of image handles plus a frame
count and UTF-8 path, rejects null or empty sequences, and currently validates
every frame against the same `1`/`L`/`LA`/`P`/`RGB`/`RGBA`/`CMYK`/`I`/`F`/`I;16`
mode boundary. For mode `1`, TIFF save writes MSB-first packed strip bytes, omits
`BitsPerSample` and `SamplesPerPixel`, uses PhotometricInterpretation `1`, and
writes PlanarConfiguration `1`. For mode `P`, TIFF save requires RGB palette
metadata, writes BitsPerSample `8`, Compression `1`, bounded PackBits, bounded
LZW, or bounded Adobe Deflate, PhotometricInterpretation `3`, raw index strip
bytes, RowsPerStrip/StripByteCounts, PlanarConfiguration `1`, omits
`SamplesPerPixel`, and writes ColorMap tag `320` as `768` SHORT values padded
to 256 entries per color plane using `palette_byte * 256`. For mode `LA`, TIFF
save writes Pillow-style BitsPerSample count `2` inline as `[8,8]`,
Compression `1` or bounded PackBits/LZW/Adobe Deflate for the single-image
compression export, PhotometricInterpretation `1`, SamplesPerPixel `2`,
RowsPerStrip/StripByteCounts, PlanarConfiguration `1`, and ExtraSamples `2`,
with raw or encoded `LA` strip bytes. For mode `CMYK`, TIFF save writes
BitsPerSample `[8,8,8,8]`, Compression `1` or bounded PackBits/LZW/Adobe
Deflate for the single-image compression export, PhotometricInterpretation
`5`, SamplesPerPixel `4`, RowsPerStrip/StripByteCounts, PlanarConfiguration
`1`, and raw or encoded `CMYK` strip bytes. For modes `I` and `F`, TIFF save
writes BitsPerSample `32`, Compression `1` or bounded PackBits/LZW/Adobe
Deflate for the single-image compression export, PhotometricInterpretation
`1`, RowsPerStrip/StripByteCounts, PlanarConfiguration `1`, SampleFormat `2`
for `I` or `3` for `F`, omits SamplesPerPixel, and stores direct or encoded
four-byte sample strip bytes. For mode `I;16`, TIFF save writes BitsPerSample
`16`, Compression `1` or bounded PackBits/LZW/Adobe Deflate for the
single-image compression export, PhotometricInterpretation `1`,
RowsPerStrip/StripByteCounts, PlanarConfiguration `1`, omits SamplesPerPixel
and SampleFormat, and stores direct or encoded two-byte little-endian sample
strip bytes.
Other supported modes write
baseline width/height,
8-bit bits-per-sample, compression `1` or bounded PackBits/LZW/Adobe Deflate
for the single-image compression export, photometric interpretation, strip
offset/count, rows-per-strip, samples-per-pixel, optional
`XResolution`/`YResolution`/`ResolutionUnit` for the single-image options
paths, planar configuration for multichannel modes, and `ExtraSamples=2` for
`RGBA`. Arbitrary TIFF tag dictionaries, JPEG-in-TIFF compression, predictor
variants, non-inch resolution units, and BigTIFF return `-3` or remain outside
this ABI surface.

`pillow_c_image_open_gif`, `pillow_c_image_open_gif_frame`,
`pillow_c_image_frame_count_gif`, `pillow_c_image_gif_metadata`,
`pillow_c_image_gif_metadata_ex`, `pillow_c_image_save_gif`,
`pillow_c_image_save_gif_options`, `pillow_c_image_save_gif_comment`,
`pillow_c_image_save_gif_comment_options`,
`pillow_c_image_save_gif_animation`,
`pillow_c_image_save_gif_animation_options`,
`pillow_c_image_save_gif_animation_metadata_options`, and
`pillow_c_image_save_gif_animation_comment`, and
`pillow_c_image_save_gif_animation_comment_metadata_options` keep GIF
decode/encode and basic animation metadata inside the DLL. Frame `0` opens as
mode `P` with
RGB palette metadata preserved. Later frames first try native GIF block parsing
and LZW decode to compose local image rectangles onto the logical canvas with
transparency and disposal handling. The canvas remains RGB for ordinary GIF
fixtures, but upgrades to RGBA when frame 0 actually uses its Graphic Control
Extension transparency index so later composited frames preserve Pillow's
transparent canvas pixels.

Verified read-side coverage includes disposal `1` preservation, disposal `2`
restoration to the logical-screen background color, disposal `3` restoration to
the pre-frame canvas for local rectangles that the next frame does not
overwrite, transparent local-rectangle pixels that preserve the existing canvas
while drawing, and the bounded mixed first-RGB / second-RGBA caller-transparency
fixture whose second frame reopens as mode `RGBA` with bytes
`[0,0,0,0, 1,2,3,255]` and no per-frame transparency metadata. Unsupported
parser cases fall back to WIC frame conversion. `pillow_c_image_open_gif_frame`
uses zero-based indexes and returns `-3` for negative or out-of-range frames.
`pillow_c_image_gif_metadata` parses GIF blocks directly and returns frame
duration in milliseconds, NETSCAPE loop count, Graphic Control Extension
disposal method, and logical-screen background index.
`pillow_c_image_gif_metadata_ex` preserves that ABI and adds the Graphic
Control Extension transparent color index as an optional integer output.
Missing optional values return `-1` except disposal, which defaults to `0`.

Native single-frame save writes mode `P` images with an attached RGB palette
directly; mode `L` inputs and `RGB` inputs with no more than 256 unique colors
are first exact-quantized into a native `P` temporary. `RGB` inputs with more
than 256 unique colors use the current bounded weighted median-cut fallback,
preserving the exact path for smaller palettes and targeting approximate
reopened RGB pixels rather than byte-identical Pillow palette order. Mode
`RGBA` inputs are accepted for the verified exact-color path when the effective
palette fits in 256 entries: `alpha == 0` pixels share one transparent palette
index, the first transparent pixel's RGB becomes that palette entry, and all
nonzero alpha values are treated as opaque RGB. When an RGBA single-frame save
exceeds 256 effective colors, the current bounded fallback reserves one
transparent palette slot if any fully transparent pixel exists, quantizes
non-transparent RGB pixels into the remaining 255 or 256 slots with the same
deterministic weighted median-cut style path, maps all `alpha == 0` pixels to
the transparent index, and keeps partial alpha opaque. RGBA saves without fully
transparent pixels write no GIF transparency extension.

`pillow_c_image_save_gif_options` currently adds P-mode single-frame
transparency: nonzero `has_transparency` writes a GIF89a Graphic Control
Extension with packed flag `0x01`, zero delay, and `transparency & 0xff` as
the transparent color index. The no-transparency options path delegates to
`pillow_c_image_save_gif`.
`pillow_c_image_save_gif_comment` writes bounded single-frame GIF comment
metadata. `pillow_c_image_save_gif_comment_options` composes that same comment
route with P-mode integer transparency; when transparency is enabled it writes
the comment extension before the GCE, matching the bounded Pillow 11.3.0
oracle for `comment=b"hello"` and `transparency=1`.

`pillow_c_image_save_gif_animation` writes same-size mode `P` sequences plus
the bounded same-size `L`, `LA`, `RGB`, and `RGBA` animation surfaces. `L` and
`RGB` animation input is quantized into temporary DLL-owned `P` images with the
existing exact/fallback native quantizer before the common GIF animation
differencing/writer path runs. `LA` animation input uses a luminance-only exact
temporary `P` quantizer and ignores alpha for the covered Pillow 11.3.0
fixture.
RGBA animation input first uses the exact animation quantizer with transparency
index `0` reserved when source alpha `0` appears; if the frame exceeds the
exact palette budget, it falls back to the existing GIF-specific weighted
median-cut RGBA quantizer before the same writer path. The writer carries a
per-frame source-transparency flag so opaque RGBA frames do not receive
spurious transparency GCEs, while transparent RGBA frames write
Pillow-compatible composited output through the existing animation path.
Optional duration and disposal arrays may have length `1` or frame count; the
writer emits an optional NETSCAPE loop extension and merges visually identical
consecutive frames by accumulating duration. The first P frame uses the global
color table from the first image. Later changed P frames may use their own RGB
palette as a local color table, so P-frame animations with different per-frame
palettes can preserve each frame's colors. Delta rectangles and unchanged-pixel
transparency decisions compare resolved palette RGB values rather than raw
palette indexes; for the verified optimized paths, changed sub-rectangles are
written as local image descriptors with local color tables and an unused
transparency index for unchanged pixels. The GIF LZW encoder follows the
reader-compatible code-size transition by growing code size after
`next_code > (1 << code_size)`. It rejects unsupported modes outside mode `P`
and bounded `L`/`LA`/`RGB`/`RGBA`, size mismatches, invalid palettes, invalid
disposal values, and unsupported loop counts with stable status codes. Facade
`ImageSequence.Iterator` coverage now includes Pillow's live seek-state frame
references over a complex transparent local-rectangle GIF fixture.
Caller-provided animation transparency is covered for optimized and
`optimize=False` bounded P-mode fixtures, and caller `background` is covered
for the bounded logical-screen background-byte slice. Caller-provided
transparency is also covered for the bounded exact RGBA animation fixture:
explicit `transparency=2` no longer rejects, the RGBA frames are quantized to
temporary P images, source-alpha index `0` remains separate from the caller
transparency index, and optimized local-palette compaction reserves an
out-of-source-palette transparency slot instead of failing.
The bounded post-`disposal=2` optimized parity path is covered: when caller
transparency is known, the native writer can re-diff the following frame
against the restored background instead of forcing full size on the selected
padded-palette `3x1` fixture. The short-palette variant is also covered: when
Pillow keeps the restored background comparison in P-index space because the
caller transparency index is outside the two-entry source palette, the native
writer emits the following frame full width and preserves the caller
transparency GCE. The restored-background full-frame variant is covered too:
when the source frame after a `disposal=2` frame equals the restored
transparent/background state, the optimized writer emits a full `3x1` local
frame with no transparency GCE and zeros unused local-palette entries before
computing the local color-table size, matching Pillow's all-black four-entry
local palette on the bounded fixture. The larger restored-background
index-comparison variant is also covered: when a
`4x2` optimized animation keeps a persistent first-frame red pixel and the
current frame's optimized palette matches the optimized first/global palette,
the writer compares against the caller transparency index in P-index space and
emits a full local frame matching Pillow's descriptor shape. The matching
mixed-unused-palette variant is covered as well: when raw first/third palettes
differ only in unused entries but their optimized full-frame palettes match,
the same metadata-options ABI keeps the restored-background comparison in
P-index space and emits Pillow's full `4x2` local frame with no transparency
GCE. The sparse optimized local-palette variant is covered as well: when caller
transparency is present and a changed
local rectangle uses sparse source palette indexes, the writer preserves local
index `0` plus the caller transparency index, compacts used source colors into
the lowest non-reserved local indexes, and remaps frame pixels before GIF LZW
encoding. This matches the bounded `disposal=2`, `background=1`,
`transparency=2` Pillow fixture where a blue source color at palette index `3`
is written as local index `1` in a four-entry local color table, and a later
red source color at palette index `1` is written through the same compact
shape while reopened RGB bytes are preserved. The default optimized sparse
palette variant is covered too: when no caller transparency/background is
provided, the writer reserves a generated local transparency index after
compacting used source colors, so the bounded fixture writes source index `3`
blue as local index `1` and generated transparency index `2`, matching Pillow.
The default post-`disposal=2` sparse-palette follow-up is also covered: after
writing that generated-transparent sparse blue local frame, the following
full-width local frame has no transparency GCE and zeros unused sparse local
palette entries before encoding, matching Pillow's bounded `3x1` fixture.
The matching `include_color_table=True` sparse-palette matrix is covered under
the same animation options export: the writer uses the optimized first/global
palette for the global color table and forced first local color table, then
keeps the later compact local palettes and unused-entry zeroing described
above.
The bounded `optimize=False` caller-transparency payload matrix is covered:
the writer still emits caller transparency GCEs, 4-entry color tables, and
`LzwMin=8`, but does not replace unchanged local-frame pixels with the
transparent index when optimization is disabled. The selected `3x1` fixture
therefore keeps frame payload indices `[1,0,1]` instead of `[1,2,1]`, matching
Pillow 11.3.0.
The bounded `optimize=False`, post-`disposal=2`, caller `background=1` plus
`transparency=2` matrix is covered as well: when the following source frame is
entirely the caller transparency index, the writer emits a full-width frame
with GCE transparency `2`, decoded payload indices `[2,2,2]`, `LzwMin=8`, and
no local color table, reusing the global table like Pillow 11.3.0.
The bounded `disposal=3` plus
caller-transparency matrix is also covered for the same P-mode animation
surface: the native writer preserves Pillow's current source-frame bbox
behavior on the selected `3x1` fixture and writes animation frame `LzwMin=8`
instead of the earlier native minimum `2`. These were behavior corrections
under existing GIF animation exports. The exact-color L/LA/RGB/RGBA, bounded
mixed RGB/RGBA, and bounded lossy mixed RGB/RGBA animation quantization slices
also reused the same export, added no ABI symbol, and the Release x64 DLL now
exposed `327` `pillow_c_*` exports at that point. For the bounded L fixture, each grayscale
frame is exact-quantized to a temporary P image and reopens with preserved
grayscale bytes plus duration and loop metadata. For the bounded LA fixture,
each grayscale+alpha frame is quantized from luminance bytes only and reopens
with preserved grayscale bytes plus duration and loop metadata; broader LA
alpha/transparency semantics remain future GIF quantization surfaces. For the
bounded mixed lossy fixture, each non-P frame is independently quantized to a
temporary P image before the existing writer runs. For the bounded exact RGBA
caller-transparency fixture, `transparency=2` is preserved as the second-frame
GCE transparency while the source alpha-0 pixel reopens opaque like Pillow
11.3.0. The bounded mixed first-RGB / second-RGBA caller-transparency fixture
is also covered: explicit `transparency=1` is preserved on both frame GCEs and
the second local palette head matches Pillow's `[1,2,3, 255,0,0]` ordering;
the read-side follow-up keeps frame 1 on an RGBA composited canvas with the
transparent frame-0 pixel preserved.
Broader caller-transparency matrices beyond that mixed fixture, larger lossy
animation palette stability, and full Pillow quantize algorithm parity remain
future ABI surfaces.

`pillow_c_image_save_gif_animation_options` preserves the animation save
argument list and adds two tri-state integers after `disposal_count`:
`include_color_table` and `optimize`, where `-1` means unset/default, `0`
means false, and `1` means true. For the covered P-mode animation fixtures,
`include_color_table=True` forces frame 0 to write a local color table,
`include_color_table=False` keeps frame 0 on the global color table without
suppressing later bbox-frame local tables, `optimize=True` keeps the existing
optimized local-rectangle transparency behavior, and `optimize=False` writes
4-entry global/local color tables with `LzwMin=8` for the bounded 2-color
fixtures while disabling unchanged-pixel transparency substitution, including
when caller transparency is supplied. In the covered `include_color_table=True`,
sparse-palette, post-`disposal=2` matrix, no new ABI argument is needed: the
same tri-state option route writes Pillow-compatible optimized global and
forced first local palettes, then compact later local color tables.

`pillow_c_image_save_gif_animation_metadata_options` extends the same argument
list with `has_transparency` and `transparency` after the tri-state animation
options. `has_transparency` must be `0` or `1`; when it is `1`,
`transparency` must be in `0..255`. For the covered optimized P-mode animation
fixture, caller transparency replaces the native unused-index transparency on
later local frames, while frame 0 keeps Pillow's no-transparency GCE default.
For the covered P-mode comment-plus-transparency animation fixture, frame 0
does write a transparency GCE when the source P pixels actually contain the
caller transparent index. This source-P check is intentionally not applied to
temporary RGBA quantization reserve slots.
If the caller index already exists in the later frame palette, the writer pads
one extra zero-color palette entry before computing the local color-table size,
matching the covered Pillow 11.3.0 4-entry local table. With
`optimize=False`, caller transparency is covered for the same bounded fixture:
frame 0 and changed later frames write a transparency GCE, color tables stay at
4 entries for the selected palettes, `LzwMin=8` is preserved, and unchanged
payload pixels stay encoded as opaque source indexes. The verified
optimized/default `disposal=3` plus caller-transparency fixture also preserves
`LzwMin=8` for all animation frames. The restored-background post-`disposal=2`
fixture where the next source frame equals the restored background keeps the
same metadata-options ABI shape while emitting a full local frame with no
transparency GCE and an all-black optimized local palette. The mixed-unused
palette post-`disposal=2` fixture also keeps the same metadata-options ABI
shape; the native writer now compares optimized palette contents rather than
requiring raw source palette equality before taking the P-index restored
background path. Sparse optimized local-palette compaction also uses this
existing metadata shape when no caller background is supplied; when no caller
transparency is supplied either, the writer generates the compact local
transparency index after remapping used source colors. Broader disposal
interactions, broader lossy animation palette stability, and full Pillow
quantize parity remain future ABI surfaces.

`pillow_c_image_save_gif_animation_background_options` preserves the same
argument list as the metadata-options export and appends `has_background` plus
`background`. When `has_background == 1`, `background` must be in `0..255` and
is written to the GIF logical-screen descriptor background byte. For the
covered bounded `3x1` fixture, this matches Pillow 11.3.0's logical-screen
background metadata without changing the already-covered optimized local-frame
geometry. Local Pillow 11.3.0 source/probes did not show logical-screen
`background` alone driving next-frame bbox optimization on the covered
fixtures. The bounded padded-palette and short-palette post-`disposal=2`
transparency-aware re-diff paths are now covered. The same existing export also
covers the bounded sparse-palette `disposal=2`, `background=1`,
`transparency=2` matrix: optimized later local frames compact sparse palette
indexes to Pillow-compatible four-entry local color tables while remapping the
encoded pixels inside the DLL. For `optimize=False`, the same export covers
the all-transparent post-`disposal=2` follow-up frame described above, including
Pillow-compatible omission of the local color table. For the matching default
optimized all-transparent post-`disposal=2` matrix, the same export now emits
Pillow's full-width local transparent frame with a 4-entry all-black local color
table, GCE transparency `0`, decoded indices `[0,0,0]`, and `LzwMin=8`, while
the no-background restored-frame matrix remains on the older no-transparency-GCE
path. `FMT-GIF-004Q` adds coverage, without ABI change, for the sparse-palette
`background=1` and no-caller-transparency post-`disposal=2` matrix where the
existing writer already matches Pillow's generated-transparent compact blue
frame and full-width red/background follow-up frame. No new ABI symbol was added
for this behavior; the Release x64 DLL exposed `327` `pillow_c_*` exports at
that point.
`FMT-GIF-004R` changes the existing GIF animation save behavior without adding
an export: when optimization collapses a multi-frame input sequence to a single
output frame and the caller supplied more than one disposal value,
`pillow_c_image_save_gif_animation*` returns `-3` before writing the file. This
matches Pillow 11.3.0's all-identical-frame collapse boundary for list-valued
`disposal`; scalar disposal and partial-collapse duration merging remain
accepted on their existing paths. Current source and Release x64 DLL export
counts remain `341` `pillow_c_*` names.

`FMT-GIF-004S` and `FMT-GIF-002F` also change existing GIF behavior without adding an export:
`save_gif_animation_image` preserves explicit caller transparency for the first
quantized RGB animation frame in the bounded mixed RGB/RGBA animation fixture,
and `open_gif_frame_image` maps frame-0 GIF GCE transparency into palette alpha
so P-mode frame 0 converts to RGBA with the caller-transparent pixel alpha
`0`. `open_gif_composited_frame_image` now also upgrades that composited canvas
to RGBA only when frame 0 actually used the GCE transparency index, so frame 1
reopens as Pillow-compatible RGBA with transparent canvas pixels preserved and
no frame-level `Info["transparency"]`. Broader disposal/background/mixed-mode
matrices remain future ABI surfaces.

`pillow_c_image_linear_gradient` and `pillow_c_image_linear_gradient_into` implement Pillow's fixed-size top-to-bottom `256x256` `Image.linear_gradient` generator for modes `1`, `L`, and `P`. Internal storage writes the row value `0..255` across each row; mode `1` still uses the existing raw encoder when callers request Pillow's bit-packed external bytes. Unsupported modes return `-3`, and `_into` target shape or mode mismatches return `-5`.

`pillow_c_image_radial_gradient` and `pillow_c_image_radial_gradient_into` implement Pillow's fixed-size `256x256` `Image.radial_gradient` generator for modes `1`, `L`, and `P`. Pixel values are generated from the Pillow-compatible center-distance formula and clipped to `0..255`; mode `1` uses the same internal unpacked storage and external bit-packed raw encoder as other native mode `1` paths. Unsupported modes return `-3`, and `_into` target shape or mode mismatches return `-5`.

`pillow_c_image_effect_mandelbrot` implements Pillow's `Image.effect_mandelbrot(size, extent, quality)` generator and returns a mode `L` image. It accepts non-empty or empty dimensions through the same native handle model, rejects reversed extents and `quality < 2` with `-3`, and otherwise uses Pillow's escape-time formula for deterministic byte output.

`pillow_c_image_effect_noise` implements Pillow's `Image.effect_noise(size, sigma)` generator and returns a mode `L` image. It accepts non-empty or empty dimensions through the same native handle model and follows Pillow 11.3.0's C core: C `rand()` drives the Marsaglia polar Gaussian generator, `sigma` is narrowed to `float`, and output bytes use Pillow's `CLIP8(128 + sigma * value)` semantics.

`pillow_c_image_effect_spread` implements Pillow's `Image.effect_spread(distance)` for existing native image handles. It returns a same-mode, same-size image, preserves RGB palette metadata for mode `P`, rejects negative distances with `-3`, and follows Pillow 11.3.0's C core: `distance == 0` copies the source bytes, otherwise C `rand()` chooses the source-neighborhood offset and output pixels are assigned from the original source image.

`pillow_c_image_logical_and`, `pillow_c_image_logical_or`, `pillow_c_image_logical_xor`, and their `_into` variants implement Pillow `ImageChops.logical_*` for mode `1` images only. They return `-3` for other modes, use overlapping output dimensions for mismatched sizes, and allow empty width or height outputs. The shared ImageChops binary target validator refreshes attached readonly `frombuffer` source views before these exports read source pixels.

The non-logical `ImageChops` binary operations (`difference`, `multiply`, `screen`, `lighter`, `darker`, `soft_light`, `hard_light`, `overlay`, `add`, `subtract`, `add_modulo`, and `subtract_modulo`) use the same overlapping-output rule and operate channel-generically on matching source modes. The current verified byte-storage modes are `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. Modes `I` and `F` return `-3`, matching Pillow's public wrong-mode rejection instead of treating four-byte numeric samples as independent byte channels. These exports and their `_into` variants refresh attached readonly `frombuffer` source views before reading source pixels on supported modes, so facade `ImageChops.Difference(...)`, `ImageChops.Multiply(...)`, and the same helper-backed binary operations sample current caller bytes without an AHK-side pre-refresh while source views remain readonly. The numeric wrong-mode behavior change added no export; source and Release x64 DLL export counts remain `371` / `371`.

`pillow_c_image_chops_invert` and `pillow_c_image_chops_invert_into` apply Pillow `ImageChops.invert` channel-generically to the source storage and refresh an attached readonly `frombuffer` source view before reading pixels.

`pillow_c_image_invert` and `pillow_c_image_invert_into` implement `ImageOps.invert` for modes `1`, `L`, and `RGB`. `pillow_c_image_posterize` and `pillow_c_image_solarize`, plus their `_into` variants, follow Pillow's `_lut` boundary for modes `L` and `RGB`; mode `1`, `LA`, and `RGBA` return `-3`. The ImageOps LUT exports refresh an attached readonly `frombuffer` source view before reading source pixels, so facade `ImageOps.Invert(...)`, `ImageOps.Posterize(...)`, and `ImageOps.Solarize(...)` sample current caller bytes without an AHK-side pre-refresh while the source view remains readonly.

`pillow_c_image_blend` and `pillow_c_image_blend_into` refresh attached readonly `frombuffer` left/right source views before blending pixels for supported byte-storage modes, matching Pillow 11.3.0's `Image.blend(...)` read behavior while leaving inputs readonly and returning or reusing owned target storage. Modes `I` and `F` return `-3`, matching Pillow's public wrong-mode rejection instead of blending four-byte numeric samples as independent byte channels. This numeric wrong-mode behavior change added no export; source and Release x64 DLL export counts remain `371` / `371`.

`pillow_c_image_composite` and `pillow_c_image_composite_into` blend through mask modes `1`, `L`, `LA`, and `RGBA`; `LA` and `RGBA` use their alpha band. Composite refreshes attached readonly `frombuffer` source, target-source, and mask views before reading pixels, matching Pillow 11.3.0's `Image.composite(...)` read behavior while leaving inputs readonly and returning or reusing owned target storage.

`pillow_c_image_offset` and `pillow_c_image_offset_into` refresh attached readonly `frombuffer` source views before wrapping pixels, matching Pillow 11.3.0's `ImageChops.offset(...)` read behavior while leaving the input readonly and returning or reusing owned target storage. This is an existing-export behavior correction; no ABI symbol was added and the Release x64 DLL still exposes `341` `pillow_c_*` exports.

`pillow_c_image_alpha_composite_rgba_in_place` implements Pillow's instance `Image.alpha_composite` geometry for RGBA images. It mutates the destination handle, accepts destination coordinates and a source rectangle, clips visible pixels to the destination, treats source pixels outside the source image as transparent, and rejects negative source coordinates with `-3`.

`pillow_c_image_paste` and `pillow_c_image_paste_masked` mutate the target in place, clip the source rectangle to the target bounds, convert the source to the target mode when needed, and the masked export blends through a same-size source mask. Mask modes `1`, `L`, `LA`, and `RGBA` are accepted; `LA` and `RGBA` use their alpha band. Paste mutators detach active readonly `frombuffer` buffer views on the target before writing.

`pillow_c_image_paste_color` mutates the target in place by filling a four-coordinate rectangle with a caller-packed target-mode color. The optional mask must match the unclipped rectangle size and may use `1`, `L`, `LA`, or `RGBA`; masked calls clip the destination rectangle and sample the matching mask offset after clipping. Like image paste, color paste detaches active readonly `frombuffer` buffer views on the target before writing. The AHK facade owns Pillow-style scalar/tuple color parsing before making this single native call.

`pillow_c_image_getpixel` refreshes an attached readonly `frombuffer` buffer view before reading native storage. The existing export shape is unchanged; facade `Image.GetPixel()` relies on this native refresh and no longer performs a wrapper-level pre-refresh.

`pillow_c_image_crop` and `pillow_c_image_crop_into` refresh an attached readonly `frombuffer` source view before copying crop pixels. The existing export shapes are unchanged; facade `Image.Crop()` relies on this native refresh and does not perform a wrapper-level pre-refresh. Crop results are owned images and `_into` writes into caller-provided owned target storage while leaving the source readonly view attached.

`pillow_c_image_copy` and `pillow_c_image_copy_into` refresh an attached readonly `frombuffer` source view before copying full-image pixels. The existing export shapes are unchanged; facade `Image.Copy()` relies on this native refresh and does not perform a wrapper-level pre-refresh. Copy results are owned images and `_into` writes into caller-provided owned target storage while leaving the source readonly view attached.

`pillow_c_image_get_channel`, `pillow_c_image_get_channel_into`, and `pillow_c_image_split_bands` refresh an attached readonly `frombuffer` source view before copying band pixels. The existing export shapes are unchanged; facade `Image.GetChannel()` and `Image.Split()` rely on this native refresh and do not perform wrapper-level pre-refresh. Returned band images are owned images and `_into` writes into caller-provided owned target storage while leaving the source readonly view attached.

`pillow_c_image_merge_bands` and `pillow_c_image_merge_bands_into` refresh attached readonly `frombuffer` source band views before interleaving pixels into the target mode. The existing export shapes are unchanged; facade `Image.Merge()` relies on this native refresh and does not perform wrapper-level pre-refresh. Merged images are owned images and `_into` writes into caller-provided owned target storage while leaving each source band readonly view attached.

`pillow_c_image_autocontrast` and `pillow_c_image_autocontrast_into` accept an optional mode `1` or `L` mask handle after the ignore list arguments, followed by a `preserve_tone` integer flag. A null mask keeps full-image histogram behavior. The autocontrast exports refresh attached readonly `frombuffer` source views, and refresh the optional mask view when provided, before building the histogram-derived LUT; facade ImageOps autocontrast relies on this native refresh and does not perform wrapper-level pre-refresh. Numeric modes `I` and `F` return `PILLOW_C_INVALID_ARGUMENT`; `MODE-NUM-001O` maps that status to Pillow-style `not supported for mode ...` or `image has wrong mode` messages in the facade. The equalize/autocontrast readonly-refresh and numeric wrong-mode corrections use existing exports; no ABI symbol was added and the Release x64 DLL still exposes `378` `pillow_c_*` exports.

`pillow_c_image_convert_mode` and `pillow_c_image_convert_mode_into` cover the verified `1`/`L`/`LA`/`RGB`/`RGBA`/`P`/`CMYK` conversion paths used by the facade, bounded `RGB <-> YCbCr`, `1/L/LA/RGBA/RGBX -> YCbCr`, `1/L/LA/RGB/RGBA/I/F -> RGBX`, `RGBX -> CMYK`, `1/L/LA/RGB/RGBA/RGBX/YCbCr/CMYK/HSV -> I/F`, `YCbCr -> L/LA`, `RGB <-> HSV`, and `1/L/LA/RGBA/RGBX/CMYK/I/F -> HSV` color-mode paths, `1/L/LA/I/F/RGB/RGBA/RGBX -> LAB`, plus the bounded numeric `I -> L` and `F -> L` paths. RGB-like LAB targets use the statically linked LittleCMS 2.17 sRGB-to-Lab2 transform and ignore alpha/X. The convert-mode exports, including the dither variants, refresh an attached readonly `frombuffer` source view before reading source storage, so facade `Image.Convert()` samples current caller bytes without an AHK-side pre-refresh while the source view remains readonly. Numeric conversion truncates toward zero, clamps to `0..255`, maps `NaN` and `-Inf` to `0`, maps `+Inf` to `255`, preserves empty `(0, n)` or `(n, 0)` output shapes, and lets `_into` reuse matching caller-provided targets. Numeric RGBX targets replicate the clipped byte into RGB and write X=`255`. Quantizing targets such as `CMYK -> P` remain unsupported. `LA` preserves alpha as the second band; alpha is ignored for `RGBA`/`LA -> CMYK`, `RGBA -> YCbCr`, `LA -> YCbCr`, and `RGBA -> RGBX`, matching Pillow. Direct `1/L/LA -> RGBX` promotes logical mode-1 samples to 0/255, preserves LA alpha as X, and writes X=`255` for mode 1 and L; RGB/RGBA copies RGB and writes X=`255`. Direct `L/LA -> YCbCr` writes `[L,128,128]` rather than routing through RGB lookup rounding; mode 1 uses the same direct shape after logical nonzero-to-255 promotion. `CMYK -> RGB` uses Pillow's black-channel-scaled RGB conversion before luma or alpha insertion, and `P -> CMYK` converts through the handle palette.

`pillow_c_image_convert_mode_dither(source, target_mode, dither, out_image)` and `pillow_c_image_convert_mode_dither_into` extend convert-mode with a Pillow dither argument for mode `1` targets from `L`/`LA`/`RGB`/`RGBA`/`RGBX`/`CMYK` sources: dither `0` (`NONE`) thresholds luma at `128`, dither `3` (`FLOYDSTEINBERG`, Pillow's default) applies Floyd-Steinberg error diffusion, and other dither values return `-3`. RGBX ignores X in both paths. Non-mode-`1` targets route through the plain convert-mode behavior. The facade maps `Image.Convert("1", dither)` to this export.

`pillow_c_image_quantize(source, colors, out_image)` and `pillow_c_image_quantize_into` implement the bounded exact-color `Image.quantize()` path for `RGB`/`L` sources whose unique colors fit `colors` (`1..256`), returning a mode `P` image. `pillow_c_image_quantize_palette(source, palette, out_image)` and `pillow_c_image_quantize_palette_into` implement `Image.quantize(palette=...)`: RGB pixels map to the nearest palette RGB entry of a mode `P` palette image, `L` pixels are treated as direct indices, and the palette image's RGB/alpha palette metadata is copied to the result. The facade `Image.Quantize(colors, method, kmeans, palette, dither)` validates Pillow's method/mode boundaries in AHK, then routes to these exports; median-cut fallback for over-256-color sources currently exists only inside the GIF save quantizers, not behind these public exports.

`pillow_c_image_tobitmap(image, name, out, out_size, out_required)` implements the bounded `Image.tobitmap()` XBM-source export for mode `1` images using the two-call size protocol; the facade wraps it as `Image.ToBitmap(name := "image")`.

`pillow_c_image_convert_matrix` and `pillow_c_image_convert_matrix_into` implement Pillow `Image.convert(..., matrix=...)` for RGB input to `L` or `RGB`. `L` targets require four double values, `RGB` targets require twelve, values are converted to Pillow-style float math with `+0.5` before byte clipping, and unsupported source or target modes return `-3`. Matrix length mismatches return `-2`, and `_into` target shape or mode mismatches return `-5`.

`pillow_c_image_put_alpha_value` and `pillow_c_image_put_alpha_image` return `LA` for `L`/`LA` sources and `RGBA` for `RGB`/`RGBA` sources. The matching `_into` variants require the caller to provide that target mode and shape.

`pillow_c_image_histogram` refreshes an attached readonly `frombuffer` buffer view before reading source storage, then follows Pillow's 256-bin-per-visible-band layout. For byte-storage modes, callers provide `channels * 256` bins. For numeric storage modes `I` and `F`, callers provide exactly `256` bins: the export reads little-endian signed int32 or float32 samples, computes Pillow-style numeric extrema for the image, scales finite sample values into bins `0..255`, and returns an all-zero histogram for all-equal numeric images. `pillow_c_image_histogram_masked` refreshes attached readonly buffer views on both source and mask handles before masked counting. It accepts a same-size mode `1` or `L` mask for byte-storage modes; any nonzero native mask byte includes that pixel once, and zero excludes it. A null mask falls back to the unmasked histogram path. Masked mode `I` and `F` histograms return `PILLOW_C_INVALID_ARGUMENT`, matching Pillow's wrong-mode rejection for numeric masked histograms. For `LA`, Pillow 11.3.0 reports two bands where the second histogram repeats the luminance bins rather than the alpha bins; both native histogram exports mirror that behavior so `ImageStat.Stat` matches Pillow. `MODE-NUM-001M` adds a facade-only guard before this export for unmasked empty numeric `I` and `F` images, raising Pillow-compatible `min/max not given` instead of interpreting the empty numeric histogram as valid statistics. This behavior change added no export; source and Release x64 DLL export counts remain `378` / `378`.

`pillow_c_image_entropy` refreshes attached readonly source and optional mask
views before reading pixels. For byte-storage modes, entropy is computed from
the visible byte-band histogram, with optional mode `1` or `L` masks matching
the histogram mask semantics. For numeric modes `I` and `F`, unmasked entropy
uses the same one-band Pillow-compatible numeric histogram as
`pillow_c_image_histogram`, so four-byte storage is not counted as four byte
bands. Masked numeric entropy returns `PILLOW_C_INVALID_ARGUMENT`, matching
Pillow's public wrong-mode rejection. This behavior change added no export;
source and Release x64 DLL export counts remain `371` / `371`.

`pillow_c_image_getbbox` refreshes an attached readonly `frombuffer` buffer view before scanning source storage for the nonzero bounding box. The existing export shape is unchanged; the refresh happens inside the DLL so facade `Image.GetBbox()` does not need an AHK-side pre-refresh.

`pillow_c_image_getprojection` refreshes an attached readonly `frombuffer` buffer view before computing x/y nonzero axis projections. The existing export shape is unchanged; the refresh happens inside the DLL so facade `Image.GetProjection()` does not need an AHK-side pre-refresh.

`pillow_c_image_getcolors` refreshes an attached readonly `frombuffer` buffer view before counting unique colors for byte-storage modes. The existing export shape is unchanged; the refresh happens inside the DLL so facade `Image.GetColors()` does not need an AHK-side pre-refresh.

`pillow_c_image_getcolors_numeric(image, maxcolors, out_counts, out_values,
out_capacity, out_count, out_exceeded)` is the scalar numeric count/value route
for modes `I` and `F`. Passing null `out_counts` and null `out_values` with
capacity `0` performs a size probe. `out_counts` contains unsigned 64-bit
counts and `out_values` contains double materialized values; mode `I` values
are exact signed int32 samples widened to double, and mode `F` values are
float32 samples widened to double. The export counts distinct four-byte
numeric samples, returns `out_exceeded=1` for Pillow-style `None` when
`maxcolors` is too small or negative for a nonempty image, returns an empty
list for empty images with nonnegative `maxcolors`, and refreshes attached
readonly `frombuffer` source views before counting. It rejects nonnumeric modes
with `-3`. This ABI addition brings source and Release x64 DLL export counts
to `372` / `372`.

`pillow_c_image_get_extrema_numeric` accepts one double min buffer, one double
max buffer, one byte has-value buffer, and a Pillow-visible band count. It
exists to expose numeric single-band storage modes without treating four-byte
storage slots as four bands. For modes `I` and `F`, `out_count` must be `1`;
empty images clear the has-value byte. Mode `I` reads little-endian signed
int32 samples. Mode `F` reads little-endian 32-bit float samples, initializes
min and max from the first sample, uses ordinary float comparisons for later
samples, ignores later `NaN` values by comparison, preserves first-sample
`NaN` as both extrema, and allows `-Inf` / `Inf` to participate normally. Byte
modes are accepted with the same band count as `pillow_c_image_get_extrema`.

`pillow_c_image_rotate` and `pillow_c_image_rotate_into` accept angle, resample, expand, optional center, optional translate, and optional fill color arguments. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC` rotate, including CMYK fill-color packing, `LA`/`RGBA` premultiplied color sampling, and fill sampling; unsupported resamplers return `-3`.

`pillow_c_image_filter_kernel` and `pillow_c_image_filter_kernel_into` accept kernel width, kernel height, a pointer to double coefficients, coefficient count, scale, and offset. The current implementation supports Pillow's `(3, 3)` and `(5, 5)` kernel sizes for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`; it also supports bounded mode `I` signed-int32 samples for `(3, 3)` / `(5, 5)` kernels through the same ABI shape. Border pixels are copied unchanged, coefficients are applied with Pillow's vertical kernel flip, and byte-mode filtered values use half-up rounding and byte clipping. Mode `I` filtered values use one little-endian signed-int32 sample per pixel, half-up positive rounding, filtered-negative clipping to `0`, filtered-overflow clipping to `2147483647`, and Pillow-compatible `scale=0` output of `-2147483648`. Mode `F` returns `-3`, matching Pillow's public wrong-mode rejection at the facade. Unsupported kernel sizes return `-3`; coefficient count mismatches return `-2`. This behavior change added no export; source and Release x64 DLL export counts remain `371` / `371`.

`pillow_c_image_filter_rank` and `pillow_c_image_filter_rank_into` accept filter size and rank. The current implementation supports arbitrary positive odd sizes for `L`, `LA`, `RGB`, `RGBA`, `CMYK`, `I`, and `F`; rank must satisfy `0 <= rank < size * size`. Byte-storage modes rank each visible channel independently. Mode `I` ranks one little-endian signed-int32 sample per pixel, and mode `F` ranks one little-endian float32 sample per pixel. Edge pixels are computed with Pillow-style clamped coordinates, not copied unchanged. Invalid size or rank returns `-3`. The numeric rank-filter behavior change added no export; source and Release x64 DLL export counts remain `371` / `371`.

`pillow_c_image_filter_mode` and `pillow_c_image_filter_mode_into` accept filter size. The current implementation supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`, applying Pillow's single-band mode filter independently per channel. Size is converted to a radius with integer division by 2, so even sizes behave like the next odd window size. Only in-image coordinates are counted; outside pixels are ignored. A winning value replaces the original pixel only when it appears more than twice, and equal counts keep the smaller pixel value. Modes `I` and `F` return `-3`, matching Pillow's public `ImageFilter.ModeFilter` wrong-mode rejection instead of treating four-byte numeric storage as four independent byte channels. This behavior change added no export; source and Release x64 DLL export counts remain `371` / `371`.

`pillow_c_image_filter_box_blur` and `pillow_c_image_filter_box_blur_into` accept `xradius` and `yradius` doubles. The current implementation supports non-negative finite radii for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`, including fractional radii and single-axis blurs. It follows Pillow's separable fixed-point box blur with endpoint edge extension; radius `(0, 0)` returns a byte copy. Modes `I` and `F` return `-3`, matching Pillow's public wrong-mode rejection instead of treating four-byte numeric storage as byte channels. Invalid radius returns `-3`.

`pillow_c_image_reduce` and `pillow_c_image_reduce_into` accept integer `xscale`, `yscale`, and a source box as `left`, `top`, `right`, `bottom`. Scales must be greater than zero, boxes must be non-empty and inside the source image, and `_into` targets must match the ceiling-divided output size and source mode. `L`, `RGB`, and `CMYK` reduce by Pillow's fixed-point block average. `LA` and `RGBA` reduce color data in premultiplied-alpha space and convert back to the public mode.

`pillow_c_image_filter_gaussian_blur` and `pillow_c_image_filter_gaussian_blur_into` accept `xradius` and `yradius` doubles. The current implementation supports finite radii for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`, including fractional radii and single-axis blurs. It mirrors Pillow's default three-pass Gaussian approximation by transforming each requested radius into a BoxBlur radius, running all horizontal passes before vertical passes, and returning a byte copy when both effective radii are zero. Modes `I` and `F` return `-3`, matching Pillow's public wrong-mode rejection. Non-finite or out-of-range radii return `-3`.

`pillow_c_image_filter_unsharp_mask` and `pillow_c_image_filter_unsharp_mask_into` accept a scalar `radius` double plus integer `percent` and `threshold`. The current implementation supports finite radii for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. It reuses native GaussianBlur, then applies Pillow's per-channel `abs(original - blurred) > threshold` condition and `original + (original - blurred) * percent / 100` sharpening with byte clipping. Modes `I` and `F` return `-3`, matching Pillow's public wrong-mode rejection. Non-finite or out-of-range radii return `-3`. The numeric blur/unsharp wrong-mode behavior change added no export; source and Release x64 DLL export counts remain `371` / `371`.

`pillow_c_image_filter_color_3d_lut` and `pillow_c_image_filter_color_3d_lut_into` accept a target mode id, table channel count, three LUT dimensions, a pointer to double table values, and a table value count. The table uses Pillow's flattened order: channels change first, then the first, second, and third dimensions. The native path prepares table values into Pillow-compatible signed 16-bit fixed-point values and applies trilinear interpolation over the source image's first three bands. Source images must have at least three bands; table channels must be 3 or 4; target modes must have at least that many bands; and a 3-channel table preserves the source fourth band for 4-band targets such as `RGBA` and `CMYK`. Invalid mode or size arguments return `-3`, table length mismatches return `-2`, and `_into` target shape or mode mismatches return `-5`.

The native ImageFilter exports refresh an attached readonly `frombuffer` source view before reading source pixels. This applies to `pillow_c_image_filter_kernel`, `pillow_c_image_filter_rank`, `pillow_c_image_filter_mode`, `pillow_c_image_filter_box_blur`, `pillow_c_image_filter_gaussian_blur`, `pillow_c_image_filter_unsharp_mask`, `pillow_c_image_filter_color_3d_lut`, and their `_into` variants. The existing export shapes are unchanged; facade `Image.Filter(...)` relies on this native refresh and does not perform wrapper-level pre-refresh. Filter results are owned images and `_into` writes into caller-provided owned target storage while leaving the source readonly view attached.

`pillow_c_image_transform_affine` and `pillow_c_image_transform_affine_into` accept output width, output height, a pointer to six doubles `(a, b, c, d, e, f)`, resample, and optional fill color arguments. The matrix follows Pillow `Image.transform(..., Transform.AFFINE, matrix, ...)` destination-to-source coordinates. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC`, with CMYK covered by the same channel-generic path and `LA`/`RGBA` filtered transforms using premultiplied color sampling; transform-only unsupported resamplers return `-3`.

`pillow_c_image_transform_perspective` and `pillow_c_image_transform_perspective_into` accept output width, output height, a pointer to eight doubles `(a, b, c, d, e, f, g, h)`, resample, and optional fill color arguments. The coefficients follow Pillow `Image.transform(..., Transform.PERSPECTIVE, coefficients, ...)` destination-to-source coordinates. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC`; transform-only unsupported resamplers return `-3`.

`pillow_c_image_transform_quad` and `pillow_c_image_transform_quad_into` accept output width, output height, a pointer to eight doubles `(nw_x, nw_y, sw_x, sw_y, se_x, se_y, ne_x, ne_y)`, resample, and optional fill color arguments. The corners follow Pillow `Image.transform(..., Transform.QUAD, corners, ...)` order. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC`; transform-only unsupported resamplers return `-3`.

`pillow_c_image_transform_mesh` and `pillow_c_image_transform_mesh_into` accept output width, output height, a pointer to `mesh_count * 4` integer boxes, a pointer to `mesh_count * 8` double QUAD corner values, `mesh_count`, resample, and optional fill color arguments. MESH patches are applied in input order and later patches overwrite earlier overlap, matching Pillow `Image.transform(..., Transform.MESH, mesh, ...)`. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC`; transform-only unsupported resamplers return `-3`.

`pillow_c_image_transpose` and `pillow_c_image_transpose_into` refresh an attached readonly `frombuffer` source view before copying flipped or rotated pixels, matching Pillow 11.3.0's `Image.transpose(...)` source-read behavior while leaving the input readonly and returning or reusing owned target storage. Allocating transpose delegates through the same helper as `_into`, so the export shapes are unchanged; no ABI symbol was added and the Release x64 DLL still exposes `341` `pillow_c_*` exports.

## TIFF BigTIFF ABI Behavior

`pillow_c_image_open_tiff`, `pillow_c_image_open_tiff_frame`, and
`pillow_c_image_frame_count_tiff` keep the existing signatures and status-code
contract for the bounded BigTIFF route. A little-endian BigTIFF with magic
`43`, offset size `8`, chained 20-byte-entry IFDs, compressed chunky 8-bit
`L`, `RGB`, `RGBA`, or `LA` tiles, and LONG8 tile offset/count arrays is
reconstructed into a DLL-owned image. The parser accepts scalar SHORT, LONG,
and LONG8 fields, validates every 64-bit offset/count against the input byte
span, clips partial right/bottom tiles, selects nonzero frames through the
existing frame-open export, and counts the validated next-IFD chain while
rejecting repeated offsets. Unsupported shapes, malformed IFD/array ranges,
and BigTIFF Orientation combinations return `PILLOW_C_INVALID_ARGUMENT` in
the native path; the facade exposes the same failure through its existing
error translation. No export, signature, handle ownership, pointer lifetime,
or buffer-retention rule changed; source and Release x64 DLL exports are
`453` / `453`.

## Resize Resampling IDs

Current resize support:

```text
0 NEAREST
1 LANCZOS
2 BILINEAR
3 BICUBIC
4 BOX
5 HAMMING
```

## Transpose Method IDs

These match Pillow 11.3.0 `Image.Transpose` values:

```text
0 FLIP_LEFT_RIGHT
1 FLIP_TOP_BOTTOM
2 ROTATE_90
3 ROTATE_180
4 ROTATE_270
5 TRANSPOSE
6 TRANSVERSE
```

## Pointer Lifetime

`pillow_c_image_data` returns a pointer into handle-owned storage. The pointer is valid only while the image handle remains alive and the underlying storage is not reallocated.

The AHK facade pins the configured `pillow_c.dll` with `LoadLibraryW` before
the first exported call and keeps that module handle for process lifetime. This
avoids path-based `DllCall` repeatedly loading and unloading the DLL around
facade calls. Once loaded, `Pillow.Configure({DllPath: ...})` rejects attempts
to switch to a different DLL path in the same process; use a separate AHK
process for testing another DLL build. This is a facade lifetime rule only and
does not add, remove, or change any exported ABI symbol.
