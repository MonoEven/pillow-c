# Pillow Direct Difference Assessment

Date: 2026-06-20

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

- AHK-first high-performance runtime: about `52-55%`.
- Full Pillow replacement: `26-31%`.
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
| Pillow module surface | mixed | `ImageOps`/`ImageChops` are broad by name; `ImageFont.truetype`, `ImageCms`, `ImagePalette`, and dependency constructors are major gaps |
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
| `ImageCms` | Absent | Full ICC transform/color-management stack is a major Pillow-replacement gap |

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
- `YCbCr`, `LAB`, `HSV`, and plugin raw modes are not real parity surfaces yet.

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
