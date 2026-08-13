# ARCH-MOD-007 Operations Module Extraction Implementation Plan

> Execute inline in the current session. Do not dispatch subagents, stage,
> commit, or push.

**Goal:** Move the complete native conversion, palette, compositing,
ImageChops, point/ImageOps, statistics, crop/paste/transpose, quantization, and
related allocating/`_into` exports out of `src/pillow_c.cpp` into
`src/pillow_c_ops.cpp` without changing the public ABI or facade behavior.

**Architecture:** `pillow_c_ops.cpp` owns whole-image arithmetic and conversion
algorithms plus their public exports. It consumes core image-shape, buffer,
color-kernel, CMS, and mask Interfaces from `pillow_c_internal.h`; codec,
metadata, raw byte storage, gradients/effects, and mode-name ownership stay in
their existing Modules. Shared quantization seams used by GIF become direct
operations-module Interfaces rather than monolith adapters. No operation body
or public forwarding export remains in the monolith.

## Task 1: Structural RED

- [x] Add one structural AHK test requiring the operations source, project
  entry, representative conversion/composite/statistics implementations and
  exports, and absence of those bodies/exports from `pillow_c.cpp`.
- [x] Run the test before creating the source file and record the expected
  missing-file failure.

## Task 2: Native ownership move

- [x] Move the complete contiguous operations helper block into
  `pillow_c_ops.cpp`.
- [x] Move low-level blend/luma/alpha-composite exports, spatial/`_into`
  exports, palette exports, statistics exports, and all allocating operations
  exports into the operations unit.
- [x] Move internal GIF quantization and spatial helper Interfaces to the
  operations unit; add only explicit shared color/mask/core seams required by
  other Modules.
- [x] Add the explicit project source entry and leave no duplicate body.

## Task 3: Verification and documentation

- [x] Build Release x64 with zero warnings/errors and keep the DLL current.
- [x] Run structural, operation-targeted raw/facade, raw-file, facade-file,
  and full-directory AHK suites.
- [x] Verify alias-aware source/DLL export parity, DLL SHA-256, no duplicate
  markers, no trailing whitespace, and `git diff --check`.
- [x] Update checkpoint, gap ledger, architecture, native ABI, testing, and
  this plan with exact evidence while keeping the compatibility estimate
  unchanged.

## Result

`ARCH-MOD-007` is complete. `pillow_c_ops.cpp` is 7,993 lines and owns the
operations implementation plus public fill/getpixel/putpixel exports;
`pillow_c.cpp` is 4,606 lines and retains no operations forwarding shell.
Release x64 is green with `0 Warning(s), 0 Error(s)`. Structural ownership is
`1/1`; raw targeted operations are fill `2/2`, get/putpixel `2/2`, paste
`13/13`, transpose `12/12`, point LUT `6/6`, and handle `3/3`; facade targeted
fill is `1/1` and get/putpixel is `2/2`; the complete directory suite is
`2623/2623` in `29453ms`; source/DLL export parity is `445/445`; and the DLL
SHA-256 is
`50C0FCB6CCABBA75098C5CB90732F540624EBD4BD562F24EE852E18D4E900EBE`.
