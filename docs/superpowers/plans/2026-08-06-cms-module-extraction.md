# ARCH-MOD-004 CMS Module Extraction Implementation Plan

> **For agentic workers:** Execute this plan inline in the current session. Do not dispatch subagents, and do not stage, commit, or push.

**Goal:** Move all LittleCMS profile/transform ownership and the built-in LAB conversion helper out of `src/pillow_c.cpp` into a separately compiled native module without changing the 445-symbol ABI.

**Architecture:** `pillow_c_cms.cpp` owns `PillowCCmsProfile`, `PillowCCmsTransform`, LittleCMS RAII, all `pillow_c_cms_*` exports, and the cached built-in RGB/LAB transforms. `pillow_c_internal.h` exposes only the image-facing seams needed by the conversion code: one bulk built-in LAB conversion function and the existing image buffer refresh/detach contracts. The main unit keeps non-CMS conversion logic and calls the seam; it does not retain LittleCMS types or export bodies.

**Tech Stack:** MSVC C++17, x64, LittleCMS 2.17, existing `pillow_c.dll` ABI, AutoHotkey v2 `ahktest`, local Pillow 11.3.0 fixtures.

---

### Task 1: Establish the structural ownership contract

**Files:**

- Create: `src/pillow_c_cms.cpp` (initially absent; test must prove this)
- Modify: `ahk/pillow_c.test.ahk`
- Modify: `docs/pillow-gap-checkpoint.md`
- Modify: `docs/pillow-gap-analysis.md`

- [ ] Add one structural test that requires `pillow_c_cms.cpp`, requires CMS profile/transform exports and the vcxproj entry in the new unit/project, and asserts those export definitions and `PillowCCmsProfile` are absent from `pillow_c.cpp`.
- [ ] Run the single structural test through `run-ahktest.ps1` with `-TimeoutSeconds 120` and record the expected RED failure caused by the missing translation unit.
- [ ] Add the bounded architecture ID `ARCH-MOD-004` to the checkpoint and detailed ledger as `in progress`; this is architecture evidence, not compatibility percentage.

### Task 2: Extract CMS implementation and wire the internal contract

**Files:**

- Create: `src/pillow_c_cms.cpp`
- Modify: `src/pillow_c.cpp`
- Modify: `src/pillow_c_internal.h`
- Modify: `src/pillow_c.vcxproj`

- [ ] Move the top-level LittleCMS include, `PillowCLabTransforms`, `PillowCCmsProfile`, `PillowCCmsTransform`, CMS helper functions, and the complete CMS export block from `pillow_c.cpp` into `pillow_c_cms.cpp`.
- [ ] Add a declared internal seam for RGB/RGBA/RGBX↔LAB built-in conversion and a declared detach seam for CMS in-place operations; keep every row traversal in the CMS translation unit.
- [ ] Replace the two direct cached-LittleCMS branches in `convert_image_mode_into` with the bulk internal seam.
- [ ] Add an explicit `ClCompile` entry for `pillow_c_cms.cpp`; do not use wildcard project inclusion for project-owned C++ sources.
- [ ] Confirm `pillow_c.cpp` has no CMS export definitions, CMS ownership structs, or direct LittleCMS calls after extraction.

### Task 3: Build and regression verification

- [ ] Run Release x64 MSBuild and require zero warnings/errors.
- [ ] Compare source and DLL exports and require `445/445` with zero difference.
- [ ] Run CMS targeted raw/facade tests, then JPEG and TIFF targeted filters serially.
- [ ] Run the full AHK directory suite from the parent workspace with `-TimeoutSeconds 240`; require zero failures, errors, and skips.
- [ ] Confirm no AHK per-pixel loop was introduced in the facade or conversion route.

### Task 4: Close the architecture packet

**Files:**

- Modify: `docs/pillow-gap-checkpoint.md`
- Modify: `docs/pillow-gap-analysis.md`
- Modify: `docs/architecture.md`
- Modify: `docs/native-abi.md`
- Modify: `docs/testing.md`

- [ ] Record module ownership, line counts, unchanged ABI, build result, targeted/full test counts, export parity, and DLL SHA-256 under `ARCH-MOD-004`.
- [ ] Keep the overall `59% ±4%` compatibility estimate unchanged unless a separate Pillow behavior gap is covered.
- [ ] Run `git diff --check` for the task scope and leave the worktree uncommitted as requested.
