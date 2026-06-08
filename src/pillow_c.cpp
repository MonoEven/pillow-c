#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>

namespace {

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

constexpr int PILLOW_C_RESAMPLE_NEAREST = 0;
constexpr int PILLOW_C_RESAMPLE_LANCZOS = 1;
constexpr int PILLOW_C_RESAMPLE_BILINEAR = 2;
constexpr int PILLOW_C_RESAMPLE_BICUBIC = 3;
constexpr int PILLOW_C_RESAMPLE_BOX = 4;
constexpr int PILLOW_C_RESAMPLE_HAMMING = 5;

constexpr int PILLOW_C_GRADIENT_SIZE = 256;
constexpr double PILLOW_C_PI = 3.1415926535897932384626433832795;
constexpr double PILLOW_C_SQRT2 = 1.4142135623730950488;

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
    std::vector<std::uint8_t> palette_rgb;
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

struct PolygonEdge {
    int x0;
    int y0;
    int xmin;
    int ymin;
    int xmax;
    int ymax;
    float dx;
};

struct QuarterState {
    std::int32_t a;
    std::int32_t b;
    std::int32_t cx;
    std::int32_t cy;
    std::int32_t ex;
    std::int32_t ey;
    std::int64_t a2;
    std::int64_t b2;
    std::int64_t a2b2;
    bool finished;
};

struct EllipseState {
    QuarterState outer;
    QuarterState inner;
    std::int32_t py;
    std::int32_t pl;
    std::int32_t pr;
    std::int32_t cy[4];
    std::int32_t cl[4];
    std::int32_t cr[4];
    int bufcnt;
    bool finished;
    bool leftmost;
};

enum class ClipNodeType {
    And,
    Or,
    Clip,
};

struct ClipNode {
    ClipNodeType type = ClipNodeType::Clip;
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    int left = -1;
    int right = -1;
};

struct ClipEvent {
    std::int32_t x;
    int type;
};

struct ClipEllipseState {
    EllipseState ellipse;
    int root = -1;
    ClipNode nodes[7];
    int node_count = 0;
    std::vector<ClipEvent> events;
    std::int32_t y = 0;
};

enum class RawCodecKind {
    Unsupported,
    One,
    P,
    L,
    LA,
    RGB,
    RGBX,
    BGR,
    BGRX,
    XBGR,
    RGBA,
    BGRA,
    ARGB,
    ABGR,
    CMYK,
};

struct RawCodecSpec {
    RawCodecKind kind;
    int bytes_per_pixel;
};

template <typename T>
struct ComPtr {
    T* ptr = nullptr;

    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ~ComPtr()
    {
        reset();
    }

    T** put()
    {
        reset();
        return &ptr;
    }

    void reset(T* value = nullptr)
    {
        if (ptr) {
            ptr->Release();
        }
        ptr = value;
    }

    T* get() const
    {
        return ptr;
    }

    T* operator->() const
    {
        return ptr;
    }
};

struct ComInitScope {
    HRESULT hr;
    bool initialized;

    ComInitScope()
        : hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED)),
          initialized(SUCCEEDED(hr))
    {
    }

    ~ComInitScope()
    {
        if (initialized) {
            CoUninitialize();
        }
    }

    bool usable() const
    {
        return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
};

inline std::uint32_t shift_for_div255(std::uint32_t value)
{
    return (((value >> 8) + value) >> 8);
}

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
        shift_for_div255(tmpr + (0x80u << precision_bits)) >> precision_bits);
    out[1] = static_cast<std::uint8_t>(
        shift_for_div255(tmpg + (0x80u << precision_bits)) >> precision_bits);
    out[2] = static_cast<std::uint8_t>(
        shift_for_div255(tmpb + (0x80u << precision_bits)) >> precision_bits);
    out[3] = static_cast<std::uint8_t>(shift_for_div255(outa255 + 0x80u));
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
    rgb[0] = mul_div_255(static_cast<std::uint8_t>(255u - cmyk[0]), inverse_k);
    rgb[1] = mul_div_255(static_cast<std::uint8_t>(255u - cmyk[1]), inverse_k);
    rgb[2] = mul_div_255(static_cast<std::uint8_t>(255u - cmyk[2]), inverse_k);
}

inline std::uint8_t cmyk_luma_u8(const std::uint8_t* cmyk)
{
    std::uint8_t rgb[3];
    cmyk_to_rgb_u8(cmyk, rgb);
    return rgb_luma_u8(rgb);
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
    case PILLOW_C_MODE_1:
        return 1;
    case PILLOW_C_MODE_P:
        return 1;
    case PILLOW_C_MODE_L:
        return 1;
    case PILLOW_C_MODE_LA:
        return 2;
    case PILLOW_C_MODE_RGB:
        return 3;
    case PILLOW_C_MODE_RGBA:
        return 4;
    case PILLOW_C_MODE_CMYK:
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

const char* mode_name(int mode)
{
    switch (mode) {
    case PILLOW_C_MODE_1:
        return "1";
    case PILLOW_C_MODE_P:
        return "P";
    case PILLOW_C_MODE_L:
        return "L";
    case PILLOW_C_MODE_LA:
        return "LA";
    case PILLOW_C_MODE_RGB:
        return "RGB";
    case PILLOW_C_MODE_RGBA:
        return "RGBA";
    case PILLOW_C_MODE_CMYK:
        return "CMYK";
    default:
        return nullptr;
    }
}

RawCodecSpec raw_decode_spec(int target_mode, const char* raw_mode)
{
    if (!raw_mode) {
        return {RawCodecKind::Unsupported, 0};
    }

    switch (target_mode) {
    case PILLOW_C_MODE_1:
        if (std::strcmp(raw_mode, "1") == 0) {
            return {RawCodecKind::One, 0};
        }
        break;
    case PILLOW_C_MODE_P:
        if (std::strcmp(raw_mode, "P") == 0) {
            return {RawCodecKind::P, 1};
        }
        break;
    case PILLOW_C_MODE_L:
        if (std::strcmp(raw_mode, "L") == 0) {
            return {RawCodecKind::L, 1};
        }
        break;
    case PILLOW_C_MODE_LA:
        if (std::strcmp(raw_mode, "LA") == 0) {
            return {RawCodecKind::LA, 2};
        }
        break;
    case PILLOW_C_MODE_RGB:
        if (std::strcmp(raw_mode, "RGB") == 0) {
            return {RawCodecKind::RGB, 3};
        }
        if (std::strcmp(raw_mode, "RGBX") == 0) {
            return {RawCodecKind::RGBX, 4};
        }
        if (std::strcmp(raw_mode, "BGR") == 0) {
            return {RawCodecKind::BGR, 3};
        }
        if (std::strcmp(raw_mode, "BGRX") == 0) {
            return {RawCodecKind::BGRX, 4};
        }
        if (std::strcmp(raw_mode, "XBGR") == 0) {
            return {RawCodecKind::XBGR, 4};
        }
        break;
    case PILLOW_C_MODE_RGBA:
        if (std::strcmp(raw_mode, "RGBA") == 0) {
            return {RawCodecKind::RGBA, 4};
        }
        if (std::strcmp(raw_mode, "BGRA") == 0) {
            return {RawCodecKind::BGRA, 4};
        }
        if (std::strcmp(raw_mode, "ARGB") == 0) {
            return {RawCodecKind::ARGB, 4};
        }
        if (std::strcmp(raw_mode, "ABGR") == 0) {
            return {RawCodecKind::ABGR, 4};
        }
        if (std::strcmp(raw_mode, "BGR") == 0) {
            return {RawCodecKind::BGR, 3};
        }
        break;
    case PILLOW_C_MODE_CMYK:
        if (std::strcmp(raw_mode, "CMYK") == 0) {
            return {RawCodecKind::CMYK, 4};
        }
        break;
    default:
        break;
    }

    return {RawCodecKind::Unsupported, 0};
}

RawCodecSpec raw_encode_spec(int source_mode, const char* raw_mode)
{
    if (!raw_mode) {
        return {RawCodecKind::Unsupported, 0};
    }

    switch (source_mode) {
    case PILLOW_C_MODE_1:
        if (std::strcmp(raw_mode, "1") == 0) {
            return {RawCodecKind::One, 0};
        }
        break;
    case PILLOW_C_MODE_P:
        if (std::strcmp(raw_mode, "P") == 0) {
            return {RawCodecKind::P, 1};
        }
        break;
    case PILLOW_C_MODE_L:
        if (std::strcmp(raw_mode, "L") == 0) {
            return {RawCodecKind::L, 1};
        }
        break;
    case PILLOW_C_MODE_LA:
        if (std::strcmp(raw_mode, "LA") == 0) {
            return {RawCodecKind::LA, 2};
        }
        break;
    case PILLOW_C_MODE_RGB:
        if (std::strcmp(raw_mode, "RGB") == 0) {
            return {RawCodecKind::RGB, 3};
        }
        if (std::strcmp(raw_mode, "BGR") == 0) {
            return {RawCodecKind::BGR, 3};
        }
        if (std::strcmp(raw_mode, "RGBX") == 0 || std::strcmp(raw_mode, "RGBA") == 0) {
            return {RawCodecKind::RGBX, 4};
        }
        if (std::strcmp(raw_mode, "BGRX") == 0) {
            return {RawCodecKind::BGRX, 4};
        }
        if (std::strcmp(raw_mode, "XBGR") == 0) {
            return {RawCodecKind::XBGR, 4};
        }
        break;
    case PILLOW_C_MODE_RGBA:
        if (std::strcmp(raw_mode, "RGBA") == 0) {
            return {RawCodecKind::RGBA, 4};
        }
        if (std::strcmp(raw_mode, "BGRA") == 0) {
            return {RawCodecKind::BGRA, 4};
        }
        if (std::strcmp(raw_mode, "ABGR") == 0) {
            return {RawCodecKind::ABGR, 4};
        }
        if (std::strcmp(raw_mode, "RGB") == 0) {
            return {RawCodecKind::RGB, 3};
        }
        if (std::strcmp(raw_mode, "BGR") == 0) {
            return {RawCodecKind::BGR, 3};
        }
        break;
    case PILLOW_C_MODE_CMYK:
        if (std::strcmp(raw_mode, "CMYK") == 0) {
            return {RawCodecKind::CMYK, 4};
        }
        break;
    default:
        break;
    }

    return {RawCodecKind::Unsupported, 0};
}

bool checked_raw_output_size(const PillowCImage* image, int bytes_per_pixel, std::size_t* out_size)
{
    if (!image || bytes_per_pixel <= 0 || !out_size) {
        return false;
    }
    const std::size_t pixels = static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height);
    if (bytes_per_pixel != 0 && pixels > static_cast<std::size_t>(-1) / static_cast<std::size_t>(bytes_per_pixel)) {
        return false;
    }
    *out_size = pixels * static_cast<std::size_t>(bytes_per_pixel);
    return true;
}

bool checked_mode1_raw_size(const PillowCImage* image, std::size_t* row_bytes, std::size_t* out_size)
{
    if (!image || !row_bytes || !out_size || image->width < 0 || image->height < 0) {
        return false;
    }
    const std::size_t row = (static_cast<std::size_t>(image->width) + 7u) / 8u;
    const std::size_t height = static_cast<std::size_t>(image->height);
    if (row != 0 && height > static_cast<std::size_t>(-1) / row) {
        return false;
    }
    *row_bytes = row;
    *out_size = row * height;
    return true;
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
    }
}

bool supports_gradient_mode(int mode)
{
    return mode == PILLOW_C_MODE_1 || mode == PILLOW_C_MODE_L || mode == PILLOW_C_MODE_P;
}

std::uint16_t read_le16(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8);
}

std::uint32_t read_le32(const std::uint8_t* data)
{
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

std::int32_t read_le_i32(const std::uint8_t* data)
{
    return static_cast<std::int32_t>(read_le32(data));
}

std::uint32_t read_be32(const std::uint8_t* data)
{
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

std::uint16_t read_be16(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) |
                                      static_cast<std::uint16_t>(data[1]));
}

void append_le16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

void append_le32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

void append_be32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

bool utf8_path_to_wide(const char* path, std::vector<wchar_t>* out)
{
    if (!path || !out) {
        return false;
    }
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
    if (required <= 0) {
        return false;
    }
    out->assign(static_cast<std::size_t>(required), wchar_t{0});
    const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, out->data(), required);
    return written == required;
}

bool read_binary_file(const char* path, std::vector<std::uint8_t>* out)
{
    if (!out) {
        return false;
    }
    std::vector<wchar_t> wide_path;
    if (!utf8_path_to_wide(path, &wide_path)) {
        return false;
    }
    std::FILE* file = nullptr;
    if (_wfopen_s(&file, wide_path.data(), L"rb") != 0 || !file) {
        return false;
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    const long length = std::ftell(file);
    if (length < 0) {
        std::fclose(file);
        return false;
    }
    if (std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    try {
        out->assign(static_cast<std::size_t>(length), std::uint8_t{0});
    } catch (const std::bad_alloc&) {
        std::fclose(file);
        throw;
    }
    if (!out->empty()) {
        const std::size_t read_count = std::fread(out->data(), 1, out->size(), file);
        if (read_count != out->size()) {
            std::fclose(file);
            return false;
        }
    }
    std::fclose(file);
    return true;
}

struct PngHeaderInfo {
    int bit_depth;
    int color_type;
};

bool read_png_header_info(const char* path, PngHeaderInfo* info)
{
    if (!path || !info) {
        return false;
    }
    std::vector<wchar_t> wide_path;
    if (!utf8_path_to_wide(path, &wide_path)) {
        return false;
    }
    std::FILE* file = nullptr;
    if (_wfopen_s(&file, wide_path.data(), L"rb") != 0 || !file) {
        return false;
    }
    std::uint8_t header[33] = {};
    const std::size_t read_count = std::fread(header, 1, sizeof(header), file);
    std::fclose(file);
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (read_count != sizeof(header) || std::memcmp(header, signature, sizeof(signature)) != 0 ||
        read_be32(header + 8) != 13u || std::memcmp(header + 12, "IHDR", 4) != 0) {
        return false;
    }
    info->bit_depth = header[24];
    info->color_type = header[25];
    return true;
}

bool jpeg_is_sof_marker(std::uint8_t marker)
{
    return (marker >= 0xc0u && marker <= 0xcfu) &&
           marker != 0xc4u &&
           marker != 0xc8u &&
           marker != 0xccu;
}

bool read_jpeg_component_count(const char* path, int* components)
{
    if (!path || !components) {
        return false;
    }
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    if (data.size() < 4 || data[0] != 0xffu || data[1] != 0xd8u) {
        return false;
    }

    std::size_t offset = 2u;
    while (offset < data.size()) {
        while (offset < data.size() && data[offset] == 0xffu) {
            ++offset;
        }
        if (offset >= data.size()) {
            return false;
        }
        const std::uint8_t marker = data[offset++];
        if (marker == 0xd9u || marker == 0xdau) {
            break;
        }
        if (marker == 0x01u || (marker >= 0xd0u && marker <= 0xd7u)) {
            continue;
        }
        if (offset + 2u > data.size()) {
            return false;
        }
        const std::uint16_t segment_length = read_be16(data.data() + offset);
        if (segment_length < 2u || offset + segment_length > data.size()) {
            return false;
        }
        if (jpeg_is_sof_marker(marker)) {
            if (segment_length < 8u) {
                return false;
            }
            *components = data[offset + 7u];
            return *components > 0;
        }
        offset += segment_length;
    }
    return false;
}

bool write_binary_file(const char* path, const std::vector<std::uint8_t>& data)
{
    std::vector<wchar_t> wide_path;
    if (!utf8_path_to_wide(path, &wide_path)) {
        return false;
    }
    std::FILE* file = nullptr;
    if (_wfopen_s(&file, wide_path.data(), L"wb") != 0 || !file) {
        return false;
    }
    if (!data.empty()) {
        const std::size_t written = std::fwrite(data.data(), 1, data.size(), file);
        if (written != data.size()) {
            std::fclose(file);
            return false;
        }
    }
    const bool ok = std::fclose(file) == 0;
    return ok;
}

int bmp_row_stride(int width, int bits_per_pixel, std::size_t* out_stride)
{
    if (width <= 0 || bits_per_pixel <= 0 || !out_stride) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint64_t bits = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(bits_per_pixel);
    const std::uint64_t stride = ((bits + 31u) / 32u) * 4u;
    if (stride > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_stride = static_cast<std::size_t>(stride);
    return PILLOW_C_OK;
}

int open_bmp_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 54 || data[0] != 'B' || data[1] != 'M') {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const std::uint32_t file_size = read_le32(data.data() + 2);
        const std::uint32_t pixel_offset = read_le32(data.data() + 10);
        const std::uint32_t dib_size = read_le32(data.data() + 14);
        if (dib_size < 40 || pixel_offset > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (file_size != 0 && file_size > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (14u + dib_size > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const std::int32_t width_i32 = read_le_i32(data.data() + 18);
        const std::int32_t height_i32 = read_le_i32(data.data() + 22);
        const std::uint16_t planes = read_le16(data.data() + 26);
        const std::uint16_t bits_per_pixel = read_le16(data.data() + 28);
        const std::uint32_t compression = read_le32(data.data() + 30);
        const std::uint32_t colors_used = read_le32(data.data() + 46);
        if (width_i32 <= 0 || height_i32 == 0 || planes != 1 || compression != 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const bool top_down = height_i32 < 0;
        const int width = width_i32;
        const int height = top_down ? -height_i32 : height_i32;
        if (height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        int mode = 0;
        int channels = 0;
        if (bits_per_pixel == 8) {
            mode = PILLOW_C_MODE_L;
            channels = 1;
        } else if (bits_per_pixel == 24 || bits_per_pixel == 32) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
        } else {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::size_t source_stride = 0;
        int status = bmp_row_stride(width, bits_per_pixel, &source_stride);
        if (status != PILLOW_C_OK) {
            return status;
        }
        std::size_t target_stride = 0;
        std::size_t target_size = 0;
        if (!checked_image_size(width, height, channels, &target_stride, &target_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint64_t required_end =
            static_cast<std::uint64_t>(pixel_offset) +
            static_cast<std::uint64_t>(source_stride) * static_cast<std::uint64_t>(height);
        if (required_end > data.size()) {
            return PILLOW_C_INVALID_LENGTH;
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            target_stride,
            std::vector<std::uint8_t>(target_size)};

        if (bits_per_pixel == 8) {
            const std::uint32_t palette_entries =
                colors_used != 0 ? colors_used : ((pixel_offset > 14u + dib_size) ? (pixel_offset - 14u - dib_size) / 4u : 0u);
            if (palette_entries > 0) {
                bool grayscale = true;
                const std::size_t palette_offset = static_cast<std::size_t>(14u + dib_size);
                if (palette_offset + static_cast<std::size_t>(palette_entries) * 4u > data.size()) {
                    delete image;
                    return PILLOW_C_INVALID_LENGTH;
                }
                for (std::uint32_t i = 0; i < palette_entries; ++i) {
                    const std::uint8_t b = data[palette_offset + static_cast<std::size_t>(i) * 4u + 0u];
                    const std::uint8_t g = data[palette_offset + static_cast<std::size_t>(i) * 4u + 1u];
                    const std::uint8_t r = data[palette_offset + static_cast<std::size_t>(i) * 4u + 2u];
                    if (r != g || g != b || r != static_cast<std::uint8_t>(i & 0xffu)) {
                        grayscale = false;
                        break;
                    }
                }
                if (!grayscale) {
                    image->mode = PILLOW_C_MODE_P;
                    image->palette_rgb.assign(256u * 3u, std::uint8_t{0});
                    const std::uint32_t copy_entries = std::min<std::uint32_t>(palette_entries, 256u);
                    for (std::uint32_t i = 0; i < copy_entries; ++i) {
                        const std::size_t src = palette_offset + static_cast<std::size_t>(i) * 4u;
                        image->palette_rgb[static_cast<std::size_t>(i) * 3u + 0u] = data[src + 2u];
                        image->palette_rgb[static_cast<std::size_t>(i) * 3u + 1u] = data[src + 1u];
                        image->palette_rgb[static_cast<std::size_t>(i) * 3u + 2u] = data[src + 0u];
                    }
                }
            }
        }

        for (int y = 0; y < height; ++y) {
            const int source_y = top_down ? y : (height - 1 - y);
            const std::uint8_t* src_row =
                data.data() + static_cast<std::size_t>(pixel_offset) + static_cast<std::size_t>(source_y) * source_stride;
            std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            if (bits_per_pixel == 8) {
                std::memcpy(dst_row, src_row, static_cast<std::size_t>(width));
            } else if (bits_per_pixel == 24) {
                for (int x = 0; x < width; ++x) {
                    dst_row[static_cast<std::size_t>(x) * 3u + 0u] = src_row[static_cast<std::size_t>(x) * 3u + 2u];
                    dst_row[static_cast<std::size_t>(x) * 3u + 1u] = src_row[static_cast<std::size_t>(x) * 3u + 1u];
                    dst_row[static_cast<std::size_t>(x) * 3u + 2u] = src_row[static_cast<std::size_t>(x) * 3u + 0u];
                }
            } else {
                for (int x = 0; x < width; ++x) {
                    dst_row[static_cast<std::size_t>(x) * 3u + 0u] = src_row[static_cast<std::size_t>(x) * 4u + 2u];
                    dst_row[static_cast<std::size_t>(x) * 3u + 1u] = src_row[static_cast<std::size_t>(x) * 4u + 1u];
                    dst_row[static_cast<std::size_t>(x) * 3u + 2u] = src_row[static_cast<std::size_t>(x) * 4u + 0u];
                }
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_bmp_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->mode != PILLOW_C_MODE_L &&
        image->mode != PILLOW_C_MODE_RGB &&
        image->mode != PILLOW_C_MODE_RGBA) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int bits_per_pixel = image->mode == PILLOW_C_MODE_L ? 8 : (image->mode == PILLOW_C_MODE_RGBA ? 32 : 24);
    std::size_t row_stride = 0;
    int status = bmp_row_stride(image->width, bits_per_pixel, &row_stride);
    if (status != PILLOW_C_OK) {
        return status;
    }
    const std::size_t palette_size = image->mode == PILLOW_C_MODE_L ? 256u * 4u : 0u;
    const std::uint64_t pixel_size_u64 = static_cast<std::uint64_t>(row_stride) * static_cast<std::uint64_t>(image->height);
    const std::uint64_t pixel_offset_u64 = 14u + 40u + palette_size;
    const std::uint64_t file_size_u64 = pixel_offset_u64 + pixel_size_u64;
    if (file_size_u64 > std::numeric_limits<std::uint32_t>::max()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<std::uint8_t> out;
        out.reserve(static_cast<std::size_t>(file_size_u64));
        out.push_back('B');
        out.push_back('M');
        append_le32(out, static_cast<std::uint32_t>(file_size_u64));
        append_le16(out, 0);
        append_le16(out, 0);
        append_le32(out, static_cast<std::uint32_t>(pixel_offset_u64));
        append_le32(out, 40);
        append_le32(out, static_cast<std::uint32_t>(image->width));
        append_le32(out, static_cast<std::uint32_t>(image->height));
        append_le16(out, 1);
        append_le16(out, static_cast<std::uint16_t>(bits_per_pixel));
        append_le32(out, 0);
        append_le32(out, static_cast<std::uint32_t>(pixel_size_u64));
        append_le32(out, 3780);
        append_le32(out, 3780);
        append_le32(out, image->mode == PILLOW_C_MODE_L ? 256u : 0u);
        append_le32(out, image->mode == PILLOW_C_MODE_L ? 256u : 0u);

        if (image->mode == PILLOW_C_MODE_L) {
            for (int i = 0; i < 256; ++i) {
                out.push_back(static_cast<std::uint8_t>(i));
                out.push_back(static_cast<std::uint8_t>(i));
                out.push_back(static_cast<std::uint8_t>(i));
                out.push_back(0);
            }
        }

        const std::size_t bytes_per_pixel = static_cast<std::size_t>(bits_per_pixel / 8);
        std::vector<std::uint8_t> row(row_stride, std::uint8_t{0});
        for (int y = image->height - 1; y >= 0; --y) {
            std::fill(row.begin(), row.end(), std::uint8_t{0});
            const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            if (image->mode == PILLOW_C_MODE_L) {
                std::memcpy(row.data(), src_row, static_cast<std::size_t>(image->width));
            } else if (image->mode == PILLOW_C_MODE_RGB) {
                for (int x = 0; x < image->width; ++x) {
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 0u] = src_row[static_cast<std::size_t>(x) * 3u + 2u];
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 1u] = src_row[static_cast<std::size_t>(x) * 3u + 1u];
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 2u] = src_row[static_cast<std::size_t>(x) * 3u + 0u];
                }
            } else {
                for (int x = 0; x < image->width; ++x) {
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 0u] = src_row[static_cast<std::size_t>(x) * 4u + 2u];
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 1u] = src_row[static_cast<std::size_t>(x) * 4u + 1u];
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 2u] = src_row[static_cast<std::size_t>(x) * 4u + 0u];
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 3u] = src_row[static_cast<std::size_t>(x) * 4u + 3u];
                }
            }
            out.insert(out.end(), row.begin(), row.end());
        }

        if (!write_binary_file(path, out)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int create_wic_factory(ComPtr<IWICImagingFactory>* factory)
{
    if (!factory) {
        return PILLOW_C_NULL_POINTER;
    }
    const HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(factory->put()));
    return SUCCEEDED(hr) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
}

int wic_format_to_mode(const WICPixelFormatGUID& format, int* mode, int* channels, WICPixelFormatGUID* target_format)
{
    if (!mode || !channels || !target_format) {
        return PILLOW_C_NULL_POINTER;
    }
    if (IsEqualGUID(format, GUID_WICPixelFormat8bppGray)) {
        *mode = PILLOW_C_MODE_L;
        *channels = 1;
        *target_format = GUID_WICPixelFormat8bppGray;
        return PILLOW_C_OK;
    }
    if (IsEqualGUID(format, GUID_WICPixelFormat24bppRGB)) {
        *mode = PILLOW_C_MODE_RGB;
        *channels = 3;
        *target_format = GUID_WICPixelFormat24bppRGB;
        return PILLOW_C_OK;
    }
    if (IsEqualGUID(format, GUID_WICPixelFormat32bppRGBA)) {
        *mode = PILLOW_C_MODE_RGBA;
        *channels = 4;
        *target_format = GUID_WICPixelFormat32bppRGBA;
        return PILLOW_C_OK;
    }
    if (IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA) ||
        IsEqualGUID(format, GUID_WICPixelFormat32bppPBGRA) ||
        IsEqualGUID(format, GUID_WICPixelFormat32bppPRGBA)) {
        *mode = PILLOW_C_MODE_RGBA;
        *channels = 4;
        *target_format = GUID_WICPixelFormat32bppRGBA;
        return PILLOW_C_OK;
    }
    *mode = PILLOW_C_MODE_RGB;
    *channels = 3;
    *target_format = GUID_WICPixelFormat24bppRGB;
    return PILLOW_C_OK;
}

int copy_wic_palette_rgb(IWICBitmapSource* source, IWICImagingFactory* factory, std::vector<std::uint8_t>* out_palette)
{
    if (!source || !factory || !out_palette) {
        return PILLOW_C_NULL_POINTER;
    }
    ComPtr<IWICPalette> palette;
    HRESULT hr = factory->CreatePalette(palette.put());
    if (FAILED(hr)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    hr = source->CopyPalette(palette.get());
    if (FAILED(hr)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    UINT count = 0;
    hr = palette->GetColorCount(&count);
    if (FAILED(hr) || count == 0 || count > 256u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<WICColor> colors(count);
    UINT actual = 0;
    hr = palette->GetColors(count, colors.data(), &actual);
    if (FAILED(hr) || actual == 0 || actual > 256u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    out_palette->assign(static_cast<std::size_t>(actual) * 3u, std::uint8_t{0});
    for (UINT i = 0; i < actual; ++i) {
        const WICColor color = colors[i];
        (*out_palette)[static_cast<std::size_t>(i) * 3u + 0u] = static_cast<std::uint8_t>((color >> 16) & 0xffu);
        (*out_palette)[static_cast<std::size_t>(i) * 3u + 1u] = static_cast<std::uint8_t>((color >> 8) & 0xffu);
        (*out_palette)[static_cast<std::size_t>(i) * 3u + 2u] = static_cast<std::uint8_t>(color & 0xffu);
    }
    return PILLOW_C_OK;
}

int create_wic_palette_from_rgb(
    IWICImagingFactory* factory,
    const std::vector<std::uint8_t>& palette_rgb,
    ComPtr<IWICPalette>* out_palette)
{
    if (!factory || !out_palette) {
        return PILLOW_C_NULL_POINTER;
    }
    if (palette_rgb.size() % 3u != 0u || palette_rgb.size() > 256u * 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t source_count = palette_rgb.size() / 3u;
    const std::size_t color_count = std::max<std::size_t>(source_count, 2u);
    if (color_count > 256u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<WICColor> colors(color_count, 0xff000000u);
    for (std::size_t i = 0; i < source_count; ++i) {
        colors[i] = 0xff000000u |
                    (static_cast<std::uint32_t>(palette_rgb[i * 3u + 0u]) << 16) |
                    (static_cast<std::uint32_t>(palette_rgb[i * 3u + 1u]) << 8) |
                    static_cast<std::uint32_t>(palette_rgb[i * 3u + 2u]);
    }

    HRESULT hr = factory->CreatePalette(out_palette->put());
    if (FAILED(hr)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    hr = out_palette->get()->InitializeCustom(colors.data(), static_cast<UINT>(colors.size()));
    return SUCCEEDED(hr) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
}

int open_png_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        PngHeaderInfo header_info = {};
        if (!read_png_header_info(path, &header_info)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromFilename(
            wide_path.data(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            decoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        GUID container = {};
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatPng)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, frame.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        UINT width_u = 0;
        UINT height_u = 0;
        hr = frame->GetSize(&width_u, &height_u);
        if (FAILED(hr) || width_u > static_cast<UINT>(std::numeric_limits<int>::max()) ||
            height_u > static_cast<UINT>(std::numeric_limits<int>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = static_cast<int>(width_u);
        const int height = static_cast<int>(height_u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID source_format = {};
        hr = frame->GetPixelFormat(&source_format);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        int mode = 0;
        int channels = 0;
        int decoded_channels = 0;
        WICPixelFormatGUID target_format = {};
        if (header_info.color_type == 4 && header_info.bit_depth == 8) {
            mode = PILLOW_C_MODE_LA;
            channels = 2;
            decoded_channels = 4;
            target_format = GUID_WICPixelFormat32bppRGBA;
        } else if (header_info.color_type == 3 && header_info.bit_depth <= 8) {
            mode = PILLOW_C_MODE_P;
            channels = 1;
            decoded_channels = 1;
            target_format = GUID_WICPixelFormat8bppIndexed;
        } else {
            status = wic_format_to_mode(source_format, &mode, &channels, &target_format);
            if (status != PILLOW_C_OK) {
                return status;
            }
            decoded_channels = channels;
        }

        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t decoded_stride = 0;
        std::size_t decoded_size = 0;
        if (!checked_image_size(width, height, decoded_channels, &decoded_stride, &decoded_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> palette_rgb;
        ComPtr<IWICPalette> source_palette;
        IWICPalette* converter_palette = nullptr;
        if (mode == PILLOW_C_MODE_P) {
            status = copy_wic_palette_rgb(frame.get(), factory.get(), &palette_rgb);
            if (status != PILLOW_C_OK) {
                return status;
            }
            HRESULT palette_hr = factory->CreatePalette(source_palette.put());
            if (FAILED(palette_hr) || FAILED(frame->CopyPalette(source_palette.get()))) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            converter_palette = source_palette.get();
        }

        ComPtr<IWICBitmapSource> source;
        if (IsEqualGUID(source_format, target_format)) {
            source.reset(frame.get());
            source.get()->AddRef();
        } else {
            ComPtr<IWICFormatConverter> converter;
            hr = factory->CreateFormatConverter(converter.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = converter->Initialize(
                frame.get(),
                target_format,
                WICBitmapDitherTypeNone,
                converter_palette,
                0.0,
                mode == PILLOW_C_MODE_P ? WICBitmapPaletteTypeCustom : WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            source.reset(converter.get());
            source.get()->AddRef();
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};
        std::vector<std::uint8_t> decoded;
        std::uint8_t* copy_target = image->pixels.data();
        UINT copy_stride = static_cast<UINT>(stride);
        UINT copy_size = static_cast<UINT>(image->pixels.size());
        if (decoded_channels != channels) {
            decoded.assign(decoded_size, std::uint8_t{0});
            copy_target = decoded.data();
            copy_stride = static_cast<UINT>(decoded_stride);
            copy_size = static_cast<UINT>(decoded.size());
        }
        hr = source->CopyPixels(
            nullptr,
            copy_stride,
            copy_size,
            copy_target);
        if (FAILED(hr)) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (mode == PILLOW_C_MODE_LA) {
            for (int y = 0; y < height; ++y) {
                const std::uint8_t* src_row = decoded.data() + static_cast<std::size_t>(y) * decoded_stride;
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < width; ++x) {
                    const std::size_t src = static_cast<std::size_t>(x) * 4u;
                    const std::size_t dst = static_cast<std::size_t>(x) * 2u;
                    dst_row[dst + 0u] = src_row[src + 0u];
                    dst_row[dst + 1u] = src_row[src + 3u];
                }
            }
        }
        if (mode == PILLOW_C_MODE_P) {
            image->palette_rgb = std::move(palette_rgb);
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int png_mode_format(const PillowCImage* image, WICPixelFormatGUID* format)
{
    if (!image || !format) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
        *format = GUID_WICPixelFormat8bppGray;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
        *format = GUID_WICPixelFormat24bppBGR;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4) {
        *format = GUID_WICPixelFormat32bppBGRA;
        return PILLOW_C_OK;
    }
    return PILLOW_C_INVALID_ARGUMENT;
}

std::uint32_t crc32_bytes(const std::uint8_t* data, std::size_t size)
{
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return crc ^ 0xffffffffu;
}

std::uint32_t adler32_bytes(const std::uint8_t* data, std::size_t size)
{
    constexpr std::uint32_t mod = 65521u;
    std::uint32_t a = 1u;
    std::uint32_t b = 0u;
    for (std::size_t i = 0; i < size; ++i) {
        a = (a + data[i]) % mod;
        b = (b + a) % mod;
    }
    return (b << 16) | a;
}

void append_png_chunk(std::vector<std::uint8_t>& out, const char type[4], const std::vector<std::uint8_t>& data)
{
    append_be32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t type_offset = out.size();
    out.push_back(static_cast<std::uint8_t>(type[0]));
    out.push_back(static_cast<std::uint8_t>(type[1]));
    out.push_back(static_cast<std::uint8_t>(type[2]));
    out.push_back(static_cast<std::uint8_t>(type[3]));
    out.insert(out.end(), data.begin(), data.end());
    append_be32(out, crc32_bytes(out.data() + type_offset, out.size() - type_offset));
}

int append_zlib_stored(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& raw)
{
    out.push_back(0x78u);
    out.push_back(0x01u);
    std::size_t offset = 0;
    do {
        const std::size_t remaining = raw.size() - offset;
        const std::uint16_t block_size = static_cast<std::uint16_t>(std::min<std::size_t>(remaining, 65535u));
        const bool final_block = offset + block_size == raw.size();
        out.push_back(final_block ? 0x01u : 0x00u);
        append_le16(out, block_size);
        append_le16(out, static_cast<std::uint16_t>(~block_size));
        out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset), raw.begin() + static_cast<std::ptrdiff_t>(offset + block_size));
        offset += block_size;
    } while (offset < raw.size());
    append_be32(out, adler32_bytes(raw.empty() ? nullptr : raw.data(), raw.size()));
    return PILLOW_C_OK;
}

int save_png_custom_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!((image->mode == PILLOW_C_MODE_LA && image->channels == 2) ||
          (image->mode == PILLOW_C_MODE_P && image->channels == 1))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int payload_channels = image->mode == PILLOW_C_MODE_LA ? 2 : 1;
    if (image->width > (std::numeric_limits<int>::max() - 1) / payload_channels) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t raw_stride = 1u + static_cast<std::size_t>(image->width) * static_cast<std::size_t>(payload_channels);
    if (image->height > 0 && raw_stride > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(image->height)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t raw_size = raw_stride * static_cast<std::size_t>(image->height);

    try {
        std::vector<std::uint8_t> raw(raw_size, std::uint8_t{0});
        for (int y = 0; y < image->height; ++y) {
            std::uint8_t* dst = raw.data() + static_cast<std::size_t>(y) * raw_stride;
            const std::uint8_t* src = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            dst[0] = 0;
            std::memcpy(dst + 1, src, static_cast<std::size_t>(image->width) * static_cast<std::size_t>(payload_channels));
        }

        std::vector<std::uint8_t> png;
        static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        png.insert(png.end(), signature, signature + 8);

        std::vector<std::uint8_t> ihdr;
        append_be32(ihdr, static_cast<std::uint32_t>(image->width));
        append_be32(ihdr, static_cast<std::uint32_t>(image->height));
        ihdr.push_back(8);
        ihdr.push_back(image->mode == PILLOW_C_MODE_LA ? 4 : 3);
        ihdr.push_back(0);
        ihdr.push_back(0);
        ihdr.push_back(0);
        append_png_chunk(png, "IHDR", ihdr);

        if (image->mode == PILLOW_C_MODE_P) {
            std::vector<std::uint8_t> plte = image->palette_rgb;
            if (plte.empty()) {
                plte.assign(3u, std::uint8_t{0});
            }
            if (plte.size() % 3u != 0u || plte.size() > 256u * 3u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            append_png_chunk(png, "PLTE", plte);
        }

        std::vector<std::uint8_t> zlib;
        append_zlib_stored(zlib, raw);
        append_png_chunk(png, "IDAT", zlib);

        std::vector<std::uint8_t> empty;
        append_png_chunk(png, "IEND", empty);

        if (!write_binary_file(path, png)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_png_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->mode == PILLOW_C_MODE_LA || image->mode == PILLOW_C_MODE_P) {
        return save_png_custom_image(image, path);
    }

    WICPixelFormatGUID format = {};
    int status = png_mode_format(image, &format);
    if (status != PILLOW_C_OK) {
        return status;
    }

    try {
        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        ComPtr<IWICStream> stream;
        HRESULT hr = factory->CreateStream(stream.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = stream->InitializeFromFilename(wide_path.data(), GENERIC_WRITE);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapEncoder> encoder;
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameEncode> frame;
        hr = encoder->CreateNewFrame(frame.put(), nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->Initialize(nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->SetSize(static_cast<UINT>(image->width), static_cast<UINT>(image->height));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        WICPixelFormatGUID encoder_format = format;
        hr = frame->SetPixelFormat(&encoder_format);
        if (FAILED(hr) || !IsEqualGUID(encoder_format, format)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::vector<std::uint8_t> encoded_pixels;
        const std::uint8_t* write_data = image->pixels.data();
        if (image->mode == PILLOW_C_MODE_RGB || image->mode == PILLOW_C_MODE_RGBA) {
            encoded_pixels.assign(image->pixels.size(), std::uint8_t{0});
            const std::size_t channels = static_cast<std::size_t>(image->channels);
            for (int y = 0; y < image->height; ++y) {
                const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                std::uint8_t* dst_row = encoded_pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < image->width; ++x) {
                    const std::size_t offset = static_cast<std::size_t>(x) * channels;
                    dst_row[offset + 0u] = src_row[offset + 2u];
                    dst_row[offset + 1u] = src_row[offset + 1u];
                    dst_row[offset + 2u] = src_row[offset + 0u];
                    if (channels == 4u) {
                        dst_row[offset + 3u] = src_row[offset + 3u];
                    }
                }
            }
            write_data = encoded_pixels.data();
        }
        hr = frame->WritePixels(
            static_cast<UINT>(image->height),
            static_cast<UINT>(image->stride),
            static_cast<UINT>(image->pixels.size()),
            const_cast<BYTE*>(write_data));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->Commit();
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Commit();
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_jpeg_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        int component_count = 0;
        if (!read_jpeg_component_count(path, &component_count)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (component_count != 1 && component_count != 3) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromFilename(
            wide_path.data(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            decoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        GUID container = {};
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatJpeg)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, frame.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        UINT width_u = 0;
        UINT height_u = 0;
        hr = frame->GetSize(&width_u, &height_u);
        if (FAILED(hr) || width_u > static_cast<UINT>(std::numeric_limits<int>::max()) ||
            height_u > static_cast<UINT>(std::numeric_limits<int>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = static_cast<int>(width_u);
        const int height = static_cast<int>(height_u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const int mode = component_count == 1 ? PILLOW_C_MODE_L : PILLOW_C_MODE_RGB;
        const int channels = component_count == 1 ? 1 : 3;
        const WICPixelFormatGUID target_format =
            component_count == 1 ? GUID_WICPixelFormat8bppGray : GUID_WICPixelFormat24bppRGB;

        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size) ||
            stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
            size > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID source_format = {};
        hr = frame->GetPixelFormat(&source_format);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapSource> source;
        if (IsEqualGUID(source_format, target_format)) {
            source.reset(frame.get());
            source.get()->AddRef();
        } else {
            ComPtr<IWICFormatConverter> converter;
            hr = factory->CreateFormatConverter(converter.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = converter->Initialize(
                frame.get(),
                target_format,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            source.reset(converter.get());
            source.get()->AddRef();
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};
        hr = source->CopyPixels(
            nullptr,
            static_cast<UINT>(stride),
            static_cast<UINT>(image->pixels.size()),
            image->pixels.data());
        if (FAILED(hr)) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_jpeg_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!((image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_RGB && image->channels == 3))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
        image->pixels.size() > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        ComPtr<IWICStream> stream;
        HRESULT hr = factory->CreateStream(stream.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = stream->InitializeFromFilename(wide_path.data(), GENERIC_WRITE);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapEncoder> encoder;
        hr = factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, encoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameEncode> frame;
        hr = encoder->CreateNewFrame(frame.put(), nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->Initialize(nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->SetSize(static_cast<UINT>(image->width), static_cast<UINT>(image->height));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID format =
            image->mode == PILLOW_C_MODE_L ? GUID_WICPixelFormat8bppGray : GUID_WICPixelFormat24bppBGR;
        WICPixelFormatGUID encoder_format = format;
        hr = frame->SetPixelFormat(&encoder_format);
        if (FAILED(hr) || !IsEqualGUID(encoder_format, format)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> encoded_pixels;
        const std::uint8_t* write_data = image->pixels.data();
        if (image->mode == PILLOW_C_MODE_RGB) {
            encoded_pixels.assign(image->pixels.size(), std::uint8_t{0});
            for (int y = 0; y < image->height; ++y) {
                const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                std::uint8_t* dst_row = encoded_pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < image->width; ++x) {
                    const std::size_t offset = static_cast<std::size_t>(x) * 3u;
                    dst_row[offset + 0u] = src_row[offset + 2u];
                    dst_row[offset + 1u] = src_row[offset + 1u];
                    dst_row[offset + 2u] = src_row[offset + 0u];
                }
            }
            write_data = encoded_pixels.data();
        }

        hr = frame->WritePixels(
            static_cast<UINT>(image->height),
            static_cast<UINT>(image->stride),
            static_cast<UINT>(image->pixels.size()),
            const_cast<BYTE*>(write_data));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->Commit();
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Commit();
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_tiff_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromFilename(
            wide_path.data(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            decoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        GUID container = {};
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatTiff)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, frame.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        UINT width_u = 0;
        UINT height_u = 0;
        hr = frame->GetSize(&width_u, &height_u);
        if (FAILED(hr) || width_u > static_cast<UINT>(std::numeric_limits<int>::max()) ||
            height_u > static_cast<UINT>(std::numeric_limits<int>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = static_cast<int>(width_u);
        const int height = static_cast<int>(height_u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID source_format = {};
        hr = frame->GetPixelFormat(&source_format);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        int mode = 0;
        int channels = 0;
        WICPixelFormatGUID target_format = {};
        status = wic_format_to_mode(source_format, &mode, &channels, &target_format);
        if (status != PILLOW_C_OK) {
            return status;
        }
        if (!((mode == PILLOW_C_MODE_L && channels == 1) ||
              (mode == PILLOW_C_MODE_RGB && channels == 3) ||
              (mode == PILLOW_C_MODE_RGBA && channels == 4))) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size) ||
            stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
            size > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapSource> source;
        if (IsEqualGUID(source_format, target_format)) {
            source.reset(frame.get());
            source.get()->AddRef();
        } else {
            ComPtr<IWICFormatConverter> converter;
            hr = factory->CreateFormatConverter(converter.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = converter->Initialize(
                frame.get(),
                target_format,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            source.reset(converter.get());
            source.get()->AddRef();
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};
        hr = source->CopyPixels(
            nullptr,
            static_cast<UINT>(stride),
            static_cast<UINT>(image->pixels.size()),
            image->pixels.data());
        if (FAILED(hr)) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_tiff_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!((image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) ||
          (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
        image->pixels.size() > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        ComPtr<IWICStream> stream;
        HRESULT hr = factory->CreateStream(stream.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = stream->InitializeFromFilename(wide_path.data(), GENERIC_WRITE);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapEncoder> encoder;
        hr = factory->CreateEncoder(GUID_ContainerFormatTiff, nullptr, encoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameEncode> frame;
        hr = encoder->CreateNewFrame(frame.put(), nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->Initialize(nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->SetSize(static_cast<UINT>(image->width), static_cast<UINT>(image->height));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID format = GUID_WICPixelFormat8bppGray;
        if (image->mode == PILLOW_C_MODE_RGB) {
            format = GUID_WICPixelFormat24bppBGR;
        } else if (image->mode == PILLOW_C_MODE_RGBA) {
            format = GUID_WICPixelFormat32bppBGRA;
        }
        WICPixelFormatGUID encoder_format = format;
        hr = frame->SetPixelFormat(&encoder_format);
        if (FAILED(hr) || !IsEqualGUID(encoder_format, format)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> encoded_pixels;
        const std::uint8_t* write_data = image->pixels.data();
        if (image->mode == PILLOW_C_MODE_RGB || image->mode == PILLOW_C_MODE_RGBA) {
            encoded_pixels.assign(image->pixels.size(), std::uint8_t{0});
            const std::size_t channels = static_cast<std::size_t>(image->channels);
            for (int y = 0; y < image->height; ++y) {
                const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                std::uint8_t* dst_row = encoded_pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < image->width; ++x) {
                    const std::size_t offset = static_cast<std::size_t>(x) * channels;
                    dst_row[offset + 0u] = src_row[offset + 2u];
                    dst_row[offset + 1u] = src_row[offset + 1u];
                    dst_row[offset + 2u] = src_row[offset + 0u];
                    if (channels == 4u) {
                        dst_row[offset + 3u] = src_row[offset + 3u];
                    }
                }
            }
            write_data = encoded_pixels.data();
        }

        hr = frame->WritePixels(
            static_cast<UINT>(image->height),
            static_cast<UINT>(image->stride),
            static_cast<UINT>(image->pixels.size()),
            const_cast<BYTE*>(write_data));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->Commit();
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Commit();
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_gif_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromFilename(
            wide_path.data(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            decoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        GUID container = {};
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatGif)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, frame.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        UINT width_u = 0;
        UINT height_u = 0;
        hr = frame->GetSize(&width_u, &height_u);
        if (FAILED(hr) || width_u > static_cast<UINT>(std::numeric_limits<int>::max()) ||
            height_u > static_cast<UINT>(std::numeric_limits<int>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = static_cast<int>(width_u);
        const int height = static_cast<int>(height_u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> palette_rgb;
        status = copy_wic_palette_rgb(frame.get(), factory.get(), &palette_rgb);
        if (status != PILLOW_C_OK) {
            return status;
        }
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, 1, &stride, &size) ||
            stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
            size > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID source_format = {};
        hr = frame->GetPixelFormat(&source_format);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICPalette> source_palette;
        hr = factory->CreatePalette(source_palette.put());
        if (FAILED(hr) || FAILED(frame->CopyPalette(source_palette.get()))) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapSource> source;
        if (IsEqualGUID(source_format, GUID_WICPixelFormat8bppIndexed)) {
            source.reset(frame.get());
            source.get()->AddRef();
        } else {
            ComPtr<IWICFormatConverter> converter;
            hr = factory->CreateFormatConverter(converter.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = converter->Initialize(
                frame.get(),
                GUID_WICPixelFormat8bppIndexed,
                WICBitmapDitherTypeNone,
                source_palette.get(),
                0.0,
                WICBitmapPaletteTypeCustom);
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            source.reset(converter.get());
            source.get()->AddRef();
        }

        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_P,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        hr = source->CopyPixels(
            nullptr,
            static_cast<UINT>(stride),
            static_cast<UINT>(image->pixels.size()),
            image->pixels.data());
        if (FAILED(hr)) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }
        image->palette_rgb = std::move(palette_rgb);
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_gif_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->mode != PILLOW_C_MODE_P || image->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->palette_rgb.empty() || image->palette_rgb.size() % 3u != 0u || image->palette_rgb.size() > 256u * 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
        image->pixels.size() > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }
        ComPtr<IWICPalette> palette;
        status = create_wic_palette_from_rgb(factory.get(), image->palette_rgb, &palette);
        if (status != PILLOW_C_OK) {
            return status;
        }

        ComPtr<IWICStream> stream;
        HRESULT hr = factory->CreateStream(stream.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = stream->InitializeFromFilename(wide_path.data(), GENERIC_WRITE);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapEncoder> encoder;
        hr = factory->CreateEncoder(GUID_ContainerFormatGif, nullptr, encoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->SetPalette(palette.get());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameEncode> frame;
        hr = encoder->CreateNewFrame(frame.put(), nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->Initialize(nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->SetSize(static_cast<UINT>(image->width), static_cast<UINT>(image->height));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->SetPalette(palette.get());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID format = GUID_WICPixelFormat8bppIndexed;
        WICPixelFormatGUID encoder_format = format;
        hr = frame->SetPixelFormat(&encoder_format);
        if (FAILED(hr) || !IsEqualGUID(encoder_format, format)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->WritePixels(
            static_cast<UINT>(image->height),
            static_cast<UINT>(image->stride),
            static_cast<UINT>(image->pixels.size()),
            const_cast<BYTE*>(image->pixels.data()));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->Commit();
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Commit();
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
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
    if (!image_shape_matches(target, PILLOW_C_GRADIENT_SIZE, PILLOW_C_GRADIENT_SIZE, mode, 1)) {
        return PILLOW_C_MISMATCH;
    }

    for (int y = 0; y < PILLOW_C_GRADIENT_SIZE; ++y) {
        std::uint8_t* row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        std::fill_n(row, PILLOW_C_GRADIENT_SIZE, static_cast<std::uint8_t>(y));
    }
    target->palette_rgb.clear();
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
    if (!image_shape_matches(target, PILLOW_C_GRADIENT_SIZE, PILLOW_C_GRADIENT_SIZE, mode, 1)) {
        return PILLOW_C_MISMATCH;
    }

    for (int y = 0; y < PILLOW_C_GRADIENT_SIZE; ++y) {
        std::uint8_t* row = target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        const double dy = static_cast<double>(y - 128);
        for (int x = 0; x < PILLOW_C_GRADIENT_SIZE; ++x) {
            const double dx = static_cast<double>(x - 128);
            const int value = static_cast<int>(std::sqrt(dx * dx + dy * dy) * PILLOW_C_SQRT2);
            row[x] = static_cast<std::uint8_t>(value > 255 ? 255 : value);
        }
    }
    target->palette_rgb.clear();
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
                row[x] = pillow_clip8_double(128.0 + static_cast<double>(sigma_f) * factor * v1);
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

void decode_raw_pixel(const RawCodecSpec& spec, const std::uint8_t* src, std::uint8_t* dst, int target_mode)
{
    switch (target_mode) {
    case PILLOW_C_MODE_P:
        dst[0] = src[0];
        return;
    case PILLOW_C_MODE_L:
        dst[0] = src[0];
        return;
    case PILLOW_C_MODE_LA:
        dst[0] = src[0];
        dst[1] = src[1];
        return;
    case PILLOW_C_MODE_RGB:
        switch (spec.kind) {
        case RawCodecKind::RGB:
        case RawCodecKind::RGBX:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            return;
        case RawCodecKind::BGR:
        case RawCodecKind::BGRX:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            return;
        case RawCodecKind::XBGR:
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            return;
        default:
            return;
        }
    case PILLOW_C_MODE_RGBA:
        switch (spec.kind) {
        case RawCodecKind::RGBA:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            return;
        case RawCodecKind::BGRA:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            return;
        case RawCodecKind::ARGB:
            dst[0] = src[1];
            dst[1] = src[2];
            dst[2] = src[3];
            dst[3] = src[0];
            return;
        case RawCodecKind::ABGR:
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
            return;
        case RawCodecKind::BGR:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = 255;
            return;
        default:
            return;
        }
    case PILLOW_C_MODE_CMYK:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        return;
    default:
        return;
    }
}

int set_mode1_raw_bytes_image(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size,
    int stride,
    int orientation)
{
    if (!image || !data) {
        return PILLOW_C_NULL_POINTER;
    }
    if (stride < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t tight_stride = 0;
    std::size_t tight_size = 0;
    if (!checked_mode1_raw_size(image, &tight_stride, &tight_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t source_stride = stride == 0 ? tight_stride : static_cast<std::size_t>(stride);
    if (source_stride < tight_stride) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (image->height > 0) {
        const std::size_t required = source_stride * static_cast<std::size_t>(image->height - 1) + tight_stride;
        if (required > size) {
            return PILLOW_C_INVALID_LENGTH;
        }
    }

    for (int y = 0; y < image->height; ++y) {
        const int source_y = orientation < 0 ? (image->height - 1 - y) : y;
        const std::uint8_t* src_row = data + static_cast<std::size_t>(source_y) * source_stride;
        std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = 0; x < image->width; ++x) {
            const std::uint8_t packed = src_row[static_cast<std::size_t>(x) / 8u];
            const int shift = 7 - (x & 7);
            dst_row[x] = ((packed >> shift) & 1u) ? 255u : 0u;
        }
    }
    return PILLOW_C_OK;
}

int get_mode1_raw_bytes_image(
    const PillowCImage* image,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!image || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }

    std::size_t row_bytes = 0;
    std::size_t required = 0;
    if (!checked_mode1_raw_size(image, &row_bytes, &required)) {
        *out_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_required = required;
    if (!out) {
        return PILLOW_C_OK;
    }
    if (out_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (required == 0) {
        return PILLOW_C_OK;
    }
    std::fill(out, out + required, std::uint8_t{0});

    for (int y = 0; y < image->height; ++y) {
        const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        std::uint8_t* dst_row = out + static_cast<std::size_t>(y) * row_bytes;
        for (int x = 0; x < image->width; ++x) {
            if (src_row[x] != 0) {
                dst_row[static_cast<std::size_t>(x) / 8u] |= static_cast<std::uint8_t>(0x80u >> (x & 7));
            }
        }
    }
    return PILLOW_C_OK;
}

void encode_raw_pixel(const RawCodecSpec& spec, const std::uint8_t* src, std::uint8_t* dst, int source_mode)
{
    switch (source_mode) {
    case PILLOW_C_MODE_P:
        dst[0] = src[0];
        return;
    case PILLOW_C_MODE_L:
        dst[0] = src[0];
        return;
    case PILLOW_C_MODE_LA:
        dst[0] = src[0];
        dst[1] = src[1];
        return;
    case PILLOW_C_MODE_RGB:
        switch (spec.kind) {
        case RawCodecKind::RGB:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            return;
        case RawCodecKind::BGR:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            return;
        case RawCodecKind::RGBX:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;
            return;
        case RawCodecKind::BGRX:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = 0;
            return;
        case RawCodecKind::XBGR:
            dst[0] = 0;
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
            return;
        default:
            return;
        }
    case PILLOW_C_MODE_RGBA:
        switch (spec.kind) {
        case RawCodecKind::RGBA:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            return;
        case RawCodecKind::BGRA:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            return;
        case RawCodecKind::ABGR:
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
            return;
        case RawCodecKind::RGB:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            return;
        case RawCodecKind::BGR:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            return;
        default:
            return;
        }
    case PILLOW_C_MODE_CMYK:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        return;
    default:
        return;
    }
}

int set_raw_bytes_image(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size,
    const char* raw_mode,
    int stride,
    int orientation)
{
    if (!image || !data || !raw_mode) {
        return PILLOW_C_NULL_POINTER;
    }
    const RawCodecSpec spec = raw_decode_spec(image->mode, raw_mode);
    if (spec.kind == RawCodecKind::One) {
        return set_mode1_raw_bytes_image(image, data, size, stride, orientation);
    }
    if (spec.kind == RawCodecKind::Unsupported || spec.bytes_per_pixel <= 0 || stride < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const std::size_t tight_stride = static_cast<std::size_t>(image->width) * static_cast<std::size_t>(spec.bytes_per_pixel);
    const std::size_t source_stride = stride == 0 ? tight_stride : static_cast<std::size_t>(stride);
    if (source_stride < tight_stride) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (image->height > 0) {
        const std::size_t required = source_stride * static_cast<std::size_t>(image->height - 1) + tight_stride;
        if (required > size) {
            return PILLOW_C_INVALID_LENGTH;
        }
    }

    for (int y = 0; y < image->height; ++y) {
        const int source_y = orientation < 0 ? (image->height - 1 - y) : y;
        const std::uint8_t* src_row = data + static_cast<std::size_t>(source_y) * source_stride;
        std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = 0; x < image->width; ++x) {
            decode_raw_pixel(
                spec,
                src_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(spec.bytes_per_pixel),
                dst_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels),
                image->mode);
        }
    }
    return PILLOW_C_OK;
}

int get_raw_bytes_image(
    const PillowCImage* image,
    const char* raw_mode,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!image || !raw_mode || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    const RawCodecSpec spec = raw_encode_spec(image->mode, raw_mode);
    if (spec.kind == RawCodecKind::One) {
        return get_mode1_raw_bytes_image(image, out, out_size, out_required);
    }
    if (spec.kind == RawCodecKind::Unsupported || spec.bytes_per_pixel <= 0) {
        *out_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t required = 0;
    if (!checked_raw_output_size(image, spec.bytes_per_pixel, &required)) {
        *out_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_required = required;
    if (!out) {
        return PILLOW_C_OK;
    }
    if (out_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }

    for (int y = 0; y < image->height; ++y) {
        const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        std::uint8_t* dst_row = out + static_cast<std::size_t>(y) * static_cast<std::size_t>(image->width) * static_cast<std::size_t>(spec.bytes_per_pixel);
        for (int x = 0; x < image->width; ++x) {
            encode_raw_pixel(
                spec,
                src_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels),
                dst_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(spec.bytes_per_pixel),
                image->mode);
        }
    }
    return PILLOW_C_OK;
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

    copy_palette_if_same_mode(source, target);
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
    if (!supported_composite_mask(mask)) {
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
            const std::uint8_t alpha = mask_alpha_at(mask, mask_row, x);
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
        if (!supported_composite_mask(mask)) {
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

            const std::uint8_t alpha = mask_alpha_at(mask, mask_row, x);
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
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    int status = fill_image_pixels(target, color, color_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    status = paste_image_pixels_into(target, source, left, top);
    if (status == PILLOW_C_OK) {
        copy_palette_if_same_mode(source, target);
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

    copy_palette_if_same_mode(source, target);
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
        copy_palette_if_same_mode(source, target);
        return PILLOW_C_OK;
    }

    const int width = source->width;
    const int height = source->height;
    const int channels = source->channels;
    if (width <= 0 || height <= 0) {
        copy_palette_if_same_mode(source, target);
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
    copy_palette_if_same_mode(source, target);
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

void fill_horizontal_span(
    PillowCImage* image,
    std::int64_t left,
    std::int64_t top,
    std::int64_t right_exclusive,
    const std::uint8_t* color)
{
    if (left < 0) {
        left = 0;
    }
    if (right_exclusive > image->width) {
        right_exclusive = image->width;
    }
    if (top < 0 || top >= image->height || right_exclusive <= left) {
        return;
    }

    const int clipped_left = static_cast<int>(left);
    const int clipped_top = static_cast<int>(top);
    const int clipped_right = static_cast<int>(right_exclusive);
    std::uint8_t* dst =
        image->pixels.data() +
        static_cast<std::size_t>(clipped_top) * image->stride +
        static_cast<std::size_t>(clipped_left) * image->channels;
    const std::size_t color_size = static_cast<std::size_t>(image->channels);
    const int pixel_count = clipped_right - clipped_left;
    if (image->channels == 1) {
        std::memset(dst, color[0], static_cast<std::size_t>(pixel_count));
        return;
    }

    std::memcpy(dst, color, color_size);
    std::size_t filled = color_size;
    const std::size_t total = static_cast<std::size_t>(pixel_count) * color_size;
    while (filled < total) {
        const std::size_t copy_size = std::min(filled, total - filled);
        std::memcpy(dst + filled, dst, copy_size);
        filled += copy_size;
    }
}

void fill_rectangle_region(
    PillowCImage* image,
    std::int64_t left,
    std::int64_t top,
    std::int64_t right_exclusive,
    std::int64_t bottom_exclusive,
    const std::uint8_t* color)
{
    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right_exclusive > image->width) {
        right_exclusive = image->width;
    }
    if (bottom_exclusive > image->height) {
        bottom_exclusive = image->height;
    }
    if (right_exclusive <= left || bottom_exclusive <= top) {
        return;
    }

    const int clipped_left = static_cast<int>(left);
    const int clipped_top = static_cast<int>(top);
    const int clipped_right = static_cast<int>(right_exclusive);
    const int clipped_bottom = static_cast<int>(bottom_exclusive);

    fill_horizontal_span(image, clipped_left, clipped_top, clipped_right, color);
    if (image->channels == 1) {
        for (int y = clipped_top + 1; y < clipped_bottom; ++y) {
            std::uint8_t* dst = image->pixels.data() + static_cast<std::size_t>(y) * image->stride + clipped_left;
            std::memset(dst, color[0], static_cast<std::size_t>(clipped_right - clipped_left));
        }
        return;
    }

    const std::size_t row_bytes = static_cast<std::size_t>(clipped_right - clipped_left) * image->channels;
    const std::uint8_t* first_row =
        image->pixels.data() +
        static_cast<std::size_t>(clipped_top) * image->stride +
        static_cast<std::size_t>(clipped_left) * image->channels;
    for (int y = clipped_top + 1; y < clipped_bottom; ++y) {
        std::uint8_t* dst =
            image->pixels.data() +
            static_cast<std::size_t>(y) * image->stride +
            static_cast<std::size_t>(clipped_left) * image->channels;
        std::memcpy(dst, first_row, row_bytes);
    }
}

void fill_vertical_span(
    PillowCImage* image,
    std::int64_t x,
    std::int64_t top,
    std::int64_t bottom_exclusive,
    const std::uint8_t* color)
{
    if (x < 0 || x >= image->width) {
        return;
    }
    if (top < 0) {
        top = 0;
    }
    if (bottom_exclusive > image->height) {
        bottom_exclusive = image->height;
    }
    if (bottom_exclusive <= top) {
        return;
    }

    const int clipped_x = static_cast<int>(x);
    const int clipped_top = static_cast<int>(top);
    const int clipped_bottom = static_cast<int>(bottom_exclusive);
    const std::size_t color_size = static_cast<std::size_t>(image->channels);
    for (int y = clipped_top; y < clipped_bottom; ++y) {
        std::uint8_t* dst =
            image->pixels.data() +
            static_cast<std::size_t>(y) * image->stride +
            static_cast<std::size_t>(clipped_x) * image->channels;
        std::memcpy(dst, color, color_size);
    }
}

double color_diff_1norm(const std::uint8_t* left, const std::uint8_t* right, int channels)
{
    double diff = 0.0;
    for (int channel = 0; channel < channels; ++channel) {
        diff += std::abs(static_cast<int>(left[channel]) - static_cast<int>(right[channel]));
    }
    return diff;
}

bool pixel_color_equal(const std::uint8_t* left, const std::uint8_t* right, int channels)
{
    return std::memcmp(left, right, static_cast<std::size_t>(channels)) == 0;
}

int draw_floodfill_image(
    PillowCImage* image,
    int seed_x,
    int seed_y,
    const std::uint8_t* value,
    std::size_t value_size,
    const std::uint8_t* border,
    std::size_t border_size,
    double thresh)
{
    if (!image || !value) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if (value_size != channels || (border_size != 0 && border_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_OK;
    }

    std::size_t seed_offset = 0;
    const int seed_status = image_pixel_offset(image, seed_x, seed_y, &seed_offset);
    if (seed_status == PILLOW_C_INVALID_ARGUMENT) {
        return PILLOW_C_OK;
    }
    if (seed_status != PILLOW_C_OK) {
        return seed_status;
    }

    try {
        std::vector<std::uint8_t> background(channels);
        std::memcpy(background.data(), image->pixels.data() + seed_offset, channels);
        if (color_diff_1norm(value, background.data(), image->channels) <= thresh) {
            return PILLOW_C_OK;
        }

        std::vector<std::uint8_t> seen(static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height), 0);
        std::vector<int> edge;
        std::vector<int> next_edge;
        edge.reserve(64);
        next_edge.reserve(64);

        int normalized_seed_x = 0;
        int normalized_seed_y = 0;
        if (normalize_coordinate(seed_x, image->width, &normalized_seed_x) != PILLOW_C_OK ||
            normalize_coordinate(seed_y, image->height, &normalized_seed_y) != PILLOW_C_OK) {
            return PILLOW_C_OK;
        }

        std::memcpy(image->pixels.data() + seed_offset, value, channels);
        edge.push_back(normalized_seed_y * image->width + normalized_seed_x);

        while (!edge.empty()) {
            next_edge.clear();
            for (const int index : edge) {
                const int x = index % image->width;
                const int y = index / image->width;
                const int neighbors[4][2] = {
                    {x + 1, y},
                    {x - 1, y},
                    {x, y + 1},
                    {x, y - 1},
                };

                for (const auto& neighbor : neighbors) {
                    const int nx = neighbor[0];
                    const int ny = neighbor[1];
                    if (nx < 0 || ny < 0 || nx >= image->width || ny >= image->height) {
                        continue;
                    }
                    const int neighbor_index = ny * image->width + nx;
                    std::uint8_t& visited = seen[static_cast<std::size_t>(neighbor_index)];
                    if (visited) {
                        continue;
                    }
                    visited = 1;

                    std::uint8_t* pixel =
                        image->pixels.data() +
                        static_cast<std::size_t>(ny) * image->stride +
                        static_cast<std::size_t>(nx) * channels;
                    bool should_fill = false;
                    if (border) {
                        should_fill =
                            !pixel_color_equal(pixel, value, image->channels) &&
                            !pixel_color_equal(pixel, border, image->channels);
                    } else {
                        should_fill = color_diff_1norm(pixel, background.data(), image->channels) <= thresh;
                    }
                    if (should_fill) {
                        std::memcpy(pixel, value, channels);
                        next_edge.push_back(neighbor_index);
                    }
                }
            }
            edge.swap(next_edge);
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

void quarter_init(QuarterState* state, std::int32_t a, std::int32_t b)
{
    if (a < 0 || b < 0) {
        state->finished = true;
        return;
    }
    state->a = a;
    state->b = b;
    state->cx = a;
    state->cy = b % 2;
    state->ex = a % 2;
    state->ey = b;
    state->a2 = static_cast<std::int64_t>(a) * a;
    state->b2 = static_cast<std::int64_t>(b) * b;
    state->a2b2 = state->a2 * state->b2;
    state->finished = false;
}

std::int64_t quarter_delta(const QuarterState* state, std::int64_t x, std::int64_t y)
{
    return std::llabs(state->a2 * y * y + state->b2 * x * x - state->a2b2);
}

bool quarter_next(QuarterState* state, std::int32_t* out_x, std::int32_t* out_y)
{
    if (state->finished) {
        return false;
    }
    *out_x = state->cx;
    *out_y = state->cy;
    if (state->cx == state->ex && state->cy == state->ey) {
        state->finished = true;
        return true;
    }

    std::int32_t nx = state->cx;
    std::int32_t ny = state->cy + 2;
    std::int64_t ndelta = quarter_delta(state, nx, ny);
    if (nx > 1) {
        std::int64_t new_delta = quarter_delta(state, state->cx - 2, state->cy + 2);
        if (ndelta > new_delta) {
            nx = state->cx - 2;
            ny = state->cy + 2;
            ndelta = new_delta;
        }
        new_delta = quarter_delta(state, state->cx - 2, state->cy);
        if (ndelta > new_delta) {
            nx = state->cx - 2;
            ny = state->cy;
        }
    }
    state->cx = nx;
    state->cy = ny;
    return true;
}

void ellipse_init(EllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width)
{
    state->bufcnt = 0;
    state->leftmost = (a % 2) != 0;
    quarter_init(&state->outer, a, b);
    if (width < 1 || !quarter_next(&state->outer, &state->pr, &state->py)) {
        state->finished = true;
        return;
    }
    state->finished = false;
    quarter_init(&state->inner, a - 2 * (width - 1), b - 2 * (width - 1));
    state->pl = state->leftmost ? 1 : 0;
}

bool ellipse_next(EllipseState* state, std::int32_t* out_x0, std::int32_t* out_y, std::int32_t* out_x1)
{
    if (state->bufcnt == 0) {
        if (state->finished) {
            return false;
        }

        const std::int32_t y = state->py;
        std::int32_t l = state->pl;
        const std::int32_t r = state->pr;
        std::int32_t cx = 0;
        std::int32_t cy = 0;
        bool has_next = false;

        while ((has_next = quarter_next(&state->outer, &cx, &cy)) && cy <= y) {
        }
        if (!has_next) {
            state->finished = true;
        } else {
            state->pr = cx;
            state->py = cy;
        }

        while ((has_next = quarter_next(&state->inner, &cx, &cy)) && cy <= y) {
            l = cx;
        }
        state->pl = has_next ? cx : (state->leftmost ? 1 : 0);

        if ((l > 0 || l < r) && y > 0) {
            state->cl[state->bufcnt] = l == 0 ? 2 : l;
            state->cy[state->bufcnt] = y;
            state->cr[state->bufcnt] = r;
            ++state->bufcnt;
        }
        if (y > 0) {
            state->cl[state->bufcnt] = -r;
            state->cy[state->bufcnt] = y;
            state->cr[state->bufcnt] = -l;
            ++state->bufcnt;
        }
        if (l > 0 || l < r) {
            state->cl[state->bufcnt] = l == 0 ? 2 : l;
            state->cy[state->bufcnt] = -y;
            state->cr[state->bufcnt] = r;
            ++state->bufcnt;
        }
        state->cl[state->bufcnt] = -r;
        state->cy[state->bufcnt] = -y;
        state->cr[state->bufcnt] = -l;
        ++state->bufcnt;
    }

    --state->bufcnt;
    *out_x0 = state->cl[state->bufcnt];
    *out_y = state->cy[state->bufcnt];
    *out_x1 = state->cr[state->bufcnt];
    return true;
}

void draw_ellipse_spans(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* color,
    int width)
{
    const int a = right - left;
    const int b = bottom - top;
    EllipseState state{};
    ellipse_init(&state, a, b, width);

    std::int32_t x0 = 0;
    std::int32_t y = 0;
    std::int32_t x1 = 0;
    while (ellipse_next(&state, &x0, &y, &x1)) {
        fill_horizontal_span(
            image,
            static_cast<std::int64_t>(left) + (static_cast<std::int64_t>(x0) + a) / 2,
            static_cast<std::int64_t>(top) + (static_cast<std::int64_t>(y) + b) / 2,
            static_cast<std::int64_t>(left) + (static_cast<std::int64_t>(x1) + a) / 2 + 1,
            color);
    }
}

int clip_node(ClipEllipseState* state, ClipNodeType type)
{
    const int index = state->node_count++;
    state->nodes[index].type = type;
    state->nodes[index].a = 0.0;
    state->nodes[index].b = 0.0;
    state->nodes[index].c = 0.0;
    state->nodes[index].left = -1;
    state->nodes[index].right = -1;
    return index;
}

void clip_tree_transpose(ClipEllipseState* state, int node_index)
{
    if (node_index < 0) {
        return;
    }
    ClipNode& node = state->nodes[node_index];
    if (node.type == ClipNodeType::Clip) {
        std::swap(node.a, node.b);
    } else {
        clip_tree_transpose(state, node.left);
        clip_tree_transpose(state, node.right);
    }
}

bool clip_tree_do_clip(
    const ClipEllipseState* state,
    int node_index,
    std::int32_t x0,
    std::int32_t y,
    std::int32_t x1,
    std::vector<ClipEvent>* out_events)
{
    if (node_index < 0) {
        out_events->push_back({x0, 1});
        out_events->push_back({x1, -1});
        return true;
    }

    const ClipNode& node = state->nodes[node_index];
    if (node.type == ClipNodeType::Clip) {
        constexpr double eps = 1e-9;
        const double a = node.a;
        const double b = node.b;
        const double c = node.c;
        if (std::fabs(a) < eps) {
            if (b * y + c < -eps) {
                x0 = 1;
                x1 = 0;
            }
        } else {
            const double ix = -(b * y + c) / a;
            if (a * x0 + b * y + c < eps) {
                x0 = static_cast<std::int32_t>(std::lround(std::fmax(x0, ix)));
            }
            if (a * x1 + b * y + c < eps) {
                x1 = static_cast<std::int32_t>(std::lround(std::fmin(x1, ix)));
            }
        }
        if (x0 <= x1) {
            out_events->push_back({x0, 1});
            out_events->push_back({x1, -1});
        }
        return true;
    }

    std::vector<ClipEvent> left_events;
    std::vector<ClipEvent> right_events;
    try {
        left_events.reserve(8);
        right_events.reserve(8);
    } catch (const std::bad_alloc&) {
        return false;
    }
    if (!clip_tree_do_clip(state, node.left, x0, y, x1, &left_events) ||
        !clip_tree_do_clip(state, node.right, x0, y, x1, &right_events)) {
        return false;
    }

    std::size_t left_index = 0;
    std::size_t right_index = 0;
    int left_depth = 0;
    int right_depth = 0;
    bool has_tail = false;
    int tail_type = 0;

    while (left_index < left_events.size() || right_index < right_events.size()) {
        ClipEvent event{};
        if (right_index >= right_events.size() ||
            (left_index < left_events.size() &&
             (left_events[left_index].x < right_events[right_index].x ||
              (left_events[left_index].x == right_events[right_index].x &&
               left_events[left_index].type > right_events[right_index].type)))) {
            event = left_events[left_index++];
            left_depth += event.type;
        } else {
            event = right_events[right_index++];
            right_depth += event.type;
        }

        const bool take_or =
            node.type == ClipNodeType::Or &&
            ((event.type == 1 && (!has_tail || tail_type == -1)) ||
             (event.type == -1 && left_depth == 0 && right_depth == 0));
        const bool take_and =
            node.type == ClipNodeType::And &&
            ((event.type == 1 && (!has_tail || tail_type == -1) && left_depth > 0 && right_depth > 0) ||
             (event.type == -1 && has_tail && tail_type == 1 && (left_depth == 0 || right_depth == 0)));

        if (take_or || take_and) {
            out_events->push_back(event);
            has_tail = true;
            tail_type = event.type;
        }
    }

    return true;
}

void normalize_arc_angles(double* start, double* end)
{
    if (*end - *start >= 360.0) {
        *start = 0.0;
        *end = 360.0;
        return;
    }

    *start = std::fmod(*start < 0.0 ? 360.0 - std::fmod(-*start, 360.0) : *start, 360.0);
    *end = *start + std::fmod(*end < *start ? 360.0 - std::fmod(*start - *end, 360.0) : *end - *start, 360.0);
}

void arc_init(ClipEllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width, double start, double end)
{
    if (a < b) {
        arc_init(state, b, a, width, 90.0 - end, 90.0 - start);
        ellipse_init(&state->ellipse, a, b, width);
        clip_tree_transpose(state, state->root);
        return;
    }

    ellipse_init(&state->ellipse, a, b, width);
    state->root = -1;
    state->node_count = 0;
    state->events.clear();
    normalize_arc_angles(&start, &end);

    if (end == start + 360.0) {
        return;
    }

    const int left_clip = clip_node(state, ClipNodeType::Clip);
    const int right_clip = clip_node(state, ClipNodeType::Clip);
    ClipNode& left = state->nodes[left_clip];
    ClipNode& right = state->nodes[right_clip];
    left.a = -a * std::sin(start * PILLOW_C_PI / 180.0);
    left.b = b * std::cos(start * PILLOW_C_PI / 180.0);
    left.c = (static_cast<double>(a) * a - static_cast<double>(b) * b) * std::sin(start * PILLOW_C_PI / 90.0) / 2.0;
    right.a = a * std::sin(end * PILLOW_C_PI / 180.0);
    right.b = -b * std::cos(end * PILLOW_C_PI / 180.0);
    right.c = (static_cast<double>(b) * b - static_cast<double>(a) * a) * std::sin(end * PILLOW_C_PI / 90.0) / 2.0;

    if (std::fmod(start, 180.0) == 0.0 || std::fmod(end, 180.0) == 0.0) {
        state->root = clip_node(state, end - start < 180.0 ? ClipNodeType::And : ClipNodeType::Or);
        state->nodes[state->root].left = left_clip;
        state->nodes[state->root].right = right_clip;
    } else if ((static_cast<int>(start / 180.0) + static_cast<int>(end / 180.0)) % 2 == 1) {
        state->root = clip_node(state, ClipNodeType::Or);
        const int left_and = clip_node(state, ClipNodeType::And);
        const int left_half = clip_node(state, ClipNodeType::Clip);
        const int right_and = clip_node(state, ClipNodeType::And);
        const int right_half = clip_node(state, ClipNodeType::Clip);
        state->nodes[state->root].left = left_and;
        state->nodes[state->root].right = right_and;
        state->nodes[left_and].left = left_half;
        state->nodes[left_and].right = left_clip;
        state->nodes[right_and].left = right_half;
        state->nodes[right_and].right = right_clip;
        state->nodes[left_half].b = static_cast<int>(start / 180.0) % 2 == 0 ? 1.0 : -1.0;
        state->nodes[right_half].b = static_cast<int>(end / 180.0) % 2 == 0 ? 1.0 : -1.0;
    } else {
        state->root = clip_node(state, end - start < 180.0 ? ClipNodeType::And : ClipNodeType::Or);
        const int combined = clip_node(state, end - start < 180.0 ? ClipNodeType::And : ClipNodeType::Or);
        const int half = clip_node(state, ClipNodeType::Clip);
        state->nodes[state->root].left = combined;
        state->nodes[state->root].right = half;
        state->nodes[combined].left = left_clip;
        state->nodes[combined].right = right_clip;
        state->nodes[half].b = end < 180.0 || end > 540.0 ? 1.0 : -1.0;
    }
}

void chord_init(ClipEllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width, double start, double end)
{
    ellipse_init(&state->ellipse, a, b, width);
    state->root = -1;
    state->node_count = 0;
    state->events.clear();

    const double xl = a * std::cos(start * PILLOW_C_PI / 180.0);
    const double xr = a * std::cos(end * PILLOW_C_PI / 180.0);
    const double yl = b * std::sin(start * PILLOW_C_PI / 180.0);
    const double yr = b * std::sin(end * PILLOW_C_PI / 180.0);
    state->root = clip_node(state, ClipNodeType::Clip);
    ClipNode& root = state->nodes[state->root];
    root.a = yr - yl;
    root.b = xl - xr;
    root.c = -(root.a * xl + root.b * yl);
}

void chord_line_init(ClipEllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width, double start, double end)
{
    ellipse_init(&state->ellipse, a, b, a + b + 1);
    state->root = -1;
    state->node_count = 0;
    state->events.clear();

    const double xl = a * std::cos(start * PILLOW_C_PI / 180.0);
    const double xr = a * std::cos(end * PILLOW_C_PI / 180.0);
    const double yl = b * std::sin(start * PILLOW_C_PI / 180.0);
    const double yr = b * std::sin(end * PILLOW_C_PI / 180.0);

    state->root = clip_node(state, ClipNodeType::And);
    const int left = clip_node(state, ClipNodeType::Clip);
    const int right = clip_node(state, ClipNodeType::Clip);
    state->nodes[state->root].left = left;
    state->nodes[state->root].right = right;

    ClipNode& left_node = state->nodes[left];
    left_node.a = yr - yl;
    left_node.b = xl - xr;
    left_node.c = -(left_node.a * xl + left_node.b * yl);

    ClipNode& right_node = state->nodes[right];
    right_node.a = -left_node.a;
    right_node.b = -left_node.b;
    right_node.c = 2.0 * width * std::sqrt(left_node.a * left_node.a + left_node.b * left_node.b) - left_node.c;
}

void pie_side_init(ClipEllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width, double start, double)
{
    ellipse_init(&state->ellipse, a, b, a + b + 1);
    state->root = -1;
    state->node_count = 0;
    state->events.clear();

    const double x = a * std::cos(start * PILLOW_C_PI / 180.0);
    const double y = b * std::sin(start * PILLOW_C_PI / 180.0);
    const double side_a = -y;
    const double side_b = x;
    const double side_c = width * std::sqrt(side_a * side_a + side_b * side_b);

    state->root = clip_node(state, ClipNodeType::And);
    const int side_and = clip_node(state, ClipNodeType::And);
    const int left = clip_node(state, ClipNodeType::Clip);
    const int right = clip_node(state, ClipNodeType::Clip);
    const int half = clip_node(state, ClipNodeType::Clip);

    state->nodes[state->root].left = side_and;
    state->nodes[state->root].right = half;
    state->nodes[side_and].left = left;
    state->nodes[side_and].right = right;

    state->nodes[left].a = side_a;
    state->nodes[left].b = side_b;
    state->nodes[left].c = side_c;
    state->nodes[right].a = -side_a;
    state->nodes[right].b = -side_b;
    state->nodes[right].c = side_c;
    state->nodes[half].a = side_b;
    state->nodes[half].b = -side_a;
}

void pie_init(ClipEllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width, double start, double end)
{
    ellipse_init(&state->ellipse, a, b, width);
    state->root = -1;
    state->node_count = 0;
    state->events.clear();

    const double xl = a * std::cos(start * PILLOW_C_PI / 180.0);
    const double xr = a * std::cos(end * PILLOW_C_PI / 180.0);
    const double yl = b * std::sin(start * PILLOW_C_PI / 180.0);
    const double yr = b * std::sin(end * PILLOW_C_PI / 180.0);

    const int left_clip = clip_node(state, ClipNodeType::Clip);
    const int right_clip = clip_node(state, ClipNodeType::Clip);
    state->nodes[left_clip].a = -yl;
    state->nodes[left_clip].b = xl;
    state->nodes[right_clip].a = yr;
    state->nodes[right_clip].b = -xr;

    state->root = clip_node(state, end - start < 180.0 ? ClipNodeType::And : ClipNodeType::Or);
    state->nodes[state->root].left = left_clip;
    state->nodes[state->root].right = right_clip;

    if (end - start < 90.0) {
        const int old_root = state->root;
        const int spike_clipper = clip_node(state, ClipNodeType::Clip);
        state->root = clip_node(state, ClipNodeType::And);
        state->nodes[state->root].left = old_root;
        state->nodes[state->root].right = spike_clipper;
        state->nodes[spike_clipper].a = (xl + xr) / 2.0;
        state->nodes[spike_clipper].b = (yl + yr) / 2.0;
    }
}

int clip_ellipse_next(ClipEllipseState* state, std::int32_t* out_x0, std::int32_t* out_y, std::int32_t* out_x1)
{
    std::int32_t x0 = 0;
    std::int32_t y = 0;
    std::int32_t x1 = 0;
    while (state->events.empty() && ellipse_next(&state->ellipse, &x0, &y, &x1)) {
        try {
            if (!clip_tree_do_clip(state, state->root, x0, y, x1, &state->events)) {
                return PILLOW_C_ALLOCATION_FAILED;
            }
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
        state->y = y;
    }

    if (!state->events.empty()) {
        *out_y = state->y;
        const ClipEvent left = state->events.front();
        state->events.erase(state->events.begin());
        const ClipEvent right = state->events.front();
        state->events.erase(state->events.begin());
        *out_x0 = left.x;
        *out_x1 = right.x;
        return PILLOW_C_OK;
    }

    return PILLOW_C_INVALID_LENGTH;
}

int round_up_away_from_zero(float value)
{
    return value >= 0.0f
        ? static_cast<int>(std::floor(value + 0.5f))
        : -static_cast<int>(std::floor(std::fabs(value) + 0.5f));
}

int round_down_toward_zero(float value)
{
    return value >= 0.0f
        ? static_cast<int>(std::ceil(value - 0.5f))
        : -static_cast<int>(std::ceil(std::fabs(value) - 0.5f));
}

void add_polygon_edge(PolygonEdge* edge, int x0, int y0, int x1, int y1)
{
    if (x0 <= x1) {
        edge->xmin = x0;
        edge->xmax = x1;
    } else {
        edge->xmin = x1;
        edge->xmax = x0;
    }

    if (y0 <= y1) {
        edge->ymin = y0;
        edge->ymax = y1;
    } else {
        edge->ymin = y1;
        edge->ymax = y0;
    }

    edge->dx = y0 == y1 ? 0.0f : static_cast<float>(x1 - x0) / static_cast<float>(y1 - y0);
    edge->x0 = x0;
    edge->y0 = y0;
}

bool polygon_mask_allows(const PillowCImage* mask, int x, int y)
{
    if (!mask) {
        return true;
    }
    if (x < 0 || x >= mask->width || y < 0 || y >= mask->height || mask->channels != 1) {
        return false;
    }
    const std::uint8_t* pixel =
        mask->pixels.data() +
        static_cast<std::size_t>(y) * mask->stride +
        static_cast<std::size_t>(x);
    return *pixel != 0;
}

void fill_horizontal_span_masked(
    PillowCImage* image,
    int left,
    int y,
    std::int64_t right_exclusive,
    const std::uint8_t* color,
    const PillowCImage* mask)
{
    if (!mask) {
        fill_horizontal_span(image, left, y, right_exclusive, color);
        return;
    }
    if (y < 0 || y >= image->height || right_exclusive <= 0 || left >= image->width) {
        return;
    }
    int clipped_left = left < 0 ? 0 : left;
    int clipped_right = right_exclusive > image->width ? image->width : static_cast<int>(right_exclusive);
    if (clipped_right <= clipped_left) {
        return;
    }

    std::uint8_t* dst =
        image->pixels.data() +
        static_cast<std::size_t>(y) * image->stride +
        static_cast<std::size_t>(clipped_left) * image->channels;
    for (int x = clipped_left; x < clipped_right; ++x) {
        if (polygon_mask_allows(mask, x, y)) {
            std::memcpy(dst, color, static_cast<std::size_t>(image->channels));
        }
        dst += image->channels;
    }
}

int fill_polygon_edges(
    PillowCImage* image,
    const std::vector<PolygonEdge>& edges,
    const std::uint8_t* color,
    const PillowCImage* mask = nullptr)
{
    if (edges.empty()) {
        return PILLOW_C_OK;
    }

    int ymin = image->height - 1;
    int ymax = 0;
    std::vector<const PolygonEdge*> edge_table;
    try {
        edge_table.reserve(edges.size());
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    for (const PolygonEdge& edge : edges) {
        ymin = std::min(ymin, edge.ymin);
        ymax = std::max(ymax, edge.ymax);
        if (edge.ymin == edge.ymax) {
            fill_horizontal_span_masked(image, edge.xmin, edge.ymin, static_cast<std::int64_t>(edge.xmax) + 1, color, mask);
            continue;
        }
        edge_table.push_back(&edge);
    }

    if (ymin < 0) {
        ymin = 0;
    }
    if (ymax > image->height) {
        ymax = image->height;
    }

    std::vector<float> intersections;
    try {
        intersections.resize(edge_table.size() * 2u);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    for (; ymin <= ymax; ++ymin) {
        std::size_t count = 0;
        for (std::size_t index = 0; index < edge_table.size(); ++index) {
            const PolygonEdge* current = edge_table[index];
            if (ymin < current->ymin || ymin > current->ymax) {
                continue;
            }

            intersections[count++] =
                static_cast<float>(ymin - current->y0) * current->dx +
                static_cast<float>(current->x0);

            if (ymin == current->ymax && ymin < ymax) {
                intersections[count] = intersections[count - 1u];
                ++count;
            } else if ((ymin == current->ymin || ymin == current->ymax) && current->dx != 0.0f) {
                for (std::size_t other_index = 0; other_index < index; ++other_index) {
                    const PolygonEdge* other = edge_table[other_index];
                    if ((ymin != other->ymin && ymin != other->ymax) || other->dx == 0.0f) {
                        continue;
                    }
                    const float other_x =
                        static_cast<float>(ymin - other->y0) * other->dx +
                        static_cast<float>(other->x0);
                    if (std::round(intersections[count - 1u]) != std::round(other_x)) {
                        continue;
                    }

                    const int offset = ymin == current->ymax ? -1 : 1;
                    const float adjacent_current =
                        static_cast<float>(ymin + offset - current->y0) * current->dx +
                        static_cast<float>(current->x0);
                    if (ymin + offset >= other->ymin && ymin + offset <= other->ymax) {
                        const float adjacent_other =
                            static_cast<float>(ymin + offset - other->y0) * other->dx +
                            static_cast<float>(other->x0);
                        if (intersections[count - 1u] > adjacent_current + 1.0f &&
                            intersections[count - 1u] > adjacent_other + 1.0f) {
                            intersections[count - 1u] = std::round(std::max(adjacent_current, adjacent_other)) + 1.0f;
                        } else if (intersections[count - 1u] < adjacent_current - 1.0f &&
                                   intersections[count - 1u] < adjacent_other - 1.0f) {
                            intersections[count - 1u] = std::round(std::min(adjacent_current, adjacent_other)) - 1.0f;
                        }
                        break;
                    }
                }
            }
        }

        std::sort(intersections.begin(), intersections.begin() + static_cast<std::ptrdiff_t>(count));
        for (std::size_t index = 1; index < count; index += 2u) {
            fill_horizontal_span_masked(
                image,
                round_up_away_from_zero(intersections[index - 1u]),
                ymin,
                static_cast<std::int64_t>(round_down_toward_zero(intersections[index])) + 1,
                color,
                mask);
        }
    }

    return PILLOW_C_OK;
}

void draw_point_image(PillowCImage* image, int x, int y, const std::uint8_t* color)
{
    if (x < 0 || x >= image->width || y < 0 || y >= image->height) {
        return;
    }
    std::uint8_t* dst =
        image->pixels.data() +
        static_cast<std::size_t>(y) * image->stride +
        static_cast<std::size_t>(x) * image->channels;
    std::memcpy(dst, color, static_cast<std::size_t>(image->channels));
}

int draw_pieslice_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width);

void draw_line_segment_image(
    PillowCImage* image,
    int x0,
    int y0,
    int x1,
    int y1,
    const std::uint8_t* color)
{
    int dx = x1 - x0;
    int xs = 1;
    if (dx < 0) {
        dx = -dx;
        xs = -1;
    }
    int dy = y1 - y0;
    int ys = 1;
    if (dy < 0) {
        dy = -dy;
        ys = -1;
    }

    if (dx == 0) {
        for (int i = 0; i < dy; ++i) {
            draw_point_image(image, x0, y0, color);
            y0 += ys;
        }
    } else if (dy == 0) {
        for (int i = 0; i < dx; ++i) {
            draw_point_image(image, x0, y0, color);
            x0 += xs;
        }
    } else if (dx > dy) {
        const int n = dx;
        dy += dy;
        int e = dy - dx;
        dx += dx;
        for (int i = 0; i < n; ++i) {
            draw_point_image(image, x0, y0, color);
            if (e >= 0) {
                y0 += ys;
                e -= dx;
            }
            e += dy;
            x0 += xs;
        }
    } else {
        const int n = dy;
        dx += dx;
        int e = dx - dy;
        dy += dy;
        for (int i = 0; i < n; ++i) {
            draw_point_image(image, x0, y0, color);
            if (e >= 0) {
                x0 += xs;
                e -= dy;
            }
            e += dx;
            y0 += ys;
        }
    }
}

int draw_wide_line_segment_image(
    PillowCImage* image,
    int x0,
    int y0,
    int x1,
    int y1,
    const std::uint8_t* color,
    int width,
    const PillowCImage* mask = nullptr)
{
    const int dx = x1 - x0;
    const int dy = y1 - y0;
    if (dx == 0 && dy == 0) {
        if (polygon_mask_allows(mask, x0, y0)) {
            draw_point_image(image, x0, y0, color);
        }
        return PILLOW_C_OK;
    }

    const double big_hypotenuse = std::hypot(static_cast<double>(dx), static_cast<double>(dy));
    const double small_hypotenuse = (width - 1) / 2.0;
    const double ratio_max = static_cast<double>(round_up_away_from_zero(static_cast<float>(small_hypotenuse))) / big_hypotenuse;
    const double ratio_min = static_cast<double>(round_down_toward_zero(static_cast<float>(small_hypotenuse))) / big_hypotenuse;

    const int dxmin = round_down_toward_zero(static_cast<float>(ratio_min * dy));
    const int dxmax = round_down_toward_zero(static_cast<float>(ratio_max * dy));
    const int dymin = round_down_toward_zero(static_cast<float>(ratio_min * dx));
    const int dymax = round_down_toward_zero(static_cast<float>(ratio_max * dx));

    const int vertices[4][2] = {
        {x0 - dxmin, y0 + dymax},
        {x1 - dxmin, y1 + dymax},
        {x1 + dxmax, y1 - dymin},
        {x0 + dxmax, y0 - dymin},
    };

    std::vector<PolygonEdge> edges;
    try {
        edges.resize(4);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    add_polygon_edge(&edges[0], vertices[0][0], vertices[0][1], vertices[1][0], vertices[1][1]);
    add_polygon_edge(&edges[1], vertices[1][0], vertices[1][1], vertices[2][0], vertices[2][1]);
    add_polygon_edge(&edges[2], vertices[2][0], vertices[2][1], vertices[3][0], vertices[3][1]);
    add_polygon_edge(&edges[3], vertices[3][0], vertices[3][1], vertices[0][0], vertices[0][1]);
    return fill_polygon_edges(image, edges, color, mask);
}

int draw_line_image(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size,
    int width)
{
    if (!image || !points || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (point_count < 2 || point_count > static_cast<std::size_t>(INT_MAX)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    for (std::size_t index = 0; index + 1 < point_count; ++index) {
        const int* current = points + index * 2u;
        if (width <= 1) {
            draw_line_segment_image(image, current[0], current[1], current[2], current[3], color);
        } else {
            const int status = draw_wide_line_segment_image(image, current[0], current[1], current[2], current[3], color, width);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }
    }
    if (width <= 1) {
        const int* last = points + (point_count - 1u) * 2u;
        draw_point_image(image, last[0], last[1], color);
    }
    return PILLOW_C_OK;
}

double line_joint_angle_degrees(const int* start, const int* end)
{
    double angle =
        std::atan2(
            static_cast<double>(end[0] - start[0]),
            static_cast<double>(start[1] - end[1])) *
        180.0 / PILLOW_C_PI;
    angle = std::fmod(angle, 360.0);
    if (angle < 0.0) {
        angle += 360.0;
    }
    return angle;
}

int line_joint_coordinate_delta(double delta)
{
    return static_cast<int>(delta > 0.0 ? std::floor(delta) : std::ceil(delta));
}

int line_joint_x_at_angle(int x, double angle, int width)
{
    angle -= 90.0;
    const double distance = static_cast<double>(width) / 2.0 - 1.0;
    const double delta = distance * std::cos(angle * PILLOW_C_PI / 180.0);
    return x + line_joint_coordinate_delta(delta);
}

int line_joint_y_at_angle(int y, double angle, int width)
{
    angle -= 90.0;
    const double distance = static_cast<double>(width) / 2.0 - 1.0;
    const double delta = distance * std::sin(angle * PILLOW_C_PI / 180.0);
    return y + line_joint_coordinate_delta(delta);
}

int draw_line_curve_joints_image(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size,
    int width)
{
    for (std::size_t index = 1; index + 1u < point_count; ++index) {
        const int* previous = points + (index - 1u) * 2u;
        const int* point = points + index * 2u;
        const int* next = points + (index + 1u) * 2u;

        const double previous_angle = line_joint_angle_degrees(previous, point);
        const double next_angle = line_joint_angle_degrees(point, next);
        if (previous_angle == next_angle) {
            continue;
        }

        const bool flipped =
            (next_angle > previous_angle && next_angle - 180.0 > previous_angle) ||
            (next_angle < previous_angle && next_angle + 180.0 > previous_angle);

        const double half_width = static_cast<double>(width) / 2.0;
        const int left = static_cast<int>(static_cast<double>(point[0]) - half_width + 1.0);
        const int top = static_cast<int>(static_cast<double>(point[1]) - half_width + 1.0);
        const int right = static_cast<int>(static_cast<double>(point[0]) + half_width - 1.0);
        const int bottom = static_cast<int>(static_cast<double>(point[1]) + half_width - 1.0);

        double start = 0.0;
        double end = 0.0;
        if (flipped) {
            start = next_angle + 90.0;
            end = previous_angle + 90.0;
        } else {
            start = previous_angle - 90.0;
            end = next_angle - 90.0;
        }

        int status = draw_pieslice_image(
            image,
            left,
            top,
            right,
            bottom,
            start - 90.0,
            end - 90.0,
            color,
            color_size,
            nullptr,
            0,
            1);
        if (status != PILLOW_C_OK) {
            return status;
        }

        if (width > 8) {
            int gap_points[6] = {};
            if (flipped) {
                gap_points[0] = line_joint_x_at_angle(point[0], previous_angle + 90.0, width);
                gap_points[1] = line_joint_y_at_angle(point[1], previous_angle + 90.0, width);
                gap_points[4] = line_joint_x_at_angle(point[0], next_angle + 90.0, width);
                gap_points[5] = line_joint_y_at_angle(point[1], next_angle + 90.0, width);
            } else {
                gap_points[0] = line_joint_x_at_angle(point[0], previous_angle - 90.0, width);
                gap_points[1] = line_joint_y_at_angle(point[1], previous_angle - 90.0, width);
                gap_points[4] = line_joint_x_at_angle(point[0], next_angle - 90.0, width);
                gap_points[5] = line_joint_y_at_angle(point[1], next_angle - 90.0, width);
            }
            gap_points[2] = point[0];
            gap_points[3] = point[1];

            status = draw_line_image(image, gap_points, 3, color, color_size, 3);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }
    }

    return PILLOW_C_OK;
}

int draw_line_joint_image(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size,
    int width,
    int joint_curve)
{
    const int status = draw_line_image(image, points, point_count, color, color_size, width);
    if (status != PILLOW_C_OK || !joint_curve || width <= 4 || point_count < 3 || !image || image->pixels.empty()) {
        return status;
    }
    return draw_line_curve_joints_image(image, points, point_count, color, color_size, width);
}

int draw_points_image(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size)
{
    if (!image || !color || (!points && point_count != 0)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (point_count > static_cast<std::size_t>(INT_MAX)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    for (std::size_t index = 0; index < point_count; ++index) {
        const int* current = points + index * 2u;
        draw_point_image(image, current[0], current[1], color);
    }
    return PILLOW_C_OK;
}

int build_polygon_edges(const int* points, std::size_t point_count, std::vector<PolygonEdge>* edges)
{
    try {
        edges->resize(point_count);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    std::size_t edge_count = 0;
    for (std::size_t index = 0; index + 1u < point_count; ++index) {
        const int* current = points + index * 2u;
        const int* next = current + 2;
        if (current[1] == next[1] && index != 0 && current[1] == points[index * 2u - 1u]) {
            PolygonEdge* last = &(*edges)[edge_count - 1u];
            if (next[0] > current[0] && current[0] > points[index * 2u - 2u]) {
                last->xmax = next[0];
                continue;
            }
            if (next[0] < current[0] && current[0] < points[index * 2u - 2u]) {
                last->xmin = next[0];
                continue;
            }
        }
        add_polygon_edge(&(*edges)[edge_count++], current[0], current[1], next[0], next[1]);
    }

    const int* last = points + (point_count - 1u) * 2u;
    if (last[0] != points[0] || last[1] != points[1]) {
        add_polygon_edge(&(*edges)[edge_count++], last[0], last[1], points[0], points[1]);
    }
    edges->resize(edge_count);
    return PILLOW_C_OK;
}

int draw_polygon_image(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    if (!image || !points) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (point_count < 2 || point_count > static_cast<std::size_t>(INT_MAX)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (outline_size != 0 && width > 1 && width > INT_MAX / 2) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    if (fill_size != 0) {
        std::vector<PolygonEdge> edges;
        const int edge_status = build_polygon_edges(points, point_count, &edges);
        if (edge_status != PILLOW_C_OK) {
            return edge_status;
        }
        const int fill_status = fill_polygon_edges(image, edges, fill);
        if (fill_status != PILLOW_C_OK) {
            return fill_status;
        }
    }

    if (outline_size != 0 && width != 0) {
        if (width <= 1) {
            for (std::size_t index = 0; index + 1u < point_count; ++index) {
                const int* current = points + index * 2u;
                draw_line_segment_image(image, current[0], current[1], current[2], current[3], outline);
            }
            const int* last = points + (point_count - 1u) * 2u;
            draw_line_segment_image(image, last[0], last[1], points[0], points[1], outline);
            draw_point_image(image, points[0], points[1], outline);
        } else {
            std::size_t mask_stride = 0;
            std::size_t mask_size = 0;
            if (!checked_image_size_allow_empty(image->width, image->height, 1, &mask_stride, &mask_size)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            try {
                PillowCImage mask{
                    image->width,
                    image->height,
                    PILLOW_C_MODE_1,
                    1,
                    mask_stride,
                    std::vector<std::uint8_t>(mask_size)};
                std::vector<PolygonEdge> mask_edges;
                const int edge_status = build_polygon_edges(points, point_count, &mask_edges);
                if (edge_status != PILLOW_C_OK) {
                    return edge_status;
                }
                const std::uint8_t mask_value = 255;
                const int mask_status = fill_polygon_edges(&mask, mask_edges, &mask_value);
                if (mask_status != PILLOW_C_OK) {
                    return mask_status;
                }

                const int wide_width = width * 2 - 1;
                for (std::size_t index = 0; index + 1u < point_count; ++index) {
                    const int* current = points + index * 2u;
                    const int status = draw_wide_line_segment_image(
                        image,
                        current[0],
                        current[1],
                        current[2],
                        current[3],
                        outline,
                        wide_width,
                        &mask);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                }
                const int* last = points + (point_count - 1u) * 2u;
                const int status = draw_wide_line_segment_image(
                    image,
                    last[0],
                    last[1],
                    points[0],
                    points[1],
                    outline,
                    wide_width,
                    &mask);
                if (status != PILLOW_C_OK) {
                    return status;
                }
            } catch (const std::bad_alloc&) {
                return PILLOW_C_ALLOCATION_FAILED;
            }
        }
    }

    return PILLOW_C_OK;
}

int draw_rectangle_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::int64_t x0 = left;
    const std::int64_t y0 = top;
    const std::int64_t x1 = right;
    const std::int64_t y1 = bottom;
    if (fill_size != 0) {
        fill_rectangle_region(image, x0, y0, x1 + 1, y1 + 1, fill);
    }
    if (outline_size == 0 || width <= 0) {
        return PILLOW_C_OK;
    }

    for (int inset = 0; inset < width; ++inset) {
        fill_horizontal_span(image, x0, y0 + inset, x1 + 1, outline);
        fill_horizontal_span(image, x0, y1 - inset, x1 + 1, outline);
        fill_vertical_span(image, x1 - inset, y0 + width, y1 - width + 2, outline);
        fill_vertical_span(image, x0 + inset, y0 + width, y1 - width + 2, outline);
    }
    return PILLOW_C_OK;
}

int draw_ellipse_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int span_width = right - left;
    const int span_height = bottom - top;
    if (fill_size != 0) {
        draw_ellipse_spans(image, left, top, right, bottom, fill, span_width + span_height);
    }
    if (outline_size != 0 && width != 0) {
        draw_ellipse_spans(image, left, top, right, bottom, outline, width);
    }
    return PILLOW_C_OK;
}

int draw_clip_ellipse_spans(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* color,
    void (*init)(ClipEllipseState*, std::int32_t, std::int32_t, std::int32_t, double, double),
    int width,
    double start,
    double end)
{
    const int a = right - left;
    const int b = bottom - top;
    ClipEllipseState state{};
    try {
        state.events.reserve(8);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    init(&state, a, b, width, start, end);

    std::int32_t x0 = 0;
    std::int32_t y = 0;
    std::int32_t x1 = 0;
    while (true) {
        const int next_status = clip_ellipse_next(&state, &x0, &y, &x1);
        if (next_status == PILLOW_C_INVALID_LENGTH) {
            return PILLOW_C_OK;
        }
        if (next_status != PILLOW_C_OK) {
            return next_status;
        }
        fill_horizontal_span(
            image,
            static_cast<std::int64_t>(left) + (static_cast<std::int64_t>(x0) + a) / 2,
            static_cast<std::int64_t>(top) + (static_cast<std::int64_t>(y) + b) / 2,
            static_cast<std::int64_t>(left) + (static_cast<std::int64_t>(x1) + a) / 2 + 1,
            color);
    }
}

int draw_arc_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* color,
    std::size_t color_size,
    int width)
{
    if (!image || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top || !std::isfinite(start) || !std::isfinite(end)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty() || width <= 0) {
        return PILLOW_C_OK;
    }

    normalize_arc_angles(&start, &end);
    if (start + 360.0 == end) {
        return draw_ellipse_image(image, left, top, right, bottom, nullptr, 0, color, color_size, width);
    }
    if (start == end) {
        return PILLOW_C_OK;
    }

    return draw_clip_ellipse_spans(image, left, top, right, bottom, color, arc_init, width, start, end);
}

int draw_chord_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top || !std::isfinite(start) || !std::isfinite(end)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    normalize_arc_angles(&start, &end);
    if (start + 360.0 == end) {
        return draw_ellipse_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
    }
    if (start == end) {
        return PILLOW_C_OK;
    }

    if (fill_size != 0) {
        const int fill_width = right - left + bottom - top + 1;
        const int status = draw_clip_ellipse_spans(image, left, top, right, bottom, fill, chord_init, fill_width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    if (outline_size != 0 && width != 0) {
        int status = draw_clip_ellipse_spans(image, left, top, right, bottom, outline, chord_line_init, width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
        status = draw_clip_ellipse_spans(image, left, top, right, bottom, outline, chord_init, width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    return PILLOW_C_OK;
}

int draw_pieslice_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top || !std::isfinite(start) || !std::isfinite(end)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    normalize_arc_angles(&start, &end);
    if (start + 360.0 == end) {
        return draw_ellipse_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
    }
    if (start == end) {
        return PILLOW_C_OK;
    }

    if (fill_size != 0) {
        const int fill_width = right + bottom - left - top;
        const int status = draw_clip_ellipse_spans(image, left, top, right, bottom, fill, pie_init, fill_width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    if (outline_size != 0 && width != 0) {
        int status = draw_clip_ellipse_spans(image, left, top, right, bottom, outline, pie_side_init, width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
        status = draw_clip_ellipse_spans(image, left, top, right, bottom, outline, pie_side_init, width, end, 0.0);
        if (status != PILLOW_C_OK) {
            return status;
        }
        const int center_left = static_cast<int>(std::lround((left + right - width) / 2.0));
        const int center_top = static_cast<int>(std::lround((top + bottom - width) / 2.0));
        status = draw_ellipse_image(
            image,
            center_left,
            center_top,
            center_left + width - 1,
            center_top + width - 1,
            outline,
            outline_size,
            nullptr,
            0,
            0);
        if (status != PILLOW_C_OK) {
            return status;
        }
        status = draw_clip_ellipse_spans(image, left, top, right, bottom, outline, pie_init, width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    return PILLOW_C_OK;
}

void fill_rounded_rectangle_bar(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* color)
{
    fill_rectangle_region(
        image,
        left,
        top,
        static_cast<std::int64_t>(right) + 1,
        static_cast<std::int64_t>(bottom) + 1,
        color);
}

int draw_rounded_rectangle_part(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* color,
    std::size_t color_size,
    int width,
    bool fill)
{
    return fill
        ? draw_pieslice_image(image, left, top, right, bottom, start, end, color, color_size, nullptr, 0, 1)
        : draw_arc_image(image, left, top, right, bottom, start, end, color, color_size, width);
}

bool rounded_rectangle_colors_match(
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size)
{
    return fill_size != 0 &&
        outline_size == fill_size &&
        std::memcmp(fill, outline, fill_size) == 0;
}

int draw_rounded_rectangle_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double radius,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width,
    int corners_mask)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top || !std::isfinite(radius) || radius < 0.0 || corners_mask < 0 || corners_mask > 15) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const bool corners[4] = {
        (corners_mask & 1) != 0,
        (corners_mask & 2) != 0,
        (corners_mask & 4) != 0,
        (corners_mask & 8) != 0,
    };
    const bool any_corners = corners_mask != 0;
    const bool all_corners = corners_mask == 15;

    double diameter = radius * 2.0;
    bool full_x = false;
    bool full_y = false;
    if (all_corners) {
        full_x = diameter >= static_cast<double>(right - left - 1);
        if (full_x) {
            diameter = static_cast<double>(right - left);
        }
        full_y = diameter >= static_cast<double>(bottom - top - 1);
        if (full_y) {
            diameter = static_cast<double>(bottom - top);
        }
        if (full_x && full_y) {
            return draw_ellipse_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
        }
    }

    if (diameter == 0.0 || !any_corners) {
        return draw_rectangle_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
    }

    const int d = static_cast<int>(std::floor(diameter));
    const int r = static_cast<int>(std::floor(diameter / 2.0));

    if (fill_size != 0) {
        if (full_x) {
            int status = draw_rounded_rectangle_part(
                image, left, top, left + d, top + d, 180.0, 360.0, fill, fill_size, 1, true);
            if (status != PILLOW_C_OK) {
                return status;
            }
            status = draw_rounded_rectangle_part(
                image, left, bottom - d, left + d, bottom, 0.0, 180.0, fill, fill_size, 1, true);
            if (status != PILLOW_C_OK) {
                return status;
            }
            fill_rounded_rectangle_bar(image, left, top + r + 1, right, bottom - r - 1, fill);
        } else if (full_y) {
            int status = draw_rounded_rectangle_part(
                image, left, top, left + d, top + d, 90.0, 270.0, fill, fill_size, 1, true);
            if (status != PILLOW_C_OK) {
                return status;
            }
            status = draw_rounded_rectangle_part(
                image, right - d, top, right, top + d, 270.0, 90.0, fill, fill_size, 1, true);
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (right - r - 1 > left + r + 1) {
                fill_rounded_rectangle_bar(image, left + r + 1, top, right - r - 1, bottom, fill);
            }
        } else {
            int status = PILLOW_C_OK;
            if (corners[0]) {
                status = draw_rounded_rectangle_part(
                    image, left, top, left + d, top + d, 180.0, 270.0, fill, fill_size, 1, true);
            }
            if (status == PILLOW_C_OK && corners[1]) {
                status = draw_rounded_rectangle_part(
                    image, right - d, top, right, top + d, 270.0, 360.0, fill, fill_size, 1, true);
            }
            if (status == PILLOW_C_OK && corners[2]) {
                status = draw_rounded_rectangle_part(
                    image, right - d, bottom - d, right, bottom, 0.0, 90.0, fill, fill_size, 1, true);
            }
            if (status == PILLOW_C_OK && corners[3]) {
                status = draw_rounded_rectangle_part(
                    image, left, bottom - d, left + d, bottom, 90.0, 180.0, fill, fill_size, 1, true);
            }
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (right - r - 1 > left + r + 1) {
                fill_rounded_rectangle_bar(image, left + r + 1, top, right - r - 1, bottom, fill);
            }

            int side_top = top;
            int side_bottom = bottom;
            if (corners[0]) {
                side_top += r + 1;
            }
            if (corners[3]) {
                side_bottom -= r + 1;
            }
            fill_rounded_rectangle_bar(image, left, side_top, left + r, side_bottom, fill);

            side_top = top;
            side_bottom = bottom;
            if (corners[1]) {
                side_top += r + 1;
            }
            if (corners[2]) {
                side_bottom -= r + 1;
            }
            fill_rounded_rectangle_bar(image, right - r, side_top, right, side_bottom, fill);
        }
    }

    if (outline_size == 0 || width == 0 || rounded_rectangle_colors_match(fill, fill_size, outline, outline_size)) {
        return PILLOW_C_OK;
    }

    if (full_x) {
        int status = draw_rounded_rectangle_part(
            image, left, top, left + d, top + d, 180.0, 360.0, outline, outline_size, width, false);
        if (status != PILLOW_C_OK) {
            return status;
        }
        status = draw_rounded_rectangle_part(
            image, left, bottom - d, left + d, bottom, 0.0, 180.0, outline, outline_size, width, false);
        if (status != PILLOW_C_OK) {
            return status;
        }
    } else if (full_y) {
        int status = draw_rounded_rectangle_part(
            image, left, top, left + d, top + d, 90.0, 270.0, outline, outline_size, width, false);
        if (status != PILLOW_C_OK) {
            return status;
        }
        status = draw_rounded_rectangle_part(
            image, right - d, top, right, top + d, 270.0, 90.0, outline, outline_size, width, false);
        if (status != PILLOW_C_OK) {
            return status;
        }
    } else {
        int status = PILLOW_C_OK;
        if (corners[0]) {
            status = draw_rounded_rectangle_part(
                image, left, top, left + d, top + d, 180.0, 270.0, outline, outline_size, width, false);
        }
        if (status == PILLOW_C_OK && corners[1]) {
            status = draw_rounded_rectangle_part(
                image, right - d, top, right, top + d, 270.0, 360.0, outline, outline_size, width, false);
        }
        if (status == PILLOW_C_OK && corners[2]) {
            status = draw_rounded_rectangle_part(
                image, right - d, bottom - d, right, bottom, 0.0, 90.0, outline, outline_size, width, false);
        }
        if (status == PILLOW_C_OK && corners[3]) {
            status = draw_rounded_rectangle_part(
                image, left, bottom - d, left + d, bottom, 90.0, 180.0, outline, outline_size, width, false);
        }
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    if (!full_x) {
        int edge_left = left;
        int edge_right = right;
        if (corners[0]) {
            edge_left += r + 1;
        }
        if (corners[1]) {
            edge_right -= r + 1;
        }
        fill_rounded_rectangle_bar(image, edge_left, top, edge_right, top + width - 1, outline);

        edge_left = left;
        edge_right = right;
        if (corners[3]) {
            edge_left += r + 1;
        }
        if (corners[2]) {
            edge_right -= r + 1;
        }
        fill_rounded_rectangle_bar(image, edge_left, bottom - width + 1, edge_right, bottom, outline);
    }
    if (!full_y) {
        int edge_top = top;
        int edge_bottom = bottom;
        if (corners[0]) {
            edge_top += r + 1;
        }
        if (corners[3]) {
            edge_bottom -= r + 1;
        }
        fill_rounded_rectangle_bar(image, left, edge_top, left + width - 1, edge_bottom, outline);

        edge_top = top;
        edge_bottom = bottom;
        if (corners[1]) {
            edge_top += r + 1;
        }
        if (corners[2]) {
            edge_bottom -= r + 1;
        }
        fill_rounded_rectangle_bar(image, right - width + 1, edge_top, right, edge_bottom, outline);
    }

    return PILLOW_C_OK;
}

int draw_bitmap_image(
    PillowCImage* image,
    int left,
    int top,
    const PillowCImage* mask,
    const std::uint8_t* color,
    std::size_t color_size)
{
    if (!image || !mask || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!supported_bitmap_mask(mask)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty() || mask->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::int64_t dst_left_i64 = left < 0 ? 0 : left;
    const std::int64_t dst_top_i64 = top < 0 ? 0 : top;
    const std::int64_t mask_right_i64 = static_cast<std::int64_t>(left) + mask->width;
    const std::int64_t mask_bottom_i64 = static_cast<std::int64_t>(top) + mask->height;
    const std::int64_t dst_right_i64 = mask_right_i64 > image->width ? image->width : mask_right_i64;
    const std::int64_t dst_bottom_i64 = mask_bottom_i64 > image->height ? image->height : mask_bottom_i64;
    if (dst_right_i64 <= dst_left_i64 || dst_bottom_i64 <= dst_top_i64) {
        return PILLOW_C_OK;
    }

    const int dst_left = static_cast<int>(dst_left_i64);
    const int dst_top = static_cast<int>(dst_top_i64);
    const int dst_right = static_cast<int>(dst_right_i64);
    const int dst_bottom = static_cast<int>(dst_bottom_i64);
    const int src_left = dst_left - left;
    const int src_top = dst_top - top;
    const int channels = image->channels;

    for (int y = 0; y < dst_bottom - dst_top; ++y) {
        const std::uint8_t* mask_row =
            mask->pixels.data() +
            static_cast<std::size_t>(src_top + y) * mask->stride +
            static_cast<std::size_t>(src_left) * mask->channels;
        std::uint8_t* dst_row =
            image->pixels.data() +
            static_cast<std::size_t>(dst_top + y) * image->stride +
            static_cast<std::size_t>(dst_left) * image->channels;

        for (int x = 0; x < dst_right - dst_left; ++x) {
            const std::uint8_t alpha = mask_alpha_at(mask, mask_row, x);
            if (alpha == 0) {
                continue;
            }

            const std::size_t pixel_offset = static_cast<std::size_t>(x) * channels;
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
            const std::uint8_t alpha = mask_alpha_at(mask, mask_row, x);
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
        copy_palette_if_same_mode(source, target);
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
    copy_palette_if_same_mode(source, target);
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
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (source->mode == PILLOW_C_MODE_1 && source->channels == 1) {
        if (!image_shape_matches(target, source)) {
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
int histogram_image_masked(
    const PillowCImage* source,
    const PillowCImage* mask,
    std::uint64_t* out_histogram,
    std::size_t out_count);
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
    if (!image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
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
    if (!image_shape_matches(target, source->width, source->height, target_mode, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        copy_palette_if_same_mode(source, target);
        return PILLOW_C_OK;
    }

    const std::uint8_t* src = source->pixels.data() + channel_index;
    std::uint8_t* dst = target->pixels.data();
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    for (std::size_t i = 0; i < pixels; ++i) {
        dst[i] = src[i * source->channels];
    }
    copy_palette_if_same_mode(source, target);
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
    if (!image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
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
    if (!image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
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
    if (source->mode != PILLOW_C_MODE_P || source->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int target_channels = channels_for_mode(target_mode);
    if (target_channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    for (std::size_t i = 0; i < pixels; ++i) {
        std::uint8_t rgb[3]{};
        palette_rgb_at(source, source->pixels[i], rgb);
        if (target_mode == PILLOW_C_MODE_RGB) {
            std::uint8_t* dst = target->pixels.data() + i * 3u;
            dst[0] = rgb[0];
            dst[1] = rgb[1];
            dst[2] = rgb[2];
        } else if (target_mode == PILLOW_C_MODE_RGBA) {
            std::uint8_t* dst = target->pixels.data() + i * 4u;
            dst[0] = rgb[0];
            dst[1] = rgb[1];
            dst[2] = rgb[2];
            dst[3] = 255;
        } else if (target_mode == PILLOW_C_MODE_L) {
            target->pixels[i] = rgb_luma_u8(rgb);
        } else if (target_mode == PILLOW_C_MODE_CMYK) {
            std::uint8_t* dst = target->pixels.data() + i * 4u;
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
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
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
    if (!image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->mode == target_mode) {
        if (!source->pixels.empty()) {
            std::memcpy(target->pixels.data(), source->pixels.data(), source->pixels.size());
        }
        target->palette_rgb = source->palette_rgb;
        return PILLOW_C_OK;
    }
    if (source->pixels.empty()) {
        if (source->mode == target_mode) {
            target->palette_rgb = source->palette_rgb;
        }
        return PILLOW_C_OK;
    }

    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    if (source->mode == PILLOW_C_MODE_P) {
        return convert_palette_image_into(source, target_mode, target);
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
            } else if (target_mode == PILLOW_C_MODE_RGBA) {
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
            } else if (source->mode == PILLOW_C_MODE_RGB) {
                const std::uint8_t* src = source->pixels.data() + i * 3u;
                dst[0] = static_cast<std::uint8_t>(255u - src[0]);
                dst[1] = static_cast<std::uint8_t>(255u - src[1]);
                dst[2] = static_cast<std::uint8_t>(255u - src[2]);
                dst[3] = 0;
            } else if (source->mode == PILLOW_C_MODE_RGBA) {
                const std::uint8_t* src = source->pixels.data() + i * 4u;
                dst[0] = static_cast<std::uint8_t>(255u - src[0]);
                dst[1] = static_cast<std::uint8_t>(255u - src[1]);
                dst[2] = static_cast<std::uint8_t>(255u - src[2]);
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

    if (target_mode == PILLOW_C_MODE_L && (source->mode == PILLOW_C_MODE_RGB || source->mode == PILLOW_C_MODE_RGBA)) {
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
            dst[3] = 255;
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

    if (source->mode == PILLOW_C_MODE_RGB && target_mode == PILLOW_C_MODE_LA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            const std::uint8_t* src = source->pixels.data() + i * 3;
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

int convert_image_to_mode1_floyd_steinberg_into(const PillowCImage* source, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_1, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        return PILLOW_C_OK;
    }
    if (source->mode != PILLOW_C_MODE_L &&
        source->mode != PILLOW_C_MODE_LA &&
        source->mode != PILLOW_C_MODE_RGB &&
        source->mode != PILLOW_C_MODE_RGBA &&
        source->mode != PILLOW_C_MODE_CMYK) {
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
                } else if (source->mode == PILLOW_C_MODE_CMYK) {
                    value = cmyk_luma_u8(src + static_cast<std::size_t>(x) * source->channels);
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
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_1, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
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
    if (source->mode == PILLOW_C_MODE_RGB) {
        for (std::size_t i = 0; i < pixels; ++i) {
            dst[i] = rgb_luma_1000(src + i * 3u) >= 128000 ? 255u : 0u;
        }
        return PILLOW_C_OK;
    }
    if (source->mode == PILLOW_C_MODE_RGBA) {
        for (std::size_t i = 0; i < pixels; ++i) {
            dst[i] = rgb_luma_1000(src + i * 4u) >= 128000 ? 255u : 0u;
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
    if (!statistics_mask_matches(mask, source->width, source->height)) {
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
    if (mask && !statistics_mask_matches(mask, source->width, source->height)) {
        return PILLOW_C_MISMATCH;
    }

    std::vector<std::uint64_t> histogram(static_cast<std::size_t>(source->channels) * 256u, 0);
    const std::size_t pixels = static_cast<std::size_t>(source->width) * source->height;
    const std::size_t samples = pixels * static_cast<std::size_t>(source->channels);
    const std::uint8_t* data = source->pixels.data();
    const std::uint8_t* mask_data = mask ? mask->pixels.data() : nullptr;
    std::uint64_t total = 0;

    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        if (mask_data && mask_data[pixel] == 0) {
            continue;
        }
        const std::uint8_t* src = data + pixel * static_cast<std::size_t>(source->channels);
        for (int channel = 0; channel < source->channels; ++channel) {
            ++histogram[static_cast<std::size_t>(channel) * 256u + src[channel]];
            ++total;
        }
    }

    if (!mask && samples == 0) {
        *out_entropy = std::numeric_limits<double>::quiet_NaN();
        return PILLOW_C_OK;
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
    if (mask && !statistics_mask_matches(mask, source->width, source->height)) {
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
        return mul_div_255(px[channel], px[alpha_channel]);
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
            return mul_div_255(px[channel], px[alpha_channel]);
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
    if (is_premultiplied_alpha_mode(source)) {
        const int alpha_channel = alpha_channel_index(source);
        const std::uint8_t alpha = values[alpha_channel];
        for (int channel = 0; channel < alpha_channel; ++channel) {
            dst[channel] = (alpha == 0 || alpha == 255)
                ? values[channel]
                : clip_u8_int(255 * static_cast<int>(values[channel]) / alpha);
        }
        dst[alpha_channel] = alpha;
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
        copy_palette_if_same_mode(source, target);
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
        copy_palette_if_same_mode(source, target);
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
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }

    std::uint8_t fill[4]{0, 0, 0, 0};
    int status = normalize_transform_fill(source, fill_color, fill_color_size, fill);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (target->pixels.empty()) {
        copy_palette_if_same_mode(source, target);
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
                    ? bilinear_transform_channel(source, source_x, source_y, channel, is_premultiplied_alpha_mode(source))
                    : bicubic_transform_channel(source, source_x, source_y, channel, is_premultiplied_alpha_mode(source));
            }
            write_transform_values(source, values, dst);
        }
    }
    copy_palette_if_same_mode(source, target);
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
                        ? bilinear_transform_channel(source, source_x, source_y, channel, is_premultiplied_alpha_mode(source))
                        : bicubic_transform_channel(source, source_x, source_y, channel, is_premultiplied_alpha_mode(source));
                }
                write_transform_values(source, values, dst);
            }
        }
    }
    copy_palette_if_same_mode(source, target);
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
    const bool alpha_mode = source->mode == PILLOW_C_MODE_LA || source->mode == PILLOW_C_MODE_RGBA;
    const int alpha_channel = source->channels - 1;
    if (alpha_mode && channel < alpha_channel) {
        return mul_div_255(px[channel], px[alpha_channel]);
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
                if (source->mode == PILLOW_C_MODE_LA || source->mode == PILLOW_C_MODE_RGBA) {
                    const int alpha_channel = source->channels - 1;
                    const std::uint8_t alpha = values[alpha_channel];
                    for (int channel = 0; channel < alpha_channel; ++channel) {
                        const std::uint8_t premultiplied = values[channel];
                        dst[channel] = (alpha == 0 || alpha == 255)
                            ? premultiplied
                            : clip_u8_int(255 * static_cast<int>(premultiplied) / alpha);
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
        copy_palette_if_same_mode(source, target);
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
        copy_palette_if_same_mode(source, target);
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
    if (!image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (!valid_box_blur_radius(xradius) || !valid_box_blur_radius(yradius)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (passes <= 0) {
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
    if (!image_shape_matches(target, source)) {
        return PILLOW_C_MISMATCH;
    }
    if (!std::isfinite(radius)) {
        return PILLOW_C_INVALID_ARGUMENT;
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
                target->pixels[index] = clip_u8_int(sharpened);
            } else {
                target->pixels[index] = static_cast<std::uint8_t>(source_value);
            }
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
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
        copy_palette_if_same_mode(source, target);
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
        copy_palette_if_same_mode(source, target);
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
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
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
            copy_palette_if_same_mode(source, target);
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
            source->mode == PILLOW_C_MODE_CMYK);
}

int reduce_output_width(int left, int right, int xscale)
{
    return ceil_div_int(right - left, xscale);
}

int reduce_output_height(int top, int bottom, int yscale)
{
    return ceil_div_int(bottom - top, yscale);
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
    if (!image_shape_matches(target, out_width, out_height, source->mode, source->channels)) {
        return PILLOW_C_MISMATCH;
    }
    if (target->pixels.empty()) {
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
                const std::uint8_t alpha = reduce_average_u8(alpha_sum, count);
                dst[alpha_channel] = alpha;
                for (int channel = 0; channel < alpha_channel; ++channel) {
                    std::uint64_t premultiplied_sum = 0;
                    for (int y = y0; y < y1; ++y) {
                        const std::uint8_t* src_row =
                            source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
                        for (int x = x0; x < x1; ++x) {
                            const std::uint8_t* src =
                                src_row + static_cast<std::size_t>(x) * source->channels;
                            premultiplied_sum += mul_div_255(src[channel], src[alpha_channel]);
                        }
                    }
                    const std::uint8_t premultiplied = reduce_average_u8(premultiplied_sum, count);
                    if (alpha == 0 || alpha == 255) {
                        dst[channel] = premultiplied;
                    } else {
                        dst[channel] = clip_u8_int(255 * static_cast<int>(premultiplied) / alpha);
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
                    dst[channel] = reduce_average_u8(sum, count);
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
    if (std::strcmp(mode_name_text, "P") == 0) {
        *out_mode = PILLOW_C_MODE_P;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "CMYK") == 0) {
        *out_mode = PILLOW_C_MODE_CMYK;
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
    if (!checked_image_size(PILLOW_C_GRADIENT_SIZE, PILLOW_C_GRADIENT_SIZE, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            PILLOW_C_GRADIENT_SIZE,
            PILLOW_C_GRADIENT_SIZE,
            mode,
            1,
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
    if (!checked_image_size(PILLOW_C_GRADIENT_SIZE, PILLOW_C_GRADIENT_SIZE, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            PILLOW_C_GRADIENT_SIZE,
            PILLOW_C_GRADIENT_SIZE,
            mode,
            1,
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

extern "C" __declspec(dllexport) int pillow_c_image_open_bmp(
    const char* path,
    PillowCImage** out_image)
{
    return open_bmp_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_bmp(
    const PillowCImage* image,
    const char* path)
{
    return save_bmp_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_png(
    const char* path,
    PillowCImage** out_image)
{
    return open_png_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png(
    const PillowCImage* image,
    const char* path)
{
    return save_png_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_jpeg(
    const char* path,
    PillowCImage** out_image)
{
    return open_jpeg_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_jpeg(
    const PillowCImage* image,
    const char* path)
{
    return save_jpeg_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_tiff(
    const char* path,
    PillowCImage** out_image)
{
    return open_tiff_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff(
    const PillowCImage* image,
    const char* path)
{
    return save_tiff_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_gif(
    const char* path,
    PillowCImage** out_image)
{
    return open_gif_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif(
    const PillowCImage* image,
    const char* path)
{
    return save_gif_image(image, path);
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

        copy_palette_if_same_mode(source, image);
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

extern "C" __declspec(dllexport) int pillow_c_image_paste_masked(
    PillowCImage* target,
    const PillowCImage* source,
    int left,
    int top,
    const PillowCImage* mask)
{
    return paste_image_masked_into(target, source, left, top, mask);
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
    return paste_color_into(target, color, color_size, left, top, right, bottom, mask);
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
    target->palette_rgb = source->palette_rgb;
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

extern "C" __declspec(dllexport) int pillow_c_image_equalize_masked_into(
    const PillowCImage* source,
    const PillowCImage* mask,
    PillowCImage* target)
{
    return equalize_image_masked_into(source, mask, target);
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

extern "C" __declspec(dllexport) int pillow_c_image_convert_mode_dither_into(
    const PillowCImage* source,
    int target_mode,
    int dither,
    PillowCImage* target)
{
    return convert_image_mode_dither_into(source, target_mode, dither, target);
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

        copy_palette_if_same_mode(source, image);
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
    if (image->mode != PILLOW_C_MODE_P || image->channels != 1 || size % 3u != 0 || size > 768u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        image->palette_rgb.assign(data, data + size);
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
    if (image->mode != PILLOW_C_MODE_P || image->channels != 1) {
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

extern "C" __declspec(dllexport) int pillow_c_image_set_raw_bytes(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size,
    const char* raw_mode,
    int stride,
    int orientation)
{
    return set_raw_bytes_image(image, data, size, raw_mode, stride, orientation);
}

extern "C" __declspec(dllexport) int pillow_c_image_put_data(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size,
    std::size_t pixel_count)
{
    if (!image || (!data && size > 0)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (pixel_count > static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    const std::size_t expected_size = pixel_count * static_cast<std::size_t>(image->channels);
    if (size != expected_size || expected_size > image->pixels.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (expected_size > 0) {
        std::memcpy(image->pixels.data(), data, expected_size);
    }
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

extern "C" __declspec(dllexport) int pillow_c_image_draw_rectangle(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    return draw_rectangle_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_ellipse(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    return draw_ellipse_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_arc(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* color,
    std::size_t color_size,
    int width)
{
    return draw_arc_image(image, left, top, right, bottom, start, end, color, color_size, width);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_chord(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    return draw_chord_image(image, left, top, right, bottom, start, end, fill, fill_size, outline, outline_size, width);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_pieslice(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    return draw_pieslice_image(image, left, top, right, bottom, start, end, fill, fill_size, outline, outline_size, width);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_rounded_rectangle(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double radius,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width,
    int corners_mask)
{
    return draw_rounded_rectangle_image(
        image,
        left,
        top,
        right,
        bottom,
        radius,
        fill,
        fill_size,
        outline,
        outline_size,
        width,
        corners_mask);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_bitmap(
    PillowCImage* image,
    int left,
    int top,
    const PillowCImage* mask,
    const std::uint8_t* color,
    std::size_t color_size)
{
    return draw_bitmap_image(image, left, top, mask, color, color_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_floodfill(
    PillowCImage* image,
    int seed_x,
    int seed_y,
    const std::uint8_t* value,
    std::size_t value_size,
    const std::uint8_t* border,
    std::size_t border_size,
    double thresh)
{
    return draw_floodfill_image(image, seed_x, seed_y, value, value_size, border, border_size, thresh);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_line(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size,
    int width)
{
    return draw_line_image(image, points, point_count, color, color_size, width);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_line_joint(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size,
    int width,
    int joint_curve)
{
    return draw_line_joint_image(image, points, point_count, color, color_size, width, joint_curve);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_points(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size)
{
    return draw_points_image(image, points, point_count, color, color_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_polygon(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    return draw_polygon_image(image, points, point_count, fill, fill_size, outline, outline_size, width);
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

extern "C" __declspec(dllexport) int pillow_c_image_get_raw_bytes(
    const PillowCImage* image,
    const char* raw_mode,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    return get_raw_bytes_image(image, raw_mode, out, out_size, out_required);
}

extern "C" __declspec(dllexport) int pillow_c_image_histogram(
    const PillowCImage* image,
    std::uint64_t* out_histogram,
    std::size_t out_count)
{
    return histogram_image(image, out_histogram, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_histogram_masked(
    const PillowCImage* image,
    const PillowCImage* mask,
    std::uint64_t* out_histogram,
    std::size_t out_count)
{
    return histogram_image_masked(image, mask, out_histogram, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_entropy(
    const PillowCImage* image,
    const PillowCImage* mask,
    double* out_entropy)
{
    return entropy_image(image, mask, out_entropy);
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
        auto* image = new PillowCImage{source->width, source->height, source->mode, source->channels, source->stride, source->pixels, source->palette_rgb};
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
        const int status = resize_image_box_into(source, out_width, out_height, resample, box_left, box_top, box_right, box_bottom, image);
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

    const int effective_resample = resize_resample_for_mode(source, resample);
    if (effective_resample != PILLOW_C_RESAMPLE_NEAREST && !filter_spec_for_resample(effective_resample)) {
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
            std::vector<std::uint8_t>(size),
            source->palette_rgb};
        const int status = resize_image_reducing_gap_into(
            source,
            out_width,
            out_height,
            resample,
            box_left,
            box_top,
            box_right,
            box_bottom,
            reducing_gap,
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
