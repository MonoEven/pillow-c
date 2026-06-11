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
#include <string>
#include <unordered_map>
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
constexpr int PILLOW_C_MODE_I = 8;
constexpr int PILLOW_C_MODE_F = 9;

constexpr int PILLOW_C_PALETTE_ALPHA_NONE = 0;
constexpr int PILLOW_C_PALETTE_ALPHA_RGBA = 1;
constexpr int PILLOW_C_PALETTE_ALPHA_RGBX = 2;

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

constexpr int COLOR_LUT_PRECISION_BITS = 16 - 8 - 2;
constexpr int COLOR_LUT_PRECISION_ROUNDING = 1 << (COLOR_LUT_PRECISION_BITS - 1);
constexpr int COLOR_LUT_SCALE_BITS = 32 - 8 - 6;
constexpr int COLOR_LUT_SCALE_MASK = (1 << COLOR_LUT_SCALE_BITS) - 1;
constexpr int COLOR_LUT_SHIFT_BITS = 16 - 1;

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
};

constexpr int PILLOW_C_FONT_DEFAULT = 1;

constexpr int PILLOW_C_TEXT_ALIGN_LEFT = 0;
constexpr int PILLOW_C_TEXT_ALIGN_CENTER = 1;
constexpr int PILLOW_C_TEXT_ALIGN_RIGHT = 2;
constexpr int PILLOW_C_TEXT_ALIGN_JUSTIFY = 3;
constexpr int PILLOW_C_DEFAULT_FONT_ASCENT = 10;
constexpr int PILLOW_C_DEFAULT_FONT_DESCENT = 3;

struct PillowCFont {
    int kind;
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
    I,
    I32Big,
    I32Native,
    I16Little,
    I16Big,
    I16Native,
    F,
    F32Big,
    F32Native,
    CMYK,
};

struct RawCodecSpec {
    RawCodecKind kind;
    int bytes_per_pixel;
};

struct DefaultFontGlyph {
    unsigned char ch;
    int width;
    int height;
    int offset_x;
    int offset_y;
    int advance;
    int bbox_left;
    int bbox_top;
    int bbox_right;
    int bbox_bottom;
    const std::uint8_t* mask;
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

inline bool native_is_little_endian()
{
    const std::uint16_t value = 1;
    return *reinterpret_cast<const std::uint8_t*>(&value) == 1;
}

inline std::uint16_t read_u16_le(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8);
}

inline std::uint16_t read_u16_be(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[0]) << 8) |
           static_cast<std::uint16_t>(data[1]);
}

inline void write_i32_le(std::uint8_t* data, std::uint32_t value)
{
    data[0] = static_cast<std::uint8_t>(value & 0xFFu);
    data[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    data[2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    data[3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

inline std::int32_t read_i32_le(const std::uint8_t* data)
{
    const std::uint32_t value =
        static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);
    return static_cast<std::int32_t>(value);
}

inline std::uint16_t clip_i32_to_u16(std::int32_t value)
{
    if (value <= 0) {
        return 0;
    }
    if (value >= 65535) {
        return 65535;
    }
    return static_cast<std::uint16_t>(value);
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
    case PILLOW_C_MODE_I:
        return 4;
    case PILLOW_C_MODE_F:
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
    case PILLOW_C_MODE_I:
        return "I";
    case PILLOW_C_MODE_F:
        return "F";
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
    case PILLOW_C_MODE_I:
        if (std::strcmp(raw_mode, "I") == 0 || std::strcmp(raw_mode, "I;32") == 0) {
            return {RawCodecKind::I, 4};
        }
        if (std::strcmp(raw_mode, "I;32B") == 0) {
            return {RawCodecKind::I32Big, 4};
        }
        if (std::strcmp(raw_mode, "I;32N") == 0) {
            return {RawCodecKind::I32Native, 4};
        }
        if (std::strcmp(raw_mode, "I;16") == 0) {
            return {RawCodecKind::I16Little, 2};
        }
        if (std::strcmp(raw_mode, "I;16B") == 0) {
            return {RawCodecKind::I16Big, 2};
        }
        if (std::strcmp(raw_mode, "I;16N") == 0) {
            return {RawCodecKind::I16Native, 2};
        }
        break;
    case PILLOW_C_MODE_F:
        if (std::strcmp(raw_mode, "F") == 0 || std::strcmp(raw_mode, "F;32F") == 0) {
            return {RawCodecKind::F, 4};
        }
        if (std::strcmp(raw_mode, "F;32BF") == 0) {
            return {RawCodecKind::F32Big, 4};
        }
        if (std::strcmp(raw_mode, "F;32NF") == 0) {
            return {RawCodecKind::F32Native, 4};
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
    case PILLOW_C_MODE_I:
        if (std::strcmp(raw_mode, "I") == 0) {
            return {RawCodecKind::I, 4};
        }
        if (std::strcmp(raw_mode, "I;16B") == 0) {
            return {RawCodecKind::I16Big, 2};
        }
        break;
    case PILLOW_C_MODE_F:
        if (std::strcmp(raw_mode, "F") == 0 || std::strcmp(raw_mode, "F;32F") == 0) {
            return {RawCodecKind::F, 4};
        }
        if (std::strcmp(raw_mode, "F;32NF") == 0) {
            return {RawCodecKind::F32Native, 4};
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

void append_be16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
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

struct JpegMetadata {
    int components = 0;
    int exif_orientation = 0;
    bool has_dpi = false;
    double dpi_x = 0.0;
    double dpi_y = 0.0;
    bool has_jfif = false;
    int jfif_major = 0;
    int jfif_minor = 0;
    int jfif_unit = -1;
    int jfif_density_x = 0;
    int jfif_density_y = 0;
};

struct GifMetadata {
    int duration_ms = -1;
    int loop = -1;
    int disposal = 0;
    int background = -1;
    int transparency = -1;
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

bool read_png_dpi_metadata(const char* path, double* out_dpi_x, double* out_dpi_y)
{
    if (!path || !out_dpi_x || !out_dpi_y) {
        return false;
    }
    *out_dpi_x = 0.0;
    *out_dpi_y = 0.0;
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (length == 9u && std::memcmp(type, "pHYs", 4u) == 0) {
            if (payload[8] != 1u) {
                return false;
            }
            *out_dpi_x = static_cast<double>(read_be32(payload)) * 0.0254;
            *out_dpi_y = static_cast<double>(read_be32(payload + 4u)) * 0.0254;
            return true;
        }
        if (std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool skip_gif_sub_blocks(const std::vector<std::uint8_t>& data, std::size_t* pos)
{
    if (!pos) {
        return false;
    }
    while (*pos < data.size()) {
        const std::size_t block_size = data[(*pos)++];
        if (block_size == 0u) {
            return true;
        }
        if (block_size > data.size() - *pos) {
            return false;
        }
        *pos += block_size;
    }
    return false;
}

bool gif_app_extension_is_looping(const std::uint8_t* data, std::size_t size)
{
    return size == 11u &&
           (std::memcmp(data, "NETSCAPE2.0", 11u) == 0 ||
            std::memcmp(data, "ANIMEXTS1.0", 11u) == 0);
}

bool read_gif_metadata(const char* path, int frame_index, GifMetadata* out)
{
    if (!path || frame_index < 0 || !out) {
        return false;
    }
    *out = GifMetadata{};

    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data) || data.size() < 13u) {
        return false;
    }
    if (!(std::memcmp(data.data(), "GIF87a", 6u) == 0 ||
          std::memcmp(data.data(), "GIF89a", 6u) == 0)) {
        return false;
    }

    const std::uint8_t logical_packed = data[10];
    out->background = data[11];

    std::size_t pos = 13u;
    if ((logical_packed & 0x80u) != 0u) {
        const std::size_t global_color_count = std::size_t{1} << ((logical_packed & 0x07u) + 1u);
        const std::size_t global_color_table_size = global_color_count * 3u;
        if (global_color_table_size > data.size() - pos) {
            return false;
        }
        pos += global_color_table_size;
    }

    int pending_duration_cs = -1;
    int pending_disposal = 0;
    int pending_transparency = -1;
    int loop_count = -1;
    int current_frame = 0;

    while (pos < data.size()) {
        const std::uint8_t introducer = data[pos++];
        if (introducer == 0x3bu) {
            return false;
        }
        if (introducer == 0x21u) {
            if (pos >= data.size()) {
                return false;
            }
            const std::uint8_t label = data[pos++];
            if (label == 0xf9u) {
                if (pos >= data.size()) {
                    return false;
                }
                const std::size_t block_size = data[pos++];
                if (block_size < 4u || block_size > data.size() - pos) {
                    return false;
                }
                const std::uint8_t packed = data[pos];
                pending_disposal = (packed >> 2) & 0x07;
                pending_duration_cs = static_cast<int>(read_le16(data.data() + pos + 1u));
                pending_transparency = (packed & 0x01u) != 0u ? static_cast<int>(data[pos + 3u]) : -1;
                pos += block_size;
                if (pos >= data.size() || data[pos] != 0u) {
                    return false;
                }
                ++pos;
            } else if (label == 0xffu) {
                if (pos >= data.size()) {
                    return false;
                }
                const std::size_t app_size = data[pos++];
                if (app_size > data.size() - pos) {
                    return false;
                }
                const std::uint8_t* app = data.data() + pos;
                const bool is_looping_app = gif_app_extension_is_looping(app, app_size);
                pos += app_size;
                bool terminated = false;
                while (pos < data.size()) {
                    const std::size_t block_size = data[pos++];
                    if (block_size == 0u) {
                        terminated = true;
                        break;
                    }
                    if (block_size > data.size() - pos) {
                        return false;
                    }
                    if (is_looping_app && block_size >= 3u && data[pos] == 1u) {
                        loop_count = static_cast<int>(read_le16(data.data() + pos + 1u));
                    }
                    pos += block_size;
                }
                if (!terminated) {
                    return false;
                }
            } else if (!skip_gif_sub_blocks(data, &pos)) {
                return false;
            }
            continue;
        }
        if (introducer != 0x2cu) {
            return false;
        }

        if (9u > data.size() - pos) {
            return false;
        }
        const std::uint8_t image_packed = data[pos + 8u];
        pos += 9u;
        if ((image_packed & 0x80u) != 0u) {
            const std::size_t local_color_count = std::size_t{1} << ((image_packed & 0x07u) + 1u);
            const std::size_t local_color_table_size = local_color_count * 3u;
            if (local_color_table_size > data.size() - pos) {
                return false;
            }
            pos += local_color_table_size;
        }
        if (pos >= data.size()) {
            return false;
        }
        ++pos; // LZW minimum code size.
        if (!skip_gif_sub_blocks(data, &pos)) {
            return false;
        }

        if (current_frame == frame_index) {
            out->duration_ms = pending_duration_cs >= 0 ? pending_duration_cs * 10 : -1;
            out->loop = loop_count;
            out->disposal = pending_disposal;
            out->transparency = pending_transparency;
            return true;
        }
        ++current_frame;
        pending_duration_cs = -1;
        pending_disposal = 0;
        pending_transparency = -1;
    }

    return false;
}

bool read_gif_color_table(
    const std::vector<std::uint8_t>& data,
    std::size_t* pos,
    std::size_t entry_count,
    std::vector<std::uint8_t>* out)
{
    if (!pos || !out || entry_count == 0u || entry_count > 256u) {
        return false;
    }
    const std::size_t byte_count = entry_count * 3u;
    if (byte_count > data.size() - *pos) {
        return false;
    }
    out->assign(data.begin() + static_cast<std::ptrdiff_t>(*pos),
                data.begin() + static_cast<std::ptrdiff_t>(*pos + byte_count));
    *pos += byte_count;
    return true;
}

bool read_gif_sub_blocks(
    const std::vector<std::uint8_t>& data,
    std::size_t* pos,
    std::vector<std::uint8_t>* out)
{
    if (!pos || !out) {
        return false;
    }
    out->clear();
    while (*pos < data.size()) {
        const std::size_t block_size = data[(*pos)++];
        if (block_size == 0u) {
            return true;
        }
        if (block_size > data.size() - *pos) {
            return false;
        }
        out->insert(out->end(),
                    data.begin() + static_cast<std::ptrdiff_t>(*pos),
                    data.begin() + static_cast<std::ptrdiff_t>(*pos + block_size));
        *pos += block_size;
    }
    return false;
}

struct GifBitReader {
    const std::vector<std::uint8_t>& bytes;
    std::size_t bit_pos = 0;

    bool read(int bit_count, int* out_code)
    {
        if (!out_code || bit_count <= 0 || bit_count > 12) {
            return false;
        }
        const std::size_t total_bits = bytes.size() * 8u;
        if (static_cast<std::size_t>(bit_count) > total_bits - bit_pos) {
            return false;
        }
        std::uint32_t code = 0;
        for (int bit = 0; bit < bit_count; ++bit) {
            const std::uint8_t value = bytes[bit_pos / 8u];
            if ((value & (std::uint8_t{1} << (bit_pos % 8u))) != 0u) {
                code |= std::uint32_t{1} << bit;
            }
            ++bit_pos;
        }
        *out_code = static_cast<int>(code);
        return true;
    }
};

bool gif_lzw_decode_indices(
    const std::vector<std::uint8_t>& compressed,
    int min_code_size,
    std::size_t expected_pixels,
    std::vector<std::uint8_t>* out)
{
    if (!out || min_code_size < 2 || min_code_size > 8 || expected_pixels == 0u) {
        return false;
    }
    out->clear();
    out->reserve(expected_pixels);

    const int clear_code = 1 << min_code_size;
    const int end_code = clear_code + 1;
    int next_code = end_code + 1;
    int code_size = min_code_size + 1;
    std::vector<std::vector<std::uint8_t>> dictionary(4096);

    auto reset_dictionary = [&]() {
        for (auto& entry : dictionary) {
            entry.clear();
        }
        for (int code = 0; code < clear_code; ++code) {
            dictionary[code] = {static_cast<std::uint8_t>(code)};
        }
        next_code = end_code + 1;
        code_size = min_code_size + 1;
    };

    reset_dictionary();
    GifBitReader reader{compressed};
    std::vector<std::uint8_t> previous;
    bool have_previous = false;

    while (out->size() < expected_pixels) {
        int code = 0;
        if (!reader.read(code_size, &code)) {
            return false;
        }
        if (code == clear_code) {
            reset_dictionary();
            previous.clear();
            have_previous = false;
            continue;
        }
        if (code == end_code) {
            break;
        }

        std::vector<std::uint8_t> entry;
        if (code >= 0 && code < next_code && !dictionary[code].empty()) {
            entry = dictionary[code];
        } else if (code == next_code && have_previous && !previous.empty()) {
            entry = previous;
            entry.push_back(previous.front());
        } else {
            return false;
        }

        out->insert(out->end(), entry.begin(), entry.end());
        if (have_previous && next_code < 4096 && !previous.empty() && !entry.empty()) {
            std::vector<std::uint8_t> next_entry = previous;
            next_entry.push_back(entry.front());
            dictionary[next_code++] = std::move(next_entry);
            if (next_code == (1 << code_size) && code_size < 12) {
                ++code_size;
            }
        }
        previous = std::move(entry);
        have_previous = true;
    }

    if (out->size() < expected_pixels) {
        return false;
    }
    if (out->size() > expected_pixels) {
        out->resize(expected_pixels);
    }
    return true;
}

bool gif_deinterlace_indices(
    const std::vector<std::uint8_t>& decoded,
    int width,
    int height,
    std::vector<std::uint8_t>* out)
{
    if (!out || width <= 0 || height <= 0 ||
        decoded.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return false;
    }
    out->assign(decoded.size(), 0);
    static constexpr int pass_starts[4] = {0, 4, 2, 1};
    static constexpr int pass_steps[4] = {8, 8, 4, 2};
    std::size_t src_row = 0;
    for (int pass = 0; pass < 4; ++pass) {
        for (int y = pass_starts[pass]; y < height; y += pass_steps[pass]) {
            if (src_row >= static_cast<std::size_t>(height)) {
                return false;
            }
            std::copy_n(decoded.data() + src_row * static_cast<std::size_t>(width),
                        width,
                        out->data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width));
            ++src_row;
        }
    }
    return src_row == static_cast<std::size_t>(height);
}

void gif_palette_color(
    const std::vector<std::uint8_t>& palette,
    int index,
    std::uint8_t* r,
    std::uint8_t* g,
    std::uint8_t* b)
{
    const std::size_t offset = static_cast<std::size_t>(index) * 3u;
    if (index < 0 || offset + 2u >= palette.size()) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }
    *r = palette[offset];
    *g = palette[offset + 1u];
    *b = palette[offset + 2u];
}

void gif_fill_rgb_rect(
    std::vector<std::uint8_t>* canvas,
    int logical_width,
    int logical_height,
    int left,
    int top,
    int width,
    int height,
    const std::uint8_t* color)
{
    if (!canvas || !color || logical_width <= 0 || logical_height <= 0 || width <= 0 || height <= 0) {
        return;
    }
    const int x0 = std::max(0, left);
    const int y0 = std::max(0, top);
    const int x1 = std::min(logical_width, left + width);
    const int y1 = std::min(logical_height, top + height);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(logical_width) +
                                        static_cast<std::size_t>(x)) * 3u;
            (*canvas)[offset] = color[0];
            (*canvas)[offset + 1u] = color[1];
            (*canvas)[offset + 2u] = color[2];
        }
    }
}

void gif_draw_indexed_frame_rgb(
    std::vector<std::uint8_t>* canvas,
    int logical_width,
    int logical_height,
    int left,
    int top,
    int width,
    int height,
    const std::vector<std::uint8_t>& indices,
    const std::vector<std::uint8_t>& palette,
    int transparency)
{
    if (!canvas || logical_width <= 0 || logical_height <= 0 || width <= 0 || height <= 0) {
        return;
    }
    for (int y = 0; y < height; ++y) {
        const int dst_y = top + y;
        if (dst_y < 0 || dst_y >= logical_height) {
            continue;
        }
        for (int x = 0; x < width; ++x) {
            const int dst_x = left + x;
            if (dst_x < 0 || dst_x >= logical_width) {
                continue;
            }
            const std::size_t src_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                           static_cast<std::size_t>(x);
            if (src_offset >= indices.size()) {
                return;
            }
            const int index = indices[src_offset];
            if (index == transparency) {
                continue;
            }
            std::uint8_t r = 0;
            std::uint8_t g = 0;
            std::uint8_t b = 0;
            gif_palette_color(palette, index, &r, &g, &b);
            const std::size_t dst_offset = (static_cast<std::size_t>(dst_y) * static_cast<std::size_t>(logical_width) +
                                            static_cast<std::size_t>(dst_x)) * 3u;
            (*canvas)[dst_offset] = r;
            (*canvas)[dst_offset + 1u] = g;
            (*canvas)[dst_offset + 2u] = b;
        }
    }
}

int open_gif_composited_frame_image(const char* path, int frame_index, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (frame_index <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data) || data.size() < 13u ||
        !(std::memcmp(data.data(), "GIF87a", 6u) == 0 ||
          std::memcmp(data.data(), "GIF89a", 6u) == 0)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int logical_width = static_cast<int>(read_le16(data.data() + 6u));
    const int logical_height = static_cast<int>(read_le16(data.data() + 8u));
    if (logical_width <= 0 || logical_height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(logical_width, logical_height, 3, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const std::uint8_t logical_packed = data[10];
    const int background_index = data[11];
    std::size_t pos = 13u;
    std::vector<std::uint8_t> global_palette;
    if ((logical_packed & 0x80u) != 0u) {
        const std::size_t entry_count = std::size_t{1} << ((logical_packed & 0x07u) + 1u);
        if (!read_gif_color_table(data, &pos, entry_count, &global_palette)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    std::uint8_t background[3] = {0, 0, 0};
    gif_palette_color(global_palette, background_index, &background[0], &background[1], &background[2]);

    try {
        std::vector<std::uint8_t> canvas(size, 0);
        for (std::size_t pixel = 0; pixel < size; pixel += 3u) {
            canvas[pixel] = background[0];
            canvas[pixel + 1u] = background[1];
            canvas[pixel + 2u] = background[2];
        }

        int pending_disposal = 0;
        int pending_transparency = -1;
        int current_frame = 0;

        while (pos < data.size()) {
            const std::uint8_t introducer = data[pos++];
            if (introducer == 0x3bu) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (introducer == 0x21u) {
                if (pos >= data.size()) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const std::uint8_t label = data[pos++];
                if (label == 0xf9u) {
                    if (pos >= data.size()) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    const std::size_t block_size = data[pos++];
                    if (block_size < 4u || block_size > data.size() - pos) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    const std::uint8_t packed = data[pos];
                    pending_disposal = (packed >> 2) & 0x07;
                    pending_transparency = (packed & 0x01u) != 0u ? static_cast<int>(data[pos + 3u]) : -1;
                    pos += block_size;
                    if (pos >= data.size() || data[pos] != 0u) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    ++pos;
                } else if (!skip_gif_sub_blocks(data, &pos)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                continue;
            }
            if (introducer != 0x2cu || 9u > data.size() - pos) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            const int left = static_cast<int>(read_le16(data.data() + pos));
            const int top = static_cast<int>(read_le16(data.data() + pos + 2u));
            const int frame_width = static_cast<int>(read_le16(data.data() + pos + 4u));
            const int frame_height = static_cast<int>(read_le16(data.data() + pos + 6u));
            const std::uint8_t image_packed = data[pos + 8u];
            pos += 9u;
            if (frame_width <= 0 || frame_height <= 0) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            std::vector<std::uint8_t> local_palette;
            const std::vector<std::uint8_t>* palette = &global_palette;
            if ((image_packed & 0x80u) != 0u) {
                const std::size_t entry_count = std::size_t{1} << ((image_packed & 0x07u) + 1u);
                if (!read_gif_color_table(data, &pos, entry_count, &local_palette)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                palette = &local_palette;
            }
            if (palette->empty() || pos >= data.size()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            const int min_code_size = data[pos++];
            std::vector<std::uint8_t> compressed;
            if (!read_gif_sub_blocks(data, &pos, &compressed)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::size_t expected_pixels = static_cast<std::size_t>(frame_width) *
                                                static_cast<std::size_t>(frame_height);
            std::vector<std::uint8_t> decoded;
            if (!gif_lzw_decode_indices(compressed, min_code_size, expected_pixels, &decoded)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if ((image_packed & 0x40u) != 0u) {
                std::vector<std::uint8_t> deinterlaced;
                if (!gif_deinterlace_indices(decoded, frame_width, frame_height, &deinterlaced)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                decoded = std::move(deinterlaced);
            }

            std::vector<std::uint8_t> restore_canvas;
            if (pending_disposal == 3) {
                restore_canvas = canvas;
            }
            gif_draw_indexed_frame_rgb(
                &canvas,
                logical_width,
                logical_height,
                left,
                top,
                frame_width,
                frame_height,
                decoded,
                *palette,
                pending_transparency);

            if (current_frame == frame_index) {
                auto* image = new PillowCImage{
                    logical_width,
                    logical_height,
                    PILLOW_C_MODE_RGB,
                    3,
                    stride,
                    std::move(canvas)};
                *out_image = image;
                return PILLOW_C_OK;
            }

            if (pending_disposal == 2) {
                gif_fill_rgb_rect(&canvas, logical_width, logical_height, left, top, frame_width, frame_height, background);
            } else if (pending_disposal == 3 && !restore_canvas.empty()) {
                canvas = std::move(restore_canvas);
            }

            ++current_frame;
            pending_disposal = 0;
            pending_transparency = -1;
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    return PILLOW_C_INVALID_ARGUMENT;
}

bool jpeg_is_sof_marker(std::uint8_t marker)
{
    return (marker >= 0xc0u && marker <= 0xcfu) &&
           marker != 0xc4u &&
           marker != 0xc8u &&
           marker != 0xccu;
}

std::uint16_t read_tiff16(const std::uint8_t* data, bool little_endian)
{
    return little_endian ? read_le16(data) : read_be16(data);
}

std::uint32_t read_tiff32(const std::uint8_t* data, bool little_endian)
{
    return little_endian ? read_le32(data) : read_be32(data);
}

int parse_exif_orientation(const std::uint8_t* payload, std::size_t payload_size)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return 0;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || read_tiff16(tiff + 2u, little_endian) != 42u) {
        return 0;
    }

    const std::uint32_t ifd_offset = read_tiff32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return 0;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = read_tiff16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return 0;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
        if (tag == 0x0112u && type == 3u && count == 1u) {
            const std::uint16_t value = read_tiff16(entry + 8u, little_endian);
            return value >= 1u && value <= 8u ? static_cast<int>(value) : 0;
        }
    }
    return 0;
}

void apply_jpeg_jfif_metadata(const std::uint8_t* payload, std::size_t payload_size, JpegMetadata* metadata)
{
    if (!payload || payload_size < 12u || !metadata || std::memcmp(payload, "JFIF\0", 5u) != 0) {
        return;
    }
    metadata->has_jfif = true;
    metadata->jfif_major = payload[5];
    metadata->jfif_minor = payload[6];
    metadata->jfif_unit = payload[7];
    metadata->jfif_density_x = read_be16(payload + 8u);
    metadata->jfif_density_y = read_be16(payload + 10u);
    if (metadata->jfif_unit == 1) {
        metadata->has_dpi = true;
        metadata->dpi_x = static_cast<double>(metadata->jfif_density_x);
        metadata->dpi_y = static_cast<double>(metadata->jfif_density_y);
    } else if (metadata->jfif_unit == 2) {
        metadata->has_dpi = true;
        metadata->dpi_x = static_cast<double>(metadata->jfif_density_x) * 2.54;
        metadata->dpi_y = static_cast<double>(metadata->jfif_density_y) * 2.54;
    }
}

bool read_jpeg_metadata(const char* path, JpegMetadata* metadata)
{
    if (!path || !metadata) {
        return false;
    }
    *metadata = JpegMetadata{};
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
        const std::uint8_t* segment_payload = data.data() + offset + 2u;
        const std::size_t segment_payload_size = static_cast<std::size_t>(segment_length) - 2u;
        if (marker == 0xe0u && !metadata->has_jfif) {
            apply_jpeg_jfif_metadata(segment_payload, segment_payload_size, metadata);
        }
        if (marker == 0xe1u && metadata->exif_orientation == 0) {
            metadata->exif_orientation = parse_exif_orientation(segment_payload, segment_payload_size);
        }
        if (jpeg_is_sof_marker(marker)) {
            if (segment_length < 8u) {
                return false;
            }
            metadata->components = data[offset + 7u];
        }
        offset += segment_length;
    }
    return metadata->components > 0;
}

bool read_jpeg_component_count_and_orientation(const char* path, int* components, int* orientation)
{
    if (!path || !components || !orientation) {
        return false;
    }
    JpegMetadata metadata;
    if (!read_jpeg_metadata(path, &metadata)) {
        return false;
    }
    *components = metadata.components;
    *orientation = metadata.exif_orientation;
    return true;
}

bool read_jpeg_component_count(const char* path, int* components)
{
    int orientation = 0;
    return read_jpeg_component_count_and_orientation(path, components, &orientation);
}

bool pillow_round_to_i64(double value, std::int64_t* out_value)
{
    if (!out_value || !std::isfinite(value) ||
        value < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
        value > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        return false;
    }
    const double lower = std::floor(value);
    const double fraction = value - lower;
    double rounded = lower;
    if (fraction > 0.5) {
        rounded = lower + 1.0;
    } else if (fraction == 0.5) {
        const auto lower_i = static_cast<std::int64_t>(lower);
        rounded = (lower_i % 2 == 0) ? lower : lower + 1.0;
    }
    *out_value = static_cast<std::int64_t>(rounded);
    return true;
}

bool jpeg_standalone_marker(std::uint8_t marker)
{
    return marker == 0xd8u || marker == 0xd9u || (marker >= 0xd0u && marker <= 0xd7u);
}

bool write_binary_file(const char* path, const std::vector<std::uint8_t>& data);

int jpeg_density_from_dpi(double dpi_x, double dpi_y, std::uint8_t* out_unit, std::uint16_t* out_x, std::uint16_t* out_y)
{
    if (!out_unit || !out_x || !out_y) {
        return PILLOW_C_NULL_POINTER;
    }
    std::int64_t rounded_x = 0;
    std::int64_t rounded_y = 0;
    if (!pillow_round_to_i64(dpi_x, &rounded_x) || !pillow_round_to_i64(dpi_y, &rounded_y)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (rounded_x <= 0 || rounded_y <= 0) {
        *out_unit = 0;
        *out_x = 1;
        *out_y = 1;
        return PILLOW_C_OK;
    }
    *out_unit = 1;
    *out_x = static_cast<std::uint16_t>(rounded_x);
    *out_y = static_cast<std::uint16_t>(rounded_y);
    return PILLOW_C_OK;
}

int patch_jpeg_jfif_density(const char* path, double dpi_x, double dpi_y)
{
    std::uint8_t unit = 0;
    std::uint16_t x_density = 0;
    std::uint16_t y_density = 0;
    int status = jpeg_density_from_dpi(dpi_x, dpi_y, &unit, &x_density, &y_density);
    if (status != PILLOW_C_OK) {
        return status;
    }

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data) || data.size() < 2u || data[0] != 0xffu || data[1] != 0xd8u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t pos = 2u;
        while (pos + 4u <= data.size()) {
            if (data[pos] != 0xffu) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::uint8_t marker = data[pos + 1u];
            pos += 2u;
            if (jpeg_standalone_marker(marker)) {
                continue;
            }
            if (pos + 2u > data.size()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::uint16_t length = read_be16(data.data() + pos);
            if (length < 2u || pos + static_cast<std::size_t>(length) > data.size()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::size_t payload = pos + 2u;
            const std::size_t payload_size = static_cast<std::size_t>(length) - 2u;
            if (marker == 0xe0u && payload_size >= 14u &&
                std::memcmp(data.data() + payload, "JFIF\0", 5u) == 0) {
                data[payload + 7u] = unit;
                data[payload + 8u] = static_cast<std::uint8_t>((x_density >> 8) & 0xffu);
                data[payload + 9u] = static_cast<std::uint8_t>(x_density & 0xffu);
                data[payload + 10u] = static_cast<std::uint8_t>((y_density >> 8) & 0xffu);
                data[payload + 11u] = static_cast<std::uint8_t>(y_density & 0xffu);
                return write_binary_file(path, data) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
            }
            pos += length;
        }

        std::vector<std::uint8_t> app0;
        app0.push_back(0xffu);
        app0.push_back(0xe0u);
        append_be16(app0, 16u);
        app0.insert(app0.end(), {'J', 'F', 'I', 'F', 0, 1, 1, unit});
        append_be16(app0, x_density);
        append_be16(app0, y_density);
        app0.push_back(0);
        app0.push_back(0);
        data.insert(data.begin() + 2, app0.begin(), app0.end());
        return write_binary_file(path, data) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
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

bool ppm_is_space(std::uint8_t value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' || value == '\v';
}

bool ppm_skip_space_and_comments(const std::vector<std::uint8_t>& data, std::size_t* offset)
{
    if (!offset) {
        return false;
    }
    while (*offset < data.size()) {
        if (ppm_is_space(data[*offset])) {
            ++(*offset);
            continue;
        }
        if (data[*offset] == '#') {
            while (*offset < data.size() && data[*offset] != '\n' && data[*offset] != '\r') {
                ++(*offset);
            }
            continue;
        }
        break;
    }
    return *offset < data.size();
}

bool ppm_read_positive_int(const std::vector<std::uint8_t>& data, std::size_t* offset, int* out)
{
    if (!offset || !out || !ppm_skip_space_and_comments(data, offset)) {
        return false;
    }
    std::uint64_t value = 0;
    bool has_digit = false;
    while (*offset < data.size()) {
        const std::uint8_t ch = data[*offset];
        if (ch < '0' || ch > '9') {
            break;
        }
        has_digit = true;
        value = value * 10u + static_cast<std::uint64_t>(ch - '0');
        if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        ++(*offset);
    }
    if (!has_digit || value == 0) {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

bool ppm_read_nonnegative_int(const std::vector<std::uint8_t>& data, std::size_t* offset, int* out)
{
    if (!offset || !out || !ppm_skip_space_and_comments(data, offset)) {
        return false;
    }
    std::uint64_t value = 0;
    bool has_digit = false;
    while (*offset < data.size()) {
        const std::uint8_t ch = data[*offset];
        if (ch < '0' || ch > '9') {
            break;
        }
        has_digit = true;
        value = value * 10u + static_cast<std::uint64_t>(ch - '0');
        if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        ++(*offset);
    }
    if (!has_digit) {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

std::uint8_t ppm_scale_sample_to_u8(int value, int max_value)
{
    if (value <= 0) {
        return 0;
    }
    if (max_value <= 0) {
        return 0;
    }
    if (max_value == 255) {
        return static_cast<std::uint8_t>(std::min(value, 255));
    }

    const std::uint64_t numerator = static_cast<std::uint64_t>(value) * 255u;
    const std::uint64_t denominator = static_cast<std::uint64_t>(max_value);
    std::uint64_t quotient = numerator / denominator;
    const std::uint64_t remainder = numerator % denominator;
    const std::uint64_t twice_remainder = remainder * 2u;
    if (twice_remainder > denominator || (twice_remainder == denominator && (quotient & 1u) != 0u)) {
        ++quotient;
    }
    if (quotient > 255u) {
        return 255;
    }
    return static_cast<std::uint8_t>(quotient);
}

std::uint32_t ppm_scale_sample_to_u16(int value, int max_value)
{
    if (value <= 0) {
        return 0;
    }
    if (max_value <= 0) {
        return 0;
    }
    if (max_value == 65535) {
        return static_cast<std::uint32_t>(std::min(value, 65535));
    }

    const std::uint64_t numerator = static_cast<std::uint64_t>(value) * 65535u;
    const std::uint64_t denominator = static_cast<std::uint64_t>(max_value);
    std::uint64_t quotient = numerator / denominator;
    const std::uint64_t remainder = numerator % denominator;
    const std::uint64_t twice_remainder = remainder * 2u;
    if (twice_remainder > denominator || (twice_remainder == denominator && (quotient & 1u) != 0u)) {
        ++quotient;
    }
    if (quotient > 65535u) {
        return 65535u;
    }
    return static_cast<std::uint32_t>(quotient);
}

void write_le32_pixel(std::uint8_t* dst, std::uint32_t value)
{
    dst[0] = static_cast<std::uint8_t>(value & 0xFFu);
    dst[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    dst[2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    dst[3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

int ppm_data_offset_after_header(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t* out_offset)
{
    if (!out_offset || offset >= data.size() || !ppm_is_space(data[offset])) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (data[offset] == '\r' && offset + 1u < data.size() && data[offset + 1u] == '\n') {
        offset += 2u;
    } else {
        ++offset;
    }
    if (offset >= data.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    *out_offset = offset;
    return PILLOW_C_OK;
}

void append_ascii_int(std::vector<std::uint8_t>& out, int value)
{
    char buf[32] = {};
    const int written = std::snprintf(buf, sizeof(buf), "%d", value);
    for (int i = 0; i < written; ++i) {
        out.push_back(static_cast<std::uint8_t>(buf[i]));
    }
}

int open_ppm_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data) || data.size() < 3u || data[0] != 'P' ||
            (data[1] < '1' || data[1] > '6')) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const bool is_plain = data[1] == '1' || data[1] == '2' || data[1] == '3';
        const bool is_pbm = data[1] == '1' || data[1] == '4';
        const bool is_gray = data[1] == '2' || data[1] == '5';
        int mode = is_pbm ? PILLOW_C_MODE_1 : (is_gray ? PILLOW_C_MODE_L : PILLOW_C_MODE_RGB);
        std::size_t offset = 2u;
        int width = 0;
        int height = 0;
        int max_value = 0;
        if (!ppm_read_positive_int(data, &offset, &width) ||
            !ppm_read_positive_int(data, &offset, &height)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (!is_pbm) {
            if (!ppm_read_positive_int(data, &offset, &max_value)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (max_value >= 65536) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (is_gray && max_value > 255) {
                mode = PILLOW_C_MODE_I;
            }
        }

        const int channels = channels_for_mode(mode);
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};
        if (is_plain) {
            const std::size_t sample_count = mode == PILLOW_C_MODE_I ? size / 4u : size;
            for (std::size_t i = 0; i < sample_count; ++i) {
                int value = 0;
                if (!ppm_read_nonnegative_int(data, &offset, &value)) {
                    delete image;
                    return PILLOW_C_INVALID_LENGTH;
                }
                const int limit = is_pbm ? 1 : max_value;
                if (value > limit) {
                    delete image;
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                if (is_pbm) {
                    image->pixels[i] = value == 0 ? 255 : 0;
                } else if (mode == PILLOW_C_MODE_I) {
                    write_le32_pixel(image->pixels.data() + i * 4u, ppm_scale_sample_to_u16(value, max_value));
                } else {
                    image->pixels[i] = ppm_scale_sample_to_u8(value, max_value);
                }
            }
        } else if (is_pbm) {
            std::size_t pixel_offset = 0;
            int status = ppm_data_offset_after_header(data, offset, &pixel_offset);
            if (status != PILLOW_C_OK) {
                delete image;
                return status;
            }
            const std::size_t packed_row_bytes = (static_cast<std::size_t>(width) + 7u) / 8u;
            const std::size_t packed_size = packed_row_bytes * static_cast<std::size_t>(height);
            if (packed_size > data.size() - pixel_offset) {
                delete image;
                return PILLOW_C_INVALID_LENGTH;
            }
            for (int y = 0; y < height; ++y) {
                const std::uint8_t* src_row = data.data() + pixel_offset + static_cast<std::size_t>(y) * packed_row_bytes;
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * stride;
                for (int x = 0; x < width; ++x) {
                    const std::uint8_t packed = src_row[static_cast<std::size_t>(x) / 8u];
                    const int bit = (packed >> (7 - (x & 7))) & 1;
                    dst_row[x] = bit ? 0 : 255;
                }
            }
        } else {
            std::size_t pixel_offset = 0;
            int status = ppm_data_offset_after_header(data, offset, &pixel_offset);
            if (status != PILLOW_C_OK) {
                delete image;
                return status;
            }
            const int bytes_per_sample = max_value < 256 ? 1 : 2;
            const std::size_t sample_count = mode == PILLOW_C_MODE_I ? size / 4u : size;
            if (sample_count > (data.size() - pixel_offset) / static_cast<std::size_t>(bytes_per_sample)) {
                delete image;
                return PILLOW_C_INVALID_LENGTH;
            }
            if (max_value == 255 && mode != PILLOW_C_MODE_I) {
                std::memcpy(image->pixels.data(), data.data() + pixel_offset, size);
            } else {
                for (std::size_t i = 0; i < sample_count; ++i) {
                    int value = 0;
                    if (bytes_per_sample == 1) {
                        value = data[pixel_offset + i];
                    } else {
                        const std::size_t src_offset = pixel_offset + i * 2u;
                        value = (static_cast<int>(data[src_offset]) << 8) | static_cast<int>(data[src_offset + 1u]);
                    }
                    if (mode == PILLOW_C_MODE_I) {
                        write_le32_pixel(image->pixels.data() + i * 4u, ppm_scale_sample_to_u16(value, max_value));
                    } else {
                        image->pixels[i] = ppm_scale_sample_to_u8(value, max_value);
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

int save_ppm_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!((image->mode == PILLOW_C_MODE_1 && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_I && image->channels == 4) ||
          (image->mode == PILLOW_C_MODE_RGB && image->channels == 3))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        if (image->mode == PILLOW_C_MODE_1) {
            std::size_t packed_row_bytes = 0;
            std::size_t packed_size = 0;
            if (!checked_mode1_raw_size(image, &packed_row_bytes, &packed_size)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            std::vector<std::uint8_t> out;
            out.reserve(32u + packed_size);
            out.push_back('P');
            out.push_back('4');
            out.push_back('\n');
            append_ascii_int(out, image->width);
            out.push_back(' ');
            append_ascii_int(out, image->height);
            out.push_back('\n');
            const std::size_t pixel_offset = out.size();
            out.resize(pixel_offset + packed_size, 0);
            for (int y = 0; y < image->height; ++y) {
                const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                std::uint8_t* dst_row = out.data() + pixel_offset + static_cast<std::size_t>(y) * packed_row_bytes;
                for (int x = 0; x < image->width; ++x) {
                    if (src_row[x] == 0) {
                        dst_row[static_cast<std::size_t>(x) / 8u] |= static_cast<std::uint8_t>(0x80u >> (x & 7));
                    }
                }
            }
            return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> out;
        const std::size_t pixel_count =
            static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height);
        out.reserve(32u + (image->mode == PILLOW_C_MODE_I ? pixel_count * 2u : image->pixels.size()));
        out.push_back('P');
        out.push_back(image->mode == PILLOW_C_MODE_RGB ? '6' : '5');
        out.push_back('\n');
        append_ascii_int(out, image->width);
        out.push_back(' ');
        append_ascii_int(out, image->height);
        out.push_back('\n');
        if (image->mode == PILLOW_C_MODE_I) {
            out.push_back('6');
            out.push_back('5');
            out.push_back('5');
            out.push_back('3');
            out.push_back('5');
        } else {
            out.push_back('2');
            out.push_back('5');
            out.push_back('5');
        }
        out.push_back('\n');
        if (image->mode == PILLOW_C_MODE_I) {
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::uint8_t* src = image->pixels.data() + i * 4u;
                const std::uint16_t value = clip_i32_to_u16(read_i32_le(src));
                out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
                out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
            }
        } else {
            out.insert(out.end(), image->pixels.begin(), image->pixels.end());
        }
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

std::size_t qoi_hash_pixel(const std::uint8_t pixel[4])
{
    return (static_cast<std::size_t>(pixel[0]) * 3u +
            static_cast<std::size_t>(pixel[1]) * 5u +
            static_cast<std::size_t>(pixel[2]) * 7u +
            static_cast<std::size_t>(pixel[3]) * 11u) %
           64u;
}

int qoi_signed_delta(std::uint8_t left, std::uint8_t right)
{
    int result = (static_cast<int>(left) - static_cast<int>(right)) & 255;
    if (result >= 128) {
        result -= 256;
    }
    return result;
}

void qoi_write_run(std::vector<std::uint8_t>& out, int* run)
{
    out.push_back(static_cast<std::uint8_t>(0xc0u | static_cast<std::uint8_t>(*run - 1)));
    *run = 0;
}

int open_qoi_image(const char* path, PillowCImage** out_image)
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
        if (data.size() < 4u || std::memcmp(data.data(), "qoif", 4u) != 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 14u) {
            return PILLOW_C_INVALID_LENGTH;
        }

        const std::uint32_t width_u32 = read_be32(data.data() + 4u);
        const std::uint32_t height_u32 = read_be32(data.data() + 8u);
        const int channels = data[12] == 3u ? 3 : (data[12] == 4u ? 4 : 0);
        if (width_u32 == 0 || height_u32 == 0 || width_u32 > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
            height_u32 > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) || channels == 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const int width = static_cast<int>(width_u32);
        const int height = static_cast<int>(height_u32);
        const int mode = channels == 3 ? PILLOW_C_MODE_RGB : PILLOW_C_MODE_RGBA;
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};

        std::uint8_t index[64][4] = {};
        std::uint8_t previous[4] = {0, 0, 0, 255};
        std::size_t offset = 14u;
        const std::size_t pixel_count = size / static_cast<std::size_t>(channels);
        std::size_t out_pixel = 0;
        while (out_pixel < pixel_count) {
            if (offset >= data.size()) {
                delete image;
                return PILLOW_C_INVALID_LENGTH;
            }

            const std::uint8_t byte = data[offset++];
            std::uint8_t pixel[4] = {previous[0], previous[1], previous[2], previous[3]};
            if (byte == 0xfeu) {
                if (offset + 3u > data.size()) {
                    delete image;
                    return PILLOW_C_INVALID_LENGTH;
                }
                pixel[0] = data[offset++];
                pixel[1] = data[offset++];
                pixel[2] = data[offset++];
            } else if (byte == 0xffu) {
                if (offset + 4u > data.size()) {
                    delete image;
                    return PILLOW_C_INVALID_LENGTH;
                }
                pixel[0] = data[offset++];
                pixel[1] = data[offset++];
                pixel[2] = data[offset++];
                pixel[3] = data[offset++];
            } else {
                const std::uint8_t op = byte >> 6;
                if (op == 0u) {
                    const std::uint8_t slot = byte & 0x3fu;
                    pixel[0] = index[slot][0];
                    pixel[1] = index[slot][1];
                    pixel[2] = index[slot][2];
                    pixel[3] = index[slot][3];
                } else if (op == 1u) {
                    pixel[0] = static_cast<std::uint8_t>(previous[0] + ((byte >> 4) & 0x03u) - 2);
                    pixel[1] = static_cast<std::uint8_t>(previous[1] + ((byte >> 2) & 0x03u) - 2);
                    pixel[2] = static_cast<std::uint8_t>(previous[2] + (byte & 0x03u) - 2);
                } else if (op == 2u) {
                    if (offset >= data.size()) {
                        delete image;
                        return PILLOW_C_INVALID_LENGTH;
                    }
                    const std::uint8_t second = data[offset++];
                    const int dg = static_cast<int>(byte & 0x3fu) - 32;
                    const int dr = static_cast<int>((second >> 4) & 0x0fu) - 8;
                    const int db = static_cast<int>(second & 0x0fu) - 8;
                    pixel[0] = static_cast<std::uint8_t>(previous[0] + dg + dr);
                    pixel[1] = static_cast<std::uint8_t>(previous[1] + dg);
                    pixel[2] = static_cast<std::uint8_t>(previous[2] + dg + db);
                } else {
                    const std::size_t run = static_cast<std::size_t>(byte & 0x3fu) + 1u;
                    if (run > pixel_count - out_pixel) {
                        delete image;
                        return PILLOW_C_INVALID_LENGTH;
                    }
                    for (std::size_t i = 0; i < run; ++i) {
                        std::uint8_t* dst = image->pixels.data() + (out_pixel + i) * static_cast<std::size_t>(channels);
                        dst[0] = previous[0];
                        dst[1] = previous[1];
                        dst[2] = previous[2];
                        if (channels == 4) {
                            dst[3] = previous[3];
                        }
                    }
                    out_pixel += run;
                    continue;
                }
            }

            previous[0] = pixel[0];
            previous[1] = pixel[1];
            previous[2] = pixel[2];
            previous[3] = pixel[3];
            const std::size_t slot = qoi_hash_pixel(pixel);
            index[slot][0] = pixel[0];
            index[slot][1] = pixel[1];
            index[slot][2] = pixel[2];
            index[slot][3] = pixel[3];

            std::uint8_t* dst = image->pixels.data() + out_pixel * static_cast<std::size_t>(channels);
            dst[0] = pixel[0];
            dst[1] = pixel[1];
            dst[2] = pixel[2];
            if (channels == 4) {
                dst[3] = pixel[3];
            }
            ++out_pixel;
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_qoi_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!((image->mode == PILLOW_C_MODE_RGB && image->channels == 3) ||
          (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4)) ||
        image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<std::uint8_t> out;
        out.reserve(14u + image->pixels.size() + 8u);
        out.insert(out.end(), {'q', 'o', 'i', 'f'});
        append_be32(out, static_cast<std::uint32_t>(image->width));
        append_be32(out, static_cast<std::uint32_t>(image->height));
        out.push_back(static_cast<std::uint8_t>(image->channels));
        out.push_back(1u);

        std::uint8_t index[64][4] = {};
        bool valid[64] = {};
        valid[0] = true;
        std::uint8_t previous[4] = {0, 0, 0, 255};
        int run = 0;
        for (int y = 0; y < image->height; ++y) {
            const std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            for (int x = 0; x < image->width; ++x) {
                const std::uint8_t* src = row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels);
                std::uint8_t pixel[4] = {src[0], src[1], src[2], image->channels == 4 ? src[3] : static_cast<std::uint8_t>(255)};
                if (std::memcmp(pixel, previous, 4u) == 0) {
                    ++run;
                    if (run == 62) {
                        qoi_write_run(out, &run);
                    }
                } else {
                    if (run != 0) {
                        qoi_write_run(out, &run);
                    }

                    const std::size_t slot = qoi_hash_pixel(pixel);
                    if (valid[slot] && std::memcmp(index[slot], pixel, 4u) == 0) {
                        out.push_back(static_cast<std::uint8_t>(slot));
                    } else {
                        valid[slot] = true;
                        index[slot][0] = pixel[0];
                        index[slot][1] = pixel[1];
                        index[slot][2] = pixel[2];
                        index[slot][3] = pixel[3];

                        if (previous[3] == pixel[3]) {
                            const int dr = qoi_signed_delta(pixel[0], previous[0]);
                            const int dg = qoi_signed_delta(pixel[1], previous[1]);
                            const int db = qoi_signed_delta(pixel[2], previous[2]);
                            if (dr >= -2 && dr < 2 && dg >= -2 && dg < 2 && db >= -2 && db < 2) {
                                out.push_back(static_cast<std::uint8_t>(
                                    0x40u |
                                    static_cast<std::uint8_t>((dr + 2) << 4) |
                                    static_cast<std::uint8_t>((dg + 2) << 2) |
                                    static_cast<std::uint8_t>(db + 2)));
                            } else {
                                const int dgr = qoi_signed_delta(static_cast<std::uint8_t>(dr), static_cast<std::uint8_t>(dg));
                                const int dgb = qoi_signed_delta(static_cast<std::uint8_t>(db), static_cast<std::uint8_t>(dg));
                                if (dgr >= -8 && dgr < 8 && dg >= -32 && dg < 32 && dgb >= -8 && dgb < 8) {
                                    out.push_back(static_cast<std::uint8_t>(0x80u | static_cast<std::uint8_t>(dg + 32)));
                                    out.push_back(static_cast<std::uint8_t>(
                                        static_cast<std::uint8_t>((dgr + 8) << 4) |
                                        static_cast<std::uint8_t>(dgb + 8)));
                                } else {
                                    out.push_back(0xfeu);
                                    out.push_back(pixel[0]);
                                    out.push_back(pixel[1]);
                                    out.push_back(pixel[2]);
                                }
                            }
                        } else {
                            out.push_back(0xffu);
                            out.push_back(pixel[0]);
                            out.push_back(pixel[1]);
                            out.push_back(pixel[2]);
                            out.push_back(pixel[3]);
                        }
                    }
                }
                previous[0] = pixel[0];
                previous[1] = pixel[1];
                previous[2] = pixel[2];
                previous[3] = pixel[3];
            }
        }
        if (run != 0) {
            qoi_write_run(out, &run);
        }
        out.insert(out.end(), {0, 0, 0, 0, 0, 0, 0, 1});
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

void append_tga_footer(std::vector<std::uint8_t>& out)
{
    out.insert(out.end(), {
        0, 0, 0, 0, 0, 0, 0, 0,
        'T', 'R', 'U', 'E', 'V', 'I', 'S', 'I', 'O', 'N',
        '-', 'X', 'F', 'I', 'L', 'E', '.', 0,
    });
}

bool tga_file_pixel_equal(
    const std::vector<std::uint8_t>& pixels,
    std::size_t left_index,
    std::size_t right_index,
    int file_pixel_bytes)
{
    const std::size_t left = left_index * static_cast<std::size_t>(file_pixel_bytes);
    const std::size_t right = right_index * static_cast<std::size_t>(file_pixel_bytes);
    return std::memcmp(pixels.data() + left, pixels.data() + right, static_cast<std::size_t>(file_pixel_bytes)) == 0;
}

bool tga_file_pixel_equal(
    const std::uint8_t* pixels,
    std::size_t left_index,
    std::size_t right_index,
    int file_pixel_bytes)
{
    const std::size_t left = left_index * static_cast<std::size_t>(file_pixel_bytes);
    const std::size_t right = right_index * static_cast<std::size_t>(file_pixel_bytes);
    return std::memcmp(pixels + left, pixels + right, static_cast<std::size_t>(file_pixel_bytes)) == 0;
}

void encode_tga_rle_block(
    const std::uint8_t* pixels,
    int file_pixel_bytes,
    std::size_t pixel_count,
    std::vector<std::uint8_t>& out)
{
    std::size_t index = 0;
    while (index < pixel_count) {
        std::size_t run = 1;
        while (index + run < pixel_count &&
               run < 128u &&
               tga_file_pixel_equal(pixels, index, index + run, file_pixel_bytes)) {
            ++run;
        }

        if (run >= 2u) {
            out.push_back(static_cast<std::uint8_t>(0x80u | (run - 1u)));
            const std::size_t pixel_offset = index * static_cast<std::size_t>(file_pixel_bytes);
            out.insert(out.end(), pixels + pixel_offset, pixels + pixel_offset + static_cast<std::size_t>(file_pixel_bytes));
            index += run;
            continue;
        }

        const std::size_t raw_start = index;
        ++index;
        while (index < pixel_count && index - raw_start < 128u) {
            std::size_t next_run = 1;
            while (index + next_run < pixel_count &&
                   next_run < 128u &&
                   tga_file_pixel_equal(pixels, index, index + next_run, file_pixel_bytes)) {
                ++next_run;
            }
            if (next_run >= 2u) {
                break;
            }
            ++index;
        }

        const std::size_t raw_count = index - raw_start;
        out.push_back(static_cast<std::uint8_t>(raw_count - 1u));
        const std::size_t start_offset = raw_start * static_cast<std::size_t>(file_pixel_bytes);
        const std::size_t end_offset = index * static_cast<std::size_t>(file_pixel_bytes);
        out.insert(out.end(), pixels + start_offset, pixels + end_offset);
    }
}

void encode_tga_rle(
    const std::vector<std::uint8_t>& pixels,
    int file_pixel_bytes,
    std::size_t pixel_count,
    std::vector<std::uint8_t>& out)
{
    encode_tga_rle_block(pixels.data(), file_pixel_bytes, pixel_count, out);
}

void encode_tga_rle_rows(
    const std::vector<std::uint8_t>& pixels,
    int file_pixel_bytes,
    int width,
    int height,
    std::vector<std::uint8_t>& out)
{
    const std::size_t row_pixels = static_cast<std::size_t>(width);
    const std::size_t row_bytes = row_pixels * static_cast<std::size_t>(file_pixel_bytes);
    for (int row = 0; row < height; ++row) {
        encode_tga_rle_block(pixels.data() + static_cast<std::size_t>(row) * row_bytes, file_pixel_bytes, row_pixels, out);
    }
}

int open_tga_image(const char* path, PillowCImage** out_image)
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
        if (data.size() < 18u) {
            return PILLOW_C_INVALID_LENGTH;
        }

        const std::uint8_t id_length = data[0];
        const std::uint8_t color_map_type = data[1];
        const std::uint8_t image_type = data[2];
        const std::uint16_t color_map_first = read_le16(data.data() + 3u);
        const std::uint16_t color_map_length = read_le16(data.data() + 5u);
        const std::uint8_t color_map_depth = data[7];
        const int width = static_cast<int>(read_le16(data.data() + 12u));
        const int height = static_cast<int>(read_le16(data.data() + 14u));
        const std::uint8_t bits_per_pixel = data[16];
        const std::uint8_t descriptor = data[17];
        if (width <= 0 || height <= 0 || (color_map_type != 0 && color_map_type != 1)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        int mode = 0;
        int channels = 0;
        int file_pixel_bytes = 0;
        const bool is_rle = image_type == 9 || image_type == 10 || image_type == 11;
        const bool is_palette = color_map_type == 1 && (image_type == 1 || image_type == 9);
        if (is_palette && bits_per_pixel == 8) {
            if (color_map_depth != 24 ||
                static_cast<std::uint32_t>(color_map_first) + static_cast<std::uint32_t>(color_map_length) > 256u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            mode = PILLOW_C_MODE_P;
            channels = 1;
            file_pixel_bytes = 1;
        } else if (color_map_type == 0 && (image_type == 3 || image_type == 11) && bits_per_pixel == 8) {
            mode = PILLOW_C_MODE_L;
            channels = 1;
            file_pixel_bytes = 1;
        } else if (color_map_type == 0 && (image_type == 2 || image_type == 10) && bits_per_pixel == 24) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
            file_pixel_bytes = 3;
        } else if (color_map_type == 0 && (image_type == 2 || image_type == 10) && bits_per_pixel == 32) {
            mode = PILLOW_C_MODE_RGBA;
            channels = 4;
            file_pixel_bytes = 4;
        } else {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const std::size_t palette_offset = 18u + static_cast<std::size_t>(id_length);
        const std::size_t palette_entry_bytes = is_palette ? 3u : 0u;
        const std::size_t palette_size = static_cast<std::size_t>(color_map_length) * palette_entry_bytes;
        if (palette_offset > data.size() || palette_size > data.size() - palette_offset) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::vector<std::uint8_t> palette_rgb;
        if (is_palette) {
            palette_rgb.assign(
                (static_cast<std::size_t>(color_map_first) + static_cast<std::size_t>(color_map_length)) * 3u,
                std::uint8_t{0});
            for (std::size_t i = 0; i < static_cast<std::size_t>(color_map_length); ++i) {
                const std::size_t src = palette_offset + i * 3u;
                const std::size_t dst = (static_cast<std::size_t>(color_map_first) + i) * 3u;
                palette_rgb[dst + 0u] = data[src + 2u];
                palette_rgb[dst + 1u] = data[src + 1u];
                palette_rgb[dst + 2u] = data[src + 0u];
            }
        }

        const std::size_t pixel_offset = palette_offset + palette_size;
        const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        const std::size_t file_pixel_size = pixel_count * static_cast<std::size_t>(file_pixel_bytes);
        if (pixel_offset > data.size()) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::vector<std::uint8_t> file_pixels(file_pixel_size);
        if (is_rle) {
            std::size_t input = pixel_offset;
            std::size_t decoded_pixels = 0;
            while (decoded_pixels < pixel_count) {
                if (input >= data.size()) {
                    return PILLOW_C_INVALID_LENGTH;
                }
                const std::uint8_t packet = data[input++];
                const std::size_t packet_count = static_cast<std::size_t>((packet & 0x7fu) + 1u);
                if (packet_count > pixel_count - decoded_pixels) {
                    return PILLOW_C_INVALID_LENGTH;
                }
                if ((packet & 0x80u) != 0) {
                    if (static_cast<std::size_t>(file_pixel_bytes) > data.size() - input) {
                        return PILLOW_C_INVALID_LENGTH;
                    }
                    for (std::size_t i = 0; i < packet_count; ++i) {
                        const std::size_t dst = (decoded_pixels + i) * static_cast<std::size_t>(file_pixel_bytes);
                        std::memcpy(file_pixels.data() + dst, data.data() + input, static_cast<std::size_t>(file_pixel_bytes));
                    }
                    input += static_cast<std::size_t>(file_pixel_bytes);
                } else {
                    const std::size_t raw_size = packet_count * static_cast<std::size_t>(file_pixel_bytes);
                    if (raw_size > data.size() - input) {
                        return PILLOW_C_INVALID_LENGTH;
                    }
                    const std::size_t dst = decoded_pixels * static_cast<std::size_t>(file_pixel_bytes);
                    std::memcpy(file_pixels.data() + dst, data.data() + input, raw_size);
                    input += raw_size;
                }
                decoded_pixels += packet_count;
            }
        } else {
            if (file_pixel_size > data.size() - pixel_offset) {
                return PILLOW_C_INVALID_LENGTH;
            }
            std::memcpy(file_pixels.data(), data.data() + pixel_offset, file_pixel_size);
        }

        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};
        if (is_palette) {
            image->palette_rgb = std::move(palette_rgb);
            image->palette_alpha.clear();
            image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        }

        const bool origin_top = (descriptor & 0x20u) != 0;
        const bool origin_right = (descriptor & 0x10u) != 0;
        for (int y = 0; y < height; ++y) {
            const int file_y = origin_top ? y : (height - 1 - y);
            std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const int file_x = origin_right ? (width - 1 - x) : x;
                const std::uint8_t* src = file_pixels.data() +
                    (static_cast<std::size_t>(file_y) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(file_x)) * static_cast<std::size_t>(file_pixel_bytes);
                std::uint8_t* dst = dst_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels);
                if (channels == 1) {
                    dst[0] = src[0];
                } else {
                    dst[0] = src[2];
                    dst[1] = src[1];
                    dst[2] = src[0];
                    if (channels == 4) {
                        dst[3] = src[3];
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

int save_tga_image_with_options(const PillowCImage* image, const char* path, bool rle)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0 ||
        image->width > 65535 || image->height > 65535 ||
        !((image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_P && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) ||
          (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->mode == PILLOW_C_MODE_P &&
        (image->palette_rgb.size() > 256u * 3u || image->palette_rgb.size() % 3u != 0)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        const bool is_l = image->mode == PILLOW_C_MODE_L;
        const bool is_palette = image->mode == PILLOW_C_MODE_P;
        const int file_pixel_bytes = (is_l || is_palette) ? 1 : image->channels;
        const std::size_t palette_entries = is_palette ? image->palette_rgb.size() / 3u : 0u;
        const std::size_t pixel_count =
            static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height);
        std::vector<std::uint8_t> out;
        out.reserve(18u + palette_entries * 3u + pixel_count * static_cast<std::size_t>(file_pixel_bytes) + 26u);

        out.push_back(0);
        out.push_back(static_cast<std::uint8_t>(is_palette ? 1 : 0));
        out.push_back(static_cast<std::uint8_t>((is_palette ? 1 : (is_l ? 3 : 2)) + (rle ? 8 : 0)));
        append_le16(out, 0);
        append_le16(out, static_cast<std::uint16_t>(palette_entries));
        out.push_back(static_cast<std::uint8_t>(is_palette ? 24 : 0));
        append_le16(out, 0);
        append_le16(out, 0);
        append_le16(out, static_cast<std::uint16_t>(image->width));
        append_le16(out, static_cast<std::uint16_t>(image->height));
        out.push_back(static_cast<std::uint8_t>((is_l || is_palette) ? 8 : image->channels * 8));
        out.push_back(static_cast<std::uint8_t>(image->mode == PILLOW_C_MODE_RGBA ? 8 : 0));
        if (is_palette) {
            for (std::size_t i = 0; i < palette_entries; ++i) {
                const std::size_t offset = i * 3u;
                out.push_back(image->palette_rgb[offset + 2u]);
                out.push_back(image->palette_rgb[offset + 1u]);
                out.push_back(image->palette_rgb[offset + 0u]);
            }
        }

        std::vector<std::uint8_t> file_pixels;
        if (rle) {
            file_pixels.reserve(pixel_count * static_cast<std::size_t>(file_pixel_bytes));
        }
        for (int y = image->height - 1; y >= 0; --y) {
            const std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            for (int x = 0; x < image->width; ++x) {
                const std::uint8_t* src = row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels);
                if (is_l || is_palette) {
                    (rle ? file_pixels : out).push_back(src[0]);
                } else {
                    std::vector<std::uint8_t>& target = rle ? file_pixels : out;
                    target.push_back(src[2]);
                    target.push_back(src[1]);
                    target.push_back(src[0]);
                    if (image->mode == PILLOW_C_MODE_RGBA) {
                        target.push_back(src[3]);
                    }
                }
            }
        }
        if (rle) {
            encode_tga_rle_rows(file_pixels, file_pixel_bytes, image->width, image->height, out);
        }

        append_tga_footer(out);
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_tga_image(const PillowCImage* image, const char* path)
{
    return save_tga_image_with_options(image, path, false);
}

bool xbm_is_space(std::uint8_t value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' || value == '\v';
}

bool xbm_is_digit(std::uint8_t value)
{
    return value >= '0' && value <= '9';
}

int xbm_hex_value(std::uint8_t value)
{
    if (value >= '0' && value <= '9') {
        return static_cast<int>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<int>(value - 'a') + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<int>(value - 'A') + 10;
    }
    return -1;
}

bool xbm_match_at(const std::vector<std::uint8_t>& data, std::size_t pos, const char* text)
{
    const std::size_t length = std::strlen(text);
    return pos <= data.size() && length <= data.size() - pos &&
           std::memcmp(data.data() + pos, text, length) == 0;
}

bool xbm_identifier_ends_with(
    const std::vector<std::uint8_t>& data,
    std::size_t begin,
    std::size_t end,
    const char* suffix)
{
    const std::size_t suffix_length = std::strlen(suffix);
    return end >= begin && suffix_length <= end - begin &&
           std::memcmp(data.data() + end - suffix_length, suffix, suffix_length) == 0;
}

bool xbm_read_decimal_int(const std::vector<std::uint8_t>& data, std::size_t* pos, int* out_value)
{
    if (!pos || !out_value) {
        return false;
    }
    while (*pos < data.size() && xbm_is_space(data[*pos])) {
        ++*pos;
    }
    if (*pos >= data.size() || !xbm_is_digit(data[*pos])) {
        return false;
    }
    std::uint64_t value = 0;
    while (*pos < data.size() && xbm_is_digit(data[*pos])) {
        value = value * 10u + static_cast<std::uint64_t>(data[*pos] - '0');
        if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        ++*pos;
    }
    if (*pos < data.size() && !xbm_is_space(data[*pos])) {
        return false;
    }
    *out_value = static_cast<int>(value);
    return true;
}

bool xbm_read_byte_literal(const std::vector<std::uint8_t>& data, std::size_t* pos, std::uint8_t* out_value)
{
    if (!pos || !out_value) {
        return false;
    }
    while (*pos < data.size() && (xbm_is_space(data[*pos]) || data[*pos] == ',')) {
        ++*pos;
    }
    if (*pos >= data.size() || data[*pos] == '}') {
        return false;
    }

    int base = 10;
    if (data[*pos] == '0' && *pos + 1u < data.size() && (data[*pos + 1u] == 'x' || data[*pos + 1u] == 'X')) {
        base = 16;
        *pos += 2u;
    }

    int value = 0;
    int digits = 0;
    while (*pos < data.size()) {
        const int digit = base == 16 ? xbm_hex_value(data[*pos]) : (xbm_is_digit(data[*pos]) ? static_cast<int>(data[*pos] - '0') : -1);
        if (digit < 0 || digit >= base) {
            break;
        }
        value = value * base + digit;
        if (value > 255) {
            return false;
        }
        ++digits;
        ++*pos;
    }
    if (digits == 0) {
        return false;
    }
    *out_value = static_cast<std::uint8_t>(value);
    return true;
}

bool xbm_parse_header(
    const std::vector<std::uint8_t>& data,
    int* out_width,
    int* out_height,
    std::size_t* out_bits_offset,
    bool* out_has_hotspot,
    int* out_hotspot_x,
    int* out_hotspot_y)
{
    if (!out_width || !out_height || !out_bits_offset || !out_has_hotspot || !out_hotspot_x || !out_hotspot_y) {
        return false;
    }
    *out_width = 0;
    *out_height = 0;
    *out_bits_offset = 0;
    *out_has_hotspot = false;
    *out_hotspot_x = 0;
    *out_hotspot_y = 0;
    bool has_x_hot = false;
    bool has_y_hot = false;

    std::size_t pos = 0;
    while (pos < data.size()) {
        if (xbm_match_at(data, pos, "#define")) {
            pos += 7u;
            if (pos < data.size() && !(data[pos] == ' ' || data[pos] == '\t')) {
                continue;
            }
            while (pos < data.size() && (data[pos] == ' ' || data[pos] == '\t')) {
                ++pos;
            }
            const std::size_t name_start = pos;
            while (pos < data.size() && !xbm_is_space(data[pos])) {
                ++pos;
            }
            const std::size_t name_end = pos;
            int value = 0;
            if (!xbm_read_decimal_int(data, &pos, &value)) {
                continue;
            }
            if (xbm_identifier_ends_with(data, name_start, name_end, "_width")) {
                *out_width = value;
            } else if (xbm_identifier_ends_with(data, name_start, name_end, "_height")) {
                *out_height = value;
            } else if (xbm_identifier_ends_with(data, name_start, name_end, "_x_hot")) {
                *out_hotspot_x = value;
                has_x_hot = true;
                *out_has_hotspot = has_y_hot;
            } else if (xbm_identifier_ends_with(data, name_start, name_end, "_y_hot")) {
                *out_hotspot_y = value;
                has_y_hot = true;
                *out_has_hotspot = has_x_hot;
            }
            continue;
        }
        if (data[pos] == '_' && xbm_match_at(data, pos, "_bits[]")) {
            pos += 7u;
            while (pos < data.size() && xbm_is_space(data[pos])) {
                ++pos;
            }
            if (pos >= data.size() || data[pos] != '=') {
                return false;
            }
            ++pos;
            while (pos < data.size() && xbm_is_space(data[pos])) {
                ++pos;
            }
            if (pos >= data.size() || data[pos] != '{') {
                return false;
            }
            *out_bits_offset = pos + 1u;
            return *out_width > 0 && *out_height > 0;
        }
        ++pos;
    }
    return false;
}

int open_xbm_image(const char* path, PillowCImage** out_image)
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
        int width = 0;
        int height = 0;
        std::size_t bits_offset = 0;
        bool has_hotspot = false;
        int hotspot_x = 0;
        int hotspot_y = 0;
        if (!xbm_parse_header(data, &width, &height, &bits_offset, &has_hotspot, &hotspot_x, &hotspot_y)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::size_t packed_row_bytes = 0;
        std::size_t packed_size = 0;
        PillowCImage size_probe{width, height, PILLOW_C_MODE_1, 1, static_cast<std::size_t>(width), {}};
        if (!checked_mode1_raw_size(&size_probe, &packed_row_bytes, &packed_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, 1, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> packed(packed_size, 0);
        std::size_t pos = bits_offset;
        for (std::size_t i = 0; i < packed_size; ++i) {
            if (!xbm_read_byte_literal(data, &pos, &packed[i])) {
                return PILLOW_C_INVALID_LENGTH;
            }
        }

        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_1,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        image->has_hotspot = has_hotspot;
        image->hotspot_x = hotspot_x;
        image->hotspot_y = hotspot_y;
        for (int y = 0; y < height; ++y) {
            const std::uint8_t* src_row = packed.data() + static_cast<std::size_t>(y) * packed_row_bytes;
            std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const std::uint8_t packed_byte = src_row[static_cast<std::size_t>(x) / 8u];
                const int bit = (packed_byte >> (x & 7)) & 1;
                dst_row[x] = bit ? 255 : 0;
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

void append_xbm_hex_byte(std::vector<std::uint8_t>& out, std::uint8_t value)
{
    static constexpr char digits[] = "0123456789abcdef";
    out.push_back('0');
    out.push_back('x');
    out.push_back(static_cast<std::uint8_t>(digits[(value >> 4) & 0x0fu]));
    out.push_back(static_cast<std::uint8_t>(digits[value & 0x0fu]));
}

void append_ascii_text(std::vector<std::uint8_t>& out, const char* text)
{
    while (*text) {
        out.push_back(static_cast<std::uint8_t>(*text));
        ++text;
    }
}

bool read_ascii_name(const char* text, std::string* out)
{
    if (!text || !out) {
        return false;
    }
    out->clear();
    while (*text) {
        const auto ch = static_cast<unsigned char>(*text);
        if (ch > 0x7fu) {
            return false;
        }
        out->push_back(static_cast<char>(ch));
        ++text;
    }
    return true;
}

int append_xbm_bitmap_data(const PillowCImage* image, std::vector<std::uint8_t>* out)
{
    if (!image || !out) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->mode != PILLOW_C_MODE_1 || image->channels != 1 || image->width < 0 || image->height < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t packed_row_bytes = 0;
    std::size_t packed_size = 0;
    if (!checked_mode1_raw_size(image, &packed_row_bytes, &packed_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t written = 0;
    for (int y = 0; y < image->height; ++y) {
        const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (std::size_t byte_index = 0; byte_index < packed_row_bytes; ++byte_index) {
            std::uint8_t packed = 0;
            for (int bit = 0; bit < 8; ++bit) {
                const int x = static_cast<int>(byte_index * 8u) + bit;
                if (x < image->width && src_row[x] != 0) {
                    packed |= static_cast<std::uint8_t>(1u << bit);
                }
            }
            append_xbm_hex_byte(*out, packed);
            ++written;
            if (written < packed_size) {
                out->push_back(',');
                if (written % 15u == 0u) {
                    out->push_back('\n');
                }
            }
        }
    }
    if (packed_size > 0) {
        out->push_back('\n');
    }
    return PILLOW_C_OK;
}

int tobitmap_image(
    const PillowCImage* image,
    const char* name,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!image || !name || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_required = 0;
    if (image->mode != PILLOW_C_MODE_1 || image->channels != 1 || image->width < 0 || image->height < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::string ascii_name;
    if (!read_ascii_name(name, &ascii_name)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::size_t packed_row_bytes = 0;
        std::size_t packed_size = 0;
        if (!checked_mode1_raw_size(image, &packed_row_bytes, &packed_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> encoded;
        encoded.reserve(80u + ascii_name.size() * 3u + packed_size * 5u);
        append_ascii_text(encoded, "#define ");
        encoded.insert(encoded.end(), ascii_name.begin(), ascii_name.end());
        append_ascii_text(encoded, "_width ");
        append_ascii_int(encoded, image->width);
        encoded.push_back('\n');
        append_ascii_text(encoded, "#define ");
        encoded.insert(encoded.end(), ascii_name.begin(), ascii_name.end());
        append_ascii_text(encoded, "_height ");
        append_ascii_int(encoded, image->height);
        encoded.push_back('\n');
        append_ascii_text(encoded, "static char ");
        encoded.insert(encoded.end(), ascii_name.begin(), ascii_name.end());
        append_ascii_text(encoded, "_bits[] = {\n");
        const int status = append_xbm_bitmap_data(image, &encoded);
        if (status != PILLOW_C_OK) {
            return status;
        }
        encoded.push_back('}');
        encoded.push_back(';');

        *out_required = encoded.size();
        if (!out) {
            return PILLOW_C_OK;
        }
        if (out_size < encoded.size()) {
            return PILLOW_C_INVALID_LENGTH;
        }
        if (!encoded.empty()) {
            std::memcpy(out, encoded.data(), encoded.size());
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_xbm_image_with_options(
    const PillowCImage* image,
    const char* path,
    bool has_hotspot,
    int hotspot_x,
    int hotspot_y)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->mode != PILLOW_C_MODE_1 || image->channels != 1 || image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (has_hotspot && (hotspot_x < 0 || hotspot_y < 0)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::size_t packed_row_bytes = 0;
        std::size_t packed_size = 0;
        if (!checked_mode1_raw_size(image, &packed_row_bytes, &packed_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> out;
        out.reserve(120u + packed_size * 5u);
        out.insert(out.end(), {'#', 'd', 'e', 'f', 'i', 'n', 'e', ' ', 'i', 'm', '_', 'w', 'i', 'd', 't', 'h', ' '});
        append_ascii_int(out, image->width);
        out.push_back('\n');
        out.insert(out.end(), {'#', 'd', 'e', 'f', 'i', 'n', 'e', ' ', 'i', 'm', '_', 'h', 'e', 'i', 'g', 'h', 't', ' '});
        append_ascii_int(out, image->height);
        out.push_back('\n');
        if (has_hotspot) {
            out.insert(out.end(), {'#', 'd', 'e', 'f', 'i', 'n', 'e', ' ', 'i', 'm', '_', 'x', '_', 'h', 'o', 't', ' '});
            append_ascii_int(out, hotspot_x);
            out.push_back('\n');
            out.insert(out.end(), {'#', 'd', 'e', 'f', 'i', 'n', 'e', ' ', 'i', 'm', '_', 'y', '_', 'h', 'o', 't', ' '});
            append_ascii_int(out, hotspot_y);
            out.push_back('\n');
        }
        out.insert(out.end(), {
            's', 't', 'a', 't', 'i', 'c', ' ', 'c', 'h', 'a', 'r', ' ',
            'i', 'm', '_', 'b', 'i', 't', 's', '[', ']', ' ', '=', ' ', '{', '\n',
        });

        const int status = append_xbm_bitmap_data(image, &out);
        if (status != PILLOW_C_OK) {
            return status;
        }
        out.push_back('}');
        out.push_back(';');
        out.push_back('\n');
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_xbm_image(const PillowCImage* image, const char* path)
{
    return save_xbm_image_with_options(image, path, false, 0, 0);
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
                    image->palette_alpha.clear();
                    image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
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

int wic_container_frame_count(const char* path, const GUID& container_format, int* out_count)
{
    if (!path || !out_count) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_count = 0;
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
        GUID actual_container = {};
        if (FAILED(decoder->GetContainerFormat(&actual_container)) || !IsEqualGUID(actual_container, container_format)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        UINT frame_count = 0;
        hr = decoder->GetFrameCount(&frame_count);
        if (FAILED(hr) || frame_count > static_cast<UINT>(std::numeric_limits<int>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_count = static_cast<int>(frame_count);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_ico_image(const char* path, PillowCImage** out_image)
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
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatIco)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        UINT frame_count = 0;
        hr = decoder->GetFrameCount(&frame_count);
        if (FAILED(hr) || frame_count == 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        UINT width_u = 0;
        UINT height_u = 0;
        std::uint64_t best_area = 0;
        for (UINT index = 0; index < frame_count; ++index) {
            ComPtr<IWICBitmapFrameDecode> candidate;
            hr = decoder->GetFrame(index, candidate.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            UINT candidate_width = 0;
            UINT candidate_height = 0;
            hr = candidate->GetSize(&candidate_width, &candidate_height);
            if (FAILED(hr) ||
                candidate_width > static_cast<UINT>(std::numeric_limits<int>::max()) ||
                candidate_height > static_cast<UINT>(std::numeric_limits<int>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::uint64_t area =
                static_cast<std::uint64_t>(candidate_width) * static_cast<std::uint64_t>(candidate_height);
            if (!frame.get() || area > best_area) {
                frame.reset(candidate.get());
                frame.get()->AddRef();
                width_u = candidate_width;
                height_u = candidate_height;
                best_area = area;
            }
        }
        if (!frame.get()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = static_cast<int>(width_u);
        const int height = static_cast<int>(height_u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, 4, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID source_format = {};
        hr = frame->GetPixelFormat(&source_format);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapSource> source;
        if (IsEqualGUID(source_format, GUID_WICPixelFormat32bppRGBA)) {
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
                GUID_WICPixelFormat32bppRGBA,
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
            PILLOW_C_MODE_RGBA,
            4,
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
            image->palette_alpha.clear();
            image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        }
        double dpi_x = 0.0;
        double dpi_y = 0.0;
        if (read_png_dpi_metadata(path, &dpi_x, &dpi_y)) {
            image->has_dpi = true;
            image->dpi_x = dpi_x;
            image->dpi_y = dpi_y;
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

int png_custom_mode_spec(const PillowCImage* image, int* color_type, int* payload_channels)
{
    if (!image || !color_type || !payload_channels) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
        *color_type = 0;
        *payload_channels = 1;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
        *color_type = 2;
        *payload_channels = 3;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_P && image->channels == 1) {
        *color_type = 3;
        *payload_channels = 1;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_LA && image->channels == 2) {
        *color_type = 4;
        *payload_channels = 2;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4) {
        *color_type = 6;
        *payload_channels = 4;
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

bool png_dpi_to_pixels_per_meter(double dpi, std::uint32_t* out_value)
{
    if (!out_value || !std::isfinite(dpi) || dpi <= 0.0) {
        return false;
    }
    const double pixels_per_meter = dpi / 0.0254 + 0.5;
    if (pixels_per_meter < 0.0 || pixels_per_meter > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
        return false;
    }
    *out_value = static_cast<std::uint32_t>(pixels_per_meter);
    return true;
}

int append_png_phys_chunk(std::vector<std::uint8_t>& png, double dpi_x, double dpi_y)
{
    std::uint32_t x_pixels_per_meter = 0;
    std::uint32_t y_pixels_per_meter = 0;
    if (!png_dpi_to_pixels_per_meter(dpi_x, &x_pixels_per_meter) ||
        !png_dpi_to_pixels_per_meter(dpi_y, &y_pixels_per_meter)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<std::uint8_t> phys;
    append_be32(phys, x_pixels_per_meter);
    append_be32(phys, y_pixels_per_meter);
    phys.push_back(1);
    append_png_chunk(png, "pHYs", phys);
    return PILLOW_C_OK;
}

int encode_png_custom_image(
    const PillowCImage* image,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    std::vector<std::uint8_t>* out_png)
{
    if (!image || !out_png) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int color_type = 0;
    int payload_channels = 0;
    int status = png_custom_mode_spec(image, &color_type, &payload_channels);
    if (status != PILLOW_C_OK) {
        return status;
    }
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
        ihdr.push_back(static_cast<std::uint8_t>(color_type));
        ihdr.push_back(0);
        ihdr.push_back(0);
        ihdr.push_back(0);
        append_png_chunk(png, "IHDR", ihdr);

        if (has_dpi) {
            status = append_png_phys_chunk(png, dpi_x, dpi_y);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

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

        *out_png = std::move(png);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_png_custom_image_with_dpi(const PillowCImage* image, const char* path, bool has_dpi, double dpi_x, double dpi_y)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image(image, has_dpi, dpi_x, dpi_y, &png);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image(const PillowCImage* image, const char* path)
{
    return save_png_custom_image_with_dpi(image, path, false, 0.0, 0.0);
}

int proportional_resize_size(
    const PillowCImage* source,
    int requested_width,
    int requested_height,
    bool cover,
    int* out_width,
    int* out_height);

int resize_image_into(const PillowCImage* source, int out_width, int out_height, int resample, PillowCImage* target);

struct IcoRequestedSize {
    int width;
    int height;
};

struct IcoResource {
    int width;
    int height;
    int bit_count;
    int color_count;
    std::vector<std::uint8_t> payload;
};

int encode_ico_dib_image(const PillowCImage* image, std::vector<std::uint8_t>* out, int* out_bit_count, int* out_color_count)
{
    if (!image || !out || !out_bit_count || !out_color_count) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0 || image->width > 256 || image->height > 256) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int bits_per_pixel = 0;
    int dib_color_count = 0;
    int directory_color_count = 0;
    std::size_t palette_entries = 0;
    if (image->mode == PILLOW_C_MODE_1) {
        bits_per_pixel = 1;
        dib_color_count = 2;
        directory_color_count = 2;
        palette_entries = 2;
    } else if (image->mode == PILLOW_C_MODE_L) {
        bits_per_pixel = 8;
        dib_color_count = 256;
        directory_color_count = 0;
        palette_entries = 256;
    } else if (image->mode == PILLOW_C_MODE_P) {
        if (image->palette_rgb.size() > 256u * 3u || image->palette_rgb.size() % 3u != 0u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        bits_per_pixel = 8;
        dib_color_count = static_cast<int>(image->palette_rgb.size() / 3u);
        directory_color_count = 0;
        palette_entries = static_cast<std::size_t>(dib_color_count);
    } else if (image->mode == PILLOW_C_MODE_RGB) {
        bits_per_pixel = 24;
        dib_color_count = 0;
        directory_color_count = 0;
    } else if (image->mode == PILLOW_C_MODE_RGBA) {
        bits_per_pixel = 32;
        dib_color_count = 0;
        directory_color_count = 0;
    } else {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t xor_stride = 0;
    int status = bmp_row_stride(image->width, bits_per_pixel, &xor_stride);
    if (status != PILLOW_C_OK) {
        return status;
    }
    const bool has_and_mask = bits_per_pixel != 32;
    std::size_t and_stride = 0;
    if (has_and_mask) {
        and_stride = (static_cast<std::size_t>(image->width) + 7u) / 8u;
    }

    const std::uint64_t xor_size_u64 =
        static_cast<std::uint64_t>(xor_stride) * static_cast<std::uint64_t>(image->height);
    const std::uint64_t and_size_u64 =
        static_cast<std::uint64_t>(and_stride) * static_cast<std::uint64_t>(image->height);
    const std::uint64_t palette_size_u64 = static_cast<std::uint64_t>(palette_entries) * 4u;
    const std::uint64_t total_size_u64 = 40u + palette_size_u64 + xor_size_u64 + and_size_u64;
    if (xor_size_u64 > std::numeric_limits<std::uint32_t>::max() ||
        total_size_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<std::uint8_t> dib;
        dib.reserve(static_cast<std::size_t>(total_size_u64));
        append_le32(dib, 40);
        append_le32(dib, static_cast<std::uint32_t>(image->width));
        append_le32(dib, static_cast<std::uint32_t>(image->height * 2));
        append_le16(dib, 1);
        append_le16(dib, static_cast<std::uint16_t>(bits_per_pixel));
        append_le32(dib, 0);
        append_le32(dib, static_cast<std::uint32_t>(xor_size_u64));
        append_le32(dib, 3780);
        append_le32(dib, 3780);
        append_le32(dib, static_cast<std::uint32_t>(dib_color_count));
        append_le32(dib, static_cast<std::uint32_t>(dib_color_count));

        if (image->mode == PILLOW_C_MODE_1) {
            dib.push_back(0);
            dib.push_back(0);
            dib.push_back(0);
            dib.push_back(0);
            dib.push_back(255);
            dib.push_back(255);
            dib.push_back(255);
            dib.push_back(0);
        } else if (image->mode == PILLOW_C_MODE_L) {
            for (int value = 0; value < 256; ++value) {
                const auto byte = static_cast<std::uint8_t>(value);
                dib.push_back(byte);
                dib.push_back(byte);
                dib.push_back(byte);
                dib.push_back(0);
            }
        } else if (image->mode == PILLOW_C_MODE_P) {
            for (std::size_t index = 0; index < palette_entries; ++index) {
                const std::size_t src = index * 3u;
                dib.push_back(image->palette_rgb[src + 2u]);
                dib.push_back(image->palette_rgb[src + 1u]);
                dib.push_back(image->palette_rgb[src + 0u]);
                dib.push_back(0);
            }
        }

        std::vector<std::uint8_t> row(xor_stride, std::uint8_t{0});
        for (int y = image->height - 1; y >= 0; --y) {
            std::fill(row.begin(), row.end(), std::uint8_t{0});
            const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            if (image->mode == PILLOW_C_MODE_1) {
                for (int x = 0; x < image->width; ++x) {
                    if (src_row[x] != 0) {
                        row[static_cast<std::size_t>(x) / 8u] |=
                            static_cast<std::uint8_t>(0x80u >> (static_cast<unsigned>(x) & 7u));
                    }
                }
            } else if (image->mode == PILLOW_C_MODE_L || image->mode == PILLOW_C_MODE_P) {
                std::memcpy(row.data(), src_row, static_cast<std::size_t>(image->width));
            } else if (image->mode == PILLOW_C_MODE_RGB) {
                for (int x = 0; x < image->width; ++x) {
                    row[static_cast<std::size_t>(x) * 3u + 0u] = src_row[static_cast<std::size_t>(x) * 3u + 2u];
                    row[static_cast<std::size_t>(x) * 3u + 1u] = src_row[static_cast<std::size_t>(x) * 3u + 1u];
                    row[static_cast<std::size_t>(x) * 3u + 2u] = src_row[static_cast<std::size_t>(x) * 3u + 0u];
                }
            } else {
                for (int x = 0; x < image->width; ++x) {
                    row[static_cast<std::size_t>(x) * 4u + 0u] = src_row[static_cast<std::size_t>(x) * 4u + 2u];
                    row[static_cast<std::size_t>(x) * 4u + 1u] = src_row[static_cast<std::size_t>(x) * 4u + 1u];
                    row[static_cast<std::size_t>(x) * 4u + 2u] = src_row[static_cast<std::size_t>(x) * 4u + 0u];
                    row[static_cast<std::size_t>(x) * 4u + 3u] = src_row[static_cast<std::size_t>(x) * 4u + 3u];
                }
            }
            dib.insert(dib.end(), row.begin(), row.end());
        }

        if (has_and_mask) {
            dib.resize(dib.size() + static_cast<std::size_t>(and_size_u64), std::uint8_t{0});
        }

        *out_bit_count = bits_per_pixel;
        *out_color_count = directory_color_count;
        *out = std::move(dib);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_ico_image_with_sizes(
    const PillowCImage* image,
    const char* path,
    const IcoRequestedSize* requested_sizes,
    std::size_t requested_count,
    bool bitmap_format_bmp)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (requested_count > 0 && !requested_sizes) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    if (requested_count == 0) {
        std::vector<std::uint8_t> ico;
        ico.reserve(6);
        append_le16(ico, 0);
        append_le16(ico, 1);
        append_le16(ico, 0);
        return write_binary_file(path, ico) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    }

    int status = PILLOW_C_OK;
    if (!bitmap_format_bmp) {
        int color_type = 0;
        int payload_channels = 0;
        status = png_custom_mode_spec(image, &color_type, &payload_channels);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    try {
        std::vector<IcoRequestedSize> sizes;
        sizes.reserve(requested_count);
        for (std::size_t index = 0; index < requested_count; ++index) {
            const IcoRequestedSize requested = requested_sizes[index];
            if (requested.width <= 0 || requested.height <= 0) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            sizes.push_back(requested);
        }
        std::sort(sizes.begin(), sizes.end(), [](const IcoRequestedSize& left, const IcoRequestedSize& right) {
            if (left.width != right.width) {
                return left.width < right.width;
            }
            return left.height < right.height;
        });
        sizes.erase(
            std::unique(sizes.begin(), sizes.end(), [](const IcoRequestedSize& left, const IcoRequestedSize& right) {
                return left.width == right.width && left.height == right.height;
            }),
            sizes.end());

        std::vector<IcoResource> resources;
        resources.reserve(sizes.size());

        for (const IcoRequestedSize& requested : sizes) {
            if (requested.width > image->width || requested.height > image->height ||
                requested.width > 256 || requested.height > 256) {
                continue;
            }

            const PillowCImage* frame = image;
            PillowCImage resized{};
            if (requested.width != image->width || requested.height != image->height) {
                int out_width = 0;
                int out_height = 0;
                status = proportional_resize_size(image, requested.width, requested.height, false, &out_width, &out_height);
                if (status != PILLOW_C_OK) {
                    return status;
                }
                std::size_t stride = 0;
                std::size_t size = 0;
                if (!checked_image_size(out_width, out_height, image->channels, &stride, &size)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                resized = PillowCImage{
                    out_width,
                    out_height,
                    image->mode,
                    image->channels,
                    stride,
                    std::vector<std::uint8_t>(size)};
                status = resize_image_into(image, out_width, out_height, PILLOW_C_RESAMPLE_LANCZOS, &resized);
                if (status != PILLOW_C_OK) {
                    return status;
                }
                copy_palette_if_same_mode(image, &resized);
                frame = &resized;
            }

            IcoResource resource{};
            resource.width = frame->width;
            resource.height = frame->height;
            resource.bit_count = 32;
            resource.color_count = 0;
            if (bitmap_format_bmp) {
                status = encode_ico_dib_image(frame, &resource.payload, &resource.bit_count, &resource.color_count);
                if (status != PILLOW_C_OK) {
                    return status;
                }
            } else {
                status = encode_png_custom_image(frame, false, 0.0, 0.0, &resource.payload);
                if (status != PILLOW_C_OK) {
                    return status;
                }
            }
            if (resource.width > 256 || resource.height > 256 ||
                resource.payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            resources.push_back(std::move(resource));
        }

        if (resources.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::uint64_t total_size = 6u + static_cast<std::uint64_t>(resources.size()) * 16u;
        for (const IcoResource& resource : resources) {
            total_size += static_cast<std::uint64_t>(resource.payload.size());
            if (total_size > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }

        std::vector<std::uint8_t> ico;
        ico.reserve(static_cast<std::size_t>(total_size));
        append_le16(ico, 0);
        append_le16(ico, 1);
        append_le16(ico, static_cast<std::uint16_t>(resources.size()));

        std::uint32_t offset = static_cast<std::uint32_t>(6u + resources.size() * 16u);
        for (const IcoResource& resource : resources) {
            ico.push_back(static_cast<std::uint8_t>(resource.width == 256 ? 0 : resource.width));
            ico.push_back(static_cast<std::uint8_t>(resource.height == 256 ? 0 : resource.height));
            ico.push_back(static_cast<std::uint8_t>(resource.color_count >= 256 ? 0 : resource.color_count));
            ico.push_back(0);
            append_le16(ico, 0);
            append_le16(ico, static_cast<std::uint16_t>(resource.bit_count));
            append_le32(ico, static_cast<std::uint32_t>(resource.payload.size()));
            append_le32(ico, offset);
            offset += static_cast<std::uint32_t>(resource.payload.size());
        }
        for (const IcoResource& resource : resources) {
            ico.insert(ico.end(), resource.payload.begin(), resource.payload.end());
        }

        if (!write_binary_file(path, ico)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_ico_image(const PillowCImage* image, const char* path)
{
    static constexpr IcoRequestedSize default_sizes[] = {
        {16, 16},
        {24, 24},
        {32, 32},
        {48, 48},
        {64, 64},
        {128, 128},
        {256, 256},
    };
    return save_ico_image_with_sizes(
        image,
        path,
        default_sizes,
        sizeof(default_sizes) / sizeof(default_sizes[0]),
        false);
}

int save_ico_image_options(
    const PillowCImage* image,
    const char* path,
    const int* sizes,
    std::size_t size_count)
{
    if (size_count > 0 && !sizes) {
        return PILLOW_C_NULL_POINTER;
    }
    try {
        std::vector<IcoRequestedSize> requested;
        requested.reserve(size_count);
        for (std::size_t index = 0; index < size_count; ++index) {
            requested.push_back(IcoRequestedSize{
                sizes[index * 2],
                sizes[index * 2 + 1],
            });
        }
        return save_ico_image_with_sizes(
            image,
            path,
            requested.empty() ? nullptr : requested.data(),
            requested.size(),
            false);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_ico_image_format_options(
    const PillowCImage* image,
    const char* path,
    const int* sizes,
    std::size_t size_count,
    int has_sizes,
    const char* bitmap_format)
{
    static constexpr IcoRequestedSize default_sizes[] = {
        {16, 16},
        {24, 24},
        {32, 32},
        {48, 48},
        {64, 64},
        {128, 128},
        {256, 256},
    };
    if (has_sizes && size_count > 0 && !sizes) {
        return PILLOW_C_NULL_POINTER;
    }
    const bool bitmap_format_bmp = bitmap_format && std::strcmp(bitmap_format, "bmp") == 0;
    try {
        std::vector<IcoRequestedSize> requested;
        if (has_sizes) {
            requested.reserve(size_count);
            for (std::size_t index = 0; index < size_count; ++index) {
                requested.push_back(IcoRequestedSize{
                    sizes[index * 2],
                    sizes[index * 2 + 1],
                });
            }
        }
        const IcoRequestedSize* requested_data = nullptr;
        std::size_t requested_count = 0;
        if (has_sizes) {
            requested_data = requested.empty() ? nullptr : requested.data();
            requested_count = requested.size();
        } else {
            requested_data = default_sizes;
            requested_count = sizeof(default_sizes) / sizeof(default_sizes[0]);
        }
        return save_ico_image_with_sizes(
            image,
            path,
            requested_data,
            requested_count,
            bitmap_format_bmp);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_png_image_with_dpi(const PillowCImage* image, const char* path, bool has_dpi, double dpi_x, double dpi_y)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return save_png_custom_image_with_dpi(image, path, true, dpi_x, dpi_y);
    }
    if (image->mode == PILLOW_C_MODE_LA || image->mode == PILLOW_C_MODE_P) {
        return save_png_custom_image_with_dpi(image, path, has_dpi, dpi_x, dpi_y);
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

int save_png_image(const PillowCImage* image, const char* path)
{
    return save_png_image_with_dpi(image, path, false, 0.0, 0.0);
}

int save_png_image_with_compress_level(const PillowCImage* image, const char* path, int compress_level)
{
    if (compress_level == -1) {
        return save_png_image(image, path);
    }
    if (compress_level < 0 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (compress_level == 0) {
        return save_png_custom_image(image, path);
    }
    return save_png_image(image, path);
}

int save_png_image_with_options(const PillowCImage* image, const char* path, int compress_level, double dpi_x, double dpi_y)
{
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    if (compress_level == 0) {
        return save_png_custom_image_with_dpi(image, path, has_dpi, dpi_x, dpi_y);
    }
    return save_png_image_with_dpi(image, path, has_dpi, dpi_x, dpi_y);
}

int open_jpeg_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        JpegMetadata metadata;
        if (!read_jpeg_metadata(path, &metadata)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (metadata.components != 1 && metadata.components != 3) {
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

        const int mode = metadata.components == 1 ? PILLOW_C_MODE_L : PILLOW_C_MODE_RGB;
        const int channels = metadata.components == 1 ? 1 : 3;
        const WICPixelFormatGUID target_format =
            metadata.components == 1 ? GUID_WICPixelFormat8bppGray : GUID_WICPixelFormat24bppRGB;

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
        image->exif_orientation = metadata.exif_orientation;
        image->has_dpi = metadata.has_dpi;
        image->dpi_x = metadata.dpi_x;
        image->dpi_y = metadata.dpi_y;
        image->has_jfif = metadata.has_jfif;
        image->jfif_major = metadata.jfif_major;
        image->jfif_minor = metadata.jfif_minor;
        image->jfif_unit = metadata.jfif_unit;
        image->jfif_density_x = metadata.jfif_density_x;
        image->jfif_density_y = metadata.jfif_density_y;
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

int save_jpeg_image_with_options(const PillowCImage* image, const char* path, int quality, bool has_dpi, double dpi_x, double dpi_y)
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
    const bool has_quality = quality != -1;
    const int clamped_quality = std::max(0, std::min(quality, 100));
    if (has_dpi) {
        std::uint8_t unit = 0;
        std::uint16_t x_density = 0;
        std::uint16_t y_density = 0;
        const int status = jpeg_density_from_dpi(dpi_x, dpi_y, &unit, &x_density, &y_density);
        if (status != PILLOW_C_OK) {
            return status;
        }
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
        ComPtr<IPropertyBag2> encoder_options;
        hr = has_quality ? encoder->CreateNewFrame(frame.put(), encoder_options.put()) :
                           encoder->CreateNewFrame(frame.put(), nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (has_quality) {
            if (!encoder_options.get()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            PROPBAG2 option = {};
            option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
            VARIANT value;
            VariantInit(&value);
            value.vt = VT_R4;
            value.fltVal = static_cast<float>(clamped_quality) / 100.0f;
            hr = encoder_options->Write(1, &option, &value);
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
        hr = frame->Initialize(has_quality ? encoder_options.get() : nullptr);
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
        frame.reset();
        encoder_options.reset();
        encoder.reset();
        stream.reset();
        factory.reset();
        if (has_dpi) {
            return patch_jpeg_jfif_density(path, dpi_x, dpi_y);
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_jpeg_image_with_quality(const PillowCImage* image, const char* path, int quality)
{
    return save_jpeg_image_with_options(image, path, quality, false, 0.0, 0.0);
}

int save_jpeg_image(const PillowCImage* image, const char* path)
{
    return save_jpeg_image_with_quality(image, path, -1);
}

int open_tiff_frame_image(const char* path, int frame_index, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (frame_index < 0) {
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
        UINT frame_count = 0;
        hr = decoder->GetFrameCount(&frame_count);
        if (FAILED(hr) || static_cast<UINT>(frame_index) >= frame_count) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(static_cast<UINT>(frame_index), frame.put());
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

int open_tiff_image(const char* path, PillowCImage** out_image)
{
    return open_tiff_frame_image(path, 0, out_image);
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

int open_gif_frame_image(const char* path, int frame_index, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (frame_index < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (frame_index > 0) {
        const int composited_status = open_gif_composited_frame_image(path, frame_index, out_image);
        if (composited_status == PILLOW_C_OK) {
            return PILLOW_C_OK;
        }
        if (composited_status == PILLOW_C_ALLOCATION_FAILED) {
            return composited_status;
        }
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
        UINT frame_count = 0;
        hr = decoder->GetFrameCount(&frame_count);
        if (FAILED(hr) || static_cast<UINT>(frame_index) >= frame_count) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(static_cast<UINT>(frame_index), frame.put());
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

        if (frame_index > 0) {
            std::size_t stride = 0;
            std::size_t size = 0;
            if (!checked_image_size(width, height, 3, &stride, &size) ||
                stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
                size > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            ComPtr<IWICFormatConverter> converter;
            hr = factory->CreateFormatConverter(converter.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = converter->Initialize(
                frame.get(),
                GUID_WICPixelFormat24bppBGR,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            std::vector<std::uint8_t> bgr(size, std::uint8_t{0});
            hr = converter->CopyPixels(nullptr, static_cast<UINT>(stride), static_cast<UINT>(bgr.size()), bgr.data());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            auto* image = new PillowCImage{
                width,
                height,
                PILLOW_C_MODE_RGB,
                3,
                stride,
                std::vector<std::uint8_t>(size)};
            for (int y = 0; y < height; ++y) {
                const std::uint8_t* src_row = bgr.data() + static_cast<std::size_t>(y) * stride;
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < width; ++x) {
                    const std::size_t offset = static_cast<std::size_t>(x) * 3u;
                    dst_row[offset + 0u] = src_row[offset + 2u];
                    dst_row[offset + 1u] = src_row[offset + 1u];
                    dst_row[offset + 2u] = src_row[offset + 0u];
                }
            }
            *out_image = image;
            return PILLOW_C_OK;
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
        image->palette_alpha.clear();
        image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_gif_image(const char* path, PillowCImage** out_image)
{
    return open_gif_frame_image(path, 0, out_image);
}

int quantize_exact_image_into(const PillowCImage* source, int colors, PillowCImage* target);
int quantize_exact_rgba_gif_into(const PillowCImage* source, PillowCImage* target, bool* out_has_transparency, int* out_transparency);
int save_gif_indexed_native(const PillowCImage* image, const char* path, bool has_transparency, int transparency);

int save_gif_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if ((image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
        (image->mode == PILLOW_C_MODE_RGB && image->channels == 3)) {
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size_allow_empty(image->width, image->height, 1, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        try {
            PillowCImage quantized{
                image->width,
                image->height,
                PILLOW_C_MODE_P,
                1,
                stride,
                std::vector<std::uint8_t>(size)};
            const int status = quantize_exact_image_into(image, 256, &quantized);
            if (status != PILLOW_C_OK) {
                return status;
            }
            return save_gif_image(&quantized, path);
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
    }
    if (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4) {
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size_allow_empty(image->width, image->height, 1, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        try {
            PillowCImage quantized{
                image->width,
                image->height,
                PILLOW_C_MODE_P,
                1,
                stride,
                std::vector<std::uint8_t>(size)};
            bool has_transparency = false;
            int transparency = 0;
            const int status = quantize_exact_rgba_gif_into(image, &quantized, &has_transparency, &transparency);
            if (status != PILLOW_C_OK) {
                return status;
            }
            return save_gif_indexed_native(&quantized, path, has_transparency, transparency);
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
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

struct GifBitWriter {
    std::vector<std::uint8_t> bytes;
    std::uint32_t bits = 0;
    int bit_count = 0;

    void write(int code, int size)
    {
        bits |= static_cast<std::uint32_t>(code) << bit_count;
        bit_count += size;
        while (bit_count >= 8) {
            bytes.push_back(static_cast<std::uint8_t>(bits & 0xffu));
            bits >>= 8;
            bit_count -= 8;
        }
    }

    void flush()
    {
        if (bit_count > 0) {
            bytes.push_back(static_cast<std::uint8_t>(bits & 0xffu));
            bits = 0;
            bit_count = 0;
        }
    }
};

int gif_color_table_entries(const PillowCImage* image, int* out_entries, int* out_min_code_size)
{
    if (!image || !out_entries || !out_min_code_size ||
        image->palette_rgb.empty() || image->palette_rgb.size() % 3u != 0u ||
        image->palette_rgb.size() > 256u * 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t palette_entries = image->palette_rgb.size() / 3u;
    int table_entries = 2;
    while (static_cast<std::size_t>(table_entries) < palette_entries) {
        table_entries <<= 1;
    }
    if (table_entries > 256) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int bits = 0;
    while ((1 << bits) < table_entries) {
        ++bits;
    }
    *out_entries = table_entries;
    *out_min_code_size = std::max(2, bits);
    return PILLOW_C_OK;
}

void append_gif_sub_blocks(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& data)
{
    std::size_t offset = 0;
    while (offset < data.size()) {
        const std::size_t chunk = std::min<std::size_t>(255u, data.size() - offset);
        out.push_back(static_cast<std::uint8_t>(chunk));
        out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(offset),
                   data.begin() + static_cast<std::ptrdiff_t>(offset + chunk));
        offset += chunk;
    }
    out.push_back(0);
}

bool gif_lzw_encode_indices(
    const std::uint8_t* pixels,
    int width,
    int height,
    std::size_t stride,
    int color_table_entries,
    int min_code_size,
    std::vector<std::uint8_t>* out);

bool gif_lzw_encode_image(
    const PillowCImage* image,
    int color_table_entries,
    int min_code_size,
    std::vector<std::uint8_t>* out)
{
    if (!image || !out || image->width <= 0 || image->height <= 0 || color_table_entries <= 0) {
        return false;
    }
    return gif_lzw_encode_indices(
        image->pixels.data(),
        image->width,
        image->height,
        image->stride,
        color_table_entries,
        min_code_size,
        out);
}

bool gif_lzw_encode_indices(
    const std::uint8_t* pixels,
    int width,
    int height,
    std::size_t stride,
    int color_table_entries,
    int min_code_size,
    std::vector<std::uint8_t>* out)
{
    if (!pixels || !out || width <= 0 || height <= 0 || stride < static_cast<std::size_t>(width) ||
        color_table_entries <= 0 || min_code_size < 2 || min_code_size > 8) {
        return false;
    }
    const int clear_code = 1 << min_code_size;
    const int end_code = clear_code + 1;
    int next_code = end_code + 1;
    int code_size = min_code_size + 1;

    GifBitWriter writer;
    std::unordered_map<std::uint32_t, int> dictionary;
    dictionary.reserve(4096);

    writer.write(clear_code, code_size);
    int prefix = -1;
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* row = pixels + static_cast<std::size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const int value = row[x];
            if (value < 0 || value >= color_table_entries) {
                return false;
            }
            if (prefix < 0) {
                prefix = value;
                continue;
            }

            const std::uint32_t key = (static_cast<std::uint32_t>(prefix) << 8) |
                                      static_cast<std::uint32_t>(value);
            const auto found = dictionary.find(key);
            if (found != dictionary.end()) {
                prefix = found->second;
                continue;
            }

            writer.write(prefix, code_size);
            if (next_code <= 4095) {
                dictionary.emplace(key, next_code++);
                if (next_code > (1 << code_size) && code_size < 12) {
                    ++code_size;
                }
            } else {
                writer.write(clear_code, code_size);
                dictionary.clear();
                next_code = end_code + 1;
                code_size = min_code_size + 1;
            }
            prefix = value;
        }
    }
    if (prefix < 0) {
        return false;
    }
    writer.write(prefix, code_size);
    writer.write(end_code, code_size);
    writer.flush();
    *out = std::move(writer.bytes);
    return true;
}

int gif_table_size_code_for_entries(int entries)
{
    int table_size_code = 0;
    for (int size = 2; size < entries; size <<= 1) {
        ++table_size_code;
    }
    return table_size_code;
}

int gif_color_table_entries_for_palette_size(std::size_t palette_rgb_size, int* out_entries, int* out_min_code_size)
{
    if (!out_entries || !out_min_code_size ||
        palette_rgb_size == 0u || palette_rgb_size % 3u != 0u || palette_rgb_size > 256u * 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t palette_entries = palette_rgb_size / 3u;
    int table_entries = 2;
    while (static_cast<std::size_t>(table_entries) < palette_entries) {
        table_entries <<= 1;
    }
    if (table_entries > 256) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int bits = 0;
    while ((1 << bits) < table_entries) {
        ++bits;
    }
    *out_entries = table_entries;
    *out_min_code_size = std::max(2, bits);
    return PILLOW_C_OK;
}

struct GifAnimationRect {
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
};

bool gif_palette_index_rgb(const PillowCImage* image, std::uint8_t index, std::uint8_t* rgb)
{
    if (!image || !rgb || image->palette_rgb.size() % 3u != 0u || image->palette_rgb.size() > 256u * 3u) {
        return false;
    }
    const std::size_t offset = static_cast<std::size_t>(index) * 3u;
    if (offset + 2u >= image->palette_rgb.size()) {
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0;
        return true;
    }
    rgb[0] = image->palette_rgb[offset + 0u];
    rgb[1] = image->palette_rgb[offset + 1u];
    rgb[2] = image->palette_rgb[offset + 2u];
    return true;
}

bool gif_palette_pixels_equal(
    const PillowCImage* left,
    std::uint8_t left_index,
    const PillowCImage* right,
    std::uint8_t right_index)
{
    std::uint8_t left_rgb[3] = {};
    std::uint8_t right_rgb[3] = {};
    return gif_palette_index_rgb(left, left_index, left_rgb) &&
           gif_palette_index_rgb(right, right_index, right_rgb) &&
           left_rgb[0] == right_rgb[0] &&
           left_rgb[1] == right_rgb[1] &&
           left_rgb[2] == right_rgb[2];
}

bool gif_difference_bbox(const PillowCImage* previous, const PillowCImage* current, GifAnimationRect* out_rect)
{
    if (!previous || !current || !out_rect ||
        previous->width != current->width || previous->height != current->height ||
        previous->channels != 1 || current->channels != 1) {
        return false;
    }
    int left = current->width;
    int top = current->height;
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < current->height; ++y) {
        const std::uint8_t* prev_row = previous->pixels.data() + static_cast<std::size_t>(y) * previous->stride;
        const std::uint8_t* curr_row = current->pixels.data() + static_cast<std::size_t>(y) * current->stride;
        for (int x = 0; x < current->width; ++x) {
            if (gif_palette_pixels_equal(previous, prev_row[x], current, curr_row[x])) {
                continue;
            }
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x + 1);
            bottom = std::max(bottom, y + 1);
        }
    }
    if (right < 0 || bottom < 0) {
        out_rect->left = 0;
        out_rect->top = 0;
        out_rect->width = 0;
        out_rect->height = 0;
        return true;
    }
    out_rect->left = left;
    out_rect->top = top;
    out_rect->width = right - left;
    out_rect->height = bottom - top;
    return true;
}

bool gif_difference_bbox_against_background_rgb(
    const PillowCImage* current,
    const std::uint8_t background_rgb[3],
    GifAnimationRect* out_rect)
{
    if (!current || !background_rgb || !out_rect || current->channels != 1) {
        return false;
    }
    int left = current->width;
    int top = current->height;
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < current->height; ++y) {
        const std::uint8_t* curr_row = current->pixels.data() + static_cast<std::size_t>(y) * current->stride;
        for (int x = 0; x < current->width; ++x) {
            std::uint8_t curr_rgb[3] = {};
            if (!gif_palette_index_rgb(current, curr_row[x], curr_rgb)) {
                return false;
            }
            if (curr_rgb[0] == background_rgb[0] &&
                curr_rgb[1] == background_rgb[1] &&
                curr_rgb[2] == background_rgb[2]) {
                continue;
            }
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x + 1);
            bottom = std::max(bottom, y + 1);
        }
    }
    if (right < 0 || bottom < 0) {
        out_rect->left = 0;
        out_rect->top = 0;
        out_rect->width = 0;
        out_rect->height = 0;
        return true;
    }
    out_rect->left = left;
    out_rect->top = top;
    out_rect->width = right - left;
    out_rect->height = bottom - top;
    return true;
}

int gif_find_unused_palette_index(const PillowCImage* image, int palette_entries)
{
    if (!image || palette_entries < 0 || palette_entries > 256) {
        return -1;
    }
    if (palette_entries < 256) {
        return palette_entries;
    }
    bool used[256] = {};
    for (int y = 0; y < image->height; ++y) {
        const std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = 0; x < image->width; ++x) {
            used[row[x]] = true;
        }
    }
    for (int index = 255; index >= 0; --index) {
        if (!used[index]) {
            return index;
        }
    }
    return -1;
}

int gif_sequence_value(const int* values, std::size_t count, std::size_t index, int fallback, bool* ok)
{
    if (!ok || count == 0 || !values) {
        return fallback;
    }
    if (count == 1) {
        return values[0];
    }
    if (index >= count) {
        *ok = false;
        return fallback;
    }
    return values[index];
}

int save_gif_indexed_native(
    const PillowCImage* image,
    const char* path,
    bool has_transparency,
    int transparency)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0 ||
        image->mode != PILLOW_C_MODE_P || image->channels != 1 ||
        image->width > std::numeric_limits<std::uint16_t>::max() ||
        image->height > std::numeric_limits<std::uint16_t>::max()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int color_table_entries = 0;
    int min_code_size = 0;
    int status = gif_color_table_entries(image, &color_table_entries, &min_code_size);
    if (status != PILLOW_C_OK) {
        return status;
    }

    try {
        std::vector<std::uint8_t> lzw;
        if (!gif_lzw_encode_image(image, color_table_entries, min_code_size, &lzw)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> gif;
        gif.reserve(32u + image->palette_rgb.size() + lzw.size());
        gif.push_back('G');
        gif.push_back('I');
        gif.push_back('F');
        gif.push_back('8');
        gif.push_back(static_cast<std::uint8_t>(has_transparency ? '9' : '7'));
        gif.push_back('a');
        append_le16(gif, static_cast<std::uint16_t>(image->width));
        append_le16(gif, static_cast<std::uint16_t>(image->height));
        int table_size_code = 0;
        for (int entries = 2; entries < color_table_entries; entries <<= 1) {
            ++table_size_code;
        }
        const int color_resolution = std::max(0, min_code_size - 1);
        gif.push_back(static_cast<std::uint8_t>(0x80 | ((color_resolution & 0x07) << 4) | (table_size_code & 0x07)));
        gif.push_back(0);
        gif.push_back(0);
        gif.insert(gif.end(), image->palette_rgb.begin(), image->palette_rgb.end());
        gif.resize(gif.size() + static_cast<std::size_t>(color_table_entries) * 3u - image->palette_rgb.size(), 0);

        if (has_transparency) {
            gif.insert(gif.end(), {0x21, 0xf9, 0x04, 0x01});
            append_le16(gif, 0);
            gif.push_back(static_cast<std::uint8_t>(transparency & 0xff));
            gif.push_back(0);
        }

        gif.push_back(0x2c);
        append_le16(gif, 0);
        append_le16(gif, 0);
        append_le16(gif, static_cast<std::uint16_t>(image->width));
        append_le16(gif, static_cast<std::uint16_t>(image->height));
        gif.push_back(0);
        gif.push_back(static_cast<std::uint8_t>(min_code_size));
        append_gif_sub_blocks(gif, lzw);
        gif.push_back(0x3b);

        if (!write_binary_file(path, gif)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_gif_animation_image(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    int include_color_table,
    int optimize,
    int has_transparency,
    int transparency,
    int has_background,
    int background)
{
    if (!images || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image_count == 0 || image_count > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        (duration_count != 0 && duration_count != 1 && duration_count != image_count) ||
        (disposal_count != 0 && disposal_count != 1 && disposal_count != image_count) ||
        loop < -1 ||
        include_color_table < -1 || include_color_table > 1 ||
        optimize < -1 || optimize > 1 ||
        (has_transparency != 0 && has_transparency != 1) ||
        (has_transparency && (transparency < 0 || transparency > 255)) ||
        (has_background != 0 && has_background != 1) ||
        (has_background && (background < 0 || background > 255))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool force_first_local_color_table = include_color_table == 1;
    const bool optimize_enabled = optimize != 0;
    const bool caller_has_transparency = has_transparency != 0;
    const int caller_transparency = transparency & 0xff;
    const int logical_screen_background = has_background ? (background & 0xff) : 0;
    const PillowCImage* first = images[0];
    if (!first || first->width <= 0 || first->height <= 0 ||
        first->mode != PILLOW_C_MODE_P || first->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int color_table_entries = 0;
    int min_code_size = 0;
    int status = gif_color_table_entries(first, &color_table_entries, &min_code_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (color_table_entries < 4) {
        color_table_entries = 4;
    }
    if (caller_has_transparency) {
        while (color_table_entries <= caller_transparency && color_table_entries < 256) {
            color_table_entries <<= 1;
        }
        if (color_table_entries <= caller_transparency) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    if (!optimize_enabled) {
        min_code_size = 8;
    }
    if (first->width > std::numeric_limits<std::uint16_t>::max() ||
        first->height > std::numeric_limits<std::uint16_t>::max()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    for (std::size_t i = 0; i < image_count; ++i) {
        const PillowCImage* image = images[i];
        if (!image || image->mode != PILLOW_C_MODE_P || image->channels != 1 ||
            image->width != first->width || image->height != first->height ||
            image->stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
            return i == 0 ? PILLOW_C_INVALID_ARGUMENT : PILLOW_C_MISMATCH;
        }
        int image_color_table_entries = 0;
        int image_min_code_size = 0;
        status = gif_color_table_entries(image, &image_color_table_entries, &image_min_code_size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    try {
        struct GifAnimationOutputFrame {
            int left = 0;
            int top = 0;
            int width = 0;
            int height = 0;
            int duration_ms = 0;
            int disposal = 0;
            int transparency = -1;
            int color_table_entries = 0;
            int min_code_size = 0;
            bool include_local_color_table = false;
            std::vector<std::uint8_t> palette_rgb;
            std::vector<std::uint8_t> pixels;
        };

        std::vector<GifAnimationOutputFrame> frames;
        frames.reserve(image_count);
        const PillowCImage* previous_image = nullptr;

        for (std::size_t i = 0; i < image_count; ++i) {
            bool ok = true;
            const int duration = gif_sequence_value(durations_ms, duration_count, i, 0, &ok);
            const int disposal = gif_sequence_value(disposals, disposal_count, i, 0, &ok);
            if (!ok || duration < 0 || disposal < 0 || disposal > 7) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            const PillowCImage* image = images[i];
            GifAnimationOutputFrame frame;
            frame.duration_ms = duration;
            frame.disposal = disposal;

            if (frames.empty()) {
                frame.left = 0;
                frame.top = 0;
                frame.width = image->width;
                frame.height = image->height;
                frame.color_table_entries = color_table_entries;
                frame.min_code_size = min_code_size;
                frame.include_local_color_table = force_first_local_color_table;
                if (caller_has_transparency && !optimize_enabled) {
                    frame.transparency = caller_transparency;
                }
                if (frame.include_local_color_table) {
                    frame.palette_rgb = image->palette_rgb;
                    if (caller_has_transparency && frame.transparency >= 0) {
                        const int current_palette_entries = static_cast<int>(frame.palette_rgb.size() / 3u);
                        if (frame.transparency >= current_palette_entries) {
                            frame.palette_rgb.resize(
                                (static_cast<std::size_t>(frame.transparency) + 1u) * 3u, 0);
                        }
                    }
                }
                frame.pixels.resize(static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height));
                for (int y = 0; y < image->height; ++y) {
                    const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                    std::uint8_t* dst_row = frame.pixels.data() + static_cast<std::size_t>(y) * image->width;
                    std::copy_n(src_row, image->width, dst_row);
                }
                frames.push_back(std::move(frame));
                previous_image = image;
                continue;
            }

            GifAnimationRect rect;
            if (!gif_difference_bbox(previous_image, image, &rect)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (rect.width == 0 || rect.height == 0) {
                if (duration > 0) {
                    const long long merged_duration =
                        static_cast<long long>(frames.back().duration_ms) + static_cast<long long>(duration);
                    frames.back().duration_ms = merged_duration > std::numeric_limits<int>::max()
                        ? std::numeric_limits<int>::max()
                        : static_cast<int>(merged_duration);
                }
                continue;
            }

            const bool previous_restores_to_background = frames.back().disposal == 2;
            const bool use_transparency_background_rediff =
                previous_restores_to_background && caller_has_transparency && optimize_enabled;
            if (use_transparency_background_rediff) {
                std::uint8_t background_rgb[3] = {};
                if (!gif_palette_index_rgb(
                        first, static_cast<std::uint8_t>(caller_transparency), background_rgb) ||
                    !gif_difference_bbox_against_background_rgb(image, background_rgb, &rect)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                if (rect.width == 0 || rect.height == 0) {
                    rect.left = 0;
                    rect.top = 0;
                    rect.width = image->width;
                    rect.height = image->height;
                }
            } else if (previous_restores_to_background) {
                rect.left = 0;
                rect.top = 0;
                rect.width = image->width;
                rect.height = image->height;
            }

            frame.left = rect.left;
            frame.top = rect.top;
            frame.width = rect.width;
            frame.height = rect.height;
            frame.include_local_color_table = true;
            frame.palette_rgb = image->palette_rgb;
            const int source_palette_entries = static_cast<int>(image->palette_rgb.size() / 3u);
            if (caller_has_transparency && !use_transparency_background_rediff) {
                frame.transparency = caller_transparency;
                const int current_palette_entries = static_cast<int>(frame.palette_rgb.size() / 3u);
                if (frame.transparency >= current_palette_entries) {
                    frame.palette_rgb.resize((static_cast<std::size_t>(frame.transparency) + 1u) * 3u, 0);
                } else if (optimize_enabled && !previous_restores_to_background &&
                           current_palette_entries < 256) {
                    frame.palette_rgb.resize((static_cast<std::size_t>(current_palette_entries) + 1u) * 3u, 0);
                }
            } else if (optimize_enabled && !previous_restores_to_background) {
                frame.transparency = gif_find_unused_palette_index(image, source_palette_entries);
                if (frame.transparency >= source_palette_entries) {
                    frame.palette_rgb.resize((static_cast<std::size_t>(frame.transparency) + 1u) * 3u, 0);
                }
            }
            status = gif_color_table_entries_for_palette_size(
                frame.palette_rgb.size(), &frame.color_table_entries, &frame.min_code_size);
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (frame.color_table_entries < 4) {
                frame.color_table_entries = 4;
            }
            if (!optimize_enabled) {
                frame.min_code_size = 8;
            }

            frame.pixels.resize(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height));
            for (int y = 0; y < frame.height; ++y) {
                const std::uint8_t* prev_row = previous_image->pixels.data() +
                    static_cast<std::size_t>(frame.top + y) * previous_image->stride;
                const std::uint8_t* curr_row = image->pixels.data() +
                    static_cast<std::size_t>(frame.top + y) * image->stride;
                std::uint8_t* dst_row = frame.pixels.data() + static_cast<std::size_t>(y) * frame.width;
                for (int x = 0; x < frame.width; ++x) {
                    const int src_x = frame.left + x;
                    std::uint8_t value = curr_row[src_x];
                    if (frame.transparency >= 0 &&
                        gif_palette_pixels_equal(previous_image, prev_row[src_x], image, curr_row[src_x])) {
                        value = static_cast<std::uint8_t>(frame.transparency);
                    }
                    dst_row[x] = value;
                }
            }

            frames.push_back(std::move(frame));
            previous_image = image;
        }

        std::vector<std::uint8_t> gif;
        gif.reserve(32u + first->palette_rgb.size() + frames.size() * (first->pixels.size() + 32u));
        gif.insert(gif.end(), {'G', 'I', 'F', '8', '9', 'a'});
        append_le16(gif, static_cast<std::uint16_t>(first->width));
        append_le16(gif, static_cast<std::uint16_t>(first->height));
        const int table_size_code = gif_table_size_code_for_entries(color_table_entries);
        const int color_resolution = std::max(0, min_code_size - 1);
        gif.push_back(static_cast<std::uint8_t>(0x80 | ((color_resolution & 0x07) << 4) | (table_size_code & 0x07)));
        gif.push_back(static_cast<std::uint8_t>(logical_screen_background));
        gif.push_back(0);
        gif.insert(gif.end(), first->palette_rgb.begin(), first->palette_rgb.end());
        gif.resize(gif.size() + static_cast<std::size_t>(color_table_entries) * 3u - first->palette_rgb.size(), 0);

        if (loop >= 0) {
            gif.insert(gif.end(), {0x21, 0xff, 0x0b, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
                                   0x03, 0x01});
            append_le16(gif, static_cast<std::uint16_t>(loop));
            gif.push_back(0);
        }

        for (const GifAnimationOutputFrame& frame : frames) {
            const int delay_cs = std::min(frame.duration_ms / 10, 65535);
            const std::uint8_t gce_packed = static_cast<std::uint8_t>(
                ((frame.disposal & 0x07) << 2) | (frame.transparency >= 0 ? 0x01 : 0x00));
            gif.insert(gif.end(), {0x21, 0xf9, 0x04, gce_packed});
            append_le16(gif, static_cast<std::uint16_t>(delay_cs));
            gif.push_back(frame.transparency >= 0 ? static_cast<std::uint8_t>(frame.transparency & 0xff) : 0);
            gif.push_back(0);

            gif.push_back(0x2c);
            append_le16(gif, static_cast<std::uint16_t>(frame.left));
            append_le16(gif, static_cast<std::uint16_t>(frame.top));
            append_le16(gif, static_cast<std::uint16_t>(frame.width));
            append_le16(gif, static_cast<std::uint16_t>(frame.height));
            std::uint8_t image_flags = 0;
            if (frame.include_local_color_table) {
                image_flags = static_cast<std::uint8_t>(
                    0x80 | (gif_table_size_code_for_entries(frame.color_table_entries) & 0x07));
            }
            gif.push_back(image_flags);
            if (frame.include_local_color_table) {
                gif.insert(gif.end(), frame.palette_rgb.begin(), frame.palette_rgb.end());
                gif.resize(gif.size() + static_cast<std::size_t>(frame.color_table_entries) * 3u -
                           frame.palette_rgb.size(), 0);
            }
            gif.push_back(static_cast<std::uint8_t>(frame.min_code_size));

            std::vector<std::uint8_t> lzw;
            if (!gif_lzw_encode_indices(
                    frame.pixels.data(),
                    frame.width,
                    frame.height,
                    static_cast<std::size_t>(frame.width),
                    frame.color_table_entries,
                    frame.min_code_size,
                    &lzw)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            append_gif_sub_blocks(gif, lzw);
        }

        gif.push_back(0x3b);
        if (!write_binary_file(path, gif)) {
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
                row[x] = pillow_clip8_double(128.0 + static_cast<double>(sigma_f) * factor * v1);
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
    case PILLOW_C_MODE_I:
        switch (spec.kind) {
        case RawCodecKind::I:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            return;
        case RawCodecKind::I32Big:
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
            return;
        case RawCodecKind::I32Native:
            if (native_is_little_endian()) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = src[3];
            } else {
                dst[0] = src[3];
                dst[1] = src[2];
                dst[2] = src[1];
                dst[3] = src[0];
            }
            return;
        case RawCodecKind::I16Little:
            write_i32_le(dst, read_u16_le(src));
            return;
        case RawCodecKind::I16Big:
            write_i32_le(dst, read_u16_be(src));
            return;
        case RawCodecKind::I16Native:
            write_i32_le(dst, native_is_little_endian() ? read_u16_le(src) : read_u16_be(src));
            return;
        default:
            return;
        }
        return;
    case PILLOW_C_MODE_F:
        if (spec.kind == RawCodecKind::F ||
            (spec.kind == RawCodecKind::F32Native && native_is_little_endian())) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        } else if (spec.kind == RawCodecKind::F32Big ||
                   spec.kind == RawCodecKind::F32Native) {
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
        }
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
    case PILLOW_C_MODE_I:
        switch (spec.kind) {
        case RawCodecKind::I:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            return;
        case RawCodecKind::I16Big: {
            const std::uint16_t value = clip_i32_to_u16(read_i32_le(src));
            dst[0] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
            dst[1] = static_cast<std::uint8_t>(value & 0xFFu);
            return;
        }
        default:
            return;
        }
        return;
    case PILLOW_C_MODE_F:
        if (spec.kind == RawCodecKind::F ||
            (spec.kind == RawCodecKind::F32Native && native_is_little_endian())) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        } else if (spec.kind == RawCodecKind::F32Native) {
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
        }
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

// Generated from local Pillow 11.3.0 ImageFont.load_default() masks for printable ASCII.
static constexpr std::uint8_t DEFAULT_FONT_MASK_DATA[] = {
    0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0,
    240, 0, 0, 11, 0, 0, 184, 0, 0, 240, 240, 0, 240, 240, 0, 202,
    202, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 123, 30, 145, 0, 0, 0, 149, 34, 115, 0, 0, 0, 149, 70,
    77, 0, 0, 160, 215, 201, 173, 15, 0, 36, 112, 147, 0, 0, 57, 214,
    207, 229, 153, 0, 0, 137, 27, 133, 0, 0, 0, 149, 68, 80, 0, 0,
    0, 0, 0, 120, 0, 0, 0, 0, 14, 124, 250, 123, 7, 0, 0, 181,
    107, 241, 121, 145, 0, 0, 236, 12, 240, 10, 106, 0, 0, 171, 170, 245,
    20, 0, 0, 0, 6, 109, 251, 232, 84, 0, 0, 0, 0, 240, 43, 225,
    0, 0, 192, 6, 240, 16, 219, 0, 0, 82, 193, 250, 195, 68, 0, 0,
    0, 0, 240, 0, 0, 0, 120, 212, 117, 0, 2, 181, 16, 231, 14, 231,
    0, 102, 98, 0, 226, 36, 225, 18, 181, 1, 0, 93, 200, 90, 150, 52,
    0, 0, 0, 0, 51, 150, 86, 181, 84, 0, 1, 181, 19, 224, 24, 222,
    0, 99, 104, 0, 233, 25, 232, 16, 184, 2, 0, 120, 213, 117, 0, 48,
    184, 189, 193, 39, 0, 0, 214, 39, 0, 0, 0, 0, 0, 211, 43, 0,
    0, 221, 0, 0, 45, 236, 213, 192, 252, 177, 0, 172, 111, 1, 0, 240,
    0, 0, 237, 3, 0, 0, 240, 0, 0, 208, 59, 0, 0, 240, 0, 0,
    53, 192, 192, 190, 202, 0, 240, 240, 202, 0, 0, 0, 0, 0, 0, 0,
    75, 5, 0, 23, 165, 0, 0, 122, 90, 0, 0, 194, 33, 0, 0, 232,
    3, 0, 0, 237, 0, 0, 0, 218, 13, 0, 0, 162, 57, 0, 0, 72,
    128, 0, 0, 1, 165, 5, 5, 73, 0, 0, 0, 165, 23, 0, 0, 91,
    121, 0, 0, 35, 193, 0, 0, 4, 231, 0, 0, 0, 237, 0, 0, 14,
    218, 0, 0, 57, 162, 0, 0, 129, 71, 0, 5, 163, 0, 0, 0, 0,
    0, 240, 0, 0, 0, 0, 90, 68, 240, 68, 88, 0, 0, 9, 118, 232,
    115, 8, 0, 0, 10, 176, 21, 175, 10, 0, 0, 1, 25, 0, 25, 1,
    0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0,
    150, 192, 252, 192, 150, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0,
    240, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 38, 0, 45, 191,
    0, 138, 80, 0, 132, 192, 81, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    12, 0, 186, 0, 0, 0, 0, 157, 2, 0, 0, 7, 151, 0, 0, 0,
    72, 86, 0, 0, 0, 143, 15, 0, 0, 0, 157, 0, 0, 0, 42, 115,
    0, 0, 0, 118, 39, 0, 0, 0, 156, 0, 0, 0, 0, 76, 0, 0,
    0, 15, 176, 196, 176, 15, 0, 138, 114, 0, 115, 136, 0, 212, 26, 0,
    27, 211, 0, 235, 2, 0, 2, 234, 0, 235, 2, 0, 2, 234, 0, 212,
    26, 0, 27, 211, 0, 138, 114, 0, 115, 137, 0, 15, 176, 196, 176, 15,
    0, 0, 16, 160, 241, 0, 0, 0, 195, 109, 240, 0, 0, 0, 24, 0,
    240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0,
    0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0,
    0, 0, 98, 195, 208, 70, 0, 21, 193, 2, 40, 215, 0, 40, 85, 0,
    11, 234, 0, 0, 0, 0, 95, 157, 0, 0, 0, 35, 212, 21, 0, 0,
    12, 204, 55, 0, 0, 1, 173, 89, 0, 0, 0, 97, 244, 192, 192, 180,
    0, 10, 169, 192, 205, 75, 0, 118, 116, 0, 25, 224, 0, 17, 6, 0,
    68, 219, 0, 0, 0, 163, 245, 67, 0, 0, 0, 0, 79, 162, 0, 147,
    4, 0, 3, 237, 0, 162, 84, 0, 61, 198, 0, 30, 191, 193, 187, 40,
    0, 0, 0, 0, 74, 247, 0, 0, 0, 13, 198, 242, 0, 0, 0, 149,
    86, 240, 0, 0, 60, 175, 0, 240, 0, 7, 200, 26, 0, 240, 0, 90,
    213, 192, 192, 252, 162, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240,
    0, 69, 222, 192, 192, 132, 0, 93, 104, 0, 0, 0, 0, 118, 79, 0,
    0, 0, 0, 143, 154, 196, 178, 35, 0, 142, 107, 0, 82, 190, 0, 32,
    3, 0, 3, 235, 0, 168, 79, 0, 58, 190, 0, 37, 196, 192, 184, 35,
    0, 2, 146, 199, 194, 34, 0, 106, 133, 0, 81, 174, 0, 197, 36, 0,
    6, 118, 0, 235, 114, 192, 175, 32, 0, 241, 83, 0, 83, 189, 0, 218,
    3, 0, 3, 235, 0, 152, 65, 0, 65, 188, 0, 22, 179, 191, 187, 35,
    0, 168, 192, 192, 198, 236, 0, 0, 0, 0, 100, 130, 0, 0, 0, 1,
    207, 22, 0, 0, 0, 78, 154, 0, 0, 0, 0, 193, 39, 0, 0, 0,
    55, 177, 0, 0, 0, 0, 173, 62, 0, 0, 0, 36, 199, 0, 0, 0,
    0, 67, 198, 191, 195, 57, 0, 224, 33, 0, 35, 214, 0, 205, 52, 0,
    54, 225, 0, 53, 248, 211, 248, 76, 0, 193, 83, 0, 85, 158, 0, 238,
    2, 0, 3, 236, 0, 199, 61, 0, 60, 204, 0, 47, 193, 193, 192, 48,
    0, 33, 185, 192, 180, 23, 0, 187, 67, 0, 67, 151, 0, 235, 3, 0,
    3, 217, 0, 189, 81, 0, 84, 240, 0, 33, 175, 191, 115, 234, 0, 122,
    6, 0, 37, 196, 0, 174, 80, 0, 133, 107, 0, 36, 196, 199, 147, 2,
    0, 185, 0, 13, 0, 0, 0, 0, 0, 12, 0, 186, 0, 0, 185, 0,
    0, 13, 0, 0, 0, 0, 0, 0, 0, 6, 147, 0, 65, 125, 0, 134,
    63, 0, 0, 0, 0, 0, 60, 0, 0, 0, 37, 161, 129, 0, 0, 140,
    142, 22, 0, 0, 0, 170, 115, 7, 0, 0, 0, 0, 74, 182, 96, 0,
    0, 0, 0, 3, 102, 0, 39, 192, 192, 192, 192, 0, 0, 0, 0, 0,
    0, 0, 39, 192, 192, 192, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 61, 0, 0, 0, 0, 0, 133, 163, 37, 0, 0,
    0, 0, 26, 149, 142, 0, 0, 0, 5, 107, 168, 0, 0, 88, 176, 74,
    0, 0, 0, 101, 3, 0, 0, 0, 60, 200, 205, 76, 206, 38, 25, 218,
    96, 0, 6, 231, 0, 0, 52, 178, 0, 0, 131, 73, 0, 0, 168, 1,
    0, 0, 24, 0, 0, 0, 186, 0, 0, 0, 0, 89, 182, 196, 184, 63,
    0, 0, 0, 0, 158, 162, 19, 0, 43, 214, 52, 0, 0, 90, 169, 16,
    169, 187, 145, 78, 182, 0, 0, 193, 51, 156, 98, 37, 220, 11, 230, 0,
    0, 235, 3, 228, 8, 44, 167, 5, 225, 0, 0, 236, 14, 228, 11, 139,
    122, 81, 153, 0, 0, 184, 80, 92, 169, 104, 171, 144, 13, 0, 0, 58,
    220, 56, 0, 5, 84, 66, 0, 0, 0, 0, 54, 178, 201, 188, 103, 3,
    0, 0, 0, 0, 100, 249, 23, 0, 0, 0, 0, 177, 139, 97, 0, 0,
    0, 10, 188, 39, 173, 0, 0, 0, 81, 116, 0, 205, 6, 0, 0, 160,
    208, 200, 231, 69, 0, 3, 198, 0, 0, 71, 145, 0, 62, 151, 0, 0,
    10, 215, 0, 141, 85, 0, 0, 0, 193, 41, 0, 246, 192, 193, 207, 77,
    0, 240, 0, 0, 47, 225, 0, 240, 0, 0, 1, 221, 0, 240, 0, 0,
    67, 77, 0, 246, 192, 199, 252, 186, 0, 240, 0, 0, 45, 240, 0, 240,
    0, 0, 26, 205, 0, 246, 192, 190, 190, 52, 0, 101, 206, 193, 181, 29,
    0, 69, 190, 9, 0, 73, 189, 0, 185, 56, 0, 0, 0, 154, 5, 228,
    7, 0, 0, 0, 0, 0, 231, 7, 0, 0, 0, 0, 0, 193, 55, 0,
    0, 1, 161, 0, 87, 187, 7, 0, 89, 168, 0, 0, 124, 212, 191, 169,
    20, 0, 0, 246, 192, 191, 197, 96, 0, 0, 240, 0, 0, 11, 189, 85,
    0, 240, 0, 0, 0, 51, 196, 0, 240, 0, 0, 0, 5, 232, 0, 240,
    0, 0, 0, 9, 227, 0, 240, 0, 0, 0, 63, 182, 0, 240, 0, 0,
    17, 200, 64, 0, 246, 192, 191, 196, 81, 0, 0, 246, 192, 192, 192, 114,
    0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 0, 246, 192, 192, 192, 57, 0, 240, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 246, 192, 192, 192, 135, 0, 246, 192, 192, 192, 114,
    0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 0, 246, 192, 192, 192, 39, 0, 240, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 102, 202, 198, 182, 28,
    0, 69, 183, 5, 0, 119, 185, 0, 185, 53, 0, 0, 13, 176, 3, 228,
    7, 0, 0, 0, 0, 0, 232, 7, 0, 114, 192, 216, 0, 195, 53, 0,
    0, 16, 247, 0, 93, 183, 5, 0, 123, 244, 0, 1, 134, 215, 189, 97,
    240, 0, 0, 240, 0, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 0,
    240, 0, 0, 240, 0, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 0,
    240, 0, 0, 246, 192, 192, 192, 192, 246, 0, 0, 240, 0, 0, 0, 0,
    240, 0, 0, 240, 0, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 0,
    240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240,
    0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0,
    0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0,
    240, 0, 0, 0, 0, 0, 240, 0, 0, 194, 0, 0, 240, 0, 0, 213,
    29, 35, 216, 0, 0, 75, 205, 204, 74, 0, 0, 240, 0, 0, 29, 207,
    24, 0, 240, 0, 8, 196, 56, 0, 0, 240, 0, 160, 100, 0, 0, 0,
    240, 110, 152, 0, 0, 0, 0, 246, 217, 176, 0, 0, 0, 0, 243, 14,
    187, 104, 0, 0, 0, 240, 0, 28, 229, 41, 0, 0, 240, 0, 0, 89,
    208, 7, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0,
    0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 246, 196, 196,
    196, 137, 0, 247, 170, 0, 0, 0, 164, 247, 0, 0, 240, 209, 3, 0,
    1, 199, 240, 0, 0, 240, 159, 56, 0, 49, 153, 240, 0, 0, 240, 88,
    127, 0, 120, 82, 240, 0, 0, 240, 18, 195, 0, 186, 14, 240, 0, 0,
    240, 0, 193, 29, 186, 0, 240, 0, 0, 240, 0, 121, 162, 118, 0, 240,
    0, 0, 240, 0, 46, 254, 44, 0, 240, 0, 0, 247, 191, 0, 0, 0,
    240, 0, 0, 240, 195, 64, 0, 0, 240, 0, 0, 240, 66, 193, 0, 0,
    240, 0, 0, 240, 0, 193, 65, 0, 240, 0, 0, 240, 0, 64, 194, 0,
    240, 0, 0, 240, 0, 0, 191, 66, 240, 0, 0, 240, 0, 0, 62, 195,
    240, 0, 0, 240, 0, 0, 0, 189, 247, 0, 0, 97, 205, 199, 205, 95,
    0, 71, 196, 16, 0, 16, 196, 68, 188, 57, 0, 0, 0, 58, 187, 229,
    8, 0, 0, 0, 8, 229, 229, 8, 0, 0, 0, 9, 228, 187, 58, 0,
    0, 0, 59, 185, 70, 196, 15, 0, 16, 196, 67, 0, 98, 206, 199, 205,
    96, 0, 0, 246, 192, 191, 196, 53, 0, 240, 0, 0, 45, 203, 0, 240,
    0, 0, 0, 236, 0, 240, 0, 0, 67, 188, 0, 246, 192, 193, 178, 32,
    0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 0, 97, 205, 199, 204, 91, 0, 71, 196, 16, 0, 16, 196, 62,
    188, 57, 0, 0, 0, 58, 182, 229, 8, 0, 0, 0, 8, 225, 229, 8,
    0, 0, 0, 9, 237, 186, 58, 0, 0, 0, 59, 195, 67, 196, 15, 0,
    16, 195, 75, 0, 95, 204, 201, 242, 242, 109, 0, 0, 0, 0, 0, 20,
    64, 0, 246, 192, 193, 206, 63, 0, 0, 240, 0, 0, 45, 210, 0, 0,
    240, 0, 0, 1, 237, 0, 0, 240, 0, 0, 71, 171, 0, 0, 246, 192,
    197, 237, 24, 0, 0, 240, 0, 0, 118, 120, 0, 0, 240, 0, 0, 49,
    174, 0, 0, 240, 0, 0, 10, 220, 2, 0, 50, 186, 191, 187, 29, 0,
    208, 28, 0, 83, 169, 0, 230, 36, 0, 5, 61, 0, 96, 231, 157, 65,
    0, 0, 0, 17, 97, 209, 128, 21, 58, 0, 0, 21, 232, 20, 228, 29,
    0, 40, 202, 0, 74, 200, 191, 188, 44, 81, 192, 192, 252, 192, 192, 84,
    0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0,
    0, 240, 0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 240, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 240, 0, 0,
    240, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 240, 0, 0, 240, 0,
    0, 0, 240, 0, 0, 238, 3, 0, 4, 236, 0, 0, 185, 72, 0, 75,
    183, 0, 0, 38, 193, 193, 193, 38, 0, 138, 99, 0, 0, 0, 186, 47,
    62, 172, 0, 0, 12, 216, 0, 3, 221, 4, 0, 80, 144, 0, 0, 165,
    60, 0, 155, 65, 0, 0, 88, 133, 0, 211, 4, 0, 0, 16, 200, 49,
    162, 0, 0, 0, 0, 190, 147, 82, 0, 0, 0, 0, 114, 243, 11, 0,
    0, 160, 85, 0, 0, 209, 143, 0, 0, 144, 84, 102, 138, 0, 13, 205,
    198, 0, 0, 199, 23, 44, 190, 0, 69, 131, 208, 7, 5, 209, 0, 1,
    224, 1, 127, 70, 157, 56, 53, 156, 0, 0, 184, 39, 181, 13, 99, 112,
    108, 94, 0, 0, 126, 93, 190, 0, 41, 168, 163, 33, 0, 0, 68, 188,
    146, 0, 1, 205, 190, 0, 0, 0, 12, 251, 86, 0, 0, 180, 165, 0,
    0, 71, 174, 0, 0, 43, 202, 3, 0, 180, 62, 0, 186, 56, 0, 0,
    39, 199, 83, 155, 0, 0, 0, 0, 144, 226, 19, 0, 0, 0, 0, 158,
    222, 31, 0, 0, 0, 59, 173, 69, 177, 0, 0, 4, 202, 34, 0, 173,
    79, 0, 114, 134, 0, 0, 30, 215, 11, 42, 211, 2, 0, 0, 204, 44,
    0, 167, 88, 0, 75, 169, 0, 0, 41, 211, 2, 196, 42, 0, 0, 0,
    165, 155, 166, 0, 0, 0, 0, 39, 253, 39, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 141, 192, 192, 195, 250, 0, 0, 0, 0, 0, 111, 135, 0, 0,
    0, 0, 26, 209, 10, 0, 0, 0, 0, 169, 75, 0, 0, 0, 0, 71,
    172, 0, 0, 0, 0, 8, 207, 28, 0, 0, 0, 0, 131, 114, 0, 0,
    0, 0, 0, 249, 195, 192, 192, 192, 12, 0, 216, 192, 3, 0, 240, 0,
    0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0,
    0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 216, 192,
    3, 1, 154, 0, 0, 0, 0, 146, 10, 0, 0, 0, 80, 77, 0, 0,
    0, 12, 145, 0, 0, 0, 0, 157, 0, 0, 0, 0, 110, 48, 0, 0,
    0, 35, 123, 0, 0, 0, 0, 159, 0, 0, 0, 0, 77, 2, 3, 192,
    216, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0,
    240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0,
    240, 0, 3, 192, 216, 0, 0, 0, 68, 42, 0, 0, 0, 0, 158, 142,
    0, 0, 0, 57, 103, 142, 15, 0, 0, 151, 17, 59, 105, 0, 13, 162,
    0, 0, 171, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    96, 192, 192, 192, 96, 52, 119, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 43, 196, 212, 113, 0, 151,
    61, 18, 230, 0, 13, 101, 137, 244, 0, 184, 101, 28, 242, 0, 236, 15,
    36, 249, 0, 130, 213, 166, 233, 7, 0, 240, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 240, 139, 195, 194, 33, 0, 247, 87, 0, 92, 175,
    0, 246, 6, 0, 7, 232, 0, 244, 3, 0, 7, 225, 0, 248, 77, 0,
    92, 160, 0, 240, 160, 194, 178, 21, 20, 174, 192, 186, 22, 161, 89, 0,
    97, 151, 228, 6, 0, 3, 15, 231, 7, 0, 0, 0, 177, 86, 0, 100,
    135, 35, 192, 193, 178, 16, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0,
    240, 0, 23, 178, 195, 160, 240, 0, 163, 91, 0, 80, 248, 0, 227, 7,
    0, 4, 245, 0, 233, 7, 0, 6, 246, 0, 175, 86, 0, 87, 247, 0,
    33, 194, 194, 136, 240, 0, 17, 164, 188, 194, 54, 0, 156, 55, 0, 30,
    208, 0, 227, 192, 192, 192, 219, 3, 231, 8, 0, 2, 62, 0, 172, 89,
    0, 75, 183, 0, 29, 183, 193, 189, 34, 0, 0, 40, 119, 15, 0, 215,
    110, 11, 0, 239, 0, 0, 105, 252, 192, 12, 0, 240, 0, 0, 0, 240,
    0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 23, 178,
    195, 163, 236, 0, 163, 91, 0, 80, 248, 0, 227, 7, 0, 4, 245, 0,
    233, 7, 0, 6, 246, 0, 175, 86, 0, 87, 247, 0, 33, 194, 194, 137,
    236, 0, 160, 46, 0, 53, 194, 0, 51, 194, 185, 191, 44, 0, 0, 240,
    0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240, 133, 193,
    211, 81, 0, 0, 247, 113, 0, 38, 226, 0, 0, 248, 16, 0, 0, 241,
    0, 0, 242, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 240, 0, 0,
    240, 0, 0, 0, 240, 0, 43, 105, 0, 0, 0, 0, 0, 240, 0, 0,
    240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 54, 95,
    0, 0, 0, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0,
    0, 240, 0, 0, 240, 0, 3, 239, 0, 169, 165, 0, 0, 240, 0, 0,
    0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 22, 210, 43, 0, 240,
    6, 190, 66, 0, 0, 240, 155, 99, 0, 0, 0, 247, 199, 155, 0, 0,
    0, 240, 2, 187, 101, 0, 0, 240, 0, 21, 221, 54, 0, 240, 0, 0,
    240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240,
    0, 0, 203, 121, 0, 240, 158, 221, 104, 164, 222, 113, 0, 0, 248, 55,
    30, 254, 54, 31, 230, 0, 0, 246, 3, 0, 248, 2, 0, 240, 0, 0,
    240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240,
    0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 133, 193, 209, 81,
    0, 0, 246, 113, 0, 34, 226, 0, 0, 248, 16, 0, 0, 241, 0, 0,
    242, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 240, 0, 0, 240, 0,
    0, 0, 240, 0, 24, 182, 194, 182, 24, 165, 93, 0, 93, 164, 229, 7,
    0, 7, 229, 230, 7, 0, 7, 229, 165, 91, 0, 91, 165, 26, 183, 194,
    183, 25, 0, 240, 139, 195, 194, 33, 0, 247, 87, 0, 92, 175, 0, 246,
    6, 0, 7, 232, 0, 244, 3, 0, 7, 225, 0, 248, 77, 0, 92, 160,
    0, 240, 160, 194, 178, 21, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 23, 178, 195, 160, 240, 0, 163, 91, 0, 80, 248, 0, 227, 7,
    0, 4, 245, 0, 233, 7, 0, 6, 246, 0, 175, 86, 0, 87, 247, 0,
    33, 194, 194, 136, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0,
    240, 0, 0, 240, 159, 110, 0, 247, 71, 0, 0, 244, 2, 0, 0, 240,
    0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 109, 207, 206, 70, 0,
    234, 13, 40, 162, 0, 160, 175, 69, 0, 0, 0, 62, 174, 159, 15, 193,
    5, 11, 233, 0, 117, 205, 207, 108, 0, 120, 0, 0, 240, 0, 129, 252,
    186, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 242, 2, 0, 181, 180,
    0, 240, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 240, 0, 0, 240,
    0, 0, 0, 242, 0, 0, 242, 0, 0, 16, 248, 0, 0, 227, 43, 0,
    107, 247, 0, 0, 81, 213, 191, 137, 240, 0, 208, 27, 0, 7, 218, 2,
    124, 105, 0, 72, 146, 0, 39, 184, 0, 149, 60, 0, 0, 203, 14, 198,
    1, 0, 0, 124, 134, 144, 0, 0, 0, 39, 249, 58, 0, 0, 217, 31,
    0, 185, 145, 0, 64, 167, 145, 93, 2, 187, 193, 0, 126, 94, 73, 155,
    46, 138, 185, 13, 188, 23, 9, 208, 104, 78, 129, 77, 195, 0, 0, 184,
    186, 19, 69, 191, 132, 0, 0, 112, 213, 0, 13, 252, 60, 0, 0, 174,
    80, 0, 154, 93, 0, 28, 207, 53, 183, 0, 0, 0, 114, 230, 33, 0,
    0, 0, 126, 226, 45, 0, 0, 38, 194, 46, 193, 1, 0, 190, 58, 0,
    150, 100, 207, 36, 0, 14, 219, 1, 118, 116, 0, 87, 137, 0, 29, 194,
    0, 166, 48, 0, 0, 192, 27, 198, 0, 0, 0, 105, 169, 125, 0, 0,
    0, 20, 250, 36, 0, 0, 0, 29, 188, 0, 0, 0, 78, 207, 49, 0,
    0, 0, 0, 150, 192, 196, 251, 0, 0, 0, 0, 124, 129, 0, 0, 0,
    45, 202, 5, 0, 0, 4, 200, 48, 0, 0, 0, 125, 128, 0, 0, 0,
    0, 250, 197, 192, 192, 9, 0, 145, 127, 0, 240, 4, 0, 240, 0, 0,
    240, 0, 75, 171, 0, 74, 176, 0, 0, 240, 0, 0, 240, 0, 0, 241,
    3, 0, 145, 126, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0,
    0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0,
    240, 0, 0, 240, 0, 127, 143, 0, 4, 240, 0, 0, 240, 0, 0, 240,
    0, 0, 170, 75, 0, 166, 74, 0, 240, 0, 0, 240, 0, 3, 240, 0,
    126, 143, 0, 0, 137, 190, 79, 45, 101, 0, 132, 14, 136, 189, 33, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

const DefaultFontGlyph* default_font_glyph(unsigned char ch)
{
    static constexpr DefaultFontGlyph glyphs[] = {
        {32, 2, 0, 0, 10, 2, 0, 10, 2, 10, DEFAULT_FONT_MASK_DATA + 0},
        {33, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 0},
        {34, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 24},
        {35, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 48},
        {36, 7, 10, 0, 1, 7, 0, 1, 7, 11, DEFAULT_FONT_MASK_DATA + 96},
        {37, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 166},
        {38, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 222},
        {39, 1, 8, 0, 2, 1, 0, 2, 1, 10, DEFAULT_FONT_MASK_DATA + 278},
        {40, 4, 10, 0, 1, 4, 0, 1, 4, 11, DEFAULT_FONT_MASK_DATA + 286},
        {41, 4, 10, -1, 1, 3, -1, 1, 3, 11, DEFAULT_FONT_MASK_DATA + 326},
        {42, 7, 5, 0, 5, 7, 0, 5, 7, 10, DEFAULT_FONT_MASK_DATA + 366},
        {43, 7, 6, 0, 4, 7, 0, 4, 7, 10, DEFAULT_FONT_MASK_DATA + 401},
        {44, 3, 3, -1, 8, 2, -1, 8, 2, 11, DEFAULT_FONT_MASK_DATA + 443},
        {45, 3, 4, 0, 6, 3, 0, 6, 3, 10, DEFAULT_FONT_MASK_DATA + 452},
        {46, 2, 2, 0, 8, 2, 0, 8, 2, 10, DEFAULT_FONT_MASK_DATA + 464},
        {47, 5, 9, -1, 2, 3, -1, 2, 4, 11, DEFAULT_FONT_MASK_DATA + 468},
        {48, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 513},
        {49, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 561},
        {50, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 609},
        {51, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 657},
        {52, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 705},
        {53, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 753},
        {54, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 801},
        {55, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 849},
        {56, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 897},
        {57, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 945},
        {58, 2, 6, 0, 4, 2, 0, 4, 2, 10, DEFAULT_FONT_MASK_DATA + 993},
        {59, 3, 7, -1, 4, 2, -1, 4, 2, 11, DEFAULT_FONT_MASK_DATA + 1005},
        {60, 6, 6, 0, 4, 6, 0, 4, 6, 10, DEFAULT_FONT_MASK_DATA + 1026},
        {61, 6, 5, 0, 5, 6, 0, 5, 6, 10, DEFAULT_FONT_MASK_DATA + 1062},
        {62, 6, 6, 0, 4, 6, 0, 4, 6, 10, DEFAULT_FONT_MASK_DATA + 1092},
        {63, 4, 8, 0, 2, 4, 0, 2, 4, 10, DEFAULT_FONT_MASK_DATA + 1128},
        {64, 10, 9, 0, 2, 10, 0, 2, 10, 11, DEFAULT_FONT_MASK_DATA + 1160},
        {65, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1250},
        {66, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 1306},
        {67, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1354},
        {68, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1410},
        {69, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 1466},
        {70, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 1514},
        {71, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1562},
        {72, 8, 8, 0, 2, 8, 0, 2, 8, 10, DEFAULT_FONT_MASK_DATA + 1618},
        {73, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 1682},
        {74, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 1706},
        {75, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1754},
        {76, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 1810},
        {77, 9, 8, 0, 2, 9, 0, 2, 9, 10, DEFAULT_FONT_MASK_DATA + 1858},
        {78, 8, 8, 0, 2, 8, 0, 2, 8, 10, DEFAULT_FONT_MASK_DATA + 1930},
        {79, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1994},
        {80, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 2050},
        {81, 7, 9, 0, 2, 7, 0, 2, 7, 11, DEFAULT_FONT_MASK_DATA + 2098},
        {82, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2161},
        {83, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 2217},
        {84, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2265},
        {85, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2321},
        {86, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2377},
        {87, 10, 8, 0, 2, 10, 0, 2, 10, 10, DEFAULT_FONT_MASK_DATA + 2433},
        {88, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2513},
        {89, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2569},
        {90, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2625},
        {91, 4, 10, 0, 1, 3, 0, 1, 4, 11, DEFAULT_FONT_MASK_DATA + 2681},
        {92, 5, 9, -1, 2, 3, -1, 2, 4, 11, DEFAULT_FONT_MASK_DATA + 2721},
        {93, 4, 10, -1, 1, 3, -1, 1, 3, 11, DEFAULT_FONT_MASK_DATA + 2766},
        {94, 6, 7, 0, 3, 6, 0, 3, 6, 10, DEFAULT_FONT_MASK_DATA + 2806},
        {95, 5, 1, 0, 10, 5, 0, 10, 5, 11, DEFAULT_FONT_MASK_DATA + 2848},
        {96, 3, 7, 0, 3, 3, 0, 3, 3, 10, DEFAULT_FONT_MASK_DATA + 2853},
        {97, 5, 6, 0, 4, 5, 0, 4, 5, 10, DEFAULT_FONT_MASK_DATA + 2874},
        {98, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 2904},
        {99, 5, 6, 0, 4, 5, 0, 4, 5, 10, DEFAULT_FONT_MASK_DATA + 2952},
        {100, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 2982},
        {101, 6, 6, 0, 4, 6, 0, 4, 6, 10, DEFAULT_FONT_MASK_DATA + 3030},
        {102, 4, 9, 0, 1, 3, 0, 1, 4, 10, DEFAULT_FONT_MASK_DATA + 3066},
        {103, 6, 8, 0, 4, 6, 0, 4, 6, 12, DEFAULT_FONT_MASK_DATA + 3102},
        {104, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 3150},
        {105, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 3206},
        {106, 3, 10, 0, 2, 3, 0, 2, 3, 12, DEFAULT_FONT_MASK_DATA + 3230},
        {107, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 3260},
        {108, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 3308},
        {109, 9, 6, 0, 4, 9, 0, 4, 9, 10, DEFAULT_FONT_MASK_DATA + 3332},
        {110, 7, 6, 0, 4, 7, 0, 4, 7, 10, DEFAULT_FONT_MASK_DATA + 3386},
        {111, 5, 6, 0, 4, 5, 0, 4, 5, 10, DEFAULT_FONT_MASK_DATA + 3428},
        {112, 6, 8, 0, 4, 6, 0, 4, 6, 12, DEFAULT_FONT_MASK_DATA + 3458},
        {113, 6, 8, 0, 4, 6, 0, 4, 6, 12, DEFAULT_FONT_MASK_DATA + 3506},
        {114, 4, 6, 0, 4, 4, 0, 4, 4, 10, DEFAULT_FONT_MASK_DATA + 3554},
        {115, 5, 6, -1, 4, 4, -1, 4, 4, 10, DEFAULT_FONT_MASK_DATA + 3578},
        {116, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 3608},
        {117, 7, 6, 0, 4, 7, 0, 4, 7, 10, DEFAULT_FONT_MASK_DATA + 3632},
        {118, 6, 6, 0, 4, 5, 0, 4, 6, 10, DEFAULT_FONT_MASK_DATA + 3674},
        {119, 8, 6, 0, 4, 8, 0, 4, 8, 10, DEFAULT_FONT_MASK_DATA + 3710},
        {120, 6, 6, -1, 4, 5, -1, 4, 5, 10, DEFAULT_FONT_MASK_DATA + 3758},
        {121, 6, 8, 0, 4, 5, 0, 4, 6, 12, DEFAULT_FONT_MASK_DATA + 3794},
        {122, 6, 6, 0, 4, 6, 0, 4, 6, 10, DEFAULT_FONT_MASK_DATA + 3842},
        {123, 3, 10, 0, 1, 3, 0, 1, 3, 11, DEFAULT_FONT_MASK_DATA + 3878},
        {124, 3, 11, 0, 1, 3, 0, 1, 3, 12, DEFAULT_FONT_MASK_DATA + 3908},
        {125, 3, 10, 0, 1, 3, 0, 1, 3, 11, DEFAULT_FONT_MASK_DATA + 3941},
        {126, 6, 4, 0, 6, 6, 0, 6, 6, 10, DEFAULT_FONT_MASK_DATA + 3971},
    };
    static_assert(sizeof(glyphs) / sizeof(glyphs[0]) == 95, "default font glyph table must cover printable ASCII");
    if (ch < 32u || ch > 126u) {
        return nullptr;
    }
    return &glyphs[ch - 32u];
}

int default_font_text_metrics_span(
    const char* text,
    std::size_t text_length,
    int* out_length,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!text || !out_length) {
        return PILLOW_C_NULL_POINTER;
    }
    int cursor = 0;
    bool has_bbox = false;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    for (std::size_t index = 0; index < text_length; ++index) {
        const auto ch = static_cast<unsigned char>(text[index]);
        if (ch < 32u || ch > 126u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const DefaultFontGlyph* glyph = default_font_glyph(ch);
        if (!glyph) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (!has_bbox) {
            left = cursor + glyph->bbox_left;
            top = glyph->bbox_top;
            right = cursor + glyph->bbox_right;
            bottom = glyph->bbox_bottom;
            has_bbox = true;
        } else {
            left = std::min(left, cursor + glyph->bbox_left);
            top = std::min(top, glyph->bbox_top);
            right = std::max(right, cursor + glyph->bbox_right);
            bottom = std::max(bottom, glyph->bbox_bottom);
        }
        cursor += glyph->advance;
    }

    *out_length = cursor;
    if (out_left) {
        *out_left = has_bbox ? left : 0;
    }
    if (out_top) {
        *out_top = has_bbox ? top : 0;
    }
    if (out_right) {
        *out_right = has_bbox ? right : 0;
    }
    if (out_bottom) {
        *out_bottom = has_bbox ? bottom : 0;
    }
    return PILLOW_C_OK;
}

int default_font_text_metrics(
    const char* text,
    int* out_length,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!text) {
        return PILLOW_C_NULL_POINTER;
    }
    return default_font_text_metrics_span(
        text,
        std::strlen(text),
        out_length,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int font_text_metrics(
    const PillowCFont* font,
    const char* text,
    int* out_length,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_text_metrics(text, out_length, out_left, out_top, out_right, out_bottom);
}

struct DefaultFontLineMetrics {
    std::size_t start;
    std::size_t length;
    int advance;
    int left;
    int top;
    int right;
    int bottom;
};

bool valid_text_align(int align)
{
    return align == PILLOW_C_TEXT_ALIGN_LEFT ||
        align == PILLOW_C_TEXT_ALIGN_CENTER ||
        align == PILLOW_C_TEXT_ALIGN_RIGHT ||
        align == PILLOW_C_TEXT_ALIGN_JUSTIFY;
}

int collect_default_font_multiline_metrics(
    const char* text,
    std::vector<DefaultFontLineMetrics>* out_lines,
    int* out_max_width)
{
    if (!text || !out_lines || !out_max_width) {
        return PILLOW_C_NULL_POINTER;
    }

    out_lines->clear();
    *out_max_width = 0;
    const std::size_t total_length = std::strlen(text);
    std::size_t line_start = 0;
    while (line_start <= total_length) {
        std::size_t line_end = line_start;
        while (line_end < total_length && text[line_end] != '\n') {
            ++line_end;
        }

        DefaultFontLineMetrics line{};
        line.start = line_start;
        line.length = line_end - line_start;
        const int status = default_font_text_metrics_span(
            text + line.start,
            line.length,
            &line.advance,
            &line.left,
            &line.top,
            &line.right,
            &line.bottom);
        if (status != PILLOW_C_OK) {
            return status;
        }
        *out_max_width = std::max(*out_max_width, line.advance);
        out_lines->push_back(line);

        if (line_end == total_length) {
            break;
        }
        line_start = line_end + 1;
    }

    return PILLOW_C_OK;
}

double default_font_multiline_align_offset(int max_width, int line_width, int align)
{
    const int width_difference = max_width - line_width;
    if (align == PILLOW_C_TEXT_ALIGN_CENTER) {
        return static_cast<double>(width_difference) / 2.0;
    }
    if (align == PILLOW_C_TEXT_ALIGN_RIGHT) {
        return static_cast<double>(width_difference);
    }
    return 0.0;
}

struct DefaultFontJustifyWordMetrics {
    std::size_t start;
    std::size_t length;
    int advance;
    int left;
    int top;
    int right;
    int bottom;
};

int collect_default_font_justify_words(
    const char* text,
    const DefaultFontLineMetrics& line,
    std::vector<DefaultFontJustifyWordMetrics>* out_words,
    int* out_word_width_sum)
{
    if (!text || !out_words || !out_word_width_sum) {
        return PILLOW_C_NULL_POINTER;
    }

    out_words->clear();
    *out_word_width_sum = 0;
    std::size_t word_offset = 0;
    while (word_offset <= line.length) {
        std::size_t word_end = word_offset;
        while (word_end < line.length && text[line.start + word_end] != ' ') {
            ++word_end;
        }

        DefaultFontJustifyWordMetrics word{};
        word.start = line.start + word_offset;
        word.length = word_end - word_offset;
        const int status = default_font_text_metrics_span(
            text + word.start,
            word.length,
            &word.advance,
            &word.left,
            &word.top,
            &word.right,
            &word.bottom);
        if (status != PILLOW_C_OK) {
            return status;
        }
        *out_word_width_sum += word.advance;
        out_words->push_back(word);

        if (word_end == line.length) {
            break;
        }
        word_offset = word_end + 1;
    }

    return PILLOW_C_OK;
}

bool default_font_should_justify_line(
    int align,
    const DefaultFontLineMetrics& line,
    int max_width,
    std::size_t line_index,
    std::size_t line_count)
{
    return align == PILLOW_C_TEXT_ALIGN_JUSTIFY &&
        max_width - line.advance != 0 &&
        line_index + 1u != line_count;
}

double default_font_justify_start_left(int left, int max_width, char horizontal)
{
    double line_left = static_cast<double>(left);
    if (horizontal == 'm') {
        line_left -= static_cast<double>(max_width) / 2.0;
    } else if (horizontal == 'r') {
        line_left -= static_cast<double>(max_width);
    }
    return line_left;
}

double default_font_justify_gap(int max_width, int word_width_sum, std::size_t word_count)
{
    return static_cast<double>(max_width - word_width_sum) /
        static_cast<double>(word_count - 1u);
}

int pillow_round_text_origin(double value)
{
    return static_cast<int>(std::floor(value + 0.5));
}

bool parse_default_font_anchor(const char* anchor, char* out_horizontal, char* out_vertical)
{
    if (!anchor || !out_horizontal || !out_vertical) {
        return false;
    }
    if (std::strlen(anchor) != 2u) {
        return false;
    }

    const char horizontal = anchor[0];
    const char vertical = anchor[1];
    const bool horizontal_ok = horizontal == 'l' || horizontal == 'm' || horizontal == 'r';
    const bool vertical_ok = vertical == 'a' ||
        vertical == 't' ||
        vertical == 'm' ||
        vertical == 'b' ||
        vertical == 'd' ||
        vertical == 's';
    if (!horizontal_ok || !vertical_ok) {
        return false;
    }
    *out_horizontal = horizontal;
    *out_vertical = vertical;
    return true;
}

bool parse_default_font_multiline_anchor(const char* anchor, char* out_horizontal, char* out_vertical)
{
    if (!parse_default_font_anchor(anchor, out_horizontal, out_vertical)) {
        return false;
    }
    return *out_vertical != 't' && *out_vertical != 'b';
}

int default_font_anchor_x_offset(int length, char horizontal)
{
    if (horizontal == 'm') {
        return -pillow_round_text_origin(static_cast<double>(length) / 2.0);
    }
    if (horizontal == 'r') {
        return -length;
    }
    return 0;
}

int default_font_anchor_y_offset(int bbox_top, int bbox_bottom, char vertical)
{
    if (vertical == 't') {
        return -bbox_top;
    }
    if (vertical == 'm') {
        return -((PILLOW_C_DEFAULT_FONT_ASCENT + PILLOW_C_DEFAULT_FONT_DESCENT) / 2);
    }
    if (vertical == 'b') {
        return -bbox_bottom;
    }
    if (vertical == 'd') {
        return -(PILLOW_C_DEFAULT_FONT_ASCENT + PILLOW_C_DEFAULT_FONT_DESCENT);
    }
    if (vertical == 's') {
        return -PILLOW_C_DEFAULT_FONT_ASCENT;
    }
    return 0;
}

int default_font_anchor_origin_offset(
    const char* text,
    const char* anchor,
    int* out_x_offset,
    int* out_y_offset)
{
    if (!text || !anchor || !out_x_offset || !out_y_offset) {
        return PILLOW_C_NULL_POINTER;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int length = 0;
    int bbox_left = 0;
    int bbox_top = 0;
    int bbox_right = 0;
    int bbox_bottom = 0;
    const int status =
        default_font_text_metrics(text, &length, &bbox_left, &bbox_top, &bbox_right, &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (length == 0 && text[0] == '\0') {
        *out_x_offset = 0;
        *out_y_offset = 0;
        return PILLOW_C_OK;
    }

    *out_x_offset = default_font_anchor_x_offset(length, horizontal);
    *out_y_offset = default_font_anchor_y_offset(bbox_top, bbox_bottom, vertical);
    return PILLOW_C_OK;
}

int default_font_textbbox_anchor(
    int left,
    int top,
    const char* text,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!text || !anchor || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int length = 0;
    int bbox_left = 0;
    int bbox_top = 0;
    int bbox_right = 0;
    int bbox_bottom = 0;
    const int status =
        default_font_text_metrics(text, &length, &bbox_left, &bbox_top, &bbox_right, &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (length == 0 && text[0] == '\0') {
        *out_left = left;
        *out_top = top;
        *out_right = left;
        *out_bottom = top;
        return PILLOW_C_OK;
    }

    const int x_offset = default_font_anchor_x_offset(length, horizontal);
    const int y_offset = default_font_anchor_y_offset(bbox_top, bbox_bottom, vertical);
    *out_left = left + x_offset + bbox_left;
    *out_top = top + y_offset + bbox_top;
    *out_right = left + x_offset + bbox_right;
    *out_bottom = top + y_offset + bbox_bottom;
    return PILLOW_C_OK;
}

void expand_i32_bbox(int stroke_width, int* left, int* top, int* right, int* bottom)
{
    *left -= stroke_width;
    *top -= stroke_width;
    *right += stroke_width;
    *bottom += stroke_width;
}

void expand_f64_bbox(int stroke_width, double* left, double* top, double* right, double* bottom)
{
    const double stroke = static_cast<double>(stroke_width);
    *left -= stroke;
    *top -= stroke;
    *right += stroke;
    *bottom += stroke;
}

int default_font_textbbox_stroke(
    int left,
    int top,
    const char* text,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int length = 0;
    int bbox_left = 0;
    int bbox_top = 0;
    int bbox_right = 0;
    int bbox_bottom = 0;
    const int status =
        default_font_text_metrics(text, &length, &bbox_left, &bbox_top, &bbox_right, &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_left = left + bbox_left;
    *out_top = top + bbox_top;
    *out_right = left + bbox_right;
    *out_bottom = top + bbox_bottom;
    expand_i32_bbox(stroke_width, out_left, out_top, out_right, out_bottom);
    return PILLOW_C_OK;
}

int default_font_textbbox_anchor_stroke(
    int left,
    int top,
    const char* text,
    const char* anchor,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int status =
        default_font_textbbox_anchor(left, top, text, anchor, out_left, out_top, out_right, out_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    expand_i32_bbox(stroke_width, out_left, out_top, out_right, out_bottom);
    return PILLOW_C_OK;
}

void default_font_line_textbbox_anchor(
    double left,
    double top,
    const DefaultFontLineMetrics& line,
    char horizontal,
    char vertical,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (line.advance == 0 && line.length == 0) {
        *out_left = left;
        *out_top = top;
        *out_right = left;
        *out_bottom = top;
        return;
    }

    const int x_offset = default_font_anchor_x_offset(line.advance, horizontal);
    const int y_offset = default_font_anchor_y_offset(line.top, line.bottom, vertical);
    *out_left = left + static_cast<double>(x_offset + line.left);
    *out_top = top + static_cast<double>(y_offset + line.top);
    *out_right = left + static_cast<double>(x_offset + line.right);
    *out_bottom = top + static_cast<double>(y_offset + line.bottom);
}

void default_font_justify_word_textbbox_anchor(
    double left,
    double top,
    const DefaultFontJustifyWordMetrics& word,
    char horizontal,
    char vertical,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (word.advance == 0 && word.length == 0) {
        *out_left = left;
        *out_top = top;
        *out_right = left;
        *out_bottom = top;
        return;
    }

    const int x_offset = default_font_anchor_x_offset(word.advance, horizontal);
    const int y_offset = default_font_anchor_y_offset(word.top, word.bottom, vertical);
    *out_left = left + static_cast<double>(x_offset + word.left);
    *out_top = top + static_cast<double>(y_offset + word.top);
    *out_right = left + static_cast<double>(x_offset + word.right);
    *out_bottom = top + static_cast<double>(y_offset + word.bottom);
}

double default_font_multiline_anchor_top(
    int top,
    std::size_t line_count,
    int line_spacing,
    char vertical)
{
    const double line_span =
        static_cast<double>(line_count == 0 ? 0 : line_count - 1) * static_cast<double>(line_spacing);
    if (vertical == 'm') {
        return static_cast<double>(top) - line_span / 2.0;
    }
    if (vertical == 'd') {
        return static_cast<double>(top) - line_span;
    }
    return static_cast<double>(top);
}

double default_font_multiline_anchor_line_left(
    int left,
    int max_width,
    int line_width,
    int align,
    char horizontal)
{
    const int width_difference = max_width - line_width;
    double line_left = static_cast<double>(left) +
        default_font_multiline_align_offset(max_width, line_width, align);
    if (horizontal == 'm') {
        line_left -= static_cast<double>(width_difference) / 2.0;
    } else if (horizontal == 'r') {
        line_left -= static_cast<double>(width_difference);
    }
    return line_left;
}

int default_font_multiline_textbbox_anchor(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    const char* anchor,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!text || !anchor || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_multiline_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    constexpr int default_line_height = 10;
    const int line_spacing = default_line_height + spacing;
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    const double first_line_top =
        default_font_multiline_anchor_top(top, lines.size(), line_spacing, vertical);
    bool has_bbox = false;
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = default_font_justify_start_left(left, max_width, horizontal);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top = first_line_top +
                    static_cast<double>(line_index) * static_cast<double>(line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    double word_bbox_left = 0.0;
                    double word_bbox_top = 0.0;
                    double word_bbox_right = 0.0;
                    double word_bbox_bottom = 0.0;
                    default_font_justify_word_textbbox_anchor(
                        word_left,
                        line_top,
                        word,
                        'l',
                        vertical,
                        &word_bbox_left,
                        &word_bbox_top,
                        &word_bbox_right,
                        &word_bbox_bottom);
                    if (!has_bbox) {
                        bbox_left = word_bbox_left;
                        bbox_top = word_bbox_top;
                        bbox_right = word_bbox_right;
                        bbox_bottom = word_bbox_bottom;
                        has_bbox = true;
                    } else {
                        bbox_left = std::min(bbox_left, word_bbox_left);
                        bbox_top = std::min(bbox_top, word_bbox_top);
                        bbox_right = std::max(bbox_right, word_bbox_right);
                        bbox_bottom = std::max(bbox_bottom, word_bbox_bottom);
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_left = default_font_multiline_anchor_line_left(
            left,
            max_width,
            line.advance,
            align,
            horizontal);
        const double line_top = first_line_top +
            static_cast<double>(line_index) * static_cast<double>(line_spacing);
        double line_bbox_left = 0.0;
        double line_bbox_top = 0.0;
        double line_bbox_right = 0.0;
        double line_bbox_bottom = 0.0;
        default_font_line_textbbox_anchor(
            line_left,
            line_top,
            line,
            horizontal,
            vertical,
            &line_bbox_left,
            &line_bbox_top,
            &line_bbox_right,
            &line_bbox_bottom);
        if (!has_bbox) {
            bbox_left = line_bbox_left;
            bbox_top = line_bbox_top;
            bbox_right = line_bbox_right;
            bbox_bottom = line_bbox_bottom;
            has_bbox = true;
        } else {
            bbox_left = std::min(bbox_left, line_bbox_left);
            bbox_top = std::min(bbox_top, line_bbox_top);
            bbox_right = std::max(bbox_right, line_bbox_right);
            bbox_bottom = std::max(bbox_bottom, line_bbox_bottom);
        }
    }

    *out_left = has_bbox ? bbox_left : static_cast<double>(left);
    *out_top = has_bbox ? bbox_top : static_cast<double>(top);
    *out_right = has_bbox ? bbox_right : static_cast<double>(left);
    *out_bottom = has_bbox ? bbox_bottom : static_cast<double>(top);
    return PILLOW_C_OK;
}

int default_font_multiline_line_spacing(int spacing, int stroke_width)
{
    constexpr int default_line_height = 10;
    return default_line_height + spacing + stroke_width * 2;
}

std::uint8_t blend_text_channel(std::uint8_t dst, std::uint8_t src, std::uint8_t alpha)
{
    if (alpha == 255) {
        return src;
    }
    const std::uint32_t blended =
        static_cast<std::uint32_t>(dst) * (255u - alpha) +
        static_cast<std::uint32_t>(src) * alpha +
        128u;
    return static_cast<std::uint8_t>(shift_for_div255(blended));
}

int draw_text_image_span(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    std::size_t text_length,
    const std::uint8_t* fill,
    std::size_t fill_size);

bool colors_equal(const std::uint8_t* left, std::size_t left_size, const std::uint8_t* right, std::size_t right_size)
{
    return left_size == right_size &&
        (left_size == 0 || std::memcmp(left, right, left_size) == 0);
}

int blend_text_mask_region(
    PillowCImage* image,
    int mask_left,
    int mask_top,
    int mask_width,
    int mask_height,
    const std::vector<std::uint8_t>& mask,
    const std::uint8_t* color,
    std::size_t color_size)
{
    if (!image || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (image->pixels.empty() || mask.empty() || mask_width <= 0 || mask_height <= 0) {
        return PILLOW_C_OK;
    }

    const int dst_left = std::max(mask_left, 0);
    const int dst_top = std::max(mask_top, 0);
    const int dst_right = std::min(mask_left + mask_width, image->width);
    const int dst_bottom = std::min(mask_top + mask_height, image->height);
    if (dst_right <= dst_left || dst_bottom <= dst_top) {
        return PILLOW_C_OK;
    }

    const int channels = image->channels;
    for (int y = dst_top; y < dst_bottom; ++y) {
        const int mask_y = y - mask_top;
        const std::uint8_t* mask_row =
            mask.data() + static_cast<std::size_t>(mask_y) * static_cast<std::size_t>(mask_width);
        std::uint8_t* dst_row =
            image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = dst_left; x < dst_right; ++x) {
            const std::uint8_t alpha = mask_row[x - mask_left];
            if (alpha == 0) {
                continue;
            }
            std::uint8_t* dst =
                dst_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels);
            for (int channel = 0; channel < channels; ++channel) {
                dst[channel] = blend_text_channel(dst[channel], color[channel], alpha);
            }
        }
    }
    return PILLOW_C_OK;
}

int draw_text_stroke_mask_image_span(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    std::size_t text_length,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size,
    int stroke_width)
{
    if (!image || !text || !stroke_fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (stroke_fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (stroke_width == 0) {
        return draw_text_image_span(image, left, top, text, text_length, stroke_fill, stroke_fill_size);
    }

    int text_pixel_length = 0;
    int bbox_left = 0;
    int bbox_top = 0;
    int bbox_right = 0;
    int bbox_bottom = 0;
    const int metrics_status = default_font_text_metrics_span(
        text,
        text_length,
        &text_pixel_length,
        &bbox_left,
        &bbox_top,
        &bbox_right,
        &bbox_bottom);
    if (metrics_status != PILLOW_C_OK) {
        return metrics_status;
    }
    if (image->pixels.empty() || text_pixel_length == 0) {
        return PILLOW_C_OK;
    }

    const int mask_left = left + bbox_left - stroke_width;
    const int mask_top = top + bbox_top - stroke_width;
    const int mask_right = left + bbox_right + stroke_width;
    const int mask_bottom = top + bbox_bottom + stroke_width;
    const int mask_width = mask_right - mask_left;
    const int mask_height = mask_bottom - mask_top;
    if (mask_width <= 0 || mask_height <= 0) {
        return PILLOW_C_OK;
    }

    const std::uint64_t mask_area =
        static_cast<std::uint64_t>(mask_width) * static_cast<std::uint64_t>(mask_height);
    if (mask_area > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    try {
        std::vector<std::uint8_t> mask(static_cast<std::size_t>(mask_area), 0);
        int cursor = 0;
        for (std::size_t index = 0; index < text_length; ++index) {
            const DefaultFontGlyph* glyph = default_font_glyph(static_cast<unsigned char>(text[index]));
            if (!glyph) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            for (int glyph_y = 0; glyph_y < glyph->height; ++glyph_y) {
                const std::uint8_t* glyph_row =
                    glyph->mask + static_cast<std::size_t>(glyph_y) * static_cast<std::size_t>(glyph->width);
                for (int glyph_x = 0; glyph_x < glyph->width; ++glyph_x) {
                    const std::uint8_t alpha = glyph_row[glyph_x];
                    if (alpha == 0) {
                        continue;
                    }
                    const int base_x = left + cursor + glyph->offset_x + glyph_x;
                    const int base_y = top + glyph->offset_y + glyph_y;
                    for (int offset_y = -stroke_width; offset_y <= stroke_width; ++offset_y) {
                        const int mask_y = base_y + offset_y - mask_top;
                        if (mask_y < 0 || mask_y >= mask_height) {
                            continue;
                        }
                        std::uint8_t* mask_row =
                            mask.data() + static_cast<std::size_t>(mask_y) * static_cast<std::size_t>(mask_width);
                        for (int offset_x = -stroke_width; offset_x <= stroke_width; ++offset_x) {
                            const int mask_x = base_x + offset_x - mask_left;
                            if (mask_x < 0 || mask_x >= mask_width) {
                                continue;
                            }
                            std::uint8_t& current = mask_row[mask_x];
                            current = std::max(current, alpha);
                        }
                    }
                }
            }
            cursor += glyph->advance;
        }
        return blend_text_mask_region(
            image,
            mask_left,
            mask_top,
            mask_width,
            mask_height,
            mask,
            stroke_fill,
            stroke_fill_size);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int draw_text_image_span(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    std::size_t text_length,
    const std::uint8_t* fill,
    std::size_t fill_size)
{
    if (!image || !text || !fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    int text_pixel_length = 0;
    const int metrics_status =
        default_font_text_metrics_span(text, text_length, &text_pixel_length, nullptr, nullptr, nullptr, nullptr);
    if (metrics_status != PILLOW_C_OK) {
        return metrics_status;
    }
    if (image->pixels.empty() || text_pixel_length == 0) {
        return PILLOW_C_OK;
    }

    int cursor = 0;
    for (std::size_t index = 0; index < text_length; ++index) {
        const DefaultFontGlyph* glyph = default_font_glyph(static_cast<unsigned char>(text[index]));
        if (!glyph) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        for (int glyph_y = 0; glyph_y < glyph->height; ++glyph_y) {
            const int dst_y = top + glyph->offset_y + glyph_y;
            if (dst_y < 0 || dst_y >= image->height) {
                continue;
            }
            const std::uint8_t* mask_row =
                glyph->mask + static_cast<std::size_t>(glyph_y) * static_cast<std::size_t>(glyph->width);
            std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(dst_y) * image->stride;
            for (int glyph_x = 0; glyph_x < glyph->width; ++glyph_x) {
                const std::uint8_t alpha = mask_row[glyph_x];
                if (alpha == 0) {
                    continue;
                }
                const int dst_x = left + cursor + glyph->offset_x + glyph_x;
                if (dst_x < 0 || dst_x >= image->width) {
                    continue;
                }
                std::uint8_t* dst =
                    dst_row + static_cast<std::size_t>(dst_x) * static_cast<std::size_t>(image->channels);
                for (int channel = 0; channel < image->channels; ++channel) {
                    dst[channel] = blend_text_channel(dst[channel], fill[channel], alpha);
                }
            }
        }
        cursor += glyph->advance;
    }
    return PILLOW_C_OK;
}

int draw_text_image(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size)
{
    if (!text) {
        return PILLOW_C_NULL_POINTER;
    }
    return draw_text_image_span(image, left, top, text, std::strlen(text), fill, fill_size);
}

int draw_text_image_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (!text) {
        return PILLOW_C_NULL_POINTER;
    }
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (stroke_width == 0) {
        return draw_text_image(image, left, top, text, fill, fill_size);
    }
    const std::size_t text_length = std::strlen(text);
    const int stroke_status = draw_text_stroke_mask_image_span(
        image,
        left,
        top,
        text,
        text_length,
        stroke_fill,
        stroke_fill_size,
        stroke_width);
    if (stroke_status != PILLOW_C_OK) {
        return stroke_status;
    }
    if (colors_equal(fill, fill_size, stroke_fill, stroke_fill_size)) {
        return PILLOW_C_OK;
    }
    return draw_text_image_span(image, left, top, text, text_length, fill, fill_size);
}

int draw_text_image_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor)
{
    int x_offset = 0;
    int y_offset = 0;
    const int anchor_status = default_font_anchor_origin_offset(text, anchor, &x_offset, &y_offset);
    if (anchor_status != PILLOW_C_OK) {
        return anchor_status;
    }
    return draw_text_image(image, left + x_offset, top + y_offset, text, fill, fill_size);
}

int draw_text_image_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    int x_offset = 0;
    int y_offset = 0;
    const int anchor_status = default_font_anchor_origin_offset(text, anchor, &x_offset, &y_offset);
    if (anchor_status != PILLOW_C_OK) {
        return anchor_status;
    }
    return draw_text_image_stroke(
        image,
        left + x_offset,
        top + y_offset,
        text,
        fill,
        fill_size,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

int default_font_multiline_textbbox_align(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!text || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    constexpr int default_line_height = 10;
    const int line_spacing = default_line_height + spacing;
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    bool has_bbox = false;
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = static_cast<double>(left);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top =
                    static_cast<double>(top + static_cast<int>(line_index) * line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    double absolute_left = 0.0;
                    double absolute_top = 0.0;
                    double absolute_right = 0.0;
                    double absolute_bottom = 0.0;
                    default_font_justify_word_textbbox_anchor(
                        word_left,
                        line_top,
                        word,
                        'l',
                        'a',
                        &absolute_left,
                        &absolute_top,
                        &absolute_right,
                        &absolute_bottom);
                    if (!has_bbox) {
                        bbox_left = absolute_left;
                        bbox_top = absolute_top;
                        bbox_right = absolute_right;
                        bbox_bottom = absolute_bottom;
                        has_bbox = true;
                    } else {
                        bbox_left = std::min(bbox_left, absolute_left);
                        bbox_top = std::min(bbox_top, absolute_top);
                        bbox_right = std::max(bbox_right, absolute_right);
                        bbox_bottom = std::max(bbox_bottom, absolute_bottom);
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_x = static_cast<double>(left) +
            default_font_multiline_align_offset(max_width, line.advance, align);
        const int line_y = static_cast<int>(line_index) * line_spacing;
        const double absolute_left = line_x + line.left;
        const double absolute_top = static_cast<double>(top + line_y + line.top);
        const double absolute_right = line_x + line.right;
        const double absolute_bottom = static_cast<double>(top + line_y + line.bottom);
        if (!has_bbox) {
            bbox_left = absolute_left;
            bbox_top = absolute_top;
            bbox_right = absolute_right;
            bbox_bottom = absolute_bottom;
            has_bbox = true;
        } else {
            bbox_left = std::min(bbox_left, absolute_left);
            bbox_top = std::min(bbox_top, absolute_top);
            bbox_right = std::max(bbox_right, absolute_right);
            bbox_bottom = std::max(bbox_bottom, absolute_bottom);
        }
    }

    *out_left = has_bbox ? bbox_left : static_cast<double>(left);
    *out_top = has_bbox ? bbox_top : static_cast<double>(top);
    *out_right = has_bbox ? bbox_right : static_cast<double>(left);
    *out_bottom = has_bbox ? bbox_bottom : static_cast<double>(top);
    return PILLOW_C_OK;
}

int default_font_multiline_textbbox_align_stroke(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!text || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!valid_text_align(align) || stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int line_spacing = default_font_multiline_line_spacing(spacing, stroke_width);
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    bool has_bbox = false;
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = static_cast<double>(left);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top =
                    static_cast<double>(top + static_cast<int>(line_index) * line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    double absolute_left = 0.0;
                    double absolute_top = 0.0;
                    double absolute_right = 0.0;
                    double absolute_bottom = 0.0;
                    default_font_justify_word_textbbox_anchor(
                        word_left,
                        line_top,
                        word,
                        'l',
                        'a',
                        &absolute_left,
                        &absolute_top,
                        &absolute_right,
                        &absolute_bottom);
                    expand_f64_bbox(stroke_width, &absolute_left, &absolute_top, &absolute_right, &absolute_bottom);
                    if (!has_bbox) {
                        bbox_left = absolute_left;
                        bbox_top = absolute_top;
                        bbox_right = absolute_right;
                        bbox_bottom = absolute_bottom;
                        has_bbox = true;
                    } else {
                        bbox_left = std::min(bbox_left, absolute_left);
                        bbox_top = std::min(bbox_top, absolute_top);
                        bbox_right = std::max(bbox_right, absolute_right);
                        bbox_bottom = std::max(bbox_bottom, absolute_bottom);
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_x = static_cast<double>(left) +
            default_font_multiline_align_offset(max_width, line.advance, align);
        const int line_y = static_cast<int>(line_index) * line_spacing;
        double absolute_left = line_x + static_cast<double>(line.left);
        double absolute_top = static_cast<double>(top + line_y + line.top);
        double absolute_right = line_x + static_cast<double>(line.right);
        double absolute_bottom = static_cast<double>(top + line_y + line.bottom);
        expand_f64_bbox(stroke_width, &absolute_left, &absolute_top, &absolute_right, &absolute_bottom);
        if (!has_bbox) {
            bbox_left = absolute_left;
            bbox_top = absolute_top;
            bbox_right = absolute_right;
            bbox_bottom = absolute_bottom;
            has_bbox = true;
        } else {
            bbox_left = std::min(bbox_left, absolute_left);
            bbox_top = std::min(bbox_top, absolute_top);
            bbox_right = std::max(bbox_right, absolute_right);
            bbox_bottom = std::max(bbox_bottom, absolute_bottom);
        }
    }

    *out_left = has_bbox ? bbox_left : static_cast<double>(left);
    *out_top = has_bbox ? bbox_top : static_cast<double>(top);
    *out_right = has_bbox ? bbox_right : static_cast<double>(left);
    *out_bottom = has_bbox ? bbox_bottom : static_cast<double>(top);
    return PILLOW_C_OK;
}

int default_font_multiline_textbbox_align_stroke_i32(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;
    const int status = default_font_multiline_textbbox_align_stroke(
        left,
        top,
        text,
        spacing,
        align,
        stroke_width,
        &bbox_left,
        &bbox_top,
        &bbox_right,
        &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_left = static_cast<int>(std::floor(bbox_left));
    *out_top = static_cast<int>(std::floor(bbox_top));
    *out_right = static_cast<int>(std::ceil(bbox_right));
    *out_bottom = static_cast<int>(std::ceil(bbox_bottom));
    return PILLOW_C_OK;
}

int default_font_multiline_textbbox_anchor_stroke(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!text || !anchor || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!valid_text_align(align) || stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_multiline_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int line_spacing = default_font_multiline_line_spacing(spacing, stroke_width);
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    const double first_line_top =
        default_font_multiline_anchor_top(top, lines.size(), line_spacing, vertical);
    bool has_bbox = false;
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = default_font_justify_start_left(left, max_width, horizontal);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top = first_line_top +
                    static_cast<double>(line_index) * static_cast<double>(line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    double line_bbox_left = 0.0;
                    double line_bbox_top = 0.0;
                    double line_bbox_right = 0.0;
                    double line_bbox_bottom = 0.0;
                    default_font_justify_word_textbbox_anchor(
                        word_left,
                        line_top,
                        word,
                        'l',
                        vertical,
                        &line_bbox_left,
                        &line_bbox_top,
                        &line_bbox_right,
                        &line_bbox_bottom);
                    expand_f64_bbox(stroke_width, &line_bbox_left, &line_bbox_top, &line_bbox_right, &line_bbox_bottom);
                    if (!has_bbox) {
                        bbox_left = line_bbox_left;
                        bbox_top = line_bbox_top;
                        bbox_right = line_bbox_right;
                        bbox_bottom = line_bbox_bottom;
                        has_bbox = true;
                    } else {
                        bbox_left = std::min(bbox_left, line_bbox_left);
                        bbox_top = std::min(bbox_top, line_bbox_top);
                        bbox_right = std::max(bbox_right, line_bbox_right);
                        bbox_bottom = std::max(bbox_bottom, line_bbox_bottom);
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_left = default_font_multiline_anchor_line_left(
            left,
            max_width,
            line.advance,
            align,
            horizontal);
        const double line_top = first_line_top +
            static_cast<double>(line_index) * static_cast<double>(line_spacing);
        int x_offset = 0;
        int y_offset = 0;
        if (!(line.advance == 0 && line.length == 0)) {
            x_offset = default_font_anchor_x_offset(line.advance, horizontal);
            y_offset = default_font_anchor_y_offset(line.top, line.bottom, vertical);
        }

        double line_bbox_left = line_left + static_cast<double>(x_offset + line.left);
        double line_bbox_top = line_top + static_cast<double>(y_offset + line.top);
        double line_bbox_right = line_left + static_cast<double>(x_offset + line.right);
        double line_bbox_bottom = line_top + static_cast<double>(y_offset + line.bottom);
        expand_f64_bbox(stroke_width, &line_bbox_left, &line_bbox_top, &line_bbox_right, &line_bbox_bottom);
        if (!has_bbox) {
            bbox_left = line_bbox_left;
            bbox_top = line_bbox_top;
            bbox_right = line_bbox_right;
            bbox_bottom = line_bbox_bottom;
            has_bbox = true;
        } else {
            bbox_left = std::min(bbox_left, line_bbox_left);
            bbox_top = std::min(bbox_top, line_bbox_top);
            bbox_right = std::max(bbox_right, line_bbox_right);
            bbox_bottom = std::max(bbox_bottom, line_bbox_bottom);
        }
    }

    *out_left = has_bbox ? bbox_left : static_cast<double>(left);
    *out_top = has_bbox ? bbox_top : static_cast<double>(top);
    *out_right = has_bbox ? bbox_right : static_cast<double>(left);
    *out_bottom = has_bbox ? bbox_bottom : static_cast<double>(top);
    return PILLOW_C_OK;
}

int default_font_multiline_textbbox_align_i32(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;
    const int status = default_font_multiline_textbbox_align(
        left,
        top,
        text,
        spacing,
        align,
        &bbox_left,
        &bbox_top,
        &bbox_right,
        &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_left = static_cast<int>(std::floor(bbox_left));
    *out_top = static_cast<int>(std::floor(bbox_top));
    *out_right = static_cast<int>(std::ceil(bbox_right));
    *out_bottom = static_cast<int>(std::ceil(bbox_bottom));
    return PILLOW_C_OK;
}

int default_font_multiline_textbbox(
    int left,
    int top,
    const char* text,
    int spacing,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_align_i32(
        left,
        top,
        text,
        spacing,
        PILLOW_C_TEXT_ALIGN_LEFT,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int draw_multiline_text_image_align(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align)
{
    if (!image || !text || !fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    constexpr int default_line_height = 10;
    const int line_spacing = default_line_height + spacing;
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = static_cast<double>(left);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const int line_top = top + static_cast<int>(line_index) * line_spacing;
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    const int status = draw_text_image_span(
                        image,
                        pillow_round_text_origin(word_left),
                        line_top,
                        text + word.start,
                        word.length,
                        fill,
                        fill_size);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const int line_left = left + pillow_round_text_origin(
            default_font_multiline_align_offset(max_width, line.advance, align));
        const int status = draw_text_image_span(
            image,
            line_left,
            top + static_cast<int>(line_index) * line_spacing,
            text + line.start,
            line.length,
            fill,
            fill_size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }
    return PILLOW_C_OK;
}

int draw_multiline_text_image_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor)
{
    if (!image || !text || !fill || !anchor) {
        return PILLOW_C_NULL_POINTER;
    }
    if (fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_multiline_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    constexpr int default_line_height = 10;
    const int line_spacing = default_line_height + spacing;
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    const double first_line_top =
        default_font_multiline_anchor_top(top, lines.size(), line_spacing, vertical);
    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = default_font_justify_start_left(left, max_width, horizontal);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top = first_line_top +
                    static_cast<double>(line_index) * static_cast<double>(line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    int y_offset = 0;
                    if (!(word.advance == 0 && word.length == 0)) {
                        y_offset = default_font_anchor_y_offset(word.top, word.bottom, vertical);
                    }
                    const int status = draw_text_image_span(
                        image,
                        pillow_round_text_origin(word_left),
                        pillow_round_text_origin(line_top + static_cast<double>(y_offset)),
                        text + word.start,
                        word.length,
                        fill,
                        fill_size);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_left = default_font_multiline_anchor_line_left(
            left,
            max_width,
            line.advance,
            align,
            horizontal);
        const double line_top = first_line_top +
            static_cast<double>(line_index) * static_cast<double>(line_spacing);
        int x_offset = 0;
        int y_offset = 0;
        if (!(line.advance == 0 && line.length == 0)) {
            x_offset = default_font_anchor_x_offset(line.advance, horizontal);
            y_offset = default_font_anchor_y_offset(line.top, line.bottom, vertical);
        }

        const int status = draw_text_image_span(
            image,
            pillow_round_text_origin(line_left + static_cast<double>(x_offset)),
            pillow_round_text_origin(line_top + static_cast<double>(y_offset)),
            text + line.start,
            line.length,
            fill,
            fill_size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }
    return PILLOW_C_OK;
}

int draw_multiline_text_line_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    std::size_t text_length,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    const int stroke_status = draw_text_stroke_mask_image_span(
        image,
        left,
        top,
        text,
        text_length,
        stroke_fill,
        stroke_fill_size,
        stroke_width);
    if (stroke_status != PILLOW_C_OK) {
        return stroke_status;
    }
    if (colors_equal(fill, fill_size, stroke_fill, stroke_fill_size)) {
        return PILLOW_C_OK;
    }
    return draw_text_image_span(image, left, top, text, text_length, fill, fill_size);
}

int draw_multiline_text_image_align_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (stroke_width == 0) {
        return draw_multiline_text_image_align(image, left, top, text, fill, fill_size, spacing, align);
    }
    if (!image || !text || !fill || !stroke_fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (fill_size != static_cast<std::size_t>(image->channels) ||
        stroke_fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int line_spacing = default_font_multiline_line_spacing(spacing, stroke_width);
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = static_cast<double>(left);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const int line_top = top + static_cast<int>(line_index) * line_spacing;
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    const int status = draw_multiline_text_line_stroke(
                        image,
                        pillow_round_text_origin(word_left),
                        line_top,
                        text + word.start,
                        word.length,
                        fill,
                        fill_size,
                        stroke_width,
                        stroke_fill,
                        stroke_fill_size);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const int line_left = left + pillow_round_text_origin(
            default_font_multiline_align_offset(max_width, line.advance, align));
        const int status = draw_multiline_text_line_stroke(
            image,
            line_left,
            top + static_cast<int>(line_index) * line_spacing,
            text + line.start,
            line.length,
            fill,
            fill_size,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }
    return PILLOW_C_OK;
}

int draw_multiline_text_image_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (stroke_width == 0) {
        return draw_multiline_text_image_anchor(image, left, top, text, fill, fill_size, spacing, align, anchor);
    }
    if (!image || !text || !fill || !anchor || !stroke_fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (fill_size != static_cast<std::size_t>(image->channels) ||
        stroke_fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_multiline_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int line_spacing = default_font_multiline_line_spacing(spacing, stroke_width);
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    const double first_line_top =
        default_font_multiline_anchor_top(top, lines.size(), line_spacing, vertical);
    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = default_font_justify_start_left(left, max_width, horizontal);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top = first_line_top +
                    static_cast<double>(line_index) * static_cast<double>(line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    int y_offset = 0;
                    if (!(word.advance == 0 && word.length == 0)) {
                        y_offset = default_font_anchor_y_offset(word.top, word.bottom, vertical);
                    }
                    const int status = draw_multiline_text_line_stroke(
                        image,
                        pillow_round_text_origin(word_left),
                        pillow_round_text_origin(line_top + static_cast<double>(y_offset)),
                        text + word.start,
                        word.length,
                        fill,
                        fill_size,
                        stroke_width,
                        stroke_fill,
                        stroke_fill_size);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_left = default_font_multiline_anchor_line_left(
            left,
            max_width,
            line.advance,
            align,
            horizontal);
        const double line_top = first_line_top +
            static_cast<double>(line_index) * static_cast<double>(line_spacing);
        int x_offset = 0;
        int y_offset = 0;
        if (!(line.advance == 0 && line.length == 0)) {
            x_offset = default_font_anchor_x_offset(line.advance, horizontal);
            y_offset = default_font_anchor_y_offset(line.top, line.bottom, vertical);
        }

        const int status = draw_multiline_text_line_stroke(
            image,
            pillow_round_text_origin(line_left + static_cast<double>(x_offset)),
            pillow_round_text_origin(line_top + static_cast<double>(y_offset)),
            text + line.start,
            line.length,
            fill,
            fill_size,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }
    return PILLOW_C_OK;
}

int draw_multiline_text_image(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing)
{
    return draw_multiline_text_image_align(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        PILLOW_C_TEXT_ALIGN_LEFT);
}

int draw_multiline_text_image_font_align(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_multiline_text_image_align(image, left, top, text, fill, fill_size, spacing, align);
}

int draw_multiline_text_image_font_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_multiline_text_image_anchor(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        align,
        anchor);
}

int draw_multiline_text_image_font_align_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_multiline_text_image_align_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        align,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

int draw_multiline_text_image_font_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_multiline_text_image_anchor_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        align,
        anchor,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

int draw_multiline_text_image_font(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing)
{
    return draw_multiline_text_image_font_align(
        image,
        left,
        top,
        text,
        font,
        fill,
        fill_size,
        spacing,
        PILLOW_C_TEXT_ALIGN_LEFT);
}

int default_font_multiline_textbbox_font_align(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_align_i32(
        left,
        top,
        text,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font_align_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_align(
        left,
        top,
        text,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font_anchor_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    const char* anchor,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_anchor(
        left,
        top,
        text,
        spacing,
        align,
        anchor,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font_align_stroke(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_align_stroke_i32(
        left,
        top,
        text,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font_align_stroke_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_align_stroke(
        left,
        top,
        text,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font_anchor_stroke_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_anchor_stroke(
        left,
        top,
        text,
        spacing,
        align,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_font_align(
        left,
        top,
        text,
        font,
        spacing,
        PILLOW_C_TEXT_ALIGN_LEFT,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int draw_text_image_font(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_text_image(image, left, top, text, fill, fill_size);
}

int draw_text_image_font_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_text_image_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

int draw_text_image_font_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_text_image_anchor(image, left, top, text, fill, fill_size, anchor);
}

int draw_text_image_font_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_text_image_anchor_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        anchor,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

int default_font_textbbox_font_anchor(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_textbbox_anchor(left, top, text, anchor, out_left, out_top, out_right, out_bottom);
}

int default_font_textbbox_font_anchor_stroke(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const char* anchor,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_textbbox_anchor_stroke(
        left,
        top,
        text,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
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
        copy_palette_if_point_preserves_core_palette(source, target);
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
    if (!image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        copy_palette_if_same_mode(source, target);
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
    copy_palette_if_point_preserves_core_palette(source, target);
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
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
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
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
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
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
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

int quantize_exact_image_into(const PillowCImage* source, int colors, PillowCImage* target)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (colors < 1 || colors > 256) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!((source->mode == PILLOW_C_MODE_RGB && source->channels == 3) ||
          (source->mode == PILLOW_C_MODE_L && source->channels == 1))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!image_shape_matches(target, source->width, source->height, PILLOW_C_MODE_P, 1)) {
        return PILLOW_C_MISMATCH;
    }
    if (source->pixels.empty()) {
        target->palette_rgb.clear();
        target->palette_alpha.clear();
        target->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_OK;
    }
    if (source->mode == PILLOW_C_MODE_L) {
        return quantize_exact_l_into(source, colors, target);
    }
    return quantize_exact_rgb_into(source, colors, target);
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
    if (!image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
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
    if (source->pixels.empty()) {
        if (source->mode == target_mode) {
            target->palette_rgb = source->palette_rgb;
            target->palette_alpha = source->palette_alpha;
            target->palette_alpha_mode = source->palette_alpha_mode;
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
    if (!image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
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
    if (!image_shape_matches(target, source->width, source->height, target_mode, target_channels)) {
        return PILLOW_C_MISMATCH;
    }

    std::vector<std::int16_t> prepared_table;
    const int prepare_status = prepare_color_lut_table(table, table_count, &prepared_table);
    if (prepare_status != PILLOW_C_OK) {
        return prepare_status;
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
    if (std::strcmp(mode_name_text, "I") == 0) {
        *out_mode = PILLOW_C_MODE_I;
        return PILLOW_C_OK;
    }
    if (std::strcmp(mode_name_text, "F") == 0) {
        *out_mode = PILLOW_C_MODE_F;
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

extern "C" __declspec(dllexport) int pillow_c_image_effect_spread(
    const PillowCImage* source,
    int distance,
    PillowCImage** out_image)
{
    return effect_spread_image(source, distance, out_image);
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

extern "C" __declspec(dllexport) int pillow_c_image_open_ppm(
    const char* path,
    PillowCImage** out_image)
{
    return open_ppm_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_ppm(
    const PillowCImage* image,
    const char* path)
{
    return save_ppm_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_qoi(
    const char* path,
    PillowCImage** out_image)
{
    return open_qoi_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_qoi(
    const PillowCImage* image,
    const char* path)
{
    return save_qoi_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_tga(
    const char* path,
    PillowCImage** out_image)
{
    return open_tga_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tga(
    const PillowCImage* image,
    const char* path)
{
    return save_tga_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tga_options(
    const PillowCImage* image,
    const char* path,
    int rle)
{
    return save_tga_image_with_options(image, path, rle != 0);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_xbm(
    const char* path,
    PillowCImage** out_image)
{
    return open_xbm_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_xbm(
    const PillowCImage* image,
    const char* path)
{
    return save_xbm_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_xbm_options(
    const PillowCImage* image,
    const char* path,
    int has_hotspot,
    int hotspot_x,
    int hotspot_y)
{
    return save_xbm_image_with_options(image, path, has_hotspot != 0, hotspot_x, hotspot_y);
}

extern "C" __declspec(dllexport) int pillow_c_image_tobitmap(
    const PillowCImage* image,
    const char* name,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    return tobitmap_image(image, name, out, out_size, out_required);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_ico(
    const char* path,
    PillowCImage** out_image)
{
    return open_ico_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_ico(
    const PillowCImage* image,
    const char* path)
{
    return save_ico_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_ico_options(
    const PillowCImage* image,
    const char* path,
    const int* sizes,
    std::size_t size_count)
{
    return save_ico_image_options(image, path, sizes, size_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_ico_format_options(
    const PillowCImage* image,
    const char* path,
    const int* sizes,
    std::size_t size_count,
    int has_sizes,
    const char* bitmap_format)
{
    return save_ico_image_format_options(image, path, sizes, size_count, has_sizes, bitmap_format);
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

extern "C" __declspec(dllexport) int pillow_c_image_save_png_compress_level(
    const PillowCImage* image,
    const char* path,
    int compress_level)
{
    return save_png_image_with_compress_level(image, path, compress_level);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y)
{
    return save_png_image_with_options(image, path, compress_level, dpi_x, dpi_y);
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

extern "C" __declspec(dllexport) int pillow_c_image_save_jpeg_quality(
    const PillowCImage* image,
    const char* path,
    int quality)
{
    return save_jpeg_image_with_quality(image, path, quality);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_jpeg_options(
    const PillowCImage* image,
    const char* path,
    int quality,
    int has_dpi,
    double dpi_x,
    double dpi_y)
{
    return save_jpeg_image_with_options(image, path, quality, has_dpi != 0, dpi_x, dpi_y);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_tiff(
    const char* path,
    PillowCImage** out_image)
{
    return open_tiff_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_tiff_frame(
    const char* path,
    int frame_index,
    PillowCImage** out_image)
{
    return open_tiff_frame_image(path, frame_index, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_frame_count_tiff(
    const char* path,
    int* out_count)
{
    return wic_container_frame_count(path, GUID_ContainerFormatTiff, out_count);
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

extern "C" __declspec(dllexport) int pillow_c_image_open_gif_frame(
    const char* path,
    int frame_index,
    PillowCImage** out_image)
{
    return open_gif_frame_image(path, frame_index, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_frame_count_gif(
    const char* path,
    int* out_count)
{
    return wic_container_frame_count(path, GUID_ContainerFormatGif, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_gif_metadata(
    const char* path,
    int frame_index,
    int* out_duration_ms,
    int* out_loop,
    int* out_disposal,
    int* out_background)
{
    if (!path || !out_duration_ms || !out_loop || !out_disposal || !out_background) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_duration_ms = -1;
    *out_loop = -1;
    *out_disposal = -1;
    *out_background = -1;
    if (frame_index < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        GifMetadata metadata;
        if (!read_gif_metadata(path, frame_index, &metadata)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_duration_ms = metadata.duration_ms;
        *out_loop = metadata.loop;
        *out_disposal = metadata.disposal;
        *out_background = metadata.background;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_gif_metadata_ex(
    const char* path,
    int frame_index,
    int* out_duration_ms,
    int* out_loop,
    int* out_disposal,
    int* out_background,
    int* out_transparency)
{
    if (!path || !out_duration_ms || !out_loop || !out_disposal || !out_background || !out_transparency) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_duration_ms = -1;
    *out_loop = -1;
    *out_disposal = -1;
    *out_background = -1;
    *out_transparency = -1;
    if (frame_index < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        GifMetadata metadata;
        if (!read_gif_metadata(path, frame_index, &metadata)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_duration_ms = metadata.duration_ms;
        *out_loop = metadata.loop;
        *out_disposal = metadata.disposal;
        *out_background = metadata.background;
        *out_transparency = metadata.transparency;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif(
    const PillowCImage* image,
    const char* path)
{
    return save_gif_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_options(
    const PillowCImage* image,
    const char* path,
    int has_transparency,
    int transparency)
{
    if (!has_transparency) {
        return save_gif_image(image, path);
    }
    return save_gif_indexed_native(image, path, true, transparency);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count)
{
    return save_gif_animation_image(
        images, image_count, path, durations_ms, duration_count, loop, disposals, disposal_count, -1, -1, 0, 0, 0, 0);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    int include_color_table,
    int optimize)
{
    return save_gif_animation_image(
        images,
        image_count,
        path,
        durations_ms,
        duration_count,
        loop,
        disposals,
        disposal_count,
        include_color_table,
        optimize,
        0,
        0,
        0,
        0);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation_metadata_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    int include_color_table,
    int optimize,
    int has_transparency,
    int transparency)
{
    return save_gif_animation_image(
        images,
        image_count,
        path,
        durations_ms,
        duration_count,
        loop,
        disposals,
        disposal_count,
        include_color_table,
        optimize,
        has_transparency,
        transparency,
        0,
        0);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation_background_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    int include_color_table,
    int optimize,
    int has_transparency,
    int transparency,
    int has_background,
    int background)
{
    return save_gif_animation_image(
        images,
        image_count,
        path,
        durations_ms,
        duration_count,
        loop,
        disposals,
        disposal_count,
        include_color_table,
        optimize,
        has_transparency,
        transparency,
        has_background,
        background);
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
    target->palette_alpha = source->palette_alpha;
    target->palette_alpha_mode = source->palette_alpha_mode;
    target->exif_orientation = source->exif_orientation;
    target->has_hotspot = source->has_hotspot;
    target->hotspot_x = source->hotspot_x;
    target->hotspot_y = source->hotspot_y;
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

extern "C" __declspec(dllexport) int pillow_c_font_load_default(PillowCFont** out_font)
{
    if (!out_font) {
        return PILLOW_C_NULL_POINTER;
    }
    try {
        *out_font = new PillowCFont{PILLOW_C_FONT_DEFAULT};
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        *out_font = nullptr;
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_font_free(PillowCFont* font)
{
    delete font;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_font_getmetrics(
    const PillowCFont* font,
    int* out_ascent,
    int* out_descent)
{
    if (!font || !out_ascent || !out_descent) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_ascent = 10;
    *out_descent = 3;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_font_getname(
    const PillowCFont* font,
    char* out_family,
    std::size_t family_size,
    std::size_t* out_family_required,
    char* out_style,
    std::size_t style_size,
    std::size_t* out_style_required)
{
    if (!font || !out_family_required || !out_style_required) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        *out_family_required = 0;
        *out_style_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }

    constexpr const char* family = "Aileron";
    constexpr const char* style = "Regular";
    const std::size_t family_required = std::strlen(family) + 1;
    const std::size_t style_required = std::strlen(style) + 1;
    *out_family_required = family_required;
    *out_style_required = style_required;

    if (!out_family || !out_style) {
        return PILLOW_C_NULL_POINTER;
    }
    if (family_size < family_required || style_size < style_required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_family, family, family_required);
    std::memcpy(out_style, style, style_required);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_font_variant(
    const PillowCFont* font,
    PillowCFont** out_font)
{
    if (!font || !out_font) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_font = nullptr;
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        *out_font = new PillowCFont{font->kind};
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_font_getlength(
    const PillowCFont* font,
    const char* text,
    double* out_length)
{
    if (!out_length) {
        return PILLOW_C_NULL_POINTER;
    }
    int length = 0;
    const int status = font_text_metrics(font, text, &length, nullptr, nullptr, nullptr, nullptr);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_length = static_cast<double>(length);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_font_getbbox(
    const PillowCFont* font,
    const char* text,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    int length = 0;
    return font_text_metrics(font, text, &length, out_left, out_top, out_right, out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_font_getbbox_anchor(
    const PillowCFont* font,
    const char* text,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_font_anchor(0, 0, text, font, anchor, out_left, out_top, out_right, out_bottom);
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

extern "C" __declspec(dllexport) int pillow_c_image_exif_orientation(const PillowCImage* image, int* out_orientation)
{
    if (!image || !out_orientation) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_orientation = image->exif_orientation;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_resolution(
    const PillowCImage* image,
    int* out_has_dpi,
    double* out_dpi_x,
    double* out_dpi_y,
    int* out_jfif,
    int* out_jfif_major,
    int* out_jfif_minor,
    int* out_jfif_unit,
    int* out_jfif_density_x,
    int* out_jfif_density_y)
{
    if (!image || !out_has_dpi || !out_dpi_x || !out_dpi_y || !out_jfif || !out_jfif_major || !out_jfif_minor ||
        !out_jfif_unit || !out_jfif_density_x || !out_jfif_density_y) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_dpi = image->has_dpi ? 1 : 0;
    *out_dpi_x = image->dpi_x;
    *out_dpi_y = image->dpi_y;
    *out_jfif = image->has_jfif ? ((image->jfif_major << 8) | image->jfif_minor) : 0;
    *out_jfif_major = image->has_jfif ? image->jfif_major : 0;
    *out_jfif_minor = image->has_jfif ? image->jfif_minor : 0;
    *out_jfif_unit = image->has_jfif ? image->jfif_unit : -1;
    *out_jfif_density_x = image->has_jfif ? image->jfif_density_x : 0;
    *out_jfif_density_y = image->has_jfif ? image->jfif_density_y : 0;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_hotspot(
    const PillowCImage* image,
    int* out_has_hotspot,
    int* out_hotspot_x,
    int* out_hotspot_y)
{
    if (!image || !out_has_hotspot || !out_hotspot_x || !out_hotspot_y) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_hotspot = image->has_hotspot ? 1 : 0;
    *out_hotspot_x = image->has_hotspot ? image->hotspot_x : 0;
    *out_hotspot_y = image->has_hotspot ? image->hotspot_y : 0;
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
    if (!((image->mode == PILLOW_C_MODE_P || image->mode == PILLOW_C_MODE_L) && image->channels == 1) ||
        size % 3u != 0 ||
        size > 768u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        image->mode = PILLOW_C_MODE_P;
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
    if (!((image->mode == PILLOW_C_MODE_P || image->mode == PILLOW_C_MODE_L) && image->channels == 1) ||
        size % 4u != 0 ||
        size > 1024u ||
        alpha_mode < PILLOW_C_PALETTE_ALPHA_NONE ||
        alpha_mode > PILLOW_C_PALETTE_ALPHA_RGBX) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        image->mode = PILLOW_C_MODE_P;
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

extern "C" __declspec(dllexport) int pillow_c_image_get_palette_rgba(
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
    if (image->mode != PILLOW_C_MODE_P || image->channels != 1) {
        *out_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_mode = image->palette_alpha_mode;
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

extern "C" __declspec(dllexport) int pillow_c_image_draw_text(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size)
{
    return draw_text_image(image, left, top, text, fill, fill_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor)
{
    return draw_text_image_anchor(image, left, top, text, fill, fill_size, anchor);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return draw_text_image_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return draw_text_image_anchor_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        anchor,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_font(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size)
{
    return draw_text_image_font(image, left, top, text, font, fill, fill_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_font_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return draw_text_image_font_stroke(
        image,
        left,
        top,
        text,
        font,
        fill,
        fill_size,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_font_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor)
{
    return draw_text_image_font_anchor(image, left, top, text, font, fill, fill_size, anchor);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_font_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return draw_text_image_font_anchor_stroke(
        image,
        left,
        top,
        text,
        font,
        fill,
        fill_size,
        anchor,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing)
{
    return draw_multiline_text_image(image, left, top, text, fill, fill_size, spacing);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_align(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align)
{
    return draw_multiline_text_image_align(image, left, top, text, fill, fill_size, spacing, align);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor)
{
    return draw_multiline_text_image_anchor(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        align,
        anchor);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_align_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return draw_multiline_text_image_align_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        align,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return draw_multiline_text_image_anchor_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        align,
        anchor,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_font(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing)
{
    return draw_multiline_text_image_font(image, left, top, text, font, fill, fill_size, spacing);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_font_align(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align)
{
    return draw_multiline_text_image_font_align(
        image,
        left,
        top,
        text,
        font,
        fill,
        fill_size,
        spacing,
        align);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_font_align_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return draw_multiline_text_image_font_align_stroke(
        image,
        left,
        top,
        text,
        font,
        fill,
        fill_size,
        spacing,
        align,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_font_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor)
{
    return draw_multiline_text_image_font_anchor(
        image,
        left,
        top,
        text,
        font,
        fill,
        fill_size,
        spacing,
        align,
        anchor);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_font_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return draw_multiline_text_image_font_anchor_stroke(
        image,
        left,
        top,
        text,
        font,
        fill,
        fill_size,
        spacing,
        align,
        anchor,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_textlength(
    const char* text,
    double* out_length)
{
    if (!out_length) {
        return PILLOW_C_NULL_POINTER;
    }
    int length = 0;
    const int status = default_font_text_metrics(text, &length, nullptr, nullptr, nullptr, nullptr);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_length = static_cast<double>(length);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox(
    int left,
    int top,
    const char* text,
    int spacing,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox(left, top, text, spacing, out_left, out_top, out_right, out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_align(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_align_i32(
        left,
        top,
        text,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_align_f64(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_align(
        left,
        top,
        text,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_align_stroke(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_align_stroke_i32(
        left,
        top,
        text,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_align_stroke_f64(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_align_stroke(
        left,
        top,
        text,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_anchor_f64(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    const char* anchor,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_anchor(
        left,
        top,
        text,
        spacing,
        align,
        anchor,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_anchor_stroke_f64(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_anchor_stroke(
        left,
        top,
        text,
        spacing,
        align,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_font(
        left,
        top,
        text,
        font,
        spacing,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_align(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_font_align(
        left,
        top,
        text,
        font,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_align_stroke(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_font_align_stroke(
        left,
        top,
        text,
        font,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_align_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_font_align_f64(
        left,
        top,
        text,
        font,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_align_stroke_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_font_align_stroke_f64(
        left,
        top,
        text,
        font,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_anchor_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    const char* anchor,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_font_anchor_f64(
        left,
        top,
        text,
        font,
        spacing,
        align,
        anchor,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_anchor_stroke_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_font_anchor_stroke_f64(
        left,
        top,
        text,
        font,
        spacing,
        align,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox(
    int left,
    int top,
    const char* text,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    int length = 0;
    int bbox_left = 0;
    int bbox_top = 0;
    int bbox_right = 0;
    int bbox_bottom = 0;
    const int status =
        default_font_text_metrics(text, &length, &bbox_left, &bbox_top, &bbox_right, &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_left = left + bbox_left;
    *out_top = top + bbox_top;
    *out_right = left + bbox_right;
    *out_bottom = top + bbox_bottom;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox_stroke(
    int left,
    int top,
    const char* text,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_stroke(
        left,
        top,
        text,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox_anchor(
    int left,
    int top,
    const char* text,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_anchor(left, top, text, anchor, out_left, out_top, out_right, out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox_anchor_stroke(
    int left,
    int top,
    const char* text,
    const char* anchor,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_anchor_stroke(
        left,
        top,
        text,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox_font_anchor(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_font_anchor(left, top, text, font, anchor, out_left, out_top, out_right, out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox_font_anchor_stroke(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const char* anchor,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_font_anchor_stroke(
        left,
        top,
        text,
        font,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
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
        auto* image = new PillowCImage{source->width, source->height, source->mode, source->channels, source->stride, source->pixels, source->palette_rgb, source->exif_orientation};
        image->palette_alpha = source->palette_alpha;
        image->palette_alpha_mode = source->palette_alpha_mode;
        image->has_hotspot = source->has_hotspot;
        image->hotspot_x = source->hotspot_x;
        image->hotspot_y = source->hotspot_y;
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

extern "C" __declspec(dllexport) int pillow_c_image_point_lut_mode(
    const PillowCImage* source,
    const std::uint8_t* lut,
    std::size_t lut_size,
    int target_mode,
    PillowCImage** out_image)
{
    if (!source || !lut || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const int target_channels = point_lut_target_channels(source, target_mode);
    if (target_channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (lut_size != static_cast<std::size_t>(source->channels) * 256u) {
        return PILLOW_C_INVALID_LENGTH;
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
        image->palette_alpha = source->palette_alpha;
        image->palette_alpha_mode = source->palette_alpha_mode;
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
