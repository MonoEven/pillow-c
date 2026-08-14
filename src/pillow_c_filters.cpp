#include "pillow_c_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace {
constexpr int PILLOW_C_RESAMPLE_NEAREST = 0;
constexpr int PILLOW_C_RESAMPLE_LANCZOS = 1;
constexpr int PILLOW_C_RESAMPLE_BILINEAR = 2;
constexpr int PILLOW_C_RESAMPLE_BICUBIC = 3;
constexpr int PILLOW_C_RESAMPLE_BOX = 4;
constexpr int PILLOW_C_RESAMPLE_HAMMING = 5;

constexpr int RESAMPLE_PRECISION_BITS = 32 - 8 - 2;
constexpr int RESAMPLE_PRECISION_SCALE = 1 << RESAMPLE_PRECISION_BITS;
constexpr int RESAMPLE_ROUNDING_BIAS = 1 << (RESAMPLE_PRECISION_BITS - 1);

constexpr int COLOR_LUT_PRECISION_BITS = 16 - 8 - 2;
constexpr int COLOR_LUT_PRECISION_ROUNDING = 1 << (COLOR_LUT_PRECISION_BITS - 1);
constexpr int COLOR_LUT_SCALE_BITS = 32 - 8 - 6;
constexpr int COLOR_LUT_SCALE_MASK = (1 << COLOR_LUT_SCALE_BITS) - 1;
constexpr int COLOR_LUT_SHIFT_BITS = 16 - 1;

struct ResampleCoefficients {
    int kernel_size;
    std::vector<int> bounds;
    std::vector<std::int32_t> weights;
    std::vector<double> normalized_weights;
};

struct ResampleFilterSpec {
    double support;
    double (*filter)(double);
};

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
    coeffs->normalized_weights.assign(static_cast<std::size_t>(out_size) * kernel_size, 0.0);

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
        double* normalized_weights =
            coeffs->normalized_weights.data() +
            static_cast<std::size_t>(out_index) * kernel_size;
        for (int i = 0; i < kernel_size; ++i) {
            const double value = normalized[static_cast<std::size_t>(i)];
            normalized_weights[i] = value;
            const double scaled = value * RESAMPLE_PRECISION_SCALE;
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
    const bool alpha_mode = source->mode == PILLOW_C_MODE_LA || source->mode == PILLOW_C_MODE_RGBA;
    const int alpha_channel = source->channels - 1;
    if (alpha_mode && channel < alpha_channel) {
        // Pillow's RGBa/La premultiply: (c * a + 127) // 255.
        return static_cast<std::uint8_t>((px[channel] * px[alpha_channel] + 127) / 255);
    }
    return px[channel];
}

double resize_numeric_sample(const PillowCImage* source, int x, int y)
{
    const std::uint8_t* px =
        source->pixels.data() +
        static_cast<std::size_t>(y) * source->stride +
        static_cast<std::size_t>(x) * 4u;
    return source->mode == PILLOW_C_MODE_I
        ? static_cast<double>(read_le_i32(px))
        : static_cast<double>(pillow_c_read_f32_le(px));
}

std::int32_t resize_round_i32_sample(double value)
{
    return static_cast<std::int32_t>(value >= 0.0 ? value + 0.5 : value - 0.5);
}

std::uint16_t resize_read_i16_sample(const PillowCImage* source, int x, int y)
{
    const std::uint8_t* px =
        source->pixels.data() +
        static_cast<std::size_t>(y) * source->stride +
        static_cast<std::size_t>(x) * 2u;
    return static_cast<std::uint16_t>(px[0]) |
           (static_cast<std::uint16_t>(px[1]) << 8);
}

std::uint16_t resize_round_clip_i16_sample(double value)
{
    // Pillow 11.3.0 Resample.c 16bpc writes each byte through CLIP8
    // (CLIP8(ss_int % 256), CLIP8(ss_int >> 8)), so values above 65535
    // wrap the high byte to 255 instead of clamping the whole sample.
    const std::int32_t rounded =
        static_cast<std::int32_t>(value >= 0.0 ? value + 0.5 : value - 0.5);
    const std::int32_t low = rounded % 256;
    const std::int32_t high = rounded >> 8;
    const auto clip8 = [](std::int32_t v) -> std::uint8_t {
        if (v < 0) {
            return 0;
        }
        if (v > 255) {
            return 255;
        }
        return static_cast<std::uint8_t>(v);
    };
    return static_cast<std::uint16_t>(clip8(low)) |
           (static_cast<std::uint16_t>(clip8(high)) << 8);
}

void resize_write_i16_sample(std::uint16_t value, std::uint8_t* dst)
{
    dst[0] = static_cast<std::uint8_t>(value);
    dst[1] = static_cast<std::uint8_t>(value >> 8);
}

bool valid_resize_box(const PillowCImage* source, double left, double top, double right, double bottom)
{
    return source &&
           std::isfinite(left) &&
           std::isfinite(top) &&
           std::isfinite(right) &&
           std::isfinite(bottom) &&
           left >= 0.0 &&
           top >= 0.0 &&
           right <= static_cast<double>(source->width) &&
           bottom <= static_cast<double>(source->height) &&
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
    if (!pillow_c_image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->mode == PILLOW_C_MODE_I16B) {
        // Pillow 11.3.0's 16bpc resampler misreads I;16B raw bytes
          // (endian-bug garbage), so filter resizes on I;16B are an
          // explicit documented boundary instead of replicated garbage.
        return PILLOW_C_INVALID_ARGUMENT;
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

        if (source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F) {
            // Pillow 11.3.0 resamples 32-bit modes per sample with
              // unquantized double weights (Resample.c 32bpc paths):
              // mode F keeps float32 intermediates, mode I rounds half
              // away from zero after EACH pass.
            const bool float_mode = source->mode == PILLOW_C_MODE_F;
            std::vector<double> temp(
                static_cast<std::size_t>(out_width) *
                static_cast<std::size_t>(source->height),
                0.0);
            const double* x_norm = x_coeffs.normalized_weights.data();
            const double* y_norm = y_coeffs.normalized_weights.data();
            for (int y = 0; y < source->height; ++y) {
                for (int out_x = 0; out_x < out_width; ++out_x) {
                    const int xmin = x_coeffs.bounds[static_cast<std::size_t>(out_x) * 2u];
                    const int count = x_coeffs.bounds[static_cast<std::size_t>(out_x) * 2u + 1u];
                    const double* weights = x_norm + static_cast<std::size_t>(out_x) * x_coeffs.kernel_size;
                    double sum = 0.0;
                    for (int i = 0; i < count; ++i) {
                        sum += resize_numeric_sample(source, xmin + i, y) * weights[i];
                    }
                    double value = float_mode ? sum : static_cast<double>(resize_round_i32_sample(sum));
                    temp[static_cast<std::size_t>(y) * out_width + out_x] =
                        float_mode ? static_cast<double>(static_cast<float>(value)) : value;
                }
            }
            for (int out_y = 0; out_y < out_height; ++out_y) {
                const int ymin = y_coeffs.bounds[static_cast<std::size_t>(out_y) * 2u];
                const int count = y_coeffs.bounds[static_cast<std::size_t>(out_y) * 2u + 1u];
                const double* weights = y_norm + static_cast<std::size_t>(out_y) * y_coeffs.kernel_size;
                for (int out_x = 0; out_x < out_width; ++out_x) {
                    double sum = 0.0;
                    for (int i = 0; i < count; ++i) {
                        sum += temp[static_cast<std::size_t>(ymin + i) * out_width + out_x] * weights[i];
                    }
                    std::uint8_t* dst =
                        target->pixels.data() +
                        static_cast<std::size_t>(out_y) * target->stride +
                        static_cast<std::size_t>(out_x) * 4u;
                    if (float_mode) {
                        pillow_c_write_f32_le(dst, static_cast<float>(sum));
                    } else {
                        pillow_c_write_i32_le(
                            dst,
                            static_cast<std::uint32_t>(resize_round_i32_sample(sum)));
                    }
                }
            }
            return PILLOW_C_OK;
        }

        if (source->mode == PILLOW_C_MODE_I16 && source->channels == 2) {
            // Pillow 11.3.0 Resample.c 16bpc: one uint16 sample per
              // pixel, double weights, ROUND_UP plus 0..65535 clipping
              // after EACH pass (little-endian storage).
            std::vector<double> temp(
                static_cast<std::size_t>(out_width) *
                static_cast<std::size_t>(source->height),
                0.0);
            const double* x_norm = x_coeffs.normalized_weights.data();
            const double* y_norm = y_coeffs.normalized_weights.data();
            for (int y = 0; y < source->height; ++y) {
                for (int out_x = 0; out_x < out_width; ++out_x) {
                    const int xmin = x_coeffs.bounds[static_cast<std::size_t>(out_x) * 2u];
                    const int count = x_coeffs.bounds[static_cast<std::size_t>(out_x) * 2u + 1u];
                    const double* weights = x_norm + static_cast<std::size_t>(out_x) * x_coeffs.kernel_size;
                    double sum = 0.0;
                    for (int i = 0; i < count; ++i) {
                        sum += static_cast<double>(resize_read_i16_sample(source, xmin + i, y)) * weights[i];
                    }
                    temp[static_cast<std::size_t>(y) * out_width + out_x] =
                        static_cast<double>(resize_round_clip_i16_sample(sum));
                }
            }
            for (int out_y = 0; out_y < out_height; ++out_y) {
                const int ymin = y_coeffs.bounds[static_cast<std::size_t>(out_y) * 2u];
                const int count = y_coeffs.bounds[static_cast<std::size_t>(out_y) * 2u + 1u];
                const double* weights = y_norm + static_cast<std::size_t>(out_y) * y_coeffs.kernel_size;
                for (int out_x = 0; out_x < out_width; ++out_x) {
                    double sum = 0.0;
                    for (int i = 0; i < count; ++i) {
                        sum += temp[static_cast<std::size_t>(ymin + i) * out_width + out_x] * weights[i];
                    }
                    std::uint8_t* dst =
                        target->pixels.data() +
                        static_cast<std::size_t>(out_y) * target->stride +
                        static_cast<std::size_t>(out_x) * 2u;
                    resize_write_i16_sample(resize_round_clip_i16_sample(sum), dst);
                }
            }
            return PILLOW_C_OK;
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
                        pillow_c_clip_resample_u8(sum);
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
                    values[channel] = pillow_c_clip_resample_u8(sum);
                }

                std::uint8_t* dst =
                    target->pixels.data() +
                    static_cast<std::size_t>(out_y) * target->stride +
                    static_cast<std::size_t>(out_x) * target->channels;
                if (source->mode == PILLOW_C_MODE_LA || source->mode == PILLOW_C_MODE_RGBA) {
                    const int alpha_channel = source->channels - 1;
                    const std::uint8_t alpha = values[alpha_channel];
                    for (int channel = 0; channel < alpha_channel; ++channel) {
                        const std::uint8_t premultiplied = values[channel];
                        // Pillow's RGBa/La back-conversion: c * 255 // a,
                        // with a == 0 keeping the premultiplied value and
                        // a == 255 passing it through unchanged.
                        dst[channel] = (alpha == 0 || alpha == 255)
                            ? premultiplied
                            : pillow_c_clip_u8_int(255 * static_cast<int>(premultiplied) / alpha);
                    }
                    dst[alpha_channel] = alpha;
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
    if (!pillow_c_image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
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
        pillow_c_image_shape_matches(target, source)) {
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        pillow_c_copy_palette_if_same_mode(source, target);
        return PILLOW_C_OK;
    }

    int status = PILLOW_C_OK;
    switch (resample) {
    case PILLOW_C_RESAMPLE_NEAREST:
        status = resize_nearest_into(source, out_width, out_height, target);
        break;
    case PILLOW_C_RESAMPLE_BOX:
    case PILLOW_C_RESAMPLE_HAMMING:
    case PILLOW_C_RESAMPLE_BILINEAR:
    case PILLOW_C_RESAMPLE_BICUBIC:
    case PILLOW_C_RESAMPLE_LANCZOS:
        status = resize_filter_into(source, out_width, out_height, resample, target);
        break;
    default:
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (status == PILLOW_C_OK) {
        pillow_c_copy_palette_if_same_mode(source, target);
    }
    return status;
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
    if (!pillow_c_image_shape_matches(target, source)) {
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
    if (source->mode == PILLOW_C_MODE_F) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
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
    if (source->mode == PILLOW_C_MODE_I) {
        for (int y = radius_y; y < source->height - radius_y; ++y) {
            for (int x = radius_x; x < source->width - radius_x; ++x) {
                const std::size_t dst_offset =
                    static_cast<std::size_t>(y) * target->stride +
                    static_cast<std::size_t>(x) * 4u;
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
                            static_cast<std::size_t>(src_x) * 4u;
                        sum += static_cast<double>(pillow_c_read_i32_le(source_data + src_offset)) * kernel[kernel_row + kx];
                    }
                }

                const std::int32_t filtered = scale == 0.0
                    ? std::numeric_limits<std::int32_t>::min()
                    : pillow_c_round_half_up_clip_i32_nonnegative((sum / scale) + offset);
                pillow_c_write_i32_le(
                    target->pixels.data() + dst_offset,
                    static_cast<std::uint32_t>(filtered));
            }
        }
        return PILLOW_C_OK;
    }

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
                target->pixels[dst_offset + static_cast<std::size_t>(channel)] = pillow_c_round_half_up_clip_u8(filtered);
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
    if (!pillow_c_image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (!valid_rank_filter_arguments(size, rank)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
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
    if (source->mode == PILLOW_C_MODE_I) {
        std::vector<std::int32_t> window(window_size);
        for (int y = 0; y < source->height; ++y) {
            for (int x = 0; x < source->width; ++x) {
                const std::size_t dst_offset =
                    static_cast<std::size_t>(y) * target->stride +
                    static_cast<std::size_t>(x) * 4u;
                std::size_t window_index = 0;
                for (int ky = 0; ky < size; ++ky) {
                    const int src_y = pillow_c_clamp_int(y + ky - radius, 0, source->height - 1);
                    const std::size_t src_row = static_cast<std::size_t>(src_y) * source->stride;
                    for (int kx = 0; kx < size; ++kx) {
                        const int src_x = pillow_c_clamp_int(x + kx - radius, 0, source->width - 1);
                        const std::size_t src_offset =
                            src_row +
                            static_cast<std::size_t>(src_x) * 4u;
                        window[window_index++] = pillow_c_read_i32_le(source_data + src_offset);
                    }
                }
                auto rank_iter = window.begin() + rank;
                std::nth_element(window.begin(), rank_iter, window.end());
                pillow_c_write_i32_le(
                    target->pixels.data() + dst_offset,
                    static_cast<std::uint32_t>(*rank_iter));
            }
        }
        return PILLOW_C_OK;
    }
    if (source->mode == PILLOW_C_MODE_F) {
        std::vector<float> window(window_size);
        for (int y = 0; y < source->height; ++y) {
            for (int x = 0; x < source->width; ++x) {
                const std::size_t dst_offset =
                    static_cast<std::size_t>(y) * target->stride +
                    static_cast<std::size_t>(x) * 4u;
                std::size_t window_index = 0;
                for (int ky = 0; ky < size; ++ky) {
                    const int src_y = pillow_c_clamp_int(y + ky - radius, 0, source->height - 1);
                    const std::size_t src_row = static_cast<std::size_t>(src_y) * source->stride;
                    for (int kx = 0; kx < size; ++kx) {
                        const int src_x = pillow_c_clamp_int(x + kx - radius, 0, source->width - 1);
                        const std::size_t src_offset =
                            src_row +
                            static_cast<std::size_t>(src_x) * 4u;
                        window[window_index++] = pillow_c_read_f32_le(source_data + src_offset);
                    }
                }
                auto rank_iter = window.begin() + rank;
                std::nth_element(window.begin(), rank_iter, window.end());
                pillow_c_write_f32_le(target->pixels.data() + dst_offset, *rank_iter);
            }
        }
        return PILLOW_C_OK;
    }

    std::vector<std::uint8_t> window(window_size);
    for (int y = 0; y < source->height; ++y) {
        for (int x = 0; x < source->width; ++x) {
            const std::size_t dst_offset =
                static_cast<std::size_t>(y) * target->stride +
                static_cast<std::size_t>(x) * target->channels;
            for (int channel = 0; channel < source->channels; ++channel) {
                std::size_t window_index = 0;
                for (int ky = 0; ky < size; ++ky) {
                    const int src_y = pillow_c_clamp_int(y + ky - radius, 0, source->height - 1);
                    const std::size_t src_row = static_cast<std::size_t>(src_y) * source->stride;
                    for (int kx = 0; kx < size; ++kx) {
                        const int src_x = pillow_c_clamp_int(x + kx - radius, 0, source->width - 1);
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
                    const int y0 = pillow_c_clamp_int(y - radius, 0, source->height - 1);
                    const int y1 = pillow_c_clamp_int(y + radius, 0, source->height - 1);
                    const int x0 = pillow_c_clamp_int(x - radius, 0, source->width - 1);
                    const int x1 = pillow_c_clamp_int(x + radius, 0, source->width - 1);
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

bool valid_box_blur_radius(double radius)
{
    return std::isfinite(radius) &&
           radius >= 0.0 &&
           radius <= static_cast<double>((INT_MAX - 1) / 2);
}

double gaussian_blur_radius(double radius, int passes)
{
    const float float_radius = static_cast<float>(radius);
    const float sigma2 = float_radius * float_radius / static_cast<float>(passes);
    const float length = std::sqrt(12.0f * sigma2 + 1.0f);
    const float floor_radius = std::floor((length - 1.0f) / 2.0f);
    float alpha = (2.0f * floor_radius + 1.0f) *
                  (floor_radius * (floor_radius + 1.0f) - 3.0f * sigma2);
    alpha /= 6.0f * (sigma2 - (floor_radius + 1.0f) * (floor_radius + 1.0f));
    return static_cast<double>(floor_radius + alpha);
}

inline std::uint8_t box_blur_save_u8(std::uint32_t bulk)
{
    return static_cast<std::uint8_t>((bulk + (1u << 23)) >> 24);
}

void box_blur_horizontal_buffer(
    const std::uint8_t* source_data,
    std::uint8_t* target_data,
    int width,
    int height,
    int channels,
    std::size_t stride,
    double radius_value)
{
    const float float_radius = static_cast<float>(radius_value);
    const int radius = static_cast<int>(float_radius);
    const std::uint32_t ww = static_cast<std::uint32_t>(
        static_cast<float>(1u << 24) / (float_radius * 2.0f + 1.0f));
    const std::uint32_t fw = ((1u << 24) - static_cast<std::uint32_t>(radius * 2 + 1) * ww) / 2u;
    const int last_x = width - 1;
    const int edge_a = std::min(radius + 1, width);
    const int edge_b = std::max(width - radius - 1, 0);

    for (int y = 0; y < height; ++y) {
        const std::size_t row = static_cast<std::size_t>(y) * stride;
        for (int channel = 0; channel < channels; ++channel) {
            auto sample = [&](int x) -> std::uint32_t {
                return source_data[
                    row +
                    static_cast<std::size_t>(x) * channels +
                    static_cast<std::size_t>(channel)];
            };
            auto save = [&](int x, std::uint32_t bulk) {
                target_data[
                    row +
                    static_cast<std::size_t>(x) * channels +
                    static_cast<std::size_t>(channel)] = box_blur_save_u8(bulk);
            };

            std::uint32_t acc = sample(0) * static_cast<std::uint32_t>(radius + 1);
            for (int x = 0; x < edge_a - 1; ++x) {
                acc += sample(x);
            }
            acc += sample(last_x) * static_cast<std::uint32_t>(radius - edge_a + 1);

            if (edge_a <= edge_b) {
                for (int x = 0; x < edge_a; ++x) {
                    acc -= sample(0);
                    acc += sample(x + radius);
                    const std::uint32_t bulk = acc * ww + (sample(0) + sample(x + radius + 1)) * fw;
                    save(x, bulk);
                }
                for (int x = edge_a; x < edge_b; ++x) {
                    acc -= sample(x - radius - 1);
                    acc += sample(x + radius);
                    const std::uint32_t bulk =
                        acc * ww + (sample(x - radius - 1) + sample(x + radius + 1)) * fw;
                    save(x, bulk);
                }
                for (int x = edge_b; x <= last_x; ++x) {
                    acc -= sample(x - radius - 1);
                    acc += sample(last_x);
                    const std::uint32_t bulk =
                        acc * ww + (sample(x - radius - 1) + sample(last_x)) * fw;
                    save(x, bulk);
                }
            } else {
                for (int x = 0; x < edge_b; ++x) {
                    acc -= sample(0);
                    acc += sample(x + radius);
                    const std::uint32_t bulk = acc * ww + (sample(0) + sample(x + radius + 1)) * fw;
                    save(x, bulk);
                }
                for (int x = edge_b; x < edge_a; ++x) {
                    acc -= sample(0);
                    acc += sample(last_x);
                    const std::uint32_t bulk = acc * ww + (sample(0) + sample(last_x)) * fw;
                    save(x, bulk);
                }
                for (int x = edge_a; x <= last_x; ++x) {
                    acc -= sample(x - radius - 1);
                    acc += sample(last_x);
                    const std::uint32_t bulk =
                        acc * ww + (sample(x - radius - 1) + sample(last_x)) * fw;
                    save(x, bulk);
                }
            }
        }
    }
}

void box_blur_vertical_buffer(
    const std::uint8_t* source_data,
    std::uint8_t* target_data,
    int width,
    int height,
    int channels,
    std::size_t stride,
    double radius_value)
{
    const float float_radius = static_cast<float>(radius_value);
    const int radius = static_cast<int>(float_radius);
    const std::uint32_t ww = static_cast<std::uint32_t>(
        static_cast<float>(1u << 24) / (float_radius * 2.0f + 1.0f));
    const std::uint32_t fw = ((1u << 24) - static_cast<std::uint32_t>(radius * 2 + 1) * ww) / 2u;
    const int last_y = height - 1;
    const int edge_a = std::min(radius + 1, height);
    const int edge_b = std::max(height - radius - 1, 0);

    for (int x = 0; x < width; ++x) {
        const std::size_t column = static_cast<std::size_t>(x) * channels;
        for (int channel = 0; channel < channels; ++channel) {
            auto sample = [&](int y) -> std::uint32_t {
                return source_data[
                    static_cast<std::size_t>(y) * stride +
                    column +
                    static_cast<std::size_t>(channel)];
            };
            auto save = [&](int y, std::uint32_t bulk) {
                target_data[
                    static_cast<std::size_t>(y) * stride +
                    column +
                    static_cast<std::size_t>(channel)] = box_blur_save_u8(bulk);
            };

            std::uint32_t acc = sample(0) * static_cast<std::uint32_t>(radius + 1);
            for (int y = 0; y < edge_a - 1; ++y) {
                acc += sample(y);
            }
            acc += sample(last_y) * static_cast<std::uint32_t>(radius - edge_a + 1);

            if (edge_a <= edge_b) {
                for (int y = 0; y < edge_a; ++y) {
                    acc -= sample(0);
                    acc += sample(y + radius);
                    const std::uint32_t bulk = acc * ww + (sample(0) + sample(y + radius + 1)) * fw;
                    save(y, bulk);
                }
                for (int y = edge_a; y < edge_b; ++y) {
                    acc -= sample(y - radius - 1);
                    acc += sample(y + radius);
                    const std::uint32_t bulk =
                        acc * ww + (sample(y - radius - 1) + sample(y + radius + 1)) * fw;
                    save(y, bulk);
                }
                for (int y = edge_b; y <= last_y; ++y) {
                    acc -= sample(y - radius - 1);
                    acc += sample(last_y);
                    const std::uint32_t bulk =
                        acc * ww + (sample(y - radius - 1) + sample(last_y)) * fw;
                    save(y, bulk);
                }
            } else {
                for (int y = 0; y < edge_b; ++y) {
                    acc -= sample(0);
                    acc += sample(y + radius);
                    const std::uint32_t bulk = acc * ww + (sample(0) + sample(y + radius + 1)) * fw;
                    save(y, bulk);
                }
                for (int y = edge_b; y < edge_a; ++y) {
                    acc -= sample(0);
                    acc += sample(last_y);
                    const std::uint32_t bulk = acc * ww + (sample(0) + sample(last_y)) * fw;
                    save(y, bulk);
                }
                for (int y = edge_a; y <= last_y; ++y) {
                    acc -= sample(y - radius - 1);
                    acc += sample(last_y);
                    const std::uint32_t bulk =
                        acc * ww + (sample(y - radius - 1) + sample(last_y)) * fw;
                    save(y, bulk);
                }
            }
        }
    }
}

int filter_box_blur_passes_image_into(
    const PillowCImage* source,
    double xradius,
    double yradius,
    int passes,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (!valid_box_blur_radius(xradius) || !valid_box_blur_radius(yradius)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (passes <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
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

    if (xradius == 0.0 && yradius == 0.0) {
        std::memcpy(target->pixels.data(), source_data, source->pixels.size());
        return PILLOW_C_OK;
    }

    try {
        if (passes == 1) {
            if (xradius != 0.0 && yradius != 0.0) {
                std::vector<std::uint8_t> temp(source->pixels.size());
                box_blur_horizontal_buffer(
                    source_data,
                    temp.data(),
                    source->width,
                    source->height,
                    source->channels,
                    source->stride,
                    xradius);
                box_blur_vertical_buffer(
                    temp.data(),
                    target->pixels.data(),
                    source->width,
                    source->height,
                    source->channels,
                    source->stride,
                    yradius);
            } else if (xradius != 0.0) {
                box_blur_horizontal_buffer(
                    source_data,
                    target->pixels.data(),
                    source->width,
                    source->height,
                    source->channels,
                    source->stride,
                    xradius);
            } else {
                box_blur_vertical_buffer(
                    source_data,
                    target->pixels.data(),
                    source->width,
                    source->height,
                    source->channels,
                    source->stride,
                    yradius);
            }
            return PILLOW_C_OK;
        }

        const std::size_t byte_count = source->pixels.size();
        std::vector<std::uint8_t> temp_a(byte_count);
        std::vector<std::uint8_t> temp_b(byte_count);
        const std::uint8_t* current = source_data;
        std::uint8_t* next = temp_a.data();
        bool next_is_a = true;

        for (int pass = 0; pass < passes && xradius != 0.0; ++pass) {
            box_blur_horizontal_buffer(
                current,
                next,
                source->width,
                source->height,
                source->channels,
                source->stride,
                xradius);
            current = next;
            next = next_is_a ? temp_b.data() : temp_a.data();
            next_is_a = !next_is_a;
        }
        for (int pass = 0; pass < passes && yradius != 0.0; ++pass) {
            box_blur_vertical_buffer(
                current,
                next,
                source->width,
                source->height,
                source->channels,
                source->stride,
                yradius);
            current = next;
            next = next_is_a ? temp_b.data() : temp_a.data();
            next_is_a = !next_is_a;
        }
        std::memcpy(target->pixels.data(), current, byte_count);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int filter_box_blur_image_into(
    const PillowCImage* source,
    double xradius,
    double yradius,
    PillowCImage* target)
{
    return filter_box_blur_passes_image_into(source, xradius, yradius, 1, target);
}

int filter_gaussian_blur_image_into(
    const PillowCImage* source,
    double xradius,
    double yradius,
    PillowCImage* target)
{
    constexpr int passes = 3;
    if (!std::isfinite(xradius) || !std::isfinite(yradius)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const double xbox_radius = gaussian_blur_radius(xradius, passes);
    const double ybox_radius = gaussian_blur_radius(yradius, passes);
    return filter_box_blur_passes_image_into(source, xbox_radius, ybox_radius, passes, target);
}

int filter_unsharp_mask_image_into(
    const PillowCImage* source,
    double radius,
    int percent,
    int threshold,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!pillow_c_image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (!std::isfinite(radius)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    try {
        PillowCImage blurred{
            source->width,
            source->height,
            source->mode,
            source->channels,
            source->stride,
            std::vector<std::uint8_t>(source->pixels.size())};
        const int blur_status = filter_gaussian_blur_image_into(source, radius, radius, &blurred);
        if (blur_status != PILLOW_C_OK) {
            return blur_status;
        }

        std::vector<std::uint8_t> source_snapshot;
        const std::uint8_t* source_data = source->pixels.data();
        if (source == target) {
            source_snapshot = source->pixels;
            source_data = source_snapshot.data();
        }

        for (std::size_t index = 0; index < source->pixels.size(); ++index) {
            const int source_value = static_cast<int>(source_data[index]);
            const int diff = source_value - static_cast<int>(blurred.pixels[index]);
            if (std::abs(diff) > threshold) {
                const int sharpened = source_value + diff * percent / 100;
                target->pixels[index] = pillow_c_clip_u8_int(sharpened);
            } else {
                target->pixels[index] = static_cast<std::uint8_t>(source_value);
            }
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

bool valid_color_lut_mode_target(const PillowCImage* source, int target_mode, int table_channels)
{
    if (!source || source->channels < 3 || table_channels < 3 || table_channels > 4) {
        return false;
    }
    const int target_channels = channels_for_mode(target_mode);
    if (target_channels < table_channels) {
        return false;
    }
    if (target_channels > table_channels && target_channels > source->channels) {
        return false;
    }
    return true;
}

bool valid_color_lut_size(int size_1d, int size_2d, int size_3d)
{
    return size_1d >= 2 && size_1d <= 65 &&
           size_2d >= 2 && size_2d <= 65 &&
           size_3d >= 2 && size_3d <= 65;
}

bool checked_color_lut_table_count(
    int table_channels,
    int size_1d,
    int size_2d,
    int size_3d,
    std::size_t* out_count)
{
    if (!out_count || table_channels < 3 || table_channels > 4 || !valid_color_lut_size(size_1d, size_2d, size_3d)) {
        return false;
    }
    std::size_t count = static_cast<std::size_t>(table_channels);
    count *= static_cast<std::size_t>(size_1d);
    count *= static_cast<std::size_t>(size_2d);
    count *= static_cast<std::size_t>(size_3d);
    *out_count = count;
    return true;
}

std::int16_t prepare_color_lut_value(double value)
{
    constexpr double high_limit =
        (static_cast<double>(0x7fff) - 0.5) /
        static_cast<double>(255 << COLOR_LUT_PRECISION_BITS);
    constexpr double low_limit =
        (static_cast<double>(-0x8000) + 0.5) /
        static_cast<double>(255 << COLOR_LUT_PRECISION_BITS);
    if (value >= high_limit) {
        return static_cast<std::int16_t>(0x7fff);
    }
    if (value <= low_limit) {
        return static_cast<std::int16_t>(-0x8000);
    }
    const double scaled = value * static_cast<double>(255 << COLOR_LUT_PRECISION_BITS);
    return static_cast<std::int16_t>(scaled < 0.0 ? scaled - 0.5 : scaled + 0.5);
}

int prepare_color_lut_table(
    const double* table,
    std::size_t table_count,
    std::vector<std::int16_t>* out_table)
{
    if (!table || !out_table) {
        return PILLOW_C_NULL_POINTER;
    }
    try {
        out_table->resize(table_count);
        for (std::size_t index = 0; index < table_count; ++index) {
            if (!std::isfinite(table[index])) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            (*out_table)[index] = prepare_color_lut_value(table[index]);
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

inline std::uint8_t clip_color_lut_u8(int value)
{
    const int shifted = (value + COLOR_LUT_PRECISION_ROUNDING) >> COLOR_LUT_PRECISION_BITS;
    if (shifted <= 0) {
        return 0;
    }
    if (shifted >= 256) {
        return 255;
    }
    return static_cast<std::uint8_t>(shifted);
}

template <int Channels>
void color_lut_interpolate(
    std::int16_t* out,
    const std::int16_t* left,
    const std::int16_t* right,
    std::int16_t shift)
{
    for (int channel = 0; channel < Channels; ++channel) {
        out[channel] = static_cast<std::int16_t>(
            (left[channel] * ((1 << COLOR_LUT_SHIFT_BITS) - shift) + right[channel] * shift) >>
            COLOR_LUT_SHIFT_BITS);
    }
}

inline int color_lut_table_index_3d(int index_1d, int index_2d, int index_3d, int size_1d, int size_1d_2d)
{
    return index_1d + index_2d * size_1d + index_3d * size_1d_2d;
}

template <int TableChannels>
void color_lut_filter_pixel(
    const std::uint8_t* source_pixel,
    const std::int16_t* table,
    int size_1d,
    int size_1d_2d,
    std::uint32_t scale_1d,
    std::uint32_t scale_2d,
    std::uint32_t scale_3d,
    std::uint8_t* target_pixel)
{
    const std::uint32_t index_1d = static_cast<std::uint32_t>(source_pixel[0]) * scale_1d;
    const std::uint32_t index_2d = static_cast<std::uint32_t>(source_pixel[1]) * scale_2d;
    const std::uint32_t index_3d = static_cast<std::uint32_t>(source_pixel[2]) * scale_3d;
    const auto shift_1d = static_cast<std::int16_t>(
        (COLOR_LUT_SCALE_MASK & index_1d) >> (COLOR_LUT_SCALE_BITS - COLOR_LUT_SHIFT_BITS));
    const auto shift_2d = static_cast<std::int16_t>(
        (COLOR_LUT_SCALE_MASK & index_2d) >> (COLOR_LUT_SCALE_BITS - COLOR_LUT_SHIFT_BITS));
    const auto shift_3d = static_cast<std::int16_t>(
        (COLOR_LUT_SCALE_MASK & index_3d) >> (COLOR_LUT_SCALE_BITS - COLOR_LUT_SHIFT_BITS));
    const int idx = TableChannels * color_lut_table_index_3d(
        static_cast<int>(index_1d >> COLOR_LUT_SCALE_BITS),
        static_cast<int>(index_2d >> COLOR_LUT_SCALE_BITS),
        static_cast<int>(index_3d >> COLOR_LUT_SCALE_BITS),
        size_1d,
        size_1d_2d);

    std::int16_t result[4] = {0, 0, 0, 0};
    std::int16_t left[4] = {0, 0, 0, 0};
    std::int16_t right[4] = {0, 0, 0, 0};
    std::int16_t left_left[4] = {0, 0, 0, 0};
    std::int16_t left_right[4] = {0, 0, 0, 0};
    std::int16_t right_left[4] = {0, 0, 0, 0};
    std::int16_t right_right[4] = {0, 0, 0, 0};

    color_lut_interpolate<TableChannels>(left_left, &table[idx], &table[idx + TableChannels], shift_1d);
    color_lut_interpolate<TableChannels>(
        left_right,
        &table[idx + size_1d * TableChannels],
        &table[idx + size_1d * TableChannels + TableChannels],
        shift_1d);
    color_lut_interpolate<TableChannels>(left, left_left, left_right, shift_2d);
    color_lut_interpolate<TableChannels>(
        right_left,
        &table[idx + size_1d_2d * TableChannels],
        &table[idx + size_1d_2d * TableChannels + TableChannels],
        shift_1d);
    color_lut_interpolate<TableChannels>(
        right_right,
        &table[idx + size_1d_2d * TableChannels + size_1d * TableChannels],
        &table[idx + size_1d_2d * TableChannels + size_1d * TableChannels + TableChannels],
        shift_1d);
    color_lut_interpolate<TableChannels>(right, right_left, right_right, shift_2d);
    color_lut_interpolate<TableChannels>(result, left, right, shift_3d);

    for (int channel = 0; channel < TableChannels; ++channel) {
        target_pixel[channel] = clip_color_lut_u8(result[channel]);
    }
}

int filter_color_3d_lut_image_into(
    const PillowCImage* source,
    int target_mode,
    int table_channels,
    int size_1d,
    int size_2d,
    int size_3d,
    const double* table,
    std::size_t table_count,
    PillowCImage* target)
{
    if (!source || !table || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    std::size_t expected_count = 0;
    if (!checked_color_lut_table_count(table_channels, size_1d, size_2d, size_3d, &expected_count)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (table_count != expected_count) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!valid_color_lut_mode_target(source, target_mode, table_channels)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int target_channels = channels_for_mode(target_mode);
    if (!pillow_c_image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }

    std::vector<std::int16_t> prepared_table;
    const int prepare_status = prepare_color_lut_table(table, table_count, &prepared_table);
    if (prepare_status != PILLOW_C_OK) {
        return prepare_status;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    std::vector<std::uint8_t> source_snapshot;
    const std::uint8_t* source_data = source->pixels.data();
    if (source == target) {
        source_snapshot = source->pixels;
        source_data = source_snapshot.data();
    }

    const std::uint32_t scale_1d = static_cast<std::uint32_t>(
        static_cast<double>(size_1d - 1) / 255.0 * static_cast<double>(1 << COLOR_LUT_SCALE_BITS));
    const std::uint32_t scale_2d = static_cast<std::uint32_t>(
        static_cast<double>(size_2d - 1) / 255.0 * static_cast<double>(1 << COLOR_LUT_SCALE_BITS));
    const std::uint32_t scale_3d = static_cast<std::uint32_t>(
        static_cast<double>(size_3d - 1) / 255.0 * static_cast<double>(1 << COLOR_LUT_SCALE_BITS));
    const int size_1d_2d = size_1d * size_2d;

    for (int y = 0; y < source->height; ++y) {
        const std::size_t source_row = static_cast<std::size_t>(y) * source->stride;
        const std::size_t target_row = static_cast<std::size_t>(y) * target->stride;
        for (int x = 0; x < source->width; ++x) {
            const auto* source_pixel =
                source_data + source_row + static_cast<std::size_t>(x) * source->channels;
            auto* target_pixel =
                target->pixels.data() + target_row + static_cast<std::size_t>(x) * target_channels;

            if (table_channels == 3) {
                color_lut_filter_pixel<3>(
                    source_pixel,
                    prepared_table.data(),
                    size_1d,
                    size_1d_2d,
                    scale_1d,
                    scale_2d,
                    scale_3d,
                    target_pixel);
                if (target_channels > 3) {
                    target_pixel[3] = source_pixel[3];
                }
            } else {
                color_lut_filter_pixel<4>(
                    source_pixel,
                    prepared_table.data(),
                    size_1d,
                    size_1d_2d,
                    scale_1d,
                    scale_2d,
                    scale_3d,
                    target_pixel);
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
        pillow_c_image_shape_matches(target, source)) {
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        pillow_c_copy_palette_if_same_mode(source, target);
        return PILLOW_C_OK;
    }

    int status = PILLOW_C_OK;
    switch (resample) {
    case PILLOW_C_RESAMPLE_NEAREST:
        status = resize_nearest_box_into(source, out_width, out_height, box_left, box_top, box_right, box_bottom, target);
        break;
    case PILLOW_C_RESAMPLE_BOX:
    case PILLOW_C_RESAMPLE_HAMMING:
    case PILLOW_C_RESAMPLE_BILINEAR:
    case PILLOW_C_RESAMPLE_BICUBIC:
    case PILLOW_C_RESAMPLE_LANCZOS:
        status = resize_filter_box_into(source, out_width, out_height, resample, box_left, box_top, box_right, box_bottom, target);
        break;
    default:
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (status == PILLOW_C_OK) {
        pillow_c_copy_palette_if_same_mode(source, target);
    }
    return status;
}

bool valid_reduce_box(const PillowCImage* source, int left, int top, int right, int bottom);
bool supports_reduce_mode(const PillowCImage* source);
int reduce_output_width(int left, int right, int xscale);
int reduce_output_height(int top, int bottom, int yscale);
int reduce_image_into(
    const PillowCImage* source,
    int xscale,
    int yscale,
    int left,
    int top,
    int right,
    int bottom,
    PillowCImage* target);

int resize_resample_for_mode(const PillowCImage* source, int resample)
{
    if (!source) {
        return resample;
    }
    if (source->mode == PILLOW_C_MODE_1 || source->mode == PILLOW_C_MODE_P) {
        return PILLOW_C_RESAMPLE_NEAREST;
    }
    return resample;
}

bool get_resize_safe_box(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    double box_left,
    double box_top,
    double box_right,
    double box_bottom,
    int* safe_left,
    int* safe_top,
    int* safe_right,
    int* safe_bottom)
{
    if (!source || !safe_left || !safe_top || !safe_right || !safe_bottom || out_width <= 0 || out_height <= 0) {
        return false;
    }
    const ResampleFilterSpec* filter = filter_spec_for_resample(resample);
    if (!filter || !valid_resize_box(source, box_left, box_top, box_right, box_bottom)) {
        return false;
    }

    const double filter_support = filter->support - 0.5;
    const double scale_x = (box_right - box_left) / static_cast<double>(out_width);
    const double scale_y = (box_bottom - box_top) / static_cast<double>(out_height);
    const double support_x = filter_support * scale_x;
    const double support_y = filter_support * scale_y;

    *safe_left = std::max(0, static_cast<int>(box_left - support_x));
    *safe_top = std::max(0, static_cast<int>(box_top - support_y));
    *safe_right = std::min(source->width, static_cast<int>(std::ceil(box_right + support_x)));
    *safe_bottom = std::min(source->height, static_cast<int>(std::ceil(box_bottom + support_y)));
    return valid_reduce_box(source, *safe_left, *safe_top, *safe_right, *safe_bottom);
}

int resize_image_reducing_gap_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    double box_left,
    double box_top,
    double box_right,
    double box_bottom,
    double reducing_gap,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_width <= 0 ||
        out_height <= 0 ||
        !std::isfinite(reducing_gap) ||
        reducing_gap < 1.0 ||
        !valid_resize_box(source, box_left, box_top, box_right, box_bottom)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    resample = resize_resample_for_mode(source, resample);
    const bool supported =
        resample == PILLOW_C_RESAMPLE_NEAREST ||
        filter_spec_for_resample(resample) != nullptr;
    if (!supported) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    if (resample == PILLOW_C_RESAMPLE_NEAREST) {
        return resize_image_box_into(source, out_width, out_height, resample, box_left, box_top, box_right, box_bottom, target);
    }

    const int factor_x = std::max(1, static_cast<int>(((box_right - box_left) / static_cast<double>(out_width)) / reducing_gap));
    const int factor_y = std::max(1, static_cast<int>(((box_bottom - box_top) / static_cast<double>(out_height)) / reducing_gap));
    if (factor_x <= 1 && factor_y <= 1) {
        return resize_image_box_into(source, out_width, out_height, resample, box_left, box_top, box_right, box_bottom, target);
    }
    if (!supports_reduce_mode(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int reduce_left = 0;
    int reduce_top = 0;
    int reduce_right = 0;
    int reduce_bottom = 0;
    if (!get_resize_safe_box(
            source,
            out_width,
            out_height,
            resample,
            box_left,
            box_top,
            box_right,
            box_bottom,
            &reduce_left,
            &reduce_top,
            &reduce_right,
            &reduce_bottom)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int reduced_width = reduce_output_width(reduce_left, reduce_right, factor_x);
    const int reduced_height = reduce_output_height(reduce_top, reduce_bottom, factor_y);
    std::size_t reduced_stride = 0;
    std::size_t reduced_size = 0;
    if (!checked_image_size(reduced_width, reduced_height, source->channels, &reduced_stride, &reduced_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        PillowCImage reduced{
            reduced_width,
            reduced_height,
            source->mode,
            source->channels,
            reduced_stride,
            std::vector<std::uint8_t>(reduced_size),
            source->palette_rgb};
        reduced.palette_alpha = source->palette_alpha;
        reduced.palette_alpha_mode = source->palette_alpha_mode;

        int status = reduce_image_into(source, factor_x, factor_y, reduce_left, reduce_top, reduce_right, reduce_bottom, &reduced);
        if (status != PILLOW_C_OK) {
            return status;
        }

        const double adjusted_left = (box_left - static_cast<double>(reduce_left)) / static_cast<double>(factor_x);
        const double adjusted_top = (box_top - static_cast<double>(reduce_top)) / static_cast<double>(factor_y);
        const double adjusted_right = (box_right - static_cast<double>(reduce_left)) / static_cast<double>(factor_x);
        const double adjusted_bottom = (box_bottom - static_cast<double>(reduce_top)) / static_cast<double>(factor_y);
        status = resize_image_box_into(
            &reduced,
            out_width,
            out_height,
            resample,
            adjusted_left,
            adjusted_top,
            adjusted_right,
            adjusted_bottom,
            target);
        if (status == PILLOW_C_OK) {
            pillow_c_copy_palette_if_same_mode(source, target);
        }
        return status;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

bool valid_reduce_box(const PillowCImage* source, int left, int top, int right, int bottom)
{
    return source &&
           left >= 0 &&
           top >= 0 &&
           right <= source->width &&
           bottom <= source->height &&
           right > left &&
           bottom > top;
}

bool supports_reduce_mode(const PillowCImage* source)
{
    return source &&
           (source->mode == PILLOW_C_MODE_L ||
            source->mode == PILLOW_C_MODE_LA ||
            source->mode == PILLOW_C_MODE_RGB ||
            source->mode == PILLOW_C_MODE_RGBA ||
            source->mode == PILLOW_C_MODE_CMYK ||
            source->mode == PILLOW_C_MODE_I ||
            source->mode == PILLOW_C_MODE_F);
}

int reduce_output_width(int left, int right, int xscale)
{
    return pillow_c_ceil_div_int(right - left, xscale);
}

int reduce_output_height(int top, int bottom, int yscale)
{
    return pillow_c_ceil_div_int(bottom - top, yscale);
}

int reduce_image_into(
    const PillowCImage* source,
    int xscale,
    int yscale,
    int left,
    int top,
    int right,
    int bottom,
    PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (xscale <= 0 || yscale <= 0 || !valid_reduce_box(source, left, top, right, bottom)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!supports_reduce_mode(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int out_width = reduce_output_width(left, right, xscale);
    const int out_height = reduce_output_height(top, bottom, yscale);
    if (!pillow_c_image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }
    if (target->pixels.empty()) {
        return PILLOW_C_OK;
    }

    if (source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F) {
        // Pillow 11.3.0 Reduce.c 32bpc: one 32-bit sample per pixel,
        // block average in double; I stores ROUND_UP(ss / count), F
        // stores the float32 cast (no rounding).
        const bool float_mode = source->mode == PILLOW_C_MODE_F;
        for (int out_y = 0; out_y < out_height; ++out_y) {
            const int y0 = top + out_y * yscale;
            const int y1 = std::min(y0 + yscale, bottom);
            std::uint8_t* dst_row = target->pixels.data() + static_cast<std::size_t>(out_y) * target->stride;
            for (int out_x = 0; out_x < out_width; ++out_x) {
                const int x0 = left + out_x * xscale;
                const int x1 = std::min(x0 + xscale, right);
                const auto count = static_cast<std::uint32_t>((x1 - x0) * (y1 - y0));
                double sum = 0.0;
                for (int y = y0; y < y1; ++y) {
                    const std::uint8_t* src_row =
                        source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
                    for (int x = x0; x < x1; ++x) {
                        const std::uint8_t* px = src_row + static_cast<std::size_t>(x) * 4u;
                        sum += float_mode
                            ? static_cast<double>(pillow_c_read_f32_le(px))
                            : static_cast<double>(read_le_i32(px));
                    }
                }
                const double average = sum / static_cast<double>(count);
                std::uint8_t* dst = dst_row + static_cast<std::size_t>(out_x) * 4u;
                if (float_mode) {
                    pillow_c_write_f32_le(dst, static_cast<float>(average));
                } else {
                    pillow_c_write_i32_le(
                        dst,
                        static_cast<std::uint32_t>(resize_round_i32_sample(average)));
                }
            }
        }
        return PILLOW_C_OK;
    }

    for (int out_y = 0; out_y < out_height; ++out_y) {
        const int y0 = top + out_y * yscale;
        const int y1 = std::min(y0 + yscale, bottom);
        std::uint8_t* dst_row = target->pixels.data() + static_cast<std::size_t>(out_y) * target->stride;
        for (int out_x = 0; out_x < out_width; ++out_x) {
            const int x0 = left + out_x * xscale;
            const int x1 = std::min(x0 + xscale, right);
            const auto count = static_cast<std::uint32_t>((x1 - x0) * (y1 - y0));
            std::uint8_t* dst = dst_row + static_cast<std::size_t>(out_x) * source->channels;
            if (source->mode == PILLOW_C_MODE_LA || source->mode == PILLOW_C_MODE_RGBA) {
                const int alpha_channel = source->channels - 1;
                std::uint64_t alpha_sum = 0;
                for (int y = y0; y < y1; ++y) {
                    const std::uint8_t* src_row =
                        source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
                    for (int x = x0; x < x1; ++x) {
                        alpha_sum += src_row[static_cast<std::size_t>(x) * source->channels +
                                             static_cast<std::size_t>(alpha_channel)];
                    }
                }
                const std::uint8_t alpha = pillow_c_reduce_average_u8(alpha_sum, count);
                dst[alpha_channel] = alpha;
                for (int channel = 0; channel < alpha_channel; ++channel) {
                    std::uint64_t premultiplied_sum = 0;
                    for (int y = y0; y < y1; ++y) {
                        const std::uint8_t* src_row =
                            source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
                        for (int x = x0; x < x1; ++x) {
                            const std::uint8_t* src =
                                src_row + static_cast<std::size_t>(x) * source->channels;
                            premultiplied_sum += pillow_c_mul_div_255(src[channel], src[alpha_channel]);
                        }
                    }
                    const std::uint8_t premultiplied = pillow_c_reduce_average_u8(premultiplied_sum, count);
                    if (alpha == 0 || alpha == 255) {
                        dst[channel] = premultiplied;
                    } else {
                        dst[channel] = pillow_c_clip_u8_int(255 * static_cast<int>(premultiplied) / alpha);
                    }
                }
            } else {
                for (int channel = 0; channel < source->channels; ++channel) {
                    std::uint64_t sum = 0;
                    for (int y = y0; y < y1; ++y) {
                        const std::uint8_t* src_row =
                            source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
                        for (int x = x0; x < x1; ++x) {
                            sum += src_row[static_cast<std::size_t>(x) * source->channels +
                                           static_cast<std::size_t>(channel)];
                        }
                    }
                    dst[channel] = pillow_c_reduce_average_u8(sum, count);
                }
            }
        }
    }
    return PILLOW_C_OK;
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
    if (!pillow_c_image_shape_matches(target, requested_width, requested_height, source->mode, source->channels)) {
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
    if (!pillow_c_image_shape_matches(target, requested_width, requested_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    if (resized_width == requested_width && resized_height == requested_height) {
        return resize_image_into(source, requested_width, requested_height, resample, target);
    }

    status = pillow_c_fill_image_pixels(target, color, color_size);
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
        return pillow_c_paste_image_pixels_into(target, &resized, left, top);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

} // namespace

extern "C" __declspec(dllexport) int pillow_c_image_resize_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    PillowCImage* target)
{
     const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return resize_image_into(source, out_width, out_height, resample, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_resize_box_into(
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
     const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return resize_image_box_into(source, out_width, out_height, resample, box_left, box_top, box_right, box_bottom, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_resize_reducing_gap_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    double box_left,
    double box_top,
    double box_right,
    double box_bottom,
    double reducing_gap,
    PillowCImage* target)
{
     const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return resize_image_reducing_gap_into(
        source,
        out_width,
        out_height,
        resample,
        box_left,
        box_top,
        box_right,
        box_bottom,
        reducing_gap,
        target);
}

extern "C" __declspec(dllexport) int pillow_c_image_reduce_into(
    const PillowCImage* source,
    int xscale,
    int yscale,
    int left,
    int top,
    int right,
    int bottom,
    PillowCImage* target)
{
    return reduce_image_into(source, xscale, yscale, left, top, right, bottom, target);
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

extern "C" __declspec(dllexport) int pillow_c_image_filter_box_blur_into(
    const PillowCImage* source,
    double xradius,
    double yradius,
    PillowCImage* target)
{
    return filter_box_blur_image_into(source, xradius, yradius, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_filter_gaussian_blur_into(
    const PillowCImage* source,
    double xradius,
    double yradius,
    PillowCImage* target)
{
    return filter_gaussian_blur_image_into(source, xradius, yradius, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_filter_unsharp_mask_into(
    const PillowCImage* source,
    double radius,
    int percent,
    int threshold,
    PillowCImage* target)
{
    return filter_unsharp_mask_image_into(source, radius, percent, threshold, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_filter_color_3d_lut_into(
    const PillowCImage* source,
    int target_mode,
    int table_channels,
    int size_1d,
    int size_2d,
    int size_3d,
    const double* table,
    std::size_t table_count,
    PillowCImage* target)
{
    return filter_color_3d_lut_image_into(
        source,
        target_mode,
        table_channels,
        size_1d,
        size_2d,
        size_3d,
        table,
        table_count,
        target);
}

bool resize_uses_rgba_premultiply(const PillowCImage* source, int resample)
{
    return resample != PILLOW_C_RESAMPLE_NEAREST && source &&
        ((source->mode == PILLOW_C_MODE_RGBA && source->channels == 4) ||
         (source->mode == PILLOW_C_MODE_LA && source->channels == 2));
}

// Pillow 11.3.0 resize rules shared by the plain/box/reducing-gap
// routes: mode 1/P always forces NEAREST, and RGBA/LA with a
// non-NEAREST resample resamples the premultiplied La/RGBa values
// (the filter premultiplies per sample with Pillow's exact
// (c*a+127)//255 and unpremultiplies with c*255//a at the end);
// the RGBA/LA inner resize drops reducing_gap.
int resize_with_mode_rules(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    const double* box,
    const double* reducing_gap,
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
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    const int effective_resample = resize_resample_for_mode(source, resample);
    const bool drop_reducing_gap = reducing_gap != nullptr &&
        resize_uses_rgba_premultiply(source, effective_resample);

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
        int status = PILLOW_C_OK;
        if (box && reducing_gap && !drop_reducing_gap) {
            status = resize_image_reducing_gap_into(
                source, out_width, out_height, resample,
                box[0], box[1], box[2], box[3], *reducing_gap, image);
        } else if (box) {
            status = resize_image_box_into(
                source, out_width, out_height, effective_resample,
                box[0], box[1], box[2], box[3], image);
        } else {
            status = resize_image_into(source, out_width, out_height, effective_resample, image);
        }
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
    return resize_with_mode_rules(source, out_width, out_height, resample, nullptr, nullptr, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_resize_box(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    double box_left,
    double box_top,
    double box_right,
    double box_bottom,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (out_width <= 0 || out_height <= 0 || !valid_resize_box(source, box_left, box_top, box_right, box_bottom)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const double box[4] = {box_left, box_top, box_right, box_bottom};
    return resize_with_mode_rules(source, out_width, out_height, resample, box, nullptr, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_resize_reducing_gap(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    double box_left,
    double box_top,
    double box_right,
    double box_bottom,
    double reducing_gap,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (out_width <= 0 ||
        out_height <= 0 ||
        !std::isfinite(reducing_gap) ||
        reducing_gap < 1.0 ||
        !valid_resize_box(source, box_left, box_top, box_right, box_bottom)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const double box[4] = {box_left, box_top, box_right, box_bottom};
    return resize_with_mode_rules(
        source, out_width, out_height, resample, box, &reducing_gap, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_reduce(
    const PillowCImage* source,
    int xscale,
    int yscale,
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
    if (xscale <= 0 || yscale <= 0 || !valid_reduce_box(source, left, top, right, bottom)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!supports_reduce_mode(source)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int out_width = reduce_output_width(left, right, xscale);
    const int out_height = reduce_output_height(top, bottom, yscale);
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
        const int status = reduce_image_into(source, xscale, yscale, left, top, right, bottom, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_filter_box_blur(
    const PillowCImage* source,
    double xradius,
    double yradius,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!valid_box_blur_radius(xradius) || !valid_box_blur_radius(yradius)) {
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
        const int status = filter_box_blur_image_into(source, xradius, yradius, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_filter_gaussian_blur(
    const PillowCImage* source,
    double xradius,
    double yradius,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!std::isfinite(xradius) || !std::isfinite(yradius)) {
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
        const int status = filter_gaussian_blur_image_into(source, xradius, yradius, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_filter_unsharp_mask(
    const PillowCImage* source,
    double radius,
    int percent,
    int threshold,
    PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!std::isfinite(radius)) {
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
        const int status = filter_unsharp_mask_image_into(source, radius, percent, threshold, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_filter_color_3d_lut(
    const PillowCImage* source,
    int target_mode,
    int table_channels,
    int size_1d,
    int size_2d,
    int size_3d,
    const double* table,
    std::size_t table_count,
    PillowCImage** out_image)
{
    if (!source || !table || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    std::size_t expected_count = 0;
    if (!checked_color_lut_table_count(table_channels, size_1d, size_2d, size_3d, &expected_count)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (table_count != expected_count) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!valid_color_lut_mode_target(source, target_mode, table_channels)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int target_channels = channels_for_mode(target_mode);
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
        const int status = filter_color_3d_lut_image_into(
            source,
            target_mode,
            table_channels,
            size_1d,
            size_2d,
            size_3d,
            table,
            table_count,
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

int pillow_c_resize_image_into(
    const PillowCImage* source,
    int out_width,
    int out_height,
    int resample,
    PillowCImage* target)
{
    return resize_image_into(source, out_width, out_height, resample, target);
}

int pillow_c_proportional_resize_size(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    bool cover,
    int* out_width,
    int* out_height)
{
    return proportional_resize_size(source, requested_width, requested_height, cover, out_width, out_height);
}
