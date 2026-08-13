# Pillow Direct Difference Assessment

Date: 2026-06-20

Refreshed counts, 2026-07-04: the tree now has `379` source/DLL exports,
`1326` AHK tests (`664` facade / `662` raw DLL, full directory run green in
about `4-8s`), `43124` native lines, `9793` facade lines, and `71417` AHK test
lines. Since this audit was written, `getxmp()` gained bounded PNG/JPEG
coverage (`META-002A..F`), `get_child_images()` / `format_description` /
`has_transparency_data` gained facade parity (`API-IMG-001A..C`), bounded
DIB-backed CUR open landed (`FMT-ICO-002F`, `pillow_c_image_open_cur`), TIFF
tag/compression/mode children `FMT-TIFF-002A..G` / `FMT-TIFF-003A..K` were
covered, and TIFF common ASCII EXIF readback landed (`META-001L`/`META-001M`).
The stale numbers and "absent" claims below are kept as the 2026-06-20
route-audit snapshot; use `docs/pillow-gap-checkpoint.md` for current state.

Current real-workload replacement-readiness recalibration, 2026-08-06: the
user explicitly asked to ignore the former evaluation model. This model uses
the ability to replace Pillow in real AHK workloads as the denominator and
therefore charges format/plugin breadth and public semantic cross-products
more heavily. Exports, tests, source lines, and isolated tag children validate
closure but do not earn points by themselves. The rounded estimate carries an
uncertainty band of about four points.

| Real-workload replacement-readiness area | Maximum | Earned points |
| --- | ---: | ---: |
| Native storage, lifetime, and hot-path ownership | 20 | 17 |
| Common public API, mode, and operation compatibility | 25 | 17 |
| End-to-end formats, codecs, save options, and multiframe breadth | 30 | 15 |
| Metadata, color, fonts, and dependency interoperability | 15 | 6 |
| Performance evidence, ABI, testing, packaging, and release maturity | 10 | 4 |
| **AHK-first Pillow-runtime overall completion** | **100** | **59 (`59%`, about ±4%)** |

Latest architecture ownership update, 2026-08-06: `ARCH-MOD-002` moved the
complete JPEG implementation and JPEG exports into
`src/pillow_c_codec_jpeg.cpp`, reducing `src/pillow_c.cpp` to about 37,318
lines and leaving the public export table at `445/445`. This materially
improves native ownership and module boundaries, but does not by itself earn
compatibility points; the replacement-readiness estimate remains `59% ±4%`.

The native common-format core, operation hot paths, color transforms, and
facade name coverage are already strong. The main deductions are WebP/APNG/
AVIF/JPEG2000 and plugin breadth, incomplete mode-by-operation and public-
object lifecycles, mutable/nested metadata lifecycles, FreeType/Unicode/RAQM,
broad quantize parity, array/Arrow/Qt constructors, repeatable benchmarks,
fuzz/leak/stress coverage, versioned ABI/header, CI, packaging, and release
automation. Historical audit snapshots and their old percentages below remain
dated evidence but are superseded by this `59%` replacement-readiness
baseline, including the former `70%` goal-attainment score.

Latest RGB 4:2:0-to-YCbCr JPEG draft closure note, 2026-08-06:
`FMT-JPEG-003BI` extends the requested-mode ABI to a repeatable native-
generated 4:2:0 input. WIC supplies full reduced Y/Cb/Cr planes and the DLL
interleaves them directly; Pillow's tuple, exact `24x16` YCbCr bytes, and
second-call no-op match. Release x64 builds cleanly, all `2595/2595` tests
pass, and source/DLL exports remain `445/445`. This bounded compatibility/
performance increment does not change the rounded `59%` replacement-readiness
estimate or ±4-point uncertainty.

Latest RGB 4:2:0-to-YCbCr quarter-scale closure note, 2026-08-06:
`FMT-JPEG-003BJ` verifies the existing native requested-mode route at decoder
scale 4 on the repeatable 4:2:0 input. Pillow's exact `12x8` tuple/bytes and
second-call no-op match; no native source, ABI, or DLL rebuild was required.
All `2597/2597` tests pass and the rounded `59%` replacement-readiness
estimate remains unchanged.

Latest RGB 4:2:0-to-YCbCr scale-8 closure note, 2026-08-06:
`FMT-JPEG-003BK` verifies the existing native requested-mode route at decoder
scale 8 on the repeatable 4:2:0 input. Pillow's exact `6x4` tuple/bytes and
second-call no-op match; no native source, ABI, or DLL rebuild was required.
All `2599/2599` tests pass and the rounded `59%` replacement-readiness
estimate remains unchanged.

Latest RGB 4:2:2-to-YCbCr scale-4 closure note, 2026-08-06:
`FMT-JPEG-003BL` verifies the existing native requested-mode route at decoder
scale 4 on the stable 4:2:2 input. Pillow's exact `12x8` tuple/bytes and
second-call no-op match; no native source, ABI, or DLL rebuild was required.
All `2601/2601` tests pass and the rounded `59%` replacement-readiness
estimate remains unchanged.

Latest RGB 4:2:2-to-YCbCr scale-8 closure note, 2026-08-06:
`FMT-JPEG-003BM` adds Pillow's scale-dependent nearest horizontal chroma
replication for WIC's reduced `6x4 / 3x4 / 3x4` Y/Cb/Cr planes while retaining
fancy h2v1 filtering at scales 2/4. The exact tuple/bytes and second-call no-op
match; Release x64 builds cleanly, all `2603/2603` tests pass, and source/DLL
exports remain `445/445`. The rounded `59%` replacement-readiness estimate
remains unchanged.

Latest RGB 4:2:2-to-YCbCr full-scale closure note, 2026-08-06:
`FMT-JPEG-003BN` verifies the existing native requested-mode route at decoder
scale 1 on the stable 4:2:2 input. Pillow's exact `48x32` tuple/bytes and
second-call no-op match; no production source, ABI, or DLL rebuild was
required. All `2605/2605` tests pass, source/DLL exports remain `445/445`, and
the rounded `59%` replacement-readiness estimate remains unchanged.

Latest RGB 4:2:0-to-YCbCr full-scale closure note, 2026-08-06:
`FMT-JPEG-003BO` adds native h2v2 fancy reconstruction for WIC's
`48x32 / 24x16 / 24x16` Y/Cb/Cr planes. Pillow's exact `48x32` tuple/bytes and
second-call no-op match without RGB conversion, resize fallback, or AHK pixel
loops. Release x64 builds cleanly, all `2607/2607` tests pass, source/DLL
exports remain `445/445`, and the rounded `59%` replacement-readiness estimate
remains unchanged.

Latest RGB 4:2:2-to-L scale-4 closure note, 2026-08-06:
`FMT-JPEG-003BP` verifies the existing native requested-mode route at decoder
scale 4 on the stable 4:2:2 input. Pillow's exact `12x8` tuple/bytes and
second-call no-op match; no production source, ABI, or DLL rebuild was
required. All `2609/2609` tests pass, source/DLL exports remain `445/445`, and
the rounded `59%` replacement-readiness estimate remains unchanged.

Latest RGB 4:2:2-to-L scale-8 closure note, 2026-08-06:
`FMT-JPEG-003BQ` verifies the existing native requested-mode route at decoder
scale 8 on the same stable input. Pillow's exact `6x4` tuple/bytes and
second-call no-op match, with pixel SHA-256
`4F1BFE45ACDE072617748D7095B446061302477DBBC41DA2173C445D4FE0C137`.
Raw/facade tests pass `1/1` in `31ms` / `16ms`; the draft filters pass
`13/13` / `14/14`; raw `open_jpeg` passes `45/45`; and the full suite passes
`2611/2611` in `17922ms`. No native source, ABI, or DLL rebuild was required;
exports remain `445/445`, and the rounded `59%` replacement-readiness
estimate remains unchanged.

Acceleration reference note, 2026-08-06: Pillow 11.3.0's `setup.py` and
`src/PIL/features.py` are the compatibility/dependency index, while the
official native backend build references are libjpeg-turbo 3.1.1, OpenJPEG
2.5.3, libwebp 1.5.0, libavif 1.1.1, libpng/zlib, libtiff, lcms2, and
FreeType/HarfBuzz/FriBidi/Raqm. The current project already has the standard
MSVC v143 Release x64 + WIC + vendored lcms2 baseline. This makes same-day
batch progress realistic for one or two reusable backend pillars, but not a
truthful claim of full Pillow replacement across every plugin and dependency.

Latest RGB-to-YCbCr JPEG draft closure note, 2026-08-06:
`FMT-JPEG-003BH` extends the requested-mode ABI to decoder-native YCbCr on the
bounded 4:2:2 fixture. WIC's half-width Cb/Cr planes are upsampled with exact
libjpeg-turbo h2v1 fancy filtering and interleaved inside the DLL; Pillow's
tuple, exact `24x16` YCbCr bytes, and second-call no-op match. Release x64
builds cleanly, all `2593/2593` tests pass, and source/DLL exports remain
`445/445`. This bounded compatibility/performance increment does not change
the rounded `59%` replacement-readiness estimate or ±4-point uncertainty.

Latest RGB-to-L JPEG draft closure note, 2026-08-06: `FMT-JPEG-003BG` adds an
explicit requested-mode ABI and uses WIC decoder-native reduced planar Y/Cb/Cr
output, retaining Y rather than converting RGB. Pillow's exact L bytes and
second-call no-op match; all `2591/2591` tests pass and source/DLL exports are
`445/445`. This bounded compatibility/performance increment does not change
the rounded `59%` replacement-readiness estimate or ±4-point uncertainty.

Latest RGB JPEG draft closure note, 2026-08-06: `FMT-JPEG-003BF` extends the
decoder-native half-scale route to ordinary opened RGB JPEGs. WIC supplies
reduced BGR and the DLL owns the contiguous B/R swap; the facade remains a
thin same-mode/lifetime route. Pillow's tuple, exact `24x16` RGB bytes, and
second-call no-op match; Release x64 builds cleanly, all `2589/2589` tests
pass, and exports remain `444/444`. This bounded increment does not change the
rounded `59%` replacement-readiness estimate or ±4-point uncertainty band.

Latest JPEG draft closure note, 2026-08-06: `FMT-JPEG-003BE` adds a native
decoder-reduced half-scale route for the odd real YCCK fixture and a thin
one-shot facade mutation/lifetime route. Pillow's tuple, CMYK `9x6` bytes, and
second-call no-op match; Release x64 builds cleanly, all `2587/2587` tests
pass, and source/DLL exports are `444/444`. This bounded compatibility and
performance-boundary increment does not change the rounded `59%` replacement-
readiness estimate or its ±4-point uncertainty band.

Latest YCCK closure note, 2026-08-06: `FMT-JPEG-003BD` adds a reproducibly
generated 397-byte ImageMagick/libjpeg-turbo APP14-transform-2 fixture with
odd `17x11` dimensions and `2x2/1x1/1x1/2x2` component sampling. Native and
facade open bytes match Pillow; quality/qtables keep normalize to transform-0
all-`1x1` CMYK output while preserving both DQT tables. All `2585/2585` tests
pass, with `443/443` source/DLL exports. This bounded evidence increment does
not change the replacement-readiness estimate.

Current closure note, 2026-08-06: `FMT-JPEG-003BA` adds explicit raw/facade
contracts for Pillow-compatible last-wins structured Photoshop ResolutionInfo
metadata inside one APP13 marker. The behavior was already native-owned, all
`2579/2579` tests pass, and this narrow evidence increment does not change the
rounded `70%` estimate or its ±5-point uncertainty band.

Subsequent closure note, 2026-08-06: `FMT-JPEG-003BB` fixes the native
structured APP13 minimum from 16 bytes to the 14 bytes actually consumed,
matching Pillow's available-field behavior for a 15-byte duplicate. Release
x64 and all `2581/2581` tests pass. This real compatibility correction remains
too narrow to change the rounded `70%` estimate or uncertainty band.

Boundary closure note, 2026-08-06: `FMT-JPEG-003BC` adds exact 14-byte accept
and 13-byte ignore contracts around BB's native threshold. All `2583/2583`
tests pass; this strengthens correctness evidence without changing the rounded
`70%` estimate or ±5-point uncertainty band.

Latest bounded metadata closure `META-002DG` adds XMLPacket/XMP tag 700's
EXIF BYTE-write lifecycle while preserving its separation from codec-level
XMP. It adds no export and does not change the rounded 70% estimate.

Latest bounded metadata closure `META-002DF` adds OECF,
ComponentsConfiguration, MakerNote, and SpatialFrequencyResponse BYTE-write
versus TIFF-UNDEFINED-open lifecycles through existing native routes. It adds
no export and does not change the rounded 70% estimate.

Latest bounded metadata closure `META-002DE` adds registered type-7 tags
347/33723/34675/37724 through the existing native EXIF serializer and
JPEG/PNG codecs. It adds no export and does not change the rounded 70%
estimate.

Latest bounded metadata closure `META-002DD` adds the registered mixed
UNDEFINED/BYTE family 40960/41728/41729/41730/41995 through the existing
native EXIF serializer and JPEG/PNG codecs. It adds no export and does not
change the rounded 70% estimate.

Latest bounded metadata closure `META-002DC` adds OriginalRawFileData and
IlluminantData1/2/3 bytes-write versus TIFF-UNDEFINED-open lifecycles through
existing native EXIF and JPEG/PNG routes. It adds no export and does not
change the rounded 70% estimate.

Latest bounded metadata closure `META-002DB` adds DNG OpcodeList1/2/3
bytes-write and TIFF-UNDEFINED-open dual representation through the existing
native EXIF and JPEG/PNG routes. It adds no export and does not change the
rounded 70% estimate.

Latest bounded metadata closure `META-002DA` adds PhotoshopInfo and TimeCodes
BYTE-tag write/read lifecycles and aligns the current native-readable/facade-
writable BYTE allowlists at 20/20 through existing native EXIF and JPEG/PNG
routes. It adds no export and does not change the rounded 70% estimate.

Latest bounded metadata closure `META-002CZ` adds XPComment, XPAuthor,
XPKeywords, and XPSubject BYTE-tag write/read lifecycles through the existing
generic native serializer and JPEG/PNG codecs. It adds no export and does not
change the rounded 70% goal-attainment estimate.

Latest bounded metadata closure `META-002CY` adds DNG `AsShotICCProfile` and
`CurrentICCProfile` EXIF BYTE-tag write/read lifecycles through the existing
generic native serializer and JPEG/PNG codecs. It adds no export and does not
change the rounded 70% goal-attainment estimate.

Latest bounded metadata closure `META-002CX` batches four DNG version/model/
CFA tags onto the existing generic native BYTE-array EXIF route and adds
explicit JPEG/PNG save/reopen lifecycle coverage. It adds no export and does
not change the rounded 70% goal-attainment estimate.

Latest bounded metadata closure `META-002CW` batches five 16-byte DNG
digest/identifier tags onto the existing generic native BYTE-array EXIF route
and adds explicit JPEG/PNG save/reopen lifecycle coverage. It adds no export
and does not change the rounded 70% goal-attainment estimate.

Latest bounded metadata closure `META-002CV` adds `Image.Exif`
`ProfileGainTableMap` 52525 Buffer/bytes serialization plus explicit JPEG/PNG
save/reopen readback. Pillow writes BYTE/type-1, count-8 while TIFF open-side
fixtures remain UNDEFINED/type-7. The slice reuses the generic native EXIF and
codec paths, adds no export, and does not change the rounded 70%
goal-attainment estimate.

Latest bounded metadata closure `META-002CU` adds native/facade inline TIFF/
DNG `ProfileGainTableMap` 52525 UNDEFINED/count-4 readback and completes the
bounded count-4/count-8 inline/out-of-line matrix. It adds no export and does
not change the rounded 70% goal-attainment estimate.

Latest bounded metadata closure `META-002CT` adds native/facade TIFF/DNG
`ProfileGainTableMap` 52525 UNDEFINED/count-8 readback through the existing
EXIF UNDEFINED ABI. It adds no export and does not change the rounded 70%
goal-attainment estimate; interpretation/application, other counts/types,
writeback, and arbitrary tags remain explicit gaps.

Latest bounded metadata closure `META-002CS` completes the exact
`DefaultCropOrigin` 50719 RATIONAL/LONG/SHORT readback matrix by adding
SHORT/count-2 through the existing EXIF ushort-array ABI. It adds no export
and does not change the rounded 70% goal-attainment estimate.

Latest bounded metadata closure `META-002CR` adds native/facade TIFF/DNG
`DefaultCropOrigin` 50719 LONG/count-2 readback through the existing EXIF
uint-array ABI. Explicit TIFF-type dispatch keeps the same tag's RATIONAL and
LONG forms independent. It adds no export and does not change the rounded 70%
goal-attainment estimate.

Latest bounded metadata closure `META-002CQ` completes the exact
`DefaultCropSize` 50720 RATIONAL/LONG/SHORT readback matrix by adding
SHORT/count-2 through the existing EXIF ushort-array ABI. It adds no export
and does not change the rounded 70% goal-attainment estimate.

Latest bounded metadata closure `META-002CP` adds native/facade TIFF/DNG
`DefaultCropSize` 50720 LONG/count-2 readback through the existing EXIF
uint-array ABI. Explicit TIFF-type dispatch keeps the same tag's RATIONAL and
LONG forms independent. It adds no export and does not change the rounded 70%
goal-attainment estimate.

Latest bounded metadata closure `META-002CO` adds native/facade TIFF/DNG
`DefaultCropSize` 50720 RATIONAL/count-2 readback through the existing EXIF
RATIONAL-array ABI. It adds no export and does not change the rounded 70%
goal-attainment estimate; it closes one real metadata compatibility boundary
while leaving crop-size application, alternate TIFF types, and writeback
explicitly separate.

Incremental refresh, 2026-08-06: `META-002CN` adds bounded TIFF/DNG
`DefaultCropOrigin` 50719 RATIONAL/count-2 metadata readback. Pillow 11.3.0
names 50719 in `TiffTags.TAGS` but not `TAGS_V2`. The 152-byte strip-decoded
2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return two
exact `IFDRational` values `(5/2,7/4)`; info reports raw compression plus
`(1,1)` DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in
`172ms` / `1/1` in `63ms`; raw `open_tiff` passes `223/223` in `671ms`;
facade `getexif` passes `216/216` in `3187ms`; full passes `2539/2539` in
`27672ms` with registrations `1251/1288`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`83CCF587C943CF623C7128AB7AB902C02CA326390C5AE5A8167B2EAB66572364`.
The rounded AHK-first goal-attainment estimate remains `70%`.

Incremental refresh, 2026-08-06: `META-002CM` adds bounded TIFF/DNG
`DefaultScale` 50718 RATIONAL/count-2 metadata readback. Pillow 11.3.0 names
50718 in `TiffTags.TAGS` but not `TAGS_V2`. The 152-byte strip-decoded 2x1 L
fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return two exact
`IFDRational` values `(1/2,3/4)`; info reports raw compression plus `(1,1)`
DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in `172ms` /
`1/1` in `78ms`; raw `open_tiff` passes `222/222` in `766ms`; facade
`getexif` passes `215/215` in `3172ms`; full passes `2537/2537` in `27781ms`
with registrations `1250/1287`. Release x64 builds with zero warnings/errors;
exports remain `443/443`, zero difference; SHA-256 is
`1506BDB1A43C07E006A498F8159D0A146E2847107F6107296DB5222E9F9FD082`.
The rounded AHK-first goal-attainment estimate remains `70%`.

Incremental refresh, 2026-08-06: `META-002CL` adds bounded TIFF/DNG
`WhiteLevel` 50717 SHORT/count-1 metadata readback. Pillow 11.3.0 names 50717
in `TiffTags.TAGS` but not `TAGS_V2`. The 136-byte strip-decoded 2x1 L fixture
preserves pixels `[17,34]`; `getexif()` and `tag_v2` return integer `1023`;
info reports raw compression plus `(1,1)` DPI/resolution and no warnings. Raw/
facade GREEN passes `1/1` in `109ms` / `1/1` in `63ms`; raw `open_tiff`
passes `221/221` in `703ms`; facade `getexif` passes `214/214` in `3219ms`;
full passes `2535/2535` in `27937ms` with registrations `1249/1286`. Release
x64 builds with zero warnings/errors; exports remain `443/443`, zero
difference; SHA-256 is
`D0B6E765558584F92F6E1640B777B639E42CF5D7232649CB9EA3A49CD5897E21`.
The rounded AHK-first goal-attainment estimate remains `70%`.

Incremental refresh, 2026-08-06: `META-002CK` adds bounded TIFF/DNG
`BlackLevelDeltaV` 50716 SRATIONAL/count-1 metadata readback. Pillow 11.3.0
names 50716 in `TiffTags.TAGS` but not `TAGS_V2`. The 144-byte strip-decoded
2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return one
exact `IFDRational` value `-1/3`; info reports raw compression plus `(1,1)`
DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in `125ms` /
`1/1` in `62ms`; raw `open_tiff` passes `220/220` in `750ms`; facade
`getexif` passes `213/213` in `3172ms`; full passes `2533/2533` in `27375ms`
with registrations `1248/1285`. Release x64 builds with zero warnings/errors;
exports remain `443/443`, zero difference; SHA-256 is
`6EE08274AC0FCE33DC47A5FCA7FC2133C6C328A930A2FDC286919D5C7DB4E60D`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002CJ` adds bounded TIFF/DNG
`BlackLevelDeltaH` 50715 SRATIONAL/count-2 metadata readback. Pillow 11.3.0
names 50715 in `TiffTags.TAGS` but not `TAGS_V2`. The 152-byte strip-decoded
2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return two
exact `IFDRational` values `(-1/2,3/4)`; info reports raw compression plus
`(1,1)` DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in
`141ms` / `1/1` in `78ms`; raw `open_tiff` passes `219/219` in `766ms`;
facade `getexif` passes `212/212` in `3110ms`; full passes `2531/2531` in
`27359ms` with registrations `1247/1284`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`C361674E869131CC09CD70DCAC21B786CFF9E7FF496C72C06AB2874D672AF726`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002CI` adds bounded TIFF/DNG
`BlackLevel` 50714 RATIONAL/count-4 metadata readback. Pillow 11.3.0 names
50714 in `TiffTags.TAGS` but not `TAGS_V2`. The 168-byte strip-decoded 2x1 L
fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return four exact
`IFDRational` values `(1/2,3/4,5/6,7/8)`; info reports raw compression plus
`(1,1)` DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in
`125ms` / `1/1` in `63ms`; raw `open_tiff` passes `218/218` in `813ms`;
facade `getexif` passes `211/211` in `3110ms`; full passes `2529/2529` in
`27703ms` with registrations `1246/1283`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`CDADE4E2A3019A6766D8930BA72D907B9067C798B16E411DD84A6DA285E94974`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002CH` adds bounded TIFF/DNG
`BlackLevelRepeatDim` 50713 SHORT/count-2 metadata readback. Pillow 11.3.0
names 50713 in `TiffTags.TAGS` but not `TAGS_V2`. The 136-byte strip-decoded
2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
tuple `(2,2)`; info reports raw compression plus `(1,1)` DPI/resolution and no
warnings. Raw/facade GREEN passes `1/1` in `141ms` / `1/1` in `62ms`; raw
`open_tiff` passes `217/217` in `813ms`; facade `getexif` passes `210/210` in
`3109ms`; full passes `2527/2527` in `27813ms` with registrations `1245/1282`.
Release x64 builds with zero warnings/errors; exports remain `443/443`, zero
difference; SHA-256 is
`695E064262A52A218368A3DA7CD5FE10B113CC7949F99B58AC7DFB3EF891BE4B`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002CG` adds bounded TIFF/DNG
`LinearizationTable` 50712 SHORT/count-4 metadata readback. Pillow 11.3.0
names 50712 in `TiffTags.TAGS` but not `TAGS_V2`. The 144-byte strip-decoded
2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
tuple `(0,1,2,3)`; info reports raw compression plus `(1,1)` DPI/resolution and
no warnings. Raw/facade GREEN passes `1/1` in `109ms` / `1/1` in `109ms`; raw
`open_tiff` passes `216/216` in `781ms`; facade `getexif` passes `209/209` in
`3110ms`; full passes `2525/2525` in `27938ms` with registrations `1244/1281`.
Release x64 builds with zero warnings/errors; exports remain `443/443`, zero
difference; SHA-256 is
`B1967A33E132A7EC74EA633A2829453FFB113CD07691C700A009260325265D2D`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002CF` adds bounded TIFF/DNG
`RowInterleaveFactor` 50975 LONG/count-1 metadata readback. Pillow 11.3.0
leaves 50975 unnamed/unregistered in both registries. The 136-byte strip-
decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2`
return integer `3`; info reports raw compression plus `(1,1)` DPI/resolution
and no warnings. Raw/facade GREEN passes `1/1` in `125ms` / `1/1` in `63ms`;
raw `open_tiff` passes `215/215` in `688ms`; facade `getexif` passes `208/208`
in `2985ms`; full passes `2523/2523` in `27562ms` with registrations
`1243/1280`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`7A6F5FF56DF7FC8559F8CAE389614445D9F664284DA0646CBA21787EF07F12C0`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002CE` adds bounded TIFF/DNG
`SubTileBlockSize` 50974 LONG/count-1 metadata readback. Pillow 11.3.0 leaves
50974 unnamed/unregistered in both registries. The 136-byte strip-decoded 2x1
L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return integer
`4`; info reports raw compression plus `(1,1)` DPI/resolution and no warnings.
Raw/facade GREEN passes `1/1` in `125ms` / `1/1` in `62ms`; raw `open_tiff`
passes `214/214` in `750ms`; facade `getexif` passes `207/207` in `3140ms`;
full passes `2521/2521` in `27734ms` with registrations `1242/1279`. Release
x64 builds with zero warnings/errors; exports remain `443/443`, zero
difference; SHA-256 is
`875143AD68ABCFB903F1CB06483E69387598A8C8734A0577EBA95612745C39E7`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002CD` adds bounded TIFF/DNG
`RawDataUniqueID` 50781 BYTE/count-16 metadata readback. Pillow 11.3.0 leaves
50781 unnamed/unregistered in both registries. The 152-byte strip-decoded 2x1
L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
bytes `01..10`; info reports raw compression plus `(1,1)` DPI/resolution and
no warnings. Raw/facade GREEN passes `1/1` in `125ms` / `1/1` in `63ms`; raw
`open_tiff` passes `213/213` in `703ms`; facade `getexif` passes `206/206` in
`2938ms`; full passes `2519/2519` in `27250ms` with registrations `1241/1278`.
Release x64 builds with zero warnings/errors; exports remain `443/443`, zero
difference; SHA-256 is
`5FBD24C732E2B73E0A1B2FBD28E818457CA3F59F38E086859AB807636911E304`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002CC` adds bounded TIFF/DNG
`OriginalRawFileDigest` 50973 BYTE/count-16 metadata readback. Pillow 11.3.0
leaves 50973 unnamed/unregistered in both registries. The 152-byte strip-
decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2`
return exact bytes `01..10`; info reports raw compression plus `(1,1)` DPI/
resolution and no warnings. Raw/facade GREEN passes `1/1` in `125ms` / `1/1`
in `63ms`; raw `open_tiff` passes `212/212` in `688ms`; facade `getexif`
passes `205/205` in `3000ms`; full passes `2517/2517` in `27344ms` with
registrations `1240/1277`. Release x64 builds with zero warnings/errors;
exports remain `443/443`, zero difference; SHA-256 is
`DE7D42A30F17CB0FA9A748D701D69BF026DBF84B311B9836016D4377287475D2`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002CB` adds bounded TIFF/DNG
`RawImageDigest` 50972 BYTE/count-16 metadata readback. Pillow 11.3.0 leaves
50972 unnamed/unregistered in both registries. The 152-byte strip-decoded 2x1
L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
bytes `01..10`; info reports raw compression plus `(1,1)` DPI/resolution and
no warnings. Raw/facade GREEN filters pass `2/2` in `140ms` / `2/2` in
`78ms`; raw `open_tiff` passes `211/211` in `656ms`; facade `getexif` passes
`204/204` in `3000ms`; full passes `2515/2515` in `28188ms` with registrations
`1239/1276`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`792BFCFB09823D981366D875ADD2128C650A38842E069A8EEE6D1E18F582A577`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002CA` adds bounded TIFF/DNG
`PreviewSettingsDigest` 50969 UNDEFINED/count-16 metadata readback. Pillow
11.3.0 leaves 50969 unnamed/unregistered in both registries. The 152-byte
strip-decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and
`tag_v2` return exact bytes `01..10`; info reports raw compression plus `(1,1)`
DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in `125ms` /
`62ms`; raw `open_tiff` passes `210/210` in `672ms`; facade `getexif` passes
`203/203` in `2969ms`; full passes `2513/2513` in `27156ms` with registrations
`1238/1275`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`F06D8C96A6F6EF9FBA0D11E50DC17B83E807CB403C194FB9476A9BECBDEA937D`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002BZ` adds bounded TIFF/DNG
`OpcodeList3` 51022 UNDEFINED/count-8 metadata readback. Pillow 11.3.0 leaves
51022 unnamed/unregistered in both registries. The 144-byte strip-decoded 2x1
L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
bytes `01 02 03 04 05 06 07 08`; info reports raw compression plus `(1,1)`
DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in `125ms` /
`63ms`; raw `open_tiff` passes `209/209` in `687ms`; facade `getexif` passes
`202/202` in `2969ms`; full passes `2511/2511` in `27812ms` with registrations
`1237/1274`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`D600F073D8DEC391194B3785B81071A14484317E809BF01F33973BA866C9A174`.
The rounded replacement-readiness estimate remains `58%`.

Incremental refresh, 2026-08-06: `META-002BY` adds bounded TIFF/DNG
`OpcodeList2` 51009 UNDEFINED/count-8 metadata readback. Pillow 11.3.0 leaves
51009 unnamed/unregistered in both registries. The 144-byte strip-decoded 2x1
L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
bytes `01 02 03 04 05 06 07 08`; info reports raw compression plus `(1,1)`
DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in `141ms` /
`63ms`; raw `open_tiff` passes `208/208` in `797ms`; facade `getexif` passes
`201/201` in `2922ms`; full passes `2509/2509` in `27593ms` with registrations
`1236/1273`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`C543E5341058B6EDACF063C0F58C71A758F2341CBDE9713073E00E2B4582E954`.
The rounded goal-maturity estimate remains `64%`.

Incremental refresh, 2026-08-06: `META-002BX` adds bounded TIFF/DNG
`OpcodeList1` 51008 UNDEFINED/count-8 metadata readback. Pillow 11.3.0 leaves
51008 unnamed/unregistered in both registries. The 144-byte strip-decoded 2x1
L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
bytes `01 02 03 04 05 06 07 08`; info reports raw compression plus `(1,1)`
DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in `125ms` /
`62ms`; raw `open_tiff` passes `207/207` in `734ms`; facade `getexif` passes
`200/200` in `2891ms`; full passes `2507/2507` in `27734ms` with registrations
`1235/1272`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`64D75DA69A5557066AD3B071BBD7C51821EC7F0487C72E7A1E42FA62ECDDED85`.
The rounded goal-maturity estimate remains `64%`.

Incremental refresh, 2026-08-06: `META-002BW` adds bounded TIFF/DNG
`OriginalRawFileData` 50828 UNDEFINED/count-8 metadata readback. Pillow 11.3.0
leaves 50828 unnamed/unregistered in both registries. The 144-byte strip-
decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2`
return exact bytes `01 02 03 04 05 06 07 08`; info reports raw compression
plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in
`140ms` / `62ms`; raw `open_tiff` passes `206/206` in `687ms`; facade
`getexif` passes `199/199` in `2782ms`; full passes `2505/2505` in `27625ms`
with registrations `1234/1271`. Release x64 builds with zero warnings/errors;
exports remain `443/443`, zero difference; SHA-256 is
`995D76AB0C77C005DE40DB5EB6E36540C62DD618C170B58D12986B415EE8E00A`.
The rounded goal-maturity estimate remains `64%`.

Incremental refresh, 2026-08-06: `META-002BV` adds bounded TIFF/DNG
`ColorimetricReference` 50879 SHORT/count-1 metadata readback. Pillow 11.3.0
leaves 50879 unnamed/unregistered in both registries. The 136-byte strip-
decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2`
return integer `1`; info reports raw compression plus `(1,1)` DPI/resolution
and no warnings. Raw/facade GREEN passes `1/1` in `125ms` / `63ms`; raw
`open_tiff` passes `205/205` in `625ms`; facade `getexif` passes `198/198` in
`2797ms`; full passes `2503/2503` in `26406ms` with registrations `1233/1270`.
Release x64 builds with zero warnings/errors; exports remain `443/443`, zero
difference; SHA-256 is
`7E4ACDC7F08C285ABAB29879A0D766D6C882FB32781A5A1506571D8B634D106A`.
The rounded goal-maturity estimate remains `64%`.

Incremental refresh, 2026-08-06: `META-002BU` adds bounded TIFF/DNG
`CurrentPreProfileMatrix` 50834 SRATIONAL/count-9 metadata readback. Pillow
11.3.0 leaves 50834 unnamed/unregistered in both registries. The 208-byte
strip-decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and
`tag_v2` return nine exact IFDRational pairs including `(-1,2)`; info reports
raw compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `125ms` / `63ms`; raw `open_tiff` passes `204/204` in `719ms`;
facade `getexif` passes `197/197` in `2719ms`; full passes `2501/2501` in
`27171ms` with registrations `1232/1269`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`FD655F6E558AAAF25A3C353618CCC047FE9CCAB0966B251D02BF5973C4DBA5DB`.
The rounded goal-maturity estimate remains `64%`.

Incremental refresh, 2026-08-06: `META-002BT` adds bounded TIFF/DNG
`CurrentICCProfile` 50833 BYTE/count-8 metadata readback. Pillow 11.3.0 leaves
50833 unnamed/unregistered in both registries. The 144-byte strip-decoded 2x1
L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
bytes `01 02 03 04 05 06 07 08`; info reports raw compression plus `(1,1)`
DPI/resolution without `icc_profile` and no warnings. Raw/facade GREEN passes
`1/1` in `94ms` / `62ms`; raw `open_tiff` passes `203/203` in `719ms`;
facade `getexif` passes `196/196` in `2750ms`; full passes `2499/2499` in
`26781ms` with registrations `1231/1268`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`5EF8D42F9C0A749D37A1596D4BA4A242BD5BE5CE41A8665F5902028C938C5DD5`.
The rounded goal-maturity estimate remains `64%`.

Incremental refresh, 2026-08-06: `META-002BS` adds bounded TIFF/DNG
`AsShotPreProfileMatrix` 50832 SRATIONAL/count-9 metadata readback. Pillow
11.3.0 leaves 50832 unnamed/unregistered in both registries. The 208-byte
strip-decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and
`tag_v2` return nine exact IFDRational pairs including `(-1,2)`; info reports
raw compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `141ms` / `62ms`; raw `open_tiff` passes `202/202` in `688ms`;
facade `getexif` passes `195/195` in `2734ms`; full passes `2497/2497` in
`27250ms` with registrations `1230/1267`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`FBD112B12870F0C106C5184FAD26FE23C2C24F5063C0015CFC6E764BABCC44E4`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BR` adds bounded TIFF/DNG
`AsShotICCProfile` 50831 BYTE/count-8 metadata readback. Pillow 11.3.0 leaves
50831 unnamed/unregistered in both registries. The 144-byte strip-decoded 2x1
L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
bytes `01 02 03 04 05 06 07 08`; info reports raw compression plus `(1,1)`
DPI/resolution without `icc_profile` and no warnings. Raw/facade GREEN passes
`1/1` in `312ms` / `63ms`; raw `open_tiff` passes `201/201` in `718ms`;
facade `getexif` passes `194/194` in `2579ms`; full passes `2495/2495` in
`26234ms` with registrations `1229/1266`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`E801D2E72A4683B59DE718F204E83B9D62B6A0A585C13A41463AC00F1D3A7578`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BQ` extends bounded TIFF/DNG
`MaskedAreas` 50830 LONG-array metadata readback to count 8 (two rectangles).
Pillow 11.3.0 leaves 50830 unnamed/unregistered in both registries. The
168-byte strip-decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()`
and `tag_v2` return exact integer tuple `(0,0,1,1,0,1,1,2)`; info reports raw
compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `157ms` / `79ms`; raw `open_tiff` passes `200/200` in `734ms`;
facade `getexif` passes `193/193` in `2578ms`; full passes `2493/2493` in
`26594ms` with registrations `1228/1265`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`12AFB52CA0375E5A926330DED25490E2AFA436C9CCB9D122C3EFCDBC85116B47`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BP` adds bounded TIFF/DNG
`MaskedAreas` 50830 LONG/count-4 one-rectangle metadata readback. Pillow
11.3.0 leaves 50830 unnamed/unregistered in both registries. The 152-byte
strip-decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and
`tag_v2` return exact integer tuple `(0,0,1,2)`; info reports raw compression
plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in
`110ms` / `47ms`; raw `open_tiff` passes `199/199` in `609ms`; facade
`getexif` passes `192/192` in `2563ms`; full passes `2491/2491` in `26109ms`
with registrations `1227/1264`. Release x64 builds with zero warnings/errors;
exports remain `443/443`, zero difference; SHA-256 is
`935BEAF26BAD20D9D19435BE70EC2C2C925FE8A567ADCC2B05F9A68CC42185F0`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BO` adds bounded alternate-type
TIFF/DNG `ActiveArea` 50829 SHORT/count-4 metadata readback. Pillow 11.3.0
leaves 50829 unnamed/unregistered in both registries. The 144-byte strip-
decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2`
return exact integer tuple `(0,0,1,2)`; info reports raw compression plus
`(1,1)` DPI/resolution and no warnings. Native metadata dispatch selects the
SHORT or LONG array route only for TIFF type 3 or 4. Fresh raw/facade GREEN
passes `1/1` in `62ms` / `62ms`; raw `open_tiff` passes `198/198` in `641ms`;
facade `getexif` passes `191/191` in `2640ms`; full passes `2489/2489` in
`26547ms` with registrations `1226/1263`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`4F358ECF4C0672369E7157E1CB0E1943D2B64C4585998C56ED1FBD1F4FAEF283`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BN` adds bounded TIFF/DNG
`ActiveArea` 50829 LONG/count-4 metadata readback. Pillow 11.3.0 leaves 50829
unnamed/unregistered in both registries. The 152-byte strip-decoded 2x1 L
fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
integer tuple `(0,0,1,2)`; info reports raw compression plus `(1,1)` DPI/
resolution and no warnings. Raw/facade GREEN passes `1/1` in `141ms` / `31ms`;
raw `open_tiff` passes `197/197` in `671ms`; facade `getexif` passes `190/190`
in `2594ms`; full passes `2487/2487` in `26937ms` with registrations
`1225/1262`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`BF3B4D8B718C02AC3B9BD9747AF1ADC103EC20D691702E838541BCE04494D4C5`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BM` adds bounded TIFF/DNG
`CameraLabel` 51092 ASCII/count-9 metadata readback. Pillow 11.3.0 leaves
51092 unnamed/unregistered in both registries. The 145-byte strip-decoded 2x1
L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
string `Camera A`; info reports raw compression plus `(1,1)` DPI/resolution
and no warnings. Raw/facade GREEN passes `1/1` in `62ms` / `31ms`; raw
`open_tiff` passes `196/196` in `718ms`; facade `getexif` passes `189/189` in
`2813ms`; full passes `2485/2485` in `26407ms` with registrations `1224/1261`.
Release x64 builds with zero warnings/errors; exports remain `443/443`, zero
difference; SHA-256 is
`6359CFB3DFD63B0C9D47DF7DE814F4F212DCF1198ECCDCF5F258113B7B3DEF55`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BL` adds bounded alternate-type
TIFF/DNG `OriginalDefaultCropSize` 51091 LONG/count-2 metadata readback.
Pillow 11.3.0 leaves 51091 unnamed/unregistered in both registries. The
144-byte strip-decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()`
and `tag_v2` return exact integer tuple `(4000,3000)`; info reports raw
compression plus `(1,1)` DPI/resolution and no warnings. Native metadata
dispatch now selects the RATIONAL or LONG array route from TIFF type 5 or 4.
Raw/facade GREEN passes `1/1` in `125ms` / `31ms`; RATIONAL raw/facade
regression passes `1/1` in `16ms` / `32ms`; raw `open_tiff` passes `195/195`
in `656ms`; facade `getexif` passes `188/188` in `2484ms`; full passes
`2483/2483` in `26750ms` with registrations `1223/1260`. Release x64 builds
with zero warnings/errors; exports remain `443/443`, zero difference;
SHA-256 is
`4F8415CF5F80FDA284A0011F591255AECE49737D5BF68FA674425B355436F7DC`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BK` adds bounded TIFF/DNG
`OriginalDefaultCropSize` 51091 RATIONAL/count-2 metadata readback. Pillow
11.3.0 leaves 51091 unnamed/unregistered in both registries. The 152-byte
strip-decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and
`tag_v2` return exact IFDRational values `8001/2` and `6001/2`; info reports
raw compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `125ms` / `31ms`; raw `open_tiff` passes `194/194` in `734ms`;
facade `getexif` passes `187/187` in `2563ms`; full passes `2481/2481` in
`26422ms` with registrations `1222/1259`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`46C943988AD2C5448E4459DA4A258B5040E570ADA1058950833D8C4517097F61`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BJ` adds bounded TIFF/DNG
`OriginalBestQualityFinalSize` 51090 LONG/count-2 metadata readback. Pillow
11.3.0 leaves 51090 unnamed/unregistered in both registries. The 144-byte
strip-decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and
`tag_v2` return exact tuple `(6000,4000)`; info reports raw compression plus
`(1,1)` DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in
`141ms` / `63ms`; raw `open_tiff` passes `193/193` in `687ms`; facade
`getexif` passes `186/186` in `2672ms`; full passes `2479/2479` in `26579ms`
with registrations `1221/1258`. Release x64 builds with zero warnings/errors;
exports remain `443/443`, zero difference; SHA-256 is
`5F392D7940A11C6D802E91CBED8BF32BB5369E0E31140B0A3862DDE44127CC8D`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BI` adds bounded TIFF/DNG
`OriginalDefaultFinalSize` 51089 LONG/count-2 metadata readback. Pillow 11.3.0
leaves 51089 unnamed/unregistered in both registries. The 144-byte strip-
decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2`
return exact tuple `(4000,3000)`; info reports raw compression plus `(1,1)`
DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in `109ms` /
`62ms`; raw `open_tiff` passes `192/192` in `640ms`; facade `getexif` passes
`185/185` in `2547ms`; full passes `2477/2477` in `26687ms` with registrations
`1220/1257`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`7FB1A351E254E8592592DAC374B2D07F40AED028FABA548273B09E2AF85B71CB`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BH` adds bounded TIFF/DNG `ReelName`
51081 ASCII/count-10 metadata readback. Pillow 11.3.0 leaves 51081 unnamed/
unregistered in both registries. The 146-byte strip-decoded 2x1 L fixture
preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact string
`A001_C001` after removing the trailing NUL; info reports raw compression plus
`(1,1)` DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in
`171ms` / `63ms`; raw `open_tiff` passes `191/191` in `609ms`; facade
`getexif` passes `184/184` in `2672ms`; full passes `2475/2475` in `27375ms`
with registrations `1219/1256`. Release x64 builds with zero warnings/errors;
exports remain `443/443`, zero difference; SHA-256 is
`EE6220F222AC8D0CD59D6A6C6C25411231FC9F80400B7E16F2547B4F6EE9736E`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BG` adds bounded TIFF/DNG `TStop`
51058 RATIONAL/count-1 metadata readback. Pillow 11.3.0 leaves 51058 unnamed/
unregistered in both registries. The 144-byte strip-decoded 2x1 L fixture
preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
`IFDRational(28,10)` with float value `2.8`; info reports raw compression plus
`(1,1)` DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in
`78ms` / `63ms`; raw `open_tiff` passes `190/190` in `656ms`; facade
`getexif` passes `183/183` in `2484ms`; full passes `2473/2473` in
`27422ms` with registrations `1218/1255`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`387990225767EA05C7C319AFBBA98D10B4776FD16D595FC1FEBD5A1FBBE3CD04`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BF` adds bounded TIFF/DNG
`FrameRate` 51044 SRATIONAL/count-1 metadata readback. Pillow 11.3.0 leaves
51044 unnamed/unregistered in both registries. The 144-byte strip-decoded 2x1
L fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
`IFDRational(30000,1001)` with float value `29.97002997002997`; info reports
raw compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `141ms` / `62ms`; raw `open_tiff` passes `189/189` in
`640ms`; facade `getexif` passes `182/182` in `2828ms`; full passes
`2471/2471` in `26406ms` with registrations `1217/1254`. Release x64 builds
with zero warnings/errors; exports remain `443/443`, zero difference;
SHA-256 is
`6B07E5B4A172C4E5D680AFAC0814F738D1E649C02ED572657A39F66540872B16`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BE` adds bounded TIFF/DNG
`TimeCodes` 51043 BYTE/count-8 metadata readback. Pillow 11.3.0 leaves 51043
unnamed/unregistered in both registries. The 144-byte strip-decoded 2x1 L
fixture preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact bytes
`01 02 03 04 05 06 07 08`; info reports raw compression plus `(1,1)` DPI/
resolution and no warnings. Raw/facade GREEN passes `1/1` in `78ms` / `63ms`;
raw `open_tiff` passes `188/188` in `625ms`; facade `getexif` passes `181/181`
in `2531ms`; full passes `2469/2469` in `27610ms` with registrations
`1216/1253`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`D9ABA496C6A31656C34B942FE9609470ABA377EB27FE8B7CD5AAFE55F844D216`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BD` extends bounded TIFF
`NoiseProfile` 51041 DOUBLE-array metadata readback to count 4, completing
counts 2/4/6/8. Pillow 11.3.0 leaves 51041 unnamed/unregistered in both
registries. The 168-byte strip-decoded 2x1 L fixture preserves pixels
`[17,34]`; `getexif()` and `tag_v2` return exact tuple
`(0.125,0.25,0.5,1.0)`; info reports raw compression plus `(1,1)` DPI/
resolution and no warnings. Raw/facade GREEN passes `1/1` in `109ms` / `32ms`;
raw `open_tiff` passes `187/187` in `703ms`; facade `getexif` passes `180/180`
in `2719ms`; full passes `2467/2467` in `28110ms` with registrations
`1215/1252`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`09C6C3CD5E5864F903A1D1ECA6A8094E87562EB03B1605252D3FAADE322B7048`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BC` extends bounded TIFF
`NoiseProfile` 51041 DOUBLE-array metadata readback from count 6 to counts 2
and 8. Pillow 11.3.0 leaves 51041 unnamed/unregistered in both registries. The
152-byte count-2 and 200-byte count-8 strip-decoded 2x1 L fixtures preserve
pixels `[17,34]`; `getexif()` and `tag_v2` return exact tuples
`(0.125,0.75)` and `(0.125,0.25,0.5,1.0,2.0,4.0,8.0,16.0)`; info reports
raw compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `109ms` / `47ms`; raw `open_tiff` passes `186/186` in `703ms`;
facade `getexif` passes `179/179` in `2437ms`; full passes `2465/2465` in
`27781ms` with registrations `1214/1251`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`177426C6BEACB6971D7E7111008E1B0075F0B5AA4360678D92AE56671E89261C`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BB` adds bounded TIFF DOUBLE-array
metadata readback for `NoiseProfile` 51041 with exact count 6. Pillow 11.3.0
leaves 51041 unnamed/unregistered in both `TiffTags.TAGS` and `TAGS_V2`. The
184-byte strip-decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()`
and `tag_v2` return exact tuple `(0.25,0.5,1.0,2.0,4.0,8.0)`; info reports raw
compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `109ms` / `32ms`; raw `open_tiff` passes `185/185` in `422ms`;
facade `getexif` passes `178/178` in `2453ms`; full passes `2463/2463` in
`28000ms` with registrations `1213/1250`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`6D54586EE0C0FD10E6FFA0803E831A1CFC3F8BC7C9E8223D64C47F9559E2A980`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002BA` extends the established native
TIFF FLOAT-array route for `ProfileToneCurve` 50940 from count 6 to bounded
count 18. Pillow 11.3.0 leaves 50940 unnamed/unregistered in both
`TiffTags.TAGS` and `TAGS_V2`. The 208-byte strip-decoded 2x1 L fixture
preserves pixels `[17,34]`; `getexif()` and `tag_v2` return nine exact
`(x,x^2)` control points for `x=0.0,0.125,...,1.0`; info reports raw
compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `94ms` / `31ms`; raw `open_tiff` passes `184/184` in `391ms`;
facade `getexif` passes `177/177` in `1485ms`; full passes `2461/2461` in
`19094ms` with registrations `1212/1249`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`E05427246016849891808FA38AF4E53E8EC5713B228A3896E617A3CEAF17DD92`.
The rounded workload-substitutability estimate remains `60%`.

Incremental refresh, 2026-08-06: `META-002AZ` composes the established native
TIFF metadata routes for `ProfileHueSatMapDims` 50937 LONG/count-3 `(6,3,1)`
and both `ProfileHueSatMapData1` 50938 and `ProfileHueSatMapData2` 50939 as
FLOAT/count-54. Pillow 11.3.0 leaves all three tags unnamed/unregistered in
`TiffTags.TAGS` and `TAGS_V2`. The 604-byte strip-decoded 2x1 L fixture
preserves pixels `[17,34]`; `getexif()` and `tag_v2` return all three exact
tuples; info reports raw compression plus `(1,1)` DPI/resolution and no
warnings. Raw/facade GREEN passes `1/1` in `125ms` / `46ms`; raw `open_tiff`
passes `183/183` in `656ms`; facade `getexif` passes `176/176` in `2422ms`;
full passes `2459/2459` in `26687ms` with registrations `1211/1248`. Release
x64 builds with zero warnings/errors; exports remain `443/443`, zero
difference; SHA-256 is
`C9C8F0BF21CA3F6B99DE40AEBCDDDF633C3F88D83C30E2C78395028FE269181A`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-06: `META-002AY` composes the established native
TIFF metadata routes for `ProfileLookTableDims` 50981 LONG/count-3 `(6,3,1)`
and `ProfileLookTableData` 50982 FLOAT/count-54. Pillow 11.3.0 leaves both tags
unnamed/unregistered in `TiffTags.TAGS` and `TAGS_V2`. The 376-byte
strip-decoded 2x1 L fixture preserves pixels `[17,34]`; `getexif()` and
`tag_v2` return exact dims and the 54-value tuple from `0.0` through `6.625`
in `0.125` steps; info reports raw compression plus `(1,1)` DPI/resolution and
no warnings. Raw/facade GREEN passes `1/1` in `94ms` / `32ms`; raw
`open_tiff` passes `182/182` in `656ms`; facade `getexif` passes `175/175` in
`2281ms`; full passes `2457/2457` in `27828ms` with registrations
`1210/1247`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`474370B7068E785B4C90B3D4FC7EB08F65ACA65429533A89B2155C320770F5AB`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-06: `META-002AX` extends the established native
TIFF FLOAT-array route for `ProfileLookTableData` 50982 from count 6 to bounded
count 18. Pillow 11.3.0 leaves 50982 unnamed/unregistered in both
`TiffTags.TAGS` and `TAGS_V2`. The 208-byte strip-decoded 2x1 L fixture
preserves pixels `[17,34]`; `getexif()` and `tag_v2` return the exact tuple
from `0.0` through `4.25` in `0.25` steps; info reports raw compression plus
`(1,1)` DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in
`94ms` / `47ms`; raw `open_tiff` passes `181/181` in `735ms`; facade
`getexif` passes `174/174` in `2328ms`; full passes `2455/2455` in `26984ms`
with registrations `1209/1246`. Release x64 builds with zero warnings/errors;
exports remain `443/443`, zero difference; SHA-256 is
`4DDD2E0207F2A4C92FA542B58D0660A6A84C093CA1A21E5E1E1104DC0C26F1E6`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-06: `META-002AW` extends the established native
TIFF FLOAT-array route to bounded `ProfileLookTableData` 50982 type-11/count-6.
Pillow 11.3.0 leaves 50982 unnamed/unregistered in both `TiffTags.TAGS` and
`TAGS_V2`. The 160-byte strip-decoded 2x1 L fixture preserves pixels
`[17,34]`; `getexif()` and `tag_v2` return exact tuple
`(0.0,1.0,1.0,0.5,0.75,1.25)`; info reports raw compression plus `(1,1)`
DPI/resolution and no warnings. Raw/facade GREEN passes `1/1` in `79ms` /
`47ms`; raw `open_tiff` passes `180/180` in `609ms`; facade `getexif` passes
`173/173` in `2265ms`; full passes `2453/2453` in `27360ms` with registrations
`1208/1245`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`A196C4BE9A353E5A104EAE2A528B31DA0BA053056D4C2D5969BE0766D4E9B03B`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AV` extends the established native
TIFF LONG-array route to bounded `ProfileLookTableDims` 50981 type-4/count-3.
Pillow 11.3.0 leaves 50981 unnamed/unregistered in both `TiffTags.TAGS` and
`TAGS_V2`. The 148-byte strip-decoded 2x1 L fixture preserves pixels
`[17,34]`; `getexif()` and `tag_v2` return exact tuple `(6,3,1)`; info reports
raw compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `78ms` / `47ms`; raw `open_tiff` passes `179/179` in `657ms`;
facade `getexif` passes `172/172` in `2360ms`; full passes `2451/2451` in
`27063ms` with registrations `1207/1244`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`D3C5327D0027B9292427B7187AB8016A58BF9B7470DBD204FC623ECFA13F2BD5`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AU` extends the established native
TIFF scalar-RATIONAL route to bounded `NoiseReductionApplied` 50935
type-5/count-1. Pillow 11.3.0 leaves 50935 unnamed/unregistered in both
`TiffTags.TAGS` and `TAGS_V2`. The 144-byte strip-decoded 2x1 L fixture
preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact
`IFDRational(3,4)`; info reports raw compression plus `(1,1)` DPI/resolution
and no warnings. Raw/facade GREEN passes `1/1` in `78ms` / `32ms`; raw
`open_tiff` passes `178/178` in `641ms`; facade `getexif` passes `171/171` in
`2281ms`; full passes `2449/2449` in `27594ms` with registrations `1206/1243`.
Release x64 builds with zero warnings/errors; exports remain `443/443`, zero
difference; SHA-256 is
`8BCFDA444FC3DD98B21D5A01AF6C63ADA3BDA22858D0A7B60AE3DAD91E82AD77`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AT` extends the established native
TIFF FLOAT-array route to one bounded `ProfileToneCurve` 50940 type-11/count-6
three-control-point curve. Pillow 11.3.0 leaves 50940 unnamed/unregistered in
both `TiffTags.TAGS` and `TAGS_V2`. The 160-byte strip-decoded 2x1 L fixture
preserves pixels `[17,34]`; `getexif()` and `tag_v2` return exact tuple
`(0.0,0.0,0.5,0.25,1.0,1.0)`; info reports raw compression plus `(1,1)` DPI/
resolution and no warnings. Raw/facade GREEN passes `1/1` in `94ms` / `15ms`;
raw `open_tiff` passes `177/177` in `641ms`; facade `getexif` passes `170/170`
in `2281ms`; full passes `2447/2447` in `41344ms` with registrations
`1205/1242`. Release x64 builds with zero warnings/errors; exports remain
`443/443`, zero difference; SHA-256 is
`EDF2043BBEC611FEBA4261687FBE0A4907DCECC07FCC5639A641BAC91E941373`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AS` extends the established native
TIFF FLOAT-array route to `ProfileHueSatMapData2` 50939 type-11/count-6.
Pillow 11.3.0 leaves 50939 unnamed/unregistered in both `TiffTags.TAGS` and
`TAGS_V2`. The 160-byte strip-decoded 2x1 L fixture preserves pixels
`[17,34]`; `getexif()` and `tag_v2` return exact float32-quantized tuple
`(0.0,-0.125,0.20000000298023224,1.75,-2.5,3.125)`; info reports raw
compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `94ms` / `31ms`; raw `open_tiff` passes `176/176` in `625ms`;
facade `getexif` passes `169/169` in `2312ms`; full passes `2445/2445` in
`26453ms` with registrations `1204/1241`. Release x64 builds with zero
warnings/errors; exports remain `443/443`, zero difference; SHA-256 is
`0DF7F6FB0637D9151F336EA21A60F3891960D25A88DD56FCB00C00B57BBCFB5E`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AR` adds the first bounded native
TIFF FLOAT-array route for `ProfileHueSatMapData1` 50938 type-11/count-6.
Pillow 11.3.0 leaves 50938 unnamed/unregistered in both `TiffTags.TAGS` and
`TAGS_V2`. The 160-byte strip-decoded 2x1 L fixture preserves pixels
`[17,34]`; `getexif()` and `tag_v2` return exact float32-quantized tuple
`(0.0,0.10000000149011612,-0.25,1.5,2.25,-3.75)`; info reports raw
compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `16ms` / `47ms`; raw `open_tiff` passes `175/175` in `922ms`;
facade `getexif` passes `168/168` in `2406ms`; full passes `2443/2443` in
`27375ms` with registrations `1203/1240`. Release x64 builds with zero
warnings/errors; exports are `443/443`, zero difference; SHA-256 is
`885E91AA9C7A2665A2064DF55499F65C0B97D00841440D91797FEED45186729C`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AQ` extends the established native
LONG-array route to TIFF `ProfileHueSatMapDims` 50937 type-4/count-3. Pillow
11.3.0 leaves 50937 unnamed/unregistered in both `TiffTags.TAGS` and
`TAGS_V2`. The 148-byte strip-decoded 2x1 L fixture preserves pixels
`[17,34]`; `getexif()` and `tag_v2` return exact tuple `(6,3,1)`; info reports
raw compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `63ms` / `47ms`; raw `open_tiff` passes `174/174` in `359ms`;
facade `getexif` passes `167/167` in `1438ms`; full passes `2441/2441` in
`15750ms` with registrations `1202/1239`. Release x64 builds with zero
warnings/errors; exports remain `442/442`, zero difference; SHA-256 is
`269AE21A0052C16CE1C34EC9CDAFE1FA5A31732B47421D2BE34F0F9028020ABA`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AP` extends the established native
DOUBLE-array route to TIFF `RPCCoefficientTag` 50844 type-12/count-92. Pillow
11.3.0 leaves 50844 unnamed/unregistered in both `TiffTags.TAGS` and
`TAGS_V2`. The 872-byte strip-decoded 2x1 L fixture preserves pixels
`[17,34]`; `getexif()` and `tag_v2` return the exact 92-item tuple defined by
`values[index] = (index - 46) / 8.0`, from `-5.75` through `5.625`; info
reports raw compression plus `(1,1)` DPI/resolution and no warnings. Raw/
facade GREEN passes `1/1` in `109ms` / `63ms`; raw `open_tiff` passes
`173/173` in `766ms`; facade `getexif` passes `166/166` in `2562ms`; full
passes `2439/2439` in `28187ms` with registrations `1201/1238`. Release x64
builds with zero warnings/errors; exports remain `442/442`, zero difference;
SHA-256 is
`F59E6E398D92A50D6815A3932531392A3994DBB4AFC117E7AA31D45C3C8E92D4`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AO` batches TIFF
`GDAL_METADATA` 42112 and `GDAL_NODATA` 42113 through the established native
ASCII route. Pillow 11.3.0 names both only in legacy `TiffTags.TAGS`;
`TAGS_V2` has no registered type or length. The 211-byte strip-decoded 2x1 L
fixture preserves pixels `[17,34]`; type-2/count-57 42112 returns exact XML
string `<GDALMetadata><Item name="scale">2</Item></GDALMetadata>` and
count-6 42113 returns `-9999`, with only NUL removed; info reports raw
compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `141ms` / `31ms`; raw `open_tiff` passes `172/172` in `640ms`;
facade `getexif` passes `165/165` in `2093ms`; full passes `2437/2437` in
`25547ms` with registrations `1200/1237`. Release x64 builds with zero
warnings/errors; exports remain `442/442`, zero difference; SHA-256 is
`D5DBA342EB4F5AFE5AF7ABA04F1A55A22DEFC412F9CF0996D55FF8772AB1195B`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AN` extends the established native
SHORT-array route to TIFF `GeoKeyDirectoryTag` 34735 type-3/count-8. Pillow
11.3.0 names the tag only in legacy `TiffTags.TAGS`; `TAGS_V2` has no
registered type or length. The 152-byte strip-decoded 2x1 L fixture preserves
pixels `[17,34]`; `getexif()` and `tag_v2` return exact tuple
`(1,1,0,1,1024,0,1,1)`; info reports raw compression plus `(1,1)` DPI/
resolution and no warnings. Raw/facade GREEN passes `1/1` in `125ms` /
`31ms`; raw `open_tiff` passes `171/171` in `593ms`; facade `getexif` passes
`164/164` in `2015ms`; full passes `2435/2435` in `26922ms` with registrations
`1199/1236`. Release x64 builds with zero warnings/errors; exports remain
`442/442`, zero difference; SHA-256 is
`DE431941CBB09198CB17730D4256E9CCEB75E3B9663E196468E9E4F6FE6BF771`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AM` extends the established native
ASCII route to TIFF `GeoAsciiParamsTag` 34737 type-2/count-15. Pillow 11.3.0
names the tag only in legacy `TiffTags.TAGS`; `TAGS_V2` has no registered type
or length. The 151-byte strip-decoded 2x1 L fixture preserves pixels
`[17,34]`; `getexif()` and `tag_v2` return exact string `WGS 84|meters|`,
preserving the trailing `|` while omitting the NUL terminator; info reports
raw compression plus `(1,1)` DPI/resolution and no warnings. Raw/facade GREEN
passes `1/1` in `94ms` / `47ms`; raw `open_tiff` passes `170/170` in `657ms`;
facade `getexif` passes `163/163` in `3203ms`; full passes `2433/2433` in
`26453ms` with registrations `1198/1235`. Release x64 builds with zero
warnings/errors; exports remain `442/442`, zero difference; SHA-256 is
`7242AEA0D0FC8A69A9BDEEB7F05E5C8E753F7FCFC415BACAEDAC1740B3BD4C91`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AL` extends the generalized native
floating-array route to TIFF `GeoDoubleParamsTag` 34736 DOUBLE/count-3.
Pillow 11.3.0 names the tag only in legacy `TiffTags.TAGS`; `TAGS_V2` has no
registered type or length. The 160-byte strip-decoded 2x1 L fixture preserves
pixels `[17,34]`, returns exact tuple `(6378137.0,298.257223563,-123.5)` from
`getexif()` and `tag_v2`, reports raw compression plus `(1,1)` DPI/resolution,
and emits no warnings. Raw/facade GREEN passes `1/1` in `94ms` / `31ms`; raw
`open_tiff` passes `169/169` in `609ms`; facade `getexif` passes `162/162` in
`1985ms`; full passes `2431/2431` in `26156ms` with registrations `1197/1234`.
Release x64 builds with zero warnings/errors; exports remain `442/442`, zero
difference; SHA-256 is
`0C38092979CDEEC528AD94E7745761910AC6D561564B57E0A7B2598F0960F73B`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AK` extends the generalized native
floating-array route to TIFF `ModelTransformationTag` 34264 DOUBLE/count-16.
Pillow 11.3.0 names the tag only in legacy `TiffTags.TAGS`; `TAGS_V2` has no
registered type or length. The 264-byte strip-decoded 2x1 L fixture preserves
pixels `[17,34]`, returns exact tuple
`(1,0,0,100.5,0,1,0,200.25,0,0,1,300.75,0,0,0,1)` from `getexif()` and
`tag_v2`, reports raw compression plus `(1,1)` DPI/resolution, and emits no
warnings. Raw/facade GREEN passes `1/1` in `109ms` / `62ms`; raw `open_tiff`
passes `168/168` in `609ms`; facade `getexif` passes `161/161` in `2093ms`;
full passes `2429/2429` in `27203ms` with registrations `1196/1233`. Release
x64 builds with zero warnings/errors; exports remain `442/442`, zero
difference; SHA-256 is
`F1A3D3518671B4F57BF028E64660F973E890C3788FF01FD4674E2D46D161820C`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AJ` extends the generalized native
floating-array route to TIFF `ModelTiepointTag` 33922 DOUBLE/count-6. Pillow
11.3.0 names the tag only in legacy `TiffTags.TAGS`; `TAGS_V2` has no
registered type or length. The 184-byte strip-decoded 2x1 L fixture preserves
pixels `[17,34]`, returns exact tuple `(0,0,0,10.5,20.25,30.75)` from
`getexif()` and `tag_v2`, reports raw compression plus `(1,1)` DPI/resolution,
and emits no warnings. Raw/facade GREEN passes `1/1` in `78ms` / `31ms`; raw
`open_tiff` passes `167/167` in `609ms`; facade `getexif` passes `160/160` in
`2016ms`; full passes `2427/2427` in `26406ms` with registrations `1195/1232`.
Release x64 builds with zero warnings/errors; exports remain `442/442`, zero
difference; SHA-256 is
`C506D9BA6465843EE44DF934E48286BC3BF1FF35A15F6EC3FDEA5260D9285D86`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AI` adds bounded TIFF
`ModelPixelScaleTag` 33550 DOUBLE/count-3 readback through a reusable native
floating-array ABI. Pillow 11.3.0 names the tag only in legacy
`TiffTags.TAGS`; `TAGS_V2` has no registered type or length. The 160-byte
strip-decoded 2x1 L fixture preserves pixels `[17,34]`, returns exact tuple
`(0.5,1.25,2.75)` from `getexif()` and `tag_v2`, reports raw compression plus
`(1,1)` DPI/resolution, and emits no warnings. Raw/facade GREEN passes `1/1`
in `94ms` / `31ms`; raw `open_tiff` passes `166/166` in `625ms`; facade
`getexif` passes `159/159` in `2157ms`; full passes `2425/2425` in `27641ms`
with registrations `1194/1231`. Release x64 builds with zero warnings/errors;
exports are `442/442`, zero difference; SHA-256 is
`648CB69FC634E4C33BB86698C674AF555C32072BDCEEC144707AC95B7212C8C8`.
The rounded complete-runtime estimate remains `62%` because this bounded
metadata child does not change a full capability-matrix point.

Incremental refresh, 2026-08-05: `META-002AH` adds bounded TIFF
`PhotoshopInfo` tag 34377 BYTE/count-14 readback. The 150-byte strip-decoded
2x1 L fixture preserves pixels `[17,34]`, leaves `Info["photoshop"]`,
`Info["iptc"]`, and `Info["exif"]` absent, and Pillow 11.3.0 returns exact
bytes `38 42 49 4D 04 04 00 00 00 00 00 02 41 42` from `getexif()` and
`tag_v2` without warnings. Raw/facade GREEN passes `1/1` in `16ms` / `32ms`;
raw `open_tiff` passes `165/165` in `375ms`; facade `getexif` passes `158/158`
in `1219ms`; full passes `2423/2423` in `15656ms` with registrations
`1193/1230`. Release x64 builds with zero warnings/errors; exports remain
`441/441`, zero difference; SHA-256 is
`0374A7AE74A4227D4041D5A08A4BB96F711B931F5C3A5505013F2E2E54720202`.
The rounded complete-runtime estimate remains `62%`.

Incremental refresh, 2026-08-05: `META-002AG` adds bounded TIFF
`IptcNaaInfo` tag 33723 type-7 readback. The 144-byte strip-decoded 2x1 L
fixture preserves pixels `[17,34]`, leaves `Info["iptc"]` / `Info["exif"]`
absent, and Pillow 11.3.0 returns exact bytes `1C 02 05 00 03 41 48 4B` from
`getexif()` and `tag_v2` without warnings. Raw/facade GREEN passes `1/1` in
`93ms` / `31ms`; raw `open_tiff` passes `164/164` in `375ms`; facade
`getexif` passes `157/157` in `1204ms`; full passes `2421/2421` in `16344ms`
with registrations `1192/1229`. Release x64 builds with zero warnings/errors;
exports remain `441/441`, zero difference; SHA-256 is
`AF347A62BB8B3B15DDA76AB7946D50205652E20C41F1AFD72896003086183D19`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001FD` removes the synthetic count-2
ceiling from non-pixel-source TIFF `TileOffsets` 324 / `TileByteCounts` 325
LONG-array recognition and serialization. The 172-byte strip-decoded 2x1 L
fixture preserves pixels `[17,34]`, leaves `Info["exif"]` absent, and Pillow
11.3.0 returns exact tuples `(200,220,240)` / `(2,2,2)` from `getexif()` and
`tag_v2` without warnings. Raw/facade GREEN passes `1/1` in `62ms` / `31ms`;
raw `open_tiff` passes `163/163` in `375ms`; facade `getexif` passes `156/156`
in `1234ms`; full passes `2419/2419` in `15953ms` with registrations
`1191/1228`. Release x64 builds with zero warnings/errors; exports remain
`441/441`, zero difference; SHA-256 is
`ED14677939A8C54A58A724D99EBABE5CC5C6ECF2F55F7A43A6A2EF62B585E235`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001FC` removes the synthetic fixed
count ceiling from the native TIFF `StripOffsets` 273 / `StripByteCounts` 279
LONG-array route. The 182-byte 2x6 L fixture preserves pixels
`[17,34,51,68,85,102,119,136,153,170,187,204]`, leaves `Info["exif"]`
absent, and Pillow 11.3.0 returns exact tuples `(170,172,174,176,178,180)` /
`(2,2,2,2,2,2)` from `getexif()` and `tag_v2` without warnings. Raw/facade
GREEN passes `1/1` in `171ms` / `62ms`; raw `open_tiff` passes `162/162` in
`656ms`; facade `getexif` passes `155/155` in `1969ms`; full passes
`2417/2417` in `27469ms` with registrations `1190/1227`. Release x64 builds
with zero warnings/errors; exports remain `441/441`, zero difference;
SHA-256 is
`1F11AE2A5BB584B73A93C9798FDA1A09D05C0CCC0076EB8F666E8E6BA4E43DF5`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001FB` extends the native TIFF
LONG-array route to valid five-strip `StripOffsets` 273 and
`StripByteCounts` 279 count 5. The 172-byte 2x5 L fixture preserves pixels
`[17,34,51,68,85,102,119,136,153,170]`, leaves `Info["exif"]` absent, and
Pillow 11.3.0 returns exact tuples `(162,164,166,168,170)` / `(2,2,2,2,2)`
from `getexif()` and `tag_v2` without warnings. Raw/facade GREEN passes `1/1`
in `125ms` / `62ms`; raw `open_tiff` passes `161/161` in `610ms`; facade
`getexif` passes `154/154` in `1937ms`; full passes `2415/2415` in `27953ms`
with registrations `1189/1226`. Release x64 builds with zero warnings/errors;
exports remain `441/441`, zero difference; SHA-256 is
`A7BBA1529EB2D2D8D26097A631D8BD306D44CC8A3EB9A201B0DF2BE3500B1C72`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001FA` extends the native TIFF
LONG-array route to valid four-strip `StripOffsets` 273 and
`StripByteCounts` 279 count 4. The 162-byte 2x4 L fixture preserves pixels
`[17,34,51,68,85,102,119,136]`, leaves `Info["exif"]` absent, and Pillow
11.3.0 returns exact tuples `(154,156,158,160)` / `(2,2,2,2)` from
`getexif()` and `tag_v2` without warnings. Raw/facade GREEN passes `1/1` in
`125ms` / `63ms`; raw `open_tiff` passes `160/160` in `563ms`; facade
`getexif` passes `153/153` in `1859ms`; full passes `2413/2413` in `27094ms`
with registrations `1188/1225`. Release x64 builds with zero warnings/errors;
exports remain `441/441`, zero difference; SHA-256 is
`6DE3FE177B939B06D49233D069F21908337E5E4107667B8656B9B1D2BA7F7703`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EZ` extends the native TIFF
LONG-array route to valid three-strip `StripOffsets` 273 and
`StripByteCounts` 279 count 3. The 152-byte 2x3 L fixture preserves pixels
`[17,34,51,68,85,102]`, leaves `Info["exif"]` absent, and Pillow 11.3.0
returns exact tuples `(146,148,150)` / `(2,2,2)` from `getexif()` and
`tag_v2` without warnings. Raw/facade GREEN passes `1/1` in `79ms` / `32ms`;
raw `open_tiff` passes `159/159` in `343ms`; facade `getexif` passes `152/152`
in `1094ms`; full passes `2411/2411` in `23625ms` with registrations
`1187/1224`. Release x64 builds with zero warnings/errors; exports remain
`441/441`, zero difference; SHA-256 is
`9B47AA8572DBF44332A56B12732EE1FBAC35F7CE546D033ED4B3C6D515EB5EC9`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EY` extends the native TIFF
LONG-array route to valid two-strip `StripOffsets` 273 and `StripByteCounts`
279 count 2. The 142-byte 2x2 L fixture preserves pixels `[17,34,51,68]`,
leaves `Info["exif"]` absent, and Pillow 11.3.0 returns exact tuples
`(138,140)` / `(2,2)` from `getexif()` and `tag_v2` without warnings. Final
raw/facade GREEN passes `1/1` in `109ms` / `63ms`; raw `open_tiff` passes
`158/158` in `641ms`; facade `getexif` passes `151/151` in `1938ms`; full
passes `2409/2409` in `26500ms` with registrations `1186/1223`. Release x64
builds with zero warnings/errors; exports remain `441/441`, zero difference;
SHA-256 is
`71C9903631C8D7A3697156CFAB4F296D6E2389DC2A013656AE7219CCE91DEDCD`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EX` extends the native TIFF
LONG-array route to IFD0 `TileOffsets` 324 and `TileByteCounts` 325 count 2.
The 164-byte strip-based fixture preserves pixels `[17,34]`, leaves
`Info["exif"]` absent, and Pillow 11.3.0 returns exact tuples `(200,220)` /
`(2,2)` from `getexif()` and `tag_v2` without warnings. Raw/facade GREEN
passes `1/1` in `31ms` / `63ms`; the scalar-shape regression passes `1/1`;
raw `open_tiff` passes `157/157` in `735ms`; facade `getexif` passes `150/150`
in `1875ms`; full passes `2407/2407` in `27453ms` with registrations
`1185/1222`. Release x64 builds with zero warnings/errors; exports remain
`441/441`, zero difference; SHA-256 is
`B1A6B7078923BA0C3727F955B1B591797420ACA98A7E2B51D4318992F14A2C32`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EW` adds the generalized native
TIFF LONG-array route and `pillow_c_exif_uint_array_tag` required-count ABI for
IFD0 `MaskSubArea` 52536 count 4. The 152-byte fixture preserves pixels
`[17,34]`, leaves `Info["exif"]` absent, and Pillow 11.3.0 returns exact tuple
`(1,2,9,8)` from `getexif()` and `tag_v2`. Raw/facade GREEN passes `1/1` in
`109ms` / `31ms`; raw `open_tiff` passes `156/156` in `547ms`; facade
`getexif` passes `149/149` in `1828ms`; full passes `2405/2405` in `25157ms`
with registrations `1184/1221`. Release x64 builds with zero warnings/errors;
exports are `441/441`, zero difference; SHA-256 is
`57EAEE953EA7D1B3270FF60B83B2595E6A36E9EF3FFEEDA4B77592A01FF6DA4F`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EV` extends the native TIFF
UNDEFINED route to `IlluminantData1/2/3` 52533..52535. The 172-byte fixture
covers inline and out-of-line payloads, preserves pixels `[17,34]`, and leaves
`Info["exif"]` absent; Pillow 11.3.0 returns exact Python `bytes` from
`getexif()` and `tag_v2`. Raw/facade GREEN passes `1/1` in `109ms` / `31ms`;
raw `open_tiff` passes `155/155` in `579ms`; facade `getexif` passes `148/148`
in `1953ms`; full passes `2403/2403` in `27422ms` with registrations
`1183/1220`. Release x64 builds with zero warnings/errors; exports remain
`440/440`, zero difference; SHA-256 is
`999AC5A31CE4C771598CA143B4CF4FAA6D4EDEA495628C8EE859F3511683B322`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EU` extends the native TIFF count-9
SRATIONAL-array route to `CameraCalibration3` 52530, `ColorMatrix3` 52531,
and `ForwardMatrix3` 52532. The 376-byte fixture preserves pixels `[17,34]`
and leaves `Info["exif"]` absent; Pillow 11.3.0 returns tuples of nine exact
`IFDRational` values from `getexif()` and `tag_v2`. Raw/facade GREEN passes
`1/1` in `125ms` / `32ms`; raw `open_tiff` passes `154/154` in `547ms`;
facade `getexif` passes `147/147` in `1843ms`; full passes `2401/2401` in
`22594ms` with registrations `1182/1219`. Release x64 builds with zero
warnings/errors; exports remain `440/440`, zero difference; SHA-256 is
`C3DE15D28FF950967039DD6AF1A3F28C4C74AF729D9AE7BF2DC9DB00F76BB070`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001ET` extends the native TIFF scalar-
integer route to `CalibrationIlluminant3` 52529=`23`. The 136-byte fixture
preserves pixels `[17,34]` and leaves `Info["exif"]` absent; Pillow 11.3.0
returns Python `int(23)` from `getexif()` and `tag_v2`. Raw/facade GREEN passes
`1/1` in `63ms` / `15ms`; raw `open_tiff` passes `153/153` in `407ms`;
facade `getexif` passes `146/146` in `1078ms`; full passes `2399/2399` in
`15875ms` with registrations `1181/1218`. Release x64 builds with zero
warnings/errors; exports remain `440/440`, zero difference; SHA-256 is
`AC8F2763A8CB0BB0BD194AADBC47B6CAD765724650AC3BB3BFEF5B713209371D`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001ES` extends the native TIFF ASCII
route to `SemanticName` 52526=`foreground` and `SemanticInstanceID`
52528=`instance-1`. The 170-byte fixture preserves pixels `[17,34]` and leaves
`Info["exif"]` absent; Pillow 11.3.0 returns exact strings from `getexif()` and
`tag_v2`. Raw/facade GREEN passes `1/1` in `47ms` / `15ms`; raw `open_tiff`
passes `152/152` in `359ms`; facade `getexif` passes `145/145` in `1047ms`;
full passes `2397/2397` in `16625ms` with registrations `1180/1217`. Release
x64 builds with zero warnings/errors; exports remain `440/440`, zero
difference; SHA-256 is
`5EF9D7E038B52B2E44318943A7185D517468AF99883EF119133B3C014653BE65`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001ER` extends the native TIFF ASCII
route to `EnhanceParams` 51182=`gain=1`. The 143-byte fixture preserves pixels
`[17,34]` and leaves `Info["exif"]` absent; Pillow 11.3.0 returns the exact
string from `getexif()` and `tag_v2`. Raw/facade GREEN passes `1/1` in `125ms`
/ `31ms`; raw `open_tiff` passes `151/151` in `563ms`; facade `getexif` passes
`144/144` in `1734ms`; full passes `2395/2395` in `28141ms` with registrations
`1179/1216`. Release x64 builds with zero warnings/errors; exports remain
`440/440`, zero difference; SHA-256 is
`0324036328D67B153503661DDB77AB0742E74E63AF7FF582C367DB9FEE13F3CE`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EQ` extends the native TIFF scalar-
integer route to `DepthUnits` 51180=`1` and `DepthMeasureType` 51181=`2`. The
148-byte fixture preserves pixels `[17,34]` and leaves `Info["exif"]` absent;
Pillow 11.3.0 returns Python `int` values from `getexif()` and `tag_v2`.
Raw/facade GREEN passes `1/1` in `78ms` / `32ms`; raw `open_tiff` passes
`150/150` in `656ms`; facade `getexif` passes `143/143` in `1813ms`; full
passes `2393/2393` in `26812ms` with registrations `1178/1215`. Release x64
builds with zero warnings/errors; exports remain `440/440`, zero difference;
SHA-256 is
`B7D3D97DB7D2589CB4DD8D6FAE9BA27081961A1C0C1F52381229AE12D31603D7`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EP` extends the native TIFF scalar-
RATIONAL route to `DepthNear` 51178=`3/2` and `DepthFar` 51179=`25/2`. The
164-byte fixture preserves pixels `[17,34]` and leaves `Info["exif"]` absent;
Pillow 11.3.0 returns exact `IFDRational` values from `getexif()` and `tag_v2`.
Raw/facade GREEN passes `1/1` in `109ms` / `31ms`; raw `open_tiff` passes
`149/149` in `578ms`; facade `getexif` passes `142/142` in `1796ms`; full
passes `2391/2391` in `26687ms` with registrations `1177/1214`. Release x64
builds with zero warnings/errors; exports remain `440/440`, zero difference;
SHA-256 is
`A8B6B7AEB648214EEA06DD3F8C96BD640440D31EF917679237DD2C2EF831742C`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EO` extends the native TIFF scalar-
integer route to `DepthFormat` 51177=`1`. The 136-byte fixture preserves
pixels `[17,34]` and leaves `Info["exif"]` absent; Pillow 11.3.0 returns
Python `int(1)` from both `getexif()` and `tag_v2`. Raw/facade GREEN passes
`1/1` in `110ms` / `31ms`; raw `open_tiff` passes `148/148` in `547ms`;
facade `getexif` passes `141/141` in `1687ms`; full passes `2389/2389` in
`26203ms` with registrations `1176/1213`. Release x64 builds with zero
warnings/errors; exports remain `440/440`, zero difference; SHA-256 is
`31DBF42A0908ABED726E6093D75BF73E732237D911064158FD1B7E11414124D9`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EN` extends the native TIFF
RATIONAL-array route to count-4 `DefaultUserCrop` 51125 with exact nested
pairs `[[1,10],[2,10],[9,10],[8,10]]`. The 168-byte fixture preserves pixels
`[17,34]` and leaves `Info["exif"]` absent. Raw/facade GREEN passes `1/1` in
`109ms` / `31ms`; raw `open_tiff` passes `147/147` in `562ms`; facade
`getexif` passes `140/140` in `1828ms`; full passes `2387/2387` in `26703ms`
with registrations `1175/1212`. Release x64 builds with zero warnings/errors;
exports remain `440/440`, zero difference; SHA-256 is
`4B32B99FDF9526E607E6C7C0F8159DCC35AF8415A5065EA4FAFA99AB1705F17D`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EM` extends the native TIFF scalar-
RATIONAL route to `RawToPreviewGain` 51112=`7/4`. The 144-byte fixture
preserves pixels `[17,34]` and leaves `Info["exif"]` absent. Raw/facade GREEN
passes `1/1` in `110ms` / `31ms`; raw `open_tiff` passes `146/146` in `641ms`;
facade `getexif` passes `139/139` in `1735ms`; full passes `2385/2385` in
`26609ms` with registrations `1174/1211`. Release x64 builds with zero
warnings/errors; exports remain `440/440`, zero difference; SHA-256 is
`357F8B89DA7910145A0A204CA363BDB35264C9FCEB158F191716E1293D69E2DF`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EL` extends the native TIFF BYTE-
array route to `NewRawImageDigest` 51111=`00..0f`. The 152-byte fixture
preserves pixels `[17,34]` and leaves `Info["exif"]` absent. Raw/facade GREEN
passes `1/1` in `125ms` / `31ms`; raw `open_tiff` passes `145/145` in `515ms`;
facade `getexif` passes `138/138` in `1703ms`; full passes `2383/2383` in
`26922ms` with registrations `1173/1210`. Release x64 builds with zero
warnings/errors; exports remain `440/440`, zero difference; SHA-256 is
`456BBA568BBFD5326BAA0F6BE46055BA78B5093E6BBDB988CE84BE6E94959933`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EK` extends the native TIFF scalar-
integer route to `DefaultBlackRender` 51110=`1`. The 136-byte fixture
preserves pixels `[17,34]` and leaves `Info["exif"]` absent. Raw/facade GREEN
passes `1/1` in `125ms` / `32ms`; raw `open_tiff` passes `144/144` in `562ms`;
facade `getexif` passes `137/137` in `1656ms`; full passes `2381/2381` in
`27141ms` with registrations `1172/1209`. Release x64 builds with zero
warnings/errors; exports remain `440/440`, zero difference; SHA-256 is
`5801FD3430FAC67E031D3D98861D2A82AD1D42B7C27CBED3077411FF4E8BCA7B`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EJ` extends the native TIFF scalar-
SRATIONAL route to `BaselineExposureOffset` 51109=`-5/6`. The 144-byte
fixture preserves pixels `[17,34]` and leaves `Info["exif"]` absent. Raw/
facade GREEN passes `1/1` in `110ms` / `46ms`; raw `open_tiff` passes
`143/143` in `531ms`; facade `getexif` passes `136/136` in `1625ms`; full
passes `2379/2379` in `27328ms` with registrations `1171/1208`. Release x64
builds with zero warnings/errors; exports remain `440/440`, zero difference;
SHA-256 is
`733883A4A6BE43B9042A0F1782D73E66DBD32C090F40BAA62A207BFAD8F6F460`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EI` extends the native TIFF scalar-
integer route to `ProfileHueSatMapEncoding` 51107=`1` and
`ProfileLookTableEncoding` 51108=`0`. The 148-byte fixture preserves pixels
`[17,34]` and leaves `Info["exif"]` absent. Raw/facade GREEN passes `1/1` in
`109ms` / `31ms`; raw `open_tiff` passes `142/142` in `531ms`; facade
`getexif` passes `135/135` in `1688ms`; full passes `2377/2377` in `26187ms`
with registrations `1170/1207`. Release x64 builds with zero warnings/errors;
exports remain `440/440`, zero difference; SHA-256 is
`CD59FE9346A70B4468C472514AC8278ADC98EDF8C5F1CB966D37F21A67C761B1`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EH` extends the native TIFF scalar-
integer route to `ProfileEmbedPolicy` 50941=`1` and `PreviewColorSpace`
50970=`2`. The 148-byte fixture preserves pixels `[17,34]` and leaves
`Info["exif"]` absent. Raw/facade GREEN passes `1/1` in `109ms` / `47ms`;
raw `open_tiff` passes `141/141` in `547ms`; facade `getexif` passes `134/134`
in `1578ms`; full passes `2375/2375` in `26625ms` with registrations
`1169/1206`. Release x64 builds with zero warnings/errors; exports remain
`440/440`, zero difference; SHA-256 is
`3923CFEC64FABB5823DC6CBABDAA3A611DD97C667D84BEEBE26C8170AEE23BC5`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EG` extends the generalized native
TIFF SRATIONAL-array route to count-9 `ForwardMatrix1` 50964 and
`ForwardMatrix2` 50965 with exact signed nested pairs. The 292-byte fixture
preserves pixels `[17,34]` and leaves `Info["exif"]` absent. Raw/facade GREEN
passes `1/1` in `125ms` / `63ms`; raw `open_tiff` passes `140/140` in `516ms`;
facade `getexif` passes `133/133` in `1578ms`; full passes `2373/2373` in
`27141ms` with registrations `1168/1205`. Release x64 builds with zero
warnings/errors; exports remain `440/440`, zero difference; SHA-256 is
`0F84990C1AD7D85024D4437205D701F02820C5C32EAA329099BE2845180E302A`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EF` extends the generalized native
TIFF SRATIONAL-array route to count-9 `CameraCalibration1/2` 50723/50724 and
`ReductionMatrix1/2` 50725/50726 with exact signed nested pairs. The 460-byte
fixture preserves pixels `[17,34]` and leaves `Info["exif"]` absent. Raw/
facade GREEN passes `1/1` in `125ms` / `62ms`; raw `open_tiff` passes
`139/139` in `563ms`; facade `getexif` passes `132/132` in `1515ms`; full
passes `2371/2371` in `24797ms` with registrations `1167/1204`. Release x64
builds with zero warnings/errors; exports remain `440/440`, zero difference;
SHA-256 is
`4CC2EFACC00C3F718C1081E797E99D131252DF720DC9A198086023A2F2A6BC81`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001EE` adds a generalized native TIFF
SRATIONAL-array route plus `pillow_c_exif_signed_rational_array_tag`, covering
count-9 `ColorMatrix1` 50721 and `ColorMatrix2` 50722 with exact signed nested
pairs. The 292-byte fixture preserves pixels `[17,34]` and leaves
`Info["exif"]` absent. Raw/facade GREEN passes `1/1` in `109ms` / `63ms`;
raw `open_tiff` passes `138/138` in `578ms`; facade `getexif` passes `131/131`
in `1719ms`; full passes `2369/2369` in `26156ms` with registrations
`1166/1203`. Release x64 builds with zero warnings/errors; exports are
`440/440`, zero difference; SHA-256 is
`FC352ACA7327DD77B85F9CB48743248C6BC0058746E0EE018B5DE6B6647E7639`.
The rounded workflow estimate remains `67%`.

Incremental refresh, 2026-08-05: `META-001ED` adds bounded TIFF IFD0
RATIONAL-array `AsShotWhiteXY` 50729=`[[3127,10000],[3290,10000]]` count `2`
and `LensInfo` 50736=`[[24,1],[70,1],[28,10],[4,1]]` count `4` readback
through the existing native TIFF EXIF blob and facade `GetExif()` / `getexif()`
route. The 196-byte fixture preserves pixels `[17,34]` and leaves
`Info["exif"]` absent. Raw/facade GREEN passes `1/1` in `125ms` / `31ms`;
raw `open_tiff` passes `137/137` in `313ms`; facade `getexif` passes `130/130`
in `891ms`; full passes `2367/2367` in `16172ms` with registrations
`1165/1202`. Release x64 builds with zero warnings/errors; exports remain
`439/439`, zero difference; SHA-256 is
`2A57BFD683396184AA89F2E35B54DE33DBC468911CA3DB4C235D3401386D3F12`.
The rounded independent estimate remains `69%`.

Incremental refresh, 2026-08-05: `META-001EC` adds bounded TIFF IFD0 count-3
RATIONAL-array `AnalogBalance` 50727=`[[2,1],[1,1],[3,2]]` and
`AsShotNeutral` 50728=`[[1,2],[1,1],[2,3]]` readback through the existing
native TIFF EXIF blob and facade `GetExif()` / `getexif()` route. The 196-byte
fixture preserves pixels `[17,34]` and leaves `Info["exif"]` absent. Raw/
facade GREEN passes `1/1` in `125ms` / `78ms`; raw `open_tiff` passes
`136/136` in `484ms`; facade `getexif` passes `129/129` in `1500ms`; full
passes `2365/2365` in `27265ms` with registrations `1164/1201`. Release x64
builds with zero warnings/errors; exports remain `439/439`, zero difference;
SHA-256 is
`76A70C52BAFF46A86B849AA1A2286AE69BA07CCEC122E093B9203D2841671481`.
The rounded independent estimate remains `69%`.

Incremental refresh, 2026-08-05: `META-001EB` adds bounded TIFF IFD0 scalar-
SRATIONAL `BaselineExposure` 50730=`-3/2` and `ShadowScale` 50739=`5/4`
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. The 164-byte fixture preserves pixels `[17,34]` and leaves
`Info["exif"]` absent. Raw/facade GREEN passes `1/1` in `47ms` / `47ms`; raw
`open_tiff` passes `135/135` in `328ms`; facade `getexif` passes `128/128` in
`938ms`; full passes `2363/2363` in `15406ms` with registrations `1163/1200`.
Release x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`448BDF085FCF0A3AB436FE74F5176C206E48C9AF53246AB8F5953AE1B0FDDC7B`.
The rounded independent estimate remains `69%`.

Incremental refresh, 2026-08-05: `META-001EA` adds bounded TIFF IFD0 scalar-
RATIONAL `AntiAliasStrength` 50738=`4/5` and `BestQualityScale` 50780=`9/8`
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. The 164-byte fixture preserves pixels `[17,34]` and leaves
`Info["exif"]` absent. Raw/facade GREEN passes `1/1` in `46ms` / `78ms`; raw
`open_tiff` passes `134/134` in `297ms`; facade `getexif` passes `127/127` in
`922ms`; full passes `2361/2361` in `15843ms` with registrations `1162/1199`.
Release x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`787511F3B8F4E4CD655FB82FF76F5A54336473AEDD670DFF047B7BBF523BDC42`.
The rounded independent estimate remains `69%`.

Incremental refresh, 2026-08-05: `META-001DZ` adds bounded TIFF IFD0 scalar-
RATIONAL `LinearResponseLimit` 50734=`3/4` and `ChromaBlurRadius` 50737=`7/3`
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. The 164-byte fixture preserves pixels `[17,34]` and leaves
`Info["exif"]` absent. Raw/facade GREEN passes `1/1` in `62ms` / `31ms`; raw
`open_tiff` passes `133/133` in `297ms`; facade `getexif` passes `126/126` in
`937ms`; full passes `2359/2359` in `15734ms` with registrations `1161/1198`.
Release x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`D63D9B718A5FB91E97BD37DF061FC3DA31536A2434806A026B59A9D5E6D16FBB`.
The rounded independent estimate remains `69%`.

Incremental refresh, 2026-08-05: `META-001DY` adds bounded TIFF IFD0 scalar-
RATIONAL `BaselineNoise` 50731=`3/2` and `BaselineSharpness` 50732=`5/4`
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. The 164-byte fixture preserves pixels `[17,34]` and leaves
`Info["exif"]` absent. Raw/facade GREEN passes `1/1` in `62ms` / `31ms`; raw
`open_tiff` passes `132/132` in `422ms`; facade `getexif` passes `125/125` in
`907ms`; full passes `2357/2357` in `15282ms` with registrations `1160/1197`.
Release x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`9B8F454E583ABE7912E5215BD5712457AD94D3526705B62176961A5C6692980B`.
The rounded independent estimate remains `69%`.

Incremental refresh, 2026-08-05: `META-001DX` adds bounded TIFF IFD0 scalar-
integer `CalibrationIlluminant1` 50778=`17` and `CalibrationIlluminant2`
50779=`21` readback through the existing native TIFF EXIF blob and facade
`GetExif()` / `getexif()` route. The 148-byte fixture preserves pixels
`[17,34]` and leaves `Info["exif"]` absent. Raw/facade GREEN passes `1/1` in
`140ms` / `47ms`; raw `open_tiff` passes `131/131` in `328ms`; facade
`getexif` passes `124/124` in `953ms`; full passes `2355/2355` in `15547ms`
with registrations `1159/1196`. Release x64 builds with zero warnings/errors;
exports remain `439/439`, zero difference; SHA-256 is
`AD1AB30FD1A6855ECD9868EB75724A017CE644553F39E3437C96559917BFF808`.
The rounded independent estimate remains `69%`.

Incremental refresh, 2026-08-05: `META-001DW` adds bounded TIFF IFD0 scalar-
integer `CFALayout` 50711=`2` and `MakerNoteSafety` 50741=`1` readback through
the existing native TIFF EXIF blob and facade `GetExif()` / `getexif()` route.
The 148-byte fixture preserves pixels `[17,34]` and leaves `Info["exif"]`
absent. Raw/facade GREEN passes `1/1` in `125ms` / `31ms`; raw `open_tiff`
passes `130/130` in `328ms`; facade `getexif` passes `123/123` in `765ms`;
full passes `2353/2353` in `15703ms` with registrations `1158/1195`. Release
x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`FDF150A2D011F812CA29AE1EA3B31BA18E7CB00F875FE517EA3CD2C103303466`.
The rounded independent estimate remains `69%`.

Incremental refresh, 2026-08-05: `META-001DV` adds bounded TIFF IFD0 type-2
ASCII `CameraSerialNumber` 50735 and `ProfileCopyright` 50942 exact string
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Values `SN1` / `copyright` exercise inline and out-of-line
storage; pixels remain `[17,34]` and `Info["exif"]` remains absent. Raw/facade
GREEN passes `1/1` in `63ms` / `31ms`; raw `open_tiff` passes `129/129` in
`250ms`; facade `getexif` passes `122/122` in `718ms`; full passes `2351/2351`
in `14296ms` with registrations `1157/1194`. Release x64 builds with zero
warnings/errors; exports remain `439/439`, zero difference; SHA-256 is
`FE32ACE5FC6C4719E0B3CAE88C57A44C0672E5665AF816AA45683F144B51B286`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DU` adds bounded TIFF IFD0 type-2
ASCII `PreviewSettingsName` 50968 and `PreviewDateTime` 50971 exact string
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Values `SET` / `2026:08:05 12:34:56` exercise inline and
out-of-line storage; pixels remain `[17,34]` and `Info["exif"]` remains absent.
Raw/facade GREEN passes `1/1` in `94ms` / `47ms`; raw `open_tiff` passes
`128/128` in `329ms`; facade `getexif` passes `121/121` in `875ms`; full
passes `2349/2349` in `15688ms` with registrations `1156/1193`. Release x64
builds with zero warnings/errors; exports remain `439/439`, zero difference;
SHA-256 is
`14FF2E76A893DD74CBAA4CEA82802BC3DF79CCC7807AA8F8D432773C49ECD448`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DT` adds bounded TIFF IFD0 type-2
ASCII `PreviewApplicationName` 50966 and `PreviewApplicationVersion` 50967
exact string readback through the existing native TIFF EXIF blob and facade
`GetExif()` / `getexif()` route. Values `APP` / `1.2.3` exercise inline and
out-of-line storage; pixels remain `[17,34]` and `Info["exif"]` remains absent.
Raw/facade GREEN passes `1/1` in `93ms` / `31ms`; raw `open_tiff` passes
`127/127` in `328ms`; facade `getexif` passes `120/120` in `891ms`; full
passes `2347/2347` in `15406ms` with registrations `1155/1192`. Release x64
builds with zero warnings/errors; exports remain `439/439`, zero difference;
SHA-256 is
`8EF7C717F11C6A560E79E0226A6BE08ABEAFE1FDE24E4A0B28E0B781EF325CFB`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DS` adds bounded TIFF IFD0 type-2
ASCII `AsShotProfileName` 50934 and `ProfileName` 50936 exact string readback
through the existing native TIFF EXIF blob and facade `GetExif()` / `getexif()`
route. Values `ASP` / `profile` exercise inline and out-of-line storage;
pixels remain `[17,34]` and `Info["exif"]` remains absent. Raw/facade GREEN
passes `1/1` in `46ms` / `32ms`; raw `open_tiff` passes `126/126` in `313ms`;
facade `getexif` passes `119/119` in `797ms`; full passes `2345/2345` in
`15125ms` with registrations `1154/1191`. Release x64 builds with zero
warnings/errors; exports remain `439/439`, zero difference; SHA-256 is
`DC1F10F9AF4885B77BCC96747594B1306AB31F59C0741BC1404BEE3683412F4D`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DR` adds bounded TIFF IFD0 type-2
ASCII `CameraCalibrationSignature` 50931 and `ProfileCalibrationSignature`
50932 exact string readback through the existing native TIFF EXIF blob and
facade `GetExif()` / `getexif()` route. Values `CAL` / `profile` exercise
inline and out-of-line storage; pixels remain `[17,34]` and `Info["exif"]`
remains absent. Raw/facade GREEN passes `1/1` in `79ms` / `47ms`; raw
`open_tiff` passes `125/125` in `266ms`; facade `getexif` passes `118/118` in
`812ms`; full passes `2343/2343` in `15454ms` with registrations `1153/1190`.
Release x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`AB900BB1D9AD154313C68FCAF557F194E7EBA0200543D56D997DD0DAB6090CE9`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DQ` adds bounded TIFF IFD0 type-2
ASCII `UniqueCameraModel` 50708 and `OriginalRawFileName` 50827 exact string
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Values `CAM` / `raw.dng` exercise inline and out-of-line
storage; pixels remain `[17,34]` and `Info["exif"]` remains absent. Raw/facade
GREEN passes `1/1` in `140ms` / `47ms`; raw `open_tiff` passes `124/124` in
`469ms`; facade `getexif` passes `117/117` in `1172ms`; full passes
`2341/2341` in `24719ms` with registrations `1152/1189`. Release x64 builds
with zero warnings/errors; exports remain `439/439`, zero difference; SHA-256
is `A3623563D62A1AC7B6A647CCA6D075AC95191928396EF86B9793B28A577F6FF3`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DP` adds bounded TIFF IFD0 type-1
BYTE `LocalizedCameraModel` 50709 and `CFAPlaneColor` 50710 exact byte
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Values `[67,65,77,0,255]` / `[0,1,2]` exercise out-of-line
and inline storage; pixels remain `[17,34]` and `Info["exif"]` remains absent.
Raw/facade GREEN passes `1/1` in `63ms` / `32ms`; raw `open_tiff` passes
`123/123` in `343ms`; facade `getexif` passes `116/116` in `781ms`; full
passes `2339/2339` in `15469ms` with registrations `1151/1188`. Release x64
builds with zero warnings/errors; exports remain `439/439`, zero difference;
SHA-256 is
`654C1AEADF64FF64526EF041B980D0108BEB9855CC9543A727AE56BFBA34E39A`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DO` adds bounded TIFF IFD0 type-1
BYTE `DNGVersion` 50706 and `DNGBackwardVersion` 50707 exact byte readback
through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Values `[1,6,0,0]` / `[1,4,0,0]` preserve the standard
four-byte versions; pixels remain `[17,34]` and `Info["exif"]` remains absent.
Raw/facade GREEN passes `1/1` in `62ms` / `47ms`; raw `open_tiff` passes
`122/122` in `312ms`; facade `getexif` passes `115/115` in `782ms`; full
passes `2337/2337` in `15672ms` with registrations `1150/1187`. Release x64
builds with zero warnings/errors; exports remain `439/439`, zero difference;
SHA-256 is
`2F3228B64ABA7AF1E861F7830CDE98B704B78A9B0050B8F878D2DF09741DA468`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DN` adds bounded TIFF IFD0 type-7
`UserComment` 37510 and `ImageSourceData` 37724 exact UNDEFINED byte readback
through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Values `[65,66,0,255]` / `[73,83,68,0,255]` exercise inline
and out-of-line storage; pixels remain `[17,34]` and `Info["exif"]` remains
absent. Raw/facade GREEN passes `1/1` in `63ms` / `31ms`; raw `open_tiff`
passes `121/121` in `265ms`; facade `getexif` passes `114/114` in `797ms`;
full passes `2335/2335` in `15281ms` with registrations `1149/1186`. Release
x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`13231A1195E4B5EEDB3E0690EC3853FB08D6303A50DFF7D57EDE3ED835E1FD76`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DM` adds bounded TIFF IFD0
`CFAPattern` 41730 and `DeviceSettingDescription` 41995 exact UNDEFINED byte
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Values `[2,2,0,1]` / `[68,69,86,0,255]` exercise inline and
out-of-line storage; pixels remain `[17,34]` and `Info["exif"]` remains absent.
Raw/facade GREEN passes `1/1` in `125ms` / `62ms`; raw `open_tiff` passes
`120/120` in `485ms`; facade `getexif` passes `113/113` in `1094ms`; full
passes `2333/2333` in `26422ms` with registrations `1148/1185`. Release x64
builds with zero warnings/errors; exports remain `439/439`, zero difference;
SHA-256 is
`83F36D5C7E535214A7D6C08D3A121C6CCE461FB05E24222B56CE9490B8297FB2`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DL` adds bounded TIFF IFD0
`ComponentsConfiguration` 37121 and `MakerNote` 37500 exact UNDEFINED byte
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Values `[1,2,3,0]` / `[77,75,0,255,1,2]` exercise inline
and out-of-line storage; pixels remain `[17,34]` and `Info["exif"]` remains
absent. Raw/facade GREEN passes `1/1` in `109ms` / `47ms`; raw `open_tiff`
passes `119/119` in `469ms`; facade `getexif` passes `112/112` in `1093ms`;
full passes `2331/2331` in `26563ms` with registrations `1147/1184`. Release
x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`076456D7BE3BCC45989C6B1536C7E774A0A41048DE2335BAE8C63480564E15E5`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DK` adds bounded TIFF IFD0 `OECF`
34856 and `SpatialFrequencyResponse` 41484 exact UNDEFINED byte readback
through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Values `[1,0,2,255,3]` / `[9,0,8]` exercise out-of-line and
inline storage; pixels remain `[17,34]` and `Info["exif"]` remains absent.
Raw/facade GREEN passes `1/1` in `141ms` / `63ms`; raw `open_tiff` passes
`118/118` in `563ms`; facade `getexif` passes `111/111` in `1078ms`; full
passes `2329/2329` in `25859ms` with registrations `1146/1183`. Release x64
builds with zero warnings/errors; exports remain `439/439`, zero difference;
SHA-256 is
`EC0733C628CD70DBE9F0CAEB4E184B0A212687E5FE74870DB24CD6CCB2D13D17`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DJ` adds bounded TIFF IFD0 scalar-
LONG sensitivity tags 34865..34869 through the existing native TIFF EXIF blob
and facade `GetExif()` / `getexif()` route. Values are exact `100001` /
`200002` / `300003` / `400004` / `500005`; pixels remain `[17,34]` and
`Info["exif"]` remains absent. Raw/facade GREEN passes `1/1` in `78ms` /
`63ms`; raw `open_tiff` passes `117/117` in `422ms`; facade `getexif` passes
`110/110` in `1016ms`; full passes `2327/2327` in `25250ms` with
registrations `1145/1182`. Release x64 builds with zero warnings/errors;
exports remain `439/439`, zero difference; SHA-256 is
`F3915887A3CC9C8E8E273FE0FF90A7BF7ED26B905221A6898183DF6FCA3B5926`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DI` adds bounded TIFF IFD0
`ISOSpeedRatings` / `PhotographicSensitivity` 34855 and `SensitivityType`
34864 scalar-SHORT readback through the existing native TIFF EXIF blob and
facade `GetExif()` / `getexif()` route. Values are exact `400` / `3`; pixels
remain `[17,34]` and `Info["exif"]` remains absent. Raw/facade GREEN passes
`1/1` in `78ms` / `47ms`; raw `open_tiff` passes `116/116` in `406ms`;
facade `getexif` passes `109/109` in `1125ms`; full passes `2325/2325` in
`26219ms` with registrations `1144/1181`. Release x64 builds with zero
warnings/errors; exports remain `439/439`, zero difference; SHA-256 is
`51122D7BA4D7729761A67DA01661C902E201A7A8201B4364B95017B0A30DBF0A`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DH` adds bounded TIFF IFD0
`SubjectArea` 37396 exact two- and three-element SHORT-array readback through
the existing native TIFF EXIF blob and facade `GetExif()` / `getexif()` route.
Values `[7,9]` / `[7,9,11]` complement the previously covered count-4 shape;
pixels remain `[17,34]` and `Info["exif"]` remains absent. Raw GREEN passes
`1/1` in `78ms`; the existing facade route passes directly `1/1` in `47ms`;
raw `open_tiff` passes `115/115` in `250ms`; facade `getexif` passes `108/108`
in `672ms`; full passes `2323/2323` in `15047ms` with registrations
`1143/1180`. Release x64 builds with zero warnings/errors; exports remain
`439/439`, zero difference; SHA-256 is
`92B809C293235D145C7A4661FE9F522F7114D27BA9F0931399689871D9547FD1`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DG` adds bounded TIFF IFD0
`SubjectArea` 37396 exact four-element SHORT-array readback through the
existing native TIFF EXIF blob and facade `GetExif()` / `getexif()` route. The
value is `[7,9,11,13]`; pixels remain `[17,34]` and `Info["exif"]` remains
absent. Raw/facade GREEN passes `1/1` in `125ms` / `31ms`; raw `open_tiff`
passes `114/114` in `297ms`; facade `getexif` passes `107/107` in `579ms`;
full passes `2321/2321` in `15500ms` with registrations `1142/1179`. Release
x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`AE4857AB6077BD0A497A38BF610992EFF6F55C73D9704907CC3DDE7841A040CA`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DF` adds bounded TIFF IFD0
`SecurityClassification` 37394 and `ImageHistory` 37395 exact ASCII readback
through the existing native TIFF EXIF blob and facade `GetExif()` / `getexif()`
route. The values are `secret` / `edited`; pixels remain `[17,34]` and
`Info["exif"]` remains absent. Raw/facade GREEN passes `1/1` in `78ms` /
`31ms`; raw `open_tiff` passes `113/113` in `281ms`; facade `getexif` passes
`106/106` in `547ms`; full passes `2319/2319` in `15266ms` with registrations
`1141/1178`. Release x64 builds with zero warnings/errors; exports remain
`439/439`, zero difference; SHA-256 is
`E4F413F6B281C89446867AD175106DAA10E943BECE955A594BD67727EEB7E6F0`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DE` adds bounded TIFF IFD0
`SpectralSensitivity` 34852 exact ASCII readback through the existing native
TIFF EXIF blob and facade `GetExif()` / `getexif()` route. The value is
`spec42`; pixels remain `[17,34]` and `Info["exif"]` remains absent. Raw/
facade GREEN passes `1/1` in `47ms` / `47ms`; raw `open_tiff` passes
`112/112` in `282ms`; facade `getexif` passes `105/105` in `578ms`; full
passes `2317/2317` in `14969ms` with registrations `1140/1177`. Release x64
builds with zero warnings/errors; exports remain `439/439`, zero difference;
SHA-256 is
`26FAE27A742E16D04A222082183F1BDC82A94122F6D6D6875F2127827426E75B`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DD` adds bounded TIFF IFD0
`SubjectLocation` 41492 exact two-element SHORT-array readback through the
existing native TIFF EXIF blob and facade `GetExif()` / `getexif()` route.
The value is `[7,9]`; pixels remain `[17,34]` and `Info["exif"]` remains
absent. Raw/facade GREEN passes `1/1` in `47ms` / `31ms`; raw `open_tiff`
passes `111/111` in `265ms`; facade `getexif` passes `104/104` in `641ms`;
full passes `2315/2315` in `15187ms` with registrations `1139/1176`. Release
x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`8BF87B038AD9BD815697DBA1D0D982FA7A438A23B662E74C527ABDE17247F52F`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DC` adds bounded TIFF IFD0
`CompressedBitsPerPixel` 37122 and `ExposureIndex` 41493 scalar-RATIONAL
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Exact pairs are `[24,10]` and `[200,1]`; pixels remain
`[17,34]` and `Info["exif"]` remains absent. Raw/facade GREEN passes `1/1` in
`94ms` / `47ms`; raw `open_tiff` passes `110/110` in `312ms`; facade
`getexif` passes `103/103` in `578ms`; full passes `2313/2313` in `15188ms`
with registrations `1138/1175`. Release x64 builds with zero warnings/errors;
exports remain `439/439`, zero difference; SHA-256 is
`D8CBED6CF819880BD5026DEDD189764108E6C66063236E6F83E76695CEC6626B`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DB` adds bounded TIFF IFD0
`ExposureBiasValue` 37380 scalar-SRATIONAL readback through the existing
native TIFF EXIF blob and facade `GetExif()` / `getexif()` route. The exact
pair is `[-1,2]`; pixels remain `[17,34]` and `Info["exif"]` remains absent.
Raw RED failed `Expected [-1, 2], got []`; raw GREEN passes `1/1` in `62ms`,
and the existing facade enumeration passes the new public test directly
`1/1` in `31ms`. Raw `open_tiff` passes `109/109` in `250ms`; facade
`getexif` passes `102/102` in `594ms`; full passes `2311/2311` in `15078ms`
with registrations `1137/1174`. Release x64 builds with zero warnings/errors;
exports remain `439/439`, zero difference; SHA-256 is
`E5FB99F2E3763ABCEF004D9D20629763A9CB78411DD322DD396657CA9C2834BD`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001DA` adds bounded TIFF IFD0
`ShutterSpeedValue` 37377 and `BrightnessValue` 37379 scalar-SRATIONAL
readback through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Exact pairs are `[-3,2]` and `[7,4]`; `Info["exif"]`
remains absent. Final raw/facade passes `1/1` in `31ms` / `32ms`; raw
`open_tiff` passes `108/108` in `282ms`; facade `getexif` passes `101/101` in
`547ms`; final full passes `2309/2309` in `15250ms` with registrations `1136/1173`.
Release x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`EA91ABC0F5E4B96ACBC63661307B51C17EAD240785E12EAB390F912DB77D676A`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001CZ` adds bounded TIFF IFD0
`SubjectDistance` 37382 and `FocalLength` 37386 scalar-RATIONAL readback
through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Exact pairs are `[125,10]` and `[50,1]`; `Info["exif"]`
remains absent. Raw/facade GREEN passes `1/1` in `79ms` / `47ms`; raw
`open_tiff` passes `107/107` in `312ms`; facade `getexif` passes `100/100` in
`594ms`; full passes `2307/2307` in `15203ms` with registrations `1135/1172`.
Release x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`7478F8E56D39FF187E6FE1F916AD87DD45360E0BC60F440C90BA769907C59D21`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001CY` adds bounded TIFF IFD0
`ApertureValue` 37378 and `MaxApertureValue` 37381 scalar-RATIONAL readback
through the existing native TIFF EXIF blob and facade `GetExif()` /
`getexif()` route. Exact pairs are `[28,10]` and `[4,1]`; `Info["exif"]`
remains absent. Raw/facade GREEN passes `1/1` in `62ms` / `31ms`; raw
`open_tiff` passes `106/106` in `250ms`; facade `getexif` passes `99/99` in
`547ms`; full passes `2305/2305` in `14813ms` with registrations `1134/1171`.
Release x64 builds with zero warnings/errors; exports remain `439/439`, zero
difference; SHA-256 is
`D59D2A116DDAAA21B90B24468BC0EA94D23D1D922959420BE5DC09A44D0CF34B`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-001CX` adds bounded TIFF IFD0
`ExposureTime` 33434 and `FNumber` 33437 scalar-RATIONAL readback through the
existing native TIFF EXIF blob and facade `GetExif()` / `getexif()` route.
Exact pairs are `[1,125]` and `[14,5]`; `Info["exif"]` remains absent. Raw/
facade GREEN passes `1/1` in `62ms` / `31ms`; raw `open_tiff` passes `105/105`
in `250ms`; facade `getexif` passes `98/98` in `484ms`; full passes
`2303/2303` in `14703ms` with registrations `1133/1170`. Release x64 builds
with zero warnings/errors; exports remain `439/439`, zero difference; SHA-256
is `59C182E4C7353D41113CECF25755DB07399431B1B5CEEF56ACC6FE2EACAB9B1F`.
The comprehensive estimate remains `66.2%` under the production-usage model.

Incremental refresh, 2026-08-05: `META-003EN` adds one native 12-slot intent-
support batch plus low-level `intent_supported` and `is_intent_supported`.
Built-in and serialized/reopened sRGB expose keys 0..3 with exact input/
output/proof tuples `(True, True, True)` and integer `1` from every bounded
method call. Raw/facade GREEN passes `1/1` in `46ms` / `15ms`; ImageCms passes
`287/287` in `5640ms`; full passes `2301/2301` in `15516ms` with registrations
`1132/1169`. Release x64 builds with zero warnings/errors; exports are
`439/439`, zero difference; SHA-256 is
`A9FB9E89C4E684B6E3532D6863621961F0C7A5B95C2003F0C849B76AA459FDED`.
The current independent production-usage estimate is `66.2%`.

Incremental refresh, 2026-08-05: `META-003EM` adds one native 12-slot CLUT
batch and the low-level facade `clut` map. Built-in and serialized/reopened
sRGB expose keys 0..3 with exact input/output/proof tuples
`(False, False, True)`. Raw/facade GREEN passes `1/1` in `93ms` / `16ms`;
ImageCms passes `285/285` in `5625ms`; full passes `2299/2299` in `15422ms`
with registrations `1131/1168`. Release x64 builds with zero warnings/errors;
exports are `438/438`, zero difference; SHA-256 is
`4973418EFF0114EBF9C9E820472B4F80377FF3885622558EDC32E09E4806D42C`.
The current independent production-usage estimate is `66.2%`.

Incremental refresh, 2026-08-05: `META-003EL` adds one native named-color
batch plus low-level `colorant_table` and `colorant_table_out`. Both bounded
sRGB profile shapes return None with independent zero presence/count/required
slots. Raw/facade GREEN passes `1/1` in `63ms` / `31ms`; ImageCms passes
`283/283` in `5750ms`; full passes `2297/2297` in `15484ms` with registrations
`1130/1167`. Release x64 builds with zero warnings/errors; exports are
`437/437`, zero difference; SHA-256 was
`3B44D76135E56D3E6C0A6B01809BD7C742A0C1B62168FF6A72D5B7B665DF6DDF`.
The current independent production-usage estimate is `66.2%`.

Incremental refresh, 2026-08-05: `META-003EK` adds one native batch and two
low-level facade properties for sRGB `attributes` and optional
`colorimetric_intent`. Local Pillow 11.3.0 returns integer `0` and None on
built-in and serialized/reopened profiles. Raw/facade GREEN passes `1/1` in
`62ms` / `16ms`; ImageCms passes `281/281` in `6015ms`; full passes `2295/2295`
in `15438ms` with registrations `1129/1166`. Release x64 builds with zero
warnings/errors; exports are `436/436`, zero difference; SHA-256 is
`8F89C79B39791134190F5C86279E3D3C3FCC26970F4C568156E9D4D86AD30654`.
The comprehensive estimate remains `64.5%` under the end-to-end denominator.

Incremental refresh, 2026-08-05: `META-003EJ` adds one native condition batch
and three low-level facade properties for sRGB measurement/viewing structs and
viewing-condition text. Local Pillow 11.3.0 returns None for all three on built-
in and serialized/reopened profiles; all native slots remain zero without
defaults. Raw/facade GREEN passes `1/1` in `93ms` / `31ms`; ImageCms passes
`279/279` in `5688ms`; full passes `2293/2293` in `15188ms` with registrations
`1128/1165`. Release x64 builds with zero warnings/errors; exports are
`435/435`, zero difference; SHA-256 is
`DA35205BCAD86A9433557014EA99CAF0C1302DB700C35D21DA3B06CC0BB9F69C`.
The comprehensive estimate remains `64.5%` under the end-to-end denominator.

Incremental refresh, 2026-08-05: `META-003EI` adds one native UTF-8 batch and
two facade properties for low-level sRGB `screening_description` and `target`.
Local Pillow 11.3.0 returns None for both on built-in and serialized/reopened
profiles; independent presence/required slots remain zero without defaults.
Raw/facade GREEN passes `1/1` in `78ms` / `16ms`; ImageCms passes `277/277` in
`5750ms`; full passes `2291/2291` in `14984ms` with registrations `1127/1164`.
Release x64 builds with zero warnings/errors; exports are `434/434`, zero
difference; SHA-256 is
`170B5D9A6147745601AB83E26F20E9F3822DDE3903E260BA8643F81CCA19D8DC`.
The comprehensive estimate remains `64.5%` under the end-to-end denominator.

Incremental refresh, 2026-08-05: `META-003EH` adds one native batch and three
facade properties for optional low-level sRGB perceptual/saturation rendering-
intent gamut and technology signatures. Local Pillow 11.3.0 returns None for
all three on built-in and serialized/reopened profiles; presence/value slots
remain independently zero without defaults. Raw/facade GREEN passes `1/1` in
`46ms` / `16ms`; ImageCms passes `275/275` in `5656ms`; full passes
`2289/2289` in `15031ms` with registrations `1126/1163`. Release x64 builds
with zero warnings/errors; exports are `433/433`, zero difference; SHA-256 is
`87CA443DEC300C00391534CAA95F53F1A65FE786747995BDADC329B79827D9D2`.
The comprehensive estimate remains `64.5%` under the end-to-end denominator.

Incremental refresh, 2026-08-05: `META-003EG` adds one native batch for
low-level sRGB `creation_date`, `header_flags`, `header_manufacturer`,
`header_model`, and `profile_id`. A fixed serialized-sRGB header proves
Pillow's direct zero-based `tm_mon` exposure, exact four-byte signatures and
flags, and all 16 profile-ID bytes after source release. Raw/facade final GREEN
passes `1/1` in `16ms` / `15ms`; ImageCms passes `273/273` in `5703ms`; full
passes `2287/2287` in `15484ms` with registrations `1125/1162`. Release x64
builds with zero warnings/errors; exports are `432/432`, zero difference;
SHA-256 is
`7E06A77903BEC6A45B229400A9612CB44A09C67CA6E2FEEB142606AE5B5FBB8A`.
The comprehensive estimate remains `64.5%` under the end-to-end denominator.

Incremental refresh, 2026-08-05: `META-003EF` adds native-backed low-level
sRGB `chromaticity`. One query returns the Pillow-exact red/green/blue xyY tag
triples and preserves built-in floating-point versus serialized/reopened
s15Fixed16 precision. Raw/facade RED/GREEN passes `1/1` in `94ms` / `15ms`;
ImageCms passes `271/271` in `5797ms`; full passes `2285/2285` in `15156ms`
with registrations `1124/1161`. Release x64 builds with zero warnings/errors;
exports are `431/431`, zero difference; SHA-256 is
`BB429CE6CFC1C951945B792081A8AB161A0346A4F2CBA3F65543CAF3DACA7023`.
The comprehensive estimate remains `64.5%` under the end-to-end denominator.

Incremental refresh, 2026-08-05: `META-003EE` adds one native batch and two
low-level facade properties for optional sRGB `media_black_point` and
`luminance`. Local Pillow 11.3.0 returns `None` for both tags on built-in and
serialized/reopened sRGB profiles; the ABI preserves independent absence and
zeroes all value slots without defaults. Raw/facade RED/GREEN passes `1/1` in
`78ms` / `15ms`; ImageCms passes `269/269` in `5625ms`; full passes
`2283/2283` in `15313ms` with registrations `1123/1160`. Release x64 builds
with zero warnings/errors; exports are `430/430`, zero difference; SHA-256 is
`7022AA1168CF0D8EB2113A2C02EE43CAAE460DD2D22008AE22D1E29B5F3A5807`.
The comprehensive estimate remains `64.5%` under the end-to-end denominator.

Incremental refresh, 2026-08-05: `META-003ED` adds one native double-precision
transform batch for exact low-level sRGB `red_primary`, `green_primary`, and
`blue_primary` XYZ/xyY values. Raw/facade RED/GREEN passes `1/1` in `63ms` /
`16ms`; ImageCms passes `267/267` in `5735ms`; full passes `2281/2281` in
`15015ms` with registrations `1122/1159`. Release x64 builds with zero warnings/
errors; exports are `429/429`, zero difference; SHA-256 is
`652D5D7587D39E13CBEBC8BFDEEDA70E9D79BC3884E16C61AAA788AEBA95C07B`.
The comprehensive estimate remains `62.1%` under the eight-domain denominator.

Incremental refresh, 2026-08-05: `META-003EC` adds native-backed low-level sRGB
`chromatic_adaptation` with exact built-in/serialized 3x3 XYZ and row-wise xyY
matrices. Raw/facade RED/GREEN passes `1/1` in `47ms` / `16ms`; ImageCms passes
`265/265` in `5547ms`; full passes `2279/2279` in `14969ms` with registrations
`1121/1158`. Release x64 builds with zero warnings/errors; exports are
`428/428`, zero difference; SHA-256 is
`145125212DD172068C201D93F0BA0D83F49A84CBC0A74D4005E2D6BFC1920AC7`.
The comprehensive estimate remains `62.1%` under the eight-domain denominator.

Incremental refresh, 2026-08-05: `META-003EB` adds one native batch for exact
low-level sRGB `red_colorant`, `green_colorant`, and `blue_colorant` XYZ/xyY
values across built-in and serialized/reopened profiles. Raw/facade RED/GREEN
passes `1/1` in `94ms` / `15ms`; ImageCms passes `263/263` in `5625ms`; full
passes `2277/2277` in `15250ms` with registrations `1120/1157`. Release x64
builds with zero warnings/errors; exports are `427/427`, zero difference;
SHA-256 is
`B9588372B3558E2AF7CAE9EBF259979914F15BDBCE6F9AA6876B5F951B26AE1A`.
The comprehensive estimate remains `62.1%` under the eight-domain denominator.

Incremental refresh, 2026-08-05: `META-003EA` adds native-backed low-level sRGB
`media_white_point_temperature`, preserving exact built-in
`5000.726053819035` and serialized/reopened `5000.722328847392` values. Raw/
facade RED/GREEN passes `1/1` in `78ms` / `16ms`; ImageCms passes `261/261` in
`5828ms`; full passes `2275/2275` in `15219ms` with registrations `1119/1156`.
Release x64 builds with zero warnings/errors; exports are `426/426`, zero
difference; SHA-256 is
`8578C4AB19C614E11359F6295928FC1F1E217BA70EEA174F30E5B2384EB9363D`.
The comprehensive estimate remains `62.1%` under the eight-domain denominator.

Incremental refresh, 2026-08-05: `META-003DZ` adds native-backed low-level sRGB
`media_white_point`, preserving Pillow's distinct nominal built-in D50 and
serialized ICC fixed-point XYZ/xyY tuples. Raw RED then provenance-correction
GREEN passes `1/1` in `78ms`; facade RED/GREEN passes `1/1` in `31ms`;
ImageCms passes `259/259` in `5750ms`; full passes `2273/2273` in `15203ms`
with registrations `1118/1155`. Release x64 builds with zero warnings/errors;
exports are `425/425`, zero difference; SHA-256 is
`62C3DB2C5B6180784F31AC57735E36B6F5D7F07DC63ADC960D7B5F2C4DADC99D`.
The comprehensive estimate remains `62.1%` under the eight-domain denominator.

Incremental refresh, 2026-08-05: `META-003DY` adds one native profile-header
query and six Pillow-exact low-level sRGB properties for signatures, matrix-
shaper state, and ICC versions. Raw RED/GREEN passes `1/1` in `79ms`; facade
RED then the corrected final GREEN passes `1/1` in `31ms`; combined ImageCms
passes `257/257` in `5828ms`; full passes `2271/2271` in `15078ms` with
registrations `1117/1154`. Release x64 builds with zero warnings/errors;
exports are `424/424`, zero difference; SHA-256 is
`8E9219B6B39517D365F62C620BC1EE0382D33A0AF391DFD5C7BA4F97036DFA4B`.
The comprehensive estimate remains `62.1%` under the eight-domain denominator.

Incremental refresh, 2026-08-05: `META-003DX` exposes Pillow-exact low-level
sRGB `CmsProfile` description, copyright, manufacturer, model, and rendering-
intent properties through established native queries. Facade RED reported
missing `profile_description`; GREEN passes `1/1` in `31ms`, related raw
queries pass `6/6` in `32ms`, combined ImageCms passes `255/255` in `5625ms`,
and full passes `2269/2269` in `14797ms` with registrations `1116/1153`. No
native code or ABI changed; exports remain `423/423`, zero difference, and
SHA-256 remains
`9BE524D2B9F1BF769310D8579557C0984DCE8794D13BD107225C03E854608ED1`.
The comprehensive estimate remains `62.1%` under the eight-domain denominator.

Incremental refresh, 2026-08-05: `META-003DW` adds the public
`ImageCmsProfile.tobytes()` spelling over the established native profile-byte
route while preserving Pillow's deliberate absence on low-level `CmsProfile`.
Facade RED reported the missing method; GREEN passes `1/1` in `15ms`, ImageCms
passes `254/254` in `5672ms`, and fresh full passes `2268/2268` in `15328ms`
with registrations `1116/1152`. No native code or ABI changed; exports remain
`423/423`, zero difference, and SHA-256 remains
`9BE524D2B9F1BF769310D8579557C0984DCE8794D13BD107225C03E854608ED1`.
The comprehensive estimate is `62.1%` under the new eight-domain denominator.

Incremental refresh, 2026-08-05: `META-003DV` adds exact existing-
`CmsProfile` wrapping for `ImageCms.getOpenProfile`. Native atomic retain/
release lets the resulting `ImageCmsProfile` share the same pointer, expose the
source as `profile`, and survive either close order without serialization
cloning. Raw RED/GREEN passes `1/1` in `109ms`; facade RED/GREEN `1/1` in
`31ms`; reverse close order `1/1` in `16ms`; ImageCms `253/253` in `5766ms`;
full `2267/2267` in `15093ms` with registrations `1116/1151`. Release x64
builds with zero warnings/errors; exports are `423/423`, zero difference;
SHA-256 is
`9BE524D2B9F1BF769310D8579557C0984DCE8794D13BD107225C03E854608ED1`.
The comprehensive AHK-first goal estimate remains `72.40%`.

Incremental refresh, 2026-08-05: `META-003DU` adds bounded file-like
`ImageCms.getOpenProfile` input. The facade bulk-reads the remaining 588-byte
AHK File stream to EOF without closing it and hands it to the established
native memory-open route; public `filename` is None, and the profile remains
independent after stream close/source deletion. Facade RED exposed the rejected
type; GREEN passes `1/1` in `31ms`, ImageCms `250/250` in `5766ms`, and full
`2264/2264` in `15187ms` with registrations `1115/1149`. No native code or ABI
changed; exports remain `422/422`, and SHA-256 remains
`C152B20A2FEA82ADE6FC0C107B0F1853EA55C9419E74C871103D61CE1B076298`.
The comprehensive AHK-first goal estimate remains `72.40%`.

Incremental refresh, 2026-08-05: `META-003DT` adds Pillow's distinct native
non-ASCII Windows path branch for `ImageCms.getOpenProfile`. The DLL reads the
UTF-16 path into native memory and closes it before creating the profile; the
facade leaves `filename` as None, the source deletes immediately, and exact
description/default intent remain available. Raw/facade REDs exposed the
missing export and wrong ANSI branch; GREEN passes `1/1` in `78ms` / `31ms`,
the path matrix `4/4` in `47ms`, ImageCms `249/249` in `5594ms`, and full
`2263/2263` in `15188ms` with registrations `1115/1148`. Release x64 builds
with zero warnings/errors; exports are `422/422`, zero difference; SHA-256 is
`C152B20A2FEA82ADE6FC0C107B0F1853EA55C9419E74C871103D61CE1B076298`.
The comprehensive AHK-first goal estimate remains `72.40%`.

Incremental refresh, 2026-08-05: `META-003DS` adds native-backed
`ImageCms.getOpenProfile` for one absolute ASCII path to a serialized 588-byte
sRGB ICC file. It preserves `filename`, returns exact `sRGB built-in\n`, holds
Pillow-exact Windows deletion locking while live, and releases it on close.
Raw/facade REDs exposed the missing export/String route; GREEN passes `1/1` in
`47ms` / `16ms`, ImageCms `247/247` in `5625ms`, and full `2261/2261` in
`14922ms` with registrations `1114/1147`. Release x64 builds with zero
warnings/errors; exports are `421/421` with zero set difference; SHA-256 is
`CC4249651999235AD3D666341A32CAAAD6DD7010A504CCD6F8B1B220EAF0EF27`.
The comprehensive AHK-first goal estimate remains `72.40%`.

Incremental refresh, 2026-08-05: `META-003DR` adds the distinct public
`ImageCms.getProfileDescription` method for existing in-memory sRGB profiles
by reusing the established native LittleCMS description route. Built-in and
memory-opened profiles return exact `sRGB built-in\n` after source-memory
release. The raw lifetime baseline passes `1/1` in `15ms`; facade RED exposed
the missing method and GREEN passes `1/1` in `32ms`; ImageCms passes `245/245`
in `5750ms`, and full passes `2259/2259` in `15640ms` with registrations
`1113/1146`. No native code or ABI changed; exports remain `420/420` with zero
set difference and SHA-256 remains
`36828F30514995CD98435BDD8C25DF67FEB7A94164377C7562C773ED56C14DEA`.
The comprehensive AHK-first goal estimate remains `72.40%`.

Incremental refresh, 2026-08-05: `META-003DQ` adds native-backed
`ImageCms.getProfileModel` for existing in-memory sRGB profiles. Pillow 11.3.0
returns exact `\n` for built-in and memory-opened profiles after source-memory
release via `(model or "") + "\n"`. Raw/facade REDs exposed the missing
export/method; GREEN passes `1/1` in `62ms` / `15ms`, ImageCms `244/244` in
`5750ms`, and full `2258/2258` in `14938ms` with registrations `1113/1145`.
Release x64 builds with zero warnings/errors; exports are `420/420` with zero
set difference; SHA-256 is
`36828F30514995CD98435BDD8C25DF67FEB7A94164377C7562C773ED56C14DEA`.
The comprehensive AHK-first goal estimate remains `72.40%`.

Incremental refresh, 2026-08-05: `META-003DP` adds native-backed
`ImageCms.getProfileManufacturer` for existing in-memory sRGB profiles.
Pillow 11.3.0 and its source fix exact `\n` for built-in and memory-opened
profiles after source-memory release via `(manufacturer or "") + "\n"`.
Raw RED exposed the missing export; the first GREEN exposed LittleCMS's absent
optional tag, and the root fix plus facade route pass `1/1` in `47ms` / `15ms`,
ImageCms `242/242` in `5765ms`, and full `2256/2256` in `14937ms` with
registrations `1112/1144`. Release x64 builds with zero warnings/errors;
exports are `419/419` with zero set difference; SHA-256 is
`773C252013B3DA54A342CBF84C70597072AD0341DA72335C4460B802092E48D7`.
The comprehensive AHK-first goal estimate remains `72.40%`.

Incremental refresh, 2026-08-05: `META-003DO` adds native-backed
`ImageCms.getProfileCopyright` for existing in-memory sRGB profiles. Pillow
11.3.0 returns exact `No copyright, use freely\n` for built-in and memory-
opened profiles after source-memory release. Raw/facade REDs exposed the
missing export/method; GREEN passes `1/1` in `109ms` / `32ms`, ImageCms
`240/240` in `5687ms`, and full `2254/2254` in `15062ms` with registrations
`1111/1143`. Release x64 builds with zero warnings/errors; exports are
`418/418` with zero set difference; SHA-256 is
`F76BF5219E05A577EE696DF4EBEBD8A0353314208BA0ED41CEB5606C96D7A69F`.
The comprehensive AHK-first goal estimate remains `72.40%`.

Incremental refresh, 2026-08-05: `META-003DN` adds native-backed
`ImageCms.getProfileInfo` for existing in-memory sRGB profiles. Pillow 11.3.0
returns exact `sRGB built-in\r\n\r\nNo copyright, use freely\r\n\r\n` for
built-in and memory-opened profiles after source-memory release. Raw/facade
REDs exposed the missing export/method; GREEN passes `1/1` in `78ms` / `16ms`,
ImageCms `238/238` in `5625ms`, and full `2252/2252` in `15281ms` with
registrations `1110/1142`. Release x64 builds with zero warnings/errors;
exports are `417/417` with zero set difference; SHA-256 is
`570E2737B412E5351A6BAB7E7EBA3400EB60912BCABE3BD0DB5A556BDD964BA9`.
The comprehensive AHK-first goal estimate remains `72.40%`.

Incremental refresh, 2026-08-05: `META-003DM` adds native-backed
`ImageCms.isIntentSupported` for existing in-memory sRGB profiles. Pillow
11.3.0 returns exact value `1` across intents `0..3` and input/output/proof
directions for built-in and memory-opened profiles after source-memory release.
Raw/facade REDs exposed a missing export/method; GREEN passes `1/1` in `78ms`
/ `16ms`, ImageCms `236/236` in `5531ms`, and full `2250/2250` in `15093ms`
with registrations `1109/1141`. Release x64 builds with zero warnings/errors;
exports are `416/416` with zero set difference; SHA-256 is
`D8CC164C7F1672B36DB0CB87DF80EB449E3C76576901119D40514D5CA7C12E06`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DL` adds native-backed
`ImageCms.getDefaultIntent` for existing in-memory sRGB profiles. Pillow 11.3.0
returns exact intent `0` for built-in and memory-opened profiles after source-
memory release. Raw/facade REDs exposed a missing export/method; GREEN passes
`1/1` in `78ms` / `15ms`, ImageCms `234/234` in `5546ms`, and full
`2248/2248` in `15343ms` with registrations `1108/1140`. Release x64 builds
with zero warnings/errors; exports are `415/415` with zero set difference;
SHA-256 is
`E9EB12B3B9AD7418CA5F3381AD522850FAEE95B552AA8E2DC6E190F006969EA2`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DK` adds absolute-colorimetric render
intent `3` to RGB/sRGB-to-LAB/LAB gamut-check proofing and completes intents
`0..3` for all four established RGB/LAB proof mode pairs. Pillow 11.3.0 fixes
the same exact 3x2/1x1 LAB bytes as DH-DJ, unchanged RGB sources/Info, distinct
572-byte ICC objects, and profile-memory-independent reuse. Raw/facade REDs
exposed `-3` / `cannot build proof transform`; GREEN passes `1/1` in `203ms` /
`172ms`, ImageCms `232/232` in `5797ms`, and full `2246/2246` in `15250ms`
with registrations `1107/1139`. Release x64 builds with zero warnings/errors;
exports remain `414/414` with zero set difference; SHA-256 is
`E0F0FBB92CA7F5DCDB102923E40391AC26D3A21A6808B1A4AD31F43787359FAC`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DJ` adds saturation render intent
`2` to RGB/sRGB-to-LAB/LAB gamut-check proofing. Pillow 11.3.0 fixes the same
exact 3x2/1x1 LAB bytes as DH/DI, unchanged RGB sources/Info, distinct 572-byte
ICC objects, and profile-memory-independent reuse. Raw/facade REDs exposed
`-3` / `cannot build proof transform`; GREEN passes `1/1` in `203ms` / `156ms`,
ImageCms `230/230` in `5360ms`, and full `2244/2244` in `14625ms` with
registrations `1106/1138`. Release x64 builds with zero warnings/errors;
exports remain `414/414` with zero set difference; SHA-256 is
`20435AF3C33EADB46C32EE3E6AA5E9FB34CBDC49D3E492E29BA770D1B491DD64`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DI` adds relative-colorimetric render
intent `1` to RGB/sRGB-to-LAB/LAB gamut-check proofing. Pillow 11.3.0 fixes the
same exact 3x2/1x1 LAB bytes as DH, unchanged RGB sources/Info, distinct 572-
byte ICC objects, and profile-memory-independent reuse. Raw/facade REDs exposed
`-3` / `cannot build proof transform`; GREEN passes `1/1` in `203ms` / `125ms`,
ImageCms `228/228` in `5047ms`, and full `2242/2242` in `15015ms` with
registrations `1105/1137`. Release x64 builds with zero warnings/errors;
exports remain `414/414` with zero set difference; SHA-256 is
`1D0A1D39AEE132C08C7394D66B7AE0F3C4582A3CCBA34F07A1C50A403DD56B55`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DH` opens RGB/sRGB-to-LAB/LAB gamut-
check proofing at perceptual render intent `0`. Pillow 11.3.0 fixes exact 3x2/
1x1 LAB bytes, unchanged RGB sources/Info, distinct 572-byte ICC objects, and
profile-memory-independent reuse. Raw/facade REDs exposed `-3` / `cannot build
proof transform`; GREEN passes `1/1` in `188ms` / `156ms`, ImageCms `226/226`
in `5016ms`, and full `2240/2240` in `14250ms` with registrations `1104/1136`.
Release x64 builds with zero warnings/errors; exports remain `414/414` with
zero set difference; SHA-256 is
`25105F88F8CF5A7C652E7634F58382A60B434520F7D54D9569CD6CDFC2CFB352`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DG` adds absolute-colorimetric render
intent `3` to RGB/sRGB-to-RGB/sRGB gamut-check proofing and completes intents
`0..3` for the pair. Pillow 11.3.0 fixes exact identity 3x2/1x1 RGB bytes,
unchanged sources/Info, distinct 588-byte ICC objects, and profile-memory-
independent reuse. Raw/facade REDs exposed `-3` / `cannot build proof
transform`; GREEN passes `1/1` in `203ms` / `172ms`, ImageCms `224/224` in
`4578ms`, and full `2238/2238` in `14062ms` with registrations `1103/1135`.
Release x64 builds with zero warnings/errors; exports remain `414/414` with
zero set difference; SHA-256 is
`756A717649DE278C54C4B52D94052ED6797C37A0C9746BDF2F91038A281EED2D`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DF` adds saturation render intent
`2` to RGB/sRGB-to-RGB/sRGB gamut-check proofing. Pillow 11.3.0 fixes exact
identity 3x2/1x1 RGB bytes, unchanged sources/Info, distinct 588-byte ICC
objects, and profile-memory-independent reuse. Raw/facade REDs exposed `-3` /
`cannot build proof transform`; GREEN passes `1/1` in `188ms` / `187ms`,
ImageCms `222/222` in `4328ms`, and full `2236/2236` in `13843ms` with
registrations `1102/1134`. Release x64 builds with zero warnings/errors;
exports remain `414/414` with zero set difference; SHA-256 is
`B35D3CE8A4B91A621CC9D4F8B1DA2A10FFA753C082A8B41208A4A10F69F5DEC0`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DE` adds relative-colorimetric
render intent `1` to RGB/sRGB-to-RGB/sRGB gamut-check proofing. Pillow 11.3.0
fixes the same exact identity 3x2/1x1 RGB bytes as DD, unchanged sources/Info,
distinct 588-byte ICC objects, and profile-memory-independent reuse. Raw/
facade REDs exposed `-3` / `cannot build proof transform`; GREEN passes `1/1`
in `219ms` / `157ms`, ImageCms `220/220` in `4016ms`, and full `2234/2234` in
`12922ms` with registrations `1101/1133`. Release x64 builds with zero
warnings/errors; exports remain `414/414` with zero set difference; SHA-256 is
`96A878B9B180D44BD5F0FEC273AC28C34AE0CE1DCE39665CD71964F9F65F6D51`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DD` opens RGB-input gamut checking
with RGB/sRGB-to-RGB/sRGB at perceptual render intent `0`, proof intent `3`,
and flags `0x5000`. Pillow 11.3.0 fixes exact identity 3x2/1x1 RGB bytes,
unchanged sources/Info, distinct 588-byte ICC objects, and profile-memory-
independent reuse. Raw/facade REDs exposed `-3` / `cannot build proof
transform`; GREEN passes `1/1` in `234ms` / `172ms`, ImageCms `218/218` in
`3546ms`, and full `2232/2232` in `13156ms` with registrations `1100/1132`.
Release x64 builds with zero warnings/errors; exports remain `414/414` with
zero set difference; SHA-256 is
`9BFDDFEF8A8360E82619452DB3532D97B17DB1F8679DB845DE617EA06B117530`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DC` admits absolute-colorimetric
render intent `3` for bounded D50-to-6500K LAB/LAB gamut-check proofing,
completing intents `0..3` for both established LAB-input mode pairs. Pillow
11.3.0 fixes the same exact 3x2 LAB alarm bytes as CV/DA/DB, unchanged 1x1
`[32,254,239]`, unchanged sources/Info, distinct 572-byte ICC objects, and
profile-memory-independent reuse. Raw/facade REDs exposed `-3` / `cannot build
proof transform`; GREEN passes `1/1` in `171ms` / `140ms`, ImageCms `216/216`
in `3203ms`, and full `2230/2230` in `12688ms` with registrations `1099/1131`.
Release x64 builds with zero warnings/errors; exports remain `414/414` with
zero set difference; SHA-256 is
`19BCE546A1986595033EE82F4A883E47FD7C4C2B5F3F8184E5ABCAFEA3A397E9`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DB` admits saturation render intent
`2` for bounded D50-to-6500K LAB/LAB gamut-check proofing. Pillow 11.3.0 fixes
the same exact 3x2 LAB alarm bytes as CV/DA, unchanged 1x1 `[32,254,239]`,
unchanged sources/Info, distinct 572-byte ICC objects, and profile-memory-
independent reuse. Raw/facade REDs exposed `-3` / `cannot build proof
transform`; GREEN passes `1/1` in `156ms` / `109ms`, ImageCms `214/214` in
`2906ms`, and full `2228/2228` in `12141ms` with registrations `1098/1130`.
Release x64 builds with zero warnings/errors; exports remain `414/414` with
zero set difference; SHA-256 is
`5FA6EC3BFE4C6E4DD59BAA65AEF1BF8E43C1E1B152595179E4C18B7554DA7F82`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003DA` admits relative-colorimetric
render intent `1` for bounded D50-to-6500K LAB/LAB gamut-check proofing.
Pillow 11.3.0 fixes exact 3x2 LAB bytes
`[0,0,0,255,0,0,127,255,255,127,255,255,75,68,144,32,254,239]`, unchanged
1x1 `[32,254,239]`, unchanged sources/Info, distinct 572-byte ICC objects,
and profile-memory-independent reuse. Raw/facade REDs exposed `-3` / `cannot
build proof transform`; GREEN passes `1/1` in `187ms` / `109ms`, ImageCms
`212/212` in `2719ms`, and full `2226/2226` in `11937ms` with registrations
`1097/1129`. Release x64 builds with zero warnings/errors; exports remain
`414/414` with zero set difference; SHA-256 is
`DB35967AC2A04168B45685753BABA8146B45F82AE8A0372339AD7E07F9C57E6C`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003CZ` admits absolute-colorimetric
render intent `3` for the bounded LAB/LAB-to-RGB/sRGB gamut-check proof route,
completing intents `0..3` for this pair. Pillow 11.3.0 fixes the same exact 3x2
RGB alarm bytes as CW-CY, 1x1 `[15,34,56]`, unchanged sources/Info, distinct
588-byte ICC objects, and profile-memory-independent reuse. Raw/facade REDs
exposed `-3` / `cannot build proof transform`; GREEN passes `1/1` in `157ms` /
`109ms`, ImageCms `210/210` in `2594ms`, and full `2224/2224` in `11828ms`
with registrations `1096/1128`. Release x64 builds with zero warnings/errors;
exports remain `414/414` with zero set difference; SHA-256 is
`94CA5DF10ADC9826825EF13E44642553E3C6C675146774A095188C4FCEB0AD7B`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003CY` admits saturation render intent
`2` for the bounded LAB/LAB-to-RGB/sRGB gamut-check proof route. Pillow
11.3.0 fixes the same exact 3x2 RGB alarm bytes as CW/CX, 1x1 `[15,34,56]`,
unchanged sources/Info, distinct 588-byte ICC objects, and profile-memory-
independent reuse. Raw/facade REDs exposed `-3` / `cannot build proof
transform`; GREEN passes `1/1` in `140ms` / `109ms`, ImageCms `208/208` in
`2297ms`, and full `2222/2222` in `11891ms` with registrations `1095/1127`.
Release x64 builds with zero warnings/errors; exports remain `414/414`;
SHA-256 is
`391CFC8C6EC50CC4F7DB0331D1A0E3D9E268F9B21A8FA699E8E35D8BD1B63848`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003CX` admits relative-colorimetric
render intent `1` for the bounded LAB/LAB-to-RGB/sRGB gamut-check proof route.
Pillow 11.3.0 fixes the same exact 3x2 RGB alarm bytes as CW, 1x1
`[15,34,56]`, unchanged LAB sources/Info, distinct 588-byte ICC objects, and
profile-memory-independent reuse. Raw/facade REDs exposed `-3` / `cannot build
proof transform`; GREEN passes `1/1` in `141ms` / `93ms`, ImageCms `206/206`
in `2250ms`, and full `2220/2220` in `11390ms` with registrations
`1094/1126`. Release x64 builds with zero warnings/errors; exports remain
`414/414`; SHA-256 is
`41A9469CDFEA84B830B39D8E7443A345EBF2F9BEF6518C2B157F748AA4C7B58E`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003CW` extends bounded gamut-check
reusable soft proofing from LAB/LAB output to RGB/sRGB output at settings
0/3/20480. Pillow 11.3.0 fixes exact 3x2 RGB bytes
`[1,0,1,254,255,254,127,127,127,127,127,127,22,7,252,15,34,56]`, 1x1
`[15,34,56]`, unchanged LAB sources/Info, distinct 588-byte `acsp` ICC
objects, and profile-memory-independent reuse. Raw/facade REDs exposed `-3` /
`cannot build proof transform`; GREEN passes `1/1` in `188ms` / `109ms`,
ImageCms `204/204` in `1891ms`, and full `2218/2218` in `11125ms` with
registrations `1093/1125`. Release x64 builds with zero warnings/errors;
exports remain `414/414`; SHA-256 is
`45E74FB5FBF11D8C05212FA14F904CDE01E730D2A531264F7FEAFE46D426BA8C`.
The AHK-first goal estimate remains `67.20%` (`67%` rounded).

Incremental refresh, 2026-08-05: `META-003CV` opens bounded gamut-check
reusable soft proofing for D50-to-6500K LAB/LAB at settings 0/3/20480. Pillow
11.3.0 fixes exact 3x2 output bytes
`[0,0,0,255,0,0,127,255,255,127,255,255,75,68,144,32,254,239]`, unchanged
1x1 `[32,254,239]`, unchanged sources/Info, distinct 572-byte `acsp` ICC
objects, and profile-memory-independent reuse. Raw/facade REDs exposed `-3` /
`cannot build proof transform`; GREEN passes `1/1` in `203ms` / `157ms`,
ImageCms `202/202` in `1671ms`, and full `2216/2216` in `11328ms` with
registrations `1092/1124`. Release x64 builds with zero warnings/errors;
exports remain `414/414`; SHA-256 is
`CE7DC2A13AEC9F369E902A5E15C7D0099ADDD1155840467DB0639F8FF18444DA`.
The AHK-first goal estimate is `67.20%` (`67%` rounded); other gamut-check
mode pairs/intents, explicit alarm codes, proof in-place apply, and the larger
ecosystem and shipping gaps remain.

Incremental refresh, 2026-08-05: `META-003CU` completes absolute-colorimetric
reusable soft-proofing with D50-to-6500K LAB/LAB at settings 3/3/16384. Pillow
11.3.0 fixes exact identity 3x2/1x1 LAB bytes, unchanged sources/Info, distinct
572-byte `acsp` ICC objects, and profile-memory-independent reuse. Raw/facade
REDs exposed `-3` / `cannot build proof transform`; GREEN passes `1/1` in
`46ms` / `15ms`, ImageCms `200/200` in `1484ms`, and full `2214/2214` in
`11328ms` with registrations `1091/1123`. Reusable proof transforms now cover
render intents `0..3` across all four established RGB/LAB mode pairs. Release
x64 builds with zero warnings/errors; exports remain `414/414`; SHA-256 is
`6586FA42C426B355D16F1C4B9E979699D50F54797AD257C1A873C4498F907DF8`.
The independent production-parity estimate remains `60.15%` (`60%` rounded);
in-place proof apply, other flags, gamut checking, and larger ecosystem gaps
remain.

Incremental refresh, 2026-08-05: `META-003CT` extends absolute-colorimetric
reusable soft-proofing to LAB/LAB-to-RGB/sRGB with render intent `3`, proof
intent `3`, and `SOFTPROOFING=16384`. Pillow 11.3.0 fixes exact 3x2 RGB bytes
`[1,0,1,254,255,254,251,5,4,13,254,14,22,7,252,15,34,56]` and 1x1
`[15,34,56]`, unchanged sources/Info, distinct 588-byte `acsp` ICC objects,
and profile-memory-independent reuse. Raw/facade REDs exposed `-3` / `cannot
build proof transform`; GREEN passes `1/1` in `78ms` / `16ms`, ImageCms
`198/198` in `1406ms`, and full `2212/2212` in `11125ms` with registrations
`1090/1122`. Release x64 builds with zero warnings/errors; exports remain
`414/414`; SHA-256 is
`7887B4EC37AA5E41B10925EB2A2E051308FA7D578568CE9ACDD9EF09F93040D4`.
The independent production-parity estimate remains `60.15%` (`60%` rounded);
absolute LAB/LAB output and the larger ecosystem gaps remain.

Incremental refresh, 2026-08-05: `META-003CS` extends absolute-colorimetric
reusable soft-proofing to RGB/sRGB-to-LAB/LAB with render intent `3`, proof
intent `3`, and `SOFTPROOFING=16384`. Pillow 11.3.0 fixes exact repeat-apply
3x2 LAB bytes `[0,0,0,255,0,0,138,81,70,224,177,81,75,68,144,32,254,239]`
and 1x1 `[32,254,239]`, unchanged sources/Info, distinct 572-byte `acsp` ICC
objects, and profile-memory-independent reuse. Raw/facade REDs exposed `-3` /
`cannot build proof transform`; GREEN passes `1/1` in `78ms` / `15ms`,
ImageCms `196/196` in `1531ms`, and full `2210/2210` in `10938ms` with
registrations `1089/1121`. Release x64 builds with zero warnings/errors;
exports remain `414/414`; SHA-256 is
`CB366AA17FD9DC6385661F0455EFA489767DAD239F8CC78BABA42E5EDBCBB593`.
The independent production-parity estimate remains `60.15%` (`60%` rounded);
absolute LAB-input pairs and the larger ecosystem gaps remain.

Incremental refresh, 2026-08-05: `META-003CR` opens absolute-colorimetric
reusable soft-proofing for RGB/sRGB-to-RGB/sRGB with render intent `3`, proof
intent `3`, and `SOFTPROOFING=16384`. Pillow 11.3.0 fixes exact identity
repeat-apply 3x2/1x1 pixels, unchanged sources/Info, distinct 588-byte `acsp`
ICC objects, and transform lifetime independent of all profile objects and
serialized profile memory. Raw/facade REDs exposed the intended `-3` /
`cannot build proof transform` boundaries; GREEN passes `1/1` in `78ms` /
`47ms`, ImageCms `194/194` in `1359ms`, and full `2208/2208` in `11000ms`
with registrations `1088/1120`. Release x64 builds with zero warnings/errors;
exports remain `414/414`; SHA-256 is
`3322780DFEDB6FCD50BC3C3C15C79D84F3355D77920B07D5412C717826FD6BA7`.
The independent production-parity estimate remains `60.15%` (`60%` rounded);
other absolute mode pairs, proof flags, in-place proof apply, gamut checking,
and the larger ecosystem gaps remain.

Incremental refresh, 2026-08-05: `META-003CN` through `META-003CQ` complete
saturation-intent reusable soft-proofing across all four established RGB/LAB
mode pairs with render intent `2`, proof intent `3`, and
`SOFTPROOFING=16384`. Pillow 11.3.0 fixes exact repeat-apply 3x2/1x1 bytes,
unchanged sources/Info, distinct 572-byte LAB or 588-byte RGB `acsp` ICC
objects, and profile-memory-independent reuse. Native/facade REDs exposed the
intended `-3` / admission boundaries; GREEN culminates in ImageCms `192/192`
(`1391ms`) and full `2206/2206` (`11047ms`) with registrations `1087/1119`.
Release x64 builds with zero warnings/errors; exports remain `414/414`;
SHA-256 is
`20D07E683F8645A4290405AF68EFE11A66E2100D628A6F17F9F1941030DDFB64`.
The independent production-parity estimate remains `60.15%` (`60%` rounded);
absolute proof intent, other proof flags, in-place proof apply, gamut checking,
and the larger ecosystem gaps remain.

Incremental refresh, 2026-08-05: `META-003CJ` through `META-003CM` complete
relative-colorimetric reusable soft-proofing across all four established
RGB/LAB mode pairs with render intent `1`, proof intent `3`, and
`SOFTPROOFING=16384`. Pillow 11.3.0 fixes exact repeat-apply 3x2/1x1 bytes,
unchanged sources/Info, distinct 572-byte LAB or 588-byte RGB `acsp` ICC
objects, and transform lifetime independent of profile and serialized-memory
release. Native/facade REDs exposed the intended `-3` / admission errors;
GREEN culminates in ImageCms `184/184` (`2079ms`) and full `2198/2198`
(`17110ms`) with registrations `1083/1115`. Release x64 builds with zero
warnings/errors; exports remain `414/414`; SHA-256 is
`0978F263221D2F70523AFA3DED47144779E777D11B65FFAAA81E845699E0CB14`.
The independently weighted production-parity estimate remains `60.15%`
(`60%` rounded); saturation/absolute proof intents, other proof flags,
in-place proof apply, gamut checking, and the larger ecosystem gaps remain.

Incremental refresh, 2026-08-05: `META-003CG` through `META-003CI` complete
default reusable soft-proofing across the four established RGB/LAB mode pairs:
RGB-to-RGB, RGB-to-LAB, LAB-to-RGB, and D50-to-6500K LAB-to-LAB, all with an
sRGB proof profile, render intent `0`, proof intent `3`, and
`SOFTPROOFING=16384`. Bounded Pillow 11.3.0 probes fix exact repeat-apply
3x2/1x1 bytes, unchanged sources/Info, distinct 572-byte LAB or 588-byte RGB
`acsp` ICC objects, and profile-memory-independent transforms. The DLL owns
format selection and every row; the facade only admits mode pairs and manages
objects. CG raw/facade GREEN pass `1/1` in `360ms` / `63ms`, ImageCms
`172/172` in `1609ms`, full `2186/2186` in `17843ms`. CH passes `1/1` in
`266ms` / `47ms`, ImageCms `174/174` in `1594ms`, full `2188/2188` in
`16797ms`. CI passes `1/1` / `1/1` in `78ms` each, ImageCms `176/176` in
`1719ms`, full `2190/2190` in `16718ms`; registrations are `1079/1111`.
CI also exposed and fixed raw-test DLL auto-unload across opaque-handle calls;
10 fresh pinned AHK processes and 200 native driver process/loop cases are
stable. Exports remain `414/414`; current SHA-256 is
`A4281086CCFD2529A449356CA17DCB90AE6C6EACE428ECF86E2E656C950067AE`.
The production-parity estimate remains `60%` after rounding; broader intents,
flags, in-place proof apply, gamut checking, information, fonts, interop,
performance, and shipping remain open.

Incremental refresh, 2026-08-04: `META-003CF` opens the first reusable RGB
soft-proof transform route. A bounded Pillow 11.3.0 probe fixes default render
intent `0`, proof intent `3`, `SOFTPROOFING=16384`, exact repeat-apply 3x2/1x1
RGB bytes, unchanged sources/Info, distinct 588-byte `acsp` result ICC objects,
and transform lifetime independent of all three profiles and serialized
profile memory. The DLL now exports a real LittleCMS proof-transform builder
and reuses existing native apply/output-profile/free paths; the facade adds one
coarse constructor route. Raw RED reported a nonexistent export; facade RED
reported a missing method. Raw/facade GREEN pass `1/1` in `265ms` / `109ms`;
ImageCms `170/170` in `1610ms`; full `2184/2184` in `19266ms`; registrations
`1076/1108`; exports `414/414`; SHA-256
`994C4E68A62F2D4D1585ED8A2D0BC616A105848329839032F2FE4F2C232D0822`.
The production-parity estimate remains `60%`: in-place/broader proofing,
information, codecs, fonts, interop, performance, and shipping remain open.

Incremental refresh, 2026-08-04: `META-003CE` completes absolute-colorimetric
in-place BPC across both established same-mode pairs by adding D50-to-6500K
LAB/LAB at intent `3`. A bounded Pillow 11.3.0 probe fixes `None` return, exact
identity 3x2 LAB bytes, retained caller sentinel, one 572-byte `acsp` output
ICC, and profile-independent image lifetime. The DLL owns temporary transform
construction and every same-storage LAB row; the facade adds only exact
admission, Info merge, and `""` return. Raw RED returned `-3`; facade RED raised
`cannot build transform`. Raw/facade GREEN pass `1/1` in `141ms` / `31ms`;
ImageCms `168/168` in `984ms`; full `2182/2182` in `16875ms`; registrations
`1075/1107`; exports `413/413`; SHA-256
`A6FC4F9CDD52D3683A8B84CAE1C0BD7B00CAE900E1AEAB40F81758A9DC4CA428`.
The production-parity estimate remains `60%`: proofing/information, codecs,
fonts, interop, performance, and shipping remain open.

Incremental refresh, 2026-08-04: `META-003CD` opens absolute-colorimetric in-
place BPC with RGB/sRGB-to-RGB/memory-opened-sRGB at intent `3`. A bounded
Pillow 11.3.0 probe fixes `None` return, exact identity 3x2 RGB bytes, retained
caller sentinel, one 588-byte `acsp` output ICC, and profile-memory/profile-
independent image lifetime. The DLL owns temporary transform construction and
every same-storage RGB row; the facade adds only exact admission, Info merge,
and `""` return. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` in `172ms` / `47ms`; ImageCms
`166/166` in `1625ms`; full `2180/2180` in `10672ms`; registrations
`1074/1106`; exports `413/413`; SHA-256
`1942EE638692A297ECD0B7DDDC0EFFEEF51E5421AEC81BFB46E49F78833AA75D`.
The production-parity estimate remains `60%`: LAB absolute in-place BPC,
proofing/information, codecs, fonts, interop, performance, and shipping remain
open.

Incremental refresh, 2026-08-04: `META-003CC` completes saturation in-place
BPC across both established same-mode pairs by adding D50-to-6500K LAB/LAB at
intent `2`. A bounded Pillow 11.3.0 probe fixes `None` return, exact identity
3x2 LAB bytes, retained caller sentinel, one 572-byte `acsp` output ICC, and
profile-independent image lifetime. The DLL owns temporary transform
construction and every same-storage LAB row; the facade adds only exact
admission, Info merge, and `""` return. Raw RED returned `-3`; facade RED raised
`cannot build transform`. Raw/facade GREEN pass `1/1` in `141ms` / `31ms`;
ImageCms `164/164` in `1406ms`; full `2178/2178` in `17859ms`; registrations
`1073/1105`; exports `413/413`; SHA-256
`DA44080F6FA2F1B407B251F2474D411222F7B170A58C5D4C8B7D95C8305E6402`.
The production-parity estimate remains `60%`: absolute in-place BPC,
proofing/information, codecs, fonts, interop, performance, and shipping remain
open.

Incremental refresh, 2026-08-04: `META-003CB` opens saturation in-place BPC
with RGB/sRGB-to-RGB/memory-opened-sRGB at intent `2`. A bounded Pillow 11.3.0
probe fixes `None` return, exact identity 3x2 RGB bytes, retained caller
sentinel, one 588-byte `acsp` output ICC, and profile-memory/profile-independent
image lifetime. The DLL owns temporary transform construction and every same-
storage RGB row; the facade adds only exact admission, Info merge, and `""`
return. Raw RED returned `-3`; facade RED raised `cannot build transform`.
Raw/facade GREEN pass `1/1` in `141ms` / `16ms`; ImageCms `162/162` in
`1328ms`; full `2176/2176` in `17156ms`; registrations `1072/1104`; exports
`413/413`; SHA-256
`024A970B5AB947E6B0143ECD1FA6ADC270D864B0ED7E711F69F6348E020BE359`.
The production-parity estimate remains `60%`: LAB saturation and absolute in-
place BPC, proofing/information, codecs, fonts, interop, performance, and
shipping remain open.

Incremental refresh, 2026-08-04: `META-003CA` completes relative-colorimetric
in-place BPC across both established same-mode pairs by adding D50-to-6500K
LAB/LAB at intent `1`. A bounded Pillow 11.3.0 probe fixes `None` return,
exact identity 3x2 LAB bytes, retained caller sentinel, one 572-byte `acsp`
output ICC, and profile-independent image lifetime. The DLL owns temporary
transform construction and every same-storage LAB row; the facade adds only
exact admission, Info merge, and `""` return. Raw RED returned `-3`; facade RED
raised `cannot build transform`. Raw/facade GREEN pass `1/1` in `157ms` /
`31ms`; ImageCms `160/160` in `1421ms`; full `2174/2174` in `23609ms`;
registrations `1071/1103`; exports `413/413`; SHA-256
`4D419E74375E79C5BF7BCA98BFCC2F4280B088037CD48A10FF77AEB1023B9216`.
The production-parity estimate remains `60%`: in-place saturation/absolute
BPC, proofing/information, codecs, fonts, interop, performance, and shipping
remain open.

Incremental refresh, 2026-08-04: `META-003BZ` opens relative-colorimetric in-
place BPC with RGB/sRGB-to-RGB/memory-opened-sRGB at intent `1`. A bounded
Pillow 11.3.0 probe fixes `None` return, exact identity 3x2 RGB bytes, retained
caller sentinel, one 588-byte `acsp` output ICC, and profile-memory/profile-
independent image lifetime. The DLL owns temporary transform construction and
every same-storage RGB row; the facade adds only exact admission, Info merge,
and `""` return. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` in `218ms` / `31ms`; ImageCms
`158/158` in `1531ms`; full `2172/2172` in `18750ms`; registrations
`1070/1102`; exports `413/413`; SHA-256
`A3B84337DADD346ED2FB3EA60186F831D2D86F190304F52CA79AA713DB5E8E0F`.
The production-parity estimate remains `60%`: LAB relative in-place BPC,
other in-place BPC intents, proofing/information, codecs, fonts, interop,
performance, and shipping remain open.

Incremental refresh, 2026-08-04: `META-003BY` completes perceptual in-place
BPC across both established same-mode pairs by adding D50-to-6500K LAB/LAB.
A bounded Pillow 11.3.0 probe fixes `None` return, exact identity 3x2 LAB
bytes, retained caller sentinel, one 572-byte `acsp` output ICC, and profile-
independent image lifetime. The DLL owns temporary transform construction and
every same-storage LAB row; the facade adds only exact admission, Info merge,
and `""` return. Raw RED returned `-3`; facade RED raised `cannot build
transform`. Raw/facade GREEN pass `1/1` in `188ms` / `15ms`; ImageCms
`156/156` in `1328ms`; full `2170/2170` in `19250ms`; registrations
`1069/1101`; exports `413/413`; SHA-256
`1B89C158A8347B2365CBEA2995D15D6AFC39EDB54F8D610034C5811D28815728`.
The production-parity estimate remains `60%`: other in-place BPC intents,
proofing/information, codecs, fonts, interop, performance, and shipping remain
open.

Incremental refresh, 2026-08-04: `META-003BX` opens in-place one-shot BPC with
RGB/sRGB-to-RGB/memory-opened-sRGB at perceptual intent `0`. A bounded Pillow
11.3.0 probe fixes `None` return, exact identity 3x2 RGB bytes, retained caller
sentinel, one 588-byte `acsp` output ICC, and profile-memory/profile-independent
image lifetime. The DLL owns temporary transform construction and every in-
place row; the facade adds only exact admission, Info merge, and `""` return.
Raw RED returned `-3`; facade RED raised `cannot build transform`. Raw/facade
GREEN pass `1/1` in `219ms` / `15ms`; ImageCms `154/154` in `891ms`; full
`2168/2168` in `12750ms`; registrations `1068/1100`; exports `413/413`;
SHA-256 `039166C0E006A238013FB551F1A9E226F6EF7B8C403FBF2395AEC31AA434C595`.
The production-parity estimate remains `60%`: LAB and other-intent in-place
BPC, proofing/information, codecs, fonts, interop, performance, and shipping
remain open.

Incremental refresh, 2026-08-04: `META-003BW` completes allocating one-shot
absolute-colorimetric/BPC admission across all four established pairs by adding
D50-to-6500K LAB/LAB with exact identity 3x2 LAB pixels, unchanged source and
caller Info, profile-independent result lifetime, one 572-byte ICC Info, and no
AHK pixel loop. ImageCms passes `152/152` in `781ms`; full `2166/2166` in
`11265ms`; registrations `1067/1099`; exports `413/413`; SHA-256
`E3E9C988F5A82E80339E7716C502DC97B932CFFEEB15337B0D2DE59C09FD7722`.
The workflow-substitution estimate remains `64%`: in-place BPC, proofing/
information, codecs, fonts, interop, performance, and shipping remain open.

Incremental refresh, 2026-08-04: `META-003BV` adds allocating one-shot RGB/
sRGB-to-RGB/memory-opened-sRGB absolute-colorimetric/BPC admission with exact
identity 3x2 RGB pixels, unchanged source and caller Info, profile-memory/
profile-independent result lifetime, one 588-byte ICC Info, and no AHK pixel
loop. ImageCms passes `150/150` in `1343ms`; full `2164/2164` in `16594ms`;
registrations `1066/1098`; exports `413/413`; SHA-256
`6F254FC54336DF10867E6E18BD6EB363A73A0D905272C088547070B0C9AAC034`.
The workflow-substitution estimate remains `64%`: LAB-to-LAB absolute one-shot
BPC, proofing/information, codecs, fonts, interop, performance, and shipping
remain open.

Incremental refresh, 2026-08-04: `META-003BU` extends allocating one-shot
absolute-colorimetric/BPC admission to LAB/LAB-to-RGB/sRGB, completing both
mode-changing pairs with exact 3x2 RGB pixels, unchanged source and caller
Info, profile-independent result lifetime, one 588-byte ICC Info, and no AHK
pixel loop. ImageCms passes `148/148` in `703ms`; full `2162/2162` in
`9938ms`; registrations `1065/1097`; exports `413/413`; SHA-256
`B78A0BAF25C4E555EC6148CBCEC816EB684910F4AB1CAB923B9F73503B0EBDD4`.
The workflow-substitution estimate remains `64%`: same-mode absolute one-shot
BPC, proofing/information, codecs, fonts, interop, performance, and shipping
remain open.

Incremental refresh, 2026-08-04: `META-003BT` opens allocating one-shot
absolute-colorimetric/BPC admission with RGB/sRGB-to-LAB/LAB, exact 3x2 LAB
pixels, unchanged source and caller Info, profile-independent result lifetime,
one 572-byte ICC Info, and no AHK pixel loop. ImageCms passes `146/146` in
`703ms`; full `2160/2160` in `10156ms`; registrations `1064/1096`; exports
`413/413`; SHA-256
`13713869393CEE107FD9597B9BFCB55159CA17461C6A0CD62E64976523278A6A`.
The workflow-substitution estimate remains `64%`: other absolute one-shot BPC,
proofing/information, codecs, fonts, interop, performance, and shipping remain
open.

Incremental refresh, 2026-08-04: `META-003BS` completes allocating one-shot
saturation/BPC admission across all four established pairs by adding D50-to-
6500K LAB/LAB, exact identity 3x2 LAB pixels, unchanged source and caller Info,
profile-independent result lifetime, one 572-byte ICC Info, and no AHK pixel
loop. ImageCms passes `144/144` in `1266ms`; full `2158/2158` in `17313ms`;
registrations `1063/1095`; exports `413/413`; SHA-256
`FF747CF87374210D6F5AA110C1BC51F519E601C43BD836A8B393A32D350CF6C1`.
The workflow-substitution estimate remains `64%`: absolute one-shot BPC,
proofing/information, codecs, fonts, interop, performance, and shipping remain
open.

Incremental refresh, 2026-08-04: `META-003BR` extends allocating one-shot
saturation/BPC admission to RGB/sRGB-to-RGB/memory-opened-sRGB, exact identity
3x2 RGB pixels, unchanged source and caller Info, profile-memory/profile-
independent result lifetime, one 588-byte ICC Info, and no AHK pixel loop.
ImageCms passes `142/142` in `1328ms`; full `2156/2156` in `17328ms`;
registrations `1062/1094`; exports `413/413`; SHA-256
`D4C124C26CD858E1C9E9E77A26C39E096EF8E9F5025833DC36D8AD9891E5EE43`.
The workflow-substitution estimate remains `64%`: LAB-to-LAB saturation/
absolute one-shot BPC, proofing/information, codecs, fonts, interop,
performance, and shipping remain open.

Incremental refresh, 2026-08-04: `META-003BQ` extends allocating one-shot
saturation/BPC admission to both mode-changing pairs by adding LAB/LAB-to-RGB/
sRGB, exact 3x2 RGB pixels, unchanged source and caller Info, profile-
independent result lifetime, one 588-byte ICC Info, and no AHK pixel loop.
ImageCms passes `140/140` in `1156ms`; full `2154/2154` in `17187ms`;
registrations `1061/1093`; exports `413/413`; SHA-256
`0FF2388CF03A0AC036F7990FA428AAF1063E69DA0AA31282EDCFADD23344635F`.
The workflow-substitution estimate remains `64%`: same-mode saturation/
absolute one-shot BPC, proofing/information, codecs, fonts, interop,
performance, and shipping remain open.

Incremental refresh, 2026-08-04: `META-003BP` opens allocating one-shot
saturation/BPC admission with RGB/sRGB-to-LAB/LAB, exact 3x2 LAB pixels,
unchanged source and caller Info, profile-independent result lifetime, one
572-byte ICC Info, and no AHK pixel loop. ImageCms passes `138/138` in
`1156ms`; full `2152/2152` in `16687ms`; registrations `1060/1092`; exports
`413/413`; SHA-256
`955894B342D929BBE706D1494344D781061EFF8DDB942E5763AE491DD5E7F221`.
The capability-point estimate remains `62%`: other saturation one-shot pairs,
absolute one-shot BPC, proofing/information, codecs, fonts, interop,
performance, and shipping remain open.

Incremental refresh, 2026-08-04: `META-003BO` completes allocating one-shot
relative/BPC admission across all four established pairs by adding D50-to-
6500K LAB-to-LAB with exact identity 3x2 LAB pixels, unchanged source and
caller Info, profile-independent result lifetime, one 572-byte ICC Info, and no
AHK pixel loop. ImageCms passes `136/136` in `1125ms`; full `2150/2150` in
`16328ms`; registrations `1059/1091`; exports `413/413`; SHA-256
`BC78D6575B41E9EE0D647E83422152D7946E00B0D0D7D7C7AB7B11C62E132281`.
The capability-point estimate remains `62%`: saturation/absolute one-shot BPC,
proofing/information, codecs, fonts, interop, performance, and shipping remain
open.

Incremental refresh, 2026-08-04: `META-003BN` adds allocating one-shot RGB-to-
RGB relative/BPC with exact identity pixels, unchanged source and caller Info,
profile-memory/profile-independent result lifetime, one 588-byte ICC Info, and
no AHK pixel loop. ImageCms passes `134/134` in `1078ms`; full `2148/2148` in
`17093ms`; registrations `1058/1090`; exports `413/413`; SHA-256
`787BB7513BAEE4F3E5139BBE07C73CB8F204F014686E77A656A9FC55353B8647`.
The capability-point estimate remains `62%`: LAB-to-LAB relative one-shot,
proofing/information, codecs, fonts, interop, performance, and shipping remain
open.

Incremental refresh, 2026-08-04: `META-003BM` completes both allocating one-
shot mode-changing relative/BPC pairs by adding LAB/LAB-to-RGB/sRGB with exact
3x2 RGB pixels, unchanged source and caller Info, profile-independent result
lifetime, one 588-byte ICC Info, and no AHK pixel loop. ImageCms passes
`132/132` in `1094ms`; full `2146/2146` in `16328ms`; registrations
`1057/1089`; exports `413/413`; SHA-256
`B1C4292B6D129155E899719347248B0FA0656A5548E1139502FC6D9AE12984BD`.
The capability-point estimate remains `62%`: same-mode relative one-shot
pairs, proofing/information, codecs, fonts, interop, performance, and shipping
remain open.

Incremental refresh, 2026-08-04: `META-003BL` opens allocating one-shot
relative/BPC admission with RGB/sRGB-to-LAB/LAB, exact 3x2 LAB pixels,
unchanged source and caller Info, profile-independent result lifetime, one
572-byte ICC Info, and no AHK pixel loop. ImageCms passes `130/130` in
`1047ms`; full `2144/2144` in `16000ms`; registrations `1056/1088`; exports
`413/413`; SHA-256
`57A1024D9C8C8D5EFBF0F2A21651D35AD387F279E01E6162F76BB425AF799DD0`.
The capability-point estimate remains `62%`: other relative one-shot pairs,
proofing/information, codecs, fonts, interop, performance, and shipping remain
open.

Incremental refresh, 2026-08-04: `META-003BK` completes allocating one-shot
perceptual/BPC admission across all four established pairs by adding D50-to-
6500K LAB-to-LAB with exact identity 3x2 LAB pixels, unchanged source and
caller Info, profile-independent result lifetime, one 572-byte ICC Info, and no
AHK pixel loop. ImageCms passes `128/128` in `1016ms`; full `2142/2142` in
`17297ms`; registrations `1055/1087`; exports `413/413`; SHA-256
`042C0DD7D51EAE67082178EF48EB663F8DA729BE4AA2E7BF752E39378A7C8BA1`.
The capability-point estimate remains `62%`: other-intent one-shot flags,
proofing/information, codecs, fonts, interop, performance, and shipping remain
open.

Incremental refresh, 2026-08-04: `META-003BJ` adds allocating one-shot RGB-to-
RGB perceptual/BPC admission with a memory-opened output profile, exact identity
3x2 RGB pixels, unchanged source and caller Info, profile-memory/profile-
independent result lifetime, one 588-byte ICC Info, and no AHK pixel loop.
ImageCms passes `126/126` in `1047ms`; full `2140/2140` in `16328ms`;
registrations `1054/1086`; exports `413/413`; SHA-256
`88B83F8AE354FB6F1771DB84ECAADFECECE584925528DD074F47328DACC0CC0F`.
The capability-point estimate remains `62%`: LAB-to-LAB/other-intent one-shot
flags, proofing/information, codecs, fonts, interop, performance, and shipping
remain open.

Incremental refresh, 2026-08-04: `META-003BI` completes both established
mode-changing allocating one-shot perceptual/BPC routes by adding LAB-to-RGB
with exact 3x2 RGB pixels, unchanged source and caller Info, profile-independent
result lifetime, one 588-byte ICC Info, and no AHK pixel loop. ImageCms passes
`124/124` in `984ms`; full `2138/2138` in `17266ms`; registrations
`1053/1085`; exports `413/413`; SHA-256
`133A9875DC0706C1E3D3A3EE69E9C761BD6DBC01AF2FD89C60AB6C8F5599746B`.
The capability-point estimate remains `62%`: same-mode/other-intent one-shot
flags, proofing/information, codecs, fonts, interop, performance, and shipping
remain open.

Incremental refresh, 2026-08-04: `META-003BH` opens allocating one-shot RGB-
to-LAB perceptual/BPC admission with exact 3x2 LAB pixels, unchanged source and
caller Info, profile-independent result lifetime, one 572-byte ICC Info, and no
AHK pixel loop. ImageCms passes `122/122` in `985ms`; full `2136/2136` in
`16313ms`; registrations `1052/1084`; exports `413/413`; SHA-256
`D3055F3760C03796E8215BDF1AE8791CEE61746C61B2EF38EE1D5D348A602EA7`.
The effort-weighted estimate remains `53%`: other one-shot/in-place flags,
proofing/information, codec, font, interop, performance, and shipping workstreams
remain open.

Incremental refresh, 2026-08-04: `META-003BG` completes reusable absolute-
colorimetric/BPC admission across all four established pairs by adding D50-to-
6500K LAB-to-LAB with exact repeat 3x2/1x1 identity LAB pixels, unchanged
sources and caller Info, profile-independent lifetime, separate distinct 572-
byte ICC Info on each result, and no AHK pixel loop. ImageCms passes `120/120`
in `656ms`; full `2134/2134` in `11000ms`; registrations `1051/1083`;
exports `413/413`; SHA-256
`A80DA2A1C8CB266858A18B1799F9EE61F70B321E849995B0F5D15F737EEB7147`.
The effort-weighted estimate remains `53%`: one-shot/in-place flags, proofing/
information, codec, font, interop, performance, and shipping workstreams remain
open.

Incremental refresh, 2026-08-04: `META-003BF` adds reusable RGB-to-RGB
absolute-colorimetric/BPC admission with a memory-opened output profile, exact
repeat 3x2/1x1 identity RGB pixels, unchanged sources and caller Info, profile-
memory/profile-independent lifetime, separate distinct 588-byte ICC Info on
each result, and no AHK pixel loop. ImageCms passes `118/118` in `640ms`; full
`2132/2132` in `10969ms`; registrations `1050/1082`; exports `413/413`;
SHA-256 `F9B52490F8557074EA72068D2351E8F271AEB1F1184DCF742B282502ADCA968B`.
The effort-weighted estimate remains `53%`: LAB/LAB absolute BPC, one-shot
flags, proofing/information, codec, font, interop, performance, and shipping
workstreams remain open.

Incremental refresh, 2026-08-04: `META-003BE` completes reusable absolute-
colorimetric/BPC admission across both mode-changing pairs by adding LAB-to-
RGB with exact repeat 3x2/1x1 reverse RGB pixels, unchanged sources and caller
Info, profile-independent lifetime, separate distinct 588-byte ICC Info on
each result, and no AHK pixel loop. ImageCms passes `116/116` in `593ms`; full
`2130/2130` in `10688ms`; registrations `1049/1081`; exports `413/413`;
SHA-256 `E118088CC55492410774BCDA44FCA0A8360B52914F6987E24258743B99EA6624`.
The effort-weighted estimate remains `53%`: same-mode absolute reusable pairs,
one-shot flags, proofing/information, codec, font, interop, performance, and
shipping workstreams remain open.

Incremental refresh, 2026-08-04: `META-003BD` opens reusable RGB-to-LAB
absolute-colorimetric/BPC admission with exact repeat 3x2/1x1 LAB pixels,
unchanged sources and caller Info, profile-independent lifetime, separate
distinct 572-byte ICC Info on each result, and no AHK pixel loop. ImageCms
passes `114/114` in `532ms`; full `2128/2128` in `15047ms`; registrations
`1048/1080`; exports `413/413`; SHA-256
`43E9F4E3A46131DEB57540F7C692CCF58A55296AA40ECAA7FB16BBBBCC977603`.
The effort-weighted estimate remains `53%`: the other absolute reusable pairs,
one-shot flags, proofing/information, codec, font, interop, performance, and
shipping workstreams remain open.

Incremental refresh, 2026-08-04: `META-003BC` completes reusable saturation/
BPC admission across all four established pairs by adding D50-to-6500K LAB-to-
LAB with exact repeat 3x2/1x1 identity LAB pixels, unchanged sources and caller
Info, profile-independent lifetime, separate distinct 572-byte ICC Info on
each result, and no AHK pixel loop. ImageCms passes `112/112` in `578ms`; full
`2126/2126` in `10735ms`; registrations `1047/1079`; exports `413/413`;
SHA-256 `5BC61E57225926A3528E28FD9B685723AE01A4D9525831B7FFEE9A2EB76B2D9C`.
The effort-weighted estimate remains `53%`: absolute reusable/one-shot flags,
proofing/information, codec, font, interop, performance, and shipping
workstreams remain open.

Incremental refresh, 2026-08-04: `META-003BB` adds reusable RGB-to-RGB
saturation/BPC admission with a memory-opened output profile, exact repeat
3x2/1x1 identity RGB pixels, unchanged sources and caller Info, profile-memory/
profile-independent lifetime, separate distinct 588-byte ICC Info on each
result, and no AHK pixel loop. ImageCms passes `110/110` in `563ms`; full
`2124/2124` in `11391ms`; registrations `1046/1078`; exports `413/413`;
SHA-256 `A40D84E96D877509CFE00F5954EC4294232D154E63700A061812E1264CA6DCD4`.
The effort-weighted estimate remains `53%`: LAB/LAB saturation, absolute
reusable/one-shot flags, proofing/information, codec, font, interop,
performance, and shipping workstreams remain open.

Incremental refresh, 2026-08-04: `META-003BA` completes reusable saturation/
BPC admission across both mode-changing pairs by adding LAB-to-RGB with exact
repeat 3x2/1x1 reverse RGB pixels, unchanged sources and caller Info, profile-
independent lifetime, separate distinct 588-byte ICC Info on each result, and
no AHK pixel loop. ImageCms passes `108/108` in `609ms`; full `2122/2122` in
`11125ms`; registrations `1045/1077`; exports `413/413`; SHA-256
`EADC87096D89902FD73DAE04F97596A3FBE97F89863628BAC930F92D5A2C313B`.
The effort-weighted estimate remains `53%`: same-mode saturation and absolute
reusable/one-shot flags, proofing/information, codec, font, interop,
performance, and shipping workstreams remain open.

Incremental refresh, 2026-08-04: `META-003AZ` opens reusable RGB-to-LAB
saturation/BPC admission with exact repeat 3x2/1x1 LAB pixels, unchanged
sources and caller Info, profile-independent lifetime, separate distinct 572-
byte ICC Info on each result, and no AHK pixel loop. ImageCms passes `106/106`
in `593ms`; full `2120/2120` in `11609ms`; registrations `1044/1076`; exports
`413/413`; SHA-256
`A180FD81089BFCABF9F375F8325A25A624A79F80986159BB4BB5DDC4DDC9505F`.
The effort-weighted estimate remains `53%`: remaining saturation/absolute
reusable and one-shot flags, proofing/information, codec, font, interop,
performance, and shipping workstreams remain open.

Incremental refresh, 2026-08-04: `META-003AY` completes reusable relative-
colorimetric/BPC admission across all four established pairs by adding D50-to-
6500K LAB-to-LAB with exact repeat 3x2/1x1 identity LAB pixels, unchanged
sources and caller Info, profile-independent lifetime, separate distinct 572-
byte ICC Info on each result, and no AHK pixel loop. ImageCms passes `104/104`
in `531ms`; full `2118/2118` in `11719ms`; registrations `1043/1075`; exports
`413/413`; SHA-256
`855712FE0FF147089DA4F57DDC1F89814C2E2D17B2266D4AE82069939511FEF5`.
The effort-weighted estimate remains `53%`: saturation/absolute reusable and
one-shot flags, proofing/information, codec, font, interop, performance, and
shipping workstreams remain open.

Incremental refresh, 2026-08-04: `META-003AX` adds reusable RGB-to-RGB
relative-colorimetric/BPC admission with a memory-opened output profile, exact
repeat 3x2/1x1 identity RGB pixels, unchanged sources and caller Info,
profile-memory/profile-independent lifetime, separate distinct 588-byte ICC
Info on each result, and no AHK pixel loop. ImageCms passes `102/102` in
`610ms`; full `2116/2116` in `11875ms`; registrations `1042/1074`; exports
`413/413`; SHA-256
`131BB72367D5296BF4A75A2D444D1B0255A424D666F60B919D6D103728605D3A`.
The effort-weighted estimate remains `53%`: LAB/LAB relative BPC, remaining-
intent reusable and one-shot flags, proofing/information, codec, font,
interop, performance, and shipping workstreams remain open.

Incremental refresh, 2026-08-04: `META-003AW` completes reusable relative-
colorimetric/BPC admission across both mode-changing pairs by adding LAB-to-
RGB with exact repeat 3x2/1x1 reverse RGB pixels, unchanged sources and caller
Info, profile-independent lifetime, separate distinct 588-byte ICC Info on each
result, and no AHK pixel loop. ImageCms passes `100/100` in `563ms`; full
`2114/2114` in `13141ms`; registrations `1041/1073`; exports `413/413`;
SHA-256 `7803C98276EE73B524B55BBEA8F040C17A8FB08F02D1D2EDF3D4CDE72C0BE225`.
The effort-weighted estimate remains `53%`: same-mode/remaining-intent reusable
and one-shot flags, proofing/information, codec, font, interop, performance,
and shipping workstreams remain open.

Incremental refresh, 2026-08-04: `META-003AV` opens reusable RGB-to-LAB
relative-colorimetric/BPC admission with exact repeat 3x2/1x1 LAB pixels,
unchanged sources and caller Info, profile-independent lifetime, separate
distinct 572-byte ICC Info on each result, and no AHK pixel loop. ImageCms
passes `98/98` in `516ms`; full `2112/2112` in `13265ms`; registrations
`1040/1072`; exports `413/413`; SHA-256
`0524291D2A58709EF2A0463D98EAB4E0D2FDE06A15F59768FF83B1C97401A76E`.
The effort-weighted estimate remains `53%`: this bounded relative flag route
does not close remaining intents/pairs, one-shot flags, proofing/information,
codec, font, interop, performance, or shipping workstreams.

Incremental refresh, 2026-08-04: `META-003AU` completes reusable perceptual/
BPC admission across all four established pairs by adding D50-to-6500K
LAB-to-LAB with exact repeat 3x2/1x1 identity LAB pixels, unchanged sources and
caller Info, profile-independent lifetime, separate distinct 572-byte ICC Info
on each result, and no AHK pixel loop. ImageCms passes `96/96` in `547ms`;
full `2110/2110` in `12922ms`; registrations `1039/1071`; exports `413/413`;
SHA-256 `BA4E8CAC692E7AFF483551D6BBE2D1E1906DD661CF5FA45580BCA4E53A78F852`.
The effort-weighted estimate remains `53%`: this reusable perceptual flag
matrix does not close other intents, one-shot flags, proofing/information,
codec, font, interop, performance, or shipping workstreams.

Incremental refresh, 2026-08-04: `META-003AT` adds reusable RGB-to-RGB
perceptual/BPC admission with a memory-opened output profile, exact repeat
3x2/1x1 identity RGB pixels, unchanged sources and caller Info, profile-memory-
independent lifetime, separate distinct 588-byte ICC Info on each result, and
no AHK pixel loop. ImageCms passes `94/94` in `515ms`; full `2108/2108` in
`12844ms`; registrations `1038/1070`; exports `413/413`; SHA-256
`26FB915D689B5A5C9BFFFD786408BA7CD4FC609843FCB0EB7E874E74451164F5`.
The effort-weighted estimate remains `53%`: this bounded same-mode flag route
does not close LAB/LAB or other-intent CMS flags, proofing/information, codec,
font, interop, performance, or shipping workstreams.

Incremental refresh, 2026-08-04: `META-003AS` completes perceptual/BPC
admission across both reusable mode-changing pairs by adding LAB-to-RGB with
exact repeat 3x2/1x1 RGB pixels, unchanged LAB sources and caller Info,
profile-independent lifetime, separate distinct 588-byte ICC Info on each
result, and no AHK pixel loop. ImageCms passes `92/92` in `578ms`; full
`2106/2106` in `14016ms`; registrations `1037/1069`; exports `413/413`;
SHA-256 `C301A54FB0CF19DAA9850C4DB3F0AE7B370F3EA28BBBDDDDAC89AE2956096120`.
The effort-weighted estimate remains `53%`: this second bounded flag route does
not close same-mode/other-intent CMS flags, proofing/information, codec, font,
interop, performance, or shipping workstreams.

Incremental refresh, 2026-08-04: `META-003AR` opens reusable RGB-to-LAB
perceptual intent `0` with Pillow's bounded black-point-compensation flag
`8192`, exact repeat 3x2/1x1 LAB pixels, unchanged RGB sources and caller Info,
profile-independent lifetime, separate distinct 572-byte ICC Info on each
result, and no AHK pixel loop. ImageCms passes `90/90` in `453ms`; full
`2104/2104` in `12953ms`; registrations `1036/1068`; exports `413/413`;
SHA-256 `5022FA192187DC881048D4D6982203D07084F91315F9C2EC782AA2A86FC5A5ED`.
The effort-weighted estimate remains `53%`: this one bounded flag route does
not close the wider CMS flags/proofing/information, codec, font, interop,
performance, or shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AQ` completes reusable intents
`0..3` across all four established pairs with LAB-to-LAB absolute-colorimetric
intent `3`, exact repeat 3x2/1x1 identity pixels, unchanged LAB sources,
profile-independent lifetime, separate distinct 572-byte ICC Info on each
result, and no AHK pixel loop. ImageCms passes `88/88` in `860ms`; full
`2102/2102` in `20500ms`; registrations `1035/1067`; exports `413/413`;
SHA-256 `5958951C963A441633262D5A69E7C0845B277983E6AB2B9B2E5091763FB1C468`.
The effort-weighted estimate remains `53%`: completed intent matrices do not
close the wider CMS flags/proofing/information, codec, font, interop,
performance, or shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AP` adds reusable LAB-to-LAB
saturation intent `2` with exact repeat 3x2/1x1 identity pixels, unchanged LAB
sources, profile-independent lifetime, separate distinct 572-byte ICC Info on
each result, and no AHK pixel loop. ImageCms passes `86/86` in `375ms`; full
`2100/2100` in `9954ms`; registrations `1034/1066`; exports `413/413`;
SHA-256 `A212108E787C112C4B8A4A79CD07866E86888BB586D9C2D09E20C86DD99DA417`.
The effort-weighted estimate remains `53%`: this bounded reusable same-mode
intent branch does not close the wider CMS, codec, font, interop, performance,
or shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AO` completes reusable RGB-to-RGB
intents `0..3` with absolute-colorimetric intent `3`, exact repeat 3x2/1x1
identity pixels, unchanged RGB sources, profile-memory-independent lifetime,
separate 588-byte ICC Info on each result, and no AHK pixel loop. ImageCms
passes `84/84` in `375ms`; full `2098/2098` in `10016ms`; registrations
`1033/1065`; exports `413/413`; SHA-256
`2555122CE65CC6EFB330FB5886E1C1D08646B42F4AD1F03800014D39677A1674`.
The effort-weighted estimate remains `53%`: one completed same-mode reusable
intent matrix does not close the wider CMS, codec, font, interop, performance,
or shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AN` adds reusable RGB-to-RGB
saturation intent `2` with exact repeat 3x2/1x1 identity pixels, unchanged RGB
sources, profile-memory-independent lifetime, separate 588-byte ICC Info on
each result, and no AHK pixel loop. ImageCms passes `82/82` in `687ms`; full
`2096/2096` in `17781ms`; registrations `1032/1064`; exports `413/413`;
SHA-256 `E757CFF604A83E7F9DB5E62748DA0866A015E4030D1DFFD52ECC0396049FB5C6`.
The effort-weighted estimate remains `53%`: this bounded reusable same-mode
intent branch does not close the wider CMS, codec, font, interop, performance,
or shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AM` completes reusable mode-changing
intents `0..3` with LAB-to-RGB absolute-colorimetric intent `3`, exact repeat
3x2/1x1 reverse pixels, unchanged LAB sources, profile-independent lifetime,
separate 588-byte ICC Info on each result, and no AHK pixel loop. ImageCms
passes `80/80` in `875ms`; full `2094/2094` in `20125ms`; registrations
`1031/1063`; exports `413/413`; SHA-256
`68DD191A38DDC994D54C92063F3F6BD3EEA26EB9227BD32CDE069FB936766A3F`.
The effort-weighted estimate remains `53%`: completed mode-changing reusable
intent matrices do not close the wider CMS, codec, font, interop, performance,
or shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AL` adds reusable LAB-to-RGB
saturation intent `2` with exact repeat 3x2/1x1 reverse pixels, unchanged LAB
sources, profile-independent lifetime, separate 588-byte ICC Info on each
result, and no AHK pixel loop. ImageCms passes `78/78` in `703ms`; full
`2092/2092` in `18828ms`; registrations `1030/1062`; exports `413/413`;
SHA-256 `7DC94CEE64953E580B4FF528EF1FE4F883A7782C8FD252312F1F6E46E2895BF5`.
The effort-weighted estimate remains `53%`: this bounded reusable intent branch
does not close the wider CMS, codec, font, interop, performance, or shipping
workstreams.

Incremental refresh, 2026-07-19: `META-003AK` completes one-shot in-place
intents `0..3` across both legal RGB/RGB and LAB/LAB pairs with LAB-to-LAB
absolute-colorimetric intent `3`, the Python `None` analogue, same-handle exact
identity pixels, preserved caller Info, profile-independent lifetime, distinct
572-byte ICC Info, and no AHK pixel loop. ImageCms passes `76/76` in `344ms`;
full `2090/2090` in `10672ms`; registrations `1029/1061`; exports `413/413`;
SHA-256 `9EAA6510AB9D48C0E3CC81A2F095FF8ECF83294D3131DEA83BCC9607E79268B6`.
The effort-weighted estimate remains `53%`: completed one-shot in-place intent
matrices do not close the wider CMS, codec, font, interop, performance, or
shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AJ` adds one-shot LAB-to-LAB
in-place saturation intent `2` with the Python `None` analogue, same-handle
exact identity pixels, preserved caller Info, profile-independent lifetime,
distinct 572-byte ICC Info, and no AHK pixel loop. ImageCms passes `74/74` in
`312ms`; full `2088/2088` in `10109ms`; registrations `1028/1060`; exports
`413/413`; SHA-256
`67DB2646A0BF9D0F224A5A9E034DD885200765F1300E2171D8B154383F3AD0AD`.
The effort-weighted estimate remains `53%`: this bounded in-place intent branch
does not close the wider CMS, codec, font, interop, performance, or shipping
workstreams.

Incremental refresh, 2026-07-19: `META-003AI` completes one-shot RGB-to-RGB
in-place intents `0..3` with absolute-colorimetric intent `3`, the Python
`None` analogue, same-handle exact identity pixels, preserved caller Info,
profile-memory-independent lifetime, 588-byte ICC Info, and no AHK pixel loop.
ImageCms passes `72/72` in `1141ms`; full `2086/2086` in `27437ms`;
registrations `1027/1059`; exports `413/413`; SHA-256
`984CCCB8B0F98E1C0BA4C56F2E524E351DABD7B473ABE824CE65A32FF8E82E0D`.
The effort-weighted estimate remains `53%`: one completed in-place intent
matrix does not close the wider CMS, codec, font, interop, performance, or
shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AH` adds one-shot RGB-to-RGB
in-place saturation intent `2` with the Python `None` analogue, same-handle
exact identity pixels, preserved caller Info, profile-memory-independent
lifetime, 588-byte ICC Info, and no AHK pixel loop. ImageCms passes `70/70` in
`1000ms`; full `2084/2084` in `28047ms`; registrations `1026/1058`; exports
`413/413`; SHA-256
`C9EE1E7AC81CD5167851643A72838DB819ED0C351C27622EEC056EDB8E8E414E`.
The effort-weighted estimate remains `53%`: this bounded in-place intent branch
does not close the wider CMS, codec, font, interop, performance, or shipping
workstreams.

Incremental refresh, 2026-07-19: `META-003AG` completes intents `0..3` across
all four allocating one-shot pairs with LAB-to-LAB absolute-colorimetric
intent `3`, exact identity pixels, source/profile-independent lifetime,
distinct 572-byte ICC Info, and no AHK pixel loop. ImageCms passes `68/68` in
`1062ms`; full `2082/2082` in `31766ms`; registrations `1025/1057`; exports
`413/413`; SHA-256
`B3B4C87F9DE2E1D248B2589582E82E3D50F6348F4D9041ECC11FED709DC418F5`.
The effort-weighted estimate remains `53%`: completing the allocating intent
matrix does not close the wider CMS, codec, font, interop, performance, or
shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AF` adds allocating one-shot LAB-to-
LAB saturation intent `2` with exact identity pixels, source/profile-
independent lifetime, distinct 572-byte ICC Info, and no AHK pixel loop.
ImageCms passes `66/66` in `907ms`; full `2080/2080` in `29265ms`;
registrations `1024/1056`; exports `413/413`; SHA-256
`BDEC3B6A5E806D0C27A13EC831275277958750F0FC0197AFAD168864978037CA`.
The effort-weighted estimate remains `53%`: this bounded same-mode intent
branch does not close the wider CMS, codec, font, interop, performance, or
shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AE` completes allocating one-shot
RGB-to-RGB intent `0..3` coverage with absolute-colorimetric intent `3`, exact
identity pixels, source/profile-memory-independent lifetime, fresh 588-byte ICC
Info, and no AHK pixel loop. ImageCms passes `64/64` in `906ms`; full
`2078/2078` in `27703ms`; registrations `1023/1055`; exports `413/413`;
SHA-256 `119D575872616F90F27A69BD5D66BEE20BB99B2DFA0EC80160A7B6BD796C6A15`.
The effort-weighted estimate remains `53%`: this completed same-mode intent
matrix does not close the wider CMS, codec, font, interop, performance, or
shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AD` adds allocating one-shot RGB-to-
RGB saturation intent `2` with exact identity pixels, source/profile-memory-
independent lifetime, fresh 588-byte ICC Info, and no AHK pixel loop. ImageCms
passes `62/62` in `938ms`; full `2076/2076` in `26735ms`; registrations
`1022/1054`; exports `413/413`; SHA-256
`320E5DFA1A75D541663FA1E203EF9A029F693B66A55D84F27B8C951BBFD363FA`.
The effort-weighted estimate remains `53%`: this bounded same-mode intent
branch does not close the wider CMS, codec, font, interop, performance, or
shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AC` completes allocating one-shot
RGB↔LAB mode-changing intent `0..3` coverage with LAB-to-RGB absolute-
colorimetric intent `3`, exact reverse pixels, source/profile-independent
lifetime, fresh 588-byte ICC Info, and no AHK pixel loop. ImageCms passes
`60/60` in `922ms`; full `2074/2074` in `27953ms`; registrations `1021/1053`;
exports `413/413`; SHA-256
`7ED4248BD3567D1DCD9DAEDBEFA409F494DB836EEF23C6881C554FB6EA71508A`.
The effort-weighted estimate remains `53%`: this completed mode-changing
intent matrix does not close the wider CMS, codec, font, interop, performance,
or shipping workstreams.

Incremental refresh, 2026-07-19: `META-003AB` adds allocating one-shot LAB-to-
RGB saturation intent `2` with exact reverse pixels, source/profile-independent
lifetime, fresh 588-byte ICC Info, and no AHK pixel loop. ImageCms passes
`58/58` in `281ms`; full `2072/2072` in `9985ms`; registrations `1020/1052`;
exports `413/413`; SHA-256
`FF6A23B754663EC415B516163824064DEBD29766CAD83CE74155BA59C5B3CB33`.
The effort-weighted estimate remains `53%`: this bounded reverse intent branch
does not close the wider CMS, codec, font, interop, performance, or shipping
workstreams.

Incremental refresh, 2026-07-19: `META-003AA` completes allocating one-shot
RGB-to-LAB intent `0..3` coverage with absolute-colorimetric intent `3`, exact
pixels, source/profile-independent lifetime, fresh 572-byte ICC Info, and no
AHK pixel loop. ImageCms passes `56/56` in `313ms`; full `2070/2070` in
`15093ms`; registrations `1019/1051`; exports `413/413`; SHA-256
`BF16B7CA7182BD19D24961B84DC278475F65D6A8FBF92E17214C730B82929685`.
The effort-weighted estimate remains `53%`: one complete intent/pair matrix
does not close the wider CMS, codec, font, interop, performance, or shipping
workstreams.

Incremental refresh, 2026-07-19: `META-003Z` adds allocating one-shot RGB-to-
LAB saturation intent `2` with exact pixels, source/profile-independent
lifetime, fresh 572-byte ICC Info, and no AHK pixel loop. ImageCms passes
`54/54` in `437ms`; full `2068/2068` in `18031ms`; registrations `1018/1050`;
exports `413/413`; SHA-256
`93DF64E845F43421F8F45151F5A0665EE8C1C5173B6D4AC0A2BFB49E35605C65`.
The effort-weighted estimate remains `53%`: this bounded intent branch does not
close the wider CMS, codec, font, interop, performance, or shipping workstreams.

Incremental refresh, 2026-07-19: `META-003Y` completes reusable RGB-to-LAB
intent `0..3` coverage with absolute-colorimetric intent `3`, exact repeat
pixels, profile-independent lifetime, fresh 572-byte ICC Info, and no AHK pixel
loop. ImageCms passes `52/52` in `453ms`; full `2066/2066` in `13657ms`;
registrations `1017/1049`; exports `413/413`; SHA-256
`95105B01C1B252AEDB6BAC8D9AD974F2E2F28ADBC61C74E8D03DD3726D22BB49`.
The release-gate estimate remains `61%`: broader CMS APIs, codecs, fonts,
interop, benchmarks, stable ABI, and packaging remain open.

Incremental refresh, 2026-07-19: `META-003X` adds reusable RGB-to-LAB
saturation intent `2` with exact repeat pixels, profile-independent lifetime,
fresh 572-byte ICC Info, and no AHK pixel loop. ImageCms passes `50/50` in
`437ms`; full `2064/2064` in `19734ms`; registrations `1016/1048`; exports
`413/413`; SHA-256
`4458CEFD629534307A29EDAC34F0D60C548BF188C864032C8DFCB1C2DCAD6C45`.
The release-gate estimate remains `61%`: this begins another intent slice but
does not close the wider CMS/release capability gates.

Incremental refresh, 2026-07-19: `META-003W` completes one-shot in-place
relative-colorimetric intent `1` across both legal same-mode pairs with D50-to-
6500K LAB/LAB same-handle identity bytes, caller Info preservation, distinct
572-byte ICC merge, profile independence, and no AHK pixel loop. ImageCms
passes `48/48` in `359ms`; full `2062/2062` in `15015ms`; registrations
`1015/1047`; exports `413/413`; SHA-256
`2BF08CFD0D4FF7ADAE7BE52E9A2990AAFFF02C6253ED5C3B6055333ACEC8076A`.
The release-gate estimate remains `61%`: intent-matrix depth alone does not
close the broader CMS or release gates.

Incremental refresh, 2026-07-19: `META-003V` adds one-shot RGB-to-RGB native
in-place relative-colorimetric intent `1` with same-handle exact bytes, caller
Info preservation, 588-byte ICC merge, profile-memory independence, and no AHK
pixel loop. ImageCms passes `46/46` in `344ms`; full `2060/2060` in `18297ms`;
registrations `1014/1046`; exports `413/413`; SHA-256
`694A49B193366C86E26B60A83EEF6181EC7392EECDDF7681579AF0C359F973DC`.
The release-gate estimate remains `61%`: this is another bounded CMS branch,
not closure of metadata/CMS, codecs, fonts, interop, benchmarks, or release.

Incremental refresh, 2026-07-19: `META-003U` completes allocating one-shot
relative-colorimetric intent `1` across the fourth established pair, D50-to-
6500K LAB/LAB, with exact identity bytes, source/profile independence, distinct
572-byte output ICC Info, and no AHK pixel loop. ImageCms passes `44/44` in
`328ms`; full `2058/2058` in `17734ms`; registrations `1013/1045`; exports
`413/413`; SHA-256
`1CECC50202ACA44A144614F6CCB64E5EE387EEC0712FDA15DC0F529C3127C1CB`.
The release-gate estimate remains `61%`: allocating intent breadth deepens but
does not complete the wider CMS/release capability gates.

Incremental refresh, 2026-07-19: `META-003T` admits allocating one-shot RGB-
to-RGB relative-colorimetric intent `1` with a memory-opened output profile,
exact identity pixels, source/profile-memory independence, 588-byte output ICC
Info, and no AHK pixel loop. ImageCms passes `42/42` in `359ms`; full
`2056/2056` in `18312ms`; registrations `1012/1044`; exports `413/413`;
SHA-256 `42A4AD167F1F5F1522EA6FD9748A455F37A1A2E053DB0F30A4EA9DB8AB698EC5`.
The release-gate estimate remains `61%`: same-mode intent depth does not close
the broader CMS, format, font, interop, benchmark, or release gates.

Incremental refresh, 2026-07-19: `META-003S` admits allocating one-shot LAB-
to-RGB relative-colorimetric intent `1` with flags `0`, exact reverse pixels,
source preservation, 588-byte output ICC Info, profile-independent result
lifetime, and no AHK pixel loop. ImageCms passes `40/40` in `391ms`; full
`2054/2054` in `17797ms`; registrations `1011/1043`; exports `413/413`;
SHA-256 `F485C52DA03EC7E5C12366F0261A8A734CE5EDDAEE369B0ED326709B0564CD68`.
The release-gate estimate remains `61%`: this remains one bounded branch inside
the incomplete CMS gate.

Incremental refresh, 2026-07-19: `META-003R` admits allocating one-shot RGB-
to-LAB relative-colorimetric intent `1` with flags `0`, exact result/source
semantics, output ICC Info, profile-independent result lifetime, and no AHK
pixel loop. ImageCms passes `38/38` in `328ms`; full `2052/2052` in `18156ms`;
registrations `1010/1042`; exports `413/413`; SHA-256
`95C5C6849BAEA28D26CA37E40EF7E49D3FEFE163D32248B1C234E64D1348A5E3`.
The release-gate estimate remains `61%`: this deepens the same CMS gate while
format breadth, fonts, interop, benchmarks, stable ABI, and release engineering
remain material deductions.

Incremental refresh, 2026-07-19: `META-003Q` admits reusable RGB-to-LAB
relative-colorimetric intent `1` with flags `0`, exact repeat pixels, profile-
independent lifetime, output ICC Info, and no AHK pixel loop. ImageCms passes
`36/36` in `250ms`; full `2050/2050` in `17734ms`; registrations `1009/1041`;
exports `413/413`; SHA-256
`7A31411CD5CBA65AC69018274FF0F438BB00115E42AFFD00242258860A67D4E2`.
The release-gate estimate remains `61%`: proofing, full profile information,
profile input variants, fonts, interop, codec breadth, benchmarks, and release
engineering still prevent another gate point.

Incremental refresh, 2026-07-19: `META-003P` adds one-shot LAB-to-LAB native
in-place ImageCms `profileToProfile` with same-object exact bytes, D50/6500K
ICC Info merge, and no AHK pixel loop. ImageCms passes `34/34` in `218ms`;
full `2048/2048` in `17719ms`; registrations `1008/1040`; exports `413/413`;
SHA-256 `21FB8FCB5790DE8AEFDE7A80A03585055154B3DE199BF15726F2F5E8752A4818`.
The workload estimate remains `68%`: non-default intent/flags, proofing, broad
profile information, and profile input variants remain material ImageCms gaps.

Incremental refresh, 2026-07-19: `META-003O` adds reusable LAB-to-LAB native
in-place ImageCms apply with same-handle repeat execution, exact identity
public bytes, profile-independent lifetime, and 572-byte ICC Info merge.
ImageCms passes `32/32` in `235ms`; full `2046/2046` in `18687ms`;
registrations `1007/1039`; exports `413/413`; SHA-256
`346A02F825C6D84995D0B61E48DCA0BFE903CF6EDCFACD8C48F9F01C5F0435AD`.
The workload estimate remains `68%`: ImageCms remains incomplete beyond these
bounded profile/mode pairs.

Incremental refresh, 2026-07-19: `META-003N` adds allocating one-shot LAB-to-
LAB ImageCms `profileToProfile` for bounded D50/6500K profiles with exact
identity public bytes, distinct 572-byte output ICC Info, and source
preservation. ImageCms passes `30/30` in `250ms`; full `2044/2044` in
`18796ms`; registrations `1006/1038`; exports `413/413`; SHA-256
`05D52211CC02AD3211E4BC530F67F4CFB5A48FE737636EAA346CD1B3D4212FBA`.
The workload estimate remains `68%`: ImageCms breadth is still incomplete.

Incremental refresh, 2026-07-19: `META-003M` adds reusable LAB-to-LAB
ImageCms transforms for bounded D50/6500K profiles with exact Pillow identity
bytes, profile-independent lifetime, and distinct 572-byte output ICC Info.
ImageCms passes `28/28` in `250ms`; full `2042/2042` in `17250ms`;
registrations `1005/1037`; exports `413/413`; SHA-256
`DB72B63D489E28B8E97BBFEDD22BF6F389407903AC052C83D7F8E3F96833F551`.
The workload estimate remains `68%`: this adds a mode pair but ImageCms remains
materially incomplete across profiles, intents, proofing, and information APIs.

Incremental refresh, 2026-07-19: `META-003L` adds one-shot ImageCms
`profileToProfile` RGB-to-RGB native in-place execution, same-handle mutation,
Pillow-compatible return/Info merge, and no AHK pixel loop. ImageCms passes
`26/26` in `125ms`; full `2040/2040` in `20110ms`; registrations `1004/1036`;
exports `413/413`; SHA-256
`E000DE6664FA98AE1293BC6C2713EB2971A9C70279B70474706F618870CA5C8C`.
The workload estimate remains `68%`: ImageCms breadth is still incomplete.

Incremental refresh, 2026-07-19: `META-003K` adds allocating one-shot
ImageCms `profileToProfile` RGB-to-RGB for built-in/memory-opened profiles,
exact output ICC Info, and source preservation without AHK pixel loops.
ImageCms passes `24/24` in `109ms`; full `2038/2038` in `10844ms`;
registrations `1003/1035`; exports `412/412`; SHA-256
`435763AE8D4B0A7EEDA62A9073B7A983E8155624E44022F70012B8F86B33BF99`.
The workload estimate remains `68%`: this closes another API route inside the
same incomplete ImageCms representative workload.

Incremental refresh, 2026-07-19: `META-003J` adds native RGB-to-RGB
ImageCms in-place apply with same-handle storage reuse, repeat execution,
Pillow-compatible return/Info merge, and no AHK pixel loop. ImageCms passes
`22/22` in `94ms`; full `2036/2036` in `10218ms`; registrations `1002/1034`;
exports `412/412`; SHA-256
`75898676451C19C6DFC8F4D57AE94F2211F2DA0F8DFDBE132D80F43A9BE67613`.
The workload estimate remains `68%`: the in-place hot path deepens the current
CMS job but complete ImageCms still spans additional representative work.

Incremental refresh, 2026-07-19: `META-003I` adds reusable native RGB-to-RGB
ImageCms transforms with built-in/memory-opened profile and source-Buffer-
independent lifetime, exact repeat-apply bytes, and fresh 588-byte output ICC
Info. ImageCms passes `20/20` in `109ms`; full `2034/2034` in `10078ms`;
registrations `1001/1033`; exports `411/411`; SHA-256
`588BE122B6341DE05A58AA2C298104D002C77C78DDA70311DBBE7DD7D6DDDD2E`.
The workload estimate remains `68%`: same-mode transforms deepen the existing
color-management job without completing another representative job yet.

Incremental refresh, 2026-07-19: `META-003H` generalizes the reusable native
ImageCmsTransform to LAB-to-RGB with profile-independent lifetime, exact
repeat-apply pixels, and fresh 588-byte sRGB ICC Info. ImageCms passes `18/18`
in `93ms`; full `2032/2032` in `9610ms`; registrations `1000/1032`; exports
`411/411`; SHA-256
`4F7A0035D4DB9E55206BB050AF8455754E923938456834A11C0B9EFD0FE36E29`.
The workload estimate remains `68%`: this deepens an existing color-management
job but does not yet complete another representative job in the 100-job model.

Incremental refresh, 2026-07-19: `META-003G` adds reusable native
ImageCmsTransform build/repeat-apply/output-ICC/lifetime for RGB-to-LAB.
ImageCms passes `16/16` in `94ms`; full `2030/2030` in `9625ms`;
registrations `999/1031`; exports `411/411`; SHA-256
`F44BC7520328F2636BB6BFB0B7FBD00A2C42FE16983822DAE0CA53CD969DB9AD`.

Incremental refresh, 2026-07-19: `META-003F` adds native ICC Buffer profile
opening with source-memory-independent lifetime and an `ImageCmsProfile`
facade wrapper. ImageCms passes `14/14` in `63ms`; full `2028/2028` in
`9656ms`; registrations `998/1030`; exports `407/407`; SHA-256
`3EB41071735EAF2910A72F8654F3D27D57C3127A452339D19057D37D483479F1`.

Incremental refresh, 2026-07-19: `META-003E` adds the native reverse
LAB/LAB-to-RGB/sRGB profile-to-profile pair with exact output ICC Info.
ImageCms passes `12/12` in `78ms`; full `2026/2026` in `9797ms`;
registrations `997/1029`; exports `406/406`; SHA-256
`55EB6CACE37F53BE334667529E4492FA23F64A50CE288D9CD853C7B82A802E6A`.

Incremental refresh, 2026-07-19: `META-003D` adds native ImageCms
RGB/sRGB-to-LAB/LAB profile-to-profile conversion plus output ICC profile
serialization. ImageCms passes `10/10` in `47ms`; full `2024/2024` in
`10016ms`; registrations `996/1028`; exports `406/406`; SHA-256
`E483B8741FE459F83950BE8C453B226953AB526796540A13EBE6D21A34E03085`.

Incremental refresh, 2026-07-19: `META-003C` adds built-in XYZ profile
creation and Pillow-compatible ignored color temperature. ImageCms tests pass
`7/7` in `32ms`; full `2021/2021` in `10093ms`; registrations
`994/1027`; exports `404/404`; SHA-256
`ECF493E6F7974F0130DB00CBE4215DFD4A0EC0A02C83BE022C98089218C4D0CD`.

Incremental refresh, 2026-07-19: `META-003B` adds built-in LAB profile
creation for default/6500K requests and case-sensitive facade profile-space
normalization. ImageCms tests pass `5/5` in `125ms`; full `2019/2019` in
`10282ms`; registrations `993/1026`; exports `403/403`; SHA-256
`64B5497BC1A23DD9C062B15587A14E44F7C85D91703CFC8055BDA9C8621826FB`.

Incremental refresh, 2026-07-19: `META-003A` adds the first public ImageCms
profile object: DLL-owned built-in sRGB creation, exact `sRGB built-in\n` name
query, and explicit facade lifetime. Raw/facade pass `1/1` in `62ms`/`31ms`;
full `2016/2016` in `10000ms`; registrations `992/1024`; exports `402/402`;
SHA-256 `05F61A2793F56B7673D6EDD4FD944D48D84CE2DFAAD06AF50CFC1A85421BC1E0`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AX` completes full-binary+
single-ASCII parity. Raw/facade `1/1` in `141ms`/`31ms`; full `2014/2014` in
`10046ms`; registrations `991/1023`; exports `399/399`; SHA-256
`053B555079CFFEDB4E0E75C350CF683BF821A082DCAE825C6CEB1879D4673E59`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AW` adds strict uncompressed
`I;16B` DPI+ICC+XMP+Description composition. Raw/facade passed `1/1` in
`156ms`/`31ms`; TIFF `342/342`, save_all `67/67`, full `2012/2012` in
`10421ms`; registrations `990/1022`; exports `399/399`; SHA-256
`4B06A3C27570CF35284C1AC1916A2B42EA13ECCB28CFDFF1BC8D999BF3F7773C`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AV` composes one uncompressed
`I;16B` frame with DPI, XMP, ImageDescription `Hello`, and Artist `Ada`.
Pillow writes a 548-byte big-endian TIFF with 15 sorted IFD entries,
Description offset `194`, inline `Ada\0`, DPI offsets `200/208`, XMP offset
`216`, raw strip offset `540`, and reopened mode `I;16B`. Native completes the
strict XMP+validated-one/two-ASCII family while retaining its uncompressed
single-frame boundary; existing facade routing passes without production
change. Raw/facade passed `1/1` in `125ms` and `1/1` in `47ms`; TIFF `340/340`
in `1625ms`, save_all `67/67` in `578ms`, and full `2010/2010` in `9938ms`.
Registrations are `989/1021`; exports/DLL remain `399/399`, and SHA-256 is
`B4EF7519F3E16437EEB4A90224281A888D556DD3C54FF12D30334141C6919B28`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AU` composes one uncompressed
`I;16B` frame with DPI, XMP, and Artist `Ada`. Pillow writes a 530-byte big-
endian TIFF with 14 sorted IFD entries, inline `Ada\0`, DPI offsets `182/190`,
XMP offset `198`, raw strip offset `522`, and reopened mode `I;16B`. Native
completes the strict XMP+one-ASCII family for tags `270`/`315` while retaining
its uncompressed single-frame boundary; existing facade routing passes
without production change. Raw/facade passed `1/1` in `140ms` and `1/1` in
`32ms`; TIFF `338/338` in `1657ms`, save_all `67/67` in `563ms`, and full
`2008/2008` in `10093ms`. Registrations are `988/1020`; exports/DLL remain
`399/399`, and SHA-256 is
`71B3A33D96CCBE4000477572ACCCA274542E2A65837C04B1C40048426A4AEB5E`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AT` composes one uncompressed
`I;16B` frame with DPI, XMP, and ImageDescription `Hello`. Pillow writes a
536-byte big-endian TIFF with 14 sorted IFD entries, Description offset `182`,
DPI offsets `188/196`, XMP offset `204`, raw strip offset `528`, and reopened
mode `I;16B`. Native admits only the strict XMP+tag-270 configuration while
retaining its uncompressed single-frame boundary; existing facade routing
passes without production change. Raw/facade passed `1/1` in `109ms` and
`1/1` in `31ms`; TIFF `336/336` in `1547ms`, save_all `67/67` in `531ms`, and
full `2006/2006` in `10234ms`. Registrations are `987/1019`; exports/DLL
remain `399/399`, and SHA-256 is
`E643014A8FF90F7DF552D598E052E359625FAEE16A0FA48DE12D6741DB6D32F0`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AS` composes one uncompressed
`I;16B` frame with DPI, ICC, ImageDescription `Hello`, and Artist `Ada`.
Pillow writes a 232-byte big-endian TIFF with 15 sorted IFD entries,
Description offset `194`, inline `Ada\0`, DPI offsets `200/208`, ICC offset
`216`, raw strip offset `224`, and reopened mode `I;16B`. Native completes the
validated ICC+one/two-ASCII family while retaining its strict uncompressed
single-frame boundary; existing facade routing passes. Raw/facade passed
`1/1` in `140ms` and `1/1` in `31ms`; TIFF `334/334` in `1703ms`, save_all
`67/67` in `578ms`, and full `2004/2004` in `9703ms`. Registrations are
`986/1018`; exports/DLL remain `399/399`, and SHA-256 is
`B5A93E491DDEA00D8EC4C8DC34C39EE9434BBAFA0740071DEBFE9BDECC1E0377`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AR` composes one uncompressed
`I;16B` frame with DPI, ICC, and Artist `Ada`. Pillow writes a 214-byte big-
endian TIFF with 14 sorted IFD entries, inline `Ada\0`, DPI offsets `182/190`,
ICC offset `198`, raw strip offset `206`, and reopened mode `I;16B`. Native
deepens AQ's exact guard to the two validated single-ASCII tags while retaining
the uncompressed single-frame boundary; existing facade routing passes. Raw/
facade passed `1/1` in `94ms` and `1/1` in `16ms`; TIFF `332/332` in `1828ms`,
save_all `67/67` in `563ms`, and full `2002/2002` in `9766ms`. Registrations
are `985/1017`; exports/DLL remain `399/399`, and SHA-256 is
`815E6936CD5D7DC3F22268E35B3E7C144B31498654CB95D75C2DBEECF4940F43`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AQ` composes one uncompressed
`I;16B` frame with DPI, ICC, and ImageDescription `Hello`. Pillow writes a
220-byte big-endian TIFF with 14 sorted IFD entries, Description offset `182`,
DPI offsets `188/196`, ICC offset `204`, raw strip offset `212`, and reopened
mode `I;16B`. Native admits only this exact uncompressed binary+ASCII subset;
existing facade routing passes without production change. Raw/facade passed
`1/1` in `157ms` and `1/1` in `47ms`; TIFF `330/330` in `1703ms`, save_all
`67/67` in `594ms`, and full `2000/2000` in `9500ms`. Registrations are
`984/1016`; exports/DLL remain `399/399`, and SHA-256 is
`E556B39212C51BF8C5096DFD25CE04F2070863FD7FD1E58289554CB298B7850E`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AP` composes one uncompressed
`I;16B` frame with DPI, ICC, and XMP. Pillow writes a 538-byte big-endian TIFF
with 14 sorted IFD entries, DPI offsets `182/190`, XMP offset `198`, ICC offset
`522`, raw strip offset `530`, and reopened mode `I;16B`. Native admits the
binary pair through the AI writer; existing facade routing passes without
production change. Raw/facade passed `1/1` in `125ms` and `1/1` in `47ms`;
TIFF `328/328` in `1766ms`, save_all `67/67` in `546ms`, and full `1998/1998`
in `9484ms`. Registrations are `983/1015`; exports/DLL remain `399/399`, and
SHA-256 is
`44F8A1B59900DD92247DF2A31E56B1B2263D9FC2D4CCE45837A3145840202683`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AO` composes one uncompressed
`I;16B` frame with DPI, ImageDescription `Hello`, and Artist `Ada`. Pillow
writes a 212-byte big-endian TIFF with 14 sorted IFD entries, Description
offset `182`, inline `Ada\0`, DPI offsets `188/196`, raw strip offset `204`,
and reopened mode `I;16B`. Native admits exact tag set `{270,315}` through the
AI writer; existing facade routing passes without production change. Raw/
facade passed `1/1` in `109ms` and `1/1` in `15ms`; TIFF `326/326` in `1625ms`,
save_all `67/67` in `609ms`, and full `1996/1996` in `9859ms`. Registrations
are `982/1014`; exports/DLL remain `399/399`, and SHA-256 is
`03D69209EF9EB07964A9B483FC0018B4DFA966A1DF130EA6CCF9C650BB8391B7`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AN` adds one uncompressed
`I;16B` frame with DPI and Artist `Ada`. Pillow writes a 194-byte big-endian
TIFF with 13 sorted IFD entries, inline `Ada\0`, DPI offsets `170/178`, raw
strip offset `186`, and reopened mode `I;16B`. Native admits DPI+tag-315
through the AI writer; facade `TiffInfo[315]` plus DPI now routes through the
metadata-ASCII ABI. Raw/facade passed `1/1` in `93ms` and `1/1` in `15ms`;
TIFF `324/324` in `1562ms`, save_all `67/67` in `562ms`, and full `1994/1994`
in `9922ms`. Registrations are `981/1013`; exports/DLL remain `399/399`, and
SHA-256 is
`4D501BCA2D12C97AA656339A8AE2BC60066303C2DECCAAB9BB2AFB2AD89C9011`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AM` adds one uncompressed
`I;16B` frame with DPI and ImageDescription `Hello`. Pillow writes a 200-byte
big-endian TIFF with 13 sorted IFD entries, Description offset `170`, DPI
offsets `176/184`, raw strip offset `192`, and reopened mode `I;16B`. Native
admits DPI+tag-270 through the AI writer; facade `TiffInfo[270]` plus DPI now
routes through the metadata-ASCII ABI. Raw/facade passed `1/1` in `94ms` and
`1/1` in `31ms`; TIFF `322/322` in `1500ms`, save_all `67/67` in `562ms`, and
full `1992/1992` in `9547ms`. Registrations are `980/1012`; exports/DLL remain
`399/399`, and SHA-256 is
`94ED2CE0CA2151A47DA4C2BFDE0ED922D717908E4BE4FD384E7F7A812C46F6DD`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AL` adds one uncompressed
`I;16B` frame with DPI and XMP. Pillow writes a 518-byte big-endian TIFF with
13 sorted IFD entries, DPI offsets `170/178`, BYTE XMP offset `186`, raw strip
offset `510`, and reopened mode `I;16B`. Native admits DPI+XMP through the AI
writer; facade `TiffInfo[700]` plus DPI now routes through the metadata ABI.
Raw/facade passed `1/1` in `78ms` and `1/1` in `16ms`; TIFF `320/320` in
`1563ms`, save_all `67/67` in `578ms`, and full `1990/1990` in `9532ms`.
Registrations are `979/1011`; exports/DLL remain `399/399`, and SHA-256 is
`6E523F767A679733489B27668FBF5AEB4278A5E57C02C63655F47FF154AD4AE8`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AK` adds one uncompressed
`I;16B` frame with DPI and ICC. Pillow writes a 202-byte big-endian TIFF with
13 sorted IFD entries, DPI offsets `170/178`, UNDEFINED ICC offset `186`, raw
strip offset `194`, and reopened mode `I;16B`. Native admits DPI+ICC through
the AI writer; facade ICC+DPI now routes through the metadata ABI. Raw/facade
passed `1/1` in `109ms` and `1/1` in `15ms`; TIFF `318/318` in `1579ms`,
save_all `67/67` in `578ms`, and full `1988/1988` in `9422ms`. Registrations
are `978/1010`; exports/DLL remain `399/399`, and SHA-256 is
`E2759F99E0F9B50F1ADCFCAF0B2BB85D2DF7D06A0EBE29BED43BE27EDF097BB3`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AJ` adds one uncompressed
`I;16B` frame with DPI only. Pillow writes a 182-byte big-endian TIFF with 12
sorted IFD entries, X/Y Resolution offsets `158/166`, raw strip offset `174`,
and reopened mode `I;16B`. Native admits DPI-only through the AI big-endian
writer; the existing facade options route required no change. Raw/facade
passed `1/1` in `78ms` and `1/1` in `16ms`; TIFF `316/316` in `1469ms`,
save_all `67/67` in `515ms`, and full `1986/1986` in `9391ms`. Registrations
are `977/1009`; exports/DLL remain `399/399`, and SHA-256 is
`62A84B20D6EC34EFFB944B0A7E73303496FC8A1AD395511F7CA5B98321977598`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AI` composes one uncompressed
`I;16B` frame with DPI, ICC, XMP, ImageDescription, and Artist. Pillow writes
a 568-byte big-endian TIFF with 16 sorted IFD entries, exact external offsets,
an unchanged raw strip, and reopened mode `I;16B`. Native extends the dedicated
big-endian writer and the facade now routes the exact single-frame full-
metadata case through the generalized ABI. Raw/facade passed `1/1` in `94ms`
and `1/1` in `15ms`; TIFF `314/314` in `1469ms`, save_all `67/67` in `609ms`,
and full `1984/1984` in `9313ms`. Registrations are `976/1008`; exports/DLL
remain `399/399`, and SHA-256 is
`8D23C9F10BCA13E3AA63C9D703C53D70127E2F8D9AE1E128474D01845D7E6CA9`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AH` composes homogeneous
two-frame `I;16B` LZW+DPI with ICC, XMP, ImageDescription, and Artist. Pillow
writes exact type/count/payload layouts for tags `270`, `315`, `700`, and
`34675` in both IFDs, exact 12-byte LZW strips, little-endian reopened samples,
and unchanged sources. Native now admits the exact full composition through
temporary normalization. Raw/facade passed `1/1` in `157ms` and `1/1` in
`16ms`; TIFF `312/312` in `1532ms`, save_all `67/67` in `547ms`, and full
`1982/1982` in `9641ms`. Registrations are `975/1007`; exports/DLL remain
`399/399`, and SHA-256 is
`CA98C6A7BA635438C202FBC89EE7F341204E4995313FA2A2424BE437693D2063`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AG` composes homogeneous
two-frame `I;16B` LZW+DPI with both ImageDescription `Hello` and Artist `Ada`.
Pillow writes out-of-line ASCII type `2`, count `6`, exact `Hello\0` and inline
type `2`, count `4`, exact `Ada\0` to both IFDs, with exact 12-byte LZW strips,
little-endian reopened samples, and unchanged sources. Native now admits the
exact two-tag set through temporary normalization. Raw/facade passed `1/1` in
`157ms` and `1/1` in `32ms`; TIFF `310/310` in `1547ms`, save_all `66/66` in
`562ms`, and full `1980/1980` in `9938ms`. Registrations are `974/1006`;
exports/DLL remain `399/399`, and SHA-256 is
`703A00252FCEDF2C6244E6AEBFC18E9C0891A0A394C895EC046BFE0DB3F77067`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AF` composes homogeneous
two-frame `I;16B` LZW+DPI with Artist `Ada`. Pillow writes inline ASCII type
`2`, count `4`, exact `Ada\0` to both IFDs, normalizes saved frames to
little-endian `I;16`, and preserves sources. Native now admits either one tag
`270` or `315` through temporary normalization. Raw/facade passed `1/1` in
`125ms` and `1/1` in `31ms`; TIFF `308/308` in `1485ms`, save_all `65/65` in
`515ms`, and full `1978/1978` in `9829ms`. Registrations are `973/1005`;
exports/DLL remain `399/399`, and SHA-256 is
`36F2542CA039D955BFA5B47D1BD41A3C31380092BE9DDEFD65E02AE3A7514F30`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AE` composes homogeneous
two-frame `I;16B` LZW+DPI with ImageDescription `Hello`. Pillow writes ASCII
type `2`, count `6`, exact `Hello\0` to both IFDs, normalizes saved frames to
little-endian `I;16`, and preserves source handles. Native now carries tag-270
arrays through temporary normalization and explicitly rejects uncompressed
metadata instead of dropping it. Raw/facade passed `1/1` in `125ms` and `1/1`
in `31ms`; TIFF `306/306` in `1468ms`, save_all `64/64` in `562ms`, and full
`1976/1976` in `9750ms`. Registrations are `972/1004`; exports/DLL remain
`399/399`, and SHA-256 is
`A80222381976DA923E3EE4EFDF3FD8D1DB1E30FD61B002C4C0E472B800372FB0`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AD` composes a homogeneous
two-frame `I;16B` pair with LZW, DPI `(300,150)`, exact ICC, and exact XMP.
Pillow writes UNDEFINED `34675` and BYTE `700` to both IFDs, normalizes both
saved frames to little-endian `I;16`, and preserves source handles. Raw RED
exposed the explicit combined guard; native now carries both payloads through
temporary normalization. Raw/facade passed `1/1` in `109ms` and `1/1` in
`32ms`; TIFF `304/304` in `1500ms`, save_all `63/63` in `500ms`, and full
`1974/1974` in `9688ms`. Registrations are `971/1003`; Release x64 built with
zero warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`629DD662C6ACDCA4A8805493FF0ED866274533D370279DBF05C9D06CD84D3AEA`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AC` composes a homogeneous
two-frame `I;16B` pair with LZW, DPI `(300,150)`, and exact tag-700 XMP bytes.
Pillow writes BYTE count `324` to both IFDs, normalizes both saved frames to
little-endian `I;16`, and preserves source handles. Raw RED exposed the native
XMP guard; native now carries XMP-only through temporary normalization; AD
later composes ICC+XMP. Raw/facade passed `1/1` in `172ms` and `1/1` in
`47ms`; TIFF `302/302` in `1500ms`, save_all `62/62` in `500ms`, and full
`1972/1972` in `9656ms`. Registrations are `970/1002`; Release x64 built with
zero warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`29770E7CAC1A59C4D6455405A9A7A39F54A8DFD517E4D5D6A607F9D9C4C7365A`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AB` composes a homogeneous
two-frame `I;16B` pair with LZW, DPI `(300,150)`, and exact ICC bytes. Pillow
writes UNDEFINED tag `34675` to both IFDs, normalizes both saved frames to
little-endian `I;16`, and preserves source handles. Raw RED exposed the native
metadata guard; native now carries validated ICC through temporary
normalization into the shared writer. Raw/facade passed `1/1` in `94ms` and
`1/1` in `31ms`; TIFF `300/300` in `1563ms`, save_all `61/61` in `484ms`, and
full `1970/1970` in `9422ms`. Registrations are `969/1001`; Release x64 built
with zero warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`58D1896D7F7C6B99AB68D2F1BB1D8D027512F011BA7DEC793E9C90D92990E30F`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001AA` batches both mixed
`I;16`/`I;16B` two-frame orders under PackBits/LZW/Adobe Deflate plus DPI.
Pillow preserves both source handles and normalizes each saved frame
independently to little-endian `I;16`. Raw RED exposed Z's homogeneous-only
guard. Native now recognizes the 16-bit endian family and swaps only big-endian
members. Raw/facade passed `1/1` in `250ms` and `1/1` in `63ms`; TIFF
`298/298` in `1500ms`, save_all `60/60` in `469ms`, and full `1968/1968` in
`9312ms`. Registrations are `968/1000`; Release x64 built with zero warnings/
errors; exports/DLL remain `399/399`, and SHA-256 is
`3C90BAEA740D3DA52D91891777F6E39E3994A57B1752584C362D5EBF8B4ECF2F`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001Z` batches homogeneous two-frame
mode `I;16B` PackBits/LZW/Adobe Deflate plus DPI. Pillow preserves source modes
but normalizes every saved frame to little-endian `I;16`. Raw RED exposed the
native single-frame/no-DPI restriction. Native now creates per-frame temporary
`I;16` copies and reuses the shared compression/IFD/DPI writer. PackBits/LZW
bounded strips match Pillow exactly; native Deflate uses valid 19-byte stored
blocks versus Pillow's 16-byte streams. Raw/facade passed `1/1` in `156ms` and
`1/1` in `47ms`; TIFF `296/296` in `1422ms`, save_all `59/59` in `437ms`, and
full `1966/1966` in `9516ms`. Registrations are `967/999`; Release x64 built
with zero warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`395E0D5D1CE3394556031B6C25C5A77FA380361927D26DC4A3064568278072CB`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001Y` batches two-frame mode `I`
and `F` PackBits/LZW/Adobe Deflate plus DPI. Raw RED exposed a native PackBits
strategy mismatch on a two-zero run. The writer now follows libtiff's
Base/Literal/Run/LiteralRun transitions and conditional coalescing, making
the bounded PackBits and LZW strips byte-identical to Pillow. Native Deflate
uses valid 27-byte stored blocks; all six combinations preserve exact samples,
no Predictor, and matching DPI/GetExif. Raw/facade passed `1/1` in `219ms`
and `1/1` in `62ms`; TIFF `294/294` in `1375ms`, save_all `58/58` in `438ms`,
and full `1964/1964` in `9625ms`. Registrations are `966/998`; Release x64
built with zero warnings/errors; exports/DLL remain `399/399`, and SHA-256 is
`F877A0057B8D54ACD2DD11A32622BDCE37BD22D185E222862E9B5AB00E2E7CBB`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001X` batches two-frame
little-endian `I;16` LZW/Adobe Deflate plus DPI. LZW/native/Pillow use exact
12-byte strips; native Deflate uses valid 19-byte stored blocks versus
Pillow's 16-byte streams, with exact decoded samples and matching DPI/GetExif.
Existing raw/facade routes passed `1/1` in `109ms` and `1/1` in `47ms`; TIFF
`292/292` in `1219ms`, save_all `57/57` in `375ms`, and full `1962/1962` in
`9468ms`. Registrations are `965/997`; no production change or rebuild;
exports/DLL remain `399/399` and
`A61C6E56CA121828C29F19222157E950924D9BC2290BFAC2737E756409E003F5`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001W` composes two-frame
little-endian `I;16` PackBits plus DPI. Pillow/native write Compression
`32773`, RowsPerStrip `2`, exact ten-byte strips, exact decoded samples, and
matching initial/selected DPI/GetExif. Existing raw/facade routes passed
`1/1` in `62ms` and `1/1` in `31ms`; TIFF `290/290` in `1125ms`, save_all
`56/56` in `359ms`, and full `1960/1960` in `9422ms`. Registrations are
`964/996`; no production change or rebuild; exports/DLL remain `399/399` and
`A61C6E56CA121828C29F19222157E950924D9BC2290BFAC2737E756409E003F5`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001V` closes IFD0 DPI for bounded
mode `I` and mode `F` two-frame TIFFs before any seek. Batched raw RED found
both initial early numeric handles had exact bytes but `HasDpi == 0`; IFD0
resolution parsing fixed both without facade or ABI changes. Final raw/facade
passed `1/1` in `157ms` and `1/1` in `31ms`; TIFF `288/288` in `1172ms`,
save_all `55/55` in `359ms`, and full `1958/1958` in `9641ms`. Registrations
are `963/995`; Release x64 built cleanly; exports/DLL remain `399/399` and
`A61C6E56CA121828C29F19222157E950924D9BC2290BFAC2737E756409E003F5`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001U` closes IFD0 DPI for the
bounded little-endian `I;16` two-frame TIFF before any seek. Raw RED found
the initial early `I;16` return had exact bytes but `HasDpi == 0`; IFD0
resolution parsing fixed the native handle without facade or ABI changes.
Final raw/facade passed `1/1` in `79ms` and `1/1` in `16ms`; TIFF `286/286`
in `1188ms`, save_all `54/54` in `359ms`, and full `1956/1956` in `9109ms`.
Registrations are `962/994`; Release x64 built cleanly; exports/DLL remain
`399/399` and
`0C23441D2E57C4300FEB6025D3A3DDF9F7C21786618928D30705246EBF5581FE`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001T` batches selected-frame DPI
for same-size mode `I` and mode `F` two-frame TIFFs. Pillow/native preserve
exact signed-int32/float32 bytes and expose `300/1`, `150/1`, unit `2`, and
facade DPI/GetExif after seek. Raw RED found `HasDpi == 0` in the shared early
numeric selected-frame return; selected-IFD parsing fixed both modes without
a facade or ABI change. Final raw/facade passed `1/1` in `156ms` and `1/1` in
`31ms`; TIFF `284/284` in `1046ms`, save_all `53/53` in `297ms`, and full
`1954/1954` in `8938ms`. Registrations are `961/993`; Release x64 built
cleanly; exports/DLL remain `399/399` and
`2AA796C0723CB49838E4D899D42BF9725F00F4A1F603659C0595A9EFF32040D4`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001S` closes selected-frame DPI on
one same-size little-endian `I;16` two-frame TIFF. Pillow 11.3.0 preserves the
exact samples and exposes `300/1`, `150/1`, unit `2`, and `Info["dpi"]` for
both frames. Raw RED found `HasDpi == 0` only on the early selected `I;16`
return; selected-IFD parsing fixed it without a facade or ABI change. Final
raw/facade passed `1/1` in `63ms` and `1/1` in `32ms`; TIFF `282/282` in
`1469ms`, save_all `52/52` in `297ms`, and full `1952/1952` in `9078ms`.
Registrations are `960/992`; Release x64 built cleanly; exports/DLL remain
`399/399` and
`906397FBD41F7BA0CB19A2B6A08BCF3582CF419C80E1041A47C366D6C447DA20`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001R` composes two-frame RGB LZW,
DPI `(300,150)`, ICC, XMP, and ASCII tags `270`/`315` through the generalized
route. Every IFD/seek state preserves all metadata and exact pixels. Raw RED
found selected frame DPI absent because native resolution parsing was gated to
IFD0; selected-IFD parsing fixed the root cause. Final raw/facade passed `1/1`
in `125ms` and `1/1` in `15ms`; TIFF `280/280` in `1187ms`, save_all `51/51`
in `328ms`, and full `1950/1950` in `8938ms`. Registrations are `959/991`;
Release x64 built cleanly; exports/DLL remain `399/399` and
`6E658A0051D5E8BF7346691872B39A176B1C13F7DB1F0B2F8CF2498B2DFF7997`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001Q` generalizes two-frame RGB
ASCII save_all to one `tiffinfo={270: "Hello", 315: "Ada"}` Map. Native writes
both type-`2` entries in each IFD, combining count-`6` out-of-line `Hello\0`
with count-`4` inline `Ada\0`; facade seek states preserve both GetExif values
and exact pixels. Raw/facade REDs found a missing entry-array export and the
singular facade restriction. Final raw/facade passed `1/1` in `141ms` and
`1/1` in `16ms`; TIFF `278/278` in `1219ms`, save_all `50/50` in `360ms`, and
full `1948/1948` in `9250ms`. Registrations are `958/990`; Release x64 built
cleanly; exports/DLL are `399/399` and
`DB5C38C111E322C1BDBB66B9643C2E85CF54C1A3BC991BA5D1AFDF98CBA4291A`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001P` adds bounded two-frame RGB
Artist save_all through `tiffinfo={315: "Ada"}`. Native writes TIFF ASCII type
`2`, count `4`, and exact inline bytes `41 64 61 00` in both IFDs; both facade
seek states preserve exact pixels and expose `GetExif()[315] == "Ada"`.
Raw/facade REDs found native and facade tag rejection. Final raw/facade passed
`1/1` in `172ms` and `1/1` in `16ms`; TIFF `276/276` in `1078ms`, save_all
`49/49` in `343ms`, and full `1946/1946` in `9250ms`. Registrations are
`957/989`; Release x64 built cleanly; exports/DLL remain `398/398` and
`D0B49F66FA232749D7E22AAB39D2CEED838D7B48AF01D4E890A1F1F4A2401B0F`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001O` adds two-frame RGB
ImageDescription save_all through `tiffinfo={270: "Hello"}`. Native writes
TIFF ASCII type `2`, count `6`, and exact `Hello\0` bytes in both IFDs; both
facade seek states preserve exact pixels and expose `GetExif()[270] ==
"Hello"`. Raw/facade REDs found the missing ASCII export and tag rejection.
Final raw/facade passed `1/1` in `140ms` and `1/1` in `16ms`; TIFF `274/274`
in `1250ms`, save_all `48/48` in `265ms`, and full `1944/1944` in `9453ms`.
Registrations are `956/988`; Release x64 built cleanly; exports/DLL are
`398/398` and
`95E388362A7930CDC95CA49B03C7F4867DB0AE983D32803D322D9601005E9C85`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001N` proves two-frame RGB explicit
ICC plus tiffinfo XMP composition. Both IFDs and facade seek states preserve
ICC UNDEFINED `34675`, XMP BYTE `700`, exact payloads, and decoded bytes.
Existing raw/facade routes passed directly `1/1` in `94ms` and `1/1` in
`47ms`; TIFF `272/272` in `1109ms`, save_all `47/47` in `282ms`, and full
`1942/1942` in `8765ms`. Registrations are `955/987`; no rebuild;
exports/DLL remain `397/397` and
`D2F771BCFDD56D6EB70993FCC503DD60E77350CCFC1EC61A998D80CD6DF50EBB`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001M` adds two-frame RGB XMP via
Pillow's actual TIFF save surface, `tiffinfo={700: packet}`; direct TIFF
`xmp=` is confirmed ignored. Native writes exact 324-byte BYTE tag `700`
payloads in both IFDs and exposes Info/GetExif after both seeks. Raw/facade
REDs failed on the missing extended metadata export and ignored tiffinfo.
Final raw/facade passed `1/1` in `156ms` and `1/1` in `47ms`; TIFF `270/270`
in `1172ms`, save_all `46/46` in `266ms`, and full `1940/1940` in `9468ms`.
Registrations are `954/986`; Release x64 built cleanly; exports/DLL are
`397/397` and
`D2F771BCFDD56D6EB70993FCC503DD60E77350CCFC1EC61A998D80CD6DF50EBB`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001L` adds explicit two-frame RGB
ICC save_all metadata. Pillow writes the exact 8-byte payload, including NUL,
as UNDEFINED tag `34675` in both IFDs and exposes identical Info/GetExif bytes
after both seeks. Raw/facade REDs failed on a missing metadata export and the
ignored public option. Native now owns payload validation, IFD layout, bytes,
and lifetime; facade normalizes only bytes/options. Final raw/facade passed
`1/1` in `141ms` and `1/1` in `47ms`; TIFF `268/268` in `1140ms`, save_all
`45/45` in `281ms`, and full `1938/1938` in `9078ms`. Registrations are
`953/985`; Release x64 built cleanly; exports/DLL are `396/396` and
`684BF0C50053C043CD3127C22C0772F907C94F440589C429D6A7BAE14DC25D0C`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001K` batches two-frame RGB LZW and
Adobe Deflate with `dpi=(300,150)`. Both compression tags, per-frame
X/YResolution and ResolutionUnit, decoded bytes, and facade DPI/GetExif match
Pillow. LZW strips are 14 bytes in both; native Deflate's documented valid
stored blocks remain 23 bytes versus Pillow's 20, both with `78 9C`. Existing
raw/facade routes passed `1/1` in `78ms` and `1/1` in `63ms`; TIFF `266/266`
in `953ms`, save_all `44/44` in `218ms`, and full `1936/1936` in `8734ms`.
Registrations are `952/984`; exports/DLL remain `395/395` and
`F347533B35A6E6AE3243247AEF5639F7C24CDBB8CDF5062987ABB61A96C7832F`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001J` composes two-frame RGB
PackBits with `dpi=(300,150)`. Pillow and native write Compression `32773`,
14-byte strips, XResolution `300/1`, YResolution `150/1`, and ResolutionUnit
`2` in both IFDs while preserving exact decoded bytes and facade DPI/GetExif.
The generalized options ABI and facade route passed directly without a
production change. Raw/facade passed `1/1` in `78ms` and `1/1` in `31ms`;
TIFF `264/264` in `1204ms`, save_all `43/43` in `250ms`, and full `1934/1934`
in `9140ms`. Registrations are `951/983`; exports/DLL remain `395/395` and
`F347533B35A6E6AE3243247AEF5639F7C24CDBB8CDF5062987ABB61A96C7832F`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001I` composes uncompressed RGB
two-frame save_all with `dpi=(300,150)`. Pillow and native write exact
XResolution `300/1`, YResolution `150/1`, and ResolutionUnit `2` in both IFDs,
preserve exact decoded bytes, and expose DPI/GetExif after both facade seeks.
Raw RED found the missing generalized multiframe options export; facade RED
showed the old route ignored DPI. Final raw/facade passed `1/1` in `156ms`
and `1/1` in `47ms`; TIFF `262/262` in `1093ms`, save_all `42/42` in `219ms`,
and full `1932/1932` in `9156ms`. Registrations are `950/982`; Release x64
builds with zero warnings/errors; exports/DLL are `395/395` and
`F347533B35A6E6AE3243247AEF5639F7C24CDBB8CDF5062987ABB61A96C7832F`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001H` batches RGB two-frame LZW and
Adobe Deflate save_all through the FMT-TIFF-001G ABI. Pillow LZW uses
Compression `5` and 14-byte strips; Adobe Deflate uses Compression `8`, zlib
`78 9C`, and 20-byte strips. Existing native/facade routes preserve exact
decoded bytes; native Deflate's valid stored-block strategy uses 23 bytes, an
explicit compressed-representation difference outside this composition gap.
Raw/facade passed `1/1` in `94ms` and `1/1` in `31ms`; TIFF `260/260` in
`1125ms`, save_all `41/41` in `281ms`, and full `1930/1930` in `9094ms`.
Registrations are `949/981`; exports/DLL remain `394/394` and
`2C5F6E6929FB0900F8E70D2FF5F2E6095A6D242ACCAE395B6318F7D5A1FFF29F`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001G` composes RGB two-frame TIFF
save_all with PackBits. Pillow 11.3.0 writes Compression `32773`,
RowsPerStrip `2`, StripByteCounts `14`, and exact decoded bytes for both
frames. Raw RED failed because the multiframe compression export did not
exist; facade RED wrote Compression `1`. A new coarse native export reuses
existing compression normalization and frame-vector encoding, while facade
routes the option. Final raw/facade passed `1/1` in `93ms` and `1/1` in
`31ms`; TIFF passed `258/258` in `921ms`, save_all `40/40` in `187ms`, and
full `1928/1928` in `8938ms`. Registrations are `948/980`; Release x64 rebuilt
cleanly; exports/DLL are `394/394` and
`2C5F6E6929FB0900F8E70D2FF5F2E6095A6D242ACCAE395B6318F7D5A1FFF29F`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001F` covers one same-size RGB
three-frame TIFF. Pillow 11.3.0 preserves all three exact frame byte arrays;
the IFD chain has two nonzero links and a final zero terminator. Existing
native frame-vector layout and facade two-element append array matched without
production changes. Combined raw/facade passed `2/2` in `47ms`, TIFF
`256/256` in `1062ms`, save_all `39/39` in `188ms`, and full `1926/1926` in
`9250ms`. Registrations are `947/979`; exports/DLL remain `393/393` and
`2189246D2C8C390C236DD4595A909BF72084DF8448C430A1216FC095A2D3A06A`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001E` covers one RGB mixed-size
two-frame TIFF. Pillow 11.3.0 preserves `2x1` then `1x2` sizes and exact RGB
bytes; the second IFD writes Width `1`, Height/RowsPerStrip `2`, and a six-byte
strip. Existing native per-frame layout and facade seek-size refresh matched
without production changes. Combined raw/facade passed `2/2` in `31ms`, TIFF
`254/254` in `1078ms`, save_all `38/38` in `188ms`, and full `1924/1924` in
`8907ms`. Registrations are `946/978`; exports/DLL remain `393/393` and
`2189246D2C8C390C236DD4595A909BF72084DF8448C430A1216FC095A2D3A06A`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001D` covers one same-size mixed
L/RGB two-frame TIFF. Pillow 11.3.0 preserves L then RGB modes and exact bytes;
its L IFD omits SamplesPerPixel and writes PlanarConfiguration `1`, while the
RGB IFD carries `(8,8,8)`, photometric `2`, three samples, and a six-byte
strip. Raw RED failed `0/1` because native L output still included tag `277`;
the same native condition also omitted tag `284`. Layout counting and entry
emission now match Pillow. Final raw/facade passed `1/1` each, combined `2/2`
in `62ms`, TIFF `252/252` in `1000ms`, save_all `37/37` in `172ms`, and full
`1922/1922` in `8891ms`. Registrations are `945/977`; Release x64 rebuilt with
zero warnings/errors; exports/DLL remain `393/393` and
`2189246D2C8C390C236DD4595A909BF72084DF8448C430A1216FC095A2D3A06A`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001C` covers same-size RGBA
two-frame TIFF `save_all` / `append_images`. Pillow 11.3.0 emits two
little-endian uncompressed RGBA IFDs with `(8,8,8,8)` bits, photometric `2`,
four samples, contiguous planar layout, ExtraSamples `2`, one eight-byte strip
per frame, and exact interleaved bytes after both seeks. Existing native and
facade routes matched without production changes. Combined raw/facade passed
`2/2` in `46ms`, TIFF `250/250` in `1079ms`, save_all `36/36` in `188ms`, and
full `1920/1920` in `8953ms`. Registrations are `944/976`; exports/DLL remain
`393/393` and
`BE715A737810DC3D43799A6775D4E629D3B0AB1582F7F6E06A22CF18C3B20EE3`.

Incremental refresh, 2026-07-19: `FMT-TIFF-001B` covers same-size RGB
two-frame TIFF `save_all` / `append_images`. Pillow 11.3.0 emits two
little-endian uncompressed RGB IFDs with `(8,8,8)` bits, photometric `2`,
three samples, contiguous planar layout, one six-byte strip per frame, and
exact interleaved bytes after both seeks. The existing native multipage writer
and facade route matched without production changes. Final raw/facade passed
`1/1` each; combined target passed `2/2` in `32ms`, TIFF `248/248` in
`1047ms`, save_all `35/35` in `172ms`, and full `1918/1918` in `9188ms`.
Registrations are `943/975`; exports/DLL remain `393/393` and
`BE715A737810DC3D43799A6775D4E629D3B0AB1582F7F6E06A22CF18C3B20EE3`.

Incremental refresh, 2026-07-19: `FMT-JPEG-003AZ` covers duplicate ordinary
Photoshop resource-code precedence. Pillow 11.3.0 exposes one `1028: b"CD"`
entry for ordered duplicate values `AB`, `CD`, then drops APP13 from both keep
saves while preserving DQT/RGB/size. Raw RED failed `0/1` in `47ms` with
count `2`; native parser state now keeps the first key position and replaces
its bytes with the last value. Final raw/facade passed `1/1` in `93ms` and
`1/1` in `46ms`. Duplicate-Photoshop/Photoshop/APP13/open_jpeg/quality-keep/
qtables-keep/JPEG passed `2/2`, `10/10`, `6/6`, `29/29`, `57/57`, `76/76`,
`399/399`; full `1916/1916` passed in `8578ms`. Registrations are `942/974`;
Release x64 rebuilt cleanly; exports/DLL remain `393/393` and
`BE715A737810DC3D43799A6775D4E629D3B0AB1582F7F6E06A22CF18C3B20EE3`.

Incremental refresh, 2026-07-19: `FMT-JPEG-003AY` covers multiple recognized
Photoshop APP13 markers. Pillow 11.3.0 merges ordinary `1028: b"AB"` from the
first and structured ResolutionInfo `1005: {...}` from the second into one
map, then drops all APP13 markers on both keep saves while preserving DQT/RGB/
size. Existing native accumulation and facade composition passed raw/facade
`1/1` in `46ms` and `1/1` in `31ms` without production changes. Photoshop/
APP13/open_jpeg/quality-keep/qtables-keep/JPEG passed `8/8`, `6/6`, `28/28`,
`56/56`, `75/75`, `397/397`; full `1914/1914` passed in `8844ms`.
Registrations are `941/973`; exports/DLL remain `393/393` and
`114E2F544DA1848E35D828EEC5246075C11753945B440A57ADC26360D76510B6`.

Incremental refresh, 2026-07-19: `FMT-JPEG-003AX` covers mixed Photoshop
resource composition. Pillow 11.3.0 exposes ordinary `1028: b"AB"` and
structured ResolutionInfo `1005: {...}` in the same `info["photoshop"]` map,
then drops APP13 from both keep saves while preserving DQT/RGB/size. Existing
native ordinary/structured routes and facade composition passed raw/facade
`1/1` in `62ms` and `1/1` in `47ms` without production changes.
ResolutionInfo/Photoshop/open_jpeg/quality-keep/qtables-keep/JPEG passed `4/4`,
`6/6`, `27/27`, `55/55`, `74/74`, `395/395`; full `1912/1912` passed in
`8765ms`. Registrations are `940/972`; exports/DLL remain `393/393` and
`114E2F544DA1848E35D828EEC5246075C11753945B440A57ADC26360D76510B6`.

Incremental refresh, 2026-07-19: `FMT-JPEG-003AW` covers Photoshop
ResolutionInfo resource `0x03ED`. Pillow 11.3.0 exposes a nested dictionary
with resolutions `300.5`/`150.25` as floats and displayed units `1`/`3` as
integers, then drops APP13 from both keep saves while preserving DQT/RGB/size.
Native state decodes the 16.16 fields, preserves metadata-copy lifetime, and
exposes one structured scalar ABI; the facade composes the nested Map. Raw/
facade REDs failed `0/1` in `47ms` and `0/1` in `32ms`; final raw/facade
passed `1/1` in `94ms` and `1/1` in `31ms`. ResolutionInfo/Photoshop/APP13/
open_jpeg/quality-keep/qtables-keep/JPEG passed `2/2`, `4/4`, `4/4`, `26/26`,
`54/54`, `73/73`, `393/393`; full `1910/1910` passed in `9078ms`.
Registrations are `939/971`; exports/DLL are `393/393` and
`114E2F544DA1848E35D828EEC5246075C11753945B440A57ADC26360D76510B6`.

Incremental refresh, 2026-07-19: `FMT-JPEG-003AV` covers one ordinary
Photoshop APP13 `8BIM` resource. Pillow 11.3.0 exposes
`info["photoshop"] == {1028: b"AB"}` and drops APP13 on both keep saves.
Native parsing/state plus two enumerable exports now own the resource map; the
facade materializes `Map(1028 => Buffer("AB"))`. Raw red failed `0/1` in
`31ms`; facade red failed `0/1` in `31ms`; final raw/facade passed `1/1` in
`93ms` and `1/1` in `31ms`. Photoshop/APP13/open_jpeg/quality-keep/qtables-
keep/JPEG passed `2/2`, `4/4`, `25/25`, `53/53`, `72/72`, `391/391`; full
`1908/1908` passed in `8531ms`. Registrations are `938/970`; exports/DLL are
`392/392` and
`1C0EB831B28942B7A9579B655DA77AAD258AF5CFE694CDBBE65D426A991F19FD`.

Incremental refresh, 2026-07-19: `FMT-JPEG-003AU` covers unknown pre-DQT
APP13 keep-save disposition. Pillow 11.3.0 exposes the source marker in
`applist` but omits it from both `quality="keep"` and `qtables="keep"` outputs
while preserving DQT, RGB mode, and `16x8` size; existing native/facade routes
match. Raw/facade passed `1/1` in `62ms` and `1/1` in `31ms`; no production
change or rebuild. APP13/quality-keep/qtables-keep/JPEG passed `2/2`, `52/52`,
`71/71`, `389/389`; full `1906/1906` passed in `9063ms`. Registrations are
`937/969`; exports/DLL remain `390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Incremental refresh, 2026-07-19: `META-002AE` covers the JPEG ICC multi-marker
count-mismatch None matrix. Pillow 11.3.0 keeps the ICC key and assigns `None`
for `1/0:A + 2/0:B` and `1/255:A + 2/255:B`; existing native finalization
returns state `2` with no byte blob, and the facade maps both cases to `""`.
Raw/facade passed `1/1` in `63ms` and `1/1` in `32ms`; no production change or
rebuild. ICC/open_jpeg/JPEG passed `91/91`, `23/23`, `387/387`; full
`1904/1904` passed in `9250ms`. Registrations are `936/968`; exports/DLL
remain `390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Incremental refresh, 2026-07-19: `META-002AD` covers the JPEG ICC singleton
count-mismatch None matrix. Pillow 11.3.0 keeps the ICC key and assigns `None`
for `1/0:A` / `1/255:B`; existing native finalization returns state `2` with
no byte blob, and the facade maps both cases to `""`. Raw/facade passed `1/1`
in `62ms` and `1/1` in `32ms`; no production change or rebuild. ICC/open_jpeg/
JPEG passed `89/89`, `22/22`, `385/385`; full `1902/1902` passed in `8578ms`.
Registrations are `935/967`; exports/DLL remain `390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Incremental refresh, 2026-07-19: `META-002AC` covers the JPEG ICC high
out-of-range singleton sequence matrix. Pillow 11.3.0 exposes `A` / `B` for
`2/1:A` / `255/1:B`; existing native count-based finalization returns state
`1` and matching bytes, and the facade exposes matching Buffers. Raw/facade
passed `1/1` in `62ms` and `1/1` in `31ms`; no production change or rebuild.
ICC/open_jpeg/JPEG passed `87/87`, `21/21`, `383/383`; full `1900/1900`
passed in `8437ms`. Registrations are `934/966`; exports/DLL remain `390/390`
and `5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Incremental refresh, 2026-07-19: `META-002AB` covers JPEG ICC zero-sequence
permissive collation. Pillow 11.3.0 opens singleton `0/1:A` as RGB `2x1` and
exposes exact `icc_profile=b"A"`; existing native sorting/count finalization
returns state `1` and byte `[65]`, and the facade exposes the matching Buffer.
Raw/facade passed `1/1` in `47ms` and `1/1` in `32ms`; no production change or
rebuild. ICC/open_jpeg/JPEG passed `85/85`, `20/20`, `381/381`; full
`1898/1898` passed in `9062ms`. Registrations are `933/965`; exports/DLL remain
`390/390` and
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Incremental refresh, 2026-07-19: `META-002AA` covers JPEG ICC missing-middle
fragment `None` state. Pillow 11.3.0 opens RGB `2x1`, keeps the
`icc_profile` key, and assigns Python `None`; native metadata now preserves
states `0/1/2` through a new read-only export, while the facade maps state `2`
to `""`. Raw/facade REDs were `0/1` in `78ms` and `0/1` in `16ms`; final
raw/facade passed `1/1` in `31ms` and `1/1` in `31ms`. ICC/open_jpeg/JPEG
passed `83/83`, `19/19`, `379/379`; full `1896/1896` passed in `8828ms`.
Release x64 rebuilt cleanly; registrations are `932/964`, exports `390/390`,
and DLL SHA-256 is
`5E8064F81E95ED78E80E47318CD99B0BFC066ABBA65F79E8B485AB3CC1BFB61B`.

Incremental refresh, 2026-07-19: `META-002Z` covers JPEG ICC middle-zero
fragment collation. Pillow 11.3.0 opens the bounded RGB `2x1` fixture carrying
`1/3:A`, `2/3:empty`, `3/3:B` and exposes exact public ICC bytes `AB`; the
existing DLL deferred finalizer and facade metadata route match without
production changes. Raw/facade passed `1/1` in `62ms` and `1/1` in `63ms`;
ICC/open_jpeg/JPEG passed `81/81`, `18/18`, `377/377`; full `1894/1894`
passed in `15640ms`. Registrations are `931/963`; exports/DLL remain `389/389`
and `E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Incremental refresh, 2026-07-19: `MODE-NUM-001CG` covers border-unset numeric
Floodfill above-neighbor-distance fill-all traversal. Threshold `8.0` remains
below initial distances `16` / `8.75` but above zero-neighbor distances `7.0`
/ `7.25`, so the DLL writes the seed, admits the zero neighbor, and fills all
three I/F samples while preserving identity, mode, size, and exact storage.
Raw/facade passed `1/1` in `1453ms` and `1/1` in `79ms`; no production change
or rebuild. Gates passed `2/2`, `14/14`, `60/60`, `75/75`, `96/96`; full
`1892/1892` passed in `15375ms`. Exports/DLL remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Incremental refresh, 2026-07-19: `MODE-NUM-001CF` covers border-unset numeric
Floodfill below-neighbor-distance seed-only traversal. Initial distances `16`
/ `8.75` exceed thresholds `6.0` / `6.25`, so the seed mutates; the zero
neighbor's distance `7.0` / `7.25` exceeds the threshold and is rejected,
leaving exact seed-only I/F storage while preserving identity, mode, and size.
Raw/facade passed `1/1` in `703ms` and `1/1` in `47ms`; no production change
or rebuild. Gates passed `2/2`, `14/14`, `60/60`, `75/75`, `96/96`; full
`1892/1892` passed in `8578ms`. Exports/DLL remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Incremental refresh, 2026-07-19: `MODE-NUM-001CE` covers border-unset numeric
Floodfill neighbor-distance equality traversal. Initial distances `16` /
`8.75` exceed thresholds `7.0` / `7.25`, so the seed mutates; the zero
neighbor's distance equals the threshold and native `<=` admission fills all
three I/F samples while preserving image/data-pointer identity, mode, size,
and exact storage. Raw/facade passed `1/1` in `656ms` and `1/1` in `46ms`;
no production change or rebuild. Gates passed `2/2`, `14/14`, `60/60`,
`75/75`, `96/96`; full `1892/1892` passed in `9015ms`. Exports/DLL remain
`389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Incremental refresh, 2026-07-19: `MODE-NUM-001CD` covers nonmatching scalar-
border plus above-initial-distance numeric Floodfill no-op precedence. The
corrected native scalar distances `16` / `8.75` are below thresholds `17.0` /
`9.75`, so the DLL returns before mutation or supplied-border traversal while
facade identity, mode, size, and exact source bytes remain stable. Raw/facade
passed `1/1` in `766ms` and `1/1` in `47ms`; no additional production change
or rebuild. Gates passed `2/2`, `14/14`, `60/60`, `75/75`, `96/96`; full
`1892/1892` passed in `9062ms`. Exports/DLL remain `389/389` and
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Incremental refresh, 2026-07-19: `MODE-NUM-001CC` closes numeric Floodfill
threshold-equality no-op precedence. The raw proof first failed because native
I/F distance summed four storage-byte differences; local Pillow 11.3.0 proved
that exact thresholds I `16.0` / F `8.75` leave source bytes unchanged. A
mode-aware DLL helper now decodes signed-int32/float32 scalar samples for both
initial and border-unset neighbor comparisons. Release x64 rebuilt cleanly;
raw/facade passed `1/1` in `578ms` and `1/1` in `47ms`; gates passed `2/2`,
`14/14`, `60/60`, `75/75`, `96/96`; full `1892/1892` passed in `9125ms`.
Exports remain `389/389`; DLL SHA-256 is
`E1221A50F809492D95507EEC57D02E4ACB6A931FC7986467D4D5131D87819108`.

Incremental refresh, 2026-07-19: `MODE-NUM-001CB` covers nonmatching scalar-
border plus finite positive-below-distance numeric Floodfill traversal. The
existing native packed-scalar route fills all three I/F samples because
distances `16` / `8.75` exceed threshold `1.0` and no source sample equals the
supplied border, preserving image/data-pointer identity, mode, size, and exact
storage. Raw and facade proofs passed `1/1` in `547ms` and `1/1` in `47ms`;
no production change or rebuild. Gates passed `2/2`, `14/14`, `60/60`,
`75/75`, and `96/96`; full `1892/1892` passed in `9047ms`. Registrations
`930/962`, exports `389/389`; DLL remains SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001CA` covers nonmatching scalar-
border plus zero-threshold numeric Floodfill fill-all traversal. The existing
native packed-scalar route fills all three I/F samples because the initial
distances `16` / `8.75` exceed `0.0` and no source sample equals the supplied
border, preserving image/data-pointer identity, mode, size, and exact storage.
Raw and facade proofs passed `1/1` in `547ms` and `1/1` in `47ms`; no
production change or rebuild. Gates passed `2/2`, `14/14`, `60/60`, `75/75`,
and `96/96`; full `1892/1892` passed in `8672ms`. Registrations `930/962`,
exports `389/389`; DLL remains SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BZ` covers nonmatching scalar-
border plus finite negative-threshold numeric Floodfill fill-all traversal.
The existing native packed-scalar route fills all three I/F samples after the
initial ordered comparison against `-1.0` is false, preserving image/data-
pointer identity, mode, size, and exact storage. Raw and facade proofs passed
`1/1` in `578ms` and `1/1` in `47ms`; no production change or rebuild. Gates
passed `2/2`, `14/14`, `60/60`, `75/75`, and `96/96`; full `1892/1892` passed
in `8516ms`. Registrations `930/962`, exports `389/389`; DLL remains SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BY` covers nonmatching scalar-
border plus positive-infinity numeric Floodfill no-op precedence. Pillow and
the existing native route return before mutation or supplied-border traversal,
preserving image/data-pointer identity, mode, size, and exact source storage.
Raw and facade proofs passed `1/1` in `485ms` and `1/1` in `31ms`; no
production change or rebuild. Gates passed `2/2`, `14/14`, `60/60`, `75/75`,
and `96/96`; full `1892/1892` passed in `8657ms`. Registrations `930/962`,
exports `389/389`; DLL remains SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BX` covers nonmatching scalar-
border plus quiet-NaN numeric Floodfill fill-all traversal. Pillow and the
existing native packed-scalar route fill all three I/F samples while preserving
image/data-pointer identity, mode, size, and exact storage. Raw and facade
proofs passed `1/1` in `438ms` and `1/1` in `31ms`; no production change or
rebuild. Gates passed `2/2`, `14/14`, `60/60`, `75/75`, and `96/96`; full
`1892/1892` passed in `9187ms`. Registrations `930/962`, exports `389/389`;
DLL remains SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BW` covers nonmatching scalar-
border plus negative-infinity numeric Floodfill fill-all traversal. Pillow and
the existing native packed-scalar route fill all three I/F samples while
preserving image/data-pointer identity, mode, size, and exact storage. Raw and
facade proofs passed `1/1` in `515ms` and `1/1` in `47ms`; no production change
or rebuild. Gates passed `2/2`, `14/14`, `60/60`, `75/75`, and `96/96`; full
`1892/1892` passed in `8656ms`. Registrations `930/962`, exports `389/389`;
DLL remains SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BV` covers empty Array-border
plus negative-infinity numeric Floodfill traversal and completes the Array-
shape matrix at this threshold. Pillow empty tuple/list borders and the shared
native sentinel fill all three I/F samples while preserving image/data-pointer
identity and exact storage. The reused raw proof and extended facade proof
passed `1/1` in `344ms` and `1/1` in `31ms`; no production change or rebuild.
Full `1892/1892` passed in `8516ms`; registrations `930/962`, exports
`389/389`; DLL remains SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BU` covers one-element
Array-border plus negative-infinity numeric Floodfill traversal. Pillow
one-element tuple/list borders and the shared native sentinel fill all three
I/F samples while preserving image/data-pointer identity and exact storage.
The reused raw proof and extended facade proof passed `1/1` in `484ms` and
`1/1` in `46ms`; no production change or rebuild. Full `1892/1892` passed in
`8672ms`; registrations `930/962`, exports `389/389`; DLL remains SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BT` covers multi-element
Array-border plus negative-infinity numeric Floodfill traversal. Pillow
tuple/list borders and the existing native supplied-incomparable-border route
fill all three I/F samples while preserving allocation and exact storage.
Raw/facade passed `1/1` in `375ms` and `1/1` in `31ms`; no production change
or rebuild. Full `1892/1892` passed in `8546ms`; registrations `930/962`,
exports `389/389`; DLL remains SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BS` covers scalar-border plus
negative-infinity seed-only Floodfill precedence. Existing raw/facade/native
routes passed without production changes; full `1892/1892` in `8718ms`,
exports `389/389`, DLL hash unchanged.

Incremental refresh, 2026-07-19: `MODE-NUM-001BR` covers numeric
`ImageDraw.Floodfill` empty Array-border plus positive-infinity no-op
precedence, completing the Array-shape matrix over one native sentinel. Pillow
tuple/list borders and the shared route return before mutation or traversal,
preserving allocation and exact I/F bytes. The reused raw proof and extended
facade proof passed without production changes. Full `1892/1892` in `8781ms`;
registrations `930/962`, exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BQ` covers numeric
`ImageDraw.Floodfill` one-element Array-border plus positive-infinity no-op
precedence. Pillow tuple/list borders and the shared sentinel route return
before seed mutation or traversal, preserving allocation and exact I/F bytes.
The reused raw proof and extended facade proof passed without production
changes. Full `1892/1892` in `8922ms`; registrations `930/962`, exports
`389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BP` covers numeric
`ImageDraw.Floodfill` multi-element Array-border plus positive-infinity no-op
precedence. Pillow tuple/list borders, the shared sentinel, and the native
route return before seed mutation or sentinel traversal, preserving allocation
and exact I/F bytes. Raw/facade proofs passed immediately without production
changes. Full `1892/1892` in `8609ms`; registrations `930/962`, exports
`389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BO` covers numeric
`ImageDraw.Floodfill` matching scalar-border plus positive-infinity no-op
precedence. Pillow and the existing native route return before seed mutation
or supplied-border traversal, preserving allocation and exact I/F bytes. Raw/
facade proofs passed immediately without production changes. Full `1892/1892`
in `9078ms`; registrations `930/962`, exports `389/389`; DLL unchanged at
SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BN` covers numeric
`ImageDraw.Floodfill` empty Array-border plus quiet-NaN traversal, completing
the empty/one/multi-element Array-shape matrix over one native sentinel.
Pillow tuple/list borders and the existing route fill all I/F samples while
preserving allocation and exact bytes. Reused raw/extended facade proofs
passed without production changes. Full `1892/1892` in `9000ms`;
registrations `930/962`, exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-19: `MODE-NUM-001BM` covers numeric
`ImageDraw.Floodfill` one-element Array-border plus quiet-NaN traversal.
Pillow tuple/list borders and the existing shared sentinel route fill all I/F
samples while preserving allocation and exact bytes. The reused raw proof and
extended facade proof passed immediately without production changes. Full
`1892/1892` in `8735ms`; registrations `930/962`, exports `389/389`; DLL
unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BL` covers numeric
`ImageDraw.Floodfill` multi-element Array-border plus quiet-NaN traversal.
Pillow tuple/list borders and the DLL fill all three I/F samples because the
initial NaN comparison is false and scalar samples remain unequal to the non-
scalar/sentinel border, preserving allocation and exact bytes. Extended raw/
facade proofs passed immediately without production changes. Full `1892/1892`
in `8594ms`; registrations `930/962`, exports `389/389`; DLL unchanged at
SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BK` covers numeric
`ImageDraw.Floodfill` matching scalar-border plus quiet-NaN threshold
precedence. Pillow and the DLL write only the I/F seed: the initial NaN
comparison is false, then the matching zero border stops traversal while
preserving allocation and exact remaining bytes. Extended raw/facade proofs
passed immediately without production changes. Full `1892/1892` in `8594ms`;
registrations `930/962`, exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BJ` covers numeric
`ImageDraw.Floodfill` border-unset quiet-NaN threshold seed-only semantics.
Pillow and the DLL write only the I/F seed because every ordered comparison
against NaN is false, preserving allocation and exact remaining bytes.
Extended raw/facade proofs passed immediately without production changes. Full
`1892/1892` in `8875ms`; registrations `930/962`, exports `389/389`; DLL
unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BI` covers numeric
`ImageDraw.Floodfill` border-unset negative-infinity threshold seed-only
semantics. Pillow and the DLL write only the I/F seed because no finite
distance is at most negative infinity, preserving allocation and exact
remaining bytes. Extended raw/facade proofs passed immediately without
production changes. Full `1892/1892` in `8688ms`; registrations `930/962`,
exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BH` covers numeric
`ImageDraw.Floodfill` border-unset positive-infinity threshold no-op semantics.
Pillow and the DLL return before seed mutation while preserving I/F identity
and exact bytes. Extended and renamed raw/facade proofs passed immediately
without production changes. Full `1892/1892` in `8437ms`; registrations
`930/962`, exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BG` covers numeric
`ImageDraw.Floodfill` empty Array-border composition at threshold `-1.0`.
Pillow fills all three I/F samples because empty tuple/list borders remain
incomparable to scalar samples. The reused raw and extended facade proofs
passed immediately without production changes. Full `1892/1892` in `15875ms`;
registrations `930/962`, exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BF` covers numeric
`ImageDraw.Floodfill` one-element Array-border composition at threshold
`-1.0`. Pillow fills all three I/F samples because `(0,)` / `(0.0,)` remain
incomparable to scalar samples. The reused raw and extended facade proofs
passed immediately without production changes. Full `1892/1892` in `15656ms`;
registrations `930/962`, exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BE` covers numeric
`ImageDraw.Floodfill` nonempty multi-element Array-border composition at
threshold `-1.0`. Pillow and the native DLL fill all three I/F samples because
the supplied incomparable border selects border-only neighbor admission.
Extended raw/facade proofs passed immediately without production changes.
Full `1892/1892` in `8484ms`; registrations `930/962`, exports `389/389`;
DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BD` covers numeric
`ImageDraw.Floodfill` scalar-border composition at threshold `-1.0`. Pillow
and the native DLL write only the I/F seed because the supplied matching zero
border stops traversal. Extended raw/facade proofs passed immediately without
production changes. Full `1892/1892` in `8922ms`; registrations `930/962`,
exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BC` covers numeric
`ImageDraw.Floodfill` border-unset threshold `-1.0` seed-only semantics.
Pillow and the native DLL write only the I/F seed while preserving allocation
and exact remaining storage. Extended raw/facade proofs passed immediately,
without production changes. Full `1892/1892` in `8484ms`; registrations
`930/962`, exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BB` closes numeric
`ImageDraw.Floodfill` empty-value precedence over invalid string threshold
`"bad"`. Pillow reaches the caught empty-tuple `IndexError` before threshold
comparison; empty lists retain subtraction errors. Facade RED exposed eager
threshold validation, then GREEN passed after empty values were detected first
and dispatched with native threshold `0.0`. Full `1892/1892` in `9266ms`;
registrations `930/962`, exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001BA` closes numeric
`ImageDraw.Floodfill` empty-value precedence over one invalid string border.
Pillow ignores `"bad"` after the empty tuple triggers its caught `IndexError`;
empty lists retain subtraction errors. The raw ABI already ignored invalid
border lengths, while facade RED exposed eager color parsing. Skipping border
normalization only for empty numeric values made GREEN pass without a native
change. Full `1892/1892` in `9140ms`; registrations `930/962`, exports
`389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001AZ` covers numeric
`ImageDraw.Floodfill` empty-value precedence with empty tuple/list or AHK Array
borders. Pillow keeps empty I/F tuples as allocation-preserving no-ops and
empty lists retain subtraction errors. The reused raw dual-sentinel and
extended facade route passed without production changes. Full `1892/1892` in
`8532ms`; registrations `930/962`, exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001AY` covers numeric
`ImageDraw.Floodfill` empty-value precedence with one-element tuple/list or AHK
Array borders. Pillow keeps empty I/F tuples as allocation-preserving no-ops
and empty lists retain subtraction errors. The reused raw dual-sentinel and
extended facade route passed without production changes. Full `1892/1892` in
`8594ms`; registrations `930/962`, exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001AX` covers numeric
`ImageDraw.Floodfill` empty-value precedence with one nonempty multi-element
tuple/list or AHK Array border. Pillow keeps empty I/F tuples as allocation-
preserving no-ops and empty lists retain subtraction errors. The existing
native dual-sentinel composition and facade lifetime route passed without
production changes. Full `1892/1892` in `10344ms`; registrations `930/962`,
exports `389/389`; DLL unchanged at SHA-256
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001AW` extends numeric
`ImageDraw.Floodfill` empty-value no-op semantics across one valid scalar
border. Pillow evaluates `_color_diff` before its border branch, so empty I/F
tuples still become caught `IndexError` no-ops while empty lists retain
subtraction errors. The native sentinel and facade route are now border-
independent for the proven shape. Full `1892/1892` in `15719ms`;
registrations `930/962`, exports `389/389`; rebuilt DLL SHA-256 is
`8C0986528F58B76E44E104FA43077DF2C7CBDE618079E385A68990F39A595B97`.

Incremental refresh, 2026-07-18: `MODE-NUM-001AV` extends numeric
`ImageDraw.Floodfill` empty-value no-op semantics to a positive threshold.
Pillow's empty tuple raises `IndexError` inside `_color_diff`, caught before
threshold use; empty lists retain subtraction errors. AU's native sentinel and
facade route are now threshold-independent for absent-border calls. Full
`1892/1892` in `24563ms`; registrations `930/962`, exports `389/389`;
rebuilt DLL SHA-256 is
`EABD8291824F88BCAAAD0D92C277500DD6F488C8DFD55DBA62CB1AE57623C9E3`.

Incremental refresh, 2026-07-18: `MODE-NUM-001AU` closes numeric
`ImageDraw.Floodfill` empty value-sequence semantics at threshold zero. Pillow
treats empty I/F tuples as successful allocation-preserving no-ops while empty
lists raise subtraction errors. The native ABI now owns an explicit non-null/
zero-size value sentinel, and AHK only routes the bounded empty Array shape.
Full `1892/1892` in `20906ms`; registrations `930/962`, exports `389/389`;
rebuilt DLL SHA-256 is
`AE26A1E30AB59D5A28D498C17A1D55F0E36FB428C5C32CF7AC710E485BFC2F45`.

Incremental refresh, 2026-07-18: `MODE-NUM-001AT` closes numeric
`ImageDraw.Floodfill` one-element value-sequence packing. Pillow accepts
one-element I/F tuples as scalar colors but one-element lists reach subtraction
errors; AHK Array follows the tuple analogue. The existing AO length gate,
numeric `ColorBuffer`, and native Floodfill path passed immediately without
production changes. Full `1890/1890` in `15078ms`; registrations `929/961`,
exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AS` closes numeric
`ImageDraw.Floodfill` empty border-sequence incomparability. Pillow keeps empty
tuple/list borders as incomparable objects, so bounded I/F fixtures fill
through scalar zero while preserving core identity and exact storage. The
existing AP native sentinel and AQ all-Array facade route passed raw/facade
proofs without production changes. Full `1889/1889` in `16656ms`;
registrations `929/960`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AR` closes numeric
`ImageDraw.Floodfill` scalar-border packing and stopping. Pillow and both
runtime surfaces pack/compare mode `I` zero as int32 and mode `F` zero as
float32, stop before the matching middle sample, and preserve allocation with
exact bytes. Existing native/facade routes passed immediately. Full
`1888/1888` in `15547ms`; registrations `929/959`, exports `389/389`, DLL
unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AQ` closes numeric
`ImageDraw.Floodfill` one-element border-sequence incomparability. Pillow
keeps tuple/list borders as objects, so `(0,)` / `[0]` and `(0.0,)` / `[0.0]`
never match scalar I/F zero samples. The facade now routes every numeric border
Array to AP's native sentinel; scalar borders retain packed-color routing.
Full `1886/1886` in `16672ms`; registrations `928/958`, exports `389/389`,
DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AP` closes numeric
`ImageDraw.Floodfill` multi-element border incomparability. Pillow accepts the
tuple/list objects but they never compare equal to scalar `I`/`F` samples, so
the native queue fills through zero samples. The existing ABI now gives
non-null/zero-size border an explicit incomparable meaning, and AHK only owns
sentinel routing/lifetime. Full `1885/1885` in `15687ms`; registrations
`928/957`, exports `389/389`; rebuilt DLL SHA-256 is
`B9DE93EE8F1E8E1986014BF2651F3B525D0A4E65DB27ADB66568CDB2E1675301`.

Incremental refresh, 2026-07-18: `MODE-NUM-001AO` closes numeric
`ImageDraw.Floodfill` multi-element value rejection with border unset and
threshold zero. Value-local facade validation emits Pillow's mode-specific
tuple errors before native dispatch and preserves allocation/bytes; the
flood-fill queue remains native. Full `1883/1883` in `15141ms`; registrations
`927/956`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AN` closes numeric
`ImageDraw.Bitmap` multi-element fill rejection with a valid mode `1` mask.
Bitmap-local facade validation emits Pillow's mode-specific tuple errors
before native dispatch and preserves target/mask allocation and bytes; valid
bitmap compositing remains native. Full `1882/1882` in `15610ms`;
registrations `927/955`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AM` closes numeric
`ImageDraw.RoundedRectangle` multi-element outline rejection with fill unset.
Outline-local facade validation emits Pillow's mode-specific tuple errors
before native dispatch and preserves allocation/bytes; fill handling and valid
rounded-rectangle loops remain native. Full `1881/1881` in `16594ms`;
registrations `927/954`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AL` closes numeric
`ImageDraw.RoundedRectangle` multi-element fill rejection with outline unset.
Fill-local facade validation emits Pillow's mode-specific tuple errors before
native dispatch and preserves allocation/bytes; outline handling and valid
rounded-rectangle loops remain native. Full `1880/1880` in `15328ms`;
registrations `927/953`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AK` closes numeric
`ImageDraw.Pieslice` multi-element outline rejection with fill unset.
Outline-local facade validation emits Pillow's mode-specific tuple errors
before native dispatch and preserves allocation/bytes; fill handling and valid
pieslice loops remain native. Full `1879/1879` in `15344ms`; registrations
`927/952`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AJ` closes numeric
`ImageDraw.Pieslice` multi-element fill rejection with outline unset.
Fill-local facade validation emits Pillow's mode-specific tuple errors before
native dispatch and preserves allocation/bytes; outline handling and valid
pieslice loops remain native. Full `1878/1878` in `16328ms`; registrations
`927/951`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AI` closes numeric
`ImageDraw.Chord` multi-element outline rejection with fill unset.
Outline-local facade validation emits Pillow's mode-specific tuple errors
before native dispatch and preserves allocation/bytes; fill handling and valid
chord loops remain native. Full `1877/1877` in `15657ms`; registrations
`927/950`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AH` closes numeric
`ImageDraw.Chord` multi-element fill rejection with outline unset. Fill-local
facade validation emits Pillow's mode-specific tuple errors before native
dispatch and preserves allocation/bytes; outline handling and valid chord
loops remain native. Full `1876/1876` in `16922ms`; registrations `927/949`,
exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AG` closes numeric
`ImageDraw.Arc` multi-element fill rejection. Arc-local facade validation
emits Pillow's mode-specific tuple errors before native dispatch and preserves
allocation/bytes; valid arc loops remain native. Full `1875/1875` in
`17110ms`; registrations `927/948`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AF` closes numeric
`ImageDraw.Ellipse` multi-element outline rejection with fill unset.
Outline-local facade validation emits Pillow's mode-specific tuple errors
before native dispatch and preserves allocation/bytes; fill handling and valid
ellipse loops remain native. Full `1874/1874` in `16796ms`; registrations
`927/947`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AE` closes numeric
`ImageDraw.Ellipse` multi-element fill rejection with outline unset.
Fill-local facade validation emits Pillow's mode-specific tuple errors before
native dispatch and preserves allocation/bytes; outline handling and valid
ellipse loops remain native. Full `1873/1873` in `14875ms`; registrations
`927/946`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AD` closes numeric
`ImageDraw.Rectangle` multi-element outline rejection with fill unset.
Outline-local facade validation emits Pillow's mode-specific tuple errors
before native dispatch and preserves allocation/bytes; fill handling and valid
rectangle loops remain native. Full `1872/1872` in `15469ms`; registrations
`927/945`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AC` closes numeric
`ImageDraw.Rectangle` multi-element fill rejection with outline unset.
Fill-local facade validation emits Pillow's mode-specific tuple errors before
the native call and preserves allocation/bytes; outline handling and valid
rectangle loops remain native. Full `1871/1871` in `15250ms`; registrations
`927/944`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AB` closes numeric
`ImageDraw.Line` multi-element color rejection. Line-local facade validation
now emits Pillow's mode `I` / tuple-like mode `F` errors before straight or
curve-joint DLL dispatch, preserving target allocation/bytes while valid line
loops remain native. Full `1870/1870` in `15344ms`; registrations `927/943`,
exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001AA` closes numeric
`ImageDraw.Point` multi-element color rejection. Point-local facade validation
now emits Pillow's distinct mode `I` and tuple-like mode `F` errors before the
DLL call while preserving target allocation/bytes; valid point loops remain
native. Full `1869/1869` in `15563ms`; registrations `927/942`, exports
`389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001Z` closes numeric multi-element
color-sequence `Image.Paste` rejection. The facade now emits Pillow's distinct
mode `I` and tuple-like mode `F` errors before the DLL call and preserves the
target allocation/bytes; valid fills remain native. Full `1868/1868` in
`15594ms`; registrations `927/941`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001Y` closes numeric one-element
color-sequence `Image.Paste` packing. AHK Arrays are the facade's established
tuple analogue, so `[300]` / `[1.5]` now reuse the signed-int32 / float32
packing accepted by Pillow tuples while the existing DLL owns the fill. Full
`1867/1867` in `16672ms`; registrations `927/940`, exports `389/389`, DLL
unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001X` closes numeric scalar-color
`Image.Paste` packing. The facade now passes signed-int32 `I` and float32 `F`
colors to the existing DLL fill instead of truncating them to one byte. Full
`1866/1866` in `16563ms`; registrations `927/939`, exports `389/389`, DLL
unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001W` closes numeric masked
`Image.Paste` in-place semantics. Existing DLL byte blending preserves target
allocation and exact `I`/`F` bytes while leaving source/mask unchanged. Full
`1864/1864` in `15672ms`; registrations `926/938`, exports `389/389`, DLL
unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001V` closes numeric
`Image.Composite` / `ImageChops.Composite` mode-L mask semantics. Existing
DLL byte blending exactly matches Pillow's four-byte `I`/`F` storage results.
Full `1862/1862` in `16734ms`; registrations `925/937`, exports `389/389`, DLL
unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001U` closes numeric
`ImageChops.Duplicate` copy semantics. Existing DLL allocation preserves exact
`I`/`F` bytes and independent lifetime. Full `1860/1860` in `15297ms`;
registrations `924/936`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001T` closes numeric-source
`ImageChops.Constant` semantics. Existing DLL mode-L allocation and clipped
fill are exact. Full `1858/1858` in `16625ms`; registrations `923/935`,
exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001S` closes numeric
`ImageChops.Offset` whole-sample wrap semantics. Existing DLL pixel copies
preserve four-byte `I`/`F` samples. Full `1856/1856` in `16203ms`;
registrations `922/934`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001R` closes numeric
`ImageChops.Invert` semantics. Existing DLL complement logic exactly matches
Pillow's 32-bit bitwise sample behavior for `I`/`F`. Full `1854/1854` in
`16281ms`; registrations `921/933`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001Q` closes matching numeric
`ImageChops.logical_and/or/xor` rejection. Existing native exports return
`-3`; the facade now raises exact `image has wrong mode`. Full `1852/1852` in
`16234ms`; registrations `920/932`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `MODE-NUM-001P` closes nonempty numeric
`ImageStat.Stat` semantics for modes `I` and `F` through native 256-bin
histograms and facade property derivation. Full `1850/1850` in `14469ms`;
registrations `919/931`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `META-002Y` closes zero-length-final JPEG ICC
fragment-mixture collation. `1/2:AB` plus empty `2/2` opens as public ICC
bytes `AB`. Full `1848/1848` in `16297ms`; registrations `918/930`, exports
`389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `META-002X` closes zero-length-first JPEG ICC
fragment-mixture collation. Empty `1/2` plus `2/2:AB` opens as public ICC
bytes `AB`. Full `1846/1846` in `17391ms`; registrations `917/929`, exports
`389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `META-002W` closes explicit empty JPEG ICC
save omission parity. Raw/facade saves emit no ICC APP2 and reopen without
public ICC metadata. Full `1844/1844` in `16234ms`; registrations `916/928`,
exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `META-002V` closes permissive duplicate-
sequence pre-SOF JPEG ICC collation. Two `1/2` fragments carrying `A` and `B`
open as public ICC bytes `AB` through raw/facade routes. Full `1842/1842` in
`17578ms`; registrations `915/927`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CS` closes CR's complete
1505-byte marker-stream/whole-file parity with SHA-256
`3af51c969694046b53a4fb0a4b73caee13617ffb286cbf5119b7be62458b5565`.
Full `1840/1840` in `16391ms`; registrations `914/926`, exports `389/389`,
DLL unchanged.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CR` closes default-4:2:0
rows-6 over-scan/no-RST parity. Pillow and native share DRI
`[18,36,18,36,18,36]`, ten empty RST arrays, exact DHT/SOS payloads, and 736
entropy bytes. Full `1838/1838` in `17109ms`; registrations `913/925`, exports
`389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CQ` closes CP's complete
1757-byte marker-stream/whole-file parity with SHA-256
`a51da5feec5f2a113a166e7df82fca3ca696713ff5b4cdf573cda267e73fa74f`.
Full `1836/1836` in `8579ms`; registrations `912/924`, exports `389/389`, DLL
unchanged.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CP` closes source-4:2:2
rows-6 over-scan/no-RST parity. Full `1834/1834` in `8547ms`; registrations
`911/923`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CO` closes CN's complete
1505-byte file. Full `1832/1832` passed in `17063ms`; registrations `910/922`,
exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CN` closes default-4:2:0
optimized-progressive rows-5 over-scan/no-RST parity. Full `1830/1830` passed
in `18062ms`; registrations `909/921`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CM` closes CL's complete
1757-byte file. Full `1828/1828` passed in `19593ms`; registrations `908/920`,
exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CL` closes source-4:2:2
optimized-progressive rows-5 over-scan/no-RST codec parity. Full `1826/1826`
passed in `18063ms`; registrations `907/919`, exports `389/389`, DLL unchanged.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CK` closes complete default-
4:2:0 rows-4 whole-file parity at 1505 bytes. Raw/facade/combined passed
`1/1`, `1/1`, `2/2`; full `1824/1824` passed in `8484ms`. Registrations are
`906` raw / `918` facade; exports/DLL remain `389/389` and
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CJ` closes exact per-scan DHT/
SOS/entropy parity for opened RGB qtables-keep omitted/default-4:2:0 optimized-
progressive rows-4 output. DRI is `[12,24,12,24,12,24]` and all ten restart
arrays are empty. Raw/facade/combined passed immediately `1/1`, `1/1`, and
`2/2` without production changes. Full `1822/1822` passes in `8531ms`;
registrations are `905` raw / `917` facade, exports remain `389/389`, and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CI` closes complete marker-
stream/whole-file parity for CH's opened RGB qtables-keep source-4:2:2
optimized-progressive rows-4 route. Raw/facade/combined immediately match
Pillow's 1757-byte file and SHA-256
`ea3a041a14fae80939d499cebf1c2f11eaa2d5089ab1618a392f65a0e1ac4e10`
in `1/1`, `1/1`, and `2/2` without production changes. Full `1820/1820`
passes in `8297ms`; registrations are `904` raw / `916` facade, exports
remain `389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CH` closes exact per-scan DHT/
SOS/entropy parity for opened RGB qtables-keep source-4:2:2 optimized-
progressive rows-4 output. The interval equals each complete scan, so DRI is
`[12,24,12,24,12,24]` and all ten restart arrays are empty. Raw/facade/
combined passed immediately `1/1`, `1/1`, and `2/2` without production
changes. Full `1818/1818` passes in `15422ms`; registrations are `903` raw /
`915` facade, exports remain `389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CG` closes complete marker-
stream/whole-file parity for CF's opened RGB qtables-keep omitted/default-
4:2:0 optimized-progressive rows-3 route. Raw/facade/combined immediately
match Pillow's 1517-byte file and SHA-256
`41f426c0d538d08799d332013ce9003d5df4a7ed095beb9fcde5108c4ff73282`
in `1/1`, `1/1`, and `2/2` without production changes. Full `1816/1816`
passes in `15579ms`; registrations are `902` raw / `914` facade, exports
remain `389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CF` closes exact per-scan DHT/
SOS/entropy parity for opened RGB qtables-keep omitted/default-4:2:0 optimized-
progressive rows-3 output. Existing h2v2 coefficients and component-local
tables compose with DRI `[9,18,9,18,9,18]` and mixed empty/RST0 scan state,
so raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2` without
production changes. Full `1814/1814` passes in `8516ms`; registrations are
`901` raw / `913` facade, exports remain `389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-18: `FMT-JPEG-002B2CE` closes complete marker-
stream/whole-file parity for CD's opened RGB qtables-keep source-4:2:2
optimized-progressive rows-3 route. Raw/facade/combined immediately match
Pillow's 1785-byte file and SHA-256
`77c26748760271b0edda207cd00054cbec155b38686c41abe4a25f5fc797128c`
in `1/1`, `1/1`, and `2/2` without production changes. Full `1812/1812`
passes in `10172ms`; registrations are `900` raw / `912` facade, exports
remain `389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2CD` closes exact per-scan DHT/
SOS/entropy parity for opened RGB qtables-keep source-4:2:2 optimized-
progressive rows-3 output. Existing component-local tables handle the full
three-row interval plus short tail with DRI `[9,18,9,18,9,18]` and RST0-only
scans, so raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2`
without production changes. Full `1810/1810` passes in `8375ms`;
registrations are `899` raw / `911` facade, exports remain `389/389`, and DLL
SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2CC` closes complete marker-
stream/whole-file parity for CB's opened RGB qtables-keep omitted/default-
4:2:0 optimized-progressive rows-2 route. Raw/facade/combined immediately
match Pillow's 1518-byte file and SHA-256
`5b280a28b104db2c0112d3070f9fb942674d7d4ed49c8bdcfe013931f0b19c8a`
in `1/1`, `1/1`, and `2/2` without production changes. Full `1808/1808`
passes in `15281ms`; registrations are `898` raw / `910` facade, exports
remain `389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2CB` closes exact per-scan DHT/
SOS/entropy parity for opened RGB qtables-keep omitted/default-4:2:0 optimized-
progressive rows-2 output. Existing h2v2 coefficients and component-local
tables compose with DRI `[6,12,6,12,6,12]` and mixed empty/RST0 scan state,
so raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2` without
production changes. Full `1806/1806` passes in `15344ms`; registrations are
`897` raw / `909` facade, exports remain `389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2CA` closes complete marker-
stream/whole-file parity for BZ's opened RGB qtables-keep source-4:2:2
optimized-progressive rows-2 route. Raw/facade/combined immediately match
Pillow's 1786-byte file and SHA-256
`84ca571e80c61740233af5d1ee999d62fd5138357b893d22ad3a30a5139853cb`
in `1/1`, `1/1`, and `2/2` without production changes. Full `1804/1804`
passes in `15109ms`; registrations are `896` raw / `908` facade, exports
remain `389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BZ` closes exact per-scan DHT/
SOS/entropy parity for opened RGB qtables-keep source-4:2:2 optimized-
progressive rows-2 output. Existing component-local tables compose with DRI
`[6,12,6,12,6,12]` and RST0-only scans, so raw/facade/combined passed
immediately `1/1`, `1/1`, and `2/2` without production changes. Full
`1802/1802` passes in `15453ms`; registrations are `895` raw / `907` facade,
exports remain `389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BY` closes complete marker-
stream/whole-file parity for BX's opened RGB qtables-keep omitted/default-
4:2:0 optimized-progressive rows-1 route. Raw/facade/combined immediately
match Pillow's 1552-byte file and SHA-256
`92552ebc5330f3e0616553bb7b0a3ce0f996181c156d9a50fd62e23bc47c4ef1` in
`1/1`, `1/1`, and `2/2` without production changes. Full `1800/1800` passes
in `15203ms`; registrations are `894` raw / `906` facade, exports remain
`389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BX` closes exact per-scan DHT/
SOS/entropy parity for opened RGB qtables-keep omitted/default-4:2:0 optimized-
progressive rows-1 output. BV's component-local tables and BS's h2v2 rounding
already compose exactly, so raw/facade/combined passed immediately `1/1`,
`1/1`, and `2/2` without production changes. Full `1798/1798` passes in
`15172ms`; registrations are `893` raw / `905` facade, exports remain
`389/389`, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BW` closes complete marker-
stream/whole-file parity for BV's opened RGB source-4:2:2 optimized-progressive
rows-1 route. Raw and facade both immediately emit Pillow's complete 1836-byte
file with SHA-256
`7c07b262b27d3e71cd82fc132f1546b0291c6b56aef3e4537e48b7888cedd659`.
Raw/facade/combined pass `1/1`, `1/1`, and `2/2`; full `1796/1796` passes in
`15297ms`. Registrations are `892` raw / `904` facade; exports remain
`389/389`, no rebuild was required, and DLL SHA-256 remains
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BV` closes exact per-scan
DHT/SOS/entropy parity for the opened RGB source-4:2:2 qtables-keep/
subsampling-keep optimized-progressive rows-1 route. Native now uses separate
Cr/Cb AC-first and final-refine Huffman tables, matching Pillow's ten DHT
segments and all ten entropy streams. Raw/facade/combined pass `1/1`, `1/1`,
and `2/2`; full `1794/1794` passes in `15265ms`. Registrations are `891` raw /
`903` facade; exports remain `389/389`, and rebuilt DLL SHA-256 is
`7C4E4AE1DAFAC682CA4E5DB328F548C8D68927421264A499224EA4256BE87C8A`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BU` closes complete marker-
stream/whole-file parity for BR's opened RGB source-4:2:2 optimized rows-1
route. Raw and facade both immediately emit Pillow's complete 1446-byte file
with SHA-256
`7dbee7c1e161ee84bd3175f3a5b96a99d92fc5e904a6cedad2fee81f1b361b83`.
Raw/facade/combined pass `1/1`, `1/1`, and `2/2`; full `1792/1792` passes in
`14891ms`. Registrations are `890` raw / `902` facade; exports remain
`389/389`, no rebuild was required, and DLL SHA-256 remains
`85249326EFFCFCEE05F8A00FBAAB0172FB3CD959B8A4FD29FA8F8B09E655C038`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BT` closes complete marker-
stream/whole-file parity for BS's opened RGB default-4:2:0 optimized rows-1
route. Raw and facade both immediately emit Pillow's complete 1190-byte file
with SHA-256
`8ce6c8b4f72e89d2e7493fde5e7f3f2a6312574a126c9f69ad710b736200a2ce`.
Raw/facade/combined pass `1/1`, `1/1`, and `2/2`; full `1790/1790` passes in
`14985ms`. Registrations are `889` raw / `901` facade; exports remain
`389/389`, no rebuild was required, and DLL SHA-256 remains
`85249326EFFCFCEE05F8A00FBAAB0172FB3CD959B8A4FD29FA8F8B09E655C038`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BS` closes exact optimized
DHT/entropy parity for opened RGB qtables-keep/default-4:2:0 rows-1 output.
Matching raw/facade REDs isolated the fourth chroma AC DHT to native h2v2
downsampling; alternating libjpeg-turbo's `1,2` bias now yields Pillow's four
exact DHT payloads and 751-byte entropy stream. Release x64 rebuilt with zero
warnings/errors. Raw/facade/combined pass `1/1`, `1/1`, and `2/2`; full
`1788/1788` passes in `15781ms`. Registrations are `888` raw / `900` facade,
exports remain `389/389`, and rebuilt DLL SHA-256 is
`85249326EFFCFCEE05F8A00FBAAB0172FB3CD959B8A4FD29FA8F8B09E655C038`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BR` closes exact optimized
DHT/entropy parity for opened RGB source-4:2:2 qtables-keep/subsampling-keep
rows-1 output. Matching raw/facade REDs isolated chroma DHT drift to native
h2v1 downsampling; alternating libjpeg-turbo's `0,1` bias now yields Pillow's
four exact DHT payloads and 1001-byte entropy stream. Release x64 rebuilt with
zero warnings/errors. Raw/facade/combined pass `1/1`, `1/1`, and `2/2`; full
`1786/1786` passes in `14922ms`. Registrations are `887` raw / `899` facade,
exports remain `389/389`, and rebuilt DLL SHA-256 is
`7A00F5EA1255AF5C64B2C0C86DDD377C55E130D81D5698AF2C8982D56D769C93`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BQ` closes exact DPI/JFIF
composition and whole-file parity on BP's opened real-YCCK quality-keep
progressive rows-2 XMP/core-metadata route. Pillow and native both write 10972
bytes with SHA-256
`a533204d5714f5166be1e591fd2ea2f755c678706c41f2bb62e86a777fc39e98`.
Unit-1 JFIF density 300x150 adds 18 bytes while BP's APP14-through-EOI suffix
remains exact. Raw/facade/combined passed immediately `1/1`, `1/1`, and
`2/2`. Full `1784/1784` passed in `14906ms`; registrations are `886` raw /
`898` facade; exports remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BP` closes exact explicit XMP
composition and whole-file parity on BO's opened real-YCCK quality-keep
progressive rows-2 route. Pillow and native both write 10954 bytes with
SHA-256
`566ae8f27df9e985b45a6ff9fbaa48ea3ef06b754623d819c1fec9b9db6861dc`.
XMP adds 357 bytes while BO's DQT-through-EOI suffix remains exact. Raw/facade/
combined passed immediately `1/1`, `1/1`, and `2/2`. Full `1782/1782` passed
in `15094ms`; registrations are `885` raw / `897` facade; exports remain
`389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BO` closes exact explicit
comment/ICC/EXIF composition and whole-file parity on BN's opened real-YCCK
quality-keep progressive rows-2 route. Pillow and native both write 10597
bytes with SHA-256
`170575141e6a0608bd493e1620dd8600da736d662dfcce6c45b6ace4753077e5`.
Metadata adds 86 bytes while BN's DQT-through-EOI suffix remains exact. Raw/
facade/combined passed immediately `1/1`, `1/1`, and `2/2`. Full `1780/1780`
passed in `15125ms`; registrations are `884` raw / `896` facade; exports
remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BN` closes complete marker-
stream/whole-file parity for AZ's opened real-YCCK quality-keep progressive
rows-2 route. Pillow and native both write 10511 bytes with SHA-256
`1b58f34f4ba33e9ea91f13c83d0ebc871678c5dafef8fae4f641d5412d49e098`.
All 42 non-RST and 108 RST markers across 18 scans match. Raw/facade/combined
passed immediately `1/1`, `1/1`, and `2/2`. Full `1778/1778` passed in
`14969ms`; registrations are `883` raw / `895` facade; exports remain
`389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BM` closes exact DPI/JFIF
composition and whole-file parity on BL's opened real-YCCK web-low progressive
rows-2 route. Pillow and native both write 3081 bytes with SHA-256
`425c1d53d71b5ae5f92be44f18dadd4d46d7c90e9a81d67bd8cf353246871512`.
JFIF adds 18 bytes while BL's APP14-through-EOI suffix remains exact. Raw/
facade/combined passed immediately `1/1`, `1/1`, and `2/2`. Full `1776/1776`
passed in `15031ms`; registrations are `882` raw / `894` facade; exports
remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BL` closes exact explicit XMP
composition and whole-file parity on BK's opened real-YCCK web-low progressive
rows-2 route. Pillow and native both write 3063 bytes with SHA-256
`7e7a9cfe6a082ec17e2313df4a4ef4dab11b378339994bd97b084267c4203912`.
XMP adds 357 bytes while BK's DQT-through-EOI suffix remains exact. Raw/facade/
combined passed immediately `1/1`, `1/1`, and `2/2`. Full `1774/1774` passed
in `14844ms`; registrations are `881` raw / `893` facade; exports remain
`389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BK` closes exact comment/ICC/
EXIF composition and whole-file parity on BJ's opened real-YCCK web-low
progressive rows-2 route. Pillow and native both write 2706 bytes with SHA-256
`78052658f630fb4b06715997271d47a345d6dd20fa150c9ecccd2ad8187a8b25`.
Metadata adds 86 bytes while BJ's DQT-through-EOI suffix remains exact. Raw/
facade/combined passed immediately `1/1`, `1/1`, and `2/2`. Full `1772/1772`
passed in `14875ms`; registrations are `880` raw / `892` facade; exports
remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BJ` closes complete marker-
stream/whole-file parity for AY's opened real-YCCK web-low progressive rows-2
route. Pillow and native both write 2620 bytes with SHA-256
`5b989fdbfc8551b15366762c7022b70d315584de61711107f94531c69629c611`.
The complete file has 50 non-RST markers and 66 RST markers across 18 scans;
all DHT/DRI/SOS/entropy/restart bytes remain AY-exact. Raw/facade/combined
passed immediately `1/1`, `1/1`, and `2/2`. Full `1770/1770` passed in
`14766ms`; registrations are `879` raw / `891` facade; exports remain
`389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BI` closes exact DPI/JFIF plus
XMP/comment/ICC/EXIF marker composition and whole-file parity on BH's opened
real-YCCK web-low optimized rows-2 route. Pillow and native both write 2320
bytes with SHA-256
`659d1cb624236da009b5cb0b79364a9ccaeb7e6e416af0f258e3ec2177331c8c`
and marker order
`SOI/APP0/APP14/APP1/APP1/APP2/COM/DQT/DQT/SOF0/DHT/DHT/DRI/SOS/RST0/RST1/RST2/EOI`.
JFIF adds exactly 18 bytes while every later BH byte stays exact. Raw/facade/
combined passed immediately `1/1`, `1/1`, and `2/2`. Full `1768/1768`
passed in `15031ms`; registrations are `878` raw / `890` facade; exports
remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BH` closes exact XMP plus
comment/ICC/EXIF marker composition and whole-file parity on BG's opened
real-YCCK web-low optimized rows-2 route. Pillow and native both write 2302
bytes with SHA-256
`f66203a245291c86186251b7b1c42c5c6c328e827ad3822eb23dbfe506c91ddc`
and marker order
`SOI/APP14/APP1/APP1/APP2/COM/DQT/DQT/SOF0/DHT/DHT/DRI/SOS/RST0/RST1/RST2/EOI`.
XMP adds exactly 357 bytes while BG's ICC-through-EOI suffix stays exact. Raw/
facade/combined passed immediately `1/1`, `1/1`, and `2/2`. Full `1766/1766`
passed in `14641ms`; registrations are `877` raw / `889` facade; exports
remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BG` closes exact explicit
comment/ICC/EXIF marker composition and whole-file parity on BF's opened
real-YCCK web-low optimized rows-2 route. Pillow and native both write 1945
bytes with SHA-256
`c0c8e3013e01187954fa9e1b34c3d1e4b9b4cb7c8e10f69a48ceeb0b1ae0530a`
and marker order
`SOI/APP14/APP1/APP2/COM/DQT/DQT/SOF0/DHT/DHT/DRI/SOS/RST0/RST1/RST2/EOI`.
Metadata adds exactly 86 bytes while BF's codec suffix stays exact. Raw/
facade/combined passed immediately `1/1`, `1/1`, and `2/2`. Full `1764/1764`
passed in `14469ms`; registrations are `876` raw / `888` facade; exports
remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BF` closes complete marker-
stream/whole-file parity for AX's opened real-YCCK web-low optimized rows-2
route. Pillow and native both write 1859 bytes with SHA-256
`557bf3d8032ca005c910ab0536426720f1869620f9edb960f87b6dc2b952b6d2`
and marker order
`SOI/APP14/DQT/DQT/SOF0/DHT/DHT/DRI/SOS/RST0/RST1/RST2/EOI`.
Raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2`. Full
`1762/1762` passed in `14844ms`; registrations are `875` raw / `887` facade;
exports remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BE` closes exact DPI/JFIF plus
XMP/comment/ICC/EXIF marker composition and whole-file parity on BD's opened
real-YCCK quality-keep optimized rows-2 route. Pillow and native both write
10676 bytes with SHA-256
`5bfd8153d5f80be7936e07c950fb98008f6ae61e521a9c72954dbb1506641b7f`
and marker order
`SOI/APP0/APP14/APP1/APP1/APP2/COM/DQT/DQT/SOF0/DHT/DHT/DRI/SOS/RST0..RST5/EOI`.
JFIF carries unit-1 density 300x150; all later metadata, codec, restart, and
EOI bytes remain exact. Raw/facade/combined passed immediately `1/1`, `1/1`,
and `2/2`. Full `1760/1760` passed in `14391ms`; registrations are `874` raw /
`886` facade; exports remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BD` closes exact XMP plus
comment/ICC/EXIF marker composition and whole-file parity on BC's opened
real-YCCK quality-keep optimized rows-2 route. Pillow and native both write
10658 bytes with SHA-256
`fca9c47e5a1305ebdeb02293997ee702d8317d38d9f81d28bdf1e22a55fabf8d`
and marker order
`SOI/APP14/APP1/APP1/APP2/COM/DQT/DQT/SOF0/DHT/DHT/DRI/SOS/RST0..RST5/EOI`.
Raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2`. Full
`1758/1758` passes; registrations are `873` raw / `885` facade; exports
remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BC` closes exact explicit
comment/ICC/EXIF marker composition and whole-file parity on BB's opened
real-YCCK quality-keep optimized rows-2 route. Pillow and native both write
10301 bytes with SHA-256
`b333a57d144684cd7c88852bc18e09665f731925b631b085c3681b5fef4fbd8c`
and marker order
`SOI/APP14/APP1/APP2/COM/DQT/DQT/SOF0/DHT/DHT/DRI/SOS/RST0..RST5/EOI`.
Raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2`. Full
`1756/1756` passes; registrations are `872` raw / `884` facade; exports
remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BB` closes complete
marker-stream/whole-file parity for BA's opened real-YCCK quality-keep
optimized rows-2 fixture. Pillow and native both write 10215 bytes with
SHA-256
`b184bd798ee1946c26d54590c06499d12aab607e29adaa928dad4fea7dd66b75`
and marker order `SOI/APP14/DQT/DQT/SOF0/DHT/DHT/DRI/SOS/RST0..RST5/EOI`.
Raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2`. Full
`1754/1754` passes; registrations are `871` raw / `883` facade; exports
remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2BA` closes exact optimized
baseline DHT/entropy parity for AS's opened real-YCCK quality-keep rows-2
fixture. Pillow's two DHT payloads total 117 bytes with SHA-256
`f37376dd1ca78fd06b58fb5d4d019c25bc08372d7c7a4376e26c622bdde8fd81`;
DRI is `26`, six restart markers are emitted, and the 9888-byte entropy stream
has SHA-256
`24bab3aea4b9593a9809f197ab174cacc597365d97002c850230f6bc63b890c9`.
Raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2`, proving the
existing optimized frequency/DC reset/restart/entropy paths already match.
Full `1752/1752` passes; registrations are `870` raw / `882` facade; exports
remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2AZ` closes exact progressive
DHT/entropy parity for AU's opened real-YCCK quality-keep rows-2 fixture.
Pillow's 17 DHT payloads total 673 bytes with SHA-256
`15acadd369c28e11cd67dc6734d38fd4552ddd3614efa32d8dbcc26c68bd7b18`;
18 entropy streams total 9392 bytes with SHA-256
`3762d51c2cb9ea99c433bd5886d47d3d258b7c4141b4781844874cc2fc9106d3`.
Raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2`, proving the
existing progressive restart/frequency/entropy paths already match. Full
`1750/1750` passes; registrations are `869` raw / `881` facade; exports remain
`389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2AY` closes exact progressive
DHT/entropy parity for AV's opened real-YCCK web-low rows-2 fixture. Pillow's
17 DHT payloads total 477 bytes with SHA-256
`675051c54cf609414e5437cee4a9687f1f2ab1faaf7bb94206c7094200f0b6e8`;
18 entropy streams total 1649 bytes with SHA-256
`69178ca1f1c5ff13a13a639d918f61d7cb1634f6e95e1d041880bd1b96f24d28`.
Raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2`, proving the
existing progressive restart/frequency/entropy paths already match. Full
`1748/1748` passes; registrations are `868` raw / `880` facade; exports remain
`389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2AX` closes exact optimized
baseline DHT/entropy parity for AW's opened real-YCCK web-low rows-2 fixture.
Pillow retains AW's two DHT payloads totaling 87 bytes, writes DRI `14`, three
restart markers, and 1562 entropy bytes with SHA-256
`bcf983601fbdc6f472b5e7a573faf16a14c3563cb9f40d96327be9ca34b4a5eb`.
Raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2`, proving the
existing optimized-Huffman, DC reset, and restart paths already match. Full
`1746/1746` passes; registrations are `867` raw / `879` facade; exports remain
`389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2AW` closes exact optimized
baseline DHT/entropy parity for AL's opened real-YCCK web-low rows-1 fixture.
Pillow writes two DHT payloads totaling 87 bytes with SHA-256
`48f5721c8573a5afeea606130fd04f23abd7c4bf3f1492f08836f47b62ad879c`,
DRI `7`, six restart markers, and 1578 entropy bytes with SHA-256
`56dd23d5b7ab697c753ba267e7c026d841530adc62ab1896011118d7d9a41fa5`.
Raw/facade/combined passed immediately `1/1`, `1/1`, and `2/2`, proving the
existing h2v2, integer FDCT, optimized-Huffman, and restart paths already
compose. Full `1744/1744` passes; registrations are `866` raw / `878` facade;
exports remain `389/389` and the DLL was not rebuilt.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2AV` closes exact progressive
DHT/entropy parity for AL's opened real-YCCK web-low optimized rows-1 fixture.
Pillow's 17 DHT payloads total 462 bytes with SHA-256
`a3303dc74eb6f20293a45e580afb5c17dc3da258c54ca96447019baac2b69b49`;
18 scan entropy streams total 1831 bytes with SHA-256
`c798b5b5ae59884ad5d7ee89d419bbdf1f52c7619bfbe5b5730f2dcafc8d8dcc`.
Native 4:2:0 M/Y/K downsampling now applies libjpeg's alternating h2v2
rounding bias instead of floor-only averaging. Raw/facade/combined pass `1/1`,
`1/1`, and `2/2`; related filters and full `1742/1742` pass. The tree
registers `865` raw and `877` facade tests; exports remain `389/389`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2AU` closes exact progressive
DHT/entropy parity for AL's opened real-YCCK quality-keep optimized rows-1
fixture. Pillow writes 17 DHT payloads totaling 657 bytes with SHA-256
`319771eac880d557b1ffc40ee4f6bbadf62f7eb18836bd84b81641498fcc2ab8`;
18 scan entropy streams total 9756 bytes with SHA-256
`a587c434f5b076907f3345e41f2d1a026640239ac2070b71ba858302f16b0539`.
The shared AC-first path now accumulates cross-block EOBRUN instead of emitting
one EOB per block, while DC-first uses libjpeg-compatible signed successive
approximation for negative coefficients. Raw/facade/combined pass `1/1`,
`1/1`, and `2/2`; related filters and full `1740/1740` pass. The tree
registers `864` raw and `876` facade tests; exports remain `389/389`.

Incremental refresh, 2026-07-14: `FMT-JPEG-002B2AT` closes exact progressive
DHT/entropy parity for the bounded `48x32` RGB keep-rgb/custom-qtables
optimized rows-1 fixture. Pillow's 13 DHT payload lengths are
`[24,27,28,26,34,39,37,28,31,24,27,24,25]`; 14 scan entropy lengths total
2566 bytes and concatenate to SHA-256
`5ca7ccdfc2dbbaf98d5d58f8a56d071017c8c2525ae6518475ea80205e1bc874`.
Native already matched the first eleven scans and final B-refine scan exactly;
only final R/G-refine differed because EOB/correction state was emitted per
block. Shared frequency/output passes now retain libjpeg-compatible EOBRUN and
correction bits across blocks and flush identically at restart/end boundaries.
Raw/facade/combined pass `1/1`, `1/1`, and `2/2`; related filters and full
`1738/1738` pass. The tree registers `863` raw and `875` facade tests;
exports remain `389/389`.

Incremental refresh, 2026-07-13: `FMT-JPEG-002B2AS` closes exact optimized
DHT/entropy parity for one opened real-YCCK quality-keep row-restart case.
Pillow writes DHT lengths `[29,88]`, DRI `13`, twelve RST markers, and 9944
entropy bytes with SHA-256
`b377c4c2751df94add98dc971268c11e917f455bcd15c83d961ea491b8dc5145`;
native had `[29,90]` and 9921 bytes. A libjpeg-compatible integer ISLOW probe
matched all 676 blocks and all coefficients, and libjpeg's 257-symbol optimized
Huffman procedure reproduced both DHT payloads byte-for-byte. Native now uses
those shared algorithms. Raw/facade/combined passed `1/1`, `1/1`, and `2/2`;
related filters and full `1736/1736` passed. The tree registers `862` raw and
`874` facade tests; exports remain `389/389`. The shared builder also changes
the existing keep-RGB progressive native tail to `[26,23,25]`, while Pillow is
`[27,24,25]`; that exact progressive boundary remains separate.

Incremental refresh, 2026-07-13: `FMT-JPEG-002B2AR` composes simultaneous
quality, qtables, and subsampling on AP's complete real-YCCK baseline restart
DPI/XMP/core-metadata surface. Pillow proves quality keep/`web_low` overrides
keep/preset/custom callers and does not validate invalid shadowed values; each
conflict is byte-identical to its pure-quality baseline. Raw was already green;
facade RED exposed eager subsampling validation, so quality precedence now runs
before lower-priority parsing and dispatches the same one-call DLL route.
Facade/combined passed `1/1` and `2/2`; related filters passed; full passed
`1734/1734` in `17454ms`, registering `861` raw and `873` facade tests. No
native/ABI/DLL change occurred; exports remain `389/389`.

Incremental refresh, 2026-07-13: `FMT-JPEG-002B2AQ` adds qtables keep/preset
to AP's full real-YCCK baseline restart surface and fixes shared FDCT half-tie
quantization. Entropy comparison found native `0` versus libjpeg `-1` for
an exact `-27/54=-0.5` M coefficient because floating cosine roundoff yielded
`-0.4999999999999997`. Native now stabilizes scaled-ULP half ties before
half-away rounding. The rebuilt DLL and generalized facade sentinel route pass
raw/facade/combined plus qtables/CMYK/RGB/DPI/XMP/metadata/restart/YCCK/JPEG;
full passed `1732/1732` in `8657ms`, registering `860` raw and `872`
facade tests. Exports remain `389/389`.

Incremental refresh, 2026-07-13: `FMT-JPEG-002B2AP` adds `dpi=(300,150)`
to AO's opened real-YCCK keep/`web_low` baseline restart XMP/core-metadata
pair. Pillow and native write unit-1 JFIF before APP14 and reopen matching DPI/
JFIF density while all metadata, restart, sampling, and pixel expectations
remain stable. Raw was already green; the facade removed only AO's no-DPI
term. Facade/combined passed `1/1` and `2/2`; DPI/XMP/metadata/restart/
YCCK/JPEG passed `48/48`, `28/28`, `289/289`, `59/59`, `15/15`, and
`257/257`; full passed `1730/1730` in `16828ms`, registering `859` raw
and `871` facade tests. No native/ABI/DLL change occurred; exports remain
`389/389`.

Incremental refresh, 2026-07-13: `FMT-JPEG-002B2AO` adds explicit XMP to AN's
opened real-YCCK keep/`web_low` baseline restart core-metadata pair. Pillow
and native write APP14, EXIF APP1, XMP APP1, ICC APP2, COM, then DQT; exact XMP
bytes and parsed title `Hello` reopen while sampling, DRI/RST counts, core
metadata, and bounded pixels remain stable. Raw was already green; the facade
removed only AN's XMP exclusion and retained no-DPI routing. Facade/combined
passed `1/1` and `2/2`; XMP/metadata/restart/YCCK/JPEG passed `26/26`,
`287/287`, `57/57`, `13/13`, and `255/255`; full passed `1728/1728`
in `15625ms`, registering `858` raw and `870` facade tests. No native/ABI/
DLL change occurred; source/DLL exports remain `389/389`.

Incremental refresh, 2026-07-13: `FMT-JPEG-002B2AN` composes opened real-YCCK
baseline block restarts with explicit comment/ICC/EXIF and either quality keep
or `web_low`. Pillow and native write APP14-transform-0 then EXIF/ICC/COM/DQT,
with source DQT plus CMYK 1x1 and 56 RST markers for keep, or preset DQT plus
4:2:0 and 16 RST markers for `web_low`. The raw companion was already green;
the facade now routes this no-XMP/no-DPI combination through the existing
qtables metadata restart export. Facade/combined passed `1/1` and `2/2`;
metadata/restart/qtables/CMYK/YCCK/JPEG passed `285/285`, `55/55`,
`103/103`, `145/145`, `11/11`, and `253/253`; full passed `1726/1726`
in `8047ms`, registering `857` raw and `869` facade tests. No native/ABI/
DLL change occurred; source/DLL exports remain `389/389`.

Incremental refresh, 2026-07-13: `FMT-JPEG-002B2AM` corrects RGB 4:2:0
progressive component scans at non-MCU-aligned dimensions. On the bounded
`100x100` rows=1 fixture, Pillow uses interleaved DRI `7`, Y-only DRI `13`,
and per-scan RST counts `[6,12,6,6,12,12,6,6,6,12]`; native previously used
padded Y DRI `14`. The DLL now preserves MCU-padded Y order for interleaved DC
scans and uses a true-size raster Y view for Y-only AC scans. Facade routing
already used this path and required no production change. BV later upgrades
the shared encoder to ten component-local DHT segments; exact default-4:2:0
entropy parity remains separate. Release x64 rebuilt cleanly; raw/facade/combined passed `1/1`,
`1/1`, and `2/2`; progressive/restart/RGB/JPEG passed `51/51`, `53/53`,
`276/276`, and `251/251`; full passed `1724/1724` in `7922ms`, registering
`856` raw and `868` facade tests. Source/DLL exports remain `389/389`.

This document compares the current `pillow-c` surface directly against local
Pillow `11.3.0`, instead of inferring the route only from existing gap IDs.
It is a route-finding audit, not a coverage-completion claim.

Authority and inputs:

- Pillow oracle: `F:\Python\Python310\python.exe`, Pillow `11.3.0`.
- Project facade: `ahk\pillow.ahk`.
- Native implementation: `src\pillow_c.cpp`.
- Test surface: `ahk\pillow.test.ahk` and `ahk\pillow_c.test.ahk`.
- Current verification from `docs\pillow-gap-checkpoint.md`: the current tree
  has `1053` tests. Facade full file passed `530/530`, raw DLL full file
  passed `523/523`, and the full directory runner with `-TimeoutSeconds 120`
  timed out after `974` PASS lines with no FAIL/ERROR lines.

Fresh local recheck on 2026-06-20:

- `PIL.Image.ID` / `PIL.Image.OPEN`: `45` open-capable format IDs.
- Open/save/save_all format union: `48` identifiers. This is the count used
  for "registered format identifiers" below.
- `PIL.Image.SAVE`: `30` identifiers.
- `PIL.Image.SAVE_ALL`: `7` identifiers.
- Local optional Pillow modules/codecs report WebP, AVIF, JPEG, JPEG2000,
  zlib, libtiff, FreeType, and littlecms2 as available. Pillow feature flags
  also report WebP animation/mux/transparency, RAQM, FriBiDi, HarfBuzz,
  libjpeg-turbo, and zlib-ng support.
- `PIL.Image.Image` exposes `59` non-private names in this environment,
  including `50` callable methods and `7` properties. Name coverage is only a
  pointer to where semantic tests should go; it is not a parity score.
- Direct facade parse after `MODE-NUM-001B`: `PIL.Image.Image` has `49` strict
  name-normalized direct facade name/property analogues for those `59` names,
  or about `50` if the AHK-style `Format` property is counted as the `format`
  analogue. The missing names are mostly Python object-model or
  external-integration surfaces, not common hot-loop operations. `Format`
  exists as an AHK-style property, but this is not full Python object-model
  parity.
- Current source recheck after `MODE-NUM-001B`: `src\pillow_c.cpp` declares
  `337` exported `pillow_c_*` functions; the Release x64 DLL export table also
  has `337`.
- Current test tree recheck: `1053` AHK tests, split as `530` facade tests
  and `523` raw DLL tests. `MODE-NUM-001B` is covered by raw/facade numeric
  histogram tests and histogram regression filters.
- Current file-size recheck: `src\pillow_c.cpp` has `36190` lines,
  `ahk\pillow.ahk` has `7664`, `ahk\pillow_c.test.ahk` has `31312`, and
  `ahk\pillow.test.ahk` has `23877`.
- Current direct `frombuffer` oracle: local Pillow 11.3.0 keeps
  `RGB`/`RGB` as a copy with `readonly == 0`, while `RGBA`/`RGBA` and
  requested `RGB` plus rawmode `RGBX` are readonly aliases under the probed
  stride/orientation matrix and detach after the first write. Current native
  and facade tests now cover those bounded direct `RGB`/`RGBA` rules.

## Executive Finding

The current project is strong in the common AHK scripting path, but weak as a
full Pillow replacement.

- Current delivery-closure completion for the explicit AHK-first goal: `73%`.
- This single score supersedes the earlier dual AHK-first/full-replacement
  percentages for current reporting.
- File-format breadth is the biggest numeric gap.
- Common object-method name coverage is no longer the main bottleneck.
- Metadata object behavior beyond the bounded EXIF-orientation child, color
  management, font/RAQM, constructor interop, and long-tail modes are the
  biggest semantic gaps.
- PNG/JPEG/GIF have enough depth that future work must be route-driven, not
  one-combination branch-driven.
- The current finish speed problem is not insufficient implementation volume:
  it is over-splitting already-known option combinations. New branches should
  buy a reusable semantic pillar, a new ABI shape, a new native route, or a
  locally proven Pillow boundary.
- The immediate default route should be one of four reusable pillars: another
  mode-scoped `I`/`F` operation child, one locally proven `META-001` EXIF/TIFF
  object child, split TIFF tag/compression/mode work, or a dependency-scoped
  `FMT-WEBP-001` still-image milestone if broad format coverage is explicitly
  selected. The selected default is now `MODE-NUM-001C`, numeric
  `I`/`F -> L` conversion, because its Pillow oracle is already pinned and it
  extends a reusable operation family across both numeric modes. This directly
  attacks semantic gaps that affect many Pillow APIs instead of adding another
  PNG option cross-product.

## Route Scorecard

Use this scorecard when choosing the next implementation packet after a broad
compatibility question. It deliberately favors work that shifts a boundary,
not work that only adds another known option combination.

| Rank | Packet | Why it moves faster | Stop condition |
| ---: | --- | --- | --- |
| 1 | `MODE-NUM-001C`: `I`/`F -> L` conversion | One native conversion branch covers two four-byte numeric modes and a common public method. The local Pillow oracle is already pinned. | Stop after `Convert("L")`; do not expand to all numeric conversions in the same slice. |
| 2 | `META-001` EXIF/TIFF object child | Metadata object semantics affect JPEG, PNG, TIFF, and public `getexif()` behavior. | Only start after a local Pillow oracle proves one bounded object behavior. |
| 3 | `FMT-TIFF-002` / `FMT-TIFF-003` split child | TIFF tags, compression, high-bit modes, and `I`/`F` participation are visible format gaps after multipage save. | Split tags, compression, and mode breadth; do not make one broad TIFF rewrite. |
| 4 | `FMT-WEBP-001` still-image milestone | WebP is registered locally for open/save/save_all and is a visible missing format family. | Requires an explicit dependency/package decision before ABI work. |
| 5 | Hot-format edge packet | GIF/JPEG/PNG work is valuable only when an oracle reveals a new semantic boundary or the implementation adds a new native route/ABI shape. | Batch same-route PNG/JPEG/GIF combinations; do not create more `FMT-PNG-001AA`-style branches. |

## Direct Pillow Inventory

Local Pillow `11.3.0` inventory:

| Surface | Pillow 11.3.0 baseline | Current project signal |
| --- | ---: | --- |
| Format identifiers in open/save/save_all union | `48` | Native explicit open/save families for about `10` practical families |
| Open-capable formats | `45` | BMP, PPM-family, QOI, TGA, XBM, ICO, PNG, JPEG, TIFF, GIF have native paths |
| Save-capable formats | `30` | Same core set, with partial per-format option depth |
| Save-all formats | `7` | GIF animation has real depth; bounded TIFF multipage save is covered; APNG/WebP/MPO/PDF save-all are not covered |
| `Image.Image` public names | `59` | `49` strict direct facade name/property analogues, about `50` if AHK `Format` is counted for Python `format`; `I` and `F` numeric `getextrema()` and `histogram()` are covered; remaining names are mostly `getxmp`, `getim`/`im`, `get_child_images`, Qt/show integration, `format_description`, and `has_transparency_data` |
| Pillow module surface | mixed | `ImageOps`/`ImageChops` are broad by name; the public `ImageCms` module remains absent despite the bounded statically linked bidirectional sRGB/LAB route; `ImageFont.truetype`, `ImagePalette`, and dependency constructors are major gaps |
| Native ABI exports | N/A | `337` `pillow_c_*` exports |
| AHK tests | N/A | `1053` total: `530` facade, `523` raw DLL; split full-file runs are green, directory run exceeds the 120-second budget |
| Implementation/test size | N/A | `36190` native lines, `7664` facade lines, `55189` AHK test lines |

The raw format count understates practical progress because the current
covered formats include common hot paths. It also overstates Pillow parity if
read as "core formats are enough", because Pillow's plugin ecosystem is a
major part of real compatibility. The correct planning denominator is split:
AHK-first hot-path compatibility should prioritize semantic pillars and core
formats, while full Pillow replacement must account for plugins, optional
codecs, Python object-model surfaces, CMS, fonts, and array/Qt constructors.

### Registered Format Matrix

This matrix is generated from the local Pillow `11.3.0` `Image.OPEN`,
`Image.SAVE`, and `Image.SAVE_ALL` registries, then mapped to the current
project state. It is meant to select work, not to create 48 immediate tasks.

| Pillow ID | Pillow capability | Current project state | Route |
| --- | --- | --- | --- |
| `AVIF` | open/save/save_all | absent | Dependency-gated; explicit format milestone only. |
| `BLP` | open/save | absent | Long-tail; defer. |
| `BMP` | open/save | native core, bounded Windows BMP | Keep as edge-case/mode work, not a priority branch. |
| `BUFR` | open/save | absent | Long-tail; defer. |
| `CUR` | open | absent | Pair with ICO/CUR hotspot work if chosen. |
| `DCX` | open | absent | Long-tail; defer. |
| `DDS` | open/save | absent | Practical legacy format, but behind explicit gap ID. |
| `DIB` | open/save | no standalone DIB facade/plugin parity | Treat separately from BMP only if needed. |
| `EPS` | open/save | absent | Dependency/export work; defer. |
| `FITS` | open | absent | Long-tail; defer. |
| `FLI` | open | absent | Long-tail; defer. |
| `FPX` | open | absent | Long-tail; defer. |
| `FTEX` | open | absent | Long-tail; defer. |
| `GBR` | open | absent | Long-tail; defer. |
| `GIF` | open/save/save_all | native deep partial | Continue only for proven animation/palette misses. |
| `GRIB` | open/save | absent | Long-tail; defer. |
| `HDF5` | open/save | absent | Dependency-heavy; defer. |
| `ICNS` | open/save | absent | Icon-format packet only after ICO/CUR priority. |
| `ICO` | open/save | native partial | Split multi-entry/frame-selection gaps from CUR. |
| `IM` | open/save | absent | Long-tail; defer. |
| `IMT` | open | absent | Long-tail; defer. |
| `IPTC` | open | absent | Metadata-adjacent, but lower ROI than EXIF/XMP. |
| `JPEG` | open/save | native deep partial | Continue only for proven metadata/codec boundaries. |
| `JPEG2000` | open/save | absent | Dependency-gated; lower ROI than JPEG metadata. |
| `MCIDAS` | open | absent | Long-tail; defer. |
| `MIC` | open | absent | Long-tail; defer. |
| `MPEG` | open | absent | Not an AHK-first still-image priority. |
| `MPO` | save/save_all | absent | JPEG-family multipicture work; explicit gap only. |
| `MSP` | open/save | absent | Long-tail; defer. |
| `PALM` | save | absent | Long-tail; defer. |
| `PCD` | open | absent | Long-tail; defer. |
| `PCX` | open/save | absent | Practical legacy format; explicit gap if needed. |
| `PDF` | save/save_all | absent | Export/packaging feature, not hot-loop runtime. |
| `PIXAR` | open | absent | Long-tail; defer. |
| `PNG` | open/save/save_all | native still-image deep partial; APNG absent | Use generalized metadata route; APNG separate. |
| `PPM` | open/save | native Netpbm family partial | Continue only for mode/high-bit edge gaps. |
| `PSD` | open | absent | Long-tail; defer. |
| `QOI` | open/save | native core | Low current risk. |
| `SGI` | open/save | absent | Long-tail; defer. |
| `SPIDER` | open/save | absent | Long-tail; defer. |
| `SUN` | open | absent | Long-tail; defer. |
| `TGA` | open/save | native core partial | Edge-case/mode work only. |
| `TIFF` | open/save/save_all | native partial with bounded multipage save | Continue with tags/compression/high-bit/mode children only. |
| `WEBP` | open/save/save_all | absent | First dependency-gated new format candidate. |
| `WMF` | open/save | absent | Platform/export work; defer. |
| `XBM` | open/save | native core | Low current risk. |
| `XPM` | open | absent | Long-tail; defer. |
| `XVTHUMB` | open | absent | Long-tail; defer. |

The matrix changes the route in two important ways. First, it makes plugin
breadth a known full-replacement deficit without letting it dominate the
AHK-first queue. Second, it shows why PNG-style option branching is too slow:
same-route metadata combinations should be batched once the generalized
native writer can express them. The direct `BYTES-001` core raw matrix is no
longer the default next packet: raw `L`, mapmode `L`/`RGBA`/`RGBX`, direct
`RGB` copy, and direct `RGBA` stride/orientation alias/detach are covered.
Continue BYTES work only for a concrete lower-level readonly/detach miss or a
deliberately scoped dependency constructor; otherwise move to mode-scoped
`I`/`F` operation breadth, metadata object behavior, split TIFF
tag/compression/mode children, or a deliberate dependency-gated WebP
still-image milestone.

## Public API Difference

The name-level comparison is useful for finding holes, but it does not prove
semantic parity. Many names exist with intentionally bounded behavior.

### `Image` Constructors And Module Functions

Pillow high-value constructors/functions compared:

- Covered by name: `open`, `new`, `frombytes`, `linear_gradient`,
  `radial_gradient`, `effect_mandelbrot`, `effect_noise`, `blend`,
  `composite`, `alpha_composite`, `eval`, `merge`.
- Partially covered: bounded `frombuffer` raw `L` buffer-view alias/detach
  lifecycle through native refresh/detach ABI, raw mapmode override aliases
  for requested `RGB` with rawmode `L`, `RGBA`, or `RGBX`, direct `RGB` copy
  semantics, and direct `RGBA` stride/orientation alias/detach. The `RGBX`
  child exposes public native mode id `10` and `R/G/B/X` band names for this
  raw/frombuffer surface.
- Missing or materially absent: full `frombuffer` matrix, `fromarray`,
  `fromarrow`, `fromqimage`, `fromqpixmap`.
- Lower-level plugin registry APIs such as `register_open`, `register_save`,
  and `registered_extensions` are not a current facade goal.

Route impact:

- `frombuffer` remains the most important constructor family for AHK
  performance, but the highest-value bounded ownership matrix is now covered.
  Continue only for a concrete remaining readonly/detach behavior or a
  dependency constructor boundary.
- `fromarray` and `fromarrow` need a dependency policy before implementation.
- Qt constructors should stay out unless there is an AHK use case.

### `Image.Image` Object

Direct parse result: local Pillow exposes `59` public `Image.Image` names; the
current facade has direct name/property analogues for about `50`. Hot method
name coverage is high: the current facade has names for most common
object methods such as `save`, `copy`, `crop`, `resize`, `rotate`,
`transpose`, `transform`, `convert`, `filter`, `point`, `paste`, `split`,
`getchannel`, `putalpha`, `quantize`, `remap_palette`, `reduce`, `thumbnail`,
`getdata`, `putdata`, `getpixel`, `putpixel`, `histogram`, `entropy`,
`getbbox`, `getcolors`, `getextrema`, `getprojection`, `seek`, `tell`,
`draft`, `load`, `close`, `verify`, `getpalette`, `putpalette`, `tobytes`,
and `frombytes`.

Material object gaps:

- `getexif()` is now covered only for a bounded orientation-tag lifecycle on
  JPEG/PNG. Full mutable EXIF dictionaries, TIFF tag lifecycle, nested IFDs,
  MakerNote preservation, and implicit mutation writeback remain open.
- `getxmp()` is absent.
- `has_transparency_data` and low-level `im/getim` semantics are absent or
  intentionally not exposed. `readonly` exists only as a bounded buffer-view
  signal, not as full Pillow core object parity.
- Property shape exists in AHK style (`Mode`, `Size`, `Width`, `Height`,
  `Info`, `Text`, `Format`) but is not a Python object model clone.
- Many present methods are mode-bounded: `I`, `F`, `YCbCr`, `LAB`, `HSV`, and
  plugin-specific raw modes remain incomplete.

Names without a direct current analogue:

`format_description`, `get_child_images`, `getim`, `getxmp`,
`has_transparency_data`, `im`, `show`, `toqimage`, and `toqpixmap`.

Name-level object classification from local Pillow:

| Category | Pillow names | Current project route |
| --- | --- | --- |
| Hot object methods with substantial facade/native coverage | `save`, `copy`, `crop`, `resize`, `rotate`, `transpose`, `transform`, `convert`, `filter`, `point`, `paste`, `split`, `getchannel`, `putalpha`, `putdata`, `getdata`, `getpixel`, `putpixel`, `histogram`, `entropy`, `getbbox`, `getcolors`, `getextrema`, `getprojection`, `seek`, `tell`, `load`, `close`, `verify`, `tobytes`, `frombytes` | Keep expanding by semantic gaps and mode matrices, not by name counting. |
| Present but bounded semantics | `alpha_composite`, `apply_transparency`, `draft`, `effect_spread`, `getbands`, `getexif`, `getpalette`, `putpalette`, `quantize`, `reduce`, `remap_palette`, `thumbnail`, `tobitmap` | Continue only with targeted Pillow oracles. `getexif` and `frombuffer` stay separate semantic pillars. |
| Missing or intentionally absent | `get_child_images`, `getim`, `getxmp`, `show`, `toqimage`, `toqpixmap`, full `im` property behavior, full `format_description` / `has_transparency_data` parity | Add only when they unlock a real AHK workflow or a broader compatibility pillar. |
| Python object-model properties | `format`, `format_description`, `has_transparency_data`, `height`, `im`, `mode`, `readonly`, `size`, `width` | AHK facade exposes AHK-style properties for common state; low-level Python core object parity is not a near-term goal. |

Route impact:

- Do not spend more time counting method names. The useful next work is
  semantic matrices for metadata, modes, and constructors.
- Continue `getexif()` only as bounded children. The core `frombuffer` matrix
  now covers the highest-value `L`/`RGBA`/`RGBX` alias cases plus direct
  `RGB` copy and direct `RGBA` stride/orientation behavior; continue it only
  for a concrete remaining readonly/detach miss or dependency constructor.

### Common Modules

| Module | Current state | Main difference from Pillow |
| --- | --- | --- |
| `ImageOps` | Strong common coverage; about 18 meaningful public functions covered by name | Remaining gaps are mostly mode/edge semantics, not missing names |
| `ImageChops` | Strong name coverage; 21 meaningful public functions covered by name | Needs broader mode matrix and edge cases, especially non-core modes |
| `ImageFilter` | Strong common filter coverage through native classes/functions | Missing broad class compatibility and full mode coverage |
| `ImageEnhance` | Common enhancers present | Mode and exact degenerate behavior remain bounded |
| `ImageDraw` | Shapes and default-font text present through `DrawHandle` | Missing `shape`, `getfont`, `getdraw`, broad text layout options, and full font behavior |
| `ImageFont` | Default-font handle exists | `truetype`, `load`, `load_path`, `TransposedFont`, variation axes, RAQM/Harfbuzz shaping are absent |
| `ImageStat` | Basic `Stat` exists | Full property/cache semantics are bounded |
| `ImageColor` | Common color parsing exists | Mostly sufficient for current route |
| `ImageSequence` | Iterator/all_frames exist | Good enough for GIF/TIFF open paths; save-all breadth remains format-specific |
| `ImagePalette` | Absent as a module | Palette data is handled on images, but module factories/utilities are absent |
| `ImageCms` | Public module absent | LittleCMS 2.17 is statically linked for bounded RGB/RGBA/RGBX-to-LAB and LAB-to-RGB/RGBA/RGBX conversion; direct LAB-to-1/L/LA/P/PA/I/F/CMYK/YCbCr/HSV rejection, LAB-to-P WEB/ADAPTIVE and LAB-to-PA ignored-option precedence, native empty adaptive quantization, and public LAB `Image.Quantize` all-method plus bounded integer-kmeans validation order are covered, but profile objects and the general ICC transform/color-management stack remain a major Pillow-replacement gap |

## Format Difference

### Native Core Formats With Tests

These formats have real native/facade test coverage:

- BMP
- PPM-family: PBM, PGM, PNM, PPM, plus bounded high-bit PGM behavior
- QOI
- TGA
- XBM
- ICO
- PNG
- JPEG
- TIFF
- GIF

This is enough for a useful AHK-first image runtime, but not enough for Pillow
format parity.

### High-Value Format Gaps

| Gap | Pillow support | Current state | Route |
| --- | --- | --- | --- |
| WebP | open/save/save_all, local features show WebP animation and mux support | No native/facade path | Add only after dependency and still-vs-animation scope decision |
| AVIF | open/save/save_all registered locally | No native/facade path | Dependency-heavy; keep behind explicit gap ID |
| JPEG2000 | open/save registered locally | No native/facade path | Lower ROI than JPEG metadata unless user needs it |
| PDF | save/save_all registered | No native path | Treat as packaging/export feature, not hot-loop image runtime |
| APNG | PNG extension plus save_all support in Pillow | Current PNG work is still-image focused | Do not mix with PNG metadata tails; make an explicit APNG gap if needed |
| TIFF tags/compression | Pillow supports multipage save, tags, and compression | Bounded multipage save is covered; tags/compression remain open | Continue only as split tag/compression/high-bit/mode children |
| ICO/CUR | Pillow opens CUR and handles multi-entry icon behavior | ICO partial; CUR absent | Split ICO multi-entry from CUR hotspot semantics |
| PCX/DDS/ICNS/DIB/SGI | Pillow supports several practical legacy formats | Mostly absent | Add only one format family at a time with fixtures |

### Long-Tail Format Gaps

The following registered Pillow formats are not current hot-path priorities:

`BLP`, `BUFR`, `DCX`, `EPS`, `FITS`, `FLI`, `FPX`, `FTEX`, `GBR`, `GRIB`,
`HDF5`, `IM`, `IMT`, `IPTC`, `MCIDAS`, `MIC`, `MPEG`, `MPO`, `MSP`, `PALM`,
`PCD`, `PIXAR`, `PSD`, `SPIDER`, `SUN`, `WMF`, `XPM`, `XVTHUMB`.

Do not let these inflate the next work queue. They explain why full Pillow
replacement is still low, but most should stay behind explicit user demand or
packaging decisions.

## Semantic Difference

### Modes

Strong covered modes:

- `1`, `L`, `LA`, `P`, `RGB`, `RGBA`, `CMYK` for many common operations.

Bounded or incomplete modes:

- `I`: raw bytes, Netpbm/high-bit paths, `getextrema()`, and `histogram()`
  exist, but arithmetic, conversion, filtering, and broader file-format
  behavior remain incomplete.
- `F`: raw bytes, scalar access, `getextrema()`, and `histogram()` exist, but
  arithmetic, conversion, filtering, broader NaN/Inf behavior, and save
  restrictions remain incomplete.
- `YCbCr` and `HSV` now have bounded native storage/raw/RGB-conversion parity;
  YCbCr also covers Pillow's direct L/LA targets, composed I/F numeric targets,
  and opaque RGBA expansion through clipped RGB, while HSV accepts RGBA,
  RGBX, CMYK, mode `1`, L, LA, and explicit RGB/RGBA palette sources. `LAB`
  now has native construction, public tuple storage, L/A/B bands, empty-image
  handling, signed-channel raw packing, exact-table grayscale targets,
  LittleCMS-exact RGB/RGBA/RGBX targets, LittleCMS-exact RGB-like source
  conversion, direct L/LA/CMYK/YCbCr/HSV rejection parity including empty
  images, LAB-to-P WEB/ADAPTIVE precedence, LAB-to-PA ignored-option
  precedence, and public all-method `Image.Quantize` validation order over the
  native empty-image quantize route, including unavailable LIBIMAGEQUANT and
  bounded integer-kmeans precedence; other LAB targets, quantize option
  variants, general ImageCms APIs, broader operations, and file participation
  remain open. `PA` now has native P/A
  storage, bands, empty-image handling, explicit RGB/RGBA palette ownership,
  P/PA conversion, P-to-LA/YCbCr, PA-to-RGBA per-pixel-alpha expansion,
  PA-to-RGB/L/LA common targets, PA-to-CMYK/YCbCr/HSV color-space targets, and
  P/PA-to-I/F numeric luma targets with exact int32/float32 storage plus
  P/PA-to-mode-1 direct weighted thresholding and P/PA-to-RGBX expansion.
  RGBX-to-RGB/RGBA/L/LA/CMYK/YCbCr/I/F/mode-1 and bounded empty-palette
  RGB/RGBA behavior are also verified. Remaining RGBX quantizing targets,
  other implicit/default palette families, codecs, default-palette HSV edges,
  and plugin raw modes remain open.
  RGB/RGBA-to-I/F numeric luma conversion is also native and ignores RGBA
  alpha. Mode `1`/`L`/`LA` numeric promotion is native as well: mode `1` maps
  logical values to 0/255, L promotes directly, and LA ignores alpha. YCbCr
  numeric conversion composes clipped RGB expansion with those RGB luma
  kernels; CMYK numeric conversion likewise composes rounded black-scaled RGB
  expansion with the shared luma kernels. HSV numeric conversion composes its
  hue-sector RGB expansion with those same luma kernels, HSV-to-L/LA uses the
  rounded byte-luma form with opaque LA alpha, and HSV-to-RGBA preserves those
  RGB bytes while appending opaque alpha in one native traversal.
  HSV mode-1 NONE/Floyd/default dither now composes RGB thresholding or error
  diffusion inside DLL-owned loops. YCbCr mode-1 now similarly composes clipped
  RGB thresholding/error diffusion instead of direct Y. YCbCr-to-RGBX and
  HSV-to-RGBX now share their respective four-channel RGB expansions with
  X=`255`; YCbCr-to-CMYK now composes clipped RGB with subtractive channels
  and K=`0`, while HSV CMYK and remaining color-space targets stay separate.

Route impact:

- Mode work should be attached to one operation or one format at a time.
- A broad "add all modes everywhere" pass would be slow and hard to verify.

### Metadata

Strong format-specific progress:

- PNG text, iTXt/zTXt, ICC, EXIF, gamma/chromaticity, transparency, and
  metadata combinations through generalized native routes.
- JPEG comment/ICC/EXIF, DPI, qtables, subsampling, quality keep, progressive
  and CMYK/YCCK normalization slices.
- GIF frame metadata, loop/duration/disposal/transparency.
- XBM hotspot and basic resolution metadata.

Remaining Pillow-level metadata gaps:

- Full `Image.Exif` object lifecycle beyond the covered orientation tag:
  dictionary behavior, TIFF tags, nested IFDs, MakerNote preservation, and
  implicit mutation writeback.
- XMP/IPTC.
- Broad arbitrary marker/chunk preservation.
- ICC lifecycle beyond format-local byte preservation.
- `ImageCms` transforms and profile APIs.
- TIFF tags and compression metadata.

Route impact:

- The next metadata abstraction should be driven by a proven `getexif()` or
  TIFF/JPEG preservation child, not by more one-off PNG combinations.
- If the goal is fastest cross-surface progress rather than metadata depth,
  `BYTES-001` should outrank the next metadata child.

### Codecs And Save Options

GIF:

- Strong current focus: animation metadata, disposal, optimized rectangles,
  palette compaction, transparency, exact L/LA/RGB/RGBA quantization slices.
- Remaining: broader quantization algorithms, palette stability, pathological
  animation matrices, APNG/WebP animation not in scope.

PNG:

- Strong current focus: still PNG metadata/options through generalized native
  route.
- Remaining: true compression strategy parity, APNG, arbitrary chunk
  preservation rules, and deeper interop with EXIF/ICC object behavior.
- Route rule: no more one-combination branches unless they prove a new native
  capability or Pillow semantic boundary.

JPEG:

- Strong current focus: JPEG metadata, CMYK/YCCK, qtables, subsampling,
  progressive/optimize, quality keep.
- Remaining: broad marker preservation, explicit metadata option precedence
  across wider keep/progressive matrices, alternate YCCK fixtures, exact codec
  strategy boundaries, JPEG2000 out of scope.
- Covered route decision: opened COM/comment is implicitly preserved on
  keep-style saves, while opened ICC/EXIF are not implicitly preserved unless
  explicitly passed.

TIFF:

- Current: basic WIC-backed open/save, frame count/seek, and bounded native
  uncompressed multipage `save_all` / `append_images`.
- Remaining: broader multipage option/mode interactions, tags, compression,
  high-bit modes, palette/bilevel, `I`/`F`, BigTIFF.

## Route From The Direct Diff

The fastest route should be diff-driven by value and reuse, not by raw missing
count.

### Phase A: Add Missing Semantic Pillars

1. Treat the current `frombuffer` ownership/readonly/stride/orientation
   contract as covered for the highest-value bounded matrix:
   `BYTES-001A` covers raw `L` alias/detach and `BYTES-001B` covers raw
   `L`/`RGBA` mapmode override aliases. `BYTES-001C` covers public raw
   `RGBX` mapmode alias/detach. `BYTES-001D` covers direct `RGB` copy and
   direct `RGBA` stride/orientation alias/detach. Continue only for a concrete
   remaining readonly/detach edge or dependency constructor, not as the
   default next branch.
2. Next bounded `getexif()` / `Image.Exif` object lifecycle child, only when
   the local Pillow oracle proves behavior beyond the covered JPEG/PNG
   orientation tag.
3. Broader `I` and `F` mode operation slices tied to one operation family at a
   time. `MODE-I-001A` covers mode `I` numeric `getextrema()`,
   `MODE-F-001A` covers mode `F` numeric `getextrema()` including later-NaN,
   first-NaN, Inf, and empty behavior, and `MODE-NUM-001B` covers one
   256-bin numeric histogram for `I` and `F` plus masked numeric rejection.
   The selected next child is `MODE-NUM-001C` for numeric `I`/`F -> L`
   conversion. After that, remaining `I`/`F` work should continue with
   arithmetic, stats, or filters as separate oracle-backed children.
4. TIFF bounded multipage save is covered; keep TIFF tags/compression and
   broader mode coverage as separate gaps.

Why this phase matters:

- These gaps block multiple Pillow surfaces at once.
- They improve compatibility more than another narrow PNG metadata combination.

### Phase B: Stabilize Existing Hot Formats

1. GIF: continue only when a local Pillow 11.3.0 probe exposes a concrete
   palette-stability or pathological animation boundary.
2. JPEG: continue only for a newly proven bounded keep/metadata or
   codec-strategy miss, especially broad marker preservation or explicit
   metadata precedence beyond the covered implicit COM/ICC/EXIF rule.
3. PNG: stop adding one-combination branches; extend generalized metadata
   routing and batch same-route cases.

Why this phase matters:

- It moves real compatibility for formats users already expect.
- It keeps native hot loops in the DLL.
- It avoids spending weeks on long-tail plugin inventory.

### Phase C: Dependency-Gated Format Expansion

1. WebP still image open/save first, animation later only if scoped.
2. AVIF only after dependency/package decision.
3. JPEG2000, PCX, DDS, ICNS, CUR as one-format packets with fixtures.
4. PDF save/export only if the package goal expands beyond image runtime.

Why this phase matters:

- It closes visible format gaps without destabilizing the current core.
- It keeps external codec costs explicit.

### Phase D: Full Pillow Replacement Work

1. `ImageCms` and ICC transform pipeline.
2. `ImageFont.truetype`, variation axes, RAQM/Harfbuzz text shaping.
3. `fromarray`/`fromarrow` and optional array dependencies.
4. Long-tail plugins and broad plugin registry semantics.
5. Benchmarks and SIMD/threading after correctness is stable for each hot path.

This phase is required for "full Pillow replacement", but it should not be
mixed into current GIF/PNG/JPEG correctness slices.

## Speed Correction

The slow path is not "Pillow compatibility is inherently branchy"; it is
letting already-known option combinations become separate implementation
branches.

- PNG should not repeat the `FMT-PNG-001AA` pattern. Once a native metadata
  route supports text/ICC/EXIF/transparency-style payloads, same-route
  combinations should be batched in one test packet or documented as already
  covered by the generalized route.
- GIF should not continue by speculative matrix expansion. Add a child only
  when a local Pillow 11.3.0 probe proves a concrete palette-stability or
  pathological animation miss.
- JPEG remains high ROI because keep-save metadata, qtables, subsampling, and
  CMYK/YCCK behavior are close to existing native code and affect common files.
- Reusable semantic pillars such as bounded `getexif()` children, split TIFF
  tag/compression children, and mode-scoped `I`/`F` behavior should outrank
  another narrow file-option combination when they unblock multiple Pillow
  surfaces.

## Practical Next Work Packet

If the next increment should maximize compatibility per unit time, choose one
of these:

1. `MODE-NUM-001C` numeric `I` / `F -> L` conversion: `getextrema()` and
   `histogram()` are covered, and the local Pillow oracle for `Convert("L")`
   is pinned. Add raw/facade red tests, implement through
   `pillow_c_image_convert_mode`, rebuild Release x64, then run targeted plus
   full AHK verification.
2. `META-001` child: only for a newly proven EXIF/TIFF object behavior beyond
   the covered JPEG/PNG orientation read, mutate, native serialize, and
   explicit save writeback path.
3. `FMT-TIFF-002` / `FMT-TIFF-003`: TIFF tag/compression/high-bit/mode child,
   split from the covered bounded multipage save path.
4. `FMT-JPEG-003C` child: only for a newly proven keep/metadata precedence or
   broad marker-preservation miss beyond the covered COM-preserved,
   ICC/EXIF-not-implicit rule.
5. `FMT-GIF-003B` child: only if a concrete Pillow oracle shows a new GIF
   palette-stability or pathological animation miss.

The best default is no longer PNG option branching or another completed
numeric-histogram pass. Start with `MODE-NUM-001C` unless the user explicitly
redirects to `META-001`, split TIFF tag/compression/mode depth, or
dependency-scoped `FMT-WEBP-001`. Continue `BYTES-001` only for a specific
remaining readonly/detach miss with a local Pillow oracle. This is faster than
continuing PNG-style option branches because it attacks reusable semantic
pillars or a high-value format gap rather than another same-route option
combination.
