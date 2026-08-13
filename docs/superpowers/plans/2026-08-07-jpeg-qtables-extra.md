# FMT-JPEG-002B2T-QTABLES Implementation Plan

> **For inline execution:** This bounded plan is executed in the current workspace without subagents, staging, committing, or pushing.

**Goal:** Extend the native JPEG qtables save surface so Pillow-compatible `extra` marker bytes can be composed with custom qtables without an AHK-side pixel loop.

**Architecture:** Reuse the existing DLL-owned qtables encoder for `L`, `RGB`, and bounded `CMYK` output, then call the existing one-shot marker composer inside `pillow_c_codec_jpeg_common.cpp`. Add one additive export carrying qtables plus metadata/extra fields; keep the existing qtables and metadata exports unchanged. The facade normalizes `qtables` and `extra`, retains every Buffer through `DllCall`, and routes the new export only for the supported combination.

**Tech Stack:** MSVC Release x64, C++17 native DLL, AutoHotkey v2 facade/tests, Pillow 11.3.0 cached source and local oracle.

---

### Task 1: Establish the red contract

**Files:**
- Modify: `ahk/pillow_c.test.ahk`
- Modify: `ahk/pillow.test.ahk`

- [x] Add one raw test that calls the new qtables-plus-extra export for RGB and asserts the APP marker appears before DQT.
- [x] Add one facade test with a custom two-table RGB save and the same raw marker assertion.
- [x] Run both files and confirm failure is caused by the missing export/facade boundary.

### Task 2: Implement the native route

**Files:**
- Modify: `src/pillow_c_codec_jpeg_internal.h`
- Modify: `src/pillow_c_codec_jpeg_common.cpp`
- Modify: `src/pillow_c_codec_jpeg_save.cpp`
- Modify: `src/pillow_c.vcxproj`
- Modify: `docs/native-abi.md`

- [x] Declare an additive qtables-plus-extra C++ seam and exported ABI with qtables count, metadata buffers, codec options, and raw extra bytes.
- [x] Validate all borrowed pointers and qtable counts explicitly.
- [x] Reuse `save_jpeg_rgb_qtables_optimized_huffman` with CMYK dispatch enabled, then insert the ordered metadata/extra group once through the existing native composer.
- [x] Keep the existing qtables, keep-rgb, restart, and metadata ABI symbols behaviorally unchanged.

### Task 3: Route the public facade

**Files:**
- Modify: `ahk/pillow.ahk`

- [x] Allow `extra` with qtables when `keep_rgb` and restart markers are absent.
- [x] Normalize comment/ICC/EXIF/XMP/extra and qtables buffers once and retain them through `DllCall`.
- [x] Route the new export for qtables plus extra, and preserve explicit errors for unsupported qtables-plus-keep-rgb/restart compositions.

### Task 4: Verify and broaden the bounded matrix

**Files:**
- Modify: `ahk/pillow_c.test.ahk`
- Modify: `ahk/pillow.test.ahk`
- Modify: `docs/pillow-gap-checkpoint.md`
- Modify: `docs/pillow-gap-analysis.md`
- Modify: `docs/native-abi.md`
- Modify: `docs/testing.md`

- [x] Rebuild Release x64 and confirm zero warnings/errors plus source/DLL export parity.
- [x] Run targeted raw and facade files, then the full AHK directory suite with `-TimeoutSeconds 240`.
- [x] Add the bounded `L`, `RGB`, and `CMYK` qtables-plus-extra cases that the existing encoder supports, including one metadata composition and one progressive/optimized case where the oracle is stable.
- [x] Record the cached source hash, test counts, ABI export count, and remaining qtables-extra boundaries in all required docs.

No commit operation is part of this plan because the workspace already contains the user's uncommitted migration work and the current execution constraints prohibit staging or committing it.
