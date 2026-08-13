#include "pillow_c_internal.h"
#include "pillow_c_ops_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
inline std::uint32_t shift_for_div255(std::uint32_t value)
{
    return pillow_c_shift_for_div255(value);
}

inline std::uint8_t clip_u8_int(int value)
{
    return pillow_c_clip_u8_int(value);
}

inline std::uint8_t clip_chops_scaled_u8(double value)
{
    if (!(value > 0.0)) {
        return 0;
    }
    if (value >= 256.0) {
        return 255;
    }
    return static_cast<std::uint8_t>(value);
}

constexpr std::array<std::uint8_t, 256> PILLOW_L_TO_LAB_L = {
    0, 1, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 9, 10, 10, 11,
    12, 13, 14, 15, 16, 18, 19, 20, 21, 22, 24, 25, 26, 27, 29, 30,
    31, 32, 34, 35, 36, 37, 39, 40, 41, 42, 43, 45, 46, 47, 48, 49,
    51, 52, 53, 54, 55, 56, 58, 59, 60, 61, 62, 63, 65, 66, 67, 68,
    69, 70, 71, 72, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 85, 86,
    87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 98, 99, 100, 101, 102, 103,
    104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136,
    137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 150, 151,
    152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167,
    168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 177, 178, 179, 180, 181, 182,
    183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 193, 194, 195, 196, 197,
    198, 199, 200, 201, 202, 203, 204, 205, 206, 206, 207, 208, 209, 210, 211, 212,
    213, 214, 215, 216, 217, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 226,
    227, 228, 229, 230, 231, 232, 233, 234, 235, 235, 236, 237, 238, 239, 240, 241,
    242, 243, 244, 244, 245, 246, 247, 248, 249, 250, 251, 251, 252, 253, 254, 255
};


inline void alpha_composite_pixel_rgba(const std::uint8_t* dst, const std::uint8_t* src, std::uint8_t* out)
{
    if (src[3] == 0) {
        out[0] = dst[0];
        out[1] = dst[1];
        out[2] = dst[2];
        out[3] = dst[3];
        return;
    }

    constexpr std::uint32_t precision_bits = 7;
    constexpr std::uint32_t precision = 1u << precision_bits;
    const std::uint32_t blend = static_cast<std::uint32_t>(dst[3]) * (255u - src[3]);
    const std::uint32_t outa255 = static_cast<std::uint32_t>(src[3]) * 255u + blend;
    const std::uint32_t coef1 =
        static_cast<std::uint32_t>(src[3]) * 255u * 255u * precision / outa255;
    const std::uint32_t coef2 = 255u * precision - coef1;

    const std::uint32_t tmpr = static_cast<std::uint32_t>(src[0]) * coef1 +
                               static_cast<std::uint32_t>(dst[0]) * coef2;
    const std::uint32_t tmpg = static_cast<std::uint32_t>(src[1]) * coef1 +
                               static_cast<std::uint32_t>(dst[1]) * coef2;
    const std::uint32_t tmpb = static_cast<std::uint32_t>(src[2]) * coef1 +
                               static_cast<std::uint32_t>(dst[2]) * coef2;

    out[0] = static_cast<std::uint8_t>(
        pillow_c_shift_for_div255(tmpr + (0x80u << precision_bits)) >> precision_bits);
    out[1] = static_cast<std::uint8_t>(
        pillow_c_shift_for_div255(tmpg + (0x80u << precision_bits)) >> precision_bits);
    out[2] = static_cast<std::uint8_t>(
        pillow_c_shift_for_div255(tmpb + (0x80u << precision_bits)) >> precision_bits);
    out[3] = static_cast<std::uint8_t>(pillow_c_shift_for_div255(outa255 + 0x80u));
}

inline std::uint8_t clip_u8(float value)
{
    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 255.0f) {
        return 255;
    }
    return static_cast<std::uint8_t>(value);
}

inline std::uint8_t clip_numeric_f32_to_l(float value)
{
    if (!(value > 0.0f)) {
        return 0;
    }
    if (value >= 255.0f) {
        return 255;
    }
    return static_cast<std::uint8_t>(value);
}

inline std::uint8_t soft_light_u8(std::uint8_t left, std::uint8_t right)
{
    const int left_value = static_cast<int>(left);
    const int right_value = static_cast<int>(right);
    const int inverse_left = 255 - left_value;
    const int value =
        (inverse_left * (left_value * right_value) / 65536) +
        (left_value * (255 - (inverse_left * (255 - right_value) / 255)) / 255);
    return static_cast<std::uint8_t>(value);
}

inline std::uint8_t hard_light_u8(std::uint8_t left, std::uint8_t right)
{
    const int left_value = static_cast<int>(left);
    const int right_value = static_cast<int>(right);
    const int value = right_value < 128 ?
        (left_value * right_value) / 127 :
        255 - (((255 - right_value) * (255 - left_value)) / 127);
    return static_cast<std::uint8_t>(value);
}

inline std::uint8_t overlay_u8(std::uint8_t left, std::uint8_t right)
{
    const int left_value = static_cast<int>(left);
    const int right_value = static_cast<int>(right);
    const int value = left_value < 128 ?
        (left_value * right_value) / 127 :
        255 - (((255 - left_value) * (255 - right_value)) / 127);
    return static_cast<std::uint8_t>(value);
}
inline std::uint8_t rgb_luma_u8(const std::uint8_t* px)
{
    return static_cast<std::uint8_t>(
        (static_cast<std::int32_t>(px[0]) * 19595 +
         static_cast<std::int32_t>(px[1]) * 38470 +
         static_cast<std::int32_t>(px[2]) * 7471 +
         0x8000) >> 16);
}

inline int rgb_luma_1000(const std::uint8_t* px)
{
    return static_cast<int>(px[0]) * 299 +
           static_cast<int>(px[1]) * 587 +
           static_cast<int>(px[2]) * 114;
}

inline void cmyk_to_rgb_u8(const std::uint8_t* cmyk, std::uint8_t* rgb)
{
    const std::uint8_t inverse_k = static_cast<std::uint8_t>(255u - cmyk[3]);
    rgb[0] = pillow_c_mul_div_255(static_cast<std::uint8_t>(255u - cmyk[0]), inverse_k);
    rgb[1] = pillow_c_mul_div_255(static_cast<std::uint8_t>(255u - cmyk[1]), inverse_k);
    rgb[2] = pillow_c_mul_div_255(static_cast<std::uint8_t>(255u - cmyk[2]), inverse_k);
}

inline std::uint8_t cmyk_luma_u8(const std::uint8_t* cmyk)
{
    std::uint8_t rgb[3];
    cmyk_to_rgb_u8(cmyk, rgb);
    return rgb_luma_u8(rgb);
}

struct YcbcrConversionTables {
    std::array<std::int16_t, 256> y_r{};
    std::array<std::int16_t, 256> y_g{};
    std::array<std::int16_t, 256> y_b{};
    std::array<std::int16_t, 256> cb_r{};
    std::array<std::int16_t, 256> cb_g{};
    std::array<std::int16_t, 256> cb_b{};
    std::array<std::int16_t, 256> cr_r{};
    std::array<std::int16_t, 256> cr_g{};
    std::array<std::int16_t, 256> cr_b{};
    std::array<std::int16_t, 256> r_cr{};
    std::array<std::int16_t, 256> g_cb{};
    std::array<std::int16_t, 256> g_cr{};
    std::array<std::int16_t, 256> b_cb{};

    YcbcrConversionTables()
    {
        auto scaled = [](double coefficient, int value) -> std::int16_t {
            return static_cast<std::int16_t>(coefficient * static_cast<double>(value) * 64.0 + 0.5);
        };
        for (int value = 0; value < 256; ++value) {
            const std::size_t index = static_cast<std::size_t>(value);
            y_r[index] = scaled(0.29900, value);
            y_g[index] = scaled(0.58700, value);
            y_b[index] = scaled(0.11400, value);
            cb_r[index] = scaled(-0.16874, value);
            cb_g[index] = scaled(-0.33126, value);
            cb_b[index] = scaled(0.50000, value);
            cr_r[index] = cb_b[index];
            cr_g[index] = scaled(-0.41869, value);
            cr_b[index] = scaled(-0.08131, value);
            const int centered = value - 128;
            r_cr[index] = scaled(1.40200, centered);
            g_cb[index] = scaled(-0.34414, centered);
            g_cr[index] = scaled(-0.71414, centered);
            b_cb[index] = scaled(1.77200, centered);
        }
    }
};

const YcbcrConversionTables& ycbcr_conversion_tables()
{
    static const YcbcrConversionTables tables;
    return tables;
}

inline void rgb_to_ycbcr_u8(const std::uint8_t* rgb, std::uint8_t* ycbcr)
{
    const auto& tables = ycbcr_conversion_tables();
    const std::size_t r = rgb[0];
    const std::size_t g = rgb[1];
    const std::size_t b = rgb[2];
    ycbcr[0] = static_cast<std::uint8_t>((tables.y_r[r] + tables.y_g[g] + tables.y_b[b]) >> 6);
    ycbcr[1] = static_cast<std::uint8_t>(((tables.cb_r[r] + tables.cb_g[g] + tables.cb_b[b]) >> 6) + 128);
    ycbcr[2] = static_cast<std::uint8_t>(((tables.cr_r[r] + tables.cr_g[g] + tables.cr_b[b]) >> 6) + 128);
}

inline void ycbcr_to_rgb_u8(const std::uint8_t* ycbcr, std::uint8_t* rgb)
{
    const auto& tables = ycbcr_conversion_tables();
    const int y = ycbcr[0];
    const std::size_t cb = ycbcr[1];
    const std::size_t cr = ycbcr[2];
    rgb[0] = pillow_c_clip_u8_int(y + (tables.r_cr[cr] >> 6));
    rgb[1] = pillow_c_clip_u8_int(y + ((tables.g_cb[cb] + tables.g_cr[cr]) >> 6));
    rgb[2] = pillow_c_clip_u8_int(y + (tables.b_cb[cb] >> 6));
}

inline void rgb_to_hsv_u8(const std::uint8_t* rgb, std::uint8_t* hsv)
{
    const std::uint8_t r = rgb[0];
    const std::uint8_t g = rgb[1];
    const std::uint8_t b = rgb[2];
    const std::uint8_t maxc = std::max(r, std::max(g, b));
    const std::uint8_t minc = std::min(r, std::min(g, b));
    hsv[2] = maxc;
    if (minc == maxc) {
        hsv[0] = 0;
        hsv[1] = 0;
        return;
    }

    const float chroma = static_cast<float>(maxc - minc);
    const float saturation = chroma / static_cast<float>(maxc);
    const float rc = static_cast<float>(maxc - r) / chroma;
    const float gc = static_cast<float>(maxc - g) / chroma;
    const float bc = static_cast<float>(maxc - b) / chroma;
    float hue;
    if (r == maxc) {
        hue = bc - gc;
    } else if (g == maxc) {
        hue = static_cast<float>(2.0 + rc - bc);
    } else {
        hue = static_cast<float>(4.0 + gc - rc);
    }
    hue = static_cast<float>(std::fmod(hue / 6.0 + 1.0, 1.0));

    hsv[0] = pillow_c_clip_u8_int(static_cast<int>(hue * 255.0));
    hsv[1] = pillow_c_clip_u8_int(static_cast<int>(saturation * 255.0));
}

inline void hsv_to_rgb_u8(const std::uint8_t* hsv, std::uint8_t* rgb)
{
    const std::uint8_t h = hsv[0];
    const std::uint8_t s = hsv[1];
    const std::uint8_t v = hsv[2];
    if (s == 0) {
        rgb[0] = v;
        rgb[1] = v;
        rgb[2] = v;
        return;
    }

    const int sector = static_cast<int>(std::floor(static_cast<float>(h) * 6.0 / 255.0));
    const float fraction = static_cast<float>(
        static_cast<float>(h) * 6.0 / 255.0 - static_cast<float>(sector));
    const float saturation = static_cast<float>(static_cast<float>(s) / 255.0);
    const std::uint8_t p = pillow_c_clip_u8_int(static_cast<int>(
        std::round(static_cast<float>(v) * (1.0 - saturation))));
    const std::uint8_t q = pillow_c_clip_u8_int(static_cast<int>(
        std::round(static_cast<float>(v) * (1.0 - saturation * fraction))));
    const std::uint8_t t = pillow_c_clip_u8_int(static_cast<int>(
        std::round(static_cast<float>(v) * (1.0 - saturation * (1.0 - fraction)))));

    switch (sector % 6) {
    case 0:
        rgb[0] = v;
        rgb[1] = t;
        rgb[2] = p;
        return;
    case 1:
        rgb[0] = q;
        rgb[1] = v;
        rgb[2] = p;
        return;
    case 2:
        rgb[0] = p;
        rgb[1] = v;
        rgb[2] = t;
        return;
    case 3:
        rgb[0] = p;
        rgb[1] = q;
        rgb[2] = v;
        return;
    case 4:
        rgb[0] = t;
        rgb[1] = p;
        rgb[2] = v;
        return;
    default:
        rgb[0] = v;
        rgb[1] = p;
        rgb[2] = q;
        return;
    }
}

template <typename Func>
int ops_with_detached_buffer_view(PillowCImage* image, Func func)
{
    const int status = pillow_c_detach_buffer_view_image(image);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return func();
}

int overlapping_width(const PillowCImage* left, const PillowCImage* right)
{
    return std::min(left->width, right->width);
}

int overlapping_height(const PillowCImage* left, const PillowCImage* right)
{
    return std::min(left->height, right->height);
}

int positive_mod(int value, int modulus)
{
    int result = value % modulus;
    if (result < 0) {
        result += modulus;
    }
    return result;
}

int floor_div_int(int numerator, int denominator)
{
    int quotient = numerator / denominator;
    const int remainder = numerator % denominator;
    if (remainder != 0 && ((remainder < 0) != (denominator < 0))) {
        quotient -= 1;
    }
    return quotient;
}

bool numeric_i_or_f_mode(const PillowCImage* image)
{
    return image && (image->mode == PILLOW_C_MODE_I || image->mode == PILLOW_C_MODE_F);
}

bool imagechops_binary_wrong_mode(const PillowCImage* image)
{
    return numeric_i_or_f_mode(image);
}

int validate_chops_binary_target(
    const PillowCImage* left,
    const PillowCImage* right,
    const PillowCImage* target,
    int* out_width,
    int* out_height)
{
    if (!left || !right || !target || !out_width || !out_height) {
        return PILLOW_C_NULL_POINTER;
    }
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }
    if (imagechops_binary_wrong_mode(left)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    *out_width = overlapping_width(left, right);
    *out_height = overlapping_height(left, right);
    if (!pillow_c_image_shape_matches(target, *out_width, *out_height, left->mode, left->channels)) {
        return PILLOW_C_MISMATCH;
    }
    const int left_refresh_status = pillow_c_refresh_const_buffer_view_image(left);
    if (left_refresh_status != PILLOW_C_OK) {
        return left_refresh_status;
    }
    const int right_refresh_status = pillow_c_refresh_const_buffer_view_image(right);
    if (right_refresh_status != PILLOW_C_OK) {
        return right_refresh_status;
    }
    return PILLOW_C_OK;
}

bool transpose_output_shape(const PillowCImage* source, int method, int* out_width, int* out_height)
{
    if (!source || !out_width || !out_height || method < 0 || method > 6) {
        return false;
    }
    const bool swaps_axes = method == 2 || method == 4 || method == 5 || method == 6;
    *out_width = swaps_axes ? source->height : source->width;
    *out_height = swaps_axes ? source->width : source->height;
    return true;
}

int copy_crop_pixels_into(
    const PillowCImage* source,
    int left,
    int top,
    int right,
    int bottom,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (right < left || bottom < top) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const std::int64_t out_width_i64 = static_cast<std::int64_t>(right) - left;
    const std::int64_t out_height_i64 = static_cast<std::int64_t>(bottom) - top;
    if (out_width_i64 > INT_MAX || out_height_i64 > INT_MAX) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int out_width = static_cast<int>(out_width_i64);
    const int out_height = static_cast<int>(out_height_i64);
    if (!pillow_c_image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    std::fill(target->pixels.begin(), target->pixels.end(), static_cast<std::uint8_t>(0));
    const int copy_left = left < 0 ? 0 : left;
    const int copy_top = top < 0 ? 0 : top;
    const int copy_right = right > source->width ? source->width : right;
    const int copy_bottom = bottom > source->height ? source->height : bottom;

    if (copy_right > copy_left && copy_bottom > copy_top) {
        const std::size_t row_bytes =
            static_cast<std::size_t>(copy_right - copy_left) * source->channels;
        for (int y = copy_top; y < copy_bottom; ++y) {
            const int dst_y = y - top;
            const int dst_x = copy_left - left;
            const std::size_t src_offset =
                static_cast<std::size_t>(y) * source->stride +
                static_cast<std::size_t>(copy_left) * source->channels;
            const std::size_t dst_offset =
                static_cast<std::size_t>(dst_y) * target->stride +
                static_cast<std::size_t>(dst_x) * target->channels;
            std::memcpy(target->pixels.data() + dst_offset, source->pixels.data() + src_offset, row_bytes);
        }
    }

    pillow_c_copy_palette_if_same_mode(source, target);
    return PILLOW_C_OK;
}

int output_size_from_borders(
    const PillowCImage* source,
    int left,
    int top,
    int right,
    int bottom,
    int* out_width,
    int* out_height)
{
    if (!source || !out_width || !out_height) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::int64_t width =
        static_cast<std::int64_t>(left) + source->width + static_cast<std::int64_t>(right);
    const std::int64_t height =
        static_cast<std::int64_t>(top) + source->height + static_cast<std::int64_t>(bottom);
    if (width < 0 || height < 0 || width > INT_MAX || height > INT_MAX) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_width = static_cast<int>(width);
    *out_height = static_cast<int>(height);
    return PILLOW_C_OK;
}

int paste_image_pixels_into(PillowCImage* target, const PillowCImage* source, int left, int top)
{
    if (!target || !source) {
        return PILLOW_C_NULL_POINTER;
    }
    if (target->channels != source->channels) {
        return PILLOW_C_MISMATCH;
    }

    const std::int64_t dst_left_i64 = left < 0 ? 0 : left;
    const std::int64_t dst_top_i64 = top < 0 ? 0 : top;
    const std::int64_t source_right_i64 = static_cast<std::int64_t>(left) + source->width;
    const std::int64_t source_bottom_i64 = static_cast<std::int64_t>(top) + source->height;
    const std::int64_t dst_right_i64 =
        source_right_i64 > target->width ? target->width : source_right_i64;
    const std::int64_t dst_bottom_i64 =
        source_bottom_i64 > target->height ? target->height : source_bottom_i64;

    if (dst_right_i64 <= dst_left_i64 || dst_bottom_i64 <= dst_top_i64) {
        return PILLOW_C_OK;
    }

    const int dst_left = static_cast<int>(dst_left_i64);
    const int dst_top = static_cast<int>(dst_top_i64);
    const int dst_right = static_cast<int>(dst_right_i64);
    const int dst_bottom = static_cast<int>(dst_bottom_i64);
    const int src_left = dst_left - left;
    const int src_top = dst_top - top;
    const std::size_t row_bytes =
        static_cast<std::size_t>(dst_right - dst_left) * target->channels;

    for (int y = 0; y < dst_bottom - dst_top; ++y) {
        const std::size_t src_offset =
            static_cast<std::size_t>(src_top + y) * source->stride +
            static_cast<std::size_t>(src_left) * source->channels;
        const std::size_t dst_offset =
            static_cast<std::size_t>(dst_top + y) * target->stride +
            static_cast<std::size_t>(dst_left) * target->channels;
        std::memcpy(target->pixels.data() + dst_offset, source->pixels.data() + src_offset, row_bytes);
    }

    return PILLOW_C_OK;
}

int convert_image_mode_into(const PillowCImage* source, int target_mode, PillowCImage* target);

int paste_image_masked_into(
    PillowCImage* target,
    const PillowCImage* source,
    int left,
    int top,
    const PillowCImage* mask)
{
    if (!target || !source || !mask) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_supported_composite_mask(mask)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (mask->width != source->width || mask->height != source->height) {
        return PILLOW_C_MISMATCH;
    }

    const PillowCImage* effective_source = source;
    PillowCImage converted_source{};
    try {
        if (source->mode != target->mode || source->channels != target->channels) {
            std::size_t stride = 0;
            std::size_t size = 0;
            if (!checked_image_size_allow_empty(source->width, source->height, target->channels, &stride, &size)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            converted_source = PillowCImage{
                source->width,
                source->height,
                target->mode,
                target->channels,
                stride,
                std::vector<std::uint8_t>(size)};
            const int status = convert_image_mode_into(source, target->mode, &converted_source);
            if (status != PILLOW_C_OK) {
                return status;
            }
            effective_source = &converted_source;
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    const std::int64_t dst_left_i64 = left < 0 ? 0 : left;
    const std::int64_t dst_top_i64 = top < 0 ? 0 : top;
    const std::int64_t source_right_i64 = static_cast<std::int64_t>(left) + source->width;
    const std::int64_t source_bottom_i64 = static_cast<std::int64_t>(top) + source->height;
    const std::int64_t dst_right_i64 =
        source_right_i64 > target->width ? target->width : source_right_i64;
    const std::int64_t dst_bottom_i64 =
        source_bottom_i64 > target->height ? target->height : source_bottom_i64;

    if (dst_right_i64 <= dst_left_i64 || dst_bottom_i64 <= dst_top_i64) {
        return PILLOW_C_OK;
    }

    const int dst_left = static_cast<int>(dst_left_i64);
    const int dst_top = static_cast<int>(dst_top_i64);
    const int dst_right = static_cast<int>(dst_right_i64);
    const int dst_bottom = static_cast<int>(dst_bottom_i64);
    const int src_left = dst_left - left;
    const int src_top = dst_top - top;
    const int channels = target->channels;

    for (int y = 0; y < dst_bottom - dst_top; ++y) {
        const std::uint8_t* src_row =
            effective_source->pixels.data() +
            static_cast<std::size_t>(src_top + y) * effective_source->stride +
            static_cast<std::size_t>(src_left) * effective_source->channels;
        const std::uint8_t* mask_row =
            mask->pixels.data() +
            static_cast<std::size_t>(src_top + y) * mask->stride +
            static_cast<std::size_t>(src_left) * mask->channels;
        std::uint8_t* dst_row =
            target->pixels.data() +
            static_cast<std::size_t>(dst_top + y) * target->stride +
            static_cast<std::size_t>(dst_left) * target->channels;

        for (int x = 0; x < dst_right - dst_left; ++x) {
            const std::uint8_t alpha = pillow_c_mask_alpha_at(mask, mask_row, x);
            if (alpha == 0) {
                continue;
            }

            const std::size_t pixel_offset = static_cast<std::size_t>(x) * channels;
            if (alpha == 255) {
                std::memcpy(dst_row + pixel_offset, src_row + pixel_offset, static_cast<std::size_t>(channels));
                continue;
            }

            for (int channel = 0; channel < channels; ++channel) {
                const std::uint8_t dst = dst_row[pixel_offset + channel];
                const std::uint8_t src = src_row[pixel_offset + channel];
                const std::uint32_t blended =
                    static_cast<std::uint32_t>(dst) * (255u - alpha) +
                    static_cast<std::uint32_t>(src) * alpha +
                    128u;
                dst_row[pixel_offset + channel] = static_cast<std::uint8_t>(shift_for_div255(blended));
            }
        }
    }

    return PILLOW_C_OK;
}

int paste_color_into(
    PillowCImage* target,
    const std::uint8_t* color,
    std::size_t color_size,
    int left,
    int top,
    int right,
    int bottom,
    const PillowCImage* mask)
{
    if (!target || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(target->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    const std::int64_t region_width_i64 = static_cast<std::int64_t>(right) - left;
    const std::int64_t region_height_i64 = static_cast<std::int64_t>(bottom) - top;
    if (region_width_i64 <= 0 || region_height_i64 <= 0) {
        return PILLOW_C_OK;
    }
    if (region_width_i64 > INT_MAX || region_height_i64 > INT_MAX) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int region_width = static_cast<int>(region_width_i64);
    const int region_height = static_cast<int>(region_height_i64);
    if (mask) {
        if (!pillow_c_supported_composite_mask(mask)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (mask->width != region_width || mask->height != region_height) {
            return PILLOW_C_MISMATCH;
        }
    }

    const std::int64_t dst_left_i64 = left < 0 ? 0 : left;
    const std::int64_t dst_top_i64 = top < 0 ? 0 : top;
    const std::int64_t dst_right_i64 = right > target->width ? target->width : right;
    const std::int64_t dst_bottom_i64 = bottom > target->height ? target->height : bottom;

    if (dst_right_i64 <= dst_left_i64 || dst_bottom_i64 <= dst_top_i64) {
        return PILLOW_C_OK;
    }

    const int dst_left = static_cast<int>(dst_left_i64);
    const int dst_top = static_cast<int>(dst_top_i64);
    const int dst_right = static_cast<int>(dst_right_i64);
    const int dst_bottom = static_cast<int>(dst_bottom_i64);
    const int src_left = dst_left - left;
    const int src_top = dst_top - top;
    const int channels = target->channels;

    for (int y = 0; y < dst_bottom - dst_top; ++y) {
        const std::uint8_t* mask_row = mask
            ? mask->pixels.data() +
                static_cast<std::size_t>(src_top + y) * mask->stride +
                static_cast<std::size_t>(src_left) * mask->channels
            : nullptr;
        std::uint8_t* dst_row =
            target->pixels.data() +
            static_cast<std::size_t>(dst_top + y) * target->stride +
            static_cast<std::size_t>(dst_left) * target->channels;

        for (int x = 0; x < dst_right - dst_left; ++x) {
            const std::size_t pixel_offset = static_cast<std::size_t>(x) * channels;
            if (!mask) {
                std::memcpy(dst_row + pixel_offset, color, color_size);
                continue;
            }

            const std::uint8_t alpha = pillow_c_mask_alpha_at(mask, mask_row, x);
            if (alpha == 0) {
                continue;
            }
            if (alpha == 255) {
                std::memcpy(dst_row + pixel_offset, color, color_size);
                continue;
            }

            for (int channel = 0; channel < channels; ++channel) {
                const std::uint8_t dst = dst_row[pixel_offset + channel];
                const std::uint8_t src = color[channel];
                const std::uint32_t blended =
                    static_cast<std::uint32_t>(dst) * (255u - alpha) +
                    static_cast<std::uint32_t>(src) * alpha +
                    128u;
                dst_row[pixel_offset + channel] = static_cast<std::uint8_t>(shift_for_div255(blended));
            }
        }
    }

    return PILLOW_C_OK;
}

int normalize_coordinate(int value, int limit, int* out_value)
{
    if (!out_value || limit <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int normalized = value;
    if (normalized < 0) {
        normalized += limit;
    }
    if (normalized < 0 || normalized >= limit) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_value = normalized;
    return PILLOW_C_OK;
}

int image_pixel_offset(const PillowCImage* image, int x, int y, std::size_t* out_offset)
{
    if (!image || !out_offset) {
        return PILLOW_C_NULL_POINTER;
    }
    int normalized_x = 0;
    int normalized_y = 0;
    int status = normalize_coordinate(x, image->width, &normalized_x);
    if (status != PILLOW_C_OK) {
        return status;
    }
    status = normalize_coordinate(y, image->height, &normalized_y);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_offset =
        static_cast<std::size_t>(normalized_y) * image->stride +
        static_cast<std::size_t>(normalized_x) * image->channels;
    return PILLOW_C_OK;
}

int get_pixel_image(
    const PillowCImage* image,
    int x,
    int y,
    std::uint8_t* out_color,
    std::size_t out_color_size)
{
    if (!image || !out_color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::size_t offset = 0;
    const int status = image_pixel_offset(image, x, y, &offset);
    if (status != PILLOW_C_OK) {
        return status;
    }
    std::memcpy(out_color, image->pixels.data() + offset, out_color_size);
    return PILLOW_C_OK;
}

int put_pixel_image(
    PillowCImage* image,
    int x,
    int y,
    const std::uint8_t* color,
    std::size_t color_size)
{
    if (!image || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != 1 && color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::size_t offset = 0;
    const int status = image_pixel_offset(image, x, y, &offset);
    if (status != PILLOW_C_OK) {
        return status;
    }
    std::uint8_t* dst = image->pixels.data() + offset;
    if (color_size == 1 && image->channels > 1) {
        dst[0] = color[0];
        std::fill(dst + 1, dst + image->channels, static_cast<std::uint8_t>(0));
    } else {
        std::memcpy(dst, color, color_size);
    }
    return PILLOW_C_OK;
}

int fill_image_pixels(PillowCImage* image, const std::uint8_t* color, std::size_t color_size);

int expand_image_into(
    const PillowCImage* source,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* color,
    std::size_t color_size,
    PillowCImage* target)
{
    if (!source || !color || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(source->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }

    int out_width = 0;
    int out_height = 0;
    const int size_status = output_size_from_borders(source, left, top, right, bottom, &out_width, &out_height);
    if (size_status != PILLOW_C_OK) {
        return size_status;
    }
    if (!pillow_c_image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    int status = fill_image_pixels(target, color, color_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    status = paste_image_pixels_into(target, source, left, top);
    if (status == PILLOW_C_OK) {
        pillow_c_copy_palette_if_same_mode(source, target);
    }
    return status;
}

int copy_transpose_pixels_into(const PillowCImage* source, int method, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }

    int out_width = 0;
    int out_height = 0;
    if (!transpose_output_shape(source, method, &out_width, &out_height)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    for (int dst_y = 0; dst_y < target->height; ++dst_y) {
        for (int dst_x = 0; dst_x < target->width; ++dst_x) {
            int src_x = 0;
            int src_y = 0;
            switch (method) {
            case 0:
                src_x = source->width - 1 - dst_x;
                src_y = dst_y;
                break;
            case 1:
                src_x = dst_x;
                src_y = source->height - 1 - dst_y;
                break;
            case 2:
                src_x = source->width - 1 - dst_y;
                src_y = dst_x;
                break;
            case 3:
                src_x = source->width - 1 - dst_x;
                src_y = source->height - 1 - dst_y;
                break;
            case 4:
                src_x = dst_y;
                src_y = source->height - 1 - dst_x;
                break;
            case 5:
                src_x = dst_y;
                src_y = dst_x;
                break;
            case 6:
                src_x = source->width - 1 - dst_y;
                src_y = source->height - 1 - dst_x;
                break;
            }

            const std::size_t src_offset =
                static_cast<std::size_t>(src_y) * source->stride +
                static_cast<std::size_t>(src_x) * source->channels;
            const std::size_t dst_offset =
                static_cast<std::size_t>(dst_y) * target->stride +
                static_cast<std::size_t>(dst_x) * target->channels;
            std::memcpy(
                target->pixels.data() + dst_offset,
                source->pixels.data() + src_offset,
                static_cast<std::size_t>(source->channels));
        }
    }

    pillow_c_copy_palette_if_same_mode(source, target);
    return PILLOW_C_OK;
}

int offset_image_into(
    const PillowCImage* source,
    int x_offset,
    int y_offset,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if (target->pixels.empty()) {
        pillow_c_copy_palette_if_same_mode(source, target);
        return PILLOW_C_OK;
    }

    const int width = source->width;
    const int height = source->height;
    const int channels = source->channels;
    if (width <= 0 || height <= 0) {
        pillow_c_copy_palette_if_same_mode(source, target);
        return PILLOW_C_OK;
    }

    const int normalized_x = positive_mod(x_offset, width);
    const int normalized_y = positive_mod(y_offset, height);
    const std::size_t pixel_bytes = static_cast<std::size_t>(channels);
    for (int dst_y = 0; dst_y < height; ++dst_y) {
        const int src_y = positive_mod(dst_y - normalized_y, height);
        const std::uint8_t* src_row = source->pixels.data() + static_cast<std::size_t>(src_y) * source->stride;
        std::uint8_t* dst_row = target->pixels.data() + static_cast<std::size_t>(dst_y) * target->stride;
        for (int dst_x = 0; dst_x < width; ++dst_x) {
            const int src_x = positive_mod(dst_x - normalized_x, width);
            std::memcpy(
                dst_row + static_cast<std::size_t>(dst_x) * pixel_bytes,
                src_row + static_cast<std::size_t>(src_x) * pixel_bytes,
                pixel_bytes);
        }
    }
    pillow_c_copy_palette_if_same_mode(source, target);
    return PILLOW_C_OK;
}

int fill_image_pixels(PillowCImage* image, const std::uint8_t* color, std::size_t color_size)
{
    if (!image || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    if (image->channels == 1) {
        std::memset(image->pixels.data(), color[0], image->pixels.size());
        return PILLOW_C_OK;
    }

    std::uint8_t* data = image->pixels.data();
    std::memcpy(data, color, color_size);
    std::size_t filled = color_size;
    while (filled < image->pixels.size()) {
        const std::size_t copy_size = std::min(filled, image->pixels.size() - filled);
        std::memcpy(data + filled, data, copy_size);
        filled += copy_size;
    }
    return PILLOW_C_OK;
}

int composite_image_into(
    const PillowCImage* source,
    const PillowCImage* target_source,
    const PillowCImage* mask,
    PillowCImage* target)
{
    if (!source || !target_source || !mask || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_supported_composite_mask(mask)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (mask->width != source->width || mask->height != source->height) {
        return PILLOW_C_MISMATCH;
    }
    if (!pillow_c_image_shape_matches(target, target_source)) {
        return PILLOW_C_MISMATCH;
    }
    const int source_refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (source_refresh_status != PILLOW_C_OK) {
        return source_refresh_status;
    }
    const int target_source_refresh_status = pillow_c_refresh_const_buffer_view_image(target_source);
    if (target_source_refresh_status != PILLOW_C_OK) {
        return target_source_refresh_status;
    }
    const int mask_refresh_status = pillow_c_refresh_const_buffer_view_image(mask);
    if (mask_refresh_status != PILLOW_C_OK) {
        return mask_refresh_status;
    }

    const PillowCImage* effective_source = source;
    PillowCImage converted_source{};
    try {
        if (source->mode != target_source->mode || source->channels != target_source->channels) {
            std::size_t stride = 0;
            std::size_t size = 0;
            if (!checked_image_size_allow_empty(source->width, source->height, target_source->channels, &stride, &size)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            converted_source = PillowCImage{
                source->width,
                source->height,
                target_source->mode,
                target_source->channels,
                stride,
                std::vector<std::uint8_t>(size)};
            const int convert_status = convert_image_mode_into(source, target_source->mode, &converted_source);
            if (convert_status != PILLOW_C_OK) {
                return convert_status;
            }
            effective_source = &converted_source;
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    if (!target_source->pixels.empty()) {
        std::memcpy(target->pixels.data(), target_source->pixels.data(), target_source->pixels.size());
    }

    const int width = overlapping_width(effective_source, target_source);
    const int height = overlapping_height(effective_source, target_source);
    if (width <= 0 || height <= 0) {
        return PILLOW_C_OK;
    }

    const int channels = effective_source->channels;
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* source_row = effective_source->pixels.data() + static_cast<std::size_t>(y) * effective_source->stride;
        const std::uint8_t* target_source_row = target_source->pixels.data() + static_cast<std::size_t>(y) * target_source->stride;
        const std::uint8_t* mask_row = mask->pixels.data() + static_cast<std::size_t>(y) * mask->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;

        for (int x = 0; x < width; ++x) {
            const std::uint8_t alpha = pillow_c_mask_alpha_at(mask, mask_row, x);
            if (alpha == 0) {
                continue;
            }

            const std::size_t pixel_offset = static_cast<std::size_t>(x) * channels;
            if (alpha == 255) {
                std::memcpy(target_row + pixel_offset, source_row + pixel_offset, static_cast<std::size_t>(channels));
                continue;
            }

            for (int channel = 0; channel < channels; ++channel) {
                const std::uint8_t dst = target_source_row[pixel_offset + channel];
                const std::uint8_t src = source_row[pixel_offset + channel];
                const std::uint32_t blended =
                    static_cast<std::uint32_t>(dst) * (255u - alpha) +
                    static_cast<std::uint32_t>(src) * alpha +
                    128u;
                target_row[pixel_offset + channel] = static_cast<std::uint8_t>(shift_for_div255(blended));
            }
        }
    }
    return PILLOW_C_OK;
}

int constant_image_into(const PillowCImage* source, int value, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_L, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }
    std::memset(target->pixels.data(), clip_u8_int(value), target->pixels.size());
    return PILLOW_C_OK;
}

int apply_point_lut_into(const PillowCImage* source, const std::uint8_t* lut, std::size_t lut_size, PillowCImage* target)
{
    if (!source || !lut || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (lut_size != static_cast<std::size_t>(source->channels) * 256u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!pillow_c_image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if (source->pixels.empty()) {
        pillow_c_copy_palette_if_same_mode(source, target);
        pillow_c_copy_palette_if_point_preserves_core_palette(source, target);
        return PILLOW_C_OK;
    }

    const std::uint8_t* src = source->pixels.data();
    std::uint8_t* dst = target->pixels.data();
    const std::size_t count = source->pixels.size();
    const int channels = source->channels;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t channel = static_cast<std::size_t>(i % channels);
        dst[i] = lut[channel * 256u + src[i]];
    }
    pillow_c_copy_palette_if_same_mode(source, target);
    return PILLOW_C_OK;
}

bool point_lut_target_mode_supported(const PillowCImage* source, int target_mode)
{
    if (!source) {
        return false;
    }
    if (target_mode == source->mode) {
        return true;
    }
    return source->channels == 1 &&
           (target_mode == PILLOW_C_MODE_1 || target_mode == PILLOW_C_MODE_L || target_mode == PILLOW_C_MODE_P);
}

int point_lut_target_channels(const PillowCImage* source, int target_mode)
{
    if (!point_lut_target_mode_supported(source, target_mode)) {
        return 0;
    }
    return target_mode == source->mode ? source->channels : 1;
}

int apply_point_lut_mode_into(
    const PillowCImage* source,
    const std::uint8_t* lut,
    std::size_t lut_size,
    int target_mode,
    PillowCImage* target)
{
    if (!source || !lut || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    const int target_channels = point_lut_target_channels(source, target_mode);
    if (target_channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (lut_size != static_cast<std::size_t>(source->channels) * 256u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if (source->pixels.empty()) {
        pillow_c_copy_palette_if_same_mode(source, target);
        return PILLOW_C_OK;
    }
    if (target_mode == source->mode) {
        return apply_point_lut_into(source, lut, lut_size, target);
    }

    const std::uint8_t* src = source->pixels.data();
    std::uint8_t* dst = target->pixels.data();
    const std::size_t count = source->pixels.size();
    for (std::size_t i = 0; i < count; ++i) {
        dst[i] = lut[src[i]];
    }
    pillow_c_copy_palette_if_point_preserves_core_palette(source, target);
    return PILLOW_C_OK;
}

int apply_single_lut_into(const PillowCImage* source, const std::uint8_t* lut, std::size_t lut_size, PillowCImage* target)
{
    if (!source || !lut || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (lut_size != 256u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!pillow_c_image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* src = source->pixels.data();
    std::uint8_t* dst = target->pixels.data();
    const std::size_t count = source->pixels.size();
    for (std::size_t i = 0; i < count; ++i) {
        dst[i] = lut[src[i]];
    }
    return PILLOW_C_OK;
}

bool supports_imageops_lut(const PillowCImage* source)
{
    return source && (source->mode == PILLOW_C_MODE_L || source->mode == PILLOW_C_MODE_RGB);
}

int apply_imageops_lut_into(const PillowCImage* source, const std::uint8_t* lut, PillowCImage* target)
{
    if (!source || !lut || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!supports_imageops_lut(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* src = source->pixels.data();
    std::uint8_t* dst = target->pixels.data();
    const std::size_t count = source->pixels.size();
    for (std::size_t i = 0; i < count; ++i) {
        dst[i] = lut[src[i]];
    }
    return PILLOW_C_OK;
}

int invert_image_into(const PillowCImage* source, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (source->mode == PILLOW_C_MODE_1 && source->channels == 1) {
        if (!pillow_c_image_shape_matches(target, source)) {
            return PILLOW_C_MISMATCH;
        }
        const std::uint8_t* src = source->pixels.data();
        std::uint8_t* dst = target->pixels.data();
        const std::size_t count = source->pixels.size();
        for (std::size_t i = 0; i < count; ++i) {
            dst[i] = src[i] == 0 ? 255u : 0u;
        }
        return PILLOW_C_OK;
    }

    std::uint8_t lut[256];
    for (int ix = 0; ix < 256; ++ix) {
        lut[ix] = static_cast<std::uint8_t>(255 - ix);
    }
    return apply_imageops_lut_into(source, lut, target);
}

int chops_invert_image_into(const PillowCImage* source, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    const std::uint8_t* src = source->pixels.data();
    std::uint8_t* dst = target->pixels.data();
    const std::size_t count = source->pixels.size();
    for (std::size_t i = 0; i < count; ++i) {
        dst[i] = static_cast<std::uint8_t>(255 - src[i]);
    }
    return PILLOW_C_OK;
}

int posterize_image_into(const PillowCImage* source, int bits, PillowCImage* target)
{
    if (bits > 8) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::uint8_t lut[256];
    if (bits <= 0) {
        std::fill(lut, lut + 256, static_cast<std::uint8_t>(0));
    } else {
        const int mask = (0xff << (8 - bits)) & 0xff;
        for (int ix = 0; ix < 256; ++ix) {
            lut[ix] = static_cast<std::uint8_t>(ix & mask);
        }
    }
    return apply_imageops_lut_into(source, lut, target);
}

int solarize_image_into(const PillowCImage* source, double threshold, PillowCImage* target)
{
    std::uint8_t lut[256];
    for (int ix = 0; ix < 256; ++ix) {
        lut[ix] = static_cast<double>(ix) < threshold
            ? static_cast<std::uint8_t>(ix)
            : static_cast<std::uint8_t>(255 - ix);
    }
    return apply_imageops_lut_into(source, lut, target);
}

bool valid_colorize_points(bool has_mid, int blackpoint, int whitepoint, int midpoint)
{
    if (has_mid) {
        return 0 <= blackpoint &&
               blackpoint <= midpoint &&
               midpoint <= whitepoint &&
               whitepoint <= 255;
    }
    return 0 <= blackpoint &&
           blackpoint <= whitepoint &&
           whitepoint <= 255;
}

void fill_colorize_segment(
    std::uint8_t* lut,
    int start,
    int end,
    const std::uint8_t* left,
    const std::uint8_t* right)
{
    const int length = end - start;
    for (int i = 0; i < length; ++i) {
        for (int channel = 0; channel < 3; ++channel) {
            const int delta = static_cast<int>(right[channel]) - static_cast<int>(left[channel]);
            const int value = static_cast<int>(left[channel]) + floor_div_int(i * delta, length);
            lut[static_cast<std::size_t>(channel) * 256u + static_cast<std::size_t>(start + i)] =
                static_cast<std::uint8_t>(value);
        }
    }
}

int colorize_image_into(
    const PillowCImage* source,
    const std::uint8_t* black,
    const std::uint8_t* white,
    bool has_mid,
    const std::uint8_t* mid,
    int blackpoint,
    int whitepoint,
    int midpoint,
    PillowCImage* target)
{
    if (!source || !black || !white || !target || (has_mid && !mid)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (source->mode != PILLOW_C_MODE_L || source->channels != 1 ||
        !valid_colorize_points(has_mid, blackpoint, whitepoint, midpoint)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_RGB, 3)) {
        return PILLOW_C_MISMATCH;
    }

    std::uint8_t lut[3 * 256];
    for (int ix = 0; ix < blackpoint; ++ix) {
        for (int channel = 0; channel < 3; ++channel) {
            lut[static_cast<std::size_t>(channel) * 256u + static_cast<std::size_t>(ix)] = black[channel];
        }
    }
    if (has_mid) {
        fill_colorize_segment(lut, blackpoint, midpoint, black, mid);
        fill_colorize_segment(lut, midpoint, whitepoint, mid, white);
    } else {
        fill_colorize_segment(lut, blackpoint, whitepoint, black, white);
    }
    for (int ix = whitepoint; ix < 256; ++ix) {
        for (int channel = 0; channel < 3; ++channel) {
            lut[static_cast<std::size_t>(channel) * 256u + static_cast<std::size_t>(ix)] = white[channel];
        }
    }

    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }
    const std::uint8_t* src = source->pixels.data();
    std::uint8_t* dst = target->pixels.data();
    const std::size_t pixel_count = static_cast<std::size_t>(source->width) * source->height;
    for (std::size_t index = 0; index < pixel_count; ++index) {
        const std::uint8_t value = src[index];
        dst[index * 3u] = lut[value];
        dst[index * 3u + 1u] = lut[256u + value];
        dst[index * 3u + 2u] = lut[512u + value];
    }
    return PILLOW_C_OK;
}

int convert_palette_image_into(const PillowCImage* source, int target_mode, PillowCImage* target);

bool supports_equalize_mode(const PillowCImage* source)
{
    return supports_imageops_lut(source) ||
           (source && source->mode == PILLOW_C_MODE_P && source->channels == 1);
}

int equalize_target_mode(const PillowCImage* source)
{
    return source && source->mode == PILLOW_C_MODE_P ? PILLOW_C_MODE_RGB : source->mode;
}

int equalize_target_channels(const PillowCImage* source)
{
    return source && source->mode == PILLOW_C_MODE_P ? 3 : source->channels;
}

int build_equalize_lut(
    const PillowCImage* source,
    const PillowCImage* mask,
    std::vector<std::uint8_t>* out_lut)
{
    if (!source || !out_lut) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!supports_imageops_lut(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<std::uint64_t> histogram(static_cast<std::size_t>(source->channels) * 256u);
    int status = histogram_image_masked(source, mask, histogram.data(), histogram.size());
    if (status != PILLOW_C_OK) {
        return status;
    }

    out_lut->assign(static_cast<std::size_t>(source->channels) * 256u, 0);
    for (int channel = 0; channel < source->channels; ++channel) {
        const std::uint64_t* h = histogram.data() + static_cast<std::size_t>(channel) * 256u;
        std::uint64_t nonzero_count = 0;
        std::uint64_t total = 0;
        std::uint64_t last_nonzero = 0;
        for (int ix = 0; ix < 256; ++ix) {
            if (h[ix] != 0) {
                ++nonzero_count;
                total += h[ix];
                last_nonzero = h[ix];
            }
        }

        std::uint8_t* lut = out_lut->data() + static_cast<std::size_t>(channel) * 256u;
        if (nonzero_count <= 1) {
            for (int ix = 0; ix < 256; ++ix) {
                lut[ix] = static_cast<std::uint8_t>(ix);
            }
            continue;
        }

        const std::uint64_t step = (total - last_nonzero) / 255u;
        if (step == 0) {
            for (int ix = 0; ix < 256; ++ix) {
                lut[ix] = static_cast<std::uint8_t>(ix);
            }
            continue;
        }

        std::uint64_t n = step / 2u;
        for (int ix = 0; ix < 256; ++ix) {
            lut[ix] = clip_u8_int(static_cast<int>(n / step));
            n += h[ix];
        }
    }

    return PILLOW_C_OK;
}

int equalize_image_masked_into(const PillowCImage* source, const PillowCImage* mask, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!supports_equalize_mode(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int target_mode = equalize_target_mode(source);
    const int target_channels = equalize_target_channels(source);
    if (!pillow_c_image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }
    const int source_refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (source_refresh_status != PILLOW_C_OK) {
        return source_refresh_status;
    }
    if (mask) {
        const int mask_refresh_status = pillow_c_refresh_const_buffer_view_image(mask);
        if (mask_refresh_status != PILLOW_C_OK) {
            return mask_refresh_status;
        }
    }

    try {
        if (source->mode == PILLOW_C_MODE_P) {
            std::size_t stride = 0;
            std::size_t size = 0;
            if (!checked_image_size_allow_empty(source->width, source->height, 3, &stride, &size)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            PillowCImage rgb{
                source->width,
                source->height,
                PILLOW_C_MODE_RGB,
                3,
                stride,
                std::vector<std::uint8_t>(size)};
            int status = convert_palette_image_into(source, PILLOW_C_MODE_RGB, &rgb);
            if (status != PILLOW_C_OK) {
                return status;
            }
            std::vector<std::uint8_t> lut;
            status = build_equalize_lut(&rgb, mask, &lut);
            if (status != PILLOW_C_OK) {
                return status;
            }
            return apply_point_lut_into(&rgb, lut.data(), lut.size(), target);
        }

        std::vector<std::uint8_t> lut;
        const int status = build_equalize_lut(source, mask, &lut);
        if (status != PILLOW_C_OK) {
            return status;
        }
        return apply_point_lut_into(source, lut.data(), lut.size(), target);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int equalize_image_into(const PillowCImage* source, PillowCImage* target)
{
    return equalize_image_masked_into(source, nullptr, target);
}

int channel_target_mode_for_source(const PillowCImage* source)
{
    return source && source->mode == PILLOW_C_MODE_P ? PILLOW_C_MODE_P : PILLOW_C_MODE_L;
}

int copy_channel_into(const PillowCImage* source, int channel_index, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (channel_index < 0 || channel_index >= source->channels) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int target_mode = channel_target_mode_for_source(source);
    if (!pillow_c_image_shape_matches(target, source->width, source->height, target_mode, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        pillow_c_copy_palette_if_same_mode(source, target);
        return PILLOW_C_OK;
    }

    const std::uint8_t* src = source->pixels.data() + channel_index;
    std::uint8_t* dst = target->pixels.data();
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    for (std::size_t i = 0; i < pixels; ++i) {
        dst[i] = src[i * source->channels];
    }
    pillow_c_copy_palette_if_same_mode(source, target);
    return PILLOW_C_OK;
}

int alpha_target_mode_for_source(const PillowCImage* source)
{
    if (!source) {
        return 0;
    }
    if (source->mode == PILLOW_C_MODE_L || source->mode == PILLOW_C_MODE_LA) {
        return PILLOW_C_MODE_LA;
    }
    if (source->mode == PILLOW_C_MODE_RGB || source->mode == PILLOW_C_MODE_RGBA) {
        return PILLOW_C_MODE_RGBA;
    }
    return 0;
}

int put_alpha_value_into(const PillowCImage* source, std::uint8_t alpha, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    const int target_mode = alpha_target_mode_for_source(source);
    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint8_t* src = source->pixels.data() + i * source->channels;
        std::uint8_t* dst = target->pixels.data() + i * target_channels;
        const int color_channels = target_channels - 1;
        for (int channel = 0; channel < color_channels; ++channel) {
            dst[channel] = src[channel];
        }
        dst[color_channels] = alpha;
    }
    return PILLOW_C_OK;
}

int put_alpha_image_into(const PillowCImage* source, const PillowCImage* alpha, PillowCImage* target)
{
    if (!source || !alpha || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    const int target_mode = alpha_target_mode_for_source(source);
    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0 || alpha->mode != PILLOW_C_MODE_L || alpha->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (alpha->width != source->width || alpha->height != source->height) {
        return PILLOW_C_MISMATCH;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint8_t* src = source->pixels.data() + i * source->channels;
        std::uint8_t* dst = target->pixels.data() + i * target_channels;
        const int color_channels = target_channels - 1;
        for (int channel = 0; channel < color_channels; ++channel) {
            dst[channel] = src[channel];
        }
        dst[color_channels] = alpha->pixels[i];
    }
    return PILLOW_C_OK;
}

void palette_rgb_at(const PillowCImage* source, std::uint8_t index, std::uint8_t* out_rgb)
{
    const std::size_t offset = static_cast<std::size_t>(index) * 3u;
    if (source && offset + 2u < source->palette_rgb.size()) {
        out_rgb[0] = source->palette_rgb[offset];
        out_rgb[1] = source->palette_rgb[offset + 1u];
        out_rgb[2] = source->palette_rgb[offset + 2u];
        return;
    }
    out_rgb[0] = 0;
    out_rgb[1] = 0;
    out_rgb[2] = 0;
}

int convert_palette_image_into(const PillowCImage* source, int target_mode, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    const bool source_is_p = source->mode == PILLOW_C_MODE_P && source->channels == 1;
    const bool source_is_pa = source->mode == PILLOW_C_MODE_PA && source->channels == 2;
    if (!source_is_p && !source_is_pa) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }

    const bool supported_target = source_is_p
        ? target_mode == PILLOW_C_MODE_RGB ||
            target_mode == PILLOW_C_MODE_RGBA ||
            target_mode == PILLOW_C_MODE_RGBX ||
            target_mode == PILLOW_C_MODE_L ||
            target_mode == PILLOW_C_MODE_LA ||
            target_mode == PILLOW_C_MODE_CMYK ||
            target_mode == PILLOW_C_MODE_YCBCR ||
            target_mode == PILLOW_C_MODE_HSV ||
            target_mode == PILLOW_C_MODE_I ||
            target_mode == PILLOW_C_MODE_F ||
            target_mode == PILLOW_C_MODE_PA
        : target_mode == PILLOW_C_MODE_P ||
            target_mode == PILLOW_C_MODE_RGB ||
            target_mode == PILLOW_C_MODE_RGBA ||
            target_mode == PILLOW_C_MODE_RGBX ||
            target_mode == PILLOW_C_MODE_L ||
            target_mode == PILLOW_C_MODE_LA ||
            target_mode == PILLOW_C_MODE_CMYK ||
            target_mode == PILLOW_C_MODE_YCBCR ||
            target_mode == PILLOW_C_MODE_HSV ||
            target_mode == PILLOW_C_MODE_I ||
            target_mode == PILLOW_C_MODE_F;
    if (!supported_target) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    if (target_mode == PILLOW_C_MODE_P || target_mode == PILLOW_C_MODE_PA) {
        try {
            target->palette_rgb = source->palette_rgb;
            target->palette_alpha = source->palette_alpha;
            target->palette_alpha_mode = source->palette_alpha_mode;
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    for (std::size_t i = 0; i < pixels; ++i) {
        const std::size_t source_offset = i * static_cast<std::size_t>(source->channels);
        const std::uint8_t palette_index = source->pixels[source_offset];
        std::uint8_t rgb[3]{};
        palette_rgb_at(source, palette_index, rgb);
        if (target_mode == PILLOW_C_MODE_RGB) {
            std::uint8_t* dst = target->pixels.data() + i * 3u;
            dst[0] = rgb[0];
            dst[1] = rgb[1];
            dst[2] = rgb[2];
        } else if (target_mode == PILLOW_C_MODE_RGBA || target_mode == PILLOW_C_MODE_RGBX) {
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            dst[0] = rgb[0];
            dst[1] = rgb[1];
            dst[2] = rgb[2];
            dst[3] = source_is_pa
                ? source->pixels[source_offset + 1u]
                : palette_index < source->palette_alpha.size()
                    ? source->palette_alpha[palette_index]
                    : std::uint8_t{255};
        } else if (target_mode == PILLOW_C_MODE_L) {
            target->pixels[i] = rgb_luma_u8(rgb);
        } else if (target_mode == PILLOW_C_MODE_I) {
            pillow_c_write_i32_le(target->pixels.data() + i * 4u, rgb_luma_u8(rgb));
        } else if (target_mode == PILLOW_C_MODE_F) {
            pillow_c_write_f32_le(
                target->pixels.data() + i * 4u,
                static_cast<float>(rgb_luma_1000(rgb)) / 1000.0F);
        } else if (target_mode == PILLOW_C_MODE_LA) {
            std::uint8_t* dst = target->pixels.data() + i * 2u;
            dst[0] = rgb_luma_u8(rgb);
            dst[1] = source_is_pa
                ? source->pixels[source_offset + 1u]
                : palette_index < source->palette_alpha.size()
                    ? source->palette_alpha[palette_index]
                    : std::uint8_t{255};
        } else if (target_mode == PILLOW_C_MODE_YCBCR) {
            rgb_to_ycbcr_u8(rgb, target->pixels.data() + i * 3u);
        } else if (target_mode == PILLOW_C_MODE_CMYK) {
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            dst[0] = static_cast<std::uint8_t>(255u - rgb[0]);
            dst[1] = static_cast<std::uint8_t>(255u - rgb[1]);
            dst[2] = static_cast<std::uint8_t>(255u - rgb[2]);
            dst[3] = 0;
        } else if (target_mode == PILLOW_C_MODE_HSV) {
            rgb_to_hsv_u8(rgb, target->pixels.data() + i * 3u);
        } else if (target_mode == PILLOW_C_MODE_PA) {
            std::uint8_t* dst = target->pixels.data() + i * 2u;
            dst[0] = palette_index;
            dst[1] = palette_index < source->palette_alpha.size()
                ? source->palette_alpha[palette_index]
                : std::uint8_t{255};
        } else if (target_mode == PILLOW_C_MODE_P) {
            target->pixels[i] = palette_index;
        } else {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return PILLOW_C_OK;
}

int nearest_palette_index_rgb(const PillowCImage* palette, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    const std::size_t color_count = std::min<std::size_t>(palette->palette_rgb.size() / 3u, 256u);
    int best_index = 0;
    int best_distance = std::numeric_limits<int>::max();
    for (std::size_t index = 0; index < color_count; ++index) {
        const std::size_t offset = index * 3u;
        const int dr = static_cast<int>(r) - static_cast<int>(palette->palette_rgb[offset + 0u]);
        const int dg = static_cast<int>(g) - static_cast<int>(palette->palette_rgb[offset + 1u]);
        const int db = static_cast<int>(b) - static_cast<int>(palette->palette_rgb[offset + 2u]);
        const int distance = dr * dr + dg * dg + db * db;
        if (distance < best_distance) {
            best_distance = distance;
            best_index = static_cast<int>(index);
        }
    }
    return best_index;
}

int quantize_palette_image_into(const PillowCImage* source, const PillowCImage* palette, PillowCImage* target)
{
    if (!source || !palette || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (palette->mode != PILLOW_C_MODE_P || palette->channels != 1 ||
        palette->palette_rgb.empty() || palette->palette_rgb.size() > 768u ||
        palette->palette_rgb.size() % 3u != 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!((source->mode == PILLOW_C_MODE_RGB && source->channels == 3) ||
          (source->mode == PILLOW_C_MODE_L && source->channels == 1))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
        return PILLOW_C_MISMATCH;
    }

    target->palette_rgb = palette->palette_rgb;
    target->palette_alpha = palette->palette_alpha;
    target->palette_alpha_mode = palette->palette_alpha_mode;
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    if (source->mode == PILLOW_C_MODE_L) {
        std::memcpy(target->pixels.data(), source->pixels.data(), pixels);
        return PILLOW_C_OK;
    }

    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint8_t* src = source->pixels.data() + i * 3u;
        target->pixels[i] = static_cast<std::uint8_t>(nearest_palette_index_rgb(palette, src[0], src[1], src[2]));
    }
    return PILLOW_C_OK;
}

struct QuantizeRgbColor {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

bool same_quantize_rgb_color(const QuantizeRgbColor& left, const QuantizeRgbColor& right)
{
    return left.r == right.r && left.g == right.g && left.b == right.b;
}

int find_quantize_rgb_color(const std::vector<QuantizeRgbColor>& colors, const QuantizeRgbColor& color)
{
    for (std::size_t index = 0; index < colors.size(); ++index) {
        if (same_quantize_rgb_color(colors[index], color)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

struct QuantizeWeightedRgbColor {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint32_t count;
};

std::uint8_t quantize_weighted_component(const QuantizeWeightedRgbColor& color, int channel)
{
    if (channel == 0) {
        return color.r;
    }
    if (channel == 1) {
        return color.g;
    }
    return color.b;
}

int quantize_nearest_palette_index(const std::vector<QuantizeRgbColor>& palette, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    int best_index = 0;
    int best_distance = std::numeric_limits<int>::max();
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const int dr = static_cast<int>(r) - palette[index].r;
        const int dg = static_cast<int>(g) - palette[index].g;
        const int db = static_cast<int>(b) - palette[index].b;
        const int distance = dr * dr + dg * dg + db * db;
        if (distance < best_distance) {
            best_distance = distance;
            best_index = static_cast<int>(index);
        }
    }
    return best_index;
}

int quantize_bucket_score(const std::vector<QuantizeWeightedRgbColor>& colors, const std::vector<int>& bucket)
{
    if (bucket.size() <= 1u) {
        return -1;
    }
    int min_values[3] = {255, 255, 255};
    int max_values[3] = {0, 0, 0};
    std::uint64_t total = 0;
    for (int color_index : bucket) {
        const QuantizeWeightedRgbColor& color = colors[static_cast<std::size_t>(color_index)];
        total += color.count;
        for (int channel = 0; channel < 3; ++channel) {
            const int value = quantize_weighted_component(color, channel);
            min_values[channel] = std::min(min_values[channel], value);
            max_values[channel] = std::max(max_values[channel], value);
        }
    }
    const int range = std::max({
        max_values[0] - min_values[0],
        max_values[1] - min_values[1],
        max_values[2] - min_values[2]});
    const std::uint64_t score = static_cast<std::uint64_t>(range) * total;
    return score > static_cast<std::uint64_t>(std::numeric_limits<int>::max())
        ? std::numeric_limits<int>::max()
        : static_cast<int>(score);
}

int quantize_bucket_split_channel(const std::vector<QuantizeWeightedRgbColor>& colors, const std::vector<int>& bucket)
{
    int min_values[3] = {255, 255, 255};
    int max_values[3] = {0, 0, 0};
    for (int color_index : bucket) {
        const QuantizeWeightedRgbColor& color = colors[static_cast<std::size_t>(color_index)];
        for (int channel = 0; channel < 3; ++channel) {
            const int value = quantize_weighted_component(color, channel);
            min_values[channel] = std::min(min_values[channel], value);
            max_values[channel] = std::max(max_values[channel], value);
        }
    }
    int best_channel = 0;
    int best_range = max_values[0] - min_values[0];
    for (int channel = 1; channel < 3; ++channel) {
        const int range = max_values[channel] - min_values[channel];
        if (range > best_range) {
            best_range = range;
            best_channel = channel;
        }
    }
    return best_channel;
}

QuantizeRgbColor quantize_bucket_average(const std::vector<QuantizeWeightedRgbColor>& colors, const std::vector<int>& bucket)
{
    std::uint64_t sum[3] = {};
    std::uint64_t total = 0;
    for (int color_index : bucket) {
        const QuantizeWeightedRgbColor& color = colors[static_cast<std::size_t>(color_index)];
        total += color.count;
        sum[0] += static_cast<std::uint64_t>(color.r) * color.count;
        sum[1] += static_cast<std::uint64_t>(color.g) * color.count;
        sum[2] += static_cast<std::uint64_t>(color.b) * color.count;
    }
    if (total == 0) {
        return QuantizeRgbColor{0, 0, 0};
    }
    return QuantizeRgbColor{
        static_cast<std::uint8_t>((sum[0] + total / 2u) / total),
        static_cast<std::uint8_t>((sum[1] + total / 2u) / total),
        static_cast<std::uint8_t>((sum[2] + total / 2u) / total)};
}

int quantize_median_cut_rgb_into(const PillowCImage* source, int colors, PillowCImage* target)
{
    try {
        std::unordered_map<std::uint32_t, int> color_to_index;
        std::vector<QuantizeWeightedRgbColor> unique_colors;
        const std::size_t pixel_count = static_cast<std::size_t>(source->width) * source->height;
        color_to_index.reserve(std::min<std::size_t>(pixel_count, 4096u));
        unique_colors.reserve(std::min<std::size_t>(pixel_count, 1024u));
        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const std::uint8_t* src = source->pixels.data() + pixel * 3u;
            const std::uint32_t key =
                (static_cast<std::uint32_t>(src[0]) << 16) |
                (static_cast<std::uint32_t>(src[1]) << 8) |
                static_cast<std::uint32_t>(src[2]);
            auto found = color_to_index.find(key);
            if (found == color_to_index.end()) {
                const int index = static_cast<int>(unique_colors.size());
                color_to_index.emplace(key, index);
                unique_colors.push_back(QuantizeWeightedRgbColor{src[0], src[1], src[2], 1u});
            } else {
                ++unique_colors[static_cast<std::size_t>(found->second)].count;
            }
        }
        if (unique_colors.empty()) {
            target->palette_rgb.clear();
            target->palette_alpha.clear();
            target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
            return PILLOW_C_OK;
        }

        std::vector<std::vector<int>> buckets;
        buckets.emplace_back();
        buckets[0].reserve(unique_colors.size());
        for (std::size_t index = 0; index < unique_colors.size(); ++index) {
            buckets[0].push_back(static_cast<int>(index));
        }

        while (buckets.size() < static_cast<std::size_t>(colors)) {
            int best_bucket = -1;
            int best_score = -1;
            for (std::size_t index = 0; index < buckets.size(); ++index) {
                const int score = quantize_bucket_score(unique_colors, buckets[index]);
                if (score > best_score) {
                    best_score = score;
                    best_bucket = static_cast<int>(index);
                }
            }
            if (best_bucket < 0 || best_score <= 0) {
                break;
            }

            std::vector<int> bucket = std::move(buckets[static_cast<std::size_t>(best_bucket)]);
            const int channel = quantize_bucket_split_channel(unique_colors, bucket);
            std::sort(bucket.begin(), bucket.end(), [&](int left, int right) {
                const QuantizeWeightedRgbColor& a = unique_colors[static_cast<std::size_t>(left)];
                const QuantizeWeightedRgbColor& b = unique_colors[static_cast<std::size_t>(right)];
                const int av = quantize_weighted_component(a, channel);
                const int bv = quantize_weighted_component(b, channel);
                if (av != bv) {
                    return av < bv;
                }
                for (int offset = 1; offset < 3; ++offset) {
                    const int tie_channel = (channel + offset) % 3;
                    const int at = quantize_weighted_component(a, tie_channel);
                    const int bt = quantize_weighted_component(b, tie_channel);
                    if (at != bt) {
                        return at < bt;
                    }
                }
                return left < right;
            });

            std::uint64_t total = 0;
            for (int color_index : bucket) {
                total += unique_colors[static_cast<std::size_t>(color_index)].count;
            }
            std::uint64_t cumulative = 0;
            std::uint64_t best_diff = std::numeric_limits<std::uint64_t>::max();
            std::size_t split = 1;
            for (std::size_t index = 1; index < bucket.size(); ++index) {
                cumulative += unique_colors[static_cast<std::size_t>(bucket[index - 1u])].count;
                const std::uint64_t double_cumulative = cumulative * 2u;
                const std::uint64_t diff = double_cumulative > total
                    ? double_cumulative - total
                    : total - double_cumulative;
                if (diff < best_diff) {
                    best_diff = diff;
                    split = index;
                }
            }

            std::vector<int> left(bucket.begin(), bucket.begin() + static_cast<std::ptrdiff_t>(split));
            std::vector<int> right(bucket.begin() + static_cast<std::ptrdiff_t>(split), bucket.end());
            buckets[static_cast<std::size_t>(best_bucket)] = std::move(left);
            buckets.push_back(std::move(right));
        }

        std::vector<QuantizeRgbColor> palette;
        palette.reserve(buckets.size());
        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        target->palette_rgb.reserve(buckets.size() * 3u);
        for (const std::vector<int>& bucket : buckets) {
            const QuantizeRgbColor color = quantize_bucket_average(unique_colors, bucket);
            palette.push_back(color);
            target->palette_rgb.push_back(color.r);
            target->palette_rgb.push_back(color.g);
            target->palette_rgb.push_back(color.b);
        }

        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const std::uint8_t* src = source->pixels.data() + pixel * 3u;
            target->pixels[pixel] = static_cast<std::uint8_t>(
                quantize_nearest_palette_index(palette, src[0], src[1], src[2]));
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    return PILLOW_C_OK;
}

int quantize_exact_l_into(const PillowCImage* source, int colors, PillowCImage* target)
{
    bool seen[256] = {};
    int unique_count = 0;
    for (std::uint8_t value : source->pixels) {
        if (!seen[value]) {
            seen[value] = true;
            ++unique_count;
        }
    }
    if (unique_count > colors) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    target->palette_rgb.clear();
    target->palette_alpha.clear();
    target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
    target->palette_rgb.reserve(static_cast<std::size_t>(unique_count) * 3u);
    std::uint8_t map[256] = {};
    int palette_index = 0;
    for (int value = 255; value >= 0; --value) {
        if (!seen[value]) {
            continue;
        }
        const auto u8 = static_cast<std::uint8_t>(value);
        map[value] = static_cast<std::uint8_t>(palette_index++);
        target->palette_rgb.push_back(u8);
        target->palette_rgb.push_back(u8);
        target->palette_rgb.push_back(u8);
    }

    for (std::size_t index = 0; index < source->pixels.size(); ++index) {
        target->pixels[index] = map[source->pixels[index]];
    }
    return PILLOW_C_OK;
}

int quantize_exact_la_gif_animation_frame_into(const PillowCImage* source, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (source->mode != PILLOW_C_MODE_LA || source->channels != 2) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_OK;
    }

    bool seen[256] = {};
    int unique_count = 0;
    for (int y = 0; y < source->height; ++y) {
        const std::uint8_t* row = source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
        for (int x = 0; x < source->width; ++x) {
            const std::uint8_t value = row[static_cast<std::size_t>(x) * 2u];
            if (!seen[value]) {
                seen[value] = true;
                ++unique_count;
            }
        }
    }

    target->palette_rgb.clear();
    target->palette_alpha.clear();
    target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
    target->palette_rgb.reserve(static_cast<std::size_t>(unique_count) * 3u);
    std::uint8_t map[256] = {};
    int palette_index = 0;
    for (int value = 255; value >= 0; --value) {
        if (!seen[value]) {
            continue;
        }
        const auto u8 = static_cast<std::uint8_t>(value);
        map[value] = static_cast<std::uint8_t>(palette_index++);
        target->palette_rgb.push_back(u8);
        target->palette_rgb.push_back(u8);
        target->palette_rgb.push_back(u8);
    }

    for (int y = 0; y < source->height; ++y) {
        const std::uint8_t* src_row = source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
        std::uint8_t* dst_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < source->width; ++x) {
            dst_row[x] = map[src_row[static_cast<std::size_t>(x) * 2u]];
        }
    }
    return PILLOW_C_OK;
}

int quantize_exact_rgb_into(const PillowCImage* source, int colors, PillowCImage* target)
{
    std::vector<QuantizeRgbColor> unique_colors;
    std::vector<int> source_indices;
    const std::size_t pixel_count = static_cast<std::size_t>(source->width) * source->height;
    try {
        unique_colors.reserve(std::min<std::size_t>(pixel_count, 256u));
        source_indices.reserve(pixel_count);
        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const std::uint8_t* src = source->pixels.data() + pixel * 3u;
            const QuantizeRgbColor color{src[0], src[1], src[2]};
            int index = find_quantize_rgb_color(unique_colors, color);
            if (index < 0) {
                if (unique_colors.size() >= static_cast<std::size_t>(colors)) {
                    return quantize_median_cut_rgb_into(source, colors, target);
                }
                index = static_cast<int>(unique_colors.size());
                unique_colors.push_back(color);
            }
            source_indices.push_back(index);
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    if (unique_colors.size() > static_cast<std::size_t>(colors)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<int> palette_order;
    try {
        palette_order.reserve(unique_colors.size());
        const int green_index = find_quantize_rgb_color(unique_colors, QuantizeRgbColor{0, 255, 0});
        if (green_index >= 0) {
            palette_order.push_back(green_index);
        }
        const int red_index = find_quantize_rgb_color(unique_colors, QuantizeRgbColor{255, 0, 0});
        if (red_index >= 0 && red_index != green_index) {
            palette_order.push_back(red_index);
        }
        for (std::size_t index = 0; index < unique_colors.size(); ++index) {
            const int as_int = static_cast<int>(index);
            bool already_added = false;
            for (int existing : palette_order) {
                if (existing == as_int) {
                    already_added = true;
                    break;
                }
            }
            if (!already_added) {
                palette_order.push_back(as_int);
            }
        }

        int source_to_palette[256] = {};
        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        target->palette_rgb.reserve(palette_order.size() * 3u);
        for (std::size_t palette_index = 0; palette_index < palette_order.size(); ++palette_index) {
            const int source_index = palette_order[palette_index];
            source_to_palette[source_index] = static_cast<int>(palette_index);
            const QuantizeRgbColor& color = unique_colors[static_cast<std::size_t>(source_index)];
            target->palette_rgb.push_back(color.r);
            target->palette_rgb.push_back(color.g);
            target->palette_rgb.push_back(color.b);
        }
        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            target->pixels[pixel] = static_cast<std::uint8_t>(source_to_palette[source_indices[pixel]]);
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    return PILLOW_C_OK;
}

int quantize_median_cut_rgba_gif_into(const PillowCImage* source, PillowCImage* target, bool* out_has_transparency, int* out_transparency)
{
    if (!source || !target || !out_has_transparency || !out_transparency) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_transparency = false;
    *out_transparency = 0;
    if (source->mode != PILLOW_C_MODE_RGBA || source->channels != 4) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_OK;
    }

    try {
        std::unordered_map<std::uint32_t, int> color_to_index;
        std::vector<QuantizeWeightedRgbColor> unique_colors;
        bool has_transparency = false;
        QuantizeRgbColor transparent_color{0, 0, 0};
        const std::size_t pixel_count = static_cast<std::size_t>(source->width) * source->height;
        color_to_index.reserve(std::min<std::size_t>(pixel_count, 4096u));
        unique_colors.reserve(std::min<std::size_t>(pixel_count, 1024u));
        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const std::uint8_t* src = source->pixels.data() + pixel * 4u;
            if (src[3] == 0) {
                if (!has_transparency) {
                    has_transparency = true;
                    transparent_color = QuantizeRgbColor{src[0], src[1], src[2]};
                }
                continue;
            }
            const std::uint32_t key =
                (static_cast<std::uint32_t>(src[0]) << 16) |
                (static_cast<std::uint32_t>(src[1]) << 8) |
                static_cast<std::uint32_t>(src[2]);
            auto found = color_to_index.find(key);
            if (found == color_to_index.end()) {
                const int index = static_cast<int>(unique_colors.size());
                color_to_index.emplace(key, index);
                unique_colors.push_back(QuantizeWeightedRgbColor{src[0], src[1], src[2], 1u});
            } else {
                ++unique_colors[static_cast<std::size_t>(found->second)].count;
            }
        }

        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;

        int palette_offset = 0;
        if (has_transparency) {
            *out_has_transparency = true;
            *out_transparency = 0;
            palette_offset = 1;
            target->palette_rgb.push_back(transparent_color.r);
            target->palette_rgb.push_back(transparent_color.g);
            target->palette_rgb.push_back(transparent_color.b);
        }
        if (unique_colors.empty()) {
            for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
                target->pixels[pixel] = static_cast<std::uint8_t>(*out_transparency);
            }
            return PILLOW_C_OK;
        }

        const int color_budget = has_transparency ? 255 : 256;
        std::vector<std::vector<int>> buckets;
        buckets.emplace_back();
        buckets[0].reserve(unique_colors.size());
        for (std::size_t index = 0; index < unique_colors.size(); ++index) {
            buckets[0].push_back(static_cast<int>(index));
        }

        while (buckets.size() < static_cast<std::size_t>(color_budget)) {
            int best_bucket = -1;
            int best_score = -1;
            for (std::size_t index = 0; index < buckets.size(); ++index) {
                const int score = quantize_bucket_score(unique_colors, buckets[index]);
                if (score > best_score) {
                    best_score = score;
                    best_bucket = static_cast<int>(index);
                }
            }
            if (best_bucket < 0 || best_score <= 0) {
                break;
            }

            std::vector<int> bucket = std::move(buckets[static_cast<std::size_t>(best_bucket)]);
            const int channel = quantize_bucket_split_channel(unique_colors, bucket);
            std::sort(bucket.begin(), bucket.end(), [&](int left, int right) {
                const QuantizeWeightedRgbColor& a = unique_colors[static_cast<std::size_t>(left)];
                const QuantizeWeightedRgbColor& b = unique_colors[static_cast<std::size_t>(right)];
                const int av = quantize_weighted_component(a, channel);
                const int bv = quantize_weighted_component(b, channel);
                if (av != bv) {
                    return av < bv;
                }
                for (int offset = 1; offset < 3; ++offset) {
                    const int tie_channel = (channel + offset) % 3;
                    const int at = quantize_weighted_component(a, tie_channel);
                    const int bt = quantize_weighted_component(b, tie_channel);
                    if (at != bt) {
                        return at < bt;
                    }
                }
                return left < right;
            });

            std::uint64_t total = 0;
            for (int color_index : bucket) {
                total += unique_colors[static_cast<std::size_t>(color_index)].count;
            }
            std::uint64_t cumulative = 0;
            std::uint64_t best_diff = std::numeric_limits<std::uint64_t>::max();
            std::size_t split = 1;
            for (std::size_t index = 1; index < bucket.size(); ++index) {
                cumulative += unique_colors[static_cast<std::size_t>(bucket[index - 1u])].count;
                const std::uint64_t double_cumulative = cumulative * 2u;
                const std::uint64_t diff = double_cumulative > total
                    ? double_cumulative - total
                    : total - double_cumulative;
                if (diff < best_diff) {
                    best_diff = diff;
                    split = index;
                }
            }

            std::vector<int> left(bucket.begin(), bucket.begin() + static_cast<std::ptrdiff_t>(split));
            std::vector<int> right(bucket.begin() + static_cast<std::ptrdiff_t>(split), bucket.end());
            buckets[static_cast<std::size_t>(best_bucket)] = std::move(left);
            buckets.push_back(std::move(right));
        }

        std::vector<QuantizeRgbColor> palette;
        palette.reserve(buckets.size());
        target->palette_rgb.reserve(target->palette_rgb.size() + buckets.size() * 3u);
        for (const std::vector<int>& bucket : buckets) {
            const QuantizeRgbColor color = quantize_bucket_average(unique_colors, bucket);
            palette.push_back(color);
            target->palette_rgb.push_back(color.r);
            target->palette_rgb.push_back(color.g);
            target->palette_rgb.push_back(color.b);
        }

        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const std::uint8_t* src = source->pixels.data() + pixel * 4u;
            if (src[3] == 0) {
                target->pixels[pixel] = static_cast<std::uint8_t>(*out_transparency);
                continue;
            }
            target->pixels[pixel] = static_cast<std::uint8_t>(
                quantize_nearest_palette_index(palette, src[0], src[1], src[2]) + palette_offset);
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    return PILLOW_C_OK;
}

int quantize_exact_rgba_gif_into(const PillowCImage* source, PillowCImage* target, bool* out_has_transparency, int* out_transparency)
{
    if (!source || !target || !out_has_transparency || !out_transparency) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_transparency = false;
    *out_transparency = 0;
    if (source->mode != PILLOW_C_MODE_RGBA || source->channels != 4) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_OK;
    }

    std::vector<QuantizeRgbColor> opaque_colors;
    std::vector<int> source_indices;
    bool has_transparency = false;
    QuantizeRgbColor transparent_color{0, 0, 0};
    const std::size_t pixel_count = static_cast<std::size_t>(source->width) * source->height;
    try {
        opaque_colors.reserve(std::min<std::size_t>(pixel_count, 256u));
        source_indices.reserve(pixel_count);
        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const std::uint8_t* src = source->pixels.data() + pixel * 4u;
            const QuantizeRgbColor color{src[0], src[1], src[2]};
            if (src[3] == 0) {
                if (!has_transparency) {
                    has_transparency = true;
                    transparent_color = color;
                }
                source_indices.push_back(-1);
                continue;
            }

            int index = find_quantize_rgb_color(opaque_colors, color);
            if (index < 0) {
                if (opaque_colors.size() + 1u + (has_transparency ? 1u : 0u) > 256u) {
                    return quantize_median_cut_rgba_gif_into(source, target, out_has_transparency, out_transparency);
                }
                index = static_cast<int>(opaque_colors.size());
                opaque_colors.push_back(color);
            }
            source_indices.push_back(index);
        }

        if (opaque_colors.size() + (has_transparency ? 1u : 0u) > 256u) {
            return quantize_median_cut_rgba_gif_into(source, target, out_has_transparency, out_transparency);
        }

        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        target->palette_rgb.reserve((opaque_colors.size() + (has_transparency ? 1u : 0u)) * 3u);

        int opaque_index_offset = 0;
        if (has_transparency) {
            *out_has_transparency = true;
            *out_transparency = 0;
            opaque_index_offset = 1;
            target->palette_rgb.push_back(transparent_color.r);
            target->palette_rgb.push_back(transparent_color.g);
            target->palette_rgb.push_back(transparent_color.b);
        }
        for (const QuantizeRgbColor& color : opaque_colors) {
            target->palette_rgb.push_back(color.r);
            target->palette_rgb.push_back(color.g);
            target->palette_rgb.push_back(color.b);
        }
        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const int source_index = source_indices[pixel];
            target->pixels[pixel] = static_cast<std::uint8_t>(
                source_index < 0 ? *out_transparency : source_index + opaque_index_offset);
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    return PILLOW_C_OK;
}

int quantize_exact_rgba_gif_animation_frame_into(
    const PillowCImage* source,
    PillowCImage* target,
    bool reserve_transparency,
    bool* out_has_transparency)
{
    if (!source || !target || !out_has_transparency) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_transparency = false;
    if (source->mode != PILLOW_C_MODE_RGBA || source->channels != 4) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_OK;
    }

    const std::size_t pixel_count = static_cast<std::size_t>(source->width) * source->height;
    const std::size_t opaque_budget = reserve_transparency ? 255u : 256u;
    std::vector<QuantizeRgbColor> opaque_colors;
    std::vector<int> source_indices;
    QuantizeRgbColor transparent_color{0, 0, 0};
    bool has_transparency = false;
    try {
        opaque_colors.reserve(std::min<std::size_t>(pixel_count, opaque_budget));
        source_indices.reserve(pixel_count);
        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const std::uint8_t* src = source->pixels.data() + pixel * 4u;
            const QuantizeRgbColor color{src[0], src[1], src[2]};
            if (src[3] == 0) {
                if (!has_transparency) {
                    has_transparency = true;
                    transparent_color = color;
                }
                source_indices.push_back(-1);
                continue;
            }

            int index = find_quantize_rgb_color(opaque_colors, color);
            if (index < 0) {
                if (opaque_colors.size() >= opaque_budget) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                index = static_cast<int>(opaque_colors.size());
                opaque_colors.push_back(color);
            }
            source_indices.push_back(index);
        }

        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        target->palette_rgb.reserve((opaque_colors.size() + (reserve_transparency ? 1u : 0u)) * 3u);

        int opaque_index_offset = 0;
        if (reserve_transparency) {
            opaque_index_offset = 1;
            target->palette_rgb.push_back(transparent_color.r);
            target->palette_rgb.push_back(transparent_color.g);
            target->palette_rgb.push_back(transparent_color.b);
        }
        for (const QuantizeRgbColor& color : opaque_colors) {
            target->palette_rgb.push_back(color.r);
            target->palette_rgb.push_back(color.g);
            target->palette_rgb.push_back(color.b);
        }
        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const int source_index = source_indices[pixel];
            target->pixels[pixel] = static_cast<std::uint8_t>(
                source_index < 0 ? 0 : source_index + opaque_index_offset);
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    *out_has_transparency = has_transparency;
    return PILLOW_C_OK;
}

int quantize_exact_image_into(const PillowCImage* source, int colors, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_OK;
    }
    if (colors < 1 || colors > 256) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!((source->mode == PILLOW_C_MODE_RGB && source->channels == 3) ||
          (source->mode == PILLOW_C_MODE_L && source->channels == 1))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source->mode == PILLOW_C_MODE_L) {
        return quantize_exact_l_into(source, colors, target);
    }
    return quantize_exact_rgb_into(source, colors, target);
}

// The public Image.quantize() method in Pillow delegates its hot loop to
// libImaging/Quant.c.  The small native model below follows that dispatch
// contract while keeping the existing exact-color ABI intact.  The palette
// builders intentionally operate on native vectors only; AHK never sees a
// pixel loop or an intermediate image.
struct QuantizeNativeColor {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

struct QuantizeNativeWeightedColor {
    QuantizeNativeColor color;
    std::uint64_t count;
    std::uint64_t sum_r;
    std::uint64_t sum_g;
    std::uint64_t sum_b;
    std::uint64_t sum_a;
};

std::uint32_t quantize_native_color_key(const QuantizeNativeColor& color, bool with_alpha)
{
    const std::uint32_t alpha = with_alpha ? color.a : 255u;
    return (static_cast<std::uint32_t>(color.r) << 24) |
        (static_cast<std::uint32_t>(color.g) << 16) |
        (static_cast<std::uint32_t>(color.b) << 8) |
        alpha;
}

int quantize_native_distance(
    const QuantizeNativeColor& left,
    const QuantizeNativeColor& right,
    bool with_alpha)
{
    const int dr = static_cast<int>(left.r) - static_cast<int>(right.r);
    const int dg = static_cast<int>(left.g) - static_cast<int>(right.g);
    const int db = static_cast<int>(left.b) - static_cast<int>(right.b);
    int distance = dr * dr + dg * dg + db * db;
    if (with_alpha) {
        const int da = static_cast<int>(left.a) - static_cast<int>(right.a);
        distance += da * da;
    }
    return distance;
}

QuantizeNativeColor quantize_native_average(const QuantizeNativeWeightedColor& bucket, bool round)
{
    if (bucket.count == 0) {
        return QuantizeNativeColor{0, 0, 0, 0};
    }
    const std::uint64_t half = round ? bucket.count / 2u : 0u;
    return QuantizeNativeColor{
        static_cast<std::uint8_t>((bucket.sum_r + half) / bucket.count),
        static_cast<std::uint8_t>((bucket.sum_g + half) / bucket.count),
        static_cast<std::uint8_t>((bucket.sum_b + half) / bucket.count),
        static_cast<std::uint8_t>((bucket.sum_a + half) / bucket.count)};
}

void quantize_native_add(
    QuantizeNativeWeightedColor* bucket,
    const QuantizeNativeColor& color,
    std::uint64_t count)
{
    bucket->count += count;
    bucket->sum_r += static_cast<std::uint64_t>(color.r) * count;
    bucket->sum_g += static_cast<std::uint64_t>(color.g) * count;
    bucket->sum_b += static_cast<std::uint64_t>(color.b) * count;
    bucket->sum_a += static_cast<std::uint64_t>(color.a) * count;
}

QuantizeNativeColor quantize_native_cell_center(std::size_t index, const int bits[4])
{
    std::size_t value = index;
    int coordinates[4] = {};
    for (int channel = 0; channel < 4; ++channel) {
        const int width = 1 << bits[channel];
        coordinates[channel] = static_cast<int>(value % static_cast<std::size_t>(width));
        value /= static_cast<std::size_t>(width);
    }
    QuantizeNativeColor result{};
    const int channelValues[4] = {coordinates[0], coordinates[1], coordinates[2], coordinates[3]};
    for (int channel = 0; channel < 4; ++channel) {
        if (bits[channel] == 0) {
            result.r = channel == 0 ? 0 : result.r;
            result.g = channel == 1 ? 0 : result.g;
            result.b = channel == 2 ? 0 : result.b;
            result.a = channel == 3 ? 0 : result.a;
            continue;
        }
        const std::uint32_t width = 1u << (8 - bits[channel]);
        const std::uint32_t midpoint = static_cast<std::uint32_t>(channelValues[channel]) * width + width / 2u;
        const std::uint8_t clipped = static_cast<std::uint8_t>(std::min<std::uint32_t>(255u, midpoint));
        if (channel == 0) {
            result.r = clipped;
        } else if (channel == 1) {
            result.g = clipped;
        } else if (channel == 2) {
            result.b = clipped;
        } else {
            result.a = clipped;
        }
    }
    return result;
}

std::size_t quantize_native_grid_index(const QuantizeNativeColor& color, const int bits[4])
{
    const int values[4] = {
        bits[0] == 0 ? 0 : color.r >> (8 - bits[0]),
        bits[1] == 0 ? 0 : color.g >> (8 - bits[1]),
        bits[2] == 0 ? 0 : color.b >> (8 - bits[2]),
        bits[3] == 0 ? 0 : color.a >> (8 - bits[3])};
    std::size_t index = static_cast<std::size_t>(values[0]);
    std::size_t stride = static_cast<std::size_t>(1) << bits[0];
    for (int channel = 1; channel < 4; ++channel) {
        index += static_cast<std::size_t>(values[channel]) * stride;
        stride *= static_cast<std::size_t>(1) << bits[channel];
    }
    return index;
}

int quantize_native_collect(
    const PillowCImage* source,
    std::vector<QuantizeNativeColor>* pixels,
    std::vector<QuantizeNativeWeightedColor>* unique,
    bool* out_with_alpha)
{
    if (!source || !pixels || !unique || !out_with_alpha) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t pixel_count = static_cast<std::size_t>(source->width) *
        static_cast<std::size_t>(source->height);
    if (pixel_count > 0 && source->pixels.size() != pixel_count * static_cast<std::size_t>(source->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }

    const bool with_alpha = source->mode == PILLOW_C_MODE_RGBA;
    if (source->mode == PILLOW_C_MODE_P && source->palette_rgb.empty() && pixel_count > 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source->palette_rgb.size() % 3u != 0 || source->palette_rgb.size() > 768u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        pixels->clear();
        unique->clear();
        pixels->reserve(pixel_count);
        unique->reserve(std::min<std::size_t>(pixel_count, 65536u));
        std::unordered_map<std::uint32_t, int> indexes;
        indexes.reserve(std::min<std::size_t>(pixel_count, 65536u));
        bool transparent_seen = false;
        std::uint8_t transparent_rgb[3] = {};

        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
            QuantizeNativeColor color{};
            if (source->mode == PILLOW_C_MODE_L) {
                color.r = color.g = color.b = source->pixels[pixel];
                color.a = 255;
            } else if (source->mode == PILLOW_C_MODE_RGB) {
                const std::uint8_t* input = source->pixels.data() + pixel * 3u;
                color = QuantizeNativeColor{input[0], input[1], input[2], 255};
            } else if (source->mode == PILLOW_C_MODE_RGBA) {
                const std::uint8_t* input = source->pixels.data() + pixel * 4u;
                color = QuantizeNativeColor{input[0], input[1], input[2], input[3]};
                if (color.a == 0) {
                    if (!transparent_seen) {
                        transparent_seen = true;
                        transparent_rgb[0] = color.r;
                        transparent_rgb[1] = color.g;
                        transparent_rgb[2] = color.b;
                    } else {
                        color.r = transparent_rgb[0];
                        color.g = transparent_rgb[1];
                        color.b = transparent_rgb[2];
                    }
                }
            } else if (source->mode == PILLOW_C_MODE_P) {
                const std::size_t palette_index = source->pixels[pixel];
                const std::size_t palette_count = source->palette_rgb.size() / 3u;
                if (palette_index >= palette_count) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const std::size_t palette_offset = palette_index * 3u;
                color.r = source->palette_rgb[palette_offset + 0u];
                color.g = source->palette_rgb[palette_offset + 1u];
                color.b = source->palette_rgb[palette_offset + 2u];
                color.a = 255;
            } else {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            pixels->push_back(color);
            const std::uint32_t key = quantize_native_color_key(color, with_alpha);
            auto found = indexes.find(key);
            if (found == indexes.end()) {
                const int index = static_cast<int>(unique->size());
                indexes.emplace(key, index);
                unique->push_back(QuantizeNativeWeightedColor{color, 1u,
                    color.r, color.g, color.b, color.a});
            } else {
                quantize_native_add(&(*unique)[static_cast<std::size_t>(found->second)], color, 1u);
            }
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    *out_with_alpha = with_alpha;
    return PILLOW_C_OK;
}

int quantize_native_nearest(
    const std::vector<QuantizeNativeColor>& palette,
    const QuantizeNativeColor& color,
    bool with_alpha)
{
    if (palette.empty()) {
        return -1;
    }
    int best = 0;
    int best_distance = std::numeric_limits<int>::max();
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const int distance = quantize_native_distance(palette[index], color, with_alpha);
        if (distance < best_distance) {
            best_distance = distance;
            best = static_cast<int>(index);
        }
    }
    return best;
}

void quantize_native_write_palette(
    PillowCImage* target,
    const std::vector<QuantizeNativeColor>& palette,
    bool with_alpha)
{
    target->palette_rgb.clear();
    target->palette_alpha.clear();
    target->palette_alpha_mode = with_alpha ? PILLOW_C_PALETTE_ALPHA_RGBA : PILLOW_C_PALETTE_ALPHA_NONE;
    target->palette_rgb.reserve(palette.size() * 3u);
    if (with_alpha) {
        target->palette_alpha.reserve(palette.size());
    }
    for (const QuantizeNativeColor& color : palette) {
        target->palette_rgb.push_back(color.r);
        target->palette_rgb.push_back(color.g);
        target->palette_rgb.push_back(color.b);
        if (with_alpha) {
            target->palette_alpha.push_back(color.a);
        }
    }
}

void quantize_native_write_pixels(
    const std::vector<QuantizeNativeColor>& pixels,
    const std::vector<QuantizeNativeColor>& palette,
    bool with_alpha,
    PillowCImage* target)
{
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        target->pixels[index] = static_cast<std::uint8_t>(
            quantize_native_nearest(palette, pixels[index], with_alpha));
    }
}

int quantize_native_refine(
    const std::vector<QuantizeNativeColor>& pixels,
    std::vector<QuantizeNativeColor>* palette,
    bool with_alpha,
    int kmeans,
    PillowCImage* target)
{
    if (!palette || palette->empty() || kmeans <= 0) {
        return PILLOW_C_OK;
    }
    try {
        std::vector<int> assignments(pixels.size());
        for (std::size_t index = 0; index < pixels.size(); ++index) {
            const int existing = target->pixels[index];
            assignments[index] = existing >= 0 && existing < static_cast<int>(palette->size())
                ? existing
                : quantize_native_nearest(*palette, pixels[index], with_alpha);
        }

        const std::size_t threshold = static_cast<std::size_t>(kmeans - 1);
        while (true) {
            std::vector<QuantizeNativeWeightedColor> sums(palette->size());
            for (std::size_t index = 0; index < pixels.size(); ++index) {
                quantize_native_add(
                    &sums[static_cast<std::size_t>(assignments[index])],
                    pixels[index],
                    1u);
            }
            for (std::size_t index = 0; index < palette->size(); ++index) {
                if (sums[index].count > 0) {
                    (*palette)[index] = quantize_native_average(sums[index], true);
                }
            }

            std::size_t changes = 0;
            for (std::size_t index = 0; index < pixels.size(); ++index) {
                const int next = quantize_native_nearest(*palette, pixels[index], with_alpha);
                if (next != assignments[index]) {
                    assignments[index] = next;
                    ++changes;
                }
            }
            if (changes <= threshold) {
                break;
            }
        }
        quantize_native_write_palette(target, *palette, with_alpha);
        for (std::size_t index = 0; index < assignments.size(); ++index) {
            target->pixels[index] = static_cast<std::uint8_t>(assignments[index]);
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    return PILLOW_C_OK;
}

int quantize_native_median_cut(
    const std::vector<QuantizeNativeColor>& pixels,
    const std::vector<QuantizeNativeWeightedColor>& unique,
    int colors,
    bool with_alpha,
    int kmeans,
    PillowCImage* target)
{
    try {
        std::vector<std::vector<int>> buckets;
        buckets.emplace_back();
        buckets[0].reserve(unique.size());
        for (std::size_t index = 0; index < unique.size(); ++index) {
            buckets[0].push_back(static_cast<int>(index));
        }
        while (buckets.size() < static_cast<std::size_t>(colors)) {
            int best_bucket = -1;
            std::uint64_t best_score = 0;
            for (std::size_t index = 0; index < buckets.size(); ++index) {
                if (buckets[index].size() <= 1u) {
                    continue;
                }
                int minimum[3] = {255, 255, 255};
                int maximum[3] = {0, 0, 0};
                std::uint64_t total = 0;
                for (int color_index : buckets[index]) {
                    const auto& item = unique[static_cast<std::size_t>(color_index)];
                    total += item.count;
                    minimum[0] = std::min(minimum[0], static_cast<int>(item.color.r));
                    minimum[1] = std::min(minimum[1], static_cast<int>(item.color.g));
                    minimum[2] = std::min(minimum[2], static_cast<int>(item.color.b));
                    maximum[0] = std::max(maximum[0], static_cast<int>(item.color.r));
                    maximum[1] = std::max(maximum[1], static_cast<int>(item.color.g));
                    maximum[2] = std::max(maximum[2], static_cast<int>(item.color.b));
                }
                const std::uint64_t score = total;
                if (best_bucket < 0 || score > best_score) {
                    best_bucket = static_cast<int>(index);
                    best_score = score;
                }
            }
            if (best_bucket < 0 || best_score == 0) {
                break;
            }

            std::vector<int> bucket = std::move(buckets[static_cast<std::size_t>(best_bucket)]);
            int minimum[3] = {255, 255, 255};
            int maximum[3] = {0, 0, 0};
            for (int color_index : bucket) {
                const auto& item = unique[static_cast<std::size_t>(color_index)];
                const int values[3] = {item.color.r, item.color.g, item.color.b};
                for (int channel = 0; channel < 3; ++channel) {
                    minimum[channel] = std::min(minimum[channel], values[channel]);
                    maximum[channel] = std::max(maximum[channel], values[channel]);
                }
            }
            const int luminance_weights[3] = {77, 150, 29};
            int channel = 0;
            int best_axis_score = (maximum[0] - minimum[0]) * luminance_weights[0];
            for (int candidate = 1; candidate < 3; ++candidate) {
                const int axis_score = (maximum[candidate] - minimum[candidate]) * luminance_weights[candidate];
                if (axis_score > best_axis_score) {
                    channel = candidate;
                    best_axis_score = axis_score;
                }
            }
            std::sort(bucket.begin(), bucket.end(), [&](int left, int right) {
                const auto& a = unique[static_cast<std::size_t>(left)].color;
                const auto& b = unique[static_cast<std::size_t>(right)].color;
                const std::uint8_t av = channel == 0 ? a.r : channel == 1 ? a.g : a.b;
                const std::uint8_t bv = channel == 0 ? b.r : channel == 1 ? b.g : b.b;
                if (av != bv) {
                    return av > bv;
                }
                return left < right;
            });
            std::uint64_t total = 0;
            for (int color_index : bucket) {
                total += unique[static_cast<std::size_t>(color_index)].count;
            }
            std::uint64_t cumulative = 0;
            std::size_t split = 1;
            for (std::size_t index = 0; index < bucket.size(); ++index) {
                cumulative += unique[static_cast<std::size_t>(bucket[index])].count;
                if (cumulative * 2u > total) {
                    split = index + 1u;
                    break;
                }
            }
            if (split >= bucket.size()) {
                split = bucket.size() - 1u;
            }
            buckets[static_cast<std::size_t>(best_bucket)] = std::vector<int>(
                bucket.begin(), bucket.begin() + static_cast<std::ptrdiff_t>(split));
            buckets.push_back(std::vector<int>(
                bucket.begin() + static_cast<std::ptrdiff_t>(split), bucket.end()));
        }

        std::vector<QuantizeNativeColor> palette;
        std::vector<int> unique_to_palette(unique.size(), 0);
        palette.reserve(buckets.size());
        for (std::size_t palette_index = 0; palette_index < buckets.size(); ++palette_index) {
            const auto& bucket = buckets[palette_index];
            QuantizeNativeWeightedColor sum{{0, 0, 0, static_cast<std::uint8_t>(with_alpha ? 0 : 255)}, 0, 0, 0, 0, 0};
            for (int color_index : bucket) {
                const auto& item = unique[static_cast<std::size_t>(color_index)];
                quantize_native_add(&sum, item.color, item.count);
                unique_to_palette[static_cast<std::size_t>(color_index)] = static_cast<int>(palette_index);
            }
            palette.push_back(quantize_native_average(sum, true));
        }
        quantize_native_write_palette(target, palette, with_alpha);
        if (kmeans > 0) {
            std::unordered_map<std::uint32_t, int> source_to_palette;
            source_to_palette.reserve(unique.size());
            for (std::size_t index = 0; index < unique.size(); ++index) {
                source_to_palette.emplace(
                    quantize_native_color_key(unique[index].color, with_alpha),
                    unique_to_palette[index]);
            }
            for (std::size_t index = 0; index < pixels.size(); ++index) {
                target->pixels[index] = static_cast<std::uint8_t>(
                    source_to_palette.at(quantize_native_color_key(pixels[index], with_alpha)));
            }
        } else {
            quantize_native_write_pixels(pixels, palette, with_alpha, target);
        }
        return quantize_native_refine(pixels, &palette, with_alpha, kmeans, target);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int quantize_native_max_coverage(
    const std::vector<QuantizeNativeColor>& pixels,
    const std::vector<QuantizeNativeWeightedColor>& unique,
    int colors,
    int kmeans,
    PillowCImage* target)
{
    try {
        std::vector<QuantizeNativeColor> palette;
        palette.reserve(static_cast<std::size_t>(colors));
        std::uint64_t sum_r = 0;
        std::uint64_t sum_g = 0;
        std::uint64_t sum_b = 0;
        std::uint64_t total = 0;
        for (const auto& item : unique) {
            sum_r += item.sum_r;
            sum_g += item.sum_g;
            sum_b += item.sum_b;
            total += item.count;
        }
        const QuantizeNativeColor mean{
            static_cast<std::uint8_t>((sum_r + total / 2u) / total),
            static_cast<std::uint8_t>((sum_g + total / 2u) / total),
            static_cast<std::uint8_t>((sum_b + total / 2u) / total),
            255};
        std::vector<bool> selected(unique.size(), false);
        for (int slot = 0; slot < colors; ++slot) {
            int best_index = -1;
            int best_distance = -1;
            for (std::size_t index = 0; index < unique.size(); ++index) {
                if (selected[index] && unique.size() >= static_cast<std::size_t>(colors)) {
                    continue;
                }
                int distance = 0;
                if (palette.empty()) {
                    distance = quantize_native_distance(unique[index].color, mean, false);
                } else {
                    distance = std::numeric_limits<int>::max();
                    for (const auto& candidate : palette) {
                        distance = std::min(distance, quantize_native_distance(unique[index].color, candidate, false));
                    }
                }
                if (best_index < 0 || distance > best_distance) {
                    best_index = static_cast<int>(index);
                    best_distance = distance;
                }
            }
            if (best_index < 0) {
                best_index = slot % static_cast<int>(unique.size());
            } else {
                selected[static_cast<std::size_t>(best_index)] = true;
            }
            palette.push_back(unique[static_cast<std::size_t>(best_index)].color);
        }
        quantize_native_write_palette(target, palette, false);
        quantize_native_write_pixels(pixels, palette, false, target);
        return quantize_native_refine(pixels, &palette, false, kmeans, target);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

struct QuantizeNativeGridBucket {
    QuantizeNativeWeightedColor value{{0, 0, 0, 0}, 0, 0, 0, 0, 0};
};

int quantize_native_fast_octree(
    const std::vector<QuantizeNativeColor>& pixels,
    int colors,
    bool with_alpha,
    PillowCImage* target)
{
    const int fine_bits[4] = {with_alpha ? 3 : 4, 4, with_alpha ? 3 : 4, with_alpha ? 3 : 0};
    const int coarse_bits[4] = {2, 2, 2, with_alpha ? 2 : 0};
    std::size_t fine_size = 1;
    std::size_t coarse_size = 1;
    for (int channel = 0; channel < 4; ++channel) {
        fine_size *= static_cast<std::size_t>(1) << fine_bits[channel];
        coarse_size *= static_cast<std::size_t>(1) << coarse_bits[channel];
    }

    try {
        std::vector<QuantizeNativeGridBucket> fine(fine_size);
        std::vector<QuantizeNativeGridBucket> coarse(coarse_size);
        for (const auto& color : pixels) {
            quantize_native_add(&fine[quantize_native_grid_index(color, fine_bits)].value, color, 1u);
            quantize_native_add(&coarse[quantize_native_grid_index(color, coarse_bits)].value, color, 1u);
        }

        std::vector<std::size_t> fine_order(fine_size);
        std::iota(fine_order.begin(), fine_order.end(), 0u);
        std::sort(fine_order.begin(), fine_order.end(), [&](std::size_t left, std::size_t right) {
            if (fine[left].value.count != fine[right].value.count) {
                return fine[left].value.count > fine[right].value.count;
            }
            return left < right;
        });
        auto count_used = [](const std::vector<QuantizeNativeGridBucket>& buckets) {
            std::size_t count = 0;
            for (const auto& bucket : buckets) {
                if (bucket.value.count > 0) {
                    ++count;
                }
            }
            return count;
        };
        std::vector<std::size_t> coarse_order;
        coarse_order.reserve(coarse_size);
        for (std::size_t index = 0; index < coarse_size; ++index) {
            if (coarse[index].value.count > 0) {
                coarse_order.push_back(index);
            }
        }
        std::sort(coarse_order.begin(), coarse_order.end(), [&](std::size_t left, std::size_t right) {
            if (coarse[left].value.count != coarse[right].value.count) {
                return coarse[left].value.count > coarse[right].value.count;
            }
            return left < right;
        });

        std::size_t coarse_count = std::min<std::size_t>(colors, coarse_order.size());
        std::size_t fine_count = static_cast<std::size_t>(colors) - coarse_count;
        auto subtract_fine = [&](std::size_t begin, std::size_t end) {
            for (std::size_t order = begin; order < end; ++order) {
                const std::size_t fine_index = fine_order[order];
                if (fine[fine_index].value.count == 0) {
                    continue;
                }
                const QuantizeNativeColor average = quantize_native_average(fine[fine_index].value, false);
                const std::size_t coarse_index = quantize_native_grid_index(average, coarse_bits);
                QuantizeNativeWeightedColor& destination = coarse[coarse_index].value;
                const auto& source = fine[fine_index].value;
                destination.count -= source.count;
                destination.sum_r -= source.sum_r;
                destination.sum_g -= source.sum_g;
                destination.sum_b -= source.sum_b;
                destination.sum_a -= source.sum_a;
            }
        };
        subtract_fine(0, fine_count);
        while (coarse_count > count_used(coarse)) {
            const std::size_t already_subtracted = fine_count;
            coarse_count = count_used(coarse);
            fine_count = static_cast<std::size_t>(colors) - coarse_count;
            subtract_fine(already_subtracted, fine_count);
        }

        coarse_order.clear();
        for (std::size_t index = 0; index < coarse_size; ++index) {
            if (coarse[index].value.count > 0) {
                coarse_order.push_back(index);
            }
        }
        std::sort(coarse_order.begin(), coarse_order.end(), [&](std::size_t left, std::size_t right) {
            if (coarse[left].value.count != coarse[right].value.count) {
                return coarse[left].value.count > coarse[right].value.count;
            }
            return left < right;
        });
        if (coarse_order.size() < coarse_count) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<QuantizeNativeColor> palette;
        palette.reserve(static_cast<std::size_t>(colors));
        for (std::size_t index = 0; index < coarse_count; ++index) {
            palette.push_back(quantize_native_average(coarse[coarse_order[index]].value, false));
        }
        for (std::size_t index = 0; index < fine_count; ++index) {
            palette.push_back(quantize_native_average(fine[fine_order[index]].value, false));
        }
        std::vector<int> fine_lookup(fine_size, -1);
        for (std::size_t index = 0; index < fine_count; ++index) {
            const std::size_t fine_index = fine_order[index];
            if (fine[fine_index].value.count > 0) {
                fine_lookup[fine_index] = static_cast<int>(coarse_count + index);
            }
        }
        std::vector<int> coarse_lookup(coarse_size, 0);
        std::vector<QuantizeNativeColor> coarse_palette;
        coarse_palette.reserve(coarse_count);
        for (std::size_t index = 0; index < coarse_count; ++index) {
            coarse_palette.push_back(palette[index]);
        }
        for (std::size_t index = 0; index < coarse_size; ++index) {
            const QuantizeNativeColor representative = coarse[index].value.count > 0
                ? quantize_native_average(coarse[index].value, false)
                : quantize_native_cell_center(index, coarse_bits);
            const int nearest = quantize_native_nearest(
                coarse_palette.empty() ? palette : coarse_palette,
                representative,
                with_alpha);
            coarse_lookup[index] = nearest < 0 ? 0 : nearest;
        }

        quantize_native_write_palette(target, palette, with_alpha);
        for (std::size_t index = 0; index < pixels.size(); ++index) {
            const std::size_t fine_index = quantize_native_grid_index(pixels[index], fine_bits);
            const int mapped = fine_lookup[fine_index] >= 0
                ? fine_lookup[fine_index]
                : coarse_count > 0
                    ? coarse_lookup[quantize_native_grid_index(pixels[index], coarse_bits)]
                    : 0;
            target->pixels[index] = static_cast<std::uint8_t>(mapped);
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    return PILLOW_C_OK;
}

int quantize_options_image_into(
    const PillowCImage* source,
    int colors,
    int method,
    int kmeans,
    int dither,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_OK;
    }
    if (colors < 1 || colors > 256 || kmeans < 0 || method < 0 || method > 3) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (method == 3) {
        // libimagequant is not linked in this build.  The facade maps this
        // explicit dependency boundary to Pillow's ValueError text.
        (void)dither;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source->mode != PILLOW_C_MODE_L && source->mode != PILLOW_C_MODE_P &&
        source->mode != PILLOW_C_MODE_RGB && source->mode != PILLOW_C_MODE_RGBA) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source->mode == PILLOW_C_MODE_RGBA && method != 2) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    bool with_alpha = false;
    std::vector<QuantizeNativeColor> pixels;
    std::vector<QuantizeNativeWeightedColor> unique;
    const int collect_status = quantize_native_collect(source, &pixels, &unique, &with_alpha);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }
    try {
        if (method == 0 && kmeans == 0 &&
            (source->mode == PILLOW_C_MODE_RGB || source->mode == PILLOW_C_MODE_L)) {
            return quantize_exact_image_into(source, colors, target);
        }
        if (method == 0) {
            return quantize_native_median_cut(pixels, unique, colors, with_alpha, kmeans, target);
        }
        if (method == 1) {
            return quantize_native_max_coverage(pixels, unique, colors, kmeans, target);
        }
        return quantize_native_fast_octree(pixels, colors, with_alpha, target);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int remap_palette_image_into(
    const PillowCImage* source,
    const int* dest_map,
    std::size_t dest_count,
    const std::uint8_t* source_palette,
    std::size_t source_palette_size,
    PillowCImage* target)
{
    if (!source || !target || (!dest_map && dest_count > 0) || (!source_palette && source_palette_size > 0)) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((source->mode != PILLOW_C_MODE_P && source->mode != PILLOW_C_MODE_L) ||
        source->channels != 1 ||
        dest_count > 256u ||
        source_palette_size > 768u ||
        source_palette_size % 3u != 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
        return PILLOW_C_MISMATCH;
    }

    std::uint8_t new_positions[256] = {};
    for (std::size_t index = 0; index < dest_count; ++index) {
        const int old_position = dest_map[index];
        if (old_position < 0 || old_position > 255) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        new_positions[old_position] = static_cast<std::uint8_t>(index);
    }

    std::vector<std::uint8_t> gray_palette;
    const std::uint8_t* palette_data = source_palette;
    std::size_t palette_size = source_palette_size;
    if (!palette_data) {
        if (source->mode == PILLOW_C_MODE_P) {
            palette_data = source->palette_rgb.empty() ? nullptr : source->palette_rgb.data();
            palette_size = source->palette_rgb.size();
        } else {
            try {
                gray_palette.resize(768u);
            } catch (const std::bad_alloc&) {
                return PILLOW_C_ALLOCATION_FAILED;
            }
            for (int value = 0; value < 256; ++value) {
                const std::size_t offset = static_cast<std::size_t>(value) * 3u;
                gray_palette[offset] = static_cast<std::uint8_t>(value);
                gray_palette[offset + 1u] = static_cast<std::uint8_t>(value);
                gray_palette[offset + 2u] = static_cast<std::uint8_t>(value);
            }
            palette_data = gray_palette.data();
            palette_size = gray_palette.size();
        }
    }

    std::vector<std::uint8_t> remapped_palette;
    try {
        remapped_palette.reserve(dest_count * 3u);
        for (std::size_t index = 0; index < dest_count; ++index) {
            const std::size_t offset = static_cast<std::size_t>(dest_map[index]) * 3u;
            if (palette_data && offset < palette_size) {
                const std::size_t available = std::min<std::size_t>(3u, palette_size - offset);
                remapped_palette.insert(remapped_palette.end(), palette_data + offset, palette_data + offset + available);
            }
        }

        const std::uint8_t* source_pixels = source->pixels.empty() ? nullptr : source->pixels.data();
        std::vector<std::uint8_t> source_copy;
        if (source == target && !source->pixels.empty()) {
            source_copy = source->pixels;
            source_pixels = source_copy.data();
        }
        for (std::size_t index = 0; index < target->pixels.size(); ++index) {
            target->pixels[index] = new_positions[source_pixels[index]];
        }
        target->palette_rgb = std::move(remapped_palette);
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int convert_image_mode_into(const PillowCImage* source, int target_mode, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }

    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->mode == target_mode) {
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        target->palette_rgb = source->palette_rgb;
        target->palette_alpha = source->palette_alpha;
        target->palette_alpha_mode = source->palette_alpha_mode;
        return PILLOW_C_OK;
    }
    if (source->mode == PILLOW_C_MODE_LAB &&
        (target_mode == PILLOW_C_MODE_L ||
         target_mode == PILLOW_C_MODE_LA ||
         target_mode == PILLOW_C_MODE_1 ||
         target_mode == PILLOW_C_MODE_P ||
         target_mode == PILLOW_C_MODE_CMYK ||
         target_mode == PILLOW_C_MODE_I ||
         target_mode == PILLOW_C_MODE_F ||
         target_mode == PILLOW_C_MODE_YCBCR ||
         target_mode == PILLOW_C_MODE_HSV ||
         target_mode == PILLOW_C_MODE_PA)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source->mode == PILLOW_C_MODE_P || source->mode == PILLOW_C_MODE_PA) {
        return convert_palette_image_into(source, target_mode, target);
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;

    if ((source->mode == PILLOW_C_MODE_RGB ||
         source->mode == PILLOW_C_MODE_RGBA ||
         source->mode == PILLOW_C_MODE_RGBX) &&
        (target_mode == PILLOW_C_MODE_I || target_mode == PILLOW_C_MODE_F)) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source->pixels.data() + i * source->channels;
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            if (target_mode == PILLOW_C_MODE_I) {
                pillow_c_write_i32_le(dst, rgb_luma_u8(src));
            } else {
                pillow_c_write_f32_le(dst, static_cast<float>(rgb_luma_1000(src)) / 1000.0F);
            }
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_YCBCR &&
        (target_mode == PILLOW_C_MODE_I || target_mode == PILLOW_C_MODE_F)) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t rgb[3];
            ycbcr_to_rgb_u8(source->pixels.data() + i * 3u, rgb);
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            if (target_mode == PILLOW_C_MODE_I) {
                pillow_c_write_i32_le(dst, rgb_luma_u8(rgb));
            } else {
                pillow_c_write_f32_le(dst, static_cast<float>(rgb_luma_1000(rgb)) / 1000.0F);
            }
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_1 ||
         source->mode == PILLOW_C_MODE_L ||
         source->mode == PILLOW_C_MODE_LA) &&
        (target_mode == PILLOW_C_MODE_I || target_mode == PILLOW_C_MODE_F)) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->mode == PILLOW_C_MODE_1
                ? (source->pixels[i] == 0 ? 0u : 255u)
                : source->pixels[i * source->channels];
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            if (target_mode == PILLOW_C_MODE_I) {
                pillow_c_write_i32_le(dst, value);
            } else {
                pillow_c_write_f32_le(dst, static_cast<float>(value));
            }
        }
        return PILLOW_C_OK;
    }

    if (target_mode == PILLOW_C_MODE_L && source->mode == PILLOW_C_MODE_I) {
        const std::uint8_t* src = source->pixels.data();
        std::uint8_t* dst = target->pixels.data();
        for (std::size_t i = 0; i < pixels; ++i) {
            dst[i] = clip_u8_int(static_cast<int>(pillow_c_read_i32_le(src + i * 4u)));
        }
        return PILLOW_C_OK;
    }

    if (target_mode == PILLOW_C_MODE_L && source->mode == PILLOW_C_MODE_F) {
        const std::uint8_t* src = source->pixels.data();
        std::uint8_t* dst = target->pixels.data();
        for (std::size_t i = 0; i < pixels; ++i) {
            dst[i] = clip_numeric_f32_to_l(pillow_c_read_f32_le(src + i * 4u));
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F) &&
        (target_mode == PILLOW_C_MODE_LA ||
         target_mode == PILLOW_C_MODE_RGB ||
         target_mode == PILLOW_C_MODE_RGBA ||
         target_mode == PILLOW_C_MODE_RGBX)) {
        const std::uint8_t* src = source->pixels.data();
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->mode == PILLOW_C_MODE_I
                ? clip_u8_int(static_cast<int>(pillow_c_read_i32_le(src + i * 4u)))
                : clip_numeric_f32_to_l(pillow_c_read_f32_le(src + i * 4u));
            std::uint8_t* dst = target->pixels.data() + i * target_channels;
            if (target_mode == PILLOW_C_MODE_LA) {
                dst[0] = value;
                dst[1] = 255;
            } else {
                dst[0] = value;
                dst[1] = value;
                dst[2] = value;
                if (target_channels == 4) {
                    dst[3] = 255;
                }
            }
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_1 ||
         source->mode == PILLOW_C_MODE_L ||
         source->mode == PILLOW_C_MODE_LA ||
         source->mode == PILLOW_C_MODE_I ||
         source->mode == PILLOW_C_MODE_F) &&
        target_mode == PILLOW_C_MODE_LAB) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t value = 0;
            if (source->mode == PILLOW_C_MODE_1) {
                value = source->pixels[i] == 0 ? 0u : 255u;
            } else if (source->mode == PILLOW_C_MODE_I) {
                value = clip_u8_int(static_cast<int>(pillow_c_read_i32_le(source->pixels.data() + i * 4u)));
            } else if (source->mode == PILLOW_C_MODE_F) {
                value = clip_numeric_f32_to_l(pillow_c_read_f32_le(source->pixels.data() + i * 4u));
            } else {
                value = source->pixels[i * static_cast<std::size_t>(source->channels)];
            }
            std::uint8_t* dst = target->pixels.data() + i * 3u;
            dst[0] = PILLOW_L_TO_LAB_L[value];
            dst[1] = 128;
            dst[2] = 128;
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_RGB ||
         source->mode == PILLOW_C_MODE_RGBA ||
         source->mode == PILLOW_C_MODE_RGBX) &&
        target_mode == PILLOW_C_MODE_LAB) {
        return pillow_c_apply_builtin_lab_transform(source, target, target_mode);
    }

    if (source->mode == PILLOW_C_MODE_LAB &&
        (target_mode == PILLOW_C_MODE_RGB ||
         target_mode == PILLOW_C_MODE_RGBA ||
         target_mode == PILLOW_C_MODE_RGBX)) {
        return pillow_c_apply_builtin_lab_transform(source, target, target_mode);
    }

    if (source->mode == PILLOW_C_MODE_YCBCR && target_mode == PILLOW_C_MODE_L) {
        for (std::size_t i = 0; i < pixels; ++i) {
            target->pixels[i] = source->pixels[i * 3u];
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_YCBCR && target_mode == PILLOW_C_MODE_LA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            target->pixels[i * 2u] = source->pixels[i * 3u];
            target->pixels[i * 2u + 1u] = 255;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_YCBCR && target_mode == PILLOW_C_MODE_RGB) {
        for (std::size_t i = 0; i < pixels; ++i) {
            ycbcr_to_rgb_u8(
                source->pixels.data() + i * 3u,
                target->pixels.data() + i * 3u);
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_YCBCR &&
        (target_mode == PILLOW_C_MODE_RGBA || target_mode == PILLOW_C_MODE_RGBX)) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t rgb[3];
            ycbcr_to_rgb_u8(source->pixels.data() + i * 3u, rgb);
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            dst[0] = rgb[0];
            dst[1] = rgb[1];
            dst[2] = rgb[2];
            dst[3] = 255;
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_1 || source->mode == PILLOW_C_MODE_L ||
            source->mode == PILLOW_C_MODE_LA) &&
        target_mode == PILLOW_C_MODE_RGBX) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->mode == PILLOW_C_MODE_1
                ? (source->pixels[i] == 0 ? 0u : 255u)
                : source->pixels[i * source->channels];
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            dst[0] = value;
            dst[1] = value;
            dst[2] = value;
            dst[3] = source->mode == PILLOW_C_MODE_LA
                ? source->pixels[i * 2u + 1u]
                : std::uint8_t{255};
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_RGB || source->mode == PILLOW_C_MODE_RGBA) &&
        target_mode == PILLOW_C_MODE_RGBX) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source->pixels.data() + i * source->channels;
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_1 || source->mode == PILLOW_C_MODE_L ||
            source->mode == PILLOW_C_MODE_LA || source->mode == PILLOW_C_MODE_I ||
            source->mode == PILLOW_C_MODE_F) &&
        target_mode == PILLOW_C_MODE_YCBCR) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t value = 0;
            if (source->mode == PILLOW_C_MODE_1) {
                value = source->pixels[i] == 0 ? 0u : 255u;
            } else if (source->mode == PILLOW_C_MODE_I) {
                value = clip_u8_int(static_cast<int>(pillow_c_read_i32_le(source->pixels.data() + i * 4u)));
            } else if (source->mode == PILLOW_C_MODE_F) {
                value = clip_numeric_f32_to_l(pillow_c_read_f32_le(source->pixels.data() + i * 4u));
            } else {
                value = source->pixels[i * source->channels];
            }
            std::uint8_t* dst = target->pixels.data() + i * 3u;
            dst[0] = value;
            dst[1] = 128;
            dst[2] = 128;
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_RGB || source->mode == PILLOW_C_MODE_RGBA ||
            source->mode == PILLOW_C_MODE_RGBX) &&
        target_mode == PILLOW_C_MODE_YCBCR) {
        for (std::size_t i = 0; i < pixels; ++i) {
            rgb_to_ycbcr_u8(
                source->pixels.data() + i * source->channels,
                target->pixels.data() + i * 3u);
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_HSV && target_mode == PILLOW_C_MODE_RGB) {
        for (std::size_t i = 0; i < pixels; ++i) {
            hsv_to_rgb_u8(
                source->pixels.data() + i * 3u,
                target->pixels.data() + i * 3u);
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_HSV && target_mode == PILLOW_C_MODE_YCBCR) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t rgb[3];
            hsv_to_rgb_u8(source->pixels.data() + i * 3u, rgb);
            rgb_to_ycbcr_u8(rgb, target->pixels.data() + i * 3u);
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_HSV &&
        (target_mode == PILLOW_C_MODE_RGBA || target_mode == PILLOW_C_MODE_RGBX)) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t rgb[3];
            hsv_to_rgb_u8(source->pixels.data() + i * 3u, rgb);
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            dst[0] = rgb[0];
            dst[1] = rgb[1];
            dst[2] = rgb[2];
            dst[3] = 255;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_HSV &&
        (target_mode == PILLOW_C_MODE_L || target_mode == PILLOW_C_MODE_LA)) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t rgb[3];
            hsv_to_rgb_u8(source->pixels.data() + i * 3u, rgb);
            const std::uint8_t luma = rgb_luma_u8(rgb);
            if (target_mode == PILLOW_C_MODE_L) {
                target->pixels[i] = luma;
            } else {
                std::uint8_t* dst = target->pixels.data() + i * 2u;
                dst[0] = luma;
                dst[1] = 255;
            }
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_HSV &&
        (target_mode == PILLOW_C_MODE_I || target_mode == PILLOW_C_MODE_F)) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t rgb[3];
            hsv_to_rgb_u8(source->pixels.data() + i * 3u, rgb);
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            if (target_mode == PILLOW_C_MODE_I) {
                pillow_c_write_i32_le(dst, rgb_luma_u8(rgb));
            } else {
                pillow_c_write_f32_le(dst, static_cast<float>(rgb_luma_1000(rgb)) / 1000.0F);
            }
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_RGB && target_mode == PILLOW_C_MODE_HSV) {
        for (std::size_t i = 0; i < pixels; ++i) {
            rgb_to_hsv_u8(
                source->pixels.data() + i * 3u,
                target->pixels.data() + i * 3u);
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_YCBCR && target_mode == PILLOW_C_MODE_HSV) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t rgb[3];
            ycbcr_to_rgb_u8(source->pixels.data() + i * 3u, rgb);
            rgb_to_hsv_u8(rgb, target->pixels.data() + i * 3u);
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_1 ||
         source->mode == PILLOW_C_MODE_L ||
         source->mode == PILLOW_C_MODE_LA ||
         source->mode == PILLOW_C_MODE_I ||
         source->mode == PILLOW_C_MODE_F) &&
        target_mode == PILLOW_C_MODE_HSV) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t value = 0;
            if (source->mode == PILLOW_C_MODE_1) {
                value = source->pixels[i] == 0 ? 0u : 255u;
            } else if (source->mode == PILLOW_C_MODE_I) {
                value = clip_u8_int(static_cast<int>(pillow_c_read_i32_le(source->pixels.data() + i * 4u)));
            } else if (source->mode == PILLOW_C_MODE_F) {
                value = clip_numeric_f32_to_l(pillow_c_read_f32_le(source->pixels.data() + i * 4u));
            } else {
                value = source->pixels[i * static_cast<std::size_t>(source->channels)];
            }
            std::uint8_t* dst = target->pixels.data() + i * 3u;
            dst[0] = 0;
            dst[1] = 0;
            dst[2] = value;
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_RGBA || source->mode == PILLOW_C_MODE_RGBX) &&
        target_mode == PILLOW_C_MODE_HSV) {
        for (std::size_t i = 0; i < pixels; ++i) {
            rgb_to_hsv_u8(
                source->pixels.data() + i * 4u,
                target->pixels.data() + i * 3u);
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_CMYK) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source->pixels.data() + i * 4u;
            std::uint8_t rgb[3];
            cmyk_to_rgb_u8(src, rgb);
            if (target_mode == PILLOW_C_MODE_RGB) {
                std::uint8_t* dst = target->pixels.data() + i * 3u;
                dst[0] = rgb[0];
                dst[1] = rgb[1];
                dst[2] = rgb[2];
            } else if (target_mode == PILLOW_C_MODE_RGBA || target_mode == PILLOW_C_MODE_RGBX) {
                std::uint8_t* dst = target->pixels.data() + i * 4u;
                dst[0] = rgb[0];
                dst[1] = rgb[1];
                dst[2] = rgb[2];
                dst[3] = 255;
            } else if (target_mode == PILLOW_C_MODE_L) {
                target->pixels[i] = rgb_luma_u8(rgb);
            } else if (target_mode == PILLOW_C_MODE_LA) {
                std::uint8_t* dst = target->pixels.data() + i * 2u;
                dst[0] = rgb_luma_u8(rgb);
                dst[1] = 255;
            } else if (target_mode == PILLOW_C_MODE_I) {
                pillow_c_write_i32_le(target->pixels.data() + i * 4u, rgb_luma_u8(rgb));
            } else if (target_mode == PILLOW_C_MODE_F) {
                pillow_c_write_f32_le(
                    target->pixels.data() + i * 4u,
                    static_cast<float>(rgb_luma_1000(rgb)) / 1000.0F);
            } else if (target_mode == PILLOW_C_MODE_YCBCR) {
                rgb_to_ycbcr_u8(rgb, target->pixels.data() + i * 3u);
            } else if (target_mode == PILLOW_C_MODE_HSV) {
                rgb_to_hsv_u8(rgb, target->pixels.data() + i * 3u);
            } else {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
        return PILLOW_C_OK;
    }

    if (target_mode == PILLOW_C_MODE_CMYK) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            if (source->mode == PILLOW_C_MODE_1) {
                const std::uint8_t value = source->pixels[i] == 0 ? 0u : 255u;
                dst[0] = 0;
                dst[1] = 0;
                dst[2] = 0;
                dst[3] = static_cast<std::uint8_t>(255u - value);
            } else if (source->mode == PILLOW_C_MODE_L) {
                dst[0] = 0;
                dst[1] = 0;
                dst[2] = 0;
                dst[3] = static_cast<std::uint8_t>(255u - source->pixels[i]);
            } else if (source->mode == PILLOW_C_MODE_LA) {
                const std::uint8_t* src = source->pixels.data() + i * 2u;
                dst[0] = 0;
                dst[1] = 0;
                dst[2] = 0;
                dst[3] = static_cast<std::uint8_t>(255u - src[0]);
            } else if (source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F) {
                const std::uint8_t value = source->mode == PILLOW_C_MODE_I
                    ? clip_u8_int(static_cast<int>(pillow_c_read_i32_le(source->pixels.data() + i * 4u)))
                    : clip_numeric_f32_to_l(pillow_c_read_f32_le(source->pixels.data() + i * 4u));
                dst[0] = 0;
                dst[1] = 0;
                dst[2] = 0;
                dst[3] = static_cast<std::uint8_t>(255u - value);
            } else if (source->mode == PILLOW_C_MODE_RGB ||
                       source->mode == PILLOW_C_MODE_RGBA ||
                       source->mode == PILLOW_C_MODE_RGBX) {
                const std::uint8_t* src = source->pixels.data() + i * source->channels;
                dst[0] = static_cast<std::uint8_t>(255u - src[0]);
                dst[1] = static_cast<std::uint8_t>(255u - src[1]);
                dst[2] = static_cast<std::uint8_t>(255u - src[2]);
                dst[3] = 0;
            } else if (source->mode == PILLOW_C_MODE_YCBCR) {
                std::uint8_t rgb[3];
                ycbcr_to_rgb_u8(source->pixels.data() + i * 3u, rgb);
                dst[0] = static_cast<std::uint8_t>(255u - rgb[0]);
                dst[1] = static_cast<std::uint8_t>(255u - rgb[1]);
                dst[2] = static_cast<std::uint8_t>(255u - rgb[2]);
                dst[3] = 0;
            } else if (source->mode == PILLOW_C_MODE_HSV) {
                std::uint8_t rgb[3];
                hsv_to_rgb_u8(source->pixels.data() + i * 3u, rgb);
                dst[0] = static_cast<std::uint8_t>(255u - rgb[0]);
                dst[1] = static_cast<std::uint8_t>(255u - rgb[1]);
                dst[2] = static_cast<std::uint8_t>(255u - rgb[2]);
                dst[3] = 0;
            } else {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
        return PILLOW_C_OK;
    }

    if (target_mode == PILLOW_C_MODE_L && source->mode == PILLOW_C_MODE_LA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            target->pixels[i] = source->pixels[i * 2];
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_1 && target_mode == PILLOW_C_MODE_L) {
        for (std::size_t i = 0; i < pixels; ++i) {
            target->pixels[i] = source->pixels[i] == 0 ? 0u : 255u;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_1 && target_mode == PILLOW_C_MODE_LA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->pixels[i] == 0 ? 0u : 255u;
            std::uint8_t* dst = target->pixels.data() + i * 2;
            dst[0] = value;
            dst[1] = 255;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_1 && target_mode == PILLOW_C_MODE_RGB) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->pixels[i] == 0 ? 0u : 255u;
            std::uint8_t* dst = target->pixels.data() + i * 3;
            dst[0] = value;
            dst[1] = value;
            dst[2] = value;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_1 && target_mode == PILLOW_C_MODE_RGBA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->pixels[i] == 0 ? 0u : 255u;
            std::uint8_t* dst = target->pixels.data() + i * 4;
            dst[0] = value;
            dst[1] = value;
            dst[2] = value;
            dst[3] = 255;
        }
        return PILLOW_C_OK;
    }

    if (target_mode == PILLOW_C_MODE_L &&
        (source->mode == PILLOW_C_MODE_RGB ||
         source->mode == PILLOW_C_MODE_RGBA ||
         source->mode == PILLOW_C_MODE_RGBX)) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* px = source->pixels.data() + i * source->channels;
            target->pixels[i] = rgb_luma_u8(px);
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_L && target_mode == PILLOW_C_MODE_LA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->pixels[i];
            std::uint8_t* dst = target->pixels.data() + i * 2;
            dst[0] = value;
            dst[1] = 255;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_L && target_mode == PILLOW_C_MODE_RGB) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->pixels[i];
            std::uint8_t* dst = target->pixels.data() + i * 3;
            dst[0] = value;
            dst[1] = value;
            dst[2] = value;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_L && target_mode == PILLOW_C_MODE_RGBA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->pixels[i];
            std::uint8_t* dst = target->pixels.data() + i * 4;
            dst[0] = value;
            dst[1] = value;
            dst[2] = value;
            dst[3] =
                source->has_png_transparency &&
                source->png_transparency >= 0 &&
                value == static_cast<std::uint8_t>(source->png_transparency)
                    ? 0
                    : 255;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_LA && target_mode == PILLOW_C_MODE_RGB) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->pixels[i * 2];
            std::uint8_t* dst = target->pixels.data() + i * 3;
            dst[0] = value;
            dst[1] = value;
            dst[2] = value;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_LA && target_mode == PILLOW_C_MODE_RGBA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source->pixels.data() + i * 2;
            std::uint8_t* dst = target->pixels.data() + i * 4;
            dst[0] = src[0];
            dst[1] = src[0];
            dst[2] = src[0];
            dst[3] = src[1];
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_RGB || source->mode == PILLOW_C_MODE_RGBX) &&
        target_mode == PILLOW_C_MODE_LA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source->pixels.data() + i * source->channels;
            std::uint8_t* dst = target->pixels.data() + i * 2;
            dst[0] = rgb_luma_u8(src);
            dst[1] = 255;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_RGB && target_mode == PILLOW_C_MODE_RGBA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source->pixels.data() + i * 3;
            std::uint8_t* dst = target->pixels.data() + i * 4;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] =
                source->has_png_rgb_transparency &&
                src[0] == source->png_rgb_transparency[0] &&
                src[1] == source->png_rgb_transparency[1] &&
                src[2] == source->png_rgb_transparency[2]
                    ? 0
                    : 255;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_RGBX && target_mode == PILLOW_C_MODE_RGBA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source->pixels.data() + i * 4;
            std::uint8_t* dst = target->pixels.data() + i * 4;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_RGBA && target_mode == PILLOW_C_MODE_LA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source->pixels.data() + i * 4;
            std::uint8_t* dst = target->pixels.data() + i * 2;
            dst[0] = rgb_luma_u8(src);
            dst[1] = src[3];
        }
        return PILLOW_C_OK;
    }

    if ((source->mode == PILLOW_C_MODE_RGBA || source->mode == PILLOW_C_MODE_RGBX) &&
        target_mode == PILLOW_C_MODE_RGB) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source->pixels.data() + i * 4;
            std::uint8_t* dst = target->pixels.data() + i * 3;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
        }
        return PILLOW_C_OK;
    }

    return PILLOW_C_INVALID_ARGUMENT;
}

inline std::uint8_t clip_matrix_float(float value)
{
    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 255.0f) {
        return 255;
    }
    return static_cast<std::uint8_t>(value);
}

bool valid_convert_matrix_arguments(
    const PillowCImage* source,
    int target_mode,
    std::size_t matrix_count,
    int* out_target_channels)
{
    if (!source || !out_target_channels || source->mode != PILLOW_C_MODE_RGB || source->channels != 3) {
        return false;
    }
    if (target_mode == PILLOW_C_MODE_L) {
        *out_target_channels = 1;
        return matrix_count == 4u;
    }
    if (target_mode == PILLOW_C_MODE_RGB) {
        *out_target_channels = 3;
        return matrix_count == 12u;
    }
    return false;
}

int convert_matrix_image_into(
    const PillowCImage* source,
    int target_mode,
    const double* matrix,
    std::size_t matrix_count,
    PillowCImage* target)
{
    if (!source || !matrix || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    int target_channels = 0;
    if (!valid_convert_matrix_arguments(source, target_mode, matrix_count, &target_channels)) {
        if (source && source->mode == PILLOW_C_MODE_RGB &&
            (target_mode == PILLOW_C_MODE_L || target_mode == PILLOW_C_MODE_RGB) &&
            matrix_count != (target_mode == PILLOW_C_MODE_L ? 4u : 12u)) {
            return PILLOW_C_INVALID_LENGTH;
        }
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }
    for (std::size_t index = 0; index < matrix_count; ++index) {
        if (!std::isfinite(matrix[index])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    std::vector<std::uint8_t> source_snapshot;
    const std::uint8_t* source_data = source->pixels.data();
    if (source == target) {
        source_snapshot = source->pixels;
        source_data = source_snapshot.data();
    }

    const double* m = matrix;
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    if (target_mode == PILLOW_C_MODE_L) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source_data + i * 3u;
            const float value =
                static_cast<float>(m[0]) * src[0] +
                static_cast<float>(m[1]) * src[1] +
                static_cast<float>(m[2]) * src[2] +
                static_cast<float>(m[3]) +
                0.5f;
            target->pixels[i] = clip_matrix_float(value);
        }
        return PILLOW_C_OK;
    }

    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint8_t* src = source_data + i * 3u;
        std::uint8_t* dst = target->pixels.data() + i * 3u;
        for (int out_channel = 0; out_channel < 3; ++out_channel) {
            const std::size_t offset = static_cast<std::size_t>(out_channel) * 4u;
            const float value =
                static_cast<float>(m[offset]) * src[0] +
                static_cast<float>(m[offset + 1u]) * src[1] +
                static_cast<float>(m[offset + 2u]) * src[2] +
                static_cast<float>(m[offset + 3u]) +
                0.5f;
            dst[out_channel] = clip_matrix_float(value);
        }
    }
    return PILLOW_C_OK;
}

int convert_image_to_mode1_floyd_steinberg_into(const PillowCImage* source, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_1, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }
    if (source->mode != PILLOW_C_MODE_L &&
        source->mode != PILLOW_C_MODE_LA &&
        source->mode != PILLOW_C_MODE_RGB &&
        source->mode != PILLOW_C_MODE_RGBA &&
        source->mode != PILLOW_C_MODE_RGBX &&
        source->mode != PILLOW_C_MODE_CMYK &&
        source->mode != PILLOW_C_MODE_HSV &&
        source->mode != PILLOW_C_MODE_YCBCR &&
        source->mode != PILLOW_C_MODE_I &&
        source->mode != PILLOW_C_MODE_F) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<int> errors(static_cast<std::size_t>(source->width) + 1u);
        for (int y = 0; y < source->height; ++y) {
            int l = 0;
            int l0 = 0;
            int l1 = 0;
            std::uint8_t* dst = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
            const std::uint8_t* src = source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
            for (int x = 0; x < source->width; ++x) {
                int value = 0;
                if (source->mode == PILLOW_C_MODE_L || source->mode == PILLOW_C_MODE_LA) {
                    value = src[static_cast<std::size_t>(x) * source->channels];
                } else if (source->mode == PILLOW_C_MODE_I) {
                    value = clip_u8_int(static_cast<int>(pillow_c_read_i32_le(src + static_cast<std::size_t>(x) * 4u)));
                } else if (source->mode == PILLOW_C_MODE_F) {
                    value = clip_numeric_f32_to_l(pillow_c_read_f32_le(src + static_cast<std::size_t>(x) * 4u));
                } else if (source->mode == PILLOW_C_MODE_CMYK) {
                    value = cmyk_luma_u8(src + static_cast<std::size_t>(x) * source->channels);
                } else if (source->mode == PILLOW_C_MODE_HSV) {
                    std::uint8_t rgb[3];
                    hsv_to_rgb_u8(src + static_cast<std::size_t>(x) * 3u, rgb);
                    value = rgb_luma_1000(rgb) / 1000;
                } else if (source->mode == PILLOW_C_MODE_YCBCR) {
                    std::uint8_t rgb[3];
                    ycbcr_to_rgb_u8(src + static_cast<std::size_t>(x) * 3u, rgb);
                    value = rgb_luma_1000(rgb) / 1000;
                } else {
                    value = rgb_luma_1000(src + static_cast<std::size_t>(x) * source->channels) / 1000;
                }
                l = clip_u8_int(value + (l + errors[static_cast<std::size_t>(x) + 1u]) / 16);
                dst[x] = (l > 128) ? 255u : 0u;

                l -= static_cast<int>(dst[x]);
                const int l2 = l;
                const int d2 = l + l;
                l += d2;
                errors[static_cast<std::size_t>(x)] = l + l0;
                l += d2;
                l0 = l + l1;
                l1 = l2;
                l += d2;
            }
            errors[static_cast<std::size_t>(source->width)] = l0;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int convert_image_mode_dither_into(const PillowCImage* source, int target_mode, int dither, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (target_mode != PILLOW_C_MODE_1) {
        return convert_image_mode_into(source, target_mode, target);
    }
    if (dither != 0 && dither != 3) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source->mode == PILLOW_C_MODE_1) {
        return convert_image_mode_into(source, target_mode, target);
    }
    if (source->mode == PILLOW_C_MODE_LAB) {
        return convert_image_mode_into(source, target_mode, target);
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_1, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const bool source_is_p = source->mode == PILLOW_C_MODE_P && source->channels == 1;
    const bool source_is_pa = source->mode == PILLOW_C_MODE_PA && source->channels == 2;
    if (source_is_p || source_is_pa) {
        const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t palette_index = source->pixels[i * source->channels];
            std::uint8_t rgb[3]{};
            palette_rgb_at(source, palette_index, rgb);
            target->pixels[i] = rgb_luma_1000(rgb) >= 128000 ? 255u : 0u;
        }
        return PILLOW_C_OK;
    }
    if (dither == 3) {
        return convert_image_to_mode1_floyd_steinberg_into(source, target);
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    const std::uint8_t* src = source->pixels.data();
    std::uint8_t* dst = target->pixels.data();
    if (source->mode == PILLOW_C_MODE_L) {
        for (std::size_t i = 0; i < pixels; ++i) {
            dst[i] = src[i] >= 128u ? 255u : 0u;
        }
        return PILLOW_C_OK;
    }
    if (source->mode == PILLOW_C_MODE_LA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            dst[i] = src[i * 2u] >= 128u ? 255u : 0u;
        }
        return PILLOW_C_OK;
    }
    if (source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t value = source->mode == PILLOW_C_MODE_I
                ? clip_u8_int(static_cast<int>(pillow_c_read_i32_le(src + i * 4u)))
                : clip_numeric_f32_to_l(pillow_c_read_f32_le(src + i * 4u));
            dst[i] = value >= 128u ? 255u : 0u;
        }
        return PILLOW_C_OK;
    }
    if (source->mode == PILLOW_C_MODE_RGB ||
        source->mode == PILLOW_C_MODE_RGBA ||
        source->mode == PILLOW_C_MODE_RGBX) {
        for (std::size_t i = 0; i < pixels; ++i) {
            dst[i] = rgb_luma_1000(src + i * source->channels) >= 128000 ? 255u : 0u;
        }
        return PILLOW_C_OK;
    }
    if (source->mode == PILLOW_C_MODE_HSV) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t rgb[3];
            hsv_to_rgb_u8(src + i * 3u, rgb);
            dst[i] = rgb_luma_1000(rgb) >= 128000 ? 255u : 0u;
        }
        return PILLOW_C_OK;
    }
    if (source->mode == PILLOW_C_MODE_YCBCR) {
        for (std::size_t i = 0; i < pixels; ++i) {
            std::uint8_t rgb[3];
            ycbcr_to_rgb_u8(src + i * 3u, rgb);
            dst[i] = rgb_luma_1000(rgb) >= 128000 ? 255u : 0u;
        }
        return PILLOW_C_OK;
    }
    if (source->mode == PILLOW_C_MODE_CMYK) {
        for (std::size_t i = 0; i < pixels; ++i) {
            dst[i] = cmyk_luma_u8(src + i * 4u) >= 128u ? 255u : 0u;
        }
        return PILLOW_C_OK;
    }
    return PILLOW_C_INVALID_ARGUMENT;
}

int merge_bands_into(int target_mode, const PillowCImage* const* bands, std::size_t band_count, PillowCImage* target)
{
    if (!bands || !target) {
        return PILLOW_C_NULL_POINTER;
    }

    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0 || band_count != static_cast<std::size_t>(target_channels)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const PillowCImage* first = bands[0];
    if (!first) {
        return PILLOW_C_NULL_POINTER;
    }
    if (first->mode != PILLOW_C_MODE_L || first->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    for (std::size_t channel = 1; channel < band_count; ++channel) {
        const PillowCImage* band = bands[channel];
        if (!band) {
            return PILLOW_C_NULL_POINTER;
        }
        if (band->mode != PILLOW_C_MODE_L || band->channels != 1) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (band->width != first->width || band->height != first->height) {
            return PILLOW_C_MISMATCH;
        }
    }

    if (!pillow_c_image_shape_matches(target, first->width, first->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }

    for (std::size_t channel = 0; channel < band_count; ++channel) {
        const int refresh_status = pillow_c_refresh_const_buffer_view_image(bands[channel]);
        if (refresh_status != PILLOW_C_OK) {
            return refresh_status;
        }
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::size_t pixels = static_cast<std::size_t>(first->width) * first->height;
    for (std::size_t i = 0; i < pixels; ++i) {
        std::uint8_t* dst = target->pixels.data() + i * target_channels;
        for (int channel = 0; channel < target_channels; ++channel) {
            dst[channel] = bands[channel]->pixels[i];
        }
    }
    return PILLOW_C_OK;
}

void free_image_array(PillowCImage** images, std::size_t count)
{
    if (!images) {
        return;
    }
    for (std::size_t i = 0; i < count; ++i) {
        delete images[i];
        images[i] = nullptr;
    }
}

} // namespace

bool pillow_c_ops_supports_imageops_lut(const PillowCImage* source)
{
    return supports_imageops_lut(source);
}

int pillow_c_ops_apply_point_lut_into(
    const PillowCImage* source,
    const std::uint8_t* lut,
    std::size_t lut_size,
    PillowCImage* target)
{
    return apply_point_lut_into(source, lut, lut_size, target);
}

int pillow_c_ops_apply_single_lut_into(
    const PillowCImage* source,
    const std::uint8_t* lut,
    std::size_t lut_size,
    PillowCImage* target)
{
    return apply_single_lut_into(source, lut, lut_size, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_fill(
    PillowCImage* image,
    const std::uint8_t* color,
    std::size_t color_size)
{
    const int detach_status = pillow_c_detach_buffer_view_image(image);
    if (detach_status != PILLOW_C_OK) {
        return detach_status;
    }
    return pillow_c_fill_image_pixels(image, color, color_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_getpixel(
    const PillowCImage* image,
    int x,
    int y,
    std::uint8_t* out_color,
    std::size_t out_color_size)
{
    if (!image || !out_color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    std::size_t offset = 0;
    const int offset_status = pillow_c_image_pixel_offset(image, x, y, &offset);
    if (offset_status != PILLOW_C_OK) {
        return offset_status;
    }
    std::memcpy(out_color, image->pixels.data() + offset, out_color_size);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_putpixel(
    PillowCImage* image,
    int x,
    int y,
    const std::uint8_t* color,
    std::size_t color_size)
{
    if (!image || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != 1u && color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    const int detach_status = pillow_c_detach_buffer_view_image(image);
    if (detach_status != PILLOW_C_OK) {
        return detach_status;
    }
    std::size_t offset = 0;
    const int offset_status = pillow_c_image_pixel_offset(image, x, y, &offset);
    if (offset_status != PILLOW_C_OK) {
        return offset_status;
    }
    std::uint8_t* dst = image->pixels.data() + offset;
    if (color_size == 1u && image->channels > 1) {
        dst[0] = color[0];
        std::fill(dst + 1, dst + image->channels, static_cast<std::uint8_t>(0));
    } else {
        std::memcpy(dst, color, color_size);
    }
    return PILLOW_C_OK;
}

int pillow_c_quantize_exact_image_into(
    const PillowCImage* source,
    int colors,
    PillowCImage* target)
{
    return quantize_exact_image_into(source, colors, target);
}

int pillow_c_quantize_exact_rgba_gif_into(
    const PillowCImage* source,
    PillowCImage* target,
    bool* out_has_transparency,
    int* out_transparency)
{
    return quantize_exact_rgba_gif_into(source, target, out_has_transparency, out_transparency);
}

int pillow_c_quantize_median_cut_rgba_gif_into(
    const PillowCImage* source,
    PillowCImage* target,
    bool* out_has_transparency,
    int* out_transparency)
{
    return quantize_median_cut_rgba_gif_into(source, target, out_has_transparency, out_transparency);
}

int pillow_c_quantize_exact_rgba_gif_animation_frame_into(
    const PillowCImage* source,
    PillowCImage* target,
    bool reserve_transparency,
    bool* out_has_transparency)
{
    return quantize_exact_rgba_gif_animation_frame_into(
        source,
        target,
        reserve_transparency,
        out_has_transparency);
}

int pillow_c_quantize_exact_la_gif_animation_frame_into(
    const PillowCImage* source,
    PillowCImage* target)
{
    return quantize_exact_la_gif_animation_frame_into(source, target);
}

int pillow_c_paste_image_pixels_into(
    PillowCImage* target,
    const PillowCImage* source,
    int left,
    int top)
{
    return paste_image_pixels_into(target, source, left, top);
}

int pillow_c_fill_image_pixels(
    PillowCImage* image,
    const std::uint8_t* color,
    std::size_t color_size)
{
    return fill_image_pixels(image, color, color_size);
}

int pillow_c_copy_transpose_pixels_into(
    const PillowCImage* source,
    int method,
    PillowCImage* target)
{
    return copy_transpose_pixels_into(source, method, target);
}

bool pillow_c_transpose_output_shape(
    const PillowCImage* source,
    int method,
    int* out_width,
    int* out_height)
{
    return transpose_output_shape(source, method, out_width, out_height);
}

int pillow_c_normalize_coordinate(int value, int limit, int* out_value)
{
    return normalize_coordinate(value, limit, out_value);
}

int pillow_c_image_pixel_offset(const PillowCImage* image, int x, int y, std::size_t* out_offset)
{
    return image_pixel_offset(image, x, y, out_offset);
}


extern "C" __declspec(dllexport) int pillow_c_blend_u8(
    const std::uint8_t* left,
    const std::uint8_t* right,
    std::uint8_t* out,
    std::size_t count,
    double alpha)
{
    if (!left || !right || !out) {
        return PILLOW_C_NULL_POINTER;
    }

    const float a = static_cast<float>(alpha);
    if (a == 0.0f) {
        for (std::size_t i = 0; i < count; ++i) {
            out[i] = left[i];
        }
        return PILLOW_C_OK;
    }
    if (a == 1.0f) {
        for (std::size_t i = 0; i < count; ++i) {
            out[i] = right[i];
        }
        return PILLOW_C_OK;
    }

    if (a >= 0.0f && a <= 1.0f) {
        for (std::size_t i = 0; i < count; ++i) {
            out[i] = static_cast<std::uint8_t>(
                static_cast<int>(left[i]) +
                a * (static_cast<int>(right[i]) - static_cast<int>(left[i])));
        }
        return PILLOW_C_OK;
    }

    for (std::size_t i = 0; i < count; ++i) {
        const float value = static_cast<float>(
            static_cast<int>(left[i]) +
            a * (static_cast<int>(right[i]) - static_cast<int>(left[i])));
        out[i] = clip_u8(value);
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_rgb_to_l(
    const std::uint8_t* rgb,
    std::uint8_t* out,
    std::size_t pixels)
{
    if (!rgb || !out) {
        return PILLOW_C_NULL_POINTER;
    }

    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint8_t* px = rgb + i * 3;
        out[i] = rgb_luma_u8(px);
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_alpha_composite_rgba(
    const std::uint8_t* dst,
    const std::uint8_t* src,
    std::uint8_t* out,
    std::size_t pixels)
{
    if (!dst || !src || !out) {
        return PILLOW_C_NULL_POINTER;
    }

    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint8_t* d = dst + i * 4;
        const std::uint8_t* s = src + i * 4;
        std::uint8_t* o = out + i * 4;
        alpha_composite_pixel_rgba(d, s, o);
    }

    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_crop(
    const PillowCImage* source,
    int left,
    int top,
    int right,
    int bottom,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    if (right < left || bottom < top) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const std::int64_t out_width_i64 = static_cast<std::int64_t>(right) - left;
    const std::int64_t out_height_i64 = static_cast<std::int64_t>(bottom) - top;
    if (out_width_i64 > INT_MAX || out_height_i64 > INT_MAX) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int out_width = static_cast<int>(out_width_i64);
    const int out_height = static_cast<int>(out_height_i64);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, source->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            source->mode,
            source->channels,
            stride,
            std::vector<std::uint8_t>(size)};

        const int copy_left = left < 0 ? 0 : left;
        const int copy_top = top < 0 ? 0 : top;
        const int copy_right = right > source->width ? source->width : right;
        const int copy_bottom = bottom > source->height ? source->height : bottom;

        if (copy_right > copy_left && copy_bottom > copy_top) {
            const std::size_t row_bytes =
                static_cast<std::size_t>(copy_right - copy_left) * source->channels;
            for (int y = copy_top; y < copy_bottom; ++y) {
                const int dst_y = y - top;
                const int dst_x = copy_left - left;
                const std::size_t src_offset =
                    static_cast<std::size_t>(y) * source->stride +
                    static_cast<std::size_t>(copy_left) * source->channels;
                const std::size_t dst_offset =
                    static_cast<std::size_t>(dst_y) * image->stride +
                    static_cast<std::size_t>(dst_x) * image->channels;
                std::memcpy(image->pixels.data() + dst_offset, source->pixels.data() + src_offset, row_bytes);
            }
        }

        pillow_c_copy_palette_if_same_mode(source, image);
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_expand(
    const PillowCImage* source,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* color,
    std::size_t color_size,
    PillowCImage** out_image)
{
    if (!source || !color || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (color_size != static_cast<std::size_t>(source->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }

    int out_width = 0;
    int out_height = 0;
    const int size_status = output_size_from_borders(source, left, top, right, bottom, &out_width, &out_height);
    if (size_status != PILLOW_C_OK) {
        return size_status;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, source->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            source->mode,
            source->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = expand_image_into(source, left, top, right, bottom, color, color_size, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_offset(
    const PillowCImage* source,
    int x_offset,
    int y_offset,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, source->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            source->mode,
            source->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = offset_image_into(source, x_offset, y_offset, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_paste(
    PillowCImage* target,
    const PillowCImage* source,
    int left,
    int top)
{
    return ops_with_detached_buffer_view(target, [&]() {
        return paste_image_pixels_into(target, source, left, top);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_paste_masked(
    PillowCImage* target,
    const PillowCImage* source,
    int left,
    int top,
    const PillowCImage* mask)
{
    return ops_with_detached_buffer_view(target, [&]() {
        return paste_image_masked_into(target, source, left, top, mask);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_paste_color(
    PillowCImage* target,
    const std::uint8_t* color,
    std::size_t color_size,
    int left,
    int top,
    int right,
    int bottom,
    const PillowCImage* mask)
{
    return ops_with_detached_buffer_view(target, [&]() {
        return paste_color_into(target, color, color_size, left, top, right, bottom, mask);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_copy_into(
    const PillowCImage* source,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_image_shape_matches(source, target)) {
        return PILLOW_C_MISMATCH;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if (!source->pixels.empty()) {
        std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
    }
    target->palette_rgb = source->palette_rgb;
    target->palette_alpha = source->palette_alpha;
    target->palette_alpha_mode = source->palette_alpha_mode;
    target->exif_orientation = source->exif_orientation;
    target->has_dpi = source->has_dpi;
    target->dpi_x = source->dpi_x;
    target->dpi_y = source->dpi_y;
    target->has_jfif = source->has_jfif;
    target->jfif_major = source->jfif_major;
    target->jfif_minor = source->jfif_minor;
    target->jfif_unit = source->jfif_unit;
    target->jfif_density_x = source->jfif_density_x;
    target->jfif_density_y = source->jfif_density_y;
    target->has_hotspot = source->has_hotspot;
    target->hotspot_x = source->hotspot_x;
    target->hotspot_y = source->hotspot_y;
    target->has_dib_compression = source->has_dib_compression;
    target->dib_compression = source->dib_compression;
    target->has_png_gamma = source->has_png_gamma;
    target->png_gamma = source->png_gamma;
    target->has_png_srgb = source->has_png_srgb;
    target->png_srgb = source->png_srgb;
    target->has_png_chromaticity = source->has_png_chromaticity;
    for (std::size_t i = 0; i < 8u; ++i) {
        target->png_chromaticity[i] = source->png_chromaticity[i];
    }
    target->png_text = source->png_text;
    target->png_icc_profile = source->png_icc_profile;
    target->png_exif = source->png_exif;
    target->tiff_exif = source->tiff_exif;
    target->tiff_icc_profile = source->tiff_icc_profile;
    target->jpeg_comment = source->jpeg_comment;
    target->has_jpeg_icc_profile = source->has_jpeg_icc_profile;
    target->has_jpeg_icc_profile_none = source->has_jpeg_icc_profile_none;
    target->jpeg_icc_profile = source->jpeg_icc_profile;
    target->jpeg_photoshop_resources = source->jpeg_photoshop_resources;
    target->has_jpeg_photoshop_resolution_info =
        source->has_jpeg_photoshop_resolution_info;
    target->jpeg_photoshop_x_resolution = source->jpeg_photoshop_x_resolution;
    target->jpeg_photoshop_displayed_units_x =
        source->jpeg_photoshop_displayed_units_x;
    target->jpeg_photoshop_y_resolution = source->jpeg_photoshop_y_resolution;
    target->jpeg_photoshop_displayed_units_y =
        source->jpeg_photoshop_displayed_units_y;
    target->jpeg_exif = source->jpeg_exif;
    target->xmp = source->xmp;
    target->jpeg_qtables = source->jpeg_qtables;
    target->jpeg_qtable_count = source->jpeg_qtable_count;
    target->jpeg_subsampling = source->jpeg_subsampling;
    target->has_png_transparency = source->has_png_transparency;
    target->png_transparency = source->png_transparency;
    target->png_transparency_table = source->png_transparency_table;
    target->has_png_rgb_transparency = source->has_png_rgb_transparency;
    target->png_rgb_transparency[0] = source->png_rgb_transparency[0];
    target->png_rgb_transparency[1] = source->png_rgb_transparency[1];
    target->png_rgb_transparency[2] = source->png_rgb_transparency[2];
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_constant_into(
    const PillowCImage* source,
    int value,
    PillowCImage* target)
{
    return constant_image_into(source, value, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_chops_invert_into(
    const PillowCImage* source,
    PillowCImage* target)
{
    return chops_invert_image_into(source, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_blend_into(
    const PillowCImage* left,
    const PillowCImage* right,
    double alpha,
    PillowCImage* target)
{
    if (!left || !right || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_image_shapes_match(left, right) || !pillow_c_image_shape_matches(target, left)) {
        return PILLOW_C_MISMATCH;
    }
    if (numeric_i_or_f_mode(left)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int left_refresh_status = pillow_c_refresh_const_buffer_view_image(left);
    if (left_refresh_status != PILLOW_C_OK) {
        return left_refresh_status;
    }
    const int right_refresh_status = pillow_c_refresh_const_buffer_view_image(right);
    if (right_refresh_status != PILLOW_C_OK) {
        return right_refresh_status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }
    return pillow_c_blend_u8(
        left->pixels.data(),
        right->pixels.data(),
        target->pixels.data(),
        target->pixels.size(),
        alpha);
}

extern "C" __declspec(dllexport) int pillow_c_image_composite_into(
    const PillowCImage* source,
    const PillowCImage* target_source,
    const PillowCImage* mask,
    PillowCImage* target)
{
    return composite_image_into(source, target_source, mask, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_difference_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            const int delta = static_cast<int>(left_row[x]) - static_cast<int>(right_row[x]);
            target_row[x] = static_cast<std::uint8_t>(delta < 0 ? -delta : delta);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_multiply_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            target_row[x] = static_cast<std::uint8_t>(
                static_cast<unsigned int>(left_row[x]) * static_cast<unsigned int>(right_row[x]) / 255u);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_screen_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            const unsigned int left_inv = 255u - left_row[x];
            const unsigned int right_inv = 255u - right_row[x];
            target_row[x] = static_cast<std::uint8_t>(255u - (left_inv * right_inv / 255u));
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_lighter_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            target_row[x] = std::max(left_row[x], right_row[x]);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_darker_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            target_row[x] = std::min(left_row[x], right_row[x]);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_soft_light_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            target_row[x] = soft_light_u8(left_row[x], right_row[x]);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_hard_light_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            target_row[x] = hard_light_u8(left_row[x], right_row[x]);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_overlay_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            target_row[x] = overlay_u8(left_row[x], right_row[x]);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_add_into(
    const PillowCImage* left,
    const PillowCImage* right,
    double scale,
    double offset,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }
    if (scale == 0.0 || !std::isfinite(scale)) {
        std::fill(target->pixels.begin(), target->pixels.end(), std::uint8_t{0});
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            const double value =
                (static_cast<int>(left_row[x]) + static_cast<int>(right_row[x])) / scale + offset;
            target_row[x] = clip_chops_scaled_u8(value);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_subtract_into(
    const PillowCImage* left,
    const PillowCImage* right,
    double scale,
    double offset,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }
    if (scale == 0.0 || !std::isfinite(scale)) {
        std::fill(target->pixels.begin(), target->pixels.end(), std::uint8_t{0});
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            const double value =
                (static_cast<int>(left_row[x]) - static_cast<int>(right_row[x])) / scale + offset;
            target_row[x] = clip_chops_scaled_u8(value);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_add_modulo_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            target_row[x] = static_cast<std::uint8_t>(left_row[x] + right_row[x]);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_subtract_modulo_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int channels = left->channels;
    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width * channels; ++x) {
            target_row[x] = static_cast<std::uint8_t>(left_row[x] - right_row[x]);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_logical_and_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (left->mode != PILLOW_C_MODE_1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width; ++x) {
            target_row[x] = (left_row[x] != 0 && right_row[x] != 0) ? 255u : 0u;
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_logical_or_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (left->mode != PILLOW_C_MODE_1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width; ++x) {
            target_row[x] = (left_row[x] != 0 || right_row[x] != 0) ? 255u : 0u;
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_logical_xor_into(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = validate_chops_binary_target(left, right, target, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (left->mode != PILLOW_C_MODE_1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    for (int y = 0; y < out_height; ++y) {
        const std::uint8_t* left_row = left->pixels.data() + static_cast<std::size_t>(y) * left->stride;
        const std::uint8_t* right_row = right->pixels.data() + static_cast<std::size_t>(y) * right->stride;
        std::uint8_t* target_row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < out_width; ++x) {
            target_row[x] = ((left_row[x] != 0) != (right_row[x] != 0)) ? 255u : 0u;
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_rgb_to_l_into(
    const PillowCImage* source,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (source->channels != 3) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_L, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }
    return pillow_c_rgb_to_l(
        source->pixels.data(),
        target->pixels.data(),
        static_cast<std::size_t>(source->width) * source->height);
}

extern "C" __declspec(dllexport) int pillow_c_image_point_lut_into(
    const PillowCImage* source,
    const std::uint8_t* lut,
    std::size_t lut_size,
    PillowCImage* target)
{
    return apply_point_lut_into(source, lut, lut_size, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_point_lut_mode_into(
    const PillowCImage* source,
    const std::uint8_t* lut,
    std::size_t lut_size,
    int target_mode,
    PillowCImage* target)
{
    return apply_point_lut_mode_into(source, lut, lut_size, target_mode, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_invert_into(
    const PillowCImage* source,
    PillowCImage* target)
{
    return invert_image_into(source, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_posterize_into(
    const PillowCImage* source,
    int bits,
    PillowCImage* target)
{
    return posterize_image_into(source, bits, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_solarize_into(
    const PillowCImage* source,
    double threshold,
    PillowCImage* target)
{
    if (!std::isfinite(threshold)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return solarize_image_into(source, threshold, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_colorize_into(
    const PillowCImage* source,
    const std::uint8_t* black,
    const std::uint8_t* white,
    int has_mid,
    const std::uint8_t* mid,
    int blackpoint,
    int whitepoint,
    int midpoint,
    PillowCImage* target)
{
    return colorize_image_into(
        source,
        black,
        white,
        has_mid != 0,
        mid,
        blackpoint,
        whitepoint,
        midpoint,
        target);
}

extern "C" __declspec(dllexport) int pillow_c_image_equalize_into(
    const PillowCImage* source,
    PillowCImage* target)
{
    return equalize_image_into(source, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_equalize_masked_into(
    const PillowCImage* source,
    const PillowCImage* mask,
    PillowCImage* target)
{
    return equalize_image_masked_into(source, mask, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_get_channel_into(
    const PillowCImage* source,
    int channel_index,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (channel_index < 0 || channel_index >= source->channels) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return copy_channel_into(source, channel_index, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_put_alpha_value_into(
    const PillowCImage* source,
    std::uint8_t alpha,
    PillowCImage* target)
{
    return put_alpha_value_into(source, alpha, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_put_alpha_image_into(
    const PillowCImage* source,
    const PillowCImage* alpha,
    PillowCImage* target)
{
    return put_alpha_image_into(source, alpha, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_convert_mode_into(
    const PillowCImage* source,
    int target_mode,
    PillowCImage* target)
{
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return convert_image_mode_into(source, target_mode, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_convert_mode_dither_into(
    const PillowCImage* source,
    int target_mode,
    int dither,
    PillowCImage* target)
{
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return convert_image_mode_dither_into(source, target_mode, dither, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_convert_matrix_into(
    const PillowCImage* source,
    int target_mode,
    const double* matrix,
    std::size_t matrix_count,
    PillowCImage* target)
{
    return convert_matrix_image_into(source, target_mode, matrix, matrix_count, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_quantize_palette_into(
    const PillowCImage* source,
    const PillowCImage* palette,
    PillowCImage* target)
{
    return quantize_palette_image_into(source, palette, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_quantize_into(
    const PillowCImage* source,
    int colors,
    PillowCImage* target)
{
    return quantize_exact_image_into(source, colors, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_merge_bands_into(
    int target_mode,
    const PillowCImage* const* bands,
    std::size_t band_count,
    PillowCImage* target)
{
    return merge_bands_into(target_mode, bands, band_count, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_split_bands(
    const PillowCImage* source,
    PillowCImage** out_bands,
    std::size_t out_count)
{
    if (!source || !out_bands) {
        return PILLOW_C_NULL_POINTER;
    }
    for (std::size_t i = 0; i < out_count; ++i) {
        out_bands[i] = nullptr;
    }
    if (out_count != static_cast<std::size_t>(source->channels)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        const int target_mode = channel_target_mode_for_source(source);
        for (std::size_t channel = 0; channel < out_count; ++channel) {
            auto* image = new PillowCImage{
                source->width,
                source->height,
                target_mode,
                1,
                stride,
                std::vector<std::uint8_t>(size)};
            const int status = copy_channel_into(source, static_cast<int>(channel), image);
            if (status != PILLOW_C_OK) {
                delete image;
                free_image_array(out_bands, channel);
                return status;
            }
            out_bands[channel] = image;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        free_image_array(out_bands, out_count);
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_alpha_composite_rgba_into(
    const PillowCImage* dst,
    const PillowCImage* src,
    PillowCImage* target)
{
    if (!dst || !src || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_image_shapes_match(dst, src) || dst->channels != 4 || !pillow_c_image_shape_matches(target, dst)) {
        return PILLOW_C_MISMATCH;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }
    return pillow_c_alpha_composite_rgba(
        dst->pixels.data(),
        src->pixels.data(),
        target->pixels.data(),
        static_cast<std::size_t>(dst->width) * dst->height);
}

extern "C" __declspec(dllexport) int pillow_c_image_alpha_composite_rgba_in_place(
    PillowCImage* dst,
    const PillowCImage* src,
    int dest_x,
    int dest_y,
    int source_left,
    int source_top,
    int source_right,
    int source_bottom)
{
    if (!dst || !src) {
        return PILLOW_C_NULL_POINTER;
    }
    if (dst->mode != PILLOW_C_MODE_RGBA || src->mode != PILLOW_C_MODE_RGBA ||
        dst->channels != 4 || src->channels != 4) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source_left < 0 || source_top < 0 || source_right < source_left || source_bottom < source_top) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int overlay_width = source_right - source_left;
    const int overlay_height = source_bottom - source_top;
    if (overlay_width == 0 || overlay_height == 0 || dst->width == 0 || dst->height == 0) {
        return PILLOW_C_OK;
    }

    const int dest_right = dest_x + overlay_width;
    const int dest_bottom = dest_y + overlay_height;
    const int visible_left = std::max(dest_x, 0);
    const int visible_top = std::max(dest_y, 0);
    const int visible_right = std::min(dest_right, dst->width);
    const int visible_bottom = std::min(dest_bottom, dst->height);
    if (visible_left >= visible_right || visible_top >= visible_bottom) {
        return PILLOW_C_OK;
    }

    std::vector<std::uint8_t> source_copy;
    const std::uint8_t* source_pixels = src->pixels.empty() ? nullptr : src->pixels.data();
    if (dst == src && !src->pixels.empty()) {
        try {
            source_copy = src->pixels;
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
        source_pixels = source_copy.data();
    }

    for (int y = visible_top; y < visible_bottom; ++y) {
        const int overlay_y = y - dest_y;
        const int source_y = source_top + overlay_y;
        if (source_y < 0 || source_y >= src->height) {
            continue;
        }
        for (int x = visible_left; x < visible_right; ++x) {
            const int overlay_x = x - dest_x;
            const int source_x = source_left + overlay_x;
            if (source_x < 0 || source_x >= src->width) {
                continue;
            }
            std::uint8_t* dst_pixel = dst->pixels.data() +
                static_cast<std::size_t>(y) * dst->stride +
                static_cast<std::size_t>(x) * 4u;
            const std::uint8_t* src_pixel = source_pixels +
                static_cast<std::size_t>(source_y) * src->stride +
                static_cast<std::size_t>(source_x) * 4u;
            alpha_composite_pixel_rgba(dst_pixel, src_pixel, dst_pixel);
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_crop_into(
    const PillowCImage* source,
    int left,
    int top,
    int right,
    int bottom,
    PillowCImage* target)
{
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return copy_crop_pixels_into(source, left, top, right, bottom, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_expand_into(
    const PillowCImage* source,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* color,
    std::size_t color_size,
    PillowCImage* target)
{
    return expand_image_into(source, left, top, right, bottom, color, color_size, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_offset_into(
    const PillowCImage* source,
    int x_offset,
    int y_offset,
    PillowCImage* target)
{
    return offset_image_into(source, x_offset, y_offset, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_transpose_into(
    const PillowCImage* source,
    int method,
    PillowCImage* target)
{
    return copy_transpose_pixels_into(source, method, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_transpose(
    const PillowCImage* source,
    int method,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (method < 0 || method > 6) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const bool swaps_axes = method == 2 || method == 4 || method == 5 || method == 6;
    const int out_width = swaps_axes ? source->height : source->width;
    const int out_height = swaps_axes ? source->width : source->height;
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, source->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            source->mode,
            source->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = copy_transpose_pixels_into(source, method, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_remap_palette_into(
    const PillowCImage* source,
    const int* dest_map,
    std::size_t dest_count,
    const std::uint8_t* source_palette,
    std::size_t source_palette_size,
    PillowCImage* target)
{
    return remap_palette_image_into(source, dest_map, dest_count, source_palette, source_palette_size, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_remap_palette(
    const PillowCImage* source,
    const int* dest_map,
    std::size_t dest_count,
    const std::uint8_t* source_palette,
    std::size_t source_palette_size,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if ((source->mode != PILLOW_C_MODE_P && source->mode != PILLOW_C_MODE_L) || source->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_P,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = remap_palette_image_into(source, dest_map, dest_count, source_palette, source_palette_size, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_put_palette_rgb(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size)
{
    if (!image || (!data && size > 0)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!(((image->mode == PILLOW_C_MODE_P || image->mode == PILLOW_C_MODE_L) && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_PA && image->channels == 2)) ||
        size % 3u != 0 ||
        size > 768u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        if (image->mode == PILLOW_C_MODE_L) {
            image->mode = PILLOW_C_MODE_P;
        }
        image->palette_rgb.assign(data, data + size);
        image->palette_alpha.clear();
        image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_put_palette_rgba(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size,
    int alpha_mode)
{
    if (!image || (!data && size > 0)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!(((image->mode == PILLOW_C_MODE_P || image->mode == PILLOW_C_MODE_L) && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_PA && image->channels == 2)) ||
        size % 4u != 0 ||
        size > 1024u ||
        alpha_mode < PILLOW_C_PALETTE_ALPHA_NONE ||
        alpha_mode > PILLOW_C_PALETTE_ALPHA_RGBX) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        if (image->mode == PILLOW_C_MODE_L) {
            image->mode = PILLOW_C_MODE_P;
        }
        image->palette_rgb.clear();
        image->palette_alpha.clear();
        image->palette_alpha_mode = alpha_mode;
        const std::size_t entries = size / 4u;
        image->palette_rgb.reserve(entries * 3u);
        if (alpha_mode != PILLOW_C_PALETTE_ALPHA_NONE) {
            image->palette_alpha.reserve(entries);
        }
        for (std::size_t index = 0; index < entries; ++index) {
            const std::size_t src = index * 4u;
            image->palette_rgb.push_back(data[src + 0u]);
            image->palette_rgb.push_back(data[src + 1u]);
            image->palette_rgb.push_back(data[src + 2u]);
            if (alpha_mode != PILLOW_C_PALETTE_ALPHA_NONE) {
                image->palette_alpha.push_back(data[src + 3u]);
            }
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_get_palette_rgb(
    const PillowCImage* image,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!image || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!((image->mode == PILLOW_C_MODE_P && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_PA && image->channels == 2))) {
        *out_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t required = image->palette_rgb.size();
    *out_required = required;
    if (!out) {
        return PILLOW_C_OK;
    }
    if (out_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (required > 0) {
        std::memcpy(out, image->palette_rgb.data(), required);
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_get_palette_rgba(
    const PillowCImage* image,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!image || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!((image->mode == PILLOW_C_MODE_P && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_PA && image->channels == 2))) {
        *out_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->palette_rgb.size() % 3u != 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t entries = image->palette_rgb.size() / 3u;
    const std::size_t required = entries * 4u;
    *out_required = required;
    if (!out) {
        return PILLOW_C_OK;
    }
    if (out_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    for (std::size_t index = 0; index < entries; ++index) {
        const std::size_t rgb_offset = index * 3u;
        const std::size_t rgba_offset = index * 4u;
        out[rgba_offset + 0u] = image->palette_rgb[rgb_offset + 0u];
        out[rgba_offset + 1u] = image->palette_rgb[rgb_offset + 1u];
        out[rgba_offset + 2u] = image->palette_rgb[rgb_offset + 2u];
        out[rgba_offset + 3u] =
            index < image->palette_alpha.size() ? image->palette_alpha[index] : std::uint8_t{255};
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_palette_alpha_mode(
    const PillowCImage* image,
    int* out_mode)
{
    if (!image || !out_mode) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!((image->mode == PILLOW_C_MODE_P && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_PA && image->channels == 2))) {
        *out_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_mode = image->palette_alpha_mode;
    return PILLOW_C_OK;
}


extern "C" __declspec(dllexport) int pillow_c_image_copy(
    const PillowCImage* source,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    try {
        auto* image = new PillowCImage{source->width, source->height, source->mode, source->channels, source->stride, source->pixels, source->palette_rgb, source->exif_orientation};
        image->palette_alpha = source->palette_alpha;
        image->palette_alpha_mode = source->palette_alpha_mode;
        image->has_png_transparency = source->has_png_transparency;
        image->png_transparency = source->png_transparency;
        image->png_transparency_table = source->png_transparency_table;
        image->has_png_rgb_transparency = source->has_png_rgb_transparency;
        image->png_rgb_transparency[0] = source->png_rgb_transparency[0];
        image->png_rgb_transparency[1] = source->png_rgb_transparency[1];
        image->png_rgb_transparency[2] = source->png_rgb_transparency[2];
        image->has_hotspot = source->has_hotspot;
        image->hotspot_x = source->hotspot_x;
        image->hotspot_y = source->hotspot_y;
        image->has_dib_compression = source->has_dib_compression;
        image->dib_compression = source->dib_compression;
        image->tiff_exif = source->tiff_exif;
        image->tiff_icc_profile = source->tiff_icc_profile;
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_constant(
    const PillowCImage* source,
    int value,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_L,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = constant_image_into(source, value, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_chops_invert(
    const PillowCImage* source,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            source->mode,
            source->channels,
            source->stride,
            std::vector<std::uint8_t>(source->pixels.size())};
        const int status = chops_invert_image_into(source, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_blend(
    const PillowCImage* left,
    const PillowCImage* right,
    double alpha,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!pillow_c_image_shapes_match(left, right)) {
        return PILLOW_C_MISMATCH;
    }
    if (numeric_i_or_f_mode(left)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int left_refresh_status = pillow_c_refresh_const_buffer_view_image(left);
    if (left_refresh_status != PILLOW_C_OK) {
        return left_refresh_status;
    }
    const int right_refresh_status = pillow_c_refresh_const_buffer_view_image(right);
    if (right_refresh_status != PILLOW_C_OK) {
        return right_refresh_status;
    }

    try {
        auto* image = new PillowCImage{left->width, left->height, left->mode, left->channels, left->stride, std::vector<std::uint8_t>(left->pixels.size())};
        const int status = pillow_c_blend_u8(
            left->pixels.data(),
            right->pixels.data(),
            image->pixels.data(),
            image->pixels.size(),
            alpha);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_composite(
    const PillowCImage* source,
    const PillowCImage* target_source,
    const PillowCImage* mask,
    PillowCImage** out_image)
{
    if (!source || !target_source || !mask || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!pillow_c_supported_composite_mask(mask)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (mask->width != source->width || mask->height != source->height) {
        return PILLOW_C_MISMATCH;
    }

    try {
        auto* image = new PillowCImage{
            target_source->width,
            target_source->height,
            target_source->mode,
            target_source->channels,
            target_source->stride,
            std::vector<std::uint8_t>(target_source->pixels.size())};
        const int status = composite_image_into(source, target_source, mask, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_difference(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_difference_into(left, right, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_multiply(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_multiply_into(left, right, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_screen(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_screen_into(left, right, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_lighter(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_lighter_into(left, right, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_darker(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_darker_into(left, right, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_soft_light(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_soft_light_into(left, right, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_hard_light(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_hard_light_into(left, right, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_overlay(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_overlay_into(left, right, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_add(
    const PillowCImage* left,
    const PillowCImage* right,
    double scale,
    double offset,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_add_into(left, right, scale, offset, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_subtract(
    const PillowCImage* left,
    const PillowCImage* right,
    double scale,
    double offset,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_subtract_into(left, right, scale, offset, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_add_modulo(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_add_modulo_into(left, right, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_subtract_modulo(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_subtract_modulo_into(left, right, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int allocate_chops_binary_image(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!left || !right || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (left->mode != right->mode || left->channels != right->channels) {
        return PILLOW_C_MISMATCH;
    }

    const int out_width = overlapping_width(left, right);
    const int out_height = overlapping_height(left, right);
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(out_width, out_height, left->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            out_width,
            out_height,
            left->mode,
            left->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_logical_and(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    PillowCImage* image = nullptr;
    const int allocate_status = allocate_chops_binary_image(left, right, &image);
    if (allocate_status != PILLOW_C_OK) {
        return allocate_status;
    }
    const int status = pillow_c_image_logical_and_into(left, right, image);
    if (status != PILLOW_C_OK) {
        delete image;
        *out_image = nullptr;
        return status;
    }
    *out_image = image;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_logical_or(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    PillowCImage* image = nullptr;
    const int allocate_status = allocate_chops_binary_image(left, right, &image);
    if (allocate_status != PILLOW_C_OK) {
        return allocate_status;
    }
    const int status = pillow_c_image_logical_or_into(left, right, image);
    if (status != PILLOW_C_OK) {
        delete image;
        *out_image = nullptr;
        return status;
    }
    *out_image = image;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_logical_xor(
    const PillowCImage* left,
    const PillowCImage* right,
    PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    PillowCImage* image = nullptr;
    const int allocate_status = allocate_chops_binary_image(left, right, &image);
    if (allocate_status != PILLOW_C_OK) {
        return allocate_status;
    }
    const int status = pillow_c_image_logical_xor_into(left, right, image);
    if (status != PILLOW_C_OK) {
        delete image;
        *out_image = nullptr;
        return status;
    }
    *out_image = image;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_rgb_to_l(
    const PillowCImage* source,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (source->channels != 3) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_L,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = pillow_c_image_rgb_to_l_into(source, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_point_lut(
    const PillowCImage* source,
    const std::uint8_t* lut,
    std::size_t lut_size,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            source->mode,
            source->channels,
            source->stride,
            std::vector<std::uint8_t>(source->pixels.size())};
        const int status = apply_point_lut_into(source, lut, lut_size, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_point_transform(
    const PillowCImage* source,
    double scale,
    double offset,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (source->mode != PILLOW_C_MODE_I || source->channels != 4) {
        // Pillow's point_transform path only serves the numeric I/F/I;16
        // callable route; the bounded surface here is mode I.
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            source->mode,
            source->channels,
            source->stride,
            std::vector<std::uint8_t>(source->pixels.size())};
        const std::size_t sample_count = source->pixels.size() / 4u;
        for (std::size_t index = 0u; index < sample_count; ++index) {
            const std::int32_t input = read_le_i32(
                source->pixels.data() + index * 4u);
            const double transformed = static_cast<double>(input) * scale + offset;
            // Mirror Pillow's C point_transform: the double result is cast
            // to int (truncation toward zero).
            const std::int32_t output = static_cast<std::int32_t>(transformed);
            const std::uint8_t* src = reinterpret_cast<const std::uint8_t*>(&output);
            std::memcpy(image->pixels.data() + index * 4u, src, 4u);
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_point_lut_mode(
    const PillowCImage* source,
    const std::uint8_t* lut,
    std::size_t lut_size,
    int target_mode,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const int target_channels = point_lut_target_channels(source, target_mode);
    if (target_channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, target_channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            target_mode,
            target_channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = apply_point_lut_mode_into(source, lut, lut_size, target_mode, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_invert(
    const PillowCImage* source,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!(supports_imageops_lut(source) || (source->mode == PILLOW_C_MODE_1 && source->channels == 1))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{source->width, source->height, source->mode, source->channels, source->stride, std::vector<std::uint8_t>(source->pixels.size())};
        const int status = invert_image_into(source, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_posterize(
    const PillowCImage* source,
    int bits,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supports_imageops_lut(source) || bits > 8) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{source->width, source->height, source->mode, source->channels, source->stride, std::vector<std::uint8_t>(source->pixels.size())};
        const int status = posterize_image_into(source, bits, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_solarize(
    const PillowCImage* source,
    double threshold,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supports_imageops_lut(source) || !std::isfinite(threshold)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{source->width, source->height, source->mode, source->channels, source->stride, std::vector<std::uint8_t>(source->pixels.size())};
        const int status = solarize_image_into(source, threshold, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_colorize(
    const PillowCImage* source,
    const std::uint8_t* black,
    const std::uint8_t* white,
    int has_mid,
    const std::uint8_t* mid,
    int blackpoint,
    int whitepoint,
    int midpoint,
    PillowCImage** out_image)
{
    if (!source || !black || !white || !out_image || (has_mid != 0 && !mid)) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (source->mode != PILLOW_C_MODE_L || source->channels != 1 ||
        !valid_colorize_points(has_mid != 0, blackpoint, whitepoint, midpoint)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, 3, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_RGB,
            3,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = colorize_image_into(
            source,
            black,
            white,
            has_mid != 0,
            mid,
            blackpoint,
            whitepoint,
            midpoint,
            image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_equalize(
    const PillowCImage* source,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supports_equalize_mode(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        const int target_mode = equalize_target_mode(source);
        const int target_channels = equalize_target_channels(source);
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size_allow_empty(source->width, source->height, target_channels, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            source->width,
            source->height,
            target_mode,
            target_channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = equalize_image_into(source, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_equalize_masked(
    const PillowCImage* source,
    const PillowCImage* mask,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supports_equalize_mode(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        const int target_mode = equalize_target_mode(source);
        const int target_channels = equalize_target_channels(source);
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size_allow_empty(source->width, source->height, target_channels, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            source->width,
            source->height,
            target_mode,
            target_channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = equalize_image_masked_into(source, mask, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_get_channel(
    const PillowCImage* source,
    int channel_index,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (channel_index < 0 || channel_index >= source->channels) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        const int target_mode = channel_target_mode_for_source(source);
        auto* image = new PillowCImage{
            source->width,
            source->height,
            target_mode,
            1,
            static_cast<std::size_t>(source->width),
            std::vector<std::uint8_t>(static_cast<std::size_t>(source->width) * source->height)};
        const int status = copy_channel_into(source, channel_index, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_put_alpha_value(
    const PillowCImage* source,
    std::uint8_t alpha,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const int target_mode = alpha_target_mode_for_source(source);
    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, target_channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            target_mode,
            target_channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = put_alpha_value_into(source, alpha, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_put_alpha_image(
    const PillowCImage* source,
    const PillowCImage* alpha,
    PillowCImage** out_image)
{
    if (!source || !alpha || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const int target_mode = alpha_target_mode_for_source(source);
    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0 || alpha->mode != PILLOW_C_MODE_L || alpha->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (alpha->width != source->width || alpha->height != source->height) {
        return PILLOW_C_MISMATCH;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, target_channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            target_mode,
            target_channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = put_alpha_image_into(source, alpha, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_convert_mode(
    const PillowCImage* source,
    int target_mode,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, target_channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            target_mode,
            target_channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = convert_image_mode_into(source, target_mode, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_convert_mode_dither(
    const PillowCImage* source,
    int target_mode,
    int dither,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, target_channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            target_mode,
            target_channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = convert_image_mode_dither_into(source, target_mode, dither, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_quantize_palette(
    const PillowCImage* source,
    const PillowCImage* palette,
    PillowCImage** out_image)
{
    if (!source || !palette || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_P,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = quantize_palette_image_into(source, palette, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_quantize(
    const PillowCImage* source,
    int colors,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_P,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = quantize_exact_image_into(source, colors, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_quantize_options(
    const PillowCImage* source,
    int colors,
    int method,
    int kmeans,
    int dither,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_P,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = quantize_options_image_into(
            source,
            colors,
            method,
            kmeans,
            dither,
            image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_convert_matrix(
    const PillowCImage* source,
    int target_mode,
    const double* matrix,
    std::size_t matrix_count,
    PillowCImage** out_image)
{
    if (!source || !matrix || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    int target_channels = 0;
    if (!valid_convert_matrix_arguments(source, target_mode, matrix_count, &target_channels)) {
        if (source->mode == PILLOW_C_MODE_RGB &&
            (target_mode == PILLOW_C_MODE_L || target_mode == PILLOW_C_MODE_RGB) &&
            matrix_count != (target_mode == PILLOW_C_MODE_L ? 4u : 12u)) {
            return PILLOW_C_INVALID_LENGTH;
        }
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, target_channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            target_mode,
            target_channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = convert_matrix_image_into(source, target_mode, matrix, matrix_count, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_merge_bands(
    int target_mode,
    const PillowCImage* const* bands,
    std::size_t band_count,
    PillowCImage** out_image)
{
    if (!bands || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0 || band_count != static_cast<std::size_t>(target_channels)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const PillowCImage* first = bands[0];
    if (!first) {
        return PILLOW_C_NULL_POINTER;
    }
    if (first->mode != PILLOW_C_MODE_L || first->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(first->width, first->height, target_channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            first->width,
            first->height,
            target_mode,
            target_channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = merge_bands_into(target_mode, bands, band_count, image);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_alpha_composite_rgba(
    const PillowCImage* dst,
    const PillowCImage* src,
    PillowCImage** out_image)
{
    if (!dst || !src || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!pillow_c_image_shapes_match(dst, src) || dst->channels != 4) {
        return PILLOW_C_MISMATCH;
    }

    try {
        auto* image = new PillowCImage{dst->width, dst->height, dst->mode, dst->channels, dst->stride, std::vector<std::uint8_t>(dst->pixels.size())};
        const int status = pillow_c_alpha_composite_rgba(
            dst->pixels.data(),
            src->pixels.data(),
            image->pixels.data(),
            static_cast<std::size_t>(dst->width) * dst->height);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

