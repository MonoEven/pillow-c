# PNG Codec Module Extraction Implementation Plan

> **For agentic workers:** This plan is executed inline in the current session because the user explicitly prohibited subagents.

**Goal:** Move the complete PNG codec implementation and PNG public exports out of `pillow_c.cpp` into a real independently compiled translation unit while preserving the existing ABI and behavior.

**Architecture:** `pillow_c_codec_png.cpp` owns PNG header/chunk parsing, zlib inflate/stored-deflate helpers used by PNG/TIFF, WIC PNG decode, native PNG chunk construction, metadata/transparency handling, all PNG save option normalization, and the existing PNG exports. `pillow_c.cpp` retains image/core/other-codec behavior and no longer defines PNG routes. Shared image, endian, file, WIC, EXIF, and status contracts remain in the existing internal headers.

**Tech Stack:** MSVC v143, C++17, Windows Imaging Component, existing native `PillowCImage` ABI, AutoHotkey v2 ahktest, Pillow 11.3.0 oracle fixtures.

---

### Task 1: Lock the ownership seam with a failing structural test

**Files:**
- Modify: `tasks/2026-06-07-pillow-c-foundation/ahk/pillow_c.test.ahk`

- [x] Add one test that requires the new PNG translation unit, requires the PNG open/export symbols in that file, and requires the old PNG implementation markers to be absent from the main unit.
- [x] Run the filtered test and observe the expected failure because the file and seam do not exist yet.

### Task 2: Extract the PNG implementation mechanically as one ownership unit

**Files:**
- Create: `tasks/2026-06-07-pillow-c-foundation/src/pillow_c_codec_png.cpp`
- Modify: `tasks/2026-06-07-pillow-c-foundation/src/pillow_c.cpp`

- [x] Copy the existing PNG helper block (including the deflate reader/inflater and PNG metadata parsers), PNG open/save implementation block, PNG zlib internal wrappers, and PNG export block into the new translation unit.
- [x] Keep `GifMetadata` and GIF code in the main unit; do not leave duplicate PNG definitions.
- [x] Compile until all cross-unit dependencies are explicit in `pillow_c_internal.h` or `pillow_c_wic_internal.h`; do not add wrapper fallbacks or duplicate implementations.

### Task 3: Register and build the new translation unit

**Files:**
- Modify: `tasks/2026-06-07-pillow-c-foundation/src/pillow_c.vcxproj`

- [x] Add `pillow_c_codec_png.cpp` to the Release/Debug source item group beside the existing codec units.
- [x] Build Release x64 and verify zero warnings/errors and unchanged source/DLL export parity.

### Task 4: Verify behavior and ownership

**Files:**
- Modify: `tasks/2026-06-07-pillow-c-foundation/docs/architecture.md`
- Modify: `tasks/2026-06-07-pillow-c-foundation/docs/native-abi.md`
- Modify: `tasks/2026-06-07-pillow-c-foundation/docs/pillow-gap-checkpoint.md`
- Modify: `tasks/2026-06-07-pillow-c-foundation/docs/pillow-gap-analysis.md`
- Modify: `tasks/2026-06-07-pillow-c-foundation/docs/testing.md`

- [x] Run the PNG-focused raw/facade tests, then the complete AHK directory suite with `-TimeoutSeconds 240`.
- [x] Confirm the rebuilt `build/x64/Release/pillow_c.dll` is current and export parity is unchanged.
- [x] Record the real translation-unit ownership and measured line counts; do not increase the Pillow compatibility percentage because this is architecture work.
