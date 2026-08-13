# OPS Deep Decomposition Implementation Plan

> **For agentic workers:** Execute inline in the current session. Do not dispatch subagents, stage, commit, or push.

**Goal:** Physically separate the native statistics/autocontrast implementation from the already-extracted operations translation unit.

**Architecture:** `pillow_c_ops_statistics.cpp` owns histogram, entropy, extrema, bbox, projection, getcolors, and autocontrast algorithms plus their public exports. `pillow_c_ops.cpp` retains arithmetic, conversion, palette, compositing, ImageChops, point/LUT, quantization, and spatial implementations. A private `pillow_c_ops_internal.h` seam exposes only the existing cross-module operations required by the remaining operations unit; no public ABI symbol changes.

**Tech Stack:** MSVC v143 C++17, AutoHotkey v2 `ahktest`, Pillow 11.3.0 behavior authority, Release x64 MSBuild.

---

### Task 1: Structural RED

**Files:**
- Modify: `ahk/pillow_c.test.ahk`

- [x] Add one test requiring `pillow_c_ops_statistics.cpp`, its project entry, representative statistics/autocontrast bodies, and absence of those bodies from `pillow_c_ops.cpp`.
- [x] Run the structural filter and observe the missing-file failure.

### Task 2: Native ownership move

**Files:**
- Create: `src/pillow_c_ops_statistics.cpp`
- Create: `src/pillow_c_ops_internal.h`
- Modify: `src/pillow_c_ops.cpp`
- Modify: `src/pillow_c.vcxproj`

- [ ] Move the real histogram/entropy/extrema/bbox/projection/getcolors/autocontrast implementations and corresponding exports.
- [ ] Keep all loops native and preserve status, allocation, and handle ownership contracts.
- [ ] Replace any cross-unit calls with declarations in the private seam; do not copy helper bodies or create forwarding exports.

### Task 3: Verification

**Files:**
- Modify: `docs/architecture.md`
- Modify: `docs/native-abi.md`
- Modify: `docs/testing.md`
- Modify: `docs/pillow-gap-checkpoint.md`
- Modify: `docs/pillow-gap-analysis.md`

- [ ] Rebuild Release x64 with zero warnings/errors.
- [ ] Run the structural test, raw/facade statistics filters, full AHK suite with `-TimeoutSeconds 240`, and source/DLL export parity.
- [ ] Record exact module sizes, tests, build hash, and preserve the compatibility estimate.
