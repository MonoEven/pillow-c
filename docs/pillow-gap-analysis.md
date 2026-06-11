# Pillow Gap Analysis

This document is the persistent checkpoint for Pillow coverage. Read it before
continuing feature work so each session does not need to rediscover the same
state.

For the fastest resume path, read `docs/pillow-gap-checkpoint.md` first, then
use this file as the detailed ledger.

Last updated: 2026-06-10

## Read This First

Before changing Pillow behavior in this repository:

1. Read `docs/pillow-gap-checkpoint.md`.
2. Read this file.
3. Read `docs/native-abi.md` for the current exported ABI contract.
4. Read `docs/testing.md` for the required AHK test command shape.
5. Check the current worktree before editing.
6. After a feature changes coverage, update this file and the checkpoint in the
   same patch.

This file is the persistent memory for the gap audit. Do not re-inventory the
whole project from scratch at the start of each small Pillow task. Read this
file first, confirm whether the target area is already listed below, then only
probe the exact behavior needed for the next increment.

Current local constraints:

- Behavior authority is `F:\Python\Python310\python.exe` with Pillow `11.3.0`.
- Do not use the parent `pillow.ahk` as a reference implementation.
- Do not modify parent `tools`; only call the parent test runner.
- Every AHK test run should include `-TimeoutSeconds 120`.
- AHK tests should use `ahktest` and captured errors, not modal popups.
- Keep `build\x64\Release\pillow_c.dll` current after native changes.
- Do not remote or push unless explicitly requested.

## Maintenance Protocol

Use this document as a working ledger:

1. Start every Pillow feature session by reading this file.
2. Pick or add a bounded work packet from `Detailed Gap Ledger` or
   `Recommended Priority Queue`.
3. Probe Pillow behavior with the local Python/Pillow version before writing
   implementation when semantics are unclear.
4. Add red AHK tests against the raw DLL and/or facade before implementation
   unless the change is pure documentation.
5. Update the relevant ledger row from "remaining" to "covered" only after the
   DLL, facade, docs, and tests agree.
6. Leave unknown or deferred edge cases explicit. Do not silently collapse them
   into a broad "covered" claim.

For feature commits, this document should answer four questions without a new
audit:

- What is already implemented?
- What is missing?
- Where should the next task begin in code/tests?
- Which Pillow behavior still needs source or oracle confirmation?

## Resume Contract

This file is the authoritative detailed "read disk first" ledger for the
project. When a future session continues Pillow work, the intended flow is:

1. Read `AGENTS.md`.
2. Read `docs/pillow-gap-checkpoint.md`.
3. Read this file.
4. Identify the requested area or choose the first still-relevant item in
   `Recommended Priority Queue`.
5. Open only the source/test files named by that item's `Start in code/tests`
   note.
6. Probe local Pillow behavior only for that item, then write tests and code.

A read is complete when the next worker can state:

- the current completion estimate,
- the latest completed work packet,
- the next selected gap ID,
- the exact test command shape,
- and whether native rebuild is required.

After reading this file, say the selected gap ID and the latest known test
state before editing. If the requested work is not represented by an existing
gap ID, add one first. This keeps future sessions from spending time on a full
fresh inventory.

Do not add vague status like "mostly works" or "needs more tests". Add concrete
gap IDs, affected modes/formats, relevant code entry points, and the exact
Pillow behavior still needing proof.

## Session Entry Checkpoint

Use this as the shortest current-state readout after loading the ledger:

```text
Estimate: overall project target 40-45%; full Pillow replacement 25-30%.
Latest covered gap: FMT-GIF-004D.
Next recommended gap: FMT-GIF-004E unless the user names another area.
Current WIP: none. Use FMT-GIF-004E as the next bounded GIF animation
             disposal=`2` edge-case slice unless the user names a different
             area.
Native rebuild needed: only after touching src/pillow_c.cpp or project files.
Test shape: parent tools runner, -TimeoutSeconds 120, no parallel AHK tests.
```

Current next recommended gap:

```text
ID: FMT-GIF-004E
Area: GIF
Parent: FMT-GIF-004
Status: remaining
Gap: Remaining GIF animation `disposal=2` cases after the covered
     transparency-aware re-diff path, especially combinations where the next
     frame equals the restored background, `optimize=False`, or larger/more
     pathological per-frame palette layouts alter Pillow's bbox choice.
Start in code/tests: `save_gif_animation_image` post-`disposal=2` branches,
                     Pillow `GifImagePlugin._write_multiple_frames`, and
                     raw/facade tests beside FMT-GIF-004D.
Oracle/source note: FMT-GIF-004D resolved the first minimal optimized
                    transparency-aware re-diff fixture. The next oracle work
                    should stay bounded to one uncovered `disposal=2` edge
                    case at a time.
Done when: one additional `disposal=2` edge case is pinned with raw/facade
           tests or documented unsupported behavior, existing GIF animation
           tests remain green, and the full AHK directory suite passes with
           -TimeoutSeconds 120.
```

When the read is complete, use this exact shape in the work log or user update
before editing:

```text
Read disk complete:
- Estimate: overall project target 40-45%; full Pillow replacement 25-30%.
- Latest covered gap: FMT-GIF-004D.
- Selected gap: <gap ID and one-line scope>.
- Known tests: current tree registers 757 tests; latest full AHK directory
  suite passed 757/757 with 120s timeout.
- Native rebuild: required only if src/pillow_c.cpp or project files change.
```

If any line cannot be filled from this file, update this file first. Do not
substitute a new broad audit in chat.

## Gap Item Template

When adding a new gap row, use this shape in the ledger and expand the matching
section if the row is not self-explanatory:

```text
ID: AREA-NNN
Area: Format, Mode, Metadata, Performance, ABI, Testing, or Packaging
Status: not started | partial | remaining | covered
Gap: one concrete behavior boundary
Start in code/tests: exact native function, facade method, and test block
Oracle/source note: local probe, installed Pillow source, or upstream source
Done when: raw DLL, facade if applicable, docs, DLL artifact, and tests agree
```

Use `partial` when some behavior exists but the Pillow contract is materially
incomplete. Use `remaining` for a known missing increment inside an otherwise
active area. Use `covered` only after tests and docs back the claim.

## Snapshot Evidence

The current implementation is substantial but still far from a full Pillow
replacement.

Measured in this repository on 2026-06-10:

- Native symbols discovered in `src\pillow_c.cpp`: about `276` unique
  `pillow_c_*` names.
- DLL export table for `build\x64\Release\pillow_c.dll`: `276`
  `pillow_c_*` exports.
- AHK tests present in the current worktree: `757` total.
- Facade tests: `382` in `ahk\pillow.test.ahk`.
- Raw DLL tests: `375` in `ahk\pillow_c.test.ahk`.
- Last fully verified suite is `757/757`.
- Core implementation and tests inspected together:
  `ahk\pillow.ahk`, `ahk\pillow.test.ahk`, `ahk\pillow_c.test.ahk`,
  `src\pillow_c.cpp` total about `58,114` lines.
- Last known full AHK directory run passed:
  `Ran 757 tests in 40985ms; Passed: 757, Failed: 0, Errors: 0, Skipped: 0`.

Local Pillow comparison:

- Pillow version: `11.3.0`.
- `PIL.Image.Image` public callable methods: about `50`.
- Registered Pillow file-format families: about `44`.
- Main modules compared: `Image`, `ImageOps`, `ImageChops`, `ImageFilter`,
  `ImageEnhance`, `ImageDraw`, `ImageFont`, `ImageStat`, `ImageColor`,
  `ImageSequence`.

## Current Completion Estimate

These percentages are pragmatic engineering estimates, not a generated coverage
score.

| Area | Estimated completion | Notes |
| --- | ---: | --- |
| AHK facade for common Pillow scripting | 55-60% | Most common image object methods, ImageOps, ImageChops, filters, draw basics, stats, sequences, and color parsing exist. Missing metadata, some constructors, full font stack, and advanced plugin semantics are still material. |
| Native hot-path acceleration layer | 50-55% | Many pixel, geometry, filter, draw, palette, and IO paths are native and tested. Still scalar, limited in some modes, and missing broad codec/plugin coverage. |
| File-format parity with Pillow | 25-30% | Several practical formats are covered, but Pillow registers about 44 format families and many per-format options remain absent. |
| Full Pillow replacement | 25-30% | Full Pillow includes plugin ecosystem, metadata, color management, font engines, optional codecs, array interop, and long-tail modes. |
| stdlib-quality AHK package experience | 35-40% | API shape is promising, but docs, packaging, release discipline, compatibility matrix, and Python-like edge semantics are incomplete. |
| Overall stated project target | 40-45% | For "Pillow-like AHK API backed by a performance-first C DLL", the foundation is real, but the remaining work is still large. |

Short version: the project is past proof-of-concept and into a serious
foundation stage, but not near full Pillow parity.

## Current Work Packet

There is no active implementation packet after `FMT-GIF-004D` was completed.
The recommended next bounded packet is `FMT-GIF-004E`.

```text
ID: FMT-GIF-004E
Parent: FMT-GIF-004
Title: GIF animation disposal=2 remaining edge cases
Status: remaining
Scope: The next bounded `disposal=2` slice after the covered
       transparency-aware re-diff behavior, such as Pillow's handling when the
       next frame matches the restored background, `optimize=False`, or a more
       complex per-frame palette changes the post-restore bbox.
Out of scope: Replacing the whole GIF encoder, broad lossy quantization
              algorithm parity, animation RGB/RGBA quantization, and redoing
              the already-covered minimal transparency-aware re-diff fixture.
```

Start in code/tests:

- `src/pillow_c.cpp`: `save_gif_animation_image` post-`disposal=2` branches
  after the covered transparency-aware re-diff path.
- `ahk/pillow.ahk`: `SaveGifAnimation` option parsing is already covered;
  focus on broader behavior, not option plumbing.
- `ahk/pillow_c.test.ahk` and `ahk/pillow.test.ahk`: add fixture(s) beside
  `FMT-GIF-004D`.

Resolved oracle/source checkpoint:

- FMT-GIF-004D established the first minimal covered optimized fixture:
  `3x1` P-mode sequence, shared black/red palette, frame 1 pixels `[1,0,0]`,
  frame 2 pixels `[0,1,0]`, `disposal=[0,2,0]`, `transparency=2`.
- On that fixture, Pillow 11.3.0 writes frame 2 as local rectangle
  `[1,0,1,1]` even though the previous written frame used `disposal=2`.
- Remaining oracle work should target one uncovered `disposal=2` edge case at
  a time instead of re-probing the covered transparency-aware branch.

Done when:

- The selected next `disposal=2` edge case has raw/facade tests, or
  unsupported combinations are rejected with documented captured errors.
- Existing GIF animation and quantization tests remain green, including
  `FMT-GIF-004A`, `FMT-GIF-004B`, `FMT-GIF-004C1`, `FMT-GIF-004C2`,
  `FMT-GIF-004C3`, and `FMT-GIF-004D`.
- Release x64 `pillow_c.dll`, `docs/native-abi.md`, and this ledger are
  updated if native behavior changes.

## Latest Work Packet

The latest completed GIF increment is:

```text
ID: FMT-GIF-004D
Parent: FMT-GIF-004
Title: GIF animation disposal=2 transparency re-diff
Status: covered
Scope: Same-size P-mode GIF animation where the previous frame uses
       `disposal=2` and the caller supplies a transparency index, covering the
       bounded `3x1` fixture where Pillow re-diffs the following frame against
       a transparency-filled restored canvas and still emits a `1x1`
       local rectangle.
Out of scope: `optimize=False` post-restore behavior, cases where the next
              frame matches the restored background, animation RGB/RGBA
              quantization, full public `Image.quantize` algorithm parity, and
              unrelated metadata work.
```

Resolved oracle/source notes:

- Fixture: `3x1` P animation with shared palette `[black, red]`.
- Frame 0 pixels `[0,0,0]`, frame 1 pixels `[1,0,0]`, frame 2 pixels
  `[0,1,0]`.
- Save options: `duration=[10,20,30]`, `loop=0`, `disposal=[0,2,0]`,
  `transparency=2`.
- Local Pillow 11.3.0 writes frame 1 as local rectangle `[0,0,1,1]` with
  GCE disposal `2` and transparency index `2`.
- Local Pillow 11.3.0 then writes frame 2 as local rectangle `[1,0,1,1]`
  with no transparency GCE, proving that the writer re-diffs against a
  transparency-filled restored canvas instead of forcing the next frame to
  full size.
- Installed Pillow 11.3.0 source checked:
  `GifImagePlugin._write_multiple_frames`.

Current implementation notes:

- `pillow_c_image_save_gif_animation` remains the default animation save ABI.
- `pillow_c_image_save_gif_animation_options` remains the ABI for
  `include_color_table` and `optimize`.
- `pillow_c_image_save_gif_animation_metadata_options` now also covers the
  first verified post-`disposal=2` transparency-aware re-diff branch.
- In the optimized caller-transparency path, when the previous output frame
  uses `disposal=2`, the native writer now recomputes the next bbox against
  the restored background RGB resolved from the caller transparency index
  instead of forcing a full-frame rectangle.
- For this covered branch, the native writer intentionally leaves the
  following frame without a transparency GCE, matching Pillow's bounded
  optimized output on the selected fixture.
- Release x64 `pillow_c.dll` was rebuilt after the native change.

Verification evidence:

- Raw red before implementation:
  `pillow_c image save_gif_animation_metadata_options re-diffs after disposal 2 with transparency`
  failed with frame 3 descriptor `[0,0,3,1]` instead of Pillow's
  `[1,0,1,1]`.
- Facade red before implementation:
  `Pillow Image.Save GIF save_all re-diffs after disposal 2 with transparency`
  failed with frame 3 descriptor `[0,0,3,1]` instead of Pillow's
  `[1,0,1,1]`.
- Release x64 build succeeded with `0 Warning(s), 0 Error(s)`.
- Raw full-file run succeeded:
  `Ran 375 tests in 12125ms; Passed: 375, Failed: 0, Errors: 0, Skipped: 0`.
- Facade targeted rerun succeeded:
  `Ran 1 tests in 109ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run succeeded:
  `Ran 757 tests in 40985ms; Passed: 757, Failed: 0, Errors: 0, Skipped: 0`.
- One facade full-file verification attempt again hit the known runner
  `status` truncation issue, so the facade path was additionally verified with
  a direct filtered rerun of the new public test plus the full directory suite.

The previous completed GIF increment is:

```text
ID: FMT-GIF-004C3
Parent: FMT-GIF-004C
Title: Caller-provided GIF animation background option
Status: covered
Scope: Same-size P-mode GIF animation where the caller supplies a
       `background` index, covering the logical-screen background byte on the
       bounded `3x1` per-frame palette fixture.
Out of scope: Broader disposal `2` interactions, animation RGB/RGBA
              quantization, full public `Image.quantize` algorithm parity, and
              unrelated metadata work.
```

The previous completed GIF increment is:

```text
ID: FMT-GIF-004C2
Parent: FMT-GIF-004C
Title: Caller-provided GIF animation transparency with optimize=False
Status: covered
Scope: Same-size P-mode GIF animation where the caller supplies a
       `transparency` index together with `optimize=False`.
Out of scope: `background`, broader disposal interaction matrices, animation
              RGB/RGBA quantization, full public `Image.quantize` algorithm
              parity, and unrelated metadata work.
```

The previous completed GIF increment is:

```text
ID: FMT-GIF-004C1
Parent: FMT-GIF-004C
Title: Caller-provided GIF animation transparency
Status: covered
Scope: Optimized same-size P-mode GIF animation where the caller supplies a
       `transparency` index and the changed local frame uses that index.
Out of scope: `background`, broader disposal interaction matrices, animation
              RGB/RGBA quantization, full public `Image.quantize` algorithm
              parity, and unrelated metadata work.
```

Resolved oracle/source notes:

- Fixture: `3x1` P animation. Frame 0 pixels `[0,0,0]`, palette index `1` =
  red. Frame 1 pixels `[0,1,0]`, palette index `1` = blue.
- Default optimized save uses frame 1 GCE transparency index `2` for the
  native unused-index optimization.
- `transparency=1` changes frame 1 GCE transparency to caller index `1`, while
  frame 0 has no transparency GCE. Reopened frame 1 is black because local
  pixel index `1` is transparent, preserving the previous black canvas.
- The optimized local frame keeps a local palette whose first entries are
  `[0,0,0, 0,0,255, 0,0,0, 0,0,0]`; the native writer pads the local palette
  before calculating the GIF color-table size even when the caller transparency
  index already exists in the source palette.
- `transparency=2` matches the default optimized unused-index result for this
  fixture.
- `transparency=1,optimize=False` is covered by `FMT-GIF-004C2`.

Current implementation notes:

- `pillow_c_image_save_gif_animation` remains the default animation save ABI.
- `pillow_c_image_save_gif_animation_options` remains the ABI for
  `include_color_table` and `optimize`.
- `pillow_c_image_save_gif_animation_metadata_options` extends the animation
  options ABI with `has_transparency` and `transparency` integers after the
  tri-state `include_color_table` and `optimize` arguments.
- The facade `SaveGifAnimation` accepts both AHK-style `Transparency` and
  Python-style `transparency`, validates it as an integer, and routes through
  the metadata-options export only when the option is supplied.
- For optimized later local frames, caller transparency replaces the native
  unused-index transparency selection. If the supplied index already exists in
  the frame palette, the writer pads one extra zero-color entry before table
  sizing so the covered fixture matches Pillow's 4-entry local color table.
- Release x64 `pillow_c.dll` was rebuilt after the native change.

Verification evidence:

- Raw red before implementation:
  `pillow_c image save_gif_animation_metadata_options honors transparency`
  errored with missing export
  `pillow_c_image_save_gif_animation_metadata_options`.
- Facade red before implementation:
  `Pillow Image.Save GIF save_all honors transparency option` failed because
  the old path wrote transparency index `2` instead of caller index `1`.
- Release x64 build succeeded with `0 Warning(s), 0 Error(s)`.
- Raw full-file run succeeded:
  `Ran 372 tests in 13500ms; Passed: 372, Failed: 0, Errors: 0, Skipped: 0`.
- Facade full-file run succeeded:
  `Ran 379 tests in 30985ms; Passed: 379, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run succeeded:
  `Ran 751 tests in 42312ms; Passed: 751, Failed: 0, Errors: 0, Skipped: 0`.

The previous completed GIF increment before that is:

```text
ID: FMT-GIF-004B
Parent: FMT-GIF-004
Title: Explicit GIF animation include_color_table and optimize options
Status: covered
Scope: Same-size P-mode GIF animation save option behavior for explicit
       `include_color_table` and `optimize` on bounded per-frame palette
       fixtures.
Out of scope: Caller-provided animation `transparency`, `background`, broader
              disposal interaction matrices, animation RGB/RGBA quantization,
              full public `Image.quantize` algorithm parity, and unrelated
              metadata work.
```

Resolved oracle/source notes:

- Fixtures:
  - `same`: `3x3` P animation, same palette, center pixel changes to red.
  - `diff`: `3x1` P animation. Frame 0 palette index `1` is red but pixels are
    all black; frame 1 palette index `1` is blue and pixels are `[0,1,0]`.
- Default and `optimize=True`: frame 0 has no local color table; the changed
  later frame has a local color table and uses transparency index `2` for
  unchanged pixels.
- `include_color_table=True`: frame 0 also gets a local color table. The
  changed later frame still has a local color table.
- `include_color_table=False`: same as default for frame 0. It does not
  suppress the later changed-frame local color table because Pillow's
  `_write_multiple_frames` path forces `include_color_table` for bbox frames
  without a global palette.
- `optimize=False`: for the bounded 2-color fixture, global and later local
  color tables use 4 entries, preserve Pillow's `LzwMin=8`, and the changed
  later frame does not use an unchanged-pixel transparency index
  (`Transparency = -1` in the current descriptor helper).
- `include_color_table=True, optimize=False`: frame 0 has a 4-entry local
  color table; the changed later frame has a 4-entry local color table; both
  preserve `LzwMin=8`, and no unchanged-pixel transparency optimization is
  used.
- Installed Pillow source checked only for this option boundary:
  `GifImagePlugin._write_local_header`, `_get_optimize`,
  `_normalize_palette`, and `_write_multiple_frames`.

Current implementation notes:

- `pillow_c_image_save_gif_animation` remains the default animation save ABI.
- `pillow_c_image_save_gif_animation_options` adds explicit tri-state
  `include_color_table` and `optimize` parameters using `-1` unset, `0` false,
  and `1` true.
- The facade `SaveGifAnimation` accepts both AHK-style `IncludeColorTable` /
  `Optimize` and Python-style `include_color_table` / `optimize` option names.
- `include_color_table=True` forces frame 0 to write a local color table.
  `include_color_table=False` does not suppress later bbox-frame local color
  tables, matching Pillow's scoped behavior.
- `optimize=False` no longer forces full-palette padding for the covered
  bounded P-mode animation fixtures. It writes 4-entry global/local color
  tables with `LzwMin=8` and disables unchanged-pixel transparency
  substitution on changed later frames when no caller transparency is supplied.
- Release x64 `pillow_c.dll` was rebuilt after the native change.

Verification evidence:

- Raw red before implementation:
  `pillow_c image save_gif_animation_options controls color tables and optimize`
  errored with missing export `pillow_c_image_save_gif_animation_options`.
- Facade red before implementation:
  `Pillow Image.Save GIF save_all honors include_color_table and optimize options`
  failed because the old path did not write the forced first-frame local color
  table.
- Release x64 build succeeded with `0 Warning(s), 0 Error(s)`.
- Raw full-file run succeeded:
  `Ran 371 tests in 11594ms; Passed: 371, Failed: 0, Errors: 0, Skipped: 0`.
- Facade full-file run succeeded:
  `Ran 378 tests in 26562ms; Passed: 378, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run succeeded:
  `Ran 749 tests in 38407ms; Passed: 749, Failed: 0, Errors: 0, Skipped: 0`.

The previous completed GIF increment before that is:

```text
ID: FMT-GIF-004A
Parent: FMT-GIF-004
Title: GIF animation per-frame local palettes
Status: covered
Scope: P-mode GIF animation save where later frames have a different RGB
       palette from frame 0. The native writer preserves later-frame colors by
       writing per-frame local color tables and comparing palette RGB values
       for differencing.
Out of scope: Explicit `include_color_table`/`optimize` option ABI, animation
              RGB/RGBA quantization, full public `Image.quantize` algorithm
              parity, and unrelated metadata work.
```

Resolved oracle/source notes:

- Fixture: `3x1` P-mode animation. Frame 0 uses palette index `0` as black and
  index `1` as red but all pixels are `0`. Frame 1 uses index `0` as black and
  index `1` as blue, with pixels `[0, 1, 0]`.
- Pillow 11.3.0 default save writes frame 1 as a one-pixel local rectangle at
  `(1, 0, 1, 1)` with a local color table whose index `1` is blue. It also
  uses transparency index `2` for unchanged pixels in the optimized local
  rectangle path.
- Installed source path checked:
  `GifImagePlugin._write_multiple_frames`, `_write_local_header`,
  `_get_optimize`, and `_normalize_palette`.

Current implementation notes:

- `pillow_c_image_save_gif_animation` no longer rejects later P frames solely
  because their RGB palettes differ from frame 0.
- Each later changed frame uses its own `palette_rgb` as the local color table.
- GIF animation differencing and unchanged-pixel transparency substitution now
  compare resolved palette RGB values rather than raw palette indexes, so the
  same index with different colors is treated as a visual change.
- Release x64 `pillow_c.dll` was rebuilt after the native change.

Verification evidence:

- Raw red before implementation:
  `pillow_c image save_gif_animation writes per-frame local palette` failed
  with `Expected 0, got -5`.
- Facade red before implementation:
  `Pillow Image.Save GIF save_all writes per-frame local palette` errored with
  `pillow_c: mismatch`.
- Release x64 build succeeded with `0 Warning(s), 0 Error(s)`.
- Raw full-file run succeeded:
  `Ran 370 tests in 13782ms; Passed: 370, Failed: 0, Errors: 0, Skipped: 0`.
- Facade full-file run succeeded:
  `Ran 377 tests in 30844ms; Passed: 377, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run succeeded:
  `Ran 747 tests in 39796ms; Passed: 747, Failed: 0, Errors: 0, Skipped: 0`.

The previous completed GIF increment before that is:

```text
ID: FMT-GIF-003B3
Parent: FMT-GIF-003B
Title: GIF quantization option and dither boundary
Status: covered
Scope: Single-frame GIF save behavior for already-covered RGB/RGBA >256
       fixtures when explicit `dither` save options are supplied.
Out of scope: Animation RGB/RGBA quantization, full public `Image.quantize`
              algorithm parity, per-frame palette option matrix,
              `include_color_table`, and unrelated metadata work.
```

Resolved oracle/source notes:

- Fixtures reused the bounded RGB `17x17` >256-color pattern from
  `FMT-GIF-003B1` and the bounded RGBA `18x18` >256-color pattern from
  `FMT-GIF-003B2`.
- Pillow 11.3.0 produced byte-identical GIF output for default save,
  `dither=Image.Dither.NONE`, `dither=Image.Dither.FLOYDSTEINBERG`, integer
  `dither=0`, integer `dither=3`, and arbitrary string values such as
  `dither="x"`/`"none"` for the selected fixtures.
- Installed source path checked:
  `GifImagePlugin._normalize_mode`, `GifImagePlugin._normalize_palette`, and
  `Image.Image.quantize`.
- `GifImagePlugin._normalize_mode` and `_normalize_palette` do not consume the
  GIF save `dither` option for these paths. The public `Image.Image.quantize`
  API still has a `dither` parameter, but that is separate from GIF save option
  handling.
- The compatibility target for this slice is preserving deterministic
  single-frame GIF output while treating facade GIF `Dither`/`dither` save
  options as ignored, like Pillow, for the selected RGB/RGBA >256 fixtures.

Current implementation notes:

- No native code change was required.
- Raw DLL behavior is deterministic for repeated RGB/RGBA >256 GIF saves, and
  the existing `pillow_c_image_save_gif_options(..., hasTransparency=0)` path
  delegates to the same bytes as the default save.
- The facade already ignores unknown GIF save `Dither`/`dither` options in the
  tested path, matching the Pillow 11.3.0 oracle boundary.
- Release x64 `pillow_c.dll` was not rebuilt for this documentation/test-only
  slice.

Verification evidence:

- Raw targeted test added:
  `pillow_c image save_gif quantization output is deterministic`.
- Facade targeted test added:
  `Pillow Image.Save GIF ignores dither option like Pillow`.
- Raw targeted run succeeded:
  `Ran 1 tests in 62ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Facade targeted run succeeded:
  `Ran 1 tests in 47ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw full-file run succeeded:
  `Ran 369 tests in 13656ms; Passed: 369, Failed: 0, Errors: 0, Skipped: 0`.
- Facade full-file first run had a transient missing `ahktest-status.txt` with
  no stderr/stdout failure and no AutoHotkey process left; the rerun succeeded:
  `Ran 376 tests in 29953ms; Passed: 376, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory rerun succeeded:
  `Ran 745 tests in 42032ms; Passed: 745, Failed: 0, Errors: 0, Skipped: 0`.

```text
ID: FMT-GIF-003B2
Parent: FMT-GIF-003B
Title: RGBA >256-color single-frame GIF save quantization
Status: covered
Scope: Single-frame RGBA GIF save when the effective opaque/transparent color
       set exceeds 256 entries. The implementation preserves the alpha==0
       transparency boundary from FMT-GIF-003A while adding a bounded lossy
       quantization path.
Out of scope: Animation RGBA quantization, dither parity,
              Pillow's exact palette ordering for every fixture, per-frame
              palette option matrix, include_color_table, and unrelated
              metadata work.
```

Resolved oracle/source notes:

- Fixture: `18x18` RGBA with 324 source pixels and fully transparent pixels at
  `(0,0)`, `(5,7)`, and `(17,17)`. Non-transparent RGB values are generated by
  `(x*13 + y*5) % 256`, `(x*17 + y*11) % 256`, and
  `((x*7 + y*19) * 3) % 256`; alpha is `1` when `(x+y) % 5 == 0`, otherwise
  `255`.
- Pillow 11.3.0 saves that fixture as `GIF89a`, reopens as mode `P`, uses 256
  palette indexes, and writes transparency metadata. The probed Pillow
  transparent index was `33`, not fixed to zero; all fully transparent source
  pixels map to that same transparent index.
- Partial alpha remains opaque after reopen/convert. For the selected fixture,
  Pillow's non-transparent reopened RGB approximation had max channel error
  `38` and average Manhattan error about `8.56`.
- Installed source path checked:
  `GifImagePlugin._normalize_mode`, `GifImagePlugin._normalize_palette`, and
  `Image.Image.quantize`.
- The compatibility target for this slice is approximate reopened RGB pixels
  for non-transparent source pixels plus Pillow's `alpha == 0` transparency
  boundary, not byte-identical GIF palette order or a fixed transparent index.

Current implementation notes:

- `quantize_exact_rgba_gif_into` still preserves the exact-color path when the
  effective RGBA GIF palette fits in 256 entries.
- When the effective color set exceeds GIF palette capacity, the RGBA save path
  reserves one transparent palette slot if any `alpha == 0` pixel exists and
  quantizes non-transparent RGB colors into the remaining 255 or 256 slots
  using the existing weighted median-cut fallback.
- Fully transparent source pixels all map to the selected transparent palette
  index. Partial alpha remains treated as opaque RGB.
- Release x64 `pillow_c.dll` was rebuilt after the native change.

Verification evidence:

- Raw targeted red before implementation:
  `pillow_c image save_gif quantizes RGBA images above 256 colors`;
  failed with `Expected 0, got -3`.
- Facade targeted red before implementation:
  `Pillow Image.Save GIF quantizes RGBA images above 256 colors through native path`;
  failed with `pillow_c: invalid argument`.
- Raw targeted green after implementation:
  `Ran 1 tests in 63ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Facade targeted green after implementation:
  `Ran 1 tests in 203ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw full-file run succeeded:
  `Ran 368 tests in 21250ms; Passed: 368, Failed: 0, Errors: 0, Skipped: 0`.
- Facade full-file rerun succeeded:
  `Ran 375 tests in 37375ms; Passed: 375, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run initially ended without status during a long aggregate
  run, with no AHK stderr/stdout failure; a subsequent clean rerun succeeded.
- Full AHK directory rerun succeeded:
  `Ran 743 tests in 44312ms; Passed: 743, Failed: 0, Errors: 0, Skipped: 0`.

The previous completed GIF increment is:

```text
ID: FMT-GIF-003B1
Parent: FMT-GIF-003B
Title: RGB >256-color single-frame GIF save quantization
Status: covered
Scope: Single-frame RGB GIF save when the source has more than 256 unique
       colors. The implementation uses a deterministic weighted median-cut
       fallback after the exact palette path overflows.
Out of scope: RGBA >256 GIF save, animation RGBA quantization, dither parity,
              Pillow's exact palette ordering for every fixture, per-frame
              palette option matrix, include_color_table, and unrelated
              metadata work.
```

Resolved oracle/source notes:

- Fixture: `17x17` RGB with pixel `(x,y)` =
  `(x*15, y*15, ((x*9 + y*7) * 3) % 256)`, producing 289 source colors.
- Pillow 11.3.0 saves that fixture as `GIF87a`, mode `P`, with no transparency
  metadata and about 254 used palette indexes after reopen/convert.
- Installed source path checked:
  `GifImagePlugin._normalize_mode` routes RGB GIF save through
  `im.convert("P", palette=Image.Palette.ADAPTIVE)`, and
  `Image.Image.quantize` defaults RGB quantization to `MEDIANCUT`.
- The compatibility target for this slice is approximate reopened RGB pixels,
  not byte-identical GIF palette order.

Implementation notes:

- `quantize_exact_rgb_into` still preserves the exact-color path for
  `<= colors` unique RGB values.
- When unique RGB colors exceed the requested palette size, it falls back to
  `quantize_median_cut_rgb_into`.
- The fallback groups weighted unique colors, splits buckets by largest
  component range, emits weighted-average palette entries, and maps source
  pixels to nearest palette color.
- Release x64 `pillow_c.dll` was rebuilt after the native change.

Verification evidence:

- Targeted raw and facade tests failed red before implementation and pass after
  implementation.
- Raw full-file run passed: `367/367`.
- Facade full-file run passed: `374/374`.
- Full AHK directory suite passed: `741/741`.

The previous completed GIF increment is:

```text
ID: FMT-GIF-003A
Parent: FMT-GIF-003
Title: RGBA exact-color GIF save with binary transparency
Status: covered
Scope: Single-frame RGBA -> GIF when the effective palette has <= 256 entries.
       Pixels with alpha == 0 map to one transparent palette index. Pixels with
       nonzero alpha, including partial alpha, are treated as opaque RGB.
Out of scope: Full libimagequant-quality parity, median-cut/dither/fast-octree
              behavior, >256 colors, complex same-RGB transparent/opaque
              conflicts beyond the selected fixtures, animation RGBA
              quantization, per-frame local color-table combinations, and
              unrelated format metadata work.
```

Resolved Pillow 11.3.0 oracle behavior:

- For `[(255,0,0,255), (1,2,3,0), (0,255,0,255)]` saved as GIF:
  `info["transparency"] == 0`, palette starts with
  `[1,2,3, 255,0,0, 0,255,0]`, and P bytes are `[1,0,2]`.
- Partial alpha is opaque. Pixels with alpha `1` or `254` do not create an
  implicit transparent index.
- Multiple fully transparent source RGB values collapse to the first
  transparent RGB color for the probed tiny fixture. For
  `[(1,2,3,0), (4,5,6,0), (7,8,9,255)]`, Pillow writes palette
  `[1,2,3, 7,8,9, ...]`, P bytes `[0,0,1]`, and transparency index `0`.

Implementation notes:

- Native helper `quantize_exact_rgba_gif_into` handles this GIF-save-specific
  path without changing the broader `Image.quantize` contract.
- `save_gif_image` now accepts RGBA handles, exact-quantizes them into a native
  `P` temporary, and calls `save_gif_indexed_native`.
- Fully transparent pixels share one palette index. The first transparent
  pixel's RGB becomes that palette entry; nonzero-alpha pixels are treated as
  opaque RGB colors.
- If no fully transparent pixel is present, RGBA exact save emits no GIF
  transparency extension.
- The raw GIF unsupported-mode regression now uses CMYK so RGBA remains a
  supported save input.
- Release x64 build succeeded and updated
  `build\x64\Release\pillow_c.dll`.

Verification evidence:

- Raw targeted red before implementation:
  `pillow_c image save_gif quantizes exact RGBA transparency`;
  failed with `Expected 0, got -3`.
- Facade targeted red before implementation:
  `Pillow Image.Save GIF quantizes exact RGBA transparency through native path`;
  failed with `pillow_c: invalid argument`.
- Raw targeted green after implementation:
  `Ran 1 tests in 109ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Facade targeted green after implementation:
  `Ran 1 tests in 141ms; Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw full-file run succeeded:
  `Ran 366 tests in 14422ms; Passed: 366, Failed: 0, Errors: 0, Skipped: 0`.
- Facade full-file run succeeded:
  `Ran 373 tests in 31406ms; Passed: 373, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run succeeded:
  `Ran 739 tests in 43297ms; Passed: 739, Failed: 0, Errors: 0, Skipped: 0`.

The previous completed GIF increment is:

```text
ID: FMT-GIF-002E
Title: GIF animation save differencing and optimized local rectangles
Status: covered
Scope: Match Pillow's first practical P-mode animation differencing behavior:
       write local frame rectangles when only a sub-rectangle changes, merge
       identical frames by duration, and use an unused transparency index for
       unchanged pixels inside optimized local rectangles.
Out of scope: RGB/RGBA GIF save quantization, broad dither/quantize parity,
              per-frame palette option matrix, include_color_table, and all
              optimize/background/transparency combinations not exercised by
              the selected 3x3 fixture.
```

Known Pillow 11.3.0 probe/source results:

- Installed Pillow source inspected:
  `PIL.GifImagePlugin._write_multiple_frames`, `_write_frame_data`,
  `_write_local_header`, `_getbbox`, `_normalize_mode`, `_normalize_palette`,
  `_get_optimize`, and `ImagePalette.ImagePalette._new_color_index`.
- From the second frame onward, Pillow computes a delta/bounding box against
  the previous output frame. If the box is empty, the frame is not written and
  its duration is merged into the previous output frame.
- For the selected 3x3 P-mode animation where only the center pixel changes
  from palette index `0` to `1`, Pillow writes frame 0 as `(0,0,3,3)` and
  frame 1 as `(1,1,1,1)` with a local color table, GCE transparency index `2`,
  and `20ms` duration.
- For an identical second frame in the same fixture shape, Pillow writes one
  frame and merges durations to `30ms`.

Implementation notes:

- Raw regression:
  `pillow_c image save_gif_animation writes optimized local rectangles`.
- Facade regression:
  `Pillow Image.Save GIF save_all writes optimized local rectangles`.
- Native helpers touched include `gif_lzw_encode_indices`,
  `gif_table_size_code_for_entries`, `gif_color_table_entries_for_palette_size`,
  `GifAnimationRect`, `gif_difference_bbox`, `gif_find_unused_palette_index`,
  and `save_gif_animation_image`.
- The last failing symptom was misleading: reopening the saved GIF fell back to
  a WIC local-frame image, so later byte reads failed with `-2`. The underlying
  bug was the GIF LZW writer's code-size transition. The encoder now increases
  code size only after `next_code > (1 << code_size)`, matching the reader's
  threshold for these files.
- Release x64 build succeeded and updated
  `build\x64\Release\pillow_c.dll` to size `449536`, timestamp
  `2026-06-10 15:52:40`.

Verification evidence:

- Raw targeted run succeeded:
  `pillow_c image save_gif_animation writes optimized local rectangles`;
  `Ran 1 tests ... Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Raw full-file run succeeded:
  `Ran 365 tests in 16265ms; Passed: 365, Failed: 0, Errors: 0, Skipped: 0`.
- Facade targeted run succeeded:
  `Pillow Image.Save GIF save_all writes optimized local rectangles`;
  `Ran 1 tests ... Passed: 1, Failed: 0, Errors: 0, Skipped: 0`.
- Facade full-file rerun succeeded:
  `Ran 372 tests in 36031ms; Passed: 372, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run succeeded:
  `Ran 737 tests in 56890ms; Passed: 737, Failed: 0, Errors: 0, Skipped: 0`.

The previous completed GIF increment was:

```text
ID: FMT-GIF-002D
Title: GIF ImageSequence.Iterator live frame state
Status: covered
Scope: Confirm Pillow 11.3.0 `ImageSequence.Iterator` returns the original
       image object as a live seek-state view, not retained frame copies, while
       iterating over a complex transparent local-rectangle GIF fixture.
       Coverage records immediate per-frame pixels/metadata and the retained
       reference behavior after iteration advances to the final frame.
Out of scope: save-side frame differencing, transparency optimization, RGBA GIF
              save, general lossy quantization, and mutation side effects not
              exercised by Pillow source or the selected fixture.
```

Known Pillow 11.3.0 probe results:

- `PIL.ImageSequence.Iterator.__next__` seeks the wrapped image to the current
  iterator position and returns `self.im`.
- `PIL.ImageSequence.Iterator.__getitem__` seeks the wrapped image to the
  requested index and returns `self.im`; EOF is converted to
  `IndexError("end of sequence")`.
- For the transparent local-rectangle fixture from `FMT-GIF-002C`, iterator
  traversal matches repeated direct `seek` for frame pixels, frame mode,
  `info["duration"]`, and disposal method.
- Frames retained from the iterator are the same object as the source image.
  After iteration reaches frame 2, all retained references report frame 2,
  mode `RGB`, duration `30`, and final-frame bytes.
- Facade regression:
  `Pillow ImageSequence.Iterator keeps live GIF frame references`.

Verification evidence:

- Facade targeted run succeeded:
  `Ran 371 tests in 51296ms; Passed: 371, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run succeeded:
  `Ran 735 tests in 82390ms; Passed: 735, Failed: 0, Errors: 0, Skipped: 0`.
- No native code changed in this slice, so no DLL rebuild was required.

The previous completed GIF increment was:

```text
ID: FMT-GIF-002C
Title: GIF transparent local-rectangle composition
Status: covered
Scope: For later-frame local GIF rectangles with a Graphic Control Extension
       transparent color index, verify transparent pixels preserve the
       existing logical canvas while drawing. Also verify disposal `2` clears
       the whole local rectangle to the logical-screen background before the
       following frame, including transparent pixels. Coverage includes raw
       `open_gif_frame`, facade `Seek`, and facade `ImageSequence.Iterator`.
Out of scope: RGBA GIF promotion cases, save-side frame differencing,
              transparency optimization, RGBA GIF save, lossy quantization.
```

Known Pillow 11.3.0 probe results:

- A hand-crafted 3x3 GIF with frame 0 as a full green canvas, frame 1 as a
  2x1 local rectangle `[red, transparent]` at `(1, 1)`, and frame 2 as a 1x1
  blue local rectangle at `(0, 1)` distinguishes transparency from disposal.
- With frame 1 disposal `1`, Pillow frame 1 keeps the right pixel green and
  frame 2 preserves the red center while adding the blue left-middle pixel.
- With frame 1 disposal `2`, Pillow frame 1 still keeps the right pixel green,
  but frame 2 shows the whole frame-1 local rectangle restored to the black
  logical-screen background before the blue pixel is drawn.
- Later composited RGB GIF frames do not expose `info["transparency"]` in
  Pillow, even when the frame GCE carried a transparent color index. The facade
  now preserves `info["transparency"]` only for P-mode GIF frames.
- `ImageSequence.Iterator` returns the same frame pixels and transparency-info
  presence as repeated `seek` for these fixtures.
- Raw DLL regression:
  `pillow_c image open_gif_frame composes transparent local rectangles`.
- Facade regression:
  `Pillow Image.Open GIF composes transparent local rectangles`.

Verification evidence:

- Raw DLL targeted run succeeded:
  `Ran 364 tests ... Passed: 364, Failed: 0, Errors: 0, Skipped: 0`.
- Facade targeted run first failed on the expected RGB-frame
  `info["transparency"]` mismatch, then succeeded after the wrapper metadata
  fix:
  `Ran 370 tests ... Passed: 370, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run succeeded:
  `Ran 734 tests in 78781ms; Passed: 734, Failed: 0, Errors: 0, Skipped: 0`.
- No native code changed in this slice, so no DLL rebuild was required.

The previous completed GIF increment was:

```text
ID: FMT-GIF-002B
Title: GIF disposal 2/3 local-rectangle restoration
Status: covered
Scope: For later-frame local GIF rectangles, verify disposal method `2`
       restores the just-displayed rectangle to the logical-screen background
       color and disposal method `3` restores the canvas from before that
       frame. Coverage includes raw `open_gif_frame`, facade `Seek`, and
       facade `ImageSequence.Iterator`.
Out of scope: transparent local rectangles, save-side frame differencing,
              transparency optimization, RGBA GIF save, lossy quantization.
```

Known Pillow 11.3.0 probe results:

- A hand-crafted 3x3 GIF with frame 0 as a full green canvas, frame 1 as a
  1x1 red local rectangle at `(1, 1)`, and frame 2 as a 1x1 blue local
  rectangle at `(2, 1)` distinguishes the two restoration modes.
- With frame 1 disposal `2`, Pillow frame 2 is the green canvas with the red
  center restored to the black background and the blue pixel at the
  right-middle position.
- With frame 1 disposal `3`, Pillow frame 2 is the green canvas restored from
  before frame 1, plus the blue pixel at the right-middle position.
- `ImageSequence.Iterator` returns the same frame pixels as repeated `seek`
  for the two disposal fixtures.
- Raw DLL regression:
  `pillow_c image open_gif_frame restores disposal background and previous canvas`.
- Facade regression:
  `Pillow Image.Open GIF restores disposal background and previous canvas`.

Verification evidence:

- Raw DLL targeted run succeeded:
  `Ran 363 tests ... Passed: 363, Failed: 0, Errors: 0, Skipped: 0`.
- Facade targeted run succeeded after rerunning the known runner missing-status
  case:
  `Ran 369 tests ... Passed: 369, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run succeeded:
  `Ran 732 tests in 62109ms; Passed: 732, Failed: 0, Errors: 0, Skipped: 0`.
- No native code changed in this slice, so no DLL rebuild was required.

The previous completed GIF increment was:

```text
ID: FMT-GIF-002A
Title: GIF read-side local rectangle composition
Status: covered
Scope: For frame indexes after 0, parse GIF blocks in native code, LZW-decode
       local image rectangles, and compose them onto a logical RGB canvas so
       `open_gif_frame`, facade `Seek`, and `ImageSequence` no longer expose
       raw 1x1 local frame sizes for the covered path.
Out of scope: full disposal matrix tests, save-side frame differencing,
              transparency optimization, RGBA GIF save, lossy quantization.
```

Known Pillow 11.3.0 probe results:

- A hand-crafted 3x3 GIF with frame 1 as a 1x1 red local rectangle at `(1, 1)`
  and disposal `1`, followed by frame 2 as a 1x1 blue local rectangle at
  `(2, 1)`, returns frame 2 as a 3x3 RGB logical canvas.
- Frame 2 pixels preserve the red center from frame 1 and add the blue pixel at
  the right-middle position.
- Raw DLL regression:
  `pillow_c image open_gif_frame composes local disposal frames`.
- Facade regression:
  `Pillow Image.Open GIF composes local disposal frames`.

Verification evidence:

- MSBuild Release x64 succeeded with:
  `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\src\pillow_c.vcxproj /p:Configuration=Release /p:Platform=x64 /m`
- Raw DLL targeted run succeeded:
  `Ran 362 tests ... Passed: 362, Failed: 0, Errors: 0, Skipped: 0`.
- Facade targeted run succeeded:
  `Ran 368 tests ... Passed: 368, Failed: 0, Errors: 0, Skipped: 0`.
- Full AHK directory run succeeded:
  `Ran 730 tests in 60657ms; Passed: 730, Failed: 0, Errors: 0, Skipped: 0`.

Implemented shape:

- Adds native GIF block parsing for later-frame open.
- Adds GIF LZW index decode for the read path.
- Composes local rectangles against the logical screen in RGB.
- Handles transparency skips and implements disposal `1`, `2`, and `3` in the
  compositor, with current tests covering the disposal-`1` preservation case.
- Keeps frame `0` on the existing P-mode path and keeps a WIC fallback for
  unsupported later-frame parser cases.

The previous completed format increment was:

```text
ID: FMT-GIF-001
Title: GIF single-frame transparency index read/write
Status: covered
Scope: Parse Graphic Control Extension transparency on open/metadata and save
       a single-frame P-mode GIF with a caller-provided transparency index.
Out of scope: animation composition, per-frame animation transparency
              optimization, RGBA GIF save, lossy quantization.
```

Known Pillow 11.3.0 probe results:

- `P` image saved with `transparency=1` writes GIF89a and GCE bytes
  `21 f9 04 01 00 00 01 00`.
- Opened image has `info["transparency"] == 1` and `info["duration"] == 0`.
- `transparency=0` writes index `0`.
- `Transparency=1` is ignored by Python Pillow, but this AHK facade commonly
  accepts PascalCase option aliases for wrapper ergonomics.
- Integer transparency values are masked to one byte by Pillow behavior:
  `-1 -> 255`, `256 -> 0`.
- String or bytes transparency values raise in Python Pillow for the tested
  path.
- Minimal first increment should target `P` mode. `RGB` and `L` transparency
  save behavior can remain explicit future work.

Likely code entry points:

- Native metadata model: `src/pillow_c.cpp` `GifMetadata`.
- Native parser: `src/pillow_c.cpp` `read_gif_metadata`.
- Native save: `src/pillow_c.cpp` `save_gif_image`.
- Native exports: `src/pillow_c.cpp` near `pillow_c_image_gif_metadata` and
  `pillow_c_image_save_gif`.
- Raw AHK helper/tests: `ahk/pillow_c.test.ahk`
  `PillowCImageGifMetadata`, GIF tests near the current GIF block.
- Facade metadata: `ahk/pillow.ahk` `ApplyFrameMetadata`.
- Facade save dispatch: `ahk/pillow.ahk` `Save` and `SaveGifAnimation`.
- Existing palette alpha consumer: `ahk/pillow.ahk` `ApplyTransparency`.

Implemented shape:

- Preserves the current `pillow_c_image_gif_metadata` ABI.
- Adds an extended metadata export instead of changing existing call arity.
- Adds a single-frame GIF save-options export for transparency rather than
  overloading the existing save function.
- Masks integer transparency to `0..255` at the native boundary.
- Updates `docs/native-abi.md`, this document, and tests in the same patch.

Next recommended GIF work packet:

```text
ID: FMT-GIF-003
Title: RGB/RGBA GIF save quantization
Status: partial
Scope: Exact `L`/`RGB` and exact `RGBA` <=256-color save paths are covered.
       RGB >256-color single-frame save is covered as `FMT-GIF-003B1`.
       RGBA >256-color single-frame save is covered as `FMT-GIF-003B2`.
       Dither/options, animation quantization, and broad Pillow algorithm
       parity remain open under `FMT-GIF-003B`.
```

Current resume pointer for `FMT-GIF-002`:

- Read-side local rectangle preservation has first coverage.
- Disposal `2` and `3` restoration now has raw/facade/iterator coverage.
- Transparent local rectangles now have raw/facade/iterator coverage, including
  transparent-pixel preservation, disposal-`2` rectangle clearing, and facade
  RGB-frame transparency-info removal.
- Iterator live seek-state behavior now has facade coverage: retained iterator
  frames are the same source image object and report the final frame after
  iteration advances, matching Pillow's `ImageSequence.Iterator` source.
- Save-side frame differencing is covered for the selected 3x3 P-mode fixture:
  optimized local frame rectangles, identical-frame duration merge, and the
  selected default transparency-index behavior round-trip through raw DLL and
  facade tests.
- Broader GIF animation semantics remain partial because untested option
  combinations, per-frame palette cases, and general RGB/RGBA quantization are
  still separate gaps.

Concrete sub-slices for `FMT-GIF-002`:

| ID | Status | Gap | Oracle needed | Done when |
| --- | --- | --- | --- | --- |
| FMT-GIF-002A | covered | Later-frame local image rectangles compose to logical RGB canvas for the disposal-`1` preservation fixture. | Completed local Pillow probe. | Existing raw/facade tests and full suite pass. |
| FMT-GIF-002B | covered | Disposal `2` and `3` restoration behavior when a later frame leaves the old rectangle visible or cleared. | Completed local Pillow probe. | Raw/facade/iterator tests prove matching pixels. |
| FMT-GIF-002C | covered | Transparent local rectangles during animation composition. | Completed local Pillow probe. | Transparent pixels preserve or clear exactly as Pillow does. |
| FMT-GIF-002D | covered | `ImageSequence.Iterator` live seek-state behavior over a complex transparent local-rectangle GIF fixture. | Completed local Pillow probe and `PIL.ImageSequence.Iterator` source inspection. | Iterator returns the source image, retained references mutate to the final frame, and immediate frame pixels/metadata match Pillow. |
| FMT-GIF-002E | covered | Save-side frame differencing and optimized frame rectangles for the selected 3x3 P-mode fixture, including identical-frame duration merge and the selected transparency-index optimization. | Completed local Pillow source inspection for `GifImagePlugin._write_multiple_frames`, `_write_frame_data`, `_getbbox`, and palette/optimize helpers. | Raw/facade targeted tests and the full 737-test AHK suite pass. |

Suggested probe skeleton:

```powershell
@'
from PIL import Image, ImageSequence
import io

p0 = Image.new("P", (3, 3), 0)
p0.putpalette([0,0,0, 255,0,0, 0,255,0] + [0,0,0] * 253)
p1 = Image.new("P", (1, 1), 1)
p1.putpalette(p0.getpalette())

buf = io.BytesIO()
p0.save(
    buf,
    format="GIF",
    save_all=True,
    append_images=[p1],
    duration=[10, 20],
    disposal=[0, 0],
    loop=0,
)
data = buf.getvalue()
print(data.hex())

im = Image.open(io.BytesIO(data))
print("open", im.mode, im.size, dict(im.info), getattr(im, "dispose_extent", None), getattr(im, "tile", None))
for i, frame in enumerate(ImageSequence.Iterator(im)):
    print("frame", i, frame.mode, frame.size, dict(frame.info), getattr(frame, "dispose_extent", None), getattr(frame, "tile", None), list(frame.convert("RGB").getdata()))
'@ | & 'F:\Python\Python310\python.exe' -
```

## Strongly Covered Areas

These areas have both facade and raw DLL evidence unless noted otherwise.

### Core Handles And Modes

- Opaque native image handles.
- Mode-aware storage for `1`, `L`, `LA`, `RGB`, `RGBA`, `P`, `CMYK`, `I`,
  and `F`.
- Internal unpacked mode `1` storage with Pillow-style bit-packed external raw
  bytes.
- Palette metadata for `P`, including RGB/RGBA palette storage and alpha mode
  tracking.
- `CMYK` direct storage and broad operation coverage.
- Narrow but working `I` and `F` foundations for scalar storage and raw bytes.

Remaining gap:

- `I` and `F` are not full arithmetic/filter/file-format participants.
- Public `I;16*` mode family is not fully modeled as first-class image modes.
- Additional Pillow modes such as `YCbCr`, `LAB`, `HSV`, and plugin-specific
  raw modes are not generally implemented.

Next probes:

- Pillow conversion and arithmetic behavior for `I`, `F`, and `I;16*`.
- Source-level behavior for long-tail modes before adding public facade names.

### Raw Bytes And Pixel Data

Covered:

- `frombytes` / `tobytes` raw decoder and encoder paths for common raw modes.
- Direct `1`, `L`, `LA`, `RGB`, `RGBA`, `CMYK`, `I`, and `F` raw storage flows.
- BGR/BGRA/ARGB/ABGR/RGBX/BGRX-style byte shuffles.
- Stride and orientation handling for raw import.
- `getpixel`, `putpixel`, `getdata`, `putdata`, `DataPointer`, and storage
  sharing.

Remaining gap:

- `frombuffer`, `fromarray`, `fromarrow`, Qt constructors, and zero-copy
  external memory ownership are not implemented.
- Only raw decoder/encoder semantics are exposed through facade bytes APIs.

Next probes:

- `Image.frombuffer` ownership, readonly flags, and raw decoder defaults.
- `Image.fromarray` dtype/mode inference if NumPy support becomes a target.
- Whether AHK should expose shared-memory buffers directly or keep a narrow
  DLL-owned handle model for stability.

### Geometry, Resize, Transform

Covered:

- `crop`, `expand`, `resize`, `resize(box=...)`, `thumbnail`,
  `reducing_gap`, `reduce`.
- Resamplers: `NEAREST`, `BOX`, `BILINEAR`, `HAMMING`, `BICUBIC`, `LANCZOS`
  for resize.
- `contain`, `cover`, `fit`, `pad`.
- `transpose` methods matching Pillow constants.
- `transform` for `AFFINE`, `EXTENT`, `PERSPECTIVE`, `QUAD`, and `MESH`.
- `rotate` with `NEAREST`, `BILINEAR`, `BICUBIC`, expansion, center,
  translate, and fill color.
- `_into` variants for many allocation-reuse paths.
- `CMYK` geometry coverage.
- Premultiplied-alpha sampling for `LA`/`RGBA` filtered geometry.

Remaining gap:

- Transform/rotate support is intentionally limited to the listed resamplers.
- More exhaustive large-image, weird-box, and numeric edge tests would improve
  confidence.

Next probes:

- Edge behavior for negative and fractional boxes across all resamplers.
- Decompression-bomb and huge-image memory behavior, if this wrapper adopts
  Pillow's safety policy rather than leaving it to callers.

### ImageOps, ImageChops, ImageStat

Covered:

- `ImageOps`: invert, grayscale, mirror, flip, exif transpose basics,
  posterize, solarize, colorize, equalize, autocontrast, crop, expand, scale,
  contain, cover, fit, pad, deform.
- `ImageChops`: blend, composite, constant, duplicate, invert, difference,
  multiply, screen, soft/hard light, overlay, lighter, darker, add, subtract,
  modulo operations, logical mode `1` operations, offset.
- `ImageStat.Stat`: histogram-backed extrema, count, sum, sum2, mean, median,
  rms, variance, stddev.
- Masked histograms/statistics for mode `1`/`L` masks.

Remaining gap:

- Some operations cover selected modes rather than every Pillow-supported mode.
- EXIF transpose is based on currently parsed orientation behavior, not full
  EXIF lifecycle support.

Next probes:

- Per-mode failures for each ImageOps and ImageChops operation, especially
  palette, `I`, `F`, and alpha-bearing modes.
- Full EXIF orientation lifecycle once EXIF metadata write/read is in scope.

### Filters And Enhancements

Covered:

- `ImageFilter.Kernel` for 3x3 and 5x5.
- Built-in kernel filters: blur, contour, detail, edge enhance, emboss, find
  edges, sharpen, smooth.
- Rank, min, median, max, mode filters.
- Box blur, Gaussian blur, UnsharpMask.
- `Color3DLUT`.
- `ImageEnhance`: brightness, contrast, sharpness, color.
- Mode coverage includes `L`, `LA`, `RGB`, `RGBA`, and `CMYK` for many filters.

Remaining gap:

- Filter coverage for `I`, `F`, and long-tail modes remains incomplete.
- No SIMD or threading has been introduced yet.
- Performance benchmarks are not yet a first-class regression gate.

Next probes:

- Pillow C source behavior for `I` and `F` filters before exposing them.
- Baseline native timings for representative small, medium, and large images.

### Palette And Quantize

Covered:

- `putpalette`, `getpalette`, `apply_transparency`, `remap_palette`.
- Palette preservation across many operations.
- Quantize with reference palette.
- Exact unique-color quantization for simple `L`/`RGB` cases.

Remaining gap:

- General Pillow quantization algorithms are not complete.
- Missing or incomplete: median cut, max coverage, fast octree parity,
  k-means behavior, full dither behavior, libimagequant-style optional path,
  and broad RGBA/GIF quantization workflows.

Next probes:

- Pillow `Image.quantize` algorithm selection by mode and option.
- GIF save quantization behavior for RGBA/RGB images with more than 256
  colors.
- Dither matrix and palette stability for repeated saves.

### ImageDraw And ImageFont

Covered:

- Rectangle, rounded rectangle, bitmap, floodfill, ellipse, arc, chord,
  pieslice, line, curve joints, point, polygon, regular polygon.
- Default-font text draw, text length, text bbox, multiline text, multiline
  bbox.
- Default font handle lifecycle and metadata.
- Bounded stroke support for current default-font paths.
- Printable ASCII default-font glyph coverage.

Remaining gap:

- No full FreeType loading.
- No general Unicode shaping/glyph coverage.
- Missing advanced text layout: direction, language, OpenType features,
  variation axes/names, embedded color glyphs, and full `justify` semantics.
- Facade currently rejects non-`Pillow.ImageFont` fonts in text paths.

Next probes:

- Dependency choice for FreeType on Windows, including binary distribution.
- Whether text shaping should stay out of scope until the DLL has a stable
  font ABI and packaging story.

## File Format Coverage

Current native file format surface:

- BMP open/save.
- Netpbm PBM/PGM/PPM open/save, including high-bit-depth grayscale PGM as `I`.
- QOI RGB/RGBA open/save.
- TGA open/save, including RLE and 24-bit color-mapped `P`.
- XBM mode `1` open/save, including hotspot metadata.
- ICO open and PNG/BMP-backed save, including default and custom `sizes`.
- PNG open/save for `L`, `LA`, `P`, `RGB`, `RGBA`, with selected options.
- JPEG open/save for `L` and `RGB`, with quality and DPI/JFIF density paths.
- TIFF open/save for `L`, `RGB`, `RGBA`, plus basic frame open/count.
- GIF open/save for selected `P`, exact `L`/`RGB`, later-frame read-side local
  rectangle composition, and simple same-palette animation paths.

Important format gaps:

- Pillow has about `44` registered format families; this project covers the
  practical core subset, not the long tail.
- Missing major families include WebP, AVIF, JPEG2000, PDF, PSD, DDS, CUR,
  ICNS, PCX, SGI, SUN, EPS, MPO, FLI, DCX, XPM, and others.
- Even for covered formats, plugin options and metadata are partial.

Format risk rule:

- Treat every new format option as a small compatibility feature, not as a
  generic string option. Pillow plugin semantics are often format-specific and
  need local probes or source confirmation.

### PNG Gaps

Covered:

- Core decode/encode for `L`, `LA`, `P`, `RGB`, `RGBA`.
- Stored `compress_level=0`.
- DPI `pHYs` write/read.
- Palette-mode and `LA` semantics that WIC does not preserve reliably.

Remaining:

- Tuned native compression for levels `1..9`.
- Full metadata chunks: text, ICC, EXIF, gamma, chromaticity, transparency,
  interlace and optimization behaviors.
- Broader mode and edge-case parity.

Next probes:

- `PngImagePlugin` source for option precedence among `compress_level`,
  `optimize`, `bits`, `dictionary`, `icc_profile`, `exif`, and `pnginfo`.
- Exact `transparency` option behavior for `P`, `L`, and `RGB`.
- Interlace and text chunk round-trips.

### JPEG Gaps

Covered:

- `L` and `RGB` lossy encode/decode.
- Quality option.
- DPI/JFIF density write/read.
- Basic EXIF orientation probing for transpose workflows.

Remaining:

- EXIF write lifecycle.
- ICC profiles.
- Progressive, optimize, subsampling, qtables, comments, keep_rgb, and related
  options.
- CMYK/YCCK handling.
- More exact Pillow parity around marker preservation.

Next probes:

- WIC capability boundary for progressive/optimize/subsampling versus native
  marker patching or an alternate codec.
- Pillow `JpegImagePlugin` source for `quality`, `subsampling`, `qtables`,
  `keep_rgb`, and CMYK/YCCK behavior.

### TIFF Gaps

Covered:

- WIC-backed `L`, `RGB`, `RGBA` open/save.
- Basic multiframe open/count/seek.

Remaining:

- Multipage save.
- Compression options.
- Tag read/write parity.
- Palette, bilevel, high-bit, `I`, `F`, and many TIFF plugin paths.
- BigTIFF and advanced metadata are not covered.

Next probes:

- WIC support for compression and tags versus a native TIFF writer.
- Pillow `TiffImagePlugin` behavior for `save_all`, `append_images`, `tiffinfo`,
  `compression`, and high-bit modes.

### GIF Gaps

Covered:

- Frame `0` as `P` with palette.
- Later simple frames through current WIC/native paths.
- Duration, loop, background, disposal metadata parsing.
- Graphic Control Extension transparency index parsing into
  `info["transparency"]`.
- Later-frame GIF open/seek composes tested local rectangles into a logical
  RGB canvas instead of exposing the raw local frame size.
- Disposal `1`, `2`, and `3` read-side local-rectangle behavior is covered for
  the current hand-crafted fixtures.
- Transparent local-rectangle read-side composition is covered for the current
  hand-crafted disposal `1` and `2` fixtures.
- `ImageSequence.Iterator` live seek-state behavior is covered for a complex
  transparent local-rectangle fixture: immediate yielded frames match direct
  seek, and retained yielded references report the final frame like Pillow.
- Single-frame `P` save and exact-color `L`/`RGB` quantization.
- Single-frame exact-color `RGBA` GIF save for <=256 effective palette entries,
  including `alpha == 0` transparency and partial-alpha-as-opaque behavior.
- Single-frame `RGB` and `RGBA` GIF save above 256 effective colors for the
  bounded default-save fixtures, using deterministic weighted median-cut style
  fallbacks and preserving the verified `alpha == 0` transparency rule for
  RGBA.
- Single-frame GIF save `dither` options are pinned for the bounded RGB/RGBA
  >256 fixtures: Pillow 11.3.0 ignores the save option, and raw/facade tests
  assert deterministic/default-identical output.
- Single-frame P-mode GIF save with integer `Transparency`/`transparency`
  index.
- Simple same-size same-palette animation save.
- Save-side frame differencing is covered for the selected 3x3 P-mode
  animation fixture: frame 1 is written as a local `(1, 1, 1, 1)` rectangle,
  identical frames merge duration, and the selected default transparency-index
  optimization round-trips through raw DLL and facade tests.
- Save-side per-frame local palette behavior is covered for the selected
  P-mode animation fixture where frame 1 has a different RGB palette from
  frame 0. The writer compares palette RGB values for differencing and writes
  frame 1's own local color table.
- Explicit animation `include_color_table` and `optimize` save options are
  covered for the bounded P-mode fixtures: `include_color_table=True` forces a
  frame-0 local color table, `include_color_table=False` does not suppress
  later bbox-frame local tables, and `optimize=False` writes 4-entry color
  tables with `LzwMin=8` while disabling unchanged-pixel transparency
  substitution when no caller transparency is supplied.
- Caller-provided animation `background` is covered for the bounded
  logical-screen background-byte slice on the `3x1` per-frame palette fixture.

Remaining:

- Broader animation composition semantics.
- Save-side frame differencing and frame-rectangle optimization beyond the
  current `FMT-GIF-002E` fixture.
- Remaining bounded post-`disposal=2` edge cases after the covered
  transparency-aware re-diff fixture.
- Animation RGB/RGBA quantization before GIF differencing and behavior above
  256 colors outside the current bounded single-frame fixtures.
- Broader caller-provided `background` interactions, broader disposal
  combinations, and complex per-frame palettes.

Next probes:

- Continue with `FMT-GIF-004E` for the next bounded post-`disposal=2` edge
  case, especially frame-equals-restored-background, `optimize=False`, or a
  more pathological per-frame palette layout.
- Pillow `GifImagePlugin` source remains useful for option combinations not
  covered by the current fixture, especially caller-provided `transparency`
  and background handling.

### ICO/CUR Gaps

Covered:

- ICO open through WIC, returning largest frame as `RGBA`.
- PNG-backed ICO save with Pillow default sizes.
- Custom `sizes` option with sorting, de-duplication, skipping, and
  thumbnail-style LANCZOS containment.
- `sizes=[]` empty ICO directory behavior.
- BMP-backed ICO save via exact lowercase `bitmap_format="bmp"` for Pillow's
  `1`, `L`, `P`, `RGB`, and `RGBA` save modes.
- Uppercase or non-`bmp` bitmap format strings fall back to PNG-backed output
  like Pillow.

Remaining:

- `append_images`.
- Caller-selected frame open size.
- CUR cursor format and hotspot semantics.

Next probes:

- Pillow `IcoImagePlugin` frame selection and `append_images` behavior.
- CUR header/hotspot parsing and save option naming.

## Detailed Gap Ledger

Use these IDs when choosing the next task. A row is "covered" only when raw DLL
behavior, facade behavior where applicable, docs, and tests all agree.

| ID | Area | Status | Gap | Start in code/tests |
| --- | --- | --- | --- | --- |
| FMT-GIF-001 | GIF | covered | Single-frame transparency index read/write for `P` mode. | `pillow_c_image_gif_metadata_ex`, `pillow_c_image_save_gif_options`, `ApplyFrameMetadata`, GIF tests. |
| FMT-GIF-002 | GIF | partial | Read-side local rectangle composition covers disposal `1`/`2`/`3`, transparent local-rectangle fixtures, iterator live seek-state behavior, and one save-side optimized local-rectangle fixture; broader animation option matrix remains split into child gaps. | `open_gif_frame_image`, `save_gif_animation_image`, `ImageSequence` tests. |
| FMT-GIF-002A | GIF | covered | Later-frame local image rectangles compose to the logical RGB canvas for the covered disposal-`1` fixture. | `open_gif_composited_frame_image`, GIF local-disposal tests. |
| FMT-GIF-002B | GIF | covered | Disposal `2`/`3` restoration behavior for local rectangles not overwritten by the next frame. | `gif_draw_indexed_frame_rgb`, `open_gif_composited_frame_image`, raw/facade GIF tests. |
| FMT-GIF-002C | GIF | covered | Transparent local rectangles during animation composition. | GIF parser transparency path, local rectangle tests. |
| FMT-GIF-002D | GIF | covered | `ImageSequence.Iterator` live seek-state parity over complex transparent local rectangles and disposal state. | Facade `ImageSequence` tests. |
| FMT-GIF-002E | GIF | covered | Save-side frame differencing and optimized local frame rectangles for the selected 3x3 P-mode fixture, including identical-frame duration merge and selected transparency-index behavior. | `save_gif_animation_image`, animation save tests. |
| FMT-GIF-003 | GIF | partial | Exact `L`/`RGB`, exact `RGBA` <=256-color, RGB/RGBA >256-color single-frame default saves, and GIF-save dither-ignore behavior are covered; animation quantization and broad lossy quantization remain open. | Quantize code, `save_gif_image`, facade `Save`, GIF save tests. |
| FMT-GIF-003A | GIF | covered | Exact `RGBA` single-frame GIF save with binary transparency and partial-alpha-as-opaque behavior. | `quantize_exact_rgba_gif_into`, `save_gif_image`, raw/facade RGBA GIF save tests. |
| FMT-GIF-003B | GIF | partial | General RGB/RGBA GIF save quantization beyond exact <=256-color cases. RGB/RGBA >256 single-frame default-save fixtures and GIF-save dither-ignore behavior are covered; animation quantization and broader algorithm parity remain open. | Quantize algorithms, `save_gif_image`, `Image.quantize`, raw/facade GIF save tests. |
| FMT-GIF-003B1 | GIF | covered | RGB single-frame GIF save above 256 unique colors via bounded weighted median-cut fallback, verified through targeted raw/facade, full-file, and full-directory AHK tests. | `quantize_median_cut_rgb_into`, `quantize_exact_rgb_into`, raw/facade RGB >256 GIF tests. |
| FMT-GIF-003B2 | GIF | covered | RGBA single-frame GIF save above 256 effective colors while preserving the `alpha == 0` transparency boundary from `FMT-GIF-003A`, verified through targeted raw/facade, full-file, and full-directory AHK tests. | `quantize_exact_rgba_gif_into`, `quantize_median_cut_rgb_into`, `save_gif_image`, raw/facade RGBA >256 GIF tests. |
| FMT-GIF-003B3 | GIF | covered | Single-frame GIF save ignores `Dither`/`dither` options for the bounded RGB/RGBA >256 fixtures like Pillow 11.3.0, while public `Image.Quantize` remains a separate algorithm surface. | `save_gif_image`, `Image.Quantize`, facade save option parsing, raw/facade GIF save tests. |
| FMT-GIF-004 | GIF | partial | Per-frame local palettes, explicit `include_color_table`/`optimize`, caller-provided transparency/background, and the first bounded post-`disposal=2` transparency-aware re-diff slice are covered for P-mode animation work; the remaining gap is the next bounded post-`disposal=2` edge case under `FMT-GIF-004E`. | `save_gif_animation_image`, palette comparison tests. |
| FMT-GIF-004A | GIF | covered | GIF animation save supports later P frames with different RGB palettes by writing per-frame local color tables and comparing palette RGB values for differencing. | `save_gif_animation_image`, raw/facade per-frame local palette tests. |
| FMT-GIF-004B | GIF | covered | Explicit GIF animation `include_color_table` and `optimize` save-option behavior for bounded P-mode fixtures, including frame-0 local tables and `optimize=False` 4-entry tables with `LzwMin=8` and no unchanged-pixel transparency substitution. | `pillow_c_image_save_gif_animation_options`, facade SaveGifAnimation option parsing, descriptor tests. |
| FMT-GIF-004C | GIF | partial | Caller-provided GIF animation `transparency`, `background`, and their interaction with `include_color_table`/`optimize`. `FMT-GIF-004C1`, `FMT-GIF-004C2`, and `FMT-GIF-004C3` cover caller-provided transparency plus the bounded logical-screen `background` byte slice; the first bounded `disposal=2` plus transparency re-diff slice is now covered by `FMT-GIF-004D`. | Animation options ABI, facade SaveGifAnimation option parsing, descriptor and reopen tests. |
| FMT-GIF-004C1 | GIF | covered | Caller-provided `transparency` for optimized P-mode GIF animation local frames, using a bounded `3x1` per-frame palette fixture. | `pillow_c_image_save_gif_animation_metadata_options`, facade SaveGifAnimation `Transparency`/`transparency`, raw/facade descriptor and reopen tests. |
| FMT-GIF-004C2 | GIF | covered | Caller-provided `transparency` with `optimize=False` for P-mode GIF animation, including frame-0 and later-frame transparency GCEs, 4-entry global/local color tables, and `LzwMin=8` on the bounded fixture. | Metadata-options ABI branch, facade SaveGifAnimation combined options, raw/facade descriptor and reopen tests. |
| FMT-GIF-004C3 | GIF | covered | Caller-provided `background` for P-mode GIF animation, covering logical-screen background byte write plus facade/native metadata exposure on the bounded `3x1` fixture. | `pillow_c_image_save_gif_animation_background_options`, facade SaveGifAnimation `Background`/`background`, raw/facade descriptor and reopen tests. |
| FMT-GIF-004D | GIF | covered | Optimized P-mode GIF animation re-diffs the frame after `disposal=2` against a transparency-filled restored canvas when caller transparency is known, on the bounded shared-palette `3x1` fixture. | `save_gif_animation_image`, metadata-options ABI, raw/facade descriptor and reopen tests. |
| FMT-PNG-001 | PNG | remaining | Text chunks, ICC, EXIF, gamma/chromaticity, and metadata preservation. | `save_png` options path, metadata open/save tests. |
| FMT-PNG-002 | PNG | remaining | `transparency` option and `tRNS` chunk parity for `P`, `L`, and `RGB`. | PNG native chunk writer and facade save options. |
| FMT-PNG-003 | PNG | remaining | Nonzero native compression behavior, `optimize`, interlace, and advanced options. | `pillow_c_image_save_png_options`. |
| FMT-JPEG-001 | JPEG | remaining | ICC, EXIF write, comments, and marker preservation. | JPEG marker scan/patch path and facade save options. |
| FMT-JPEG-002 | JPEG | remaining | Progressive, optimize, subsampling, qtables, `keep_rgb`. | WIC capability probe, potential native encoder boundary. |
| FMT-JPEG-003 | JPEG | remaining | CMYK/YCCK decode/save semantics. | JPEG open mode probe and mode conversion tests. |
| FMT-TIFF-001 | TIFF | remaining | Multipage save via `save_all` and `append_images`. | TIFF frame-count/open APIs and facade save dispatch. |
| FMT-TIFF-002 | TIFF | remaining | Compression options and tag read/write surface. | `TiffImagePlugin` source probes, native/WIC split. |
| FMT-TIFF-003 | TIFF | remaining | Palette, bilevel, high-bit, `I`, `F`, and BigTIFF paths. | TIFF open/save mode matrix tests. |
| FMT-ICO-001 | ICO | remaining | `append_images` and multi-source icon entries. | ICO save options and facade save option parsing. |
| FMT-ICO-002 | ICO/CUR | remaining | Caller-selected ICO frame open size and CUR hotspot semantics. | ICO open API, XBM hotspot precedent. |
| FMT-WEBP-001 | WebP | not started | Open/save WebP and animation if a codec strategy is selected. | New format module boundary. |
| FMT-AVIF-001 | AVIF | not started | Open/save AVIF if dependency and packaging constraints allow it. | New format module boundary. |
| FMT-LONGTAIL-001 | Formats | not started | PDF, PSD, DDS, PCX, ICNS, SGI, SUN, EPS, MPO, FLI, DCX, XPM, and other registered families. | Add only when a real task needs one. |
| MODE-I-001 | Modes | partial | Full `I` and `I;16*` arithmetic, conversion, filters, and file-format participation. | Mode conversion, filters, raw bytes, Netpbm tests. |
| MODE-F-001 | Modes | partial | Full `F` arithmetic, conversion, filters, and file-format participation. | Float storage and filter source probes. |
| MODE-COLOR-001 | Modes | not started | `YCbCr`, `LAB`, `HSV`, and plugin-specific raw modes. | Mode registry and conversion matrix design. |
| BYTES-001 | Constructors | not started | `frombuffer` with ownership and readonly semantics. | Facade constructors and handle storage lifetime rules. |
| BYTES-002 | Constructors | not started | `fromarray`, `fromarrow`, Qt constructors. | Decide optional dependency policy first. |
| QUANT-001 | Quantize | partial | Median cut, max coverage, fast octree, k-means, dither parity. | Native quantize path and Pillow source probes. |
| META-001 | Metadata | partial | Full EXIF object behavior and save lifecycle. | JPEG/PNG/TIFF metadata and facade `getexif`. |
| META-002 | Metadata | not started | XMP, IPTC, ICC lifecycle, and `ImageCms` color management. | Dedicated metadata/color-management design. |
| DRAW-FONT-001 | Font | partial | FreeType loading, Unicode glyphs, shaping, variation axes, color glyphs. | New font ABI and dependency packaging. |
| DRAW-TEXT-001 | Text | partial | Advanced layout options: direction, language, OpenType features, `justify`. | ImageDraw text facade and native font backend. |
| OPS-001 | ImageOps/Chops | partial | Per-mode parity for palette, `I`, `F`, and long-tail modes. | Operation mode matrix tests. |
| GEOM-001 | Geometry | partial | Exhaustive weird boxes, numeric edge cases, and huge image behavior. | Resize/transform/rotate tests and memory checks. |
| PERF-001 | Performance | not started | Benchmark suite with budgets for hot paths. | New benchmark harness, not ahktest-only. |
| PERF-002 | Performance | not started | SIMD/threaded kernels behind stable ABI. | After scalar benchmark baselines exist. |
| ABI-001 | ABI | partial | Public C header, versioned ABI, and generalized save-options ABI. | `docs/native-abi.md`, export grouping. |
| TEST-001 | Testing | partial | Format matrix and oracle fixture expansion. | `oracle/pillow_oracle.py`, AHK tests. |
| TEST-002 | Testing | not started | Fuzz-ish decoder rejection and memory lifetime stress tests. | Native parsers and handle lifecycle. |
| PKG-001 | Packaging | partial | stdlib-quality examples, release process, compatibility table. | README plus docs, after APIs settle. |

## Open Gap Detail Cards

These cards expand the ledger rows into execution-ready work packets. Keep
them current enough that a future session can choose one card, open only the
named files, and begin with a bounded Pillow probe instead of re-auditing the
repository.

If a card conflicts with the ledger table, fix both in the same patch. If a
user asks for an area without a card, add the card before implementation.

### FMT-GIF-002D: Iterator State Parity

Status: covered.

Current covered baseline:

- `FMT-GIF-002A` proves local image rectangles compose into a logical RGB
  canvas for a disposal-`1` preservation fixture.
- `FMT-GIF-002B` proves disposal `2` and `3` local-rectangle restoration.
- `FMT-GIF-002C` proves transparent local-rectangle composition and RGB-frame
  transparency-info removal.
- Existing probes found `ImageSequence.Iterator` matched repeated `seek` for
  the current hand-crafted fixtures.
- The `FMT-GIF-002D` probe confirmed retained iterator frames remain live
  references to the source image and mutate to the final frame after
  iteration.

Resolved behavior:

- Pillow 11.3.0 `PIL.ImageSequence.Iterator.__next__` and `__getitem__` seek
  the wrapped image and return `self.im`.
- Retained yielded frames are not safe independent snapshots. Callers need
  `copy()`/`ImageSequence.all_frames` when they want stable frame objects.
- For the selected complex GIF fixture, immediate iterator pixels, mode, and
  duration match direct `seek`; retained references report the final frame
  after iteration.
- Mutation side effects between iterator steps are still deferred because the
  selected source and black-box probes only prove live seek-state identity, not
  arbitrary user mutation behavior.

Start in code/tests:

- `ahk/pillow.ahk`: `Pillow.ImageSequence.Iterator`, `Image.Seek`,
  `ApplyFrameMetadata`.
- `ahk/pillow.test.ahk`: GIF open/sequence block.
- Add raw DLL coverage in `ahk/pillow_c.test.ahk` only if the iterator probe
  exposes a native frame-composition gap.

Oracle/source work:

- Completed with `F:\Python\Python310\python.exe` and Pillow `11.3.0`.
- Installed `PIL/ImageSequence.py` was inspected for `Iterator.__next__` and
  `Iterator.__getitem__`.

Exit criteria:

- A facade test covers the selected iterator-only edge and records that AHK
  returns live seek-state views, not retained frame copies.
- Direct `Seek` and iterator behavior are both asserted when Pillow says they
  should match.
- This document records any iterator semantics deliberately deferred.

### FMT-GIF-002E: Animation Save Differencing

Status: covered for the selected 3x3 P-mode fixture.

Current covered baseline:

- The project can save simple same-size same-palette GIF animations.
- Read-side local-rectangle composition now handles several Pillow-like
  disposal and transparency fixtures.
- Raw and facade tests cover optimized local rectangles for a small P-mode
  animation where only the center pixel changes.
- The native writer now emits the expected frame descriptors, metadata, local
  color table, and LZW stream for the selected fixture. The saved file reopens
  through the native GIF compositor instead of falling back to WIC.

Resolved behavior for the current fixture:

- Pillow's current frame differencing rules were checked in
  `GifImagePlugin._write_multiple_frames`, `_write_frame_data`,
  `_write_local_header`, and `_getbbox`.
- Identical frames are dropped and their duration is merged into the previous
  output frame.
- A one-pixel center change in a 3x3 P-mode same-palette animation writes a
  second local rectangle `(1, 1, 1, 1)`.
- With default optimize behavior, Pillow allocates transparency index `2` for
  this fixture and writes it in the second frame's Graphic Control Extension.
- If the previous output frame disposal is `2`, the next frame tends toward a
  full-frame write for the currently scoped implementation.
- The native LZW encoder must increase code size only after
  `next_code > (1 << code_size)` for this fixture. The earlier equality
  threshold produced a stream that confused the native reader and caused a WIC
  fallback to a local-frame image.

Still unknown or deferred:

- Full option precedence for `background`, mixed disposal lists, and their
  interaction with `include_color_table`, `optimize=False`, and caller
  transparency.
- RGB/RGBA quantization before GIF differencing; keep that in `FMT-GIF-003`.
- Per-frame palette changes and palette reduction; keep that in `FMT-GIF-004`
  unless a small test exposes a required parser/writer fix.

Start in code/tests:

- `src/pillow_c.cpp`: `save_gif_animation_image`.
- `ahk/pillow.ahk`: `Save`, `SaveGifAnimation`, GIF option normalization.
- `ahk/pillow_c.test.ahk` and `ahk/pillow.test.ahk`: animation save blocks.

Oracle/source work:

- Completed for the current 3x3 P-mode fixture with local Python/Pillow
  `11.3.0`.
- This section keeps the durable `FMT-GIF-002E` evidence; do not rely on the
  moving `Latest Work Packet` heading to recover historical proof.

Verification evidence:

- At least one optimized local-rectangle animation save fixture round-trips
  through Pillow and the facade with matching pixels and core metadata.
- Raw targeted test passes:
  `pillow_c image save_gif_animation writes optimized local rectangles`.
- Facade targeted test passes:
  `Pillow Image.Save GIF save_all writes optimized local rectangles`.
- Full AHK directory suite passes from the parent runner with
  `-TimeoutSeconds 120`.
- Any unsupported options fail with captured errors or are documented as
  deferred rather than silently ignored.

### FMT-GIF-003: RGB/RGBA GIF Save Quantization

Status: partial. Completed child slices: `FMT-GIF-003A`,
`FMT-GIF-003B1`, `FMT-GIF-003B2`, and `FMT-GIF-003B3`. No current child is
open under this exact section; the next recommended broader GIF packet is
`FMT-GIF-004`. Broader remaining umbrella: `FMT-GIF-003B`.

Current covered baseline:

- Single-frame `P` save is covered.
- Exact-color `L`/`RGB` quantization is covered for simple cases.
- General Pillow quantization algorithms are not complete.
- `FMT-GIF-003A` covers exact-color `RGBA` single-frame GIF save for <=256
  effective palette entries, including `alpha == 0` transparency and
  partial-alpha-as-opaque behavior.
- `FMT-GIF-003B1` adds a deterministic RGB >256-color single-frame fallback
  and is covered by targeted raw/facade tests, raw and facade full-file tests,
  and the full AHK directory suite.
- `FMT-GIF-003B2` adds a deterministic RGBA >256-color single-frame fallback
  that preserves the verified `alpha == 0` transparency boundary and is
  covered by targeted raw/facade tests, raw and facade full-file tests, and
  the full AHK directory suite.
- `FMT-GIF-003B3` pins the single-frame GIF save dither/options boundary:
  Pillow ignores `dither` on GIF save for the selected RGB/RGBA >256 fixtures,
  while public `Image.Quantize` remains a separate algorithm surface.

Completed child slice:

```text
ID: FMT-GIF-003A
Title: RGBA exact-color GIF save with binary transparency
Status: covered
Target behavior: RGBA single-frame images with <= 256 effective palette entries
                 save through GIF by exact quantization. Fully transparent
                 pixels use one transparent palette index. Nonzero alpha is
                 opaque RGB.
```

Pillow 11.3.0 oracle results already resolved for `FMT-GIF-003A`:

- Fixture:
  `[(255,0,0,255), (1,2,3,0), (0,255,0,255)]`.
  Saved GIF has transparency index `0`, palette bytes
  `[1,2,3, 255,0,0, 0,255,0]`, and P bytes `[1,0,2]`.
- Partial alpha is opaque. Pixels with alpha `1` or `254` do not create an
  implicit transparent index.
- If multiple fully transparent source pixels have different RGB values, this
  slice follows Pillow's observed behavior for the tiny fixture: later fully
  transparent RGB values collapse to the first transparent palette entry.

Tests already added for `FMT-GIF-003A`:

- Raw DLL:
  `pillow_c image save_gif quantizes exact RGBA transparency`
  in `ahk/pillow_c.test.ahk`.
- Facade:
  `Pillow Image.Save GIF quantizes exact RGBA transparency through native path`
  in `ahk/pillow.test.ahk`.
- Both tests also assert that partial alpha does not create implicit GIF
  transparency.
- The old unsupported-input GIF save assertions were updated because RGBA GIF
  save is now supported; raw unsupported-mode coverage now uses CMYK, and the
  facade invalid GIF test covers invalid open rejection.

Historical verification for this child:

- Targeted raw and facade tests failed red before implementation and pass after
  implementation.
- Raw full-file, facade full-file, and full AHK directory suites pass:
  `366/366`, `373/373`, and `739/739`, all with `-TimeoutSeconds 120`.

Implementation notes from `FMT-GIF-003A`:

- Add an RGBA-specific exact quantize path rather than changing the broader
  `Image.quantize` contract in the same slice.
- Use one transparent palette entry for all `alpha == 0` pixels. The first
  transparent pixel's RGB becomes that entry for the scoped fixture.
- Count non-transparent RGB colors plus the optional transparent entry; reject
  if the result exceeds 256.
- Call `save_gif_indexed_native` with `has_transparency=true` and the selected
  transparent index when transparent pixels are present.
- If no transparent pixels are present, exact-quantize to `P` and save without
  a GIF transparency extension.

Completed child slice:

```text
ID: FMT-GIF-003B1
Title: RGB >256-color single-frame GIF save quantization
Status: covered
Target behavior: RGB source images with more than 256 unique colors save as GIF
                 through a native quantization fallback instead of rejecting.
```

Resolved behavior for this child:

- Fixture: `17x17` RGB, pixel `(x,y)` =
  `(x*15, y*15, ((x*9 + y*7) * 3) % 256)`, giving 289 source colors.
- Pillow 11.3.0 saves it as `GIF87a`, mode `P`, with no transparency and
  about 254 used palette indexes.
- Source path checked: `GifImagePlugin._normalize_mode` calls
  `im.convert("P", palette=Image.Palette.ADAPTIVE)` for RGB GIF save, and
  `Image.Image.quantize` defaults RGB to `MEDIANCUT`.
- Native compatibility target is approximate reopened RGB pixels:
  `max channel error <= 24` and `average Manhattan error <= 8` for the selected
  fixture, not byte-identical palette bytes.

Start in code/tests:

- `src/pillow_c.cpp`: quantize helpers, `save_gif_image`,
  `quantize_median_cut_rgb_into`, `quantize_exact_rgb_into`.
- Raw test:
  `pillow_c image save_gif quantizes RGB images above 256 colors`.
- Facade test:
  `Pillow Image.Save GIF quantizes RGB images above 256 colors through native path`.

Current verification:

- Targeted raw and facade tests failed red before implementation and pass after
  implementation.
- Raw full-file run passes: `367/367`.
- Facade full-file run passes: `374/374`.
- Full AHK directory suite passes: `741/741`.

Covered exit criteria:

- Facade full-file run passes from the parent runner with `-TimeoutSeconds 120`.
- Full AHK directory run passes from the parent runner with
  `-TimeoutSeconds 120`.
- `FMT-GIF-003B` remained partial after this child for the later-covered RGBA
  >256 slice, plus dither, animation quantization, and broader algorithm
  parity.

Completed child slice:

```text
ID: FMT-GIF-003B2
Title: RGBA >256-color single-frame GIF save quantization
Status: covered
Target behavior: RGBA source images with more than 256 effective colors save as
                 GIF through a lossy quantization fallback while preserving
                 the Pillow-observed `alpha == 0` transparency rule from
                 `FMT-GIF-003A`.
```

Resolved behavior for this child:

- Fixture: `18x18` RGBA with 324 source pixels, more than 256 effective
  colors, fully transparent pixels at `(0,0)`, `(5,7)`, and `(17,17)`, and
  partial-alpha pixels with alpha `1`.
- Pillow 11.3.0 saves the fixture as `GIF89a`, reopens as mode `P`, uses 256
  palette indexes, writes transparency metadata, maps every fully transparent
  source pixel to one transparent palette index, and treats partial alpha as
  opaque RGB.
- Native compatibility target is approximate reopened RGB pixels for
  non-transparent source pixels, plus the `alpha == 0` transparency boundary.
  The detailed oracle and verification evidence are retained in the
  `FMT-GIF-003B2` historical entry above.

Current verification:

- Targeted raw and facade tests failed red before implementation and pass after
  implementation.
- Raw full-file run passes: `368/368`.
- Facade full-file run passes: `375/375`.
- Full AHK directory suite passes: `743/743`.

Completed child slice:

```text
ID: FMT-GIF-003B3
Title: GIF quantization option and dither boundary
Status: covered
Target behavior: Explicit `Dither`/`dither` GIF save options are ignored for
                 the bounded RGB/RGBA >256 single-frame fixtures, matching
                 Pillow 11.3.0. Public `Image.Quantize` keeps its own dither
                 parameter and is not treated as the same option surface.
```

Resolved behavior for this child:

- Pillow output is byte-identical for default GIF save,
  `Image.Dither.NONE`, `Image.Dither.FLOYDSTEINBERG`, integer `0`, integer
  `3`, and arbitrary string values for the selected RGB/RGBA fixtures.
- `GifImagePlugin._normalize_mode` and `_normalize_palette` were inspected;
  neither consumes GIF save `dither` for these paths.
- `Image.Image.quantize` still exposes `dither`, but that is the public
  quantize operation, not a GIF save option for this slice.

Current verification:

- Raw targeted run passes:
  `pillow_c image save_gif quantization output is deterministic`.
- Facade targeted run passes:
  `Pillow Image.Save GIF ignores dither option like Pillow`.
- Raw full-file run passes: `369/369`.
- Facade full-file rerun passes: `376/376`.
- Full AHK directory suite passes: `745/745`.

Remaining child work under `FMT-GIF-003B` after `FMT-GIF-003B3`:

- Animation RGB/RGBA quantization before GIF differencing.
- Palette stability and algorithm differences across larger or pathological
  RGB fixtures.
- Public `Image.Quantize` algorithm parity beyond the already-covered exact
  and reference-palette slices.

### FMT-GIF-004: Per-Frame Palettes And Optimize

Status: partial. Completed child slices: `FMT-GIF-004A`, `FMT-GIF-004B`,
`FMT-GIF-004C1`, `FMT-GIF-004C2`, `FMT-GIF-004C3`, and `FMT-GIF-004D`.
Current next child: `FMT-GIF-004E`.

Current covered baseline:

- Pillow handling of one per-frame local color table case is covered: a later
  P frame with a different RGB palette from frame 0 saves with its own local
  color table and reopens with the intended color.
- The native writer now compares palette RGB values, not just palette indexes,
  when computing animation deltas and unchanged-pixel transparency fills.

Current covered option behavior:

- `include_color_table` and `optimize` behavior is pinned down for the bounded
  P-mode fixtures described under `FMT-GIF-004B`, with raw/facade tests.
- Caller-provided optimized animation `transparency` is covered for the
  `3x1` per-frame palette fixture described under `FMT-GIF-004C1`.
- Caller-provided `transparency + optimize=False` is covered for the same
  bounded `3x1` fixture described under `FMT-GIF-004C2`.
- Caller-provided `background` is covered for the same bounded `3x1` fixture
  at the logical-screen background-byte level described under `FMT-GIF-004C3`.
- The first bounded optimized post-`disposal=2` transparency-aware re-diff
  fixture is covered under `FMT-GIF-004D`.
- Default and `optimize=True` use optimized later local color tables with
  unchanged-pixel transparency. `optimize=False` keeps 4-entry color tables
  with `LzwMin=8` for the bounded fixtures and disables unchanged-pixel
  transparency optimization unless caller transparency is supplied.

Unknown behavior still to pin down:

- Which next bounded `disposal=2` edge case matters most after the covered
  optimized transparency-aware `3x1` fixture.
- How Pillow behaves when the following frame equals the restored background
  or when `optimize=False` is combined with a prior `disposal=2` frame.
- Complex per-frame palettes beyond the covered shared-palette `3x1` fixture.

Resolved negative result worth not re-probing:

- On the covered same-size `3x1` P-mode fixtures, varying caller
  `background` alone changes the GIF logical-screen background byte and
  reopened `info["background"]`, but does not change local-rectangle geometry,
  local/global table structure, or reopened RGB pixels.
- For local Pillow 11.3.0, `GifImagePlugin._write_multiple_frames` only takes
  a special post-`disposal=2` bbox path when a transparency index is known.
  Pure logical-screen `background` metadata is not the optimization driver for
  the next frame on the probed cases.

Completed child slice:

```text
ID: FMT-GIF-004D
Title: GIF animation disposal=2 transparency re-diff
Status: covered
Target behavior: Optimized P-mode GIF animation keeps a smaller post-restore
                 local rectangle after a `disposal=2` frame when caller
                 transparency lets Pillow re-diff against the restored
                 background on the bounded `3x1` fixture.
```

Resolved behavior for this child:

- Fixture: shared-palette `3x1` P-mode animation.
- Frame 0 pixels `[0,0,0]`, frame 1 pixels `[1,0,0]`, frame 2 pixels
  `[0,1,0]`.
- Save options: `duration=[10,20,30]`, `loop=0`, `disposal=[0,2,0]`,
  `transparency=2`.
- Local Pillow 11.3.0 writes frame 1 as local rectangle `[0,0,1,1]` with GCE
  disposal `2` and transparency `2`.
- Local Pillow 11.3.0 writes frame 2 as local rectangle `[1,0,1,1]` with no
  transparency GCE, not a full-frame rectangle.

Implementation notes:

- `save_gif_animation_image` now distinguishes the covered optimized
  caller-transparency plus `disposal=2` branch from the older full-frame
  fallback.
- For that branch, the native writer recomputes the next bbox against the RGB
  color resolved from the caller transparency index in the first/global
  palette, matching Pillow's bounded restore-background diff behavior.
- The covered branch intentionally leaves the following frame without a
  transparency GCE, matching Pillow's bounded output on the selected fixture.
- Release x64 `pillow_c.dll` was rebuilt after the native change.

Verification:

- Raw red before implementation:
  `pillow_c image save_gif_animation_metadata_options re-diffs after disposal 2 with transparency`
  failed with frame 3 descriptor `[0,0,3,1]`.
- Facade red before implementation:
  `Pillow Image.Save GIF save_all re-diffs after disposal 2 with transparency`
  failed with frame 3 descriptor `[0,0,3,1]`.
- Raw full-file run passes: `375/375`.
- Facade targeted rerun passes: `1/1`.
- Full AHK directory suite passes:
  `Ran 757 tests in 40985ms; Passed: 757, Failed: 0, Errors: 0, Skipped: 0`.

Completed child slice:

```text
ID: FMT-GIF-004A
Title: GIF animation per-frame local palettes
Status: covered
Target behavior: A later P frame with a different RGB palette from frame 0 can
                 be saved through the native GIF animation path. The changed
                 frame writes a local color table and round-trips through
                 native open as the intended RGB pixels.
```

Resolved behavior for this child:

- Fixture: `3x1` P-mode animation. Frame 0 has pixels `[0, 0, 0]` and palette
  index `1` = red. Frame 1 has pixels `[0, 1, 0]` and palette index `1` =
  blue.
- Pillow 11.3.0 default save writes frame 1 as local rectangle
  `(1, 0, 1, 1)`, uses a local color table whose first entries are
  `[0,0,0, 0,0,255, 0,0,0, 0,0,0]`, and uses transparency index `2` for the
  optimized unchanged pixels.
- Source checked: `GifImagePlugin._write_multiple_frames`,
  `_write_local_header`, `_get_optimize`, and `_normalize_palette`.

Implementation notes:

- `save_gif_animation_image` now validates each frame palette but no longer
  requires later P-frame palettes to equal frame 0.
- Later changed frames write their own `palette_rgb` as a local color table.
- The native delta bounding-box and unchanged-pixel transparency logic compare
  resolved palette RGB values, preventing same-index/different-color frames
  from being merged incorrectly.
- Release x64 `pillow_c.dll` was rebuilt after the native change.

Verification:

- Raw red before implementation:
  `pillow_c image save_gif_animation writes per-frame local palette` failed
  with `Expected 0, got -5`.
- Facade red before implementation:
  `Pillow Image.Save GIF save_all writes per-frame local palette` errored with
  `pillow_c: mismatch`.
- Raw full-file run passes: `370/370`.
- Facade full-file run passes: `377/377`.
- Full AHK directory suite passes:
  `Ran 747 tests in 39796ms; Passed: 747, Failed: 0, Errors: 0, Skipped: 0`.

Completed child slice:

```text
ID: FMT-GIF-004B
Title: Explicit GIF animation include_color_table and optimize options
Status: covered
Target behavior: Facade/native handling for `include_color_table` and
                 `optimize` animation save options on the bounded P-mode
                 fixtures.
```

Implementation notes:

- `pillow_c_image_save_gif_animation_options` extends the animation save ABI
  with tri-state `include_color_table` and `optimize` integers.
- The existing `pillow_c_image_save_gif_animation` export keeps default
  behavior by passing both options as unset.
- The facade accepts `IncludeColorTable`/`include_color_table` and
  `Optimize`/`optimize`.
- `include_color_table=True` forces a first-frame local color table.
  `include_color_table=False` keeps default first-frame behavior and does not
  suppress later bbox-frame local color tables.
- `optimize=False` writes 4-entry color tables with `LzwMin=8` for the bounded
  2-color fixture and disables unchanged-pixel transparency substitution on
  the covered changed-frame path when no caller transparency is supplied.

Resolved oracle/source notes:

- Fixtures:
  - `same`: `3x3` P animation, same palette, center pixel changes to red.
  - `diff`: `3x1` P animation. Frame 0 palette index `1` is red but pixels are
    all black; frame 1 palette index `1` is blue and pixels are `[0,1,0]`.
- Default and `optimize=True`: frame 0 has no local color table; the changed
  later frame has a local color table and uses transparency index `2` for
  unchanged pixels.
- `include_color_table=True`: frame 0 also gets a local color table. The later
  changed frame still has a local color table.
- `include_color_table=False`: same as default for frame 0. It does not
  suppress the later changed-frame local table because Pillow's bbox frame path
  forces `include_color_table` when no palette is available there.
- `optimize=False`: global and later local color tables use 4 entries,
  preserve `LzwMin=8`, and no unchanged-pixel transparency index is used on
  the changed later frame.
- `include_color_table=True, optimize=False`: frame 0 and the changed later
  frame both have 4-entry local color tables with `LzwMin=8`; no
  unchanged-pixel transparency optimization is used.
- Installed Pillow 11.3.0 source checked: `GifImagePlugin._write_multiple_frames`,
  `_write_local_header`, `_get_optimize`, and `_normalize_palette`.

Verification:

- Raw helper added:
  `PillowCImageSaveGifAnimationOptions(handles, path, durations?, loopCount := -1, disposals?, includeColorTable := -1, optimize := -1)`,
  backed by `pillow_c_image_save_gif_animation_options`.
- Raw test added and passing:
  `pillow_c image save_gif_animation_options controls color tables and optimize`.
- Facade test added and passing:
  `Pillow Image.Save GIF save_all honors include_color_table and optimize options`.
- Covered fixture uses two `3x1` P frames: frame 0 pixels `[0,0,0]`
  with palette `[black, red]`; frame 1 pixels `[0,1,0]` with palette
  `[black, blue]`; calls options with durations `[10,20]`, loop `3`,
  disposals `[0,0]`, `includeColorTable=1`, and `optimize=0`.
- Release x64 build succeeded with `0 Warning(s), 0 Error(s)`.
- Raw full-file run passes: `371/371`.
- Facade full-file run passes: `378/378`.
- Full AHK directory suite passes:
  `Ran 749 tests in 38407ms; Passed: 749, Failed: 0, Errors: 0, Skipped: 0`.

Completed child slice:

```text
ID: FMT-GIF-004C1
Title: Caller-provided GIF animation transparency
Status: covered
Target behavior: Facade/native handling for `transparency` animation save
                 options on optimized same-size P-mode local-frame output.
```

Implementation notes:

- `pillow_c_image_save_gif_animation_metadata_options` extends the animation
  save-options ABI with `has_transparency` and `transparency`.
- The existing default and include/optimize exports continue to call
  `save_gif_animation_image` with unset transparency, preserving earlier ABI
  behavior.
- The facade accepts `Transparency`/`transparency` and routes through the new
  metadata-options export only when the caller supplies the option.
- Optimized later local frames use the caller transparency index instead of
  choosing the native unused index. If the caller index is already inside the
  source palette, the writer pads one zero-color entry before computing the
  local color table size so the covered fixture matches Pillow's 4-entry
  local table.

Resolved oracle/source notes:

- Fixture: `3x1` P animation. Frame 0 pixels `[0,0,0]` with palette
  `[black, red]`; frame 1 pixels `[0,1,0]` with palette `[black, blue]`.
- Default optimized save uses frame 1 GCE transparency index `2`.
- `transparency=1` keeps frame 0 without a transparency GCE, writes frame 1
  GCE transparency index `1`, writes the changed local frame as rectangle
  `[1,0,1,1]` with the blue local palette at index `1`, and reopens frame 1
  as all black because local index `1` is transparent.
- `transparency=1,optimize=False` is covered by `FMT-GIF-004C2`.

Verification:

- Raw helper added:
  `PillowCImageSaveGifAnimationMetadataOptions(...)`, backed by
  `pillow_c_image_save_gif_animation_metadata_options`.
- Raw test added and passing:
  `pillow_c image save_gif_animation_metadata_options honors transparency`.
- Facade test added and passing:
  `Pillow Image.Save GIF save_all honors transparency option`.
- Release x64 build succeeded with `0 Warning(s), 0 Error(s)`.
- Raw full-file run passes: `372/372`.
- Facade full-file run passes: `379/379`.
- Full AHK directory suite passes:
  `Ran 751 tests in 42312ms; Passed: 751, Failed: 0, Errors: 0, Skipped: 0`.

Completed child slice:

```text
ID: FMT-GIF-004C2
Title: Caller-provided GIF animation transparency with optimize=False
Status: covered
Target behavior: Raw/facade coverage for caller-provided `transparency` when
                 `optimize=False`, including frame-0 and later-frame
                 transparency GCEs, 4-entry global/local color tables, and
                 `LzwMin=8` on the bounded P-mode fixture.
```

Resolved notes:

- Local Pillow 11.3.0 writes transparency index `1` on both frames for the
  selected `3x1` fixture when saved with `transparency=1,optimize=False`.
- Frame 0 uses the 4-entry global color table and `LzwMin=8`.
- The changed later local rectangle uses a 4-entry local color table and
  `LzwMin=8`.
- Reopened frame 1 is all black because the blue local index is transparent.

Verification:

- Raw full-file run passes: `373/373`.
- Facade full-file run passes: `380/380`.
- Full AHK directory suite passes:
  `Ran 753 tests in 45219ms; Passed: 753, Failed: 0, Errors: 0, Skipped: 0`.

Historical next child at that point:

```text
ID: FMT-GIF-004C3
Title: Caller-provided GIF animation background option
Status: remaining
Target behavior: Probe and cover caller-provided `background` on the bounded
                 P-mode animation fixture, including logical-screen
                 background index and interactions with existing transparency
                 and disposal metadata.
```

Start in code/tests:

- `src/pillow_c.cpp`: `save_gif_animation_image` metadata/options path.
- `ahk/pillow.ahk`: `Background`/`background` option dispatch.
- Raw/facade descriptor tests around the existing `FMT-GIF-004C1` and
  `FMT-GIF-004C2` animation save fixtures.

Exit criteria:

- The selected `background` option combination has raw/facade tests or
  documented captured errors.
- Existing `FMT-GIF-004A`, `FMT-GIF-004B`, `FMT-GIF-004C1`, and
  `FMT-GIF-004C2` behavior stays green.

Current next child after `FMT-GIF-004D` is `FMT-GIF-004E`; use the checkpoint
and the `Current Work Packet` section above for the current execution target.

### PNG Metadata And Save Options

Rows: `FMT-PNG-001`, `FMT-PNG-002`, `FMT-PNG-003`.

Status: remaining.

Current covered baseline:

- Native PNG open/save covers `L`, `LA`, `P`, `RGB`, and `RGBA`.
- `compress_level=0`, DPI `pHYs`, palette mode, and `LA` behavior have
  focused coverage.

Unknown behavior to pin down:

- Text chunks, ICC, EXIF, gamma, chromaticity, interlace, and transparency
  chunk rules by mode.
- Option precedence among `compress_level`, `optimize`, `bits`,
  `dictionary`, `icc_profile`, `exif`, and `pnginfo`.
- Whether native zlib work is enough or whether some metadata can be patched
  around the existing writer.

Start in code/tests:

- `src/pillow_c.cpp`: PNG chunk writer/reader and save-options export.
- `ahk/pillow.ahk`: save option parsing and metadata propagation.
- PNG metadata and save-option tests in both raw and facade suites.

Exit criteria:

- Each option gets a narrow oracle fixture and a ledger row update. Do not
  claim "PNG metadata" covered until individual chunks are tested.

### JPEG Options And Metadata

Rows: `FMT-JPEG-001`, `FMT-JPEG-002`, `FMT-JPEG-003`.

Status: remaining.

Current covered baseline:

- `L` and `RGB` JPEG open/save work through current native/WIC paths.
- Quality and DPI/JFIF density are covered.
- Basic EXIF orientation is used for transpose workflows.

Unknown behavior to pin down:

- EXIF write lifecycle, ICC profiles, comments, marker preservation.
- Progressive, optimize, subsampling, qtables, and `keep_rgb`.
- CMYK/YCCK decode/save behavior and whether WIC can support it acceptably.

Start in code/tests:

- JPEG marker scan/patch path in `src/pillow_c.cpp`.
- Facade save option parsing in `ahk/pillow.ahk`.
- JPEG option and metadata test blocks.

Exit criteria:

- Each slice states whether it is WIC-backed, marker-patched, or requires a
  new codec dependency. Unsupported options must be explicit.

### TIFF Multipage, Tags, And Modes

Rows: `FMT-TIFF-001`, `FMT-TIFF-002`, `FMT-TIFF-003`.

Status: remaining.

Current covered baseline:

- WIC-backed `L`, `RGB`, and `RGBA` open/save.
- Basic multiframe open/count/seek.

Unknown behavior to pin down:

- `save_all`, `append_images`, compression options, tag read/write, palette,
  bilevel, high-bit, `I`, `F`, BigTIFF, and metadata behavior.
- Which parts can remain WIC-backed and which require a native TIFF path.

Start in code/tests:

- TIFF frame-count/open APIs in `src/pillow_c.cpp`.
- Facade save dispatch and metadata mapping.
- TIFF mode matrix and multipage tests.

Exit criteria:

- Multipage save and tag work are treated as separate increments. Do not mix
  them into a single broad TIFF patch.

### ICO And CUR

Rows: `FMT-ICO-001`, `FMT-ICO-002`.

Status: remaining.

Current covered baseline:

- ICO open returns the largest frame as `RGBA`.
- PNG-backed and BMP-backed ICO save are covered for several modes and sizes.
- XBM hotspot metadata provides a precedent for cursor hotspot handling.

Unknown behavior to pin down:

- `append_images` and multi-source icon entries.
- Caller-selected frame open size.
- CUR header/hotspot parse/save semantics and option names.

Start in code/tests:

- ICO open/save APIs in `src/pillow_c.cpp`.
- Facade open/save option parsing.
- ICO/CUR tests, with CUR split out if the ABI needs a new entry point.

Exit criteria:

- ICO multi-entry behavior and CUR hotspot behavior each get their own tests
  and documentation.

### New Format Families

Rows: `FMT-WEBP-001`, `FMT-AVIF-001`, `FMT-LONGTAIL-001`.

Status: not started.

Decision boundary:

- Do not start a new format family until there is a user task or a packaging
  decision for its codec dependency.
- For WebP/AVIF, decide open-only, save-only, still-only, and animation scope
  before writing ABI.
- Long-tail formats should be added one at a time with a real fixture and
  source/oracle notes.

Exit criteria:

- A new format must include a clear dependency strategy, raw tests, facade
  tests, docs, and DLL artifact updates if native code changes.

### Modes, Constructors, And Numeric Semantics

Rows: `MODE-I-001`, `MODE-F-001`, `MODE-COLOR-001`, `BYTES-001`,
`BYTES-002`.

Status: partial or not started.

Current covered baseline:

- Core modes `1`, `L`, `LA`, `P`, `RGB`, and `RGBA` have significant
  operation and file-format coverage.
- Some `I` coverage exists through high-bit-depth PGM.

Unknown behavior to pin down:

- `I` and `I;16*` conversion, arithmetic, filter, endian, and raw byte rules.
- `F` conversion, arithmetic, filter, NaN/Inf, and save restrictions.
- `YCbCr`, `LAB`, `HSV`, and plugin-specific raw mode behavior.
- `frombuffer` ownership, readonly, stride/orientation, and lifetime rules.
- Optional dependency policy for `fromarray`, `fromarrow`, and Qt constructors.

Start in code/tests:

- Native image storage and conversion functions in `src/pillow_c.cpp`.
- Facade constructors and handle lifetime logic in `ahk/pillow.ahk`.
- Mode conversion, raw bytes, and constructor tests.

Exit criteria:

- Each mode or constructor gets a small matrix with Pillow oracle results,
  explicit memory ownership rules, and failure-mode tests.

### Quantize And Palette Algorithms

Row: `QUANT-001`.

Status: partial.

Current covered baseline:

- Palette preservation, `putpalette`, `getpalette`, `apply_transparency`,
  `remap_palette`, reference-palette quantize, and exact unique-color
  quantization for simple `L`/`RGB` cases.

Unknown behavior to pin down:

- Median cut, max coverage, fast octree, k-means, dither parity, optional
  libimagequant-style behavior, and RGBA/GIF quantization workflows.

Start in code/tests:

- Native quantize code in `src/pillow_c.cpp`.
- `Image.quantize` facade and GIF save paths.

Exit criteria:

- A chosen algorithm slice has oracle fixtures, deterministic output
  expectations, and documented differences where exact Pillow parity is not
  practical.

### Metadata And Color Management

Rows: `META-001`, `META-002`.

Status: partial or not started.

Current covered baseline:

- Basic `info` propagation, PNG/JPEG DPI, XBM hotspot, GIF animation metadata,
  and basic EXIF orientation read path.

Unknown behavior to pin down:

- Full EXIF object behavior, XMP, IPTC, ICC profile lifecycle, `ImageCms`, and
  metadata preservation across save paths.

Start in code/tests:

- Format-specific metadata read/write paths.
- Facade `info`, `getexif`, and save option mapping.

Exit criteria:

- Metadata work is format-scoped unless a stable shared metadata ABI is being
  deliberately designed.

### Font And Text Rendering

Rows: `DRAW-FONT-001`, `DRAW-TEXT-001`.

Status: partial.

Current covered baseline:

- Default-font text drawing, text length, bbox, multiline variants, stroke for
  current paths, and printable ASCII default-font glyph coverage.

Unknown behavior to pin down:

- FreeType loading, Unicode glyph coverage, shaping, direction, language,
  OpenType features, variation axes/names, embedded color glyphs, and full
  `justify` semantics.

Start in code/tests:

- New font ABI and dependency packaging decision.
- `ImageDraw` text facade and native text backend.

Exit criteria:

- Dependency choice and ABI are documented before broad Unicode text work
  begins.

### Operations, Geometry, And Huge Images

Rows: `OPS-001`, `GEOM-001`.

Status: partial.

Current covered baseline:

- Many common `ImageOps`, `ImageChops`, filters, resize, rotate, transform,
  crop, paste, point, histogram, stats, and draw operations are present.

Unknown behavior to pin down:

- Per-mode parity for palette, `I`, `F`, and long-tail modes.
- Weird boxes, numeric edge cases, clipping, rounding, and huge-image memory
  behavior.

Start in code/tests:

- Operation-specific native functions and facade wrappers.
- Mode matrix tests and memory/lifetime checks.

Exit criteria:

- Add mode-by-mode fixtures only for the operation being changed. Avoid a
  one-shot "all operations" audit.

### Performance, ABI, Testing, And Packaging

Rows: `PERF-001`, `PERF-002`, `ABI-001`, `TEST-001`, `TEST-002`, `PKG-001`.

Status: partial or not started.

Current covered baseline:

- Many hot paths already call into native code.
- Handle-owned contiguous storage and `_into` allocation reuse variants exist.
- README and docs are split so README does not carry all iteration history.

Unknown behavior to pin down:

- Benchmark budgets for representative image sizes and workloads.
- SIMD/threaded kernels behind stable ABI.
- Public C header, ABI versioning, and generalized options ABI.
- Oracle fixture generation, decoder rejection tests, memory stress tests, CI,
  release verification, and compatibility matrix.

Start in code/tests:

- New benchmark harness outside normal `ahktest` correctness tests.
- `docs/native-abi.md`, eventual public header, and release docs.
- `oracle/pillow_oracle.py` or equivalent fixture tooling.

Exit criteria:

- Performance claims require repeatable benchmark output.
- ABI changes require docs and compatibility notes in the same patch.
- Packaging work should make a release easier to consume without moving
  volatile iteration notes back into README.

## Source-Probe Rules

Some Pillow behaviors are too specific to infer from black-box tests alone.
Source inspection is appropriate when:

- Pillow behavior depends on plugin option precedence.
- A native algorithm must match integer rounding, clipping, or palette choice.
- The observed fixture result is ambiguous across modes.
- The feature touches metadata preservation or animation state.

Preferred order:

1. Create a tiny Python fixture with `F:\Python\Python310\python.exe`.
2. Inspect installed Pillow source for the exact local version when present.
3. If local source is insufficient, inspect Pillow 11.3.0 upstream source.
4. Encode the discovered rule in AHK tests before implementing.

When documenting source-derived behavior, name the Pillow module or function
that was checked so future work can verify drift after Pillow upgrades.

## Facade API Gaps

Rough comparison against `PIL.Image.Image` public methods shows the current
facade has analogues for most common methods:

- Present or roughly present: `alpha_composite`, `apply_transparency`, `blend`,
  `close`, `composite`, `convert`, `copy`, `crop`, `draft`, `effect_spread`,
  `entropy`, `eval`, `filter`, `frombytes`, `getbands`, `getbbox`,
  `getchannel`, `getcolors`, `getdata`, `getextrema`, `getpalette`,
  `getpixel`, `getprojection`, `histogram`, `load`, `merge`, `paste`,
  `point`, `putalpha`, `putdata`, `putpalette`, `putpixel`, `quantize`,
  `reduce`, `remap_palette`, `resize`, `rotate`, `save`, `seek`, `split`,
  `tell`, `thumbnail`, `tobitmap`, `tobytes`, `transform`, `transpose`,
  `verify`.
- Missing or not currently meaningful in this AHK/native design:
  `get_child_images`, `getexif`, `getim`, `getxmp`, `show`, `toqimage`,
  `toqpixmap`.

Additional constructor/module gaps:

- `fromarray`.
- `frombuffer`.
- `fromarrow`.
- `fromqimage`.
- `fromqpixmap`.
- Full plugin registration APIs are not exposed as public extension points.

The facade intentionally uses some AHK-friendly casing and helper methods.
Python-like aliases exist for many methods, but source-compatible naming is not
complete across the whole surface.

## Metadata And Color Management Gaps

This is one of the largest remaining parity areas.

Covered:

- Basic `info` map propagation for many derived images.
- PNG DPI.
- JPEG DPI/JFIF fields.
- XBM hotspot.
- GIF duration/loop/background/disposal/transparency.
- Basic EXIF orientation read path for transpose behavior.

Remaining:

- Full EXIF object behavior.
- XMP.
- ICC profile lifecycle.
- IPTC and other plugin metadata.
- Color management via `ImageCms`.
- Metadata preservation on save across formats.
- General plugin-specific `info` dictionaries.

## Performance And ABI Gaps

Strong direction already in place:

- Coarse native calls for hot operations.
- Handle-owned contiguous storage.
- `_into` allocation-reuse variants for many operations.
- Direct data pointer sharing with explicit lifetime rules.

Still missing:

- Benchmark suite with fixed image sizes and budgets.
- SIMD or threaded kernels.
- Streaming/tiled decode/encode for large images.
- Stable public C header and versioned ABI documentation.
- A more general native options ABI for format saves, instead of adding a new
  export for every option cluster.
- Memory-leak and lifetime stress tests.

## Testing Gaps

Current testing is strong for implemented behavior but not exhaustive for full
Pillow parity.

Covered:

- 757 AHK tests split between facade and raw DLL.
- Many tests compare small fixtures to local Pillow behavior.
- Tests use `ahktest` and captured errors.
- Current test-file counts are `382` facade and `375` raw.
- Full known suite passes as of the latest `FMT-GIF-004D` update:
  `Ran 757 tests in 40985ms; Passed: 757, Failed: 0, Errors: 0, Skipped: 0`.

Needed:

- Broader oracle regeneration and snapshot fixtures for new surfaces.
- Format matrix tests by mode, option, metadata, and round-trip behavior.
- Large image tests.
- Randomized/fuzz-ish decoder rejection tests for native parsers.
- Performance regression tests.
- Memory lifetime stress tests.
- CI or repeatable release verification.

## Recommended Priority Queue

These priorities are ordered to build useful parity while keeping tasks bounded.

1. GIF quantization and animation correctness.
   The current GIF layer is useful, but Pillow-like user expectations often hit
   animation transparency optimization, RGBA save, local frame rectangles, and
   disposal behavior.

2. PNG save option and metadata expansion.
   PNG is a primary interchange format; text/ICC/EXIF/transparency/interlace
   support would raise practical compatibility.

3. JPEG option and metadata expansion.
   Add ICC/EXIF/progressive/optimize/subsampling behavior after confirming WIC
   limits and where native marker patching is needed.

4. TIFF multipage and tag surface.
   Useful for serious imaging workflows, but likely larger than ICO/PNG/JPEG
   option increments.

5. Full quantize algorithms.
   Required for serious palette/GIF parity.

6. FreeType and Unicode text.
   This is a large dependency/design decision. It should be treated as a
   dedicated milestone, not a small ImageDraw patch.

7. `I`/`F` mode expansion.
   Add arithmetic, conversion, filters, and file-format behavior only after
   confirming Pillow source semantics for each path.

8. Performance benchmark harness.
   Once more surfaces stabilize, add repeatable native-vs-AHK and native-vs-
   Pillow measurements.

9. Packaging and stdlib-level API polish.
    Add clearer examples, release process, public ABI header, and compatibility
    table.

## Update Rule

Whenever a feature moves from "remaining" to "covered":

1. Add or update AHK tests first.
2. Implement native/facade behavior.
3. Rebuild `build\x64\Release\pillow_c.dll` if native code changed.
4. Run targeted tests, then the full AHK directory suite from the parent
   `visual_studio` directory with `-TimeoutSeconds 120`.
5. Update this document and `docs/native-abi.md` in the same change.
