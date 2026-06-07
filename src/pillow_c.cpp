#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <climits>
#include <cstring>
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

struct PillowCImage {
    int width;
    int height;
    int mode;
    int channels;
    std::size_t stride;
    std::vector<std::uint8_t> pixels;
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

extern "C" __declspec(dllexport) int pillow_c_image_paste(
    PillowCImage* target,
    const PillowCImage* source,
    int left,
    int top)
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
