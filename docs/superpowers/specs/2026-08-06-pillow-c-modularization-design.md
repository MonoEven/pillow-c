# pillow_c.dll Multi-Translation-Unit Modularization

## Decision

Adopt a staged, formal multi-translation-unit split for `pillow_c.dll`. The
native ABI remains unchanged: exported symbol names, parameter layouts, status
codes, handle ownership, and pointer lifetime rules must remain compatible with
the current AHK facade. The first implementation wave extracts the shared
image/memory model, metadata, TIFF codec, and ABI seam, then uses the resulting
seams to advance the selected `FMT-TIFF-001AZ` compatibility child.

The split is by domain responsibility rather than by arbitrary file size. The
existing monolith is retained only as a temporary migration source; it is not a
second runtime or a fallback implementation.

## Evidence and Reference Sources

The current native source is approximately 1.83 MB and defines 445 exported
functions in one translation unit. The project file currently compiles only
`src/pillow_c.cpp` plus the statically linked lcms2 C sources. Its authoritative
Release configuration is MSVC v143, C++17, x64, Windows SDK 10.0, WIC
(`windowscodecs.lib`), `ole32.lib`, and lcms2 include paths.

Behavior references are:

- Local Pillow 11.3.0 Python sources under
  `F:\Python\Python310\lib\site-packages\PIL`, especially
  `TiffImagePlugin.py`, `JpegImagePlugin.py`, `PngImagePlugin.py`, `Image.py`,
  and `ImageFile.py`.
- Pillow 11.3.0 upstream `src/libImaging/Imaging.h` and
  `src/libImaging/ImagingUtils.h` for storage, arithmetic, and utility
  conventions.
- Pillow 11.3.0 upstream `setup.py` and `pyproject.toml` for the standard
  optional dependency/build matrix. These are references, not files to copy
  into the AHK runtime.
- The repository's native architecture document and the gap ledger remain the
  project-specific authority for ABI, lifetime, and compatibility scope.

Pillow `libImaging` will not be copied wholesale. Its CPython extension
integration and dependency assumptions do not match the WIC-backed Windows
runtime. Pure algorithms or data-layout ideas may be reimplemented only when a
bounded Pillow oracle and raw/facade regression test require them.

## Target Modules

The intended module set is:

```text
src/
  pillow_c_internal.h       shared internal Interface and declarations
  pillow_c_core.cpp        image handles, modes, allocation, ownership
  pillow_c_memory.cpp      byte storage, endian helpers, buffer views
  pillow_c_metadata.cpp    EXIF, XMP, ICC, metadata ownership/serialization
  pillow_c_codec_wic.cpp   WIC factories, streams, converters, shared codec IO
  pillow_c_codec_tiff.cpp  TIFF IFDs, strips, compression, multiframe layout
  pillow_c_codec_jpeg.cpp  JPEG markers, tables, scans, restart state
  pillow_c_codec_png.cpp   PNG chunks, metadata, compression routing
  pillow_c_codec_legacy.cpp BMP, PPM, QOI, TGA, XBM, and ICO containers
  pillow_c_codec_gif.cpp   GIF frames, disposal, transparency
  pillow_c_ops.cpp         conversions, compositing, ImageChops operations
  pillow_c_filters.cpp     filters and LUT operations
  pillow_c_transform.cpp   resize, rotate, affine, perspective, mesh
  pillow_c_draw.cpp        drawing, fonts, gradients, effects
  pillow_c_cms.cpp         lcms2 profiles and transforms
  pillow_c_abi.cpp         exported ABI adapters and status translation
```

The first wave creates and wires only `pillow_c_internal.h`,
`pillow_c_core.cpp`, `pillow_c_memory.cpp`, `pillow_c_metadata.cpp`,
`pillow_c_codec_tiff.cpp`, and `pillow_c_abi.cpp` as needed for the TIFF slice.
The JPEG, PNG, GIF, operation, filter, transform, drawing, and CMS modules are
subsequent migration waves and do not get speculative interfaces in the first
wave.

## Interface and Seam Rules

`pillow_c_internal.h` is an internal Interface, not a public ABI header. It
contains the shared `PillowCImage` representation, mode constants, metadata
containers, status constants, ownership helpers, and declarations required by
more than one module. It must not expose WIC implementation details to pure
storage or metadata code.

The ABI seam is the existing `extern "C"` surface. Its adapters validate
arguments, call internal implementations, transfer output handles, and return
the established status codes. No adapter may contain an AHK loop or duplicate a
codec algorithm.

The codec seams are concrete internal functions first. A generic codec
interface will be introduced only when two independent adapters need the same
contract; a premature registry or plugin abstraction would add shallow code
without leverage. WIC remains the current adapter for Windows codec operations,
while native TIFF/JPEG/PNG code remains responsible for Pillow-specific byte
parity where WIC cannot provide it.

Each extracted Module must have:

1. one clear responsibility;
2. a small Interface listing ownership, input invariants, output ownership, and
   status/error behavior;
3. no dependency on AHK or facade implementation details;
4. raw tests through the existing DLL ABI when public behavior is involved;
5. no duplicate implementation left in the migration source.

## Migration Order

1. Capture the current baseline: full AHK suite, source/DLL export manifest,
   Release DLL hash, and the selected TIFF targeted filters.
2. Introduce `pillow_c_internal.h` without changing behavior or exports.
3. Move core image storage and lifetime implementation into
   `pillow_c_core.cpp`; keep callers on the same internal contract.
4. Move endian/buffer/owned-storage helpers into `pillow_c_memory.cpp`.
5. Move shared EXIF/XMP/ICC metadata ownership and serializers into
   `pillow_c_metadata.cpp`.
6. Move the native TIFF writer/parser and the `FMT-TIFF-001AZ` route into
   `pillow_c_codec_tiff.cpp`. The three-frame child remains bounded by its
   Pillow oracle; arbitrary frame-count generalization is out of scope for this
   wave.
7. Move the related exported wrappers into `pillow_c_abi.cpp`, preserving every
   existing decorated-name-free `pillow_c_*` symbol and its signature.
8. Remove the moved definitions from the monolith and update the vcxproj with
   explicit source entries. Do not use wildcard C++ inclusion or duplicate
   source compilation.
9. Rebuild Release x64, compare source and DLL exports, run TIFF targeted raw
   and facade tests, then run the complete AHK suite with a 240-second timeout.
10. Record the final module list, ABI result, gap evidence, and DLL hash in the
    checkpoint, gap analysis, native ABI, and testing documents.

## Verification Contract

The migration is acceptable only when all of the following are freshly proven:

- `FMT-TIFF-001AY` remains green during every extraction checkpoint.
- The new `FMT-TIFF-001AZ` raw and facade tests first fail before the behavior
  implementation is added, then pass after the native implementation is wired.
- The Release x64 build has zero warnings and zero errors.
- Source and DLL export manifests remain identical at `445/445`.
- The full AHK directory suite passes with zero failures, errors, or skips.
- No implemented hot path gains an AHK per-pixel loop.
- No fallback, mock, silent degradation, new cap, or swallowed error is added.
- A failed module build or ABI mismatch is exposed directly and fixed at the
  module seam rather than bypassed.

The modularization itself is not counted as compatibility coverage. The coverage
increment is counted only when the bounded TIFF child and its tests are green.

## Non-Goals

- Rewriting all 445 exports in one pass.
- Importing all of Pillow's C sources or its CPython extension layer.
- Introducing a new public ABI solely to make the split easier.
- Adding a generic codec registry before a second real adapter requires it.
- Moving JPEG, PNG, GIF, drawing, filters, transforms, or CMS code before the
  TIFF wave is verified.
- Claiming one-day completion of the entire Pillow compatibility surface.

## Success Shape

After the first wave, a maintainer should be able to change TIFF metadata or
multiframe layout by reading the TIFF codec, metadata, internal contract, and
ABI adapter modules rather than searching the entire monolith. The DLL should
still expose the same public behavior, and the next bounded gap should be
implementable by reusing those deep Modules instead of adding another isolated
branch to `pillow_c.cpp`.
