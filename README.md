# pillow-c

`pillow-c` is the native acceleration layer for bringing a Pillow-like API to AutoHotkey v2.

The project centers on `pillow_c.dll`: a performance-first C/C++ DLL that owns image handles, pixel storage, and bulk image operations. AHK code should stay thin: argument conversion, lifetime management, exceptions, and Python-like ergonomics.

This is not a port of any existing AHK Pillow wrapper. Behavior is constrained by local Python 3.10.11 and Pillow 11.3.0.

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
img := Pillow.Image.New("RGB", [2, 1])
bytes := img.ToBytes()
img.Close()
```

## Build

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\src\pillow_c.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

## Test

Tests use `ahktest` from [MonoEven/stdlib-ahk](https://github.com/MonoEven/stdlib-ahk). In the current local workspace, run tests from the parent `visual_studio` directory with its `tools` runner so AHK errors are captured instead of shown in blocking popups. Every AHK test run should include `-TimeoutSeconds 120`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run-ahktest.ps1 -Target .\tasks\2026-06-07-pillow-c-foundation\ahk -Report .codex\pillow-c-report.txt -TimeoutSeconds 120
```

## Docs

- [Architecture](docs/architecture.md)
- [Native ABI](docs/native-abi.md)
- [Testing](docs/testing.md)

## Current Surface

The verified native surface currently covers mode-aware image handles and facade band names including `CMYK`, BMP/PNG/JPEG/TIFF/GIF file open/save, byte import/export including `1` bit-packed raw bytes, direct `CMYK`, common raw BGR/BGRA-style modes, native `1`/`L`/`P` gradient generation, native Mandelbrot effect generation, Pillow-like getdata/putdata facade operations, P/L palette remapping, solid fill, data-pointer sharing, copy, blend, composite, ImageChops helper/binary/logical operations including LA/CMYK binary coverage, histogram-backed entropy and ImageStat including native L-mask histograms, extrema, ImageOps transforms including `deform` through native MESH, masked equalize, masked and preserve-tone autocontrast, L-to-RGB colorize, expand/contain/cover/fit/pad, LUT point transforms, channel extraction, split/merge, alpha insertion, `1`/`L`/`LA`/`RGB`/`RGBA`/`P`/`CMYK` conversion paths, Pillow-compatible resize filters with direct box sampling, `reducing_gap`, and integer reduce including CMYK coverage, Python-like `ImageFilter.Kernel`, rank filters, `ModeFilter`, `BoxBlur`, `GaussianBlur`, and `UnsharpMask`, ImageEnhance facade composition including RGBA and CMYK `Color`, ImageDraw rectangle, ellipse, arc, chord, line, point, and polygon mutation, AFFINE/EXTENT/PERSPECTIVE/QUAD/MESH transform dispatch, general NEAREST/BILINEAR/BICUBIC affine, perspective, quad, and mesh transforms including CMYK geometry, NEAREST/BILINEAR/BICUBIC affine rotate, RGBA alpha composite including in-place geometry, crop, image paste, masked paste, color-source paste, transpose, and allocation-avoiding `*_into` variants.

The AHK facade is intentionally smaller and grows as the Python-like wrapper stabilizes.

## Friendly Links

- [MonoEven/stdlib-ahk](https://github.com/MonoEven/stdlib-ahk)
- [Linux.do](https://linux.do/)
- [AutoHotkey Community Forum](https://www.autohotkey.com/boards/)
