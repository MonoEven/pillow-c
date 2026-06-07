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
- `image.Transpose(...)`
- `image.Convert(...)`
- static helpers such as `Pillow.Image.Blend(...)`

AHK owns ergonomics and lifetime. The DLL owns image bytes and transformations.

## Performance Direction

Scalar behavior must stay tested before SIMD or threading is introduced. Later optimizations should happen behind the same ABI so wrapper code does not change when the native backend improves.
