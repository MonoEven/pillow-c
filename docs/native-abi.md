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
```

`pillow_c_mode_from_string`, `pillow_c_mode_name`, `pillow_c_image_create_mode`, and `pillow_c_image_mode` keep handles mode-aware. Channel count is storage layout; mode is wrapper-visible Pillow semantics. `pillow_c_image_create_mode` accepts Pillow-style empty mode-aware image shapes where width or height is zero. Shared mode-aware target shape checks use the same allow-empty size validation, so `_into` exports can validate empty matching targets for public paths that otherwise support empty output.

The legacy `pillow_c_image_create(width, height, channels, ...)` maps channel count `1`, `2`, `3`, and `4` to `L`, `LA`, `RGB`, and `RGBA`, and remains a non-empty channel-count creation API.

Mode `1` uses one unpacked byte per pixel internally for native operations and data-pointer sharing. `pillow_c_image_set_raw_bytes` and `pillow_c_image_get_raw_bytes` expose Pillow's external bit-packed row format for raw mode `1`.

Mode `P` uses one palette index byte per pixel internally. RGB palette metadata lives on the image handle and is exposed through `pillow_c_image_put_palette_rgb` and `pillow_c_image_get_palette_rgb`; optional palette alpha metadata is exposed through `pillow_c_image_put_palette_rgba`, `pillow_c_image_get_palette_rgba`, and `pillow_c_image_palette_alpha_mode`. `pillow_c_image_put_palette_rgb` and `pillow_c_image_put_palette_rgba` also mirror Pillow's `L.putpalette(...)` behavior by converting an `L` handle to mode `P` while keeping its one-byte pixel indexes. Same-mode pixel-copy, point/LUT, reorder, expand, offset, resize, transform, and rotate paths preserve that palette so later conversion still resolves indexes like Pillow.

Mode `CMYK` uses four direct channel bytes per pixel. The current verified CMYK foundation covers mode mapping, raw byte import/export, getdata/putdata facade packing, getpixel/putpixel, copy, `ImageChops.invert`, and non-logical `ImageChops` binary operations.

Mode `I` uses four bytes per pixel as a little-endian signed 32-bit Pillow integer storage slot. The current verified `I` surface is intentionally narrow: mode mapping, byte-size/data export, raw `I` byte import/export, Pillow-compatible unsigned 16-bit raw decode aliases, `I;16B` raw encode, high-bit-depth Netpbm grayscale open/save, bounded TIFF save/open for frame 0 including uncompressed, PackBits, TIFF LZW, and Adobe Deflate, facade scalar `getpixel`/`putpixel`/`getdata`/`putdata` semantics, facade `GetBands()` / `getbands()` band-name materialization, numeric `getextrema()` through `pillow_c_image_get_extrema_numeric`, one-band numeric `histogram()` and entropy through `pillow_c_image_histogram` / `pillow_c_image_entropy`, scalar numeric `getcolors()` through `pillow_c_image_getcolors_numeric`, `convert("L")` through `pillow_c_image_convert_mode`, bounded `ImageFilter.Kernel` through `pillow_c_image_filter_kernel`, bounded rank filters through `pillow_c_image_filter_rank`, bounded `ImageFilter.ModeFilter` wrong-mode rejection through `pillow_c_image_filter_mode`, and bounded BoxBlur/GaussianBlur/UnsharpMask wrong-mode rejection through the native blur/unsharp filter exports. General `I` arithmetic, conversion beyond the covered `L` target, unsupported numeric filter algorithms, public `I;16*` behavior beyond the first-class little-endian `I;16` mode, and broader file-format participation are future ABI surfaces.

Mode `F` uses four bytes per pixel as a little-endian 32-bit float storage slot. The current verified `F` surface is intentionally narrow: mode mapping, byte-size/data export, raw `F`/`F;32F` byte import/export, bounded TIFF save/open for frame 0 including uncompressed, PackBits, TIFF LZW, and Adobe Deflate, facade scalar `getpixel`/`putpixel`/`getdata`/`putdata` semantics, facade `GetBands()` / `getbands()` band-name materialization, numeric `getextrema()` through `pillow_c_image_get_extrema_numeric`, one-band numeric `histogram()` and entropy through `pillow_c_image_histogram` / `pillow_c_image_entropy`, scalar numeric `getcolors()` through `pillow_c_image_getcolors_numeric`, `convert("L")` through `pillow_c_image_convert_mode`, bounded `ImageFilter.Kernel` wrong-mode rejection through `pillow_c_image_filter_kernel`, bounded rank filters through `pillow_c_image_filter_rank`, bounded `ImageFilter.ModeFilter` wrong-mode rejection through `pillow_c_image_filter_mode`, and bounded BoxBlur/GaussianBlur/UnsharpMask wrong-mode rejection through the native blur/unsharp filter exports. General `F` arithmetic, conversion beyond the covered `L` target, unsupported numeric filter algorithms, broader NaN/Inf behavior outside the covered extrema/histogram/convert/entropy/getcolors fixtures, and broader file-format participation are future ABI surfaces.

Mode `I;16` uses two bytes per pixel as little-endian unsigned 16-bit grayscale storage. The current verified public surface is bounded to `pillow_c_mode_from_string("I;16")`, `pillow_c_mode_name(11)`, `pillow_c_image_create_mode`, direct byte import/export, `Image.FromBytes("I;16", ...)`, `Image.ToBytes()`, and little-endian TIFF save/open for frame `0` with uncompressed, PackBits, TIFF LZW, and Adobe Deflate strip handling. It does not claim `I;16N`, arithmetic, filters, broad conversion, predictor variants, or non-frame-0 special parsing.

Mode `I;16B` uses two bytes per pixel as big-endian unsigned 16-bit grayscale storage. The current verified public surface is bounded to `pillow_c_mode_from_string("I;16B")`, `pillow_c_mode_name(12)`, `pillow_c_image_create_mode`, direct byte import/export, `Image.FromBytes("I;16B", ...)`, `Image.ToBytes()`, frame-0 uncompressed big-endian TIFF save/open, and compressed TIFF save via PackBits, TIFF LZW, Adobe Deflate, or `tiff_deflate`. The uncompressed TIFF writer emits the bounded Pillow-compatible single-strip `MM` IFD shape with BitsPerSample `16`, Compression `1`, PhotometricInterpretation `1`, RowsPerStrip equal to image height, PlanarConfiguration `1`, no SamplesPerPixel tag, and no SampleFormat tag. Compressed public `I;16B` saves match Pillow 11.3.0 by byte-swapping into a temporary little-endian `I;16` TIFF, writing Compression `32773`, `5`, or `8`, and reopening as mode `I;16` with preserved numeric samples. `I;16B` DPI, multipage save, `I;16N`, arithmetic, filters, broad conversion, predictor variants, and non-frame-0 special parsing remain future ABI surfaces.

Mode `RGBX` uses four direct channel bytes per pixel with Pillow-visible band names `R`, `G`, `B`, and `X`. The current verified public `RGBX` surface is intentionally bounded to raw byte import/export and `Image.frombuffer(..., "raw", "RGBX", stride, orientation)` alias/detach behavior. General operations, format save/open paths, conversion matrices, and draw/filter coverage for public `RGBX` remain future mode-expansion work.

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
- `pillow_c_image_save_jpeg`
- `pillow_c_image_save_jpeg_quality`
- `pillow_c_image_save_jpeg_options`
- `pillow_c_image_save_jpeg_subsampling_options`
- `pillow_c_image_save_jpeg_encode_options`
- `pillow_c_image_save_jpeg_encode_keep_rgb_options`
- `pillow_c_image_save_jpeg_qtables_encode_options`
- `pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options`
- `pillow_c_image_save_jpeg_metadata_options`
- `pillow_c_image_save_jpeg_metadata_subsampling_options`
- `pillow_c_image_save_jpeg_metadata_encode_options`
- `pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options`
- `pillow_c_image_save_jpeg_metadata_keep_rgb_xmp_encode_options`
- `pillow_c_image_save_jpeg_qtables_metadata_encode_options`
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
- `pillow_c_image_save_gif`
- `pillow_c_image_save_gif_options`
- `pillow_c_image_save_gif_comment`
- `pillow_c_image_save_gif_comment_options`
- `pillow_c_image_save_gif_animation`
- `pillow_c_image_save_gif_animation_options`
- `pillow_c_image_save_gif_animation_metadata_options`
- `pillow_c_image_save_gif_animation_comment`
- `pillow_c_image_save_gif_animation_comment_metadata_options`
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
- `pillow_c_image_convert_matrix`
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
- `pillow_c_image_convert_matrix_into`
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

`pillow_c_image_draw_floodfill` mutates one image handle in place for Pillow `ImageDraw.floodfill` calls. It accepts a seed coordinate, caller-packed value color, optional caller-packed border color, and a threshold. The implementation follows Pillow 11.3.0's Python flood-fill semantics while moving the queue walk into C++: seed pixels use Pillow coordinate normalization, out-of-range seeds are no-ops, no-border mode fills pixels whose 1-norm color difference from the seed background is within `thresh`, and border mode fills pixels that are neither the fill value nor the border value.

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
header. The ABI follows the same size-probe pattern as binary PNG/JPEG
metadata exports: missing metadata returns `out_has_xmp == 0`,
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
open parser populates this blob with common ASCII IFD0 tags `270`, `271`,
`272`, `305`, and `306`; it includes Orientation `274` only when the stored
value is identity `1`. TIFF Orientation values `2..8` continue to be represented as
open-side pixel transforms and hidden metadata, matching the bounded Pillow
11.3.0 oracle.
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
The export sets the required size before returning `-1` for a null output
buffer, returns `-2` when the supplied buffer is too small, and returns `-3`
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

`pillow_c_image_open_jpeg`, `pillow_c_image_save_jpeg`,
`pillow_c_image_save_jpeg_quality`, `pillow_c_image_save_jpeg_options`,
`pillow_c_image_save_jpeg_subsampling_options`,
`pillow_c_image_save_jpeg_encode_options`,
`pillow_c_image_save_jpeg_encode_keep_rgb_options`,
`pillow_c_image_save_jpeg_qtables_encode_options`,
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
`ahk\fixtures`; that earlier open-only slice added coverage only and no new
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
bytes so the JPEG segment length stays within `65535`. The JPEG metadata scan
reassembles complete same-count APP2 ICC sequences and exposes the joined bytes
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
shared `Info["xmp"]` route for JPEG and PNG images. The facade's bounded
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
`pillow_c_image_metadata_jpeg_subsampling` exposes parsed RGB JPEG SOF
sampling metadata as Pillow's integer subsampling option. It writes `0` for
4:4:4, `1` for 4:2:2, `2` for 4:2:0, and `-1` when the opened JPEG has no
recognized RGB subsampling shape. The facade uses this with the qtable
metadata exports for bounded opened RGB JPEG `quality="keep"` /
`qtables="keep"` saves, and to resolve public `subsampling="keep"` before
entering those same native qtables or metadata subsampling save routes. The
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
`-3` for negative or out-of-range frames. After successful frame-0 decode,
native open reads the original file bytes and parses the TIFF header/IFD0;
failure to read those bytes returns `-3` rather than silently dropping
metadata.

For frame-0 mode `P`, native open also requires and parses ColorMap tag `320`
as `768` SHORT values in red/green/blue planes, converting each value with
`value >> 8` so Pillow-style `byte * 256` ColorMap entries round-trip exactly
instead of using WIC's quantized palette. For frame-0 mode `LA`, native open
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
`I` or mode `F` handles with preserved four-byte sample storage. Native TIFF
frame-0 open also recognizes the bounded little-endian public `I;16` IFD shape
with BitsPerSample `16`, Compression `1`, `32773`, `5`, or `8`,
PhotometricInterpretation `1`, StripOffsets/StripByteCounts for one strip,
RowsPerStrip equal to image height for compressed strips, PlanarConfiguration
`1`, no SamplesPerPixel, and no SampleFormat. It decodes uncompressed,
PackBits, TIFF LZW, or zlib Deflate strip bytes and returns mode `I;16` with
preserved two-byte sample storage. Broader TIFF
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
codes, and compression `8` writes tag `259` as Adobe Deflate and stores the
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

`pillow_c_image_convert_mode` and `pillow_c_image_convert_mode_into` cover the verified `1`/`L`/`LA`/`RGB`/`RGBA`/`P`/`CMYK` conversion paths used by the facade, plus the bounded numeric `I -> L` and `F -> L` paths. The convert-mode exports, including the dither variants, refresh an attached readonly `frombuffer` source view before reading source storage, so facade `Image.Convert()` samples current caller bytes without an AHK-side pre-refresh while the source view remains readonly. Numeric conversion truncates toward zero, clamps to `0..255`, maps `NaN` and `-Inf` to `0`, maps `+Inf` to `255`, preserves empty `(0, n)` or `(n, 0)` output shapes, and lets `_into` reuse a matching caller-provided mode `L` target. Quantizing targets such as `CMYK -> P` remain unsupported. `LA` preserves alpha as the second band; alpha is ignored for `RGBA`/`LA -> CMYK`, matching Pillow. `CMYK -> RGB` uses Pillow's black-channel-scaled RGB conversion before luma or alpha insertion, and `P -> CMYK` converts through the handle palette.

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
