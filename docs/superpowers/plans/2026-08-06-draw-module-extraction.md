# Drawing and Font Module Extraction Implementation Plan

> **For agentic workers:** Execute this plan inline in the current session. Do not dispatch subagents, and do not stage, commit, or push.

**Goal:** Move the complete native ImageDraw and default-font implementation, including its public exports, out of `src/pillow_c.cpp` into a real drawing translation unit.

**Architecture:** `pillow_c_draw.cpp` owns raster primitives, flood-fill, bitmap-mask compositing, shape clipping, polygon/line/ellipse/arc/chord/pieslice/rounded-rectangle drawing, the embedded default-font glyph table and text layout, the font handle ABI, and all draw/text/font exports. The module consumes explicit image-buffer seams from `pillow_c_internal.h`; the monolith retains only shared primitives used by unrelated image operations. Public export names, signatures, status codes, handle ownership, and pointer lifetimes remain unchanged.

**Tech Stack:** MSVC v143 C++17, existing `PillowCImage` ABI, AutoHotkey v2 `ahktest`, Release x64 MSBuild.

---

### Task 1: Structural RED

**Files:**
- Modify: `ahk/pillow_c.test.ahk`
- Create: `src/pillow_c_draw.cpp` (expected only after RED)
- Modify: `src/pillow_c.vcxproj` (expected only after RED)

- [x] Add `PillowCTestDrawImplementationHasItsOwnTranslationUnit` asserting that the draw file, project entry, flood-fill/polygon/multiline/text/font bodies exist in the draw unit and are absent from `pillow_c.cpp`.
- [x] Run the structural test with `run-ahktest.ps1 -Filter "drawing and font implementation" -TimeoutSeconds 120` and observe the expected missing-file failure.

### Task 2: Establish explicit shared seams

**Files:**
- Modify: `src/pillow_c_internal.h`
- Modify: `src/pillow_c.cpp`

- [x] Declare seams for coordinate normalization, pixel offset calculation, bitmap-mask admission/alpha extraction, and the divide-by-255 blend primitive.
- [x] Define those seams beside the existing ABI-independent helper adapters, leaving the existing main-unit callers on their local implementations.

### Task 3: Move drawing and font ownership

**Files:**
- Create: `src/pillow_c_draw.cpp`
- Modify: `src/pillow_c.cpp`
- Modify: `src/pillow_c.vcxproj`

- [x] Move the draw-only structs/constants, raster helpers, flood-fill, shape functions, bitmap drawing, default-font mask data, text layout, font ABI, and all draw/text/textbbox exports into the new translation unit.
- [x] Route the new unit through the explicit seams and keep all image traversal, glyph rendering, alpha blending, and clipping loops native.
- [x] Remove the moved implementations and exports from `pillow_c.cpp`; do not leave duplicate bodies or forwarding export shells.
- [x] Add `pillow_c_draw.cpp` to the Release and Debug project item list.

### Task 4: Verify ownership and behavior

**Files:**
- Modify: `docs/pillow-gap-checkpoint.md`
- Modify: `docs/pillow-gap-analysis.md`
- Modify: `docs/architecture.md`
- Modify: `docs/native-abi.md`
- Modify: `docs/testing.md`

- [x] Build Release x64 and require `0 Warning(s), 0 Error(s)`.
- [x] Verify source/DLL export parity remains `445/445` with zero difference.
- [x] Run draw/font raw and facade filters, then the complete AHK directory suite with `-TimeoutSeconds 240`.
- [x] Record module ownership, source line counts, ABI/ownership invariants, test totals, and the rebuilt DLL SHA-256. State explicitly that architecture extraction does not change the `59% ±4%` compatibility estimate.

## Final evidence

The completed draw/font extraction owns 6,310 lines in
`src/pillow_c_draw.cpp`; `src/pillow_c.cpp` is 15,261 lines after the later
legacy-codec extraction. Draw raw tests pass `35/35`, default-font raw tests
`6/6`, facade ImageDraw tests `57/57`, facade ImageFont tests `4/4`, and the
structural ownership test passes `1/1`. Release x64 builds with `0 Warning(s),
0 Error(s)`; source/DLL exports are `445/445` with zero difference; the full
AHK suite passes `2621/2621` in `27734ms`; and the current DLL SHA-256 is
`5FE477FD9D8F45473010D908D87B6903D9A2C931807AD4578A7F818454D364AF`.
Public ABI names/signatures, status codes, font-handle ownership, and pointer
lifetimes are unchanged. This architecture work does not increase the
`59% ±4%` compatibility estimate.
