# ARCH-MOD-006 GIF Codec Module Extraction Implementation Plan

> Execute inline in the current session. Do not dispatch subagents, stage,
> commit, or push.

**Goal:** Move the complete GIF reader, LZW codec, frame compositor, metadata
parser, indexed writer, animation writer, and public GIF exports out of
`src/pillow_c.cpp` into one explicit translation unit
without changing the public ABI or facade behavior.

**Architecture:** `pillow_c_codec_gif.cpp` owns GIF container semantics from
file bytes through decoded/composited frames and encoded animation bytes. It
uses WIC only for the existing single-frame adapter and consumes shared image,
file-IO, buffer-refresh, and exact RGB/L/GIF quantization seams from
`pillow_c_internal.h`. Quantization remains shared with Image.quantize until
the operations unit is extracted. No GIF implementation body or exported
forwarding shell remains in the monolith.

## Task 1: Structural RED

- [x] Add one AHK structural test requiring the new source file, project
  entry, representative parser/LZW/compositor/writer implementations, GIF
  exports, and absence of those bodies/exports from `pillow_c.cpp`.
- [x] Run only that test before creating the module and record the expected
  missing-file failure.

## Task 2: Native ownership move

- [x] Move GIF structs, parser/LZW helpers, frame composition, metadata,
  writer, and animation optimization into
  `pillow_c_codec_gif.cpp`.
- [x] Move every `pillow_c_image_*gif*` export into the GIF unit with unchanged
  signatures and status behavior.
- [x] Add only the internal seams needed for shared exact RGB/L/GIF quantization
  and buffer refresh; do not duplicate code or add fallback behavior.
- [x] Add `pillow_c_codec_gif.cpp` explicitly to `pillow_c.vcxproj`.

## Task 3: Verification

- [x] Rebuild Release x64 with zero warnings and errors, keeping
  `build/x64/Release/pillow_c.dll` current.
- [x] Run the structural test, targeted raw/facade GIF filters, raw file,
  facade file, and the full AHK directory suite with `-TimeoutSeconds 240`.
- [x] Compare source and DLL exports alias-aware against the `445/445`
  baseline and record the rebuilt DLL SHA-256.
- [x] Run `git diff --check` and verify no implementation copy remains in the
  monolith.

## Task 4: Documentation

- [x] Record `ARCH-MOD-006`, module ownership, internal seams, line counts,
  build/test/export evidence, and unchanged compatibility estimate in the
  checkpoint, gap ledger, architecture, native ABI, and testing documents.
