# JPEG Extra Marker Stream Implementation Plan

> **For agentic workers:** This plan is executed inline in the current session because the user explicitly prohibited subagents. Each checkbox is a vertical red→green verification slice.

**Goal:** Add a DLL-owned JPEG save route that preserves Pillow 11.3.0's raw `extra` marker stream for the bounded L/RGB/CMYK option combinations selected in `FMT-JPEG-002B2T`.

**Architecture:** Reuse the existing native JPEG option encoders, then insert the caller-provided marker bytes at the existing JFIF/Adobe boundary inside the DLL. The AHK facade will normalize the `Extra`/`extra` option, retain its Buffer for the native call, and route only the documented bounded combinations; unsupported metadata/qtables/keep-rgb/restart compositions remain explicit native-boundary errors.

**Tech Stack:** C++17 MSVC Release x64 DLL, Windows Imaging Component/libjpeg-compatible native JPEG encoders, AutoHotkey v2 facade/tests, ahktest, Pillow 11.3.0 local oracle.

---

### Task 1: Raw DLL tracer test

**Files:**
- Modify: `ahk/pillow_c.test.ahk`

- [x] Add one raw test for a 16x16 RGB image and the exact stream `FF ED 00 08 54 45 53 54 01 02 FF FE 00 08 45 58 54 52 41 21`; assert APP13/COM payloads, their order after APP0/Adobe and before DQT, and successful native JPEG open.
- [x] Run the single raw test with the repository runner and `-TimeoutSeconds 120`; confirm RED because `pillow_c_image_save_jpeg_extra_options` is not yet exported.

### Task 2: Facade tracer test

**Files:**
- Modify: `ahk/pillow.test.ahk`

- [x] Add one public `Image.Save(..., "JPEG", {extra: Buffer(...)})` test using the same marker stream; assert marker payload/order and reopen the output through `Image.Open`.
- [x] Run the single facade test with `-TimeoutSeconds 120`; confirm RED because the facade has no `extra` route yet.

### Task 3: Native marker patch seam and export

**Files:**
- Modify: `src/pillow_c_codec_jpeg_internal.h`
- Modify: `src/pillow_c_codec_jpeg_common.cpp`
- Modify: `src/pillow_c_codec_jpeg_save.cpp`
- Modify: `src/pillow_c.vcxproj` only if the existing JPEG save/common units are not already listed

- [x] Declare `patch_jpeg_extra_segments(const char*, const uint8_t*, size_t)` beside the existing metadata patch seam.
- [x] Insert the caller bytes unchanged at `jpeg_metadata_insert_position`, after JFIF/Adobe headers and before DQT, using the existing binary file read/write seams and without an AHK-side rewrite. Pillow's `_save()` copies `extra` verbatim, so this route must not reinterpret or silently normalize the stream.
- [x] Implement additive export `pillow_c_image_save_jpeg_extra_options(image, path, quality, has_dpi, dpi_x, dpi_y, subsampling, progressive, optimize, extra, extra_size)` by calling `save_jpeg_image_with_options` and then `patch_jpeg_extra_segments`; preserve existing source-comment behavior for opened JPEG comments.

### Task 4: Native green and facade routing

**Files:**
- Modify: `ahk/pillow.ahk`
- Modify: `ahk/pillow_c.test.ahk` only for additional option rows after the tracer is green
- Modify: `ahk/pillow.test.ahk` only for additional public rows after the tracer is green

- [x] Add `Extra`/`extra` option normalization through `Pillow.Image.BinaryBuffer`.
- [x] Route only RGB/L/CMYK JPEG saves with `extra` and no explicit comment/ICC/EXIF/XMP, qtables, keep-rgb, or restart options; pass quality, DPI, subsampling, progressive, and optimize values to the new export and return immediately.
- [x] Reject the out-of-scope compositions explicitly through the existing facade error style; do not silently discard `extra`.
- [x] Run the raw and facade tracer tests; confirm GREEN.

### Task 5: Bounded option matrix

**Files:**
- Modify: `ahk/pillow_c.test.ahk`
- Modify: `ahk/pillow.test.ahk`

- [x] Add baseline, optimize, progressive, DPI, and subsampling rows for RGB and L; add one CMYK row, checking marker order/payload and native reopenability.
- [x] Probe Pillow only for any newly ambiguous row and record the exact observed behavior in the ledger.
- [x] Run the raw/facade JPEG filters with `-TimeoutSeconds 120`.

### Task 6: Rebuild, full verification, and documentation

**Files:**
- Modify: `docs/pillow-gap-checkpoint.md`
- Modify: `docs/pillow-gap-analysis.md`
- Modify: `docs/native-abi.md`
- Modify: `docs/testing.md` if the new raw helper/test category changes the documented test inventory

- [x] Rebuild Release x64 with the repository MSBuild command and verify zero warnings/errors.
- [x] Verify source/DLL export parity and record the new count and DLL SHA-256.
- [x] Run the targeted tests and then the complete AHK directory suite with `-TimeoutSeconds 240`.
- [x] Record `FMT-JPEG-002B2T` as covered only for the bounded combinations above, and list metadata+extra, qtables+extra, keep-rgb+extra, restart+extra, structural-marker rejection, and exact entropy/file parity as remaining boundaries.
