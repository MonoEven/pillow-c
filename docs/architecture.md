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
- `Pillow.Image.FromBytes(...)`
- `image.Fill(...)`
- `image.Crop(...)`
- `image.Resize(...)`
- `image.Transpose(...)`
- `image.Convert(...)`
- `image.Point(...)`
- `Pillow.ImageOps.Invert(...)`
- `Pillow.ImageOps.Posterize(...)`
- `Pillow.ImageOps.Solarize(...)`
- `Pillow.ImageOps.Equalize(...)`
- `Pillow.ImageOps.Autocontrast(...)`
- `Pillow.ImageOps.Expand(...)`
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
- `Pillow.ImageChops.Offset(...)`
- `image.GetChannel(...)`
- `image.Split(...)`
- `image.PutAlpha(...)`
- `Pillow.Image.Composite(...)`
- `Pillow.Image.Eval(...)`
- `Pillow.ImageChops.Blend(...)`
- `Pillow.ImageChops.Composite(...)`
- static helpers such as `Pillow.Image.Blend(...)` and `Pillow.Image.Merge(...)`

AHK owns ergonomics and lifetime. The DLL owns image bytes and transformations.

Core `L`, `RGB`, and `RGBA` conversions, histogram/extrema scans, fixed-LUT and histogram-derived `ImageOps` transforms for supported `L`/`RGB` modes, `Image.eval`/`Image.point` LUT mapping, `ImageOps.expand`, proportional, fitted, and padded `ImageOps` resize helpers, current `ImageChops` helpers and binary operations, mask compositing, L-band split, L-band merge, and all current Pillow resize filters are single native operations so the wrapper does not fall back to per-pixel AHK loops as mode coverage grows.

`ImageOps.autocontrast` currently implements the common histogram/LUT path with `cutoff` and `ignore`. Masked histograms and `preserve_tone` remain future ABI work.

Resize behavior follows Pillow 11.3.0 for the supported 8-bit modes. `NEAREST` uses Pillow's affine-scale coordinate progression. `BOX`, `BILINEAR`, `HAMMING`, `BICUBIC`, and `LANCZOS` use separable two-pass filtering with Pillow-style fixed-point coefficient normalization. Non-NEAREST `RGBA` resize uses premultiplied color internally and preserves identity resizes as byte copies.

## Performance Direction

Scalar behavior must stay tested before SIMD or threading is introduced. Later optimizations should happen behind the same ABI so wrapper code does not change when the native backend improves.
