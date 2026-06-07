# pillow_c.dll Foundation

This task starts the native imaging core for the Pillow-to-AHK work.

It does not claim that Pillow has been fully reproduced. It establishes a verified `pillow_c.dll` build/test path and the first byte-buffer primitives for a performance-first native implementation.

`pillow_c.dll` is not a C translation of an existing AHK implementation. It should be designed as the primary image-processing engine. AHK v2 code should call it through thin bindings and should not define the internal architecture.

## Behavior Oracle

Behavior is constrained by local Python 3.10.11 and Pillow 11.3.0:

```text
Python: F:\Python\Python310\python.exe
Pillow: 11.3.0
```

The fixture generator is:

```text
oracle\pillow_oracle.py
```

It writes:

```text
oracle\pillow_oracle.json
```

The initial algorithms were checked against local Pillow behavior and Pillow 11.3.0 source under `src/libImaging`:

- `Blend.c`
- `Convert.c`
- `AlphaComposite.c`

The downloaded source used for inspection is temporary local evidence under `.codex/` and is not part of the repository.

## Exported ABI

All functions return:

```text
0   success
-1  null pointer
-2  invalid length
-3  invalid argument
-4  allocation failed
-5  mismatch
```

Current exports:

```cpp
extern "C" __declspec(dllexport) int pillow_c_abi_version(
    int* out_major,
    int* out_minor,
    int* out_patch);

extern "C" __declspec(dllexport) int pillow_c_status_message(
    int status,
    char* out,
    size_t out_size,
    size_t* out_required);

extern "C" __declspec(dllexport) int pillow_c_mode_from_string(
    const char* mode_name,
    int* out_mode);

extern "C" __declspec(dllexport) int pillow_c_mode_name(
    int mode,
    char* out,
    size_t out_size,
    size_t* out_required);

extern "C" __declspec(dllexport) int pillow_c_blend_u8(
    const uint8_t* left,
    const uint8_t* right,
    uint8_t* out,
    size_t count,
    double alpha);

extern "C" __declspec(dllexport) int pillow_c_rgb_to_l(
    const uint8_t* rgb,
    uint8_t* out,
    size_t pixels);

extern "C" __declspec(dllexport) int pillow_c_alpha_composite_rgba(
    const uint8_t* dst,
    const uint8_t* src,
    uint8_t* out,
    size_t pixels);
```

Native image handle exports:

```cpp
extern "C" __declspec(dllexport) int pillow_c_image_create(
    int width,
    int height,
    int channels,
    PillowCImage** out_image);

extern "C" __declspec(dllexport) int pillow_c_image_create_mode(
    int width,
    int height,
    int mode,
    PillowCImage** out_image);

extern "C" __declspec(dllexport) int pillow_c_image_free(PillowCImage* image);

extern "C" __declspec(dllexport) int pillow_c_image_width(const PillowCImage* image, int* out_width);
extern "C" __declspec(dllexport) int pillow_c_image_height(const PillowCImage* image, int* out_height);
extern "C" __declspec(dllexport) int pillow_c_image_mode(const PillowCImage* image, int* out_mode);
extern "C" __declspec(dllexport) int pillow_c_image_channels(const PillowCImage* image, int* out_channels);
extern "C" __declspec(dllexport) int pillow_c_image_stride(const PillowCImage* image, int* out_stride);
extern "C" __declspec(dllexport) int pillow_c_image_size(const PillowCImage* image, size_t* out_size);

extern "C" __declspec(dllexport) int pillow_c_image_data(
    PillowCImage* image,
    uint8_t** out_data,
    size_t* out_size);

extern "C" __declspec(dllexport) int pillow_c_image_set_bytes(
    PillowCImage* image,
    const uint8_t* data,
    size_t size);

extern "C" __declspec(dllexport) int pillow_c_image_get_bytes(
    const PillowCImage* image,
    uint8_t* out,
    size_t size);

extern "C" __declspec(dllexport) int pillow_c_image_copy(
    const PillowCImage* source,
    PillowCImage** out_image);

extern "C" __declspec(dllexport) int pillow_c_image_blend(
    const PillowCImage* left,
    const PillowCImage* right,
    double alpha,
    PillowCImage** out_image);

extern "C" __declspec(dllexport) int pillow_c_image_rgb_to_l(
    const PillowCImage* source,
    PillowCImage** out_image);

extern "C" __declspec(dllexport) int pillow_c_image_alpha_composite_rgba(
    const PillowCImage* dst,
    const PillowCImage* src,
    PillowCImage** out_image);

extern "C" __declspec(dllexport) int pillow_c_image_crop(
    const PillowCImage* source,
    int left,
    int top,
    int right,
    int bottom,
    PillowCImage** out_image);

extern "C" __declspec(dllexport) int pillow_c_image_paste(
    PillowCImage* target,
    const PillowCImage* source,
    int left,
    int top);

extern "C" __declspec(dllexport) int pillow_c_image_transpose(
    const PillowCImage* source,
    int method,
    PillowCImage** out_image);

extern "C" __declspec(dllexport) int pillow_c_image_copy_into(
    const PillowCImage* source,
    PillowCImage* target);

extern "C" __declspec(dllexport) int pillow_c_image_blend_into(
    const PillowCImage* left,
    const PillowCImage* right,
    double alpha,
    PillowCImage* target);

extern "C" __declspec(dllexport) int pillow_c_image_rgb_to_l_into(
    const PillowCImage* source,
    PillowCImage* target);

extern "C" __declspec(dllexport) int pillow_c_image_alpha_composite_rgba_into(
    const PillowCImage* dst,
    const PillowCImage* src,
    PillowCImage* target);

extern "C" __declspec(dllexport) int pillow_c_image_crop_into(
    const PillowCImage* source,
    int left,
    int top,
    int right,
    int bottom,
    PillowCImage* target);

extern "C" __declspec(dllexport) int pillow_c_image_transpose_into(
    const PillowCImage* source,
    int method,
    PillowCImage* target);
```

`pillow_c_abi_version` starts the ABI at `0.1.0`. `pillow_c_status_message` maps status codes to stable UTF-8 text so `pillow.ahk` can raise idiomatic exceptions without hard-coding C status text.

The current core mode ids are:

```text
1 L
3 RGB
4 RGBA
```

`pillow_c_mode_from_string`, `pillow_c_mode_name`, `pillow_c_image_create_mode`, and `pillow_c_image_mode` make image handles mode-aware for the future Python-like wrapper. The legacy `pillow_c_image_create(width, height, channels, ...)` remains available and maps channels `1/3/4` to `L/RGB/RGBA`.

`pillow_c_image_data` exists for AHK-specific memory sharing. The returned pointer is valid only while the image handle remains alive and no API reallocates its pixel storage.

`pillow_c_image_crop` follows Pillow's box semantics for the tested raw byte modes: out-of-source pixels are zero-filled, and zero-width or zero-height results are valid handles with empty pixel storage. Inverted boxes return `-3`.

`pillow_c_image_paste` is the no-mask raw fast path. It clips the source region to the target bounds and copies row spans in place. This initial fast path requires matching channel counts and returns `-5` for channel mismatch instead of doing Pillow mode conversion.

`pillow_c_image_transpose` uses Pillow 11.3.0 `Image.Transpose` method values:

```text
0 FLIP_LEFT_RIGHT
1 FLIP_TOP_BOTTOM
2 ROTATE_90
3 ROTATE_180
4 ROTATE_270
5 TRANSPOSE
6 TRANSVERSE
```

It returns `-3` for unsupported method values and preserves Pillow's tested zero-dimension metadata behavior.

The `*_into` exports are allocation-avoidance fast paths for the future Python-like AHK wrapper. They write into an existing target image handle and return `-5` when the target shape cannot hold the result. AHK can use these to build temporary-handle pools and chained APIs without allocating a fresh native image for every intermediate step.

## Build

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\src\pillow_c.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Output:

```text
build\x64\Release\pillow_c.dll
```

## Test

Regenerate the oracle fixture:

```powershell
py -3.10 .\oracle\pillow_oracle.py
```

Run the AHK DLL tests:

```powershell
powershell -ExecutionPolicy Bypass -File ..\..\tools\run-ahktest.ps1 -Target .\tasks\2026-06-07-pillow-c-foundation\ahk\pillow_c.test.ahk -Report .codex\pillow-c-green-report.txt -TimeoutSeconds 20
```

Passing result:

```text
Ran 49 tests in 750ms
Passed: 49, Failed: 0, Errors: 0, Skipped: 0
```

## AHK Facade

The first `pillow.ahk` facade is:

```text
ahk\pillow.ahk
```

It is intentionally thin. It gives AHK users an object-oriented entry point while keeping image storage and pixel movement inside `pillow_c.dll`.

Currently verified facade surface:

- `Pillow.Configure({ DllPath })`
- `Pillow.AbiVersion()`
- `Pillow.Image.New(mode, [width, height])`
- `Pillow.Image.FromBytes(mode, [width, height], buffer)`
- `image.Mode`
- `image.Size`
- `image.Width`
- `image.Height`
- `image.Channels`
- `image.ByteSize`
- `image.ToBytes()`
- `image.DataPointer()`
- `image.Close()`

`Image.FromBytes` copies caller bytes into native storage. `DataPointer()` exposes the native handle's memory with the same lifetime rule as `pillow_c_image_data`: the pointer is valid only while the image remains open and the underlying storage is not reallocated.

## Performance-First Direction

Future work should grow `pillow_c.dll` as a native imaging core:

- Own image buffers in native memory where practical.
- Prefer contiguous row-major byte buffers with explicit width, height, stride, mode, and channel count.
- Keep image handles mode-aware. Channels are storage layout; mode is wrapper-visible Pillow semantics.
- Keep AHK calls coarse-grained. Avoid one `DllCall` per pixel.
- Support AHK-friendly memory sharing through handle-owned pointers with strict lifetime rules.
- Use in-place and out-of-place variants where that reduces allocations.
- Prefer reusable-output `*_into` variants for wrapper internals and hot chains.
- Shape the ABI so a future Python-like `pillow.ahk` wrapper can remain thin: AHK should provide ergonomic classes, chained methods, named constants, and exceptions, while `pillow_c.dll` owns handles, intermediate images, and pixel movement.
- Add SIMD and multithreading only behind stable scalar behavior and tests.
- Keep behavior constrained by local Python 3.10.11 + Pillow 11.3.0, not by any existing AHK implementation.

The next step should continue adding Pillow-compatible operations around the native handle model, with AHK v2 kept as a thin wrapper over those handles.
