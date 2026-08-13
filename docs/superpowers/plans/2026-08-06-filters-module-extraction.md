# Filters and Resampling Module Extraction Implementation Plan

> **For agentic workers:** Execute this plan inline in the current session. Do not dispatch subagents, and do not stage, commit, or push.

**Goal:** Move the complete native resize/filter/reducing-gap/fit/pad implementation and its public exports out of `src/pillow_c.cpp` into a dedicated translation unit.

**Architecture:** `pillow_c_filters.cpp` owns resampling coefficient generation, nearest/box/filter resize loops, reducing-gap composition, rank/mode/kernel/blur/unsharp/LUT filters, and fit/pad/contain/cover allocation routes. The monolith retains only explicit shared seams for image shape, palette propagation, buffer refresh, storage fill, and the existing ICO caller. The public 445-symbol ABI remains unchanged.

**Tech Stack:** MSVC C++17, existing Pillow-compatible native image ABI, AutoHotkey v2 `ahktest`, Release x64 MSBuild.

---

### Task 1: Structural RED

- [x] Add a structural AHK test requiring `pillow_c_filters.cpp`, its project entry, filter/resampling implementation bodies, and absence of those bodies from `pillow_c.cpp`.
- [x] Run the test and observe the expected missing-translation-unit failure.

### Task 2: Native ownership move

- [ ] Move the resampling/filter helper block and resize/filter/fit/pad exports into `pillow_c_filters.cpp`.
- [ ] Add only the explicit internal seams required by remaining native callers.
- [ ] Keep all filter and resize loops native; do not add facade loops or fallback routes.

### Task 3: Verification and documentation

- [ ] Rebuild Release x64 with zero warnings/errors.
- [ ] Verify source/DLL export parity remains 445/445.
- [ ] Run filter and resize targeted tests, then the full AHK suite with a 240-second timeout.
- [ ] Record module ownership, line counts, ABI invariants, tests, and DLL hash in the five authoritative docs.
