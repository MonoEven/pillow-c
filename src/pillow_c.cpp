#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <climits>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace {

constexpr int PILLOW_C_OK = 0;
constexpr int PILLOW_C_NULL_POINTER = -1;
constexpr int PILLOW_C_INVALID_LENGTH = -2;
constexpr int PILLOW_C_INVALID_ARGUMENT = -3;
constexpr int PILLOW_C_ALLOCATION_FAILED = -4;
constexpr int PILLOW_C_MISMATCH = -5;

constexpr int PILLOW_C_MODE_L = 1;
constexpr int PILLOW_C_MODE_RGB = 3;
constexpr int PILLOW_C_MODE_RGBA = 4;

constexpr int PILLOW_C_RESAMPLE_NEAREST = 0;
constexpr int PILLOW_C_RESAMPLE_LANCZOS = 1;
constexpr int PILLOW_C_RESAMPLE_BILINEAR = 2;
constexpr int PILLOW_C_RESAMPLE_BICUBIC = 3;
constexpr int PILLOW_C_RESAMPLE_BOX = 4;
constexpr int PILLOW_C_RESAMPLE_HAMMING = 5;

constexpr int RESAMPLE_PRECISION_BITS = 32 - 8 - 2;
constexpr int RESAMPLE_PRECISION_SCALE = 1 << RESAMPLE_PRECISION_BITS;
constexpr int RESAMPLE_ROUNDING_BIAS = 1 << (RESAMPLE_PRECISION_BITS - 1);

struct PillowCImage {
    int width;
    int height;
    int mode;
    int channels;
    std::size_t stride;
    std::vector<std::uint8_t> pixels;
};

struct ResampleCoefficients {
    int kernel_size;
    std::vector<int> bounds;
    std::vector<std::int32_t> weights;
};

struct ResampleFilterSpec {
    double support;
    double (*filter)(double);
};

struct AffineGeometry {
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
    int width;
    int height;
};

struct PerspectiveGeometry {
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
    double g;
    double h;
    int width;
    int height;
};

struct QuadGeometry {
    double x0;
    double x1;
    double x2;
    double x3;
    double y0;
    double y1;
    double y2;
    double y3;
    int width;
    int height;
};

struct ColorCountEntry {
    std::uint64_t count;
    std::uint8_t color[4];
};

inline std::uint32_t shift_for_div255(std::uint32_t value)
{
    return (((value >> 8) + value) >> 8);
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

inline int clamp_int(int value, int low, int high)
{
    return value < low ? low : (value > high ? high : value);
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
    case PILLOW_C_MODE_L:
        return 1;
    case PILLOW_C_MODE_RGB:
        return 3;
    case PILLOW_C_MODE_RGBA:
        return 4;
    default:
        return 0;
    }
}

int mode_for_channels(int channels)
{
    switch (channels) {
    case 1:
        return PILLOW_C_MODE_L;
    case 3:
        return PILLOW_C_MODE_RGB;
    case 4:
        return PILLOW_C_MODE_RGBA;
    default:
        return 0;
    }
}

const char* mode_name(int mode)
{
    switch (mode) {
    case PILLOW_C_MODE_L:
        return "L";
    case PILLOW_C_MODE_RGB:
        return "RGB";
    case PILLOW_C_MODE_RGBA:
        return "RGBA";
    default:
        return nullptr;
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

bool checked_image_size_allow_empty(int width, int height, int channels, std::size_t* stride, std::size_t* size)
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
           ((mask->mode == PILLOW_C_MODE_L && mask->channels == 1) ||
            (mask->mode == PILLOW_C_MODE_RGBA && mask->channels == 4));
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

bool image_shape_matches(const PillowCImage* left, const PillowCImage* right)
{
    return left && right &&
           image_shape_matches(left, right->width, right->height, right->channels) &&
           left->mode == right->mode &&
           left->stride == right->stride &&
           left->pixels.size() == right->pixels.size();
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

    *out_width = overlapping_width(left, right);
    *out_height = overlapping_height(left, right);
    if (!image_shape_matches(target, *out_width, *out_height, left->mode, left->channels)) {
        return PILLOW_C_MISMATCH;
    }
    return PILLOW_C_OK;
}

const char* status_message(int status)
{
    switch (status) {
    case PILLOW_C_OK:
        return "ok";
    case PILLOW_C_NULL_POINTER:
        return "null pointer";
    case PILLOW_C_INVALID_LENGTH:
        return "invalid length";
    case PILLOW_C_INVALID_ARGUMENT:
        return "invalid argument";
    case PILLOW_C_ALLOCATION_FAILED:
        return "allocation failed";
    case PILLOW_C_MISMATCH:
        return "mismatch";
    default:
        return "unknown status";
    }
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
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
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
int convert_image_mode_into(const PillowCImage* source, int target_mode, PillowCImage* target);

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
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    int status = fill_image_pixels(target, color, color_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return paste_image_pixels_into(target, source, left, top);
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
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
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
    if (!image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int width = source->width;
    const int height = source->height;
    const int channels = source->channels;
    if (width <= 0 || height <= 0) {
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
    if (!supported_composite_mask(mask)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (mask->width != source->width || mask->height != source->height) {
        return PILLOW_C_MISMATCH;
    }
    if (!image_shape_matches(target, target_source)) {
        return PILLOW_C_MISMATCH;
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
            const std::uint8_t alpha = mask->channels == 1
                ? mask_row[x]
                : mask_row[static_cast<std::size_t>(x) * 4u + 3u];
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
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_L, 1)) {
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
    if (!image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
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
    if (!image_shape_matches(target, source)) {
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
    if (!image_shape_matches(target, source)) {
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

int invert_image_into(const PillowCImage* source, PillowCImage* target)
{
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
    if (!image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
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
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_RGB, 3)) {
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

int histogram_image(const PillowCImage* source, std::uint64_t* out_histogram, std::size_t out_count);

int build_equalize_lut(const PillowCImage* source, std::vector<std::uint8_t>* out_lut)
{
    if (!source || !out_lut) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!supports_imageops_lut(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<std::uint64_t> histogram(static_cast<std::size_t>(source->channels) * 256u);
    int status = histogram_image(source, histogram.data(), histogram.size());
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

int equalize_image_into(const PillowCImage* source, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }

    try {
        std::vector<std::uint8_t> lut;
        const int status = build_equalize_lut(source, &lut);
        if (status != PILLOW_C_OK) {
            return status;
        }
        return apply_point_lut_into(source, lut.data(), lut.size(), target);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int copy_channel_into(const PillowCImage* source, int channel_index, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (channel_index < 0 || channel_index >= source->channels) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_L, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* src = source->pixels.data() + channel_index;
    std::uint8_t* dst = target->pixels.data();
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    for (std::size_t i = 0; i < pixels; ++i) {
        dst[i] = src[i * source->channels];
    }
    return PILLOW_C_OK;
}

bool supports_rgba_alpha_target(const PillowCImage* source)
{
    return source && (source->mode == PILLOW_C_MODE_RGB || source->mode == PILLOW_C_MODE_RGBA);
}

int put_alpha_value_into(const PillowCImage* source, std::uint8_t alpha, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!supports_rgba_alpha_target(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_RGBA, 4)) {
        return PILLOW_C_MISMATCH;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint8_t* src = source->pixels.data() + i * source->channels;
        std::uint8_t* dst = target->pixels.data() + i * 4;
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = alpha;
    }
    return PILLOW_C_OK;
}

int put_alpha_image_into(const PillowCImage* source, const PillowCImage* alpha, PillowCImage* target)
{
    if (!source || !alpha || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!supports_rgba_alpha_target(source) || alpha->mode != PILLOW_C_MODE_L || alpha->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (alpha->width != source->width || alpha->height != source->height) {
        return PILLOW_C_MISMATCH;
    }
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_RGBA, 4)) {
        return PILLOW_C_MISMATCH;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint8_t* src = source->pixels.data() + i * source->channels;
        std::uint8_t* dst = target->pixels.data() + i * 4;
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = alpha->pixels[i];
    }
    return PILLOW_C_OK;
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
    if (!image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->mode == target_mode) {
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        return PILLOW_C_OK;
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    if (target_mode == PILLOW_C_MODE_L && (source->mode == PILLOW_C_MODE_RGB || source->mode == PILLOW_C_MODE_RGBA)) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* px = source->pixels.data() + i * source->channels;
            target->pixels[i] = static_cast<std::uint8_t>(
                (static_cast<std::int32_t>(px[0]) * 19595 +
                 static_cast<std::int32_t>(px[1]) * 38470 +
                 static_cast<std::int32_t>(px[2]) * 7471 +
                 0x8000) >> 16);
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
            dst[3] = 255;
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
            dst[3] = 255;
        }
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_RGBA && target_mode == PILLOW_C_MODE_RGB) {
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

    if (!image_shape_matches(target, first->width, first->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
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

int histogram_image(const PillowCImage* source, std::uint64_t* out_histogram, std::size_t out_count)
{
    if (!source || !out_histogram) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t required = static_cast<std::size_t>(source->channels) * 256u;
    if (out_count != required) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_histogram, out_histogram + out_count, 0);
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    const std::uint8_t* data = source->pixels.data();
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        const std::uint8_t* src = data + pixel * source->channels;
        for (int channel = 0; channel < source->channels; ++channel) {
            ++out_histogram[static_cast<std::size_t>(channel) * 256u + src[channel]];
        }
    }
    return PILLOW_C_OK;
}

int histogram_image_masked(
    const PillowCImage* source,
    const PillowCImage* mask,
    std::uint64_t* out_histogram,
    std::size_t out_count)
{
    if (!source || !out_histogram) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!mask) {
        return histogram_image(source, out_histogram, out_count);
    }
    if (mask->mode != PILLOW_C_MODE_L || mask->channels != 1 ||
        !image_shape_matches(mask, source->width, source->height, PILLOW_C_MODE_L, 1)) {
        return PILLOW_C_MISMATCH;
    }
    const std::size_t required = static_cast<std::size_t>(source->channels) * 256u;
    if (out_count != required) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_histogram, out_histogram + out_count, 0);
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    const std::uint8_t* data = source->pixels.data();
    const std::uint8_t* mask_data = mask->pixels.data();
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        if (mask_data[pixel] == 0) {
            continue;
        }
        const std::uint8_t* src = data + pixel * source->channels;
        for (int channel = 0; channel < source->channels; ++channel) {
            ++out_histogram[static_cast<std::size_t>(channel) * 256u + src[channel]];
        }
    }
    return PILLOW_C_OK;
}

int extrema_image(
    const PillowCImage* source,
    std::uint8_t* out_min,
    std::uint8_t* out_max,
    std::uint8_t* out_has_value,
    std::size_t out_count)
{
    if (!source || !out_min || !out_max || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_count != static_cast<std::size_t>(source->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_min, out_min + out_count, static_cast<std::uint8_t>(0));
    std::fill(out_max, out_max + out_count, static_cast<std::uint8_t>(0));
    std::fill(out_has_value, out_has_value + out_count, static_cast<std::uint8_t>(0));
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    if (pixels == 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* data = source->pixels.data();
    for (int channel = 0; channel < source->channels; ++channel) {
        const std::uint8_t first = data[channel];
        out_min[channel] = first;
        out_max[channel] = first;
        out_has_value[channel] = 1;
    }

    for (std::size_t pixel = 1; pixel < pixels; ++pixel) {
        const std::uint8_t* src = data + pixel * source->channels;
        for (int channel = 0; channel < source->channels; ++channel) {
            const std::uint8_t value = src[channel];
            if (value < out_min[channel]) {
                out_min[channel] = value;
            } else if (value > out_max[channel]) {
                out_max[channel] = value;
            }
        }
    }
    return PILLOW_C_OK;
}

int getbbox_image(
    const PillowCImage* source,
    bool alpha_only,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom,
    int* out_has_bbox)
{
    if (!source || !out_left || !out_top || !out_right || !out_bottom || !out_has_bbox) {
        return PILLOW_C_NULL_POINTER;
    }

    *out_left = 0;
    *out_top = 0;
    *out_right = 0;
    *out_bottom = 0;
    *out_has_bbox = 0;
    if (source->width <= 0 || source->height <= 0) {
        return PILLOW_C_OK;
    }

    int left = source->width;
    int top = source->height;
    int right = 0;
    int bottom = 0;
    const bool use_alpha = alpha_only && source->mode == PILLOW_C_MODE_RGBA && source->channels == 4;

    for (int y = 0; y < source->height; ++y) {
        const std::uint8_t* row = source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
        for (int x = 0; x < source->width; ++x) {
            const std::uint8_t* pixel = row + static_cast<std::size_t>(x) * source->channels;
            bool nonzero = false;
            if (use_alpha) {
                nonzero = pixel[3] != 0;
            } else {
                for (int channel = 0; channel < source->channels; ++channel) {
                    if (pixel[channel] != 0) {
                        nonzero = true;
                        break;
                    }
                }
            }
            if (!nonzero) {
                continue;
            }

            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x + 1);
            bottom = std::max(bottom, y + 1);
        }
    }

    if (right <= left || bottom <= top) {
        return PILLOW_C_OK;
    }

    *out_left = left;
    *out_top = top;
    *out_right = right;
    *out_bottom = bottom;
    *out_has_bbox = 1;
    return PILLOW_C_OK;
}

int getprojection_image(
    const PillowCImage* source,
    std::uint8_t* out_x_projection,
    std::size_t out_x_count,
    std::uint8_t* out_y_projection,
    std::size_t out_y_count)
{
    if (!source || (out_x_count > 0 && !out_x_projection) || (out_y_count > 0 && !out_y_projection)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_x_count != static_cast<std::size_t>(source->width) ||
        out_y_count != static_cast<std::size_t>(source->height)) {
        return PILLOW_C_INVALID_LENGTH;
    }

    if (out_x_count > 0) {
        std::fill(out_x_projection, out_x_projection + out_x_count, static_cast<std::uint8_t>(0));
    }
    if (out_y_count > 0) {
        std::fill(out_y_projection, out_y_projection + out_y_count, static_cast<std::uint8_t>(0));
    }
    for (int y = 0; y < source->height; ++y) {
        const std::uint8_t* row = source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
        for (int x = 0; x < source->width; ++x) {
            const std::uint8_t* pixel = row + static_cast<std::size_t>(x) * source->channels;
            bool nonzero = false;
            for (int channel = 0; channel < source->channels; ++channel) {
                if (pixel[channel] != 0) {
                    nonzero = true;
                    break;
                }
            }
            if (nonzero) {
                out_x_projection[x] = 1;
                out_y_projection[y] = 1;
            }
        }
    }
    return PILLOW_C_OK;
}

int find_color_entry(const std::vector<ColorCountEntry>& entries, const std::uint8_t* color, int channels)
{
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (std::memcmp(entries[index].color, color, static_cast<std::size_t>(channels)) == 0) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int getcolors_image(
    const PillowCImage* source,
    int maxcolors,
    std::uint64_t* out_counts,
    std::uint8_t* out_colors,
    std::size_t out_capacity,
    std::size_t* out_count,
    int* out_exceeded)
{
    if (!source || !out_count || !out_exceeded) {
        return PILLOW_C_NULL_POINTER;
    }

    *out_count = 0;
    *out_exceeded = 0;
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    if (pixels == 0) {
        if (maxcolors < 0) {
            *out_exceeded = 1;
        }
        return PILLOW_C_OK;
    }
    if (maxcolors < 1) {
        *out_exceeded = 1;
        return PILLOW_C_OK;
    }

    std::vector<ColorCountEntry> entries;
    const auto max_unique = static_cast<std::size_t>(maxcolors);
    const std::uint8_t* data = source->pixels.data();
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        const std::uint8_t* color = data + pixel * source->channels;
        const int existing = find_color_entry(entries, color, source->channels);
        if (existing >= 0) {
            ++entries[static_cast<std::size_t>(existing)].count;
            continue;
        }

        if (entries.size() >= max_unique) {
            *out_exceeded = 1;
            return PILLOW_C_OK;
        }

        ColorCountEntry entry{};
        entry.count = 1;
        std::memcpy(entry.color, color, static_cast<std::size_t>(source->channels));
        try {
            entries.push_back(entry);
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
    }

    *out_count = entries.size();
    if (!out_counts && !out_colors && out_capacity == 0) {
        return PILLOW_C_OK;
    }
    if (!out_counts || !out_colors) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_capacity < entries.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }

    for (std::size_t index = 0; index < entries.size(); ++index) {
        out_counts[index] = entries[index].count;
        std::memcpy(
            out_colors + index * static_cast<std::size_t>(source->channels),
            entries[index].color,
            static_cast<std::size_t>(source->channels));
    }
    return PILLOW_C_OK;
}

bool autocontrast_supported_mode(const PillowCImage* source)
{
    return supports_imageops_lut(source);
}

bool autocontrast_preserve_tone_supported_mode(const PillowCImage* source)
{
    return source && (source->mode == PILLOW_C_MODE_L || source->mode == PILLOW_C_MODE_RGB);
}

int apply_histogram_end_cut(std::uint64_t* histogram, long double cut, bool from_high)
{
    if (!histogram) {
        return PILLOW_C_NULL_POINTER;
    }
    for (int step = 0; step < 256; ++step) {
        const int ix = from_high ? 255 - step : step;
        const long double current = static_cast<long double>(histogram[ix]);
        if (cut > current) {
            cut -= current;
            histogram[ix] = 0;
        } else {
            const long double next = current - cut;
            if (next < 0.0L || next > static_cast<long double>(std::numeric_limits<std::uint64_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            histogram[ix] = static_cast<std::uint64_t>(next);
            cut = 0.0L;
        }
        if (cut <= 0.0L) {
            break;
        }
    }
    return PILLOW_C_OK;
}

int histogram_image_preserve_tone(
    const PillowCImage* source,
    const PillowCImage* mask,
    std::uint64_t* out_histogram,
    std::size_t out_count)
{
    if (!source || !out_histogram) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!autocontrast_preserve_tone_supported_mode(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (out_count != 256u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (mask && (mask->mode != PILLOW_C_MODE_L || mask->channels != 1 ||
        !image_shape_matches(mask, source->width, source->height, PILLOW_C_MODE_L, 1))) {
        return PILLOW_C_MISMATCH;
    }

    std::fill(out_histogram, out_histogram + out_count, 0);
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    const std::uint8_t* data = source->pixels.data();
    const std::uint8_t* mask_data = mask ? mask->pixels.data() : nullptr;
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        if (mask_data && mask_data[pixel] == 0) {
            continue;
        }
        const std::uint8_t value = source->channels == 1
            ? data[pixel]
            : static_cast<std::uint8_t>(
                (static_cast<std::int32_t>(data[pixel * 3u]) * 19595 +
                 static_cast<std::int32_t>(data[pixel * 3u + 1u]) * 38470 +
                 static_cast<std::int32_t>(data[pixel * 3u + 2u]) * 7471 +
                 0x8000) >> 16);
        ++out_histogram[value];
    }
    return PILLOW_C_OK;
}

int build_autocontrast_lut(
    const PillowCImage* source,
    double low_cutoff,
    double high_cutoff,
    const std::uint8_t* ignore_values,
    std::size_t ignore_count,
    const PillowCImage* mask,
    bool preserve_tone,
    std::vector<std::uint8_t>* out_lut)
{
    if (!source || !out_lut) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((!preserve_tone && !autocontrast_supported_mode(source)) ||
        (preserve_tone && !autocontrast_preserve_tone_supported_mode(source)) ||
        !std::isfinite(low_cutoff) ||
        !std::isfinite(high_cutoff)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (ignore_count > 0 && !ignore_values) {
        return PILLOW_C_NULL_POINTER;
    }

    const int lut_channels = preserve_tone ? 1 : source->channels;
    std::vector<std::uint64_t> histogram(static_cast<std::size_t>(lut_channels) * 256u);
    int status = preserve_tone
        ? histogram_image_preserve_tone(source, mask, histogram.data(), histogram.size())
        : histogram_image_masked(source, mask, histogram.data(), histogram.size());
    if (status != PILLOW_C_OK) {
        return status;
    }

    out_lut->assign(static_cast<std::size_t>(lut_channels) * 256u, 0);
    for (int channel = 0; channel < lut_channels; ++channel) {
        std::uint64_t* h = histogram.data() + static_cast<std::size_t>(channel) * 256u;
        for (std::size_t i = 0; i < ignore_count; ++i) {
            h[ignore_values[i]] = 0;
        }
        if (low_cutoff != 0.0 || high_cutoff != 0.0) {
            std::uint64_t total = 0;
            for (int ix = 0; ix < 256; ++ix) {
                total += h[ix];
            }
            const long double total_value = static_cast<long double>(total);
            const long double low_cut = std::floor(total_value * static_cast<long double>(low_cutoff) / 100.0L);
            const long double high_cut = std::floor(total_value * static_cast<long double>(high_cutoff) / 100.0L);
            status = apply_histogram_end_cut(h, low_cut, false);
            if (status != PILLOW_C_OK) {
                return status;
            }
            status = apply_histogram_end_cut(h, high_cut, true);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        int lo = 255;
        int hi = 0;
        for (int ix = 0; ix < 256; ++ix) {
            if (h[ix] != 0) {
                lo = ix;
                break;
            }
        }
        for (int ix = 255; ix >= 0; --ix) {
            if (h[ix] != 0) {
                hi = ix;
                break;
            }
        }

        std::uint8_t* lut = out_lut->data() + static_cast<std::size_t>(channel) * 256u;
        if (hi <= lo) {
            for (int ix = 0; ix < 256; ++ix) {
                lut[ix] = static_cast<std::uint8_t>(ix);
            }
        } else {
            const double scale = 255.0 / static_cast<double>(hi - lo);
            const double offset = -lo * scale;
            for (int ix = 0; ix < 256; ++ix) {
                const int value = static_cast<int>(ix * scale + offset);
                lut[ix] = clip_u8_int(value);
            }
        }
    }

    return PILLOW_C_OK;
}

int autocontrast_image_into(
    const PillowCImage* source,
    double low_cutoff,
    double high_cutoff,
    const std::uint8_t* ignore_values,
    std::size_t ignore_count,
    const PillowCImage* mask,
    bool preserve_tone,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }

    try {
        std::vector<std::uint8_t> lut;
        const int status = build_autocontrast_lut(source, low_cutoff, high_cutoff, ignore_values, ignore_count, mask, preserve_tone, &lut);
        if (status != PILLOW_C_OK) {
            return status;
        }
        return preserve_tone
            ? apply_single_lut_into(source, lut.data(), lut.size(), target)
            : apply_point_lut_into(source, lut.data(), lut.size(), target);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

double round_15(double value)
{
    constexpr double scale = 1000000000000000.0;
    return std::round(value * scale) / scale;
}

void affine_transform_point(const AffineGeometry& geometry, double x, double y, double* out_x, double* out_y)
{
    *out_x = geometry.a * x + geometry.b * y + geometry.c;
    *out_y = geometry.d * x + geometry.e * y + geometry.f;
}

bool perspective_transform_point(const PerspectiveGeometry& geometry, double x, double y, double* out_x, double* out_y)
{
    const double denominator = geometry.g * x + geometry.h * y + 1.0;
    if (denominator == 0.0 || !std::isfinite(denominator)) {
        return false;
    }
    const double source_x = (geometry.a * x + geometry.b * y + geometry.c) / denominator;
    const double source_y = (geometry.d * x + geometry.e * y + geometry.f) / denominator;
    if (!std::isfinite(source_x) || !std::isfinite(source_y)) {
        return false;
    }
    *out_x = source_x;
    *out_y = source_y;
    return true;
}

void quad_transform_point(const QuadGeometry& geometry, double x, double y, double* out_x, double* out_y)
{
    *out_x = geometry.x0 + geometry.x1 * x + geometry.x2 * y + geometry.x3 * x * y;
    *out_y = geometry.y0 + geometry.y1 * x + geometry.y2 * y + geometry.y3 * x * y;
}

int normalize_angle_degrees(double angle, double* out_angle)
{
    if (!std::isfinite(angle) || !out_angle) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    double normalized = std::fmod(angle, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    if (normalized == 360.0) {
        normalized = 0.0;
    }
    *out_angle = normalized;
    return PILLOW_C_OK;
}

bool rotate_fast_path_method(
    double normalized_angle,
    bool expand,
    bool has_center,
    bool has_translate,
    const PillowCImage* source,
    int* out_method)
{
    if (!source || !out_method || has_center || has_translate) {
        return false;
    }
    if (normalized_angle == 180.0) {
        *out_method = 3;
        return true;
    }
    if ((normalized_angle == 90.0 || normalized_angle == 270.0) && (expand || source->width == source->height)) {
        *out_method = normalized_angle == 90.0 ? 2 : 4;
        return true;
    }
    return false;
}

int rotate_affine_geometry(
    const PillowCImage* source,
    double angle,
    bool expand,
    double center_x,
    double center_y,
    bool has_center,
    double translate_x,
    double translate_y,
    bool has_translate,
    AffineGeometry* out_geometry)
{
    if (!source || !out_geometry) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!std::isfinite(center_x) || !std::isfinite(center_y) ||
        !std::isfinite(translate_x) || !std::isfinite(translate_y)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    double normalized_angle = 0.0;
    const int angle_status = normalize_angle_degrees(angle, &normalized_angle);
    if (angle_status != PILLOW_C_OK) {
        return angle_status;
    }

    const double cx = has_center ? center_x : static_cast<double>(source->width) / 2.0;
    const double cy = has_center ? center_y : static_cast<double>(source->height) / 2.0;
    const double tx = has_translate ? translate_x : 0.0;
    const double ty = has_translate ? translate_y : 0.0;
    constexpr double pi = 3.1415926535897932384626433832795;
    const double radians = -normalized_angle * pi / 180.0;

    AffineGeometry geometry{
        round_15(std::cos(radians)),
        round_15(std::sin(radians)),
        0.0,
        round_15(-std::sin(radians)),
        round_15(std::cos(radians)),
        0.0,
        source->width,
        source->height};

    affine_transform_point(geometry, -cx - tx, -cy - ty, &geometry.c, &geometry.f);
    geometry.c += cx;
    geometry.f += cy;

    if (expand) {
        double xx[4]{};
        double yy[4]{};
        const double w = static_cast<double>(source->width);
        const double h = static_cast<double>(source->height);
        const double corners[4][2]{{0.0, 0.0}, {w, 0.0}, {w, h}, {0.0, h}};
        for (int i = 0; i < 4; ++i) {
            affine_transform_point(geometry, corners[i][0], corners[i][1], &xx[i], &yy[i]);
        }
        const double min_x = *std::min_element(xx, xx + 4);
        const double max_x = *std::max_element(xx, xx + 4);
        const double min_y = *std::min_element(yy, yy + 4);
        const double max_y = *std::max_element(yy, yy + 4);
        const double new_width = std::ceil(max_x) - std::floor(min_x);
        const double new_height = std::ceil(max_y) - std::floor(min_y);
        if (new_width < 0.0 || new_height < 0.0 ||
            new_width > static_cast<double>(std::numeric_limits<int>::max()) ||
            new_height > static_cast<double>(std::numeric_limits<int>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        geometry.width = static_cast<int>(new_width);
        geometry.height = static_cast<int>(new_height);
        affine_transform_point(
            geometry,
            -(static_cast<double>(geometry.width) - source->width) / 2.0,
            -(static_cast<double>(geometry.height) - source->height) / 2.0,
            &geometry.c,
            &geometry.f);
    }

    *out_geometry = geometry;
    return PILLOW_C_OK;
}

int rotate_output_shape(
    const PillowCImage* source,
    double angle,
    bool expand,
    double center_x,
    double center_y,
    bool has_center,
    double translate_x,
    double translate_y,
    bool has_translate,
    int* out_width,
    int* out_height)
{
    if (!source || !out_width || !out_height) {
        return PILLOW_C_NULL_POINTER;
    }
    double normalized_angle = 0.0;
    int status = normalize_angle_degrees(angle, &normalized_angle);
    if (status != PILLOW_C_OK) {
        return status;
    }
    int method = 0;
    if (rotate_fast_path_method(normalized_angle, expand, has_center, has_translate, source, &method)) {
        return transpose_output_shape(source, method, out_width, out_height) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    }
    if (!has_center && !has_translate && normalized_angle == 0.0) {
        *out_width = source->width;
        *out_height = source->height;
        return PILLOW_C_OK;
    }

    AffineGeometry geometry{};
    status = rotate_affine_geometry(
        source,
        normalized_angle,
        expand,
        center_x,
        center_y,
        has_center,
        translate_x,
        translate_y,
        has_translate,
        &geometry);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_width = geometry.width;
    *out_height = geometry.height;
    return PILLOW_C_OK;
}

int normalize_transform_fill(
    const PillowCImage* source,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    std::uint8_t* out_fill)
{
    if (!source || !out_fill) {
        return PILLOW_C_NULL_POINTER;
    }
    std::fill(out_fill, out_fill + source->channels, static_cast<std::uint8_t>(0));
    if (!fill_color) {
        return fill_color_size == 0 ? PILLOW_C_OK : PILLOW_C_NULL_POINTER;
    }
    if (fill_color_size != 1 && fill_color_size != static_cast<std::size_t>(source->channels) &&
        !(source->mode == PILLOW_C_MODE_RGB && fill_color_size == 4) &&
        !(source->mode == PILLOW_C_MODE_RGBA && fill_color_size == 3)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (fill_color_size == 1) {
        out_fill[0] = fill_color[0];
        return PILLOW_C_OK;
    }
    const std::size_t copy_size = std::min(fill_color_size, static_cast<std::size_t>(source->channels));
    std::memcpy(out_fill, fill_color, copy_size);
    if (source->mode == PILLOW_C_MODE_RGBA && fill_color_size == 3) {
        out_fill[3] = 255;
    }
    return PILLOW_C_OK;
}

int nearest_transform_coordinate(double value)
{
    return static_cast<int>(std::floor(value + 0.5));
}

int clamp_index(int value, int upper_exclusive)
{
    if (value < 0) {
        return 0;
    }
    if (value >= upper_exclusive) {
        return upper_exclusive - 1;
    }
    return value;
}

std::uint8_t transform_sample_channel(
    const PillowCImage* source,
    int x,
    int y,
    int channel,
    bool premultiply_alpha)
{
    const std::uint8_t* px =
        source->pixels.data() +
        static_cast<std::size_t>(y) * source->stride +
        static_cast<std::size_t>(x) * source->channels;
    if (premultiply_alpha && source->mode == PILLOW_C_MODE_RGBA && channel < 3) {
        return mul_div_255(px[channel], px[3]);
    }
    return px[channel];
}

std::uint8_t bilinear_transform_channel(
    const PillowCImage* source,
    double source_x,
    double source_y,
    int channel,
    bool premultiply_alpha)
{
    source_x -= 0.5;
    source_y -= 0.5;
    const int x = static_cast<int>(std::floor(source_x));
    const int y = static_cast<int>(std::floor(source_y));
    const double dx = source_x - x;
    const double dy = source_y - y;
    const int x0 = clamp_index(x, source->width);
    const int x1 = clamp_index(x + 1, source->width);
    const int y0 = clamp_index(y, source->height);
    const int y1 = (y + 1 >= 0 && y + 1 < source->height) ? y + 1 : y0;

    const std::uint8_t* row0 = source->pixels.data() + static_cast<std::size_t>(y0) * source->stride;
    const std::uint8_t* row1 = source->pixels.data() + static_cast<std::size_t>(y1) * source->stride;
    const std::size_t offset0 = static_cast<std::size_t>(x0) * source->channels + channel;
    const std::size_t offset1 = static_cast<std::size_t>(x1) * source->channels + channel;
    const auto sample = [source, channel, premultiply_alpha](const std::uint8_t* row, std::size_t offset) -> std::uint8_t {
        const std::uint8_t* px = row + offset - static_cast<std::size_t>(channel);
        if (premultiply_alpha && source->mode == PILLOW_C_MODE_RGBA && channel < 3) {
            return mul_div_255(px[channel], px[3]);
        }
        return px[channel];
    };
    const double top_left = sample(row0, offset0);
    const double top_right = sample(row0, offset1);
    const double bottom_left = sample(row1, offset0);
    const double bottom_right = sample(row1, offset1);
    const double v1 = top_left + (top_right - top_left) * dx;
    const double v2 = bottom_left + (bottom_right - bottom_left) * dx;
    const double value = v1 + (v2 - v1) * dy;
    return static_cast<std::uint8_t>(value);
}

double bicubic_interpolate(double v1, double v2, double v3, double v4, double d)
{
    const double p1 = v2;
    const double p2 = -v1 + v3;
    const double p3 = 2.0 * (v1 - v2) + v3 - v4;
    const double p4 = -v1 + v2 - v3 + v4;
    return p1 + d * (p2 + d * (p3 + d * p4));
}

std::uint8_t bicubic_transform_channel(
    const PillowCImage* source,
    double source_x,
    double source_y,
    int channel,
    bool premultiply_alpha)
{
    source_x -= 0.5;
    source_y -= 0.5;
    int x = static_cast<int>(std::floor(source_x));
    int y = static_cast<int>(std::floor(source_y));
    const double dx = source_x - x;
    const double dy = source_y - y;
    --x;
    --y;

    const int x0 = clamp_index(x, source->width);
    const int x1 = clamp_index(x + 1, source->width);
    const int x2 = clamp_index(x + 2, source->width);
    const int x3 = clamp_index(x + 3, source->width);
    const auto horizontal_value = [source, channel, premultiply_alpha, x0, x1, x2, x3, dx](int row_y) -> double {
        return bicubic_interpolate(
            transform_sample_channel(source, x0, row_y, channel, premultiply_alpha),
            transform_sample_channel(source, x1, row_y, channel, premultiply_alpha),
            transform_sample_channel(source, x2, row_y, channel, premultiply_alpha),
            transform_sample_channel(source, x3, row_y, channel, premultiply_alpha),
            dx);
    };
    const double v1 = horizontal_value(clamp_index(y, source->height));
    const double v2 = y + 1 >= 0 && y + 1 < source->height ? horizontal_value(y + 1) : v1;
    const double v3 = y + 2 >= 0 && y + 2 < source->height ? horizontal_value(y + 2) : v2;
    const double v4 = y + 3 >= 0 && y + 3 < source->height ? horizontal_value(y + 3) : v3;

    return clip_u8_double(bicubic_interpolate(v1, v2, v3, v4, dy));
}

void write_transform_values(const PillowCImage* source, const std::uint8_t* values, std::uint8_t* dst)
{
    if (source->mode == PILLOW_C_MODE_RGBA) {
        const std::uint8_t alpha = values[3];
        if (alpha == 0 || alpha == 255) {
            dst[0] = values[0];
            dst[1] = values[1];
            dst[2] = values[2];
        } else {
            dst[0] = clip_u8_int(255 * values[0] / alpha);
            dst[1] = clip_u8_int(255 * values[1] / alpha);
            dst[2] = clip_u8_int(255 * values[2] / alpha);
        }
        dst[3] = alpha;
    } else {
        for (int channel = 0; channel < source->channels; ++channel) {
            dst[channel] = values[channel];
        }
    }
}

int rotate_nearest_into(
    const PillowCImage* source,
    double angle,
    bool expand,
    double center_x,
    double center_y,
    bool has_center,
    double translate_x,
    double translate_y,
    bool has_translate,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }

    double normalized_angle = 0.0;
    int status = normalize_angle_degrees(angle, &normalized_angle);
    if (status != PILLOW_C_OK) {
        return status;
    }
    int method = 0;
    if (rotate_fast_path_method(normalized_angle, expand, has_center, has_translate, source, &method)) {
        return copy_transpose_pixels_into(source, method, target);
    }
    if (!has_center && !has_translate && normalized_angle == 0.0) {
        if (!image_shape_matches(target, source)) {
            return PILLOW_C_MISMATCH;
        }
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        return PILLOW_C_OK;
    }

    AffineGeometry geometry{};
    status = rotate_affine_geometry(
        source,
        normalized_angle,
        expand,
        center_x,
        center_y,
        has_center,
        translate_x,
        translate_y,
        has_translate,
        &geometry);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!image_shape_matches(target, geometry.width, geometry.height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    std::uint8_t fill[4]{0, 0, 0, 0};
    status = normalize_transform_fill(source, fill_color, fill_color_size, fill);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::size_t pixel_bytes = static_cast<std::size_t>(source->channels);
    for (int dst_y = 0; dst_y < target->height; ++dst_y) {
        std::uint8_t* dst_row = target->pixels.data() + static_cast<std::size_t>(dst_y) * target->stride;
        for (int dst_x = 0; dst_x < target->width; ++dst_x) {
            double src_x_value = 0.0;
            double src_y_value = 0.0;
            affine_transform_point(
                geometry,
                static_cast<double>(dst_x) + 0.5,
                static_cast<double>(dst_y) + 0.5,
                &src_x_value,
                &src_y_value);
            src_x_value -= 0.5;
            src_y_value -= 0.5;
            const int src_x = nearest_transform_coordinate(src_x_value);
            const int src_y = nearest_transform_coordinate(src_y_value);
            std::uint8_t* dst = dst_row + static_cast<std::size_t>(dst_x) * pixel_bytes;
            if (src_x < 0 || src_y < 0 || src_x >= source->width || src_y >= source->height) {
                std::memcpy(dst, fill, pixel_bytes);
                continue;
            }
            const std::uint8_t* src =
                source->pixels.data() +
                static_cast<std::size_t>(src_y) * source->stride +
                static_cast<std::size_t>(src_x) * pixel_bytes;
            std::memcpy(dst, src, pixel_bytes);
        }
    }
    return PILLOW_C_OK;
}

int rotate_bilinear_into(
    const PillowCImage* source,
    double angle,
    bool expand,
    double center_x,
    double center_y,
    bool has_center,
    double translate_x,
    double translate_y,
    bool has_translate,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }

    double normalized_angle = 0.0;
    int status = normalize_angle_degrees(angle, &normalized_angle);
    if (status != PILLOW_C_OK) {
        return status;
    }
    int method = 0;
    if (rotate_fast_path_method(normalized_angle, expand, has_center, has_translate, source, &method)) {
        return copy_transpose_pixels_into(source, method, target);
    }
    if (!has_center && !has_translate && normalized_angle == 0.0) {
        if (!image_shape_matches(target, source)) {
            return PILLOW_C_MISMATCH;
        }
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        return PILLOW_C_OK;
    }

    AffineGeometry geometry{};
    status = rotate_affine_geometry(
        source,
        normalized_angle,
        expand,
        center_x,
        center_y,
        has_center,
        translate_x,
        translate_y,
        has_translate,
        &geometry);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!image_shape_matches(target, geometry.width, geometry.height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    std::uint8_t fill[4]{0, 0, 0, 0};
    status = normalize_transform_fill(source, fill_color, fill_color_size, fill);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::size_t pixel_bytes = static_cast<std::size_t>(source->channels);
    for (int dst_y = 0; dst_y < target->height; ++dst_y) {
        std::uint8_t* dst_row = target->pixels.data() + static_cast<std::size_t>(dst_y) * target->stride;
        for (int dst_x = 0; dst_x < target->width; ++dst_x) {
            double source_x = 0.0;
            double source_y = 0.0;
            affine_transform_point(
                geometry,
                static_cast<double>(dst_x) + 0.5,
                static_cast<double>(dst_y) + 0.5,
                &source_x,
                &source_y);
            std::uint8_t* dst = dst_row + static_cast<std::size_t>(dst_x) * pixel_bytes;
            if (source_x < 0.0 || source_y < 0.0 || source_x >= source->width || source_y >= source->height) {
                write_transform_values(source, fill, dst);
                continue;
            }
            std::uint8_t values[4]{0, 0, 0, 0};
            for (int channel = 0; channel < source->channels; ++channel) {
                values[channel] = bilinear_transform_channel(source, source_x, source_y, channel, source->mode == PILLOW_C_MODE_RGBA);
            }
            write_transform_values(source, values, dst);
        }
    }
    return PILLOW_C_OK;
}

int rotate_bicubic_into(
    const PillowCImage* source,
    double angle,
    bool expand,
    double center_x,
    double center_y,
    bool has_center,
    double translate_x,
    double translate_y,
    bool has_translate,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }

    double normalized_angle = 0.0;
    int status = normalize_angle_degrees(angle, &normalized_angle);
    if (status != PILLOW_C_OK) {
        return status;
    }
    int method = 0;
    if (rotate_fast_path_method(normalized_angle, expand, has_center, has_translate, source, &method)) {
        return copy_transpose_pixels_into(source, method, target);
    }
    if (!has_center && !has_translate && normalized_angle == 0.0) {
        if (!image_shape_matches(target, source)) {
            return PILLOW_C_MISMATCH;
        }
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        return PILLOW_C_OK;
    }

    AffineGeometry geometry{};
    status = rotate_affine_geometry(
        source,
        normalized_angle,
        expand,
        center_x,
        center_y,
        has_center,
        translate_x,
        translate_y,
        has_translate,
        &geometry);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!image_shape_matches(target, geometry.width, geometry.height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    std::uint8_t fill[4]{0, 0, 0, 0};
    status = normalize_transform_fill(source, fill_color, fill_color_size, fill);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::size_t pixel_bytes = static_cast<std::size_t>(source->channels);
    for (int dst_y = 0; dst_y < target->height; ++dst_y) {
        std::uint8_t* dst_row = target->pixels.data() + static_cast<std::size_t>(dst_y) * target->stride;
        for (int dst_x = 0; dst_x < target->width; ++dst_x) {
            double source_x = 0.0;
            double source_y = 0.0;
            affine_transform_point(
                geometry,
                static_cast<double>(dst_x) + 0.5,
                static_cast<double>(dst_y) + 0.5,
                &source_x,
                &source_y);
            std::uint8_t* dst = dst_row + static_cast<std::size_t>(dst_x) * pixel_bytes;
            if (source_x < 0.0 || source_y < 0.0 || source_x >= source->width || source_y >= source->height) {
                write_transform_values(source, fill, dst);
                continue;
            }
            std::uint8_t values[4]{0, 0, 0, 0};
            for (int channel = 0; channel < source->channels; ++channel) {
                values[channel] = bicubic_transform_channel(source, source_x, source_y, channel, source->mode == PILLOW_C_MODE_RGBA);
            }
            write_transform_values(source, values, dst);
        }
    }
    return PILLOW_C_OK;
}

int rotate_image_into(
    const PillowCImage* source,
    double angle,
    int resample,
    bool expand,
    double center_x,
    double center_y,
    bool has_center,
    double translate_x,
    double translate_y,
    bool has_translate,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    if (resample == PILLOW_C_RESAMPLE_NEAREST) {
        return rotate_nearest_into(
            source,
            angle,
            expand,
            center_x,
            center_y,
            has_center,
            translate_x,
            translate_y,
            has_translate,
            fill_color,
            fill_color_size,
            target);
    }
    if (resample == PILLOW_C_RESAMPLE_BILINEAR) {
        return rotate_bilinear_into(
            source,
            angle,
            expand,
            center_x,
            center_y,
            has_center,
            translate_x,
            translate_y,
            has_translate,
            fill_color,
            fill_color_size,
            target);
    }
    if (resample == PILLOW_C_RESAMPLE_BICUBIC) {
        return rotate_bicubic_into(
            source,
            angle,
            expand,
            center_x,
            center_y,
            has_center,
            translate_x,
            translate_y,
            has_translate,
            fill_color,
            fill_color_size,
            target);
    }
    return PILLOW_C_INVALID_ARGUMENT;
}

bool supported_affine_transform_resample(int resample)
{
    return resample == PILLOW_C_RESAMPLE_NEAREST ||
           resample == PILLOW_C_RESAMPLE_BILINEAR ||
           resample == PILLOW_C_RESAMPLE_BICUBIC;
}

template <typename MapPoint>
int transform_with_mapper_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target,
    MapPoint map_point)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!supported_affine_transform_resample(resample) || out_width < 0 || out_height < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    std::uint8_t fill[4]{0, 0, 0, 0};
    int status = normalize_transform_fill(source, fill_color, fill_color_size, fill);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::size_t pixel_bytes = static_cast<std::size_t>(source->channels);
    for (int dst_y = 0; dst_y < target->height; ++dst_y) {
        std::uint8_t* dst_row = target->pixels.data() + static_cast<std::size_t>(dst_y) * target->stride;
        for (int dst_x = 0; dst_x < target->width; ++dst_x) {
            double source_x = 0.0;
            double source_y = 0.0;
            const bool valid_point = map_point(
                static_cast<double>(dst_x) + 0.5,
                static_cast<double>(dst_y) + 0.5,
                &source_x,
                &source_y);
            std::uint8_t* dst = dst_row + static_cast<std::size_t>(dst_x) * pixel_bytes;
            if (!valid_point) {
                write_transform_values(source, fill, dst);
                continue;
            }
            if (resample == PILLOW_C_RESAMPLE_NEAREST) {
                const int src_x = source_x < 0.0 ? -1 : static_cast<int>(source_x);
                const int src_y = source_y < 0.0 ? -1 : static_cast<int>(source_y);
                if (src_x < 0 || src_y < 0 || src_x >= source->width || src_y >= source->height) {
                    std::memcpy(dst, fill, pixel_bytes);
                    continue;
                }
                const std::uint8_t* src =
                    source->pixels.data() +
                    static_cast<std::size_t>(src_y) * source->stride +
                    static_cast<std::size_t>(src_x) * pixel_bytes;
                std::memcpy(dst, src, pixel_bytes);
                continue;
            }
            if (source_x < 0.0 || source_y < 0.0 || source_x >= source->width || source_y >= source->height) {
                write_transform_values(source, fill, dst);
                continue;
            }
            std::uint8_t values[4]{0, 0, 0, 0};
            for (int channel = 0; channel < source->channels; ++channel) {
                values[channel] = resample == PILLOW_C_RESAMPLE_BILINEAR
                    ? bilinear_transform_channel(source, source_x, source_y, channel, source->mode == PILLOW_C_MODE_RGBA)
                    : bicubic_transform_channel(source, source_x, source_y, channel, source->mode == PILLOW_C_MODE_RGBA);
            }
            write_transform_values(source, values, dst);
        }
    }
    return PILLOW_C_OK;
}

int affine_transform_image_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const double* matrix,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    if (!source || !matrix || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    for (int i = 0; i < 6; ++i) {
        if (!std::isfinite(matrix[i])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }

    const AffineGeometry geometry{
        matrix[0],
        matrix[1],
        matrix[2],
        matrix[3],
        matrix[4],
        matrix[5],
        out_width,
        out_height};
    return transform_with_mapper_into(
        source,
        out_width,
        out_height,
        resample,
        fill_color,
        fill_color_size,
        target,
        [&geometry](double x, double y, double* out_x, double* out_y) -> bool {
            affine_transform_point(
                geometry,
                x,
                y,
                out_x,
                out_y);
            return true;
        });
}

int perspective_transform_image_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const double* coefficients,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    if (!source || !coefficients || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    for (int i = 0; i < 8; ++i) {
        if (!std::isfinite(coefficients[i])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }

    const PerspectiveGeometry geometry{
        coefficients[0],
        coefficients[1],
        coefficients[2],
        coefficients[3],
        coefficients[4],
        coefficients[5],
        coefficients[6],
        coefficients[7],
        out_width,
        out_height};
    return transform_with_mapper_into(
        source,
        out_width,
        out_height,
        resample,
        fill_color,
        fill_color_size,
        target,
        [&geometry](double x, double y, double* out_x, double* out_y) -> bool {
            return perspective_transform_point(geometry, x, y, out_x, out_y);
        });
}

int quad_transform_image_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const double* corners,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    if (!source || !corners || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_width <= 0 || out_height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (int i = 0; i < 8; ++i) {
        if (!std::isfinite(corners[i])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }

    const double x0 = corners[0];
    const double y0 = corners[1];
    const double sw_x = corners[2];
    const double sw_y = corners[3];
    const double se_x = corners[4];
    const double se_y = corners[5];
    const double ne_x = corners[6];
    const double ne_y = corners[7];
    const double as = 1.0 / static_cast<double>(out_width);
    const double at = 1.0 / static_cast<double>(out_height);
    const QuadGeometry geometry{
        x0,
        (ne_x - x0) * as,
        (sw_x - x0) * at,
        (se_x - sw_x - ne_x + x0) * as * at,
        y0,
        (ne_y - y0) * as,
        (sw_y - y0) * at,
        (se_y - sw_y - ne_y + y0) * as * at,
        out_width,
        out_height};

    return transform_with_mapper_into(
        source,
        out_width,
        out_height,
        resample,
        fill_color,
        fill_color_size,
        target,
        [&geometry](double x, double y, double* out_x, double* out_y) -> bool {
            quad_transform_point(geometry, x, y, out_x, out_y);
            return std::isfinite(*out_x) && std::isfinite(*out_y);
        });
}

int fill_image_storage_with_color(PillowCImage* target, const std::uint8_t* fill)
{
    if (!target || !fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }
    const std::size_t pixel_bytes = static_cast<std::size_t>(target->channels);
    for (int y = 0; y < target->height; ++y) {
        std::uint8_t* row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < target->width; ++x) {
            std::memcpy(row + static_cast<std::size_t>(x) * pixel_bytes, fill, pixel_bytes);
        }
    }
    return PILLOW_C_OK;
}

int mesh_transform_image_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const int* boxes,
    const double* quads,
    std::size_t mesh_count,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    if (!source || !target || (mesh_count > 0 && (!boxes || !quads))) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!supported_affine_transform_resample(resample) || out_width < 0 || out_height < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }
    if (mesh_count > static_cast<std::size_t>(-1) / 8 || mesh_count > static_cast<std::size_t>(-1) / 4) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::uint8_t fill[4]{0, 0, 0, 0};
    int status = normalize_transform_fill(source, fill_color, fill_color_size, fill);
    if (status != PILLOW_C_OK) {
        return status;
    }
    status = fill_image_storage_with_color(target, fill);
    if (status != PILLOW_C_OK || target->pixels.empty()) {
        return status;
    }

    for (std::size_t mesh_index = 0; mesh_index < mesh_count; ++mesh_index) {
        const int* box = boxes + mesh_index * 4;
        const double* quad = quads + mesh_index * 8;
        const int left = box[0];
        const int top = box[1];
        const int right = box[2];
        const int bottom = box[3];
        if (right <= left || bottom <= top) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        for (int i = 0; i < 8; ++i) {
            if (!std::isfinite(quad[i])) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }

        const double box_width = static_cast<double>(right - left);
        const double box_height = static_cast<double>(bottom - top);
        const double x0 = quad[0];
        const double y0 = quad[1];
        const double sw_x = quad[2];
        const double sw_y = quad[3];
        const double se_x = quad[4];
        const double se_y = quad[5];
        const double ne_x = quad[6];
        const double ne_y = quad[7];
        const double as = 1.0 / box_width;
        const double at = 1.0 / box_height;
        const QuadGeometry geometry{
            x0,
            (ne_x - x0) * as,
            (sw_x - x0) * at,
            (se_x - sw_x - ne_x + x0) * as * at,
            y0,
            (ne_y - y0) * as,
            (sw_y - y0) * at,
            (se_y - sw_y - ne_y + y0) * as * at,
            right - left,
            bottom - top};

        const int start_y = std::max(top, 0);
        const int end_y = std::min(bottom, target->height);
        const int start_x = std::max(left, 0);
        const int end_x = std::min(right, target->width);
        const std::size_t pixel_bytes = static_cast<std::size_t>(source->channels);
        for (int dst_y = start_y; dst_y < end_y; ++dst_y) {
            std::uint8_t* dst_row = target->pixels.data() + static_cast<std::size_t>(dst_y) * target->stride;
            for (int dst_x = start_x; dst_x < end_x; ++dst_x) {
                double source_x = 0.0;
                double source_y = 0.0;
                quad_transform_point(
                    geometry,
                    static_cast<double>(dst_x - left) + 0.5,
                    static_cast<double>(dst_y - top) + 0.5,
                    &source_x,
                    &source_y);
                std::uint8_t* dst = dst_row + static_cast<std::size_t>(dst_x) * pixel_bytes;
                if (!std::isfinite(source_x) || !std::isfinite(source_y)) {
                    write_transform_values(source, fill, dst);
                    continue;
                }
                if (resample == PILLOW_C_RESAMPLE_NEAREST) {
                    const int src_x = source_x < 0.0 ? -1 : static_cast<int>(source_x);
                    const int src_y = source_y < 0.0 ? -1 : static_cast<int>(source_y);
                    if (src_x < 0 || src_y < 0 || src_x >= source->width || src_y >= source->height) {
                        std::memcpy(dst, fill, pixel_bytes);
                        continue;
                    }
                    const std::uint8_t* src =
                        source->pixels.data() +
                        static_cast<std::size_t>(src_y) * source->stride +
                        static_cast<std::size_t>(src_x) * pixel_bytes;
                    std::memcpy(dst, src, pixel_bytes);
                    continue;
                }
                if (source_x < 0.0 || source_y < 0.0 || source_x >= source->width || source_y >= source->height) {
                    write_transform_values(source, fill, dst);
                    continue;
                }
                std::uint8_t values[4]{0, 0, 0, 0};
                for (int channel = 0; channel < source->channels; ++channel) {
                    values[channel] = resample == PILLOW_C_RESAMPLE_BILINEAR
                        ? bilinear_transform_channel(source, source_x, source_y, channel, source->mode == PILLOW_C_MODE_RGBA)
                        : bicubic_transform_channel(source, source_x, source_y, channel, source->mode == PILLOW_C_MODE_RGBA);
                }
                write_transform_values(source, values, dst);
            }
        }
    }
    return PILLOW_C_OK;
}

bool precompute_nearest_indices_for_box(
    int src_size,
    int dst_size,
    double box_start,
    double box_end,
    std::vector<int>* indices)
{
    if (src_size <= 0 || dst_size <= 0 || !(box_end > box_start) || !indices) {
        return false;
    }

    indices->assign(static_cast<std::size_t>(dst_size), 0);
    const double scale = (box_end - box_start) / dst_size;
    double source_position = box_start + scale * 0.5;
    for (int dst_index = 0; dst_index < dst_size; ++dst_index) {
        int value = source_position < 0.0 ? -1 : static_cast<int>(source_position);
        if (value < 0) {
            value = 0;
        }
        if (value >= src_size) {
            value = src_size - 1;
        }
        (*indices)[static_cast<std::size_t>(dst_index)] = value;
        source_position += scale;
    }
    return true;
}

bool precompute_nearest_indices(int src_size, int dst_size, std::vector<int>* indices)
{
    return precompute_nearest_indices_for_box(src_size, dst_size, 0.0, static_cast<double>(src_size), indices);
}

double bilinear_filter(double value)
{
    if (value < 0.0) {
        value = -value;
    }
    if (value < 1.0) {
        return 1.0 - value;
    }
    return 0.0;
}

double box_filter(double value)
{
    if (value > -0.5 && value <= 0.5) {
        return 1.0;
    }
    return 0.0;
}

double hamming_filter(double value)
{
    if (value < 0.0) {
        value = -value;
    }
    if (value == 0.0) {
        return 1.0;
    }
    if (value >= 1.0) {
        return 0.0;
    }
    constexpr double pi = 3.1415926535897932384626433832795;
    value *= pi;
    return std::sin(value) / value * (0.54 + 0.46 * std::cos(value));
}

double bicubic_filter(double value)
{
    constexpr double a = -0.5;
    if (value < 0.0) {
        value = -value;
    }
    if (value < 1.0) {
        return ((a + 2.0) * value - (a + 3.0)) * value * value + 1.0;
    }
    if (value < 2.0) {
        return (((value - 5.0) * value + 8.0) * value - 4.0) * a;
    }
    return 0.0;
}

double sinc_filter(double value)
{
    if (value == 0.0) {
        return 1.0;
    }
    constexpr double pi = 3.1415926535897932384626433832795;
    value *= pi;
    return std::sin(value) / value;
}

double lanczos_filter(double value)
{
    if (-3.0 <= value && value < 3.0) {
        return sinc_filter(value) * sinc_filter(value / 3.0);
    }
    return 0.0;
}

const ResampleFilterSpec* filter_spec_for_resample(int resample)
{
    static const ResampleFilterSpec box{0.5, box_filter};
    static const ResampleFilterSpec bilinear{1.0, bilinear_filter};
    static const ResampleFilterSpec hamming{1.0, hamming_filter};
    static const ResampleFilterSpec bicubic{2.0, bicubic_filter};
    static const ResampleFilterSpec lanczos{3.0, lanczos_filter};

    switch (resample) {
    case PILLOW_C_RESAMPLE_BOX:
        return &box;
    case PILLOW_C_RESAMPLE_BILINEAR:
        return &bilinear;
    case PILLOW_C_RESAMPLE_HAMMING:
        return &hamming;
    case PILLOW_C_RESAMPLE_BICUBIC:
        return &bicubic;
    case PILLOW_C_RESAMPLE_LANCZOS:
        return &lanczos;
    default:
        return nullptr;
    }
}

bool precompute_filter_coefficients_for_box(
    int in_size,
    int out_size,
    double box_start,
    double box_end,
    const ResampleFilterSpec& filter,
    ResampleCoefficients* coeffs)
{
    if (in_size <= 0 || out_size <= 0 || !(box_end > box_start) || !coeffs) {
        return false;
    }

    double filterscale = (box_end - box_start) / out_size;
    if (filterscale < 1.0) {
        filterscale = 1.0;
    }
    const double support = filter.support * filterscale;
    const int kernel_size = static_cast<int>(std::ceil(support)) * 2 + 1;
    if (kernel_size <= 0) {
        return false;
    }

    coeffs->kernel_size = kernel_size;
    coeffs->bounds.assign(static_cast<std::size_t>(out_size) * 2u, 0);
    coeffs->weights.assign(static_cast<std::size_t>(out_size) * kernel_size, 0);

    const double scale = (box_end - box_start) / out_size;
    const double ss = 1.0 / filterscale;
    std::vector<double> normalized(static_cast<std::size_t>(kernel_size), 0.0);
    for (int out_index = 0; out_index < out_size; ++out_index) {
        const double center = box_start + (out_index + 0.5) * scale;
        int xmin = static_cast<int>(center - support + 0.5);
        if (xmin < 0) {
            xmin = 0;
        }
        int xmax = static_cast<int>(center + support + 0.5);
        if (xmax > in_size) {
            xmax = in_size;
        }
        const int count = xmax - xmin;
        double sum = 0.0;
        std::fill(normalized.begin(), normalized.end(), 0.0);
        for (int i = 0; i < count; ++i) {
            const double weight = filter.filter((i + xmin - center + 0.5) * ss);
            normalized[static_cast<std::size_t>(i)] = weight;
            sum += weight;
        }
        if (sum != 0.0) {
            for (int i = 0; i < count; ++i) {
                normalized[static_cast<std::size_t>(i)] /= sum;
            }
        }
        std::int32_t* weights =
            coeffs->weights.data() +
            static_cast<std::size_t>(out_index) * kernel_size;
        for (int i = 0; i < kernel_size; ++i) {
            const double scaled = normalized[static_cast<std::size_t>(i)] * RESAMPLE_PRECISION_SCALE;
            weights[i] = scaled < 0.0 ?
                static_cast<std::int32_t>(-0.5 + scaled) :
                static_cast<std::int32_t>(0.5 + scaled);
        }
        coeffs->bounds[static_cast<std::size_t>(out_index) * 2u] = xmin;
        coeffs->bounds[static_cast<std::size_t>(out_index) * 2u + 1u] = count;
    }
    return true;
}

bool precompute_filter_coefficients(int in_size, int out_size, const ResampleFilterSpec& filter, ResampleCoefficients* coeffs)
{
    return precompute_filter_coefficients_for_box(
        in_size,
        out_size,
        0.0,
        static_cast<double>(in_size),
        filter,
        coeffs);
}

std::uint8_t source_sample_for_resize(const PillowCImage* source, int x, int y, int channel)
{
    const std::uint8_t* px =
        source->pixels.data() +
        static_cast<std::size_t>(y) * source->stride +
        static_cast<std::size_t>(x) * source->channels;
    if (source->mode == PILLOW_C_MODE_RGBA && channel < 3) {
        return mul_div_255(px[channel], px[3]);
    }
    return px[channel];
}

bool valid_resize_box(const PillowCImage* source, double left, double top, double right, double bottom)
{
    return source &&
           std::isfinite(left) &&
           std::isfinite(top) &&
           std::isfinite(right) &&
           std::isfinite(bottom) &&
           right > left &&
           bottom > top;
}

int resize_filter_box_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    double box_left,
    double box_top,
    double box_right,
    double box_bottom,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_width <= 0 || out_height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!valid_resize_box(source, box_left, box_top, box_right, box_bottom)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    try {
        const ResampleFilterSpec* filter = filter_spec_for_resample(resample);
        if (!filter) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ResampleCoefficients x_coeffs{};
        ResampleCoefficients y_coeffs{};
        if (!precompute_filter_coefficients_for_box(source->width, out_width, box_left, box_right, *filter, &x_coeffs) ||
            !precompute_filter_coefficients_for_box(source->height, out_height, box_top, box_bottom, *filter, &y_coeffs)) {
            return PILLOW_C_ALLOCATION_FAILED;
        }

        std::vector<std::uint8_t> temp(
            static_cast<std::size_t>(out_width) *
            static_cast<std::size_t>(source->height) *
            static_cast<std::size_t>(source->channels),
            0);

        for (int y = 0; y < source->height; ++y) {
            for (int out_x = 0; out_x < out_width; ++out_x) {
                const int xmin = x_coeffs.bounds[static_cast<std::size_t>(out_x) * 2u];
                const int count = x_coeffs.bounds[static_cast<std::size_t>(out_x) * 2u + 1u];
                const std::int32_t* weights =
                    x_coeffs.weights.data() +
                    static_cast<std::size_t>(out_x) * x_coeffs.kernel_size;
                for (int channel = 0; channel < source->channels; ++channel) {
                    std::int64_t sum = RESAMPLE_ROUNDING_BIAS;
                    for (int i = 0; i < count; ++i) {
                        sum += source_sample_for_resize(source, xmin + i, y, channel) * weights[i];
                    }
                    temp[(static_cast<std::size_t>(y) * out_width + out_x) * source->channels + channel] =
                        clip_resample_u8(sum);
                }
            }
        }

        for (int out_y = 0; out_y < out_height; ++out_y) {
            const int ymin = y_coeffs.bounds[static_cast<std::size_t>(out_y) * 2u];
            const int count = y_coeffs.bounds[static_cast<std::size_t>(out_y) * 2u + 1u];
            const std::int32_t* weights =
                y_coeffs.weights.data() +
                static_cast<std::size_t>(out_y) * y_coeffs.kernel_size;
            for (int out_x = 0; out_x < out_width; ++out_x) {
                std::uint8_t values[4] = {0, 0, 0, 0};
                for (int channel = 0; channel < source->channels; ++channel) {
                    std::int64_t sum = RESAMPLE_ROUNDING_BIAS;
                    for (int i = 0; i < count; ++i) {
                        sum += temp[(static_cast<std::size_t>(ymin + i) * out_width + out_x) * source->channels + channel] * weights[i];
                    }
                    values[channel] = clip_resample_u8(sum);
                }

                std::uint8_t* dst =
                    target->pixels.data() +
                    static_cast<std::size_t>(out_y) * target->stride +
                    static_cast<std::size_t>(out_x) * target->channels;
                if (source->mode == PILLOW_C_MODE_RGBA) {
                    const std::uint8_t premul_r = values[0];
                    const std::uint8_t premul_g = values[1];
                    const std::uint8_t premul_b = values[2];
                    const std::uint8_t alpha = values[3];
                    if (alpha == 0 || alpha == 255) {
                        dst[0] = premul_r;
                        dst[1] = premul_g;
                        dst[2] = premul_b;
                    } else {
                        dst[0] = clip_u8_int(255 * premul_r / alpha);
                        dst[1] = clip_u8_int(255 * premul_g / alpha);
                        dst[2] = clip_u8_int(255 * premul_b / alpha);
                    }
                    dst[3] = alpha;
                } else {
                    for (int channel = 0; channel < source->channels; ++channel) {
                        dst[channel] = values[channel];
                    }
                }
            }
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int resize_filter_into(const PillowCImage* source, int out_width, int out_height, int resample, PillowCImage* target)
{
    return resize_filter_box_into(
        source,
        out_width,
        out_height,
        resample,
        0.0,
        0.0,
        source ? static_cast<double>(source->width) : 0.0,
        source ? static_cast<double>(source->height) : 0.0,
        target);
}

int resize_nearest_box_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    double box_left,
    double box_top,
    double box_right,
    double box_bottom,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_width <= 0 || out_height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!valid_resize_box(source, box_left, box_top, box_right, box_bottom)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    try {
        std::vector<int> x_indices;
        std::vector<int> y_indices;
        if (!precompute_nearest_indices_for_box(source->width, out_width, box_left, box_right, &x_indices) ||
            !precompute_nearest_indices_for_box(source->height, out_height, box_top, box_bottom, &y_indices)) {
            return PILLOW_C_ALLOCATION_FAILED;
        }

        for (int y = 0; y < out_height; ++y) {
            const int src_y = y_indices[static_cast<std::size_t>(y)];
            for (int x = 0; x < out_width; ++x) {
                const int src_x = x_indices[static_cast<std::size_t>(x)];
                const std::size_t src_offset =
                    static_cast<std::size_t>(src_y) * source->stride +
                    static_cast<std::size_t>(src_x) * source->channels;
                const std::size_t dst_offset =
                    static_cast<std::size_t>(y) * target->stride +
                    static_cast<std::size_t>(x) * target->channels;
                std::memcpy(
                    target->pixels.data() + dst_offset,
                    source->pixels.data() + src_offset,
                    static_cast<std::size_t>(source->channels));
            }
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int resize_nearest_into(const PillowCImage* source, int out_width, int out_height, PillowCImage* target)
{
    return resize_nearest_box_into(
        source,
        out_width,
        out_height,
        0.0,
        0.0,
        source ? static_cast<double>(source->width) : 0.0,
        source ? static_cast<double>(source->height) : 0.0,
        target);
}

int resize_image_into(const PillowCImage* source, int out_width, int out_height, int resample, PillowCImage* target)
{
    const bool supported =
        resample == PILLOW_C_RESAMPLE_NEAREST ||
        filter_spec_for_resample(resample) != nullptr;
    if (!supported) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source && target &&
        out_width == source->width &&
        out_height == source->height &&
        image_shape_matches(target, source)) {
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        return PILLOW_C_OK;
    }

    switch (resample) {
    case PILLOW_C_RESAMPLE_NEAREST:
        return resize_nearest_into(source, out_width, out_height, target);
    case PILLOW_C_RESAMPLE_BOX:
    case PILLOW_C_RESAMPLE_HAMMING:
    case PILLOW_C_RESAMPLE_BILINEAR:
    case PILLOW_C_RESAMPLE_BICUBIC:
    case PILLOW_C_RESAMPLE_LANCZOS:
        return resize_filter_into(source, out_width, out_height, resample, target);
    default:
        return PILLOW_C_INVALID_ARGUMENT;
    }
}

bool supported_kernel_size(int kernel_width, int kernel_height)
{
    return (kernel_width == 3 && kernel_height == 3) ||
           (kernel_width == 5 && kernel_height == 5);
}

int filter_kernel_image_into(
    const PillowCImage* source,
    int kernel_width,
    int kernel_height,
    const double* kernel,
    std::size_t kernel_count,
    double scale,
    double offset,
    PillowCImage* target)
{
    if (!source || !kernel || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (!supported_kernel_size(kernel_width, kernel_height) || !std::isfinite(scale) || !std::isfinite(offset)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const std::size_t expected_count = static_cast<std::size_t>(kernel_width) * kernel_height;
    if (kernel_count != expected_count) {
        return PILLOW_C_INVALID_LENGTH;
    }
    for (std::size_t index = 0; index < kernel_count; ++index) {
        if (!std::isfinite(kernel[index])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }

    std::vector<std::uint8_t> source_snapshot;
    const std::uint8_t* source_data = source->pixels.data();
    if (source == target) {
        source_snapshot = source->pixels;
        source_data = source_snapshot.data();
    } else if (!source->pixels.empty()) {
        std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
    }
    if (source->width < kernel_width || source->height < kernel_height || source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int radius_x = kernel_width / 2;
    const int radius_y = kernel_height / 2;
    for (int y = radius_y; y < source->height - radius_y; ++y) {
        for (int x = radius_x; x < source->width - radius_x; ++x) {
            const std::size_t dst_offset =
                static_cast<std::size_t>(y) * target->stride +
                static_cast<std::size_t>(x) * target->channels;
            for (int channel = 0; channel < source->channels; ++channel) {
                double sum = 0.0;
                for (int ky = 0; ky < kernel_height; ++ky) {
                    const int src_y = y + ky - radius_y;
                    const int kernel_y = kernel_height - 1 - ky;
                    const std::size_t src_row = static_cast<std::size_t>(src_y) * source->stride;
                    const std::size_t kernel_row = static_cast<std::size_t>(kernel_y) * kernel_width;
                    for (int kx = 0; kx < kernel_width; ++kx) {
                        const int src_x = x + kx - radius_x;
                        const std::size_t src_offset =
                            src_row +
                            static_cast<std::size_t>(src_x) * source->channels +
                            static_cast<std::size_t>(channel);
                        sum += static_cast<double>(source_data[src_offset]) * kernel[kernel_row + kx];
                    }
                }

                const double filtered = scale == 0.0 ? 0.0 : (sum / scale) + offset;
                target->pixels[dst_offset + static_cast<std::size_t>(channel)] = round_half_up_clip_u8(filtered);
            }
        }
    }

    return PILLOW_C_OK;
}

bool valid_rank_filter_arguments(int size, int rank)
{
    if (size <= 0 || (size % 2) == 0) {
        return false;
    }
    const std::int64_t count = static_cast<std::int64_t>(size) * size;
    return count <= INT_MAX && rank >= 0 && static_cast<std::int64_t>(rank) < count;
}

int filter_rank_image_into(const PillowCImage* source, int size, int rank, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (!valid_rank_filter_arguments(size, rank)) {
        return PILLOW_C_INVALID_ARGUMENT;
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

    const int radius = size / 2;
    const std::size_t window_size = static_cast<std::size_t>(size) * size;
    std::vector<std::uint8_t> window(window_size);
    for (int y = 0; y < source->height; ++y) {
        for (int x = 0; x < source->width; ++x) {
            const std::size_t dst_offset =
                static_cast<std::size_t>(y) * target->stride +
                static_cast<std::size_t>(x) * target->channels;
            for (int channel = 0; channel < source->channels; ++channel) {
                std::size_t window_index = 0;
                for (int ky = 0; ky < size; ++ky) {
                    const int src_y = clamp_int(y + ky - radius, 0, source->height - 1);
                    const std::size_t src_row = static_cast<std::size_t>(src_y) * source->stride;
                    for (int kx = 0; kx < size; ++kx) {
                        const int src_x = clamp_int(x + kx - radius, 0, source->width - 1);
                        const std::size_t src_offset =
                            src_row +
                            static_cast<std::size_t>(src_x) * source->channels +
                            static_cast<std::size_t>(channel);
                        window[window_index++] = source_data[src_offset];
                    }
                }
                auto rank_iter = window.begin() + rank;
                std::nth_element(window.begin(), rank_iter, window.end());
                target->pixels[dst_offset + static_cast<std::size_t>(channel)] = *rank_iter;
            }
        }
    }

    return PILLOW_C_OK;
}

int filter_mode_image_into(const PillowCImage* source, int size, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
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

    const int radius = size / 2;
    for (int y = 0; y < source->height; ++y) {
        for (int x = 0; x < source->width; ++x) {
            const std::size_t dst_offset =
                static_cast<std::size_t>(y) * target->stride +
                static_cast<std::size_t>(x) * target->channels;
            for (int channel = 0; channel < source->channels; ++channel) {
                int histogram[256];
                std::fill(histogram, histogram + 256, 0);

                if (radius >= 0) {
                    const int y0 = clamp_int(y - radius, 0, source->height - 1);
                    const int y1 = clamp_int(y + radius, 0, source->height - 1);
                    const int x0 = clamp_int(x - radius, 0, source->width - 1);
                    const int x1 = clamp_int(x + radius, 0, source->width - 1);
                    for (int yy = y0; yy <= y1; ++yy) {
                        const std::size_t src_row = static_cast<std::size_t>(yy) * source->stride;
                        for (int xx = x0; xx <= x1; ++xx) {
                            const std::size_t src_offset =
                                src_row +
                                static_cast<std::size_t>(xx) * source->channels +
                                static_cast<std::size_t>(channel);
                            ++histogram[source_data[src_offset]];
                        }
                    }
                }

                int max_pixel = 0;
                int max_count = histogram[0];
                for (int value = 1; value < 256; ++value) {
                    if (histogram[value] > max_count) {
                        max_count = histogram[value];
                        max_pixel = value;
                    }
                }

                const std::size_t original_offset =
                    static_cast<std::size_t>(y) * source->stride +
                    static_cast<std::size_t>(x) * source->channels +
                    static_cast<std::size_t>(channel);
                target->pixels[dst_offset + static_cast<std::size_t>(channel)] =
                    max_count > 2 ? static_cast<std::uint8_t>(max_pixel) : source_data[original_offset];
            }
        }
    }

    return PILLOW_C_OK;
}

int resize_image_box_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    double box_left,
    double box_top,
    double box_right,
    double box_bottom,
    PillowCImage* target)
{
    const bool supported =
        resample == PILLOW_C_RESAMPLE_NEAREST ||
        filter_spec_for_resample(resample) != nullptr;
    if (!supported) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!valid_resize_box(source, box_left, box_top, box_right, box_bottom)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source && target &&
        out_width == source->width &&
        out_height == source->height &&
        box_left == 0.0 &&
        box_top == 0.0 &&
        box_right == source->width &&
        box_bottom == source->height &&
        image_shape_matches(target, source)) {
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        return PILLOW_C_OK;
    }

    switch (resample) {
    case PILLOW_C_RESAMPLE_NEAREST:
        return resize_nearest_box_into(source, out_width, out_height, box_left, box_top, box_right, box_bottom, target);
    case PILLOW_C_RESAMPLE_BOX:
    case PILLOW_C_RESAMPLE_HAMMING:
    case PILLOW_C_RESAMPLE_BILINEAR:
    case PILLOW_C_RESAMPLE_BICUBIC:
    case PILLOW_C_RESAMPLE_LANCZOS:
        return resize_filter_box_into(source, out_width, out_height, resample, box_left, box_top, box_right, box_bottom, target);
    default:
        return PILLOW_C_INVALID_ARGUMENT;
    }
}

int python_round_to_int(double value)
{
    const double floor_value = std::floor(value);
    const double fraction = value - floor_value;
    if (fraction < 0.5) {
        return static_cast<int>(floor_value);
    }
    if (fraction > 0.5) {
        return static_cast<int>(floor_value + 1.0);
    }
    const auto floor_int = static_cast<std::int64_t>(floor_value);
    return static_cast<int>((floor_int % 2 == 0) ? floor_int : floor_int + 1);
}

int proportional_resize_size(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    bool cover,
    int* out_width,
    int* out_height)
{
    if (!source || !out_width || !out_height) {
        return PILLOW_C_NULL_POINTER;
    }
    if (requested_height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int width = requested_width;
    int height = requested_height;
    const double image_ratio = static_cast<double>(source->width) / source->height;
    const double destination_ratio = static_cast<double>(requested_width) / requested_height;

    if (image_ratio != destination_ratio) {
        if ((!cover && image_ratio > destination_ratio) || (cover && image_ratio < destination_ratio)) {
            const int new_height = python_round_to_int(
                static_cast<double>(source->height) / source->width * requested_width);
            if (new_height != requested_height) {
                height = new_height;
            }
        } else {
            const int new_width = python_round_to_int(
                static_cast<double>(source->width) / source->height * requested_height);
            if (new_width != requested_width) {
                width = new_width;
            }
        }
    }

    if (width <= 0 || height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_width = width;
    *out_height = height;
    return PILLOW_C_OK;
}

int proportional_resize_into(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    int resample,
    bool cover,
    PillowCImage* target)
{
    int out_width = 0;
    int out_height = 0;
    const int status = proportional_resize_size(source, requested_width, requested_height, cover, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return resize_image_into(source, out_width, out_height, resample, target);
}

double clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

double fit_centering_value(double value)
{
    return (value >= 0.0 && value <= 1.0) ? value : 0.5;
}

double fit_bleed_value(double value)
{
    return (value >= 0.0 && value < 0.5) ? value : 0.0;
}

int fit_image_into(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    int resample,
    double bleed,
    double center_x,
    double center_y,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (requested_width <= 0 || requested_height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!image_shape_matches(target, requested_width, requested_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    bleed = fit_bleed_value(bleed);
    center_x = fit_centering_value(center_x);
    center_y = fit_centering_value(center_y);

    const double bleed_x = bleed * source->width;
    const double bleed_y = bleed * source->height;
    const double live_width = source->width - bleed_x * 2.0;
    const double live_height = source->height - bleed_y * 2.0;
    if (!(live_width > 0.0) || !(live_height > 0.0)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const double live_ratio = live_width / live_height;
    const double output_ratio = static_cast<double>(requested_width) / requested_height;

    double crop_width = live_width;
    double crop_height = live_height;
    if (live_ratio == output_ratio) {
        crop_width = live_width;
        crop_height = live_height;
    } else if (live_ratio >= output_ratio) {
        crop_width = output_ratio * live_height;
        crop_height = live_height;
    } else {
        crop_width = live_width;
        crop_height = live_width / output_ratio;
    }

    const double crop_left = bleed_x + (live_width - crop_width) * center_x;
    const double crop_top = bleed_y + (live_height - crop_height) * center_y;
    return resize_image_box_into(
        source,
        requested_width,
        requested_height,
        resample,
        crop_left,
        crop_top,
        crop_left + crop_width,
        crop_top + crop_height,
        target);
}

int pad_image_into(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    int resample,
    const std::uint8_t* color,
    std::size_t color_size,
    double center_x,
    double center_y,
    PillowCImage* target)
{
    if (!source || !color || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(source->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }

    int resized_width = 0;
    int resized_height = 0;
    int status = proportional_resize_size(source, requested_width, requested_height, false, &resized_width, &resized_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!image_shape_matches(target, requested_width, requested_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    if (resized_width == requested_width && resized_height == requested_height) {
        return resize_image_into(source, requested_width, requested_height, resample, target);
    }

    status = fill_image_pixels(target, color, color_size);
    if (status != PILLOW_C_OK) {
        return status;
    }

    std::size_t resized_stride = 0;
    std::size_t resized_size = 0;
    if (!checked_image_size(resized_width, resized_height, source->channels, &resized_stride, &resized_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        PillowCImage resized{
            resized_width,
            resized_height,
            source->mode,
            source->channels,
            resized_stride,
            std::vector<std::uint8_t>(resized_size)};
        status = resize_image_into(source, resized_width, resized_height, resample, &resized);
        if (status != PILLOW_C_OK) {
            return status;
        }

        int left = 0;
        int top = 0;
        if (resized_width != requested_width) {
            left = python_round_to_int((requested_width - resized_width) * clamp_unit(center_x));
        } else if (resized_height != requested_height) {
            top = python_round_to_int((requested_height - resized_height) * clamp_unit(center_y));
        }
        return paste_image_pixels_into(target, &resized, left, top);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
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

extern "C" __declspec(dllexport) int pillow_c_abi_version(
    int* out_major,
    int* out_minor,
    int* out_patch)
{
    if (!out_major || !out_minor || !out_patch) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_major = 0;
    *out_minor = 1;
    *out_patch = 0;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_status_message(
    int status,
    char* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!out_required) {
        return PILLOW_C_NULL_POINTER;
    }

    const char* message = status_message(status);
    const std::size_t required = std::strlen(message) + 1;
    *out_required = required;
    if (!out) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out, message, required);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_mode_from_string(
    const char* mode_name_text,
    int* out_mode)
{
    if (!mode_name_text || !out_mode) {
        return PILLOW_C_NULL_POINTER;
    }
    if (std::strcmp(mode_name_text, "L") == 0) {
        *out_mode = PILLOW_C_MODE_L;
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
        out[i] = static_cast<std::uint8_t>(
            (static_cast<std::int32_t>(px[0]) * 19595 +
             static_cast<std::int32_t>(px[1]) * 38470 +
             static_cast<std::int32_t>(px[2]) * 7471 +
             0x8000) >> 16);
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

    constexpr std::uint32_t precision_bits = 7;
    constexpr std::uint32_t precision = 1u << precision_bits;

    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint8_t* d = dst + i * 4;
        const std::uint8_t* s = src + i * 4;
        std::uint8_t* o = out + i * 4;

        if (s[3] == 0) {
            o[0] = d[0];
            o[1] = d[1];
            o[2] = d[2];
            o[3] = d[3];
            continue;
        }

        const std::uint32_t blend = static_cast<std::uint32_t>(d[3]) * (255u - s[3]);
        const std::uint32_t outa255 = static_cast<std::uint32_t>(s[3]) * 255u + blend;
        const std::uint32_t coef1 =
            static_cast<std::uint32_t>(s[3]) * 255u * 255u * precision / outa255;
        const std::uint32_t coef2 = 255u * precision - coef1;

        const std::uint32_t tmpr = static_cast<std::uint32_t>(s[0]) * coef1 +
                                   static_cast<std::uint32_t>(d[0]) * coef2;
        const std::uint32_t tmpg = static_cast<std::uint32_t>(s[1]) * coef1 +
                                   static_cast<std::uint32_t>(d[1]) * coef2;
        const std::uint32_t tmpb = static_cast<std::uint32_t>(s[2]) * coef1 +
                                   static_cast<std::uint32_t>(d[2]) * coef2;

        o[0] = static_cast<std::uint8_t>(
            shift_for_div255(tmpr + (0x80u << precision_bits)) >> precision_bits);
        o[1] = static_cast<std::uint8_t>(
            shift_for_div255(tmpg + (0x80u << precision_bits)) >> precision_bits);
        o[2] = static_cast<std::uint8_t>(
            shift_for_div255(tmpb + (0x80u << precision_bits)) >> precision_bits);
        o[3] = static_cast<std::uint8_t>(shift_for_div255(outa255 + 0x80u));
    }

    return PILLOW_C_OK;
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
    return paste_image_pixels_into(target, source, left, top);
}

extern "C" __declspec(dllexport) int pillow_c_image_copy_into(
    const PillowCImage* source,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!image_shape_matches(source, target)) {
        return PILLOW_C_MISMATCH;
    }
    if (!source->pixels.empty()) {
        std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
    }
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
    if (!images_match(left, right) || !image_shape_matches(target, left)) {
        return PILLOW_C_MISMATCH;
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
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_L, 1)) {
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

extern "C" __declspec(dllexport) int pillow_c_image_autocontrast_into(
    const PillowCImage* source,
    double low_cutoff,
    double high_cutoff,
    const std::uint8_t* ignore_values,
    std::size_t ignore_count,
    const PillowCImage* mask,
    int preserve_tone,
    PillowCImage* target)
{
    return autocontrast_image_into(source, low_cutoff, high_cutoff, ignore_values, ignore_count, mask, preserve_tone != 0, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_get_channel_into(
    const PillowCImage* source,
    int channel_index,
    PillowCImage* target)
{
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
    return convert_image_mode_into(source, target_mode, target);
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

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(source->width, source->height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        for (std::size_t channel = 0; channel < out_count; ++channel) {
            auto* image = new PillowCImage{
                source->width,
                source->height,
                PILLOW_C_MODE_L,
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
    if (!images_match(dst, src) || dst->channels != 4 || !image_shape_matches(target, dst)) {
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

extern "C" __declspec(dllexport) int pillow_c_image_crop_into(
    const PillowCImage* source,
    int left,
    int top,
    int right,
    int bottom,
    PillowCImage* target)
{
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

extern "C" __declspec(dllexport) int pillow_c_image_resize_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    PillowCImage* target)
{
    return resize_image_into(source, out_width, out_height, resample, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_filter_kernel_into(
    const PillowCImage* source,
    int kernel_width,
    int kernel_height,
    const double* kernel,
    std::size_t kernel_count,
    double scale,
    double offset,
    PillowCImage* target)
{
    return filter_kernel_image_into(source, kernel_width, kernel_height, kernel, kernel_count, scale, offset, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_filter_rank_into(
    const PillowCImage* source,
    int size,
    int rank,
    PillowCImage* target)
{
    return filter_rank_image_into(source, size, rank, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_filter_mode_into(
    const PillowCImage* source,
    int size,
    PillowCImage* target)
{
    return filter_mode_image_into(source, size, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_transform_affine_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const double* matrix,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    return affine_transform_image_into(
        source,
        out_width,
        out_height,
        matrix,
        resample,
        fill_color,
        fill_color_size,
        target);
}

extern "C" __declspec(dllexport) int pillow_c_image_transform_perspective_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const double* coefficients,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    return perspective_transform_image_into(
        source,
        out_width,
        out_height,
        coefficients,
        resample,
        fill_color,
        fill_color_size,
        target);
}

extern "C" __declspec(dllexport) int pillow_c_image_transform_quad_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const double* corners,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    return quad_transform_image_into(
        source,
        out_width,
        out_height,
        corners,
        resample,
        fill_color,
        fill_color_size,
        target);
}

extern "C" __declspec(dllexport) int pillow_c_image_transform_mesh_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const int* boxes,
    const double* quads,
    std::size_t mesh_count,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    return mesh_transform_image_into(
        source,
        out_width,
        out_height,
        boxes,
        quads,
        mesh_count,
        resample,
        fill_color,
        fill_color_size,
        target);
}

extern "C" __declspec(dllexport) int pillow_c_image_rotate_into(
    const PillowCImage* source,
    double angle,
    int resample,
    int expand,
    double center_x,
    double center_y,
    int has_center,
    double translate_x,
    double translate_y,
    int has_translate,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage* target)
{
    return rotate_image_into(
        source,
        angle,
        resample,
        expand != 0,
        center_x,
        center_y,
        has_center != 0,
        translate_x,
        translate_y,
        has_translate != 0,
        fill_color,
        fill_color_size,
        target);
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

        for (int dst_y = 0; dst_y < out_height; ++dst_y) {
            for (int dst_x = 0; dst_x < out_width; ++dst_x) {
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
                    static_cast<std::size_t>(dst_y) * image->stride +
                    static_cast<std::size_t>(dst_x) * image->channels;
                std::memcpy(
                    image->pixels.data() + dst_offset,
                    source->pixels.data() + src_offset,
                    static_cast<std::size_t>(source->channels));
            }
        }

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

extern "C" __declspec(dllexport) int pillow_c_image_width(const PillowCImage* image, int* out_width)
{
    if (!image || !out_width) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_width = image->width;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_height(const PillowCImage* image, int* out_height)
{
    if (!image || !out_height) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_height = image->height;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_channels(const PillowCImage* image, int* out_channels)
{
    if (!image || !out_channels) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_channels = image->channels;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_mode(const PillowCImage* image, int* out_mode)
{
    if (!image || !out_mode) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_mode = image->mode;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_stride(const PillowCImage* image, int* out_stride)
{
    if (!image || !out_stride) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->stride > static_cast<std::size_t>(INT32_MAX)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    *out_stride = static_cast<int>(image->stride);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_size(const PillowCImage* image, std::size_t* out_size)
{
    if (!image || !out_size) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_size = image->pixels.size();
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_data(
    PillowCImage* image,
    std::uint8_t** out_data,
    std::size_t* out_size)
{
    if (!image || !out_data || !out_size) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_data = image->pixels.data();
    *out_size = image->pixels.size();
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_set_bytes(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size)
{
    if (!image || !data) {
        return PILLOW_C_NULL_POINTER;
    }
    if (size != image->pixels.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(image->pixels.data(), data, size);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_fill(
    PillowCImage* image,
    const std::uint8_t* color,
    std::size_t color_size)
{
    return fill_image_pixels(image, color, color_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_getpixel(
    const PillowCImage* image,
    int x,
    int y,
    std::uint8_t* out_color,
    std::size_t out_color_size)
{
    return get_pixel_image(image, x, y, out_color, out_color_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_putpixel(
    PillowCImage* image,
    int x,
    int y,
    const std::uint8_t* color,
    std::size_t color_size)
{
    return put_pixel_image(image, x, y, color, color_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_get_bytes(
    const PillowCImage* image,
    std::uint8_t* out,
    std::size_t size)
{
    if (!image || !out) {
        return PILLOW_C_NULL_POINTER;
    }
    if (size != image->pixels.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out, image->pixels.data(), size);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_histogram(
    const PillowCImage* image,
    std::uint64_t* out_histogram,
    std::size_t out_count)
{
    return histogram_image(image, out_histogram, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_get_extrema(
    const PillowCImage* image,
    std::uint8_t* out_min,
    std::uint8_t* out_max,
    std::uint8_t* out_has_value,
    std::size_t out_count)
{
    return extrema_image(image, out_min, out_max, out_has_value, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_getbbox(
    const PillowCImage* image,
    int alpha_only,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom,
    int* out_has_bbox)
{
    return getbbox_image(image, alpha_only != 0, out_left, out_top, out_right, out_bottom, out_has_bbox);
}

extern "C" __declspec(dllexport) int pillow_c_image_getprojection(
    const PillowCImage* image,
    std::uint8_t* out_x_projection,
    std::size_t out_x_count,
    std::uint8_t* out_y_projection,
    std::size_t out_y_count)
{
    return getprojection_image(image, out_x_projection, out_x_count, out_y_projection, out_y_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_getcolors(
    const PillowCImage* image,
    int maxcolors,
    std::uint64_t* out_counts,
    std::uint8_t* out_colors,
    std::size_t out_capacity,
    std::size_t* out_count,
    int* out_exceeded)
{
    return getcolors_image(image, maxcolors, out_counts, out_colors, out_capacity, out_count, out_exceeded);
}

extern "C" __declspec(dllexport) int pillow_c_image_copy(
    const PillowCImage* source,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    try {
        auto* image = new PillowCImage{source->width, source->height, source->mode, source->channels, source->stride, source->pixels};
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
    if (!images_match(left, right)) {
        return PILLOW_C_MISMATCH;
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
    if (!supported_composite_mask(mask)) {
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

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_L,
            1,
            static_cast<std::size_t>(source->width),
            std::vector<std::uint8_t>(static_cast<std::size_t>(source->width) * source->height)};
        const int status = pillow_c_rgb_to_l(
            source->pixels.data(),
            image->pixels.data(),
            static_cast<std::size_t>(source->width) * source->height);
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
    if (!source || !lut || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (lut_size != static_cast<std::size_t>(source->channels) * 256u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    try {
        auto* image = new PillowCImage{source->width, source->height, source->mode, source->channels, source->stride, std::vector<std::uint8_t>(source->pixels.size())};
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

extern "C" __declspec(dllexport) int pillow_c_image_invert(
    const PillowCImage* source,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supports_imageops_lut(source)) {
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
    if (!supports_imageops_lut(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{source->width, source->height, source->mode, source->channels, source->stride, std::vector<std::uint8_t>(source->pixels.size())};
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

extern "C" __declspec(dllexport) int pillow_c_image_autocontrast(
    const PillowCImage* source,
    double low_cutoff,
    double high_cutoff,
    const std::uint8_t* ignore_values,
    std::size_t ignore_count,
    const PillowCImage* mask,
    int preserve_tone,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if ((preserve_tone == 0 && !autocontrast_supported_mode(source)) ||
        (preserve_tone != 0 && !autocontrast_preserve_tone_supported_mode(source))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{source->width, source->height, source->mode, source->channels, source->stride, std::vector<std::uint8_t>(source->pixels.size())};
        const int status = autocontrast_image_into(source, low_cutoff, high_cutoff, ignore_values, ignore_count, mask, preserve_tone != 0, image);
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

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_L,
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
    if (!supports_rgba_alpha_target(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_RGBA,
            4,
            static_cast<std::size_t>(source->width) * 4u,
            std::vector<std::uint8_t>(static_cast<std::size_t>(source->width) * source->height * 4u)};
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
    if (!supports_rgba_alpha_target(source) || alpha->mode != PILLOW_C_MODE_L || alpha->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (alpha->width != source->width || alpha->height != source->height) {
        return PILLOW_C_MISMATCH;
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            PILLOW_C_MODE_RGBA,
            4,
            static_cast<std::size_t>(source->width) * 4u,
            std::vector<std::uint8_t>(static_cast<std::size_t>(source->width) * source->height * 4u)};
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

extern "C" __declspec(dllexport) int pillow_c_image_resize(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (out_width <= 0 || out_height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (resample != PILLOW_C_RESAMPLE_NEAREST && !filter_spec_for_resample(resample)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(out_width, out_height, source->channels, &stride, &size)) {
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
        const int status = resize_image_into(source, out_width, out_height, resample, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_filter_kernel(
    const PillowCImage* source,
    int kernel_width,
    int kernel_height,
    const double* kernel,
    std::size_t kernel_count,
    double scale,
    double offset,
    PillowCImage** out_image)
{
    if (!source || !kernel || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supported_kernel_size(kernel_width, kernel_height) || !std::isfinite(scale) || !std::isfinite(offset)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t expected_count = static_cast<std::size_t>(kernel_width) * kernel_height;
    if (kernel_count != expected_count) {
        return PILLOW_C_INVALID_LENGTH;
    }
    for (std::size_t index = 0; index < kernel_count; ++index) {
        if (!std::isfinite(kernel[index])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }

    try {
        auto* image = new PillowCImage{
            source->width,
            source->height,
            source->mode,
            source->channels,
            source->stride,
            std::vector<std::uint8_t>(source->pixels.size())};
        const int status = filter_kernel_image_into(
            source,
            kernel_width,
            kernel_height,
            kernel,
            kernel_count,
            scale,
            offset,
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

extern "C" __declspec(dllexport) int pillow_c_image_filter_rank(
    const PillowCImage* source,
    int size,
    int rank,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!valid_rank_filter_arguments(size, rank)) {
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
        const int status = filter_rank_image_into(source, size, rank, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_filter_mode(
    const PillowCImage* source,
    int size,
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
        const int status = filter_mode_image_into(source, size, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_transform_affine(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const double* matrix,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage** out_image)
{
    if (!source || !matrix || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supported_affine_transform_resample(resample) || out_width < 0 || out_height < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (int i = 0; i < 6; ++i) {
        if (!std::isfinite(matrix[i])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
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
        const int status = affine_transform_image_into(
            source,
            out_width,
            out_height,
            matrix,
            resample,
            fill_color,
            fill_color_size,
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

extern "C" __declspec(dllexport) int pillow_c_image_transform_perspective(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const double* coefficients,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage** out_image)
{
    if (!source || !coefficients || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supported_affine_transform_resample(resample) || out_width < 0 || out_height < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (int i = 0; i < 8; ++i) {
        if (!std::isfinite(coefficients[i])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
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
        const int status = perspective_transform_image_into(
            source,
            out_width,
            out_height,
            coefficients,
            resample,
            fill_color,
            fill_color_size,
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

extern "C" __declspec(dllexport) int pillow_c_image_transform_quad(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const double* corners,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage** out_image)
{
    if (!source || !corners || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supported_affine_transform_resample(resample) || out_width <= 0 || out_height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (int i = 0; i < 8; ++i) {
        if (!std::isfinite(corners[i])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
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
        const int status = quad_transform_image_into(
            source,
            out_width,
            out_height,
            corners,
            resample,
            fill_color,
            fill_color_size,
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

extern "C" __declspec(dllexport) int pillow_c_image_transform_mesh(
    const PillowCImage* source,
    int out_width,
    int out_height,
    const int* boxes,
    const double* quads,
    std::size_t mesh_count,
    int resample,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage** out_image)
{
    if (!source || !out_image || (mesh_count > 0 && (!boxes || !quads))) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supported_affine_transform_resample(resample) || out_width < 0 || out_height < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (mesh_count > static_cast<std::size_t>(-1) / 8 || mesh_count > static_cast<std::size_t>(-1) / 4) {
        return PILLOW_C_INVALID_ARGUMENT;
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
        const int status = mesh_transform_image_into(
            source,
            out_width,
            out_height,
            boxes,
            quads,
            mesh_count,
            resample,
            fill_color,
            fill_color_size,
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

extern "C" __declspec(dllexport) int pillow_c_image_rotate(
    const PillowCImage* source,
    double angle,
    int resample,
    int expand,
    double center_x,
    double center_y,
    int has_center,
    double translate_x,
    double translate_y,
    int has_translate,
    const std::uint8_t* fill_color,
    std::size_t fill_color_size,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (resample != PILLOW_C_RESAMPLE_NEAREST &&
        resample != PILLOW_C_RESAMPLE_BILINEAR &&
        resample != PILLOW_C_RESAMPLE_BICUBIC) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int out_width = 0;
    int out_height = 0;
    int status = rotate_output_shape(
        source,
        angle,
        expand != 0,
        center_x,
        center_y,
        has_center != 0,
        translate_x,
        translate_y,
        has_translate != 0,
        &out_width,
        &out_height);
    if (status != PILLOW_C_OK) {
        return status;
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
        status = rotate_image_into(
            source,
            angle,
            resample,
            expand != 0,
            center_x,
            center_y,
            has_center != 0,
            translate_x,
            translate_y,
            has_translate != 0,
            fill_color,
            fill_color_size,
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

int proportional_resize_allocating(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    int resample,
    bool cover,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    int out_width = 0;
    int out_height = 0;
    int status = proportional_resize_size(source, requested_width, requested_height, cover, &out_width, &out_height);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (resample != PILLOW_C_RESAMPLE_NEAREST && !filter_spec_for_resample(resample)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(out_width, out_height, source->channels, &stride, &size)) {
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
        status = resize_image_into(source, out_width, out_height, resample, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_fit(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    int resample,
    double bleed,
    double center_x,
    double center_y,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (requested_width <= 0 || requested_height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (resample != PILLOW_C_RESAMPLE_NEAREST && !filter_spec_for_resample(resample)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(requested_width, requested_height, source->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            requested_width,
            requested_height,
            source->mode,
            source->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = fit_image_into(
            source,
            requested_width,
            requested_height,
            resample,
            bleed,
            center_x,
            center_y,
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

extern "C" __declspec(dllexport) int pillow_c_image_pad(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    int resample,
    const std::uint8_t* color,
    std::size_t color_size,
    double center_x,
    double center_y,
    PillowCImage** out_image)
{
    if (!source || !color || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (color_size != static_cast<std::size_t>(source->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (resample != PILLOW_C_RESAMPLE_NEAREST && !filter_spec_for_resample(resample)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int resized_width = 0;
    int resized_height = 0;
    int status = proportional_resize_size(source, requested_width, requested_height, false, &resized_width, &resized_height);
    if (status != PILLOW_C_OK) {
        return status;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(requested_width, requested_height, source->channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            requested_width,
            requested_height,
            source->mode,
            source->channels,
            stride,
            std::vector<std::uint8_t>(size)};
        status = pad_image_into(
            source,
            requested_width,
            requested_height,
            resample,
            color,
            color_size,
            center_x,
            center_y,
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

extern "C" __declspec(dllexport) int pillow_c_image_contain(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    int resample,
    PillowCImage** out_image)
{
    return proportional_resize_allocating(source, requested_width, requested_height, resample, false, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_cover(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    int resample,
    PillowCImage** out_image)
{
    return proportional_resize_allocating(source, requested_width, requested_height, resample, true, out_image);
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
    if (!images_match(dst, src) || dst->channels != 4) {
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
