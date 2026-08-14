# Testing

Tests use `ahktest` from [MonoEven/stdlib-ahk](https://github.com/MonoEven/stdlib-ahk).

In the current local workspace, run tests through the parent `visual_studio\tools\run-ahktest.ps1` wrapper. That runner adds `#ErrorStdOut`, captures unhandled AHK errors, writes reports, and prevents modal error popups from blocking automation.

The wrapper validates discovered `.test.ahk` sources plus statically resolved AHK `#Include`, `-Config`, and `-Plugin` sources as strict UTF-8 before launching AutoHotkey. A source decoding warning such as `Some non-ASCII characters could not be decoded` is reported as an ahktest collection failure instead of timing out behind a modal warning.

Use `-TimeoutSeconds 120` for filtered and single-file AHK runs. Use
`-TimeoutSeconds 240` for the full AHK directory suite.

## Build

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\tasks\2026-06-07-pillow-c-foundation\src\pillow_c.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

From inside this repository:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\src\pillow_c.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

## Oracle

Regenerate Pillow behavior fixtures with local Python 3.10.11:

```powershell
F:\Python\Python310\python.exe .\tasks\2026-06-07-pillow-c-foundation\oracle\pillow_oracle.py
```

From inside this repository:

```powershell
F:\Python\Python310\python.exe .\oracle\pillow_oracle.py
```

Note: `oracle/pillow_oracle.json` is human reference material for pinning
expectations. No AHK test reads the JSON at runtime; the tests hardcode their
expected values inline.

## AHK Tests

From the parent `visual_studio` workspace:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run-ahktest.ps1 -Target .\tasks\2026-06-07-pillow-c-foundation\ahk -Report .codex\pillow-c-report.txt -TimeoutSeconds 240
```

This suite currently registers `2804` AHK tests: `1388` raw DLL tests and
`1416` facade tests.

Latest `API-TRANSFORMCLS-001` verification: the Pillow 11.3.0 oracle
(kept in `oracle/probe_imagetransform.py`) records the base
`Transform` class (data storage, the base getdata AttributeError
shape, transform routing) and the five method-constant subclasses;
the module is not callable. The facade `Pillow.ImageTransform`
covers all six classes exactly — `GetData()` returns `[method,
data]`, `Transform()` routes through the facade `Image.Transform`
seam (oracle-verified byte-equal affine identity, BILINEAR option,
and extent routing), and constructing the module class fails loudly.
Facade-only change, no native rebuild: the ImageTransform target
passes `1/1` in `47ms`, and the full directory suite passes
`2804/2804` in `20890ms`, with zero failures, errors, or skips.
Source/DLL export parity remains `466/466` (DLL byte-identical), and
the DLL SHA-256 remains
`7C1D4A9145A70EC864997FF5EBE4C52CB14A1BFEEF5901994A9E38E3572B8930`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed.
The overall Pillow replacement-readiness estimate moves to `92% ±5%`.

Latest `API-PATH-001` verification: the Pillow 11.3.0 oracles (kept
in `oracle/probe_imagepath.py`, `oracle/probe_imagepath2.py`, and
`oracle/probe_imagepath3.py`) show the simplified five-method
`Image.core.path` (constructor pairs/flat with float coordinates,
tolist flat pairs, getbbox with the empty (0,0,0,0) rule, in-place
no-op compact, 6-value affine transform with Pillow's length error,
None-returning callable map). The facade `Pillow.ImagePath.Path`
mirrors all five. Facade-only change, no native rebuild: the
ImagePath target passes `1/1` in `47ms`, and the full directory suite
passes `2800/2800` in `19516ms`, with zero failures, errors, or
skips. Source/DLL export parity remains `466/466` with zero
difference, and the DLL SHA-256 remains
`7C1D4A9145A70EC864997FF5EBE4C52CB14A1BFEEF5901994A9E38E3572B8930`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed.
The overall Pillow replacement-readiness estimate moves to `88% ±5%`.

Latest `API-GRAB-001` verification: the Pillow 11.3.0 oracles (kept
in `oracle/probe_imagegrab_clip.py` and
`oracle/probe_imagegrab_dib.py`) pin grab()/grabclipboard() semantics,
and the ctypes cross-check (kept in
`oracle/probe_imagegrab_dll_compose.py`) matches Pillow's
grabclipboard outputs byte-exactly across 24/32/8/1bpp DIBs and the
empty path (`FAILURES: 0`). The new `pillow_c_grab.cpp` module adds
`pillow_c_image_grab` and `pillow_c_image_grab_clipboard`, and the
facade `ImageGrab` class adds Grab/GrabClipboard with Pillow's
coordinate errors and the None analogue. Raw/facade grab targets pass
`2/2` in `172ms`; the math filter passes `2/2` in `47ms`; and the
full directory suite passes `2799/2799` in `21953ms`, with zero
failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity moves to
`466/466` (two deliberate new exports) with zero difference; and the
rebuilt DLL SHA-256 is
`7C1D4A9145A70EC864997FF5EBE4C52CB14A1BFEEF5901994A9E38E3572B8930`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `87% ±5%`.

Latest `API-MATH-001` verification: the Pillow 11.3.0 oracles (kept
in `oracle/probe_imagemath.py`, `oracle/probe_imagemath2.py`, and
`oracle/probe_imagemath3.py`) pin the bounded eval/unsafe_eval grammar
over L/I/F operands (binary ops, comparisons, unary -/~,
abs/min/max/float/int/convert, literals; mode I/F results; C
truncation division and C remainder; RGB/name/type error parity).
The new `pillow_c_image_math_rpn` export runs the per-pixel RPN stack
machine, and the facade `ImageMath` class adds the tokenizer,
shunting-yard compiler, and scalar evaluator. The ctypes cross-check
(kept in `oracle/probe_imagemath_dll_compose.py`) matches Pillow's
eval outputs for 20 expressions byte-exactly (`FAILURES: 0`).
Raw/facade math targets pass `2/2` in `47ms`; the numeric filter
passes `128/128` in `625ms`; and the full directory suite passes
`2797/2797` in `21547ms`, with zero failures, errors, or skips.
Release x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export
parity moves to `464/464` (one deliberate new export) with zero
difference; and the rebuilt DLL SHA-256 is
`E721E4C964B99CE6D12E7E77043847C0DAC578A95A956E36444E154A3BD032BF`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `86% ±5%`.

Latest `AUDIT-002` verification (independent re-audit): the surface
enumerator (`oracle/audit_pillow_surface.py` →
`oracle/pillow_surface.json`) measured Pillow 11.3.0's real public
surface — 59 `Image.Image` names, 69 `ImagingCore` names, 23
submodules, 30 SAVE and 45 OPEN formats — and the diff against the
facade found unrecorded gaps (ImageMath/ImageGrab/ImagePath/
ImageQt/ImageTk/ImageFile/ImagePalette module surfaces, ImageTransform
class objects, ImageFont variation fonts, `Image.readonly`, and the
unrecorded BLP/BUFR/DIB/GRIB/HDF5/IM/MSP/PALM/SPIDER/WMF save plus
FITS/FPX/FTEX/GBR/IMT/IPTC/MCIDAS/MIC/MPEG/PCD/PIXAR/SPIDER/WMF/
XVTHUMB open formats). The estimate is demoted to `85% ±5%` and the
gaps are recorded as not-started rows (`oracle/audit_report_2026-08-14.md`).
No native change: the suite stays `2795/2795`, exports `463/463`, and
the DLL SHA-256 stays
`8C5B3EE20232B304CB6F06F8EB971DC043B5CFF562F8519D3994444033290308`.
The next bounded child is `API-MATH-001`.

Latest `BNDRY-001` verification: the explicit remaining-item boundary
ledger completes the coverage definition. The dependency-gated formats
(WebP/AVIF/JPEG2000/PDF/PSD/DDS/PCX/ICNS/SGI/SUN/EPS/MPO/FLI/DCX/XPM)
fail loudly with `Pillow image file format is unsupported`, and
libimagequant keeps Pillow's exact
`dependency required by this method was not enabled at compile time`
error; APNG/PNG compression strategy, dither exact parity beyond the
FLOYDSTEINBERG slices, qtables beyond two tables, malformed marker
streams, explicit YCCK encoding, the META-002 tail, and the
whole-file parity policy are all recorded as explicit documented
boundaries. The facade boundary test
(`PillowTestDependencyGatedFormatBoundaries`) pins the rejections.
Facade-only change, no native rebuild: the BNDRY-001 boundary target
passes `1/1` in `31ms`, and the final full directory suite passes
`2795/2795` in `18437ms`, with zero failures, errors, or skips.
Source/DLL export parity remains `463/463` with zero difference, and
the DLL SHA-256 remains
`8C5B3EE20232B304CB6F06F8EB971DC043B5CFF562F8519D3994444033290308`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed.
The overall Pillow replacement-readiness estimate reaches
`100% ±4%`.

Latest `API-IMG-001F` verification: Pillow 11.3.0's `im` attribute is
the per-image `ImagingCore` C object, whose AHK analogue is the native
image handle (the `pillow_c_*` ABI covers the ImagingCore method
surface). The facade adds the `Im` property returning the same handle
as `GetIm()` (AHK case-insensitivity serves `im`) with the
Pillow-shaped closed-image error; the boundary is recorded in the
ledger, completing the named `PIL.Image.Image` object-model list.
Facade-only change, no native rebuild: the im-accessor target passes
`1/1` in `47ms`, and the full directory suite passes `2794/2794` in
`19266ms`, with zero failures, errors, or skips. Source/DLL export
parity remains `463/463` with zero difference, and the DLL SHA-256
remains
`8C5B3EE20232B304CB6F06F8EB971DC043B5CFF562F8519D3994444033290308`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed.
The overall Pillow replacement-readiness estimate stays `99% ±4%`.

Latest `MODE-NUM-001CQ` verification: the Pillow 11.3.0 oracle (kept
in `oracle/probe_mode_i16_stats2.py`) shows I;16/I;16B `getcolors()`
raises `image has wrong mode`, while the C entropy returns
layout-dependent byte-misread values (`log2(6)` for six
misread-distinct samples) and `ImageStat.Stat` derives from that
misread histogram. The native entropy and getcolors entry points now
return `PILLOW_C_INVALID_ARGUMENT` for I;16/I;16B, and the facade
surfaces Pillow's `image has wrong mode` for `GetColors`, a documented
boundary error for `Entropy`, and inherits the histogram boundary
through `ImageStat.Stat`. Raw/facade boundary targets pass `5/5` in
`140ms`; the numeric filter passes `128/128` in `578ms`; the entropy
filter passes `48/48` in `406ms`; the getcolors filter passes `8/8` in
`47ms`; and the full directory suite passes `2793/2793` in `19219ms`,
with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity remains `463/463`
with zero difference; and the rebuilt DLL SHA-256 is
`8C5B3EE20232B304CB6F06F8EB971DC043B5CFF562F8519D3994444033290308`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `99% ±4%`.

Latest `MODE-NUM-001CP` verification: the Pillow 11.3.0 oracle (kept
in `oracle/probe_mode_i16_stats.py`) shows I;16 `getextrema()` scans
uint16 samples, I;16B `getextrema()` raises `image has wrong mode`,
`convert("I")`/`convert("F")` copy the sample exactly, `convert("L")`
applies the Convert.c high-byte rule (high byte nonzero -> 255, else
the low byte), and the I;16 `histogram()` C path reads 2-byte storage
through layout-dependent byte misreads (documented boundary). The
native `extrema_image_numeric` gains the uint16 branch (rejecting
I;16B), `convert_image_mode_into` gains the I;16/I;16B source branch,
and the facade surfaces the I;16B GetExtrema and I;16 Histogram
boundaries. The ctypes cross-check (kept in
`oracle/probe_mode_i16_stats_dll_compose.py`) matches Pillow exactly
(`FAILURES: 0`). Raw/facade I;16 stats targets pass `4/4` in `47ms`;
the numeric filter passes `128/128` in `578ms`; the convert filter
passes `141/141` in `250ms`; the Extrema filter passes `11/11` in
`47ms`; and the full directory suite passes `2791/2791` in `18750ms`,
with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity remains `463/463`
with zero difference; and the rebuilt DLL SHA-256 is
`793768DFFDBD8E3088960A3E1B27E58AD9295581C13B3F86AC60D2DC4B3BD393`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `98% ±4%`.

Latest `MODE-NUM-001CO` verification: the Pillow 11.3.0 oracles (kept
in `oracle/probe_mode_i16_fill.py` and
`oracle/probe_mode_i16_fill_range.py`) show I;16/I;16B NEAREST
transform/rotate fills pack one uint16 sample: int scalars and
single-element tuples wrap modulo 65536 (70000 -> 4464, -5 -> 65531),
color names resolve through the grayscale map, floats and
multi-element sequences reject with `color must be int or
single-element tuple`, and I;16B keeps big-endian raw bytes. The
facade `TransformFillBuffer` gains the I;16/I;16B branch while the
native ABI already accepted the raw 2-byte fill. Raw/facade I;16 fill
targets pass `2/2` in `32ms`; the numeric filter passes `128/128` in
`641ms`; the Rotate filter passes `22/22` in `47ms`; and the full
directory suite passes `2789/2789` in `18219ms`, with zero failures,
errors, or skips. Source/DLL export parity remains `463/463` with zero
difference, and the DLL SHA-256 remains
`ADAB3C0F6DBFD41B8C116D35F5B92C9B7A968F817F292C5B675EA1A667F0BA05`
(facade-only slice). No facade lifetime rule, fallback, or AHK pixel
loop changed. The overall Pillow replacement-readiness estimate moves
to `97% ±4%`.

Latest `MODE-NUM-001CN` verification: the Pillow 11.3.0 oracles (kept
in `oracle/probe_mode_reducing_gap.py` and
`oracle/probe_mode_reducing_gap2.py`) show reducing-gap resize runs
`reduce()` (32bpc block-average per sample: ROUND_UP for I, float32
for F, partial-edge corner multipliers) then a boxed resize, while the
I;16 reduce step raises `ValueError: image has wrong mode`. The native
`supports_reduce_mode` now accepts I and F, `reduce_image_into` gains
the numeric branch, the I;16 reduce step stays rejected, and the
facade surfaces Pillow's `image has wrong mode` message when the
factor exceeds 1. The ctypes cross-check (kept in
`oracle/probe_mode_reducing_gap_dll_compose.py`) matches Pillow's
24x24-to-3x3 I/F NEAREST/BILINEAR/BICUBIC outputs, the I;16 NEAREST
output, and the boundary exactly (`FAILURES: 0`). Raw/facade
reducing-gap numeric targets pass `4/4` in `47ms`; the numeric filter
passes `127/127` in `609ms`; the resize filter passes `31/31` in
`62ms`; and the full directory suite passes `2787/2787` in `18859ms`,
with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity remains `463/463`
with zero difference; and the rebuilt DLL SHA-256 is
`ADAB3C0F6DBFD41B8C116D35F5B92C9B7A968F817F292C5B675EA1A667F0BA05`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `96% ±4%`.

Latest `MODE-NUM-001CM` verification: the Pillow 11.3.0 oracle (kept
in `oracle/probe_mode_i16.py`) shows I;16 resize runs the 16bpc
two-pass resampler with ROUND_UP plus per-byte CLIP8 writes per pass
(values above 65535 wrap the high byte to 255), while I;16
transform/rotate NEAREST whole-copies samples and bilinear/bicubic
transform/rotate interpolate the storage bytes as byte channels
(endian-bug garbage on I;16B). The native resize filter gains the
uint16 branch (`resize_read_i16_sample`,
`resize_round_clip_i16_sample` with the exact CLIP8 artifact,
`resize_write_i16_sample`), and bilinear/bicubic transforms on
I;16/I;16B plus I;16B filter resizes become explicit documented
boundaries (`PILLOW_C_INVALID_ARGUMENT`); the facade defaults
`;`-modes to NEAREST and surfaces the boundary errors. The ctypes
cross-check (kept in `oracle/probe_mode_i16_dll_compose.py`) matches
Pillow's I;16 resize NEAREST/BILINEAR/BICUBIC outputs, the NEAREST
transform/rotate outputs, and the boundary statuses exactly
(`FAILURES: 0`). Raw/facade I;16 targets pass `3/3` in `63ms`; the
numeric filter passes `125/125` in `593ms`; the resize filter passes
`29/29` in `63ms`; the transform filter passes `181/181` in `4985ms`;
and the full directory suite passes `2785/2785` in `18718ms`, with
zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity remains `463/463`
with zero difference; and the rebuilt DLL SHA-256 is
`9280B7142878AB5A8CF17A379AA40E1B22CB2C5FB0C4FA5B0FBC380CECEFF26E`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `95% ±4%`.

Latest `MODE-NUM-001CL` verification: the Pillow 11.3.0 oracle (kept
in `oracle/probe_mode_box_thumb.py`) shows boxed
`resize(..., box=...)` on I and F runs through the same 32bpc
two-pass resampler as the plain resize, and `thumbnail()` applies the
aspect-preserving size math then the plain resize. The ctypes
cross-check (kept in `oracle/probe_mode_box_thumb_dll_compose.py`)
matches Pillow's I/F boxed NEAREST/BILINEAR/BICUBIC outputs and the
4x3-to-2x2 thumbnail-dimension resizes exactly (`FAILURES: 0`) with
ZERO native changes: the MODE-NUM-001CK numeric branch already serves
`pillow_c_image_resize_box`, and the facade thumbnail aspect math
already routes through `pillow_c_image_resize`. Raw/facade
box/thumbnail numeric targets pass `2/2` in `31ms`; the resize filter
passes `27/27` in `62ms`; the Thumbnail filter passes `7/7` in
`125ms`; and the full directory suite passes `2783/2783` in `18797ms`,
with zero failures, errors, or skips. Source/DLL export parity remains
`463/463` with zero difference, and the DLL SHA-256 remains
`77D2F0BB93546810708B69A4F39671FE6186A93762DB00D9871186D1AC4BEE9F`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `94% ±4%`.

Latest `MODE-NUM-001CK` verification: Pillow 11.3.0's `Resample.c`
32bpc paths (source from the 11.3.0 tag) resample one 32-bit sample
per pixel with the unquantized normalized double weights from the same
coefficient precompute as the 8-bit path: mode F keeps float32
intermediates between the two separable passes, mode I rounds half away
from zero after each pass. The native `ResampleCoefficients` now
retains the double weights, and `resize_filter_box_into` gains a
numeric two-pass branch serving BILINEAR/BICUBIC/LANCZOS/BOX/HAMMING
uniformly. The ctypes cross-check (kept in
`oracle/probe_mode_resize_dll_compose.py`) matches Pillow's 2x2-to-3x3
I/F outputs for all six kernels exactly (`FAILURES: 0`). Raw/facade
numeric resize targets pass `2/2` in `47ms`; the resize filter passes
`25/25` in `47ms`; the numeric filter passes `123/123` in `578ms`;
and the full directory suite passes `2781/2781` in `19296ms`, with
zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity remains `463/463`
with zero difference; and the rebuilt DLL SHA-256 is
`77D2F0BB93546810708B69A4F39671FE6186A93762DB00D9871186D1AC4BEE9F`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `93% ±4%`.

Latest `MODE-NUM-001CJ` verification: the Pillow 11.3.0 oracle (kept
in `oracle/probe_mode_pqm.py`) shows perspective/quad/mesh on modes I
and F interpolate one 32-bit sample per pixel like AFFINE. The ctypes
cross-check (kept in `oracle/probe_mode_pqm_dll_compose.py`) proved
PERSPECTIVE and QUAD already matched through the shared
`transform_with_mapper_into` numeric branch, while MESH
bilinear/bicubic diverged because `mesh_transform_image_into` kept its
own per-byte channel loop (the compose recorded `FAILURES: 4` before
the fix, `FAILURES: 0` after). Raw/facade PQM numeric targets pass
`2/2` in `47ms`; the transform filter passes `179/179` in `5016ms`;
the numeric filter passes `121/121` in `578ms`; and the full directory
suite passes `2779/2779` in `18531ms`, with zero failures, errors, or
skips. Release x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL
export parity remains `463/463` with zero difference; and the rebuilt
DLL SHA-256 is
`2CADF992A741F27D32A7E72F5A3881162F2FE882920ADD8F7601A348FE742F1B`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `92% ±4%`.

Latest `MODE-NUM-001CI` verification: the Pillow 11.3.0 oracle (kept
in `oracle/probe_mode_rotate.py`) shows `rotate()` builds the same
affine matrix as the existing native `rotate_affine_geometry` and
dispatches through the AFFINE transform path, so mode I/F rotate needs
the same one-sample interpolation as transform. The rotate
bilinear/bicubic loops now route numeric modes through
`bilinear_transform_numeric_sample` /
`bicubic_transform_numeric_sample` with float32 casts and int32
truncation (NEAREST already whole-copied samples), and the rotate fill
reuses the numeric `TransformFillBuffer` packing. The ctypes
cross-check (kept in `oracle/probe_mode_rotate_dll_compose.py`)
matches Pillow's I/F NEAREST/BILINEAR/BICUBIC rotate outputs at 45
degrees with expand=False/True plus fills exactly (`FAILURES: 0`).
Raw/facade numeric rotate targets pass `3/3` in `125ms`; the Rotate
filter passes `20/20` in `47ms`; the numeric filter passes `119/119`
in `610ms`; and the full directory suite passes `2777/2777` in
`18562ms`, with zero failures, errors, or skips. Release x64 Rebuild
has `0 Warning(s), 0 Error(s)`; source/DLL export parity remains
`463/463` with zero difference; and the rebuilt DLL SHA-256 is
`0F5D6849D8818779F2D8D72361CB4DC724DF44EA3DB73034E1CE71ECFA67644B`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `91% ±4%`.

Latest `MODE-NUM-001CH` verification: the Pillow 11.3.0 oracles (kept
in `oracle/probe_mode_transform.py`,
`oracle/probe_mode_transform_math.py`,
`oracle/probe_mode_transform_fill_errors.py`, and
`oracle/probe_mode_transform_fill_accept.py`) show Pillow converts
EXTENT to an AFFINE matrix in `Image.py` and interpolates ONE 32-bit
sample per pixel for NEAREST/BILINEAR/BICUBIC on modes I and F with the
existing byte-mode geometry; mode F stores the float32 cast, mode I
stores the int32 truncation toward zero, and numeric fills pack one
int32/float32 sample (scalars, single-element tuples, color names
through the grayscale map, plus the two Pillow error messages). The
ctypes cross-check (kept in
`oracle/probe_mode_transform_dll_compose.py`) matches Pillow's I/F
NEAREST/BILINEAR/BICUBIC outputs and fills exactly (`FAILURES: 0`).
The shared native transform loop now routes numeric modes through
per-sample bilinear/bicubic helpers, and the facade
`TransformFillBuffer` packs the numeric fill branch. Raw/facade numeric
transform targets pass `3/3` in `141ms`; the transform filter passes
`178/178` in `4969ms`; the numeric filter passes `116/116` in `578ms`;
and the full directory suite passes `2774/2774` in `18938ms`, with
zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity remains `463/463`
with zero difference; and the rebuilt DLL SHA-256 is
`DD2247B6595424F722922E86FA9E7F05D054286D4C7F52E5E45F4BC37DD2ABDC`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `90% ±4%`.

Latest `MODE-F-001B` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_mode_f_point.py`) shows list tables on I/I;16/F are
rejected with `ValueError: point operation not supported for this mode`
while linear callables on F route through `point_transform(scale,
offset)` with float32 math (fractional scales included: `0.5*x` on
`[1.5,-2.5,3.5,0]` gives `[0.75,-1.25,1.75,0]`). The ctypes cross-check
(kept in `oracle/probe_mode_f_point_dll_compose.py`) matches Pillow's
identity/2x+5/half/constant F outputs exactly (`FAILURES: 0`). The
`pillow_c_image_point_transform` export now applies the transform for
mode F as well as mode I, and the facade Point routes linear F
callables through it while rejecting lists, non-linear callables,
modeName, and I;16 with the Pillow message. Raw/facade targets pass
`1/1` each; the point filter passes `159/159` in `1422ms`; and the full
directory suite passes `2771/2771` in `18953ms`, with zero failures,
errors, or skips. Release x64 Rebuild has `0 Warning(s), 0 Error(s)`;
source/DLL export parity remains `463/463` with zero difference; and
the rebuilt DLL SHA-256 is
`1D6F3743A14FCC4D37C16FF99B6D2C7ADAD76143F6BEB82DBDD093A5CED37E5B`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `89% ±4%`.

Latest `MODE-I-001B` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_mode_i_point.py`) shows list tables on I/I;16/F are
rejected with `ValueError: point operation not supported for this mode`
while linear callables on I route through `point_transform(scale,
offset)` with int32 truncating math. The ctypes cross-check (kept in
`oracle/probe_mode_i_point_dll_compose.py`) matches Pillow's identity/
2x+5/x-1000/negate/constant outputs exactly (`FAILURES: 0`). The new
`pillow_c_image_point_transform` export applies the transform for mode
I only, and the facade Point routes linear callables through it while
rejecting lists, non-linear callables, modeName, and I;16/F with the
Pillow message. Raw/facade targets pass `1/1` each; the point filter
passes `157/157` in `1437ms`; and the full directory suite passes
`2769/2769` in `19797ms`, with zero failures, errors, or skips. Release
x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity
moves to `463/463` (one deliberate new export) with zero difference;
and the rebuilt DLL SHA-256 is
`80255CEA0BA94055F2C7CC11D7A415CF58C381BA84D558B743868FF43F977152`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `88% ±4%`.

Latest `API-IMG-001E` verification: the Pillow 11.3.0 probe (kept in
`oracle/probe_image_display_apis.py`, run in isolated subprocesses)
shows `toqimage()`/`toqpixmap()` raise `ImportError("Qt bindings are
not installed")` without PyQt6/PySide6 and `show()` dispatches to a
registered system viewer. The AHK runtime ships neither, so the facade
adds `ToQImage()`/`ToQPixmap()` (case-insensitive aliases serve
`toqimage()`/`toqpixmap()`) raising the exact Qt message and `Show()`
raising the Pillow-shaped `no viewers found` error; these are recorded
explicit documented boundaries. Facade-only change, no native rebuild:
the display-API boundary target passes `1/1`, and the full directory
suite passes `2767/2767` in `19469ms`, with zero failures, errors, or
skips. Source/DLL export parity remains `462/462` with zero difference,
and the DLL SHA-256 remains
`13D0390BED6D26E94B1640407004C81BE279F8255A5F3A4968DA25F2C0628566`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed.
The overall Pillow replacement-readiness estimate moves to `87% ±4%`.

Latest `API-IMG-001D` verification: the Pillow 11.3.0 probe shows
`getim()` returns the low-level "Pillow Imaging" capsule for open
(including empty) images and raises `ValueError: Operation on closed
image` once closed. The facade adds `Image.GetIm()` returning the
native handle pointer with the Pillow-shaped closed-image error; AHK
identifiers are case-insensitive so `getim()` resolves to the same
method. Facade-only change, no native rebuild: the getim target passes
`1/1`, and the full directory suite passes `2766/2766` in `19203ms`,
with zero failures, errors, or skips. Source/DLL export parity remains
`462/462` with zero difference, and the DLL SHA-256 remains
`13D0390BED6D26E94B1640407004C81BE279F8255A5F3A4968DA25F2C0628566`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed.
The overall Pillow replacement-readiness estimate moves to `86% ±4%`.

Latest `FMT-ICO-002G` verification: Pillow 11.3.0 registers no CUR save
and its CUR reader only accepts DIB payloads, so the CUR save is a
standards extension following the XBM hotspot precedent. The ctypes
cross-check (kept in `oracle/probe_cur_save_dll_compose.py`) reopens the
DLL-written CUR through Pillow 11.3.0's CUR reader (format/size/bytes)
and our open exposes the hotspot (5, 7) (`FAILURES: 0`). The new
`pillow_c_image_save_cur_options` export writes the ICO type-2 container
with one DIB payload and the hotspot in the planes/bit_count fields; the
facade routes `Save(path, "CUR", { hotspot: [x, y] })` and surfaces
`Info["hotspot"]`. Raw/facade CUR hotspot targets pass `1/1` each; the
CUR filter passes `18/18` in `140ms`; and the full directory suite
passes `2765/2765` in `19016ms`, with zero failures, errors, or skips.
Release x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export
parity moves to `462/462` (one deliberate new export) with zero
difference; and the rebuilt DLL SHA-256 is
`13D0390BED6D26E94B1640407004C81BE279F8255A5F3A4968DA25F2C0628566`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `85% ±4%`.

Latest `FMT-ICO-001C` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_ico_multi_source_matrix.py`) confirms each exact-size
source keeps its own mode, the first same-size PNG source wins, and
bmp-format same-size frames use distinct bit depths; the existing ICO
frames writer already implements all three, so zero native changes were
needed. The ctypes cross-check (kept in
`oracle/probe_ico_multi_source_dll_compose.py`) reopens the DLL-written
mixed-mode and same-size ICOs in Pillow 11.3.0 with exact modes and
pixels (`FAILURES: 0`). Raw/facade matrix targets pass `1/1` each; the
ICO filter passes `28/28` in `282ms`; and the full directory suite
passes `2763/2763` in `18922ms`, with zero failures, errors, or skips.
24-bit RGB and grayscale PNG payloads remain a documented WIC reopen
boundary. Source/DLL export parity remains `461/461` with zero
difference, and the DLL SHA-256 remains
`984A7D84657C5D9EF8D236A44CF13697A6C8939F2932FC5CB19A7B4CCDD180A3`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed.
The overall Pillow replacement-readiness estimate moves to `84% ±4%`.

Latest `FMT-ICO-001B` verification: the Pillow 11.3.0 oracles (kept in
`oracle/probe_ico_non_exact_sources.py` and
`oracle/probe_ico_thumbnail_fallback.py`) confirm exact-size sources
win, otherwise the last provided image is thumbnailed proportionally
with LANCZOS and never upscaled. The ctypes cross-check (kept in
`oracle/probe_ico_non_exact_dll_compose.py`) reopens the DLL-written
downscale/aspect/smaller-source ICOs in Pillow 11.3.0 with exact
per-size source pixels (`FAILURES: 0`). `save_ico_images_with_sizes`
caps the proportional fallback at the last source's dimensions. Raw and
facade non-exact selection targets pass `1/1` each; the ICO filter
passes `26/26` in `250ms`; and the full directory suite passes
`2761/2761` in `18672ms`, with zero failures, errors, or skips. Release
x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity
remains `461/461` with zero difference; and the rebuilt DLL SHA-256 is
`984A7D84657C5D9EF8D236A44CF13697A6C8939F2932FC5CB19A7B4CCDD180A3`.
Grayscale PNG payloads remain a documented WIC reopen boundary. No
facade lifetime rule, fallback, or AHK pixel loop changed. The overall
Pillow replacement-readiness estimate moves to `83% ±4%`.

Latest `FMT-TIFF-003BJ` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_bigtiff_mixed_size_frames.py`) confirms
`save_all`+`big_tiff` writes chained BigTIFF with per-frame dimensions;
the existing frames writer already handles per-frame width/height, so
zero native changes were needed. The ctypes cross-check (kept in
`oracle/probe_tiff_bigtiff_mixed_size_dll_compose.py`) reopens a
DLL-written three-frame 2x2/3x1/1x3 BigTIFF through Pillow 11.3.0 with
exact per-frame sizes and bytes (`FAILURES: 0`). Raw mixed-size
three-frame and facade save_all mixed-size targets pass `1/1` each; the
TIFF filter passes `707/707` in `5140ms`; and the full directory suite
passes `2759/2759` in `18485ms`, with zero failures, errors, or skips.
Source/DLL export parity remains `461/461` with zero difference, and the
DLL SHA-256 remains
`D829043BE5BF716DAD7A5D6FBA958958D01D777A10D0C5C86921D9195EE4EC6E`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed.
The overall Pillow replacement-readiness estimate moves to `82% ±4%`.

Latest `FMT-TIFF-003BI` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_bigtiff_1_save.py`) confirms mode-1 `big_tiff=True`
saves use eight entries with no 258 tag, photometric 1, and packed
MSB-first strips, while big_tiff+packbits falls back to classic TIFF.
The ctypes cross-check (kept in
`oracle/probe_tiff_bigtiff_1_dll_compose.py`) reopens the DLL-written
mode-1 BigTIFF in Pillow 11.3.0 and the DLL reopens Pillow's own mode-1
BigTIFF (`FAILURES: 0`). The frames writer gained the mode-1 case and
the strip parser gained the bilevel predicate with MSB-first unpack into
0/255-per-pixel storage; the facade big_tiff guard accepts `1`. Raw save
round-trip and Pillow-layout fixture open targets plus the facade
bilevel save target pass `1/1` each; the TIFF filter passes `705/705` in
`5375ms`; and the full directory suite passes `2757/2757` in `18750ms`,
with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity remains `461/461`
with zero difference; and the rebuilt DLL SHA-256 is
`D829043BE5BF716DAD7A5D6FBA958958D01D777A10D0C5C86921D9195EE4EC6E`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed
beyond the BigTIFF bilevel save family. The overall Pillow
replacement-readiness estimate moves to `81% ±4%`.

Latest `FMT-TIFF-003BH` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_bigtiff_p_save.py`) confirms P-mode `big_tiff=True`
saves use photometric 3 with a full 256-entry channel-major ColorMap
320 SHORT[768] blob and the index strip after it. The ctypes cross-check
(kept in `oracle/probe_tiff_bigtiff_p_dll_compose.py`) reopens the
DLL-written P BigTIFF in Pillow 11.3.0 (mode/indices/palette) and the
DLL reopens Pillow's own P BigTIFF (`FAILURES: 0`). The frames metadata
writer gained the P mode case plus the colormap blob, the strip parser
gained the `matches_palette` predicate plus the channel-major `>> 8`
palette conversion, and the facade big_tiff guard accepts P. Raw save
round-trip and Pillow-layout fixture open targets plus the facade P save
target pass `1/1` each; the TIFF filter passes `702/702` in `5063ms`;
and the full directory suite passes `2754/2754` in `19062ms`, with zero
failures, errors, or skips. Release x64 Rebuild has `0 Warning(s),
0 Error(s)`; source/DLL export parity remains `461/461` with zero
difference; and the rebuilt DLL SHA-256 is
`E920652B69C1F2733281781B5A09A74FE0A25B38840B26A60C589A36ECCB91E9`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed
beyond the BigTIFF palette save family. The overall Pillow
replacement-readiness estimate moves to `80% ±4%`.

Latest `FMT-TIFF-003BG` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_bigtiff_saveall_compose.py`) confirms `save_all`+
`big_tiff` writes chained BigTIFF with exact per-frame I;16 bytes,
dpi/icc_profile/tiffinfo land in every frame's IFD, and numeric save_all
plus compression falls back to classic TIFF; the existing frames and
per-frame metadata writers already implement all three shapes, so zero
native changes were needed. The ctypes cross-check (kept in
`oracle/probe_tiff_bigtiff_saveall_dll_compose.py`) reopens both
DLL-written compositions through Pillow 11.3.0 with exact bytes and
per-frame metadata (`FAILURES: 0`). Raw numeric two-frame and per-frame
metadata two-frame targets plus the facade save_all composition target
pass `1/1` each; the TIFF filter passes `699/699` in `5437ms`; and the
full directory suite passes `2751/2751` in `18469ms`, with zero
failures, errors, or skips. Source/DLL export parity remains `461/461`
with zero difference, and the DLL SHA-256 remains
`7B8A9E95408CF59C584495C8E9B9467776A8D60A422560ABB60E1F9D9E46FD6D`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed.
The overall Pillow replacement-readiness estimate moves to `79% ±4%`.

Latest `FMT-TIFF-003BF` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_bigtiff_exif_save.py`) confirms exif object tags land
directly in the BigTIFF IFD0 (inline <= 8 value fields, out-of-line
blobs, reopened through the rational registry even when upstream
degrades scalar rationals to inline SHORT pairs). The ctypes cross-check
(kept in `oracle/probe_tiff_bigtiff_exif_dll_patch.py`) reopens every
patched family through Pillow 11.3.0 with exact values (`FAILURES: 0`).
The classic patch's classifier and blob parser are extracted into shared
builders, and the new `pillow_c_image_patch_tiff_bigtiff_exif_entries`/
`_bytes` exports post-patch plain BigTIFF saves; the facade routes
big_tiff exif (object and Buffer forms) through them and composes exif
with dpi/icc_profile uncompressed. Raw/facade targets pass `1/1` each;
the TIFF filter passes `696/696` in `5000ms`; and the full directory
suite passes `2748/2748` in `18906ms`, with zero failures, errors, or
skips. Release x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL
export parity moves to `461/461` (two deliberate new exports) with zero
difference; and the rebuilt DLL SHA-256 is
`7B8A9E95408CF59C584495C8E9B9467776A8D60A422560ABB60E1F9D9E46FD6D`.
No facade lifetime rule, fallback, or AHK pixel loop changed beyond the
BigTIFF exif save family. The overall Pillow
replacement-readiness estimate moves to `78% ±4%`.

Latest `FMT-TIFF-003BE` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_bigtiff_metadata_save.py`) confirms dpi/icc_profile/
tiffinfo compose with `big_tiff=True` and keep the BigTIFF layout
(inline RATIONAL 282/283, SHORT 296, inline ASCII <= 8, BYTE XMP,
UNDEFINED ICC, even-aligned LONG8 blobs), while Pillow ignores
`big_tiff` when compression is set. The ctypes cross-check (kept in
`oracle/probe_tiff_bigtiff_metadata_dll_save.py`) reopens every
DLL-written combination through Pillow 11.3.0 with exact metadata
(`FAILURES: 0`). The new export
`pillow_c_image_save_tiff_bigtiff_frames_metadata_ascii_entries_options`
generalizes the frames writer, and the facade routes dpi/icc_profile/
tiffinfo through it while keeping compression combos and big_tiff exif
on the classic writer. Raw/facade targets pass `1/1` each; the TIFF
filter passes `694/694` in `5172ms`; and the full directory suite passes
`2746/2746` in `19797ms`, with zero failures, errors, or skips. Release
x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity
moves to `459/459` (one deliberate new export) with zero difference; and
the rebuilt DLL SHA-256 is
`DB37A0C75CE70EFFCA5CC31CD7C880B8684257DCFC4482F8C712D0361781BDE0`.
No facade lifetime rule, fallback, or AHK pixel loop changed beyond the
BigTIFF metadata save family. The overall Pillow
replacement-readiness estimate moves to `77% ±4%`.

Latest `FMT-TIFF-003BD` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_bigtiff_numeric_save.py`) confirms every numeric
`big_tiff=True` save uses the classic-strip layout (count-1 `258` bits
16/32, `339` SampleFormat 2/3 for I/F, photometric 1 or 5, LONG 273/279,
planar 1), and the ctypes cross-check (kept in
`oracle/probe_tiff_bigtiff_numeric_dll_save.py`) reopens every
DLL-written I16/I16B/I/F/CMYK BigTIFF through Pillow 11.3.0 with exact
bytes (`FAILURES: 0`). The frames writer gained the numeric family
(count-1 258 bits, 339, CMYK photometric 5, I16B little-endian swap,
numeric+compression -3 rejection), the strip open route gained the
matching predicates plus big-endian I/F normalization and I;16B
passthrough, and the facade extends the big_tiff mode guard to
CMYK/I;16/I;16B/I/F with Pillow's classic-TIFF fallback for
numeric+compression. Raw/facade targets pass `1/1` each; the TIFF filter
passes `692/692` in `5157ms`; and the full directory suite passes
`2744/2744` in `18109ms`, with zero failures, errors, or skips. Release
x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity
remains `458/458` with zero difference; and the rebuilt DLL SHA-256 is
`B893B53F49D4B3B20F7BD4C0AD79FBAEC2A2F15374BC8DE16C2A37A2BBFC4DDE`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed
beyond the numeric save/open family. The overall Pillow
replacement-readiness estimate moves to `76% ±4%`.

Latest `FMT-TIFF-003BC` verification: rechecking the next pointers (kept
in `oracle/probe_tiff_classic_two_frame_save.py` and
`oracle/probe_tiff_bigtiff_two_frame_save.py`) CORRECTS the round-16
note — Pillow 11.3.0's `save_all` output (classic AND `big_tiff`) is
CHAIN-LINKED with each page's own inline header as a writer artifact, so
the DLL's chained readers already open Pillow-written multi-frame files.
Hand-built oracle-layout fixtures (classic 256-byte, BigTIFF 448-byte
two-frame files) lock the interop in; raw/facade targets pass `1/1` and
`1/1`; the TIFF filter passes `686/686` in `5063ms`; and the full
directory suite passes `2738/2738` in `18453ms`, with zero failures,
errors, or skips. No native change; source/DLL export parity remains
`458/458` with zero difference; and the DLL SHA-256 remains
`A71C8407B801A45AF5C86A980E59146B966C10C4B9F6342AD6AE4D41C022EB41`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The overall
Pillow replacement-readiness estimate moves to `75% ±4%`.

Latest `FMT-TIFF-003BB` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_bigtiff_two_frame_save.py`) confirms Pillow's own
`save_all`+`big_tiff` emits CONCATENATED single-frame BigTIFFs, while the
new `pillow_c_image_save_tiff_bigtiff_frames_compression_options` export
writes standard chained-IFD multi-frame BigTIFF (same-mode frames,
per-frame strip offsets, u64 next pointers) and the facade composes
`big_tiff`+`save_all`+`append_images`. The chained layout reopens in both
readers; opening Pillow's concatenated layout is the next gap. Raw
chained two-frame and facade save_all targets pass `1/1` each; the TIFF
filter passes `684/684` in `4985ms`; and the full directory suite passes
`2736/2736` in `18687ms`, with zero failures, errors, or skips. Release
x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity is
`458/458` (one deliberate new export) with zero difference; and the
rebuilt DLL SHA-256 is
`A71C8407B801A45AF5C86A980E59146B966C10C4B9F6342AD6AE4D41C022EB41`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The overall
Pillow replacement-readiness estimate moves to `74% ±4%`.

Latest `FMT-TIFF-003BA` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_bigtiff_compressed_save.py`) confirms Pillow's
`big_tiff`+compression silently falls back to classic TIFF (the libtiff
encoder ignores `big_tiff`), so compressed BigTIFF round trips are a
standards extension with the open side covering compressed BigTIFF from
other writers. The new `pillow_c_image_save_tiff_bigtiff_compression_-
options` export reuses the existing PackBits/LZW/Adobe-Deflate encoders,
and the strip open route decodes through the shared decoder seam. The raw
compression matrix (3 compressions x 4 modes) and the facade
`big_tiff`+`compression` composition pass `1/1` each; the TIFF filter
passes `683/683` in `5750ms`; and the full directory suite passes
`2735/2735` in `18359ms`, with zero failures, errors, or skips. Release
x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity is
`457/457` (one deliberate new export) with zero difference; and the
rebuilt DLL SHA-256 is
`8034E4AB70BD9475DE31595AE019A270F603FB7EAEA02CDDD3A7A7AC4A7C2FB5`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The overall
Pillow replacement-readiness estimate moves to `73% ±4%`.

Latest `FMT-TIFF-003AZ` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_bigtiff_save.py` and
`oracle/probe_tiff_bigtiff_tiled_save.py`) confirms the option name
`big_tiff`, the strip-only save layout, and that Pillow ignores `tile=`
for BigTIFF saves. The new `pillow_c_image_save_tiff_bigtiff` export
writes that exact layout for the uncompressed single-frame L/RGB/RGBA/LA
matrix, and the private `parse_tiff_bigtiff_strip_image_for_ifd` open
route plus dispatcher fallbacks make Pillow-written strip BigTIFFs reopen
natively. Raw/facade BigTIFF save and strip-open targets pass `1/1` each;
the TIFF filter passes `682/682` in `5109ms`; and the full directory suite
passes `2734/2734` in `19265ms`, with zero failures, errors, or skips.
Release x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export
parity is `456/456` (one deliberate new export) with zero difference; and
the rebuilt DLL SHA-256 is
`9A1726906A0EB96F19D4222CCC6E1BB2271CA2D9D96D18A888BD99E6B6933E1E`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The overall
Pillow replacement-readiness estimate moves to `72% ±4%`.

Latest `FMT-TIFF-003AY` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_exif_save.py`) confirms the `exif=` bytes form parses
the caller's EXIF blob into IFD0 with the same reopen behavior as the
object form. The new `pillow_c_image_patch_tiff_exif_bytes` export parses
`Exif\0\0`-prefixed or bare MM TIFF blobs into the bounded tag families
(ascii with NUL stripping, uint scalars, rational/signed-rational
scalars, rational/short/LONG arrays, byte and undefined blobs, 4096-entry
and 0xFFFFFF-count caps) and patches IFD0 through the same vector path;
the facade `Image.Save` accepts a Buffer for `exif=`. Raw/facade
bytes-form targets pass `1/1` and `1/1`; the TIFF filter passes `679/679`
in `5297ms`; and the full directory suite passes `2731/2731` in `19171ms`,
with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `455/455` (one
deliberate new export) with zero difference; and the rebuilt DLL SHA-256
is `8CE63E4837EF18A39A16124FE2187032AF83FC624C86538BEAD93E5DD0BDFC05`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The overall
Pillow replacement-readiness estimate moves to `71% ±4%`.

Latest `FMT-TIFF-003AX` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_exif_combos.py`) confirms `exif=`+`dpi=` and
`exif=`+`icc_profile=` compose with every exif tag surviving, while
`tiffinfo` takes precedence and silently drops `exif`. The facade mirrors
the drop with one guard in `SaveTiffFrames`, and the raw patch test gained
a dpi-saved base case (collision rule keeps the base 282/283/296 trio).
Raw/facade compose targets pass `1/1` and `1/1`; the TIFF filter passes
`677/677` in `5453ms`; and the full directory suite passes `2729/2729` in
`19578ms`, with zero failures, errors, or skips. No native change;
source/DLL export parity remains `454/454` with zero difference; and the
DLL SHA-256 remains
`A008AA7ED59172DE327959B3DEA6103A19CDA6E122A650C7CC79D773FCFC53D9`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The overall
Pillow replacement-readiness estimate moves to `70% ±4%`.

Latest `FMT-TIFF-003AW` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_exif_save.py` and the new
`oracle/probe_tiff_exif_compression.py`) confirms `exif=` writes `318`
rational arrays and `50719` LONG arrays directly into IFD0; scalar exif
composes with every TIFF compression, while array exif plus compression
fails upstream in libtiff (`RuntimeError: Error setting from dictionary`) —
the DLL's generic offset-shifting patch keeps working for that combo
(benign superset note). `pillow_c_image_patch_tiff_exif_entries` gains the
type-5 count-N rational-array and type-4 count-N LONG-array groups, and
the facade `PatchTiffExifEntries` drops its two array serialization
boundaries. The raw/facade exif targets were extended in place (rational
array 318, LONG array 50719, `compression="lzw"` composition) and pass
`1/1` and `1/1`; the TIFF filter passes `676/676` in `5015ms`; and the
full directory suite passes `2728/2728` in `18360ms`, with zero failures,
errors, or skips. Release x64 Rebuild has `0 Warning(s), 0 Error(s)`;
source/DLL export parity remains `454/454` with zero difference; and the
rebuilt DLL SHA-256 is
`A008AA7ED59172DE327959B3DEA6103A19CDA6E122A650C7CC79D773FCFC53D9`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The overall
Pillow replacement-readiness estimate moves to `69% ±4%`.

Latest `FMT-TIFF-003AV` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_exif_save.py`) confirms Pillow writes `exif=` tags
directly into IFD0 for both the Exif-object and raw-bytes forms, and the
unit-2 `282`/`283`/`296` trio drives reopened `info dpi`. The new
`pillow_c_image_patch_tiff_exif_entries` export post-patches a saved classic
TIFF's IFD0 with the caller's EXIF families, merging entries ascending,
shifting offsets, and appending blobs; the facade `Image.Save` routes
`exif=<Image.Exif>` through the existing save seams and then the patch.
Raw/facade exif targets pass `1/1` and `1/1`; the TIFF filter passes
`676/676` in `4954ms`; and the full directory suite passes `2728/2728` in
`18453ms`, with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `454/454` (one
deliberate new export) with zero difference; and the rebuilt DLL SHA-256 is
`43E2466126F9C0ABD1110F654784D8EEBDF5BC6AFB20E75E6CBA7B43B60DE51A`.
No facade lifetime rule, fallback, or AHK pixel loop changed. The overall
Pillow replacement-readiness estimate moves to `68% ±4%`.

Latest `FMT-TIFF-003AU` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_bigtiff_subifd.py`) confirms that a BigTIFF fixture with
ExifIFD (34665) and GPSInfo (34853) sub-IFDs keeps the sub-IFD tags ONLY in
`Exif.get_ifd(0x8769)`/`get_ifd(0x8825)` while the flat `getexif()` holds
just the pointer values — the same container split as the classic route.
The DLL flattens one BigTIFF sub-IFD level into `tiff_exif` through the
private `collect_tiff_bigtiff_exif_entries` seam reusing the shared
`TiffExifCollector` struct, with 64-bit count reads, 20-byte-entry span
checks, a 4096-entry cap, and the bounded GPS tag sets; per-frame
attachment makes the flattening per-frame. Raw/facade BigTIFF sub-IFD
targets pass `1/1` and `1/1`; the TIFF filter passes `674/674` in `5094ms`;
and the full directory suite passes `2726/2726` in `18515ms`, with zero
failures, errors, or skips. Release x64 Rebuild has `0 Warning(s),
0 Error(s)`; source/DLL export parity is `453/453` with zero difference;
and the rebuilt DLL SHA-256 is
`D80CF40D8D44AF04830C89EC577B4CCC34529EAAE8AEC9FA68D6B765EDCC8671`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `67% ±4%`.

Latest `FMT-TIFF-003AT` verification: the Pillow 11.3.0 oracle (kept in
`oracle/probe_tiff_subifd.py`) confirms that a classic strip TIFF with
ExifIFD (34665) and GPSInfo (34853) sub-IFDs keeps the sub-IFD tags ONLY in
`Exif.get_ifd(0x8769)`/`get_ifd(0x8825)` while the flat `getexif()` holds
just the pointer values. The DLL flattens one sub-IFD level into
`tiff_exif` through the private `TiffExifCollector`/`collect_tiff_exif_-
entries` seam plus bounded GPS tag sets, so facade `GetExif()` exposes the
sub-IFD tags alongside the pointers; the flat-vs-get_ifd container
difference is a recorded divergence and `get_ifd()` itself an explicit
boundary. The four legacy pointer fixtures now assert their flattened
contents. Raw/facade sub-IFD targets pass `1/1` and `1/1`; the TIFF filter
passes `672/672` in `5015ms`; and the full directory suite passes
`2724/2724` in `19016ms`, with zero failures, errors, or skips. Release x64
Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity is
`453/453` with zero difference; and the rebuilt DLL SHA-256 is
`6738D442FCA4F49D19259E951B0D93DB18BA8D8E4CBDA8A05D107001B20F987F`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `66% ±4%`.

Latest `FMT-TIFF-003AS` verification: the Pillow 11.3.0 oracle confirms the
malformed-BigTIFF-metadata surface — truncated out-of-line blobs,
count-overflow rationals, and zero-count blobs open with exact pixels and
the tag absent; invalid-type entries are reinterpreted by tag semantics
(`exif[270] == 7` for a type-4 ascii entry, `exif[282] == 2` for a type-3
rational entry) and zero denominators expose `exif[33434] == nan`. The DLL
keeps the classic strict-type convention and skips the malformed entries;
those two Pillow behaviors are recorded as documented divergences. Raw and
facade malformed matrices pass `1/1` and `1/1` with zero native changes
(the BigTIFF readers already validated type, count, offset, and
denominator); the BigTIFF filter passes `44/44` in `953ms`; the TIFF filter
passes `670/670` in `4937ms`; and the full directory suite passes
`2722/2722` in `20625ms`, with zero failures, errors, or skips. No native
rebuild; source/DLL export parity remains `453/453` with zero difference;
and the DLL SHA-256 remains
`BC81F6446CC71FA32B5B9B9CB87C8782B6A39E362F508A67C9D162F37B1F425E`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `65% ±4%`.

Latest `FMT-TIFF-003AR` verification: an `MM` BigTIFF fixture carrying the
rational/signed-rational/double/float/byte/undefined families plus the
unit-2 `282`/`283`/`296` trio reopens through the existing endian-aware
readers with zero native changes. The raw/facade typed fixture builders
gained a big-endian inline-field layout (left-justified value bytes, zero
padding) and type-3/type-4 raw value handling, plus
`PillowCPackBeRationalBytes`/`PillowTestPackBeRationalBytes`. Raw/facade MM
metadata targets pass `1/1` and `1/1`; the BigTIFF filter passes `42/42` in
`1047ms`; the TIFF filter passes `668/668` in `4875ms`; and the full
directory suite passes `2720/2720` in `18750ms`, with zero failures, errors,
or skips. No native rebuild; source/DLL export parity remains `453/453` with
zero difference; and the DLL SHA-256 remains
`BC81F6446CC71FA32B5B9B9CB87C8782B6A39E362F508A67C9D162F37B1F425E`.
Pillow 11.3.0 itself rejects valid MM BigTIFF, so this is a standards
extension and the estimate stays `64% ±4%`.

Latest `FMT-TIFF-003AN`–`FMT-TIFF-003AQ` verification: the Pillow 11.3.0
oracle confirms BigTIFF family-matrix parity — rational scalars/arrays,
signed rational scalars/arrays, double arrays, float arrays, byte arrays,
and undefined blobs all open on the 4×3, 2×2 chunky tiled BigTIFF shape
with the count-≤-8 inline value-field rule; Pillow exposes floats for the
rational families (the DLL/facade keep the established `[num, den]` pair
contract — recorded divergence) and raw bytes for byte/undefined families.
Native `read_tiff_bigtiff_rational_entry_value` /
`read_tiff_bigtiff_rational_array_entry_value`,
`read_tiff_bigtiff_signed_rational_entry_value` /
`read_tiff_bigtiff_signed_rational_array_entry_value`,
`read_tiff_bigtiff_double_array_entry_value`, and
`read_tiff_bigtiff_float_array_entry_value` read 64-bit counts with inline
and LONG8-offset layouts, and `build_tiff_bigtiff_common_ascii_exif_for_ifd`
serializes the full classic family matrix plus the unit-2
`282`/`283`/`296` resolution trio with the classic size rules; facade
`GetExif()` exposes numeric arrays and byte buffers with no facade change.
Raw/facade rational, double/float, and byte/undefined targets pass `1/1`
each; the BigTIFF filter passes `40/40` in `953ms`; the TIFF filter passes
`666/666` in `11031ms`; and the full directory suite passes `2718/2718` in
`25766ms`, with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `453/453` with zero
difference; and the rebuilt DLL SHA-256 is
`BC81F6446CC71FA32B5B9B9CB87C8782B6A39E362F508A67C9D162F37B1F425E`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate moves to `64% ±4%`.

Latest `FMT-TIFF-003AM` verification: the Pillow 11.3.0 oracle confirms
BigTIFF SHORT-array parity — a nine-tag matrix covering inline (530/291/297/
42081/37396) and out-of-line (34735/342/50712/50829) type-3 arrays exposes
exact tuples through `getexif()`. Native
`read_tiff_bigtiff_ushort_array_entry_value` reads 64-bit counts with inline
and LONG8-offset layouts, and
`build_tiff_bigtiff_common_ascii_exif_for_ifd` serializes the classic
SHORT-array tag set with the classic size rules, including multi-channel
`258` arrays; facade `GetExif()` exposes the arrays with no facade change.
Raw and facade SHORT-array targets pass `1/1` and `1/1`; TIFF filters pass
`331/331` in `1219ms` and `329/329` in `3610ms`; and the full directory
suite passes `2712/2712` in `19203ms`, with zero failures, errors, or skips.
Release x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity
is `453/453` with zero difference; and the rebuilt DLL SHA-256 is
`A8F32EC557E2880BAB4D6B0F5ED75C8AF18A7AB6AA45191D045E05902D6D81BE`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate remains `62% ±4%`.

Latest `FMT-TIFF-003AL` verification: the Pillow 11.3.0 oracle confirms
BigTIFF uint-array parity — `exif[324]`/`exif[325]` tuples match each
fixture's tile offsets and counts, and an inline type-4 count-2 `279` array
reopens as `(12, 34)`. Native `read_tiff_bigtiff_uint_array_entry_value`
reads type-4/type-16 arrays with 64-bit counts, inline and LONG8-offset
layouts, and uint32-fitting LONG8 validation, and
`build_tiff_bigtiff_common_ascii_exif_for_ifd` serializes LONG8 `324`/`325`,
type-4 `273`/`279`, and the bounded 50719/50720/50829/50830/50937/50981/
51089/51090/51091/52536 families with the classic expected-count rules;
facade `GetExif()` exposes the arrays with no facade change. Raw and facade
uint-array targets pass `1/1` and `1/1`; TIFF filters pass `330/330` in
`1141ms` and `328/328` in `3765ms`; and the full directory suite passes
`2710/2710` in `18890ms`, with zero failures, errors, or skips. Release x64
Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity is `453/453`
with zero difference; and the rebuilt DLL SHA-256 is
`FABED34225AA68493F81800D06BEF179626FB8A1B93D86B3395901419A82121C`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate remains `62% ±4%`.

Latest `FMT-TIFF-003AK` verification: the Pillow 11.3.0 oracle confirms
BigTIFF scalar uint parity — the base tiled tags (256=4, 257=3, 258=8,
259=1, 262=1, 277=1, 284=1, 322=2, 323=2) plus SHORT 317=1/340=0/341=255 and
a LONG8 339=1 scalar expose as integers through `getexif()`. Native
`read_tiff_bigtiff_uint_scalar_entry_value` reads count-1 SHORT/LONG/LONG8
scalars from the inline value field and normalizes uint32-fitting LONG8 into
the serializer's LONG shape, and
`build_tiff_bigtiff_common_ascii_exif_for_ifd` serializes the
`tiff_common_uint_tag` set alongside ascii/orientation/undefined; the plain
tiled fixture now always yields a HasExif blob and facade `GetExif()` exposes
the integers with no facade change. Raw and facade uint targets pass `1/1`
and `1/1`; TIFF filters pass `330/330` in `1203ms` and `328/328` in `6235ms`;
and the full directory suite passes `2710/2710` in `18500ms`, with zero
failures, errors, or skips. Release x64 Rebuild has `0 Warning(s), 0 Error(s)`;
source/DLL export parity is `453/453` with zero difference; and the rebuilt
DLL SHA-256 is
`9DC6FDAC375ABEB29AB8F594FE828E17F4EAC87B70CA86A56D9D9E038955EC28`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate remains `62% ±4%`.

Latest `FMT-TIFF-003AJ` verification: the Pillow 11.3.0 oracle confirms that
BigTIFF ASCII (type-2) entries follow the same inline/offset rule as other
blobs — count ≤ 8 inline in the value field, larger values at LONG8 offsets —
and that out-of-line `exif[270] == "Document Alpha"` / `exif[315] == "Ada
Lovelace"` plus inline `exif[305] == "pillow-c"` / `exif[315] == "A"` expose
through per-frame `getexif()` with no ascii `info` keys. Native
`read_tiff_bigtiff_ascii_entry_value` performs NUL stripping like the
classic reader, and `build_tiff_bigtiff_common_ascii_exif_for_ifd` serializes
the `tiff_common_ascii_tag` set alongside orientation and 34675/700 undefined
tags; facade `GetExif()` exposes the strings with no facade change. Raw and
facade ascii targets pass `1/1` and `1/1`; TIFF filters pass `329/329` in
`1406ms` and `327/327` in `3765ms`; and the full directory suite passes
`2708/2708` in `18438ms`, with zero failures, errors, or skips. Release x64
Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity is `453/453`
with zero difference; and the rebuilt DLL SHA-256 is
`266F6DBAE44FDB56AE981B63D4CF075892AEF5D94575232610F7ECE3B53178C7`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate remains `62% ±4%`.

Latest `FMT-TIFF-003AI` verification: the Pillow 11.3.0 oracle confirms that
BigTIFF RATIONAL `282`/`283` with count 1 occupy exactly eight bytes and sit
inline in the value field, so the unit-2 trio reopens with exact doubles
`info["dpi"] == (300.0, 150.0)` and `(145.5, 144.0)` for `291/2`. Pillow's
no-unit raw-dpi, centimeter-to-dpi `2.54` conversion, and absent-tag `(1, 1)`
defaults are recorded as boundary notes; the DLL keeps the classic-route
convention of requiring the explicit unit-2 trio.
`parse_tiff_bigtiff_resolution_for_ifd` reads the 32-bit numerator and
denominator halves with the decoded byte order and rejects zero denominators;
`attach_tiff_bigtiff_metadata_for_ifd` fills `has_dpi`/`dpi_x`/`dpi_y` per
frame, and the facade surfaces `Info["dpi"]` through the existing
`pillow_c_image_metadata_resolution` export with no facade change. Raw and
facade DPI targets pass `1/1` and `1/1`; TIFF filters pass `328/328` in
`1140ms` and `326/326` in `3657ms`; and the full directory suite passes
`2706/2706` in `18234ms`, with zero failures, errors, or skips. Release x64
Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity is `453/453`
with zero difference; and the rebuilt DLL SHA-256 is
`2689C9A364C3671356D6D06C4D546708FA9A7E977EB8AB84E252318BA8D436A0`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate remains `62% ±4%`.

Latest `FMT-TIFF-003AH` verification: the two-frame Pillow 11.3.0 oracle
probe confirms per-frame reload for BigTIFF metadata — `seek(0)`/`seek(1)`
expose each IFD's own ICC/XMP through `info["icc_profile"]`, `info["xmp"]`,
and a per-frame `getexif()` with distinct `exif[34675]`/`exif[700]`. When IFD1
lacks a tag, Pillow deletes `info["xmp"]` and empties `getexif()` but leaves
`info["icc_profile"]` stale; the DLL keeps fresh per-frame handles and
records the staleness as an oracle note. Native `attach_tiff_bigtiff_ifd0_metadata`
generalizes to `attach_tiff_bigtiff_metadata_for_ifd` for every frame index;
the facade XMP-refresh deletion now covers TIFF so `Seek(1)` without XMP
removes the stale `Info["xmp"]`. Raw and facade per-frame targets pass `1/1`
and `1/1`; TIFF filters pass `327/327` in `1125ms` and `325/325` in `3609ms`;
and the full directory suite passes `2704/2704` in `17469ms`, with zero
failures, errors, or skips. Release x64 Rebuild has `0 Warning(s), 0 Error(s)`;
source/DLL export parity is `453/453` with zero difference; and the rebuilt
DLL SHA-256 is
`4F9ED0A4222300A8919B1B7E362A2687D210603E0F75166794FBADD586EE3470`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate remains `62% ±4%`.

Latest `FMT-TIFF-003AG` verification: a local Pillow 11.3.0 oracle built from
the exact 4×3, 2×2 chunky tiled little-endian BigTIFF fixture confirms
`info["icc_profile"]` and `info["xmp"]` byte parity for out-of-line type-7 and
type-1 ICC (twelve bytes) and XMP (324 bytes), and the count-≤-8 inline
value-field behavior for a six-byte ICC. Native
`read_tiff_bigtiff_blob_entry_value`, `parse_tiff_bigtiff_icc_profile_for_ifd`,
`parse_tiff_bigtiff_xmp_for_ifd`,
`build_tiff_bigtiff_common_ascii_exif_for_ifd`, and
`attach_tiff_bigtiff_ifd0_metadata` attach the IFD0 blobs only for frame 0; a
two-frame raw test proves nonzero-IFD metadata stays separate. Raw and facade
targeted matrices pass `1/1` and `1/1`; TIFF filters pass `327/327` in
`1125ms` and `324/324` in `3782ms`; and the full directory suite passes
`2703/2703` in `18625ms`, with zero failures, errors, or skips. Release x64
Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity is `453/453`
with zero difference; and the rebuilt DLL SHA-256 is
`84E80429DDD51C7ACE54ECA5EB89604F9DC41971DA7AD6C039958F08FD1BA06C`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate remains `62% ±4%`.

Latest `FMT-TIFF-003AF` verification: the pinned Pillow 11.3.0 source archive
documents the upstream endian-invalid BigTIFF test `ifh[2] == 43`; valid
`MM 00 2B` fixtures are therefore used as a standards-extension matrix rather
than a positive Pillow oracle. Native header/IFD/LONG8/next-IFD reads now share
one decoded byte order. MM unsigned 16-bit returns `I;16B`, MM signed `I` and
float `F` normalize to little-endian DLL storage, and byte/planar/RGBX/LA/CMYK
samples remain unswapped. MM single-frame raw/facade targets pass `2/2` and
`2/2`; II/MM two-frame compressed targets pass `1/1` and `1/1`; TIFF filters
pass `325/325` in `1531ms` and `323/323` in `4016ms`; and the full directory
suite passes `2700/2700` in `19625ms`, with zero failures, errors, or skips.
Release x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity is
`453/453` with zero difference; and the rebuilt DLL SHA-256 is
`AE3EC777D5CAD0DCB6B242BBFA8F9DFD38BDD8062D94908E1B8BB635F1191EF1`.
No export, facade lifetime rule, fallback, or AHK pixel loop changed. The
overall Pillow replacement-readiness estimate remains `62% ±4%`.

Latest `FMT-TIFF-003AE` verification: a 16-case local Pillow 11.3.0 oracle
confirms exact BigTIFF tiled bytes for `I;16`, signed `I`, float `F`, and
`CMYK` under raw, PackBits, TIFF LZW, and Adobe Deflate. Native open reads
scalar SampleFormat, maps two/four-byte storage, and reuses LONG8 validation,
tile decoding, clipping, and row copies. Raw/facade REDs were `-3` and
`pillow_c: invalid argument`; targeted GREEN is `1/1` and `1/1`. TIFF filters
pass `323/323` and `321/321`; the full directory suite passes `2696/2696` in
`19407ms`, with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `453/453` with zero
difference; and the rebuilt DLL SHA-256 is
`6D508D23C3043F1D3B0BE15412B18D17FB5BB14DBC77B8E2BC773E51D0D17CCF`. No
export, facade lifetime rule, fallback, or AHK pixel loop changed.

Latest `FMT-TIFF-003AD` verification: the cached Pillow 11.3.0 source and a
20-case local oracle cover planar-separate `L`, `RGB`, `RGBA`, `RGBX`, and
`LA` under raw, PackBits, TIFF LZW, and Adobe Deflate. All 15 compressed cases
load; RGBX exposes public RGB without X, while compressed LA exposes the
libtiff-produced zero alpha plane. Raw planar RGBX/LA raise `unknown raw mode`
in Pillow and remain rejected. The raw and facade REDs were native `-3` and
`pillow_c: invalid argument`; targeted GREEN is `1/1` and `1/1`. TIFF filters
pass `322/322` and `320/320`; the full directory suite passes `2694/2694` in
`19109ms`, with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `453/453` with zero
difference; and the rebuilt DLL SHA-256 is
`B0929C5AB1CB8592A1CE0265A37F0D9EC7EA22D1E43969EAAB4A7346F1663DDF`. No
export, facade lifetime rule, fallback, or AHK pixel loop changed.

Latest `FMT-TIFF-003AC` verification: the cached Pillow 11.3.0 source and
local oracle confirm the fixed 4×3, 2×2 BigTIFF planar-separate fixture for
`L`, `RGB`, and `RGBA`. Plane-major LONG8 tile arrays reopen with exact public
bytes. The corresponding planar `LA` probe fails in Pillow itself during
`load()`/`tobytes()` with `ValueError: unknown raw mode for given image mode`,
so it is explicitly excluded. Native open validates one tile plane per sample,
uses one-channel tile strides, clips edge tiles, and interleaves output in the
DLL. Targeted raw/facade tests pass `1/1` and `1/1`; TIFF filters pass
`321/321` and `319/319`; and the full directory suite passes `2692/2692` in
`18578ms`, with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `453/453` with zero
difference; and the rebuilt DLL SHA-256 is
`3D37666B94FD63654DE5D850B69FD0569847F1ACDA1F4F7D8A7310914411726E`. No
export, facade lifetime rule, fallback, or AHK pixel loop changed.

Latest `FMT-TIFF-003AB` verification: the cached Pillow 11.3.0 source and a
paired local oracle probe confirm that BigTIFF
`Photometric=2`/`BitsPerSample=[8,8,8,8]`/`SamplesPerPixel=4` with explicit
`ExtraSamples=0` opens as public `RGB` with rawmode `RGBX`; omitting tag 338
instead opens as `RGBA/RGBA`. The corrected 4×3, 2×2 chunky tiled raw/facade
fixtures therefore preserve explicit zero presence. Native open validates
LONG8 tile arrays using a four-byte input stride, allocates three-channel RGB,
and skips every X byte during clipped copies. Targeted raw/facade tests pass
`1/1` and `1/1`; TIFF filters pass `320/320` and `318/318`; and the full
directory suite passes `2690/2690` in `19078ms`, with zero failures, errors,
or skips. Release x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export
parity is `453/453` with zero difference; and the rebuilt DLL SHA-256 is
`005EC635204D81FBEE7F4C1FFBA756AD1ECB955B6CEC4CACBAD0857C1158F9A1`. No
export, facade lifetime rule, fallback, or AHK pixel loop changed.

Latest `FMT-TIFF-003AA` verification: the cached Pillow 11.3.0 source and
local oracle confirm BigTIFF tag `274` Orientation 2–8 for the fixed 4×3,
2×2 chunky tiled `L`/`RGB`/`RGBA`/`LA` matrix. Orientations 2–4 preserve
`4×3`; 5–8 swap to `3×4`. The native parser reads the scalar orientation and
reuses the DLL transform helper after tile reconstruction; raw/facade targeted
tests pass `1/1` and `1/1`, TIFF filters pass `319/319` and `317/317`, and the
full directory suite passes `2688/2688` in `18500ms`, with zero failures,
errors, or skips. Release x64 Rebuild has `0 Warning(s), 0 Error(s)`;
source/DLL export parity is `453/453` with zero difference; and the rebuilt
DLL SHA-256 is
`6C4D462439E30467C67BBC599DEBA3B13307B23225B13519FC2F782D84147141`. No
export, facade lifetime rule, or AHK pixel loop changed. BigTIFF RGBX storage,
planar BigTIFF, and broader modes remain separate.

Latest `FMT-TIFF-003Z` verification: the cached Pillow 11.3.0 source archive
and local oracle confirm two chained little-endian BigTIFF IFDs for the fixed
4×3, 2×2 chunky tiled matrix. PackBits (`32773`) and Adobe Deflate (`8`) both
reopen through Pillow with `n_frames=2` and exact frame-1 bytes; the AHK
matrix also covers TIFF LZW (`5`) across `L`, `RGB`, `RGBA`, and `LA`. The
native parser now selects nonzero BigTIFF frames and the count route validates
the 64-bit next-IFD chain with repeated-offset rejection. Raw/facade targeted
tests pass `1/1` and `1/1`; TIFF filters pass `318/318` and `316/316`; and the
full directory suite passes `2686/2686` in `18172ms`, with zero failures,
errors, or skips. Release x64 Rebuild has `0 Warning(s), 0 Error(s)`;
source/DLL export parity is `453/453` with zero difference; and the rebuilt
DLL SHA-256 is
`AAA43C1B099D5C6DADDFC69969A603482A12C43120FF9C6BF88ADF38AB0C37E3`. No
export, facade lifetime rule, or AHK pixel loop changed. BigTIFF Orientation,
RGBX planar storage, and broader BigTIFF mode breadth remain separate.

Latest `FMT-TIFF-003Y` verification: the cached Pillow 11.3.0 source archive
and local source mapping confirm PackBits, TIFF LZW, and Adobe Deflate for the
single-frame BigTIFF `L`/`RGB`/`RGBA`/`LA` matrix. Native tile decompression and
clipped copies stay in the DLL; raw/facade targeted tests pass `1/1` and
`1/1`, and the pre-003Z TIFF filters passed `317/317` and `315/315`. No ABI
symbol or facade lifetime rule changed.

Latest `FMT-TIFF-003X` verification: the native TIFF route now accepts the
bounded little-endian BigTIFF shape with magic `43`, offset size `8`, one
20-byte-entry IFD, scalar SHORT/LONG/LONG8 fields, and LONG8 tile offset/count
arrays. The same DLL-owned uncompressed chunky tiled reconstruction covers
`L`, `RGB`, `RGBA`, and `LA` on the fixed 4×3, 2×2 row-major fixture, including
right/bottom clipping and single-frame count validation. The raw/facade REDs
failed with native `-3` / `pillow_c: invalid argument`; after the native parser
and facade matrix tests were added, targeted raw/facade tests pass `2/2` and
`2/2`, TIFF filters pass `316/316` and `314/314`, and the full directory suite
passes `2680/2680` in `19078ms` with zero failures, errors, or skips. Release
x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity is
`452/452` with zero difference; and the rebuilt DLL SHA-256 is
`D7CE5A49145F26A231C95EF522F64355B3E1BEFAA44355BB1DCB578306583A2D`. No
export, facade lifetime rule, or AHK pixel loop changed. The compressed
single-frame and two-frame BigTIFF children are covered; BigTIFF Orientation
combinations, RGBX planar storage, and broader BigTIFF mode breadth remain
separate.

Latest `FMT-TIFF-003W` verification: the native chunky tiled route now
accepts TIFF Orientation `2` through `8` for the fixed 4×3, 2×2 uncompressed
matrix covering `L`, `RGB`, `RGBA`, and `LA`. The local Pillow 11.3.0 oracle
confirmed tiled Orientation 6 dimensions and bytes; the raw RED returned
native `-3`, and the facade RED raised `pillow_c: invalid argument`. After
sharing the existing native orientation transform helper between strip and
tiled routes, targeted raw/facade tests pass `1/1` and `1/1` (each covers all
four modes and seven orientations); TIFF filters pass `314/314` and
`312/312`; and the full directory suite passes `2676/2676` in `18422ms`,
with zero failures, errors, or skips. Release x64 Rebuild has `0 Warning(s),
0 Error(s)`; source/DLL export parity is `452/452` with zero difference; and
the rebuilt DLL SHA-256 is
`ABE167C3F4BB52A5E4E3A5F6A5AA48FCC363F81EAEC4696CFABBC963930E1A26`.
The DLL owns transformed allocation, dimension changes, and pixel copies; no
facade lifetime rule, ABI export, or AHK pixel loop changed.

Latest `FMT-TIFF-003V` verification: the existing generalized native per-IFD
chunky tiled route now has raw/facade proof for a third chained IFD in the
4×3, 2×2 uncompressed `L`, `RGB`, `RGBA`, and `LA` matrix. The tests assert
`n_frames=3`, seek/open frame 2, clipped edge bytes, and existing DLL-owned
lifetime. Targeted raw/facade tests pass `1/1` and `1/1`; TIFF filters pass
`313/313` and `311/311`; and the full directory suite passes `2674/2674` in
`18031ms`, with zero failures, errors, or skips. No native rebuild was
required; source/DLL export parity remains `452/452` with zero difference;
and the current DLL SHA-256 remains
`83FF14E210B7FE5791616C1D55AC7999164844B24013D1A08E8A16757DC751F9`.
No facade lifetime rule, ABI export, or AHK pixel loop changed.

Latest `FMT-TIFF-003U` verification: the cached Pillow 11.3.0 source archive
and local oracle fix a two-frame, uncompressed chunky tiled 4×3 TIFF with 2×2
row-major tiles for `L`, `RGB`, `RGBA`, and `LA`. The raw RED was
`Expected 2, got 1`. After routing nonzero frames through the native per-IFD
parser and walking the validated next-IFD chain in the frame-count export,
targeted raw/facade tests pass `2/2` and `2/2`; TIFF filters pass `312/312`
and `310/310`; and the full directory suite passes `2672/2672` in `18782ms`,
with zero failures, errors, or skips. Release x64 Rebuild has `0 Warning(s),
0 Error(s)`; source/DLL export parity is `452/452` with zero difference; and
the rebuilt DLL SHA-256 is
`83FF14E210B7FE5791616C1D55AC7999164844B24013D1A08E8A16757DC751F9`.
The DLL owns frame allocation, tile reads, clipping, and copied bytes; no
facade lifetime rule, ABI export, or AHK pixel loop changed.

Latest `FMT-TIFF-003T` verification: the cached Pillow 11.3.0 source archive
and local compressed planar fixtures cover PackBits, TIFF LZW, and Adobe
Deflate plane tiles for the 4×3, 2×2 `L`, `RGB`, `RGBA`, and `LA` matrix.
The existing native per-tile decoder seam already handled the route; targeted
raw/facade tests pass `1/1` and `1/1`; TIFF filters pass `310/310` and
`308/308`; and the full directory suite passes `2668/2668` in `18969ms`,
with zero failures, errors, or skips. No native source changed in that packet;
source/DLL export parity remained `452/452`, and the DLL SHA-256 remained
`A49F66602B02F097D3E42596BA75F33C64FB8BF7EB2ABBEAE775914F5784DA0F`.

Latest `FMT-TIFF-003S` verification: the cached Pillow 11.3.0 source archive
and local Pillow oracle fix an uncompressed `PlanarConfiguration=2` 4×3,
2×2-tiled matrix for `L`, `RGB`, `RGBA`, and `LA`, with plane-major LONG tile
arrays and padded right/bottom edge tiles. The raw RED failed with native
`-3`; the facade RED raised `pillow_c: invalid argument`. After the native
plane-major validation, single-channel decode, edge clipping, and DLL-owned
interleave route, targeted raw/facade tests pass `1/1` and `1/1`; TIFF filters
pass `309/309` and `307/307`; and the full directory suite passes `2666/2666`
in `18625ms`, with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `452/452` with zero
difference; and the rebuilt DLL SHA-256 is
`A49F66602B02F097D3E42596BA75F33C64FB8BF7EB2ABBEAE775914F5784DA0F`.
The DLL owns allocation, tile reads, clipping, and interleaving; no ABI,
facade lifetime rule, or AHK pixel loop changed. Compressed planar storage,
multi-frame tiled files, tiled orientation, and BigTIFF remain explicit
boundaries.

Latest `FMT-TIFF-003R` verification: the cached Pillow 11.3.0 source and
local oracle fix Compression `5` (`tiff_lzw`) and `8` (`tiff_adobe_deflate`)
for the 4×3, 2×2 row-major chunky tiled matrix covering storage `L`, `RGB`,
`RGBA`, `LA`, and RGBX exposed as public `RGB` with rawmode `RGBX`. The raw
RED failed with native `-3`; the facade RED raised `pillow_c: invalid
argument`. After the native per-tile LZW/zlib implementation, targeted
raw/facade tests pass `1/1` and `1/1`; TIFF filters pass `308/308` and
`306/306`; and the full directory suite passes `2664/2664` in `19438ms`,
with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `452/452` with zero
difference; and the rebuilt DLL SHA-256 is
`550C606EB56C4E5062A75EFAE5F26189521419AB54648766E34FDF5BF28D905A`.
The DLL owns tile decompression, clipping, and RGBX X-byte removal; no ABI,
facade lifetime rule, or AHK pixel loop changed. Compressed planar-separate
storage, multi-frame tiled files, tiled orientation, and BigTIFF remain
explicit boundaries.

Latest `FMT-TIFF-003Q` verification: the cached Pillow 11.3.0 source maps TIFF
Compression `32773` to `packbits`, and the local oracle fixes a 4×3, 2×2
row-major chunky tiled matrix for storage `L`, `RGB`, `RGBA`, `LA`, and RGBX
exposed as public `RGB` with rawmode `RGBX`. The raw RED failed with native
`-3`; the facade RED raised `pillow_c: invalid argument`. After the native
per-tile PackBits implementation, targeted raw/facade tests pass `1/1` and
`1/1`; TIFF filters pass `307/307` and `305/305`; and the full directory suite
passes `2662/2662` in `17704ms`, with zero failures, errors, or skips. Release
x64 Rebuild has `0 Warning(s), 0 Error(s)`; source/DLL export parity remains
`452/452` with zero difference; and the rebuilt DLL SHA-256 is
`F7D5D4513672C22D41B368EEA3B27FBC4E8D635BA3CF884C0E2167D31DD21BE2`.
The DLL owns tile decompression, clipping, and RGBX X-byte removal; no ABI,
facade lifetime rule, or AHK pixel loop changed. LZW/Adobe Deflate tiled
compression, planar-separate storage, multi-frame tiled files, tiled
orientation, and BigTIFF remain explicit boundaries.

Latest `FMT-TIFF-003P` verification: the cached Pillow 11.3.0 source and
local fixture establish native open parity for a 4×3 uncompressed chunky tiled
RGBX-storage TIFF with 2×2 row-major tiles, `Photometric=2`,
`BitsPerSample=[8,8,8,8]`, `SamplesPerPixel=4`, `ExtraSamples=0`, and clipped
right/bottom edges. Pillow exposes it as public mode `RGB` with rawmode `RGBX`;
the DLL allocates three-channel RGB output and removes each X byte from the
four-byte tile input stride. The existing native TIFF frame-count route also
recognizes the single-frame fixture, so `Pillow.Image.Open` completes without
an AHK pixel loop. Raw and facade targeted tests pass `1/1` each; raw/facade
TIFF filters pass `306/306` and `304/304`; raw/facade files pass `1318/1318`
and `1342/1342`; and the full directory suite passes `2660/2660` in `18860ms`,
with zero failures, errors, or skips. Release x64 has `0 Warning(s), 0 Error(s)`;
source/DLL export parity is `452/452` with zero difference; and the current
DLL SHA-256 is
`35D7DF93688837456DBDAF1CE21A1BC410D37B60C48AA7A78A06F917017A2CF6`.
Compressed/planar tiled storage, multipage tiled storage, tiled orientation,
and BigTIFF remain explicit boundaries.

Latest `FMT-TIFF-003O` verification: the cached Pillow 11.3.0 source and local
fixture establish native open parity for a 4×3 uncompressed chunky tiled
mode-`LA` TIFF with 2×2 row-major tiles, inline `BitsPerSample=[8,8]`,
`ExtraSamples=2`, and clipped right/bottom edges. The existing native TIFF
frame-count route also recognizes the single-frame tiled fixture, so
`Pillow.Image.Open` completes its facade initialization without an AHK pixel
loop. Raw and facade targeted tests pass `1/1` each; raw/facade TIFF filters
pass `305/305` and `303/303`; raw/facade files pass `1317/1317` and
`1341/1341`; and the full directory suite passes `2658/2658` in `18797ms`,
with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `452/452` with zero
difference; and the rebuilt DLL SHA-256 is
`F16E5063EA263486676BF57ED09227213D64E55EE80597CFB6A5EAB66F8D6D1C`.
RGBX tiled TIFF, compressed/planar tiled storage, multipage tiled storage,
tiled orientation, and BigTIFF remain explicit boundaries.

Latest `FMT-JPEG-002B2T-KEEP-RGB-QTABLES-EXTRA` verification: the cached Pillow
11.3.0 source confirms that RGB `keep_rgb=True` output uses Adobe APP14
transform `0`; with one or two qtables, `extra` remains a raw marker stream
placed after APP14 and before DQT. The additive
`pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_extra_encode_options`
export reuses the DLL-owned RGB qtables/keep-rgb encoder and composes explicit
comment/ICC/EXIF/XMP metadata plus `extra` in one native route. Raw and facade
files pass `1313/1313` and `1337/1337`; the full directory suite passes
`2650/2650` in `17953ms` with zero failures, errors, or skips. Release x64 has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `452/452` with zero
difference; and the current DLL SHA-256 is
`27937210D81D2DEE52E879CFF4EA6BC68C42E8B41953C51F4CF7E8A19E7F8011`.
Keep-rgb plus extra plus restart markers, three/four-table qtables, malformed
marker streams, broader option precedence, and exact whole-file or entropy
parity remain explicit boundaries.

Previous `FMT-JPEG-002B2T-RESTART-EXTRA` verification: the saved Pillow 11.3.0
source archive was used to pin the raw `extra` marker-stream contract while
the native route composed bounded block/row restart markers with explicit
metadata and one/two custom qtables for `L`, `RGB`, and `CMYK`. The additive
`pillow_c_image_save_jpeg_metadata_restart_marker_extra_encode_options` export
keeps marker insertion and restart entropy inside the DLL; the facade keeps
all normalized Buffers alive through `DllCall` and reports unsupported keep or
preset combinations explicitly. Raw and facade files pass `1311/1311` and
`1335/1335`; the full directory suite passes `2646/2646` in `18422ms` with
zero failures, errors, or skips. Release x64 has `0 Warning(s), 0 Error(s)`;
source/DLL export parity is `450/450` with zero difference; and the current
DLL SHA-256 is
`C874D7B0A435FE70178C661039FABF3C95F4AAD2B945F968EE275B14F7A9FF12`.
Keep-rgb plus extra, three/four-table qtables, malformed marker streams,
broader option precedence, and exact whole-file/entropy parity remain open.

Latest `FMT-JPEG-002B2T-QTABLES` verification: the cached Pillow 11.3.0
source was used to confirm the one-to-four Python qtable boundary and the
bounded native one/two-table route. Raw and facade qtables-plus-extra files
pass `1309/1309` and `1333/1333`; the full directory suite passes
`2642/2642` in `18844ms` with zero failures, errors, or skips. Release x64
has `0 Warning(s), 0 Error(s)`; source/DLL export parity is `449/449` with
zero difference; and the current DLL SHA-256 is
`793E71B180B949BC7D23D2F707077698AE239503EF689BB83C1F3528503A98AF`. The
tests cover `L` optimized baseline, `RGB` progressive/optimized, `CMYK`
optimized baseline, marker ordering/payload preservation, and qtables+XMP
composition. Remaining explicit boundaries are three/four-table qtables,
keep-rgb/restart plus extra, malformed marker streams, and exact whole-file
or entropy parity.

Latest `FMT-JPEG-002B2T-META` verification extends the raw JPEG `extra`
marker-stream tests with explicit comment/ICC/EXIF/XMP metadata. Raw and
facade files pass `1307/1307` and `1331/1331`; the complete directory suite
passes `2638/2638` in `29203ms` with zero failures, errors, or skips. The
`pillow_c_image_save_jpeg_metadata_extra_encode_options` export preserves
Pillow's `EXIF -> raw extra -> XMP -> ICC -> COM` order inside the DLL, while
facade `Comment`/`comment`, `IccProfile`/`icc_profile`, `Exif`/`exif`,
`Xmp`/`xmp`, and `Extra`/`extra` buffers remain rooted through `DllCall`.
Release x64 has `0 Warning(s), 0 Error(s)`; source/DLL export parity is
`448/448` with zero difference; and the current DLL SHA-256 is
`96CF42F004149BDFBE202257EA24C063751C9CB5128E853666150BE6CF8257A8`.
QTables-plus-extra, keep-rgb/restart-plus-extra, other modes, malformed
marker streams, and exact whole-file/entropy parity remain explicit deferred
boundaries.

Latest `QUANT-001` verification: the saved Pillow 11.3.0 source archive was
used to implement the bounded native quantize options route. Raw and facade
algorithm-option tests pass `2/2`; the full directory suite passes `2630/2630`
in `29906ms` with zero failures, errors, or skips. Release x64 Rebuild has
`0 Warning(s), 0 Error(s)`; source/DLL export parity is `446/446` with zero
difference; and the current DLL SHA-256 is
`FE00E99AE5524552D45AC49824C9EA413AA63A88BB2F8DB447F08AC38A55BF56`.
The additive ABI is `pillow_c_image_quantize_options`; reference-palette dither
parity and libimagequant remain open under `QUANT-001`. Pillow's algorithm core
does not consume the dither argument, matching the native method route.

Latest `ARCH-MOD-012` verification: Release x64 explicitly compiles both
`pillow_c_ops.cpp` and the new `pillow_c_ops_statistics.cpp` through the
private `pillow_c_ops_internal.h` seam and finishes with `0 Warning(s),
0 Error(s)`. The statistics ownership RED failed `0/1` because the unit did
not exist; the operations structural filter is now `13/13`. Statistics
filters pass histogram `26/26`, entropy `46/46`, extrema `9/9`, bbox `4/4`,
projection `4/4`, getcolors `6/6`, and autocontrast `13/13`. Restored
cross-cutting operations pass logical `13/13`, RGB-to-L `3/3`, point LUT
`6/6`, put-alpha `2/2`, and convert-mode `9/9`. The complete directory suite
passes `2628/2628` in `28375ms` with zero failures, errors, or skips.
Alias-aware source/DLL export parity is `445/445` with zero difference; the
rebuilt DLL SHA-256 is
`F98BC86BF9181F0473AD2C7320D9FFC16157E97220C7380DDE49D86F295313E2`.
This architecture packet does not change the `59% ±4%` compatibility
estimate.

Latest `ARCH-MOD-011` verification: Release x64 explicitly compiles the seven
JPEG Modules `pillow_c_codec_jpeg_decode.cpp`,
`pillow_c_codec_jpeg_common.cpp`, `pillow_c_codec_jpeg_encode_l.cpp`,
`pillow_c_codec_jpeg_encode_rgb.cpp`, `pillow_c_codec_jpeg_encode_cmyk.cpp`,
`pillow_c_codec_jpeg_save.cpp`, and `pillow_c_codec_jpeg_metadata.cpp`; the
build finishes with `0 Warning(s), 0 Error(s)`. The structural ownership test
passes `1/1`; raw JPEG passes `212/212`; facade JPEG passes `219/219`; and the
full directory suite passes `2627/2627` in `34375ms` with zero failures,
errors, or skips. Source/DLL export parity is `445/445` with zero difference;
the rebuilt DLL SHA-256 is
`988DAA0F12507201F4AF8B01C889703FAD69614839868A71E3C6DB9ABD670462`.
`pillow_c.cpp` is a 31-byte include-only translation unit, and the JPEG
implementation has no duplicate body or forwarding export shell. This
architecture packet does not change the `59% ±4%` compatibility estimate.

Latest `ARCH-MOD-010` verification: Release x64 explicitly compiles
`pillow_c_core.cpp` and `pillow_c_effects.cpp` and finishes with `0 Warning(s),
0 Error(s)`. The structural ownership test passes `1/1` after a `0/1` RED;
raw linear/radial/effects pass `1/1`, `1/1`, and `3/3`; facade linear/radial/
effects pass `1/1`, `1/1`, and `4/4`. The full directory suite passes
`2626/2626` in `28843ms` with zero failures, errors, or skips. Source/DLL
export parity is `445/445` with zero difference; the rebuilt DLL SHA-256 is
`69FF7A140E8EA0E3AA5E1B75BF394D1ADA8C9B5271709242A89497C1D23DF484`.
`pillow_c.cpp` is now a one-line include-only translation unit; core and effects
Modules contain the remaining shared seam and generator implementations. This
architecture packet does not change the `59% ±4%` compatibility estimate.

Latest `ARCH-MOD-009` verification: Release x64 explicitly compiles
`pillow_c_raw.cpp` and finishes with `0 Warning(s), 0 Error(s)`. The structural
ownership test passes `1/1` after a `0/1` RED; raw bytes passes `3/3`, raw
numeric/Mode-1 passes `8/8`, raw FromBuffer map/alias passes `4/4`, raw
readonly refresh/detach passes `41/41`, facade `Image.FromBuffer` passes
`32/32`, facade `Image.FromBytes` passes `5/5`, and facade `Image.ToBytes`
passes `1/1`. The full directory suite passes `2625/2625` in `28422ms` with
zero failures, errors, or skips. Source/DLL export parity is `445/445` with
zero difference; the rebuilt DLL SHA-256 is
`0756B2D899800BA9E0A81B95C220E9E712F694F44C99280B2B88F2BFCF49A269`.
The raw Module contains the implementation and public exports; the main unit
retains neither raw copies nor forwarding export shells. This architecture
packet does not change the `59% ±4%` compatibility estimate.

Latest `ARCH-MOD-008` verification: Release x64 explicitly compiles the
metadata unit and finishes with `0 Warning(s), 0 Error(s)`. The structural
ownership test passes `1/1`; raw EXIF passes `23/23`; raw PNG metadata passes
`12/12`; the combined raw metadata filter passes `182/182`; facade PNG
metadata passes `12/12`; and facade JPEG metadata/open passes `25/25`. The
full directory suite passes `2624/2624` in `28796ms` with zero failures,
errors, or skips. Source/DLL export parity is `445/445` with zero difference;
the rebuilt DLL SHA-256 is
`BF103E627C2DAD72C8781AD42A289818F1074F4FC53D915555F4CBC72BEBE31D`.
The metadata module contains the implementation and exports; the main unit
retains neither metadata copies nor forwarding export shells. This
architecture packet does not change the `59% ±4%` compatibility estimate.

Latest `ARCH-MOD-007` verification: Release x64 explicitly compiles
`pillow_c_ops.cpp` and finishes with `0 Warning(s), 0 Error(s)`. The operations
structural ownership test passes `1/1`; raw targeted operations pass fill
`2/2`, get/putpixel `2/2`, paste `13/13`, transpose `12/12`, point LUT `6/6`,
and handle `3/3`; facade targeted fill passes `1/1` and get/putpixel passes
`2/2`. The raw/facade file suites remain green inside the full run, which
passes `2623/2623` in `29453ms`, with zero failures, errors, or skips.
Alias-aware source/DLL export parity is `445/445` with zero difference; the
rebuilt DLL SHA-256 is
`50C0FCB6CCABBA75098C5CB90732F540624EBD4BD562F24EE852E18D4E900EBE`.
The operations module contains the implementation and public exports; the
main unit retains neither operation copies nor forwarding export shells. This
architecture packet does not change the `59% ±4%` compatibility estimate.

Latest `ARCH-MOD-006` verification: Release x64 explicitly compiles
`pillow_c_codec_gif.cpp` and finishes with `0 Warning(s), 0 Error(s)`. The
structural ownership test is RED `0/1` before extraction and GREEN `1/1`
afterward. Raw GIF passes `52/52`, facade GIF `51/51`, raw full-file
`1296/1296` in `11735ms`, facade full-file `1326/1326` in `17235ms`, and the
full directory suite `2622/2622` in `28485ms`, with zero failures, errors, or
skips. Alias-aware source/DLL export parity is `445/445` with zero difference;
the rebuilt DLL SHA-256 is
`9B84DFA634A14EAEC4ABD3607446C05DFC8D878BB8EE8B1BD46880695FB566FD`.
The GIF module contains the implementation and exports; the main unit retains
neither copies nor forwarding export shells. This architecture packet does not
change the `59% ±4%` compatibility estimate.

Latest `ARCH-MOD-004` and `ARCH-MOD-005` verification: Release x64 explicitly
compiles `pillow_c_draw.cpp` and `pillow_c_codec_legacy.cpp` with the existing
native units and finishes with `0 Warning(s), 0 Error(s)`. The draw/font
structural test passes `1/1`; raw draw/default-font tests pass `35/35` and
`6/6`; facade ImageDraw/ImageFont tests pass `57/57` and `4/4`. The legacy
codec structural test passes `1/1`; its raw/facade filters pass BMP `8/8`, PPM
`11/11`, QOI `5/5`, TGA `11/11`, XBM `9/9`, and ICO `24/24`. The raw file run
passes `1295/1295` in `11547ms`, the facade run passes `1326/1326` in
`17204ms`, and the full directory suite passes `2621/2621` in `27734ms` with
zero failures, errors, or skips. Source/DLL export parity is `445/445` with
zero difference; the rebuilt DLL SHA-256 is
`5FE477FD9D8F45473010D908D87B6903D9A2C931807AD4578A7F818454D364AF`.
The legacy module uses only explicit internal seams and contains no AHK-side
per-pixel loop; these architecture packets do not change the `59% ±4%`
compatibility estimate.

Latest `ARCH-MOD-003` verification: Release x64 explicitly compiles
`pillow_c_codec_png.cpp` alongside the core, memory, metadata, WIC, TIFF,
JPEG, and ABI units, with `0 Warning(s), 0 Error(s)`. The PNG filter passes
`221/221` across raw DLL and facade tests; the full AHK directory suite passes
`2616/2616` in `18016ms` with zero failures, errors, or skips. The structural
ownership test is red before extraction and green after it: PNG implementation
and PNG exports are present in `pillow_c_codec_png.cpp`, absent from the main
unit, and the project lists the new source. Source/DLL export parity is
`445/445` with zero difference; the rebuilt DLL SHA-256 is
`6236EE06518E445F2830D81D4F4D8C4F2F148FDACE05A1A3ECF8B1BCFFF1BCEB`.

Historical `ARCH-MOD-002` verification: Release x64 explicitly compiled the
initial single JPEG translation unit alongside the core, memory, metadata,
WIC, TIFF, and ABI units, with `0 Warning(s), 0 Error(s)`. The seven-module
JPEG ownership and current verification are recorded under `ARCH-MOD-011`
above.

Latest `ARCH-MOD-001` plus `FMT-TIFF-001AZ` verification: the Release x64
build explicitly compiles `pillow_c_core.cpp`, `pillow_c_memory.cpp`,
`pillow_c_metadata.cpp`, `pillow_c_codec_wic.cpp`, `pillow_c_codec_tiff.cpp`,
and `pillow_c_abi.cpp`, with `0 Warning(s), 0 Error(s)`. Source/DLL export
parity is `445/445` with zero difference. The three-frame uncompressed
`I;16B` DPI/ICC/XMP composition targeted tests pass `3/3` raw and `3/3`
facade; TIFF filters pass `301/301` raw and `299/299` facade; and the full AHK
directory suite passes `2615/2615` in `19750ms` with zero failures, errors, or
skips. The rebuilt DLL SHA-256 is
`33DC056E15794A5E451BD430BAAE9696AC8FE0960B793A8859C8A233699E2AA`.

Latest `FMT-TIFF-001AY`: two homogeneous 2x1 uncompressed `I;16B` frames
compose a 672-byte big-endian TIFF with per-IFD DPI `(300,150)`, type-7 ICC,
type-1 XMP, aligned repeated IFD blocks, and exact raw strips. Raw/facade
targeted tests pass `1/1` each; TIFF filters pass `300/300` and `298/298`;
the full suite passes `2613/2613` in `17500ms`. Release x64 built with zero
warnings/errors; source/DLL exports are `445/445`, zero difference; DLL
SHA-256 is
`4870243958A0CB738CD123BB23472CABABD0B554ED6FCEA83106FA33E046B869`.

Latest `FMT-JPEG-003BQ`: the stable RGB 4:2:2 source selects decoder scale 8
for `draft("L", (6,4))`. Pillow 11.3.0 returns
`("L", (0,0,6.0,4.0))`, produces pixel SHA-256
`4F1BFE45ACDE072617748D7095B446061302477DBBC41DA2173C445D4FE0C137`,
returns `None` on the second call, and emits no warnings. Draft-RGB followed
by `convert("L")` has the same hash at this scale. Raw/facade tests were
first-run GREEN `1/1` (`31ms`) / `1/1` (`16ms`); draft filters pass `13/13`
(`62ms`) / `14/14` (`79ms`); raw `open_jpeg` passes `45/45` (`218ms`); and
full passes `2611/2611` (`17922ms`). No production source, ABI, or DLL
artifact changed; registrations are `1287/1324`; exports remain `445/445`,
zero difference; DLL SHA-256 remains
`F8BCA8B5A57D28241FA996DF686253DF3655F68FA97609D6577E1C15936EC92A`.

Latest `FMT-JPEG-003BP`: the stable RGB 4:2:2 source selects decoder scale 4
for `draft("L", (12,8))`. Pillow 11.3.0 returns
`("L", (0,0,12.0,8.0))`, produces pixel SHA-256
`E7FBFB3D41429AEE13906FEC3D4F9FDA3F370B7E953FE655D1B6D6FC835748A4`,
returns `None` on the second call, and emits no warnings. Draft-RGB followed
by `convert("L")` has the same hash at this scale. Raw/facade tests were
first-run GREEN `1/1` (`31ms`) / `1/1` (`31ms`); draft filters pass `12/12`
(`62ms`) / `13/13` (`109ms`); raw `open_jpeg` passes `44/44` (`219ms`); and
full passes `2609/2609` (`17766ms`). No production source, ABI, or DLL
artifact changed; registrations are `1286/1323`; exports remain `445/445`,
zero difference; DLL SHA-256 remains
`F8BCA8B5A57D28241FA996DF686253DF3655F68FA97609D6577E1C15936EC92A`.

Latest `FMT-JPEG-003BO`: the repeatable native-generated RGB 4:2:0 source
selects decoder scale 1 for `draft("YCbCr", (48,32))`. Pillow 11.3.0 returns
`("YCbCr", (0,0,48.0,32.0))`, produces pixel SHA-256
`BED3C13D33544DF3EF1DF583CCA583CED5AA3BA3BD511E80557F34AB273E610D`,
returns `None` on the second call, and emits no warnings. Draft-RGB followed
by `convert("YCbCr")` instead hashes to
`7BC97380D6B254CA8526E4A5937A81719822B7209515F479D1E7515FE859CC00`.
Raw/facade RED returned `-3` / an error. WIC supplies
`48x32 / 24x16 / 24x16` Y/Cb/Cr planes; the final native route applies exact
h2v2 fancy reconstruction inside the DLL. Raw/facade GREEN pass `1/1`
(`110ms`) / `1/1` (`31ms`); draft filters pass `11/11` (`78ms`) / `12/12`
(`78ms`); raw `open_jpeg` passes `43/43` (`234ms`); and full passes
`2607/2607` (`18937ms`). Release x64 builds with zero warnings/errors;
registrations are `1285/1322`; exports remain `445/445`, zero difference;
DLL SHA-256 is
`F8BCA8B5A57D28241FA996DF686253DF3655F68FA97609D6577E1C15936EC92A`.

Latest `FMT-JPEG-003BN`: the stable RGB 4:2:2 source selects decoder scale 1
for `draft("YCbCr", (48,32))`. Pillow 11.3.0 returns
`("YCbCr", (0,0,48.0,32.0))`, produces pixel SHA-256
`C17E5465A1FD9F0418A620F09FCA0EBDD3862B82A71BA5C36B5005AE2524121A`,
returns `None` on the second call, and emits no warnings. Draft-RGB followed
by `convert("YCbCr")` instead hashes to
`53DDBF576E8A116F7AA0E85B96E72D9022092BB6177D2762E3E6D7B6478EC9BB`.
Raw/facade tests were first-run GREEN `1/1` (`47ms`) / `1/1` (`31ms`);
draft filters pass `10/10` (`47ms`) / `11/11` (`78ms`); raw `open_jpeg`
passes `42/42` (`219ms`); and full passes `2605/2605` (`18375ms`). No
production source, ABI, or DLL artifact changed; registrations are
`1284/1321`; exports remain `445/445`, zero difference; DLL SHA-256 remains
`E9C69DDDC99210311F4B543C20E7B0ABA801FE88A0C845AA1402446A3BCBE43C`.

Latest `FMT-JPEG-003BM`: the stable RGB 4:2:2 source selects decoder scale 8
for `draft("YCbCr", (6,4))`. Pillow 11.3.0 returns
`("YCbCr", (0,0,6.0,4.0))`, produces pixel SHA-256
`53089E589F2DE2C2A4D538388B608CAEAAA3602ADB75843A2FF6C09EB2D2BCDC`,
returns `None` on the second call, and emits no warnings. Draft-RGB followed
by `convert("YCbCr")` instead hashes to
`EC837EEC3E2EC91F4E77EF0B49473BF3EA48A3A83F4D6C01D99947E837C2D838`.
Raw RED exposed the old fancy-upsample hash at `0/1` (`31ms`). WIC planes are
`6x4 / 3x4 / 3x4`; the final native route uses nearest horizontal chroma
replication only at scale 8. Raw/facade GREEN pass `1/1` (`93ms`) / `1/1`
(`31ms`); draft filters pass `9/9` (`47ms`) / `10/10` (`63ms`); raw
`open_jpeg` passes `40/40` (`250ms`); and full passes `2603/2603`
(`17687ms`). Release x64 builds with zero warnings/errors; registrations are
`1283/1320`; exports remain `445/445`, zero difference; DLL SHA-256 is
`E9C69DDDC99210311F4B543C20E7B0ABA801FE88A0C845AA1402446A3BCBE43C`.

Latest `FMT-JPEG-003BL`: the stable RGB 4:2:2 source selects decoder scale 4
for `draft("YCbCr", (12,8))`. Pillow 11.3.0 returns
`("YCbCr", (0,0,12.0,8.0))`, produces pixel SHA-256
`111820C0CB10B4EAC6F7E3D80660FC24E2FE6782AF9C1FD2C6ADD716648589E9`,
returns `None` on the second call, and emits no warnings. Raw/facade tests
pass `1/1` (`31ms`) / `1/1` (`31ms`); raw/facade draft filters pass `8/8`
(`46ms`) / `9/9` (`94ms`); raw `open_jpeg` passes `39/39` (`250ms`); and
full passes `2601/2601` (`17391ms`). No native source or ABI change was
required; registrations are `1282/1319`; exports remain `445/445`, zero
difference; DLL SHA-256 remains
`6CD7A23D500A6C8D37AC3600CC2769186A9CA42F360A7CB305D021FBD56F6FA1`.

Latest `FMT-JPEG-003BK`: the repeatable native-generated RGB 4:2:0 source
selects decoder scale 8 for `draft("YCbCr", (6,4))`. Pillow 11.3.0 returns
`("YCbCr", (0,0,6.0,4.0))`, produces pixel SHA-256
`43DB98CCF817AFBE33D98A16A86BCAFE0D526ED282714A93A1B166B7628762EE`,
returns `None` on the second call, and emits no warnings. Draft-RGB followed
by `convert("YCbCr")` instead hashes to
`18216EDA1CE6F7FB1E8D2476201CA09AE2DBACA580EC425515B67779BEB08D79`.
Raw/facade tests were first-run GREEN `1/1` (`47ms`) / `1/1` (`31ms`);
raw/facade draft filters pass `7/7` (`63ms`) / `8/8` (`62ms`); raw
`open_jpeg` passes `39/39` (`281ms`); and full passes `2599/2599`
(`18172ms`). No native source or ABI change was required; registrations are
`1281/1318`; exports remain `445/445`, zero difference; DLL SHA-256 remains
`6CD7A23D500A6C8D37AC3600CC2769186A9CA42F360A7CB305D021FBD56F6FA1`.

Latest `FMT-JPEG-003BJ`: the repeatable native-generated RGB 4:2:0 source
selects decoder scale 4 for `draft("YCbCr", (12,8))`. Pillow 11.3.0 returns
`("YCbCr", (0,0,12.0,8.0))`, produces pixel SHA-256
`EDDCCC5E431031AD03A61E767CDF474710B14642FE31A25C6D136F09C1766062`,
returns `None` on the second call, and emits no warnings. Draft-RGB followed
by `convert("YCbCr")` instead hashes to
`B1DA65ADC59F947018E112CB363507C0D9F9A0E61609055C6A687A86B4E7BEEB`.
Raw/facade tests were first-run GREEN `1/1` (`47ms`) / `1/1` (`31ms`);
raw/facade draft filters pass `6/6` (`62ms`) / `7/7` (`47ms`); raw
`open_jpeg` passes `38/38` (`219ms`); and full passes `2597/2597`
(`18047ms`). No native source or ABI change was required; registrations are
`1280/1317`; exports remain `445/445`, zero difference; DLL SHA-256 remains
`6CD7A23D500A6C8D37AC3600CC2769186A9CA42F360A7CB305D021FBD56F6FA1`.

Latest `FMT-JPEG-003BI`: the test creates a repeatable native-generated
1431-byte RGB 4:2:0 JPEG with file SHA-256
`F9564EAECD07E15111F7D6539FAC64B99DE2D7A77F84715C5370E83B9C5D8819`.
Pillow 11.3.0 returns `("YCbCr", (0,0,24.0,16.0))`, produces pixel SHA-256
`5EF558D11947288CFC1D16F2732A124B4F8A01F0C9302A3719D643F99A39FC63`,
returns `None` on the second call, and emits no warnings. Draft-RGB followed
by `convert("YCbCr")` instead hashes to
`D362A1129F46531746FD789DAFC83501C6384645AACD9D69FFA6008CD2E77650`.
WIC reports full `24x16` Y/Cb/Cr planes; the DLL directly interleaves them.
Raw/facade RED returned `-3` / invalid argument, then GREEN passed `1/1`
(`94ms`) / `1/1` (`31ms`); raw/facade draft filters pass `5/5` (`31ms`) /
`6/6` (`47ms`); raw `open_jpeg` passes `37/37` (`188ms`); and full passes
`2595/2595` (`18188ms`). Release x64 builds with zero warnings/errors;
registrations are `1279/1316`; source/DLL exports remain `445/445`, zero
difference; DLL SHA-256 is
`6CD7A23D500A6C8D37AC3600CC2769186A9CA42F360A7CB305D021FBD56F6FA1`.

Latest `FMT-JPEG-003BH`: local Pillow 11.3.0 returns
`("YCbCr", (0,0,24.0,16.0))` for RGB-JPEG `draft("YCbCr", (24,16))`,
produces pixel SHA-256
`47A50DA85103D91CF0B19FB13FA64C0F2FDA80EBA1986CA09CD6E6ACF86ECC13`,
returns `None` on the second call, and emits no warnings. Draft RGB followed
by `convert("YCbCr")` instead hashes to
`6F05C698DE44C8B5E822E620403ADB33448B2CD5D60BD7C7AB44A6C42434A600`.
Raw/facade RED returned `-3` / no tuple (`0/1`, `31ms`). WIC reports Y
`24x16` and Cb/Cr `12x16`; the final DLL route applies exact libjpeg-turbo
3.1.1 h2v1 fancy upsampling and native interleaving. GREEN passes `1/1`
(`157ms`) / `1/1` (`47ms`); raw/facade draft filters pass `4/4` (`47ms`) /
`5/5` (`31ms`); raw `open_jpeg` passes `36/36` (`187ms`); and full passes
`2593/2593` (`18750ms`). Release x64 builds with zero warnings/errors;
registrations are `1278/1315`; source/DLL exports remain `445/445`, zero
difference; DLL SHA-256 is
`75172FA128F0A0CCBA892014C2444CB7BFA517B3CC06B9DA9F5B0DD52B5C58B8`.

Latest `FMT-JPEG-003BG`: local Pillow 11.3.0 returns
`("L", (0,0,24.0,16.0))` for RGB-JPEG `draft("L", (24,16))`, produces pixel
SHA-256 `AD0FDBB2BD3421A5F8F9192DBFCD783DD06B95B9357EABDDE503A210191943B8`,
returns `None` on the second call, and emits no warnings. Reduced RGB followed
by `convert("L")` hashes differently. Raw RED lacked
`pillow_c_image_open_jpeg_draft_mode`; facade RED returned no tuple, both
`0/1` (`32ms`). The final native route requests complete reduced WIC planar
Y/Cb/Cr output and retains Y without a conversion fallback. GREEN passes
`1/1` (`93ms`) / `1/1` (`31ms`); raw/facade draft filters pass `3/3`
(`32ms`) / `4/4` (`31ms`); raw `open_jpeg` passes `35/35` (`296ms`); and
full passes `2591/2591` (`18016ms`). Release x64 builds with zero warnings/
errors; registrations are `1277/1314`; source/DLL exports are `445/445`, zero
difference; DLL SHA-256 is
`C8EEEFE67A4EDCB7484F24064BB5986C70A0270FCD6AC8E6E2D42E84DB922EDD`.

Latest `FMT-JPEG-003BF`: local Pillow 11.3.0 returns
`("RGB", (0,0,24.0,16.0))` for `draft("RGB", (24,16))` on the stable
1806-byte `48x32` 4:2:2 fixture, produces pixel SHA-256
`5FBBC621CFC49F97902D811AA2E78F69E2D48034ACB7A93AED420E6EFA024192`,
returns `None` on the second call, and emits no warnings. Fixture SHA-256 is
`BC199D543E5867C8CC948824F3EF89773A57B9CEDF159F557D2C289151DFE486`.
Raw RED returned `-3`; facade RED returned no tuple, both `0/1` (`47ms`).
After WIC reduced-BGR plus the native contiguous B/R swap and same-mode facade
routing, GREEN passes `1/1` (`125ms`) / `1/1` (`47ms`); raw/facade draft
filters pass `2/2` (`47ms`) / `3/3` (`62ms`); raw `open_jpeg` passes `34/34`
(`297ms`); and full passes `2589/2589` (`29328ms`). Release x64 builds with
zero warnings/errors; registrations are `1276/1313`; source/DLL exports remain
`444/444`, zero difference; DLL SHA-256 is
`A837676696AF28A1FB5FF500AA0BEC5532629DB997CC4B8322A3A669063B1155`.
There is no full-size resize, fallback, or AHK pixel loop.

Latest `FMT-JPEG-003BE`: local Pillow 11.3.0 returns
`("CMYK", (0,0,8.5,5.5))` for `draft("CMYK", (8,5))` on the odd `17x11`
alternate real YCCK fixture, mutates the image to CMYK `9x6`, produces pixel
SHA-256 `4FF2199B3E6C02946B2C64FBB75480161A55DB07548663DF7BC41074F7A4E60E`,
returns `None` on a second call, and emits no warnings. Raw RED lacked
`pillow_c_image_open_jpeg_draft`; facade RED received no tuple. After the
decoder-native WIC reduced-decode route and facade handle/lifetime routing,
final raw/facade draft filters pass `1/1` (`47ms`) / `1/1` (`63ms`), raw/
facade YCCK filters pass `35/35` (`1078ms`) / `36/36` (`1437ms`), raw
`open_jpeg` passes `33/33` (`312ms`), and full passes `2587/2587`
(`28328ms`). Release x64 builds with zero warnings/errors; registrations are
`1275/1312`; source/DLL exports are `444/444`, zero difference; DLL SHA-256
is `901CA0B96ECA45305BCF43FD81A4F348E08FAB7E98B76AD7366402EFE4534235`.
No AHK pixel loop or fallback is used.

Latest `FMT-JPEG-003BD`: the project-owned ImageMagick/libjpeg-turbo fixture
`imagemagick_ycck_h2v2_17x11.jpg` is 397 bytes with SHA-256
`42F5019F3F9C6B5EFA153E24160206573141A9641402ABCA39E89D22C3EECCFC`,
APP14 transform `2`, odd size `17x11`, and component sampling
`2x2/1x1/1x1/2x2`. Pillow 11.3.0 opens CMYK with Adobe `100`, transform `2`,
and source-pixel SHA-256
`9849846710971F610072C1AC91430CFF74391B2278DD4E95CC54E530148EF620`.
Quality/qtables keep each produce the same 563-byte transform-0, all-`1x1`
CMYK file, preserving both DQT tables, with SHA-256
`70AB11ABC399316E0C824F50EEC1BBEFD59135D3B3D8ABF2970C634E31957306`.
Raw YCCK filters pass `34/34` (`1078ms`), facade YCCK filters pass `35/35`
(`1438ms`), and full passes `2585/2585` (`28891ms`). Registrations are
`1274/1311`. No production source changed and no rebuild was required;
source/DLL exports remain `443/443`, zero difference, and SHA-256 remains
`6E871350292B5B9D07B2336F4E0BDEB205B2B3653DC81515B4C0E5C24CD4F831`.

Latest `FMT-JPEG-003BC`: Pillow 11.3.0 uses the second structured
ResolutionInfo tuple `(72.25,4,96.5,5)` when its data length is 14, but ignores
a 13-byte duplicate and retains `(300.5,1,150.25,3)` from the valid first
resource. Both 68-byte recognized APP13 payloads open RGB `16x8` with no
warnings. Batched raw/facade tests were already GREEN (`1/1`, `47ms` / `1/1`,
`62ms`). Raw/facade Photoshop filters pass `10/10` (`125ms`) / `10/10`
(`203ms`); full passes `2583/2583` (`28719ms`). Registrations are `1273/1310`.
No production source changed after BB and no rebuild was required;
source/DLL exports remain `443/443`, zero difference; SHA-256 remains
`6E871350292B5B9D07B2336F4E0BDEB205B2B3653DC81515B4C0E5C24CD4F831`.

Latest `FMT-JPEG-003BB`: Pillow 11.3.0 reads the available fields from a
15-byte duplicate structured ResolutionInfo after one valid resource in one
70-byte recognized APP13 payload, exposes `(72.25,4,96.5,5)`, emits no
warnings, and opens RGB `16x8`. Raw RED retained first XResolution `300.5`
instead of `72.25` (`0/1`, `47ms`). Lowering the native read threshold from 16
to the actual 14 bytes required produced raw/facade GREEN `1/1` (`141ms`) /
`1/1` (`47ms`). Raw/facade Photoshop filters pass `9/9` (`125ms`) / `9/9`
(`203ms`); full passes `2581/2581` (`28485ms`). Registrations are `1272/1309`.
Release x64 builds with zero warnings/errors; source/DLL exports remain
`443/443`, zero difference; SHA-256 is
`6E871350292B5B9D07B2336F4E0BDEB205B2B3653DC81515B4C0E5C24CD4F831`.

Latest `FMT-JPEG-003BA`: Pillow 11.3.0 exposes only the second structured
ResolutionInfo tuple `(72.25,4,96.5,5)` from two `0x03ED` resources in one
exact 70-byte recognized Photoshop APP13 payload and emits no warnings. The
new raw and facade tests were already GREEN on first run (`1/1`, `62ms` /
`1/1`, `47ms`), proving the existing native scalar overwrite state and public
`Info["photoshop"][1005]` nested Map. Raw/facade Photoshop filters pass `8/8`
(`109ms`) / `8/8` (`188ms`); full passes `2579/2579` (`28625ms`).
Registrations are `1271/1308`. No production or native source changed, so no
rebuild was required; source/DLL exports remain `443/443`, zero difference,
and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002DG`: local Pillow 11.3.0 registers XMLPacket/XMP tag 700 as
BYTE/type-1 and emits an exact 38-byte EXIF blob with count 5 at offset 26.
JPEG/PNG reopen exact EXIF bytes, PNG pixels remain exact, codec-level
`Info["xmp"]` stays absent, `getxmp()` stays empty, and no warnings are
emitted. Raw serializer proof passed `1/1` (`47ms`). Facade RED rejected 700
(`0/1`, `47ms`); facade GREEN passed `1/1` (`63ms`); XMP regressions passed
`36/36` (`1187ms`); and full passed `2577/2577` (`28735ms`). Registrations are
`1270/1307`; BYTE read/write allowlists are `35/35`, zero difference, and all
current TIFF binary-read tags have a bounded write route. No native source
changed, so no rebuild was required; source/DLL exports remain `443/443`, zero
difference, and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002DF`: local Pillow 11.3.0 leaves tags 34856, 37121, 37500, and
41484 unregistered and serializes Buffer values as BYTE/type-1 with counts
`6/4/5/5`, inline 37121, and offsets `62/68/74` in one exact 86-byte EXIF
blob. JPEG/PNG reopen every value, PNG pixels remain exact, and no warnings
are emitted; TIFF open remains UNDEFINED/type-7. Raw serializer proof passed
`1/1` (`31ms`). Facade RED rejected 34856 (`0/1`, `32ms`); facade GREEN
passed `1/1` (`94ms`); `getexif` regressions passed `222/222` (`3578ms`);
and full passed `2575/2575` (`28766ms`). Registrations are `1269/1306`; BYTE
read/write allowlists are `34/34`, zero difference. No native source changed,
so no rebuild was required; source/DLL exports remain `443/443`, zero
difference, and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002DE`: local Pillow 11.3.0 registers tags 347, 33723, 34675,
and 37724 as UNDEFINED/type-7 and serializes values with counts `5/6/5/5` at
offsets `62/68/74/80` in one exact 92-byte EXIF blob. JPEG/PNG reopen every
value, PNG pixels remain exact, and no warnings are emitted. Raw generic
serializer proof passed `1/1` (`47ms`). Facade RED rejected 347 (`0/1`,
`31ms`); facade GREEN passed `1/1` (`62ms`); `getexif` regressions passed
`222/222` (`3469ms`); and full passed `2573/2573` (`29109ms`). Registrations
are `1268/1305`; all four tags occur in both bounded UNDEFINED read/write
routes, and BYTE read/write allowlists remain `30/30`, zero difference. No
native source changed, so no rebuild was required; source/DLL exports remain
`443/443`, zero difference, and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002DD`: local Pillow 11.3.0 serializes registered tags 40960 and
41730 as UNDEFINED/type-7 with counts 4/4, while 41728, 41729, and 41995 use
BYTE/type-1 with counts 1/1/5. The combined result is an exact 86-byte EXIF
blob with tag 41995 at offset 74 and one final alignment byte. JPEG/PNG reopen
every value, PNG pixels remain exact, and no warnings are emitted. Raw exact
serializer proof passed `1/1` (`31ms`). Facade RED rejected 40960 (`0/1`,
`31ms`); facade GREEN passed `1/1` (`78ms`); the TIFF regression passed
`297/297` (`4797ms`); and full passed `2571/2571` (`27969ms`). Registrations
are `1267/1304`; BYTE read/write allowlists are `30/30`, zero difference. No
native source changed, so no rebuild was required; source/DLL exports remain
`443/443`, zero difference, and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002DC`: local Pillow 11.3.0 leaves DNG tags 50828 and
52533/52534/52535 unregistered, serializes Buffer values as BYTE/type-1 with
counts `8/4/5/6`, stores 52533 inline and the others at offsets `62/70/76`,
includes one alignment byte after count-5, and emits an exact 88-byte EXIF
blob. JPEG/PNG reopen every value, PNG pixels remain exact, and no warnings
are emitted; TIFF open remains UNDEFINED/type-7. Raw exact serializer proof
passed `1/1` (`31ms`). Facade RED rejected 50828; facade GREEN passed `1/1`
(`62ms`), IlluminantData and OriginalRawFileData TIFF regressions passed
`1/1` (`78ms` / `63ms`), and full passed `2569/2569` (`28500ms`).
Registrations are `1266/1303`; BYTE read/write allowlists are `27/27`, zero
difference. No native source changed, so no rebuild was required; source/DLL
exports remain `443/443`, zero difference, and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002DB`: local Pillow 11.3.0 leaves DNG OpcodeList tags 51008,
51009, and 51022 unregistered, serializes their eight-byte Buffer values as
BYTE/type-1, count-8 entries at offsets `50/58/66` in one exact 80-byte EXIF
blob, reopens all three after explicit JPEG/PNG saves, preserves PNG pixels,
and emits no warnings; TIFF open remains UNDEFINED/type-7. Raw exact
serializer proof passed `1/1` (`47ms`). Facade RED rejected 51008. The first
GREEN run exposed AHK's 20-parameter case syntax limit; splitting the exact
23-tag set into `20+3` true branches fixed collection. Facade GREEN passed
`1/1` (`78ms`), OpcodeList regressions passed `4/4` (`125ms`), and full passed
`2567/2567` (`28453ms`). Registrations are `1265/1302`; BYTE read/write
allowlists are `23/23`, zero difference. No native source changed, so no
rebuild was required; source/DLL exports remain `443/443`, zero difference,
and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002DA`: local Pillow 11.3.0 registers `PhotoshopInfo` 34377 in
`TAGS`/`TAGS_V2` but leaves `TimeCodes` 51043 unregistered, serializes both
eight-byte Buffer values as BYTE/type-1, count-8 entries at offsets 38/46 in
one exact 60-byte EXIF blob, reopens both after explicit JPEG/PNG saves,
preserves PNG pixels, and emits no warnings. Raw exact serializer proof passed
`1/1` (`31ms`). Facade RED rejected 34377; facade GREEN passed `1/1` (`63ms`),
PhotoshopInfo and TimeCodes read regressions each passed `2/2` (`78ms` /
`94ms`), and full passed `2565/2565` (`28734ms`). Registrations are
`1264/1301`; bounded BYTE-array read/write allowlists are `20/20`, zero
difference. No native source changed, so no rebuild was required; source/DLL
exports remain `443/443`, zero difference, and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002CZ`: local Pillow 11.3.0 leaves XP tags 40092 through 40095
unregistered, serializes realistic UTF-16LE+NUL Buffer values as BYTE/type-1
entries with counts `10/8/16/16` and offsets `62/72/80/96` in one exact
118-byte EXIF blob, reopens every value after explicit JPEG/PNG saves,
preserves PNG pixels, and emits no warnings. Raw exact serializer proof passed
`1/1` (`31ms`). Facade RED rejected 40092; facade GREEN passed `1/1` (`62ms`),
filtered XP regressions passed `84/84` (`859ms`), and full passed `2563/2563`
(`28718ms`). Registrations are `1263/1300`. No native source changed, so no
rebuild was required; source/DLL exports remain `443/443`, zero difference,
and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002CY`: local Pillow 11.3.0 leaves DNG embedded-profile tags
50831 and 50833 unregistered, serializes their eight-byte Buffer values as
BYTE/type-1, count-8 entries at offsets 38/46 in one exact 60-byte EXIF blob,
reopens both values after explicit JPEG/PNG saves, preserves PNG pixels, and
emits no warnings. Raw exact serializer proof passed `1/1` (`32ms`). Facade
RED rejected 50831; facade GREEN passed `1/1` (`79ms`), TIFF profile-tag
regressions passed `2/2` (`78ms`), and full passed `2561/2561` (`29375ms`).
Registrations are `1262/1299`. No native source changed, so no rebuild was
required; source/DLL exports remain `443/443`, zero difference, and the DLL
SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002CX`: local Pillow 11.3.0 names DNG tags 50706, 50707, 50709,
and 50710 in `TiffTags.TAGS` but not `TAGS_V2`, serializes their Buffer/bytes
values inline as BYTE/type-1 entries with counts `4/4/4/3` in one exact
68-byte EXIF blob, reopens every value after explicit JPEG/PNG saves,
preserves PNG pixels, and emits no warnings. Raw exact serializer proof passed
`1/1` (`31ms`). Facade RED rejected 50706; facade GREEN passed `1/1` (`78ms`),
DNG regressions passed `37/37` (`703ms`), and full passed `2559/2559`
(`27547ms`). Registrations are `1261/1298`. No native source changed, so no
rebuild was required; source/DLL exports remain `443/443`, zero difference,
and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002CW`: local Pillow 11.3.0 leaves DNG tags 50969, 50972, 50973,
50781, and 51111 unregistered, serializes five Buffer/bytes values `01..10`
as BYTE/type-1, count-16 entries in one exact 160-byte EXIF blob, reopens every
value after explicit JPEG/PNG saves, preserves PNG pixels, and emits no
warnings. Raw exact serializer proof passed `1/1` (`31ms`). Facade RED rejected
50969 with `Pillow.Image.Exif BYTE array tag is not covered by this native
route`; facade GREEN passed `1/1` (`78ms`), digest regressions passed `5/5`
(`141ms`), facade `getexif` passed `223/223` (`3438ms`), and full passed
`2557/2557` (`26953ms`). Registrations are `1260/1297`. No native source
changed, so no rebuild was required; source/DLL exports remain `443/443`, zero
difference, and the DLL SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002CV`: local Pillow 11.3.0 serializes
`Image.Exif()[52525] = bytes(01..08)` as a 40-byte EXIF blob containing
BYTE/type-1, count-8 and value offset 26, then reopens the exact bytes after
explicit JPEG and PNG saves; PNG pixels remain exact and no warnings are
emitted. The facade RED rejected 52525 with `Pillow.Image.Exif BYTE array tag
is not covered by this native route`. The raw exact serializer proof passed
`1/1` (`32ms`) through the existing generic native serializer; facade GREEN
passed `1/1` (`78ms`), and the combined ProfileGainTableMap target passed
`6/6` (`156ms`). Raw `open_tiff` passed `230/230` (`796ms`); fresh facade
`getexif` passed `223/223` (`3344ms`); full passed `2555/2555` (`27531ms`).
Registrations are `1259/1296`. No native source changed, so no rebuild was
required; source/DLL exports remain `443/443`, zero difference, and the DLL
SHA-256 remains
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002CU`: one 136-byte strip-decoded 2x1 mode-L TIFF carries inline
IFD0 UNDEFINED/count-4 `ProfileGainTableMap` 52525 bytes `01..04`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution.
Pillow 11.3.0 leaves 52525 unnamed/unregistered but returns exact bytes through
`getexif()` and `tag_v2` without warnings. Raw RED received `[]`; facade RED
missed 52525. Raw/facade GREEN pass `1/1` (`140ms`) / `1/1` (`62ms`), the
count-4/count-8 combined target passes `4/4` (`109ms`), raw `open_tiff` passes
`230/230` (`813ms`), facade `getexif` passes `223/223` (`3375ms`), and full
passes `2553/2553` (`27797ms`). Registrations are `1258/1295`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`6A955215D77C1B38DCC5D10B751E518D7F5F42671CC3FDD67823C7ABF5993937`.

Latest `META-002CT`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
UNDEFINED/count-8 `ProfileGainTableMap` 52525 bytes `01..08`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 52525 unnamed/unregistered but returns exact bytes through
`getexif()` and `tag_v2` without warnings. Raw RED received `[]`; facade RED
missed 52525. Raw/facade GREEN pass `1/1` (`141ms`) / `1/1` (`140ms`), the
combined target passes `2/2` (`109ms`), raw `open_tiff` passes `229/229`
(`797ms`), facade `getexif` passes `222/222` (`3437ms`), and full passes
`2551/2551` (`28328ms`). Registrations are `1257/1294`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`250360EA186F9EB4A6296762B68C3E0FBEC51F45D2D7D452A60254716F40F90B`.

Latest `META-002CS`: one 136-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SHORT/count-2 `DefaultCropOrigin` 50719 inline values `(12,34)`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution
info. Pillow 11.3.0 returns an integer tuple from both `getexif()` and
`tag_v2` without warnings. Raw RED received `[]`; facade RED missed 50719.
Raw/facade GREEN pass `1/1` (`141ms`) / `1/1` (`78ms`); the combined three-
type target passes `6/6` (`125ms`), raw `open_tiff` passes `228/228` (`750ms`),
facade `getexif` passes `221/221` (`3312ms`), and full passes `2549/2549`
(`27688ms`). Registrations are `1256/1293`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`41A4F6E7B0466195870D253D56B235542041D7BA1D8EF6A66763A9C0DCDC5FD1`.

Latest `META-002CR`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-2 `DefaultCropOrigin` 50719 values `(12,34)`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 returns an integer tuple from both `getexif()` and `tag_v2`
without warnings. Raw RED received `[]`; facade RED missed 50719. Explicit
type-5/type-4 native dispatch makes raw/facade GREEN pass `1/1` (`156ms`) /
`1/1` (`62ms`); the combined RATIONAL/LONG target passes `4/4` (`109ms`), raw
`open_tiff` passes `227/227` (`844ms`), facade `getexif` passes `220/220`
(`3250ms`), and full passes `2547/2547` (`28766ms`). Registrations are
`1255/1292`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `443/443`, zero difference; SHA-256 is
`0A1D444C12E8F4F1868C727ADDD8659E712CEEFCEA93F5EE246FADDAC6A15497`.

Latest `META-002CQ`: one 136-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SHORT/count-2 `DefaultCropSize` 50720 inline values `(320,240)`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution
info. Pillow 11.3.0 returns an integer tuple from both `getexif()` and
`tag_v2` without warnings. Raw RED received `[]`; facade RED missed 50720.
Raw/facade GREEN pass `1/1` (`125ms`) / `1/1` (`62ms`), raw `open_tiff`
passes `226/226` (`796ms`), facade `getexif` passes `219/219` (`3469ms`), and
full passes `2545/2545` (`27766ms`). Registrations are `1254/1291`. Release
x64 builds with zero warnings/errors; source/DLL exports remain `443/443`,
zero difference; SHA-256 is
`A90061B241C951A9EB5347A663BCF783F03D330950BF055E1C646009FF4648D5`.

Latest `META-002CP`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-2 `DefaultCropSize` 50720 values `(640,480)`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 returns an integer tuple from both `getexif()` and `tag_v2`
without warnings. Raw RED received `[]`; facade RED missed 50720. The first
native attempt exposed an `else-if` dispatch collision: the untyped 50720
RATIONAL branch consumed the LONG tag before the uint-array route. Explicit
type-5/type-4 dispatch then made the exact raw/facade GREEN filters pass `2/2`
(`125ms`) / `2/2` (`93ms`), including the existing LONG regression; raw
`open_tiff` passes `225/225` (`781ms`), facade `getexif` passes `218/218`
(`3266ms`), and full passes `2543/2543` (`28063ms`). Registrations are
`1253/1290`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `443/443`, zero difference; SHA-256 is
`D7F6F25EF289740291878406F9B2F71FB51045EA8D86508556FB6EAD0F559F2A`.

Latest `META-002CO`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
RATIONAL/count-2 `DefaultCropSize` 50720 values `(11/2,13/4)`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution
info. Pillow 11.3.0 names 50720 in `TiffTags.TAGS` but leaves it absent from
`TAGS_V2`; `getexif()` and `tag_v2` return two exact `IFDRational` values
without warnings. Raw RED received `[]`; facade RED missed 50720. The exact
raw/facade GREEN filters pass `2/2` (`125ms`) / `2/2` (`94ms`), including the
existing `OriginalDefaultCropSize` regression; raw `open_tiff` passes
`224/224` (`703ms`), facade `getexif` passes `217/217` (`3188ms`), and full
passes `2541/2541` (`28203ms`). Registrations are `1252/1289`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`6246F478156903A9A8A5AFA2E67682D6D6062608AC3F7DA985B9F451BDD4AA3A`.

Latest `META-002CN`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
RATIONAL/count-2 `DefaultCropOrigin` 50719 values `(5/2,7/4)`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution
info. Pillow 11.3.0 names 50719 in `TiffTags.TAGS` but leaves it absent from
`TAGS_V2`; `getexif()` and `tag_v2` return two exact `IFDRational` values
without warnings. Raw RED received `[]`; facade RED missed 50719. Raw/facade
GREEN passes `1/1` (`172ms`) / `1/1` (`63ms`), raw `open_tiff` passes
`223/223` (`671ms`), facade `getexif` passes `216/216` (`3187ms`), and full
passes `2539/2539` (`27672ms`). Registrations are `1251/1288`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`83CCF587C943CF623C7128AB7AB902C02CA326390C5AE5A8167B2EAB66572364`.

Latest `META-002CM`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
RATIONAL/count-2 `DefaultScale` 50718 values `(1/2,3/4)`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 names 50718 in `TiffTags.TAGS` but leaves it absent from
`TAGS_V2`; `getexif()` and `tag_v2` return two exact `IFDRational` values
without warnings. Raw RED received `[]`; facade RED missed 50718. Raw/facade
GREEN passes `1/1` (`172ms`) / `1/1` (`78ms`), raw `open_tiff` passes
`222/222` (`766ms`), facade `getexif` passes `215/215` (`3172ms`), and full
passes `2537/2537` (`27781ms`). Registrations are `1250/1287`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`1506BDB1A43C07E006A498F8159D0A146E2847107F6107296DB5222E9F9FD082`.

Latest `META-002CL`: one 136-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SHORT/count-1 `WhiteLevel` 50717 value `1023`, preserves pixels `[17,34]`, and
reports raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 names
50717 in `TiffTags.TAGS` but leaves it absent from `TAGS_V2`; `getexif()` and
`tag_v2` return integer `1023` without warnings. Raw RED received the default
`-1`; facade RED missed 50717. Raw/facade GREEN passes `1/1` (`109ms`) / `1/1`
(`63ms`), raw `open_tiff` passes `221/221` (`703ms`), facade `getexif` passes
`214/214` (`3219ms`), and full passes `2535/2535` (`27937ms`). Registrations
are `1249/1286`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`D0B6E765558584F92F6E1640B777B639E42CF5D7232649CB9EA3A49CD5897E21`.

Latest `META-002CK`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SRATIONAL/count-1 `BlackLevelDeltaV` 50716 value `-1/3`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 names 50716 in `TiffTags.TAGS` but leaves it absent from
`TAGS_V2`; `getexif()` and `tag_v2` return one exact `IFDRational` value
without warnings. Raw RED received `[]`; facade RED missed 50716. Raw/facade
GREEN passes `1/1` (`125ms`) / `1/1` (`62ms`), raw `open_tiff` passes
`220/220` (`750ms`), facade `getexif` passes `213/213` (`3172ms`), and full
passes `2533/2533` (`27375ms`). Registrations are `1248/1285`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`6EE08274AC0FCE33DC47A5FCA7FC2133C6C328A930A2FDC286919D5C7DB4E60D`.

Latest `META-002CJ`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SRATIONAL/count-2 `BlackLevelDeltaH` 50715 values `(-1/2,3/4)`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 names 50715 in `TiffTags.TAGS` but leaves it absent from
`TAGS_V2`; `getexif()` and `tag_v2` return two exact `IFDRational` values
without warnings. Raw RED received `[]`; facade RED missed 50715. Raw/facade
GREEN passes `1/1` (`141ms`) / `1/1` (`78ms`), raw `open_tiff` passes
`219/219` (`766ms`), facade `getexif` passes `212/212` (`3110ms`), and full
passes `2531/2531` (`27359ms`). Registrations are `1247/1284`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`C361674E869131CC09CD70DCAC21B786CFF9E7FF496C72C06AB2874D672AF726`.

Latest `META-002CI`: one 168-byte strip-decoded 2x1 mode-L TIFF carries IFD0
RATIONAL/count-4 `BlackLevel` 50714 values `(1/2,3/4,5/6,7/8)`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 names 50714 in `TiffTags.TAGS` but leaves it absent from
`TAGS_V2`; `getexif()` and `tag_v2` return four exact `IFDRational` values
without warnings. Raw RED received `[]`; facade RED missed 50714. Raw/facade
GREEN passes `1/1` (`125ms`) / `1/1` (`63ms`), raw `open_tiff` passes
`218/218` (`813ms`), facade `getexif` passes `211/211` (`3110ms`), and full
passes `2529/2529` (`27703ms`). Registrations are `1246/1283`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`CDADE4E2A3019A6766D8930BA72D907B9067C798B16E411DD84A6DA285E94974`.

Latest `META-002CH`: one 136-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SHORT/count-2 `BlackLevelRepeatDim` 50713 values `(2,2)`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 names 50713 in `TiffTags.TAGS` but leaves it absent from
`TAGS_V2`; `getexif()` and `tag_v2` return the exact tuple without warnings.
Raw RED received `[]`; facade RED missed 50713. Raw/facade GREEN passes `1/1`
(`141ms`) / `1/1` (`62ms`), raw `open_tiff` passes `217/217` (`813ms`),
facade `getexif` passes `210/210` (`3109ms`), and full passes `2527/2527`
(`27813ms`). Registrations are `1245/1282`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`695E064262A52A218368A3DA7CD5FE10B113CC7949F99B58AC7DFB3EF891BE4B`.

Latest `META-002CG`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SHORT/count-4 `LinearizationTable` 50712 values `(0,1,2,3)`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 names 50712 in `TiffTags.TAGS` but leaves it absent from
`TAGS_V2`; `getexif()` and `tag_v2` return the exact tuple without warnings.
Raw RED received `[]`; facade RED missed 50712. Raw/facade GREEN passes `1/1`
(`109ms`) / `1/1` (`109ms`), raw `open_tiff` passes `216/216` (`781ms`),
facade `getexif` passes `209/209` (`3110ms`), and full passes `2525/2525`
(`27938ms`). Registrations are `1244/1281`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`B1967A33E132A7EC74EA633A2829453FFB113CD07691C700A009260325265D2D`.

Latest `META-002CF`: one 136-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-1 `RowInterleaveFactor` 50975 with inline value `3`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50975 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return integer `3` without warnings. Raw
RED received `-1`; facade RED missed 50975. Raw/facade GREEN passes `1/1`
(`125ms`) / `1/1` (`63ms`), raw `open_tiff` passes `215/215` (`688ms`),
facade `getexif` passes `208/208` (`2985ms`), and full passes `2523/2523`
(`27562ms`). Registrations are `1243/1280`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`7A6F5FF56DF7FC8559F8CAE389614445D9F664284DA0646CBA21787EF07F12C0`.

Latest `META-002CE`: one 136-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-1 `SubTileBlockSize` 50974 with inline value `4`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50974 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return integer `4` without warnings. Raw
RED received `-1`; facade RED missed 50974. Raw/facade GREEN passes `1/1`
(`125ms`) / `1/1` (`62ms`), raw `open_tiff` passes `214/214` (`750ms`),
facade `getexif` passes `207/207` (`3140ms`), and full passes `2521/2521`
(`27734ms`). Registrations are `1242/1279`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`875143AD68ABCFB903F1CB06483E69387598A8C8734A0577EBA95612745C39E7`.

Latest `META-002CD`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
BYTE/count-16 `RawDataUniqueID` 50781 bytes `01..10`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50781 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact bytes without warnings.
Raw RED received `[]`; facade RED missed 50781. Raw/facade GREEN passes `1/1`
(`125ms`) / `1/1` (`63ms`), raw `open_tiff` passes `213/213` (`703ms`),
facade `getexif` passes `206/206` (`2938ms`), and full passes `2519/2519`
(`27250ms`). Registrations are `1241/1278`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`5FBD24C732E2B73E0A1B2FBD28E818457CA3F59F38E086859AB807636911E304`.

Latest `META-002CC`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
BYTE/count-16 `OriginalRawFileDigest` 50973 bytes `01..10`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50973 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact bytes without warnings.
Raw RED received `[]`; facade RED missed 50973. Raw/facade GREEN passes `1/1`
(`125ms`) / `1/1` (`63ms`), raw `open_tiff` passes `212/212` (`688ms`),
facade `getexif` passes `205/205` (`3000ms`), and full passes `2517/2517`
(`27344ms`). Registrations are `1240/1277`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`DE7D42A30F17CB0FA9A748D701D69BF026DBF84B311B9836016D4377287475D2`.

Latest `META-002CB`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
BYTE/count-16 `RawImageDigest` 50972 bytes `01..10`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50972 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact bytes without warnings.
Raw RED received `[]`; facade RED missed 50972. Raw/facade GREEN filters pass
`2/2` (`140ms`) / `2/2` (`78ms`), including the existing 51111 digest
regression; raw `open_tiff` passes `211/211` (`656ms`), facade `getexif`
passes `204/204` (`3000ms`), and full passes `2515/2515` (`28188ms`).
Registrations are `1239/1276`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`792BFCFB09823D981366D875ADD2128C650A38842E069A8EEE6D1E18F582A577`.

Latest `META-002CA`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
UNDEFINED/count-16 `PreviewSettingsDigest` 50969 bytes `01..10`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution
info. Pillow 11.3.0 leaves 50969 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact bytes without warnings.
Raw RED received `[]`; facade RED missed 50969. Raw/facade GREEN passes `1/1`
(`125ms`) / `1/1` (`62ms`), raw `open_tiff` passes `210/210` (`672ms`),
facade `getexif` passes `203/203` (`2969ms`), and full passes `2513/2513`
(`27156ms`). Registrations are `1238/1275`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`F06D8C96A6F6EF9FBA0D11E50DC17B83E807CB403C194FB9476A9BECBDEA937D`.

Latest `META-002BZ`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
UNDEFINED/count-8 `OpcodeList3` 51022 bytes `01 02 03 04 05 06 07 08`,
preserves pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/
resolution info. Pillow 11.3.0 leaves 51022 unnamed/unregistered in
`TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2` return the exact bytes
without warnings. Raw RED received `[]`; facade RED missed 51022. Raw/facade
GREEN passes `1/1` (`125ms`) / `1/1` (`63ms`), raw `open_tiff` passes
`209/209` (`687ms`), facade `getexif` passes `202/202` (`2969ms`), and full
passes `2511/2511` (`27812ms`). Registrations are `1237/1274`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`D600F073D8DEC391194B3785B81071A14484317E809BF01F33973BA866C9A174`.

Latest `META-002BY`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
UNDEFINED/count-8 `OpcodeList2` 51009 bytes `01 02 03 04 05 06 07 08`,
preserves pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/
resolution info. Pillow 11.3.0 leaves 51009 unnamed/unregistered in
`TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2` return the exact bytes
without warnings. Raw RED received `[]`; facade RED missed 51009. Raw/facade
GREEN passes `1/1` (`141ms`) / `1/1` (`63ms`), raw `open_tiff` passes
`208/208` (`797ms`), facade `getexif` passes `201/201` (`2922ms`), and full
passes `2509/2509` (`27593ms`). Registrations are `1236/1273`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`C543E5341058B6EDACF063C0F58C71A758F2341CBDE9713073E00E2B4582E954`.

Latest `META-002BX`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
UNDEFINED/count-8 `OpcodeList1` 51008 bytes `01 02 03 04 05 06 07 08`,
preserves pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/
resolution info. Pillow 11.3.0 leaves 51008 unnamed/unregistered in
`TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2` return the exact bytes
without warnings. Raw RED received `[]`; facade RED missed 51008. Raw/facade
GREEN passes `1/1` (`125ms`) / `1/1` (`62ms`), raw `open_tiff` passes
`207/207` (`734ms`), facade `getexif` passes `200/200` (`2891ms`), and full
passes `2507/2507` (`27734ms`). Registrations are `1235/1272`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`64D75DA69A5557066AD3B071BBD7C51821EC7F0487C72E7A1E42FA62ECDDED85`.

Latest `META-002BW`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
UNDEFINED/count-8 `OriginalRawFileData` 50828 bytes
`01 02 03 04 05 06 07 08`, preserves pixels `[17,34]`, and reports raw
compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 leaves 50828
unnamed/unregistered in `TiffTags.TAGS` and `TAGS_V2`; `getexif()` and
`tag_v2` return the exact bytes without warnings. Raw RED received `[]`;
facade RED missed 50828. Raw/facade GREEN passes `1/1` (`140ms`) / `1/1`
(`62ms`), raw `open_tiff` passes `206/206` (`687ms`), facade `getexif` passes
`199/199` (`2782ms`), and full passes `2505/2505` (`27625ms`). Registrations
are `1234/1271`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`995D76AB0C77C005DE40DB5EB6E36540C62DD618C170B58D12986B415EE8E00A`.

Latest `META-002BV`: one 136-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SHORT/count-1 `ColorimetricReference` 50879 with value `1`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50879 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return integer `1` without warnings. Raw
RED received `-1`; facade RED missed 50879. Raw/facade GREEN passes `1/1`
(`125ms`) / `1/1` (`63ms`), raw `open_tiff` passes `205/205` (`625ms`),
facade `getexif` passes `198/198` (`2797ms`), and full passes `2503/2503`
(`26406ms`). Registrations are `1233/1270`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`7E4ACDC7F08C285ABAB29879A0D766D6C882FB32781A5A1506571D8B634D106A`.

Latest `META-002BU`: one 208-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SRATIONAL/count-9 `CurrentPreProfileMatrix` 50834 with exact pairs
`[(1,1),(-1,2),(0,1),(0,1),(1,1),(0,1),(0,1),(0,1),(1,1)]`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50834 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return nine exact IFDRational values
without warnings. Raw RED received `[]`; facade RED missed 50834. Raw/facade
GREEN passes `1/1` (`125ms`) / `1/1` (`63ms`), raw `open_tiff` passes
`204/204` (`719ms`), facade `getexif` passes `197/197` (`2719ms`), and full
passes `2501/2501` (`27171ms`). Registrations are `1232/1269`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`FD655F6E558AAAF25A3C353618CCC047FE9CCAB0966B251D02BF5973C4DBA5DB`.

Latest `META-002BT`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
BYTE/count-8 `CurrentICCProfile` 50833 bytes `01 02 03 04 05 06 07 08`,
preserves pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/
resolution info without `icc_profile`. Pillow 11.3.0 leaves 50833 unnamed/
unregistered in `TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2` return
the exact bytes without warnings. Raw RED received `[]`; facade RED missed
50833. Raw/facade GREEN passes `1/1` (`94ms`) / `1/1` (`62ms`), raw
`open_tiff` passes `203/203` (`719ms`), facade `getexif` passes `196/196`
(`2750ms`), and full passes `2499/2499` (`26781ms`). Registrations are
`1231/1268`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `443/443`, zero difference; SHA-256 is
`5EF8D42F9C0A749D37A1596D4BA4A242BD5BE5CE41A8665F5902028C938C5DD5`.

Latest `META-002BS`: one 208-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SRATIONAL/count-9 `AsShotPreProfileMatrix` 50832 with exact pairs
`[(1,1),(-1,2),(0,1),(0,1),(1,1),(0,1),(0,1),(0,1),(1,1)]`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50832 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return nine exact IFDRational values
without warnings. Raw RED received `[]`; facade RED missed 50832. Raw/facade
GREEN passes `1/1` (`141ms`) / `1/1` (`62ms`), raw `open_tiff` passes
`202/202` (`688ms`), facade `getexif` passes `195/195` (`2734ms`), and full
passes `2497/2497` (`27250ms`). Registrations are `1230/1267`. Release x64
builds with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`FBD112B12870F0C106C5184FAD26FE23C2C24F5063C0015CFC6E764BABCC44E4`.

Latest `META-002BR`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
BYTE/count-8 `AsShotICCProfile` 50831 bytes `01 02 03 04 05 06 07 08`,
preserves pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/
resolution info without `icc_profile`. Pillow 11.3.0 leaves 50831 unnamed/
unregistered in `TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2` return
the exact bytes without warnings. Raw RED received `[]`; facade RED missed
50831. Raw/facade GREEN passes `1/1` (`312ms`) / `1/1` (`63ms`), raw
`open_tiff` passes `201/201` (`718ms`), facade `getexif` passes `194/194`
(`2579ms`), and full passes `2495/2495` (`26234ms`). Registrations are
`1229/1266`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `443/443`, zero difference; SHA-256 is
`E801D2E72A4683B59DE718F204E83B9D62B6A0A585C13A41463AC00F1D3A7578`.

Latest `META-002BQ`: one 168-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-8 `MaskedAreas` 50830 as `(0,0,1,1,0,1,1,2)`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50830 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact integer tuple without
warnings. Raw RED received `[]`; facade RED missed 50830. Raw/facade GREEN
passes `1/1` (`157ms`) / `1/1` (`79ms`), raw `open_tiff` passes `200/200`
(`734ms`), facade `getexif` passes `193/193` (`2578ms`), and full passes
`2493/2493` (`26594ms`). Registrations are `1228/1265`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`12AFB52CA0375E5A926330DED25490E2AFA436C9CCB9D122C3EFCDBC85116B47`.

Latest `META-002BP`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-4 `MaskedAreas` 50830 as `(0,0,1,2)`, preserves pixels `[17,34]`,
and reports raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0
leaves 50830 unnamed/unregistered in `TiffTags.TAGS` and `TAGS_V2`;
`getexif()` and `tag_v2` return the exact integer tuple without warnings. Raw
RED received `[]`; facade RED missed 50830. Raw/facade GREEN passes `1/1`
(`110ms`) / `1/1` (`47ms`), raw `open_tiff` passes `199/199` (`609ms`), facade
`getexif` passes `192/192` (`2563ms`), and full passes `2491/2491` (`26109ms`).
Registrations are `1227/1264`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`935BEAF26BAD20D9D19435BE70EC2C2C925FE8A567ADCC2B05F9A68CC42185F0`.

Latest `META-002BO`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SHORT/count-4 `ActiveArea` 50829 as `(0,0,1,2)`, preserves pixels `[17,34]`,
and reports raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0
leaves 50829 unnamed/unregistered in `TiffTags.TAGS` and `TAGS_V2`;
`getexif()` and `tag_v2` return the exact integer tuple without warnings. Raw
RED received `[]`; facade RED missed 50829. Native dispatch selects the SHORT
or LONG route only for TIFF type 3 or 4. Fresh raw/facade GREEN passes `1/1`
(`62ms`) / `1/1` (`62ms`), raw `open_tiff` passes `198/198` (`641ms`), facade
`getexif` passes `191/191` (`2640ms`), and full passes `2489/2489` (`26547ms`).
Registrations are `1226/1263`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`4F358ECF4C0672369E7157E1CB0E1943D2B64C4585998C56ED1FBD1F4FAEF283`.

Latest `META-002BN`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-4 `ActiveArea` 50829 as `(0,0,1,2)`, preserves pixels `[17,34]`,
and reports raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0
leaves 50829 unnamed/unregistered in `TiffTags.TAGS` and `TAGS_V2`;
`getexif()` and `tag_v2` return the exact integer tuple without warnings. Raw
RED received `[]`; facade RED missed 50829. Raw/facade GREEN passes `1/1`
(`141ms`) / `1/1` (`31ms`), raw `open_tiff` passes `197/197` (`671ms`),
facade `getexif` passes `190/190` (`2594ms`), and full passes `2487/2487`
(`26937ms`). Registrations are `1225/1262`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`BF3B4D8B718C02AC3B9BD9747AF1ADC103EC20D691702E838541BCE04494D4C5`.

Latest `META-002BM`: one 145-byte strip-decoded 2x1 mode-L TIFF carries IFD0
ASCII/count-9 `CameraLabel` 51092 as `Camera A\0`, preserves pixels `[17,34]`,
and reports raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0
leaves 51092 unnamed/unregistered in `TiffTags.TAGS` and `TAGS_V2`;
`getexif()` and `tag_v2` return exact string `Camera A` without warnings. Raw
RED received an empty string; facade RED missed 51092. Raw/facade GREEN passes
`1/1` (`62ms`) / `1/1` (`31ms`), raw `open_tiff` passes `196/196` (`718ms`),
facade `getexif` passes `189/189` (`2813ms`), and full passes `2485/2485`
(`26407ms`). Registrations are `1224/1261`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`6359CFB3DFD63B0C9D47DF7DE814F4F212DCF1198ECCDCF5F258113B7B3DEF55`.

Latest `META-002BL`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-2 `OriginalDefaultCropSize` 51091 as `(4000,3000)`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 51091 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact integer tuple without
warnings. Raw RED received `[]`; facade RED missed 51091. Native metadata
dispatch now selects the type-5 RATIONAL or type-4 LONG array parser before
the existing EXIF route. Raw/facade GREEN passes `1/1` (`125ms`) / `1/1`
(`31ms`); RATIONAL raw/facade regression passes `1/1` (`16ms`) / `1/1`
(`32ms`); raw `open_tiff` passes `195/195` (`656ms`), facade `getexif` passes
`188/188` (`2484ms`), and full passes `2483/2483` (`26750ms`). Registrations
are `1223/1260`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`4F8415CF5F80FDA284A0011F591255AECE49737D5BF68FA674425B355436F7DC`.

Latest `META-002BK`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
RATIONAL/count-2 `OriginalDefaultCropSize` 51091 as exact values `8001/2` and
`6001/2`, preserves pixels `[17,34]`, and reports raw compression plus `(1,1)`
DPI/resolution info. Pillow 11.3.0 leaves 51091 unnamed/unregistered in
`TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2` return the exact
IFDRational values without warnings. Raw RED received `[]`; facade RED missed
51091. Raw/facade GREEN passes `1/1` (`125ms`) / `1/1` (`31ms`), raw
`open_tiff` passes `194/194` (`734ms`), facade `getexif` passes `187/187`
(`2563ms`), and full passes `2481/2481` (`26422ms`). Registrations are
`1222/1259`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `443/443`, zero difference; SHA-256 is
`46C943988AD2C5448E4459DA4A258B5040E570ADA1058950833D8C4517097F61`.

Latest `META-002BJ`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-2 `OriginalBestQualityFinalSize` 51090 as `(6000,4000)`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 51090 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact tuple without warnings.
Raw RED received `[]`; facade RED missed 51090. Raw/facade GREEN passes `1/1`
(`141ms`) / `1/1` (`63ms`), raw `open_tiff` passes `193/193` (`687ms`), facade
`getexif` passes `186/186` (`2672ms`), and full passes `2479/2479` (`26579ms`).
Registrations are `1221/1258`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`5F392D7940A11C6D802E91CBED8BF32BB5369E0E31140B0A3862DDE44127CC8D`.

Latest `META-002BI`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-2 `OriginalDefaultFinalSize` 51089 as `(4000,3000)`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 51089 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact tuple without warnings.
Raw RED received `[]`; facade RED missed 51089. Raw/facade GREEN passes `1/1`
(`109ms`) / `1/1` (`62ms`), raw `open_tiff` passes `192/192` (`640ms`), facade
`getexif` passes `185/185` (`2547ms`), and full passes `2477/2477` (`26687ms`).
Registrations are `1220/1257`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`7FB1A351E254E8592592DAC374B2D07F40AED028FABA548273B09E2AF85B71CB`.

Latest `META-002BH`: one 146-byte strip-decoded 2x1 mode-L TIFF carries IFD0
ASCII/count-10 `ReelName` 51081 as `A001_C001\0`, preserves pixels `[17,34]`,
and reports raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0
leaves 51081 unnamed/unregistered in `TiffTags.TAGS` and `TAGS_V2`;
`getexif()` and `tag_v2` return exact string `A001_C001` without warnings.
Raw RED received an empty string; facade RED missed 51081. Raw/facade GREEN
passes `1/1` (`171ms`) / `1/1` (`63ms`), raw `open_tiff` passes `191/191`
(`609ms`), facade `getexif` passes `184/184` (`2672ms`), and full passes
`2475/2475` (`27375ms`). Registrations are `1219/1256`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`EE6220F222AC8D0CD59D6A6C6C25411231FC9F80400B7E16F2547B4F6EE9736E`.

Latest `META-002BG`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
RATIONAL/count-1 `TStop` 51058 as numerator `28` and denominator `10`,
preserves pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/
resolution info. Pillow 11.3.0 leaves 51058 unnamed/unregistered in
`TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2` return exact
`IFDRational(28,10)` without warnings. Raw RED received `[]`; facade RED
missed 51058. Raw/facade GREEN passes `1/1` (`78ms`) / `1/1` (`63ms`), raw
`open_tiff` passes `190/190` (`656ms`), facade `getexif` passes `183/183`
(`2484ms`), and full passes `2473/2473` (`27422ms`). Registrations are
`1218/1255`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `443/443`, zero difference; SHA-256 is
`387990225767EA05C7C319AFBBA98D10B4776FD16D595FC1FEBD5A1FBBE3CD04`.

Latest `META-002BF`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SRATIONAL/count-1 `FrameRate` 51044 as numerator `30000` and denominator
`1001`, preserves pixels `[17,34]`, and reports raw compression plus `(1,1)`
DPI/resolution info. Pillow 11.3.0 leaves 51044 unnamed/unregistered in
`TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2` return exact
`IFDRational(30000,1001)` without warnings. Raw RED received `[]`; facade RED
missed 51044. Raw/facade GREEN passes `1/1` (`141ms`) / `1/1` (`62ms`), raw
`open_tiff` passes `189/189` (`640ms`), facade `getexif` passes `182/182`
(`2828ms`), and full passes `2471/2471` (`26406ms`). Registrations are
`1217/1254`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `443/443`, zero difference; SHA-256 is
`6B07E5B4A172C4E5D680AFAC0814F738D1E649C02ED572657A39F66540872B16`.

Latest `META-002BE`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
BYTE/count-8 `TimeCodes` 51043 bytes `01 02 03 04 05 06 07 08`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 51043 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact bytes without warnings.
Raw RED received `[]`; facade RED missed 51043. Raw/facade GREEN passes `1/1`
(`78ms`) / `1/1` (`63ms`), raw `open_tiff` passes `188/188` (`625ms`), facade
`getexif` passes `181/181` (`2531ms`), and full passes `2469/2469` (`27610ms`).
Registrations are `1216/1253`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`D9ABA496C6A31656C34B942FE9609470ABA377EB27FE8B7CD5AAFE55F844D216`.

Latest `META-002BD`: one 168-byte strip-decoded 2x1 mode-L TIFF carries IFD0
DOUBLE/count-4 `NoiseProfile` 51041 tuple `(0.125,0.25,0.5,1.0)`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 51041 unnamed/unregistered in `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact tuple without warnings.
Raw/facade REDs received `[]`. Raw/facade GREEN passes `1/1` (`109ms`) / `1/1`
(`32ms`), raw `open_tiff` passes `187/187` (`703ms`), facade `getexif` passes
`180/180` (`2719ms`), and full passes `2467/2467` (`28110ms`). Registrations
are `1215/1252`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`09C6C3CD5E5864F903A1D1ECA6A8094E87562EB03B1605252D3FAADE322B7048`.

Latest `META-002BC`: 152-byte count-2 and 200-byte count-8 strip-decoded 2x1
mode-L TIFFs carry IFD0 DOUBLE `NoiseProfile` 51041 tuples `(0.125,0.75)` and
`(0.125,0.25,0.5,1.0,2.0,4.0,8.0,16.0)`, preserve pixels `[17,34]`, and
report raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 leaves
51041 unnamed/unregistered in `TiffTags.TAGS` and `TAGS_V2`; `getexif()` and
`tag_v2` return both exact tuples without warnings. Raw/facade REDs received
`[[],[]]`. Raw/facade GREEN passes `1/1` (`109ms`) / `1/1` (`47ms`), raw
`open_tiff` passes `186/186` (`703ms`), facade `getexif` passes `179/179`
(`2437ms`), and full passes `2465/2465` (`27781ms`). Registrations are
`1214/1251`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `443/443`, zero difference; SHA-256 is
`177426C6BEACB6971D7E7111008E1B0075F0B5AA4360678D92AE56671E89261C`.

Latest `META-002BB`: one 184-byte strip-decoded 2x1 mode-L TIFF carries IFD0
DOUBLE/count-6 `NoiseProfile` 51041 values
`(0.25,0.5,1.0,2.0,4.0,8.0)`, preserves pixels `[17,34]`, and reports raw
compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 leaves 51041
unnamed/unregistered in `TiffTags.TAGS` and `TAGS_V2`; `getexif()` and
`tag_v2` return the exact tuple without warnings. Raw RED received `[]`;
facade RED missed 51041. Raw/facade GREEN passes `1/1` (`109ms`) / `1/1`
(`32ms`), raw `open_tiff` passes `185/185` (`422ms`), facade `getexif` passes
`178/178` (`2453ms`), and full passes `2463/2463` (`28000ms`). Registrations
are `1213/1250`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`6D54586EE0C0FD10E6FFA0803E831A1CFC3F8BC7C9E8223D64C47F9559E2A980`.

Latest `META-002BA`: one 208-byte strip-decoded 2x1 mode-L TIFF carries IFD0
FLOAT/count-18 `ProfileToneCurve` 50940 as nine `(x,x^2)` control points for
`x=0.0,0.125,...,1.0`, preserves pixels `[17,34]`, and reports raw compression
plus `(1,1)` DPI/resolution info. Pillow 11.3.0 leaves 50940 unnamed/
unregistered in `TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2`
return the exact 18-value tuple without warnings. Raw RED received `[]`;
facade RED missed 50940. Raw/facade GREEN passes `1/1` (`94ms`) / `1/1`
(`31ms`), raw `open_tiff` passes `184/184` (`391ms`), facade `getexif` passes
`177/177` (`1485ms`), and full passes `2461/2461` (`19094ms`). Registrations
are `1212/1249`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`E05427246016849891808FA38AF4E53E8EC5713B228A3896E617A3CEAF17DD92`.

Latest `META-002AZ`: one 604-byte strip-decoded 2x1 mode-L TIFF composes IFD0
LONG/count-3 `ProfileHueSatMapDims` 50937 `(6,3,1)` with FLOAT/count-54
`ProfileHueSatMapData1` 50938 values from `-3.375` through `3.25` in `0.125`
steps and `ProfileHueSatMapData2` 50939 values from `0.0` through `3.3125` in
`0.0625` steps, preserves pixels `[17,34]`, and reports raw compression plus
`(1,1)` DPI/resolution info. Pillow 11.3.0 leaves all three tags unnamed/
unregistered in `TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2`
return all exact tuples without warnings. Raw RED received `[]` for 50938
after 50937 succeeded; facade RED missed 50938. Raw/facade GREEN passes `1/1`
(`125ms`) / `1/1` (`46ms`), raw `open_tiff` passes `183/183` (`656ms`),
facade `getexif` passes `176/176` (`2422ms`), and full passes `2459/2459`
(`26687ms`). Registrations are `1211/1248`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `443/443`, zero difference;
SHA-256 is
`C9C8F0BF21CA3F6B99DE40AEBCDDDF633C3F88D83C30E2C78395028FE269181A`.

Latest `META-002AY`: one 376-byte strip-decoded 2x1 mode-L TIFF composes IFD0
LONG/count-3 `ProfileLookTableDims` 50981 `(6,3,1)` with FLOAT/count-54
`ProfileLookTableData` 50982 values from `0.0` through `6.625` in `0.125`
steps, preserves pixels `[17,34]`, and reports raw compression plus `(1,1)`
DPI/resolution info. Pillow 11.3.0 leaves both tags unnamed/unregistered in
`TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2` return both exact
tuples without warnings. Raw RED received `[]` for 50982 after 50981
succeeded; facade RED missed 50982. Raw/facade GREEN passes `1/1` (`94ms`) /
`1/1` (`32ms`), raw `open_tiff` passes `182/182` (`656ms`), facade `getexif`
passes `175/175` (`2281ms`), and full passes `2457/2457` (`27828ms`).
Registrations are `1210/1247`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`474370B7068E785B4C90B3D4FC7EB08F65ACA65429533A89B2155C320770F5AB`.

Latest `META-002AX`: one 208-byte strip-decoded 2x1 mode-L TIFF carries IFD0
FLOAT/count-18 `ProfileLookTableData` 50982 values from `0.0` through `4.25`
in `0.25` steps, preserves pixels `[17,34]`, and reports raw compression plus
`(1,1)` DPI/resolution info. Pillow 11.3.0 leaves 50982 unnamed/unregistered
in both `TiffTags.TAGS` and `TAGS_V2`; `getexif()` and `tag_v2` return the
exact tuple without warnings. Raw RED received `[]`; facade RED missed 50982.
Raw/facade GREEN passes `1/1` (`94ms`) / `1/1` (`47ms`), raw `open_tiff`
passes `181/181` (`735ms`), facade `getexif` passes `174/174` (`2328ms`), and
full passes `2455/2455` (`26984ms`). Registrations are `1209/1246`. Release
x64 builds with zero warnings/errors; source/DLL exports remain `443/443`,
zero difference; SHA-256 is
`4DDD2E0207F2A4C92FA542B58D0660A6A84C093CA1A21E5E1E1104DC0C26F1E6`.

Latest `META-002AW`: one 160-byte strip-decoded 2x1 mode-L TIFF carries IFD0
FLOAT/count-6 `ProfileLookTableData` 50982 values
`(0.0,1.0,1.0,0.5,0.75,1.25)`, preserves pixels `[17,34]`, and reports raw
compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 leaves 50982
unnamed/unregistered in both `TiffTags.TAGS` and `TAGS_V2`; `getexif()` and
`tag_v2` return the exact tuple without warnings. Raw RED received `[]`;
facade RED missed 50982. Raw/facade GREEN passes `1/1` (`79ms`) / `1/1`
(`47ms`), raw `open_tiff` passes `180/180` (`609ms`), facade `getexif` passes
`173/173` (`2265ms`), and full passes `2453/2453` (`27360ms`). Registrations
are `1208/1245`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`A196C4BE9A353E5A104EAE2A528B31DA0BA053056D4C2D5969BE0766D4E9B03B`.

Latest `META-002AV`: one 148-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-3 `ProfileLookTableDims` 50981 values `(6,3,1)`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50981 unnamed/unregistered in both `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return the exact tuple without warnings.
Raw RED received `[]`; facade RED missed 50981. Raw/facade GREEN passes `1/1`
(`78ms`) / `1/1` (`47ms`), raw `open_tiff` passes `179/179` (`657ms`), facade
`getexif` passes `172/172` (`2360ms`), and full passes `2451/2451` (`27063ms`).
Registrations are `1207/1244`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`D3C5327D0027B9292427B7187AB8016A58BF9B7470DBD204FC623ECFA13F2BD5`.

Latest `META-002AU`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
RATIONAL/count-1 `NoiseReductionApplied` 50935 value `3/4`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 leaves 50935 unnamed/unregistered in both `TiffTags.TAGS` and
`TAGS_V2`; `getexif()` and `tag_v2` return exact `IFDRational(3,4)` without
warnings. Raw RED received `[]`; facade RED missed 50935. Raw/facade GREEN
passes `1/1` (`78ms`) / `1/1` (`32ms`), raw `open_tiff` passes `178/178`
(`641ms`), facade `getexif` passes `171/171` (`2281ms`), and full passes
`2449/2449` (`27594ms`). Registrations are `1206/1243`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `443/443`, zero
difference; SHA-256 is
`8BCFDA444FC3DD98B21D5A01AF6C63ADA3BDA22858D0A7B60AE3DAD91E82AD77`.

Latest `META-002AT`: one 160-byte strip-decoded 2x1 mode-L TIFF carries IFD0
FLOAT/count-6 `ProfileToneCurve` 50940 values
`(0.0,0.0,0.5,0.25,1.0,1.0)`, preserves pixels `[17,34]`, and reports raw
compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 leaves 50940
unnamed/unregistered in both `TiffTags.TAGS` and `TAGS_V2`; `getexif()` and
`tag_v2` return the exact tuple without warnings. Raw RED received `[]`;
facade RED missed 50940. Raw/facade GREEN passes `1/1` (`94ms`) / `1/1`
(`15ms`), raw `open_tiff` passes `177/177` (`641ms`), facade `getexif` passes
`170/170` (`2281ms`), and full passes `2447/2447` (`41344ms`). Registrations
are `1205/1242`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `443/443`, zero difference; SHA-256 is
`EDF2043BBEC611FEBA4261687FBE0A4907DCECC07FCC5639A641BAC91E941373`.

Latest `META-002AS`: one 160-byte strip-decoded 2x1 mode-L TIFF carries IFD0
FLOAT/count-6 `ProfileHueSatMapData2` 50939, preserves pixels `[17,34]`, and
reports raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 leaves
50939 unnamed/unregistered in both `TiffTags.TAGS` and `TAGS_V2`; `getexif()`
and `tag_v2` return exact float32-quantized tuple
`(0.0,-0.125,0.20000000298023224,1.75,-2.5,3.125)` without warnings. Raw RED
received `[]`; facade RED missed 50939. Raw/facade GREEN passes `1/1` (`94ms`)
/ `1/1` (`31ms`), raw `open_tiff` passes `176/176` (`625ms`), facade
`getexif` passes `169/169` (`2312ms`), and full passes `2445/2445` (`26453ms`).
Registrations are `1204/1241`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `443/443`, zero difference; SHA-256 is
`0DF7F6FB0637D9151F336EA21A60F3891960D25A88DD56FCB00C00B57BBCFB5E`.

Latest `META-002AR`: one 160-byte strip-decoded 2x1 mode-L TIFF carries IFD0
FLOAT/count-6 `ProfileHueSatMapData1` 50938, preserves pixels `[17,34]`, and
reports raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 leaves
50938 unnamed/unregistered in both `TiffTags.TAGS` and `TAGS_V2`; `getexif()`
and `tag_v2` return exact float32-quantized tuple
`(0.0,0.10000000149011612,-0.25,1.5,2.25,-3.75)` without warnings. Raw RED
failed on the missing `pillow_c_exif_float_array_tag` export; facade RED missed
50938. Raw/facade GREEN passes `1/1` (`16ms`) / `1/1` (`47ms`), raw
`open_tiff` passes `175/175` (`922ms`), facade `getexif` passes `168/168`
(`2406ms`), and full passes `2443/2443` (`27375ms`). Registrations are
`1203/1240`. Release x64 builds with zero warnings/errors; source/DLL exports
are `443/443`, zero difference; SHA-256 is
`885E91AA9C7A2665A2064DF55499F65C0B97D00841440D91797FEED45186729C`.

Latest `META-002AQ`: one 148-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-3 `ProfileHueSatMapDims` 50937, preserves pixels `[17,34]`, and
reports raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 leaves
50937 unnamed/unregistered in both `TiffTags.TAGS` and `TAGS_V2`;
`getexif()` and `tag_v2` return exact tuple `(6,3,1)` without warnings. Raw
RED received `[]`; facade RED missed 50937. Raw/facade GREEN passes `1/1`
(`63ms`) / `1/1` (`47ms`), raw `open_tiff` passes `174/174` (`359ms`), facade
`getexif` passes `167/167` (`1438ms`), and full passes `2441/2441` (`15750ms`).
Registrations are `1202/1239`. Release x64 builds with zero warnings/errors;
source/DLL exports remain `442/442`, zero difference; SHA-256 is
`269AE21A0052C16CE1C34EC9CDAFE1FA5A31732B47421D2BE34F0F9028020ABA`.

Latest `META-002AP`: one 872-byte strip-decoded 2x1 mode-L TIFF carries IFD0
DOUBLE/count-92 `RPCCoefficientTag` 50844, preserves pixels `[17,34]`, and
reports raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 leaves
50844 unnamed/unregistered in both `TiffTags.TAGS` and `TAGS_V2`;
`getexif()` and `tag_v2` return the exact 92-item tuple defined by
`values[index] = (index - 46) / 8.0` for index 0..91, from `-5.75` through
`5.625`, without warnings. Raw RED received `[]`; facade RED missed 50844.
Raw/facade GREEN passes `1/1` (`109ms`) / `1/1` (`63ms`), raw `open_tiff`
passes `173/173` (`766ms`), facade `getexif` passes `166/166` (`2562ms`), and
full passes `2439/2439` (`28187ms`). Registrations are `1201/1238`. Release
x64 builds with zero warnings/errors; source/DLL exports remain `442/442`,
zero difference; SHA-256 is
`F59E6E398D92A50D6815A3932531392A3994DBB4AFC117E7AA31D45C3C8E92D4`.

Latest `META-002AO`: one 211-byte strip-decoded 2x1 mode-L TIFF carries IFD0
ASCII `GDAL_METADATA` 42112 and `GDAL_NODATA` 42113, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 names both only in legacy `TiffTags.TAGS`; `TAGS_V2` has no
registered type or length. Type-2/count-57 42112 returns exact XML string
`<GDALMetadata><Item name="scale">2</Item></GDALMetadata>` and count-6 42113
returns `-9999` from `getexif()` and `tag_v2`, removing only NUL, without
warnings. Raw RED received `""`; facade RED missed 42112. Raw/facade GREEN
passes `1/1` (`141ms`) / `1/1` (`31ms`), raw `open_tiff` passes `172/172`
(`640ms`), facade `getexif` passes `165/165` (`2093ms`), and full passes
`2437/2437` (`25547ms`). Registrations are `1200/1237`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `442/442`, zero
difference; SHA-256 is
`D5DBA342EB4F5AFE5AF7ABA04F1A55A22DEFC412F9CF0996D55FF8772AB1195B`.

Latest `META-002AN`: one 152-byte strip-decoded 2x1 mode-L TIFF carries IFD0
SHORT/count-8 `GeoKeyDirectoryTag` 34735 values `(1,1,0,1,1024,0,1,1)`,
preserves pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/
resolution info. Pillow 11.3.0 names the tag only in legacy `TiffTags.TAGS`;
`TAGS_V2` has no registered type or length. `getexif()` and `tag_v2` return
the exact tuple without warnings. Raw RED received `[]`; facade RED missed
34735. Raw/facade GREEN passes `1/1` (`125ms`) / `1/1` (`31ms`), raw
`open_tiff` passes `171/171` (`593ms`), facade `getexif` passes `164/164`
(`2015ms`), and full passes `2435/2435` (`26922ms`). Registrations are
`1199/1236`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `442/442`, zero difference; SHA-256 is
`DE431941CBB09198CB17730D4256E9CCEB75E3B9663E196468E9E4F6FE6BF771`.

Latest `META-002AM`: one 151-byte strip-decoded 2x1 mode-L TIFF carries IFD0
ASCII/count-15 `GeoAsciiParamsTag` 34737 payload `WGS 84|meters|\0`, preserves
pixels `[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution
info. Pillow 11.3.0 names the tag only in legacy `TiffTags.TAGS`; `TAGS_V2`
has no registered type or length. `getexif()` and `tag_v2` return exact string
`WGS 84|meters|`, preserving the trailing `|` while omitting NUL, without
warnings. Raw RED received `""`; facade RED missed 34737. Raw/facade GREEN
passes `1/1` (`94ms`) / `1/1` (`47ms`), raw `open_tiff` passes `170/170`
(`657ms`), facade `getexif` passes `163/163` (`3203ms`), and full passes
`2433/2433` (`26453ms`). Registrations are `1198/1235`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `442/442`, zero
difference; SHA-256 is
`7242AEA0D0FC8A69A9BDEEB7F05E5C8E753F7FCFC415BACAEDAC1740B3BD4C91`.

Latest `META-002AL`: one 160-byte strip-decoded 2x1 mode-L TIFF carries IFD0
DOUBLE/count-3 `GeoDoubleParamsTag` 34736 values
`(6378137.0,298.257223563,-123.5)`, preserves pixels `[17,34]`, and reports
raw compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 names the tag
only in legacy `TiffTags.TAGS`; `TAGS_V2` has no registered type or length.
`getexif()` and `tag_v2` return the exact tuple without warnings. Raw RED
received `[]`; facade RED missed 34736. Raw/facade GREEN passes `1/1` (`94ms`)
/ `1/1` (`31ms`), raw `open_tiff` passes `169/169` (`609ms`), facade
`getexif` passes `162/162` (`1985ms`), and full passes `2431/2431`
(`26156ms`). Registrations are `1197/1234`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `442/442`, zero difference;
SHA-256 is
`0C38092979CDEEC528AD94E7745761910AC6D561564B57E0A7B2598F0960F73B`.

Latest `META-002AK`: one 264-byte strip-decoded 2x1 mode-L TIFF carries IFD0
DOUBLE/count-16 `ModelTransformationTag` 34264 values
`(1,0,0,100.5,0,1,0,200.25,0,0,1,300.75,0,0,0,1)`, preserves pixels
`[17,34]`, and reports raw compression plus `(1,1)` DPI/resolution info.
Pillow 11.3.0 names the tag only in legacy `TiffTags.TAGS`; `TAGS_V2` has no
registered type or length. `getexif()` and `tag_v2` return the exact tuple
without warnings. Raw RED received `[]`; facade RED missed 34264. Raw/facade
GREEN passes `1/1` (`109ms`) / `1/1` (`62ms`), raw `open_tiff` passes
`168/168` (`609ms`), facade `getexif` passes `161/161` (`2093ms`), and full
passes `2429/2429` (`27203ms`). Registrations are `1196/1233`. Release x64
builds with zero warnings/errors; source/DLL exports remain `442/442`, zero
difference; SHA-256 is
`F1A3D3518671B4F57BF028E64660F973E890C3788FF01FD4674E2D46D161820C`.

Latest `META-002AJ`: one 184-byte strip-decoded 2x1 mode-L TIFF carries IFD0
DOUBLE/count-6 `ModelTiepointTag` 33922 values
`(0,0,0,10.5,20.25,30.75)`, preserves pixels `[17,34]`, and reports raw
compression plus `(1,1)` DPI/resolution info. Pillow 11.3.0 names the tag only
in legacy `TiffTags.TAGS`; `TAGS_V2` has no registered type or length.
`getexif()` and `tag_v2` return the exact tuple without warnings. Raw RED
received `[]`; facade RED missed 33922. Raw/facade GREEN passes `1/1` (`78ms`)
/ `1/1` (`31ms`), raw `open_tiff` passes `167/167` (`609ms`), facade
`getexif` passes `160/160` (`2016ms`), and full passes `2427/2427`
(`26406ms`). Registrations are `1195/1232`. Release x64 builds with zero
warnings/errors; source/DLL exports remain `442/442`, zero difference;
SHA-256 is
`C506D9BA6465843EE44DF934E48286BC3BF1FF35A15F6EC3FDEA5260D9285D86`.

Latest `META-002AI`: one 160-byte strip-decoded 2x1 mode-L TIFF carries IFD0
DOUBLE/count-3 `ModelPixelScaleTag` 33550 values `(0.5,1.25,2.75)`, preserves
pixels `[17,34]`, and reports only raw compression plus `(1,1)` DPI/resolution
info. Pillow 11.3.0 names the tag only in legacy `TiffTags.TAGS`; `TAGS_V2`
has no registered type or length. `getexif()` and `tag_v2` return the exact
tuple without warnings. Raw RED failed on the missing
`pillow_c_exif_double_array_tag` export; facade RED missed 33550. Raw/facade
GREEN passes `1/1` (`94ms`) / `1/1` (`31ms`), raw `open_tiff` passes
`166/166` (`625ms`), facade `getexif` passes `159/159` (`2157ms`), and full
passes `2425/2425` (`27641ms`). Registrations are `1194/1231`. Release x64
builds with zero warnings/errors; source/DLL exports are `442/442`, zero
difference; SHA-256 is
`648CB69FC634E4C33BB86698C674AF555C32072BDCEEC144707AC95B7212C8C8`.

Latest `META-002AH`: one 150-byte strip-decoded 2x1 mode-L TIFF carries IFD0
BYTE/count-14 `PhotoshopInfo` tag 34377 bytes
`38 42 49 4D 04 04 00 00 00 00 00 02 41 42`, preserves pixels `[17,34]`,
and leaves `Info["photoshop"]`, `Info["iptc"]`, and `Info["exif"]` absent.
Pillow 11.3.0 returns the exact bytes from `getexif()` and `tag_v2` without
warnings. Raw RED received `[]`; facade RED missed 34377. Raw/facade GREEN
passes `1/1` (`16ms`) / `1/1` (`32ms`), raw `open_tiff` passes `165/165`
(`375ms`), facade `getexif` passes `158/158` (`1219ms`), and full passes
`2423/2423` (`15656ms`). Registrations are `1193/1230`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `441/441`, zero
difference; SHA-256 is
`0374A7AE74A4227D4041D5A08A4BB96F711B931F5C3A5505013F2E2E54720202`.

Latest `META-002AG`: one 144-byte strip-decoded 2x1 mode-L TIFF carries IFD0
UNDEFINED/count-8 `IptcNaaInfo` tag 33723 bytes
`1C 02 05 00 03 41 48 4B`, preserves pixels `[17,34]`, and leaves both
`Info["iptc"]` and `Info["exif"]` absent. Pillow 11.3.0 returns the exact bytes
from `getexif()` and `tag_v2` without warnings. Raw RED received `[]`; facade
RED missed 33723. Raw/facade GREEN passes `1/1` (`93ms`) / `1/1` (`31ms`),
raw `open_tiff` passes `164/164` (`375ms`), facade `getexif` passes `157/157`
(`1204ms`), and full passes `2421/2421` (`16344ms`). Registrations are
`1192/1229`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `441/441`, zero difference; SHA-256 is
`AF347A62BB8B3B15DDA76AB7946D50205652E20C41F1AFD72896003086183D19`.

Latest `META-001FD`: one 172-byte strip-decoded 2x1 mode-L TIFF carries IFD0
LONG/count-3 `TileOffsets` 324=`(200,220,240)` and `TileByteCounts`
325=`(2,2,2)`, preserves pixels `[17,34]`, and leaves `Info["exif"]` absent.
Pillow 11.3.0 returns both exact tuples from `getexif()` and `tag_v2` without
warnings. Raw RED opened with status `-3`; facade RED raised
`pillow_c: invalid argument`. Native recognition now requires both arrays to
be range-valid with equal count greater than one; EXIF serialization preserves
that exact count. Raw/facade GREEN passes `1/1` (`62ms`) / `1/1` (`31ms`),
raw `open_tiff` passes `163/163` (`375ms`), facade `getexif` passes `156/156`
(`1234ms`), and full passes `2419/2419` (`15953ms`). Registrations are
`1191/1228`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `441/441`, zero difference; SHA-256 is
`ED14677939A8C54A58A724D99EBABE5CC5C6ECF2F55F7A43A6A2EF62B585E235`.

Latest `META-001FC`: one 182-byte valid six-strip 2x6 mode-L TIFF carries
IFD0 LONG/count-6 `StripOffsets` 273=`(170,172,174,176,178,180)` and
`StripByteCounts` 279=`(2,2,2,2,2,2)`, preserves pixels
`[17,34,51,68,85,102,119,136,153,170,187,204]`, and leaves `Info["exif"]`
absent. Pillow 11.3.0 returns both exact tuples from `getexif()` and `tag_v2`
without warnings. Raw RED received `[]`; facade RED missed 273. Native
273/279 routing now accepts every validated count greater than one while
count-1 remains scalar. Raw/facade GREEN passes `1/1` (`171ms`) / `1/1`
(`62ms`), raw `open_tiff` passes `162/162` (`656ms`), facade `getexif` passes
`155/155` (`1969ms`), and full passes `2417/2417` (`27469ms`). Registrations
are `1190/1227`. Release x64 builds with zero warnings/errors; source/DLL
exports remain `441/441`, zero difference; SHA-256 is
`1F11AE2A5BB584B73A93C9798FDA1A09D05C0CCC0076EB8F666E8E6BA4E43DF5`.

Latest `META-001FB`: one 172-byte valid five-strip 2x5 mode-L TIFF carries
IFD0 LONG/count-5 `StripOffsets` 273=`(162,164,166,168,170)` and
`StripByteCounts` 279=`(2,2,2,2,2)`, preserves pixels
`[17,34,51,68,85,102,119,136,153,170]`, and leaves `Info["exif"]` absent.
Pillow 11.3.0 returns both exact tuples from `getexif()` and `tag_v2` without
warnings. Raw RED received `[]`; facade RED missed 273. Raw/facade GREEN
passes `1/1` (`125ms`) / `1/1` (`62ms`), raw `open_tiff` passes `161/161`
(`610ms`), facade `getexif` passes `154/154` (`1937ms`), and full passes
`2415/2415` (`27953ms`). Registrations are `1189/1226`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `441/441`, zero
difference; SHA-256 is
`A7BBA1529EB2D2D8D26097A631D8BD306D44CC8A3EB9A201B0DF2BE3500B1C72`.

Latest `META-001FA`: one 162-byte valid four-strip 2x4 mode-L TIFF carries
IFD0 LONG/count-4 `StripOffsets` 273=`(154,156,158,160)` and
`StripByteCounts` 279=`(2,2,2,2)`, preserves pixels
`[17,34,51,68,85,102,119,136]`, and leaves `Info["exif"]` absent. Pillow
11.3.0 returns both exact tuples from `getexif()` and `tag_v2` without
warnings. Raw RED received `[]`; facade RED missed 273. Raw/facade GREEN
passes `1/1` (`125ms`) / `1/1` (`63ms`), raw `open_tiff` passes `160/160`
(`563ms`), facade `getexif` passes `153/153` (`1859ms`), and full passes
`2413/2413` (`27094ms`). Registrations are `1188/1225`. Release x64 builds
with zero warnings/errors; source/DLL exports remain `441/441`, zero
difference; SHA-256 is
`6DE3FE177B939B06D49233D069F21908337E5E4107667B8656B9B1D2BA7F7703`.

Latest `META-001EZ`: one 152-byte valid three-strip 2x3 mode-L TIFF carries
IFD0 LONG/count-3 `StripOffsets` 273=`(146,148,150)` and `StripByteCounts`
279=`(2,2,2)`, preserves pixels `[17,34,51,68,85,102]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns both exact tuples from
`getexif()` and `tag_v2` without warnings. Raw RED received `[]`; facade RED
missed 273. Raw/facade GREEN passes `1/1` (`79ms`) / `1/1` (`32ms`), raw
`open_tiff` passes `159/159` (`343ms`), facade `getexif` passes `152/152`
(`1094ms`), and full passes `2411/2411` (`23625ms`). Registrations are
`1187/1224`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `441/441`, zero difference; SHA-256 is
`9B47AA8572DBF44332A56B12732EE1FBAC35F7CE546D033ED4B3C6D515EB5EC9`.

Latest `META-001EY`: one 142-byte valid two-strip 2x2 mode-L TIFF carries
IFD0 LONG/count-2 `StripOffsets` 273=`(138,140)` and `StripByteCounts`
279=`(2,2)`, preserves pixels `[17,34,51,68]`, and leaves `Info["exif"]`
absent. Pillow 11.3.0 returns both exact tuples from `getexif()` and `tag_v2`
without warnings. Raw RED received `[]`; facade RED missed 273. The first
facade subdomain run then exposed count-1 273/279 duplication across scalar
and array maps; length-based routing restored the scalar shape. Final raw/
facade GREEN passes `1/1` (`109ms`) / `1/1` (`63ms`), raw `open_tiff` passes
`158/158` (`641ms`), facade `getexif` passes `151/151` (`1938ms`), and full
passes `2409/2409` (`26500ms`). Registrations are `1186/1223`. Release x64
builds with zero warnings/errors; source/DLL exports remain `441/441`, zero
difference; SHA-256 is
`71C9903631C8D7A3697156CFAB4F296D6E2389DC2A013656AE7219CCE91DEDCD`.

Latest `META-001EX`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
LONG/count-2 `TileOffsets` 324=`(200,220)` and `TileByteCounts` 325=`(2,2)`,
preserves pixels `[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0
returns both exact tuples from `getexif()` and `tag_v2` without warnings. Raw/
facade RED failed with open status `-3` / `pillow_c: invalid argument` because
WIC interpreted the metadata as actual tiled storage. The native L-strip path
now recognizes the bounded pair while reading pixels from the valid strip;
count-1 324/325 entries remain scalar. Final raw/facade GREEN passes `1/1`
(`31ms`) / `1/1` (`63ms`), the scalar regression passes `1/1` (`109ms`), raw
`open_tiff` passes `157/157` (`735ms`), facade `getexif` passes `150/150`
(`1875ms`), and full passes `2407/2407` (`27453ms`). Registrations are
`1185/1222`. Release x64 builds with zero warnings/errors; source/DLL exports
remain `441/441`, zero difference; SHA-256 is
`B1A6B7078923BA0C3727F955B1B591797420ACA98A7E2B51D4318992F14A2C32`.

Latest `META-001EW`: one 152-byte strip-based 2x1 mode-L TIFF carries IFD0
LONG/count-4 `MaskSubArea` 52536=`(1,2,9,8)`, preserves pixels `[17,34]`, and
leaves `Info["exif"]` absent. Pillow 11.3.0 returns the exact tuple from both
`getexif()` and `tag_v2`; the facade exposes exact AHK Array values through a
distinct uint-array route. Raw RED failed on the missing
`pillow_c_exif_uint_array_tag` export; facade RED missed tag 52536. Raw/facade
GREEN passes `1/1` (`109ms`) / `1/1` (`31ms`), raw `open_tiff` passes
`156/156` (`547ms`), facade `getexif` passes `149/149` (`1828ms`), and full
passes `2405/2405` (`25157ms`). Registrations are `1184/1221`. Release x64
builds with zero warnings/errors; source/DLL exports are `441/441`, zero
difference; SHA-256 is
`57EAEE953EA7D1B3270FF60B83B2595E6A36E9EF3FFEEDA4B77592A01FF6DA4F`.

Latest `META-001EV`: one 172-byte strip-based 2x1 mode-L TIFF carries IFD0
UNDEFINED `IlluminantData1` 52533=`[1,2,3,4]`, `IlluminantData2`
52534=`[5,6,7,8,9]`, and `IlluminantData3`
52535=`[10,11,12,13,14,15]`, covering inline and out-of-line payloads while
preserving pixels `[17,34]` and leaving `Info["exif"]` absent. Pillow 11.3.0
returns exact Python `bytes` from both `getexif()` and `tag_v2`; the facade
exposes exact `Buffer` values. Raw RED expected `[1,2,3,4]` but received `[]`;
facade RED missed tag 52533. Raw/facade GREEN passes `1/1` (`109ms`) / `1/1`
(`31ms`), raw `open_tiff` passes `155/155` (`579ms`), facade `getexif` passes
`148/148` (`1953ms`), and full passes `2403/2403` (`27422ms`). Registrations
are `1183/1220`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `440/440`, zero difference; SHA-256 is
`999AC5A31CE4C771598CA143B4CF4FAA6D4EDEA495628C8EE859F3511683B322`.

Latest `META-001EU`: one 376-byte strip-based 2x1 mode-L TIFF carries IFD0
count-9 SRATIONAL `CameraCalibration3` 52530, `ColorMatrix3` 52531, and
`ForwardMatrix3` 52532, including negative numerators, preserves pixels
`[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0 returns tuples of
nine exact `IFDRational` values from both `getexif()` and `tag_v2`; the facade
exposes nested signed pairs. Raw RED expected the 52530 matrix but received
`[]`; facade RED missed tag 52530. Raw/facade GREEN passes `1/1` (`125ms`) /
`1/1` (`32ms`), raw `open_tiff` passes `154/154` (`547ms`), facade `getexif`
passes `147/147` (`1843ms`), and full passes `2401/2401` (`22594ms`).
Registrations are `1182/1219`. Release x64 builds with zero warnings/errors;
no export was added, source/DLL exports remain `440/440`, zero difference;
SHA-256 is
`C3DE15D28FF950967039DD6AF1A3F28C4C74AF729D9AE7BF2DC9DB00F76BB070`.

Latest `META-001ET`: one 136-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar SHORT `CalibrationIlluminant3` 52529=`23`, preserves pixels `[17,34]`,
and leaves `Info["exif"]` absent. Pillow 11.3.0 returns Python `int(23)` from
both `getexif()` and `tag_v2`; the facade exposes scalar `23`. Raw RED failed
`Expected 23, got -1`; facade RED missed tag 52529. Raw/facade GREEN passes
`1/1` (`63ms`) / `1/1` (`15ms`), raw `open_tiff` passes `153/153` (`407ms`),
facade `getexif` passes `146/146` (`1078ms`), and full passes `2399/2399`
(`15875ms`). Registrations are `1181/1218`. Release x64 builds with zero
warnings/errors; no export was added, source/DLL exports remain `440/440`,
zero difference; SHA-256 is
`AC8F2763A8CB0BB0BD194AADBC47B6CAD765724650AC3BB3BFEF5B713209371D`.

Latest `META-001ES`: one 170-byte strip-based 2x1 mode-L TIFF carries IFD0
ASCII `SemanticName` 52526=`foreground` and `SemanticInstanceID`
52528=`instance-1`, preserves pixels `[17,34]`, and leaves `Info["exif"]`
absent. Pillow 11.3.0 returns exact strings from both `getexif()` and `tag_v2`;
the facade exposes both strings. Raw RED failed `Expected "foreground", got
""`; facade RED missed tag 52526. Raw/facade GREEN passes `1/1` (`47ms`) /
`1/1` (`15ms`), raw `open_tiff` passes `152/152` (`359ms`), facade `getexif`
passes `145/145` (`1047ms`), and full passes `2397/2397` (`16625ms`).
Registrations are `1180/1217`. Release x64 builds with zero warnings/errors;
no export was added, source/DLL exports remain `440/440`, zero difference;
SHA-256 is
`5EF9D7E038B52B2E44318943A7185D517468AF99883EF119133B3C014653BE65`.

Latest `META-001ER`: one 143-byte strip-based 2x1 mode-L TIFF carries IFD0
ASCII `EnhanceParams` 51182=`gain=1`, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns the exact string from both
`getexif()` and `tag_v2`; the facade exposes `gain=1`. Raw RED failed
`Expected "gain=1", got ""`; facade RED missed tag 51182. Raw/facade GREEN
passes `1/1` (`125ms`) / `1/1` (`31ms`), raw `open_tiff` passes `151/151`
(`563ms`), facade `getexif` passes `144/144` (`1734ms`), and full passes
`2395/2395` (`28141ms`). Registrations are `1179/1216`. Release x64 builds
with zero warnings/errors; no export was added, source/DLL exports remain
`440/440`, zero difference; SHA-256 is
`0324036328D67B153503661DDB77AB0742E74E63AF7FF582C367DB9FEE13F3CE`.

Latest `META-001EQ`: one 148-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar SHORT `DepthUnits` 51180=`1` and `DepthMeasureType` 51181=`2`,
preserves pixels `[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0
returns Python `int` values from both `getexif()` and `tag_v2`; the facade
exposes `1` / `2`. Raw RED failed `Expected 1, got -1`; facade RED missed tag
51180. Raw/facade GREEN passes `1/1` (`78ms`) / `1/1` (`32ms`), raw
`open_tiff` passes `150/150` (`656ms`), facade `getexif` passes `143/143`
(`1813ms`), and full passes `2393/2393` (`26812ms`). Registrations are
`1178/1215`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `440/440`, zero difference; SHA-256 is
`B7D3D97DB7D2589CB4DD8D6FAE9BA27081961A1C0C1F52381229AE12D31603D7`.

Latest `META-001EP`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar RATIONAL `DepthNear` 51178=`3/2` and `DepthFar` 51179=`25/2`, preserves
pixels `[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0 returns exact
`IFDRational` values from both `getexif()` and `tag_v2`; the facade exposes
`[3,2]` / `[25,2]`. Raw RED failed `Expected [3, 2], got []`; facade RED
missed tag 51178. Raw/facade GREEN passes `1/1` (`109ms`) / `1/1` (`31ms`),
raw `open_tiff` passes `149/149` (`578ms`), facade `getexif` passes `142/142`
(`1796ms`), and full passes `2391/2391` (`26687ms`). Registrations are
`1177/1214`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `440/440`, zero difference; SHA-256 is
`A8B6B7AEB648214EEA06DD3F8C96BD640440D31EF917679237DD2C2EF831742C`.

Latest `META-001EO`: one 136-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar SHORT `DepthFormat` 51177=`1`, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns Python `int(1)` from both
`getexif()` and `tag_v2`; the facade exposes scalar `1`. Raw RED failed
`Expected 1, got -1`; facade RED missed tag 51177. Raw/facade GREEN passes
`1/1` (`110ms`) / `1/1` (`31ms`), raw `open_tiff` passes `148/148` (`547ms`),
facade `getexif` passes `141/141` (`1687ms`), and full passes `2389/2389`
(`26203ms`). Registrations are `1176/1213`. Release x64 builds with zero
warnings/errors; no export was added, source/DLL exports remain `440/440`,
zero difference; SHA-256 is
`31DBF42A0908ABED726E6093D75BF73E732237D911064158FD1B7E11414124D9`.

Latest `META-001EN`: one 168-byte strip-based 2x1 mode-L TIFF carries IFD0
count-4 RATIONAL-array `DefaultUserCrop` 51125 as
`[[1,10],[2,10],[9,10],[8,10]]`, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns four exact `IFDRational` values
from both `getexif()` and `tag_v2`; the facade exposes nested pairs. Raw RED
expected the four pairs but received `[]`; facade RED missed tag 51125. Raw/
facade GREEN passes `1/1` (`109ms`) / `1/1` (`31ms`), raw `open_tiff` passes
`147/147` (`562ms`), facade `getexif` passes `140/140` (`1828ms`), and full
passes `2387/2387` (`26703ms`). Registrations are `1175/1212`. Release x64
builds with zero warnings/errors; no export was added, source/DLL exports
remain `440/440`, zero difference; SHA-256 is
`4B32B99FDF9526E607E6C7C0F8159DCC35AF8415A5065EA4FAFA99AB1705F17D`.

Latest `META-001EM`: one 144-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar RATIONAL `RawToPreviewGain` 51112=`7/4`, preserves pixels `[17,34]`,
and leaves `Info["exif"]` absent. Pillow 11.3.0 returns an exact
`IFDRational` from both `getexif()` and `tag_v2`; the facade exposes `[7,4]`.
Raw RED failed `Expected [7, 4], got []`; facade RED missed tag 51112. Raw/
facade GREEN passes `1/1` (`110ms`) / `1/1` (`31ms`), raw `open_tiff` passes
`146/146` (`641ms`), facade `getexif` passes `139/139` (`1735ms`), and full
passes `2385/2385` (`26609ms`). Registrations are `1174/1211`. Release x64
builds with zero warnings/errors; no export was added, source/DLL exports
remain `440/440`, zero difference; SHA-256 is
`357F8B89DA7910145A0A204CA363BDB35264C9FCEB158F191716E1293D69E2DF`.

Latest `META-001EL`: one 152-byte strip-based 2x1 mode-L TIFF carries IFD0
BYTE-array `NewRawImageDigest` 51111=`00..0f`, preserves pixels `[17,34]`, and
leaves `Info["exif"]` absent. Pillow 11.3.0 returns exact Python `bytes` from
both `getexif()` and `tag_v2`; the facade materializes a `Buffer`. Raw RED
expected the 16 bytes but received `[]`; facade RED missed tag 51111. Raw/
facade GREEN passes `1/1` (`125ms`) / `1/1` (`31ms`), raw `open_tiff` passes
`145/145` (`515ms`), facade `getexif` passes `138/138` (`1703ms`), and full
passes `2383/2383` (`26922ms`). Registrations are `1173/1210`. Release x64
builds with zero warnings/errors; no export was added, source/DLL exports
remain `440/440`, zero difference; SHA-256 is
`456BBA568BBFD5326BAA0F6BE46055BA78B5093E6BBDB988CE84BE6E94959933`.

Latest `META-001EK`: one 136-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar LONG `DefaultBlackRender` 51110=`1`, preserves pixels `[17,34]`, and
leaves `Info["exif"]` absent. Pillow 11.3.0 returns an exact Python `int` from
both `getexif()` and `tag_v2`. Raw RED failed `Expected 1, got -1`; facade RED
missed tag 51110. Raw/facade GREEN passes `1/1` (`125ms`) / `1/1` (`32ms`),
raw `open_tiff` passes `144/144` (`562ms`), facade `getexif` passes `137/137`
(`1656ms`), and full passes `2381/2381` (`27141ms`). Registrations are
`1172/1209`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `440/440`, zero difference; SHA-256 is
`5801FD3430FAC67E031D3D98861D2A82AD1D42B7C27CBED3077411FF4E8BCA7B`.

Latest `META-001EJ`: one 144-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar SRATIONAL `BaselineExposureOffset` 51109=`-5/6`, preserves pixels
`[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0 returns an exact
`IFDRational` from both `getexif()` and `tag_v2`. Raw RED failed `Expected
[-5, 6], got []`; facade RED missed tag 51109. Raw/facade GREEN passes `1/1`
(`110ms`) / `1/1` (`46ms`), raw `open_tiff` passes `143/143` (`531ms`),
facade `getexif` passes `136/136` (`1625ms`), and full passes `2379/2379`
(`27328ms`). Registrations are `1171/1208`. Release x64 builds with zero
warnings/errors; no export was added, source/DLL exports remain `440/440`,
zero difference; SHA-256 is
`733883A4A6BE43B9042A0F1782D73E66DBD32C090F40BAA62A207BFAD8F6F460`.

Latest `META-001EI`: one 148-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar LONG `ProfileHueSatMapEncoding` 51107=`1` and
`ProfileLookTableEncoding` 51108=`0`, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns exact Python `int` values from
both `getexif()` and `tag_v2`. Raw RED failed `Expected 1, got -1`; facade RED
missed tag 51107. Raw/facade GREEN passes `1/1` (`109ms`) / `1/1` (`31ms`),
raw `open_tiff` passes `142/142` (`531ms`), facade `getexif` passes `135/135`
(`1688ms`), and full passes `2377/2377` (`26187ms`). Registrations are
`1170/1207`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `440/440`, zero difference; SHA-256 is
`CD59FE9346A70B4468C472514AC8278ADC98EDF8C5F1CB966D37F21A67C761B1`.

Latest `META-001EH`: one 148-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar LONG `ProfileEmbedPolicy` 50941=`1` and `PreviewColorSpace` 50970=`2`,
preserves pixels `[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0
returns exact Python `int` values from both `getexif()` and `tag_v2`. Raw RED
failed `Expected 1, got -1`; facade RED missed tag 50941. Raw/facade GREEN
passes `1/1` (`109ms`) / `1/1` (`47ms`), raw `open_tiff` passes `141/141`
(`547ms`), facade `getexif` passes `134/134` (`1578ms`), and full passes
`2375/2375` (`26625ms`). Registrations are `1169/1206`. Release x64 builds
with zero warnings/errors; no export was added, source/DLL exports remain
`440/440`, zero difference; SHA-256 is
`3923CFEC64FABB5823DC6CBABDAA3A611DD97C667D84BEEBE26C8170AEE23BC5`.

Latest `META-001EG`: one 292-byte strip-based 2x1 mode-L TIFF carries IFD0
count-9 SRATIONAL arrays `ForwardMatrix1` 50964 and `ForwardMatrix2` 50965,
including negative numerators, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns tuples of nine exact
`IFDRational` values from both `getexif()` and `tag_v2`; the facade exposes
the established nested `[numerator, denominator]` shape. Raw RED expected the
50964 matrix but received `[]`; facade RED missed tag 50964. Raw/facade GREEN
passes `1/1` (`125ms`) / `1/1` (`63ms`), raw `open_tiff` passes `140/140`
(`516ms`), facade `getexif` passes `133/133` (`1578ms`), and full passes
`2373/2373` (`27141ms`). Registrations are `1168/1205`. Release x64 builds
with zero warnings/errors; no export was added, source/DLL exports remain
`440/440`, zero difference; SHA-256 is
`0F84990C1AD7D85024D4437205D701F02820C5C32EAA329099BE2845180E302A`.

Latest `META-001EF`: one 460-byte strip-based 2x1 mode-L TIFF carries IFD0
count-9 SRATIONAL arrays `CameraCalibration1/2` 50723/50724 and
`ReductionMatrix1/2` 50725/50726, including negative numerators, preserves
pixels `[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0 returns
tuples of nine exact `IFDRational` values from both `getexif()` and `tag_v2`;
the facade exposes the established nested `[numerator, denominator]` shape.
Raw RED expected the 50723 matrix but received `[]`; facade RED missed tag
50723. Raw/facade GREEN passes `1/1` (`125ms`) / `1/1` (`62ms`), raw
`open_tiff` passes `139/139` (`563ms`), facade `getexif` passes `132/132`
(`1515ms`), and full passes `2371/2371` (`24797ms`). Registrations are
`1167/1204`. Release x64 builds with zero warnings/errors; no export was added,
source/DLL exports remain `440/440`, zero difference; SHA-256 is
`4CC2EFACC00C3F718C1081E797E99D131252DF720DC9A198086023A2F2A6BC81`.

Latest `META-001EE`: one 292-byte strip-based 2x1 mode-L TIFF carries IFD0
count-9 SRATIONAL arrays `ColorMatrix1` 50721 and `ColorMatrix2` 50722,
including negative numerators, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns tuples of nine exact
`IFDRational` values from both `getexif()` and `tag_v2`; the facade exposes
the established nested `[numerator, denominator]` shape. Raw RED failed on
the missing `pillow_c_exif_signed_rational_array_tag` export; facade RED
failed on missing tag 50721. Raw/facade GREEN passes `1/1` (`109ms`) / `1/1`
(`63ms`), raw `open_tiff` passes `138/138` (`578ms`), facade `getexif` passes
`131/131` (`1719ms`), and full passes `2369/2369` (`26156ms`). Registrations
are `1166/1203`. Release x64 builds with zero warnings/errors; source/DLL
exports are `440/440`, zero difference; SHA-256 is
`FC352ACA7327DD77B85F9CB48743248C6BC0058746E0EE018B5DE6B6647E7639`.

Latest `META-001ED`: one 196-byte strip-based 2x1 mode-L TIFF carries IFD0
RATIONAL arrays `AsShotWhiteXY` 50729=`[[3127,10000],[3290,10000]]` with count
`2` and `LensInfo` 50736=`[[24,1],[70,1],[28,10],[4,1]]` with count `4`,
preserves pixels `[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0
returns tuples of exact `IFDRational` values from `getexif()` and `tag_v2`;
the facade exposes the established nested `[numerator, denominator]` shape.
Raw RED failed `Expected [[3127, 10000], [3290, 10000]], got []`; facade RED
failed on missing tag 50729. Raw/facade GREEN passes `1/1` (`125ms`) / `1/1`
(`31ms`), raw `open_tiff` passes `137/137` (`313ms`), facade `getexif` passes
`130/130` (`891ms`), and full passes `2367/2367` (`16172ms`). Registrations
are `1165/1202`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`2A57BFD683396184AA89F2E35B54DE33DBC468911CA3DB4C235D3401386D3F12`.

Latest `META-001EC`: one 196-byte strip-based 2x1 mode-L TIFF carries IFD0
count-3 RATIONAL arrays `AnalogBalance` 50727=`[[2,1],[1,1],[3,2]]` and
`AsShotNeutral` 50728=`[[1,2],[1,1],[2,3]]`, preserves pixels `[17,34]`, and
leaves `Info["exif"]` absent. Pillow 11.3.0 returns tuples of exact
`IFDRational` values from `getexif()` and `tag_v2`; the facade exposes the
established nested `[numerator, denominator]` shape. Raw RED failed
`Expected [[2, 1], [1, 1], [3, 2]], got []`; facade RED failed on missing tag
50727. Raw/facade GREEN passes `1/1` (`125ms`) / `1/1` (`78ms`), raw
`open_tiff` passes `136/136` (`484ms`), facade `getexif` passes `129/129`
(`1500ms`), and full passes `2365/2365` (`27265ms`). Registrations are
`1164/1201`. Release x64 builds with zero warnings/errors; no export was added,
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`76A70C52BAFF46A86B849AA1A2286AE69BA07CCEC122E093B9203D2841671481`.

Latest `META-001EB`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar SRATIONAL `BaselineExposure` 50730=`-3/2` and `ShadowScale`
50739=`5/4`, preserves pixels `[17,34]`, and leaves `Info["exif"]` absent.
Pillow 11.3.0 returns exact `IFDRational` values from `getexif()` and `tag_v2`;
the facade exposes their established `[numerator, denominator]` shape. Raw RED
failed `Expected [-3, 2], got []`; facade RED failed on missing tag 50730.
Raw/facade GREEN passes `1/1` (`47ms`) / `1/1` (`47ms`), raw `open_tiff`
passes `135/135` (`328ms`), facade `getexif` passes `128/128` (`938ms`), and
full passes `2363/2363` (`15406ms`). Registrations are `1163/1200`. Release
x64 builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`448BDF085FCF0A3AB436FE74F5176C206E48C9AF53246AB8F5953AE1B0FDDC7B`.

Latest `META-001EA`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar RATIONAL `AntiAliasStrength` 50738=`4/5` and `BestQualityScale`
50780=`9/8`, preserves pixels `[17,34]`, and leaves `Info["exif"]` absent.
Pillow 11.3.0 returns exact `IFDRational` values from `getexif()` and `tag_v2`;
the facade exposes their established `[numerator, denominator]` shape. Raw RED
failed `Expected [4, 5], got []`; facade RED failed on missing tag 50738.
Raw/facade GREEN passes `1/1` (`46ms`) / `1/1` (`78ms`), raw `open_tiff`
passes `134/134` (`297ms`), facade `getexif` passes `127/127` (`922ms`), and
full passes `2361/2361` (`15843ms`). Registrations are `1162/1199`. Release
x64 builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`787511F3B8F4E4CD655FB82FF76F5A54336473AEDD670DFF047B7BBF523BDC42`.

Latest `META-001DZ`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar RATIONAL `LinearResponseLimit` 50734=`3/4` and `ChromaBlurRadius`
50737=`7/3`, preserves pixels `[17,34]`, and leaves `Info["exif"]` absent.
Pillow 11.3.0 returns exact `IFDRational` values from `getexif()` and `tag_v2`;
the facade exposes their established `[numerator, denominator]` shape. Raw RED
failed `Expected [3, 4], got []`; facade RED failed on missing tag 50734.
Raw/facade GREEN passes `1/1` (`62ms`) / `1/1` (`31ms`), raw `open_tiff`
passes `133/133` (`297ms`), facade `getexif` passes `126/126` (`937ms`), and
full passes `2359/2359` (`15734ms`). Registrations are `1161/1198`. Release
x64 builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`D63D9B718A5FB91E97BD37DF061FC3DA31536A2434806A026B59A9D5E6D16FBB`.

Latest `META-001DY`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar RATIONAL `BaselineNoise` 50731=`3/2` and `BaselineSharpness`
50732=`5/4`, preserves pixels `[17,34]`, and leaves `Info["exif"]` absent.
Pillow 11.3.0 returns exact `IFDRational` values from `getexif()` and `tag_v2`;
the facade exposes their established `[numerator, denominator]` shape. Raw RED
failed `Expected [3, 2], got []`; facade RED failed on missing tag 50731.
Raw/facade GREEN passes `1/1` (`62ms`) / `1/1` (`31ms`), raw `open_tiff`
passes `132/132` (`422ms`), facade `getexif` passes `125/125` (`907ms`), and
full passes `2357/2357` (`15282ms`). Registrations are `1160/1197`. Release
x64 builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`9B8F454E583ABE7912E5215BD5712457AD94D3526705B62176961A5C6692980B`.

Latest `META-001DX`: one 148-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar SHORT `CalibrationIlluminant1` 50778=`17` and
`CalibrationIlluminant2` 50779=`21`, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns exact Python `int` values from
`getexif()` and `tag_v2`. Raw RED failed `Expected 17, got -1`; facade RED
failed on missing tag 50778. Raw/facade GREEN passes `1/1` (`140ms`) / `1/1`
(`47ms`), raw `open_tiff` passes `131/131` (`328ms`), facade `getexif` passes
`124/124` (`953ms`), and full passes `2355/2355` (`15547ms`). Registrations
are `1159/1196`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`AD1AB30FD1A6855ECD9868EB75724A017CE644553F39E3437C96559917BFF808`.

Latest `META-001DW`: one 148-byte strip-based 2x1 mode-L TIFF carries IFD0
scalar SHORT `CFALayout` 50711=`2` and `MakerNoteSafety` 50741=`1`, preserves
pixels `[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0 returns exact
Python `int` values from `getexif()` and `tag_v2`. Raw RED failed
`Expected 2, got -1`; facade RED failed on missing tag 50711. Raw/facade GREEN
passes `1/1` (`125ms`) / `1/1` (`31ms`), raw `open_tiff` passes `130/130`
(`328ms`), facade `getexif` passes `123/123` (`765ms`), and full passes
`2353/2353` (`15703ms`). Registrations are `1158/1195`. Release x64 builds
with zero warnings/errors; no export was added, source/DLL exports remain
`439/439`, zero difference; SHA-256 is
`FDF150A2D011F812CA29AE1EA3B31BA18E7CB00F875FE517EA3CD2C103303466`.

Latest `META-001DV`: one 158-byte strip-based 2x1 mode-L TIFF carries IFD0
type-2 ASCII `CameraSerialNumber` 50735=`SN1` inline and `ProfileCopyright`
50942=`copyright` out-of-line, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns exact `str` values from
`getexif()` and `tag_v2`. Raw RED failed `Expected "SN1", got ""`; facade RED
failed on missing tag 50735. Raw/facade GREEN passes `1/1` (`63ms`) / `1/1`
(`31ms`), raw `open_tiff` passes `129/129` (`250ms`), facade `getexif` passes
`122/122` (`718ms`), and full passes `2351/2351` (`14296ms`). Registrations
are `1157/1194`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`FE32ACE5FC6C4719E0B3CAE88C57A44C0672E5665AF816AA45683F144B51B286`.

Latest `META-001DU`: one 168-byte strip-based 2x1 mode-L TIFF carries IFD0
type-2 ASCII `PreviewSettingsName` 50968=`SET` inline and `PreviewDateTime`
50971=`2026:08:05 12:34:56` out-of-line, preserves pixels `[17,34]`, and
leaves `Info["exif"]` absent. Pillow 11.3.0 returns exact `str` values from
`getexif()` and `tag_v2`. Raw RED failed `Expected "SET", got ""`; facade RED
failed on missing tag 50968. Raw/facade GREEN passes `1/1` (`94ms`) / `1/1`
(`47ms`), raw `open_tiff` passes `128/128` (`329ms`), facade `getexif` passes
`121/121` (`875ms`), and full passes `2349/2349` (`15688ms`). Registrations
are `1156/1193`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`14FF2E76A893DD74CBAA4CEA82802BC3DF79CCC7807AA8F8D432773C49ECD448`.

Latest `META-001DT`: one 154-byte strip-based 2x1 mode-L TIFF carries IFD0
type-2 ASCII `PreviewApplicationName` 50966=`APP` inline and
`PreviewApplicationVersion` 50967=`1.2.3` out-of-line, preserves pixels
`[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0 returns exact `str`
values from `getexif()` and `tag_v2`. Raw RED failed
`Expected "APP", got ""`; facade RED failed on missing tag 50966. Raw/facade
GREEN passes `1/1` (`93ms`) / `1/1` (`31ms`), raw `open_tiff` passes
`127/127` (`328ms`), facade `getexif` passes `120/120` (`891ms`), and full
passes `2347/2347` (`15406ms`). Registrations are `1155/1192`. Release x64
builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`8EF7C717F11C6A560E79E0226A6BE08ABEAFE1FDE24E4A0B28E0B781EF325CFB`.

Latest `META-001DS`: one 156-byte strip-based 2x1 mode-L TIFF carries IFD0
type-2 ASCII `AsShotProfileName` 50934=`ASP` inline and `ProfileName`
50936=`profile` out-of-line, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns exact `str` values from
`getexif()` and `tag_v2`. Raw RED failed `Expected "ASP", got ""`; facade RED
failed on missing tag 50934. Raw/facade GREEN passes `1/1` (`46ms`) / `1/1`
(`32ms`), raw `open_tiff` passes `126/126` (`313ms`), facade `getexif` passes
`119/119` (`797ms`), and full passes `2345/2345` (`15125ms`). Registrations
are `1154/1191`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`DC1F10F9AF4885B77BCC96747594B1306AB31F59C0741BC1404BEE3683412F4D`.

Latest `META-001DR`: one 156-byte strip-based 2x1 mode-L TIFF carries IFD0
type-2 ASCII `CameraCalibrationSignature` 50931=`CAL` inline and
`ProfileCalibrationSignature` 50932=`profile` out-of-line, preserves pixels
`[17,34]`, and leaves `Info["exif"]` absent. Pillow 11.3.0 returns exact `str`
values from `getexif()` and `tag_v2`. Raw RED failed
`Expected "CAL", got ""`; facade RED failed on missing tag 50931. Raw/facade
GREEN passes `1/1` (`79ms`) / `1/1` (`47ms`), raw `open_tiff` passes
`125/125` (`266ms`), facade `getexif` passes `118/118` (`812ms`), and full
passes `2343/2343` (`15454ms`). Registrations are `1153/1190`. Release x64
builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`AB900BB1D9AD154313C68FCAF557F194E7EBA0200543D56D997DD0DAB6090CE9`.

Latest `META-001DQ`: one 156-byte strip-based 2x1 mode-L TIFF carries IFD0
type-2 ASCII `UniqueCameraModel` 50708=`CAM` inline and
`OriginalRawFileName` 50827=`raw.dng` out-of-line, preserves pixels `[17,34]`,
and leaves `Info["exif"]` absent. Pillow 11.3.0 returns exact `str` values from
`getexif()` and `tag_v2`. Raw RED failed `Expected "CAM", got ""`; facade RED
failed on missing tag 50708. Raw/facade GREEN passes `1/1` (`140ms`) / `1/1`
(`47ms`), raw `open_tiff` passes `124/124` (`469ms`), facade `getexif` passes
`117/117` (`1172ms`), and full passes `2341/2341` (`24719ms`). Registrations
are `1152/1189`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`A3623563D62A1AC7B6A647CCA6D075AC95191928396EF86B9793B28A577F6FF3`.

Latest `META-001DP`: one 154-byte strip-based 2x1 mode-L TIFF carries IFD0
type-1 BYTE `LocalizedCameraModel` 50709=`[67,65,77,0,255]` out-of-line and
`CFAPlaneColor` 50710=`[0,1,2]` inline, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns exact `bytes` from `getexif()` and
`tag_v2`. Raw RED failed `Expected [67, 65, 77, 0, 255], got []`; facade RED
failed on missing tag 50709. Raw/facade GREEN passes `1/1` (`63ms`) / `1/1`
(`32ms`), raw `open_tiff` passes `123/123` (`343ms`), facade `getexif` passes
`116/116` (`781ms`), and full passes `2339/2339` (`15469ms`). Registrations
are `1151/1188`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`654C1AEADF64FF64526EF041B980D0108BEB9855CC9543A727AE56BFBA34E39A`.

Latest `META-001DO`: one 148-byte strip-based 2x1 mode-L TIFF carries IFD0
type-1 BYTE `DNGVersion` 50706=`[1,6,0,0]` and `DNGBackwardVersion`
50707=`[1,4,0,0]`, preserves pixels `[17,34]`, and leaves `Info["exif"]`
absent. Pillow 11.3.0 returns exact `bytes` from `getexif()` and `tag_v2`.
Raw RED failed `Expected [1, 6, 0, 0], got []`; facade RED failed on missing
tag 50706. Raw/facade GREEN passes `1/1` (`62ms`) / `1/1` (`47ms`), raw
`open_tiff` passes `122/122` (`312ms`), facade `getexif` passes `115/115`
(`782ms`), and full passes `2337/2337` (`15672ms`). Registrations are
`1150/1187`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`2F3228B64ABA7AF1E861F7830CDE98B704B78A9B0050B8F878D2DF09741DA468`.

Latest `META-001DN`: one 154-byte strip-based 2x1 mode-L TIFF carries IFD0
`UserComment` 37510 bytes `[65,66,0,255]` as an inline type-7 UNDEFINED
payload and `ImageSourceData` 37724 bytes `[73,83,68,0,255]` as an out-of-line
type-7 UNDEFINED payload, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Pillow 11.3.0 returns exact `bytes` from `getexif()` and
`tag_v2`. Raw RED failed `Expected [65, 66, 0, 255], got []`; facade RED
failed on missing tag 37510. Raw/facade GREEN passes `1/1` (`63ms`) / `1/1`
(`31ms`), raw `open_tiff` passes `121/121` (`265ms`), facade `getexif` passes
`114/114` (`797ms`), and full passes `2335/2335` (`15281ms`). Registrations
are `1149/1186`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`13231A1195E4B5EEDB3E0690EC3853FB08D6303A50DFF7D57EDE3ED835E1FD76`.

Latest `META-001DM`: one 154-byte strip-based 2x1 mode-L TIFF carries IFD0
`CFAPattern` 41730 bytes `[2,2,0,1]` as an inline UNDEFINED payload and
`DeviceSettingDescription` 41995 bytes `[68,69,86,0,255]` as an out-of-line
UNDEFINED payload, preserves pixels `[17,34]`, and leaves `Info["exif"]`
absent. Raw RED failed `Expected [2, 2, 0, 1], got []`; facade RED failed on
missing tag 41730. Raw/facade GREEN passes `1/1` (`125ms`) / `1/1` (`62ms`),
raw `open_tiff` passes `120/120` (`485ms`), facade `getexif` passes `113/113`
(`1094ms`), and full passes `2333/2333` (`26422ms`). Registrations are
`1148/1185`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`83F36D5C7E535214A7D6C08D3A121C6CCE461FB05E24222B56CE9490B8297FB2`.

Latest `META-001DL`: one 154-byte strip-based 2x1 mode-L TIFF carries IFD0
`ComponentsConfiguration` 37121 bytes `[1,2,3,0]` as an inline UNDEFINED
payload and `MakerNote` 37500 bytes `[77,75,0,255,1,2]` as an out-of-line
UNDEFINED payload, preserves pixels `[17,34]`, and leaves `Info["exif"]`
absent. Raw RED failed `Expected [1, 2, 3, 0], got []`; facade RED failed on
missing tag 37121. Raw/facade GREEN passes `1/1` (`109ms`) / `1/1` (`47ms`),
raw `open_tiff` passes `119/119` (`469ms`), facade `getexif` passes `112/112`
(`1093ms`), and full passes `2331/2331` (`26563ms`). Registrations are
`1147/1184`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`076456D7BE3BCC45989C6B1536C7E774A0A41048DE2335BAE8C63480564E15E5`.

Latest `META-001DK`: one 154-byte strip-based 2x1 mode-L TIFF carries IFD0
`OECF` 34856 bytes `[1,0,2,255,3]` as an out-of-line UNDEFINED payload and
`SpatialFrequencyResponse` 41484 bytes `[9,0,8]` as an inline UNDEFINED
payload, preserves pixels `[17,34]`, and leaves `Info["exif"]` absent. Raw RED
failed `Expected [1, 0, 2, 255, 3], got []`; facade RED failed on missing tag
34856. Raw/facade GREEN passes `1/1` (`141ms`) / `1/1` (`63ms`), raw
`open_tiff` passes `118/118` (`563ms`), facade `getexif` passes `111/111`
(`1078ms`), and full passes `2329/2329` (`25859ms`). Registrations are
`1146/1183`. Release x64 builds with zero warnings/errors; no export was added,
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`EC0733C628CD70DBE9F0CAEB4E184B0A212687E5FE74870DB24CD6CCB2D13D17`.

Latest `META-001DJ`: one 184-byte strip-based 2x1 mode-L TIFF carries IFD0
`StandardOutputSensitivity` 34865=`100001`, `RecommendedExposureIndex`
34866=`200002`, `ISOSpeed` 34867=`300003`, `ISOSpeedLatitudeyyy`
34868=`400004`, and `ISOSpeedLatitudezzz` 34869=`500005` as scalar LONG
entries, preserves pixels `[17,34]`, and leaves `Info["exif"]` absent. Raw RED
failed `Expected 100001, got -1`; facade RED failed on missing tag 34865.
Raw/facade GREEN passes `1/1` (`78ms`) / `1/1` (`63ms`), raw `open_tiff`
passes `117/117` (`422ms`), facade `getexif` passes `110/110` (`1016ms`), and
full passes `2327/2327` (`25250ms`). Registrations are `1145/1182`. Release
x64 builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`F3915887A3CC9C8E8E273FE0FF90A7BF7ED26B905221A6898183DF6FCA3B5926`.

Latest `META-001DI`: one 148-byte strip-based 2x1 mode-L TIFF carries IFD0
`ISOSpeedRatings` / `PhotographicSensitivity` 34855=`400` and
`SensitivityType` 34864=`3` as scalar SHORT entries, preserves pixels
`[17,34]`, and leaves `Info["exif"]` absent. Raw RED failed
`Expected 400, got -1`; facade RED failed on missing tag 34855. Raw/facade
GREEN passes `1/1` (`78ms`) / `1/1` (`47ms`), raw `open_tiff` passes
`116/116` (`406ms`), facade `getexif` passes `109/109` (`1125ms`), and full
passes `2325/2325` (`26219ms`). Registrations are `1144/1181`. Release x64
builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`51122D7BA4D7729761A67DA01661C902E201A7A8201B4364B95017B0A30DBF0A`.

Latest `META-001DH`: 136/142-byte strip-based 2x1 mode-L TIFF fixtures carry
IFD0 `SubjectArea` 37396=`[7,9]` and `[7,9,11]` as two- and three-element
SHORT entries, preserve pixels `[17,34]`, and leave `Info["exif"]` absent.
Raw RED failed `Expected [7, 9], got []`; raw GREEN passes `1/1` (`78ms`),
and the existing facade route passes its new public test directly `1/1`
(`47ms`). Raw `open_tiff` passes `115/115` (`250ms`), facade `getexif` passes
`108/108` (`672ms`), and full passes `2323/2323` (`15047ms`). Registrations
are `1143/1180`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`92B809C293235D145C7A4661FE9F522F7114D27BA9F0931399689871D9547FD1`.

Latest `META-001DG`: one 144-byte strip-based 2x1 mode-L TIFF carries IFD0
`SubjectArea` 37396=`[7,9,11,13]` as a four-element SHORT entry, preserves
pixels `[17,34]`, and leaves `Info["exif"]` absent. Raw RED failed
`Expected [7, 9, 11, 13], got []`; facade RED failed on missing tag 37396.
Raw/facade GREEN passes `1/1` (`125ms`) / `1/1` (`31ms`), raw `open_tiff`
passes `114/114` (`297ms`), facade `getexif` passes `107/107` (`579ms`), and
full passes `2321/2321` (`15500ms`). Registrations are `1142/1179`. Release
x64 builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`AE4857AB6077BD0A497A38BF610992EFF6F55C73D9704907CC3DDE7841A040CA`.

Latest `META-001DF`: one 162-byte strip-based 2x1 mode-L TIFF carries IFD0
`SecurityClassification` 37394=`secret` and `ImageHistory` 37395=`edited` as
NUL-terminated ASCII entries, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Raw RED failed `Expected "secret", got ""`; facade RED
failed on missing tag 37394. Raw/facade GREEN passes `1/1` (`78ms`) / `1/1`
(`31ms`), raw `open_tiff` passes `113/113` (`281ms`), facade `getexif` passes
`106/106` (`547ms`), and full passes `2319/2319` (`15266ms`). Registrations
are `1141/1178`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`E4F413F6B281C89446867AD175106DAA10E943BECE955A594BD67727EEB7E6F0`.

Latest `META-001DE`: one 143-byte strip-based 2x1 mode-L TIFF carries IFD0
`SpectralSensitivity` 34852=`spec42` as a NUL-terminated ASCII entry,
preserves pixels `[17,34]`, and leaves `Info["exif"]` absent. Raw RED failed
`Expected "spec42", got ""`; facade RED failed on missing tag 34852.
Raw/facade GREEN passes `1/1` (`47ms`) / `1/1` (`47ms`), raw `open_tiff`
passes `112/112` (`282ms`), facade `getexif` passes `105/105` (`578ms`), and
full passes `2317/2317` (`14969ms`). Registrations are `1140/1177`. Release
x64 builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`26FAE27A742E16D04A222082183F1BDC82A94122F6D6D6875F2127827426E75B`.

Latest `META-001DD`: one 136-byte strip-based 2x1 mode-L TIFF carries IFD0
`SubjectLocation` 41492=`[7,9]` as a two-element SHORT entry, preserves pixels
`[17,34]`, and leaves `Info["exif"]` absent. Raw RED failed
`Expected [7, 9], got []`; facade RED failed on missing tag 41492. Raw/facade
GREEN passes `1/1` (`47ms`) / `1/1` (`31ms`), raw `open_tiff` passes
`111/111` (`265ms`), facade `getexif` passes `104/104` (`641ms`), and full
passes `2315/2315` (`15187ms`). Registrations are `1139/1176`. Release x64
builds with zero warnings/errors; no export was added, source/DLL exports
remain `439/439`, zero difference; SHA-256 is
`8BF87B038AD9BD815697DBA1D0D982FA7A438A23B662E74C527ABDE17247F52F`.

Latest `META-001DC`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
`CompressedBitsPerPixel` 37122=`24/10` and `ExposureIndex` 41493=`200/1` as
scalar RATIONAL entries, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent. Raw RED failed `Expected [24, 10], got []`; facade RED
failed on missing tag 37122. Raw/facade GREEN passes `1/1` (`94ms`) / `1/1`
(`47ms`), raw `open_tiff` passes `110/110` (`312ms`), facade `getexif` passes
`103/103` (`578ms`), and full passes `2313/2313` (`15188ms`). Registrations
are `1138/1175`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`D8CBED6CF819880BD5026DEDD189764108E6C66063236E6F83E76695CEC6626B`.

Latest `META-001DB`: one 144-byte strip-based 2x1 mode-L TIFF carries IFD0
`ExposureBiasValue` 37380=`-1/2` as a scalar SRATIONAL entry, preserves pixels
`[17,34]`, and leaves `Info["exif"]` absent. Raw RED failed
`Expected [-1, 2], got []`; raw GREEN passes `1/1` (`62ms`), and the existing
facade enumeration passes the new public test directly `1/1` (`31ms`). Raw
`open_tiff` passes `109/109` (`250ms`), facade `getexif` passes `102/102`
(`594ms`), and full passes `2311/2311` (`15078ms`). Registrations are
`1137/1174`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`E5FB99F2E3763ABCEF004D9D20629763A9CB78411DD322DD396657CA9C2834BD`.

Latest `META-001DA`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
`ShutterSpeedValue` 37377=`-3/2` and `BrightnessValue` 37379=`7/4` as scalar
SRATIONAL entries. Raw RED failed `Expected [-3, 2], got []`; facade RED
failed on missing tag 37377. Final raw/facade passes `1/1` (`31ms`) / `1/1`
(`32ms`), raw `open_tiff` passes `108/108` (`282ms`), facade `getexif` passes
`101/101` (`547ms`), and final full passes `2309/2309` (`15250ms`). Registrations
are `1136/1173`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`EA91ABC0F5E4B96ACBC63661307B51C17EAD240785E12EAB390F912DB77D676A`.

Latest `META-001CZ`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
`SubjectDistance` 37382=`125/10` and `FocalLength` 37386=`50/1` as scalar
RATIONAL entries. Raw RED failed `Expected [125, 10], got []`; facade RED
failed on missing tag 37382. Raw/facade GREEN passes `1/1` (`79ms`) / `1/1`
(`47ms`), raw `open_tiff` passes `107/107` (`312ms`), facade `getexif` passes
`100/100` (`594ms`), and full passes `2307/2307` (`15203ms`). Registrations
are `1135/1172`. Release x64 builds with zero warnings/errors; no export was
added, source/DLL exports remain `439/439`, zero difference; SHA-256 is
`7478F8E56D39FF187E6FE1F916AD87DD45360E0BC60F440C90BA769907C59D21`.

Latest `META-001CY`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
`ApertureValue` 37378=`28/10` and `MaxApertureValue` 37381=`4/1` as scalar
RATIONAL entries. Raw RED failed `Expected [28, 10], got []`; facade RED
failed on missing tag 37378. Raw/facade GREEN passes `1/1` (`62ms`) / `1/1`
(`31ms`), raw `open_tiff` passes `106/106` (`250ms`), facade `getexif` passes
`99/99` (`547ms`), and full passes `2305/2305` (`14813ms`). Registrations are
`1134/1171`. Release x64 builds with zero warnings/errors; no export was added,
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`D59D2A116DDAAA21B90B24468BC0EA94D23D1D922959420BE5DC09A44D0CF34B`.

Latest `META-001CX`: one 164-byte strip-based 2x1 mode-L TIFF carries IFD0
`ExposureTime` 33434=`1/125` and `FNumber` 33437=`14/5` as scalar RATIONAL
entries. Raw RED failed `Expected [1, 125], got []`; facade RED failed on the
missing 33434 tag. Raw/facade GREEN passes `1/1` (`62ms`) / `1/1` (`31ms`),
raw `open_tiff` passes `105/105` (`250ms`), facade `getexif` passes `98/98`
(`484ms`), and full passes `2303/2303` (`14703ms`). Registrations are
`1133/1170`. Release x64 builds with zero warnings/errors; no export was added,
source/DLL exports remain `439/439`, zero difference; SHA-256 is
`59C182E4C7353D41113CECF25755DB07399431B1B5CEEF56ACC6FE2EACAB9B1F`.

Latest `META-003EN`: one native 12-slot batch exposes low-level sRGB
`intent_supported`, while `is_intent_supported` routes each explicit query to
the established scalar native ABI. Pillow 11.3.0 returns `(True, True, True)`
for intents `0..3` and integer `1` for every bounded method call on built-in
and serialized/reopened sRGB. Raw/facade REDs reported the missing export/
property; GREEN passes `1/1` (`46ms`) / `1/1` (`15ms`). ImageCms passes
`287/287` (`5640ms`), full passes `2301/2301` (`15516ms`), and registrations
are `1132/1169`. Release x64 builds with zero warnings/errors; exports are
`439/439`, zero difference; SHA-256 is
`A9FB9E89C4E684B6E3532D6863621961F0C7A5B95C2003F0C849B76AA459FDED`.

Latest `META-003EM`: one native 12-slot batch exposes low-level sRGB `clut`
for intents `0..3` in input/output/proof order. Pillow 11.3.0 returns
`(False, False, True)` for every intent on built-in and serialized/reopened
sRGB; the facade caches the exact map. Raw/facade REDs reported the missing
export/property; GREEN passes `1/1` (`93ms`) / `1/1` (`16ms`). ImageCms passes
`285/285` (`5625ms`), full passes `2299/2299` (`15422ms`), and registrations
are `1131/1168`. Release x64 builds with zero warnings/errors; exports are
`438/438`, zero difference; SHA-256 is
`4973418EFF0114EBF9C9E820472B4F80377FF3885622558EDC32E09E4806D42C`.

Latest `META-003EL`: one native named-color batch exposes low-level sRGB
`colorant_table` and `colorant_table_out`. Pillow 11.3.0 returns None for both
on built-in and serialized/reopened sRGB; independent presence/count/required
slots remain zero and the facade caches both properties. Raw/facade REDs
reported the missing export/property; GREEN passes `1/1` (`63ms`) / `1/1`
(`31ms`). ImageCms passes `283/283` (`5750ms`), full passes `2297/2297`
(`15484ms`), and registrations were `1130/1167`. Release x64 built with zero
warnings/errors; exports were `437/437`, zero difference; SHA-256 was
`3B44D76135E56D3E6C0A6B01809BD7C742A0C1B62168FF6A72D5B7B665DF6DDF`.

Latest `META-003EK`: one native batch exposes low-level sRGB `attributes` and
optional `colorimetric_intent`. Pillow 11.3.0 returns integer `0` and None on
built-in and serialized/reopened sRGB; the signature presence/value remains
zero without defaults and the facade caches both. Raw/facade REDs reported the
missing export/property; GREEN passes `1/1` (`62ms`) / `1/1` (`16ms`).
ImageCms passes `281/281` (`6015ms`), full passes `2295/2295` (`15438ms`), and
registrations are `1129/1166`. Release x64 builds with zero warnings/errors;
exports are `436/436`, zero difference; SHA-256 is
`8F89C79B39791134190F5C86279E3D3C3FCC26970F4C568156E9D4D86AD30654`.

Latest `META-003EJ`: one native batch exposes low-level sRGB
`icc_measurement_condition`, `icc_viewing_condition`, and `viewing_condition`.
Pillow 11.3.0 returns None for all three on built-in and serialized/reopened
sRGB; native presence/code/value/required slots remain zero and the facade
caches all properties without ICC parsing. Raw/facade REDs reported the missing
export/property; GREEN passes `1/1` (`93ms`) / `1/1` (`31ms`). ImageCms passes
`279/279` (`5688ms`), full passes `2293/2293` (`15188ms`), and registrations
are `1128/1165`. Release x64 builds with zero warnings/errors; exports are
`435/435`, zero difference; SHA-256 is
`DA35205BCAD86A9433557014EA99CAF0C1302DB700C35D21DA3B06CC0BB9F69C`.

Latest `META-003EI`: one native UTF-8 batch exposes low-level sRGB optional
`screening_description` and `target`. Pillow 11.3.0 returns None for both on
built-in and serialized/reopened sRGB, so independent presence/required slots
remain zero without defaults and the facade caches both properties. Raw/facade
REDs reported the missing export/property; GREEN passes `1/1` (`78ms`) / `1/1`
(`16ms`). ImageCms passes `277/277` (`5750ms`), full passes `2291/2291`
(`14984ms`), and registrations are `1127/1164`. Release x64 builds with zero
warnings/errors; exports are `434/434`, zero difference; SHA-256 is
`170B5D9A6147745601AB83E26F20E9F3822DDE3903E260BA8643F81CCA19D8DC`.

Latest `META-003EH`: one native batch exposes low-level sRGB optional
`perceptual_rendering_intent_gamut`, `saturation_rendering_intent_gamut`, and
`technology` signatures. Pillow 11.3.0 returns None for all three on built-in
and serialized/reopened sRGB, so presence/value slots remain independently
zero without defaults. Raw/facade REDs reported the missing export/property;
GREEN passes `1/1` (`46ms`) / `1/1` (`16ms`). ImageCms passes `275/275`
(`5656ms`), full passes `2289/2289` (`15031ms`), and registrations are
`1126/1163`. Release x64 builds with zero warnings/errors; exports are
`433/433`, zero difference; SHA-256 is
`87CA443DEC300C00391534CAA95F53F1A65FE786747995BDADC329B79827D9D2`.

Latest `META-003EG`: one native header batch exposes low-level sRGB
`creation_date`, `header_flags`, `header_manufacturer`, `header_model`, and
`profile_id`. A fixed modified serialized-sRGB header proves Pillow's direct
zero-based `tm_mon`, exact signatures/flags, and all 16 ID bytes after source
release. Raw/facade REDs reported the missing export/property; final GREEN
passes `1/1` (`16ms`) / `1/1` (`15ms`). ImageCms passes `273/273` (`5703ms`),
full passes `2287/2287` (`15484ms`), and registrations are `1125/1162`.
Release x64 builds with zero warnings/errors; exports are `432/432`, zero
difference; SHA-256 is
`7E06A77903BEC6A45B229400A9612CB44A09C67CA6E2FEEB142606AE5B5FBB8A`.

Latest `META-003EF`: native-backed low-level sRGB `chromaticity` returns the
Pillow-exact red/green/blue xyY triples, including the built-in-versus-reopened
s15Fixed16 precision split. Raw/facade REDs reported the missing export/
property; GREEN passes `1/1` (`94ms`) / `1/1` (`15ms`). ImageCms passes
`271/271` (`5797ms`), full passes `2285/2285` (`15156ms`), and registrations
are `1124/1161`. Release x64 builds with zero warnings/errors; exports are
`431/431`, zero difference; SHA-256 is
`BB429CE6CFC1C951945B792081A8AB161A0346A4F2CBA3F65543CAF3DACA7023`.

Latest `META-003EE`: one native batch exposes low-level sRGB
`media_black_point` and `luminance` with independent tag presence. Pillow
11.3.0 returns `None` for both properties on built-in and serialized/reopened
sRGB profiles, so the ABI returns two zero presence slots and zeroed values
without defaults. Raw/facade REDs reported the missing export/property; GREEN
passes `1/1` (`78ms`) / `1/1` (`15ms`). ImageCms passes `269/269` (`5625ms`),
full passes `2283/2283` (`15313ms`), and registrations are `1123/1160`.
Release x64 builds with zero warnings/errors; exports are `430/430`, zero
difference; SHA-256 is
`7022AA1168CF0D8EB2113A2C02EE43CAAE460DD2D22008AE22D1E29B5F3A5807`.

Latest `META-003ED`: one native double-precision transform batch exposes exact
low-level sRGB `red_primary`, `green_primary`, and `blue_primary` XYZ/xyY
values for built-in and serialized profiles after source-memory release. Raw/
facade REDs reported the missing export/property; GREEN passes `1/1` (`63ms`) /
`1/1` (`16ms`). ImageCms passes `267/267` (`5735ms`), full passes `2281/2281`
(`15015ms`), and registrations are `1122/1159`. Release x64 builds with zero
warnings/errors; exports are `429/429`, zero difference; SHA-256 is
`652D5D7587D39E13CBEBC8BFDEEDA70E9D79BC3884E16C61AAA788AEBA95C07B`.

Latest `META-003EC`: native-backed low-level sRGB `chromatic_adaptation`
preserves the exact built-in/serialized 3x3 XYZ `chad` matrix and row-wise
derived xyY matrix after source-memory release. Raw/facade REDs reported the
missing export/property; GREEN passes `1/1` (`47ms`) / `1/1` (`16ms`). ImageCms
passes `265/265` (`5547ms`), full passes `2279/2279` (`14969ms`), and
registrations are `1121/1158`. Release x64 builds with zero warnings/errors;
exports are `428/428`, zero difference; SHA-256 is
`145125212DD172068C201D93F0BA0D83F49A84CBC0A74D4005E2D6BFC1920AC7`.

Latest `META-003EB`: one native batch exposes Pillow-exact low-level sRGB
`red_colorant`, `green_colorant`, and `blue_colorant` XYZ/xyY values, including
the built-in-versus-serialized precision split after source-memory release.
Raw/facade REDs reported the missing export/property; GREEN passes `1/1`
(`94ms`) / `1/1` (`15ms`). ImageCms passes `263/263` (`5625ms`), full passes
`2277/2277` (`15250ms`), and registrations are `1120/1157`. Release x64 builds
with zero warnings/errors; exports are `427/427`, zero difference; SHA-256 is
`B9588372B3558E2AF7CAE9EBF259979914F15BDBCE6F9AA6876B5F951B26AE1A`.

Latest `META-003EA`: native-backed low-level sRGB
`media_white_point_temperature` preserves exact built-in `5000.726053819035`
and serialized/reopened `5000.722328847392` values through profile-memory
release. Raw/facade REDs reported the missing export/property; GREEN passes
`1/1` (`78ms`) / `1/1` (`16ms`). ImageCms passes `261/261` (`5828ms`), full
passes `2275/2275` (`15219ms`), and registrations are `1119/1156`. Release x64
builds with zero warnings/errors; exports are `426/426`, zero difference;
SHA-256 is
`8578C4AB19C614E11359F6295928FC1F1E217BA70EEA174F30E5B2384EB9363D`.

Latest `META-003DZ`: native-backed low-level sRGB `media_white_point` preserves
Pillow's distinct nominal built-in D50 and serialized ICC fixed-point XYZ/xyY
tuples. Raw RED reported the missing export; the first GREEN exposed the
provenance mismatch; final raw/facade pass `1/1` (`78ms`) / `1/1` (`31ms`).
ImageCms passes `259/259` (`5750ms`), full passes `2273/2273` (`15203ms`), and
registrations are `1118/1155`. Release x64 builds with zero warnings/errors;
exports are `425/425`, zero difference; SHA-256 is
`62C3DB2C5B6180784F31AC57735E36B6F5D7F07DC63ADC960D7B5F2C4DADC99D`.

Latest `META-003DY`: one native profile-header query exposes exact low-level
sRGB connection/device/color-space signatures, matrix-shaper state, and
floating/encoded ICC versions for built-in and memory-opened profiles. Raw RED
reported the missing export and GREEN passes `1/1` (`79ms`); facade RED reported
missing `connection_space`, and after correcting the first run's one-character
signature assembly, GREEN passes `1/1` (`31ms`). ImageCms passes `257/257`
(`5828ms`), full passes `2271/2271` (`15078ms`), and registrations are
`1117/1154`. Release x64 builds with zero warnings/errors; exports are `424/424`,
zero difference; SHA-256 is
`8E9219B6B39517D365F62C620BC1EE0382D33A0AF391DFD5C7BA4F97036DFA4B`.

Latest `META-003DX`: built-in and memory-opened sRGB low-level `CmsProfile`
objects expose exact description, copyright, absent manufacturer/model, and
rendering intent through established native queries after source release.
Facade RED reported missing `profile_description`; GREEN passes `1/1` (`31ms`),
related raw queries pass `6/6` (`32ms`), combined ImageCms passes `255/255`
(`5625ms`), and full passes `2269/2269` (`14797ms`). Registrations are
`1116/1153`; no native code or ABI changed, so exports remain `423/423` and
SHA-256 remains
`9BE524D2B9F1BF769310D8579557C0984DCE8794D13BD107225C03E854608ED1`.

Latest `META-003DW`: shared sRGB `ImageCmsProfile` wrappers expose Pillow's
public `tobytes()` name, return the exact 588-byte ICC (`0000024c` size prefix,
`acsp` at offset 36), and remain serializable after the source profile closes.
Facade RED reported the missing method; GREEN passes `1/1` (`15ms`), ImageCms
passes `254/254` (`5672ms`), and fresh full passes `2268/2268` (`15328ms`).
Registrations are `1116/1152`; no native code or ABI changed, so exports remain
`423/423` and SHA-256 remains
`9BE524D2B9F1BF769310D8579557C0984DCE8794D13BD107225C03E854608ED1`.

Latest `META-003DV`: existing sRGB `CmsProfile` input now returns an
`ImageCmsProfile` over the exact same native pointer, exposes the source as
`profile`, leaves `filename` as None, and supports either close order through
native atomic retain/release. Raw RED/GREEN passes `1/1` (`109ms`); facade
RED/GREEN passes `1/1` (`31ms`); reverse close order passes `1/1` (`16ms`);
ImageCms passes `253/253` (`5766ms`), and full passes `2267/2267` (`15093ms`).
Registrations are `1116/1151`; Release x64 builds with zero warnings/errors;
source/DLL exports are `423/423`, zero difference; SHA-256 is
`9BE524D2B9F1BF769310D8579557C0984DCE8794D13BD107225C03E854608ED1`.

Latest `META-003DU`: bounded file-like `ImageCms.getOpenProfile` reads the
remaining 588-byte AHK File stream to EOF without closing it, leaves public
`filename` as None, and keeps an independent native profile after stream close
and source deletion. Facade RED reported the rejected type; GREEN passes `1/1`
(`31ms`), ImageCms passes `250/250` (`5766ms`), and the full directory passes
`2264/2264` (`15187ms`). Registrations are `1115/1149`; no native code or ABI
changed, so exports remain `422/422` and SHA-256 remains
`C152B20A2FEA82ADE6FC0C107B0F1853EA55C9419E74C871103D61CE1B076298`.

Latest `META-003DT`: native-backed non-ASCII Windows path opening reads one
serialized sRGB ICC into native memory, leaves public `filename` as None,
permits immediate source deletion, and retains exact description/default
intent. Raw/facade REDs reported the missing wide export and incorrect ANSI
branch; GREEN passes `1/1` (`78ms`) / `1/1` (`31ms`), the path matrix passes
`4/4` (`47ms`), ImageCms passes `249/249` (`5594ms`), and the full directory
passes `2263/2263` (`15188ms`). Registrations are `1115/1148`; Release x64
builds with zero warnings/errors; source/DLL exports are `422/422` with zero
set difference; SHA-256 is
`C152B20A2FEA82ADE6FC0C107B0F1853EA55C9419E74C871103D61CE1B076298`.

Latest `META-003DS`: native-backed `ImageCms.getOpenProfile` accepts one
absolute ASCII path to a serialized 588-byte sRGB ICC file, preserves public
`filename`, returns exact `sRGB built-in\n`, holds Pillow-exact Windows deletion
locking while live, and releases it on close. Raw/facade REDs reported the
missing export/String route; GREEN passes `1/1` (`47ms`) / `1/1` (`16ms`),
ImageCms passes `247/247` (`5625ms`), and the full directory passes `2261/2261`
(`14922ms`). Registrations are `1114/1147`; Release x64 builds with zero
warnings/errors; source/DLL exports are `421/421` with zero set difference;
SHA-256 is
`CC4249651999235AD3D666341A32CAAAD6DD7010A504CCD6F8B1B220EAF0EF27`.

Latest `META-003DR`: bounded `ImageCms.getProfileDescription` returns exact
`sRGB built-in\n` for built-in and memory-opened sRGB profiles after serialized
Buffer release by reusing the existing native profile-description query. The
raw lifetime baseline passes `1/1` (`15ms`); facade RED reported the missing
method and GREEN passes `1/1` (`32ms`); ImageCms passes `245/245` (`5750ms`),
and the full directory passes `2259/2259` (`15640ms`). Registrations are
`1113/1146`; no native code or ABI changed, so source/DLL exports remain
`420/420` with zero set difference and SHA-256 remains
`36828F30514995CD98435BDD8C25DF67FEB7A94164377C7562C773ED56C14DEA`.

Latest `META-003DQ`: native-backed `ImageCms.getProfileModel` returns exact
`\n` for built-in and memory-opened sRGB profiles after serialized Buffer
release, matching Pillow's explicit `(model or "") + "\n"` behavior when the
optional tag is absent. Raw/facade REDs reported the missing export/method;
GREEN passes `1/1` (`62ms`) / `1/1` (`15ms`), ImageCms passes `244/244`
(`5750ms`), and the full directory passes `2258/2258` (`14938ms`).
Registrations are `1113/1145`; Release x64 builds with zero warnings/errors;
source/DLL exports are `420/420` with zero set difference; SHA-256 is
`36828F30514995CD98435BDD8C25DF67FEB7A94164377C7562C773ED56C14DEA`.

Latest `META-003DP`: native-backed `ImageCms.getProfileManufacturer` returns
exact `\n` for built-in and memory-opened sRGB profiles after serialized
Buffer release. Raw RED reported the missing export; the first GREEN exposed
that LittleCMS reports the legal optional manufacturer tag absent while
Pillow explicitly computes `(manufacturer or "") + "\n"`. The root fix and
facade route pass `1/1` (`47ms`) / `1/1` (`15ms`), ImageCms passes `242/242`
(`5765ms`), and the full directory passes `2256/2256` (`14937ms`).
Registrations are `1112/1144`; Release x64 builds with zero warnings/errors;
source/DLL exports are `419/419` with zero set difference; SHA-256 is
`773C252013B3DA54A342CBF84C70597072AD0341DA72335C4460B802092E48D7`.

Latest `META-003DO`: native-backed `ImageCms.getProfileCopyright` returns
exact `No copyright, use freely\n` for built-in and memory-opened sRGB
profiles after serialized Buffer release. Raw RED reported the missing
`pillow_c_cms_profile_copyright` export; facade RED reported the missing
`getProfileCopyright` method. GREEN passes `1/1` (`109ms`) / `1/1` (`32ms`),
ImageCms passes `240/240` (`5687ms`), and the full directory passes
`2254/2254` (`15062ms`). Registrations are `1111/1143`; Release x64 builds
with zero warnings/errors; source/DLL exports are `418/418` with zero set
difference; SHA-256 is
`F76BF5219E05A577EE696DF4EBEBD8A0353314208BA0ED41CEB5606C96D7A69F`.

Latest `META-003DN`: native-backed `ImageCms.getProfileInfo` returns exact
`sRGB built-in\r\n\r\nNo copyright, use freely\r\n\r\n` for built-in and
memory-opened sRGB profiles after serialized Buffer release. Raw/facade REDs
reported the missing export/method; GREEN passes `1/1` (`78ms`) / `1/1`
(`16ms`), ImageCms passes `238/238` (`5625ms`), and the full directory passes
`2252/2252` (`15281ms`). Registrations are `1110/1142`; Release x64 builds
with zero warnings/errors; source/DLL exports are `417/417` with zero set
difference; SHA-256 is
`570E2737B412E5351A6BAB7E7EBA3400EB60912BCABE3BD0DB5A556BDD964BA9`.

Latest `META-003DM`: native-backed `ImageCms.isIntentSupported` returns exact
Pillow value `1` for built-in and memory-opened sRGB profiles across intents
`0..3` and input/output/proof directions after source-memory release. Raw RED
reported the missing `pillow_c_cms_profile_intent_supported` export; facade RED
reported the missing `isIntentSupported` method. GREEN passes `1/1` (`78ms`) /
`1/1` (`16ms`), ImageCms passes `236/236` (`5531ms`), and the full directory
passes `2250/2250` (`15093ms`). Registrations are `1109/1141`; Release x64
builds with zero warnings/errors; source/DLL exports are `416/416` with zero
set difference; SHA-256 is
`D8CC164C7F1672B36DB0CB87DF80EB449E3C76576901119D40514D5CA7C12E06`.

Latest `META-003DL`: native-backed `ImageCms.getDefaultIntent` returns exact
intent `0` for built-in and memory-opened sRGB profiles after serialized Buffer
release. Raw RED reported the missing
`pillow_c_cms_profile_default_intent` export; facade RED reported the missing
`getDefaultIntent` method. GREEN passes `1/1` (`78ms`) / `1/1` (`15ms`),
ImageCms passes `234/234` (`5546ms`), and the full directory passes `2248/2248`
(`15343ms`). Registrations are `1108/1140`; Release x64 builds with zero
warnings/errors; source/DLL exports are `415/415` with zero set difference;
SHA-256 is
`E9EB12B3B9AD7418CA5F3381AD522850FAEE95B552AA8E2DC6E190F006969EA2`.

Latest `META-003DK`: reusable RGB/sRGB-to-LAB/LAB gamut checking now admits
absolute-colorimetric render intent `3` with proof intent `3` and flags
`0x5000`, completing gamut-check intents `0..3` for all four established RGB/
LAB proof mode pairs. Pillow 11.3.0 fixes the same exact 3x2/1x1 LAB bytes as
DH-DJ, unchanged RGB sources/Info, distinct 572-byte ICC Buffers, and profile-
memory-independent reuse. Raw/facade REDs reported `Expected 0, got -3` /
`cannot build proof transform`; GREEN passes `1/1` (`203ms`) / `1/1`
(`172ms`), ImageCms passes `232/232` (`5797ms`), and the full directory passes
`2246/2246` (`15250ms`). Registrations are `1107/1139`; Release x64 builds
with zero warnings/errors; source/DLL exports remain `414/414` with zero set
difference; SHA-256 is
`E0F0FBB92CA7F5DCDB102923E40391AC26D3A21A6808B1A4AD31F43787359FAC`.

Latest `META-003DJ`: reusable RGB/sRGB-to-LAB/LAB gamut checking now admits
saturation render intent `2` with proof intent `3` and flags `0x5000`. Pillow
11.3.0 fixes the same exact 3x2/1x1 LAB bytes as DH/DI, unchanged RGB sources/
Info, distinct 572-byte ICC Buffers, and profile-memory-independent reuse.
Raw/facade REDs reported `Expected 0, got -3` / `cannot build proof transform`;
GREEN passes `1/1` (`203ms`) / `1/1` (`156ms`), ImageCms passes `230/230`
(`5360ms`), and the full directory passes `2244/2244` (`14625ms`).
Registrations are `1106/1138`; Release x64 builds with zero warnings/errors;
source/DLL exports remain `414/414` with zero set difference; SHA-256 is
`20435AF3C33EADB46C32EE3E6AA5E9FB34CBDC49D3E492E29BA770D1B491DD64`.

Latest `META-003DI`: reusable RGB/sRGB-to-LAB/LAB gamut checking now admits
relative-colorimetric render intent `1` with proof intent `3` and flags
`0x5000`. Pillow 11.3.0 fixes the same exact 3x2/1x1 LAB bytes as DH,
unchanged RGB sources/Info, distinct 572-byte ICC Buffers, and profile-memory-
independent reuse. Raw/facade REDs reported `Expected 0, got -3` / `cannot
build proof transform`; GREEN passes `1/1` (`203ms`) / `1/1` (`125ms`),
ImageCms passes `228/228` (`5047ms`), and the full directory passes `2242/2242`
(`15015ms`). Registrations are `1105/1137`; Release x64 builds with zero
warnings/errors; source/DLL exports remain `414/414` with zero set difference;
SHA-256 is
`1D0A1D39AEE132C08C7394D66B7AE0F3C4582A3CCBA34F07A1C50A403DD56B55`.

Latest `META-003DH`: reusable RGB/sRGB-to-LAB/LAB gamut checking now admits
perceptual render intent `0` with proof intent `3` and flags `0x5000`. Pillow
11.3.0 fixes exact 3x2 LAB bytes
`[0,0,0,255,0,0,138,81,70,224,177,81,75,68,144,32,254,239]`, 1x1
`[32,254,239]`, unchanged RGB sources/Info, distinct 572-byte ICC Buffers, and
profile-memory-independent reuse. Raw/facade REDs reported
`Expected 0, got -3` / `cannot build proof transform`; GREEN passes `1/1`
(`188ms`) / `1/1`
(`156ms`), ImageCms passes `226/226` (`5016ms`), and the full directory passes
`2240/2240` (`14250ms`). Registrations are `1104/1136`; Release x64 builds
with zero warnings/errors; source/DLL exports remain `414/414` with zero set
difference; SHA-256 is
`25105F88F8CF5A7C652E7634F58382A60B434520F7D54D9569CD6CDFC2CFB352`.

Latest `META-003DG`: reusable RGB/sRGB-to-RGB/sRGB gamut checking now admits
absolute-colorimetric render intent `3` with proof intent `3` and flags
`0x5000`, completing intents `0..3` for this pair. Pillow 11.3.0 fixes exact
identity 3x2/1x1 RGB bytes, unchanged sources/Info, distinct 588-byte ICC
Buffers, and profile-memory-independent reuse. Raw/facade REDs reported
`Expected 0, got -3` / `cannot build proof transform`; GREEN passes `1/1`
(`203ms`) / `1/1` (`172ms`), ImageCms passes `224/224` (`4578ms`), and the
full directory passes `2238/2238` (`14062ms`). Registrations are `1103/1135`;
Release x64 builds with zero warnings/errors; source/DLL exports remain
`414/414` with zero set difference; SHA-256 is
`756A717649DE278C54C4B52D94052ED6797C37A0C9746BDF2F91038A281EED2D`.

Latest `META-003DF`: reusable RGB/sRGB-to-RGB/sRGB gamut checking now admits
saturation render intent `2` with proof intent `3` and flags `0x5000`. Pillow
11.3.0 fixes exact identity 3x2/1x1 RGB bytes, unchanged sources/Info,
distinct 588-byte ICC Buffers, and profile-memory-independent reuse. Raw/
facade REDs reported `Expected 0, got -3` / `cannot build proof transform`;
GREEN passes `1/1` (`188ms`) / `1/1` (`187ms`), ImageCms passes `222/222`
(`4328ms`), and the full directory passes `2236/2236` (`13843ms`).
Registrations are `1102/1134`; Release x64 builds with zero warnings/errors;
source/DLL exports remain `414/414` with zero set difference; SHA-256 is
`B35D3CE8A4B91A621CC9D4F8B1DA2A10FFA753C082A8B41208A4A10F69F5DEC0`.

Latest `META-003DE`: reusable RGB/sRGB-to-RGB/sRGB gamut checking now admits
relative-colorimetric render intent `1` with proof intent `3` and flags
`0x5000`. Pillow 11.3.0 fixes the same exact identity 3x2/1x1 RGB bytes as DD,
unchanged sources/Info, distinct 588-byte ICC Buffers, and profile-memory-
independent reuse. Raw/facade REDs reported `Expected 0, got -3` / `cannot
build proof transform`; GREEN passes `1/1` (`219ms`) / `1/1` (`157ms`),
ImageCms passes `220/220` (`4016ms`), and the full directory passes `2234/2234`
(`12922ms`). Registrations are `1101/1133`; Release x64 builds with zero
warnings/errors; source/DLL exports remain `414/414` with zero set difference;
SHA-256 is
`96A878B9B180D44BD5F0FEC273AC28C34AE0CE1DCE39665CD71964F9F65F6D51`.

Latest `META-003DD`: reusable RGB/sRGB-to-RGB/sRGB gamut checking now admits
perceptual render intent `0` with proof intent `3` and flags `0x5000`. Pillow
11.3.0 fixes exact identity 3x2/1x1 RGB bytes, unchanged sources/Info, distinct
588-byte ICC Buffers, and profile-memory-independent reuse. Raw/facade REDs
reported `Expected 0, got -3` / `cannot build proof transform`; GREEN passes
`1/1` (`234ms`) / `1/1` (`172ms`), ImageCms passes `218/218` (`3546ms`), and
the full directory passes `2232/2232` (`13156ms`). Registrations are
`1100/1132`; Release x64 builds with zero warnings/errors; source/DLL exports
remain `414/414` with zero set difference; SHA-256 is
`9BFDDFEF8A8360E82619452DB3532D97B17DB1F8679DB845DE617EA06B117530`.

Latest `META-003DC`: reusable D50-to-6500K LAB/LAB gamut checking now admits
absolute-colorimetric render intent `3` with proof intent `3` and flags
`0x5000`, completing intents `0..3` for both established LAB-input mode pairs.
Pillow 11.3.0 fixes the same exact 3x2 LAB bytes as CV/DA/DB, unchanged 1x1
`[32,254,239]`, unchanged LAB sources/Info, distinct 572-byte ICC Buffers, and
profile-memory-independent reuse. Raw/facade REDs reported `Expected 0, got
-3` / `cannot build proof transform`; GREEN passes `1/1` (`171ms`) / `1/1`
(`140ms`), ImageCms passes `216/216` (`3203ms`), and the full directory passes
`2230/2230` (`12688ms`). Registrations are `1099/1131`; Release x64 builds
with zero warnings/errors; source/DLL exports remain `414/414` with zero set
difference; SHA-256 is
`19BCE546A1986595033EE82F4A883E47FD7C4C2B5F3F8184E5ABCAFEA3A397E9`.

Latest `META-003DB`: reusable D50-to-6500K LAB/LAB gamut checking now admits
saturation render intent `2` with proof intent `3` and flags `0x5000`. Pillow
11.3.0 fixes the same exact 3x2 LAB bytes as CV/DA, unchanged 1x1
`[32,254,239]`, unchanged LAB sources/Info, distinct 572-byte ICC Buffers, and
profile-memory-independent reuse. Raw/facade REDs reported `Expected 0, got
-3` / `cannot build proof transform`; GREEN passes `1/1` (`156ms`) / `1/1`
(`109ms`), ImageCms passes `214/214` (`2906ms`), and the full directory passes
`2228/2228` (`12141ms`). Registrations are `1098/1130`; Release x64 builds
with zero warnings/errors; source/DLL exports remain `414/414` with zero set
difference; SHA-256 is
`5FA6EC3BFE4C6E4DD59BAA65AEF1BF8E43C1E1B152595179E4C18B7554DA7F82`.

Latest `META-003DA`: reusable D50-to-6500K LAB/LAB gamut checking now admits
relative-colorimetric render intent `1` with proof intent `3` and flags
`0x5000`. Pillow 11.3.0 fixes exact 3x2 LAB bytes
`[0,0,0,255,0,0,127,255,255,127,255,255,75,68,144,32,254,239]`, unchanged
1x1 `[32,254,239]`, unchanged LAB sources/Info, distinct 572-byte ICC Buffers,
and profile-memory-independent reuse. Raw/facade REDs reported `Expected 0,
got -3` / `cannot build proof transform`; GREEN passes `1/1` (`187ms`) / `1/1`
(`109ms`), ImageCms passes `212/212` (`2719ms`), and the full directory passes
`2226/2226` (`11937ms`). Registrations are `1097/1129`; Release x64 builds
with zero warnings/errors; source/DLL exports remain `414/414` with zero set
difference; SHA-256 is
`DB35967AC2A04168B45685753BABA8146B45F82AE8A0372339AD7E07F9C57E6C`.

Latest `META-003CZ`: reusable LAB/LAB-to-RGB/sRGB gamut checking now admits
absolute-colorimetric render intent `3` with proof intent `3` and flags
`0x5000`, completing intents `0..3` for this mode pair. Pillow 11.3.0 fixes
exact 3x2 RGB bytes
`[1,0,1,254,255,254,127,127,127,127,127,127,22,7,252,15,34,56]`, 1x1
`[15,34,56]`, unchanged LAB sources/Info, distinct 588-byte ICC Buffers, and
profile-memory-independent reuse. Raw/facade REDs reported `Expected 0, got
-3` / `cannot build proof transform`; GREEN passes `1/1` (`157ms`) / `1/1`
(`109ms`), ImageCms passes `210/210` (`2594ms`), and the full directory passes
`2224/2224` (`11828ms`). Registrations are `1096/1128`; Release x64 builds
with zero warnings/errors; source/DLL exports remain `414/414` with zero set
difference; SHA-256 is
`94CA5DF10ADC9826825EF13E44642553E3C6C675146774A095188C4FCEB0AD7B`.

Latest `META-003CY`: reusable LAB/LAB-to-RGB/sRGB gamut checking now admits
saturation render intent `2` with proof intent `3` and flags `0x5000`.
Pillow 11.3.0 fixes exact 3x2 RGB bytes
`[1,0,1,254,255,254,127,127,127,127,127,127,22,7,252,15,34,56]`, 1x1
`[15,34,56]`, unchanged LAB sources/Info, distinct 588-byte ICC Buffers, and
profile-memory-independent reuse. Raw/facade REDs reported `Expected 0, got
-3` / `cannot build proof transform`; GREEN passes `1/1` (`140ms`) / `1/1`
(`109ms`), ImageCms passes `208/208` (`2297ms`), and the full directory passes
`2222/2222` (`11891ms`). Registrations are `1095/1127`; Release x64 builds
with zero warnings/errors; source/DLL exports remain `414/414`; SHA-256 is
`391CFC8C6EC50CC4F7DB0331D1A0E3D9E268F9B21A8FA699E8E35D8BD1B63848`.

Latest `META-003CX`: reusable LAB/LAB-to-RGB/sRGB gamut checking now admits
relative-colorimetric render intent `1` with proof intent `3` and flags
`0x5000`. Pillow 11.3.0 fixes the same exact 3x2 RGB bytes
`[1,0,1,254,255,254,127,127,127,127,127,127,22,7,252,15,34,56]`, 1x1
`[15,34,56]`, unchanged LAB sources/Info, distinct 588-byte ICC Buffers, and
profile-memory-independent reuse. Raw/facade REDs reported `Expected 0, got
-3` / `cannot build proof transform`; GREEN passes `1/1` (`141ms`) / `1/1`
(`93ms`), ImageCms passes `206/206` (`2250ms`), and the full directory passes
`2220/2220` (`11390ms`). Registrations are `1094/1126`; Release x64 builds
with zero warnings/errors; source/DLL exports remain `414/414`; SHA-256 is
`41A9469CDFEA84B830B39D8E7443A345EBF2F9BEF6518C2B157F748AA4C7B58E`.

Latest `META-003CW`: reusable `buildProofTransform` now supports LAB/LAB-to-
RGB/sRGB gamut checking at render intent `0`, proof intent `3`, and flags
`0x5000`. Pillow 11.3.0 fixes exact 3x2 output bytes
`[1,0,1,254,255,254,127,127,127,127,127,127,22,7,252,15,34,56]`, 1x1
`[15,34,56]`, unchanged LAB sources/Info, distinct 588-byte `acsp` ICC
Buffers, and profile-memory-independent reuse. Raw/facade REDs reported
`Expected 0, got -3` / `cannot build proof transform`; GREEN passes `1/1`
(`188ms`) / `1/1` (`109ms`), ImageCms passes `204/204` (`1891ms`), and the
full directory passes `2218/2218` (`11125ms`). Registrations are `1093/1125`;
Release x64 builds with zero warnings/errors; source/DLL exports remain
`414/414`; SHA-256 is
`45E74FB5FBF11D8C05212FA14F904CDE01E730D2A531264F7FEAFE46D426BA8C`.

Latest `META-003CV`: reusable `buildProofTransform` now supports the bounded
D50-to-6500K LAB/LAB gamut-check route with render intent `0`, proof intent
`3`, and `SOFTPROOFING|GAMUTCHECK=20480` (`0x5000`). Pillow 11.3.0 fixes exact
3x2 output bytes
`[0,0,0,255,0,0,127,255,255,127,255,255,75,68,144,32,254,239]`, keeps the
1x1 `[32,254,239]` sample unchanged, leaves source pixels and Info untouched,
attaches distinct 572-byte `acsp` ICC Buffers, and keeps the transform usable
after all profiles and serialized profile memory are released. Raw/facade REDs
reported `Expected 0, got -3` / `cannot build proof transform`; GREEN passes
`1/1` (`203ms`) / `1/1` (`157ms`), ImageCms passes `202/202` (`1671ms`), and
the full directory passes `2216/2216` (`11328ms`). Registrations are
`1092/1124`; Release x64 builds with zero warnings/errors; source/DLL exports
remain `414/414` with zero set difference; SHA-256 is
`CE7DC2A13AEC9F369E902A5E15C7D0099ADDD1155840467DB0639F8FF18444DA`.

Latest `META-003CU`: reusable `buildProofTransform` now supports D50-to-6500K
LAB/LAB absolute render intent `3` with an sRGB proof profile, proof intent
`3`, and `SOFTPROOFING=16384`, completing render intents `0..3` across all four
established RGB/LAB pairs. Pillow 11.3.0 fixes exact identity repeat-apply
3x2/1x1 LAB pixels, unchanged sources/Info, distinct 572-byte `acsp` ICC
Buffers, and profile-memory-independent reuse. Raw/facade REDs returned `-3` /
`cannot build proof transform`; GREEN passes `1/1` (`46ms`) / `1/1` (`15ms`),
ImageCms passes `200/200` (`1484ms`), and full passes `2214/2214` (`11328ms`).
Registrations are `1091/1123`; Release x64 builds with zero warnings/errors;
exports remain `414/414` with zero set difference; SHA-256 is
`6586FA42C426B355D16F1C4B9E979699D50F54797AD257C1A873C4498F907DF8`.

Latest `META-003CT`: reusable `buildProofTransform` now supports LAB/LAB-to-
RGB/sRGB absolute render intent `3` with an sRGB proof profile, proof intent
`3`, and `SOFTPROOFING=16384`. Pillow 11.3.0 fixes exact repeat-apply 3x2/1x1
RGB pixels, unchanged sources/Info, distinct 588-byte `acsp` ICC Buffers, and
profile-memory-independent reuse. Raw/facade REDs returned `-3` / `cannot
build proof transform`; GREEN passes `1/1` (`78ms`) / `1/1` (`16ms`),
ImageCms passes `198/198` (`1406ms`), and full passes `2212/2212` (`11125ms`).
Registrations are `1090/1122`; Release x64 builds with zero warnings/errors;
exports remain `414/414` with zero set difference; SHA-256 is
`7887B4EC37AA5E41B10925EB2A2E051308FA7D578568CE9ACDD9EF09F93040D4`.

Latest `META-003CS`: reusable `buildProofTransform` now supports RGB/sRGB-to-
LAB/LAB absolute render intent `3` with an sRGB proof profile, proof intent
`3`, and `SOFTPROOFING=16384`. Pillow 11.3.0 fixes exact repeat-apply 3x2/1x1
LAB pixels, unchanged sources/Info, distinct 572-byte `acsp` ICC Buffers, and
profile-memory-independent reuse. Raw/facade REDs returned `-3` / `cannot
build proof transform`; GREEN passes `1/1` (`78ms`) / `1/1` (`15ms`),
ImageCms passes `196/196` (`1531ms`), and full passes `2210/2210` (`10938ms`).
Registrations are `1089/1121`; Release x64 builds with zero warnings/errors;
exports remain `414/414` with zero set difference; SHA-256 is
`CB366AA17FD9DC6385661F0455EFA489767DAD239F8CC78BABA42E5EDBCBB593`.

Latest `META-003CR`: reusable `buildProofTransform` now supports RGB/sRGB-to-
RGB/sRGB absolute render intent `3` with proof intent `3` and
`SOFTPROOFING=16384`. Pillow 11.3.0 fixes exact identity repeat-apply 3x2/1x1
pixels, unchanged sources/Info, distinct 588-byte `acsp` ICC Buffers, and
profile-memory-independent reuse. Raw/facade REDs returned `-3` / `cannot
build proof transform`; GREEN passes `1/1` (`78ms`) / `1/1` (`47ms`),
ImageCms passes `194/194` (`1359ms`), and full passes `2208/2208` (`11000ms`).
Registrations are `1088/1120`; Release x64 builds with zero warnings/errors;
exports remain `414/414` with zero set difference; SHA-256 is
`3322780DFEDB6FCD50BC3C3C15C79D84F3355D77920B07D5412C717826FD6BA7`.

Latest `META-003CN` through `META-003CQ`: reusable `buildProofTransform`
supports saturation render intent `2` across all four established RGB/LAB
mode pairs while retaining proof intent `3` and `SOFTPROOFING=16384`. Pillow
11.3.0 probes fix exact repeat-apply 3x2/1x1 bytes, unchanged sources/Info,
distinct 572-byte LAB or 588-byte RGB `acsp` ICC Buffers, and profile-memory-
independent reuse. Raw/facade REDs returned `-3` / `cannot build proof
transform`. CN GREEN passes `1/1` (`187ms`) / `1/1` (`93ms`), ImageCms
`186/186` (`2250ms`), full `2200/2200` (`11172ms`). CO passes `109ms` /
`31ms`, ImageCms `188/188` (`1281ms`), full `2202/2202` (`11266ms`). CP
passes `63ms` / `31ms`, ImageCms `190/190` (`1250ms`), full `2204/2204`
(`11125ms`). CQ passes `63ms` / `31ms`, ImageCms `192/192` (`1391ms`), and
full `2206/2206` (`11047ms`). Registrations are `1087/1119`; Release x64
builds with zero warnings/errors; exports remain `414/414` with zero set
difference; SHA-256 is
`20D07E683F8645A4290405AF68EFE11A66E2100D628A6F17F9F1941030DDFB64`.

Latest `META-003CJ` through `META-003CM`: reusable `buildProofTransform`
supports relative-colorimetric render intent `1` across all four established
RGB/LAB mode pairs while retaining proof intent `3` and
`SOFTPROOFING=16384`. Bounded Pillow 11.3.0 probes fix exact repeat-apply
3x2/1x1 bytes, unchanged sources/Info, distinct 572-byte LAB or 588-byte RGB
`acsp` ICC Buffers, and profile-memory-independent reuse. Every raw/facade RED
failed at the intended `-3` / `cannot build proof transform` boundary. CJ
GREEN passes `1/1` (`141ms`) / `1/1` (`110ms`), ImageCms `178/178`
(`1843ms`), full `2192/2192` (`17516ms`). CK passes `125ms` / `46ms`,
ImageCms `180/180` (`2015ms`), full `2194/2194` (`17235ms`). CL passes `93ms`
/ `47ms`, ImageCms `182/182` (`2000ms`), full `2196/2196` (`17296ms`). CM
passes `94ms` / `47ms`, ImageCms `184/184` (`2079ms`), and full `2198/2198`
(`17110ms`). Registrations are `1083/1115`; Release x64 builds with zero
warnings/errors; exports remain `414/414` with zero set difference; SHA-256 is
`0978F263221D2F70523AFA3DED47144779E777D11B65FFAAA81E845699E0CB14`.

Latest `META-003CI`: D50-to-6500K LAB/LAB `buildProofTransform` completes the
default soft-proof matrix across all four established RGB/LAB mode pairs with
an sRGB proof profile, render intent `0`, proof intent `3`, and
`SOFTPROOFING=16384`. Pillow 11.3.0 fixes exact identity 3x2/1x1 LAB bytes,
unchanged sources/Info, distinct 572-byte `acsp` ICC Buffers, and
profile-memory-independent reuse. Raw/facade RED returned `-3` / `cannot build
proof transform`; GREEN pass `1/1` (`78ms`) / `1/1` (`78ms`), ImageCms passes
`176/176` (`1719ms`), and full passes `2190/2190` (`16718ms`). During GREEN,
repeated single-file raw runs exposed AHK's implicit DLL unload between
path-qualified DllCalls; caching one `LoadLibraryW` handle in
`PillowCDllPath()` fixed the real opaque-handle lifetime boundary. Ten fresh
AHK processes then passed `10/10`; native DLL drivers passed 100 iterations in
one process and 100 fresh processes. Registrations are `1079/1111`; Release
x64 builds with zero warnings/errors; exports remain `414/414` with zero set
difference; SHA-256 is
`A4281086CCFD2529A449356CA17DCB90AE6C6EACE428ECF86E2E656C950067AE`.

Latest `META-003CH`: reusable LAB/LAB-to-RGB/sRGB soft-proofing uses the same
default proof settings, produces Pillow's exact 3x2/1x1 RGB bytes, attaches
distinct 588-byte ICC Buffers, and remains valid after profile and serialized
memory release. Raw/facade GREEN pass `1/1` (`266ms`)/`1/1` (`47ms`),
ImageCms passes `174/174` (`1594ms`), and full passes `2188/2188` (`16797ms`).

Latest `META-003CG`: reusable RGB/sRGB-to-LAB/LAB soft-proofing produces
Pillow's exact 3x2/1x1 LAB bytes, attaches distinct 572-byte ICC Buffers, and
keeps source pixels/Info unchanged after profile release. Raw/facade GREEN
pass `1/1` (`360ms`)/`1/1` (`63ms`), ImageCms passes `172/172` (`1609ms`),
and full passes `2186/2186` (`17843ms`).

Latest `META-003CF`: bounded RGB `ImageCms.buildProofTransform` uses sRGB
input/output/proof profiles with default render intent `0`, proof intent `3`,
and `SOFTPROOFING=16384`. A Pillow 11.3.0 oracle fixes exact repeat-apply
3x2/1x1 RGB bytes, unchanged sources and caller Info, distinct 588-byte `acsp`
result ICC Buffers, and transform lifetime independent of all three profiles
and serialized profile memory. Raw RED reported the nonexistent
`pillow_c_cms_proof_transform_build`; facade RED reported missing
`buildProofTransform`. Raw/facade GREEN pass `1/1` (`265ms`)/`1/1` (`109ms`),
ImageCms targeted passes `170/170` (`1610ms`), and full `2184/2184` passes
(`19266ms`). Registrations are `1076/1108`; Release x64 builds with zero
warnings/errors; exports are `414/414` with zero set difference; SHA-256 is
`994C4E68A62F2D4D1585ED8A2D0BC616A105848329839032F2FE4F2C232D0822`.

Latest `META-003CE`: D50-to-6500K LAB/LAB in-place profileToProfile accepts
absolute-colorimetric intent `3` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`), completing intents `0..3` plus BPC across both
established same-mode pairs. A bounded Pillow 11.3.0 oracle fixes `None`
return, exact identity 3x2 LAB bytes, retained caller sentinel, one fresh
572-byte `acsp` output ICC blob, and profile-independent image lifetime. Raw
RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`141ms`)/`1/1` (`31ms`), ImageCms targeted passes `168/168`
(`984ms`), and full `2182/2182` passes (`16875ms`). Registrations are
`1075/1107`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`A6FC4F9CDD52D3683A8B84CAE1C0BD7B00CAE900E1AEAB40F81758A9DC4CA428`.

Latest `META-003CD`: RGB/sRGB-to-RGB/memory-opened-sRGB in-place
profileToProfile accepts absolute-colorimetric intent `3` with Pillow's bounded
black-point-compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle
fixes `None` return, exact identity 3x2 RGB bytes, retained caller sentinel, one
fresh 588-byte `acsp` output ICC blob, and profile-memory/profile-independent
image lifetime. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`172ms`)/`1/1` (`47ms`), ImageCms
targeted passes `166/166` (`1625ms`), and full `2180/2180` passes (`10672ms`).
Registrations are `1074/1106`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`1942EE638692A297ECD0B7DDDC0EFFEEF51E5421AEC81BFB46E49F78833AA75D`.

Latest `META-003CC`: D50-to-6500K LAB/LAB in-place profileToProfile accepts
saturation intent `2` with Pillow's bounded black-point-compensation flag
`8192` (`0x2000`), completing this exact route across both established same-
mode pairs. A bounded Pillow 11.3.0 oracle fixes `None` return, exact identity
3x2 LAB bytes, retained caller sentinel, one fresh 572-byte `acsp` output ICC
blob, and profile-independent image lifetime. Raw RED returned `-3`; facade
RED raised `cannot build transform`. Raw/facade GREEN pass `1/1` (`141ms`)/
`1/1` (`31ms`), ImageCms targeted passes `164/164` (`1406ms`), and full
`2178/2178` passes (`17859ms`). Registrations are `1073/1105`; Release x64
builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`DA44080F6FA2F1B407B251F2474D411222F7B170A58C5D4C8B7D95C8305E6402`.

Latest `META-003CB`: RGB/sRGB-to-RGB/memory-opened-sRGB in-place
profileToProfile accepts saturation intent `2` with Pillow's bounded black-
point-compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes
`None` return, exact identity 3x2 RGB bytes, retained caller sentinel, one fresh
588-byte `acsp` output ICC blob, and profile-memory/profile-independent image
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`141ms`)/`1/1` (`16ms`), ImageCms targeted passes
`162/162` (`1328ms`), and full `2176/2176` passes (`17156ms`). Registrations are
`1072/1104`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`024A970B5AB947E6B0143ECD1FA6ADC270D864B0ED7E711F69F6348E020BE359`.

Latest `META-003CA`: D50-to-6500K LAB/LAB in-place profileToProfile accepts
relative-colorimetric intent `1` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`), completing this exact route across both established
same-mode pairs. A bounded Pillow 11.3.0 oracle fixes `None` return, exact
identity 3x2 LAB bytes, retained caller sentinel, one fresh 572-byte `acsp`
output ICC blob, and profile-independent image lifetime. Raw RED returned
`-3`; facade RED raised `cannot build transform`. Raw/facade GREEN pass `1/1`
(`157ms`)/`1/1` (`31ms`), ImageCms targeted passes `160/160` (`1421ms`), and
full `2174/2174` passes (`23609ms`). Registrations are `1071/1103`; Release x64
builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`4D419E74375E79C5BF7BCA98BFCC2F4280B088037CD48A10FF77AEB1023B9216`.

Latest `META-003BZ`: RGB/sRGB-to-RGB/memory-opened-sRGB in-place
profileToProfile accepts relative-colorimetric intent `1` with Pillow's bounded
black-point-compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle
fixes `None` return, exact identity 3x2 RGB bytes, retained caller sentinel, one
fresh 588-byte `acsp` output ICC blob, and profile-memory/profile-independent
image lifetime. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`218ms`)/`1/1` (`31ms`), ImageCms
targeted passes `158/158` (`1531ms`), and full `2172/2172` passes (`18750ms`).
Registrations are `1070/1102`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`A3B84337DADD346ED2FB3EA60186F831D2D86F190304F52CA79AA713DB5E8E0F`.

Latest `META-003BY`: D50-to-6500K LAB/LAB in-place profileToProfile accepts
perceptual intent `0` with Pillow's bounded black-point-compensation flag
`8192` (`0x2000`), completing this exact BPC route across both established
same-mode pairs. A bounded Pillow 11.3.0 oracle fixes `None` return, exact
identity 3x2 LAB bytes, retained caller sentinel, one fresh 572-byte `acsp`
output ICC blob, and profile-independent image lifetime. Raw RED returned
`-3`; facade RED raised `cannot build transform`. Raw/facade GREEN pass `1/1`
(`188ms`)/`1/1` (`15ms`), ImageCms targeted passes `156/156` (`1328ms`), and
full `2170/2170` passes (`19250ms`). Registrations are `1069/1101`; Release x64
builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`1B89C158A8347B2365CBEA2995D15D6AFC39EDB54F8D610034C5811D28815728`.

Latest `META-003BX`: RGB/sRGB-to-RGB/memory-opened-sRGB in-place
profileToProfile accepts perceptual intent `0` with Pillow's bounded black-
point-compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes
`None` return, exact identity 3x2 RGB bytes, retained caller sentinel, one fresh
588-byte `acsp` output ICC blob, and profile-memory/profile-independent image
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`219ms`)/`1/1` (`15ms`), ImageCms targeted passes
`154/154` (`891ms`), and full `2168/2168` passes (`12750ms`). Registrations are
`1068/1100`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`039166C0E006A238013FB551F1A9E226F6EF7B8C403FBF2395AEC31AA434C595`.

Latest `META-003BW`: allocating one-shot D50-to-6500K LAB/LAB profileToProfile
accepts absolute-colorimetric intent `3` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`), completing intents `0..3` plus this BPC
flag across all four established one-shot pairs. A bounded Pillow 11.3.0 oracle
fixes exact identity 3x2 LAB bytes, unchanged LAB source pixels and caller Info,
one fresh 572-byte `acsp` output ICC blob, and profile-independent result
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`141ms`)/`1/1` (`16ms`), ImageCms targeted passes
`152/152` (`781ms`), and full `2166/2166` passes (`11265ms`). Registrations are
`1067/1099`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`E3E9C988F5A82E80339E7716C502DC97B932CFFEEB15337B0D2DE59C09FD7722`.

Latest `META-003BV`: allocating one-shot RGB/sRGB-to-RGB/memory-opened-sRGB
profileToProfile accepts absolute-colorimetric intent `3` with Pillow's bounded
black-point-compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle
fixes exact identity 3x2 RGB bytes, unchanged RGB source pixels and caller Info,
one fresh 588-byte `acsp` output ICC blob, and profile-memory/profile-independent
result lifetime. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`141ms`)/`1/1` (`47ms`), ImageCms
targeted passes `150/150` (`1343ms`), and full `2164/2164` passes (`16594ms`).
Registrations are `1066/1098`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`6F254FC54336DF10867E6E18BD6EB363A73A0D905272C088547070B0C9AAC034`.

Latest `META-003BU`: allocating one-shot LAB/LAB-to-RGB/sRGB profileToProfile
accepts absolute-colorimetric intent `3` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`), completing both mode-changing pairs for
this exact one-shot route. A bounded Pillow 11.3.0 oracle fixes exact 3x2 RGB
bytes, unchanged LAB source pixels and caller Info, one fresh 588-byte `acsp`
output ICC blob, and profile-independent result lifetime. Raw RED returned
`-3`; facade RED raised `cannot build transform`. Raw/facade GREEN pass `1/1`
(`141ms`)/`1/1` (`47ms`), ImageCms targeted passes `148/148` (`703ms`), and
full `2162/2162` passes (`9938ms`). Registrations are `1065/1097`; Release x64
builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`B78A0BAF25C4E555EC6148CBCEC816EB684910F4AB1CAB923B9F73503B0EBDD4`.

Latest `META-003BT`: allocating one-shot RGB/sRGB-to-LAB/LAB profileToProfile
accepts absolute-colorimetric intent `3` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact
3x2 LAB bytes, unchanged RGB source pixels and caller Info, one fresh 572-byte
`acsp` output ICC blob, and profile-independent result lifetime. Raw RED
returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`94ms`)/`1/1` (`32ms`), ImageCms targeted passes `146/146`
(`703ms`), and full `2160/2160` passes (`10156ms`). Registrations are
`1064/1096`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`13713869393CEE107FD9597B9BFCB55159CA17461C6A0CD62E64976523278A6A`.

Latest `META-003BS`: allocating one-shot D50-to-6500K LAB/LAB profileToProfile
accepts saturation intent `2` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`), completing this exact route across all four established
one-shot pairs. A bounded Pillow 11.3.0 oracle fixes exact identity 3x2 LAB
bytes, unchanged LAB source pixels and caller Info, one fresh 572-byte `acsp`
output ICC blob, and profile-independent result lifetime. Raw RED returned
`-3`; facade RED raised `cannot build transform`. Raw/facade GREEN pass `1/1`
(`141ms`)/`1/1` (`31ms`), ImageCms targeted passes `144/144` (`1266ms`), and
full `2158/2158` passes (`17313ms`). Registrations are `1063/1095`; Release x64
builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`FF747CF87374210D6F5AA110C1BC51F519E601C43BD836A8B393A32D350CF6C1`.

Latest `META-003BR`: allocating one-shot RGB/sRGB-to-RGB/memory-opened-sRGB
profileToProfile accepts saturation intent `2` with Pillow's bounded black-
point-compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes
exact identity 3x2 RGB bytes, unchanged RGB source pixels and caller Info, one
fresh 588-byte `acsp` output ICC blob, and profile-memory/profile-independent
result lifetime. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`187ms`)/`1/1` (`31ms`), ImageCms
targeted passes `142/142` (`1328ms`), and full `2156/2156` passes (`17328ms`).
Registrations are `1062/1094`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`D4C124C26CD858E1C9E9E77A26C39E096EF8E9F5025833DC36D8AD9891E5EE43`.

Latest `META-003BQ`: allocating one-shot LAB/LAB-to-RGB/sRGB profileToProfile
accepts saturation intent `2` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`), completing both mode-changing saturation/BPC pairs. A
bounded Pillow 11.3.0 oracle fixes exact 3x2 RGB bytes, unchanged LAB source
pixels and caller Info, one fresh 588-byte `acsp` output ICC blob, and profile-
independent result lifetime. Raw RED returned `-3`; facade RED raised `cannot
build transform`. Raw/facade GREEN pass `1/1` (`171ms`)/`1/1` (`63ms`),
ImageCms targeted passes `140/140` (`1156ms`), and full `2154/2154` passes
(`17187ms`). Registrations are `1061/1093`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`0FF2388CF03A0AC036F7990FA428AAF1063E69DA0AA31282EDCFADD23344635F`.

Latest `META-003BP`: allocating one-shot RGB/sRGB-to-LAB/LAB profileToProfile
accepts saturation intent `2` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact 3x2 LAB
bytes, unchanged RGB source pixels and caller Info, one fresh 572-byte `acsp`
output ICC blob, and profile-independent result lifetime. Raw RED returned
`-3`; facade RED raised `cannot build transform`. Raw/facade GREEN pass `1/1`
(`157ms`)/`1/1` (`47ms`), ImageCms targeted passes `138/138` (`1156ms`), and
full `2152/2152` passes (`16687ms`). Registrations are `1060/1092`; Release
x64 builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`955894B342D929BBE706D1494344D781061EFF8DDB942E5763AE491DD5E7F221`.

Latest `META-003BO`: allocating one-shot D50-to-6500K LAB/LAB
profileToProfile accepts relative-colorimetric intent `1` with Pillow's bounded
black-point-compensation flag `8192` (`0x2000`), completing this exact route
across all four established one-shot pairs. A bounded Pillow 11.3.0 oracle
fixes exact identity 3x2 LAB bytes, unchanged LAB source pixels and caller Info,
one fresh 572-byte `acsp` output ICC blob, and profile-independent result
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`141ms`)/`1/1` (`47ms`), ImageCms targeted passes
`136/136` (`1125ms`), and full `2150/2150` passes (`16328ms`). Registrations
are `1059/1091`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`BC78D6575B41E9EE0D647E83422152D7946E00B0D0D7D7C7AB7B11C62E132281`.

Latest `META-003BN`: allocating one-shot RGB/sRGB-to-RGB/memory-opened-sRGB
profileToProfile accepts relative-colorimetric intent `1` with Pillow's bounded
black-point-compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle
fixes exact identity 3x2 RGB bytes, unchanged RGB source pixels and caller Info,
one fresh 588-byte `acsp` output ICC blob, and profile-memory/profile-
independent result lifetime. Raw RED returned `-3`; facade RED raised `cannot
build transform`. Raw/facade GREEN pass `1/1` (`156ms`)/`1/1` (`47ms`),
ImageCms targeted passes `134/134` (`1078ms`), and full `2148/2148` passes
(`17093ms`). Registrations are `1058/1090`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`787BB7513BAEE4F3E5139BBE07C73CB8F204F014686E77A656A9FC55353B8647`.

Latest `META-003BM`: allocating one-shot LAB/LAB-to-RGB/sRGB profileToProfile
accepts relative-colorimetric intent `1` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`), completing both mode-changing pairs for
this exact route. A bounded Pillow 11.3.0 oracle fixes exact 3x2 RGB bytes,
unchanged LAB source pixels and caller Info, one fresh 588-byte `acsp` output
ICC blob, and profile-independent result lifetime. Raw RED returned `-3`;
facade RED raised `cannot build transform`. Raw/facade GREEN pass `1/1`
(`156ms`)/`1/1` (`47ms`), ImageCms targeted passes `132/132` (`1094ms`), and
full `2146/2146` passes (`16328ms`). Registrations are `1057/1089`; Release
x64 builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`B1C4292B6D129155E899719347248B0FA0656A5548E1139502FC6D9AE12984BD`.

Latest `META-003BL`: allocating one-shot RGB/sRGB-to-LAB/LAB profileToProfile
accepts relative-colorimetric intent `1` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact
3x2 LAB bytes, unchanged RGB source pixels and caller Info, one fresh 572-byte
`acsp` output ICC blob, and profile-independent result lifetime. Raw RED
returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`156ms`)/`1/1` (`31ms`), ImageCms targeted passes `130/130`
(`1047ms`), and full `2144/2144` passes (`16000ms`). Registrations are
`1056/1088`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`57A1024D9C8C8D5EFBF0F2A21651D35AD387F279E01E6162F76BB425AF799DD0`.

Latest `META-003BK`: allocating one-shot D50-to-6500K LAB/LAB
profileToProfile accepts perceptual intent `0` with Pillow's bounded black-
point-compensation flag `8192` (`0x2000`), completing this exact route across
all four established one-shot pairs. A bounded Pillow 11.3.0 oracle fixes exact
identity 3x2 LAB bytes, unchanged LAB source pixels and caller Info, one fresh
572-byte `acsp` output ICC blob, and profile-independent result lifetime. Raw
RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`141ms`)/`1/1` (`31ms`), ImageCms targeted passes `128/128`
(`1016ms`), and full `2142/2142` passes (`17297ms`). Registrations are
`1055/1087`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`042C0DD7D51EAE67082178EF48EB663F8DA729BE4AA2E7BF752E39378A7C8BA1`.

Latest `META-003BJ`: allocating one-shot RGB/sRGB-to-RGB/memory-opened-sRGB
profileToProfile accepts perceptual intent `0` with Pillow's bounded black-
point-compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes
exact identity 3x2 RGB bytes, unchanged RGB source pixels and caller Info, one
fresh 588-byte `acsp` output ICC blob, and profile-memory/profile-independent
result lifetime. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`219ms`)/`1/1` (`63ms`), ImageCms
targeted passes `126/126` (`1047ms`), and full `2140/2140` passes (`16328ms`).
Registrations are `1054/1086`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`88B83F8AE354FB6F1771DB84ECAADFECECE584925528DD074F47328DACC0CC0F`.

Latest `META-003BI`: allocating one-shot LAB/LAB-to-RGB/sRGB
profileToProfile accepts perceptual intent `0` with Pillow's bounded black-
point-compensation flag `8192` (`0x2000`), completing both established mode-
changing pairs for this exact one-shot intent/flag. A bounded Pillow 11.3.0
oracle fixes exact 3x2 RGB bytes, unchanged LAB source pixels and caller Info,
one fresh 588-byte `acsp` output ICC blob, and profile-independent result
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`203ms`)/`1/1` (`46ms`), ImageCms targeted
passes `124/124` (`984ms`), and full `2138/2138` passes (`17266ms`).
Registrations are `1053/1085`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`133A9875DC0706C1E3D3A3EE69E9C761BD6DBC01AF2FD89C60AB6C8F5599746B`.

Latest `META-003BH`: allocating one-shot RGB/sRGB-to-LAB/LAB profileToProfile
accepts perceptual intent `0` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact 3x2 LAB
bytes, unchanged RGB source pixels and caller Info, one fresh 572-byte `acsp`
output ICC blob, and profile-independent result lifetime. Raw RED returned
`-3`; facade RED raised `cannot build transform`. Raw/facade GREEN pass `1/1`
(`156ms`)/`1/1` (`47ms`), ImageCms targeted passes `122/122` (`985ms`), and
full `2136/2136` passes (`16313ms`). Registrations are `1052/1084`; Release
x64 builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`D3055F3760C03796E8215BDF1AE8791CEE61746C61B2EF38EE1D5D348A602EA7`.

Latest `META-003BG`: reusable D50-to-6500K LAB/LAB transform build/apply
accepts absolute-colorimetric intent `3` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`), completing intents `0..3` plus BPC across
all four established reusable pairs. A bounded Pillow 11.3.0 oracle fixes exact
repeat 3x2/1x1 identity LAB bytes, unchanged LAB sources and caller Info, two
fresh distinct 572-byte `acsp` output ICC blobs, and profile-independent
transform lifetime. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`172ms`)/`1/1` (`16ms`), ImageCms
targeted passes `120/120` (`656ms`), and full `2134/2134` passes (`11000ms`).
Registrations are `1051/1083`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`A80DA2A1C8CB266858A18B1799F9EE61F70B321E849995B0F5D15F737EEB7147`.

Latest `META-003BF`: reusable RGB/sRGB-to-RGB/memory-opened-sRGB transform
build/apply accepts absolute-colorimetric intent `3` with Pillow's bounded
black-point-compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle
fixes exact repeat 3x2/1x1 identity RGB bytes, unchanged RGB sources and caller
Info, two fresh distinct 588-byte `acsp` output ICC blobs, and profile-memory/
profile-independent transform lifetime. Raw RED returned `-3`; facade RED
raised `cannot build transform`. Raw/facade GREEN pass `1/1` (`172ms`)/`1/1`
(`16ms`), ImageCms targeted passes `118/118` (`640ms`), and full `2132/2132`
passes (`10969ms`). Registrations are `1050/1082`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`F9B52490F8557074EA72068D2351E8F271AEB1F1184DCF742B282502ADCA968B`.

Latest `META-003BE`: reusable LAB/LAB-to-RGB/sRGB transform build/apply accepts
absolute-colorimetric intent `3` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`), completing both mode-changing pairs for that intent/
flag. A bounded Pillow 11.3.0 oracle fixes exact repeat 3x2/1x1 reverse RGB
bytes, unchanged LAB sources and caller Info, two fresh distinct 588-byte
`acsp` output ICC blobs, and profile-independent transform lifetime. Raw RED
returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`172ms`)/`1/1` (`47ms`), ImageCms targeted passes `116/116`
(`593ms`), and full `2130/2130` passes (`10688ms`). Registrations are
`1049/1081`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`E118088CC55492410774BCDA44FCA0A8360B52914F6987E24258743B99EA6624`.

Latest `META-003BD`: reusable RGB/sRGB-to-LAB/LAB transform build/apply
accepts absolute-colorimetric intent `3` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact
repeat 3x2/1x1 LAB bytes, unchanged RGB sources and caller Info, two fresh
distinct 572-byte `acsp` output ICC blobs, and profile-independent transform
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`188ms`)/`1/1` (`31ms`), ImageCms targeted passes
`114/114` (`532ms`), and full `2128/2128` passes (`15047ms`). Registrations are
`1048/1080`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`43E9F4E3A46131DEB57540F7C692CCF58A55296AA40ECAA7FB16BBBBCC977603`.

Latest `META-003BC`: reusable D50-to-6500K LAB/LAB transform build/apply
accepts saturation intent `2` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`), completing that route across all four established
reusable pairs. A bounded Pillow 11.3.0 oracle fixes exact repeat 3x2/1x1
identity LAB bytes, unchanged LAB sources and caller Info, two fresh distinct
572-byte `acsp` output ICC blobs, and profile-independent transform lifetime.
Raw RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade
GREEN pass `1/1` (`140ms`)/`1/1` (`16ms`), ImageCms targeted passes `112/112`
(`578ms`), and full `2126/2126` passes (`10735ms`). Registrations are
`1047/1079`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`5BC61E57225926A3528E28FD9B685723AE01A4D9525831B7FFEE9A2EB76B2D9C`.

Latest `META-003BB`: reusable RGB/sRGB-to-RGB/memory-opened-sRGB transform
build/apply accepts saturation intent `2` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact
repeat 3x2/1x1 identity RGB bytes, unchanged RGB sources and caller Info, two
fresh distinct 588-byte `acsp` output ICC blobs, and profile-memory/profile-
independent transform lifetime. Raw RED returned `-3`; facade RED raised
`cannot build transform`. Raw/facade GREEN pass `1/1` (`110ms`)/`1/1`
(`16ms`), ImageCms targeted passes `110/110` (`563ms`), and full `2124/2124`
passes (`11391ms`). Registrations are `1046/1078`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`A40D84E96D877509CFE00F5954EC4294232D154E63700A061812E1264CA6DCD4`.

Latest `META-003BA`: reusable LAB/LAB-to-RGB/sRGB transform build/apply accepts
saturation intent `2` with Pillow's bounded black-point-compensation flag
`8192` (`0x2000`), completing both mode-changing pairs for that intent/flag. A
bounded Pillow 11.3.0 oracle fixes exact repeat 3x2/1x1 reverse RGB bytes,
unchanged LAB sources and caller Info, two fresh distinct 588-byte `acsp`
output ICC blobs, and profile-independent transform lifetime. Raw RED returned
`-3`; facade RED raised `cannot build transform`. Raw/facade GREEN pass `1/1`
(`140ms`)/`1/1` (`16ms`), ImageCms targeted passes `108/108` (`609ms`), and
full `2122/2122` passes (`11125ms`). Registrations are `1045/1077`; Release x64
builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`EADC87096D89902FD73DAE04F97596A3FBE97F89863628BAC930F92D5A2C313B`.

Latest `META-003AZ`: reusable RGB/sRGB-to-LAB/LAB transform build/apply
accepts saturation intent `2` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact repeat
3x2/1x1 LAB bytes, unchanged RGB sources and caller Info, two fresh distinct
572-byte `acsp` output ICC blobs, and profile-independent transform lifetime.
Raw RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade
GREEN pass `1/1` (`157ms`)/`1/1` (`15ms`), ImageCms targeted passes `106/106`
(`593ms`), and full `2120/2120` passes (`11609ms`). Registrations are
`1044/1076`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`A180FD81089BFCABF9F375F8325A25A624A79F80986159BB4BB5DDC4DDC9505F`.

Latest `META-003AY`: reusable D50-to-6500K LAB/LAB transform build/apply
accepts relative-colorimetric intent `1` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`), completing that route across all four
established reusable pairs. A bounded Pillow 11.3.0 oracle fixes exact repeat
3x2/1x1 identity LAB bytes, unchanged LAB sources and caller Info, two fresh
distinct 572-byte `acsp` output ICC blobs, and profile-independent transform
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`125ms`)/`1/1` (`16ms`), ImageCms targeted passes
`104/104` (`531ms`), and full `2118/2118` passes (`11719ms`). Registrations are
`1043/1075`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`855712FE0FF147089DA4F57DDC1F89814C2E2D17B2266D4AE82069939511FEF5`.

Latest `META-003AX`: reusable RGB/sRGB-to-RGB/memory-opened-sRGB transform
build/apply accepts relative-colorimetric intent `1` with Pillow's bounded
black-point-compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle
fixes exact repeat 3x2/1x1 identity RGB bytes, unchanged RGB sources and caller
Info, two fresh distinct 588-byte `acsp` output ICC blobs, and profile-memory/
profile-independent transform lifetime. Raw RED returned `-3`; facade RED
raised `cannot build transform`. Raw/facade GREEN pass `1/1` (`203ms`)/`1/1`
(`16ms`), ImageCms targeted passes `102/102` (`610ms`), and full `2116/2116`
passes (`11875ms`). Registrations are `1042/1074`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`131BB72367D5296BF4A75A2D444D1B0255A424D666F60B919D6D103728605D3A`.

Latest `META-003AW`: reusable LAB/LAB-to-RGB/sRGB transform build/apply
accepts relative-colorimetric intent `1` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact
repeat 3x2/1x1 reverse RGB bytes, unchanged LAB sources and caller Info, two
fresh distinct 588-byte `acsp` output ICC blobs, and profile-independent
transform lifetime. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`172ms`)/`1/1` (`16ms`), ImageCms
targeted passes `100/100` (`563ms`), and full `2114/2114` passes (`13141ms`).
Registrations are `1041/1073`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`7803C98276EE73B524B55BBEA8F040C17A8FB08F02D1D2EDF3D4CDE72C0BE225`.

Latest `META-003AV`: reusable RGB/sRGB-to-LAB/LAB transform build/apply
accepts relative-colorimetric intent `1` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact
repeat 3x2/1x1 LAB bytes, unchanged RGB sources and caller Info, two fresh
distinct 572-byte `acsp` output ICC blobs, and profile-independent transform
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`187ms`)/`1/1` (`15ms`), ImageCms targeted
passes `98/98` (`516ms`), and full `2112/2112` passes (`13265ms`).
Registrations are `1040/1072`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`0524291D2A58709EF2A0463D98EAB4E0D2FDE06A15F59768FF83B1C97401A76E`.

Latest `META-003AU`: reusable D50-to-6500K LAB/LAB transform build/apply
accepts perceptual intent `0` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`), completing that route across all four established
reusable pairs. A bounded Pillow 11.3.0 oracle fixes exact repeat 3x2/1x1
identity LAB bytes, unchanged LAB sources and caller Info, two fresh distinct
572-byte `acsp` output ICC blobs, and profile-independent transform lifetime.
Raw RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade
GREEN pass `1/1` (`171ms`)/`1/1` (`16ms`), ImageCms targeted passes `96/96`
(`547ms`), and full `2110/2110` passes (`12922ms`). Registrations are
`1039/1071`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`BA4E8CAC692E7AFF483551D6BBE2D1E1906DD661CF5FA45580BCA4E53A78F852`.

Latest `META-003AT`: reusable RGB/sRGB-to-RGB/memory-opened-sRGB transform
build/apply accepts perceptual intent `0` with Pillow's bounded black-point-
compensation flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact
repeat 3x2/1x1 identity RGB bytes, unchanged RGB sources and caller Info, two
fresh distinct 588-byte `acsp` output ICC blobs, and profile-memory/profile-
independent transform lifetime. Raw RED returned `-3`; facade RED raised
`cannot build transform`. Raw/facade GREEN pass `1/1` (`187ms`)/`1/1`
(`16ms`), ImageCms targeted passes `94/94` (`515ms`), and full `2108/2108`
passes (`12844ms`). Registrations are `1038/1070`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`26FB915D689B5A5C9BFFFD786408BA7CD4FC609843FCB0EB7E874E74451164F5`.

Latest `META-003AS`: reusable LAB/LAB-to-RGB/sRGB transform build/apply
accepts perceptual intent `0` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact repeat
3x2/1x1 RGB bytes, unchanged LAB sources and caller Info, two fresh distinct
588-byte `acsp` output ICC blobs, and profile-independent transform lifetime.
Raw RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade
GREEN pass `1/1` (`171ms`)/`1/1` (`16ms`), ImageCms targeted passes `92/92`
(`578ms`), and full `2106/2106` passes (`14016ms`). Registrations are
`1037/1069`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`C301A54FB0CF19DAA9850C4DB3F0AE7B370F3EA28BBBDDDDAC89AE2956096120`.

Latest `META-003AR`: reusable RGB/sRGB-to-LAB/LAB transform build/apply
accepts perceptual intent `0` with Pillow's bounded black-point-compensation
flag `8192` (`0x2000`). A bounded Pillow 11.3.0 oracle fixes exact repeat
3x2/1x1 LAB bytes, unchanged RGB sources and caller Info, two fresh distinct
572-byte `acsp` output ICC blobs, and profile-independent transform lifetime.
Raw RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade
GREEN pass `1/1` (`172ms`)/`1/1` (`15ms`), ImageCms targeted passes `90/90`
(`453ms`), and full `2104/2104` passes (`12953ms`). Registrations are
`1036/1068`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`5022FA192187DC881048D4D6982203D07084F91315F9C2EC782AA2A86FC5A5ED`.

Latest `META-003AQ`: reusable D50-to-6500K LAB/LAB transform build/apply
accepts absolute-colorimetric rendering intent `3` with flags `0`, completing
intents `0..3` across all four established reusable pairs. A bounded Pillow
11.3.0 oracle fixes exact repeat 3x2/1x1 identity LAB bytes, unchanged sources,
two fresh distinct 572-byte `acsp` output ICC blobs, and profile-independent
transform lifetime. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`172ms`)/`1/1` (`15ms`), ImageCms
targeted passes `88/88` (`860ms`), and full `2102/2102` passes (`20500ms`).
Registrations are `1035/1067`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`5958951C963A441633262D5A69E7C0845B277983E6AB2B9B2E5091763FB1C468`.

Latest `META-003AP`: reusable D50-to-6500K LAB/LAB transform build/apply
accepts saturation rendering intent `2` with flags `0`. A bounded Pillow 11.3.0
oracle fixes exact repeat 3x2/1x1 identity LAB bytes, unchanged sources, two
fresh distinct 572-byte `acsp` output ICC blobs, and profile-independent
transform lifetime. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`156ms`)/`1/1` (`31ms`), ImageCms
targeted passes `86/86` (`375ms`), and full `2100/2100` passes (`9954ms`).
Registrations are `1034/1066`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`A212108E787C112C4B8A4A79CD07866E86888BB586D9C2D09E20C86DD99DA417`.

Latest `META-003AO`: reusable RGB/sRGB-to-RGB/memory-opened-sRGB transform
build/apply accepts absolute-colorimetric rendering intent `3` with flags `0`,
completing intents `0..3` for reusable RGB/RGB. A bounded Pillow 11.3.0 oracle
fixes exact repeat 3x2/1x1 identity RGB bytes, unchanged sources, two fresh
588-byte `acsp` output ICC blobs, and profile-memory-independent transform
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`234ms`)/`1/1` (`16ms`), ImageCms targeted passes
`84/84` (`375ms`), and full `2098/2098` passes (`10016ms`). Registrations are
`1033/1065`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`2555122CE65CC6EFB330FB5886E1C1D08646B42F4AD1F03800014D39677A1674`.

Latest `META-003AN`: reusable RGB/sRGB-to-RGB/memory-opened-sRGB transform
build/apply accepts saturation rendering intent `2` with flags `0`. A bounded
Pillow 11.3.0 oracle fixes exact repeat 3x2/1x1 identity RGB bytes, unchanged
sources, two fresh 588-byte `acsp` output ICC blobs, and profile-memory-
independent transform lifetime. Raw RED returned `-3`; facade RED raised
`cannot build transform`. Raw/facade GREEN pass `1/1` (`218ms`)/`1/1`
(`63ms`), ImageCms targeted passes `82/82` (`687ms`), and full `2096/2096`
passes (`17781ms`). Registrations are `1032/1064`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`E757CFF604A83E7F9DB5E62748DA0866A015E4030D1DFFD52ECC0396049FB5C6`.

Latest `META-003AM`: reusable LAB/LAB-profile-to-RGB/sRGB transform build/apply
accepts absolute-colorimetric rendering intent `3` with flags `0`, completing
intents `0..3` across both mode-changing reusable pairs. A bounded Pillow
11.3.0 oracle fixes exact repeat 3x2/1x1 reverse RGB bytes, unchanged LAB
sources, two fresh 588-byte `acsp` output ICC blobs, and profile-independent
transform lifetime. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`203ms`)/`1/1` (`47ms`), ImageCms
targeted passes `80/80` (`875ms`), and full `2094/2094` passes (`20125ms`).
Registrations are `1031/1063`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`68DD191A38DDC994D54C92063F3F6BD3EEA26EB9227BD32CDE069FB936766A3F`.

Latest `META-003AL`: reusable LAB/LAB-profile-to-RGB/sRGB transform build/apply
accepts saturation rendering intent `2` with flags `0`. A bounded Pillow 11.3.0
oracle fixes exact repeat 3x2/1x1 reverse RGB bytes, unchanged LAB sources, two
fresh 588-byte `acsp` output ICC blobs, and profile-independent transform
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`218ms`)/`1/1` (`47ms`), ImageCms targeted passes
`78/78` (`703ms`), and full `2092/2092` passes (`18828ms`). Registrations are
`1030/1062`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`7DC94CEE64953E580B4FF528EF1FE4F883A7782C8FD252312F1F6E46E2895BF5`.

Latest `META-003AK`: one-shot D50-to-6500K LAB/LAB in-place
`profileToProfile` accepts absolute-colorimetric rendering intent `3` with
flags `0`, completing intents `0..3` across both legal one-shot in-place pairs.
A bounded Pillow 11.3.0 oracle fixes the Python `None` return, exact identity
LAB bytes, preserved caller Info plus the distinct 572-byte `acsp` output ICC,
and profile-independent lifetime. Raw RED returned `-3`; facade RED raised
`cannot build transform`. Raw/facade GREEN pass `1/1` (`78ms`)/`1/1`
(`16ms`), ImageCms targeted passes `76/76` (`344ms`), and full `2090/2090`
passes (`10672ms`). Registrations are `1029/1061`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`9EAA6510AB9D48C0E3CC81A2F095FF8ECF83294D3131DEA83BCC9607E79268B6`.

Latest `META-003AJ`: one-shot D50-to-6500K LAB/LAB in-place
`profileToProfile` accepts saturation rendering intent `2` with flags `0`. A
bounded Pillow 11.3.0 oracle fixes the Python `None` return, exact identity LAB
bytes, preserved caller Info plus the distinct 572-byte `acsp` output ICC, and
profile-independent lifetime. Raw RED returned `-3`; facade RED raised `cannot
build transform`. Raw/facade GREEN pass `1/1` (`141ms`)/`1/1` (`32ms`),
ImageCms targeted passes `74/74` (`312ms`), and full `2088/2088` passes
(`10109ms`). Registrations are `1028/1060`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`67DB2646A0BF9D0F224A5A9E034DD885200765F1300E2171D8B154383F3AD0AD`.

Latest `META-003AI`: one-shot RGB/sRGB-to-RGB/memory-opened-sRGB in-place
`profileToProfile` accepts absolute-colorimetric rendering intent `3` with
flags `0`, completing intents `0..3` for that pair. A bounded Pillow 11.3.0
oracle fixes the Python `None` return, exact identity RGB bytes, preserved
caller Info plus the 588-byte `acsp` output ICC, and profile-memory-independent
lifetime. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`156ms`)/`1/1` (`31ms`), ImageCms targeted passes
`72/72` (`1141ms`), and full `2086/2086` passes (`27437ms`). Registrations are
`1027/1059`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`984CCCB8B0F98E1C0BA4C56F2E524E351DABD7B473ABE824CE65A32FF8E82E0D`.

Latest `META-003AH`: one-shot RGB/sRGB-to-RGB/memory-opened-sRGB in-place
`profileToProfile` accepts saturation rendering intent `2` with flags `0`. A
bounded Pillow 11.3.0 oracle fixes the Python `None` return, exact identity RGB
bytes, preserved caller Info plus the 588-byte `acsp` output ICC, and profile-
memory-independent lifetime. Raw RED returned `-3`; facade RED raised `cannot
build transform`. Raw/facade GREEN pass `1/1` (`250ms`)/`1/1` (`47ms`),
ImageCms targeted passes `70/70` (`1000ms`), and full `2084/2084` passes
(`28047ms`). Registrations are `1026/1058`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`C9EE1E7AC81CD5167851643A72838DB819ED0C351C27622EEC056EDB8E8E414E`.

Latest `META-003AG`: allocating one-shot D50-to-6500K LAB/LAB
`profileToProfile` accepts absolute-colorimetric rendering intent `3` with
flags `0`, completing intents `0..3` across all four allocating pairs. A
bounded Pillow 11.3.0 oracle fixes exact identity LAB bytes, the distinct
572-byte `acsp` output ICC, and source/profile-independent lifetime. Raw RED
returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`266ms`)/`1/1` (`15ms`), ImageCms targeted passes `68/68`
(`1062ms`), and full `2082/2082` passes (`31766ms`). Registrations are
`1025/1057`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`B3B4C87F9DE2E1D248B2589582E82E3D50F6348F4D9041ECC11FED709DC418F5`.

Latest `META-003AF`: allocating one-shot D50-to-6500K LAB/LAB
`profileToProfile` accepts saturation rendering intent `2` with flags `0`.
A bounded Pillow 11.3.0 oracle fixes exact identity LAB bytes, the distinct
572-byte `acsp` output ICC, and source/profile-independent lifetime. Raw RED
returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`234ms`)/`1/1` (`47ms`), ImageCms targeted passes `66/66`
(`907ms`), and full `2080/2080` passes (`29265ms`). Registrations are
`1024/1056`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`BDEC3B6A5E806D0C27A13EC831275277958750F0FC0197AFAD168864978037CA`.

Latest `META-003AE`: allocating one-shot RGB/sRGB-to-RGB/memory-opened-sRGB
`profileToProfile` accepts absolute-colorimetric rendering intent `3` with
flags `0`, completing intents `0..3` for that same-mode pair. A bounded Pillow
11.3.0 oracle fixes exact identity RGB bytes, the 588-byte `acsp` output ICC,
and source/profile-memory-independent lifetime. Raw RED returned `-3`; facade
RED raised `cannot build transform`. Raw/facade GREEN pass `1/1` (`265ms`)/
`1/1` (`16ms`), ImageCms targeted passes `64/64` (`906ms`), and full
`2078/2078` passes (`27703ms`). Registrations are `1023/1055`; Release x64
builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`119D575872616F90F27A69BD5D66BEE20BB99B2DFA0EC80160A7B6BD796C6A15`.

Latest `META-003AD`: allocating one-shot RGB/sRGB-to-RGB/memory-opened-sRGB
`profileToProfile` accepts saturation rendering intent `2` with flags `0`.
A bounded Pillow 11.3.0 oracle fixes exact identity RGB bytes, the 588-byte
`acsp` output ICC, and source/profile-memory-independent lifetime. Raw RED
returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`359ms`)/`1/1` (`16ms`), ImageCms targeted passes `62/62`
(`938ms`), and full `2076/2076` passes (`26735ms`). Registrations are
`1022/1054`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`320E5DFA1A75D541663FA1E203EF9A029F693B66A55D84F27B8C951BBFD363FA`.

Latest `META-003AC`: allocating one-shot LAB/LAB-to-RGB/sRGB
`profileToProfile` accepts absolute-colorimetric rendering intent `3` with
flags `0`, completing intents `0..3` across both allocating mode-changing
pairs. A bounded Pillow 11.3.0 oracle fixes the exact reverse RGB bytes and
588-byte `acsp` ICC contract. The result remains valid after both profiles
close and leaves source pixels/Info unchanged. Raw RED returned `-3`; facade
RED raised `cannot build transform`. Raw/facade GREEN pass `1/1` (`141ms`)/
`1/1` (`47ms`), ImageCms targeted passes `60/60` (`922ms`), and full
`2074/2074` passes (`27953ms`). Registrations are `1021/1053`; Release x64
builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`7ED4248BD3567D1DCD9DAEDBEFA409F494DB836EEF23C6881C554FB6EA71508A`.

Latest `META-003AB`: allocating one-shot LAB/LAB-to-RGB/sRGB
`profileToProfile` accepts saturation rendering intent `2` with flags `0`.
A bounded Pillow 11.3.0 oracle fixes the exact reverse RGB bytes and 588-byte
`acsp` ICC contract. The result remains valid after both profiles close and
leaves source pixels/Info unchanged. Raw RED returned `-3`; facade RED raised
`cannot build transform`. Raw/facade GREEN pass `1/1` (`203ms`)/`1/1`
(`32ms`), ImageCms targeted passes `58/58` (`281ms`), and full `2072/2072`
passes (`9985ms`). Registrations are `1020/1052`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`FF6A23B754663EC415B516163824064DEBD29766CAD83CE74155BA59C5B3CB33`.

Latest `META-003AA`: allocating one-shot RGB/sRGB-to-LAB/LAB
`profileToProfile` accepts absolute-colorimetric rendering intent `3` with
flags `0`, completing intents `0..3` for that allocating pair. The result
matches exact Pillow LAB bytes, contains only the fresh 572-byte `acsp` output
ICC, remains valid after both profiles close, and leaves source pixels/Info
unchanged. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`156ms`)/`1/1` (`47ms`), ImageCms targeted passes
`56/56` (`313ms`), and full `2070/2070` passes (`15093ms`). Registrations are
`1019/1051`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`BF16B7CA7182BD19D24961B84DC278475F65D6A8FBF92E17214C730B82929685`.

Latest `META-003Z`: allocating one-shot RGB/sRGB-to-LAB/LAB
`profileToProfile` accepts saturation rendering intent `2` with flags `0`.
The result matches exact Pillow LAB bytes, contains only the fresh 572-byte
`acsp` output ICC, remains valid after both profiles close, and leaves source
pixels/Info unchanged. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`157ms`)/`1/1` (`46ms`), ImageCms
targeted passes `54/54` (`437ms`), and full `2068/2068` passes (`18031ms`).
Registrations are `1018/1050`; Release x64 builds with zero warnings/errors;
exports remain `413/413` with zero set difference; SHA-256 is
`93DF64E845F43421F8F45151F5A0665EE8C1C5173B6D4AC0A2BFB49E35605C65`.

Latest `META-003Y`: reusable RGB/sRGB-to-LAB/LAB transforms accept absolute-
colorimetric rendering intent `3` with flags `0`. Profiles close before repeat
3x2/1x1 apply; results match exact Pillow LAB bytes and contain only fresh
572-byte `acsp` output ICC Buffers, while sources/Info remain unchanged. Raw
RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`188ms`)/`1/1` (`31ms`), ImageCms targeted passes `52/52`
(`453ms`), and full `2066/2066` passes (`13657ms`). Registrations are
`1017/1049`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`95105B01C1B252AEDB6BAC8D9AD974F2E2F28ADBC61C74E8D03DD3726D22BB49`.

Latest `META-003X`: reusable RGB/sRGB-to-LAB/LAB transforms accept saturation
rendering intent `2` with flags `0`. Profiles close before repeat 3x2/1x1
apply; results match exact Pillow LAB bytes and contain only fresh 572-byte
`acsp` output ICC Buffers, while sources/Info remain unchanged. Raw RED
returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`203ms`)/`1/1` (`47ms`), ImageCms targeted passes `50/50`
(`437ms`), and full `2064/2064` passes (`19734ms`). Registrations are
`1016/1048`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`4458CEFD629534307A29EDAC34F0D60C548BF188C864032C8DFCB1C2DCAD6C45`.

Latest `META-003W`: one-shot D50-to-6500K LAB/LAB `profileToProfile` applies
in place with relative-colorimetric rendering intent `1` and flags `0`. It
retains the same image handle and exact LAB bytes, preserves caller Info,
installs the distinct 572-byte `acsp` output ICC, survives both profile
releases, and returns the Python `None` analogue. Raw RED returned `-3`; facade
RED raised `cannot build transform`. Raw/facade GREEN pass `1/1` (`125ms`)/
`1/1` (`31ms`), ImageCms targeted passes `48/48` (`359ms`), and full
`2062/2062` passes (`15015ms`). Registrations are `1015/1047`; Release x64
builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`2BF08CFD0D4FF7ADAE7BE52E9A2990AAFFF02C6253ED5C3B6055333ACEC8076A`.

Latest `META-003V`: one-shot RGB/sRGB-to-RGB/memory-opened-sRGB
`profileToProfile` applies in place with relative-colorimetric rendering intent
`1` and flags `0`. It retains the same image handle and exact RGB bytes,
preserves caller Info, installs the 588-byte `acsp` output ICC, survives Buffer/
profile release, and returns the Python `None` analogue. Raw RED returned `-3`;
facade RED raised `cannot build transform`. Raw/facade GREEN pass `1/1`
(`172ms`)/`1/1` (`47ms`), ImageCms targeted passes `46/46` (`344ms`), and
full `2060/2060` passes (`18297ms`). Registrations are `1014/1046`; Release x64
builds with zero warnings/errors; exports remain `413/413` with zero set
difference; SHA-256 is
`694A49B193366C86E26B60A83EEF6181EC7392EECDDF7681579AF0C359F973DC`.

Latest `META-003U`: allocating one-shot D50-to-6500K LAB/LAB
`profileToProfile` accepts relative-colorimetric rendering intent `1` with
flags `0`. The exact identity LAB result contains only the distinct 572-byte
`acsp` output ICC, survives both profile releases, and leaves source bytes/Info
unchanged. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`140ms`)/`1/1` (`31ms`), ImageCms targeted passes
`44/44` (`328ms`), and full `2058/2058` passes (`17734ms`). Registrations are
`1013/1045`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`1CECC50202ACA44A144614F6CCB64E5EE387EEC0712FDA15DC0F529C3127C1CB`.

Latest `META-003T`: allocating one-shot RGB/sRGB-to-RGB/memory-opened-sRGB
`profileToProfile` accepts relative-colorimetric rendering intent `1` with
flags `0`. The exact identity RGB result contains only the fresh 588-byte
`acsp` output ICC, survives source-profile Buffer plus both profile releases,
and leaves source bytes/Info unchanged. Raw RED returned `-3`; facade RED
raised `cannot build transform`. Raw/facade GREEN pass `1/1` (`157ms`)/`1/1`
(`47ms`), ImageCms targeted passes `42/42` (`359ms`), and full `2056/2056`
passes (`18312ms`). Registrations are `1012/1044`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`42A4AD167F1F5F1522EA6FD9748A455F37A1A2E053DB0F30A4EA9DB8AB698EC5`.

Latest `META-003S`: allocating one-shot LAB/LAB-to-RGB/sRGB
`profileToProfile` accepts relative-colorimetric rendering intent `1` with
flags `0`. The result matches Pillow's exact reverse RGB bytes, contains only
the fresh 588-byte `acsp` output ICC, remains valid after both profiles close,
and leaves source bytes/Info unchanged. Raw RED returned `-3`; facade RED
raised `cannot build transform`. Raw/facade GREEN pass `1/1` (`172ms`)/`1/1`
(`47ms`), ImageCms targeted passes `40/40` (`391ms`), and full `2054/2054`
passes (`17797ms`). Registrations are `1011/1043`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`F485C52DA03EC7E5C12366F0261A8A734CE5EDDAEE369B0ED326709B0564CD68`.

Latest `META-003R`: allocating one-shot RGB/sRGB-to-LAB/LAB
`profileToProfile` accepts relative-colorimetric rendering intent `1` with
flags `0`. The result matches exact Pillow LAB bytes, contains only the fresh
572-byte `acsp` output ICC, remains valid after both profiles close, and leaves
source bytes/Info unchanged. Raw RED returned `-3`; facade RED raised `cannot
build transform`. Raw/facade GREEN pass `1/1` (`188ms`)/`1/1` (`47ms`),
ImageCms targeted passes `38/38` (`328ms`), and full `2052/2052` passes
(`18156ms`). Registrations are `1010/1042`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`95C5C6849BAEA28D26CA37E40EF7E49D3FEFE163D32248B1C234E64D1348A5E3`.

Latest `META-003Q`: reusable RGB/sRGB-to-LAB/LAB transforms accept relative-
colorimetric rendering intent `1` with flags `0`. Profiles close before repeat
3x2/1x1 apply; results match exact Pillow LAB bytes and contain only the fresh
572-byte `acsp` output ICC, while both sources remain unchanged. Raw RED
returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`219ms`)/`1/1` (`47ms`), ImageCms targeted passes `36/36`
(`250ms`), and full `2050/2050` passes (`17734ms`). Registrations are
`1009/1041`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`7A31411CD5CBA65AC69018274FF0F438BB00115E42AFFD00242258860A67D4E2`.

Latest `META-003P`: one-shot `profileToProfile` LAB/LAB applies in place for
default-D50 input and 6500K output profiles. It retains source handle/mode/size/
exact bytes, preserves caller Info, installs the 572-byte `acsp` output ICC,
and returns the Python `None` analogue. Raw RED returned `-3`; facade RED raised
`cannot build transform`. Raw/facade GREEN pass `1/1` (`140ms`)/`1/1`
(`31ms`), ImageCms targeted passes `34/34` (`218ms`), and full `2048/2048`
passes (`17719ms`). Registrations are `1008/1040`; Release x64 builds with zero
warnings/errors; exports remain `413/413` with zero set difference; SHA-256 is
`21FB8FCB5790DE8AEFDE7A80A03585055154B3DE199BF15726F2F5E8752A4818`.

Latest `META-003O`: reusable LAB/LAB transforms apply in place after D50/6500K
profiles close. Two calls retain the same handle and exact public bytes, use no
allocating result, preserve caller Info, install a fresh 572-byte `acsp` output
ICC, and return the Python `None` analogue. Raw RED returned `-3`; existing
facade routing passed after native GREEN without production changes. Raw/facade
GREEN pass `1/1` (`157ms`)/`1/1` (`31ms`), ImageCms targeted passes `32/32`
(`235ms`), and full `2046/2046` passes (`18687ms`). Registrations are
`1007/1039`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`346A02F825C6D84995D0B61E48DCA0BFE903CF6EDCFACD8C48F9F01C5F0435AD`.

Latest `META-003N`: allocating one-shot `profileToProfile` accepts default-D50
input and 6500K output LAB profiles, returns a new exact LAB image with only
the distinct 572-byte `acsp` output ICC, and preserves source bytes/Info. Raw
RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade GREEN
pass `1/1` (`172ms`)/`1/1` (`31ms`), ImageCms targeted passes `30/30`
(`250ms`), and full `2044/2044` passes (`18796ms`). Registrations are
`1006/1038`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`05D52211CC02AD3211E4BC530F67F4CFB5A48FE737636EAA346CD1B3D4212FBA`.

Latest `META-003M`: reusable LAB/LAB accepts default-D50 input and 6500K output
profiles. The local oracle corrected the initial assumption: public LAB pixels
remain exactly identical, while results contain only the distinct 572-byte
`acsp` output ICC. Profiles close before 3x2/1x1 repeat apply and sources remain
unchanged. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` (`110ms`)/`1/1` (`31ms`), ImageCms targeted passes
`28/28` (`250ms`), and full `2042/2042` passes (`17250ms`). Registrations are
`1005/1037`; Release x64 builds with zero warnings/errors; exports remain
`413/413` with zero set difference; SHA-256 is
`DB72B63D489E28B8E97BBFEDD22BF6F389407903AC052C83D7F8E3F96833F551`.

Latest `META-003L`: one-shot `profileToProfile` RGB/RGB applies in place through
one coarse native ABI. It retains source handle/mode/size/exact bytes, preserves
caller Info, installs the 588-byte `acsp` output ICC, and returns the Python
`None` analogue. Raw RED was a missing export; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`188ms`)/`1/1` (`31ms`), ImageCms
targeted passes `26/26` (`125ms`), and full `2040/2040` passes (`20110ms`).
Registrations are `1004/1036`; Release x64 builds with zero warnings/errors;
exports are `413/413` with zero set difference; SHA-256 is
`E000DE6664FA98AE1293BC6C2713EB2971A9C70279B70474706F618870CA5C8C`.

Latest `META-003K`: allocating one-shot `profileToProfile` accepts built-in
sRGB input and a memory-reopened sRGB output profile, returns a new exact RGB
image with only the 588-byte `acsp` output ICC, and preserves source bytes/Info.
Raw RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade
GREEN pass `1/1` (`79ms`)/`1/1` (`31ms`), ImageCms targeted passes `24/24`
(`109ms`), and full `2038/2038` passes (`10844ms`). Registrations are
`1003/1035`; Release x64 builds with zero warnings/errors; exports remain
`412/412` with zero set difference; SHA-256 is
`435763AE8D4B0A7EEDA62A9073B7A983E8155624E44022F70012B8F86B33BF99`.

Latest `META-003J`: reusable RGB/RGB transforms apply in place through one new
native ABI. Two calls retain the same source handle and exact RGB bytes, use no
allocating result, preserve caller Info, install a fresh 588-byte `acsp` output
ICC Buffer, and return the Python `None` analogue. Raw RED was a missing export;
facade RED raised `mode mismatch`. Raw/facade GREEN pass `1/1` (`78ms`)/`1/1`
(`16ms`), ImageCms targeted passes `22/22` (`94ms`), and full `2036/2036`
passes (`10218ms`). Registrations are `1002/1034`; Release x64 builds with zero
warnings/errors; exports are `412/412` with zero set difference; SHA-256 is
`75898676451C19C6DFC8F4D57AE94F2211F2DA0F8DFDBE132D80F43A9BE67613`.

Latest `META-003I`: one reusable transform accepts built-in sRGB input and a
588-byte memory-reopened sRGB output profile. Profiles and the source ICC
Buffer release before repeat allocating apply; 3x2 and 1x1 RGB bytes remain
exact, sources remain unchanged, and every result owns only a fresh 588-byte
`acsp` output ICC Buffer. Raw RED returned `-3`; facade RED raised `cannot
build transform`. Raw/facade GREEN pass `1/1` (`110ms`)/`1/1` (`15ms`),
ImageCms targeted passes `20/20` (`109ms`), and full `2034/2034` passes
(`10078ms`). Registrations are `1001/1033`; Release x64 builds with zero
warnings/errors; exports remain `411/411` with zero set difference; SHA-256 is
`588BE122B6341DE05A58AA2C298104D002C77C78DDA70311DBBE7DD7D6DDDD2E`.

Latest `META-003H`: the reusable transform also accepts the exact LAB-to-RGB
reverse pair. Profiles close before repeat apply; 3x2 and 1x1 results match
Pillow's exact RGB bytes, sources remain unchanged, each result owns only a
fresh 588-byte sRGB ICC Info Buffer, and explicit/idempotent transform close
is preserved. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` (`109ms`)/`1/1` (`16ms`), ImageCms
targeted passes `18/18` (`93ms`), and full `2032/2032` passes (`9610ms`).
Registrations are `1000/1032`; exports remain `411/411` with zero set
difference; SHA-256 is
`4F7A0035D4DB9E55206BB050AF8455754E923938456834A11C0B9EFD0FE36E29`.

Latest `META-003G`: one reusable RGB-to-LAB ImageCms transform survives
profile closure, applies to 3x2 and 1x1 sources with exact bytes, and returns a
fresh 572-byte ICC Info Buffer per result. Raw/facade REDs hit missing native
build and public build routes. ImageCms targeted passes `16/16` (`94ms`)
and full `2030/2030` passes (`9625ms`). Registrations are `999/1031`;
exports are `411/411` with zero set difference; SHA-256 is
`F44BC7520328F2636BB6BFB0B7FBD00A2C42FE16983822DAE0CA53CD969DB9AD`.

Latest `META-003F`: a 588-byte sRGB ICC memory block opens into an independent
native profile; releasing the input Buffer leaves exact name and serialization
available. Raw/facade REDs hit the missing native/facade open routes. ImageCms
targeted passes `14/14` (`63ms`) and full `2028/2028` passes
(`9656ms`). Registrations are `998/1030`; exports are `407/407` with zero
set difference; SHA-256 is
`3EB41071735EAF2910A72F8654F3D27D57C3127A452339D19057D37D483479F1`.

Latest `META-003E`: bounded LAB/LAB-to-RGB/sRGB profile-to-profile conversion
matches Pillow's exact reverse bytes, leaves source bytes/Info unchanged, and
returns only a 588-byte sRGB ICC profile. Raw/facade REDs hit the exact mode-
pair guards; both pass after generalizing the native/facade pair. ImageCms
targeted passes `12/12` (`78ms`) and full `2026/2026` passes
(`9797ms`). Registrations are `997/1029`; exports remain `406/406` with
zero set difference; SHA-256 is
`55EB6CACE37F53BE334667529E4492FA23F64A50CE288D9CD853C7B82A802E6A`.

Latest `META-003D`: bounded RGB/sRGB-to-LAB/LAB profile-to-profile conversion
matches Pillow's exact 3x2 public bytes, leaves the source unchanged, returns
only a 572-byte output `icc_profile` Info entry, and performs all pixel work
in the DLL. Separate raw RED/GREEN cycles covered the missing transform and
profile serialization exports; facade RED covered public routing. ImageCms
targeted passes `10/10` (`47ms`) and full `2024/2024` passes
(`10016ms`). Registrations are `996/1028`; exports are `406/406` with
zero set difference; SHA-256 is
`E483B8741FE459F83950BE8C453B226953AB526796540A13EBE6D21A34E03085`.

Latest `META-003C`: Pillow 11.3.0 exposes exact
`XYZ identity built-in\n` and ignores non-LAB color temperature. Raw/facade
REDs failed on missing native/facade XYZ routes. Release x64 rebuilt with zero
warnings/errors; raw/facade pass `1/1` (`47ms`)/`1/1` (`16ms`),
ImageCms targeted passes `7/7` (`32ms`), and full `2021/2021` passes
(`10093ms`). Registrations are `994/1027`; exports are `404/404` with
zero set difference; SHA-256 is
`ECF493E6F7974F0130DB00CBE4215DFD4A0EC0A02C83BE022C98089218C4D0CD`.

Latest `META-003B`: Pillow 11.3.0 returns exact
`Lab identity built-in\n` for default and 6500K LAB profile requests and
rejects lowercase profile-space names. Raw RED failed on the missing LAB
create export; facade RED rejected LAB, and the case-sensitivity test exposed
AHK's ordinary string equality. Release x64 rebuilt with zero warnings/errors;
ImageCms targeted tests pass `5/5` (`125ms`), and full `2019/2019`
passes (`10282ms`). Registrations are `993/1026`; exports are `403/403`
with zero set difference; SHA-256 is
`64B5497BC1A23DD9C062B15587A14E44F7C85D91703CFC8055BDA9C8621826FB`.

Latest `META-003A`: Pillow 11.3.0 returns a core `CmsProfile` whose description
is `sRGB built-in` and whose public profile name is exact `sRGB built-in\n`.
Raw RED failed on the missing create export. Release x64 rebuilt with zero
warnings/errors; raw/facade pass `1/1` (`62ms`)/`1/1` (`31ms`), combined
`2/2` (`32ms`), and full `2016/2016` (`10000ms`). Registrations are
`992/1024`; exports are `402/402` with zero set difference; SHA-256 is
`05F61A2793F56B7673D6EDD4FD944D48D84CE2DFAAD06AF50CFC1A85421BC1E0`.

Latest `FMT-TIFF-001AX`: full-binary+Artist emits 550 bytes with offsets
`194/202`, `210`, `534`, strip `542`. Raw/facade `1/1` (`141ms`)/`1/1`
(`31ms`); TIFF `344/344`, save_all `67/67`, full `2014/2014` (`10046ms`).
Build clean; exports `399/399`; SHA-256
`053B555079CFFEDB4E0E75C350CF683BF821A082DCAE825C6CEB1879D4673E59`.

Latest `FMT-TIFF-001AW`: uncompressed single-frame `I;16B` plus DPI, ICC,
XMP, and ImageDescription writes 556 bytes with offsets `194`, `200/208`,
`216`, `540`, and strip `548`. Raw RED was `-3`; raw/facade pass `1/1`
(`156ms`) and `1/1` (`31ms`). Combined `2/2` (`47ms`), TIFF `342/342`
(`1828ms`), save_all `67/67` (`593ms`), full `2012/2012` (`10421ms`). Build
is clean; exports `399/399`; SHA-256
`4B06A3C27570CF35284C1AC1916A2B42EA13ECCB28CFDFF1BC8D999BF3F7773C`.

Latest `FMT-TIFF-001AV`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)`, XMP, ImageDescription `Hello`, Artist `Ada`, and no ICC writes a
548-byte big-endian TIFF with 15 sorted IFD entries, Description offset `194`,
inline `Ada\0`, DPI offsets `200/208`, XMP offset `216`, raw strip offset
`540`, and reopens as exact `I;16B`. Raw RED returned `-3`; native now
completes the strict uncompressed XMP+validated-one/two-ASCII family, and raw
passes `1/1` (`125ms`). Existing facade routing passed `1/1` (`47ms`) without
production changes. Combined passes `2/2` (`63ms`), TIFF `340/340` (`1625ms`),
save_all `67/67` (`578ms`), and full `2010/2010` (`9938ms`). Release x64 built
with zero warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`B4EF7519F3E16437EEB4A90224281A888D556DD3C54FF12D30334141C6919B28`.

Latest `FMT-TIFF-001AU`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)`, XMP, Artist `Ada`, and no ICC/ImageDescription writes a 530-byte
big-endian TIFF with 14 sorted IFD entries, inline `Ada\0`, DPI offsets
`182/190`, XMP offset `198`, raw strip offset `522`, and reopens as exact
`I;16B`. Raw RED returned `-3`; native now completes the strict uncompressed
XMP+one-ASCII family for tags `270`/`315`, and raw passes `1/1` (`140ms`).
Existing facade routing passed `1/1` (`32ms`) without production changes.
Combined passes `2/2` (`47ms`), TIFF `338/338` (`1657ms`), save_all `67/67`
(`563ms`), and full `2008/2008` (`10093ms`). Release x64 built with zero
warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`71B3A33D96CCBE4000477572ACCCA274542E2A65837C04B1C40048426A4AEB5E`.

Latest `FMT-TIFF-001AT`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)`, XMP, ImageDescription `Hello`, and no ICC/Artist writes a
536-byte big-endian TIFF with 14 sorted IFD entries, Description offset `182`,
DPI offsets `188/196`, XMP offset `204`, raw strip offset `528`, and reopens as
exact `I;16B`. Raw RED returned `-3`; native now admits only the strict
uncompressed DPI+XMP+tag-270 configuration, and raw passes `1/1` (`109ms`).
Existing facade routing passed `1/1` (`31ms`) without production changes.
Combined passes `2/2` (`63ms`), TIFF `336/336` (`1547ms`), save_all `67/67`
(`531ms`), and full `2006/2006` (`10234ms`). Release x64 built with zero
warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`E643014A8FF90F7DF552D598E052E359625FAEE16A0FA48DE12D6741DB6D32F0`.

Latest `FMT-TIFF-001AS`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)`, ICC, ImageDescription `Hello`, and Artist `Ada` writes a 232-byte
big-endian TIFF with 15 sorted IFD entries, Description offset `194`, inline
`Ada\0`, DPI offsets `200/208`, ICC offset `216`, raw strip offset `224`, and
reopens as exact `I;16B`. Raw RED returned `-3`; native now admits the exact
uncompressed ICC+validated-ASCII family for one or both tags, and raw passes
`1/1` (`140ms`). Existing facade routing passed `1/1` (`31ms`) without
production changes. Combined passes `2/2` (`47ms`), TIFF `334/334` (`1703ms`),
save_all `67/67` (`578ms`), and full `2004/2004` (`9703ms`). Release x64 built
with zero warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`B5A93E491DDEA00D8EC4C8DC34C39EE9434BBAFA0740071DEBFE9BDECC1E0377`.

Latest `FMT-TIFF-001AR`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)`, ICC, and Artist `Ada` writes a 214-byte big-endian TIFF with 14
sorted IFD entries, inline `Ada\0`, DPI offsets `182/190`, ICC offset `198`,
raw strip offset `206`, and reopens as exact `I;16B`. Raw RED returned `-3`;
native now admits the exact uncompressed ICC+single-ASCII family for tags
`270`/`315`, and raw passes `1/1` (`94ms`). Existing facade routing passed
`1/1` (`16ms`) without production changes. Combined passes `2/2` (`47ms`),
TIFF `332/332` (`1828ms`), save_all `67/67` (`563ms`), and full `2002/2002`
(`9766ms`). Release x64 built with zero warnings/errors; exports/DLL remain
`399/399`, and SHA-256 is
`815E6936CD5D7DC3F22268E35B3E7C144B31498654CB95D75C2DBEECF4940F43`.

Latest `FMT-TIFF-001AQ`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)`, ICC, and ImageDescription `Hello` writes a 220-byte big-endian
TIFF with 14 sorted IFD entries, Description offset `182`, DPI offsets
`188/196`, ICC offset `204`, raw strip offset `212`, and reopens as exact
`I;16B`. Raw RED returned `-3`; native now admits only the exact uncompressed
ICC+tag-270 composition and raw passes `1/1` (`157ms`). Existing facade
routing passed `1/1` (`47ms`) without production changes. Combined passes
`2/2` (`47ms`), TIFF `330/330` (`1703ms`), save_all `67/67` (`594ms`), and
full `2000/2000` (`9500ms`). Release x64 built with zero warnings/errors;
exports/DLL remain `399/399`, and SHA-256 is
`E556B39212C51BF8C5096DFD25CE04F2070863FD7FD1E58289554CB298B7850E`.

Latest `FMT-TIFF-001AP`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)`, ICC, and XMP writes a 538-byte big-endian TIFF with 14 sorted IFD
entries, DPI offsets `182/190`, XMP offset `198`, ICC offset `522`, raw strip
offset `530`, and reopens as exact `I;16B`. Raw RED returned `-3`; native now
admits DPI+ICC+XMP without ASCII and raw passes `1/1` (`125ms`). Existing
facade routing passed `1/1` (`47ms`) without production changes. Combined
passes `2/2` (`47ms`), TIFF `328/328` (`1766ms`), save_all `67/67` (`546ms`),
and full `1998/1998` (`9484ms`). Release x64 built with zero warnings/errors;
exports/DLL remain `399/399`, and SHA-256 is
`44F8A1B59900DD92247DF2A31E56B1B2263D9FC2D4CCE45837A3145840202683`.

Latest `FMT-TIFF-001AO`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)`, ImageDescription `Hello`, and Artist `Ada` writes a 212-byte big-
endian TIFF with 14 sorted IFD entries, Description offset `182`, inline
`Ada\0`, DPI offsets `188/196`, raw strip offset `204`, and reopens as exact
`I;16B`. Raw RED returned `-3`; native now admits DPI+exact tag set
`{270,315}` and raw passes `1/1` (`109ms`). Existing facade routing passed
`1/1` (`15ms`) without production changes. Combined passes `2/2` (`47ms`),
TIFF `326/326` (`1625ms`), save_all `67/67` (`609ms`), and full `1996/1996`
(`9859ms`). Release x64 built with zero warnings/errors; exports/DLL remain
`399/399`, and SHA-256 is
`03D69209EF9EB07964A9B483FC0018B4DFA966A1DF130EA6CCF9C650BB8391B7`.

Latest `FMT-TIFF-001AN`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)` and Artist `Ada` writes a 194-byte big-endian TIFF with 13 sorted
IFD entries, inline `Ada\0`, DPI offsets `170/178`, raw strip offset `186`, and
reopens as exact `I;16B`. Raw RED returned `-3`; native now admits DPI+tag-315
and raw passes `1/1` (`93ms`). Facade RED wrote the 182-byte DPI-only file;
single-frame `TiffInfo[315]` plus DPI now routes through the metadata-ASCII ABI
and passes `1/1` (`15ms`). Combined passes `2/2` (`47ms`), TIFF `324/324`
(`1562ms`), save_all `67/67` (`562ms`), and full `1994/1994` (`9922ms`).
Release x64 built with zero warnings/errors; exports/DLL remain `399/399`, and
SHA-256 is
`4D501BCA2D12C97AA656339A8AE2BC60066303C2DECCAAB9BB2AFB2AD89C9011`.

Latest `FMT-TIFF-001AM`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)` and ImageDescription `Hello` writes a 200-byte big-endian TIFF
with 13 sorted IFD entries, Description offset `170`, DPI offsets `176/184`,
raw strip offset `192`, and reopens as exact `I;16B`. Raw RED returned `-3`;
native now admits DPI+tag-270 and raw passes `1/1` (`94ms`). Facade RED wrote
the 182-byte DPI-only file; single-frame `TiffInfo[270]` plus DPI now routes
through the metadata-ASCII ABI and passes `1/1` (`31ms`). Combined passes
`2/2` (`31ms`), TIFF `322/322` (`1500ms`), save_all `67/67` (`562ms`), and
full `1992/1992` (`9547ms`). Release x64 built with zero warnings/errors;
exports/DLL remain `399/399`, and SHA-256 is
`94ED2CE0CA2151A47DA4C2BFDE0ED922D717908E4BE4FD384E7F7A812C46F6DD`.

Latest `FMT-TIFF-001AL`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)` and XMP writes a 518-byte big-endian TIFF with 13 sorted IFD
entries, DPI offsets `170/178`, BYTE XMP offset `186`, raw strip offset `510`,
and reopens as exact `I;16B`. Raw RED returned `-3`; native now admits DPI+XMP
and raw passes `1/1` (`78ms`). Facade RED wrote the 182-byte DPI-only file;
single-frame `TiffInfo[700]` plus DPI now routes through the metadata ABI and
passes `1/1` (`16ms`). Combined passes `2/2` (`47ms`), TIFF `320/320`
(`1563ms`), save_all `67/67` (`578ms`), and full `1990/1990` (`9532ms`).
Release x64 built with zero warnings/errors; exports/DLL remain `399/399`, and
SHA-256 is
`6E523F767A679733489B27668FBF5AEB4278A5E57C02C63655F47FF154AD4AE8`.

Latest `FMT-TIFF-001AK`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)` and ICC writes a 202-byte big-endian TIFF with 13 sorted IFD
entries, DPI offsets `170/178`, ICC offset `186`, raw strip offset `194`, and
reopens as exact `I;16B`. Raw RED returned `-3`; native now admits DPI+ICC and
raw passes `1/1` (`109ms`). Facade RED wrote the 182-byte DPI-only file;
metadata routing now preserves ICC and passes `1/1` (`15ms`). Combined passes
`2/2` (`47ms`), TIFF `318/318` (`1579ms`), save_all `67/67` (`578ms`), and
full `1988/1988` (`9422ms`). Release x64 built with zero warnings/errors;
exports/DLL remain `399/399`, and SHA-256 is
`E2759F99E0F9B50F1ADCFCAF0B2BB85D2DF7D06A0EBE29BED43BE27EDF097BB3`.

Latest `FMT-TIFF-001AJ`: one uncompressed single-frame `I;16B` image with DPI
`(300,150)` writes a 182-byte big-endian TIFF with 12 sorted IFD entries,
RATIONAL offsets `158/166`, raw strip offset `174`, and reopens as exact
`I;16B`. Raw RED failed `0/1` (`16ms`) with status `-3` at the uncompressed
configuration guard. Native now admits DPI-only through the AI big-endian
writer. Raw/facade pass `1/1` (`78ms`) and `1/1` (`16ms`); combined `2/2`
(`31ms`), TIFF `316/316` (`1469ms`), save_all `67/67` (`515ms`), and full
`1986/1986` (`9391ms`). Release x64 built with zero warnings/errors; exports/
DLL remain `399/399`, and SHA-256 is
`62A84B20D6EC34EFFB944B0A7E73303496FC8A1AD395511F7CA5B98321977598`.

Latest `FMT-TIFF-001AI`: one uncompressed single-frame `I;16B` image composes
DPI `(300,150)`, ICC, XMP, ImageDescription, and Artist in a 568-byte big-
endian TIFF and reopens as exact `I;16B`. Raw RED failed `0/1` (`16ms`) with
status `-3` at the uncompressed metadata guard; the extended big-endian writer
passes raw `1/1` (`94ms`). Facade RED then exposed single-frame metadata
bypassing `SaveTiffFrames`; the exact full-composition route now passes `1/1`
(`15ms`). Combined passes `2/2` (`47ms`), TIFF `314/314` (`1469ms`), save_all
`67/67` (`609ms`), and full `1984/1984` (`9313ms`). Release x64 built with
zero warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`8D23C9F10BCA13E3AA63C9D703C53D70127E2F8D9AE1E128474D01845D7E6CA9`.

Latest `FMT-TIFF-001AH`: one homogeneous two-frame `I;16B` pair composes LZW,
DPI `(300,150)`, ICC, XMP, ImageDescription, and Artist in both IFDs and
facade seek states. Raw RED failed `0/1` (`16ms`) with status `-3` at the
binary-metadata plus ASCII I16B guard. Native now admits only the exact full
combination while retaining partial metadata+ASCII and uncompressed-metadata
rejection. Raw/facade pass `1/1` (`157ms`) and `1/1` (`16ms`); combined `2/2`
(`63ms`), TIFF `312/312` (`1532ms`), save_all `67/67` (`547ms`), and full
`1982/1982` (`9641ms`). Release x64 built with zero warnings/errors; exports/
DLL remain `399/399`, and SHA-256 is
`CA98C6A7BA635438C202FBC89EE7F341204E4995313FA2A2424BE437693D2063`.

Latest `FMT-TIFF-001AG`: one homogeneous two-frame `I;16B` pair composes LZW,
DPI `(300,150)`, ImageDescription `Hello\0`, and Artist `Ada\0` in both IFDs
and facade seek states. Raw RED failed `0/1` (`16ms`) with status `-3` at the
count-two I16B guard. Native now admits the exact validated pair `{270,315}`;
ICC/XMP+ASCII and uncompressed metadata remain rejected. Raw/facade pass
`1/1` (`157ms`) and `1/1` (`32ms`); combined `2/2` (`62ms`), TIFF `310/310`
(`1547ms`), save_all `66/66` (`562ms`), and full `1980/1980` (`9938ms`).
Release x64 built with zero warnings/errors; exports/DLL remain `399/399`, and
SHA-256 is
`703A00252FCEDF2C6244E6AEBFC18E9C0891A0A394C895EC046BFE0DB3F77067`.

Latest `FMT-TIFF-001AF`: one homogeneous two-frame `I;16B` pair composes LZW,
DPI `(300,150)`, and exact inline Artist `Ada\0` in both IFDs and facade seek
states. Raw RED failed `0/1` (`31ms`) with status `-3` at the tag-315 guard.
Native now admits either single ASCII tag `270` or `315`. Raw/facade pass
`1/1` (`125ms`) and `1/1` (`31ms`); combined `2/2` (`47ms`), TIFF `308/308`
(`1485ms`), save_all `65/65` (`515ms`), and full `1978/1978` (`9829ms`).
Release x64 built with zero warnings/errors; exports/DLL remain `399/399`, and
SHA-256 is
`36F2542CA039D955BFA5B47D1BD41A3C31380092BE9DDEFD65E02AE3A7514F30`.

Latest `FMT-TIFF-001AE`: one homogeneous two-frame `I;16B` pair composes LZW,
DPI `(300,150)`, and exact ImageDescription `Hello\0` in both IFDs and facade
seek states. Raw RED failed `0/1` (`32ms`) with status `-3` at the ASCII guard.
Native now routes tag-270-only through normalization and explicitly rejects
metadata on the uncompressed writer. Raw/facade pass `1/1` (`125ms`) and
`1/1` (`31ms`); combined `2/2` (`47ms`), TIFF `306/306` (`1468ms`), save_all
`64/64` (`562ms`), and full `1976/1976` (`9750ms`). Release x64 built with
zero warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`A80222381976DA923E3EE4EFDF3FD8D1DB1E30FD61B002C4C0E472B800372FB0`.

Latest `FMT-TIFF-001AD`: one homogeneous two-frame `I;16B` pair composes LZW,
DPI `(300,150)`, exact ICC, and exact XMP in both IFDs and both facade seek
states. Raw RED failed `0/1` (`16ms`) with status `-3` at the explicit native
combined-payload guard. Removing only that guard lets the shared writer own
both layouts while preserving source handles. Raw/facade pass `1/1` (`109ms`)
and `1/1` (`32ms`); combined `2/2` (`63ms`), TIFF `304/304` (`1500ms`),
save_all `63/63` (`500ms`), and full `1974/1974` (`9688ms`). Release x64
built with zero warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`629DD662C6ACDCA4A8805493FF0ED866274533D370279DBF05C9D06CD84D3AEA`.

Latest `FMT-TIFF-001AC`: one homogeneous two-frame `I;16B` pair composes LZW,
DPI `(300,150)`, and an exact 324-byte XMP tag-700 packet in both IFDs and both
facade seek states. Raw RED failed `0/1` (`31ms`) with status `-3` at the
native XMP guard. Native now passes XMP-only through temporary little-endian
normalization into the shared writer while retaining ICC+XMP rejection. Raw/
facade pass `1/1` (`172ms`) and `1/1` (`47ms`); combined `2/2` (`62ms`), TIFF
`302/302` (`1500ms`), save_all `62/62` (`500ms`), and full `1972/1972`
(`9656ms`). Release x64 built with zero warnings/errors; exports/DLL remain
`399/399`, and SHA-256 is
`29770E7CAC1A59C4D6455405A9A7A39F54A8DFD517E4D5D6A607F9D9C4C7365A`.

Latest `FMT-TIFF-001AB`: one homogeneous two-frame `I;16B` pair composes LZW,
DPI `(300,150)`, and exact ICC bytes in both IFDs and both facade seek states.
Raw RED failed `0/1` (`15ms`) with status `-3` at the native I16B metadata
guard. Native now passes validated ICC through temporary little-endian
normalization into the shared writer while preserving source handles. Raw/
facade pass `1/1` (`94ms`) and `1/1` (`31ms`); combined `2/2` (`47ms`), TIFF
`300/300` (`1563ms`), save_all `61/61` (`484ms`), and full `1970/1970`
(`9422ms`). Release x64 built with zero warnings/errors; exports/DLL remain
`399/399`, and SHA-256 is
`58D1896D7F7C6B99AB68D2F1BB1D8D027512F011BA7DEC793E9C90D92990E30F`.

Latest `FMT-TIFF-001AA`: both mixed two-frame orders, `I;16` then `I;16B` and
the reverse, compose DPI `(300,150)` with PackBits, LZW, and Adobe Deflate.
Raw RED failed `0/1` (`32ms`) with status `-3` because Z required every frame
to be `I;16B`. Native now recognizes the whole 16-bit endian family and swaps
only big-endian members. Raw/facade pass `1/1` (`250ms`) and `1/1` (`63ms`);
combined `2/2` (`93ms`), TIFF `298/298` (`1500ms`), save_all `60/60`
(`469ms`), and full `1968/1968` (`9312ms`). Both source orderings remain
unchanged; PackBits/LZW strips are exact and native Deflate uses valid
19-byte stored blocks. Release x64 built with zero warnings/errors;
exports/DLL remain `399/399`, and SHA-256 is
`3C90BAEA740D3DA52D91891777F6E39E3994A57B1752584C362D5EBF8B4ECF2F`.

Latest `FMT-TIFF-001Z`: batched same-size mode `I;16B` two-frame fixtures
compose DPI `(300,150)` with PackBits, LZW, and Adobe Deflate. Raw RED failed
`0/1` (`31ms`) with native status `-3` because any `I;16B` route rejected
multiframe or DPI. Native now copies and byte-swaps every homogeneous compressed
frame to temporary `I;16` storage while preserving source handles. Raw/facade
pass `1/1` (`156ms`) and `1/1` (`47ms`); combined `2/2` (`63ms`), TIFF
`296/296` (`1422ms`), save_all `59/59` (`437ms`), and full `1966/1966`
(`9516ms`). PackBits/LZW strips match Pillow at `10/10` and `12/12` bytes;
native Deflate uses valid `19/19`-byte stored blocks versus Pillow's `16/16`.
Release x64 built with zero warnings/errors; exports/DLL remain `399/399`,
and SHA-256 is
`395E0D5D1CE3394556031B6C25C5A77FA380361927D26DC4A3064568278072CB`.

Latest `FMT-TIFF-001Y`: batched same-size mode `I` and mode `F` two-frame
fixtures compose DPI `(300,150)` with PackBits, LZW, and Adobe Deflate. Raw
RED failed `0/1` (`31ms`) because the first mode-I PackBits strip encoded a
two-zero run as a four-byte literal. The native PackBits state machine now
matches libtiff/Pillow's conditional two-byte-run strategy. Raw/facade pass
`1/1` (`219ms`) and `1/1` (`62ms`); combined `2/2` (`94ms`), TIFF `294/294`
(`1375ms`), save_all `58/58` (`438ms`), and full `1964/1964` (`9625ms`).
LZW and PackBits strips are exact for the bounded fixtures; native Deflate
uses valid 27-byte stored blocks while preserving exact samples and DPI.
Release x64 built with zero warnings/errors; exports/DLL remain `399/399`,
and SHA-256 is
`F877A0057B8D54ACD2DD11A32622BDCE37BD22D185E222862E9B5AB00E2E7CBB`.

Latest `FMT-TIFF-001X`: batched same-size little-endian `I;16` two-frame
fixtures compose DPI `(300,150)` with LZW and Adobe Deflate. LZW matches
Pillow's exact 12-byte strips; native Deflate's documented valid stored block
is 19 bytes versus Pillow's 16, with exact decoded samples and DPI. Existing
raw/facade routes passed `1/1` (`109ms`) and `1/1` (`47ms`); combined `2/2`
(`47ms`), TIFF `292/292` (`1219ms`), save_all `57/57` (`375ms`), and full
`1962/1962` (`9468ms`). This was coverage-only; no production change or
rebuild. Exports/DLL remain `399/399`, and SHA-256 remains
`A61C6E56CA121828C29F19222157E950924D9BC2290BFAC2737E756409E003F5`.

Latest `FMT-TIFF-001W`: one same-size little-endian `I;16` two-frame fixture
composes PackBits and DPI `(300,150)`. Pillow 11.3.0 writes Compression
`32773`, RowsPerStrip `2`, and exact ten-byte row strips in both IFDs. Existing
raw/facade routes passed directly `1/1` (`62ms`) and `1/1` (`31ms`); combined
passed `2/2` (`46ms`), TIFF `290/290` (`1125ms`), save_all `56/56` (`359ms`),
and full `1960/1960` (`9422ms`). This was coverage-only; no production change
or rebuild. Exports/DLL remain `399/399`, and SHA-256 remains
`A61C6E56CA121828C29F19222157E950924D9BC2290BFAC2737E756409E003F5`.

Latest `FMT-TIFF-001V`: the T mode `I` and mode `F` two-frame DPI fixtures
are opened at frame `0` without a seek. Batched raw RED failed `0/1` (`47ms`)
with initial `HasDpi == 0`; adding IFD0 resolution parsing to the shared early
numeric return made raw GREEN `1/1` (`157ms`). Facade passed `1/1` (`31ms`),
combined `2/2` (`47ms`), TIFF `288/288` (`1172ms`), save_all `55/55`
(`359ms`), and full `1958/1958` (`9641ms`). Release x64 built with zero
warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`A61C6E56CA121828C29F19222157E950924D9BC2290BFAC2737E756409E003F5`.

Latest `FMT-TIFF-001U`: the S little-endian `I;16` two-frame DPI fixture is
opened at frame `0` without a seek. Raw RED failed `0/1` (`62ms`) with initial
`HasDpi == 0`; adding IFD0 resolution parsing to the early `I;16` return made
raw GREEN `1/1` (`79ms`). Facade passed `1/1` (`16ms`), combined `2/2`
(`47ms`), TIFF `286/286` (`1188ms`), save_all `54/54` (`359ms`), and full
`1956/1956` (`9109ms`). Release x64 built with zero warnings/errors;
exports/DLL remain `399/399`, and SHA-256 is
`0C23441D2E57C4300FEB6025D3A3DDF9F7C21786618928D30705246EBF5581FE`.

Latest `FMT-TIFF-001T`: batched mode `I` and mode `F` same-size two-frame
fixtures use `save_all` plus DPI `(300,150)`. Pillow 11.3.0 preserves exact
signed-int32/float32 bytes and exposes `300/1`, `150/1`, unit `2`, and DPI
Info on both frames. Raw RED failed `0/1` (`47ms`) with selected-frame
`HasDpi == 0`; the native early numeric selected-IFD assignment made raw
GREEN `1/1` (`156ms`). Facade passed `1/1` (`31ms`), combined `2/2` (`47ms`),
TIFF `284/284` (`1046ms`), save_all `53/53` (`297ms`), and full `1954/1954`
(`8938ms`). Release x64 built with zero warnings/errors; exports/DLL remain
`399/399`, and SHA-256 is
`2AA796C0723CB49838E4D899D42BF9725F00F4A1F603659C0595A9EFF32040D4`.

Latest `FMT-TIFF-001S`: one same-size little-endian `I;16` two-frame fixture
uses `save_all`, one append image, and DPI `(300,150)`. Pillow 11.3.0 preserves
both exact byte arrays and exposes `300/1`, `150/1`, unit `2`, and
`Info["dpi"]` on both frames. Raw RED failed `0/1` (`47ms`) with selected
frame `HasDpi == 0`; the native selected-IFD resolution assignment made raw
GREEN `1/1` (`63ms`). Facade GREEN passed `1/1` (`32ms`), combined passed
`2/2` (`31ms`), TIFF `282/282` (`1469ms`), save_all `52/52` (`297ms`), and
full `1952/1952` (`9078ms`). Release x64 built with zero warnings/errors;
exports/DLL remain `399/399`, and SHA-256 is
`906397FBD41F7BA0CB19A2B6A08BCF3582CF419C80E1041A47C366D6C447DA20`.

Latest `FMT-TIFF-001R`: one RGB two-frame fixture composes LZW, DPI
`(300,150)`, ICC, XMP, ImageDescription, and Artist in both IFDs and facade
seek states while preserving exact pixels. Raw RED found selected frame DPI
was absent because native resolution parsing was restricted to IFD0; the
selected-IFD parser fix made raw GREEN `1/1` (`125ms`). Facade passed `1/1`
(`15ms`) through the existing generalized route; TIFF `280/280` (`1187ms`),
save_all `51/51` (`328ms`), and full `1950/1950` (`8938ms`) passed. Release
x64 built cleanly; exports/DLL remain `399/399` and
`6E658A0051D5E8BF7346691872B39A176B1C13F7DB1F0B2F8CF2498B2DFF7997`.

Latest `FMT-TIFF-001Q`: one `TiffInfo` Map writes ImageDescription `270` as
ASCII count `6` with out-of-line `Hello\0` and Artist `315` as ASCII count `4`
with inline `Ada\0` in both RGB save_all IFDs; both frames reopen with exact
pixels and both GetExif values. Raw/facade REDs found a missing array export
and the singular facade restriction. Final raw/facade passed `1/1` (`141ms`)
and `1/1` (`16ms`); ASCII raw `3/3` (`172ms`), tiffinfo facade `4/4`
(`78ms`), TIFF `278/278` (`1219ms`), save_all `50/50` (`360ms`), and full
`1948/1948` (`9250ms`) passed. Release x64 built cleanly; exports/DLL are
`399/399` and
`DB5C38C111E322C1BDBB66B9643C2E85CF54C1A3BC991BA5D1AFDF98CBA4291A`.

Latest `FMT-TIFF-001P`: `TiffInfo` Artist tag `315` with value `"Ada"`
writes ASCII type `2`, NUL-inclusive count `4`, and exact inline bytes
`41 64 61 00` in both RGB save_all IFDs; both frames reopen with exact pixels
and `GetExif()[315] == "Ada"`. Raw/facade REDs found native and facade tag
rejection. Final raw/facade passed `1/1` (`172ms`) and `1/1` (`16ms`); ASCII
raw `2/2` (`125ms`), tiffinfo facade `3/3` (`63ms`), TIFF `276/276`
(`1078ms`), save_all `49/49` (`343ms`), and full `1946/1946` (`9250ms`)
passed. Release x64 built cleanly; exports/DLL remain `398/398` and
`D0B49F66FA232749D7E22AAB39D2CEED838D7B48AF01D4E890A1F1F4A2401B0F`.

Latest `FMT-TIFF-001O`: `TiffInfo` ImageDescription tag `270` writes exact
ASCII type `2`, count `6`, and `Hello\0` bytes into both RGB save_all IFDs;
both frames reopen with exact pixels and `GetExif()[270] == "Hello"`. Raw RED
found the missing ASCII metadata export and facade RED found tag `270`
rejection. Final raw/facade passed `1/1` (`140ms`) and `1/1` (`16ms`); TIFF
`274/274` (`1250ms`), save_all `48/48` (`265ms`), and full `1944/1944`
(`9453ms`) passed. Release x64 built cleanly; exports/DLL are `398/398` and
`95E388362A7930CDC95CA49B03C7F4867DB0AE983D32803D322D9601005E9C85`.

Latest `FMT-TIFF-001N`: explicit ICC plus `TiffInfo` XMP coexist in both RGB
save_all IFDs and reopen through Info/GetExif after both seeks. Existing
raw/facade passed `1/1` (`94ms`) and `1/1` (`47ms`); TIFF `272/272`
(`1109ms`), save_all `47/47` (`282ms`), and full `1942/1942` (`8765ms`)
passed. No rebuild was required; exports/DLL remain `397/397` and
`D2F771BCFDD56D6EB70993FCC503DD60E77350CCFC1EC61A998D80CD6DF50EBB`.

Latest `FMT-TIFF-001M`: `TiffInfo` tag `700` writes exact 324-byte XMP into
both RGB save_all IFDs and reopens through Info/GetExif after both seeks;
direct TIFF `xmp=` is ignored like Pillow. Raw/facade REDs found the missing
extended metadata export and ignored tiffinfo; final raw/facade passed `1/1`
(`156ms`) and `1/1` (`47ms`). TIFF `270/270` (`1172ms`), save_all `46/46`
(`266ms`), and full `1940/1940` (`9468ms`) passed. Release x64 built cleanly;
exports/DLL are `397/397` and
`D2F771BCFDD56D6EB70993FCC503DD60E77350CCFC1EC61A998D80CD6DF50EBB`.

Latest `FMT-TIFF-001L`: explicit ICC bytes on RGB two-frame save_all are
written as UNDEFINED tag `34675` in both IFDs and reopen through Info/GetExif
with embedded NUL preserved. Raw/facade REDs found the missing metadata export
and ignored public option; final raw/facade passed `1/1` (`141ms`) and `1/1`
(`47ms`). TIFF `268/268` (`1140ms`), save_all `45/45` (`281ms`), and full
`1938/1938` (`9078ms`) passed. Release x64 built cleanly; exports/DLL are
`396/396` and
`684BF0C50053C043CD3127C22C0772F907C94F440589C429D6A7BAE14DC25D0C`.

Latest `FMT-TIFF-001K`: the generalized two-frame options route composes DPI
with LZW and Adobe Deflate. Compression and all DPI tags/Info plus exact
decoded bytes match Pillow; LZW strips are 14 bytes, while native Deflate's
documented valid stored-block route remains 23 bytes versus Pillow's 20.
Existing raw/facade passed `1/1` (`78ms`) and `1/1` (`63ms`); TIFF `266/266`
(`953ms`), save_all `44/44` (`218ms`), and full `1936/1936` (`8734ms`)
passed. No rebuild was required; exports/DLL remain `395/395` and
`F347533B35A6E6AE3243247AEF5639F7C24CDBB8CDF5062987ABB61A96C7832F`.

Latest `FMT-TIFF-001J`: the generalized two-frame options route composes
PackBits and `dpi=(300,150)`. Both IFDs carry Compression `32773`, 14-byte
strips, XResolution `300/1`, YResolution `150/1`, and ResolutionUnit `2`;
decoded bytes and facade DPI/GetExif match Pillow. Existing production passed
raw/facade `1/1` (`78ms`) and `1/1` (`31ms`); TIFF `264/264` (`1204ms`),
save_all `43/43` (`250ms`), and full `1934/1934` (`9140ms`) passed. No rebuild
was required; exports/DLL remain `395/395` and
`F347533B35A6E6AE3243247AEF5639F7C24CDBB8CDF5062987ABB61A96C7832F`.

Latest `FMT-TIFF-001I`: one RGB base plus one RGB append frame saved with
`dpi=(300,150)` carries XResolution `300/1`, YResolution `150/1`, and
ResolutionUnit `2` in both IFDs, preserves exact decoded bytes, and refreshes
facade DPI/GetExif after both seeks. Raw RED found a missing generalized
multiframe options export; facade RED proved the former route ignored DPI.
Raw/facade passed `1/1` (`156ms`) and `1/1` (`47ms`); TIFF `262/262`
(`1093ms`), save_all `42/42` (`219ms`), and full `1932/1932` (`9156ms`)
passed. Release x64 builds with zero warnings/errors; exports/DLL are
`395/395` and
`F347533B35A6E6AE3243247AEF5639F7C24CDBB8CDF5062987ABB61A96C7832F`.

Latest `FMT-TIFF-001H`: the same RGB two-frame save_all route handles
`tiff_lzw` and `tiff_adobe_deflate`. Pillow LZW writes Compression `5` and
14-byte strips; Adobe Deflate writes Compression `8`, zlib `78 9C`, and
20-byte strips. Native preserves all decoded bytes and the tags/prefix; its
documented stored-block Deflate strategy uses 23-byte strips. Raw/facade passed
`1/1` (`94ms`) and `1/1` (`31ms`); TIFF `260/260` (`1125ms`), save_all `41/41`
(`281ms`), and full `1930/1930` (`9094ms`) passed. No rebuild was required;
exports/DLL remain `394/394` and
`2C5F6E6929FB0900F8E70D2FF5F2E6095A6D242ACCAE395B6318F7D5A1FFF29F`.

Latest `FMT-TIFF-001G`: Pillow 11.3.0 writes both RGB `2x2` frames with
Compression `32773`, RowsPerStrip `2`, StripByteCounts `14`, and exact decoded
bytes. Raw RED failed on the missing multiframe compression export; facade RED
wrote Compression `1`. The new native export reuses compression normalization
and frame-vector encoding; facade only routes the normalized option. Final raw
and facade passed `1/1` (`93ms`) and `1/1` (`31ms`); TIFF `258/258` (`921ms`),
save_all `40/40` (`187ms`), and full `1928/1928` (`8938ms`) passed. Release
x64 rebuilt with zero warnings/errors; exports/DLL are `394/394` and
`2C5F6E6929FB0900F8E70D2FF5F2E6095A6D242ACCAE395B6318F7D5A1FFF29F`.

Latest `FMT-TIFF-001F`: Pillow 11.3.0 preserves one RGB base plus two RGB
append frames with exact `2x1` bytes. Raw parsing proves two nonzero next-IFD
links followed by a zero terminator; facade `AppendImages` accepts both images
and seeks all three frames. Existing native/facade routes passed without
production changes: combined `2/2` (`47ms`), TIFF `256/256` (`1062ms`),
save_all `39/39` (`188ms`), and full `1926/1926` (`9250ms`). No native rebuild
was required; exports/DLL remain `393/393` and
`2189246D2C8C390C236DD4595A909BF72084DF8448C430A1216FC095A2D3A06A`.

Latest `FMT-TIFF-001E`: Pillow 11.3.0 preserves an RGB `2x1` base plus RGB
`1x2` append frame with exact bytes. The second IFD writes Width `1`, Height
and RowsPerStrip `2`, the standard three-sample RGB layout, and a six-byte
strip; facade seek updates `Size` to `[1,2]`. Existing native/facade routes
passed without production changes: combined `2/2` (`31ms`), TIFF `254/254`
(`1078ms`), save_all `38/38` (`188ms`), and full `1924/1924` (`8907ms`). No
native rebuild was required; exports/DLL remain `393/393` and
`2189246D2C8C390C236DD4595A909BF72084DF8448C430A1216FC095A2D3A06A`.

Latest `FMT-TIFF-001D`: Pillow 11.3.0 preserves a same-size L base plus RGB
append frame as `L` then `RGB` with exact bytes. The L IFD has one 8-bit
grayscale sample, omits SamplesPerPixel, writes PlanarConfiguration `1`, and
stores a two-byte strip; the RGB IFD carries `(8,8,8)`, photometric `2`, three
samples, and a six-byte strip. Raw RED failed `0/1` on the unexpected native
L SamplesPerPixel entry; the shared native condition also omitted L
PlanarConfiguration. After correcting layout counting and entry emission,
combined raw/facade passed `2/2` (`62ms`), TIFF `252/252` (`1000ms`),
save_all `37/37` (`172ms`), and full `1922/1922` (`8891ms`). Release x64
rebuilt with zero warnings/errors; exports/DLL remain `393/393` and
`2189246D2C8C390C236DD4595A909BF72084DF8448C430A1216FC095A2D3A06A`.

Latest `FMT-TIFF-001C`: Pillow 11.3.0 writes a bounded same-size RGBA base and
append frame as two little-endian uncompressed TIFF IFDs. Both reopen as
`RGBA 2x1` with exact interleaved bytes; each IFD has BitsPerSample
`(8,8,8,8)`, Compression `1`, PhotometricInterpretation `2`, SamplesPerPixel
`4`, RowsPerStrip `1`, StripByteCounts `8`, PlanarConfiguration `1`, and
ExtraSamples `2`. Existing DLL/facade routes passed without production
changes: combined raw/facade `2/2` (`46ms`), TIFF `250/250` (`1079ms`),
save_all `36/36` (`188ms`), and full `1920/1920` (`8953ms`). No native rebuild
was required; exports/DLL remain `393/393` and
`BE715A737810DC3D43799A6775D4E629D3B0AB1582F7F6E06A22CF18C3B20EE3`.

Latest `FMT-TIFF-001B`: Pillow 11.3.0 writes a bounded same-size RGB base and
append frame as two little-endian uncompressed TIFF IFDs. Both reopen as
`RGB 2x1` with exact interleaved bytes; each IFD has BitsPerSample `(8,8,8)`,
Compression `1`, PhotometricInterpretation `2`, SamplesPerPixel `3`,
RowsPerStrip `1`, StripByteCounts `6`, and PlanarConfiguration `1`. The first
raw harness run exposed a test-only end-index/length mistake; after correcting
that assertion, the existing DLL/facade routes passed without production
changes. Combined raw/facade passed `2/2` (`32ms`), TIFF `248/248` (`1047ms`),
save_all `35/35` (`172ms`), and the JSON-backed full run `1918/1918`
(`9188ms`). No native rebuild was required; exports/DLL remain `393/393` and
`BE715A737810DC3D43799A6775D4E629D3B0AB1582F7F6E06A22CF18C3B20EE3`.

Latest `FMT-JPEG-003AZ`: Pillow 11.3.0 uses the last value for duplicate
ordinary Photoshop resource code `0x0404`, exposing one entry with bytes
`CD`, then drops APP13 from both keep saves while preserving DQT/RGB/size.
The raw RED failed `0/1` (`47ms`) with `Expected 1, got 2`; after native
deduplication the raw test passed `1/1` (`93ms`) and facade passed `1/1`
(`46ms`). Duplicate-Photoshop/Photoshop/APP13/open_jpeg/quality-keep/qtables-
keep/JPEG filters passed `2/2` (`47ms`), `10/10` (`94ms`), `6/6` (`93ms`),
`29/29` (`968ms`), `57/57` (`953ms`), `76/76` (`750ms`), and `399/399`
(`5235ms`). The JSON-backed full run passed `1916/1916` in `8578ms`.
Release x64 rebuilt with zero warnings/errors; exports/DLL remain `393/393`
and `BE715A737810DC3D43799A6775D4E629D3B0AB1582F7F6E06A22CF18C3B20EE3`.

Latest `FMT-JPEG-003AY`: Pillow 11.3.0 merges two separately recognized
Photoshop APP13 markers into one map, retaining ordinary `1028: b"AB"` from
the first and structured ResolutionInfo `1005: {...}` from the second. Both
keep saves drop all APP13 markers and preserve DQT/RGB/size. Existing raw and
facade routes passed directly `1/1` (`46ms`) and `1/1` (`31ms`) without a
production change or rebuild. Photoshop/APP13/open_jpeg/quality-keep/qtables-
keep/JPEG filters passed `8/8` (`94ms`), `6/6` (`78ms`), `28/28` (`922ms`),
`56/56` (`1031ms`), `75/75` (`719ms`), and `397/397` (`5390ms`). The JSON-
backed full run passed `1914/1914` in `8844ms`. Exports/DLL remain `393/393`
and `114E2F544DA1848E35D828EEC5246075C11753945B440A57ADC26360D76510B6`.

Latest `FMT-JPEG-003AX`: Pillow 11.3.0 composes one ordinary Photoshop
resource `1028: b"AB"` and ResolutionInfo `1005: {...}` in the same map, then
drops APP13 from both keep saves while preserving DQT/RGB/size. Existing raw
and facade routes passed directly `1/1` (`62ms`) and `1/1` (`47ms`) without a
production change or rebuild. ResolutionInfo/Photoshop/open_jpeg/quality-keep/
qtables-keep/JPEG filters passed `4/4` (`63ms`), `6/6` (`78ms`), `27/27`
(`891ms`), `55/55` (`938ms`), `74/74` (`718ms`), and `395/395` (`5375ms`).
The JSON-backed full run passed `1912/1912` in `8765ms`. Exports/DLL remain
`393/393` and
`114E2F544DA1848E35D828EEC5246075C11753945B440A57ADC26360D76510B6`.

Latest `FMT-JPEG-003AW`: Pillow 11.3.0 exposes Photoshop ResolutionInfo code
`0x03ED` as a nested map with `XResolution=300.5`, `DisplayedUnitsX=1`,
`YResolution=150.25`, and `DisplayedUnitsY=3`; both keep saves omit APP13
while preserving DQT/RGB/size. Raw/facade REDs failed `0/1` (`47ms`) and
`0/1` (`32ms`); final raw/facade passed `1/1` (`94ms`) and `1/1` (`31ms`).
ResolutionInfo/Photoshop/APP13/open_jpeg/quality-keep/qtables-keep/JPEG
filters passed `2/2` (`47ms`), `4/4` (`62ms`), `4/4` (`78ms`), `26/26`
(`813ms`), `54/54` (`953ms`), `73/73` (`703ms`), and `393/393` (`5234ms`).
The JSON-backed full run passed `1910/1910` in `9078ms`. Release x64 rebuilt
with zero warnings/errors; exports/DLL are `393/393` and
`114E2F544DA1848E35D828EEC5246075C11753945B440A57ADC26360D76510B6`.

Latest `FMT-JPEG-003AV`: Pillow 11.3.0 exposes one ordinary Photoshop APP13
resource as `{1028: b"AB"}` and drops APP13 from both keep-save outputs.
Native parser/state plus two enumerable ABI exports and facade Map routing
closed raw/facade REDs `0/1` (`31ms`) and `0/1` (`31ms`); final raw/facade
passed `1/1` (`93ms`) and `1/1` (`31ms`). Photoshop/APP13/open_jpeg/quality-
keep/qtables-keep/JPEG filters passed `2/2` (`47ms`), `4/4` (`62ms`), `25/25`
(`734ms`), `53/53` (`968ms`), `72/72` (`688ms`), and `391/391` (`5313ms`).
The JSON-backed full run passed `1908/1908` in `8531ms`. Release x64 rebuilt
with zero warnings/errors; exports/DLL are `392/392` and
`1C0EB831B28942B7A9579B655DA77AAD258AF5CFE694CDBBE65D426A991F19FD`.

Latest `FMT-JPEG-003AU`: Pillow 11.3.0 exposes one unknown pre-DQT APP13 in
`applist`, then omits it from both public keep-save outputs while preserving
DQT, RGB mode, and `16x8` size. Existing native/facade routes passed raw/facade
`1/1` (`62ms`) and `1/1` (`31ms`) without production changes. APP13/quality-
keep/qtables-keep/JPEG filters passed `2/2` (`47ms`), `52/52` (`890ms`),
`71/71` (`640ms`), and `389/389` (`5219ms`). The JSON-backed full run passed
`1906/1906` in `9063ms`. Exports/DLL remain `390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Latest `META-002AE`: Pillow 11.3.0 keeps the ICC key and assigns `None` for
two-marker fragments `1/0:A + 2/0:B` / `1/255:A + 2/255:B`. Existing native
state/blob routing and facade None mapping passed raw/facade `1/1` (`63ms`)
and `1/1` (`32ms`) without production changes. ICC/open_jpeg/JPEG filters
passed `91/91` (`735ms`), `23/23` (`734ms`), and `387/387` (`5032ms`). The
JSON-backed full run passed `1904/1904` in `9250ms`. Exports/DLL remain
`390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Latest `META-002AD`: Pillow 11.3.0 keeps the ICC key and assigns `None` for
singleton fragments `1/0:A` / `1/255:B`. Existing native state/blob routing
and facade None mapping passed raw/facade `1/1` (`62ms`) and `1/1` (`32ms`)
without production changes. ICC/open_jpeg/JPEG filters passed `89/89`
(`703ms`), `22/22` (`828ms`), and `385/385` (`5328ms`). The JSON-backed full
run passed `1902/1902` in `8578ms`. Exports/DLL remain `390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Latest `META-002AC`: Pillow 11.3.0 exposes exact ICC bytes `A` / `B` for
singleton fragments `2/1:A` / `255/1:B`. Existing native state/blob routing
and facade metadata application passed raw/facade `1/1` (`62ms`) and `1/1`
(`31ms`) without production changes. ICC/open_jpeg/JPEG filters passed `87/87`
(`734ms`), `21/21` (`672ms`), and `383/383` (`5172ms`). The JSON-backed full
run passed `1900/1900` in `8437ms`. Exports/DLL remain `390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Latest `META-002AB`: Pillow 11.3.0 opens a bounded RGB JPEG carrying singleton
ICC fragment `0/1:A` and exposes exact public bytes `A`. Existing native state/
blob routing and facade metadata application passed raw/facade `1/1` (`47ms`)
and `1/1` (`32ms`) without production changes. ICC/open_jpeg/JPEG filters
passed `85/85` (`625ms`), `20/20` (`610ms`), and `381/381` (`5156ms`). The
JSON-backed full run passed `1898/1898` in `9062ms`. Exports/DLL remain
`390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Latest `META-002AA`: Pillow 11.3.0 opens a bounded RGB JPEG carrying
`1/3:A` and `3/3:B`, keeps `info["icc_profile"]`, and assigns Python `None`.
The raw RED failed `0/1` (`78ms`) for the missing state export; the facade RED
failed `0/1` (`16ms`) because the key was absent. After native tri-state and
facade `""` routing, raw/facade passed `1/1` (`31ms`) and `1/1` (`31ms`).
ICC/open_jpeg/JPEG filters passed `83/83` (`610ms`), `19/19` (`516ms`), and
`379/379` (`5172ms`). The JSON-backed full run passed `1896/1896` in `8828ms`.
Release x64 rebuilt with zero warnings/errors; exports/DLL are `390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Latest `META-002Z`: a bounded RGB JPEG carrying pre-SOF ICC fragments
`1/3:A`, `2/3:empty`, `3/3:B` opens with exact public ICC bytes `AB` through
the existing DLL finalizer and facade metadata route. Raw/facade passed `1/1`
in `62ms` and `1/1` in `63ms`; ICC/open_jpeg/JPEG filters passed `81/81`
(`1235ms`), `18/18` (`875ms`), and `377/377` (`8906ms`). The final
JSON-backed full run passed `1894/1894` in `15640ms`; no production change or
rebuild. Exports/DLL remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Latest `MODE-NUM-001CG`: border-unset I/F Floodfill with source
`[-7,0,-7]` / `[-7.25,0,-7.25]`, value `9` / `1.5`, and threshold `8.0`
writes the seed and fills all three samples because the zero neighbor's scalar
background distance `7.0` / `7.25` is below the threshold. Raw/facade passed
`1/1` in `1453ms` and `1/1` in `79ms`; no production change or rebuild.
Threshold-precedence/Floodfill/ImageDraw/color/numeric passed `2/2` (`125ms`),
`14/14` (`188ms`), `60/60` (`250ms`), `75/75` (`313ms`), and `96/96`
(`563ms`); full JSON-backed run passed `1892/1892` in `15375ms`. Exports/DLL
remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Latest `MODE-NUM-001CF`: border-unset I/F Floodfill with source
`[-7,0,-7]` / `[-7.25,0,-7.25]`, value `9` / `1.5`, and threshold `6.0` /
`6.25` writes only the seed because the zero neighbor's scalar background
distance `7.0` / `7.25` exceeds the threshold. Raw/facade passed `1/1` in
`703ms` and `1/1` in `47ms`; no production change or rebuild. Threshold-
precedence/Floodfill/ImageDraw/color/numeric passed `2/2` (`63ms`), `14/14`
(`78ms`), `60/60` (`125ms`), `75/75` (`156ms`), and `96/96` (`281ms`);
full JSON-backed run passed `1892/1892` in `8578ms`. Exports/DLL remain
`389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Latest `MODE-NUM-001CE`: border-unset I/F Floodfill with source
`[-7,0,-7]` / `[-7.25,0,-7.25]`, value `9` / `1.5`, and threshold `7.0` /
`7.25` writes the seed, admits the zero neighbor at exact scalar-distance
equality, and fills all three samples through the existing DLL queue. Raw/
facade passed `1/1` in `656ms` and `1/1` in `46ms`; no production change or
rebuild. Threshold-precedence/Floodfill/ImageDraw/color/numeric passed `2/2`
(`46ms`), `14/14` (`78ms`), `60/60` (`125ms`), `75/75` (`156ms`), and
`96/96` (`281ms`); full JSON-backed run passed `1892/1892` in `9015ms`.
Exports/DLL remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Latest `MODE-NUM-001CD`: nonmatching scalar border `300` / `2.5` with per-mode
threshold I `17.0` / F `9.75` leaves I `[-7,0,-7]` and F
`[-7.25,0,-7.25]` unchanged through the corrected mode-aware native initial
comparison. Raw/facade passed `1/1` in `766ms` and `1/1` in `47ms`; no
additional production change or rebuild. Threshold-precedence/Floodfill/
ImageDraw/color/numeric passed `2/2` (`62ms`), `14/14` (`62ms`), `60/60`
(`109ms`), `75/75` (`156ms`), and `96/96` (`281ms`); full JSON-backed run
passed `1892/1892` in `9062ms`. Exports/DLL remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Latest `MODE-NUM-001CC`: local Pillow 11.3.0 with nonmatching scalar border
`300` / `2.5` and threshold equal to initial distance I `16.0` / F `8.75`
leaves I `[-7,0,-7]` and F `[-7.25,0,-7.25]` unchanged. The raw proof first
failed against byte-wise native I/F distance, then passed after mode-aware
scalar distance was implemented and Release x64 rebuilt with zero warnings or
errors. Final raw/facade passed `1/1` in `578ms` and `1/1` in `47ms`;
threshold-precedence/Floodfill/ImageDraw/color/numeric passed `2/2` (`47ms`),
`14/14` (`78ms`), `60/60` (`109ms`), `75/75` (`156ms`), and `96/96`
(`281ms`); full JSON-backed run passed `1892/1892` in `9125ms`. Exports remain
`389/389`; DLL SHA-256 is
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Latest `MODE-NUM-001CB`: Pillow 11.3.0 with nonmatching scalar border `300` /
`2.5` and finite positive threshold `1.0` fills I `[-7,0,-7]` to `[9,9,9]`
and F `[-7.25,0,-7.25]` to `[1.5,1.5,1.5]`, preserving image/data-pointer
identity, mode, size, and exact native bytes. Raw/facade passed `1/1` in
`547ms` and `1/1` in `47ms`; no production change or rebuild. Threshold-
precedence/Floodfill/ImageDraw/color/numeric passed `2/2` (`62ms`), `14/14`
(`78ms`), `60/60` (`110ms`), `75/75` (`141ms`), and `96/96` (`265ms`); full
JSON-backed run passed `1892/1892` in `9047ms`. Exports/DLL remain `389/389`
and `8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001CA`: Pillow 11.3.0 with nonmatching scalar border `300` /
`2.5` and zero threshold `0.0` fills I `[-7,0,-7]` to `[9,9,9]` and F
`[-7.25,0,-7.25]` to `[1.5,1.5,1.5]`, preserving image/data-pointer identity,
mode, size, and exact native bytes. Raw/facade passed `1/1` in `547ms` and
`1/1` in `47ms`; no production change or rebuild. Threshold-precedence/
Floodfill/ImageDraw/color/numeric passed `2/2` (`62ms`), `14/14` (`78ms`),
`60/60` (`109ms`), `75/75` (`156ms`), and `96/96` (`406ms`); full JSON-backed
run passed `1892/1892` in `8672ms`. Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BZ`: Pillow 11.3.0 with nonmatching scalar border `300` /
`2.5` and finite negative threshold `-1.0` fills I `[-7,0,-7]` to `[9,9,9]`
and F `[-7.25,0,-7.25]` to `[1.5,1.5,1.5]`, preserving image/data-pointer
identity, mode, size, and exact native bytes. Raw/facade passed `1/1` in
`578ms` and `1/1` in `47ms`; no production change or rebuild. Threshold-
precedence/Floodfill/ImageDraw/color/numeric passed `2/2` (`62ms`), `14/14`
(`78ms`), `60/60` (`109ms`), `75/75` (`156ms`), and `96/96` (`282ms`); full
JSON-backed run passed `1892/1892` in `8516ms`. Exports/DLL remain `389/389`
and `8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BY`: Pillow 11.3.0 with nonmatching scalar border `300` /
`2.5` and positive-infinity threshold returns before mutation, preserving I
`[-7,0,-7]` and F `[-7.25,0,-7.25]` plus image/data-pointer identity, mode,
size, and exact native bytes. Raw/facade passed `1/1` in `485ms` and `1/1` in
`31ms`; no production change or rebuild. Threshold-precedence/Floodfill/
ImageDraw/color/numeric passed `2/2` (`47ms`), `14/14` (`63ms`), `60/60`
(`141ms`), `75/75` (`156ms`), and `96/96` (`344ms`); full JSON-backed run
passed `1892/1892` in `8657ms`. Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BX`: Pillow 11.3.0 with nonmatching scalar border `300` /
`2.5` and quiet-NaN threshold fills I `[-7,0,-7]` to `[9,9,9]` and F
`[-7.25,0,-7.25]` to `[1.5,1.5,1.5]`, preserving image/data-pointer identity,
mode, size, and exact native bytes. Raw/facade passed `1/1` in `438ms` and
`1/1` in `31ms`; no production change or rebuild. Threshold-precedence/
Floodfill/ImageDraw/color/numeric passed `2/2` (`46ms`), `14/14` (`63ms`),
`60/60` (`109ms`), `75/75` (`156ms`), and `96/96` (`281ms`); full JSON-backed
run passed `1892/1892` in `9187ms`. Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BW`: Pillow 11.3.0 with nonmatching scalar border `300` /
`2.5` and negative-infinity threshold fills I `[-7,0,-7]` to `[9,9,9]` and F
`[-7.25,0,-7.25]` to `[1.5,1.5,1.5]`, preserving image/data-pointer identity,
mode, size, and exact native bytes. Raw/facade passed `1/1` in `515ms` and
`1/1` in `47ms`; no production change or rebuild. Threshold-precedence/
Floodfill/ImageDraw/color/numeric passed `2/2` (`47ms`), `14/14` (`78ms`),
`60/60` (`110ms`), `75/75` (`156ms`), and `96/96` (`297ms`); full JSON-backed
run passed `1892/1892` in `8656ms`. Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BV`: Pillow 11.3.0 with empty tuple/list or AHK Array
border and negative-infinity threshold fills I `[-7,0,-7]` to `[9,9,9]` and F
`[-7.25,0,-7.25]` to `[1.5,1.5,1.5]`, preserving image/data-pointer identity,
mode, size, and exact native bytes while completing the Array-shape matrix.
The reused raw companion and extended facade proof passed `1/1` in `344ms`
and `1/1` in `31ms`; no production change or rebuild. Threshold-precedence/
Floodfill/ImageDraw/color/numeric passed `2/2` (`47ms`), `14/14` (`63ms`),
`60/60` (`94ms`), `75/75` (`157ms`), and `96/96` (`281ms`); full JSON-backed
run passed `1892/1892` in `8516ms`. Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BU`: Pillow 11.3.0 with one-element tuple/list or AHK
Array border `(0,)` / `(0.0,)` and negative-infinity threshold fills I
`[-7,0,-7]` to `[9,9,9]` and F `[-7.25,0,-7.25]` to `[1.5,1.5,1.5]`, while
preserving image/data-pointer identity, mode, size, and exact native bytes. The
reused raw companion and extended facade proof passed `1/1` in `484ms` and
`1/1` in `46ms`; no production change or rebuild. Threshold-precedence/
Floodfill/ImageDraw/color/numeric passed `2/2` (`47ms`), `14/14` (`62ms`),
`60/60` (`94ms`), `75/75` (`141ms`), and `96/96` (`266ms`); full JSON-backed
run passed `1892/1892` in `8672ms`. Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BT`: Pillow 11.3.0 with multi-element tuple/list or AHK
Array border `(300,1)` / `(2.5,3.5)` and negative-infinity threshold fills I
`[-7,0,-7]` to `[9,9,9]` and F `[-7.25,0,-7.25]` to `[1.5,1.5,1.5]`,
preserving image/data-pointer identity and exact native bytes. Raw/facade
passed immediately `1/1` in `375ms` and `1/1` in `31ms`; no production change
or rebuild. Threshold-precedence/Floodfill/ImageDraw/color/numeric passed
`2/2` (`47ms`), `14/14` (`47ms`), `60/60` (`109ms`), `75/75` (`156ms`),
and `96/96` (`281ms`); full JSON-backed run passed `1892/1892` in `8546ms`.
Exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BS`: matching scalar border plus negative infinity writes
only the I/F seed, preserving allocation and exact remaining bytes. Raw/facade
passed `1/1` in `437ms` and `1/1` in `31ms`; gates passed `2/2`, `14/14`,
`60/60`, `75/75`, `96/96`; full `1892/1892` in `8718ms`. No production
change or rebuild; exports/DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BR`: Pillow 11.3.0 with empty tuple/list border and
positive-infinity threshold leaves I `[-7,0,-7]` and F `[-7.25,0,-7.25]`
unchanged because the initial finite distance is at most the threshold,
preserving core identity. AHK empty Array shares BQ/BP's native sentinel and
completes the Array-shape matrix. Reused raw/extended facade proofs passed
`1/1` in `391ms` and `1/1` in `31ms`; no production change or rebuild.
Threshold-precedence/Floodfill/ImageDraw/color/numeric filters passed `2/2`
(`47ms`), `14/14` (`63ms`), `60/60` (`109ms`), `75/75` (`156ms`), and
`96/96` (`281ms`); full JSON-backed run passed `1892/1892` in `8781ms`.
Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BQ`: Pillow 11.3.0 with one-element tuple/list border
`(0,)` / `(0.0,)` and positive-infinity threshold leaves I `[-7,0,-7]` and F
`[-7.25,0,-7.25]` unchanged because the initial finite distance is at most the
threshold, preserving core identity. AHK one-element Array shares BP's native
sentinel. Reused raw/extended facade proofs passed `1/1` in `359ms` and `1/1`
in `32ms`; no production change or rebuild. Threshold-precedence/Floodfill/
ImageDraw/color/numeric filters passed `2/2` (`62ms`), `14/14` (`62ms`),
`60/60` (`110ms`), `75/75` (`156ms`), and `96/96` (`281ms`); full JSON-backed
run passed `1892/1892` in `8922ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BP`: Pillow 11.3.0 with multi-element tuple/list border
`(300,1)` / `(2.5,3.5)` and positive-infinity threshold leaves I `[-7,0,-7]`
and F `[-7.25,0,-7.25]` unchanged because the initial finite distance is at
most the threshold, preserving core identity. AHK Array uses the shared native
sentinel. Extended raw/facade proofs passed `1/1` in `313ms` and `1/1` in
`32ms`; no production change or rebuild. Threshold-precedence/Floodfill/
ImageDraw/color/numeric filters passed `2/2` (`46ms`), `14/14` (`63ms`),
`60/60` (`110ms`), `75/75` (`140ms`), and `96/96` (`266ms`); full JSON-backed
run passed `1892/1892` in `8609ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BO`: Pillow 11.3.0 with matching scalar border `0` / `0.0`
and positive-infinity threshold leaves I `[-7,0,-7]` and F
`[-7.25,0,-7.25]` unchanged because the initial finite distance is at most the
threshold, preserving core identity. Extended raw/facade proofs passed `1/1`
in `313ms` and `1/1` in `31ms`; no production change or rebuild. Threshold-
precedence/Floodfill/ImageDraw/color/numeric filters passed `2/2` (`47ms`),
`14/14` (`62ms`), `60/60` (`94ms`), `75/75` (`140ms`), and `96/96`
(`266ms`); full JSON-backed run passed `1892/1892` in `9078ms`. Exports and
DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BN`: Pillow 11.3.0 with empty tuple/list border and quiet-
NaN threshold fills all three samples: I `[-7,0,-7] -> [9,9,9]` and F
`[-7.25,0,-7.25] -> [1.5,1.5,1.5]`, preserving core identity. AHK empty Array
shares BL/BM's native sentinel. Reused raw/extended facade proofs passed `1/1`
in `281ms` and `1/1` in `31ms`; no production change or rebuild. Threshold-
precedence/Floodfill/ImageDraw/color/numeric filters passed `2/2` (`47ms`),
`14/14` (`62ms`), `60/60` (`93ms`), `75/75` (`141ms`), and `96/96`
(`281ms`); full JSON-backed run passed `1892/1892` in `9000ms`. Exports and
DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BM`: Pillow 11.3.0 with one-element tuple/list border
`(0,)` / `(0.0,)` and quiet-NaN threshold fills all three samples: I
`[-7,0,-7] -> [9,9,9]` and F `[-7.25,0,-7.25] -> [1.5,1.5,1.5]`, preserving
core identity. AHK one-element Array shares BL's native sentinel. Reused raw/
extended facade proofs passed `1/1` in `281ms` and `1/1` in `31ms`; no
production change or rebuild. Threshold-precedence/Floodfill/ImageDraw/color/
numeric filters passed `2/2` (`31ms`), `14/14` (`63ms`), `60/60` (`110ms`),
`75/75` (`156ms`), and `96/96` (`281ms`); full JSON-backed run passed
`1892/1892` in `8735ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BL`: Pillow 11.3.0 with multi-element tuple/list border
`(300,1)` / `(2.5,3.5)` and quiet-NaN threshold fills all three samples: I
`[-7,0,-7] -> [9,9,9]` and F `[-7.25,0,-7.25] -> [1.5,1.5,1.5]`, preserving
core identity. AHK Array uses the existing non-null/zero-size sentinel route;
raw/facade proofs passed `1/1` in `250ms` and `1/1` in `31ms`; no production
change or rebuild. Threshold-precedence/Floodfill/ImageDraw/color/numeric
filters passed `2/2` (`46ms`), `14/14` (`47ms`), `60/60` (`94ms`), `75/75`
(`156ms`), and `96/96` (`281ms`); full JSON-backed run passed `1892/1892` in
`8594ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BK`: Pillow 11.3.0 with scalar border `0` / `0.0` and
quiet-NaN threshold writes only the seed: I `[-7,0,-7] -> [9,0,-7]` and F
`[-7.25,0,-7.25] -> [1.5,0,-7.25]`, preserving core identity. Raw/facade
reuse BJ's explicit IEEE quiet NaN and existing scalar-border routing;
extended proofs passed `1/1` in `265ms` and `1/1` in `32ms`; no production
change or rebuild. Threshold-precedence/Floodfill/ImageDraw/color/numeric
filters passed `2/2` (`47ms`), `14/14` (`47ms`), `60/60` (`94ms`), `75/75`
(`156ms`), and `96/96` (`266ms`); full JSON-backed run passed `1892/1892` in
`8594ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BJ`: Pillow 11.3.0 with border unset and quiet-NaN
threshold writes only the seed: I `[-7,0,-7] -> [9,0,-7]` and F
`[-7.25,0,-7.25] -> [1.5,0,-7.25]`, preserving core identity. Raw/facade
construct IEEE binary64 quiet NaN explicitly; extended proofs passed `1/1` in
`203ms` and `1/1` in `16ms`; no production change or rebuild. Threshold-
precedence/Floodfill/ImageDraw/color/numeric filters passed `2/2` (`31ms`),
`14/14` (`63ms`), `60/60` (`94ms`), `75/75` (`140ms`), and `96/96`
(`266ms`); full JSON-backed run passed `1892/1892` in `8875ms`. Exports and
DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BI`: Pillow 11.3.0 with border unset and threshold negative
infinity writes only the seed: I `[-7,0,-7] -> [9,0,-7]` and F
`[-7.25,0,-7.25] -> [1.5,0,-7.25]`, preserving core identity. Raw/facade
negate BH's explicit IEEE positive infinity; extended proofs passed `1/1` in
`203ms` and `1/1` in `32ms`; no production change or rebuild. Threshold-
precedence/Floodfill/ImageDraw/color/numeric filters passed `2/2` (`31ms`),
`14/14` (`46ms`), `60/60` (`94ms`), `75/75` (`141ms`), and `96/96`
(`265ms`); full JSON-backed run passed `1892/1892` in `8688ms`. Exports and
DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BH`: Pillow 11.3.0 with border unset and threshold positive
infinity leaves I `[-7,0,-7]` and F `[-7.25,0,-7.25]` unchanged because the
initial finite distance is at most the threshold, preserving core identity.
Raw/facade construct IEEE binary64 infinity explicitly; extended and renamed
proofs passed `1/1` in `312ms` and `1/1` in `31ms`; no production change or
rebuild. Threshold-precedence/Floodfill/ImageDraw/color/numeric filters passed
`2/2` (`32ms`), `14/14` (`188ms`), `60/60` (`391ms`), `75/75` (`484ms`),
and `96/96` (`1016ms`); full JSON-backed run passed `1892/1892` in `8437ms`.
Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BG`: Pillow 11.3.0 with empty tuple/list border and
threshold `-1.0` fills all three samples: I `[-7,0,-7] -> [9,9,9]` and F
`[-7.25,0,-7.25] -> [1.5,1.5,1.5]`, preserving core identity. AHK Array
follows the tuple analogue. The reused raw proof and extended facade proof
passed `1/1` in `250ms` and `1/1` in `63ms`; no production change or rebuild.
Negative-threshold/Floodfill/ImageDraw/color/numeric filters passed `2/2`
(`63ms`), `14/14` (`110ms`), `60/60` (`188ms`), `75/75` (`313ms`), and
`96/96` (`484ms`); full JSON-backed run passed `1892/1892` in `15875ms`.
Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BF`: Pillow 11.3.0 with one-element tuple/list border
`(0,)` / `(0.0,)` and threshold `-1.0` fills all three samples: I
`[-7,0,-7] -> [9,9,9]` and F `[-7.25,0,-7.25] -> [1.5,1.5,1.5]`, preserving
core identity. AHK Array follows the tuple analogue. The reused raw proof and
extended facade proof passed `1/1` in `234ms` and `1/1` in `31ms`; no
production change or rebuild. Negative-threshold/Floodfill/ImageDraw/color/
numeric filters passed `2/2` (`62ms`), `14/14` (`78ms`), `60/60` (`203ms`),
`75/75` (`297ms`), and `96/96` (`516ms`); full JSON-backed run passed
`1892/1892` in `15656ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BE`: Pillow 11.3.0 with nonempty multi-element tuple/list
border `(300, 1)` / `(2.5, 3.5)` and threshold `-1.0` fills all three samples:
I `[-7,0,-7] -> [9,9,9]` and F `[-7.25,0,-7.25] -> [1.5,1.5,1.5]`, preserving
core identity. AHK Array follows the tuple analogue. Extended and renamed raw/
facade proofs passed immediately `1/1` in `125ms` and `1/1` in `16ms`; no
production change or rebuild. Negative-threshold/Floodfill/ImageDraw/color/
numeric filters passed `2/2` (`31ms`), `14/14` (`47ms`), `60/60` (`79ms`),
`75/75` (`156ms`), and `96/96` (`266ms`); full JSON-backed run passed
`1892/1892` in `8484ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BD`: Pillow 11.3.0 with scalar border `0` / `0.0` and
threshold `-1.0` writes only the seed: I `[-7,0,-7] -> [9,0,-7]` and F
`[-7.25,0,-7.25] -> [1.5,0,-7.25]`, preserving core identity. Extended raw/
facade proofs passed immediately `1/1` in `110ms` and `1/1` in `16ms`; no
production change or rebuild. Negative-threshold/Floodfill/ImageDraw/color/
numeric filters passed `2/2` (`31ms`), `14/14` (`47ms`), `60/60` (`109ms`),
`75/75` (`172ms`), and `96/96` (`265ms`); full JSON-backed run passed
`1892/1892` in `8922ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BC`: Pillow 11.3.0 with border unset and threshold `-1.0`
writes only the seed: I `[-7,0,-7] -> [9,0,-7]` and F
`[-7.25,0,-7.25] -> [1.5,0,-7.25]`, preserving core identity. Extended raw/
facade proofs passed immediately `1/1` in `79ms` and `1/1` in `16ms`; no
production change or rebuild. Negative-threshold/Floodfill/ImageDraw/color/
numeric filters passed `2/2` (`31ms`), `14/14` (`47ms`), `60/60` (`94ms`),
`75/75` (`172ms`), and `96/96` (`313ms`); full JSON-backed run passed
`1892/1892` in `8484ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BB`: Pillow 11.3.0 empty I/F tuples ignore invalid string
threshold `"bad"`, return `None`, preserve core identity, and leave exact bytes
unchanged; empty lists retain list-minus-int/float errors. The reused raw proof
passed `1/1` in `94ms`; facade RED failed `0/1` in `15ms` with
`thresh must be numeric`, then GREEN passed `1/1` in `32ms` after empty numeric
values were detected before threshold validation and dispatched with native
threshold `0.0`. No native change or rebuild. Empty-value/Floodfill/ImageDraw/
color/numeric filters passed `2/2` (`31ms`), `14/14` (`31ms`), `60/60`
(`109ms`), `75/75` (`156ms`), and `96/96` (`250ms`); full JSON-backed run
passed `1892/1892` in `9266ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001BA`: Pillow 11.3.0 empty I/F tuples ignore invalid string
border `"bad"`, return `None`, preserve core identity, and leave exact bytes
unchanged; empty lists retain list-minus-int/float errors. The extended raw
proof passed `1/1` in `109ms`; facade RED failed `0/1` in `15ms` with
`unknown color specifier: 'bad'`, then GREEN passed `1/1` in `31ms` after
empty numeric values stopped pre-normalizing border. No native change or
rebuild was required. Empty-value/Floodfill/ImageDraw/color/numeric filters
passed `2/2` (`47ms`), `14/14` (`47ms`), `60/60` (`125ms`), `75/75`
(`172ms`), and `96/96` (`250ms`); full JSON-backed run passed `1892/1892` in
`9140ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001AZ`: Pillow 11.3.0 empty I/F tuples with empty tuple
borders return `None`, preserve core identity, and leave exact bytes unchanged;
empty list values with empty list borders retain list-minus-int/float errors.
The reused raw dual-sentinel and extended facade proofs passed immediately
`1/1` in `94ms` and `1/1` in `31ms`, so no production change or rebuild was
required. Empty-value/Floodfill/ImageDraw/color/numeric filters passed `2/2`
(`31ms`), `14/14` (`32ms`), `60/60` (`93ms`), `75/75` (`156ms`), and `96/96`
(`313ms`); the full JSON-backed run passed `1892/1892` in `8532ms`. Exports
and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001AY`: Pillow 11.3.0 empty I/F tuples with one-element tuple
borders `(0,)` / `(0.0,)` return `None`, preserve core identity, and leave
exact bytes unchanged; empty list values with list borders retain list-minus-
int/float errors. The reused raw dual-sentinel and extended facade proofs
passed immediately `1/1` in `94ms` and `1/1` in `16ms`, so no production
change or rebuild was required. Empty-value/Floodfill/ImageDraw/color/numeric
filters passed `2/2` (`31ms`), `14/14` (`47ms`), `60/60` (`125ms`), `75/75`
(`172ms`), and `96/96` (`281ms`); the full JSON-backed run passed `1892/1892`
in `8594ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001AX`: Pillow 11.3.0 empty I/F tuples with nonempty multi-
element tuple borders `(300, 1)` / `(2.5, 3.5)` return `None`, preserve core
identity, and leave exact bytes unchanged; empty list values with list borders
retain list-minus-int/float errors. The extended raw dual-sentinel and facade
Array tests passed immediately `1/1` in `187ms` and `1/1` in `31ms`, so no
production change or rebuild was required. Empty-value/Floodfill/ImageDraw/
color/numeric filters passed `2/2` (`63ms`), `14/14` (`78ms`), `60/60`
(`172ms`), `75/75` (`281ms`), and `96/96` (`484ms`); the full JSON-backed
run passed `1892/1892` in `10344ms`. Exports and DLL remain `389/389` and
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001AW`: Pillow 11.3.0 empty I/F tuples with scalar borders
`0` / `0.0` remain successful allocation-preserving no-ops at threshold zero;
empty lists retain list-minus-int/float errors. Local source confirms
`_color_diff(value, background)` runs before border handling and Floodfill
catches the empty tuple's `IndexError`. Extending AU/AV's raw/facade tests
produced RED `0/1` in `62ms` (expected `0`, got `-2`) and `0/1` in `31ms`
(tuple-color rejection). After removing only the border-unset native/facade
conditions and a zero-warning/error Release rebuild, raw/facade GREEN passed
`1/1` in `328ms` and `1/1` in `31ms`. Empty-value/Floodfill/ImageDraw/color/
numeric filters passed `2/2` (`47ms`), `14/14` (`78ms`), `60/60` (`187ms`),
`75/75` (`297ms`), and `96/96` (`562ms`); the full JSON-backed run passed
`1892/1892` in `15719ms`. Exports remain `389/389`; DLL SHA-256 is
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Latest `MODE-NUM-001AV`: Pillow 11.3.0 empty I/F tuples remain successful
allocation-preserving no-ops at threshold `1.0`; empty lists retain list-minus-
int/float errors. Local source confirms `_color_diff` raises `IndexError` on
the empty tuple and Floodfill catches it before threshold use. Extending AU's
existing raw/facade tests produced RED `0/1` in `109ms` and `0/1` in `32ms`.
After removing only the threshold-zero sentinel/routing conditions and a zero-
warning/error Release rebuild, raw/facade GREEN passed `1/1` in `265ms` and
`1/1` in `31ms`. Combined/Floodfill/ImageDraw/color/numeric passed `2/2`
(`62ms`), `14/14` (`188ms`), `60/60` (`187ms`), `75/75` (`344ms`), and
`96/96` (`579ms`); the full JSON-backed run passed `1892/1892` in `24563ms`.
Exports remain `389/389`; DLL SHA-256 is
`EABD8291824F88BCAAAD0D92C277500DD6F488C8DFD55DBA62CB1AE57623C9E3`.

Latest `MODE-NUM-001AU`: Pillow 11.3.0 empty I/F tuples return `None`, preserve
core identity, and leave exact storage unchanged; empty Python lists instead
raise list-minus-int/float errors. AHK Array follows the tuple analogue. Raw
RED failed `0/1` in `62ms` with status `-2`; facade RED failed `0/1` in `32ms`
with the old tuple-color error. After the explicit native non-null/zero-size
value sentinel, facade routing, and a zero-warning/error Release x64 rebuild,
raw/facade GREEN passed `1/1` in `79ms` and `1/1` in `31ms`. Combined/
Floodfill/ImageDraw/color/numeric passed `2/2` (`63ms`), `14/14` (`78ms`),
`60/60` (`156ms`), `75/75` (`359ms`), and `96/96` (`766ms`); the full
JSON-backed run passed `1892/1892` in `20906ms`. Exports remain `389/389`;
DLL SHA-256 is
`AE26A1E30AB59D5A28D498C17A1D55F0E36FB428C5C32CF7AC710E485BFC2F45`.

Latest `MODE-NUM-001AT`: Pillow 11.3.0 accepts one-element I/F tuples as
signed-int32/float32 Floodfill values, preserves core identity, and emits exact
`[9,0,-7]` / `[1.5,0,-7.25]` storage; one-element Python lists instead raise
list-minus-int/float errors before mutation. AHK Array follows the tuple
analogue. The new facade proof passed immediately `1/1` in `31ms`, and AR's
reused raw packed-value proof passed `1/1` in `78ms`; no production change or
rebuild. Floodfill/ImageDraw/color/numeric passed `12/12` (`62ms`), `59/59`
(`219ms`), `75/75` (`329ms`), and `94/94` (`485ms`); the full JSON-backed run
passed `1890/1890` in `15078ms`. Exports and DLL remain `389/389` and
`B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

Latest `MODE-NUM-001AS`: Pillow 11.3.0 empty tuple/list border probes filled
through scalar zero, returned `None`, preserved core identity, and emitted
exact I/F bytes. The existing raw sentinel proof passed `1/1` in `93ms`; the
new facade empty-Array proof passed `1/1` in `31ms`, with no production change
or rebuild. Numeric-border/Floodfill/ImageDraw/color/numeric passed `6/6`
(`78ms`), `11/11` (`62ms`), `58/58` (`187ms`), `75/75` (`265ms`), and `93/93`
(`516ms`); the full JSON-backed run passed `1889/1889` in `16656ms`. Exports
and DLL remain `389/389` and
`B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

Latest `MODE-NUM-001AR`: Pillow 11.3.0 scalar I/F border probes stopped at the
matching zero sample, returned `None`, preserved core identity, and emitted
exact bytes. Existing raw and facade routes passed immediately `1/1` in `94ms`
and `1/1` in `32ms`; no production change or rebuild. Combined/Floodfill/
ImageDraw/color/numeric passed `2/2` (`47ms`), `10/10` (`78ms`), `57/57`
(`156ms`), `75/75` (`282ms`), and `92/92` (`516ms`); the full JSON-backed run
passed `1888/1888` in `15547ms`. Exports and DLL remain `389/389` and
`B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

Latest `MODE-NUM-001AQ`: Pillow 11.3.0 tuple/list probes both filled through
zero, returned `None`, preserved core identity, and emitted exact I/F bytes.
Facade red failed `0/1` (`32ms`) because `[0]` was packed as a real scalar
border; after routing every numeric border Array to AP's sentinel, facade green
passed `1/1` (`31ms`). The existing raw incomparable-border ABI proof passed
`1/1` (`78ms`). Combined/Floodfill/ImageDraw/color/numeric passed `3/3`
(`47ms`), `8/8` (`62ms`), `56/56` (`156ms`), `75/75` (`281ms`), and `90/90`
(`453ms`); the full JSON-backed run passed `1886/1886` in `16672ms`. No native
change or rebuild; exports and DLL remain `389/389` and
`B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

Latest `MODE-NUM-001AP`: raw red failed `0/1` (`47ms`) because the old native
border comparison stopped at the scalar zero sample; after the explicit
non-null/zero-size incomparable-border implementation and a zero-warning/error
Release x64 rebuild, raw green passed `1/1` (`110ms`). Facade red errored
`0/1` (`31ms`) in `PasteColorBuffer`, then green passed `1/1` (`32ms`) after
numeric multi-element Array sentinel routing. Combined/Floodfill/ImageDraw/
color/numeric passed `2/2` (`62ms`), `7/7` (`78ms`), `55/55` (`156ms`),
`75/75` (`422ms`), and `89/89` (`516ms`); the full JSON-backed run passed
`1885/1885` in `15687ms`. Exports remain `389/389`; rebuilt DLL SHA-256 is
`B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

Latest `MODE-NUM-001AO`: facade red failed `0/1` (`31ms`) with the generic
channel-length error; existing raw Floodfill invalid-argument coverage passed
`1/1` (`94ms`), then facade green passed `1/1` (`31ms`). Floodfill/ImageDraw/
color/numeric filters passed `3/3` (`31ms`), `54/54` (`140ms`), `75/75`
(`297ms`), and `87/87` (`469ms`); the full JSON-backed run passed `1883/1883`
in `15141ms`. Facade-only value validation; no native or DLL change.

Latest `MODE-NUM-001AN`: facade red failed `0/1` (`32ms`) with the generic
channel-length error; existing raw Bitmap invalid-argument coverage passed
`1/1` (`78ms`), then facade green passed `1/1` (`31ms`). Bitmap/ImageDraw/
color/numeric filters passed `3/3` (`32ms`), `53/53` (`125ms`), `75/75`
(`297ms`), and `86/86` (`453ms`); the full JSON-backed run passed `1882/1882`
in `15610ms`. Facade-only fill validation; no native or DLL change.

Latest `MODE-NUM-001AM`: facade red failed `0/1` (`32ms`) with the generic
channel-length error; existing raw RoundedRectangle invalid-argument coverage
passed `1/1` (`78ms`), then facade green passed `1/1` (`31ms`).
RoundedRectangle/ImageDraw/color/numeric filters passed `5/5` (`47ms`),
`52/52` (`125ms`), `74/74` (`328ms`), and `85/85` (`516ms`); the full
JSON-backed run passed `1881/1881` in `16594ms`. Facade-only outline
validation; no native or DLL change.

Latest `MODE-NUM-001AL`: facade red failed `0/1` (`31ms`) with the generic
channel-length error; existing raw RoundedRectangle invalid-argument coverage
passed `1/1` (`94ms`), then facade green passed `1/1` (`31ms`).
RoundedRectangle/ImageDraw/color/numeric filters passed `4/4` (`31ms`),
`51/51` (`125ms`), `73/73` (`359ms`), and `84/84` (`532ms`); the full
JSON-backed run passed `1880/1880` in `15328ms`. Facade-only fill validation;
no native or DLL change.

Latest `MODE-NUM-001AK`: facade red failed `0/1` (`31ms`) with the generic
channel-length error; existing raw Pieslice invalid-argument coverage passed
`1/1` (`78ms`), then facade green passed `1/1` (`31ms`). Pieslice/ImageDraw/
color/numeric filters passed `4/4` (`31ms`), `50/50` (`157ms`), `72/72`
(`266ms`), and `83/83` (`453ms`); the full JSON-backed run passed `1879/1879`
in `15344ms`. Facade-only outline validation; no native or DLL change.

Latest `MODE-NUM-001AJ`: facade red failed `0/1` (`31ms`) with the generic
channel-length error; existing raw Pieslice invalid-argument coverage passed
`1/1` (`78ms`), then facade green passed `1/1` (`31ms`). Pieslice/ImageDraw/
color/numeric filters passed `3/3` (`32ms`), `49/49` (`157ms`), `71/71`
(`281ms`), and `82/82` (`454ms`); the full JSON-backed run passed `1878/1878`
in `16328ms`. Facade-only fill validation; no native or DLL change.

Latest `MODE-NUM-001AI`: facade red failed `0/1` (`31ms`) with the generic
channel-length error; existing raw Chord invalid-argument coverage passed
`1/1` (`62ms`), then facade green passed `1/1` (`31ms`). Chord/ImageDraw/
color/numeric filters passed `4/4` (`32ms`), `48/48` (`140ms`), `70/70`
(`312ms`), and `81/81` (`484ms`); the full JSON-backed run passed `1877/1877`
in `15657ms`. Facade-only outline validation; no native or DLL change.

Latest `MODE-NUM-001AH`: facade red failed `0/1` (`31ms`) with the generic
channel-length error; existing raw Chord invalid-argument coverage passed
`1/1` (`78ms`), then facade green passed `1/1` (`32ms`). Chord/ImageDraw/
color/numeric filters passed `3/3` (`31ms`), `47/47` (`140ms`), `69/69`
(`281ms`), and `80/80` (`500ms`); the full JSON-backed run passed `1876/1876`
in `16922ms`. Facade-only fill validation; no native or DLL change.

Latest `MODE-NUM-001AG`: facade red failed `0/1` (`31ms`) with the generic
channel-length error; existing raw Arc invalid-argument coverage passed `1/1`
(`79ms`), then facade green passed `1/1` (`31ms`). Arc/ImageDraw/color/numeric
filters passed `3/3` (`32ms`), `46/46` (`156ms`), `68/68` (`312ms`), and
`79/79` (`516ms`); the full JSON-backed run passed `1875/1875` in `17110ms`.
Facade-only fill validation; no native or DLL change.

Latest `MODE-NUM-001AF`: facade red failed `0/1` (`31ms`) with the generic
channel-length error; existing raw Ellipse invalid-argument coverage passed
`1/1` (`94ms`), then facade green passed `1/1` (`32ms`). Ellipse/ImageDraw/
color/numeric filters passed `4/4` (`32ms`), `45/45` (`140ms`), `67/67`
(`328ms`), and `78/78` (`562ms`); the full JSON-backed run passed `1874/1874`
in `16796ms`. Facade-only outline validation; no native or DLL change.

Latest `MODE-NUM-001AE`: facade red failed `0/1` (`16ms`) with generic
`Pillow color length must match image channels`; existing raw Ellipse invalid-
argument coverage passed `1/1` (`78ms`), then facade green passed `1/1`
(`16ms`). Ellipse/ImageDraw/color/numeric filters passed `3/3` (`47ms`),
`44/44` (`140ms`), `66/66` (`266ms`), and `77/77` (`500ms`); the full
JSON-backed run passed `1873/1873` in `14875ms`. Facade-only fill validation;
no native or DLL change.

Latest `MODE-NUM-001AD`: facade red failed `0/1` (`47ms`) with the generic
channel-length error; existing raw Rectangle invalid-argument coverage passed
`1/1` (`63ms`), then facade green passed `1/1` (`31ms`). Rectangle/ImageDraw/
color/numeric filters passed `4/4` (`47ms`), `43/43` (`125ms`), `65/65`
(`250ms`), and `76/76` (`484ms`); the full JSON-backed run passed `1872/1872`
in `15469ms`. Facade-only outline validation; no native or DLL change.

Latest `MODE-NUM-001AC`: facade red failed `0/1` (`47ms`) with the generic
channel-length error; the existing raw draw-rectangle invalid-argument proof
passed `1/1` (`62ms`), then facade green passed `1/1` (`31ms`). Rectangle/
ImageDraw/color/numeric filters passed `3/3` (`31ms`), `42/42` (`140ms`),
`64/64` (`266ms`), and `75/75` (`531ms`); the full JSON-backed run passed
`1871/1871` in `15250ms`. Facade-only fill validation; no native or DLL change.

Latest `MODE-NUM-001AB`: facade red failed `0/1` (`31ms`) with the generic
channel-length error; the existing raw draw-line invalid-argument ABI proof
passed `1/1` (`93ms`), then facade green passed `1/1` (`31ms`).
Line/ImageDraw/color/numeric filters passed `6/6` (`47ms`), `41/41`
(`141ms`), `63/63` (`297ms`), and `74/74` (`453ms`); the full JSON-backed run
passed `1870/1870` in `15344ms`. Facade-only Line validation change; no native
or DLL change.

Latest `MODE-NUM-001AA`: facade red failed `0/1` (`31ms`) with the generic
channel-length error; the existing raw draw-points invalid-fill ABI proof
passed `1/1` (`63ms`), then the renamed facade green passed `1/1` (`32ms`).
Point/ImageDraw/color/numeric filters passed `3/3` (`47ms`), `40/40`
(`125ms`), `62/62` (`250ms`), and `73/73` (`610ms`); the full JSON-backed run
passed `1869/1869` in `15563ms`. Facade-only Point validation change; no native
or DLL change.

Latest `MODE-NUM-001Z`: facade red failed `0/1` (`16ms`) with generic
`Pillow color length must match image channels`; the existing raw ABI proof
passed `1/1` (`109ms`), then facade green passed `1/1` (`31ms`). The public
Paste-color family passed `6/6` (`31ms`); Paste/color/numeric filters passed
`29/29` (`93ms`), `61/61` (`281ms`), and `72/72` (`438ms`); full JSON-backed
run passed `1868/1868` in `15594ms`. Facade-only validation change; no native
or DLL change.

Latest `MODE-NUM-001Y`: the existing raw ABI proof passed `1/1` (`109ms`);
facade red failed `0/1` (`15ms`) because `[300]` produced `2c000000` instead
of signed-int32 `2c010000`, then facade/combined passed `1/1` (`31ms`) and
`2/2` (`62ms`). Paste/color/numeric filters passed `28/28` (`63ms`), `60/60`
(`281ms`), and `71/71` (`422ms`); full `1867/1867` in `16672ms`. Facade-only
packing change; no native or DLL change.

Latest `MODE-NUM-001X`: raw targeted passed `1/1` (`93ms`); facade red failed
`0/1` (`15ms`) with `2c000000` instead of int32 `2c010000`, then facade/
combined passed `1/1` (`31ms`) and `2/2` (`62ms`). Paste/color/numeric filters
passed `27/27`, `59/59`, `70/70`; full `1866/1866` in `16563ms`. Facade-only
packing change; no native or DLL change.

Latest `MODE-NUM-001W`: targeted raw/facade/combined `1/1` (`188ms`), `1/1`
(`31ms`), `2/2` (`47ms`); Paste/masked/numeric filters `25/25`, `8/8`,
`68/68`; full `1864/1864` in `15672ms`. No production or DLL change.

Latest `MODE-NUM-001V`: targeted raw/facade/combined `1/1` (`140ms`), `1/1`
(`32ms`), `2/2` (`62ms`); Composite/ImageChops/numeric filters `29/29`,
`59/59`, `66/66`; full `1862/1862` in `16734ms`. No production or DLL change.

Latest `MODE-NUM-001U`: targeted raw/facade/combined `1/1` (`156ms`), `1/1`
(`32ms`), `2/2` (`63ms`); Duplicate/ImageChops/numeric filters `11/11`,
`58/58`, `64/64`; full `1860/1860` in `15297ms`. No production or DLL change.

Latest `MODE-NUM-001T`: targeted raw/facade/combined `1/1` (`109ms`), `1/1`
(`15ms`), `2/2` (`47ms`); Constant/ImageChops/numeric filters `12/12`, `56/56`,
`62/62`; full `1858/1858` in `16625ms`. No production or DLL change.

Latest `MODE-NUM-001S`: targeted raw/facade/combined `1/1` (`109ms`), `1/1`
(`31ms`), `2/2` (`63ms`); Offset/ImageChops/numeric filters `21/21`, `54/54`,
`60/60`; full `1856/1856` in `16203ms`. No production or DLL change.

Latest `MODE-NUM-001R`: targeted raw/facade/combined `1/1` (`109ms`), `1/1`
(`31ms`), `2/2` (`63ms`); Invert/ImageChops/numeric filters `8/8`, `52/52`,
`58/58`; full `1854/1854` in `16281ms`. No production or DLL change.

Latest `MODE-NUM-001Q`: targeted raw/facade/combined `1/1` (`94ms`), `1/1`
(`32ms`), `2/2` (`47ms`); logical/ImageChops/numeric filters `13/13`, `50/50`,
`56/56`; full `1852/1852` in `16234ms`. Facade-only routing change; no native
or DLL change.

Latest `MODE-NUM-001P`: targeted `1/1` (`62ms`), `1/1` (`31ms`), `2/2`
(`47ms`); ImageStat/histogram/numeric filters `5/5`, `26/26`, `54/54`; full
`1850/1850` in `14469ms`. No production/DLL change.

Latest `META-002Y`: targeted `1/1` (`78ms`), `1/1` (`62ms`), `2/2` (`125ms`);
ICC/open_jpeg/JPEG filters `79/79`, `17/17`, `375/375`; full `1848/1848` in
`16297ms`. No production/DLL change.

Latest `META-002X`: targeted `1/1` (`62ms`), `1/1` (`94ms`), `2/2` (`62ms`);
ICC/open_jpeg/JPEG filters `77/77`, `16/16`, `373/373`; full `1846/1846` in
`17391ms`. No production/DLL change.

Latest `META-002W`: targeted `1/1` (`62ms`), `1/1` (`31ms`), `2/2` (`109ms`);
ICC/JPEG filters `75/75`, `371/371`; full `1844/1844` in `16234ms`. No
production/DLL change.

Latest `META-002V`: targeted `1/1` (`93ms`), `1/1` (`47ms`), `2/2` (`78ms`);
ICC/open_jpeg/JPEG filters `73/73`, `15/15`, `369/369`; full `1842/1842` in
`17578ms`. No production/DLL change.

Latest `FMT-JPEG-002B2CS`: targeted `1/1` (`78ms`), `1/1` (`32ms`), `2/2`
(`78ms`); filters `60/60`, `8/8`, `125/125`, `156/156`, `169/169`, `367/367`;
full `1840/1840` in `16391ms`. No production/DLL change.

Latest `FMT-JPEG-002B2CR`: targeted `1/1` (`47ms`), `1/1` (`31ms`), `2/2`
(`79ms`); filters `6/6`, `161/161`, `123/123`, `154/154`, `167/167`, `43/43`,
`365/365`; full `1838/1838` in `17109ms`. No production/DLL change.

Latest `FMT-JPEG-002B2CQ`: targeted `1/1` (`46ms`), `1/1` (`15ms`), `2/2`
(`47ms`); filters `58/58`, `8/8`, `121/121`, `152/152`, `165/165`, `363/363`;
full `1836/1836` in `8579ms`. No production/DLL change.

Latest `FMT-JPEG-002B2CO`: complete default-4:2:0 rows-5 targeted tests passed
`1/1` (`78ms`), `1/1` (`94ms`), `2/2` (`109ms`); filters `56/56`, `4/4`,
`117/117`, `148/148`, `161/161`, `359/359`; full `1832/1832` in `17063ms`.
No production/DLL change; exports `389/389`.

Latest `FMT-JPEG-002B2CN`: default-4:2:0 rows-5 targeted tests passed `1/1`
(`47ms`), `1/1` (`32ms`), `2/2` (`47ms`); filters `2/2`, `161/161`,
`115/115`, `146/146`, `159/159`, `43/43`, `357/357`; full `1830/1830` in
`18062ms`. No production/DLL change; exports `389/389`.

Latest `FMT-JPEG-002B2CM`: complete source-4:2:2 rows-5 targeted tests passed
`1/1` (`79ms`), `1/1` (`63ms`), `2/2` (`109ms`); related filters `54/54`,
`4/4`, `161/161`, `113/113`, `144/144`, `157/157`, `43/43`, `355/355`; full
`1828/1828` in `19593ms`. No production/DLL change; exports `389/389`.

Latest `FMT-JPEG-002B2CL`: raw/facade/combined rows-5 over-scan tests passed
`1/1` in `78ms`, `1/1` in `63ms`, `2/2` in `79ms`; full `1826/1826` passed
in `18063ms`. Related filters passed `2/2`, `161/161`, `111/111`, `142/142`,
`155/155`, `43/43`, `353/353`. No production/DLL change; exports `389/389`;
DLL SHA-256 remains `7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CK`: raw/facade/combined complete default-4:2:0 rows-4
tests passed `1/1` in `31ms`, `1/1` in `15ms`, and `2/2` in `47ms`, matching
Pillow's 1505-byte SHA-256
`1bbb855feed950de122ed61036d4647dbe55169f2d5421b2f9908cea9e3fb722`.
Whole-file/default-subsampling/qtables-keep/progressive/optimize/restart/
qtables/DHT/JPEG passed `52/52`, `22/22`, `70/70`, `109/109`, `140/140`,
`153/153`, `161/161`, `42/42`, `351/351`; full `1824/1824` in `8484ms`.
No production/DLL change; exports `389/389`; DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CJ` verification: raw/facade/combined exact default-
4:2:0 rows-4 progressive-stream tests passed immediately `1/1` in `47ms`,
`1/1` in `15ms`, and `2/2` in `62ms`. All ten DHT/SOS payloads, DRI sequence
`[12,24,12,24,12,24]`, ten empty restart arrays, and 736 entropy bytes match
Pillow 11.3.0; no production or DLL change was required. Default-subsampling/
qtables-keep/progressive/optimize/restart/qtables/Huffman/DHT/JPEG passed
`20/20`, `68/68`, `107/107`, `138/138`, `151/151`, `159/159`, `8/8`, `42/42`,
and `349/349`; full passed `1822/1822` in `8531ms`. Exports remain `389/389`;
unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CI` verification: raw/facade/combined complete source-
4:2:2 rows-4 progressive-file tests passed immediately `1/1` in `31ms`,
`1/1` in `31ms`, and `2/2` in `47ms`. Both routes emit Pillow 11.3.0's
complete 1757-byte JPEG with SHA-256
`ea3a041a14fae80939d499cebf1c2f11eaa2d5089ab1618a392f65a0e1ac4e10`;
no production or DLL change was required. Whole-file/subsampling-keep/
qtables-keep/progressive/optimize/restart/qtables/DHT/JPEG passed `50/50`,
`28/28`, `66/66`, `105/105`, `136/136`, `149/149`, `157/157`, `40/40`, and
`347/347`; full passed `1820/1820` in `8297ms`. Exports remain `389/389`;
unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CH` verification: raw/facade/combined exact source-
4:2:2 rows-4 progressive-stream tests passed immediately `1/1` in `78ms`,
`1/1` in `63ms`, and `2/2` in `93ms`. All ten DHT/SOS payloads, DRI sequence
`[12,24,12,24,12,24]`, ten empty restart arrays, and 973 entropy bytes match
Pillow 11.3.0 at the whole-scan interval boundary; no production or DLL change
was required. Subsampling-keep/qtables-keep/progressive/optimize/restart/
qtables/Huffman/DHT/JPEG passed `26/26`, `64/64`, `103/103`, `134/134`,
`147/147`, `155/155`, `8/8`, `40/40`, and `345/345`; full passed `1818/1818`
in `15422ms`. Exports remain `389/389`; unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CG` verification: raw/facade/combined complete default-
4:2:0 rows-3 progressive-file tests passed immediately `1/1` in `94ms`,
`1/1` in `47ms`, and `2/2` in `78ms`. Both routes emit Pillow 11.3.0's
complete 1517-byte JPEG with SHA-256
`41f426c0d538d08799d332013ce9003d5df4a7ed095beb9fcde5108c4ff73282`;
no production or DLL change was required. Whole-file/default-subsampling/
qtables-keep/progressive/optimize/restart/qtables/DHT/JPEG passed `48/48`,
`18/18`, `62/62`, `101/101`, `132/132`, `145/145`, `153/153`, `38/38`, and
`343/343`; full passed `1816/1816` in `15579ms`. Exports remain `389/389`;
unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CF` verification: raw/facade/combined exact default-
4:2:0 rows-3 progressive-stream tests passed immediately `1/1` in `31ms`,
`1/1` in `31ms`, and `2/2` in `47ms`. All ten DHT/SOS payloads, DRI sequence
`[9,18,9,18,9,18]`, mixed empty/RST0 scan restart state, and 748 entropy bytes
match Pillow 11.3.0; no production or DLL change was required. Default-
subsampling/qtables-keep/progressive/optimize/restart/qtables/Huffman/DHT/JPEG
passed `16/16`, `60/60`, `99/99`, `130/130`, `143/143`, `151/151`, `8/8`,
`38/38`, and `341/341`; full passed `1814/1814` in `8516ms`. Exports remain
`389/389`; unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CE` verification: raw/facade/combined complete source-
4:2:2 rows-3 progressive-file tests passed immediately `1/1`, `1/1`, and
`2/2` in `31ms` each. Both routes emit Pillow 11.3.0's complete 1785-byte JPEG
with SHA-256
`77c26748760271b0edda207cd00054cbec155b38686c41abe4a25f5fc797128c`;
no production or DLL change was required. Whole-file/subsampling-keep/qtables-
keep/progressive/optimize/restart/qtables/DHT/JPEG passed `46/46`, `24/24`,
`58/58`, `97/97`, `128/128`, `141/141`, `149/149`, `36/36`, and `339/339`;
full passed `1812/1812` in `10172ms`. Exports remain `389/389`; unchanged DLL
SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CD` verification: raw/facade/combined exact source-
4:2:2 rows-3 progressive-stream tests passed immediately `1/1` in `47ms`,
`1/1` in `31ms`, and `2/2` in `31ms`. All ten DHT/SOS payloads, DRI sequence
`[9,18,9,18,9,18]`, RST0-only scan state, and 1001 entropy bytes match Pillow
11.3.0; no production or DLL change was required. Subsampling-keep/qtables-
keep/progressive/optimize/restart/qtables/Huffman/DHT/JPEG passed `22/22`,
`56/56`, `95/95`, `126/126`, `139/139`, `147/147`, `8/8`, `36/36`, and
`337/337`; full passed `1810/1810` in `8375ms`. Exports remain `389/389`;
unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CC` verification: raw/facade/combined complete default-
4:2:0 rows-2 progressive-file tests passed immediately `1/1` in `78ms`,
`1/1` in `47ms`, and `2/2` in `78ms`. Both routes emit Pillow 11.3.0's
complete 1518-byte JPEG with SHA-256
`5b280a28b104db2c0112d3070f9fb942674d7d4ed49c8bdcfe013931f0b19c8a`;
no production or DLL change was required. Whole-file/default-subsampling/
qtables-keep/progressive/optimize/restart/qtables/DHT/JPEG passed `44/44`,
`14/14`, `54/54`, `93/93`, `124/124`, `137/137`, `145/145`, `34/34`, and
`335/335`; full passed `1808/1808` in `15281ms`. Exports remain `389/389`;
unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CB` verification: raw/facade/combined exact default-
4:2:0 rows-2 progressive-stream tests passed immediately `1/1` in `93ms`,
`1/1` in `62ms`, and `2/2` in `78ms`. All ten DHT/SOS payloads, DRI sequence
`[6,12,6,12,6,12]`, mixed empty/RST0 scan restart state, and 749 entropy bytes
match Pillow 11.3.0; no production or DLL change was required. Default-
subsampling/qtables-keep/progressive/optimize/restart/qtables/Huffman/DHT/JPEG
passed `12/12`, `52/52`, `91/91`, `122/122`, `135/135`, `143/143`, `8/8`,
`34/34`, and `333/333`; full passed `1806/1806` in `15344ms`. Exports remain
`389/389`; unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2CA` verification: raw/facade/combined complete source-
4:2:2 rows-2 progressive-file tests passed immediately `1/1` in `78ms`,
`1/1` in `47ms`, and `2/2` in `78ms`. Both routes emit Pillow 11.3.0's
complete 1786-byte JPEG with SHA-256
`84ca571e80c61740233af5d1ee999d62fd5138357b893d22ad3a30a5139853cb`;
no production or DLL change was required. Whole-file/subsampling-keep/qtables-
keep/progressive/optimize/restart/qtables/DHT/JPEG passed `42/42`, `20/20`,
`50/50`, `89/89`, `120/120`, `133/133`, `141/141`, `32/32`, and `331/331`;
full passed `1804/1804` in `15109ms`. Exports remain `389/389`; unchanged DLL
SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2BZ` verification: raw/facade/combined exact source-
4:2:2 rows-2 progressive-stream tests passed immediately `1/1` in `78ms`,
`1/1` in `47ms`, and `2/2` in `78ms`. All ten DHT/SOS payloads, DRI sequence
`[6,12,6,12,6,12]`, RST0-only scan state, and 1002 entropy bytes match Pillow
11.3.0; no production or DLL change was required. Subsampling-keep/qtables-
keep/progressive/optimize/restart/qtables/Huffman/DHT/JPEG passed `18/18`,
`48/48`, `87/87`, `118/118`, `131/131`, `139/139`, `8/8`, `32/32`, and
`329/329`; full passed `1802/1802` in `15453ms`. Exports remain `389/389`;
unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2BY` verification: raw/facade/combined complete default-
4:2:0 progressive-file tests passed immediately `1/1` in `78ms`, `1/1` in
`46ms`, and `2/2` in `78ms`. Both routes emit Pillow 11.3.0's complete
1552-byte JPEG with SHA-256
`92552ebc5330f3e0616553bb7b0a3ce0f996181c156d9a50fd62e23bc47c4ef1`;
no production or DLL change was required. Whole-file/default-subsampling/
qtables-keep/progressive/optimize/restart/qtables/DHT/JPEG passed `40/40`,
`10/10`, `46/46`, `85/85`, `116/116`, `129/129`, `137/137`, `30/30`, and
`327/327`; full passed `1800/1800` in `15203ms`. Exports remain `389/389`;
unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2BX` verification: raw/facade/combined exact default-
4:2:0 progressive-stream tests passed immediately `1/1` in `93ms`, `1/1` in
`62ms`, and `2/2` in `78ms`. All ten DHT/SOS payloads, DRI/RST sequences, and
783 entropy bytes match Pillow 11.3.0; no production or DLL change was
required. Default-subsampling/qtables-keep/progressive/optimize/restart/qtables/
Huffman/DHT/JPEG passed `8/8`, `44/44`, `83/83`, `114/114`, `127/127`,
`135/135`, `8/8`, `30/30`, and `325/325`; full passed `1798/1798` in
`15172ms`. Exports remain `389/389`; unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2BW` verification: raw and facade complete-file tests
passed immediately `1/1` in `94ms` and `1/1` in `47ms`; combined passed `2/2`
in `79ms`. Both routes emit Pillow 11.3.0's complete 1836-byte progressive
JPEG with SHA-256
`7c07b262b27d3e71cd82fc132f1546b0291c6b56aef3e4537e48b7888cedd659`.
No production or DLL change was required. Whole-file/qtables-keep/
subsampling-keep/progressive/optimize/restart/qtables/DHT/JPEG passed `38/38`,
`42/42`, `16/16`, `81/81`, `112/112`, `125/125`, `133/133`, `28/28`, and
`323/323`; full passed `1796/1796` in `15297ms`. Exports remain `389/389`;
unchanged DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2BV` verification: raw and facade REDs matched the first
three DHT payloads and failed on native's combined Cr/Cb AC-first table. The
restart-aware sampled RGB progressive encoder now collects and emits separate
Cr/Cb AC-first and final-refine Huffman tables. After a zero-warning/error
Release x64 rebuild, raw/facade/combined passed `1/1` in `172ms`, `1/1` in
`47ms`, and `2/2` in `78ms`. All ten DHT/SOS payloads, DRI values, restart
sequences, and entropy streams match Pillow 11.3.0. QTables-keep/subsampling-
keep/progressive/optimize/restart/qtables/Huffman/DHT/JPEG passed `40/40`,
`14/14`, `79/79`, `110/110`, `123/123`, `131/131`, `8/8`, `28/28`, and
`321/321`; full passed `1794/1794` in `15265ms`. Exports remain `389/389`;
rebuilt DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Latest `FMT-JPEG-002B2BU` verification: raw and facade complete-file tests
passed immediately `1/1` in `78ms` and `1/1` in `47ms`; combined passed `2/2`
in `63ms`. Both routes emit Pillow 11.3.0's complete 1446-byte JPEG with
SHA-256
`7dbee7c1e161ee84bd3175f3a5b96a99d92fc5e904a6cedad2fee81f1b361b83`.
No production or DLL change was required. Whole-file/qtables-keep/
subsampling-keep/optimize/restart/qtables/DHT/JPEG passed `36/36`, `38/38`,
`12/12`, `108/108`, `121/121`, `129/129`, `26/26`, and `319/319`. Full passed
`1792/1792` in `14891ms`. Exports remain `389/389`; unchanged DLL SHA-256 is
`85249326EFFCFCEE05F8A00FBAAB0172FB3CD959B8A4FD29FA8F8B09E655C038`.

Latest `FMT-JPEG-002B2BT` verification: raw and facade complete-file tests
passed immediately `1/1` in `79ms` and `1/1` in `47ms`; combined passed `2/2`
in `62ms`. Both routes emit Pillow 11.3.0's complete 1190-byte JPEG with
SHA-256
`8ce6c8b4f72e89d2e7493fde5e7f3f2a6312574a126c9f69ad710b736200a2ce`.
No production or DLL change was required. Whole-file/qtables-keep/default-
subsampling/optimize/restart/qtables/DHT/JPEG passed `34/34`, `36/36`, `6/6`,
`106/106`, `119/119`, `127/127`, `26/26`, and `317/317`. Full passed
`1790/1790` in `14985ms`. Exports remain `389/389`; unchanged DLL SHA-256 is
`85249326EFFCFCEE05F8A00FBAAB0172FB3CD959B8A4FD29FA8F8B09E655C038`.

Latest `FMT-JPEG-002B2BS` verification: raw and facade RED both matched the
first three DHT payloads and failed on the fourth chroma AC payload. After the
native h2v2 alternating-bias correction and a zero-warning/error Release x64
rebuild, raw and facade filters passed `1/1` in `157ms` and `1/1` in `78ms`;
combined passed `2/2` in `78ms`. The exact four DHT payloads and complete
751-byte entropy stream now match Pillow 11.3.0; entropy SHA-256 is
`1824fdfa1c1b02cb63dc5ed094df3b1729f24c7be1687c1d47548da77604a49a`.
QTables-keep/subsampling-keep/default-subsampling/subsampling/optimize/restart/
qtables/progressive/Huffman/DHT/JPEG passed `34/34`, `10/10`, `4/4`, `71/71`,
`104/104`, `117/117`, `125/125`, `77/77`, `8/8`, `26/26`, and `315/315`.
Full passed `1788/1788` in `15781ms`. Exports remain `389/389`; rebuilt DLL
SHA-256 is
`85249326EFFCFCEE05F8A00FBAAB0172FB3CD959B8A4FD29FA8F8B09E655C038`.

Latest `FMT-JPEG-002B2BR` verification: the raw RED and matching facade RED
both failed on the first differing chroma DHT payload. After the native h2v1
downsampling correction and a zero-warning/error Release x64 rebuild, raw and
facade filters passed `1/1` in `156ms` and `1/1` in `46ms`; combined passed
`2/2` in `78ms`. The exact four DHT payloads and complete 1001-byte entropy
stream now match Pillow `11.3.0`; entropy SHA-256 is
`82d41f37e99ebaf1125ffd0f47d44233ead49dc1be707a2a91638d8da0d06e8e`.
QTables-keep/subsampling-keep/subsampling/optimize/restart/qtables/progressive/
Huffman/DHT/JPEG passed `32/32`, `10/10`, `69/69`, `102/102`, `115/115`,
`123/123`, `77/77`, `8/8`, `24/24`, and `313/313`. Full passed `1786/1786`
in `14922ms`. Exports remain `389/389`; rebuilt DLL SHA-256 is
`7A00F5EA1255AF5C64B2C0C86DDD377C55E130D81D5698AF2C8982D56D769C93`.

Latest `FMT-JPEG-002B2BQ` verification: raw and facade quality-keep
progressive rows-2 DPI/JFIF-plus-XMP/core-metadata complete-file tests passed
immediately `1/1` in `78ms` and `1/1` in `47ms`; combined passed `2/2` in
`125ms`. Native and facade outputs match Pillow's complete 10972-byte file and
SHA-256
`a533204d5714f5166be1e591fd2ea2f755c678706c41f2bb62e86a777fc39e98`.
All 47 non-RST and 108 RST markers, 18 scans, JFIF/metadata segments, and EOI
are exact; BQ offset 20 through EOI equals BP offset 2 through EOI. No
production or ABI change was required. DPI/XMP/metadata/progressive/optimize/
restart/qtables/CMYK/YCCK/Huffman/DHT/JPEG passed `60/60`, `48/48`,
`317/317`, `77/77`, `100/100`, `113/113`, `121/121`, `195/195`, `67/67`,
`8/8`, `22/22`, and `311/311`. Full passed `1784/1784` in `14906ms`.
Exports remain `389/389`; the unchanged DLL SHA-256 is
`9F5A8CB8FF8606FA2E0DDD6B2B554CE1FD2D4843AAC4FB2FE0300AE6400C8C98`.
