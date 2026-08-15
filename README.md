# pillow-c

`pillow-c` is the native acceleration layer for bringing a Pillow-like API to AutoHotkey v2.

The project centers on `pillow_c.dll`: a performance-first C/C++ DLL that owns image handles, pixel storage, and bulk image operations. AHK code should stay thin: argument conversion, lifetime management, exceptions, and Python-like ergonomics.

This is not a port of any existing AHK Pillow wrapper. Behavior is constrained by local Python 3.10.11 and Pillow 11.3.0.

## Documentation

Bilingual (English / 中文) API documentation, modeled on the official Pillow docs and covering every public API down to its parameters, return values, error messages, and behavioral boundaries:

- [pillow-c docs site](pages/index.html) — open locally or serve `pages/` via GitHub Pages
  - [Image](pages/reference/Image.html) · [ImageDraw](pages/reference/ImageDraw.html) · [ImageFont](pages/reference/ImageFont.html) · [ImageFilter](pages/reference/ImageFilter.html) · [ImageChops](pages/reference/ImageChops.html) · [ImageOps](pages/reference/ImageOps.html) · [ImageStat](pages/reference/ImageStat.html) · [ImageMath](pages/reference/ImageMath.html) · [ImageCms](pages/reference/ImageCms.html) · [ImageColor](pages/reference/ImageColor.html) · [ImageEnhance](pages/reference/ImageEnhance.html) · [ImagePalette](pages/reference/ImagePalette.html) · [ImagePath](pages/reference/ImagePath.html) · [ImageDraw2](pages/reference/ImageDraw2.html) · [ImageSequence](pages/reference/ImageSequence.html) · [ImageGrab](pages/reference/ImageGrab.html) · [ImageFile](pages/reference/ImageFile.html) · [ImageTransform](pages/reference/ImageTransform.html) · [ImageQt](pages/reference/ImageQt.html) · [ImageTk](pages/reference/ImageTk.html) · [Numpy interop](pages/reference/Numpy.html) · [Constants](pages/reference/constants.html) · [Boundaries](pages/reference/boundaries.html)
  - Engineer-facing ledgers: [Architecture](docs/architecture.md) · [Native ABI](docs/native-abi.md) · [Gap Checkpoint](docs/pillow-gap-checkpoint.md) · [Gap Analysis](docs/pillow-gap-analysis.md) · [Testing](docs/testing.md)

## NumPy interop (cnumpy)

pillow-c links with [**cnumpy**](https://github.com/MonoEven/cnumpy) (the companion NumPy-style native array library for AutoHotkey v2, `v1.21.0-cnumpy`): `Image.FromArray` is Pillow's `Image.fromarray`, and `image.AsArray()` is `numpy.asarray(im)`. dtype/shape mappings, byte layouts and error shapes are oracle-verified against local NumPy 1.25.0 + Pillow 11.3.0 (`oracle/probe_numpy_interop.py`).

```autohotkey
#Include "ahk\pillow.ahk"
#Include "..\2026-07-19-cnumpy-foundation\ahk\numpy.ahk"   ; your cnumpy checkout

Pillow.Configure({ DllPath: A_ScriptDir "\build\x64\Release\pillow_c.dll" })
img := Pillow.Image.Open("photo.jpg")
a := img.AsArray()                    ; uint8 NdArray, [H, W, 3] — numpy.asarray(im)
img.Close()
b := Numpy.Zeros([64, 64, 4], 3)     ; uint8 RGBA
im := Pillow.Image.FromArray(b)      ; mode RGBA — Image.fromarray
```

## Quick Start

The current Release DLL is committed for direct use:

```text
build\x64\Release\pillow_c.dll
```

AHK facade entry point:

```text
ahk\pillow.ahk
```

Minimal AHK sketch:

```autohotkey
#Requires AutoHotkey v2.0
#Include "ahk\pillow.ahk"

Pillow.Configure({ DllPath: A_ScriptDir "\build\x64\Release\pillow_c.dll" })
img := Pillow.Image.Open("photo.png")
img := img.Resize([800, 600])
img.Save("photo-800.jpg")
img.Close()
```

## Behavioral Parity

Behavior is verified against local Pillow 11.3.0 (`F:\Python\Python310\python.exe`) through oracle probes plus the full AutoHotkey suite: **2867/2867** tests, **524/524** DLL exports in parity, Release x64 DLL SHA-256 `4B8ABA30335284C99C776B81E0924777A30A644D22BEE6610115C4BC6C4A2481`. Error messages, option validation, conversion rules, metadata shapes, and numeric edge cases are pinned against the Python implementation. The few remaining honest boundaries (WebP/AVIF/JPEG2000 decoders, FPX, ImageQt/ImageTk, ImagePalette.random, the ImagePath map handler, I;16B big-endian AsArray, …) fail loudly with Pillow's exact messages — see [Boundaries](pages/reference/boundaries.html).

## Build

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\src\pillow_c.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

## Test

Tests use `ahktest` from [MonoEven/stdlib-ahk](https://github.com/MonoEven/stdlib-ahk). In the current local workspace, run tests from the parent `visual_studio` directory with its `tools` runner so AHK errors are captured instead of shown in blocking popups. Every AHK test run should include `-TimeoutSeconds 120`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run-ahktest.ps1 -Target .\tasks\2026-06-07-pillow-c-foundation\ahk -Report .codex\pillow-c-report.txt -TimeoutSeconds 120
```

## Friendly Links

- [MonoEven/stdlib-ahk](https://github.com/MonoEven/stdlib-ahk)
- [MonoEven/cnumpy](https://github.com/MonoEven/cnumpy) — the NumPy interop partner
- [Linux.do](https://linux.do/)
- [AutoHotkey Community Forum](https://www.autohotkey.com/boards/)
