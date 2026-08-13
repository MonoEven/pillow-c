# MM BigTIFF Tiled Extension Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use
> `superpowers:executing-plans` inline. The user explicitly prohibits
> subagents and all stage/commit/push operations.

**Goal:** Implement one endian-general native BigTIFF tiled route covering the
existing byte, planar, numeric, and CMYK mode/compression matrix for valid MM
files while documenting Pillow 11.3.0's inability to open MM BigTIFF.

**Architecture:** Decode the BigTIFF header once, thread its byte order through
the existing IFD walker/parser/counter, retain `I;16B` file bytes, and normalize
MM `I`/`F` samples into native little-endian storage. Reuse every existing tile
decoder and copy path.

**Tech Stack:** C++17, Win64 DLL, AutoHotkey v2/AhkTest, Pillow 11.3.0 oracle,
MSBuild Release x64.

---

### Task 1: Register matrix REDs

**Files:**

- Modify: `ahk/pillow_c.test.ahk`
- Modify: `ahk/pillow.test.ahk`

- [x] Add `AppendBe64` and endian-selecting fixture serialization helpers.
- [x] Extend the existing BigTIFF fixture builders with
  `littleEndian := true`; for MM numeric fixtures use:

```text
I;16: file bytes = big-endian pairs, expected mode = I;16B, expected bytes = file bytes
I:    file bytes = big-endian int32, expected mode = I, expected bytes = little-endian int32
F:    file bytes = big-endian float32, expected mode = F, expected bytes = little-endian float32
```

- [x] Register one raw and one facade test that batch all existing MM BigTIFF
  chunky, planar, numeric, CMYK, and compression cases.
- [x] Run each target serially and verify the existing DLL fails because the
  MM BigTIFF header is not recognized by the native route.

### Task 2: Generalize native BigTIFF byte order

**Files:**

- Modify: `src/pillow_c_codec_tiff.cpp`

- [x] Add a private header decoder with this contract:

```cpp
bool parse_tiff_bigtiff_header(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool* out_little_endian,
    std::uint64_t* out_first_ifd_offset);
```

- [x] Replace the three II-only header gates in
  `locate_tiff_bigtiff_ifd`, `parse_tiff_bigtiff_tiled_image_for_ifd`, and
  `count_tiff_bigtiff_ifds` with that decoder.
- [x] Select `PILLOW_C_MODE_I16B` for MM unsigned 16-bit grayscale.
- [x] In the chunky row copy, convert each MM `I`/`F` four-byte sample from
  file order to native little-endian order after tile decompression; leave
  all 8-bit samples and `I;16B` unchanged.
- [x] Rebuild Release x64 and rerun raw/facade matrix targets until GREEN.

### Task 3: Regression and evidence

**Files:**

- Modify: `docs/pillow-gap-checkpoint.md`
- Modify: `docs/pillow-gap-analysis.md`
- Modify: `docs/native-abi.md`
- Modify: `docs/testing.md`

- [x] Run raw TIFF and facade TIFF filters serially.
- [x] Run the full AHK directory suite with `-TimeoutSeconds 240`.
- [x] Compare all source `pillow_c_*` exports with Release DLL exports and
  require an empty set difference.
- [x] Record build warnings/errors, test counts/timings, export counts, and
  DLL SHA-256 in all four required documents.
- [x] Record that Pillow 11.3.0 rejects MM BigTIFF and that this extension does
  not increase the compatibility percentage by itself.
