# Pillow Gap Checkpoint

This is the short resume layer for Pillow work in this repository. Read this
file first, then open `docs/pillow-gap-analysis.md` only for the selected gap
card or broader evidence.

Last updated: 2026-06-10

## Required Resume Order

1. Read `AGENTS.md`.
2. Read this file.
3. Read `docs/pillow-gap-analysis.md`.
4. Read `docs/native-abi.md` before changing exported DLL behavior.
5. Read `docs/testing.md` before running AHK tests.
6. Check `git status --short` before editing.

Do not restart with a full broad audit. Update this checkpoint and the detailed
ledger together whenever coverage meaningfully changes.

## Current Snapshot

```text
Estimate: overall project target 40-45%; full Pillow replacement 25-30%.
Latest covered gap: FMT-GIF-004D.
Next recommended gap: FMT-GIF-004E unless the user names another area.
Current WIP: none.
Native rebuild needed: only after touching src/pillow_c.cpp or project files.
Test shape: parent tools runner, -TimeoutSeconds 120, no parallel AHK tests.
```

Known current counts:

- AHK tests in tree: `757` total.
- Facade tests: `382` in `ahk\pillow.test.ahk`.
- Raw DLL tests: `375` in `ahk\pillow_c.test.ahk`.
- Latest full directory verification: `Ran 757 tests in 40985ms; Passed: 757, Failed: 0, Errors: 0, Skipped: 0`.

## Current Phase Goal

Goal: turn `pillow_c.dll` plus `ahk/pillow.ahk` into a high-performance,
AHK-first Pillow-compatible runtime for the common scripting surface before
chasing full plugin parity.

Next execution target:

1. Finish GIF animation correctness around the remaining bounded
   post-`disposal=2` cases.
2. Move to PNG/JPEG metadata and save-option parity, because these formats
   dominate practical AHK image automation.
3. Add focused benchmarks only after the corresponding correctness slice is
   stable; performance claims must be backed by repeatable numbers.
4. Keep each slice native-first: AHK normalizes arguments and lifetimes, while
   image allocation, bytes, transforms, codecs, and hot loops stay in the DLL.

## Current Recommended Gap

```text
ID: FMT-GIF-004E
Area: GIF
Parent: FMT-GIF-004
Status: remaining
Gap: Remaining bounded post-disposal=2 animation edge cases after the covered
     transparency-aware re-diff path, especially combinations where the next
     frame equals the restored background, optimize=False, or more pathological
     per-frame palette layouts alter Pillow's bbox choice.
Start in code/tests: save_gif_animation_image post-disposal=2 branches,
                     Pillow GifImagePlugin._write_multiple_frames, and
                     raw/facade tests beside FMT-GIF-004D.
Done when: one additional disposal=2 edge case is pinned with raw/facade
           tests or explicitly documented unsupported behavior, and the full
           AHK directory suite still passes with -TimeoutSeconds 120.
```

Resolved facts that should not be re-probed before `FMT-GIF-004E`:

- `FMT-GIF-004D` already covers the first optimized caller-transparency
  post-`disposal=2` re-diff fixture.
- On the covered `3x1` P-mode probes, caller `background` changes metadata but
  does not itself drive Pillow's next optimized bbox choice.
- The current bounded authority remains local Python `3.10.11` with Pillow
  `11.3.0`.

## High-Level Gap Map

Current highest-value remaining areas:

1. `FMT-GIF-004E`: next bounded GIF `disposal=2` edge case.
2. `FMT-PNG-001` to `FMT-PNG-003`: PNG metadata, `tRNS`, and advanced save options.
3. `FMT-JPEG-001` to `FMT-JPEG-003`: JPEG metadata, progressive/optimize options, and CMYK/YCCK semantics.
4. `FMT-TIFF-001` to `FMT-TIFF-003`: TIFF multipage save, tags/compression, and broader mode coverage.
5. `QUANT-001`: broader quantize/public algorithm parity beyond bounded GIF save slices.
6. `META-001` and `META-002`: EXIF/XMP/ICC lifecycle and preservation.

## Read Complete Template

Use this exact shape before implementation work:

```text
Read disk complete:
- Estimate: overall project target 40-45%; full Pillow replacement 25-30%.
- Latest covered gap: FMT-GIF-004D.
- Selected gap: <gap ID and one-line scope>.
- Known tests: current tree registers 757 tests; latest full AHK directory
  suite passed 757/757 with 120s timeout.
- Native rebuild: required only if src/pillow_c.cpp or project files change.
```

If any line above is no longer true, update this file first, then update
`docs/pillow-gap-analysis.md` in the same patch.
