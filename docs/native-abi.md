# Native ABI

All exported functions currently return an integer status code:

```text
0   success
-1  null pointer
-2  invalid length
-3  invalid argument
-4  allocation failed
-5  mismatch
```

`pillow_c_status_message` maps status codes to stable UTF-8 text for wrapper exceptions.

## Modes

Current mode IDs:

```text
1 L
2 LA
3 RGB
4 RGBA
5 1
6 P
7 CMYK
```

`pillow_c_mode_from_string`, `pillow_c_mode_name`, `pillow_c_image_create_mode`, and `pillow_c_image_mode` keep handles mode-aware. Channel count is storage layout; mode is wrapper-visible Pillow semantics.

The legacy `pillow_c_image_create(width, height, channels, ...)` maps channel count `1`, `2`, `3`, and `4` to `L`, `LA`, `RGB`, and `RGBA`.

Mode `1` uses one unpacked byte per pixel internally for native operations and data-pointer sharing. `pillow_c_image_set_raw_bytes` and `pillow_c_image_get_raw_bytes` expose Pillow's external bit-packed row format for raw mode `1`.

Mode `P` uses one palette index byte per pixel internally. RGB palette metadata lives on the image handle and is exposed through `pillow_c_image_put_palette_rgb` and `pillow_c_image_get_palette_rgb`. Same-mode pixel-copy, point/LUT, reorder, expand, offset, resize, transform, and rotate paths preserve that palette so later conversion still resolves indexes like Pillow.

Mode `CMYK` uses four direct channel bytes per pixel. The current verified CMYK foundation covers mode mapping, raw byte import/export, getdata/putdata facade packing, getpixel/putpixel, copy, `ImageChops.invert`, and non-logical `ImageChops` binary operations.

## Export Groups

Infrastructure:

- `pillow_c_abi_version`
- `pillow_c_status_message`
- `pillow_c_mode_from_string`
- `pillow_c_mode_name`

Buffer primitives:

- `pillow_c_blend_u8`
- `pillow_c_rgb_to_l`
- `pillow_c_alpha_composite_rgba`

Image lifecycle and metadata:

- `pillow_c_image_create`
- `pillow_c_image_create_mode`
- `pillow_c_image_free`
- `pillow_c_image_width`
- `pillow_c_image_height`
- `pillow_c_image_mode`
- `pillow_c_image_channels`
- `pillow_c_image_stride`
- `pillow_c_image_size`
- `pillow_c_image_data`
- `pillow_c_image_set_bytes`
- `pillow_c_image_put_palette_rgb`
- `pillow_c_image_get_palette_rgb`
- `pillow_c_image_remap_palette`
- `pillow_c_image_set_raw_bytes`
- `pillow_c_image_put_data`
- `pillow_c_image_open_bmp`
- `pillow_c_image_save_bmp`
- `pillow_c_image_open_png`
- `pillow_c_image_save_png`
- `pillow_c_image_open_jpeg`
- `pillow_c_image_save_jpeg`
- `pillow_c_image_open_tiff`
- `pillow_c_image_save_tiff`
- `pillow_c_image_open_gif`
- `pillow_c_image_save_gif`
- `pillow_c_image_linear_gradient`
- `pillow_c_image_radial_gradient`
- `pillow_c_image_effect_mandelbrot`
- `pillow_c_image_fill`
- `pillow_c_image_getpixel`
- `pillow_c_image_putpixel`
- `pillow_c_image_draw_rectangle`
- `pillow_c_image_draw_ellipse`
- `pillow_c_image_draw_arc`
- `pillow_c_image_draw_chord`
- `pillow_c_image_draw_pieslice`
- `pillow_c_image_draw_rounded_rectangle`
- `pillow_c_image_draw_bitmap`
- `pillow_c_image_draw_line`
- `pillow_c_image_draw_points`
- `pillow_c_image_draw_polygon`
- `pillow_c_image_get_bytes`
- `pillow_c_image_get_raw_bytes`
- `pillow_c_image_histogram`
- `pillow_c_image_histogram_masked`
- `pillow_c_image_entropy`
- `pillow_c_image_get_extrema`
- `pillow_c_image_getbbox`
- `pillow_c_image_getprojection`
- `pillow_c_image_getcolors`

Image operations:

- `pillow_c_image_copy`
- `pillow_c_image_constant`
- `pillow_c_image_chops_invert`
- `pillow_c_image_blend`
- `pillow_c_image_composite`
- `pillow_c_image_difference`
- `pillow_c_image_multiply`
- `pillow_c_image_screen`
- `pillow_c_image_soft_light`
- `pillow_c_image_hard_light`
- `pillow_c_image_overlay`
- `pillow_c_image_lighter`
- `pillow_c_image_darker`
- `pillow_c_image_add`
- `pillow_c_image_subtract`
- `pillow_c_image_add_modulo`
- `pillow_c_image_subtract_modulo`
- `pillow_c_image_logical_and`
- `pillow_c_image_logical_or`
- `pillow_c_image_logical_xor`
- `pillow_c_image_offset`
- `pillow_c_image_point_lut`
- `pillow_c_image_invert`
- `pillow_c_image_posterize`
- `pillow_c_image_solarize`
- `pillow_c_image_colorize`
- `pillow_c_image_equalize`
- `pillow_c_image_equalize_masked`
- `pillow_c_image_autocontrast`
- `pillow_c_image_get_channel`
- `pillow_c_image_split_bands`
- `pillow_c_image_put_alpha_value`
- `pillow_c_image_put_alpha_image`
- `pillow_c_image_convert_mode`
- `pillow_c_image_merge_bands`
- `pillow_c_image_rgb_to_l`
- `pillow_c_image_alpha_composite_rgba`
- `pillow_c_image_alpha_composite_rgba_in_place`
- `pillow_c_image_crop`
- `pillow_c_image_expand`
- `pillow_c_image_resize`
- `pillow_c_image_resize_box`
- `pillow_c_image_resize_reducing_gap`
- `pillow_c_image_reduce`
- `pillow_c_image_filter_kernel`
- `pillow_c_image_filter_rank`
- `pillow_c_image_filter_mode`
- `pillow_c_image_filter_box_blur`
- `pillow_c_image_filter_gaussian_blur`
- `pillow_c_image_filter_unsharp_mask`
- `pillow_c_image_transform_affine`
- `pillow_c_image_transform_perspective`
- `pillow_c_image_transform_quad`
- `pillow_c_image_transform_mesh`
- `pillow_c_image_rotate`
- `pillow_c_image_contain`
- `pillow_c_image_cover`
- `pillow_c_image_fit`
- `pillow_c_image_pad`
- `pillow_c_image_paste`
- `pillow_c_image_paste_masked`
- `pillow_c_image_paste_color`
- `pillow_c_image_transpose`

Reusable target operations:

- `pillow_c_image_linear_gradient_into`
- `pillow_c_image_radial_gradient_into`
- `pillow_c_image_remap_palette_into`
- `pillow_c_image_copy_into`
- `pillow_c_image_constant_into`
- `pillow_c_image_chops_invert_into`
- `pillow_c_image_blend_into`
- `pillow_c_image_composite_into`
- `pillow_c_image_difference_into`
- `pillow_c_image_multiply_into`
- `pillow_c_image_screen_into`
- `pillow_c_image_soft_light_into`
- `pillow_c_image_hard_light_into`
- `pillow_c_image_overlay_into`
- `pillow_c_image_lighter_into`
- `pillow_c_image_darker_into`
- `pillow_c_image_add_into`
- `pillow_c_image_subtract_into`
- `pillow_c_image_add_modulo_into`
- `pillow_c_image_subtract_modulo_into`
- `pillow_c_image_logical_and_into`
- `pillow_c_image_logical_or_into`
- `pillow_c_image_logical_xor_into`
- `pillow_c_image_offset_into`
- `pillow_c_image_point_lut_into`
- `pillow_c_image_invert_into`
- `pillow_c_image_posterize_into`
- `pillow_c_image_solarize_into`
- `pillow_c_image_colorize_into`
- `pillow_c_image_equalize_into`
- `pillow_c_image_equalize_masked_into`
- `pillow_c_image_autocontrast_into`
- `pillow_c_image_get_channel_into`
- `pillow_c_image_put_alpha_value_into`
- `pillow_c_image_put_alpha_image_into`
- `pillow_c_image_convert_mode_into`
- `pillow_c_image_merge_bands_into`
- `pillow_c_image_rgb_to_l_into`
- `pillow_c_image_alpha_composite_rgba_into`
- `pillow_c_image_crop_into`
- `pillow_c_image_expand_into`
- `pillow_c_image_resize_into`
- `pillow_c_image_resize_box_into`
- `pillow_c_image_resize_reducing_gap_into`
- `pillow_c_image_reduce_into`
- `pillow_c_image_filter_kernel_into`
- `pillow_c_image_filter_rank_into`
- `pillow_c_image_filter_mode_into`
- `pillow_c_image_filter_box_blur_into`
- `pillow_c_image_filter_gaussian_blur_into`
- `pillow_c_image_filter_unsharp_mask_into`
- `pillow_c_image_transform_affine_into`
- `pillow_c_image_transform_perspective_into`
- `pillow_c_image_transform_quad_into`
- `pillow_c_image_transform_mesh_into`
- `pillow_c_image_rotate_into`
- `pillow_c_image_transpose_into`

`pillow_c_image_equalize` and `pillow_c_image_equalize_into` implement Pillow's `ImageOps.equalize` for `L` and `RGB`; mode `P` sources are converted through their RGB palette first and produce an `RGB` target. `pillow_c_image_equalize_masked` and `pillow_c_image_equalize_masked_into` accept a same-size mode `1` or `L` mask handle after the source handle. A null mask keeps full-image histogram behavior.

`pillow_c_image_put_data` accepts already-packed mode-sized pixel bytes plus a pixel count, writes that prefix into the image in row-major order, and leaves any remaining pixels unchanged. The AHK facade owns Python-like `putdata` value coercion before making this single native call.

`pillow_c_image_draw_rectangle` mutates one image handle in place for the first ImageDraw native primitive. It accepts inclusive integer coordinates, optional caller-packed fill and outline colors, and an outline width. The behavior follows Pillow 11.3.0's `ImageDraw.rectangle`: fill is applied first, outline is skipped when `width <= 0`, `right < left` or `bottom < top` returns `-3`, and drawing is clipped to the image bounds. The AHK facade owns Pillow-style scalar/tuple color packing before making this single native call.

`pillow_c_image_draw_ellipse` mutates one image handle in place for Pillow `ImageDraw.ellipse` calls. It accepts inclusive integer bounding-box coordinates, optional caller-packed fill and outline colors, and an outline width. The implementation follows Pillow 11.3.0's `ellipseNew` integer span generator: fill is applied first with the native full-width ellipse rule, outline is skipped when `width == 0`, reversed coordinates return `-3`, and drawing is clipped to the image bounds.

`pillow_c_image_draw_arc` mutates one image handle in place for Pillow `ImageDraw.arc` calls. It accepts inclusive integer bounding-box coordinates, start/end angles in degrees, a caller-packed stroke color, and a width. The implementation follows Pillow 11.3.0's `ImagingDrawArc` and `arcNew` paths: angles are normalized before drawing, full-circle arcs reuse the native ellipse outline path, equal start/end angles are a no-op, and ordinary arcs use the Pillow clip-ellipse half-plane tree over integer ellipse spans. Reversed coordinates return `-3`, `width <= 0` is a no-op, and drawing is clipped to the image bounds.

`pillow_c_image_draw_chord` mutates one image handle in place for Pillow `ImageDraw.chord` calls. It accepts inclusive integer bounding-box coordinates, start/end angles in degrees, optional caller-packed fill and outline colors, and an outline width. The implementation follows Pillow 11.3.0's `ImagingDrawChord` path: angles are normalized, full-circle chords delegate to the native ellipse path, fill uses the chord clip tree with Pillow's full-width fill rule, and outline draws the chord line plus clipped ellipse boundary. Equal start/end angles and `width == 0` outlines are no-ops; reversed coordinates return `-3`.

`pillow_c_image_draw_pieslice` mutates one image handle in place for Pillow `ImageDraw.pieslice` calls. It accepts inclusive integer bounding-box coordinates, start/end angles in degrees, optional caller-packed fill and outline colors, and an outline width. The implementation follows Pillow 11.3.0's `ImagingDrawPieslice` path: angles are normalized, full-circle pieslices delegate to the native ellipse path, fill uses the pie clip tree, and outline draws both radial sides, the center join ellipse, and the clipped curved boundary. Equal start/end angles and `width == 0` outlines are no-ops; reversed coordinates return `-3`.

`pillow_c_image_draw_rounded_rectangle` mutates one image handle in place for Pillow `ImageDraw.rounded_rectangle` calls. It accepts inclusive integer bounding-box coordinates, a finite non-negative radius, optional caller-packed fill and outline colors, an outline width, and a corners bitmask in top-left, top-right, bottom-right, bottom-left order. The implementation follows Pillow 11.3.0's wrapper composition: radius-zero and no-corner cases delegate to rectangle, fully joined all-corner cases delegate to ellipse, and ordinary rounded rectangles draw native pieslice/arc corner spans plus native rectangle bars in one DLL call. Reversed coordinates and invalid masks return `-3`; drawing is clipped to image bounds.

`pillow_c_image_draw_bitmap` mutates one image handle in place for Pillow `ImageDraw.bitmap` calls. It accepts an inclusive destination origin, a native bitmap/mask image handle, and a caller-packed fill color. The implementation follows Pillow 11.3.0's `ImagingDrawBitmap`/`ImagingFill2` path: mode `1` masks write the fill color where nonzero, mode `L` and `RGBA` masks alpha-blend the fill color, other mask modes return `-3`, color length must match the destination channel count, and drawing is clipped to the destination bounds.

`pillow_c_image_draw_line` mutates one image handle in place for ordinary Pillow `ImageDraw.line` calls. It accepts a pointer to packed `int x, y` pairs, a point count, a caller-packed color, and a width. The current verified path supports `width <= 1` Bresenham-style segments with the final endpoint draw, plus `width > 1` segment filling through Pillow's wide-line quadrilateral rules. Multi-segment wide lines draw each segment separately like Pillow's C core, clipped to image bounds. Curved `joint="curve"` handling is intentionally a wrapper/native future surface.

`pillow_c_image_draw_points` mutates one image handle in place for Pillow `ImageDraw.point` calls. It accepts a pointer to packed `int x, y` pairs, a point count, and a caller-packed color. Empty point lists are a no-op, single points are valid, out-of-bounds points are clipped away, and color length must match the image channel count.

`pillow_c_image_draw_polygon` mutates one image handle in place for Pillow `ImageDraw.polygon` calls. It accepts packed `int x, y` vertices, optional caller-packed fill and outline colors, and an outline width. The current verified path supports fill and `width <= 1` outlines using Pillow's scanline edge rules and line closing behavior, clips to the image bounds, accepts two-point line-like polygons, and returns `-3` for fewer than two points or wide outlines. Pillow's mask-assisted wide outline path is intentionally a future native surface.

`pillow_c_image_remap_palette` and `pillow_c_image_remap_palette_into` implement Pillow's `Image.remap_palette` for mode `P` and `L` images over RGB palettes. The native path builds the new palette from `dest_map`, remaps all one-byte pixels in one pass, returns a mode `P` image, and uses Pillow's default grayscale source palette for `L` inputs. RGBA palette remapping is intentionally outside the current RGB palette ABI.

`pillow_c_image_resize_box` and `pillow_c_image_resize_box_into` expose Pillow-style `Image.resize(..., box=...)` sampling directly against a source region. The current ABI accepts finite positive-area boxes contained inside the source image, supports the same resampling IDs as `pillow_c_image_resize`, preserves same-mode palettes, uses premultiplied color sampling for `LA` and `RGBA`, returns `-3` for invalid boxes, and returns `-5` for `_into` target shape or mode mismatches.

`pillow_c_image_resize_reducing_gap` and `pillow_c_image_resize_reducing_gap_into` expose Pillow-style `Image.resize(..., box=..., reducing_gap=...)` as one native operation. `reducing_gap` must be finite and at least `1.0`; when the computed factor is greater than one for either axis, the DLL computes Pillow's safe reduce box, runs native integer `reduce`, then runs final box resize against the reduced temporary. Modes `1` and `P` force `NEAREST` like Pillow, so palette images avoid reduce and preserve palette metadata. Invalid boxes or gaps return `-3`, and `_into` target shape or mode mismatches return `-5`.

`pillow_c_image_set_raw_bytes` and `pillow_c_image_get_raw_bytes` implement common Pillow raw decoder/encoder modes without AHK-side byte reordering. The current raw decode support covers `1`->`1`, `L`->`L`, `LA`->`LA`, `CMYK`->`CMYK`, `RGB` target raw modes `RGB`, `RGBX`, `BGR`, `BGRX`, `XBGR`, and `RGBA` target raw modes `RGBA`, `BGRA`, `ARGB`, `ABGR`, `BGR`. Mode `1` raw bytes are bit-packed most-significant-bit first per row, while native image storage remains one byte per pixel. Decode accepts a non-negative source stride, where `0` means tightly packed, and negative orientation reads rows bottom-up. Raw encode support covers matching direct modes plus common `RGB`/`RGBA` BGR-family packers; callers can first pass a null output pointer to query the required byte size.

`pillow_c_image_open_bmp` and `pillow_c_image_save_bmp` are the first native file-format entry points. Paths are UTF-8 strings from AHK and are opened through Windows wide-path APIs. The current BMP support is intentionally uncompressed Windows BMP: open accepts 8-bit indexed/grayscale, 24-bit BGR, and 32-bit BGRA; save supports `L`, `RGB`, and `RGBA`. `RGB` save bytes match Pillow's 24-bit BMP output, `L` saves with a grayscale palette, and `RGBA` saves as 32-bit BGRA like Pillow, which opens back as `RGB`.

`pillow_c_image_open_png` and `pillow_c_image_save_png` keep PNG decode/encode inside the DLL. The current PNG path supports `L`, `LA`, `P`, `RGB`, and `RGBA` image handles. Native open converts supported source PNG pixel formats into the DLL's row-major public modes and preserves short RGB palettes for `P`. Native save writes valid PNG files from those modes after any required RGB/BGR channel packing inside C++; `LA` and `P` saves use native PNG chunk writing so they reopen with Pillow-style mode and palette semantics.

`pillow_c_image_open_jpeg` and `pillow_c_image_save_jpeg` keep JPEG decode/encode inside the DLL through WIC. The current JPEG path supports lossy `L` and `RGB` image handles. Native open probes the JPEG frame component count before decoding so one-component JPEGs reopen as `L` and three-component JPEGs reopen as public RGB byte order. Native save accepts `L` and `RGB`, packs RGB to WIC's BGR encoder format inside C++, and rejects alpha, palette, and CMYK modes with `-3`.

`pillow_c_image_open_tiff` and `pillow_c_image_save_tiff` keep TIFF decode/encode inside the DLL through WIC. The current TIFF path supports lossless `L`, `RGB`, and `RGBA` image handles. Native open converts supported TIFF pixel formats into the DLL's public row-major byte order. Native save packs RGB/RGBA to WIC's BGR/BGRA encoder formats inside C++ and rejects palette, LA, mode `1`, and CMYK modes with `-3`.

`pillow_c_image_open_gif` and `pillow_c_image_save_gif` keep single-frame GIF decode/encode inside the DLL through WIC. The current GIF path supports mode `P` image handles only, preserving RGB palette metadata on open and requiring an attached RGB palette on save. Native save rejects non-`P` modes with `-3`; RGB/RGBA quantization and multi-frame ImageSequence behavior are intentionally separate future ABI surfaces.

`pillow_c_image_linear_gradient` and `pillow_c_image_linear_gradient_into` implement Pillow's fixed-size top-to-bottom `256x256` `Image.linear_gradient` generator for modes `1`, `L`, and `P`. Internal storage writes the row value `0..255` across each row; mode `1` still uses the existing raw encoder when callers request Pillow's bit-packed external bytes. Unsupported modes return `-3`, and `_into` target shape or mode mismatches return `-5`.

`pillow_c_image_radial_gradient` and `pillow_c_image_radial_gradient_into` implement Pillow's fixed-size `256x256` `Image.radial_gradient` generator for modes `1`, `L`, and `P`. Pixel values are generated from the Pillow-compatible center-distance formula and clipped to `0..255`; mode `1` uses the same internal unpacked storage and external bit-packed raw encoder as other native mode `1` paths. Unsupported modes return `-3`, and `_into` target shape or mode mismatches return `-5`.

`pillow_c_image_effect_mandelbrot` implements Pillow's `Image.effect_mandelbrot(size, extent, quality)` generator and returns a mode `L` image. It accepts non-empty or empty dimensions through the same native handle model, rejects reversed extents and `quality < 2` with `-3`, and otherwise uses Pillow's escape-time formula for deterministic byte output.

`pillow_c_image_logical_and`, `pillow_c_image_logical_or`, `pillow_c_image_logical_xor`, and their `_into` variants implement Pillow `ImageChops.logical_*` for mode `1` images only. They return `-3` for other modes, use overlapping output dimensions for mismatched sizes, and allow empty width or height outputs.

The non-logical `ImageChops` binary operations (`difference`, `multiply`, `screen`, `lighter`, `darker`, `soft_light`, `hard_light`, `overlay`, `add`, `subtract`, `add_modulo`, and `subtract_modulo`) use the same overlapping-output rule and operate channel-generically on matching source modes. The current verified modes are `L`, `LA`, `RGB`, `RGBA`, and `CMYK`.

`pillow_c_image_invert` and `pillow_c_image_invert_into` implement `ImageOps.invert` for modes `1`, `L`, and `RGB`. `pillow_c_image_posterize` and `pillow_c_image_solarize`, plus their `_into` variants, follow Pillow's `_lut` boundary for modes `L` and `RGB`; mode `1`, `LA`, and `RGBA` return `-3`.

`pillow_c_image_composite` and `pillow_c_image_composite_into` blend through mask modes `1`, `L`, `LA`, and `RGBA`; `LA` and `RGBA` use their alpha band.

`pillow_c_image_alpha_composite_rgba_in_place` implements Pillow's instance `Image.alpha_composite` geometry for RGBA images. It mutates the destination handle, accepts destination coordinates and a source rectangle, clips visible pixels to the destination, treats source pixels outside the source image as transparent, and rejects negative source coordinates with `-3`.

`pillow_c_image_paste_masked` mutates the target in place, clips the source rectangle to the target bounds, converts the source to the target mode when needed, and blends through a same-size source mask. Mask modes `1`, `L`, `LA`, and `RGBA` are accepted; `LA` and `RGBA` use their alpha band.

`pillow_c_image_paste_color` mutates the target in place by filling a four-coordinate rectangle with a caller-packed target-mode color. The optional mask must match the unclipped rectangle size and may use `1`, `L`, `LA`, or `RGBA`; masked calls clip the destination rectangle and sample the matching mask offset after clipping. The AHK facade owns Pillow-style scalar/tuple color parsing before making this single native call.

`pillow_c_image_autocontrast` and `pillow_c_image_autocontrast_into` accept an optional mode `1` or `L` mask handle after the ignore list arguments, followed by a `preserve_tone` integer flag. A null mask keeps full-image histogram behavior.

`pillow_c_image_convert_mode` and `pillow_c_image_convert_mode_into` cover the verified `1`/`L`/`LA`/`RGB`/`RGBA`/`P`/`CMYK` conversion paths used by the facade, excluding quantizing targets such as `CMYK -> P`. `LA` preserves alpha as the second band; alpha is ignored for `RGBA`/`LA -> CMYK`, matching Pillow. `CMYK -> RGB` uses Pillow's black-channel-scaled RGB conversion before luma or alpha insertion, and `P -> CMYK` converts through the handle palette.

`pillow_c_image_put_alpha_value` and `pillow_c_image_put_alpha_image` return `LA` for `L`/`LA` sources and `RGBA` for `RGB`/`RGBA` sources. The matching `_into` variants require the caller to provide that target mode and shape.

`pillow_c_image_histogram` follows Pillow's 256-bin-per-band layout. `pillow_c_image_histogram_masked` accepts a same-size mode `1` or `L` mask; any nonzero native mask byte includes that pixel once, and zero excludes it. A null mask falls back to the unmasked histogram path. For `LA`, Pillow 11.3.0 reports two bands where the second histogram repeats the luminance bins rather than the alpha bins; both native histogram exports mirror that behavior so `ImageStat.Stat` matches Pillow.

`pillow_c_image_rotate` and `pillow_c_image_rotate_into` accept angle, resample, expand, optional center, optional translate, and optional fill color arguments. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC` rotate, including CMYK fill-color packing, `LA`/`RGBA` premultiplied color sampling, and fill sampling; unsupported resamplers return `-3`.

`pillow_c_image_filter_kernel` and `pillow_c_image_filter_kernel_into` accept kernel width, kernel height, a pointer to double coefficients, coefficient count, scale, and offset. The current implementation supports Pillow's `(3, 3)` and `(5, 5)` kernel sizes for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. Border pixels are copied unchanged, coefficients are applied with Pillow's vertical kernel flip, and filtered values use half-up rounding and byte clipping. Unsupported kernel sizes return `-3`; coefficient count mismatches return `-2`.

`pillow_c_image_filter_rank` and `pillow_c_image_filter_rank_into` accept filter size and rank. The current implementation supports arbitrary positive odd sizes for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`; rank must satisfy `0 <= rank < size * size`. Edge pixels are computed with Pillow-style clamped coordinates, not copied unchanged. Invalid size or rank returns `-3`.

`pillow_c_image_filter_mode` and `pillow_c_image_filter_mode_into` accept filter size. The current implementation supports `L`, `LA`, `RGB`, `RGBA`, and `CMYK`, applying Pillow's single-band mode filter independently per channel. Size is converted to a radius with integer division by 2, so even sizes behave like the next odd window size. Only in-image coordinates are counted; outside pixels are ignored. A winning value replaces the original pixel only when it appears more than twice, and equal counts keep the smaller pixel value.

`pillow_c_image_filter_box_blur` and `pillow_c_image_filter_box_blur_into` accept `xradius` and `yradius` doubles. The current implementation supports non-negative finite radii for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`, including fractional radii and single-axis blurs. It follows Pillow's separable fixed-point box blur with endpoint edge extension; radius `(0, 0)` returns a byte copy. Invalid radius returns `-3`.

`pillow_c_image_reduce` and `pillow_c_image_reduce_into` accept integer `xscale`, `yscale`, and a source box as `left`, `top`, `right`, `bottom`. Scales must be greater than zero, boxes must be non-empty and inside the source image, and `_into` targets must match the ceiling-divided output size and source mode. `L`, `RGB`, and `CMYK` reduce by Pillow's fixed-point block average. `LA` and `RGBA` reduce color data in premultiplied-alpha space and convert back to the public mode.

`pillow_c_image_filter_gaussian_blur` and `pillow_c_image_filter_gaussian_blur_into` accept `xradius` and `yradius` doubles. The current implementation supports finite radii for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`, including fractional radii and single-axis blurs. It mirrors Pillow's default three-pass Gaussian approximation by transforming each requested radius into a BoxBlur radius, running all horizontal passes before vertical passes, and returning a byte copy when both effective radii are zero. Non-finite or out-of-range radii return `-3`.

`pillow_c_image_filter_unsharp_mask` and `pillow_c_image_filter_unsharp_mask_into` accept a scalar `radius` double plus integer `percent` and `threshold`. The current implementation supports finite radii for `L`, `LA`, `RGB`, `RGBA`, and `CMYK`. It reuses native GaussianBlur, then applies Pillow's per-channel `abs(original - blurred) > threshold` condition and `original + (original - blurred) * percent / 100` sharpening with byte clipping. Non-finite or out-of-range radii return `-3`.

`pillow_c_image_transform_affine` and `pillow_c_image_transform_affine_into` accept output width, output height, a pointer to six doubles `(a, b, c, d, e, f)`, resample, and optional fill color arguments. The matrix follows Pillow `Image.transform(..., Transform.AFFINE, matrix, ...)` destination-to-source coordinates. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC`, with CMYK covered by the same channel-generic path and `LA`/`RGBA` filtered transforms using premultiplied color sampling; transform-only unsupported resamplers return `-3`.

`pillow_c_image_transform_perspective` and `pillow_c_image_transform_perspective_into` accept output width, output height, a pointer to eight doubles `(a, b, c, d, e, f, g, h)`, resample, and optional fill color arguments. The coefficients follow Pillow `Image.transform(..., Transform.PERSPECTIVE, coefficients, ...)` destination-to-source coordinates. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC`; transform-only unsupported resamplers return `-3`.

`pillow_c_image_transform_quad` and `pillow_c_image_transform_quad_into` accept output width, output height, a pointer to eight doubles `(nw_x, nw_y, sw_x, sw_y, se_x, se_y, ne_x, ne_y)`, resample, and optional fill color arguments. The corners follow Pillow `Image.transform(..., Transform.QUAD, corners, ...)` order. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC`; transform-only unsupported resamplers return `-3`.

`pillow_c_image_transform_mesh` and `pillow_c_image_transform_mesh_into` accept output width, output height, a pointer to `mesh_count * 4` integer boxes, a pointer to `mesh_count * 8` double QUAD corner values, `mesh_count`, resample, and optional fill color arguments. MESH patches are applied in input order and later patches overwrite earlier overlap, matching Pillow `Image.transform(..., Transform.MESH, mesh, ...)`. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC`; transform-only unsupported resamplers return `-3`.

## Resize Resampling IDs

Current resize support:

```text
0 NEAREST
1 LANCZOS
2 BILINEAR
3 BICUBIC
4 BOX
5 HAMMING
```

## Transpose Method IDs

These match Pillow 11.3.0 `Image.Transpose` values:

```text
0 FLIP_LEFT_RIGHT
1 FLIP_TOP_BOTTOM
2 ROTATE_90
3 ROTATE_180
4 ROTATE_270
5 TRANSPOSE
6 TRANSVERSE
```

## Pointer Lifetime

`pillow_c_image_data` returns a pointer into handle-owned storage. The pointer is valid only while the image handle remains alive and the underlying storage is not reallocated.
