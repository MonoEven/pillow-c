# Pillow Gap Checkpoint

This is the short resume layer for Pillow work in this repository. Read this
file first, then open `docs/pillow-gap-analysis.md` only for the selected gap
card or broader evidence.

Last updated: 2026-07-03

## Required Resume Order

1. Read `AGENTS.md`.
2. Read this file.
3. Read `docs/pillow-gap-analysis.md`.
4. Read `docs/native-abi.md` before changing exported DLL behavior.
5. Read `docs/testing.md` before running AHK tests.
6. Check `git status --short` before editing.

Do not restart with a full broad audit. Update this checkpoint and the detailed
ledger together whenever coverage meaningfully changes.

## Current Snapshot

```text
Estimate: AHK-first high-performance Pillow-compatible runtime about 52-55%; full Pillow replacement 26-31%.
    Latest covered gap: META-001M bounded TIFF ImageDescription tag 270 getexif readback.
`META-001M` extends reopened TIFF IFD0 common ASCII EXIF readback to tag `270`
(`ImageDescription`). The local Pillow 11.3.0 oracle for
`tiffinfo={270:"desc",271:"ACME",272:"MODEL1",305:"pillow-c",306:"2026:07:03 12:34:56",274:1}`
reopens with `getexif()[270] == "desc"`, `tag_v2.get(270) == "desc"`, all
previous common ASCII tags plus identity Orientation `274=1`, and no
`Info["exif"]`. Native TIFF open now admits tag `270` in the same DLL-owned
`pillow_c_image_metadata_tiff_exif` blob, and the facade enumerates it through
`Image.GetExif()` / `Image.getexif()` without AHK per-pixel loops. No native
ABI symbol was added; Release x64 was rebuilt after native code changed, and
source/Release x64 DLL export counts remain `379` / `379`.
`FMT-PNG-001AD` extends the bytes-valued `PngInfo.add_text(...)` route to
embedded NUL values. The local Pillow 11.3.0 oracle writes
`b"a\0b"` as exact value bytes in both `tEXt` and compressed `zTXt` chunks and
reopens the value as the three-character string `a`, `Chr(0)`, `b`. Native PNG
save now exposes
`pillow_c_image_save_png_text_entries_value_sizes_options`, a bounded
text-entry ABI that accepts explicit value byte sizes for `tEXt`/`zTXt` values
instead of using `strlen`, while existing NUL-terminated text-entry exports
remain unchanged. The facade copies Buffer values for lifetime safety, routes
only simple embedded-NUL `tEXt`/`zTXt` entries through the new DLL export, and
decodes reopened PNG text metadata from explicit UTF-8 byte counts so
`Info`/`Text` can contain `Chr(0)` without truncation. Release x64 was rebuilt
after native code changed; source and Release x64 DLL export counts are now
`379` / `379`.
`FMT-PNG-001AC` adds bounded `PngInfo.add_text(..., bytes)` parity for
NUL-free byte values. The local Pillow 11.3.0 oracle accepts `b"caf\xe9"` and
writes raw Latin-1 bytes in `tEXt` when `zip=False`, or a compressed `zTXt`
payload when `zip=True`, reopening both as Unicode text `caf` plus
`Chr(0xE9)`. The facade now accepts `Buffer` values for `add_text`, copies
them for lifetime safety, passes NUL-terminated raw byte values to the DLL, and
explicitly rejects embedded NUL for this bounded route instead of silently
truncating through the current NUL-terminated ABI. Native PNG save now permits
Latin-1 high bytes in `tEXt`/`zTXt` values, and native PNG open converts
`tEXt`/`zTXt` Latin-1 metadata to UTF-8 before exposing it through
`pillow_c_image_metadata_png_text`. No native ABI symbol was added; source and
Release x64 DLL export counts remain `378` / `378`, and Release x64 was
rebuilt after native code changed.
`FMT-JPEG-002B2R` aligns the public facade route for opened RGB JPEG
`Image.Save(..., "JPEG", {qtables:"keep", keep_rgb:true})`. The bounded local
Pillow 11.3.0 oracle accepts an opened RGB JPEG originally saved with
`subsampling=2` and `comment=...`, then saves `qtables="keep", keep_rgb=True`
with APP14 Adobe transform `0`, no JFIF marker, SOF0/SOS component IDs
`R/G/B` with `1x1` sampling, preserved opened DQT tables, and preserved opened
COM/comment metadata. The existing native qtables keep-rgb export already
matched that shape when called with default subsampling (`-1`); the facade now
normalizes this exact `qtables="keep"` plus `keep_rgb=True` case to the default
RGB keep-rgb sampling instead of copying the opened JPEG's sampled YCbCr
subsampling. This keeps `quality="keep" + keep_rgb=True` sampled-source
rejection unchanged, adds no native ABI symbol, leaves source and Release x64
DLL export counts at `378` / `378`, and required no native rebuild.
`MODE-NUM-001O` aligns the public `Pillow.ImageOps.Equalize` and
`Pillow.ImageOps.Autocontrast` facade boundary for numeric storage modes `I`
and `F`. The bounded local Pillow 11.3.0 oracle raises
`OSError: not supported for mode I` / `F` for unmasked equalize/autocontrast
and masked `autocontrast(..., preserve_tone=True)`, while masked
`equalize(...)` and masked `autocontrast(..., preserve_tone=False)` raise
`ValueError: image has wrong mode`. The native DLL already rejects those
numeric histogram-transform routes with `PILLOW_C_INVALID_ARGUMENT` before
treating four-byte numeric storage as byte channels, and raw tests now pin the
allocating and `_into` rejections for equalize/autocontrast, masked and
unmasked. The facade maps that native invalid-argument status to Pillow's
public `not supported for mode ...` or `image has wrong mode` message
according to the proven Pillow boundary. No native ABI symbol was added,
source and Release x64 DLL export counts remain `378` / `378`, and no native
rebuild was required.
`MODE-NUM-001N` aligns the public `Pillow.ImageOps.Invert`,
`Pillow.ImageOps.Posterize`, and `Pillow.ImageOps.Solarize` facade boundary
for numeric storage modes `I` and `F`. The bounded local Pillow 11.3.0 oracle
raises `OSError: not supported for mode I` / `F` for those LUT-style
operations. The native DLL already rejects modes `I` and `F` through the
shared ImageOps LUT support gate instead of treating four-byte numeric storage
as byte channels, and raw tests now pin both allocating and `_into` rejections.
The facade maps that native invalid-argument status to Pillow's public
`not supported for mode ...` message while preserving native mode `1` invert
and `L`/`RGB` LUT paths. No native ABI symbol was added, source and Release
x64 DLL export counts remain `378` / `378`, and no native rebuild was
required.
`MODE-NUM-001M` aligns the public `Pillow.ImageStat.Stat(...)` facade for
empty numeric storage modes `I` and `F`. The bounded local Pillow 11.3.0
oracle raises `ValueError: min/max not given` for unmasked empty `I` and `F`
images, while masked numeric statistics continue to reject through the
existing wrong-mode histogram path. The facade now detects unmasked empty
numeric images before asking the native histogram route for an all-zero
numeric histogram, so public `ImageStat.Stat` matches Pillow without any AHK
per-pixel loop. No native ABI symbol was added, source and Release x64 DLL
export counts remain `378` / `378`, and no native rebuild was required.
`META-001L` adds bounded TIFF IFD0 common ASCII EXIF tag readback for reopened
TIFF handles. The local Pillow 11.3.0 oracle for
`tiffinfo={271:"ACME",272:"MODEL1",305:"pillow-c",306:"2026:07:03 12:34:56",274:1}`
reopens with `getexif()` containing all five tags while `info` has no
`"exif"` key. For TIFF Orientation values `2..8`, Pillow applies the
open-side orientation transform and hides tag `274`; the bounded route keeps
that existing behavior while preserving the other common ASCII tags in
`getexif()`. Native TIFF open now builds DLL-owned Pillow-style `Exif\0\0`
bytes for tags `271`, `272`, `305`, and `306`, plus tag `274` only when the
stored orientation is `1`, and exposes them through
`pillow_c_image_metadata_tiff_exif`. The facade routes public
`Image.GetExif()` / `Image.getexif()` through that native metadata blob for
TIFF images without adding `Info["exif"]` and without AHK per-pixel loops.
Source and Release x64 DLL export counts are now `378` / `378`, and Release
x64 was rebuilt after the native implementation changed.
`FMT-JPEG-002B2Q` extends the existing RGB JPEG `keep_rgb=True` route to
explicit `dpi=(300,150)`. The local Pillow 11.3.0 oracle accepts
`Image.save(..., "JPEG", keep_rgb=True, dpi=(300,150))`, writes APP0/JFIF
before APP14 Adobe transform `0`, keeps SOF0/SOS component IDs `R/G/B` with
`1x1` sampling, and reopens as `RGB` with Pillow-style DPI/JFIF metadata.
Native `pillow_c_image_save_jpeg_encode_keep_rgb_options` now writes the JFIF
APP0 segment before APP14 for bounded RGB keep-rgb saves with `has_dpi=1`,
while keeping the existing `subsampling=1/2` rejection. The facade routes
public `Image.Save(..., "JPEG", {keep_rgb:true, dpi:[300,150]})` through the
DLL without an AHK per-pixel loop. No native ABI symbol was added, source and
Release x64 DLL export counts remain `377` / `377`, and Release x64 was
rebuilt after the native implementation changed.
`FMT-JPEG-002B2P` extends the existing RGB JPEG qtables plus `keep_rgb=True`
route to explicit `comment`, `icc_profile`, and `exif` metadata. The local
Pillow 11.3.0 oracle accepts `Image.save(..., "JPEG", qtables=...,
keep_rgb=True, comment=..., icc_profile=..., exif=...)`, writes APP14 Adobe
transform `0`, APP1 EXIF, APP2 ICC, COM, custom DQT tables, SOF0 RGB
component IDs `R/G/B` with qtable selectors `[0,1,1]`, and reopens as `RGB`
with comment, ICC, EXIF bytes, and EXIF orientation `6`. Native code now
composes this through the existing
`pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options` export by
writing the DLL-owned RGB-component qtables JPEG first and patching metadata
segments before DQT; the facade removes the stale rejection and routes the
public `Image.Save(..., "JPEG", {qtables: ..., keep_rgb: true, comment: ...,
icc_profile: ..., exif: ...})` shape without AHK per-pixel loops. No native
ABI symbol was added, source and Release x64 DLL export counts remain `377` /
`377`, and Release x64 was rebuilt after the native implementation changed.
`META-002F` extends the public `Image.GetXmp()` / `Image.getxmp()` facade
parser to match Pillow 11.3.0 for repeated RDF container children in bounded
PNG XMP packets. The local oracle for a PNG `iTXt` key
`XML:com.adobe.xmp` containing `dc:creator/rdf:Seq/rdf:li` values `Ada` and
`Grace` returns `creator.Seq.li == ["Ada", "Grace"]`, while the existing raw
native XMP metadata route already preserves the exact packet bytes through
`pillow_c_image_metadata_xmp`. The facade red test proved the previous parser
overwrote repeated `li` nodes instead of materializing a list; the fix keeps
text-only leaf elements as scalars, repeated sibling names as arrays, and
attribute-bearing leaves as maps. No native ABI symbol was added, source and
Release x64 DLL export counts remain `377` / `377`, and no native rebuild was
required.
`FMT-PNG-004AT` removes the next stale facade optimize guard and routes the
no-custom PNG advanced text kinds (`zTXt`, plain `iTXt`, compressed `iTXt`,
and language-keyed `iTXt`) together with explicit `icc_profile`, explicit
`exif`, RGB `transparency=(10,20,30)`, and `optimize=True` through the
existing native `pillow_c_image_save_png_metadata_options` route. The local
Pillow 11.3.0 oracle writes chunk order `IHDR`, `iCCP`, selected text chunk,
`tRNS`, `eXIf`, `IDAT`, `IEND`, with optimized `IDAT` header `[0x78,0xDA]`;
reopening preserves RGB bytes, text metadata, ICC bytes, Pillow-style EXIF
orientation `6`, RGB transparency metadata, and native `RGB -> RGBA`
transparent-pixel conversion. Raw DLL coverage proved the generalized ABI
already handled the shape; the facade red test failed only on the
`Pillow.Image.Save optimize with compressed or iTXt pnginfo is not supported`
guard, and the fix narrows that over-broad rejection without AHK per-pixel
loops. No native ABI symbol was added, source and Release x64 DLL export
counts remain `377` / `377`, and no native rebuild was required.
`FMT-PNG-004AS` removes the next stale facade guard and routes
language-keyed `PngInfo.add_itxt("Comment", "Bonjour UTF8", "fr",
"Commentaire")` together with explicit `icc_profile`, explicit `exif`, and
RGB `transparency=(10,20,30)` through the existing native
`pillow_c_image_save_png_metadata_options` route. The local Pillow 11.3.0
oracle writes chunk order `IHDR`, `iCCP`, `iTXt`, `tRNS`, `eXIf`, `IDAT`,
`IEND`; reopening preserves RGB bytes, text metadata, ICC bytes,
Pillow-style EXIF orientation `6`, RGB transparency metadata, and native
`RGB -> RGBA` transparent-pixel conversion. Raw DLL coverage proved the
generalized ABI already handled the shape; the facade red test failed only on
the `Pillow.Image.Save pnginfo with icc_profile and transparency is not supported`
guard, and the fix narrows that over-broad rejection without AHK per-pixel
loops. No native ABI symbol was added, source and Release x64 DLL export
counts remain `377` / `377`, and no native rebuild was required.
`FMT-PNG-004AR` removes a stale facade guard and routes language-keyed
`PngInfo.add_itxt("Comment", "Bonjour UTF8", "fr", "Commentaire")` together
with explicit `icc_profile` and explicit `exif` through the existing native
`pillow_c_image_save_png_metadata_options` route. The local Pillow 11.3.0
oracle writes chunk order `IHDR`, `iCCP`, `iTXt`, `eXIf`, `IDAT`, `IEND`;
reopening preserves RGB bytes, text metadata, ICC bytes, and Pillow-style EXIF
orientation `6`. Raw DLL coverage proved the generalized ABI already handled
the shape; the facade red test failed only on the
`Pillow.Image.Save pnginfo language and translated keyword with metadata is not supported`
error, and the fix deletes that over-narrow guard without AHK
per-pixel loops. No native ABI symbol was added, source and Release x64 DLL
export counts remain `377` / `377`, and no native rebuild was required.
`META-002E` extends the JPEG XMP write surface to the combined custom
quantization-table plus `keep_rgb=True` route. The local Pillow 11.3.0 oracle
accepts `Image.save(..., "JPEG", qtables=..., keep_rgb=True, xmp=...)`,
writes APP14 Adobe transform `0`, writes APP1 XMP before DQT payloads, uses
RGB component markers with caller qtables, and reopens with `Info["xmp"]` /
`Image.getxmp()`. Native JPEG metadata segment construction now composes
explicit XMP with caller qtables and `keep_rgb` through
`pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_xmp_encode_options`; the
facade routes explicit
`Image.Save(..., "JPEG", {qtables: ..., keep_rgb: true, xmp: Buffer})`
through that DLL path without AHK per-pixel loops. Broader XMP schemas,
implicit XMP preservation, PNG `PngInfo` XMP write convenience, IPTC, full ICC
lifecycle, and ImageCms remain future `META-002` children. Source and Release
x64 DLL export counts are now `377` / `377`, and Release x64 was rebuilt
after the ABI changed.
`META-002D` extends the JPEG XMP write surface to no-qtables
`keep_rgb=True` saves. The local Pillow 11.3.0 oracle accepts
`Image.save(..., "JPEG", keep_rgb=True, xmp=...)`, writes an APP14 Adobe
marker with transform `0`, writes APP1 XMP before DQT payloads, and reopens
with `Info["xmp"]` / `Image.getxmp()` while preserving RGB component markers.
Native JPEG metadata segment construction now composes explicit XMP with
`keep_rgb` through
`pillow_c_image_save_jpeg_metadata_keep_rgb_xmp_encode_options`; the facade
routes explicit `Image.Save(..., "JPEG", {keep_rgb: true, xmp: Buffer})`
through that DLL path without AHK per-pixel loops. Broader XMP schemas,
implicit XMP preservation, JPEG `qtables + keep_rgb + xmp`, PNG `PngInfo` XMP
write convenience, IPTC, and ImageCms remain future `META-002` children.
Source and Release x64 DLL export counts are now `376` / `376`, and Release
x64 was rebuilt after the ABI changed.
`META-002C` extends the JPEG XMP write surface to custom quantization-table
saves. The local Pillow 11.3.0 oracle accepts `Image.save(..., "JPEG",
qtables=..., xmp=...)`, writes APP1 XMP metadata before DQT payloads, does not
implicitly preserve opened XMP even for `quality="keep"`, and keeps
`keep_rgb + xmp` outside this bounded slice. Native JPEG metadata segment
construction now composes explicit XMP with caller qtables through
`pillow_c_image_save_jpeg_qtables_metadata_xmp_encode_options`; the facade
routes explicit `Image.Save(..., "JPEG", {qtables: ..., xmp: Buffer})`
through that DLL path without AHK per-pixel loops, and reopen exposes
`Info["xmp"]` / `Image.getxmp()`. Broader XMP schemas, implicit XMP
preservation, JPEG `qtables + keep_rgb + xmp`, PNG `PngInfo` XMP write
convenience, IPTC, and ImageCms remain future `META-002` children. Source and
Release x64 DLL export counts are now `375` / `375`, and Release x64 was
rebuilt after the ABI changed.
`META-002B` adds bounded write-side XMP support for JPEG. The local Pillow
11.3.0 oracle shows lowercase `xmp=` writes an APP1
`http://ns.adobe.com/xap/1.0/\0` packet after JFIF/EXIF and before ICC/COM
metadata, while PNG `xmp=` is a no-op and remains intentionally outside this
slice. Native JPEG metadata segment construction now accepts an XMP payload
through `pillow_c_image_save_jpeg_metadata_xmp_encode_options`; the facade
routes explicit `Image.Save(..., "JPEG", {xmp: Buffer})` through the DLL, and
reopen maps the packet back to `Info["xmp"]` / `Image.getxmp()` without AHK
per-pixel loops. Broader XMP schemas, implicit XMP preservation, additional
JPEG qtables/keep_rgb XMP combinations, PNG `PngInfo` XMP write convenience,
IPTC, and ImageCms remain future `META-002`
children. Source and Release x64 DLL export counts are now `374` / `374`, and
Release x64 was rebuilt after the ABI changed.
`META-002A` adds bounded XMP read support for PNG and JPEG open paths. The
bounded local Pillow 11.3.0 oracle shows new images and files without XMP
return `{}` from `getxmp()`, PNG `iTXt` key `XML:com.adobe.xmp` exposes raw
`Info["xmp"]` bytes plus the text key, and JPEG APP1
`http://ns.adobe.com/xap/1.0/\0` exposes raw `Info["xmp"]` bytes. Native open
now extracts those raw XMP bytes into the image handle and exposes them through
the shared `pillow_c_image_metadata_xmp` export; the facade maps reopened
metadata to `Info["xmp"]` and parses the bounded nested
`xmpmeta/RDF/Description/title/Alt/li` packet through the Python-style
`Image.GetXmp()` / `Image.getxmp()` route without AHK per-pixel loops. Broader
XMP schemas, write-side XMP options, IPTC, and ImageCms remain future
`META-002` children.
`MODE-NUM-001L` aligns numeric mode band-name materialization for the public
facade `Image.GetBands()` / `Image.getbands()` route. The bounded local Pillow
11.3.0 oracle shows `I.getbands() == ("I",)` and
`F.getbands() == ("F",)`, alongside existing byte-mode tuples such as
`RGB -> ("R", "G", "B")`. The facade instance `BandNames()` now delegates to
the shared `ModeInfo` table instead of a shorter hard-coded byte-mode list, so
numeric modes and already-registered public modes such as `I;16`, `I;16B`, and
`RGBX` expose Pillow band names without AHK per-pixel loops. No native ABI
symbol was added, and no native rebuild was required.
`MODE-NUM-001K` adds native numeric `Image.getcolors()` value materialization
for modes `I` and `F`. The bounded local Pillow 11.3.0 oracle shows mode `I`
returns scalar integer entries such as `(2, -1)`, `(1, 0)`, and `(1, 70000)`
for values `[-1, 0, -1, 70000]`, mode `F` returns scalar float entries such
as `(2, 1.5)`, `(1, 2.5)`, and `(1, 3.5)` for values
`[1.5, 2.5, 1.5, 3.5]`, maxcolor overflow returns `None`, and empty numeric
images return `[]` for nonnegative `maxcolors` but `None` for `-1`. Native
`pillow_c_image_getcolors_numeric` now counts distinct four-byte numeric
samples in the DLL and returns counts plus double materialized values so the
facade can expose scalar `I`/`F` entries without AHK per-pixel loops. Pillow's
internal hash-table ordering and broader NaN payload ordering remain out of
scope. Source and Release x64 DLL export counts are now `372` / `372`, and
Release x64 was rebuilt after the ABI changed.
`MODE-NUM-001J` aligns `Image.entropy()` for numeric storage modes with
Pillow's one-band numeric histogram semantics and wrong-mode masked boundary.
The bounded local Pillow 11.3.0 oracle shows mode `I` values
`[-1, 0, -1, 70000]` produce entropy `0.8112781244591328`, mode `F` values
`[1.5, -0.0, 1.5, 3.5]` produce entropy `1.5`, and masked entropy for modes
`I` and `F` raises `ValueError: image has wrong mode`. Native
`pillow_c_image_entropy` now routes unmasked numeric modes through the existing
numeric histogram path instead of counting the four storage bytes as four
bands, and returns `PILLOW_C_INVALID_ARGUMENT` for masked numeric entropy. The
facade maps that native rejection to `Error("image has wrong mode", -1)`
without AHK per-pixel loops. No ABI symbol was added; source and Release x64
DLL export counts remain `371` / `371`, and Release x64 was rebuilt after
native changes.
`MODE-NUM-001I` aligns `Image.blend` and the facade `ImageChops.Blend` alias
for numeric storage modes with Pillow's public wrong-mode boundary. The
bounded local Pillow 11.3.0 oracle shows `Image.blend(left, right, alpha)`
raises `ValueError: image has wrong mode` for matching mode `I` and matching
mode `F` inputs with valid alpha values, while `Image.composite` remains a
separate allowed byte-level numeric path. Native `pillow_c_image_blend` and
`pillow_c_image_blend_into` now return `PILLOW_C_INVALID_ARGUMENT` for modes
`I` and `F` before blending four-byte numeric storage as byte channels; covered
byte modes keep the existing native blend path. The facade maps that native
rejection to `Error("image has wrong mode", -1)` without AHK per-pixel loops.
No ABI symbol was added; source and Release x64 DLL export counts remain
`371` / `371`, and Release x64 was rebuilt after native changes.
`MODE-NUM-001H` aligns non-logical `ImageChops` binary operations for numeric
storage modes with Pillow's public wrong-mode boundary. The bounded local
Pillow 11.3.0 oracle shows `difference`, `multiply`, `screen`, `lighter`,
`darker`, `soft_light`, `hard_light`, `overlay`, `add`, `subtract`,
`add_modulo`, and `subtract_modulo` all raise
`ValueError: image has wrong mode` for matching mode `I` and matching mode
`F` inputs. Native shared ImageChops binary validation now returns
`PILLOW_C_INVALID_ARGUMENT` for modes `I` and `F` before any non-logical
operation can treat four-byte numeric storage as four byte channels, while
covered byte modes keep the existing native operation paths. The facade maps
that native rejection to `Error("image has wrong mode", -1)` through shared
ImageChops routing without AHK per-pixel loops. No ABI symbol was added; source
and Release x64 DLL export counts remain `371` / `371`, and Release x64 was
rebuilt after native changes.
`MODE-NUM-001G` aligns `ImageFilter.BoxBlur`, `ImageFilter.GaussianBlur`, and
`ImageFilter.UnsharpMask` for numeric storage modes with Pillow's public
wrong-mode boundary. The bounded local Pillow 11.3.0 oracle shows those three
filters raise `ValueError: image has wrong mode` for modes `I` and `F` with
valid radius/options. Native `pillow_c_image_filter_box_blur` /
`pillow_c_image_filter_box_blur_into`,
`pillow_c_image_filter_gaussian_blur` /
`pillow_c_image_filter_gaussian_blur_into`, and
`pillow_c_image_filter_unsharp_mask` /
`pillow_c_image_filter_unsharp_mask_into` now return
`PILLOW_C_INVALID_ARGUMENT` for modes `I` and `F` before treating four-byte
numeric storage as byte channels; byte modes keep the existing native blur and
sharpen paths. The facade maps that native rejection to
`Error("image has wrong mode", -1)` without AHK per-pixel loops. No ABI symbol
was added; source and Release x64 DLL export counts remain `371` / `371`, and
Release x64 was rebuilt after native changes.
`MODE-NUM-001F` aligns `ImageFilter.ModeFilter` for numeric storage modes with
Pillow's public wrong-mode boundary. The bounded local Pillow 11.3.0 oracle
shows `ImageFilter.ModeFilter(size)` raises `ValueError: image has wrong mode`
for modes `I` and `F`, including tested sizes `3`, `1`, `2`, `4`, `0`, and
`-1`. Native `pillow_c_image_filter_mode` /
`pillow_c_image_filter_mode_into` now return `PILLOW_C_INVALID_ARGUMENT` for
mode `I` and mode `F` before trying to treat four-byte numeric storage as four
byte channels; byte modes keep the existing native ModeFilter path. The facade
maps that native rejection to `Error("image has wrong mode", -1)` without AHK
per-pixel loops. No ABI symbol was added; source and Release x64 DLL export
counts remain `371` / `371`, and Release x64 was rebuilt after native changes.
`MODE-NUM-001E` extends native `ImageFilter.RankFilter` / `MinFilter` /
`MedianFilter` / `MaxFilter` semantics to the four-byte numeric storage modes
`I` and `F`. The bounded local Pillow 11.3.0 oracle shows rank filters use
Pillow's clamped-edge window semantics for signed-int32 `I` samples and
float32 `F` samples rather than sorting the four storage bytes as independent
channels. Native `pillow_c_image_filter_rank` /
`pillow_c_image_filter_rank_into` now branch mode `I` through one
little-endian signed-int32 window per pixel and mode `F` through one float32
window per pixel; byte modes keep the existing channel-generic path, and the
facade routes the public filter classes through the DLL without AHK per-pixel
loops. No ABI symbol was added; source and Release x64 DLL export counts
remain `371` / `371`, and Release x64 was rebuilt after native changes.
`MODE-NUM-001D` adds native `ImageFilter.Kernel` support for mode `I`
signed-int32 samples while keeping mode `F` on Pillow's wrong-mode rejection
path. The bounded local Pillow 11.3.0 oracle shows mode `I` `(3, 3)` kernels
copy border int32 pixels unchanged, apply the existing vertical kernel flip to
interior signed-int32 samples, round positive fractional results half-up, clip
filtered negatives to `0`, clip filtered overflow to `2147483647`, and return
`-2147483648` for explicit `scale=0`; mode `F` raises
`ValueError: image has wrong mode`. Native `pillow_c_image_filter_kernel` /
`pillow_c_image_filter_kernel_into` now treat mode `I` as one int32 sample per
pixel instead of four byte channels, and reject mode `F`; the facade routes
mode `I` through the DLL and normalizes mode `F` Kernel rejection without AHK
per-pixel loops. No ABI symbol was added; source and Release x64 DLL export
counts remain `371` / `371`, and Release x64 was rebuilt after native changes.
The native EXIF route now serializes Pillow-style `Exif\0\0` bytes for an
orientation value plus zero or more ASCII IFD0 tag/value entries through
`pillow_c_exif_entries_bytes`, parses one ASCII tag from an opened JPEG/PNG
EXIF blob through `pillow_c_exif_ascii_tag`, and now serializes/parses mixed
ASCII plus scalar integer IFD0 entries through
`pillow_c_exif_entries_typed_bytes` and `pillow_c_exif_uint_tag`; the full
serializer route `pillow_c_exif_entries_full_bytes` now adds TIFF RATIONAL
type `5` entries, and `pillow_c_exif_rational_tag` parses them from opened
JPEG/PNG EXIF blobs. `pillow_c_exif_entries_short_array_bytes` now extends
that serializer with bounded TIFF SHORT arrays, and
`pillow_c_exif_ushort_array_tag` parses those arrays from opened JPEG/PNG EXIF
blobs. `pillow_c_exif_entries_byte_array_bytes` now extends that serializer
with bounded TIFF BYTE arrays, and `pillow_c_exif_byte_array_tag` parses those
arrays from opened JPEG/PNG EXIF blobs. `pillow_c_exif_entries_signed_rational_bytes`
now extends that serializer with bounded TIFF SRATIONAL type `10` entries, and
`pillow_c_exif_signed_rational_tag` parses those signed rationals from opened
JPEG/PNG EXIF blobs. `pillow_c_exif_entries_undefined_bytes` now extends that
serializer with bounded TIFF UNDEFINED type `7` entries, and
`pillow_c_exif_undefined_tag` parses those byte payloads from opened JPEG/PNG
EXIF blobs. The bounded local Pillow 11.3.0 oracle
covers common ASCII tags `271`, `272`, `305`, and `306`; scalar integer tags
`256`/`257` as TIFF LONG plus `296`/`531` as TIFF SHORT; rational tags
`282`/`283` as `145/2` and `300/1`; SHORT array tag `530`
(`YCbCrSubSampling`) as `[2, 1]`; BYTE array tag `40091` (`XPTitle`) as
the bytes `[72,0,105,0,0,0]`; BYTE array tag `37510` (`UserComment`) as
the bytes `[99,111,109,109,101,110,116]` with Pillow's even-byte out-of-line
TIFF payload padding; SRATIONAL tag `37380`
(`ExposureBiasValue`) as `[-1, 2]`; and UNDEFINED tag `36864` (`ExifVersion`)
as bytes `[48,50,51,48]`: raw bytes match
`Image.Exif().tobytes()`, facade `Pillow.Image.Exif()` stores string,
integer, rational, bounded SHORT array, bounded BYTE array, bounded
signed-rational, and bounded UNDEFINED values together, explicit JPEG/PNG
`exif` saves round-trip, and reopened images expose all covered tags through
`getexif()`.
`META-001K` added no DLL exports; source and Release x64 DLL export counts
remained `355` / `355` at that slice, and Release x64 was rebuilt after native
serializer padding changed.
`FMT-PNG-004AJ` adds `pillow_c_image_save_png_text_entries_custom_chunks_options`
for ordinary `PngInfo.add_text("Author", "Ada")` plus multiple Pillow-style
private custom chunks before and after `IDAT`. The bounded local Pillow 11.3.0
oracle writes `IHDR`, `tEXt`, `vpAg`, `vpBg`, `IDAT`, `vpCg`, `vpDg`, `IEND`;
the native route preserves RGB bytes, exposes reopened text metadata, hides
unknown private chunks from `Info` / `Text`, and keeps compressed text, `iTXt`,
ICC, EXIF, transparency, APNG, and public/standard arbitrary chunk combinations
behind future gap IDs. Source and Release x64 DLL export counts are now `356`
/ `356`, and Release x64 was rebuilt after native PNG writer routing changed.
`FMT-PNG-004AK` adds
`pillow_c_image_save_png_text_entries_custom_chunks_kind_options` for the same
multiple Pillow-style private chunk batch shape mixed with already-covered
advanced text kinds: compressed `zTXt`, uncompressed `iTXt`, compressed
`iTXt`, and language-keyed `iTXt`. The bounded local Pillow 11.3.0 oracle
writes text first, then pre-`IDAT` private chunks in caller order, then `IDAT`,
then after-`IDAT` private chunks in caller order, then `IEND`; native reopen
preserves RGB bytes and text metadata while hiding unknown private chunks.
ICC, EXIF, transparency, APNG, public/standard arbitrary chunk combinations,
and compressed language-keyed `iTXt` remain separate future surfaces. Source
and Release x64 DLL export counts are now `357` / `357`, and Release x64 was
rebuilt after the native ABI changed.
`FMT-PNG-004AL` adds
`pillow_c_image_save_png_metadata_custom_chunks_options` as the generalized
PNG metadata plus multiple-private-chunk ABI route. The bounded local Pillow
11.3.0 oracle for ordinary `PngInfo.add_text("Author", "Ada")`, multiple
private chunks before and after `IDAT`, and `icc_profile` writes `IHDR`,
`iCCP`, `tEXt`, pre-`IDAT` private chunks, `IDAT`, post-`IDAT` private
chunks, `IEND`; native reopen preserves RGB bytes, exposes text and ICC
metadata, and hides unknown private chunks. This replaces another narrow PNG
one-combination branch with a reusable metadata/custom-chunk batch ABI. Source
and Release x64 DLL export counts are now `358` / `358`, and Release x64 was
rebuilt after the native ABI changed.
`FMT-PNG-004AM` reuses that generalized PNG metadata/custom-chunk batch ABI for
ordinary `PngInfo.add_text("Author", "Ada")`, multiple private chunks before
and after `IDAT`, and explicit `exif` bytes. The bounded local Pillow 11.3.0
oracle writes `IHDR`, `tEXt`, pre-`IDAT` private chunks, `eXIf`, `IDAT`,
post-`IDAT` private chunks, `IEND`; native reopen preserves RGB bytes, exposes
text and Pillow-style EXIF metadata including orientation `6`, and hides
unknown private chunks. No new export was added; source and Release x64 DLL
export counts remain `358` / `358`, and Release x64 was rebuilt after native
PNG writer ordering changed.
`FMT-PNG-004AN` reuses the same generalized PNG metadata/custom-chunk batch ABI
for ordinary `PngInfo.add_text("Author", "Ada")`, multiple private chunks
before and after `IDAT`, explicit `icc_profile`, and explicit `exif` bytes.
The bounded local Pillow 11.3.0 oracle writes `IHDR`, `iCCP`, `tEXt`,
pre-`IDAT` private chunks, `eXIf`, `IDAT`, post-`IDAT` private chunks,
`IEND`; native reopen preserves RGB bytes, exposes text, ICC, and
Pillow-style EXIF metadata including orientation `6`, and hides unknown
private chunks. No new export was added and no native code change or rebuild
was required; source and Release x64 DLL export counts remain `358` / `358`.
`FMT-PNG-004AO` reuses that generalized PNG metadata/custom-chunk batch ABI for
advanced text kinds (`zTXt`, uncompressed `iTXt`, compressed `iTXt`, and
uncompressed language-keyed `iTXt`), multiple private chunks before and after
`IDAT`, explicit `icc_profile`, and explicit `exif` bytes. The bounded local
Pillow 11.3.0 oracle writes `IHDR`, `iCCP`, selected advanced text chunk,
pre-`IDAT` private chunks, `eXIf`, `IDAT`, post-`IDAT` private chunks, `IEND`;
native reopen preserves RGB bytes, exposes selected text, ICC, and
Pillow-style EXIF metadata including orientation `6`, and hides unknown
private chunks. No new export was added and no native code change or rebuild
was required; source and Release x64 DLL export counts remain `358` / `358`.
`FMT-PNG-004AP` reuses that generalized PNG metadata/custom-chunk batch ABI for
compressed language-keyed `iTXt`, multiple private chunks before and after
`IDAT`, explicit `icc_profile`, and explicit `exif` bytes. The bounded local
Pillow 11.3.0 oracle writes `IHDR`, `iCCP`, compressed language-keyed `iTXt`,
pre-`IDAT` private chunks, `eXIf`, `IDAT`, post-`IDAT` private chunks,
`IEND`; native reopen preserves RGB bytes, exposes text, ICC, and
Pillow-style EXIF metadata including orientation `6`, and hides unknown
private chunks. No new export was added; Release x64 was rebuilt after native
code removed a stale compressed language-keyed `iTXt` rejection in this route.
`FMT-PNG-004AQ` reuses that same generalized route for the first proven
multiple-private metadata/options batch with `optimize=True` and RGB
`transparency=(10,20,30)`: compressed language-keyed `iTXt`, explicit
`icc_profile`, explicit `exif`, RGB `tRNS`, optimized IDAT, and multiple
private chunks. The bounded local Pillow 11.3.0 oracle writes `IHDR`, `iCCP`,
compressed language-keyed `iTXt`, pre-`IDAT` private chunks, `tRNS`, `eXIf`,
`IDAT`, post-`IDAT` private chunks, `IEND`; native reopen preserves RGB bytes
and exposes text, ICC, EXIF orientation `6`, and RGB transparency metadata.
This slice added no export and no native code change; the facade now routes
only this locally proven multi-private advanced-text ICC+EXIF RGB-transparency
optimize shape through the existing DLL route.
`FMT-GIF-004T` adds `pillow_c_image_gif_comment`,
`pillow_c_image_save_gif_comment`, and
`pillow_c_image_save_gif_animation_comment` for bounded GIF comment metadata.
The bounded local Pillow 11.3.0 oracle writes a single-frame comment extension
as `[33,254,5,104,101,108,108,111,0]`, writes animation comments after the
NETSCAPE loop extension and before the first frame GCE, and reopens both frame
0 and frame 1 with `info["comment"] == b"hello"`. The native route writes and
reads comment bytes in the DLL; the facade maps reopened GIF comments to
`Info["comment"]` as a `Buffer` and routes `Image.Save(..., "GIF",
{Comment: "hello"})` for both single-frame and comment-only `save_all` GIF
saves without AHK pixel loops. Unsupported comment combinations with
single-frame transparency or animation transparency/background/
include_color_table/optimize are explicit facade errors. Source and Release
x64 DLL export counts are now `361` / `361`, and Release x64 was rebuilt after
the native ABI changed.
`FMT-GIF-004U` adds `pillow_c_image_save_gif_comment_options` for the bounded
single-frame P-mode GIF save combination of `comment=b"hello"` plus
`transparency=1`. The bounded local Pillow 11.3.0 oracle writes a comment
extension before the Graphic Control Extension, reopens with both
`info["comment"] == b"hello"` and `info["transparency"] == 1`, and converts the
transparent palette index to alpha `0`. The native route writes both metadata
groups inside the DLL, and the facade now routes
`Image.Save(..., "GIF", {Comment: "hello", Transparency: 1})` without AHK pixel
loops. Animation comment combinations with background/include_color_table/
optimize remain explicit facade errors. Source and Release x64 DLL export
counts are now `362` / `362`, and Release x64 was rebuilt after the native ABI
changed.
`FMT-GIF-004V` adds
`pillow_c_image_save_gif_animation_comment_metadata_options` for the bounded
P-mode GIF animation save combination of `comment=b"hello"` plus
`transparency=1`. The bounded local Pillow 11.3.0 oracle writes the NETSCAPE
loop extension, then the comment extension, then frame GCEs; because frame 0
actually uses palette index `1`, both frame GCEs carry transparency index `1`.
Native now writes the combined animation metadata in the DLL, preserves comment
bytes on reopen for both frames, preserves the existing RGBA composited frame 1
behavior, and avoids treating RGBA quantizer reserve slots as source P-mode
transparency usage. The facade routes
`Image.Save(..., "GIF", {save_all: true, Comment: "hello", Transparency: 1})`
through the new native route without AHK pixel loops. Source and Release x64
DLL export counts are now `363` / `363`, and Release x64 was rebuilt after the
native ABI changed.
`FMT-GIF-004W` adds
`pillow_c_image_save_gif_animation_comment_background_options` for the bounded
P-mode GIF animation save combination of `comment=b"hello"` plus
`background=1`, `include_color_table=True`, `optimize=False`, and
`transparency=2`. The bounded local Pillow 11.3.0 oracle writes a 256-entry
global color table, logical-screen background byte `1`, NETSCAPE loop metadata,
then the comment extension before frame GCEs; both covered frames carry
transparency index `2` and local 256-entry color tables. Native now composes
that metadata/options batch in the DLL, preserves reopened comment/background
metadata, keeps frame-0 transparency metadata in P mode, and matches Pillow's
frame-1 reopened RGBA bytes after `disposal=2`. The facade routes
`Image.Save(..., "GIF", {save_all: true, Comment: "hello", Background: 1,
IncludeColorTable: true, Optimize: false, Transparency: 2})` through the new
native route without AHK pixel loops. Source and Release x64 DLL export counts
are now `364` / `364`, and Release x64 was rebuilt after the native ABI
changed.
`BYTES-001AC` covers readonly raw `L` `Image.frombuffer(..., "raw", "L", 2, 1)`
refresh before native PNG save reads source pixels. The bounded local Pillow
11.3.0 oracle shows mutating the caller bytearray after construction, then
saving to PNG, reopens with the mutated bytes `[77,2,3,4]` while the source
image remains readonly. Native PNG save now refreshes attached readonly
frombuffer views before both the default WIC writer and the custom PNG metadata
writer read `image->pixels`; the facade routes `Image.FromBuffer(...).Save(...,
"PNG")` through the DLL without AHK pixel loops. No export was added; source
and Release x64 DLL export counts remain `364` / `364`, and Release x64 was
rebuilt after native PNG save behavior changed.
`BYTES-001AD` covers readonly raw `L` `Image.frombuffer(..., "raw", "L", 8, 1)`
refresh before native JPEG save reads source pixels. The bounded local Pillow
11.3.0 oracle shows mutating all 64 caller bytes from `1` to `200` after
construction, then saving to JPEG with `quality=100`, reopens as mode `L`,
size `8x8`, and bytes all `200` while the source image remains readonly.
Native JPEG save now refreshes attached readonly frombuffer views before the
shared WIC/default JPEG writer, qtables/optimized writer, keep-rgb writer, and
qtables keep-rgb writer read `image->pixels`; the facade routes
`Image.FromBuffer(...).Save(..., "JPEG")` through the DLL without AHK pixel
loops. No export was added; source and Release x64 DLL export counts remain
`364` / `364`, and Release x64 was rebuilt after native JPEG save behavior
changed.
`BYTES-001AE` covers readonly raw `L` `Image.frombuffer(..., "raw", "L", 2, 1)`
refresh before native GIF save reads source pixels. The bounded local Pillow
11.3.0 oracle shows mutating the first caller byte from `1` to `77` after
construction, then saving to GIF, reopens as mode `P` and converts to `L`
bytes `[77,2,3,4]` while the source image remains readonly. Native GIF save
now refreshes attached readonly frombuffer views before the shared single-frame
helper, indexed writer, and animation frame validation/quantization paths read
`image->pixels`; the facade routes `Image.FromBuffer(...).Save(..., "GIF")`
through the DLL without AHK pixel loops. No export was added; source and
Release x64 DLL export counts remain `364` / `364`, and Release x64 was
rebuilt after native GIF save behavior changed.
`BYTES-001AF` covers readonly raw `L` `Image.frombuffer(..., "raw", "L", 2, 1)`
refresh before the remaining native save formats read source pixels. The
bounded local Pillow 11.3.0 oracle shows mutating the first caller byte from
`1` to `77` after construction, then saving to BMP, TIFF, PPM/PGM, and TGA;
each reopen returns bytes `[77,2,3,4]` while the source image remains readonly.
Native save paths now refresh attached readonly frombuffer views before BMP,
TIFF frame, PPM/PGM, TGA, QOI, XBM, and ICO encoders read `image->pixels`; the
facade routes `Image.FromBuffer(...).Save(...)` for the covered public BMP,
TIFF, PPM, and TGA formats through the DLL without AHK pixel loops. No export
was added; source and Release x64 DLL export counts remain `364` / `364`, and
Release x64 was rebuilt after native save behavior changed.
`FMT-ICO-001A` covers ICO `append_images` exact source-size entry selection.
The bounded local Pillow 11.3.0 oracle shows `_save` builds
`provided_ims = [base] + append_images`, sorts/deduplicates `sizes`, skips
sizes larger than the base image, uses the first provided image with the exact
requested size, and otherwise thumbnails the last provided image. The covered
fixture saves a `32x32` red RGBA base plus `16x16` green and `24x24` blue
append images with default ICO sizes, yielding PNG-backed ICO directory
entries `[16,16]`, `[24,24]`, and `[32,32]` whose decoded first pixels come
from the matching source image. Native adds
`pillow_c_image_save_ico_frames_format_options`, keeps old single-image ICO
exports routed through the same DLL helper, and the facade routes
`Image.Save(..., "ICO", {AppendImages: [...]})` without AHK pixel loops.
Source and Release x64 DLL export counts are now `365` / `365`, and Release
x64 was rebuilt after the native ABI changed.
`FMT-ICO-002A` covers Pillow's public ICO size-selection load behavior. The
bounded local Pillow 11.3.0 oracle shows `Image.open(path)` loads the largest
ICO frame by default, `im.ico.sizes()` contains `{(16,16), (24,24), (32,32)}`,
assigning `im.size = (16,16)` then calling `load()` loads the green `16x16`
frame, and assigning a size outside `info["sizes"]` is rejected by the public
size setter. Native adds `pillow_c_image_open_ico_size(path, width, height,
out_image)` for exact-size ICO frame decode through WIC, and the facade routes
`image.Size := [w,h]` for opened ICO images by swapping to the requested native
handle without AHK pixel loops. CUR hotspot semantics, `im.ico.getimage(...)`
fallback behavior for missing sizes, non-exact append-image thumbnail tests,
and broader mixed-mode multi-source ICO matrices remain future surfaces. Source
and Release x64 DLL export counts are now `366` / `366`, and Release x64 was
rebuilt after the native ABI changed.
`FMT-ICO-002B` covers Pillow's public ICO `ico.sizes()` and
`ico.getimage(size)` route. The bounded local Pillow 11.3.0 oracle shows
`im.ico.sizes()` exposes `{(16,16), (24,24), (32,32)}`, `getimage((16,16))`
returns a new green `16x16` `RGBA` image without mutating the opened largest
`32x32` image, and `getimage((18,18))` falls back to the largest red `32x32`
frame. Native adds `pillow_c_image_ico_sizes(path, out_sizes, out_pair_count,
out_required)` to enumerate sorted unique frame sizes through WIC, and the
facade routes `image.ico.sizes()` and `image.ico.getimage([w,h])` through the
DLL without AHK pixel loops. CUR hotspot semantics, non-exact append-image
thumbnail tests, embedded-payload `format` metadata, and broader mixed-mode
multi-source ICO matrices remain future
surfaces. Source and Release x64 DLL export counts are now `367` / `367`, and
Release x64 was rebuilt after the native ABI changed.
`FMT-ICO-002C` covers Pillow's duplicate-size ICO color-depth selection on
open. The bounded local Pillow 11.3.0 oracle shows ICO directory entries are
ordered by Pillow's stable color-depth-ascending, square-descending rule, so a
duplicate `16x16` PNG-backed pair with an `8bpp` red entry before a `32bpp`
green entry opens the red entry for both default-largest and exact-size
routes. Native now parses the ICO directory, chooses the Pillow-ordered entry,
wraps only that payload as a one-entry ICO, and decodes it through WIC so WIC's
own duplicate-size ordering cannot select the wrong frame. The existing
`pillow_c_image_open_ico` and `pillow_c_image_open_ico_size` exports are
unchanged; facade `Image.Open(...)`, `image.Size := [w,h]`, and
`image.ico.getimage(...)` inherit the corrected DLL behavior without AHK pixel
loops. No ABI symbol was added; source and Release x64 DLL export counts remain
`367` / `367`, and Release x64 was rebuilt after native ICO open behavior
changed.
`FMT-ICO-002D` covers Pillow's embedded PNG payload `format` metadata for
`ico.getimage(...)`. The bounded local Pillow 11.3.0 oracle shows top-level
`Image.open(path)` remains `format == "ICO"` before and after selected-size
`load()`, while PNG-backed `im.ico.getimage((16,16))` and the missing-size
fallback `im.ico.getimage((18,18))` return images with `format == "PNG"` and
`format_description == "Portable network graphics"`. Native adds
`pillow_c_image_ico_payload_format(path, width, height, require_requested_size,
out_format, out_size, out_required)` to apply the same ICO entry selection as
open and expose a bounded payload format string without decoding pixels in
AHK; the facade sets `Format` on images returned by `ico.getimage(...)`.
BMP/DIB-backed payload metadata is covered by `FMT-ICO-002E`; CUR and broader
embedded-payload metadata remain future surfaces. Source and Release x64 DLL
export counts were then `368` / `368`, and Release x64 was rebuilt after the
native ABI changed.
`FMT-ICO-002E` covers Pillow's DIB-backed payload metadata for
`ico.getimage(...)`. The bounded local Pillow 11.3.0 oracle shows
lowercase `bitmap_format="bmp"` ICO entries return child images with
`format is None`, `format_description is None`,
`info["compression"] == 0`, and `info["dpi"] ~= (96.012, 96.012)` for both
exact-size and missing-size fallback `getimage(...)` calls. Native adds
`pillow_c_image_ico_payload_dib_metadata(path, width, height,
require_requested_size, out_has_dib, out_has_dpi, out_dpi_x, out_dpi_y,
out_compression)` to parse the selected DIB payload in the DLL, and the facade
maps those values into returned child `Info` without AHK pixel loops. Bounded
CUR open is covered by `FMT-ICO-002F`; CUR save/hotspot exposure and broader
embedded-payload metadata remain future surfaces. Source and Release x64 DLL
export counts were then `369` / `369`, and Release x64 was rebuilt after the
native ABI changed.
`FMT-ICO-002F` covers bounded DIB-backed CUR open behavior. The local Pillow
11.3.0 oracle shows a CUR file built from a DIB-backed cursor payload opens as
`format == "CUR"`, `format_description == "Windows Cursor"`, mode `RGBA`,
size `16x16`, preserves decoded RGBA bytes, and exposes
`info["compression"] == 0` plus `info["dpi"] ~= (96.012, 96.012)`. Native adds
`pillow_c_image_open_cur` plus `pillow_c_image_metadata_dib_compression`;
`open_cur` parses the CUR directory, wraps the selected DIB payload as a
one-entry ICO for WIC decode, and attaches DIB DPI/compression metadata to the
image handle. The facade recognizes `.cur` / `CUR`, maps the metadata through
`ApplyNativeMetadata()`, and explicitly rejects CUR save until a save gap is
covered. Source and Release x64 DLL export counts are now `371` / `371`, and
Release x64 was rebuilt after the native ABI changed.
`API-IMG-001C` covers the bounded `Image.get_child_images()` public object API
for currently implemented image handles. The local Pillow 11.3.0 oracle shows
new images and opened PNG/JPEG/GIF/ICO images return an empty list. The AHK
facade now exposes `Image.GetChildImages()` plus `Image.get_child_images()` as
a handle-validated empty `Array` result without AHK pixel loops and without a
native ABI change. Source and Release x64 DLL export counts remain `371` /
`371`; no native rebuild was required for this facade-only slice.
Previous `FMT-PNG-001AB` PNG dynamic-Huffman compressed text open metadata
remains covered.
Previous `FMT-GIF-002F` GIF composited-frame alpha parity remains covered.
Previous `FMT-PNG-004AK` PNG advanced text plus multiple private chunks
remains covered.
Previous `FMT-PNG-004AI` PNG `sPLT` public chunk save support remains covered.
Previous `API-IMG-001B` `has_transparency_data` coverage remains in place, and
previous `API-IMG-001A` `format_description` coverage remains in place.
Previous `FMT-GIF-004R` GIF all-identical-frame collapse rejection remains
covered, and previous readonly-frombuffer coverage remains through
`BYTES-001AF`.
Default next gap: continue GIF correctness only for a new post-`FMT-GIF-002F`
local oracle; otherwise choose a bounded PNG/JPEG/META/TIFF/BYTES/API/ICO
child only when it proves a reusable semantic boundary.
TIFF, BYTES, and META children remain available only for separately proven
tag/compression/high-bit/mode, readonly/detach, or EXIF/TIFF object lifecycle
boundaries.
Avoid PNG one-combination branches; extend the generalized PNG metadata route
and batch same-route cases when semantics are already known.
Current implementation WIP: no active code WIP after `META-001M`; select
the next exact bounded behavior and add red raw/facade AHK tests before
implementation. Current runtime stability fix: `ahk/pillow.ahk` pins the
configured native DLL with `LoadLibraryW` for process lifetime, preventing
path-based `DllCall` load/unload churn during facade-heavy suites. This added
no ABI symbol and required no native rebuild.
The direct audit has been refreshed against local Pillow 11.3.0, source
exports, DLL exports, facade API names, module surfaces, optional Pillow
feature flags, registered format IDs, and current test counts. The speed rule
is to pick only gaps that buy a reusable semantic pillar, a new native route,
a new ABI shape, or a locally proven Pillow boundary.
Native rebuild needed: only after touching src/pillow_c.cpp or project files.
Test shape: parent tools runner, -TimeoutSeconds 120, no parallel AHK tests.
```

Known current counts:

- AHK tests in tree: `1326` total: `662` raw DLL / `664` facade. Latest
  `META-001M` red evidence: raw
  `pillow_c image open_tiff exposes common ASCII EXIF tags` failed with
  `Expected "desc", got ""`, and facade
  `Pillow Image.getexif reads TIFF common ASCII tags` failed with
  `Expected 6, got 5`, proving TIFF tag `270` was absent from the native blob
  and public `GetExif()` object. Green evidence: Release x64 rebuild succeeded
  with `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in `140ms`;
  targeted facade passed `1/1` in `31ms`; raw TIFF filter passed `27/27` in
  `1828ms`; facade TIFF filter passed `26/26` in `265ms`; raw EXIF filter
  passed `35/35` in `3078ms`; facade EXIF filter passed `37/37` in `500ms`;
  full AHK directory invocation passed `1326/1326` in `7578ms` with the known
  non-failing libjpeg stderr warnings. Source/DLL export counts remain `379`
  / `379`.
  Previous `FMT-PNG-001AD` red evidence: raw
  `pillow_c image save_png_text_entries_value_sizes_options preserves embedded NUL text`
  failed with a missing `pillow_c_image_save_png_text_entries_value_sizes_options`
  export, and facade
  `Pillow Image.Save PNG pnginfo add_text bytes preserves embedded NUL`
  errored with `Pillow.PngInfo.add_text bytes with embedded NUL are not supported`.
  Green evidence: targeted raw passed `1/1` in `187ms`; targeted facade passed
  `1/1` in `47ms`; PNG filter passed `206/206` in `1688ms`; full AHK
  directory invocation passed `1326/1326` in `7344ms` with the known
  non-failing libjpeg stderr warnings. Source/DLL export counts are now `379`
  / `379`, and Release x64 was rebuilt after native code changed.
  Previous `MODE-NUM-001O` red evidence: facade
  `Pillow ImageOps histogram transforms reject numeric modes like Pillow`
  failed with `Expected "not supported for mode I", got "pillow_c: invalid argument"`,
  proving the facade surfaced a generic native error even though the DLL
  rejected numeric equalize/autocontrast histogram transforms. Green evidence:
  targeted facade passed `1/1` in `16ms`; targeted raw passed `1/1` in `78ms`;
  ImageOps filter passed `62/62` in `62ms`; full AHK directory invocation
  passed `1320/1320` in `4063ms` with the known non-failing libjpeg stderr
  warnings. Source/DLL export counts remain `378` / `378`, and no native
  rebuild was required.
  Previous `MODE-NUM-001N` red evidence: facade
  `Pillow ImageOps LUT transforms reject numeric modes like Pillow` failed
  with `Expected "not supported for mode I", got "pillow_c: invalid argument"`,
  proving the facade surfaced a generic native error even though the DLL
  rejected the numeric ImageOps LUT operation. Green evidence: targeted facade
  passed `1/1` in `16ms`; targeted raw passed `1/1` in `63ms`; ImageOps filter
  passed `60/60` in `62ms`; full AHK directory invocation passed `1318/1318`
  in `4156ms` with the known non-failing libjpeg stderr warnings. Source/DLL
  export counts remain `378` / `378`, and no native rebuild was required.
  Previous `MODE-NUM-001M` red evidence: facade
  `Pillow ImageStat.Stat rejects empty numeric modes` failed with
  `Expected "min/max not given", got "Expected empty numeric ImageStat.Stat to reject"`,
  proving empty `I`/`F` images were accepted through the numeric histogram
  route. Green evidence: targeted facade passed `1/1` in `16ms`; ImageStat
  filter passed `3/3` in `16ms`; full AHK directory invocation passed
  `1316/1316` in `4110ms` with the known non-failing libjpeg stderr warnings.
  Source/DLL export counts remain `378` / `378`, and no native rebuild was
  required.
  Previous `META-001L` red evidence: raw
  `pillow_c image open_tiff exposes common ASCII EXIF tags` failed with
  `Expected pillow_c_image_metadata_tiff_exif export`; facade
  `Pillow Image.getexif reads TIFF common ASCII tags` failed with
  `Expected 5, got 1`, proving TIFF common ASCII IFD0 tags were absent from
  `GetExif()` and the native metadata surface. Green evidence: Release x64
  rebuild succeeded with `0 Warning(s), 0 Error(s)`; targeted raw passed
  `1/1`; targeted facade passed `1/1`; TIFF filter passed `53/53`; EXIF
  filter passed `72/72`; full AHK directory invocation passed `1315/1315` in
  `4016ms` with the known non-failing libjpeg stderr warning. Source/DLL
  export counts are now `378` / `378`.
  Previous `FMT-JPEG-002B2Q` red evidence: raw
  `pillow_c image save_jpeg_encode_keep_rgb_options writes RGB DPI JFIF`
  failed with `Expected 0, got -3`; facade
  `Pillow Image.Save JPEG keep_rgb dpi writes JFIF and info` errored with
  `pillow_c: invalid argument`, proving bounded RGB keep-rgb DPI saves were
  rejected before JFIF/APP14 emission. Green evidence: Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in
  `32ms`; targeted facade passed `1/1` in `16ms`; raw keep-rgb filter passed
  `20/20` in `1265ms`; facade keep-rgb filter passed `19/19` in `250ms`; full
  AHK directory invocation passed `1313/1313` in `4171ms` with the known
  non-failing libjpeg stderr warnings. Source/DLL export counts remain `377` /
  `377`.
  Previous `FMT-JPEG-002B2P` red evidence: raw
  `pillow_c image save_jpeg_qtables_metadata_keep_rgb_encode_options writes
  RGB DQT and metadata` failed with `Expected 0, got -3`; facade
  `Pillow Image.Save JPEG qtables keep_rgb metadata writes RGB components and
  info` failed with
  `Pillow.Image.Save JPEG qtables with metadata and keep_rgb is not supported`,
  proving the RGB qtables+keep_rgb+metadata route was still rejected before
  reaching the DLL. Green evidence: Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in `94ms`; targeted
  facade passed `1/1` in `16ms`; facade keep-rgb metadata filter passed `3/3`
  in `79ms`; raw keep-rgb metadata filter passed `11/11` in `844ms`; full AHK
  directory invocation passed `1311/1311` in `4297ms` with the known
  non-failing libjpeg stderr warnings. Source/DLL export counts remain `377` /
  `377`.
  Previous `META-002F` red evidence: facade
  `Pillow Image.getxmp parses repeated RDF li sequences` failed with
  `Expected value to be true` at the assertion that `creator.Seq.li` is an
  array, proving repeated `rdf:li` children were overwritten. Green evidence:
  targeted raw repeated-RDF XMP bytes passed `1/1` in `31ms`; targeted facade
  repeated-RDF `getxmp()` passed `1/1` in `31ms`; facade XMP filter passed
  `6/6` in `63ms`; full AHK directory invocation passed `1309/1309` in
  `3984ms` with the known non-failing libjpeg stderr warnings. Source/DLL
  export counts remain `377` / `377`, and no native rebuild was required.
  Previous `FMT-PNG-004AT` green evidence: targeted raw passed
  `1/1` in `157ms`; targeted facade passed `1/1` in `32ms`; full AHK
  directory invocation passed `1307/1307` in `4079ms` with the known
  non-failing libjpeg stderr warnings. Previous
  `META-002E` red evidence: raw
  `pillow_c image save_jpeg_qtables_metadata_keep_rgb_xmp_encode_options
  writes RGB DQT and XMP` failed with
  `Expected pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_xmp_encode_options
  export`; facade `Pillow Image.Save JPEG qtables keep_rgb and xmp options
  write RGB DQT and XMP` failed with
  `Pillow.Image.Save JPEG qtables with metadata and keep_rgb is not supported`,
  proving the combined qtables+keep_rgb+XMP route was missing. Green evidence:
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`; targeted raw
  passed `1/1` in `78ms`; targeted facade passed `1/1` in `16ms`; full AHK
  directory invocation passed `1301/1301` in `4266ms` with the known
  non-failing libjpeg stderr warnings; source/DLL export counts are now `377`
  / `377`. Previous `META-002D` red evidence: raw
  `pillow_c image save_jpeg_metadata_keep_rgb_xmp_encode_options writes RGB
  components and XMP` failed with
  `Expected pillow_c_image_save_jpeg_metadata_keep_rgb_xmp_encode_options
  export`; facade `Pillow Image.Save JPEG keep_rgb and xmp options write RGB
  components and XMP` failed with `Pillow.Image.Save JPEG keep_rgb with xmp is
  not supported`, proving the no-qtables keep_rgb+XMP route was missing. Green
  evidence: Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`;
  targeted raw passed `1/1` in `32ms`; targeted facade passed `1/1` in
  `15ms`; full AHK directory invocation passed `1299/1299` in `3828ms` with
  the known non-failing libjpeg stderr warnings; source/DLL export counts are
  now `376` / `376`. Previous `META-002C` red evidence: raw
  `pillow_c image save_jpeg_qtables_metadata_xmp_encode_options writes custom
  DQT and XMP` failed with
  `Expected pillow_c_image_save_jpeg_qtables_metadata_xmp_encode_options
  export`; facade `Pillow Image.Save JPEG qtables and xmp options write DQT
  and XMP` failed with `Pillow.Image.Save JPEG qtables with xmp is not
  supported`, proving the qtables+XMP route was missing. Green evidence:
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`; targeted raw
  passed `1/1` in `109ms`; targeted facade passed `1/1` in `47ms`; full AHK
  directory invocation passed `1297/1297` in `7062ms` with the known
  non-failing libjpeg stderr warnings; source/DLL export counts are now `375`
  / `375`. Previous `META-002B` red evidence: raw
  `pillow_c image save_jpeg_metadata_xmp_encode_options writes and reads XMP
  marker` failed with
  `Expected pillow_c_image_save_jpeg_metadata_xmp_encode_options export`;
  facade `Pillow Image.Save JPEG xmp option round-trips XMP packet` failed
  before `Info["xmp"]` was written on reopen, proving explicit JPEG XMP save
  was missing. Green evidence: Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in `110ms`; targeted
  facade passed `1/1` in `47ms`; full AHK directory invocation passed
  `1295/1295` in `7234ms` with the known non-failing libjpeg stderr warnings;
  source/DLL export counts are now `374` / `374`. Previous `META-002A` red
  evidence: raw `pillow_c image open_png/open_jpeg read XMP metadata` failed
  with `Expected pillow_c_image_metadata_xmp export`; facade
  `Pillow Image.getxmp reads PNG and JPEG XMP packets` failed before
  `Info["xmp"]` was exposed, proving opened PNG/JPEG XMP metadata was missing.
  Green evidence: targeted raw passed `1/1` in `125ms`; targeted facade passed
  `1/1` in `47ms`; full AHK directory invocation passed `1293/1293` in
  `7578ms`. Previous `MODE-NUM-001L` red evidence: facade
  `Pillow Image.getbands alias returns Pillow mode band names` failed with
  `Expected ["I"], got []`, proving the public band-name route did not expose
  Pillow's numeric `I`/`F` band names. Green evidence: targeted facade
  `getbands` passed `1/1` in `32ms`; adjacent `GetBands` regression passed
  `1/1` in `15ms`; full AHK directory invocation passed `1291/1291` in
  `7344ms` with the known non-failing libjpeg stderr warnings; no native ABI
  symbol was added and no native rebuild was required. Previous
  `MODE-NUM-001K` red evidence: raw
  `pillow_c image getcolors_numeric matches numeric I and F modes` failed
  with nonexistent `pillow_c_image_getcolors_numeric`; facade
  `Pillow Image.GetColors matches numeric I and F modes` failed with
  `Expected [2, -1], got 0`, proving the public route returned byte-storage
  colors instead of numeric scalar values. Green evidence: Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in
  `141ms`; targeted facade passed `1/1` in `31ms`; full AHK directory
  invocation passed `1290/1290` in `7188ms` with the known non-failing libjpeg
  stderr warnings; source/DLL export counts are now `372` / `372`. Previous
  `MODE-NUM-001J` red evidence: raw
  `pillow_c image entropy matches numeric I and F modes` failed with
  `Expected 0.81127812445913283 +/- 9.9999999999999995e-08, got 3.375`;
  facade `Pillow Image.Entropy matches numeric I and F modes` failed with the
  same wrong byte-storage entropy value. Green evidence: Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in
  `109ms`; targeted facade passed `1/1` in `62ms`; full AHK directory
  invocation passed `1288/1288` in `7266ms` with the known non-failing libjpeg
  stderr warnings; source/DLL export counts remain `371` / `371`. Previous
  `MODE-NUM-001I` red evidence: raw
  `pillow_c image blend rejects numeric I and F modes` failed with
  `Expected -3, got 0`; facade
  `Pillow Image.Blend rejects numeric I and F modes` failed without
  `image has wrong mode`. Green evidence: Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in `109ms`; targeted
  facade passed `1/1` in `16ms`; full AHK directory invocation with
  `-TimeoutSeconds 120` passed `1286/1286` in `7500ms` with the known
  non-failing libjpeg stderr warnings; source/DLL export counts remain `371` /
  `371`. Previous `MODE-NUM-001H` red evidence: raw
  `pillow_c image ImageChops binary ops reject numeric I and F modes` failed
  with `Expected -3, got 0`; facade
  `Pillow ImageChops binary ops reject numeric I and F modes` failed without
  `image has wrong mode`. Green evidence: Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in `281ms`; targeted
  facade passed `1/1` in `16ms`; full AHK directory invocation with
  `-TimeoutSeconds 120` passed `1284/1284` in `11297ms` with the known
  non-failing libjpeg stderr warnings; source/DLL export counts remain `371` /
  `371`. Previous `API-IMG-001C` red evidence: facade
  `Pillow Image get_child_images returns empty array` failed with
  `This value of type "Pillow.Image" has no method named "GetChildImages"`.
  Green evidence: targeted facade passed `1/1` in `16ms`; the complete facade
  file passed `637/637` in `2265ms`; the then-current full AHK directory invocation
  with `-TimeoutSeconds 120` passed `1274/1274` in `3657ms` with the known
  non-failing libjpeg stderr warnings; source/DLL export counts remain `371` /
  `371` and no native rebuild was required. Previous `FMT-ICO-002F` red
  evidence: raw
  `pillow_c image open_cur reads DIB cursor metadata` failed with nonexistent
  `pillow_c_image_open_cur`; facade
  `Pillow Image.Open supports CUR through native path` failed with
  `Pillow image file format is unsupported`. Green evidence: Release x64
  rebuild first exposed and then fixed a WIC const-pointer compile error, then
  succeeded with `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in
  `63ms`; targeted facade passed `1/1` in `31ms`; raw ICO regression passed
  `14/14` in `375ms`; facade ICO regression passed `10/10` in `78ms`; the
  full AHK directory invocation with `-TimeoutSeconds 120` passed
  `1273/1273` in `4016ms` with the known non-failing libjpeg stderr warnings;
  source/DLL export counts are now `371` / `371`. Previous `FMT-ICO-002E` red
  evidence: raw
  `pillow_c image ico_payload_dib_metadata reports dpi and compression` failed
  with nonexistent `pillow_c_image_ico_payload_dib_metadata`; facade
  `Pillow Image.Open ICO getimage exposes DIB metadata` failed because
  `Info["compression"]` was absent. Green evidence: Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in
  `63ms`; targeted facade passed `1/1` in `32ms`; raw ICO regression passed
  `14/14` in `328ms`; facade ICO regression passed `10/10` in `78ms`; the
  latest full AHK directory invocation with `-TimeoutSeconds 120` passed
  `1271/1271` in `3890ms` with the known non-failing libjpeg stderr warnings;
  source/DLL export counts were then `369` / `369`. Previous `FMT-ICO-002D` red
  evidence: raw `pillow_c image ico_payload_format reports embedded PNG format`
  failed with nonexistent `pillow_c_image_ico_payload_format`; facade
  `Pillow Image.Open ICO getimage exposes embedded PNG format` failed with
  `Expected "PNG", got ""`. Green evidence: Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in `32ms`; targeted
  facade passed `1/1` in `31ms`; raw ICO regression passed `13/13` in `312ms`;
  facade ICO regression passed `9/9` in `63ms`; full AHK directory invocation
  passed `1269/1269` in `4016ms`; source/DLL export counts were then
  `368` / `368`. Previous `FMT-ICO-002C` red evidence: raw
  `pillow_c image open_ico_size chooses Pillow duplicate-size color depth`
  failed with the duplicate `16x16` frame decoded as green `[0,255,0,255]`
  instead of Pillow's red `[255,0,0,255]`; facade
  `Pillow Image.Open ICO chooses Pillow duplicate-size color depth` failed
  with the same wrong frame. Green evidence: Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in `78ms`; targeted
  facade passed `1/1` in `32ms`; raw ICO regression passed `12/12` in `344ms`;
  facade ICO regression passed `8/8` in `63ms`; full AHK directory invocation
  passed `1267/1267` in `3812ms`; source/DLL export counts remained
  `367` / `367`. Previous `FMT-ICO-002B` red evidence: raw
  `pillow_c image ico_sizes reports available icon frames` failed with
  `Call to nonexistent function` for `pillow_c_image_ico_sizes`; facade
  `Pillow Image.Open ICO getimage returns requested or largest frame` failed
  with `Pillow.Image` having no `ico` property. Green evidence: Release x64
  rebuild succeeded with `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1`
  in `78ms`; targeted facade passed `1/1` in `31ms`; raw ICO regression passed
  `11/11` in `265ms`; facade ICO regression passed `7/7` in `63ms`; the full
  AHK directory invocation passed `1265/1265` in `3765ms`; source/DLL export
  counts were then `367` / `367`. Previous `FMT-ICO-002A` red evidence:
  raw `pillow_c image open_ico_size reads requested icon frame` failed with
  `Call to nonexistent function` for `pillow_c_image_open_ico_size`; facade
  `Pillow Image.Open ICO size setter loads requested frame` failed with
  `Property is read-only` for `Size`. Green evidence: Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in
  `62ms`; targeted facade passed `1/1` in `31ms`; raw ICO regression passed
  `10/10` in `265ms`; facade ICO regression passed `6/6` in `47ms`; full AHK
  directory invocation passed `1263/1263` in `3735ms`; source/DLL export counts
  were then `366` / `366`. Previous `FMT-ICO-001A` red evidence:
  raw `pillow_c image save_ico_frames_format_options uses append_images for
  exact sizes` failed before implementation with `Call to nonexistent function`
  for `pillow_c_image_save_ico_frames_format_options`; facade
  `Pillow Image.Save ICO append_images uses exact source sizes` failed with
  the `16x16` ICO entry decoded from the red base pixel `[255,0,0,255]`
  instead of the green append image pixel `[0,255,0,255]`. Green evidence:
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`; targeted raw
  passed `1/1` in `125ms`; targeted facade passed `1/1` in `47ms`; raw ICO
  regression passed `9/9` in `265ms`; facade ICO regression passed `5/5` in
  `62ms`; full AHK directory invocation passed `1261/1261` in `4031ms` with the
  known non-failing libjpeg stderr warnings; source/DLL export counts were then
  `365` / `365`. Previous
  `BYTES-001AF` red evidence: raw `pillow_c image remaining saves refresh
  frombuffer readonly view` failed with `Expected [77, 2, 3, 4], got
  [1, 2, 3, 4]`; facade `Pillow Image.FromBuffer remaining Saves refresh
  readonly view` failed with the same stale bytes for the remaining save
  formats. Green evidence: Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`; targeted `remaining saves refresh` passed `2/2`
  in `79ms`; combined FromBuffer regression passed `77/77` in `78ms`; the
  latest full AHK directory invocation with `-TimeoutSeconds 120` passed
  `1259/1259` in `4031ms` with the known non-failing libjpeg stderr warnings;
  source/DLL export counts remain `364` / `364`. Previous
  `BYTES-001AD` red evidence: raw `pillow_c image save_jpeg refreshes
  frombuffer readonly view` failed with JPEG bytes still all `1` instead of
  all `200`; facade `Pillow Image.FromBuffer Save JPEG refreshes readonly view`
  failed with the same stale JPEG bytes. Green evidence: Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in
  `78ms`; targeted facade passed `1/1` in `47ms`; facade `Image.Save JPEG`
  regression passed `67/67` in `1437ms`; combined FromBuffer regression passed
  `73/73` in `93ms`; that slice's full AHK directory invocation with
  `-TimeoutSeconds 120` passed `1255/1255` in `7187ms` with the known
  non-failing libjpeg stderr warnings; source/DLL export counts remain
  `364` / `364`. Previous `BYTES-001AC` red evidence: raw
  `pillow_c image save_png refreshes frombuffer readonly view` failed with
  `Expected [77, 2, 3, 4], got [1, 2, 3, 4]`; facade
  `Pillow Image.FromBuffer Save PNG refreshes readonly view` failed with the
  same stale PNG bytes. Green evidence: Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in `62ms`; targeted
  facade passed `1/1` in `16ms`; raw `save_png` regression passed `75/75` in
  `6312ms`; facade `Image.Save PNG` regression passed `77/77` in `1422ms`;
  combined FromBuffer regression passed `71/71` in `78ms`; that slice's full
  AHK directory invocation with `-TimeoutSeconds 120` passed `1253/1253` in
  `11812ms` with the known non-failing libjpeg stderr warnings; source/DLL
  export counts remain `364` / `364`. Previous `FMT-GIF-004W` red evidence:
  raw animation comment plus background/options coverage first failed with the
  missing `pillow_c_image_save_gif_animation_comment_background_options`;
  facade coverage errored at the existing unsupported `save_all` comment plus
  background/include_color_table/optimize guard. During implementation, raw GIF
  coverage exposed first-frame transparency GCE handling for the short-palette
  default optimized `transparency=1` case, and raw/facade GIF open regressions
  pinned Pillow 11.3.0's RGBA canvas disposal=2 behavior as clearing to
  transparent black. Green evidence: Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1`; targeted facade
  passed `1/1`; raw `pillow_c image save_gif_animation` regression passed
  `33/33`; facade `Pillow Image.Save GIF` regression passed `41/41`; raw
  `pillow_c image open_gif_frame` regression passed `4/4`; facade
  `Pillow Image.Open GIF` regression passed `7/7`; the latest full AHK
  directory invocation with `-TimeoutSeconds 120` passed `1251/1251` in
  `3562ms` with the known non-failing libjpeg stderr warnings; source/DLL
  export counts are `364` / `364`. Previous `FMT-GIF-004V` red evidence:
  raw animation comment plus transparency coverage first failed with missing
  `pillow_c_image_save_gif_animation_comment_metadata_options`; facade
  coverage errored at the existing unsupported `save_all` comment plus
  transparency guard. During implementation, raw coverage exposed the missing
  first-frame P-mode transparency GCE, and raw GIF save regression exposed an
  over-broad RGBA quantizer-slot transparency condition before the fix was
  narrowed to source P-mode frames. Green evidence: Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`; targeted raw passed `1/1` in
  `125ms`; targeted facade passed `1/1` in `16ms`; raw `save_gif` regression
  passed `41/41` in `2718ms`; facade `Image.Save GIF` regression passed
  `40/40` in `343ms`; facade `Image.Open GIF` regression passed `7/7` in
  `109ms`; raw `open_gif` regression passed `5/5` in `297ms`; the latest full
  AHK directory invocation with `-TimeoutSeconds 120` passed `1249/1249` in
  `6718ms` with the known non-failing libjpeg stderr warnings; source/DLL
  export counts are `363` / `363`. Previous `FMT-GIF-004U` red evidence:
  raw single-frame comment plus transparency coverage failed with missing
  `pillow_c_image_save_gif_comment_options`; facade coverage errored at
  `Pillow.Image.Save GIF comment with transparency is not supported`. Green
  evidence: Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`;
  targeted raw passed `1/1` in `78ms`; targeted facade passed `1/1` in `31ms`;
  raw `save_gif` regression passed `40/40` in `2609ms`; facade
  `Image.Save GIF` regression passed `39/39` in `344ms`; facade
  `Image.Open GIF` regression passed `7/7` in `93ms`; raw `open_gif`
  regression passed `5/5` in `313ms`; full AHK directory invocation passed
  `1247/1247` in `7047ms` with the known non-failing libjpeg stderr warnings.
  Previous `FMT-GIF-004T` red evidence:
  raw/facade single-frame and save-all comment coverage initially required new
  DLL exports and facade routing for GIF comment metadata. Green evidence:
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`; targeted raw
  `save_gif_comment` passed `1/1` in `47ms`; targeted raw
  `save_gif_animation_comment` passed `1/1` in `78ms`; targeted facade GIF
  comment option passed `1/1` in `32ms`; targeted facade GIF save-all comment
  option passed `1/1` in `32ms`; full AHK directory verification passed
  `1245/1245` in `6797ms`. Previous `FMT-PNG-004AQ` red evidence:
  raw targeted coverage passed immediately on the existing generalized native
  route, proving the DLL already produced the locally probed Pillow ordering
  and metadata; facade targeted coverage errored at
  `Pillow.Image.Save pnginfo with icc_profile and transparency is not supported`.
  Green evidence: targeted raw passed `1/1` in `110ms`; targeted facade passed
  `1/1` in `31ms`; raw `save_png` regression passed `74/74` in `5422ms`;
  facade `Image.Save PNG` regression passed `77/77` in `750ms`; the latest
  full AHK directory invocation with `-TimeoutSeconds 120` passed
  `1241/1241` in `6296ms` with the known non-failing libjpeg stderr warnings;
  source/DLL export counts are `358` / `358`. Previous `FMT-PNG-004AP` red
  evidence: raw targeted coverage failed with `Expected 0, got -3`, and facade
  targeted coverage errored at the existing
  `Pillow.Image.Save pnginfo multiple custom chunks with other metadata or options is not supported`
  guard. Green evidence: Release x64 rebuild succeeded after native code
  changed; targeted raw passed `1/1` in `140ms`; targeted facade passed `1/1`
  in `46ms`; raw PNG save regression passed `73/73` in `5375ms`; facade PNG
  save regression passed `76/76` in `750ms`; full AHK directory verification
  passed `1239/1239` in `6609ms`, with known non-failing libjpeg stderr
  warnings; source/DLL export counts remained `358` / `358`. Previous
  `FMT-PNG-004AO` red evidence:
  raw targeted coverage passed immediately on the existing generalized native
  route, proving no native code was missing; facade targeted coverage first
  errored at the existing
  `Pillow.Image.Save pnginfo multiple custom chunks with other metadata or options is not supported`
  guard, then at the
  `Pillow.Image.Save pnginfo language and translated keyword with metadata is not supported`
  guard. Green evidence: targeted raw passed `1/1` in `265ms`; targeted facade
  passed `1/1` in `63ms`; raw PNG save regression passed `72/72` in `5093ms`;
  facade PNG save regression passed `75/75` in `719ms`; the latest full AHK
  directory invocation with `-TimeoutSeconds 120` passed `1237/1237` in
  `6563ms` with the known non-failing libjpeg stderr warnings; source/DLL
  export counts are `358` / `358`. Previous `FMT-PNG-004AN` red evidence:
  raw targeted coverage passed immediately on the existing generalized native
  route, proving no native code was missing; facade targeted coverage errored
  at the existing
  `Pillow.Image.Save pnginfo multiple custom chunks with other metadata or options is not supported`
  guard. Green evidence: targeted raw passed `1/1` in `62ms`; targeted facade
  passed `1/1` in `15ms`; raw PNG save regression passed `71/71` in `2937ms`;
  facade PNG save regression passed `74/74` in `375ms`; the full AHK
  directory invocation with `-TimeoutSeconds 120` passed `1235/1235` in
  `3672ms` with the known non-failing libjpeg stderr warnings; source/DLL
  export counts are `358` / `358`. Previous `FMT-PNG-004AM` red evidence:
  raw targeted coverage failed on the expected `preB < eXIf` chunk-order
  assertion, and facade targeted coverage errored at the existing
  `Pillow.Image.Save pnginfo multiple custom chunks with other metadata or options is not supported`
  guard. Green evidence: Release x64 rebuild succeeded after native code
  changed; targeted raw passed `1/1` in `47ms`; targeted facade passed `1/1`
  in `31ms`; raw PNG save regression passed `70/70` in `3140ms`; facade PNG
  save regression passed `73/73` in `375ms`; the latest full AHK directory
  invocation with `-TimeoutSeconds 120` passed `1233/1233` in `3469ms` with
  the known non-failing libjpeg stderr warnings; source/DLL export counts were
  `358` / `358`. Previous `FMT-PNG-004AL` evidence remains: targeted raw
  passed `1/1` in `78ms`; targeted facade passed `1/1` in `47ms`; raw PNG
  save regression passed `69/69` in `2890ms`; facade PNG save regression
  passed `72/72` in `329ms`; full AHK directory verification passed
  `1231/1231` in `3829ms`. Previous `FMT-PNG-004AK` evidence remains: targeted raw
  passed `1/1` in `125ms`; targeted facade passed `1/1` in `31ms`; raw PNG
  save regression passed `68/68` in `2719ms`; facade PNG regression passed
  `94/94` in `453ms`; full AHK directory verification passed `1229/1229` in
  `3469ms`. Previous `FMT-PNG-004AJ` evidence remains: targeted
  raw/facade passed `1/1` in `47ms` and `1/1` in `63ms`; raw PNG save
  regression passed `67/67` in `2500ms`; facade PNG regression passed `93/93`
  in `390ms`; full AHK directory verification passed `1227/1227` in `3515ms`.
  Previous `META-001K` evidence remains: targeted raw/facade
  passed `1/1` in `31ms` and `1/1` in `47ms`; facade `getexif` regression
  passed `11/11` in `94ms`; raw EXIF regression passed `10/10` in `188ms`;
  full AHK directory verification passed `1225/1225` in `3609ms`. Previous
  `META-001J` evidence remains: targeted raw/facade
  passed `1/1` in `63ms` and `1/1` in `31ms`; full AHK directory verification
  passed `1223/1223` in `3485ms`. Previous `META-001I` evidence remains: targeted raw/facade
  passed `1/1` in `47ms` and `1/1` in `32ms`; full AHK directory verification
  passed `1221/1221` in `3375ms`. Previous `META-001H` evidence remains: targeted raw/facade
  passed `1/1` in `94ms` and `1/1` in `46ms`; full AHK directory verification
  passed `1219/1219` in `8016ms`. Previous `META-001G` evidence remains: targeted raw/facade
  passed `1/1` in `93ms` and `1/1` in `31ms`; full AHK directory verification
  passed `1217/1217` in `3843ms`. Previous `META-001F` evidence remains: targeted raw/facade
  passed `1/1` in `47ms` and `1/1` in `16ms`; full AHK directory verification
  passed `1215/1215` in `3421ms`. Previous `META-001E` evidence remains: targeted raw/facade
  passed `1/1` in `93ms` and `1/1` in `31ms`; full AHK directory verification
  passed `1213/1213` in `8203ms`. Previous `META-001D` evidence remains: targeted raw/facade
  passed `1/1` in `63ms` and `1/1` in `31ms`; full AHK directory verification
  passed `1211/1211` in `6594ms`. Previous `META-001C` evidence remains:
  targeted raw/facade passed `1/1` in `63ms` and `1/1` in `31ms`; full AHK
  directory verification passed `1209/1209` in `6500ms`. Previous
  `FMT-PNG-001AB` evidence remains: targeted raw passed
  `1/1` in `47ms`, targeted facade passed `1/1` in `31ms`, raw PNG open
  metadata regression passed `11/11` in `250ms`, and facade PNG open metadata
  regression passed `9/9` in `47ms`. Previous `FMT-GIF-002F`
  evidence remains: targeted raw passed `1/1` in `62ms`, targeted facade
  passed `1/1` in `16ms`, raw `open_gif_frame` coverage passed `4/4` in
  `141ms`, raw GIF animation regression passed `30/30` in `1156ms`, facade GIF
  `save_all` regression passed `30/30` in `141ms`, and facade Open GIF
  regression passed `7/7` in `63ms`. Previous `FMT-GIF-004S` evidence remains: raw targeted passed
  `1/1` in `47ms`, facade targeted passed `1/1` in `31ms`, raw GIF animation
  regression passed `30/30` in `1140ms`, and facade GIF `save_all` regression
  passed `30/30` in `141ms`. Previous `FMT-PNG-004AI` evidence
  remains: targeted raw passed `1/1` in `31ms` and targeted facade passed
  `1/1` in `32ms`. Previous `API-IMG-001B` evidence remains: targeted facade
  passed `1/1` in `16ms`.
  Previous `API-IMG-001A`
  `format_description` evidence remains: targeted facade passed `1/1` in
  `31ms`. Previous `FMT-GIF-004R` evidence remains:
  targeted raw passed `1/1` in `31ms`, targeted facade passed `1/1` in `16ms`,
  raw GIF animation regression passed `29/29` in `1141ms`, facade GIF
  `save_all` regression passed `29/29` in `141ms`. Earlier DLL-residency evidence remains:
  the targeted facade pin test passed `1/1` in `0ms`, the complete facade file
  passed `597/597` in `1968ms`, and the complete raw DLL file passed `600/600`
  in `20719ms`.
- Facade tests: `650` in `ahk\pillow.test.ahk`.
- Raw DLL tests: `649` in `ahk\pillow_c.test.ahk`.
- DLL export table for `build\x64\Release\pillow_c.dll`: `377`
  `pillow_c_*` exports.
- Native source export declarations in `src\pillow_c.cpp`: `377`.
- Core implementation size: `src\pillow_c.cpp` has `42746` lines.
- Facade size: `ahk\pillow.ahk` has `9575` lines.
- Test surface size: `ahk\pillow_c.test.ahk` has `39528` lines and
  `ahk\pillow.test.ahk` has `29707` lines.
- Direct Pillow comparison route: read
  `docs\pillow-direct-diff-assessment.md` when choosing work from a broad
  compatibility or roadmap question.
- Direct Pillow recheck on 2026-06-20: local Pillow `11.3.0` reports `45`
  open-capable format IDs, `30` save-capable IDs, `7` save-all IDs, and `48`
  IDs in the open/save/save_all union. `PIL.Image.Image` exposes `59`
  non-private names, including `50` callable methods and `7` properties; a
  strict name-normalized direct-member parse of the current facade now finds
  about `54` direct name/property analogues for those `59` names, or about
  `55` when the AHK-style `Format` property is counted as the `format`
  analogue.
  The remaining object gaps are concentrated in low-level `im`/`getim`
  semantics, Qt/show integration, and related Python object-model surfaces;
  broader XMP schemas remain under `META-002`. Local optional Pillow
  features include WebP, AVIF, JPEG2000, libtiff, FreeType, littlecms2, RAQM,
  FriBiDi, HarfBuzz, WebP animation/mux/transparency, libjpeg-turbo, and
  zlib-ng, none of which should be treated as "free" parity in the current
  DLL. Current test-tree correction: `MODE-NUM-001B` is now covered by raw
  and facade tests; the full registered-format and module matrix is in
  `docs\pillow-direct-diff-assessment.md`.
- Direct re-audit script re-run on 2026-06-20 confirmed the same Pillow
  registry counts, `337` source `pillow_c_*` symbols, `337` Release x64 DLL
  exported names via `dumpbin /exports`, and then `MODE-NUM-001C` raised the
  current test tree to `1055` AHK tests split as `531` facade / `524` raw.
  The covered mode child was reprobed directly against Pillow 11.3.0:
  `I -> L` truncates toward zero then clamps to `0..255`; `F -> L` does the
  same, with `NaN -> 0`, `-Inf -> 0`, and `+Inf -> 255`; empty `(0, 1)`
  `I`/`F` images convert to empty mode `L`.
- `META-001B` then raised the current test tree to `1057` AHK tests split as
  `532` facade / `525` raw. Full AHK directory verification passed
  `1057/1057` in `70656ms` with the same four non-failing libjpeg stderr
  warnings.
- `FMT-TIFF-002A` then raised the current test tree to `1059` AHK tests split
  as `533` facade / `526` raw. Full AHK directory verification passed
  `1059/1059` in `72266ms` with the same four non-failing libjpeg stderr
  warnings.
- `FMT-TIFF-002B` then raised the current test tree to `1061` AHK tests split
  as `534` facade / `527` raw. Full AHK directory verification passed
  `1061/1061` in `69891ms` with the same four non-failing libjpeg stderr
  warnings.
- `FMT-TIFF-002C` then raised the current test tree to `1063` AHK tests split
  as `535` facade / `528` raw. Full AHK directory verification passed
  `1063/1063` in `72406ms` with the same four non-failing libjpeg stderr
  warnings.
- `FMT-TIFF-002D` then raised the current test tree to `1065` AHK tests split
  as `536` facade / `529` raw. Full AHK directory verification passed
  `1065/1065` in `71547ms` with the same four non-failing libjpeg stderr
  warnings.
- `FMT-TIFF-002E` then raised the current test tree to `1067` AHK tests split
  as `537` facade / `530` raw. Full AHK directory verification passed
  `1067/1067` in `70782ms` with the same four non-failing libjpeg stderr
  warnings.
- `FMT-TIFF-003A` then raised the current test tree to `1069` AHK tests split
  as `538` facade / `531` raw. Full AHK directory verification passed
  `1069/1069` in `71297ms` with the same four non-failing libjpeg stderr
  warnings.
- `FMT-TIFF-003B` then raised the current test tree to `1071` AHK tests split
  as `539` facade / `532` raw. Targeted raw and facade TIFF palette tests
  passed `1/1` each; combined TIFF regression passed `25/25` in `3141ms`.
  The full AHK directory single invocation timed out twice at the required
  `-TimeoutSeconds 120` budget, but the complete facade file passed `539/539`
  in `97953ms` and the complete raw DLL file passed `532/532` in `35188ms`.
- `FMT-TIFF-002F` then raised the current test tree to `1073` AHK tests split
  as `540` facade / `533` raw. Targeted raw and facade TIFF LZW tests passed
  `1/1` each; combined TIFF regression passed `27/27` in `2078ms`; full AHK
  directory verification passed `1073/1073` in `103375ms` with the same four
  non-failing libjpeg stderr warnings.
- `FMT-TIFF-002G` then raised the current test tree to `1075` AHK tests split
  as `541` facade / `534` raw. Targeted raw and facade TIFF Deflate tests
  passed `1/1` each; combined TIFF regression passed `29/29` in `1985ms`;
  full AHK directory verification passed `1075/1075` in `80406ms` with the
  same four non-failing libjpeg stderr warnings.
- `FMT-TIFF-003C` then raised the current test tree to `1077` AHK tests split
  as `542` facade / `535` raw. Targeted raw and facade TIFF `LA` tests passed
  `1/1` each; combined TIFF regression passed `31/31` in `3156ms`; full AHK
  directory verification passed `1077/1077` in `102515ms` with the same four
  non-failing libjpeg stderr warnings.
- `FMT-TIFF-003D` then raised the current test tree to `1079` AHK tests split
  as `543` facade / `536` raw. Targeted raw and facade TIFF `CMYK` tests
  passed `1/1` each; combined TIFF regression passed `33/33` in `2359ms`;
  full AHK directory verification passed `1079/1079` in `100047ms` with the
  same four non-failing libjpeg stderr warnings.
- `FMT-TIFF-003E` then raised the current test tree to `1081` AHK tests split
  as `544` facade / `537` raw. Targeted raw and facade TIFF `CMYK`
  compression tests passed `1/1` each; combined TIFF regression passed
  `35/35` in `4016ms`. The full AHK directory single invocation timed out
  twice at the required `-TimeoutSeconds 120` budget, but the complete facade
  file passed `544/544` in `94250ms` and the complete raw DLL file passed
  `537/537` in `34125ms`, with the same non-failing libjpeg stderr warnings.
- `FMT-TIFF-003F` then raised the current test tree to `1083` AHK tests split
  as `545` facade / `538` raw. Targeted raw and facade TIFF numeric `I`/`F`
  tests passed `1/1` each; combined TIFF regression passed `37/37` in
  `4250ms`. The full AHK directory single invocation timed out at the
  required `-TimeoutSeconds 120` budget, but the complete facade file passed
  `545/545` in `96234ms` and the complete raw DLL file passed `538/538` in
  `36313ms`, with the same non-failing libjpeg stderr warnings.
- `FMT-TIFF-003G` then raised the current test tree to `1087` AHK tests split
  as `547` facade / `540` raw. Targeted raw and facade TIFF numeric
  compression tests passed `1/1` each; targeted raw and facade Pillow-generated
  Deflate `F` fixture tests passed `1/1` each; combined TIFF regression passed
  `41/41` in `3266ms`; full AHK directory verification passed `1087/1087`
  in `76000ms` with the same non-failing libjpeg stderr warnings.
- `FMT-TIFF-003H` then raised the current test tree to `1091` AHK tests split
  as `549` facade / `542` raw. Targeted raw and facade TIFF `I;16` tests
  passed `1/1` each; the combined TIFF regression passed `45/45` in
  `3657ms`. Full AHK directory verification passed `1091/1091` in `78907ms`
  with the same four non-failing libjpeg stderr warnings.
- `FMT-TIFF-003I` then raised the current test tree to `1093` AHK tests split
  as `550` facade / `543` raw. The facade `I16 compression` filter passed
  `1/1`; the raw `I16 mode` filter passed `2/2`; the combined TIFF regression
  passed `47/47` in `4500ms`. The full AHK directory single invocation timed
  out at the required `-TimeoutSeconds 120` budget, but complete facade and raw
  file runs passed `550/550` in `97859ms` and `543/543` in `36031ms`, with the
  same non-failing libjpeg stderr warnings.
- `FMT-TIFF-003J` then raised the current test tree to `1095` AHK tests split
  as `551` facade / `544` raw. The combined `I16B mode` filter passed `2/2`
  in `329ms`; raw mode regression passed `122/122` in `8360ms`; combined TIFF
  regression passed `49/49` in `6844ms`. The full AHK directory single
  invocation timed out at the required `-TimeoutSeconds 120` budget, but
  complete facade and raw file runs passed `551/551` in `97688ms` and
  `544/544` in `37313ms`, with the same non-failing libjpeg stderr warnings.
- `FMT-TIFF-003K` then raised the current test tree to `1097` AHK tests split
  as `552` facade / `545` raw. The combined `I16B` filter passed `4/4` in
  `781ms`; combined TIFF regression passed `51/51` in `7140ms`. The full AHK
  directory single invocation timed out at the required `-TimeoutSeconds 120`
  budget, but complete facade and raw file runs passed `552/552` in `94765ms`
  and `545/545` in `35656ms`, with the same non-failing libjpeg stderr
  warnings.

## 2026-06-20 Full Direct Pillow Difference Audit Refresh

This recheck was requested explicitly to find a faster route by comparing
directly against local Pillow `11.3.0`, not by walking the existing gap list.
It intentionally audits broad Pillow deltas once, then turns the result back
into bounded gap IDs for implementation.

Verified Pillow inventory:

| Pillow surface | Local Pillow `11.3.0` | Current project interpretation |
| --- | ---: | --- |
| open/save/save_all format ID union | `48` | Native paths cover about `10` practical families; plugin breadth is still the largest full-parity gap. |
| Open-capable format IDs | `45` | Core native open coverage is useful, but WebP/AVIF/JPEG2000/CUR/long-tail plugins are absent. |
| Save-capable format IDs | `30` | PNG/JPEG/GIF have real option depth; many save plugins and TIFF tag/compression behavior remain open. |
| Save-all format IDs | `7` | GIF animation is deep; bounded TIFF multipage save is now covered; APNG/WebP/MPO/PDF save-all are not current native surfaces. |
| `PIL.Image.Image` non-private names | `59` | Hot method names mostly exist. `getexif()` is bounded to JPEG/PNG orientation lifecycle, TIFF Orientation=1 read metadata, and TIFF common ASCII IFD0 tag readback through `META-001L` plus TIFF `ImageDescription` tag `270` through `META-001M`; `getxmp()` is bounded to PNG/JPEG XMP open metadata in `META-002A`, explicit JPEG `xmp=` save round-trips through `META-002B`, explicit JPEG `qtables + xmp` saves through `META-002C`, explicit JPEG `keep_rgb + xmp` saves through `META-002D`, and explicit JPEG `qtables + keep_rgb + xmp` saves through `META-002E`; `frombuffer` now has bounded raw `L`, raw `L`/`RGBA` mapmode, public `RGBX` mapmode, direct `RGB` copy, direct `RGBA` stride/orientation coverage, readonly ImageDraw/Paste detach, readonly Resize/Thumbnail refresh, readonly raw byte-read refresh, readonly histogram refresh, readonly getbbox refresh, readonly getprojection refresh, readonly getcolors refresh, readonly entropy refresh, readonly getextrema refresh, readonly convert-source refresh, readonly getpixel refresh, readonly crop refresh, readonly full-image copy refresh, readonly getchannel/split band extraction refresh, readonly merge-bands refresh, readonly ImageFilter refresh behavior, readonly point/LUT refresh behavior, readonly ImageOps LUT and histogram-transform refresh behavior, readonly ImageChops unary/binary refresh behavior, readonly offset refresh behavior, readonly blend/composite refresh behavior, readonly transpose refresh behavior, and readonly PNG save refresh behavior; `I` and `F` `getextrema()`, `histogram()`, and `convert("L")` numeric semantics are covered; low-level `im/getim` and broader mode breadth remain material. |
| `PIL.Image.Image` names with direct facade analogue | strict `54` / `59`; about `55` / `59` if AHK `Format` is counted for Python `format` | `format_description` is exposed through `FormatDescription` / `format_description`, `has_transparency_data` is exposed through `HasTransparencyData` / `has_transparency_data`, `get_child_images()` is exposed through `GetChildImages()` / `get_child_images()`, `getxmp()` is exposed through `GetXmp()` / `getxmp()` for bounded PNG/JPEG packets, and `getbands()` shares the existing `GetBands()` route including numeric `I`/`F` names. The remaining names are mostly Python object-model or external-integration surfaces: `getim`, `im`, `show`, `toqimage`, and `toqpixmap`. `Format` exists as an AHK-style property but is not full Python object-model parity. |
| Common module name coverage | `ImageOps` and `ImageChops` high by name; `ImageCms` and `ImagePalette` absent | The route should not chase name counts. The important missing modules are font/RAQM, ImageCms, palette factories, and dependency constructors. |
| Current native exports | `379` source declarations / `379` DLL exports | Native surface is broad for the AHK-first path, but export count is not a Pillow parity score. |
| Current AHK tests | `1326` total: `664` facade / `662` raw DLL | Latest `META-001M` TIFF tag `270` raw/facade `getexif()` checks passed; latest full AHK directory invocation passed `1326/1326` in `7578ms` with known non-failing libjpeg stderr warnings. |
| Current implementation size | `43124` native lines / `9793` facade lines / `71405` AHK test lines | Work volume is high; the limiting factor remains selecting slices that collapse multiple gaps and keeping the full test gate inside the 120-second runtime budget. |

Route conclusions:

- The old `40-45%` project estimate is stale for the AHK-first target.
  Move the current AHK-first estimate to `52-55%`; keep full Pillow
  replacement low at `26-31%`.
- The biggest full-Pillow gap is not common RGB/RGBA hot-loop behavior. It is
  plugin breadth, full metadata object lifecycle beyond the covered
  EXIF-orientation child, `frombuffer` and constructor interop, `I`/`F` and
  other mode breadth, ImageCms, FreeType/RAQM, and dependency-gated formats
  such as WebP/AVIF/JPEG2000.
- The direct API comparison does not support dropping the AHK-first estimate
  back to `40-45%`: common `Image.Image`, `ImageOps`, and `ImageChops` names
  are already broad. It also does not justify claiming Pillow parity: the
  remaining gaps are deeper semantic pillars and plugin/module surfaces.
- The speed problem is route selection, not lack of work volume. Continue only
  work packets that buy a reusable semantic pillar, a new native capability, a
  new ABI shape, or a locally proven Pillow boundary. Do not open a branch for
  every observed Pillow option combination.
- For speed, use a ranked direct-diff queue instead of another broad audit:
  `MODE-NUM-001C`, the TIFF Orientation=1 `META-001B` child, TIFF
  Orientation=3 `FMT-TIFF-002A`, TIFF Orientation=6/8 `FMT-TIFF-002B`,
  TIFF Orientation=2/4/5/7 `FMT-TIFF-002C`, TIFF DPI tag
  save/open `FMT-TIFF-002D`, TIFF PackBits save compression
  `FMT-TIFF-002E`, bounded TIFF mode `1` save/open `FMT-TIFF-003A`, bounded
  TIFF palette mode `P` save/open `FMT-TIFF-003B`, TIFF LZW save compression
  `FMT-TIFF-002F`, TIFF Adobe Deflate save compression `FMT-TIFF-002G`,
  bounded TIFF mode `CMYK` save/open `FMT-TIFF-003D`, bounded TIFF mode
  `CMYK` PackBits/LZW/Adobe Deflate save/open `FMT-TIFF-003E`, bounded TIFF
  numeric mode `I`/`F` uncompressed save/open `FMT-TIFF-003F`, bounded
  TIFF numeric mode `I`/`F` PackBits/LZW/Adobe Deflate save/open
  `FMT-TIFF-003G`, bounded little-endian TIFF `I;16` save/open
  `FMT-TIFF-003H`, and bounded little-endian TIFF `I;16` PackBits/LZW/Adobe
  Deflate save/open `FMT-TIFF-003I`, bounded big-endian TIFF `I;16B`
  save/open `FMT-TIFF-003J`, and compressed public `I;16B` normalization to
  little-endian TIFF `I;16` `FMT-TIFF-003K` are now covered. Continue split
  `FMT-TIFF-002` / `FMT-TIFF-003` tag/compression/mode depth only for locally
  proven boundaries, then dependency-gated `FMT-WEBP-001` still-image format
  breadth if broad format coverage is explicitly selected. PNG/JPEG/GIF option
  tails should only continue when the oracle exposes a new semantic boundary
  or the change adds a new native route or ABI shape.
- Roadmap from the direct diff: the reusable numeric conversion pillar is now
  covered, the first TIFF metadata object read child is covered, TIFF
  Orientation values `1` through `8` are covered for the bounded TIFF fixture,
  TIFF DPI Resolution tags are covered for the bounded save/open route, the
  real TIFF save compression children PackBits, LZW, and Adobe Deflate are
  covered for RGB, the bounded CMYK route, and the bounded numeric `I`/`F`
  route, and bounded TIFF mode `1` / `P` / `LA` / `CMYK` / numeric `I` /
  numeric `F` / little-endian `I;16` / big-endian `I;16B` save/open is
  covered, and compressed public `I;16B` save now follows Pillow by
  normalizing to little-endian `I;16`. The `LA`, `I;16`, and normalized
  `I;16B` PackBits/LZW/Adobe Deflate routes are explicitly covered by tests
  through the generalized TIFF compression writer.
  Continue with TIFF compression/tag/mode breadth or another
  metadata object lifecycle child only when the local Pillow boundary is
  proven first.
- The latest completed work packet is `FMT-GIF-004W`, a child of
  `FMT-GIF-004`, for bounded P-mode GIF animation comment plus background,
  include_color_table, optimize, and transparency metadata. Local Pillow
  11.3.0 writes the NETSCAPE extension, then `comment=b"hello"`, then both
  frame GCEs with transparency index `2`, while the logical screen carries
  background index `1` and both frames use local 256-color tables. Native
  `pillow_c_image_save_gif_animation_comment_background_options` keeps the
  combined write path in the DLL, and the facade routes `save_all` `Comment`
  plus `Background`/`IncludeColorTable`/`Optimize`/`Transparency` without AHK
  pixel loops.
- The previous completed work packet is `FMT-GIF-004U`, a child of
  `FMT-GIF-004`, for bounded single-frame GIF comment plus transparency
  metadata. Local Pillow 11.3.0 writes `comment=b"hello"` before the GCE for
  `transparency=1`, reopens with both comment bytes and transparency metadata,
  and converts the transparent palette index to alpha `0`. Native
  `pillow_c_image_save_gif_comment_options` keeps the combined write path in
  the DLL, and the facade routes `Comment` plus `Transparency` without AHK
  pixel loops.
- The previous completed work packet is `FMT-GIF-004T`, a child of
  `FMT-GIF-004`, for bounded GIF comment save/open metadata. Local Pillow
  11.3.0 writes single-frame comments as a GIF comment extension after the
  global color table and writes animation comments after the NETSCAPE loop
  extension; reopened frames expose `info["comment"] == b"hello"`. Native
  `pillow_c_image_gif_comment`, `pillow_c_image_save_gif_comment`, and
  `pillow_c_image_save_gif_animation_comment` keep the bytes in the DLL, and
  the facade exposes reopened comments as `Info["comment"]` buffers.
- The previous completed work packet is `FMT-PNG-004AQ`, a child of
  `FMT-PNG-001`, for compressed language-keyed `PngInfo.add_itxt(..., zip=True,
  lang, tkey)` plus explicit `icc_profile`, explicit `exif`, RGB
  `transparency=(10,20,30)`, `optimize=True`, and multiple Pillow-style
  private chunks before and after `IDAT`. Local Pillow 11.3.0 writes `iCCP`,
  compressed language-keyed `iTXt`, pre-`IDAT` private chunks, `tRNS`, `eXIf`,
  optimized `IDAT`, after-`IDAT` private chunks, and `IEND`, preserves RGB
  bytes, reopens with text, ICC, Pillow-style EXIF metadata and orientation
  `6`, RGB transparency metadata, and hides unknown private chunks. Native
  `pillow_c_image_save_png_metadata_custom_chunks_options` is reused; no new
  export or native code change was required for this slice.
- The prior PNG open packet is `FMT-PNG-001AB`, a child of
  `FMT-PNG-001`, for dynamic-Huffman compressed PNG text metadata on open.
  Local Pillow 11.3.0 exposes a bounded grayscale fixture containing dynamic
  `zTXt` keyword `DynamicNote` and compressed `iTXt` keyword `DynamicComment`
  as ordinary string-compatible `Info` / `Text` values while preserving pixel
  byte `[7]`. Native `inflate_zlib_deflate` now decodes dynamic Huffman
  deflate blocks for the existing PNG text metadata ABI, and `open_png_image`
  removes compressed text ancillary chunks from the WIC decode copy so WIC does
  not hang before native metadata scanning. No ABI symbol was added.
- The previous completed work packet is `FMT-PNG-004AI`, a child of
  `FMT-PNG-001`, for standard `sPLT` suggested-palette public chunk save
  support. Local Pillow 11.3.0 writes bytes-valued `PngInfo.add(b"sPLT", ...)`
  after `IHDR` and before `IDAT`, preserves RGB bytes, and does not expose
  `sPLT` through reopened `Info` or `Text`. Native validation accepts `sPLT`
  on the standalone public chunk route, adds no ABI symbol, and the facade
  routes standalone `PngInfo.add(b"sPLT", ...)` through the DLL.
- The previous completed work packet is `FMT-PNG-004AH`, a child of
  `FMT-PNG-001`, for standard four-byte `cICP` public chunk save support.
  Local Pillow 11.3.0 writes `cICP` after `IHDR` and before `IDAT`, preserves
  RGB bytes, and does not expose `cICP` through reopened `Info` or `Text`.
- The preceding completed work packet is `FMT-PNG-004AG`, a child of
  `FMT-PNG-001`, for standard RGB three-byte `sBIT` public chunk save support.
  Local Pillow 11.3.0 writes `sBIT` after `IHDR` and before `IDAT`, preserves
  RGB bytes, and does not expose `sBIT` through reopened `Info` or `Text`.
- The preceding completed work packet is `FMT-PNG-004AF`, a child of
  `FMT-PNG-001`, for standard valid-length `tIME` public chunk save support.
  Local Pillow 11.3.0 writes `tIME` after `IHDR` and before `IDAT`, preserves
  RGB bytes, and does not expose `tIME` through reopened `Info` or `Text`.
- The previous completed work packet is `FMT-PNG-004AE`, a child of
  `FMT-PNG-001`, for standard P-mode `hIST` public chunk save support. Local
  Pillow 11.3.0 writes `hIST` after `PLTE`, or after `tRNS` when palette
  transparency is present, preserves P-mode bytes, and does not expose `hIST`
  through reopened `Info` or `Text`. Native validation now requires a P-mode
  payload length of `2 * palette_entries`, reuses the existing palette
  deferral point, adds no ABI symbol, and the facade routes standalone
  bytes-valued `PngInfo.add(b"hIST", ...)` through the DLL.
- The earlier completed work packet is `FMT-JPEG-003AM`, a child of
  `FMT-JPEG-003`, for no-qtables CMYK `keep_rgb=True` optimized progressive
  aliases. Local Pillow 11.3.0 accepts `quality=95, keep_rgb=True,
  progressive=True, optimize=True` on bounded CMYK saves, with optional
  `dpi=(300,150)` and optional explicit comment/ICC/EXIF metadata. The marker
  contract is APP14 before DQT, optional APP0/JFIF before APP14 for DPI saves,
  optional APP1 EXIF, APP2 ICC, and COM after APP14 and before DQT for metadata
  saves, one DQT, SOF2 `1x1` CMYK components, and 18 progressive SOS scans.
  The reopened image is mode `CMYK` with the expected bytes under the existing
  progressive tolerance, plus DPI/JFIF and metadata where requested. Native
  exports already supported the route; the facade change removes the stale
  progressive-plus-optimize CMYK keep-rgb guard and routes through
  `pillow_c_image_save_jpeg_encode_keep_rgb_options` or
  `pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options` without AHK pixel
  loops. No native code changed and no rebuild was needed.
- The earlier TIFF completed work packet is `FMT-TIFF-003K`, a child of
  `FMT-TIFF-003`, for compressed public `I;16B` TIFF save/open normalization.
  Local Pillow 11.3.0 shows `Image.frombytes("I;16B", (3,1),
  bytes([0,1,0,255,1,0]))` with `compression="packbits"`, `"tiff_lzw"`,
  `"tiff_adobe_deflate"`, or `"tiff_deflate"` saves as a little-endian `II`
  TIFF, reopens as mode `I;16`, and exposes byte-swapped sample storage
  `[1,0,255,0,0,1]`. The IFD0 shape keeps BitsPerSample `16`,
  PhotometricInterpretation `1`, RowsPerStrip `1`, PlanarConfiguration `1`,
  no SamplesPerPixel tag, no SampleFormat tag, and Compression `32773`, `5`,
  or `8`. Native TIFF save now byte-swaps public `I;16B` storage into a
  temporary `I;16` view and reuses the PackBits/LZW/Adobe Deflate strip
  encoders, so frame-0 reopen returns mode `I;16` with Pillow-compatible
  direct bytes. `I;16B` DPI/multipage behavior, `I;16N`, predictor variants,
  non-frame-0 high-bit parsing, high-bit conversions, filters, arithmetic,
  BigTIFF, and exact libtiff whole-file layout bytes remain deferred.
- The previous completed work packet is `FMT-TIFF-003J`, a child of
  `FMT-TIFF-003`, for bounded big-endian TIFF `I;16B` save/open. Local Pillow
  11.3.0 shows `Image.frombytes("I;16B", (3,1),
  bytes([0,1,0,255,1,0]))` saves as a big-endian `MM` TIFF and reopens as
  mode `I;16B` with preserved bytes. The IFD0 shape keeps BitsPerSample `16`,
  Compression `1`, PhotometricInterpretation `1`, RowsPerStrip `1`,
  PlanarConfiguration `1`, no SamplesPerPixel tag, and no SampleFormat tag.
  Native mode id `12` now maps to public mode name `I;16B`, stores direct
  two-byte big-endian samples, saves that bounded IFD shape through the
  DLL-owned TIFF writer, and parses big-endian 16-bit grayscale frame 0 before
  WIC can collapse the high-bit storage. Compressed public `I;16B` save/open
  normalization is covered later by `FMT-TIFF-003K`; `I;16B` DPI/multipage
  behavior, `I;16N`, predictor variants, non-frame-0 high-bit parsing,
  high-bit conversions, filters, arithmetic, BigTIFF, and exact libtiff
  whole-file layout bytes remain deferred.
- The previous completed work packet is `FMT-TIFF-003I`, a child of
  `FMT-TIFF-003`, for bounded little-endian TIFF `I;16` PackBits/LZW/Adobe
  Deflate save/open. Local Pillow 11.3.0 shows `Image.frombytes("I;16",
  (3,1), bytes([1,0,255,0,0,1]))` reopens as mode `I;16` with preserved bytes
  under `compression="packbits"`, `"tiff_lzw"`, `"tiff_adobe_deflate"`, and
  `"tiff_deflate"`. The IFD0 shape keeps BitsPerSample `16`,
  PhotometricInterpretation `1`, RowsPerStrip `1`, PlanarConfiguration `1`,
  no SamplesPerPixel tag, and no SampleFormat tag, while Compression is
  `32773`, `5`, or `8`. Native TIFF save now encodes the two-byte sample strip
  through the existing compression writers, and native TIFF frame-0 open
  decodes PackBits, TIFF LZW, and bounded zlib Deflate before allocating mode
  `I;16`. `I;16B` uncompressed save/open is covered later by
  `FMT-TIFF-003J`; compressed public `I;16B` save normalization is covered
  later by `FMT-TIFF-003K`; `I;16B` DPI/multipage behavior, `I;16N`,
  predictor variants, non-frame-0 high-bit parsing, high-bit conversions,
  filters, arithmetic, BigTIFF, and exact libtiff whole-file layout bytes
  remain deferred.
- The previous completed work packet is `FMT-TIFF-003H`, a child of
  `FMT-TIFF-003`, for bounded little-endian TIFF `I;16` save/open. Local
  Pillow 11.3.0 shows `Image.frombytes("I;16", (3,1),
  bytes([1,0,255,0,0,1]))` saves as a little-endian TIFF with BitsPerSample
  `16`, Compression `1`, PhotometricInterpretation `1`, RowsPerStrip `1`,
  PlanarConfiguration `1`, no SamplesPerPixel tag, no SampleFormat tag, and a
  six-byte strip that reopens as mode `I;16` with preserved bytes. Native mode
  id `11` now maps to public mode name `I;16`, stores direct two-byte
  little-endian samples, saves that bounded IFD shape through the DLL-owned
  TIFF writer, and parses frame 0 before WIC can collapse the high-bit storage.
  `I;16B` / `I;16N`, high-bit conversions, filters, arithmetic, non-frame-0
  high-bit parsing, BigTIFF, and exact libtiff whole-file layout bytes remain
  deferred.
- The previous completed work packet is `FMT-TIFF-003G`, a child of
  `FMT-TIFF-003`, for bounded TIFF numeric modes `I` and `F` compressed
  save/open. Local Pillow 11.3.0 shows the same `3x1` mode `I` bytes
  `[255,255,255,255,0,0,0,0,112,17,1,0]` and mode `F` bytes
  `[0,0,192,191,0,0,0,0,0,0,16,64]` reopen with preserved bytes under
  `compression="packbits"`, `"tiff_lzw"`, `"tiff_adobe_deflate"`, and
  `"tiff_deflate"`. The IFD0 shape keeps BitsPerSample `32`,
  PhotometricInterpretation `1`, RowsPerStrip `1`, PlanarConfiguration `1`,
  SampleFormat `2` or `3`, and no SamplesPerPixel tag, while Compression is
  `32773`, `5`, or `8`. Native TIFF save now encodes the numeric strip through
  the existing compression writers, and native TIFF frame-0 open decodes
  PackBits, TIFF LZW, and bounded zlib Deflate strips before allocating mode
  `I` or `F` handles. Non-frame-0 numeric special parsing, arbitrary tag
  dictionaries, JPEG-in-TIFF, predictor variants, BigTIFF, and exact libtiff
  whole-file layout bytes remain deferred.
- The previous completed work packet is `FMT-TIFF-003F`, a child of
  `FMT-TIFF-003`, for bounded TIFF numeric modes `I` and `F` uncompressed
  save/open. Local Pillow 11.3.0 shows `3x1` mode `I` bytes
  `[255,255,255,255,0,0,0,0,112,17,1,0]` and mode `F` bytes
  `[0,0,192,191,0,0,0,0,0,0,16,64]` reopen with preserved bytes. The IFD0
  shape uses BitsPerSample `32`, Compression `1`,
  PhotometricInterpretation `1`, StripByteCounts `12`, RowsPerStrip `1`,
  PlanarConfiguration `1`, SampleFormat `2` or `3`, and no
  SamplesPerPixel tag. Native TIFF save writes that one-logical-sample
  layout through the DLL-owned IFD writer, and native TIFF frame-0 open parses
  the bounded numeric IFD directly before WIC pixel-format routing.
- The previous completed work packet before that is `FMT-TIFF-003E`, a child of
  `FMT-TIFF-003`, for bounded TIFF mode `CMYK` save/open with the existing
  PackBits, LZW, and Adobe Deflate compression export. Local Pillow 11.3.0
  shows the `2x1` mode `CMYK` fixture reopens as mode `CMYK` with preserved
  bytes under `compression="packbits"`, `"tiff_lzw"`,
  `"tiff_adobe_deflate"`, and `"tiff_deflate"`; the IFD0 Compression tag is
  `32773`, `5`, or `8`, while BitsPerSample `[8,8,8,8]`,
  PhotometricInterpretation `5`, SamplesPerPixel `4`, RowsPerStrip `1`, and
  PlanarConfiguration `1` remain the same as the uncompressed CMYK route.
  Native TIFF save now lets the existing compression writer encode four-byte
  CMYK rows, and the facade routes the same public `Image.Save(..., "TIFF",
  {compression: ...})` strings without AHK pixel loops. High-bit plugin modes,
  arbitrary tag dictionaries, JPEG-in-TIFF, predictor variants,
  non-inch resolution units, BigTIFF, and non-frame-0 special tag parsing
  remain deferred.
- The previous completed work packet is `FMT-TIFF-003D`, a child of
  `FMT-TIFF-003`, for bounded TIFF mode `CMYK` save/open. Local Pillow 11.3.0
  shows a `2x1` mode `CMYK` TIFF reopens as mode `CMYK`, preserves bytes
  `[1,2,3,4,10,20,30,40]`, writes IFD0 BitsPerSample `[8,8,8,8]`,
  PhotometricInterpretation `5`, SamplesPerPixel `4`, PlanarConfiguration `1`,
  and a raw eight-byte strip. Native TIFF save writes that bounded CMYK layout
  through the DLL-owned IFD writer, and native TIFF frame-0 open accepts WIC
  32bpp CMYK decode as public `CMYK` bytes inside the handle. The facade routes
  `Image.Save(..., "TIFF")` and `Image.Open(..., ["TIFF"])` without AHK pixel
  loops. High-bit `I`/`F`, arbitrary tag dictionaries, JPEG-in-TIFF,
  non-inch resolution units, BigTIFF, and non-frame-0 special tag parsing
  remain deferred.
- The previous completed work packet is `FMT-TIFF-003C`, a child of
  `FMT-TIFF-003`, for bounded TIFF mode `LA` save/open. Local Pillow 11.3.0
  shows a `2x1` mode `LA` TIFF reopens as mode `LA`, preserves bytes
  `[10,255,40,128]`, writes IFD0 BitsPerSample `[8,8]`,
  PhotometricInterpretation `1`, SamplesPerPixel `2`, PlanarConfiguration `1`,
  ExtraSamples `2`, and a raw four-byte strip. Native TIFF save writes that
  bounded grayscale-alpha layout through the DLL-owned IFD writer, and native
  TIFF frame-0 open recognizes that IFD0 shape, asks WIC for RGBA decode, and
  stores public `LA` bytes inside the handle. The facade routes
  `Image.Save(..., "TIFF")` and `Image.Open(..., ["TIFF"])` without AHK pixel
  loops. The existing generalized TIFF compression writer now also has raw and
  facade tests for `LA` PackBits/LZW/Adobe Deflate round-trip with the same
  Pillow-style tags. High-bit modes, non-frame-0 `LA` metadata, BigTIFF, and
  arbitrary tag dictionaries remain deferred.
- The previous completed work packet is `FMT-TIFF-002G`, a child of
  `FMT-TIFF-002`, for TIFF save-side Adobe Deflate compression. Local Pillow
  11.3.0 shows `Image.save(..., format="TIFF",
  compression="tiff_adobe_deflate")` and `compression="tiff_deflate"` write
  IFD0 Compression tag `259` as SHORT value `8`, store a zlib strip beginning
  `[0x78,0x9C]`, and reopen a bounded RGB fixture with preserved pixels; the
  bare string `compression="deflate"` writes uncompressed tag `1` and is not
  treated as a Deflate alias. Native
  `pillow_c_image_save_tiff_compression_options` now accepts compression code
  `8`, writes tag `8`, and zlib-stores the prepared strip inside the DLL-owned
  little-endian TIFF writer; the facade routes `Image.Save(..., "TIFF",
  {compression:"tiff_adobe_deflate"})` and `{compression:"tiff_deflate"}`
  without AHK pixel loops. JPEG-in-TIFF compression, predictor variants,
  `tiffinfo`, high-bit/mode depth, and full TIFF tag dictionaries remain
  deferred.
- The previous completed work packet is `FMT-TIFF-002F`, a child of
  `FMT-TIFF-002`, for TIFF save-side LZW compression. Local Pillow 11.3.0
  shows `Image.save(..., format="TIFF", compression="tiff_lzw")` writes IFD0
  Compression tag `259` as SHORT value `5`, stores a single LZW strip, and
  reopens a bounded RGB fixture with preserved pixels. Native
  `pillow_c_image_save_tiff_compression_options` now accepts compression code
  `5`, writes tag `5`, and encodes the strip with TIFF clear/EOI codes and
  MSB-first 9-to-12-bit LZW codes inside the DLL-owned little-endian TIFF
  writer; the facade routes `Image.Save(..., "TIFF",
  {compression:"tiff_lzw"})` and `{compression:"lzw"}` without AHK pixel
  loops. JPEG-in-TIFF compression, predictor variants, `tiffinfo`,
  high-bit/mode depth, and full TIFF tag dictionaries remain deferred.
- The previous completed work packet is `FMT-TIFF-003B`, a child of
  `FMT-TIFF-003`, for bounded TIFF palette mode `P` save/open. Local Pillow
  11.3.0 shows a `3x2` mode `P` TIFF reopens as mode `P`, preserves index
  bytes `[0,1,2,1,0,2]`, writes PhotometricInterpretation `3`, omits
  `SamplesPerPixel`, writes PlanarConfiguration `1`, writes ColorMap tag `320`
  as `768` SHORT values in red/green/blue planes, and stores palette bytes as
  `byte * 256`. Native TIFF save writes that bounded palette layout through
  the DLL-owned IFD writer, and native TIFF frame-0 open parses ColorMap from
  the original file bytes to avoid WIC palette quantization. The facade routes
  `Image.Save(..., "TIFF")` and `Image.Open(..., ["TIFF"])` without AHK pixel
  loops. High-bit palette variants, non-frame-0 palette ColorMap parsing,
  `I`/`F`, additional compression families, BigTIFF, and arbitrary tag
  dictionaries remain deferred.
- The previous completed work packet is `FMT-TIFF-003A`, a child of
  `FMT-TIFF-003`, for bounded TIFF mode `1` save/open. Local Pillow 11.3.0
  shows an `8x2` mode `1` TIFF reopens as mode `1`, preserves packed
  `tobytes()` bytes `[0x5A, 0xC5]`, stores strip bytes `[0x5A, 0xC5]`, omits
  `BitsPerSample` and `SamplesPerPixel`, and uses single-strip uncompressed
  photometric-minisblack tags. Native TIFF save writes that bounded layout
  through the DLL-owned IFD writer, and native TIFF open accepts the decoded
  1bpp WIC pixel format as internal mode `1` pixels. The facade routes
  `Image.Save(..., "TIFF")` and `Image.Open(..., ["TIFF"])` without AHK pixel
  loops. High-bit modes, `I`/`F`, additional compression families, BigTIFF,
  and arbitrary tag dictionaries remain deferred.
- The previous completed work packet is `FMT-TIFF-002E`, a child of
  `FMT-TIFF-002`, for TIFF save-side PackBits compression. Local Pillow
  11.3.0 shows `Image.save(..., format="TIFF", compression="packbits")`
  writes IFD0 Compression tag `259` as SHORT value `32773`, uses PackBits
  strip bytes, and reopens with preserved RGB pixels. Native
  `pillow_c_image_save_tiff_compression_options` now writes tag `32773` and
  PackBits-encodes scanlines inside the DLL-owned little-endian TIFF writer;
  the facade routes `Image.Save(..., "TIFF", {compression:"packbits"})`
  without AHK pixel loops. LZW and Adobe Deflate are covered by later TIFF
  compression children; JPEG-in-TIFF compression, `tiffinfo`, non-inch
  resolution units, high-bit/mode depth, and full TIFF tag dictionaries remain
  deferred.
- The previous completed work packet is `FMT-TIFF-002D`, a child of
  `FMT-TIFF-002`, for TIFF save/open DPI Resolution tags. Local Pillow
  11.3.0 shows `Image.save(..., format="TIFF", dpi=(300,150))` writes IFD0
  `XResolution=300/1`, `YResolution=150/1`, and `ResolutionUnit=2`, reopens
  with preserved RGB bytes, and exposes `Image.info["dpi"] == (300.0,150.0)`.
  Native `pillow_c_image_save_tiff_options` now writes those tags for positive
  dpi pairs through the DLL-owned little-endian TIFF writer, native TIFF open
  parses them into `pillow_c_image_metadata_resolution`, and the facade routes
  `Image.Save(..., "TIFF", {dpi:[300,150]})` without AHK pixel loops.
  `tiffinfo`, non-inch resolution units, high-bit/mode depth, and full TIFF
  tag dictionaries remain deferred.
- The previous completed work packet is `FMT-TIFF-002C`, a child of
  `FMT-TIFF-002`, for TIFF IFD0 Orientation=2/4/5/7 open semantics. Local
  Pillow 11.3.0 shows the same bounded hand-written `2x3` mode `L` TIFF with
  Orientation tag `2` reopens as size `(2, 3)` with pixels
  `[20, 10, 40, 30, 60, 50]`, Orientation tag `4` reopens as size `(2, 3)`
  with pixels `[50, 60, 30, 40, 10, 20]`, Orientation tag `5` reopens as
  size `(3, 2)` with pixels `[10, 30, 50, 20, 40, 60]`, and Orientation tag
  `7` reopens as size `(3, 2)` with pixels `[60, 40, 20, 50, 30, 10]`;
  none exposes `getexif()[274]`. Native TIFF open now applies these
  mirror/transposed transforms inside the DLL after successful WIC decode and
  does not set the EXIF-orientation metadata slot. Orientation values `1`,
  `3`, `6`, and `8` remain covered by earlier children; broader tag
  dictionaries, compression, and high-bit/mode TIFF coverage remain deferred.
- The previous completed work packet is `FMT-TIFF-002B`, a child of
  `FMT-TIFF-002`, for TIFF IFD0 Orientation=6/8 open semantics. Local Pillow
  11.3.0 shows the same bounded hand-written `2x3` mode `L` TIFF with
  Orientation tag `6` reopens as size `(3, 2)` with pixels
  `[50, 30, 10, 60, 40, 20]`, while Orientation tag `8` reopens as size
  `(3, 2)` with pixels `[20, 40, 60, 10, 30, 50]`; neither exposes
  `getexif()[274]`. Native TIFF open now applies these dimension-swapping
  rotations inside the DLL after successful WIC decode and does not set the
  EXIF-orientation metadata slot. Orientation `3` remains covered by
  `FMT-TIFF-002A`.
- The previous completed work packet is `FMT-TIFF-002A`, a child of
  `FMT-TIFF-002`, for TIFF IFD0 Orientation=3 open semantics. Local Pillow
  11.3.0 shows a bounded hand-written `2x3` mode `L` TIFF with Orientation
  tag `3` reopens as size `(2, 3)`, pixels `[60, 50, 40, 30, 20, 10]`, and no
  exposed `getexif()[274]` value. Native TIFF open applies a 180-degree pixel
  reorder inside the DLL for orientation `3` after successful WIC decode and
  does not set the EXIF-orientation metadata slot.
- The previous completed work packet is `META-001B`, a child of `META-001`, for
  TIFF IFD0 Orientation=1 read metadata. Local Pillow 11.3.0 shows a bounded
  hand-written `2x3` mode `L` TIFF with Orientation tag `1` reopens with
  unchanged pixels and `getexif()[274] == 1`. Native TIFF open now parses the
  file's IFD0 orientation after successful WIC decode and attaches value `1`
  through the existing orientation metadata slot; the facade returns it from
  both `Image.ExifOrientation()` and `Image.GetExif()` without AHK pixel loops.
  Non-1 TIFF orientation values, pixel reorientation, and broad TIFF tag
  lifecycle remain deferred.
- The latest completed work packet is `MODE-NUM-001I`, a child of
  `MODE-I-001` / `MODE-F-001`, for numeric-mode `Image.blend` wrong-mode
  rejection. Local Pillow 11.3.0 shows `Image.blend(left, right, alpha)`
  raises `ValueError: image has wrong mode` for matching mode `I` and
  matching mode `F` inputs with valid alpha values, while `Image.composite`
  remains an allowed byte-level numeric path. Native `pillow_c_image_blend`
  and `pillow_c_image_blend_into` now reject those modes with
  `PILLOW_C_INVALID_ARGUMENT`, and the facade normalizes that status to
  Pillow-style `Error("image has wrong mode", -1)` without AHK per-pixel
  loops.
- The previous completed work packet is `MODE-NUM-001H`, a child of
  `MODE-I-001` / `MODE-F-001`, for numeric-mode non-logical
  `ImageChops` binary operation wrong-mode rejection. Local Pillow 11.3.0
  shows `difference`, `multiply`, `screen`, `lighter`, `darker`,
  `soft_light`, `hard_light`, `overlay`, `add`, `subtract`, `add_modulo`,
  and `subtract_modulo` raise `ValueError: image has wrong mode` for matching
  mode `I` and matching mode `F` inputs. Native shared ImageChops binary
  validation now rejects those modes with `PILLOW_C_INVALID_ARGUMENT`, and the
  facade normalizes that status to Pillow-style
  `Error("image has wrong mode", -1)` without AHK per-pixel loops.
- The previous completed work packet is `MODE-NUM-001G`, a child of
  `MODE-I-001` / `MODE-F-001`, for numeric-mode `ImageFilter.BoxBlur`,
  `GaussianBlur`, and `UnsharpMask` wrong-mode rejection. Local Pillow 11.3.0
  shows all three filters raise `ValueError: image has wrong mode` for mode
  `I` and mode `F` with valid filter arguments. Native blur/unsharp filter
  helpers now reject those modes with `PILLOW_C_INVALID_ARGUMENT`, and the
  facade normalizes that status to Pillow-style
  `Error("image has wrong mode", -1)` without AHK per-pixel loops.
- The previous completed work packet is `MODE-NUM-001F`, a child of
  `MODE-I-001` / `MODE-F-001`, for numeric-mode `ImageFilter.ModeFilter`
  wrong-mode rejection. Local Pillow 11.3.0 shows `ModeFilter` raises
  `ValueError: image has wrong mode` for mode `I` and mode `F` across the
  bounded size probes. Native `filter_mode_image_into` now rejects those modes
  with `PILLOW_C_INVALID_ARGUMENT`, and the facade normalizes that status to
  Pillow-style `Error("image has wrong mode", -1)` without AHK per-pixel
  loops.
- The prior completed work packet is `MODE-NUM-001E`, a child of
  `MODE-I-001` / `MODE-F-001`, for numeric-mode `ImageFilter.RankFilter`.
  Local Pillow 11.3.0 shows mode `I` Rank/Min/Median/Max filters sort one
  signed-int32 sample per pixel with clamped-edge windows, and mode `F` uses
  one float32 sample per pixel for the same rank-window semantics. Native
  `filter_rank_image_into` handles modes `I` and `F` as numeric samples
  instead of four independent byte channels, `_into` reuses caller storage, and
  the facade routes `Image.Filter(Pillow.ImageFilter.MinFilter/MedianFilter/
  MaxFilter/RankFilter)` through the DLL without AHK per-pixel loops.
- The earlier completed work packet is `MODE-NUM-001D`, a child of
  `MODE-I-001` / `MODE-F-001`, for numeric-mode `ImageFilter.Kernel`.
  Local Pillow 11.3.0 shows mode `I` Kernel filters one signed-int32 sample
  per pixel, copies border int32 pixels unchanged, uses Pillow's vertical
  kernel flip, rounds positive fractional results half-up, clips filtered
  negatives to `0`, clips filtered overflow to `2147483647`, and writes
  `-2147483648` for explicit `scale=0`; mode `F` Kernel raises
  `ValueError: image has wrong mode`. Native `filter_kernel_image_into` now
  handles mode `I` as little-endian signed int32 samples and rejects mode `F`;
  the facade routes mode `I` through the DLL and normalizes mode `F` Kernel
  rejection without AHK per-pixel loops.
- The previous completed work packet is `MODE-NUM-001C`, a child of
  `MODE-I-001` / `MODE-F-001`, for numeric-mode `Image.Convert("L")` /
  native conversion semantics. Local Pillow 11.3.0 shows `I -> L` and
  `F -> L` truncate toward zero then clamp to `0..255`, with `NaN -> 0`,
  `-Inf -> 0`, and `+Inf -> 255` for `F`; empty `(0, 1)` numeric images
  convert to empty mode `L`. Native `convert_image_mode_into` now handles
  source modes `I` and `F` targeting `L`, `_into` reuses caller storage, and
  `pillow_c_image_create_mode` plus shared shape matching preserve empty image
  shapes for this public path. The facade routes `Image.Convert("L")` through
  the native handle path without AHK per-pixel loops.
- The previous completed work packet is `MODE-NUM-001B`, a child of
  `MODE-I-001` / `MODE-F-001`, for `Image.Histogram()` / native histogram
  semantics. Local Pillow 11.3.0 reports 256 bins for `I.histogram()` and
  `F.histogram()`, scaling values from image extrema; all-equal numeric
  images produce an all-zero histogram. Masked `I`/`F` histograms raise
  `ValueError: image has wrong mode`. Native `pillow_c_image_histogram` now
  exposes one 256-bin numeric histogram for `I` and `F` instead of four
  byte-storage bands, and `pillow_c_image_histogram_masked` rejects numeric
  modes with `PILLOW_C_INVALID_ARGUMENT`. The facade uses
  `Pillow.Image.GetModeBands(this.Mode) * 256` so public `Image.Histogram()`
  returns the Pillow-visible band count.
- The previous completed work packet is the `MODE-F-001A` child for mode `F`
  `getextrema()` semantics. Local Pillow 11.3.0 shows finite float32 samples
  `[-1.5, 0.0, 2.25, -3.75]` return `(-3.75, 2.25)`, later `NaN` samples are
  ignored by the normal comparison path, a first-sample `NaN` leaves both
  extrema as `NaN`, `-Inf` / `Inf` participate normally, and empty mode `F`
  images return `None`. Native `pillow_c_image_get_extrema_numeric` exposes
  one Pillow-visible numeric band for mode `F`; the facade routes
  `Pillow.Image.GetExtrema()` through it for mode `F`.
- The previous completed mode work packet is the `MODE-I-001A` child for mode `I`
  `getextrema()` semantics. Local Pillow 11.3.0 shows a mode `I` image with
  signed int32 samples `[-10, 0, 70000, -2147483648]` returns
  `(-2147483648, 70000)` from `getextrema()`, and empty mode `I` images return
  `None`. Native `pillow_c_image_get_extrema_numeric` exposes numeric extrema
  with one Pillow-visible band for mode `I`; the facade routes
  `Pillow.Image.GetExtrema()` through it for mode `I` so it no longer reports
  four byte-storage bands. `I`/`F` conversions, arithmetic, filters, and
  broader file-format participation remain deferred under `MODE-F-001` /
  `MODE-I-001`.
- The previous completed work packet is the `FMT-TIFF-001A` child for bounded
  TIFF multipage save. Local Pillow 11.3.0 shows
  `Image.save(..., format="TIFF", save_all=True, append_images=[...])` writes
  a little-endian TIFF with two frames, reopens with `n_frames == 2` and
  `is_animated == True`, preserves frame bytes, and raises `EOFError` when
  seeking past the sequence. Native `pillow_c_image_save_tiff_frames` writes a
  little-endian uncompressed TIFF IFD chain inside the DLL for the
  native-supported `L`/`RGB`/`RGBA` modes; the public facade routes TIFF
  `save_all` / `append_images` to that export without AHK pixel loops. TIFF
  tags, compression options, palette/bilevel/high-bit modes, `I`/`F`, and
  BigTIFF remain deferred under `FMT-TIFF-002` / `FMT-TIFF-003`.
- The latest completed work packet is the `BYTES-001AB` child for readonly
  `frombuffer` view refresh on native transpose reads. Local Pillow 11.3.0
  shows `im.transpose(Image.Transpose.ROTATE_90)` over a readonly raw `L`
  buffer view samples caller mutations made after construction, producing
  `[3,6,2,5,1,4]` for the bounded `3x2` stride-4 fixture while the input
  remains readonly and the output is owned. Native `pillow_c_image_transpose`
  and `pillow_c_image_transpose_into` now share the same helper path and refresh
  the source view before copying transposed pixels. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001AA` child for readonly
  `frombuffer` view refresh on native ImageOps histogram-transform reads. Local
  Pillow 11.3.0 shows `ImageOps.equalize(im)`,
  `ImageOps.autocontrast(im)`, and masked autocontrast sample the latest caller
  bytes from attached readonly raw `L` source and mask buffer views before
  building histograms and LUTs. Native `pillow_c_image_equalize`,
  `pillow_c_image_equalize_into`, `pillow_c_image_equalize_masked`,
  `pillow_c_image_equalize_masked_into`, `pillow_c_image_autocontrast`, and
  `pillow_c_image_autocontrast_into` refresh the source and optional mask views
  before histogram/LUT reads. No ABI symbol was added.
- The earlier completed work packet is the `BYTES-001Z` child for readonly
  `frombuffer` view refresh on native `ImageChops.Offset` reads. Local Pillow
  11.3.0 shows `ImageChops.offset(im, 1, 0)` and
  `ImageChops.offset(im, 0, 1)` sample the latest caller bytes from an attached
  readonly raw `L` buffer view while leaving the input image readonly and
  returning owned output images. Native `pillow_c_image_offset` and
  `pillow_c_image_offset_into` now refresh the source buffer view before
  reading pixels. No ABI symbol was added.
- The earlier completed work packet is the `BYTES-001Y` child for readonly
  `frombuffer` view refresh on native blend/composite reads. Local Pillow
  11.3.0 shows `Image.blend(left, right, alpha)` and
  `Image.composite(left, right, mask)` sample the latest caller bytes from
  attached readonly raw `L` source and mask buffer views while leaving input
  images readonly and returning owned output images. Native
  `pillow_c_image_blend`, `pillow_c_image_blend_into`,
  `pillow_c_image_composite`, and `pillow_c_image_composite_into` now refresh
  source/mask buffer views before reading pixels. No ABI symbol was added.
- The earlier completed work packet is the `BYTES-001X` child for readonly
  `frombuffer` view refresh on native ImageChops unary/binary reads. Local
  Pillow 11.3.0 shows `ImageChops.invert(im)`,
  `ImageChops.difference(left, right)`, and `ImageChops.multiply(left, right)`
  sample the latest caller bytes from attached readonly raw `L` buffer views
  while leaving source images readonly and returning owned output images.
  Native `pillow_c_image_chops_invert`,
  `pillow_c_image_difference`, `pillow_c_image_multiply`, and the shared
  helper-backed `_into`/binary/logical families now refresh source buffer
  views before reading pixels. No ABI symbol was added.
- The earlier completed work packet is the `BYTES-001W` child for readonly
  `frombuffer` view refresh on native ImageOps LUT reads. Local Pillow 11.3.0
  shows `ImageOps.invert(im)`, `ImageOps.posterize(im, bits)`, and
  `ImageOps.solarize(im, threshold)` sample the latest caller bytes from an
  attached readonly raw `L` buffer view while leaving the source image readonly
  and returning an owned output image. Native `pillow_c_image_invert`,
  `pillow_c_image_posterize`, `pillow_c_image_solarize`, and their `_into`
  variants now refresh the source buffer view before reading pixels. No ABI
  symbol was added.
- The earlier completed work packet is the `BYTES-001V` child for readonly
  `frombuffer` view refresh on native point/LUT transform reads. Local Pillow
  11.3.0 shows `im.point(lut)` and `im.point(lut, "L")` sample the latest
  caller bytes from an attached readonly raw `L` buffer view while leaving the
  source image readonly and returning an owned output image. Native
  `pillow_c_image_point_lut`, `pillow_c_image_point_lut_mode`,
  `pillow_c_image_point_lut_into`, and
  `pillow_c_image_point_lut_mode_into` refresh the source buffer view before
  reading pixels. No ABI symbol was added.
- The prior completed work packet is the `BYTES-001U` child for readonly
  `frombuffer` view refresh on native filter reads. Local Pillow 11.3.0 shows
  `im.filter(ImageFilter.Kernel(...))` samples the latest caller bytes from an
  attached readonly raw `L` buffer view while leaving the source image readonly
  and returning an owned filtered image. Native
  `pillow_c_image_filter_kernel`, `pillow_c_image_filter_rank`,
  `pillow_c_image_filter_mode`, `pillow_c_image_filter_box_blur`,
  `pillow_c_image_filter_gaussian_blur`,
  `pillow_c_image_filter_unsharp_mask`, `pillow_c_image_filter_color_3d_lut`,
  and their `_into` variants now refresh the source buffer view before reading
  pixels. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001T` child for readonly
  `frombuffer` view refresh on band merge reads. Local Pillow 11.3.0 shows
  `Image.merge("RGB", [im, im, im])` samples the latest caller bytes from
  attached readonly raw `L` band views while leaving each source band readonly
  and returning an owned merged image. Native `pillow_c_image_merge_bands` and
  `pillow_c_image_merge_bands_into` now refresh each source band buffer view
  before interleaving pixels. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001S` child for readonly
  `frombuffer` view refresh on band extraction reads. Local Pillow 11.3.0
  shows `im.getchannel(0)` and `im.split()[0]` sample the latest caller bytes
  from an attached readonly raw `L` buffer view while leaving the source image
  readonly and returning owned band images. Native
  `pillow_c_image_get_channel`, `pillow_c_image_get_channel_into`, and
  `pillow_c_image_split_bands` now refresh the source buffer view before
  copying band pixels. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001R` child for readonly
  `frombuffer` view refresh on full-image copy reads. Local Pillow 11.3.0
  shows `im.copy()` samples the latest caller bytes from an attached readonly
  raw `L` buffer view while leaving the source image readonly and returning an
  owned copy image. Native `pillow_c_image_copy` and
  `pillow_c_image_copy_into` now refresh the source buffer view before copying
  pixels, and facade `Image.Copy()` no longer performs a wrapper-level
  pre-refresh. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001Q` child for readonly
  `frombuffer` view refresh on crop reads. Local Pillow 11.3.0 shows
  `im.crop((0, 0, 2, 1))` samples the latest caller bytes from an attached
  readonly raw `L` buffer view while leaving the source image readonly and
  returning an owned cropped image. Native `pillow_c_image_crop` and
  `pillow_c_image_crop_into` now refresh the source buffer view before copying
  pixels. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001P` child for readonly
  `frombuffer` view refresh on getpixel reads. Local Pillow 11.3.0 shows
  `im.getpixel((1, 0))` samples the latest caller bytes from an attached
  readonly raw `L` buffer view while leaving the source image readonly.
  Native `pillow_c_image_getpixel` now refreshes the source buffer view before
  reading the pixel, and the facade no longer pre-refreshes in
  `Image.GetPixel()`. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001O` child for readonly
  `frombuffer` view refresh on convert reads. Local Pillow 11.3.0 shows
  `im.convert("RGB")` samples the latest caller bytes from an attached readonly
  raw `L` buffer view while leaving the source image readonly and returning an
  owned converted image. Native `pillow_c_image_convert_mode`,
  `pillow_c_image_convert_mode_into`, and their dither variants now refresh
  the source buffer view before converting. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001N` child for readonly
  `frombuffer` view refresh on getextrema reads. Local Pillow 11.3.0 shows
  `im.getextrema()` samples the latest caller bytes from an attached readonly
  raw `L` buffer view while leaving the source image readonly. Native
  `pillow_c_image_get_extrema` now refreshes the source buffer view before
  computing band extrema. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001M` child for readonly
  `frombuffer` view refresh on entropy reads. Local Pillow 11.3.0 shows
  `im.entropy()` samples the latest caller bytes from an attached readonly raw
  `L` buffer view while leaving the source image readonly. Native
  `pillow_c_image_entropy` now refreshes the source buffer view, and refreshes
  a mask handle when present, before computing entropy. No ABI symbol was
  added.
- The previous completed work packet is the `BYTES-001L` child for readonly
  `frombuffer` view refresh on getcolors reads. Local Pillow 11.3.0 shows
  `im.getcolors()` samples the latest caller bytes from an attached readonly
  raw `L` buffer view while leaving the source image readonly. Native
  `pillow_c_image_getcolors` now refreshes the source buffer view before
  counting unique colors. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001K` child for readonly
  `frombuffer` view refresh on getprojection reads. Local Pillow 11.3.0 shows
  `im.getprojection()` samples the latest caller bytes from an attached
  readonly raw `L` buffer view while leaving the source image readonly. Native
  `pillow_c_image_getprojection` now refreshes the source buffer view before
  computing x/y projections. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001J` child for readonly
  `frombuffer` view refresh on getbbox reads. Local Pillow 11.3.0 shows
  `im.getbbox()` samples the latest caller bytes from an attached readonly raw
  `L` buffer view while leaving the source image readonly. Native
  `pillow_c_image_getbbox` now refreshes the source buffer view before scanning
  for a nonzero bounding box. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001I` child for readonly
  `frombuffer` view refresh on histogram reads. Local Pillow 11.3.0 shows
  `im.histogram()` samples the latest caller bytes from an attached readonly
  raw `L` buffer view while leaving the source image readonly. Native
  `pillow_c_image_histogram` now refreshes the source buffer view before
  computing bins, and `pillow_c_image_histogram_masked` refreshes both source
  and mask handles before masked counting. No ABI symbol was added.
- The previous completed work packet is the `BYTES-001H` child for readonly
  `frombuffer` view refresh on raw byte extraction. Local Pillow 11.3.0 shows
  `im.tobytes()` and `im.tobytes("raw", "L")` sample the latest caller bytes
  from an attached readonly raw `L` buffer view while leaving the source image
  readonly. Native `pillow_c_image_get_bytes` and
  `pillow_c_image_get_raw_bytes` now refresh source buffer views before copying
  or raw-encoding bytes; no ABI symbol was added.
- The previous completed work packet is the `BYTES-001G` child for readonly
  `frombuffer` view refresh on native resize reads and public `Thumbnail`
  detach. Local Pillow 11.3.0 shows `im.resize((1,1), NEAREST)` samples the
  latest caller byte from an attached readonly raw `L` buffer view while
  leaving the source image readonly, and `im.thumbnail((1,1), NEAREST)` samples
  the same latest caller byte but replaces the image with an owned resized
  handle whose `readonly` is `0`. Native resize, resize-box, and reducing-gap
  exports now refresh source buffer views before sampling; no ABI symbol was
  added.
- The previous completed work packet is the `BYTES-001F` child for readonly
  `frombuffer` view detachment on native paste writes. Local Pillow 11.3.0
  shows both `im.paste(color, box)` and `im.paste(source_image, xy)` detach a
  readonly raw `L` buffer view: `readonly` becomes `0`, pasted pixels remain in
  owned image storage, and later source bytearray mutation is ignored. Native
  paste image, masked paste, and paste color exports now use the same
  detach-before-write helper as ImageDraw. No ABI symbol was added, and facade
  `Image.Paste` routing stays native-first.
- The previous completed work packet is the `BYTES-001E` child for readonly
  `frombuffer` view detachment on native ImageDraw writes. Local Pillow 11.3.0
  shows a readonly raw `L` buffer view detaches on
  `ImageDraw.Draw(im).rectangle(...)`: `readonly` becomes `0`, the drawn pixel
  remains in owned image storage, and later source bytearray mutation is
  ignored. Native `with_detached_buffer_view` now wraps exported ImageDraw
  mutators before they call their existing DLL drawing implementations. No ABI
  symbol was added, and facade ImageDraw routing stays native-first.
- The previous completed work packet is the `BYTES-001D` child for direct
  `Image.frombuffer` RGB/RGBA ownership semantics. Local Pillow 11.3.0 shows
  direct `RGB` rawmode copies with `readonly == 0` and does not reflect source
  mutation, while direct `RGBA` rawmode with stride `10` and orientation `-1`
  starts readonly, reads rows bottom-up, reflects source mutation until the
  first write, and detaches after `putpixel`. Native
  `pillow_c_image_frombuffer_raw` now filters the caller `alias_source`
  request through Pillow-compatible raw mapmodes, so `RGB` remains owned even
  if a raw caller asks for aliasing; `RGBA` still aliases through the existing
  bulk refresh/detach path. The facade covers both public behaviors through
  `Pillow.Image.FromBuffer`.
- The previous completed work packet is the `BYTES-001C` child for public
  `RGBX` `Image.frombuffer` raw mapmode semantics. Local Pillow 11.3.0 shows
  `Image.frombuffer("RGB", size, bytearray, "raw", "RGBX", 10, -1)` returns
  a readonly `RGBX` image with bands `("R", "G", "B", "X")`, reflects source
  mutation until the first write, and detaches after `putpixel`. Native mode
  id `10` is now `RGBX`; `pillow_c_image_frombuffer_raw` selects `RGBX` from
  the raw mapmode and refreshes/detaches through the existing bulk buffer-view
  path. The facade allows requested mode/rawmode `RGBX`, keeps the source
  AHK `Buffer` alive while readonly, and reports `GetBands()` as
  `["R", "G", "B", "X"]`.
- The previous completed work packet is the `BYTES-001B` child for bounded
  `Image.frombuffer` raw mapmode override semantics. Local Pillow 11.3.0 shows
  raw mapmodes `L` and `RGBA` override requested `RGB` and preserve readonly
  source-buffer aliasing; the current DLL/facade route matches that bounded
  behavior.
- The previous completed work packet is the `BYTES-001A` child for bounded
  `Image.frombuffer` raw `L` alias/detach semantics. The native
  `pillow_c_image_frombuffer_raw`, `pillow_c_image_refresh_buffer`,
  `pillow_c_image_detach_buffer`, and `pillow_c_image_readonly` exports record
  a caller-owned external buffer view, bulk-refresh native storage from it, and
  detach before covered writes. The facade exposes `Pillow.Image.FromBuffer`
  and keeps the source AHK `Buffer` alive while the handle is readonly.
- The previous metadata work packet is the `META-001` child for
  bounded `Image.getexif()` / `Image.Exif` orientation lifecycle. The native
  `pillow_c_exif_orientation_bytes` export serializes Pillow-compatible empty
  EXIF and orientation tag bytes; the facade exposes a bounded Exif object and
  accepts it in JPEG/PNG `exif` save options.
- The most recent JPEG coverage-only packet before that remains valid:
  Pillow preserves opened `comment` implicitly on keep-style saves but does not
  implicitly preserve opened ICC/EXIF unless the caller passes them. Current
  native/facade behavior already matched that rule.
- The default next packet should move off the now-covered direct
  RGB/RGBA/RGBX constructor and numeric-histogram surfaces unless a concrete
  remaining readonly/detach or numeric-mode edge is proven. Best next choices
  are another locally proven `META-001` EXIF/TIFF object behavior, a new TIFF
  tag/compression child under `FMT-TIFF-002` / `FMT-TIFF-003`, another
  mode-scoped `MODE-I-001` / `MODE-F-001` operation slice, or a
  dependency-scoped `FMT-WEBP-001` still-image milestone if broad format parity
  is selected.
- PNG speed problem: `FMT-PNG-001AA`-style one-combination branches are too
  slow because known metadata routes were being validated as a cross-product
  of options instead of as route-level behavior. Future PNG work should add a
  branch only for a new native capability, ABI shape, or newly proven Pillow
  semantic boundary; same-route combinations should be batched through
  generalized metadata routing.
- GIF remains important, but only continue when a concrete local Pillow oracle
  proves a palette-stability or pathological animation boundary that current
  tests miss. Do not expand speculative GIF matrices.
- If the next goal is broader compatibility rather than format-local cleanup,
  do not continue PNG/GIF/JPEG option tails by default. Choose a new
  `META-001` child when metadata-object behavior is the priority, choose a
  mode-scoped `MODE-I-001` / `MODE-F-001` slice when operation breadth is the
  priority, or continue TIFF only as tag/compression/high-bit/mode children.
  Continue `BYTES-001` only for a concrete remaining
  readonly/detach miss or dependency constructor.

Direct-diff route rule:

- Prefer work that collapses several Pillow gaps at once: metadata object
  lifecycle, constructor/buffer ownership, mode semantics, and save-all/tags.
- Use direct Pillow comparison to choose the next boundary, but do not turn
  every missing Pillow name into a branch. Name coverage is a route signal; the
  acceptance unit is still a bounded gap ID with oracle-backed behavior.
- Route order for speed: first reusable semantic pillars, then hot-format
  boundaries with a new native route, then dependency-gated format families,
  then long-tail plugin parity.
- Only open a format-local branch when it proves a new native capability, a new
  ABI shape, or a newly observed Pillow semantic boundary.
- Treat same-route PNG/JPEG/GIF option combinations as batched coverage, not
  separate implementation projects.
- Keep long-tail formats behind explicit format gap IDs unless the user
  chooses dependency-gated format expansion as the main milestone.
- `FMT-PNG-001AA` got slow because same-route metadata combinations were being
  validated as separate branches. Going forward, one new branch must buy a new
  native route, ABI shape, or proven Pillow semantic boundary; otherwise batch
  the cases under the existing generalized route.
- The direct Pillow matrix confirms that broad plugin parity is still the main
  full-replacement deficit, but it should not drive the next increment unless
  the user explicitly chooses dependency-gated format expansion. For faster
  progress on the AHK-first target, prefer reusable semantic pillars before
  long-tail format plugins.

Covered oracle and implementation evidence for `FMT-TIFF-003D`:

- Local Pillow 11.3.0 oracle for a `2x1` mode `CMYK` TIFF showed reopen mode
  `CMYK`, size `(2,1)`, bytes `[1,2,3,4,10,20,30,40]`, IFD0 BitsPerSample
  `[8,8,8,8]`, Compression `1`, PhotometricInterpretation `5`,
  SamplesPerPixel `4`, RowsPerStrip `1`, StripByteCounts `8`, and
  PlanarConfiguration `1`.
- Raw red:
  `pillow_c image save_tiff round-trips CMYK mode` failed with
  `Expected 0, got -3`, proving the native TIFF save validation rejected mode
  `CMYK`.
- Facade red:
  `Pillow Image.Save TIFF CMYK mode uses native path` errored with
  `pillow_c: invalid argument`, proving public `Image.Save(..., "TIFF")`
  could not route mode `CMYK` through the native TIFF path.
- Native change:
  `validate_tiff_save_image` accepts mode `CMYK`, WIC 32bpp CMYK TIFF decode
  maps to public mode `CMYK`, and the DLL-owned TIFF writer emits
  Pillow-compatible CMYK IFD tags plus raw strip bytes. Compressed CMYK TIFF
  remains rejected until separately probed.
- Facade change:
  no new facade dispatch was needed; existing `Image.Save(..., "TIFF")` and
  `Image.Open(..., ["TIFF"])` routes now work for `CMYK` handles without AHK
  pixel loops.
- ABI:
  no new export; existing TIFF save/open exports gained bounded mode `CMYK`
  behavior.
- Release x64 rebuild refreshed `build\x64\Release\pillow_c.dll` with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 79ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- TIFF regression filter:
  `Ran 33 tests in 2359ms; Passed: 33, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory suite:
  `Ran 1079 tests in 100047ms; Passed: 1079, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.
- Export count after rebuild: unchanged at `339` source declarations / `339`
  DLL exports.

Covered oracle and implementation evidence for `FMT-TIFF-003C`:

- Local Pillow 11.3.0 oracle for a `2x1` mode `LA` TIFF showed reopen mode
  `LA`, size `(2,1)`, bytes `[10,255,40,128]`, IFD0 BitsPerSample `[8,8]`,
  Compression `1`, PhotometricInterpretation `1`, SamplesPerPixel `2`,
  RowsPerStrip `1`, StripByteCounts `4`, PlanarConfiguration `1`, and
  ExtraSamples `2`.
- Raw red:
  `pillow_c image save_tiff round-trips LA mode` failed with
  `Expected 0, got -3`, proving the native TIFF save validation rejected mode
  `LA`.
- Facade red:
  `Pillow Image.Save TIFF LA mode uses native path` errored with
  `pillow_c: invalid argument`, proving public `Image.Save(..., "TIFF")`
  could not route mode `LA` through the native TIFF path.
- Native change:
  `validate_tiff_save_image` accepts mode `LA`, the DLL-owned TIFF writer emits
  the Pillow-compatible grayscale-alpha IFD tags and raw strip bytes, and
  frame-0 native TIFF open recognizes that IFD0 shape and stores public `LA`
  bytes after WIC decode.
- Facade change:
  no new facade dispatch was needed; existing `Image.Save(..., "TIFF")` and
  `Image.Open(..., ["TIFF"])` routes now work for `LA` handles without AHK
  pixel loops.
- ABI:
  no new export; existing TIFF save/open exports gained bounded mode `LA`
  behavior.
- Release x64 rebuild refreshed `build\x64\Release\pillow_c.dll` with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 62ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 110ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- TIFF regression filter:
  `Ran 31 tests in 3156ms; Passed: 31, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory suite:
  `Ran 1077 tests in 102515ms; Passed: 1077, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.
- Export count after rebuild: unchanged at `339` source declarations / `339`
  DLL exports.

Covered oracle and implementation evidence for `FMT-TIFF-002G`:

- Local Pillow 11.3.0 oracle for a `2x2` mode `RGB` TIFF saved with
  `format="TIFF", compression="tiff_adobe_deflate"` showed IFD0 Compression
  tag `259` as SHORT value `8`, RowsPerStrip `2`, StripByteCounts `20`, and a
  zlib strip beginning `[0x78,0x9C]` that inflates to the original RGB bytes
  `[10,20,30,10,20,30,40,50,60,40,50,60]`.
- The same oracle showed `compression="tiff_deflate"` behaves the same, while
  bare `compression="deflate"` writes uncompressed tag `1`; the facade does
  not alias bare `"deflate"` to code `8`.
- Raw red:
  `pillow_c image save_tiff_compression_options writes deflate` failed with
  `Expected 0, got -3`, proving the existing native compression export still
  rejected compression code `8`.
- Facade red:
  `Pillow Image.Save TIFF deflate compression uses native path` errored with
  `Pillow.Image.Save TIFF compression is not supported`, proving public option
  normalization did not route `tiff_adobe_deflate`.
- Native change:
  `normalize_tiff_save_compression` now accepts code `8`, and
  `save_tiff_frames_image_with_options` routes that code through a native zlib
  stored stream for the prepared single strip with Pillow-style `0x78 0x9C`
  header.
- Facade change:
  `Image.Save(..., "TIFF", {compression:"tiff_adobe_deflate"})` and
  `{compression:"tiff_deflate"}` normalize to code `8` and use the existing
  DLL compression export; no AHK-side pixel loop was added.
- ABI:
  no new export; the existing TIFF compression export gained bounded Adobe
  Deflate code support.
- Release x64 rebuild refreshed `build\x64\Release\pillow_c.dll` with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 93ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- TIFF regression filter:
  `Ran 29 tests in 1985ms; Passed: 29, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory suite:
  `Ran 1075 tests in 80406ms; Passed: 1075, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.
- Export count after rebuild: `339` source declarations / `339` DLL exports.

Covered oracle and implementation evidence for `FMT-TIFF-002F`:

- Local Pillow 11.3.0 oracle for a `2x2` mode `RGB` TIFF saved with
  `format="TIFF", compression="tiff_lzw"` showed IFD0 Compression tag `259`
  as SHORT value `5`, RowsPerStrip `2`, StripByteCounts `14`, and a single
  LZW strip that reopens as mode `RGB`, size `(2,2)`, with RGB bytes
  `[10,20,30,10,20,30,40,50,60,40,50,60]`.
- Raw red:
  `pillow_c image save_tiff_compression_options writes lzw` failed with
  `Expected 0, got -3`, proving the existing native compression export still
  rejected compression code `5`.
- Facade red:
  `Pillow Image.Save TIFF lzw compression uses native path` errored with
  `Pillow.Image.Save TIFF compression is not supported`, proving the public
  option normalization did not route `tiff_lzw`.
- Native change:
  `normalize_tiff_save_compression` now accepts code `5`, and
  `save_tiff_frames_image_with_options` routes that code through a native TIFF
  LZW encoder using clear/EOI codes, a prefix-byte dictionary, and MSB-first
  9-to-12-bit code packing for the prepared single strip.
- Facade change:
  `Image.Save(..., "TIFF", {compression:"tiff_lzw"})` and
  `{compression:"lzw"}` normalize to code `5` and use the existing DLL
  compression export; no AHK-side pixel loop was added.
- ABI:
  no new export; the existing TIFF compression export gained bounded LZW code
  support.
- Release x64 rebuild refreshed `build\x64\Release\pillow_c.dll` with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- TIFF regression filter:
  `Ran 27 tests in 2078ms; Passed: 27, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory suite:
  `Ran 1073 tests in 103375ms; Passed: 1073, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.
- Export count after rebuild: `339` source declarations / `339` DLL exports.

Covered oracle and implementation evidence for `FMT-TIFF-003B`:

- Local Pillow 11.3.0 oracle for a `3x2` mode `P` TIFF with palette bytes
  `[10,20,30, 40,50,60, 70,80,90]` showed reopen mode `P`, size `(3,2)`,
  index bytes `[0,1,2,1,0,2]`, and first palette bytes
  `[10,20,30,40,50,60,70,80,90]`.
- The same oracle showed IFD0 contains `BitsPerSample=8`,
  `Compression=1`, `PhotometricInterpretation=3`, `PlanarConfiguration=1`,
  `ColorMap` tag `320` as `768` SHORT values, no `SamplesPerPixel`, and strip
  bytes equal to the raw palette indexes.
- Raw red:
  `pillow_c image save_tiff round-trips P mode` failed with
  `Expected 0, got -3`, proving native TIFF save still rejected mode `P`.
- Facade red:
  `Pillow Image.Save TIFF P mode uses native path` errored with
  `pillow_c: invalid argument`, proving the public save path surfaced the same
  missing native support.
- Native change:
  `save_tiff_frames_image_with_options` now validates palette handles,
  writes palette TIFF IFD entries and ColorMap planes inside the DLL-owned
  writer, and preserves raw index strip bytes. `open_tiff_frame_image` maps
  WIC 8bpp indexed TIFF frames to mode `P` and parses the original TIFF IFD0
  ColorMap for frame `0` so palette bytes match Pillow's `byte * 256` layout
  instead of WIC's quantized palette.
- No new ABI export was added; existing TIFF save/open exports gained this
  bounded mode behavior.
- Release x64 rebuild refreshed `build\x64\Release\pillow_c.dll`.
- Targeted raw green:
  `Ran 1 tests in 125ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 219ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- TIFF regression filter:
  `Ran 25 tests in 3141ms; Passed: 25, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory single invocation at `-TimeoutSeconds 120` timed out
  twice during the raw-file portion after the facade file alone took
  `97953ms`. Split full-file verification passed facade `539/539` in
  `97953ms` and raw DLL `532/532` in `35188ms`, with the same known
  non-failing libjpeg stderr warnings.
- Export count after rebuild: `339` source declarations / `339` DLL exports.

Covered oracle and implementation evidence for `FMT-TIFF-003A`:

- Local Pillow 11.3.0 oracle for an `8x2` mode `1` TIFF: reopening preserves
  mode `1`, size `(8, 2)`, packed `tobytes()` bytes `[0x5A, 0xC5]`, and
  pixels `[0,255,0,255,255,0,255,0,255,255,0,0,0,255,0,255]`. The saved IFD0
  omits `BitsPerSample` and `SamplesPerPixel`, uses Compression `1`,
  PhotometricInterpretation `1`, RowsPerStrip `2`, StripByteCounts `2`, and
  strip bytes `[0x5A, 0xC5]`.
- Raw red:
  `pillow_c image save_tiff round-trips mode 1` failed with `Expected 0, got -3`
  while reopening the just-saved TIFF.
- Facade red:
  `Pillow Image.Save TIFF mode 1 uses native path` failed with
  `pillow_c: invalid argument` when reopening the TIFF through the public
  facade.
- Native change:
  the existing TIFF save path now accepts mode `1`, packs one byte per eight
  pixels for the strip, omits the Pillow-omitted bilevel tags, and the TIFF
  open guard now accepts WIC's 1bpp pixel format as mode `1` instead of
  rejecting it after format detection.
- Facade change:
  mode `1` TIFF save/open is routed through the existing native TIFF facade
  path; no AHK-side per-pixel loop was added.
- ABI:
  no new export; the existing TIFF save/open exports gained bounded mode `1`
  behavior.
- Release x64 rebuild after the native behavior change:
  `build\x64\Release\pillow_c.dll` was rebuilt successfully with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 141ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- TIFF regression filter:
  `Ran 23 tests in 1625ms; Passed: 23, Failed: 0, Errors: 0, Skipped: 0`.
- Current full AHK directory suite:
  `Ran 1069 tests in 71297ms; Passed: 1069, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.
- DLL/source export count after rebuild: `339` `pillow_c_*` exports.

Covered oracle and implementation evidence for `FMT-TIFF-002E`:

- Local Pillow 11.3.0 oracle for a `2x2` mode `RGB` image saved as
  `format="TIFF", compression="packbits"`: IFD0 Compression tag `259` is
  SHORT value `32773`, StripByteCounts reflects PackBits-compressed strip
  bytes, and reopening preserves mode, size, and RGB bytes.
- Raw red:
  `pillow_c image save_tiff_compression_options writes packbits` failed with
  `Expected pillow_c_image_save_tiff_compression_options export: Call to nonexistent function.`
- Facade red:
  `Pillow Image.Save TIFF packbits compression uses native path` failed
  because current TIFF save wrote Compression tag `1`.
- Native change:
  `pillow_c_image_save_tiff_compression_options` now accepts the existing
  bounded DPI option shape plus a compression code. Compression `32773`
  writes TIFF PackBits scanlines row-by-row and records tag `259` as
  `32773`; compression `0` / `1` stays uncompressed. Unsupported compression
  values return an error rather than being silently ignored.
- Facade change:
  `Image.Save(..., "TIFF", {compression:"packbits"})` normalizes the option
  through `SaveTiffCompression` and calls the new DLL export; no AHK-side
  pixel loop was added.
- New ABI export:
  `pillow_c_image_save_tiff_compression_options(image, path, has_dpi, dpi_x, dpi_y, compression)`.
- Release x64 rebuild after the native behavior change:
  `build\x64\Release\pillow_c.dll` was rebuilt successfully with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 79ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- TIFF regression filter:
  `Ran 21 tests in 1375ms; Passed: 21, Failed: 0, Errors: 0, Skipped: 0`.
- Current full AHK directory suite:
  `Ran 1067 tests in 70782ms; Passed: 1067, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.
- DLL/source export count after rebuild: `339` `pillow_c_*` exports.

Covered oracle and implementation evidence for `FMT-TIFF-002D`:

- Local Pillow 11.3.0 oracle for a `2x1` mode `RGB` image saved as
  `format="TIFF", dpi=(300,150)`: reopened size/mode/bytes stay
  `(2, 1)` / `RGB` / `[10, 20, 30, 40, 50, 60]`; `Image.info["dpi"]` is
  `(300.0, 150.0)`; `tag_v2[282] == 300.0`, `tag_v2[283] == 150.0`, and
  `tag_v2[296] == 2`.
- The saved IFD0 contains `XResolution` tag `282` as RATIONAL `300/1`,
  `YResolution` tag `283` as RATIONAL `150/1`, and `ResolutionUnit` tag
  `296` as SHORT value `2`.
- Raw red:
  `pillow_c image save_tiff_options writes dpi metadata` failed with
  `Expected pillow_c_image_save_tiff_options export: Call to nonexistent function.`
- Facade red:
  `Pillow Image.Save TIFF dpi option uses native path` failed because the
  current TIFF save path did not write DPI tags.
- Native change:
  `pillow_c_image_save_tiff_options` now writes TIFF IFD0 resolution tags
  through `save_tiff_frames_image_with_options`; `open_tiff_frame_image`
  parses those tags after frame-0 WIC decode and fills `image->has_dpi`,
  `dpi_x`, and `dpi_y` for the existing metadata export.
- Facade change:
  `Image.Save(..., "TIFF", {dpi:[x,y]})` normalizes the pair with
  `SaveDpiPair` and calls `pillow_c_image_save_tiff_options`; `ApplyNativeMetadata`
  already maps reopened native DPI to `Info["dpi"]`.
- New ABI export:
  `pillow_c_image_save_tiff_options(image, path, has_dpi, dpi_x, dpi_y)`.
- Release x64 rebuild after the native behavior change:
  `build\x64\Release\pillow_c.dll` was rebuilt successfully with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw TIFF regression filter:
  `Ran 10 tests in 312ms; Passed: 10, Failed: 0, Errors: 0, Skipped: 0`.
- Facade TIFF regression filter:
  `Ran 9 tests in 1125ms; Passed: 9, Failed: 0, Errors: 0, Skipped: 0`.
- Current full AHK directory suite:
  `Ran 1065 tests in 71547ms; Passed: 1065, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.
- DLL/source export count after rebuild: `338` `pillow_c_*` exports.

Covered oracle and implementation evidence for `FMT-TIFF-002C`:

- Local Pillow 11.3.0 oracle for the hand-written little-endian `2x3` mode
  `L` TIFF with IFD0 Orientation tag `2`: reopened size stays `(2, 3)`,
  pixels become `[20, 10, 40, 30, 60, 50]`, and `getexif().get(274)` returns
  no value.
- The same bounded oracle for Orientation tag `4`: reopened size stays
  `(2, 3)`, pixels become `[50, 60, 30, 40, 10, 20]`, and
  `getexif().get(274)` returns no value.
- The same bounded oracle for Orientation tag `5`: reopened size becomes
  `(3, 2)`, pixels become `[10, 30, 50, 20, 40, 60]`, and
  `getexif().get(274)` returns no value.
- The same bounded oracle for Orientation tag `7`: reopened size becomes
  `(3, 2)`, pixels become `[60, 40, 20, 50, 30, 10]`, and
  `getexif().get(274)` returns no value.
- Raw red:
  `pillow_c image open_tiff applies orientation mirrors transposes` failed
  with `Expected [20, 10, 40, 30, 60, 50], got [10, 20, 30, 40, 50, 60]`.
- Facade red:
  `Pillow Image.Open TIFF applies orientation mirrors transposes` failed with
  the same unchanged-pixel mismatch.
- Native change:
  `open_tiff_frame_image` now branches on parsed TIFF Orientation values `2`,
  `4`, `5`, and `7`, calling `apply_tiff_orientation_mirror_or_transpose` to
  allocate transformed storage, update `width`, `height`, and `stride` for
  swapped-dimension cases, and leave `image->exif_orientation == 0`.
- No new ABI export was added. The existing TIFF open ABI now covers the full
  bounded Orientation `1..8` fixture set.
- Release x64 rebuild after the native behavior change:
  `build\x64\Release\pillow_c.dll` was rebuilt successfully with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 109ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 235ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw TIFF regression filter:
  `Ran 6 tests in 235ms; Passed: 6, Failed: 0, Errors: 0, Skipped: 0`.
- Facade TIFF regression filter:
  `Ran 8 tests in 875ms; Passed: 8, Failed: 0, Errors: 0, Skipped: 0`.
- Current full AHK directory suite:
  `Ran 1063 tests in 72406ms; Passed: 1063, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Covered oracle and implementation evidence for `FMT-TIFF-002B`:

- Local Pillow 11.3.0 oracle for the hand-written little-endian `2x3` mode
  `L` TIFF with IFD0 Orientation tag `6`: reopened size becomes `(3, 2)`,
  pixels become `[50, 30, 10, 60, 40, 20]`, and `getexif().get(274)` returns
  no value.
- The same bounded oracle for Orientation tag `8`: reopened size becomes
  `(3, 2)`, pixels become `[20, 40, 60, 10, 30, 50]`, and
  `getexif().get(274)` returns no value.
- Raw red:
  `pillow_c image open_tiff applies orientation six eight` failed with
  `Expected [3, 2], got [2, 3]`.
- Facade red:
  `Pillow Image.Open TIFF applies orientation six eight` failed with
  `Expected [3, 2], got [2, 3]`.
- Native change:
  `open_tiff_frame_image` now branches on parsed TIFF Orientation values `6`
  and `8`, calling `apply_tiff_orientation_six_or_eight` to allocate rotated
  storage, update `width`, `height`, and `stride`, and leave
  `image->exif_orientation == 0`.
- No new ABI export was added. The existing TIFF open ABI now covers this
  bounded dimension-swapping orientation-transform behavior.
- Release x64 rebuild after the native behavior change:
  `build\x64\Release\pillow_c.dll` was rebuilt successfully with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 140ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw TIFF regression filter:
  `Ran 5 tests in 156ms; Passed: 5, Failed: 0, Errors: 0, Skipped: 0`.
- Facade TIFF regression filter:
  `Ran 7 tests in 625ms; Passed: 7, Failed: 0, Errors: 0, Skipped: 0`.
- Current full AHK directory suite:
  `Ran 1061 tests in 69891ms; Passed: 1061, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Covered oracle and implementation evidence for `FMT-TIFF-002A`:

- Local Pillow 11.3.0 oracle for a hand-written little-endian `2x3` mode `L`
  TIFF with IFD0 Orientation tag `3`: reopened size stays `(2, 3)`, pixels
  become `[60, 50, 40, 30, 20, 10]`, and `getexif().get(274)` returns no
  value. `ImageOps.exif_transpose` keeps the already-reoriented pixels.
- The same bounded oracle confirms Orientation `1` remains the identity
  metadata case, while Orientation `6` and `8` dimension-swapping behavior is
  now covered separately by `FMT-TIFF-002B`.
- Raw red:
  `pillow_c image open_tiff applies orientation three` failed with
  `Expected [60, 50, 40, 30, 20, 10], got [10, 20, 30, 40, 50, 60]`.
- Facade red:
  `Pillow Image.Open TIFF applies orientation three` failed with the same
  unchanged-pixel mismatch.
- Native change:
  `open_tiff_frame_image` now branches on the parsed TIFF Orientation value.
  Value `1` keeps the previous metadata behavior, while value `3` calls
  `apply_tiff_orientation_three` to reverse row-major pixels by channel group
  inside the DLL and intentionally leaves `image->exif_orientation == 0`.
- No new ABI export was added. The existing TIFF open ABI now covers this
  bounded orientation-transform behavior.
- Release x64 rebuild after the native behavior change:
  `build\x64\Release\pillow_c.dll` was rebuilt successfully with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 63ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw TIFF regression filter:
  `Ran 7 tests in 266ms; Passed: 7, Failed: 0, Errors: 0, Skipped: 0`.
- Facade TIFF regression filter:
  `Ran 6 tests in 609ms; Passed: 6, Failed: 0, Errors: 0, Skipped: 0`.
- Current full AHK directory suite:
  `Ran 1059 tests in 72266ms; Passed: 1059, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Covered oracle and implementation evidence for `META-001B`:

- Local Pillow 11.3.0 oracle for a hand-written little-endian `2x3` mode `L`
  TIFF with IFD0 Orientation tag `1`: reopened pixels stay
  `[10, 20, 30, 40, 50, 60]`, size stays `(2, 3)`, and
  `getexif().get(274) == 1`.
- The same bounded oracle showed no Orientation tag reports no value, while
  non-1 Orientation values such as `3`, `6`, and `8` involve Pillow/WIC/libtiff
  pixel reorientation and do not expose the same simple tag lifecycle. Those
  non-1 TIFF orientation semantics remain separate future work.
- Raw red:
  `pillow_c image open_tiff reads orientation one metadata` failed with
  `Expected 1, got 0`, proving native TIFF open did not attach the tag.
- Facade red:
  `Pillow Image.getexif reads TIFF orientation one metadata` failed with
  `Expected 1, got 0`, proving public `GetExif()` / `ExifOrientation()` had no
  TIFF orientation metadata.
- Native change:
  shared TIFF-header parsing was factored into `parse_tiff_orientation`, and
  `open_tiff_frame_image` now reads the original TIFF bytes after successful
  WIC pixel decode. File-read failure is returned as an explicit invalid
  argument instead of being treated as missing metadata. For frame `0`, only
  parsed Orientation value `1` is copied into `image->exif_orientation`.
- No new ABI export was added. The existing
  `pillow_c_image_exif_orientation` and facade `Image.GetExif()` object expose
  the bounded TIFF metadata.
- Release x64 rebuild after the native behavior change:
  `build\x64\Release\pillow_c.dll` was rebuilt successfully with
  `0 Warning(s), 0 Error(s)`.
- Targeted raw green:
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 63ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw TIFF regression filter:
  `Ran 6 tests in 203ms; Passed: 6, Failed: 0, Errors: 0, Skipped: 0`.
- Facade TIFF regression filter:
  `Ran 5 tests in 484ms; Passed: 5, Failed: 0, Errors: 0, Skipped: 0`.
- Current full AHK directory suite:
  `Ran 1057 tests in 70656ms; Passed: 1057, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Covered oracle and implementation evidence for `MODE-NUM-001C`:

- Local Pillow 11.3.0 `I -> L` fixture values
  `[-300, -1, 0, 1, 127, 128, 255, 256, 1000, 2147483647, -2147483648]`
  convert to bytes `[0, 0, 0, 1, 127, 128, 255, 255, 255, 255, 0]`.
- Local Pillow 11.3.0 `F -> L` fixture values
  `[-300.5, -1.5, -0.5, 0.0, 0.49, 0.5, 1.0, 127.4, 127.5, 128.5,
  254.5, 255.0, 255.5, 1000.0, NaN, -Inf, +Inf]` convert to bytes
  `[0, 0, 0, 0, 0, 0, 1, 127, 127, 128, 254, 255, 255, 255, 0, 0, 255]`.
- Covered bounded rule: truncate numeric values toward zero,
  clamp to `0..255`, map `NaN` to `0`, `-Inf` to `0`, and `+Inf` to `255`.
  Empty `(0, 1)` images in modes `I` and `F` convert to mode `L` with empty
  bytes.
- Raw red:
  `pillow_c image convert_mode covers numeric I and F to L` failed with
  `Expected 0, got -3`, proving the native conversion path rejected the
  numeric source modes.
- Facade red:
  `Pillow Image.Convert supports numeric I and F to L through native handles`
  failed with `pillow_c: invalid argument`, proving the public facade could
  not route the numeric conversion successfully.
- Native change:
  `convert_image_mode_into` now handles source modes `I` and `F` targeting
  mode `L`; `F` maps `NaN` / infinities with the covered Pillow rule.
  `pillow_c_image_create_mode` and shared shape matching now preserve empty
  Pillow-style mode-aware shapes for this public conversion path, while the
  legacy channel-count create path remains non-empty.
- Release x64 rebuild after the native behavior change:
  `build\x64\Release\pillow_c.dll` was rebuilt successfully.
- Targeted raw green:
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 187ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw conversion regression filter:
  `Ran 8 tests in 203ms; Passed: 8, Failed: 0, Errors: 0, Skipped: 0`.
- Facade conversion regression filter:
  `Ran 13 tests in 406ms; Passed: 13, Failed: 0, Errors: 0, Skipped: 0`.
- MODE-NUM-001C-era raw full file:
  `Ran 524 tests in 32172ms; Passed: 524, Failed: 0, Errors: 0, Skipped: 0`.
- MODE-NUM-001C-era facade full file:
  `Ran 531 tests in 42109ms; Passed: 531, Failed: 0, Errors: 0, Skipped: 0`.
- MODE-NUM-001C-era full AHK directory suite:
  `Ran 1055 tests in 68860ms; Passed: 1055, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Fresh verification evidence for the `MODE-F-001A` mode `F` extrema child:

- Local Pillow 11.3.0 oracle showed
  `Image.frombytes("F", (4,1), float32_bytes).getextrema()` returns
  `(-3.75, 2.25)` for finite samples `[-1.5, 0.0, 2.25, -3.75]`.
- The same oracle showed later `NaN` samples are ignored by normal extrema
  comparisons (`[1.0, NaN, 2.0] -> (1.0, 2.0)`), a first-sample `NaN`
  preserves `NaN` for both extrema, `-Inf` / `Inf` return `(-Inf, Inf)`, and
  empty mode `F` images return `None`.
- Raw red:
  `pillow_c image get_extrema_numeric supports mode F float extrema` failed
  with `Expected 0, got -3`, proving the native numeric export still rejected
  mode `F`.
- Facade red:
  `Pillow Image.GetExtrema supports mode F float extrema` failed with
  `Expected numeric value -3.75 +/- 9.9999999999999995e-08, got Array`,
  proving the facade still returned four byte-storage bands for mode `F`.
- Release x64 rebuild after the native behavior change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `337` `pillow_c_*` exports. No new
  export was added; `pillow_c_image_get_extrema_numeric` now also covers mode
  `F`.
- Targeted raw green:
  `Ran 1 tests in 109ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 219ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- GetExtrema facade regression filter:
  `Ran 3 tests in 265ms; Passed: 3, Failed: 0, Errors: 0, Skipped: 0`.
- Raw get_extrema regression filter:
  `Ran 3 tests in 94ms; Passed: 3, Failed: 0, Errors: 0, Skipped: 0`.
- Fresh full AHK directory suite:
  `Ran 1051 tests in 70359ms; Passed: 1051, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.
  This is the completed-slice green baseline before the later
  `MODE-NUM-001B` histogram implementation.

Fresh verification evidence for the `MODE-NUM-001B` numeric histogram child:

- Local Pillow 11.3.0 oracle showed modes `I` and `F` return one
  Pillow-visible 256-bin histogram, with values scaled from image extrema.
  All-equal numeric images produce an all-zero histogram, and masked `I`/`F`
  histograms raise `ValueError: image has wrong mode`.
- Raw red:
  `pillow_c image histogram matches Pillow numeric modes` failed with
  `Expected 0, got -2`, proving the native histogram path still rejected or
  mis-sized numeric-mode output.
- Facade red:
  `Pillow Image.Histogram matches Pillow numeric modes` failed with
  `Expected 256, got 1024`, proving the facade exposed four byte-storage bands
  instead of one Pillow-visible numeric band.
- Native change:
  `pillow_c_image_histogram` now accepts a 256-bin output for modes `I` and
  `F`, reads little-endian signed int32 / float32 samples, and scales finite
  numeric extrema into bins `0..255`; `pillow_c_image_histogram_masked`
  rejects `I`/`F` with `PILLOW_C_INVALID_ARGUMENT`.
- Facade change:
  `Image.Histogram()` sizes the output as
  `Pillow.Image.GetModeBands(this.Mode) * 256`, so `I` and `F` use one
  public band rather than four storage bytes.
- Release x64 rebuild after the native behavior change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `337` `pillow_c_*` exports. No new
  export was added.
- Targeted raw green:
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 172ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw histogram regression filter:
  passed `7/7` in the implementation handoff evidence.
- Facade histogram regression filter:
  passed `11/11` in the implementation handoff evidence.
- MODE-NUM-001C-era raw full file:
  `Ran 524 tests in 32172ms; Passed: 524, Failed: 0, Errors: 0, Skipped: 0`.
- MODE-NUM-001C-era facade full file:
  `Ran 531 tests in 42109ms; Passed: 531, Failed: 0, Errors: 0, Skipped: 0`.
- MODE-NUM-001C-era full AHK directory suite:
  `Ran 1055 tests in 68860ms; Passed: 1055, Failed: 0, Errors: 0, Skipped: 0`.
  The same non-failing JPEG stderr warnings appeared.

Fresh verification evidence for the `MODE-I-001A` mode `I` extrema child:

- Local Pillow 11.3.0 oracle showed
  `Image.frombytes("I", (4,1), int32_bytes).getextrema()` returns
  `(-2147483648, 70000)` for signed samples
  `[-10, 0, 70000, -2147483648]`, while empty mode `I` crops return `None`.
- Raw red:
  `pillow_c image get_extrema_numeric supports mode I int32 extrema` failed
  with `Expected pillow_c_image_get_extrema_numeric export: Call to nonexistent function.`
- Facade red:
  `Pillow Image.GetExtrema supports mode I int32 extrema` failed with
  `Expected [-2147483648, 70000], got [[0, 246], [0, 255], [0, 255], [0, 255]]`.
- Release x64 rebuild after the native ABI change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `337` `pillow_c_*` exports. New export:
  `pillow_c_image_get_extrema_numeric`.
- Targeted raw green:
  `Ran 1 tests in 47ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 31ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- GetExtrema facade regression filter:
  `Ran 2 tests in 78ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`.
- Raw get_extrema regression filter:
  `Ran 2 tests in 47ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`.
- Fresh full AHK directory suite:
  `Ran 1049 tests in 69750ms; Passed: 1049, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Fresh verification evidence for the `FMT-TIFF-001A` bounded TIFF multipage
save child:

- Local Pillow 11.3.0 oracle for two `L` frames showed a little-endian TIFF
  header `[73, 73, 42, 0]`, `n_frames == 2`, `is_animated == True`, frame `0`
  bytes `[10, 20]`, frame `1` bytes `[30, 40]`, and `EOFError: attempt to
  seek outside sequence` after the last frame.
- Raw red:
  `pillow_c image save_tiff_frames writes multipage L images` first failed
  because `pillow_c_image_save_tiff_frames` did not exist. During
  implementation, the WIC multipage attempt then reproduced a native
  `WINCODEC_ERR_STREAMWRITE` (`0x88982F71`) at the second frame commit, so the
  final path uses a native little-endian uncompressed TIFF IFD writer instead
  of relying on WIC for multipage save.
- Facade red:
  `Pillow Image.Save TIFF save_all writes multipage images` failed with
  `Pillow.Image.Save save_all currently supports GIF only`.
- Release x64 rebuild after the native ABI change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `336` `pillow_c_*` exports. New export:
  `pillow_c_image_save_tiff_frames`.
- Targeted raw green:
  `Ran 1 tests in 141ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 125ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- TIFF regression filter:
  `Ran 9 tests in 734ms; Passed: 9, Failed: 0, Errors: 0, Skipped: 0`.
- Fresh full AHK directory suite:
  `Ran 1047 tests in 69641ms; Passed: 1047, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Fresh verification evidence for the `BYTES-001G` readonly Resize/Thumbnail
refresh child:

- Local Pillow 11.3.0 oracle showed that
  `Image.frombuffer("L", (2,2), bytearray, "raw", "L", 4, 1)` starts with
  `readonly == 1`, reflects source bytearray mutation while attached, and
  `im.resize((1,1), Image.Resampling.NEAREST)` samples the latest caller byte
  while leaving the source image readonly. The same oracle showed
  `im.thumbnail((1,1), Image.Resampling.NEAREST)` samples the latest caller
  byte, mutates the image to size `(1, 1)`, reports `readonly == 0`, and
  ignores later source bytearray mutation.
- Raw red:
  `pillow_c image resize refreshes frombuffer readonly view` failed with
  `Expected [99], got [4]`, proving native resize sampled stale owned storage.
- Facade red:
  `Pillow Image.FromBuffer thumbnail refreshes and detaches readonly view`
  failed with `Expected [99], got [4]`, proving public `Thumbnail` inherited
  the stale native resize source.
- Release x64 rebuild after native behavior change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `341` `pillow_c_*` exports. No new
  exported function was added; the ABI behavior change is that existing resize
  source reads refresh readonly buffer views before sampling.
- Targeted raw green:
  `Ran 1 tests in 125ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 141ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw frombuffer regression filter:
  `Ran 7 tests in 407ms; Passed: 7, Failed: 0, Errors: 0, Skipped: 0`.
- Facade FromBuffer regression filter:
  `Ran 7 tests in 1250ms; Passed: 7, Failed: 0, Errors: 0, Skipped: 0`.
- Raw resize regression filter:
  `Ran 11 tests in 875ms; Passed: 11, Failed: 0, Errors: 0, Skipped: 0`.
- Facade Thumbnail regression filter:
  `Ran 3 tests in 390ms; Passed: 3, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory invocation:
  `TIMEOUT after 120s; killed AutoHotkey process tree rooted at PID 66952`.
  The report had only `PASS` lines through the last recorded JPEG qtables
  case before the timeout cutoff.
- Split complete facade file:
  `Ran 575 tests in 106563ms; Passed: 575, Failed: 0, Errors: 0, Skipped: 0`.
- Split complete raw DLL file:
  `Ran 566 tests in 37250ms; Passed: 566, Failed: 0, Errors: 0, Skipped: 0`.
  The split runs reported only the known non-failing libjpeg stderr warnings.

Fresh verification evidence for the `BYTES-001F` readonly Paste detach child:

- Local Pillow 11.3.0 oracle showed that
  `Image.frombuffer("L", (2,2), bytearray, "raw", "L", 4, 1)` starts with
  `readonly == 1`, reflects source bytearray mutation while attached, and
  after either `im.paste(7, (0,0,1,1))` or `im.paste(Image.new("L",
  (1,1), 7), (0,0))` reports `readonly == 0`, preserves the pasted pixel, and
  ignores later source bytearray mutation.
- Raw red:
  `pillow_c image paste detaches frombuffer readonly view` failed with
  `Expected 0, got 1`, proving native paste left the readonly view attached.
- Facade red:
  `Pillow Image.FromBuffer detaches readonly view on Paste write` failed with
  `Expected 0, got 1`, proving public `Image.Paste` writes kept the attached
  source view.
- Release x64 rebuild after native behavior change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `341` `pillow_c_*` exports. No new
  exported function was added; the ABI behavior change is that existing paste
  mutator exports detach readonly buffer views before writing.
- Targeted raw green:
  `Ran 1 tests in 109ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 140ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw frombuffer regression filter:
  `Ran 6 tests in 234ms; Passed: 6, Failed: 0, Errors: 0, Skipped: 0`.
- Facade FromBuffer regression filter:
  `Ran 6 tests in 703ms; Passed: 6, Failed: 0, Errors: 0, Skipped: 0`.
- Raw paste regression filter:
  `Ran 13 tests in 438ms; Passed: 13, Failed: 0, Errors: 0, Skipped: 0`.
- Facade Paste regression filter:
  `Ran 10 tests in 578ms; Passed: 10, Failed: 0, Errors: 0, Skipped: 0`.
- Fresh full AHK directory suite:
  `Ran 1139 tests in 78578ms; Passed: 1139, Failed: 0, Errors: 0, Skipped: 0`.
  The same four non-failing libjpeg stderr warnings appeared.

Fresh verification evidence for the `BYTES-001E` readonly ImageDraw detach
child:

- Local Pillow 11.3.0 oracle showed that
  `Image.frombuffer("L", (2,2), bytearray, "raw", "L", 4, 1)` starts with
  `readonly == 1`, reflects source bytearray mutation while attached, and
  after `ImageDraw.Draw(im).rectangle((0,0,0,0), fill=7)` reports
  `readonly == 0`, preserves the drawn pixel, and ignores later source
  bytearray mutation.
- Raw red:
  `pillow_c image draw_rectangle detaches frombuffer readonly view` failed
  with `Expected 0, got 1`, proving the native draw path wrote without
  clearing the readonly view.
- Facade red:
  `Pillow Image.FromBuffer detaches readonly view on ImageDraw write` failed
  with `Expected 0, got 1`, proving public ImageDraw writes kept the attached
  source view.
- Release x64 rebuild after native behavior change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `341` `pillow_c_*` exports. No new
  exported function was added; the ABI behavior change is that existing
  ImageDraw mutator exports detach readonly buffer views before writing.
- Targeted raw green:
  `Ran 1 tests in 93ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 187ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw frombuffer regression filter:
  `Ran 4 tests in 250ms; Passed: 4, Failed: 0, Errors: 0, Skipped: 0`.
- Facade FromBuffer regression filter:
  `Ran 5 tests in 1047ms; Passed: 5, Failed: 0, Errors: 0, Skipped: 0`.
- Raw draw regression filter:
  `Ran 32 tests in 1782ms; Passed: 32, Failed: 0, Errors: 0, Skipped: 0`.
- Facade ImageDraw regression filter:
  `Ran 39 tests in 6453ms; Passed: 39, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory suite with `-TimeoutSeconds 120` timed out at the
  120-second budget. Split full-file verification passed `573/573` facade
  tests in `104078ms` and `564/564` raw DLL tests in `37407ms`; the same
  non-failing libjpeg stderr warnings appeared.

Fresh verification evidence for the `BYTES-001D` bounded direct RGB/RGBA
frombuffer child:

- Local Pillow 11.3.0 oracle showed that
  `Image.frombuffer("RGB", (2,2), bytearray, "raw", "RGB", 0, 1)` returns
  mode `RGB`, starts `readonly == 0`, copies source bytes, ignores later
  source bytearray mutation, and remains detached after `putpixel`.
- The same oracle showed that
  `Image.frombuffer("RGBA", (2,2), bytearray, "raw", "RGBA", 10, -1)` returns
  mode `RGBA`, starts `readonly == 1`, reads rows with the requested
  stride/orientation, reflects source bytearray mutation while readonly, and
  detaches after the first `putpixel`.
- Raw red:
  `pillow_c image frombuffer raw RGB copies while RGBA stride aliases` failed
  with `Expected 0, got 1`, proving the native raw path was aliasing direct
  `RGB` when the raw caller passed `alias_source=1`.
- Release x64 rebuild after native behavior change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `335` `pillow_c_*` exports. No new
  exported function was added; the ABI behavior change is the stricter
  `alias_source` policy for `pillow_c_image_frombuffer_raw`.
- Targeted raw green:
  `Ran 1 tests in 93ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 187ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Facade FromBuffer regression filter:
  `Ran 4 tests in 453ms; Passed: 4, Failed: 0, Errors: 0, Skipped: 0`.
- Raw frombuffer regression filter:
  `Ran 4 tests in 125ms; Passed: 4, Failed: 0, Errors: 0, Skipped: 0`.
- Raw bytes regression filter:
  `Ran 10 tests in 219ms; Passed: 10, Failed: 0, Errors: 0, Skipped: 0`.
- Fresh full AHK directory suite:
  `Ran 1045 tests in 69187ms; Passed: 1045, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Fresh verification evidence for the `BYTES-001C` bounded `RGBX` frombuffer
mapmode child:

- Local Pillow 11.3.0 oracle showed that
  `Image.frombuffer("RGB", (2,2), bytearray, "raw", "RGBX", 10, -1)` returns
  mode `RGBX`, bands `("R", "G", "B", "X")`, starts `readonly == 1`, reads
  rows with the requested stride/orientation, reflects source bytearray
  mutation while readonly, and detaches after the first `putpixel`.
- Raw red:
  `pillow_c image frombuffer raw RGBX map mode aliases source until write`
  failed before mode id `10`/raw `RGBX` support with `Expected 10, got 3`.
- Facade red:
  `Pillow Image.FromBuffer raw RGBX map mode aliases source until write`
  failed before facade/native routing with expected mode `RGBX`, got `RGB`.
- Release x64 rebuild after native behavior change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `335` `pillow_c_*` exports. No new
  exported function was added; the ABI change is mode id `10` plus expanded
  raw/frombuffer semantics.
- Targeted raw green:
  `Ran 1 tests in 31ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 125ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- FromBuffer regression filter from the implementation evidence:
  `Ran 3 tests in 250ms; Passed: 3, Failed: 0, Errors: 0, Skipped: 0`.
- Raw frombuffer regression filter from the implementation evidence:
  `Ran 3 tests in 78ms; Passed: 3, Failed: 0, Errors: 0, Skipped: 0`.
- Raw bytes regression filter from the implementation evidence:
  `Ran 10 tests in 297ms; Passed: 10, Failed: 0, Errors: 0, Skipped: 0`.
- Fresh full AHK directory suite:
  `Ran 1043 tests in 68797ms; Passed: 1043, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Fresh verification evidence for the `BYTES-001B` bounded frombuffer mapmode
child:

- Local Pillow 11.3.0 oracle showed that raw mapmodes override the requested
  mode for this constructor: `Image.frombuffer("RGB", size, bytearray, "raw",
  "L", 0, 1)` returns mode `L`, starts `readonly == 1`, and reflects source
  bytearray mutation; the analogous `"RGBA"` raw mapmode returns mode `RGBA`,
  starts `readonly == 1`, and aliases the source buffer. The oracle also
  showed plain `RGB`/`RGB` still copies with `readonly == 0`; the `RGBX`
  mapmode decision was split out and is now covered by `BYTES-001C`.
- Raw red:
  `pillow_c image frombuffer raw map modes override requested mode` failed
  with `Expected 0, got -3`.
- Facade red:
  `Pillow Image.FromBuffer raw map modes override requested mode` errored
  with `pillow_c: invalid argument`.
- Release x64 rebuild after native behavior change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `335` `pillow_c_*` exports.
- Targeted raw green:
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 141ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- FromBuffer regression filter:
  `Ran 2 tests in 250ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`.
- Raw frombuffer regression filter:
  `Ran 2 tests in 78ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`.
- Raw bytes regression filter:
  `Ran 10 tests in 297ms; Passed: 10, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory suite:
  `Ran 1041 tests in 67969ms; Passed: 1041, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Fresh verification evidence for the `BYTES-001A` bounded frombuffer child:

- Local Pillow 11.3.0 oracle for `Image.frombuffer("L", (2,2), bytearray,
  "raw", "L", 0, 1)` showed initial `readonly == 1`, source bytearray
  mutation reflected in `tobytes()`, first `putpixel` changed `readonly` to
  `0`, and later source mutation no longer changed the image.
- Raw red:
  `pillow_c image frombuffer raw aliases source until detach` failed because
  `pillow_c_image_frombuffer_raw` did not exist.
- Facade red:
  `Pillow Image.FromBuffer aliases L buffers until first write` failed because
  `Pillow.Image` had no `FromBuffer`.
- Release x64 rebuild after native ABI change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `335` `pillow_c_*` exports.
- Targeted raw green:
  `Ran 1 tests in 47ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- FromBytes regression filter:
  `Ran 6 tests in 453ms; Passed: 6, Failed: 0, Errors: 0, Skipped: 0`.
- Raw bytes regression filter:
  `Ran 10 tests in 297ms; Passed: 10, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory suite:
  `Ran 1039 tests in 68937ms; Passed: 1039, Failed: 0, Errors: 0, Skipped: 0`.
  The same four libjpeg stderr warnings appeared; no failures or skips.

Fresh verification evidence after closing the JPEG implicit metadata child:

- Narrow `drops opened ICC` filter:
  `Ran 2 tests in 250ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`.
- `quality keep` filter:
  `Ran 10 tests in 1234ms; Passed: 10, Failed: 0, Errors: 0, Skipped: 0`.
- JPEG filter:
  `Ran 131 tests in 12172ms; Passed: 131, Failed: 0, Errors: 0, Skipped: 0`.
  The run still emits four libjpeg stderr warnings
  `Corrupt JPEG data: 1 extraneous bytes before marker 0xda`; they did not
  cause failures.
- Full AHK directory suite:
  `Ran 1035 tests in 63641ms; Passed: 1035, Failed: 0, Errors: 0, Skipped: 0`.
  The same four JPEG stderr warnings appeared; no failures or skips.

Fresh verification evidence for the `META-001` bounded getexif child:

- Raw red:
  `pillow_c exif_orientation_bytes matches Pillow Image.Exif tobytes` failed
  because `pillow_c_exif_orientation_bytes` did not exist.
- Facade red:
  `Pillow Image.getexif object saves orientation to JPEG and PNG` failed
  because `Pillow.Image` had no `GetExif`.
- Release x64 rebuild after native ABI change:
  `Build succeeded. 0 Warning(s), 0 Error(s)`.
- DLL/source export count after rebuild: `331` `pillow_c_*` exports.
- Targeted raw green:
  `Ran 1 tests in 32ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Targeted facade green:
  `Ran 1 tests in 157ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- EXIF filter:
  `Ran 32 tests in 2859ms; Passed: 32, Failed: 0, Errors: 0, Skipped: 0`.
- PNG filter:
  `Ran 138 tests in 9781ms; Passed: 138, Failed: 0, Errors: 0, Skipped: 0`.
- JPEG filter:
  `Ran 132 tests in 12640ms; Passed: 132, Failed: 0, Errors: 0, Skipped: 0`.
  The run still emits four libjpeg stderr warnings
  `Corrupt JPEG data: 1 extraneous bytes before marker 0xda`; they did not
  cause failures.
- Full AHK directory suite:
  `Ran 1037 tests in 64797ms; Passed: 1037, Failed: 0, Errors: 0, Skipped: 0`.
  The same four JPEG stderr warnings appeared; no failures or skips.

## 2026-06-19 Full Progress Audit

This audit was requested explicitly, so it supersedes the normal "do not
restart with a broad audit" rule for this checkpoint update.
Route guidance in this section is historical; use the 2026-06-20 full direct
Pillow difference audit above for current next-gap selection.

Progress estimate after the audit:

- The old `40-45%` estimate is stale.
- The current AHK-first target is now best treated as roughly `52-55%`: native
  ownership is real across allocation, buffers, many transforms/filters/draw
  paths, core file formats, and heavy PNG/JPEG/GIF metadata surfaces, but the
  uncovered work is still large and uneven.
- A full Pillow replacement remains only `26-31%`: long-tail formats, `I`/`F`
  and color modes, full EXIF/XMP/ICC/ImageCms behavior, full FreeType/text
  stack, broad quantize algorithm parity, benchmarks, packaging, and some
  metadata preservation contracts are still outside the covered core.

Fresh verification evidence written under `.codex`:

- PNG filter:
  `Ran 137 tests in 10532ms; Passed: 137, Failed: 0, Errors: 0, Skipped: 0`.
- JPEG filter:
  `Ran 129 tests in 13203ms; Passed: 129, Failed: 0, Errors: 0, Skipped: 0`.
  The run still emits four libjpeg stderr warnings
  `Corrupt JPEG data: 1 extraneous bytes before marker 0xda`; they did not
  cause failures.
- GIF filter:
  `Ran 86 tests in 7890ms; Passed: 86, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory suite:
  `Ran 1033 tests in 69406ms; Passed: 1033, Failed: 0, Errors: 0, Skipped: 0`.
  The same four JPEG stderr warnings appeared; no failures or skips.

Detailed ledger read:

- The current `docs\pillow-gap-analysis.md` file has mostly covered leaf gap
  cards, but its `Status:` lines include umbrella rows and explanatory status
  text; do not convert those line counts directly into a completion
  percentage.
- Material active non-covered areas remain: broader GIF animation quantization
  and palette-stability, bounded JPEG codec/keep/metadata strategy, TIFF
  multipage/tags/modes, ICO/CUR multi-entry and cursor behavior, WebP/AVIF and
  long-tail formats, `I`/`F`/long-tail color modes, constructor/raw-buffer
  ownership, full quantize algorithms, metadata/color-management lifecycle,
  FreeType/text shaping, edge-case operation/mode matrices, benchmarks, ABI
  hardening, testing strategy, and packaging.
- PNG is now a cautionary area for speed: after `FMT-PNG-004A` introduced the
  generalized native metadata route, future PNG work should add a new narrow
  branch only for a genuinely new native capability or ABI shape. Combination
  coverage should be batched through `pillow_c_image_save_png_metadata_options`
  and adjacent facade normalization instead of repeating `FMT-PNG-001AA`-style
  one-combination branches.

Speed guidance:

- Highest near-term ROI is not another broad audit. Pick one bounded gap and
  prove it with the local Pillow 11.3.0 oracle only when semantics are unclear.
- Prefer GIF only when a concrete palette-stability/pathological animation
  fixture goes red. The bounded opened-JPEG implicit metadata child is now
  covered, so the best default next packet is a semantic pillar:
  `getexif()` for compatibility or `frombuffer` for performance interop.
- Continue JPEG only when a new local Pillow oracle exposes a keep/metadata,
  marker-preservation, or codec-strategy miss beyond the covered COM-preserved,
  ICC/EXIF-not-implicit rule.
- Defer TIFF, new formats, benchmarks, and font/CMS expansion unless the user
  explicitly redirects priority there.
- Keep each increment native-first, with raw DLL tests, facade tests for public
  API, docs, and a Release x64 rebuild only when native code or project files
  change.

## Historical Latest-Slice Evidence

- Latest targeted GIF verification after `FMT-GIF-003B10`:
  local Pillow 11.3.0 probe for two exact RGBA `2x1` animation frames, source
  pixels `[(black, opaque), (red, opaque)]` then `[(red, opaque),
  (1,2,3, transparent)]`, and save options `transparency=2`,
  `duration=[10,20]`, `loop=0`, and `disposal=[0,0]` showed that Pillow accepts
  the save. It writes no first-frame transparency GCE, writes the second frame
  full-width with local palette head `[1,2,3, 255,0,0]`, writes second-frame
  GCE transparency index `2`, reopens duration metadata `10` and `20` with loop
  `0`, reopens frame 1 as black/red RGBA, and reopens frame 2 as
  red/opaque-`1,2,3` RGBA. Native `save_gif_animation_image` now permits caller
  transparency with RGBA animation input, keeps caller transparency distinct
  from the automatic source-alpha index `0`, and reserves out-of-source-palette
  transparency slots during optimized local-palette compaction. The facade
  already routed `Image.Save(..., "GIF", {save_all: true, append_images: [...],
  transparency: 2})` through the GIF animation metadata export, so no facade
  routing change was needed. Release x64 rebuild succeeded with `0 Warning(s),
  0 Error(s)`, and dumpbin reports `330` `pillow_c_*` exports. Raw red failed
  `Expected 0, got -3`; facade red failed `pillow_c: invalid argument`.
  Targeted green passed `2/2`; raw `save_gif_animation` filter passed `28/28`;
  facade `save_all` filter passed `28/28`; broader `GIF` filter passed `86/86`;
  full AHK directory suite passed
  `Ran 1033 tests in 70000ms; Passed: 1033, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after the `FMT-JPEG-003C` opened JPEG
  keep-save implicit metadata rule coverage:
  local Pillow 11.3.0 probes showed that an opened RGB JPEG with COM, ICC, and
  EXIF metadata implicitly carries `info["comment"]` through a keep-style save,
  but does not implicitly carry opened ICC or EXIF unless the caller passes
  those bytes explicitly. Existing native/facade behavior already matched:
  non-explicit keep-style JPEG routes patch the stored COM payload back into
  the encoded output, while ICC and EXIF stay caller-controlled. No native
  source changed, no Release x64 rebuild was required, and dumpbin still
  reports `330` `pillow_c_*` exports. The added raw/facade coverage tests
  passed immediately. Targeted `drops opened ICC` filter passed `2/2`;
  `quality keep` filter passed `10/10`; broader `JPEG` filter passed
  `131/131`; full AHK directory suite passed
  `Ran 1035 tests in 63641ms; Passed: 1035, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after the `FMT-JPEG-003C` RGB progressive
  `quality="keep"` preservation slice:
  local Pillow 11.3.0 probe for an opened RGB `16x8` JPEG saved with
  `quality=70`, `subsampling=2`, and a COM comment showed that re-saving with
  `quality="keep", progressive=True` preserves the opened comment, preserves
  both source DQT tables exactly, changes SOF0 to SOF2, preserves 4:2:0
  sampling `[1,2,2,0, 2,1,1,1, 3,1,1,1]`, writes ten RGB progressive SOS
  scans, and reopens as RGB with `comment`, `progressive`, and `progression`
  info keys. The existing native qtables encoder and facade keep route already
  composed this behavior: opened handles supply stored DQT tables plus stored
  subsampling with `quality == -1`, the caller's progressive flag reaches
  `pillow_c_image_save_jpeg_qtables_encode_options`, and the opened COM
  payload is patched back by the non-explicit-metadata save path. No native
  source changed, no Release x64 rebuild was required, and dumpbin still
  reports `330` `pillow_c_*` exports. The added raw/facade tests passed
  immediately. Targeted `quality keep` filter passed `9/9`; broader `JPEG`
  filter passed `129/129`; combined full-directory runs with and without
  `-Quiet` both hit the runner's `120s` timeout, so the recorded full evidence
  for this slice is the file-level facade run `519/519` and raw run `512/512`.
- Latest targeted JPEG verification after the `FMT-JPEG-003C` opened JPEG
  comment keep-preservation slice:
  local Pillow 11.3.0 probes showed that an opened RGB JPEG with a COM segment
  implicitly carries `info["comment"]` through a keep-style save, including
  `quality="keep"` / `qtables="keep"`, while ICC and EXIF are not implicitly
  carried unless passed explicitly. Native JPEG open already stores COM bytes in
  `image->jpeg_comment`; non-explicit-metadata JPEG save routes now patch those
  stored comment bytes back into the encoded JPEG after successful output, while
  explicit metadata routes remain caller-controlled so explicit empty/missing
  comment can still drop the opened comment like Pillow. Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`, and dumpbin reports `330`
  `pillow_c_*` exports. Targeted red failed because the saved JPEG had no COM
  segment. Targeted green passed `2/2`; broader `JPEG` filter passed
  `127/127`; facade full-file run passed `518/518`; raw full-file run passed
  `511/511`; the combined full directory non-quiet runner hit the known
  status-file issue, so the two full file-level runs are the current recorded
  full-suite evidence for this slice.
- Latest targeted JPEG verification after the `FMT-JPEG-003C` L
  `quality="keep"` DQT preservation slice:
  local Pillow 11.3.0 probes for a bounded mode `L` `16x8` JPEG saved with
  `quality=70` showed that reopening and re-saving with `quality="keep"`
  preserves the one source DQT table exactly, keeps SOF0 sampling
  `[1,1,1,0]`, writes one baseline luminance SOS, and reopens as mode `L`.
  The existing native qtables encoder already supported `quality=-1` plus one
  stored luminance qtable. The facade now accepts bounded opened mode `L` JPEG
  `quality: "keep"` / `qtables: "keep"` saves, pulls native qtables, and
  routes through the existing native qtables encoder without AHK pixel loops.
  No native source changed in this slice, so no Release x64 rebuild was
  required and the DLL export count remains `330`. Targeted red first exposed
  an incorrect test SOS expectation, then the corrected raw test passed while
  the facade errored on the old RGB/CMYK-only keep guard. Targeted green passed
  `2/2`; broader `JPEG` filter passed `125/125`; facade full-file run passed
  `517/517`; raw full-file run passed `510/510`; combined full directory run
  with `-Quiet -TimeoutSeconds 120` exited `0` and wrote status `0`.
- Latest targeted JPEG verification after the `FMT-JPEG-003C` RGB
  `quality="keep"` qtables/subsampling preservation slice:
  local Pillow 11.3.0 probes for bounded RGB `16x8` images saved with
  `quality=70` and `subsampling=0`, `1`, or `2` showed that reopening and
  re-saving with `quality="keep"` preserves the original two DQT tables
  exactly and preserves SOF0 sampling: `subsampling=0` writes Y/Cb/Cr all
  `1x1`, `subsampling=1` writes Y `2x1` with Cb/Cr `1x1`, and
  `subsampling=2` writes Y `2x2` with Cb/Cr `1x1`. The implemented coverage
  batches the high-value `1` and `2` cases. Native JPEG open now parses SOF
  sampling into handle metadata and exposes it through the new
  `pillow_c_image_metadata_jpeg_subsampling` ABI export. The facade accepts
  bounded opened RGB JPEG `quality: "keep"` / `qtables: "keep"` saves, pulls
  native qtables plus stored subsampling, and routes through the existing
  qtables encoder without AHK pixel loops. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`, and dumpbin reports `330` `pillow_c_*` exports.
  Raw targeted red failed on the missing subsampling metadata export; facade
  targeted red failed on the old CMYK-only keep guard. Targeted raw/facade
  filter passed `2/2`; broader `JPEG` filter passed `123/123`; full AHK
  directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 1025 tests in 68203ms; Passed: 1025, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after the `FMT-JPEG-003C` real YCCK
  quality keep save-normalization slice:
  local Pillow 11.3.0 source and probe confirmed Pillow does not encode true
  YCCK JPEGs on save. Opening the project-owned real YCCK fixture
  `ahk\fixtures\pil_sample_cmyk_ycck.jpg` reports mode `CMYK`, size
  `(100,100)`, `info["adobe"] == 100`, `info["adobe_transform"] == 2`, and an
  APP14 Adobe transform byte `2`. Re-saving that opened image with
  `quality="keep"` succeeds, normalizes the output APP14 transform to `0`,
  writes SOF0 components `C/M/Y/K` with qtable selectors `[0,1,1,1]` and
  `1x1` sampling, preserves the two original DQT tables exactly, reopens as
  mode `CMYK`, and starts with bytes
  `[0,242,230,0,0,242,230,0,0,242,230,0,0,242,230,0]`.
  Native JPEG open now parses DQT segments into handle metadata and exposes
  them through new `pillow_c_image_metadata_jpeg_qtable_count` and
  `pillow_c_image_metadata_jpeg_qtable` ABI exports. The native qtable JPEG
  save route treats `quality=-1` with supplied custom qtables as exact table
  preservation, and the facade accepts bounded `quality: "keep"` /
  `qtables: "keep"` for opened CMYK JPEG images without AHK pixel loops.
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`, and dumpbin
  reports `329` `pillow_c_*` exports. Raw targeted red first failed on the
  missing qtable metadata export; facade targeted red failed with
  `Pillow.Image.Save quality must be an integer`. Raw targeted green passed
  `1/1`; facade targeted green passed `1/1`; broader `JPEG` filter passed
  `121/121`; full AHK directory run with `-TimeoutSeconds 120` exited `0` and
  recorded `Ran 1023 tests in 87437ms; Passed: 1023, Failed: 0, Errors: 0,
  Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-001A`:
  local Pillow 11.3.0 probe for a bounded RGB `1x1` JPEG saved with a
  70,000-byte ICC profile, `comment=b"Large ICC"`, and `quality=75` showed
  that Pillow writes two APP2 `ICC_PROFILE` segments: segment `1/2` has
  length field `65535`, payload length `65533`, and `65519` ICC data bytes;
  segment `2/2` has length field `4497`, payload length `4495`, and `4481`
  ICC data bytes. Reopen restores `info["icc_profile"]` exactly and preserves
  the comment. Native JPEG metadata save now splits ICC payloads into APP2
  chunks of at most `65519` profile bytes with Pillow-compatible sequence/count
  headers, and native JPEG open reassembles complete same-count ICC APP2
  sequences before exposing `pillow_c_image_metadata_jpeg_icc_profile`. The
  facade routes public `Image.Save(..., "JPEG", {icc_profile, comment})` to
  the DLL and maps reopened large ICC bytes to `Info["icc_profile"]` without
  AHK pixel loops. No ABI symbol was added; dumpbin still reports `327`
  `pillow_c_*` exports. Release x64 rebuild succeeded with `0 Warning(s), 0
  Error(s)`. Raw red failed `Expected 0, got -2`; facade red failed
  `pillow_c: invalid length`; raw targeted green passed `1/1`; facade
  targeted green passed `1/1`; broader `JPEG` filter passed
  `Ran 119 tests in 11719ms; Passed: 119, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 1021 tests in 68875ms; Passed: 1021, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted GIF verification after `FMT-GIF-003B9`:
  local Pillow 11.3.0 probe for same-size `LA` animation frames with raw bytes
  `[0,255,128,255,255,255]` and `[255,255,64,0,0,255]`,
  `duration=[10,20]`, `loop=0`, and `disposal=[0,0]` showed that Pillow
  accepts the save, writes two full-width frames, reopens duration metadata
  `10` and `20` with loop `0`, and preserves luminance bytes on
  `convert("L")` as `[0,128,255]` then `[255,64,0]`. Reopened
  `convert("LA")` reports alpha `255` for the transparent source sample in
  this fixture, so the covered GIF save semantics ignore LA alpha. Native
  `save_gif_animation_image` now accepts bounded non-P `LA` animation input,
  exact-quantizes each frame into a temporary DLL-owned `P` image with the new
  luminance-only `quantize_exact_la_gif_animation_frame_into` helper, and then
  runs the existing GIF animation differencing/writer path. The public facade
  routes `Image.Save(..., "GIF", {save_all: true, append_images: [...]})` for
  `LA` frames through the DLL without an AHK pixel loop. Exact Pillow GIF byte
  layout, mixed `L`/`LA`/RGB/RGBA animation matrices, and broader LA alpha
  semantics remain future GIF quantization surfaces. No ABI symbol was added;
  dumpbin still reports `327` `pillow_c_*` exports. Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`. Raw red failed `Expected 0, got
  -3`; facade red failed `pillow_c: invalid argument`; raw targeted green
  passed `1/1`; facade targeted green passed `1/1`; raw
  `save_gif_animation` filter passed
  `Ran 27 tests in 1094ms; Passed: 27, Failed: 0, Errors: 0, Skipped: 0`;
  facade `save_all` filter passed
  `Ran 27 tests in 3687ms; Passed: 27, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 1019 tests in 69672ms; Passed: 1019, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003AH`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `dpi=(300,150)`, JPEG metadata (`comment=b"Hello CMYK"`,
  eight-byte ICC, EXIF orientation `6`), real baseline `subsampling=1` or
  `2`, no `optimize`, no `progressive`, and no `keep_rgb` showed that
  non-optimized DPI CMYK qtables metadata saves with real subsampling are
  accepted. Pillow writes APP0/JFIF density `300x150` before APP14 transform
  `0`, writes APP1 EXIF, APP2 ICC, and COM before two custom DQT segments,
  SOF0 with `C` sampling `2x1` or `2x2` and `M/Y/K` sampling `1x1`, standard
  DHTs `[29,179]`, and one SOS, then reopens as mode `CMYK` with sampled
  bytes `[0,42,79,148,10,42,79,148]` while preserving DPI/JFIF and JPEG
  metadata. The existing native
  `pillow_c_image_save_jpeg_qtables_metadata_encode_options` route already
  produced this bounded sampled DPI metadata output; `ahk/pillow.ahk` now
  routes public `Image.Save(..., "JPEG", {dpi, qtables, subsampling: 1|2,
  comment, icc_profile, exif})` to the DLL without an AHK pixel loop. Broader
  YCCK save behavior, multi-segment ICC, and exact libjpeg entropy byte
  parity remain explicit future gaps. No ABI symbol was added; dumpbin still
  reports `327` `pillow_c_*` exports. No native source changed in this slice,
  so no Release x64 rebuild was required. The raw expanded targeted test
  passed before implementation (`1/1`); facade red failed on the old
  sampled-CMYK guard. Facade targeted green passed `1/1`; raw targeted green
  passed `1/1`; broader JPEG filter passed
  `Ran 117 tests in 12844ms; Passed: 117, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 1015 tests in 68359ms; Passed: 1015, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003AG`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `optimize=True`, JPEG metadata (`comment=b"Hello CMYK"`,
  eight-byte ICC, EXIF orientation `6`), real baseline `subsampling=1` or
  `2`, and optional `dpi=(300,150)` showed that optimized non-keep-rgb CMYK
  qtables metadata saves with real subsampling are accepted. Pillow omits
  APP0/JFIF without DPI, writes APP0/JFIF density `300x150` before APP14 when
  DPI is supplied, writes APP14 transform `0`, APP1 EXIF, APP2 ICC, and COM
  before two custom DQT segments, SOF0 with `C` sampling `2x1` or `2x2` and
  `M/Y/K` sampling `1x1`, optimized DHTs, and one SOS, then reopens as mode
  `CMYK` with sampled bytes `[0,42,79,148,10,42,79,148]` while preserving
  optional DPI/JFIF and JPEG metadata. The existing native
  `pillow_c_image_save_jpeg_qtables_metadata_encode_options` route already
  produced this bounded sampled optimized metadata output; `ahk/pillow.ahk`
  now routes public `Image.Save(..., "JPEG", {qtables, optimize: true,
  subsampling: 1|2, comment, icc_profile, exif, dpi?})` to the DLL without an
  AHK pixel loop. Broader YCCK save behavior, multi-segment ICC, and exact
  libjpeg entropy byte parity remain explicit future gaps. No ABI symbol was
  added; dumpbin still reports `327` `pillow_c_*` exports. No native source
  changed in this slice, so no Release x64 rebuild was required. Raw coverage
  was already green for the selected export (`2/2` with the adjacent keep-rgb
  optimized filter); facade red failed on the old sampled-CMYK guard. Facade
  targeted green passed `1/1`; raw targeted green passed `2/2`; broader JPEG
  filter passed
  `Ran 117 tests in 11984ms; Passed: 117, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 1015 tests in 68437ms; Passed: 1015, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003AF`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `dpi=(300,150)`, `progressive=True`, and real
  `subsampling=1` or `2`, with direct output, `keep_rgb=True`, and optional
  JPEG metadata (`comment=b"Hello CMYK"`, eight-byte ICC, EXIF orientation
  `6`), showed that DPI sampled progressive CMYK qtables saves are accepted.
  Pillow writes APP0/JFIF density `300x150` before APP14 transform `0`,
  optional APP1 EXIF, APP2 ICC, and COM before two custom DQT segments, SOF2
  with `C` sampling `2x1` or `2x2` and `M/Y/K` sampling `1x1`, and 18 CMYK
  progressive scans, then reopens as mode `CMYK` with sampled bytes
  `[0,42,79,148,10,42,79,148]` while preserving DPI/JFIF and optional
  metadata. Native `pillow_c_image_save_jpeg_qtables_encode_options`,
  `pillow_c_image_save_jpeg_qtables_metadata_encode_options`, and the covered
  keep-rgb aliases now allow this bounded DPI sampled progressive route
  through the existing native CMYK progressive qtables encoder and metadata
  patcher; the facade routes public
  `Image.Save(..., "JPEG", {dpi, qtables, progressive: true,
  subsampling: 1|2, ...})` without AHK pixel loops. Optimized non-keep-rgb
  metadata sampled qtables, broader YCCK save behavior, multi-segment ICC,
  and exact libjpeg entropy byte parity remain explicit future gaps. No ABI
  symbol was added; dumpbin still reports `327` `pillow_c_*` exports. Release
  x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`. Raw reds failed
  `Expected 0, got -3`; facade red failed on the old sampled-CMYK guard. Raw
  targeted green passed `2/2`; facade targeted green passed `1/1`; broader
  JPEG filter passed
  `Ran 115 tests in 11671ms; Passed: 115, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 1013 tests in 68734ms; Passed: 1013, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003AE`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, no `keep_rgb`, `progressive=True`, no DPI, and real
  `subsampling=1` or `2`, with and without JPEG metadata (`comment=b"Hello
  CMYK"`, eight-byte ICC, EXIF orientation `6`), showed that direct sampled
  progressive CMYK qtables saves are accepted. Pillow omits APP0/JFIF, writes
  APP14 transform `0`, optional APP1 EXIF, APP2 ICC, and COM before two custom
  DQT segments, writes SOF2 with `C` sampling `2x1` or `2x2` and `M/Y/K`
  sampling `1x1`, writes 18 CMYK progressive scans, and reopens as mode `CMYK`
  with bytes `[0,42,79,148,10,42,79,148]` while preserving optional comment,
  ICC, and EXIF orientation. Native
  `pillow_c_image_save_jpeg_qtables_encode_options` and
  `pillow_c_image_save_jpeg_qtables_metadata_encode_options` now allow this
  bounded no-DPI sampled progressive route through the existing native CMYK
  progressive qtables encoder and metadata patcher; the facade routes public
  `Image.Save(..., "JPEG", {qtables, progressive: true, subsampling: 1|2,
  ...})` without AHK pixel loops. At that point, DPI sampled progressive
  qtables, optimized non-keep-rgb metadata sampled qtables, broader YCCK save
  behavior, multi-segment ICC, and exact libjpeg entropy byte parity remained
  explicit future gaps. No ABI symbol was added; dumpbin still reports `327`
  `pillow_c_*` exports. Release x64 rebuild succeeded with `0 Warning(s), 0
  Error(s)`. Raw reds failed `Expected 0, got -3`; facade red failed on the
  old sampled-CMYK guard. Raw targeted green passed `2/2`; facade targeted
  green passed `1/1`; broader JPEG filter passed
  `Ran 112 tests in 11063ms; Passed: 112, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 1010 tests in 66703ms; Passed: 1010, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003AD`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `keep_rgb=True`, `progressive=True`, no DPI, and real
  `subsampling=1` or `2`, with and without JPEG metadata (`comment=b"Hello
  CMYK"`, eight-byte ICC, EXIF orientation `6`), showed that sampled
  progressive CMYK qtables keep-rgb saves are accepted. Pillow omits
  APP0/JFIF, writes APP14 transform `0`, optional APP1 EXIF, APP2 ICC, and
  COM before two custom DQT segments, writes SOF2 with `C` sampling `2x1` or
  `2x2` and `M/Y/K` sampling `1x1`, writes the covered 18-scan CMYK
  progressive script, and reopens as mode `CMYK` with bytes
  `[0,42,79,148,10,42,79,148]` while preserving optional comment, ICC, and
  EXIF orientation. Native
  `pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options` and
  `pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options` now
  allow this bounded no-DPI sampled progressive keep-rgb alias and route it
  through the native sampled CMYK progressive qtables encoder; the facade
  routes public `Image.Save(..., "JPEG", {qtables, keep_rgb: true,
  progressive: true, subsampling: 1|2, ...})` without AHK pixel loops.
  At that point, DPI sampled progressive qtables, non-keep-rgb sampled
  progressive qtables, optimized non-keep-rgb metadata sampled qtables,
  broader YCCK save behavior, multi-segment ICC, and exact libjpeg entropy byte
  parity remained explicit future gaps. No ABI symbol was added; dumpbin still reports `327`
  `pillow_c_*` exports. Release x64 rebuild succeeded with `0 Warning(s), 0
  Error(s)`. Raw reds failed `Expected 0, got -3`; facade red failed on the
  old sampled-CMYK guard. Raw targeted green passed `2/2`; facade targeted
  green passed `1/1`; broader JPEG filter passed
  `Ran 109 tests in 10234ms; Passed: 109, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 1007 tests in 67859ms; Passed: 1007, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003AC`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, JPEG metadata (`comment=b"Hello CMYK"`, eight-byte ICC, EXIF
  orientation `6`), no DPI, and real baseline `subsampling=1` or `2` without
  `keep_rgb` showed that non-keep-rgb CMYK qtables metadata subsampling saves
  are accepted. Pillow omits APP0/JFIF, writes APP14 transform `0`, APP1 EXIF,
  APP2 ICC, COM, two DQT segments, SOF0 with `C` sampling `2x1` or `2x2` and
  `M/Y/K` sampling `1x1`, standard DHT payload lengths `[29,179]`, and one
  SOS, then reopens as mode `CMYK` with bytes
  `[0,42,79,148,10,42,79,148]` while preserving comment, ICC, and EXIF
  orientation. The existing native
  `pillow_c_image_save_jpeg_qtables_metadata_encode_options` route already
  produced this sampled CMYK qtables metadata output; `ahk/pillow.ahk` now
  allows public `Image.Save(..., "JPEG", {qtables, subsampling: 1|2, comment,
  icc_profile, exif})` with no DPI to reach that DLL path without AHK pixel loops while
  still rejecting progressive sampled qtables and uncovered optimized
  non-keep-rgb metadata subsampling. No ABI symbol was added; dumpbin still
  reports `327` `pillow_c_*` exports. No native source changed in this slice,
  so no Release x64 rebuild was required. Raw targeted coverage passed before
  and after the facade change (`1/1`); the facade red failed on the old
  sampled-CMYK guard; facade targeted green passed `1/1`; broader JPEG filter
  passed
  `Ran 106 tests in 10641ms; Passed: 106, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 1004 tests in 68078ms; Passed: 1004, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003AB`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `keep_rgb=True`, `optimize=True`, and real baseline
  `subsampling=1` or `2`, both without metadata and with JPEG metadata
  (`comment=b"Hello CMYK"`, eight-byte ICC, EXIF orientation `6`), showed
  that optimized sampled CMYK qtables keep-rgb saves are accepted as CMYK
  encoder aliases. Pillow omits APP0/JFIF, writes APP14 transform `0`, writes
  optional APP1 EXIF, APP2 ICC, and COM before two custom DQT segments, writes
  SOF0 with `C` sampling `2x1` for `subsampling=1` or `2x2` for
  `subsampling=2` and `M/Y/K` sampling `1x1`, writes compact optimized DHT
  payloads, one SOS, and reopens as mode `CMYK` with bytes
  `[0,42,79,148,10,42,79,148]` within tolerance while preserving optional
  comment, ICC, and EXIF orientation. Native
  `pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options` and
  `pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options` now
  allow the bounded optimized sampled CMYK qtables keep-rgb alias and route
  through the existing sampled CMYK qtables encoder plus metadata patcher where
  needed, while the facade routes public
  `Image.Save(..., "JPEG", {qtables, keep_rgb: true, optimize: true,
  subsampling: 1|2, ...})` without AHK pixel loops. Progressive sampled
  keep-rgb qtables, non-keep-rgb qtables metadata with real subsampling, and
  broader YCCK save behavior remain explicit future gaps. No ABI symbol was
  added; dumpbin still reports `327` `pillow_c_*` exports. Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`. Raw reds failed
  `Expected 0, got -3`; facade reds failed on the old sampled qtables guard.
  Raw targeted greens passed `1/1` for the non-metadata case and `1/1` for the
  metadata case; facade targeted greens passed `1/1` for the non-metadata case
  and `1/1` for the metadata case; broader JPEG filter passed
  `Ran 104 tests in 10531ms; Passed: 104, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 1002 tests in 65328ms; Passed: 1002, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003AA`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `keep_rgb=True`, `dpi=(300,150)`, and real baseline
  `subsampling=1` or `2`, both without metadata and with JPEG metadata
  (`comment=b"Hello CMYK"`, eight-byte ICC, EXIF orientation `6`), showed
  that sampled CMYK qtables keep-rgb saves with DPI are accepted as CMYK
  encoder aliases. Pillow writes APP0/JFIF density `300x150` before APP14,
  uses APP14 transform `0`, writes optional APP1 EXIF, APP2 ICC, and COM
  before two custom DQT segments, writes SOF0 with `C` sampling `2x1` for
  `subsampling=1` or `2x2` for `subsampling=2` and `M/Y/K` sampling `1x1`,
  writes standard DHT payload lengths `[29,179]`, one SOS, and reopens as
  mode `CMYK` with bytes `[0,42,79,148,10,42,79,148]` within tolerance while
  preserving DPI/JFIF and optional comment, ICC, and EXIF orientation. Native
  `pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options` and
  `pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options` now
  allow the bounded baseline sampled CMYK qtables keep-rgb plus DPI alias and
  route through the existing sampled CMYK qtables encoder plus metadata
  patcher where needed, while the facade routes public
  `Image.Save(..., "JPEG", {qtables, keep_rgb: true, dpi, subsampling: 1|2,
  ...})` without AHK pixel loops. Optimized/progressive sampled keep-rgb
  qtables, non-keep-rgb qtables metadata with real subsampling, and broader
  YCCK save behavior remain explicit future gaps. No ABI symbol was added;
  dumpbin still reports `327` `pillow_c_*` exports. Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`. Raw reds failed
  `Expected 0, got -3`; facade reds failed on the old sampled qtables DPI
  guards. Raw targeted greens passed `1/1` for the non-metadata case and
  `1/1` for the metadata case; facade targeted green passed `2/2`; broader
  JPEG filter passed
  `Ran 100 tests in 8907ms; Passed: 100, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 998 tests in 62875ms; Passed: 998, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003Z`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `keep_rgb=True`, JPEG metadata (`comment=b"Hello CMYK"`,
  eight-byte ICC, EXIF orientation `6`), `subsampling=1` or `2`, no DPI, no
  progressive, and no optimize showed that metadata sampled CMYK qtables
  keep-rgb saves are accepted as a CMYK encoder alias. Pillow writes APP14
  transform `0`, omits APP0/JFIF, inserts APP1 EXIF, APP2 ICC, and COM before
  two custom DQT segments, writes SOF0 with `C` sampling `2x1` for
  `subsampling=1` or `2x2` for `subsampling=2` and `M/Y/K` sampling `1x1`,
  writes standard DHT payload lengths `[29,179]`, one SOS, and reopens as mode
  `CMYK` with bytes `[0,42,79,148,10,42,79,148]` within tolerance while
  preserving comment, ICC, and EXIF orientation. Native
  `pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options` now
  allows the bounded baseline sampled CMYK metadata qtables keep-rgb alias and
  delegates to the existing sampled CMYK qtables encoder plus metadata patcher,
  while the facade routes public
  `Image.Save(..., "JPEG", {qtables, keep_rgb: true, subsampling: 1|2,
  comment, icc_profile, exif})` without AHK pixel loops. Optimized/progressive
  sampled qtables, sampled qtables plus DPI until `FMT-JPEG-003AA`, and
  broader YCCK save behavior remained explicit future gaps at that point. No
  ABI symbol was added; dumpbin still reports
  `327` `pillow_c_*` exports. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`. Raw red failed `Expected 0, got -3`; facade red
  failed on the old
  `Pillow.Image.Save JPEG CMYK subsampling currently supports only baseline quality and dpi saves`
  guard. Raw targeted green passed
  `Ran 1 tests in 109ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 172ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 96 tests in 8563ms; Passed: 96, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 994 tests in 61438ms; Passed: 994, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003Y`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `keep_rgb=True`, `subsampling=1` or `2`, no DPI, no
  metadata, no progressive, and no optimize showed that baseline sampled CMYK
  qtables keep-rgb saves are accepted as a CMYK encoder alias. Pillow writes
  APP14 transform `0`, omits APP0/JFIF, writes two custom DQT segments, writes
  SOF0 with `C` sampling `2x1` for `subsampling=1` or `2x2` for
  `subsampling=2` and `M/Y/K` sampling `1x1`, writes standard DHT payload
  lengths `[29,179]`, one SOS, and reopens as mode `CMYK` with bytes
  `[0,42,79,148,10,42,79,148]` within tolerance. Native
  `pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options` now treats
  bounded CMYK `keep_rgb=1` plus custom qtables and real baseline
  subsampling as a dispatch alias into the existing sampled CMYK qtables
  baseline encoder, while the facade routes public
  `Image.Save(..., "JPEG", {qtables, keep_rgb: true, subsampling: 1|2})`
  without AHK pixel loops. Optimized/progressive sampled qtables, metadata
  sampled qtables until `FMT-JPEG-003Z`, sampled qtables plus DPI until
  `FMT-JPEG-003AA`, and broader YCCK save behavior remained explicit future
  gaps at that point. No ABI symbol was added; dumpbin still reports
  `327` `pillow_c_*` exports. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`. Raw red failed `Expected 0, got -3`; facade red
  failed on the old
  `Pillow.Image.Save JPEG CMYK subsampling currently supports only baseline quality and dpi saves`
  guard. Raw targeted green passed
  `Ran 1 tests in 125ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 109ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 94 tests in 8500ms; Passed: 94, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 992 tests in 61797ms; Passed: 992, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003X`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `dpi=(300,150)`, `keep_rgb=True`, JPEG metadata
  (`comment=b"Hello CMYK"`, eight-byte ICC, EXIF orientation `6`), and no
  real subsampling showed that CMYK keep-rgb qtables metadata plus DPI is
  accepted as a CMYK encoder alias. The bounded batch covers baseline metadata
  qtables DPI, `optimize=True`, `progressive=True`, and combined
  `progressive=True,optimize=True` output. Pillow writes APP0/JFIF before
  APP14, then APP1 EXIF, APP2 ICC, and COM before two custom DQT segments,
  writes SOF0 plus one SOS for baseline/optimized output, writes SOF2 with the
  covered 18-scan CMYK progressive script for progressive output, and reopens
  as mode `CMYK` within qtables tolerance with DPI/JFIF, comment, ICC, and
  EXIF orientation preserved. Native
  `pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options` now
  treats bounded CMYK `keep_rgb=1` as a dispatch alias into the existing
  native CMYK qtables metadata encode path when DPI is present or absent and
  subsampling is default/4:4:4, while the facade routes public
  `Image.Save(..., "JPEG", {dpi, qtables, keep_rgb: true, comment,
  icc_profile, exif, ...})` without AHK pixel loops. CMYK keep-rgb qtables
  metadata with real subsampling, and broader YCCK save behavior remain
  explicit future gaps. No ABI symbol was added; dumpbin still reports `327`
  `pillow_c_*` exports. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`. Raw red failed `Expected 0, got -3`; facade red
  failed on the old
  `Pillow.Image.Save JPEG CMYK qtables metadata with keep_rgb is not supported`
  guard. Raw targeted green passed
  `Ran 1 tests in 141ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 313ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 92 tests in 8328ms; Passed: 92, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 990 tests in 60781ms; Passed: 990, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003W`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `keep_rgb=True`, JPEG metadata
  (`comment=b"Hello CMYK"`, eight-byte ICC, EXIF orientation `6`), and no
  DPI/subsampling showed that CMYK keep-rgb qtables metadata is accepted as a
  CMYK encoder alias. The bounded batch covers baseline metadata qtables,
  `optimize=True`, `progressive=True`, and combined
  `progressive=True,optimize=True` output. Pillow writes APP14 transform `0`,
  omits APP0/JFIF, writes APP1 EXIF, APP2 ICC, and COM after APP14 and before
  two custom DQT segments, writes SOF0 plus one SOS for baseline/optimized
  output, writes SOF2 with the covered 18-scan CMYK progressive script for
  progressive output, and reopens as mode `CMYK` within qtables tolerance with
  comment, ICC, and EXIF orientation preserved. Native
  `pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options` now
  treats bounded CMYK `keep_rgb=1` as a dispatch alias into the existing
  native CMYK qtables metadata encode path when DPI is absent and subsampling
  is default/4:4:4, while the facade routes public
  `Image.Save(..., "JPEG", {qtables, keep_rgb: true, comment, icc_profile,
  exif, ...})` without AHK pixel loops. CMYK keep-rgb qtables with DPI or real
  subsampling, and broader YCCK save behavior remain explicit future gaps.
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)` and dumpbin
  reports `327` `pillow_c_*` exports. Raw red failed on the missing export;
  facade red failed on the old
  `Pillow.Image.Save JPEG CMYK qtables metadata with keep_rgb is not supported`
  guard. Raw targeted green passed
  `Ran 1 tests in 157ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 297ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 90 tests in 8454ms; Passed: 90, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 988 tests in 60109ms; Passed: 988, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003V`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and `[64..1]`,
  `quality=95`, `keep_rgb=True`, and no metadata showed that CMYK keep-rgb
  qtables are accepted as a CMYK encoder alias. The bounded batch covers
  baseline qtables, `optimize=True`, `progressive=True`, and combined
  `progressive=True,optimize=True` output. Pillow writes APP14 transform `0`,
  omits APP0/JFIF, writes two custom DQT segments with `C` selecting table `0`
  and `M/Y/K` selecting table `1`, writes SOF0 plus one SOS for baseline and
  optimized output, writes SOF2 with the covered 18-scan CMYK progressive
  script for progressive output, and reopens as mode `CMYK` within qtables
  tolerance. Native `pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options`
  now treats bounded CMYK `keep_rgb=1` as a dispatch alias into the existing
  native CMYK qtables encode-options path when DPI, metadata, and subsampling
  are absent, while the facade routes public `Image.Save(..., "JPEG", {qtables,
  keep_rgb: true, ...})` without AHK pixel loops. CMYK keep-rgb with qtables
  plus metadata, qtables plus subsampling, and broader YCCK save behavior
  remained explicit future gaps at that point. No ABI symbol was added. Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)` and dumpbin still reports `326`
  `pillow_c_*` exports. Raw red failed `Expected 0, got -3`; facade red failed
  on the old `Pillow.Image.Save JPEG CMYK qtables with keep_rgb is not
  supported` guard. Raw targeted green passed
  `Ran 1 tests in 203ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 250ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 88 tests in 7578ms; Passed: 88, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 986 tests in 59109ms; Passed: 986, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003U`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, `quality=95`, `keep_rgb=True`, JPEG metadata
  (`comment=b"Hello CMYK"`, eight-byte ICC, EXIF orientation `6`), and no
  qtables showed that CMYK keep-rgb metadata is accepted as a CMYK encoder
  alias. The bounded batch covers baseline metadata, `dpi=(300,150)` plus
  `subsampling=1` metadata, `optimize=True` metadata, and `progressive=True`
  metadata. Pillow writes APP14 transform `0`; DPI cases write APP0/JFIF
  before APP14; APP1 EXIF, APP2 ICC, and COM are inserted after APP14 and
  before DQT; the subsampling case writes `C` as `2x1` and `M/Y/K` as `1x1`;
  optimized output writes compact DHT payload lengths `[19,38]`; progressive
  output writes SOF2 with the covered 18-scan CMYK script; reopened metadata
  preserves comment, ICC, and EXIF orientation. Native
  `pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options` now treats
  bounded CMYK `keep_rgb=1` as a dispatch alias into the existing native CMYK
  metadata encode-options path when qtables are absent, while the facade routes
  public `Image.Save(..., "JPEG", {keep_rgb: true, comment, icc_profile,
  exif, ...})` without AHK pixel loops. CMYK keep-rgb with qtables, combined
  progressive+optimize, and broader YCCK save behavior remain explicit future
  gaps. No ABI symbol was added. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)` and dumpbin still reports `326` `pillow_c_*`
  exports. Raw red failed `Expected 0, got -3`; facade red failed on the old
  `Pillow.Image.Save JPEG CMYK metadata with keep_rgb is not supported` guard.
  Raw targeted green passed
  `Ran 1 tests in 218ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 406ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 86 tests in 7203ms; Passed: 86, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 984 tests in 59016ms; Passed: 984, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003T`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, `quality=95`, and `keep_rgb=True` showed
  that CMYK keep-rgb is accepted and behaves as a CMYK encoder alias for the
  covered non-metadata surfaces. The bounded batch covers baseline
  `dpi=(300,150)` plus `subsampling=0`, baseline `dpi=(300,150)` plus
  `subsampling=1`, `optimize=True`, and `progressive=True`. Pillow writes
  APP0/JFIF before APP14 only for the DPI cases, uses APP14 transform `0`,
  writes SOF0 sampling `1x1` for all components or `C` as `2x1` with `M/Y/K`
  as `1x1` for `subsampling=1`, writes optimized DHT payload lengths
  `[19,38]` for the optimized case, and writes SOF2 with 18 SOS scans for
  the progressive case. Native
  `pillow_c_image_save_jpeg_encode_keep_rgb_options` now treats bounded CMYK
  `keep_rgb=1` as a dispatch alias into the existing native CMYK
  encode-options path, while the facade routes public
  `Image.Save(..., "JPEG", {keep_rgb: true, ...})` for those non-metadata,
  non-qtables CMYK saves without AHK pixel loops. CMYK keep-rgb with metadata,
  qtables, combined progressive+optimize, and broader YCCK save behavior
  remain explicit future gaps. No ABI symbol was added. Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)` and dumpbin still reports `326`
  `pillow_c_*` exports. Raw red failed `Expected 0, got -3`; facade red
  failed on the old `Pillow.Image.Save JPEG CMYK keep_rgb is not supported`
  guard. Raw targeted green passed
  `Ran 1 tests in 110ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 313ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 84 tests in 6781ms; Passed: 84, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 982 tests in 59360ms; Passed: 982, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003S`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, `quality=95`, `dpi=(300,150)`, and
  `subsampling=0`, `1`, or `2` showed that CMYK subsampling is accepted.
  `subsampling=0` writes SOF0 `C/M/Y/K` sampling `1x1/1x1/1x1/1x1` and
  reopens exact bytes; `subsampling=1` writes `C` as `2x1` and `M/Y/K` as
  `1x1`, and `subsampling=2` writes `C` as `2x2` and `M/Y/K` as `1x1`.
  For the covered `2x1` fixture, both subsampled outputs reopen as
  `[0,42,79,148,10,42,79,148]` and preserve DPI/JFIF metadata. Native
  `jpeg_prepare_cmyk_blocks` now prepares sampled CMYK MCUs by keeping `C`
  full-resolution and floor-downsampling inverted `M/Y/K` samples, while
  `pillow_c_image_save_jpeg_encode_options` routes bounded baseline CMYK
  subsampling plus optional DPI through the native encoder. The facade routes
  public `Image.Save(..., "JPEG", {subsampling, dpi})` for CMYK baseline
  saves and keeps CMYK subsampling with metadata, qtables, optimize, or
  progressive explicitly unsupported for future gap IDs. No ABI
  symbol was added. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)` and dumpbin still reports `326`
  `pillow_c_*` exports. Raw red failed `Expected 0, got -3`; facade red failed
  on the old
  `Pillow.Image.Save JPEG CMYK currently supports only baseline quality, dpi, metadata, qtables, optimize, and progressive saves`
  guard. Raw targeted green passed
  `Ran 1 tests in 140ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 187ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 82 tests in 6422ms; Passed: 82, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 980 tests in 58515ms; Passed: 980, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003R`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and
  `[64..1]`, `quality=95`, `dpi=(300,150)`, `comment=b"Hello CMYK"`,
  eight-byte ICC, EXIF orientation `6`, and `optimize=True`,
  `progressive=True`, or both together showed that optimized DPI metadata
  qtables output writes marker order `APP0/JFIF`, `APP14`, `APP1`, `APP2`,
  `COM`, two `DQT` segments, `SOF0`, optimized `DHT` payload lengths
  `[20,33]`, one `SOS`, then `EOI`, while progressive output writes
  `APP0/JFIF`, `APP14`, `APP1`, `APP2`, `COM`, two `DQT` segments, `SOF2`,
  then the covered 18-scan CMYK progressive script. APP0 uses unit `1` and
  density `300x150`, APP14 Adobe transform is `0`, SOF uses `C/M/Y/K` with
  `1x1` sampling and qtable selectors `0,1,1,1`, and reopening preserves mode
  `CMYK` bytes within qtables tolerance plus DPI/JFIF/comment/ICC/EXIF
  metadata. Native `pillow_c_image_save_jpeg_qtables_metadata_encode_options`
  now routes bounded CMYK DPI qtables metadata plus `optimize=True`,
  `progressive=True`, or both through the existing CMYK baseline/progressive
  qtables encoders, JFIF writer, and metadata patcher. The facade allows the
  matching CMYK options through the same export. No ABI symbol was added.
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)` and dumpbin
  still reports `326` `pillow_c_*` exports. Raw red failed
  `Expected 0, got -3`; facade red failed on the old
  `Pillow.Image.Save JPEG CMYK dpi qtables metadata with optimize or progressive is not supported`
  guard. Raw targeted green passed
  `Ran 1 tests in 156ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 250ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 80 tests in 6047ms; Passed: 80, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 978 tests in 58656ms; Passed: 978, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003Q`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and
  `[64..1]`, `quality=95`, `comment=b"Hello CMYK"`, eight-byte ICC, EXIF
  orientation `6`, and `optimize=True`, `progressive=True`, or both together
  showed that optimized metadata qtables output writes marker order `APP14`,
  `APP1`, `APP2`, `COM`, two `DQT` segments, `SOF0`, two optimized `DHT`
  segments with payload lengths `[20,33]`, one `SOS`, then `EOI`, while
  progressive metadata qtables output writes `APP14`, `APP1`, `APP2`, `COM`,
  two `DQT` segments, `SOF2`, then the covered 18-scan CMYK progressive
  script. APP14 Adobe transform is `0`, SOF uses `C/M/Y/K` with `1x1`
  sampling and qtable selectors `0,1,1,1`, and reopening preserves mode
  `CMYK` bytes within qtables tolerance plus comment/ICC/EXIF metadata.
  Native `pillow_c_image_save_jpeg_qtables_metadata_encode_options` now routes
  bounded non-DPI CMYK qtables metadata plus `optimize=True`,
  `progressive=True`, or both through the existing CMYK baseline/progressive
  qtables encoders and metadata patcher. The facade allows the matching CMYK
  qtables metadata options while keeping DPI plus advanced metadata qtables
  explicitly unsupported until a later tested slice. No ABI symbol was added.
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)` and dumpbin
  still reports `326` `pillow_c_*` exports. Raw red failed
  `Expected 0, got -3`; facade red failed on the old
  `Pillow.Image.Save JPEG CMYK qtables metadata with optimize or progressive is not supported`
  guard. Raw targeted green passed
  `Ran 1 tests in 187ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 234ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 78 tests in 5547ms; Passed: 78, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 976 tests in 57734ms; Passed: 976, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003P`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and
  `[64..1]`, `quality=95`, and `optimize=True`, `progressive=True`, or both
  together showed that optimized qtables output writes marker order `APP14`,
  two `DQT` segments, `SOF0`, two optimized `DHT` segments with payload
  lengths `[20,33]`, one `SOS`, then `EOI`, while progressive qtables output
  writes `APP14`, two `DQT` segments, `SOF2`, then the covered 18-scan CMYK
  progressive script. APP14 Adobe transform is `0`, SOF uses `C/M/Y/K` with
  `1x1` sampling and qtable selectors `0,1,1,1`, and reopening preserves mode
  `CMYK` bytes within qtables tolerance. Native
  `pillow_c_image_save_jpeg_qtables_encode_options` now routes bounded CMYK
  qtables plus `optimize=True`, `progressive=True`, or both through the
  existing CMYK baseline/progressive encoders with custom qtable preparation.
  The facade allows the matching non-metadata CMYK qtables options; metadata
  plus advanced qtables remained unsupported at this point until
  `FMT-JPEG-003Q`. No ABI symbol was added. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`. Raw red failed `Expected 0, got -3`; facade red
  failed on the old
  `Pillow.Image.Save JPEG CMYK qtables with optimize is not supported` guard.
  Raw targeted green passed
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 219ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 76 tests in 5484ms; Passed: 76, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 974 tests in 58703ms; Passed: 974, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003O`:
  local Pillow 11.3.0 probes for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` and
  `[64..1]`, `quality=95`, and `dpi=(300,150)` showed marker order
  `APP0/JFIF`, `APP14`, two `DQT` segments, `SOF0`, two standard `DHT`
  segments, one `SOS`, then `EOI`; the metadata variant with
  `comment=b"Hello CMYK"`, eight-byte ICC, and EXIF orientation `6` writes
  `APP0/JFIF`, `APP14`, `APP1`, `APP2`, `COM`, two `DQT` segments, `SOF0`,
  two `DHT` segments, one `SOS`, then `EOI`. APP0 uses unit `1` and density
  `300x150`, APP14 Adobe transform is `0`, SOF0 uses `C/M/Y/K` with `1x1`
  sampling and qtable selectors `0,1,1,1`, and reopening preserves mode
  `CMYK` bytes within qtables tolerance plus DPI/JFIF and, for the metadata
  case, comment/ICC/EXIF metadata. Native
  `pillow_c_image_save_jpeg_qtables_encode_options` and
  `pillow_c_image_save_jpeg_qtables_metadata_encode_options` now pass DPI
  through the existing CMYK baseline qtables encoder; the facade routes CMYK
  qtables plus DPI with and without JPEG metadata. No ABI symbol was added.
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)` and dumpbin
  still reports `326` `pillow_c_*` exports. Raw reds failed
  `Expected 0, got -3`; facade reds failed on the old
  `Pillow.Image.Save JPEG CMYK dpi with qtables is not supported` guard. Raw
  targeted green passed
  `Ran 2 tests in 78ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 2 tests in 187ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 74 tests in 5422ms; Passed: 74, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 972 tests in 56672ms; Passed: 972, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-003N`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, `quality=95`, `dpi=(300,150)`,
  `progressive=True`, `comment=b"Hello CMYK"`, eight-byte ICC, and EXIF
  orientation `6` showed marker order `APP0/JFIF`, `APP14`, `APP1`, `APP2`,
  `COM`, `DQT`, `SOF2`, then 17 `DHT` segments and 18 `SOS` scans before
  `EOI`; APP0 uses unit `1` and density `300x150`, APP14 Adobe transform is
  `0`, SOF2 components are `C/M/Y/K` with `1x1` sampling, and reopening
  preserves mode `CMYK` bytes plus DPI/JFIF/comment/ICC/EXIF metadata. Native
  `save_jpeg_cmyk_progressive` now accepts the existing DPI/JFIF arguments and
  writes APP0 before APP14, and the facade allows CMYK DPI plus progressive
  metadata through the existing `pillow_c_image_save_jpeg_metadata_encode_options`
  route. No ABI symbol was added. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`. Raw red failed `Expected 0, got -3`; facade red
  failed with
  `Expected CMYK JPEG dpi progressive metadata save to succeed, got: Pillow.Image.Save JPEG CMYK dpi currently supports only baseline quality, optimized baseline, baseline metadata, or optimized metadata saves`.
  Raw targeted green passed
  `Ran 1 tests in 125ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 110ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 70 tests in 4891ms; Passed: 70, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 968 tests in 57297ms; Passed: 968, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-003M`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, `quality=95`, `dpi=(300,150)`,
  `optimize=True`, `comment=b"Hello CMYK"`, eight-byte ICC, and EXIF
  orientation `6` showed marker order `APP0/JFIF`, `APP14`, `APP1`,
  `APP2`, `COM`, `DQT`, `SOF0`, two optimized `DHT` segments, one `SOS`,
  then `EOI`; APP0 uses unit `1` and density `300x150`; APP14 Adobe
  transform is `0`; SOF0 components are `C/M/Y/K` with `1x1` sampling; and
  reopening preserves exact mode `CMYK` bytes plus DPI/JFIF/comment/ICC/EXIF
  metadata. Native `save_jpeg_image_with_options` now allows bounded CMYK
  `dpi` plus `optimize=True` to reach the existing CMYK baseline encoder, and
  the facade passes DPI through the existing
  `pillow_c_image_save_jpeg_metadata_encode_options` route for CMYK optimized
  metadata saves. No ABI symbol was added. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)` and dumpbin still reports `326` `pillow_c_*`
  exports. Raw red failed `Expected 0, got -3`; facade red failed with
  `Expected CMYK JPEG dpi optimize metadata save to succeed, got: Pillow.Image.Save JPEG CMYK dpi currently supports only baseline quality or baseline metadata saves`.
  Raw targeted green passed
  `Ran 1 tests in 79ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 68 tests in 4875ms; Passed: 68, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 966 tests in 57266ms; Passed: 966, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after the `FMT-JPEG-003C` real-fixture
  coverage extension:
  local Pillow 11.3.0 probe for
  `.codex\pillow-src\pillow-11.3.0\Tests\images\pil_sample_cmyk.jpg`
  showed mode `CMYK`, size `100x100`, `info["adobe"] == 100`,
  `info["adobe_transform"] == 2`, `40000` decoded bytes, first bytes
  `[0,242,230,0,...]`, center window
  `[242,212,210,229,242,213,209,232,...]`, and tail window repeating
  `[242,213,209,230]`. The fixture was copied into
  `ahk\fixtures\pil_sample_cmyk_ycck.jpg` so tests do not depend on ignored
  `.codex` state. Existing native `pillow_c_image_open_jpeg` and facade
  `Image.Open(..., ["JPEG"])` already matched Pillow on this real APP14
  transform `2` fixture, so this slice added raw/facade coverage only; no
  source, ABI, export, DLL rebuild, or facade routing change was required.
  Raw targeted coverage passed
  `Ran 1 tests in 47ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted coverage passed
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 66 tests in 4657ms; Passed: 66, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 964 tests in 57078ms; Passed: 964, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-003L`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, `quality=95`, `dpi=(300,150)`,
  `comment=b"Hello CMYK"`, eight-byte ICC, and EXIF orientation `6` showed
  Pillow writes `APP0/JFIF` first with unit `1` and density `300x150`, then
  `APP14` Adobe transform `0`, followed by `APP1` EXIF, `APP2`
  `ICC_PROFILE`, `COM`, `DQT`, `SOF0`, two standard `DHT` segments, one
  `SOS`, and `EOI`; reopening preserves exact mode `CMYK` bytes plus DPI/JFIF,
  comment, ICC, and EXIF metadata. Native `pillow_c_image_save_jpeg_metadata_options`
  now allows the existing CMYK baseline DPI encoder to run before metadata
  patching, and `jpeg_metadata_insert_position` skips both leading JFIF and
  APP14 markers so metadata is inserted after APP14. The facade routes bounded
  baseline CMYK `dpi` plus JPEG metadata through the same export; at that
  point, CMYK DPI with qtables, optimize/progressive, subsampling, or keep-rgb
  still remained rejected. No ABI symbol was added. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)` and dumpbin still reports `326` `pillow_c_*`
  exports. Raw red failed `Expected 0, got -3`; facade red failed with
  `Expected CMYK JPEG dpi metadata save to succeed, got: Pillow.Image.Save JPEG CMYK dpi currently supports only baseline quality saves`.
  Raw targeted green passed
  `Ran 1 tests in 93ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 64 tests in 4656ms; Passed: 64, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 962 tests in 57062ms; Passed: 962, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-003K`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, `quality=95`, and `dpi=(300,150)`
  showed Pillow writes `APP0/JFIF` first with unit `1` and density
  `300x150`, then `APP14` Adobe transform `0`, followed by baseline CMYK
  DQT/SOF0/DHT/SOS markers; reopening preserves exact mode `CMYK` bytes and
  exposes `dpi`, `jfif_unit`, and `jfif_density` metadata. Native
  `save_jpeg_cmyk_baseline` now optionally writes the JFIF APP0 segment before
  APP14 when called through `pillow_c_image_save_jpeg_options`; CMYK qtables,
  metadata, progressive, optimize, subsampling, and keep-rgb with DPI remain
  explicit future gaps. No ABI symbol was added. Release x64 rebuild succeeded
  with `0 Warning(s), 0 Error(s)` and dumpbin still reports `326`
  `pillow_c_*` exports. Raw red failed `Expected 0, got -3`; facade red failed
  with `Expected CMYK JPEG dpi save to succeed, got: Pillow.Image.Save JPEG CMYK currently supports only baseline quality, metadata, qtables, optimize, and progressive saves`.
  Raw targeted green passed
  `Ran 1 tests in 93ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 62 tests in 4703ms; Passed: 62, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 960 tests in 58000ms; Passed: 960, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-GIF-003B7`:
  local Pillow 11.3.0 probe for a same-size lossy mixed `18x18` animation with
  first RGB stress frame and second RGBA stress frame, `duration=[10,20]`,
  `loop=0`, and `disposal=[0,0]` showed Pillow accepts the mixed >256-color
  frames, writes two frames, gives the RGBA frame a transparency GCE, reopens
  with duration metadata `10` and `20` plus loop `0`, and produces bounded
  approximate RGB pixels with max channel errors `21` and `38` and average
  Manhattan errors about `8.71` and `8.56`. Native
  `pillow_c_image_save_gif_animation` now falls back from exact RGBA animation
  quantization to the existing GIF-specific weighted median-cut RGBA quantizer
  before the existing animation writer; mixed RGB/RGBA >256-color animation
  saves therefore stay inside the DLL without AHK pixel loops. No ABI symbol
  was added. Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)` and
  dumpbin still reports `326` `pillow_c_*` exports. Raw red failed
  `Expected 0, got -3`; facade red failed with
  `pillow_c: invalid argument`. Raw targeted green passed
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 188ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  raw GIF animation filter passed
  `Ran 25 tests in 922ms; Passed: 25, Failed: 0, Errors: 0, Skipped: 0`;
  facade `save_all` filter passed
  `Ran 25 tests in 3140ms; Passed: 25, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 958 tests in 57656ms; Passed: 958, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-GIF-003B6`:
  local Pillow 11.3.0 probe for mixed RGB/RGBA `2x1` animation frames with
  first RGB pixels `[black, red]`, second RGBA pixels
  `[(red, opaque), (1,2,3, transparent)]`, `duration=[10,20]`, `loop=0`, and
  `disposal=[0,0]` showed Pillow accepts the mixed modes, writes two full-width
  frames, gives the second frame a transparency GCE index `0`, reopens frame 1
  as black/red RGBA, and reopens frame 2 composited as red/red RGBA with
  duration metadata `20` and loop `0`. Native `pillow_c_image_save_gif_animation`
  now quantizes each RGB or RGBA animation frame independently into temporary
  DLL-owned P images before the existing animation writer; no ABI symbol was
  added. Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)` and the
  DLL still exposes `326` `pillow_c_*` exports. Raw red failed
  `Expected 0, got -5`; facade red failed with `pillow_c: mismatch`. Mixed
  raw/facade targeted green passed
  `Ran 2 tests in 266ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`;
  raw GIF animation filter passed
  `Ran 24 tests in 891ms; Passed: 24, Failed: 0, Errors: 0, Skipped: 0`;
  facade `save_all` filter passed
  `Ran 24 tests in 3094ms; Passed: 24, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 956 tests in 56906ms; Passed: 956, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-003J`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, two custom qtables `[1..64]` plus
  `[64..1]`, `quality=95`, `comment=b"Hello CMYK"`, eight-byte ICC, and EXIF
  orientation `6` showed marker order `APP14`, `APP1`, `APP2`, `COM`, `DQT`,
  `DQT`, `SOF0`, two standard `DHT` segments, one `SOS`, then `EOI`; APP14
  Adobe transform `0`; SOF0 components `C/M/Y/K` with `1x1` sampling, C using
  qtable `0`, and M/Y/K using qtable `1`; and mode `CMYK` reopen bytes within
  tolerance plus comment, ICC, and EXIF metadata. Existing native
  `pillow_c_image_save_jpeg_qtables_metadata_encode_options` already composed
  the CMYK qtables encoder with metadata marker patching after APP14, so no
  native source or ABI symbol changed and no Release x64 rebuild was required.
  The facade now routes CMYK JPEG qtables plus metadata through the same export
  while still rejecting CMYK DPI, subsampling, keep-rgb, qtables plus
  optimize/progressive, and optimize plus progressive. Raw targeted coverage
  passed immediately
  `Ran 1 tests in 47ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade red failed with
  `Pillow.Image.Save JPEG CMYK qtables with metadata is not supported`;
  facade targeted green passed
  `Ran 1 tests in 93ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 60 tests in 4516ms; Passed: 60, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 954 tests in 56063ms; Passed: 954, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-003I`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, `quality=95`, `progressive=True`,
  `comment=b"Hello CMYK"`, eight-byte ICC, and EXIF orientation `6` showed
  marker order `APP14`, `APP1`, `APP2`, `COM`, `DQT`, `SOF2`, then 17 `DHT`
  segments and 18 `SOS` scans before `EOI`; APP14 Adobe transform `0`; SOF2
  components `C/M/Y/K` with `1x1` sampling and qtable `0`; and exact mode
  `CMYK` reopen bytes plus comment, ICC, and EXIF metadata. Existing native
  `pillow_c_image_save_jpeg_metadata_encode_options` already composed the
  CMYK progressive encoder with metadata marker patching after APP14, so no
  native source or ABI symbol changed and no Release x64 rebuild was required.
  The facade now routes CMYK JPEG metadata plus `progressive=True` through the
  same export while still rejecting CMYK DPI, subsampling, keep-rgb, qtables
  plus metadata, qtables plus optimize/progressive, and optimize plus
  progressive. Raw targeted coverage passed immediately
  `Ran 1 tests in 31ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade red failed with
  `Pillow.Image.Save JPEG CMYK progressive with metadata, qtables, or optimize is not supported`;
  facade targeted green passed
  `Ran 1 tests in 109ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 58 tests in 4313ms; Passed: 58, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 952 tests in 56860ms; Passed: 952, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-003H`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, `quality=95`, `optimize=True`,
  `comment=b"Hello CMYK"`, eight-byte ICC, and EXIF orientation `6` showed
  marker order `APP14`, `APP1`, `APP2`, `COM`, `DQT`, `SOF0`, two compact
  optimized `DHT` segments, one `SOS`, then `EOI`; APP14 Adobe transform `0`;
  SOF0 components `C/M/Y/K` with `1x1` sampling and qtable `0`; optimized DHT
  payload lengths `[19,38]`; and exact mode `CMYK` reopen bytes plus comment,
  ICC, and EXIF metadata. Existing native
  `pillow_c_image_save_jpeg_metadata_encode_options` already composed the CMYK
  optimized encoder with metadata marker patching after APP14, so no native
  source or ABI symbol changed and no Release x64 rebuild was required. The
  facade now routes CMYK JPEG metadata plus `optimize=True` while still
  rejecting CMYK DPI, subsampling, keep-rgb, qtables plus metadata,
  metadata plus `progressive=True`, qtables plus optimize/progressive, and
  optimize plus progressive. Raw targeted coverage passed immediately
  `Ran 1 tests in 62ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade red failed with
  `Pillow.Image.Save JPEG CMYK metadata with optimize is not supported`;
  facade targeted green passed
  `Ran 1 tests in 79ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 56 tests in 4188ms; Passed: 56, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 950 tests in 56797ms; Passed: 950, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-GIF-003B5`:
  local Pillow 11.3.0 probe for two exact-color RGBA `2x1` animation frames
  with pixels `[(black, opaque), (red, opaque)] -> [(red, opaque),
  (1,2,3, transparent)]`, `duration=[10,20]`, `loop=0`, and
  `disposal=[0,0]` showed Pillow writes frame 2 full width with local palette
  head `[1,2,3, 255,0,0]`, GCE transparency index `0`, reopens frame 1 as
  black/red RGBA, and reopens frame 2 composited as red/red RGBA with duration
  metadata `20` and loop `0`. Native `pillow_c_image_save_gif_animation` now
  quantizes bounded RGBA animation frames into temporary DLL-owned P images,
  reserves transparency index `0` only when source alpha needs it, and uses the
  existing animation writer without adding an ABI symbol. Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)` and the DLL still exposes `326`
  `pillow_c_*` exports. Raw red failed `Expected 0, got -3`; facade red failed
  with `pillow_c: invalid argument`. Raw targeted green passed
  `Ran 1 tests in 109ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 219ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  raw GIF animation filter passed
  `Ran 23 tests in 859ms; Passed: 23, Failed: 0, Errors: 0, Skipped: 0`;
  facade `save_all` filter passed
  `Ran 23 tests in 3046ms; Passed: 23, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 948 tests in 56125ms; Passed: 948, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-GIF-003B4`:
  local Pillow 11.3.0 probe for two exact-color RGB `2x1` animation frames
  with pixels `[black, red] -> [red, blue]`, `duration=[10,20]`, `loop=0`,
  and `disposal=[0,0]` showed Pillow saves a two-frame GIF that reopens with
  exact RGB pixels, duration metadata `10` and `20`, and loop `0`; frame 2 is
  a full-width local frame with generated transparency index `2`. Native
  `pillow_c_image_save_gif_animation` now quantizes bounded RGB animation
  frames into temporary DLL-owned P images before running the existing GIF
  animation differencing/writer path; no ABI symbol was added and the Release
  x64 DLL was rebuilt with `0 Warning(s), 0 Error(s)`. Raw red failed
  `Expected 0, got -3`; facade red failed with `pillow_c: invalid argument`.
  Raw targeted green passed
  `Ran 1 tests in 94ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 172ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  raw GIF animation filter passed
  `Ran 22 tests in 890ms; Passed: 22, Failed: 0, Errors: 0, Skipped: 0`;
  facade `save_all` filter passed
  `Ran 22 tests in 3063ms; Passed: 22, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 946 tests in 55282ms; Passed: 946, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-003G`:
  local Pillow 11.3.0 probe for the existing bounded `2x1` CMYK fixture with
  APP14 Adobe transform patched from `0` to `2` showed Pillow opens it as mode
  `CMYK`, reports `adobe_transform == 2`, and returns bytes
  `[254,234,255,255,255,139,255,40]`; current WIC-backed native JPEG open
  already matched this bounded YCCK transform surface, so raw and facade tests
  were added as coverage and passed immediately. A second local Pillow 11.3.0
  probe for bounded mode `CMYK`, `quality=95`, `progressive=True`, and pixels
  `[0,64,128,255,10,20,30,40]` showed marker order `APP14`, `DQT`, `SOF2`,
  then 17 `DHT` segments and 18 `SOS` scans before `EOI`; APP14 Adobe
  transform `0`; SOF2 components `C/M/Y/K` with `1x1` sampling and qtable `0`;
  and a progressive scan script of interleaved DC, four low-AC scans, four
  high-AC scans, four AC refinement scans, interleaved DC refinement, then four
  final AC refinement scans. Native `pillow_c_image_save_jpeg_encode_options`
  now routes this bounded CMYK progressive save through a DLL-owned progressive
  encoder using the existing CMYK block preparation and CMYK;I sample
  inversion; no ABI symbol was added. The facade now routes bare CMYK
  `progressive=True` / `progression=True` saves while still rejecting CMYK DPI,
  subsampling, keep-rgb, qtables plus progressive, metadata plus progressive,
  and optimize plus progressive. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`. Raw CMYK progressive red failed with
  `Expected 0, got -3`; facade red failed with
  `Pillow.Image.Save JPEG CMYK progressive saves are not supported`. Raw
  targeted green passed
  `Ran 1 tests in 32ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 54 tests in 4328ms; Passed: 54, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 944 tests in 55328ms; Passed: 944, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-003F`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, `quality=95`,
  `comment=b"Hello CMYK"`, eight-byte ICC, EXIF orientation `6`, and pixels
  `[0,64,128,255,10,20,30,40]` showed marker order `APP14`, `APP1 EXIF`,
  `APP2 ICC`, `COM`, `DQT`, `SOF0`, two `DHT`, one `SOS`, then `EOI`;
  APP14 Adobe transform `0`; SOF0 components `C/M/Y/K` with `1x1` sampling
  and qtable `0`; DHT payload lengths `[29,179]`; one four-component SOS; and
  exact mode `CMYK` reopen bytes plus comment, ICC, and EXIF metadata. Existing
  native `pillow_c_image_save_jpeg_metadata_options` already composed the CMYK
  baseline encoder with metadata marker patching after APP14, so no native
  source or ABI symbol changed and no Release x64 rebuild was required. The
  facade now routes baseline CMYK JPEG metadata saves while still rejecting
  CMYK DPI, subsampling, keep-rgb, `progressive=True`, qtables plus metadata,
  and metadata plus `optimize=True`. Raw coverage passed immediately
  `Ran 1 tests in 63ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; facade
  red failed with
  `Pillow.Image.Save JPEG CMYK currently supports only baseline quality, qtables, and optimize saves`;
  facade targeted green passed
  `Ran 1 tests in 93ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; broader
  JPEG filter passed
  `Ran 50 tests in 4063ms; Passed: 50, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 940 tests in 53891ms; Passed: 940, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-003E`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, `quality=95`,
  `optimize=True`, and pixels `[0,64,128,255,10,20,30,40]` showed APP14 Adobe
  transform `0`, one DQT table `0`, SOF0 components `C/M/Y/K` with `1x1`
  sampling and qtable `0`, compact optimized DHT payload lengths `[19,38]`,
  one four-component SOS, and exact mode `CMYK` reopen bytes. Native
  `save_jpeg_cmyk_baseline` now collects Huffman frequencies across C/M/Y/K
  blocks when `optimize=True`, builds optimized DC/AC tables, keeps CMYK;I
  sample inversion before DCT, and writes the optimized stream through existing
  JPEG encode-options exports; no new ABI symbol was added. The facade now
  routes baseline CMYK `optimize=True` saves while still rejecting CMYK DPI,
  metadata, subsampling, keep-rgb, `progressive=True`, qtables plus optimize,
  and YCCK. Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`.
  Raw red failed with `Expected 0, got -3`; facade red failed with
  `Pillow.Image.Save JPEG CMYK progressive and optimize saves are not supported`.
  Raw targeted green passed
  `Ran 1 tests in 62ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed
  `Ran 48 tests in 3687ms; Passed: 48, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 938 tests in 54859ms; Passed: 938, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-002B2O`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, pixels
  `[0,64,128,255,10,20,30,40]`, one custom qtable `[1..64]`, and two custom
  qtables `[1..64]` plus `[64..1]` showed Pillow uses
  `RAWMODE["CMYK"] == "CMYK;I"`, APP14 Adobe transform `0`, standard
  luminance DHT payload lengths `[29,179]`, and one four-component SOS. With
  one qtable at `quality=75`, Pillow writes one DQT table `0`, SOF0
  `C/M/Y/K` components all selecting qtable `0`, and exact mode `CMYK` reopen
  bytes. With two qtables at `quality=95`, Pillow writes DQT tables `0` and
  `1`, SOF0 component `C` selecting qtable `0` and `M/Y/K` selecting qtable
  `1`, and reopens as mode `CMYK`. Native
  `pillow_c_image_save_jpeg_qtables_encode_options` now routes bounded CMYK
  baseline qtables saves through `save_jpeg_cmyk_baseline`, quality-scales one
  or two caller qtables, keeps the CMYK;I sample inversion before DCT, and adds
  no new ABI symbol. The facade now routes baseline CMYK `qtables` saves while
  still rejecting CMYK DPI, metadata, subsampling, keep-rgb, qtables plus
  `optimize=True`, `progressive=True`, and YCCK. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`. Raw red failed with `Expected 0, got -3`;
  facade red failed with
  `Pillow.Image.Save JPEG CMYK currently supports only baseline quality saves`.
  Raw targeted green passed
  `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed
  `Ran 1 tests in 140ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  qtables filter passed
  `Ran 10 tests in 1281ms; Passed: 10, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 936 tests in 57422ms; Passed: 936, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-003D`:
  local Pillow 11.3.0 probe for bounded mode `CMYK`, `quality=95`, and pixels
  `[0,64,128,255,10,20,30,40]` showed `RAWMODE["CMYK"] == "CMYK;I"`,
  APP14 Adobe transform `0`, one DQT table `0`, SOF0 components `C/M/Y/K`
  with `1x1` sampling and qtable `0`, standard luminance DHT payload lengths
  `[29,179]`, one four-component SOS, and exact mode `CMYK` reopen bytes.
  Native `save_jpeg_cmyk_baseline` now inverts CMYK samples before DCT and
  writes the bounded baseline stream through existing JPEG save exports; no new
  ABI symbol was added. The facade now routes baseline CMYK quality saves while
  leaving CMYK metadata, DPI, subsampling, keep-rgb,
  `optimize=True`, `progressive=True`, and YCCK deferred. Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`. Raw red failed with `Expected 0,
  got -3`; facade red failed on the explicit CMYK guard. Raw targeted green
  passed `Ran 1 tests in 78ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed `Ran 1 tests in 93ms; Passed: 1, Failed: 0,
  Errors: 0, Skipped: 0`; broader JPEG filter passed
  `Ran 44 tests in 3515ms; Passed: 44, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 934 tests in 55391ms; Passed: 934, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-002B2N`:
  local Pillow 11.3.0 probe for bounded mode `RGB`, `quality=75`,
  `keep_rgb=True`, `comment=b"Hello JPEG"`, eight-byte ICC, and EXIF
  orientation `6` showed APP14 Adobe transform `0`, no APP0/JFIF, APP1 EXIF,
  APP2 ICC, COM, then DQT before SOF. Baseline and optimized output use SOF0,
  RGB component IDs `R/G/B`, `1x1` sampling, one SOS, and baseline DHT lengths
  `[29,179]` or compact optimized DHT lengths `[24,40]`; progressive output
  uses SOF2 with 14 SOS scans. Native
  `pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options` composes the
  DLL-owned keep-rgb encoder with metadata patching after APP14. Release x64
  rebuild succeeded with `0 Warning(s), 0 Error(s)`. Raw targeted green passed
  `Ran 1 tests in 172ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green passed `Ran 1 tests in 235ms; Passed: 1, Failed: 0,
  Errors: 0, Skipped: 0`; broader JPEG filter passed
  `Ran 44 tests in 3750ms; Passed: 44, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory run with `-TimeoutSeconds 120` exited `0` and recorded
  `Ran 934 tests in 54610ms; Passed: 934, Failed: 0, Errors: 0, Skipped: 0`.
- Latest full directory verification after `FMT-JPEG-002B2M`:
  local Pillow 11.3.0 probe for bounded mode `L` custom `qtables` with a
  second ignored table showed one custom DQT table `0`, SOF0 component
  `[1,1,1,0]`, standard baseline DHT payload lengths `[29,179]`, one SOS,
  compact optimized DHTs for `optimize=True`, SOF2 plus six SOS scans for
  `progressive=True`, and metadata marker order `APP0/JFIF`, `APP1 EXIF`,
  `APP2 ICC`, `COM`, then DQT for the metadata fixture. The worktree already
  contained the red/green implementation when resumed; Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`. Fresh qtables filter passed
  `Ran 8 tests in 1078ms; Passed: 8, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed `Ran 42 tests in 3437ms; Passed: 42, Failed: 0,
  Errors: 0, Skipped: 0`; full AHK directory run with `-TimeoutSeconds 120`
  exited `0` and recorded `Ran 932 tests in 54563ms; Passed: 932, Failed: 0,
  Errors: 0, Skipped: 0`.
- Latest targeted GIF verification after `FMT-GIF-004M`:
  local Pillow 11.3.0 probe for a P-mode `3x1` animation with pixels
  `[0,0,0] -> [1,0,0] -> [2,2,2]`, palette entries black/red/black, caller
  `transparency=2`, caller `background=1`, `optimize=False`,
  `duration=[10,20,30]`, `loop=0`, and `disposal=[0,2,0]` writes frame 3 full
  width with GCE transparency `2`, decoded indices `[2,2,2]`, `LzwMin=8`, and
  no local color table (`Flags=0`, `LocalColorCount=0`). Raw and facade red
  both failed with `Expected 0, got 129` on frame 3 descriptor flags. Release
  x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`. Raw targeted green
  `Ran 1 tests in 110ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green `Ran 1 tests in 344ms; Passed: 1, Failed: 0, Errors:
  0, Skipped: 0`; raw GIF animation filter `Ran 17 tests in 1437ms; Passed:
  17, Failed: 0, Errors: 0, Skipped: 0`; facade GIF `save_all` filter
  `Ran 17 tests in 5250ms; Passed: 17, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted GIF verification after `FMT-GIF-004N`:
  local Pillow 11.3.0 probe for a P-mode `3x1` animation with pixels
  `[0,0,0] -> [1,0,0] -> [2,2,2]`, palette entries black/red/black, caller
  `transparency=2`, caller `background=1`, default optimized save,
  `duration=[10,20,30]`, `loop=0`, and `disposal=[0,2,0]` writes frame 3 full
  width with a local 4-entry all-black color table, GCE transparency `0`,
  decoded indices `[0,0,0]`, `LzwMin=8`, and reopened black RGB bytes. Raw and
  facade red both failed with `Expected 0, got -1` on frame 3 GCE
  transparency. Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`.
  Raw targeted green `Ran 1 tests in 187ms; Passed: 1, Failed: 0, Errors: 0,
  Skipped: 0`; facade targeted green `Ran 1 tests in 359ms; Passed: 1,
  Failed: 0, Errors: 0, Skipped: 0`; raw GIF animation filter `Ran 18 tests
  in 1265ms; Passed: 18, Failed: 0, Errors: 0, Skipped: 0`; facade GIF
  `save_all` filter `Ran 18 tests in 4453ms; Passed: 18, Failed: 0, Errors:
  0, Skipped: 0`; full AHK directory rerun `Ran 902 tests in 104563ms;
  Passed: 902, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted GIF verification after `FMT-GIF-004O`:
  local Pillow 11.3.0 probe for a P-mode `4x2` animation with frame pixels
  `[1,0,0,0,0,0,0,0] -> [1,1,0,0,0,0,0,0] ->
  [1,0,0,0,0,0,0,0]`, palette entries black/red/black, caller
  `transparency=2`, default optimized save, `duration=[10,20,30]`, `loop=0`,
  and `disposal=[0,2,0]` writes frame 3 as a full `4x2` local frame with a
  4-entry local color table, no transparency GCE, decoded indices
  `[1,0,0,0,0,0,0,0]`, and `LzwMin=8`. Raw and facade red both failed with
  `Expected [0, 0, 4, 2], got [0, 0, 1, 1]` on frame 3 descriptor geometry.
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`. Raw targeted
  green `Ran 1 tests in 110ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade targeted green `Ran 1 tests in 282ms; Passed: 1, Failed: 0, Errors:
  0, Skipped: 0`; raw GIF animation filter `Ran 19 tests in 1453ms; Passed:
  19, Failed: 0, Errors: 0, Skipped: 0`; facade GIF `save_all` filter
  `Ran 19 tests in 4641ms; Passed: 19, Failed: 0, Errors: 0, Skipped: 0`;
  strict quiet full-suite run with `-TimeoutSeconds 120` exited `0`.
- Latest targeted GIF verification after `FMT-GIF-004P`:
  local Pillow 11.3.0 probe for a mixed-unused-palette P-mode `4x2`
  animation with frame pixels `[1,0,0,0,0,0,0,0] ->
  [1,1,0,0,0,0,0,0] -> [1,0,0,0,0,0,0,0]`, first/second palette entries
  black/red/black/green, third palette entries black/red/black/blue, caller
  `transparency=2`, default optimized save, `duration=[10,20,30]`, `loop=0`,
  and `disposal=[0,2,0]` writes frame 3 as a full `4x2` local frame with a
  4-entry local color table `[black, red, black, black]`, no transparency GCE,
  decoded indices `[1,0,0,0,0,0,0,0]`, and `LzwMin=8`. Raw and facade red
  both failed with `Expected [0, 0, 4, 2], got [0, 0, 1, 1]` on frame 3
  descriptor geometry. Release x64 rebuild succeeded with `0 Warning(s), 0
  Error(s)`. Raw targeted green `Ran 1 tests in 109ms; Passed: 1, Failed: 0,
  Errors: 0, Skipped: 0`; facade targeted green `Ran 1 tests in 187ms;
  Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; raw GIF animation filter
  `Ran 20 tests in 1157ms; Passed: 20, Failed: 0, Errors: 0, Skipped: 0`;
  facade GIF `save_all` filter `Ran 20 tests in 3922ms; Passed: 20, Failed:
  0, Errors: 0, Skipped: 0`; full AHK directory rerun
  `Ran 906 tests in 60375ms; Passed: 906, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted GIF verification after `FMT-GIF-004Q`:
  local Pillow 11.3.0 probe for a sparse P-mode `3x1` animation with frame
  pixels `[0,0,0] -> [3,0,0] -> [1,0,0]`, palette entries black/red/black/blue,
  caller `background=1`, no caller transparency, default optimized save,
  `duration=[10,20,30]`, `loop=0`, and `disposal=[0,2,0]` writes logical screen
  background `1`, compacts frame 2 to local palette `[black, blue, black,
  black]` with generated transparency `2`, and writes frame 3 full width as
  local palette `[black, red, black, black]`, no transparency GCE, decoded
  indices `[1,0,0]`, and `LzwMin=8`. Existing native behavior already matched
  the probe. Raw targeted coverage passed `Ran 1 tests in 110ms; Passed: 1,
  Failed: 0, Errors: 0, Skipped: 0`; facade targeted coverage passed
  `Ran 1 tests in 422ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; raw GIF
  animation filter passed `Ran 21 tests in 1547ms; Passed: 21, Failed: 0,
  Errors: 0, Skipped: 0`; facade GIF `save_all` filter passed `Ran 21 tests in
  5750ms; Passed: 21, Failed: 0, Errors: 0, Skipped: 0`; full AHK directory
  rerun passed `Ran 912 tests in 111593ms; Passed: 912, Failed: 0, Errors: 0,
  Skipped: 0`. No native source changed, no ABI export changed, and no Release
  x64 rebuild was required for this coverage-only slice.
- Latest targeted GIF verification after `FMT-GIF-004R`:
  local Pillow 11.3.0 probe for two identical P-mode frames showed that an
  all-output-frame collapse with scalar `disposal` succeeds, but the same
  collapse with list-valued `disposal=[0,0]` raises `TypeError: int() argument
  must be a string, a bytes-like object or a real number, not 'list'`. A
  three-frame probe where only the middle frame is identical still succeeds by
  merging duration. Raw red failed with `Expected -3, got 0`; facade red failed
  by not throwing. Native `save_gif_animation_image` now returns
  `PILLOW_C_INVALID_ARGUMENT` when the optimized frame vector has collapsed to
  one frame while the caller supplied more than one disposal value. Release x64
  rebuild succeeded with `0 Warning(s), 0 Error(s)`. Targeted raw green passed
  `Ran 1 tests in 31ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; targeted
  facade green passed `Ran 1 tests in 16ms; Passed: 1, Failed: 0, Errors: 0,
  Skipped: 0`; raw GIF animation filter passed `Ran 29 tests in 1141ms;
  Passed: 29, Failed: 0, Errors: 0, Skipped: 0`; facade GIF `save_all` filter
  passed `Ran 29 tests in 141ms; Passed: 29, Failed: 0, Errors: 0, Skipped:
  0`; full AHK directory rerun passed `Ran 1199 tests in 3516ms; Passed: 1199,
  Failed: 0, Errors: 0, Skipped: 0` with the known non-failing libjpeg stderr
  warnings. No ABI export was added; source and DLL export counts remain `341`.
- Previous targeted GIF verification after `FMT-GIF-004S`:
  local Pillow 11.3.0 probe for a `2x1` mixed first-RGB / second-RGBA animation
  with `duration=[10,20]`, `loop=0`, `disposal=[0,0]`, and explicit
  `transparency=1` writes both frame GCE transparency values as `1`; reopening
  frame 0 and converting to RGBA maps the black caller-transparent pixel to
  alpha `0`; the second frame has a 4-entry local color table whose head is
  `[1,2,3, 255,0,0]`. Raw red first failed with `Expected 1, got -1`; facade
  red failed on the same missing GCE transparency; after the writer fix, raw
  exposed missing frame-0 palette-alpha mapping on GIF open. Native
  `save_gif_animation_image` now preserves explicit caller transparency for the
  first quantized RGB animation frame, and `open_gif_frame_image` maps frame-0
  GCE transparency into palette alpha. Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`. Targeted raw green passed `Ran 1 tests in 47ms;
  Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; targeted facade green passed
  `Ran 1 tests in 31ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; raw GIF
  animation filter passed `Ran 30 tests in 1140ms; Passed: 30, Failed: 0,
  Errors: 0, Skipped: 0`; facade GIF `save_all` filter passed `Ran 30 tests in
  141ms; Passed: 30, Failed: 0, Errors: 0, Skipped: 0`; full AHK directory
  rerun passed `Ran 1205 tests in 3641ms; Passed: 1205, Failed: 0, Errors: 0,
  Skipped: 0` with the known non-failing libjpeg stderr warnings. No ABI export
  was added; source and DLL export counts remain `341`.
- Latest targeted GIF verification after `FMT-GIF-002F`:
  local Pillow 11.3.0 on the same mixed first-RGB / second-RGBA
  caller-transparency fixture reopens frame 1 as mode `RGBA`, exposes no
  frame-level `Info["transparency"]`, and returns RGBA bytes
  `[0,0,0,0, 1,2,3,255]`. Raw red failed with `Expected 4, got 3` for frame 1
  mode; facade red reopened frame 1 as `[255,0,0,255, 1,2,3,255]`. Native
  `open_gif_composited_frame_image` now upgrades to an RGBA canvas only when
  frame 0 actually used GCE transparency, skips transparent local pixels, and
  draws opaque later pixels with alpha `255`. Release x64 rebuild succeeded
  with `0 Warning(s), 0 Error(s)`. Targeted raw green passed `Ran 1 tests in
  62ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; targeted facade green
  passed `Ran 1 tests in 16ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  raw `open_gif_frame` coverage passed `Ran 4 tests in 141ms; Passed: 4,
  Failed: 0, Errors: 0, Skipped: 0`; raw GIF animation filter passed
  `Ran 30 tests in 1156ms; Passed: 30, Failed: 0, Errors: 0, Skipped: 0`;
  facade GIF `save_all` filter passed `Ran 30 tests in 141ms; Passed: 30,
  Failed: 0, Errors: 0, Skipped: 0`; facade Open GIF filter passed
  `Ran 7 tests in 63ms; Passed: 7, Failed: 0, Errors: 0, Skipped: 0`; full
  AHK directory rerun passed `Ran 1205 tests in 3672ms; Passed: 1205, Failed:
  0, Errors: 0, Skipped: 0` with the known non-failing libjpeg stderr
  warnings. No ABI export was added; source and DLL export counts remain
  `341`.
- Latest targeted JPEG verification after `FMT-JPEG-003B`:
  `Ran 22 tests in 2047ms; Passed: 22, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2A`:
  local Pillow 11.3.0 probe for an `8x8` mode `L` fixture with
  `quality=75,optimize=True` showed baseline SOF0, one SOS scan, two compact
  optimized DHT tables, mode `L` reopen, and no progressive SOF2. Native
  `pillow_c_image_save_jpeg_encode_options` now routes `mode=L,optimize=1`
  through a DLL-owned grayscale JPEG encoder with DCT, quantization, optimized
  Huffman table generation, entropy output, JFIF, DQT, SOF0, DHT, SOS, and EOI
  markers. The facade now passes non-metadata JPEG `optimize=True` to the DLL
  instead of pre-rejecting it; unsupported mode/option combinations still fail
  through native status. Red targeted
  failed with raw `Expected 0, got -3` and facade
  `JPEG optimize true is not supported`; Release x64 rebuild succeeded with
  `0 Warning(s), 0 Error(s)`. Targeted raw/facade green passed
  `Ran 2 tests in 266ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`; broader
  JPEG filter passed `Ran 24 tests in 2219ms; Passed: 24, Failed: 0, Errors:
  0, Skipped: 0`; full AHK directory rerun passed `Ran 914 tests in 104578ms;
  Passed: 914, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2B`:
  local Pillow 11.3.0 probe for an `8x8` mode `RGB` fixture with
  `quality=75,optimize=True,subsampling=0` showed baseline SOF0, one SOS scan,
  4:4:4 sampling `[1,1,1,0, 2,1,1,1, 3,1,1,1]`, and four compact optimized
  DHT payloads `18,25,19,26` instead of the standard table sizes. Native
  `pillow_c_image_save_jpeg_encode_options` now routes
  `mode=RGB,optimize=1,subsampling=0` through a DLL-owned baseline JPEG
  encoder with RGB-to-YCbCr conversion, luminance/chrominance quantization,
  four optimized Huffman tables, interleaved Y/Cb/Cr entropy, JFIF, DQT, SOF0,
  DHT, SOS, and EOI markers. Red targeted failed with raw
  `Expected 0, got -3` and facade `pillow_c: invalid argument`; Release x64
  rebuild succeeded with `0 Warning(s), 0 Error(s)`. Targeted raw/facade green
  passed `Ran 2 tests in 234ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed `Ran 26 tests in 2437ms; Passed: 26, Failed: 0,
  Errors: 0, Skipped: 0`; full AHK directory rerun passed
  `Ran 916 tests in 109813ms; Passed: 916, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2C`:
  local Pillow 11.3.0 probe for a `16x16` mode `RGB` fixture with
  `quality=75,optimize=True` showed Pillow default sampling is 4:2:0
  `[1,2,2,0, 2,1,1,1, 3,1,1,1]`, explicit `subsampling=1` writes 4:2:2
  `[1,2,1,0, 2,1,1,1, 3,1,1,1]`, and explicit `subsampling=2` writes 4:2:0,
  all as baseline SOF0 with one SOS and four compact optimized DHT tables.
  Native `pillow_c_image_save_jpeg_encode_options` now routes
  `mode=RGB,optimize=1,subsampling=-1/1/2` through the same generalized
  DLL-owned RGB optimized-Huffman encoder used for 4:4:4, with sampled MCU
  ordering, downsampled chroma planes, luminance/chrominance quantization, four
  optimized Huffman tables, and interleaved entropy. Red targeted failed with
  raw `Expected 0, got -3` and facade `pillow_c: invalid argument`; Release
  x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`. Targeted raw/facade
  green passed `Ran 2 tests in 625ms; Passed: 2, Failed: 0, Errors: 0,
  Skipped: 0`; broader JPEG filter passed `Ran 28 tests in 3062ms; Passed: 28,
  Failed: 0, Errors: 0, Skipped: 0`; full AHK directory rerun passed
  `Ran 918 tests in 107015ms; Passed: 918, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2D`:
  local Pillow 11.3.0 probe for an `8x8` mode `L` fixture with
  `quality=75,progressive=True` and the legacy `progression=True` alias showed
  SOF2, one luminance component, six SOS scans with headers
  `[0,0,0,1]`, `[1,5,0,2]`, `[6,63,0,2]`, `[1,63,2,1]`,
  `[0,0,1,0]`, and `[1,63,1,0]`, compact scan-local DHT tables, and mode `L`
  reopen. Native `pillow_c_image_save_jpeg_encode_options` now routes
  `mode=L,progressive=1,subsampling=-1` through a DLL-owned progressive
  grayscale JPEG encoder with SOF2, DCT, quantization, optimized scan-local
  Huffman tables, progressive DC/AC first and refinement scans, JFIF, DQT, DHT,
  SOS, and EOI markers. The facade now passes non-metadata JPEG
  `progressive=True`/`progression=True` to the DLL instead of pre-rejecting it;
  RGB progressive is covered by `FMT-JPEG-002B2E`; metadata plus progressive,
  qtables, keep_rgb, CMYK save, and YCCK remain deferred. Red targeted failed
  with raw `Expected 0, got -3` and
  facade `JPEG progressive/progression true is not supported`; Release x64
  rebuild succeeded with `0 Warning(s), 0 Error(s)`. Targeted progressive
  filter passed `Ran 4 tests in 454ms; Passed: 4, Failed: 0, Errors: 0,
  Skipped: 0`; broader JPEG filter passed `Ran 30 tests in 3172ms; Passed: 30,
  Failed: 0, Errors: 0, Skipped: 0`; full AHK directory rerun passed
  `Ran 920 tests in 89063ms; Passed: 920, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2E`:
  local Pillow 11.3.0 probe for a `16x16` mode `RGB` fixture with pixels
  `[(x*17+y*11)%256, (x*29+y*7)%256, (x*5+y*37)%256]`,
  `quality=75,progressive=True` showed SOF2, default 4:2:0 sampling,
  explicit `subsampling=0` 4:4:4, explicit `subsampling=1` 4:2:2,
  explicit `subsampling=2` 4:2:0, and ten Pillow-compatible SOS scans. Native
  `pillow_c_image_save_jpeg_encode_options` now routes
  `mode=RGB,progressive=1,subsampling=-1/0/1/2` through a DLL-owned
  progressive RGB JPEG encoder with sampled MCU ordering, scan-local optimized
  Huffman tables, interleaved DC first/refinement scans, component AC
  first/refinement scans, JFIF, DQT, SOF2, DHT, SOS, and EOI markers. The same
  route also accepts `optimize=1` with progressive RGB because the progressive
  encoder already owns its scan-local tables. Red targeted failed with raw
  `Expected 0, got -3` and facade `pillow_c: invalid argument`; Release x64
  rebuild succeeded with `0 Warning(s), 0 Error(s)`. Targeted progressive
  filter passed `Ran 4 tests in 922ms; Passed: 4, Failed: 0, Errors: 0,
  Skipped: 0`; broader JPEG filter passed `Ran 30 tests in 3640ms; Passed: 30,
  Failed: 0, Errors: 0, Skipped: 0`; full AHK directory rerun passed
  `Ran 920 tests in 104797ms; Passed: 920, Failed: 0, Errors: 0, Skipped: 0`.
  Direct local Pillow 11.3.0 reopened a DLL-generated RGB progressive JPEG as
  `RGB (16, 16) JPEG`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2F`:
  local Pillow 11.3.0 probe for a `16x16` mode `RGB` fixture with
  `comment=b"Hello JPEG"`, an eight-byte `icc_profile`, EXIF orientation `6`,
  and `quality=75` showed `optimize=True` writes APP0/JFIF, APP1 EXIF, APP2
  ICC, COM, DQT, SOF0, compact DHT tables, one SOS, and EOI; and
  `progressive=True,optimize=True` writes the same metadata order before DQT,
  SOF2, and ten SOS scans. Native
  `pillow_c_image_save_jpeg_metadata_encode_options` now preserves the
  existing metadata arguments and appends `subsampling`, `progressive`, and
  `optimize`, routing through the selected native encoder before patching
  metadata markers after APP0/JFIF. The facade now routes JPEG metadata saves
  with `progressive`, `progression`, or `optimize` to that export. Red targeted
  failed with raw `Expected pillow_c_image_save_jpeg_metadata_encode_options
  export` and facade `Pillow.Image.Save JPEG optimize with metadata is not
  supported`; Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`.
  Raw targeted green passed `Ran 1 tests in 234ms; Passed: 1, Failed: 0,
  Errors: 0, Skipped: 0`; facade targeted green passed `Ran 1 tests in 297ms;
  Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; broader JPEG filter passed
  `Ran 32 tests in 3813ms; Passed: 32, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory rerun passed `Ran 922 tests in 67782ms; Passed: 922,
  Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2G`:
  local Pillow 11.3.0 probe for a mode `RGB` fixture with
  `quality=75,keep_rgb=True` showed APP14 Adobe transform `0`, no APP0/JFIF,
  SOF component IDs `R/G/B`, all `1x1` sampling with qtable `0`, one DQT
  table, and one baseline SOS. `keep_rgb=True,optimize=True` keeps the same
  marker shape with optimized DHT tables; `keep_rgb=True,progressive=True`
  writes SOF2 with 14 SOS scans; `keep_rgb=True` plus subsampling `1` or `2`
  raises Pillow's codec configuration error. Native
  `pillow_c_image_save_jpeg_encode_keep_rgb_options` now writes bounded
  baseline/optimized/progressive RGB-component JPEGs through the DLL. The
  facade routes non-metadata JPEG `keep_rgb=True` to that export and rejects
  metadata plus `keep_rgb` for this bounded slice. Release x64 rebuild
  succeeded with `0 Warning(s), 0 Error(s)`. Targeted keep-rgb filter passed
  `Ran 2 tests in 609ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed `Ran 34 tests in 4375ms; Passed: 34, Failed: 0,
  Errors: 0, Skipped: 0`; full AHK directory rerun passed
  `Ran 924 tests in 108125ms; Passed: 924, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2H`:
  local Pillow 11.3.0 probe for bounded mode `RGB` custom `qtables` showed
  one or two 64-entry natural-order integer quantization tables are quality
  scaled and written to DQT in zigzag order; table `0` applies to Y and table
  `1` applies to Cb/Cr when two tables are provided. RGB default subsampling
  remains 4:2:0, explicit `subsampling=0` writes 4:4:4, `optimize=True`
  writes compact optimized DHTs, and invalid short qtables raise
  `ValueError Invalid quantization table`. Native
  `pillow_c_image_save_jpeg_qtables_encode_options` now accepts bounded mode
  `RGB`, one or two 64-entry qtables with values `1..255`,
  `optimize == 1`, `progressive != 1`, and subsampling `-1/0/1/2`, writing
  APP0/JFIF, custom DQT payloads, SOF0 table selectors, optimized DHTs, one
  SOS, and EOI through the DLL. The facade parses `QTables`/`qtables` and
  routes non-metadata, non-keep-rgb JPEG qtables saves to that export.
  QTables plus progressive, keep_rgb, metadata, CMYK/YCCK, other modes, and
  exact entropy byte parity remain deferred. Release x64 rebuild succeeded
  with `0 Warning(s), 0 Error(s)`. Targeted qtables green passed
  `Ran 2 tests in 266ms; Passed: 2, Failed: 0, Errors: 0, Skipped: 0`;
  broader JPEG filter passed `Ran 36 tests in 4532ms; Passed: 36, Failed: 0,
  Errors: 0, Skipped: 0`; full AHK directory rerun passed
  `Ran 926 tests in 104844ms; Passed: 926, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2I`:
  local Pillow 11.3.0 probe for bounded mode `RGB` custom `qtables` plus
  `progressive=True` showed Pillow accepts the combination without requiring
  `optimize=True`, writes SOF2, two custom DQT payloads in zigzag order, ten
  SOS scans with the standard RGB progressive scan schedule, default 4:2:0
  sampling, and explicit `subsampling=0` as 4:4:4. Native
  `pillow_c_image_save_jpeg_qtables_encode_options` now routes
  `progressive == 1` through the DLL-owned RGB progressive JPEG encoder with
  the caller's one or two 64-entry natural-order qtables. The facade allows
  qtables when either `optimize=True` or `progressive=True` is set, and still
  rejects qtables plus metadata or keep_rgb for this bounded slice. Raw red
  failed with `Expected 0, got -3`; facade red failed with
  `Pillow.Image.Save JPEG qtables currently requires optimize=True`. Release
  x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`. Targeted raw green
  `Ran 1 tests in 156ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  targeted facade green `Ran 1 tests in 172ms; Passed: 1, Failed: 0, Errors:
  0, Skipped: 0`; qtables filter passed `Ran 4 tests in 344ms; Passed: 4,
  Failed: 0, Errors: 0, Skipped: 0`; broader JPEG filter passed
  `Ran 38 tests in 4781ms; Passed: 38, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory rerun passed `Ran 928 tests in 110797ms; Passed: 928,
  Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2J`:
  local Pillow 11.3.0 probe for bounded mode `RGB` custom `qtables` plus
  `comment`, ICC, and EXIF metadata showed Pillow accepts baseline,
  `optimize=True`, and `progressive=True` combinations; all write APP0/JFIF,
  APP1 EXIF, APP2 ICC, COM, then custom DQT payloads, with default 4:2:0
  sampling and reopened comment/ICC/EXIF orientation metadata. This bounded
  slice covers native optimized and progressive qtables plus metadata
  composition through the new
  `pillow_c_image_save_jpeg_qtables_metadata_encode_options` export. The
  facade routes `qtables` plus metadata when `optimize=True` or
  `progressive=True`, and still rejects qtables without either flag and
  qtables plus `keep_rgb`. Raw red failed with
  `Expected pillow_c_image_save_jpeg_qtables_metadata_encode_options export`;
  facade red failed with
  `Pillow.Image.Save JPEG qtables with metadata is not supported`. Release x64
  rebuild succeeded with `0 Warning(s), 0 Error(s)`. Targeted raw green
  `Ran 1 tests in 281ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  targeted facade green `Ran 1 tests in 281ms; Passed: 1, Failed: 0, Errors:
  0, Skipped: 0`; qtables filter passed `Ran 6 tests in 750ms; Passed: 6,
  Failed: 0, Errors: 0, Skipped: 0`; broader JPEG filter passed
  `Ran 40 tests in 4828ms; Passed: 40, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory rerun passed `Ran 930 tests in 107985ms; Passed: 930,
  Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted JPEG verification after `FMT-JPEG-002B2K`:
  local Pillow 11.3.0 probe for bounded mode `RGB` custom `qtables` plus
  `keep_rgb=True` showed baseline and `optimize=True` output write APP14 Adobe
  transform `0`, no APP0/JFIF, two quality-scaled DQT payloads, SOF0
  component IDs `R/G/B` with qtable selectors `[0,1,1]`, and one SOS;
  `progressive=True` writes SOF2 with the same RGB component/qtable selectors
  and 14 keep-rgb progressive SOS scans; `subsampling=1` raises Pillow's codec
  configuration error. Native
  `pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options` now routes this
  bounded surface through the DLL-owned RGB-component JPEG encoder. The facade
  routes non-metadata `qtables` plus `keep_rgb=True` to that export and still
  rejects metadata plus `keep_rgb`. Raw red failed with
  `Expected pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options export`;
  facade red failed with
  `Pillow.Image.Save JPEG qtables currently requires optimize=True or progressive=True`.
  Release x64 rebuild succeeded with `0 Warning(s), 0 Error(s)`. Targeted raw
  green `Ran 1 tests in 141ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  targeted facade green `Ran 1 tests in 328ms; Passed: 1, Failed: 0, Errors:
  0, Skipped: 0`; qtables filter passed `Ran 8 tests in 1109ms; Passed: 8,
  Failed: 0, Errors: 0, Skipped: 0`; broader JPEG filter passed
  `Ran 42 tests in 5344ms; Passed: 42, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory rerun passed `Ran 932 tests in 104797ms; Passed: 932,
  Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted PNG verification after `FMT-PNG-004L`:
  raw generalized optimize plus text/ICC/EXIF metadata `Ran 1 tests in 109ms;
  Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; facade optimize plus
  metadata batch `Ran 1 tests in 579ms; Passed: 1, Failed: 0, Errors: 0,
  Skipped: 0`; PNG subset `Ran 118 tests in 14203ms; Passed: 118, Failed: 0,
  Errors: 0, Skipped: 0`.
- Latest targeted PNG verification after `FMT-PNG-004M`:
  raw generalized `compress_level=9` plus ordinary text/ICC/EXIF metadata
  `Ran 1 tests in 140ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade `compress_level=9` plus ordinary metadata batch
  `Ran 1 tests in 610ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  PNG subset `Ran 120 tests in 13953ms; Passed: 120, Failed: 0, Errors: 0,
  Skipped: 0`.
- Latest targeted PNG verification after `FMT-PNG-004N`:
  raw generalized `cHRM` plus compressed `tEXt`/uncompressed
  `iTXt`/compressed `iTXt`/language-keyed `iTXt` batch
  `Ran 1 tests in 250ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade `cHRM` plus text-kind batch `Ran 1 tests in 500ms; Passed: 1,
  Failed: 0, Errors: 0, Skipped: 0`; PNG subset `Ran 122 tests in 14641ms;
  Passed: 122, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted PNG verification after `FMT-PNG-004O`:
  raw generalized `optimize=True` plus `cHRM` plus compressed
  `tEXt`/uncompressed `iTXt`/compressed `iTXt`/language-keyed `iTXt`
  batch `Ran 1 tests in 234ms; Passed: 1, Failed: 0, Errors: 0, Skipped:
  0`; facade red before guard narrowing failed with
  `Pillow.Image.Save optimize with pnginfo chunks is not supported`;
  facade green `Ran 1 tests in 500ms; Passed: 1, Failed: 0, Errors: 0,
  Skipped: 0`; PNG subset `Ran 124 tests in 15203ms; Passed: 124, Failed:
  0, Errors: 0, Skipped: 0`.
- Latest targeted PNG verification after `FMT-PNG-004Q`:
  raw generalized `cHRM` plus compressed `tEXt`/uncompressed
  `iTXt`/compressed `iTXt`/language-keyed `iTXt` plus RGB tuple `tRNS`
  batch `Ran 1 tests in 312ms; Passed: 1, Failed: 0, Errors: 0, Skipped:
  0`; facade red before guard narrowing failed with
  `Pillow.Image.Save pnginfo with transparency is not supported`; facade
  green `Ran 1 tests in 703ms; Passed: 1, Failed: 0, Errors: 0, Skipped:
  0`; PNG subset `Ran 126 tests in 16437ms; Passed: 126, Failed: 0,
  Errors: 0, Skipped: 0`.
- Latest targeted PNG verification after `FMT-PNG-004R`:
  raw generalized `cHRM` plus compressed `tEXt`/uncompressed
  `iTXt`/compressed `iTXt`/language-keyed `iTXt` plus RGB `tRNS`
  batch `Ran 1 tests in 297ms; Passed: 1, Failed: 0, Errors: 0, Skipped:
  0`; facade red before guard narrowing failed with
  `Pillow.Image.Save pnginfo with transparency is not supported`; facade
  byte-form transparency green `Ran 1 tests in 828ms; Passed: 1, Failed:
  0, Errors: 0, Skipped: 0`; PNG subset `Ran 127 tests in 18500ms;
  Passed: 127, Failed: 0, Errors: 0, Skipped: 0`.
- Latest targeted PNG verification after `FMT-PNG-004S`:
  local Pillow 11.3.0 probe for explicit safe `cHRM` plus advanced text-kind
  entries and ICC/EXIF metadata showed chunk orders `IHDR`, `iCCP`, `cHRM`,
  text, `IDAT`, `IEND` for ICC-only; `IHDR`, `cHRM`, text, `eXIf`, `IDAT`,
  `IEND` for EXIF-only; and `IHDR`, `iCCP`, `cHRM`, text, `eXIf`, `IDAT`,
  `IEND` for ICC+EXIF. Raw generalized route green
  `Ran 1 tests in 218ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade red before guard narrowing failed with
  `Pillow.Image.Save pnginfo custom chunk with advanced text and other metadata is not supported`;
  facade green `Ran 1 tests in 438ms; Passed: 1, Failed: 0, Errors: 0,
  Skipped: 0`; PNG subset `Ran 129 tests in 18328ms; Passed: 129, Failed: 0,
  Errors: 0, Skipped: 0`; no native source changed and no DLL rebuild was
  required.
- Latest targeted PNG verification after `FMT-PNG-004T`:
  local Pillow 11.3.0 probe for `optimize=True` with explicit safe `cHRM`,
  advanced text-kind entries, and ICC/EXIF metadata showed the same metadata
  ordering as `FMT-PNG-004S` while writing optimized IDAT zlib header
  `[0x78,0xDA]`: ICC-only order `IHDR`, `iCCP`, `cHRM`, text, `IDAT`,
  `IEND`; EXIF-only order `IHDR`, `cHRM`, text, `eXIf`, `IDAT`, `IEND`; and
  ICC+EXIF order `IHDR`, `iCCP`, `cHRM`, text, `eXIf`, `IDAT`, `IEND`. Raw
  generalized route green `Ran 1 tests in 218ms; Passed: 1, Failed: 0,
  Errors: 0, Skipped: 0`; facade red before guard narrowing failed with
  `Pillow.Image.Save optimize with pnginfo chunks is not supported`; facade
  green `Ran 1 tests in 422ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  PNG subset `Ran 131 tests in 20203ms; Passed: 131, Failed: 0, Errors: 0,
  Skipped: 0`; no native source changed and no DLL rebuild was required.
- Latest targeted PNG verification after `FMT-PNG-004U`:
  local Pillow 11.3.0 probe for explicit safe `cHRM`, advanced text-kind
  entries, ICC and/or EXIF, and RGB tuple transparency showed ICC before
  `cHRM`, text before `eXIf`, `tRNS` before `IDAT`, and reopened
  chromaticity, text, ICC, EXIF orientation, and transparency metadata. Raw
  generalized route green `Ran 1 tests in 312ms; Passed: 1, Failed: 0,
  Errors: 0, Skipped: 0`; facade red before guard narrowing failed with
  `Pillow.Image.Save pnginfo with icc_profile and transparency is not supported`;
  facade green `Ran 1 tests in 594ms; Passed: 1, Failed: 0, Errors: 0,
  Skipped: 0`; PNG subset `Ran 133 tests in 18297ms; Passed: 133, Failed: 0,
  Errors: 0, Skipped: 0`; full AHK directory rerun
  `Ran 900 tests in 100594ms; Passed: 900, Failed: 0, Errors: 0, Skipped: 0`;
  no native source changed and no DLL rebuild was required.
- Latest targeted PNG verification after `FMT-PNG-004V`:
  local Pillow 11.3.0 probe for `optimize=True`, explicit safe `cHRM`,
  advanced text-kind entries, ICC and/or EXIF, and RGB tuple transparency
  showed chunk orders `IHDR`, `iCCP`, `cHRM`, text, `tRNS`, `IDAT`, `IEND`
  for ICC-only; `IHDR`, `cHRM`, text, `tRNS`, `eXIf`, `IDAT`, `IEND` for
  EXIF-only; and `IHDR`, `iCCP`, `cHRM`, text, `tRNS`, `eXIf`, `IDAT`,
  `IEND` for ICC+EXIF, with optimized IDAT zlib header `[0x78,0xDA]`.
  Raw generalized route evidence passed
  `Ran 1 tests in 266ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`;
  facade red before guard narrowing failed with
  `Pillow.Image.Save optimize with this PNG option combination is not supported`;
  facade green passed `Ran 1 tests in 515ms; Passed: 1, Failed: 0, Errors:
  0, Skipped: 0`; PNG subset passed `Ran 135 tests in 19766ms; Passed: 135,
  Failed: 0, Errors: 0, Skipped: 0`; full AHK directory rerun passed
  `Ran 908 tests in 109359ms; Passed: 908, Failed: 0, Errors: 0, Skipped: 0`;
  no native source changed, no new ABI export was added, and no DLL rebuild was
  required.
- Latest targeted PNG verification after `FMT-PNG-004W`:
  local Pillow 11.3.0 probe for `compress_level=6`, explicit safe `cHRM`,
  advanced text-kind entries, ICC and/or EXIF, and RGB tuple transparency
  showed chunk orders `IHDR`, `iCCP`, `cHRM`, text, `tRNS`, `IDAT`, `IEND`
  for ICC-only; `IHDR`, `cHRM`, text, `tRNS`, `eXIf`, `IDAT`, `IEND` for
  EXIF-only; and `IHDR`, `iCCP`, `cHRM`, text, `tRNS`, `eXIf`, `IDAT`,
  `IEND` for ICC+EXIF, with IDAT zlib header `[0x78,0x9C]` and no reopened
  `Info["compress_level"]`. Focused raw/facade coverage passed against the
  existing generalized route, so no native or facade implementation change was
  needed. Raw targeted passed `Ran 1 tests in 156ms; Passed: 1, Failed: 0,
  Errors: 0, Skipped: 0`; facade targeted passed `Ran 1 tests in 328ms;
  Passed: 1, Failed: 0, Errors: 0, Skipped: 0`; PNG subset passed
  `Ran 137 tests in 10547ms; Passed: 137, Failed: 0, Errors: 0, Skipped: 0`;
  full AHK directory rerun passed `Ran 910 tests in 57375ms; Passed: 910,
  Failed: 0, Errors: 0, Skipped: 0`; no native source changed, no new ABI
  export was added, and no DLL rebuild was required.
- Latest targeted JPEG verification after the `FMT-JPEG-003C` explicit
  metadata keep extension:
  local Pillow 11.3.0 probe for an opened RGB JPEG containing COM, ICC, and
  EXIF showed that `quality="keep"` plus explicit `icc_profile` and/or `exif`
  still preserves the opened COM/comment when `comment` is omitted, while an
  explicit empty comment drops the COM. The existing raw DLL qtables metadata
  export already writes caller-supplied COM/ICC/EXIF markers; the facade now
  maps the opened JPEG comment into that route for keep-style explicit
  ICC/EXIF saves without AHK pixel loops. No native source changed, no Release
  x64 rebuild was required, and no ABI export was added. Red evidence:
  facade failed `Expected value to be true` on the missing COM while raw route
  passed. Green evidence: targeted `explicit ICC EXIF` passed `2/2` in
  `422ms`; broader JPEG filter passed `134/134` in `27109ms`; single full
  directory invocation timed out at 120s; split full-file verification passed
  `553/553` facade tests in `109781ms` and `546/546` raw DLL tests in
  `42516ms`, with the same non-failing libjpeg stderr warnings.
- Latest targeted JPEG verification after the `FMT-JPEG-003C`
  `subsampling="keep"` extension:
  local Pillow 11.3.0 probe showed that opened RGB JPEGs saved with
  `subsampling=0`, `1`, or `2`, then re-saved with
  `quality="keep", subsampling="keep"`, preserve the source SOF sampling,
  source DQT tables, and opened COM/comment bytes. The implemented bounded
  public surface covers opened RGB `quality="keep"` plus `subsampling="keep"`:
  `ahk/pillow.ahk` resolves the keep sentinel through the existing native JPEG
  subsampling metadata and routes through the existing native qtables encoder.
  No native source changed, no Release x64 rebuild was required, and no ABI
  export was added. Red evidence: facade `subsampling keep` failed on the old
  parser guard at `ahk/pillow.ahk:5484`. Green evidence: targeted facade
  `subsampling keep` passed `1/1` in `282ms`; targeted raw
  `native subsampling metadata` passed `1/1` in `125ms`; broader JPEG filter
  passed `136/136` in `26953ms`; single full-directory invocation timed out at
  120s; split full-file verification passed `554/554` facade tests in
  `91125ms` and `547/547` raw DLL tests in `41015ms`, with the same
  non-failing libjpeg stderr warnings.
- Latest targeted JPEG verification after the `FMT-JPEG-003C` explicit
  quality `subsampling="keep"` metadata extension:
  local Pillow 11.3.0 probe showed that opened RGB JPEGs saved with
  `quality=70`, `subsampling=2`, COM, ICC, and EXIF, then re-saved with
  `quality=80`, `subsampling="keep"`, explicit `icc_profile`, explicit `exif`,
  and omitted `comment`, preserve source 4:2:0 SOF sampling, preserve opened
  COM/comment, preserve caller ICC/EXIF, and recompute DQT tables for the
  explicit quality. The facade now treats `subsampling="keep"` like the other
  keep-style metadata precedence routes by supplying the opened COM to the
  existing native JPEG metadata subsampling route only when `comment` is
  omitted. No native source changed, no Release x64 rebuild was required, and
  no ABI export was added. Red evidence: facade targeted failed on a missing
  COM marker with `Expected value to be true` at `ahk/pillow.test.ahk:7496`.
  Green evidence: targeted facade filter passed `1/1` in `297ms`; targeted
  raw metadata subsampling filter passed `1/1` in `140ms`; broader JPEG filter
  passed `138/138` in `28094ms`; single full-directory invocation timed out at
  120s; split full-file verification passed `555/555` facade tests in
  `111391ms` and `548/548` raw DLL tests in `41234ms`, with the same
  non-failing libjpeg stderr warnings.
- Previous targeted JPEG verification after the `FMT-JPEG-002A` subsampling
  preset alias extension:
  local Pillow 11.3.0 probe showed that `"4:1:1"`, `"web_low"`,
  `"web_medium"`, `"low"`, and `"medium"` write 4:2:0 SOF sampling; and
  `"web_high"`, `"web_very_high"`, `"web_maximum"`, `"high"`, and
  `"maximum"` write 4:4:4 SOF sampling. Public `"keep"` on a non-opened/new
  image remains an error. `ahk/pillow.ahk` now normalizes those preset aliases
  to the existing native integer subsampling route, so the DLL still owns
  image bytes and encoding without a new ABI export. No native source changed,
  no Release x64 rebuild was required, and no ABI export was added. Red
  evidence: targeted facade failed on the old parser guard at
  `ahk/pillow.ahk:5487`. Green evidence: targeted facade filter passed `1/1`
  in `281ms`; broader JPEG filter passed `139/139` in `27750ms`; single
  full-directory invocation timed out at 120s; split full-file verification
  passed `556/556` facade tests in `108062ms` and `548/548` raw DLL tests in
  `39563ms`, with the same non-failing libjpeg stderr warnings.

## Current Phase Goal

Goal: turn `pillow_c.dll` plus `ahk/pillow.ahk` into a high-performance,
AHK-first Pillow-compatible runtime for the common scripting surface before
chasing full plugin parity.

Next execution target:

1. Move to JPEG codec strategy work instead of continuing PNG one-combination
   tails or speculative GIF disposal/background matrices.
2. Continue `FMT-JPEG-002B2` after `FMT-JPEG-002B2N`: bounded mode `L`
   `optimize=True`, RGB `optimize=True` with default/0/1/2 subsampling, mode
   `L` `progressive=True`, and RGB `progressive=True` with default/0/1/2
   subsampling now have real native JPEG encoder coverage, RGB metadata plus
   native optimize/progressive routing is covered, bounded RGB `keep_rgb=True`
   baseline/optimized/progressive output is covered, and bounded RGB
   `qtables` with default-Huffman, `optimize=True`, or `progressive=True` is
   covered, including default/optimized/progressive metadata and `qtables`
   plus `keep_rgb=True`; mode `L` custom qtables are covered for
   default-Huffman, optimized, progressive, and metadata output; metadata plus
   `keep_rgb=True` is covered for baseline, optimized, and progressive RGB
   component output.
3. Defer CMYK metadata-plus-advanced tails, CMYK qtables plus advanced
   options, and broader real YCCK fixtures until the next JPEG codec-strategy
   decision. `FMT-JPEG-003D`, `FMT-JPEG-002B2O`, `FMT-JPEG-003E`, and
   `FMT-JPEG-003G` now cover bounded baseline CMYK save, CMYK qtables, CMYK
   optimize, and CMYK progressive without relying on WIC's incompatible CMYK
   encoder; `FMT-JPEG-003K` covers bounded baseline CMYK DPI/JFIF.
4. Add focused benchmarks only after the corresponding correctness slice is
   stable; performance claims must be backed by repeatable numbers.
5. Keep each slice native-first: AHK normalizes arguments and lifetimes, while
   image allocation, bytes, transforms, codecs, and hot loops stay in the DLL.

## Current Recommended Gap

```text
ID: FMT-JPEG-002B2
Area: JPEG
Status: partial
Gap: Extend the native JPEG codec strategy beyond the covered
     `FMT-JPEG-002B2A` mode `L` and `FMT-JPEG-002B2B` RGB 4:4:4
     optimized-Huffman paths plus `FMT-JPEG-002B2C` RGB default/4:2:2/4:2:0
     optimized-Huffman output, `FMT-JPEG-002B2D` mode `L` SOF2 progressive
     output, `FMT-JPEG-002B2E` RGB SOF2 progressive output, and
     `FMT-JPEG-002B2F` metadata plus native optimize/progressive routing, and
     `FMT-JPEG-002B2G` bounded RGB keep_rgb output, and
     `FMT-JPEG-002B2H` bounded RGB qtables plus optimize output, and
     `FMT-JPEG-002B2I` bounded RGB qtables plus progressive output, and
     `FMT-JPEG-002B2J` bounded RGB qtables plus metadata for optimized and
     progressive output, `FMT-JPEG-002B2K` bounded RGB qtables plus keep_rgb
     output, `FMT-JPEG-002B2L` bounded RGB qtables default-Huffman output
     with and without metadata, and `FMT-JPEG-002B2M` bounded mode `L` qtables
     default-Huffman/optimized/progressive output with baseline metadata, and
     `FMT-JPEG-002B2N` bounded RGB metadata plus keep_rgb output, and
     `FMT-JPEG-003D` bounded baseline CMYK save, `FMT-JPEG-002B2O` bounded
     baseline CMYK qtables, `FMT-JPEG-003E` bounded CMYK optimize,
     `FMT-JPEG-003F` bounded CMYK metadata, `FMT-JPEG-003G` bounded CMYK
     progressive, `FMT-JPEG-003H` bounded CMYK metadata plus optimize, and
     `FMT-JPEG-003I` bounded CMYK metadata plus progressive, and
     `FMT-JPEG-003J` bounded CMYK qtables plus metadata, and
     `FMT-JPEG-003K` bounded baseline CMYK DPI/JFIF. Later slices also cover
     the CMYK qtables/subsampling/DPI family through `FMT-JPEG-003AH` and the
     real YCCK fixture `quality="keep"` normalization under `FMT-JPEG-003C`.
     Remaining adjacent JPEG surface is broader `quality="keep"` /
     `qtables="keep"` preservation, alternate YCCK fixtures, and codec
     strategy semantics.
Start in code/tests: JPEG encoder abstraction in `src/pillow_c.cpp`, existing
                     JPEG marker helpers/tests, and facade JPEG save option
                     routing only after the native path can write the markers.
Done when: the next bounded Pillow 11.3.0 oracle fixture proves the selected
           codec can open or write broader CMYK/YCCK markers without AHK pixel
           loops, or the ledger records the explicit dependency decision and
           blocked surface without pretending WIC output is compatible.
```

Resolved JPEG facts from `FMT-JPEG-001`:

- Pillow 11.3.0 writes bounded JPEG metadata in marker order `APP0/JFIF`,
  `APP1/Exif`, `APP2/ICC_PROFILE`, `COM`, then image tables for the covered
  RGB fixture with `quality=95`, `dpi=(300,150)`, `comment=b"Hello JPEG"`,
  an 8-byte ICC profile, and EXIF orientation `6`.
- Native `pillow_c_image_save_jpeg_metadata_options` saves through the existing
  WIC JPEG path, patches JFIF density first when requested, then inserts APP1,
  single-segment APP2 ICC, and COM markers without AHK pixel loops.
- Native `pillow_c_image_open_jpeg` now scans and stores bounded JPEG comment,
  single-segment ICC, and EXIF bytes on the image handle; new
  `pillow_c_image_metadata_jpeg_comment`,
  `pillow_c_image_metadata_jpeg_icc_profile`, and
  `pillow_c_image_metadata_jpeg_exif` exports expose those bytes.
- `ahk/pillow.ahk` routes JPEG `comment`/`Comment`,
  `icc_profile`/`IccProfile`, and `exif`/`Exif` save options to the DLL and
  maps reopened JPEG metadata to `Info["comment"]`, `Info["icc_profile"]`,
  and `Info["exif"]` as `Buffer` values. `FMT-JPEG-001A` covers bounded
  multi-segment ICC split/reassembly. Full malformed EXIF behavior, marker
  preservation across arbitrary source JPEGs, ICC color-management behavior,
  and progressive/optimize remain deferred.
- `FMT-JPEG-002A` covers bounded RGB JPEG save-side subsampling. Pillow 11.3.0
  writes SOF0 sampling triples for `subsampling=0` / `"4:4:4"` plus
  `"web_high"`, `"web_very_high"`, `"web_maximum"`, `"high"`, and
  `"maximum"` as Y `1x1`, Cb `1x1`, Cr `1x1`; `subsampling=1` / `"4:2:2"`
  as Y `2x1`, Cb `1x1`, Cr `1x1`; and `subsampling=2` / `"4:2:0"`,
  `"4:1:1"`, `"web_low"`, `"web_medium"`, `"low"`, and `"medium"` as
  Y `2x2`, Cb `1x1`, Cr `1x1`. Native
  `pillow_c_image_save_jpeg_subsampling_options` uses WIC's
  `JpegYCrCbSubsampling` encoder option for these values, and
  `pillow_c_image_save_jpeg_metadata_subsampling_options` combines that path
  with bounded COM/ICC/EXIF marker patching. The facade routes JPEG
  `subsampling`/`Subsampling` integer and string aliases without AHK pixel
  loops.
- `FMT-JPEG-002B1` covers the historical explicit JPEG progressive/optimize
  option boundary for the WIC-backed encoder. Pillow 11.3.0 writes
  `progressive=True` and `progression=True` as SOF2 progressive JPEGs with
  ten SOS scans on the bounded RGB fixture; `optimize=True` stays SOF0 with
  one SOS scan and optimized entropy tables. Later native codec slices now
  cover optimized-Huffman output plus bounded mode `L` and `RGB` progressive
  output, while other advanced true-option surfaces still fail with `-3`
  instead of silently writing the wrong JPEG.
- `FMT-JPEG-002B2A` covers the first real native optimized-Huffman JPEG encoder
  slice: for a bounded mode `L` `8x8` fixture with `quality=75,optimize=True`,
  the DLL writes baseline SOF0, one SOS scan, compact optimized DHT tables, and
  a decodable grayscale JPEG without AHK pixel loops.
- `FMT-JPEG-002B2B` extends the real native optimized-Huffman encoder to a
  bounded RGB `8x8` fixture with `quality=75,optimize=True,subsampling=0`:
  the DLL writes baseline SOF0, 4:4:4 sampling, one SOS scan, separate luma and
  chroma quantization tables, four compact optimized DHT tables, and interleaved
  Y/Cb/Cr entropy without AHK pixel loops.
- `FMT-JPEG-002B2C` generalizes that RGB optimized-Huffman encoder to Pillow's
  default 4:2:0 sampling plus explicit `subsampling=1` / 4:2:2 and
  `subsampling=2` / 4:2:0 on a bounded `16x16` RGB fixture. The DLL writes the
  expected SOF0 sampling, one SOS scan, four compact optimized DHT tables, and
  decodable RGB output without AHK pixel loops.
- `FMT-JPEG-002B2D` adds real mode `L` progressive JPEG save coverage:
  `progressive=True` and `progression=True` write SOF2 with Pillow-compatible
  six-scan grayscale headers and decodable mode `L` output without AHK pixel
  loops.
- `FMT-JPEG-002B2E` adds real mode `RGB` progressive JPEG save coverage:
  `progressive=True` with default/0/1/2 subsampling writes SOF2 with
  Pillow-compatible ten-scan RGB headers, expected Y/Cb/Cr sampling factors,
  and decodable RGB output without AHK pixel loops.
- `FMT-JPEG-002B2F` adds RGB JPEG metadata plus native encoder composition:
  bounded `comment`, `icc_profile`, and `exif` saves now combine with
  `optimize=True` baseline output or `progressive=True,optimize=True` SOF2
  output through `pillow_c_image_save_jpeg_metadata_encode_options`, preserving
  APP1/APP2/COM marker order after APP0/JFIF and reopened metadata without AHK
  pixel loops.
- `FMT-JPEG-002B2G` adds bounded RGB `keep_rgb=True` JPEG save coverage:
  native save writes Pillow-compatible APP14 Adobe transform `0`, no JFIF,
  SOF0/SOF2 component IDs `R/G/B`, all `1x1` sampling, qtable `0`, one
  quantization table, optimized DHTs when requested, and 14 progressive scans
  for the progressive route. The facade routes non-metadata `keep_rgb=True`
  to the DLL; metadata plus `keep_rgb` is covered later by `FMT-JPEG-002B2N`.
- `FMT-JPEG-002B2H` adds bounded RGB JPEG `qtables` plus `optimize=True`
  coverage: native save accepts one or two natural-order 64-entry integer
  tables, quality-scales them, writes custom DQT payloads in zigzag order,
  preserves default/0/1/2 RGB subsampling, writes optimized DHTs, and routes
  non-metadata, non-keep-rgb facade `qtables` saves to the DLL. QTables plus
  keep_rgb is covered later by `FMT-JPEG-002B2K`; metadata, CMYK/YCCK, and
  other modes remain future JPEG codec-strategy surfaces.
- `FMT-JPEG-002B2I` adds bounded RGB JPEG `qtables` plus `progressive=True`
  coverage: native save uses the same custom qtable scaling and DQT payloads
  in the RGB progressive encoder, writes SOF2 with Pillow-compatible ten-scan
  RGB progressive SOS headers, preserves default/0/1/2 subsampling, and the
  facade routes qtables saves when `progressive=True` even if `optimize` is
  unset. QTables plus keep_rgb is covered later by `FMT-JPEG-002B2K`;
  metadata, CMYK/YCCK, and other modes remain future JPEG codec-strategy
  surfaces.
- `FMT-JPEG-002B2J` adds bounded RGB JPEG `qtables` plus metadata coverage
  for the optimized and progressive native qtables routes: native save writes
  custom DQT payloads through the qtables encoder, then patches APP1 EXIF,
  APP2 ICC, and COM markers after APP0/JFIF in Pillow order; reopened JPEGs
  expose comment, ICC, and EXIF orientation metadata without AHK byte loops.
  QTables plus keep_rgb is covered by `FMT-JPEG-002B2K`; default-Huffman
  qtables with metadata is covered by `FMT-JPEG-002B2L`; mode `L` qtables are
  covered by `FMT-JPEG-002B2M`; qtables beyond mode `L`/`RGB`, CMYK/YCCK, and
  exact entropy byte parity remain future surfaces.
- `FMT-JPEG-002B2K` adds bounded RGB JPEG `qtables` plus `keep_rgb=True`
  coverage: native save accepts one or two custom natural-order 64-entry
  integer qtables, quality-scales them, writes APP14/no-JFIF RGB-component
  JPEGs with DQT table `0` for R and table `1` for G/B when present, supports
  baseline, optimized, and 14-scan progressive keep-rgb output, and rejects
  unsupported keep-rgb subsampling values without AHK pixel loops.
- `FMT-JPEG-002B2L` adds bounded RGB JPEG `qtables` default-Huffman coverage:
  the existing qtables and qtables-metadata exports now accept baseline
  non-progressive saves when `optimize` is false/default, write standard DHT
  payload lengths `[29,179,29,179]`, preserve custom DQT payloads and default
  4:2:0 sampling, and route facade saves without a pre-rejection. The metadata
  variant preserves APP1/APP2/COM order before DQT without AHK byte loops.
- `FMT-JPEG-002B2M` adds bounded mode `L` JPEG `qtables` coverage through the
  existing qtables and qtables-metadata exports: native save accepts one or two
  caller tables but writes only quality-scaled DQT table `0`, matching Pillow's
  ignored-second-table behavior for grayscale output. Baseline output writes
  SOF0, standard DHT lengths `[29,179]`, and one SOS; `optimize=True` writes
  compact optimized DHTs; `progressive=True` writes SOF2 with six grayscale
  SOS scans; metadata baseline output preserves APP1/APP2/COM order before
  DQT. Raw DLL and facade tests cover the public route without AHK pixel loops.
- `FMT-JPEG-002B2N` adds bounded RGB JPEG `keep_rgb=True` plus metadata
  coverage through `pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options`:
  native save writes APP14/no-JFIF RGB-component JPEGs, then patches APP1 EXIF,
  APP2 ICC, and COM after APP14 and before DQT. Baseline and optimized output
  use SOF0 with one SOS; progressive output uses SOF2 with 14 keep-rgb scans;
  reopened images expose comment, ICC, EXIF, and orientation metadata.
- `FMT-JPEG-003A` covers one bounded CMYK JPEG open fixture generated by
  Pillow 11.3.0 from a `2x1` mode `CMYK` image. The file has four SOF0
  components `C/M/Y/K` and Adobe APP14 transform `0`; Pillow reopens it as
  `CMYK` with exact bytes `[0,64,128,255,10,20,30,40]`. Native
  `pillow_c_image_open_jpeg` now maps four-component JPEGs to mode `CMYK` and
  decodes through WIC `32bppCMYK`; `ahk/pillow.ahk` exposes the reopened mode
  and bytes without AHK pixel loops.
- `FMT-JPEG-003B` covers the historical CMYK JPEG save boundary. Pillow 11.3.0
  saves the bounded `2x1` CMYK fixture as APP14 Adobe transform `0` with four
  SOF0 `C/M/Y/K` components using `1x1` sampling and qtable `0`. WIC's CMYK
  encoder instead writes an incompatible marker/sampling shape.
- `FMT-JPEG-003D` covers bounded baseline CMYK JPEG save at `quality=95`
  through the native encoder, with Pillow's `CMYK;I` sample inversion, APP14
  transform `0`, SOF0 `C/M/Y/K`, standard luminance DHTs, and exact mode
  `CMYK` reopen bytes. `FMT-JPEG-003F` covers baseline CMYK metadata,
  `FMT-JPEG-003G` covers bounded CMYK progressive output, and
  `FMT-JPEG-003H` covers CMYK metadata plus `optimize=True`;
  `FMT-JPEG-003I` covers CMYK metadata plus `progressive=True`; and
  `FMT-JPEG-003J` covers CMYK qtables plus metadata. Broader YCCK remains a
  codec-strategy gap.

Resolved PNG facts from `FMT-PNG-001A`:

- Pillow 11.3.0 opens PNG `gAMA` as `Image.info["gamma"] == raw_int / 100000`;
  the covered fixture uses raw `45455`, exposed as `0.45455`.
- Pillow 11.3.0 ignores `Image.save(..., gamma=...)`; save-side `gAMA`
  through `pnginfo`/chunk writing remains deferred.
- Native `pillow_c_image_open_png` scans `gAMA`, the new
  `pillow_c_image_metadata_png_gamma` export exposes the optional double, and
  `ahk/pillow.ahk` maps it to `Info["gamma"]` without AHK pixel loops.
- `FMT-PNG-001T` covers one explicit save-side `gAMA` fixture through
  `PngInfo.add(b"gAMA", struct.pack(">I", 45455))` / `pnginfo`. Pillow
  11.3.0 writes chunk order `IHDR`, `gAMA`, `IDAT`, `IEND`, reopens as mode
  `RGB` with preserved bytes `[10, 20, 30, 40, 50, 60]`, and exposes
  `Image.info["gamma"] == 0.45455`. Native
  `pillow_c_image_save_png_gama_options` writes the four-byte raw integer
  payload before `IDAT`, and `ahk/pillow.ahk` routes the bounded
  `PngInfo.add(Buffer("gAMA"), Buffer(4))` facade case without AHK pixel
  loops. String `"gAMA"` chunk IDs, `after_idat`, text-plus-`gAMA`, and broad
  arbitrary chunk injection remain deferred.
- `FMT-PNG-001U` covers one explicit save-side `cHRM` fixture through
  `PngInfo.add(b"cHRM", struct.pack(">8I", ...))` / `pnginfo`. Pillow
  11.3.0 writes chunk order `IHDR`, `cHRM`, `IDAT`, `IEND`, reopens as mode
  `RGB` with preserved bytes `[10, 20, 30, 40, 50, 60]`, and exposes
  `Image.info["chromaticity"] == [0.3127, 0.329, 0.64, 0.33, 0.3, 0.6,
  0.15, 0.06]`. Native `pillow_c_image_save_png_chunk_options` writes the
  bounded `cHRM` payload through a generic pre-`IDAT` chunk route,
  `pillow_c_image_metadata_png_chromaticity` exposes reopened chromaticity,
  and `ahk/pillow.ahk` routes the bounded
  `PngInfo.add(Buffer("cHRM"), Buffer(32))` facade case without AHK pixel
  loops. String `"cHRM"` chunk IDs, `after_idat`, multiple public/fixed-size
  chunks, arbitrary chunk injection, and metadata/transparency combinations
  remain deferred.
- `FMT-PNG-001V` covers one explicit custom-chunk plus ordinary metadata
  combination: `PngInfo.add(b"cHRM", ...)` plus
  `PngInfo.add_text("Author", "Ada")`. Pillow 11.3.0 writes chunk order
  `IHDR`, `cHRM`, `tEXt`, `IDAT`, `IEND`, reopens as mode `RGB` with
  preserved bytes `[10, 20, 30, 40, 50, 60]`, and exposes both
  `Image.info["chromaticity"] == [0.3127, 0.329, 0.64, 0.33, 0.3, 0.6,
  0.15, 0.06]` and `Image.info["Author"] == "Ada"`. Native
  `pillow_c_image_save_png_text_entries_chunk_options` writes the bounded
  pre-`IDAT` chunk before ordered plain text chunks, and `ahk/pillow.ahk`
  routes this combined `pnginfo` case without AHK pixel loops. Compressed
  text, `iTXt`, ICC/EXIF combinations beyond `FMT-PNG-001X`, transparency,
  multiple public/fixed-size chunks, and `after_idat` remain deferred for
  custom-chunk combinations.
- `FMT-PNG-001W` covers explicit `cHRM` plus ICC metadata:
  `PngInfo.add(b"cHRM", ...)` plus
  `Image.save(..., icc_profile=bytes([1,2,3,4,5,6,7,8]))`. Pillow 11.3.0
  writes chunk order `IHDR`, `iCCP`, `cHRM`, `IDAT`, `IEND`, reopens as mode
  `RGB` with preserved bytes `[10, 20, 30, 40, 50, 60]`, and exposes both
  `Image.info["icc_profile"]` as the caller profile bytes and
  `Image.info["chromaticity"] == [0.3127, 0.329, 0.64, 0.33, 0.3, 0.6,
  0.15, 0.06]`. Native `pillow_c_image_save_png_chunk_icc_options` writes
  the bounded `iCCP` chunk before the bounded `cHRM` chunk and before `IDAT`,
  and `ahk/pillow.ahk` routes this combined `pnginfo` plus `icc_profile` case
  without AHK pixel loops. Text, `iTXt`, transparency, multiple
  public/fixed-size chunks, arbitrary chunks, and `after_idat` remain deferred
  for custom-chunk combinations.
- `FMT-PNG-001X` covers explicit `cHRM` plus EXIF metadata:
  `PngInfo.add(b"cHRM", ...)` plus
  `Image.save(..., exif=Image.Exif().tobytes())`. Pillow 11.3.0 writes chunk
  order `IHDR`, `cHRM`, `eXIf`, `IDAT`, `IEND`, stores the `eXIf` payload
  without the leading `Exif\0\0` header, reopens as mode `RGB` with preserved
  bytes `[10, 20, 30, 40, 50, 60]`, and exposes both Pillow-style
  `Image.info["exif"]` bytes and
  `Image.info["chromaticity"] == [0.3127, 0.329, 0.64, 0.33, 0.3, 0.6,
  0.15, 0.06]`. Native `pillow_c_image_save_png_chunk_exif_options` writes
  the bounded `cHRM` chunk before the bounded `eXIf` chunk and before `IDAT`,
  and `ahk/pillow.ahk` routes this combined `pnginfo` plus `exif` case without
  AHK pixel loops. Text, `iTXt`, transparency, multiple public/fixed-size
  chunks, arbitrary chunks, and `after_idat` remain deferred for custom-chunk
  combinations.
- `FMT-PNG-001Y` covers explicit `cHRM` plus RGB tuple transparency:
  `PngInfo.add(b"cHRM", ...)` plus
  `Image.save(..., transparency=(10,20,30))`. Pillow 11.3.0 writes chunk
  order `IHDR`, `cHRM`, `tRNS`, `IDAT`, `IEND`, stores the truecolor `tRNS`
  payload `[0,10,0,20,0,30]`, reopens as mode `RGB` with preserved bytes
  `[10, 20, 30, 40, 50, 60]`, and exposes both
  `Image.info["transparency"] == [10,20,30]` and
  `Image.info["chromaticity"] == [0.3127, 0.329, 0.64, 0.33, 0.3, 0.6,
  0.15, 0.06]`. Native
  `pillow_c_image_save_png_chunk_rgb_transparency_options` writes the bounded
  `cHRM` chunk before the bounded `tRNS` chunk and before `IDAT`, and
  `ahk/pillow.ahk` routes this combined `pnginfo` plus RGB tuple transparency
  case without AHK pixel loops. Text, `iTXt`, ICC/EXIF, multiple
  public/fixed-size chunks, arbitrary chunks, and `after_idat` remain deferred
  for custom-chunk combinations.
- `FMT-PNG-001Z` covers explicit `cHRM` plus RGB bytes transparency:
  `PngInfo.add(b"cHRM", ...)` plus
  `Image.save(..., transparency=bytes([10,20,30]))`. Pillow 11.3.0 writes
  chunk order `IHDR`, `cHRM`, `tRNS`, `IDAT`, `IEND`, stores the truecolor
  `tRNS` payload `[0,10,0,20,0,30]`, reopens as mode `RGB` with preserved
  bytes `[10, 20, 30, 40, 50, 60]`, and exposes both
  `Image.info["transparency"] == [10,20,30]` and
  `Image.info["chromaticity"] == [0.3127, 0.329, 0.64, 0.33, 0.3, 0.6,
  0.15, 0.06]`. Native
  `pillow_c_image_save_png_chunk_rgb_transparency_bytes_options` accepts
  exactly three RGB bytes, reuses the bounded `cHRM` plus RGB `tRNS` writer,
  and `ahk/pillow.ahk` routes this combined `pnginfo` plus RGB byte
  transparency case without AHK pixel loops. Text, `iTXt`, ICC/EXIF, multiple
  public/fixed-size chunks, arbitrary chunks, and `after_idat` remain deferred
  for custom-chunk combinations.
- `FMT-PNG-004A` replaces the one-export-per-combination PNG metadata save
  growth pattern with `pillow_c_image_save_png_metadata_options`. The facade
  normalizes supported bounded PNG save metadata/options once and passes a
  native metadata plan with flags for explicit `gAMA`, safe custom chunks,
  text ordering before EXIF, optimize, scalar transparency, and RGB
  transparency. Existing narrow PNG save exports remain for ABI stability, but
  new PNG metadata/transparency work should extend the generalized route.
- `FMT-PNG-004B` covers PNG `icc_profile` plus RGB tuple transparency through
  the generalized native metadata route, without adding a new export. Pillow
  11.3.0 writes chunk order `IHDR`, `iCCP`, `tRNS`, `IDAT`, `IEND`, stores
  truecolor `tRNS` bytes `[0,10,0,20,0,30]`, reopens with caller ICC bytes and
  `Info["transparency"] == [10,20,30]`, and converts the matching RGB pixel
  to alpha `0`. The facade now allows only the covered RGB tuple form with
  `icc_profile`; scalar, table, RGB byte-form facade, and broader ICC/EXIF
  transparency combinations remain separate gap IDs.
- `FMT-PNG-004C` covers PNG `exif` plus RGB tuple transparency through the
  generalized native metadata route, without adding a new export. Pillow
  11.3.0 writes chunk order `IHDR`, `tRNS`, `eXIf`, `IDAT`, `IEND`, stores
  truecolor `tRNS` bytes `[0,10,0,20,0,30]`, stores the `eXIf` payload without
  the leading `Exif\0\0` header, reopens with Pillow-style EXIF bytes and
  `Info["transparency"] == [10,20,30]`, and converts the matching RGB pixel
  to alpha `0`. The facade allows only the covered RGB tuple form with `exif`;
  scalar/table/RGB byte-form EXIF transparency, `pnginfo` plus EXIF plus
  transparency, and other unprobed EXIF transparency forms remain separate gap
  IDs.
- `FMT-PNG-004D` covers PNG `icc_profile` plus `exif` plus RGB tuple
  transparency through the generalized native metadata route, without adding a
  new export. Pillow 11.3.0 writes chunk order `IHDR`, `iCCP`, `tRNS`, `eXIf`,
  `IDAT`, `IEND`, stores truecolor `tRNS` bytes `[0,10,0,20,0,30]`, stores
  the `eXIf` payload without the leading `Exif\0\0` header, reopens with ICC
  bytes, Pillow-style EXIF bytes, and `Info["transparency"] == [10,20,30]`,
  and converts the matching RGB pixel to alpha `0`. The facade allows only the
  covered RGB tuple form with ICC plus EXIF; scalar/table/RGB byte-form
  transparency and `pnginfo` plus transparency combinations remain separate
  gap IDs. Native DLL behavior already matched the Pillow probe, so this slice
  required facade routing and tests but no new export or rebuild.
- `FMT-PNG-004E` covers ordinary PNG `PngInfo.add_text("Author", "Ada")` plus
  RGB tuple transparency through the generalized native metadata route, without
  adding a new export. Pillow 11.3.0 writes chunk order `IHDR`, `tEXt`, `tRNS`,
  `IDAT`, `IEND`, stores text payload `Author\0Ada`, stores truecolor `tRNS`
  bytes `[0,10,0,20,0,30]`, reopens with final text metadata in `Info`/`Text`
  and `Info["transparency"] == [10,20,30]`, and converts the matching RGB
  pixel to alpha `0`. Native DLL behavior already matched the Pillow probe, so
  this slice required facade routing and tests but no new export or rebuild.
  `iTXt`, scalar/table/RGB byte-form transparency, and broader
  `pnginfo` plus transparency combinations remain separate gap IDs.
- `FMT-PNG-004F` covers compressed PNG
  `PngInfo.add_text("Note", "Compressed hello", zip=True)` plus RGB tuple
  transparency through the generalized native metadata route, without adding a
  new export. Pillow 11.3.0 writes chunk order `IHDR`, `zTXt`, `tRNS`, `IDAT`,
  `IEND`, stores the zTXt keyword/method prefix `Note\0\0` followed by a zlib
  stream that inflates to `Compressed hello`, stores truecolor `tRNS` bytes
  `[0,10,0,20,0,30]`, reopens with final text metadata in `Info`/`Text` and
  `Info["transparency"] == [10,20,30]`, and converts the matching RGB pixel to
  alpha `0`. Native DLL behavior already matched the Pillow probe, so this
  slice required facade routing and tests but no new export or rebuild.
  `iTXt`, scalar/table/RGB byte-form transparency, and broader `pnginfo` plus
  transparency combinations remain separate gap IDs.
- `FMT-PNG-004G` covers uncompressed PNG
  `PngInfo.add_itxt("Comment", "Cafe " Chr(0x2603))` plus RGB tuple
  transparency through the generalized native metadata route, without adding a
  new export. Pillow 11.3.0 writes chunk order `IHDR`, `iTXt`, `tRNS`, `IDAT`,
  `IEND`, stores the uncompressed iTXt payload `Comment\0\0\0\0\0` followed by
  UTF-8 bytes `[67,97,102,101,32,226,152,131]`, stores truecolor `tRNS` bytes
  `[0,10,0,20,0,30]`, reopens with final text metadata in `Info`/`Text` and
  `Info["transparency"] == [10,20,30]`, and converts the matching RGB pixel to
  alpha `0`. Native DLL behavior already matched the Pillow probe, so this
  slice required facade routing and tests but no new export or rebuild.
  Compressed/language-keyed `iTXt`, scalar/table/RGB byte-form transparency,
  and broader `pnginfo` plus transparency combinations remain separate gap IDs.
- `FMT-PNG-004H` covers compressed PNG
  `PngInfo.add_itxt("CompressedComment", "Zip hello UTF8", "", "", true)` plus
  RGB tuple transparency through the generalized native metadata route, without
  adding a new export. Pillow 11.3.0 writes chunk order `IHDR`, `iTXt`, `tRNS`,
  `IDAT`, `IEND`, stores the compressed iTXt prefix
  `CompressedComment\0\1\0\0\0` followed by a zlib stream that inflates to
  `Zip hello UTF8`, stores truecolor `tRNS` bytes `[0,10,0,20,0,30]`, reopens
  with final text metadata in `Info`/`Text` and
  `Info["transparency"] == [10,20,30]`, and converts the matching RGB pixel to
  alpha `0`. Native DLL behavior already matched the Pillow probe, so this
  slice required facade routing and tests but no new export or rebuild.
  Language-keyed `iTXt`, scalar/table/RGB byte-form transparency,
  and broader `pnginfo` plus transparency combinations remain separate gap IDs.
- `FMT-PNG-004I` covers language-keyed PNG
  `PngInfo.add_itxt("Comment", "Bonjour UTF8", "fr", "Commentaire")` plus
  RGB tuple transparency through the generalized native metadata route, without
  adding a new export. Pillow 11.3.0 writes chunk order `IHDR`, `iTXt`, `tRNS`,
  `IDAT`, `IEND`, stores the iTXt payload prefix
  `Comment\0\0\0fr\0Commentaire\0` followed by UTF-8 bytes for
  `Bonjour UTF8`, stores truecolor `tRNS` bytes `[0,10,0,20,0,30]`, reopens
  with final text metadata in `Info`/`Text` and
  `Info["transparency"] == [10,20,30]`, and converts the matching RGB pixel to
  alpha `0`. Native DLL behavior already matched the Pillow probe, so this
  slice required facade routing and tests but no new export or rebuild.
  Scalar/table/RGB byte-form transparency,
  and broader `pnginfo` plus transparency combinations remain separate gap IDs.
- `FMT-PNG-004J` covers ordinary PNG `PngInfo.add_text("Author", "Ada")` plus
  RGB byte-form transparency through the generalized native metadata route,
  without adding a new export. Pillow 11.3.0 writes chunk order `IHDR`,
  `tEXt`, `tRNS`, `IDAT`, `IEND`, stores text payload `Author\0Ada`, treats
  three transparency bytes as truecolor `tRNS` bytes `[0,10,0,20,0,30]`,
  reopens with text metadata and `Info["transparency"] == [10,20,30]`, and
  converts the matching RGB pixel to alpha `0`. Native DLL behavior already
  matched through the generalized RGB transparency fields; this slice required
  facade byte-form normalization and tests but no new export or rebuild.
  Remaining RGB byte-form metadata combinations should be batched behind
  `FMT-PNG-004K` rather than split into more narrow exports.
- `FMT-PNG-004K` covers a batched set of same-route RGB byte-form transparency
  tails through the generalized native metadata route, without adding a new
  export. Pillow 11.3.0 treats `Buffer([10,20,30])` like the RGB tuple for
  ICC, EXIF, ICC+EXIF, compressed `tEXt`, uncompressed `iTXt`, compressed
  `iTXt`, and language-keyed `iTXt` combinations, writing the same truecolor
  `tRNS` payload `[0,10,0,20,0,30]` in the already-covered tuple chunk order
  for each metadata family. Native DLL behavior already matched through the
  generalized RGB transparency fields; this slice required facade byte-form
  normalization and two batched facade tests but no new export or rebuild.
- `FMT-PNG-004L` covers a batched set of `optimize=True` plus ordinary PNG
  metadata tails through the generalized native metadata route, without adding
  a new export. Pillow 11.3.0 keeps the already-covered metadata chunk orders
  for ordinary `tEXt`, ICC, EXIF, ICC+EXIF, and ordinary `tEXt`+ICC+EXIF while
  writing the optimized IDAT zlib header `[0x78,0xDA]`. Native DLL behavior
  already matched through the generalized optimize flag; this slice required
  facade guard narrowing and raw/facade batch tests but no rebuild.
- `FMT-PNG-004M` covers a batched set of `compress_level=9` plus ordinary PNG
  metadata tails through the generalized native metadata route, without adding
  a new export. Pillow 11.3.0 keeps the already-covered metadata chunk orders
  for ordinary `tEXt`, ICC, EXIF, ICC+EXIF, and ordinary `tEXt`+ICC+EXIF while
  writing the high-compression IDAT zlib header `[0x78,0xDA]`. Native
  `pillow_c_image_save_png_metadata_options` now honors the existing
  `compress_level` argument in the custom writer's zlib header selection
  while keeping image bytes and metadata chunk writing in the DLL. Exact
  compressed byte/size parity and broad compression-strategy parity remain
  deferred.
- `FMT-PNG-004N` covers a batched set of explicit safe `cHRM` custom chunk plus
  advanced text-kind tails through the generalized native metadata route,
  without adding a new export. Pillow 11.3.0 writes chunk order `IHDR`, `cHRM`,
  the selected text chunk, `IDAT`, `IEND` for compressed `tEXt`, uncompressed
  `iTXt`, compressed `iTXt`, and language-keyed `iTXt`; reopening preserves
  RGB bytes, chromaticity metadata, and final text metadata. Native DLL
  behavior already matched through the generalized route; this slice required
  facade guard narrowing and raw/facade batch tests but no rebuild.
- `FMT-PNG-004O` covers `optimize=True` plus explicit safe `cHRM` plus
  advanced text-kind tails through the generalized native metadata route,
  without adding a new export. Pillow 11.3.0 keeps chunk order `IHDR`, `cHRM`,
  the selected `zTXt`/`iTXt` chunk, `IDAT`, `IEND` while writing optimized
  IDAT zlib header bytes `[0x78,0xDA]`. Native DLL behavior already matched
  through the generalized optimize flag and chunk/text-kind fields; this slice
  required facade guard narrowing and raw/facade batch tests but no rebuild.
- `FMT-PNG-004Q` covers explicit safe `cHRM` plus advanced text-kind tails plus
  RGB tuple transparency through the generalized native metadata route, without
  adding a new export. Pillow 11.3.0 writes chunk order `IHDR`, `cHRM`, the
  selected `zTXt`/`iTXt` chunk, `tRNS`, `IDAT`, `IEND` for compressed `tEXt`,
  uncompressed `iTXt`, compressed `iTXt`, and language-keyed `iTXt`; reopening
  preserves RGB bytes, chromaticity metadata, final text metadata, and
  `Info["transparency"] == [10,20,30]`, and native RGB-to-RGBA conversion
  applies alpha to matching pixels. Native DLL behavior already matched through
  the generalized route; this slice required facade guard narrowing and
  raw/facade batch tests but no rebuild.
- `FMT-PNG-004R` covers the matching RGB byte-form transparency tail for
  explicit safe `cHRM` plus advanced text-kind tails through the generalized
  native metadata route, without adding a new export. The facade normalizes
  `Buffer(3)` transparency into the existing RGB channel arguments; native PNG
  writing remains in the DLL and no rebuild was required in the completion
  pass.
- `FMT-PNG-004S` covers explicit safe `cHRM` plus advanced text-kind tails with
  ICC and/or EXIF through the generalized native metadata route, without
  adding a new export. Pillow 11.3.0 writes ICC before `cHRM`, writes the
  selected `zTXt`/`iTXt` text chunk before `eXIf`, and reopens with
  chromaticity, text, ICC, EXIF, and orientation metadata. Native DLL behavior
  already matched through the generalized route; this slice required facade
  guard narrowing and raw/facade batch tests but no rebuild.
- `FMT-PNG-004T` covers `optimize=True` with explicit safe `cHRM` plus
  advanced text-kind tails and ICC and/or EXIF through the generalized native
  metadata route, without adding a new export. Pillow 11.3.0 preserves ICC
  before `cHRM`, selected text before `eXIf`, and optimized IDAT zlib header
  `[0x78,0xDA]`; reopening preserves chromaticity, text, ICC, EXIF, and
  orientation metadata. Native DLL behavior already matched through the
  generalized route; this slice required facade guard narrowing and
  raw/facade batch tests but no rebuild.
- `FMT-PNG-001B` covers one uncompressed PNG `tEXt` chunk on open. Pillow
  11.3.0 exposes `tEXt` as ordinary string entries in `Image.info`; the
  covered fixture maps keyword `Author` to value `Ada`.
- `FMT-PNG-001C` covers one uncompressed PNG `iTXt` chunk on open. Pillow
  11.3.0 exposes `iTXt` values as string-like entries in `Image.info`; the
  covered fixture maps keyword `Comment` to value `Hello UTF8`.
- `FMT-PNG-001D` covers one compressed PNG `zTXt` chunk on open. Pillow 11.3.0
  exposes `zTXt` values as ordinary string entries in `Image.info`; the
  covered fixture maps keyword `Note` to value `Compressed hello`.
- `FMT-PNG-001E` covers one compressed PNG `iTXt` chunk on open. Pillow 11.3.0
  exposes compressed `iTXt` values as `PngImagePlugin.iTXt`, string-compatible
  entries in `Image.info`; the covered fixture maps keyword
  `CompressedComment` to value `Zip hello UTF8`.
- `FMT-PNG-001AB` extends the same open-side compressed text path to
  dynamic-Huffman zlib payloads for both `zTXt` and compressed `iTXt`.
  Pillow 11.3.0 exposes `DynamicNote` and `DynamicComment` as
  string-compatible values and preserves the grayscale pixel byte `[7]`.
  Native open now decodes those dynamic deflate blocks itself and strips
  compressed text chunks only from the WIC decode copy to avoid a WIC hang; the
  original file remains the source for native text metadata.
- `FMT-PNG-001F` covers duplicate PNG `tEXt` keyword open semantics. Pillow
  11.3.0 preserves both file chunks but exposes only the final value through
  `Image.info["Author"]` and `Image.text["Author"]`; the covered fixture maps
  duplicate keyword `Author` to final value `Grace`.
- `FMT-PNG-001G` covers one save-side uncompressed PNG `tEXt` chunk through
  `PngInfo().add_text("Author", "Ada")` / `pnginfo`. Pillow 11.3.0 writes a
  `tEXt` payload `Author\0Ada` between `IHDR` and `IDAT`, reopens as mode
  `RGB` with preserved bytes `[10, 20, 30]`, and exposes both
  `Image.info["Author"] == "Ada"` and `Image.text["Author"] == "Ada"`.
- `FMT-PNG-001H` covers duplicate save-side uncompressed PNG `tEXt` chunks
  through two ordered `PngInfo().add_text("Author", ...)` entries. Pillow
  11.3.0 writes two `tEXt` chunks before `IDAT`, with payloads `Author\0Ada`
  then `Author\0Grace`, reopens as mode `RGB` with preserved bytes
  `[10, 20, 30]`, and exposes final-value metadata
  `Image.info["Author"] == "Grace"` and `Image.text["Author"] == "Grace"`.
- `FMT-PNG-001I` covers one save-side compressed PNG `zTXt` chunk through
  `PngInfo().add_text("Note", "Compressed hello", zip=True)` / `pnginfo`.
  Pillow 11.3.0 writes a `zTXt` chunk between `IHDR` and `IDAT`, with payload
  prefix `Note\0\0` followed by a zlib stream that decompresses to
  `Compressed hello`, reopens as mode `RGB` with preserved bytes
  `[10, 20, 30]`, and exposes both
  `Image.info["Note"] == "Compressed hello"` and
  `Image.text["Note"] == "Compressed hello"`.
- `FMT-PNG-001J` covers one save-side uncompressed PNG `iTXt` chunk through
  `PngInfo().add_itxt("Comment", value)` / `pnginfo`, where `value` is
  `"Cafe " Chr(0x2603)` in AHK. Pillow 11.3.0 writes an `iTXt` payload
  `Comment\0\0\0\0\0` followed by UTF-8 bytes
  `[67, 97, 102, 101, 32, 226, 152, 131]`, reopens as mode `RGB` with
  preserved bytes `[10, 20, 30]`, and exposes both
  `Image.info["Comment"] == value` and `Image.text["Comment"] == value`.
- `FMT-PNG-001K` covers one save-side PNG ICC profile fixture through
  `Image.save(..., icc_profile=bytes([1,2,3,4,5,6,7,8]))`. Pillow 11.3.0
  writes chunk order `IHDR`, `iCCP`, `IDAT`, `IEND`; the `iCCP` payload uses
  keyword `ICC Profile`, compression method `0`, and a zlib stream that
  decompresses to the original profile bytes. Reopening preserves mode `RGB`,
  preserves bytes `[10, 20, 30]`, and exposes
  `Image.info["icc_profile"]` as the original bytes.
- `FMT-PNG-001L` covers one save-side PNG EXIF fixture through
  `Image.save(..., exif=Image.Exif().tobytes())`, where the EXIF contains
  orientation tag `6`. Pillow 11.3.0 writes chunk order
  `IHDR`, `eXIf`, `IDAT`, `IEND`; the `eXIf` payload is the TIFF payload
  without the leading `Exif\0\0` bytes. Reopening preserves mode `RGB`,
  preserves bytes `[10, 20, 30]`, restores `Image.info["exif"]` with the
  `Exif\0\0` header, and reports orientation `6` through `getexif()`.
- Native `pillow_c_image_open_png` scans `tEXt`, bounded `zTXt`, plus
  uncompressed and bounded compressed `iTXt`, and the existing
  `pillow_c_image_metadata_png_text_count` plus
  `pillow_c_image_metadata_png_text` exports expose indexed UTF-8 key/value
  pairs. The raw ABI preserves duplicate chunk order, while
  `ahk/pillow.ahk` maps PNG text into both `Info` and `Text` with Pillow-like
  last-value-wins behavior for duplicate keys.
- Native `pillow_c_image_save_png_text_options` writes one ASCII `tEXt` chunk,
  and `pillow_c_image_save_png_text_entries_options` writes ordered multiple
  ASCII `tEXt` chunks through the native PNG writer.
  `pillow_c_image_save_png_text_entries_ex_options` additionally accepts
  per-entry compression flags and writes bounded ASCII `zTXt` chunks with
  stored zlib payloads for compressed entries.
  `pillow_c_image_save_png_text_entries_kind_options` additionally accepts
  per-entry text kinds and writes UTF-8 `iTXt` chunks with empty language and
  translated-keyword fields for `PngInfo.add_itxt(...)`; `FMT-PNG-001N` adds
  compressed `iTXt` save through the same export by honoring the existing
  per-entry compression flag.
  `pillow_c_image_save_png_text_entries_itxt_options` adds per-entry language
  and translated-keyword arrays for bounded `PngInfo.add_itxt(..., lang,
  tkey)` saves.
  `ahk/pillow.ahk` exposes the bounded facade route through
  `Pillow.PngImagePlugin.PngInfo().add_text(...)`,
  `Pillow.PngImagePlugin.PngInfo().add_itxt(...)`, and the PNG `pnginfo` save
  option.
- Native `pillow_c_image_save_png_icc_options` writes the bounded `iCCP` chunk
  before `IDAT`, and `pillow_c_image_metadata_png_icc_profile` exposes the
  decompressed profile bytes on open. `ahk/pillow.ahk` routes PNG
  `icc_profile`/`IccProfile` saves to the DLL and maps reopened PNG ICC
  metadata into `Info["icc_profile"]` as a `Buffer`.
- `FMT-PNG-001P` covers PNG `pnginfo` plus `icc_profile` save on a bounded RGB
  fixture. Pillow 11.3.0 writes chunk order `IHDR`, `iCCP`, `tEXt`, `IDAT`,
  `IEND`; reopens with preserved RGB bytes, final text metadata
  `Info["Author"] == "Ada"` / `Text["Author"] == "Ada"`, and
  `Info["icc_profile"]` equal to the caller bytes. Native
  `pillow_c_image_save_png_text_entries_icc_options` writes both metadata
  groups through the DLL, and `ahk/pillow.ahk` routes ordinary `pnginfo`
  text entries plus `icc_profile` without AHK pixel loops. ICC combinations
  with transparency, language-keyed `iTXt` with ICC, `gAMA` combinations,
  and ImageCms behavior remain deferred.
- `FMT-PNG-001Q` covers PNG `pnginfo` plus `exif` save on a bounded RGB
  fixture. Pillow 11.3.0 writes chunk order `IHDR`, `tEXt`, `eXIf`, `IDAT`,
  `IEND`; reopens with preserved RGB bytes, final text metadata
  `Info["Author"] == "Ada"` / `Text["Author"] == "Ada"`, Pillow-style
  `Info["exif"]` bytes, and EXIF orientation `6`. Native
  `pillow_c_image_save_png_text_entries_exif_options` writes both metadata
  groups through the DLL, and `ahk/pillow.ahk` routes ordinary `pnginfo`
  text entries plus `exif` without AHK pixel loops. EXIF combinations with
  transparency, language-keyed `iTXt` with EXIF, `gAMA` combinations, and
  full `getexif()` object behavior remain deferred.
- `FMT-PNG-001R` covers PNG `icc_profile` plus `exif` save on a bounded RGB
  fixture. Pillow 11.3.0 writes chunk order `IHDR`, `iCCP`, `eXIf`, `IDAT`,
  `IEND`; reopens with preserved RGB bytes, `Info["icc_profile"]` equal to the
  caller bytes, Pillow-style `Info["exif"]` bytes, and EXIF orientation `6`.
  Native `pillow_c_image_save_png_icc_exif_options` writes both metadata chunks
  through the DLL, and `ahk/pillow.ahk` routes PNG `icc_profile` plus `exif`
  without AHK pixel loops. Transparency combinations, `gAMA` combinations,
  and full `getexif()` object behavior remain deferred.
- `FMT-PNG-001S` covers PNG `pnginfo` plus `icc_profile` plus `exif` save on a
  bounded RGB fixture. Pillow 11.3.0 writes chunk order `IHDR`, `iCCP`,
  `tEXt`, `eXIf`, `IDAT`, `IEND`; reopens with preserved RGB bytes, final text
  metadata `Info["Author"] == "Ada"` / `Text["Author"] == "Ada"`, caller ICC
  bytes, Pillow-style `Info["exif"]` bytes, and EXIF orientation `6`. Native
  `pillow_c_image_save_png_text_entries_icc_exif_options` writes all three
  metadata groups through the DLL, and `ahk/pillow.ahk` routes ordinary
  `pnginfo` text entries plus `icc_profile` plus `exif` without AHK pixel
  loops. Transparency combinations, language-keyed `iTXt` with ICC/EXIF,
  `gAMA` combinations, and full `getexif()` object behavior remain deferred.
- Native `pillow_c_image_save_png_exif_options` writes the bounded `eXIf`
  chunk before `IDAT`, stripping the `Exif\0\0` header from the stored chunk.
  `pillow_c_image_metadata_png_exif` restores and exposes the Pillow-style
  bytes on open, and `ahk/pillow.ahk` routes PNG `exif`/`Exif` saves to the
  DLL and maps reopened PNG EXIF metadata into `Info["exif"]` as a `Buffer`.
  EXIF combinations with `pnginfo`, `transparency`, or `gAMA`, and full
  `getexif()` object behavior remain deferred.
- `FMT-PNG-001M` covers PNG save-side `gamma` option behavior. Pillow 11.3.0
  accepts `Image.save(..., gamma=...)` for the bounded RGB fixture but writes
  no `gAMA` chunk, reopens with preserved RGB bytes, and does not expose
  `Info["gamma"]`. Native `pillow_c_image_save_png_gamma_options` accepts the
  option as a no-op, writes a PNG without `gAMA` through the custom PNG writer,
  and the facade routes PNG `gamma`/`Gamma` without AHK pixel loops.
- `FMT-PNG-003A` covers PNG save-side `interlace` option behavior. Pillow
  11.3.0 accepts `Image.save(..., interlace=...)` for the bounded RGB fixture
  but writes IHDR interlace byte `0`, reopens with preserved RGB bytes, and
  does not expose `Info["interlace"]`. Native
  `pillow_c_image_save_png_interlace_options` accepts the option as a no-op,
  writes a non-interlaced PNG through the custom PNG writer, and the facade
  routes PNG `interlace`/`Interlace` without AHK pixel loops.
- `FMT-PNG-003B` covers PNG save-side `optimize` option behavior. Pillow
  11.3.0 accepts `Image.save(..., optimize=True)` for the bounded RGB fixture,
  writes ordinary `IHDR`, `IDAT`, `IEND` chunks, uses zlib header bytes
  `[0x78, 0xDA]` for the `IDAT` stream, reopens with preserved RGB bytes, and
  does not expose `Info["optimize"]`. Native
  `pillow_c_image_save_png_optimize_options` routes nonzero optimize through
  the custom PNG writer with the same zlib header marker, and the facade routes
  PNG `optimize`/`Optimize` without AHK pixel loops. Full compression-strategy
  parity and metadata/transparency combinations remain deferred.
- `FMT-PNG-003C` covers one bounded nonzero PNG compression-level behavior.
  Pillow 11.3.0 saves the selected RGB fixture with `compress_level=1` as
  ordinary `IHDR`, `IDAT`, `IEND` chunks, uses zlib header bytes
  `[0x78, 0x01]` for the `IDAT` stream, reopens with preserved RGB bytes, and
  does not expose `Info["compress_level"]`. Native
  `pillow_c_image_save_png_compress_level` and
  `pillow_c_image_save_png_options` route `compress_level=1` through the
  custom PNG writer with the same zlib header marker while keeping image bytes
  native. Exact compressed byte parity, real deflate-size parity for other
  levels, and metadata/transparency combinations remain deferred.
- `FMT-PNG-002A` covers one palette-mode PNG `tRNS` open fixture. Pillow
  11.3.0 exposes a single fully transparent palette entry as
  `Image.info["transparency"] == 1`; the native path preserves the palette
  index bytes and palette alpha metadata so `P -> RGBA` conversion uses alpha
  inside the DLL.
- `FMT-PNG-002B` covers P-mode PNG scalar `Image.save(..., transparency=1)`.
  Pillow 11.3.0 writes a `tRNS` payload `[255, 0]`, reopens as mode `P` with
  preserved index bytes, exposes `Image.info["transparency"] == 1`, and
  converts palette index `1` to alpha `0`. Native
  `pillow_c_image_save_png_transparency_options` writes that chunk and the
  facade routes PNG `transparency`/`Transparency` to the DLL.
- `FMT-PNG-002C` covers one RGB truecolor PNG `tRNS` open fixture. Pillow
  11.3.0 reopens the fixture as mode `RGB`, exposes
  `Image.info["transparency"] == (10, 20, 30)`, preserves RGB bytes, and
  converts matching pixels to alpha `0`. Native `pillow_c_image_open_png`
  forces 8-bit truecolor PNG decode to RGB storage, stores the transparency
  tuple, exposes it through
  `pillow_c_image_metadata_png_rgb_transparency`, and applies it in native
  `RGB -> RGBA` conversion.
- `FMT-PNG-002D` covers one grayscale PNG `tRNS` open fixture. Pillow 11.3.0
  reopens the fixture as mode `L`, exposes
  `Image.info["transparency"] == 10`, preserves grayscale bytes `[10, 40]`,
  and converts matching grayscale pixels to alpha `0`. Native
  `pillow_c_image_open_png` strips the `tRNS` chunk only for the accepted
  8-bit grayscale decode path so WIC does not zero the transparent sample,
  stores the scalar metadata through the existing
  `pillow_c_image_metadata_png_transparency` ABI, and applies it in native
  `L -> RGBA` conversion.
- `FMT-PNG-002E` covers L-mode scalar
  `Image.save(..., transparency=10)`. Pillow 11.3.0 writes a `tRNS` payload
  `[0, 10]`, reopens as mode `L`, preserves grayscale bytes `[10, 40]`,
  exposes `Image.info["transparency"] == 10`, and converts the matching
  grayscale sample to alpha `0`. Native
  `pillow_c_image_save_png_transparency_options` now writes the grayscale
  `tRNS` chunk while reusing the existing scalar transparency ABI.
- `FMT-PNG-002F` covers RGB tuple
  `Image.save(..., transparency=(10,20,30))`. Pillow 11.3.0 writes a `tRNS`
  payload `[0, 10, 0, 20, 0, 30]`, reopens as mode `RGB`, preserves RGB bytes
  `[10, 20, 30, 40, 50, 60]`, exposes
  `Image.info["transparency"] == (10, 20, 30)`, and converts the matching RGB
  pixel to alpha `0`. Native
  `pillow_c_image_save_png_rgb_transparency_options` writes the truecolor
  `tRNS` chunk and the facade routes three-integer PNG transparency values to
  that DLL path.
- `FMT-PNG-002G` covers P-mode byte-table
  `Image.save(..., transparency=bytes([255,128,0,64]))`. Pillow 11.3.0 writes
  chunk order `IHDR`, `PLTE`, `tRNS`, `IDAT`, `IEND`, stores the exact `tRNS`
  payload `[255,128,0,64]`, reopens as mode `P`, exposes
  `Image.info["transparency"]` as the same bytes, and uses the alpha table in
  `P -> RGBA` conversion. Native
  `pillow_c_image_save_png_transparency_table_options` writes the bounded table,
  `pillow_c_image_metadata_png_transparency_table` exposes reopened table bytes,
  and the facade routes PNG `Buffer` transparency values without AHK pixel
  loops.
- `FMT-PNG-002H` covers RGB bytes-valued
  `Image.save(..., transparency=bytes([10,20,30]))`. Pillow 11.3.0 treats the
  three bytes like the tuple `(10,20,30)`, writes truecolor `tRNS` payload
  `[0,10,0,20,0,30]`, reopens as mode `RGB`, exposes
  `Image.info["transparency"] == (10, 20, 30)`, and converts the matching RGB
  pixel to alpha `0`. Native
  `pillow_c_image_save_png_rgb_transparency_bytes_options` accepts exactly
  three RGB bytes and reuses the native truecolor `tRNS` writer; the facade
  routes RGB `Buffer(3)` transparency values through that DLL path while
  keeping P-mode `Buffer` transparency on the byte-table route.

Resolved GIF facts that should not be re-probed before moving to PNG:

- `FMT-GIF-004D` already covers the first optimized caller-transparency
  post-`disposal=2` re-diff fixture with an explicit padded transparent palette
  entry.
- `FMT-GIF-004E` covers the short-palette variant where Pillow keeps the
  post-`disposal=2` background comparison in P-index space, writes the next
  frame full width, and preserves caller transparency on that frame.
- `FMT-GIF-004L` covers the `optimize=False` caller-transparency payload case
  where Pillow preserves opaque unchanged local-frame indices instead of using
  transparency as an optimization, even though the frame GCE still carries the
  caller transparency index.
- `FMT-GIF-004M` covers the `optimize=False`, post-`disposal=2`, caller
  `background=1` plus `transparency=2` all-transparent follow-up frame where
  Pillow reuses the global color table and writes decoded indices `[2,2,2]`
  with no local color table.
- `FMT-GIF-004N` covers the matching default optimized, post-`disposal=2`,
  caller `background=1` plus `transparency=2` all-transparent follow-up frame
  where Pillow writes a full-width local frame with a 4-entry all-black local
  color table, GCE transparency `0`, and decoded indices `[0,0,0]`.
- `FMT-GIF-004O` covers a larger `4x2` default optimized post-`disposal=2`
  matrix where the optimized first/global palette and current frame palette
  match, so Pillow compares the restored background in P-index space and writes
  the following frame as a full `4x2` local frame instead of shrinking to the
  persistent red pixel's RGB bbox.
- `FMT-GIF-004P` covers the matching mixed-unused-palette `4x2` default
  optimized post-`disposal=2` matrix where raw source palettes differ only in an
  unused entry but their optimized palettes match, so Pillow still uses the
  P-index restored-background comparison and writes the following frame as a
  full `4x2` local frame.
- `FMT-GIF-004Q` covers the sparse-palette, no-caller-transparency,
  `background=1`, post-`disposal=2` matrix where existing native behavior
  already matched Pillow 11.3.0: generated transparency is used for the compact
  blue frame, and the following red/background frame is full width with no
  transparency GCE.
- `FMT-GIF-004R` covers the all-identical-frame GIF animation collapse boundary:
  if all input frames collapse to a single output frame, a multi-element
  `disposal` list is rejected like Pillow 11.3.0, while scalar disposal and
  partial-collapse duration merging remain out of the rejection path.
- `FMT-GIF-004S` covers the mixed first-RGB / second-RGBA caller transparency
  boundary where Pillow preserves explicit `transparency=1` on both frame GCEs
  and maps frame-0 GCE transparency to palette alpha on reopen.
- `FMT-GIF-002F` covers the read-side follow-up for that same mixed fixture:
  later composited GIF frames upgrade to RGBA only when the frame-0 canvas used
  transparency, preserve transparent canvas pixels while drawing opaque later
  pixels, return frame 1 as RGBA, and clear frame-level transparency metadata
  like Pillow 11.3.0.
- On the covered `3x1` P-mode probes, caller `background` changes metadata but
  does not itself drive Pillow's next optimized bbox choice.
- The current bounded authority remains local Python `3.10.11` with Pillow
  `11.3.0`.

## High-Level Gap Map

Current highest-value remaining areas:

1. `META-001` remaining children: continue only for a new bounded EXIF/TIFF
   metadata object behavior that the local Pillow `11.3.0` oracle proves.
   The JPEG/PNG orientation read/mutate/native-serialize/explicit-save
   writeback child is covered, and `META-001B` now covers TIFF IFD0
   Orientation=1 read metadata through `GetExif()` / `ExifOrientation()`.
   `META-001C` covers bounded EXIF ASCII Make tag `271` storage,
   `META-001D` extends the same route to common IFD0 ASCII tags `271`, `272`,
   `305`, and `306`, `META-001E` covers scalar integer IFD0 tags `256`/`257`
   as LONG plus `296`/`531` as SHORT, `META-001F` covers rational IFD0 tags
   `282`/`283` as RATIONAL `145/2` and `300/1`, `META-001G` covers SHORT
   array tag `530` (`YCbCrSubSampling`) as `[2, 1]`, `META-001H` covers BYTE
   array tag `40091` (`XPTitle`) as `[72,0,105,0,0,0]`, `META-001I` covers
   SRATIONAL tag `37380` (`ExposureBiasValue`) as `[-1, 2]`,
   `META-001J` covers UNDEFINED tag `36864` (`ExifVersion`) as bytes
   `[48,50,51,48]`, `META-001K` covers BYTE array tag `37510`
   (`UserComment`) as bytes `[99,111,109,109,101,110,116]` across native
   serialization/parsing, explicit JPEG/PNG save, and reopened `getexif()`
   readback, `META-001L` covers reopened TIFF IFD0 common ASCII tags `271`,
   `272`, `305`, and `306` plus Orientation `274` only when stored as identity
   orientation `1`, and `META-001M` adds reopened TIFF `ImageDescription` tag
   `270` readback. Continue only beyond those covered children.
2. `MODE-I-001` / `MODE-F-001` remaining children: `getextrema()`,
   `histogram()`, `I`/`F -> L` conversion, bounded mode `I`
   `ImageFilter.Kernel`, mode `F` Kernel wrong-mode rejection, bounded
   `I`/`F` `ImageFilter.RankFilter` / Min / Median / Max semantics,
   `I`/`F` `ImageFilter.ModeFilter` wrong-mode rejection, and `I`/`F`
   BoxBlur/GaussianBlur/UnsharpMask wrong-mode rejection are now covered for
   the four-byte numeric storage modes. `MODE-NUM-001K` covers numeric
   `Image.getcolors()` scalar value materialization, and `MODE-NUM-001L`
   covers numeric `Image.GetBands()` / `getbands()` band names.
   `MODE-NUM-001M` covers unmasked empty numeric `ImageStat.Stat(...)`
   rejection with `min/max not given`, `MODE-NUM-001N` covers
   `ImageOps.Invert` / `Posterize` / `Solarize` numeric wrong-mode facade
   parity on top of the existing native rejection, and `MODE-NUM-001O` covers
   `ImageOps.Equalize` / `Autocontrast` numeric histogram-transform wrong-mode
   facade parity on top of the existing native rejection.
   `MODE-NUM-001J` covers unmasked numeric `Image.entropy()` plus masked
   numeric wrong-mode rejection.
   `MODE-NUM-001H` also covers
   non-logical `ImageChops` binary operation wrong-mode rejection for matching
   `I` and `F` inputs, and `MODE-NUM-001I` covers `Image.blend` /
   `ImageChops.Blend` wrong-mode rejection for matching `I` and `F` inputs.
   Continue only one operation family at a time, such as broader arithmetic,
   broader statistics, or newly proven filter semantics, after a local oracle
   proves the exact numeric behavior.
3. `FMT-TIFF-002` to `FMT-TIFF-003`: TIFF tag/compression behavior and broader
   mode coverage after the now-covered `FMT-TIFF-001A` bounded multipage
   `save_all` child. `FMT-TIFF-002A` covers Orientation=3 open-side
   180-degree pixel reorientation, and `FMT-TIFF-002B` covers Orientation=6/8
   dimension-swapping open-side rotations. `FMT-TIFF-002C` covers the
   remaining bounded Orientation=2/4/5/7 mirror/transposed open-side
   transforms, `FMT-TIFF-002D` covers DPI Resolution tags,
   `FMT-TIFF-002E` covers PackBits save compression, `FMT-TIFF-002F` covers
   LZW save compression, `FMT-TIFF-002G` covers Adobe Deflate save
   compression, `FMT-TIFF-003A` covers bounded mode `1` save/open,
   `FMT-TIFF-003B` covers bounded palette mode `P` save/open,
   `FMT-TIFF-003C` covers bounded mode `LA` save/open plus the now-tested
   PackBits/LZW/Adobe Deflate compression route,
   `FMT-TIFF-003D` covers bounded mode `CMYK` save/open,
   `FMT-TIFF-003E` covers bounded mode `CMYK` save/open with PackBits/LZW/
   Adobe Deflate compression, and `FMT-TIFF-003F` covers bounded numeric modes
   `I` and `F` uncompressed save/open. `FMT-TIFF-003G` covers bounded numeric
   modes `I` and `F` save/open with PackBits, LZW, and Adobe Deflate
   compression. `FMT-TIFF-003H` covers bounded little-endian `I;16`
   save/open, and `FMT-TIFF-003I` covers bounded little-endian `I;16`
   PackBits/LZW/Adobe Deflate save/open. `FMT-TIFF-003J` covers bounded
   big-endian `I;16B` save/open, and `FMT-TIFF-003K` covers compressed
   public `I;16B` normalization to little-endian TIFF `I;16`. Do not merge
   compression, tag, and mode work into one broad TIFF patch.
4. `BYTES-001`: direct `L`, `RGB`, `RGBA`, and `RGBX` raw/frombuffer coverage
   is now useful for the AHK-first path. Continue only for a concrete
   lower-level readonly/detach edge on an implemented read or mutating path, or
   for a deliberately scoped constructor dependency such as `fromarray`.
5. `FMT-GIF-003B` / `FMT-GIF-004` children only when a local Pillow `11.3.0`
   oracle exposes a concrete palette-stability, collapse, or pathological
   animation miss.
   Do not expand speculative GIF matrices; broad public quantize parity remains
   `QUANT-001`.
6. PNG after `FMT-PNG-001AD`: use the generalized native metadata route for
   same-route combinations. Remaining value is true compression strategy,
   public/standard chunk rules, APNG as a separate gap, and EXIF/ICC object
   interop through
   `META-001`/`META-002`.
7. JPEG after `FMT-JPEG-003C`: continue only for a newly proven keep/metadata,
   marker-preservation, or codec-strategy boundary beyond the covered rule that
   opened COM/comment is implicit while opened ICC/EXIF require explicit
   options.
8. New save-all format families such as APNG/WebP/MPO/PDF only after an
   explicit dependency and scope decision.
9. `QUANT-001`: broader quantize/public algorithm parity beyond bounded GIF
   save slices.
10. Dependency-gated format families: WebP still image open/save first if a
   dependency/package decision is made; AVIF, JPEG2000, PDF, PCX, DDS, ICNS,
   CUR, and other long-tail formats stay behind explicit gap IDs.
11. `META-002A` covers bounded PNG/JPEG XMP open metadata plus `getxmp()`,
    `META-002B` covers bounded explicit JPEG `xmp=` save round-trip,
    `META-002C` covers bounded explicit JPEG `qtables + xmp` save routing,
    `META-002D` covers bounded explicit JPEG no-qtables `keep_rgb + xmp`
    save routing, `META-002E` covers bounded explicit JPEG
    `qtables + keep_rgb + xmp` save routing, and `META-002F` covers repeated
    RDF sequence list materialization for bounded XMP `getxmp()` packets.
    Remaining `META-002`, `ImageCms`, FreeType/RAQM, array interop,
    benchmarks, and packaging work stays behind explicit bounded children and
    should not be mixed into the next hot-format correctness slice.
12. Facade API direct-diff children: `API-IMG-001A` now covers
    `format_description`, `API-IMG-001B` covers `has_transparency_data`, and
    `API-IMG-001C` covers `get_child_images()` empty-list parity;
    continue only for a small object/API gap when the Pillow 11.3.0 semantics
    are clear and the facade can route without AHK pixel loops.

## Read Complete Template

Use this exact shape before implementation work:

```text
- Estimate: AHK-first high-performance Pillow-compatible runtime about 52-55%; full Pillow replacement 26-31%.
- Latest covered gap: META-001M bounded TIFF ImageDescription tag 270 getexif readback.
- Selected next gap: continue only a bounded GIF/JPEG/PNG/META/TIFF/BYTES/API/ICO/MODE behavior with a locally proven Pillow boundary; avoid broad re-audits and same-route option branches.
- Known tests: current tree registers 1326 tests: 662 raw DLL / 664 facade.
  Latest targeted `META-001M` raw TIFF tag `270` test passed `1/1` in
  `140ms`; facade TIFF `getexif()` tag `270` test passed `1/1` in `31ms`;
  raw/facade TIFF filters passed `27/27` and `26/26`; raw/facade EXIF filters
  passed `35/35` and `37/37`; latest full AHK directory invocation passed
  `1326/1326` in `7578ms` with known non-failing libjpeg stderr warnings.
- Native rebuild: required only if src/pillow_c.cpp or project files change.
```

If any line above is no longer true, update this file first, then update
`docs/pillow-gap-analysis.md` in the same patch.
