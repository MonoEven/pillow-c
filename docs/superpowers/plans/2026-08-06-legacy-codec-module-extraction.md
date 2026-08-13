# ARCH-MOD-005 Legacy Codec Module Extraction Implementation Plan

> Execute inline in the current session. Do not dispatch subagents, stage,
> commit, or push.

**Goal:** Move the complete BMP, PPM/Netpbm, QOI, TGA, XBM, and ICO native
implementation and public exports out of `src/pillow_c.cpp` into one explicit
translation unit without changing the public ABI or facade behavior.

**Architecture:** `pillow_c_codec_legacy.cpp` owns the legacy file-format
parsers/writers, ICO directory and payload logic, and all legacy codec exports.
ICO reuses the existing explicit PNG custom-mode/encoder seams and the native
resize seam. The module consumes internal Mode-1 sizing, LE int32 decoding,
int32-to-uint16 clipping, buffer refresh, file IO, WIC, and image-shape seams;
none are DLL exports.

## Task 1: Structural RED

- [x] Add a structural AHK test requiring the new source file, project entry,
  representative BMP/PPM/ICO implementation bodies, and absence of those
  bodies/exports from `pillow_c.cpp`.
- [x] Run the filtered test before creating the module. RED evidence was
  `0/1`: the expected `pillow_c_codec_legacy.cpp` file did not exist.

## Task 2: Native ownership move

- [x] Move the contiguous BMP/PPM/QOI/TGA/XBM/ICO helper and implementation
  block into `pillow_c_codec_legacy.cpp`.
- [x] Move the corresponding open/save/ICO export block into the new unit;
  leave no forwarding shell or duplicate implementation in the monolith.
- [x] Add the explicit Release/Debug project entry.
- [x] Add the three required internal seams for Mode-1 raw sizing, LE int32
  decoding, and int32-to-uint16 clipping; route ICO through existing PNG and
  resize seams.

## Task 3: Verification

- [x] Release x64 build: `0 Warning(s), 0 Error(s)`.
- [x] Structural ownership test: `1/1`.
- [x] Targeted raw/facade filters: BMP `8/8`, PPM `11/11`, QOI `5/5`, TGA
  `11/11`, XBM `9/9`, and ICO `24/24`.
- [x] Full AHK directory suite with `-TimeoutSeconds 240`: `2621/2621` in
  `27734ms`; raw file run `1295/1295` and facade file run `1326/1326`.
- [x] Alias-aware source/DLL export comparison: `445/445`, zero difference.
- [x] Current Release DLL SHA-256:
  `5FE477FD9D8F45473010D908D87B6903D9A2C931807AD4578A7F818454D364AF`.

## Task 4: ABI and compatibility outcome

- [x] Public export names, signatures, status codes, handle ownership, and
  pointer lifetimes are unchanged.
- [x] No AHK per-pixel loop or fallback path was added.
- [x] Architecture completion is recorded as `ARCH-MOD-005`; the overall
  Pillow replacement-readiness estimate remains `59% ±4%` because this is
  ownership restructuring, not a new Pillow behavior gap.
