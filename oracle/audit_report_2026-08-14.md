# Independent Pillow 11.3.0 Surface Re-Audit (2026-08-14)

Prompt: verify whether the recorded 100% is real or only a facade shell.
Method: enumerate the real public surface of the locally installed
Pillow 11.3.0 (`F:\Python\Python310\python.exe`) and diff it against
`ahk/pillow.ahk` and the 463 `pillow_c_*` exports.

Evidence artifacts:
- `oracle/audit_pillow_surface.py` — surface enumerator
- `oracle/pillow_surface.json` — full name inventory (PIL, Image.Image,
  ImagingCore, 23 submodules, SAVE/OPEN format registries)

## Pillow's real surface (measured)

| Surface | Count |
|---|---|
| `Image.Image` non-private names | 59 |
| `ImagingCore` (`im`) names | 69 |
| Submodules | 23 |
| SAVE formats | 30 |
| OPEN formats | 45 |

## Verdict

NOT true 100% behavioral parity. The recorded 100% was "100% of the
ledger's own bounded coverage definition". The re-audit found whole
families that are neither implemented nor recorded in the BNDRY-001
boundary ledger, plus formats missing from the dependency-gated list.

### Genuinely covered (verified byte-exact in this session)
- `Image.Image`: 58/59 names (only the `readonly` property name absent;
  behavior partially covered through DetachBufferView).
- Mainstream codecs: BMP/PNG/JPEG/TIFF/GIF/PPM/QOI/TGA/XBM/ICO/CUR.
- Numeric mode families: I/F/I;16 point/transform/resize/rotate/stats/
  conversion/fill with Pillow-exact cross-checks (`FAILURES: 0`).
- ImageChops (full 23-name set), ImageEnhance (all 5 classes),
  ImageStat, ImageSequence, ImageFilter mainstream classes (incl.
  Min/Median/MaxFilter), ImageDraw core, ImageCms breadth
  (CreateProfile/GetProfileName/GetProfileDescription/
  IsIntentSupported/BuildTransform/BuildProofTransform/ApplyTransform),
  ImageFont FreeTypeFont + load_default.

### Missing and NOT recorded in the boundary ledger (new findings)
| Gap | Pillow names | Status |
|---|---|---|
| ImageMath (eval/unsafe_eval) | 18 | absent, unrecorded |
| ImageGrab (grab/grabclipboard) | 11 | absent, unrecorded |
| ImagePath | 3 | absent, unrecorded |
| ImageQt module | 26 | absent; only toqimage boundary recorded |
| ImageTk module | 10 | absent; only toqpixmap boundary recorded |
| ImageFile module surface (Parser/ImageFile/StubImageFile/_save) | 31 | absent as module, unrecorded |
| ImagePalette module (wedge/sepia/random/raw/negative/make_*_lut) | 18 | facade `Palette` is only WEB/ADAPTIVE constants |
| ImageTransform class objects (AffineTransform etc.) | 10 | constants+methods only |
| ImageFont variation surface (TransposedFont/Axis/Layout) | — | unrecorded |
| `Image.readonly` property name | 1 | unrecorded |
| SAVE formats not implemented AND not in BNDRY-001 | BLP, BUFR, DIB, GRIB, HDF5, IM, MSP, PALM, SPIDER, WMF (10) | unrecorded |
| OPEN formats not implemented AND not in BNDRY-001 | BLP, BUFR, DIB, FITS, FPX, FTEX, GBR, GRIB, HDF5, IMT, IPTC, MCIDAS, MIC, MPEG, MSP, PCD, PIXAR, SPIDER, WMF, XVTHUMB (20) | unrecorded |

## Honest estimate

~85% ±5% of Pillow's full public surface (judgment-based, not a
precision metric): the core codec/geometry/numeric/statistics behavior
is real byte-level parity; the gap is UI/platform/niche-format modules
plus the unrecorded format list.

## Remediation decision (user-directed)

Demote the checkpoint estimate to `85% ±5%`, mark the new gap rows
`not started`, select `API-MATH-001` (ImageMath eval/unsafe_eval) as
the next bounded packet, and re-arm the goal toward an honest 100%.
