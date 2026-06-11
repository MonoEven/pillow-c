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
8 I
9 F
```

`pillow_c_mode_from_string`, `pillow_c_mode_name`, `pillow_c_image_create_mode`, and `pillow_c_image_mode` keep handles mode-aware. Channel count is storage layout; mode is wrapper-visible Pillow semantics.

The legacy `pillow_c_image_create(width, height, channels, ...)` maps channel count `1`, `2`, `3`, and `4` to `L`, `LA`, `RGB`, and `RGBA`.

Mode `1` uses one unpacked byte per pixel internally for native operations and data-pointer sharing. `pillow_c_image_set_raw_bytes` and `pillow_c_image_get_raw_bytes` expose Pillow's external bit-packed row format for raw mode `1`.

Mode `P` uses one palette index byte per pixel internally. RGB palette metadata lives on the image handle and is exposed through `pillow_c_image_put_palette_rgb` and `pillow_c_image_get_palette_rgb`; optional palette alpha metadata is exposed through `pillow_c_image_put_palette_rgba`, `pillow_c_image_get_palette_rgba`, and `pillow_c_image_palette_alpha_mode`. `pillow_c_image_put_palette_rgb` and `pillow_c_image_put_palette_rgba` also mirror Pillow's `L.putpalette(...)` behavior by converting an `L` handle to mode `P` while keeping its one-byte pixel indexes. Same-mode pixel-copy, point/LUT, reorder, expand, offset, resize, transform, and rotate paths preserve that palette so later conversion still resolves indexes like Pillow.

Mode `CMYK` uses four direct channel bytes per pixel. The current verified CMYK foundation covers mode mapping, raw byte import/export, getdata/putdata facade packing, getpixel/putpixel, copy, `ImageChops.invert`, and non-logical `ImageChops` binary operations.

Mode `I` uses four bytes per pixel as a little-endian signed 32-bit Pillow integer storage slot. The current verified `I` surface is intentionally narrow: mode mapping, byte-size/data export, raw `I` byte import/export, Pillow-compatible unsigned 16-bit raw decode aliases, `I;16B` raw encode, high-bit-depth Netpbm grayscale open/save, and facade scalar `getpixel`/`putpixel`/`getdata`/`putdata` semantics. General `I` arithmetic, conversion, full public `I;16*` modes, and non-Netpbm file paths are future ABI surfaces.

Mode `F` uses four bytes per pixel as a little-endian 32-bit float storage slot. The current verified `F` surface is intentionally narrow: mode mapping, byte-size/data export, raw `F`/`F;32F` byte import/export, and facade scalar `getpixel`/`putpixel`/`getdata`/`putdata` semantics. General `F` arithmetic, conversion, filtering, and file-format paths are future ABI surfaces.

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

Font lifecycle and default-font metrics:

- `pillow_c_font_load_default`
- `pillow_c_font_free`
- `pillow_c_font_getlength`
- `pillow_c_font_getbbox`
- `pillow_c_font_getbbox_anchor`
- `pillow_c_font_getmetrics`
- `pillow_c_font_getname`
- `pillow_c_font_variant`

Image lifecycle and metadata:

- `pillow_c_image_create`
- `pillow_c_image_create_mode`
- `pillow_c_image_free`
- `pillow_c_image_width`
- `pillow_c_image_height`
- `pillow_c_image_mode`
- `pillow_c_image_metadata_resolution`
- `pillow_c_image_metadata_hotspot`
- `pillow_c_image_channels`
- `pillow_c_image_stride`
- `pillow_c_image_size`
- `pillow_c_image_data`
- `pillow_c_image_set_bytes`
- `pillow_c_image_put_palette_rgb`
- `pillow_c_image_put_palette_rgba`
- `pillow_c_image_get_palette_rgb`
- `pillow_c_image_get_palette_rgba`
- `pillow_c_image_palette_alpha_mode`
- `pillow_c_image_remap_palette`
- `pillow_c_image_set_raw_bytes`
- `pillow_c_image_put_data`
- `pillow_c_image_open_bmp`
- `pillow_c_image_save_bmp`
- `pillow_c_image_open_ppm`
- `pillow_c_image_save_ppm`
- `pillow_c_image_open_qoi`
- `pillow_c_image_save_qoi`
- `pillow_c_image_open_tga`
- `pillow_c_image_save_tga`
- `pillow_c_image_save_tga_options`
- `pillow_c_image_open_xbm`
- `pillow_c_image_save_xbm`
- `pillow_c_image_save_xbm_options`
- `pillow_c_image_open_ico`
- `pillow_c_image_save_ico`
- `pillow_c_image_save_ico_options`
- `pillow_c_image_save_ico_format_options`
- `pillow_c_image_open_png`
- `pillow_c_image_save_png`
- `pillow_c_image_save_png_compress_level`
- `pillow_c_image_save_png_options`
- `pillow_c_image_open_jpeg`
- `pillow_c_image_save_jpeg`
- `pillow_c_image_save_jpeg_quality`
- `pillow_c_image_save_jpeg_options`
- `pillow_c_image_open_tiff`
- `pillow_c_image_open_tiff_frame`
- `pillow_c_image_frame_count_tiff`
- `pillow_c_image_save_tiff`
- `pillow_c_image_open_gif`
- `pillow_c_image_open_gif_frame`
- `pillow_c_image_frame_count_gif`
- `pillow_c_image_gif_metadata`
- `pillow_c_image_gif_metadata_ex`
- `pillow_c_image_save_gif`
- `pillow_c_image_save_gif_options`
- `pillow_c_image_save_gif_animation`
- `pillow_c_image_save_gif_animation_options`
- `pillow_c_image_save_gif_animation_metadata_options`
- `pillow_c_image_linear_gradient`
- `pillow_c_image_radial_gradient`
- `pillow_c_image_effect_mandelbrot`
- `pillow_c_image_effect_noise`
- `pillow_c_image_effect_spread`
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
- `pillow_c_image_draw_floodfill`
- `pillow_c_image_draw_line`
- `pillow_c_image_draw_line_joint`
- `pillow_c_image_draw_points`
- `pillow_c_image_draw_polygon`
- `pillow_c_image_draw_text`
- `pillow_c_image_draw_text_anchor`
- `pillow_c_image_draw_text_stroke`
- `pillow_c_image_draw_text_anchor_stroke`
- `pillow_c_image_draw_text_font`
- `pillow_c_image_draw_text_font_stroke`
- `pillow_c_image_draw_text_font_anchor`
- `pillow_c_image_draw_text_font_anchor_stroke`
- `pillow_c_image_draw_multiline_text`
- `pillow_c_image_draw_multiline_text_align`
- `pillow_c_image_draw_multiline_text_anchor`
- `pillow_c_image_draw_multiline_text_align_stroke`
- `pillow_c_image_draw_multiline_text_anchor_stroke`
- `pillow_c_image_draw_multiline_text_font`
- `pillow_c_image_draw_multiline_text_font_align`
- `pillow_c_image_draw_multiline_text_font_align_stroke`
- `pillow_c_image_draw_multiline_text_font_anchor`
- `pillow_c_image_draw_multiline_text_font_anchor_stroke`
- `pillow_c_image_textlength`
- `pillow_c_image_textbbox`
- `pillow_c_image_textbbox_stroke`
- `pillow_c_image_textbbox_anchor`
- `pillow_c_image_textbbox_anchor_stroke`
- `pillow_c_image_textbbox_font_anchor`
- `pillow_c_image_textbbox_font_anchor_stroke`
- `pillow_c_image_multiline_textbbox`
- `pillow_c_image_multiline_textbbox_align`
- `pillow_c_image_multiline_textbbox_align_f64`
- `pillow_c_image_multiline_textbbox_align_stroke`
- `pillow_c_image_multiline_textbbox_align_stroke_f64`
- `pillow_c_image_multiline_textbbox_anchor_f64`
- `pillow_c_image_multiline_textbbox_anchor_stroke_f64`
- `pillow_c_image_multiline_textbbox_font`
- `pillow_c_image_multiline_textbbox_font_align`
- `pillow_c_image_multiline_textbbox_font_align_stroke`
- `pillow_c_image_multiline_textbbox_font_align_f64`
- `pillow_c_image_multiline_textbbox_font_align_stroke_f64`
- `pillow_c_image_multiline_textbbox_font_anchor_f64`
- `pillow_c_image_multiline_textbbox_font_anchor_stroke_f64`
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
- `pillow_c_image_point_lut_mode`
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
- `pillow_c_image_convert_matrix`
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
- `pillow_c_image_filter_color_3d_lut`
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
- `pillow_c_image_point_lut_mode_into`
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
- `pillow_c_image_convert_matrix_into`
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
- `pillow_c_image_filter_color_3d_lut_into`
- `pillow_c_image_transform_affine_into`
- `pillow_c_image_transform_perspective_into`
- `pillow_c_image_transform_quad_into`
- `pillow_c_image_transform_mesh_into`
- `pillow_c_image_rotate_into`
- `pillow_c_image_transpose_into`

`pillow_c_image_equalize` and `pillow_c_image_equalize_into` implement Pillow's `ImageOps.equalize` for `L` and `RGB`; mode `P` sources are converted through their RGB palette first and produce an `RGB` target. `pillow_c_image_equalize_masked` and `pillow_c_image_equalize_masked_into` accept a same-size mode `1` or `L` mask handle after the source handle. A null mask keeps full-image histogram behavior.

`pillow_c_image_point_lut_mode` and `pillow_c_image_point_lut_mode_into` extend the point LUT path with a target mode. The current native target-mode path accepts single-band `1`, `L`, and `P` sources targeting `1`, `L`, or `P`; same-mode calls reuse `pillow_c_image_point_lut` behavior. LUT length remains `source_channels * 256`, `_into` targets must already match the output shape and mode, `P -> P` preserves the RGB palette, and `P -> 1/L` keeps the core palette metadata like Pillow 11.3.0.

`pillow_c_image_put_data` accepts already-packed mode-sized pixel bytes plus a pixel count, writes that prefix into the image in row-major order, and leaves any remaining pixels unchanged. The AHK facade owns Python-like `putdata` value coercion before making this single native call.

`pillow_c_image_draw_rectangle` mutates one image handle in place for the first ImageDraw native primitive. It accepts inclusive integer coordinates, optional caller-packed fill and outline colors, and an outline width. The behavior follows Pillow 11.3.0's `ImageDraw.rectangle`: fill is applied first, outline is skipped when `width <= 0`, `right < left` or `bottom < top` returns `-3`, and drawing is clipped to the image bounds. The AHK facade owns Pillow-style scalar/tuple color packing before making this single native call.

`pillow_c_image_draw_ellipse` mutates one image handle in place for Pillow `ImageDraw.ellipse` calls. It accepts inclusive integer bounding-box coordinates, optional caller-packed fill and outline colors, and an outline width. The implementation follows Pillow 11.3.0's `ellipseNew` integer span generator: fill is applied first with the native full-width ellipse rule, outline is skipped when `width == 0`, reversed coordinates return `-3`, and drawing is clipped to the image bounds.

`pillow_c_image_draw_arc` mutates one image handle in place for Pillow `ImageDraw.arc` calls. It accepts inclusive integer bounding-box coordinates, start/end angles in degrees, a caller-packed stroke color, and a width. The implementation follows Pillow 11.3.0's `ImagingDrawArc` and `arcNew` paths: angles are normalized before drawing, full-circle arcs reuse the native ellipse outline path, equal start/end angles are a no-op, and ordinary arcs use the Pillow clip-ellipse half-plane tree over integer ellipse spans. Reversed coordinates return `-3`, `width <= 0` is a no-op, and drawing is clipped to the image bounds.

`pillow_c_image_draw_chord` mutates one image handle in place for Pillow `ImageDraw.chord` calls. It accepts inclusive integer bounding-box coordinates, start/end angles in degrees, optional caller-packed fill and outline colors, and an outline width. The implementation follows Pillow 11.3.0's `ImagingDrawChord` path: angles are normalized, full-circle chords delegate to the native ellipse path, fill uses the chord clip tree with Pillow's full-width fill rule, and outline draws the chord line plus clipped ellipse boundary. Equal start/end angles and `width == 0` outlines are no-ops; reversed coordinates return `-3`.

`pillow_c_image_draw_pieslice` mutates one image handle in place for Pillow `ImageDraw.pieslice` calls. It accepts inclusive integer bounding-box coordinates, start/end angles in degrees, optional caller-packed fill and outline colors, and an outline width. The implementation follows Pillow 11.3.0's `ImagingDrawPieslice` path: angles are normalized, full-circle pieslices delegate to the native ellipse path, fill uses the pie clip tree, and outline draws both radial sides, the center join ellipse, and the clipped curved boundary. Equal start/end angles and `width == 0` outlines are no-ops; reversed coordinates return `-3`.

`pillow_c_image_draw_rounded_rectangle` mutates one image handle in place for Pillow `ImageDraw.rounded_rectangle` calls. It accepts inclusive integer bounding-box coordinates, a finite non-negative radius, optional caller-packed fill and outline colors, an outline width, and a corners bitmask in top-left, top-right, bottom-right, bottom-left order. The implementation follows Pillow 11.3.0's wrapper composition: radius-zero and no-corner cases delegate to rectangle, fully joined all-corner cases delegate to ellipse, and ordinary rounded rectangles draw native pieslice/arc corner spans plus native rectangle bars in one DLL call. Reversed coordinates and invalid masks return `-3`; drawing is clipped to image bounds.

`pillow_c_image_draw_bitmap` mutates one image handle in place for Pillow `ImageDraw.bitmap` calls. It accepts an inclusive destination origin, a native bitmap/mask image handle, and a caller-packed fill color. The implementation follows Pillow 11.3.0's `ImagingDrawBitmap`/`ImagingFill2` path: mode `1` masks write the fill color where nonzero, mode `L` and `RGBA` masks alpha-blend the fill color, other mask modes return `-3`, color length must match the destination channel count, and drawing is clipped to the destination bounds.

`pillow_c_image_draw_floodfill` mutates one image handle in place for Pillow `ImageDraw.floodfill` calls. It accepts a seed coordinate, caller-packed value color, optional caller-packed border color, and a threshold. The implementation follows Pillow 11.3.0's Python flood-fill semantics while moving the queue walk into C++: seed pixels use Pillow coordinate normalization, out-of-range seeds are no-ops, no-border mode fills pixels whose 1-norm color difference from the seed background is within `thresh`, and border mode fills pixels that are neither the fill value nor the border value.

`pillow_c_image_draw_line` mutates one image handle in place for ordinary Pillow `ImageDraw.line` calls. It accepts a pointer to packed `int x, y` pairs, a point count, a caller-packed color, and a width. The current verified path supports `width <= 1` Bresenham-style segments with the final endpoint draw, plus `width > 1` segment filling through Pillow's wide-line quadrilateral rules. Multi-segment wide lines draw each segment separately like Pillow's C core, clipped to image bounds.

`pillow_c_image_draw_line_joint` extends the line path with a `joint_curve` flag for Pillow `ImageDraw.line(..., joint="curve")`. It first draws the ordinary native wide polyline, then for `width > 4` and non-straight interior vertices adds Pillow-style filled pieslice joints. For `width > 8`, it also adds Pillow's narrow gap-cover line between the calculated tangent points. The implementation follows Pillow 11.3.0's `ImageDraw.line` wrapper angle, flipped-arc, and `coord_at_angle` rules while keeping all intermediate drawing in one DLL call from AHK.

`pillow_c_image_draw_points` mutates one image handle in place for Pillow `ImageDraw.point` calls. It accepts a pointer to packed `int x, y` pairs, a point count, and a caller-packed color. Empty point lists are a no-op, single points are valid, out-of-bounds points are clipped away, and color length must match the image channel count.

`pillow_c_image_draw_polygon` mutates one image handle in place for Pillow `ImageDraw.polygon` calls. It accepts packed `int x, y` vertices, optional caller-packed fill and outline colors, and an outline width. The verified path supports fill and outlines using Pillow's scanline edge rules and closed-outline behavior, clips to the image bounds, and accepts two-point line-like polygons. For `width > 1`, it follows Pillow 11.3.0's wrapper strategy: fill a same-size mode `1` polygon mask, draw `width * 2 - 1` wide outline segments, and apply those segments only where the mask is nonzero so the outline does not expand outside the polygon.

`pillow_c_font_load_default`, `pillow_c_font_free`, `pillow_c_font_getlength`, `pillow_c_font_getbbox`, `pillow_c_font_getbbox_anchor`, `pillow_c_font_getmetrics`, `pillow_c_font_getname`, and `pillow_c_font_variant` establish the initial native `ImageFont` handle ABI. The current font backend embeds Pillow 11.3.0's default font masks and metrics for printable ASCII (`0x20..0x7E`), reports default `FreeTypeFont` metadata as ascent/descent `10/3` and name `Aileron`/`Regular`, clones independent default-font handles through `font_variant`, and rejects non-ASCII text with `-3`. `pillow_c_image_draw_text` keeps the legacy implicit-default-font call, while `pillow_c_image_draw_text_font` accepts an explicit native font handle so the AHK facade can pass `Pillow.ImageFont.LoadDefault()` into draw calls. The single-line anchor exports (`pillow_c_image_draw_text_anchor`, `pillow_c_image_draw_text_font_anchor`, `pillow_c_image_textbbox_anchor`, and `pillow_c_image_textbbox_font_anchor`) support Pillow's default-font anchor pairs with horizontal `l/m/r` and vertical `a/t/m/b/d/s`. The single-line stroke exports add bounded default-font `stroke_width`/`stroke_fill` drawing and bbox expansion. `pillow_c_image_draw_multiline_text`, `pillow_c_image_draw_multiline_text_font`, `pillow_c_image_multiline_textbbox`, and `pillow_c_image_multiline_textbbox_font` keep the legacy left-aligned multiline path. The `_align` variants add an integer alignment id (`0=left`, `1=center`, `2=right`) for Pillow-style default-font multiline text with integer `spacing`, including trailing empty-line bbox behavior and center/right per-line drawing offsets. The legacy aligned bbox exports return integer-expanded boxes, while `_align_f64` exports preserve Pillow's fractional bbox coordinates for center/right alignment. The multiline anchor exports (`pillow_c_image_draw_multiline_text_anchor`, `pillow_c_image_draw_multiline_text_font_anchor`, `pillow_c_image_multiline_textbbox_anchor_f64`, and `pillow_c_image_multiline_textbbox_font_anchor_f64`) mirror Pillow's horizontal-text multiline anchor handling for horizontal `l/m/r` and vertical `a/m/d/s`; vertical `t/b` anchors return invalid argument like Pillow's unsupported multiline-anchor path. The multiline stroke exports add the same bounded default-font stroke path and use Pillow's stroke-aware line spacing (`10 + spacing + 2 * stroke_width`) for drawing and bbox calculations. Full FreeType loading, Unicode glyph coverage, `justify`, direction/features/language, and variation axes/names remain future ABI surfaces.

`pillow_c_image_remap_palette` and `pillow_c_image_remap_palette_into` implement Pillow's `Image.remap_palette` for mode `P` and `L` images over RGB palettes. The native path builds the new palette from `dest_map`, remaps all one-byte pixels in one pass, returns a mode `P` image, and uses Pillow's default grayscale source palette for `L` inputs. RGBA palette remapping is intentionally outside the current RGB palette ABI.

`pillow_c_image_put_palette_rgba` accepts normalized RGBA palette bytes plus an alpha-mode id: `0` means no stored alpha metadata, `1` means Pillow `RGBA` palette semantics, and `2` means Pillow `RGBX` palette semantics. `pillow_c_image_get_palette_rgba` returns RGB plus stored alpha bytes, or `255` alpha when no alpha metadata exists. `pillow_c_image_palette_alpha_mode` lets the AHK facade reproduce Pillow's rawmode restrictions, including `getpalette("RGBX")`/`getpalette("BGRX")` rejection for true `RGBA` palettes.

`pillow_c_image_resize_box` and `pillow_c_image_resize_box_into` expose Pillow-style `Image.resize(..., box=...)` sampling directly against a source region. The current ABI accepts finite positive-area boxes contained inside the source image, supports the same resampling IDs as `pillow_c_image_resize`, preserves same-mode palettes, uses premultiplied color sampling for `LA` and `RGBA`, returns `-3` for invalid boxes, and returns `-5` for `_into` target shape or mode mismatches.

`pillow_c_image_resize_reducing_gap` and `pillow_c_image_resize_reducing_gap_into` expose Pillow-style `Image.resize(..., box=..., reducing_gap=...)` as one native operation. `reducing_gap` must be finite and at least `1.0`; when the computed factor is greater than one for either axis, the DLL computes Pillow's safe reduce box, runs native integer `reduce`, then runs final box resize against the reduced temporary. Modes `1` and `P` force `NEAREST` like Pillow, so palette images avoid reduce and preserve palette metadata. Invalid boxes or gaps return `-3`, and `_into` target shape or mode mismatches return `-5`.

`pillow_c_image_set_raw_bytes` and `pillow_c_image_get_raw_bytes` implement common Pillow raw decoder/encoder modes without AHK-side byte reordering. The current raw decode support covers `1`->`1`, `L`->`L`, `LA`->`LA`, `CMYK`->`CMYK`, `I`/`I;32`/`I;32B`/`I;32N` raw input into mode `I`, `I;16`/`I;16B`/`I;16N` raw input into mode `I`, `F`/`F;32F`/`F;32BF`/`F;32NF` raw input into mode `F`, `RGB` target raw modes `RGB`, `RGBX`, `BGR`, `BGRX`, `XBGR`, and `RGBA` target raw modes `RGBA`, `BGRA`, `ARGB`, `ABGR`, `BGR`. Mode `1` raw bytes are bit-packed most-significant-bit first per row, while native image storage remains one byte per pixel. Mode `I` raw bytes are stored as little-endian 32-bit slots; 16-bit raw decoders expand unsigned samples into those 32-bit slots, and big/native-endian 32-bit aliases normalize into the same internal storage. Mode `F` raw bytes are stored as little-endian float32 slots, with big/native-endian aliases normalized during decode. Decode accepts a non-negative source stride, where `0` means tightly packed, and negative orientation reads rows bottom-up. Raw encode support covers matching direct modes, mode `I` to `I;16B` with Pillow-style `0..65535` clipping, direct `F`/`F;32F` plus native-endian `F;32NF`, and common `RGB`/`RGBA` BGR-family packers; callers can first pass a null output pointer to query the required byte size.

`pillow_c_image_open_bmp` and `pillow_c_image_save_bmp` are the first native file-format entry points. Paths are UTF-8 strings from AHK and are opened through Windows wide-path APIs. The current BMP support is intentionally uncompressed Windows BMP: open accepts 8-bit indexed/grayscale, 24-bit BGR, and 32-bit BGRA; save supports `L`, `RGB`, and `RGBA`. `RGB` save bytes match Pillow's 24-bit BMP output, `L` saves with a grayscale palette, and `RGBA` saves as 32-bit BGRA like Pillow, which opens back as `RGB`.

`pillow_c_image_open_ppm` and `pillow_c_image_save_ppm` keep Netpbm PBM/PGM/PPM files in the DLL. Native open supports plain `P1` and binary `P4` bitmap as mode `1`, plain `P2` and binary `P5` grayscale as mode `L` when `maxval <= 255`, high-bit-depth `P2`/`P5` grayscale as mode `I`, and plain `P3` plus binary `P6` truecolor as mode `RGB` when `maxval < 65536`. Non-255 8-bit samples are scaled to 8-bit with Pillow's half-even rounding. High-bit-depth grayscale samples are scaled to Pillow's `0..65535` `I` range with half-even rounding and stored as little-endian 32-bit values; `maxval=65535` keeps the sample value directly. Binary over-range samples clamp through the scaling path, while plain over-range samples reject like Pillow. PBM bits are inverted relative to Pillow's external mode `1` raw bytes: Netpbm bit `1` is black and Pillow raw bit `1` is white. Native save writes Pillow-style binary `P4`/`P5`/`P6` headers for `1`, `L`, `I`, and `RGB` handles. Mode `I` writes `P5`, `maxval=65535`, and big-endian unsigned 16-bit samples after clipping signed int32 pixels to Pillow's `0..65535` PGM save range.

`pillow_c_image_open_qoi` and `pillow_c_image_save_qoi` keep Quite OK Image encode/decode inside the DLL. The current path supports Pillow-compatible `RGB` and `RGBA` QOI files, writes Pillow's default colorspace byte, and rejects unsupported modes such as `L`, `P`, and `CMYK` with `-3`.

`pillow_c_image_open_tga`, `pillow_c_image_save_tga`, and `pillow_c_image_save_tga_options` keep Truevision TGA decode/encode inside the DLL. The current path supports Pillow-compatible uncompressed and RLE `L`, `RGB`, `RGBA`, and 24-bit color-mapped `P` files: default save writes uncompressed image types `3`, `2`, and `1`; option save with `rle != 0` writes image types `11`, `10`, and `9`; both save paths write TGA 2.0 footer bytes, bottom-left origin rows, BGR/BGRA channel order for color images, and BGR palette entries for `P`. RLE encoding is row-bounded to match Pillow's TGA encoder packet boundaries. Open accepts 8-bit grayscale, 24/32-bit truecolor, and 8-bit indexed files with 24-bit color maps for both uncompressed and RLE files, handles top/bottom plus left/right origin descriptor bits, preserves RGB palette metadata on `P` handles, and rejects truncated RLE packets with `-2`. Non-24-bit palettes and advanced TGA metadata remain future surfaces.

`pillow_c_image_open_xbm`, `pillow_c_image_save_xbm`, and `pillow_c_image_save_xbm_options` keep X11 bitmap files in the DLL for Pillow mode `1` images. Native save writes Pillow-style `im_width`, `im_height`, and `im_bits[]` text with low-bit-first XBM bytes and 15 byte literals per line. The options path writes `im_x_hot` and `im_y_hot` between height and bits for non-negative integer hotspot pairs, matching Pillow's stable round-trippable XBM metadata path. Native open accepts ordinary Pillow XBM headers, decodes low-bit-first file bytes into the DLL's unpacked mode `1` storage where nonzero bits become `255`, stores non-negative integer hotspot pairs on the image handle, and reports truncated bitmap data with `-2`.

`pillow_c_image_open_ico`, `pillow_c_image_save_ico`, `pillow_c_image_save_ico_options`, and `pillow_c_image_save_ico_format_options` keep the current ICO path inside the DLL. Native open decodes through WIC and returns the largest available frame as a public `RGBA` handle. Native save writes PNG-backed ICO entries generated by the native PNG chunk writer, with ICO directory bit depth set to Pillow's PNG-backed default `32`. The default save path mirrors Pillow's built-in square icon sizes `16`, `24`, `32`, `48`, `64`, `128`, and `256`, skipping any size that does not fit inside the source image and using native LANCZOS resize for generated frames. The size options path accepts a pointer to `size_count` integer pairs, sorts and de-duplicates requested pairs like Pillow's `sorted(set(sizes))`, skips pairs larger than the source image or ICO's `256x256` limit, and uses thumbnail-style contained LANCZOS resizing for non-square requested boxes. A zero explicit `size_count` writes an empty ICO directory. `pillow_c_image_save_ico_format_options` adds a `has_sizes` flag so callers can distinguish default sizes from explicit `sizes=[]`, and only exact lowercase `bitmap_format="bmp"` selects Pillow-style DIB-backed ICO payloads. The BMP-backed path supports Pillow's `1`, `L`, `P`, `RGB`, and `RGBA` modes, writes doubled DIB heights, BGRA/BGR pixel rows, palette entries where Pillow writes them, and raw 1-bit zero AND masks for non-32-bit entries; other bitmap format strings fall back to PNG-backed entries like Pillow. The current verified path covers facade `Image.Open(..., "ICO")`, `Image.Save(..., "ICO")`, `Image.Save(..., "ICO", { Sizes: [...] })`, `Image.Save(..., "ICO", { BitmapFormat: "bmp" })`, raw DLL round-tripping for `RGBA`, default multi-entry directories, custom size directories, DIB-backed `RGBA`/`RGB` entries, and uppercase `"BMP"` fallback. ICO `append_images` and frame selection by caller-requested size remain future ABI surfaces.

`pillow_c_image_open_png`, `pillow_c_image_save_png`, `pillow_c_image_save_png_compress_level`, and `pillow_c_image_save_png_options` keep PNG decode/encode inside the DLL. The current PNG path supports `L`, `LA`, `P`, `RGB`, and `RGBA` image handles. Native open converts supported source PNG pixel formats into the DLL's row-major public modes, preserves short RGB palettes for `P`, and reads `pHYs` DPI metadata into the handle. Native save writes valid PNG files from those modes after any required RGB/BGR channel packing inside C++; `LA` and `P` default saves use native PNG chunk writing so they reopen with Pillow-style mode and palette semantics. `pillow_c_image_save_png_compress_level` accepts Pillow-style `-1` default plus `0..9`; level `0` writes native stored zlib output for supported modes, while nonzero levels currently reuse the existing encoder path. `pillow_c_image_save_png_options` is the extensible PNG save-options ABI used by the facade for `compress_level` and `dpi`; positive DPI values are converted to PNG `pHYs` pixels-per-meter values with Pillow's rounding formula, and invalid or partial DPI values return `-3`.

`pillow_c_image_open_jpeg`, `pillow_c_image_save_jpeg`, `pillow_c_image_save_jpeg_quality`, and `pillow_c_image_save_jpeg_options` keep JPEG decode/encode inside the DLL through WIC. The current JPEG path supports lossy `L` and `RGB` image handles. Native open probes the JPEG frame component count before decoding so one-component JPEGs reopen as `L` and three-component JPEGs reopen as public RGB byte order, while the same native segment scan captures EXIF orientation and JFIF DPI/density metadata. Native save accepts `L` and `RGB`, packs RGB to WIC's BGR encoder format inside C++, and rejects alpha, palette, and CMYK modes with `-3`. `pillow_c_image_save_jpeg_quality` accepts an integer quality value, maps `-1` to the encoder default, clamps other values into WIC's `0..100` quality range, and is retained for ABI compatibility. `pillow_c_image_save_jpeg_options` is the extensible JPEG save-options ABI used by the facade for `quality` and `dpi`; DPI values follow Pillow's `round()` behavior, positive rounded pairs write JFIF `units=1` density values, and any non-positive rounded component writes Pillow's default `units=0, density=1x1` metadata.

`pillow_c_image_metadata_resolution` exposes resolution metadata already attached to an image handle. It returns a boolean DPI flag plus double `dpi_x`/`dpi_y`, and returns JFIF version, density unit, and density pair when JPEG APP0 JFIF metadata is present. Non-JPEG images report `jfif=0`; images without usable DPI report `has_dpi=0` without clearing other handle metadata.

`pillow_c_image_metadata_hotspot` exposes XBM hotspot metadata already attached to an image handle. It returns a boolean hotspot flag plus integer `x` and `y` coordinates; images without hotspot metadata report `has_hotspot=0` and zero coordinates.

`pillow_c_image_open_tiff`, `pillow_c_image_open_tiff_frame`, `pillow_c_image_frame_count_tiff`, and `pillow_c_image_save_tiff` keep TIFF decode/encode inside the DLL through WIC. The current TIFF path supports lossless `L`, `RGB`, and `RGBA` image handles. Native open converts supported TIFF pixel formats into the DLL's public row-major byte order. `pillow_c_image_open_tiff` opens frame `0`; `pillow_c_image_open_tiff_frame` opens a zero-based frame index and returns `-3` for negative or out-of-range frames. `pillow_c_image_frame_count_tiff` returns WIC's decoded frame count after validating the container. Native save packs RGB/RGBA to WIC's BGR/BGRA encoder formats inside C++ and rejects palette, LA, mode `1`, and CMYK modes with `-3`.

`pillow_c_image_open_gif`, `pillow_c_image_open_gif_frame`,
`pillow_c_image_frame_count_gif`, `pillow_c_image_gif_metadata`,
`pillow_c_image_gif_metadata_ex`, `pillow_c_image_save_gif`,
`pillow_c_image_save_gif_options`, `pillow_c_image_save_gif_animation`, and
`pillow_c_image_save_gif_animation_options`, and
`pillow_c_image_save_gif_animation_metadata_options` keep GIF decode/encode
and basic animation metadata inside the DLL. Frame `0` opens as mode `P` with
RGB palette metadata preserved. Later frames first try native GIF block parsing
and LZW decode to compose local image rectangles onto the logical RGB canvas
with transparency and disposal handling.

Verified read-side coverage includes disposal `1` preservation, disposal `2`
restoration to the logical-screen background color, disposal `3` restoration to
the pre-frame canvas for local rectangles that the next frame does not
overwrite, and transparent local-rectangle pixels that preserve the existing
canvas while drawing; unsupported parser cases fall back to WIC frame
conversion. `pillow_c_image_open_gif_frame` uses zero-based indexes and returns
`-3` for negative or out-of-range frames. `pillow_c_image_gif_metadata` parses
GIF blocks directly and returns frame duration in milliseconds, NETSCAPE loop
count, Graphic Control Extension disposal method, and logical-screen background
index. `pillow_c_image_gif_metadata_ex` preserves that ABI and adds the Graphic
Control Extension transparent color index as an optional integer output.
Missing optional values return `-1` except disposal, which defaults to `0`.

Native single-frame save writes mode `P` images with an attached RGB palette
directly; mode `L` inputs and `RGB` inputs with no more than 256 unique colors
are first exact-quantized into a native `P` temporary. `RGB` inputs with more
than 256 unique colors use the current bounded weighted median-cut fallback,
preserving the exact path for smaller palettes and targeting approximate
reopened RGB pixels rather than byte-identical Pillow palette order. Mode
`RGBA` inputs are accepted for the verified exact-color path when the effective
palette fits in 256 entries: `alpha == 0` pixels share one transparent palette
index, the first transparent pixel's RGB becomes that palette entry, and all
nonzero alpha values are treated as opaque RGB. When an RGBA single-frame save
exceeds 256 effective colors, the current bounded fallback reserves one
transparent palette slot if any fully transparent pixel exists, quantizes
non-transparent RGB pixels into the remaining 255 or 256 slots with the same
deterministic weighted median-cut style path, maps all `alpha == 0` pixels to
the transparent index, and keeps partial alpha opaque. RGBA saves without fully
transparent pixels write no GIF transparency extension.

`pillow_c_image_save_gif_options` currently adds P-mode single-frame
transparency: nonzero `has_transparency` writes a GIF89a Graphic Control
Extension with packed flag `0x01`, zero delay, and `transparency & 0xff` as
the transparent color index. The no-transparency options path delegates to
`pillow_c_image_save_gif`.

`pillow_c_image_save_gif_animation` writes same-size mode `P` sequences,
accepts optional duration and disposal arrays of length `1` or frame count,
writes an optional NETSCAPE loop extension, and merges visually identical
consecutive frames by accumulating duration. The first frame uses the global
color table from the first image. Later changed frames may use their own RGB
palette as a local color table, so P-frame animations with different per-frame
palettes can preserve each frame's colors. Delta rectangles and
unchanged-pixel transparency decisions compare resolved palette RGB values
rather than raw palette indexes; for the verified optimized paths, changed
sub-rectangles are written as local image descriptors with local color tables
and an unused transparency index for unchanged pixels. The GIF LZW encoder
follows the reader-compatible code-size transition by growing code size after
`next_code > (1 << code_size)`. It rejects non-`P` animation frames, size
mismatches, invalid palettes, invalid disposal values, and unsupported loop
counts with stable status codes. Facade `ImageSequence.Iterator` coverage now
includes Pillow's live seek-state frame references over a complex transparent
local-rectangle GIF fixture. Caller-provided animation transparency is covered
for optimized and `optimize=False` bounded P-mode fixtures, and caller
`background` is covered for the bounded logical-screen background-byte slice.
The first bounded post-`disposal=2` optimized parity gap is now covered: when
caller transparency is known, the native writer can re-diff the following
frame against the restored background instead of forcing full size on the
selected `3x1` fixture. Animation RGBA quantization and full Pillow quantize
algorithm parity remain future ABI surfaces.

`pillow_c_image_save_gif_animation_options` preserves the animation save
argument list and adds two tri-state integers after `disposal_count`:
`include_color_table` and `optimize`, where `-1` means unset/default, `0`
means false, and `1` means true. For the covered P-mode animation fixtures,
`include_color_table=True` forces frame 0 to write a local color table,
`include_color_table=False` keeps frame 0 on the global color table without
suppressing later bbox-frame local tables, `optimize=True` keeps the existing
optimized local-rectangle transparency behavior, and `optimize=False` writes
4-entry global/local color tables with `LzwMin=8` for the bounded 2-color
fixtures while disabling unchanged-pixel transparency substitution when no
caller transparency is supplied.

`pillow_c_image_save_gif_animation_metadata_options` extends the same argument
list with `has_transparency` and `transparency` after the tri-state animation
options. `has_transparency` must be `0` or `1`; when it is `1`,
`transparency` must be in `0..255`. For the covered optimized P-mode animation
fixture, caller transparency replaces the native unused-index transparency on
later local frames, while frame 0 keeps Pillow's no-transparency GCE default.
If the caller index already exists in the later frame palette, the writer pads
one extra zero-color palette entry before computing the local color-table size,
matching the covered Pillow 11.3.0 4-entry local table. With
`optimize=False`, caller transparency is covered for the same bounded fixture:
frame 0 and changed later frames write a transparency GCE, color tables stay at
4 entries for the selected palettes, and `LzwMin=8` is preserved. `background`,
broader disposal interactions, animation RGBA quantization, and full Pillow
quantize parity remain future ABI surfaces.

`pillow_c_image_save_gif_animation_background_options` preserves the same
argument list as the metadata-options export and appends `has_background` plus
`background`. When `has_background == 1`, `background` must be in `0..255` and
is written to the GIF logical-screen descriptor background byte. For the
covered bounded `3x1` fixture, this matches Pillow 11.3.0's logical-screen
background metadata without changing the already-covered optimized local-frame
geometry. Local Pillow 11.3.0 source/probes did not show logical-screen
`background` alone driving next-frame bbox optimization on the covered
fixtures. The first bounded post-`disposal=2` transparency-aware re-diff path
is now covered; remaining work is the next bounded edge case after that path.

`pillow_c_image_linear_gradient` and `pillow_c_image_linear_gradient_into` implement Pillow's fixed-size top-to-bottom `256x256` `Image.linear_gradient` generator for modes `1`, `L`, and `P`. Internal storage writes the row value `0..255` across each row; mode `1` still uses the existing raw encoder when callers request Pillow's bit-packed external bytes. Unsupported modes return `-3`, and `_into` target shape or mode mismatches return `-5`.

`pillow_c_image_radial_gradient` and `pillow_c_image_radial_gradient_into` implement Pillow's fixed-size `256x256` `Image.radial_gradient` generator for modes `1`, `L`, and `P`. Pixel values are generated from the Pillow-compatible center-distance formula and clipped to `0..255`; mode `1` uses the same internal unpacked storage and external bit-packed raw encoder as other native mode `1` paths. Unsupported modes return `-3`, and `_into` target shape or mode mismatches return `-5`.

`pillow_c_image_effect_mandelbrot` implements Pillow's `Image.effect_mandelbrot(size, extent, quality)` generator and returns a mode `L` image. It accepts non-empty or empty dimensions through the same native handle model, rejects reversed extents and `quality < 2` with `-3`, and otherwise uses Pillow's escape-time formula for deterministic byte output.

`pillow_c_image_effect_noise` implements Pillow's `Image.effect_noise(size, sigma)` generator and returns a mode `L` image. It accepts non-empty or empty dimensions through the same native handle model and follows Pillow 11.3.0's C core: C `rand()` drives the Marsaglia polar Gaussian generator, `sigma` is narrowed to `float`, and output bytes use Pillow's `CLIP8(128 + sigma * value)` semantics.

`pillow_c_image_effect_spread` implements Pillow's `Image.effect_spread(distance)` for existing native image handles. It returns a same-mode, same-size image, preserves RGB palette metadata for mode `P`, rejects negative distances with `-3`, and follows Pillow 11.3.0's C core: `distance == 0` copies the source bytes, otherwise C `rand()` chooses the source-neighborhood offset and output pixels are assigned from the original source image.

`pillow_c_image_logical_and`, `pillow_c_image_logical_or`, `pillow_c_image_logical_xor`, and their `_into` variants implement Pillow `ImageChops.logical_*` for mode `1` images only. They return `-3` for other modes, use overlapping output dimensions for mismatched sizes, and allow empty width or height outputs.

The non-logical `ImageChops` binary operations (`difference`, `multiply`, `screen`, `lighter`, `darker`, `soft_light`, `hard_light`, `overlay`, `add`, `subtract`, `add_modulo`, and `subtract_modulo`) use the same overlapping-output rule and operate channel-generically on matching source modes. The current verified modes are `L`, `LA`, `RGB`, `RGBA`, and `CMYK`.

`pillow_c_image_invert` and `pillow_c_image_invert_into` implement `ImageOps.invert` for modes `1`, `L`, and `RGB`. `pillow_c_image_posterize` and `pillow_c_image_solarize`, plus their `_into` variants, follow Pillow's `_lut` boundary for modes `L` and `RGB`; mode `1`, `LA`, and `RGBA` return `-3`.

`pillow_c_image_composite` and `pillow_c_image_composite_into` blend through mask modes `1`, `L`, `LA`, and `RGBA`; `LA` and `RGBA` use their alpha band.

`pillow_c_image_alpha_composite_rgba_in_place` implements Pillow's instance `Image.alpha_composite` geometry for RGBA images. It mutates the destination handle, accepts destination coordinates and a source rectangle, clips visible pixels to the destination, treats source pixels outside the source image as transparent, and rejects negative source coordinates with `-3`.

`pillow_c_image_paste_masked` mutates the target in place, clips the source rectangle to the target bounds, converts the source to the target mode when needed, and blends through a same-size source mask. Mask modes `1`, `L`, `LA`, and `RGBA` are accepted; `LA` and `RGBA` use their alpha band.

`pillow_c_image_paste_color` mutates the target in place by filling a four-coordinate rectangle with a caller-packed target-mode color. The optional mask must match the unclipped rectangle size and may use `1`, `L`, `LA`, or `RGBA`; masked calls clip the destination rectangle and sample the matching mask offset after clipping. The AHK facade owns Pillow-style scalar/tuple color parsing before making this single native call.

`pillow_c_image_autocontrast` and `pillow_c_image_autocontrast_into` accept an optional mode `1` or `L` mask handle after the ignore list arguments, followed by a `preserve_tone` integer flag. A null mask keeps full-image histogram behavior.

`pillow_c_image_convert_mode` and `pillow_c_image_convert_mode_into` cover the verified `1`/`L`/`LA`/`RGB`/`RGBA`/`P`/`CMYK` conversion paths used by the facade, excluding quantizing targets such as `CMYK -> P`. `LA` preserves alpha as the second band; alpha is ignored for `RGBA`/`LA -> CMYK`, matching Pillow. `CMYK -> RGB` uses Pillow's black-channel-scaled RGB conversion before luma or alpha insertion, and `P -> CMYK` converts through the handle palette.

`pillow_c_image_convert_matrix` and `pillow_c_image_convert_matrix_into` implement Pillow `Image.convert(..., matrix=...)` for RGB input to `L` or `RGB`. `L` targets require four double values, `RGB` targets require twelve, values are converted to Pillow-style float math with `+0.5` before byte clipping, and unsupported source or target modes return `-3`. Matrix length mismatches return `-2`, and `_into` target shape or mode mismatches return `-5`.

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

`pillow_c_image_filter_color_3d_lut` and `pillow_c_image_filter_color_3d_lut_into` accept a target mode id, table channel count, three LUT dimensions, a pointer to double table values, and a table value count. The table uses Pillow's flattened order: channels change first, then the first, second, and third dimensions. The native path prepares table values into Pillow-compatible signed 16-bit fixed-point values and applies trilinear interpolation over the source image's first three bands. Source images must have at least three bands; table channels must be 3 or 4; target modes must have at least that many bands; and a 3-channel table preserves the source fourth band for 4-band targets such as `RGBA` and `CMYK`. Invalid mode or size arguments return `-3`, table length mismatches return `-2`, and `_into` target shape or mode mismatches return `-5`.

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
