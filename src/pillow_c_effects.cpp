#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <new>
#include <vector>

#include "pillow_c_internal.h"

namespace {
constexpr int PILLOW_C_GRADIENT_SIZE = 256;

bool supports_gradient_mode(int mode)
{
    return mode == PILLOW_C_MODE_1 || mode == PILLOW_C_MODE_L || mode == PILLOW_C_MODE_P ||
           mode == PILLOW_C_MODE_I || mode == PILLOW_C_MODE_F;
}

void write_gradient_sample(PillowCImage* target, int x, int y, int value)
{
    std::uint8_t* row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
    switch (target->mode) {
    case PILLOW_C_MODE_I:
        pillow_c_write_i32_le(row + static_cast<std::size_t>(x) * 4u, static_cast<std::uint32_t>(value));
        return;
    case PILLOW_C_MODE_F:
        pillow_c_write_f32_le(row + static_cast<std::size_t>(x) * 4u, static_cast<float>(value));
        return;
    default:
        row[x] = static_cast<std::uint8_t>(value);
        return;
    }
}

int linear_gradient_image_into(int mode, PillowCImage* target)
{
    if (!target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!supports_gradient_mode(mode)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, PILLOW_C_GRADIENT_SIZE, PILLOW_C_GRADIENT_SIZE, mode, channels_for_mode(mode))) {
        return PILLOW_C_MISMATCH;
    }

    // Pillow 11.3.0 Fill.c: linear gradient value is y (top to bottom),
    // stored per mode (byte for 1/L/P, int32 for I, float32 for F).
    for (int y = 0; y < PILLOW_C_GRADIENT_SIZE; ++y) {
        for (int x = 0; x < PILLOW_C_GRADIENT_SIZE; ++x) {
            write_gradient_sample(target, x, y, y);
        }
    }
    target->palette_rgb.clear();
    target->palette_alpha.clear();
    target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
    return PILLOW_C_OK;
}

int radial_gradient_image_into(int mode, PillowCImage* target)
{
    if (!target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!supports_gradient_mode(mode)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, PILLOW_C_GRADIENT_SIZE, PILLOW_C_GRADIENT_SIZE, mode, channels_for_mode(mode))) {
        return PILLOW_C_MISMATCH;
    }

    for (int y = 0; y < PILLOW_C_GRADIENT_SIZE; ++y) {
        const double dy = static_cast<double>(y - 128);
        for (int x = 0; x < PILLOW_C_GRADIENT_SIZE; ++x) {
            const double dx = static_cast<double>(x - 128);
            int value = static_cast<int>(std::sqrt((dx * dx + dy * dy) * 2.0));
            if (value >= 255) {
                value = 255;
            }
            write_gradient_sample(target, x, y, value);
        }
    }
    target->palette_rgb.clear();
    target->palette_alpha.clear();
    target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
    return PILLOW_C_OK;
}

int effect_mandelbrot_image(int width, int height, const double* extent, int quality, PillowCImage** out_image)
{
    if (!extent || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    const double extent_width = extent[2] - extent[0];
    const double extent_height = extent[3] - extent[1];
    if (extent_width < 0.0 || extent_height < 0.0 || quality < 2) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(width, height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_L,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        if (width == 0 || height == 0) {
            *out_image = image;
            return PILLOW_C_OK;
        }

        const double dr = extent_width / static_cast<double>(width - 1);
        const double di = extent_height / static_cast<double>(height - 1);
        constexpr double radius = 100.0;

        for (int y = 0; y < height; ++y) {
            std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            for (int x = 0; x < width; ++x) {
                double x1 = 0.0;
                double y1 = 0.0;
                double xi2 = 0.0;
                double yi2 = 0.0;
                const double cr = static_cast<double>(x) * dr + extent[0];
                const double ci = static_cast<double>(y) * di + extent[1];

                for (int k = 1;; ++k) {
                    y1 = 2.0 * x1 * y1 + ci;
                    x1 = xi2 - yi2 + cr;
                    xi2 = x1 * x1;
                    yi2 = y1 * y1;
                    if ((xi2 + yi2) > radius) {
                        row[x] = static_cast<std::uint8_t>(k * 255 / quality);
                        break;
                    }
                    if (k > quality) {
                        row[x] = 0;
                        break;
                    }
                }
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int effect_noise_image(int width, int height, double sigma, PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(width, height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_L,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        if (width == 0 || height == 0) {
            *out_image = image;
            return PILLOW_C_OK;
        }

        const float sigma_f = static_cast<float>(sigma);
        for (int y = 0; y < height; ++y) {
            std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            for (int x = 0; x < width; ++x) {
                double v1 = 0.0;
                double v2 = 0.0;
                double radius = 0.0;
                do {
                    v1 = std::rand() * (2.0 / RAND_MAX) - 1.0;
                    v2 = std::rand() * (2.0 / RAND_MAX) - 1.0;
                    radius = v1 * v1 + v2 * v2;
                } while (radius >= 1.0);

                const double factor = std::sqrt(-2.0 * std::log(radius) / radius);
                row[x] = pillow_c_clip_u8_double(128.0 + static_cast<double>(sigma_f) * factor * v1);
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int effect_spread_image(const PillowCImage* source, int distance, PillowCImage** out_image)
{
    if (!source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (distance < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

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
            std::vector<std::uint8_t>(size),
            source->palette_rgb};
        image->palette_alpha = source->palette_alpha;
        image->palette_alpha_mode = source->palette_alpha_mode;
        if (source->width == 0 || source->height == 0 || distance == 0) {
            image->pixels = source->pixels;
            *out_image = image;
            return PILLOW_C_OK;
        }

        const int half_distance = distance / 2;
        for (int y = 0; y < source->height; ++y) {
            const std::size_t row_offset = static_cast<std::size_t>(y) * source->stride;
            for (int x = 0; x < source->width; ++x) {
                const int xx = x + (std::rand() % distance) - half_distance;
                const int yy = y + (std::rand() % distance) - half_distance;
                const std::size_t source_offset = row_offset + static_cast<std::size_t>(x) * source->channels;
                if (xx >= 0 && xx < source->width && yy >= 0 && yy < source->height) {
                    const std::size_t target_offset =
                        static_cast<std::size_t>(yy) * image->stride + static_cast<std::size_t>(xx) * image->channels;
                    for (int channel = 0; channel < source->channels; ++channel) {
                        image->pixels[target_offset + static_cast<std::size_t>(channel)] =
                            source->pixels[source_offset + static_cast<std::size_t>(channel)];
                    }
                    for (int channel = 0; channel < source->channels; ++channel) {
                        image->pixels[source_offset + static_cast<std::size_t>(channel)] =
                            source->pixels[target_offset + static_cast<std::size_t>(channel)];
                    }
                } else {
                    for (int channel = 0; channel < source->channels; ++channel) {
                        image->pixels[source_offset + static_cast<std::size_t>(channel)] =
                            source->pixels[source_offset + static_cast<std::size_t>(channel)];
                    }
                }
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

} // namespace

extern "C" __declspec(dllexport) int pillow_c_image_linear_gradient_into(
    int mode,
    PillowCImage* target)
{
    return linear_gradient_image_into(mode, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_linear_gradient(
    int mode,
    PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supports_gradient_mode(mode)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(PILLOW_C_GRADIENT_SIZE, PILLOW_C_GRADIENT_SIZE, channels_for_mode(mode), &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            PILLOW_C_GRADIENT_SIZE,
            PILLOW_C_GRADIENT_SIZE,
            mode,
            channels_for_mode(mode),
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = linear_gradient_image_into(mode, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_radial_gradient_into(
    int mode,
    PillowCImage* target)
{
    return radial_gradient_image_into(mode, target);
}

extern "C" __declspec(dllexport) int pillow_c_image_radial_gradient(
    int mode,
    PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!supports_gradient_mode(mode)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(PILLOW_C_GRADIENT_SIZE, PILLOW_C_GRADIENT_SIZE, channels_for_mode(mode), &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            PILLOW_C_GRADIENT_SIZE,
            PILLOW_C_GRADIENT_SIZE,
            mode,
            channels_for_mode(mode),
            stride,
            std::vector<std::uint8_t>(size)};
        const int status = radial_gradient_image_into(mode, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_effect_mandelbrot(
    int width,
    int height,
    const double* extent,
    int quality,
    PillowCImage** out_image)
{
    return effect_mandelbrot_image(width, height, extent, quality, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_effect_noise(
    int width,
    int height,
    double sigma,
    PillowCImage** out_image)
{
    return effect_noise_image(width, height, sigma, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_effect_spread(
    const PillowCImage* source,
    int distance,
    PillowCImage** out_image)
{
    return effect_spread_image(source, distance, out_image);
}
