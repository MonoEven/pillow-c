#include "pillow_c_internal.h"

#include <cmath>
#include <limits>
#include <new>

bool valid_image_shape(int width, int height, int channels)
{
    return width > 0 && height > 0 && channels > 0 && channels <= 4;
}

bool valid_image_shape_allow_empty(int width, int height, int channels)
{
    return width >= 0 && height >= 0 && channels > 0 && channels <= 4;
}

int channels_for_mode(int mode)
{
    switch (mode) {
    case PILLOW_C_MODE_1:
    case PILLOW_C_MODE_P:
    case PILLOW_C_MODE_L:
        return 1;
    case PILLOW_C_MODE_LA:
    case PILLOW_C_MODE_PA:
        return 2;
    case PILLOW_C_MODE_RGB:
    case PILLOW_C_MODE_YCBCR:
    case PILLOW_C_MODE_HSV:
    case PILLOW_C_MODE_LAB:
        return 3;
    case PILLOW_C_MODE_RGBA:
    case PILLOW_C_MODE_RGBX:
    case PILLOW_C_MODE_CMYK:
    case PILLOW_C_MODE_I:
    case PILLOW_C_MODE_F:
        return 4;
    case PILLOW_C_MODE_I16:
    case PILLOW_C_MODE_I16B:
        return 2;
    default:
        return 0;
    }
}

int mode_for_channels(int channels)
{
    switch (channels) {
    case 1:
        return PILLOW_C_MODE_L;
    case 2:
        return PILLOW_C_MODE_LA;
    case 3:
        return PILLOW_C_MODE_RGB;
    case 4:
        return PILLOW_C_MODE_RGBA;
    default:
        return 0;
    }
}

bool checked_image_size(int width, int height, int channels, std::size_t* stride, std::size_t* size)
{
    if (!valid_image_shape(width, height, channels) || !stride || !size) {
        return false;
    }

    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    const auto c = static_cast<std::size_t>(channels);
    constexpr std::size_t max_size = static_cast<std::size_t>(-1);
    if (w > max_size / c) {
        return false;
    }
    const std::size_t row = w * c;
    if (h > max_size / row) {
        return false;
    }
    *stride = row;
    *size = row * h;
    return true;
}

bool checked_image_size_allow_empty(
    int width,
    int height,
    int channels,
    std::size_t* stride,
    std::size_t* size)
{
    if (!valid_image_shape_allow_empty(width, height, channels) || !stride || !size) {
        return false;
    }

    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    const auto c = static_cast<std::size_t>(channels);
    constexpr std::size_t max_size = static_cast<std::size_t>(-1);
    if (w > max_size / c) {
        return false;
    }
    const std::size_t row = w * c;
    if (row != 0 && h > max_size / row) {
        return false;
    }
    *stride = row;
    *size = row * h;
    return true;
}

extern "C" __declspec(dllexport) int pillow_c_image_create(
    int width,
    int height,
    int channels,
    PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    const int mode = mode_for_channels(channels);
    if (mode == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(width, height, channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{width, height, mode, channels, stride, std::vector<std::uint8_t>(size)};
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_create_mode(
    int width,
    int height,
    int mode,
    PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    const int channels = channels_for_mode(mode);
    if (channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(width, height, channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{width, height, mode, channels, stride, std::vector<std::uint8_t>(size)};
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_free(PillowCImage* image)
{
    delete image;
    return PILLOW_C_OK;
}

namespace {
constexpr int RESAMPLE_PRECISION_BITS = 32 - 8 - 2;

inline std::uint32_t shift_for_div255(std::uint32_t value)
{
    return (((value >> 8) + value) >> 8);
}


inline std::uint8_t clip_u8_int(int value)
{
    if (value <= 0) {
        return 0;
    }
    if (value >= 256) {
        return 255;
    }
    return static_cast<std::uint8_t>(value);
}

inline std::uint8_t clip_u8_double(double value)
{
    if (value <= 0.0) {
        return 0;
    }
    if (value >= 255.0) {
        return 255;
    }
    return static_cast<std::uint8_t>(value);
}


inline std::uint8_t pillow_clip8_double(double value)
{
    if (value <= 0.0) {
        return 0;
    }
    if (value < 256.0) {
        return static_cast<std::uint8_t>(value);
    }
    return 255;
}

inline std::uint8_t round_half_up_clip_u8(double value)
{
    if (!(value > 0.0)) {
        return 0;
    }
    if (value >= 254.5) {
        return 255;
    }
    return static_cast<std::uint8_t>(std::floor(value + 0.5));
}

inline std::int32_t round_half_up_clip_i32_nonnegative(double value)
{
    if (!(value > 0.0)) {
        return 0;
    }
    constexpr double max_i32 = static_cast<double>(std::numeric_limits<std::int32_t>::max());
    if (value >= max_i32 - 0.5) {
        return std::numeric_limits<std::int32_t>::max();
    }
    return static_cast<std::int32_t>(std::floor(value + 0.5));
}

inline int clamp_int(int value, int low, int high)
{
    return value < low ? low : (value > high ? high : value);
}

inline int ceil_div_int(int value, int divisor)
{
    return (value + divisor - 1) / divisor;
}

std::uint32_t fixed_point_division_u32(int divider, int result_bits)
{
    const double max_dividend = static_cast<double>(1 << result_bits) * divider;
    constexpr double max_int = 4294967296.0;
    return static_cast<std::uint32_t>(max_int / max_dividend);
}

std::uint8_t reduce_average_u8(std::uint64_t sum, std::uint32_t count)
{
    const std::uint64_t amended = sum + count / 2u;
    const std::uint64_t multiplier = fixed_point_division_u32(static_cast<int>(count), 8);
    return static_cast<std::uint8_t>((amended * multiplier) >> 24);
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


inline std::uint8_t overlay_u8(std::uint8_t left, std::uint8_t right)
{
    const int left_value = static_cast<int>(left);
    const int right_value = static_cast<int>(right);
    const int value = left_value < 128 ?
        (left_value * right_value) / 127 :
        255 - (((255 - left_value) * (255 - right_value)) / 127);
    return static_cast<std::uint8_t>(value);
}

inline std::uint8_t clip_resample_u8(std::int64_t value)
{
    const std::int64_t shifted = value >> RESAMPLE_PRECISION_BITS;
    if (shifted <= 0) {
        return 0;
    }
    if (shifted >= 256) {
        return 255;
    }
    return static_cast<std::uint8_t>(shifted);
}

inline std::uint8_t mul_div_255(std::uint8_t value, std::uint8_t alpha)
{
    const std::uint32_t tmp = static_cast<std::uint32_t>(value) * alpha + 128u;
    return static_cast<std::uint8_t>(shift_for_div255(tmp));
}

const char* mode_name(int mode)
{
    switch (mode) {
    case PILLOW_C_MODE_1:
        return "1";
    case PILLOW_C_MODE_P:
        return "P";
    case PILLOW_C_MODE_PA:
        return "PA";
    case PILLOW_C_MODE_L:
        return "L";
    case PILLOW_C_MODE_LA:
        return "LA";
    case PILLOW_C_MODE_RGB:
        return "RGB";
    case PILLOW_C_MODE_RGBA:
        return "RGBA";
    case PILLOW_C_MODE_RGBX:
        return "RGBX";
    case PILLOW_C_MODE_CMYK:
        return "CMYK";
    case PILLOW_C_MODE_I:
        return "I";
    case PILLOW_C_MODE_F:
        return "F";
    case PILLOW_C_MODE_I16:
        return "I;16";
    case PILLOW_C_MODE_I16B:
        return "I;16B";
    case PILLOW_C_MODE_YCBCR:
        return "YCbCr";
    case PILLOW_C_MODE_HSV:
        return "HSV";
    case PILLOW_C_MODE_LAB:
        return "LAB";
    default:
        return nullptr;
    }
}


bool images_match(const PillowCImage* left, const PillowCImage* right)
{
    return left && right &&
           left->width == right->width &&
           left->height == right->height &&
           left->mode == right->mode &&
           left->channels == right->channels &&
           left->stride == right->stride &&
           left->pixels.size() == right->pixels.size();
}

bool supported_composite_mask(const PillowCImage* mask)
{
    return mask &&
           ((mask->mode == PILLOW_C_MODE_1 && mask->channels == 1) ||
            (mask->mode == PILLOW_C_MODE_L && mask->channels == 1) ||
            (mask->mode == PILLOW_C_MODE_LA && mask->channels == 2) ||
            (mask->mode == PILLOW_C_MODE_RGBA && mask->channels == 4));
}

bool supported_statistics_mask(const PillowCImage* mask)
{
    return mask &&
           ((mask->mode == PILLOW_C_MODE_1 && mask->channels == 1) ||
            (mask->mode == PILLOW_C_MODE_L && mask->channels == 1));
}

bool supported_bitmap_mask(const PillowCImage* mask)
{
    return mask &&
           ((mask->mode == PILLOW_C_MODE_1 && mask->channels == 1) ||
            (mask->mode == PILLOW_C_MODE_L && mask->channels == 1) ||
            (mask->mode == PILLOW_C_MODE_RGBA && mask->channels == 4));
}

std::uint8_t mask_alpha_at(const PillowCImage* mask, const std::uint8_t* mask_row, int x)
{
    if (mask->channels == 1) {
        return mask_row[x];
    }
    if (mask->channels == 2) {
        return mask_row[static_cast<std::size_t>(x) * 2u + 1u];
    }
    return mask_row[static_cast<std::size_t>(x) * 4u + 3u];
}

bool image_shape_matches(const PillowCImage* image, int width, int height, int channels)
{
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(width, height, channels, &stride, &size)) {
        return false;
    }
    return image &&
           image->width == width &&
           image->height == height &&
           image->channels == channels &&
           image->stride == stride &&
           image->pixels.size() == size;
}

bool image_shape_matches(const PillowCImage* image, int width, int height, int mode, int channels)
{
    return image_shape_matches(image, width, height, channels) && image->mode == mode;
}

bool statistics_mask_matches(const PillowCImage* mask, int width, int height)
{
    return supported_statistics_mask(mask) &&
           image_shape_matches(mask, width, height, 1);
}

bool image_shape_matches(const PillowCImage* left, const PillowCImage* right)
{
    return left && right &&
           image_shape_matches(left, right->width, right->height, right->channels) &&
           left->mode == right->mode &&
           left->stride == right->stride &&
           left->pixels.size() == right->pixels.size();
}

void copy_palette_if_same_mode(const PillowCImage* source, PillowCImage* target)
{
    if (source && target && source->mode == target->mode) {
        target->palette_rgb = source->palette_rgb;
        target->palette_alpha = source->palette_alpha;
        target->palette_alpha_mode = source->palette_alpha_mode;
    }
}

void copy_palette_if_point_preserves_core_palette(const PillowCImage* source, PillowCImage* target)
{
    if (source && target && source->mode == PILLOW_C_MODE_P && source->channels == 1) {
        target->palette_rgb = source->palette_rgb;
        target->palette_alpha = source->palette_alpha;
        target->palette_alpha_mode = source->palette_alpha_mode;
    }
}

} // namespace

bool pillow_c_image_shape_matches(
    const PillowCImage* image,
    int width,
    int height,
    int channels)
{
    return image_shape_matches(image, width, height, channels);
}

bool pillow_c_image_shape_matches(
    const PillowCImage* image,
    int width,
    int height,
    int mode,
    int channels)
{
    return image_shape_matches(image, width, height, mode, channels);
}

bool pillow_c_image_shape_matches(const PillowCImage* left, const PillowCImage* right)
{
    return image_shape_matches(left, right);
}

bool pillow_c_image_shape_matches_mode(
    const PillowCImage* image,
    int width,
    int height,
    int mode,
    int channels)
{
    return image_shape_matches(image, width, height, mode, channels);
}

bool pillow_c_image_shapes_match(const PillowCImage* left, const PillowCImage* right)
{
    return image_shape_matches(left, right);
}

void pillow_c_copy_palette_if_same_mode(const PillowCImage* source, PillowCImage* target)
{
    copy_palette_if_same_mode(source, target);
}

std::uint8_t pillow_c_clip_u8_int(int value)
{
    return clip_u8_int(value);
}

std::uint8_t pillow_c_clip_u8_double(double value)
{
    return clip_u8_double(value);
}

std::uint8_t pillow_c_clip_resample_u8(std::int64_t value)
{
    return clip_resample_u8(value);
}

std::uint8_t pillow_c_mul_div_255(std::uint8_t value, std::uint8_t alpha)
{
    return mul_div_255(value, alpha);
}

std::uint8_t pillow_c_reduce_average_u8(std::uint64_t sum, std::uint32_t count)
{
    return reduce_average_u8(sum, count);
}

int pillow_c_ceil_div_int(int value, int divisor)
{
    return ceil_div_int(value, divisor);
}

int pillow_c_clamp_int(int value, int low, int high)
{
    return clamp_int(value, low, high);
}


std::uint8_t pillow_c_round_half_up_clip_u8(double value)
{
    return round_half_up_clip_u8(value);
}

std::int32_t pillow_c_round_half_up_clip_i32_nonnegative(double value)
{
    return round_half_up_clip_i32_nonnegative(value);
}



bool pillow_c_supported_bitmap_mask(const PillowCImage* mask)
{
    return supported_bitmap_mask(mask);
}

std::uint8_t pillow_c_mask_alpha_at(
    const PillowCImage* mask,
    const std::uint8_t* mask_row,
    int x)
{
    return mask_alpha_at(mask, mask_row, x);
}

std::uint32_t pillow_c_shift_for_div255(std::uint32_t value)
{
    return shift_for_div255(value);
}

bool pillow_c_supported_composite_mask(const PillowCImage* mask)
{
    return supported_composite_mask(mask);
}

bool pillow_c_statistics_mask_matches(const PillowCImage* mask, int width, int height)
{
    return statistics_mask_matches(mask, width, height);
}

void pillow_c_copy_palette_if_point_preserves_core_palette(
    const PillowCImage* source,
    PillowCImage* target)
{
    copy_palette_if_point_preserves_core_palette(source, target);
}

extern "C" __declspec(dllexport) int pillow_c_mode_from_string(
    const char* mode_name_text,
    int* out_mode)
{
    if (!mode_name_text || !out_mode) {
        return PILLOW_C_NULL_POINTER;
    }
    if (std::strcmp(mode_name_text, "1") == 0) {
        *out_mode = PILLOW_C_MODE_1;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "L") == 0) {
        *out_mode = PILLOW_C_MODE_L;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "LA") == 0) {
        *out_mode = PILLOW_C_MODE_LA;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "RGB") == 0) {
        *out_mode = PILLOW_C_MODE_RGB;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "RGBA") == 0) {
        *out_mode = PILLOW_C_MODE_RGBA;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "RGBX") == 0) {
        *out_mode = PILLOW_C_MODE_RGBX;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "P") == 0) {
        *out_mode = PILLOW_C_MODE_P;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "PA") == 0) {
        *out_mode = PILLOW_C_MODE_PA;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "CMYK") == 0) {
        *out_mode = PILLOW_C_MODE_CMYK;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "I") == 0) {
        *out_mode = PILLOW_C_MODE_I;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "F") == 0) {
        *out_mode = PILLOW_C_MODE_F;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "I;16") == 0) {
        *out_mode = PILLOW_C_MODE_I16;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "I;16B") == 0) {
        *out_mode = PILLOW_C_MODE_I16B;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "YCbCr") == 0) {
        *out_mode = PILLOW_C_MODE_YCBCR;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "HSV") == 0) {
        *out_mode = PILLOW_C_MODE_HSV;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "LAB") == 0) {
        *out_mode = PILLOW_C_MODE_LAB;
        return PILLOW_C_OK;
    }
    *out_mode = 0;
    return PILLOW_C_INVALID_ARGUMENT;
}

extern "C" __declspec(dllexport) int pillow_c_mode_name(
    int mode,
    char* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    const char* name = mode_name(mode);
    if (!name) {
        *out_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t required = std::strlen(name) + 1;
    *out_required = required;
    if (!out) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out, name, required);
    return PILLOW_C_OK;
}


