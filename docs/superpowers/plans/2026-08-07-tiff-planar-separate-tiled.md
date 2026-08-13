# FMT-TIFF-003S Planar-Separate Tiled Open Implementation Plan

> Inline execution plan. No subagents, staging, commits, pushes, parent-tool
> edits, mock paths, silent fallbacks, or AHK pixel loops.

**Goal:** Extend the native TIFF tiled-open route to Pillow-compatible
planar-separate storage for the bounded `L`, `RGB`, `RGBA`, and `LA` matrix.

**Architecture:** Keep the existing `pillow_c_image_open_tiff` and
`pillow_c_image_frame_count_tiff` ABI unchanged. The parser will recognize
`PlanarConfiguration=2`, interpret the LONG tile arrays in plane-major order,
decode each plane tile inside the DLL, and interleave only while copying into
DLL-owned public image storage. The facade remains unchanged except for
exercising the public open route.

**Tech Stack:** C++17 MSVC Release x64, Pillow 11.3.0 local oracle,
AutoHotkey v2 `ahktest`, existing TIFF LZW/PackBits/zlib seams.

---

## Task 1: Pin the oracle and fixture contract

- [x] Pin the 4×3 image, 2×2 tiles, four supported public storage modes,
  `PlanarConfiguration=2`, plane-major tile order, and edge padding from
  Pillow 11.3.0.
- [x] Keep RGBX planar storage outside this slice because the local Pillow
  oracle does not load the bounded four-plane `ExtraSamples=0` shape.

## Task 2: Add the RED tests

**Files:** `ahk/pillow_c.test.ahk`, `ahk/pillow.test.ahk`

- [x] Add one fixture builder per harness for uncompressed planar-separate
  `L`, `RGB`, `RGBA`, and `LA` storage. Assert mode, size, frame count, and
  exact public bytes for all four modes in one raw test and one facade test.
- [x] Run both tests with the chunky-only DLL and record the expected native
  `-3`/facade invalid-argument RED.

## Task 3: Implement the native plane-major route

**File:** `src/pillow_c_codec_tiff.cpp`

- [x] Admit `PlanarConfiguration=2` only for the bounded tiled matrix.
- [x] Validate tile-array count as `tile_columns * tile_rows * channels`.
- [x] Decode each plane/tile payload using the existing native compression
  seams, then interleave valid edge samples into the public image buffer.
- [x] Preserve strip dispatch, chunky dispatch, RGBX handling, frame count,
  status codes, ownership, and all existing ABI signatures.

## Task 4: Verify and document

- [x] Rebuild Release x64; run raw/facade targets and TIFF filters with
  `-TimeoutSeconds 120`; run the full `ahk` directory suite with
  `-TimeoutSeconds 240`.
- [x] Record RED/GREEN evidence, rebuild output, export parity, DLL hash,
  no-ABI/no-AHK-loop facts, and the next explicit boundary in the checkpoint,
  gap analysis, ABI, and testing docs.
