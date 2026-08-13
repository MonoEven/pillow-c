#include "pillow_c_internal.h"
#include "pillow_c_ops_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace {
inline std::uint8_t clip_u8_int(int value)
{
    return pillow_c_clip_u8_int(value);
}

struct ColorCountEntry {
    std::uint64_t count;
    std::uint8_t color[4];
};

} // namespace

std::size_t histogram_required_count(const PillowCImage* source)
{
    if (source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F) {
        return 256u;
    }
    return static_cast<std::size_t>(source->channels) * 256u;
}

int histogram_image_numeric_i(const PillowCImage* source, std::uint64_t* out_histogram)
{
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    if (pixels == 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* data = source->pixels.data();
    std::int32_t min_value = pillow_c_read_i32_le(data);
    std::int32_t max_value = min_value;
    for (std::size_t pixel = 1; pixel < pixels; ++pixel) {
        const std::int32_t value = pillow_c_read_i32_le(data + pixel * 4u);
        if (value < min_value) {
            min_value = value;
        } else if (value > max_value) {
            max_value = value;
        }
    }
    if (min_value == max_value) {
        return PILLOW_C_OK;
    }

    const double range = static_cast<double>(max_value) - static_cast<double>(min_value);
    if (!(range > 0.0) || !std::isfinite(range)) {
        return PILLOW_C_OK;
    }
    const double scale = 255.0 / range;
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        const std::int32_t value = pillow_c_read_i32_le(data + pixel * 4u);
        double scaled = (static_cast<double>(value) - static_cast<double>(min_value)) * scale;
        if (scaled < 0.0) {
            scaled = 0.0;
        } else if (scaled > 255.0) {
            scaled = 255.0;
        }
        ++out_histogram[static_cast<std::size_t>(scaled)];
    }
    return PILLOW_C_OK;
}

int histogram_image_numeric_f(const PillowCImage* source, std::uint64_t* out_histogram)
{
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    if (pixels == 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* data = source->pixels.data();
    float min_value = pillow_c_read_f32_le(data);
    float max_value = min_value;
    for (std::size_t pixel = 1; pixel < pixels; ++pixel) {
        const float value = pillow_c_read_f32_le(data + pixel * 4u);
        if (value < min_value) {
            min_value = value;
        } else if (value > max_value) {
            max_value = value;
        }
    }
    if (std::isnan(min_value) || std::isnan(max_value) || min_value == max_value) {
        return PILLOW_C_OK;
    }
    if (std::isinf(min_value)) {
        return PILLOW_C_OK;
    }
    if (std::isinf(max_value)) {
        if (max_value < 0.0f || !std::isfinite(min_value)) {
            return PILLOW_C_OK;
        }
        for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
            const float value = pillow_c_read_f32_le(data + pixel * 4u);
            if (std::isfinite(value)) {
                ++out_histogram[0];
            }
        }
        return PILLOW_C_OK;
    }
    if (!std::isfinite(min_value) || !std::isfinite(max_value)) {
        return PILLOW_C_OK;
    }

    const double range = static_cast<double>(max_value) - static_cast<double>(min_value);
    if (!(range > 0.0) || !std::isfinite(range)) {
        return PILLOW_C_OK;
    }
    const double scale = 255.0 / range;
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        const float value = pillow_c_read_f32_le(data + pixel * 4u);
        if (!std::isfinite(value)) {
            continue;
        }
        double scaled = (static_cast<double>(value) - static_cast<double>(min_value)) * scale;
        if (scaled < 0.0) {
            scaled = 0.0;
        } else if (scaled > 255.0) {
            scaled = 255.0;
        }
        ++out_histogram[static_cast<std::size_t>(scaled)];
    }
    return PILLOW_C_OK;
}

int histogram_image(const PillowCImage* source, std::uint64_t* out_histogram, std::size_t out_count)
{
    if (!source || !out_histogram) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t required = histogram_required_count(source);
    if (out_count != required) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_histogram, out_histogram + out_count, 0);
    if (source->mode == PILLOW_C_MODE_I) {
        return histogram_image_numeric_i(source, out_histogram);
    }
    if (source->mode == PILLOW_C_MODE_F) {
        return histogram_image_numeric_f(source, out_histogram);
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    const std::uint8_t* data = source->pixels.data();
    if (source->mode == PILLOW_C_MODE_LA && source->channels == 2) {
        for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
            const std::uint8_t value = data[pixel * 2u];
            ++out_histogram[value];
            ++out_histogram[256u + value];
        }
        return PILLOW_C_OK;
    }
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
    if (source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_statistics_mask_matches(mask, source->width, source->height)) {
        return PILLOW_C_MISMATCH;
    }
    const std::size_t required = histogram_required_count(source);
    if (out_count != required) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_histogram, out_histogram + out_count, 0);
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    const std::uint8_t* data = source->pixels.data();
    const std::uint8_t* mask_data = mask->pixels.data();
    if (source->mode == PILLOW_C_MODE_LA && source->channels == 2) {
        for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
            if (mask_data[pixel] == 0) {
                continue;
            }
            const std::uint8_t value = data[pixel * 2u];
            ++out_histogram[value];
            ++out_histogram[256u + value];
        }
        return PILLOW_C_OK;
    }
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

int entropy_image(const PillowCImage* source, const PillowCImage* mask, double* out_entropy)
{
    if (!source || !out_entropy) {
        return PILLOW_C_NULL_POINTER;
    }
    const bool numeric_mode = source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F;
    if (mask && numeric_mode) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (mask && !pillow_c_statistics_mask_matches(mask, source->width, source->height)) {
        return PILLOW_C_MISMATCH;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    const std::size_t histogram_count = numeric_mode ? 256u : static_cast<std::size_t>(source->channels) * 256u;
    const std::size_t samples = pixels * (numeric_mode ? 1u : static_cast<std::size_t>(source->channels));
    std::vector<std::uint64_t> histogram(histogram_count, 0);
    if (numeric_mode) {
        const int status = histogram_image(source, histogram.data(), histogram.size());
        if (status != PILLOW_C_OK) {
            return status;
        }
    } else {
        const std::uint8_t* data = source->pixels.data();
        const std::uint8_t* mask_data = mask ? mask->pixels.data() : nullptr;

        for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
            if (mask_data && mask_data[pixel] == 0) {
                continue;
            }
            const std::uint8_t* src = data + pixel * static_cast<std::size_t>(source->channels);
            for (int channel = 0; channel < source->channels; ++channel) {
                ++histogram[static_cast<std::size_t>(channel) * 256u + src[channel]];
            }
        }
    }

    if (!mask && samples == 0) {
        *out_entropy = std::numeric_limits<double>::quiet_NaN();
        return PILLOW_C_OK;
    }

    std::uint64_t total = 0;
    for (const std::uint64_t count : histogram) {
        total += count;
    }
    if (total == 0) {
        *out_entropy = std::numeric_limits<double>::quiet_NaN();
        return PILLOW_C_OK;
    }

    long double entropy = 0.0L;
    const long double inv_total = 1.0L / static_cast<long double>(total);
    for (const std::uint64_t count : histogram) {
        if (count == 0) {
            continue;
        }
        const long double p = static_cast<long double>(count) * inv_total;
        entropy -= p * (std::log(static_cast<double>(p)) / std::log(2.0));
    }
    *out_entropy = static_cast<double>(entropy);
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

int extrema_image_numeric(
    const PillowCImage* source,
    double* out_min,
    double* out_max,
    std::uint8_t* out_has_value,
    std::size_t out_count)
{
    if (!source || !out_min || !out_max || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }

    const bool mode_i = source->mode == PILLOW_C_MODE_I;
    const bool mode_f = source->mode == PILLOW_C_MODE_F;
    const std::size_t required = (mode_i || mode_f) ? 1u : static_cast<std::size_t>(source->channels);
    if (out_count != required) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_min, out_min + out_count, 0.0);
    std::fill(out_max, out_max + out_count, 0.0);
    std::fill(out_has_value, out_has_value + out_count, static_cast<std::uint8_t>(0));
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    if (pixels == 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* data = source->pixels.data();
    if (mode_i) {
        std::int32_t min_value = pillow_c_read_i32_le(data);
        std::int32_t max_value = min_value;
        for (std::size_t pixel = 1; pixel < pixels; ++pixel) {
            const std::int32_t value = pillow_c_read_i32_le(data + pixel * 4u);
            if (value < min_value) {
                min_value = value;
            } else if (value > max_value) {
                max_value = value;
            }
        }
        out_min[0] = static_cast<double>(min_value);
        out_max[0] = static_cast<double>(max_value);
        out_has_value[0] = 1;
        return PILLOW_C_OK;
    }

    if (mode_f) {
        float min_value = pillow_c_read_f32_le(data);
        float max_value = min_value;
        for (std::size_t pixel = 1; pixel < pixels; ++pixel) {
            const float value = pillow_c_read_f32_le(data + pixel * 4u);
            if (value < min_value) {
                min_value = value;
            } else if (value > max_value) {
                max_value = value;
            }
        }
        out_min[0] = static_cast<double>(min_value);
        out_max[0] = static_cast<double>(max_value);
        out_has_value[0] = 1;
        return PILLOW_C_OK;
    }

    for (int channel = 0; channel < source->channels; ++channel) {
        const std::uint8_t first = data[channel];
        out_min[channel] = static_cast<double>(first);
        out_max[channel] = static_cast<double>(first);
        out_has_value[channel] = 1;
    }

    for (std::size_t pixel = 1; pixel < pixels; ++pixel) {
        const std::uint8_t* src = data + pixel * source->channels;
        for (int channel = 0; channel < source->channels; ++channel) {
            const std::uint8_t value = src[channel];
            if (value < out_min[channel]) {
                out_min[channel] = static_cast<double>(value);
            } else if (value > out_max[channel]) {
                out_max[channel] = static_cast<double>(value);
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

int find_numeric_color_entry(const std::vector<ColorCountEntry>& entries, const std::uint8_t* color)
{
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (std::memcmp(entries[index].color, color, 4u) == 0) {
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

int getcolors_image_numeric(
    const PillowCImage* source,
    int maxcolors,
    std::uint64_t* out_counts,
    double* out_values,
    std::size_t out_capacity,
    std::size_t* out_count,
    int* out_exceeded)
{
    if (!source || !out_count || !out_exceeded) {
        return PILLOW_C_NULL_POINTER;
    }
    const bool mode_i = source->mode == PILLOW_C_MODE_I;
    const bool mode_f = source->mode == PILLOW_C_MODE_F;
    if (!mode_i && !mode_f) {
        return PILLOW_C_INVALID_ARGUMENT;
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
        const std::uint8_t* color = data + pixel * 4u;
        const int existing = find_numeric_color_entry(entries, color);
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
        std::memcpy(entry.color, color, 4u);
        try {
            entries.push_back(entry);
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
    }

    *out_count = entries.size();
    if (!out_counts && !out_values && out_capacity == 0) {
        return PILLOW_C_OK;
    }
    if (!out_counts || !out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_capacity < entries.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }

    for (std::size_t index = 0; index < entries.size(); ++index) {
        out_counts[index] = entries[index].count;
        out_values[index] = mode_i
            ? static_cast<double>(pillow_c_read_i32_le(entries[index].color))
            : static_cast<double>(pillow_c_read_f32_le(entries[index].color));
    }
    return PILLOW_C_OK;
}

bool autocontrast_supported_mode(const PillowCImage* source)
{
    return pillow_c_ops_supports_imageops_lut(source);
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
    if (mask && !pillow_c_statistics_mask_matches(mask, source->width, source->height)) {
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
    if (!pillow_c_image_shape_matches(target, source)) {
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
        std::vector<std::uint8_t> lut;
        const int status = build_autocontrast_lut(source, low_cutoff, high_cutoff, ignore_values, ignore_count, mask, preserve_tone, &lut);
        if (status != PILLOW_C_OK) {
            return status;
        }
        return preserve_tone
            ? pillow_c_ops_apply_single_lut_into(source, lut.data(), lut.size(), target)
            : pillow_c_ops_apply_point_lut_into(source, lut.data(), lut.size(), target);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_histogram(
    const PillowCImage* image,
    std::uint64_t* out_histogram,
    std::size_t out_count)
{
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return histogram_image(image, out_histogram, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_histogram_masked(
    const PillowCImage* image,
    const PillowCImage* mask,
    std::uint64_t* out_histogram,
    std::size_t out_count)
{
    const int image_refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (image_refresh_status != PILLOW_C_OK) {
        return image_refresh_status;
    }
    if (mask) {
        const int mask_refresh_status = pillow_c_refresh_const_buffer_view_image(mask);
        if (mask_refresh_status != PILLOW_C_OK) {
            return mask_refresh_status;
        }
    }
    return histogram_image_masked(image, mask, out_histogram, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_entropy(
    const PillowCImage* image,
    const PillowCImage* mask,
    double* out_entropy)
{
    const int image_refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (image_refresh_status != PILLOW_C_OK) {
        return image_refresh_status;
    }
    if (mask) {
        const int mask_refresh_status = pillow_c_refresh_const_buffer_view_image(mask);
        if (mask_refresh_status != PILLOW_C_OK) {
            return mask_refresh_status;
        }
    }
    return entropy_image(image, mask, out_entropy);
}

extern "C" __declspec(dllexport) int pillow_c_image_get_extrema(
    const PillowCImage* image,
    std::uint8_t* out_min,
    std::uint8_t* out_max,
    std::uint8_t* out_has_value,
    std::size_t out_count)
{
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return extrema_image(image, out_min, out_max, out_has_value, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_get_extrema_numeric(
    const PillowCImage* image,
    double* out_min,
    double* out_max,
    std::uint8_t* out_has_value,
    std::size_t out_count)
{
    return extrema_image_numeric(image, out_min, out_max, out_has_value, out_count);
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
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return getbbox_image(image, alpha_only != 0, out_left, out_top, out_right, out_bottom, out_has_bbox);
}

extern "C" __declspec(dllexport) int pillow_c_image_getprojection(
    const PillowCImage* image,
    std::uint8_t* out_x_projection,
    std::size_t out_x_count,
    std::uint8_t* out_y_projection,
    std::size_t out_y_count)
{
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
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
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return getcolors_image(image, maxcolors, out_counts, out_colors, out_capacity, out_count, out_exceeded);
}

extern "C" __declspec(dllexport) int pillow_c_image_getcolors_numeric(
    const PillowCImage* image,
    int maxcolors,
    std::uint64_t* out_counts,
    double* out_values,
    std::size_t out_capacity,
    std::size_t* out_count,
    int* out_exceeded)
{
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return getcolors_image_numeric(image, maxcolors, out_counts, out_values, out_capacity, out_count, out_exceeded);
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


