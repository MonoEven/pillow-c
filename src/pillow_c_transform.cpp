#include "pillow_c_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace {
constexpr int PILLOW_C_RESAMPLE_NEAREST = 0;
constexpr int PILLOW_C_RESAMPLE_LANCZOS = 1;
constexpr int PILLOW_C_RESAMPLE_BILINEAR = 2;
constexpr int PILLOW_C_RESAMPLE_BICUBIC = 3;
constexpr int PILLOW_C_RESAMPLE_BOX = 4;
constexpr int PILLOW_C_RESAMPLE_HAMMING = 5;
constexpr double PILLOW_C_PI = 3.1415926535897932384626433832795;
constexpr double PILLOW_C_SQRT2 = 1.4142135623730950488;
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
        return pillow_c_transpose_output_shape(source, method, out_width, out_height) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
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

bool is_premultiplied_alpha_mode(const PillowCImage* source)
{
    return source &&
           (source->mode == PILLOW_C_MODE_LA ||
            source->mode == PILLOW_C_MODE_RGBA);
}

int alpha_channel_index(const PillowCImage* source)
{
    return source ? source->channels - 1 : 0;
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
    const int alpha_channel = alpha_channel_index(source);
    if (premultiply_alpha && is_premultiplied_alpha_mode(source) && channel < alpha_channel) {
        return pillow_c_mul_div_255(px[channel], px[alpha_channel]);
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
        const int alpha_channel = alpha_channel_index(source);
        if (premultiply_alpha && is_premultiplied_alpha_mode(source) && channel < alpha_channel) {
            return pillow_c_mul_div_255(px[channel], px[alpha_channel]);
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

    return pillow_c_clip_u8_double(bicubic_interpolate(v1, v2, v3, v4, dy));
}

void write_transform_values(const PillowCImage* source, const std::uint8_t* values, std::uint8_t* dst)
{
    if (is_premultiplied_alpha_mode(source)) {
        const int alpha_channel = alpha_channel_index(source);
        const std::uint8_t alpha = values[alpha_channel];
        for (int channel = 0; channel < alpha_channel; ++channel) {
            dst[channel] = (alpha == 0 || alpha == 255)
                ? values[channel]
                : pillow_c_clip_u8_int(255 * static_cast<int>(values[channel]) / alpha);
        }
        dst[alpha_channel] = alpha;
    } else {
        for (int channel = 0; channel < source->channels; ++channel) {
            dst[channel] = values[channel];
        }
    }
}

bool is_numeric_transform_mode(const PillowCImage* source)
{
    return source &&
           (source->mode == PILLOW_C_MODE_I || source->mode == PILLOW_C_MODE_F);
}

bool transform_resample_unsupported_for_mode(const PillowCImage* source, int resample)
{
    return source &&
           resample != PILLOW_C_RESAMPLE_NEAREST &&
           (source->mode == PILLOW_C_MODE_I16 || source->mode == PILLOW_C_MODE_I16B);
}

double transform_numeric_sample(const PillowCImage* source, int x, int y)
{
    const std::uint8_t* px =
        source->pixels.data() +
        static_cast<std::size_t>(y) * source->stride +
        static_cast<std::size_t>(x) * 4u;
    return source->mode == PILLOW_C_MODE_I
        ? static_cast<double>(read_le_i32(px))
        : static_cast<double>(pillow_c_read_f32_le(px));
}

void write_transform_numeric_sample(const PillowCImage* source, double value, std::uint8_t* dst)
{
    if (source->mode == PILLOW_C_MODE_I) {
        pillow_c_write_i32_le(dst, static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
    } else {
        pillow_c_write_f32_le(dst, static_cast<float>(value));
    }
}

double bilinear_transform_numeric_sample(
    const PillowCImage* source,
    double source_x,
    double source_y)
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
    const double top_left = transform_numeric_sample(source, x0, y0);
    const double top_right = transform_numeric_sample(source, x1, y0);
    const double bottom_left = transform_numeric_sample(source, x0, y1);
    const double bottom_right = transform_numeric_sample(source, x1, y1);
    const double v1 = top_left + (top_right - top_left) * dx;
    const double v2 = bottom_left + (bottom_right - bottom_left) * dx;
    return v1 + (v2 - v1) * dy;
}

double bicubic_transform_numeric_sample(
    const PillowCImage* source,
    double source_x,
    double source_y)
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
    const auto horizontal_value = [source, x0, x1, x2, x3, dx](int row_y) -> double {
        return bicubic_interpolate(
            transform_numeric_sample(source, x0, row_y),
            transform_numeric_sample(source, x1, row_y),
            transform_numeric_sample(source, x2, row_y),
            transform_numeric_sample(source, x3, row_y),
            dx);
    };
    const double v1 = horizontal_value(clamp_index(y, source->height));
    const double v2 = y + 1 >= 0 && y + 1 < source->height ? horizontal_value(y + 1) : v1;
    const double v3 = y + 2 >= 0 && y + 2 < source->height ? horizontal_value(y + 2) : v2;
    const double v4 = y + 3 >= 0 && y + 3 < source->height ? horizontal_value(y + 3) : v3;
    return bicubic_interpolate(v1, v2, v3, v4, dy);
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
        return pillow_c_copy_transpose_pixels_into(source, method, target);
    }
    if (!has_center && !has_translate && normalized_angle == 0.0) {
        if (!pillow_c_image_shape_matches(target, source)) {
            return PILLOW_C_MISMATCH;
        }
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        pillow_c_copy_palette_if_same_mode(source, target);
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
    if (!pillow_c_image_shape_matches(target, geometry.width, geometry.height, source->mode, source->channels)) {
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
    if (transform_resample_unsupported_for_mode(source, PILLOW_C_RESAMPLE_BILINEAR)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    double normalized_angle = 0.0;
    int status = normalize_angle_degrees(angle, &normalized_angle);
    if (status != PILLOW_C_OK) {
        return status;
    }
    int method = 0;
    if (rotate_fast_path_method(normalized_angle, expand, has_center, has_translate, source, &method)) {
        return pillow_c_copy_transpose_pixels_into(source, method, target);
    }
    if (!has_center && !has_translate && normalized_angle == 0.0) {
        if (!pillow_c_image_shape_matches(target, source)) {
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
    if (!pillow_c_image_shape_matches(target, geometry.width, geometry.height, source->mode, source->channels)) {
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
            if (is_numeric_transform_mode(source)) {
                const double value = bilinear_transform_numeric_sample(source, source_x, source_y);
                write_transform_numeric_sample(source, value, dst);
                continue;
            }
            std::uint8_t values[4]{0, 0, 0, 0};
            for (int channel = 0; channel < source->channels; ++channel) {
                values[channel] = bilinear_transform_channel(source, source_x, source_y, channel, is_premultiplied_alpha_mode(source));
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
    if (transform_resample_unsupported_for_mode(source, PILLOW_C_RESAMPLE_BICUBIC)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    double normalized_angle = 0.0;
    int status = normalize_angle_degrees(angle, &normalized_angle);
    if (status != PILLOW_C_OK) {
        return status;
    }
    int method = 0;
    if (rotate_fast_path_method(normalized_angle, expand, has_center, has_translate, source, &method)) {
        return pillow_c_copy_transpose_pixels_into(source, method, target);
    }
    if (!has_center && !has_translate && normalized_angle == 0.0) {
        if (!pillow_c_image_shape_matches(target, source)) {
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
    if (!pillow_c_image_shape_matches(target, geometry.width, geometry.height, source->mode, source->channels)) {
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
            if (is_numeric_transform_mode(source)) {
                const double value = bicubic_transform_numeric_sample(source, source_x, source_y);
                write_transform_numeric_sample(source, value, dst);
                continue;
            }
            std::uint8_t values[4]{0, 0, 0, 0};
            for (int channel = 0; channel < source->channels; ++channel) {
                values[channel] = bicubic_transform_channel(source, source_x, source_y, channel, is_premultiplied_alpha_mode(source));
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
    int status = PILLOW_C_INVALID_ARGUMENT;
    if (resample == PILLOW_C_RESAMPLE_NEAREST) {
        status = rotate_nearest_into(
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
    } else if (resample == PILLOW_C_RESAMPLE_BILINEAR) {
        status = rotate_bilinear_into(
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
    } else if (resample == PILLOW_C_RESAMPLE_BICUBIC) {
        status = rotate_bicubic_into(
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
    if (status == PILLOW_C_OK) {
        pillow_c_copy_palette_if_same_mode(source, target);
    }
    return status;
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
    if (transform_resample_unsupported_for_mode(source, resample)) {
        // Pillow 11.3.0's bilinear/bicubic transform interpolates I;16
          // storage bytes as byte channels (endian-bug garbage for I;16B),
          // so those are explicit documented boundaries instead of
          // replicated garbage. NEAREST whole-copies samples.
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    std::uint8_t fill[4]{0, 0, 0, 0};
    int status = normalize_transform_fill(source, fill_color, fill_color_size, fill);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        pillow_c_copy_palette_if_same_mode(source, target);
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
            if (is_numeric_transform_mode(source)) {
                const double value = resample == PILLOW_C_RESAMPLE_BILINEAR
                    ? bilinear_transform_numeric_sample(source, source_x, source_y)
                    : bicubic_transform_numeric_sample(source, source_x, source_y);
                write_transform_numeric_sample(source, value, dst);
                continue;
            }
            std::uint8_t values[4]{0, 0, 0, 0};
            for (int channel = 0; channel < source->channels; ++channel) {
                values[channel] = resample == PILLOW_C_RESAMPLE_BILINEAR
                    ? bilinear_transform_channel(source, source_x, source_y, channel, is_premultiplied_alpha_mode(source))
                    : bicubic_transform_channel(source, source_x, source_y, channel, is_premultiplied_alpha_mode(source));
            }
            write_transform_values(source, values, dst);
        }
    }
    pillow_c_copy_palette_if_same_mode(source, target);
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
    if (transform_resample_unsupported_for_mode(source, resample)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!pillow_c_image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
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
                if (is_numeric_transform_mode(source)) {
                    const double value = resample == PILLOW_C_RESAMPLE_BILINEAR
                        ? bilinear_transform_numeric_sample(source, source_x, source_y)
                        : bicubic_transform_numeric_sample(source, source_x, source_y);
                    write_transform_numeric_sample(source, value, dst);
                    continue;
                }
                std::uint8_t values[4]{0, 0, 0, 0};
                for (int channel = 0; channel < source->channels; ++channel) {
                    values[channel] = resample == PILLOW_C_RESAMPLE_BILINEAR
                        ? bilinear_transform_channel(source, source_x, source_y, channel, is_premultiplied_alpha_mode(source))
                        : bicubic_transform_channel(source, source_x, source_y, channel, is_premultiplied_alpha_mode(source));
                }
                write_transform_values(source, values, dst);
            }
        }
    }
    pillow_c_copy_palette_if_same_mode(source, target);
    return PILLOW_C_OK;
}

} // namespace
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
