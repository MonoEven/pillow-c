# pillow-c

![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)
![AutoHotkey](https://img.shields.io/badge/AutoHotkey-v2-green.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey.svg)

A Pillow 11.3.0-compatible image processing library for AutoHotkey v2: a native
`pillow_c.dll` owns image handles, pixel storage, and bulk operations, while
`ahk/pillow.ahk` provides the Python-like facade (`Pillow.Image`, `ImageDraw`,
`ImageFont`, `ImageFilter`, `ImageOps`, `ImageCms`, ...). Behavior is qualified
against the local Python 3.10.11 + Pillow 11.3.0 installation — including exact
error messages, not just the happy path.

Repository: [https://github.com/MonoEven/pillow-c](https://github.com/MonoEven/pillow-c)

Technical pages: [https://monoeven.github.io/pillow-c/](https://monoeven.github.io/pillow-c/)

Practice integration: [https://github.com/MonoEven/cnumpy](https://github.com/MonoEven/cnumpy)

## Contents

- [What it does](#what-it-does)
- [Quick start](#quick-start)
- [Drawing and filters](#drawing-and-filters)
- [NumPy interop](#numpy-interop)
- [ImagePut interop](#imageput-interop)
- [Behavioral parity](#behavioral-parity)
- [Build](#build)
- [Test](#test)
- [Blog and Pages](#blog-and-pages)
- [Project layout](#project-layout)
- [Friendly links](#friendly-links)

## What it does

The center of the project is `build\x64\Release\pillow_c.dll` — a
performance-first C/C++ DLL that owns every image handle, pixel buffer, and
bulk operation, with a stable C ABI of 524 exports. The AHK side stays thin:
argument conversion, lifetime management, exceptions, and Python-like
ergonomics. The facade mirrors Pillow 11.3.0's object model:

- `Pillow.Image`: open/save, convert, resize, rotate, crop, paste, filters,
  pixels, palette, quantize, EXIF/XMP metadata;
- `ImageDraw` / `ImageDraw2` / `ImageFont`: shapes, text, truetype metrics,
  variable-font variation axes;
- `ImageChops` / `ImageOps` / `ImageFilter` / `ImageEnhance`: channel ops,
  whole-image transforms, kernels, enhancers;
- `ImageStat` / `ImageMath` / `ImageCms` / `ImageColor` / `ImagePalette` /
  `ImagePath` / `ImageSequence` / `ImageGrab` / `ImageFile` / `ImageTransform`;
- NumPy interop with [cnumpy](https://github.com/MonoEven/cnumpy):
  `Image.fromarray` / `numpy.asarray(im)` analogues.

This is not a port of any existing AHK Pillow wrapper; it is an independent
implementation constrained by the local Pillow 11.3.0 behavior.

## Quick start

The Release DLL is committed for direct use. Configure the path once, then use
the facade exactly like Pillow:

```autohotkey
#Requires AutoHotkey v2.0
#Include "ahk\pillow.ahk"

Pillow.Configure({ DllPath: A_ScriptDir "\build\x64\Release\pillow_c.dll" })

; open, resize, save
img := Pillow.Image.Open("photo.png")
img := img.Resize([800, 600])
img.Save("photo-800.jpg")
img.Close()

; create and inspect
canvas := Pillow.Image.New("RGB", [320, 200], "white")
canvas.PutPixel([10, 10], "red")
MsgBox canvas.GetPixel([10, 10]).Length      ; 3 (RGB tuple)
canvas.Close()
```

Gradients and statistics:

```autohotkey
gray := Pillow.Image.LinearGradient("L")     ; 256×256, value = y (top to bottom)
hist := gray.Histogram()                     ; 256 entries for L
ext  := gray.GetExtrema()                    ; [min, max]

grad := Pillow.Image.RadialGradient("L")     ; value = distance from centre
grad.Thumbnail([128, 128])
grad.Save("gradient.png")
```

## Drawing and filters

```autohotkey
img := Pillow.Image.New("RGB", [240, 140], "white")
d := Pillow.ImageDraw.Draw(img)
d.Line([[0, 0], [239, 0], [239, 139], [0, 139], [0, 0]], "blue", 3)
d.RoundedRectangle([20, 20, 220, 120], 12, "yellow", "red", 2)
d.Ellipse([60, 40, 180, 100], "lime", "black", 1)
d.Text([12, 118], "Hello", "navy")
img.Save("draw.png")
img.Close()

photo := Pillow.Image.Open("photo.jpg")
blurred := photo.Filter(Pillow.ImageFilter.GaussianBlur(2))
sharp   := photo.Filter(Pillow.ImageFilter.UnsharpMask(2, 150, 3))
k := Pillow.ImageFilter.Kernel([3, 3], [-1,-1,-1, -1,8,-1, -1,-1,-1], 1, 0)
edges := photo.Filter(k)
photo.Close()
```

## NumPy interop

Include the cnumpy facade from your checkout and the two libraries talk
directly — `image.AsArray()` is `numpy.asarray(im)` and
`Pillow.Image.FromArray` is `Image.fromarray`, byte-exact against NumPy 1.25.0:

```autohotkey
#Include "..\2026-07-19-cnumpy-foundation\ahk\numpy.ahk"

Numpy.DllPath := A_ScriptDir "\..\2026-07-19-cnumpy-foundation\build\x64\Release\cnumpy_ahk.dll"
Numpy.Init()

img := Pillow.Image.Open("photo.jpg")
a := img.AsArray()                 ; uint8 NdArray, [H, W, 3]
mean := a.Mean()                   ; cnumpy statistics
img.Close()                        ; the array snapshot stays valid

b := Numpy.Zeros([64, 64, 4], 3)   ; uint8 RGBA
im := Pillow.Image.FromArray(b)    ; mode RGBA
im.Save("black.png")
im.Close()

Numpy.Cleanup()
```

## ImagePut interop

[ImagePut](https://github.com/iseahound/ImagePut) is an optional companion
library for AutoHotkey v2 that handles many image sources and destinations
(files, URLs, clipboard, windows, screen regions, GDI+ bitmaps, ...).
`pillow.ahk` detects it with `IsSet(ImagePut)`; no load-time dependency is
added. When ImagePut is included, you can use pillow-c as the processing
engine inside an ImagePut-driven workflow:

```autohotkey
#Include "third_party\ImagePut\ImagePut.ahk"
#Include "ahk\pillow.ahk"

Pillow.Configure({ DllPath: A_ScriptDir "\build\x64\Release\pillow_c.dll" })

; ImagePut handles input.
myImage := ImagePutBuffer("image.png")

; pillow-c performs the pixelation/averaging step.
p := Pillow.Image.FromImagePut(myImage, "RGB")
small := p.Resize([4, 4], Pillow.Resampling.BOX)
pixelated := small.Resize([200, 200], Pillow.Resampling.NEAREST)

; Hand the result back to ImagePut for display/save.
result := pixelated.ToImagePut()
ImagePutWindow({ image: result })
; ImagePutFile(result, "pixelated.png")

pixelated.Close()
small.Close()
p.Close()
```

The bridge functions are `Pillow.Image.FromImagePut(source, mode := "RGBA")`
and `Pillow.Image.ToImagePut(image)` / `image.ToImagePut()`. They convert
through ImagePut's 32-bit ARGB buffer layout, so the two libraries can share
pixels without ImagePut being modified. `FromImagePut` validates its input
with `ImageCheckSafe`: raw pointers, handles and monitor numbers are rejected;
safe sources are file paths, URLs, clipboard data, screenshot region arrays
(`[x, y, w, h]`), and ImagePut buffers. Including `pillow.ahk` without
ImagePut adds no load-time warnings and calling the bridge raises a clear
"requires ImagePut" error.

## Behavioral parity

Behavior is verified against local Pillow 11.3.0
(`F:\Python\Python310\python.exe`) through oracle probes plus the full
AutoHotkey suite: **2870/2870** tests, **524/524** DLL exports in parity,
Release x64 DLL SHA-256
`2E6F0ABBAC878ACC483E6EE2B81F8C5810CD1144174C7E291C0BCCAA887DCC1E`. Error
messages, option validation, conversion rules, metadata shapes, and numeric
edge cases are pinned against the Python implementation. The few remaining
honest boundaries (WebP/AVIF/JPEG2000 decoders, FPX, ImageQt/ImageTk,
ImagePalette.random, the ImagePath map handler, I;16B big-endian AsArray, ...)
fail loudly with Pillow's exact messages — see
[Boundaries](pages/en/reference/boundaries.html).

## Build

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\src\pillow_c.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

## Test

Tests use `ahktest` from [MonoEven/stdlib-ahk](https://github.com/MonoEven/stdlib-ahk). In the current local workspace, run tests from the parent `visual_studio` directory with its `tools` runner so AHK errors are captured instead of shown in blocking popups. Every AHK test run should include `-TimeoutSeconds 120`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run-ahktest.ps1 -Target .\tasks\2026-06-07-pillow-c-foundation\ahk -Report .codex\pillow-c-report.txt -TimeoutSeconds 120
```

## Blog and Pages

The repository separates the blog from the technical Pages site.

- Blog: bilingual introduction posts in forum BBCode format.
  - `blog_pillow_c_en.txt`
  - `blog_pillow_c.txt`
- Pages: the API reference modeled on the official Pillow docs — every
  public API with parameters, return values, error messages, and
  behavioral boundaries. English and Chinese are two fully separate site
  trees: `pages/en/` and `pages/zh/`, with a language chooser at
  [https://monoeven.github.io/pillow-c/](https://monoeven.github.io/pillow-c/)
  (locally: open `pages/index.html` and pick a language). Served from the
  `pages/` directory via the Pages workflow in
  `.github/workflows/deploy-pages.yml`.

Engineer-facing ledgers:

- [Architecture](docs/architecture.md)
- [Native ABI](docs/native-abi.md)
- [Gap Checkpoint](docs/pillow-gap-checkpoint.md)
- [Gap Analysis](docs/pillow-gap-analysis.md)
- [Testing](docs/testing.md)

## Project layout

```text
ahk/
  pillow.ahk            facade (single entry point)
  pillow.test.ahk       full behavior suite
  pillow_numpy.test.ahk cnumpy interop suite
  pillow_demo.test.ahk  executes every README/blog example
src/                    native C/C++ (pillow_c.dll)
build/x64/Release/      committed Release DLL
oracle/                 Pillow 11.3.0 + NumPy 1.25.0 behavior probes
docs/                   engineer ledgers
pages/                  API reference site (en/ and zh/ trees + chooser)
```

## Friendly links

- [MonoEven/stdlib-ahk](https://github.com/MonoEven/stdlib-ahk)
- [MonoEven/cnumpy](https://github.com/MonoEven/cnumpy) — the NumPy interop partner
- [iseahound/ImagePut](https://github.com/iseahound/ImagePut) — the ImagePut interop partner
- [MonoEven/ahk-hack-library](https://github.com/MonoEven/ahk-hack-library)
- [Linux.do](https://linux.do/)
- [AutoHotkey Community Forum](https://www.autohotkey.com/boards/)
