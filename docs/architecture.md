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

The future `pillow.ahk` layer should feel close to Python Pillow:

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

Core `1`, `L`, `LA`, `RGB`, `RGBA`, `P`, and `CMYK` conversion paths, native BMP/PPM/PNG/JPEG/TIFF/GIF file open/save, native `1`/`L`/`P` linear and radial gradient generation, native Mandelbrot, noise, and spread effect generation, `CMYK` handle/raw-byte operations, mode `1` bit-packed byte import/export and logical chops, native `P` RGB/RGBA palette metadata including Pillow-style `L.putpalette(...) -> P`, native P/L palette remapping, histogram/extrema/entropy/bounding-box/projection/color-count scans, histogram-backed `ImageStat.Stat` properties with mode `1`/`L` mask support, fixed-LUT and histogram-derived `ImageOps` transforms for supported Pillow modes including mode `1` invert, `ImageOps.deform` dispatch through native MESH transforms, `ImageOps.colorize` L-to-RGB mapping, `Image.eval`/`Image.point` LUT mapping, `Image.Filter(ImageFilter.Kernel(...))`, `Image.Filter(ImageFilter.RankFilter(...))`, `Image.Filter(ImageFilter.ModeFilter(...))`, `Image.Filter(ImageFilter.BoxBlur(...))`, `Image.Filter(ImageFilter.GaussianBlur(...))`, `Image.Filter(ImageFilter.UnsharpMask(...))`, `Image.Filter(ImageFilter.Color3DLUT(...))`, ImageEnhance composition over native blend/convert/filter/stat operations, ImageDraw rectangle, rounded rectangle, bitmap, floodfill, ellipse, arc, chord, pieslice, line, point, and polygon mutation, `ImageOps.crop`/`ImageOps.expand`, direct box-aware and `reducing_gap`-aware `Image.resize`, scaled, proportional, fitted, padded, and integer-reduced resize helpers, current `ImageChops` helpers and binary operations including verified `LA` and `CMYK`, mask compositing, image paste, masked paste, color-source paste, RGBA alpha compositing including in-place destination/source geometry, masked equalize, masked and preserve-tone autocontrast histograms, L-band split, L-band merge, Python-like AFFINE/EXTENT/PERSPECTIVE/QUAD/MESH transform dispatch, general NEAREST/BILINEAR/BICUBIC affine, perspective, quad, and mesh transforms, NEAREST/BILINEAR/BICUBIC affine rotate, and all current Pillow resize and filter hot paths are single native operations so the wrapper does not fall back to per-pixel AHK loops as mode coverage grows.

`ImageStat.Stat` currently supports image inputs, image inputs with same-size mode `1` or `L` masks, and precomputed histogram lists. Its properties follow Pillow's histogram-derived formulas for `extrema`, `count`, `sum`, `sum2`, `mean`, `median`, `rms`, `var`, and `stddev`. A mask on a histogram list is rejected because the native mask path requires image storage.

`ImageOps.equalize` and `ImageOps.autocontrast` currently implement common histogram/LUT paths with mode `1` or `L` masks for supported `L`/`RGB` images. Equalize also mirrors Pillow's mode `P` special case by converting through the native RGB palette and returning `RGB`. Autocontrast also supports `cutoff`, `ignore`, and Pillow's `preserve_tone` mode.

Resize behavior follows Pillow 11.3.0 for the supported 8-bit modes, including verified CMYK coverage. `NEAREST` uses Pillow's affine-scale coordinate progression. `BOX`, `BILINEAR`, `HAMMING`, `BICUBIC`, and `LANCZOS` use separable two-pass filtering with Pillow-style fixed-point coefficient normalization. `Image.Resize(size, resample, box)` maps directly to the native box-resize path so AHK does not need a crop intermediate before resampling. `Image.Resize(size, resample, box, reducingGap)` also stays in the DLL: for large downsampling it computes Pillow's safe reduce box, performs native integer reduction, and runs final box resize against the reduced temporary. `Image.Thumbnail(...)` follows Pillow's aspect-preserving in-place API at the facade layer and delegates the actual resampling to those native resize paths. Non-NEAREST `LA` and `RGBA` resize use premultiplied color internally and preserve identity resizes as byte copies.

`Image.frombytes` and `Image.tobytes` keep raw byte import/export in the DLL for common interop layouts such as mode `1` bit-packed rows, direct `CMYK`, BGR, BGRA, ARGB, ABGR, RGBX, BGRX, and bottom-up stride-based source rows. Mode `1` stays unpacked as one byte per pixel inside the native handle for fast bulk operations and memory sharing, while facade `ToBytes()` returns Pillow's bit-packed external representation.

`Image.open` and `image.save` expose native file-format paths for BMP, PPM, QOI, PNG, JPEG, TIFF, and GIF. The BMP layer parses and writes uncompressed Windows BMP files, including Pillow-compatible 24-bit RGB save bytes, 8-bit grayscale BMP, and Pillow-style 32-bit RGBA BMP saves that reopen as RGB. The PPM layer parses plain and binary Netpbm `P1`/`P2`/`P3`/`P4`/`P5`/`P6` files into `1`, `L`, and `RGB` handles, including PBM/Pillow mode `1` bit inversion and Pillow-style low-`maxval` scaling; grayscale high-bit-depth Netpbm waits on native `I` mode support. Native save writes Pillow's default binary `P4`/`P5`/`P6` output. The QOI layer implements native RGB/RGBA Quite OK Image decode/encode with Pillow-compatible byte output. The PNG layer keeps `L`, `LA`, `P`, `RGB`, and `RGBA` decode/encode work inside the DLL; WIC handles the common codec path while native chunk writing preserves Pillow-style `LA`, palette-mode PNG semantics, stored `compress_level=0`, and metadata chunks that WIC does not reliably preserve for this project. Facade `compress_level=0` uses the native chunk writer for stored zlib output across those supported PNG modes, facade `dpi`/`Dpi` writes a PNG `pHYs` chunk through the extensible `pillow_c_image_save_png_options` ABI, and PNG open reads that `pHYs` metadata back into `info["dpi"]`. Nonzero compression levels currently reuse the existing encoder path unless metadata forces native chunk writing; a tuned native deflate strategy is still future work. The JPEG layer keeps lossy `L` and `RGB` decode/encode in the DLL through WIC, with component-count probing so grayscale JPEGs reopen as `L` instead of being promoted by the wrapper. Facade `quality` and `dpi`/`Dpi` route through `pillow_c_image_save_jpeg_options`: quality stays in WIC's encoder property bag, while JFIF density is patched in native code after the encoder releases the file so wrapper code does not perform byte-level JPEG edits. JPEG open reuses the native marker scan to expose Pillow-style `info["dpi"]`, `info["jfif"]`, `info["jfif_version"]`, `info["jfif_unit"]`, and `info["jfif_density"]` without AHK-side byte parsing. The TIFF layer keeps WIC-backed `L`, `RGB`, and `RGBA` open/save in the DLL for lossless interchange and exposes native frame count plus frame-open ABI so the facade can implement `n_frames`, `is_animated`, `tell`, `seek`, and `ImageSequence.Iterator` for basic multiframe TIFF files. The GIF layer opens frame `0` as mode `P` with palette metadata, opens verified later frames as `RGB`, saves `P` directly, saves exact-color `L`/`RGB` single-frame images by native palette quantization, and exposes native frame count, frame-open, metadata, single-frame save, and simple same-palette animation save ABI. The facade updates `info["duration"]`, `info["loop"]`, `info["background"]`, and `disposal_method` from DLL-parsed GIF blocks on open and seek, and maps `Image.Save(..., { SaveAll, AppendImages, Duration, Loop, Disposal })` to a single native GIF animation write for the supported same-size `P`-mode sequence path. Full animation composition, frame rectangle optimization, transparency optimization, RGBA GIF save, and general lossy GIF quantization remain future surfaces.

`Image.getdata` and `Image.putdata` expose Pillow-like pixel sequence ergonomics while keeping the native handle as the storage authority. `GetData` exports a bulk byte snapshot, and `PutData` packs AHK values once before calling the native `put_data` prefix-writer instead of crossing the DLL boundary per pixel.

`ImageColor.getrgb` and `ImageColor.getcolor` live in the AHK facade because color parsing is call-boundary normalization, not an image hot path. The facade follows Pillow's named CSS colors, hex, `rgb(...)`, `rgba(...)`, `hsl(...)`, and `hsv(...)` string parsing, converts to the target mode once, and then passes caller-packed bytes into native fill, draw, paste, expand, pad, and transform operations.

`ImageOps.expand`, `ImageOps.pad`, and related facade fill parsing support Pillow-style scalar and tuple fill colors for current core modes, including `LA` single-value fills as transparent luminance and two-value `[l, a]` fills before dispatching to native geometry paths.

`ImageDraw.Draw(image).Rectangle(...)` mutates the image handle in place through a native rectangle path. Fill, outline, width, inclusive coordinates, clipping, and reversed-coordinate rejection are modeled from Pillow 11.3.0's `ImageDraw.rectangle` wrapper and `ImagingDrawRectangle` C implementation. `ImageDraw.Draw(image).RoundedRectangle(...)` keeps Pillow's rounded-rectangle composition inside one native call, using native pieslice/arc corners plus rectangle bars and native rectangle/ellipse fallbacks for degenerate cases. `ImageDraw.Draw(image).Bitmap(...)` follows Pillow's `ImagingFill2` mask rules for mode `1`, `L`, and `RGBA` masks while keeping color fill and alpha blending in the DLL. `ImageDraw.floodfill(...)` keeps Pillow's Python flood-fill semantics in a native queue walk, including threshold matching, border-mode filling, seed negative-coordinate normalization, and no-op out-of-range seeds. `ImageDraw.Draw(image).Ellipse(...)` uses Pillow 11.3.0's integer span generator for fill and outline widths while keeping clipping in the native hot path. `ImageDraw.Draw(image).Arc(...)`, `ImageDraw.Draw(image).Chord(...)`, and `ImageDraw.Draw(image).Pieslice(...)` reuse that span foundation plus Pillow's clip-ellipse half-plane tree, including angle normalization, full-circle delegation to ellipse paths, width handling, chord/pie side outlines, center joins, and clipping. `ImageDraw.Draw(image).Line(...)` covers Pillow's ordinary multi-segment line path, including `width <= 1` final endpoint draw, `width > 1` quadrilateral segment filling, clipping behavior from `draw_lines`/`ImagingDrawLine`/`ImagingDrawWideLine`, and `joint="curve"` rounded joints for wide polylines by adding Pillow-style pieslice joints and gap-cover lines inside the DLL. `ImageDraw.Draw(image).Point(...)` batches Pillow's individual point writes, allowing empty and single-point coordinate lists while clipping out-of-bounds points. `ImageDraw.Draw(image).Polygon(...)` covers fill, `width <= 1` outlines, two-point line-like polygons, and Pillow's mask-assisted wide outline path using a native polygon mask plus `width * 2 - 1` wide-line strokes clipped to the filled polygon interior. The AHK facade only packs colors and dispatches one DLL call for covered draw operations.

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
