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
3 RGB
4 RGBA
```

`pillow_c_mode_from_string`, `pillow_c_mode_name`, `pillow_c_image_create_mode`, and `pillow_c_image_mode` keep handles mode-aware. Channel count is storage layout; mode is wrapper-visible Pillow semantics.

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
- `pillow_c_image_fill`
- `pillow_c_image_getpixel`
- `pillow_c_image_putpixel`
- `pillow_c_image_get_bytes`
- `pillow_c_image_histogram`
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
- `pillow_c_image_offset`
- `pillow_c_image_point_lut`
- `pillow_c_image_invert`
- `pillow_c_image_posterize`
- `pillow_c_image_solarize`
- `pillow_c_image_colorize`
- `pillow_c_image_equalize`
- `pillow_c_image_autocontrast`
- `pillow_c_image_get_channel`
- `pillow_c_image_split_bands`
- `pillow_c_image_put_alpha_value`
- `pillow_c_image_put_alpha_image`
- `pillow_c_image_convert_mode`
- `pillow_c_image_merge_bands`
- `pillow_c_image_rgb_to_l`
- `pillow_c_image_alpha_composite_rgba`
- `pillow_c_image_crop`
- `pillow_c_image_expand`
- `pillow_c_image_resize`
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
- `pillow_c_image_transpose`

Reusable target operations:

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
- `pillow_c_image_offset_into`
- `pillow_c_image_point_lut_into`
- `pillow_c_image_invert_into`
- `pillow_c_image_posterize_into`
- `pillow_c_image_solarize_into`
- `pillow_c_image_colorize_into`
- `pillow_c_image_equalize_into`
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

`pillow_c_image_autocontrast` and `pillow_c_image_autocontrast_into` accept an optional L-mode mask handle after the ignore list arguments, followed by a `preserve_tone` integer flag. A null mask keeps full-image histogram behavior.

`pillow_c_image_rotate` and `pillow_c_image_rotate_into` accept angle, resample, expand, optional center, optional translate, and optional fill color arguments. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC` rotate; unsupported resamplers return `-3`.

`pillow_c_image_filter_kernel` and `pillow_c_image_filter_kernel_into` accept kernel width, kernel height, a pointer to double coefficients, coefficient count, scale, and offset. The current implementation supports Pillow's `(3, 3)` and `(5, 5)` kernel sizes for `L`, `RGB`, and `RGBA`. Border pixels are copied unchanged, coefficients are applied with Pillow's vertical kernel flip, and filtered values use half-up rounding and byte clipping. Unsupported kernel sizes return `-3`; coefficient count mismatches return `-2`.

`pillow_c_image_filter_rank` and `pillow_c_image_filter_rank_into` accept filter size and rank. The current implementation supports arbitrary positive odd sizes for `L`, `RGB`, and `RGBA`; rank must satisfy `0 <= rank < size * size`. Edge pixels are computed with Pillow-style clamped coordinates, not copied unchanged. Invalid size or rank returns `-3`.

`pillow_c_image_filter_mode` and `pillow_c_image_filter_mode_into` accept filter size. The current implementation supports `L`, `RGB`, and `RGBA`, applying Pillow's single-band mode filter independently per channel. Size is converted to a radius with integer division by 2, so even sizes behave like the next odd window size. Only in-image coordinates are counted; outside pixels are ignored. A winning value replaces the original pixel only when it appears more than twice, and equal counts keep the smaller pixel value.

`pillow_c_image_filter_box_blur` and `pillow_c_image_filter_box_blur_into` accept `xradius` and `yradius` doubles. The current implementation supports non-negative finite radii for `L`, `RGB`, and `RGBA`, including fractional radii and single-axis blurs. It follows Pillow's separable fixed-point box blur with endpoint edge extension; radius `(0, 0)` returns a byte copy. Invalid radius returns `-3`.

`pillow_c_image_filter_gaussian_blur` and `pillow_c_image_filter_gaussian_blur_into` accept `xradius` and `yradius` doubles. The current implementation supports finite radii for `L`, `RGB`, and `RGBA`, including fractional radii and single-axis blurs. It mirrors Pillow's default three-pass Gaussian approximation by transforming each requested radius into a BoxBlur radius, running all horizontal passes before vertical passes, and returning a byte copy when both effective radii are zero. Non-finite or out-of-range radii return `-3`.

`pillow_c_image_filter_unsharp_mask` and `pillow_c_image_filter_unsharp_mask_into` accept a scalar `radius` double plus integer `percent` and `threshold`. The current implementation supports finite radii for `L`, `RGB`, and `RGBA`. It reuses native GaussianBlur, then applies Pillow's per-channel `abs(original - blurred) > threshold` condition and `original + (original - blurred) * percent / 100` sharpening with byte clipping. Non-finite or out-of-range radii return `-3`.

`pillow_c_image_transform_affine` and `pillow_c_image_transform_affine_into` accept output width, output height, a pointer to six doubles `(a, b, c, d, e, f)`, resample, and optional fill color arguments. The matrix follows Pillow `Image.transform(..., Transform.AFFINE, matrix, ...)` destination-to-source coordinates. The current implementation supports `NEAREST`, `BILINEAR`, and `BICUBIC`; transform-only unsupported resamplers return `-3`.

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
