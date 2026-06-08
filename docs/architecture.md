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
- `Pillow.Image.FromBytes(...)`
- `Pillow.Image.Open(...)`
- `image.ToBytes(...)`
- `image.Save(...)`
- `image.Fill(...)`
- `image.Crop(...)`
- `image.Resize(..., box := ...)`
- `image.Reduce(...)`
- `image.Filter(Pillow.ImageFilter.Kernel(...))`
- `image.Transform(...)`
- `image.TransformAffine(...)`
- `image.Rotate(...)`
- `image.Transpose(...)`
- `image.Convert(...)`
- `image.Point(...)`
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

Core `1`, `L`, `LA`, `RGB`, `RGBA`, `P`, and `CMYK` conversion paths, native BMP/PNG/JPEG/TIFF/GIF file open/save, native `1`/`L`/`P` linear and radial gradient generation, native Mandelbrot effect generation, `CMYK` handle/raw-byte operations, mode `1` bit-packed byte import/export and logical chops, native P/L palette remapping, histogram/extrema/entropy/bounding-box/projection/color-count scans, histogram-backed `ImageStat.Stat` properties with mode `1`/`L` mask support, fixed-LUT and histogram-derived `ImageOps` transforms for supported Pillow modes including mode `1` invert, `ImageOps.deform` dispatch through native MESH transforms, `ImageOps.colorize` L-to-RGB mapping, `Image.eval`/`Image.point` LUT mapping, `Image.Filter(ImageFilter.Kernel(...))`, `Image.Filter(ImageFilter.RankFilter(...))`, `Image.Filter(ImageFilter.ModeFilter(...))`, `Image.Filter(ImageFilter.BoxBlur(...))`, `Image.Filter(ImageFilter.GaussianBlur(...))`, `Image.Filter(ImageFilter.UnsharpMask(...))`, ImageEnhance composition over native blend/convert/filter/stat operations, ImageDraw rectangle and line mutation, `ImageOps.crop`/`ImageOps.expand`, direct box-aware and `reducing_gap`-aware `Image.resize`, scaled, proportional, fitted, padded, and integer-reduced resize helpers, current `ImageChops` helpers and binary operations including verified `LA` and `CMYK`, mask compositing, image paste, masked paste, color-source paste, RGBA alpha compositing including in-place destination/source geometry, masked equalize, masked and preserve-tone autocontrast histograms, L-band split, L-band merge, Python-like AFFINE/EXTENT/PERSPECTIVE/QUAD/MESH transform dispatch, general NEAREST/BILINEAR/BICUBIC affine, perspective, quad, and mesh transforms, NEAREST/BILINEAR/BICUBIC affine rotate, and all current Pillow resize filters are single native operations so the wrapper does not fall back to per-pixel AHK loops as mode coverage grows.

`ImageStat.Stat` currently supports image inputs, image inputs with same-size mode `1` or `L` masks, and precomputed histogram lists. Its properties follow Pillow's histogram-derived formulas for `extrema`, `count`, `sum`, `sum2`, `mean`, `median`, `rms`, `var`, and `stddev`. A mask on a histogram list is rejected because the native mask path requires image storage.

`ImageOps.equalize` and `ImageOps.autocontrast` currently implement common histogram/LUT paths with mode `1` or `L` masks for supported `L`/`RGB` images. Equalize also mirrors Pillow's mode `P` special case by converting through the native RGB palette and returning `RGB`. Autocontrast also supports `cutoff`, `ignore`, and Pillow's `preserve_tone` mode.

Resize behavior follows Pillow 11.3.0 for the supported 8-bit modes, including verified CMYK coverage. `NEAREST` uses Pillow's affine-scale coordinate progression. `BOX`, `BILINEAR`, `HAMMING`, `BICUBIC`, and `LANCZOS` use separable two-pass filtering with Pillow-style fixed-point coefficient normalization. `Image.Resize(size, resample, box)` maps directly to the native box-resize path so AHK does not need a crop intermediate before resampling. `Image.Resize(size, resample, box, reducingGap)` also stays in the DLL: for large downsampling it computes Pillow's safe reduce box, performs native integer reduction, and runs final box resize against the reduced temporary. Non-NEAREST `LA` and `RGBA` resize use premultiplied color internally and preserve identity resizes as byte copies.

`Image.frombytes` and `Image.tobytes` keep raw byte import/export in the DLL for common interop layouts such as mode `1` bit-packed rows, direct `CMYK`, BGR, BGRA, ARGB, ABGR, RGBX, BGRX, and bottom-up stride-based source rows. Mode `1` stays unpacked as one byte per pixel inside the native handle for fast bulk operations and memory sharing, while facade `ToBytes()` returns Pillow's bit-packed external representation.

`Image.open` and `image.save` expose native file-format paths for BMP, PNG, JPEG, TIFF, and GIF. The BMP layer parses and writes uncompressed Windows BMP files, including Pillow-compatible 24-bit RGB save bytes, 8-bit grayscale BMP, and Pillow-style 32-bit RGBA BMP saves that reopen as RGB. The PNG layer keeps `L`, `LA`, `P`, `RGB`, and `RGBA` decode/encode work inside the DLL; WIC handles the common codec path while native chunk writing preserves Pillow-style `LA` and palette-mode PNG semantics. The JPEG layer keeps lossy `L` and `RGB` decode/encode in the DLL through WIC, with component-count probing so grayscale JPEGs reopen as `L` instead of being promoted by the wrapper. The TIFF layer keeps uncompressed WIC-backed `L`, `RGB`, and `RGBA` open/save in the DLL for lossless interchange. The GIF layer currently covers single-frame `P` mode open/save and keeps palette metadata attached to the native handle.

`Image.getdata` and `Image.putdata` expose Pillow-like pixel sequence ergonomics while keeping the native handle as the storage authority. `GetData` exports a bulk byte snapshot, and `PutData` packs AHK values once before calling the native `put_data` prefix-writer instead of crossing the DLL boundary per pixel.

`ImageOps.expand`, `ImageOps.pad`, and related facade fill parsing support Pillow-style scalar and tuple fill colors for current core modes, including `LA` single-value fills as transparent luminance and two-value `[l, a]` fills before dispatching to native geometry paths.

`ImageDraw.Draw(image).Rectangle(...)` mutates the image handle in place through a native rectangle path. Fill, outline, width, inclusive coordinates, clipping, and reversed-coordinate rejection are modeled from Pillow 11.3.0's `ImageDraw.rectangle` wrapper and `ImagingDrawRectangle` C implementation. `ImageDraw.Draw(image).Line(...)` currently covers Pillow's ordinary `width <= 1` multi-segment line path, including the final endpoint draw and clipping behavior from `draw_lines`/`ImagingDrawLine`. Wide-line polygon filling and `joint="curve"` are separate native surfaces. The AHK facade only packs colors and dispatches one DLL call for covered draw operations.

`Image.reduce` supports native integer block downsampling for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`, including Pillow-style output-size ceiling, optional box regions, and allocation-avoiding `_into` calls. `LA` and `RGBA` reduce through Pillow's premultiplied-alpha semantics before converting back to the public mode.

`ImageFilter.Kernel` currently supports Pillow's 3x3 and 5x5 kernel path for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. Native filtering copies border pixels unchanged, applies Pillow's vertical kernel flip, and uses Pillow-style half-up rounding before clipping. Pillow's fixed-kernel built-ins `BLUR`, `CONTOUR`, `DETAIL`, `EDGE_ENHANCE`, `EDGE_ENHANCE_MORE`, `EMBOSS`, `FIND_EDGES`, `SHARPEN`, `SMOOTH`, and `SMOOTH_MORE` reuse the same native kernel path.

`ImageFilter.RankFilter` and the `MinFilter`, `MedianFilter`, and `MaxFilter` helpers currently support arbitrary positive odd sizes for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. Native rank filtering clamps source coordinates at image edges, matching Pillow's rank-filter behavior for windows larger than the image.

`ImageFilter.ModeFilter` currently supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. It follows Pillow's single-band `ModeFilter.c` semantics independently per channel: outside-image pixels are ignored, even sizes use `size // 2` as the radius, sparse values with counts of one or two preserve the original pixel, and ties keep the smaller pixel value.

`ImageFilter.BoxBlur` currently supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. It uses Pillow's separable fixed-point box blur, including fractional radius weighting and endpoint edge extension. This native path is the shared primitive for Gaussian blur and future unsharp-mask work instead of adding AHK-side pixel loops.

`ImageFilter.GaussianBlur` currently supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. It follows Pillow's three-pass Gaussian approximation by transforming the requested radius into a BoxBlur radius, running horizontal passes before vertical passes, and preserving allocation-avoiding `_into` behavior for native callers.

`ImageFilter.UnsharpMask` currently supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. It reuses the native GaussianBlur path, then applies Pillow's per-channel `abs(original - blurred) > threshold` rule with integer `percent` strength and byte clipping.

`ImageEnhance.Brightness`, `Contrast`, `Sharpness`, and `Color` follow Pillow's degenerate-image plus blend model. The facade composes existing native image operations instead of AHK pixel loops: blend, mode conversion, histogram mean, `ImageFilter.SMOOTH`, alpha reinsertion, and byte clipping all stay in the native path. `Color` supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`; RGBA uses Pillow's `LA` intermediate mode so alpha is preserved while color is desaturated, while CMYK uses Pillow's `L` to `CMYK` degenerate path.

`Image.rotate` currently supports Pillow-style geometry, expansion, center, translate, and fill color for `NEAREST`, `BILINEAR`, and `BICUBIC`, including verified CMYK fill-color packing and sampling plus `LA`/`RGBA` premultiplied filtered sampling. Additional rotate resamplers should build on the same affine ABI instead of adding wrapper loops.

`Image.Transform(...)` covers Pillow's `AFFINE`, `EXTENT`, `PERSPECTIVE`, `QUAD`, and `MESH` methods through native paths for `NEAREST`, `BILINEAR`, and `BICUBIC`. `Image.TransformAffine` remains as the lower-level affine convenience entry, CMYK is covered by the same channel-generic native geometry path, and `LA`/`RGBA` filtered transforms sample color in premultiplied-alpha space.

## Performance Direction

Scalar behavior must stay tested before SIMD or threading is introduced. Later optimizations should happen behind the same ABI so wrapper code does not change when the native backend improves.
