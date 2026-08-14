#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

constexpr int PILLOW_C_OK = 0;
constexpr int PILLOW_C_NULL_POINTER = -1;
constexpr int PILLOW_C_INVALID_LENGTH = -2;
constexpr int PILLOW_C_INVALID_ARGUMENT = -3;
constexpr int PILLOW_C_ALLOCATION_FAILED = -4;
constexpr int PILLOW_C_MISMATCH = -5;

constexpr int PILLOW_C_MODE_L = 1;
constexpr int PILLOW_C_MODE_LA = 2;
constexpr int PILLOW_C_MODE_RGB = 3;
constexpr int PILLOW_C_MODE_RGBA = 4;
constexpr int PILLOW_C_MODE_1 = 5;
constexpr int PILLOW_C_MODE_P = 6;
constexpr int PILLOW_C_MODE_CMYK = 7;
constexpr int PILLOW_C_MODE_I = 8;
constexpr int PILLOW_C_MODE_F = 9;
constexpr int PILLOW_C_MODE_RGBX = 10;
constexpr int PILLOW_C_MODE_I16 = 11;
constexpr int PILLOW_C_MODE_I16B = 12;
constexpr int PILLOW_C_MODE_YCBCR = 13;
constexpr int PILLOW_C_MODE_HSV = 14;
constexpr int PILLOW_C_MODE_LAB = 15;
constexpr int PILLOW_C_MODE_PA = 16;

constexpr int PILLOW_C_PALETTE_ALPHA_NONE = 0;
constexpr int PILLOW_C_PALETTE_ALPHA_RGBA = 1;
constexpr int PILLOW_C_PALETTE_ALPHA_RGBX = 2;

struct PillowCImage {
    int width;
    int height;
    int mode;
    int channels;
    std::size_t stride;
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint8_t> palette_rgb;
    int exif_orientation = 0;
    std::vector<std::uint8_t> palette_alpha;
    int palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
    bool has_dpi = false;
    double dpi_x = 0.0;
    double dpi_y = 0.0;
    bool has_jfif = false;
    int jfif_major = 0;
    int jfif_minor = 0;
    int jfif_unit = -1;
    int jfif_density_x = 0;
    int jfif_density_y = 0;
    bool has_hotspot = false;
    int hotspot_x = 0;
    int hotspot_y = 0;
    bool has_dib_compression = false;
    int dib_compression = -1;
    bool has_png_gamma = false;
    double png_gamma = 0.0;
    bool has_png_srgb = false;
    int png_srgb = 0;
    bool has_png_chromaticity = false;
    double png_chromaticity[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::vector<std::pair<std::string, std::string>> png_text;
    std::vector<std::uint8_t> png_icc_profile;
    std::vector<std::uint8_t> png_exif;
    std::vector<std::uint8_t> tiff_exif;
    std::vector<std::uint8_t> tiff_icc_profile;
    std::vector<std::uint8_t> jpeg_comment;
    bool has_jpeg_icc_profile = false;
    bool has_jpeg_icc_profile_none = false;
    std::vector<std::uint8_t> jpeg_icc_profile;
    std::vector<std::pair<int, std::vector<std::uint8_t>>> jpeg_photoshop_resources;
    bool has_jpeg_photoshop_resolution_info = false;
    double jpeg_photoshop_x_resolution = 0.0;
    int jpeg_photoshop_displayed_units_x = 0;
    double jpeg_photoshop_y_resolution = 0.0;
    int jpeg_photoshop_displayed_units_y = 0;
    std::vector<std::uint8_t> jpeg_exif;
    std::vector<std::uint8_t> xmp;
    std::vector<int> jpeg_qtables;
    std::size_t jpeg_qtable_count = 0;
    int jpeg_subsampling = -1;
    bool has_png_transparency = false;
    int png_transparency = -1;
    std::vector<std::uint8_t> png_transparency_table;
    bool has_png_rgb_transparency = false;
    std::uint8_t png_rgb_transparency[3] = {0, 0, 0};
    const std::uint8_t* buffer_source = nullptr;
    std::size_t buffer_source_size = 0;
    std::string buffer_raw_mode;
    int buffer_stride = 0;
    int buffer_orientation = 1;
    bool buffer_readonly = false;
};

bool valid_image_shape(int width, int height, int channels);
bool valid_image_shape_allow_empty(int width, int height, int channels);
int channels_for_mode(int mode);
int mode_for_channels(int channels);
bool checked_image_size(int width, int height, int channels, std::size_t* stride, std::size_t* size);
bool checked_image_size_allow_empty(
    int width,
    int height,
    int channels,
    std::size_t* stride,
    std::size_t* size);

std::uint16_t read_le16(const std::uint8_t* data);
std::uint32_t read_le32(const std::uint8_t* data);
std::int32_t read_le_i32(const std::uint8_t* data);
std::uint32_t read_be32(const std::uint8_t* data);
std::uint16_t read_be16(const std::uint8_t* data);
void append_le16(std::vector<std::uint8_t>& out, std::uint16_t value);
void append_be16(std::vector<std::uint8_t>& out, std::uint16_t value);
void append_le32(std::vector<std::uint8_t>& out, std::uint32_t value);
void append_be32(std::vector<std::uint8_t>& out, std::uint32_t value);
void append_le64(std::vector<std::uint8_t>& out, std::uint64_t value);
bool utf8_path_to_wide(const char* path, std::vector<wchar_t>* out);
bool read_binary_file(const char* path, std::vector<std::uint8_t>* out);
bool write_binary_file(const char* path, const std::vector<std::uint8_t>& data);

int pillow_c_tiff_parse_orientation(const std::uint8_t* tiff, std::size_t tiff_size);
int pillow_c_parse_exif_orientation(const std::uint8_t* payload, std::size_t payload_size);
std::uint16_t pillow_c_tiff_read16(const std::uint8_t* data, bool little_endian);
std::uint32_t pillow_c_tiff_read32(const std::uint8_t* data, bool little_endian);
std::uint64_t pillow_c_tiff_read64(const std::uint8_t* data, bool little_endian);
bool pillow_c_tiff_read_signed_rational_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::int32_t>* out_numerators,
    std::vector<std::int32_t>* out_denominators);

bool pillow_c_round_to_i64(double value, std::int64_t* out_value);
int pillow_c_refresh_const_buffer_view_image(const PillowCImage* image);
int pillow_c_detach_buffer_view_image(PillowCImage* image);
int pillow_c_normalize_coordinate(int value, int limit, int* out_value);
int pillow_c_image_pixel_offset(const PillowCImage* image, int x, int y, std::size_t* out_offset);
bool pillow_c_supported_bitmap_mask(const PillowCImage* mask);
bool pillow_c_supported_composite_mask(const PillowCImage* mask);
bool pillow_c_statistics_mask_matches(const PillowCImage* mask, int width, int height);
void pillow_c_copy_palette_if_point_preserves_core_palette(
    const PillowCImage* source,
    PillowCImage* target);
std::uint8_t pillow_c_mask_alpha_at(
    const PillowCImage* mask,
    const std::uint8_t* mask_row,
    int x);
std::uint32_t pillow_c_shift_for_div255(std::uint32_t value);
int pillow_c_apply_builtin_lab_transform(
    const PillowCImage* source,
    PillowCImage* target,
    int target_mode);

bool pillow_c_image_shape_matches(
    const PillowCImage* image,
    int width,
    int height,
    int channels);
bool pillow_c_image_shape_matches(
    const PillowCImage* image,
    int width,
    int height,
    int mode,
    int channels);
bool pillow_c_image_shape_matches(const PillowCImage* left, const PillowCImage* right);
bool pillow_c_image_shape_matches_mode(
    const PillowCImage* image,
    int width,
    int height,
    int mode,
    int channels);
bool pillow_c_image_shapes_match(const PillowCImage* left, const PillowCImage* right);
void pillow_c_copy_palette_if_same_mode(const PillowCImage* source, PillowCImage* target);
std::uint8_t pillow_c_clip_u8_int(int value);
std::uint8_t pillow_c_clip_u8_double(double value);
std::uint8_t pillow_c_clip_resample_u8(std::int64_t value);
std::uint8_t pillow_c_mul_div_255(std::uint8_t value, std::uint8_t alpha);
std::uint8_t pillow_c_reduce_average_u8(std::uint64_t sum, std::uint32_t count);
int pillow_c_ceil_div_int(int value, int divisor);
int pillow_c_clamp_int(int value, int low, int high);
bool pillow_c_checked_mode1_raw_size(
    const PillowCImage* image,
    std::size_t* row_bytes,
    std::size_t* out_size);
std::int32_t pillow_c_read_i32_le(const std::uint8_t* data);
std::uint16_t pillow_c_clip_i32_to_u16(std::int32_t value);
float pillow_c_read_f32_le(const std::uint8_t* data);
void pillow_c_write_i32_le(std::uint8_t* data, std::uint32_t value);
void pillow_c_write_f32_le(std::uint8_t* data, float value);
std::uint8_t pillow_c_round_half_up_clip_u8(double value);
std::int32_t pillow_c_round_half_up_clip_i32_nonnegative(double value);
int pillow_c_paste_image_pixels_into(
    PillowCImage* target,
    const PillowCImage* source,
    int left,
    int top);
int pillow_c_fill_image_pixels(
    PillowCImage* image,
    const std::uint8_t* color,
    std::size_t color_size);
int pillow_c_resize_image_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    PillowCImage* target);
int pillow_c_proportional_resize_size(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    bool cover,
    int* out_width,
    int* out_height);
int pillow_c_copy_transpose_pixels_into(
    const PillowCImage* source,
    int method,
    PillowCImage* target);
bool pillow_c_transpose_output_shape(
    const PillowCImage* source,
    int method,
    int* out_width,
    int* out_height);

int pillow_c_quantize_exact_image_into(
    const PillowCImage* source,
    int colors,
    PillowCImage* target);
int pillow_c_quantize_exact_rgba_gif_into(
    const PillowCImage* source,
    PillowCImage* target,
    bool* out_has_transparency,
    int* out_transparency);
int pillow_c_quantize_median_cut_rgba_gif_into(
    const PillowCImage* source,
    PillowCImage* target,
    bool* out_has_transparency,
    int* out_transparency);
int pillow_c_quantize_exact_rgba_gif_animation_frame_into(
    const PillowCImage* source,
    PillowCImage* target,
    bool reserve_transparency,
    bool* out_has_transparency);
int pillow_c_quantize_exact_la_gif_animation_frame_into(
    const PillowCImage* source,
    PillowCImage* target);

int copy_exif_entries_internal_uint_array_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
    const int* rational_array_tags,
    const std::uint32_t* rational_array_numerators,
    const std::uint32_t* rational_array_denominators,
    std::size_t rational_array_value_count,
    const std::size_t* rational_array_offsets,
    const std::size_t* rational_array_counts,
    std::size_t rational_array_count,
    const int* short_array_tags,
    const std::uint32_t* short_array_values,
    std::size_t short_array_value_count,
    const std::size_t* short_array_offsets,
    const std::size_t* short_array_counts,
    std::size_t short_array_count,
    const int* uint_array_tags,
    const std::uint32_t* uint_array_values,
    std::size_t uint_array_value_count,
    const std::size_t* uint_array_offsets,
    const std::size_t* uint_array_counts,
    std::size_t uint_array_count,
    const int* double_array_tags,
    const double* double_array_values,
    std::size_t double_array_value_count,
    const std::size_t* double_array_offsets,
    const std::size_t* double_array_counts,
    std::size_t double_array_count,
    const int* float_array_tags,
    const float* float_array_values,
    std::size_t float_array_value_count,
    const std::size_t* float_array_offsets,
    const std::size_t* float_array_counts,
    std::size_t float_array_count,
    const int* byte_array_tags,
    const std::uint8_t* byte_array_values,
    std::size_t byte_array_value_count,
    const std::size_t* byte_array_offsets,
    const std::size_t* byte_array_counts,
    std::size_t byte_array_count,
    const int* signed_rational_tags,
    const std::int32_t* signed_rational_numerators,
    const std::int32_t* signed_rational_denominators,
    std::size_t signed_rational_count,
    const int* signed_rational_array_tags,
    const std::int32_t* signed_rational_array_numerators,
    const std::int32_t* signed_rational_array_denominators,
    std::size_t signed_rational_array_value_count,
    const std::size_t* signed_rational_array_offsets,
    const std::size_t* signed_rational_array_counts,
    std::size_t signed_rational_array_count,
    const int* undefined_tags,
    const std::uint8_t* undefined_values,
    std::size_t undefined_value_count,
    const std::size_t* undefined_offsets,
    const std::size_t* undefined_counts,
    std::size_t undefined_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required);

bool pillow_c_inflate_zlib_deflate(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<std::uint8_t>* out,
    std::size_t max_output,
    bool* exceeded = nullptr);

int pillow_c_append_zlib_stored(
    std::vector<std::uint8_t>& out,
    const std::vector<std::uint8_t>& raw,
    std::uint8_t flags);

int pillow_c_png_custom_mode_spec(
    const PillowCImage* image,
    int* color_type,
    int* payload_channels);

int pillow_c_png_encode_custom_image(
    const PillowCImage* image,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    bool has_transparency,
    int transparency,
    bool has_rgb_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b,
    std::vector<std::uint8_t>* out_png,
    const char* text_key = nullptr,
    const char* text_value = nullptr,
    bool optimize = false);

int pillow_c_png_decode_memory(
    const std::uint8_t* data,
    std::size_t size,
    PillowCImage** out_image);

// BEHAV-PDF-001 seam: the default WIC JPEG save to a path, shared with the
// PDF writer (L/RGB/CMYK PDF pages embed a DCTDecode JPEG payload).
namespace pillow_c_jpeg {
int save_jpeg_image(const PillowCImage* image, const char* path);
}

int copy_metadata_blob(
    const std::vector<std::uint8_t>& data,
    int* out_has_blob,
    std::uint8_t* out_blob,
    std::size_t out_blob_size,
    std::size_t* out_blob_required);

int copy_metadata_blob(
    const std::vector<std::uint8_t>& data,
    bool has_blob,
    int* out_has_blob,
    std::uint8_t* out_blob,
    std::size_t out_blob_size,
    std::size_t* out_blob_required);
